/*
 Copyright (C) 2015-2022 IoT.bzh Company

 Author: José Bollo <jose.bollo@iot.bzh>

 $RP_BEGIN_LICENSE$
 Commercial License Usage
  Licensees holding valid commercial IoT.bzh licenses may use this file in
  accordance with the commercial license agreement provided with the
  Software or, alternatively, in accordance with the terms contained in
  a written agreement between you and The IoT.bzh Company. For licensing terms
  and conditions see https://www.iot.bzh/terms-conditions. For further
  information use the contact form at https://www.iot.bzh/contact.

 GNU General Public License Usage
  Alternatively, this file may be used under the terms of the GNU General
  Public license version 3. This license is as published by the Free Software
  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
  of this file. Please review the following information to ensure the GNU
  General Public License requirements will be met
  https://www.gnu.org/licenses/gpl-3.0.html.
 $RP_END_LICENSE$
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>

#include "path-entry.h"

#define FLAGS_IS_ADDED    1
#define FLAGS_HAS_SLASH   2

typedef unsigned char flag_t;

typedef struct var
{
	const void *key;
	struct var *next;
	void *value;
	void (*dispose)(void*);
}
	var_t;

struct path_entry {
	path_entry_t *parent;
	path_entry_t *children;
	path_entry_t *sibling;
	var_t *vars;
	unsigned length;
	flag_t flags;
	char name[];
};

static path_entry_t *search_child(path_entry_t *parent, const char *name, unsigned length, flag_t flags)
{
	path_entry_t *entry = parent->children;
	while (entry != NULL && (entry->length != length || memcmp(name, entry->name, length)))
		entry = entry->sibling;
	return entry;
}

static path_entry_t *make_child(path_entry_t *parent, const char *name, unsigned length, flag_t flags)
{
	path_entry_t **prv, *entry = malloc(length + 1 + sizeof *entry);

	if (entry != NULL) {
		entry->parent = parent;
		entry->children = NULL;
		entry->vars = NULL;
		entry->length = length;
		entry->flags = flags;
		entry->name[length] = 0;
		memcpy(entry->name, name, length);
		prv = &parent->children;
		while (*prv != NULL && strcmp((*prv)->name, name) < 0)
			prv = &(*prv)->sibling;
		entry->sibling = *prv;
		*prv = entry;
	}
	return entry;
}

static path_entry_t *get_child(path_entry_t *parent, const char *name, unsigned length, flag_t flags)
{
	return search_child(parent, name, length, flags) ?: make_child(parent, name, length, flags);
}

static path_entry_t *process(
		path_entry_t *root,
		path_entry_t *(*get)(path_entry_t*, const char*, unsigned, flag_t),
		const char *path,
		unsigned length
) {
	unsigned end, beg;
	path_entry_t *entry = root;
	flag_t flags = 0;

	for (beg = 0 ; beg < length && path[beg] == '/' ; beg++, flags = FLAGS_HAS_SLASH);
	while (entry != NULL && beg < length) {
		for (end = beg ; end < length && path[end] != '/' ; end++);
		entry = get(entry, &path[beg], end - beg, flags);
		for (beg = end ; beg < length && path[beg] == '/' ; beg++);
		flags = FLAGS_HAS_SLASH;
	}
	return entry;
}

int path_entry_is_root(path_entry_t *entry)
{
	return entry->parent == NULL;
}

int path_entry_create_root(path_entry_t **root)
{
	path_entry_t *entry = malloc(1 + sizeof *entry);
	*root = entry;
	if (entry == NULL)
		return -ENOMEM;

	entry->parent = NULL;
	entry->children = NULL;
	entry->sibling = NULL;
	entry->vars = NULL;
	entry->length = 0;
	entry->flags = 0;
	entry->name[0] = 0;
	return 0;
}

static void destroy(path_entry_t *entry)
{
	var_t *var;
	path_entry_t *child = entry->children;
	while (child != NULL) {
		path_entry_t *sibling = child->sibling;
		destroy(child);
		child = sibling;
	}
	var = entry->vars;
	while(var != NULL) {
		var_t *next = var->next;
		free(var);
		var = next;
	}
	free(entry);
}

void path_entry_destroy(path_entry_t *entry)
{
	path_entry_t *other;
	if (entry != NULL) {
		/* unlink from parents if needed */
		other = entry->parent;
		if (other != NULL) {
			if (other->children == entry)
				other->children = entry->sibling;
			else {
				other = other->children;
				while (other->sibling != entry)
					other = other->sibling;
				other->sibling = entry->sibling;
			}
		}
		/* full destroy */
		destroy(entry);
	}
}

int path_entry_add_length(path_entry_t *root, path_entry_t **result, const char *path, size_t length)
{
	path_entry_t *entry = process(root, get_child, path, (unsigned)length);
	if (result != NULL)
		*result = entry;
	if (entry == NULL)
		return -ENOMEM;
	entry->flags |= FLAGS_IS_ADDED;
	return 0;
}

int path_entry_add(path_entry_t *root, path_entry_t **result, const char *path)
{
	return path_entry_add_length(root, result, path, path == NULL ? 0 : strlen(path));
}

int path_entry_get_length(const path_entry_t *root, path_entry_t **result, const char *path, size_t length)
{
	path_entry_t *entry = process((path_entry_t*)root, search_child, path, (unsigned)length);
	if (result != NULL)
		*result = entry;
	return entry == NULL ? -ENOENT : 0;
}

int path_entry_get(const path_entry_t *root, path_entry_t **result, const char *path)
{
	return path_entry_get_length(root, result, path, path == NULL ? 0 : strlen(path));
}

int path_entry_root_prepend_length(path_entry_t *root, path_entry_t **entry, const char *path, size_t length)
{
	path_entry_t *children, *tail;
	int rc;

	if (root->parent != NULL) {
		tail = NULL;
		rc = -EINVAL;
	}
	else {
		children = root->children;
		root->children = NULL;
		tail = process(root, get_child, path, (unsigned)length);
		if (tail == NULL) {
			root->children = children;
			rc = -ENOMEM;
		}
		else {
			tail->children = children;
			while (children != NULL) {
				children->parent = tail;
				children = children->sibling;
			}
			rc = 0;
		}
	}
	if (entry)
		*entry = tail;
	return rc;
}

int path_entry_root_prepend(path_entry_t *root, path_entry_t **entry, const char *path)
{
	return path_entry_root_prepend_length(root, entry, path, strlen(path));
}

int path_entry_was_added(const path_entry_t *entry)
{
	return (entry->flags & FLAGS_IS_ADDED) != 0;
}

const char *path_entry_name(const path_entry_t *entry)
{
	return entry->name;
}

size_t path_entry_length(const path_entry_t *entry)
{
	return entry->length;
}

path_entry_t *path_entry_parent(const path_entry_t *entry)
{
	return entry->parent;
}

int path_entry_has_child(const path_entry_t *entry)
{
	return entry->children != NULL;
}

size_t path_entry_get_relpath(const path_entry_t *entry, char *buffer, size_t length, const path_entry_t *root)
{
	size_t result, offset;

	if (entry == NULL || entry == root)
		result = 0;
	else {
		offset = path_entry_get_relpath(entry->parent, buffer, length, root);
		if (entry->length == 0)
			result = offset;
		else {
			result = offset + entry->length;
			if (offset > 0 || (entry->flags & FLAGS_HAS_SLASH) != 0) {
				result++;
				if (offset < length)
					buffer[offset++] = '/';
			}
			if (offset < length) {
				length -= offset;
				if (entry->length >= length)
					length = length - 1;
				else
					length = entry->length;
				memcpy(&buffer[offset], entry->name, length);
				buffer[offset + length] = 0;
			}
		}
	}
	return result;
}

size_t path_entry_get_path(const path_entry_t *entry, char *buffer, size_t length)
{
	return path_entry_get_relpath(entry, buffer, length, NULL);
}

struct foreach {
	void *closure;
	int (*fun)(void *closure, path_entry_t *entry, const char *path, size_t length);
	unsigned flags;
	size_t length;
	size_t size;
	char *buffer;
};

static int do_foreach(struct foreach *fe, path_entry_t *entry)
{
	int rc = 0, emit;
	size_t memo;
	path_entry_t *child;

	if (fe->flags & PATH_ENTRY_FORALL_SILENT_ROOT) {
		fe->flags ^= PATH_ENTRY_FORALL_SILENT_ROOT;
		emit = 0;
	}
	else {
		if (entry->length > 0 && !(fe->flags & PATH_ENTRY_FORALL_NO_PATH)) {
			if (fe->length + entry->length + 2 > fe->size)
				return -ENAMETOOLONG;
			if (fe->length > 0 || (entry->flags & FLAGS_HAS_SLASH) != 0)
				fe->buffer[fe->length++] = '/';
			memcpy(&fe->buffer[fe->length], entry->name, entry->length);
			fe->length += entry->length;
		}
		emit = !(fe->flags & PATH_ENTRY_FORALL_ONLY_ADDED) || (entry->flags & FLAGS_IS_ADDED);
	}
	memo = fe->length;
	if (emit && (fe->flags & PATH_ENTRY_FORALL_BEFORE)) {
		if (!(fe->flags & PATH_ENTRY_FORALL_NO_PATH))
			fe->buffer[memo] = 0;
		rc = fe->fun(fe->closure, entry, fe->buffer, memo);
	}
	for (child = entry->children; rc == 0 && child != NULL ; child = child->sibling) {
		fe->length = memo;
		rc = do_foreach(fe, child);
	}
	if (emit && rc == 0 && (fe->flags & PATH_ENTRY_FORALL_AFTER)) {
		if (!(fe->flags & PATH_ENTRY_FORALL_NO_PATH))
			fe->buffer[memo] = 0;
		rc = fe->fun(fe->closure, entry, fe->buffer, memo);
	}
	return rc;
}

int path_entry_for_each_in_buffer(
	unsigned flags,
	path_entry_t *root,
	int (*fun)(void *closure, path_entry_t *entry, const char *path, size_t length),
	void *closure,
	char *buffer,
	size_t size
) {
	struct foreach fe;
	if ((flags & (PATH_ENTRY_FORALL_BEFORE|PATH_ENTRY_FORALL_AFTER)) == 0)
		flags |= PATH_ENTRY_FORALL_AFTER;
	fe.closure = closure;
	fe.fun = fun;
	fe.flags = flags;
	if ((flags & (PATH_ENTRY_FORALL_ABSOLUTE|PATH_ENTRY_FORALL_NO_PATH)) != PATH_ENTRY_FORALL_ABSOLUTE)
		fe.length = 0;
	else
		fe.length = path_entry_get_path(root, buffer, size);
	fe.size = size;
	fe.buffer = buffer;
	return root == NULL ? 0 : do_foreach(&fe, root);
}

static int for_each_scratch(
	unsigned flags,
	path_entry_t *root,
	int (*fun)(void *closure, path_entry_t *entry, const char *path, size_t length),
	void *closure
) {
	char buffer[PATH_MAX];
	return path_entry_for_each_in_buffer(flags, root, fun, closure, buffer, sizeof buffer);
}

int path_entry_for_each(
	unsigned flags,
	path_entry_t *root,
	int (*fun)(void *closure, path_entry_t *entry, const char *path, size_t length),
	void *closure
) {
	if (flags & PATH_ENTRY_FORALL_NO_PATH)
		return path_entry_for_each_in_buffer(flags, root, fun, closure, NULL, 0);
	return for_each_scratch(flags, root, fun, closure);
}




static var_t *search_var(var_t *var, const void *key)
{
	while(var != NULL && key != var->key)
		var = var->next;
	return var;
}

static void *varval(var_t *var)
{
	return var == NULL ? NULL : var->value;
}

int path_entry_var_exists(const path_entry_t *entry, const void *key)
{
	return search_var(entry->vars, key) != NULL;
}

void *path_entry_var(const path_entry_t *entry, const void *key)
{
	return varval(search_var(entry->vars, key));
}

int path_entry_var_set(path_entry_t *entry, const void *key, void *value, void (*dispose)(void*))
{
	var_t *var = search_var(entry->vars, key);
	if (var == NULL) {
		var = malloc(sizeof *var);
		if (var == NULL)
			return -ENOMEM;
		var->key = key;
		var->next = entry->vars;
		entry->vars = var;
	}
	else if (var->dispose != NULL)
		var->dispose(var->value);
	var->value = value;
	var->dispose = dispose;
	return 0;
}

void path_entry_var_del(path_entry_t *entry, const void *key)
{
	var_t *var, **prv = &entry->vars;
	while ((var = *prv) != NULL) {
		if (var->key == key) {
			*prv = var->next;
			if (var->dispose != NULL)
				var->dispose(var->value);
			free(var);
			break;
		}
		prv = &var->next;
	}
}

unsigned path_entry_var_count(const path_entry_t *entry)
{
	var_t *var = entry->vars;
	unsigned result = 0;
	while(var != NULL) {
		var = var->next;
		result++;
	}
	return result;
}

void *path_entry_var_at(const path_entry_t *entry, unsigned index, const void **key)
{
	var_t *var = entry->vars;
	unsigned idx = 0;
	while(var != NULL && idx != index) {
		var = var->next;
		idx++;
	}
	if (key != NULL)
		*key = var->key;
	return varval(var);
}

int path_entry_add_from_file(path_entry_t *root, FILE *file)
{
	int rc = 0;
	size_t idx;
	char path[PATH_MAX];

	while (rc == 0 && fgets(path, sizeof path, file) != NULL) {
		size_t len = strlen(path);
		while (len && isspace(path[len - 1]))
			len--;
		if (len && path[0] == '#') {
			if (len == 6 && path[1] == 'S' && path[2] == 'T'
			 && path[3] == 'O' && path[4] == 'P' && path[5] == '#')
				break;
		}
		else {
			for (idx = 0 ; idx < len && isspace(path[idx]) ; idx++);
			if (idx < len)
				rc = path_entry_add_length(root, NULL, &path[idx], len - idx);
		}
	}
	if (rc == 0 && ferror(file))
		rc = -errno;
	return rc;
}

/*
 Copyright (C) 2015-2025 IoT.bzh Company

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

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <rp-utils/rp-verbose.h>
#include "permset.h"


struct permission {
	char *name;
	size_t length;
	unsigned granted: 1;
	unsigned requested: 1;
};

struct permset_s {
	unsigned count;
	unsigned index;
	struct permission *permissions;
};

/* retrieves the permission of name */
static struct permission *get(permset_t *permset, const char *name, size_t length)
{
	struct permission *it = permset->permissions, *end = it + permset->count;

	for ( ; it != end ; it++)
		if (it->length == length && 0 == memcmp(it->name, name, length))
			return it;
	return NULL;
}

/* request the permission, returns 1 if granted or 0 otherwise */
static int add(permset_t *permset, const char *name, size_t length, int grant, int request)
{
	struct permission *p = get(permset, name, length);
	if (p == NULL) {
		if ((permset->count & 7) == 0) {
			p = realloc(permset->permissions,
				(permset->count + 8) * sizeof(*p));
			if (p == NULL)
				return -ENOMEM;
			permset->permissions = p;
		}
		p = permset->permissions + permset->count;
		p->name = malloc(length + 1);
		if (p->name == NULL)
			return -ENOMEM;
		memcpy(p->name, name, length);
		p->name[length] = '\0';
		p->length = length;
		p->granted = 0;
		p->requested = 0;
		permset->count++;
	}
	if (grant)
		p->granted = 1;
	if (request)
		p->requested = 1;
	return p->requested && p->granted;
}

static int add_list(permset_t *permset, const char *list, int grant, int request)
{
	const char *iter;
	size_t n;
	int rc, allowed = 1;
	static const char separators[] = " \t\n\r,";

	iter = list + strspn(list, separators);
	while(*iter) {
		n = strcspn(iter, separators);
		rc = add(permset, iter, n, grant, request);
		if (rc < 0)
			return rc;
		allowed = allowed & rc;
		iter += n;
		iter += strspn(iter, separators);
	}
	return allowed;
}

int permset_grant(permset_t *permset, const char *name)
{
	return add(permset, name, strlen(name), 1, 0);
}

int permset_grant_list(permset_t *permset, const char *list)
{
	return add_list(permset, list, 1, 0);
}

int permset_add(permset_t *permset, const char *name)
{
	return add(permset, name, strlen(name), 0, 0);
}

int permset_add_list(permset_t *permset, const char *list)
{
	return add_list(permset, list, 0, 0);
}

int permset_request(permset_t *permset, const char *name)
{
	return add(permset, name, strlen(name), 0, 1);
}

int permset_request_list(permset_t *permset, const char *list)
{
	return add_list(permset, list, 0, 1);
}

/* remove any granting */
void permset_reset(permset_t *permset, permset_reset_t it)
{
	unsigned int i;
	for (i = 0 ; i < permset->count ; i++) {
		switch (it) {
		default:
		case permset_Reset_Nothing:
			break;
		case permset_Reset_Requested:
			permset->permissions[i].requested = 0;
			break;
		case permset_Reset_Granted:
			permset->permissions[i].granted = 0;
			break;
		case permset_Reset_Requested_And_Granted:
			permset->permissions[i].granted = permset->permissions[i].requested = 0;
			break;
		}
	}
}

/* checks if the permission 'name' is recorded */
int permset_has(permset_t *permset, const char *name)
{
	return !!get(permset, name, strlen(name));
}

static int matches(struct permission *permission, permset_select_t it)
{
	switch (it) {
	default:
	case permset_Select_Any:
		return 1;
	case permset_Select_Requested:
		return permission->requested;
	case permset_Select_Granted:
		return permission->granted;
	case permset_Select_Requested_And_Granted:
		return permission->granted && permission->requested;
	}
}

static int advance(permset_t *permset, permset_select_t it)
{
	while (permset->index < permset->count
		&& !matches(&permset->permissions[permset->index], it))
		permset->index++;
	return permset->index < permset->count;
}

int permset_select_first(permset_t *permset, permset_select_t it)
{
	permset->index = 0;
	return advance(permset, it);
}

int permset_select_next(permset_t *permset, permset_select_t it)
{
	permset->index++;
	return advance(permset, it);
}

const char *permset_current(permset_t *permset)
{
	return permset->index < permset->count ? permset->permissions[permset->index].name : NULL;
}

int permset_is_current_requested(permset_t *permset)
{
	return permset->index < permset->count ? permset->permissions[permset->index].requested : 0;
}

int permset_is_current_granted(permset_t *permset)
{
	return permset->index < permset->count ? permset->permissions[permset->index].granted : 0;
}

int permset_create(permset_t **permset)
{
	return (*permset = calloc(sizeof **permset, 1)) == NULL ? -ENOMEM : 0;
}

void permset_destroy(permset_t *permset)
{
	if (permset != NULL) {
		unsigned idx = permset->count;
		while(idx)
			free(permset->permissions[--idx].name);
		free(permset->permissions);
		free(permset);
	}
}

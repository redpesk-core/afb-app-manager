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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

#include "file-hash-list.h"

/* create a simple file list */
int file_list_create(file_list_t **files)
{
	*files = (file_list_t*)calloc(1, sizeof(file_list_t));
	return NULL == *files ? -ENOMEM : 0;
}

/* destroys the given list */
void file_list_destroy(file_list_t *files)
{
	if (files != NULL) {
		file_node_t *node = files->root;
		while (node != NULL) {
			file_node_t *nxtnode = node;
			hash_node_t *hash = node->hashes;
			while (hash != NULL) {
				hash_node_t *nxthash = hash->next;
				free(hash);
				hash = nxthash;
			}
			free(node);
			node = nxtnode;
		}
		free(files);
	}
}

/* add a string of given length in the simple list */
int file_list_add_length(file_list_t *files, const char *path, size_t length)
{
	if (files) {
		int cmp;
		file_node_t *node, **prev = &files->root;
		while ((node = *prev) != NULL && (cmp = memcmp(path, node->value, length)) > 0)
			prev = &node->next;
		if (node == NULL || cmp != 0 || node->value[length]) {
			node = malloc(length + 1 + sizeof *node);
			if (node == NULL)
				return -ENOMEM;
			memcpy(node->value, path, length);
			node->value[length] = 0;
			node->hashes = NULL;
			node->pdata = NULL;
			node->idata = 0;
			node->length = length;
			node->next = *prev;
			*prev = node;
		}
	}
	return 0;
}

/* add a zero terminated string in the simple list */
int file_list_add(file_list_t *files, const char *value)
{
	return file_list_add_length(files, value, strlen(value));
}

/* call the function until it returns zero or end of the list */
void file_list_iterate(
	file_list_t *files,
	int (*function)(void*, file_node_t *),
	void *closure
) {
	file_node_t *iter = files->root;
	while (iter != NULL && function(closure, iter))
		iter = iter->next;
}

/* compute the hash of the file of the given node */
int file_node_hash(
	file_node_t *node,
	gnutls_digest_algorithm_t algorithm,
	FILE *(*open_node)(void*, const file_node_t*),
	void *closure,
	hash_node_t **hashnode
) {
	hash_node_t *hash;
	gnutls_hash_hd_t hctx;
	unsigned hashlen, idxstr;
	FILE *file;
	size_t rlen;
	unsigned char buffer[65500];

	/* search an existing hash */
	hash = node->hashes;
	while(hash != NULL && hash->algorithm != algorithm)
		hash = hash->next;

	/* create the hash if needed */
	if (hash == NULL) {

		/* open the file */
		file = open_node
			? open_node(closure, node)
			: fopen(node->value, "rb");
		if (file == NULL)
			return -errno;

		/* allocates the result */
		hashlen = gnutls_hash_get_len(algorithm);
		idxstr = hashlen + hashlen;
		hash = malloc(idxstr + 1 + sizeof *hash);
		if (hash == NULL) {
			fclose(file);
			return -ENOMEM;
		}
		hash->algorithm = algorithm;
		hash->pdata = NULL;
		hash->idata = 0;
		hash->length = idxstr;

		/* compute the hash */
		gnutls_hash_init(&hctx, algorithm);
		while ((rlen = fread(buffer, 1, sizeof buffer, file)) > 0)
			gnutls_hash(hctx, buffer, rlen);
		gnutls_hash_deinit(hctx, hash->value);

		/* convert to string */
		hash->value[idxstr] = 0;
		while (hashlen) {
			unsigned char high = hash->value[--hashlen];
			unsigned char low = high & 15;
			hash->value[--idxstr] = (char)(low + (low <= 9 ? '0' : 'a' - 10));
			low = (high >> 4) & 15;
			hash->value[--idxstr] = (char)(low + (low <= 9 ? '0' : 'a' - 10));
		}

		/* link the has to the node */
		hash->next = node->hashes;
		node->hashes = hash;
	}

	/* return the string if required */
	if (hashnode)
		*hashnode = hash;

	return 0;
}

/* find the node whose name is given */
file_node_t *file_node_find_length(
	file_list_t *files,
	const char *name,
	size_t length
) {
	file_node_t *iter = files->root;
	while (iter != NULL && (iter->length != length || memcmp(name, iter->value, length)))
		iter = iter->next;
	return iter;
}

/* find the node whose name is given */
file_node_t *file_node_find(
	file_list_t *files,
	const char *name
) {
	return file_node_find_length(files, name, strlen(name));
}

/**
 * @brief create the file list containing the files listed by file
 *
 * @param file the file to read until end or single string "#STOP#"
 * @param files the file list to create
 * @return 0 on success or a negative error value
 */
/* create the file list containing the files listed by file */
int file_list_create_from_file(FILE *file, file_list_t **files)
{
	int idx;
	char path[PATH_MAX];
	FILE *f;

	if (file_list_create(files) < 0)
		return -ENOMEM;

	while (fgets(path, sizeof path, file) != NULL) {
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
			if (idx < len) {
				path[len] = 0;
				if (file_list_add(*files, &path[idx]) < 0)
					return -ENOMEM;
			}
		}
	}
	return 0;
}
		

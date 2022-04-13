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

#pragma once

#include <stdio.h>
#include <gnutls/gnutls.h>

/**
 * @brief simple node for hash value of a file
 */
typedef
struct hash_node {
	/** next node of the list */
	struct hash_node *next;

	/** a data for the value */
	gnutls_digest_algorithm_t algorithm;

	/** some pointer data */
	void *pdata;

	/** some integer data */
	int idata;

	/* length of the hash */
	unsigned length;

	/** value of the hash (copied) */
	char value[];
} hash_node_t;

/**
 * @brief simple node for a list of strings
 */
typedef
struct file_node {
	/** next node of the list */
	struct file_node *next;

	/** hashes of the file */
	hash_node_t *hashes;

	/** some pointer data */
	void *pdata;

	/** some integer data */
	int idata;

	/* length of the path */
	unsigned length;

	/** the file path */
	char value[];
} file_node_t;

/**
 * @brief simple sorted list of strings
 */
typedef
struct {
	/** first node of the list */
	file_node_t *root;
} file_list_t;

/**
 * @brief create a simple file list
 *
 * @param files result
 * @return int 0 if success or -ENOMEM
 */
extern
int file_list_create(file_list_t **files);

/**
 * @brief destroys the given list
 *
 * @param files the list to destroy
 */
extern
void file_list_destroy(file_list_t *files);

/**
 * @brief add a string of given length in the simple list
 *
 * @param files the simple file list to change
 * @param path the string value to add (copied)
 * @param length the length of the path value to add (not including null)
 * @return int 0 on success or -ENOMEM on error
 */
extern
int file_list_add_length(file_list_t *files, const char *path, size_t length);

/**
 * @brief add a zero terminated string in the simple list
 *
 * @param files the simple file list to change
 * @param value the string value to add (copied)
 * @return int 0 on success or -ENOMEM on error
 */
extern
int file_list_add(file_list_t *files, const char *value);

/**
 * @brief compute the hash of the file of the given node
 *
 * @param node the node of the file
 * @param algorithm the agorithm to use for computing
 * @param open_file function for opening the file (can use fopen)
 * @param hashnode return a pointer on the hash node (can be NULL)
 * @return int 0 in case success or -ENOMEM if error of memory
 * or -EOVERFLOW on internal unexpected defect.
 */
extern
int file_node_hash(
	file_node_t *node,
	gnutls_digest_algorithm_t algorithm,
	FILE *(*open_file)(void*, const file_node_t*),
	void *closure,
	hash_node_t **hashnode
);

/**
 * @brief call the function until it returns zero or end of the list
 *
 * @param files the files list
 * @param function function to be called for each node
 * @param closure closure for function
 */
extern
void file_list_iterate(
	file_list_t *files,
	int (*function)(void*, file_node_t *),
	void *closure
);

/**
 * @brief find the node whose name is given
 *
 * @param files the files list
 * @param name the name to find
 * @param length the length of the name
 * @return the node found or NULL otherwise
 */
extern
file_node_t *file_node_find_length(
	file_list_t *files,
	const char *name,
	size_t length
);

/**
 * @brief find the node whose name is given
 *
 * @param files the files list
 * @param name the name to find
 * @return the node found or NULL otherwise
 */
extern
file_node_t *file_node_find(
	file_list_t *files,
	const char *name
);

/**
 * @brief create the file list containing the files listed by file
 * - ignore files starting with # on first column
 * - remove leading and tailing spaces
 * - ignore empty lines
 *
 * @param file the file to read until end or single string "#STOP#"
 * @param files the file list to create
 * @return 0 on success or a negative error value
 */
extern
int file_list_create_from_file(
	FILE *file,
	file_list_t **files
);

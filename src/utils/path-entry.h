/*
 Copyright (C) 2015-2023 IoT.bzh Company

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

#include <stdlib.h>
#include <stdio.h>

/**
 * @brief abstract path entry
 */
typedef struct path_entry path_entry_t;

/**
 * @brief creates the root entry
 *
 * @param root the root
 * @return 0 on success or -ENOMEM
 */
extern int path_entry_create_root(path_entry_t **root);

/**
 * @brief destroy the given entry and all its content
 *
 * @param entry the entry to be destroyed
 */
extern void path_entry_destroy(path_entry_t *entry);

/**
 * @brief get or add to the root the entry for the path whose length is given
 *
 * @param root the root
 * @param result where to store the result (can be NULL)
 * @param path the path from the root
 * @param length the length of the path
 * @return 0 on success or -ENOMEM
 */
extern int path_entry_add_length(path_entry_t *root, path_entry_t **result, const char *path, size_t length);

/**
 * @brief get or add to the root the entry for the path
 *
 * @param root the root
 * @param result where to store the result (can be NULL)
 * @param path the path from the root
 * @return 0 on success or -ENOMEM
 */
extern int path_entry_add(path_entry_t *root, path_entry_t **result, const char *path);

/**
 * @brief get from the root the entry for the path whose length is given
 *
 * @param root the root
 * @param result where to store the result (can be NULL)
 * @param path the path from the root
 * @param length the length of the path
 * @return 0 on success or -ENOENT
 */
extern int path_entry_get_length(const path_entry_t *root, path_entry_t **result, const char *path, size_t length);

/**
 * @brief get from the root the entry for the path
 *
 * @param root the root
 * @param result where to store the result (can be NULL)
 * @param path the path from the root
 * @return 0 on success or -ENOENT
 */
extern int path_entry_get(const path_entry_t *root, path_entry_t **result, const char *path);

/**
 * @brief prepend the path to the root, on succes, the old root is now at the given
 * path and is returned in entry.
 *
 * @param root the root to alter
 * @param entry the entry corresponding to the previous root (if not NULL)
 * @param path the path of the root
 * @param length the length of the path
 * @return 0 on success, -EINVAL if root is not a root, -ENOMEM on case of memory depletion
 */
extern int path_entry_root_prepend_length(path_entry_t *root, path_entry_t **entry, const char *path, size_t length);

/**
 * @brief prepend the path to the root, on succes, the old root is now at the given
 * path and is returned in entry.
 *
 * @param root the root to alter
 * @param entry the entry corresponding to the previous root (if not NULL)
 * @param path the path of the root
 * @return 0 on success, -EINVAL if root is not a root, -ENOMEM on case of memory depletion
 */
extern int path_entry_root_prepend(path_entry_t *root, path_entry_t **entry, const char *path);

/**
 * check if the entry was added
 *
 * @param entry the entry to inspect
 * @return a non zero value if it was added
 */
extern int path_entry_was_added(const path_entry_t *entry);

/**
 * get the name of the entry
 *
 * @param entry the entry to inspect
 * @return the name of the entry
 */
extern const char *path_entry_name(const path_entry_t *entry);

/**
 * get the length of the name of the entry
 *
 * @param entry the entry to inspect
 * @return the length of the name
 */
extern size_t path_entry_length(const path_entry_t *entry);

/**
 * get the parent of the entry
 *
 * @param entry the entry to inspect
 * @return the parent of the entry or NULL if none
 */
extern path_entry_t *path_entry_parent(const path_entry_t *entry);

/**
 * check if the entry has child
 *
 * @param entry the entry to inspect
 * @return zero if not child, otherwise 1 if children
 */
extern int path_entry_has_child(const path_entry_t *entry);

/**
 * @brief get in buffer the path of the given entry
 *
 * @param entry the entry to get
 * @param buffer the buffer for storing path (can be NULL when length == 0)
 * @param length the length of the buffer
 * @return the length of the path (excluding trailing nul) as if length was enough
 */
extern size_t path_entry_get_path(const path_entry_t *entry, char *buffer, size_t length);

/**
 * @brief get in buffer the relative path of the given entry from root
 *
 * @param entry the entry to get
 * @param buffer the buffer for storing path (can be NULL when length == 0)
 * @param length the length of the buffer
 * @param root the relative root, can be NULL
 * @return the length of the path (excluding trailing nul) as if length was enough
 */
extern size_t path_entry_get_relpath(const path_entry_t *entry, char *buffer, size_t length, const path_entry_t *root);

/**
 * Flags to control functions "path_entry_for_each" and "path_entry_for_each_in_buffer"
 */
enum {
	/**
	 * If set, the callback function is called only for added
	 * entries
	 * Otherwise, when unset, callback function is also called
	 * for intermediate directories
	 */
	PATH_ENTRY_FORALL_ONLY_ADDED  =  1,

	/**
	 * If set, the callback function receives paths of length 0
	 * and of inaccurate value. In that case, it is safe to pass
	 * a NULL pointer as buffer value for "path_entry_for_each_in_buffer"
	 */
	PATH_ENTRY_FORALL_NO_PATH     =  2,

	/**
	 * If set, the callback function is called for directories before
	 * their content.
	 */
	PATH_ENTRY_FORALL_BEFORE      =  4,

	/**
	 * If set, the callback function is called for directories after
	 * their content.
	 * When neither PATH_ENTRY_FORALL_AFTER nor PATH_ENTRY_FORALL_BEFORE
	 * are set, this is the default behaviour
	 */
	PATH_ENTRY_FORALL_AFTER       =  8,

	/**
	 * Expects absolute path, not relatives to the root entry
	 * given to the for-each function
	 */
	PATH_ENTRY_FORALL_ABSOLUTE    = 16,

	/**
	 * If set, the callback function is NOT called for the root entry
	 */
	PATH_ENTRY_FORALL_SILENT_ROOT = 32
};

/**
 * @brief iterate over entries until function returns a not zero value
 *
 * @param flags for to control iteration (see PATH_ENTRY_FORALL_...)
 * @param root root of the entries
 * @param fun function to call for each entry
 * @param closure closure for the function
 * @return 0 on success or -ENAMETOOLONG if cancelled
 */
extern
int path_entry_for_each(
	unsigned flags,
	path_entry_t *root,
	int (*fun)(void *closure, path_entry_t *entry, const char *path, size_t length),
	void *closure
);

/**
 * @brief iterate over all entries with a buffer
 *
 * @param flags for to control iteration (see PATH_ENTRY_FORALL_...)
 * @param root root of the entries
 * @param fun function to call for each entry
 * @param closure closure for the function
 * @param buffer the buffer to use for storing paths
 * @param size the size of the buffer
 * @return 0 on success or -ENAMETOOLONG if cancelled
 */
extern
int path_entry_for_each_in_buffer(
	unsigned flags,
	path_entry_t *root,
	int (*fun)(void *closure, path_entry_t *entry, const char *path, size_t length),
	void *closure,
	char *buffer,
	size_t size
);

/**
 * @brief check if a var exist for the entry
 *
 * @param entry the entry to check
 * @param key the variable key
 *
 * @return 1 if the var exists or 0 otherwise
 */
extern int path_entry_var_exists(const path_entry_t *entry, const void *key);

/**
 * @brief get the value of the var of the entry
 *
 * @param entry the entry
 * @param key the key of the variable
 *
 * @return the value of the variable
 */
extern void *path_entry_var(const path_entry_t *entry, const void *key);

/**
 * @brief set the value of a variable
 *
 * @param entry the entry
 * @param key the key of the variable
 * @param value the value to set
 * @param dispose a function to call when variable is deleted
 *
 * @return 0 on success or -ENOMEM
 */
extern int path_entry_var_set(path_entry_t *entry, const void *key, void *value, void (*dispose)(void*));

/**
 * @brief delete the variable of key of the entry
 *
 * @param entry the entry
 * @param key the key of the variable to delete
 */
extern void path_entry_var_del(path_entry_t *entry, const void *key);

/**
 * @brief get the count of existing variables
 *
 * @param entry the entry
 *
 * @return the count of existing variables
 */
extern unsigned path_entry_var_count(const path_entry_t *entry);

/**
 * @brief get a variable by its index
 *
 * @param entry the entry
 * @param index the index of the variable
 * @param key pointer receiving the key if not NULL
 *
 * @return the value of the variable
 */
extern void *path_entry_var_at(const path_entry_t *entry, unsigned index, const void **key);

/**
 * @brief read the file and add its entry to the root
 *
 * @param root the root to fulfill
 * @param file the file to read
 * @return 0 on success or a negative error code
 */
extern int path_entry_add_from_file(path_entry_t *root, FILE *file);

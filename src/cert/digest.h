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

#include "path-entry.h"

/**
 * @brief compute the hash digest of files of the root
 *
 * @param root of the file list
 * @param algorithm the agorithm to use for computing
 * @param buffer where to store the result
 * @param length length in bytes of the given buffer
 * @param real_root if not NULL, the real root for retrieving real files
 * @param isdistributor zero for author, not zero for distributor
 * @return the length of the production or a negative code on error
 */
extern
int create_digest(
	path_entry_t *root,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length,
	int isdistributor
);

/**
 * @brief compute the hash digest of files of the root
 * for author signature
 *
 * @param root of the file list
 * @param algorithm the agorithm to use for computing
 * @param buffer where to store the result
 * @param length length in bytes of the given buffer
 * @param real_root if not NULL, the real root for retrieving real files
 * @return the length of the production or a negative code on error
 */
extern
int create_author_digest(
	path_entry_t *root,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length
);

/**
 * @brief compute the hash digest of files of the root
 * for distributor signature
 *
 * @param root of the file list
 * @param algorithm the agorithm to use for computing
 * @param buffer where to store the result
 * @param length length in bytes of the given buffer
 * @param real_root if not NULL, the real root for retrieving real files
 * @return the length of the production or a negative code on error
 */
extern
int create_distributor_digest(
	path_entry_t *root,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length
);

/**
 * @brief check if the given digest matches the file list
 *
 * @param root of the file list
 * @param digest the digest to be checked
 * @param length length of the given digest
 * @param real_root if not NULL, the real root for retrieving real files
 * @return 0 if valid of a negative error code -EINVAL
 * @param isdistributor zero for author, not zero for distributor
 * if the digest doesn't match the format
 */
extern
int check_digest(
	path_entry_t *root,
	const char *digest,
	size_t length,
	int isdistributor
);


/**
 * @brief check if the given digest matches the file list
 * for author signature
 *
 * @param root of the file list
 * @param digest the digest to be checked
 * @param length length of the given digest
 * @param real_root if not NULL, the real root for retrieving real files
 * @return 0 if valid of a negative error code -EINVAL
 * if the digest doesn't match the format
 */
extern
int check_author_digest(
	path_entry_t *root,
	const char *digest,
	size_t length
);

/**
 * @brief check if the given digest matches the file list
 * for distributor signature
 *
 * @param root of the file list
 * @param digest the digest to be checked
 * @param length length of the given digest
 * @param real_root if not NULL, the real root for retrieving real files
 * @return 0 if valid of a negative error code
 */
extern
int check_distributor_digest(
	path_entry_t *root,
	const char *digest,
	size_t length
);
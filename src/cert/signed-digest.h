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

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/pkcs7.h>

#include "file-hash-list.h"

/**
 * @brief creates a pkcs7 embedding the signed digest of file with the private key
 * the certificate of the signer is searched in the given list of
 * certificates. The list of certificates is also embedded.
 *
 * @param pkcs7 the created structure must not be initialized
 *              but must be deinitialized on success
 * @param files the list of files to digest
 * @param prefix prefix to add for opening the files
 * @param isdistributor if not zero, digest for a distributor otherwise an author
 * @param algorithm the algorithm to use to hash the files
 * @param key the key to use to sign
 * @param certs an array of certificates to embed
 * @param nrcerts the count of certificates in the array
 *
 * @return 0 on success or a negative error code
 */
extern
int
make_signed_digest(
	gnutls_pkcs7_t    *pkcs7,
	file_list_t       *files,
	const char        *prefix,
	int                isdistributor,
	gnutls_digest_algorithm_t algorithm,
	gnutls_privkey_t   key,
	gnutls_x509_crt_t *certs,
	int                nrcerts
);

/**
 * @brief checks that the given pkcs7 files validates the file list
 *
 * @param pkcs7 the signed file with the digest
 * @param files the list of files to check
 * @param prefix prefix to add for opening the files
 * @param isdistributor if not zero, digest for a distributor otherwise an author
 * @param roots an array of trusted certificates
 * @param nrroots the count of trusted certificates in the array
 * @param spec returns the authorized domain specifications
 *
 * @return 0 on success or a negative error code
 */
extern
int
check_signed_digest(
	gnutls_pkcs7_t    pkcs7,
	file_list_t       *files,
	const char        *prefix,
	int                isdistributor,
	gnutls_x509_crt_t *roots,
	int                nrroots,
	domain_spec_t     *spec
);

/**
 * @brief check that list of files is valid according to the 
 * 
 * @param files file list to be checked
 * @param prefix prefix to add for opening the files
 * @param roots an array of trusted certificates
 * @param nrroots the count of trusted certificates in the array
 * @param spec returns the authorized domain specifications
 *
 * @return 0 on success or a negative error code
 */
extern
int
check_signed_digest_of_files(
	file_list_t       *files,
	const char        *prefix,
	gnutls_x509_crt_t *roots,
	int                nrroots,
	domain_spec_t     *spec
);

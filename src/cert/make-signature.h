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

#include <stdbool.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/pkcs7.h>

/**
 * @brief creates a pkcs7 embedding the data signed with the private key
 * the certificate of the signer is searched in the given list of
 * certificates. The list of certificates is also embedded.
 *
 * @param pkcs7 the created structure must not be initialized
 *              but must be deinitialized on success
 * @param data the data to be signed and embedded
 * @param size size in bytes of the data to be signed and embedded
 * @param key the key to use to sign
 * @param certs an array of certificates to embed
 * @param nrcerts the count of certificates in the array
 */
extern
int
make_signature(
	gnutls_pkcs7_t    *pkcs7,
	const void        *data,
	size_t             size,
	gnutls_privkey_t   key,
	gnutls_x509_crt_t *certs,
	int                nrcerts
);

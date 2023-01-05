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

#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <gnutls/abstract.h>

#include "common-cert.h"
#include "make-signature.h"

int
make_signature(
	gnutls_pkcs7_t *pkcs7,
	const void *data,
	size_t size,
	gnutls_privkey_t key,
	gnutls_x509_crt_t *certs,
	int nrcerts
) {
	int rc, idx, jdx;
	gnutls_datum_t datum;
	unsigned int signflags = GNUTLS_PKCS7_EMBED_DATA | GNUTLS_PKCS7_INCLUDE_CERT | GNUTLS_PKCS7_INCLUDE_TIME;
	const gnutls_digest_algorithm_t algo = GNUTLS_DIG_SHA256;
	const gnutls_keyid_flags_t keyflags = GNUTLS_KEYID_USE_SHA256;
	gnutls_x509_privkey_t xkey;
	unsigned char keyid[256], certid[256];
	size_t szkeyid, szcertid;

	/* get the x509 key id */
	rc = gnutls_privkey_export_x509(key, &xkey);
	if (rc != GNUTLS_E_SUCCESS)
		return -EINVAL;
	szkeyid = sizeof keyid;
	rc = gnutls_x509_privkey_get_key_id(xkey, keyflags, keyid, &szkeyid);
	if (rc != GNUTLS_E_SUCCESS)
		return -EOVERFLOW;
	gnutls_x509_privkey_deinit(xkey);

	/* search the certificate that signed */
	for (idx = nrcerts ; --idx >= 0 ; ) {

		/* key id of the certificate */
		szcertid = sizeof certid;
		rc = gnutls_x509_crt_get_key_id(certs[idx], keyflags, certid, &szcertid);
		if (rc != GNUTLS_E_SUCCESS)
			return -EOVERFLOW;

		/* compare the keys */
		if (szcertid == szkeyid && 0 == memcmp(keyid, certid, szkeyid)) {
			/* same keys, create the signature */
			rc = gnutls_pkcs7_init(pkcs7);
			if (rc != GNUTLS_E_SUCCESS)
				rc = -ENOMEM;
			else {
				/* sign the content and embed it */
				datum.data = (void*)data;
				datum.size = (unsigned)size;
				rc = gnutls_pkcs7_sign(*pkcs7, certs[idx], key, &datum, NULL, NULL, algo, signflags);
				if (rc != GNUTLS_E_SUCCESS)
					rc = -EFAULT;
				else {
					for (jdx = 0 ; rc == GNUTLS_E_SUCCESS && jdx < nrcerts ; jdx++)
						if (jdx != idx)
							rc = gnutls_pkcs7_set_crt(*pkcs7, certs[jdx]);
					if (rc == GNUTLS_E_SUCCESS)
						return 0;
					rc = -ENOMEM;
				}
				gnutls_pkcs7_deinit(*pkcs7);
			}
			return rc;
		}
	}
	return -EINVAL;
}

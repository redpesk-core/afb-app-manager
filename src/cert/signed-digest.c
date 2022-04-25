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
#include <errno.h>
#include <limits.h>

#include "common-cert.h"
#include "check-signature.h"
#include "make-signature.h"
#include "signature-name.h"
#include "digest.h"
#include "signed-digest.h"


int
make_signed_digest(
	gnutls_pkcs7_t    *pkcs7,
	path_entry_t      *root,
	int                isdistributor,
	gnutls_digest_algorithm_t algorithm,
	gnutls_privkey_t   key,
	gnutls_x509_crt_t *certs,
	int                nrcerts
) {
	void *buffer;
	size_t size;
	int rc;

	*pkcs7 = NULL;
	rc = create_digest(root, algorithm, NULL, 0, isdistributor);
	if (rc < 0)
		return rc;

	size = (size_t)rc;
	buffer = malloc(size);
	if (buffer == NULL)
		return -ENOMEM;

	rc = create_digest(root, algorithm, buffer, size, isdistributor);
	if (rc < 0) {
		free(buffer);
		return rc;
	}

	rc = make_signature(pkcs7, buffer, size, key, certs, nrcerts);
	free(buffer);
	return rc;
}

int
check_signed_digest(
	gnutls_pkcs7_t    pkcs7,
	path_entry_t      *root,
	int                isdistributor,
	gnutls_x509_crt_t *trustcerts,
	int                nrtrustcerts,
	domain_spec_t     *spec
) {
	int rc;
	gnutls_datum_t data;

	/* get signed data */
	data.size = 0;
	data.data = NULL;
	rc = gnutls_pkcs7_get_embedded_data(pkcs7, 0, &data);
	if (rc != GNUTLS_E_SUCCESS) {
		return -EINVAL;
	}

	/* check the digest */
	rc = check_digest(root, (char*)data.data, data.size, isdistributor);
	gnutls_free(data.data);
	if (rc < 0)
		return rc;

	/* check the signature */
	rc = check_signature(pkcs7, spec, trustcerts, nrtrustcerts);
	return rc;
}

struct check_all {
	path_entry_t      *root;
	gnutls_x509_crt_t *trustcerts;
	int                nrtrustcerts;
	domain_spec_t     *spec;
	int                rc;
};

static void get_domains_cb(const char *name, domain_permission_t perm, void *closure)
{
	struct check_all *ca = closure;

	if (perm == domain_permission_grants)
		ca->rc = domain_spec_set(ca->spec, perm, name);
}

static int check_one_of_all(struct check_all *ca, path_entry_t *entry, const char *path, size_t length, int isdistributor)
{
	domain_spec_t specs = DOMAIN_SPEC_INITIAL;
	gnutls_pkcs7_t pkcs7;
	int rc;

	rc = read_pkcs7(path, &pkcs7);
	if (rc == 0) {
		rc = check_signed_digest(pkcs7, ca->root, isdistributor,
				ca->trustcerts, ca->nrtrustcerts, &specs);
		gnutls_pkcs7_deinit(pkcs7);
		if (rc >= 0 && ca->spec != NULL) {
			domain_spec_enum(&specs, get_domains_cb, ca);
			rc = ca->rc;
		}
	}
	domain_spec_reset(&specs);
	ca->rc = rc;
	return !rc;
}

static int check_all_cb(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	struct check_all *ca = closure;
	signature_type_t st = signature_name_type_length(path, length);
	switch (st) {
	case Signature_Author:
	case Signature_distributor:
		check_one_of_all(ca, entry, path, length, st == Signature_distributor);
		break;
	default:
	case Signature_None:
		break;
	}
	return 0;
}

int
check_signed_digest_of_files(
	path_entry_t      *root,
	gnutls_x509_crt_t *trustcerts,
	int                nrtrustcerts,
	domain_spec_t     *spec
) {
	struct check_all ca = {
		.root = root,
		.trustcerts = trustcerts,
		.nrtrustcerts = nrtrustcerts,
		.spec = spec,
		.rc = 0
	};

	path_entry_for_each(
			PATH_ENTRY_FORALL_ONLY_ADDED | PATH_ENTRY_FORALL_ABSOLUTE,
			root,
			check_all_cb,
			&ca);

	return ca.rc;
}

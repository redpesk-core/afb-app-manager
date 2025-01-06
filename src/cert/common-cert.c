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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

#include "common-cert.h"

bool
is_issuer(
	gnutls_x509_crt_t cert,
	gnutls_x509_crt_t issuer,
	bool verify
) {
	unsigned verif = 0;
	if (!gnutls_x509_crt_check_issuer(cert, issuer))
		return false;
	if (!verify)
		return true;
	return gnutls_x509_crt_verify(cert, &issuer, 1, 0, &verif) == GNUTLS_E_SUCCESS
	    && verif == 0;
}

int
get_domain_spec_of_cert(
	gnutls_x509_crt_t cert,
	domain_spec_t *spec
) {
	size_t size;
	char scratch[128];
	unsigned crit;
	int rc;

	size = sizeof scratch - 1;
	rc = gnutls_x509_crt_get_extension_by_oid(cert, OID_EXT_DOMAIN_SPEC, 0, scratch, &size, &crit);
//fprintf(stdout, "get ext %d\n", rc);
	if (rc == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
		rc = -ENOENT;
	else if (rc != GNUTLS_E_SUCCESS)
		rc = -EINVAL;
	else {
		scratch[size] = 0;
//fprintf(stdout, "found ext %s:\n", scratch);
		rc = get_domain_spec_of_string(scratch, spec);
//fprintf(stdout, "->spec %d:\n", rc);
	}
	if (rc < 0)
		domain_spec_reset(spec);
	return rc;
}

int
check_crt_chain(
	check_crt_t *ckcrt,
	int targetidx,
	domain_spec_t *spec
) {
	domain_spec_t superspec = DOMAIN_SPEC_INITIAL;
	int idx, rc;

//fprintf(stdout, "checking trust of idx %d:\n", targetidx);

	/* search issuer of certificate of targetidx in common list */
	for (idx = 0 ; idx < ckcrt->nrcerts ; idx++) {
		if (idx != targetidx /* don't checks self signed */
		 && is_issuer(ckcrt->certs[targetidx], ckcrt->certs[idx], true)) {
			/* certificate of current idx issued the target certificate
			 * check if it is itself trusted */
			rc = check_crt_chain(ckcrt, idx, spec != NULL ? &superspec : NULL);
			if (rc >= 0) {
				/* a trusted certificate signed the target */
				if (spec == NULL)
					rc = 0;
				else {
					/* retrieves the domain specification of capabilities */
					rc = get_domain_spec_of_cert(ckcrt->certs[targetidx], spec);
					if (rc == -ENOENT)
						rc = 0;
					else if (rc == 0 && !is_domain_spec_able_to_sign(&superspec, spec)) {
						domain_spec_reset(spec);
						rc = -EINVAL;
					}
				}
			}
			return rc;
		}
	}

	/* no common certificate sined the target, search trusted certificates */
	for (idx = 0 ; idx < ckcrt->nrtrusteds ; idx++) {
		if (is_issuer(ckcrt->certs[targetidx], ckcrt->trusteds[idx], true))
			return spec != NULL ? get_domain_spec_of_cert(ckcrt->trusteds[idx], spec) : 0;
	}

	return -ENOENT;
}

void
sort_certs(
	gnutls_x509_crt_t *certs,
	int nrcerts,
	bool verify
) {
	int cnt, idx;
	gnutls_x509_crt_t tmp;

	for (cnt = 0 ; cnt < nrcerts ; cnt++) {
		idx = cnt + 1;
		while (idx < nrcerts) {
			if (!is_issuer(certs[cnt], certs[idx], verify))
				idx++;
			else {
				tmp = certs[cnt];
				certs[cnt] = certs[idx];
				certs[idx] = tmp;
				idx = cnt + 1;
			}
		}
	}
}

static
int read_file(const char *file, unsigned char **pointer, size_t *size)
{
	size_t sread, sz, len = 0;
	unsigned char *ptr, *buffer = NULL;
	FILE *f;

	f = fopen(file, "rb");
	if (f == NULL)
		return -errno;
	for (;;) {
		sz = len + 32768;
		ptr = realloc(buffer, sz);
		if (ptr == NULL)
			break;
		buffer = ptr;
		sread = fread(&buffer[len], 1, sz - len, f);
		if (sread == 0) {
			ptr = realloc(buffer, len);
			if (ptr == NULL)
				break;
			*pointer = ptr;
			*size = len;
			fclose(f);
			return 0;
		}
		len += (size_t)sread;
	}
	fclose(f);
	return -ENOMEM;
}

int read_pkcs7(const char *file, gnutls_pkcs7_t *pkcs7)
{
	int rc;
	size_t size;
	gnutls_datum_t datum;

	rc = read_file(file, &datum.data, &size);
	if (rc < 0)
		return rc;

	datum.size = (unsigned)size;
	gnutls_pkcs7_init(pkcs7);
	rc = gnutls_pkcs7_import(*pkcs7, &datum, GNUTLS_X509_FMT_PEM);
	free(datum.data);
	if (rc != GNUTLS_E_SUCCESS) {
		gnutls_pkcs7_deinit(*pkcs7);
		*pkcs7 = NULL;
		return -EINVAL;
	}
	return 0;
}

int read_certificates(const char *file, gnutls_x509_crt_t *certs, int maxnr)
{
	int rc;
	size_t size;
	gnutls_datum_t datum;
	unsigned count;

	rc = read_file(file, &datum.data, &size);
	if (rc < 0)
		return rc;

	datum.size = (unsigned)size;
	count = (unsigned)maxnr;
	rc = gnutls_x509_crt_list_import(certs, &count, &datum, GNUTLS_X509_FMT_PEM, 0);
	free(datum.data);
	return rc < 0 ? -EINVAL : (int)count;
}

int read_private_key(const char *file, gnutls_privkey_t *key)
{
	int rc;
	size_t size;
	gnutls_datum_t datum;
	gnutls_x509_privkey_t xkey;

	*key = NULL;
	rc = read_file(file, &datum.data, &size);
	if (rc < 0)
		return rc;

	rc = gnutls_x509_privkey_init(&xkey);
	if (rc != GNUTLS_E_SUCCESS) {
		free(datum.data);
		return -ENOMEM;
	}

	datum.size = (unsigned)size;
	rc = gnutls_x509_privkey_import(xkey, &datum, GNUTLS_X509_FMT_PEM);
	free(datum.data);
	if (rc != GNUTLS_E_SUCCESS) {
		gnutls_x509_privkey_deinit(xkey);
		return -EINVAL;
	}

	rc = gnutls_privkey_init(key);
	if (rc == GNUTLS_E_SUCCESS) {
		rc = gnutls_privkey_import_x509(*key, xkey, GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE);
		if (rc == GNUTLS_E_SUCCESS)
			return 0;
	}
	*key = NULL;
	gnutls_x509_privkey_deinit(xkey);
	return -ENOMEM;
}

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

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "check-signature.h"
#include "common-cert.h"

#define ERRMSG(...) fprintf(stderr, __VA_ARGS__)

/**
 * structure internally used for storing data
 */
typedef
struct
{
	/** holds the currently checked signature */
	gnutls_pkcs7_t pkcs7;
	/** info about the signer */
	gnutls_pkcs7_signature_info_st info;
	/** signed data */
	gnutls_datum_t data;
	/** structure holding certificates for verifying trust */
	check_crt_t ckcrt;
}
	check_pkcs7_t;

/**
 * cleanup ck7 structure
 */
static void
deinit_check_pkcs7(
	check_pkcs7_t *ck7
) {
	/* clean certificates copied from pkcs7 */
	while (ck7->ckcrt.nrcerts)
		gnutls_x509_crt_deinit(ck7->ckcrt.certs[--ck7->ckcrt.nrcerts]);
	free(ck7->ckcrt.certs);
	/* clean data taken from pkcs7 */
	if (ck7->data.data)
		gnutls_free(ck7->data.data);
	/* clean signer info taken from pkcs7 if present */
	if (ck7->pkcs7)
		gnutls_pkcs7_signature_info_deinit(&ck7->info);
	/* reset the memory */
	memset(ck7, 0, sizeof *ck7);
}

/**
 * really init without cleaning on error
 */
static int
_init_check_pkcs7(
	gnutls_pkcs7_t pkcs7,
	check_pkcs7_t *ck7
) {
	int rc, ncrts;
	gnutls_datum_t data;

	/* reset */
	memset(ck7, 0, sizeof *ck7);

	/* check that the content is only signed with one certificate */
	rc = gnutls_pkcs7_get_signature_count(pkcs7);
	if (rc != 1) {
		ERRMSG("Signature has not exactly one signer\n");
		return -1;
	}

	/* get information about the signature */
	rc = gnutls_pkcs7_get_signature_info(pkcs7, 0, &ck7->info);
	if (rc != GNUTLS_E_SUCCESS) {
		ERRMSG("can't extract signature informations\n");
		return -1;
	}

	/* mark existing info */
	ck7->pkcs7 = pkcs7;

	/* get signed data */
	rc = gnutls_pkcs7_get_embedded_data(pkcs7, 0, &ck7->data);
	if (rc != GNUTLS_E_SUCCESS) {
		ERRMSG("can't extract signed data\n");
		return -1;
	}

	/* get the count of embedded certificates */
	rc = gnutls_pkcs7_get_crt_count(pkcs7);
	if (rc < 0) {
		ERRMSG("can't extract count of certificates\n");
		return -1;
	}

	/* get the certificates */
	ncrts = rc;
	if (ncrts > 0) {
		/* allocates the list of certificates */
		ck7->ckcrt.certs = calloc((size_t)ncrts, sizeof *ck7->ckcrt.certs);
		if (ck7->ckcrt.certs == NULL) {
			ERRMSG("out of memory\n");
			return -1;
		}

		while (ck7->ckcrt.nrcerts < ncrts) {
			/* get size of the certificate */
			rc = gnutls_pkcs7_get_crt_raw2(pkcs7, (unsigned)ck7->ckcrt.nrcerts, &data);
			if (rc != GNUTLS_E_SUCCESS) {
				ERRMSG("can't get certificate size\n");
				return -1;
			}
			/* init the destination certificate */
			rc = gnutls_x509_crt_init(&ck7->ckcrt.certs[ck7->ckcrt.nrcerts]);
			if (rc != GNUTLS_E_SUCCESS) {
				gnutls_free(data.data);
				ERRMSG("can't init certificate\n");
				return -1;
			}
			/* import destination certificate */
			rc = gnutls_x509_crt_import(ck7->ckcrt.certs[ck7->ckcrt.nrcerts++], &data, GNUTLS_X509_FMT_DER);
			gnutls_free(data.data);
			if (rc != GNUTLS_E_SUCCESS) {
				ERRMSG("can't import certificate\n");
				return -1;
			}
		}
	}
	return 0;
}

/**
 * init ck7 from pkcs7 but clean on error
 */
static int
init_check_pkcs7(
	gnutls_pkcs7_t pkcs7,
	check_pkcs7_t *ck7
) {
	int rc = _init_check_pkcs7(pkcs7, ck7);
	if (rc < 0)
		deinit_check_pkcs7(ck7);
	return rc;
}

/*
void pcrt(gnutls_x509_crt_t c, FILE *f) {
	gnutls_datum_t d;
	gnutls_x509_crt_print(c, GNUTLS_CRT_PRINT_FULL, &d);
	fprintf(stdout, "%.*s\n", d.size, d.data);
	gnutls_free(d.data);
}

void pbuf(const char *prefix, unsigned char *data, size_t size)
{
	size_t idx;
	fprintf(stdout, "%s ASCII: ", prefix);
	for (idx = 0; idx < size ; idx++)
		fprintf(stdout, "%c", data[idx] < 32 || 126 < data[idx] ? '.' : data[idx]);
	fprintf(stdout, "\n%s  HEXA: ", prefix);
	for (idx = 0; idx < size ; idx++)
		fprintf(stdout, "%02x", (int)data[idx]);
	fprintf(stdout, "\n");
}

void pdat(const char *prefix, const gnutls_datum_t data)
{
	pbuf(prefix, data.data, data.size);
}
*/

/**
 * search the index of the certificate that signed
 * the pkcs7 data
 */
static int
get_signer_crt_index(
	check_pkcs7_t *ck7
) {
	gnutls_x509_dn_t idn;
	unsigned char scratch[1000];
	size_t size;
	int rc, idx = ck7->ckcrt.nrcerts, matches;

	while(idx--) {
		matches = 0;
		size = sizeof scratch;
		if (ck7->info.signer_serial.size > 0) {
			rc = gnutls_x509_crt_get_serial(ck7->ckcrt.certs[idx], scratch, &size);
			matches = rc == GNUTLS_E_SUCCESS
				&& size == ck7->info.signer_serial.size
				&& memcmp(ck7->info.signer_serial.data, scratch, size) == 0;
		}
		else if (ck7->info.issuer_keyid.size > 0) {
			rc = gnutls_x509_crt_get_subject_key_id(ck7->ckcrt.certs[idx], scratch, &size, NULL);
			matches =  rc == GNUTLS_E_SUCCESS
				&& size == ck7->info.issuer_keyid.size
				&& memcmp(ck7->info.issuer_keyid.data, scratch, size) == 0;
		}
		if (matches) {
			rc = gnutls_x509_crt_get_issuer(ck7->ckcrt.certs[idx], &idn);
			if (rc == GNUTLS_E_SUCCESS) {
				size = sizeof scratch;
				rc = gnutls_x509_dn_export(idn, GNUTLS_X509_FMT_DER, scratch, &size);
				if (rc == GNUTLS_E_SUCCESS
				&& size == ck7->info.issuer_dn.size
				&& memcmp(ck7->info.issuer_dn.data, scratch, size) == 0)
					break; /* found for sure */
			}
		}
	}
	return idx;
}

int
check_signature(
	gnutls_pkcs7_t pkcs7,
	domain_spec_t *spec,
	gnutls_x509_crt_t *trusteds,
	int nrtrusteds
) {
	return check_signature_with_crl(pkcs7, spec, trusteds, nrtrusteds, NULL, 0);
}

int
check_signature_with_crl(
	gnutls_pkcs7_t pkcs7,
	domain_spec_t *spec,
	gnutls_x509_crt_t *trusteds,
	int nrtrusteds,
	gnutls_x509_crl_t *crls,
	int nrcrls
) {
	int rc, signer_idx;
	check_pkcs7_t ck7;

	/* initialize data used for the check */
	rc = init_check_pkcs7(pkcs7, &ck7);
	if (rc >= 0) {

		/* fullfil missing parts of ck7.ckcrt */
		ck7.ckcrt.trusteds = trusteds;
		ck7.ckcrt.nrtrusteds = nrtrusteds;
		ck7.ckcrt.crls = crls;
		ck7.ckcrt.nrcrls = nrcrls;

		/* search the certificate that signed */
		rc = get_signer_crt_index(&ck7);
		if (rc < 0)
			ERRMSG("can't find signer certificate\n");
		else {
			/* verify the signature of the content */
			signer_idx = rc;
			rc = gnutls_pkcs7_verify_direct(pkcs7, ck7.ckcrt.certs[signer_idx], 0, &ck7.data, 0);
			if (rc != GNUTLS_E_SUCCESS) {
				ERRMSG("can't verify signature\n");
				rc = -1;
			}
			else {
				/* check that the signer is trusted */
				rc = check_crt_chain(&ck7.ckcrt, signer_idx, spec);
				if (rc < 0)
					ERRMSG("can't trust signature\n");
			}
		}
		deinit_check_pkcs7(&ck7);
	}
	return rc;
}

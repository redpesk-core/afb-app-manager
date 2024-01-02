/*
 Copyright (C) 2015-2024 IoT.bzh Company

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

#include <string.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-base64.h>

#include "wgtpkg-certs.h"


#if WITH_OPENSSL

#include <openssl/x509.h>
typedef X509 *cert_t;

static void cert_destroy(cert_t cert)
{
	X509_free(cert);
}

static int cert_import(cert_t *certs, int maxcount, const unsigned char *bin, size_t len)
{
	X509 *x;
	int rc, nr = 0;
	const char *b, *e;
	b = bin;
	e = bin + len;
	while (b < e) {
		x = nr < maxcount ? d2i_X509(NULL, (const unsigned char **)&b, e-b) : NULL;
		if (x == NULL) {
			RP_ERROR("import failed");
			while(nr)
				cert_destroy(certs[--nr]);
			return -1;
		}
		certs[nr++] = x;
	}
	return nr;
}

#else

#include <gnutls/x509.h>
typedef gnutls_x509_crt_t cert_t;

static void cert_destroy(cert_t cert)
{
	gnutls_x509_crt_deinit(cert);
}

static int cert_import(cert_t *certs, int maxcount, const unsigned char *bin, size_t len)
{
	unsigned count = (unsigned)maxcount;
	gnutls_datum_t data = { .data = (void*)bin, .size = (unsigned)len };
	int rc = gnutls_x509_crt_list_import(certs, &count, &data, GNUTLS_X509_FMT_PEM, GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
	return rc;
}

#endif

struct x509l {
	unsigned count;
	cert_t *certs;
};

static struct x509l certificates = { .count = 0, .certs = NULL };

static int add_certificate_x509(cert_t x)
{
	cert_t *p = realloc(certificates.certs,
			(certificates.count + 1) * sizeof *p);
	if (!p) {
		RP_ERROR("reallocation failed for certificate");
		return -1;
	}
	certificates.certs = p;
	p[certificates.count++] = x;
	return 0;
}

static int add_certificate_bin(const unsigned char *bin, size_t len)
{
	int rc, idx, nr;
	cert_t certs[10];

	rc = cert_import(certs, sizeof certs / sizeof *certs, bin, len);
	if (rc >= 0)
		for (idx = 0, nr = rc ; idx < nr ; idx++) {
			if (rc >= 0)
				rc = add_certificate_x509(certs[idx]);
			if (rc < 0)
				cert_destroy(certs[idx]);
		}
	return rc;
}

int add_certificate_b64(const char *b64)
{
	unsigned char *d;
	size_t l;
	int rc;

	rc = rp_base64_decode(b64, strlen(b64), &d, &l, 0);
	if (rc != rp_base64_ok)
		rc = -1;
	else {
		rc = add_certificate_bin(d, l);
		free(d);
	}
	return rc;
}

void clear_certificates()
{
	while(certificates.count)
		cert_destroy(certificates.certs[--certificates.count]);
	free(certificates.certs);
	certificates.certs = NULL;
}

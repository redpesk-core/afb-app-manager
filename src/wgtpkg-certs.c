/*
 Copyright (C) 2015-2020 IoT.bzh Company

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


#include <openssl/x509.h>

#include "verbose.h"
#include "wgtpkg-certs.h"
#include "wgtpkg-base64.h"

struct x509l {
	unsigned count;
	X509 **certs;
};

static struct x509l certificates = { .count = 0, .certs = NULL };

static int add_certificate_x509(X509 *x)
{
	X509 **p = realloc(certificates.certs,
			(certificates.count + 1) * sizeof(X509*));
	if (!p) {
		ERROR("reallocation failed for certificate");
		return -1;
	}
	certificates.certs = p;
	p[certificates.count++] = x;
	return 0;
}

static int add_certificate_bin(const char *bin, size_t len)
{
	int rc;
	const char *b, *e;
	b = bin;
	e = bin + len;
	while (b < e) {
		X509 *x =  d2i_X509(NULL, (const unsigned char **)&b, e-b);
		if (x == NULL) {
			ERROR("d2i_X509 failed");
			return -1;
		}
		rc = add_certificate_x509(x);
		if (rc) {
			X509_free(x);
			return rc;
		}
	}
	return 0;
}

int add_certificate_b64(const char *b64)
{
	char *d;
	ssize_t l = base64dec(b64, &d);
	int rc;
	if (l < 0)
		rc = -1;
	else {
		rc = add_certificate_bin(d, (size_t)l);
		free(d);
	}
	return rc;
}

void clear_certificates()
{
	while(certificates.count)
		X509_free(certificates.certs[--certificates.count]);
}



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

//#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

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


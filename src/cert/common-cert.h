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

#include <stdbool.h>

#include <gnutls/x509.h>
#include "domain-spec.h"

/**
 * This is the extended key OID used to track the
 * domain capabilities of certificates.
 */
#define OID_EXT_DOMAIN_SPEC   "1.3.9.812.383.370.36.1"

/**
 * This routine check if issuer certificate issued cert.
 * If verify, the signature of the certificate is checked,
 * otherwise, if not verify, only the issuer name is checked.
 *
 * @param cert the certificate to verify
 * @param issuer the possible issuer
 * @param verify if true, signature is checked
 * @result true if issuer certificate issued cert
 */
bool
is_issuer(
	gnutls_x509_crt_t cert,
	gnutls_x509_crt_t issuer,
	bool verify
);

/**
 * Extract from cert its domain specification
 *
 * @param cert the certificate
 * @param spec the spec to retrieve (must be empty)
 * @return 
 */
int
get_domain_spec_of_cert(
	gnutls_x509_crt_t cert,
	domain_spec_t *spec
);

/**
 * structure for verifying chain of certificates
 */
typedef  
struct
{
	/** array of certificates for the chain */
	gnutls_x509_crt_t *certs;
	/** count of certificates in the array */
	int nrcerts;
	/** array of trusted certificates */
	gnutls_x509_crt_t *trusteds;
	/** count of trusted certificates in the array */
	int nrtrusteds;
	/** TODO */
	gnutls_x509_crl_t *crls;
	/** TODO */
	int nrcrls;
}
check_crt_t;

/**
 * Check the chain of trust
 *
 * @param ckcrt the certificates
 * @param targetidx index of the certificate to check
 * @param spec if not NULL, check the domain capabilities and return those of target
 * @return 0 in case of success or a negative number otherwise
 */
int
check_crt_chain(
	check_crt_t *ckcrt,
	int targetidx,
	domain_spec_t *spec
);

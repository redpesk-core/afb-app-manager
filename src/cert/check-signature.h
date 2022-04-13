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

#include <gnutls/pkcs7.h>

#include "domain-spec.h"

/**
 * Checks that the signature given in pkcs7
 * is valid according the trusted list of
 * certificates
 *
 * @param pkcs7 the signature to be checked
 * @param spec if not NULL, the domain capabilities
 *             are checked and returned in spec
 * @param trusteds array of trusted certificates
 * @param nrtrusteds count of certificates in array of trusted
 * @param crls TODO
 * @param nrcrls TODO
 * @return zero on successful verification or a negative error code
 */
int
check_signature_with_crl(
	gnutls_pkcs7_t pkcs7,
	domain_spec_t *spec,
	gnutls_x509_crt_t *trusteds,
	int nrtrusteds,
	gnutls_x509_crl_t *crls,
	int nrcrls
);

/**
 * Same as @see check_signature_with_crl with
 * crl_list_size = 0.
 */
int
check_signature(
	gnutls_pkcs7_t pkcs7,
	domain_spec_t *spec,
	gnutls_x509_crt_t *trusteds,
	int nrtrusteds
);

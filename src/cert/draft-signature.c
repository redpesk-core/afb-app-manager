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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>


/* oid-reference: https://oidref.com/  ou  http://oid-info.com/ */

#define OID_redpesk_cert_perm "1.3.9.812.383.370.00036.1"

/**
 * @brief main root domain spec
 */
#define ROOT_DOMAIN_SPEC "@system,@platform,@partner,@public"


/************************************************************************/

/**
 * @brief simple node for a list of strings
 */
typedef
struct slist_node {
	/** next node of the list */
	struct slist_node *next;
	/** value of the node (copied) */
	char value[];
} slist_node_t;

/**
 * @brief simple sorted list of strings
 */
typedef
struct slist {
	/** first node of the list */
	slist_node_t *root;
} slist_t;

/**
 * @brief create a simple string list
 * 
 * @param slist result
 * @return int 0 if success or -ENOMEM
 */
int slist_create(slist_t **slist)
{
	*slist = (slist_t*)calloc(1, sizeof(slist_t));
	return NULL == *slist ? -ENOMEM : 0;
}

/**
 * @brief destroys the given list
 * 
 * @param slist the list to destroy
 */
void slist_destroy(slist_t *slist)
{
	if (slist != NULL) {
		slist_node_t *node = slist->root;
		while (node != NULL) {
			void *ptr = node;
			node = node->next;
			free(ptr);
		}
		free(slist);
	}
}

/**
 * @brief add a string of given length in the simple list
 * 
 * @param slist the simple string list to change
 * @param value the string value to add (copied)
 * @param length the length of the string value to add (not including null)
 * @return int 0 on success or -ENOMEM on error
 */
int slist_add(slist_t *slist, const char *value, size_t length)
{
	if (slist) {
		int cmp;
		slist_node_t *node, **prev = &slist->root;
		while ((node = *prev) != NULL && (cmp = memcmp(value, node->value, length)) > 0)
			prev = &node->next;
		if (node == NULL || cmp != 0 || node->value[length]) {
			node = malloc(length + 1 + sizeof *node);
			if (node == NULL)
				return -ENOMEM;
			memcpy(node->value, value, length);
			node->value[length] = 0;
			node->next = *prev;
			*prev = node;
		}
	}
	return 0;
}

/**
 * @brief add a zero terminated string in the simple list
 * 
 * @param slist the simple string list to change
 * @param value the string value to add (copied)
 * @return int 0 on success or -ENOMEM on error
 */
int slist_addz(slist_t *slist, const char *value)
{
	return slist_add(slist, value, strlen(value));
}

/**
 * @brief check that all files of the list are accessibles
 * (access) for the mode
 * 
 * @param slist the simple list
 * @param mode the mode of access to check
 * @return int 
 */
int slist_check_access(slist_t *slist, int mode)
{
	slist_node_t *node = slist ? slist->root : NULL;
	while (node != NULL) {
		if (access(node->value, mode))
			return -errno;
		node = node->next;
	}
	return 0;
}

/**
 * @brief remove the prefix if present
 * 
 * @param value the value to unprefix
 * @param prefix the prefix to remove
 * @param prefixlength the length of the prefix
 * @return const char* the value without the prefix if present
 */
static const char *unprefix(const char *value, const char *prefix, size_t prefixlength)
{
	return &value[strncmp(value, prefix, prefixlength) ? 0 : prefixlength];
}

/**
 * @brief compute the hash file of sha256sum of files of the list
 * (equivalent to the command sha256sum).
 * 
 * @param slist the simple list
 * @param hstring where to store the result
 * @param hlength where to store the length
 * @param prefix the prefix to remove
 * @param prefixlength length of prefix to remove
 * @return int 0 in case success or -ENOMEM if error of memory
 * or -EOVERFLOW on internal unexpected defect.
 */
int slist_hash_prefix_files(slist_t *slist, char **hstring, size_t *hlength, const char *prefix, size_t prefixlength)
{
	int i;
	char *hash;
	size_t pos, size, rlen;
	slist_node_t *node;
	gnutls_hash_hd_t hctx;
	const gnutls_digest_algorithm_t algo = GNUTLS_DIG_SHA256;
	const unsigned digsz = 256 / 8;
	unsigned char shadgst[digsz];
	FILE *file;
	unsigned char buffer[65500];

	/* compute the size */
	size = 0;
	node = slist ? slist->root : NULL;
	while (node != NULL) {
		size += 2 * digsz + 1 + 1;
		size += strlen(unprefix(node->value, prefix, prefixlength));
		node = node->next;
	}

	/* allocate the result */
	if (hlength != NULL)
		*hlength = size;
	if (hstring == NULL)
		return 0;
	*hstring = hash = malloc(++size);
	if (hash == NULL)
		return -ENOMEM;

	/* compute the result */
	pos = 0;
	node = slist ? slist->root : NULL;
	while (node != NULL) {
		file = fopen(node->value, "rb");
		if (file == NULL) {
			free(hash);
			*hstring = NULL;
			return -errno;
		}
		gnutls_hash_init(&hctx, algo);
		while ((rlen = fread(buffer, 1, sizeof buffer, file)) > 0)
			gnutls_hash(hctx, buffer, rlen);
		gnutls_hash_output(hctx, shadgst);
		for (i = 0 ; i < digsz ; i++)
			if (pos < size)
				pos += snprintf(&hash[pos], size - pos, "%02x", (int)shadgst[i]);
		gnutls_hash_deinit(hctx, shadgst);
		if (pos < size)
			pos += snprintf(&hash[pos], size - pos, " %s\n", unprefix(node->value, prefix, prefixlength));
		node = node->next;
	}
	if (pos >= size) {
		free(hash);
		*hstring = NULL;
		return -EOVERFLOW;
	}
	hash[pos] = '\0';
	return 0;
}

/**
 * @brief compute the hash file of sha256sum of files of the list
 * (equivalent to the command sha256sum).
 * 
 * @param slist the simple list
 * @param hstring where to store the result
 * @param hlength where to store the length
 * @param prefix the prefix to remove (zero terminated)
 * @return int 0 in case success or -ENOMEM if error of memory
 * or -EOVERFLOW on internal unexpected defect.
 */
int slist_hash_prefixz_files(slist_t *slist, char **hstring, size_t *hlength, const char *prefix)
{
	return slist_hash_prefix_files(slist, hstring, hlength, prefix, prefix ? strlen(prefix) : 0);
}

/**
 * @brief compute the hash file of sha256sum of files of the list
 * (equivalent to the command sha256sum).
 * 
 * @param slist the simple list
 * @param hstring where to store the result
 * @param hlength where to store the length
 * @return int 0 in case success or -ENOMEM if error of memory
 * or -EOVERFLOW on internal unexpected defect.
 */
int slist_hash_files(slist_t *slist, char **hstring, size_t *hlength)
{
	return slist_hash_prefix_files(slist, hstring, hlength, NULL, 0);
}

/************************************************************************/

const char stdkey[] = 
	"-----BEGIN RSA PRIVATE KEY-----\n"
	"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\n"
	"-----END RSA PRIVATE KEY-----\n"
;

int get_key(gnutls_x509_privkey_t *key, int newkey)
{
        /* Initialize an empty private key.
         */
        gnutls_x509_privkey_init(key);
	if (newkey) {
		/* Generate an RSA key of moderate security.
		*/
		unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_RSA, GNUTLS_SEC_PARAM_HIGH);
		return gnutls_x509_privkey_generate(*key, GNUTLS_PK_RSA, bits, 0);

	}
	else {
		gnutls_datum_t kd = { .data = (void*)stdkey, .size = sizeof stdkey - 1 };
		return gnutls_x509_privkey_import(*key, &kd, GNUTLS_X509_FMT_PEM);
	}
}

int generate_root_certificate(gnutls_x509_crt_t *crt, gnutls_x509_privkey_t xkey)
{
        gnutls_privkey_t key;
	gnutls_pubkey_t pubkey;
	char serial[4] = { 0, 0, 0, 1 };
	int s;

        /* Get the root key.
         */
        gnutls_privkey_init(&key);
	gnutls_pubkey_init(&pubkey);

	s = gnutls_privkey_import_x509 (key, xkey, 0);
	printf("%d\n", s);
	s = gnutls_pubkey_import_privkey(pubkey, key, 0, 0);
	printf("%d\n", s);

        /* Initialize an empty certificate.
         */
        gnutls_x509_crt_init(crt);
	gnutls_x509_crt_set_version(*crt, 3);

        /* Add stuff to the distinguished name
         */
	s = gnutls_x509_crt_set_serial(*crt, serial, sizeof serial);
	printf("%d\n", s);

	s = gnutls_x509_crt_set_activation_time(*crt, time(NULL));
	printf("%d\n", s);
	s = gnutls_x509_crt_set_expiration_time(*crt, 0);
	printf("%d\n", s);

        s = gnutls_x509_crt_set_dn_by_oid(*crt, GNUTLS_OID_X520_COUNTRY_NAME, 0, "FR", 2);
	printf("%d\n", s);

        s = gnutls_x509_crt_set_dn_by_oid(*crt, GNUTLS_OID_X520_COMMON_NAME, 0, "José", strlen("José"));
	printf("%d\n", s);

	s = gnutls_x509_crt_set_extension_by_oid(*crt, OID_redpesk_cert_perm, ROOT_DOMAIN_SPEC, strlen(ROOT_DOMAIN_SPEC), 1);
	printf("%d\n", s);

        /* set the public key.
         */
	s = gnutls_x509_crt_set_pubkey(*crt, pubkey);
	printf("%d\n", s);

        /* Self sign the certificate request.
         */
	s = gnutls_x509_crt_privkey_sign(*crt, *crt, key, GNUTLS_DIG_SHA256, 0);
	printf("%d\n", s);

	gnutls_pubkey_deinit(pubkey);
        gnutls_privkey_deinit(key);

        return 0;
}

int print_root_certificate(void)
{
        gnutls_x509_crt_t crt;
        gnutls_x509_privkey_t xkey;
	gnutls_datum_t out;

        /* Get the root key.
         */
	get_key(&xkey, 0);

        /* Initialize an empty certificate.
         */
	generate_root_certificate(&crt, xkey);

        /* Export the PEM encoded certificate, and
         * display it.
         */
	out.data = NULL;
	out.size = 0;
        gnutls_x509_crt_export2(crt, GNUTLS_X509_FMT_PEM, &out);
        printf("Certificate: \n%s", out.data);
	gnutls_free(out.data);

        /* Export the PEM encoded private key, and
         * display it.
         */
	out.data = NULL;
	out.size = 0;
        gnutls_x509_privkey_export2_pkcs8(xkey, GNUTLS_X509_FMT_PEM, 0, 0, &out);
        printf("\n\nPrivate key: \n%s", out.data);
	gnutls_free(out.data);

        gnutls_x509_crt_deinit(crt);
        gnutls_x509_privkey_deinit(xkey);
        return 0;
}

/************************************************************************/

int toto(void)
{
        gnutls_x509_crt_t crt;
        gnutls_x509_crq_t crq;
        gnutls_x509_privkey_t key;

        unsigned char buffer[10 * 1024];
        size_t buffer_size = sizeof(buffer);
        unsigned int bits;

        /* Initialize an empty private key.
         */
        gnutls_x509_privkey_init(&key);

        /* Generate an RSA key of moderate security.
         */
        bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_RSA, GNUTLS_SEC_PARAM_HIGH);
        gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, bits, 0);

        /* Initialize an empty certificate request.
         */
        gnutls_x509_crq_init(&crq);

        /* Set the request version.
         */
        gnutls_x509_crq_set_version(crq, 1);

        /* Add stuff to the distinguished name
         */
        gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_COUNTRY_NAME,
                                      0, "FR", 2);

        gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_COMMON_NAME,
                                      0, "José", strlen("José"));

	gnutls_x509_crq_set_extension_by_oid(crq, OID_redpesk_cert_perm,
                                      ROOT_DOMAIN_SPEC, strlen(ROOT_DOMAIN_SPEC), 1);

        /* Associate the request with the private key
         */
        gnutls_x509_crq_set_key(crq, key);

        /* Self sign the certificate request.
         */
        gnutls_x509_crq_sign2(crq, key, GNUTLS_DIG_SHA256, 0);

        /* Initialize an empty certificate.
         */
        gnutls_x509_crt_init(&crt);

	/* Initialize the certificate
	 */
	gnutls_x509_crt_set_crq (crt, crq);

        /* Export the PEM encoded certificate request, and
         * display it.
         */
        gnutls_x509_crq_export(crq, GNUTLS_X509_FMT_PEM, buffer,
                               &buffer_size);

        printf("Certificate Request: \n%s", buffer);

        /* Export the PEM encoded certificate, and
         * display it.
         */
        gnutls_x509_crt_export(crt, GNUTLS_X509_FMT_PEM, buffer,
                               &buffer_size);

        printf("Certificate: \n%s", buffer);

        /* Export the PEM encoded private key, and
         * display it.
         */
        buffer_size = sizeof(buffer);
        gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM, buffer,
                                   &buffer_size);

        printf("\n\nPrivate key: \n%s", buffer);


        gnutls_x509_crq_deinit(crq);
        gnutls_x509_privkey_deinit(key);

        return 0;

}


/************************************************************************/

#if 0
int main(int ac, char **av)
{
	domain_spec_t sin, sout;
	unsigned it;
	char *str1, *str2;
	int s1, s2;

	for (it = 0 ; (it >> (2 * _domain_count_)) == 0 ; it++) {
		sin.sign = it & ((1 << _domain_count_) - 1);
		sin.allow = (it >> _domain_count_) & ((1 << _domain_count_) - 1);
		s1 = get_string_of_domain_spec(&sin, &str1);
		s2 = get_domain_spec_of_string(str1, &sout);
		get_string_of_domain_spec(&sout, &str2);
		printf ("%d: %s %s [%s %d/%d]\n", it, str1, str2, sin.allow==sout.allow&&sin.sign==sout.sign?"ok":"KO!",s1,s2);
		free(str1);
		free(str2);
	}
}
#endif

#if 1
/* output the SHA256 sum of given arguments */
int main(int ac, char **av)
{
	print_root_certificate();
	//toto();
	return 0;
}
#endif

#if 0
/* output the SHA256 sum of given arguments */
int main(int ac, char **av)
{
	slist_t *slist;
	char *prf = NULL;
	char *sha = NULL;
	slist_create(&slist);
	while(*++av)
		if (**av == '-')
			prf = 1 + *av;
		else
			slist_addz(slist, *av);
	slist_hash_prefixz_files(slist, &sha, NULL, prf);
	printf("%s", sha);
	free(sha);
	slist_destroy(slist);
	return 0;
}
#endif

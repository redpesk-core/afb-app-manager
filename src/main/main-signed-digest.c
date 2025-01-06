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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "common-cert.h"
#include "signed-digest.h"

enum {
	ID_CMD_help,
	ID_CMD_make,
	ID_CMD_check,
	ID_CMD_validate
};

const char *commands[] = {
	[ID_CMD_help] = "help",
	[ID_CMD_make] = "make",
	[ID_CMD_check] = "check",
	[ID_CMD_validate] = "validate"
};

void help(const char *name, bool bad)
{
	char *r = strrchr(name, '/');
	if (r)
		name = r + 1;
	if (bad) {
		fprintf(stderr, "error, type '%s help' to get help.\n", name);
		exit(EXIT_FAILURE);
	}
	printf("usage: %s command args...\n", name);
	printf("\n");
	printf("commands are:\n");
	printf("\n");
	printf("  make [TYPE] [ALGO] FILELIST KEY CERT [CHAIN...]\n");
	printf("      \n");
	printf("\n");
	printf("  check [TYPE] FILELIST SIGFILE [TRUST...]\n");
	printf("      \n");
	printf("\n");
	printf("  validate FILELIST [TRUST...]\n");
	printf("      \n");
	printf("\n");
	printf("  help\n");
	printf("      This help\n");
	printf("\n");
	printf("where:\n");
	printf(" TYPE is one of --author --distributor\n");
	printf(" ALGO is one of --sha224 --sha256 --sha384 --sha512\n");
	printf(" FILELIST is the name of a file listing the content to digest\n");
	printf(" KEY is the private key used to sign\n");
	printf(" CERT is the certificate of the signer\n");
	printf(" CHAIN are the certificate chain of cert\n");
	printf(" TRUST is a list of certificates\n");
	printf("\n");
	exit(EXIT_SUCCESS);
}

int check_existing_file(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	//const char *filename = closure;
	if (access(path, F_OK)) {
		fprintf(stderr, "file not found %s\n", path);
		exit(EXIT_FAILURE);
	}
	if (access(path, R_OK)) {
		fprintf(stderr, "can't read file %s\n", path);
		exit(EXIT_FAILURE);
	}
	return 0;
}

void read_file_list(const char *filename, path_entry_t **root)
{
	int rc;
	FILE *f;

	f = fopen(filename, "rb");
	if (f == NULL) {
		fprintf(stderr, "Can't open file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	rc = path_entry_create_root(root);
	if (rc >= 0)
		rc = path_entry_add_from_file(*root, f);
	fclose(f);
	if (rc < 0) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	path_entry_for_each(PATH_ENTRY_FORALL_ONLY_ADDED, *root, check_existing_file, (void*)filename);
}

int read_remaining_certificates(char **av, int ai, int ac, gnutls_x509_crt_t *certs, int maxnr)
{
	int rc, n = 0;
	while (n < maxnr && ai < ac) {
		rc = read_certificates(av[ai++], &certs[n], maxnr - n);
		if (rc < 0) {
			fprintf(stderr, "Can't process file %s: %s\n", av[--ai], strerror(-rc));
			exit(EXIT_FAILURE);
		}
		n += rc;
	}
	return n;
}

int main(int ac, char **av)
{
	int cmd, rc, ai;
	int distributor = 0, ncerts;
	path_entry_t *root;
	gnutls_digest_algorithm_t algorithm = GNUTLS_DIG_SHA256;
	gnutls_x509_crt_t certs[10];
	gnutls_pkcs7_t pkcs7;
	gnutls_privkey_t key;
	domain_spec_t domains = DOMAIN_SPEC_INITIAL;
	gnutls_datum_t datum;
	char *domstr = NULL;

	/* is there a command? */
	if (ac < 2)
		help(av[0], true);

	/* search it */
	cmd = (int)(sizeof commands / sizeof *commands);
	while (strcmp(av[1], commands[--cmd]))
		if (!cmd) /* at end? not found !*/
			help(av[0], true);

	/* is for help ? */
	if (cmd == ID_CMD_help)
		help(av[0], false);

	/* check at least an extra argument */
	ai = 2;
	for ( ; ai < ac && av[ai][0] == '-' ; ai++ ) {
		if (0 == strcmp(av[ai], "--sha224"))
			algorithm = GNUTLS_DIG_SHA224;
		else if (0 == strcmp(av[ai], "--sha256"))
			algorithm = GNUTLS_DIG_SHA256;
		else if (0 == strcmp(av[ai], "--sha384"))
			algorithm = GNUTLS_DIG_SHA384;
		else if (0 == strcmp(av[ai], "--sha512"))
			algorithm = GNUTLS_DIG_SHA512;
		else if (0 == strcmp(av[ai], "--author"))
			distributor = 0;
		else if (0 == strcmp(av[ai], "--distributor"))
			distributor = 1;
		else
			help(av[0], true);
	}

	/* check at least one argument */
	if (ai == ac)
		help(av[0], true);

	read_file_list(av[ai++], &root);
	switch (cmd) {
	case ID_CMD_make:
		/* make [TYPE] [ALGO] FILELIST * KEY CERT CHAIN... */
		if (ai == ac)
			help(av[0], true);
		rc = read_private_key(av[ai++], &key);
		if (rc < 0) {
			fprintf(stderr, "can't read file %s: %s\n", av[--ai], strerror(-rc));
			exit(EXIT_FAILURE);
		}
		/* make [TYPE] [ALGO] FILELIST KEY * CERT CHAIN... */
		ncerts = read_remaining_certificates(av, ai, ac, certs, sizeof certs / sizeof *certs);
		rc = make_signed_digest(&pkcs7, root, distributor, algorithm, key, certs, ncerts);
		if (rc < 0) {
			fprintf(stderr, "creation failed: %s\n", strerror(-rc));
			exit(EXIT_FAILURE);
		}
		datum.data = NULL;
		datum.size = 0;
		rc = gnutls_pkcs7_export2(pkcs7, GNUTLS_X509_FMT_PEM, &datum);
		if (rc < 0) {
			fprintf(stderr, "failed to export pkcs7 data\n");
			exit(EXIT_FAILURE);
		}
		printf("%s", datum.data);
		gnutls_free(datum.data);
		gnutls_pkcs7_deinit(pkcs7);
		break;

	case ID_CMD_check:
		/* check [TYPE] FILELIST * SIGFILE [TRUST...] */
		if (ai == ac)
			help(av[0], true);
		rc = read_pkcs7(av[ai++], &pkcs7);
		if (rc < 0) {
			fprintf(stderr, "Can't read file %s: %s\n", av[--ai], strerror(-rc));
			exit(EXIT_FAILURE);
		}
		/* check [TYPE] FILELIST SIGFILE * [TRUST...] */
		ncerts = read_remaining_certificates(av, ai, ac, certs, sizeof certs / sizeof *certs);
		rc = check_signed_digest(pkcs7, root, distributor, certs, ncerts, &domains);
		if (rc < 0) {
			fprintf(stderr, "bad digest\n");
			exit(EXIT_FAILURE);
		}
		rc = get_string_of_domain_spec(&domains, &domstr);
		if (rc >= 0) {
			printf("%.*s\n", rc, domstr);
		}
		gnutls_pkcs7_deinit(pkcs7);
		break;

	case ID_CMD_validate:
		/* validate FILELIST * [TRUST...] */
		ncerts = read_remaining_certificates(av, ai, ac, certs, sizeof certs / sizeof *certs);

		rc = check_signed_digest_of_files(root, certs, ncerts, &domains);
		if (rc < 0) {
			fprintf(stderr, "bad digest\n");
			exit(EXIT_FAILURE);
		}
		rc = get_string_of_domain_spec(&domains, &domstr);
		if (rc >= 0) {
			printf("%.*s\n", rc, domstr);
		}
		break;
	}
	free(domstr);
	domain_spec_reset(&domains);
	exit(EXIT_SUCCESS);
	return 0;
}

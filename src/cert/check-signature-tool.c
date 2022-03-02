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

#include "check-signature.h"

enum {
	ID_CMD_help,
	ID_CMD_check
};

const char *commands[] = {
	[ID_CMD_help] = "help",
	[ID_CMD_check] = "check"
};

void help(const char *name, bool bad)
{
	char *r = strrchr(name, '/');
	if (r)
		name = r + 1;
	if (bad) {
		fprintf(stderr, "error, type `%s help` to get help.\n", name);
		exit(EXIT_FAILURE);
	}
	printf("usage: %s command args...\n", name);
	printf("\n");
	printf("where commands are:\n");
	printf("\n");
	printf("  check PKCS7 [ROOT.CERTIFICATE]\n");
	printf("      Check if the PKCS7 signature is valide and output its granted permissions\n");
	printf("\n");
	printf("  help\n");
	printf("      This help\n");
	printf("\n");
	exit(EXIT_SUCCESS);
}

void read_file(const char *file, unsigned char **pointer, unsigned *size)
{
	size_t sread, sz, len = 0;
	unsigned char *ptr, *buffer = NULL;
	FILE *f;

	f = fopen(file, "rb");
	if (f != NULL) {
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
				*size = (unsigned)len;
				fclose(f);
				return;
			}
			len += (size_t)sread;
		}
		fclose(f);
	}
	fprintf(stderr, "Can't read file %s\n", file);
	exit(EXIT_FAILURE);
}

void read_pkcs7(const char *file, gnutls_pkcs7_t *pkcs7)
{
	int rc;
	gnutls_datum_t datum;
	gnutls_pkcs7_init(pkcs7);
	read_file(file, &datum.data, &datum.size);
	rc = gnutls_pkcs7_import(*pkcs7, &datum, GNUTLS_X509_FMT_PEM);
	if (rc < 0) {
		fprintf(stderr, "Not a pkcs7 PEM encoded file %s\n", file);
		exit(EXIT_FAILURE);
	}
	free(datum.data);
}

int read_certificates(const char *file, gnutls_x509_crt_t *certs, int maxnr)
{
	int rc;
	gnutls_datum_t datum;
	unsigned count;

	read_file(file, &datum.data, &datum.size);
	count = (unsigned)maxnr;
	rc = gnutls_x509_crt_list_import(certs, &count, &datum, GNUTLS_X509_FMT_PEM, 0);
	if (rc < 0) {
		fprintf(stderr, "Can't import CA certificates PEM encoded file %s\n", file);
		exit(EXIT_FAILURE);
	}
	free(datum.data);
	return (int)count;
}

void print_granteds(const char *name, domain_permission_t perm, void *closure)
{
	if (perm == domain_permission_grants)
		printf("%s\n", name);
}

void print_domains(const domain_spec_t *spec)
{
	domain_spec_enum(spec, print_granteds, NULL);
}
		

int main(int ac, char **av)
{
	gnutls_x509_crt_t roots[10];
	gnutls_pkcs7_t pkcs7;
	domain_spec_t spec = DOMAIN_SPEC_INITIAL;
	int nca, cmd, rc;

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
	if (ac < 3)
		help(av[0], true);

	switch (cmd) {
	case ID_CMD_check:
		if (ac > 4)
			help(av[0], true);

		read_pkcs7(av[2], &pkcs7);
		if (ac < 4)
			nca = 0;
		else
			nca = read_certificates(av[3], roots, (int)(sizeof roots / sizeof roots[0]));
		rc = check_signature(pkcs7, &spec, roots, nca);
		if (rc < 0) {
			fprintf(stderr, "Signature file %s can't be used\n", av[2]);
			exit(EXIT_FAILURE);
		}
		print_domains(&spec);
		break;
	}
	exit(EXIT_SUCCESS);
	return 0;
}

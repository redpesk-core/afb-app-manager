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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/crypto.h>

#include "digest.h"

enum {
	ID_CMD_help,
	ID_CMD_make,
	ID_CMD_check
};

const char *commands[] = {
	[ID_CMD_help] = "help",
	[ID_CMD_make] = "make",
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
	printf("  make [ALGO] FILELIST\n");
	printf("      \n");
	printf("\n");
	printf("  check FILELIST\n");
	printf("      \n");
	printf("\n");
	printf("  help\n");
	printf("      This help\n");
	printf("\n");
	printf("ALGO is one of --sha224 --sha256 --sha384 --sha512\n");
	printf("\n");
	printf("FILELIST is the name of a file listing the content to digest\n");
	printf("\n");
	exit(EXIT_SUCCESS);
}

void read_file(FILE *file, const char *filename, char **pointer, size_t *size)
{
	size_t sread, sz, len = 0;
	char *ptr, *buffer = NULL;

	if (file == NULL)
		file = fopen(filename, "rb");
	if (file != NULL) {
		for (;;) {
			sz = len + 32768;
			ptr = realloc(buffer, sz);
			if (ptr == NULL)
				break;
			buffer = ptr;
			sread = fread(&buffer[len], 1, sz - len, file);
			if (sread == 0) {
				ptr = realloc(buffer, len);
				if (ptr == NULL)
					break;
				*pointer = ptr;
				*size = len;
				fclose(file);
				return;
			}
			len += (size_t)sread;
		}
		fclose(file);
	}
	fprintf(stderr, "Can't read file %s\n", filename);
	exit(EXIT_FAILURE);
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

int main(int ac, char **av)
{
	int cmd, rc, ai;
	path_entry_t *root;
	gnutls_digest_algorithm_t algorithm;
	char *digest;
	size_t length;

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
	algorithm = GNUTLS_DIG_SHA256;
	ai = 2;
	if (cmd == ID_CMD_make) {
		for ( ; ai < ac && av[ai][0] == '-' ; ai++ ) {
			if (0 == strcmp(av[ai], "--sha224"))
				algorithm = GNUTLS_DIG_SHA224;
			else if (0 == strcmp(av[ai], "--sha256"))
				algorithm = GNUTLS_DIG_SHA256;
			else if (0 == strcmp(av[ai], "--sha384"))
				algorithm = GNUTLS_DIG_SHA384;
			else if (0 == strcmp(av[ai], "--sha512"))
				algorithm = GNUTLS_DIG_SHA512;
			else
				help(av[0], true);
		}
	}

	/* check at least one argument */
	if (ai == ac)
		help(av[0], true);

	read_file_list(av[ai], &root);
	switch (cmd) {
	case ID_CMD_make:
		rc = create_author_digest(root, algorithm, NULL, 0);
		if (rc < 0) {
			fprintf(stderr, "can't compute digest\n");
			exit(EXIT_FAILURE);
		}
		length = (unsigned)rc;
		digest = malloc((unsigned)rc + 1);
		if (digest == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		create_author_digest(root, algorithm, digest, length + 1);
		printf("%s", digest);
		break;

	case ID_CMD_check:
		read_file(stdin, "<STDIN>", &digest, &length);
		rc = check_author_digest(root, digest, length);
		if (rc < 0) {
			fprintf(stderr, "bad digest\n");
			exit(EXIT_FAILURE);
		}
		break;
	}
	exit(EXIT_SUCCESS);
	return 0;
}

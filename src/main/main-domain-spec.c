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
#include <string.h>

#include "domain-spec.h"

enum {
	ID_CMD_help,
	ID_CMD_granted,
	ID_CMD_check_grant,
	ID_CMD_check_cert
};

const char *commands[] = {
	[ID_CMD_help] = "help",
	[ID_CMD_granted] = "granted",
	[ID_CMD_check_grant] = "check-grant",
	[ID_CMD_check_cert] = "check-cert"
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
	printf("  granted SPEC...\n");
	printf("      Display domains granted by any SPEC..., one by line\n");
	printf("\n");
	printf("  check-grant SPEC DOM...\n");
	printf("      Exit with success if the domains DOM... are all granted by SPEC\n");
	printf("\n");
	printf("  check-cert SPEC OTHER...\n");
	printf("      Exit with success if SPEC allows to create certificates with all OTHER specs\n");
	printf("\n");
	printf("  help\n");
	printf("      This help\n");
	printf("\n");
	exit(EXIT_SUCCESS);
}

void get_spec(const char *str, domain_spec_t *spec)
{
	int rc = get_domain_spec_of_string(str, spec);
	if (rc < 0) {
		fprintf(stderr, "error, invalid SPEC: %s\n", str);
		exit(EXIT_FAILURE);
	}
}

void print_granteds(const char *name, domain_permission_t perm, void *closure)
{
	if (perm == domain_permission_grants)
		printf("%s\n", name);
}

#define	EXIT_NO 2
#define	EXIT_YES 0

int main(int ac, char **av)
{
	domain_spec_t spec, other;
	int it;
	unsigned cmd;

	if (ac < 2)
		help(av[0], true);

	cmd = (unsigned)(sizeof commands / sizeof *commands);
	while (strcmp(av[1], commands[--cmd]))
		if (!cmd)
			help(av[0], true);

	if (cmd == ID_CMD_help)
		help(av[0], false);

	if (ac < 3)
		help(av[0], true);

	it = 2;
	spec = DOMAIN_SPEC_INITIAL;
	other = DOMAIN_SPEC_INITIAL;
	switch (cmd) {
	case ID_CMD_granted:
		while (it < ac) {
			get_spec(av[it++], &other);
			domain_spec_add_grantings(&spec, &other);
			domain_spec_reset(&other);
		}
		domain_spec_enum(&spec, print_granteds, NULL);
		break;
	case ID_CMD_check_grant:
		get_spec(av[it++], &spec);
		while (it < ac)
			if (!is_domain_spec_granting(&spec, av[it++]))
				exit(EXIT_NO);
		break;
	case ID_CMD_check_cert:
		spec = DOMAIN_SPEC_INITIAL;
		get_spec(av[it++], &spec);
		while (it < ac) {
			get_spec(av[it++], &other);
			if (!is_domain_spec_able_to_sign(&spec, &other))
				exit(EXIT_NO);
			domain_spec_reset(&other);
		}
		break;
	}
	domain_spec_reset(&spec);
	exit(EXIT_SUCCESS);
	return 0;
}

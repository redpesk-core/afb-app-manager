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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libxml/tree.h>

#include <rp-utils/rp-verbose.h>
#include "wgtpkg-files.h"
#include "wgtpkg-workdir.h"
#include "wgtpkg-digsig.h"
#include "wgtpkg-xmlsec.h"

#if !defined(MAXCERT)
#define MAXCERT 20
#endif
#if !defined(DEFAULT_KEY_FILE)
#define DEFAULT_KEY_FILE "key.pem"
#endif
#if !defined(DEFAULT_CERT_FILE)
#define DEFAULT_CERT_FILE "cert.pem"
#endif

const char appname[] = "wgtpkg-sign";

static unsigned int get_number(const char *value)
{
	char *end;
	unsigned long int val;

	val = strtoul(value, &end, 10);
	if (*end || 0 == val || val >= UINT_MAX || *value == '-') {
		RP_ERROR("bad number value %s", value);
		exit(1);
	}
	return (unsigned int)val;
}

static void make_realpath(char **x)
{
	char *p = realpath(*x, NULL);
	if (p == NULL) {
		RP_ERROR("realpath failed for %s", *x);
		exit(1);
	}
	*x = p;
}

static void version()
{
	printf(
		"\n"
		"  %s  version="AFM_VERSION"\n"
		"\n"
		"  Copyright (C) 2015-2023 IoT.bzh Company\n"
		"  AFB comes with ABSOLUTELY NO WARRANTY.\n"
		"  Licence Apache 2\n"
		"\n",
		appname
	);
}

static void usage()
{
	printf(
		"usage: %s [-f] [-k keyfile] [-c certfile]... [-d number | -a] directory\n"
		"\n"
		"   -k keyfile       the private key to use for author signing\n"
		"   -c certfile      the certificate(s) to use for author signing\n"
		"   -d number        the number of the distributor signature (zero for automatic)\n"
		"   -a               the author signature\n"
		"   -f               force overwriting\n"
		"   -q               quiet\n"
		"   -v               verbose\n"
		"   -V               version\n"
		"\n",
		appname
	);
}

static struct option options_l[] = {
	{ "author",      no_argument,       NULL, 'a' },
	{ "certificate", required_argument, NULL, 'c' },
	{ "distributor", required_argument, NULL, 'd' },
	{ "force",       no_argument,       NULL, 'f' },
	{ "help",        no_argument,       NULL, 'h' },
	{ "key",         required_argument, NULL, 'k' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "version",     no_argument,       NULL, 'V' },
	{ NULL, 0, NULL, 0 }
};

static const char options_s[] = "ac:d:fhk:qvV";

/* install the widgets of the list */
int main(int ac, char **av)
{
	int i, force, ncert, author;
	unsigned int number;
	char *keyfile, *certfiles[MAXCERT+1], *directory;
	struct stat s;

	force = ncert = author = 0;
	number = UINT_MAX;
	keyfile = directory = NULL;
	for (;;) {
		i = getopt_long(ac, av, options_s, options_l, NULL);
		if (i < 0)
			break;
		switch (i) {
		case 'c':
			if (ncert == MAXCERT) {
				RP_ERROR("maximum count of certificates reached");
				return 1;
			}
			certfiles[ncert++] = optarg;
			break;
		case 'k':
			if (keyfile) {
				RP_ERROR("key already set");
				return 1;
			}
			keyfile = optarg;
			break;
		case 'd':
			if (number != UINT_MAX) {
				RP_ERROR("number already set");
				return 1;
			}
			number = get_number(optarg);
			break;
		case 'f':
			force = 1;
			break;
		case 'a':
			author = 1;
			break;
		case 'h':
			usage();
			return 0;
		case 'V':
			version();
			return 0;
		case 'q':
			rp_verbose_dec();
			break;
		case 'v':
			rp_verbose_inc();
			break;
		case ':':
			RP_ERROR("missing argument");
			return 1;
		default:
			RP_ERROR("unrecognized option");
			return 1;
		}
	}

	/* remaining arguments and final checks */
	if (optind >= ac) {
		RP_ERROR("no directory set");
		return 1;
	}
	directory = av[optind++];
	if (optind < ac) {
		RP_ERROR("extra parameters found");
		return 1;
	}

	/* set default values */
	if (keyfile == NULL)
		keyfile = DEFAULT_KEY_FILE;
	if (ncert == 0)
		certfiles[ncert++] = DEFAULT_CERT_FILE;

	/* check values */
	if (stat(directory, &s)) {
		RP_ERROR("can't find directory %s", directory);
		return 1;
	}
	if (!S_ISDIR(s.st_mode)) {
		RP_ERROR("%s isn't a directory", directory);
		return 1;
	}
	if (access(keyfile, R_OK) != 0) {
		RP_ERROR("can't access private key %s", keyfile);
		return 1;
	}
	for(i = 0 ; i < ncert ; i++)
		if (access(certfiles[i], R_OK) != 0) {
			RP_ERROR("can't access certificate %s", certfiles[i]);
			return 1;
		}

	/* init xmlsec module */
	if (xmlsec_init())
		return 1;

	/* compute absolutes paths */
	make_realpath(&keyfile);
	for(i = 0 ; i < ncert ; i++)
		make_realpath(&certfiles[i]);

	/* set and enter the workdir */
	if (set_workdir(directory, 0))
		return 1;

	if (fill_files())
		return 1;

	if (author)
		number = 0;
	else if (number == UINT_MAX)
		for (number = 1; get_signature(number) != NULL ; number++);

	if (!force && get_signature(number) != NULL) {
		RP_ERROR("can't overwrite existing signature %s", get_signature(number)->name);
		return 1;
	}

	RP_NOTICE("-- SIGNING content of directory %s for number %u", directory, number);

	certfiles[ncert] = NULL;
	return !!create_digsig(number, keyfile, (const char**)certfiles);
}


/*
 * Copyright (C) 2018-2023 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <rp-utils/rp-verbose.h>

#include "afmpkg-request.h"

static const char stripchars[] = " \t\n\r";
static const char commentchar = '#';

static int check(const char *file)
{
	int rc;
	afmpkg_request_t request; /* in stack request */
	FILE *f;
	char buffer[1024];
	size_t base, length;

	/* init the request */
	rc = afmpkg_request_init(&request);

	/* receive the request */
	if (rc >= 0) {
		f = fopen(file, "r");
		if (f == NULL) {
			rc = -errno;
			RP_ERROR("failed to open %s", file);
		}
		else {
			while(fgets(buffer, sizeof buffer, f) != NULL) {
				length = strnlen(buffer, sizeof buffer - 1);
				base = 0;
				while (base < length && strchr(stripchars, buffer[base]) != NULL)
					base++;
				if (base < length) {
					if (buffer[base] == commentchar)
						length = base;
					else
						while (length && strchr(stripchars, buffer[length - 1]) != NULL)
							length--;
				}
				if (base < length) {
					buffer[length] = 0;
					rc = afmpkg_request_add_line(&request, &buffer[base], length - base);
					if (rc < 0)
						break;
				}
			}
			fclose(f);
		}
	}

	/* process the request */
	if (rc >= 0)
		rc = afmpkg_request_process(&request);

	/* reply to the request */
	printf ("rc=%d reply=%s\n", rc, request.reply ?: "");

	/* reset the memory */
	afmpkg_request_deinit(&request);
	return rc;
}



static const char appname[] = "afm-check-pkg";

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
		"usage: %s [options...] ARG\n"
		"options:\n"
		"   -h, --help        help\n"
		"   -q, --quiet       quiet\n"
		"   -v, --verbose     verbose\n"
		"   -V, --version     version\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "help",        no_argument,       NULL, 'h' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "version",     no_argument,       NULL, 'V' },
	{ NULL, 0, NULL, 0 }
};

/* check transations of the list */
int main(int ac, char **av)
{
	for (;;) {
		int i = getopt_long(ac, av, "fhqsvV", options, NULL);
		if (i < 0)
			break;
		switch (i) {
		case 'h':
			usage();
			return 0;
		case 'q':
			rp_verbose_dec();
			break;
		case 'v':
			rp_verbose_inc();
			break;
		case 'V':
			version();
			return 0;
		default:
			RP_ERROR("unrecognized option");
			return 1;
		}
	}
	while (optind < ac) {
		printf("checking %s\n", av[optind]);
		check(av[optind]);
		optind++;
	}
	return 0;
}

/*
 * Copyright (C) 2018-2025 IoT.bzh Company
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>

#include <rp-utils/rp-verbose.h>

#include "afmpkg.h"
#include "unit-oper.h"

static int begin(void *closure, const char *appid, afmpkg_mode_t mode)
{
	if (mode != Afmpkg_Install) {
		RP_ERROR("unexpected mode");
		return -1;
	}
	fprintf(stdout, "clear\n");
	if (appid != NULL)
		fprintf(stdout, "id %s\n", appid);
	return 0;
}

static int tagfile(void *closure, const char *path, path_type_t type)
{
	const char *slmt = path_type_for_slm(type);
	if (slmt == NULL) {
		RP_ERROR("unexpected type");
		return -1;
	}
	fprintf(stdout, "path %s %s\n", path, slmt);
	return 0;
}

static int setperm(void *closure, const char *perm)
{
	fprintf(stdout, "permission %s\n", perm);
	return 0;
}

static int setplug(
	void *closure,
	const char *exportdir,
	const char *importid,
	const char *importdir
)
{
	fprintf(stdout, "plug %s %s %s\n", exportdir, importid, importdir);
	return 0;
}

static int setunits(void *closure, const struct unitdesc *units, int nrunits)
{
	int i, rc;

	/* check that no file is overwritten by the installation */
	for (i = 0 ; i < nrunits ; i++) {
		rc = unit_oper_check_files(&units[i], 0);
		if (rc < 0) {
			RP_ERROR("invalid unit");
			return rc;
		}
	}

	/* install the units */
	for (i = 0 ; i < nrunits ; i++) {
		rc = unit_oper_install(&units[i]);
		if (rc < 0) {
			RP_ERROR("install error");
			return rc;
		}
	}
	return 0;
}

static int end(void * closure, int status)
{
	fprintf(stdout, "install\n");
	return 0;
}

static const afmpkg_operations_t offlineops =
	{
		.begin = begin,
		.tagfile = tagfile,
		.setperm = setperm,
		.setplug = setplug,
		.setunits = setunits,
		.end = end
	};

static int run()
{
	int rc;
	afmpkg_t apkg;
	char buffer[PATH_MAX + 1];
	size_t length;

	/* init */
	apkg.package = NULL;
	apkg.root = NULL;
	apkg.redpakid = NULL;
	rc = path_entry_create_root(&apkg.files);
	if (rc < 0) {
		RP_ERROR("Init failed");
		return EXIT_FAILURE;
	}

	/* read input */
	while (fgets(buffer, sizeof buffer, stdin) != NULL) {
		length = strlen(buffer);
		if (length) {
			if (buffer[length - 1] == '\n')
				buffer[--length] = 0;
			rc = path_entry_add_length(apkg.files, NULL, buffer, length);
			if (rc < 0) {
				RP_ERROR("Init failed");
				return EXIT_FAILURE;
			}
		}
	}

	/* process now */
	rc = afmpkg_install(&apkg, &offlineops, NULL);
	if (rc < 0) {
		RP_ERROR("installation failed");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static const char appname[] = "afmpkg-offline";

static void version()
{
	printf(
		"\n"
		"  %s  version="AFM_VERSION"\n"
		"\n"
		"  Copyright (C) 2015-2025 IoT.bzh Company\n"
		"  AFB comes with ABSOLUTELY NO WARRANTY.\n"
		"  Licence Apache 2\n"
		"\n",
		appname
	);
}

static void usage()
{
	printf(
		"usage: %s [options...]\n"
		"options:\n"
		"   -h, --help        help\n"
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

/* install the widgets of the list */
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
	return run();
}

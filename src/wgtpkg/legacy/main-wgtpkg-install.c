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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>

#include <libxml/tree.h>

#include <rp-utils/rp-verbose.h>
#include "wgtpkg-permissions.h"
#include "wgtpkg-xmlsec.h"
#include "wgt-info.h"
#include "wgtpkg-install.h"

static const char appname[] = "wgtpkg-install";
static const char *root;
static int force;

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
		"usage: %s [-f] [-q] [-v] [-p list] rootdir wgtfile...\n"
		"\n"
		"   rootdir       the root directory for installing\n"
		"   -p list       a list of comma separated permissions to allow\n"
		"   -f            force overwriting\n"
		"   -q            quiet\n"
		"   -v            verbose\n"
		"   -V            version\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "permissions", required_argument, NULL, 'p' },
	{ "force",       no_argument,       NULL, 'f' },
	{ "help",        no_argument,       NULL, 'h' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "version",     no_argument,       NULL, 'V' },
	{ NULL, 0, NULL, 0 }
};

/* install the widgets of the list */
int main(int ac, char **av)
{
	int i, rc;
	struct wgt_info *ifo;

	xmlsec_init();

	force = 0;
	for (;;) {
		i = getopt_long(ac, av, "hfqvVp:", options, NULL);
		if (i < 0)
			break;
		switch (i) {
		case 'f':
			force = 1;
			break;
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
		case 'p':
			rc = grant_permission_list(optarg);
			if (rc < 0) {
				RP_ERROR("Can't set granted permission list");
				exit(1);
			}
			break;
		case ':':
			RP_ERROR("missing argument value");
			return 1;
		default:
			RP_ERROR("unrecognized option");
			return 1;
		}
	}

	ac -= optind;
	if (ac < 2) {
		RP_ERROR("arguments are missing");
		return 1;
	}

	/* install widgets */
	av += optind;
	root = *av++;
	for ( ; *av ; av++) {
		ifo = install_widget(*av, root, force);
		if (ifo)
			wgt_info_unref(ifo);
	}

	return 0;
}


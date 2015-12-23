/*
 Copyright 2015 IoT.bzh

 author: José Bollo <jose.bollo@iot.bzh>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
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

#include "verbose.h"
#include "wgtpkg.h"
#include "wgt-info.h"
#include "wgtpkg-install.h"

static const char appname[] = "wgtpkg-installer";
static const char *root;
static int force;

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
	{ NULL, 0, NULL, 0 }
};

/* install the widgets of the list */
int main(int ac, char **av)
{
	int i;
	struct wgt_info *ifo;

	LOGAUTH(appname);

	xmlsec_init();

	force = 0;
	for (;;) {
		i = getopt_long(ac, av, "hfqvp:", options, NULL);
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
			if (verbosity)
				verbosity--;
			break;
		case 'v':
			verbosity++;
			break;
		case 'p':
			grant_permission_list(optarg);
			break;
		case ':':
			ERROR("missing argument value");
			return 1;
		default:
			ERROR("unrecognized option");
			return 1;
		}
	}

	ac -= optind;
	if (ac < 2) {
		ERROR("arguments are missing");
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


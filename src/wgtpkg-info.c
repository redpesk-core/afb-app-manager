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

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>

#include <libxml/tree.h>

#include "verbose.h"
#include "wgtpkg-workdir.h"
#include "wgtpkg-files.h"
#include "wgtpkg-zip.h"
#include "wgtpkg-digsig.h"
#include "wgtpkg-xmlsec.h"
#include "wgt.h"
#include "wgt-info.h"

static const char appname[] = "wgtpkg-info";

static void show(const char *wgtfile);

static void usage()
{
	printf(
		"usage: %s [-f] [-q] [-v] wgtfile...\n"
		"\n"
		"   -q            quiet\n"
		"   -v            verbose\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "help",        no_argument,       NULL, 'h' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ NULL, 0, NULL, 0 }
};

/* info the widgets of the list */
int main(int ac, char **av)
{
	int i;
	char *wpath;

	LOGUSER(appname);

	xmlsec_init();

	for (;;) {
		i = getopt_long(ac, av, "hqv", options, NULL);
		if (i < 0)
			break;
		switch (i) {
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
		case ':':
			ERROR("missing argument value");
			return 1;
		default:
			ERROR("unrecognized option");
			return 1;
		}
	}

	/* canonic names for files */
	av += optind;
	for (i = 0 ; av[i] != NULL ; i++) {
		wpath = realpath(av[i], NULL);
		if (wpath == NULL) {
			ERROR("error while getting realpath of %dth widget: %s", i+1, av[i]);
			return 1;
		}
		av[i] = wpath;
	}

	/* info widgets */
	for ( ; *av ; av++)
		show(*av);

	return 0;
}

static int check_and_show()
{
	struct wgt_info *ifo;

	ifo = wgt_info_createat(workdirfd, NULL, 1, 1, 1);
	if (!ifo)
		return -1;
	wgt_info_dump(ifo, 1, "");
	wgt_info_unref(ifo);
	return 0;
}

/* install the widget of the file */
static void show(const char *wgtfile)
{
	NOTICE("-- INFO for widget %s --", wgtfile);

	/* workdir */
	if (make_workdir_base("/tmp", "UNPACK", 0)) {
		ERROR("failed to create a working directory");
		return;
	}

	if (zread(wgtfile, 0))
		goto error2;

	if (check_all_signatures())
		goto error2;

	check_and_show();
	
error2:
	remove_workdir();
	return;
}


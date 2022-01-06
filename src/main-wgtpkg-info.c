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

static void version()
{
	printf(
		"\n"
		"  %s  version="AFM_VERSION"\n"
		"\n"
		"  Copyright (C) 2015-2022 IoT.bzh Company\n"
		"  AFB comes with ABSOLUTELY NO WARRANTY.\n"
		"  Licence Apache 2\n"
		"\n",
		appname
	);
}

static void usage()
{
	printf(
		"usage: %s [-q] [-v] wgtfile...\n"
		"\n"
		"   -q            quiet\n"
		"   -v            verbose\n"
		"   -V            version\n"
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

/* info the widgets of the list */
int main(int ac, char **av)
{
	int i;
	char *wpath;

	LOGUSER(appname);

	xmlsec_init();

	for (;;) {
		i = getopt_long(ac, av, "hqvV", options, NULL);
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
		case 'V':
			version();
			return 0;
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
	if (make_workdir("/tmp", "UNPACK", 0)) {
		ERROR("failed to create a working directory");
		return;
	}

	if (zread(wgtfile, 0))
		goto error2;

	if (check_all_signatures(1)) /* info even on WGT without signature */
		goto error2;

	check_and_show();

error2:
	remove_workdir();
	return;
}


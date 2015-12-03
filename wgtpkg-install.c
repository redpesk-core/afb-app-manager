/*
 Copyright 2015 IoT.bzh

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

#define _BSD_SOURCE /* see readdir */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>

#include "wgtpkg.h"

/* install the widget of the file */
static void install(const char *wgtfile)
{
	notice("-- INSTALLING widget %s", wgtfile);

	if (enter_workdir(1))
		goto error;

	if (zread(wgtfile, 0))
		goto error;

	if (check_all_signatures())
		goto error;

	return;

error:
	return;
	exit(1);
}

/* install the widgets of the list */
int main(int ac, char **av)
{
	int i, kwd;

	openlog("wgtpkg-install", LOG_PERROR, LOG_AUTH);

	xmlsec_init();

	ac = verbose_scan_args(ac, av);
	
	/* canonic names for files */
	for (i = 1 ; av[i] != NULL ; i++)
		if ((av[i] = realpath(av[i], NULL)) == NULL) {
			syslog(LOG_ERR, "error while getting realpath of %dth argument", i);
			return 1;
		}

	/* workdir */
	kwd = 1;
	if (make_workdir(kwd)) {
		syslog(LOG_ERR, "failed to create a working directory");
		return 1;
	}
	if (!kwd)
		atexit(remove_workdir);

	/* install widgets */
	for (av++ ; *av ; av++)
		install(*av);

	exit(0);
	return 0;
}


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
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <getopt.h>

#include "verbose.h"
#include "wgtpkg.h"
#include "wgt.h"
#include "wgt-info.h"

static const char appname[] = "wgtpkg-install";
static const char *root;
static char **permissions = NULL;
static int force;

static void install(const char *wgtfile);
static void add_permissions(const char *list);

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
	char *wpath;

	openlog(appname, LOG_PERROR, LOG_AUTH);

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
			add_permissions(optarg);
			break;
		case ':':
			syslog(LOG_ERR, "missing argument value");
			return 1;
		default:
			syslog(LOG_ERR, "unrecognized option");
			return 1;
		}
	}

	ac -= optind;
	if (ac < 2) {
		syslog(LOG_ERR, "arguments are missing");
		return 1;
	}

	/* canonic names for files */
	av += optind;
	for (i = 0 ; av[i] != NULL ; i++) {
		wpath = realpath(av[i], NULL);
		if (wpath == NULL) {
			syslog(LOG_ERR, "error while getting realpath of %dth widget: %s", i+1, av[i]);
			return 1;
		}
		av[i] = wpath;
	}
	root = *av++;

	/* install widgets */
	for ( ; *av ; av++)
		install(*av);

	return 0;
}

/* checks if the permission 'name' is granted */
static int has_permission(const char *name)
{
	char **p = permissions;
	if (p) {
		while(*p) {
			if (0 == strcmp(*p, name))
				return 1;
			p++;
		}
	}
	return 0;
}

/* add permissions granted for installation */
static void add_permissions(const char *list)
{
	char **ps, *p;
	const char *iter;
	int n, on;
	static const char separators[] = " \t\n\r,";

	n = 0;
	iter = list + strspn(list, separators);
	while(*iter) {
		n++;
		iter += strcspn(iter, separators);
		iter += strspn(iter, separators);
	}
	if (n == 0)
		return;

	on = 0;
	ps = permissions;
	if (ps)
		while(*ps++)
			on++;

	ps = realloc(permissions, (1 + on + n) * sizeof * ps);
	if (!ps) {
		syslog(LOG_ERR, "Can't allocate memory for permissions");
		exit(1);
	}

	permissions = ps;
	ps[on] = NULL;

	iter = list + strspn(list, separators);
	while(*iter) {
		n = strcspn(iter, separators);
		p = strndup(iter, n);
		if (!p) {
			syslog(LOG_ERR, "Can't allocate permission");
			exit(1);
		}
		if (has_permission(p))
			free(p);
		else {
			ps[on] = p;
			ps[++on] = NULL;
		}
		iter += n;
		iter += strspn(iter, separators);
	}
}


static struct wgt *wgt_at_workdir()
{
	int rc, wfd;
	struct wgt *wgt;

	wfd = workdirfd();
	if (wfd < 0)
		return NULL;

	wgt = wgt_create();
	if (!wgt) {
		syslog(LOG_ERR, "failed to allocate wgt");
		close(wfd);
		return NULL;
	}

	rc = wgt_connectat(wgt, wfd, NULL);
	if (rc) {
		syslog(LOG_ERR, "failed to connect wgt to workdir");
		close(wfd);
		wgt_unref(wgt);
		return NULL;
	}

	return wgt;
}


static int check_and_place()
{
	struct wgt *wgt;
	struct wgt_info *ifo;

	wgt = wgt_at_workdir();
	if (!wgt)
		return -1;

	ifo = wgt_info_get(wgt, 1, 1, 1);
	if (!ifo) {
		wgt_unref(wgt);
		return -1;
	}
	wgt_info_dump(ifo, 1, "");
	wgt_info_unref(ifo);
	wgt_unref(wgt);
	return 0;
}

/* install the widget of the file */
static void install(const char *wgtfile)
{
	notice("-- INSTALLING widget %s --", wgtfile);

	/* workdir */
	if (make_workdir_base(root, "UNPACK", 0)) {
		syslog(LOG_ERR, "failed to create a working directory");
		goto error1;
	}

	if (enter_workdir(0))
		goto error2;

	if (zread(wgtfile, 0))
		goto error2;

	if (check_all_signatures())
		goto error2;

	if (check_and_place())
		goto error2;
	
	return;

error2:
	remove_workdir();

error1:
	return;
}



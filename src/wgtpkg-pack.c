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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "verbose.h"
#include "wgtpkg.h"

#if !defined(MAXCERT)
#define MAXCERT 20
#endif
#if !defined(DEFAULT_KEY_FILE)
#define DEFAULT_KEY_FILE "key.pem"
#endif
#if !defined(DEFAULT_CERT_FILE)
#define DEFAULT_CERT_FILE "cert.pem"
#endif

const char appname[] = "wgtpkg-pack";

static void usage()
{
	printf(
		"usage: %s [-f] [-o wgtfile] directory\n"
		"\n"
		"   -o wgtfile       the output widget file\n"
		"   -f               force overwriting\n"
		"   -q               quiet\n"
		"   -v               verbose\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "output",      required_argument, NULL, 'o' },
	{ "force",       no_argument,       NULL, 'f' },
	{ "help",        no_argument,       NULL, 'h' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ NULL, 0, NULL, 0 }
};

/* install the widgets of the list */
int main(int ac, char **av)
{
	int i, force;
	char *wgtfile, *directory, *x;
	struct stat s;

	LOGUSER(appname);

	force = 0;
	wgtfile = directory = NULL;
	for (;;) {
		i = getopt_long(ac, av, "qvhfo:", options, NULL);
		if (i < 0)
			break;
		switch (i) {
		case 'o':
			wgtfile = optarg;
			break;
		case 'q':
			if (verbosity)
				verbosity--;
			break;
		case 'v':
			verbosity++;
			break;
		case 'f':
			force = 1;
			break;
		case 'h':
			usage();
			return 0;
		case ':':
			ERROR("missing argument");
			return 1;
		default:
			ERROR("unrecognized option");
			return 1;
		}
	}

	/* remaining arguments and final checks */
	if (optind >= ac) {
		ERROR("no directory set");
		return 1;
	}
	directory = av[optind++];
	if (optind < ac) {
		ERROR("extra parameters found");
		return 1;
	}

	/* set default values */
	if (wgtfile == NULL && 0 > asprintf(&wgtfile, "%s.wgt", directory)) {
		ERROR("asprintf failed");
		return 1;
	}

	/* check values */
	if (stat(directory, &s)) {
		ERROR("can't find directory %s", directory);
		return 1;
	}
	if (!S_ISDIR(s.st_mode)) {
		ERROR("%s isn't a directory", directory);
		return 1;
	}
	if (access(wgtfile, F_OK) == 0 && force == 0) {
		ERROR("can't overwrite existing %s", wgtfile);
		return 1;
	}

	NOTICE("-- PACKING widget %s from directory %s", wgtfile, directory);

	/* creates an existing widget (for realpath it must exist) */
	i = open(wgtfile, O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK, 0644);
	if (i < 0) {
		ERROR("can't write widget %s", wgtfile);
		return 1;
	}
	close(i);

	/* compute absolutes paths */
	x = realpath(wgtfile, NULL);
	if (x == NULL) {
		ERROR("realpath failed for %s",wgtfile);
		return 1;
	}
	wgtfile = x;

	/* set and enter the workdir */
	if (set_workdir(directory, 0))
		return 1;


	if (fill_files())
		return 1;

	return !!zwrite(wgtfile);
}



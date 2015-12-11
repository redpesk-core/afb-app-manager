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
#include <syslog.h>
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

const char appname[] = "wgtpkg-sign";

static unsigned int get_number(const char *value)
{
	char *end;
	unsigned long int val;

	val = strtoul(value, &end, 10);
	if (*end || 0 == val || val >= UINT_MAX || *value == '-') {
		syslog(LOG_ERR, "bad number value %s", value);
		exit(1);
	}
	return (unsigned int)val;
}

static void usage()
{
	printf(
		"usage: %s [-f] [-k keyfile] [-c certfile]... [-o wgtfile] [-d number | -a] directory\n"
		"\n"
		"   -k keyfile       the private key to use for author signing\n"
		"   -c certfile      the certificate(s) to use for author signing\n"
		"   -d number        the number of the distributor signature (zero for automatic)\n"
		"   -a               the author signature\n"
		"   -f               force overwriting\n"
		"   -q               quiet\n"
		"   -v               verbose\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "key",         required_argument, NULL, 'k' },
	{ "certificate", required_argument, NULL, 'c' },
	{ "distributor", required_argument, NULL, 'd' },
	{ "author",      no_argument,       NULL, 'a' },
	{ "force",       no_argument,       NULL, 'f' },
	{ "help",        no_argument,       NULL, 'h' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ NULL, 0, NULL, 0 }
};

/* install the widgets of the list */
int main(int ac, char **av)
{
	int i, force, ncert, author;
	unsigned int number;
	char *keyfile, *certfiles[MAXCERT+1], *directory, **x;
	struct stat s;

	openlog(appname, LOG_PERROR, LOG_USER);

	force = ncert = author = 0;
	number = UINT_MAX;
	keyfile = directory = NULL;
	for (;;) {
		i = getopt_long(ac, av, "hfqvak:c:d:", options, NULL);
		if (i < 0)
			break;
		switch (i) {
		case 'c':
			if (ncert == MAXCERT) {
				syslog(LOG_ERR, "maximum count of certificates reached");
				return 1;
			}
			certfiles[ncert++] = optarg;
			continue;
		case 'k': x = &keyfile; break;
		case 'd': number = get_number(optarg); continue;
		case 'f': force = 1; continue;
		case 'a': author = 1; continue;
		case 'h': usage(); return 0;
		case 'q':
			if (verbosity)
				verbosity--;
			break;
		case 'v':
			verbosity++;
			break;
		case ':':
			syslog(LOG_ERR, "missing argument");
			return 1;
		default:
			syslog(LOG_ERR, "unrecognized option");
			return 1;
		}
		if (*x != NULL) {
			syslog(LOG_ERR, "option set twice");
			return 1;
		}
		*x = optarg;
	}

	/* remaining arguments and final checks */
	if (optind >= ac) {
		syslog(LOG_ERR, "no directory set");
		return 1;
	}
	directory = av[optind++];
	if (optind < ac) {
		syslog(LOG_ERR, "extra parameters found");
		return 1;
	}

	/* set default values */
	if (keyfile == NULL)
		keyfile = DEFAULT_KEY_FILE;
	if (ncert == 0)
		certfiles[ncert++] = DEFAULT_CERT_FILE;

	/* check values */
	if (stat(directory, &s)) {
		syslog(LOG_ERR, "can't find directory %s", directory);
		return 1;
	}
	if (!S_ISDIR(s.st_mode)) {
		syslog(LOG_ERR, "%s isn't a directory", directory);
		return 1;
	}
	if (access(keyfile, R_OK) != 0) {
		syslog(LOG_ERR, "can't access private key %s", keyfile);
		return 1;
	}
	for(i = 0 ; i < ncert ; i++) 
		if (access(certfiles[i], R_OK) != 0) {
			syslog(LOG_ERR, "can't access certificate %s", certfiles[i]);
			return 1;
		}

	/* init xmlsec module */
	if (xmlsec_init())
		return 1;


	/* compute absolutes paths */
#define rp(x) do { char *p = realpath(x, NULL); if (p != NULL) x = p; else { syslog(LOG_ERR, "realpath failed for %s",x); return 1; } } while(0)
	rp(keyfile);
	for(i = 0 ; i < ncert ; i++) 
		rp(certfiles[i]);
#undef rp

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
		syslog(LOG_ERR, "can't overwrite existing signature %s", get_signature(number)->name);
		return 1;
	}

	notice("-- SIGNING content of directory %s for number %u", directory, number);

	certfiles[ncert] = NULL;
	return !!create_digsig(number, keyfile, (const char**)certfiles);
}


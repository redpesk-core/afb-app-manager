/*
 Copyright 2016, 2017 IoT.bzh

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
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "verbose.h"
#include "utils-file.h"

#include "wgtpkg-mustach.h"
#include "wgtpkg-unit.h"

static char *template;

/*
 * Pack a null terminated 'text' by removing empty lines,
 * lines made of blanks and terminated with \, lines
 * starting with the 'purge' character (can be null).
 * Lines made of the 'purge' character followed with
 * "nl" exactly (without quotes ") are replaced with
 * an empty line.
 *
 * Returns the pointer to the ending null.
 */
static char *pack(char *text, char purge)
{
	char *read, *write, *begin, *start, c, emit, cont, nextcont;

	cont = 0;
	c = *(begin = write = read = text);
	while (c) {
		emit = nextcont = 0;
		start = NULL;
		begin = read;
		while (c && c != '\n') {
			if (c != ' ' && c != '\t') {
				if (c == '\\' && read[1] == '\n')
					nextcont = 1;
				else {
					emit = 1;
					if (!start)
						start = read;
				}
			}
			c = *++read;
		}
		if (c)
			c = *++read;
		if (emit) {
			if (!cont && start)
				begin = start;
			if (purge && *begin == purge) {
				if (!strncmp(begin+1, "nl\n",3))
					*write++ = '\n';
			} else {
				while (begin != read)
					*write++ = *begin++;
			}
		}
		cont = nextcont;
	}
	*write = 0;
	return write;
}

/*
 * Searchs the first character of the next line
 * of the 'text' and returns its address
 * Returns NULL if there is no next line.
 */
static inline char *nextline(char *text)
{
	char *result = strchr(text, '\n');
	return result + !!result;
}

/*
 * Search in 'text' the offset of a line beginning with the 'pattern'
 * Returns NULL if not found or the address of the line contning the pattern
 * If args isn't NULL and the pattern is found, the pointed pattern is
 * updated with the address of the character following the found pattern.
 */
static char *offset(char *text, const char *pattern, char **args)
{
	size_t len;

	if (text) {
		len = strlen(pattern);
		do {
			if (strncmp(text, pattern, len))
				text = nextline(text);
			else {
				if (args)
					*args = &text[len];
				break;
			}
		} while (text);
	}
	return text;
}

/*
 * process one unit
 */

static int process_one_unit(char *spec, struct unitdesc *desc)
{
	char *nsoc, *nsrv;
	int isuser, issystem, issock, isserv;

	/* found the configuration directive of the unit */
	isuser = !!offset(spec, "%systemd-unit user\n", NULL);
	issystem = !!offset(spec, "%systemd-unit system\n", NULL);
	issock  = !!offset(spec, "%systemd-unit socket ", &nsoc);
	isserv  = !!offset(spec, "%systemd-unit service ", &nsrv);

	if (isuser ^ issystem) {
		desc->scope = isuser ? unitscope_user : unitscope_system;
	} else {
		desc->scope = unitscope_unknown;
	}

	if (issock ^ isserv) {
		if (issock) {
			desc->type = unittype_socket;
			desc->name = nsoc;
		} else {
			desc->type = unittype_service;
			desc->name = nsrv;
		}
		desc->name_length = (size_t)(strchrnul(desc->name, '\n') - desc->name);
		desc->name = strndup(desc->name, desc->name_length);
	} else {
		desc->type = unittype_unknown;
		desc->name = NULL;
		desc->name_length = 0;
	}

	desc->content = spec;
	desc->content_length = (size_t)(pack(spec, '%') - spec);

	return 0;
}

/*
 */
static int process_all_units(char *spec, int (*process)(void *closure, const struct unitdesc descs[], unsigned count), void *closure)
{
	int rc, rc2;
	unsigned n;
	char *beg, *end, *after;
	struct unitdesc *descs, *d;

	descs = NULL;
	n = 0;
	rc = 0;
	beg = offset(spec, "%begin systemd-unit\n", NULL);
	while(beg) {
		beg = nextline(beg);
		end = offset(beg, "%end systemd-unit\n", &after);
		if (!end) {
			/* unterminated unit !! */
			ERROR("unterminated unit description!! %s", beg);
			break;
		}
		*end = 0;

		d = realloc(descs, (n + 1) * sizeof *descs);
		if (d == NULL)
			rc2 = -ENOMEM;
		else {
			memset(&d[n], 0, sizeof *d);
			descs = d;
			rc2 = process_one_unit(beg, &descs[n]);
		}

		if (rc2 >= 0)
			n++;
		else if (rc == 0)
			rc = rc2;
		beg = offset(after, "%begin systemd-unit\n", NULL);
	}

	if (rc == 0 && process)
		rc = process(closure, descs, n);
	while(n)
		free((char *)(descs[--n].name));
	free(descs);

	return rc;
}

int unit_generator_on(const char *filename)
{
	size_t size;
	char *tmp;
	int rc;

	rc = getfile(filename ? : FWK_UNIT_CONF, &template, NULL);
	if (!rc) {
		size = (size_t)(pack(template, ';') - template);
		tmp = realloc(template, 1 + size);
		if (tmp)
			template = tmp;
	}
	return rc;
}

void unit_generator_off()
{
	free(template);
	template = NULL;
}

int unit_generator_process(struct json_object *jdesc, int (*process)(void *closure, const struct unitdesc descs[], unsigned count), void *closure)
{
	int rc;
	size_t size;
	char *instance;

	rc = template ? 0 : unit_generator_on(NULL);
	if (!rc) {
		instance = NULL;
		rc = apply_mustach(template, jdesc, &instance, &size);
		if (!rc) {
			rc = process_all_units(instance, process, closure);
		}
		free(instance);
	}
	return rc;
}


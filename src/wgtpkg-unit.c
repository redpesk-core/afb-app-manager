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

#if 0
#include <ctype.h>
#else
#define isblank(c) ((c)==' '||(c)=='\t')
#endif

/* the template for all units */
static char *template;

/*
 * Search for the 'pattern' in 'text'.
 * Returns 1 if 'text' matches the 'pattern' or else returns 0.
 * When returning 1 and 'after' isn't NULL, the pointer to the
 * first character after the pettern in 'text' is stored in 'after'.
 * The characters '\n' and ' ' have a special mening in the search:
 *  * '\n': matches any space or tabs (including none) followed 
 *          either by '\n' or '\0' (end of the string)
 *  * ' ': matches any space or tabs but at least one.
 */
static int matches(const char *pattern, char *text, char **after)
{
	char p, t;

	t = *text;
	p = *pattern;
	while(p) {
		switch(p) {
		case '\n':
			while (isblank(t))
				t = *++text;
			if (t) {
				if (t != p)
					return 0;
				t = *++text;
			}
			break;
		case ' ':
			if (!isblank(t))
				return 0;
			do {
				t = *++text;
			} while(isblank(t));
			break;
		default:
			if (t != p)
				return 0;
			t = *++text;
			break;
		}
		p = *++pattern;
	}
	if (after)
		*after = text;
	return 1;
}

/*
 * Pack a null terminated 'text' by removing empty lines,
 * lines made of blanks and terminated with \, lines
 * starting with the 'purge' character (can be null).
 * Lines made of the 'purge' character followed with
 * "nl" exactly (without quotes ") are replaced with
 * an empty line.
 *
 * Returns the size after packing (offset of the ending null).
 */
static size_t pack(char *text, char purge)
{
	char *read;    /* read iterator */
	char *write;   /* write iterator */
	char *begin;   /* begin the copied text of the line */
	char *start;   /* first character of the line that isn't blanck */
	char c;        /* currently scanned character (pointed by read) */
	char emit;     /* flag telling whether line is to be copied */
	char cont;     /* flag telling whether the line continues the previous one */
	char nextcont; /* flag telling whether the line will continues the next one */

	cont = 0;
	c = *(write = read = text);

	/* iteration over lines */
	while (c) {
		/* computes emit, nextcont, emit and start for the current line */
		emit = nextcont = 0;
		start = NULL;
		begin = read;
		while (c && c != '\n') {
			if (!isblank(c)) {
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
		/* emit the line if not empty */
		if (emit) {
			/* removes the blanks on the left of not continuing lines */
			if (!cont && start)
				begin = start;
			/* check if purge applies */
			if (purge && *begin == purge) {
				/* yes, insert new line if requested */
				if (!strncmp(begin+1, "nl\n",3))
					*write++ = '\n';
			} else {
				/* copies the line */
				while (begin != read)
					*write++ = *begin++;
			}
		}
		cont = nextcont;
	}
	*write = 0;
	return (size_t)(write - text);
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
	while (text && !matches(pattern, text, args))
		text = nextline(text);
	return text;
}

/*
 * process one unit
 */
static int process_one_unit(char *spec, struct unitdesc *desc)
{
	char *nsoc, *nsrv, *name;
	int isuser, issystem, issock, isserv;
	size_t len;

	/* finds the configuration directive of the unit */
	isuser = !!offset(spec, "%systemd-unit user\n", NULL);
	issystem = !!offset(spec, "%systemd-unit system\n", NULL);
	issock  = !!offset(spec, "%systemd-unit socket ", &nsoc);
	isserv  = !!offset(spec, "%systemd-unit service ", &nsrv);

	/* check the unit scope */
	if ((isuser + issystem) == 1) {
		desc->scope = isuser ? unitscope_user : unitscope_system;
	} else {
		desc->scope = unitscope_unknown;
	}

	/* check the unit type */
	if ((issock + isserv) == 1) {
		if (issock) {
			desc->type = unittype_socket;
			name = nsoc;
		} else {
			desc->type = unittype_service;
			name = nsrv;
		}
		len = (size_t)(strchrnul(name, '\n') - name);
		desc->name = strndup(name, len);
		desc->name_length = desc->name ? len : 0;
	} else {
		desc->type = unittype_unknown;
		desc->name = NULL;
		desc->name_length = 0;
	}

	desc->content = spec;
	desc->content_length = pack(spec, '%');

	return 0;
}

/*
 * Processes all the units of the 'corpus'.
 * Each unit of the corpus is separated and packed and its
 * charactistics are stored in a descriptor.
 * At the end if no error was found, calls the function 'process'
 * with its given 'closure' and the array descripbing the units.
 * Return 0 in case of success or a negative value in case of error.
 */
static int process_all_units(char *corpus, int (*process)(void *closure, const struct unitdesc descs[], unsigned count), void *closure)
{
	int rc, rc2;
	unsigned n;
	char *beg, *end;
	struct unitdesc *descs, *d;

	descs = NULL;
	n = 0;
	rc = rc2 = 0;

	/* while there is a unit in the corpus */
	while(offset(corpus, "%begin systemd-unit\n", &beg)) {

		/* get the end of the unit */
		end = offset(beg, "%end systemd-unit\n", &corpus);
		if (!end) {
			/* unterminated unit !! */
			ERROR("unterminated unit description!! %s", beg);
			corpus = beg;
			rc2 = -EINVAL;
		} else {
			/* separate the unit from the corpus */
			*end = 0;

			/* allocates a descriptor for the unit */
			d = realloc(descs, (n + 1) * sizeof *descs);
			if (d == NULL)
				rc2 = -ENOMEM;
			else {
				/* creates the unit description */
				memset(&d[n], 0, sizeof *d);
				descs = d;
				rc2 = process_one_unit(beg, &descs[n]);
				if (rc2 >= 0)
					n++;
			}
		}
		/* records the error if there is an error */
		if (rc2 < 0) {
			rc = rc ? : rc2;
			rc2 = 0;
		}
	}

	/* call the function that processes the units */
	if (rc == 0 && process)
		rc = process(closure, descs, n);

	/* cleanup and frees */
	while(n)
		free((char *)(descs[--n].name));
	free(descs);

	return rc;
}

/*
 * Clear the unit generator
 */
void unit_generator_off()
{
	free(template);
	template = NULL;
}

/*
 * Initialises the unit generator with 'filename'.
 * Returns 0 in case of success or a negative number in case of error.
 */
int unit_generator_on(const char *filename)
{
	size_t size;
	char *tmp;
	int rc;

	unit_generator_off();
	rc = getfile(filename ? : FWK_UNIT_CONF, &template, NULL);
	if (!rc) {
		size = pack(template, ';');
		tmp = realloc(template, 1 + size);
		if (tmp)
			template = tmp;
	}
	return rc;
}

/*
 * Applies the object 'jdesc' to the current unit generator.
 * The current unit generator will be set to the default one if not unit
 * was previously set using the function 'unit_generator_on'.
 * The callback function 'process' is then called with the
 * unit descriptors array and the expected closure.
 * Return what returned process in case of success or a negative
 * error code.
 */
int unit_generator_process(struct json_object *jdesc, int (*process)(void *closure, const struct unitdesc descs[], unsigned count), void *closure)
{
	int rc;
	size_t size;
	char *instance;

	rc = template ? 0 : unit_generator_on(NULL);
	if (!rc) {
		instance = NULL;
		rc = apply_mustach(template, jdesc, &instance, &size);
		if (!rc)
			rc = process_all_units(instance, process, closure);
		free(instance);
	}
	return rc;
}


/*
 Copyright (C) 2015-2020 IoT.bzh

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
#include <limits.h>

#include <json-c/json.h>

#include "verbose.h"
#include "utils-file.h"

#include "wgtpkg-mustach.h"
#include "utils-json.h"
#include "wgt-json.h"
#include "utils-systemd.h"

#include "wgtpkg-unit.h"
#include "wgt-strings.h"

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
 * The characters '\n' and ' ' have a special meaning in the search:
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

	nextcont = 0;
	c = *(write = read = text);

	/* iteration over lines */
	while (c) {
		/* computes emit, nextcont, emit and start for the current line */
		cont = nextcont;
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
		if (emit || (cont && !nextcont)) {
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
	char *nsoc, *nsrv, *name, *wanted;
	int isuser, issystem, issock, isserv, iswanted;
	size_t len;

	/* finds the configuration directive of the unit */
	isuser = !!offset(spec, "%systemd-unit user\n", NULL);
	issystem = !!offset(spec, "%systemd-unit system\n", NULL);
	issock  = !!offset(spec, "%systemd-unit socket ", &nsoc);
	isserv  = !!offset(spec, "%systemd-unit service ", &nsrv);
	iswanted = !!offset(spec, "%systemd-unit wanted-by ", &wanted);

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
		len = strcspn(name, " \t\n");
		desc->name = strndup(name, len);
		desc->name_length = desc->name ? len : 0;
	} else {
		desc->type = unittype_unknown;
		desc->name = NULL;
		desc->name_length = 0;
	}

	if (iswanted) {
		len = strcspn(wanted, " \t\n");
		desc->wanted_by = strndup(wanted, len);
		desc->wanted_by_length = len;
	} else {
		desc->wanted_by = NULL;
		desc->wanted_by_length = 0;
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
static int process_all_units(char *corpus, const struct unitconf *conf, int (*process)(void *closure, const struct generatedesc *desc), void *closure, struct json_object *jdesc)
{
	int rc, rc2;
	char *beg, *end, *befbeg, *aftend;
	struct unitdesc *u;
	struct generatedesc gdesc;

	gdesc.conf = conf;
	gdesc.desc = jdesc;
	gdesc.units = NULL;
	gdesc.nunits = 0;
	rc = rc2 = 0;

	/* while there is a unit in the corpus */
	for(;;) {
		befbeg = offset(corpus, "%begin ", &beg);
		end = offset(corpus, "%end ", &aftend);
		if (!befbeg) {
			if (end) {
				/* %end detected without %begin */
				ERROR("unexpected %%end at end");
				rc = rc ? :-EINVAL;
			}
			break;
		}
		if (!end) {
			/* unterminated unit !! */
			ERROR("unterminated unit description!!");
			corpus = beg;
			rc2 = -EINVAL;
		} else if (end < befbeg) {
			/* sequence %end ... %begin detected !! */
			ERROR("unexpected %%end before %%begin");
			corpus = aftend;
			rc2 = -EINVAL;
		} else {
			befbeg =  offset(beg, "%begin ", NULL);
			if (befbeg && befbeg < end) {
				/* sequence %begin ... %begin ... %end detected !! */
				ERROR("unexpected %%begin after %%begin");
				corpus = beg;
				rc2 = -EINVAL;
			} else {
				*end = 0;
				corpus = aftend;
				if (matches("systemd-unit\n", beg, &beg)) {
					if (!matches("systemd-unit\n", aftend, &corpus)) {
						/* end doesnt match */
						ERROR("unmatched %%begin systemd-unit (matching end mismatch)");
						rc2 = -EINVAL;
					} else {
						/* allocates a descriptor for the unit */
						u = realloc((void*)gdesc.units, ((unsigned)gdesc.nunits + 1) * sizeof *gdesc.units);
						if (u == NULL)
							rc2 = -ENOMEM;
						else {
							/* creates the unit description */
							gdesc.units = u;
							u = &u[gdesc.nunits];
							memset(u, 0, sizeof *u);
							rc2 = process_one_unit(beg, u);
							if (rc2 >= 0)
								gdesc.nunits++;
						}
					}
				} else {
					ERROR("unexpected %%begin name");
					rc2 = -EINVAL;
				}
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
		rc = process(closure, &gdesc);

	/* cleanup and frees */
	while(gdesc.nunits) {
		free((void*)(gdesc.units[--gdesc.nunits].name));
		free((void*)(gdesc.units[gdesc.nunits].wanted_by));
	}
	free((void*)gdesc.units);

	return rc;
}

/*
 * Clear the unit generator
 */
void unit_generator_close_template()
{
	free(template);
	template = NULL;
}

/*
 * Initialises the unit generator with the content of the file of path 'filename'.
 * Returns 0 in case of success or a negative number in case of error.
 */
int unit_generator_open_template(const char *filename)
{
	size_t size;
	char *tmp;
	int rc;

	unit_generator_close_template();
	rc = getfile(filename ? : FWK_UNIT_CONF, &template, NULL);
	if (!rc) {
		size = pack(template, ';');
		tmp = realloc(template, 1 + size);
		if (tmp)
			template = tmp;
	}
	return rc;
}

static int add_metadata(struct json_object *jdesc, const struct unitconf *conf)
{
	struct json_object *targets, *targ;
	char portstr[30], afidstr[30];
	int port, afid, i, n;

	if (json_object_object_get_ex(jdesc, string_targets, &targets)) {
		n = json_object_array_length(targets);
		for (i = 0 ; i < n ; i++) {
			targ = json_object_array_get_idx(targets, i);
			if (!conf->new_afid) {
				afid = 0;
				port = 0;
			} else {
				afid = conf->new_afid();
				if (afid < 0)
					return afid;
				port = conf->base_http_ports + afid;
			}
			sprintf(afidstr, "%d", afid);
			sprintf(portstr, "%d", port);
			if (!j_add_many_strings_m(targ,
				"#metatarget.http-port", portstr,
				"#metatarget.afid", afidstr,
				NULL))
				return -1;
		}
	}

	return 	j_add_many_strings_m(jdesc,
		"#metadata.install-dir", conf->installdir,
		"#metadata.icons-dir", conf->icondir,
		NULL) ? 0 : -1;
}

/*
 * Applies the object 'jdesc' augmented of meta data coming
 * from 'conf' to the current unit generator.
 * The current unit generator will be set to the default one if not unit
 * was previously set using the function 'unit_generator_open_template'.
 * The callback function 'process' is then called with the
 * unit descriptors array and the expected closure.
 * Return what returned process in case of success or a negative
 * error code.
 */
int unit_generator_process(struct json_object *jdesc, const struct unitconf *conf, int (*process)(void *closure, const struct generatedesc *desc), void *closure)
{
	int rc;
	size_t size;
	char *instance;

	rc = add_metadata(jdesc, conf);
	if (rc)
		ERROR("can't set the metadata. %m");
	else {
		rc = template ? 0 : unit_generator_open_template(NULL);
		if (!rc) {
			instance = NULL;
			rc = apply_mustach(template, jdesc, &instance, &size);
			if (!rc)
				rc = process_all_units(instance, conf, process, closure, jdesc);
			free(instance);
		}
	}
	return rc;
}

/**************** SPECIALIZED PART *****************************/

static int check_unit_desc(const struct unitdesc *desc, int tells)
{
	if (desc->scope != unitscope_unknown && desc->type != unittype_unknown && desc->name != NULL)
		return 0;

	if (tells) {
		if (desc->scope == unitscope_unknown)
			ERROR("unit of unknown scope");
		if (desc->type == unittype_unknown)
			ERROR("unit of unknown type");
		if (desc->name == NULL)
			ERROR("unit of unknown name");
	}
	errno = EINVAL;
	return -1;
}

static int get_unit_path(char *path, size_t pathlen, const struct unitdesc *desc)
{
	int rc = systemd_get_unit_path(
			path, pathlen, desc->scope == unitscope_user,
			desc->name, desc->type == unittype_socket ? "socket" : "service");

	if (rc < 0)
		ERROR("can't get the unit path for %s", desc->name);

	return rc;
}

static int get_wants_path(char *path, size_t pathlen, const struct unitdesc *desc)
{
	int rc = systemd_get_wants_path(
			path, pathlen, desc->scope == unitscope_user, desc->wanted_by,
			desc->name, desc->type == unittype_socket ? "socket" : "service");

	if (rc < 0)
		ERROR("can't get the wants path for %s and %s", desc->name, desc->wanted_by);

	return rc;
}

static int get_wants_target(char *path, size_t pathlen, const struct unitdesc *desc)
{
	int rc = systemd_get_wants_target(
			path, pathlen,
			desc->name, desc->type == unittype_socket ? "socket" : "service");

	if (rc < 0)
		ERROR("can't get the wants target for %s", desc->name);

	return rc;
}

static int do_uninstall_units(void *closure, const struct generatedesc *desc)
{
	int rc, rc2;
	int i;
	char path[PATH_MAX];
	const struct unitdesc *u;

	rc = 0;
	for (i = 0 ; i < desc->nunits ; i++) {
		u = &desc->units[i];
		rc2 = check_unit_desc(u, 0);
		if (rc2 == 0) {
			rc2 = get_unit_path(path, sizeof path, u);
			if (rc2 >= 0) {
				rc2 = unlink(path);
			}
			if (rc2 < 0 && rc == 0)
				rc = rc2;
			if (u->wanted_by != NULL) {
				rc2 = get_wants_path(path, sizeof path, u);
				if (rc2 >= 0)
					rc2 = unlink(path);
			}
		}
		if (rc2 < 0 && rc == 0)
			rc = rc2;
	}
	return rc;
}

static int do_install_units(void *closure, const struct generatedesc *desc)
{
	int rc;
	int i;
	char path[PATH_MAX + 1], target[PATH_MAX + 1];
	const struct unitdesc *u;

	i = 0;
	while (i < desc->nunits) {
		u = &desc->units[i];
		rc = check_unit_desc(u, 1);
		if (!rc) {
			rc = get_unit_path(path, sizeof path, u);
			if (rc >= 0) {
				rc = putfile(path, u->content, u->content_length);
				if (rc >= 0 && u->wanted_by != NULL) {
					rc = get_wants_path(path, sizeof path, u);
					if (rc >= 0) {
						rc = get_wants_target(target, sizeof target, u);
						if (rc >= 0) {
							unlink(path); /* TODO? check? */
							rc = symlink(target, path);
						}
					}
				}
				i++;
			}
		}
		if (rc < 0)
			goto error;
	}
	return 0;
error:
	i = errno;
	do_uninstall_units(closure, desc);
	errno = i;
	return rc;
}

static int do_install_uninstall(
		struct wgt_info *ifo,
		const struct unitconf *conf,
		int (*doer)(void *, const struct generatedesc *)
)
{
	int rc;
	struct json_object *jdesc;

	jdesc = wgt_info_to_json(ifo);
	if (!jdesc)
		rc = -1;
	else {
		rc = unit_generator_process(jdesc, conf, doer, NULL);
		json_object_put(jdesc);
	}
	return rc;
}

int unit_install(struct wgt_info *ifo, const struct unitconf *conf)
{
	return do_install_uninstall(ifo, conf, do_install_units);
}

int unit_uninstall(struct wgt_info *ifo, const struct unitconf *conf)
{
	return do_install_uninstall(ifo, conf, do_uninstall_units);
}


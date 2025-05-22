/*
 Copyright (C) 2015-2025 IoT.bzh Company

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

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-file.h>
#include <rp-utils/rp-jsonc.h>

#include "apply-mustach.h"
#include "utils-systemd.h"

#include "unit-process.h"

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
 * first character after the pattern in 'text' is stored in 'after'.
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
 * Removes duplicate slashes of a null terminated 'text'.
 *
 * Returns the size after packing (offset of the ending null).
 */
static size_t dedup_slashes(char *text)
{
#ifndef NO_BLANK_LINE
#  define NO_BLANK_LINE 1
#endif
#if !NO_BLANK_LINE
#  define NRNLMAX 2
#else
#  define NRNLMAX 1
#endif

	int nrnl = NRNLMAX;  /* count of successives new line (drop first empty lines) */
	int nrs;       /* count of successives slashes */
	int acnt;      /* allowed count of successives slashes */
	char *read;    /* read iterator */
	char *write;   /* write iterator */
	char c;        /* currently scanned character (pointed by read) */

	/* iteration over lines */
	c = *(write = read = text);
	while (c) {
		if (c == '\n') {
			if (++nrnl <= NRNLMAX)
				*write++ = c;
			c = *++read;
		}
		else {
			nrnl = 0;
			if (c != '/') {
				*write++ = c;
				c = *++read;
			}
			else {
				/* compute the allowed count of successive slashes */
#define PRECEDED_BY(txt) ((size_t)(write - text) >= strlen(txt) && !memcmp(txt, write - strlen(txt), strlen(txt)))
				if (PRECEDED_BY("http:") || PRECEDED_BY("https:"))
					acnt = 2;
				else if (PRECEDED_BY("file:"))
					acnt = 3;
				else
					acnt = 1;
#undef PRECEDED_BY
				for (nrs = 0 ; c == '/' && nrs < acnt ; nrs++) {
					*write++ = '/';
					c = *++read;
				}
				while (c == '/')
					c = *++read;
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

	/* check if wanted-by */
	if (iswanted) {
		len = strcspn(wanted, " \t\n");
		desc->wanted_by = strndup(wanted, len);
		desc->wanted_by_length = len;
	} else {
		desc->wanted_by = NULL;
		desc->wanted_by_length = 0;
	}

	/* record the normalized content */
	desc->content = spec;
	desc->content_length = pack(spec, '%');

	return 0;
}

/*
 * split the 'corpus' to its units (change the corpus by inserting nuls)
 * Each unit of the corpus is separated and packed and its
 * charactistics are stored in a descriptor.
 * At the end if no error was found, returns the count of units found and
 * store them descriptor in units.
 * A negative value is returned in case of error.
 */
static int corpus_split(
	char *corpus,
	struct unitdesc **units
) {
	int rc, rc2, nrunits;
	char *beg, *end, *befbeg, *aftend;
	struct unitdesc *u, *arrunits;
	size_t size;

	/* initialize */
	arrunits = NULL;
	nrunits = 0;
	rc = rc2 = 0;

	/* while there is a unit in the corpus */
	while(corpus != NULL) {
		/* locate the %begin / %end tags */
		befbeg = offset(corpus, "%begin ", &beg);
		end = offset(corpus, "%end ", &aftend);
		if (!befbeg) {
			if (end) {
				/* %end detected without %begin */
				RP_ERROR("unexpected %%end at end");
				rc2 = -EINVAL;
			}
			corpus = NULL;
		}
		else if (!end) {
			/* unterminated unit !! */
			RP_ERROR("unterminated unit description!!");
			corpus = beg;
			rc2 = -EINVAL;
			corpus = NULL;
		}
		else if (end < befbeg) {
			/* sequence %end ... %begin detected !! */
			RP_ERROR("unexpected %%end before %%begin");
			corpus = aftend;
			rc2 = -EINVAL;
			corpus = NULL;
		}
		else {
			befbeg =  offset(beg, "%begin ", NULL);
			if (befbeg && befbeg < end) {
				/* sequence %begin ... %begin ... %end detected !! */
				RP_ERROR("unexpected %%begin after %%begin");
				corpus = beg;
				rc2 = -EINVAL;
				corpus = NULL;
			}
			else {
				/* looks good, extract the unit from the corpus */
				*end = 0;
				corpus = aftend;

				/* process systemd sprcific values */
				if (matches("systemd-unit\n", beg, &beg)) {
					if (!matches("systemd-unit\n", aftend, &corpus)) {
						/* end doesnt match */
						RP_ERROR("unmatched %%begin systemd-unit (matching end mismatch)");
						rc2 = -EINVAL;
					}
					else {
						/* allocates a descriptor for the unit */
						size = ((unsigned)nrunits + 1) * sizeof *u;
						u = realloc(arrunits, size);
						if (u == NULL) {
							rc2 = -ENOMEM;
							corpus = NULL;
						}
						else {
							/* creates the unit description */
							arrunits = u;
							u = &u[nrunits];
							memset(u, 0, sizeof *u);
							rc2 = process_one_unit(beg, u);
							if (rc2 >= 0)
								nrunits++;
						}
					}
				}

				/* process other values */
				else {
					RP_ERROR("unexpected %%begin name");
					rc2 = -EINVAL;
				}
			}
		}
		/* records the error if there is an error */
		if (rc2 < 0) {
			rc = rc ? rc : rc2;
			rc2 = 0;
		}
	}
	if (rc >= 0) {
		*units = arrunits;
		rc = nrunits;
	}
	else {
		free(arrunits);
		*units = NULL;
	}
	return rc;
}

/*
 * Clear the unit processor template
 */
void unit_process_close_template()
{
	free(template);
	template = NULL;
}

/*
 * Initialises the unit processor template
 * with the content of the file of path 'filename'.
 * Returns 0 in case of success or a negative number in case of error.
 */
int unit_process_open_template(const char *filename)
{
	size_t size;
	char *tmp;
	int rc;

	unit_process_close_template();
	rc = rp_file_get(filename ? : FWK_UNIT_CONF, &template, NULL);
	if (!rc) {
		size = pack(template, ';');
		tmp = realloc(template, 1 + size);
		if (tmp)
			template = tmp;
	}
	return rc;
}

/*
 * calls the process with template instance using jdesc
 */
int unit_process_raw(
	struct json_object *jdesc,
	int (*process)(void *closure, char *text, size_t size),
	void *closure
) {
	int rc;
	size_t size;
	char *instance;

	/* ensure template */
	rc = template ? 0 : unit_process_open_template(NULL);
	if (!rc) {
		/* apply mustach template to jdesc */
		size = 0;
		instance = NULL;
		rc = apply_mustach(template, jdesc, &instance, &size);
		if (!rc) {
			/* process instanciated template as units */
			size = dedup_slashes(instance);
			rc = process(closure, instance, size);
		}
		free(instance);
	}
	return rc;
}

/******************************************************************************/

/**
 * Structure used by `unit_process_split` in its callback
 * for unit_process_raw
 */
struct for_split
{
	/** callback for processing splitted units */
	int (*process)(void *closure, const struct unitdesc *units, int nrunits);
	/** closures for the callback */
	void *closure;
};

/*
 * Processes all the units of the 'corpus'.
 * Each unit of the corpus is separated and packed and its
 * charactistics are stored in a descriptor.
 * At the end if no error was found, calls the function 'process'
 * with its given 'closure' and the array descripbing the units.
 * Return 0 in case of success or a negative value in case of error.
 */
static
int internal_for_split(
	void *closure,
	char *corpus,
	size_t size
) {
	int rc, nru;
	struct unitdesc *units;
	struct for_split *fspl = closure;

	/* split the corpus */
	rc = nru = corpus_split(corpus, &units);
	if (rc >= 0) {
		/* call the function that processes the units */
		if (rc > 0 && fspl->process)
			rc = fspl->process(fspl->closure, units, nru);

		/* cleanup and frees */
		while(nru) {
			nru--;
			free((void*)(units[nru].name));
			free((void*)(units[nru].wanted_by));
		}
		free(units);
	}

	return rc;
}

/*
 * calls the process with splitted files from template instance using jdesc
 */
int unit_process_split(
	struct json_object *jdesc,
	int (*process)(void *closure, const struct unitdesc *units, int nrunits),
	void *closure
) {
	struct for_split fspl = {
			.process = process,
			.closure = closure
		};

	return unit_process_raw(jdesc, internal_for_split, &fspl);
}


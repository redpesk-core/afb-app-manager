/*
 Copyright (C) 2015-2020 IoT.bzh Company

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

#include <stdio.h>
#include <stdarg.h>

#include "verbose.h"

int verbosity = 1;

#define LEVEL(x) ((x) < 0 ? 0 : (x) > 7 ? 7 : (x))

#if defined(VERBOSE_WITH_SYSLOG)

#include <syslog.h>

void vverbose(int level, const char *file, int line, const char *fmt, va_list args)
{
	char *p;

	if (file == NULL || vasprintf(&p, fmt, args) < 0)
		vsyslog(level, fmt, args);
	else {
		syslog(LEVEL(level), "%s [%s:%d]", p, file, line);
		free(p);
	}
}

void verbose_set_name(const char *name, int authority)
{
	closelog();
	openlog(name, LOG_PERROR, authority ? LOG_AUTH : LOG_USER);
}

#else

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static char *appname;

static int appauthority;

static const char *prefixes[] = {
	"<0> EMERGENCY",
	"<1> ALERT",
	"<2> CRITICAL",
	"<3> ERROR",
	"<4> WARNING",
	"<5> NOTICE",
	"<6> INFO",
	"<7> DEBUG"
};

void vverbose(int level, const char *file, int line, const char *fmt, va_list args)
{
	int saverr = errno;
	int tty = isatty(fileno(stderr));
	errno = saverr;

	fprintf(stderr, "%s: ", prefixes[LEVEL(level)] + (tty ? 4 : 0));
	vfprintf(stderr, fmt, args);
	if (file != NULL && (!tty || verbosity >5))
		fprintf(stderr, " [%s:%d]\n", file, line);
	else
		fprintf(stderr, "\n");
}

void verbose_set_name(const char *name, int authority)
{
	free(appname);
	appname = name ? strdup(name) : NULL;
	appauthority = authority;
}

#endif

void verbose(int level, const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vverbose(level, file, line, fmt, ap);
	va_end(ap);
}


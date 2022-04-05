/*
 Copyright (C) 2015-2022 IoT.bzh Company
 Copyright (C) 2020 Konsulko Group

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

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <rp-utils/rp-verbose.h>
#include "utils-dir.h"
#include "secmgr-wrap.h"
#include "wgtpkg-unit.h"
#include "wgt.h"
#include "wgt-info.h"

static int setdown_units(struct wgt_info *ifo, const char *installdir)
{
	struct unitconf uconf;

	/* generate and install units */
	uconf.installdir = installdir;
	uconf.icondir = FWK_ICON_DIR;
	uconf.new_afid = 0;
	uconf.base_http_ports = 0;
	return unit_uninstall(ifo, &uconf);
}

static int setdown_files_and_security(const struct wgt_desc *desc)
{
	char path[PATH_MAX];
	int rc;

	/* removes the icon of the application */
	rc = snprintf(path, sizeof path, "%s/%s", FWK_ICON_DIR, desc->idaver);
	assert(rc < (int)sizeof path);
	rc = unlink(path);
	if (rc < 0 && errno != ENOENT) {
		RP_ERROR("can't remove '%s': %m", path);
		return -1;
	}

#if DISTINCT_VERSIONS
	rc = secmgr_begin(desc->id);
#else
	rc = secmgr_begin(desc->idaver);
#endif
	if (rc < 0) {
		RP_ERROR("can't init sec lsm manager context");
		return rc;
	}
	rc = secmgr_uninstall();
	secmgr_end();
	if (rc < 0)
		RP_ERROR("can't uninstall sec lsm manager context");
	return rc;
}

static int uninstall_at(const char *installdir)
{
	struct wgt_info *ifo;

	/* removes the units */
	ifo = wgt_info_createat(AT_FDCWD, installdir, 1, 1, 1);
	if (!ifo) {
		RP_ERROR("can't read widget config in directory '%s': %m", installdir);
		return -1;
	}

	setdown_units(ifo, installdir);
	setdown_files_and_security(wgt_info_desc(ifo));
	wgt_info_unref(ifo);
	return 0;
}

#if WITH_WIDGETS
/* uninstall the widget of idaver */
int uninstall_widget(const char *idaver, const char *root)
{
	char path[PATH_MAX];
	int rc;

	RP_NOTICE("-- UNINSTALLING widget of id %s from %s --", idaver, root);

	/* find the last '@' of the id */
#if DISTINCT_VERSIONS
	char *id;
	char *ver;
	const char *at;
	at = strrchr(idaver, '@');
	if (at == NULL) {
		RP_ERROR("bad widget id '%s', no @", idaver);
		errno = EINVAL;
		return -1;
	}
	id = strndupa(idaver, (size_t)(at - idaver));
	ver = strdupa(at + 1);

	/* compute the path */
	rc = snprintf(path, sizeof path, "%s/%s/%s", root, id, ver);
#else
	rc = snprintf(path, sizeof path, "%s/%s", root, idaver);
#endif
	if (rc >= (int)sizeof path) {
		RP_ERROR("bad widget id '%s', too long", idaver);
		errno = EINVAL;
		return -1;
	}

	/* uninstall from directory */
	uninstall_at(path);

	/* removes the directory of the application */
	rc = remove_directory(path, 1);
	if (rc < 0) {
		RP_ERROR("while removing directory '%s': %m", path);
		return -1;
	}

#if DISTINCT_VERSIONS
	/* removes the parent directory if empty */
	rc = snprintf(path, sizeof path, "%s/%s", root, id);
	assert(rc < (int)sizeof path);
	rc = rmdir(path);
	if (rc < 0 && errno == ENOTEMPTY)
		return rc;
	if (rc < 0) {
		RP_ERROR("while removing directory '%s': %m", path);
		return -1;
	}
#endif

	return 0;
}
#endif

/* uninstall the widget of installdir */
int uninstall_redpesk(const char *installdir)
{
	RP_NOTICE("-- UNINSTALLING redpesk agl from %s  --", installdir);
	return uninstall_at(installdir);
}

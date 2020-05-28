/*
 Copyright (C) 2015-2020 IoT.bzh
 Copyright (C) 2020 Konsulko Group

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

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "verbose.h"
#include "utils-dir.h"
#include "secmgr-wrap.h"
#include "wgtpkg-unit.h"
#include "wgt.h"
#include "wgt-info.h"

/* uninstall the widget of idaver */
int uninstall_widget(const char *idaver, const char *root)
{
#if DISTINCT_VERSIONS
	char *id;
	char *ver;
	const char *at;
#endif
	char path[PATH_MAX];
	int rc;
	struct unitconf uconf;
	struct wgt_info *ifo;

	NOTICE("-- UNINSTALLING widget of id %s from %s --", idaver, root);

	/* find the last '@' of the id */
#if DISTINCT_VERSIONS
	at = strrchr(idaver, '@');
	if (at == NULL) {
		ERROR("bad widget id '%s', no @", idaver);
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
		ERROR("bad widget id '%s', too long", idaver);
		errno = EINVAL;
		return -1;
	}

	/* removes the units */
	ifo = wgt_info_createat(AT_FDCWD, path, 1, 1, 1);
	if (!ifo) {
		ERROR("can't read widget config in directory '%s': %m", path);
		return -1;
	}
	uconf.installdir = path;
	uconf.icondir = FWK_ICON_DIR;
	uconf.new_afid = 0;
	uconf.base_http_ports = 0;
	unit_uninstall(ifo, &uconf);
	wgt_info_unref(ifo);

	/* removes the directory of the application */
	rc = remove_directory(path, 1);
	if (rc < 0) {
		ERROR("while removing directory '%s': %m", path);
		return -1;
	}

	/* removes the icon of the application */
	rc = snprintf(path, sizeof path, "%s/%s", FWK_ICON_DIR, idaver);
	assert(rc < (int)sizeof path);
	rc = unlink(path);
	if (rc < 0 && errno != ENOENT) {
		ERROR("can't remove '%s': %m", path);
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
		ERROR("while removing directory '%s': %m", path);
		return -1;
	}

	/*
	 * parent directory removed: last occurrence of the application
	 * uninstall it for the security-manager
	 */
	rc = secmgr_init(id);
#else
	rc = secmgr_init(idaver);
#endif
	if (rc) {
		ERROR("can't init security manager context");
		return -1;
	}
	rc = secmgr_uninstall();
	if (rc) {
		ERROR("can't uninstall security manager context");
		return -1;
	}
	return 0;
}

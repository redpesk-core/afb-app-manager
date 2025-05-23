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

#include "afmpkg-std.h"

#include <stddef.h>
#include <errno.h>

#include <rp-utils/rp-verbose.h>

#include "path-type.h"

#include "secmgr-wrap.h"
#include "unit-oper.h"

#include "afmpkg.h"

static const char * const path_type_names[] = {
#if defined(SEC_LSM_MANAGER_PATH_TYPE_DEFAULT) /* defined since sec-lsm-manager 2.6.2 */
    [path_type_Unset]       = SEC_LSM_MANAGER_PATH_TYPE_DEFAULT,
    [path_type_Default]     = SEC_LSM_MANAGER_PATH_TYPE_DEFAULT,
    [path_type_Conf]        = SEC_LSM_MANAGER_PATH_TYPE_CONF,
    [path_type_Data]        = SEC_LSM_MANAGER_PATH_TYPE_DATA,
    [path_type_Exec]        = SEC_LSM_MANAGER_PATH_TYPE_EXEC,
    [path_type_Http]        = SEC_LSM_MANAGER_PATH_TYPE_HTTP,
    [path_type_Icon]        = SEC_LSM_MANAGER_PATH_TYPE_ICON,
    [path_type_Id]          = SEC_LSM_MANAGER_PATH_TYPE_ID,
    [path_type_Lib]         = SEC_LSM_MANAGER_PATH_TYPE_LIB,
    [path_type_Plug]        = SEC_LSM_MANAGER_PATH_TYPE_PLUG,
    [path_type_Public]      = SEC_LSM_MANAGER_PATH_TYPE_PUBLIC,
    [path_type_Public_Exec] = SEC_LSM_MANAGER_PATH_TYPE_PUBLIC,
    [path_type_Public_Lib]  = SEC_LSM_MANAGER_PATH_TYPE_PUBLIC
#else
    [path_type_Unset]       = "default",
    [path_type_Default]     = "default",
    [path_type_Conf]        = "conf",
    [path_type_Data]        = "data",
    [path_type_Exec]        = "exec",
    [path_type_Http]        = "http",
    [path_type_Icon]        = "icon",
    [path_type_Id]          = "id",
    [path_type_Lib]         = "lib",
    [path_type_Plug]        = "plug",
    [path_type_Public]      = "public",
    [path_type_Public_Exec] = "public",
    [path_type_Public_Lib]  = "public"
#endif
};

typedef
struct {
	afmpkg_mode_t mode;
}
	state_t;

static int do_uninstall_units(state_t *state, const struct unitdesc *units, int nrunits, int quiet)
{
	int i, rc, rc2, logmsk;

	logmsk = rp_logmask;
	if (quiet)
		rp_set_logmask(0);
	for (rc = i = 0 ; i < nrunits ; i++) {
		rc2 = unit_oper_uninstall(&units[i]);
		if (rc2 < 0)
			rc = rc2;
	}
	rp_set_logmask(logmsk);
	return rc;
}

static int uninstall_units(state_t *state, const struct unitdesc *units, int nrunits)
{
	return do_uninstall_units(state, units, nrunits, 0);
}

static int install_units(state_t *state, const struct unitdesc *units, int nrunits)
{
	int i, rc;

	/* check that no file is overwritten by the installation */
	for (i = 0 ; i < nrunits ; i++) {
		rc = unit_oper_check_files(&units[i], 0);
		if (rc < 0)
			return rc;
	}

	/* install the units */
	for (i = 0 ; i < nrunits ; i++) {
		rc = unit_oper_install(&units[i]);
		if (rc < 0)
			goto error;
	}
	return 0;
error:
	i = errno;
	do_uninstall_units(state, units, nrunits, 1);
	errno = i;
	return rc;
}

static
int
begin(
	void *closure,
	const char *appid,
	afmpkg_mode_t mode
) {
	state_t *state = closure;
	state->mode = mode;
	return secmgr_begin(NULL);
}

static
int
tagfile(
	void *closure,
	const char *path,
	path_type_t type
) {
	state_t *state = closure;

	if (type <= path_type_Unset || type >= _path_type_count_) {
		RP_ERROR("Invalid path type %d", (int)type);
		return -EINVAL;
	}

	/* before uninstalling, set the id for making files inaccessible */
	if (state->mode != Afmpkg_Install)
		type = path_type_Id;

	return secmgr_path(path, path_type_names[type]);
}

static
int
setperm(
	void *closure,
	const char *perm
) {
	state_t *state = closure;
	return secmgr_permit(perm);
}

static
int setplug(
	void *closure,
	const char *exportdir,
	const char *importid,
	const char *importdir
) {
	state_t *state = closure;
	return secmgr_plug(exportdir, importid, importdir);
}

static
int
setunits(
	void *closure,
	const struct unitdesc *units,
	int nrunits
) {
	state_t *state = closure;
	return (state->mode == Afmpkg_Install ? install_units : uninstall_units)(state, units, nrunits);
}

static
int
end(
	void *closure,
	int status
) {
	state_t *state = closure;
	int rc = status;
	if (status == 0)
		rc = (state->mode == Afmpkg_Install ? secmgr_install : secmgr_uninstall)();
	secmgr_end();
	return rc;
}

static afmpkg_operations_t opers = {
	.begin = begin,
	.tagfile = tagfile,
	.setperm = setperm,
	.setplug = setplug,
	.setunits = setunits,
	.end = end
};

/* install afm package */
int afmpkg_std_install(
	const afmpkg_t *apkg
) {
	state_t state = { .mode = Afmpkg_Nop };
	return afmpkg_install(apkg, &opers, &state);
}

/* install afm package */
int afmpkg_std_uninstall(
	const afmpkg_t *apkg
) {
	state_t state = { .mode = Afmpkg_Nop };
	return afmpkg_uninstall(apkg, &opers, &state);
}


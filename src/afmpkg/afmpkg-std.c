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
#include <string.h>

#include <rp-utils/rp-verbose.h>

#if SIMULATE_SEC_LSM_MANAGER
#include "simulate-sec-lsm-manager.h"
#else
#include <sec-lsm-manager.h>
#endif

#include "path-type.h"
#include "unit-oper.h"

/*************************************************************
** definition of the local state
*************************************************************/
typedef
struct {
	/** install/ uninstall mode */
	afmpkg_mode_t mode;

	/** conection to the security manager */
	sec_lsm_manager_t *slmhndl;
}
	state_t;

/*************************************************************
** local functions for processing units
*************************************************************/

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

/*************************************************************
** local interface functions
*************************************************************/

static
int
begin(
	void *closure,
	const char *appid,
	afmpkg_mode_t mode
) {
	state_t *state = closure;
	int rc = sec_lsm_manager_create(&state->slmhndl, NULL);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_create failed: %s", strerror(-rc));
	else if (appid != NULL) {
		rc = sec_lsm_manager_set_id(state->slmhndl, appid);
		if (rc >= 0)
			state->mode = mode;
		else {
			RP_ERROR("sec_lsm_manager_set_id %s failed: %s",
			         appid, strerror(-rc));
			sec_lsm_manager_destroy(state->slmhndl);
			state->slmhndl = NULL;
		}
	}
	return rc;
}

static
int
tagfile(
	void *closure,
	const char *path,
	path_type_t type
) {
	state_t *state = closure;
	const char *slm_type;
	int rc;

	if (type <= path_type_Unset || type >= _path_type_count_) {
		RP_ERROR("Invalid path type %d", (int)type);
		return -EINVAL;
	}

	/* before uninstalling, set the id for making files inaccessible */
	if (state->mode != Afmpkg_Install)
		type = path_type_Id;

	slm_type = path_type_for_slm(type);
	rc = sec_lsm_manager_add_path(state->slmhndl, path, slm_type);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_add_path %s -> %s failed: %s",
		         slm_type, path, strerror(-rc));
	return rc;
}

static
int
setperm(
	void *closure,
	const char *permission
) {
	state_t *state = closure;
	int rc = sec_lsm_manager_add_permission(state->slmhndl, permission);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_add_permission %s failed: %s",
		         permission, strerror(-rc));
	return rc;
}

static
int setplug(
	void *closure,
	const char *exportdir,
	const char *importid,
	const char *importdir
) {
	state_t *state = closure;
	int rc = sec_lsm_manager_add_plug(state->slmhndl,
	                                  exportdir, importid, importdir);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_add_plug %s -> %s @ %s failed: %s",
		         exportdir, importid, importdir, strerror(-rc));
	return rc;
}

static
int
setunits(
	void *closure,
	const struct unitdesc *units,
	int nrunits
) {
	state_t *state = closure;
	if (state->mode == Afmpkg_Install)
		return install_units(state, units, nrunits);
	else
		return uninstall_units(state, units, nrunits);
}

static
int
end(
	void *closure,
	int status
) {
	state_t *state = closure;
	int rc = status;
	if (state->slmhndl != NULL) {
		if (status == 0) {
			if (state->mode == Afmpkg_Install)
				rc = sec_lsm_manager_install(state->slmhndl);
			else
				rc = sec_lsm_manager_uninstall(state->slmhndl);
			if (rc < 0)
				RP_ERROR("sec_lsm_manager_%sinstall failed: %s",
					state->mode == Afmpkg_Install ? "" : "un",
					strerror(-rc));
		}
		sec_lsm_manager_destroy(state->slmhndl);
		state->slmhndl = NULL;
	}
	return rc;
}

/* install afm package */
int afmpkg_std_install(
	const afmpkg_t *apkg
) {
	state_t state = {
		.mode = Afmpkg_Nop,
		.slmhndl = NULL
	};
	afmpkg_operations_t opers = {
		.begin = begin,
		.tagfile = tagfile,
		.setperm = setperm,
		.setplug = setplug,
		.setunits = setunits,
		.end = end
	};

	return afmpkg_install(apkg, &opers, &state);
}

/* install afm package */
int afmpkg_std_uninstall(
	const afmpkg_t *apkg
) {
	state_t state = {
		.mode = Afmpkg_Nop,
		.slmhndl = NULL
	};
	afmpkg_operations_t opers = {
		.begin = begin,
		.tagfile = tagfile,
		.setperm = setperm,
		.setplug = setplug,
		.setunits = setunits,
		.end = end
	};

	return afmpkg_uninstall(apkg, &opers, &state);
}


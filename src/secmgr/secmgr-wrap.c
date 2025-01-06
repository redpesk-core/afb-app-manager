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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include <rp-utils/rp-verbose.h>
#include "secmgr-wrap.h"

#if SIMULATE_SEC_LSM_MANAGER
#include "simulate-sec-lsm-manager.h"
#else
#include <sec-lsm-manager.h>
#endif

const char secmgr_pathtype_conf[] = "conf";
const char secmgr_pathtype_data[] = "data";
const char secmgr_pathtype_exec[] = "exec";
const char secmgr_pathtype_http[] = "http";
const char secmgr_pathtype_icon[] = "icon";
const char secmgr_pathtype_lib[] = "lib";
const char secmgr_pathtype_plug[] = "plug";
const char secmgr_pathtype_public[] = "public";
const char secmgr_pathtype_id[] = "id";
const char secmgr_pathtype_default[] = "default";

static sec_lsm_manager_t *sm_handle = NULL;

static int retcode(int rc)
{
	if (rc < 0)
		errno = -rc;
	return rc;
}

int secmgr_begin(const char *id)
{
	int rc;

	if (sm_handle != NULL)
		return -EBUSY;

	rc = sec_lsm_manager_create(&sm_handle, NULL);

	if (rc < 0) {
		RP_ERROR("sec_lsm_manager_create failed");
		goto ret;
	}

	if (id == NULL)
		goto ret;

	rc = sec_lsm_manager_set_id(sm_handle, id);

	if (rc < 0) {
		RP_ERROR("sec_lsm_manager_set_id failed");
		goto error;
	}

	goto ret;

error:
	secmgr_end();
ret:
	return retcode(rc);
}

void secmgr_end()
{
	if (sm_handle != NULL) {
		sec_lsm_manager_destroy(sm_handle);
		sm_handle = NULL;
	}
}

int secmgr_install()
{
	int rc;
	assert(sm_handle != NULL);
	rc = sec_lsm_manager_install(sm_handle);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_install failed %d %s", -rc, strerror(-rc));
	return retcode(rc);
}

int secmgr_uninstall()
{
	int rc;
	assert(sm_handle != NULL);
	rc = sec_lsm_manager_uninstall(sm_handle);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_uninstall failed");
	return retcode(rc);
}

int secmgr_permit(const char *permission)
{
	int rc;
	assert(sm_handle != NULL);
	rc = sec_lsm_manager_add_permission(sm_handle, permission);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_add_permission %s failed", permission);
	return retcode(rc);
}

int secmgr_path(const char *pathname, const char *pathtype)
{
	int rc;
	assert(sm_handle != NULL);
	rc = sec_lsm_manager_add_path(sm_handle, pathname, pathtype);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_add_path %s failed", pathname);
	return retcode(rc);
}


int secmgr_plug(const char *expdir, const char *impid, const char *impdir)
{
	int rc;
	assert(sm_handle != NULL);
	rc = sec_lsm_manager_add_plug(sm_handle, expdir, impid, impdir);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_add_plug %s for %s failed", expdir, impid);
	return retcode(rc);
}


int secmgr_path_conf(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_conf);
}

int secmgr_path_data(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_data);
}

int secmgr_path_exec(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_exec);
}

int secmgr_path_http(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_http);
}

int secmgr_path_icon(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_icon);
}

int secmgr_path_lib(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_lib);
}

int secmgr_path_plug(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_plug);
}

int secmgr_path_public(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_public);
}

int secmgr_path_id(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_id);
}

int secmgr_path_remove(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_id);
}

int secmgr_path_default(const char *pathname)
{
	return secmgr_path(pathname, secmgr_pathtype_default);
}
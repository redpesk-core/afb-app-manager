/*
 Copyright (C) 2015-2023 IoT.bzh Company

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

static int addpath(const char *pathname, const char *path_type)
{
	int rc;
	assert(sm_handle != NULL);
	rc = sec_lsm_manager_add_path(sm_handle, pathname, path_type);
	if (rc < 0)
		RP_ERROR("sec_lsm_manager_add_path %s failed", pathname);
	return retcode(rc);
}

int secmgr_path_conf(const char *pathname)
{
	return addpath(pathname, "conf");
}

int secmgr_path_data(const char *pathname)
{
	return addpath(pathname, "data");
}

int secmgr_path_exec(const char *pathname)
{
	return addpath(pathname, "exec");
}

int secmgr_path_http(const char *pathname)
{
	return addpath(pathname, "http");
}

int secmgr_path_icon(const char *pathname)
{
	return addpath(pathname, "icon");
}

int secmgr_path_lib(const char *pathname)
{
	return addpath(pathname, "lib");
}

int secmgr_path_public(const char *pathname)
{
	return addpath(pathname, "public");
}

int secmgr_path_id(const char *pathname)
{
	return addpath(pathname, "id");
}

int secmgr_path_remove(const char *pathname)
{
	return addpath(pathname, "id");
}

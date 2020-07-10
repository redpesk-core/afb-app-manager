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

#include <string.h>
#include <errno.h>
#include <assert.h>

#if SIMULATE_SECURITY_MANAGER
#include "simulation/security-manager.h"
#else
#include <security-manager.h>
#endif

#include "verbose.h"
#include "secmgr-wrap.h"

static app_inst_req *request = NULL;

static int retcode(enum lib_retcode rc)
{
	switch (rc) {
	case SECURITY_MANAGER_SUCCESS: return 0;
	case SECURITY_MANAGER_ERROR_INPUT_PARAM: errno = EINVAL; break;
	case SECURITY_MANAGER_ERROR_MEMORY: errno = ENOMEM; break;
	case SECURITY_MANAGER_ERROR_REQ_NOT_COMPLETE: errno = EBADMSG; break;
	case SECURITY_MANAGER_ERROR_AUTHENTICATION_FAILED: errno = EPERM; break;
	case SECURITY_MANAGER_ERROR_ACCESS_DENIED: errno = EACCES; break;
	default: errno = ECANCELED; break;
	}
	return -1;
}

int secmgr_init(const char *id)
{
	int rc;
	assert(request == NULL);
	rc = security_manager_app_inst_req_new(&request);
	if (rc != SECURITY_MANAGER_SUCCESS)
		ERROR("security_manager_app_inst_req_new failed");
	else {
		rc = security_manager_app_inst_req_set_pkg_id(request, id);
		if (rc != SECURITY_MANAGER_SUCCESS)
			ERROR("security_manager_app_inst_req_set_pkg_id failed");
		else {
			rc = security_manager_app_inst_req_set_app_id(request, id);
			if (rc != SECURITY_MANAGER_SUCCESS)
				ERROR("security_manager_app_inst_req_set_app_id failed");
		}
	}
	if (rc != SECURITY_MANAGER_SUCCESS)
		secmgr_cancel();
	return retcode(rc);
}

void secmgr_cancel()
{
	security_manager_app_inst_req_free(request);
	request = NULL;
}

int secmgr_install()
{
	int rc;
	assert(request != NULL);
	rc = security_manager_app_install(request);
	if (rc != SECURITY_MANAGER_SUCCESS)
		ERROR("security_manager_app_install failed");
	secmgr_cancel();
	return retcode(rc);
}

int secmgr_uninstall()
{
	int rc;
	assert(request != NULL);
	rc = security_manager_app_uninstall(request);
	if (rc != SECURITY_MANAGER_SUCCESS)
		ERROR("security_manager_app_uninstall failed");
	secmgr_cancel();
	return retcode(rc);
}

int secmgr_permit(const char *permission)
{
	int rc;
	assert(request != NULL);
	rc = security_manager_app_inst_req_add_privilege(request, permission);
	if (rc != SECURITY_MANAGER_SUCCESS)
		ERROR("security_manager_app_inst_add_privilege %s failed", permission);
	return retcode(rc);
}

static int addpath(const char *pathname, enum app_install_path_type type)
{
	int rc;
	assert(request != NULL);
	rc = security_manager_app_inst_req_add_path(request, pathname, type);
	if (rc != SECURITY_MANAGER_SUCCESS)
		ERROR("security_manager_app_inst_add_path %s failed", pathname);
	return retcode(rc);
}

int secmgr_path_public_read_only(const char *pathname)
{
	return addpath(pathname, SECURITY_MANAGER_PATH_PUBLIC_RO);
}

int secmgr_path_read_only(const char *pathname)
{
	return addpath(pathname, SECURITY_MANAGER_PATH_RO);
}

int secmgr_path_read_write(const char *pathname)
{
	return addpath(pathname, SECURITY_MANAGER_PATH_RW);
}

int secmgr_path_private(const char *pathname)
{
	return addpath(pathname, SECURITY_MANAGER_PATH_PRIVATE);
}

int secmgr_prepare_exec(const char *appid)
{
	return retcode(security_manager_prepare_app(appid));
}


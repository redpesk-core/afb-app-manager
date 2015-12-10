/*
 Copyright 2015 IoT.bzh

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

#if 0
#include <security-manager.h>
#else
#include <stdio.h>
#include <stdint.h>
enum lib_retcode {
	SECURITY_MANAGER_SUCCESS,
	SECURITY_MANAGER_ERROR_INPUT_PARAM,
	SECURITY_MANAGER_ERROR_MEMORY,
	SECURITY_MANAGER_ERROR_REQ_NOT_COMPLETE,
	SECURITY_MANAGER_ERROR_AUTHENTICATION_FAILED,
	SECURITY_MANAGER_ERROR_ACCESS_DENIED
};
enum app_install_path_type {
	SECURITY_MANAGER_PATH_PUBLIC_RO,
	SECURITY_MANAGER_PATH_RO,
	SECURITY_MANAGER_PATH_RW
};
typedef void app_inst_req;
static int diese = 0;
#define  security_manager_app_inst_req_free(r) \
 (printf("security_manager_app_inst_req_free(%p)\n",r),(void)0)

#define  security_manager_app_inst_req_new(pr) \
 (*(pr)=(void*)(intptr_t)(++diese), printf("security_manager_app_inst_req_new(%p)\n",*pr), SECURITY_MANAGER_SUCCESS)

#define security_manager_app_inst_req_set_pkg_id(r,i) \
 (printf("security_manager_app_inst_req_set_pkg_id(%p,\"%s\")\n",r,i), SECURITY_MANAGER_SUCCESS)
 
#define security_manager_app_inst_req_set_app_id(r,i) \
 (printf("security_manager_app_inst_req_set_app_id(%p,\"%s\")\n",r,i), SECURITY_MANAGER_SUCCESS)
 
#define security_manager_app_inst_req_add_privilege(r,p) \
 (printf("security_manager_app_inst_req_add_privilege(%p,\"%s\")\n",r,p), SECURITY_MANAGER_SUCCESS)

#define security_manager_app_inst_req_add_path(r,p,t) \
 (printf("security_manager_app_inst_req_add_path(%p,\"%s\",%d)\n",r,p,t), SECURITY_MANAGER_SUCCESS)

#define security_manager_app_install(r) \
 (printf("security_manager_app_install(%p)\n",r), SECURITY_MANAGER_SUCCESS)

#endif

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
	default: errno = 0; break;
	}
	return -1;
}

int secmgr_init(const char *pkgid, const char *appid)
{
	int rc;
	assert(request == NULL);
	rc = security_manager_app_inst_req_new(&request);
	if (rc == SECURITY_MANAGER_SUCCESS) {
		rc = security_manager_app_inst_req_set_pkg_id(request, pkgid);
		if (rc == SECURITY_MANAGER_SUCCESS)
			rc = security_manager_app_inst_req_set_app_id(request, appid);
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
	return retcode(rc);
}

int secmgr_permit(const char *permission)
{
	int rc;
	assert(request != NULL);
	rc = security_manager_app_inst_req_add_privilege(request, permission);
	return retcode(rc);
}

static int addpath(const char *pathname, enum app_install_path_type type)
{
	int rc;
	assert(request != NULL);
	rc = security_manager_app_inst_req_add_path(request, pathname, type);
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


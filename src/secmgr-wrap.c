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

#include "verbose.h"
#include "secmgr-wrap.h"

#if SIMULATE_SECURITY_MANAGER
#include "simulation/security-manager.h"
#else
#include <security-manager.h>
#endif

static security_manager_handle_t *sm_handle = NULL;

static int retcode(int rc)
{
	if(rc < 0) {
		errno = -rc;
		return -1;
	}
	return 0;
}

int secmgr_init(const char *id)
{
	int rc;
	assert(sm_handle == NULL);

	rc = security_manager_create(&sm_handle, NULL);

	if(rc < 0) {
		ERROR("security_manager_create failed");
		goto ret;
	}

	rc = security_manager_set_id(sm_handle, id);

	if(rc < 0) {
		ERROR("security_manager_set_id failed");
		goto error;
	}

	goto ret;

error:
	secmgr_cancel();
ret:
	return retcode(rc);
}

void secmgr_cancel()
{
	security_manager_destroy(sm_handle);
	sm_handle = NULL;
}

int secmgr_install()
{
	int rc;
	assert(sm_handle != NULL);
	rc = security_manager_install(sm_handle);
	if (rc < 0)
		ERROR("security_manager_install failed %d %s", -rc, strerror(-rc));
	return retcode(rc);
}

int secmgr_uninstall()
{
	int rc;
	assert(sm_handle != NULL);
	rc = security_manager_uninstall(sm_handle);
	if (rc < 0)
		ERROR("security_manager_uninstall failed");
	return retcode(rc);
}

int secmgr_permit(const char *permission)
{
	int rc;
	assert(sm_handle != NULL);
	rc = security_manager_add_permission(sm_handle, permission);
	if (rc < 0)
		ERROR("security_manager_add_permission %s failed", permission);
	return retcode(rc);
}

static int addpath(const char *pathname, const char *path_type)
{
	int rc;
	assert(sm_handle != NULL);
	rc = security_manager_add_path(sm_handle, pathname, path_type);
	if (rc < 0)
		ERROR("security_manager_add_path %s failed", pathname);
	return retcode(rc);
}

int secmgr_path_conf(const char *pathname){
	return addpath(pathname, "conf");
}

int secmgr_path_data(const char *pathname){
	return addpath(pathname, "data");
}

int secmgr_path_exec(const char *pathname){
	return addpath(pathname, "exec");
}

int secmgr_path_http(const char *pathname){
	return addpath(pathname, "http");
}

int secmgr_path_icon(const char *pathname){
	return addpath(pathname, "icon");
}

int secmgr_path_lib(const char *pathname){
	return addpath(pathname, "lib");
}

int secmgr_path_public(const char *pathname){
	return addpath(pathname, "public");
}

int secmgr_path_id(const char *pathname){
	return addpath(pathname, "id");
}


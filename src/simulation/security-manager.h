/*
 Copyright 2015, 2016, 2017 IoT.bzh

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

#define security_manager_app_uninstall(r) \
 (printf("security_manager_app_uninstall(%p)\n",r), SECURITY_MANAGER_SUCCESS)

#define security_manager_prepare_app(a) \
 (printf("security_manager_prepare_app(%s)\n",a), SECURITY_MANAGER_SUCCESS)


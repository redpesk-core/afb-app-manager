/*
 Copyright (C) 2015-2020 IoT.bzh

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


typedef struct security_manager security_manager_t;
typedef struct security_manager_handle security_manager_handle_t;

enum path_type {
    type_none,
    type_conf,
    type_data,
    type_exec,
    type_http,
    type_icon,
    type_id,
    type_lib,
    type_public,
    number_path_type
};

static int ptr_ = 0;

#define  security_manager_create(sm_handle, socketspec) \
 (*(sm_handle)=(void*)(intptr_t)(++ptr_), printf("security_manager_create(%p,%s)\n",*sm_handle, (char *)socketspec), 0)

#define  security_manager_destroy(sm_handle) \
 (printf("security_manager_destroy(%p)\n", sm_handle))

#define  security_manager_disconnect(sm_handle) \
 (printf("security_manager_disconnect(%p)\n", sm_handle),0)

#define  security_manager_set_id(sm_handle, id) \
 (printf("security_manager_set_id(%p,%s)\n", sm_handle, id),0)

#define  security_manager_add_path(sm_handle, path, path_type) \
 (printf("security_manager_add_path(%p,%s,%s)\n", sm_handle, path, path_type),0)

#define  security_manager_add_permission(sm_handle, permission) \
 (printf("security_manager_add_permission(%p,%s)\n", sm_handle, permission),0)

#define  security_manager_clean(sm_handle) \
 (printf("security_manager_clean(%p)\n", sm_handle),0)

#define  security_manager_install(sm_handle) \
 (printf("security_manager_install(%p)\n", sm_handle),0)

 #define  security_manager_uninstall(sm_handle) \
 (printf("security_manager_uninstall(%p)\n", sm_handle),0)


/*
 Copyright (C) 2015-2020 IoT.bzh Company

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


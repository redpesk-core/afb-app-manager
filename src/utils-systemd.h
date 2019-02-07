/*
 Copyright (C) 2017-2019 IoT.bzh

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

#pragma once

extern const char SysD_State_Inactive[];
extern const char SysD_State_Activating[];
extern const char SysD_State_Active[];
extern const char SysD_State_Deactivating[];
extern const char SysD_State_Reloading[];
extern const char SysD_State_Failed[];

struct sd_bus;
extern int systemd_get_bus(int isuser, struct sd_bus **ret);
extern void systemd_set_bus(int isuser, struct sd_bus *bus);

extern int systemd_get_units_dir(char *path, size_t pathlen, int isuser);
extern int systemd_get_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext);
extern int systemd_get_wants_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext);
extern int systemd_get_wants_target(char *path, size_t pathlen, const char *unit, const char *uext);
extern int systemd_daemon_reload(int isuser);

extern char *systemd_unit_dpath_by_name(int isuser, const char *name, int load);
extern char *systemd_unit_dpath_by_pid(int isuser, unsigned pid);

extern int systemd_unit_start_dpath(int isuser, const char *dpath);
extern int systemd_unit_restart_dpath(int isuser, const char *dpath);
extern int systemd_unit_stop_dpath(int isuser, const char *dpath);

extern int systemd_unit_start_name(int isuser, const char *name);
extern int systemd_unit_restart_name(int isuser, const char *name);
extern int systemd_unit_stop_name(int isuser, const char *name);
extern int systemd_unit_stop_pid(int isuser, unsigned pid);

extern int systemd_unit_pid_of_dpath(int isuser, const char *dpath);
extern const char *systemd_unit_state_of_dpath(int isuser, const char *dpath);

extern int systemd_unit_list(int isuser, int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure);
extern int systemd_unit_list_all(int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure);


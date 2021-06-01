/*
 Copyright (C) 2015-2020 IoT.bzh Company

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

#pragma once

enum SysD_State {
    SysD_State_INVALID,
    SysD_State_Inactive,
    SysD_State_Activating,
    SysD_State_Active,
    SysD_State_Deactivating,
    SysD_State_Reloading,
    SysD_State_Failed
};

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

extern int systemd_unit_start_dpath(int isuser, const char *dpath, char **job);
extern int systemd_unit_restart_dpath(int isuser, const char *dpath, char **job);
extern int systemd_unit_stop_dpath(int isuser, const char *dpath, char **job);

extern int systemd_unit_start_name(int isuser, const char *name, char **job);
extern int systemd_unit_restart_name(int isuser, const char *name, char **job);
extern int systemd_unit_stop_name(int isuser, const char *name, char **job);
extern int systemd_unit_stop_pid(int isuser, unsigned pid, char **job);

extern int systemd_unit_pid_of_dpath(int isuser, const char *dpath);
extern enum SysD_State systemd_unit_state_of_dpath(int isuser, const char *dpath);

extern int systemd_unit_list(int isuser, int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure);
extern int systemd_unit_list_all(int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure);

extern const char *systemd_name_of_state(enum SysD_State state);
extern enum SysD_State systemd_state_of_name(const char *name);

extern int systemd_job_is_pending(int isuser, const char *job);


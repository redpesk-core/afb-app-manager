/*
 Copyright (C) 2015-2024 IoT.bzh Company

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

/**
 * Structure for listing units, passed to callbacks
 */
struct SysD_ListUnitItem
{
	/** The primary unit name as string */
	const char *name;

	/** The human readable description string */
	const char *description;

	/** The load state (i.e. whether the unit file has been loaded successfully) */
	const char *load_state;

	/** The active state (i.e. whether the unit is currently started or not) */
	const char *active_state;

	/** The sub state (a more fine-grained version of the active state that is specific to the unit type, which the active state is not) */
	const char *sub_state;

	/** A unit that is being followed in its state by this unit, if there is any, otherwise the empty string. */
	const char *ignored;

	/** The unit object path */
	const char *opath;

	/** If there is a job queued for the job unit, the numeric job id, 0 otherwise */
	unsigned job_id;

	/** The job type as string */
	const char *job_type;

	/** The job object path */
	const char *job_opath;
};

struct sd_bus;
extern int systemd_get_bus(int isuser, struct sd_bus **ret);
extern void systemd_set_bus(int isuser, struct sd_bus *bus);

extern int systemd_get_afm_units_dir(char *path, size_t pathlen, int isuser);
extern int systemd_get_afm_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext);
extern int systemd_get_afm_wants_unit_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext);
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

/**
 * Retrieves the units of the given pattern and activates the callback for each of them.
 *
 * @param isuser   is units of systemd user (not zero) or system (zero)?
 * @param pattern  pattern of the units to find, see systemctl manual
 * @param callback function to be called for each unit found
 * @param closure  closure to give to the callback
 *
 * @return the count of unit found on success or if error -1
 */
extern int systemd_list_unit_pattern(int isuser, const char *pattern, void (*callback)(void*,struct SysD_ListUnitItem*), void *closure);

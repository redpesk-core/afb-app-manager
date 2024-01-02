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
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if !NO_LIBSYSTEMD
# include <systemd/sd-bus.h>
# include <systemd/sd-bus-protocol.h>
#else
  struct sd_bus;
  struct sd_bus_message;
  typedef struct { const char *name; const char *message; } sd_bus_error;
# define sd_bus_unref(...)                ((void)0)
# define sd_bus_default_user(p)           ((*(p)=NULL),(-ENOTSUP))
# define sd_bus_default_system(p)         ((*(p)=NULL),(-ENOTSUP))
# define sd_bus_call_method(...)          (-ENOTSUP)
# define SD_BUS_ERROR_NULL                {NULL,NULL}
# define sd_bus_message_read_basic(...)   (-ENOTSUP)
# define sd_bus_message_unref(...)        (NULL)
# define sd_bus_get_property_string(...)  (-ENOTSUP)
# define sd_bus_get_property_trivial(...) (-ENOTSUP)
#endif

#include "utils-systemd.h"

#if !defined(AFM_UNITS_ROOT)
# define AFM_UNITS_ROOT "/usr/local/lib/systemd"
#endif

static const char sdb_destination[] = "org.freedesktop.systemd1";
static const char sdb_path[]        = "/org/freedesktop/systemd1";

static const char sdbi_job[]     = "org.freedesktop.systemd1.Job";
static const char sdbi_manager[] = "org.freedesktop.systemd1.Manager";
static const char sdbi_service[] = "org.freedesktop.systemd1.Service";
static const char sdbi_unit[]    = "org.freedesktop.systemd1.Unit";

static const char sdbp_active_state[]  = "ActiveState";
static const char sdbp_exec_main_pid[] = "ExecMainPID";

static const char sdbm_get_unit[]          = "GetUnit";
static const char sdbm_get_unit_by_pid[]   = "GetUnitByPID";
static const char sdbm_list_unit_pattern[] = "ListUnitsByPatterns";
static const char sdbm_load_unit[]         = "LoadUnit";
static const char sdbm_reload[]            = "Reload";
static const char sdbm_restart[]           = "Restart";
static const char sdbm_restart_unit[]      = "RestartUnit";
static const char sdbp_state[]             = "State";
static const char sdbm_start[]             = "Start";
static const char sdbm_start_unit[]        = "StartUnit";
static const char sdbm_stop[]              = "Stop";
static const char sdbm_stop_unit[]         = "StopUnit";

static const char *sds_state_names[] = {
	NULL,
	"inactive",
	"activating",
	"active",
	"deactivating",
	"reloading",
	"failed"
};

static struct sd_bus *sysbus;
static struct sd_bus *usrbus;

/*
 * Translate systemd errors to errno errors
 */
static int seterrno(int value)
{
	errno = value;
	return -1;
}

static int sderr2errno(int rc)
{
	return rc < 0 ? seterrno(-rc) : rc;
}

/*
static int errno2sderr(int rc)
{
	return rc < 0 ? -errno : rc;
}
*/

/*
 * Returns in 'ret' either the system bus (if isuser==0)
 * or the user bus (if isuser!=0).
 * Returns 0 in case of success or -1 in case of error
 */
int systemd_get_bus(int isuser, struct sd_bus **ret)
{
	int rc;
	struct sd_bus *bus;

	bus = isuser ? usrbus : sysbus;
	if (bus)
		*ret = bus;
	else if (isuser) {
		rc = sd_bus_default_user(ret);
		if (rc < 0)
			goto error;
		usrbus = *ret;
	} else {
		rc = sd_bus_default_system(ret);
		if (rc < 0)
			goto error;
		sysbus = *ret;
	}
	return 0;
error:
	return sderr2errno(rc);
}

void systemd_set_bus(int isuser, struct sd_bus *bus)
{
	struct sd_bus **target = isuser ? &usrbus : &sysbus;
	if (*target)
		sd_bus_unref(*target);
	*target = bus;
}

#if 0
/********************************************************************
 * routines for escaping unit names to compute dbus path of units
 *******************************************************************/
/*
 * Should the char 'c' be escaped?
 */
static inline int should_escape_for_path(char c)
{
	if (c >= 'A') {
		return c <= (c >= 'a' ? 'z' : 'Z');
	} else {
		return c >= '0' && c <= '9';
	}
}

/*
 * ascii char for the hexadecimal digit 'x'
 */
static inline char d2h(int x)
{
	return (char)(x + (x > 9 ? ('a' - 10) : '0'));
}

/*
 * escapes in 'path' of 'pathlen' the 'unit'
 * returns 0 in case of success or -1 in case of error
 */
static int unit_escape_for_path(char *path, size_t pathlen, const char *unit)
{
	size_t r, w;
	char c;

	c = unit[r = w = 0];
	while (c) {
		if (should_escape_for_path(c)) {
			if (w + 2 >= pathlen)
				goto toolong;
			path[w++] = '_';
			path[w++] = d2h((c >> 4) & 15);;
			path[w++] = d2h(c & 15);
		} else {
			if (w >= pathlen)
				goto toolong;
			path[w++] = c;
		}
		c = unit[++r];
	}
	if (w >= pathlen)
		goto toolong;
	path[w] = 0;
	return 0;
toolong:
	return seterrno(ENAMETOOLONG);
}
#endif

/********************************************************************
 * Routines for getting paths
 *******************************************************************/

static char *get_dpath(struct sd_bus_message *msg)
{
	int rc;
	const char *reply;
	char *result;

	rc = sd_bus_message_read_basic(msg, 'o', &reply);
	rc = sderr2errno(rc);
	if (rc < 0)
		result = NULL;
	else {
		result = strdup(reply);
		if (!result)
			errno = ENOMEM;
	}
	sd_bus_message_unref(msg);
	return result;
}

static char *get_unit_dpath(struct sd_bus *bus, const char *unit, int load)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, load ? sdbm_load_unit : sdbm_get_unit, &err, &ret, "s", unit);
	if (rc < 0)
		goto error;

	return get_dpath(ret);
error:
	sd_bus_message_unref(ret);
	return NULL;
}

static char *get_unit_dpath_by_pid(struct sd_bus *bus, unsigned pid)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_get_unit_by_pid, &err, &ret, "u", pid);
	if (rc < 0)
		goto error;

	return get_dpath(ret);
error:
	sd_bus_message_unref(ret);
	return NULL;
}

static int unit_pid(struct sd_bus *bus, const char *dpath)
{
	int rc;
	unsigned u = 0;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_get_property_trivial(bus, sdb_destination, dpath, sdbi_service, sdbp_exec_main_pid, &err, 'u', &u);
	return rc < 0 ? rc : (int)u;
}

static enum SysD_State unit_state(struct sd_bus *bus, const char *dpath)
{
	int rc;
	char *st;
	enum SysD_State resu;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	resu = SysD_State_INVALID;
	rc = sd_bus_get_property_string(bus, sdb_destination, dpath, sdbi_unit, sdbp_active_state, &err, &st);
	if (rc < 0) {
		errno = -rc;
	} else {
		resu = systemd_state_of_name(st);
		if (resu == SysD_State_INVALID)
			errno = EBADMSG;
		free(st);
	}
	return resu;
}

static int get_job_from_reply(struct sd_bus_message *reply, char **job)
{
	int rc;
	char *obj;

	rc = sd_bus_message_read_basic(reply, 'o', &obj);
	if (!job)
		rc = 0;
	else {
		if (rc < 0)
			obj = NULL;
		else {
			obj = strdup(obj);
			if (obj == NULL)
				rc = -ENOMEM;
		}
		*job = obj;
	}
	return rc;
}

static int unit_start(struct sd_bus *bus, const char *dpath, char **job)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, dpath, sdbi_unit, sdbm_start, &err, &ret, "s", "replace");
	if (rc >= 0)
		rc = get_job_from_reply(ret, job);
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_restart(struct sd_bus *bus, const char *dpath, char **job)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, dpath, sdbi_unit, sdbm_restart, &err, &ret, "s", "replace");
	if (rc >= 0)
		rc = get_job_from_reply(ret, job);
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_stop(struct sd_bus *bus, const char *dpath, char **job)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, dpath, sdbi_unit, sdbm_stop, &err, &ret, "s", "replace");
	if (rc >= 0)
		rc = get_job_from_reply(ret, job);
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_start_name(struct sd_bus *bus, const char *name, char **job)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_start_unit, &err, &ret, "ss", name, "replace");
	if (rc >= 0)
		rc = get_job_from_reply(ret, job);
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_restart_name(struct sd_bus *bus, const char *name, char **job)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_restart_unit, &err, &ret, "ss", name, "replace");
	if (rc >= 0)
		rc = get_job_from_reply(ret, job);
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_stop_name(struct sd_bus *bus, const char *name, char **job)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_stop_unit, &err, &ret, "ss", name, "replace");
	if (rc >= 0)
		rc = get_job_from_reply(ret, job);
	sd_bus_message_unref(ret);
	return rc;
}

static int job_is_pending(struct sd_bus *bus, const char *job)
{
	int rc;
	char *st;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_get_property_string(bus, sdb_destination, job, sdbi_job, sdbp_state, &err, &st);
	if (rc < 0)
		return 0;
	free(st);
	return 1;
}

/********************************************************************
 *
 *******************************************************************/

static int check_snprintf_result(int rc, size_t buflen)
{
	return (rc >= 0 && (size_t)rc >= buflen) ? seterrno(ENAMETOOLONG) : rc;
}

int systemd_get_afm_units_dir(char *path, size_t pathlen, int isuser)
{
	int rc = snprintf(path, pathlen, "%s/%s",
			AFM_UNITS_ROOT,
			isuser ? "user" : "system");

	return check_snprintf_result(rc, pathlen);
}

int systemd_get_afm_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.%s",
			AFM_UNITS_ROOT,
			isuser ? "user" : "system",
			unit,
			uext);

	return check_snprintf_result(rc, pathlen);
}

int systemd_get_afm_wants_unit_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.wants/%s.%s",
			AFM_UNITS_ROOT,
			isuser ? "user" : "system",
			wanter,
			unit,
			uext);

	return check_snprintf_result(rc, pathlen);
}

int systemd_get_wants_target(char *path, size_t pathlen, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "../%s.%s", unit, uext);

	return check_snprintf_result(rc, pathlen);
}

int systemd_daemon_reload(int isuser)
{
	int rc;
	struct sd_bus *bus;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0) {
		/* TODO: asynchronous bind... */
		/* TODO: more diagnostic... */
		rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_reload, &err, &ret, NULL);
		sd_bus_message_unref(ret);
	}
	return rc;
}

int systemd_unit_list(int isuser, int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure)
{
	DIR *dir;
	char path[PATH_MAX + 1];
	struct dirent *dent;
	int rc, isfile;
	size_t offset, len;
	struct stat st;

	/* get the path */
	rc = systemd_get_afm_units_dir(path, sizeof path - 1, isuser);
	if (rc < 0)
		return rc;
	offset = (size_t)rc;

	/* open the directory */
	dir = opendir(path);
	if (!dir)
		return -1;

	/* prepare path */
	path[offset++] = '/';

	/* read the directory */
	for(;;) {
		/* get next entry */
		errno = 0;
		dent = readdir(dir);
		if (dent == NULL) {
			/* end or error */
			rc = (!errno) - 1;
			break;
		}

		/* is a file? */
		if (dent->d_type == DT_REG)
			isfile = 1;
		else if (dent->d_type != DT_UNKNOWN)
			isfile = 0;
		else {
			rc = fstatat(dirfd(dir), dent->d_name, &st, AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT);
			if (rc < 0)
				break;
			isfile = S_ISREG(st.st_mode);
		}

		/* calls the callback if is a file */
		if (isfile) {
			len = strlen(dent->d_name);
			if (offset + len >= sizeof path) {
				rc = seterrno(ENAMETOOLONG);
				break;
			}
			memcpy(&path[offset], dent->d_name, 1 + len);
			rc = callback(closure, &path[offset], path, isuser);
			if (rc)
				break;
		}
	}
	closedir(dir);
	return rc;
}

int systemd_unit_list_all(int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure)
{
	return systemd_unit_list(1, callback, closure) ? : systemd_unit_list(0, callback, closure);
}

char *systemd_unit_dpath_by_name(int isuser, const char *name, int load)
{
	struct sd_bus *bus;

	return systemd_get_bus(isuser, &bus) < 0 ? NULL : get_unit_dpath(bus, name, load);
}

char *systemd_unit_dpath_by_pid(int isuser, unsigned pid)
{
	struct sd_bus *bus;

	return systemd_get_bus(isuser, &bus) < 0 ? NULL : get_unit_dpath_by_pid(bus, pid);
}

int systemd_unit_start_dpath(int isuser, const char *dpath, char **job)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? rc : unit_start(bus, dpath, job);
}

int systemd_unit_restart_dpath(int isuser, const char *dpath, char **job)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? rc : unit_restart(bus, dpath, job);
}

int systemd_unit_stop_dpath(int isuser, const char *dpath, char **job)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? rc : unit_stop(bus, dpath, job);
}

int systemd_unit_start_name(int isuser, const char *name, char **job)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0)
		rc = unit_start_name(bus, name, job);
	return rc;
}

int systemd_unit_restart_name(int isuser, const char *name, char **job)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0)
		rc = unit_restart_name(bus, name, job);
	return rc;
}

int systemd_unit_stop_name(int isuser, const char *name, char **job)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0)
		rc = unit_stop_name(bus, name, job);
	return rc;
}

int systemd_unit_stop_pid(int isuser, unsigned pid, char **job)
{
	int rc;
	struct sd_bus *bus;
	char *dpath;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0) {
		dpath = get_unit_dpath_by_pid(bus, pid);
		if (!dpath)
			rc = -1;
		else {
			rc = unit_stop(bus, dpath, job);
			free(dpath);
		}
	}
	return rc;
}

int systemd_unit_pid_of_dpath(int isuser, const char *dpath)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? rc : unit_pid(bus, dpath);
}

enum SysD_State systemd_unit_state_of_dpath(int isuser, const char *dpath)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? SysD_State_INVALID : unit_state(bus, dpath);
}

const char *systemd_name_of_state(enum SysD_State state)
{
	return sds_state_names[state >= 0 && state < sizeof sds_state_names / sizeof *sds_state_names ? state : SysD_State_INVALID];
}

enum SysD_State systemd_state_of_name(const char *name)
{
	enum SysD_State resu = SysD_State_INVALID;
	switch (name[0]) {
	case 'a':
		if (!strcmp(name, sds_state_names[SysD_State_Active]))
			resu = SysD_State_Active;
		else if (!strcmp(name, sds_state_names[SysD_State_Activating]))
			resu = SysD_State_Activating;
		break;
	case 'd':
		if (!strcmp(name, sds_state_names[SysD_State_Deactivating]))
			resu = SysD_State_Deactivating;
		break;
	case 'f':
		if (!strcmp(name, sds_state_names[SysD_State_Failed]))
			resu = SysD_State_Failed;
		break;
	case 'i':
		if (!strcmp(name, sds_state_names[SysD_State_Inactive]))
			resu = SysD_State_Inactive;
		break;
	case 'r':
		if (!strcmp(name, sds_state_names[SysD_State_Reloading]))
			resu = SysD_State_Reloading;
		break;
	default:
		break;
	}
	return resu;
}

int systemd_job_is_pending(int isuser, const char *job)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0)
		rc = job_is_pending(bus, job);
	return rc;
}

int systemd_list_unit_pattern(int isuser, const char *pattern, void (*callback)(void*,struct SysD_ListUnitItem*), void *closure)
{
	int rc, count;
	struct sd_bus *bus;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	struct SysD_ListUnitItem lui;

	/* connect to the bus */
	count = 0;
	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0) {
		/* call the method ListUnitsByPatterns */
		rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_list_unit_pattern, &err, &ret,
						"asas", (unsigned)0, (unsigned)1, pattern);
		if (rc >= 0) {
			/* got a valid result, iterate items of the array of result */
			rc = sd_bus_message_enter_container(ret, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
			while(rc >= 0 && !sd_bus_message_at_end(ret, 0)) {
				/* read the item */
				rc = sd_bus_message_enter_container(ret, SD_BUS_TYPE_STRUCT, "ssssssouso");
				if (rc >= 0) {
					rc = sd_bus_message_read(ret, "ssssssouso",
						&lui.name,
						&lui.description,
						&lui.load_state,
						&lui.active_state,
						&lui.sub_state,
						&lui.ignored,
						&lui.opath,
						&lui.job_id,
						&lui.job_type,
						&lui.job_opath
						);
					/* activate callback for the item */
					if (rc >= 0) {
						callback(closure, &lui);
						count++;
					}
					sd_bus_message_exit_container(ret);
				}
			}
		}
		sd_bus_message_unref(ret);
	}
	return rc < 0 ? rc : count;
}

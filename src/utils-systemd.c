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

#ifndef NO_LIBSYSTEMD
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

#if !defined(SYSTEMD_UNITS_ROOT)
# define SYSTEMD_UNITS_ROOT "/usr/local/lib/systemd"
#endif

static const char sdb_path[] = "/org/freedesktop/systemd1";
static const char sdb_destination[] = "org.freedesktop.systemd1";
static const char sdbi_manager[] = "org.freedesktop.systemd1.Manager";
static const char sdbi_unit[] = "org.freedesktop.systemd1.Unit";
static const char sdbi_service[] = "org.freedesktop.systemd1.Service";
static const char sdbm_reload[] = "Reload";
static const char sdbm_start_unit[] = "StartUnit";
static const char sdbm_restart_unit[] = "RestartUnit";
static const char sdbm_stop_unit[] = "StopUnit";
static const char sdbm_start[] = "Start";
static const char sdbm_restart[] = "Restart";
static const char sdbm_stop[] = "Stop";
static const char sdbm_get_unit[] = "GetUnit";
static const char sdbm_get_unit_by_pid[] = "GetUnitByPID";
static const char sdbm_load_unit[] = "LoadUnit";
static const char sdbp_active_state[] = "ActiveState";
static const char sdbp_exec_main_pid[] = "ExecMainPID";

const char SysD_State_Inactive[] = "inactive";
const char SysD_State_Activating[] = "activating";
const char SysD_State_Active[] = "active";
const char SysD_State_Deactivating[] = "deactivating";
const char SysD_State_Reloading[] = "reloading";
const char SysD_State_Failed[] = "failed";

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

static int errno2sderr(int rc)
{
	return rc < 0 ? -errno : rc;
}

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

static const char *unit_state(struct sd_bus *bus, const char *dpath)
{
	int rc;
	char *st;
	const char *resu;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_get_property_string(bus, sdb_destination, dpath, sdbi_unit, sdbp_active_state, &err, &st);
	if (rc < 0) {
		errno = -rc;
		resu = NULL;
	} else {
		if (!strcmp(st, SysD_State_Active))
			resu = SysD_State_Active;
		else if (!strcmp(st, SysD_State_Reloading))
			resu = SysD_State_Reloading;
		else if (!strcmp(st, SysD_State_Inactive))
			resu = SysD_State_Inactive;
		else if (!strcmp(st, SysD_State_Failed))
			resu = SysD_State_Failed;
		else if (!strcmp(st, SysD_State_Activating))
			resu = SysD_State_Activating;
		else if (!strcmp(st, SysD_State_Deactivating))
			resu = SysD_State_Deactivating;
		else {
			errno = EBADMSG;
			resu = NULL;
		}
		free(st);
	}
	return resu;
}

static int unit_start(struct sd_bus *bus, const char *dpath)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, dpath, sdbi_unit, sdbm_start, &err, &ret, "s", "replace");
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_restart(struct sd_bus *bus, const char *dpath)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, dpath, sdbi_unit, sdbm_restart, &err, &ret, "s", "replace");
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_stop(struct sd_bus *bus, const char *dpath)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, dpath, sdbi_unit, sdbm_stop, &err, &ret, "s", "replace");
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_start_name(struct sd_bus *bus, const char *name)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_start_unit, &err, &ret, "ss", name, "replace");
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_restart_name(struct sd_bus *bus, const char *name)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_restart_unit, &err, &ret, "ss", name, "replace");
	sd_bus_message_unref(ret);
	return rc;
}

static int unit_stop_name(struct sd_bus *bus, const char *name)
{
	int rc;
	struct sd_bus_message *ret = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;

	rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_stop_unit, &err, &ret, "ss", name, "replace");
	sd_bus_message_unref(ret);
	return rc;
}

/********************************************************************
 *
 *******************************************************************/

static int check_snprintf_result(int rc, size_t buflen)
{
	return (rc >= 0 && (size_t)rc >= buflen) ? seterrno(ENAMETOOLONG) : rc;
}

int systemd_get_units_dir(char *path, size_t pathlen, int isuser)
{
	int rc = snprintf(path, pathlen, "%s/%s",
			SYSTEMD_UNITS_ROOT,
			isuser ? "user" : "system");

	return check_snprintf_result(rc, pathlen);
}

int systemd_get_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.%s",
			SYSTEMD_UNITS_ROOT,
			isuser ? "user" : "system",
			unit,
			uext);

	return check_snprintf_result(rc, pathlen);
}

int systemd_get_wants_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.wants/%s.%s",
			SYSTEMD_UNITS_ROOT,
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
	rc = systemd_get_units_dir(path, sizeof path - 1, isuser);
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

int systemd_unit_start_dpath(int isuser, const char *dpath)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? rc : unit_start(bus, dpath);
}

int systemd_unit_restart_dpath(int isuser, const char *dpath)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? rc : unit_restart(bus, dpath);
}

int systemd_unit_stop_dpath(int isuser, const char *dpath)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? rc : unit_stop(bus, dpath);
}

int systemd_unit_start_name(int isuser, const char *name)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0)
		rc = unit_start_name(bus, name);
	return rc;
}

int systemd_unit_restart_name(int isuser, const char *name)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0)
		rc = unit_restart_name(bus, name);
	return rc;
}

int systemd_unit_stop_name(int isuser, const char *name)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	if (rc >= 0)
		rc = unit_stop_name(bus, name);
	return rc;
}

int systemd_unit_stop_pid(int isuser, unsigned pid)
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
			rc = unit_stop(bus, dpath);
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

const char *systemd_unit_state_of_dpath(int isuser, const char *dpath)
{
	int rc;
	struct sd_bus *bus;

	rc = systemd_get_bus(isuser, &bus);
	return rc < 0 ? NULL : unit_state(bus, dpath);
}


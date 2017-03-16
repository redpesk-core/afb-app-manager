/*
 Copyright 2017 IoT.bzh

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
# define sd_bus_default_user(p)   ((*(p)=NULL),(-ENOTSUP))
# define sd_bus_default_system(p) ((*(p)=NULL),(-ENOTSUP))
# define sd_bus_call_method(...)  (-ENOTSUP)
#endif

#include "utils-systemd.h"

#if !defined(SYSTEMD_UNITS_ROOT)
# define SYSTEMD_UNITS_ROOT "/usr/local/lib/systemd"
#endif

static const char sdb_path[] = "/org/freedesktop/systemd1";
static const char sdb_destination[] = "org.freedesktop.systemd1";
static const char sdbi_manager[] = "org.freedesktop.systemd1.Manager";
static const char sdbm_reload[] = "Reload";

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

/* Returns in 'ret' either the system bus (if isuser==0)
 * or the user bus (if isuser!=0).
 * Returns 0 in case of success or -1 in case of error
 */
static int get_bus(int isuser, struct sd_bus **ret)
{
	int rc;
	struct sd_bus *bus;

	bus = isuser ? usrbus : sysbus;
	if (bus) {
		*ret = bus;
		rc = 0;
	} else if (isuser) {
		rc = sd_bus_default_user(ret);
		if (!rc)
			usrbus = *ret;
	} else {
		rc = sd_bus_default_system(ret);
		if (!rc)
			sysbus = *ret;
	}
	return sderr2errno(rc);
}
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

	rc = get_bus(isuser, &bus);
	if (!rc) {
		/* TODO: asynchronous bind... */
		/* TODO: more diagnostic... */
		rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_reload, NULL, NULL, NULL);
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


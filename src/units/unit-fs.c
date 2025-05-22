/*
 Copyright (C) 2015-2025 IoT.bzh Company

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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "unit-fs.h"

#if !defined(AFM_UNITS_ROOT)
# define AFM_UNITS_ROOT "/usr/local/lib/systemd"
#endif

static const char *afm_units_root = AFM_UNITS_ROOT;

/********************************************************************
 *
 *******************************************************************/

static int check_snprintf_result(int rc, size_t buflen)
{
	if (rc >= 0 && (size_t)rc < buflen)
		return (int)rc;
	errno = ENAMETOOLONG;
	return -1;
}

const char *units_fs_set_root_dir(const char *dir)
{
	const char *prev = afm_units_root;
	if (dir)
		afm_units_root = dir;
	return prev;
}

int units_fs_get_afm_units_dir(char *path, size_t pathlen, int isuser)
{
	int rc = snprintf(path, pathlen, "%s/%s",
			afm_units_root,
			isuser ? "user" : "system");

	return check_snprintf_result(rc, pathlen);
}

int units_fs_get_afm_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.%s",
			afm_units_root,
			isuser ? "user" : "system",
			unit,
			uext);

	return check_snprintf_result(rc, pathlen);
}

int units_fs_get_afm_wants_unit_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.wants/%s.%s",
			afm_units_root,
			isuser ? "user" : "system",
			wanter,
			unit,
			uext);

	return check_snprintf_result(rc, pathlen);
}

int units_fs_get_wants_target(char *path, size_t pathlen, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "../%s.%s", unit, uext);

	return check_snprintf_result(rc, pathlen);
}


int units_fs_list(int isuser, int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure)
{
	DIR *dir;
	char path[PATH_MAX + 1];
	struct dirent *dent;
	int rc, isfile;
	size_t offset, len;
	struct stat st;

	/* get the path */
	rc = units_fs_get_afm_units_dir(path, sizeof path - 1, isuser);
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
				errno = ENAMETOOLONG;
				rc = -1;
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

int units_fs_list_all(int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure)
{
	return units_fs_list(1, callback, closure) ? : units_fs_list(0, callback, closure);
}


/*
 Copyright (C) 2015-2023 IoT.bzh Company

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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <rp-utils/rp-verbose.h>
#include "wgtpkg-workdir.h"
#include "utils-dir.h"

static const mode_t dirmode = 0755;
char workdir[PATH_MAX] = { 0, };
int workdirfd = -1;

/* removes the working directory */
void remove_workdir()
{
	assert(workdirfd >= 0);
	remove_directory_content_fd(workdirfd);
	close(workdirfd);
	workdirfd = -1;
	rmdir(workdir);
	workdir[0] = 0;
}

static void put_workdir(int fd, const char *name, size_t length)
{
	/* close the previous directory if any */
	if (workdirfd >= 0)
		close(workdirfd);

	/* copy the name and the fd if existing */
	if (fd < 0) {
		workdir[0] = '.';
		workdir[1] = 0;
		workdirfd = AT_FDCWD;
	} else {

		assert(length < sizeof workdir);
		memcpy(workdir, name, 1 + length);
		workdirfd = fd;
	}
}

int set_workdir(const char *name, int create)
{
	int rc, dirfd;
	size_t length;

	/* check the length */
	length = strlen(name);
	if (length >= sizeof workdir) {
		RP_ERROR("workdir name too long");
		errno = EINVAL;
		return -1;
	}

	/* check if . */
	if (length == 1 && name[0] == '.') {
		put_workdir(AT_FDCWD, name, length);
		return 0;
	}

	/* opens the directory */
	dirfd = openat(AT_FDCWD, name, O_PATH|O_DIRECTORY|O_RDONLY);
	if (dirfd < 0) {
		if (errno != ENOENT) {
			RP_ERROR("error while opening workdir %s: %m", name);
			return -1;
		}
		if (!create) {
			RP_ERROR("workdir %s doesn't exist", name);
			return -1;
		}
		rc = mkdirat(AT_FDCWD, name, dirmode);
		if (rc) {
			RP_ERROR("can't create workdir %s", name);
			return -1;
		}
		dirfd = openat(AT_FDCWD, name, O_PATH|O_DIRECTORY|O_RDONLY);
		if (dirfd < 0) {
			RP_ERROR("can't open workdir %s", name);
			return -1;
		}
	}

	/* record new workdir */
	put_workdir(dirfd, name, length);
	return 0;
}

int make_workdir(const char *root, const char *prefix, int reuse)
{
	int i, n, r, l;

	put_workdir(AT_FDCWD, ".", 1);

	n = snprintf(workdir, sizeof workdir, "%s/%s", root, prefix);
	if (n >= (int)sizeof workdir) {
		RP_ERROR("workdir prefix too long");
		errno = EINVAL;
		return -1;
	}
	r = (int)(sizeof workdir) - n;

	/* create a temporary directory */
	for (i = 0 ; ; i++) {
		if (i == INT_MAX) {
			RP_ERROR("exhaustion of workdirs");
			return -1;
		}
		l = snprintf(workdir + n, (unsigned)r, "%d", i);
		if (l >= r) {
			RP_ERROR("computed workdir too long");
			errno = EINVAL;
			return -1;
		}
		if (!mkdirat(AT_FDCWD, workdir, dirmode))
			break;
		if (errno != EEXIST) {
			RP_ERROR("error in creation of workdir %s: %m", workdir);
			return -1;
		}
		if (reuse)
			break;
	}
	workdirfd = openat(AT_FDCWD, workdir, O_RDONLY|O_DIRECTORY);
	if (workdirfd < 0) {
		RP_ERROR("error in onnection to workdir %s: %m", workdir);
		rmdir(workdir);
		return -1;
	}

	return 0;
}

int move_workdir(const char *dest, int parents, int force)
{
	int rc;
	size_t len;
	struct stat s;
	char *copy;
	const char *iter;

	/* check length */
	if (strlen(dest) >= sizeof workdir) {
		RP_ERROR("destination dirname too long");
		errno = EINVAL;
		return -1;
	}

	/* if an existing directory exist remove it if force */
	rc = stat(dest, &s);
	if (rc == 0) {
		if (!S_ISDIR(s.st_mode)) {
			RP_ERROR("in move_workdir, can't overwrite regular file %s", dest);
			errno = EEXIST;
			return -1;
		}
		if (!force) {
			RP_ERROR("in move_workdir, can't overwrite regular file %s", dest);
			errno = EEXIST;
			return -1;
		}
		rc = remove_directory_content(dest);
		if (rc) {
			RP_ERROR("in move_workdir, can't clean dir %s", dest);
			return rc;
		}
		rc = rmdir(dest);
		if (rc) {
			RP_ERROR("in move_workdir, can't remove dir %s", dest);
			return rc;
		}
	} else {
		/* check parent of dest */
		iter = strrchr(dest, '/');
		len = iter ? (size_t)(iter - dest) : 0;
		if (len) {
			/* is parent existing? */
			copy = strndupa(dest, len);
			rc = stat(copy, &s);
			if (!rc) {
				/* found an entry */
				if (!S_ISDIR(s.st_mode)) {
					RP_ERROR("in move_workdir, '%s' isn't a directory", copy);
					errno = ENOTDIR;
					return -1;
				}
			} else if (!parents) {
				/* parent entry not found but not allowed to create it */
				RP_ERROR("in move_workdir, parent directory '%s' not found: %m", copy);
				return -1;
			} else if (create_directory(copy, dirmode, 1)) {
				RP_ERROR("in move_workdir, creation of directory %s failed: %m", copy);
				return -1;
			}
		}
	}

	/* try to rename now */
	close(workdirfd);
	workdirfd = -1;
	rc = renameat(AT_FDCWD, workdir, AT_FDCWD, dest);
	if (rc) {
		RP_ERROR("in move_workdir, renameat failed %s -> %s: %m", workdir, dest);
		return -1;
	}

	return set_workdir(dest, 0);
}


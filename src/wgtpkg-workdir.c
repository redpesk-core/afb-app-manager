/*
 Copyright 2015 IoT.bzh

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

#include "verbose.h"
#include "wgtpkg-workdir.h"
#include "utils-dir.h"

static const int dirmode = 0755;
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

static int set_real_workdir(const char *name, int create)
{
	int rc, dirfd;
	size_t length;

	/* check the length */
	length = strlen(name);
	if (length >= sizeof workdir) {
		ERROR("workdir name too long");
		errno = EINVAL;
		return -1;
	}

	/* opens the directory */
	dirfd = openat(AT_FDCWD, name, O_DIRECTORY|O_RDONLY);
	if (dirfd < 0) {
		if (!create || errno != ENOENT) {
			ERROR("no workdir %s", name);
			return -1;
		}
		rc = mkdir(name, dirmode);
		if (rc) {
			ERROR("can't create workdir %s", name);
			return -1;
		}
		dirfd = open(name, O_PATH|O_DIRECTORY);
		if (dirfd < 0) {
			ERROR("can't open workdir %s", name);
			return -1;
		}
	}

	/* close the previous directory if any */
	if (workdirfd >= 0)
		close(workdirfd);
	workdirfd = dirfd;
	memcpy(workdir, name, 1+length);
	return 0;
}

int set_workdir(const char *name, int create)
{
	char *rp;
	int rc;

	if (name[0] == '/')
		return set_real_workdir(name, create);

	rp = realpath(name, NULL);
	if (!rp) {
		ERROR("realpath failed for %s", name);
		return -1;
	}
	rc = set_real_workdir(rp, create);
	free(rp);
	return rc;
}

static int make_real_workdir_base(const char *root, const char *prefix, int reuse)
{
	int i, n, r, l;

	n = snprintf(workdir, sizeof workdir, "%s/%s", root, prefix);
	if (n >= sizeof workdir) {
		ERROR("workdir prefix too long");
		errno = EINVAL;
		return -1;
	}
	r = (int)(sizeof workdir) - n;

	if (workdirfd >= 0)
		close(workdirfd);
	workdirfd = -1;

	/* create a temporary directory */
	for (i = 0 ; ; i++) {
		if (i == INT_MAX) {
			ERROR("exhaustion of workdirs");
			return -1;
		}
		l = snprintf(workdir + n, r, "%d", i);
		if (l >= r) {
			ERROR("computed workdir too long");
			errno = EINVAL;
			return -1;
		}
		if (!mkdir(workdir, dirmode))
			break;
		if (errno != EEXIST) {
			ERROR("error in creation of workdir %s: %m", workdir);
			return -1;
		}
		if (reuse)
			break;
	}
	workdirfd = openat(AT_FDCWD, workdir, O_RDONLY|O_DIRECTORY);
	if (workdirfd < 0) {
		ERROR("error in onnection to workdir %s: %m", workdir);
		rmdir(workdir);
		return -1;
	}

	return 0;
}

int make_workdir_base(const char *root, const char *prefix, int reuse)
{
	char *rp;
	int rc;

	if (root[0] == '/')
		return make_real_workdir_base(root, prefix, reuse);

	rp = realpath(root, NULL);
	if (!rp) {
		ERROR("realpath failed for %s", root);
		return -1;
	}
	rc = make_real_workdir_base(rp, prefix, reuse);
	free(rp);
	return rc;
}

int make_workdir(int reuse)
{
	return make_workdir_base(".", "PACK", reuse);
}

static int move_real_workdir(const char *dest, int parents, int force)
{
	int rc, len;
	struct stat s;
	char *copy;
	const char *iter;

	/* check length */
	if (strlen(dest) >= sizeof workdir) {
		ERROR("destination dirname too long");
		errno = EINVAL;
		return -1;
	}

	/* if an existing directory exist remove it if force */
	rc = stat(dest, &s);
	if (rc == 0) {
		if (!S_ISDIR(s.st_mode)) {
			ERROR("in move_real_workdir, can't overwrite regular file %s", dest);
			errno = EEXIST;
			return -1;
		}
		if (!force) {
			ERROR("in move_real_workdir, can't overwrite regular file %s", dest);
			errno = EEXIST;
			return -1;
		}
		rc = remove_directory_content(dest);
		if (rc) {
			ERROR("in move_real_workdir, can't clean dir %s", dest);
			return rc;
		}
		rc = rmdir(dest);
		if (rc) {
			ERROR("in move_real_workdir, can't remove dir %s", dest);
			return rc;
		}
	} else {
		/* check parent of dest */
		iter = strrchr(dest, '/');
		len = iter ? iter - dest : 0;
		if (len) {
			/* is parent existing? */
			copy = strndupa(dest, len);
			rc = stat(copy, &s);
			if (!rc) {
				/* found an entry */
				if (!S_ISDIR(s.st_mode)) {
					ERROR("in move_real_workdir, '%s' isn't a directory", copy);
					errno = ENOTDIR;
					return -1;
				}
			} else if (!parents) {
				/* parent entry not found but not allowed to create it */
				ERROR("in move_real_workdir, parent directory '%s' not found: %m", copy);
				return -1;
			} else if (create_directory(copy, dirmode, 1)) {
				ERROR("in move_real_workdir, creation of directory %s failed: %m", copy);
				return -1;
			}
		}
	}

	/* try to rename now */
	close(workdirfd);
	workdirfd = -1;
	rc = renameat(AT_FDCWD, workdir, AT_FDCWD, dest);
	if (rc) {
		ERROR("in move_real_workdir, renameat failed %s -> %s: %m", workdir, dest);
		return -1;
	}

	return set_real_workdir(dest, 0);
}

int move_workdir(const char *dest, int parents, int force)
{
	char *rp;
	int rc;

	if (dest[0] == '/')
		return move_real_workdir(dest, parents, force);

	rp = realpath(dest, NULL);
	if (!rp) {
		ERROR("realpath failed for %s", dest);
		return -1;
	}
	rc = move_real_workdir(rp, parents, force);
	free(rp);
	return rc;
}


/*
 Copyright 2015 IoT.bzh

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
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "wgtpkg.h"

static int mode = 0700;
static char workdir[PATH_MAX];

/* removes recursively the content of a directory */
static int clean_dirfd(int dirfd)
{
	int cr, fd;
	DIR *dir;
	struct dirent *ent;
	struct {
		struct dirent entry;
		char spare[PATH_MAX];
	} entry;

	dir = fdopendir(dirfd);
	if (dir == NULL) {
		syslog(LOG_ERR, "opendir failed in clean_dirfd");
		return -1;
	}

	cr = -1;
	for (;;) {
		if (readdir_r(dir, &entry.entry, &ent) != 0) {
			syslog(LOG_ERR, "readdir_r failed in clean_dirfd");
			goto error;
		}
		if (ent == NULL)
			break;
		if (ent->d_name[0] == '.' && (ent->d_name[1] == 0
				|| (ent->d_name[1] == '.' && ent->d_name[2] == 0)))
			continue;
		cr = unlinkat(dirfd, ent->d_name, 0);
		if (!cr)
			continue;
		if (errno != EISDIR) {
			syslog(LOG_ERR, "unlink of %s failed in clean_dirfd", ent->d_name);
			goto error;
		}
		fd = openat(dirfd, ent->d_name, O_DIRECTORY|O_RDONLY);
		if (fd < 0) {
			syslog(LOG_ERR, "opening directory %s failed in clean_dirfd", ent->d_name);
			goto error;
		}
		cr = clean_dirfd(fd);
		close(fd);
		if (cr)
			goto error;
		cr = unlinkat(dirfd, ent->d_name, AT_REMOVEDIR);
		if (cr) {
			syslog(LOG_ERR, "rmdir of %s failed in clean_dirfd", ent->d_name);
			goto error;
		}
	}
	cr = 0;
error:
	closedir(dir);
	return cr;
}

/* removes recursively the content of a directory */
static int clean_dir(const char *directory)
{
	int fd, rc;

	fd = openat(AT_FDCWD, directory, O_DIRECTORY|O_RDONLY);
	if (fd < 0) {
		syslog(LOG_ERR, "opening directory %s failed in clean_dir", directory);
		return fd;
	}
	rc = clean_dirfd(fd);
	close(fd);
	return rc;
}

/* removes the content of the working directory */
int enter_workdir(int clean)
{
	int rc = chdir(workdir);
	if (rc)
		syslog(LOG_ERR, "entring workdir %s failed", workdir);
	else if (clean)
		rc = clean_dir(workdir);
	return rc;
}

/* removes the working directory */
void remove_workdir()
{
	enter_workdir(1);
	chdir("..");
	rmdir(workdir);
}

int set_workdir(const char *name, int create)
{
	int rc;
	size_t length;
	struct stat s;

	/* check the length */
	length = strlen(name);
	if (length >= sizeof workdir) {
		syslog(LOG_ERR, "workdir name too long");
		errno = EINVAL;
		return -1;
	}

	rc = stat(name, &s);
	if (rc) {
		if (!create) {
			syslog(LOG_ERR, "no workdir %s", name);
			return -1;
		}
		rc = mkdir(name, mode);
		if (rc) {
			syslog(LOG_ERR, "can't create workdir %s", name);
			return -1;
		}

	} else if (!S_ISDIR(s.st_mode)) {
		syslog(LOG_ERR, "%s isn't a directory", name);
		errno = ENOTDIR;
		return -1;
	}
	memcpy(workdir, name, 1+length);
	return 0;
}

int make_workdir_base(const char *root, const char *prefix, int reuse)
{
	int i, n, r, l;

	n = snprintf(workdir, sizeof workdir, "%s/%s", root, prefix);
	if (n >= sizeof workdir) {
		syslog(LOG_ERR, "workdir prefix too long");
		errno = EINVAL;
		return -1;
	}
	r = (int)(sizeof workdir) - n;

	/* create a temporary directory */
	for (i = 0 ; ; i++) {
		if (i == INT_MAX) {
			syslog(LOG_ERR, "exhaustion of workdirs");
			return -1;
		}
		l = snprintf(workdir + n, r, "%d", i);
		if (l >= r) {
			syslog(LOG_ERR, "computed workdir too long");
			errno = EINVAL;
			return -1;
		}
		if (!mkdir(workdir, mode))
			break;
		if (errno != EEXIST) {
			syslog(LOG_ERR, "error in creation of workdir %s: %m", workdir);
			return -1;
		}
		if (reuse)
			break;
	}

	return 0;
}

int make_workdir(int reuse)
{
	return make_workdir_base(".", "PACK", reuse);
}

int workdirfd()
{
	int result = open(workdir, O_PATH|O_DIRECTORY);
	if (result < 0)
		syslog(LOG_ERR, "can't get fd for workdir %.*s: %m", PATH_MAX, workdir);
	return result;
}

int move_workdir(const char *dest, int parents, int force)
{
	
}


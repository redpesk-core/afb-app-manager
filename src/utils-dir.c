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

#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utils-dir.h"

static int clean_dirfd(int dirfd)
{
	int rc;
	DIR *dir;
	struct dirent *ent;
	struct {
		struct dirent entry;
		char spare[PATH_MAX];
	} entry;

	dir = fdopendir(dirfd);
	if (dir == NULL) {
		close(dirfd);
		return -1;
	}
	for (;;) {
		rc = -1;
		if (readdir_r(dir, &entry.entry, &ent) != 0)
			goto error;
		if (ent == NULL)
			break;
		if (ent->d_name[0] == '.' && (ent->d_name[1] == 0
				|| (ent->d_name[1] == '.' && ent->d_name[2] == 0)))
			continue;
		rc = unlinkat(dirfd, ent->d_name, 0);
		if (!rc)
			continue;
		if (errno != EISDIR)
			goto error;
		rc = openat(dirfd, ent->d_name, O_DIRECTORY|O_RDONLY);
		if (rc < 0)
			goto error;
		rc = clean_dirfd(rc);
		if (rc)
			goto error;
		rc = unlinkat(dirfd, ent->d_name, AT_REMOVEDIR);
		if (rc)
			goto error;
	}
	rc = 0;
error:
	closedir(dir);
	return rc;
}

/* removes recursively the content of a directory */
int remove_directory_content_fd(int dirfd)
{
	dirfd = dup(dirfd);
	return dirfd < 0 ? dirfd : clean_dirfd(dirfd);
}

/* removes recursively the content of a directory */
int remove_directory_content_at(int dirfd, const char *directory)
{
	int fd, rc;

	fd = openat(dirfd, directory, O_DIRECTORY|O_RDONLY);
	if (fd < 0)
		return fd;
	rc = remove_directory_content_fd(fd);
	return rc;
}


int remove_directory_content(const char *directory)
{
	return remove_directory_content_at(AT_FDCWD, directory);
}

/* removes the working directory */
int remove_directory(const char *directory, int force)
{
	int rc;
	rc = force ? remove_directory_content(directory) : 0;
	return rc ? rc : rmdir(directory);
}

int remove_directory_at(int dirfd, const char *directory, int force)
{
	int rc;
	rc = force ? remove_directory_content_at(dirfd, directory) : 0;
	return rc ? rc : unlinkat(dirfd, directory, AT_REMOVEDIR);
}

/* create a directory */
int create_directory_at(int dirfd, const char *directory, int mode, int mkparents)
{
	int rc, len, l;
	char *copy;
	const char *iter;

	rc = mkdirat(dirfd, directory, mode);
	if (!rc || errno != ENOENT)
		return rc;

	/* check parent of dest */
	iter = strrchr(directory, '/');
	len = iter ? iter - directory : 0;
	if (!len)
		return rc;
	copy = strndupa(directory, len);

	/* backward loop */
	l = len;
	rc = mkdirat(dirfd, copy, mode);
	while(rc) {
		if (errno != ENOENT)
			return rc;
		while (l && copy[l] != '/')
			l--;
		if (l == 0)
			return rc;
		copy[l] = 0;
		rc = mkdirat(dirfd, copy, mode);
	}
	/* forward loop */
	while(l < len) {
		copy[l] = '/';
		while (copy[++l]);
		rc = mkdirat(dirfd, copy, mode);
		if (rc)
			return rc;
	}

	/* finalize */
	return mkdirat(dirfd, directory, mode);
}

int create_directory(const char *directory, int mode, int mkparents)
{
	return create_directory_at(AT_FDCWD, directory, mode, mkparents);
}


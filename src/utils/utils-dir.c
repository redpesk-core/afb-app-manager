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

	dir = fdopendir(dirfd);
	if (dir == NULL) {
		close(dirfd);
		return -1;
	}
	for (;;) {
		rc = -1;
		errno = 0;
		ent = readdir(dir);
		if (ent == NULL) {
			if (errno)
				goto error;
			break;
		}
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
int create_directory_at(int dirfd, const char *directory, mode_t mode, int mkparents)
{
	int rc;
	size_t len, l;
	char *copy;
	const char *iter;

	rc = mkdirat(dirfd, directory, mode);
	if (!rc || errno != ENOENT)
		return rc;

	/* check parent of dest */
	iter = strrchr(directory, '/');
	len = iter ? (size_t)(iter - directory) : 0;
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

int create_directory(const char *directory, mode_t mode, int mkparents)
{
	return create_directory_at(AT_FDCWD, directory, mode, mkparents);
}


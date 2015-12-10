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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <wgt.h>

struct stringset {
	int count;
	char **strings;
};
 

struct appfwk {
	int refcount;
	struct stringset paths;
};

struct appfwk *appfwk_create()
{
	struct appfwk *appfwk = malloc(sizeof * appfwk);
	if (!appfwk)
		errno = ENOMEM;
	else {
		appfwk->refcount = 1;
		appfwk->paths.count = 0;
		appfwk->paths.strings = NULL;
	}
	return appfwk;
}

void appfwk_addref(struct appfwk *appfwk)
{
	assert(appfwk);
	appfwk->refcount++;
}

void appfwk_unref(struct appfwk *appfwk)
{
	assert(appfwk);
	if (!--appfwk->refcount) {
		while (appfwk->paths.count)
			free(appfwk->paths.strings[--appfwk->paths.count]);
		free(appfwk);
	}
}

int appfwk_add_root(struct appfwk *appfwk, const char *path)
{
	int i, n;
	char *r, **roots;
	assert(appfwk);
	r = realpath(path, NULL);
	if (!r)
		return -1;

	/* avoiding duplications */
	n = appfwk->paths.count;
	roots = appfwk->paths.strings;
	for (i = 0 ; i < n ; i++)
		if (!strcmp(r, roots[i])) {
			free(r);
			return 0;
		}

	/* add */
	roots = realloc(roots, (n + 1) * sizeof(roots[0]));
	if (!roots) {
		free(r);
		errno = ENOMEM;
		return -1;
	}
	roots[n++] = r;
	appfwk->paths.strings = roots;
	appfwk->paths.count = n;
	return 0;
}

struct aea {
	char *path;
	char *appver;
	char *ver;
	const char *root;
	const char *appid;
	const char *version;
	int (*callback)(struct wgt *wgt, void *data);
	void *data;
};

struct appfwk_enumerate_applications_context {
	const char *dirpath;
	const char *appid;
	const char *version;
	
	void *data;
};

inline int testd(int dirfd, struct dirent *e)
{
	return e->d_name[0] != '.' || (e->d_name[1] && (e->d_name[1] != '.' || e->d_name[2]));
}

#include <stdio.h>
static int aea3(int dirfd, struct aea *aea)
{
	printf("aea3 %s *** %s *** %s\n", aea->path, aea->appver, aea->ver);
	return 0;
}

static int aea2(int dirfd, struct aea *aea)
{
	DIR *dir;
	int rc, fd;
	struct dirent entry, *e;

	dir = fdopendir(dirfd);
	if (!dir)
		return -1;

	aea->version = entry.d_name;

	rc = readdir_r(dir, &entry, &e);
	while (!rc && e) {
		if (testd(dirfd, &entry)) {
			fd = openat(dirfd, entry.d_name, O_DIRECTORY|O_RDONLY);
			if (fd >= 0) {
				strcpy(aea->ver, entry.d_name);
				rc = aea3(fd, aea);			
				close(fd);
			}
		}	
		rc = readdir_r(dir, &entry, &e);
	}
	closedir(dir);
	return rc;
}

static int aea1(int dirfd, struct aea *aea)
{
	DIR *dir;
	int rc, fd;
	struct dirent entry, *e;

	dir = fdopendir(dirfd);
	if (!dir)
		return -1;

	aea->appid = entry.d_name;

	rc = readdir_r(dir, &entry, &e);
	while (!rc && e) {
		if (testd(dirfd, &entry)) {
			fd = openat(dirfd, entry.d_name, O_DIRECTORY|O_RDONLY);
			if (fd >= 0) {
				aea->ver = stpcpy(aea->appver, entry.d_name);
				*aea->ver++ = '/';
				rc = aea2(fd, aea);			
				close(fd);
			}
		}	
		rc = readdir_r(dir, &entry, &e);
	}
	closedir(dir);
	return rc;
}

int appfwk_enumerate_applications(struct appfwk *appfwk, int (*callback)(struct wgt *wgt, void *data), void *data)
{
	int rc, iroot, nroots;
	char **roots;
	int fd;
	char buffer[PATH_MAX];
	struct aea aea;

	aea.callback = callback;
	aea.data = data;
	aea.path = buffer;

	nroots = appfwk->paths.count;
	roots = appfwk->paths.strings;
	for (iroot = 0 ; iroot < nroots ; iroot++) {
		aea.root = roots[iroot];
		fd = openat(AT_FDCWD, aea.root, O_DIRECTORY|O_RDONLY);
		if (fd >= 0) {
			aea.appver = stpcpy(buffer, aea.root);
			*aea.appver++ = '/';
			rc = aea1(fd, &aea);
			close(fd);
		}
	}
	return 0;
}
/*
	struct wgt *wgt;
		wgt = wgt_create();
		if (!wgt)
			return -1;
		wgt_unref(wgt);

	rfd = AT_FDCWD;
	if (pathname) {
		rfd = openat(rfd, pathname, O_PATH|O_DIRECTORY);
*/


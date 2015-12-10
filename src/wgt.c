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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>


#include "wgt.h"

struct wgt {
	int refcount;
	int rootfd;
	int nrlocales;
	char **locales;
};

static int validsubpath(const char *subpath)
{
	int l = 0, i = 0;
	if (subpath[i] == '/')
		return 0;
	while(subpath[i]) {
		switch(subpath[i++]) {
		case '.':
			if (!subpath[i])
				break;
			if (subpath[i] == '/') {
				i++;
				break;
			}
			if (subpath[i++] == '.') {
				if (!subpath[i]) {
					l--;
					break;
				}
				if (subpath[i++] == '/') {
					l--;
					break;
				}
			}
		default:
			while(subpath[i] && subpath[i] != '/')
				i++;
			l++;
		case '/':
			break;
		}
	}
	return l >= 0;
}

struct wgt *wgt_create()
{
	struct wgt *wgt = malloc(sizeof * wgt);
	if (!wgt)
		errno = ENOMEM;
	else {
		wgt->refcount = 1;
		wgt->rootfd = -1;
		wgt->nrlocales = 0;
		wgt->locales = NULL;
	}
	return wgt;
}

void wgt_disconnect(struct wgt *wgt)
{
	assert(wgt);
	if (wgt->rootfd >= 0)
		close(wgt->rootfd);
	wgt->rootfd = -1;
}

void wgt_locales_reset(struct wgt *wgt)
{
	assert(wgt);
	while(wgt->nrlocales)
		free(wgt->locales[--wgt->nrlocales]);
}

void wgt_addref(struct wgt *wgt)
{
	assert(wgt);
	wgt->refcount++;
}

void wgt_unref(struct wgt *wgt)
{
	assert(wgt);
	if (!--wgt->refcount) {
		wgt_disconnect(wgt);
		wgt_locales_reset(wgt);
		free(wgt->locales);
		free(wgt);
	}
}

int wgt_connectat(struct wgt *wgt, int dirfd, const char *pathname)
{
	int rfd;

	assert(wgt);

	rfd = dirfd;
	if (pathname) {
		rfd = openat(rfd, pathname, O_PATH|O_DIRECTORY);
		if (rfd < 0)
			return rfd;
	}
	if (wgt->rootfd >= 0)
		close(wgt->rootfd);
	wgt->rootfd = rfd;
	return 0;
}

int wgt_connect(struct wgt *wgt, const char *pathname)
{
	return wgt_connectat(wgt, AT_FDCWD, pathname);
}

int wgt_is_connected(struct wgt *wgt)
{
	assert(wgt);
	return wgt->rootfd != -1;
}

int wgt_has(struct wgt *wgt, const char *filename)
{
	assert(wgt);
	assert(wgt_is_connected(wgt));
	if (!validsubpath(filename)) {
		errno = EINVAL;
		return -1;
	}
	return 0 == faccessat(wgt->rootfd, filename, F_OK, 0);
}

int wgt_open_read(struct wgt *wgt, const char *filename)
{
	assert(wgt);
	assert(wgt_is_connected(wgt));
	if (!validsubpath(filename)) {
		errno = EINVAL;
		return -1;
	}
	return openat(wgt->rootfd, filename, O_RDONLY);
}

static int locadd(struct wgt *wgt, const char *locstr, int length)
{
	int i;
	char *item, **ptr;

	item = strndup(locstr, length);
	if (item != NULL) {
		for (i = 0 ; item[i] ; i++)
			item[i] = tolower(item[i]);
		for (i = 0 ; i < wgt->nrlocales ; i++)
			if (!strcmp(item, wgt->locales[i])) {
				free(item);
				return 0;
			}

		ptr = realloc(wgt->locales, (1 + wgt->nrlocales) * sizeof(wgt->locales[0]));
		if (ptr) {
			wgt->locales = ptr;
			wgt->locales[wgt->nrlocales++] = item;
			return 0;
		}
		free(item);
	}
	errno = ENOMEM;
	return -1;
}

int wgt_locales_add(struct wgt *wgt, const char *locstr)
{
	const char *stop, *next;
	assert(wgt);
	while (*locstr) {
		stop = strchrnul(locstr, ',');
		next = stop + !!*stop;
		while (locstr != stop) {
			if (locadd(wgt, locstr, stop - locstr))
				return -1;
			do { stop--; } while(stop > locstr && *stop != '-');
		}
		locstr = next;
	}
	return 0;
}

int wgt_locales_score(struct wgt *wgt, const char *lang)
{
	int i;

	assert(wgt);
	if (lang)
		for (i = 0 ; i < wgt->nrlocales ; i++)
			if (!strcasecmp(lang, wgt->locales[i]))
				return i;

	return INT_MAX;
}

static const char *localize(struct wgt *wgt, const char *filename, char path[PATH_MAX])
{
	int i;

	if (!validsubpath(filename)) {
		errno = EINVAL;
		return NULL;
	}
	for (i = 0 ; i < wgt->nrlocales ; i++) {
		if (snprintf(path, PATH_MAX, "locales/%s/%s", wgt->locales[i], filename) >= PATH_MAX) {
			errno = EINVAL;
			return NULL;
		}
		if (0 == faccessat(wgt->rootfd, path, F_OK, 0))
			return path;
	}
	if (0 == faccessat(wgt->rootfd, filename, F_OK, 0))
		return filename;
	errno = ENOENT;
	return NULL;
}

char *wgt_locales_locate(struct wgt *wgt, const char *filename)
{
	char path[PATH_MAX];
	char * result;
	const char * loc;

	assert(wgt);
	assert(wgt_is_connected(wgt));
	loc = localize(wgt, filename, path);
	if (!loc)
		result = NULL;
	else {
		result = strdup(loc);
		if (!result)
			errno = ENOMEM;
	}
	return result;
}


int wgt_locales_open_read(struct wgt *wgt, const char *filename)
{
	char path[PATH_MAX];
	const char *loc;

	assert(wgt);
	assert(wgt_is_connected(wgt));
	loc = localize(wgt, filename, path);
	if (!loc)
		return -1;

	return openat(wgt->rootfd, loc, O_RDONLY);
}



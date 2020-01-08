/*
 Copyright (C) 2015-2020 IoT.bzh

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
	unsigned int nrlocales;
	char **locales;
};

/* a valid subpath is a relative path not looking deeper than root using .. */
static int validsubpath(const char *subpath)
{
	int l = 0, i = 0;

	/* absolute path is not valid */
	if (subpath[i] == '/')
		return 0;

	/* inspect the path */
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
			if (l >= 0)
				l++;
		case '/':
			break;
		}
	}
	return l >= 0;
}

/*
 * Normalizes and checks the 'subpath'.
 * Removes any starting '/' and checks that 'subpath'
 * does not contains sequence of '..' going deeper than
 * root.
 * Returns the normalized subpath or NULL in case of
 * invalid subpath.
 */
static const char *normalsubpath(const char *subpath)
{
	while(*subpath == '/')
		subpath++;
	return validsubpath(subpath) ? subpath : NULL;
}

/*
 * Creates a wgt handler and returns it or return NULL
 * in case of memory depletion.
 */
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

/*
 * Adds a reference to 'wgt'
 */
void wgt_addref(struct wgt *wgt)
{
	assert(wgt);
	wgt->refcount++;
}

/*
 * Drops a reference to 'wgt' and destroys it
 * if not more referenced
 */
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

/*
 * Creates a wgt handle and connect it to 'dirfd' and 'pathname'.
 *
 * Returns the created and connected wgt handle on success
 * or returns NULL if allocation failed or connecting had
 * error.
 */
struct wgt *wgt_createat(int dirfd, const char *pathname)
{
	struct wgt *wgt = wgt_create();
	if (wgt) {
		if (wgt_connectat(wgt, dirfd, pathname)) {
			wgt_unref(wgt);
			wgt = NULL;
		}
	}
	return wgt;
}

/*
 * Connect 'wgt' to the directory of 'pathname' relative
 * to the directory handled by 'dirfd'.
 *
 * Use AT_FDCWD for connecting relatively to the current directory.
 *
 * Use 'pathname' == NULL or "" for connecting to 'dirfd'. In
 * that case, 'dirfd' is duplicated and can safely be used later
 * by the client.
 *
 * If 'wgt' is already connected, it will be diconnected before.
 *
 * The languages settings are not changed.
 *
 * Returns 0 in case of success or -1 in case or error.
 */
int wgt_connectat(struct wgt *wgt, int dirfd, const char *pathname)
{
	int rfd;

	assert(wgt);

	rfd = (pathname && *pathname) ? openat(dirfd, pathname, O_PATH|O_DIRECTORY) : dup(dirfd);
	if (rfd < 0)
		return rfd;

	if (wgt->rootfd >= 0)
		close(wgt->rootfd);
	wgt->rootfd = rfd;
	return 0;
}

/*
 * Connect 'wgt' to the directory of 'pathname'.
 *
 * Acts like wgt_connectat(wgt, AT_FDCWD, pathname)
 */
int wgt_connect(struct wgt *wgt, const char *pathname)
{
	return wgt_connectat(wgt, AT_FDCWD, pathname);
}

/*
 * Disconnetcs 'wgt' if connected.
 */
void wgt_disconnect(struct wgt *wgt)
{
	assert(wgt);
	if (wgt->rootfd >= 0)
		close(wgt->rootfd);
	wgt->rootfd = -1;
}

/*
 * Checks if 'wgt' is connected and returns 1 if connected
 * or 0 if not connected.
 */
int wgt_is_connected(struct wgt *wgt)
{
	assert(wgt);
	return wgt->rootfd != -1;
}

/*
 * Tests wether the connected 'wgt' has the 'filename'.
 *
 * It is an error (with errno = EINVAL) to test an
 * invalid filename.
 *
 * Returns 0 if it hasn't it, 1 if it has it or
 * -1 if an error occured.
 */
int wgt_has(struct wgt *wgt, const char *filename)
{
	assert(wgt);
	assert(wgt_is_connected(wgt));

	filename = normalsubpath(filename);
	if (!filename) {
		errno = EINVAL;
		return -1;
	}
	return 0 == faccessat(wgt->rootfd, filename, F_OK, 0);
}

/*
 * Opens 'filename' for read from the connected 'wgt'.
 *
 * Returns the file descriptor as returned by openat
 * system call or -1 in case of error.
 */
int wgt_open_read(struct wgt *wgt, const char *filename)
{
	assert(wgt);
	assert(wgt_is_connected(wgt));
	filename = normalsubpath(filename);
	if (!filename) {
		errno = EINVAL;
		return -1;
	}
	return openat(wgt->rootfd, filename, O_RDONLY);
}

/*
 * Adds if needed the locale 'locstr' of 'length'
 * to the list of locales.
 */
static int locadd(struct wgt *wgt, const char *locstr, size_t length)
{
	unsigned int i;
	char *item, **ptr;

	item = strndup(locstr, length);
	if (item != NULL) {
		/* normalize in lower case */
		for (i = 0 ; item[i] ; i++)
			item[i] = (char)tolower(item[i]);

		/* search it (no duplication) */
		for (i = 0 ; i < wgt->nrlocales ; i++)
			if (!strcmp(item, wgt->locales[i])) {
				free(item);
				return 0;
			}

		/* append it to the list */
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

/*
 * clears the list of locales of 'wgt'
 */
void wgt_locales_reset(struct wgt *wgt)
{
	assert(wgt);
	while(wgt->nrlocales)
		free(wgt->locales[--wgt->nrlocales]);
}

/*
 * Adds to 'wgt' the locales defined by 'locstr'.
 *
 * Example: passing "fr-CH,en-GB" will add "fr-CH",
 * "fr", "en-GB" and then "en" to the list of locales.
 *
 * Returns 0 in case of success or -1 in case of memory
 * depletion.
 */
int wgt_locales_add(struct wgt *wgt, const char *locstr)
{
	const char *stop, *next;
	assert(wgt);

	/* iterate the comma separated languages */
	while (*locstr) {
		stop = strchrnul(locstr, ',');
		next = stop + !!*stop;
		/* iterate variant of languages in reverse order */
		while (locstr != stop) {
			if (locadd(wgt, locstr, (size_t)(stop - locstr)))
				return -1;
			do { stop--; } while(stop > locstr && *stop != '-');
		}
		locstr = next;
	}
	return 0;
}

/*
 * Get the score of the language 'lang' for 'wgt'.
 *
 * The lower result means the higher priority of the language.
 * The returned value of 0 is the top first priority.
 */
unsigned int wgt_locales_score(struct wgt *wgt, const char *lang)
{
	unsigned int i;

	assert(wgt);
	if (lang)
		for (i = 0 ; i < wgt->nrlocales ; i++)
			if (!strcasecmp(lang, wgt->locales[i]))
				return i;

	return UINT_MAX;
}

/*
 * Applies the localisation algorithm of 'filename'
 * within 'wgt'. Use the scratch buffer given by 'path'.
 *
 * Returns the filepath of the located file or NULL
 * if not found. If not NULL, the returned value is either
 * 'path' or the normalized version of 'filename'.
 */
static const char *localize(struct wgt *wgt, const char *filename, char path[PATH_MAX])
{
	unsigned int i;

	/* get the normalized name */
	filename = normalsubpath(filename);
	if (!filename) {
		errno = EINVAL;
		return NULL;
	}

	/* search in locales */
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

/*
 * Gets the localized file of 'filename' within 'wgt'.
 *
 * Returns a fresh allocated string for the found 'filename'.
 * Returns NULL if file is not found (ENOENT) or memory
 * exhausted (ENOMEM).
 */
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

/*
 * Opens for read the localized version of 'filename'
 * from the connected 'wgt'.
 *
 * Returns the file descriptor as returned by openat
 * system call or -1 in case of error.
 */
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


#if defined(TEST_wgt_validsubpath)
#include <stdio.h>
void t(const char *subpath, int validity) {
  printf("%s -> %d = %d, %s\n", subpath, validity, validsubpath(subpath), validsubpath(subpath)==validity ? "ok" : "NOT OK");
}
int main() {
  t("/",0);
  t("..",0);
  t(".",1);
  t("../a",0);
  t("a/..",1);
  t("a/../////..",0);
  t("a/../b/..",1);
  t("a/b/c/..",1);
  t("a/b/c/../..",1);
  t("a/b/c/../../..",1);
  t("a/b/c/../../../.",1);
  t("./..a/././..b/..c/./.././.././../.",1);
  t("./..a/././..b/..c/./.././.././.././..",0);
  t("./..a//.//./..b/..c/./.././/./././///.././.././a/a/a/a/a",1);
  return 0;
}
#endif


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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "wgt.h"

struct locarr {
	int count;
	char **arr;
};

static struct locarr locarr = { 0, NULL };

static int add(const char *locstr, int length)
{
	int i;
	char *item, **ptr;

	item = strndup(locstr, length);
	if (item != NULL) {
		for (i = 0 ; item[i] ; i++)
			item[i] = tolower(item[i]);
		for (i = 0 ; i < locarr.count ; i++)
			if (!strcmp(item, locarr.arr[i])) {
				free(item);
				return 0;
			}

		ptr = realloc(locarr.arr, (1 + locarr.count) * sizeof(locarr.arr[0]));
		if (ptr) {
			locarr.arr = ptr;
			locarr.arr[locarr.count++] = item;
			return 0;
		}
		free(item);
	}
	errno = ENOMEM;
	return -1;
}

void locales_reset()
{
	while (locarr.count)
		free(locarr.arr[--locarr.count]);
}

int locales_add(const char *locstr)
{
	const char *stop, *next;
	while (*locstr) {
		stop = strchrnul(locstr, ',');
		next = stop + !!*stop;
		while (locstr != stop) {
			if (add(locstr, stop - locstr))
				return -1;
			do { stop--; } while(stop > locstr && *stop != '-');
		}
		locstr = next;
	}
	return 0;
}

int locales_score(const char *lang)
{
	int i;

	if (lang)
		for (i = 0 ; i < locarr.count ; i++)
			if (!strcasecmp(lang, locarr.arr[i]))
				return i;

	return INT_MAX;
}

char *locales_locate_file(const char *filename)
{
	int i;
	char path[PATH_MAX];
	char * result;

	for (i = 0 ; i < locarr.count ; i++) {
		if (snprintf(path, sizeof path, "locales/%s/%s", locarr.arr[i], filename) >= (int)(sizeof path)) {
			errno = EINVAL;
			return NULL;
		}
		if (widget_has(path)) {
			result = strdup(path);
			if (!result)
				errno = ENOMEM;
			return result;
		}
	}
	if (widget_has(filename)) {
		result = strdup(filename);
		if (!result)
			errno = ENOMEM;
		return result;
	}
	errno = ENOENT;
	return NULL;
}


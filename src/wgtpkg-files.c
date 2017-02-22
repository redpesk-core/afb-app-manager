/*
 Copyright 2015, 2016, 2017 IoT.bzh

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
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "verbose.h"
#include "wgtpkg-workdir.h"
#include "wgtpkg-files.h"

struct fdb {
	unsigned int count;
	struct filedesc **files;
};

static struct fdb allfiles = { .count = 0, .files = NULL };
static struct fdb allsignatures = { .count = 0, .files = NULL };

static const char author_file[] = "author-signature.xml";
static const char distributor_file_prefix[] = "signature";
static const char distributor_file_suffix[] = ".xml";

static unsigned int what_signature(const char *name)
{
	unsigned int len, id, nid;

	if (!strcmp(name, author_file))
		return UINT_MAX;

	len = sizeof(distributor_file_prefix)-1;
	if (strncmp(name, distributor_file_prefix, len))
		return 0;
	if (name[len] <= '0' || name[len] > '9')
		return 0;
	id = (unsigned int)(name[len++] - '0');
	while ('0' <= name[len] && name[len] <= '9') {
		nid = 10 * id + (unsigned int)(name[len++] - '0');
		if (nid < id || nid == UINT_MAX) {
			WARNING("number too big for %s", name);
			return 0;
		}
		id = nid;
	}
	if (strcmp(name+len, distributor_file_suffix))
		return 0;

	return id;
}

static struct filedesc *get_filedesc(const char *name, int create)
{
	int cmp;
	unsigned int low, up, mid, sig;
	struct filedesc *result, **grow;

	/* search */
	low = 0;
	up = allfiles.count;
	while(low < up) {
		mid = (low + up) >> 1;
		result = allfiles.files[mid];
		cmp = strcmp(result->name, name);
		if (!cmp)
			return result; /* found */
		if (cmp > 0)
			up = mid;
		else
			low = mid + 1;
	}

	/* not found, can create ? */
	if (!create)
		return NULL;

	sig = what_signature(name);

	/* allocations */
	grow = realloc(allfiles.files, (allfiles.count + 1) * sizeof(struct filedesc *));
	if (grow == NULL) {
		ERROR("realloc failed in get_filedesc");
		return NULL;
	}
	allfiles.files = grow;

	if (sig) {
		grow = realloc(allsignatures.files, (allsignatures.count + 1) * sizeof(struct filedesc *));
		if (grow == NULL) {
			ERROR("second realloc failed in get_filedesc");
			return NULL;
		}
		allsignatures.files = grow;
	}

	result = malloc(sizeof(struct filedesc) + strlen(name));
	if (!result) {
		ERROR("calloc failed in get_filedesc");
		return NULL;
	}

	/* initialisation */
	result->type = type_unset;
	result->flags = sig == 0 ? 0 : sig == UINT_MAX ? flag_author_signature : flag_distributor_signature;
	result->zindex = 0;
	result->signum = sig;
	strcpy(result->name, name);

	/* insertion */
	if (low < allfiles.count)
		memmove(allfiles.files+low+1, allfiles.files+low, (allfiles.count - low) * sizeof(struct filedesc *));
	allfiles.files[low] = result;
	allfiles.count++;
	if (sig) {
		for (low = 0 ; low < allsignatures.count && sig > allsignatures.files[low]->signum ; low++);
		if (low < allsignatures.count)
			memmove(allsignatures.files+low+1, allsignatures.files+low, (allsignatures.count - low) * sizeof(struct filedesc *));
		allsignatures.files[low] = result;
		allsignatures.count++;
	}

	return result;
}
	

static struct filedesc *file_add(const char *name, enum entrytype type)
{
	struct filedesc *desc;

	desc = get_filedesc(name, 1);
	if (!desc)
		errno = ENOMEM;
	else if (desc->type == type_unset)
		desc->type = type;
	else {
		ERROR("redeclaration of %s in file_add", name);
		errno = EEXIST;
		desc = NULL;
	}
	return desc;
}

void file_reset()
{
	unsigned int i;

	allsignatures.count = 0;
	for (i = 0 ; i < allfiles.count ; i++)
		free(allfiles.files[i]);
	allfiles.count = 0;
}

unsigned int file_count()
{
	return allfiles.count;
}

struct filedesc *file_of_index(unsigned int index)
{
	assert(index < allfiles.count);
	return allfiles.files[index];
}

struct filedesc *file_of_name(const char *name)
{
	return get_filedesc(name, 0);
}

struct filedesc *file_add_directory(const char *name)
{
	return file_add(name, type_directory);
}

struct filedesc *file_add_file(const char *name)
{
	return file_add(name, type_file);
}

unsigned int signature_count()
{
	return allsignatures.count;
}

struct filedesc *signature_of_index(unsigned int index)
{
	assert(index < allsignatures.count);
	return allsignatures.files[index];
}

struct filedesc *get_signature(unsigned int number)
{
	unsigned int idx;

	if (number == 0)
		number = UINT_MAX;
	for (idx = 0 ; idx < allsignatures.count ; idx++)
		if (allsignatures.files[idx]->signum == number)
			return allsignatures.files[idx];
	return NULL;
}

struct filedesc *create_signature(unsigned int number)
{
	struct filedesc *result;
	char *name;
	int len;

	result = NULL;
	if (number == 0 || number == UINT_MAX)
		len = asprintf(&name, "%s", author_file);
	else
		len = asprintf(&name, "%s%u%s", distributor_file_prefix, number, distributor_file_suffix);

	if (len < 0)
		ERROR("asprintf failed in create_signature");
	else {
		assert(len > 0);
		result = file_of_name(name);
		if (result == NULL)
			result = file_add_file(name);
		free(name);
	}

	return result;
}

/* remove flags that are not related to being signature */
void file_clear_flags()
{
	unsigned int i;
	for (i = 0 ; i < allfiles.count ; i++)
		allfiles.files[i]->flags &= flag_signature;
}

static int fill_files_rec(char name[PATH_MAX], unsigned offset)
{
	int err, fd;
	unsigned len;
	DIR *dir;
	struct dirent *ent;
	struct stat st;

	fd = openat(workdirfd, offset ? name : ".", O_DIRECTORY|O_RDONLY);
	if (fd < 0) {
		ERROR("openat %.*s failed in fill_files_rec", offset, name);
		return -1;
	}
	dir = fdopendir(fd);
	if (!dir) {
		ERROR("opendir %.*s failed in fill_files_rec", offset, name);
		close(fd);
		return -1;
	}
	if (offset)
		name[offset++] = '/';

	ent = readdir(dir);
	while (ent != NULL) {
		len = (unsigned)strlen(ent->d_name);
		if (ent->d_name[0] == '.' && (len == 1 || 
			(ent->d_name[1] == '.' && len == 2)))
			;
		else if (offset + len >= PATH_MAX) {
			closedir(dir);
			ERROR("name too long in fill_files_rec");
			errno = ENAMETOOLONG;
			return -1;
		} else {
			memcpy(name + offset, ent->d_name, 1+len);
			if (ent->d_type == DT_UNKNOWN) {
				fstatat(fd, ent->d_name, &st, 0);
				if (S_ISREG(st.st_mode))
					ent->d_type = DT_REG;
				else if (S_ISDIR(st.st_mode))
					ent->d_type = DT_DIR;
			}
			switch (ent->d_type) {
			case DT_DIR:
				if (file_add_directory(name) == NULL) {
					closedir(dir);
					return -1;
				}
				err = fill_files_rec(name, offset + len);
				if (err) {
					closedir(dir);
					return err;
				}
				break;
			case DT_REG:
				if (file_add_file(name) == NULL) {
					closedir(dir);
					return -1;
				}
				break;
			default:
				break;
			}
		}
		ent = readdir(dir);
	}

	closedir(dir);
	return 0;
}

int fill_files()
{
	char name[PATH_MAX];
	return fill_files_rec(name, 0);
}


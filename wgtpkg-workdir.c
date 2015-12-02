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


#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>

#include "wgtpkg.h"

#ifndef PREDIR
#define PREDIR  "./"
#endif

static int mode = 0700;
static char workdir[PATH_MAX];

/* removes recursively the content of a directory */
static int clean_dir()
{
	int cr;
	DIR *dir;
	struct dirent *ent;
	struct {
		struct dirent entry;
		char spare[PATH_MAX];
	} entry;

	dir = opendir(".");
	if (dir == NULL) {
		syslog(LOG_ERR, "opendir failed in clean_dir");
		return -1;
	}

	cr = -1;
	for (;;) {
		if (readdir_r(dir, &entry.entry, &ent) != 0) {
			syslog(LOG_ERR, "readdir_r failed in clean_dir");
			goto error;
		}
		if (ent == NULL)
			break;
		if (ent->d_name[0] == '.' && (ent->d_name[1] == 0
				|| (ent->d_name[1] == '.' && ent->d_name[2] == 0)))
			continue;
		cr = unlink(ent->d_name);
		if (!cr)
			continue;
		if (errno != EISDIR) {
			syslog(LOG_ERR, "unlink of %s failed in clean_dir", ent->d_name);
			goto error;
		}
		if (chdir(ent->d_name)) {
			syslog(LOG_ERR, "enter directory %s failed in clean_dir", ent->d_name);
			goto error;
		}
		cr = clean_dir();
		if (cr)
			goto error;
		if (chdir(".."))
			goto error;
		cr = rmdir(ent->d_name);
		if (cr) {
			syslog(LOG_ERR, "rmdir of %s failed in clean_dir", ent->d_name);
			goto error;
		}
	}
	cr = 0;
error:
	closedir(dir);
	return cr;
}

/* removes the content of the working directory */
int enter_workdir(int clean)
{
	int rc = chdir(workdir);
	if (rc)
		syslog(LOG_ERR, "entring workdir %s failed", workdir);
	else if (clean)
		rc = clean_dir();
	return rc;
}

/* removes the working directory */
void remove_workdir()
{
	enter_workdir(1);
	chdir("..");
	unlink(workdir);
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
		return -1;
	}
	memcpy(workdir, name, 1+length);
	return 0;
}

/* install the widgets of the list */
int make_workdir(int reuse)
{
	int i;

	/* create a temporary directory */
	for (i = 0 ; ; i++) {
		if (i == INT_MAX) {
			syslog(LOG_ERR, "exhaustion of workdirs");
			return -1;
		}
		sprintf(workdir, PREDIR "PACK%d", i);
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


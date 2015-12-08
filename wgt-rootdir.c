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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "wgt.h"


static int rootfd = AT_FDCWD;

int widget_set_rootdir(const char *pathname)
{
	int rfd;

	if (!pathname)
		rfd = AT_FDCWD;
	else {
		rfd = openat(AT_FDCWD, pathname, O_PATH|O_DIRECTORY);
		if (rfd < 0)
			return rfd;
	}
	if (rootfd >= 0)
		close(rootfd);
	rootfd = AT_FDCWD;
	return 0;
}

int widget_has(const char *filename)
{
	return 0 == faccessat(rootfd, filename, F_OK, 0);
}

int widget_open_read(const char *filename)
{
	return openat(rootfd, filename, O_RDONLY);
}


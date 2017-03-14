/*
 Copyright 2017 IoT.bzh

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
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils-file.h"

/* alias for getfile_at(AT_FDCWD, file, content, size) */
int getfile(const char *file, char **content, size_t *size)
{
	return getfile_at(AT_FDCWD, file, content, size);
}

/*
 * Reads the 'file' relative to 'dfd' (see openat) in a freshly
 * allocated memory and returns it in 'content' and 'size' (if not NULL).
 * To help in reading text files, a pending null is added at the
 * end of the content.
 * Return 0 in case of success or else -1 and set 'errno'.
 */
int getfile_at(int dfd, const char *file, char **content, size_t *size)
{
	int rc, f;
	struct stat s;
	char *r;
	ssize_t rsz;
	size_t sz, i;

	if (content)
		*content = NULL;
	rc = openat(dfd, file, O_RDONLY);
	if (rc >= 0) {
		f = rc;
		rc = fstat(f, &s);
		if (rc != 0) {
			/* nothing */;
		} else if (!S_ISREG(s.st_mode)) {
			rc = -1;
			errno = EBADF;
		} else {
			sz = (size_t)s.st_size;
			if (content) {
				r = malloc(sz + 1);
				if (!r) {
					errno = ENOMEM;
					rc = -1;
				} else {
					i = 0;
					while (!rc && i < sz) {
						rsz = read(f, r + i, sz - i);
						if (rsz == 0)
							sz = i;
						else if (rsz > 0)
							i += (size_t)rsz;
						else if (errno != EINTR && errno != EAGAIN) {
							free(r);
							rc = -1;
						}
					}
					if (rc == 0) {
						r[sz] = 0;
						*content = r;
					}
				}
			}
			if (size)
				*size = sz;
		}
		close(f);
	}
	return rc;
}

/* alias for putfile_at(AT_FDCWD, file, content, size) */
int putfile(const char *file, const void *content, size_t size)
{
	return putfile_at(AT_FDCWD, file, content, size);
}

int putfile_at(int dfd, const char *file, const void *content, size_t size)
{
	int rc, f;
	ssize_t wsz;
	size_t i;

	rc = openat(dfd, file, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (rc >= 0) {
		if (size == (size_t)(ssize_t)-1)
			size = strlen(content);
		f = rc;
		i = 0;
		rc = 0;
		while (!rc && i < size) {
			wsz = write(f, content + i, size - i);
			if (wsz >= 0)
				i += (size_t)wsz;
			else if (errno != EINTR && errno != EAGAIN) {
				unlinkat(dfd, file, 0);
				rc = -1;
			}
		}
		close(f);
	}
	return rc;
}


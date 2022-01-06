/*
 Copyright (C) 2015-2022 IoT.bzh Company

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

#define _DEFAULT_SOURCE

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

#include "verbose.h"
#include "wgtpkg-files.h"
#include "wgtpkg-workdir.h"
#include "wgtpkg-zip.h"

#define MODE_OF_FILE_CREATION 0644
#define MODE_OF_DIRECTORY_CREATION 0755

#if !defined(USE_LIBZIP)
#	define USE_LIBZIP 1
#endif

/***********************************************************
 *        USING LIBZIP
 ***********************************************************/
#if USE_LIBZIP

#include <zip.h>

static int is_valid_filename(const char *filename)
{
	int lastsp = 0;
	int index = 0;
	unsigned char c;

	c = (unsigned char)filename[index];
	while (c) {
		if ((c < 0x1f)
		 || ((lastsp = (c == 0x20)) && index == 0)
		 || c == 0x7f || c == 0x3c || c == 0x3e
		 || c == 0x3a || c == 0x22
		 || c == 0x5c || c == 0x7c || c == 0x3f
		 || c == 0x2a || c == 0x5e || c == 0x60
		 || c == 0x7b || c == 0x7d || c == 0x21)
			return 0;
		c = (unsigned char)filename[++index];
	}
	return !lastsp;
}

static int create_directory(char *file, mode_t mode)
{
	int rc;
	char *last = strrchr(file, '/');
	if (last != NULL)
		*last = 0;
	rc = mkdirat(workdirfd, file, mode);
	if (rc) {
		if (errno == EEXIST)
			rc = 0;
		else if (errno == ENOENT) {
			rc = create_directory(file, mode);
			if (!rc)
				rc = mkdirat(workdirfd, file, mode);
		}
	}
	if (rc)
		ERROR("can't create directory %s", file);
	if (last != NULL)
		*last = '/';
	return rc;
}

static int create_file(char *file, int fmode, mode_t dmode)
{
	int fd = openat(workdirfd, file, O_CREAT|O_WRONLY|O_TRUNC, fmode);
	if (fd < 0 && errno == ENOENT) {
		if (!create_directory(file, dmode))
			fd = openat(workdirfd, file, O_CREAT|O_WRONLY|O_TRUNC, fmode);
	}
	if (fd < 0)
		ERROR("can't create file %s", file);
	return fd;
}

/* read (extract) 'zipfile' in current directory */
int zread(const char *zipfile, unsigned long long maxsize)
{
	struct filedesc *fdesc;
	int err, fd;
	size_t len;
	struct zip *zip;
	zip_int64_t z64;
	zip_uint64_t uz64;
	unsigned int count, index;
	struct zip_file *zfile;
	struct zip_stat zstat;
	char buffer[32768];
	ssize_t sizr, sizw;
	zip_uint64_t esize;

	/* open the zip file */
	zip = zip_open(zipfile, ZIP_CHECKCONS, &err);
	if (!zip) {
		ERROR("Can't connect to file %s", zipfile);
		return -1;
	}

	z64 = zip_get_num_entries(zip, 0);
	if (z64 < 0 || z64 > UINT_MAX) {
		ERROR("too many entries in %s", zipfile);
		goto error;
	}
	count = (unsigned int)z64;

	/* records the files */
	file_reset();
	esize = 0;
	for (index = 0 ; index < count ; index++) {
		err = zip_stat_index(zip, index, ZIP_FL_ENC_GUESS, &zstat);
		/* check the file name */
		if (!is_valid_filename(zstat.name)) {
			ERROR("invalid entry %s found in %s", zstat.name, zipfile);
			goto error;
		}
		if (zstat.name[0] == '/') {
			ERROR("absolute entry %s found in %s", zstat.name, zipfile);
			goto error;
		}
		len = strlen(zstat.name);
		if (len == 0) {
			ERROR("empty entry found in %s", zipfile);
			goto error;
		}
		if (zstat.name[len - 1] == '/')
			/* record */
			fdesc = file_add_directory(zstat.name);
		else {
			/* get the size */
			esize += zstat.size;
			/* record */
			fdesc = file_add_file(zstat.name);
		}
		if (!fdesc)
			goto error;
		fdesc->zindex = index;
	}

	/* check the size */
	if (maxsize && esize > maxsize) {
		ERROR("extracted size %zu greater than allowed size %llu", esize, maxsize);
		goto error;
	}

	/* unpack the recorded files */
	assert(count == file_count());
	for (index = 0 ; index < count ; index++) {
		fdesc = file_of_index(index);
		assert(fdesc != NULL);
		err = zip_stat_index(zip, fdesc->zindex, ZIP_FL_ENC_GUESS, &zstat);
		assert(zstat.name[0] != '/');
		len = strlen(zstat.name);
		assert(len > 0);
		if (zstat.name[len - 1] == '/') {
			/* directory name */
			err = create_directory((char*)zstat.name, MODE_OF_DIRECTORY_CREATION);
			if (err && errno != EEXIST)
				goto error;
		} else {
			/* file name */
			zfile = zip_fopen_index(zip, fdesc->zindex, 0);
			if (!zfile) {
				ERROR("Can't open %s in %s", zstat.name, zipfile);
				goto error;
			}
			fd = create_file((char*)zstat.name, MODE_OF_FILE_CREATION, MODE_OF_DIRECTORY_CREATION);
			if (fd < 0)
				goto errorz;
			/* extract */
			uz64 = zstat.size;
			while (uz64) {
				sizr = (ssize_t)zip_fread(zfile, buffer, sizeof buffer);
				if (sizr < 0) {
					ERROR("error while reading %s in %s", zstat.name, zipfile);
					goto errorzf;
				}
				sizw = write(fd, buffer, (size_t)sizr);
				if (sizw < 0) {
					ERROR("error while writing %s", zstat.name);
					goto errorzf;
				}
				uz64 -= (size_t)sizw;
			}
			close(fd);
			zip_fclose(zfile);
		}
	}

	zip_close(zip);
	return 0;

errorzf:
	close(fd);
errorz:
	zip_fclose(zfile);
error:
	zip_close(zip);
	return -1;
}

struct zws {
	struct zip *zip;
	char name[PATH_MAX];
	char buffer[32768];
};

static int zwr(struct zws *zws, size_t offset)
{
	int err, fd;
	size_t len;
	DIR *dir;
	struct dirent *ent;
	zip_int64_t z64;
	struct zip_source *zsrc;
	FILE *fp;
	struct stat st;

	fd = openat(workdirfd, offset ? zws->name : ".", O_DIRECTORY|O_RDONLY);
	if (fd < 0) {
		ERROR("opendir %.*s failed in zwr", (int)offset, zws->name);
		return -1;
	}
	dir = fdopendir(fd);
	if (!dir) {
		close(fd);
		ERROR("opendir %.*s failed in zwr", (int)offset, zws->name);
		return -1;
	}

	if (offset != 0)
		zws->name[offset++] = '/';

	ent = readdir(dir);
	while (ent != NULL) {
		len = strlen(ent->d_name);
		if (ent->d_name[0] == '.' && (len == 1 ||
			(ent->d_name[1] == '.' && len == 2)))
			;
		else if (offset + len >= sizeof(zws->name)) {
			ERROR("name too long in zwr");
			errno = ENAMETOOLONG;
			goto error;
		} else {
			memcpy(zws->name + offset, ent->d_name, 1+len);
			if (!is_valid_filename(ent->d_name)) {
				ERROR("invalid name %s", zws->name);
				goto error;
			}
			if (ent->d_type == DT_UNKNOWN) {
				fstatat(fd, ent->d_name, &st, 0);
				if (S_ISREG(st.st_mode))
					ent->d_type = DT_REG;
				else if (S_ISDIR(st.st_mode))
					ent->d_type = DT_DIR;
			}
			switch (ent->d_type) {
			case DT_DIR:
				z64 = zip_dir_add(zws->zip, zws->name, ZIP_FL_ENC_UTF_8);
				if (z64 < 0) {
					ERROR("zip_dir_add of %s failed", zws->name);
					goto error;
				}
				err = zwr(zws, offset + len);
				if (err)
					goto error;
				break;
			case DT_REG:
				fd = openat(workdirfd, zws->name, O_RDONLY);
				if (fd < 0) {
					ERROR("openat of %s failed", zws->name);
					goto error;
				}
				fp = fdopen(fd, "r");
				if (fp == NULL) {
					ERROR("fdopen of %s failed", zws->name);
					close(fd);
					goto error;
				}
				zsrc = zip_source_filep(zws->zip, fp, 0, 0);
				if (zsrc == NULL) {
					ERROR("zip_source_file of %s failed", zws->name);
					fclose(fp);
					goto error;
				}
				z64 = zip_file_add(zws->zip, zws->name, zsrc, ZIP_FL_ENC_UTF_8);
				if (z64 < 0) {
					ERROR("zip_file_add of %s failed", zws->name);
					zip_source_free(zsrc);
					goto error;
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
error:
	closedir(dir);
	return -1;
}

/* write (pack) content of the current directory in 'zipfile' */
int zwrite(const char *zipfile)
{
	int err;
	struct zws zws;

	zws.zip = zip_open(zipfile, ZIP_CREATE|ZIP_TRUNCATE, &err);
	if (!zws.zip) {
		ERROR("Can't open %s for write", zipfile);
		return -1;
	}

	err = zwr(&zws, 0);
	zip_close(zws.zip);
	return err;
}

/***********************************************************
 *        NOT USING LIBZIP: FORKING
 ***********************************************************/
#else

#include <sys/wait.h>
#include <stdlib.h>

extern char **environ;

static char *getbin(const char *progname)
{
	char name[PATH_MAX];
	char *path;
	int i;

	if (progname[0] == '/')
		return access(progname, X_OK) ? NULL : strdup(progname);

	path = getenv("PATH");
	while(path && *path) {
		for (i = 0 ; path[i] && path[i] != ':' ; i++)
			name[i] = path[i];
		path += i + !!path[i];
		name[i] = '/';
		strcpy(name + i + 1, progname);
		if (access(name, X_OK) == 0)
			return realpath(name, NULL);
	}
	return NULL;
}

static int zrun(const char *name, const char *args[])
{
	int rc;
	siginfo_t si;
	char *binary;

	binary = getbin(name);
	if (binary == NULL) {
		ERROR("error while forking in zrun: can't find %s", name);
		return -1;
	}

	rc = fork();
	if (rc == 0) {
		rc = execve(binary, (char * const*)args, environ);
		ERROR("can't execute %s in zrun: %m", args[0]);
		_exit(1);
		return rc;
	}

	free(binary);
	if (rc < 0) {
		/* can't fork */
		ERROR("error while forking in zrun: %m");
		return rc;
	}

	/* wait termination of the child */
	rc = waitid(P_PID, (id_t)rc, &si, WEXITED);
	if (rc)
		ERROR("unexpected wait status in zrun of %s: %m", args[0]);
	else if (si.si_code != CLD_EXITED)
		ERROR("unexpected termination status of %s in zrun", args[0]);
	else if (si.si_status != 0)
		ERROR("child for %s terminated with error code %d in zwrite", args[0], si.si_status);
	else
		return 0;
	return -1;
}

/* read (extract) 'zipfile' in current directory */
int zread(const char *zipfile, unsigned long long maxsize)
{
	int rc;
	const char *args[6];

	args[0] = "unzip";
	args[1] = "-q";
	args[2] = "-d";
	args[3] = workdir;
	args[4] = zipfile;
	args[5] = NULL;

	file_reset();
	rc = zrun(args[0], args);
	if (!rc)
		rc = fill_files();
	return rc;
}

/* write (pack) content of the current directory in 'zipfile' */
int zwrite(const char *zipfile)
{
	const char *args[6];

	unlink(zipfile);
	args[0] = "zip";
	args[1] = "-q";
	args[2] = "-r";
	args[3] = zipfile;
	args[4] = workdir;
	args[5] = NULL;

	return zrun(args[0], args);
}

#endif
/***********************************************************
*        TESTING
***********************************************************/

#if defined(TEST_READ)
int main(int ac, char **av)
{
	for(av++ ; *av ; av++)
		zread(*av, 0);
	return 0;
}
#endif

#if defined(TEST_WRITE)
int main(int ac, char **av)
{
	for(av++ ; *av ; av++)
		zwrite(*av);
	return 0;
}
#endif


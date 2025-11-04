/*
 * Copyright (C) 2018-2025 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 */

#include "setup-utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <linux/xattr.h>

#if 0
#define lsetxattr(path, key, value, len, flags) (printf("lsetxattr(%s, %s, %.*s, %d, %d)\n",\
                                                        path,key,(int)len,value,(int)(len),flags), 0)
#define lremovexattr(path, key)                 (printf("lremovexattr(%s, %s)\n",path,key), 0)
#define symlink(target, link)                   (printf("symlink(%s, %s)\n",target,link),0)
#define lchown(path, uid, gid)                  (printf("lchown(%s, %d, %d)\n",path,uid,gid),0)
#define mkdir(path, mode)                       (printf("mkdir(%s, %o)\n",path,mode),0)
#endif

/* print the error and return 1 */
int err(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	fprintf(stderr, "Error: ");
	vfprintf(stderr, fmt, vl);
	fprintf(stderr, "\n");
	va_end(vl);
	return 1;
}

char *putuint(char *head, unsigned value)
{
	div_t d = div(value, 10);
	if (d.quot != 0)
		head = putuint(head, d.quot);
	*head = (char)('0' + d.rem);
	return head + 1;
}

void putpath(uid_t uid, gid_t gid, char *dest, const char *src)
{
	for(;;) {
		char c = *src++;
		if (c == '%') {
			switch (*src) {
			case 'u':
				dest = putuint(dest, (unsigned)uid);
				src++;
				continue;
			case 'g':
				dest = putuint(dest, (unsigned)gid);
				src++;
				continue;
			case '%':
				src++;
				break;
			}
		}
		*dest = c;
		if (c == '\0')
			return;
		dest++;
	}
}

int read_file(const char *path, char **content)
{
	int fd;
	off_t pos;
	ssize_t rb;
	char *buf;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return err("can't open file %s for reading", path);
	pos = lseek(fd, 0, SEEK_END);
	if (pos == (off_t)-1) {
		close(fd);
		return err("cant seek end of %s", path);
	}
	if (lseek(fd, 0, SEEK_SET) != 0) {
		close(fd);
		return err("cant rewind %s", path);
	}
	buf = malloc(1 + (size_t)pos);
	if (buf == NULL) {
		close(fd);
		return err("allocation failed");
	}
	rb = read(fd, buf, (size_t)pos);
	close(fd);
	if (rb < 0 || (off_t)rb != pos) {
		free(buf);
		return err("error while reading %s", path);
	}
	*content = buf;
	buf[pos] = 0;
	return 0;
}

int read_once(const char **content, char **memo, const char *path)
{
	if (*memo == NULL) {
		int sts = read_file(path, memo);
		if (sts != 0)
			return sts;
	}
	*content = *memo;
	return 0;
}

int read_passwd(const char **content)
{
	static char *memo = NULL;
	return read_once(content, &memo, "/etc/passwd");
}

int read_group(const char **content)
{
	static char *memo = NULL;
	return read_once(content, &memo, "/etc/group");
}

const char *nextcol(const char *text)
{
	for (;;) {
		switch(*text++) {
		case '\0':
		case '\n':
			return NULL;
		case ':':
			return text;
		}
	}
}

const char *colat(const char *text, int col)
{
	while (text != NULL && col > 0) {
		text = nextcol(text);
		col--;
	}
	return text;
}

const char *search_entry(const char *text, const char *key, int column)
{
	const char *cur, *ik, *listart = text;
	while(*listart) {
		cur = colat(listart, column);
		if (cur == NULL)
			cur = listart;
		else {
			ik = key;
			while(*cur != '\n' && *ik != 0 && *ik == *cur) {
				ik++;
				cur++;
			}
			if (*ik == 0 && (*cur == 0 || *cur == '\n' || *cur == ':'))
				return listart;
		}
		while(*cur && *cur++ != '\n');
		listart = cur;
	}
	return NULL;
}

int get_user(const char *user, uid_t *uid, gid_t *gid)
{
	char *end;
	const char *pwd;
	int sts, col;
	long lval;

	if (user == NULL || *user == 0)
		return err("invalid user argument");

	lval = strtol(user, &end, 10);
	if (*end != 0)
		col = 0; /* login */
	else if (gid != NULL)
		col = 2; /* uid */
	else {
		uid_t u = (uid_t)lval;
		if (lval < 0 || (long)u != lval)
			return err("invalid user id %s", user);
		*uid = u;
		return 0;
	}

	sts = read_passwd(&pwd);
	if (sts != 0)
		return sts;

	pwd = search_entry(pwd, user, col);
	if (pwd == NULL)
		return err("no entry for %s in passwd", user);

	pwd = colat(pwd, 2);
	if (pwd == NULL)
		return err("no uid in passwd for %s", user);

	*uid = (uid_t)atoi(pwd);
	if (gid != NULL) {
		pwd = nextcol(pwd);
		if (pwd == NULL)
			return err("no gid in passwd for %s", user);
		*gid = (gid_t)atoi(pwd);
	}

	return 0;
}

int get_group(const char *group, gid_t *gid)
{
	const char *grp;
	char *end;
	int sts;
	long lval;

	if (group == NULL || *group == 0)
		return err("invalid group argument");

	lval = strtol(group, &end, 10);
	if (*end == 0) {
		gid_t g = (gid_t)lval;
		if (lval < 0 || (long)g != lval)
			return err("invalid group id %s", group);
		*gid = g;
		return 0;
	}

	sts = read_group(&grp);
	if (sts != 0)
		return sts;

	grp = search_entry(grp, group, 0);
	if (grp == NULL)
		return err("no entry for %s in group", group);

	grp = colat(grp, 2);
	if (grp == NULL)
		return err("no gid in group for %s", group);
	*gid = (gid_t)atoi(grp);

	return 0;
}

int setsmack(
	const char *path,
	const char *label,
	bool transmute
) {
	int sts;

	/* SMACK setup of directory */
	sts = lsetxattr(path, XATTR_NAME_SMACK, label, strlen(label), 0);
	if (sts < 0)
		return err("setting SMACK to %s failed", path);
	if (transmute) {
		sts = lsetxattr(path, XATTR_NAME_SMACKTRANSMUTE, "TRUE", 4, 0);
		if (sts < 0)
			return err("setting SMACK transmute to %s failed", path);
	}
	else {
		sts = lremovexattr(path, XATTR_NAME_SMACKTRANSMUTE);
		if (sts < 0 && errno != ENODATA)
			return err("dropping SMACK transmute of %s failed", path);
	}
	return 0;
}

int make_link(
	uid_t uid,
	gid_t gid,
	const char *target,
	const char *link,
	mode_t mode,
	const char *label
) {
	char lval[PATH_MAX];
	int sts;

	/* create the directory */
	sts = symlink(target, link);
	if (sts < 0) {
		if (errno != EEXIST)
			return err("creation of link failed");
		sts = readlink(link, lval, sizeof lval);
		if (sts < 0 || sts >= (int)sizeof lval)
			return err("reading existing link failed");
		lval[sts] = 0;
		if (strcmp(lval, target) != 0)
			return err("link already existing");
	}
	sts = lchown(link, uid, gid);
	if (sts < 0)
		return err("setting owner of link failed");

	/* SMACK setup of directory */
	return setsmack(link, label, false);
}

int make_dir(
	uid_t uid,
	gid_t gid,
	const char *path,
	mode_t mode,
	const char *label,
	bool transmute
) {
	int sts;

	/* create the directory */
	sts = mkdir(path, mode);
	if (sts < 0) {
		if (errno != EEXIST)
			return err("creation of directory failed");
		sts = chmod(path, mode);
		if (sts < 0)
			return err("can't set mode of existing directory");
	}
	sts = lchown(path, uid, gid);
	if (sts < 0)
		return err("setting owner of directory failed");

	/* SMACK setup of directory */
	return setsmack(path, label, transmute);
}

int setup_dirs(uid_t uid, gid_t gid, const struct descdir *iter, const struct descdir *end, bool subst)
{
	int sts;

	if (subst) {
		char path[PATH_MAX];
		for (sts = 0 ; sts == 0 && iter != end ; iter++) {
			putpath(uid, gid, path, iter->path);
			sts = make_dir(uid, gid, path, iter->mode, iter->label, iter->transmute);
		}
	}
	else {
		for (sts = 0 ; sts == 0 && iter != end ; iter++)
			sts = make_dir(uid, gid, iter->path, iter->mode, iter->label, iter->transmute);
	}
	return sts;
}

int setup_links(uid_t uid, gid_t gid, const struct desclink *iter, const struct desclink *end, bool subst)
	
{
	int sts;

	if (subst) {
		char target[PATH_MAX];
		char link[PATH_MAX];
		for (sts = 0 ; sts == 0 && iter != end ; iter++) {
			putpath(uid, gid, target, iter->target);
			putpath(uid, gid, link, iter->link);
			sts = make_link(uid, gid, target, link, iter->mode, iter->label);
		}
	}
	else {
		for (sts = 0 ; sts == 0 && iter != end ; iter++)
			sts = make_link(uid, gid, iter->target, iter->link, iter->mode, iter->label);
	}
	return sts;
}


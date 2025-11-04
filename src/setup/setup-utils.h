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

#pragma once

#include <stdbool.h>
#include <sys/types.h>

/* print the error and return 1 */
__attribute__((format(printf, 1, 2)))
int err(
	const char *fmt,
	...);

/* put the ascii decimal representation of value at head
 * returns the position of the character after the number */
char *putuint(
	char     *head,
	unsigned  value);

/* put in dest the src where %u and %g are replaced by uid and gid */
void putpath(
	uid_t       uid,
	gid_t       gid,
	char       *dest,
	const char *src);

/* set SMACK label and transmute to path */
int setsmack(
	const char *path,
	const char *label,
	bool        transmute
);

/* create the symbolic link pointing target
 * with uid, gid, mode and SMACK label */
int make_link(
	uid_t       uid,
	gid_t       gid,
	const char *target,
	const char *link,
	mode_t      mode,
	const char *label
);

/* create the directory of path with uid, gid,
 * mode, SMACK label and SMACK transmute */
int make_dir(
	uid_t       uid,
	gid_t       gid,
	const char *path,
	mode_t      mode,
	const char *label,
	bool        transmute
);

/* read the file of path and put it in content.
 * return 0 on success, 1 on error */
int read_file(
	const char  *path,
	char       **content);

/* if memo is NULL init it with content of file of path
 * init content with the memo
 * return 0 on success, 1 on error */
int read_once(
	const char **content,
	char       **memo,
	const char  *path);

/* read /etc/passwd and put it in content.
 * return 0 on success, 1 on error */
int read_passwd(
	const char **content);

/* read /etc/group and put it in content.
 * return 0 on success, 1 on error */
int read_group(
	const char **content);

/* get user uid and optionaly gid if gid isn't NULL
 * return 0 on success, 1 on error */
int get_user(
	const char *user,
	uid_t      *uid,
	gid_t      *gid);

/* get group gid
 * return 0 on success, 1 on error */
int get_group(
	const char *group,
	gid_t      *gid);

struct descdir {
	const char *path;
	mode_t      mode;
	const char *label;
	bool        transmute;
};

/* setup an array of directories
 * return 0 on success, 1 on error */
int setup_dirs(
	uid_t                 uid,
	gid_t                 gid,
	const struct descdir *head,
	const struct descdir *end,
	bool                  subst
);

struct desclink {
	const char *target;
	const char *link;
	mode_t      mode;
	const char *label;
};

/* setup an array of links
 * return 0 on success, 1 on error */
int setup_links(
	uid_t                  uid,
	gid_t                  gid,
	const struct desclink *head,
	const struct desclink *end,
	bool                   subst
);


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

#include <sys/stat.h>

#include "setup-utils.h"

#define LABEL_APP_SHARED "User:App-Shared"
#define LABEL_SYS_SHARED "System:Shared"

const struct descdir dirs[] =
{
	{ AFM_USERS_RUNDIR"/%u",           0700, LABEL_APP_SHARED, true },
	{ AFM_USERS_RUNDIR"/%u/usrshr",    0700, LABEL_APP_SHARED, false },
	{ AFM_USERS_RUNDIR"/%u/apis",      0700, LABEL_SYS_SHARED, true },
	{ AFM_USERS_RUNDIR"/%u/apis/link", 0700, LABEL_SYS_SHARED, false },
	{ AFM_USERS_RUNDIR"/%u/apis/ws",   0700, LABEL_SYS_SHARED, false }
};

const struct desclink lns[] =
{
	{
		AFM_PLATFORM_RUNDIR"/display/wayland-0",
		AFM_USERS_RUNDIR"/%u/wayland-0",
		0700, LABEL_APP_SHARED
	}
};

int setup(uid_t uid, gid_t gid)
{
	int sts;

	sts = setup_dirs(uid, gid, dirs, &dirs[sizeof dirs / sizeof *dirs], true);
	if (sts == 0)
		sts = setup_links(uid, gid, lns, &lns[sizeof lns / sizeof *lns], true);
	return sts;
}

int setup_user(const char *user)
{
	int sts;
	uid_t uid;
	gid_t gid;

	sts = get_user(user, &uid, &gid);
	return sts ? sts : setup(uid, gid);
}

int main(int ac, char **av)
{
	umask(0022);
	switch (ac) {
	case 2: return setup_user(av[1]);
	default: return err("usage: afm-user-setup user");
	}
}


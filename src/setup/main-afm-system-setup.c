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

struct descdir root_dirs[] =
{
	{ AFM_USERS_RUNDIR,                0755, LABEL_SYS_SHARED, true }
};

struct descdir platform_dirs[] =
{
	{ AFM_PLATFORM_RUNDIR,             0755, LABEL_SYS_SHARED, true },
	{ AFM_PLATFORM_RUNDIR"/display",   0755, LABEL_SYS_SHARED, true },
	{ AFM_PLATFORM_RUNDIR"/apis",      0755, LABEL_SYS_SHARED, true },
	{ AFM_PLATFORM_RUNDIR"/apis/ws",   0755, LABEL_SYS_SHARED, false },
	{ AFM_PLATFORM_RUNDIR"/apis/link", 0755, LABEL_SYS_SHARED, false },
#if WITH_PLATFORM_DEBUG
	{ AFM_PLATFORM_RUNDIR"/debug",     0755, LABEL_SYS_SHARED, false },
#endif
	{ AFM_PLATFORM_DATADIR,            0755, LABEL_APP_SHARED, true },
	{ AFM_PLATFORM_DATADIR"/share",    0755, LABEL_SYS_SHARED, false }
};

int main(int ac, char **av)
{
	int sts;
	uid_t uid;
	gid_t gid;

	umask(0022);
	sts = setup_dirs(0, 0, root_dirs, &root_dirs[sizeof root_dirs / sizeof *root_dirs], false);
	if (sts == 0) {
		sts = get_user(AFM_PLATFORM_USER, &uid, &gid);
		if (sts == 0)
			sts = setup_dirs(uid, gid, platform_dirs, &platform_dirs[sizeof platform_dirs / sizeof *platform_dirs], false);
	}
	return sts;
}


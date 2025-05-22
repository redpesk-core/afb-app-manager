/*
 Copyright (C) 2015-2025 IoT.bzh Company

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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-file.h>

#include "utils-systemd.h"
#include "unit-desc.h"

static void stop_that_unit_cb(void *closure, struct SysD_ListUnitItem *lui)
{
	int rc, isuser;

	switch (systemd_state_of_name(lui->active_state)) {
	case SysD_State_Activating:
	case SysD_State_Active:
	case SysD_State_Reloading:
		isuser = (int)(intptr_t)closure;
		rc = systemd_unit_stop_dpath(isuser, lui->opath, 0);
		if (rc < 0)
			RP_ERROR("can't stop %s", lui->name);
		break;
	case SysD_State_INVALID:
	case SysD_State_Inactive:
	case SysD_State_Deactivating:
	case SysD_State_Failed:
	default:
		break;
	}
}

int unit_oper_stop(const struct unitdesc *unit)
{
	int rc;
	char serv[PATH_MAX + 1];
	int isuser = unit->scope == unitscope_user;

	rc = unit_desc_get_service(unit, serv, sizeof serv);
	if (rc >= 0) {
		RP_INFO("stopping unit(s) %s", serv);
		rc = systemd_list_unit_pattern(isuser, serv,
		                               stop_that_unit_cb,
		                               (void*)(intptr_t)isuser);
		if (rc < 0)
			RP_ERROR("issue found when stopping unit(s) %s", serv);
	}

	return rc;
}

int unit_oper_uninstall(const struct unitdesc *unit)
{
	int rc, rc2;
	char path[PATH_MAX];

	rc = unit_desc_check(unit, 0);
	if (rc == 0) {
		unit_oper_stop(unit);
		rc = unit_desc_get_path(unit, path, sizeof path);
		if (rc >= 0) {
			RP_INFO("uninstalling unit %s", path);
			rc = unlink(path);
			if (rc < 0)
				RP_ERROR("can't unlink %s", path);
		}
		if (unit->wanted_by != NULL) {
			rc2 = unit_desc_get_wants_path(unit, path, sizeof path);
			if (rc2 >= 0) {
				rc2 = unlink(path);
				if (rc2 < 0)
					RP_ERROR("can't unlink %s", path);
			}
			if (rc2 < 0 && rc == 0)
				rc = rc2;
		}
	}
	return rc;
}

int unit_oper_install(const struct unitdesc *unit)
{
	int rc;
	char path[PATH_MAX + 1], target[PATH_MAX + 1];

	rc = unit_desc_check(unit, 1);
	if (rc == 0) {
		rc = unit_desc_get_path(unit, path, sizeof path);
		if (rc >= 0) {
			RP_INFO("installing unit %s", path);
			rc = rp_file_put(path, unit->content, unit->content_length);
			if (rc >= 0 && unit->wanted_by != NULL) {
				rc = unit_desc_get_wants_path(unit, path, sizeof path);
				if (rc >= 0) {
					rc = unit_desc_get_wants_target(unit, target, sizeof target);
					if (rc >= 0) {
						unlink(path);
						rc = symlink(target, path);
					}
				}
			}
		}
	}
	return rc;
}


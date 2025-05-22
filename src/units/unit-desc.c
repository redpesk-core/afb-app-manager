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

#include <stdio.h>
#include <errno.h>

#include <rp-utils/rp-verbose.h>

#include "unit-fs.h"
#include "unit-desc.h"

/**
 * check the unit: verify that scope, type and name are set
 *
 * @param desc  unit description
 * @param tells output errors if existing
 *
 * @return 0 if the unit is valid or else -1
 */
int unit_desc_check(const struct unitdesc *desc, int tells)
{
	if (desc->scope != unitscope_unknown
	 && desc->type != unittype_unknown
	 && desc->name != NULL
	 && desc->name_length > 0)
		return 0;

	if (tells) {
		if (desc->scope == unitscope_unknown)
			RP_ERROR("unit of unknown scope");
		if (desc->type == unittype_unknown)
			RP_ERROR("unit of unknown type");
		if (desc->name == NULL)
			RP_ERROR("unit of unknown name");
	}
	errno = EINVAL;
	return -1;
}

int unit_desc_get_path(const struct unitdesc *desc, char *path, size_t pathlen)
{
	const char *ext = desc->type == unittype_socket ? "socket" : "service";
	int isuser = desc->scope == unitscope_user;
	int rc = units_fs_get_afm_unit_path(path, pathlen, isuser, desc->name, ext);

	if (rc < 0)
		RP_ERROR("can't get the unit path for %s", desc->name);

	return rc;
}

int unit_desc_get_wants_path(const struct unitdesc *desc, char *path, size_t pathlen)
{
	const char *ext = desc->type == unittype_socket ? "socket" : "service";
	int isuser = desc->scope == unitscope_user;
	int rc = units_fs_get_afm_wants_unit_path(path, pathlen, isuser, desc->wanted_by,
	                                       desc->name, ext);

	if (rc < 0)
		RP_ERROR("can't get the wants path for %s and %s", desc->name, desc->wanted_by);

	return rc;
}

int unit_desc_get_wants_target(const struct unitdesc *desc, char *path, size_t pathlen)
{
	const char *ext = desc->type == unittype_socket ? "socket" : "service";
	int rc = units_fs_get_wants_target(path, pathlen, desc->name, ext);

	if (rc < 0)
		RP_ERROR("can't get the wants target for %s", desc->name);

	return rc;
}

int unit_desc_get_service(const struct unitdesc *desc, char *serv, size_t servlen)
{
	static const char star[2] = { '*', 0 };
	const char *ext = desc->type == unittype_socket ? "socket" : "service";
	int off = desc->name_length == 0 || desc->name[desc->name_length - 1] != '@';
	int rc = snprintf(serv, servlen, "%s%s.%s", desc->name , &star[off], ext);
	if (rc >= 0 && (size_t)rc >= servlen) {
		errno = ENAMETOOLONG;
		rc = -1;
	}
	if (rc < 0)
		RP_ERROR("can't get service for %s", desc->name);

	return rc;
}


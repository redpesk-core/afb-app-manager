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

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <rp-utils/rp-verbose.h>
#include "wgtpkg-permissions.h"

struct permission {
	char *name;
	unsigned granted: 1;
	unsigned requested: 1;
	unsigned level: 3;
};

static unsigned int nrpermissions = 0;
static struct permission *permissions = NULL;
static unsigned int indexiter = 0;

/* retrieves the permission of name */
static struct permission *get_permission(const char *name)
{
	unsigned int i;

	for (i = 0 ; i < nrpermissions ; i++)
		if (0 == strcmp(permissions[i].name, name))
			return permissions+i;
	return NULL;
}

/* add a permission of name */
static struct permission *add_permission(const char *name)
{
	struct permission *p = get_permission(name);
	if (!p) {
		p = realloc(permissions,
			((nrpermissions + 8) & ~(unsigned)7) * sizeof(*p));
		if (p) {
			permissions = p;
			p = permissions + nrpermissions;
			memset(p, 0, sizeof(*p));
			p->name = strdup(name);
			if (!p->name)
				p = NULL;
			else
				nrpermissions++;
		}
	}
	return p;
}

/* remove any granting */
void reset_permissions()
{
	unsigned int i;
	for (i = 0 ; i < nrpermissions ; i++)
		permissions[i].granted = permissions[i].requested = 0;
}

/* remove any requested permission */
void reset_requested_permissions()
{
	unsigned int i;
	for (i = 0 ; i < nrpermissions ; i++)
		permissions[i].requested = 0;
}

/* remove any granting */
void crop_permissions(unsigned level)
{
	unsigned int i;
	for (i = 0 ; i < nrpermissions ; i++)
		if (permissions[i].level < level)
			permissions[i].granted = 0;
}

/* add permissions granted for installation */
int grant_permission_list(const char *list)
{
	struct permission *p;
	char *iter, c;
	unsigned int n;
	static const char separators[] = " \t\n\r,";

	iter = strdupa(list);
	iter += strspn(iter, separators);
	while(*iter) {
		n = (unsigned)strcspn(iter, separators);
		c = iter[n];
		iter[n] = 0;
		p = add_permission(iter);
		if (!p) {
			errno = ENOMEM;
			return -1;
		}
		p->granted = 1;
		iter += n;
		*iter =c;
		iter += strspn(iter, separators);
	}
	return 0;
}

/* checks if the permission 'name' is recorded */
int permission_exists(const char *name)
{
	return !!get_permission(name);
}

/* request the permission, returns 1 if granted or 0 otherwise */
int request_permission(const char *name)
{
#define HACK_ALLOWING_REQUESTED_PERMISSIONS 1

	struct permission *p = get_permission(name);

#if defined(HACK_ALLOWING_REQUESTED_PERMISSIONS) && HACK_ALLOWING_REQUESTED_PERMISSIONS
	if (!p)
		p = add_permission(name);
#endif
	if (p) {
#if defined(HACK_ALLOWING_REQUESTED_PERMISSIONS) && HACK_ALLOWING_REQUESTED_PERMISSIONS
		p->granted = 1;
#endif
		p->requested = 1;
		if (p->granted)
			return 1;
	}
	return 0;

#undef HACK_ALLOWING_REQUESTED_PERMISSIONS
}

/* iteration over granted and requested permissions */
const char *first_usable_permission()
{
	indexiter = 0;
	return next_usable_permission();
}

const char *next_usable_permission()
{
	while(indexiter < nrpermissions) {
		struct permission *p = &permissions[indexiter++];
		if (p->granted && p->requested)
			return p->name;
	}
	return NULL;
}


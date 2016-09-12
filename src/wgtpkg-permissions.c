/*
 Copyright 2015 IoT.bzh

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

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "verbose.h"
#include "wgtpkg-permissions.h"

struct permission {
	char *name;
	unsigned granted: 1;
	unsigned requested: 1;
	unsigned level: 3;
};

static const char prefix_of_permissions[] = FWK_PREFIX_PERMISSION;

static unsigned int nrpermissions = 0;
static struct permission *permissions = NULL;
static unsigned int indexiter = 0;

/* check is the name has the correct prefix for permissions */
int is_standard_permission(const char *name)
{
	return 0 == memcmp(name, prefix_of_permissions, sizeof(prefix_of_permissions) - 1);
}

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
	struct permission *p = get_permission(name);
	if (p) {
		p->requested = 1;
		if (p->granted)
			return 1;
	}
	return 0;
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


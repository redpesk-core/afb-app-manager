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

#include <string.h>

#include "path-type.h"
#include "path-entry.h"

static
struct {
	path_type_t type;
	const char *key;
	const char *dir;
}
filetypes[] = {
	{ path_type_Conf,		"config",		"etc" },
	{ path_type_Data,		"data",			NULL },
	{ path_type_Exec,		"executable",		"bin" },
	{ path_type_Http,		"www",			"htdocs" },
	{ path_type_Lib,		"library",		"lib" },
	{ path_type_Public,		"public",		"public" },
	{ path_type_Public_Exec,	"public-executable",	NULL },
	{ path_type_Public_Lib,		"public-library",	NULL },
};

path_type_t path_type_of_key(const char *key)
{
	unsigned i = sizeof filetypes / sizeof *filetypes;
	while (i)
		if (!strcmp(key, filetypes[--i].key))
			return filetypes[i].type;
	return path_type_Unknown;
}

path_type_t path_type_of_dirname(const char *dir)
{
	unsigned i = sizeof filetypes / sizeof *filetypes;
	while (i)
		if (filetypes[--i].dir != NULL)
			if (!strcmp(dir, filetypes[i].dir))
				return filetypes[i].type;
	return path_type_Unknown;
}

path_type_t path_type_of_entry(const path_entry_t *entry, const path_entry_t *root)
{
	path_type_t type = path_type_Unknown;

	if (!path_entry_has_child(entry))
		entry = path_entry_parent(entry);

	while (type == path_type_Unknown && entry != root) {
		type = path_type_of_dirname(path_entry_name(entry));
		entry = path_entry_parent(entry);
	}
	return type;
}
/*
 Copyright (C) 2015-2023 IoT.bzh Company

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

#pragma once

#include "path-entry.h"

typedef
enum path_type
{
    path_type_Unset = 0,
    path_type_Unknown,
    path_type_Conf,
    path_type_Data,
    path_type_Exec,
    path_type_Http,
    path_type_Icon,
    path_type_Id,
    path_type_Lib,
    path_type_Public,
    path_type_Public_Exec,
    path_type_Public_Lib
}
	path_type_t;

extern path_type_t path_type_of_key(const char *key);

extern path_type_t path_type_of_dirname(const char *dir);

extern path_type_t path_type_of_entry(const path_entry_t *entry, const path_entry_t *root);

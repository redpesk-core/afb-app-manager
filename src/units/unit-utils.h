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

#pragma once

extern const char *units_set_root_dir(const char *dir);
extern int units_get_afm_units_dir(char *path, size_t pathlen, int isuser);
extern int units_get_afm_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext);
extern int units_get_afm_wants_unit_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext);
extern int units_get_wants_target(char *path, size_t pathlen, const char *unit, const char *uext);
extern int units_list(int isuser, int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure);
extern int units_list_all(int (*callback)(void *closure, const char *name, const char *path, int isuser), void *closure);


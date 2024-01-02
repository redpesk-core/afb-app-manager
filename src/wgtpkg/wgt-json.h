/*
 Copyright (C) 2015-2024 IoT.bzh Company

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

struct wgt;
struct wgt_info;
struct json_object;

extern struct json_object *wgt_info_to_json(struct wgt_info *info);
extern struct json_object *wgt_to_json(struct wgt *wgt);
extern struct json_object *wgt_path_at_to_json(int dfd, const char *path);
extern struct json_object *wgt_path_to_json(const char *path);
extern struct json_object *wgt_config_to_json(const char *path);


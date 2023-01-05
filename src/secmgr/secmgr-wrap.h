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

extern int secmgr_begin(const char *id);
extern void secmgr_end();
extern int secmgr_install();
extern int secmgr_uninstall();
extern int secmgr_permit(const char *permission);

extern int secmgr_path_id(const char *pathname);
extern int secmgr_path_lib(const char *pathname);
extern int secmgr_path_conf(const char *pathname);
extern int secmgr_path_data(const char *pathname);
extern int secmgr_path_exec(const char *pathname);
extern int secmgr_path_http(const char *pathname);
extern int secmgr_path_icon(const char *pathname);
extern int secmgr_path_public(const char *pathname);
extern int secmgr_path_remove(const char *pathname);

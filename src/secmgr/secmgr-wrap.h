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

extern int secmgr_begin(const char *id);
extern void secmgr_end();
extern int secmgr_install();
extern int secmgr_uninstall();
extern int secmgr_permit(const char *permission);

extern int secmgr_path(const char *pathname, const char *pathtype);
extern int secmgr_plug(const char *expdir, const char *impid, const char *impdir);

extern const char secmgr_pathtype_conf[];
extern const char secmgr_pathtype_data[];
extern const char secmgr_pathtype_exec[];
extern const char secmgr_pathtype_http[];
extern const char secmgr_pathtype_icon[];
extern const char secmgr_pathtype_lib[];
extern const char secmgr_pathtype_plug[];
extern const char secmgr_pathtype_public[];
extern const char secmgr_pathtype_id[];
extern const char secmgr_pathtype_default[];



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

struct afm_udb;

extern int afm_urun_start(struct json_object *appli, int uid);
extern int afm_urun_once(struct json_object *appli, int uid);
extern int afm_urun_terminate(int runid, int uid);
extern int afm_urun_pause(int runid, int uid);
extern int afm_urun_resume(int runid, int uid);
extern struct json_object *afm_urun_list(struct afm_udb *db, int all, int uid);
extern struct json_object *afm_urun_state(struct afm_udb *db, int runid, int uid);
extern int afm_urun_search_runid(struct afm_udb *db, const char *id, int uid);


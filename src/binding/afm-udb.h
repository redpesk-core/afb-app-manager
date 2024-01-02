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

struct afm_udb;
struct json_object;

extern struct afm_udb *afm_udb_create(int sys, int usr, const char *prefix);
extern void afm_udb_addref(struct afm_udb *afdb);
extern void afm_udb_unref(struct afm_udb *afdb);
extern int afm_udb_update(struct afm_udb *afdb);
extern void afm_udb_set_default_lang(const char *lang);
extern struct json_object *afm_udb_applications_private(struct afm_udb *afdb, int all, int uid);
extern struct json_object *afm_udb_get_application_private(struct afm_udb *afdb, const char *id, int uid);
extern struct json_object *afm_udb_applications_public(struct afm_udb *afdb, int all, int uid, const char *lang);
extern struct json_object *afm_udb_get_application_public(struct afm_udb *afdb, const char *id, int uid, const char *lang);


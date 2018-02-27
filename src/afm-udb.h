/*
 Copyright 2015, 2016, 2017 IoT.bzh

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

struct afm_udb;
struct json_object;

extern struct afm_udb *afm_udb_create(int sys, int usr, const char *prefix);
extern void afm_udb_addref(struct afm_udb *afdb);
extern void afm_udb_unref(struct afm_udb *afdb);
extern int afm_udb_update(struct afm_udb *afdb);
extern void afm_udb_set_default_lang(const char *lang);
extern struct json_object *afm_udb_applications_private(struct afm_udb *afdb, int uid);
extern struct json_object *afm_udb_get_application_private(struct afm_udb *afdb, const char *id, int uid);
extern struct json_object *afm_udb_applications_public(struct afm_udb *afdb, int uid, const char *lang);
extern struct json_object *afm_udb_get_application_public(struct afm_udb *afdb, const char *id, int uid, const char *lang);


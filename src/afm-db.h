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

struct afm_db;

extern struct afm_db *afm_db_create();
extern void afm_db_addref(struct afm_db *afdb);
extern void afm_db_unref(struct afm_db *afdb);

extern int afm_db_add_root(struct afm_db *afdb, const char *path);
extern int afm_db_add_application(struct afm_db *afdb, const char *path);
extern int afm_db_update_applications(struct afm_db *afdb);
extern int afm_db_ensure_applications(struct afm_db *afdb);

extern struct json_object *afm_db_application_list(struct afm_db *afdb);
extern struct json_object *afm_db_get_application(struct afm_db *afdb, const char *id);
extern struct json_object *afm_db_get_application_public(struct afm_db *afdb, const char *id);


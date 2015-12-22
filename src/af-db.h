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

struct af_db;

extern struct af_db *af_db_create();
extern void af_db_addref(struct af_db *afdb);
extern void af_db_unref(struct af_db *afdb);

extern int af_db_add_root(struct af_db *afdb, const char *path);
extern int af_db_add_application(struct af_db *afdb, const char *path);
extern int af_db_update_applications(struct af_db *afdb);
extern int af_db_ensure_applications(struct af_db *afdb);

extern struct json_object *af_db_application_list(struct af_db *afdb);
extern struct json_object *af_db_get_application(struct af_db *afdb, const char *id);
extern struct json_object *af_db_get_application_public(struct af_db *afdb, const char *id);


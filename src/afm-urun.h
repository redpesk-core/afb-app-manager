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

extern int afm_urun_start(struct json_object *appli, int uid);
extern int afm_urun_once(struct json_object *appli, int uid);
extern int afm_urun_terminate(int runid, int uid);
extern int afm_urun_pause(int runid, int uid);
extern int afm_urun_resume(int runid, int uid);
extern struct json_object *afm_urun_list(struct afm_udb *db, int uid);
extern struct json_object *afm_urun_state(struct afm_udb *db, int runid, int uid);
extern int afm_urun_search_runid(struct afm_udb *db, const char *id, int uid);


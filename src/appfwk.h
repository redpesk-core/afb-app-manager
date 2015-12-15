/*
 Copyright 2015 IoT.bzh

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

struct appfwk;

extern struct appfwk *appfwk_create();
extern void appfwk_addref(struct appfwk *appfwk);
extern void appfwk_unref(struct appfwk *appfwk);

extern int appfwk_add_root(struct appfwk *appfwk, const char *path);
extern int appfwk_update_applications(struct appfwk *af);
extern int appfwk_ensure_applications(struct appfwk *af);

extern struct json_object *appfwk_application_list(struct appfwk *af);
extern struct json_object *appfwk_get_application(struct appfwk *af, const char *id);
extern struct json_object *appfwk_get_application_public(struct appfwk *af, const char *id);

extern const char *appfwk_start(struct appfwk *af, const char *appid);
extern int appfwk_stop(struct appfwk *af, const char *runid);
extern int appfwk_suspend(struct appfwk *af, const char *runid);
extern int appfwk_resume(struct appfwk *af, const char *runid);
extern struct json_object *appfwk_running_list(struct appfwk *af);
extern struct json_object *appfwk_state(struct appfwk *af, const char *runid);

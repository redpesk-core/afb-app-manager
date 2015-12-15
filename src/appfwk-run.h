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

extern int appfwk_run_start(struct json_object *appli);
extern int appfwk_run_stop(int runid);
extern int appfwk_run_suspend(int runid);
extern int appfwk_run_resume(int runid);
extern struct json_object *appfwk_run_list();
extern struct json_object *appfwk_run_state(int runid);


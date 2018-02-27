/*
 Copyright (C) 2015-2018 IoT.bzh

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

struct wgt;
struct wgt_info;
struct json_object;

extern struct json_object *wgt_info_to_json(struct wgt_info *info);
extern struct json_object *wgt_to_json(struct wgt *wgt);
extern struct json_object *wgt_path_at_to_json(int dfd, const char *path);
extern struct json_object *wgt_path_to_json(const char *path);


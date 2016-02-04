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

int j_object(struct json_object *obj, const char *key, struct json_object **value);
int j_string(struct json_object *obj, const char *key, const char **value);
int j_boolean(struct json_object *obj, const char *key, int *value);
int j_integer(struct json_object *obj, const char *key, int *value);

extern const char *j_get_string(struct json_object *obj, const char *key, const char *defval);
extern int j_get_boolean(struct json_object *obj, const char *key, int defval);
extern int j_get_integer(struct json_object *obj, const char *key, int defval);

extern int j_add(struct json_object *obj, const char *key, struct json_object *val);
extern int j_add_string(struct json_object *obj, const char *key, const char *val);
extern int j_add_boolean(struct json_object *obj, const char *key, int val);
extern int j_add_integer(struct json_object *obj, const char *key, int val);


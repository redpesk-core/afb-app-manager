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

#include <errno.h>

#include <json.h>

#include "utils-json.h"

int j_object(struct json_object *obj, const char *key, struct json_object **value)
{
	return json_object_object_get_ex(obj, key, value);
}

int j_string(struct json_object *obj, const char *key, const char **value)
{
	json_object *data;
	return j_object(obj, key, &data)
		&& json_object_get_type(data) == json_type_string
		&& (*value = json_object_get_string(data)) != NULL;
}

int j_boolean(struct json_object *obj, const char *key, int *value)
{
	json_object *data;
	return json_object_object_get_ex(obj, key, &data)
		&& json_object_get_type(data) == json_type_boolean
		&& ((*value = (int)json_object_get_boolean(data)), 1);
}

int j_integer(struct json_object *obj, const char *key, int *value)
{
	json_object *data;
	return json_object_object_get_ex(obj, key, &data)
		&& json_object_get_type(data) == json_type_int
		&& ((*value = (int)json_object_get_int(data)), 1);
}

const char *j_get_string(struct json_object *obj, const char *key, const char *defval)
{
	struct json_object *o;
	return json_object_object_get_ex(obj, key, &o)
		&& json_object_get_type(o) == json_type_string
			? json_object_get_string(o) : defval;
}

int j_get_boolean(struct json_object *obj, const char *key, int defval)
{
	struct json_object *o;
	return json_object_object_get_ex(obj, key, &o)
		&& json_object_get_type(o) == json_type_boolean
			? json_object_get_boolean(o) : defval;
}

int j_get_integer(struct json_object *obj, const char *key, int defval)
{
	struct json_object *o;
	return json_object_object_get_ex(obj, key, &o)
		&& json_object_get_type(o) == json_type_int
			? json_object_get_int(o) : defval;
}

int j_add(struct json_object *obj, const char *key, struct json_object *val)
{
	json_object_object_add(obj, key, val);
	return 1;
}

int j_add_string(struct json_object *obj, const char *key, const char *val)
{
	struct json_object *str = json_object_new_string (val ? val : "");
	return str ? j_add(obj, key, str) : (errno = ENOMEM, 0);
}

int j_add_boolean(struct json_object *obj, const char *key, int val)
{
	struct json_object *str = json_object_new_boolean (val);
	return str ? j_add(obj, key, str) : (errno = ENOMEM, 0);
}

int j_add_integer(struct json_object *obj, const char *key, int val)
{
	struct json_object *str = json_object_new_int (val);
	return str ? j_add(obj, key, str) : (errno = ENOMEM, 0);
}


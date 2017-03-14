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

#define _GNU_SOURCE

#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include <json-c/json.h>

#include "utils-json.h"

int j_read_string(struct json_object *obj, const char **value)
{
	return j_is_string(obj) && (*value = json_object_get_string(obj)) != NULL;
}

int j_read_boolean(struct json_object *obj, int *value)
{
	return j_is_boolean(obj) && ((*value = (int)json_object_get_boolean(obj)), 1);
}

int j_read_integer(struct json_object *obj, int *value)
{
	return j_is_integer(obj) && ((*value = (int)json_object_get_int(obj)), 1);
}

const char *j_string(struct json_object *obj, const char *defval)
{
	return j_is_string(obj) ? json_object_get_string(obj) : defval;
}

int j_boolean(struct json_object *obj, int defval)
{
	return j_is_boolean(obj) ? json_object_get_boolean(obj) : defval;
}

int j_integer(struct json_object *obj, int defval)
{
	return j_is_integer(obj) ? json_object_get_int(obj) : defval;
}

int j_has(struct json_object *obj, const char *key)
{
	return json_object_object_get_ex(obj, key, NULL);
}

int j_read_object_at(struct json_object *obj, const char *key, struct json_object **value)
{
	return json_object_object_get_ex(obj, key, value);
}

int j_read_string_at(struct json_object *obj, const char *key, const char **value)
{
	json_object *data;
	return j_read_object_at(obj, key, &data) && j_read_string(data, value);
}

int j_read_boolean_at(struct json_object *obj, const char *key, int *value)
{
	json_object *data;
	return j_read_object_at(obj, key, &data) && j_read_boolean(data, value);
}

int j_read_integer_at(struct json_object *obj, const char *key, int *value)
{
	json_object *data;
	return j_read_object_at(obj, key, &data) && j_read_integer(data, value);
}

const char *j_string_at(struct json_object *obj, const char *key, const char *defval)
{
	struct json_object *data;
	return j_read_object_at(obj, key, &data) ? j_string(data, defval) : defval;
}

int j_boolean_at(struct json_object *obj, const char *key, int defval)
{
	struct json_object *data;
	return j_read_object_at(obj, key, &data) ? j_boolean(data, defval) : defval;
}

int j_integer_at(struct json_object *obj, const char *key, int defval)
{
	struct json_object *data;
	return j_read_object_at(obj, key, &data) ? j_integer(data, defval) : defval;
}

int j_add(struct json_object *obj, const char *key, struct json_object *val)
{
	if (key)
		json_object_object_add(obj, key, val);
	else
		json_object_array_add(obj, val);
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

struct json_object *j_add_new_array(struct json_object *obj, const char *key)
{
	struct json_object *result = json_object_new_array();
	if (result != NULL && !j_add(obj, key, result)) {
		json_object_put(result);
		result = NULL;
	}
	return result;
}

struct json_object *j_add_new_object(struct json_object *obj, const char *key)
{
	struct json_object *result = json_object_new_object();
	if (result != NULL && !j_add(obj, key, result)) {
		json_object_put(result);
		result = NULL;
	}
	return result;
}

int j_enter_m(struct json_object **obj, const char **mkey, int create)
{
	char *n;
	struct json_object *o, *x;
	const char *f, *k;

	k = *mkey;
	f = strchr(k, '.');
	if (f) {
		o = *obj;
		do {
			n = strndupa(k, (size_t)(f - k));
			if (!json_object_object_get_ex(o, n, &x)) {
				if (!create)
					return -ENOENT;
				x = j_add_new_object(o, n);
				if (!x)
					return -ENOMEM;
			}
			o = x;
			k = f + 1;
			f = strchr(k, '.');
		} while(f);
		*obj = o;
		*mkey = k;
	}
	return 0;
}


int j_has_m(struct json_object *obj, const char *mkey)
{
	return !j_enter_m(&obj, &mkey, 0) && j_has(obj, mkey);
}

int j_read_object_at_m(struct json_object *obj, const char *mkey, struct json_object **value)
{
	return !j_enter_m(&obj, &mkey, 0) && j_read_object_at(obj, mkey, value);
}

int j_read_string_at_m(struct json_object *obj, const char *mkey, const char **value)
{
	json_object *data;
	return j_read_object_at_m(obj, mkey, &data) && j_read_string(data, value);
}

int j_read_boolean_at_m(struct json_object *obj, const char *mkey, int *value)
{
	json_object *data;
	return j_read_object_at_m(obj, mkey, &data) && j_read_boolean(data, value);
}

int j_read_integer_at_m(struct json_object *obj, const char *mkey, int *value)
{
	json_object *data;
	return j_read_object_at_m(obj, mkey, &data) && j_read_integer(data, value);
}

const char *j_string_at_m(struct json_object *obj, const char *mkey, const char *defval)
{
	struct json_object *data;
	return j_read_object_at_m(obj, mkey, &data) ? j_string(data, defval) : defval;
}

int j_boolean_at_m(struct json_object *obj, const char *mkey, int defval)
{
	struct json_object *data;
	return j_read_object_at_m(obj, mkey, &data) ? j_boolean(data, defval) : defval;
}

int j_integer_at_m(struct json_object *obj, const char *mkey, int defval)
{
	struct json_object *data;
	return j_read_object_at_m(obj, mkey, &data) ? j_integer(data, defval) : defval;
}

int j_add_m(struct json_object *obj, const char *mkey, struct json_object *val)
{
	if (mkey)
		return !j_enter_m(&obj, &mkey, 1) && j_add(obj, mkey, val);
	json_object_array_add(obj, val);
	return 1;
}

int j_add_string_m(struct json_object *obj, const char *mkey, const char *val)
{
	struct json_object *str = json_object_new_string (val ? val : "");
	return str ? j_add_m(obj, mkey, str) : (errno = ENOMEM, 0);
}

int j_add_boolean_m(struct json_object *obj, const char *mkey, int val)
{
	struct json_object *str = json_object_new_boolean (val);
	return str ? j_add_m(obj, mkey, str) : (errno = ENOMEM, 0);
}

int j_add_integer_m(struct json_object *obj, const char *mkey, int val)
{
	struct json_object *str = json_object_new_int (val);
	return str ? j_add_m(obj, mkey, str) : (errno = ENOMEM, 0);
}

struct json_object *j_add_new_array_m(struct json_object *obj, const char *mkey)
{
	struct json_object *result = json_object_new_array();
	if (result != NULL && !j_add_m(obj, mkey, result)) {
		json_object_put(result);
		result = NULL;
	}
	return result;
}

struct json_object *j_add_new_object_m(struct json_object *obj, const char *mkey)
{
	struct json_object *result = json_object_new_object();
	if (result != NULL && !j_add_m(obj, mkey, result)) {
		json_object_put(result);
		result = NULL;
	}
	return result;
}

int j_add_many_strings_m(struct json_object *obj, ...)
{
	const char *key, *val;
	va_list ap;
	int rc;

	rc = 1;
	va_start(ap, obj);
	key = va_arg(ap, const char *);
	while (key) {
		val = va_arg(ap, const char *);
		if (val) {
			if (!j_add_string_m(obj, key, val))
				rc = 0;
		}
		key = va_arg(ap, const char *);
	}
	va_end(ap);
	return rc;
}

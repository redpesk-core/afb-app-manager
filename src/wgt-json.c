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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include <json-c/json.h>

#include "utils-json.h"
#include "wgt-info.h"
#include "wgt-json.h"
#include "wgt-strings.h"
#include "verbose.h"

/*
{
  permissions: {
	dict: {
		ID: { name: ID, level: LEVEL, index: INDEX },
		...
	},
	list: [
		{ name: ID, level: LEVEL, index: 0 },
		...
	}
  },
  targets: [
		{ name: ID, level: LEVEL, index: 0 },
		...
	]
  }
}
*/

struct paramaction
{
	const char *name;
	int (*action)(struct json_object *obj, const struct wgt_desc_param *param, void *closure);
	void *closure;
};


/* apply params */
static int apply_params(struct json_object *obj, const struct wgt_desc_param *param, const struct paramaction *actions)
{
	int rc;
	const struct paramaction *a;

	if (!obj)
		rc = -1;
	else {
		rc = 0;
		while(param) {
			for (a = actions ; a->name && strcmp(a->name, param->name) ; a++);
			if (a->action && a->action(obj, param, a->closure) < 0)
				rc = -1;
			param = param->next;
		}
	}
	return rc;
}

static int get_array(struct json_object **result, struct json_object *obj, const char *key, int create)
{
	int rc = j_enter_m(&obj, &key, 1);
	if (rc < 0)
		*result = NULL;
	else if (!json_object_object_get_ex(obj, key, result)) {
		if (!create) {
			*result = NULL;
			rc = -ENOENT;
		} else {
			if (!(*result = j_add_new_array(obj, key)))
				rc = -ENOMEM;
		}
	}
	return rc;
}

/* get the param of 'name' for the feature 'feat' */
static const char *get_target_name(const struct wgt_desc_feature *feat, const char *defval)
{
	const struct wgt_desc_param *param = feat->params;
	while (param && strcmp(param->name, string_sharp_target))
		param = param->next;
	if (param) {
		defval = param->value;
		param = param->next;
		while (param) {
			if (!strcmp(param->name, string_sharp_target)) {
				defval = NULL;
				break;
			}
			param = param->next;
		}
	}
	return defval;
}

/* get the target */
static struct json_object *get_array_item_by_key(struct json_object *array, const char *key, const char *val)
{
	struct json_object *result, *k;
	int i, n;

	n = json_object_array_length(array);
	for (i = 0 ; i < n ; i++) {
		result = json_object_array_get_idx(array, i);
		if (result && json_object_object_get_ex(result, key, &k)
			&& json_object_get_type(k) == json_type_string
			&& !strcmp(json_object_get_string(k), val))
			return result;
	}
	return NULL;
}

static int get_target(struct json_object **result, struct json_object *targets, const char *name, int create)
{
	int rc;
	struct json_object *t;

	t = get_array_item_by_key(targets, string_sharp_target, name);
	if (t) {
		if (!create)
			rc = 0;
		else {
			ERROR("duplicated target name: %s", name);
			t = NULL;
			rc = -EEXIST;
		}
	} else {
		if (!create) {
			ERROR("target name not found: %s", name);
			rc = -ENOENT;
		} else {
			t = j_add_new_object(targets, NULL);
			if (t && j_add_string(t, string_sharp_target, name))
				rc = 0;
			else {
				json_object_put(t);
				t = NULL;
				rc = -ENOMEM;
			}
		}
	}
	*result = t;
	return rc;
}

static int make_target(struct json_object *targets, const struct wgt_desc_feature *feat)
{
	const char *id;
	struct json_object *target;

	id = get_target_name(feat, NULL);
	if (id == NULL) {
		ERROR("target of feature %s is missing or repeated", feat->name);
		return -EINVAL;
	}

	return get_target(&target, targets, id, 1);
}

static int add_icon(struct json_object *target, const char *src, int width, int height)
{
	int rc;
	struct json_object *icon, *array;

	rc = get_array(&array, target, string_icon, 1);
	if (rc >= 0) {
		icon = json_object_new_object();
		if(!icon
		|| !j_add_string(icon, string_src, src)
		|| (width > 0 && !j_add_integer(icon, string_width, width))
		|| (height > 0 && !j_add_integer(icon, string_height, height))
		|| !j_add(array, NULL, icon)) {
			json_object_put(icon);
			rc = -ENOMEM;
		}
	}
	return rc;
}

static int make_main_target(struct json_object *targets, const struct wgt_desc *desc)
{
	int rc;
	struct wgt_desc_icon *icon;
	struct json_object *target;

	/* create the target 'main' */
	rc = get_target(&target, targets, string_main, 1);

	/* adds icons if any */
	icon = desc->icons;
	while (rc >= 0 && icon) {
		rc = add_icon(target, icon->src, icon->width, icon->height);
		icon = icon->next;
	}

	if (rc >= 0)
		rc = j_add_many_strings_m(target,
			"content.src", desc->content_src,
			"content.type", desc->content_type,
			"content.encoding", desc->content_encoding,
			NULL) ? 0 : -errno;

	if (rc >= 0)
		if((desc->width && !j_add_integer(target, string_width, desc->width))
		|| (desc->height && !j_add_integer(target, string_height, desc->height)))
			rc = -ENOMEM;

	return rc;
}

/***********************************************************************************************************/

static struct json_object *object_of_param(const struct wgt_desc_param *param)
{
	struct json_object *value;

	value = json_object_new_object();
	if (value
	 && j_add_string(value, string_name, param->name)
	 && j_add_string(value, string_value, param->value))
		return value;

	json_object_put(value);
	return NULL;
}

static int add_param_simple(struct json_object *obj, const struct wgt_desc_param *param, void *closure)
{
	return j_add_string_m(obj, param->name, param->value);
}

static int add_param_array(struct json_object *obj, const struct wgt_desc_param *param, void *closure)
{
	struct json_object *array, *value;

	if (!closure)
		array = obj;
	else if (!json_object_object_get_ex(obj, closure, &array)) {
		array = j_add_new_array(obj, closure);
		if (!array)
			return -ENOMEM;
	}
	value = object_of_param(param);
	if (value && j_add(array, NULL, value))
		return 0;

	json_object_put(value);
	return -ENOMEM;
}

static int add_param_object(struct json_object *obj, const struct wgt_desc_param *param, void *closure)
{
	struct json_object *object, *value;

	if (!closure)
		object = obj;
	else if (!json_object_object_get_ex(obj, closure, &object)) {
		object = j_add_new_object(obj, closure);
		if (!object)
			return -ENOMEM;
	}
	value = object_of_param(param);
	if (value && j_add(object, param->name, value))
		return 0;

	json_object_put(value);
	return -ENOMEM;
}

static int add_targeted_params(struct json_object *targets, const struct wgt_desc_feature *feat, struct paramaction actions[])
{
	int rc;
	const char *id;
	struct json_object *obj;

	id = get_target_name(feat, string_main);
	rc = get_target(&obj, targets, id, 0);
	return rc < 0 ? rc : apply_params(obj, feat->params, actions);
}

static int add_provided(struct json_object *targets, const struct wgt_desc_feature *feat)
{
	static struct paramaction actions[] = {
		{ .name = string_sharp_target, .action = NULL, .closure = NULL },
		{ .name = NULL, .action = add_param_simple, .closure = NULL }
	};
	return add_targeted_params(targets, feat, actions);
}

static int add_required_binding(struct json_object *targets, const struct wgt_desc_feature *feat)
{
	static struct paramaction actions[] = {
		{ .name = string_sharp_target, .action = NULL, .closure = NULL },
		{ .name = NULL, .action = add_param_array, .closure = (void*)string_required_binding }
	};
	return add_targeted_params(targets, feat, actions);
}


static int add_required_permission(struct json_object *targets, const struct wgt_desc_feature *feat)
{
	static struct paramaction actions[] = {
		{ .name = string_sharp_target, .action = NULL, .closure = NULL },
		{ .name = NULL, .action = add_param_object, .closure = (void*)string_required_permission }
	};
	return add_targeted_params(targets, feat, actions);
}

static int add_defined_permission(struct json_object *defperm, const struct wgt_desc_feature *feat)
{
	static struct paramaction actions[] = {
		{ .name = NULL, .action = add_param_array, .closure = NULL }
	};
	return apply_params(defperm, feat->params, actions);
}

/***********************************************************************************************************/

static struct json_object *to_json(const struct wgt_desc *desc)
{
	size_t prefixlen;
	const struct wgt_desc_feature *feat;
	const char *featname;
	struct json_object *result, *targets, *permissions;
	int rc, rc2;

	/* create the application structure */
	if(!(result = json_object_new_object())
	|| !(targets = j_add_new_array(result, string_targets))
	|| !(permissions = j_add_new_array(result, string_defined_permission))
	)
		goto error;

	/* first pass: declarations */
	rc = make_main_target(targets, desc);
	prefixlen = strlen(string_AGL_widget_prefix);
	for (feat = desc->features ; feat ; feat = feat->next) {
		featname = feat->name;
		if (!memcmp(featname, string_AGL_widget_prefix, prefixlen)) {
			if (!feat->required) {
				ERROR("feature %s can't be optional", featname);
				if (!rc)
					rc = -EINVAL;
			}
			featname += prefixlen;
			if (!strcmp(featname, string_provided_binding)
			||  !strcmp(featname, string_provided_application)) {
				rc2 = make_target(targets, feat);
				if (rc2 < 0 && !rc)
					rc = rc2;
			}
		}
	}

	/* second pass: definitions */
	for (feat = desc->features ; feat ; feat = feat->next) {
		featname = feat->name;
		if (!memcmp(featname, string_AGL_widget_prefix, prefixlen)) {
			featname += prefixlen;
			if (!strcmp(featname, string_defined_permission)) {
				rc2 = add_defined_permission(permissions, feat);
				if (rc2 < 0 && !rc)
					rc = rc2;
			}
			else if (!strcmp(featname, string_provided_application)
				|| !strcmp(featname, string_provided_binding)) {
				rc2 = add_provided(targets, feat);
				if (rc2 < 0 && !rc)
					rc = rc2;
			}
			else if (!strcmp(featname, string_required_binding)) {
				rc2 = add_required_binding(targets, feat);
				if (rc2 < 0 && !rc)
					rc = rc2;
			}
			else if (!strcmp(featname, string_required_permission)) {
				rc2 = add_required_permission(targets, feat);
				if (rc2 < 0 && !rc)
					rc = rc2;
			}
		}
	}

	/* fills the main */
	rc2 = j_add_many_strings_m(result,
		string_id, desc->id,
		string_idaver, desc->idaver,
		string_version, desc->version,
		"ver", desc->ver,
		"author.content", desc->author,
		"author.href", desc->author_href,
		"author.email", desc->author_email,
		"license.content", desc->license,
		"license.href", desc->license_href,
		"defaultlocale", desc->defaultlocale,
		"name.content", desc->name,
		"name.short", desc->name_short,
		"description", desc->description,
		NULL) ? 0 : -errno;
	if (rc2 < 0 && !rc)
		rc = rc2;

	/* */

	/* */
	if (!rc) {
		return result;
	}

error:
	json_object_put(result);
	return NULL;
}

struct json_object *wgt_info_to_json(struct wgt_info *info)
{
	return to_json(wgt_info_desc(info));
}

struct json_object *wgt_to_json(struct wgt *wgt)
{
	struct json_object *result;
	struct wgt_info *info;

	info = wgt_info_create(wgt, 1, 1, 1);
	if (info == NULL)
		result = NULL;
	else {
		result = wgt_info_to_json(info);
		wgt_info_unref(info);
	}
	return result;
}

struct json_object *wgt_path_at_to_json(int dfd, const char *path)
{
	struct json_object *result;
	struct wgt_info *info;

	info = wgt_info_createat(dfd, path, 1, 1, 1);
	if (info == NULL)
		result = NULL;
	else {
		result = wgt_info_to_json(info);
		wgt_info_unref(info);
	}
	return result;
}

struct json_object *wgt_path_to_json(const char *path)
{
	return wgt_path_at_to_json(AT_FDCWD, path);
}



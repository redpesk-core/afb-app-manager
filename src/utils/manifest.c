/*
 Copyright (C) 2015-2025 IoT.bzh Company

 Author: José Bollo <jose.bollo@iot.bzh>

 $RP_BEGIN_LICENSE$
 Commercial License Usage
  Licensees holding valid commercial IoT.bzh licenses may use this file in
  accordance with the commercial license agreement provided with the
  Software or, alternatively, in accordance with the terms contained in
  a written agreement between you and The IoT.bzh Company. For licensing terms
  and conditions see https://www.iot.bzh/terms-conditions. For further
  information use the contact form at https://www.iot.bzh/contact.

 GNU General Public License Usage
  Alternatively, this file may be used under the terms of the GNU General
  Public license version 3. This license is as published by the Free Software
  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
  of this file. Please review the following information to ensure the GNU
  General Public License requirements will be met
  https://www.gnu.org/licenses/gpl-3.0.html.
 $RP_END_LICENSE$
*/

#define _GNU_SOURCE

#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-yaml.h>
#include <rp-utils/rp-jsonc.h>

#include "manifest.h"

/* convert string to lower case */
static void make_lowercase(char *s)
{
	while(*s) {
		*s = (char)tolower(*s);
		s++;
	}
}

static void dash_to_underscore(char *s)
{
	while(*s) {
		if(*s == '-') {
			*s = '_';
		}
		s++;
	}
}

static char *mkver(const char *version)
{
	unsigned int lver;
	char c, *r;
	if (version) {
		c = version[lver = 0];
		while(c && c != ' ' && c != '.')
			c = version[++lver];
		if (c == '.') {
			c = version[++lver];
			while(c && c != ' ' && c != '.')
				c = version[++lver];
		}
		r = malloc(lver + 1);
		if (r) {
			memcpy(r, version, lver);
			r[lver] = 0;
			make_lowercase(r);
			return r;
		}
	}
	return NULL;
}

static char *mkidaver(char *id, char *ver)
{
#if DISTINCT_VERSIONS
	size_t lid, lver;
	char *r;
	if (id && ver) {
		lid = strlen(id);
		lver = strlen(ver);
		r = malloc(2 + lid + lver);
		if (r) {
			memcpy(r, id, lid);
			r[lid] = '@';
			memcpy(r + lid + 1, ver, lver);
			r[lid + lver + 1] = 0;
			return r;
		}
	}
	return NULL;
#else
	return strdup(id);
#endif
}

static int add(json_object *jso, const char *key, const char *value)
{
	json_object *v = json_object_new_string(value);
	if (v != NULL) {
		json_object_object_add(jso, key, v);
		if (json_object_object_get(jso, key) == v)
			return 0;
	}
	return -ENOMEM;
}

/*
 * add a key #target to the value of target, see bug #5358
 */
static int retargets(json_object *jso)
{
	int rc = 0;
	unsigned idx, length;
	json_object *targets, *item, *targ;

	if (!json_object_object_get_ex(jso, MANIFEST_TARGETS, &targets)
	 || !json_object_is_type(targets, json_type_array))
		rc = -EINVAL;
	else {
		length = (unsigned)json_object_array_length(targets);
		for (idx = 0 ; idx < length ; idx++) {
			item = json_object_array_get_idx(targets, idx);
			if (!json_object_is_type(item, json_type_object))
				rc = -EINVAL;
			else if (!json_object_object_get_ex(item, MANIFEST_SHARP_TARGET, &targ)) {
				if (!json_object_object_get_ex(item, MANIFEST_TARGET, &targ))
					rc = -EINVAL;
				else {
					targ = json_object_get(targ);
					json_object_object_add(item, MANIFEST_SHARP_TARGET, targ);
				}
			}
		}
	}
	return rc;
}

/*
* adapt permissions because permissions can be given as:
*
* - string
* - [string]
* - [{name:string,value:string}]
* - {name:value}
* - {name:{name:string,value:string}}
*
* for later processing, the canonical state is {name:{name:string,value:string}}
*/
static int add_perm_from_string(json_object *perms, json_object *permstring)
{
	json_object *obj;
	int rc = rp_jsonc_pack(&obj, "{sO ss}", "name", permstring, "value", MANIFEST_VALUE_REQUIRED);
	if (rc >= 0) {
		rc = json_object_object_add(perms, json_object_get_string(permstring), obj);
		if (rc < 0)
			json_object_put(obj);
	}
	return rc;
}

static int process_array_perm_cb(void *clo, json_object *item)
{
	int rc = -EINVAL;
	json_object *name, *perms = clo;

	switch (json_object_get_type(item)) {
	case json_type_object:
		if (json_object_object_get_ex(item, MANIFEST_NAME, &name)
		 && json_object_object_get_ex(item, MANIFEST_VALUE, NULL)) {
			rc = json_object_object_add(perms, json_object_get_string(name), item);
			if (rc >= 0)
				json_object_get(item);
		}
		break;
	case json_type_string:
		rc = add_perm_from_string(perms, item);
		break;
	default:
		break;
	}
	return rc;
}

static int process_object_perm_cb(void *clo, json_object *item, const char *key)
{
	int rc = -EINVAL;
	json_object *obj, *name, *perms = clo;

	switch (json_object_get_type(item)) {
	case json_type_object:
		if (json_object_object_get_ex(item, MANIFEST_NAME, &name)
		 && 0 == strcmp(key, json_object_get_string(name))
		 && json_object_object_get_ex(item, MANIFEST_VALUE, NULL)) {
			rc = json_object_object_add(perms, key, item);
			if (rc >= 0)
				json_object_get(item);
		 }
		break;
	case json_type_string:
		rc = rp_jsonc_pack(&obj, "{ss sO}", "name", key, "value", item);
		if (rc >= 0) {
			rc = json_object_object_add(perms, key, obj);
			if(rc >= 0)
				json_object_get(obj);
		}
		break;
	default:
		break;
	}
	return rc;
}

static int adapt_permissions_of(json_object *jso)
{
	json_object *in, *out;
	int rc = -EINVAL;
	if (json_object_is_type(jso, json_type_object)) {
		if (!json_object_object_get_ex(jso, MANIFEST_REQUIRED_PERMISSIONS, &in))
			rc = 0;
		else {
			out = NULL;
			switch (json_object_get_type(in)) {
			case json_type_array:
				out = json_object_new_object();
				if (out == NULL)
					rc = -ENOMEM;
				else
					rc = rp_jsonc_array_until(in, process_array_perm_cb, out);
				break;
			case json_type_object:
				out = json_object_new_object();
				if (out == NULL)
					rc = -ENOMEM;
				else
					rc = rp_jsonc_object_until(in, process_object_perm_cb, out);
				break;
			case json_type_string:
				out = json_object_new_object();
				if (out == NULL)
					rc = -ENOMEM;
				else
					rc = add_perm_from_string(out, in);
				break;
			default:
				break;
			}
			if (out != NULL) {
				if (rc < 0)
					json_object_put(out);
				else
					json_object_object_add(jso, MANIFEST_REQUIRED_PERMISSIONS, out);
			}
		}
	}
	return rc;
}

static int adapt_target_permissions_cb(void *clo, json_object *item)
{
	return adapt_permissions_of(item);
}

static int adapt_all_permissions(json_object *jso)
{
	json_object *targets;
	int rc;

	rc = adapt_permissions_of(jso);
	if (rc >= 0) {
		if (!json_object_object_get_ex(jso, MANIFEST_TARGETS, &targets)
		|| !json_object_is_type(targets, json_type_array))
			rc = -EINVAL;
		else
			rc = rp_jsonc_array_until(targets, adapt_target_permissions_cb, NULL);
	}
	return rc;
}

static int fulfill(json_object *jso)
{
	int rc = -EINVAL;
	char *lowid = NULL, *undid = NULL, *ver = NULL, *idaver = NULL;
	json_object *id, *version;

	if (json_object_object_get_ex(jso, MANIFEST_ID, &id)
	 && json_object_object_get_ex(jso, MANIFEST_VERSION, &version)) {
		ver = mkver(json_object_get_string(version));
		lowid = strdup(json_object_get_string(id));
		if (lowid != NULL) {
			make_lowercase(lowid);
			if (ver != NULL)
				idaver = mkidaver(lowid, ver);
			undid = strdup(lowid);
			if (undid != NULL)
				dash_to_underscore(undid);
		}

		if (lowid != NULL && undid != NULL && ver != NULL && idaver != NULL) {
			rc = add(jso, MANIFEST_ID, lowid);
			if (rc == 0)
				rc = add(jso, MANIFEST_ID_UNDERSCORE, undid);
			if (rc == 0)
				rc = add(jso, MANIFEST_IDAVER, idaver);
			if (rc == 0)
				rc = add(jso, MANIFEST_VER, ver);
		}
		free(lowid);
		free(undid);
		free(ver);
		free(idaver);
	}
	return rc;
}

static int check_valid_string(json_object *jso, const char *key)
{
	json_object *jval;
	const char *value;
	int pos;
	char c;

	if (!json_object_object_get_ex(jso, key, &jval))
		return -EINVAL;
	value = json_object_get_string(jval);
	pos = 0;
	c = value[pos];
	if (c == 0) {
		RP_ERROR("empty string forbidden in '%s' (temporary constraints)", key);
		return -EINVAL;
	}
	do {
		if (!isalnum(c) && !strchr(".-_", c)) {
			RP_ERROR("forbidden char %c in '%s' -> '%s' (temporary constraints)", c, key, value);
			return -EINVAL;
		}
		c = value[++pos];
	} while(c);
	return 0;
}

int
manifest_normalize(
	json_object *obj,
	const char *path
) {
	int rc = fulfill(obj);
	if (rc < 0)
		RP_ERROR("can't fulfill manifest %s", path);
	else {
		rc = retargets(obj);
		if (rc < 0)
			RP_ERROR("can't retarget manifest %s", path);
		else {
			rc = adapt_all_permissions(obj);
			if (rc < 0)
				RP_ERROR("can't adapt manifest %s", path);
		}
	}
	return rc;
}

int manifest_check(json_object *jso)
{
	json_object *jval;
	int rc = 0;

	if (!json_object_object_get_ex(jso, MANIFEST_RP_MANIFEST, &jval)
	 || strcmp("1", json_object_get_string(jval)))
		rc = -EINVAL;
	if (rc == 0)
		rc = check_valid_string(jso, MANIFEST_ID);
	if (rc == 0)
		rc = check_valid_string(jso, MANIFEST_VERSION);
	return rc;
}

int
manifest_read(
	json_object **obj,
	const char *path
) {
	int rc = rp_yaml_path_to_json_c(obj, path, path);
	if (rc < 0)
		RP_ERROR("error while reading manifest file %s", path);
	else if (*obj == NULL) {
		RP_ERROR("NULL when reading manifest file %s", path);
		rc = -EBADF;
	}
	return rc;
}

int
manifest_read_and_check(
	json_object **obj,
	const char *path
) {
	int rc = manifest_read(obj, path);
	if (rc >= 0) {
		rc = manifest_check(*obj);
		if (rc < 0)
			RP_ERROR("constraints of manifest not fulfilled for %s", path);
		else
			rc = manifest_normalize(*obj, path);
		if (rc < 0) {
			json_object_put(*obj);
			*obj = NULL;
		}
	}
	return rc;
}

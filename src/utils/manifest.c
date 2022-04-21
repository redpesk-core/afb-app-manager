/*
 Copyright (C) 2015-2022 IoT.bzh Company

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


#define MANIFEST_REQUIRED_PERMISSIONS		"required-permissions"
#define MANIFEST_FILE_PROPERTIES		"file-properties"
#define MANIFEST_TARGETS			"targets"
#define MANIFEST_NAME				"name"
#define MANIFEST_VALUE				"value"

#define MANIFEST_RP_MANIFEST			"rp-manifest"
#define MANIFEST_ID				"id"
#define MANIFEST_VERSION			"version"

#define MANIFEST_VALUE_OPTIONAL			"optional"
#define MANIFEST_VALUE_REQUIRED			"required"


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
	}
	return rc;
}


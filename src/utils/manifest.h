/*
 Copyright (C) 2015-2023 IoT.bzh Company

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

#pragma once


#define MANIFEST_REQUIRED_PERMISSIONS		"required-permission"
#define MANIFEST_PROVIDED_BINDING		"provided-binding"
#define MANIFEST_FILE_PROPERTIES		"file-properties"
#define MANIFEST_TARGETS			"targets"
#define MANIFEST_NAME				"name"
#define MANIFEST_VALUE				"value"

#define MANIFEST_RP_MANIFEST			"rp-manifest"
#define MANIFEST_ID				"id"
#define MANIFEST_VERSION			"version"
#define MANIFEST_ID_UNDERSCORE			"id-underscore"
#define MANIFEST_VER				"ver"
#define MANIFEST_IDAVER				"idaver"

#define MANIFEST_VALUE_OPTIONAL			"optional"
#define MANIFEST_VALUE_REQUIRED			"required"

#define MANIFEST_TARGET				"target"
#define MANIFEST_SHARP_TARGET			"#target"
#define MANIFEST_PLUGS				"plugs"

/**
* Normalize the given manifest object
*
* @param obj    the manifest object to normalize
* @param path   path for error reports
*
* @return 0 if normalized or a negative code if failed
*/
extern int manifest_normalize(json_object *obj, const char *path);

/**
* Validates the given manifest object
*
* @param jso    pointer where to store the readen object
*
* @return 0 if valid or a negative code if invalid
*/
extern int manifest_check(json_object *jso);

/**
* Reads the manifest file of 'path'
* and return its JSON-C representation in *'obj'
*
* @param obj    pointer where to store the readen object
* @param path   path to the manifest file
*
* @return 0 on success or a negative code on error
*/
extern int manifest_read(json_object **obj, const char *path);

/**
* Reads the manifest file of 'path', validate it
* and return its JSON-C representation in *'obj'
*
* @param obj    pointer where to store the readen object
* @param path   path to the manifest file
*
* @return 0 on success or a negative code on error
*/
extern int manifest_read_and_check(json_object **obj, const char *path);


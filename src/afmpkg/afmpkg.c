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

#include "afmpkg.h"

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-jsonc.h>

#include "cert/signed-digest.h"
#include "cert/signature-name.h"

#include "manage-afid.h"
#include "manifest.h"
#include "mime-type.h"
#include "normalize-unit-file.h"
#include "path-entry.h"
#include "path-type.h"
#include "permset.h"

#include "unit-process.h"

#if !defined(DEFAULT_TCP_PORT_BASE)
#define DEFAULT_TCP_PORT_BASE				29000
#endif

/*****************************************************************************/
/*** PRIVATE *****************************************************************/
/*****************************************************************************/

/**
* processing structure
*/
typedef
struct
{
	/** the request */
	const afmpkg_t *apkg;

	/** operations */
	const afmpkg_operations_t *opers;

	/** closure of operations */
	void *closure;

	/** processing mode */
	afmpkg_mode_t mode;

	/** the root of all entries */
	path_entry_t *files;

	/** for returning the status */
	int rc;

	/** the json object of the manifest */
	json_object *manifest;

	/** application ID from manifest */
	const char *appid;

	/** the set of permission s*/
	permset_t *permset;

	/** path entry for the package */
	path_entry_t *packdir;

	/** offset of the root path */
	unsigned offset_root;

	/** offset of the package path */
	unsigned offset_pack;

	/** path buffer */
	char path[PATH_MAX];
}
	afmpkg_state_t;


static const char name_manifest[] = ".rpconfig/manifest.yml";
static const char name_config[] = "config.xml";
static const char key_type[] = "type";

/** default given permissions */
static const char *default_permissions[] = {
	"urn:AGL:token:valid"
};

/* predeclaration of legacy functions (avoids including legacy headers at top) */
static int config_read_and_check(struct json_object **obj, const char *path);

/*****************************************************************************/
/*** MANAGE ENTRY TYPE *******************************************************/
/*****************************************************************************/

/** returns the path type attached to the entry */
static
path_type_t
get_entry_type(const path_entry_t *entry)
{
	void *value = path_entry_var(entry, key_type);
	return (path_type_t)(intptr_t)value;
}

/** attach a path type attached to the entry */
static
int
set_entry_type(path_entry_t *entry, path_type_t type)
{
	void *value = (void*)(intptr_t)type;
	return path_entry_var_set(entry, key_type, value, NULL);
}

/*****************************************************************************/
/*** ITERATION OVER JSON *****************************************************/
/*****************************************************************************/

/**
* Iterate over items of the array at key in obj
*
* @param obj the object with the key (or not)
* @param key the key indexing the array to iterate
* @param fun callback function called for each entry of the array
* @param clo the closure for the callback
*
* The callback function 'fun' receives 2 parameters:
*   - the closure value 'clo'
*   - the json-c object under iteration
*/
static
void
for_each_of_array(
	json_object *obj,
	const char *key,
	void (*fun)(void*, json_object*),
	void *clo
) {
	json_object *item;
	if (json_object_object_get_ex(obj, key, &item))
		rp_jsonc_array_for_all(item, fun, clo);
}

/**
* Iterate over items of the object at key in obj
*
* @param obj the object with the key (or not)
* @param key the key indexing the object to iterate
* @param fun callback function called for each entry of the array
* @param clo the closure for the callback
*
* The callback function 'fun' receives 2 parameters:
*   - the closure value 'clo'
*   - the json-c object under iteration
*   - the key of the value
*/
static
void
for_each_of_object(
	json_object *obj,
	const char *key,
	void (*fun)(void*, json_object*, const char*),
	void *clo
) {
	json_object *item;
	if (json_object_object_get_ex(obj, key, &item))
		rp_jsonc_object_for_all(item, fun, clo);
}

/**
* Iterate over items of the array at key in manifest
*
* @param state the state object
* @param key the key indexing the array to iterate
* @param fun callback function called for each entry of the array
*
* The callback function 'fun' receives 2 parameters:
*   - the closure value 'clo'
*   - the json-c object under iteration
*/
static
void
for_each_of_manifest(
	afmpkg_state_t *state,
	const char *key,
	void (*fun)(afmpkg_state_t*, json_object*)
) {
	for_each_of_array(state->manifest, key, (void(*)(void*,json_object*))fun, state);
}

static
void
for_each_target(afmpkg_state_t *state, void (*fun)(afmpkg_state_t*, json_object*))
{
	for_each_of_manifest(state, MANIFEST_TARGETS, fun);
}

/*****************************************************************************/
/*** MANAGE STATE RC *********************************************************/
/*****************************************************************************/

static inline
void
put_state_rc(afmpkg_state_t *state, int rc)
{
	if (rc < 0 && state->rc >= 0)
		state->rc = rc;
}

/*****************************************************************************/
/*** ITERATION OVER PLUGS ****************************************************/
/*****************************************************************************/

struct plug_cb
{
	afmpkg_state_t *state;
	void (*fun)(afmpkg_state_t*, path_entry_t*, const char*, const char*);
};

/**
* callback processing an item defining a plug
*/
static
void
for_each_plug_cb(void *closure, json_object *desc)
{
	int rc = 0;
	json_object *item;
	const char *expdir;
	const char *impid;
	path_entry_t *entry;
	char path[PATH_MAX + 1];
	struct plug_cb *pcb = closure;
	afmpkg_state_t *state = pcb->state;

	/* get the id (name) */
	if (!json_object_object_get_ex(desc, MANIFEST_NAME, &item)) {
		RP_ERROR("name missing in plug");
		put_state_rc(state, -EINVAL);
		return;
	}
	expdir = json_object_get_string(item);

	/* get exported directory (value) */
	if (!json_object_object_get_ex(desc, MANIFEST_VALUE, &item)) {
		RP_ERROR("value missing in plug");
		put_state_rc(state, -EINVAL);
		return;
	}
	impid = json_object_get_string(item);

	/* get entry of exported directory */
	rc = path_entry_get(state->packdir, &entry, expdir);
	if (rc < 0) {
		RP_ERROR("invalid plug path %s", expdir);
		put_state_rc(state, rc);
		return;
	}

	/* get export path directory */
	snprintf(&state->path[state->offset_pack],
		sizeof state->path - state->offset_pack,
		"/%s", expdir);

	/* get import path directory */
	rc = snprintf(path, sizeof path, "%.*s%s/%s/plugins",
		state->offset_root, state->path, FWK_APP_DIR, impid);
	if (rc <= 0 && rc >= (int)sizeof path) {
		RP_ERROR("can't set impdir path");
		put_state_rc(state, -ENAMETOOLONG);
		return;
	}

	/* invoke the callback function */
	pcb->fun(state, entry, impid, path);
}

/** iterate over the plugs of the package if any exist */
static
void
for_each_plug(
	afmpkg_state_t *state,
	void (*fun)(afmpkg_state_t*, path_entry_t*, const char*, const char*)
) {
	struct plug_cb pcb = { .state = state, .fun = fun };
	for_each_of_array(state->manifest, MANIFEST_PLUGS, for_each_plug_cb, &pcb);
}

/*****************************************************************************/
/*** ITERATION OVER FILES ****************************************************/
/*****************************************************************************/

/** iterate over the entries of the package */
static
int
for_each_entry(
	afmpkg_state_t *state,
	unsigned flags,
	int (*fun)(afmpkg_state_t *state, path_entry_t *entry,
                   const char *path, size_t length)
) {
	unsigned offset = state->offset_pack;
	if (!(flags & PATH_ENTRY_FORALL_SILENT_ROOT)) {
		unsigned toplen = path_entry_length(state->packdir);
		if (offset > toplen)
			offset -= toplen + 1;
	}
	return path_entry_for_each_in_buffer(
	                flags, state->packdir,
	                (path_entry_for_each_cb_t)fun, state,
	                &state->path[offset], sizeof state->path - offset);
}

/** iterate over the entries of the package */
static
int
for_each_content_entry(
	afmpkg_state_t *state,
	unsigned flags,
	int (*fun)(afmpkg_state_t *state, path_entry_t *entry,
	           const char *path, size_t length)
) {
	return for_each_entry(state, flags | PATH_ENTRY_FORALL_SILENT_ROOT, fun);
}

/*****************************************************************************/
/*** CHECKING PERMISSIONS ****************************************************/
/*****************************************************************************/

static
void
check_one_permission_cb(void * closure, json_object *jso, const char *key)
{
	afmpkg_state_t *state = closure;
	permset_t *permset = state->permset;
	const char *perm = NULL;
	json_object *name, *value;
	int hasname, hasvalue, optional = 0;

	switch (json_object_get_type(jso)) {
	case json_type_object:
		hasname = json_object_object_get_ex(jso, MANIFEST_NAME, &name);
		hasvalue = json_object_object_get_ex(jso, MANIFEST_VALUE, &value);
		if (!hasname)
			RP_ERROR("permission name is missing: %s", json_object_get_string(jso));
		else if (json_object_get_type(name) != json_type_string)
			RP_ERROR("permission name is not a string: %s", json_object_get_string(jso));
		else
			perm = json_object_get_string(name);
		if (!hasvalue)
			RP_WARNING("permission value is missing: %s", json_object_get_string(jso));
		else if (!strcmp(json_object_get_string(value), MANIFEST_VALUE_OPTIONAL))
			optional = 1;
		else if (strcmp(json_object_get_string(value), MANIFEST_VALUE_REQUIRED)) {
			RP_ERROR("invalid permission value: %s", json_object_get_string(jso));
			perm = NULL;
		}
		break;
	case json_type_string:
		perm = json_object_get_string(jso);
		break;
	default:
		RP_ERROR("invalid permission item: %s", json_object_get_string(jso));
		break;
	}
	if (perm != NULL) {
		/* TODO validate the permission name */
#define HACK_ALLOWING_REQUESTED_PERMISSIONS 1
#if HACK_ALLOWING_REQUESTED_PERMISSIONS
		permset_grant(permset, perm);
#endif
#undef HACK_ALLOWING_REQUESTED_PERMISSIONS
		/* request the permission */
		if (permset_request(permset, perm))
			RP_DEBUG("granted permission: %s", perm);
		else if (optional)
			RP_INFO("optional permission ungranted: %s", perm);
		else
			RP_ERROR("ungranted permission required: %s", perm);
	}
}

static
void
check_permissions_cb(afmpkg_state_t *state, json_object *jso)
{
	for_each_of_object(jso, MANIFEST_REQUIRED_PERMISSIONS,
	                   check_one_permission_cb, state);
}

static
int
check_permissions(afmpkg_state_t *state)
{
	check_permissions_cb(state, state->manifest);
	for_each_target(state, check_permissions_cb);
	return 0;
}

/*****************************************************************************/
/*** CHECKING CONTENT ********************************************************/
/*****************************************************************************/

/*
* check the src and type of one target
*/
static
int
check_src_type_definition(afmpkg_state_t *state, const char *src, const char *type)
{
	int rc;
	struct stat s;
	size_t length;
	path_entry_t *entry;

	/* get the entry for the src */
	rc = path_entry_get(state->packdir, &entry, src);
	if (rc < 0) {
		/* fallback to htdocs for http content */
		rc = path_entry_get(state->packdir, &entry, "htdocs");
		if (rc >= 0)
			rc = path_entry_get(entry, &entry, src);
		if (rc < 0) {
			RP_ERROR("file not installed %s", src);
			return -ENOENT;
		}
	}

	/* get the full name of the entry on disk */
	length = path_entry_relpath(entry, &state->path[state->offset_pack],
	                            PATH_MAX - state->offset_pack, state->packdir,
				    PATH_ENTRY_FORCE_LEADING_SLASH);
	if (length + state->offset_root > PATH_MAX) {
		RP_ERROR("filename too long %s", src);
		return -ENAMETOOLONG;
	}

	/* get src path information */
	rc = fstatat(AT_FDCWD, state->path, &s, AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
	if (rc < 0) {
		rc = -errno;
		RP_ERROR("can't get status of src %s: %s", state->path, strerror(errno));
		return rc;
	}

	/* check src conformity */
	if (!S_ISREG(s.st_mode) && !S_ISDIR(s.st_mode)) {
		RP_ERROR("src isn't a regular file or a directory %s", state->path);
		return -EINVAL;
	}

	return 0;
}

/*
* check definition of content of a target
*/
static
void
check_target_content(afmpkg_state_t *state, json_object *target)
{
	json_object *content, *src, *type;
	int rc = -EINVAL;

	/* check if has a content */
	if (!json_object_object_get_ex(target, "content", &content))
		RP_ERROR("no content %s", json_object_get_string(target));

	/* check that the content is an object */
	else if (!json_object_is_type(content, json_type_object))
		RP_ERROR("content isn't an object %s", json_object_get_string(content));

	/* check that the content has a key "src" */
	else if (!json_object_object_get_ex(content, "src", &src))
		RP_ERROR("no content.src %s", json_object_get_string(content));

	/* check that the content's value at key "src" is a string */
	else if (!json_object_is_type(src, json_type_string))
		RP_ERROR("no content.src isn't a string %s", json_object_get_string(src));

	/* check that the content has a key "type" */
	else if (!json_object_object_get_ex(content, "type", &type))
		RP_ERROR("no content.type %s", json_object_get_string(content));

	/* check that the content's value at key "type" is a string */
	else if (!json_object_is_type(type, json_type_string))
		RP_ERROR("no content.type isn't a string %s", json_object_get_string(type));

	else
		rc = check_src_type_definition(state, json_object_get_string(src), json_object_get_string(type));
	put_state_rc(state, rc);
}

/*****************************************************************************/
/*** CHECKING CONFIGS ********************************************************/
/*****************************************************************************/

/*
* check the src and type of one target
*/
static
int
check_config_file(afmpkg_state_t *state, const char *src)
{
	int rc;
	struct stat s;
	size_t length;
	path_entry_t *entry;

	/* get the entry for the src */
	rc = path_entry_get(state->packdir, &entry, src);
	if (rc < 0) {
		RP_ERROR("file not installed %s", src);
		return -ENOENT;
	}

	/* get the full name of the entry on disk */
	length = path_entry_relpath(entry, &state->path[state->offset_pack],
	                            PATH_MAX - state->offset_pack, state->packdir,
				    PATH_ENTRY_FORCE_LEADING_SLASH);
	if (length + state->offset_root > PATH_MAX) {
		RP_ERROR("filename too long %s", src);
		return -ENAMETOOLONG;
	}

	/* get src path information */
	rc = fstatat(AT_FDCWD, state->path, &s, AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
	if (rc < 0) {
		rc = -errno;
		RP_ERROR("can't get status of %s: %s", state->path, strerror(errno));
		return rc;
	}
	/* check src conformity */
	if (!S_ISREG(s.st_mode)) {
		RP_ERROR("config isn't a regular file %s", state->path);
		return -EINVAL;
	}

	return 0;
}

/*
* check one config of a target
*/
static
void
check_one_target_config_cb(void *closure, json_object *jso)
{
	afmpkg_state_t *state = closure;
	int rc = -EINVAL;

	/* check that the object is a string */
	if (!json_object_is_type(jso, json_type_string))
		RP_ERROR("config isn't an string %s", json_object_get_string(jso));
	else
		rc = check_config_file(state, json_object_get_string(jso));
	put_state_rc(state, rc);
}

/*
* check config of a target
*/
static
void
check_target_config(afmpkg_state_t *state, json_object *target)
{
	json_object *configs;
	if (json_object_object_get_ex(target, MANIFEST_REQUIRED_CONFIGS, &configs))
		rp_jsonc_optarray_for_all(configs, check_one_target_config_cb, state);
}

/*****************************************************************************/
/*** CHECKING TARGETS ********************************************************/
/*****************************************************************************/

/*
* check definition of one target
*/
static
void
check_target_cb(afmpkg_state_t *state, json_object *target)
{
	check_target_content(state, target);
	check_target_config(state, target);
}

/*
* checks that any target of the manifest is correctly setup
*/
static
int
check_contents(afmpkg_state_t *state)
{
	for_each_target(state, check_target_cb);
	return state->rc;
}

/*****************************************************************************/
/*** COMPUTE FILE PROPERTIES  ************************************************/
/*****************************************************************************/

/* callback for resetting the path type to UNKNOWN */
static
int
reset_type_cb(afmpkg_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	int rc = set_entry_type(entry, path_type_Unset);
	put_state_rc(state, rc);
	return 0;
}

/* callback for implementing file-properties configuration */
static
void
compute_explicit_file_properties_cb(afmpkg_state_t *state, json_object *jso)
{
	path_entry_t *entry;
	json_object *name, *value;
	const char *strval;
	path_type_t type, prvtype;
	int rc = 0;

	/* extract the values */
	if (!json_object_object_get_ex(jso, "name", &name)
	 || !json_object_object_get_ex(jso, "value", &value)) {
		RP_ERROR("bad file properties item %s", json_object_get_string(jso));
		rc = -EINVAL;
	}
	else {
		/* get the path entry matching the name */
		rc = path_entry_get(state->packdir, &entry, json_object_get_string(name));
		if (rc < 0) {
			RP_ERROR("file doesn't exist %s", json_object_get_string(jso));
			rc = -ENOENT;
		}
		else {
			/* compute the effective path type of value */
			strval = json_object_get_string(value);
			type = path_type_of_property_key(strval);
			if (type == path_type_Unset) {
				RP_ERROR("invalid value %s", json_object_get_string(jso));
				rc = -EINVAL;
			}
			else {
				/* set the value if not conflicting */
				prvtype = get_entry_type(entry);
				if (prvtype == path_type_Unset)
					set_entry_type(entry, type);
				else if (prvtype != type) {
					RP_ERROR("file property conflict %s", json_object_get_string(jso));
					rc = -EEXIST;
				}
			}
		}
	}
	put_state_rc(state, rc);
}

/* callback for implementing plug exporting property */
static
void
compute_implicit_plug_property_cb(afmpkg_state_t *state, json_object *jso)
{
	path_entry_t *entry;
	json_object *name;
	int rc = 0;

	/* extract the values */
	if (!json_object_object_get_ex(jso, "name", &name)) {
		RP_ERROR("bad plug properties %s", json_object_get_string(jso));
		rc = -EINVAL;
	}
	else {
		/* get the path entry matching the name */
		rc = path_entry_get(state->packdir, &entry, json_object_get_string(name));
		if (rc < 0) {
			RP_ERROR("entry doesn't exist %s", json_object_get_string(jso));
			rc = -ENOENT;
		}
		else {
			/* set type "plug" */
			set_entry_type(entry, path_type_Plug);
		}
	}
	put_state_rc(state, rc);
}

/* callback for implementing plug exporting property */
static
void
compute_provided_binding_property_cb(afmpkg_state_t *state, json_object *jso)
{
	path_entry_t *entry;
	json_object *path;
	int rc = 0;

	/* extract the values */
	if (!json_object_object_get_ex(jso, "value", &path)) {
		RP_ERROR("bad provided-binding property %s", json_object_get_string(jso));
		rc = -EINVAL;
	}
	else {
		/* get the path entry matching the name */
		rc = path_entry_get(state->packdir, &entry, json_object_get_string(path));
		if (rc < 0) {
			RP_ERROR("entry doesn't exist %s", json_object_get_string(jso));
			rc = -ENOENT;
		}
		else {
			/* set type "plug" */
			set_entry_type(entry, path_type_Public_Lib);
		}
	}
	put_state_rc(state, rc);
}

/* callback possibly deducing file type for contents */
static
void
compute_target_file_properties_cb(afmpkg_state_t *state, json_object *jso)
{
	json_object *content, *src, *type;
	path_entry_t *entry;

	/* extract the values (note that it normally works because manifest is valid) */
	if (json_object_object_get_ex(jso, "content", &content)
	 && json_object_object_get_ex(content, "src", &src)
	 && json_object_object_get_ex(content, "type", &type)) {
		/* check that the source exists */
		if (path_entry_get(state->packdir, &entry, json_object_get_string(src)) < 0) {
			RP_ERROR("file doesn't exist %s", json_object_get_string(jso));
			put_state_rc(state, -ENOENT);
		}
		else if (get_entry_type(entry) == path_type_Unset) {
			/* the file exists but is of unknown type */
			if (mime_type_is_executable(json_object_get_string(type)))
				/* set as executable for known mime-type */
				set_entry_type(entry, path_type_Exec);
		}
	}
}

/* callback for computing default file properties */
static
int
compute_default_files_properties_cb(afmpkg_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	int rc;
	struct stat s;
	path_type_t curtype;

	/* get path information */
	rc = fstatat(AT_FDCWD, state->path, &s, AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
	if (rc < 0) {
		rc = -errno;
		RP_ERROR("can't get status of src %s: %s", state->path, strerror(errno));
	}

	/* check conformity */
	else if (!S_ISREG(s.st_mode) && !S_ISDIR(s.st_mode)) {
		RP_ERROR("src isn't a regular file or a directory %s", state->path);
		rc = -EINVAL;
	}

	/* extract type */
	else {
		curtype = get_entry_type(entry);
		if (curtype != path_type_Unset)
			rc = 0;
		else {
			if (S_ISDIR(s.st_mode))
				curtype = path_type_of_dirname(path_entry_name(entry));
			if (curtype == path_type_Unset)
				curtype = get_entry_type(path_entry_parent(entry));
			if (curtype == path_type_Unset)
				curtype = path_type_Id;
			rc = set_entry_type(entry, curtype);
		}
	}
	put_state_rc(state, rc);
	return rc;
}

/* callback for propagation of public status */
static
int
compute_public_files_properties_cb(afmpkg_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	path_type_t curtype = get_entry_type(entry);
	switch (curtype) {
	case path_type_Plug:
		if (entry != state->packdir) {
			entry = path_entry_parent(entry);
			if (get_entry_type(entry) != path_type_Public)
				set_entry_type(entry, path_type_Plug);
		}
		break;
	case path_type_Public:
	case path_type_Public_Exec:
	case path_type_Public_Lib:
		if (entry != state->packdir) {
			entry = path_entry_parent(entry);
			set_entry_type(entry, path_type_Public);
		}
		break;
	default:
		break;
	}
	return 0;
}

/* compute security properties of files */
static
int
compute_files_properties(afmpkg_state_t *state)
{
	/* reset any type */
	state->rc = set_entry_type(state->packdir, path_type_Id);
	if (state->rc >= 0)
		for_each_content_entry(state, PATH_ENTRY_FORALL_NO_PATH, reset_type_cb);
	/* set implicit plugin types */
	if (state->rc >= 0)
		for_each_of_manifest(state, MANIFEST_PLUGS, compute_implicit_plug_property_cb);
	/* export provided binding */
	if (state->rc >= 0)
		for_each_of_manifest(state, MANIFEST_PROVIDED_BINDING, compute_provided_binding_property_cb);
	/* set explicit types */
	if (state->rc >= 0)
		for_each_of_manifest(state, MANIFEST_FILE_PROPERTIES, compute_explicit_file_properties_cb);
	/* set types of targets */
	if (state->rc >= 0)
		for_each_target(state, compute_target_file_properties_cb);
	/* all other files */
	if (state->rc >= 0)
		for_each_content_entry(state, PATH_ENTRY_FORALL_BEFORE, compute_default_files_properties_cb);
	/* propagate public status */
	if (state->rc >= 0)
		for_each_entry(state, PATH_ENTRY_FORALL_AFTER | PATH_ENTRY_FORALL_NO_PATH, compute_public_files_properties_cb);
	return state->rc;
}

/*****************************************************************************/
/*** MAKE FILE PROPERTIES EFFECTIVE ******************************************/
/*****************************************************************************/

/* set execution property of files */
static
int
make_file_executable(const char *filename)
{
	int rc = chmod(filename, 0755);
	if (rc < 0) {
		RP_ERROR("can't make file executable %s", filename);
		rc = -errno;
	}
	return rc;
}

/* callback for applying DAC properties accordingly to path types */
static
int
setup_file_properties_cb(afmpkg_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	int rc;
	switch (get_entry_type(entry)) {
	case path_type_Public_Exec:
	case path_type_Exec:
		rc = make_file_executable(state->path);
		put_state_rc(state, rc);
		break;
	default:
		break;
	}
	return 0;
}

/* apply DAC properties accordingly to path types */
static
int
setup_files_properties(afmpkg_state_t *state)
{
	for_each_content_entry(state, 0, setup_file_properties_cb);
	return state->rc;
}

/*****************************************************************************/
/*** SETTING OF THE SECURITY ITEMS *******************************************/
/*****************************************************************************/

static
int
setup_security_file_cb(afmpkg_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	int rc;
	path_type_t type = get_entry_type(entry);
	if (type == path_type_Unset) {
		RP_DEBUG("unknown path type: %s", state->path);
		type = path_type_Conf;
	}
	rc = state->opers->tagfile(state->closure, state->path, type);
	put_state_rc(state, rc);
	return 0;
}

static
int
permit(afmpkg_state_t *state, const char *perm)
{
	int rc;

	RP_INFO("Permits %s", perm);

	rc = state->opers->setperm(state->closure, perm);
	if (rc < 0)
		RP_ERROR("Fails to permit %s", perm);
	return rc;
}

static
void
set_plug_cb(afmpkg_state_t *state, path_entry_t *entry, const char *impid, const char *impdir)
{
	int rc = state->opers->setplug(state->closure, state->path, impid, impdir);
	if (rc < 0) {
		RP_ERROR("can't add plug");
		if (state->rc >= 0)
			state->rc = rc;
	}
}

static
int
setup_security(afmpkg_state_t *state)
{
	int rc;
	unsigned i, n;

	/* setup file security */
	for_each_entry(state, PATH_ENTRY_FORALL_AFTER, setup_security_file_cb);
	rc = state->rc;
	if (rc < 0)
		goto end;

	/* setup permissions */
	rc = permset_select_first(state->permset, permset_Select_Requested_And_Granted);
	while(rc) {
		rc = permit(state, permset_current(state->permset));
		if (rc < 0)
			goto end;
		rc = permset_select_next(state->permset, permset_Select_Requested_And_Granted);
	}

	/* also setup default permissions */
	n = (unsigned)(sizeof default_permissions / sizeof *default_permissions);
	for (i = 0 ; i < n ; i++) {
		rc = permit(state, default_permissions[i]);
		if (rc < 0)
			goto end;
	}

	/* setup the plugs */
	for_each_plug(state, set_plug_cb);
	rc = state->rc;

end:
	return rc;
}

static
int
setdown_security_file_cb(afmpkg_state_t *state, path_entry_t *entry,
                                    const char *path, size_t length)
{
	/* before uninstalling, set the id for making files inaccessible */
	return state->opers->tagfile(state->closure, state->path, path_type_Id);
}

static
int
setdown_security(
		afmpkg_state_t *state
) {
	for_each_entry(state, 0, setdown_security_file_cb);
	for_each_plug(state, set_plug_cb);
	return 0;
}

/*****************************************************************************/
/*** SETTING THE UNIT FILES **************************************************/
/*****************************************************************************/

/**
 * Add metadata to the JSON-C object of a target
 *
 * @param state  the current state
 * @param target the target
 */
static void add_meta_to_target(
		afmpkg_state_t *state,
		json_object *target
) {
	int rc, port, afid;
	json_object *object;

	if (state->mode != Afmpkg_Install) {
		/* fake values when uninstalling */
		afid = 0;
		port = 0;
	}
	else {
		/* compute available id */
		afid = get_new_afid();
		if (afid < 0) {
			RP_ERROR("Allocation of ID failed");
			state->rc = -EADDRNOTAVAIL;
			return;
		}
		port = DEFAULT_TCP_PORT_BASE + afid;
	}
	/* add global metadata */
	rc = rp_jsonc_pack(&object, "{si si}", "afid", afid, "http-port", port);
	if (rc < 0) {
		RP_ERROR("out of memory");
		state->rc = -ENOMEM;
		return;
	}
	json_object_object_add(target, "#metatarget", object);
}

/**
* Add to the manifest the metadata used when expanding
* mustache configuration files.
*
* The root object receive an object keyed '#metadata' containing the keys:
*
*  - root-dir: path of the root dir if any
*  - install-dir: path of installation directory relative to root directory
*  - icons-dir: path of the icon directory
*  - redpak: boolean indicating if installation occurs within a redpak container
*  - redpak-id: identifier of the redpak container or null
*
* Each target also recive metadata, see add_meta_to_target
*
* @param state      current state
*
* @return 0 in case of success or else a negative error code
*/
static int add_meta_to_manifest(
		afmpkg_state_t *state
) {
	json_object *object;
	int rc;

	/* compute installdir */
	if (state->offset_root != state->offset_pack)
		state->path[state->offset_pack] = 0;
	else {
		state->path[state->offset_root] = '/';
		state->path[state->offset_root + 1] = 0;
	}

	/* add global metadata */
	rc = rp_jsonc_pack(&object, "{ss ss sb ss* ss*}",
				"install-dir", &state->path[state->offset_root],
				"icons-dir", FWK_ICON_DIR,
				"redpak", state->apkg->redpakid != NULL,
				"redpak-id", state->apkg->redpakid,
				"root-dir", state->apkg->root);
	if (rc < 0) {
		RP_ERROR("out of memory");
		return -ENOMEM;
	}
	json_object_object_add(state->manifest, "#metadata", object);

	/* add per target meta data */
	rc = state->rc;
	state->rc = 0;
	for_each_target(state, add_meta_to_target);
	if (state->rc < 0)
		rc = state->rc;
	else
		state->rc = rc;
	return rc;
}

static
int
process_units(
		afmpkg_state_t *state
) {
	return unit_process_split(state->manifest, state->opers->setunits, state->closure);
}

/*****************************************************************************/
/*** INSTALLATION AND DEINSTALLATION OF AFMPKG *******************************/
/*****************************************************************************/

static
int
install_afmpkg(
	afmpkg_state_t *state
) {
	int rc;

	RP_NOTICE("-- Install afm pkg %s from manifest %s --", state->appid, state->path);

	/* creates the permission set */
	rc = permset_create(&state->permset);
	if (rc < 0) {
		RP_ERROR("can't create permset");
		goto error3;
	}

	/* check permissions */
	rc = check_permissions(state);
	if (rc < 0) {
		RP_ERROR("can't validate permission %s", state->appid);
		goto error4;
	}

	/* check content */
	rc = check_contents(state);
	if (rc < 0) {
		RP_ERROR("can't validate package content %s", state->appid);
		goto error4;
	}

	/* compute the security type of files */
	rc = compute_files_properties(state);
	if (rc < 0) {
		RP_ERROR("failed to setup afm pkg %s", state->appid);
		goto error4;
	}

	/* setup specific file properties */
	rc = setup_files_properties(state);
	if (rc < 0) {
		RP_ERROR("failed to setup afm pkg %s", state->appid);
		goto error4;
	}

	/* install security items */
	rc = setup_security(state);
	if (rc < 0) {
		RP_ERROR("failed to setup afm pkg %s", state->appid);
		goto error4;
	}

	/* generate and install units */
	rc = process_units(state);
error4:
	permset_destroy(state->permset);
error3:
	return rc;
}

static
int
uninstall_afmpkg(
	afmpkg_state_t *state
) {
	int rc;

	RP_NOTICE("-- Uninstall afm pkg %s from manifest %s --", state->appid, state->path);

	/* removed installed units */
	rc = process_units(state);
	if (rc < 0)
		RP_ERROR("can't set units down for %s", state->appid);
	else {
		/* uninstall security */
		rc = setdown_security(state);
		if (rc < 0)
			RP_ERROR("can't set security down for %s", state->appid);
	}

	return rc;
}

/** process a directory containing a redpesk application
 *  A redpesk application is a directory and all its content
 *  containing a manifest file denoted by 
 */
static
int
get_manifest(afmpkg_state_t *state, const char *manif)
{
	/* compute path of the manifest */
	state->path[state->offset_pack] = '/';
	strncpy(&state->path[state->offset_pack + 1], manif, sizeof state->path - 1  - state->offset_pack);

	/* read and check the manifest */
	if (manif == name_manifest)
		/* regular manifest */
		return manifest_read_and_check(&state->manifest, state->path);

	/* legacy config.xml */
	return config_read_and_check(&state->manifest, state->path);
}

/** process a directory containing a redpesk application
 *  A redpesk application is a directory and all its content
 *  containing a manifest file denoted by 
 */
static
int
process_package(afmpkg_state_t *state, const char *manif)
{
	struct json_object *id;
	int rc, rc2;

	RP_DEBUG("Processing AFMPKG package type %s found at %s", manif, state->path);

	/* TODO: process signatures */

	/* get the manifest the manifest */
	rc = get_manifest(state, manif);
	if (rc < 0) {
		RP_ERROR("Unable to get or validate manifest %s --", state->path);
		return rc;
	}
	/* shows the manifest on debug */
	RP_DEBUG("processing manifest %s",
                    json_object_to_json_string_ext(state->manifest,
                       JSON_C_TO_STRING_PRETTY|JSON_C_TO_STRING_NOSLASHESCAPE));

	/* add meta data to the manifest */
	rc = add_meta_to_manifest(state);
	if (rc < 0) {
		RP_ERROR("Unable to add metadata");
		goto cleanup;
	}

	/* get the application id, never NULL because manifest is checked */
	id = json_object_object_get(state->manifest, "id");
	state->appid = json_object_get_string(id);

	/* process */
	rc = state->opers->begin(state->closure, state->appid, state->mode);
	if (rc >= 0) {
		if (state->mode == Afmpkg_Install)
			rc = install_afmpkg(state);
		else
			rc = uninstall_afmpkg(state);
	}
	rc2 = state->opers->end(state->closure, rc);
	if (rc2 < 0)
		rc = rc2;

	/* clean up */
cleanup:
	json_object_put(state->manifest);
	state->manifest = NULL;
	state->appid = NULL;
	return rc;
}

/*****************************************************************************/
/*** SETTING DEFAULT SECURITY TAGS ON REMAINING FILES ************************/
/*****************************************************************************/

/** callback for adding path to security manager */
static
int
process_default_tree_add_cb(void *closure, path_entry_t *entry,
                                const char *path, size_t length)
{
	afmpkg_state_t *state = closure;
	return state->opers->tagfile(state->closure, state->path, path_type_Default);
}

/** callback for detecting that at least on path exists */
static
int
process_default_tree_detect_cb(void *closure, path_entry_t *entry,
                                   const char *path, size_t length)
{
	return 1;
}

/** process the default tree: the tree of installed files that
 *  are not part of redpesk packaged application (missing manifest)
 */
static
int
process_default_tree(afmpkg_state_t *state, path_entry_t *root)
{
	int rc = 0;

	/* setting label on default tree is currently only needed at install */
	if (state->mode == Afmpkg_Install) {

		/* detect if an entry fits a path of the original set
		 * the directories that are implied must be ignored */
		rc = path_entry_for_each_in_buffer(
				PATH_ENTRY_FORALL_ONLY_ADDED
				 | PATH_ENTRY_FORALL_NO_PATH
				 | PATH_ENTRY_FORALL_BEFORE,
				root,
				process_default_tree_detect_cb,
				NULL,
				NULL,
				0);
		if (rc > 0) {
			rc = state->opers->begin(state->closure, NULL, state->mode);
			if (rc >= 0) {
				rc = path_entry_for_each_in_buffer(
					PATH_ENTRY_FORALL_ONLY_ADDED
					 | PATH_ENTRY_FORALL_BEFORE,
					root,
					process_default_tree_add_cb,
					state,
					&state->path[state->offset_root],
					sizeof state->path - state->offset_root);
			}
			rc = state->opers->end(state->closure, rc);
			if (rc < 0)
				RP_ERROR("Unable to set default security");
		}
	}
	return rc;
}

/*****************************************************************************/
/*** DETECT INCLUDED PACKAGE, LOOP ON IT                                   ***/
/*****************************************************************************/

/**
* Record for each package root the
* entry end the type.
*/
typedef
struct {
	/** the entry */
	path_entry_t *entry;
	/** the type */
	const char *type;
}
	rootpkg_item_t;

/**
* Record the package roots as an array.
* The array leads to memory allocation only if it becomes huge.
*/
typedef
struct {
	/** count of root found */
	unsigned count;
	/** size of the array of roots */
	unsigned alloc;
	/** array of roots */
	rootpkg_item_t *roots;
	/** preallocated array of roots */
	rootpkg_item_t defroots[10];
}
	rootpkgs_t;

/**
* initialize a rootpkgs_t instance
*
* @param roots the instance to initialize
*/
static
void
rootpkgs_init(
	rootpkgs_t *roots
) {
	roots->count = 0;
	roots->alloc = sizeof roots->defroots / sizeof roots->defroots[0];
	roots->roots = roots->defroots;
}

/**
* uninitialize a rootpkgs_t instance, freeing the memory if needed
*
* @param roots the instance to uninitialize
*/
static
void
rootpkgs_uninit(
	rootpkgs_t *roots
) {
	if (roots->roots != roots->defroots)
		free(roots->roots);
}

/**
* add a root of package
*
* @param roots the roots where adding is done
* @param entry the entry to add
* @param type the type of the entry
*
* @return 0 on success or -ENOMEM on allocation error.
*/
static
int
rootpkgs_add(
	rootpkgs_t *roots,
	path_entry_t *entry,
	const char *type
) {
	rootpkg_item_t *root;

	/* is allocation needed? is it full? */
	if (roots->count == roots->alloc) {
		/* yes, allocate */
		unsigned nall = 2 * roots->alloc;
		rootpkg_item_t *nroots = malloc(nall * sizeof *nroots);
		if (nroots == NULL) {
			RP_ERROR("out of memory");
			return -ENOMEM;
		}
		/* copy */
		roots->alloc = nall;
		memcpy(nroots, roots->roots, roots->count * sizeof *nroots);
		if (roots->roots != roots->defroots)
			free(roots->roots);
		roots->roots = nroots;
	}
	/* add now */
	root = &roots->roots[roots->count++];
	root->entry = entry;
	root->type = type;
	return 0;
}

/**
* for each package root found (in the found order) calls the callback
* stops when the ccallbvack returned a not zero value
*
* @param roots    the roots to use
* @param callback the function to call
* @param closure  closure for the function
*
* @return 0 if callback always return 0 otherwise the value return by the callback
*/
static
int
rootpkgs_for_each(
	rootpkgs_t *roots,
	int (*callback)(void *closure, path_entry_t *entry, const char *type),
	void *closure
) {
	rootpkg_item_t *iter = roots->roots;
	rootpkg_item_t *end = &roots->roots[roots->count];
	int rc = 0;
	while(rc == 0 && iter != end) {
		rc = callback(closure, iter->entry, iter->type);
		iter++;
	}
	return rc;
}

/*
* Detect the type of the current entry. The type is identified by the name of
* the manifest file. The currently known manifests are:
*  - name_manifest for '.rpconfig/manifest.yml'
*  - name_config for 'config.xml'
*/
static
int
detect_package_roots_cb(
	void *closure,
	path_entry_t *entry,
	const char *path,
	size_t length
) {
	const char *pkg;
	int rc = 0;

	if (path_entry_has_child(entry)) {
		/* check if at root of a known entry type */
		if (0 == path_entry_get_length(entry, NULL, name_manifest,
							(sizeof name_manifest) - 1))
			pkg = name_manifest;
		else if (0 == path_entry_get_length(entry, NULL, name_config,
							(sizeof name_config) - 1))
			pkg = name_config;
		else
			pkg = NULL;
		if (pkg != NULL)
			rc = rootpkgs_add(closure, entry, pkg);
	}
	return rc;
}

/**
* process one package root
*/
static
int
process_rootpkg(void *closure, path_entry_t *entry, const char *type)
{
	int rc;
	afmpkg_state_t *state = closure;

	/* reset current state */
	state->rc = 0;
	state->permset = NULL;
	state->manifest = NULL;
	state->appid = NULL;

	/* set current package directory */
	state->packdir = entry;

	/* compute the path of the root of the package */
	state->offset_pack = state->offset_root
			   + path_entry_path(entry,
					&state->path[state->offset_root],
					sizeof state->path - state->offset_root,
					PATH_ENTRY_FORCE_LEADING_SLASH);
	state->path[state->offset_pack] = 0;

	/* process the package */
	rc = process_package(state, type);

	/* remove processed subtree */
	path_entry_destroy(state->packdir);
	if (state->packdir == state->files)
		state->files = NULL;

	return rc;
}

/*
* Common processing routine for installation, uninstallation or just check
*/
static
int
afmpkg_process(
	const afmpkg_t *apkg,
	const afmpkg_operations_t *opers,
	void *closure,
	afmpkg_mode_t mode
) {
	int rc;
	afmpkg_state_t state;
	rootpkgs_t roots;

	/* basic state init */
	state.apkg = apkg;
	state.opers = opers;
	state.closure = closure;
	state.mode = mode;
	state.files = apkg->files;

	/* Prepare path buffer of the state
	 * When apkg->root is not NULL it will prefix
	 * any path. */
	if (apkg->root == NULL)
		state.offset_root = 0;
	else {
		state.offset_root = strlen(apkg->root);
		if (state.offset_root >= PATH_MAX) {
			RP_ERROR("name too long %.200s...", apkg->root);
			return -ENAMETOOLONG;
		}
		memcpy(state.path, apkg->root, state.offset_root + 1);
		while (state.offset_root > 0
		    && state.path[state.offset_root - 1] == '/')
			--state.offset_root;
	}
	state.path[state.offset_root] = 0;
	RP_DEBUG("Processing AFMPKG at root %s", state.path);

	/* Inspect the files to find package directories.
	 * The found list has embeded directories before embedding ones. */
	rootpkgs_init(&roots);
	rc = path_entry_for_each_in_buffer(
			PATH_ENTRY_FORALL_NO_PATH | PATH_ENTRY_FORALL_AFTER,
			apkg->files,
			detect_package_roots_cb,
			&roots,
			NULL,
			0);

	/* process each found packages of entries */
	if (rc >= 0)
		rc = rootpkgs_for_each(&roots, process_rootpkg, &state);
	rootpkgs_uninit(&roots);

	/* process remaining files */
	if (rc >= 0 && state.files != NULL) {
		RP_DEBUG("Processing AFMPKG remaining files");
		rc = process_default_tree(&state, state.files);
	}

	RP_DEBUG("Processing AFMPKG ends with code %d", rc);
	return rc;
}

/* install afm package */
int
afmpkg_install(
	const afmpkg_t *apkg,
	const afmpkg_operations_t *opers,
	void *closure
) {
	return afmpkg_process(apkg, opers, closure, Afmpkg_Install);
}

/* install afm package */
int
afmpkg_uninstall(
	const afmpkg_t *apkg,
	const afmpkg_operations_t *opers,
	void *closure
) {
	return afmpkg_process(apkg, opers, closure, Afmpkg_Uninstall);
}

/*****************************************************************************/
/*** LEGACY WIDGETS WITH CONFIG.XML ******************************************/
/*****************************************************************************/
#if WITH_CONFIG_XML

#include "wgt-json.h"

static
int
config_read_and_check(struct json_object **obj, const char *path)
{
	int rc = 0;
	*obj = wgt_config_to_json(path);

	if (*obj == NULL) {
		RP_ERROR("can't read config file %s", path);
		rc = -errno;
	}
	else {
		json_object_object_add(*obj, MANIFEST_RP_MANIFEST, json_object_new_int(1));
		rc = manifest_check(*obj);
		if (rc >= 0)
			rc = manifest_normalize(*obj, path);
		else
			RP_ERROR("constraints of manifest not fulfilled for %s", path);
		if (rc < 0) {
			json_object_put(*obj);
			*obj = NULL;
		}
	}
	return rc;
}

#else /* WITH_CONFIG_XML */

static
int
config_read_and_check(struct json_object **obj, const char *path)
{
	RP_ERROR("config.xml panifest aren't supported anymore (%s)", path);
	return -ENOTSUP;
}

#endif /* WITH_CONFIG_XML */


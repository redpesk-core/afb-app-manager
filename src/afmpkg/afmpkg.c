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

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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

#include "unit-generator.h"

#include "secmgr-wrap.h"

#include "afmpkg.h"

#if !defined(DEFAULT_TCP_PORT_BASE)
#define DEFAULT_TCP_PORT_BASE				29000
#endif

/**
* processing structure
*/
typedef
struct
{
	/** the request */
	const afmpkg_t *apkg;

	/** should install? */
	bool install;

	/** should uninstall? */
	bool uninstall;

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
	process_state_t;


static const char name_manifest[] = ".rpconfig/manifest.yml";
static const char name_config[] = "config.xml";
static const char key_pkg[] = "pkg";
static const char key_type[] = "type";
static const char key_next[] = "next";

/** default given permissions */
static const char *default_permissions[] = {
	"urn:AGL:token:valid"
};

/* predeclaration of legacy functions (avoids including legacy headers at top) */
static int process_legacy_config(process_state_t *state);

/*********************************************************************************************/
/*** MANAGE ENTRY TYPE ***********************************************************************/
/*********************************************************************************************/

/** returns the path type attached to the entry */
static
path_type_t get_entry_type(const path_entry_t *entry)
{
	void *value = path_entry_var(entry, key_type);
	return (path_type_t)(intptr_t)value;
}

/** attach a path type attached to the entry */
static
int set_entry_type(path_entry_t *entry, path_type_t type)
{
	void *value = (void*)(intptr_t)type;
	return path_entry_var_set(entry, key_type, value, NULL);
}

/*********************************************************************************************/
/*** ITERATION OVER JSON *********************************************************************/
/*********************************************************************************************/

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
void for_each_of_manifest(process_state_t *state, const char *key, void (*fun)(process_state_t*, json_object*))
{
	json_object *item;
	if (json_object_object_get_ex(state->manifest, key, &item))
		rp_jsonc_array_for_all(item, (void(*)(void*,json_object*))fun, state);
}

static
void for_each_target(process_state_t *state, void (*fun)(process_state_t*, json_object*))
{
	for_each_of_manifest(state, MANIFEST_TARGETS, fun);
}

/*********************************************************************************************/
/*** ITERATION OVER PLUGS ********************************************************************/
/*********************************************************************************************/

struct plug_cb {
	process_state_t *state;
	void (*fun)(process_state_t*, path_entry_t*, const char*, const char*);
};

static void for_each_plug_cb(void *closure, json_object *desc)
{
	int rc = 0;
	json_object *item;
	const char *expdir;
	const char *impid;
	path_entry_t *entry;
	char path[PATH_MAX + 1];
	struct plug_cb *pcb = closure;
	process_state_t *state = pcb->state;

	/* get the id (name) */
	if (!json_object_object_get_ex(desc, MANIFEST_NAME, &item)) {
		rc = -EINVAL;
		RP_ERROR("name missing in plug");
	}
	else {
		expdir = json_object_get_string(item);

		/* get exported directory (value) */
		if (!json_object_object_get_ex(desc, MANIFEST_VALUE, &item)) {
			rc = -EINVAL;
			RP_ERROR("value missing in plug");
		}
		else {
			impid = json_object_get_string(item);

			/* get entry of exported directory */
			rc = path_entry_get(state->packdir, &entry, expdir);
			if (rc < 0)
				RP_ERROR("invalid plug path %s", expdir);
			else {
				/* get import path directory */
				snprintf(&state->path[state->offset_pack],
					sizeof state->path - state->offset_pack,
					"/%s", expdir);

				/* get import path directory */
				rc = snprintf(path, sizeof path,
					"%.*s%s/%s/plugins",
					state->offset_root, state->path,
					FWK_APP_DIR, impid);
				if (rc <= 0 || rc >= (int)sizeof path) {
					rc = -ENAMETOOLONG;
					RP_ERROR("can't set impdir path");
				}
				else {
					pcb->fun(state, entry, impid, path);
				}
			}
		}
	}
	if (rc < 0 && state->rc >= 0)
		state->rc = rc;
}

/** iterate over the plugs of the package */
static void for_each_plug(process_state_t *state, void (*fun)(process_state_t*, path_entry_t*, const char*, const char*))
{
	struct plug_cb pcb;
	json_object *item;

	if (json_object_object_get_ex(state->manifest, MANIFEST_PLUGS, &item)) {
		pcb.state = state;
		pcb.fun = fun;
		rp_jsonc_array_for_all(item, for_each_plug_cb, &pcb);
	}
}

/*********************************************************************************************/
/*** ITERATION OVER FILES ********************************************************************/
/*********************************************************************************************/

/** iterate over the entries of the package */
static
int for_each_entry(
	process_state_t *state,
	unsigned flags,
	int (*fun)(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
) {
	unsigned offset = state->offset_pack;
	if (!(flags & PATH_ENTRY_FORALL_SILENT_ROOT)) {
		unsigned toplen = path_entry_length(state->packdir);
		if (offset > toplen)
			offset -= toplen + 1;
	}
	return path_entry_for_each_in_buffer(flags, state->packdir, (path_entry_for_each_cb_t)fun, state,
			&state->path[offset], sizeof state->path - offset);
}

/** iterate over the entries of the package */
static
int for_each_content_entry(
	process_state_t *state,
	unsigned flags,
	int (*fun)(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
) {
	return for_each_entry(state, flags | PATH_ENTRY_FORALL_SILENT_ROOT, fun);
}

/*********************************************************************************************/
/*** MANAGE STATE RC *************************************************************************/
/*********************************************************************************************/

static inline
void put_state_rc(process_state_t *state, int rc)
{
	if (rc < 0 && state->rc >= 0)
		state->rc = rc;
}

/*********************************************************************************************/
/*** CHECKING PERMISSIONS ********************************************************************/
/*********************************************************************************************/

static void check_one_permission_cb(void * closure, json_object *jso, const char *key)
{
	process_state_t *state = closure;
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

static void check_permissions_cb(process_state_t *state, json_object *jso)
{
	json_object *perms;
	if (json_object_object_get_ex(jso, MANIFEST_REQUIRED_PERMISSIONS, &perms))
		rp_jsonc_object_for_all(perms, check_one_permission_cb, state);
}

static int check_permissions(process_state_t *state)
{
	check_permissions_cb(state, state->manifest);
	for_each_target(state, check_permissions_cb);
	return 0;
}

/*********************************************************************************************/
/*** CHECKING CONTENT ************************************************************************/
/*********************************************************************************************/

/*
* check the src and type of one target
*/
static int check_src_type_definition(process_state_t *state, const char *src, const char *type)
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
* check definition of one target
*/
static void check_target_content_cb(process_state_t *state, json_object *jso)
{
	json_object *content, *src, *type;
	int rc = -EINVAL;

	/* check if has a content */
	if (!json_object_object_get_ex(jso, "content", &content))
		RP_ERROR("no content %s", json_object_get_string(jso));

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

/*
* checks that any target of the manifest is correctly setup
*/
static int check_contents(process_state_t *state)
{
	for_each_target(state, check_target_content_cb);
	return state->rc;
}

/*********************************************************************************************/
/*** COMPUTE FILE PROPERTIES  ****************************************************************/
/*********************************************************************************************/

/* callback for resetting the path type to UNKNOWN */
static int reset_type_cb(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	int rc = set_entry_type(entry, path_type_Unknown);
	put_state_rc(state, rc);
	return 0;
}

/* callback for implementing file-properties configuration */
static void compute_explicit_file_properties_cb(process_state_t *state, json_object *jso)
{
	path_entry_t *entry;
	json_object *name, *value;
	const char *strval;
	path_type_t type;
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
			/* detect duplication of explicit name */
			type = get_entry_type(entry);
			if (type != path_type_Unknown) {
				RP_ERROR("file duplication %s", json_object_get_string(jso));
				rc = -EEXIST;
			}
			else {
				/* compute the effective path type of value */
				strval = json_object_get_string(value);
				type = path_type_of_key(strval);
				if (type != path_type_Unknown)
					set_entry_type(entry, type);
				else {
					RP_ERROR("invalid value %s", json_object_get_string(jso));
					rc = -EINVAL;
				}
			}
		}
	}
	put_state_rc(state, rc);
}

/* callback for implementing plug exporting property */
static void compute_implicit_plug_property_cb(process_state_t *state, json_object *jso)
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

/* callback possibly deducing file type for contents */
static void compute_target_file_properties_cb(process_state_t *state, json_object *jso)
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
		else if (get_entry_type(entry) == path_type_Unknown) {
			/* the file exists but is of unknown type */
			if (mime_type_is_executable(json_object_get_string(type)))
				/* set as executable for known mime-type */
				set_entry_type(entry, path_type_Exec);
		}
	}
}

/* callback for computing default file properties */
static int compute_default_files_properties_cb(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
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
		if (curtype != path_type_Unknown)
			rc = 0;
		else {
			if (S_ISDIR(s.st_mode))
				curtype = path_type_of_dirname(path_entry_name(entry));
			if (curtype == path_type_Unknown)
				curtype = get_entry_type(path_entry_parent(entry));
			if (curtype == path_type_Unknown)
				curtype = path_type_Id;
			rc = set_entry_type(entry, curtype);
		}
	}
	put_state_rc(state, rc);
	return rc;
}

/* callback for propagation of public status */
static int compute_public_files_properties_cb(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
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
static int compute_files_properties(process_state_t *state)
{
	/* reset any type */
	state->rc = set_entry_type(state->packdir, path_type_Id);
	if (state->rc >= 0)
		for_each_content_entry(state, PATH_ENTRY_FORALL_NO_PATH, reset_type_cb);
	/* set implicit plugin types */
	if (state->rc >= 0)
		for_each_of_manifest(state, MANIFEST_PLUGS, compute_implicit_plug_property_cb);
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

/*********************************************************************************************/
/*** MAKE FILE PROPERTIES EFFECTIVE **********************************************************/
/*********************************************************************************************/

/* set execution property of files */
static int make_file_executable(const char *filename)
{
	int rc = chmod(filename, 0755);
	if (rc < 0) {
		RP_ERROR("can't make file executable %s", filename);
		rc = -errno;
	}
	return rc;
}

/* callback for applying DAC properties accordingly to path types */
static int setup_file_properties_cb(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
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
static int setup_files_properties(process_state_t *state)
{
	for_each_content_entry(state, 0, setup_file_properties_cb);
	return state->rc;
}

/*********************************************************************************************/
/*** SETTING OF THE SECURITY ITEMS ***********************************************************/
/*********************************************************************************************/

static const char * const type_types[] = {
    [path_type_Unset] = NULL,
    [path_type_Unknown] = NULL,
    [path_type_Conf] = secmgr_pathtype_conf,
    [path_type_Data] = secmgr_pathtype_data,
    [path_type_Exec] = secmgr_pathtype_exec,
    [path_type_Http] = secmgr_pathtype_http,
    [path_type_Icon] = secmgr_pathtype_icon,
    [path_type_Id] = secmgr_pathtype_id,
    [path_type_Lib] = secmgr_pathtype_lib,
    [path_type_Plug] = secmgr_pathtype_plug,
    [path_type_Public] = secmgr_pathtype_public,
    [path_type_Public_Exec] = secmgr_pathtype_public,
    [path_type_Public_Lib] = secmgr_pathtype_public
};

static int setup_security_file_cb(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	const char *typath = NULL;
	const char *realpath = state->path;
	int rc = 0;
	unsigned idxpty;

	idxpty = (unsigned)get_entry_type(entry);
	if (idxpty < (unsigned)(sizeof type_types / sizeof *type_types))
		typath = type_types[idxpty];
	if (typath == NULL) {
		RP_DEBUG("unknown path type: %s", realpath);
		typath = secmgr_pathtype_conf;
	}
	rc = secmgr_path(realpath, typath);
	put_state_rc(state, rc);
	return 0;
}

static int permit(process_state_t *state, const char *perm)
{
	int rc;

	RP_INFO("Permits %s", perm);

	rc = secmgr_permit(perm);
	if (rc < 0)
		RP_ERROR("Fails to permit %s", perm);
	return rc;
}

static void set_secmgr_plug(process_state_t *state, path_entry_t *entry, const char *impid, const char *impdir)
{
	int rc = secmgr_plug(state->path, impid, impdir);
	if (rc < 0) {
		RP_ERROR("can't add plug");
		if (state->rc >= 0)
			state->rc = rc;
	}
}

static int setup_security(process_state_t *state)
{
	int rc;
	unsigned i, n;

	/* starts a transaction with the security manager */
	rc = secmgr_begin(state->appid);
	if (rc < 0) {
		RP_ERROR("can't initialize security-manager context");
		goto end;
	}

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
			goto cancel;
		rc = permset_select_next(state->permset, permset_Select_Requested_And_Granted);
	}

	/* also setup default permissions */
	n = (unsigned)(sizeof default_permissions / sizeof *default_permissions);
	for (i = 0 ; i < n ; i++) {
		rc = permit(state, default_permissions[i]);
		if (rc < 0)
			goto cancel;
	}

	/* setup the plugs */
	for_each_plug(state, set_secmgr_plug);
	rc = state->rc;
	if (rc < 0)
		goto end;

	/* installs the setting now, it commits the transaction */
	rc = secmgr_install();

cancel:
	/* terminate the transaction, cancelling it if not commited */
	secmgr_end();

end:
	return rc;
}

static int setdown_security_file_cb(process_state_t *state, path_entry_t *entry, const char *path, size_t length)
{
	/* before uninstalling, set the id for making files inaccessible */
	secmgr_path_id(state->path);
	return 0;
}

static int setdown_security(
		process_state_t *state
) {
	int rc = secmgr_begin(state->appid);
	if (rc < 0)
		RP_ERROR("can't init sec lsm manager context");
	else {
		for_each_entry(state, 0, setdown_security_file_cb);
		for_each_plug(state, set_secmgr_plug);
		rc = secmgr_uninstall();
		secmgr_end();
		if (rc < 0)
			RP_ERROR("can't uninstall sec lsm manager context");
	}
	return rc;
}

/*********************************************************************************************/
/*** SETTING THE UNIT FILES ******************************************************************/
/*********************************************************************************************/

/**
* Creates the JSON-C object containing metadata used when exanding
* mustache configuration files.
*
* The created object will contain the keys:
*
*  - root-dir: pathe of the root dir if any
*  - install-dir: path of installation directory relative to root directory
*  - icons-dir: path of the icon directory
*  - redpak: boolean indicating if installation occurs within a redpak container
*  - redpak-id: identifier of the redpak container or null
*
* The values of the keys are extracted from the given parameters.
*
* @param object pointer for storing the result
* @param apkg   the request object
* @param installdir relative path of installation directory
*
* @return 0 in case of success or else returns -ENOMEM
*/
static int make_install_metadata(
	json_object **object,
	const afmpkg_t *apkg,
	const char *installdir
) {
	int rc = rp_jsonc_pack(object, "{ss ss sb ss* ss*}",
				"install-dir", installdir,
				"icons-dir", FWK_ICON_DIR,
				"redpak", apkg->redpakid != NULL,
				"redpak-id", apkg->redpakid,
				"root-dir", apkg->root);
	return rc == 0 ? 0 : -ENOMEM;
}

static int setup_units(
		process_state_t *state,
		const char *installdir
) {
	struct unitconf uconf;
	int rc = make_install_metadata(&uconf.metadata,
				state->apkg, installdir);
	if (rc == 0) {
		uconf.new_afid = get_new_afid;
		uconf.base_http_ports = DEFAULT_TCP_PORT_BASE;
		rc = unit_generator_install(state->manifest, &uconf);
		json_object_put(uconf.metadata);
	}
	return rc;
}

static int setdown_units(
		process_state_t *state
) {
	struct unitconf uconf;
	uconf.metadata = NULL;
	uconf.new_afid = get_new_afid;
	uconf.base_http_ports = DEFAULT_TCP_PORT_BASE;
	return unit_generator_uninstall(state->manifest, &uconf);
}

/*********************************************************************************************/
/*** INSTALLATION AND DEINSTALLATION OF AFMPKG ***********************************************/
/*********************************************************************************************/

static
int
install_afmpkg(
	process_state_t *state
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
	if (state->offset_root != state->offset_pack)
		state->path[state->offset_pack] = 0;
	else {
		state->path[state->offset_root] = '/';
		state->path[state->offset_root + 1] = 0;
	}
	rc = setup_units(state, &state->path[state->offset_root]);
error4:
	permset_destroy(state->permset);
error3:
	return rc;
}

static
int
uninstall_afmpkg(
	process_state_t *state
) {
	int rc;

	RP_NOTICE("-- Uninstall afm pkg %s from manifest %s --", state->appid, state->path);

	/* removed installed units */
	rc = setdown_units(state);
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

static int process_manifest(process_state_t *state)
{
	int rc;

	/* TODO: process signatures */
	/* read the manifest */
	state->path[state->offset_pack] = '/';
	strncpy(&state->path[state->offset_pack + 1], name_manifest, sizeof state->path - 1  - state->offset_pack);
	rc = manifest_read_and_check(&state->manifest, state->path);
	if (rc < 0)
		RP_ERROR("Unable to get or validate manifest %s --", state->path);
	else {
		RP_DEBUG("processing manifest %s", json_object_to_json_string_ext(state->manifest,
		                             JSON_C_TO_STRING_PRETTY|JSON_C_TO_STRING_NOSLASHESCAPE));

		state->appid = json_object_get_string(json_object_object_get(state->manifest, "id"));
		if (state->install)
			return install_afmpkg(state);
		if (state->uninstall)
			return uninstall_afmpkg(state);
		json_object_put(state->manifest);
	}
	return rc;
}

/*********************************************************************************************/
/*** SETTING DEFAULT SECURITY TAGS ON REMAINING FILES ****************************************/
/*********************************************************************************************/

/** callback for adding path to security manager */
static
int process_default_tree_add_cb(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	process_state_t *state = closure;
	return secmgr_path_default(state->path);
}

/** callback for detecting that at least on path exists */
static
int process_default_tree_detect_cb(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	return 1;
}

static int process_default_tree(process_state_t *state, path_entry_t *root)
{
	int rc = 0;
	if (state->install || state->uninstall) {
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
			rc = secmgr_begin(NULL);
			if (rc >= 0) {
				rc = path_entry_for_each_in_buffer(
					PATH_ENTRY_FORALL_ONLY_ADDED
					 | PATH_ENTRY_FORALL_BEFORE,
					root,
					process_default_tree_add_cb,
					state,
					&state->path[state->offset_root],
					sizeof state->path - state->offset_root);

				if (rc >= 0)
					rc = state->install ? secmgr_install() : secmgr_uninstall();
				secmgr_end();
			}
			if (rc < 0)
				RP_ERROR("Unable to set default security");
		}
	}
	return rc;
}

/*********************************************************************************************/
/*** DETECT INCLUDED PACKAGE, LOOP ON IT                                                   ***/
/*********************************************************************************************/

/*
* Detect the type of the current entry. The type is identified by the name of
* the manifest file. The currently known manifests are:
*  - name_manifest for '.rpconfig/manifest.yml'
*  - name_config for 'config.xml'
*
* When the current entry has a sub-entry of the given name (found with path_entry_get_length),
* it is acted has being the root of a package. So two variables are set for the entry:
*  - with key_pkg, the detected pkg type
*  - with key_next, a link to the previous package entry found
*
* Linking the entries together in the reverse order of the found order when inspecting
* file tree in deep-last order means that children of a root are seen first when the list
* is scanned later.
*/
static
int detect_package_cb(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	const char *pkg;
	path_entry_t **head;
	int rc = 0;

	if (path_entry_has_child(entry)) {
		/* check if at root of a known entry type */
		if (0 == path_entry_get_length(entry, NULL, name_manifest, (sizeof name_manifest) - 1))
			pkg = name_manifest;
		else if (0 == path_entry_get_length(entry, NULL, name_config, (sizeof name_config) - 1))
			pkg = name_config;
		else
			pkg = NULL;
		if (pkg != NULL) {
			/* set the pkg type in a linked list reverse order */
			rc = path_entry_var_set(entry, key_pkg, (void*)pkg, NULL);
			if (rc == 0) {
				head = closure;
				rc = path_entry_var_set(entry, key_next, *head, NULL);
				if (rc == 0)
					*head = entry;
			}
			if (rc < 0)
				RP_ERROR("out of memory");
		}
	}
	return rc;
}

/*
* Common processing routine for installation, uninstallation or just check
*/
static
int afmpkg_process(
	const afmpkg_t *apkg,
	bool install,
	bool uninstall
) {
	int rc;
	process_state_t state;
	path_entry_t *entries, *root;
	const char *curpkg;

	/* prepare data for processing */
	state.apkg = apkg;
	state.install = install;
	state.uninstall = uninstall;
	if (apkg->root == NULL)
		state.offset_root = 0;
	else {
		state.offset_root = strlen(apkg->root);
		if (state.offset_root >= PATH_MAX) {
			RP_ERROR("name too long %.200s...", apkg->root);
			return -ENAMETOOLONG;
		}
		memcpy(state.path, apkg->root, state.offset_root + 1);
		if (state.offset_root > 0 && state.path[state.offset_root - 1] == '/')
			--state.offset_root;
	}
	state.path[state.offset_root] = 0;
	RP_DEBUG("Processing AFMPKG at root %s", state.path);

	/* scan the packages: builds the list of entries */
	entries = NULL;
	rc = path_entry_for_each_in_buffer(
			PATH_ENTRY_FORALL_NO_PATH | PATH_ENTRY_FORALL_BEFORE,
			apkg->files,
			detect_package_cb,
			&entries,
			NULL,
			0);

	/* process the found packages of entries */
	root = apkg->files;
	while (rc >= 0 && entries != NULL) {
		/* reset current state */
		state.rc = 0;
		state.permset = NULL;
		state.manifest = NULL;
		state.appid = NULL;

		/* extract current entry and remove it from the list */
		state.packdir = entries;
		entries = path_entry_var(state.packdir, key_next);

		/* compute the path of the base of the package */
		state.offset_pack = state.offset_root
		                  + path_entry_path(state.packdir,
						&state.path[state.offset_root],
						sizeof state.path - state.offset_root,
						PATH_ENTRY_FORCE_LEADING_SLASH);
		state.path[state.offset_pack] = 0;

		/* process the package subtree */
		curpkg = path_entry_var(state.packdir, key_pkg);
		RP_DEBUG("Processing AFMPKG package type %s found at %s", curpkg, state.path);
		if (curpkg == name_manifest)
			rc = process_manifest(&state);
		else
			rc = process_legacy_config(&state);

		/* remove processed subtree */
		path_entry_destroy(state.packdir);
		if (state.packdir == root)
			root = NULL;
	}

	/* process remaining files */
	if (rc >= 0 && root != NULL) {
		RP_DEBUG("Processing AFMPKG remaining files");
		rc = process_default_tree(&state, root);
	}

	RP_DEBUG("Processing AFMPKG ends with code %d", rc);
	return rc;
}

/* install afm package */
int afmpkg_install(
	const afmpkg_t *apkg
) {
	return afmpkg_process(apkg, true, false);
}

/* install afm package */
int afmpkg_uninstall(
	const afmpkg_t *apkg
) {
	return afmpkg_process(apkg, false, true);
}

/*********************************************************************************************/
/*** LEGACY WIDGETS WITH CONFIG.XML **********************************************************/
/*********************************************************************************************/
#if WITH_CONFIG_XML

#include "wgt-json.h"

static int config_read_and_check(struct json_object **obj, const char *path)
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

static int process_legacy_config(process_state_t *state)
{
	int rc;

	/* TODO: process signatures */
	/* read the manifest */
	state->path[state->offset_pack] = '/';
	strncpy(&state->path[state->offset_pack + 1], name_config, sizeof state->path - 1  - state->offset_pack);
	rc = config_read_and_check(&state->manifest, state->path);
	if (rc < 0)
		RP_ERROR("Unable to get or validate config %s --", state->path);
	else {
		RP_DEBUG("processing config %s", json_object_to_json_string_ext(state->manifest,
		                             JSON_C_TO_STRING_PRETTY|JSON_C_TO_STRING_NOSLASHESCAPE));

		state->appid = json_object_get_string(json_object_object_get(state->manifest, "id"));
		if (state->install)
			return install_afmpkg(state);
		if (state->uninstall)
			return uninstall_afmpkg(state);
		json_object_put(state->manifest);
	}
	return rc;
}

#else /* WITH_CONFIG_XML */

static
int
process_legacy_config(
	process_state_t *state
) {
	RP_ERROR("Failed to (un)install widget %s: widgets aren't supported anymore", state->path);
	return -ENOTSUP;
}

#endif /* WITH_CONFIG_XML */


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

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-jsonc.h>

#include "cert/signed-digest.h"
#include "cert/signature-name.h"

#include "detect-packtype.h"
#include "manage-afid.h"
#include "manifest.h"
#include "mime-type.h"
#include "normalize-unit-file.h"
#include "path-entry.h"
#include "path-type.h"
#include "permset.h"
#include "sighup-framework.h"

#include "wgtpkg-unit.h"

#include "secmgr-wrap.h"

#include "afmpkg.h"


#define HTTP_PORT_BASE				29000


typedef
struct install_state
{
	/** for returning the status */
	int rc;

	/** the installed afmpkg object */
	const afmpkg_t *apkg;

	/** the json object of the manifest */
	json_object *manifest;

	/** the set of permission s*/
	permset_t *permset;

	/** path entry for the content */
	path_entry_t *content;

	/** offset of root in path */
	unsigned offset_root;

	/** offset of package in path */
	unsigned offset_pack;

	/** buffer for setting paths */
	char *path;
}
	install_state_t;


static const char *default_permissions[] = {
	"urn:AGL:token:valid"
};



path_type_t get_entry_type(const path_entry_t *entry)
{
	void *value = path_entry_var(entry, get_entry_type);
	return (path_type_t)(intptr_t)value;
}

int set_entry_type(path_entry_t *entry, path_type_t type)
{
	void *value = (void*)(intptr_t)type;
	return path_entry_var_set(entry, get_entry_type, value, NULL);
}





int for_each_content_entry(
	unsigned flags,
	install_state_t *state,
	int (*fun)(void *closure, path_entry_t *entry, const char *path, size_t length)
) {
	return path_entry_for_each_in_buffer(flags | PATH_ENTRY_FORALL_SILENT_ROOT, state->content, fun, state,
			&state->path[state->offset_root], PATH_MAX - state->offset_root);
}


static void for_each_of(json_object *jso, void (*fun)(void*, json_object*), void *closure, const char *key)
{
	json_object *item;
	if (json_object_object_get_ex(jso, key, &item))
		rp_jsonc_array_for_all(item, fun, closure);
}

static void process_one_required_permission(void * closure, json_object *jso)
{
	permset_t *permset = closure;
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

static void process_required_permissions_of(void * closure, json_object *jso)
{
	json_object *item;
	if (json_object_object_get_ex(jso, MANIFEST_REQUIRED_PERMISSIONS, &item))
		rp_jsonc_array_for_all(item, process_one_required_permission, closure);
}

static int check_permissions(install_state_t *state)
{
	process_required_permissions_of(state->permset, state->manifest);
	for_each_of(state->manifest, process_required_permissions_of, state->permset, MANIFEST_TARGETS);
	return 0;
}



static int check_one_content(const char *src, const char *type, install_state_t *state)
{
	int rc;
	struct stat s;
	size_t length;
	path_entry_t *entry;

	rc = path_entry_get(state->content, &entry, src);
	if (rc < 0) {
		rc = path_entry_get(state->content, &entry, "htdocs");
		if (rc >= 0)
			rc = path_entry_get(entry, &entry, src);
		if (rc < 0) {
			RP_ERROR("file not installed %s", src);
			return -ENOENT;
		}
	}

	length = path_entry_get_relpath(entry, &state->path[state->offset_root], PATH_MAX - state->offset_root, state->content);
	if (length + state->offset_root > PATH_MAX) {
		RP_ERROR("filename too long %s", src);
		return -ENAMETOOLONG;
	}

	rc = fstatat(AT_FDCWD, state->path, &s, AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
	if (rc < 0) {
		rc = -errno;
		RP_ERROR("can't get status of src %s: %m", state->path);
		return rc;
	}
	if (!S_ISREG(s.st_mode) && !S_ISDIR(s.st_mode)) {
		RP_ERROR("src isn't a regular file or a directory %s", state->path);
		return -EINVAL;
	}

	return 0;
}

static void check_one_target(void *closure, json_object *jso)
{
	install_state_t *state = closure;
	int rc = -EINVAL;
	json_object *content, *src, *type;

	if (!json_object_object_get_ex(jso, "content", &content))
		RP_ERROR("no content %s", json_object_get_string(jso));

	else if (!json_object_is_type(content, json_type_object))
		RP_ERROR("content isn't an object %s", json_object_get_string(content));

	else if (!json_object_object_get_ex(content, "src", &src))
		RP_ERROR("no content.src %s", json_object_get_string(content));

	else if (!json_object_is_type(src, json_type_string))
		RP_ERROR("no content.src isn't a string %s", json_object_get_string(src));

	else if (!json_object_object_get_ex(content, "type", &type))
		RP_ERROR("no content.type %s", json_object_get_string(content));

	else if (!json_object_is_type(type, json_type_string))
		RP_ERROR("no content.type isn't a string %s", json_object_get_string(type));

	else
		rc = check_one_content(json_object_get_string(src), json_object_get_string(type), state);

	if (rc < 0 && state->rc == 0)
		state->rc = rc;
}

static int check_contents(install_state_t *state)
{
	for_each_of(state->manifest, check_one_target, state, MANIFEST_TARGETS);
	return state->rc;
}












static void set_file_type(void *closure, json_object *jso)
{
	install_state_t *state = closure;
	path_entry_t *entry;
	int rc = 0;
	json_object *name, *value;
	const char *strval;
	path_type_t type;

	if (json_object_object_get_ex(jso, "name", &name)
	 && json_object_object_get_ex(jso, "value", &value)) {
		rc = path_entry_get(state->content, &entry, json_object_get_string(name));
		if (rc < 0) {
			RP_ERROR("file doesn't exist %s", json_object_get_string(jso));
			rc = -ENOENT;
		}
		else {
			type = get_entry_type(entry);
			if (type != path_type_Unknown) {
				RP_ERROR("file duplication %s", json_object_get_string(jso));
				rc = -EEXIST;
			}
			else {
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
	else {
		RP_ERROR("bad file properties item %s", json_object_get_string(jso));
		rc = -EINVAL;
	}
	if (rc < 0 && state->rc == 0)
		state->rc = rc;
}

static int fulfill_properties(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	install_state_t *state = closure;
	path_entry_t *parent;
	path_type_t curtype;

	curtype = get_entry_type(entry);
	if (curtype == path_type_Unknown) {
		curtype = path_type_of_entry(entry, state->content);
		if (curtype == path_type_Unknown)
			curtype = path_type_Id;
		set_entry_type(entry, curtype);
	}
	switch (curtype) {
	case path_type_Public:
	case path_type_Public_Exec:
	case path_type_Public_Lib:
		parent = path_entry_parent(entry);
		set_entry_type(parent, path_type_Public);
		break;
	default:
		break;
	}
	return 0;
}

static void set_target_file_properties(void *closure, json_object *jso)
{
	install_state_t *state = closure;
	json_object *content, *src, *type;
	path_entry_t *entry;

	if (json_object_object_get_ex(jso, "content", &content)
	 && json_object_object_get_ex(content, "src", &src)
	 && json_object_object_get_ex(content, "type", &type)) {
		if (path_entry_get(state->content, &entry, json_object_get_string(src)) < 0) {
			RP_ERROR("file doesn't exist %s", json_object_get_string(jso));
			if (state->rc == 0)
				state->rc = -ENOENT;
		}
		else if (get_entry_type(entry) == path_type_Unknown
		     && mime_type_is_executable(json_object_get_string(type)))
			set_entry_type(entry, path_type_Exec);
	}
}


static int reset_type_cb(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	install_state_t *state = closure;
	int rc = set_entry_type(entry, path_type_Unknown);
	if (rc < 0 && state->rc >= 0)
		state->rc = rc;
	return 0;
}

static int compute_files_properties(install_state_t *state)
{
	if (state->rc >= 0)
		for_each_content_entry(PATH_ENTRY_FORALL_NO_PATH, state, reset_type_cb);
	if (state->rc >= 0)
		for_each_of(state->manifest, set_file_type, state, MANIFEST_FILE_PROPERTIES);
	if (state->rc >= 0)
		for_each_of(state->manifest, set_target_file_properties, state, MANIFEST_TARGETS);
	if (state->rc >= 0)
		for_each_content_entry(PATH_ENTRY_FORALL_AFTER, state, fulfill_properties);
	return state->rc;
}


static int set_one_file_security(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	install_state_t *state = closure;
	const char *realpath = state->path;
	int rc;

	switch (get_entry_type(entry)) {
	case path_type_Unset:
		rc = 0;
		break;
	case path_type_Public:
	case path_type_Public_Exec:
	case path_type_Public_Lib:
		rc = secmgr_path_public(realpath);
		break;
	case path_type_Id:
		rc = secmgr_path_id(realpath);
		break;
	case path_type_Lib:
		rc = secmgr_path_lib(realpath);
		break;
	case path_type_Conf:
		rc = secmgr_path_conf(realpath);
		break;
	case path_type_Exec:
		rc = secmgr_path_exec(realpath);
		break;
	case path_type_Icon:
		rc = secmgr_path_icon(realpath);
		break;
	case path_type_Data:
		rc = secmgr_path_data(realpath);
		break;
	case path_type_Http:
		rc = secmgr_path_http(realpath);
		break;
	default:
		RP_WARNING("unknow path : %s", realpath);
//		rc = -EINVAL;
		break;
	}
	if (rc < 0 && state->rc >= 0)
		state->rc = rc;
	return 0;
}


static int setup_security(install_state_t *state)
{
	unsigned i, n;
	const char *perm;
	int rc;

	rc = secmgr_begin(state->apkg->package);
	if (rc < 0) {
		RP_ERROR("can't initialize security-manager context");
		goto end;
	}

	/* setup file security */
	for_each_content_entry(PATH_ENTRY_FORALL_AFTER, state, set_one_file_security);
	rc = state->rc;
	if (rc < 0)
		goto end;

	/* setup  permissions */
	permset_select_first(state->permset, permset_Select_Requested_And_Granted);
	perm = permset_current(state->permset);
	while(perm) {
		rc = secmgr_permit(perm);
		RP_INFO("permitting %s %s", perm, rc ? "FAILED!" : "success");
		if (rc)
			goto cancel;
		permset_select_next(state->permset, permset_Select_Requested_And_Granted);
		perm = permset_current(state->permset);
	}

	/* install default permissions */
	n = (unsigned)(sizeof default_permissions / sizeof *default_permissions);
	for (i = 0 ; i < n ; i++) {
		perm = default_permissions[i];
		rc = secmgr_permit(perm);
		RP_INFO("permitting %s %s", perm, rc ? "FAILED!" : "success");
		if (rc < 0)
			goto cancel;
	}

	rc = secmgr_install();

cancel:
	secmgr_end();

end:
	return rc;
}

static int setdown_security(
		const afmpkg_t *apkg,
		json_object *manifest
) {
	int rc = secmgr_begin(apkg->package);
	if (rc < 0)
		RP_ERROR("can't init sec lsm manager context");
	else {
		rc = secmgr_uninstall();
		secmgr_end();
		if (rc < 0)
			RP_ERROR("can't uninstall sec lsm manager context");
	}
	return rc;
}

static int make_file_executable(const char *filename)
{
	int rc = chmod(filename, 0755);
	if (rc < 0) {
		RP_ERROR("can't make file executable %s", filename);
		rc = -errno;
	}
	return rc;
}

static int set_one_file_properties(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	install_state_t *state = closure;
	int rc;
	switch (get_entry_type(entry)) {
	case path_type_Public_Exec:
	case path_type_Exec:
		rc = make_file_executable(state->path);
		if (rc < 0 && state->rc == 0)
			state->rc = rc;
		break;
	default:
		break;
	}
	return 0;
}

static int set_files_properties(install_state_t *state)
{
	for_each_content_entry(0, state, set_one_file_properties);
	return state->rc;
}

static int setup_units(
		const afmpkg_t *apkg,
		json_object *manifest
) {
	struct unitconf uconf;
	int rc = rp_jsonc_pack(&uconf.metadata, "{ss ss sb ss* ss*}",
				"install-dir", "/",
				"icons-dir", FWK_ICON_DIR,
				"redpak", apkg->redpakid != NULL,
				"redpak-id", apkg->redpakid,
				"root-dir", apkg->root);
	if (rc != 0)
		rc = -ENOMEM;
	else {
		uconf.new_afid = get_new_afid;
		uconf.base_http_ports = HTTP_PORT_BASE;
		rc = unit_afmpkg_install(manifest, &uconf);
		json_object_put(uconf.metadata);
	}
	return rc;
}

static int setdown_units(
		const afmpkg_t *apkg,
		json_object *manifest
) {
	struct unitconf uconf;
	uconf.metadata = NULL;
	uconf.new_afid = get_new_afid;
	uconf.base_http_ports = HTTP_PORT_BASE;
	return unit_afmpkg_uninstall(manifest, &uconf);
}

static
int
install_afmpkg(
	const afmpkg_t *apkg,
	char *path,
	unsigned offset_root,
	unsigned offset_pack
) {
	int rc;
	install_state_t state;

	state.rc = 0;
	state.apkg = apkg;
	state.manifest = NULL;
	state.permset = NULL;
	state.content = NULL;
	state.offset_root = offset_root;
	state.offset_pack = offset_pack;
	state.path = path;

	RP_NOTICE("-- Install afm pkg %s from manifest %s --", apkg->package, path);

	/* TODO: processing of signatures */

	/* read the manifest */
	rc = manifest_read_and_check(&state.manifest, path);
	if (rc)
		goto error2;

	/* creates the permission set */
	rc = permset_create(&state.permset);
	if (rc < 0) {
		RP_ERROR("can't create permset");
		goto error3;
	}

	/* creates the path entries */
	state.content = apkg->files;

	/* check permissions */
	rc = check_permissions(&state);
	if (rc < 0) {
		RP_ERROR("can't validate permission %s", apkg->package);
		goto error4;
	}

	/* check content */
	rc = check_contents(&state);
	if (rc < 0) {
		RP_ERROR("can't validate package content %s", apkg->package);
		goto error4;
	}

	/* install files and security */
	rc = compute_files_properties(&state);
	if (rc < 0) {
		RP_ERROR("failed to setup afm pkg %s", apkg->package);
		goto error4;
	}

	rc = setup_security(&state);
	if (rc < 0) {
		RP_ERROR("failed to setup afm pkg %s", apkg->package);
		goto error4;
	}

	rc = set_files_properties(&state);
	if (rc < 0) {
		RP_ERROR("failed to setup afm pkg %s", apkg->package);
		goto error4;
	}

	/* generate and install units */
	rc = setup_units(apkg, state.manifest);
error4:
	permset_destroy(state.permset);
error3:
	json_object_put(state.manifest);
error2:
	return rc;
}

static
int
uninstall_afmpkg(
	const afmpkg_t *apkg,
	char *path,
	unsigned offset_root,
	unsigned offset_pack
) {
	int rc;
	json_object *manifest;

	RP_NOTICE("-- Uninstall afm pkg %s from manifest %s --", apkg->package, path);

	/* TODO: process signatures */
	/* read the manifest */
	rc = manifest_read_and_check(&manifest, path);
	if (rc)
		goto error2;

	/* removed installed units */
	rc = setdown_units(apkg, manifest);
	if (rc < 0) {
		RP_ERROR("can't set units down for %s", apkg->package);
		goto error3;
	}

	/* uninstall security */
	rc = setdown_security(apkg, manifest);
	if (rc < 0) {
		RP_ERROR("can't set security down for %s", apkg->package);
		goto error3;
	}

error3:
	json_object_put(manifest);
error2:
	return rc;
}

struct detect
{
	const char *packname;
	size_t packlen;
	size_t baselen;
	const char *path;
	unsigned offset;
};

static
int detect_cb(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	struct detect *det = closure;
	return detect_packtype(det->packname, det->packlen, det->path, length + det->offset, &det->baselen);
}

static
int prepare_and_detect(
	const afmpkg_t *apkg,
	char path[PATH_MAX],
	unsigned *offset_root,
	unsigned *offset_pack
) {
	struct detect det;

	/* prepare */
	if (apkg->root == NULL)
		*offset_root = 0;
	else {
		size_t length = strlen(apkg->root);
		if (length >= PATH_MAX) {
			RP_ERROR("name too long %.200s...", apkg->root);
			return -ENAMETOOLONG;
		}
		memcpy(path, apkg->root, length + 1);
		if (length > 0 && path[length - 1] == '/')
			path[--length] = 0;
		*offset_root = (unsigned)length;
	}

	/* detection of the type of the installed files */
	det.packname = apkg->package;
	det.packlen = det.packname == NULL ? 0 : strlen(det.packname);
	det.path = path;
	det.offset = *offset_root;
	return path_entry_for_each_in_buffer(
			PATH_ENTRY_FORALL_ONLY_ADDED | PATH_ENTRY_FORALL_SILENT_ROOT,
			apkg->files,
			detect_cb,
			&det,
			&path[det.offset],
			PATH_MAX - det.offset);
}

static int install_widget_legacy(char *path, unsigned offset_pack);
static int uninstall_widget_legacy(char *path, unsigned offset_pack);

/* install afm package */
int afmpkg_install(
	const afmpkg_t *apkg
) {
	int rc;
	char path[PATH_MAX];
	unsigned offset_root, offset_pack;

	rc = prepare_and_detect(apkg, path, &offset_root, &offset_pack);
	if (rc >= 0)
		switch ((packtype_t)rc) {
		case packtype_Widget:
			rc = install_widget_legacy(path, offset_pack);
			break;
		
		case packtype_AfmPkg:
			rc = install_afmpkg(apkg, path, offset_root, offset_pack);
			break;

		default:		
			RP_ERROR("Unknown type of package %s", apkg->package ? apkg->package : "?unknown?");
			rc = -EINVAL;
		}
	return rc;
}

/* install afm package */
int afmpkg_uninstall(
	const afmpkg_t *apkg
) {
	int rc;
	char path[PATH_MAX];
	unsigned offset_root, offset_pack;

	rc = prepare_and_detect(apkg, path, &offset_root, &offset_pack);
	if (rc >= 0)
		switch ((packtype_t)rc) {
		case packtype_Widget:
			rc = uninstall_widget_legacy(path, offset_pack);
			break;
		
		case packtype_AfmPkg:
			rc = uninstall_afmpkg(apkg, path, offset_root, offset_pack);
			break;

		default:		
			RP_ERROR("Unknown type of package %s", apkg->package ? apkg->package : "?unknown?");
			rc = -EINVAL;
		}
	return rc;
}

#include "wgt-info.h"
#include "wgtpkg-install.h"
#include "wgtpkg-uninstall.h"

static
int
install_widget_legacy(
	char *path,
	unsigned offset_pack
) {
	struct wgt_info *ifo;
	int rc;

	path[offset_pack] = 0;
	RP_NOTICE("-- Install legacy widget from %s --", path);

	ifo = install_redpesk(path);
	if (!ifo) {
		RP_ERROR("Fail to install %s", path);
		rc = -errno;
	}
	else {
		wgt_info_unref(ifo);
		sighup_all();
		rc = 0;
	}
	return rc;
}

static
int
uninstall_widget_legacy(
	char *path,
	unsigned offset_pack
) {
	int rc;

	path[offset_pack] = 0;
	RP_NOTICE("-- Uninstall legacy widget from %s --", path);
	rc = uninstall_redpesk(path);
	if (rc < 0)
		RP_ERROR("Failed to uninstall %s", path);
	else
		sighup_all();
	return rc;
}

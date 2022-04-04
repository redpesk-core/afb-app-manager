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
#include "wgt.h"
#include "wgt-info.h"
#include "wgt-strings.h"
#include "wgtpkg-files.h"
#include "wgtpkg-workdir.h"
#if WITH_WIDGETS
# include "wgtpkg-zip.h"
#endif
#include "wgtpkg-permissions.h"
#include "wgtpkg-digsig.h"
#include "wgtpkg-install.h"
#include "wgtpkg-uninstall.h"
#include "secmgr-wrap.h"
#include "utils-dir.h"
#include "wgtpkg-unit.h"
#include "utils-systemd.h"
#include "utils-file.h"
#include "normalize-unit-file.h"
#include "manage-afid.h"

static const char* exec_type_strings[] = {
	"application/x-executable",
	"application/vnd.agl.native"
};

#define HTTP_PORT_BASE		30000

static const char *default_permissions[] = {
	"urn:AGL:token:valid"
};

static int check_defined(const void *data, const char *name)
{
	if (data)
		return 0;
	RP_ERROR("widget has no defined '%s' (temporary constraints)", name);
	errno = EINVAL;
	return -1;
}

static int check_valid_string(const char *value, const char *name)
{
	int pos;
	char c;

	if (check_defined(value, name))
		return -1;
	pos = 0;
	c = value[pos];
	if (c == 0) {
		RP_ERROR("empty string forbidden in '%s' (temporary constraints)", name);
		errno = EINVAL;
		return -1;
	}
	do {
		if (!isalnum(c) && !strchr(".-_", c)) {
			RP_ERROR("forbidden char %c in '%s' -> '%s' (temporary constraints)", c, name, value);
			errno = EINVAL;
			return -1;
		}
		c = value[++pos];
	} while(c);
	return 0;
}

static int check_temporary_constraints(const struct wgt_desc *desc)
{
	int result;

	result  = check_valid_string(desc->id, "id");
	result |= check_valid_string(desc->version, "version");
	result |= check_valid_string(desc->ver, "ver");
	result |= check_defined(desc->content_src, "content");
	if (desc->icons)
		result |= check_defined(desc->icons->src, "icon.src");
	if (result)
		return result;

	if (desc->icons && desc->icons->next) {
		RP_ERROR("widget has more than one icon defined (temporary constraints)");
		errno = EINVAL;
		result = -1;
	}
	return 0;
}

static int set_required_permissions(struct wgt_desc_param *params, int required)
{
	int optional;

	while (params) {
		/* check if target */
		if (!strcmp(params->name, string_sharp_target)) {
			/* do nothing when #target */
		} else {
			/* check the value */
			if (!strcmp(params->value, string_required))
				optional = !required;
			else if (!strcmp(params->value, string_optional))
				optional = 1;
			else {
				RP_ERROR("unexpected parameter value: %s found for %s", params->value, params->name);
				errno = EPERM;
				return -1;
			}
			/* set the permission */
			if (request_permission(params->name)) {
				RP_DEBUG("granted permission: %s", params->name);
			} else if (optional) {
				RP_INFO("optional permission ungranted: %s", params->name);
			} else {
				RP_ERROR("ungranted permission required: %s", params->name);
				errno = EPERM;
				return -1;
			}
		}
		params = params->next;
	}
	return 0;
}

static int check_permissions(const struct wgt_desc *desc)
{
	int result;
	const struct wgt_desc_feature *feature;

	result = 0;
	feature = desc->features;
	while(result >= 0 && feature) {
		if (!strcmp(feature->name, feature_required_permission))
			result = set_required_permissions(feature->params, feature->required);
		feature = feature->next;
	}
	return result;
}

static int for_all_content(const struct wgt_desc *desc, int (*action)(const char *src, const char *type))
{
	int rc, rc2;
	struct wgt_desc_feature *feat;
	const char *src, *type;

	rc = action(desc->content_src, desc->content_type);
	feat = desc->features;
	while (feat) {
		if (!strcmp(feat->name, FWK_PREFIX"widget:provided-unit")) {
			src = wgt_info_param(feat, "content.src");
			type = wgt_info_param(feat, "content.type");
			rc2 = action(src, type);
			if (rc >= 0 && rc2 < 0)
				rc = rc2;
		}
		feat = feat->next;
	}
	return rc;
}

static int set_exec_flag(const char *src, const char *type)
{
	int i, rc;

	if (src && type) {
		i = sizeof exec_type_strings / sizeof *exec_type_strings;
		while (i) {
			if (!strcasecmp(type, exec_type_strings[--i])) {
				rc = fchmodat(workdirfd, src, 0755, 0);
				if (rc < 0)
					RP_ERROR("can't make executable the file %s", src);
				return rc;
			}
		}
	}
	return 0;
}

static int check_one_content(const char *src, const char *type)
{
	int rc;
	struct stat s;
	int fhtdocs, serr;

	if (!src) {
		RP_ERROR("a content src is missing");
		errno = EINVAL;
		rc = -1;
	} else {
		/* TODO: when dealing with HTML and languages, the check should
		 * include i18n path search of widgets */
		rc = fstatat(workdirfd, src, &s, AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
		if (rc < 0) {
			serr = errno;
			fhtdocs = openat(workdirfd, "htdocs", O_DIRECTORY|O_PATH);
			if (fhtdocs >= 0) {
				rc = fstatat(fhtdocs, src, &s, AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
				serr = errno;
				close(fhtdocs);
			}
			errno = serr;
		}
		if (rc < 0)
			RP_ERROR("can't get info on content %s: %m", src);
		else if (!S_ISREG(s.st_mode)) {
			RP_ERROR("content %s isn't a regular file", src);
			errno = EINVAL;
			rc = -1;
		}
	}
	return rc;
}

static int check_content(const struct wgt_desc *desc)
{
	return for_all_content(desc, check_one_content);
}

static int check_widget(const struct wgt_desc *desc)
{
	int result;

	result = check_temporary_constraints(desc);
	if (result >= 0)
		result = check_permissions(desc);
	if (result >= 0)
		result = check_content(desc);
	return result;
}

static int get_target_directory(char target[PATH_MAX], const char *root, const struct wgt_desc *desc)
{
	int rc;

#if DISTINCT_VERSIONS
	rc = snprintf(target, PATH_MAX, "%s/%s/%s", root, desc->id, desc->ver);
#else
	rc = snprintf(target, PATH_MAX, "%s/%s", root, desc->id);
#endif
	if (rc < PATH_MAX)
		rc = 0;
	else {
		RP_ERROR("path too long");
		errno = EINVAL;
		rc = -1;
	}
	return rc;
}

static int move_widget_to(const char *destdir, int force)
{
	return move_workdir(destdir, 1, force);
}

static int install_icon(const struct wgt_desc *desc)
{
	char link[PATH_MAX];
	char target[PATH_MAX];
	int rc;

	if (!desc->icons)
		return 0;

	create_directory(FWK_ICON_DIR, 0755, 1);
	rc = snprintf(link, sizeof link, "%s/%s", FWK_ICON_DIR, desc->idaver);
	if (rc >= (int)sizeof link) {
		RP_ERROR("link too long in install_icon");
		errno = EINVAL;
		return -1;
	}

	rc = snprintf(target, sizeof target, "%s/%s", workdir, desc->icons->src);
	if (rc >= (int)sizeof target) {
		RP_ERROR("target too long in install_icon");
		errno = EINVAL;
		return -1;
	}

	unlink(link);
	rc = symlink(target, link);
	if (rc)
		RP_ERROR("can't create link %s -> %s", link, target);
	return rc;
}

static int install_exec_flag(const struct wgt_desc *desc)
{
	return for_all_content(desc, set_exec_flag);
}

static int install_file_properties(const struct wgt_desc *desc)
{
	int rc, rc2;
	struct wgt_desc_feature *feat;
	struct wgt_desc_param *param;

	rc = 0;
	feat = desc->features;
	while (feat) {
		if (!strcmp(feat->name, FWK_PREFIX"widget:file-properties")) {
			param = feat->params;
			while (param) {
				if (!strcmp(param->value, "executable")) {
					rc2 = fchmodat(workdirfd, param->name, 0755, 0);
					if (rc2 < 0)
						RP_ERROR("can't make executable the file %s: %m", param->name);
				} else {
					RP_ERROR("unknown file property %s for %s", param->value, param->name);
					errno = EINVAL;
					rc2 = -1;
				}
				if (rc2 < 0 && !rc)
					rc = rc2;
				param = param->next;
			}
		}
		feat = feat->next;
	}
	return rc;
}

enum path_type {
    type_none,
    type_conf,
    type_data,
    type_exec,
    type_http,
    type_icon,
    type_id,
    type_lib,
    type_public,
    number_path_type
};

struct pathent {
		struct pathent *next;
		unsigned int len;
		enum path_type pathtype;
		char name[];
};


static int search_param(const struct wgt_desc_param *param, const char *path)
{
	while(param != NULL) {
		if (!strcmp(param->value, path))
		{
			return 1;
		}
		param = param->next;
	}
	return 0;
}

static int strpos(const char *string, const char *search)
{
	char *p = strstr(string, search);
	if(!p) return -1;
	return (int) (p - string);
}

static int set_pathtype(const char *path, const struct wgt_desc *desc, enum path_type *pathtype)
{
	const struct wgt_desc_icon *icon;
	const struct wgt_desc_feature *feat;
	const struct wgt_desc_param *param;
	size_t len;
	*pathtype = type_none;

	/* icons are public */
	icon = desc->icons;
	while (icon != NULL) {
		len = strlen(icon->src);
		if (!memcmp(path, icon->src, len) && (path[len] == 0 || path[len] == '/'))
		{
			*pathtype = type_icon;
			return 0;
		}
		icon = icon->next;
	}

	// browse through features
	feat = desc->features;
	while (feat != NULL) {
		param = feat->params;

		if (!strcasecmp(feat->name, "urn:AGL:widget:provided-binding") /* provided bindings are public */
			|| !strcasecmp(feat->name, "urn:AGL:widget:public-files")) {
			*pathtype = search_param(param, path) ? type_public : type_none;
		} else if (strcasecmp(feat->name, "urn:AGL:widget:lib-files")) {
			*pathtype = search_param(param, path) ? type_lib : type_none;
		} else if (strcasecmp(feat->name, "urn:AGL:widget:conf-files")) {
			*pathtype = search_param(param, path) ? type_conf: type_none;
		} else if (strcasecmp(feat->name, "urn:AGL:widget:exec-files")) {
			*pathtype = search_param(param, path) ? type_exec : type_none;
		} else if (strcasecmp(feat->name, "urn:AGL:widget:data-files")) {
			*pathtype = search_param(param, path) ? type_data: type_none;
		} else if (strcasecmp(feat->name, "urn:AGL:widget:http-files")) {
			*pathtype = search_param(param, path) ? type_http : type_none;
		}

		if(*pathtype != type_none)
		{
			return 0;
		}

		feat = feat->next;
	}

	if(!strpos(path, "bin")) {
		*pathtype = type_exec;
	} else if(!strpos(path, "etc")) {
		*pathtype = type_conf;
	} else if(!strpos(path, "conf")) {
		*pathtype = type_conf;
	} else if(!strpos(path, "lib")) {
		*pathtype = type_lib;
	} else if(!strpos(path, "var")) {
		*pathtype = type_data;
	} else if(!strpos(path, "htdocs")) {
		*pathtype = type_http;
	} else if(!strpos(path, "public")) {
		*pathtype = type_public;
	}

	if(*pathtype != type_none)
	{
		return 0;
	}

	return -1;
}

static int install_security(const struct wgt_desc *desc)
{
	char path[PATH_MAX], *head;
	const char *perm;
	int rc;
	enum path_type pathtype = type_none;
	unsigned int i, n, len, lf, j;
	struct filedesc *f;
	struct pathent *pe0, *pe2, *ppe;

	pe0 = NULL;
	rc = secmgr_init(desc->id);
	if (rc)
	{
		rc = -1;
		goto end;
	}

	/* instal the files */
	head = stpcpy(path, workdir);
	assert(head < path + sizeof path);
	len = (unsigned)((path + sizeof path) - head);
	if (!len) {
		RP_ERROR("root path too long in install_security");
		errno = ENAMETOOLONG;
		goto error;
	}
	len--;
	*head++ = '/';

	/* build root entry */
	pe0 = malloc(1 + sizeof *pe0);
	if (pe0 == NULL)
		goto error;
	pe0->next = NULL;
	pe0->len = 0;
	pe0->pathtype = type_id;
	pe0->name[0] = 0;

	/* build list of entries */
	n = file_count();
	for (i = 0 ; i < n ; i++) {
		f = file_of_index(i);
		rc = set_pathtype(f->name, desc, &pathtype);

		if(rc < 0) {
			RP_ERROR("invalid pathtype for %s", f->name);
			goto error;
		}

		// if one soon path is public root path is public
		if(pathtype == type_public)
			pe0->pathtype = type_public;

		lf = j = 0;
		while(f->name[j] == '/')
			j++;
		while (f->name[j] != 0) {
			/* copy next entry of the path */
			while(f->name[j] && f->name[j] != '/') {
				if (lf + 1 >= len) {
					RP_ERROR("path too long in install_security");
					errno = ENAMETOOLONG;
					goto error;
				}
				head[lf++] = f->name[j++];
			}
			head[lf] = 0;

			/* search if it already exists */
			ppe = pe0;
			pe2 = pe0->next;
			while (pe2 != NULL && pe2->len < lf) {
				ppe = pe2;
				pe2 = pe2->next;
			}
			while (pe2 != NULL && pe2->len == lf && strcmp(head, pe2->name)) {
				ppe = pe2;
				pe2 = pe2->next;
			}

			if (pe2 != NULL && pe2->len == lf) {
				/* existing, update pathtype */
				if(pe2->pathtype != type_public)
					pe2->pathtype = pathtype;
			}
			else {
				/* not existing, create it */
				pe2 = malloc(lf + 1 + sizeof *pe2);
				if (pe2 == NULL)
					goto error;
				pe2->next = ppe->next;
				pe2->len = lf;
				pe2->pathtype = pathtype;
				memcpy(pe2->name, head, 1 + lf);
				ppe->next = pe2;
			}

			/* prepare next path entry */
			head[lf++] = '/';	
			while(f->name[j] == '/')
				j++;
		}
	}

	/* set the path entries */
	for (pe2 = pe0 ; pe2 != NULL ; pe2 = pe2->next) {
		strcpy(head, pe2->name);

		switch (pe2->pathtype)
		{
		case type_public:
			rc = secmgr_path_public(path);
			break;
		case type_id:
			rc = secmgr_path_id(path);
			break;
		case type_lib:
			rc = secmgr_path_lib(path);
			break;
		case type_conf:
			rc = secmgr_path_conf(path);
			break;
		case type_exec:
			rc = secmgr_path_exec(path);
			break;
		case type_icon:
			rc = secmgr_path_icon(path);
			break;
		case type_data:
			rc = secmgr_path_data(path);
			break;
		case type_http:
			rc = secmgr_path_http(path);
			break;
		default:
			RP_ERROR("unknow path : %s", path);
			rc = -1;
			break;
		}

		if (rc)
			goto error;
	}

	/* install the permissions */
	perm = first_usable_permission();
	while(perm) {
		rc = secmgr_permit(perm);
		RP_INFO("permitting %s %s", perm, rc ? "FAILED!" : "success");
		if (rc)
			goto error;
		perm = next_usable_permission();
	}

	/* install default permissions */
	n = (unsigned int)(sizeof default_permissions / sizeof *default_permissions);
	for (i = 0 ; i < n ; i++) {
		perm = default_permissions[i];
		rc = secmgr_permit(perm);
		RP_INFO("permitting %s %s", perm, rc ? "FAILED!" : "success");
		if (rc)
			goto error;
	}

	rc = secmgr_install();
	goto end2;

error:
	rc = -1;
end2:
	secmgr_cancel();
end:
	/* free memory of path entries */
	while (pe0 != NULL) {
		ppe = pe0;
		pe0 = pe0->next;
		free(ppe);
	}
	return rc;
}

#if defined(ALLOW_NO_SIGNATURE)
# define DEFAULT_ALLOW_NO_SIGNATURE 1
#else
# define DEFAULT_ALLOW_NO_SIGNATURE 0
#endif

static int info_desc_check(struct wgt_info **pifo, const struct wgt_desc **pdesc, int allow_no_signature)
{
	int rc;
	struct wgt_info *ifo;
	const struct wgt_desc *desc;

	/* info and check */
	rc = check_all_signatures(allow_no_signature);
	if (rc)
		goto error;

	ifo = wgt_info_createat(workdirfd, NULL, 1, 1, 1);
	if (!ifo)
		goto error;

	reset_requested_permissions();
	desc = wgt_info_desc(ifo);
	if (check_widget(desc))
		goto error2;

	*pifo = ifo;
	*pdesc = desc;
	return 0;

error2:
	wgt_info_unref(ifo);
error:
	*pifo = NULL;
	*pdesc = NULL;
	return -1;
}

static int setup_files_and_security(const struct wgt_desc *desc)
{
	int rc;

	/* apply properties */
	rc = install_icon(desc);
	if (rc == 0)
		rc = install_security(desc);
	if (rc == 0)
		rc = install_exec_flag(desc);
	if (rc == 0)
		rc = install_file_properties(desc);

	return rc;
}

static int setup_units(struct wgt_info *ifo, const char *installdir)
{
	struct unitconf uconf;

	/* generate and install units */
	uconf.installdir = installdir;
	uconf.icondir = FWK_ICON_DIR;
	uconf.new_afid = get_new_afid;
	uconf.base_http_ports = HTTP_PORT_BASE;
	return unit_install(ifo, &uconf);
}

#if WITH_WIDGETS
/* install the widget of the file */
struct wgt_info *install_widget(const char *wgtfile, const char *root, int force)
{
	struct wgt_info *ifo;
	const struct wgt_desc *desc;
	char installdir[PATH_MAX];
	int err, rc;

	RP_NOTICE("-- INSTALLING widget %s to %s --", wgtfile, root);

	/* extraction */
	create_directory(root, 0755, 1);
	if (make_workdir(root, "TMP", 0)) {
		RP_ERROR("failed to create a working directory");
		goto error1;
	}

	if (zread(wgtfile, 0))
		goto error2;


	/* info and check */
	rc = info_desc_check(&ifo, &desc, DEFAULT_ALLOW_NO_SIGNATURE);
	if (rc)
		goto error2;

	/* uninstall previous and move */
	if (get_target_directory(installdir, root, desc))
		goto error3;

	if (access(installdir, F_OK) == 0) {
		if (!force) {
			RP_ERROR("widget already installed");
			errno = EEXIST;
			goto error3;
		}
		if (uninstall_widget(desc->idaver, root))
			goto error3;
	}

	if (move_widget_to(installdir, force))
		goto error3;

	/* install files and security */
	rc = setup_files_and_security(desc);
	if (rc)
		goto error3;

	/* generate and install units */
	rc = setup_units(ifo, installdir);
	if (rc)
		goto error3;

	file_reset();
	return ifo;

error3:
	wgt_info_unref(ifo);

error2:
	err = errno;
	remove_workdir();
	errno = err;

error1:
	file_reset();
	return NULL;
}
#endif

/* install redpesk from installdir widget directory */
struct wgt_info *install_redpesk(const char *installdir)
{
	struct wgt_info *ifo;
	const struct wgt_desc *desc;
	int rc;

	RP_NOTICE("-- Install redpesk widget from %s --", installdir);

	/* prepare workdir */
	set_workdir(installdir, 0);
	fill_files();

	/* info and check */
	rc = info_desc_check(&ifo, &desc, DEFAULT_ALLOW_NO_SIGNATURE);
	if (rc)
		goto error2;

	/* install files and security */
	rc = setup_files_and_security(desc);
	if (rc)
		goto error3;


	/* generate and install units */
	rc = setup_units(ifo, installdir);
	if (rc)
		goto error3;

	file_reset();
	return ifo;

error3:
	wgt_info_unref(ifo);
error2:
	file_reset();
	return NULL;
}

/*
 Copyright (C) 2015-2018 IoT.bzh

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

#include "verbose.h"
#include "wgt.h"
#include "wgt-info.h"
#include "wgt-strings.h"
#include "wgtpkg-files.h"
#include "wgtpkg-workdir.h"
#include "wgtpkg-zip.h"
#include "wgtpkg-permissions.h"
#include "wgtpkg-digsig.h"
#include "wgtpkg-install.h"
#include "secmgr-wrap.h"
#include "utils-dir.h"
#include "wgtpkg-unit.h"
#include "utils-systemd.h"
#include "utils-file.h"

static const char* exec_type_strings[] = {
	"application/x-executable",
	"application/vnd.agl.native"
};

static const char key_afm_prefix[] = "X-AFM-";
static const char key_http_port[] = "http-port";

/*
 * normalize unit files: remove comments, remove heading blanks,
 * make single lines
 */
static void normalize_unit_file(char *content)
{
	char *read, *write, c;

	read = write = content;
	c = *read++;
	while (c) {
		switch (c) {
		case '\n':
		case ' ':
		case '\t':
			c = *read++;
			break;
		case '#':
		case ';':
			do { c = *read++; } while(c && c != '\n');
			break;
		default:
			*write++ = c;
			do { *write++ = c = *read++; } while(c && c != '\n');
			if (write - content >= 2 && write[-2] == '\\')
				(--write)[-1] = ' ';
			break;
		}
	}
	*write = c;
}

static int get_port_cb(void *closure, const char *name, const char *path, int isuser)
{
	char *iter;
	char *content;
	size_t length;
	int rc, p;

	/* reads the file */
	rc = getfile(path, &content, &length);
	if (rc < 0)
		return rc;

	/* normalize the unit file */
	normalize_unit_file(content);

	/* process the file */
	iter = strstr(content, key_afm_prefix);
	while (iter) {
		iter += sizeof key_afm_prefix - 1;
		if (*iter == '-')
			iter++;
		if (!strncmp(iter, key_http_port, sizeof key_http_port - 1)) {
			iter += sizeof key_http_port - 1;
			while(*iter && *iter != '=' && *iter != '\n')
				iter++;
			if (*iter == '=') {
				while(*++iter == ' ');
				p = atoi(iter);
				if (p >= 0 && p < 32768)
					((uint32_t*)closure)[p >> 5] |= (uint32_t)1 << (p & 31);
			}
		}
		iter = strstr(iter, key_afm_prefix);
	}
	free(content);
	return 0;
}

static int get_port()
{
	int rc;
	uint32_t ports[1024]; /* 1024 * 32 = 32768 */

	memset(ports, 0, sizeof ports);
	rc = systemd_unit_list(0, get_port_cb, &ports);
	if (rc >= 0) {
		rc = systemd_unit_list(1, get_port_cb, ports);
		if (rc >= 0) {
			for (rc = 1024 ; rc < 32768 && !~ports[rc >> 5] ; rc += 32);
			if (rc == 32768) {
				ERROR("Can't compute a valid port");
				errno = EADDRNOTAVAIL;
				rc = -1;
			} else {
				while (1 & (ports[rc >> 5] >> (rc & 31))) rc++;
			}
		}
	}
	return rc;
}

static int check_defined(const void *data, const char *name)
{
	if (data)
		return 0;
	ERROR("widget has no defined '%s' (temporary constraints)", name);
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
		ERROR("empty string forbidden in '%s' (temporary constraints)", name);
		errno = EINVAL;
		return -1;			
	}
	do {
		if (!isalnum(c) && !strchr(".-_", c)) {
			ERROR("forbidden char %c in '%s' -> '%s' (temporary constraints)", c, name, value);
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
		ERROR("widget has more than one icon defined (temporary constraints)");
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
				ERROR("unexpected parameter value: %s found for %s", params->value, params->name);
				errno = EPERM;
				return -1;
			}
			/* set the permission */
			if (request_permission(params->name)) {
				DEBUG("granted permission: %s", params->name);
			} else if (optional) {
				INFO("optional permission ungranted: %s", params->name);
			} else {
				ERROR("ungranted permission required: %s", params->name);
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
		if (!strcmp(feat->name, "urn:AGL:widget:provided-unit")) {
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
					ERROR("can't make executable the file %s", src);
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
		ERROR("a content src is missing");
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
			ERROR("can't get info on content %s: %m", src);
		else if (!S_ISREG(s.st_mode)) {
			ERROR("content %s isn't a regular file", src);
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

	rc = snprintf(target, PATH_MAX, "%s/%s/%s", root, desc->id, desc->ver);
	if (rc < PATH_MAX)
		rc = 0;
	else {
		ERROR("path too long");
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
		ERROR("link too long in install_icon");
		errno = EINVAL;
		return -1;
	}

	rc = snprintf(target, sizeof target, "%s/%s", workdir, desc->icons->src);
	if (rc >= (int)sizeof target) {
		ERROR("target too long in install_icon");
		errno = EINVAL;
		return -1;
	}

	unlink(link);
	rc = symlink(target, link);
	if (rc)
		ERROR("can't create link %s -> %s", link, target);
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
		if (!strcmp(feat->name, "urn:AGL:widget:file-properties")) {
			param = feat->params;
			while (param) {
				if (!strcmp(param->value, "executable")) {
					rc2 = fchmodat(workdirfd, param->name, 0755, 0);
					if (rc2 < 0)
						ERROR("can't make executable the file %s: %m", param->name);
				} else {
					ERROR("unknown file property %s for %s", param->value, param->name);
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

static int install_security(const struct wgt_desc *desc)
{
	char path[PATH_MAX], *head;
	const char *icon, *perm;
	int rc;
	unsigned int i, n, len, lic, lf;
	struct filedesc *f;

	rc = secmgr_init(desc->id);
	if (rc)
		goto error;

	rc = secmgr_path_public_read_only(workdir);
	if (rc)
		goto error2;

	/* instal the files */
	head = stpcpy(path, workdir);
	assert(head < path + sizeof path);
	len = (unsigned)((path + sizeof path) - head);
	if (!len) {
		ERROR("root path too long in install_security");
		errno = ENAMETOOLONG;
		goto error2;
	}
	len--;
	*head++ = '/';
	icon = desc->icons ? desc->icons->src : NULL;
	lic = (unsigned)strlen(icon);
	n = file_count();
	i = 0;
	while(i < n) {
		f = file_of_index(i++);
		lf = (unsigned)strlen(f->name);
		if (lf >= len) {
			ERROR("path too long in install_security");
			errno = ENAMETOOLONG;
			goto error2;
		}
		strcpy(head, f->name);
		if (lf <= lic && icon && !memcmp(f->name, icon, lf) && (!f->name[lf] || f->name[lf] == '/'))
			rc = secmgr_path_public_read_only(path);
		else
			rc = secmgr_path_read_only(path);
		if (rc)
			goto error2;
	}

	/* install the permissions */
	perm = first_usable_permission();
	while(perm) {
		rc = secmgr_permit(perm);
		INFO("permitting %s %s", perm, rc ? "FAILED!" : "success");
		if (rc)
			goto error2;
		perm = next_usable_permission();
	}

	rc = secmgr_install();
	return rc;
error2:
	secmgr_cancel();
error:
	return -1;
}

/* install the widget of the file */
struct wgt_info *install_widget(const char *wgtfile, const char *root, int force)
{
	struct wgt_info *ifo;
	const struct wgt_desc *desc;
	char installdir[PATH_MAX];
	int port;
	struct unitconf uconf;

	NOTICE("-- INSTALLING widget %s to %s --", wgtfile, root);

	/* workdir */
	create_directory(root, 0755, 1);
	if (make_workdir(root, "TMP", 0)) {
		ERROR("failed to create a working directory");
		goto error1;
	}

	if (zread(wgtfile, 0))
		goto error2;

	if (check_all_signatures())
		goto error2;

	ifo = wgt_info_createat(workdirfd, NULL, 1, 1, 1);
	if (!ifo)
		goto error2;

	reset_requested_permissions();
	desc = wgt_info_desc(ifo);
	if (check_widget(desc))
		goto error3;

	if (get_target_directory(installdir, root, desc))
		goto error3;

	if (move_widget_to(installdir, force))
		goto error3;

	if (install_icon(desc))
		goto error3;

	if (install_security(desc))
		goto error4;

	if (install_exec_flag(desc))
		goto error4;

	if (install_file_properties(desc))
		goto error4;

	port = get_port();
	if (port < 0)
		goto error4;

	uconf.installdir = installdir;
	uconf.icondir = FWK_ICON_DIR;
	uconf.port = port;
	if (unit_install(ifo, &uconf))
		goto error4;

	file_reset();
	return ifo;

error4:
	/* TODO: cleanup */

error3:
	wgt_info_unref(ifo);

error2:
	remove_workdir();

error1:
	file_reset();
	return NULL;
}


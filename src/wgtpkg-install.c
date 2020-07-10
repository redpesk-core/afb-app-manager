/*
 Copyright (C) 2015-2020 IoT.bzh

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
#include "wgtpkg-uninstall.h"
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
static const char key_afid[] = "ID";

#define HTTP_PORT_BASE		30000

#define AFID_MIN		1
#define AFID_MAX		1999
#define AFID_IS_VALID(afid)	(AFID_MIN <= (afid) && (afid) <= AFID_MAX)
#define AFID_COUNT		(AFID_MAX - AFID_MIN + 1)
#define AFID_ACNT		((AFID_COUNT + 31) >> 5)
#define AFID_ASFT(afid)		(((afid) - AFID_MIN) & 31)
#define AFID_AIDX(afid)		(((afid) - AFID_MIN) >> 5)
#define AFID_TEST(array,afid)	((((array)[AFID_AIDX(afid)]) >> AFID_ASFT(afid)) & 1)
#define AFID_SET(array,afid)	(((array)[AFID_AIDX(afid)]) |= (((uint32_t)1) << AFID_ASFT(afid)))

static uint32_t *afids_array = NULL;

static const char *default_permissions[] = {
	"urn:AGL:token:valid"
};

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

static int get_afid_cb(void *closure, const char *name, const char *path, int isuser)
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
		if (!strncmp(iter, key_afid, sizeof key_afid - 1)) {
			iter += sizeof key_afid - 1;
			while(*iter && *iter != '=' && *iter != '\n')
				iter++;
			if (*iter == '=') {
				while(*++iter == ' ');
				p = atoi(iter);
				if (AFID_IS_VALID(p))
					AFID_SET((uint32_t*)closure, p);
			}
		}
		iter = strstr(iter, key_afm_prefix);
	}
	free(content);
	return 0;
}

static int update_afids(uint32_t *afids)
{
	int rc;

	memset(afids, 0, AFID_ACNT * sizeof(uint32_t));
	rc = systemd_unit_list(0, get_afid_cb, afids);
	if (rc >= 0)
		rc = systemd_unit_list(1, get_afid_cb, afids);
	if (rc < 0)
		ERROR("troubles while updating afids");
	return rc;
}

static int first_free_afid(uint32_t *afids)
{
	int afid;

	afid = AFID_MIN;
	while (afid <= AFID_MAX && !~afids[AFID_AIDX(afid)])
		afid += 32;
	while (afid <= AFID_MAX && AFID_TEST(afids, afid))
		afid++;
	if (afid > AFID_MAX) {
		ERROR("Can't compute a valid afid");
		errno = EADDRNOTAVAIL;
		afid = -1;
	}
	return afid;
}

static int get_new_afid()
{
	int afid;

	/* ensure existing afid bitmap */
	if (afids_array == NULL) {
		afids_array = malloc(AFID_ACNT * sizeof(uint32_t));
		if (afids_array == NULL || update_afids(afids_array) < 0)
			return -1;
	}

	/* allocates the afid */
	afid = first_free_afid(afids_array);
	if (afid < 0 && errno == EADDRNOTAVAIL) {
		/* no more ids, try to rescan */
		memset(afids_array, 0, AFID_ACNT * sizeof(uint32_t));
		if (update_afids(afids_array) >= 0)
			afid = first_free_afid(afids_array);
	}
	if (afid >= 0)
		AFID_SET(afids_array, afid);

	return afid;
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

#if DISTINCT_VERSIONS
	rc = snprintf(target, PATH_MAX, "%s/%s/%s", root, desc->id, desc->ver);
#else
	rc = snprintf(target, PATH_MAX, "%s/%s", root, desc->id);
#endif
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
		if (!strcmp(feat->name, FWK_PREFIX"widget:file-properties")) {
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

static int is_path_public(const char *path, const struct wgt_desc *desc)
{
	const struct wgt_desc_icon *icon;
	const struct wgt_desc_feature *feat;
	const struct wgt_desc_param *param;
	size_t len;

	/* icons are public */
	icon = desc->icons;
	while (icon != NULL) {
		len = strlen(icon->src);
		if (!memcmp(path, icon->src, len) && (path[len] == 0 || path[len] == '/'))
			return 1;
		icon = icon->next;
	}

	/* provided bindings are public */
	feat = desc->features;
	while (feat != NULL) {
		if (strcasecmp(feat->name, "urn:AGL:widget:provided-binding") == 0
		 || strcasecmp(feat->name, "urn:AGL:widget:public-files") == 0) {
			param = feat->params;
			while(param != NULL) {
				if (strcmp(param->value, path) == 0)
					return 1;
				param = param->next;
			}
		}
		feat = feat->next;
	}

	/* otherwise no */
	return 0;
}

static int install_security(const struct wgt_desc *desc)
{
	char path[PATH_MAX], *head;
	const char *perm;
	int rc, public;
	unsigned int i, n, len, lf, j;
	struct filedesc *f;
	struct pathent {
		struct pathent *next;
		unsigned int len;
		int public;
		char name[];
	} *pe0, *pe2, *ppe;

	pe0 = NULL;
	rc = secmgr_init(desc->id);
	if (rc)
		goto error;

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

	/* build root entry */
	pe0 = malloc(1 + sizeof *pe0);
	if (pe0 == NULL)
		goto error2;
	pe0->next = NULL;
	pe0->len = 0;
	pe0->public = 0;
	pe0->name[0] = 0;

	/* build list of entries */
	n = file_count();
	for (i = 0 ; i < n ; i++) {
		f = file_of_index(i);
		public = is_path_public(f->name, desc);
		pe0->public |= public;
		lf = j = 0;
		while(f->name[j] == '/')
			j++;
		while (f->name[j] != 0) {
			/* copy next entry of the path */
			while(f->name[j] && f->name[j] != '/') {
				if (lf + 1 >= len) {
					ERROR("path too long in install_security");
					errno = ENAMETOOLONG;
					goto error2;
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

			if (pe2 != NULL && pe2->len == lf)
				/* existing, update public status */
				pe2->public |= public;
			else {
				/* not existing, create it */
				pe2 = malloc(lf + 1 + sizeof *pe2);
				if (pe2 == NULL)
					goto error2;
				pe2->next = ppe->next;
				pe2->len = lf;
				pe2->public = public;
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
		if (pe2->public)
			rc = secmgr_path_public_read_only(path);
		else
			rc = secmgr_path_private(path);
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

	/* install default permissions */
	n = (unsigned int)(sizeof default_permissions / sizeof *default_permissions);
	for (i = 0 ; i < n ; i++) {
		perm = default_permissions[i];
		rc = secmgr_permit(perm);
		INFO("permitting %s %s", perm, rc ? "FAILED!" : "success");
		if (rc)
			goto error2;
	}

	rc = secmgr_install();
	goto end;
error2:
	secmgr_cancel();
error:
	rc = -1;
end:
	/* free memory of path entries */
	while (pe0 != NULL) {
		ppe = pe0;
		pe0 = pe0->next;
		free(ppe);
	}
	return rc;
}

/* install the widget of the file */
struct wgt_info *install_widget(const char *wgtfile, const char *root, int force)
{
	struct wgt_info *ifo;
	const struct wgt_desc *desc;
	char installdir[PATH_MAX];
	int err, rc;
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

#if defined(ALLOW_NO_SIGNATURE)
	rc = check_all_signatures(1);
#else
	rc = check_all_signatures(0);
#endif
	if (rc)
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

	if (access(installdir, F_OK) == 0) {
		if (!force) {
			ERROR("widget already installed");
			errno = EEXIST;
			goto error3;
		}
		if (uninstall_widget(desc->idaver, root))
			goto error3;
	}

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

	uconf.installdir = installdir;
	uconf.icondir = FWK_ICON_DIR;
	uconf.new_afid = get_new_afid;
	uconf.base_http_ports = HTTP_PORT_BASE;
	if (unit_install(ifo, &uconf))
		goto error4;

	file_reset();
	return ifo;

error4:
	/* TODO: cleanup */

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


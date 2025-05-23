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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include <libxml/tree.h>

#include <rp-utils/rp-verbose.h>
#include "wgt.h"
#include "wgt-config.h"
#include "wgt-strings.h"
#include "wgt-info.h"

struct wgt_info {
	int refcount;
	struct wgt *wgt;
	struct wgt_desc desc;
};

static int getpropbool(xmlNodePtr node, const char *prop, int def)
{
	int result;
	char *val = (char*)xmlGetProp(node, (const xmlChar*)prop);
	if (!val)
		result = def;
	else {
		if (!strcmp(val, "true"))
			result = 1;
		else if (!strcmp(val, "false"))
			result = 0;
		else
			result = def;
		xmlFree(val);
	}
	return result;
}

static int getpropnum(xmlNodePtr node, const char *prop, int def)
{
	int result;
	char *val = (char*)xmlGetProp(node, (const xmlChar*)prop);
	if (!val)
		result = def;
	else {
		result = atoi(val);
		xmlFree(val);
	}
	return result;
}

static xmlChar *optprop(xmlNodePtr node, const char *prop)
{
	return node ? xmlGetProp(node, (const xmlChar*)prop) : NULL;
}

static xmlChar *optcontent(xmlNodePtr node)
{
	return node ? xmlNodeGetContent(node) : NULL;
}

static char *mkver(char *version)
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

static void make_lowercase(char *s)
{
	if (s) {
		while(*s) {
			*s = (char)tolower(*s);
			s++;
		}
	}
}

static void dash_to_underscore(char *s)
{
	if (s) {
		while(*s) {
			if(*s == '-') {
				*s = '_';
			}
			s++;
		}
	}
}

static int fill_desc(struct wgt_desc *desc, int want_icons, int want_features, int want_preferences)
{
	xmlNodePtr node, pnode;
	struct wgt_desc_icon *icon, **icontail;
	struct wgt_desc_feature *feature, **featuretail;
	struct wgt_desc_preference *preference, **preferencetail;
	struct wgt_desc_param *param, **paramtail;

	node = wgt_config_widget();
	if (!node) {
		RP_WARNING("no widget");
		errno = EINVAL;
		return -1;
	}

	desc->id = (char*)xmlGetProp(node, (const xmlChar*)string_id);
	if (desc->id == NULL) {
		RP_WARNING("no id");
		errno = EINVAL;
		return -1;
	}
	desc->id_lower = strdup(desc->id);
	make_lowercase(desc->id_lower);
	desc->id_underscore = strdup(desc->id_lower);
	dash_to_underscore(desc->id_underscore);

	desc->version = (char*)xmlGetProp(node, (const xmlChar*)string_version);
	if (desc->version == NULL) {
		RP_WARNING("no version");
		errno = EINVAL;
		return -1;
	}
	desc->ver = mkver(desc->version);
	make_lowercase(desc->ver);
	desc->idaver = mkidaver(desc->id_lower, desc->ver);
	desc->width = getpropnum(node, string_width, 0);
	desc->height = getpropnum(node, string_height, 0);
	desc->viewmodes = (char*)xmlGetProp(node, (const xmlChar*)string_viewmodes);
	desc->defaultlocale = (char*)xmlGetProp(node, (const xmlChar*)string_defaultlocale);

	node = wgt_config_name();
	desc->name = (char*)optcontent(node);
	desc->name_short = (char*)optprop(node, string_short);

	node = wgt_config_description();
	desc->description = (char*)optcontent(node);

	node = wgt_config_author();
	desc->author = (char*)optcontent(node);
	desc->author_href = (char*)optprop(node, string_href);
	desc->author_email = (char*)optprop(node, string_email);

	node = wgt_config_license();
	desc->license = (char*)optcontent(node);
	desc->license_href = (char*)optprop(node, string_href);

	node = wgt_config_content();
	desc->content_src = (char*)optprop(node, string_src);
	if (node && desc->content_src == NULL) {
		RP_WARNING("content without src");
		errno = EINVAL;
		return -1;
	}
	desc->content_type = (char*)optprop(node, string_type);
	desc->content_encoding = (char*)optprop(node, string_encoding);

	if (want_icons) {
		icontail = &desc->icons;
		node = wgt_config_first_icon();
		while (node) {
			icon = malloc(sizeof * icon);
			if (icon == NULL) {
				errno = ENOMEM;
				return -1;
			}
			icon->src = (char*)xmlGetProp(node, (const xmlChar*)string_src);
			icon->width = getpropnum(node, string_width, 0);
			icon->height = getpropnum(node, string_height, 0);

			icon->next = NULL;
			*icontail = icon;

			if (icon->src == NULL) {
				RP_WARNING("icon without src");
				errno = EINVAL;
				return -1;
			}
			icontail = &icon->next;
			node = wgt_config_next_icon(node);
		}
	}

	if (want_features) {
		featuretail = &desc->features;
		node = wgt_config_first_feature();
		while (node) {
			feature = malloc(sizeof * feature);
			if (feature == NULL) {
				errno = ENOMEM;
				return -1;
			}
			feature->name = (char*)xmlGetProp(node, (const xmlChar*)string_name);
			feature->required = getpropbool(node, string_required, 1);
			feature->params = NULL;

			feature->next = NULL;
			*featuretail = feature;

			if (feature->name == NULL) {
				RP_WARNING("feature without name");
				errno = EINVAL;
				return -1;
			}

			paramtail = &feature->params;
			pnode = wgt_config_first_param(node);
			while (pnode) {
				param = malloc(sizeof * param);
				if (param == NULL) {
					errno = ENOMEM;
					return -1;
				}
				param->name = (char*)xmlGetProp(pnode, (const xmlChar*)string_name);
				param->value = (char*)xmlGetProp(pnode, (const xmlChar*)string_value);

				param->next = NULL;
				*paramtail = param;

				if (param->name == NULL || param->value == NULL) {
					RP_WARNING("param without name or value");
					errno = EINVAL;
					return -1;
				}

				paramtail = &param->next;
				pnode = wgt_config_next_param(pnode);
			}

			featuretail = &feature->next;
			node = wgt_config_next_feature(node);
		}
	}

	if (want_preferences) {
		preferencetail = &desc->preferences;
		node = wgt_config_first_preference();
		while (node) {
			preference = malloc(sizeof * preference);
			if (preference == NULL) {
				errno = ENOMEM;
				return -1;
			}
			preference->name = (char*)xmlGetProp(node, (const xmlChar*)string_name);
			preference->value = (char*)xmlGetProp(node, (const xmlChar*)string_value);
			preference->readonly = getpropbool(node, string_readonly, 0);

			*preferencetail = preference;
			preference->next = NULL;

			if (preference->name == NULL) {
				RP_WARNING("preference without name");
				errno = EINVAL;
				return -1;
			}

			preferencetail = &preference->next;
			node = wgt_config_next_preference(node);
		}
	}
	return 0;
}

static void free_desc(struct wgt_desc *desc)
{
	struct wgt_desc_icon *icon;
	struct wgt_desc_feature *feature;
	struct wgt_desc_preference *preference;
	struct wgt_desc_param *param;

	xmlFree(desc->id);
	free(desc->id_lower);
	free(desc->id_underscore);
	xmlFree(desc->version);
	free(desc->ver);
	free(desc->idaver);
	xmlFree(desc->viewmodes);
	xmlFree(desc->defaultlocale);
	xmlFree(desc->name);
	xmlFree(desc->name_short);
	xmlFree(desc->description);
	xmlFree(desc->author);
	xmlFree(desc->author_href);
	xmlFree(desc->author_email);
	xmlFree(desc->license);
	xmlFree(desc->license_href);
	xmlFree(desc->content_src);
	xmlFree(desc->content_type);
	xmlFree(desc->content_encoding);

	while(desc->icons) {
		icon = desc->icons;
		desc->icons = icon->next;
		xmlFree(icon->src);
		free(icon);
	}

	while(desc->features) {
		feature = desc->features;
		desc->features = feature->next;
		xmlFree(feature->name);
		while(feature->params) {
			param = feature->params;
			feature->params = param->next;
			xmlFree(param->name);
			xmlFree(param->value);
			free(param);
		}
		free(feature);
	}

	while(desc->preferences) {
		preference = desc->preferences;
		desc->preferences = preference->next;
		xmlFree(preference->name);
		xmlFree(preference->value);
		free(preference);
	}
}

static void dump_desc(struct wgt_desc *desc, FILE *f, const char *prefix)
{
	struct wgt_desc_icon *icon;
	struct wgt_desc_feature *feature;
	struct wgt_desc_preference *preference;
	struct wgt_desc_param *param;

	if (desc->id) fprintf(f, "%sid: %s\n", prefix, desc->id);
	if (desc->id_lower) fprintf(f, "%sid-lower: %s\n", prefix, desc->id_lower);
	if (desc->id_underscore) fprintf(f, "%sid-underscore: %s\n", prefix, desc->id_underscore);
	if (desc->version) fprintf(f, "%sversion: %s\n", prefix, desc->version);
	if (desc->ver) fprintf(f, "%sver: %s\n", prefix, desc->ver);
	if (desc->idaver) fprintf(f, "%sidaver: %s\n", prefix, desc->idaver);
	if (desc->width) fprintf(f, "%swidth: %d\n", prefix, desc->width);
	if (desc->height) fprintf(f, "%sheight: %d\n", prefix, desc->height);
	if (desc->viewmodes) fprintf(f, "%sviewmodes: %s\n", prefix, desc->viewmodes);
	if (desc->defaultlocale) fprintf(f, "%sdefaultlocale: %s\n", prefix, desc->defaultlocale);
	if (desc->name) fprintf(f, "%sname: %s\n", prefix, desc->name);
	if (desc->name_short) fprintf(f, "%sname_short: %s\n", prefix, desc->name_short);
	if (desc->description) fprintf(f, "%sdescription: %s\n", prefix, desc->description);
	if (desc->author) fprintf(f, "%sauthor: %s\n", prefix, desc->author);
	if (desc->author_href) fprintf(f, "%sauthor_href: %s\n", prefix, desc->author_href);
	if (desc->author_email) fprintf(f, "%sauthor_email: %s\n", prefix, desc->author_email);
	if (desc->license) fprintf(f, "%slicense: %s\n", prefix, desc->license);
	if (desc->license_href) fprintf(f, "%slicense_href: %s\n", prefix, desc->license_href);
	if (desc->content_src) fprintf(f, "%scontent_src: %s\n", prefix, desc->content_src);
	if (desc->content_type) fprintf(f, "%scontent_type: %s\n", prefix, desc->content_type);
	if (desc->content_encoding) fprintf(f, "%scontent_encoding: %s\n", prefix, desc->content_encoding);

	icon = desc->icons;
	while(icon) {
		fprintf(f, "%s+ icon src: %s\n", prefix, icon->src);
		if (icon->width) fprintf(f, "%s       width: %d\n", prefix, icon->width);
		if (icon->height) fprintf(f, "%s       height: %d\n", prefix, icon->height);
		icon = icon->next;
	}

	feature = desc->features;
	while(feature) {
		fprintf(f, "%s+ feature name: %s\n", prefix, feature->name);
		fprintf(f, "%s          required: %s\n", prefix, feature->required ? "true" : "false");
		param = feature->params;
		while(param) {
			fprintf(f, "%s          + param name: %s\n", prefix, param->name);
			fprintf(f, "%s                  value: %s\n", prefix, param->value);
			param = param->next;
		}
		feature = feature->next;
	}

	preference = desc->preferences;
	while(preference) {
		fprintf(f, "%s+ preference name: %s\n", prefix, preference->name);
		if (preference->value) fprintf(f, "%s             value: %s\n", prefix, preference->value);
		fprintf(f, "%s             readonly: %s\n", prefix, preference->readonly ? "true" : "false");
		preference = preference->next;
	}
}

/* create info for current config and then closes the config */
static int info_of_config(struct wgt_info **result, int icons, int features, int preferences)
{
	int rc;
	struct wgt_info *info;

	info = calloc(1, sizeof * info);
	if (info == NULL) {
		rc = -1;
		errno = ENOMEM;
	}
	else {
		info->refcount = 1;
		info->wgt = NULL;
		rc = fill_desc(&info->desc, icons, features, preferences);
		if (rc) {
			wgt_info_unref(info);
			info = NULL;
		}
	}
	*result = info;
	wgt_config_close();
	return rc;
}

struct wgt_info *wgt_info_create(struct wgt *wgt, int icons, int features, int preferences)
{
	int rc;
	struct wgt_info *result;

	assert(wgt);
	assert(wgt_is_connected(wgt));
	rc = wgt_config_open(wgt);
	if (rc) {
		errno = EINVAL;
		result = NULL;
	}
	else {
		rc = info_of_config(&result, icons, features, preferences);
		if (!rc) {
			result->wgt = wgt;
			wgt_addref(wgt);
		}
	}
	return result;
}

struct wgt_info *wgt_info_createat(int dirfd, const char *pathname, int icons, int features, int preferences)
{
	struct wgt_info *result = NULL;
	struct wgt *wgt = wgt_createat(dirfd, pathname);
	if (wgt) {
		result = wgt_info_create(wgt, icons, features, preferences);
		wgt_unref(wgt);
	}
	return result;
}

struct wgt_info *wgt_info_from_config(int dirfd, const char *path, int icons, int features, int preferences)
{
	struct wgt_info *info = NULL;
	int rc = wgt_config_open_fileat(dirfd, path);
	if (!rc)
		rc = info_of_config(&info, icons, features, preferences);
	return info;
}

const struct wgt_desc *wgt_info_desc(struct wgt_info *ifo)
{
	assert(ifo);
	return &ifo->desc;
}

struct wgt *wgt_info_wgt(struct wgt_info *ifo)
{
	assert(ifo);
	return ifo->wgt;
}

void wgt_info_addref(struct wgt_info *ifo)
{
	assert(ifo);
	assert(ifo->refcount > 0);
	ifo->refcount++;
}

void wgt_info_unref(struct wgt_info *ifo)
{
	assert(ifo);
	assert(ifo->refcount > 0);
	if (--ifo->refcount)
		return;

	free_desc(&ifo->desc);
	if (ifo->wgt != NULL)
		wgt_unref(ifo->wgt);
	free(ifo);
}

void wgt_info_dump(struct wgt_info *ifo, int fd, const char *prefix)
{
	FILE *f;

	assert(ifo);
	f = fdopen(fd, "w");
	if (f == NULL)
		RP_WARNING("can't fdopen in wgt_info_dump");
	else {
		dump_desc(&ifo->desc, f, prefix);
		fclose(f);
	}
}

const struct wgt_desc_feature *wgt_info_feature(struct wgt_info *ifo, const char *name)
{
	const struct wgt_desc_feature *result = ifo->desc.features;
	while(result && strcmp(result->name, name))
		result = result->next;
	return result;
}

const struct wgt_desc_param *wgt_info_desc_param(const struct wgt_desc_feature *feature, const char *name)
{
	const struct wgt_desc_param *param = feature->params;
	while(param != NULL && strcmp(name, param->name) != 0)
		param = param->next;
	return param;
}

const char *wgt_info_param(const struct wgt_desc_feature *feature, const char *name)
{
	const struct wgt_desc_param *param = wgt_info_desc_param(feature, name);
	return param == NULL ? NULL : param->value;
}


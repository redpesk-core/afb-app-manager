/*
 Copyright 2015 IoT.bzh

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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <syslog.h>
#include <libxml/tree.h>

#include "verbose.h"
#include "wgt.h"
#include "wgt-config.h"
#include "wgt-info.h"

static int getpropbool(xmlNodePtr node, const char *prop, int def)
{
	int result;
	char *val = xmlGetProp(node, prop);
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
	char *val = xmlGetProp(node, prop);
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
	return node ? xmlGetProp(node, prop) : NULL;
}

static xmlChar *optcontent(xmlNodePtr node)
{
	return node ? xmlNodeGetContent(node) : NULL;
}

static int fill_info(struct wgt_info *ifo, int want_icons, int want_features, int want_preferences)
{
	xmlNodePtr node, pnode;
	struct wgt_info_icon *icon, **icontail;
	struct wgt_info_feature *feature, **featuretail;
	struct wgt_info_preference *preference, **preferencetail;
	struct wgt_info_param *param, **paramtail;

	node = wgt_config_widget();
	if (!node) {
		warning("no widget");
		errno = EINVAL;
		return -1;
	}
	ifo->id = xmlGetProp(node, wgt_config_string_id);
	ifo->version = xmlGetProp(node, wgt_config_string_version);
	ifo->width = getpropnum(node, wgt_config_string_width, 0);
	ifo->height = getpropnum(node, wgt_config_string_height, 0);
	ifo->viewmodes = xmlGetProp(node, wgt_config_string_viewmodes);
	ifo->defaultlocale = xmlGetProp(node, wgt_config_string_defaultlocale);

	node = wgt_config_name();
	ifo->name = optcontent(node);
	ifo->name_short = optprop(node, wgt_config_string_short);

	node = wgt_config_description();
	ifo->description = optcontent(node);

	node = wgt_config_author();
	ifo->author = optcontent(node);
	ifo->author_href = optprop(node, wgt_config_string_href);
	ifo->author_email = optprop(node, wgt_config_string_email);

	node = wgt_config_license();
	ifo->license = optcontent(node);
	ifo->license_href = optprop(node, wgt_config_string_href);
	
	node = wgt_config_content();
	ifo->content_src = optprop(node, wgt_config_string_src);
	if (node && ifo->content_src == NULL) {
		warning("content without src");
		errno = EINVAL;
		return -1;
	}
	ifo->content_type = optprop(node, wgt_config_string_type);
	ifo->content_encoding = optprop(node, wgt_config_string_encoding);

	if (want_icons) {
		icontail = &ifo->icons;
		node = wgt_config_first_icon();
		while (node) {
			icon = malloc(sizeof * icon);
			if (icon == NULL) {
				errno = ENOMEM;
				return -1;
			}
			icon->src = xmlGetProp(node, wgt_config_string_src);
			icon->width = getpropnum(node, wgt_config_string_width, 0);
			icon->height = getpropnum(node, wgt_config_string_height, 0);

			icon->next = NULL;
			*icontail = icon;

			if (icon->src == NULL) {
				warning("icon without src");
				errno = EINVAL;
				return -1;
			}
			icontail = &icon->next;
			node = wgt_config_next_icon(node);
		}
	}

	if (want_features) {
		featuretail = &ifo->features;
		node = wgt_config_first_feature();
		while (node) {
			feature = malloc(sizeof * feature);
			if (feature == NULL) {
				errno = ENOMEM;
				return -1;
			}
			feature->name = xmlGetProp(node, wgt_config_string_name);
			feature->required = getpropbool(node, wgt_config_string_required, 1);

			feature->next = NULL;
			*featuretail = feature;

			if (feature->name == NULL) {
				warning("feature without name");
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
				param->name = xmlGetProp(pnode, wgt_config_string_name);
				param->value = xmlGetProp(pnode, wgt_config_string_value);

				param->next = NULL;
				*paramtail = param;

				if (param->name == NULL || param->value == NULL) {
					warning("param without name or value");
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
		preferencetail = &ifo->preferences;
		node = wgt_config_first_preference();
		while (node) {
			preference = malloc(sizeof * preference);
			if (preference == NULL) {
				errno = ENOMEM;
				return -1;
			}
			preference->name = xmlGetProp(node, wgt_config_string_name);
			preference->value = xmlGetProp(node, wgt_config_string_value);
			preference->readonly = getpropbool(node, wgt_config_string_readonly, 0);

			*preferencetail = preference;
			preference->next = NULL;

			if (preference->name == NULL) {
				warning("preference without name");
				errno = EINVAL;
				return -1;
			}

			preferencetail = &preference->next;
			node = wgt_config_next_preference(node);
		}
	}
	return 0;
}

struct wgt_info *wgt_info_get(struct wgt *wgt, int icons, int features, int preferences)
{
	int rc;
	struct wgt_info *result;

	assert(wgt);
	assert(wgt_is_connected(wgt));
	rc = wgt_config_open(wgt);
	if (rc) {
		errno = EINVAL;
		return NULL;
	}

	result = calloc(sizeof * result, 1);
	if (!result) {
		wgt_config_close();
		errno = ENOMEM;
		return NULL;
	}
	result->refcount = 1;

	rc = fill_info(result, icons, features, preferences);
	wgt_config_close();
	if (rc) {
		wgt_info_unref(result);
		return NULL;
	}
	return result;
}

void wgt_info_addref(struct wgt_info *ifo)
{
	assert(ifo);
	assert(ifo->refcount > 0);
	ifo->refcount++;
}

void wgt_info_unref(struct wgt_info *ifo)
{
	struct wgt_info_icon *icon;
	struct wgt_info_feature *feature;
	struct wgt_info_preference *preference;
	struct wgt_info_param *param;

	assert(ifo);
	assert(ifo->refcount > 0);
	if (--ifo->refcount)
		return;

	xmlFree(ifo->id);
	xmlFree(ifo->version);
	xmlFree(ifo->viewmodes);
	xmlFree(ifo->defaultlocale);
	xmlFree(ifo->name);
	xmlFree(ifo->name_short);
	xmlFree(ifo->description);
	xmlFree(ifo->author);
	xmlFree(ifo->author_href);
	xmlFree(ifo->author_email);
	xmlFree(ifo->license);
	xmlFree(ifo->license_href);
	xmlFree(ifo->content_src);
	xmlFree(ifo->content_type);
	xmlFree(ifo->content_encoding);

	while(ifo->icons) {
		icon = ifo->icons;
		ifo->icons = icon->next;
		xmlFree(icon->src);
		free(icon);
	}

	while(ifo->features) {
		feature = ifo->features;
		ifo->features = feature->next;
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

	while(ifo->preferences) {
		preference = ifo->preferences;
		ifo->preferences = preference->next;
		xmlFree(preference->name);
		xmlFree(preference->value);
		free(preference);
	}
	free(ifo);
}

void wgt_info_dump(struct wgt_info *ifo, int fd, const char *prefix)
{
	FILE *f;
	struct wgt_info_icon *icon;
	struct wgt_info_feature *feature;
	struct wgt_info_preference *preference;
	struct wgt_info_param *param;

	assert(ifo);
	f = fdopen(fd, "w");
	if (f == NULL) {
		warning("can't fdopen in wgt_info_dump");
		return;
	}
	
	if (ifo->id) fprintf(f, "%sid: %s\n", prefix, ifo->id);
	if (ifo->width) fprintf(f, "%swidth: %d\n", prefix, ifo->width);
	if (ifo->height) fprintf(f, "%sheight: %d\n", prefix, ifo->height);
	if (ifo->version) fprintf(f, "%sversion: %s\n", prefix, ifo->version);
	if (ifo->viewmodes) fprintf(f, "%sviewmodes: %s\n", prefix, ifo->viewmodes);
	if (ifo->defaultlocale) fprintf(f, "%sdefaultlocale: %s\n", prefix, ifo->defaultlocale);
	if (ifo->name) fprintf(f, "%sname: %s\n", prefix, ifo->name);
	if (ifo->name_short) fprintf(f, "%sname_short: %s\n", prefix, ifo->name_short);
	if (ifo->description) fprintf(f, "%sdescription: %s\n", prefix, ifo->description);
	if (ifo->author) fprintf(f, "%sauthor: %s\n", prefix, ifo->author);
	if (ifo->author_href) fprintf(f, "%sauthor_href: %s\n", prefix, ifo->author_href);
	if (ifo->author_email) fprintf(f, "%sauthor_email: %s\n", prefix, ifo->author_email);
	if (ifo->license) fprintf(f, "%slicense: %s\n", prefix, ifo->license);
	if (ifo->license_href) fprintf(f, "%slicense_href: %s\n", prefix, ifo->license_href);
	if (ifo->content_src) fprintf(f, "%scontent_src: %s\n", prefix, ifo->content_src);
	if (ifo->content_type) fprintf(f, "%scontent_type: %s\n", prefix, ifo->content_type);
	if (ifo->content_encoding) fprintf(f, "%scontent_encoding: %s\n", prefix, ifo->content_encoding);

	icon = ifo->icons;
	while(icon) {
		fprintf(f, "%s+ icon src: %s\n", prefix, icon->src);
		if (icon->width) fprintf(f, "%s       width: %d\n", prefix, icon->width);
		if (icon->height) fprintf(f, "%s       height: %d\n", prefix, icon->height);
		icon = icon->next;
	}

	feature = ifo->features;
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

	preference = ifo->preferences;
	while(preference) {
		fprintf(f, "%s+ preference name: %s\n", prefix, preference->name);
		if (preference->value) fprintf(f, "%s             value: %s\n", prefix, preference->value);
		fprintf(f, "%s             readonly: %s\n", prefix, preference->readonly ? "true" : "false");
		preference = preference->next;
	}

	fclose(f);
}


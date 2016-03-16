/*
 Copyright 2015 IoT.bzh

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

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>

#include "verbose.h"
#include "wgt.h"
#include "wgt-config.h"

const char wgt_config_string_author[] = "author";
const char wgt_config_string_content[] = "content";
const char wgt_config_string_defaultlocale[] = "defaultlocale";
const char wgt_config_string_description[] = "description";
const char wgt_config_string_email[] = "email";
const char wgt_config_string_encoding[] = "encoding";
const char wgt_config_string_feature[] = "feature";
const char wgt_config_string_height[] = "height";
const char wgt_config_string_href[] = "href";
const char wgt_config_string_icon[] = "icon";
const char wgt_config_string_id[] = "id";
const char wgt_config_string_license[] = "license";
const char wgt_config_string_name[] = "name";
const char wgt_config_string_param[] = "param";
const char wgt_config_string_preference[] = "preference";
const char wgt_config_string_readonly[] = "readonly";
const char wgt_config_string_required[] = "required";
const char wgt_config_string_short[] = "short";
const char wgt_config_string_src[] = "src";
const char wgt_config_string_type[] = "type";
const char wgt_config_string_value[] = "value";
const char wgt_config_string_version[] = "version";
const char wgt_config_string_viewmodes[] = "viewmodes";
const char wgt_config_string_widget[] = "widget";
const char wgt_config_string_width[] = "width";
const char wgt_config_string_xml_file[] = "config.xml";

static struct wgt *configwgt = NULL;
static xmlDocPtr configxml = NULL;

static xmlNodePtr next(xmlNodePtr node, const char *type)
{
	while (node && (node->type != XML_ELEMENT_NODE || strcmp(type, node->name)))
		node = node->next;
	return node;
}

static xmlNodePtr first(const char *type)
{
	assert(configxml);
	assert(xmlDocGetRootElement(configxml));
	return next(xmlDocGetRootElement(configxml)->children, type);
}

static unsigned int scorelang(xmlNodePtr node)
{
	char *lang = xmlNodeGetLang(node);
	unsigned int score = wgt_locales_score(configwgt, lang);
	xmlFree(lang);
	return score;
}

static xmlNodePtr element_based_localisation(const char *type)
{
	xmlNodePtr resu, elem;
	unsigned int sr, s;

	resu = first(type);
	if (resu) {
		sr = scorelang(resu);
		elem = next(resu->next, type);
		while (elem) {
			s = scorelang(elem);
			if (s < sr) {
				resu = elem;
				sr = s;
			}
			elem = next(elem->next, type);
		}
	}
	return resu;
}

void wgt_config_close()
{
	if (configxml) {
		xmlFreeDoc(configxml);
		configxml = NULL;
		configwgt = NULL;
	}
}

int wgt_config_open(struct wgt *wgt)
{
	int fd;
	assert(!configxml);
	fd = wgt_open_read(wgt, wgt_config_string_xml_file);
	if (fd < 0) {
		ERROR("can't open config file %s", wgt_config_string_xml_file);
		return fd;
	}
	configxml = xmlReadFd(fd, wgt_config_string_xml_file, NULL, 0);
	close(fd);
	if (configxml == NULL) {
		ERROR("xml parse of config file %s failed", wgt_config_string_xml_file);
		return -1;
	}
	assert(xmlDocGetRootElement(configxml));
	configwgt = wgt;
	return 0;
}

xmlNodePtr wgt_config_widget()
{
	xmlNodePtr root;
	assert(configxml);
	root = xmlDocGetRootElement(configxml);
	return strcmp(wgt_config_string_widget, root->name) ? NULL : root;
}

/* elements based on localisation */
xmlNodePtr wgt_config_name()
{
	assert(configxml);
	return element_based_localisation(wgt_config_string_name);
}

xmlNodePtr wgt_config_description()
{
	assert(configxml);
	return element_based_localisation(wgt_config_string_description);
}

xmlNodePtr wgt_config_license()
{
	assert(configxml);
	return element_based_localisation(wgt_config_string_license);
}

/* elements based on path localisation */
xmlNodePtr wgt_config_author()
{
	assert(configxml);
	return first(wgt_config_string_author);
}

xmlNodePtr wgt_config_content()
{
	assert(configxml);
	return first(wgt_config_string_content);
}

/* element multiple */

xmlNodePtr wgt_config_first_feature()
{
	assert(configxml);
	return first(wgt_config_string_feature);
}

xmlNodePtr wgt_config_next_feature(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, wgt_config_string_feature);
}

xmlNodePtr wgt_config_first_preference()
{
	assert(configxml);
	return first(wgt_config_string_preference);
}

xmlNodePtr wgt_config_next_preference(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, wgt_config_string_preference);
}

xmlNodePtr wgt_config_first_icon()
{
	assert(configxml);
	return first(wgt_config_string_icon);
}

xmlNodePtr wgt_config_next_icon(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, wgt_config_string_icon);
}

xmlNodePtr wgt_config_first_param(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->children, wgt_config_string_param);
}

xmlNodePtr wgt_config_next_param(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, wgt_config_string_param);
}

/* best sized icon */

static int score_dim(xmlNodePtr ref, xmlNodePtr x, const char *dim, int request)
{
	int r, iref, ix;
	char *sref, *sx;

	sref = xmlGetProp(ref, dim);
	if (sref) {
		iref = atoi(sref);
		xmlFree(sref);
		sx = xmlGetProp(x, dim);
		if (sx) {
			/* sref && sx */
			ix = atoi(sx);
			xmlFree(sx);
			if (ix >= request) {
				if (iref >= request)
					r = ix - iref;
				else
					r = request - ix;
			} else {
				if (iref >= request)
					r = iref - request;
				else
					r = iref - ix;
			}
		} else {
			/* sref && !sx */
			if (iref >= request)
				r = iref - request;
			else
				r = 0;
		}
	} else {
		sx = xmlGetProp(x, dim);
		if (sx) {
			/* !sref && sx */
			ix = atoi(sx);
			xmlFree(sx);
			if (ix >= request)
				r = request - ix;
			else
				r = 0;
		} else {
			/* !sref && !sx */
			r = 0;
		}
	}
	return r;
}

static int is_better_icon(xmlNodePtr ref, xmlNodePtr x, int width, int height)
{
	int sw = score_dim(ref, x, wgt_config_string_width, width);
	int sh = score_dim(ref, x, wgt_config_string_height, height);
	return sw+sh < 0;
}

xmlNodePtr wgt_config_icon(int width, int height)
{
	assert(configxml);
	xmlNodePtr resu, icon;

	resu = wgt_config_first_icon();
	icon = wgt_config_next_icon(resu);
	while (icon) {
		if (is_better_icon(resu, icon, width, height))
			resu = icon;
		icon = wgt_config_next_icon(icon);
	}
	return resu;
}


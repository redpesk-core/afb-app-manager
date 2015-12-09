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

#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <errno.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>

#include "wgt.h"
#include "wgt-config.h"

const char wgt_config_string_xml_file[] = "config.xml";
const char wgt_config_string_name[] = "name";
const char wgt_config_string_description[] = "description";
const char wgt_config_string_author[] = "author";
const char wgt_config_string_license[] = "license";
const char wgt_config_string_icon[] = "icon";
const char wgt_config_string_content[] = "content";
const char wgt_config_string_feature[] = "feature";
const char wgt_config_string_preference[] = "preference";
const char wgt_config_string_width[] = "width";
const char wgt_config_string_height[] = "height";


static struct wgt *configwgt = NULL;
static xmlDocPtr configxml = NULL;

static xmlNodePtr next(xmlNodePtr node, const char *type)
{
	while (node && node->type != XML_ELEMENT_NODE && strcmp(type, node->name))
		node = node->next;
	return node;
}

static xmlNodePtr first(const char *type)
{
	xmlNodePtr root;
	if (configxml) {
		root = xmlDocGetRootElement(configxml);
		if (root)
			return next(root->children, type);
	}
	return NULL;
}

static int scorelang(xmlNodePtr node)
{
	char *lang = xmlNodeGetLang(node);
	int score = wgt_locales_score(configwgt, lang);
	xmlFree(lang);
	return score;
}

static xmlNodePtr element_based_localisation(const char *type)
{
	xmlNodePtr resu, elem;
	int sr, s;

	resu = first(type);
	if (resu) {
		sr = scorelang(resu);
		elem = next(resu->next, type);
		while (resu) {
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
		syslog(LOG_ERR, "can't open config file %s", wgt_config_string_xml_file);
		return fd;
	}
	configxml = xmlReadFd(fd, wgt_config_string_xml_file, NULL, 0);
	close(fd);
	if (configxml == NULL) {
		syslog(LOG_ERR, "xml parse of config file %s failed", wgt_config_string_xml_file);
		return -1;
	}
	configwgt = wgt;
	return 0;
}

/* elements based on localisation */
xmlNodePtr wgt_config_name()
{
	return element_based_localisation(wgt_config_string_name);
}

xmlNodePtr wgt_config_description()
{
	return element_based_localisation(wgt_config_string_description);
}

xmlNodePtr wgt_config_license()
{
	return element_based_localisation(wgt_config_string_license);
}

/* elements based on path localisation */
xmlNodePtr wgt_config_author()
{
	return first(wgt_config_string_author);
}

xmlNodePtr wgt_config_content()
{
	return first(wgt_config_string_content);
}

/* element multiple */

xmlNodePtr wgt_config_first_feature()
{
	return first(wgt_config_string_feature);
}

xmlNodePtr wgt_config_next_feature(xmlNodePtr node)
{
	return next(node->next, wgt_config_string_feature);
}

xmlNodePtr wgt_config_first_preference()
{
	return first(wgt_config_string_preference);
}

xmlNodePtr wgt_config_next_preference(xmlNodePtr node)
{
	return next(node->next, wgt_config_string_preference);
}

xmlNodePtr wgt_config_first_icon()
{
	return first(wgt_config_string_icon);
}

xmlNodePtr wgt_config_next_icon(xmlNodePtr node)
{
	return next(node->next, wgt_config_string_icon);
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

static int better_icon(xmlNodePtr ref, xmlNodePtr x, int width, int height)
{
	int sw = score_dim(ref, x, wgt_config_string_width, width);
	int sh = score_dim(ref, x, wgt_config_string_height, height);
	return sw+sh < 0;
}

xmlNodePtr wgt_config_icon(int width, int height)
{
	xmlNodePtr resu, icon;

	resu = wgt_config_first_icon();
	icon = wgt_config_next_icon(resu);
	while (icon) {
		if (better_icon(resu, icon, width, height))
			resu = icon;
		icon = wgt_config_next_icon(icon);
	}
	return resu;
}


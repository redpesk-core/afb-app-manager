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
	int score = locales_score(lang);
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

void confixml_close()
{
	if (configxml) {
		xmlFreeDoc(configxml);
		configxml = NULL;
	}
}

int confixml_open()
{
	int fd;
	assert(!configxml);
	fd = widget_open_read(_config_xml_);
	if (fd < 0) {
		syslog(LOG_ERR, "can't open config file %s", _config_xml_);
		return fd;
	}
	configxml = xmlReadFd(fd, "_config_xml_", NULL, 0);
	close(fd);
	if (configxml == NULL) {
		syslog(LOG_ERR, "xml parse of config file %s failed", _config_xml_);
		return -1;
	}
	return 0;
}

/* elements based on localisation */
xmlNodePtr confixml_name()
{
	return element_based_localisation(_name_);
}

xmlNodePtr confixml_description()
{
	return element_based_localisation(_description_);
}

xmlNodePtr confixml_license()
{
	return element_based_localisation(_license_);
}

/* elements based on path localisation */
xmlNodePtr confixml_author()
{
	return first(_author_);
}

xmlNodePtr confixml_content()
{
	return first(_content_);
}

/* element multiple */

xmlNodePtr confixml_first_feature()
{
	return first(_feature_);
}

xmlNodePtr confixml_next_feature(xmlNodePtr node)
{
	return next(node->next, _feature_);
}

xmlNodePtr confixml_first_preference()
{
	return first(_preference_);
}

xmlNodePtr confixml_next_preference(xmlNodePtr node)
{
	return next(node->next, _preference_);
}

xmlNodePtr confixml_first_icon()
{
	return first(_icon_);
}

xmlNodePtr confixml_next_icon(xmlNodePtr node)
{
	return next(node->next, _icon_);
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
	int sw = score_dim(ref, x, _width_, width);
	int sh = score_dim(ref, x, _height_, height);
	return sw+sh < 0;
}

xmlNodePtr confixml_icon(int width, int height)
{
	xmlNodePtr resu, icon;

	resu = confixml_first_icon();
	icon = confixml_next_icon(resu);
	while (icon) {
		if (better_icon(resu, icon, width, height))
			resu = icon;
		icon = confixml_next_icon(icon);
	}
	return resu;
}


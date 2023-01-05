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

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>

#include <rp-utils/rp-verbose.h>
#include "wgt.h"
#include "wgt-config.h"
#include "wgt-strings.h"


static struct wgt *configwgt = NULL;
static xmlDocPtr configxml = NULL;

static xmlNodePtr next(xmlNodePtr node, const char *type)
{
	while (node && (node->type != XML_ELEMENT_NODE || strcmp(type, (char*)node->name)))
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
	char *lang = (char*)xmlNodeGetLang(node);
	unsigned int score = configwgt == NULL ? 0 : wgt_locales_score(configwgt, lang);
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

static int read_config_file(int fd, const char *pathname, struct wgt *wgt)
{
	assert(!configxml);
	if (fd < 0) {
		RP_ERROR("can't open config file %s", pathname);
		return fd;
	}
	configxml = xmlReadFd(fd, pathname, NULL, 0);
	close(fd);
	if (configxml == NULL) {
		RP_ERROR("xml parse of config file %s failed", pathname);
		return -1;
	}
	assert(xmlDocGetRootElement(configxml));
	configwgt = wgt;
	return 0;
}

int wgt_config_open(struct wgt *wgt)
{
	return read_config_file(wgt_open_read(wgt, string_config_dot_xml), string_config_dot_xml, wgt);
}

int wgt_config_open_fileat(int dirfd, const char *pathname)
{
	return read_config_file(openat(dirfd, pathname, O_RDONLY), pathname, NULL);
}

xmlNodePtr wgt_config_widget()
{
	xmlNodePtr root;
	assert(configxml);
	root = xmlDocGetRootElement(configxml);
	return strcmp(string_widget, (char*)root->name) ? NULL : root;
}

/* elements based on localisation */
xmlNodePtr wgt_config_name()
{
	assert(configxml);
	return element_based_localisation(string_name);
}

xmlNodePtr wgt_config_description()
{
	assert(configxml);
	return element_based_localisation(string_description);
}

xmlNodePtr wgt_config_license()
{
	assert(configxml);
	return element_based_localisation(string_license);
}

/* elements based on path localisation */
xmlNodePtr wgt_config_author()
{
	assert(configxml);
	return first(string_author);
}

xmlNodePtr wgt_config_content()
{
	assert(configxml);
	return first(string_content);
}

/* element multiple */

xmlNodePtr wgt_config_first_feature()
{
	assert(configxml);
	return first(string_feature);
}

xmlNodePtr wgt_config_next_feature(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, string_feature);
}

xmlNodePtr wgt_config_first_preference()
{
	assert(configxml);
	return first(string_preference);
}

xmlNodePtr wgt_config_next_preference(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, string_preference);
}

xmlNodePtr wgt_config_first_icon()
{
	assert(configxml);
	return first(string_icon);
}

xmlNodePtr wgt_config_next_icon(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, string_icon);
}

xmlNodePtr wgt_config_first_param(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->children, string_param);
}

xmlNodePtr wgt_config_next_param(xmlNodePtr node)
{
	assert(configxml);
	assert(node);
	return next(node->next, string_param);
}

/* best sized icon */

static int score_dim(xmlNodePtr ref, xmlNodePtr x, const char *dim, int request)
{
	int r, iref, ix;
	char *sref, *sx;

	sref = (char*)xmlGetProp(ref, (const xmlChar*)dim);
	if (sref) {
		iref = atoi(sref);
		xmlFree(sref);
		sx = (char*)xmlGetProp(x, (const xmlChar*)dim);
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
		sx = (char*)xmlGetProp(x, (const xmlChar*)dim);
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
	int sw = score_dim(ref, x, string_width, width);
	int sh = score_dim(ref, x, string_height, height);
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


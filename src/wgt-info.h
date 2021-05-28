/*
 Copyright (C) 2015-2020 IoT.bzh Company

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


struct wgt_desc_icon {
	struct wgt_desc_icon *next;
	char *src;
	int width;
	int height;
};

struct wgt_desc_param {
	struct wgt_desc_param *next;
	char *name;
	char *value;	
};

struct wgt_desc_feature {
	struct wgt_desc_feature *next;
	char *name;
	int required;
	struct wgt_desc_param *params;
};

struct wgt_desc_preference {
	struct wgt_desc_preference *next;
	char *name;
	char *value;
	int readonly;
};

struct wgt_desc {
	int refcount;
	char *id;
	char *id_underscore;
	char *version;
	char *ver;
	char *idaver;
	int width;
	int height;
	char *viewmodes;
	char *defaultlocale;
	char *name;
	char *name_short;
	char *description;
	char *author;
	char *author_href;
	char *author_email;
	char *license;
	char *license_href;
	char *content_src;
	char *content_type;
	char *content_encoding;
	struct wgt_desc_icon *icons;
	struct wgt_desc_feature *features;
	struct wgt_desc_preference *preferences;
};

struct wgt;
struct wgt_info;
extern struct wgt_info *wgt_info_create(struct wgt *wgt, int icons, int features, int preferences);
extern struct wgt_info *wgt_info_createat(int dirfd, const char *pathname, int icons, int features, int preferences);
extern const struct wgt_desc *wgt_info_desc(struct wgt_info *ifo);
extern struct wgt *wgt_info_wgt(struct wgt_info *ifo);
extern void wgt_info_addref(struct wgt_info *ifo);
extern void wgt_info_unref(struct wgt_info *ifo);
extern void wgt_info_dump(struct wgt_info *ifo, int fd, const char *prefix);
extern const struct wgt_desc_feature *wgt_info_feature(struct wgt_info *ifo, const char *name);
extern const char *wgt_info_param(const struct wgt_desc_feature *feature, const char *name);

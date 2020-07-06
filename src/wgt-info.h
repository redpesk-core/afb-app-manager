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


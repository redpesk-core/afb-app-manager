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


struct wgt_info_icon {
	struct wgt_info_icon *next;
	char *src;
	int width;
	int height;
};

struct wgt_info_param {
	struct wgt_info_param *next;
	char *name;
	char *value;	
};

struct wgt_info_feature {
	struct wgt_info_feature *next;
	char *name;
	int required;
	struct wgt_info_param *params;
};

struct wgt_info_preference {
	struct wgt_info_preference *next;
	char *name;
	char *value;
	int readonly;
};

struct wgt_info {
	int refcount;
	char *id;
	char *version;
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
	struct wgt_info_icon *icons;
	struct wgt_info_feature *features;
	struct wgt_info_preference *preferences;
};

struct wgt;
extern struct wgt_info *wgt_info_get(struct wgt *wgt, int icons, int features, int preferences);
extern void wgt_info_addref(struct wgt_info *info);
extern void wgt_info_unref(struct wgt_info *info);
extern void wgt_info_dump(struct wgt_info *info, int fd, const char *prefix);


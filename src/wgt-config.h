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

extern const char wgt_config_string_xml_file[];
extern const char wgt_config_string_name[];
extern const char wgt_config_string_description[];
extern const char wgt_config_string_author[];
extern const char wgt_config_string_license[];
extern const char wgt_config_string_icon[];
extern const char wgt_config_string_content[];
extern const char wgt_config_string_feature[];
extern const char wgt_config_string_preference[];
extern const char wgt_config_string_width[];
extern const char wgt_config_string_height[];

struct wgt;
extern int wgt_config_open(struct wgt *wgt);
extern void wgt_config_close();
extern xmlNodePtr wgt_config_name();
extern xmlNodePtr wgt_config_description();
extern xmlNodePtr wgt_config_license();
extern xmlNodePtr wgt_config_author();
extern xmlNodePtr wgt_config_content();
extern xmlNodePtr wgt_config_icon(int width, int height);
extern xmlNodePtr wgt_config_first_feature();
extern xmlNodePtr wgt_config_next_feature(xmlNodePtr node);
extern xmlNodePtr wgt_config_first_preference();
extern xmlNodePtr wgt_config_next_preference(xmlNodePtr node);
extern xmlNodePtr wgt_config_first_icon();
extern xmlNodePtr wgt_config_next_icon(xmlNodePtr node);


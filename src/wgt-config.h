/*
 Copyright (C) 2015-2019 IoT.bzh

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


struct wgt;
extern int wgt_config_open(struct wgt *wgt);
extern void wgt_config_close();
extern xmlNodePtr wgt_config_widget();
extern xmlNodePtr wgt_config_name();
extern xmlNodePtr wgt_config_description();
extern xmlNodePtr wgt_config_license();
extern xmlNodePtr wgt_config_author();
extern xmlNodePtr wgt_config_content();
extern xmlNodePtr wgt_config_icon(int width, int height);
extern xmlNodePtr wgt_config_first_icon();
extern xmlNodePtr wgt_config_next_icon(xmlNodePtr node);
extern xmlNodePtr wgt_config_first_feature();
extern xmlNodePtr wgt_config_next_feature(xmlNodePtr node);
extern xmlNodePtr wgt_config_first_preference();
extern xmlNodePtr wgt_config_next_preference(xmlNodePtr node);
extern xmlNodePtr wgt_config_first_param(xmlNodePtr node);
extern xmlNodePtr wgt_config_next_param(xmlNodePtr node);



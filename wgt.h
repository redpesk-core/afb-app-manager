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


#include <libxml/tree.h>
#include "config.h"


/**************************************************************/
/* from wgt-config-xml */

extern int confixml_open();
extern void confixml_close();
extern xmlNodePtr confixml_name();
extern xmlNodePtr confixml_description();
extern xmlNodePtr confixml_license();
extern xmlNodePtr confixml_author();
extern xmlNodePtr confixml_content();
extern xmlNodePtr confixml_icon(int width, int height);
extern xmlNodePtr confixml_first_feature();
extern xmlNodePtr confixml_next_feature(xmlNodePtr node);
extern xmlNodePtr confixml_first_preference();
extern xmlNodePtr confixml_next_preference(xmlNodePtr node);
extern xmlNodePtr confixml_first_icon();
extern xmlNodePtr confixml_next_icon(xmlNodePtr node);

/**************************************************************/
/* from wgt-locales */

extern void locales_reset();
extern int locales_add(const char *locstr);
extern int locales_score(const char *lang);
extern char *locales_locate_file(const char *filename);

/**************************************************************/
/* from wgt-rootdir */

extern int widget_set_rootdir(const char *pathname);
extern int widget_has(const char *filename);
extern int widget_open_read(const char *filename);

/**************************************************************/
/* from wgt-strings */

extern const char _config_xml_[];
extern const char _name_[];
extern const char _description_[];
extern const char _author_[];
extern const char _license_[];
extern const char _icon_[];
extern const char _content_[];
extern const char _feature_[];
extern const char _preference_[];
extern const char _width_[];
extern const char _height_[];


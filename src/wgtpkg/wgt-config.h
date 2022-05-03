/*
 Copyright (C) 2015-2022 IoT.bzh Company

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


struct wgt;
extern int wgt_config_open(struct wgt *wgt);
extern int wgt_config_open_fileat(int dirfd, const char *pathname);
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



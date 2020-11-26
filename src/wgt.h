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

struct wgt;

/* creation and reference */
extern struct wgt *wgt_create();
extern void wgt_addref(struct wgt *wgt);
extern void wgt_unref(struct wgt *wgt);

/* connection and disconnection */
extern struct wgt *wgt_createat(int dirfd, const char *pathname);
extern int wgt_connect(struct wgt *wgt, const char *pathname);
extern int wgt_connectat(struct wgt *wgt, int dirfd, const char *pathname);
extern void wgt_disconnect(struct wgt *wgt);
extern int wgt_is_connected(struct wgt *wgt);

/* management of locales */
extern void wgt_locales_reset(struct wgt *wgt);
extern int wgt_locales_add(struct wgt *wgt, const char *locstr);
extern unsigned int wgt_locales_score(struct wgt *wgt, const char *lang);

/* direct access to files */
extern int wgt_has(struct wgt *wgt, const char *filename);
extern int wgt_open_read(struct wgt *wgt, const char *filename);

/* localised access to files */
extern char *wgt_locales_locate(struct wgt *wgt, const char *filename);
extern int wgt_locales_open_read(struct wgt *wgt, const char *filename);


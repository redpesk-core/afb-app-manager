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

#pragma once

#ifndef AFMPKG_SOCKET_ADDRESS
#define AFMPKG_SOCKET_ADDRESS "@afmpkg-daemon.socket"
#endif

#define AFMPKG_OPERATION_ADD           "ADD"
#define AFMPKG_OPERATION_REMOVE        "REMOVE"


#define AFMPKG_KEY_BEGIN           "BEGIN"
#define AFMPKG_KEY_COUNT           "COUNT"
#define AFMPKG_KEY_END             "END"
#define AFMPKG_KEY_ERROR           "ERROR"
#define AFMPKG_KEY_FILE            "FILE"
#define AFMPKG_KEY_INDEX           "INDEX"
#define AFMPKG_KEY_OK              "OK"
#define AFMPKG_KEY_PACKAGE         "PACKAGE"
#define AFMPKG_KEY_REDPAKID        "REDPAKID"
#define AFMPKG_KEY_ROOT            "ROOT"
#define AFMPKG_KEY_STATUS          "STATUS"
#define AFMPKG_KEY_TRANSID         "TRANSID"


#define AFMPKG_ENVVAR_TRANSID      "AFMPKG_TRANSID"
#define AFMPKG_ENVVAR_REDPAKID     "AFMPKG_REDPAKID"

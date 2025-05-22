/*
 Copyright (C) 2015-2025 IoT.bzh Company

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

/** definition of unit's scope */
enum unitscope {
	/** unset or unknown scope */
	unitscope_unknown = 0,
	/** system scope */
	unitscope_system,
	/** user scope */
	unitscope_user
};

/** definition of unit's type */
enum unittype {
	/** unset or unknown type */
	unittype_unknown = 0,
	/** service type */
	unittype_service,
	/** socket type */
	unittype_socket
};

/** definition of a unit */
struct unitdesc {
	/** scope of the unit */
	enum unitscope scope;
	/** type of the unit */
	enum unittype type;
	/** name of the unit */
	const char *name;
	/** length of the name */
	size_t name_length;
	/** content of the unit (not zero ended) */
	const char *content;
	/** length of the unit */
	size_t content_length;
	/** name wanting the unit or NULL */
	const char *wanted_by;
	/** length of wanted_by */
	size_t wanted_by_length;
};

extern int unit_desc_check(const struct unitdesc *desc, int tells);
extern int unit_desc_get_path(const struct unitdesc *desc, char *path, size_t pathlen);
extern int unit_desc_get_wants_path(const struct unitdesc *desc, char *path, size_t pathlen);
extern int unit_desc_get_wants_target(const struct unitdesc *desc, char *path, size_t pathlen);
extern int unit_desc_get_service(const struct unitdesc *desc, char *serv, size_t servlen);


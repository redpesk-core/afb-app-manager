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

#include "unit-process.h"

struct unitconf {
	json_object *metadata;
	int (*new_afid)();
	int base_http_ports;
};

struct generatedesc {
	struct json_object *desc;
	const struct unitdesc *units;
	int nunits;
};

extern int unit_generator_process(struct json_object *jdesc, const struct unitconf *conf, int (*process)(void *closure, const struct generatedesc *desc), void *closure);
extern int unit_generator_install(struct json_object *manifest, const struct unitconf *conf);
extern int unit_generator_uninstall(struct json_object *manifest, const struct unitconf *conf);


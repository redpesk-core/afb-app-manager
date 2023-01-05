/*
 Copyright (C) 2015-2023 IoT.bzh Company

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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <json-c/json.h>

#include "wgtpkg-unit.h"

#include "wgt-json.h"
#include "unit-generator.h"

static int do_install_uninstall(
		struct wgt_info *ifo,
		const struct unitconf *conf,
		int (*doer)(struct json_object *, const struct unitconf *)
)
{
	int rc;
	struct json_object *jdesc;

	jdesc = wgt_info_to_json(ifo);
	if (!jdesc)
		rc = -1;
	else {
		rc = doer(jdesc, conf);
		json_object_put(jdesc);
	}
	return rc;
}

int unit_install(struct wgt_info *ifo, const struct unitconf *conf)
{
	return do_install_uninstall(ifo, conf, unit_generator_install);
}

int unit_uninstall(struct wgt_info *ifo, const struct unitconf *conf)
{
	return do_install_uninstall(ifo, conf, unit_generator_uninstall);
}

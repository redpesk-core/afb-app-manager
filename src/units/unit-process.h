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

#include "unit-desc.h"

struct json_object;

/**
 * open/select the template file
 *
 * @return 0 on success
 */
extern int unit_process_open_template(const char *filename);

/**
 * close the template file
 *
 * @return 0 on success
 */
extern void unit_process_close_template();

/**
 * Instanciate the currently opened template (or the default one if none
 * were opened) with the values of `jdesc`. Call the function process
 * with the value of that instanciation.
 *
 * @param jdesc    the actual values for the template
 * @param process  the callback receiving the instanciation
 * @param closure  closure of the callback
 *
 * @return 0 on sucess or a negative value on error
 */
extern int unit_process_raw(
		struct json_object *jdesc,
		int (*process)(void *closure, char *text, size_t size),
		void *closure);

/**
 * Instanciate the currently opened template (or the default one if none
 * were opened) with the values of `jdesc`. Split the instantiation
 * in its unit files. Call the function process with description of the
 * generated files.
 *
 * @param jdesc    the actual values for the template
 * @param process  the callback receiving the splitted files
 * @param closure  closure of the callback
 *
 * @return 0 on sucess or a negative value on error
 */
extern int unit_process_split(
		struct json_object *jdesc,
		int (*process)(void *closure, const struct unitdesc *units, int nrunits),
		void *closure);



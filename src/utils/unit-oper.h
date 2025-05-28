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

/**
 * Stops the systemd service of the given unit
 *
 * @param unit the unit to be stopped
 *
 * @return 0 on success or a negative value on error
 */
extern int unit_oper_stop(const struct unitdesc *unit);

/**
 * Stops the systemd service of the given unit
 * and uninstall it
 *
 * @param unit the unit to be uninstalled
 *
 * @return 0 on success or a negative value on error
 */
extern int unit_oper_uninstall(const struct unitdesc *unit);

/**
 * Install the service described by the unit
 *
 * @param unit the unit to be installed
 *
 * @return 0 on success or a negative value on error
 */
extern int unit_oper_install(const struct unitdesc *unit);

/**
 * Check that systemd files for the unit either exist or not
 *
 * @param unit          the unit to be checked
 * @param should_exist  0 for checking that files does not exist
 *                      not 0 for checking that files exist
 *
 * @return 0 on success (files either exist if should_exist, or
 *         file doesn't exist if not should_exist) or -1 otherwise
 */
extern int unit_oper_check_files(const struct unitdesc *unit, int should_exist);


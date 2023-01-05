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

#pragma once

#include "path-entry.h"

/**
 * @brief structure recording data of a request
 */
typedef struct
{
	/** name of the package */
	char *package;

	/** list the files */
	path_entry_t *files;

	/** root directory */
	char *root;

	/** some redakid */
	char *redpakid;
}
	afmpkg_t;

/**
 * @brief installs the package described by apkg
 *
 * @param apkg description of the package to be installed
 * @return 0 on success or a negative error code
 */
extern int afmpkg_install(const afmpkg_t *apkg);

/**
 * @brief uninstalls the package described by apkg
 *
 * @param apkg description of the package to be installed
 * @return 0 on success or a negative error code
 */
extern int afmpkg_uninstall(const afmpkg_t *apkg);

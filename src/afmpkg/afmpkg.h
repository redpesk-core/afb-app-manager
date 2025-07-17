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

#include "path-entry.h"
#include "path-type.h"
#include "unit-desc.h"

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

	/** the redpakid */
	char *redpakid;

	/** redpak automatic file */
	char *redpak_auto;
}
	afmpkg_t;

/*****************************************************************************/
/*** PUBLIC ******************************************************************/
/*****************************************************************************/

/**
* processing mode
*/
typedef
enum
{
	/** no operation */
	Afmpkg_Nop = 0,
	/** installation */
	Afmpkg_Install,
	/** uninstallation */
	Afmpkg_Uninstall
}
	afmpkg_mode_t;

/**
* operations
*/
typedef
struct
{
	/**
	* Called for beginning definition of an application
	* when appid isn't NULL, and for beginning definition
	* of out of application items when appid is NULL.
	*
	* @param closure the closure
	* @param appid   the application ID or NULL if not for an application
	* @param mode    the mode: Afmpkg_Install or Afmpkg_Uninstall
	*
	* @return 0 on success or a negative value on error
	*/
	int (*begin)(void *closure, const char *appid, afmpkg_mode_t mode);

	/**
	* Tag a file with the given value.
	*
	* @param closure the closure
	* @param path    path of the file (or directory) to tag
	* @param tag     type of the tag to apply
	*
	* @return 0 on success or a negative value on error
	*/
	int (*tagfile)(void *closure, const char *path, path_type_t tag);

	/**
	* Set permission
	*
	* @param closure the closure
	* @param perm    the permission
	*
	* @return 0 on success or a negative value on error
	*/
	int (*setperm)(void *closure, const char *perm);

	/**
	* Set plug
	*
	* @param closure   the closure
	* @param exportdir the exported directory
	* @param importid  the imported id
	* @param importdir the imported directory
	*
	* @return 0 on success or a negative value on error
	*/
	int (*setplug)(
		void *closure,
		const char *exportdir,
		const char *importid,
		const char *importdir
	);

	/**
	* Set units
	*
	* @param closure the closure
	* @param units   description of the units to set
	* @param nrunits count of units to set
	*
	* @return 0 on success or a negative value on error
	*/
	int (*setunits)(void *closure, const struct unitdesc *units, int nrunits);

	/**
	* End of the transaction
	*
	* @param closure the closure
	* @param status  the status, zero on success, a negative value for rollback
	*
	* @return 0 on success or a negative value on error or status if status is
	*         negative
	*/
	int (*end)(void * closure, int status);
}
	afmpkg_operations_t;


/**
 * @brief installs the package described by apkg
 *
 * @param apkg    description of the package to be installed
 * @param opers   operations called by installer
 * @param closure closure of operations
 *
 * @return 0 on success or a negative error code
 */
extern int afmpkg_install(
		const afmpkg_t *apkg,
		const afmpkg_operations_t *opers,
		void *closure
);

/**
 * @brief uninstalls the package described by apkg
 *
 * @param apkg    description of the package to be installed
 * @param opers   operations called by uninstaller
 * @param closure closure of operations
 *
 * @return 0 on success or a negative error code
 */
extern int afmpkg_uninstall(
		const afmpkg_t *apkg,
		const afmpkg_operations_t *opers,
		void *closure
);


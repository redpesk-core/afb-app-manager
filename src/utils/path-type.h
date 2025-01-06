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

/**
* Definition of the managed path types
*/
typedef
enum path_type
{
    path_type_Unset = 0,	/**< special unset value, don't use it */
    path_type_Unknown,		/**< special unknown value */
    path_type_Conf,		/**< for configuration */
    path_type_Data,		/**< for data */
    path_type_Exec,		/**< for executable */
    path_type_Http,		/**< for file served by HTTP */
    path_type_Icon,		/**< for icon or public image */
    path_type_Id,		/**< for private data */
    path_type_Lib,		/**< for private library */
    path_type_Plug,		/**< for exported plug */
    path_type_Public,		/**< for public data */
    path_type_Public_Exec,	/**< for public executable */
    path_type_Public_Lib	/**< for public library */
}
	path_type_t;

/**
* Get the path type of the given property key or path_type_Unknown
* if the key is unknown.
*
* @param key   the key whose type is searched
*
* @return the found path_type or path_type_Unknown when the
*         key is not known
*/
extern path_type_t path_type_of_property_key(const char *key);

/**
* Search if there is a predefined path_type for a directory of
* the given name and return it or return path_type_Unknown if
* no predefined directory name matches.
*
* @param dirname   the name of the directory to search
*
* @return the found path_type or path_type_Unknown when there
*         is no predefined valu for that name
*/
extern path_type_t path_type_of_dirname(const char *dirname);


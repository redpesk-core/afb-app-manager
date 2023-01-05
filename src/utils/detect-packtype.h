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

/**
 * detected type of a package
 */
typedef enum packtype_e {
	/** unknown */
	packtype_Unknown,
	/** type AfmPkg */
	packtype_AfmPkg,
	/** type widget (legacy) */
	packtype_Widget
} packtype_t;

/**
 * @brief Inspect the filename to detect if it represents
 * a filename of a specific packaging file and if yes of
 * what kind.
 *
 * @param packname package name (can be NULL when plen == 0)
 * @param plen length of the package name (can be 0)
 * @param filename name of the file
 * @param flen length of file name
 * @param blen if not null receive the length of the base name
 *
 * @return the detected type
 */
extern
packtype_t
detect_packtype(
	const char *packname,
	size_t plen,
	const char *filename,
	size_t flen,
	size_t *blen);


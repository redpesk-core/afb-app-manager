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
 * @brief abstract structure for set of permissions
 */
typedef struct permset_s permset_t;

/**
 * @brief selector for being used with permset_select_first
 * and permset_select_next
 */
typedef enum permset_select_e {

	/** select any permission */
	permset_Select_Any,

	/** select requested permission */
	permset_Select_Requested,

	/** select granted permission */
	permset_Select_Granted,

	/** select granted and requested permission */
	permset_Select_Requested_And_Granted
}
	permset_select_t;

/**
 * @brief selector for reseting flags with permset_reset
 */
typedef enum permset_reset_e {

	/** no reset */
	permset_Reset_Nothing,

	/** reset requested flag */
	permset_Reset_Requested,

	/** reset granted flag */
	permset_Reset_Granted,

	/** reset requested and granted flags */
	permset_Reset_Requested_And_Granted
}
	permset_reset_t;


extern void permset_reset(permset_t *permset, permset_reset_t it);
extern int permset_has(permset_t *permset, const char *name);
extern int permset_grant(permset_t *permset, const char *name);
extern int permset_grant_list(permset_t *permset, const char *list);
extern int permset_add(permset_t *permset, const char *name);
extern int permset_add_list(permset_t *permset, const char *list);
extern int permset_request(permset_t *permset, const char *name);
extern int permset_request_list(permset_t *permset, const char *list);

extern int permset_select_first(permset_t *permset, permset_select_t it);
extern int permset_select_next(permset_t *permset, permset_select_t it);
extern const char *permset_current(permset_t *permset);
extern int permset_is_current_requested(permset_t *permset);
extern int permset_is_current_granted(permset_t *permset);

extern int permset_create(permset_t **permset);
extern void permset_destroy(permset_t *permset);

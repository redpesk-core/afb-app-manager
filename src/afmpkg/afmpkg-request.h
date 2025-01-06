/*
 * Copyright (C) 2018-2025 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 */
#pragma once

#include "path-entry.h"
#include "afmpkg.h"

/**
 * @brief state of request
 */
typedef enum afmpkg_request_state
{
	/** request being composed */
	Request_Pending,

	/** request composed and ready */
	Request_Ready,

	/** request processed correctly */
	Request_Ok,

	/** request processed with error */
	Request_Error
}
	afmpkg_request_state_t;

/**
 * @brief kind of request
 */
typedef enum afmpkg_request_kind
{
	/** unset (initial value) */
	Request_Unset,

	/** request for adding a package */
	Request_Add_Package,

	/** request to remove a package */
	Request_Remove_Package,

	/** request for checking add of a package */
	Request_Check_Add_Package,

	/** request for checkinfg remove a package */
	Request_Check_Remove_Package,

	/** request to get the status of a transaction */
	Request_Get_Status
}
	afmpkg_request_kind_t;

/**
 * @brief structure recording data of a request
 */
typedef struct afmpkg_request
{
	/** the state of the request */
	afmpkg_request_state_t state;

	/** the kind of the request */
	afmpkg_request_kind_t kind;

	/** status code */
	int scode;

	/** index of the request in the transaction set */
	unsigned index;

	/** count of requests in the transaction set */
	unsigned count;

	/** identifier of the transaction */
	char *transid;

	/** scratch buffer */
	char *scratch;

	/** reply  message */
	const char *msg;

	/** the packaging request */
	afmpkg_t apkg;
}
	afmpkg_request_t;


/**
 * @brief init a request structure
 *
 * @param req the request to init
 *
 * @return 0 on success or else a negative code
 */
extern int afmpkg_request_init(afmpkg_request_t *req);

/**
 * @brief deinit a request structure, freeing its memory
 *
 * @param req the request to init
 */
extern void afmpkg_request_deinit(afmpkg_request_t *req);

/**
 * @brief set the status of the request
 *
 * @param req the request to set
 * @param scode the code to set
 * @param msg an associated message
 *
 * @return the value scode
 */
extern int afmpkg_request_error(afmpkg_request_t *req, int scode, const char *msg);

/**
 * @brief process a request
 *
 * @param req the request to be processed
 * @return 0 on success or a negative error code
 */
extern int afmpkg_request_process(afmpkg_request_t *req);

/**
 * @brief process a line of request
 *
 * @param req the request to fill
 * @param line the line to process (must be zero terminated)
 * @param length length of the line
 * @return 0 on success or a negative error code
 */
extern int afmpkg_request_add_line(afmpkg_request_t *req, const char *line, size_t length);

/**
 * @brief get the reply line of request
 *
 * @param req the request
 * @param line the line to set
 * @param length length of the line
 * @return 0 on success or a negative error code
 */
extern size_t afmpkg_request_make_reply_line(afmpkg_request_t *req, char *line, size_t length);

/**
 * @brief check if stopping is possible
 *
 * @return 0 if transactions are pending or a non zero value when stop is possible
 */
extern int afmpkg_request_can_stop();

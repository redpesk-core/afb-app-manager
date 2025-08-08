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
#define _GNU_SOURCE

#include <sys/socket.h>
#include <errno.h>

#include "afmpkg-server.h"
#include "afmpkg-proto.h"
#include "afmpkg-request.h"
#include "afmpkg.h"

#if !NO_SEND_SIGHUP_ALL
#include "sighup-framework.h"
#endif

/**
 * @brief receive the request
 *
 * @param sock the input socket
 * @param req the request for recording data
 * @return 0 on success or a negative error code
 */
static int receive(int sock, afmpkg_request_t *req)
{
	int rc;
	char buffer[32768];
	ssize_t sz;
	size_t length = 0, it, eol;

	for (rc = 0 ; rc >= 0 ; ) {
		/* blocking read of socket */
		do { sz = recv(sock, &buffer[length], sizeof buffer - length, 0); } while(sz == -1 && errno == EINTR);
		if (sz == -1) {
			rc = afmpkg_request_error(req, -2000, "receive error");
			break;
		}
		else {
			/* end of input? */
			if (sz == 0)
				break;

			/* extract the data */
			length = (size_t) sz;
			it = 0;
			for (it = 0 ; it < length && rc >= 0 ; ) {
				/* search the end of the line */
				for(eol = it ; eol < length && buffer[eol] != '\n' ; eol++);
				if (eol == length) {
					/* not found, shift end of the buffer */
					eol = it;
					length -= it;
					for (it = 0 ; it < length ; )
						buffer[it++] = buffer[eol++];
				}
				else {
					if (eol != it) {
						/* found a not empty line, afmpkg_request_process it */
						buffer[eol] = 0;
						rc = afmpkg_request_add_line(req, &buffer[it], eol - it);
					}
					it = eol + 1;
				}
			}
		}
	}
	shutdown(sock, SHUT_RD);
	return rc;
}

/**
 * @brief send a reply to the client
 *
 * @param sock socket for sending to client
 * @param rc the status
 * @param arg a string argument for the status
 * @param length size of the argument string arg
 */
static void reply(int sock, afmpkg_request_t *req)
{
	char buffer[512];
	size_t length;
	ssize_t sz;

	/* make the message */
	length = afmpkg_request_make_reply_line(req, buffer, sizeof buffer);
	if (length >= sizeof buffer) {
		length = sizeof buffer;
		buffer[length - 1] = '\n';
	}

	/* send the status */
	do { sz = send(sock, buffer, length, 0); } while(sz == -1 && errno == EINTR);
	shutdown(sock, SHUT_WR);
}

/**
 * @brief serve a client
 *
 * This is not a loop. When a client connects, only one request is served
 * and the the socket is closed.
 *
 * @param sock socket for dialing with the client
 * @return 0 on success or a negative error code
 */
int afmpkg_serve(int sock)
{
	int rc;
	afmpkg_request_t request; /* in stack request */

	/* init the request */
	rc = afmpkg_request_init(&request);

	/* receive the request */
	if (rc >= 0)
		rc = receive(sock, &request);

	/* afmpkg_request_process the request */
	if (rc >= 0) {
		rc = afmpkg_request_process(&request);
#if !NO_SEND_SIGHUP_ALL
		switch (request.kind) {
		case Request_Add_Package:
		case Request_Remove_Package:
			sighup_all();
			break;
		default:
			break;
		}
#endif
	}

	/* reply to the request */
	reply(sock, &request);

	/* reset the memory */
	afmpkg_request_deinit(&request);
	return rc;
}


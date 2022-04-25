/*
 Copyright (C) 2015-2022 IoT.bzh Company

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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "afmpkg-common.h"

static const char framework_address[] = AFMPKG_SOCKET_ADDRESS;

/** connect to the framework */
static int connect_framework()
{
	int rc, sock;
	struct sockaddr_un adr;

	/* create the socket */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		rc = -errno;
	else {
		/* connect the socket */
		adr.sun_family = AF_UNIX;
		memcpy(adr.sun_path, framework_address, sizeof framework_address);
		if (adr.sun_path[0] == '@')
			adr.sun_path[0] = 0;
		rc = connect(sock, (struct sockaddr*)&adr, sizeof adr);
		if (rc >= 0)
			rc = sock;
		else {
			rc = -errno;
			close(sock);
		}
	}
	return rc;
}

/** disconnect from the framework */
static void disconnect_framework(int sock)
{
	if (sock >= 0)
		close(sock);
}

/** send something to the framework */
static int send_framework(int sock, const char *buffer, size_t length)
{
	ssize_t sz;
	do { sz = send(sock, buffer, length, 0); } while(sz == -1 && errno == EINTR);
	return sz < 0 ? -errno : 0;
}

/** receive something from the framework */
static int recv_framework(int sock, char **arg)
{
	int rc;
	char inputbuf[1000];
	ssize_t sz;

	if (arg != NULL)
		*arg = NULL;

	/* blocking socket ensure sz == length */
	do { sz = recv(sock, inputbuf, sizeof inputbuf - 1, 0); } while(sz == -1 && errno == EINTR);
	if (sz < 0)
		return -errno;

	/* the reply must be atomic */
	while (sz && inputbuf[sz - 1] == '\n') sz--;
	inputbuf[sz] = 0;
	if (0 == memcmp(inputbuf, AFMPKG_KEY_OK, strlen(AFMPKG_KEY_OK))) {
		rc = 1;
		sz = strlen(AFMPKG_KEY_OK);
	}
	else if (0 == memcmp(inputbuf, AFMPKG_KEY_ERROR, strlen(AFMPKG_KEY_ERROR))) {
		rc = 0;
		sz = strlen(AFMPKG_KEY_ERROR);
	}
	else
		return -EBADMSG;

	if (inputbuf[sz] != 0) {
		if (inputbuf[sz] != ' ')
			return -EBADMSG;
		do { sz++; } while(inputbuf[sz] == ' ');
	}

	if (arg != NULL && inputbuf[sz] != 0)
		*arg = strdup(&inputbuf[sz]);

	return rc;
}

static int error(const char *message)
{
	if (message != NULL)
		fprintf(stderr, "error: %s\n", message);
	exit(1);
	return 1;
}

int main(int ac, char **av)
{
	int len, sock, rc;
	char buffer[1000];
	char *reply;

	if (ac != 2)
		return error("one parameter is expected");

	len = snprintf(buffer, sizeof buffer, "%s %s\n", AFMPKG_KEY_STATUS, av[1]);
	if (len < 0 || len >= (int)sizeof buffer)
		return error("too long");

	sock = connect_framework();
	if (sock < 0)
		return error("can't connect");

	rc = send_framework(sock, buffer, (size_t)len);
	if (rc < 0)
		return error("can't send");
	shutdown(sock, SHUT_WR);

	rc = recv_framework(sock, &reply);
	if (rc < 0)
		return error("can't receive");

	if (rc > 0 && reply != NULL)
		printf("%s\n", reply);
	free(reply);

	disconnect_framework(sock);
	return rc <= 0 || reply == NULL;
}
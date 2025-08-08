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

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "afmpkg-client.h"
#include "afmpkg-proto.h"

/***************************************************/

#define MEMO_ROOTDIR  1
#define MEMO_TRANSID  2
#define MEMO_REDPAKID 4

/***************************************************/

static const char framework_address[] = AFMPKG_SOCKET_ADDRESS;
static const char * const operations[] = {
	[afmpkg_operation_Add]          = AFMPKG_OPERATION_ADD,
	[afmpkg_operation_Remove]       = AFMPKG_OPERATION_REMOVE,
	[afmpkg_operation_Check_Add]    = AFMPKG_OPERATION_CHECK_ADD,
	[afmpkg_operation_Check_Remove] = AFMPKG_OPERATION_CHECK_REMOVE
};

/***************************************************/

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
		memset(&adr, 0, sizeof adr);
		adr.sun_family = AF_UNIX;
		memcpy(adr.sun_path, framework_address, sizeof framework_address);
		if (adr.sun_path[0] == '@')
			adr.sun_path[0] = 0;
		rc = connect(sock, (struct sockaddr*)&adr, offsetof(struct sockaddr_un,sun_path) + sizeof framework_address - !adr.sun_path[0]);
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
	do {
		sz = recv(sock, inputbuf, sizeof inputbuf - 1, 0);
	} while(sz == -1 && errno == EINTR);
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

	/* extract reply info */
	if (inputbuf[sz] != 0) {
		if (inputbuf[sz] != ' ')
			return -EBADMSG;
		do { sz++; } while(inputbuf[sz] == ' ');
	}
	if (arg != NULL && inputbuf[sz] != 0)
		*arg = strdup(&inputbuf[sz]);
	return rc;
}

/***************************************************/

static int put_key_val_nl(afmpkg_client_t *client, const char *key, const char *val)
{
	char *ptr;
	size_t szkey, szval, pos, fpos, size, nxtsz;

	if (client->state != afmpkg_client_state_Started)
		return -EINVAL;

	szkey = strlen(key);
	szval = strlen(val);
	pos   = client->length;
	fpos  = pos + 2 + szval + szkey;

	size = client->size;
	if (fpos >= size) { /* or equal ensures one cell for trailing null */
		while (fpos >= size) {
			nxtsz = size << 1;
			if (nxtsz < size) /* overflow? */
				return -EINVAL;
			size = nxtsz;
		}
		if (client->buffer == client->buffer0)
			ptr = malloc(size);
		else
			ptr = realloc(client->buffer, size);
		if (ptr == NULL)
			return -ENOMEM;
		client->buffer = ptr;
		client->size = size;
	}
	ptr = &client->buffer[pos];
	memcpy(ptr, key, szkey);
	ptr += szkey;
	*ptr++ = ' ';
	memcpy(ptr, val, szval);
	ptr[szval] = '\n';
	client->length = fpos;
	return 0;
}

static int put_key_val_nl_memo(afmpkg_client_t *client, const char *key, const char *val, char memo)
{
	int rc;

	if ((client->memo & memo) != 0)
		rc = -EINVAL;
	else {
		rc = put_key_val_nl(client, key, val);
		if (rc == 0)
			client->memo |= memo;
	}
	return rc;
}

/**
 * @brief translate a positive integer to a zero terminated string
 *
 * @param value the value to convert
 * @param buffer the buffer for storing the value
 * @return the computed string representation
 */
#define ITOALEN 25
static const char *itoa(int value, char buffer[ITOALEN])
{
	char *p = &buffer[ITOALEN - 1];
	*p = 0;
	do { *--p = (char)('0' + value % 10); value /= 10; } while(value);
	return p;
}

/***************************************************/

void afmpkg_client_init(afmpkg_client_t *client)
{
	client->buffer = client->buffer0;
	client->length = 0;
	client->size = sizeof client->buffer0;
	client->state = afmpkg_client_state_None;
}

void afmpkg_client_release(afmpkg_client_t *client)
{
	if (client->buffer != client->buffer0)
		free(client->buffer);
	afmpkg_client_init(client);
}

int afmpkg_client_begin(
		afmpkg_client_t *client,
		afmpkg_operation_t operation,
		const char *package_name,
		int index,
		int count
) {
	char scratch[ITOALEN];

	if (operation < afmpkg_operation_Add || operation > afmpkg_operation_Check_Remove)
		return -EINVAL;

	client->length = 0;
	client->operation = operation;
	client->state = afmpkg_client_state_Started;
	client->memo = 0;
	return put_key_val_nl(client, AFMPKG_KEY_BEGIN, operations[operation])
	    ?: put_key_val_nl(client, AFMPKG_KEY_PACKAGE, package_name)
	    ?: put_key_val_nl(client, AFMPKG_KEY_INDEX, itoa(index, scratch))
	    ?: put_key_val_nl(client, AFMPKG_KEY_COUNT, itoa(count, scratch));
}

int afmpkg_client_end(afmpkg_client_t *client)
{
	int rc;

	rc = put_key_val_nl(client, AFMPKG_KEY_END, operations[client->operation]);
	client->state = afmpkg_client_state_Ready;
	if (rc == 0)
		client->buffer[client->length] = 0;
	return rc;
}

int afmpkg_client_put_file(afmpkg_client_t *client, const char *value)
{
	return put_key_val_nl(client, AFMPKG_KEY_FILE, value);
}

int afmpkg_client_put_rootdir(afmpkg_client_t *client, const char *value)
{
	return put_key_val_nl_memo(client, AFMPKG_KEY_ROOT, value, MEMO_ROOTDIR);
}

int afmpkg_client_put_transid(afmpkg_client_t *client, const char *value)
{
	return put_key_val_nl_memo(client, AFMPKG_KEY_TRANSID, value, MEMO_TRANSID);
}

int afmpkg_client_put_redpakid(afmpkg_client_t *client, const char *value)
{
	return put_key_val_nl_memo(client, AFMPKG_KEY_REDPAKID, value, MEMO_REDPAKID);
}

int afmpkg_client_dial(afmpkg_client_t *client, char **errstr)
{
	int rc, sock;

	rc = connect_framework();
	if (rc >= 0) {
		sock = rc;
		rc = send_framework(sock, client->buffer, client->length);
		if (rc >= 0) {
			shutdown(sock, SHUT_WR);
			rc = recv_framework(sock, errstr);
		}
		disconnect_framework(sock);
	}
	return rc;
}


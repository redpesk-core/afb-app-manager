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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>
#include <rpm/rpmte.h>
#include <rpm-plugins/rpmplugin.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "afmpkg-common.h"

/***************************************************
* RPM plugins MUST have a NAME following C naming
* conventions: characters, digits, underscores
*
* The library of the plugin MUST be named:
*
*    NAME.so
*
* For transaction (no other case known), the RPM
* environment must include a declaration of the
* form:
*
*   __transaction_NAME %{__plugindir}/NAME.so
*
* RPM searchs for the symbol named:
*
*   NAME_hooks
***************************************************/

/* debug te type */
static const char * type_te(rpmte te)
{
	switch (rpmteType(te)) {
		case TR_ADDED: return "TR_ADDED";
		case TR_REMOVED: return "TR_REMOVED";
		default: return "UNKNOWN";
	}
}

static void dump_te(rpmte te)
{
	const char *filename;
	const char *name = rpmteN(te);
	rpmfiles files;
	rpmfi fi;

	rpmlog(RPMLOG_DEBUG, "[REDPESK] - TE %p\n", te);
	rpmlog(RPMLOG_DEBUG, "[REDPESK]   type ...: %d = %s\n", (int)rpmteType(te), type_te(te));
	rpmlog(RPMLOG_DEBUG, "[REDPESK]   name (N): %s (%p)\n", name, name);
	files = rpmteFiles(te);
	if (files == NULL)
		rpmlog(RPMLOG_DEBUG, "[REDPESK]   no files\n");
	else {
		rpmlog(RPMLOG_DEBUG, "[REDPESK]   files:\n");
		fi = rpmfilesIter(files, RPMFI_ITER_FWD);
		while (rpmfiNext(fi) >= 0) {
			filename = rpmfiFN(fi);
			rpmlog(RPMLOG_DEBUG, "[REDPESK]     - %s\n", filename);
		}
		rpmfiFree(fi);
		rpmfilesFree(files);
	}
}

static void dump_ts(rpmts ts, const char *step)
{
	if (rpmIsDebug()) {
		rpmte te;
		int idx;

		rpmlog(RPMLOG_DEBUG, "[REDPESK] entering step %s\n", step);
		rpmlog(RPMLOG_DEBUG, "[REDPESK] TS %p %d\n", ts, rpmtsGetTid(ts));
		rpmlog(RPMLOG_DEBUG, "[REDPESK] At root %s\n", rpmtsRootDir(ts));
		rpmlog(RPMLOG_DEBUG, "[REDPESK] %d elements\n", rpmtsNElements(ts));
		for (idx = 0 ; (te = rpmtsElement(ts, idx)) != NULL ; idx++ ) {
			rpmlog(RPMLOG_DEBUG, "[REDPESK] +element %d\n", idx);
			dump_te(te);
		}
	}
}

/***************************************************/

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

/** dial with the framework */
static int dial_framework(const char *buffer, size_t length, char **errstr)
{
	int rc, sock;

	if (rpmIsDebug())
		rpmlog(RPMLOG_DEBUG, "[REDPESK] SENDING\n%.*s", (int)length, buffer);

	rc = connect_framework();
	if (rc >= 0) {
		sock = rc;
		rc = send_framework(sock, buffer, length);
		if (rc >= 0) {
			shutdown(sock, SHUT_WR);
			rc = recv_framework(sock, errstr);
		}
		disconnect_framework(sock);
	}
	return rc;
}

/***************************************************/

/**
 * this structure is used to record installed files for post processing
 */
typedef struct record {

	/** chaining structures to record */
	struct record *next;

	/** the transaction set */
	rpmts ts;

	/** type of the element */
	rpmElementType type;

	/** index of the element in the set */
	int index;

	/** count of element in the set */
	int count;

	/** the transaction element */
	rpmte te;

	/** the recorded files of the transation element */
	rpmfiles files;

} record_t;

/** head of the record list */
static record_t *records;

static size_t put_str(char *buffer, size_t size, size_t offset, const char *str, size_t length)
{
	if (offset < size)
		memcpy(&buffer[offset], str, length <= size - offset ? length : size - offset);
	return offset + length;
}

static size_t put_nl(char *buffer, size_t size, size_t offset)
{
	return put_str(buffer, size, offset, "\n", 1);
}

static size_t put_sp(char *buffer, size_t size, size_t offset)
{
	return put_str(buffer, size, offset, " ", 1);
}

static size_t put_str_nl(char *buffer, size_t size, size_t offset, const char *str, size_t length)
{
	offset = put_str(buffer, size, offset, str, length);
	return put_nl(buffer, size, offset);
}

static size_t put_strz(char *buffer, size_t size, size_t offset, const char *str)
{
	return put_str(buffer, size, offset, str, strlen(str));
}

static size_t put_strz_nl(char *buffer, size_t size, size_t offset, const char *str)
{
	return put_str_nl(buffer, size, offset, str, strlen(str));
}

static size_t put_key_val_nl(char *buffer, size_t size, size_t offset, const char *key, const char *val)
{
	offset = put_strz(buffer, size, offset, key);
	offset = put_sp(buffer, size, offset);
	return put_strz_nl(buffer, size, offset, val);
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

/**
 * @brief Makes the message's buffer
 *
 * @param buffer the buffer that receive the message (can be NULL if size <= offset)
 * @param size size of the buffer
 * @param offset offset for starting write
 * @param record the record describing the transaction
 * @param operation the operation of the message
 * @return offset of the byte after the end of the buffer
 */
static size_t make_message(char *buffer, size_t size, size_t offset, record_t *record, const char *operation)
{
	rpmfi fi;
	char scratch[ITOALEN];
	const char *filename;
	const char *rootdir = rpmtsRootDir(record->ts);
	const char *name = rpmteN(record->te);
	const char *transid = getenv(AFMPKG_ENVVAR_TRANSID);
	const char *redpackid = getenv(AFMPKG_ENVVAR_REDPAKID);

	offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_BEGIN, operation);
	offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_PACKAGE, name);
	offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_INDEX, itoa(record->index, scratch));
	offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_COUNT, itoa(record->count, scratch));
	if (rootdir)
		offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_ROOT, rootdir);
	if (transid)
		offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_TRANSID, transid);
	if (redpackid)
		offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_REDPAKID, redpackid);
	fi = rpmfilesIter(record->files, RPMFI_ITER_FWD);
	while (rpmfiNext(fi) >= 0) {
		filename = rpmfiFN(fi);
		offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_FILE, filename);
	}
	rpmfiFree(fi);
	offset = put_key_val_nl(buffer, size, offset, AFMPKG_KEY_END, operation);
	return offset;
}

/** perform the given operation if the record is of the given type */
static int perform(record_t *record, rpmElementType type, const char *operation, rpmRC *prc)
{
	char *message;
	size_t length;
	int rc;

	/* check the type */
	if (record->type != type)
		return 0; /* don't drop */

	/* compute size of the message and allocates it */
	length = make_message(NULL, 0, 0, record, operation);
	message = malloc(length + 1);
	if (message == NULL) {
		rpmlog(RPMLOG_ERR, "malloc failed");
		*prc = RPMRC_FAIL;
	}
	else {
		/* make the message in the fresh buffer */
		make_message(message, length, 0, record, operation);
		message[length] = 0;
		/* send the message to the framework */
		rc = dial_framework(message, length, NULL);
		free(message);
		if (rc <= 0)
			*prc = RPMRC_FAIL;
	}
	return 1; /* done, drop the action */
}

/** callback for performing remove actions */
static int perform_remove(record_t *record, void *closure)
{
	return perform(record, TR_REMOVED, AFMPKG_OPERATION_REMOVE, (rpmRC*)closure);
}

/** callback for performing add actions */
static int perform_add(record_t *record, void *closure)
{
	return perform(record, TR_ADDED, AFMPKG_OPERATION_ADD, (rpmRC*)closure);
}

/** callback for performing check remove actions */
static int perform_check_remove(record_t *record, void *closure)
{
	return perform(record, TR_REMOVED, AFMPKG_OPERATION_CHECK_REMOVE, (rpmRC*)closure);
}

/** callback for performing check add actions */
static int perform_check_add(record_t *record, void *closure)
{
	return perform(record, TR_ADDED, AFMPKG_OPERATION_CHECK_ADD, (rpmRC*)closure);
}

/** apply the function to each record of the given set */
static void for_each_record(rpmts ts, int (*fun)(record_t*, void *), void *closure)
{
	int drop;
	record_t *it, **prv;

	/* iterate over all records */
	prv = &records;
	while((it = *prv) != NULL) {
		/* apply function and compute drop */
		if (it->ts != ts)
			/* not that set, don't drop */
			drop = 0;
		else if (fun)
			/* that set, fun says if droping or not */
			drop = fun(it, closure);
		else
			/* drop all requested */
			drop = 1;
		/* drop or not the current record */
		if (!drop)
			/* not dropping */
			prv = &it->next;
		else {
			/* dropping */
			*prv = it->next;
			rpmfilesFree(it->files);
			free(it);
		}
	}
}

/** callback for setting the count of actions */
static int number_count(record_t *record, void *closure)
{
	int *pcount = closure;
	record->count = *pcount;
	return 0;
}

/** callback for setting the index of remove actions */
static int number_removes(record_t *record, void *closure)
{
	int *pidx = closure;
	if (record->type == TR_REMOVED)
		record->index = ++*pidx;
	return 0;
}

/** callback for setting the index of add actions */
static int number_adds(record_t *record, void *closure)
{
	int *pidx = closure;
	if (record->type == TR_ADDED)
		record->index = ++*pidx;
	return 0;
}

rpmRC tsm_pre_cb(rpmPlugin plugin, rpmts ts)
{
	record_t *it;
	rpmfiles files;
	rpmte te;
	rpmElementType type;
	int idx, count;
	int eleidx, elecnt;
	rpmRC rc = RPMRC_OK;

	dump_ts(ts, "PRE");

	/* allocate records and count items */
	count = rpmtsNElements(ts);
	for(idx = eleidx = elecnt = 0 ; idx < count ; idx++) {
		te = rpmtsElement(ts, idx);
		type = rpmteType(te);
		switch (type) {
		case TR_ADDED:
		case TR_REMOVED:
			files = rpmteFiles(te);
			it = malloc(sizeof *it);
			if (it == NULL) {
				rpmlog(RPMLOG_ERR, "malloc failed");
				for_each_record(ts, NULL, NULL);
				return RPMRC_FAIL;
			}
			it->ts = ts;
			it->te = te;
			it->files = files;
			it->type = type;
			it->next = records;
			records = it;
			elecnt++;
			break;
		default:
			break;
		}
	}

	/* number the records */
	for_each_record(ts, number_count, &elecnt);
	for_each_record(ts, number_removes, &eleidx);
	for_each_record(ts, number_adds, &eleidx);

	/* execute the removes */
	switch (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST | RPMTRANS_FLAG_NOPREUN)) {
	case 0:
		for_each_record(ts, perform_remove, &rc);
	case RPMTRANS_FLAG_NOPREUN:
		/* nothing */
		break;
	case RPMTRANS_FLAG_TEST | RPMTRANS_FLAG_NOPREUN:
	case RPMTRANS_FLAG_TEST:
		for_each_record(ts, perform_check_remove, &rc);
		break;
	}

	return /* rc */ RPMRC_OK; /* never fail at the moment */
}

static rpmRC tsm_post_cb(rpmPlugin plugin, rpmts ts, int res)
{
	rpmRC rc = RPMRC_OK;

	dump_ts(ts, "POST");

	/* execute the adds */
	if (res == RPMRC_OK) {
		switch (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST | RPMTRANS_FLAG_NOPOST)) {
		case 0:
			for_each_record(ts, perform_add, &rc);
			break;
		case RPMTRANS_FLAG_NOPOST:
			/* nothing */
			break;
		case RPMTRANS_FLAG_TEST | RPMTRANS_FLAG_NOPOST:
		case RPMTRANS_FLAG_TEST:
			for_each_record(ts, perform_check_add, &rc);
			break;
		}
	}

	/* ensure clean */
	for_each_record(ts, NULL, NULL);

	return /* rc */ RPMRC_OK; /* never fail at the moment */
}

struct rpmPluginHooks_s redpesk_hooks = {
	.tsm_pre = tsm_pre_cb,
	.tsm_post = tsm_post_cb
};


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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-socket.h>

#include "afmpkg-request.h"
#include "afmpkg-common.h"

#if WITH_LEGACY_AFMPKG
#include "afmpkg-legacy.h"
#define afmpkg_install  afmpkg_legacy_install
#define afmpkg_uninstall  afmpkg_legacy_uninstall
#else
#include "afmpkg-std.h"
#define afmpkg_install  afmpkg_std_install
#define afmpkg_uninstall  afmpkg_std_uninstall
#endif

/**
 * @brief retention time in second for data of transactions
 */
#define RETENTION_SECONDS 3600 /* one hour */

/**
 * @brief structure for data of transactions
 */
struct transaction
{
	/** link to the next */
	struct transaction *next;

	/** expiration time */
	time_t expire;

	/** count of requests for the transaction */
	unsigned count;

	/** count of requests successful */
	unsigned success;

	/** count of requests failed */
	unsigned fail;

	/** identifier of the transaction */
	char id[];
};

/**
 * @brief mutex protecting accesses to 'all_transactions'
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief head of the list of pending transactions
 */
static struct transaction *all_transactions = NULL;

/**
 * @brief remove expired transactions
 *
 * The mutex must be taken.
 */
static void cleanup_transactions()
{
	struct transaction *trans, **previous;
	time_t now = time(NULL);

	/* search the transaction */
	for (previous = &all_transactions ; (trans = *previous) != NULL ;) {
		if (trans->expire > now)
			previous = &trans->next;
		else {
			/* drop expired transactions */
			*previous = trans->next;
			free(trans);
		}
	}
}

/**
 * @brief Get the transaction object of the given identifier,
 * creating it if needed.
 *
 * The mutex must be taken.
 *
 * @param transid the identifier
 * @param count if not zero, create the transaction object
 * @return struct transaction*
 */
static struct transaction *get_transaction(const char *transid, unsigned count)
{
	struct transaction *trans, *result;

	/* cleaup transactions */
	cleanup_transactions();

	/* search the transaction */
	result = all_transactions;
	while (result != NULL && strcmp(transid, result->id) != 0)
		result = result->next;

	/* create the transaction */
	if (result == NULL && count != 0) {
		result = malloc(sizeof *trans + 1 + strlen(transid));
		if (result != NULL) {
			result->expire = time(NULL) + RETENTION_SECONDS;
			result->count = count;
			result->success = 0;
			result->fail = 0;
			strcpy(result->id, transid);
			result->next = all_transactions;
			all_transactions = result;
		}
	}

	return result;
}

/**
 * @brief remove the transaction from the list and free its memory
 *
 * The mutex must be taken.
 *
 * @param trans the transaction to drop
 */
static void put_transaction(struct transaction *trans)
{
	struct transaction **previous;
	previous = &all_transactions;
	while (*previous != NULL && *previous != trans)
		previous = &(*previous)->next;
	if (*previous != NULL) {
		*previous = trans->next;
		free(trans);
	}
}

/**
 * @brief init a request structure
 *
 * @param req the request to init
 *
 * @return 0 on success or else a negative code
 */
int afmpkg_request_init(afmpkg_request_t *req)
{
	int rc;
	req->state = Request_Pending;
	req->kind = Request_Unset;
	req->scode = 0;
	req->index = 0;
	req->count = 0;
	req->transid = NULL;
	req->scratch = NULL;
	req->msg = NULL;
	req->apkg.package = NULL;
	req->apkg.root = NULL;
	req->apkg.redpakid = NULL;
	req->apkg.redpak_auto = NULL;
	rc = path_entry_create_root(&req->apkg.files);
	return rc;
}

/**
 * @brief set the status of the request
 *
 * @param req the request to set
 * @param scode the code to set
 * @param msg an associated message
 *
 * @return the value scode
 */
int afmpkg_request_error(afmpkg_request_t *req, int scode, const char *msg)
{
	if (req->scode == 0) {
		req->state = Request_Error;
		req->scode = scode;
		req->msg = msg;
	}
	return scode;
}

/**
 * @brief deinit a request structure, freeing its memory
 *
 * @param req the request to init
 */
void afmpkg_request_deinit(afmpkg_request_t *req)
{
	free(req->transid);
	free(req->scratch);
	free(req->apkg.package);
	free(req->apkg.root);
	free(req->apkg.redpakid);
	path_entry_destroy(req->apkg.files);
}

static
void dump(FILE *file, const char *format, ...)
{
	va_list vl;
	va_start(vl, format);
	if (file != NULL)
		vfprintf(file, format, vl);
	else
		rp_vverbose(rp_Log_Level_Info, NULL, 0, NULL, format, vl);
	va_end(vl);
}

static
int dump_one_file(void *closure, path_entry_t *entry, const char *path, size_t length) {
	dump((FILE *)closure, "    %s\n", path);
	return 0;
}

/**
 * @brief helper to print a request
 *
 * @param req the request to be printed
 * @param file output file
 */
void afmpkg_request_dump(afmpkg_request_t *req, FILE *file)
{
	static const char *snames[] = {
		[Request_Pending] = "pending",
		[Request_Ready] = "ready",
		[Request_Ok] = "ok",
		[Request_Error] = "error"
	};
	static const char *knames[] = {
		[Request_Unset] = "?unset?",
		[Request_Add_Package] = AFMPKG_OPERATION_ADD,
		[Request_Remove_Package] = AFMPKG_OPERATION_REMOVE,
		[Request_Check_Add_Package] = AFMPKG_OPERATION_CHECK_ADD,
		[Request_Check_Remove_Package] = AFMPKG_OPERATION_CHECK_REMOVE,
		[Request_Get_Status] = "status"
	};

	file = file == NULL ? stderr : file;
	dump(file, "BEGIN\n");
	dump(file, "  state     %s\n", snames[req->state]);
	dump(file, "  kind      %s\n", knames[req->kind]);
	dump(file, "  scode     %d\n", req->scode);
	dump(file, "  order     %u/%u\n", req->index, req->count);
	dump(file, "  transid   %s\n", req->transid ?: "");
	dump(file, "  package   %s\n", req->apkg.package ?: "");
	dump(file, "  root      %s\n", req->apkg.root ?: "");
	dump(file, "  redpakid  %s\n", req->apkg.redpakid ?: "");
	dump(file, "  files:\n");

	path_entry_for_each(PATH_ENTRY_FORALL_ONLY_ADDED, req->apkg.files, dump_one_file, file);

	dump(file, "END\n\n");
}

/**
 * @brief process a request
 *
 * @param req the request to be processed
 * @return 0 on success or a negative error code
 */
int afmpkg_request_process(afmpkg_request_t *req)
{
	struct transaction *trans;
	int rc = 0;

	if (rp_verbose_wants(rp_Log_Level_Info))
		afmpkg_request_dump(req, NULL);

	switch(req->kind) {
	default:
	case Request_Unset:
		/* invalid request */
		rc = afmpkg_request_error(req, -EINVAL, "invalid state");
		break;

	case Request_Add_Package:
	case Request_Remove_Package:
		/* process the request */
		if (rc == 0) {
			if (req->kind == Request_Add_Package)
				rc = afmpkg_install(&req->apkg);
			else
				rc = afmpkg_uninstall(&req->apkg);
			if (rc < 0)
				afmpkg_request_error(req, rc, req->kind == Request_Add_Package ? "can't install" : "can't uninstall");
		}
		/* record status for transaction */
		if (req->transid != NULL) {
			pthread_mutex_lock(&mutex);
			trans = get_transaction(req->transid, req->count);
			if (trans == NULL)
				rc = afmpkg_request_error(req, -ENOMEM, "out of memory");
			else if (rc >= 0)
				trans->success++;
			else
				trans->fail--;
			pthread_mutex_unlock(&mutex);
		}
		break;

	case Request_Check_Add_Package:
	case Request_Check_Remove_Package:
		RP_WARNING("Check operation isn't implemented");
		break;

	case Request_Get_Status:
		/* request for status of a transaction */
		if (req->transid == NULL)
			rc = afmpkg_request_error(req, -EINVAL, "invalid state");
		else {
			pthread_mutex_lock(&mutex);
			trans = get_transaction(req->transid, 0);
			if (trans == NULL)
				rc = afmpkg_request_error(req, -ENOMEM, "out of memory");
			else {
				rc = asprintf(&req->scratch, "%d %d %d", trans->count, trans->success, trans->fail);
				put_transaction(trans);
				if (rc < 0)
					rc = afmpkg_request_error(req, -errno, "out of memory");
				else
					req->msg = req->scratch;
			}
			pthread_mutex_unlock(&mutex);
		}
		break;
	}
	return rc;
}

/**
 * @brief Get the operation kind object
 *
 * @param string string value of the operation
 * @return the kind of the operation or Request_Unset if not recognized
 */
static afmpkg_request_kind_t get_operation_kind(const char *string)
{
	if (strcmp(string, AFMPKG_OPERATION_ADD) == 0)
		return Request_Add_Package;
	if (strcmp(string, AFMPKG_OPERATION_REMOVE) == 0)
		return Request_Remove_Package;
	if (strcmp(string, AFMPKG_OPERATION_CHECK_ADD) == 0)
		return Request_Check_Add_Package;
	if (strcmp(string, AFMPKG_OPERATION_CHECK_REMOVE) == 0)
		return Request_Check_Remove_Package;
	return Request_Unset;
}

/**
 * @brief process a line of request
 *
 * @param req the request to fill
 * @param line the line to process (must be zero terminated)
 * @param length length of the line
 * @return 0 on success or a negative error code
 */
int afmpkg_request_add_line(afmpkg_request_t *req, const char *line, size_t length)
{
	char *str;
	long val;
	int rc;

#define IF(key) \
		if (length >= sizeof(AFMPKG_KEY_##key) \
		 && memcmp(line, AFMPKG_KEY_##key, sizeof(AFMPKG_KEY_##key)-1) == 0 \
		 && line[sizeof(AFMPKG_KEY_##key)-1] == ' ') { \
			line += sizeof(AFMPKG_KEY_##key); \
			length -= sizeof(AFMPKG_KEY_##key);
#define ENDIF }
#define ELSE  }else{
#define ELSEIF(key) }else IF(key)

	/* should be pending */
	if (req->state != Request_Pending)
		return afmpkg_request_error(req, -1000, "line after end");

	IF(BEGIN)
		/* BEGIN [ADD|REMOVE] */
		if (req->kind != Request_Unset)
			return afmpkg_request_error(req, -1001, "unexpected BEGIN");
		req->kind = get_operation_kind(line);
		if (req->kind == Request_Unset)
			return afmpkg_request_error(req, -1002, "invalid BEGIN");

	ELSEIF(COUNT)
		/* COUNT VALUE */
		if (req->count != 0 || req->kind == Request_Unset)
			return afmpkg_request_error(req, -1003, "unexpected COUNT");
		errno = 0;
		val = strtol(line, &str, 10);
		if (*str)
			return afmpkg_request_error(req, -1004, "invalid COUNT");
		if (val < 1 || val > UINT_MAX || (val == LONG_MAX && errno == ERANGE))
			return afmpkg_request_error(req, -1005, "COUNT out of range");
		if (req->index != 0 && (unsigned)val < req->index)
			return afmpkg_request_error(req, -1006, "COUNT out of INDEX");
		req->count = (unsigned)val;

	ELSEIF(END)
		/* END [ADD|REMOVE] */
		if (req->kind != get_operation_kind(line))
			return afmpkg_request_error(req, -1008, "invalid END");
		req->state = Request_Ready;

	ELSEIF(FILE)
		/* FILE PATH */
		if (req->kind == Request_Unset)
			return afmpkg_request_error(req, -1009, "unexpected FILE");
		rc = path_entry_add_length(req->apkg.files, NULL, line, length);
		if (rc < 0)
			return afmpkg_request_error(req, -1010, "can't add FILE");

	ELSEIF(INDEX)
		/* INDEX VALUE */
		if (req->index != 0 || req->kind == Request_Unset)
			return afmpkg_request_error(req, -1011, "unexpected INDEX");
		errno = 0;
		val = strtol(line, &str, 10);
		if (*str)
			return afmpkg_request_error(req, -1012, "invalid INDEX");
		if (val < 1 || val > UINT_MAX || (val == LONG_MAX && errno == ERANGE))
			return afmpkg_request_error(req, -1013, "INDEX out of range");
		if (req->count != 0 && (unsigned)val > req->count)
			return afmpkg_request_error(req, -1014, "INDEX out of COUNT");
		req->index = (unsigned)val;

	ELSEIF(PACKAGE)
		/* PACKAGE NAME */
		if (req->apkg.package != NULL || req->kind == Request_Unset)
			return afmpkg_request_error(req, -1015, "unexpected PACKAGE");
		req->apkg.package = strdup(line);
		if (req->apkg.package == NULL)
			return afmpkg_request_error(req, -1016, "out of memory");

	ELSEIF(REDPAKID)
		/* REDPAKID REDPAKID */
		if (req->apkg.redpakid != NULL || req->kind == Request_Unset)
			return afmpkg_request_error(req, -1017, "unexpected REDPAKID");
		req->apkg.redpakid = strdup(line);
		if (req->apkg.redpakid == NULL)
			return afmpkg_request_error(req, -1016, "out of memory");

	ELSEIF(ROOT)
		/* ROOT NAME */
		if (req->apkg.root != NULL || req->kind == Request_Unset)
			return afmpkg_request_error(req, -1018, "unexpected ROOT");
		req->apkg.root = strdup(line);
		if (req->apkg.root == NULL)
			return afmpkg_request_error(req, -1016, "out of memory");

	ELSEIF(TRANSID)
		/* TRANSID TRANSID */
		if (req->transid != NULL || req->kind == Request_Unset)
			return afmpkg_request_error(req, -1019, "unexpected TRANSID");
		req->transid = strdup(line);
		if (req->transid == NULL)
			return afmpkg_request_error(req, -1016, "out of memory");

	ELSEIF(STATUS)
		/* STATUS TRANSID */
		if (req->kind != Request_Unset || req->transid != NULL)
			return afmpkg_request_error(req, -1020, "unexpected STATUS");
		req->transid = strdup(line);
		if (req->transid == NULL)
			return afmpkg_request_error(req, -1016, "out of memory");
		req->kind = Request_Get_Status;
		req->state = Request_Ready;

	ELSE
		return afmpkg_request_error(req, -1021, "bad line");

	ENDIF
	return 0;

#undef IF
#undef ELSE
#undef ELSEIF
#undef ENDIF
}

/**
 * @brief get the reply line of request
 *
 * @param req the request
 * @param line the line to set
 * @param length length of the line
 * @return the length of the line
 */
size_t afmpkg_request_make_reply_line(afmpkg_request_t *req, char *line, size_t length)
{
	const char *tag;
	size_t idx = 0;

	/* make the head */
	tag = req->scode >= 0 ? AFMPKG_KEY_OK : AFMPKG_KEY_ERROR;
	while (*tag) {
		if (idx < length)
			line[idx] = *tag;
		idx++;
		tag++;
	}

	/* add message */
	tag = req->msg;
	if (tag != NULL && *tag) {
		if (idx < length)
			line[idx] = ' ';
		idx++;
		while (*tag) {
			if (idx < length)
				line[idx] = *tag;
			idx++;
			tag++;
		}
	}

	/* ends the line */
	if (idx < length)
		line[idx] = '\n';
	idx++;
	if (idx < length)
		line[idx] = 0;
	return idx;
}

/**
 * @brief check if stopping is possible
 *
 * @return 0 if transactions are pending or a non zero value when stop is possible
 */
int afmpkg_request_can_stop()
{
	int result;

	pthread_mutex_lock(&mutex);
	cleanup_transactions();
	result = all_transactions == NULL;
	pthread_mutex_unlock(&mutex);
	return result;
}


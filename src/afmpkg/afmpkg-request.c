/*
 * Copyright (C) 2018-2023 IoT.bzh Company
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

#include "afmpkg-common.h"
#include "afmpkg.h"
#include "afmpkg-request.h"

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
 * @brief remove the transaction from the list and free its memory2a1ced824fcf36f3dfe676129e3d4b76317cb751
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
	req->kind = Request_Unset;
	req->ended = 0;
	req->index = 0;
	req->count = 0;
	req->transid = NULL;
	req->reply = NULL;
	req->apkg.package = NULL;
	req->apkg.root = NULL;
	req->apkg.redpakid = NULL;
	rc = path_entry_create_root(&req->apkg.files);
	return rc;
}

/**
 * @brief deinit a request structure, freeing its memory
 *
 * @param req the request to init
 */
void afmpkg_request_deinit(afmpkg_request_t *req)
{
	free(req->transid);
	free(req->reply);
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
static void dump_request(afmpkg_request_t *req, FILE *file)
{
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
	dump(file, "  kind      %s\n", knames[req->kind]);
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
		dump_request(req, NULL);

	switch(req->kind) {
	default:
	case Request_Unset:
		/* invalid request */
		rc = -EINVAL;
		break;

	case Request_Add_Package:
	case Request_Remove_Package:
		/* process the request */
		if (rc == 0) {
			if (req->kind == Request_Add_Package)
				rc = afmpkg_install(&req->apkg);
			else
				rc = afmpkg_uninstall(&req->apkg);
		}
		/* record status for transaction */
		if (req->transid != NULL) {
			pthread_mutex_lock(&mutex);
			trans = get_transaction(req->transid, req->count);
			if (trans == NULL)
				rc = -ENOMEM;
			else if (rc >= 0)
				trans->success++;
			else
				trans->fail--;
			pthread_mutex_unlock(&mutex);
		}
		break;

	case Request_Check_Add_Package:
	case Request_Check_Remove_Package:
		break;

	case Request_Get_Status:
		/* request for status of a transaction */
		if (req->transid == NULL)
			rc = -EINVAL;
		else {
			pthread_mutex_lock(&mutex);
			trans = get_transaction(req->transid, 0);
			if (trans == NULL)
				rc = -ENOMEM;
			else {
				rc = asprintf(&req->reply, "%d %d %d", trans->count, trans->success, trans->fail);
				put_transaction(trans);
				rc = rc < 0 ? -errno : 0;
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

	/* should not be ended */
	if (req->ended != 0)
		return -1000;

	IF(BEGIN)
		/* BEGIN [ADD|REMOVE] */
		if (req->kind != Request_Unset)
			return -1001;
		req->kind = get_operation_kind(line);
		if (req->kind == Request_Unset)
			return -1002;

	ELSEIF(COUNT)
		/* COUNT VALUE */
		if (req->count != 0 || req->kind == Request_Unset)
			return -1003;
		errno = 0;
		val = strtol(line, &str, 10);
		if (*str)
			return -1004;
		if (val < 1 || val > UINT_MAX || (val == LONG_MAX && errno == ERANGE))
			return -1005;
		if (req->index != 0 && (unsigned)val < req->index)
			return -1006;
		req->count = (unsigned)val;

	ELSEIF(END)
		/* END [ADD|REMOVE] */
		if (req->kind != get_operation_kind(line))
			return -1008;
		req->ended = 1;

	ELSEIF(FILE)
		/* FILE PATH */
		if (req->kind == Request_Unset)
			return -1009;
		rc = path_entry_add_length(req->apkg.files, NULL, line, length);
		if (rc < 0)
			return -1010;

	ELSEIF(INDEX)
		/* INDEX VALUE */
		if (req->index != 0 || req->kind == Request_Unset)
			return -1011;
		errno = 0;
		val = strtol(line, &str, 10);
		if (*str)
			return -1012;
		if (val < 1 || val > UINT_MAX || (val == LONG_MAX && errno == ERANGE))
			return -1013;
		if (req->count != 0 && (unsigned)val > req->count)
			return -1014;
		req->index = (unsigned)val;

	ELSEIF(PACKAGE)
		/* PACKAGE NAME */
		if (req->apkg.package != NULL || req->kind == Request_Unset)
			return -1015;
		req->apkg.package = strdup(line);
		if (req->apkg.package == NULL)
			return -1016;

	ELSEIF(REDPAKID)
		/* REDPAKID REDPAKID */
		if (req->apkg.redpakid != NULL || req->kind == Request_Unset)
			return -1017;
		req->apkg.redpakid = strdup(line);
		if (req->apkg.redpakid == NULL)
			return -1016;

	ELSEIF(ROOT)
		/* ROOT NAME */
		if (req->apkg.root != NULL || req->kind == Request_Unset)
			return -1018;
		req->apkg.root = strdup(line);
		if (req->apkg.root == NULL)
			return -1016;

	ELSEIF(TRANSID)
		/* TRANSID TRANSID */
		if (req->transid != NULL || req->kind == Request_Unset)
			return -1019;
		req->transid = strdup(line);
		if (req->transid == NULL)
			return -1016;

	ELSEIF(STATUS)
		/* STATUS TRANSID */
		if (req->kind != Request_Unset || req->transid != NULL)
			return -1020;
		req->transid = strdup(line);
		if (req->transid == NULL)
			return -1016;
		req->kind = Request_Get_Status;
		req->ended = 1;

	ELSE
		return -1021;

	ENDIF
	return 0;

#undef IF
#undef ELSE
#undef ELSEIF
#undef ENDIF
}


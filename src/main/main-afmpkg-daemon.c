/*
 * Copyright (C) 2018-2022 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 * Author: Arthur Guyader <arthur.guyader@iot.bzh>
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

#if !defined(NO_SEND_SIGUP_ALL)
#include "sighup-framework.h"
#endif

#include "afmpkg-common.h"
#include "afmpkg.h"

/**
 * @brief retention time in second for data of transactions
 */
#define RETENTION_SECONDS 3600 /* one hour */

/**
 * @brief iteration time in second for shuting down
 */
#define SHUTDOWN_CHECK_SECONDS 300 /* 5 minutes */

/**
 * @brief predefined address of the daemon's socket
 */
static const char *socket_uri = AFMPKG_SOCKET_ADDRESS;

/**
 * @brief kind of transaction
 */
enum kind
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
};

/**
 * @brief structure recording data of a request
 */
struct request
{
	/** the kind of the request */
	enum kind kind;

	/** end status of the request */
	int ended;

	/** index of the request in the transaction set */
	unsigned index;

	/** count of requests in the transaction set */
	unsigned count;

	/** identifier of the transaction */
	char *transid;

	/** argument of the reply */
	char *reply;

	/** root of files */
	path_entry_t *root;

	/** the packaging request */
	afmpkg_t apkg;
};

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
 * @brief count of living threads
 */
static int living_threads = 0;

/**
 * @brief is running for ever?
 */
static char run_forever = 0;

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
static int init_request(struct request *req)
{
	int rc;
	req->kind = Request_Unset;
	req->ended = 0;
	req->index = 0;
	req->count = 0;
	req->transid = NULL;
	req->reply = NULL;
	req->root = NULL;
	req->apkg.package = NULL;
	req->apkg.root = NULL;
	req->apkg.redpakid = NULL;
	rc = path_entry_create_root(&req->root);
	req->apkg.files = req->root;
	return rc;
}

/**
 * @brief deinit a request structure, freeing its memory
 *
 * @param req the request to init
 */
static void deinit_request(struct request *req)
{
	free(req->transid);
	free(req->reply);
	free(req->apkg.package);
	free(req->apkg.root);
	free(req->apkg.redpakid);
	path_entry_destroy(req->root);
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
static void dump_request(struct request *req, FILE *file)
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
static int process(struct request *req)
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
		/* init the pkg root if needed */
		if (req->apkg.root != NULL)
			rc = path_entry_root_prepend(req->root, &req->apkg.files, req->apkg.root);
		/* process the request */
		if (rc == 0) {
			if (req->kind == Request_Add_Package)
				rc = afmpkg_install(&req->apkg);
			else
				rc = afmpkg_uninstall(&req->apkg);
#if !defined(NO_SEND_SIGUP_ALL)
			if (rc >= 0)
				sighup_all();
#endif
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
static enum kind get_operation_kind(const char *string)
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
 * @param line the line to process
 * @param length length of the line
 * @return 0 on success or a negative error code
 */
static int receive_line(struct request *req, const char *line, size_t length)
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

/**
 * @brief receive the request
 *
 * @param sock the input socket
 * @param req the request for recording data
 * @return 0 on success or a negative error code
 */
static int receive(int sock, struct request *req)
{
	int rc;
	char buffer[32768];
	ssize_t sz;
	size_t length = 0, it, eol;

	for (rc = 0 ; rc >= 0 ; ) {
		/* blocking read of socket */
		do { sz = recv(sock, &buffer[length], sizeof buffer - length, 0); } while(sz == -1 && errno == EINTR);
		if (sz == -1) {
			rc = -errno;
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
						/* found a not empty line, process it */
						buffer[eol] = 0;
						rc = receive_line(req, &buffer[it], eol - it);
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
static void reply_length(int sock, int rc, const char *arg, size_t length)
{
	static char error[] = AFMPKG_KEY_ERROR;
	static char ok[] = AFMPKG_KEY_OK;
	static char nl[] = { '\n' };
	static char space[] = { ' ' };

	struct msghdr mh;
	struct iovec iov[4];
	ssize_t sz;

	/* raz the header */
	memset(&mh, 0, sizeof mh);
	mh.msg_iov = iov;

	/* make the message */
	if (rc >= 0) {
		iov[0].iov_base = ok;
		iov[0].iov_len = sizeof ok - 1;
	}
	else {
		iov[0].iov_base = error;
		iov[0].iov_len = sizeof error - 1;
	}
	if (length == 0)
		mh.msg_iovlen = 2;
	else {
		iov[1].iov_base = space;
		iov[1].iov_len = sizeof space;
		iov[2].iov_base = (void*)arg;
		iov[2].iov_len = length;
		mh.msg_iovlen = 4;
	}
	iov[mh.msg_iovlen - 1].iov_base = nl;
	iov[mh.msg_iovlen - 1].iov_len = sizeof nl;

	/* send the status */
	do { sz = sendmsg(sock, &mh, 0); } while(sz == -1 && errno == EINTR);
	shutdown(sock, SHUT_WR);
}

/**
 * @brief send a reply to the client
 *
 * @param sock socket for sending to client
 * @param rc the status
 * @param arg a string argument for the status
 */
static void reply(int sock, int rc, const char *arg)
{
	reply_length(sock, rc, arg, arg == NULL ? 0 : strlen(arg));
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
static int serve(int sock)
{
	int rc;
	struct request request; /* in stack request */

	/* init the request */
	rc = init_request(&request);

	/* receive the request */
	if (rc >= 0)
		rc = receive(sock, &request);

	/* process the request */
	if (rc >= 0)
		rc = process(&request);

	/* reply to the request */
	reply(sock, rc, request.reply);

	/* reset the memory */
	deinit_request(&request);
	return rc;
}

/**
 * @brief main thread for serving
 *
 * serving a client is run synchronously in a thread
 *
 * @param arg a casted integer handling the socket number
 */
static void *serve_thread(void *arg)
{
	int sock = (int)(intptr_t)arg;

	/* serve the connection */
	serve(sock);

	/* close the connection */
	close(sock);

	/* update liveness */
	pthread_mutex_lock(&mutex);
	living_threads = living_threads - 1;
	pthread_mutex_unlock(&mutex);
	return NULL;
}

/**
 * @brief basic run loop
 */
void launch_serve_thread(int socli)
{
	pthread_attr_t tat;
	pthread_t tid;
	int rc;

	/* for creating threads in detached state */
	pthread_attr_init(&tat);
	pthread_attr_setdetachstate(&tat, PTHREAD_CREATE_DETACHED);

	/* update liveness */
	pthread_mutex_lock(&mutex);
	living_threads = living_threads + 1;
	rc = pthread_create(&tid, &tat, serve_thread, (void*)(intptr_t)socli);
	if (rc != 0) {
		/* can't run thread */
		close(socli);
		living_threads = living_threads - 1;
	}
	pthread_mutex_unlock(&mutex);
}

/**
 * @brief check if stopping is possible
 */
int can_stop()
{
	int result;

	pthread_mutex_lock(&mutex);
	cleanup_transactions();
	result = living_threads == 0 && all_transactions == NULL;
	pthread_mutex_unlock(&mutex);
	return result;
}

/**
 * @brief create a socket listening to clients
 *
 * @return the opened socket or a negative error code
 */
static int listen_clients()
{
	return rp_socket_open_scheme(socket_uri, 1, "unix:");
}

/**
 * @brief prepare run loop
 */
void prepare_run()
{
	struct sigaction osa;

	/* for not dying on client disconnection */
	memset(&osa, 0, sizeof osa);
	osa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &osa, NULL);
	sigaction(SIGHUP, &osa, NULL);
}

/**
 * @brief basic run loop
 */
int run()
{
	struct pollfd pfd;
	int rc;

	/* create the listening socket */
	pfd.fd = listen_clients();
	if (pfd.fd < 0)
		return 1;

	/* loop on wait a client and serve it with a dedicated thread */
	pfd.events = POLLIN;
	for(;;) {
		rc = poll(&pfd, 1, SHUTDOWN_CHECK_SECONDS * 1000);
		if (rc == 1) {
			rc = accept(pfd.fd, NULL, NULL);
			if (rc >= 0)
				launch_serve_thread(rc);
		}
		if (rc < 0 && errno != EINTR)
			return 2;
		if (can_stop() && !run_forever)
			return 0;
	}
}

static const char appname[] = "afmpkg-daemon";

static void version()
{
	printf(
		"\n"
		"  %s  version="AFM_VERSION"\n"
		"\n"
		"  Copyright (C) 2015-2022 IoT.bzh Company\n"
		"  AFB comes with ABSOLUTELY NO WARRANTY.\n"
		"  Licence Apache 2\n"
		"\n",
		appname
	);
}

static void usage()
{
	printf(
		"usage: %s [options...]\n"
		"options:\n"
		"   -f, --forever     don't stop when unused\n"
		"   -h, --help        help\n"
		"   -q, --quiet       quiet\n"
		"   -s, --socket URI  socket URI\n"
		"   -v, --verbose     verbose\n"
		"   -V, --version     version\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "forever",     no_argument,       NULL, 'f' },
	{ "help",        no_argument,       NULL, 'h' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "socket",      required_argument, NULL, 's' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "version",     no_argument,       NULL, 'V' },
	{ NULL, 0, NULL, 0 }
};

/* install the widgets of the list */
int main(int ac, char **av)
{
	for (;;) {
		int i = getopt_long(ac, av, "hqvV", options, NULL);
		if (i < 0)
			break;
		switch (i) {
		case 'f':
			run_forever = 1;
			break;
		case 'h':
			usage();
			return 0;
		case 'q':
			rp_verbose_dec();
			break;
		case 's':
			socket_uri = optarg;
			break;
		case 'v':
			rp_verbose_inc();
			break;
		case 'V':
			version();
			return 0;
		default:
			RP_ERROR("unrecognized option");
			return 1;
		}
	}
	prepare_run();
	return run();
}

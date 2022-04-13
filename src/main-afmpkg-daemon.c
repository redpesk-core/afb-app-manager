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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <rp-utils/rp-verbose.h>

#include "afmpkg-common.h"
#include "afmpkg.h"

#define EXPIRATION 3600 /* one hour */

static const char framework_address[] = FRAMEWORK_SOCKET_ADDRESS;

enum kind
{
	Request_Unset,
	Request_Add_Package,
	Request_Remove_Package,
	Request_Get_Status
};

struct request
{
	enum kind begin;
	enum kind end;
	unsigned index;
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

struct transaction
{
	struct transaction *next;
	time_t expire;
	unsigned count;
	unsigned success;
	unsigned fail;
	char id[];
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static struct transaction *all_transactions;

static struct transaction *get_transaction(const char *transid, unsigned count)
{
	struct transaction *trans, **previous;
	time_t now = time(NULL);
	previous = &all_transactions;
	for (;;) {
		trans = *previous;
		if (trans == NULL) {
			if (count != 0) {
				trans = malloc(sizeof *trans + 1 + strlen(transid));
				if (trans != NULL) {
					trans->next = NULL;
					trans->expire = now + EXPIRATION;
					trans->count = count;
					trans->success = 0;
					trans->fail = 0;
					strcpy(trans->id, transid);
					*previous = trans;
				}
			}
			break;
		}
		if (trans->expire <= now) {
			*previous = trans->next;
			free(trans);
		}
		else if (strcmp(transid, trans->id) == 0)
			break;
		else
			previous = &trans->next;
	}
	return trans;
}

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
int dump_one_file(void *closure, path_entry_t *entry, const char *path, size_t length) {
	fprintf((FILE *)closure, "    %s\n", path);
	return 0;
}

static void dump_request(struct request *req, FILE *file)
{
	static const char *knames[] = {
		[Request_Unset] = "?unset?",
		[Request_Add_Package] = "add",
		[Request_Remove_Package] = "remove",
		[Request_Get_Status] = "status"
	};

	file = file == NULL ? stderr : file;
	fprintf(file, "BEGIN\n");
	fprintf(file, "  kind      %s\n", knames[req->begin]);
	fprintf(file, "  order     %u/%u\n", req->index, req->count);
	fprintf(file, "  transid   %s\n", req->transid ?: "");
	fprintf(file, "  package   %s\n", req->apkg.package ?: "");
	fprintf(file, "  root      %s\n", req->apkg.root ?: "");
	fprintf(file, "  redpakid  %s\n", req->apkg.redpakid ?: "");
	fprintf(file, "  files:\n");

	path_entry_for_each(PATH_ENTRY_FORALL_ONLY_ADDED, req->apkg.files, dump_one_file, file);

	fprintf(file, "END\n\n");
}

static int process(struct request *req)
{
	int rc = 0;

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
		}
		/* record status for transaction */
		if (req->transid != NULL) {
			pthread_mutex_lock(&mutex);
			trans = get_transaction(req->transid, req->count);
			if (trans == NULL)
				rc = -ENOMEM;
			pthread_mutex_unlock(&mutex);
			/* DO SOMETHING */
			pthread_mutex_lock(&mutex);
			trans = get_transaction(req->transid, 0);
			if (trans == NULL)
				rc = -ENOMEM;
			else if (rc >= 0)
				trans->success++;
			else
				trans->fail--;
			pthread_mutex_unlock(&mutex);
			break;
		case Request_Get_Status:
			pthread_mutex_lock(&mutex);
			trans = get_transaction(req->transid, req->count);
			if (trans == NULL)
				rc = -ENOMEM;
			else
				rc = asprintf(&req->reply, "%d %d %d", trans->count, trans->success, trans->fail);
			put_transaction(trans);
			pthread_mutex_unlock(&mutex);
			break;
		}
	}

	return 0;
}

/** receive a line of request */
static int receive_line(struct request *req, const char *line, size_t length)
{
	char *str;
	long val;
	int rc;
		
#define IF(pattern) \
		if (memcmp(line, #pattern, sizeof(#pattern)-1) == 0 \
		 && line[sizeof(#pattern)-1] == ' ') { \
			line += sizeof(#pattern); \
			length -= sizeof(#pattern);
#define ENDIF }
#define ELSE  }else{
#define ELSEIF(pattern) }else IF(pattern)

	if (req->end != Request_Unset)
		return -1000;

	IF(BEGIN)
		if (req->begin != Request_Unset)
			return -1001;
		if (strcmp(line, "ADD") == 0)
			req->begin = Request_Add_Package;
		else if (strcmp(line, "REMOVE") == 0)
			req->begin = Request_Remove_Package;
		else
			return -1002;

	ELSEIF(COUNT)
		if (req->count != 0 || req->begin == Request_Unset)
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
		if (strcmp(line, "ADD") == 0 || req->begin == Request_Unset)
			req->end = Request_Add_Package;
		else if (strcmp(line, "REMOVE") == 0)
			req->end = Request_Remove_Package;
		else
			return -1007;
		if (req->end != req->begin)
			return -1008;

	ELSEIF(FILE)
		if (req->begin == Request_Unset)
			return -1009;
		rc = path_entry_add_length(req->apkg.files, NULL, line, length);
		if (rc < 0)
			return -1010;
		
	ELSEIF(INDEX)
		if (req->index != 0 || req->begin == Request_Unset)
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
		if (req->transid != NULL || req->begin == Request_Unset)
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

/** receive the request */
static int receive(int sock, struct request *req)
{
	int rc;
	char buffer[32768];
	ssize_t sz;
	size_t length = 0, it, eol;

	for (;;) {
		/* blocking socket ensure sz == length */
		do { sz = recv(sock, &buffer[length], sizeof buffer - length, 0); } while(sz == -1 && errno == EINTR);
		if (sz == -1) {
			rc = -errno;
			break;
		}
		if (sz > 0) {
			/* extract the data */
			it = 0;
			length = (size_t) sz;
			for (;;) {
				for(eol = it ; eol < length && buffer[eol] != '\n' ; eol++);
				if (eol == length) {
					for(eol = it ; eol < length ; eol++)
						buffer[eol - it] = buffer[eol];
					length -= it;
					break;
				}
				if (eol != it) {
					buffer[eol] = 0;
					rc = receive_line(req, &buffer[it], eol - it);
					if (rc < 0)
						break;
				}
				it = eol + 1;
			}
		}
		else {
			/* TODO process the data */
			break;
		}
	}
	shutdown(sock, SHUT_RD);
	return rc;
}

static void reply(int sock, int rc, const char *arg)
{
	ssize_t sz;
	char buffer[1000];

	/* send the status */
	snprintf(buffer, sizeof buffer, "%s%s%s\n", rc >= 0 ? "OK" : "ERROR",
		arg == NULL ? "" : " ", arg == NULL ? "" : arg);
	buffer[sizeof buffer - 1] = 0;
	do { sz = send(sock, buffer, strlen(buffer), 0); } while(sz == -1 && errno == EINTR);
	shutdown(sock, SHUT_WR);
}

static int serve(int sock)
{
	int rc;
	struct request request;

	rc = init_request(&request);
	if (rc >= 0)
		rc = receive(sock, &request);
	if (rc >= 0)
		rc = process(&request);
	reply(sock, rc, request.reply);
	deinit_request(&request);
	close(sock);
	return rc;
}

static void *serve_thread(void *arg)
{
	serve((int)(intptr_t)arg);
	return NULL;
}


/** connect to the framework */
static int listen_clients()
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
		else if (adr.sun_path[0] != 0)
			unlink(adr.sun_path);
		rc = bind(sock, (struct sockaddr*)&adr, sizeof adr);
		if (rc < 0)
			rc = -errno;
		else {
			/* set it up */
			fcntl(sock, F_SETFD, FD_CLOEXEC);
			rc = listen(sock, 10);
			if (rc < 0)
				rc = -errno;
			else
				return sock;
		}
		close(sock);
	}
	return rc;
}

/**
 * @brief basic run loop
 */
int run()
{
	pthread_attr_t tat;
	pthread_t tid;
	struct sigaction osa;
	int rc, sock, socli;

	pthread_attr_init(&tat);
	pthread_attr_setdetachstate(&tat, PTHREAD_CREATE_DETACHED);
	sock = listen_clients();
	if (sock < 0)
		return 1;

	memset(&osa, 0, sizeof osa);
	osa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &osa, NULL);
	for(;;) {
		rc = accept(sock, NULL, NULL);
		if (rc < 0) {
			if (errno != EINTR)
				return 2;
		}
		else {
			socli = rc;
			rc = pthread_create(&tid, &tat, serve_thread, (void*)(intptr_t)socli);
			if (rc != 0)
				close(socli);
		}
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
		"usage: %s [-q] [-v] [-V]\n"
		"\n"
		"   -q            quiet\n"
		"   -v            verbose\n"
		"   -V            version\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "help",        no_argument,       NULL, 'h' },
	{ "quiet",       no_argument,       NULL, 'q' },
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
		case 'h':
			usage();
			return 0;
		case 'q':
			rp_verbose_dec();
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
	return run();
}


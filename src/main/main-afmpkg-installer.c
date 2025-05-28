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

#if !NO_SEND_SIGHUP_ALL
#include "sighup-framework.h"
#endif

#include "afmpkg-common.h"
#include "afmpkg-request.h"
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
 * @brief mutex protecting accesses to 'all_transactions'
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief count of living threads
 */
static int living_threads = 0;

/**
 * @brief is running for ever?
 */
static char run_forever = 0;

/**
 * @brief is strictly for root?
 */
static char strict = 0;

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
static int serve(int sock)
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
static void launch_serve_thread(int socli)
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
static int can_stop()
{
	int result;

	pthread_mutex_lock(&mutex);
	result = living_threads == 0;
	pthread_mutex_unlock(&mutex);
	return result && afmpkg_request_can_stop();
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
 * @brief check if client is root, otherwise, close it and report error
 *
 * @param fd socket file descriptor
 *
 * @return the fd socket if granted. Other wise, -1.
 */
static int check_strict(int fd)
{
	struct ucred cred;
	socklen_t len = (socklen_t)sizeof(cred);
	int rc = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len);
	if (rc < 0 || cred.uid != 0) {
		fprintf(stderr, "strict mode rejects uid %d\n", (int)cred.uid);
		close(fd);
		fd = -1;
	}
	return fd;
}

/**
 * @brief prepare run loop
 */
static void prepare_run()
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
static int run()
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
			if (rc >= 0 && strict)
				rc = check_strict(rc);
			if (rc >= 0)
				launch_serve_thread(rc);
		}
		if (rc < 0 && errno != EINTR)
			return 2;
		if (can_stop() && !run_forever)
			return 0;
	}
}

static const char appname[] = "afmpkg-installer";

static void version()
{
	printf(
		"\n"
		"  %s  version="AFM_VERSION"\n"
		"\n"
		"  Copyright (C) 2015-2025 IoT.bzh Company\n"
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
		"   -S, --strict      restrict to root client\n"
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
	{ "strict",      no_argument,       NULL, 'S' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "version",     no_argument,       NULL, 'V' },
	{ NULL, 0, NULL, 0 }
};

/* install the widgets of the list */
int main(int ac, char **av)
{
	for (;;) {
		int i = getopt_long(ac, av, "fhqsvV", options, NULL);
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
		case 'S':
			strict = 1;
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

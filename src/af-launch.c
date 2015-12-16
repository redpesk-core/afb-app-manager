/*
 Copyright 2015 IoT.bzh

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char **environ;

#include "verbose.h"
#include "af-launch.h"
#include "secmgr-wrap.h"

struct launchparam {
	int port;
	const char *secret;
};

static int launch_html(struct af_launch_desc *desc, struct launchparam *params);
static int launch_bin(struct af_launch_desc *desc, struct launchparam *params);
static int launch_qml(struct af_launch_desc *desc, struct launchparam *params);

static int launch_master(struct af_launch_desc *desc, struct launchparam *params, int fd, pid_t child);

static struct {
	const char *type;
	int (*launcher)(struct af_launch_desc *desc, struct launchparam *params);
}
known_launchers[] = {
	{ "text/html", launch_html },
	{ "application/x-executable", launch_bin },
	{ "application/octet-stream", launch_bin },
	{ "text/vnd.qt.qml", launch_qml }
};

static void mksecret(char buffer[9])
{
	snprintf(buffer, 9, "%08lX", (0xffffffff & random()));
}

static int mkport()
{
	static int port_ring = 12345;
	int port = port_ring;
	if (port < 12345 || port > 15432)
		port = 12345;
	port_ring = port + 1;
	return port;
}

int af_launch(struct af_launch_desc *desc, pid_t children[2])
{
	char datadir[PATH_MAX];
	int ikl, nkl, rc;
	char secret[9];
	int port;
	char message[10];
	int mpipe[2];
	int spipe[2];
	struct launchparam params;

	/* what launcher ? */
	ikl = 0;
	if (desc->type != NULL && *desc->type) {
		nkl = sizeof known_launchers / sizeof * known_launchers;
		while (ikl < nkl && strcmp(desc->type, known_launchers[ikl].type))
			ikl++;
		if (ikl == nkl) {
			ERROR("type %s not found!", desc->type);
			errno = ENOENT;
			return -1;
		}
	}

	/* prepare paths */
	rc = snprintf(datadir, sizeof datadir, "%s/%s", desc->home, desc->tag);
	if (rc < 0 || rc >= sizeof datadir) {
		ERROR("overflow for datadir");
		errno = EINVAL;
		return -1;
	}

	/* make the secret and port */
	mksecret(secret);
	port = mkport();

	params.port = port;
	params.secret = secret;

	/* prepare the pipes */
	rc = pipe2(mpipe, O_CLOEXEC);
	if (rc < 0) {
		ERROR("error while calling pipe2: %m");
		return -1;
	}
	rc = pipe2(spipe, O_CLOEXEC);
	if (rc < 0) {
		ERROR("error while calling pipe2: %m");
		close(spipe[0]);
		close(spipe[1]);
		return -1;
	}

	/* fork the master child */
	children[0] = fork();
	if (children[0] < 0) {
		ERROR("master fork failed: %m");
		close(mpipe[0]);
		close(mpipe[1]);
		close(spipe[0]);
		close(spipe[1]);
		return -1;
	}
	if (children[0]) {
		/********* in the parent process ************/
		close(mpipe[1]);
		close(spipe[0]);
		/* wait the ready signal (that transmit the slave pid) */
		rc = read(mpipe[0], &children[1], sizeof children[1]);
		if (rc  < 0) {
			ERROR("reading master pipe failed: %m");
			close(mpipe[0]);
			close(spipe[1]);
			return -1;
		}
		close(mpipe[0]);
		assert(rc == sizeof children[1]);
		/* start the child */
		rc = write(spipe[1], "start", 5);
		if (rc < 0) {
			ERROR("writing slave pipe failed: %m");
			close(spipe[1]);
			return -1;
		}
		close(spipe[1]);
		return 0;
	}

	/********* in the master child ************/
	close(mpipe[0]);
	close(spipe[1]);

	/* enter the process group */
	rc = setpgid(0, 0);
	if (rc) {
		ERROR("setpgid failed");
		_exit(1);
	}

	/* enter security mode */
	rc = secmgr_prepare_exec(desc->tag);
	if (rc < 0) {
		ERROR("call to secmgr_prepare_exec failed: %m");
		_exit(1);
	}

	/* enter the datadirectory */
	rc = mkdir(datadir, 0755);
	if (rc && errno != EEXIST) {
		ERROR("creation of datadir %s failed: %m", datadir);
		_exit(1);
	}
	rc = chdir(datadir);
	if (rc) {
		ERROR("can't enter the datadir %s: %m", datadir);
		_exit(1);
	}

	/* fork the slave child */
	children[1] = fork();
	if (children[1] < 0) {
		ERROR("slave fork failed: %m");
		_exit(1);
	}
	if (children[1] == 0) {
		/********* in the slave child ************/
		close(mpipe[0]);
		rc = read(spipe[0], message, sizeof message);
		if (rc < 0) {
			ERROR("reading slave pipe failed: %m");
			_exit(1);
		}
		rc = known_launchers[ikl].launcher(desc, &params);
		ERROR("slave launch failed: %m");
		_exit(1);
	}

	/********* still in the master child ************/
	close(spipe[1]);
	rc = launch_master(desc, &params, mpipe[1], children[1]);
	ERROR("master launch failed: %m");
	_exit(1);
}

static int launch_master(struct af_launch_desc *desc, struct launchparam *params, int fd, pid_t child)
{
	int rc;
	char *argv[6];
	argv[0] = "/bin/echo";
	(void)asprintf(&argv[1], "--alias=/icons:%s", FWK_ICON_DIR);
	(void)asprintf(&argv[2], "--port=%d", params->port);
	(void)asprintf(&argv[3], "--rootdir=%s", desc->path);
	(void)asprintf(&argv[4], "--token=%", desc->path);
	argv[5] = NULL;

	rc = write(fd, &child, sizeof child);
	if (rc < 0) {
		ERROR("can't write master pipe: %m");
		return -1;
	}

	rc = execve(argv[0], argv, environ);
	ERROR("failed to exec master %s: %m", argv[0]);
	return rc;
}

static int launch_html(struct af_launch_desc *desc, struct launchparam *params)
{
/*
	char *url = asprintf("http://localhost:%d/", params->port);
*/
	int rc;
	char *argv[3];
	argv[0] = "/usr/bin/chromium";
	(void)asprintf(&argv[1], "file://%s/%s", desc->path, desc->content);
	argv[2] = NULL;
	rc = execve(argv[0], argv, environ);
	ERROR("failed to exec slave %s: %m", argv[0]);
	return rc;
}

static int launch_bin(struct af_launch_desc *desc, struct launchparam *params)
{
	ERROR("unimplemented launch_bin");
	return -1;
}

static int launch_qml(struct af_launch_desc *desc, struct launchparam *params)
{
	ERROR("unimplemented launch_qml");
	return -1;
}



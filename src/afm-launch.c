/*
 Copyright 2015 IoT.bzh

 author: José Bollo <jose.bollo@iot.bzh>

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
#include "afm-launch.h"
#include "secmgr-wrap.h"

/*
%I icondir			FWK_ICON_DIR
%P port				params->port
%S secret			params->secret
%D datadir			params->datadir
%r rootdir			desc->path
%h homedir			desc->home
%t tag (smack label)		desc->tag
%a appid			desc->appid
%c content			desc->content
%m mime-type			desc->type
%n name				desc->name
%p plugins			desc->plugins
%W width			desc->width
%H height			desc->height
*/

static const char *args_for_afb_daemon[] = {
	"/usr/bin/afb-daemon",
	"--alias=/icons:%I",
	"--port=%P",
	"--rootdir=%D",
	"--token=%S",
	NULL
};

static const char *args_for_qmlviewer[] = {
	"/usr/bin/qt5/qmlscene",
	"-fullscreen",
	"-I",
	"%r",
	"-I",
	"%r/imports",
	"%r/%c",
	NULL
};

static const char *args_for_web_runtime[] = {
	"/usr/share/qt5/examples/webkitwidgets/browser/browser",
	"http://localhost:%P/%c?token=%S",
	NULL
};

static const char *args_for_binary[] = {
	"%r/%c",
	NULL
};

struct execdesc {
	const char *type;
	const char **master_args;
	const char **slave_args;
};

static struct execdesc known_launchers[] = {
	{ "text/html",                args_for_afb_daemon, args_for_web_runtime },
	{ "application/x-executable", args_for_binary,     NULL },
	{ "text/vnd.qt.qml",          args_for_qmlviewer,  NULL }
};

struct launchparam {
	int port;
	const char *secret;
	const char *datadir;
	const char **master_args;
	const char **slave_args;
};

static char **instantiate_arguments(const char **args, struct afm_launch_desc *desc, struct launchparam *params)
{
	const char **iter, *p, *v;
	char *data, **result, port[20], width[20], height[20], mini[3], c;
	int n, s;

	/* init */
	mini[0] = '%';
	mini[2] = 0;

	/* loop that either compute the size and build the result */
	data = NULL;
	n = s = 0;
	for (;;) {
		iter = args;
		n = 0;
		while (*iter) {
			p = *iter++;
			if (data)
				result[n] = data;
			n++;
			while((c = *p++) != 0) {
				if (c != '%') {
					if (data)
						*data++ = c;
					else
						s++;
				} else {
					c = *p++;
					switch (c) {
					case 'I': v = FWK_ICON_DIR; break;
					case 'P': if(!data) sprintf(port, "%d", params->port); v = port; break;
					case 'S': v = params->secret; break;
					case 'D': v = params->datadir; break;
					case 'r': v = desc->path; break;
					case 'h': v = desc->home; break;
					case 't': v = desc->tag; break;
					case 'a': v = desc->appid; break;
					case 'c': v = desc->content; break;
					case 'm': v = desc->type; break;
					case 'n': v = desc->name; break;
					case 'p': v = "" /*desc->plugins*/; break;
					case 'W': if(!data) sprintf(width, "%d", desc->width); v = width; break;
					case 'H': if(!data) sprintf(height, "%d", desc->height); v = height; break;
					case '%': c = 0;
					default: mini[1] = c; v = mini; break;
					}
					if (data)
						data = stpcpy(data, v);
					else
						s += strlen(v);
				}
			}
			if (data)
				*data++ = 0;
			else
				s++;
		}
		if (data) {
			result[n] = NULL;
			return result;
		}
		/* allocation */
		result = malloc((n+1)*sizeof(char*) + s);
		if (result == NULL) {
			errno = ENOMEM;
			return NULL;
		}
		data = (char*)(&result[n + 1]);
	}
}

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



static int launchexec1(struct afm_launch_desc *desc, pid_t children[2], struct launchparam *params)
{
	int rc;
	char **args;

	/* fork the master child */
	children[0] = fork();
	if (children[0] < 0) {
		ERROR("master fork failed: %m");
		return -1;
	}
	if (children[0]) {
		/********* in the parent process ************/
		return 0;
	}

	/********* in the master child ************/
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
	rc = mkdir(params->datadir, 0755);
	if (rc && errno != EEXIST) {
		ERROR("creation of datadir %s failed: %m", params->datadir);
		_exit(1);
	}
	rc = chdir(params->datadir);
	if (rc) {
		ERROR("can't enter the datadir %s: %m", params->datadir);
		_exit(1);
	}

	args = instantiate_arguments(params->master_args, desc, params);
	if (args == NULL) {
		ERROR("out of memory in master");
	}
	else {
		rc = execve(args[0], args, environ);
		ERROR("failed to exec master %s: %m", args[0]);
	}
	_exit(1);
}

static int launchexec2(struct afm_launch_desc *desc, pid_t children[2], struct launchparam *params)
{
	int rc;
	char message[10];
	int mpipe[2];
	int spipe[2];
	char **args;

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
		close(mpipe[0]);
		if (rc  <= 0) {
			ERROR("reading master pipe failed: %m");
			close(spipe[1]);
			return -1;
		}
		assert(rc == sizeof children[1]);
		/* start the child */
		rc = write(spipe[1], "start", 5);
		if (rc < 0) {
			ERROR("writing slave pipe failed: %m");
			close(spipe[1]);
			return -1;
		}
		assert(rc == 5);
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
	rc = mkdir(params->datadir, 0755);
	if (rc && errno != EEXIST) {
		ERROR("creation of datadir %s failed: %m", params->datadir);
		_exit(1);
	}
	rc = chdir(params->datadir);
	if (rc) {
		ERROR("can't enter the datadir %s: %m", params->datadir);
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
		if (rc <= 0) {
			ERROR("reading slave pipe failed: %m");
			_exit(1);
		}

		args = instantiate_arguments(params->slave_args, desc, params);
		if (args == NULL) {
			ERROR("out of memory in slave");
		}
		else {
			rc = execve(args[0], args, environ);
			ERROR("failed to exec slave %s: %m", args[0]);
		}
		_exit(1);
	}

	/********* still in the master child ************/
	close(spipe[1]);
	args = instantiate_arguments(params->master_args, desc, params);
	if (args == NULL) {
		ERROR("out of memory in master");
	}
	else {
		rc = write(mpipe[1], &children[1], sizeof children[1]);
		if (rc <= 0) {
			ERROR("can't write master pipe: %m");
		}
		else {
			close(mpipe[1]);
			rc = execve(args[0], args, environ);
			ERROR("failed to exec master %s: %m", args[0]);
		}
	}
	_exit(1);
}

int afm_launch(struct afm_launch_desc *desc, pid_t children[2])
{
	char datadir[PATH_MAX];
	int ikl, nkl, rc;
	char secret[9];
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
	params.port = mkport();
	params.secret = secret;
	params.datadir = datadir;
	params.master_args = known_launchers[ikl].master_args;
	params.slave_args = known_launchers[ikl].slave_args;

	return params.slave_args ? launchexec2(desc, children, &params) : launchexec1(desc, children, &params);
}


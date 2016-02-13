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
#include "afm-launch-mode.h"
#include "afm-launch.h"
#include "secmgr-wrap.h"

#define DEFAULT_TYPE "text/html"

struct type_list {
	struct type_list *next;
	char type[1];
};

struct desc_list {
	struct desc_list *next;
	enum afm_launch_mode mode;
	struct type_list *types;
	char **execs[2];
};

struct launchparam {
	int port;
	const char *secret;
	const char *datadir;
	const char **master;
	const char **slave;
};

struct confread {
	const char *filepath;
	FILE *file;
	int lineno;
	int index;
	int length;
	char buffer[4096];
};

struct desc_list *launchers = NULL;

static gid_t groupid = 0;

const char separators[] = " \t\n";

static void dump_launchers()
{
	int j, k;
	struct desc_list *desc;
	struct type_list *type;

	for (desc = launchers ; desc != NULL ; desc = desc->next) {
		printf("mode %s\n", name_of_launch_mode(desc->mode));
		for (type = desc->types ; type != NULL ; type = type->next)
			printf("%s\n", type->type);
		for ( j = 0 ; j < 2 ; j++)
			if (desc->execs[j] != NULL) {
				for (k = 0 ; desc->execs[j][k] != NULL ; k++)
					printf("  %s", desc->execs[j][k]);
				printf("\n");
			}
		printf("\n");
	}
}

static int next_token(struct confread *cread)
{
	cread->index += cread->length + strspn(&cread->buffer[cread->index + cread->length], separators);
	cread->length = strcspn(&cread->buffer[cread->index], separators);
	return cread->length;
}

static int read_line(struct confread *cread)
{
	while (fgets(cread->buffer, sizeof cread->buffer, cread->file) != NULL) {
		cread->lineno++;
		cread->index = strspn(cread->buffer, separators);
		if (cread->buffer[cread->index] && cread->buffer[cread->index] != '#') {
			cread->length = strcspn(&cread->buffer[cread->index], separators);
			assert(cread->length > 0);
			return cread->length;
		}
	}
	if (ferror(cread->file)) {
		ERROR("%s:%d: error while reading, %m", cread->filepath, cread->lineno);
		return -1;
	}
	return 0;
}

static char **read_vector(struct confread *cread)
{
	int index0, length0;
	char **vector, *args;
	int count, length;

	/* record origin */
	index0 = cread->index;
	length0 = cread->length;

	/* count */
	count = 0;
	length = 0;
	while(cread->length) {
		count++;
		length += cread->length;
		next_token(cread);
	}

	/* allocates */
	cread->index = index0;
	cread->length = length0;
	vector = malloc(length + count + (count + 1) * sizeof(char*));
	if (vector == NULL)
		return NULL;

	/* copies */
	args = (char*)(vector + count + 1);
	count = 0;
	while(cread->length) {
		vector[count++] = args;
		memcpy(args, &cread->buffer[cread->index], cread->length);
		args += cread->length;
		*args++ = 0;
		next_token(cread);
	}
	vector[count] = NULL;
	cread->index = index0;
	cread->length = length0;
	return vector;
}

static struct type_list *read_type(struct confread *cread)
{
	int index, length;
	struct type_list *result;

	/* record index and length */
	index = cread->index;
	length = cread->length;

	/* check no extra characters */
	if (next_token(cread)) {
		ERROR("%s:%d: extra characters found after type %.*s", cread->filepath, cread->lineno, length, &cread->buffer[index]);
		errno = EINVAL;
		return NULL;
	}

	/* allocate structure */
	result = malloc(sizeof(struct type_list) + length);
	if (result == NULL) {
		ERROR("%s:%d: out of memory", cread->filepath, cread->lineno);
		errno = ENOMEM;
		return NULL;
	}

	/* fill the structure */
	memcpy(result->type, &cread->buffer[index], length);
	result->type[length] = 0;
	return result;
}

static enum afm_launch_mode read_mode(struct confread *cread)
{
	int index, length;
	enum afm_launch_mode result;

	assert(cread->index == 0);
	assert(!strncmp(&cread->buffer[cread->index], "mode", 4));

	/* get the next token: the mode string */
	if (!next_token(cread)) {
		ERROR("%s:%d: no mode value set", cread->filepath, cread->lineno);
		errno = EINVAL;
		return invalid_launch_mode;
	}

	/* record index and length */
	index = cread->index;
	length = cread->length;

	/* check no extra characters */
	if (next_token(cread)) {
		ERROR("%s:%d: extra characters found after mode %.*s", cread->filepath, cread->lineno, length, &cread->buffer[index]);
		errno = EINVAL;
		return invalid_launch_mode;
	}

	/* get the mode */
	cread->buffer[index + length] = 0;
	result = launch_mode_of_string(&cread->buffer[index]);
	if (result == invalid_launch_mode) {
		ERROR("%s:%d: invalid mode value %s", cread->filepath, cread->lineno, &cread->buffer[index]);
		errno = EINVAL;
	}
	return result;
}

static void free_type_list(struct type_list *types)
{
	while (types != NULL) {
		struct type_list *next = types->next;
		free(types);
		types = next;
	}
}

static int read_launchers(struct confread *cread)
{
	int rc;
	struct type_list *types, *lt;
	struct desc_list *desc;
	enum afm_launch_mode mode;
	char **vector;

	/* reads the file */
	lt = NULL;
	types = NULL;
	desc = NULL;
	mode = invalid_launch_mode;
	rc = read_line(cread);
	while (rc > 0) {
		if (cread->index == 0) {
			if (cread->length == 4 && !memcmp(&cread->buffer[cread->index], "mode", 4)) {
				/* check if allowed */
				if (types != NULL) {
					ERROR("%s:%d: mode found before launch vector", cread->filepath, cread->lineno);
					errno = EINVAL;
					free_type_list(types);
					return -1;
				}

				/* read the mode */
				mode = read_mode(cread);
				if (mode == invalid_launch_mode)
					return -1;
			} else {
				if (mode == invalid_launch_mode) {
					ERROR("%s:%d: mode not found before type", cread->filepath, cread->lineno);
					errno = EINVAL;
					assert(types == NULL);
					return -1;
				}
				/* read a type */
				lt = read_type(cread);
				if (lt == NULL) {
					free_type_list(types);
					return -1;
				}
				lt->next = types;
				types = lt;
			}
			desc = NULL;
		} else if (types == NULL && desc == NULL) {
			if (lt == NULL)
				ERROR("%s:%d: untyped launch vector found", cread->filepath, cread->lineno);
			else
				ERROR("%s:%d: extra launch vector found (2 max)", cread->filepath, cread->lineno);
			errno = EINVAL;
			return -1;
		} else {
			vector = read_vector(cread);
			if (vector == NULL) {
				ERROR("%s:%d: out of memory", cread->filepath, cread->lineno);
				free_type_list(types);
				errno = ENOMEM;
				return -1;
			}
			if (types) {
				assert(desc == NULL);
				desc = malloc(sizeof * desc);
				if (desc == NULL) {
					ERROR("%s:%d: out of memory", cread->filepath, cread->lineno);
					free_type_list(types);
					errno = ENOMEM;
					return -1;
				}
				desc->next = launchers;
				desc->mode = mode;
				desc->types = types;
				desc->execs[0] = vector;
				desc->execs[1] = NULL;
				types = NULL;
				launchers = desc;
			} else {
				desc->execs[1] = vector;
				desc = NULL;
			}
		}
		rc = read_line(cread);
	}
	if (types != NULL) {
		ERROR("%s:%d: end of file found before launch vector", cread->filepath, cread->lineno);
		free_type_list(types);
		errno = EINVAL;
		return -1;
	}
	return rc;
}

static int read_configuration_file(const char *filepath)
{
	int rc;
	struct confread cread;

	/* opens the configuration file */
	cread.file = fopen(filepath, "r");
	if (cread.file == NULL) {
		/* error */
		ERROR("can't read file %s: %m", filepath);
		rc = -1;
	} else {
		/* reads it */
		cread.filepath = filepath;
		cread.lineno = 0;
		rc = read_launchers(&cread);
		fclose(cread.file);
	}
	return rc;
}

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
%% %
*/

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
	result = NULL;
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

	/* avoid set-gid effect */
	setresgid(groupid, groupid, groupid);

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

	args = instantiate_arguments(params->master, desc, params);
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

	/* avoid set-gid effect */
	setresgid(groupid, groupid, groupid);

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

		args = instantiate_arguments(params->slave, desc, params);
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
	args = instantiate_arguments(params->master, desc, params);
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

int afm_launch_initialize()
{
	int rc;
	gid_t r, e, s;

	getresgid(&r, &e, &s);
	if (s && s != e)
		groupid = s;
	else
		groupid = -1;

	rc = read_configuration_file(FWK_LAUNCH_CONF);
	dump_launchers();
	return rc;
}

static struct desc_list *search_launcher(const char *type, enum afm_launch_mode mode)
{
	struct desc_list *dl;
	struct type_list *tl;

	for (dl = launchers ; dl ; dl = dl->next)
		if (dl->mode == mode)
			for (tl = dl->types ; tl != NULL ; tl = tl->next)
				if (!strcmp(tl->type, type))
					return dl;
	return NULL;
}

int afm_launch(struct afm_launch_desc *desc, pid_t children[2], char **uri)
{
	char datadir[PATH_MAX];
	int rc;
	char secret[9];
	struct launchparam params;
	const char *type;
	struct desc_list *dl;

	/* should be init */
	assert(groupid != 0);
	assert(launch_mode_is_valid(desc->mode));

	/* init */
	children[0] = 0;
	children[1] = 0;

	/* what launcher ? */
	type = desc->type != NULL && *desc->type ? desc->type : DEFAULT_TYPE;
	dl = search_launcher(type, desc->mode);
	if (dl == NULL) {
		ERROR("type %s not found for mode %s!", type, name_of_launch_mode(desc->mode));
		errno = ENOENT;
		return -1;
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
	params.master = (const char **)dl->execs[0];
	params.slave = (const char **)dl->execs[1];

	return params.slave ? launchexec2(desc, children, &params) : launchexec1(desc, children, &params);
}


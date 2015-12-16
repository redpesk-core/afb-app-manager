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

#include <unistd.h>
#include <signal.h>



#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include <json.h>

#include <wgt-info.h>


enum appstate {
	as_starting,
	as_running,
	as_stopped,
	as_terminating,
	as_terminated
};

struct apprun {
	struct apprun *next_by_runid;
	struct apprun *next_by_pgid;
	int runid;
	pid_t backend;
	pid_t frontend;
	enum appstate state;
	json_object *appli;
};

#define ROOT_RUNNERS_COUNT  32
#define MAX_RUNNER_COUNT    32767

static struct apprun *runners_by_runid[ROOT_RUNNERS_COUNT];
static struct apprun *runners_by_pgid[ROOT_RUNNERS_COUNT];
static int runnercount = 0;
static int runnerid = 0;

/****************** manages pgids **********************/

/* get a runner by its pgid */
static struct apprun *runner_of_pgid(pid_t pgid)
{
	struct apprun *result = runners_by_pgid[(int)(pgid & (ROOT_RUNNERS_COUNT - 1))];
	while (result && result->backend != pgid)
		result = result->next_by_pgid;
	return result;
}

/* insert a runner for its pgid */
static void pgid_insert(struct apprun *runner)
{
	struct apprun **prev = &runners_by_runid[(int)(runner->backend & (ROOT_RUNNERS_COUNT - 1))];
	runner->next_by_pgid = *prev;
	*prev = runner;
}

/* remove a runner for its pgid */
static void pgid_remove(struct apprun *runner)
{
	struct apprun **prev = &runners_by_runid[(int)(runner->backend & (ROOT_RUNNERS_COUNT - 1))];
	runner->next_by_pgid = *prev;
	*prev = runner;
}

/****************** manages pids **********************/

/* get a runner by its pid */
static struct apprun *runner_of_pid(pid_t pid)
{
	/* try avoiding system call */
	struct apprun *result = runner_of_pgid(pid);
	if (result == NULL)
		result = runner_of_pgid(getpgid(pid));
	return result;
}

/****************** manages runners (by runid) **********************/

/* get a runner by its runid */
static struct apprun *getrunner(int runid)
{
	struct apprun *result = runners_by_runid[runid & (ROOT_RUNNERS_COUNT - 1)];
	while (result && result->runid != runid)
		result = result->next_by_runid;
	return result;
}

/* free an existing runner */
static void freerunner(struct apprun *runner)
{
	struct apprun **prev = &runners_by_runid[runner->runid & (ROOT_RUNNERS_COUNT - 1)];
	assert(*prev);
	while(*prev != runner) {
		prev = &(*prev)->next_by_runid;
		assert(*prev);
	}
	*prev = runner->next_by_runid;
	json_object_put(runner->appli);
	free(runner);
	runnercount--;
}

/* create a new runner */
static struct apprun *createrunner(json_object *appli)
{
	struct apprun *result;
	struct apprun **prev;

	if (runnercount >= MAX_RUNNER_COUNT)
		return NULL;
	do {
		runnerid++;
		if (runnerid > MAX_RUNNER_COUNT)
			runnerid = 1;
	} while(getrunner(runnerid));
	result = calloc(1, sizeof * result);
	if (result) {
		prev = &runners_by_runid[runnerid & (ROOT_RUNNERS_COUNT - 1)];
		result->next_by_runid = *prev;
		result->next_by_pgid = NULL;
		result->runid = runnerid;
		result->backend = 0;
		result->frontend = 0;
		result->state = as_starting;
		result->appli = json_object_get(appli);
		*prev = result;
		runnercount++;
	}
	return result;
}

/**************** signaling ************************/

static void started(int runid)
{
}

static void stopped(int runid)
{
}

static void continued(int runid)
{
}

static void terminated(int runid)
{
}

static void removed(int runid)
{
}

/**************** running ************************/

static int killrunner(int runid, int sig, enum appstate tostate)
{
	int rc;
	struct apprun *runner = getrunner(runid);
	if (runner == NULL) {
		errno = ENOENT;
		rc = -1;
	}
	else if (runner->state != as_running && runner->state != as_stopped) {
		errno = EPERM;
		rc = -1;
	}
	else if (runner->state == tostate) {
		rc = 0;
	}
	else {
		rc = killpg(runner->backend, sig);
		if (!rc)
			runner->state = tostate;
	}
	return rc;
}

/**************** API handling ************************/

int appfwk_run_start(struct json_object *appli)
{
	return -1;
}

int appfwk_run_terminate(int runid)
{
	return killrunner(runid, SIGTERM, as_terminating);
}

int appfwk_run_stop(int runid)
{
	return killrunner(runid, SIGSTOP, as_stopped);
}

int appfwk_run_continue(int runid)
{
	return killrunner(runid, SIGCONT, as_running);
}

static json_object *mkstate(struct apprun *runner, const char **runidstr)
{
	const char *state;
	struct json_object *result, *obj, *runid;
	int rc;

	/* the structure */
	result = json_object_new_object();
	if (result == NULL)
		goto error;

	/* the runid */
	runid = json_object_new_int(runner->runid);
	if (runid == NULL)
		goto error2;
	json_object_object_add(result, "runid", obj); /* TODO TEST STATUS */

	/* the state */
	switch(runner->state) {
	case as_starting:
	case as_running:
		state = "running";
		break;
	case as_stopped:
		state = "stopped";
		break;
	default:
		state = "terminated";
		break;
	}
	obj = json_object_new_string(state);
	if (obj == NULL)
		goto error2;
	json_object_object_add(result, "state", obj); /* TODO TEST STATUS */

	/* the application id */
	rc = json_object_object_get_ex(runner->appli, "public", &obj);
	assert(rc);
	rc = json_object_object_get_ex(obj, "id", &obj);
	assert(rc);
	json_object_object_add(result, "id", obj); /* TODO TEST STATUS */
	json_object_get(obj);

	/* done */
	if (runidstr)
		*runidstr = json_object_get_string(runid);
	return result;

error2:
	json_object_put(result);
error:
	errno = ENOMEM;
	return NULL;
}

struct json_object *appfwk_run_list()
{
	struct json_object *result, *obj;
	struct apprun *runner;
	const char *runidstr;
	int i;

	/* creates the object */
	result = json_object_new_object();
	if (result == NULL) {
		errno = ENOMEM;
		return NULL;		
	}

	for (i = 0 ; i < ROOT_RUNNERS_COUNT ; i++) {
		for (runner = runners_by_runid[i] ; runner ; runner = runner->next_by_runid) {
			if (runner->state != as_terminating && runner->state != as_terminated) {
				obj = mkstate(runner, &runidstr);
				if (obj == NULL) {
					json_object_put(result);
					return NULL;
				}
				/* TODO status ? */
				json_object_object_add(result, runidstr, obj);
			}
		}
	}
	return result;
}

struct json_object *appfwk_run_state(int runid)
{
	struct apprun *runner = getrunner(runid);
	if (runner == NULL || runner->state == as_terminating || runner->state == as_terminated) {
		errno = ENOENT;
		return NULL;
	}
	return mkstate(runner, NULL);
}


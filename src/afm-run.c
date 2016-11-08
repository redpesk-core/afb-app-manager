/*
 Copyright 2015, 2016 IoT.bzh

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

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <linux/xattr.h>
#if SIMULATE_LIBSMACK
#include "simulation/smack.h"
#else
#include <sys/smack.h>
#endif

#include <json-c/json.h>

#include "verbose.h"
#include "utils-dir.h"
#include "utils-json.h"
#include "afm-launch-mode.h"
#include "afm-launch.h"
#include "afm-run.h"

/*
 * State of a launched/running application
 */
enum appstate {
	as_starting,    /* start in progress */
	as_running,     /* started and running */
	as_paused,      /* paused */
	as_terminating, /* termination in progress */
	as_terminated   /* terminated */
};

/*
 * Structure for recording a runner
 */
struct apprun {
	struct apprun *next_by_runid; /* link for hashing by runid */
	struct apprun *next_by_pgid;  /* link for hashing by pgid */
	int runid;           /* runid */
	pid_t pids[2];       /* pids (0: group leader, 1: slave) */
	enum appstate state; /* current state of the application */
	json_object *appli;  /* json object describing the application */
};

/*
 * Count of item by hash table
 */
#define ROOT_RUNNERS_COUNT  32

/*
 * Maximum count of simultaneous running application
 */
#define MAX_RUNNER_COUNT    32767

/*
 * Hash tables of runners by runid and by pgid
 */
static struct apprun *runners_by_runid[ROOT_RUNNERS_COUNT];
static struct apprun *runners_by_pgid[ROOT_RUNNERS_COUNT];

/*
 * List of terminated runners
 */
static struct apprun *terminated_runners = NULL;

/*
 * Count of runners
 */
static int runnercount = 0;

/*
 * Last given runid
 */
static int runnerid = 0;

/*
 * Path name of the directory for applications in the
 * home directory of the user.
 */
static const char fwk_user_app_dir[] = FWK_USER_APP_DIR;
static const char fwk_user_app_label[] = FWK_USER_APP_DIR_LABEL;

/*
 * Path of the root directory for applications of the
 * current user
 */
static char *homeappdir;

/****************** manages pids **********************/

/*
 * Get a runner by its 'pid' (NULL if not found)
 */
static struct apprun *runner_of_pid(pid_t pid)
{
	int i;
	struct apprun *result;

	for (i = 0 ; i < ROOT_RUNNERS_COUNT ; i++) {
		result = runners_by_pgid[i];
		while (result != NULL) {
			if (result->pids[0] == pid || result->pids[1] == pid)
				return result;
			result = result->next_by_pgid;
		}
	}
	return NULL;
}

/****************** manages pgids **********************/

/*
 * Get a runner by its 'pgid' (NULL if not found)
 */
static struct apprun *runner_of_pgid(pid_t pgid)
{
	struct apprun *result;

	result = runners_by_pgid[pgid & (ROOT_RUNNERS_COUNT - 1)];
	while (result && result->pids[0] != pgid)
		result = result->next_by_pgid;
	return result;
}

/*
 * Insert a 'runner' for its pgid
 */
static void pgid_insert(struct apprun *runner)
{
	struct apprun **prev;

	prev = &runners_by_pgid[runner->pids[0] & (ROOT_RUNNERS_COUNT - 1)];
	runner->next_by_pgid = *prev;
	*prev = runner;
}

/*
 * Remove a 'runner' for its pgid
 */
static void pgid_remove(struct apprun *runner)
{
	struct apprun **prev;

	prev = &runners_by_pgid[runner->pids[0] & (ROOT_RUNNERS_COUNT - 1)];
	while (*prev) {
		if (*prev == runner) {
			*prev = runner->next_by_pgid;
			break;
		}
		prev = &(*prev)->next_by_pgid;
	}
}

/****************** manages runners (by runid) **********************/

/*
 * Get a runner by its 'runid'  (NULL if not found)
 */
static struct apprun *getrunner(int runid)
{
	struct apprun *result;

	result = runners_by_runid[runid & (ROOT_RUNNERS_COUNT - 1)];
	while (result && result->runid != runid)
		result = result->next_by_runid;
	return result;
}

/*
 * Free an existing 'runner'
 */
static void freerunner(struct apprun *runner)
{
	struct apprun **prev;

	/* get previous pointer to runner */
	prev = &runners_by_runid[runner->runid & (ROOT_RUNNERS_COUNT - 1)];
	assert(*prev);
	while(*prev != runner) {
		prev = &(*prev)->next_by_runid;
		assert(*prev);
	}

	/* unlink */
	*prev = runner->next_by_runid;
	runnercount--;

	/* release/free */
	json_object_put(runner->appli);
	free(runner);
}

/*
 * Cleans the list of runners from its terminated
 */
static void cleanrunners()
{
	struct apprun *runner;
	while (terminated_runners) {
		runner = terminated_runners;
		terminated_runners = runner->next_by_pgid;
		freerunner(runner);
	}
}

/*
 * Create a new runner for the 'appli'
 *
 * Returns the created runner or NULL
 * in case of error.
 */
static struct apprun *createrunner(json_object *appli)
{
	struct apprun *result;
	struct apprun **prev;

	/* cleanup */
	cleanrunners();

	/* get a runid */
	if (runnercount >= MAX_RUNNER_COUNT) {
		errno = EAGAIN;
		return NULL;
	}
	do {
		runnerid++;
		if (runnerid > MAX_RUNNER_COUNT)
			runnerid = 1;
	} while(getrunner(runnerid));

	/* create the structure */
	result = calloc(1, sizeof * result);
	if (result == NULL)
		errno = ENOMEM;
	else {
		/* initialize it linked to the list */
		prev = &runners_by_runid[runnerid & (ROOT_RUNNERS_COUNT - 1)];
		result->next_by_runid = *prev;
		result->next_by_pgid = NULL;
		result->runid = runnerid;
		result->pids[0] = result->pids[1] = 0;
		result->state = as_starting;
		result->appli = json_object_get(appli);
		*prev = result;
		runnercount++;
	}
	return result;
}

/**************** signaling ************************/
#if 0
static void started(int runid)
{
}

static void paused(int runid)
{
}

static void resumed(int runid)
{
}

static void terminated(int runid)
{
}

static void removed(int runid)
{
}
#endif
/**************** running ************************/

/*
 * Sends (with pgkill) the signal 'sig' to the process group
 * for 'runid' and put the runner's state to 'tostate'
 * in case of success.
 *
 * Only processes in the state 'as_running' or 'as_paused'
 * can be signalled.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
static int killrunner(int runid, int sig, enum appstate tostate)
{
	int rc;
	struct apprun *runner = getrunner(runid);
	if (runner == NULL) {
		errno = ENOENT;
		rc = -1;
	}
	else if (runner->state != as_running && runner->state != as_paused) {
		errno = EINVAL;
		rc = -1;
	}
	else if (runner->state == tostate) {
		rc = 0;
	}
	else {
		rc = killpg(runner->pids[0], sig);
		if (!rc)
			runner->state = tostate;
	}
	return rc;
}

/*
 * Signal callback called on SIGCHLD. This is set using sigaction.
 */
static void on_sigchld(int signum, siginfo_t *info, void *uctxt)
{
	struct apprun *runner;

	/* retrieves the runner */
	runner = runner_of_pid(info->si_pid);
	if (!runner)
		return;

	/* known runner, inspect cause of signal */
	switch(info->si_code) {
	case CLD_EXITED:
	case CLD_KILLED:
	case CLD_DUMPED:
	case CLD_TRAPPED:
		/* update the state */
		runner->state = as_terminated;
		/* remove it from pgid list */
		pgid_remove(runner);
		runner->next_by_pgid = terminated_runners;
		terminated_runners = runner;
		/* ensures that all the group terminates */
		killpg(runner->pids[0], SIGKILL);
		break;

	case CLD_STOPPED:
		/* update the state */
		runner->state = as_paused;
		break;

	case CLD_CONTINUED:
		/* update the state */
		runner->state = as_running;
		break;
	}
}

/**************** handle afm_launch_desc *********************/

/*
 * Initialize the data of the launch description 'desc'
 * for the application 'appli' and the 'mode'.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
static int fill_launch_desc(struct json_object *appli,
		enum afm_launch_mode mode, struct afm_launch_desc *desc)
{
	json_object *pub;

	assert(is_valid_launch_mode(mode));

	/* main items */
	if(!j_read_object_at(appli, "public", &pub)
	|| !j_read_string_at(appli, "path", &desc->path)
	|| !j_read_string_at(appli, "id", &desc->appid)
	|| !j_read_string_at(appli, "content", &desc->content)
	|| !j_read_string_at(appli, "type", &desc->type)
	|| !j_read_string_at(pub, "name", &desc->name)
	|| !j_read_integer_at(pub, "width", &desc->width)
	|| !j_read_integer_at(pub, "height", &desc->height)) {
		errno = EINVAL;
		return -1;
	}

	/* bindings */
	{
		/* TODO */
		static const char *null = NULL;
		desc->bindings = &null;
	}

	/* finaly */
	desc->home = homeappdir;
	desc->mode = mode;
	return 0;
}

/**************** report state of runner *********************/

/*
 * Creates a json object that describes the state of 'runner'.
 *
 * Returns the created object or NULL in case of error.
 */
static json_object *mkstate(struct apprun *runner)
{
	const char *state;
	struct json_object *result, *obj, *pids;
	int rc;

	/* the structure */
	result = json_object_new_object();
	if (result == NULL)
		goto error;

	/* the runid */
	if (!j_add_integer(result, "runid", runner->runid))
		goto error2;

	/* the pids */
	switch(runner->state) {
	case as_starting:
	case as_running:
	case as_paused:
		pids = j_add_new_array(result, "pids");
		if (!pids)
			goto error2;
		if (!j_add_integer(pids, NULL, runner->pids[0]))
			goto error2;
		if (runner->pids[1] && !j_add_integer(pids, NULL, runner->pids[1]))
			goto error2;
		break;
	default:
		break;
	}

	/* the state */
	switch(runner->state) {
	case as_starting:
	case as_running:
		state = "running";
		break;
	case as_paused:
		state = "paused";
		break;
	default:
		state = "terminated";
		break;
	}
	if (!j_add_string(result, "state", state))
		goto error2;

	/* the application id */
	rc = json_object_object_get_ex(runner->appli, "public", &obj);
	assert(rc);
	rc = json_object_object_get_ex(obj, "id", &obj);
	assert(rc);
	if (!j_add(result, "id", obj))
		goto error2;
	json_object_get(obj);

	/* done */
	return result;

error2:
	json_object_put(result);
error:
	errno = ENOMEM;
	return NULL;
}

/**************** API handling ************************/

/*
 * Starts the application described by 'appli' for the 'mode'.
 * In case of remote start, it returns in uri the uri to
 * connect to.
 *
 * A reference to 'appli' is kept during the live of the
 * runner. This is made using json_object_get. Thus be aware
 * that further modifications to 'appli' might create errors.
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_run_start(struct json_object *appli, enum afm_launch_mode mode,
							char **uri)
{
	static struct apprun *runner;
	struct afm_launch_desc desc;
	int rc;
	sigset_t saved, blocked;

	assert(is_valid_launch_mode(mode));
	assert(mode == mode_local || uri != NULL);
	assert(uri == NULL || *uri == NULL);

	/* prepare to launch */
	rc = fill_launch_desc(appli, mode, &desc);
	if (rc)
		return rc;
	runner = createrunner(appli);
	if (!runner)
		return -1;

	/* block children signals until launched */
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blocked, &saved);

	/* launch now */
	rc = afm_launch(&desc, runner->pids, uri);
	if (rc < 0) {
		/* fork failed */
		sigprocmask(SIG_SETMASK, &saved, NULL);
		ERROR("can't start, afm_launch failed: %m");
		freerunner(runner);
		return -1;
	}

	/* insert the pid */
	runner->state = as_running;
	pgid_insert(runner);
	rc = runner->runid;

	/* unblock children signal now */
	sigprocmask(SIG_SETMASK, &saved, NULL);
	return rc;
}

/*
 * Terminates the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_run_terminate(int runid)
{
	return killrunner(runid, SIGTERM, as_terminating);
}

/*
 * Stops (aka pause) the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_run_pause(int runid)
{
	return killrunner(runid, SIGSTOP, as_paused);
}

/*
 * Continue (aka resume) the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_run_resume(int runid)
{
	return killrunner(runid, SIGCONT, as_running);
}

/*
 * Get the list of the runners.
 *
 * Returns the list or NULL in case of error.
 */
struct json_object *afm_run_list()
{
	struct json_object *result, *obj;
	struct apprun *runner;
	int i;

	/* creates the object */
	result = json_object_new_array();
	if (result == NULL)
		goto error;

	/* iterate over runners */
	for (i = 0 ; i < ROOT_RUNNERS_COUNT ; i++) {
		runner = runners_by_runid[i];
		while (runner) {
			if (runner->state != as_terminating
					&& runner->state != as_terminated) {
				/* adds the living runner */
				obj = mkstate(runner);
				if (obj == NULL)
					goto error2;
				if (json_object_array_add(result, obj) == -1) {
					json_object_put(obj);
					goto error2;
				}
			}
			runner = runner->next_by_runid;
		}
	}
	return result;

error2:
	json_object_put(result);
error:
	errno = ENOMEM;
	return NULL;
}

/*
 * Get the state of the runner of 'runid'.
 *
 * Returns the state or NULL in case of success
 */
struct json_object *afm_run_state(int runid)
{
	struct apprun *runner = getrunner(runid);
	if (runner == NULL || runner->state == as_terminating
				|| runner->state == as_terminated) {
		errno = ENOENT;
		return NULL;
	}
	return mkstate(runner);
}

/**************** INITIALISATION **********************/

/*
 * Initialize the module
 */
int afm_run_init()
{
	char buf[2048];
	int rc;
	uid_t me;
	struct passwd passwd, *pw;
	struct sigaction siga;

	/* init launcher */
	rc = afm_launch_initialize();
	if (rc)
		return rc;

	/* computes the 'homeappdir' */
	me = geteuid();
	rc = getpwuid_r(me, &passwd, buf, sizeof buf, &pw);
	if (rc || pw == NULL) {
		errno = rc ? errno : ENOENT;
		ERROR("getpwuid_r failed for uid=%d: %m",(int)me);
		return -1;
	}
	rc = asprintf(&homeappdir, "%s/%s", passwd.pw_dir, fwk_user_app_dir);
	if (rc < 0) {
		errno = ENOMEM;
		ERROR("allocating homeappdir for uid=%d failed", (int)me);
		return -1;
	}
	rc = create_directory(homeappdir, 0755, 1);
	if (rc && errno != EEXIST) {
		ERROR("creation of directory %s failed: %m", homeappdir);
		free(homeappdir);
		return -1;
	}
	rc = smack_remove_label_for_path(homeappdir,
						XATTR_NAME_SMACKTRANSMUTE, 0);
	if (rc < 0 && errno != ENODATA) {
		ERROR("can't remove smack transmutation of directory %s: %m",
								homeappdir);
		free(homeappdir);
		return -1;
	}
	rc = smack_set_label_for_path(homeappdir, XATTR_NAME_SMACK, 0,
							fwk_user_app_label);
	if (rc < 0) {
		ERROR("can't set smack label %s to directory %s: %m",
					fwk_user_app_label, homeappdir);
		free(homeappdir);
		return -1;
	}
	/* install signal handlers */
	siga.sa_flags = SA_SIGINFO | SA_NOCLDWAIT;
	sigemptyset(&siga.sa_mask);
	sigaddset(&siga.sa_mask, SIGCHLD);
	siga.sa_sigaction = on_sigchld;
	sigaction(SIGCHLD, &siga, NULL);
	return 0;
}


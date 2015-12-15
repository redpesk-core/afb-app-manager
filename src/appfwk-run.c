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
	as_in_progress,
	as_running,
	as_paused,
	as_stopping
};

struct apprun {
	struct apprun *next;
	int id;
	enum appstate state;
	pid_t backend;
	pid_t frontend;
};

#define ROOT_RUNNERS_COUNT  32
#define MAX_RUNNER_COUNT    32767

static struct apprun *runners[ROOT_RUNNERS_COUNT];
static int runnercount = 0;
static int runnerid = 0;

static struct apprun *getrunner(int id)
{
	struct apprun *result = runners[id & (ROOT_RUNNERS_COUNT - 1)];
	while (result && result->id != id)
		result = result->next;
	return result;
}

static void freerunner(struct apprun *runner)
{
	struct apprun **prev = &runners[runner->id & (ROOT_RUNNERS_COUNT - 1)];
	assert(*prev);
	while(*prev != runner) {
		prev = &(*prev)->next;
		assert(*prev);
	}
	*prev = runner->next;
	free(runner);
	runnercount--;
}

static struct apprun *createrunner()
{
	struct apprun *result;

	if (runnercount >= MAX_RUNNER_COUNT)
		return NULL;
	do {
		runnerid++;
		if (runnerid > MAX_RUNNER_COUNT)
			runnerid = 1;
	} while(getrunner(runnerid));
	result = malloc(sizeof * result);
	if (result) {
		result->id = runnerid;
		result->state = as_in_progress;
		result->backend = 0;
		result->frontend = 0;
		result->next = runners[runnerid & (ROOT_RUNNERS_COUNT - 1)];
		runners[runnerid & (ROOT_RUNNERS_COUNT - 1)] = result;
		runnercount++;
	}
	return result;
}

int appfwk_run_start(struct json_object *appli)
{
	return -1;
}

int appfwk_run_stop(int runid)
{
	return -1;
}

int appfwk_run_suspend(int runid)
{
	return -1;
}

int appfwk_run_resume(int runid)
{
	return -1;
}

struct json_object *appfwk_run_list()
{
	return NULL;
}

struct json_object *appfwk_run_state(int runid)
{
	return NULL;
}

#if 0

static struct json_object *mkrunner(const char *appid, const char *runid)
{
	struct json_object *result = json_object_new_object();
	if (result) {
		if(json_add_str(result, "id", appid)
		|| json_add_str(result, "runid", runid)
		|| json_add_str(result, "state", NULL)) {
			json_object_put(result);
			result = NULL;
		}
	}
	return result;
}

const char *appfwk_start(struct appfwk *af, const char *appid)
{
	struct json_object *appli;
	struct json_object *runner;
	char buffer[250];

	/* get the application description */
	appli = appfwk_get_application(af, appid);
	if (appli == NULL) {
		errno = ENOENT;
		return -1;
	}

	/* prepare the execution */
	snprintf(buffer, sizeof buffer, "{\"id\":\"%s\",\"runid\":\"%s\""
}

int appfwk_stop(struct appfwk *af, const char *runid)
{
	struct json_object *runner;
	runner = appfwk_state(af, runid);
	if (runner == NULL) {
		errno = ENOENT;
		return -1;
	}
	json_object_get(runner);
	json_object_object_del(af->runners, runid);






..........






	json_object_put(runner);
}

int appfwk_suspend(struct appfwk *af, const char *runid)
{
}

int appfwk_resume(struct appfwk *af, const char *runid)
{
}

struct json_object *appfwk_running_list(struct appfwk *af)
{
	return af->runners;
}

struct json_object *appfwk_state(struct appfwk *af, const char *runid)
{
	struct json_object *result;
	int status = json_object_object_get_ex(af->runners, runid, &result);
	return status ? result : NULL;
}







#if defined(TESTAPPFWK)
#include <stdio.h>
int main()
{
struct appfwk *af = appfwk_create();
appfwk_add_root(af,FWK_APP_DIR);
appfwk_update_applications(af);
printf("array = %s\n", json_object_to_json_string_ext(af->applications.pubarr, 3));
printf("direct = %s\n", json_object_to_json_string_ext(af->applications.direct, 3));
printf("byapp = %s\n", json_object_to_json_string_ext(af->applications.byapp, 3));
return 0;
}
#endif

static struct json_object *mkrunner(const char *appid, const char *runid)
{
	struct json_object *result = json_object_new_object();
	if (result) {
		if(json_add_str(result, "id", appid)
		|| json_add_str(result, "runid", runid)
		|| json_add_str(result, "state", NULL)) {
			json_object_put(result);
			result = NULL;
		}
	}
	return result;
}

const char *appfwk_start(struct appfwk *af, const char *appid)
{
	struct json_object *appli;
	struct json_object *runner;
	char buffer[250];

	/* get the application description */
	appli = appfwk_get_application(af, appid);
	if (appli == NULL) {
		errno = ENOENT;
		return -1;
	}

	/* prepare the execution */
}

int appfwk_stop(struct appfwk *af, const char *runid)
{
	struct json_object *runner;
	runner = appfwk_state(af, runid);
	if (runner == NULL) {
		errno = ENOENT;
		return -1;
	}
	json_object_get(runner);
	json_object_object_del(af->runners, runid);






..........






	json_object_put(runner);
}

int appfwk_suspend(struct appfwk *af, const char *runid)
{
}

int appfwk_resume(struct appfwk *af, const char *runid)
{
}

struct json_object *appfwk_running_list(struct appfwk *af)
{
	return af->runners;
}

struct json_object *appfwk_state(struct appfwk *af, const char *runid)
{
	struct json_object *result;
	int status = json_object_object_get_ex(af->runners, runid, &result);
	return status ? result : NULL;
}



#endif

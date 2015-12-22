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

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>

#include <json.h>

#include "verbose.h"
#include "utils-jbus.h"
#include "af-db.h"
#include "af-run.h"

static const char appname[] = "afm-user-daemon";

static void usage()
{
	printf(
		"usage: %s [-q] [-v] [-r rootdir]... [-a appdir]...\n"
		"\n"
		"   -a appdir    adds an application directory\n"
		"   -r rootdir   adds a root directory of applications\n"
		"   -d           run as a daemon\n"
		"   -q           quiet\n"
		"   -v           verbose\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "root",        required_argument, NULL, 'r' },
	{ "application", required_argument, NULL, 'a' },
	{ "daemon",      no_argument,       NULL, 'd' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "help",        no_argument,       NULL, 'h' },
	{ NULL, 0, NULL, 0 }
};

static struct jbus *jbus;
static struct af_db *afdb;

const char error_nothing[] = "[]";
const char error_bad_request[] = "\"bad request\"";
const char error_not_found[] = "\"not found\"";
const char error_cant_start[] = "\"can't start\"";

static const char *getappid(struct json_object *obj)
{
	return json_type_string == json_object_get_type(obj) ? json_object_get_string(obj) : NULL;
}

static int getrunid(struct json_object *obj)
{
	return json_type_int == json_object_get_type(obj) ? json_object_get_int(obj) : 0;
}

static void reply(struct jreq *jreq, struct json_object *resp, const char *errstr)
{
	if (resp)
		jbus_reply_j(jreq, resp);
	else
		jbus_reply_error_s(jreq, errstr);
}

static void reply_status(struct jreq *jreq, int status)
{
	if (status)
		jbus_reply_error_s(jreq, error_not_found);
	else
		jbus_reply_s(jreq, "true");
}

static void on_runnables(struct jreq *jreq, struct json_object *obj)
{
	struct json_object *resp = af_db_application_list(afdb);
	jbus_reply_j(jreq, resp);
	json_object_put(obj);
}

static void on_detail(struct jreq *jreq, struct json_object *obj)
{
	const char *appid = getappid(obj);
	struct json_object *resp = af_db_get_application_public(afdb, appid);
	reply(jreq, resp, error_not_found);
	json_object_put(obj);
}

static void on_start(struct jreq *jreq, struct json_object *obj)
{
	const char *appid;
	struct json_object *appli;
	int runid;
	char runidstr[20];

	appid = getappid(obj);
	if (appid == NULL)
		jbus_reply_error_s(jreq, error_bad_request);
	else {
		appli = af_db_get_application(afdb, appid);
		if (appli == NULL)
			jbus_reply_error_s(jreq, error_not_found);
		else {
			runid = af_run_start(appli);
			if (runid <= 0)
				jbus_reply_error_s(jreq, error_cant_start);
			else {
				snprintf(runidstr, sizeof runidstr, "%d", runid);
				runidstr[sizeof runidstr - 1] = 0;
				jbus_reply_s(jreq, runidstr);
			}
		}
	}
	json_object_put(obj);
}

static void on_stop(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	int status = af_run_stop(runid);
	reply_status(jreq, status);
	json_object_put(obj);
}

static void on_continue(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	int status = af_run_continue(runid);
	reply_status(jreq, status);
	json_object_put(obj);
}

static void on_terminate(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	int status = af_run_terminate(runid);
	reply_status(jreq, status);
	json_object_put(obj);
}

static void on_runners(struct jreq *jreq, struct json_object *obj)
{
	struct json_object *resp = af_run_list();
	jbus_reply_j(jreq, resp);
	json_object_put(resp);
	json_object_put(obj);
}

static void on_state(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	struct json_object *resp = af_run_state(runid);
	reply(jreq, resp, error_not_found);
	json_object_put(resp);
	json_object_put(obj);
}

static int daemonize()
{
	int rc = fork();
	if (rc < 0)
		return rc;
	if (rc)
		_exit(0);
	return 0;
}

int main(int ac, char **av)
{
	int i, daemon = 0;

	LOGAUTH(appname);

	/* first interpretation of arguments */
	while ((i = getopt_long(ac, av, "hdqvr:a:", options, NULL)) >= 0) {
		switch (i) {
		case 'h':
			usage();
			return 0;
		case 'q':
			if (verbosity)
				verbosity--;
			break;
		case 'v':
			verbosity++;
			break;
		case 'd':
			daemon = 1;
			break;
		case 'r':
			break;
		case 'a':
			break;
		case ':':
			ERROR("missing argument value");
			return 1;
		default:
			ERROR("unrecognized option");
			return 1;
		}
	}

	/* init random generator */
	srandom((unsigned int)time(NULL));

	/* init runners */
	if (af_run_init()) {
		ERROR("af_run_init failed");
		return 1;
	}

	/* init framework */
	afdb = af_db_create();
	if (!afdb) {
		ERROR("af_create failed");
		return 1;
	}
	if (af_db_add_root(afdb, FWK_APP_DIR)) {
		ERROR("can't add root %s", FWK_APP_DIR);
		return 1;
	}

	/* second interpretation of arguments */
	optind = 1;
	while ((i = getopt_long(ac, av, "hdqvr:a:", options, NULL)) >= 0) {
		switch (i) {
		case 'r':
			if (af_db_add_root(afdb, optarg)) {
				ERROR("can't add root %s", optarg);
				return 1;
			}
			break;
		case 'a':
			if (af_db_add_application(afdb, optarg)) {
                                ERROR("can't add application %s", optarg);
                                return 1;
                        }
			break;
		}
	}

	/* update the database */
	if (af_db_update_applications(afdb)) {
		ERROR("af_update_applications failed");
		return 1;
	}

	if (daemon && daemonize()) {
		ERROR("daemonization failed");
		return 1;
	}

	/* init service	*/
	jbus = create_jbus(1, "/org/AGL/afmMain");
	if (!jbus) {
		ERROR("create_jbus failed");
		return 1;
	}
	if(jbus_add_service_j(jbus, "runnables", on_runnables)
	|| jbus_add_service_j(jbus, "detail", on_detail)
	|| jbus_add_service_j(jbus, "start", on_start)
	|| jbus_add_service_j(jbus, "terminate", on_terminate)
	|| jbus_add_service_j(jbus, "stop", on_stop)
	|| jbus_add_service_j(jbus, "continue", on_continue)
	|| jbus_add_service_j(jbus, "runners", on_runners)
	|| jbus_add_service_j(jbus, "state", on_state)) {
		ERROR("adding services failed");
		return 1;
	}

	/* start and run */
	if (jbus_start_serving(jbus)) {
		ERROR("cant start server");
		return 1;
	}
	while (!jbus_read_write_dispatch(jbus, -1));
	return 0;
}


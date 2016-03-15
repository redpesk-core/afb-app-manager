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
#include <string.h>

#include <json.h>

#include "verbose.h"
#include "utils-jbus.h"
#include "utils-json.h"
#include "afm.h"
#include "afm-db.h"
#include "afm-launch-mode.h"
#include "afm-run.h"

/*
 * name of the application
 */
static const char appname[] = "afm-user-daemon";

/*
 * string for printing usage
 */
static const char usagestr[] =
	"usage: %s [-q] [-v] [-m mode] [-r rootdir]... [-a appdir]...\n"
	"\n"
	"   -a appdir    adds an application directory\n"
	"   -r rootdir   adds a root directory of applications\n"
	"   -m mode      set default launch mode (local or remote)\n"
	"   -d           run as a daemon\n"
	"   -q           quiet\n"
	"   -v           verbose\n"
	"\n";

/*
 * Option definition for getopt_long
 */
static const char options_s[] = "hdqvr:a:m:";
static struct option options_l[] = {
	{ "root",        required_argument, NULL, 'r' },
	{ "application", required_argument, NULL, 'a' },
	{ "mode",        required_argument, NULL, 'm' },
	{ "daemon",      no_argument,       NULL, 'd' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "help",        no_argument,       NULL, 'h' },
	{ NULL, 0, NULL, 0 }
};

/*
 * Connections to D-Bus
 * This is an array for using the function
 *    jbus_read_write_dispatch_multiple
 * directly without transformations.
 */
static struct jbus *jbuses[2];
#define system_bus  jbuses[0]
#define user_bus    jbuses[1]

/*
 * Handle to the database of applications
 */
static struct afm_db *afdb;

/*
 * Returned error strings
 */
const char error_nothing[] = "[]";
const char error_bad_request[] = "\"bad request\"";
const char error_not_found[] = "\"not found\"";
const char error_cant_start[] = "\"can't start\"";
const char error_system[] = "\"system error\"";


/*
 * retrieves the 'runid' in 'obj' parameters received with the
 * request 'jreq' for the 'method'.
 *
 * Returns 1 in case of success.
 * Otherwise, if the 'runid' can't be retrived, an error stating
 * the bad request is replied for 'jreq' and 0 is returned.
 */
static int onrunid(struct jreq *jreq, struct json_object *obj,
						const char *method, int *runid)
{
	if (!j_read_integer(obj, runid)
				&& !j_read_integer_at(obj, "runid", runid)) {
		INFO("bad request method %s: %s", method,
					json_object_to_json_string(obj));
		jbus_reply_error_s(jreq, error_bad_request);
		return 0;
	}

	INFO("method %s called for %d", method, *runid);
	return 1;
}

/*
 * Sends the reply 'resp' to the request 'jreq' if 'resp' is not NULL.
 * Otherwise, when 'resp' is NULL replies the error string 'errstr'.
 */
static void reply(struct jreq *jreq, struct json_object *resp,
						const char *errstr)
{
	if (resp)
		jbus_reply_j(jreq, resp);
	else
		jbus_reply_error_s(jreq, errstr);
}

/*
 * Sends the reply "true" to the request 'jreq' if 'status' is zero.
 * Otherwise, when 'status' is not zero replies the error string 'errstr'.
 */
static void reply_status(struct jreq *jreq, int status, const char *errstr)
{
	if (status)
		jbus_reply_error_s(jreq, errstr);
	else
		jbus_reply_s(jreq, "true");
}

/*
 * On query "runnables" from 'jreq' with parameters of 'obj'.
 *
 * Nothing is expected in 'obj' that can be anything.
 */
static void on_runnables(struct jreq *jreq, struct json_object *obj)
{
	struct json_object *resp;
	INFO("method runnables called");
	resp = afm_db_application_list(afdb);
	jbus_reply_j(jreq, resp);
	json_object_put(resp);
}

/*
 * On query "detail" from 'jreq' with parameters of 'obj'.
 */
static void on_detail(struct jreq *jreq, struct json_object *obj)
{
	const char *appid;
	struct json_object *resp;

	/* get the parameters */
	if (j_read_string(obj, &appid))
		; /* appid as a string */
	else if (j_read_string_at(obj, "id", &appid))
		; /* appid as obj.id string */
	else {
		INFO("method detail called but bad request!");
		jbus_reply_error_s(jreq, error_bad_request);
		return;
	}

	/* wants details for appid */
	INFO("method detail called for %s", appid);
	resp = afm_db_get_application_public(afdb, appid);
	reply(jreq, resp, error_not_found);
	json_object_put(resp);
}


/*
 * On query "start" from 'jreq' with parameters of 'obj'.
 */
static void on_start(struct jreq *jreq, struct json_object *obj)
{
	const char *appid, *modestr;
	char *uri;
	struct json_object *appli, *resp;
	int runid;
	char runidstr[20];
	enum afm_launch_mode mode;

	/* get the parameters */
	mode = invalid_launch_mode;
	if (j_read_string(obj, &appid)) {
		mode = get_default_launch_mode();
	} else if (j_read_string_at(obj, "id", &appid)) {
		if (j_read_string_at(obj, "mode", &modestr)) {
			mode = launch_mode_of_name(modestr);
		} else {
			mode = get_default_launch_mode();
		}
	}
	if (!is_valid_launch_mode(mode)) {
		jbus_reply_error_s(jreq, error_bad_request);
		return;
	}

	/* get the application */
	INFO("method start called for %s mode=%s", appid,
						name_of_launch_mode(mode));
	appli = afm_db_get_application(afdb, appid);
	if (appli == NULL) {
		jbus_reply_error_s(jreq, error_not_found);
		return;
	}

	/* launch the application */
	uri = NULL;
	runid = afm_run_start(appli, mode, &uri);
	if (runid <= 0) {
		jbus_reply_error_s(jreq, error_cant_start);
		free(uri);
		return;
	}

	if (uri == NULL) {
		/* returns only the runid */
		snprintf(runidstr, sizeof runidstr, "%d", runid);
		runidstr[sizeof runidstr - 1] = 0;
		jbus_reply_s(jreq, runidstr);
		return;
	}

	/* returns the runid and its uri */
	resp = json_object_new_object();
	if (resp != NULL && j_add_integer(resp, "runid", runid)
					&& j_add_string(resp, "uri", uri))
		jbus_reply_j(jreq, resp);
	else {
		afm_run_stop(runid);
		jbus_reply_error_s(jreq, error_system);
	}
	json_object_put(resp);
	free(uri);
}

/*
 * On query "stop" from 'jreq' with parameters of 'obj'.
 */
static void on_stop(struct jreq *jreq, struct json_object *obj)
{
	int runid, status;
	if (onrunid(jreq, obj, "stop", &runid)) {
		status = afm_run_stop(runid);
		reply_status(jreq, status, error_not_found);
	}
}

/*
 * On query "continue" from 'jreq' with parameters of 'obj'.
 */
static void on_continue(struct jreq *jreq, struct json_object *obj)
{
	int runid, status;
	if (onrunid(jreq, obj, "continue", &runid)) {
		status = afm_run_continue(runid);
		reply_status(jreq, status, error_not_found);
	}
}

/*
 * On query "terminate" from 'jreq' with parameters of 'obj'.
 */
static void on_terminate(struct jreq *jreq, struct json_object *obj)
{
	int runid, status;
	if (onrunid(jreq, obj, "terminate", &runid)) {
		status = afm_run_terminate(runid);
		reply_status(jreq, status, error_not_found);
	}
}

/*
 * On query "runners" from 'jreq' with parameters of 'obj'.
 */
static void on_runners(struct jreq *jreq, struct json_object *obj)
{
	struct json_object *resp;
	INFO("method runners called");
	resp = afm_run_list();
	jbus_reply_j(jreq, resp);
	json_object_put(resp);
}

/*
 * On query "state" from 'jreq' with parameters of 'obj'.
 */
static void on_state(struct jreq *jreq, struct json_object *obj)
{
	int runid;
	struct json_object *resp;
	if (onrunid(jreq, obj, "state", &runid)) {
		resp = afm_run_state(runid);
		reply(jreq, resp, error_not_found);
		json_object_put(resp);
	}
}

/*
 * Calls the system daemon to achieve application management of
 * the 'method' gotten from 'jreq' with the parameter's string 'msg'.
 *
 * The principle is very simple: call the corresponding system method
 * and reply its response to the caller.
 *
 * The request and reply is synchronous and is blocking.
 * It is possible to implment it in an asynchrounous way but it
 * would brake the common behaviour. It would be a call like
 * jbus_call_ss(system_bus, method, msg, callback, jreq)
 */
static void propagate(struct jreq *jreq, const char *msg, const char *method)
{
	char *reply;
	INFO("method %s propagated with %s", method, msg);
	reply = jbus_call_ss_sync(system_bus, method, msg);
	if (reply) {
		jbus_reply_s(jreq, reply);
		free(reply);
	}
	else
		jbus_reply_error_s(jreq, error_system);
}

/*
 * On query "install" from 'jreq' with parameters of 'msg'.
 */
static void on_install(struct jreq *jreq, const char *msg)
{
	return propagate(jreq, msg, "install");
}

/*
 * On query "uninstall" from 'jreq' with parameters of 'msg'.
 */
static void on_uninstall(struct jreq *jreq, const char *msg)
{
	return propagate(jreq, msg, "uninstall");
}

/*
 * On system signaling that applications list changed
 */
static void on_signal_changed(struct json_object *obj)
{
	/* update the database */
	afm_db_update_applications(afdb);
	/* re-propagate now */
	jbus_send_signal_j(user_bus, "changed", obj);
}

/*
 * Tiny routine to daemonize the program
 * Return 0 on success or -1 on failure.
 */
static int daemonize()
{
	int rc = fork();
	if (rc < 0)
		return rc;
	if (rc)
		_exit(0);
	return 0;
}

/*
 * ENTRY POINT OF AFM-USER-DAEMON
 */
int main(int ac, char **av)
{
	int i, daemon = 0;
	enum afm_launch_mode mode;

	LOGAUTH(appname);

	/* first interpretation of arguments */
	while ((i = getopt_long(ac, av, options_s, options_l, NULL)) >= 0) {
		switch (i) {
		case 'h':
			printf(usagestr, appname);
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
		case 'm':
			mode = launch_mode_of_name(optarg);
			if (!is_valid_launch_mode(mode)) {
				ERROR("invalid mode '%s'", optarg);
				return 1;
			}
			set_default_launch_mode(mode);
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
	if (afm_run_init()) {
		ERROR("afm_run_init failed");
		return 1;
	}

	/* init framework */
	afdb = afm_db_create();
	if (!afdb) {
		ERROR("afm_create failed");
		return 1;
	}
	if (afm_db_add_root(afdb, FWK_APP_DIR)) {
		ERROR("can't add root %s", FWK_APP_DIR);
		return 1;
	}

	/* second interpretation of arguments */
	optind = 1;
	while ((i = getopt_long(ac, av, options_s, options_l, NULL)) >= 0) {
		switch (i) {
		case 'r':
			if (afm_db_add_root(afdb, optarg)) {
				ERROR("can't add root %s", optarg);
				return 1;
			}
			break;
		case 'a':
			if (afm_db_add_application(afdb, optarg)) {
				ERROR("can't add application %s", optarg);
				return 1;
			}
			break;
		}
	}

	/* update the database */
	if (afm_db_update_applications(afdb)) {
		ERROR("afm_update_applications failed");
		return 1;
	}

	/* daemonize if requested */
	if (daemon && daemonize()) {
		ERROR("daemonization failed");
		return 1;
	}

	/* connects to the system bus */
	system_bus = create_jbus_system(AFM_SYSTEM_DBUS_PATH);
	if (!system_bus) {
		ERROR("create_jbus failed for system");
		return 1;
	}

	/* observe signals of system */
	if(jbus_on_signal_j(system_bus, "changed", on_signal_changed)) {
		ERROR("adding signal observer failed");
		return 1;
	}

	/* connect to the session bus */
	user_bus = create_jbus_session(AFM_USER_DBUS_PATH);
	if (!user_bus) {
		ERROR("create_jbus failed");
		return 1;
	}

	/* init services */
	if (jbus_add_service_j(user_bus, "runnables", on_runnables)
	 || jbus_add_service_j(user_bus, "detail",    on_detail)
	 || jbus_add_service_j(user_bus, "start",     on_start)
	 || jbus_add_service_j(user_bus, "terminate", on_terminate)
	 || jbus_add_service_j(user_bus, "stop",      on_stop)
	 || jbus_add_service_j(user_bus, "continue",  on_continue)
	 || jbus_add_service_j(user_bus, "runners",   on_runners)
	 || jbus_add_service_j(user_bus, "state",     on_state)
	 || jbus_add_service_s(user_bus, "install",   on_install)
	 || jbus_add_service_s(user_bus, "uninstall", on_uninstall)) {
		ERROR("adding services failed");
		return 1;
	}

	/* start servicing */
	if (jbus_start_serving(user_bus)) {
		ERROR("can't start server");
		return 1;
	}

	/* run until error */
	while (jbus_read_write_dispatch_multiple(jbuses, 2, -1, 20) >= 0);
	return 0;
}


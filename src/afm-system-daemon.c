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
#include <errno.h>

#include <json.h>

#include "verbose.h"
#include "utils-jbus.h"
#include "afm.h"
#include "afm-db.h"
#include "wgt-info.h"
#include "wgtpkg-install.h"

static const char appname[] = "afm-system-daemon";

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
static struct afm_db *afdb;

const char error_nothing[] = "[]";
const char error_bad_request[] = "\"bad request\"";
const char error_not_found[] = "\"not found\"";
const char error_cant_start[] = "\"can't start\"";

static const char *getappid(struct json_object *obj)
{
	return json_type_string == json_object_get_type(obj) ? json_object_get_string(obj) : NULL;
}

static void reply(struct jreq *jreq, struct json_object *resp, const char *errstr)
{
	if (resp)
		jbus_reply_j(jreq, resp);
	else
		jbus_reply_error_s(jreq, errstr);
}

static void on_runnables(struct jreq *jreq, struct json_object *obj)
{
	struct json_object *resp = afm_db_application_list(afdb);
	jbus_reply_j(jreq, resp);
	json_object_put(resp);
}

static void on_detail(struct jreq *jreq, struct json_object *obj)
{
	const char *appid = getappid(obj);
	struct json_object *resp = afm_db_get_application_public(afdb, appid);
	reply(jreq, resp, error_not_found);
	json_object_put(resp);
}

static const char *j_get_string(struct json_object *obj, const char *key, const char *defval)
{
	struct json_object *o;
	return json_object_object_get_ex(obj, key, &o) && json_object_get_type(o) == json_type_string ? json_object_get_string(o) : defval;
}

static int j_get_boolean(struct json_object *obj, const char *key, int defval)
{
	struct json_object *o;
	return json_object_object_get_ex(obj, key, &o) && json_object_get_type(o) == json_type_boolean ? json_object_get_boolean(o) : defval;
}

static int json_add(struct json_object *obj, const char *key, struct json_object *val)
{
	json_object_object_add(obj, key, val);
	return 0;
}

static int json_add_str(struct json_object *obj, const char *key, const char *val)
{
	struct json_object *str = json_object_new_string (val ? val : "");
	return str ? json_add(obj, key, str) : (errno = ENOMEM, -1);
}
/*
static int json_add_int(struct json_object *obj, const char *key, int val)
{
	struct json_object *v = json_object_new_int (val);
	return v ? json_add(obj, key, v) : (errno = ENOMEM, -1);
}
*/
static void on_install(struct jreq *jreq, struct json_object *req)
{
	const char *wgtfile;
	const char *root;
	int force;
	struct wgt_info *ifo;
	struct json_object *resp;

	/* scan the request */
	switch (json_object_get_type(req)) {
	case json_type_string:
		wgtfile = json_object_get_string(req);
		root = FWK_APP_DIR;
		force = 0;
		break;
	case json_type_object:
		wgtfile = j_get_string(req, "wgt", NULL);
		if (wgtfile != NULL) {
			root = j_get_string(req, "root", FWK_APP_DIR);
			force = j_get_boolean(req, "force", 0);
			break;
		}
	default:
		jbus_reply_error_s(jreq, error_bad_request);
		return;
	}

	/* install the widget */
	ifo = install_widget(wgtfile, root, force);
	if (ifo == NULL) {
		jbus_reply_error_s(jreq, "\"installation failed\"");
		return;
	}

	/* build the response */
	resp = json_object_new_object();
	if(!resp || json_add_str(resp, "added", wgt_info_desc(ifo)->idaver)) {
		json_object_put(resp);
		wgt_info_unref(ifo);
		jbus_reply_error_s(jreq, "\"out of memory but installed!\"");
		return;
	}
	wgt_info_unref(ifo);

	/* update the current database */
	afm_db_update_applications(afdb);

	/* reply and propagate event */
	jbus_reply_j(jreq, resp);
	jbus_send_signal_j(jbus, "changed", resp);
	json_object_put(resp);
}

static void on_uninstall(struct jreq *jreq, struct json_object *req)
{
	jbus_reply_error_s(jreq, "\"not yet implemented\"");
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
	while ((i = getopt_long(ac, av, "hdqvr:a:", options, NULL)) >= 0) {
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

	if (daemon && daemonize()) {
		ERROR("daemonization failed");
		return 1;
	}

	/* init service	*/
	jbus = create_jbus(0, AFM_SYSTEM_DBUS_PATH);
	if (!jbus) {
		ERROR("create_jbus failed");
		return 1;
	}
	if(jbus_add_service_j(jbus, "runnables", on_runnables)
	|| jbus_add_service_j(jbus, "detail", on_detail)
	|| jbus_add_service_j(jbus, "install", on_install)
	|| jbus_add_service_j(jbus, "uninstall", on_uninstall)) {
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


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

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <json.h>

#include "verbose.h"
#include "utils-jbus.h"
#include "utils-json.h"
#include "afm.h"
#include "afm-db.h"
#include "wgt-info.h"
#include "wgtpkg-install.h"
#include "wgtpkg-uninstall.h"

static const char appname[] = "afm-system-daemon";
static const char *rootdir = NULL;

static void usage()
{
	printf(
		"usage: %s [-q] [-v] [-r rootdir]\n"
		"\n"
		"   -r rootdir   set root directory of applications\n"
		"   -d           run as a daemon\n"
		"   -q           quiet\n"
		"   -v           verbose\n"
		"\n",
		appname
	);
}

static struct option options[] = {
	{ "root",        required_argument, NULL, 'r' },
	{ "daemon",      no_argument,       NULL, 'd' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "help",        no_argument,       NULL, 'h' },
	{ NULL, 0, NULL, 0 }
};

static struct jbus *jbus;

const char error_nothing[] = "[]";
const char error_bad_request[] = "\"bad request\"";
const char error_not_found[] = "\"not found\"";
const char error_cant_start[] = "\"can't start\"";

static void on_install(struct sd_bus_message *smsg, struct json_object *req, void *unused)
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
		root = rootdir;
		force = 0;
		break;
	case json_type_object:
		wgtfile = j_string_at(req, "wgt", NULL);
		if (wgtfile != NULL) {
			root = j_string_at(req, "root", rootdir);
			force = j_boolean_at(req, "force", 0);
			break;
		}
	default:
		jbus_reply_error_s(smsg, error_bad_request);
		return;
	}

	/* install the widget */
	ifo = install_widget(wgtfile, root, force);
	if (ifo == NULL)
		jbus_reply_error_s(smsg, "\"installation failed\"");
	else {
		/* build the response */
		resp = json_object_new_object();
		if(!resp || !j_add_string(resp, "added", wgt_info_desc(ifo)->idaver))
			jbus_reply_error_s(smsg, "\"out of memory but installed!\"");
		else {
			jbus_send_signal_s(jbus, "changed", "true");
			jbus_reply_j(smsg, resp);
		}

		/* clean-up */
		wgt_info_unref(ifo);
		json_object_put(resp);
	}
}

static void on_uninstall(struct sd_bus_message *smsg, struct json_object *req, void *unused)
{
	const char *idaver;
	const char *root;
	int rc;

	/* scan the request */
	switch (json_object_get_type(req)) {
	case json_type_string:
		idaver = json_object_get_string(req);
		root = rootdir;
		break;
	case json_type_object:
		idaver = j_string_at(req, "id", NULL);
		if (idaver != NULL) {
			root = j_string_at(req, "root", rootdir);
			break;
		}
	default:
		jbus_reply_error_s(smsg, error_bad_request);
		return;
	}

	/* install the widget */
	rc = uninstall_widget(idaver, root);
	if (rc)
		jbus_reply_error_s(smsg, "\"uninstallation had error\"");
	else {
		jbus_send_signal_s(jbus, "changed", "true");
		jbus_reply_s(smsg, "true");
	}
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
	int i, daemon = 0, rc;
	struct sd_event *evloop;
	struct sd_bus *sysbus;

	LOGAUTH(appname);

	/* interpretation of arguments */
	while ((i = getopt_long(ac, av, "hdqvr:", options, NULL)) >= 0) {
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
			if (rootdir == NULL)
				rootdir = optarg;
			else {
				ERROR("duplicate definition of rootdir");
				return 1;
			}
			break;
		case ':':
			ERROR("missing argument value");
			return 1;
		default:
			ERROR("unrecognized option");
			return 1;
		}
	}

	/* check the rootdir */
	if (rootdir == NULL)
		rootdir = FWK_APP_DIR;
	else {
		rootdir = realpath(rootdir, NULL);
		if (rootdir == NULL) {
			ERROR("out of memory");
			return 1;
		}
	}
	if (chdir(rootdir)) {
		ERROR("can't enter %s", rootdir);
		return 1;
	}

	/* daemonize */
	if (daemon && daemonize()) {
		ERROR("daemonization failed");
		return 1;
	}

	/* get systemd objects */
	rc = sd_event_new(&evloop);
	if (rc < 0) {
		ERROR("can't create event loop");
		return 1;
	}
	rc = sd_bus_open_system(&sysbus);
	if (rc < 0) {
		ERROR("can't create system bus");
		return 1;
	}
	rc = sd_bus_attach_event(sysbus, evloop, 0);
	if (rc < 0) {
		ERROR("can't attach system bus to event loop");
		return 1;
	}

	/* init service	*/
	jbus = create_jbus(sysbus, AFM_SYSTEM_DBUS_PATH);
	if (!jbus) {
		ERROR("create_jbus failed");
		return 1;
	}
	if(jbus_add_service_j(jbus, "install", on_install, NULL)
	|| jbus_add_service_j(jbus, "uninstall", on_uninstall, NULL)) {
		ERROR("adding services failed");
		return 1;
	}

	/* start and run */
	if (jbus_start_serving(jbus) < 0) {
		ERROR("can't start server");
		return 1;
	}
	for(;;)
		sd_event_run(evloop, (uint64_t)-1);
	return 0;
}

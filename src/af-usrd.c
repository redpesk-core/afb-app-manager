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

#include <json.h>

#include "verbose.h"
#include "utils-jbus.h"
#include "appfwk.h"
#include "appfwk-run.h"

static struct jbus *jbus;
static struct appfwk *appfwk;

const char error_nothing[] = "[]";
const char error_bad_request[] = "{\"status\":\"error: bad request\"}";
const char error_not_found[] = "{\"status\":\"error: not found\"}";

static const char *getappid(struct json_object *obj)
{
	return json_object_get_string(obj);
}

static int getrunid(struct json_object *obj)
{
	return json_object_get_int(obj);
}

static void reply(struct jreq *jreq, struct json_object *resp, const char *errstr)
{
	if (resp)
		jbus_reply(jreq, resp);
	else
		jbus_replyj(jreq, errstr);
}

static void on_runnables(struct jreq *jreq, struct json_object *obj)
{
	struct json_object *resp = appfwk_application_list(appfwk);
	jbus_reply(jreq, resp);
	json_object_put(obj);
}

static void on_detail(struct jreq *jreq, struct json_object *obj)
{
	const char *appid = getappid(obj);
	struct json_object *resp = appfwk_get_application_public(appfwk, appid);
	reply(jreq, resp, error_not_found);
	json_object_put(obj);
}

static void on_start(struct jreq *jreq, struct json_object *obj)
{
	const char *appid = getappid(obj);
	struct json_object *appli = appid ? appfwk_get_application_public(appfwk, appid) : NULL;
	int runid = appfwk_run_start(appli);
	if (runid <= 0) {
		
	jbus_replyj(jreq, runid ? runid : error_not_found);
	json_object_put(obj);
}
}

static void on_stop(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	int status = appfwk_run_stop(runid);
	jbus_replyj(jreq, status ? error_not_found : "true");
	json_object_put(obj);
}

static void on_suspend(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	int status = appfwk_run_suspend(runid);
	jbus_replyj(jreq, status ? error_not_found : "true");
	json_object_put(obj);
}

static void on_resume(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	int status = appfwk_run_resume(runid);
	jbus_replyj(jreq, status ? error_not_found : "true");
	json_object_put(obj);
}

static void on_runners(struct jreq *jreq, struct json_object *obj)
{
	struct json_object *resp = appfwk_run_list();
	jbus_reply(jreq, resp);
	json_object_put(resp);
	json_object_put(obj);
}

static void on_state(struct jreq *jreq, struct json_object *obj)
{
	int runid = getrunid(obj);
	struct json_object *resp = appfwk_run_state(runid);
	reply(jreq, resp, error_not_found);
	json_object_put(obj);
	json_object_put(resp);
}

int main(int ac, char **av)
{
	LOGAUTH("af-usrd");

	/* init framework */
	appfwk = appfwk_create();
	if (!appfwk) {
		ERROR("appfwk_create failed");
		return 1;
	}
	if (appfwk_add_root(appfwk, FWK_APP_DIR)) {
		ERROR("can't add root %s", FWK_APP_DIR);
		return 1;
	}
	if (appfwk_update_applications(appfwk)) {
		ERROR("appfwk_update_applications failed");
		return 1;
	}

	/* init service	*/
	jbus = create_jbus(1, "/org/automotive/linux/framework");
	if (!jbus) {
		ERROR("create_jbus failed");
		return 1;
	}
	if(jbus_add_service(jbus, "runnables", on_runnables)
	|| jbus_add_service(jbus, "detail", on_detail)
	|| jbus_add_service(jbus, "start", on_start)
	|| jbus_add_service(jbus, "stop", on_stop)
	|| jbus_add_service(jbus, "suspend", on_suspend)
	|| jbus_add_service(jbus, "resume", on_resume)
	|| jbus_add_service(jbus, "runners", on_runners)
	|| jbus_add_service(jbus, "state", on_state)) {
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




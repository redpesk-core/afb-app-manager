/*
 Copyright 2015, 2016, 2017 IoT.bzh

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
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <json-c/json.h>

#include "verbose.h"
#include "utils-dir.h"
#include "utils-json.h"
#include "utils-systemd.h"
#include "afm-udb.h"
#include "afm-urun.h"

/**************** get appli basis *********************/

static int get_basis(struct json_object *appli, int *isuser, const char **dpath, int load)
{
	char *dp;
	const char *uname, *uscope;

	/* get the scope */
	if (!j_read_string_at(appli, "unit-scope", &uscope)) {
		ERROR("'unit-scope' missing in appli description %s", json_object_get_string(appli));
		goto inval;
	}
	*isuser = strcmp(uscope, "system") != 0;

	/* get dpath */
	if (!j_read_string_at(appli, "-unit-dpath-", dpath)) {
		if (!load) {
			errno = ENOENT;
			goto error;
		}
		if (!j_read_string_at(appli, "unit-name", &uname)) {
			ERROR("'unit-name' missing in appli description %s", json_object_get_string(appli));
			goto inval;
		}
		dp = systemd_unit_dpath_by_name(*isuser, uname, 1);
		if (dp == NULL) {
			ERROR("Can't load unit of name %s for %s: %m", uname, uscope);
			goto error;
		}
		if (!j_add_string(appli, "-unit-dpath-", dp)) {
			free(dp);
			goto nomem;
		}
		free(dp);
		j_read_string_at(appli, "-unit-dpath-", dpath);
	}

	return 0;

nomem:
	ERROR("out of memory");
	errno = ENOMEM;
	goto error;

inval:
	errno = EINVAL;
error:
	return -1;
}

static const char *wait_state_stable(int isuser, const char *dpath)
{
	const char *state;

	for (;;) {
		state = systemd_unit_state_of_dpath(isuser, dpath);
		if (state == NULL || state == SysD_State_Active
		 || state == SysD_State_Failed)
			return state;
		/* TODO: sleep */
	}
}

/*
 * Creates a json object that describes the state for:
 *  - 'id', the id of the application
 *  - 'runid', its runid
 *  - 'pid', its pid
 *  - 'state', its systemd state
 *
 * Returns the created object or NULL in case of error.
 */
static json_object *mkstate(const char *id, int runid, int pid, const char *state)
{
	struct json_object *result, *pids;

	/* the structure */
	result = json_object_new_object();
	if (result == NULL)
		goto error;

	/* the runid */
	if (!j_add_integer(result, "runid", runid))
		goto error;

	/* the pids */
	if (pid > 0) {
		pids = j_add_new_array(result, "pids");
		if (!pids)
			goto error;
		if (!j_add_integer(pids, NULL, pid))
			goto error;
	}

	/* the state */
	if (!j_add_string(result, "state", state == SysD_State_Active ? "running" : "terminated"))
		goto error;

	/* the application id */
	if (!j_add_string(result, "id", id))
		goto error;

	/* done */
	return result;

error:
	json_object_put(result);
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
 * Returns the runid in case of success or -1 in case of error
 */
int afm_urun_start(struct json_object *appli)
{
	return afm_urun_once(appli);
}

/*
 * Returns the runid of a previously started application 'appli'
 * or if none is running, starts the application described by 'appli'
 * in local mode.
 *
 * A reference to 'appli' is kept during the live of the
 * runner. This is made using json_object_get. Thus be aware
 * that further modifications to 'appli' might create errors.
 *
 * Returns the runid in case of success or -1 in case of error
 */
int afm_urun_once(struct json_object *appli)
{
	const char *udpath, *state, *uscope, *uname;
	int rc, isuser;

	/* retrieve basis */
	rc = get_basis(appli, &isuser, &udpath, 1);
	if (rc < 0)
		goto error;

	/* start the unit */
	rc = systemd_unit_start_dpath(isuser, udpath);
	if (rc < 0) {
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		ERROR("can't start %s unit %s", uscope, uname);
		goto error;
	}

	state = wait_state_stable(isuser, udpath);
	if (state == NULL) {
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		ERROR("can't wait %s unit %s: %m", uscope, uname);
		goto error;
	}
	if (state != SysD_State_Active) {
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		ERROR("start error %s unit %s: %s", uscope, uname, state);
		goto error;
	}

	rc = systemd_unit_pid_of_dpath(isuser, udpath);
	if (rc < 0) {
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		ERROR("can't getpid of %s unit %s: %m", uscope, uname);
		goto error;
	}
		
	return rc;

error:
	return -1;
}

static int not_yet_implemented(const char *what)
{
	ERROR("%s isn't yet implemented", what);
	errno = ENOTSUP;
	return -1;
}

/*
 * Terminates the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_urun_terminate(int runid)
{
	int rc = systemd_unit_stop_pid(1 /* TODO: isuser? */, (unsigned)runid);
	return rc < 0 ? rc : 0;
}

/*
 * Stops (aka pause) the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_urun_pause(int runid)
{
	return not_yet_implemented("pause");
}

/*
 * Continue (aka resume) the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_urun_resume(int runid)
{
	return not_yet_implemented("resume");
}

/*
 * Get the list of the runners.
 *
 * Returns the list or NULL in case of error.
 */
struct json_object *afm_urun_list(struct afm_udb *db)
{
	int i, n, isuser, pid;
	const char *udpath;
	const char *id;
	const char *state;
	struct json_object *desc;
	struct json_object *appli;
	struct json_object *apps;
	struct json_object *result;

	apps = NULL;
	result = json_object_new_array();
	if (result == NULL)
		goto error;

	apps = afm_udb_applications_private(db);
	n = json_object_array_length(apps);
	for (i = 0 ; i < n ; i++) {
		appli = json_object_array_get_idx(apps, i);
		if (appli && get_basis(appli, &isuser, &udpath, 0) >= 0) {
			pid = systemd_unit_pid_of_dpath(isuser, udpath);
			if (pid > 0 && j_read_string_at(appli, "id", &id)) {
				state = systemd_unit_state_of_dpath(isuser, udpath);
				desc = mkstate(id, pid, pid, state);
				if (desc && json_object_array_add(result, desc) == -1) {
					ERROR("can't add desc %s to result", json_object_get_string(desc));
					json_object_put(desc);
				}
			}
		}
	}

error:
	json_object_put(apps);
	return result;
}

/*
 * Get the state of the runner of 'runid'.
 *
 * Returns the state or NULL in case of success
 */
struct json_object *afm_urun_state(struct afm_udb *db, int runid)
{
	int i, n, isuser, pid;
	char *dpath;
	const char *udpath;
	const char *id;
	const char *state;
	struct json_object *appli;
	struct json_object *apps;
	struct json_object *result;

	result = NULL;

	/* get the dpath */
	dpath = systemd_unit_dpath_by_pid(1 /* TODO: isuser? */, (unsigned)runid);
	if (!dpath) {
		result = NULL;
		errno = EINVAL;
		WARNING("searched runid %d not found", runid);
	} else {
		/* search in the base */
		apps = afm_udb_applications_private(db);
		n = json_object_array_length(apps);
		for (i = 0 ; i < n ; i++) {
			appli = json_object_array_get_idx(apps, i);
			if (appli
			 && get_basis(appli, &isuser, &udpath, 0) >= 0
			 && !strcmp(dpath, udpath)
			 && j_read_string_at(appli, "id", &id)) {
				pid = systemd_unit_pid_of_dpath(isuser, udpath);
				state = systemd_unit_state_of_dpath(isuser, dpath);
				result = mkstate(id, runid, pid, state);
				goto end;
			}
		}
		result = NULL;
		errno = ENOENT;
		WARNING("searched runid %d of dpath %s isn't an applications", runid, dpath);
end:
		json_object_put(apps);
		free(dpath);	
	}

	return result;
}


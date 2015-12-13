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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <json.h>

#include <wgt-info.h>

struct appfwk {
	int refcount;
	int nrroots;
	char **roots;
	struct json_object *applications;
};

struct appfwk *appfwk_create()
{
	struct appfwk *appfwk = malloc(sizeof * appfwk);
	if (!appfwk)
		errno = ENOMEM;
	else {
		appfwk->refcount = 1;
		appfwk->nrroots = 0;
		appfwk->roots = NULL;
		appfwk->applications = NULL;
	}
	return appfwk;
}

void appfwk_addref(struct appfwk *appfwk)
{
	assert(appfwk);
	appfwk->refcount++;
}

void appfwk_unref(struct appfwk *appfwk)
{
	assert(appfwk);
	if (!--appfwk->refcount) {
		while (appfwk->nrroots)
			free(appfwk->roots[--appfwk->nrroots]);
		free(appfwk->roots);
		free(appfwk);
	}
}

int appfwk_add_root(struct appfwk *appfwk, const char *path)
{
	int i, n;
	char *r, **roots;

	assert(appfwk);

	/* don't depend on the cwd and unique name */
	r = realpath(path, NULL);
	if (!r)
		return -1;

	/* avoiding duplications */
	n = appfwk->nrroots;
	roots = appfwk->roots;
	for (i = 0 ; i < n ; i++) {
		if (!strcmp(r, roots[i])) {
			free(r);
			return 0;
		}
	}

	/* add */
	roots = realloc(roots, (n + 1) * sizeof(roots[0]));
	if (!roots) {
		free(r);
		errno = ENOMEM;
		return -1;
	}
	roots[n++] = r;
	appfwk->roots = roots;
	appfwk->nrroots = n;
	return 0;
}


static int json_add(struct json_object *obj, const char *key, struct json_object *val)
{
	json_object_object_add(obj, key, val);
	return 0;
}

static int json_add_str(struct json_object *obj, const char *key, const char *val)
{
	struct json_object *str = json_object_new_string (val ? val : "");
	return str ? json_add(obj, key, str) : -1;
}

static int json_add_int(struct json_object *obj, const char *key, int val)
{
	struct json_object *v = json_object_new_int (val);
	return v ? json_add(obj, key, v) : -1;
}

static struct json_object *read_app_desc(const char *path)
{
	struct wgt_info *info;
	const struct wgt_desc *desc;
	struct json_object *result;
	char *appid, *end;

	result = json_object_new_object();
	if (!result)
		goto error;

	info = wgt_info_createat(AT_FDCWD, path, 0, 0, 0);
	if (info == NULL)
		goto error2;
	desc = wgt_info_desc(info);

	appid = alloca(2 + strlen(desc->id) + strlen(desc->version));
	end = stpcpy(appid, desc->id);
	*end++ = '@';
	strcpy(end, desc->version);

	if(json_add_str(result, "appid", appid)
	|| json_add_str(result, "id", desc->id)
	|| json_add_str(result, "version", desc->version)
	|| json_add_str(result, "path", path)
	|| json_add_int(result, "width", desc->width)
	|| json_add_int(result, "height", desc->height)
	|| json_add_str(result, "name", desc->name)
	|| json_add_str(result, "description", desc->description)
	|| json_add_str(result, "shortname", desc->name_short)
	|| json_add_str(result, "author", desc->author))
		goto error3;

	wgt_info_unref(info);
	return result;

error3:
	wgt_info_unref(info);
error2:
	json_object_put(result);
error:
	return NULL;
}

static int add_appdesc(struct json_object *appset, struct json_object *app)
{
	struct json_object *appid;

	if (!json_object_object_get_ex(app, "appid", &appid)) {
		errno = EINVAL;
		return -1;
	}

	return json_add(appset, json_object_get_string(appid), app);
}

struct enumdata {
	char path[PATH_MAX];
	int length;
	struct json_object *apps;
};

static int enumentries(struct enumdata *data, int (*callto)(struct enumdata *))
{
	DIR *dir;
	int rc;
	char *beg, *end;
	struct dirent entry, *e;

	/* opens the directory */
	dir = opendir(data->path);
	if (!dir)
		return -1;

	/* prepare appending entry names */
	beg = data->path + data->length;
	*beg++ = '/';

	/* enumerate entries */
	rc = readdir_r(dir, &entry, &e);
	while (!rc && e) {
		if (entry.d_name[0] != '.' || (entry.d_name[1] && (entry.d_name[1] != '.' || entry.d_name[2]))) {
			/* prepare callto */
			end = stpcpy(beg, entry.d_name);
			data->length = end - data->path;
			/* call the function */
			rc = callto(data);
			if (rc)
				break;
		}	
		rc = readdir_r(dir, &entry, &e);
	}
	closedir(dir);
	return rc;
}

static int recordapp(struct enumdata *data)
{
	struct json_object *app;

	app = read_app_desc(data->path);
	if (app != NULL) {
		if (!add_appdesc(data->apps, app))
			return 0;
		json_object_put(app);
	}
	return -1;
}

/* enumerate the versions */
static int enumvers(struct enumdata *data)
{
	int rc = enumentries(data, recordapp);
	return !rc || errno != ENOTDIR ? 0 : rc;
}

/* regenerate the list of applications */
int appfwk_update_applications(struct appfwk *af)
{
	int rc, iroot;
	struct enumdata edata;
	struct json_object *oldapps;

	/* create the result */
	edata.apps = json_object_new_object();
	if (edata.apps == NULL) {
		errno = ENOMEM;
		return -1;
	}
	/* for each root */
	for (iroot = 0 ; iroot < af->nrroots ; iroot++) {
		edata.length = stpcpy(edata.path, af->roots[iroot]) - edata.path;
		assert(edata.length < sizeof edata.path);
		/* enumerate the applications */
		rc = enumentries(&edata, enumvers);
		if (rc) {
			json_object_put(edata.apps);
			return rc;
		}
	}
	/* commit the result */
	oldapps = af->applications;
	af->applications = edata.apps;
	if (oldapps)
		json_object_put(oldapps);
	return 0;
}

int main()
{
struct appfwk *af = appfwk_create();
appfwk_add_root(af,"af/apps/");
appfwk_update_applications(af);
json_object_to_file("/dev/stdout", af->applications);
return 0;
}


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

struct afapps {
	struct json_object *pubarr;
	struct json_object *direct;
	struct json_object *byapp;
};

struct appfwk {
	int refcount;
	int nrroots;
	char **roots;
	struct afapps applications;
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
		appfwk->applications.pubarr = NULL;
		appfwk->applications.direct = NULL;
		appfwk->applications.byapp = NULL;
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

static int addapp(struct afapps *apps, const char *path)
{
	struct wgt_info *info;
	const struct wgt_desc *desc;
	const struct wgt_desc_feature *feat;
	struct json_object *priv = NULL, *pub, *bya, *plugs, *str;
	char *appid, *end;

	/* connect to the widget */
	info = wgt_info_createat(AT_FDCWD, path, 0, 1, 0);
	if (info == NULL)
		goto error;
	desc = wgt_info_desc(info);

	/* create the application id */
	appid = alloca(2 + strlen(desc->id) + strlen(desc->version));
	end = stpcpy(appid, desc->id);
	*end++ = '@';
	strcpy(end, desc->version);

	/* create the application structure */
	priv = json_object_new_object();
	if (!priv)
		goto error2;

	pub = json_object_new_object();
	if (!priv)
		goto error2;

	if (json_add(priv, "public", pub)) {
		json_object_put(pub);
		goto error2;
	}

	plugs = json_object_new_array();
	if (!priv)
		goto error2;

	if (json_add(priv, "plugins", plugs)) {
		json_object_put(plugs);
		goto error2;
	}

	if(json_add_str(pub, "id", appid)
	|| json_add_str(priv, "id", desc->id)
	|| json_add_str(pub, "version", desc->version)
	|| json_add_str(priv, "path", path)
	|| json_add_int(pub, "width", desc->width)
	|| json_add_int(pub, "height", desc->height)
	|| json_add_str(pub, "name", desc->name)
	|| json_add_str(pub, "description", desc->description)
	|| json_add_str(pub, "shortname", desc->name_short)
	|| json_add_str(pub, "author", desc->author))
		goto error2;

	feat = desc->features;
	while (feat) {
		static const char prefix[] = FWK_PREFIX_PLUGIN;
		if (!memcmp(feat->name, prefix, sizeof prefix - 1)) {
			str = json_object_new_string (feat->name + sizeof prefix - 1);
			if (str == NULL)
				goto error2;
			if (json_object_array_add(plugs, str)) {
				json_object_put(str);
				goto error2;
			}
		}
		feat = feat->next;
	}

	/* record the application structure */
	if (!json_object_object_get_ex(apps->byapp, desc->id, &bya)) {
		bya = json_object_new_object();
		if (!bya)
			goto error2;
		if (json_add(apps->byapp, desc->id, bya)) {
			json_object_put(bya);
			goto error2;
		}
	}

	if (json_add(apps->direct, appid, priv))
		goto error2;
	json_object_get(priv);

	if (json_add(bya, desc->version, priv)) {
		json_object_put(priv);
		goto error2;
	}

	if (json_object_array_add(apps->pubarr, pub))
		goto error2;

	wgt_info_unref(info);
	return 0;

error2:
	json_object_put(priv);
	wgt_info_unref(info);
error:
	return -1;
}

struct enumdata {
	char path[PATH_MAX];
	int length;
	struct afapps apps;
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
	return addapp(&data->apps, data->path);
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
	struct afapps oldapps;

	/* create the result */
	edata.apps.pubarr = json_object_new_array();
	edata.apps.direct = json_object_new_object();
	edata.apps.byapp = json_object_new_object();
	if (edata.apps.pubarr == NULL || edata.apps.direct == NULL || edata.apps.byapp == NULL) {
		errno = ENOMEM;
		goto error;
	}
	/* for each root */
	for (iroot = 0 ; iroot < af->nrroots ; iroot++) {
		edata.length = stpcpy(edata.path, af->roots[iroot]) - edata.path;
		assert(edata.length < sizeof edata.path);
		/* enumerate the applications */
		rc = enumentries(&edata, enumvers);
		if (rc)
			goto error;
	}
	/* commit the result */
	oldapps = af->applications;
	af->applications = edata.apps;
	json_object_put(oldapps.pubarr);
	json_object_put(oldapps.direct);
	json_object_put(oldapps.byapp);
	return 0;

error:
	json_object_put(edata.apps.pubarr);
	json_object_put(edata.apps.direct);
	json_object_put(edata.apps.byapp);
	return -1;
}

int appfwk_ensure_applications(struct appfwk *af)
{
	return af->applications.pubarr ? 0 : appfwk_update_applications(af);
}

/* regenerate the list of applications */
struct json_object *appfwk_application_list(struct appfwk *af)
{
	return appfwk_ensure_applications(af) ? NULL : af->applications.pubarr;
}

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


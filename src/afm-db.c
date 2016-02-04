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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <json.h>

#include "utils-json.h"
#include "wgt-info.h"
#include "afm-db.h"

struct afapps {
	struct json_object *pubarr;
	struct json_object *direct;
	struct json_object *byapp;
};

enum dir_type {
	type_root,
	type_app
};

struct afm_db_dir {
	struct afm_db_dir *next;
	char *path;
	enum dir_type type;
};

struct afm_db {
	int refcount;
	struct afm_db_dir *dirhead;
	struct afm_db_dir *dirtail;
	struct afapps applications;
};

struct afm_db *afm_db_create()
{
	struct afm_db *afdb = malloc(sizeof * afdb);
	if (afdb == NULL)
		errno = ENOMEM;
	else {
		afdb->refcount = 1;
		afdb->dirhead = NULL;
		afdb->dirtail = NULL;
		afdb->applications.pubarr = NULL;
		afdb->applications.direct = NULL;
		afdb->applications.byapp = NULL;
	}
	return afdb;
}

void afm_db_addref(struct afm_db *afdb)
{
	assert(afdb);
	afdb->refcount++;
}

void afm_db_unref(struct afm_db *afdb)
{
	struct afm_db_dir *dir;
	assert(afdb);
	if (!--afdb->refcount) {
		json_object_put(afdb->applications.pubarr);
		json_object_put(afdb->applications.direct);
		json_object_put(afdb->applications.byapp);
		while (afdb->dirhead != NULL) {
			dir = afdb->dirhead;
			afdb->dirhead = dir->next;
			free(dir->path);
			free(dir);
		}
		free(afdb);
	}
}

int add_dir(struct afm_db *afdb, const char *path, enum dir_type type)
{
	struct afm_db_dir *dir;
	char *r;

	assert(afdb);

	/* don't depend on the cwd and unique name */
	r = realpath(path, NULL);
	if (!r)
		return -1;

	/* avoiding duplications */
	dir = afdb->dirhead;
	while(dir != NULL && (strcmp(dir->path, path) || dir->type != type))
		dir = dir ->next;
	if (dir != NULL) {
		free(r);
		return 0;
	}

	/* allocates the structure */
	dir = malloc(sizeof * dir);
	if (dir == NULL) {
		free(r);
		errno = ENOMEM;
		return -1;
	}

	/* add */
	dir->next = NULL;
	dir->path = r;
	dir->type = type;
	if (afdb->dirtail == NULL)
		afdb->dirhead = dir;
	else
		afdb->dirtail->next = dir;
	afdb->dirtail = dir;
	return 0;
}

int afm_db_add_root(struct afm_db *afdb, const char *path)
{
	return add_dir(afdb, path, type_root);
}

int afm_db_add_application(struct afm_db *afdb, const char *path)
{
	return add_dir(afdb, path, type_app);
}

static int addapp(struct afapps *apps, const char *path)
{
	struct wgt_info *info;
	const struct wgt_desc *desc;
	const struct wgt_desc_feature *feat;
	struct json_object *priv = NULL, *pub, *bya, *plugs, *str;

	/* connect to the widget */
	info = wgt_info_createat(AT_FDCWD, path, 0, 1, 0);
	if (info == NULL) {
		if (errno == ENOENT)
			return 0; /* silently ignore bad directories */
		goto error;
	}
	desc = wgt_info_desc(info);

	/* create the application structure */
	priv = json_object_new_object();
	if (!priv)
		goto error2;

	pub = json_object_new_object();
	if (!priv)
		goto error2;

	if (!j_add(priv, "public", pub)) {
		json_object_put(pub);
		goto error2;
	}

	plugs = json_object_new_array();
	if (!priv)
		goto error2;

	if (!j_add(priv, "plugins", plugs)) {
		json_object_put(plugs);
		goto error2;
	}

	if(!j_add_string(priv, "id", desc->id)
	|| !j_add_string(priv, "path", path)
	|| !j_add_string(priv, "content", desc->content_src)
	|| !j_add_string(priv, "type", desc->content_type)
	|| !j_add_string(pub, "id", desc->idaver)
	|| !j_add_string(pub, "version", desc->version)
	|| !j_add_integer(pub, "width", desc->width)
	|| !j_add_integer(pub, "height", desc->height)
	|| !j_add_string(pub, "name", desc->name)
	|| !j_add_string(pub, "description", desc->description)
	|| !j_add_string(pub, "shortname", desc->name_short)
	|| !j_add_string(pub, "author", desc->author))
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
		if (!j_add(apps->byapp, desc->id, bya)) {
			json_object_put(bya);
			goto error2;
		}
	}

	if (!j_add(apps->direct, desc->idaver, priv))
		goto error2;
	json_object_get(priv);

	if (!j_add(bya, desc->version, priv)) {
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
int afm_db_update_applications(struct afm_db *afdb)
{
	int rc;
	struct enumdata edata;
	struct afapps oldapps;
	struct afm_db_dir *dir;

	/* create the result */
	edata.apps.pubarr = json_object_new_array();
	edata.apps.direct = json_object_new_object();
	edata.apps.byapp = json_object_new_object();
	if (edata.apps.pubarr == NULL || edata.apps.direct == NULL || edata.apps.byapp == NULL) {
		errno = ENOMEM;
		goto error;
	}
	/* for each root */
	for (dir = afdb->dirhead ; dir != NULL ; dir = dir->next) {
		if (dir->type == type_root) {
			edata.length = stpcpy(edata.path, dir->path) - edata.path;
			assert(edata.length < sizeof edata.path);
			/* enumerate the applications */
			rc = enumentries(&edata, enumvers);
			if (rc)
				goto error;
		} else {
			rc = addapp(&edata.apps, dir->path);
		}
	}
	/* commit the result */
	oldapps = afdb->applications;
	afdb->applications = edata.apps;
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

int afm_db_ensure_applications(struct afm_db *afdb)
{
	return afdb->applications.pubarr ? 0 : afm_db_update_applications(afdb);
}

struct json_object *afm_db_application_list(struct afm_db *afdb)
{
	return afm_db_ensure_applications(afdb) ? NULL : json_object_get(afdb->applications.pubarr);
}

struct json_object *afm_db_get_application(struct afm_db *afdb, const char *id)
{
	struct json_object *result;
	if (!afm_db_ensure_applications(afdb) && json_object_object_get_ex(afdb->applications.direct, id, &result))
		return json_object_get(result);
	return NULL;
}

struct json_object *afm_db_get_application_public(struct afm_db *afdb, const char *id)
{
	struct json_object *result = afm_db_get_application(afdb, id);
	return result && json_object_object_get_ex(result, "public", &result) ? json_object_get(result) : NULL;
}




#if defined(TESTAPPFWK)
#include <stdio.h>
int main()
{
struct afm_db *afdb = afm_db_create();
afm_db_add_root(afdb,FWK_APP_DIR);
afm_db_update_applications(afdb);
printf("array = %s\n", json_object_to_json_string_ext(afdb->applications.pubarr, 3));
printf("direct = %s\n", json_object_to_json_string_ext(afdb->applications.direct, 3));
printf("byapp = %s\n", json_object_to_json_string_ext(afdb->applications.byapp, 3));
return 0;
}
#endif


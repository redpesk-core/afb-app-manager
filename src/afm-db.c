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

/*
 * The structure afm_apps records the data about applications
 * for several accesses.
 */
struct afm_apps {
	struct json_object *pubarr; /* array of the public data of applications */
	struct json_object *direct; /* hash of applications by their id */
	struct json_object *byapp;  /* hash of versions of applications by their appid */
};

/*
 * Two types of directory are handled:
 *  - root directories: contains subdirectories appid/versio containing the applications
 *  - application directories: it contains an application
 */
enum dir_type {
	type_root, /* type for root directory */
	type_app   /* type for application directory */
};

/*
 * Structure for recording a path to application(s)
 * in the list of directories.
 */
struct afm_db_dir {
	struct afm_db_dir *next; /* link to the next item of the list */
	enum dir_type type;      /* the type of the path */
	char path[1];            /* the path of the directory */
};

/*
 * The structure afm_db records the applications
 * for a set of directories recorded as a linked list
 */
struct afm_db {
	int refcount;               /* count of references to the structure */
	struct afm_db_dir *dirhead; /* first directory of the set of directories */
	struct afm_db_dir *dirtail; /* last directory of the set of directories */
	struct afm_apps applications; /* the data about applications */
};

/*
 * The structure enumdata records data used when enumerating
 * application directories of a root directory.
 */
struct enumdata {
	char path[PATH_MAX];   /* "current" computed path */
	int length;            /* length of path */
	struct afm_apps apps;  /* */
};

/*
 * Release the data of the afm_apps object 'apps'.
 */
static void apps_put(struct afm_apps *apps)
{
	json_object_put(apps->pubarr);
	json_object_put(apps->direct);
	json_object_put(apps->byapp);
}

/*
 * Adds the application widget 'desc' of the directory 'path' to the
 * afm_apps object 'apps'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 */
static int addwgt(struct afm_apps *apps, const char *path, const struct wgt_desc *desc)
{
	const struct wgt_desc_feature *feat;
	struct json_object *priv, *pub, *bya, *plugs, *str;

	/* create the application structure */
	priv = json_object_new_object();
	if (!priv)
		return -1;

	pub = j_add_new_object(priv, "public");
	if (!pub)
		goto error;

	plugs = j_add_new_array(priv, "plugins");
	if (!plugs)
		goto error;

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
		goto error;

	/* extract plugins from features */
	feat = desc->features;
	while (feat) {
		static const char prefix[] = FWK_PREFIX_PLUGIN;
		if (!memcmp(feat->name, prefix, sizeof prefix - 1)) {
			str = json_object_new_string (feat->name + sizeof prefix - 1);
			if (str == NULL)
				goto error;
			if (json_object_array_add(plugs, str)) {
				json_object_put(str);
				goto error;
			}
		}
		feat = feat->next;
	}

	/* record the application structure */
	if (!j_add(apps->direct, desc->idaver, priv))
		goto error;

	if (json_object_array_add(apps->pubarr, pub))
		goto error;
	json_object_get(pub);

	if (!json_object_object_get_ex(apps->byapp, desc->id, &bya)) {
		bya = j_add_new_object(apps->byapp, desc->id);
		if (!bya)
			goto error;
	}

	if (!j_add(bya, desc->version, priv))
		goto error;
	json_object_get(priv);
	return 0;

error:
	json_object_put(priv);
	return -1;
}

/*
 * Adds the application widget in the directory 'path' to the
 * afm_apps object 'apps'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 */
static int addapp(struct afm_apps *apps, const char *path)
{
	int rc;
	struct wgt_info *info;

	/* connect to the widget */
	info = wgt_info_createat(AT_FDCWD, path, 0, 1, 0);
	if (info == NULL) {
		if (errno == ENOENT)
			return 0; /* silently ignore bad directories */
		return -1;
	}
	/* adds the widget */
	rc = addwgt(apps, path, wgt_info_desc(info));
	wgt_info_unref(info);
	return rc;
}

/*
 * Enumerate the directories designated by 'data' and call the
 * function 'callto' for each of them.
 */
static int enumentries(struct enumdata *data, int (*callto)(struct enumdata *))
{
	DIR *dir;
	int rc;
	char *beg;
	struct dirent entry, *e;
	size_t len;

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
			len = strlen(entry.d_name);
			if (beg + len >= data->path + sizeof data->path) {
				errno = ENAMETOOLONG;
				return -1;
			}
			data->length = stpcpy(beg, entry.d_name) - data->path;
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

/*
 * called for each version directory.
 */
static int recordapp(struct enumdata *data)
{
	return addapp(&data->apps, data->path);
}

/*
 * called for each application directory.
 * enumerate directories of the existing versions.
 */
static int enumvers(struct enumdata *data)
{
	int rc = enumentries(data, recordapp);
	return !rc || errno != ENOTDIR ? 0 : rc;
}

/*
 * Adds the directory of 'path' and 'type' to the afm_db object 'afdb'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 * Possible errno values: ENOMEM, ENAMETOOLONG
 */
static int add_dir(struct afm_db *afdb, const char *path, enum dir_type type)
{
	struct afm_db_dir *dir;
	size_t len;

	assert(afdb);

	/* check size */
	len = strlen(path);
	if (len >= PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	/* avoiding duplications */
	dir = afdb->dirhead;
	while(dir != NULL && (strcmp(dir->path, path) || dir->type != type))
		dir = dir ->next;
	if (dir != NULL)
		return 0;

	/* allocates the structure */
	dir = malloc(strlen(path) + sizeof * dir);
	if (dir == NULL) {
		errno = ENOMEM;
		return -1;
	}

	/* add it at tail */
	dir->next = NULL;
	dir->type = type;
	strcpy(dir->path, path);
	if (afdb->dirtail == NULL)
		afdb->dirhead = dir;
	else
		afdb->dirtail->next = dir;
	afdb->dirtail = dir;
	return 0;
}

/*
 * Creates an afm_db object and returns it with one reference added.
 * Return NULL with errno = ENOMEM if memory exhausted.
 */
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

/*
 * Adds a reference to an existing afm_db.
 */
void afm_db_addref(struct afm_db *afdb)
{
	assert(afdb);
	afdb->refcount++;
}

/*
 * Removes a reference to an existing afm_db object.
 * Removes the objet if there no more reference to it.
 */
void afm_db_unref(struct afm_db *afdb)
{
	struct afm_db_dir *dir;

	assert(afdb);
	if (!--afdb->refcount) {
		/* no more reference, clean the memory used by the object */
		apps_put(&afdb->applications);
		while (afdb->dirhead != NULL) {
			dir = afdb->dirhead;
			afdb->dirhead = dir->next;
			free(dir);
		}
		free(afdb);
	}
}

/*
 * Adds the root directory of 'path' to the afm_db object 'afdb'.
 * Be aware that no check is done on the directory of 'path' that will
 * only be used within calls to the function 'afm_db_update_applications'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 * Possible errno values: ENOMEM, ENAMETOOLONG
 */
int afm_db_add_root(struct afm_db *afdb, const char *path)
{
	return add_dir(afdb, path, type_root);
}

/*
 * Adds the application directory of 'path' to the afm_db object 'afdb'.
 * Be aware that no check is done on the directory of 'path' that will
 * only be used within calls to the function 'afm_db_update_applications'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 * Possible errno values: ENOMEM, ENAMETOOLONG
 */
int afm_db_add_application(struct afm_db *afdb, const char *path)
{
	return add_dir(afdb, path, type_app);
}

/*
 * Regenerate the list of applications of the afm_bd object 'afdb'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 */
int afm_db_update_applications(struct afm_db *afdb)
{
	int rc;
	struct enumdata edata;
	struct afm_apps oldapps;
	struct afm_db_dir *dir;

	/* create the result */
	edata.apps.pubarr = json_object_new_array();
	edata.apps.direct = json_object_new_object();
	edata.apps.byapp = json_object_new_object();
	if (edata.apps.pubarr == NULL || edata.apps.direct == NULL || edata.apps.byapp == NULL) {
		errno = ENOMEM;
		goto error;
	}
	/* for each directory of afdb */
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
	apps_put(&oldapps);
	return 0;

error:
	apps_put(&edata.apps);
	return -1;
}

/*
 * Ensure that applications of the afm_bd object 'afdb' are listed.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 */
int afm_db_ensure_applications(struct afm_db *afdb)
{
	return afdb->applications.pubarr ? 0 : afm_db_update_applications(afdb);
}

/*
 * Get the list of the applications public data of the afm_db object 'afdb'.
 * The list is returned as a JSON-array that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_db_application_list(struct afm_db *afdb)
{
	return afm_db_ensure_applications(afdb) ? NULL : json_object_get(afdb->applications.pubarr);
}

/*
 * Get the private data of the applications of 'id' in the afm_db object 'afdb'.
 * It returns a JSON-object that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_db_get_application(struct afm_db *afdb, const char *id)
{
	struct json_object *result;
	if (!afm_db_ensure_applications(afdb) && json_object_object_get_ex(afdb->applications.direct, id, &result))
		return json_object_get(result);
	return NULL;
}

/*
 * Get the public data of the applications of 'id' in the afm_db object 'afdb'.
 * It returns a JSON-object that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_db_get_application_public(struct afm_db *afdb, const char *id)
{
	struct json_object *result;
	struct json_object *priv = afm_db_get_application(afdb, id);
	if (priv == NULL)
		return NULL;
	if (json_object_object_get_ex(priv, "public", &result))
		json_object_get(result);
	else
		result = NULL;
	json_object_put(priv);
	return result;
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


/*
 Copyright (C) 2015-2025 IoT.bzh Company

 Author: José Bollo <jose.bollo@iot.bzh>

 $RP_BEGIN_LICENSE$
 Commercial License Usage
  Licensees holding valid commercial IoT.bzh licenses may use this file in
  accordance with the commercial license agreement provided with the
  Software or, alternatively, in accordance with the terms contained in
  a written agreement between you and The IoT.bzh Company. For licensing terms
  and conditions see https://www.iot.bzh/terms-conditions. For further
  information use the contact form at https://www.iot.bzh/contact.

 GNU General Public License Usage
  Alternatively, this file may be used under the terms of the GNU General
  Public license version 3. This license is as published by the Free Software
  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
  of this file. Please review the following information to ensure the GNU
  General Public License requirements will be met
  https://www.gnu.org/licenses/gpl-3.0.html.
 $RP_END_LICENSE$
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <json-c/json.h>

#include "utils-json.h"
#include "utils-systemd.h"
#include "unit-utils.h"
#include <rp-utils/rp-file.h>

#include "afm-udb.h"

static const char x_afm_prefix[] = "X-AFM-";
static const char service_extension[] = ".service";
static const char key_unit_path[] = "-unit-path";
static const char key_unit_name[] = "-unit-name";
static const char key_unit_scope[] = "-unit-scope";
static const char scope_user[] = "user";
static const char scope_system[] = "system";
static const char key_id[] = "id";
static const char key_visibility[] = "visibility";
static const char value_visible[] = "visible";

#define x_afm_prefix_length  (sizeof x_afm_prefix - 1)
#define service_extension_length  (sizeof service_extension - 1)

/*
 * The structure afm_apps records the data about applications
 * for several accesses.
 */
struct afm_apps {
	struct {
		struct json_object *visibles; /* array of the private data of visible apps */
		struct json_object *all; /* array of the private data of all apps */
		struct json_object *byname; /* hash of application's privates */
	} privates, publics;
};

/*
 * The structure afm_udb records the applications
 * for a set of directories recorded as a linked list
 */
struct afm_udb {
	struct afm_apps applications;	/* the data about applications */
	int refcount;			/* count of references to the structure */
	int system;			/* is managing system units? */
	int user;			/* is managing user units? */
	size_t prefixlen;		/* length of the prefix */
	char prefix[1];			/* filtering prefix */
};

/*
 * The structure afm_updt is internally used for updates
 */
struct afm_updt {
	struct afm_udb *afudb;
	struct afm_apps applications;
};

/*
 * The default language
 */
static char *default_lang;

/*
 * initilize object 'apps'.
 * returns 1 if okay or 0 on case of memory depletion
 */
static int apps_init(struct afm_apps *apps)
{
	apps->publics.all = json_object_new_array();
	apps->publics.visibles = json_object_new_array();
	apps->publics.byname = json_object_new_object();

	apps->privates.all = json_object_new_array();
	apps->privates.visibles = json_object_new_array();
	apps->privates.byname = json_object_new_object();

	return apps->publics.all
	   && apps->publics.visibles
	   && apps->publics.byname
	   && apps->privates.all
	   && apps->privates.visibles
	   && apps->privates.byname;
}

/*
 * Release the data of the afm_apps object 'apps'.
 */
static void apps_put(struct afm_apps *apps)
{
	json_object_put(apps->publics.all);
	json_object_put(apps->publics.visibles);
	json_object_put(apps->publics.byname);
	json_object_put(apps->privates.all);
	json_object_put(apps->privates.visibles);
	json_object_put(apps->privates.byname);
}

/*
 * Append the field 'data' to the field 'name' of the 'object'.
 * When a second append is done to one field, it is automatically
 * transformed to an array.
 * Return 0 in case of success or -1 in case of error.
 */
static int append_field(
		struct json_object *object,
		const char *name,
		struct json_object *data
)
{
	struct json_object *item, *array;

	if (!json_object_object_get_ex(object, name, &item))
		json_object_object_add(object, name, data);
	else {
		if (json_object_is_type(item, json_type_array))
			array = item;
		else {
			array = json_object_new_array();
			if (!array)
				goto error;
			json_object_array_add(array, json_object_get(item));
			json_object_object_add(object, name, array);
		}
		json_object_array_add(array, data);
	}
	return 0;
 error:
	json_object_put(data);
	errno = ENOMEM;
	return -1;
}

/*
 * Adds the field of 'name' and 'value' in 'priv' and also if possible in 'pub'
 * Returns 0 on success or -1 on error.
 */
static int add_field(
		struct json_object *priv,
		struct json_object *pub,
		const char *name,
		const char *value
)
{
	long int ival;
	char *end;
	struct json_object *v;

	/* try to adapt the value to its type */
	errno = 0;
	ival = strtol(value, &end, 10);
	if (*value && !*end && !errno) {
		/* integer value */
		v = json_object_new_int64(ival);
	} else {
		/* string value */
		v = json_object_new_string(value);
	}
	if (!v) {
		errno = ENOMEM;
		return -1;
	}

	/* add the value */
	if (name[0] == '-') {
		/* private value */
		append_field(priv, &name[1], v);
	} else {
		/* public value */
		append_field(priv, name, json_object_get(v));
		append_field(pub, name, v);
	}
	return 0;
}

/*
 * Adds the field of 'name' and 'value' in 'priv' and also if possible in 'pub'
 * Returns 0 on success or -1 on error.
 */
static int add_fields_of_content(
		struct json_object *priv,
		struct json_object *pub,
		char *content,
		size_t length
)
{
	char *name, *value, *read, *write;

	/* start at the beginning */
	read = content;
	for (;;) {
		/* search the next key */
		read = strstr(read, x_afm_prefix);
		if (!read)
			return 0;

		/* search to equal */
		name = read + x_afm_prefix_length;
		value = strchr(name, '=');
		if (value == NULL)
			read = name; /* not found */
		else {
			/* get the value (translate it) */
			*value++ = 0;
			read = write = value;
			while(*read && *read != '\n') {
				if (*read != '\\')
					*write++ = *read++;
				else {
					switch(*++read) {
					case 'n': *write++ = '\n'; break;
					case '\n': *write++ = ' '; break;
					default: *write++ = '\\'; *write++ = *read; break;
					}
					read += !!*read;
				}
			}
			read += !!*read;
			*write = 0;

			/* add the found field now */
			if (add_field(priv, pub, name, value) < 0)
				return -1;
		}
	}
}

/*
 * Adds the application widget 'desc' of the directory 'path' to the
 * afm_apps object 'apps'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 */
static int addunit(
		struct afm_apps *apps,
		int isuser,
		const char *unitpath,
		const char *unitname,
		char *content,
		size_t length
)
{
	struct json_object *priv, *pub, *id, *visi;
	const char *strid;
	size_t len;

	/* create the application structure */
	priv = json_object_new_object();
	if (!priv)
		return -1;

	pub = json_object_new_object();
	if (!pub)
		goto error;

	/* make the unit name */
	len = strlen(unitname);
	assert(len >= (sizeof service_extension - 1));
	assert(!memcmp(&unitname[len - (sizeof service_extension - 1)], service_extension, sizeof service_extension));

	/* adds the values */
	if (add_fields_of_content(priv, pub, content, length)
	 || add_field(priv, pub, key_unit_path, unitpath)
	 || add_field(priv, pub, key_unit_name, unitname)
	 || add_field(priv, pub, key_unit_scope, isuser ? scope_user : scope_system))
		goto error;

	/* get the id */
	if (!json_object_object_get_ex(pub, key_id, &id)) {
		errno = EINVAL;
		goto error;
	}
	strid = json_object_get_string(id);

	/* record the application structure */
	json_object_get(pub);
	json_object_array_add(apps->publics.all, pub);
	json_object_object_add(apps->publics.byname, strid, pub);
	json_object_get(priv);
	json_object_array_add(apps->privates.all, priv);
	json_object_object_add(apps->privates.byname, strid, priv);

	/* handle visibility */
	if (json_object_object_get_ex(priv, key_visibility, &visi)
		&& !strcasecmp(json_object_get_string(visi), value_visible)) {
		json_object_array_add(apps->publics.visibles, json_object_get(pub));
		json_object_array_add(apps->privates.visibles, json_object_get(priv));
	}

	return 0;

error:
	json_object_put(pub);
	json_object_put(priv);
	return -1;
}

/*
 * Crop and trim unit 'content' of 'length'. Return the new length.
 */
static size_t crop_and_trim_unit_content(char *content, size_t length)
{
	int st;
	char c, *read, *write;

	/* removes any comment and join continued lines */
	st = 0;
	read = write = content;
	for (;;) {
		do { c = *read++; } while (c == '\r');
		if (!c)
			break;
		switch (st) {
		case 0:
			/* state 0: begin of a line */
			if (c == ';' || c == '#') {
				st = 3; /* removes lines starting with ; or # */
				break;
			}
			if (c == '\n')
				break; /* removes empty lines */
enter_state_1:
			st = 1;
			/*@fallthrough@*/
		case 1:
			/* state 1: emitting a normal line */
			if (c == '\\')
				st = 2;
			else {
				*write++ = c;
				if (c == '\n')
					st = 0;
			}
			break;
		case 2:
			/* state 2: character after '\' */
			if (c == '\n')
				c = ' ';
			else
				*write++ = '\\';
			goto enter_state_1;
		case 3:
			/* state 3: inside a comment, wait its end */
			if (c == '\n')
				st = 0;
			break;
		}
	}
	if (st == 1)
		*write++ = '\n';
	*write = 0;
	return (size_t)(write - content);
}

/*
 * read a unit file
 */
static int read_unit_file(const char *path, char **content, size_t *length)
{
	int rc;
	size_t nl;

	/* read the file */
	rc = rp_file_get(path, content, length);
	if (rc >= 0) {
		/* crop and trim it */
		*length = nl = crop_and_trim_unit_content(*content, *length);
		*content = realloc(*content, nl + 1);
	}
	return rc;
}

/*
 * called for each unit
 */
static int update_cb(void *closure, const char *name, const char *path, int isuser)
{
	struct afm_updt *updt = closure;
	char *content;
	size_t length;
	int rc;

	/* prefix filtering */
	length = updt->afudb->prefixlen;
	if (length && strncmp(updt->afudb->prefix, name, length))
		return 0;

	/* only services */
	length = strlen(name);
	if (length < service_extension_length || strcmp(service_extension, name + length - service_extension_length))
		return 0;

	/* reads the file */
	rc = read_unit_file(path, &content, &length);
	if (rc < 0)
		return 0;

	/* process the file */
	rc = addunit(&updt->applications, isuser, path, name, content, length);
	/* TODO: if (rc < 0)
		RP_ERROR("Ignored boggus unit %s (error: %m)", path); */
	free(content);
	return 0;
}

/*
 * Creates an afm_udb object and returns it with one reference added.
 * Return NULL with errno = ENOMEM if memory exhausted.
 */
struct afm_udb *afm_udb_create(int sys, int usr, const char *prefix)
{
	size_t length;
	struct afm_udb *afudb;

	length = prefix ? strlen(prefix) : 0;
	afudb = malloc(length + sizeof * afudb);
	if (afudb == NULL)
		errno = ENOMEM;
	else {
		afudb->refcount = 1;
		memset(&afudb->applications, 0, sizeof afudb->applications);
		afudb->system = sys;
		afudb->user = usr;
		afudb->prefixlen = length;
		if (length)
			memcpy(afudb->prefix, prefix, length);
		afudb->prefix[length] = 0;
		if (afm_udb_update(afudb) < 0) {
			afm_udb_unref(afudb);
			afudb = NULL;
		}
	}
	return afudb;
}

/*
 * Adds a reference to an existing afm_udb.
 */
void afm_udb_addref(struct afm_udb *afudb)
{
	assert(afudb);
	afudb->refcount++;
}

/*
 * Removes a reference to an existing afm_udb object.
 * Removes the objet if there no more reference to it.
 */
void afm_udb_unref(struct afm_udb *afudb)
{
	assert(afudb);
	if (!--afudb->refcount) {
		/* no more reference, clean the memory used by the object */
		apps_put(&afudb->applications);
		free(afudb);
	}
}

/*
 * Regenerate the list of applications of the afm_bd object 'afudb'.
 * Returns 0 in case of success.
 * Returns -1 and set errno in case of error
 */
int afm_udb_update(struct afm_udb *afudb)
{
	struct afm_updt updt;
	struct afm_apps tmp;
	int result;

	/* lock the db */
	afm_udb_addref(afudb);
	updt.afudb = afudb;

	/* create the apps */
	if (!apps_init(&updt.applications))
		result = -1;
	else {
		/* scan the units */
		if (afudb->user && units_list(1, update_cb, &updt) < 0)
			result = -1;
		else if (afudb->system && units_list(0, update_cb, &updt) < 0)
			result = -1;
		else {
			/* commit the result */
			tmp = afudb->applications;
			afudb->applications = updt.applications;
			updt.applications = tmp;
			result = 0;
		}
		apps_put(&updt.applications);
	}
	/* unlock the db and return status */
	afm_udb_unref(afudb);
	return result;
}

/*
 * set the default language to 'lang'
 */
void afm_udb_set_default_lang(const char *lang)
{
	char *oldval = default_lang;
	default_lang = lang ? strdup(lang) : NULL;
	free(oldval);
}

/*
 * Get the list of the applications private data of the afm_udb object 'afudb'.
 * The list is returned as a JSON-array that must be released using
 * 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_udb_applications_private(struct afm_udb *afudb, int all, int uid)
{
	return json_object_get(all ? afudb->applications.privates.all : afudb->applications.privates.visibles);
}

/*
 * Get the list of the applications public data of the afm_udb object 'afudb'.
 * The list is returned as a JSON-array that must be released using
 * 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_udb_applications_public(struct afm_udb *afudb, int all, int uid, const char *lang)
{
	return json_object_get(all ? afudb->applications.publics.all : afudb->applications.publics.visibles);
}

/*
 * Get the private data of the applications of 'id' in the afm_udb object 'afudb'.
 * It returns a JSON-object that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
static struct json_object *get_no_case(struct json_object *object, const char *id, int uid, const char *lang)
{
	struct json_object *result;
	struct json_object_iter i;

	/* search case sensitively */
	if (json_object_object_get_ex(object, id, &result))
		return json_object_get(result);

	/* fallback to a case insensitive search */
	json_object_object_foreachC(object, i) {
		if (!strcasecmp(i.key, id))
			return json_object_get(i.val);
	}
	return NULL;
}

/*
 * Get the private data of the applications of 'id' in the afm_udb object 'afudb'.
 * It returns a JSON-object that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_udb_get_application_private(struct afm_udb *afudb, const char *id, int uid)
{
	return get_no_case(afudb->applications.privates.byname, id, uid, NULL);
}

/*
 * Get the public data of the applications of 'id' in the afm_udb object 'afudb'.
 * It returns a JSON-object that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_udb_get_application_public(struct afm_udb *afudb,
							const char *id, int uid, const char *lang)
{
	return get_no_case(afudb->applications.publics.byname, id, uid, lang);
}



#if defined(TESTAPPFWK)
#include <stdio.h>
int main()
{
struct afm_udb *afudb = afm_udb_create(1, 1, NULL);
printf("publics.all = %s\n", json_object_to_json_string_ext(afudb->applications.publics.all, 3));
printf("publics.byname = %s\n", json_object_to_json_string_ext(afudb->applications.publics.byname, 3));
printf("privates.byname = %s\n", json_object_to_json_string_ext(afudb->applications.privates.byname, 3));
return 0;
}
#endif


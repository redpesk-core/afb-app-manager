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
#include "utils-file.h"

#include "afm-udb.h"

static const char x_afm_prefix[] = "X-AFM-";
static const char service_extension[] = ".service";
static const char key_unit_path[] = "-unit-path";
static const char key_unit_name[] = "-unit-name";
static const char key_unit_scope[] = "-unit-scope";
static const char scope_user[] = "user";
static const char scope_system[] = "system";
static const char key_id[] = "id";

#define x_afm_prefix_length  (sizeof x_afm_prefix - 1)
#define service_extension_length  (sizeof service_extension - 1)

/*
 * The structure afm_apps records the data about applications
 * for several accesses.
 */
struct afm_apps {
	struct json_object *prvarr; /* array of the private data of apps */
	struct json_object *pubarr; /* array of the public data of apps */
	struct json_object *pubobj; /* hash of application's publics */
	struct json_object *prvobj; /* hash of application's privates */
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
 * Release the data of the afm_apps object 'apps'.
 */
static void apps_put(struct afm_apps *apps)
{
	json_object_put(apps->prvarr);
	json_object_put(apps->pubarr);
	json_object_put(apps->pubobj);
	json_object_put(apps->prvobj);
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
			json_object_array_add(array, item);
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
		append_field(priv, &name[1], v);
	} else {
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

	read = strstr(content, x_afm_prefix);
	while (read) {
		name = read + x_afm_prefix_length;
		value = strchr(name, '=');
		if (value == NULL)
			read = strstr(name, x_afm_prefix);
		else {
			*value++ = 0;
			read = write = value;
			while(*read && *read != '\n') {
				if (read[0] != '\\')
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
			read = strstr(read, x_afm_prefix);
			*write = 0;
			if (add_field(priv, pub, name, value) < 0)
				return -1;
		}
	}
	return 0;
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
	struct json_object *priv, *pub, *id;
	const char *strid;
	char *un = NULL;
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
	if (unitname[len - sizeof service_extension] == '@') {
		char buffer[40];
		size_t l = (size_t)snprintf(buffer, sizeof buffer, "%d", (int)getuid());
		un = malloc(len + l + 1);
		if (!un)
			goto error;
		memcpy(&un[0], unitname, len - (sizeof service_extension - 1));
		if (l)
			memcpy(&un[len - (sizeof service_extension - 1)], buffer, l);
		memcpy(&un[len - (sizeof service_extension - 1) + l], service_extension, sizeof service_extension);
	}

	/* adds the values */
	if (add_fields_of_content(priv, pub, content, length)
	 || add_field(priv, pub, key_unit_path, unitpath)
	 || add_field(priv, pub, key_unit_name, un ? : unitname)
	 || add_field(priv, pub, key_unit_scope, isuser ? scope_user : scope_system))
		goto error;
	free(un);
	un = NULL;

	/* get the id */
	if (!json_object_object_get_ex(pub, key_id, &id)) {
		errno = EINVAL;
		goto error;
	}
	strid = json_object_get_string(id);

	/* record the application structure */
	json_object_get(pub);
	json_object_array_add(apps->pubarr, pub);
	json_object_object_add(apps->pubobj, strid, pub);
	json_object_get(priv);
	json_object_array_add(apps->prvarr, priv);
	json_object_object_add(apps->prvobj, strid, priv);
	return 0;

error:
	free(un);
	json_object_put(pub);
	json_object_put(priv);
	return -1;
}

/*
 * read a unit file
 */
static int read_unit_file(const char *path, char **content, size_t *length)
{
	int rc, st;
	char c, *read, *write;

	/* read the file */
	rc = getfile(path, content, length);
	if (rc >= 0) {
		/* removes any comment and join lines */
		st = 0;
		read = write = *content;
		for (;;) {
			do { c = *read++; } while (c == '\r');
			if (!c)
				break;
			switch (st) {
			case 0:
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
				if (c == '\\')
					st = 2;
				else {
					*write++ = c;
					if (c == '\n')
						st = 0;
				}
				break;
			case 2:
				if (c == '\n')
					c = ' ';
				else
					*write++ = '\\';
				goto enter_state_1;
			case 3:
				if (c == '\n')
					st = 0;
				break;
			}
		}
		if (st == 1)
			*write++ = '\n';
		*write = 0;
		*length = (size_t)(write - *content);
		*content = realloc(*content, *length + 1);
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
		ERROR("Ignored boggus unit %s (error: %m)", path); */
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
		afudb->applications.prvarr = NULL;
		afudb->applications.pubarr = NULL;
		afudb->applications.pubobj = NULL;
		afudb->applications.prvobj = NULL;
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

	/* lock the db */
	afm_udb_addref(afudb);
	updt.afudb = afudb;

	/* create the result */
	updt.applications.prvarr = json_object_new_array();
	updt.applications.pubarr = json_object_new_array();
	updt.applications.pubobj = json_object_new_object();
	updt.applications.prvobj = json_object_new_object();
	if (updt.applications.pubarr == NULL
	 || updt.applications.prvarr == NULL
	 || updt.applications.pubobj == NULL
	 || updt.applications.prvobj == NULL) {
		errno = ENOMEM;
		goto error;
	}

	/* scan the units */
	if (afudb->user)
		if (systemd_unit_list(1, update_cb, &updt) < 0)
			goto error;
	if (afudb->system)
		if (systemd_unit_list(0, update_cb, &updt) < 0)
			goto error;

	/* commit the result */
	tmp = afudb->applications;
	afudb->applications = updt.applications;
	apps_put(&tmp);
	afm_udb_addref(afudb);
	return 0;

error:
	apps_put(&updt.applications);
	afm_udb_addref(afudb);
	return -1;
}

/*
 * Get the list of the applications private data of the afm_udb object 'afudb'.
 * The list is returned as a JSON-array that must be released using
 * 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_udb_applications_private(struct afm_udb *afudb)
{
	return json_object_get(afudb->applications.prvarr);
}

/*
 * Get the list of the applications public data of the afm_udb object 'afudb'.
 * The list is returned as a JSON-array that must be released using
 * 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_udb_applications_public(struct afm_udb *afudb)
{
	return json_object_get(afudb->applications.pubarr);
}

/*
 * Get the private data of the applications of 'id' in the afm_udb object 'afudb'.
 * It returns a JSON-object that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
static struct json_object *get_no_case(struct json_object *object, const char *id)
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
struct json_object *afm_udb_get_application_private(struct afm_udb *afudb, const char *id)
{
	return get_no_case(afudb->applications.prvobj, id);
}

/*
 * Get the public data of the applications of 'id' in the afm_udb object 'afudb'.
 * It returns a JSON-object that must be released using 'json_object_put'.
 * Returns NULL in case of error.
 */
struct json_object *afm_udb_get_application_public(struct afm_udb *afudb,
							const char *id)
{
	return get_no_case(afudb->applications.pubobj, id);
}



#if defined(TESTAPPFWK)
#include <stdio.h>
int main()
{
struct afm_udb *afudb = afm_udb_create(1, 1, NULL);
printf("array = %s\n", json_object_to_json_string_ext(afudb->applications.pubarr, 3));
printf("pubobj = %s\n", json_object_to_json_string_ext(afudb->applications.pubobj, 3));
printf("prvobj = %s\n", json_object_to_json_string_ext(afudb->applications.prvobj, 3));
return 0;
}
#endif


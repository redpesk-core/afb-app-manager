/*
 Copyright (C) 2015-2024 IoT.bzh Company

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

/*
 * predicates on types
 */
#define j_is_string(o)		(json_object_get_type(o) == json_type_string)
#define j_is_boolean(o)		(json_object_get_type(o) == json_type_boolean)
#define j_is_integer(o)		(json_object_get_type(o) == json_type_int)

/*
 * Read the value if object is of the good type and return true.
 * Returns false if the type is not correct.
 */
extern int j_read_string(struct json_object *obj, const char **value);
extern int j_read_boolean(struct json_object *obj, int *value);
extern int j_read_integer(struct json_object *obj, int *value);

/*
 * Get the value of the type or the default value if the type does not match.
 */
extern const char *j_string(struct json_object *obj, const char *defval);
extern int j_boolean(struct json_object *obj, int defval);
extern int j_integer(struct json_object *obj, int defval);

/*
 * Read the value of the entry of key in object if exist and of the good type and return true.
 * Returns false if the key does not exist or if the type is not correct.
 */
extern int j_has(struct json_object *obj, const char *key);
extern int j_read_object_at(struct json_object *obj, const char *key, struct json_object **value);
extern int j_read_string_at(struct json_object *obj, const char *key, const char **value);
extern int j_read_boolean_at(struct json_object *obj, const char *key, int *value);
extern int j_read_integer_at(struct json_object *obj, const char *key, int *value);

/*
 * Get the value of the key of type or the default value if the key or type does not match.
 */
extern const char *j_string_at(struct json_object *obj, const char *key, const char *defval);
extern int j_boolean_at(struct json_object *obj, const char *key, int defval);
extern int j_integer_at(struct json_object *obj, const char *key, int defval);

/*
 * Adds a keyed value (of type) and returns true if done or false in case of error
 * when key==NULL object is an array and values are appended
 */
extern int j_add(struct json_object *obj, const char *key, struct json_object *val);
extern int j_add_string(struct json_object *obj, const char *key, const char *val);
extern int j_add_boolean(struct json_object *obj, const char *key, int val);
extern int j_add_integer(struct json_object *obj, const char *key, int val);
extern struct json_object *j_add_new_array(struct json_object *obj, const char *key);
extern struct json_object *j_add_new_object(struct json_object *obj, const char *key);

/*
 * functions below interpret the key 'mkey' as a dot separated
 * path specification.
 */
extern int j_enter_m(struct json_object **obj, const char **mkey, int create);

extern int j_has_m(struct json_object *obj, const char *mkey);

extern int j_read_object_at_m(struct json_object *obj, const char *mkey, struct json_object **value);
extern int j_read_string_at_m(struct json_object *obj, const char *mkey, const char **value);
extern int j_read_boolean_at_m(struct json_object *obj, const char *mkey, int *value);
extern int j_read_integer_at_m(struct json_object *obj, const char *mkey, int *value);

extern const char *j_string_at_m(struct json_object *obj, const char *mkey, const char *defval);
extern int j_boolean_at_m(struct json_object *obj, const char *mkey, int defval);
extern int j_integer_at_m(struct json_object *obj, const char *mkey, int defval);

extern int j_add_m(struct json_object *obj, const char *mkey, struct json_object *val);
extern int j_add_string_m(struct json_object *obj, const char *mkey, const char *val);
extern int j_add_many_strings_m(struct json_object *obj, ...);
extern int j_add_boolean_m(struct json_object *obj, const char *mkey, int val);
extern int j_add_integer_m(struct json_object *obj, const char *mkey, int val);
extern struct json_object *j_add_new_array_m(struct json_object *obj, const char *mkey);
extern struct json_object *j_add_new_object_m(struct json_object *obj, const char *mkey);



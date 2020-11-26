/*
 Copyright (C) 2015-2020 IoT.bzh Company

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

#pragma once

#ifdef __cplusplus
    extern "C" {
#endif

#include <stdarg.h>
#include <json-c/json.h>

extern int wrap_json_get_error_position(int rc);
extern int wrap_json_get_error_code(int rc);
extern const char *wrap_json_get_error_string(int rc);

extern int wrap_json_vpack(struct json_object **result, const char *desc, va_list args);
extern int wrap_json_pack(struct json_object **result, const char *desc, ...);

extern int wrap_json_vunpack(struct json_object *object, const char *desc, va_list args);
extern int wrap_json_unpack(struct json_object *object, const char *desc, ...);
extern int wrap_json_vcheck(struct json_object *object, const char *desc, va_list args);
extern int wrap_json_check(struct json_object *object, const char *desc, ...);
extern int wrap_json_vmatch(struct json_object *object, const char *desc, va_list args);
extern int wrap_json_match(struct json_object *object, const char *desc, ...);

extern void wrap_json_optarray_for_all(struct json_object *object, void (*callback)(void*,struct json_object*), void *closure);
extern void wrap_json_array_for_all(struct json_object *object, void (*callback)(void*,struct json_object*), void *closure);
extern void wrap_json_object_for_all(struct json_object *object, void (*callback)(void*,struct json_object*,const char*), void *closure);
extern void wrap_json_optobject_for_all(struct json_object *object, void (*callback)(void*,struct json_object*,const char*), void *closure);
extern void wrap_json_for_all(struct json_object *object, void (*callback)(void*,struct json_object*,const char*), void *closure);

extern struct json_object *wrap_json_clone(struct json_object *object);
extern struct json_object *wrap_json_clone_deep(struct json_object *object);
extern struct json_object *wrap_json_clone_depth(struct json_object *object, int depth);

extern struct json_object *wrap_json_object_add(struct json_object *dest, struct json_object *added);

extern struct json_object *wrap_json_sort(struct json_object *array);
extern struct json_object *wrap_json_keys(struct json_object *object);
extern int wrap_json_cmp(struct json_object *x, struct json_object *y);
extern int wrap_json_equal(struct json_object *x, struct json_object *y);
extern int wrap_json_contains(struct json_object *x, struct json_object *y);

#ifdef __cplusplus
    }
#endif

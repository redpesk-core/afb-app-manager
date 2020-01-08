/*
 Copyright (C) 2015-2020 "IoT.bzh"

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

#include <string.h>
#include <limits.h>

#include "wrap-json.h"

#define STACKCOUNT  32
#define STRCOUNT    8

enum {
	wrap_json_error_none,
	wrap_json_error_null_object,
	wrap_json_error_truncated,
	wrap_json_error_internal_error,
	wrap_json_error_out_of_memory,
	wrap_json_error_invalid_character,
	wrap_json_error_too_long,
	wrap_json_error_too_deep,
	wrap_json_error_null_spec,
	wrap_json_error_null_key,
	wrap_json_error_null_string,
	wrap_json_error_out_of_range,
	wrap_json_error_incomplete,
	wrap_json_error_missfit_type,
	wrap_json_error_key_not_found,
	wrap_json_error_bad_base64,
	_wrap_json_error_count_
};

static const char ignore_all[] = " \t\n\r,:";
static const char pack_accept_arr[] = "][{snbiIfoOyY";
static const char pack_accept_key[] = "s}";
#define pack_accept_any (&pack_accept_arr[1])

static const char unpack_accept_arr[] = "*!][{snbiIfFoOyY";
static const char unpack_accept_key[] = "*!s}";
#define unpack_accept_any (&unpack_accept_arr[3])

static const char *pack_errors[_wrap_json_error_count_] =
{
	[wrap_json_error_none] = "unknown error",
	[wrap_json_error_null_object] = "null object",
	[wrap_json_error_truncated] = "truncated",
	[wrap_json_error_internal_error] = "internal error",
	[wrap_json_error_out_of_memory] = "out of memory",
	[wrap_json_error_invalid_character] = "invalid character",
	[wrap_json_error_too_long] = "too long",
	[wrap_json_error_too_deep] = "too deep",
	[wrap_json_error_null_spec] = "spec is NULL",
	[wrap_json_error_null_key] = "key is NULL",
	[wrap_json_error_null_string] = "string is NULL",
	[wrap_json_error_out_of_range] = "array too small",
	[wrap_json_error_incomplete] = "incomplete container",
	[wrap_json_error_missfit_type] = "missfit of type",
	[wrap_json_error_key_not_found] = "key not found",
	[wrap_json_error_bad_base64] = "bad base64 encoding"
};

int wrap_json_get_error_position(int rc)
{
	if (rc < 0)
		rc = -rc;
	return (rc >> 4) + 1;
}

int wrap_json_get_error_code(int rc)
{
	if (rc < 0)
		rc = -rc;
	return rc & 15;
}

const char *wrap_json_get_error_string(int rc)
{
	rc = wrap_json_get_error_code(rc);
	if (rc >= (int)(sizeof pack_errors / sizeof *pack_errors))
		rc = 0;
	return pack_errors[rc];
}

static int encode_base64(
		const uint8_t *data,
		size_t datalen,
		char **encoded,
		size_t *encodedlen,
		int width,
		int pad,
		int url)
{
	uint16_t u16 = 0;
	uint8_t u8 = 0;
	size_t in, out, rlen, n3, r3, iout, nout;
	int iw;
	char *result, c;

	/* compute unformatted output length */
	n3 = datalen / 3;
	r3 = datalen % 3;
	nout = 4 * n3 + r3 + !!r3;

	/* deduce formatted output length */
	rlen = nout;
	if (pad)
		rlen += ((~rlen) + 1) & 3;
	if (width)
		rlen += rlen / width;

	/* allocate the output */
	result = malloc(rlen + 1);
	if (result == NULL)
		return wrap_json_error_out_of_memory;

	/* compute the formatted output */
	iw = width;
	for (in = out = iout = 0 ; iout < nout ; iout++) {
		/* get in 'u8' the 6 bits value to add */
		switch (iout & 3) {
		case 0:
			u16 = (uint16_t)data[in++];
			u8 = (uint8_t)(u16 >> 2);
			break;
		case 1:
			u16 = (uint16_t)(u16 << 8);
			if (in < datalen)
				u16 = (uint16_t)(u16 | data[in++]);
			u8 = (uint8_t)(u16 >> 4);
			break;
		case 2:
			u16 = (uint16_t)(u16 << 8);
			if (in < datalen)
				u16 = (uint16_t)(u16 | data[in++]);
			u8 = (uint8_t)(u16 >> 6);
			break;
		case 3:
			u8 = (uint8_t)u16;
			break;
		}
		u8 &= 63;

		/* encode 'u8' to the char 'c' */
		if (u8 < 52) {
			if (u8 < 26)
				c = (char)('A' + u8);
			else
				c = (char)('a' + u8 - 26);
		} else {
			if (u8 < 62)
				c = (char)('0' + u8 - 52);
			else if (u8 == 62)
				c = url ? '-' : '+';
			else
				c = url ? '_' : '/';
		}

		/* put to output with format */
		result[out++] = c;
		if (iw && !--iw) {
			result[out++] = '\n';
			iw = width;
		}
	}

	/* pad the output */
	while (out < rlen) {
		result[out++] = '=';
		if (iw && !--iw) {
			result[out++] = '\n';
			iw = width;
		}
	}

	/* terminate */
	result[out] = 0;
	*encoded = result;
	*encodedlen = rlen;
	return 0;
}

static int decode_base64(
		const char *data,
		size_t datalen,
		uint8_t **decoded,
		size_t *decodedlen,
		int url)
{
	uint16_t u16;
	uint8_t u8, *result;
	size_t in, out, iin;
	char c;

	/* allocate enougth output */
	result = malloc(datalen);
	if (result == NULL)
		return wrap_json_error_out_of_memory;

	/* decode the input */
	for (iin = in = out = 0 ; in < datalen ; in++) {
		c = data[in];
		if (c != '\n' && c != '\r' && c != '=') {
			if ('A' <= c && c <= 'Z')
				u8 = (uint8_t)(c - 'A');
			else if ('a' <= c && c <= 'z')
				u8 = (uint8_t)(c - 'a' + 26);
			else if ('0' <= c && c <= '9')
				u8 = (uint8_t)(c - '0' + 52);
			else if (c == '+' || c == '-')
				u8 = (uint8_t)62;
			else if (c == '/' || c == '_')
				u8 = (uint8_t)63;
			else {
				free(result);
				return wrap_json_error_bad_base64;
			}
			if (!iin) {
				u16 = (uint16_t)u8;
				iin = 6;
			} else {
				u16 = (uint16_t)((u16 << 6) | u8);
				iin -= 2;
				u8 = (uint8_t)(u16 >> iin);
				result[out++] = u8;
			}
		}
	}

	/* terminate */
	*decoded = realloc(result, out);
	if (out && *decoded == NULL) {
		free(result);
		return wrap_json_error_out_of_memory;
	}
	*decodedlen = out;
	return 0;
}

static inline const char *skip(const char *d)
{
	while (*d && strchr(ignore_all, *d))
		d++;
	return d;
}

int wrap_json_vpack(struct json_object **result, const char *desc, va_list args)
{
	/* TODO: the case of structs with key being single char should be optimized */
	int nstr, notnull, nullable, rc;
	size_t sz, dsz, ssz;
	char *s;
	char c;
	const char *d;
	char buffer[256];
	struct { const uint8_t *in; size_t insz; char *out; size_t outsz; } bytes;
	struct { const char *str; size_t sz; } strs[STRCOUNT];
	struct { struct json_object *cont, *key; const char *acc; char type; } stack[STACKCOUNT], *top;
	struct json_object *obj;

	ssz = sizeof buffer;
	s = buffer;
	top = stack;
	top->key = NULL;
	top->cont = NULL;
	top->acc = pack_accept_any;
	top->type = 0;
	d = desc;
	if (!d)
		goto null_spec;
	d = skip(d);
	for(;;) {
		c = *d;
		if (!c)
			goto truncated;
		if (!strchr(top->acc, c))
			goto invalid_character;
		d = skip(d + 1);
		switch(c) {
		case 's':
			nullable = 0;
			notnull = 0;
			nstr = 0;
			sz = 0;
			for (;;) {
				strs[nstr].str = va_arg(args, const char*);
				if (strs[nstr].str)
					notnull = 1;
				if (*d == '?') {
					d = skip(d + 1);
					nullable = 1;
				}
				switch(*d) {
				case '%': strs[nstr].sz = va_arg(args, size_t); d = skip(d + 1); break;
				case '#': strs[nstr].sz = (size_t)va_arg(args, int); d = skip(d + 1); break;
				default: strs[nstr].sz = strs[nstr].str ? strlen(strs[nstr].str) : 0; break;
				}
				sz += strs[nstr++].sz;
				if (*d == '?') {
					d = skip(d + 1);
					nullable = 1;
				}
				if (*d != '+')
					break;
				if (nstr >= STRCOUNT)
					goto too_long;
				d = skip(d + 1);
			}
			if (*d == '*')
				nullable = 1;
			if (notnull) {
				if (sz > ssz) {
					ssz += ssz;
					if (ssz < sz)
						ssz = sz;
					s = alloca(sz);
				}
				dsz = sz;
				while (nstr) {
					nstr--;
					dsz -= strs[nstr].sz;
					memcpy(&s[dsz], strs[nstr].str, strs[nstr].sz);
				}
				obj = json_object_new_string_len(s, (int)sz);
				if (!obj)
					goto out_of_memory;
			} else if (nullable)
				obj = NULL;
			else
				goto null_string;
			break;
		case 'n':
			obj = NULL;
			break;
		case 'b':
			obj = json_object_new_boolean(va_arg(args, int));
			if (!obj)
				goto out_of_memory;
			break;
		case 'i':
			obj = json_object_new_int(va_arg(args, int));
			if (!obj)
				goto out_of_memory;
			break;
		case 'I':
			obj = json_object_new_int64(va_arg(args, int64_t));
			if (!obj)
				goto out_of_memory;
			break;
		case 'f':
			obj = json_object_new_double(va_arg(args, double));
			if (!obj)
				goto out_of_memory;
			break;
		case 'o':
		case 'O':
			obj = va_arg(args, struct json_object*);
			if (*d == '?')
				d = skip(d + 1);
			else if (*d != '*' && !obj)
				goto null_object;
			if (c == 'O')
				json_object_get(obj);
			break;
		case 'y':
		case 'Y':
			bytes.in = va_arg(args, const uint8_t*);
			bytes.insz = va_arg(args, size_t);
			if (bytes.in == NULL || bytes.insz == 0)
				obj = NULL;
			else {
				rc = encode_base64(bytes.in, bytes.insz,
					&bytes.out, &bytes.outsz, 0, 0, c == 'y');
				if (rc)
					goto error;
				obj = json_object_new_string_len(bytes.out, (int)bytes.outsz);
				free(bytes.out);
				if (!obj)
					goto out_of_memory;
			}
			if (*d == '?')
				d = skip(d + 1);
			else if (*d != '*' && !obj) {
				obj = json_object_new_string_len(d, 0);
				if (!obj)
					goto out_of_memory;
			}
			break;
		case '[':
		case '{':
			if (++top >= &stack[STACKCOUNT])
				goto too_deep;
			top->key = NULL;
			if (c == '[') {
				top->type = ']';
				top->acc = pack_accept_arr;
				top->cont = json_object_new_array();
			} else {
				top->type = '}';
				top->acc = pack_accept_key;
				top->cont = json_object_new_object();
			}
			if (!top->cont)
				goto out_of_memory;
			continue;
		case '}':
		case ']':
			if (c != top->type || top <= stack)
				goto internal_error;
			obj = (top--)->cont;
			if (*d == '*' && !(c == '}' ? json_object_object_length(obj) : json_object_array_length(obj))) {
				json_object_put(obj);
				obj = NULL;
			}
			break;
		default:
			goto internal_error;
		}
		switch (top->type) {
		case 0:
			if (top != stack)
				goto internal_error;
			if (*d)
				goto invalid_character;
			*result = obj;
			return 0;
		case ']':
			if (obj || *d != '*')
				json_object_array_add(top->cont, obj);
			if (*d == '*')
				d = skip(d + 1);
			break;
		case '}':
			if (!obj)
				goto null_key;
			top->key = obj;
			top->acc = pack_accept_any;
			top->type = ':';
			break;
		case ':':
			if (obj || *d != '*')
				json_object_object_add(top->cont, json_object_get_string(top->key), obj);
			if (*d == '*')
				d = skip(d + 1);
			json_object_put(top->key);
			top->key = NULL;
			top->acc = pack_accept_key;
			top->type = '}';
			break;
		default:
			goto internal_error;
		}
	}

null_object:
	rc = wrap_json_error_null_object;
	goto error;
truncated:
	rc = wrap_json_error_truncated;
	goto error;
internal_error:
	rc = wrap_json_error_internal_error;
	goto error;
out_of_memory:
	rc = wrap_json_error_out_of_memory;
	goto error;
invalid_character:
	rc = wrap_json_error_invalid_character;
	goto error;
too_long:
	rc = wrap_json_error_too_long;
	goto error;
too_deep:
	rc = wrap_json_error_too_deep;
	goto error;
null_spec:
	rc = wrap_json_error_null_spec;
	goto error;
null_key:
	rc = wrap_json_error_null_key;
	goto error;
null_string:
	rc = wrap_json_error_null_string;
	goto error;
error:
	do {
		json_object_put(top->key);
		json_object_put(top->cont);
	} while (--top >= stack);
	*result = NULL;
	rc = rc | (int)((d - desc) << 4);
	return -rc;
}

int wrap_json_pack(struct json_object **result, const char *desc, ...)
{
	int rc;
	va_list args;

	va_start(args, desc);
	rc = wrap_json_vpack(result, desc, args);
	va_end(args);
	return rc;
}

static int vunpack(struct json_object *object, const char *desc, va_list args, int store)
{
	int rc = 0, optionnal, ignore;
	char c, xacc[2] = { 0, 0 };
	const char *acc;
	const char *d, *fit = NULL;
	const char *key = NULL;
	const char **ps = NULL;
	double *pf = NULL;
	int *pi = NULL;
	int64_t *pI = NULL;
	size_t *pz = NULL;
	uint8_t **py = NULL;
	struct { struct json_object *parent; const char *acc; int index; int count; char type; } stack[STACKCOUNT], *top;
	struct json_object *obj;
	struct json_object **po;

	xacc[0] = 0;
	ignore = 0;
	top = NULL;
	acc = unpack_accept_any;
	d = desc;
	if (!d)
		goto null_spec;
	d = skip(d);
	obj = object;
	for(;;) {
		fit = d;
		c = *d;
		if (!c)
			goto truncated;
		if (!strchr(acc, c))
			goto invalid_character;
		d = skip(d + 1);
		switch(c) {
		case 's':
			if (xacc[0] == '}') {
				/* expects a key */
				key = va_arg(args, const char *);
				if (!key)
					goto null_key;
				if (*d != '?')
					optionnal = 0;
				else {
					optionnal = 1;
					d = skip(d + 1);
				}
				if (ignore)
					ignore++;
				else {
					if (json_object_object_get_ex(top->parent, key, &obj)) {
						/* found */
						top->index++;
					} else {
						/* not found */
						if (!optionnal)
							goto key_not_found;
						ignore = 1;
						obj = NULL;
					}
				}
				xacc[0] = ':';
				acc = unpack_accept_any;
				continue;
			}
			/* get a string */
			if (store)
				ps = va_arg(args, const char **);
			if (!ignore) {
				if (!json_object_is_type(obj, json_type_string))
					goto missfit;
				if (store && ps)
					*ps = json_object_get_string(obj);
			}
			if (*d == '%') {
				d = skip(d + 1);
				if (store) {
					pz = va_arg(args, size_t *);
					if (!ignore && pz)
						*pz = (size_t)json_object_get_string_len(obj);
				}
			}
			break;
		case 'n':
			if (!ignore && !json_object_is_type(obj, json_type_null))
				goto missfit;
			break;
		case 'b':
			if (store)
				pi = va_arg(args, int *);

			if (!ignore) {
				if (!json_object_is_type(obj, json_type_boolean))
					goto missfit;
				if (store && pi)
					*pi = json_object_get_boolean(obj);
			}
			break;
		case 'i':
			if (store)
				pi = va_arg(args, int *);

			if (!ignore) {
				if (!json_object_is_type(obj, json_type_int))
					goto missfit;
				if (store && pi)
					*pi = json_object_get_int(obj);
			}
			break;
		case 'I':
			if (store)
				pI = va_arg(args, int64_t *);

			if (!ignore) {
				if (!json_object_is_type(obj, json_type_int))
					goto missfit;
				if (store && pI)
					*pI = json_object_get_int64(obj);
			}
			break;
		case 'f':
		case 'F':
			if (store)
				pf = va_arg(args, double *);

			if (!ignore) {
				if (!(json_object_is_type(obj, json_type_double) || (c == 'F' && json_object_is_type(obj, json_type_int))))
					goto missfit;
				if (store && pf)
					*pf = json_object_get_double(obj);
			}
			break;
		case 'o':
		case 'O':
			if (store) {
				po = va_arg(args, struct json_object **);
				if (!ignore && po) {
					if (c == 'O')
						obj = json_object_get(obj);
					*po = obj;
				}
			}
			break;
		case 'y':
		case 'Y':
			if (store) {
				py = va_arg(args, uint8_t **);
				pz = va_arg(args, size_t *);
			}
			if (!ignore) {
				if (obj == NULL) {
					if (store && py && pz) {
						*py = NULL;
						*pz = 0;
					}
				} else {
					if (!json_object_is_type(obj, json_type_string))
						goto missfit;
					if (store && py && pz) {
						rc = decode_base64(
							json_object_get_string(obj),
							(size_t)json_object_get_string_len(obj),
							py, pz, c == 'y');
						if (rc)
							goto error;
					}
				}
			}
			break;

		case '[':
		case '{':
			if (!top)
				top = stack;
			else if (++top  >= &stack[STACKCOUNT])
				goto too_deep;

			top->acc = acc;
			top->type = xacc[0];
			top->index = 0;
			top->parent = obj;
			if (ignore)
				ignore++;
			if (c == '[') {
				if (!ignore) {
					if (!json_object_is_type(obj, json_type_array))
						goto missfit;
					top->count = (int)json_object_array_length(obj);
				}
				xacc[0] = ']';
				acc = unpack_accept_arr;
			} else {
				if (!ignore) {
					if (!json_object_is_type(obj, json_type_object))
						goto missfit;
					top->count = json_object_object_length(obj);
				}
				xacc[0] = '}';
				acc = unpack_accept_key;
				continue;
			}
			break;
		case '}':
		case ']':
			if (!top || c != xacc[0])
				goto internal_error;
			acc = top->acc;
			xacc[0] = top->type;
			top = top == stack ? NULL : top - 1;
			if (ignore)
				ignore--;
			break;
		case '!':
			if (*d != xacc[0])
				goto invalid_character;
			if (!ignore && top->index != top->count)
				goto incomplete;
			/*@fallthrough@*/
		case '*':
			acc = xacc;
			continue;
		default:
			goto internal_error;
		}
		switch (xacc[0]) {
		case 0:
			if (top)
				goto internal_error;
			if (*d)
				goto invalid_character;
			return 0;
		case ']':
			if (!ignore) {
				key = strchr(unpack_accept_arr, *d);
				if (key && key >= unpack_accept_any) {
					if (top->index >= top->count)
						goto out_of_range;
					obj = json_object_array_get_idx(top->parent, top->index++);
				}
			}
			break;
		case ':':
			acc = unpack_accept_key;
			xacc[0] = '}';
			if (ignore)
				ignore--;
			break;
		default:
			goto internal_error;
		}
	}
truncated:
	rc = wrap_json_error_truncated;
	goto error;
internal_error:
	rc = wrap_json_error_internal_error;
	goto error;
invalid_character:
	rc = wrap_json_error_invalid_character;
	goto error;
too_deep:
	rc = wrap_json_error_too_deep;
	goto error;
null_spec:
	rc = wrap_json_error_null_spec;
	goto error;
null_key:
	rc = wrap_json_error_null_key;
	goto error;
out_of_range:
	rc = wrap_json_error_out_of_range;
	goto error;
incomplete:
	rc = wrap_json_error_incomplete;
	goto error;
missfit:
	rc = wrap_json_error_missfit_type;
	goto errorfit;
key_not_found:
	rc = wrap_json_error_key_not_found;
	goto error;
errorfit:
	d = fit;
error:
	rc = rc | (int)((d - desc) << 4);
	return -rc;
}

int wrap_json_vcheck(struct json_object *object, const char *desc, va_list args)
{
	return vunpack(object, desc, args, 0);
}

int wrap_json_check(struct json_object *object, const char *desc, ...)
{
	int rc;
	va_list args;

	va_start(args, desc);
	rc = wrap_json_vcheck(object, desc, args);
	va_end(args);
	return rc;
}

int wrap_json_vmatch(struct json_object *object, const char *desc, va_list args)
{
	return !vunpack(object, desc, args, 0);
}

int wrap_json_match(struct json_object *object, const char *desc, ...)
{
	int rc;
	va_list args;

	va_start(args, desc);
	rc = wrap_json_vmatch(object, desc, args);
	va_end(args);
	return rc;
}

int wrap_json_vunpack(struct json_object *object, const char *desc, va_list args)
{
	return vunpack(object, desc, args, 1);
}

int wrap_json_unpack(struct json_object *object, const char *desc, ...)
{
	int rc;
	va_list args;

	va_start(args, desc);
	rc = vunpack(object, desc, args, 1);
	va_end(args);
	return rc;
}

static void object_for_all(struct json_object *object, void (*callback)(void*,struct json_object*,const char*), void *closure)
{
	struct json_object_iterator it = json_object_iter_begin(object);
	struct json_object_iterator end = json_object_iter_end(object);
	while (!json_object_iter_equal(&it, &end)) {
		callback(closure, json_object_iter_peek_value(&it), json_object_iter_peek_name(&it));
		json_object_iter_next(&it);
	}
}

static void array_for_all(struct json_object *object, void (*callback)(void*,struct json_object*), void *closure)
{
	int n = (int)json_object_array_length(object);
	int i = 0;
	while(i < n)
		callback(closure, json_object_array_get_idx(object, i++));
}

void wrap_json_optarray_for_all(struct json_object *object, void (*callback)(void*,struct json_object*), void *closure)
{
	if (json_object_is_type(object, json_type_array))
		array_for_all(object, callback, closure);
	else
		callback(closure, object);
}

void wrap_json_array_for_all(struct json_object *object, void (*callback)(void*,struct json_object*), void *closure)
{
	if (json_object_is_type(object, json_type_array))
		array_for_all(object, callback, closure);
}

void wrap_json_object_for_all(struct json_object *object, void (*callback)(void*,struct json_object*,const char*), void *closure)
{
	if (json_object_is_type(object, json_type_object))
		object_for_all(object, callback, closure);
}

void wrap_json_optobject_for_all(struct json_object *object, void (*callback)(void*,struct json_object*,const char*), void *closure)
{
	if (json_object_is_type(object, json_type_object))
		object_for_all(object, callback, closure);
	else
		callback(closure, object, NULL);
}

void wrap_json_for_all(struct json_object *object, void (*callback)(void*,struct json_object*,const char*), void *closure)
{
	if (!object)
		/* do nothing */;
	else if (json_object_is_type(object, json_type_object))
		object_for_all(object, callback, closure);
	else if (!json_object_is_type(object, json_type_array))
		callback(closure, object, NULL);
	else {
		int n = (int)json_object_array_length(object);
		int i = 0;
		while(i < n)
			callback(closure, json_object_array_get_idx(object, i++), NULL);
	}
}

/**
 * Clones the 'object' for the depth 'subdepth'. The object 'object' is
 * duplicated and all its fields are cloned with the depth 'subdepth'.
 *
 * @param object the object to clone. MUST be an **object**.
 * @param subdepth the depth to use when cloning the fields of the object.
 *
 * @return the cloned object.
 */
static struct json_object *clone_object(struct json_object *object, int subdepth)
{
	struct json_object *r = json_object_new_object();
	struct json_object_iterator it = json_object_iter_begin(object);
	struct json_object_iterator end = json_object_iter_end(object);
	while (!json_object_iter_equal(&it, &end)) {
		json_object_object_add(r,
			json_object_iter_peek_name(&it),
			wrap_json_clone_depth(json_object_iter_peek_value(&it), subdepth));
		json_object_iter_next(&it);
	}
	return r;
}

/**
 * Clones the 'array' for the depth 'subdepth'. The array 'array' is
 * duplicated and all its fields are cloned with the depth 'subdepth'.
 *
 * @param array the array to clone. MUST be an **array**.
 * @param subdepth the depth to use when cloning the items of the array.
 *
 * @return the cloned array.
 */
static struct json_object *clone_array(struct json_object *array, int subdepth)
{
	int n = (int)json_object_array_length(array);
	struct json_object *r = json_object_new_array();
	while (n) {
		n--;
		json_object_array_put_idx(r, n,
			wrap_json_clone_depth(json_object_array_get_idx(array, n), subdepth));
	}
	return r;
}

/**
 * Clones any json 'item' for the depth 'depth'. The item is duplicated
 * and if 'depth' is not zero, its contents is recursively cloned with
 * the depth 'depth' - 1.
 *
 * Be aware that this implementation doesn't copies the primitive json
 * items (numbers, nulls, booleans, strings) but instead increments their
 * use count. This can cause issues with newer versions of libjson-c that
 * now unfortunately allows to change their values.
 *
 * @param item the item to clone. Can be of any kind.
 * @param depth the depth to use when cloning composites: object or arrays.
 *
 * @return the cloned array.
 *
 * @see wrap_json_clone
 * @see wrap_json_clone_deep
 */
struct json_object *wrap_json_clone_depth(struct json_object *item, int depth)
{
	if (depth) {
		switch (json_object_get_type(item)) {
		case json_type_object:
			return clone_object(item, depth - 1);
		case json_type_array:
			return clone_array(item, depth - 1);
		default:
			break;
		}
	}
	return json_object_get(item);
}

/**
 * Clones the 'object': returns a copy of it. But doesn't clones
 * the content. Synonym of wrap_json_clone_depth(object, 1).
 *
 * Be aware that this implementation doesn't clones content that is deeper
 * than 1 but it does link these contents to the original object and
 * increments their use count. So, everything deeper that 1 is still available.
 *
 * @param object the object to clone
 *
 * @return a copy of the object.
 *
 * @see wrap_json_clone_depth
 * @see wrap_json_clone_deep
 */
struct json_object *wrap_json_clone(struct json_object *object)
{
	return wrap_json_clone_depth(object, 1);
}

/**
 * Clones the 'object': returns a copy of it. Also clones all
 * the content recursively. Synonym of wrap_json_clone_depth(object, INT_MAX).
 *
 * @param object the object to clone
 *
 * @return a copy of the object.
 *
 * @see wrap_json_clone_depth
 * @see wrap_json_clone
 */
struct json_object *wrap_json_clone_deep(struct json_object *object)
{
	return wrap_json_clone_depth(object, INT_MAX);
}

/**
 * Adds the items of the object 'added' to the object 'dest'.
 *
 * @param dest the object to complete this object is modified
 * @added the object containing fields to add
 *
 * @return the destination object 'dest'
 *
 * @example wrap_json_object_add({"a":"a"},{"X":"X"}) -> {"a":"a","X":"X"}
 */
struct json_object *wrap_json_object_add(struct json_object *dest, struct json_object *added)
{
	struct json_object_iterator it, end;
	if (json_object_is_type(dest, json_type_object) && json_object_is_type(added, json_type_object)) {
		it = json_object_iter_begin(added);
		end = json_object_iter_end(added);
		while (!json_object_iter_equal(&it, &end)) {
			json_object_object_add(dest,
				json_object_iter_peek_name(&it),
				json_object_get(json_object_iter_peek_value(&it)));
			json_object_iter_next(&it);
		}
	}
	return dest;
}

/**
 * Sort the 'array' and returns it. Sorting is done accordingly to the
 * order given by the function 'wrap_json_cmp'. If the paramater isn't
 * an array, nothing is done and the parameter is returned unchanged.
 *
 * @param array the array to sort
 *
 * @returns the array sorted
 */
struct json_object *wrap_json_sort(struct json_object *array)
{
	if (json_object_is_type(array, json_type_array))
		json_object_array_sort(array, (int(*)(const void*, const void*))wrap_json_cmp);

	return array;
}

/**
 * Returns a json array of the sorted keys of 'object' or null if 'object' has no keys.
 *
 * @param object the object whose keys are to be returned
 *
 * @return either NULL is 'object' isn't an object or a sorted array of the key's strings.
 */
struct json_object *wrap_json_keys(struct json_object *object)
{
	struct json_object *r;
	struct json_object_iterator it, end;
	if (!json_object_is_type(object, json_type_object))
		r = NULL;
	else {
		r = json_object_new_array();
		it = json_object_iter_begin(object);
		end = json_object_iter_end(object);
		while (!json_object_iter_equal(&it, &end)) {
			json_object_array_add(r, json_object_new_string(json_object_iter_peek_name(&it)));
			json_object_iter_next(&it);
		}
		wrap_json_sort(r);
	}
	return r;
}

/**
 * Internal comparison of 'x' with 'y'
 *
 * @param x first object to compare
 * @param y second object to compare
 * @param inc boolean true if should test for inclusion of y in x
 * @param sort boolean true if comparison used for sorting
 *
 * @return an integer indicating the computed result. Refer to
 * the table below for meaning of the returned value.
 *
 * inc | sort |  x < y  |  x == y  |  x > y  |  y in x
 * ----+------+---------+----------+---------+---------
 *  0  |  0   |  != 0   |     0    |  != 0   |   > 0
 *  0  |  1   |   < 0   |     0    |   > 0   |   > 0
 *  1  |  0   |  != 0   |     0    |  != 0   |    0
 *  1  |  1   |   < 0   |     0    |   > 0   |    0
 *
 *
 * if 'x' is found, respectively, to be less  than,  to match,
 * or be greater than 'y'. This is valid when 'sort'
 */
static int jcmp(struct json_object *x, struct json_object *y, int inc, int sort)
{
	double dx, dy;
	int64_t ix, iy;
	const char *sx, *sy;
	enum json_type tx, ty;
	int r, nx, ny, i;
	struct json_object_iterator it, end;
	struct json_object *jx, *jy;

	/* check equality of pointers */
	if (x == y)
		return 0;

	/* get the types */
	tx = json_object_get_type(x);
	ty = json_object_get_type(y);
	r = (int)tx - (int)ty;
	if (r)
		return r;

	/* compare following the type */
	switch (tx) {
	default:
	case json_type_null:
		break;

	case json_type_boolean:
		r = (int)json_object_get_boolean(x)
			- (int)json_object_get_boolean(y);
		break;

	case json_type_double:
		dx = json_object_get_double(x);
		dy = json_object_get_double(y);
		r =  dx < dy ? -1 : dx > dy;
		break;

	case json_type_int:
		ix = json_object_get_int64(x);
		iy = json_object_get_int64(y);
		r = ix < iy ? -1 : ix > iy;
		break;

	case json_type_object:
		it = json_object_iter_begin(y);
		end = json_object_iter_end(y);
		nx = json_object_object_length(x);
		ny = json_object_object_length(y);
		r = nx - ny;
		if (r > 0 && inc)
			r = 0;
		while (!r && !json_object_iter_equal(&it, &end)) {
			if (json_object_object_get_ex(x, json_object_iter_peek_name(&it), &jx)) {
				jy = json_object_iter_peek_value(&it);
				json_object_iter_next(&it);
				r = jcmp(jx, jy, inc, sort);
			} else if (sort) {
				jx = wrap_json_keys(x);
				jy = wrap_json_keys(y);
				r = wrap_json_cmp(jx, jy);
				json_object_put(jx);
				json_object_put(jy);
			} else
				r = 1;
		}
		break;

	case json_type_array:
		nx = (int)json_object_array_length(x);
		ny = (int)json_object_array_length(y);
		r = nx - ny;
		if (r > 0 && inc)
			r = 0;
		for (i = 0 ; !r && i < ny ; i++) {
			jx = json_object_array_get_idx(x, i);
			jy = json_object_array_get_idx(y, i);
			r = jcmp(jx, jy, inc, sort);
		}
		break;

	case json_type_string:
		sx = json_object_get_string(x);
		sy = json_object_get_string(y);
		r = strcmp(sx, sy);
		break;
	}
	return r;
}

/**
 * Compares 'x' with 'y'
 *
 * @param x first object to compare
 * @param y second object to compare
 *
 * @return an integer less than, equal to, or greater than zero
 * if 'x' is found, respectively, to be less than, to match,
 * or be greater than 'y'.
 */
int wrap_json_cmp(struct json_object *x, struct json_object *y)
{
	return jcmp(x, y, 0, 1);
}

/**
 * Searchs wether 'x' equals 'y'
 *
 * @param x first object to compare
 * @param y second object to compare
 *
 * @return an integer equal to zero when 'x' != 'y' or 1 when 'x' == 'y'.
 */
int wrap_json_equal(struct json_object *x, struct json_object *y)
{
	return !jcmp(x, y, 0, 0);
}

/**
 * Searchs wether 'x' contains 'y'
 *
 * @param x first object to compare
 * @param y second object to compare
 *
 * @return an integer equal to 1 when 'y' is a subset of 'x' or zero otherwise
 */
int wrap_json_contains(struct json_object *x, struct json_object *y)
{
	return !jcmp(x, y, 1, 0);
}

#if defined(WRAP_JSON_TEST)
#include <stdio.h>
#if !defined(JSON_C_TO_STRING_NOSLASHESCAPE)
#define JSON_C_TO_STRING_NOSLASHESCAPE 0
#endif
#define j2t(o) json_object_to_json_string_ext((o), JSON_C_TO_STRING_NOSLASHESCAPE)

void tclone(struct json_object *object)
{
	struct json_object *o;

	o = wrap_json_clone(object);
	if (!wrap_json_equal(object, o))
		printf("ERROR in clone or equal: %s VERSUS %s\n", j2t(object), j2t(o));
	json_object_put(o);

	o = wrap_json_clone_deep(object);
	if (!wrap_json_equal(object, o))
		printf("ERROR in clone_deep or equal: %s VERSUS %s\n", j2t(object), j2t(o));
	json_object_put(o);
}

void p(const char *desc, ...)
{
	int rc;
	va_list args;
	struct json_object *result;

	va_start(args, desc);
	rc = wrap_json_vpack(&result, desc, args);
	va_end(args);
	if (!rc)
		printf("  SUCCESS %s\n\n", j2t(result));
	else
		printf("  ERROR[char %d err %d] %s\n\n", wrap_json_get_error_position(rc), wrap_json_get_error_code(rc), wrap_json_get_error_string(rc));
	tclone(result);
	json_object_put(result);
}

const char *xs[10];
int *xi[10];
int64_t *xI[10];
double *xf[10];
struct json_object *xo[10];
size_t xz[10];
uint8_t *xy[10];

void u(const char *value, const char *desc, ...)
{
	unsigned m, k;
	int rc;
	va_list args;
	struct json_object *object, *o;

	memset(xs, 0, sizeof xs);
	memset(xi, 0, sizeof xi);
	memset(xI, 0, sizeof xI);
	memset(xf, 0, sizeof xf);
	memset(xo, 0, sizeof xo);
	memset(xy, 0, sizeof xy);
	memset(xz, 0, sizeof xz);
	object = json_tokener_parse(value);
	va_start(args, desc);
	rc = wrap_json_vunpack(object, desc, args);
	va_end(args);
	if (rc)
		printf("  ERROR[char %d err %d] %s\n\n", wrap_json_get_error_position(rc), wrap_json_get_error_code(rc), wrap_json_get_error_string(rc));
	else {
		value = NULL;
		printf("  SUCCESS");
		va_start(args, desc);
		k = m = 0;
		while(*desc) {
			switch(*desc) {
			case '{': m = (m << 1) | 1; k = 1; break;
			case '}': m = m >> 1; k = m&1; break;
			case '[': m = m << 1; k = 0; break;
			case ']': m = m >> 1; k = m&1; break;
			case 's': printf(" s:%s", k ? va_arg(args, const char*) : *(va_arg(args, const char**)?:&value)); k ^= m&1; break;
			case '%': printf(" %%:%zu", *va_arg(args, size_t*)); k = m&1; break;
			case 'n': printf(" n"); k = m&1; break;
			case 'b': printf(" b:%d", *va_arg(args, int*)); k = m&1; break;
			case 'i': printf(" i:%d", *va_arg(args, int*)); k = m&1; break;
			case 'I': printf(" I:%lld", *va_arg(args, int64_t*)); k = m&1; break;
			case 'f': printf(" f:%f", *va_arg(args, double*)); k = m&1; break;
			case 'F': printf(" F:%f", *va_arg(args, double*)); k = m&1; break;
			case 'o': printf(" o:%s", j2t(*va_arg(args, struct json_object**))); k = m&1; break;
			case 'O': o = *va_arg(args, struct json_object**); printf(" O:%s", j2t(o)); json_object_put(o); k = m&1; break;
			case 'y':
			case 'Y': {
				uint8_t *p = *va_arg(args, uint8_t**);
				size_t s = *va_arg(args, size_t*);
				printf(" y/%d:%.*s", (int)s, (int)s, (char*)p);
				k ^= m&1;
				break;
				}
			default: break;
			}
			desc++;
		}
		va_end(args);
		printf("\n\n");
	}
	tclone(object);
	json_object_put(object);
}

void c(const char *sx, const char *sy, int e, int c)
{
	int re, rc;
	struct json_object *jx, *jy;

	jx = json_tokener_parse(sx);
	jy = json_tokener_parse(sy);

	re = wrap_json_cmp(jx, jy);
	rc = wrap_json_contains(jx, jy);

	printf("compare(%s)(%s)\n", sx, sy);
	printf("   -> %d / %d\n", re, rc);

	if (!re != !!e)
		printf("  ERROR should be %s\n", e ? "equal" : "different");
	if (!rc != !c)
		printf("  ERROR should %scontain\n", c ? "" : "not ");

	printf("\n");
}

#define P(...) do{ printf("pack(%s)\n",#__VA_ARGS__); p(__VA_ARGS__); } while(0)
#define U(...) do{ printf("unpack(%s)\n",#__VA_ARGS__); u(__VA_ARGS__); } while(0)

int main()
{
	char buffer[4] = {'t', 'e', 's', 't'};

	P("n");
	P("b", 1);
	P("b", 0);
	P("i", 1);
	P("I", (uint64_t)0x123456789abcdef);
	P("f", 3.14);
	P("s", "test");
	P("s?", "test");
	P("s?", NULL);
	P("s#", "test asdf", 4);
	P("s%", "test asdf", (size_t)4);
	P("s#", buffer, 4);
	P("s%", buffer, (size_t)4);
	P("s++", "te", "st", "ing");
	P("s#+#+", "test", 1, "test", 2, "test");
	P("s%+%+", "test", (size_t)1, "test", (size_t)2, "test");
	P("{}", 1.0);
	P("[]", 1.0);
	P("o", json_object_new_int(1));
	P("o?", json_object_new_int(1));
	P("o?", NULL);
	P("O", json_object_new_int(1));
	P("O?", json_object_new_int(1));
	P("O?", NULL);
	P("{s:[]}", "foo");
	P("{s+#+: []}", "foo", "barbar", 3, "baz");
	P("{s:s,s:o,s:O}", "a", NULL, "b", NULL, "c", NULL);
	P("{s:**}", "a", NULL);
	P("{s:s*,s:o*,s:O*}", "a", NULL, "b", NULL, "c", NULL);
	P("[i,i,i]", 0, 1, 2);
	P("[s,o,O]", NULL, NULL, NULL);
	P("[**]", NULL);
	P("[s*,o*,O*]", NULL, NULL, NULL);
	P(" s ", "test");
	P("[ ]");
	P("[ i , i,  i ] ", 1, 2, 3);
	P("{\n\n1");
	P("[}");
	P("{]");
	P("[");
	P("{");
	P("[i]a", 42);
	P("ia", 42);
	P("s", NULL);
	P("+", NULL);
	P(NULL);
	P("{s:i}", NULL, 1);
	P("{ {}: s }", "foo");
	P("{ s: {},  s:[ii{} }", "foo", "bar", 12, 13);
	P("[[[[[   [[[[[  [[[[ }]]]] ]]]] ]]]]]");
	P("y", "???????hello>>>>>>>", (size_t)19);
	P("Y", "???????hello>>>>>>>", (size_t)19);
	P("{sy?}", "foo", "hi", (size_t)2);
	P("{sy?}", "foo", NULL, 0);
	P("{sy*}", "foo", "hi", (size_t)2);
	P("{sy*}", "foo", NULL, 0);

	U("true", "b", &xi[0]);
	U("false", "b", &xi[0]);
	U("null", "n");
	U("42", "i", &xi[0]);
	U("123456789", "I", &xI[0]);
	U("3.14", "f", &xf[0]);
	U("12345", "F", &xf[0]);
	U("3.14", "F", &xf[0]);
	U("\"foo\"", "s", &xs[0]);
	U("\"foo\"", "s%", &xs[0], &xz[0]);
	U("{}", "{}");
	U("[]", "[]");
	U("{}", "o", &xo[0]);
	U("{}", "O", &xo[0]);
	U("{\"foo\":42}", "{si}", "foo", &xi[0]);
	U("[1,2,3]", "[i,i,i]", &xi[0], &xi[1], &xi[2]);
	U("{\"a\":1,\"b\":2,\"c\":3}", "{s:i, s:i, s:i}", "a", &xi[0], "b", &xi[1], "c", &xi[2]);
	U("42", "z");
	U("null", "[i]");
	U("[]", "[}");
	U("{}", "{]");
	U("[]", "[");
	U("{}", "{");
	U("[42]", "[i]a", &xi[0]);
	U("42", "ia", &xi[0]);
	U("[]", NULL);
	U("\"foo\"", "s", NULL);
	U("42", "s", NULL);
	U("42", "n");
	U("42", "b", NULL);
	U("42", "f", NULL);
	U("42", "[i]", NULL);
	U("42", "{si}", "foo", NULL);
	U("\"foo\"", "n");
	U("\"foo\"", "b", NULL);
	U("\"foo\"", "i", NULL);
	U("\"foo\"", "I", NULL);
	U("\"foo\"", "f", NULL);
	U("\"foo\"", "F", NULL);
	U("true", "s", NULL);
	U("true", "n");
	U("true", "i", NULL);
	U("true", "I", NULL);
	U("true", "f", NULL);
	U("true", "F", NULL);
	U("[42]", "[ii]", &xi[0], &xi[1]);
	U("{\"foo\":42}", "{si}", NULL, &xi[0]);
	U("{\"foo\":42}", "{si}", "baz", &xi[0]);
	U("[1,2,3]", "[iii!]", &xi[0], &xi[1], &xi[2]);
	U("[1,2,3]", "[ii!]", &xi[0], &xi[1]);
	U("[1,2,3]", "[ii]", &xi[0], &xi[1]);
	U("[1,2,3]", "[ii*]", &xi[0], &xi[1]);
	U("{\"foo\":42,\"baz\":45}", "{sisi}", "baz", &xi[0], "foo", &xi[1]);
	U("{\"foo\":42,\"baz\":45}", "{sisi*}", "baz", &xi[0], "foo", &xi[1]);
	U("{\"foo\":42,\"baz\":45}", "{sisi!}", "baz", &xi[0], "foo", &xi[1]);
	U("{\"foo\":42,\"baz\":45}", "{si}", "baz", &xi[0], "foo", &xi[1]);
	U("{\"foo\":42,\"baz\":45}", "{si*}", "baz", &xi[0], "foo", &xi[1]);
	U("{\"foo\":42,\"baz\":45}", "{si!}", "baz", &xi[0], "foo", &xi[1]);
	U("[1,{\"foo\":2,\"bar\":null},[3,4]]", "[i{sisn}[ii]]", &xi[0], "foo", &xi[1], "bar", &xi[2], &xi[3]);
	U("[1,2,3]", "[ii!i]", &xi[0], &xi[1], &xi[2]);
	U("[1,2,3]", "[ii*i]", &xi[0], &xi[1], &xi[2]);
	U("{\"foo\":1,\"bar\":2}", "{si!si}", "foo", &xi[1], "bar", &xi[2]);
	U("{\"foo\":1,\"bar\":2}", "{si*si}", "foo", &xi[1], "bar", &xi[2]);
	U("{\"foo\":{\"baz\":null,\"bar\":null}}", "{s{sn!}}", "foo", "bar");
	U("[[1,2,3]]", "[[ii!]]", &xi[0], &xi[1]);
	U("{}", "{s?i}", "foo", &xi[0]);
	U("{\"foo\":1}", "{s?i}", "foo", &xi[0]);
	U("{}", "{s?[ii]s?{s{si!}}}", "foo", &xi[0], &xi[1], "bar", "baz", "quux", &xi[2]);
	U("{\"foo\":[1,2]}", "{s?[ii]s?{s{si!}}}", "foo", &xi[0], &xi[1], "bar", "baz", "quux", &xi[2]);
	U("{\"bar\":{\"baz\":{\"quux\":15}}}", "{s?[ii]s?{s{si!}}}", "foo", &xi[0], &xi[1], "bar", "baz", "quux", &xi[2]);
	U("{\"foo\":{\"bar\":4}}", "{s?{s?i}}", "foo", "bar", &xi[0]);
	U("{\"foo\":{}}", "{s?{s?i}}", "foo", "bar", &xi[0]);
	U("{}", "{s?{s?i}}", "foo", "bar", &xi[0]);
	U("{\"foo\":42,\"baz\":45}", "{s?isi!}", "baz", &xi[0], "foo", &xi[1]);
	U("{\"foo\":42}", "{s?isi!}", "baz", &xi[0], "foo", &xi[1]);

	U("\"Pz8_Pz8_P2hlbGxvPj4-Pj4-Pg\"", "y", &xy[0], &xz[0]);
	U("\"\"", "y", &xy[0], &xz[0]);
	U("null", "y", &xy[0], &xz[0]);
	U("{\"foo\":\"Pz8_Pz8_P2hlbGxvPj4-Pj4-Pg\"}", "{s?y}", "foo", &xy[0], &xz[0]);
	U("{\"foo\":\"\"}", "{s?y}", "foo", &xy[0], &xz[0]);
	U("{}", "{s?y}", "foo", &xy[0], &xz[0]);

	c("null", "null", 1, 1);
	c("true", "true", 1, 1);
	c("false", "false", 1, 1);
	c("1", "1", 1, 1);
	c("1.0", "1.0", 1, 1);
	c("\"\"", "\"\"", 1, 1);
	c("\"hi\"", "\"hi\"", 1, 1);
	c("{}", "{}", 1, 1);
	c("{\"a\":true,\"b\":false}", "{\"b\":false,\"a\":true}", 1, 1);
	c("[]", "[]", 1, 1);
	c("[1,true,null]", "[1,true,null]", 1, 1);

	c("null", "true", 0, 0);
	c("null", "false", 0, 0);
	c("0", "1", 0, 0);
	c("1", "0", 0, 0);
	c("0", "true", 0, 0);
	c("0", "false", 0, 0);
	c("0", "null", 0, 0);

	c("\"hi\"", "\"hello\"", 0, 0);
	c("\"hello\"", "\"hi\"", 0, 0);

	c("{}", "null", 0, 0);
	c("{}", "true", 0, 0);
	c("{}", "1", 0, 0);
	c("{}", "1.0", 0, 0);
	c("{}", "[]", 0, 0);
	c("{}", "\"x\"", 0, 0);

	c("[1,true,null]", "[1,true]", 0, 1);
	c("{\"a\":true,\"b\":false}", "{\"a\":true}", 0, 1);
	c("{\"a\":true,\"b\":false}", "{\"a\":true,\"c\":false}", 0, 0);
	c("{\"a\":true,\"c\":false}", "{\"a\":true,\"b\":false}", 0, 0);
	return 0;
}

#endif

#if 0


    /* Unpack the same item twice */
    j = json_pack("{s:s, s:i, s:b}", "foo", "bar", "baz", 42, "quux", 1);
    if(!json_unpack_ex(j, &error, 0, "{s:s,s:s!}", "foo", &s, "foo", &s))
        fail("json_unpack object with strict validation failed");
    {
        const char *possible_errors[] = {
            "2 object item(s) left unpacked: baz, quux",
            "2 object item(s) left unpacked: quux, baz"
        };
        check_errors(possible_errors, 2, "<validation>", 1, 10, 10);
    }
    json_decref(j);

#endif

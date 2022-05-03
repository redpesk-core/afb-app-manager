/*
 Copyright (C) 2015-2022 IoT.bzh Company

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
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
*/
#include <stdio.h>
#include <string.h>

#include <json-c/json.h>

#include <wgt.h>
#include <utils-json.h>
#include <wgt-json.h>

const char tab[] = "  ";
const char beg[] = "- ";
int depth;
int begarr;
char *buffer = NULL;

const char *esc(const char *str)
{
	int nresc, length, wr, d, l;
	for (nresc = length = 0 ; ; length++) {
		switch (str[length]) {
		default:
			break;
		case '\n':
			nresc++;
			break;
		case 0:
			if (nresc == 0)
				return str;
			wr = length + 2 + (nresc + 1) * (1 + (depth + 1) * strlen(tab));
			buffer = realloc(buffer, wr);
			buffer[0] = '>';
			wr = 1;
			for (length = 0 ; ; ) {
				buffer[wr++] = '\n';
				for (d = 0 ; d <= depth ; d++)
					for (l = 0 ; tab[l] ; )
						buffer[wr++] = tab[l++];
				while ((buffer[wr] = str[length++]) != '\n')
					if (buffer[wr] == 0)
						return buffer;
					else
						wr++;
			}
		}
	}
	return str;
}

void put(const char *key, const char *value)
{
	int d = depth;
	while(d) {
		d--;
		printf("%s", d || !begarr ? tab : beg);
	}
	if (key != NULL)
		printf("%s:", key);
	if (key != NULL && value != NULL && *value != 0)
		printf(" ");
	if (value != NULL && *value != 0)
		printf("%s", esc(value));
	printf("\n");
	begarr = 0;
}

void optput(const char *key, const char *value)
{
	if (value != NULL)
		put(key, value);
}

const char *opt_str_field(struct json_object *obj, const char *fname)
{
	struct json_object *item;
	return json_object_object_get_ex(obj, fname, &item) ? json_object_get_string(item) : NULL;
}

const char *opt_str_field2(struct json_object *obj, const char *fname1, const char *fname2)
{
	struct json_object *item;
	return json_object_object_get_ex(obj, fname1, &item) ? opt_str_field(item, fname2) : NULL;
}

const char *str_field(struct json_object *obj, const char *fname)
{
	return opt_str_field(obj, fname) ?: "";
}

const char *str_field2(struct json_object *obj, const char *fname1, const char *fname2)
{
	return opt_str_field2(obj, fname1, fname2) ?: "";
}


void opt_put_str_field1(const char *key, struct json_object *obj, const char *fname)
{
	optput(key, opt_str_field(obj, fname));
}

void opt_put_str_field2(const char *key, struct json_object *obj, const char *fname1, const char *fname2)
{
	optput(key, opt_str_field2(obj, fname1, fname2));
}

void opt_put_str_field(const char *key, struct json_object *obj)
{
	opt_put_str_field1(key, obj, key);
}

void put_str_field1(const char *key, struct json_object *obj, const char *fname)
{
	put(key, str_field(obj, fname));
}

void put_str_field2(const char *key, struct json_object *obj, const char *fname1, const char *fname2)
{
	put(key, str_field2(obj, fname1, fname2));
}

void put_str_field(const char *key, struct json_object *obj)
{
	put_str_field1(key, obj, key);
}

void put_object(struct json_object *obj, const char **firsts);

void put_any(const char *key, struct json_object *obj)
{
	size_t idx, count;
	struct json_object *x;

	switch(json_object_get_type(obj)) {
	case json_type_array:
		if (key != NULL)
			put(key, NULL);
		depth++;
		count = json_object_array_length(obj);
		for (idx = 0 ; idx < count ; idx++) {
			begarr = 1;
			x = json_object_array_get_idx(obj, idx);
			put_any(NULL, x);
		}
		depth--;
		break;
	case json_type_object:
		if (key != NULL)
			put(key, NULL);
		depth++;
		put_object(obj, NULL);
		depth--;
		break;
	default:
		put(key, json_object_get_string(obj));
		break;
	}
}

void opt_put_field1(const char *key, struct json_object *obj, const char *fname)
{
	struct json_object *item;
	if (json_object_object_get_ex(obj, fname, &item))
		put_any(key, item);
}

void opt_put_field(const char *key, struct json_object *obj)
{
	opt_put_field1(key, obj, key);
}

void put_object(struct json_object *obj, const char **firsts)
{
	const char *k;
	struct json_object *x;
	struct json_object_iterator it, end;
	const char **keys = firsts;
	if (keys)
	for (keys = firsts ; keys != NULL && *keys != NULL ; keys++)
		opt_put_field(*keys, obj);
	it = json_object_iter_begin(obj);
	end = json_object_iter_end(obj);
	while (!json_object_iter_equal(&it, &end)) {
		k = json_object_iter_peek_name(&it);
		x = json_object_iter_peek_value(&it);
		for (keys = firsts ; keys != NULL && *keys != NULL ; keys++)
			if (0 == strcmp(k, *keys))
				break;
		if (keys == NULL || *keys == NULL)
			put_any(k, x);
		json_object_iter_next(&it);
	}
}

void print_target(struct json_object *root)
{
	static const char *keys[] = { "#target", "content", "icon", NULL };
	depth++;
	begarr = 1;
	put_object(root, keys);
	depth--;
}

void print_targets(struct json_object *root)
{
	size_t idx, count;
	struct json_object *tm = NULL, *t, *v;

	if (json_object_is_type(root, json_type_array)) {
		put("targets", NULL);
		depth++;
		count = json_object_array_length(root);
		for (idx = 0 ; idx < count ; idx++) {
			t = json_object_array_get_idx(root, idx);
			if (json_object_object_get_ex(t, "#target", &v)
			 && json_object_is_type(v, json_type_string)
			 && 0 == strcmp("main", json_object_get_string(v))) {
				print_target(t);
				tm = t;
				break;
			}
		}
		for (idx = 0 ; idx < count ; idx++) {
			t = json_object_array_get_idx(root, idx);
			if (t != tm)
				print_target(t);
		}
		depth--;
	}
}

void print_root(struct json_object *root)
{
	put("rp-manifest", "1");
	put_str_field("id", root);
	put_str_field("version", root);
	opt_put_str_field2("name", root, "name", "content");
	opt_put_str_field2("short-name", root, "name", "short");
	opt_put_str_field("description", root);
	opt_put_str_field2("author", root, "author", "content");
	opt_put_str_field2("email", root, "author", "email");
	opt_put_str_field2("site", root, "author", "href");
	opt_put_str_field2("license", root, "license", "content");
	opt_put_str_field2("license-href", root, "license", "href");

	opt_put_field("required-permission", root);
	opt_put_field("provided-binding", root);
	opt_put_field("defined-permission", root);
	opt_put_field("file-properties", root);

	print_targets(json_object_object_get(root, "targets"));
}

int main(int ac, char **av)
{
	struct json_object *obj;

	while(*++av) {
		obj = wgt_config_to_json(*av);
		if (!obj)
			fprintf(stderr, "can't read widget config at %s: %m",*av);
		else {
			//printf("%s", json_object_to_json_string(obj));
			print_root(obj);
			json_object_put(obj);
		}
	}
	return 0;
}



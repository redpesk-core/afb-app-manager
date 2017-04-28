/*
 Copyright 2016, 2017 IoT.bzh

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


struct json_object;
struct wgt_info;


enum unitscope {
	unitscope_unknown = 0,
	unitscope_system,
	unitscope_user
};

enum unittype {
	unittype_unknown = 0,
	unittype_service,
	unittype_socket
};

struct unitdesc {
	enum unitscope scope;
	enum unittype type;
	const char *name;
	size_t name_length;
	const char *content;
	size_t content_length;
	const char *wanted_by;
	size_t wanted_by_length;
};

struct unitconf {
	const char *installdir;
	const char *icondir;
	int port;
};

struct generatedesc {
	const struct unitconf *conf;
	const struct unitdesc *units;
	int nunits;
};

extern int unit_generator_on(const char *filename);
extern void unit_generator_off();
extern int unit_generator_process(struct json_object *jdesc, const struct unitconf *conf, int (*process)(void *closure, const struct generatedesc *desc), void *closure);
extern int unit_install(struct wgt_info *ifo, const struct unitconf *conf);
extern int unit_uninstall(struct wgt_info *ifo, const struct unitconf *conf);


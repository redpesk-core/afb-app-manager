/*
 Copyright (C) 2015-2020 IoT.bzh

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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json-c/json.h>

#include <wgt.h>
#include <utils-json.h>
#include <wgt-json.h>
#include <wgtpkg-mustach.h>
#include <wgtpkg-unit.h>


#define error(...) fprintf(stderr,__VA_ARGS__),exit(1)



static int processunit(const struct unitdesc *desc)
{
	int isuser = desc->scope == unitscope_user;
	int issystem = desc->scope == unitscope_system;
	int issock = desc->type == unittype_socket;
	int isserv = desc->type == unittype_service;
	const char *name = desc->name;
	const char *content = desc->content;

printf("\n##########################################################");
printf("\n### usr=%d sys=%d soc=%d srv=%d    name  %s%s", isuser, issystem, issock,
			isserv, name?:"?", issock?".socket":isserv?".service":"");
printf("\n##########################################################");
printf("\n%s\n\n",content);
	return 0;
}

static int process(void *closure, const struct generatedesc *desc)
{
	int i;
printf("\n##########################################################");
printf("\n###### J S O N D E S C    AFTER                    #######");
printf("\n##########################################################");
puts(json_object_to_json_string_ext(desc->desc, JSON_C_TO_STRING_PRETTY));
	for (i = 0 ; i < desc->nunits ; i++)
		processunit(&desc->units[i]);
	return 0;
}

static int new_afid()
{
	static int r = 1;
	return r++;
}

int main(int ac, char **av)
{
	struct unitconf conf;
	struct json_object *obj;
	int rc;

	conf.installdir = "INSTALL-DIR";
	conf.icondir = "ICONS-DIR";
	conf.new_afid = new_afid;
	conf.base_http_ports = 20000;
	rc = unit_generator_open_template(*++av);
	if (rc < 0)
		error("can't read template %s: %m",*av);
	while(*++av) {
		obj = wgt_path_to_json(*av);
		if (!obj)
			error("can't read widget config at %s: %m",*av);

printf("\n##########################################################");
printf("\n###### J S O N D E S C    BEFORE                   #######");
printf("\n##########################################################");
puts(json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PRETTY));
		rc = unit_generator_process(obj, &conf, process, NULL);
		if (rc)
			error("can't apply generate units, error %d",rc);
		json_object_put(obj);
	}
	return 0;
}



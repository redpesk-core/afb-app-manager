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

enum entrytype {
	type_unset = 0,
	type_file = 1,
	type_directory = 2
};

enum fileflag {
	flag_referenced = 1,
	flag_opened = 2,
	flag_author_signature = 4,
	flag_distributor_signature = 8,
	flag_signature = 12
};

struct filedesc {
	enum entrytype type;
	unsigned int flags;
	unsigned int signum;
	unsigned int zindex;
	char name[1];
};

extern void file_reset();
extern void file_clear_flags();
extern unsigned int file_count();
extern struct filedesc *file_of_index(unsigned int index);
extern struct filedesc *file_of_name(const char *name);
extern struct filedesc *file_add_directory(const char *name);
extern struct filedesc *file_add_file(const char *name);
extern int fill_files();

extern unsigned int signature_count();
extern struct filedesc *signature_of_index(unsigned int index);
extern struct filedesc *create_signature(unsigned int number);
extern struct filedesc *get_signature(unsigned int number);

extern int file_set_prop(struct filedesc *file, const char *name, const char *value);
extern const char *file_get_prop(struct filedesc *file, const char *name);


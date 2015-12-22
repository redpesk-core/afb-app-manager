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


#include <libxml/tree.h>

struct filedesc;

/**************************************************************/
/* from wgtpkg-base64 */

extern char *base64encw(const char *buffer, int length, int width);
extern char *base64enc(const char *buffer, int length);
extern int base64dec(const char *buffer, char **output);
extern int base64eq(const char *buf1, const char *buf2);

/**************************************************************/
/* from wgtpkg-certs */

extern void clear_certificates();
extern int add_certificate_b64(const char *b64);

/**************************************************************/
/* from wgtpkg-digsig */

/* verify the digital signature in file */
extern int verify_digsig(struct filedesc *fdesc);

/* create a digital signature */
extern int create_digsig(int index, const char *key, const char **certs);

/* check the signatures of the current directory */
extern int check_all_signatures();

/**************************************************************/
/* from wgtpkg-files */

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

/**************************************************************/
/* from wgtpkg-install */

extern int install_widget(const char *wgtfile, const char *root, int force);

/**************************************************************/
/* from wgtpkg-permission */

extern int is_standard_permission(const char *name);
extern void reset_permissions();
extern void crop_permissions(unsigned level);
extern void grant_permission_list(const char *list);
extern int permission_exists(const char *name);
extern int request_permission(const char *name);
extern const char *first_usable_permission();
extern const char *next_usable_permission();

/**************************************************************/
/* from wgtpkg-workdir */

extern char workdir[PATH_MAX];
extern int workdirfd;
extern void remove_workdir();
extern int set_workdir(const char *name, int create);
extern int make_workdir_base(const char *root, const char *prefix, int reuse);
extern int make_workdir(int reuse);
extern int move_workdir(const char *dest, int parents, int force);

/**************************************************************/
/* from wgtpkg-xmlsec */

extern int xmlsec_init();
extern void xmlsec_shutdown();
extern int xmlsec_verify(xmlNodePtr node);
extern xmlDocPtr xmlsec_create(int index, const char *key, const char **certs);

/**************************************************************/
/* from wgtpkg-zip */

/* read (extract) 'zipfile' in current directory */
extern int zread(const char *zipfile, unsigned long long maxsize);

/* write (pack) content of the current directory in 'zipfile' */
extern int zwrite(const char *zipfile);



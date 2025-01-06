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


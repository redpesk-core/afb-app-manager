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

#pragma once

struct json_object;

/** definition of unit's scope */
enum unitscope {
	/** unset or unknown scope */
	unitscope_unknown = 0,
	/** system scope */
	unitscope_system,
	/** user scope */
	unitscope_user
};

/** definition of unit's type */
enum unittype {
	/** unset or unknown type */
	unittype_unknown = 0,
	/** service type */
	unittype_service,
	/** socket type */
	unittype_socket
};

/** definition of a unit */
struct unitdesc {
	/** scope of the unit */
	enum unitscope scope;
	/** type of the unit */
	enum unittype type;
	/** name of the unit */
	const char *name;
	/** length of the name */
	size_t name_length;
	/** content of the unit (not zero ended) */
	const char *content;
	/** length of the unit */
	size_t content_length;
	/** name wanting the unit or NULL */
	const char *wanted_by;
	/** length of wanted_by */
	size_t wanted_by_length;
};

/**
 * open/select the template file
 *
 * @return 0 on success
 */
extern int unit_process_open_template(const char *filename);

/**
 * close the template file
 *
 * @return 0 on success
 */
extern void unit_process_close_template();

/*
 * split the 'corpus' to its units (change the corpus by inserting nuls)
 * Each unit of the corpus is separated and packed and its
 * charactistics are stored in a descriptor.
 * At the end if no error was found, returns the count of units found and
 * store them descriptor in units.
 * A negative value is returned in case of error.
 */
extern
int unit_corpus_split(
	char *corpus,
	struct unitdesc **units
);

/**
 * Instanciate the currently opened template (or the default one if none
 * were opened) with the values of `jdesc`. Call the function process
 * with the value of that instanciation.
 *
 * @param jdesc    the actual values for the template
 * @param process  the callback receiving the instanciation
 * @param closure  closure of the callback
 *
 * @return 0 on sucess or a negative value on error
 */
extern int unit_process_raw(
		struct json_object *jdesc,
		int (*process)(void *closure, char *text, size_t size),
		void *closure);

/**
 * Instanciate the currently opened template (or the default one if none
 * were opened) with the values of `jdesc`. Split the instantiation
 * in its unit files. Call the function process with description of the
 * generated files.
 *
 * @param jdesc    the actual values for the template
 * @param process  the callback receiving the splitted files
 * @param closure  closure of the callback
 *
 * @return 0 on sucess or a negative value on error
 */
extern int unit_process_split(
		struct json_object *jdesc,
		int (*process)(void *closure, const struct unitdesc *units, int nrunits),
		void *closure);


struct unitconf {
	json_object *metadata;
	int (*new_afid)();
	int base_http_ports;
};

struct generatedesc {
	const struct unitconf *conf;
	struct json_object *desc;
	const struct unitdesc *units;
	int nunits;
};

/**
 * Instanciate the currently opened template (or the default one if none
 * were opened) with the values of `jdesc`. Call the function process
 * were opened) with the values of `jdesc`. Split the instantiation
 * in its unit files. Call the function process with description of the
 * generated files and other parameters.
 *
 * @param jdesc    the actual values for the template
 * @param process  the callback receiving the splitted files
 * @param closure  closure of the callback
 *
 * @return 0 on sucess or a negative value on error
 */
extern int unit_process_legacy(
		struct json_object *jdesc,
		const struct unitconf *conf,
		int (*process)(void *closure, const struct generatedesc *desc),
		void *closure);


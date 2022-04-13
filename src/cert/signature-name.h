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

#pragma once

#include <stdlib.h>

/**
 * @brief signature type of a file/name
 *
 */
typedef
enum signature_type
{
	/** not a signature */
	Signature_None,

	/** the author signature */
	Signature_Author,

	/** a distributor signature */
	Signature_distributor
}
	signature_type_t;

/**
 * @brief Get the signature type of path/name
 *
 * @param path the path/name to inspect
 * @return the signature type
 */
extern
signature_type_t signature_name_type(const char *path);

/**
 * @brief Get the signature type of path/name
 *
 * @param path the path/name to inspect
 * @param length length of the path/name
 * @return the signature type
 */
extern
signature_type_t signature_name_type_length(const char *path, size_t length);

/**
 * @brief Get the signature filename for signer of name
 * if there is not enough space in buffer, the returned length is
 * the normal length as if the buffer had enough rooms
 *
 * @param name name of the signer
 * @param length length of the name
 * @param buffer place for storing the result
 * @param size size of the buffer
 * @return the size of the computed name not including the tailing zero
 */
extern
size_t make_signature_name_length(const char *name, size_t length, char *buffer, size_t size);

/**
 * @brief Get the signature filename for signer of name
 * if there is not enough space in buffer, the returned length is
 * the normal length as if the buffer had enough rooms
 *
 * @param name name of the signer
 * @param buffer place for storing the result
 * @param size size of the buffer
 * @return the size of the computed name not including the tailing zero
 */
extern
size_t make_signature_name(const char *name, char *buffer, size_t size);

/**
 * @brief Get the signature filename for signer of number
 * if there is not enough space in buffer, the returned length is
 * the normal length as if the buffer had enough rooms
 *
 * @param number the number of the signer
 * @param buffer place for storing the result
 * @param size size of the buffer
 * @return the size of the computed name not including the tailing zero
 */
extern
size_t make_signature_name_number(unsigned number, char *buffer, size_t size);

/**
 * @brief Get the signature filename for the author
 * if there is not enough space in buffer, the returned length is
 * the normal length as if the buffer had enough rooms
 *
 * @param buffer place for storing the result
 * @param size size of the buffer
 * @return the size of the computed name not including the tailing zero
 */
extern
size_t make_signature_name_author(char *buffer, size_t size);

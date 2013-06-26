/*
 * Samba Unix/Linux SMB client library
 *
 * Copyright (C) Gregor Beck 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief  Adapter to use reg_parse with the registry api
 * @file   reg_import.h
 * @author Gregor Beck <gb@sernet.de>
 * @date   Jun 2010
 */


#ifndef REG_IMPORT_H
#define REG_IMPORT_H

#include "reg_parse.h"

struct registry_value;
struct regval_blob;

/**
 * Protoype for function called to open a key.
 *
 * @param private_data
 * @param[in] parent    the parent of the key to open, may be NULL
 * @param[in] name      the name of the key relative to parent.
 * @param[out] key      the opened key
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_openkey_t) (void* private_data,
						void* parent,
						const char* name,
						void** key);

/**
 * Protoype for function called to close a key.
 *
 * @param private_data
 * @param key the key to close
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_closekey_t) (void* private_data,
						  void* key);

/**
 * Protoype for function called to create (or open an existing) key.
 *
 * @param private_data
 * @param[in] parent    the parent of the key to create, may be NULL
 * @param[in] name      the name of the key relative to parent.
 * @param[out] key      the opened key
 * @param[out] existing whether we opened an existing key
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_createkey_t)(void* private_data,
						void* parent,
						const char* name,
						void** key,
						bool* existing);

/**
 * Protoype for function called to delete a key.
 *
 * @param private_data
 * @param parent    the parent of the key to delete, may be NULL
 * @param[in] name  the name of the key relative to parent.
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_deletekey_t)(void* private_data,
						  void* parent,
						  const char* name);

/**
 * Protoype for function called to delete a value.
 *
 * @param private_data
 * @param parent    the key of the value to delete
 * @param[in] name  the name of the value to delete.
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_deleteval_t)(void* private_data,
						void* parent,
						const char* name);

/**
 * Protoype for function called to set a value.
 *
 * @param private_data
 * @param parent the key of the value to set
 * @param name   the name of the value
 * @param type   the type of the value
 * @param data   the value of the value
 * @param size   the number of bytes of data
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_setval_blob_t)(void* private_data,
						  void* parent,
						  const char* name,
						  uint32_t type,
						  const uint8_t* data,
						  uint32_t size);

/**
 * Protoype for function called to set a value given as struct registry_value.
 *
 * @param private_data
 * @param parent the key of the value to set
 * @param name   the name of the value
 * @param val    the value of the value
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_setval_registry_value_t) (
	void* private_data,
	void* parent,
	const char* name,
	const struct registry_value* val);

/**
 * Protoype for function called to set a value given as struct struct regval_blob.
 *
 * @param private_data
 * @param parent the key of the value to set
 * @param val    the value
 *
 * @return WERR_OK on success
 */
typedef	WERROR (*reg_import_callback_setval_regval_blob_t)(
	void* private_data,
	void* parent,
	const struct regval_blob* val);

/**
 * Type handling the output of a reg_import object.
 * It containes the functions to call and an opaque data pointer.
 */
struct reg_import_callback {
	/** Function called to open key */
	reg_import_callback_openkey_t   openkey;
	/** Function called to close key */
	reg_import_callback_closekey_t  closekey;
	/** Function called to create or open key */
	reg_import_callback_createkey_t createkey;
	/** Function called to delete key */
	reg_import_callback_deletekey_t deletekey;
	/** Function called to delete value */
	reg_import_callback_deleteval_t deleteval;

	/** Function called to set value */
	union {
		reg_import_callback_setval_blob_t           blob;
		reg_import_callback_setval_registry_value_t registry_value;
		reg_import_callback_setval_regval_blob_t    regval_blob;
	} setval;
	/** Which function is called to set a value */
	enum {
		NONE=0,         /**< no setval function used */
		BLOB,           /**< @ref reg_import_callback_setval_blob_t blob */
		REGISTRY_VALUE, /**< @ref reg_import_callback_setval_registry_value_t registry_value */
		REGVAL_BLOB,    /**< @ref reg_import_callback_setval_regval_blob_t regval_blob */
	} setval_type;
	void* data; /**< Private data passed to callback function */
};


/**
 * Create a new reg_import object.
 * Because its only purpose is to act as an reg_parse_callback the return type
 * is accordingly.
 *
 * @param talloc_ctx the talloc parent
 * @param cb         the output handler
 *
 * @return a talloc'ed reg_import object, NULL on error
 */
struct reg_parse_callback* reg_import_adapter(const void* talloc_ctx,
					      struct reg_import_callback cb);
#endif

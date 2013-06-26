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
 * @brief  Format registration entries (.reg) files.
 * A formater is a talloced incarnation of an opaque struct reg_format.
 * It is fed with registry key's and value's and emits output by calling
 * writeline from its reg_format_callback.
 * @file   reg_format.h
 * @author Gregor Beck <gb@sernet.de>
 * @date   Sep 2010
 */
#ifndef __REG_FORMAT_H
#define __REG_FORMAT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct registry_key;
struct registry_value;
struct regval_blob;


/**
 * A Formater for registration entries (.reg) files.
 *
 * It may be used as a reg_parse_callback, so the following is valid:
 * @code
 * reg_parse* p = reg_parse_new(mem_ctx,
 *                             (reg_parse_callback)reg_format_new(mem_ctx, cb, NULL, 0, "\\"),
 *                                NULL, 0);
 * @endcode
 * @see reg_parse
 */
typedef struct reg_format reg_format;

/**
 * Protoype for function called to output a line.
 *
 * @param private_data
 * @param line line to write in UNIX charset
 *
 * @return number of characters written, < 0 on error
 *
 * @see reg_parse
 */
typedef int (*reg_format_callback_writeline_t)(void* private_data,
					       const char* line);
/**
 * Type handling the output of a reg_format object.
 * It containes the functions to call and an opaque data pointer.
 */
typedef struct reg_format_callback {
	/**< Function called to write a line */
	reg_format_callback_writeline_t writeline;
	void* data; /**< Private data passed to callback function */
} reg_format_callback;

/**
 * Create a new reg_format object.
 *
 * @param talloc_ctx the talloc parent
 * @param cb         the output handler
 * @param str_enc    the charset of hex encoded strings (REG_MULTI_SZ, REG_EXAND_SZ) if not UTF-16
 * @param flags
 * @param sep        the separator for subkeys
 *
 * @return a talloc'ed reg_format object, NULL on error
 */
reg_format* reg_format_new(const void* talloc_ctx,
			   reg_format_callback cb,
			   const char* str_enc,
			   unsigned flags,
			   const char* sep);

/**
 * Create a new reg_format object, writing to a file.
 *
 * @param talloc_ctx the talloc parent
 * @param filename   the file to write to
 * @param options
 *
 * @return a talloc'ed reg_format object, NULL on error
 */
reg_format* reg_format_file(const void* talloc_ctx,
			    const char* filename,
			    const char* options);

/**
 * Format a registry key given as struct registry_key.
 * Create/Open or Delete
 *
 * @param f   the formater.
 * @param key the key to output.
 * @param del wheter to format the deletion of the key
 *
 * @retval >= 0 on success.
 */
int reg_format_registry_key(reg_format* f,
			    struct registry_key* key,
			    bool del);

/**
 * Format a registry value given as struct registry_value.
 *
 * @param f    the formater.
 * @param name the values name
 * @param val  the values value.
 *
 * @retval >= 0 on success.
 */
int reg_format_registry_value(reg_format* f,
			      const char* name,
			      struct registry_value* val);

/**
 * Format a registry value given as struct regval_blob.
 *
 * @param f    the formater.
 * @param name the values name, if NULL use val->valname which is limited in size;
 * @param val  the values value.
 *
 * @retval >= 0 on success.
 */
int reg_format_regval_blob(reg_format* f,
			   const char* name,
			   struct regval_blob* val);


/**
 * Format deletion of a registry value.
 *
 * @param f    the formater.
 * @param name the values name
 *
 * @retval >= 0 on success.
 *
 * @see reg_parse_callback_val_del_t
 */
int reg_format_value_delete(reg_format* f, const char* name);

/**
 * Format a comment.
 *
 * @param f   the formater.
 * @param txt the comment in UNIX charset, may not contain newlines.
 *
 * @retval >= 0 on success.
 *
 * @see reg_parse_callback_comment_t
 */
int reg_format_comment(reg_format* f, const char* txt);


int reg_format_set_options(reg_format* f, const char* options);


/* reg_format flags */
#define REG_FMT_HEX_SZ  1
#define REG_FMT_HEX_DW  2
#define REG_FMT_HEX_BIN 4
#define REG_FMT_HEX_ALL (REG_FMT_HEX_SZ | REG_FMT_HEX_DW | REG_FMT_HEX_BIN);
#define REG_FMT_LONG_HIVES 16
#define REG_FMT_SHORT_HIVES 32

/* lowlevel */

/**
 * Format a registry key.
 * Create/Open or Delete
 *
 * @param f   the formater
 * @param key the key to output
 * @param klen number of elements in key
 * @param del wheter to format the deletion of the key
 *
 * @retval >= 0 on success.
 *
 * @see reg_parse_callback_key_t
 */
int reg_format_key(reg_format* f,
		   const char* key[], size_t klen,
		   bool del);

/**
 * Format a registry value.
 *
 * @param f    the formater
 * @param name the values name
 * @param type the values type
 * @param data the values value
 * @param len  the number of bytes of data
 *
 * @retval >= 0 on success.
 *
 * @see reg_parse_callback_val_hex_t
 */
int reg_format_value(reg_format* f,
		     const char* name, uint32_t type,
		     const uint8_t* data, size_t len);

#endif /* __REG_FORMAT_H */

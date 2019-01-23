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
 * @brief  Parser for registration entries (.reg) files.
 * A parser is a talloced incarnation of an opaque struct reg_parse.
 * It is fed with the (.reg) file line by line calling @ref reg_parse_line
 * and emits output by calling functions from its reg_parse_callback.
 * @file   reg_parse.h
 * @author Gregor Beck <gb@sernet.de>
 * @date   Jun 2010
 */

#ifndef REG_PARSE_H
#define REG_PARSE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Protoype for function called on key found.
 * The usual action to take is delete the key if del==true, open it if
 * already existing or create a new one.
 *
 * @param private_data
 * @param key
 * @param klen number of elements in key
 * @param del whether to delete the key
 *
 * @retval >=0 on success
 *
 * @see reg_format_key
 */
typedef int (*reg_parse_callback_key_t) (void* private_data,
					 const char* key[],
					 size_t klen,
					 bool del);

/**
 * Protoype for function called on value found.
 * The usual action to take is set the value of the last opened key.
 *
 * @param private_data
 * @param name the values name
 * @param type the values type
 * @param data the values value
 * @param len  the number of bytes of data
 *
 * @retval >=0 on success
 *
 * @see reg_format_value
 */
typedef int (*reg_parse_callback_val_t) (void*  private_data,
					 const char* name,
					 uint32_t type,
					 const uint8_t* data,
					 uint32_t len);

/**
 * Protoype for function called on value delete found.
 * Delete value from the last opened key. It is usually no error if
 * no such value exist.
 *
 * @param private_data
 * @param name
 *
 * @retval >=0 on success
 *
 * @see reg_format_value_delete
 */
typedef int (*reg_parse_callback_val_del_t) (void* private_data,
					     const char* name);


/**
 * Protoype for function called on comment found.
 *
 * @param private_data
 * @param line comment with marker removed.
 *
 * @retval  >=0 on success
 *
 * @see reg_format_comment
 */
typedef int (*reg_parse_callback_comment_t) (void* private_data,
					     const char* line);

/**
 * Type handling the output of a reg_parse object.
 * It containes the functions to call and an opaque data pointer.
 */
typedef struct reg_parse_callback {
	reg_parse_callback_key_t key; /**< Function called on key found */
	reg_parse_callback_val_t val; /**< Function called on value found */
        /** Function called on value delete found */
	reg_parse_callback_val_del_t val_del;
	/** Function called on comment found */
	reg_parse_callback_comment_t comment;
	void* data; /**< Private data passed to callback function */
} reg_parse_callback;

/**
 * A Parser for a registration entries (.reg) file.
 *
 * It may be used as a reg_format_callback, so the following is valid:
 * @code
 * reg_format* f = reg_format_new(mem_ctx,
 *                                (reg_format_callback)reg_parse_new(mem_ctx, cb, NULL, 0),
 *                                NULL, 0, "\\");
 * @endcode
 * @see reg_format
 */
typedef struct reg_parse reg_parse;

/**
 * Create a new reg_parse object.
 *
 * @param talloc_ctx the talloc parent
 * @param cb         the output handler
 * @param str_enc    the charset of hex encoded strings (REG_MULTI_SZ, REG_EXAND_SZ) if not UTF-16
 * @param flags
 *
 * @return a talloc'ed reg_parse object, NULL on error
 */
reg_parse* reg_parse_new(const void* talloc_ctx,
			 reg_parse_callback cb,
			 const char* str_enc,
			 unsigned flags);

/**
 * Feed one line to the parser.
 *
 * @param parser
 * @param line one line from a (.reg) file, in UNIX charset
 *
 * @return 0 on success
 *
 * @see reg_format_callback_writeline_t
 */
int reg_parse_line(struct reg_parse* parser, const char* line);


/**
 * Parse a (.reg) file, read from a file descriptor.
 *
 * @param fd the file descriptor
 * @param cb the output handler
 * @param opts
 *
 * @return 0 on success
 */
int reg_parse_fd(int fd,
		 const reg_parse_callback* cb,
		 const char* opts);

/**
 * Parse a (.reg) file
 *
 * @param filename the file to open
 * @param cb the output handler
 * @param opts
 *
 * @return 0 on success
 */
int reg_parse_file(const char* filename,
		   const reg_parse_callback* cb,
		   const char* opts);

int reg_parse_set_options(reg_parse* parser, const char* opt);

/******************************************************************************/


#endif /* REG_PARSE_H */

/*
 * Unix SMB/CIFS implementation.
 *
 * Registry helper routines
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
 * @brief  Some stuff used by reg_parse and reg_format.
 * It might be usefull elsewehre but need some review of the interfaces.
 * @file   reg_parse_internal.h
 * @author Gregor Beck <gb@sernet.de>
 * @date   Sep 2010
 */
#ifndef __REG_PARSE_INTERNAL_H
#define __REG_PARSE_INTERNAL_H

#include "includes.h"
#include "system/iconv.h"

struct cbuf;

#define USE_NATIVE_ICONV
#if defined USE_NATIVE_ICONV && defined HAVE_NATIVE_ICONV
#  define smb_iconv_t     iconv_t
#  define smb_iconv(CD, IPTR, ILEN, OPTR, OLEN) \
	iconv(CD, (char**)(IPTR), ILEN, OPTR, OLEN)
#  define smb_iconv_open  iconv_open
#  define smb_iconv_close iconv_close
#endif

size_t iconvert_talloc(const void* ctx,
		       smb_iconv_t cd,
		       const char* src, size_t srclen,
		       char** pdst);

struct hive_info {
	uint32_t handle;
	const char* short_name;
	size_t short_name_len;
	const char* long_name;
	size_t long_name_len;
};

const struct hive_info* hive_info(const char* name, int nlen);

const char* get_charset(const char* c);

bool set_iconv(smb_iconv_t* t, const char* to, const char* from);

/**
 * Parse option string
 * @param[in,out] ptr parse position
 * @param[in] mem_ctx talloc context
 * @param[out] name ptr 2 value
 * @param[out] value ptr 2 value
 * @return true on success
 */
bool srprs_option(const char** ptr, const void* mem_ctx, char** name, char** value);

/**
 * Write Byte Order Mark for \p charset to file.
 * If \c charset==NULL write BOM for \p ctype.
 *
 * @param[in] file file to write to
 * @param[in] charset
 * @param[in] ctype
 *
 * @return number of bytes written, -1 on error
 * @todo write to cbuf
 */
int write_bom(FILE* file, const char* charset, charset_t ctype);

/**
 * Parse Byte Order Mark.
 *
 * @param[in,out] ptr parse position
 * @param[out] name name of characterset
 * @param[out] ctype charset_t
 *
 * @return true if found
 * @ingroup parse bom
 */
bool srprs_bom(const char** ptr, const char** name, charset_t* ctype);

enum fmt_case {
	FMT_CASE_PRESERVE=0,
	FMT_CASE_UPPER,
	FMT_CASE_LOWER,
	FMT_CASE_TITLE
};
int cbuf_puts_case(struct cbuf* s, const char* str, size_t len, enum fmt_case fmt);

#endif /* __REG_PARSE_INTERNAL_H */

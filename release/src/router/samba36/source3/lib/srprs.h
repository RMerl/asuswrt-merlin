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
 * @file   srprs.h
 * @author Gregor Beck <gb@sernet.de>
 * @date   Aug 2010
 *
 * @brief  A simple recursive parser.
 *
 * This file contains functions which may be used to build a simple recursive
 * parser. They all take the parse position as their first argument. If they
 * match the parse position and the output arguments are updated accordingly and
 * true is returned else no argument is altered. For arguments of type @ref cbuf
 * this may hold only up to the current write position.
 */

#ifndef __SRPRS_H
#define __SRPRS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
struct cbuf;

/**
 * Matches any amount of whitespace.
 *
 * @see isspace
 * @param ptr parse position
 *
 * @return true
 */
bool srprs_skipws(const char** ptr);

/**
 * Match a single character.
 *
 * @param[in,out] ptr parse position
 * @param         c   the character to match
 *
 * @return true if matched
 */
bool srprs_char(const char** ptr, char c);

/**
 * Match a string.
 *
 * @param[in,out] ptr parse position
 * @param str string to match
 * @param len number of bytes to compare, -1 means strlen(str)
 *
 * @return true if matched
 */
bool srprs_str(const char** ptr, const char* str, size_t len);

/**
 * Match a single character from a set.
 * Didn't match '\\0'
 *
 * @param[in,out] ptr parse position
 * @param[in] set the character set to look for
 * @param[out] oss output buffer where to put the match, may be NULL
 *
 * @return true if matched
 */
bool srprs_charset(const char** ptr, const char* set, struct cbuf* oss);

/**
 * Match a single character not in set.
 * Didn't match '\\0'
 *
 * @param[in,out] ptr parse position
 * @param[in] set the character set to look for
 * @param[out] oss output buffer where to put the match, may be NULL
 *
 * @return true if matched
 */
bool srprs_charsetinv(const char** ptr, const char* set, struct cbuf* oss);

/**
 * Match a quoted string.
 *
 *
 * If cont is not NULL the match may span multiple invocations.
 * @code
 * const char* start = "\"start...";
 * const char* cont  = "continued...";
 * const char* end   = "end\"";
 * bool  cont = false;
 * cbuf* out  = cbuf_new(talloc_tos());
 * srprs_quoted_string(&start, out, &cont);
 * assert(*cont == true);
 * srprs_quoted_string(&cont, out, &cont);
 * assert(*cont == true);
 * srprs_quoted_string(&end, out, &cont);
 * assert(*cont == false);
 * assert(strcmp(cbuf_gets(out, 0), "start...continued...end")==0);
 * @endcode
 * @see cbuf_print_quoted_string
 *
 * @param[in,out] ptr parse position
 * @param[out] str output buffer where to put the match
 * @param[in,out] cont
 *
 * @return true if matched
 */
bool srprs_quoted_string(const char** ptr, struct cbuf* str, bool* cont);

/**
 * Match a hex string.
 *
 * @param[in,out] ptr parse position
 * @param         len maximum number of diggits to match
 * @param[out]    u   value of the match
 *
 * @return true if matched
 */
bool srprs_hex(const char** ptr, size_t len, unsigned* u);

/**
 * Match the empty string at End Of String.
 * It doesn't consume the '\\0' unlike
 * @code
 *   srprs_char(ptr, '\0', NULL);
 * @endcode
 *
 * @param[in,out] ptr parse position
 *
 * @return true if **ptr == '\\0'
 */
bool srprs_eos(const char** ptr);

/**
 * Match a newline.
 * A newline is either '\\n' (LF), '\\r' (CR), or "\r\n" (CRLF)
 *
 * @param[in,out] ptr parse position
 * @param[out]    nl  output buffer where to put the match, may be NULL
 *
 * @return true if matched
 */
bool srprs_nl(const char** ptr, struct cbuf* nl);

/**
 * Match a newline or eos.
 *
 * @param ptr parse position
 * @param nl  output buffer where to put the match, may be NULL
 *
 * @return true if matched
 */
bool srprs_eol(const char** ptr, struct cbuf* nl);

/**
 * Match a line up to but not including the newline.
 *
 * @param[in,out] ptr parse position
 * @param[out]    str output buffer where to put the match, may be NULL
 *
 * @return true
 */
bool srprs_line(const char** ptr, struct cbuf* str);

/**
 * Match a quoted string with escaped characters.
 * @see cbuf_print_quoted
 *
 * @param[in,out] ptr parse position
 * @param[out] str output buffer where to put the match
 *
 * @return true if matched
 */
bool srprs_quoted(const char** ptr, struct cbuf* str);

#endif /* __SRPRS_H */

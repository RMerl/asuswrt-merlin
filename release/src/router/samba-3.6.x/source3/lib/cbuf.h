/*
 * Samba Unix/Linux SMB client library
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
 * @file   cbuf.h
 * @author Gregor Beck <gb@sernet.de>
 * @date   Aug 2010
 *
 * @brief  A talloced character buffer.
 *
 * A cbuf carries a write position and keeps track of its size.
 */

#ifndef __CBUF_H
#define __CBUF_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


struct cbuf;
typedef struct cbuf cbuf;

/**
 * Create a new character buffer.
 *
 * @param talloc_ctx the talloc parent
 *
 * @return a new cbuf object, NULL on error
 */
cbuf* cbuf_new(const void* talloc_ctx);

/**
 * Create a copy of a character buffer.
 *
 * @param b the cbuf to copy
 * @return a new cbuf object, NULL on error
 */
cbuf* cbuf_copy(const cbuf* b);

/**
 * Delete a character buffer.
 * This invalidates b and free's the memory allocated.
 * @warning don't talloc_free b directly, however freeing
 *          the parent works as expected
 * @param b the cbuf to delete
 */
void  cbuf_delete(cbuf* b);

/**
 * Reset the buffer to initial state.
 * Set the write positon to the start of buffer, effectivly
 * clearing its contents. Doesn't free memory.
 *
 * @param b the buffer to clear
 *
 * @return b
 */
cbuf* cbuf_clear(cbuf* b);

/**
 * Swap the contents of two buffers in O(1).
 *
 * @param b1 a character buffer
 * @param b2 another character buffer
 */
void cbuf_swap(cbuf* b1, cbuf* b2);

/**
 * Swap the contents of a buffer with a talloced string.
 *
 * @param b a character buffer
 * @param ptr a pointer to a talloced string
 * @param len size of string, -1 means strlen(*ptr)
 *
 * @return b
 */
cbuf* cbuf_swapptr(cbuf* b, char** ptr, size_t len);

/**
 * Let a character buffer takeover the contents of another.
 * This is equivalent to @code
 *   cbuf_swap(b1, b2);
 *   cbuf_delete(b2);
 * @endcode
 * @param b1 the destination
 * @param b2 the victim
 *
 * @return b1
 */
cbuf* cbuf_takeover(cbuf* b1, cbuf* b2);

/**
 * Resize a character buffer.
 * This may free allocated memory.
 *
 * @param b the character buffer.
 * @param size the new size
 *
 * @return b, NULL on error
 */
cbuf* cbuf_resize(cbuf* b, size_t size);

/**
 * Reserve space in a character buffer.
 * Assert there are at least len bytes following the current write position.
 *
 * @param b a character buffer
 * @param len number of bytes to reserve.
 *
 * @return a pointer to the current write position, NULL on error
 */
char* cbuf_reserve(cbuf* b, size_t len);

/**
 * Put a character into the buffer.
 *
 * @param b a charcter buffer, may be NULL.
 * @param c a character
 * @return number of charcters written ((b==NULL) ? 0 : 1)
 *
 * @retval -1 on error
 */
int cbuf_putc(cbuf* b, char c);

/**
 * Put a string into the buffer.
 *
 * @param b a character buffer, may be NULL
 * @param str a string
 * @param len number of bytes to write, -1 means strlen(str)
 *
 * @return number of characters written, -1 on error
 */
int cbuf_puts(cbuf* b, const char* str, size_t len);

/* /\** */
/*  * Put a string into the buffer, changing case. */
/*  * */
/*  * @param b a character buffer, may be NULL */
/*  * @param str a string */
/*  * @param len number of bytes to write, -1 means strlen(str) */
/*  * @param c   a character specifying case: */
/*  *        @li 'U' upper case */
/*  *        @li 'L' lower case */
/*  *        @li 'T' title case */
/*  *        @li 'P' preserve case */
/*  * @return number of characters written, -1 on error */
/*  *\/ */
/* int cbuf_puts_case(cbuf* b, const char* str, size_t len, char c); */



/**
 * Put a uint32 into the buffer.
 * Write in little endian order.
 *
 * @param b a character buffer, may be NULL
 * @param u an uint32
 *
 * @return number of characters written, -1 on error
 */
int cbuf_putdw(cbuf* b, uint32_t u);

/**
 * Print formated to a character buffer.
 *
 * @param b a charcter buffer
 * @param fmt a printf format string
 *
 * @return number of characters written, negative on error
 */
int cbuf_printf(cbuf* b, const char* fmt, ...);


/**
 * Get the current write position.
 *
 * @param b a character buffer.
 *
 * @return index of the next charcter to write.
 */
size_t cbuf_getpos(const cbuf* b);

/**
 * Set the current write position of a buffer.
 * Invalidates the buffer contents from on the new position.
 *
 * @param b a charcter buffer
 * @param pos a position obtained by cbuf_getpos
 */
void cbuf_setpos(cbuf* b, size_t pos);

/**
 * Get the buffer contents
 * starting at idx.
 * @pre @code idx <= cbuf_getpos(b) @endcode
 * @param b a character buffer
 * @param idx a position obtained by cbuf_getpos
 *
 * @return a NUL terminated string
 */
char* cbuf_gets(cbuf* b, size_t idx);

/**
 * Print quoted string to stream.
 *
 * @todo check for ssputc failure
 * @see srprs_quoted_string
 *
 * @param[out] ost outstream
 * @param[in]  s '\0' terminated string of printable characters.
 *
 * @return numner of bytes written, -1 on error
 */
int cbuf_print_quoted_string(cbuf* ost, const char* s);

/**
 * Print quoted string to stream.
 * Escapes nonprintable characters.
 *
 * @todo check for ssputc failure
 * @see srprs_quoted
 *
 * @param[out] ost outstream
 * @param[in]  s string of bytes
 * @param[in]  len number of bytes
 *
 * @return numner of bytes written, -1 on error
 */
int cbuf_print_quoted(cbuf* ost, const char* s, size_t len);


#endif /*__CBUF_H*/

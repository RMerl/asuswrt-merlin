/*  Copyright (C) 2008  Denys Vlasenko  <vda.linux@googlemail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 */

#ifndef _BITS_UCLIBC_CHARCLASS_H
#define _BITS_UCLIBC_CHARCLASS_H

/* Taking advantage of the C99 mutual-exclusion guarantees for the various
 * (w)ctype classes, including the descriptions of printing and control
 * (w)chars, we can place each in one of the following mutually-exlusive
 * subsets.  Since there are less than 16, we can store the data for
 * each (w)chars in a nibble. In contrast, glibc uses an unsigned int
 * per (w)char, with one bit flag for each is* type.  While this allows
 * a simple '&' operation to determine the type vs. a range test and a
 * little special handling for the "blank" and "xdigit" types in my
 * approach, it also uses 8 times the space for the tables on the typical
 * 32-bit archs we supported.*/
enum {
	__CTYPE_unclassified = 0,
	__CTYPE_alpha_nonupper_nonlower,
	__CTYPE_alpha_lower,
	__CTYPE_alpha_upper_lower,
	__CTYPE_alpha_upper,
	__CTYPE_digit,
	__CTYPE_punct,
	__CTYPE_graph,
	__CTYPE_print_space_nonblank,
	__CTYPE_print_space_blank,
	__CTYPE_space_nonblank_noncntrl,
	__CTYPE_space_blank_noncntrl,
	__CTYPE_cntrl_space_nonblank,
	__CTYPE_cntrl_space_blank,
	__CTYPE_cntrl_nonspace
};

#endif

/*  Copyright (C) 2002, 2003     Manuel Novoa III
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  The GNU C Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with the GNU C Library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA.
 */

/*  ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!
 *
 *  Besides uClibc, I'm using this code in my libc for elks, which is
 *  a 16-bit environment with a fairly limited compiler.  It would make
 *  things much easier for me if this file isn't modified unnecessarily.
 *  In particular, please put any new or replacement functions somewhere
 *  else, and modify the makefile to use your version instead.
 *  Thanks.  Manuel
 *
 *  ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION! */

#ifndef _UCLIBC_LOCALE_H
#define _UCLIBC_LOCALE_H

/**********************************************************************/
/* uClibc compatibilty stuff */

#ifdef __UCLIBC_HAS_LOCALE__

#undef __LOCALE_C_ONLY

#else  /* __UCLIBC_HAS_LOCALE__ */

#define __LOCALE_C_ONLY 

#define __XL_NPP(N) N
#define __LOCALE_PARAM
#define __LOCALE_ARG

#endif /* __UCLIBC_HAS_LOCALE__ */

/**********************************************************************/

#define __NL_ITEM_CATEGORY_SHIFT		(8)
#define __NL_ITEM_INDEX_MASK			(0xff)

/* TODO: Make sure these agree with the locale mmap file gererator! */

#define __LC_CTYPE			0
#define __LC_NUMERIC		1
#define __LC_MONETARY		2
#define __LC_TIME			3
#define __LC_COLLATE		4
#define __LC_MESSAGES		5
#define __LC_ALL			6

/**********************************************************************/
#ifndef __LOCALE_C_ONLY

#if defined _LIBC /* && (defined IS_IN_libc || defined NOT_IN_libc) */
#include <stddef.h>
#include <stdint.h>
#include <bits/uClibc_touplow.h>

#ifndef __UCLIBC_GEN_LOCALE
#include <bits/uClibc_locale_data.h>
#endif
#endif

/* extern void _locale_set(const unsigned char *p); */
/* extern void _locale_init(void); */

enum {
	__ctype_encoding_7_bit,		/* C/POSIX */
	__ctype_encoding_utf8,		/* UTF-8 */
	__ctype_encoding_8_bit		/* for 8-bit codeset locales */
};

#define LOCALE_STRING_SIZE (2 * __LC_ALL + 2)

 /*
  * '#' + 2_per_category + '\0'
  *       {locale row # : 0 = C|POSIX} + 0x8001
  *       encoded in two chars as (((N+1) >> 8) | 0x80) and ((N+1) & 0xff)
  *       so decode is  ((((uint16_t)(*s & 0x7f)) << 8) + s[1]) - 1
  *
  *       Note: 0s are not used as they are nul-terminators for strings.
  *       Note: 0xff, 0xff is the encoding for a non-selected locale.
  *             (see setlocale() below).
  * In particular, C/POSIX locale is '#' + "\x80\x01"}*LC_ALL + nul.
  */

#if defined _LIBC && !defined __UCLIBC_GEN_LOCALE /* && (defined IS_IN_libc || defined NOT_IN_libc) */
typedef struct {
	uint16_t num_weights;
	uint16_t num_starters;
	uint16_t ii_shift;
	uint16_t ti_shift;
	uint16_t ii_len;
	uint16_t ti_len;
	uint16_t max_weight;
	uint16_t num_col_base;
	uint16_t max_col_index;
	uint16_t undefined_idx;
	uint16_t range_low;
	uint16_t range_count;
	uint16_t range_base_weight;
	uint16_t range_rule_offset; /* change name to index? */

	uint16_t ii_mask;
	uint16_t ti_mask;

	const uint16_t *index2weight_tbl;
	const uint16_t *index2ruleidx_tbl;
	const uint16_t *multistart_tbl;
	/*	 uint16_t wcs2colidt_offset_low; */
	/*	 uint16_t wcs2colidt_offset_hi; */
	const uint16_t *wcs2colidt_tbl;

	/*	 uint16_t undefined_idx; */
	const uint16_t *overrides_tbl;
	/*	 uint16_t *multistart_tbl; */

	const uint16_t *weightstr;
	const uint16_t *ruletable;


	uint16_t *index2weight;
	uint16_t *index2ruleidx;

	uint16_t MAX_WEIGHTS;
} __collate_t;


/*  static unsigned char cur_locale[LOCALE_STRING_SIZE]; */

typedef struct __uclibc_locale_struct {
#ifdef __UCLIBC_HAS_XLOCALE__
	const __ctype_mask_t *__ctype_b;
	const __ctype_touplow_t *__ctype_tolower;
	const __ctype_touplow_t *__ctype_toupper;
#endif

	/* For now, just embed this in the structure. */
	__ctype_mask_t __ctype_b_data[256 + __UCLIBC_CTYPE_B_TBL_OFFSET];
	__ctype_touplow_t __ctype_tolower_data[256 + __UCLIBC_CTYPE_TO_TBL_OFFSET];
	__ctype_touplow_t __ctype_toupper_data[256 + __UCLIBC_CTYPE_TO_TBL_OFFSET];

/*  	int tables_loaded; */
/*  	unsigned char lctypes[LOCALE_STRING_SIZE]; */
	unsigned char cur_locale[LOCALE_STRING_SIZE];

	/* NL_LANGINFO stuff. BEWARE ORDERING!!! must agree with NL_* constants! */
	/* Also, numeric must be followed by monetary and the items must be in
	 * the "struct lconv" order. */

	uint16_t category_offsets[__LC_ALL]; /* TODO -- fix? */
	unsigned char category_item_count[__LC_ALL]; /* TODO - fix */

	/* ctype */
	unsigned char encoding;		/* C/POSIX, 8-bit, UTF-8 */
	unsigned char mb_cur_max;	/* determined by encoding _AND_ translit!!! */
	const unsigned char outdigit_length[10];

#ifdef __CTYPE_HAS_8_BIT_LOCALES
	const unsigned char *idx8ctype;
	const unsigned char *tbl8ctype;
	const unsigned char *idx8uplow;
    const unsigned char *tbl8uplow;
#ifdef __UCLIBC_HAS_WCHAR__
	const unsigned char *idx8c2wc;
	const uint16_t *tbl8c2wc;	/* char > 0x7f to wide char */
	const unsigned char *idx8wc2c;
	const unsigned char *tbl8wc2c;
	/* translit  */
#endif /* __UCLIBC_HAS_WCHAR__ */
#endif /* __CTYPE_HAS_8_BIT_LOCALES */
#ifdef __UCLIBC_HAS_WCHAR__

	const uint16_t *code2flag;

	const unsigned char *tblwctype;
	const unsigned char *tblwuplow;
/* 	const unsigned char *tblwcomb; */
	const int16_t *tblwuplow_diff; /* yes... signed */
	/* width?? */

	wchar_t decimal_point_wc;
	wchar_t thousands_sep_wc;
	int decimal_point_len;
	int thousands_sep_len;

#endif /* __UCLIBC_HAS_WCHAR__ */

	/* ctype */
	const char *outdigit0_mb;
	const char *outdigit1_mb;
	const char *outdigit2_mb;
	const char *outdigit3_mb;
	const char *outdigit4_mb;
	const char *outdigit5_mb;
	const char *outdigit6_mb;
	const char *outdigit7_mb;
	const char *outdigit8_mb;
	const char *outdigit9_mb;
	const char *codeset;		/* MUST BE LAST!!! */

	/* numeric */
	const char *decimal_point;
	const char *thousands_sep;
	const char *grouping;

	/* monetary */
	const char *int_curr_symbol;
	const char *currency_symbol;
	const char *mon_decimal_point;
	const char *mon_thousands_sep;
	const char *mon_grouping;
	const char *positive_sign;
	const char *negative_sign;
	const char *int_frac_digits;
	const char *frac_digits;
	const char *p_cs_precedes;
	const char *p_sep_by_space;
	const char *n_cs_precedes;
	const char *n_sep_by_space;
	const char *p_sign_posn;
	const char *n_sign_posn;
	const char *int_p_cs_precedes;
	const char *int_p_sep_by_space;
	const char *int_n_cs_precedes;
	const char *int_n_sep_by_space;
	const char *int_p_sign_posn;
	const char *int_n_sign_posn;

	const char *crncystr;		/* not returned by localeconv */

	/* time */
	const char *abday_1;
	const char *abday_2;
	const char *abday_3;
	const char *abday_4;
	const char *abday_5;
	const char *abday_6;
	const char *abday_7;

	const char *day_1;
	const char *day_2;
	const char *day_3;
	const char *day_4;
	const char *day_5;
	const char *day_6;
	const char *day_7;

	const char *abmon_1;
	const char *abmon_2;
	const char *abmon_3;
	const char *abmon_4;
	const char *abmon_5;
	const char *abmon_6;
	const char *abmon_7;
	const char *abmon_8;
	const char *abmon_9;
	const char *abmon_10;
	const char *abmon_11;
	const char *abmon_12;

	const char *mon_1;
	const char *mon_2;
	const char *mon_3;
	const char *mon_4;
	const char *mon_5;
	const char *mon_6;
	const char *mon_7;
	const char *mon_8;
	const char *mon_9;
	const char *mon_10;
	const char *mon_11;
	const char *mon_12;

	const char *am_str;
	const char *pm_str;

	const char *d_t_fmt;
	const char *d_fmt;
	const char *t_fmt;
	const char *t_fmt_ampm;
	const char *era;

	const char *era_year;		/* non SUSv3 */
	const char *era_d_fmt;
	const char *alt_digits;
	const char *era_d_t_fmt;
	const char *era_t_fmt;

	/* collate is at the end */

	/* messages */
	const char *yesexpr;
	const char *noexpr;
	const char *yesstr;
	const char *nostr;

	/* collate is at the end */
	__collate_t collate;

} __uclibc_locale_t;

extern __uclibc_locale_t __global_locale_data;
extern struct __uclibc_locale_struct * __global_locale;
#endif /* _LIBC */

typedef struct __uclibc_locale_struct *__locale_t;

/* if we need to leave only _LIBC, then attribute_hidden is not usable */
#if defined _LIBC && (defined IS_IN_libc || defined NOT_IN_libc)
extern int __locale_mbrtowc_l(wchar_t *__restrict dst,
							  const char *__restrict src,
							  __locale_t loc ) attribute_hidden;
#endif

#ifdef L_setlocale
/* so we only get the warning once... */
#warning need thread version of CUR_LOCALE!
#endif
/**********************************************************************/
#ifdef __UCLIBC_HAS_XLOCALE__

extern __locale_t __curlocale_var;

#ifdef __UCLIBC_HAS_THREADS__

extern __locale_t __curlocale(void)  __THROW __attribute__ ((__const__));
extern __locale_t __curlocale_set(__locale_t newloc);
#define __UCLIBC_CURLOCALE           (__curlocale())
#define __UCLIBC_CURLOCALE_DATA      (*__curlocale())

#else  /* __UCLIBC_HAS_THREADS__ */

#define __UCLIBC_CURLOCALE           (__curlocale_var)
#define __UCLIBC_CURLOCALE_DATA      (*__curlocale_var)

#endif /* __UCLIBC_HAS_THREADS__ */

#elif defined(__UCLIBC_HAS_LOCALE__)

#define __UCLIBC_CURLOCALE           (__global_locale)
#define __UCLIBC_CURLOCALE_DATA      (*__global_locale)

#endif
/**********************************************************************/
#if defined(__UCLIBC_HAS_XLOCALE__) && defined(__UCLIBC_DO_XLOCALE)

#define __XL_NPP(N) N ## _l
#define __LOCALE_PARAM    , __locale_t locale_arg
#define __LOCALE_ARG      , locale_arg
#define __LOCALE_PTR      locale_arg

#else  /* defined(__UCLIBC_HAS_XLOCALE__) && defined(__UCLIBC_DO_XLOCALE) */

#define __XL_NPP(N) N
#define __LOCALE_PARAM
#define __LOCALE_ARG
#define __LOCALE_PTR      __UCLIBC_CURLOCALE

#endif /* defined(__UCLIBC_HAS_XLOCALE__) && defined(__UCLIBC_DO_XLOCALE) */
/**********************************************************************/

#endif /* !defined(__LOCALE_C_ONLY) */
/**********************************************************************/

#endif /* _UCLIBC_LOCALE_H */

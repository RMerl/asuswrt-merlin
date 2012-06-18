/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Kanji Extensions
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Adding for Japanese language by <fujita@ainix.isac.co.jp> 1994.9.5
     and extend coding system to EUC/SJIS/JIS/HEX at 1994.10.11
     and add all jis codes sequence type at 1995.8.16
     Notes: Hexadecimal code by <ohki@gssm.otuka.tsukuba.ac.jp>
*/

#define _KANJI_C_
#include "includes.h"

/*
 * Function pointers that get overridden when multi-byte code pages
 * are loaded.
 */

const char *(*multibyte_strchr)(const char *, int ) = (const char *(*)(const char *, int )) strchr;
const char *(*multibyte_strrchr)(const char *, int ) = (const char *(*)(const char *, int )) strrchr;
const char *(*multibyte_strstr)(const char *, const char *) = (const char *(*)(const char *, const char *)) strstr;
char *(*multibyte_strtok)(char *, const char *) = (char *(*)(char *, const char *)) strtok;

/*
 * Kanji is treated differently here due to historical accident of
 * it being the first non-English codepage added to Samba.
 * The define 'KANJI' is being overloaded to mean 'use kanji codepage
 * by default' and also 'this is the filename-to-disk conversion 
 * method to use'. This really should be removed and all control
 * over this left in the smb.conf parameters 'client codepage'
 * and 'coding system'.
 */

#ifndef KANJI

/*
 * Set the default conversion to be the functions in
 * charcnv.c.
 */

static size_t skip_non_multibyte_char(char);
static BOOL not_multibyte_char_1(char);

char *(*_dos_to_unix)(char *, BOOL) = dos2unix_format;
char *(*_unix_to_dos)(char *, BOOL) = unix2dos_format;
size_t (*_skip_multibyte_char)(char) = skip_non_multibyte_char;
BOOL (*is_multibyte_char_1)(char) = not_multibyte_char_1;

#else /* KANJI */

/*
 * Set the default conversion to be the function
 * sj_to_sj in this file.
 */

static char *sj_to_sj(char *from, BOOL overwrite);
static size_t skip_kanji_multibyte_char(char);
static BOOL is_kanji_multibyte_char_1(char);

char *(*_dos_to_unix)(char *, BOOL) = sj_to_sj;
char *(*_unix_to_dos)(char *, BOOL) = sj_to_sj;
size_t (*_skip_multibyte_char)(char) = skip_kanji_multibyte_char;
int (*is_multibyte_char_1)(char) = is_kanji_multibyte_char_1;

#endif /* KANJI */

BOOL global_is_multibyte_codepage = False;

/* jis si/so sequence */
static char jis_kso = JIS_KSO;
static char jis_ksi = JIS_KSI;
static char hex_tag = HEXTAG;

/*******************************************************************
  SHIFT JIS functions
********************************************************************/

/*******************************************************************
 search token from S1 separated any char of S2
 S1 contains SHIFT JIS chars.
********************************************************************/

static char *sj_strtok(char *s1, const char *s2)
{
  static char *s = NULL;
  char *q;
  if (!s1) {
    if (!s) {
      return NULL;
    }
    s1 = s;
  }
  for (q = s1; *s1; ) {
    if (is_shift_jis (*s1)) {
      s1 += 2;
    } else if (is_kana (*s1)) {
      s1++;
    } else {
      char *p = strchr (s2, *s1);
      if (p) {
        if (s1 != q) {
          s = s1 + 1;
          *s1 = '\0';
          return q;
        }
        q = s1 + 1;
      }
      s1++;
    }
  }
  s = NULL;
  if (*q) {
    return q;
  }
  return NULL;
}

/*******************************************************************
 search string S2 from S1
 S1 contains SHIFT JIS chars.
********************************************************************/

static const char *sj_strstr(const char *s1, const char *s2)
{
  size_t len = strlen (s2);
  if (!*s2) 
    return (const char *) s1;
  for (;*s1;) {
    if (*s1 == *s2) {
      if (strncmp (s1, s2, len) == 0)
        return (const char *) s1;
    }
    if (is_shift_jis (*s1)) {
      s1 += 2;
    } else {
      s1++;
    }
  }
  return NULL;
}

/*******************************************************************
 Search char C from beginning of S.
 S contains SHIFT JIS chars.
********************************************************************/

static const char *sj_strchr (const char *s, int c)
{
  for (; *s; ) {
    if (*s == c)
      return (const char *) s;
    if (is_shift_jis (*s)) {
      s += 2;
    } else {
      s++;
    }
  }
  return NULL;
}

/*******************************************************************
 Search char C end of S.
 S contains SHIFT JIS chars.
********************************************************************/

static const char *sj_strrchr(const char *s, int c)
{
  const char *q;

  for (q = 0; *s; ) {
    if (*s == c) {
      q = (const char *) s;
    }
    if (is_shift_jis (*s)) {
      s += 2;
    } else {
      s++;
    }
  }
  return q;
}

/*******************************************************************
 Kanji multibyte char skip function.
*******************************************************************/
   
static size_t skip_kanji_multibyte_char(char c)
{
  if(is_shift_jis(c)) {
    return 2;
  } else if (is_kana(c)) {
    return 1;
  }
  return 0;
}

/*******************************************************************
 Kanji multibyte char identification.
*******************************************************************/
   
static BOOL is_kanji_multibyte_char_1(char c)
{
  return is_shift_jis(c);
}

/*******************************************************************
 The following functions are the only ones needed to do multibyte
 support for Hangul, Big5 and Simplified Chinese. Most of the
 real work for these codepages is done in the generic multibyte
 functions. The only reason these functions are needed at all
 is that the is_xxx(c) calls are really preprocessor macros.
********************************************************************/

/*******************************************************************
  Hangul (Korean - code page 949) function.
********************************************************************/

static BOOL hangul_is_multibyte_char_1(char c)
{
  return is_hangul(c);
}

/*******************************************************************
  Big5 Traditional Chinese (code page 950) function.
********************************************************************/

static BOOL big5_is_multibyte_char_1(char c)
{
  return is_big5_c1(c);
}

/*******************************************************************
  Simplified Chinese (code page 936) function.
********************************************************************/

static BOOL simpch_is_multibyte_char_1(char c)
{
  return is_simpch_c1(c);
}

/*******************************************************************
  Generic multibyte functions - used by Hangul, Big5 and Simplified
  Chinese codepages.
********************************************************************/

/*******************************************************************
 search token from S1 separated any char of S2
 S1 contains generic multibyte chars.
********************************************************************/

static char *generic_multibyte_strtok(char *s1, const char *s2)
{
  static char *s = NULL;
  char *q;
  if (!s1) {
    if (!s) {
      return NULL;
    }
    s1 = s;
  }
  for (q = s1; *s1; ) {
    if ((*is_multibyte_char_1)(*s1)) {
        s1 += 2;
    } else {
      char *p = strchr (s2, *s1);
      if (p) {
        if (s1 != q) {
          s = s1 + 1;
          *s1 = '\0';
          return q;
        }
        q = s1 + 1;
      }
    s1++;
    }
  }
  s = NULL;
  if (*q) {
    return q;
  }
  return NULL;
}

/*******************************************************************
 search string S2 from S1
 S1 contains generic multibyte chars.
********************************************************************/

static const char *generic_multibyte_strstr(const char *s1, const char *s2)
{
  size_t len = strlen (s2);
  if (!*s2)
    return (const char *) s1;
  for (;*s1;) {
    if (*s1 == *s2) {
      if (strncmp (s1, s2, len) == 0)
        return (const char *) s1;
    }
    if ((*is_multibyte_char_1)(*s1)) {
      s1 += 2;
    } else {
      s1++;
    }
  }
  return NULL;
}

/*******************************************************************
 Search char C from beginning of S.
 S contains generic multibyte chars.
********************************************************************/

static const char *generic_multibyte_strchr(const char *s, int c)
{
  for (; *s; ) {
    if (*s == c)
      return (const char *) s;
    if ((*is_multibyte_char_1)(*s)) {
      s += 2;
    } else {
      s++;
    }
  }
  return NULL;
}

/*******************************************************************
 Search char C end of S.
 S contains generic multibyte chars.
********************************************************************/

static const char *generic_multibyte_strrchr(const char *s, int c)
{
  const char *q;
 
  for (q = 0; *s; ) {
    if (*s == c) {
      q = (const char *) s;
    }
    if ((*is_multibyte_char_1)(*s)) {
      s += 2;
    } else {
      s++;
    }
  }
  return q;
}

/*******************************************************************
 Generic multibyte char skip function.
*******************************************************************/

static size_t skip_generic_multibyte_char(char c)
{
  if( (*is_multibyte_char_1)(c)) {
    return 2;
  }
  return 0;
}

/*******************************************************************
  Code conversion
********************************************************************/

/* convesion buffer */
static char cvtbuf[2*sizeof(pstring)];

/*******************************************************************
  EUC <-> SJIS
********************************************************************/

static int euc2sjis (int hi, int lo)
{
  if (hi & 1)
    return ((hi / 2 + (hi < 0xdf ? 0x31 : 0x71)) << 8) |
            (lo - (lo >= 0xe0 ? 0x60 : 0x61));
  else
    return ((hi / 2 + (hi < 0xdf ? 0x30 : 0x70)) << 8) | (lo - 2);
}

static int sjis2euc (int hi, int lo)
{
  if (lo >= 0x9f)
    return ((hi * 2 - (hi >= 0xe0 ? 0xe0 : 0x60)) << 8) | (lo + 2);
  else
    return ((hi * 2 - (hi >= 0xe0 ? 0xe1 : 0x61)) << 8) |
            (lo + (lo >= 0x7f ? 0x60 : 0x61));
}

/*******************************************************************
 Convert FROM contain SHIFT JIS codes to EUC codes
 return converted buffer
********************************************************************/

static char *sj_to_euc(char *from, BOOL overwrite)
{
  char *out;
  char *save;

  save = (char *) from;
  for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-3);) {
    if (is_shift_jis (*from)) {
      int code = sjis2euc ((int) from[0] & 0xff, (int) from[1] & 0xff);
      *out++ = (code >> 8) & 0xff;
      *out++ = code;
      from += 2;
    } else if (is_kana (*from)) {
      *out++ = (char)euc_kana;
      *out++ = *from++;
    } else {
      *out++ = *from++;
    }
  }
  *out = 0;
  if (overwrite) {
    pstrcpy((char *) save, (char *) cvtbuf);
    return (char *) save;
  } else {
    return cvtbuf;
  }
}

/*******************************************************************
 Convert FROM contain EUC codes to SHIFT JIS codes
 return converted buffer
********************************************************************/

static char *euc_to_sj(char *from, BOOL overwrite)
{
  char *out;
  char *save;

  save = (char *) from;
  for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-3); ) {
    if (is_euc (*from)) {
      int code = euc2sjis ((int) from[0] & 0xff, (int) from[1] & 0xff);
      *out++ = (code >> 8) & 0xff;
      *out++ = code;
      from += 2;
    } else if (is_euc_kana (*from)) {
      *out++ = from[1];
      from += 2;
    } else {
      *out++ = *from++;
    }
  }
  *out = 0;
  if (overwrite) {
    pstrcpy(save, (char *) cvtbuf);
    return save;
  } else {
    return cvtbuf;
  }
}

/*******************************************************************
  JIS7,JIS8,JUNET <-> SJIS
********************************************************************/

static int sjis2jis(int hi, int lo)
{
  if (lo >= 0x9f)
    return ((hi * 2 - (hi >= 0xe0 ? 0x160 : 0xe0)) << 8) | (lo - 0x7e);
  else
    return ((hi * 2 - (hi >= 0xe0 ? 0x161 : 0xe1)) << 8) |
            (lo - (lo >= 0x7f ? 0x20 : 0x1f));
}

static int jis2sjis(int hi, int lo)
{
  if (hi & 1)
    return ((hi / 2 + (hi < 0x5f ? 0x71 : 0xb1)) << 8) |
            (lo + (lo >= 0x60 ? 0x20 : 0x1f));
  else
    return ((hi / 2 + (hi < 0x5f ? 0x70 : 0xb0)) << 8) | (lo + 0x7e);
}

/*******************************************************************
 Convert FROM contain JIS codes to SHIFT JIS codes
 return converted buffer
********************************************************************/

static char *jis8_to_sj(char *from, BOOL overwrite)
{
  char *out;
  int shifted;
  char *save;

  shifted = _KJ_ROMAN;
  save = (char *) from;
  for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-3);) {
    if (is_esc (*from)) {
      if (is_so1 (from[1]) && is_so2 (from[2])) {
        shifted = _KJ_KANJI;
        from += 3;
      } else if (is_si1 (from[1]) && is_si2 (from[2])) {
        shifted = _KJ_ROMAN;
        from += 3;
      } else { /* sequence error */
        goto normal;
      }
    } else {

normal:

      switch (shifted) {
      default:
      case _KJ_ROMAN:
        *out++ = *from++;
        break;
      case _KJ_KANJI:
        {
          int code = jis2sjis ((int) from[0] & 0xff, (int) from[1] & 0xff);
          *out++ = (code >> 8) & 0xff;
          *out++ = code;
          from += 2;
          break;
        }
      }
    }
  }

  *out = 0;
  if (overwrite) {
    pstrcpy (save, (char *) cvtbuf);
    return save;
  } else {
    return cvtbuf;
  }
}

/*******************************************************************
 Convert FROM contain SHIFT JIS codes to JIS codes
 return converted buffer
********************************************************************/

static char *sj_to_jis8(char *from, BOOL overwrite)
{
  char *out;
  int shifted;
  char *save;

  shifted = _KJ_ROMAN;
  save = (char *) from;
  for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-4); ) {
    if (is_shift_jis (*from)) {
      int code;
      switch (shifted) {
      case _KJ_ROMAN: /* to KANJI */
        *out++ = jis_esc;
        *out++ = jis_so1;
        *out++ = jis_kso;
        shifted = _KJ_KANJI;
        break;
      }
      code = sjis2jis ((int) from[0] & 0xff, (int) from[1] & 0xff);
      *out++ = (code >> 8) & 0xff;
      *out++ = code;
      from += 2;
    } else {
      switch (shifted) {
      case _KJ_KANJI: /* to ROMAN/KANA */
        *out++ = jis_esc;
        *out++ = jis_si1;
        *out++ = jis_ksi;
        shifted = _KJ_ROMAN;
        break;
      }
      *out++ = *from++;
    }
  }

  switch (shifted) {
  case _KJ_KANJI: /* to ROMAN/KANA */
    *out++ = jis_esc;
    *out++ = jis_si1;
    *out++ = jis_ksi;
    shifted = _KJ_ROMAN;
    break;
  }
  *out = 0;
  if (overwrite) {
    pstrcpy (save, (char *) cvtbuf);
    return save;
  } else {
    return cvtbuf;
  }
}

/*******************************************************************
 Convert FROM contain 7 bits JIS codes to SHIFT JIS codes
 return converted buffer
********************************************************************/
static char *jis7_to_sj(char *from, BOOL overwrite)
{
    char *out;
    int shifted;
    char *save;

    shifted = _KJ_ROMAN;
    save = (char *) from;
    for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-3);) {
	if (is_esc (*from)) {
	    if (is_so1 (from[1]) && is_so2 (from[2])) {
		shifted = _KJ_KANJI;
		from += 3;
	    } else if (is_si1 (from[1]) && is_si2 (from[2])) {
		shifted = _KJ_ROMAN;
		from += 3;
	    } else {			/* sequence error */
		goto normal;
	    }
	} else if (is_so (*from)) {
	    shifted = _KJ_KANA;		/* to KANA */
	    from++;
	} else if (is_si (*from)) {
	    shifted = _KJ_ROMAN;	/* to ROMAN */
	    from++;
	} else {
	normal:
	    switch (shifted) {
	    default:
	    case _KJ_ROMAN:
		*out++ = *from++;
		break;
	    case _KJ_KANJI:
		{
		    int code = jis2sjis ((int) from[0] & 0xff, (int) from[1] & 0xff);
		    *out++ = (code >> 8) & 0xff;
		    *out++ = code;
		    from += 2;
		}
		break;
	    case _KJ_KANA:
		*out++ = ((int) from[0]) + 0x80;
		break;
	    }
	}
    }
    *out = 0;
    if (overwrite) {
	pstrcpy (save, (char *) cvtbuf);
	return save;
    } else {
	return cvtbuf;
    }
}

/*******************************************************************
 Convert FROM contain SHIFT JIS codes to 7 bits JIS codes
 return converted buffer
********************************************************************/
static char *sj_to_jis7(char *from, BOOL overwrite)
{
    char *out;
    int shifted;
    char *save;

    shifted = _KJ_ROMAN;
    save = (char *) from;
    for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-4); ) {
	if (is_shift_jis (*from)) {
	    int code;
	    switch (shifted) {
	    case _KJ_KANA:
		*out++ = jis_si;	/* to ROMAN and through down */
	    case _KJ_ROMAN:		/* to KANJI */
		*out++ = jis_esc;
		*out++ = jis_so1;
		*out++ = jis_kso;
		shifted = _KJ_KANJI;
		break;
	    }
	    code = sjis2jis ((int) from[0] & 0xff, (int) from[1] & 0xff);
	    *out++ = (code >> 8) & 0xff;
	    *out++ = code;
	    from += 2;
	} else if (is_kana (from[0])) {
	    switch (shifted) {
	    case _KJ_KANJI:		/* to ROMAN */
		*out++ = jis_esc;
		*out++ = jis_si1;
		*out++ = jis_ksi;
	    case _KJ_ROMAN:		/* to KANA */
		*out++ = jis_so;
		shifted = _KJ_KANA;
		break;
	    }
	    *out++ = ((int) *from++) - 0x80;
	} else {
	    switch (shifted) {
	    case _KJ_KANA:
		*out++ = jis_si;	/* to ROMAN */
		shifted = _KJ_ROMAN;
		break;
	    case _KJ_KANJI:		/* to ROMAN */
		*out++ = jis_esc;
		*out++ = jis_si1;
		*out++ = jis_ksi;
		shifted = _KJ_ROMAN;
		break;
	    }
	    *out++ = *from++;
	}
    }
    switch (shifted) {
    case _KJ_KANA:
	*out++ = jis_si;		/* to ROMAN */
	break;
    case _KJ_KANJI:			/* to ROMAN */
	*out++ = jis_esc;
	*out++ = jis_si1;
	*out++ = jis_ksi;
	break;
    }
    *out = 0;
    if (overwrite) {
	pstrcpy (save, (char *) cvtbuf);
	return save;
    } else {
	return cvtbuf;
    }
}

/*******************************************************************
 Convert FROM contain 7 bits JIS(junet) codes to SHIFT JIS codes
 return converted buffer
********************************************************************/

static char *junet_to_sj(char *from, BOOL overwrite)
{
    char *out;
    int shifted;
    char *save;

    shifted = _KJ_ROMAN;
    save = (char *) from;
    for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-3);) {
	if (is_esc (*from)) {
	    if (is_so1 (from[1]) && is_so2 (from[2])) {
		shifted = _KJ_KANJI;
		from += 3;
	    } else if (is_si1 (from[1]) && is_si2 (from[2])) {
		shifted = _KJ_ROMAN;
		from += 3;
	    } else if (is_juk1(from[1]) && is_juk2 (from[2])) {
		shifted = _KJ_KANA;
		from += 3;
	    } else {			/* sequence error */
		goto normal;
	    }
	} else {
	normal:
	    switch (shifted) {
	    default:
	    case _KJ_ROMAN:
		*out++ = *from++;
		break;
	    case _KJ_KANJI:
		{
		    int code = jis2sjis ((int) from[0] & 0xff, (int) from[1] & 0xff);
		    *out++ = (code >> 8) & 0xff;
		    *out++ = code;
		    from += 2;
		}
		break;
	    case _KJ_KANA:
		*out++ = ((int) from[0]) + 0x80;
		break;
	    }
	}
    }
    *out = 0;
    if (overwrite) {
	pstrcpy (save, (char *) cvtbuf);
	return save;
    } else {
	return cvtbuf;
    }
}

/*******************************************************************
 Convert FROM contain SHIFT JIS codes to 7 bits JIS(junet) codes
 return converted buffer
********************************************************************/
static char *sj_to_junet(char *from, BOOL overwrite)
{
    char *out;
    int shifted;
    char *save;

    shifted = _KJ_ROMAN;
    save = (char *) from;
    for (out = cvtbuf; *from && (out - cvtbuf < sizeof(cvtbuf)-4); ) {
	if (is_shift_jis (*from)) {
	    int code;
	    switch (shifted) {
	    case _KJ_KANA:
	    case _KJ_ROMAN:		/* to KANJI */
		*out++ = jis_esc;
		*out++ = jis_so1;
		*out++ = jis_so2;
		shifted = _KJ_KANJI;
		break;
	    }
	    code = sjis2jis ((int) from[0] & 0xff, (int) from[1] & 0xff);
	    *out++ = (code >> 8) & 0xff;
	    *out++ = code;
	    from += 2;
	} else if (is_kana (from[0])) {
	    switch (shifted) {
	    case _KJ_KANJI:		/* to ROMAN */
	    case _KJ_ROMAN:		/* to KANA */
		*out++ = jis_esc;
		*out++ = junet_kana1;
		*out++ = junet_kana2;
		shifted = _KJ_KANA;
		break;
	    }
	    *out++ = ((int) *from++) - 0x80;
	} else {
	    switch (shifted) {
	    case _KJ_KANA:
	    case _KJ_KANJI:		/* to ROMAN */
		*out++ = jis_esc;
		*out++ = jis_si1;
		*out++ = jis_si2;
		shifted = _KJ_ROMAN;
		break;
	    }
	    *out++ = *from++;
	}
    }
    switch (shifted) {
    case _KJ_KANA:
    case _KJ_KANJI:			/* to ROMAN */
	*out++ = jis_esc;
	*out++ = jis_si1;
	*out++ = jis_si2;
	break;
    }
    *out = 0;
    if (overwrite) {
	pstrcpy (save, (char *) cvtbuf);
	return save;
    } else {
	return cvtbuf;
    }
}

/*******************************************************************
  HEX <-> SJIS
********************************************************************/
/* ":xx" -> a byte */
static char *hex_to_sj(char *from, BOOL overwrite)
{
    char *sp, *dp;
    
    sp = (char *) from;
    dp = cvtbuf;
    while (*sp && (dp - cvtbuf < sizeof(cvtbuf)-3)) {
	if (*sp == hex_tag && isxdigit((int)sp[1]) && isxdigit((int)sp[2])) {
	    *dp++ = (hex2bin (sp[1])<<4) | (hex2bin (sp[2]));
	    sp += 3;
	} else
	    *dp++ = *sp++;
    }
    *dp = '\0';
    if (overwrite) {
	pstrcpy ((char *) from, (char *) cvtbuf);
	return (char *) from;
    } else {
	return cvtbuf;
    }
}
 
/*******************************************************************
  kanji/kana -> ":xx" 
********************************************************************/
static char *sj_to_hex(char *from, BOOL overwrite)
{
    unsigned char *sp, *dp;
    
    sp = (unsigned char*) from;
    dp = (unsigned char*) cvtbuf;
    while (*sp && (((char *)dp)- cvtbuf < sizeof(cvtbuf)-7)) {
	if (is_kana(*sp)) {
	    *dp++ = hex_tag;
	    *dp++ = bin2hex (((*sp)>>4)&0x0f);
	    *dp++ = bin2hex ((*sp)&0x0f);
	    sp++;
	} else if (is_shift_jis (*sp) && is_shift_jis2 (sp[1])) {
	    *dp++ = hex_tag;
	    *dp++ = bin2hex (((*sp)>>4)&0x0f);
	    *dp++ = bin2hex ((*sp)&0x0f);
	    sp++;
	    *dp++ = hex_tag;
	    *dp++ = bin2hex (((*sp)>>4)&0x0f);
	    *dp++ = bin2hex ((*sp)&0x0f);
	    sp++;
	} else
	    *dp++ = *sp++;
    }
    *dp = '\0';
    if (overwrite) {
	pstrcpy ((char *) from, (char *) cvtbuf);
	return (char *) from;
    } else {
	return cvtbuf;
    }
}

/*******************************************************************
  CAP <-> SJIS
********************************************************************/
/* ":xx" CAP -> a byte */
static char *cap_to_sj(char *from, BOOL overwrite)
{
    char *sp, *dp;

    sp = (char *) from;
    dp = cvtbuf;
    while (*sp && (dp- cvtbuf < sizeof(cvtbuf)-2)) {
        /*
         * The only change between this and hex_to_sj is here. sj_to_cap only
         * translates characters greater or equal to 0x80 - make sure that here
         * we only do the reverse (that's why the strchr is used rather than
         * isxdigit. Based on fix from ado@elsie.nci.nih.gov (Arthur David Olson).
         */
        if (*sp == hex_tag && (strchr ("89abcdefABCDEF", sp[1]) != NULL) && isxdigit((int)sp[2])) {
            *dp++ = (hex2bin (sp[1])<<4) | (hex2bin (sp[2]));
            sp += 3;
        } else
            *dp++ = *sp++;
    }
    *dp = '\0';
    if (overwrite) {
        pstrcpy ((char *) from, (char *) cvtbuf);
        return (char *) from;
    } else {
        return cvtbuf;
    }
}

/*******************************************************************
  kanji/kana -> ":xx" - CAP format.
********************************************************************/
static char *sj_to_cap(char *from, BOOL overwrite)
{
    unsigned char *sp, *dp;

    sp = (unsigned char*) from;
    dp = (unsigned char*) cvtbuf;
    while (*sp && (((char *)dp) - cvtbuf < sizeof(cvtbuf)-4)) {
	if (*sp >= 0x80) {
	    *dp++ = hex_tag;
	    *dp++ = bin2hex (((*sp)>>4)&0x0f);
	    *dp++ = bin2hex ((*sp)&0x0f);
	    sp++;
	} else {
	    *dp++ = *sp++;
	}
    }
    *dp = '\0';
    if (overwrite) {
	pstrcpy ((char *) from, (char *) cvtbuf);
	return (char *) from;
    } else {
	return cvtbuf;
    }
}

/*******************************************************************
 sj to sj
********************************************************************/
static char *sj_to_sj(char *from, BOOL overwrite)
{
    if (!overwrite) {
	pstrcpy (cvtbuf, (char *) from);
	return cvtbuf;
    } else {
	return (char *) from;
    }
}

/*******************************************************************
 cp to utf8
********************************************************************/
static char *cp_to_utf8(char *from, BOOL overwrite)
{
  unsigned char *dst;
  const unsigned char *src;
  smb_ucs2_t val;
  int w;
  size_t len;
  src = (const unsigned char *)from;
  dst = (unsigned char *)cvtbuf;
  while (*src && (((char *)dst - cvtbuf) < sizeof(cvtbuf)-4)) {
    len = _skip_multibyte_char(*src);
    if ( len == 2 ) {
      w = (int)(*src++ & 0xff);
      w = (int)((w << 8)|(*src++ & 0xff));
    } else {
      w = (int)(*src++ & 0xff);
    }
    val = doscp2ucs2(w);

    if ( val <= 0x7f ) {
      *dst++ = (char)(val & 0xff);
    } else if ( val <= 0x7ff ){
      *dst++ = (char)( 0xc0 | ((val >> 6) & 0xff)); 
      *dst++ = (char)( 0x80 | ( val & 0x3f ));
    } else {
      *dst++ = (char)( 0xe0 | ((val >> 12) & 0x0f));
      *dst++ = (char)( 0x80 | ((val >> 6)  & 0x3f));
      *dst++ = (char)( 0x80 | (val & 0x3f));
    }

  }
  *dst++='\0';

    if (overwrite) {
	pstrcpy ((char *) from, (char *) cvtbuf);
	return (char *) from;
    } else {
	return cvtbuf;
    }
}

/*******************************************************************
 utf8 to cp
********************************************************************/
static char *utf8_to_cp(char *from, BOOL overwrite)
{
  const unsigned char *src;
  unsigned char *dst;
  smb_ucs2_t val;
  int w;

  src = (const unsigned char *)from; 
  dst = (unsigned char *)cvtbuf; 

  while (*src && ((char *)dst - cvtbuf < sizeof(cvtbuf)-4)) {
    val = (*src++ & 0xff);
    if (val < 0x80) {
      *dst++ = (char)(val & 0x7f); 
    } else if ((0xc0 <= val) && (val <= 0xdf) 
	       && (0x80 <= *src) && (*src <= 0xbf)) {
      w = ucs2doscp( ((val & 31) << 6)  | ((*src++) & 63 ));
      if (w > 255) *dst++ = (char)((w >> 8) & 0xff);
      *dst++ = (char)(w & 0xff);
    } else {
      val  = (val & 0x0f) << 12;
      val |= ((*src++ & 0x3f) << 6);
      val |= (*src++ & 0x3f);
      w = ucs2doscp(val);
      if (w > 255) *dst++ = (char)((w >> 8) & 0xff);
      *dst++ = (char)(w & 0xff);
    }
  }
  *dst++='\0';

    if (overwrite) {
	pstrcpy ((char *) from, (char *) cvtbuf);
	return (char *) from;
    } else {
	return cvtbuf;
    }
}

/************************************************************************
 conversion:
 _dos_to_unix		_unix_to_dos
************************************************************************/

static void setup_string_function(int codes)
{
    switch (codes) {
    default:
        _dos_to_unix = dos2unix_format;
        _unix_to_dos = unix2dos_format;
        break;

    case UTF8_CODE:
	_dos_to_unix = cp_to_utf8;
	_unix_to_dos = utf8_to_cp;
	break;

    case SJIS_CODE:
	_dos_to_unix = sj_to_sj;
	_unix_to_dos = sj_to_sj;
	break;
	
    case EUC_CODE:
	_dos_to_unix = sj_to_euc;
	_unix_to_dos = euc_to_sj;
	break;
	
    case JIS7_CODE:
	_dos_to_unix = sj_to_jis7;
	_unix_to_dos = jis7_to_sj;
	break;

    case JIS8_CODE:
	_dos_to_unix = sj_to_jis8;
	_unix_to_dos = jis8_to_sj;
	break;

    case JUNET_CODE:
	_dos_to_unix = sj_to_junet;
	_unix_to_dos = junet_to_sj;
	break;

    case HEX_CODE:
	_dos_to_unix = sj_to_hex;
	_unix_to_dos = hex_to_sj;
	break;

    case CAP_CODE:
	_dos_to_unix = sj_to_cap;
	_unix_to_dos = cap_to_sj;
	break;
    }
}

/************************************************************************
 Interpret coding system.
************************************************************************/

void interpret_coding_system(char *str)
{
    int codes = UNKNOWN_CODE;
    
    if (strequal (str, "sjis")) {
	codes = SJIS_CODE;
    } else if (strequal (str, "utf8")) {
	codes = UTF8_CODE;
    } else if (strequal (str, "euc")) {
	codes = EUC_CODE;
    } else if (strequal (str, "cap")) {
	codes = CAP_CODE;
	hex_tag = HEXTAG;
    } else if (strequal (str, "hex")) {
	codes = HEX_CODE;
	hex_tag = HEXTAG;
    } else if (!strncasecmp (str, "hex", 3)) {
	codes = HEX_CODE;
	hex_tag = (str[3] ? str[3] : HEXTAG);
    } else if (strequal (str, "j8bb")) {
	codes = JIS8_CODE;
	jis_kso = 'B';
	jis_ksi = 'B';
    } else if (strequal (str, "j8bj") || strequal (str, "jis8")) {
	codes = JIS8_CODE;
	jis_kso = 'B';
	jis_ksi = 'J';
    } else if (strequal (str, "j8bh")) {
	codes = JIS8_CODE;
	jis_kso = 'B';
	jis_ksi = 'H';
    } else if (strequal (str, "j8@b")) {
	codes = JIS8_CODE;
	jis_kso = '@';
	jis_ksi = 'B';
    } else if (strequal (str, "j8@j")) {
	codes = JIS8_CODE;
	jis_kso = '@';
	jis_ksi = 'J';
    } else if (strequal (str, "j8@h")) {
	codes = JIS8_CODE;
	jis_kso = '@';
	jis_ksi = 'H';
    } else if (strequal (str, "j7bb")) {
	codes = JIS7_CODE;
	jis_kso = 'B';
	jis_ksi = 'B';
    } else if (strequal (str, "j7bj") || strequal (str, "jis7")) {
	codes = JIS7_CODE;
	jis_kso = 'B';
	jis_ksi = 'J';
    } else if (strequal (str, "j7bh")) {
	codes = JIS7_CODE;
	jis_kso = 'B';
	jis_ksi = 'H';
    } else if (strequal (str, "j7@b")) {
	codes = JIS7_CODE;
	jis_kso = '@';
	jis_ksi = 'B';
    } else if (strequal (str, "j7@j")) {
	codes = JIS7_CODE;
	jis_kso = '@';
	jis_ksi = 'J';
    } else if (strequal (str, "j7@h")) {
	codes = JIS7_CODE;
	jis_kso = '@';
	jis_ksi = 'H';
    } else if (strequal (str, "jubb")) {
	codes = JUNET_CODE;
	jis_kso = 'B';
	jis_ksi = 'B';
    } else if (strequal (str, "jubj") || strequal (str, "junet")) {
	codes = JUNET_CODE;
	jis_kso = 'B';
	jis_ksi = 'J';
    } else if (strequal (str, "jubh")) {
	codes = JUNET_CODE;
	jis_kso = 'B';
	jis_ksi = 'H';
    } else if (strequal (str, "ju@b")) {
	codes = JUNET_CODE;
	jis_kso = '@';
	jis_ksi = 'B';
    } else if (strequal (str, "ju@j")) {
	codes = JUNET_CODE;
	jis_kso = '@';
	jis_ksi = 'J';
    } else if (strequal (str, "ju@h")) {
	codes = JUNET_CODE;
	jis_kso = '@';
	jis_ksi = 'H';
    }	
    setup_string_function (codes);
}

/*******************************************************************
 Non multibyte char function.
*******************************************************************/
   
static size_t skip_non_multibyte_char(char c)
{
  return 0;
}

/*******************************************************************
 Function that always says a character isn't multibyte.
*******************************************************************/

static BOOL not_multibyte_char_1(char c)
{
  return False;
}

/*******************************************************************
 Setup the function pointers for the functions that are replaced
 when multi-byte codepages are used.

 The dos_to_unix and unix_to_dos function pointers are only
 replaced by setup_string_function called by interpret_coding_system
 above.
*******************************************************************/

void initialize_multibyte_vectors( int client_codepage)
{
  switch( client_codepage )
  {
  case KANJI_CODEPAGE:
    multibyte_strchr = sj_strchr;
    multibyte_strrchr = sj_strrchr;
    multibyte_strstr = sj_strstr;
    multibyte_strtok = sj_strtok;
    _skip_multibyte_char = skip_kanji_multibyte_char;
    is_multibyte_char_1 = is_kanji_multibyte_char_1;
    global_is_multibyte_codepage = True;
    break;
  case HANGUL_CODEPAGE:
    multibyte_strchr = generic_multibyte_strchr;
    multibyte_strrchr = generic_multibyte_strrchr;
    multibyte_strstr = generic_multibyte_strstr;
    multibyte_strtok = generic_multibyte_strtok;
    _skip_multibyte_char = skip_generic_multibyte_char;
    is_multibyte_char_1 = hangul_is_multibyte_char_1;
    global_is_multibyte_codepage = True;
    break;
  case BIG5_CODEPAGE:
    multibyte_strchr = generic_multibyte_strchr;
    multibyte_strrchr = generic_multibyte_strrchr;
    multibyte_strstr = generic_multibyte_strstr;
    multibyte_strtok = generic_multibyte_strtok;
    _skip_multibyte_char = skip_generic_multibyte_char;
    is_multibyte_char_1 = big5_is_multibyte_char_1;
    global_is_multibyte_codepage = True;
    break;
  case SIMPLIFIED_CHINESE_CODEPAGE:
    multibyte_strchr = generic_multibyte_strchr;
    multibyte_strrchr = generic_multibyte_strrchr;
    multibyte_strstr = generic_multibyte_strstr;
    multibyte_strtok = generic_multibyte_strtok;
    _skip_multibyte_char = skip_generic_multibyte_char;
    is_multibyte_char_1 = simpch_is_multibyte_char_1;
    global_is_multibyte_codepage = True;
    break;
  /*
   * Single char size code page.
   */
  default:
    multibyte_strchr = (const char *(*)(const char *, int )) strchr;
    multibyte_strrchr = (const char *(*)(const char *, int )) strrchr;
    multibyte_strstr = (const char *(*)(const char *, const char *)) strstr;
    multibyte_strtok = (char *(*)(char *, const char *)) strtok;
    _skip_multibyte_char = skip_non_multibyte_char;
    is_multibyte_char_1 = not_multibyte_char_1;
    global_is_multibyte_codepage = False;
    break; 
  }
}

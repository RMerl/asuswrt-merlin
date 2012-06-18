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
     and add all jis codes sequence at 1995.8.16
     Notes: Hexadecimal code by <ohki@gssm.otuka.tsukuba.ac.jp>
     and add upper/lower case conversion 1997.8.21
*/
#ifndef _KANJI_H_
#define _KANJI_H_

/* FOR SHIFT JIS CODE */
#define is_shift_jis(c) \
    ((0x81 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0x9f) \
     || (0xe0 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xef))
#define is_shift_jis2(c) \
    (0x40 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xfc \
    && ((unsigned char) (c)) != 0x7f)
#define is_kana(c) ((0xa0 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xdf))

/* case conversion */
#define is_sj_upper2(c) \
  ((0x60 <= (unsigned char) (c)) && ((unsigned char) (c) <= 0x79))
#define is_sj_lower2(c) \
  ((0x81 <= (unsigned char) (c)) && ((unsigned char) (c) <= 0x9A))
#define sjis_alph 0x82
#define is_sj_alph(c) (sjis_alph == (unsigned char) (c))
#define is_sj_upper(c1, c2) (is_sj_alph (c1) && is_sj_upper2 (c2))
#define is_sj_lower(c1, c2) (is_sj_alph (c1) && is_sj_lower2 (c2))
#define sj_toupper2(c) \
    (is_sj_lower2 (c) ? ((int) ((unsigned char) (c) - 0x81 + 0x60)) : \
     ((int) (unsigned char) (c)))
#define sj_tolower2(c) \
    (is_sj_upper2 (c) ? ((int) ((unsigned char) (c) - 0x60 + 0x81)) : \
     ((int) (unsigned char) (c)))

#ifdef _KANJI_C_
/* FOR EUC CODE */
#define euc_kana (0x8e)
#define is_euc_kana(c) (((unsigned char) (c)) == euc_kana)
#define is_euc(c)  (0xa0 < ((unsigned char) (c)) && ((unsigned char) (c)) < 0xff)

/* FOR JIS CODE */
/* default jis third shift code, use for output */
#ifndef JIS_KSO
#define JIS_KSO 'B'
#endif
#ifndef JIS_KSI
#define JIS_KSI 'J'
#endif
/* in: \E$B or \E$@ */
/* out: \E(J or \E(B or \E(H */
#define jis_esc (0x1b)
#define jis_so (0x0e)
#define jis_so1 ('$')
#define jis_so2 ('B')
#define jis_si (0x0f)
#define jis_si1 ('(')
#define jis_si2 ('J')
#define is_esc(c) (((unsigned char) (c)) == jis_esc)
#define is_so1(c) (((unsigned char) (c)) == jis_so1)
#define is_so2(c) (((unsigned char) (c)) == jis_so2 || ((unsigned char) (c)) == '@')
#define is_si1(c) (((unsigned char) (c)) == jis_si1)
#define is_si2(c) (((unsigned char) (c)) == jis_si2 || ((unsigned char) (c)) == 'B' \
    || ((unsigned char) (c)) == 'H')
#define is_so(c) (((unsigned char) (c)) == jis_so)
#define is_si(c) (((unsigned char) (c)) == jis_si)
#define junet_kana1 ('(')
#define junet_kana2 ('I')
#define is_juk1(c) (((unsigned char) (c)) == junet_kana1)
#define is_juk2(c) (((unsigned char) (c)) == junet_kana2)

#define _KJ_ROMAN (0)
#define _KJ_KANJI (1)
#define _KJ_KANA (2)

/* FOR HEX */
#define HEXTAG ':'
#define hex2bin(x)						      \
    ( ((int) '0' <= ((int) (x)) && ((int) (x)) <= (int)'9')?	      \
        (((int) (x))-(int)'0'):					      \
      ((int) 'a'<= ((int) (x)) && ((int) (x))<= (int) 'f')?	      \
        (((int) (x)) - (int)'a'+10):				      \
      (((int) (x)) - (int)'A'+10) )
#define bin2hex(x)						      \
    ( (((int) (x)) >= 10)? (((int) (x))-10 + (int) 'a'): (((int) (x)) + (int) '0') )

/* For Hangul (Korean - code page 949). */
#define is_hangul(c) ((0x81 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xfd))

/* For traditional Chinese (known as Big5 encoding - code page 950). */
#define is_big5_c1(c) ((0xa1 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xf9)) 

/* For simplified Chinese (code page - 936). */
#define is_simpch_c1(c) ((0xa1 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xf7))

#else /* not _KANJI_C_ */

/*
 * The following is needed for AIX systems that have
 * their own #defines for strchr, strrchr, strstr
 * and strtok.
 */

#ifdef strchr
#undef strchr
#endif /* strchr */

#ifdef strrchr
#undef strrchr
#endif /* strrchr */

#ifdef strstr
#undef strstr
#endif /* strstr */

#ifdef strtok
#undef strtok
#endif /* strtok */

/* Ensure we use our definitions in all other files than kanji.c. */

/* Function pointers we will replace. */
extern char *(*multibyte_strchr)(const char *s, int c);
extern char *(*multibyte_strrchr)(const char *s, int c);
extern char *(*multibyte_strstr)(const char *s1, const char *s2);
extern char *(*multibyte_strtok)(char *s1, const char *s2);
extern char *(*_dos_to_unix)(char *str, BOOL overwrite);
extern char *(*_unix_to_dos)(char *str, BOOL overwrite);
extern BOOL (*is_multibyte_char)(char c);
extern int (*_skip_multibyte_char)(char c);

#define strchr(s1, c) ((*multibyte_strchr)((s1), (c)))
#define strrchr(s1, c) ((*multibyte_strrchr)((s1), (c)))
#define strstr(s1, s2) ((*multibyte_strstr)((s1), (s2)))
#define strtok(s1, s2) ((*multibyte_strtok)((s1), (s2)))
#define dos_to_unix(x,y) ((*_dos_to_unix)((x), (y)))
#define unix_to_dos(x,y) ((*_unix_to_dos)((x), (y)))
#define skip_multibyte_char(c) ((*_skip_multibyte_char)((c)))

#endif /* _KANJI_C_ */

#define UNKNOWN_CODE (-1)
#define SJIS_CODE (0)
#define EUC_CODE (1)
#define JIS7_CODE (2)
#define JIS8_CODE (3)
#define JUNET_CODE (4)
#define HEX_CODE (5)
#define CAP_CODE (6)
#define DOSV_CODE SJIS_CODE
#define UTF8_CODE (8)

#endif /* _KANJI_H_ */

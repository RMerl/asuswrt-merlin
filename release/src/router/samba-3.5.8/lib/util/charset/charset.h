/* 
   Unix SMB/CIFS implementation.
   charset defines
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Jelmer Vernooij 2002
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This is a public header file that is installed as part of Samba. 
 * If you remove any functions or change their signature, update 
 * the so version number. */

#ifndef __CHARSET_H__
#define __CHARSET_H__

#include <talloc.h>

/* this defines the charset types used in samba */
typedef enum {CH_UTF16LE=0, CH_UTF16=0, CH_UNIX, CH_DISPLAY, CH_DOS, CH_UTF8, CH_UTF16BE, CH_UTF16MUNGED} charset_t;

#define NUM_CHARSETS 7

/*
 * SMB UCS2 (16-bit unicode) internal type.
 * smb_ucs2_t is *always* in little endian format.
 */

typedef uint16_t smb_ucs2_t;

/*
 *   for each charset we have a function that pulls from that charset to
 *     a ucs2 buffer, and a function that pushes to a ucs2 buffer
 *     */

struct charset_functions {
	const char *name;
	size_t (*pull)(void *, const char **inbuf, size_t *inbytesleft,
				   char **outbuf, size_t *outbytesleft);
	size_t (*push)(void *, const char **inbuf, size_t *inbytesleft,
				   char **outbuf, size_t *outbytesleft);
	struct charset_functions *prev, *next;
};

/* this type is used for manipulating unicode codepoints */
typedef uint32_t codepoint_t;

#define INVALID_CODEPOINT ((codepoint_t)-1)

/*
 * This is auxiliary struct used by source/script/gen-8-bit-gap.sh script
 * during generation of an encoding table for charset module
 *     */

struct charset_gap_table {
  uint16_t start;
  uint16_t end;
  int32_t idx;
};


/* generic iconv conversion structure */
typedef struct smb_iconv_s {
	size_t (*direct)(void *cd, const char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft);
	size_t (*pull)(void *cd, const char **inbuf, size_t *inbytesleft,
		       char **outbuf, size_t *outbytesleft);
	size_t (*push)(void *cd, const char **inbuf, size_t *inbytesleft,
		       char **outbuf, size_t *outbytesleft);
	void *cd_direct, *cd_pull, *cd_push;
	char *from_name, *to_name;
} *smb_iconv_t;

/* string manipulation flags */
#define STR_TERMINATE 1
#define STR_UPPER 2
#define STR_ASCII 4
#define STR_UNICODE 8
#define STR_NOALIGN 16
#define STR_NO_RANGE_CHECK 32
#define STR_LEN8BIT 64
#define STR_TERMINATE_ASCII 128 /* only terminate if ascii */
#define STR_LEN_NOTERM 256 /* the length field is the unterminated length */

struct loadparm_context;
struct smb_iconv_convenience;

/* replace some string functions with multi-byte
   versions */
#define strlower(s) strlower_m(s)
#define strupper(s) strupper_m(s)

char *strchr_m(const char *s, char c);
size_t strlen_m_term(const char *s);
size_t strlen_m_term_null(const char *s);
size_t strlen_m(const char *s);
char *alpha_strcpy(char *dest, const char *src, const char *other_safe_chars, size_t maxlength);
void string_replace_m(char *s, char oldc, char newc);
bool strcsequal_m(const char *s1,const char *s2);
bool strequal_m(const char *s1, const char *s2);
int strncasecmp_m(const char *s1, const char *s2, size_t n);
bool next_token(const char **ptr,char *buff, const char *sep, size_t bufsize);
int strcasecmp_m(const char *s1, const char *s2);
size_t count_chars_m(const char *s, char c);
void strupper_m(char *s);
void strlower_m(char *s);
char *strupper_talloc(TALLOC_CTX *ctx, const char *src);
char *talloc_strdup_upper(TALLOC_CTX *ctx, const char *src);
char *strupper_talloc_n(TALLOC_CTX *ctx, const char *src, size_t n);
char *strlower_talloc(TALLOC_CTX *ctx, const char *src);
bool strhasupper(const char *string);
bool strhaslower(const char *string);
char *strrchr_m(const char *s, char c);
char *strchr_m(const char *s, char c);

bool push_ascii_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size);
bool push_ucs2_talloc(TALLOC_CTX *ctx, smb_ucs2_t **dest, const char *src, size_t *converted_size);
bool push_utf8_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size);
bool pull_ascii_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size);
bool pull_ucs2_talloc(TALLOC_CTX *ctx, char **dest, const smb_ucs2_t *src, size_t *converted_size);
bool pull_utf8_talloc(TALLOC_CTX *ctx, char **dest, const char *src, size_t *converted_size);
ssize_t push_string(void *dest, const char *src, size_t dest_len, int flags);
ssize_t pull_string(char *dest, const void *src, size_t dest_len, size_t src_len, int flags);

bool convert_string_talloc(TALLOC_CTX *ctx, 
				       charset_t from, charset_t to, 
				       void const *src, size_t srclen, 
				       void *dest, size_t *converted_size, 
					   bool allow_badcharcnv);

size_t convert_string(charset_t from, charset_t to,
				void const *src, size_t srclen, 
				void *dest, size_t destlen, bool allow_badcharcnv);

ssize_t iconv_talloc(TALLOC_CTX *mem_ctx, 
				       smb_iconv_t cd,
				       void const *src, size_t srclen, 
				       void *dest);

extern struct smb_iconv_convenience *global_iconv_convenience;

codepoint_t next_codepoint(const char *str, size_t *size);
ssize_t push_codepoint(char *str, codepoint_t c);

/* codepoints */
codepoint_t next_codepoint_convenience(struct smb_iconv_convenience *ic, 
			    const char *str, size_t *size);
ssize_t push_codepoint_convenience(struct smb_iconv_convenience *ic, 
				char *str, codepoint_t c);
codepoint_t toupper_m(codepoint_t val);
codepoint_t tolower_m(codepoint_t val);
int codepoint_cmpi(codepoint_t c1, codepoint_t c2);

/* Iconv convenience functions */
struct smb_iconv_convenience *smb_iconv_convenience_init(TALLOC_CTX *mem_ctx,
							 const char *dos_charset,
							 const char *unix_charset,
							 bool native_iconv);

bool convert_string_convenience(struct smb_iconv_convenience *ic,
				charset_t from, charset_t to,
				void const *src, size_t srclen, 
				void *dest, size_t destlen, size_t *converted_size,
				bool allow_badcharcnv);
bool convert_string_talloc_convenience(TALLOC_CTX *ctx, 
				       struct smb_iconv_convenience *ic, 
				       charset_t from, charset_t to, 
				       void const *src, size_t srclen, 
				       void *dest, size_t *converted_size, bool allow_badcharcnv);
/* iconv */
smb_iconv_t smb_iconv_open(const char *tocode, const char *fromcode);
int smb_iconv_close(smb_iconv_t cd);
size_t smb_iconv(smb_iconv_t cd, 
		 const char **inbuf, size_t *inbytesleft,
		 char **outbuf, size_t *outbytesleft);
smb_iconv_t smb_iconv_open_ex(TALLOC_CTX *mem_ctx, const char *tocode, 
			      const char *fromcode, bool native_iconv);

void load_case_tables(void);
bool charset_register_backend(const void *_funcs);

/*
 *   Define stub for charset module which implements 8-bit encoding with gaps.
 *   Encoding tables for such module should be produced from glibc's CHARMAPs
 *   using script source/script/gen-8bit-gap.sh
 *   CHARSETNAME is CAPITALIZED charset name
 *
 *     */
#define SMB_GENERATE_CHARSET_MODULE_8_BIT_GAP(CHARSETNAME) 					\
static size_t CHARSETNAME ## _push(void *cd, const char **inbuf, size_t *inbytesleft,			\
			 char **outbuf, size_t *outbytesleft) 					\
{ 												\
	while (*inbytesleft >= 2 && *outbytesleft >= 1) { 					\
		int i; 										\
		int done = 0; 									\
												\
		uint16 ch = SVAL(*inbuf,0); 							\
												\
		for (i=0; from_idx[i].start != 0xffff; i++) {					\
			if ((from_idx[i].start <= ch) && (from_idx[i].end >= ch)) {		\
				((unsigned char*)(*outbuf))[0] = from_ucs2[from_idx[i].idx+ch];	\
				(*inbytesleft) -= 2;						\
				(*outbytesleft) -= 1;						\
				(*inbuf)  += 2;							\
				(*outbuf) += 1;							\
				done = 1;							\
				break;								\
			}									\
		}										\
		if (!done) {									\
			errno = EINVAL;								\
			return -1;								\
		}										\
												\
	}											\
												\
	if (*inbytesleft == 1) {								\
		errno = EINVAL;									\
		return -1;									\
	}											\
												\
	if (*inbytesleft > 1) {									\
		errno = E2BIG;									\
		return -1;									\
	}											\
												\
	return 0;										\
}												\
												\
static size_t CHARSETNAME ## _pull(void *cd, const char **inbuf, size_t *inbytesleft,				\
			 char **outbuf, size_t *outbytesleft)					\
{												\
	while (*inbytesleft >= 1 && *outbytesleft >= 2) {					\
		SSVAL(*outbuf, 0, to_ucs2[((unsigned char*)(*inbuf))[0]]);			\
		(*inbytesleft)  -= 1;								\
		(*outbytesleft) -= 2;								\
		(*inbuf)  += 1;									\
		(*outbuf) += 2;									\
	}											\
												\
	if (*inbytesleft > 0) {									\
		errno = E2BIG;									\
		return -1;									\
	}											\
												\
	return 0;										\
}												\
												\
struct charset_functions CHARSETNAME ## _functions = 						\
		{#CHARSETNAME, CHARSETNAME ## _pull, CHARSETNAME ## _push};			\
												\
NTSTATUS charset_ ## CHARSETNAME ## _init(void);							\
NTSTATUS charset_ ## CHARSETNAME ## _init(void)							\
{												\
	return smb_register_charset(& CHARSETNAME ## _functions);				\
}												\


#endif /* __CHARSET_H__ */

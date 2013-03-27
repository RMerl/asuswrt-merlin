/* 
   String utility functions
   Copyright (C) 1999-2007, 2009, Joe Orton <joe@manyfish.co.uk>
   strcasecmp/strncasecmp implementations are:
   Copyright (C) 1991, 1992, 1995, 1996, 1997 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>

#include "ne_alloc.h"
#include "ne_string.h"

char *ne_token(char **str, char separator)
{
    char *ret = *str, *pnt = strchr(*str, separator);

    if (pnt) {
	*pnt = '\0';
	*str = pnt + 1;
    } else {
	/* no separator found: return end of string. */
	*str = NULL;
    }
    
    return ret;
}

char *ne_qtoken(char **str, char separator, const char *quotes)
{
    char *pnt, *ret = NULL;

    for (pnt = *str; *pnt != '\0'; pnt++) {
	char *quot = strchr(quotes, *pnt);
	
	if (quot) {
	    char *qclose = strchr(pnt+1, *quot);
	    
	    if (!qclose) {
		/* no closing quote: invalid string. */
		return NULL;
	    }
	    
	    pnt = qclose;
	} else if (*pnt == separator) {
	    /* found end of token. */
	    *pnt = '\0';
	    ret = *str;
	    *str = pnt + 1;
	    return ret;
	}
    }

    /* no separator found: return end of string. */
    ret = *str;
    *str = NULL;
    return ret;
}

char *ne_shave(char *str, const char *whitespace)
{
    char *pnt, *ret = str;

    while (*ret != '\0' && strchr(whitespace, *ret) != NULL) {
	ret++;
    }

    /* pnt points at the NUL terminator. */
    pnt = &ret[strlen(ret)];
    
    while (pnt > ret && strchr(whitespace, *(pnt-1)) != NULL) {
	pnt--;
    }

    *pnt = '\0';
    return ret;
}

void ne_buffer_clear(ne_buffer *buf) 
{
    memset(buf->data, 0, buf->length);
    buf->used = 1;
}  

/* Grows for given size, returns 0 on success, -1 on error. */
void ne_buffer_grow(ne_buffer *buf, size_t newsize) 
{
#define NE_BUFFER_GROWTH 512
    if (newsize > buf->length) {
	/* If it's not big enough already... */
	buf->length = ((newsize / NE_BUFFER_GROWTH) + 1) * NE_BUFFER_GROWTH;
	
	/* Reallocate bigger buffer */
	buf->data = ne_realloc(buf->data, buf->length);
    }
}

static size_t count_concat(va_list *ap)
{
    size_t total = 0;
    char *next;

    while ((next = va_arg(*ap, char *)) != NULL)
	total += strlen(next);

    return total;
}

static void do_concat(char *str, va_list *ap) 
{
    char *next;

    while ((next = va_arg(*ap, char *)) != NULL) {
#ifdef HAVE_STPCPY
        str = stpcpy(str, next);
#else
	size_t len = strlen(next);
	memcpy(str, next, len);
	str += len;
#endif
    }
}

void ne_buffer_concat(ne_buffer *buf, ...)
{
    va_list ap;
    ssize_t total;

    va_start(ap, buf);
    total = buf->used + count_concat(&ap);
    va_end(ap);    

    /* Grow the buffer */
    ne_buffer_grow(buf, total);
    
    va_start(ap, buf);    
    do_concat(buf->data + buf->used - 1, &ap);
    va_end(ap);    

    buf->used = total;
    buf->data[total - 1] = '\0';
}

char *ne_concat(const char *str, ...)
{
    va_list ap;
    size_t total, slen = strlen(str);
    char *ret;

    va_start(ap, str);
    total = slen + count_concat(&ap);
    va_end(ap);

    ret = memcpy(ne_malloc(total + 1), str, slen);

    va_start(ap, str);
    do_concat(ret + slen, &ap);
    va_end(ap);

    ret[total] = '\0';
    return ret;    
}

/* Append zero-terminated string... returns 0 on success or -1 on
 * realloc failure. */
void ne_buffer_zappend(ne_buffer *buf, const char *str) 
{
    ne_buffer_append(buf, str, strlen(str));
}

void ne_buffer_append(ne_buffer *buf, const char *data, size_t len) 
{
    ne_buffer_grow(buf, buf->used + len);
    memcpy(buf->data + buf->used - 1, data, len);
    buf->used += len;
    buf->data[buf->used - 1] = '\0';
}

size_t ne_buffer_snprintf(ne_buffer *buf, size_t max, const char *fmt, ...)
{
    va_list ap;
    size_t ret;

    ne_buffer_grow(buf, buf->used + max);

    va_start(ap, fmt);
    ret = ne_vsnprintf(buf->data + buf->used - 1, max, fmt, ap);
    va_end(ap);
    buf->used += ret;

    return ret;    
}

ne_buffer *ne_buffer_create(void) 
{
    return ne_buffer_ncreate(512);
}

ne_buffer *ne_buffer_ncreate(size_t s) 
{
    ne_buffer *buf = ne_malloc(sizeof(*buf));
    buf->data = ne_malloc(s);
    buf->data[0] = '\0';
    buf->length = s;
    buf->used = 1;
    return buf;
}

void ne_buffer_destroy(ne_buffer *buf) 
{
    ne_free(buf->data);
    ne_free(buf);
}

char *ne_buffer_finish(ne_buffer *buf)
{
    char *ret = buf->data;
    ne_free(buf);
    return ret;
}

void ne_buffer_altered(ne_buffer *buf)
{
    buf->used = strlen(buf->data) + 1;
}


/* ascii_quote[n] gives the number of bytes needed by
 * ne_buffer_qappend() to append character 'n'. */
static const unsigned char ascii_quote[256] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

static const char hex_chars[16] = "0123456789ABCDEF";

/* Return the expected number of bytes needed to append the string
 * beginning at byte 's', where 'send' points to the last byte after
 * 's'. */ 
static size_t qappend_count(const unsigned char *s, const unsigned char *send)
{
    const unsigned char *p;
    size_t ret;
    
    for (p = s, ret = 0; p < send; p++) {
        ret += ascii_quote[*p];
    }

    return ret;
}       

/* Append the string 's', up to but not including 'send', to string
 * 'dest', quoting along the way.  Returns pointer to NUL. */
static char *quoted_append(char *dest, const unsigned char *s, 
                           const unsigned char *send)
{
    const unsigned char *p;
    char *q = dest;

    for (p = s; p < send; p++) {
        if (ascii_quote[*p] == 1) {
            *q++ = *p;
        }
        else {
            *q++ = '\\';
            *q++ = 'x';
            *q++ = hex_chars[(*p >> 4) & 0x0f];
            *q++ = hex_chars[*p & 0x0f];
        }
    }

    /* NUL terminate after the last character */
    *q = '\0';
    
    return q;
}

void ne_buffer_qappend(ne_buffer *buf, const unsigned char *data, size_t len)
{
    const unsigned char *dend = data + len;
    char *q, *qs;

    ne_buffer_grow(buf, buf->used + qappend_count(data, dend));

    /* buf->used >= 1, so this is safe. */
    qs = buf->data + buf->used - 1;

    q = quoted_append(qs, data, dend);
    
    /* used already accounts for a NUL, so increment by number of
     * characters appended, *before* the NUL. */
    buf->used += q - qs;
}

char *ne_strnqdup(const unsigned char *data, size_t len)
{
    const unsigned char *dend = data + len;
    char *dest = malloc(qappend_count(data, dend) + 1);

    quoted_append(dest, data, dend);

    return dest;
}

static const char b64_alphabet[] =  
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/=";
    
char *ne_base64(const unsigned char *text, size_t inlen)
{
    /* The tricky thing about this is doing the padding at the end,
     * doing the bit manipulation requires a bit of concentration only */
    char *buffer, *point;
    size_t outlen;
    
    /* Use 'buffer' to store the output. Work out how big it should be...
     * This must be a multiple of 4 bytes */

    outlen = (inlen*4)/3;
    if ((inlen % 3) > 0) /* got to pad */
	outlen += 4 - (inlen % 3);
    
    buffer = ne_malloc(outlen + 1); /* +1 for the \0 */
    
    /* now do the main stage of conversion, 3 bytes at a time,
     * leave the trailing bytes (if there are any) for later */

    for (point=buffer; inlen>=3; inlen-=3, text+=3) {
	*(point++) = b64_alphabet[ (*text)>>2 ]; 
	*(point++) = b64_alphabet[ ((*text)<<4 & 0x30) | (*(text+1))>>4 ]; 
	*(point++) = b64_alphabet[ ((*(text+1))<<2 & 0x3c) | (*(text+2))>>6 ];
	*(point++) = b64_alphabet[ (*(text+2)) & 0x3f ];
    }

    /* Now deal with the trailing bytes */
    if (inlen > 0) {
	/* We always have one trailing byte */
	*(point++) = b64_alphabet[ (*text)>>2 ];
	*(point++) = b64_alphabet[ (((*text)<<4 & 0x30) |
				     (inlen==2?(*(text+1))>>4:0)) ]; 
	*(point++) = (inlen==1?'=':b64_alphabet[ (*(text+1))<<2 & 0x3c ]);
	*(point++) = '=';
    }

    /* Null-terminate */
    *point = '\0';

    return buffer;
}

/* VALID_B64: fail if 'ch' is not a valid base64 character */
#define VALID_B64(ch) (((ch) >= 'A' && (ch) <= 'Z') || \
                       ((ch) >= 'a' && (ch) <= 'z') || \
                       ((ch) >= '0' && (ch) <= '9') || \
                       (ch) == '/' || (ch) == '+' || (ch) == '=')

/* DECODE_B64: decodes a valid base64 character. */
#define DECODE_B64(ch) ((ch) >= 'a' ? ((ch) + 26 - 'a') : \
                        ((ch) >= 'A' ? ((ch) - 'A') : \
                         ((ch) >= '0' ? ((ch) + 52 - '0') : \
                          ((ch) == '+' ? 62 : 63))))

size_t ne_unbase64(const char *data, unsigned char **out)
{
    size_t inlen = strlen(data);
    unsigned char *outp;
    const unsigned char *in;

    if (inlen == 0 || (inlen % 4) != 0) return 0;
    
    outp = *out = ne_malloc(inlen * 3 / 4);

    for (in = (const unsigned char *)data; *in; in += 4) {
        unsigned int tmp;
        if (!VALID_B64(in[0]) || !VALID_B64(in[1]) || !VALID_B64(in[2]) ||
            !VALID_B64(in[3]) || in[0] == '=' || in[1] == '=' ||
            (in[2] == '=' && in[3] != '=')) {
            ne_free(*out);
            return 0;
        }
        tmp = (DECODE_B64(in[0]) & 0x3f) << 18 |
            (DECODE_B64(in[1]) & 0x3f) << 12;
        *outp++ = (tmp >> 16) & 0xff;
        if (in[2] != '=') {
            tmp |= (DECODE_B64(in[2]) & 0x3f) << 6;
            *outp++ = (tmp >> 8) & 0xff;
            if (in[3] != '=') {
                tmp |= DECODE_B64(in[3]) & 0x3f;
                *outp++ = tmp & 0xff;
            }
        }
    }

    return outp - *out;
}

/* Character map array; ascii_clean[n] = isprint(n) ? n : 0x20.  Used
 * by ne_strclean as a locale-independent isprint(). */
static const unsigned char ascii_clean[256] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

char *ne_strclean(char *str)
{
    unsigned char *pnt;

    for (pnt = (unsigned char *)str; *pnt; pnt++)
        *pnt = (char)ascii_clean[*pnt];

    return str;
}

char *ne_strerror(int errnum, char *buf, size_t buflen)
{
#ifdef HAVE_STRERROR_R
#ifdef STRERROR_R_CHAR_P
    /* glibc-style strerror_r which may-or-may-not use provided buffer. */
    char *ret = strerror_r(errnum, buf, buflen);
    if (ret != buf)
	ne_strnzcpy(buf, ret, buflen);
#else /* POSIX-style strerror_r: */
    char tmp[256];

    if (strerror_r(errnum, tmp, sizeof tmp) == 0)
        ne_strnzcpy(buf, tmp, buflen);
    else
        ne_snprintf(buf, buflen, "Unknown error %d", errnum);
#endif
#else /* no strerror_r: */
    ne_strnzcpy(buf, strerror(errnum), buflen);
#endif
    return buf;
}


/* Wrapper for ne_snprintf. */
size_t ne_snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef HAVE_TRIO
    trio_vsnprintf(str, size, fmt, ap);
#else
    vsnprintf(str, size, fmt, ap);
#endif
    va_end(ap);
    str[size-1] = '\0';
    return strlen(str);
}

/* Wrapper for ne_vsnprintf. */
size_t ne_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
#ifdef HAVE_TRIO
    trio_vsnprintf(str, size, fmt, ap);
#else
    vsnprintf(str, size, fmt, ap);
#endif
    str[size-1] = '\0';
    return strlen(str);
}

/* Locale-independent strcasecmp implementations. */
static const unsigned char ascii_tolower[256] = {
0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};    

#define TOLOWER(ch) ascii_tolower[ch]

const unsigned char *ne_tolower_array(void)
{
    return ascii_tolower;
}

int ne_strcasecmp(const char *s1, const char *s2)
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    unsigned char c1, c2;

    if (p1 == p2)
        return 0;
    
    do {
        c1 = TOLOWER(*p1++);
        c2 = TOLOWER(*p2++);
        if (c1 == '\0')
            break;
    } while (c1 == c2);
    
    return c1 - c2;
}

int ne_strncasecmp(const char *s1, const char *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    unsigned char c1, c2;
    
    if (p1 == p2 || n == 0)
        return 0;
    
    do {
        c1 = TOLOWER(*p1++);
        c2 = TOLOWER(*p2++);
        if (c1 == '\0' || c1 != c2)
            return c1 - c2;
    } while (--n > 0);
    
    return c1 - c2;
}

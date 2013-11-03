/* 
   Unix SMB/CIFS implementation.
   minimal iconv implementation
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Jelmer Vernooij 2002,2003
   
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
   
   From samba 3.0 beta and GNU libiconv-1.8
   It's bad but most of the time we can't use libc iconv service:
   - it doesn't round trip for most encoding
   - it doesn't know about Apple extension
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef HAVE_USABLE_ICONV
#include <iconv.h>
#endif
#include <arpa/inet.h>

#include <atalk/unicode.h>
#include <atalk/logger.h>
#include "byteorder.h"


/**
 * @file
 *
 * @brief Samba wrapper/stub for iconv character set conversion.
 *
 * iconv is the XPG2 interface for converting between character
 * encodings.  This file provides a Samba wrapper around it, and also
 * a simple reimplementation that is used if the system does not
 * implement iconv.
 *
 * Samba only works with encodings that are supersets of ASCII: ascii
 * characters like whitespace can be tested for directly, multibyte
 * sequences start with a byte with the high bit set, and strings are
 * terminated by a nul byte.
 *
 * Note that the only function provided by iconv is conversion between
 * characters.  It doesn't directly support operations like
 * uppercasing or comparison.  We have to convert to UCS-2 and compare
 * there.
 *
 * @sa Samba Developers Guide
 **/
#define CHARSET_WIDECHAR    32

#ifdef HAVE_USABLE_ICONV
#ifdef HAVE_UCS2INTERNAL
#define UCS2ICONV "UCS-2-INTERNAL"
#else /* !HAVE_UCS2INTERNAL */
#if BYTE_ORDER==LITTLE_ENDIAN
#define UCS2ICONV "UCS-2LE"
#else /* !LITTLE_ENDIAN */
#define UCS2ICONV "UCS-2BE"
#endif /* BYTE_ORDER */
#endif /* HAVE_UCS2INTERNAL */
#else /* !HAVE_USABLE_ICONV */
#define UCS2ICONV "UCS-2"
#endif /* HAVE_USABLE_ICONV */

static size_t ascii_pull(void *,char **, size_t *, char **, size_t *);
static size_t ascii_push(void *,char **, size_t *, char **, size_t *);
static size_t iconv_copy(void *,char **, size_t *, char **, size_t *);

extern  struct charset_functions charset_mac_roman;
extern  struct charset_functions charset_mac_hebrew;
extern  struct charset_functions charset_mac_centraleurope;
extern  struct charset_functions charset_mac_cyrillic;
extern  struct charset_functions charset_mac_greek;
extern  struct charset_functions charset_mac_turkish;
extern  struct charset_functions charset_utf8;
extern  struct charset_functions charset_utf8_mac;
#ifdef HAVE_USABLE_ICONV
extern  struct charset_functions charset_mac_japanese;
extern  struct charset_functions charset_mac_chinese_trad;
extern  struct charset_functions charset_mac_korean;
extern  struct charset_functions charset_mac_chinese_simp;
#endif


static struct charset_functions builtin_functions[] = {
	{"UCS-2",   0, iconv_copy, iconv_copy, CHARSET_WIDECHAR | CHARSET_PRECOMPOSED, NULL, NULL, NULL},
	{"ASCII",     0, ascii_pull, ascii_push, CHARSET_MULTIBYTE | CHARSET_PRECOMPOSED, NULL, NULL, NULL},
	{NULL, 0, NULL, NULL, 0, NULL, NULL, NULL}
};


#define DLIST_ADD(list, p) \
{ \
        if (!(list)) { \
                (list) = (p); \
                (p)->next = (p)->prev = NULL; \
        } else { \
                (list)->prev = (p); \
                (p)->next = (list); \
                (p)->prev = NULL; \
                (list) = (p); \
        }\
}

static struct charset_functions *charsets = NULL;

struct charset_functions *find_charset_functions(const char *name) 
{
	struct charset_functions *c = charsets;

	while(c) {
		if (strcasecmp(name, c->name) == 0) {
			return c;
		}
		c = c->next;
	}

	return NULL;
}

int atalk_register_charset(struct charset_functions *funcs) 
{
	if (!funcs) {
		return -1;
	}

	/* Check whether we already have this charset... */
	if (find_charset_functions(funcs->name)) {
		LOG (log_debug, logtype_default, "Duplicate charset %s, not registering", funcs->name);
		return -2;
	}

	funcs->next = funcs->prev = NULL;
	DLIST_ADD(charsets, funcs);
	return 0;
}

static void lazy_initialize_iconv(void)
{
	static int initialized = 0;
	int i;

	if (!initialized) {
		initialized = 1;
		for(i = 0; builtin_functions[i].name; i++) 
			atalk_register_charset(&builtin_functions[i]);

		/* register additional charsets */
		atalk_register_charset(&charset_utf8);
		atalk_register_charset(&charset_utf8_mac);
		atalk_register_charset(&charset_mac_roman);
		atalk_register_charset(&charset_mac_hebrew);
		atalk_register_charset(&charset_mac_greek);
		atalk_register_charset(&charset_mac_turkish);
		atalk_register_charset(&charset_mac_centraleurope);
		atalk_register_charset(&charset_mac_cyrillic);
#ifdef HAVE_USABLE_ICONV
		atalk_register_charset(&charset_mac_japanese);
		atalk_register_charset(&charset_mac_chinese_trad);
		atalk_register_charset(&charset_mac_korean);
		atalk_register_charset(&charset_mac_chinese_simp);
#endif
	}
}

/* if there was an error then reset the internal state,
   this ensures that we don't have a shift state remaining for
   character sets like SJIS */
static size_t sys_iconv(void *cd, 
			char **inbuf, size_t *inbytesleft,
			char **outbuf, size_t *outbytesleft)
{
#ifdef HAVE_USABLE_ICONV
	size_t ret = iconv((iconv_t)cd, 
			   (ICONV_CONST char**)inbuf, inbytesleft, 
			   outbuf, outbytesleft);
	if (ret == (size_t)-1) iconv(cd, NULL, NULL, NULL, NULL);
	return ret;
#else
	errno = EINVAL;
	return -1;
#endif
}

/**
 * This is a simple portable iconv() implementaion.
 *
 * It only knows about a very small number of character sets - just
 * enough that netatalk works on systems that don't have iconv.
 **/
size_t atalk_iconv(atalk_iconv_t cd, 
		 const char **inbuf, size_t *inbytesleft,
		 char **outbuf, size_t *outbytesleft)
{
	char cvtbuf[2048];
	char *bufp = cvtbuf;
	size_t bufsize;

	/* in many cases we can go direct */
	if (cd->direct) {
		return cd->direct(cd->cd_direct, 
				  (char **)inbuf, inbytesleft, outbuf, outbytesleft);
	}


	/* otherwise we have to do it chunks at a time */
	while (*inbytesleft > 0) {
		bufp = cvtbuf;
		bufsize = sizeof(cvtbuf);
		
		if (cd->pull(cd->cd_pull, (char **)inbuf, inbytesleft, &bufp, &bufsize) == (size_t)-1
		       && errno != E2BIG) {
		    return -1;
		}

		bufp = cvtbuf;
		bufsize = sizeof(cvtbuf) - bufsize;

		if (cd->push(cd->cd_push, &bufp, &bufsize, outbuf, outbytesleft) == (size_t)-1) {
		    return -1;
		}
	}

	return 0;
}


/*
  simple iconv_open() wrapper
 */
atalk_iconv_t atalk_iconv_open(const char *tocode, const char *fromcode)
{
	atalk_iconv_t ret;
	struct charset_functions *from, *to;


	lazy_initialize_iconv();
	from = charsets;
	to = charsets;

	ret = (atalk_iconv_t)malloc(sizeof(*ret));
	if (!ret) {
		errno = ENOMEM;
		return (atalk_iconv_t)-1;
	}
	memset(ret, 0, sizeof(*ret));

	ret->from_name = strdup(fromcode);
	ret->to_name = strdup(tocode);

	/* check for the simplest null conversion */
	if (strcasecmp(fromcode, tocode) == 0) {
		ret->direct = iconv_copy;
		return ret;
	}

	/* check if we have a builtin function for this conversion */
	from = find_charset_functions(fromcode);
	if (from) ret->pull = from->pull;
	
	to = find_charset_functions(tocode);
	if (to) ret->push = to->push;

	/* check if we can use iconv for this conversion */
#ifdef HAVE_USABLE_ICONV
	if (!from || (from->flags & CHARSET_ICONV)) {
	  ret->cd_pull = iconv_open(UCS2ICONV, from && from->iname ? from->iname : fromcode);
	  if (ret->cd_pull != (iconv_t)-1) {
	    if (!ret->pull) ret->pull = sys_iconv;
	  } else ret->pull = NULL;
	}
	if (ret->pull) {
	  if (!to || (to->flags & CHARSET_ICONV)) {
	    ret->cd_push = iconv_open(to && to->iname ? to->iname : tocode, UCS2ICONV);
	    if (ret->cd_push != (iconv_t)-1) {
	      if (!ret->push) ret->push = sys_iconv;
	    } else ret->push = NULL;
	  }
	  if (!ret->push && ret->cd_pull) iconv_close((iconv_t)ret->cd_pull);
	}
#endif
	
	if (!ret->push || !ret->pull) {
		SAFE_FREE(ret->from_name);
		SAFE_FREE(ret->to_name);
		SAFE_FREE(ret);
		errno = EINVAL;
		return (atalk_iconv_t)-1;
	}

	/* check for conversion to/from ucs2 */
	if (strcasecmp(fromcode, "UCS-2") == 0) {
	  ret->direct = ret->push;
	  ret->cd_direct = ret->cd_push;
	  ret->cd_push = NULL;
	}
	if (strcasecmp(tocode, "UCS-2") == 0) {
	  ret->direct = ret->pull;
	  ret->cd_direct = ret->cd_pull;
	  ret->cd_pull = NULL;
	}

	return ret;
}

/*
  simple iconv_close() wrapper
*/
int atalk_iconv_close (atalk_iconv_t cd)
{
#ifdef HAVE_USABLE_ICONV
	if (cd->cd_direct) iconv_close((iconv_t)cd->cd_direct);
	if (cd->cd_pull) iconv_close((iconv_t)cd->cd_pull);
	if (cd->cd_push) iconv_close((iconv_t)cd->cd_push);
#endif

	SAFE_FREE(cd->from_name);
	SAFE_FREE(cd->to_name);

	memset(cd, 0, sizeof(*cd));
	SAFE_FREE(cd);
	return 0;
}


/************************************************************************
 the following functions implement the builtin character sets in Netatalk
*************************************************************************/

static size_t ascii_pull(void *cd _U_, char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	ucs2_t curchar;

	while (*inbytesleft >= 1 && *outbytesleft >= 2) {
		if ((unsigned char)(*inbuf)[0] < 0x80) {
			curchar = (ucs2_t) (*inbuf)[0];
			SSVAL((*outbuf),0,curchar);
		}
		else {
			errno = EILSEQ;
			return -1;
		}
		(*inbytesleft)  -= 1;
		(*outbytesleft) -= 2;
		(*inbuf)  += 1;
		(*outbuf) += 2;
	}

	if (*inbytesleft > 0) {
		errno = E2BIG;
		return -1;
	}
	
	return 0;
}

static size_t ascii_push(void *cd _U_, char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	int ir_count=0;
	ucs2_t curchar;

	while (*inbytesleft >= 2 && *outbytesleft >= 1) {
		curchar = SVAL((*inbuf), 0);
		if (curchar < 0x0080) {
			(*outbuf)[0] = curchar;
		}
		else {
			errno = EILSEQ;
			return -1;
		}	
		(*inbytesleft)  -= 2;
		(*outbytesleft) -= 1;
		(*inbuf)  += 2;
		(*outbuf) += 1;
	}

	if (*inbytesleft == 1) {
		errno = EINVAL;
		return -1;
	}

	if (*inbytesleft > 1) {
		errno = E2BIG;
		return -1;
	}
	
	return ir_count;
}


static size_t iconv_copy(void *cd _U_, char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	int n;

	n = MIN(*inbytesleft, *outbytesleft);

	memmove(*outbuf, *inbuf, n);

	(*inbytesleft) -= n;
	(*outbytesleft) -= n;
	(*inbuf) += n;
	(*outbuf) += n;

	if (*inbytesleft > 0) {
		errno = E2BIG;
		return -1;
	}

	return 0;
}

/* ------------------------ */

/* 
   Unix SMB/CIFS implementation.
   minimal iconv implementation
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

#include "includes.h"
#include "../lib/util/dlinklist.h"
#include "system/iconv.h"
#include "system/filesys.h"

#ifdef strcasecmp
#undef strcasecmp
#endif

#ifdef static_decl_charset
static_decl_charset;
#endif

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
 * uppercasing or comparison.  We have to convert to UTF-16LE and
 * compare there.
 *
 * @sa Samba Developers Guide
 **/

static size_t ascii_pull  (void *,const char **, size_t *, char **, size_t *);
static size_t ascii_push  (void *,const char **, size_t *, char **, size_t *);
static size_t latin1_push(void *,const char **, size_t *, char **, size_t *);
static size_t utf8_pull   (void *,const char **, size_t *, char **, size_t *);
static size_t utf8_push   (void *,const char **, size_t *, char **, size_t *);
static size_t utf16_munged_pull(void *,const char **, size_t *, char **, size_t *);
static size_t ucs2hex_pull(void *,const char **, size_t *, char **, size_t *);
static size_t ucs2hex_push(void *,const char **, size_t *, char **, size_t *);
static size_t iconv_copy  (void *,const char **, size_t *, char **, size_t *);
static size_t iconv_swab  (void *,const char **, size_t *, char **, size_t *);

static const struct charset_functions builtin_functions[] = {
	/* windows is closest to UTF-16 */
	{"UCS-2LE",  iconv_copy, iconv_copy},
	{"UTF-16LE",  iconv_copy, iconv_copy},
	{"UCS-2BE",  iconv_swab, iconv_swab},
	{"UTF-16BE",  iconv_swab, iconv_swab},

	/* we include the UTF-8 alias to cope with differing locale settings */
	{"UTF8",   utf8_pull,  utf8_push},
	{"UTF-8",   utf8_pull,  utf8_push},

	/* this handles the munging needed for String2Key */
	{"UTF16_MUNGED",   utf16_munged_pull,  iconv_copy},

	{"ASCII", ascii_pull, ascii_push},
	{"646", ascii_pull, ascii_push},
	{"ISO-8859-1", ascii_pull, latin1_push},
	{"UCS2-HEX", ucs2hex_pull, ucs2hex_push}
};

static struct charset_functions *charsets = NULL;

static struct charset_functions *find_charset_functions(const char *name)
{
	struct charset_functions *c;

	/* Check whether we already have this charset... */
	for (c = charsets; c != NULL; c = c->next) {
		if(strcasecmp(c->name, name) == 0) { 
			return c;
		}
	}

	return NULL;
}

bool smb_register_charset(const struct charset_functions *funcs_in)
{
	struct charset_functions *funcs;

	DEBUG(5, ("Attempting to register new charset %s\n", funcs_in->name));
	/* Check whether we already have this charset... */
	if (find_charset_functions(funcs_in->name)) {
		DEBUG(0, ("Duplicate charset %s, not registering\n", funcs_in->name));
		return false;
	}

	funcs = talloc(NULL, struct charset_functions);
	if (!funcs) {
		DEBUG(0, ("Out of memory duplicating charset %s\n", funcs_in->name));
		return false;
	}
	*funcs = *funcs_in;

	funcs->next = funcs->prev = NULL;
	DEBUG(5, ("Registered charset %s\n", funcs->name));
	DLIST_ADD(charsets, funcs);
	return true;
}

static void lazy_initialize_iconv(void)
{
	static bool initialized;

#ifdef static_init_charset
	if (!initialized) {
		static_init_charset;
		initialized = true;
	}
#endif
}

#ifdef HAVE_NATIVE_ICONV
/* if there was an error then reset the internal state,
   this ensures that we don't have a shift state remaining for
   character sets like SJIS */
static size_t sys_iconv(void *cd, 
			const char **inbuf, size_t *inbytesleft,
			char **outbuf, size_t *outbytesleft)
{
	size_t ret = iconv((iconv_t)cd, 
			   discard_const_p(char *, inbuf), inbytesleft, 
			   outbuf, outbytesleft);
	if (ret == (size_t)-1) iconv(cd, NULL, NULL, NULL, NULL);
	return ret;
}
#endif

/**
 * This is a simple portable iconv() implementaion.
 *
 * It only knows about a very small number of character sets - just
 * enough that Samba works on systems that don't have iconv.
 **/
_PUBLIC_ size_t smb_iconv(smb_iconv_t cd, 
		 const char **inbuf, size_t *inbytesleft,
		 char **outbuf, size_t *outbytesleft)
{
	char cvtbuf[2048];
	size_t bufsize;

	/* in many cases we can go direct */
	if (cd->direct) {
		return cd->direct(cd->cd_direct, 
				  inbuf, inbytesleft, outbuf, outbytesleft);
	}


	/* otherwise we have to do it chunks at a time */
	while (*inbytesleft > 0) {
		char *bufp1 = cvtbuf;
		const char *bufp2 = cvtbuf;

		bufsize = sizeof(cvtbuf);
		
		if (cd->pull(cd->cd_pull, 
			     inbuf, inbytesleft, &bufp1, &bufsize) == -1
		    && errno != E2BIG) return -1;

		bufsize = sizeof(cvtbuf) - bufsize;

		if (cd->push(cd->cd_push, 
			     &bufp2, &bufsize, 
			     outbuf, outbytesleft) == -1) return -1;
	}

	return 0;
}

static bool is_utf16(const char *name)
{
	return strcasecmp(name, "UCS-2LE") == 0 ||
		strcasecmp(name, "UTF-16LE") == 0;
}

static int smb_iconv_t_destructor(smb_iconv_t hwd)
{
#ifdef HAVE_NATIVE_ICONV
	if (hwd->cd_pull != NULL && hwd->cd_pull != (iconv_t)-1)
		iconv_close(hwd->cd_pull);
	if (hwd->cd_push != NULL && hwd->cd_push != (iconv_t)-1)
		iconv_close(hwd->cd_push);
	if (hwd->cd_direct != NULL && hwd->cd_direct != (iconv_t)-1)
		iconv_close(hwd->cd_direct);
#endif

	return 0;
}

_PUBLIC_ smb_iconv_t smb_iconv_open_ex(TALLOC_CTX *mem_ctx, const char *tocode, 
			      const char *fromcode, bool native_iconv)
{
	smb_iconv_t ret;
	const struct charset_functions *from=NULL, *to=NULL;
	int i;

	lazy_initialize_iconv();

	ret = (smb_iconv_t)talloc_named(mem_ctx,
					sizeof(*ret), 
					"iconv(%s,%s)", tocode, fromcode);
	if (!ret) {
		errno = ENOMEM;
		return (smb_iconv_t)-1;
	}
	memset(ret, 0, sizeof(*ret));
	talloc_set_destructor(ret, smb_iconv_t_destructor);

	/* check for the simplest null conversion */
	if (strcmp(fromcode, tocode) == 0) {
		ret->direct = iconv_copy;
		return ret;
	}

	for (i=0;i<ARRAY_SIZE(builtin_functions);i++) {
		if (strcasecmp(fromcode, builtin_functions[i].name) == 0) {
			from = &builtin_functions[i];
		}
		if (strcasecmp(tocode, builtin_functions[i].name) == 0) {
			to = &builtin_functions[i];
		}
	}

	if (from == NULL) {
		for (from=charsets; from; from=from->next) {
			if (strcasecmp(from->name, fromcode) == 0) break;
		}
	}

	if (to == NULL) {
		for (to=charsets; to; to=to->next) {
			if (strcasecmp(to->name, tocode) == 0) break;
		}
	}

#ifdef HAVE_NATIVE_ICONV
	if ((!from || !to) && !native_iconv) {
		goto failed;
	}
	if (!from) {
		ret->pull = sys_iconv;
		ret->cd_pull = iconv_open("UTF-16LE", fromcode);
		if (ret->cd_pull == (iconv_t)-1)
			ret->cd_pull = iconv_open("UCS-2LE", fromcode);
		if (ret->cd_pull == (iconv_t)-1) goto failed;
	}

	if (!to) {
		ret->push = sys_iconv;
		ret->cd_push = iconv_open(tocode, "UTF-16LE");
		if (ret->cd_push == (iconv_t)-1)
			ret->cd_push = iconv_open(tocode, "UCS-2LE");
		if (ret->cd_push == (iconv_t)-1) goto failed;
	}
#else
	if (!from || !to) {
		goto failed;
	}
#endif

	/* check for conversion to/from ucs2 */
	if (is_utf16(fromcode) && to) {
		ret->direct = to->push;
		return ret;
	}
	if (is_utf16(tocode) && from) {
		ret->direct = from->pull;
		return ret;
	}

#ifdef HAVE_NATIVE_ICONV
	if (is_utf16(fromcode)) {
		ret->direct = sys_iconv;
		ret->cd_direct = ret->cd_push;
		ret->cd_push = NULL;
		return ret;
	}
	if (is_utf16(tocode)) {
		ret->direct = sys_iconv;
		ret->cd_direct = ret->cd_pull;
		ret->cd_pull = NULL;
		return ret;
	}
#endif

	/* the general case has to go via a buffer */
	if (!ret->pull) ret->pull = from->pull;
	if (!ret->push) ret->push = to->push;
	return ret;

failed:
	talloc_free(ret);
	errno = EINVAL;
	return (smb_iconv_t)-1;
}

/*
  simple iconv_open() wrapper
 */
_PUBLIC_ smb_iconv_t smb_iconv_open(const char *tocode, const char *fromcode)
{
	return smb_iconv_open_ex(NULL, tocode, fromcode, true);
}

/*
  simple iconv_close() wrapper
*/
_PUBLIC_ int smb_iconv_close(smb_iconv_t cd)
{
	talloc_free(cd);
	return 0;
}


/**********************************************************************
 the following functions implement the builtin character sets in Samba
 and also the "test" character sets that are designed to test
 multi-byte character set support for english users
***********************************************************************/
static size_t ascii_pull(void *cd, const char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	while (*inbytesleft >= 1 && *outbytesleft >= 2) {
		(*outbuf)[0] = (*inbuf)[0];
		(*outbuf)[1] = 0;
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

static size_t ascii_push(void *cd, const char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	int ir_count=0;

	while (*inbytesleft >= 2 && *outbytesleft >= 1) {
		(*outbuf)[0] = (*inbuf)[0] & 0x7F;
		if ((*inbuf)[1]) ir_count++;
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

static size_t latin1_push(void *cd, const char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	int ir_count=0;

	while (*inbytesleft >= 2 && *outbytesleft >= 1) {
		(*outbuf)[0] = (*inbuf)[0];
		if ((*inbuf)[1]) ir_count++;
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

static size_t ucs2hex_pull(void *cd, const char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	while (*inbytesleft >= 1 && *outbytesleft >= 2) {
		unsigned int v;

		if ((*inbuf)[0] != '@') {
			/* seven bit ascii case */
			(*outbuf)[0] = (*inbuf)[0];
			(*outbuf)[1] = 0;
			(*inbytesleft)  -= 1;
			(*outbytesleft) -= 2;
			(*inbuf)  += 1;
			(*outbuf) += 2;
			continue;
		}
		/* it's a hex character */
		if (*inbytesleft < 5) {
			errno = EINVAL;
			return -1;
		}
		
		if (sscanf(&(*inbuf)[1], "%04x", &v) != 1) {
			errno = EILSEQ;
			return -1;
		}

		(*outbuf)[0] = v&0xff;
		(*outbuf)[1] = v>>8;
		(*inbytesleft)  -= 5;
		(*outbytesleft) -= 2;
		(*inbuf)  += 5;
		(*outbuf) += 2;
	}

	if (*inbytesleft > 0) {
		errno = E2BIG;
		return -1;
	}
	
	return 0;
}

static size_t ucs2hex_push(void *cd, const char **inbuf, size_t *inbytesleft,
			   char **outbuf, size_t *outbytesleft)
{
	while (*inbytesleft >= 2 && *outbytesleft >= 1) {
		char buf[6];

		if ((*inbuf)[1] == 0 && 
		    ((*inbuf)[0] & 0x80) == 0 &&
		    (*inbuf)[0] != '@') {
			(*outbuf)[0] = (*inbuf)[0];
			(*inbytesleft)  -= 2;
			(*outbytesleft) -= 1;
			(*inbuf)  += 2;
			(*outbuf) += 1;
			continue;
		}
		if (*outbytesleft < 5) {
			errno = E2BIG;
			return -1;
		}
		snprintf(buf, 6, "@%04x", SVAL(*inbuf, 0));
		memcpy(*outbuf, buf, 5);
		(*inbytesleft)  -= 2;
		(*outbytesleft) -= 5;
		(*inbuf)  += 2;
		(*outbuf) += 5;
	}

	if (*inbytesleft == 1) {
		errno = EINVAL;
		return -1;
	}

	if (*inbytesleft > 1) {
		errno = E2BIG;
		return -1;
	}
	
	return 0;
}

static size_t iconv_swab(void *cd, const char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	int n;

	n = MIN(*inbytesleft, *outbytesleft);

	swab(*inbuf, *outbuf, (n&~1));
	if (n&1) {
		(*outbuf)[n-1] = 0;
	}

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


static size_t iconv_copy(void *cd, const char **inbuf, size_t *inbytesleft,
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

/*
  this takes a UTF8 sequence and produces a UTF16 sequence
 */
static size_t utf8_pull(void *cd, const char **inbuf, size_t *inbytesleft,
			 char **outbuf, size_t *outbytesleft)
{
	size_t in_left=*inbytesleft, out_left=*outbytesleft;
	const uint8_t *c = (const uint8_t *)*inbuf;
	uint8_t *uc = (uint8_t *)*outbuf;

	while (in_left >= 1 && out_left >= 2) {
		if ((c[0] & 0x80) == 0) {
			uc[0] = c[0];
			uc[1] = 0;
			c  += 1;
			in_left  -= 1;
			out_left -= 2;
			uc += 2;
			continue;
		}

		if ((c[0] & 0xe0) == 0xc0) {
			if (in_left < 2 ||
			    (c[1] & 0xc0) != 0x80) {
				errno = EILSEQ;
				goto error;
			}
			uc[1] = (c[0]>>2) & 0x7;
			uc[0] = (c[0]<<6) | (c[1]&0x3f);
			c  += 2;
			in_left  -= 2;
			out_left -= 2;
			uc += 2;
			continue;
		}

		if ((c[0] & 0xf0) == 0xe0) {
			if (in_left < 3 ||
			    (c[1] & 0xc0) != 0x80 || 
			    (c[2] & 0xc0) != 0x80) {
				errno = EILSEQ;
				goto error;
			}
			uc[1] = ((c[0]&0xF)<<4) | ((c[1]>>2)&0xF);
			uc[0] = (c[1]<<6) | (c[2]&0x3f);
			c  += 3;
			in_left  -= 3;
			out_left -= 2;
			uc += 2;
			continue;
		}

		if ((c[0] & 0xf8) == 0xf0) {
			unsigned int codepoint;
			if (in_left < 4 ||
			    (c[1] & 0xc0) != 0x80 || 
			    (c[2] & 0xc0) != 0x80 ||
			    (c[3] & 0xc0) != 0x80) {
				errno = EILSEQ;
				goto error;
			}
			codepoint = 
				(c[3]&0x3f) | 
				((c[2]&0x3f)<<6) | 
				((c[1]&0x3f)<<12) |
				((c[0]&0x7)<<18);
			if (codepoint < 0x10000) {
				/* accept UTF-8 characters that are not
				   minimally packed, but pack the result */
				uc[0] = (codepoint & 0xFF);
				uc[1] = (codepoint >> 8);
				c += 4;
				in_left -= 4;
				out_left -= 2;
				uc += 2;
				continue;
			}

			codepoint -= 0x10000;

			if (out_left < 4) {
				errno = E2BIG;
				goto error;
			}

			uc[0] = (codepoint>>10) & 0xFF;
			uc[1] = (codepoint>>18) | 0xd8;
			uc[2] = codepoint & 0xFF;
			uc[3] = ((codepoint>>8) & 0x3) | 0xdc;
			c  += 4;
			in_left  -= 4;
			out_left -= 4;
			uc += 4;
			continue;
		}

		/* we don't handle 5 byte sequences */
		errno = EINVAL;
		goto error;
	}

	if (in_left > 0) {
		errno = E2BIG;
		goto error;
	}

	*inbytesleft = in_left;
	*outbytesleft = out_left;
	*inbuf = (const char *)c;
	*outbuf = (char *)uc;
	return 0;

error:
	*inbytesleft = in_left;
	*outbytesleft = out_left;
	*inbuf = (const char *)c;
	*outbuf = (char *)uc;
	return -1;
}


/*
  this takes a UTF16 sequence and produces a UTF8 sequence
 */
static size_t utf8_push(void *cd, const char **inbuf, size_t *inbytesleft,
			char **outbuf, size_t *outbytesleft)
{
	size_t in_left=*inbytesleft, out_left=*outbytesleft;
	uint8_t *c = (uint8_t *)*outbuf;
	const uint8_t *uc = (const uint8_t *)*inbuf;

	while (in_left >= 2 && out_left >= 1) {
		unsigned int codepoint;

		if (uc[1] == 0 && !(uc[0] & 0x80)) {
			/* simplest case */
			c[0] = uc[0];
			in_left  -= 2;
			out_left -= 1;
			uc += 2;
			c  += 1;
			continue;
		}

		if ((uc[1]&0xf8) == 0) {
			/* next simplest case */
			if (out_left < 2) {
				errno = E2BIG;
				goto error;
			}
			c[0] = 0xc0 | (uc[0]>>6) | (uc[1]<<2);
			c[1] = 0x80 | (uc[0] & 0x3f);
			in_left  -= 2;
			out_left -= 2;
			uc += 2;
			c  += 2;
			continue;
		}

		if ((uc[1] & 0xfc) == 0xdc) {
			/* its the second part of a 4 byte sequence. Illegal */
			if (in_left < 4) {
				errno = EINVAL;
			} else {
				errno = EILSEQ;
			}
			goto error;
		}

		if ((uc[1] & 0xfc) != 0xd8) {
			codepoint = uc[0] | (uc[1]<<8);
			if (out_left < 3) {
				errno = E2BIG;
				goto error;
			}
			c[0] = 0xe0 | (codepoint >> 12);
			c[1] = 0x80 | ((codepoint >> 6) & 0x3f);
			c[2] = 0x80 | (codepoint & 0x3f);
			
			in_left  -= 2;
			out_left -= 3;
			uc  += 2;
			c   += 3;
			continue;
		}

		/* its the first part of a 4 byte sequence */
		if (in_left < 4) {
			errno = EINVAL;
			goto error;
		}
		if ((uc[3] & 0xfc) != 0xdc) {
			errno = EILSEQ;
			goto error;
		}
		codepoint = 0x10000 + (uc[2] | ((uc[3] & 0x3)<<8) | 
				       (uc[0]<<10) | ((uc[1] & 0x3)<<18));
		
		if (out_left < 4) {
			errno = E2BIG;
			goto error;
		}
		c[0] = 0xf0 | (codepoint >> 18);
		c[1] = 0x80 | ((codepoint >> 12) & 0x3f);
		c[2] = 0x80 | ((codepoint >> 6) & 0x3f);
		c[3] = 0x80 | (codepoint & 0x3f);
		
		in_left  -= 4;
		out_left -= 4;
		uc       += 4;
		c        += 4;
	}

	if (in_left == 1) {
		errno = EINVAL;
		goto error;
	}

	if (in_left > 1) {
		errno = E2BIG;
		goto error;
	}

	*inbytesleft = in_left;
	*outbytesleft = out_left;
	*inbuf  = (const char *)uc;
	*outbuf = (char *)c;
	
	return 0;

error:
	*inbytesleft = in_left;
	*outbytesleft = out_left;
	*inbuf  = (const char *)uc;
	*outbuf = (char *)c;
	return -1;
}


/*
  this takes a UTF16 munged sequence, modifies it according to the
  string2key rules, and produces a UTF16 sequence

The rules are:

    1) any 0x0000 characters are mapped to 0x0001

    2) convert any instance of 0xD800 - 0xDBFF (high surrogate)
       without an immediately following 0xDC00 - 0x0xDFFF (low surrogate) to
       U+FFFD (OBJECT REPLACEMENT CHARACTER).

    3) the same for any low surrogate that was not preceded by a high surrogate.

 */
static size_t utf16_munged_pull(void *cd, const char **inbuf, size_t *inbytesleft,
			       char **outbuf, size_t *outbytesleft)
{
	size_t in_left=*inbytesleft, out_left=*outbytesleft;
	uint8_t *c = (uint8_t *)*outbuf;
	const uint8_t *uc = (const uint8_t *)*inbuf;

	while (in_left >= 2 && out_left >= 2) {
		unsigned int codepoint = uc[0] | (uc[1]<<8);

		if (codepoint == 0) {
			codepoint = 1;
		}

		if ((codepoint & 0xfc00) == 0xd800) {
			/* a high surrogate */
			unsigned int codepoint2;
			if (in_left < 4) {
				codepoint = 0xfffd;
				goto codepoint16;				
			}
			codepoint2 = uc[2] | (uc[3]<<8);
			if ((codepoint2 & 0xfc00) != 0xdc00) {
				/* high surrogate not followed by low
				   surrogate: convert to 0xfffd */
				codepoint = 0xfffd;
				goto codepoint16;
			}
			if (out_left < 4) {
				errno = E2BIG;
				goto error;
			}
			memcpy(c, uc, 4);
			in_left  -= 4;
			out_left -= 4;
			uc       += 4;
			c        += 4;
			continue;
		}

		if ((codepoint & 0xfc00) == 0xdc00) {
			/* low surrogate not preceded by high
			   surrogate: convert to 0xfffd */
			codepoint = 0xfffd;
		}

	codepoint16:
		c[0] = codepoint & 0xFF;
		c[1] = (codepoint>>8) & 0xFF;
		
		in_left  -= 2;
		out_left -= 2;
		uc  += 2;
		c   += 2;
		continue;		
	}

	if (in_left == 1) {
		errno = EINVAL;
		goto error;
	}

	if (in_left > 1) {
		errno = E2BIG;
		goto error;
	}

	*inbytesleft = in_left;
	*outbytesleft = out_left;
	*inbuf  = (const char *)uc;
	*outbuf = (char *)c;
	
	return 0;

error:
	*inbytesleft = in_left;
	*outbytesleft = out_left;
	*inbuf  = (const char *)uc;
	*outbuf = (char *)c;
	return -1;
}




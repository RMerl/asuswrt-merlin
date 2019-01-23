/*
 * Unix SMB/CIFS implementation.
 *
 * Registry helper routines
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
 * @file   reg_parse_internal.h
 * @author Gregor Beck <gb@sernet.de>
 * @date   Sep 2010
 * @brief
 */

#include "reg_parse_internal.h"
#include "cbuf.h"
#include "srprs.h"
#include "registry.h"

size_t iconvert_talloc(const void* ctx,
		       smb_iconv_t cd,
		       const char* src, size_t srclen,
		       char** pdst)
{
	size_t dstlen, ret;
	size_t obytes, ibytes;
	char *optr, *dst, *tmp;
	const char* iptr;

	if (cd == NULL || cd == ((smb_iconv_t)-1)) {
		return -1;
	}

	dst = *pdst;

	if (dst == NULL) {
		/*
		 * Allocate an extra two bytes for the
		 * terminating zero.
		 */
		dstlen = srclen + 2;
		dst = (char *)talloc_size(ctx, dstlen);
		if (dst == NULL) {
			DEBUG(0,("iconver_talloc no mem\n"));
			return -1;
		}
	} else {
		dstlen = talloc_get_size(dst);
	}
convert:
	iptr   = src;
	ibytes = srclen;
	optr   = dst;
	obytes = dstlen-2;

	ret = smb_iconv(cd, &iptr, &ibytes, &optr, &obytes);

	if(ret == -1) {
		const char *reason="unknown error";
		switch(errno) {
		case EINVAL:
			reason="Incomplete multibyte sequence";
			break;
		case E2BIG:
			dstlen = 2*dstlen + 2;
			tmp    = talloc_realloc(ctx, dst, char, dstlen);
			if (tmp == NULL) {
				reason="talloc_realloc failed";
				break;
			}
			dst = tmp;
			goto convert;
		case EILSEQ:
			reason="Illegal multibyte sequence";
			break;
		}
		DEBUG(0,("Conversion error: %s(%.80s) %li\n", reason, iptr,
			 (long int)(iptr-src)));
		talloc_free(dst);
		return -1;
	}

	dstlen = (dstlen-2) - obytes;

	SSVAL(dst, dstlen, 0);

	*pdst = dst;
	return dstlen;
}

#ifndef HKEY_CURRENT_CONFIG
#define HKEY_CURRENT_CONFIG		0x80000005
#endif
#ifndef HKEY_DYN_DATA
#define HKEY_DYN_DATA			0x80000006
#endif
#ifndef HKEY_PERFORMANCE_TEXT
#define HKEY_PERFORMANCE_TEXT		0x80000050
#endif
#ifndef HKEY_PERFORMANCE_NLSTEXT
#define HKEY_PERFORMANCE_NLSTEXT	0x80000060
#endif

#define HIVE_INFO_ENTRY(SHORT,LONG)			\
static const struct hive_info HIVE_INFO_##SHORT = {	\
	.handle = LONG,					\
	.short_name = #SHORT,				\
	.short_name_len = sizeof(#SHORT)-1,		\
	.long_name = #LONG,				\
	.long_name_len = sizeof(#LONG)-1,		\
}

HIVE_INFO_ENTRY(HKLM, HKEY_LOCAL_MACHINE);
HIVE_INFO_ENTRY(HKCU, HKEY_CURRENT_USER);
HIVE_INFO_ENTRY(HKCR, HKEY_CLASSES_ROOT);
HIVE_INFO_ENTRY(HKU , HKEY_USERS);
HIVE_INFO_ENTRY(HKCC, HKEY_CURRENT_CONFIG);
HIVE_INFO_ENTRY(HKDD, HKEY_DYN_DATA);
HIVE_INFO_ENTRY(HKPD, HKEY_PERFORMANCE_DATA);
HIVE_INFO_ENTRY(HKPT, HKEY_PERFORMANCE_TEXT);
HIVE_INFO_ENTRY(HKPN, HKEY_PERFORMANCE_NLSTEXT);
#undef HIVE_INFO_ENTRY

static const struct hive_info* HIVE_INFO[] = {
	&HIVE_INFO_HKLM, &HIVE_INFO_HKCU, &HIVE_INFO_HKCR, &HIVE_INFO_HKU,
	&HIVE_INFO_HKCC, &HIVE_INFO_HKDD, &HIVE_INFO_HKPD, &HIVE_INFO_HKPT,
	&HIVE_INFO_HKPN, NULL
};

const struct hive_info* hive_info(const char* name, int nlen)
{
	const struct hive_info** info;
	char buf[32];
	int s;

	if (nlen >= sizeof(buf)) {
		return NULL;
	}
	for (s=0; s<nlen; s++) {
		buf[s] = toupper(name[s]);
	}
	buf[s] = '\0';

	if ((s < 3) || (strncmp(buf, "HK", 2) != 0)) {
		return NULL;
	}

	if (s <= 4) {
		for(info = HIVE_INFO; *info; info++) {
			if (strcmp(buf+2, (*info)->short_name+2) == 0) {
				return *info;
			}
		}
		return NULL;
	}

	if ((s < 10) || (strncmp(buf, "HKEY_", 5)) != 0) {
		return NULL;
	}

	for(info = HIVE_INFO; *info; info++) {
		if (strcmp(buf+5, (*info)->long_name+5) == 0) {
			return *info;
		}
	}
	return NULL;
}

const char* get_charset(const char* c)
{
	if (strcmp(c, "dos") == 0) {
		return lp_dos_charset();
	} else if (strcmp(c, "unix") == 0) {
		return lp_unix_charset();
	} else {
		return c;
	}
}

bool set_iconv(smb_iconv_t* t, const char* to, const char* from)
{
	smb_iconv_t cd = (smb_iconv_t)-1;

	if (to && from) {
		to   = get_charset(to);
		from = get_charset(from);
		cd   = smb_iconv_open(to, from);
		if (cd == ((smb_iconv_t)-1)) {
			return false;
		}
	}
	if ((*t != (smb_iconv_t)NULL) && (*t != (smb_iconv_t)-1)) {
		smb_iconv_close(*t);
	}
	*t = cd;
	return true;
}

/**
 * Parse option string
 * @param[in,out] ptr parse position
 * @param[in] mem_ctx talloc context
 * @param[out] name ptr 2 value
 * @param[out] value ptr 2 value
 * @return true on success
 */
bool srprs_option(const char** ptr, const void* mem_ctx, char** name, char** value)
{
	const char* pos = *ptr;
	void* ctx = talloc_new(mem_ctx);

	cbuf* key = cbuf_new(ctx);
	cbuf* val = NULL;

	while(srprs_charsetinv(&pos, ",= \t\n\r", key))
		;
	if (pos == *ptr) {
		talloc_free(ctx);
		return false;
	}

	if (name != NULL) {
		*name = talloc_steal(mem_ctx, cbuf_gets(key, 0));
	}

	if (*pos == '=') {
		val = cbuf_new(ctx);
		pos++;
		if (!srprs_quoted_string(ptr, val, NULL)) {
			while(srprs_charsetinv(&pos, ", \t\n\r", val))
				;
		}
		if (value != NULL) {
			*value = talloc_steal(mem_ctx, cbuf_gets(val, 0));
		}
	} else {
		if (value != NULL) {
			*value = NULL;
		}
	}

	while(srprs_char(&pos, ','))
		;

	*ptr = pos;
	return true;
}

#define CH_INVALID ((charset_t)-1)
static const struct {
	const char* const name;
	charset_t ctype;
	int  len;
	char seq[4];
} BOM[] = {
	{"UTF-8",    CH_UTF8,    3, {0xEF, 0xBB, 0xBF}},
	{"UTF-32LE", CH_INVALID, 4, {0xFF, 0xFE, 0x00, 0x00}},
	{"UTF-16LE", CH_UTF16LE, 2, {0xFF, 0xFE}},
	{"UTF-16BE", CH_UTF16BE, 2, {0xFE, 0xFF}},
	{"UTF-32BE", CH_INVALID, 4, {0x00, 0x00, 0xFE, 0xFF}},
	{NULL,       CH_INVALID, 0}
};

bool srprs_bom(const char** ptr, const char** name, charset_t* ctype)
{
	int i;
	for (i=0; BOM[i].name; i++) {
		if (memcmp(*ptr, BOM[i].seq, BOM[i].len) == 0) {
			break;
		}
	}

	if (BOM[i].name != NULL) {
		DEBUG(0, ("Found Byte Order Mark for : %s\n", BOM[i].name));

		if (name != NULL) {
			*name  = BOM[i].name;
		}

		if (ctype != NULL) {
			*ctype = BOM[i].ctype;
		}

		*ptr  += BOM[i].len;

		return true;
	}
	return false;
}

int write_bom(FILE* file, const char* charset, charset_t ctype)
{
	int i;
	if ( charset == NULL ) {
		for (i=0; BOM[i].name; i++) {
			if (BOM[i].ctype == ctype) {
				return fwrite(BOM[i].seq, 1, BOM[i].len, file);
			}
		}
		DEBUG(0, ("No Byte Order Mark for charset_t: %u\n", (unsigned)ctype));
	} else {
		for (i=0; BOM[i].name; i++) {
			if (StrCaseCmp(BOM[i].name, charset) == 0) {
				return fwrite(BOM[i].seq, 1, BOM[i].len, file);
			}
		}
		DEBUG(0, ("No Byte Order Mark for charset_t: %s\n", charset));
	}
	return 0;
}


int cbuf_puts_case(cbuf* s, const char* str, size_t len, enum fmt_case fmt)
{
	size_t pos = cbuf_getpos(s);
	int ret = cbuf_puts(s, str, len);
	char* ptr = cbuf_gets(s,pos);

	if (ret <= 0) {
		return ret;
	}

	switch (fmt) {
	case FMT_CASE_PRESERVE:
		break;
	case FMT_CASE_UPPER:
		while(*ptr != '\0') {
			*ptr = toupper(*ptr);
			ptr++;
		}
		break;
	case FMT_CASE_TITLE:
		*ptr = toupper(*ptr);
		ptr++;
	case FMT_CASE_LOWER:
		while(*ptr != '\0') {
			*ptr = tolower(*ptr);
			ptr++;
		}
	}
	return ret;
}

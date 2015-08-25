/* $Id: print_util.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJSIP_PRINT_H__
#define __PJSIP_PRINT_H__

#define copy_advance_check(buf,str)   \
	do { \
	    if ((str).slen >= (endbuf-buf)) return -1;	\
	    pj_memcpy(buf, (str).ptr, (str).slen); \
	    buf += (str).slen; \
	} while (0)

#define copy_advance_pair_check(buf,str1,len1,str2)   \
	do { \
	    if (str2.slen) { \
		printed = len1+str2.slen; \
		if (printed >= (endbuf-buf)) return -1;	\
		pj_memcpy(buf,str1,len1); \
		pj_memcpy(buf+len1, str2.ptr, str2.slen); \
		buf += printed; \
	    } \
	} while (0)

#define copy_advance_pair_quote_check(buf,str1,len1,str2,quotebegin,quoteend) \
	do { \
	    if (str2.slen) { \
		printed = len1+str2.slen+2; \
		if (printed >= (endbuf-buf)) return -1;	\
		pj_memcpy(buf,str1,len1); \
		*(buf+len1)=quotebegin; \
		pj_memcpy(buf+len1+1, str2.ptr, str2.slen); \
		*(buf+printed-1) = quoteend; \
		buf += printed; \
	    } \
	} while (0)

#define copy_advance_pair_quote(buf,str1,len1,str2,quotebegin,quoteend) \
	do { \
		printed = len1+str2.slen+2; \
		if (printed >= (endbuf-buf)) return -1;	\
		pj_memcpy(buf,str1,len1); \
		*(buf+len1)=quotebegin; \
		pj_memcpy(buf+len1+1, str2.ptr, str2.slen); \
		*(buf+printed-1) = quoteend; \
		buf += printed; \
	} while (0)

#define copy_advance_pair_escape(buf,str1,len1,str2,unres)	\
	do { \
	  if (str2.slen) { \
	    if (len1+str2.slen >= (endbuf-buf)) return -1; \
	    pj_memcpy(buf,str1,len1); \
	    printed=pj_strncpy2_escape(buf+len1,&str2,(endbuf-buf-len1),&unres);\
	    if (printed < 0) return -1; \
	    buf += (printed+len1); \
	  } \
	} while (0)


#define copy_advance_no_check(buf,str)   \
	do { \
	    pj_memcpy(buf, (str).ptr, (str).slen); \
	    buf += (str).slen; \
	} while (0)

#define copy_advance_escape(buf,str,unres)    \
	do { \
	    printed = \
		pj_strncpy2_escape(buf, &(str), (endbuf-buf), &(unres)); \
	    if (printed < 0) return -1; \
	    buf += printed; \
	} while (0)

#define copy_advance_pair_no_check(buf,str1,len1,str2)   \
	if (str2.slen) { \
	    pj_memcpy(buf,str1,len1); \
	    pj_memcpy(buf+len1, str2.ptr, str2.slen); \
	    buf += len1+str2.slen; \
	}

#define copy_advance 		copy_advance_check
#define copy_advance_pair 	copy_advance_pair_check

#define copy_advance_pair_quote_cond(buf,str1,len1,str2,quotebegin,quoteend) \
	do {	\
	  if (str2.slen && *str2.ptr!=quotebegin) \
	    copy_advance_pair_quote(buf,str1,len1,str2,quotebegin,quoteend); \
	  else \
	    copy_advance_pair(buf,str1,len1,str2); \
	} while (0)

/*
 * Internal type declarations.
 */
typedef void* (*pjsip_hdr_clone_fptr)(pj_pool_t *, const void*);
typedef int   (*pjsip_hdr_print_fptr)(void *hdr, char *buf, pj_size_t len);

typedef struct pjsip_hdr_name_info_t
{
    char	*name;
    unsigned	 name_len;
    char	*sname;
} pjsip_hdr_name_info_t;

extern const pjsip_hdr_name_info_t pjsip_hdr_names[];

PJ_INLINE(void) init_hdr(void *hptr, pjsip_hdr_e htype, void *vptr)
{
    pjsip_hdr *hdr = (pjsip_hdr*) hptr;
    hdr->type = htype;
    hdr->name.ptr = pjsip_hdr_names[htype].name;
    hdr->name.slen = pjsip_hdr_names[htype].name_len;
    if (pjsip_hdr_names[htype].sname) {
	hdr->sname.ptr = pjsip_hdr_names[htype].sname;
	hdr->sname.slen = 1;
    } else {
	hdr->sname = hdr->name;
    }
    hdr->vptr = (pjsip_hdr_vptr*) vptr;
    pj_list_init(hdr);
}

#endif	/* __PJSIP_PRINT_H__ */


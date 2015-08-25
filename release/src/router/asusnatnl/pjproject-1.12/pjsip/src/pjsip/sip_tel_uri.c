/* $Id: sip_tel_uri.c 4399 2013-02-27 14:26:03Z riza $ */
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
#include <pjsip/sip_tel_uri.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_parser.h>
#include <pjsip/print_util.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/ctype.h>
#include <pj/except.h>
#include <pjlib-util/string.h>
#include <pjlib-util/scanner.h>

#define ALPHA
#define DIGITS		    "0123456789"
#define HEX		    "aAbBcCdDeEfF"
#define HEX_DIGITS	    DIGITS HEX
#define VISUAL_SEP	    "-.()"
#define PHONE_DIGITS	    DIGITS VISUAL_SEP
#define GLOBAL_DIGITS	    "+" PHONE_DIGITS
#define LOCAL_DIGITS	    HEX_DIGITS "*#" VISUAL_SEP
#define NUMBER_SPEC	    LOCAL_DIGITS GLOBAL_DIGITS
#define PHONE_CONTEXT	    ALPHA GLOBAL_DIGITS
//#define RESERVED	    ";/?:@&=+$,"
#define RESERVED	    "/:@&$,+"
#define MARK		    "-_.!~*'()"
#define UNRESERVED	    ALPHA DIGITS MARK
#define ESCAPED		    "%"
#define URIC		    RESERVED UNRESERVED ESCAPED "[]+"
#define PARAM_UNRESERVED    "[]/:&+$"
#define PARAM_CHAR	    PARAM_UNRESERVED UNRESERVED ESCAPED

static pj_cis_buf_t cis_buf;
static pj_cis_t pjsip_TEL_NUMBER_SPEC;
static pj_cis_t pjsip_TEL_EXT_VALUE_SPEC;
static pj_cis_t pjsip_TEL_PHONE_CONTEXT_SPEC;
static pj_cis_t pjsip_TEL_URIC_SPEC;
static pj_cis_t pjsip_TEL_VISUAL_SEP_SPEC;
static pj_cis_t pjsip_TEL_PNAME_SPEC;
static pj_cis_t pjsip_TEL_PVALUE_SPEC;
static pj_cis_t pjsip_TEL_PVALUE_SPEC_ESC;
static pj_cis_t pjsip_TEL_PARSING_PVALUE_SPEC;
static pj_cis_t pjsip_TEL_PARSING_PVALUE_SPEC_ESC;

static pj_str_t pjsip_ISUB_STR = { "isub", 4 };
static pj_str_t pjsip_EXT_STR = { "ext", 3 };
static pj_str_t pjsip_PH_CTX_STR = { "phone-context", 13 };


static const pj_str_t *tel_uri_get_scheme( const pjsip_tel_uri* );
static void *tel_uri_get_uri( pjsip_tel_uri* );
static pj_ssize_t tel_uri_print( pjsip_uri_context_e context,
				 const pjsip_tel_uri *url, 
				 char *buf, pj_size_t size);
static int tel_uri_cmp( pjsip_uri_context_e context,
			const pjsip_tel_uri *url1, const pjsip_tel_uri *url2);
static pjsip_tel_uri* tel_uri_clone(pj_pool_t *pool, const pjsip_tel_uri *rhs);
static void*	      tel_uri_parse( pj_scanner *scanner, pj_pool_t *pool,
				     pj_bool_t parse_params);

typedef const pj_str_t* (*P_GET_SCHEME)(const void*);
typedef void* 		(*P_GET_URI)(void*);
typedef pj_ssize_t 	(*P_PRINT_URI)(pjsip_uri_context_e,const void *,
				       char*,pj_size_t);
typedef int 		(*P_CMP_URI)(pjsip_uri_context_e, const void*, 
				     const void*);
typedef void* 		(*P_CLONE)(pj_pool_t*, const void*);

static pjsip_uri_vptr tel_uri_vptr = 
{
    (P_GET_SCHEME)	&tel_uri_get_scheme,
    (P_GET_URI) 	&tel_uri_get_uri,
    (P_PRINT_URI) 	&tel_uri_print,
    (P_CMP_URI)		&tel_uri_cmp,
    (P_CLONE)		&tel_uri_clone
};

static int is_initialized;


PJ_DEF(pjsip_tel_uri*) pjsip_tel_uri_create(pj_pool_t *pool)
{
    pjsip_tel_uri *uri = PJ_POOL_ZALLOC_T(pool, pjsip_tel_uri);
    uri->vptr = &tel_uri_vptr;
    pj_list_init(&uri->other_param);
    return uri;
}


static const pj_str_t *tel_uri_get_scheme( const pjsip_tel_uri *uri )
{
    PJ_UNUSED_ARG(uri);
    return &pjsip_parser_const()->pjsip_TEL_STR;
}

static void *tel_uri_get_uri( pjsip_tel_uri *uri )
{
    return uri;
}


pj_status_t pjsip_tel_uri_subsys_init(int inst_id)
{
    pj_status_t status;

	if (is_initialized)
		return PJ_SUCCESS;

    pj_cis_buf_init(&cis_buf);

    status = pj_cis_init(&cis_buf, &pjsip_TEL_EXT_VALUE_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_str(&pjsip_TEL_EXT_VALUE_SPEC, PHONE_DIGITS);

    status = pj_cis_init(&cis_buf, &pjsip_TEL_NUMBER_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_str(&pjsip_TEL_NUMBER_SPEC, NUMBER_SPEC);

    status = pj_cis_init(&cis_buf, &pjsip_TEL_VISUAL_SEP_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_str(&pjsip_TEL_VISUAL_SEP_SPEC, VISUAL_SEP);

    status = pj_cis_init(&cis_buf, &pjsip_TEL_PHONE_CONTEXT_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_alpha(&pjsip_TEL_PHONE_CONTEXT_SPEC);
    pj_cis_add_num(&pjsip_TEL_PHONE_CONTEXT_SPEC);
    pj_cis_add_str(&pjsip_TEL_PHONE_CONTEXT_SPEC, PHONE_CONTEXT);

    status = pj_cis_init(&cis_buf, &pjsip_TEL_URIC_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_alpha(&pjsip_TEL_URIC_SPEC);
    pj_cis_add_num(&pjsip_TEL_URIC_SPEC);
    pj_cis_add_str(&pjsip_TEL_URIC_SPEC, URIC);

    status = pj_cis_init(&cis_buf, &pjsip_TEL_PNAME_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_alpha(&pjsip_TEL_PNAME_SPEC);
    pj_cis_add_num(&pjsip_TEL_PNAME_SPEC);
    pj_cis_add_str(&pjsip_TEL_PNAME_SPEC, "-");

    status = pj_cis_init(&cis_buf, &pjsip_TEL_PVALUE_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_alpha(&pjsip_TEL_PVALUE_SPEC);
    pj_cis_add_num(&pjsip_TEL_PVALUE_SPEC);
    pj_cis_add_str(&pjsip_TEL_PVALUE_SPEC, PARAM_CHAR);

    status = pj_cis_dup(&pjsip_TEL_PVALUE_SPEC_ESC, &pjsip_TEL_PVALUE_SPEC);
    pj_cis_del_str(&pjsip_TEL_PVALUE_SPEC_ESC, "%");

    status = pj_cis_dup(&pjsip_TEL_PARSING_PVALUE_SPEC, &pjsip_TEL_URIC_SPEC);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    pj_cis_add_cis(&pjsip_TEL_PARSING_PVALUE_SPEC, &pjsip_TEL_PVALUE_SPEC);
    pj_cis_add_str(&pjsip_TEL_PARSING_PVALUE_SPEC, "=");

    status = pj_cis_dup(&pjsip_TEL_PARSING_PVALUE_SPEC_ESC, 
			&pjsip_TEL_PARSING_PVALUE_SPEC);
    pj_cis_del_str(&pjsip_TEL_PARSING_PVALUE_SPEC_ESC, "%");

    status = pjsip_register_uri_parser(inst_id, "tel", &tel_uri_parse);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	is_initialized = 1;

    return PJ_SUCCESS;
}

/* Print tel: URI */
static pj_ssize_t tel_uri_print( pjsip_uri_context_e context,
				 const pjsip_tel_uri *uri, 
				 char *buf, pj_size_t size)
{
    int printed;
    char *startbuf = buf;
    char *endbuf = buf+size-1;
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    PJ_UNUSED_ARG(context);

    /* Print scheme. */
    copy_advance(buf, pc->pjsip_TEL_STR);
    *buf++ = ':';

    /* Print number. */
    copy_advance_escape(buf, uri->number, pjsip_TEL_NUMBER_SPEC);

    /* ISDN sub-address or extension must appear first. */

    /* Extension param. */
    copy_advance_pair_escape(buf, ";ext=", 5, uri->ext_param, 
			     pjsip_TEL_EXT_VALUE_SPEC);

    /* ISDN sub-address. */
    copy_advance_pair_escape(buf, ";isub=", 6, uri->isub_param, 
			     pjsip_TEL_URIC_SPEC);

    /* Followed by phone context, if present. */
    copy_advance_pair_escape(buf, ";phone-context=", 15, uri->context, 
			     pjsip_TEL_PHONE_CONTEXT_SPEC);


    /* Print other parameters. */
    printed = pjsip_param_print_on(&uri->other_param, buf, (endbuf-buf), 
				   &pjsip_TEL_PNAME_SPEC, 
				   &pjsip_TEL_PVALUE_SPEC, ';');
    if (printed < 0)
	return -1;
    buf += printed;

    *buf = '\0';

    return (buf-startbuf);
}

/* Compare two numbers, according to RFC 3966:
 *  - both must be either local or global numbers.
 *  - The 'global-number-digits' and the 'local-number-digits' must be
 *    equal, after removing all visual separators.
 */
PJ_DEF(int) pjsip_tel_nb_cmp(const pj_str_t *number1, const pj_str_t *number2)
{
    const char *s1 = number1->ptr,
	       *e1 = number1->ptr + number1->slen,
	       *s2 = number2->ptr,
	       *e2 = number2->ptr + number2->slen;

    /* Compare each number, ignoreing visual separators. */
    while (s1!=e1 && s2!=e2) {
	int diff;

	if (pj_cis_match(&pjsip_TEL_VISUAL_SEP_SPEC, *s1)) {
	    ++s1;
	    continue;
	}
	if (pj_cis_match(&pjsip_TEL_VISUAL_SEP_SPEC, *s2)) {
	    ++s2;
	    continue;
	}

	diff = pj_tolower(*s1) - pj_tolower(*s2);
	if (!diff) {
	    ++s1, ++s2;
	    continue;
	} else
	    return diff;
    }

    /* Exhaust remaining visual separators. */
    while (s1!=e1 && pj_cis_match(&pjsip_TEL_VISUAL_SEP_SPEC, *s1))
	++s1;
    while (s2!=e2 && pj_cis_match(&pjsip_TEL_VISUAL_SEP_SPEC, *s2))
	++s2;

    if (s1==e1 && s2==e2)
	return 0;
    else if (s1==e1)
	return -1;
    else
	return 1;
}

/* Compare two tel: URI */
static int tel_uri_cmp( pjsip_uri_context_e context,
			const pjsip_tel_uri *url1, const pjsip_tel_uri *url2)
{
    int result;

    PJ_UNUSED_ARG(context);

    /* Scheme must match. */
    if (url1->vptr != url2->vptr)
	return -1;

    /* Compare number. */
    result = pjsip_tel_nb_cmp(&url1->number, &url2->number);
    if (result != 0)
	return result;

    /* Compare phone-context as hostname or as as global nb. */
    if (url1->context.slen) {
	if (*url1->context.ptr != '+')
	    result = pj_stricmp(&url1->context, &url2->context);
	else
	    result = pjsip_tel_nb_cmp(&url1->context, &url2->context);

	if (result != 0)
	    return result;

    } else if (url2->context.slen)
	return -1;

    /* Compare extension. */
    if (url1->ext_param.slen) {
	result = pjsip_tel_nb_cmp(&url1->ext_param, &url2->ext_param);
	if (result != 0)
	    return result;
    }

    /* Compare isub bytes by bytes. */
    if (url1->isub_param.slen) {
	result = pj_stricmp(&url1->isub_param, &url2->isub_param);
	if (result != 0)
	    return result;
    }

    /* Other parameters are compared regardless of the order.
     * If one URI has parameter not found in the other URI, the URIs are
     * not equal.
     */
    if (url1->other_param.next != &url1->other_param) {
	const pjsip_param *p1, *p2;
	int cnt1 = 0, cnt2 = 0;

	p1 = url1->other_param.next;
	while (p1 != &url1->other_param) {
	    p2 = pjsip_param_cfind(&url2->other_param, &p1->name);
	    if (!p2 )
		return 1;

	    result = pj_stricmp(&p1->value, &p2->value);
	    if (result != 0)
		return result;

	    p1 = p1->next;
	    ++cnt1;
	}

	p2 = url2->other_param.next;
	while (p2 != &url2->other_param)
	    ++cnt2, p2 = p2->next;

	if (cnt1 < cnt2)
	    return -1;
	else if (cnt1 > cnt2)
	    return 1;

    } else if (url2->other_param.next != &url2->other_param)
	return -1;

    /* Equal. */
    return 0;
}

/* Clone tel: URI */
static pjsip_tel_uri* tel_uri_clone(pj_pool_t *pool, const pjsip_tel_uri *rhs)
{
    pjsip_tel_uri *uri = pjsip_tel_uri_create(pool);

    pj_strdup(pool, &uri->number, &rhs->number);
    pj_strdup(pool, &uri->context, &rhs->context);
    pj_strdup(pool, &uri->ext_param, &rhs->ext_param);
    pj_strdup(pool, &uri->isub_param, &rhs->isub_param);
    pjsip_param_clone(pool, &uri->other_param, &rhs->other_param);

    return uri;
}

/* Parse tel: URI 
 * THis actually returns (pjsip_tel_uri *) type.
 */
static void* tel_uri_parse( pj_scanner *scanner, pj_pool_t *pool,
			    pj_bool_t parse_params)
{
    pjsip_tel_uri *uri;
    pj_str_t token;
    int skip_ws = scanner->skip_ws;
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    scanner->skip_ws = 0;

    /* Parse scheme. */
    pj_scan_get(scanner, &pc->pjsip_TOKEN_SPEC, &token);
    if (pj_scan_get_char(scanner) != ':')
	PJ_THROW(scanner->inst_id, PJSIP_SYN_ERR_EXCEPTION[scanner->inst_id]);
    if (pj_stricmp_alnum(&token, &pc->pjsip_TEL_STR) != 0)
	PJ_THROW(scanner->inst_id, PJSIP_SYN_ERR_EXCEPTION[scanner->inst_id]);

    /* Create URI */
    uri = pjsip_tel_uri_create(pool);

    /* Get the phone number. */
#if defined(PJSIP_UNESCAPE_IN_PLACE) && PJSIP_UNESCAPE_IN_PLACE!=0
    pj_scan_get_unescape(scanner, &pjsip_TEL_NUMBER_SPEC, &uri->number);
#else
    pj_scan_get(scanner, &pjsip_TEL_NUMBER_SPEC, &uri->number);
    uri->number = pj_str_unescape(pool, &uri->number);
#endif

    /* Get all parameters. */
    if (parse_params && *scanner->curptr==';') {
	pj_str_t pname, pvalue;
	const pjsip_parser_const_t *pc = pjsip_parser_const();

	do {
	    /* Eat the ';' separator. */
	    pj_scan_get_char(scanner);

	    /* Get pname. */
	    pj_scan_get(scanner, &pc->pjsip_PARAM_CHAR_SPEC, &pname);

	    if (*scanner->curptr == '=') {
		pj_scan_get_char(scanner);

#		if defined(PJSIP_UNESCAPE_IN_PLACE) && PJSIP_UNESCAPE_IN_PLACE!=0
		    pj_scan_get_unescape(scanner, 
					 &pjsip_TEL_PARSING_PVALUE_SPEC_ESC,
					 &pvalue);
#		else
		    pj_scan_get(scanner, &pjsip_TEL_PARSING_PVALUE_SPEC, 
				&pvalue);
		    pvalue = pj_str_unescape(pool, &pvalue);
#		endif

	    } else {
		pvalue.slen = 0;
		pvalue.ptr = NULL;
	    }

	    /* Save the parameters. */
	    if (pj_stricmp_alnum(&pname, &pjsip_ISUB_STR)==0) {
		uri->isub_param = pvalue;
	    } else if (pj_stricmp_alnum(&pname, &pjsip_EXT_STR)==0) {
		uri->ext_param = pvalue;
	    } else if (pj_stricmp_alnum(&pname, &pjsip_PH_CTX_STR)==0) {
		uri->context = pvalue;
	    } else {
		pjsip_param *param = PJ_POOL_ALLOC_T(pool, pjsip_param);
		param->name = pname;
		param->value = pvalue;
		pj_list_insert_before(&uri->other_param, param);
	    }

	} while (*scanner->curptr==';');
    }

    scanner->skip_ws = skip_ws;
    pj_scan_skip_whitespace(scanner);
    return uri;
}


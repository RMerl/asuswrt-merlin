/* $Id: sip_uri.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip/sip_uri.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_parser.h>
#include <pjsip/print_util.h>
#include <pjsip/sip_errno.h>
#include <pjlib-util/string.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>

/*
 * Generic parameter manipulation.
 */
PJ_DEF(pjsip_param*) pjsip_param_find(  const pjsip_param *param_list,
					const pj_str_t *name )
{
    pjsip_param *p = (pjsip_param*)param_list->next;
    while (p != param_list) {
	if (pj_stricmp(&p->name, name)==0)
	    return p;
	p = p->next;
    }
    return NULL;
}

PJ_DEF(int) pjsip_param_cmp( const pjsip_param *param_list1,
			     const pjsip_param *param_list2,
			     pj_bool_t ig_nf)
{
    const pjsip_param *p1;

    if ((ig_nf & 1)==0 && pj_list_size(param_list1)!=pj_list_size(param_list2))
	return 1;

    p1 = param_list1->next;
    while (p1 != param_list1) {
	const pjsip_param *p2;
	p2 = pjsip_param_find(param_list2, &p1->name);
	if (p2 ) {
	    int rc = pj_stricmp(&p1->value, &p2->value);
	    if (rc != 0)
		return rc;
	} else if ((ig_nf & 1)==0)
	    return 1;

	p1 = p1->next;
    }

    return 0;
}

PJ_DEF(void) pjsip_param_clone( pj_pool_t *pool, pjsip_param *dst_list,
				const pjsip_param *src_list)
{
    const pjsip_param *p = src_list->next;

    pj_list_init(dst_list);
    while (p && p != src_list) {
	pjsip_param *new_param = PJ_POOL_ALLOC_T(pool, pjsip_param);
	pj_strdup(pool, &new_param->name, &p->name);
	pj_strdup(pool, &new_param->value, &p->value);
	pj_list_insert_before(dst_list, new_param);
	p = p->next;
    }
}


PJ_DEF(void) pjsip_param_shallow_clone( pj_pool_t *pool, 
					pjsip_param *dst_list,
					const pjsip_param *src_list)
{
    const pjsip_param *p = src_list->next;

    pj_list_init(dst_list);
    while (p != src_list) {
	pjsip_param *new_param = PJ_POOL_ALLOC_T(pool, pjsip_param);
	new_param->name = p->name;
	new_param->value = p->value;
	pj_list_insert_before(dst_list, new_param);
	p = p->next;
    }
}

PJ_DEF(pj_ssize_t) pjsip_param_print_on( const pjsip_param *param_list,
					 char *buf, pj_size_t size,
					 const pj_cis_t *pname_spec,
					 const pj_cis_t *pvalue_spec,
					 int sep)
{
    const pjsip_param *p;
    char *startbuf;
    char *endbuf;
    int printed;

    p = param_list->next;
    if (p == NULL || p == param_list)
	return 0;

    startbuf = buf;
    endbuf = buf + size;

    PJ_UNUSED_ARG(pname_spec);

    do {
	*buf++ = (char)sep;
	copy_advance_escape(buf, p->name, (*pname_spec));
	if (p->value.slen) {
	    *buf++ = '=';
	    if (*p->value.ptr == '"')
		copy_advance(buf, p->value);
	    else
		copy_advance_escape(buf, p->value, (*pvalue_spec));
	}
	p = p->next;
	if (sep == '?') sep = '&';
    } while (p != param_list);

    return buf-startbuf;
}


/*
 * URI stuffs
 */
#define IS_SIPS(url)	((url)->vptr==&sips_url_vptr)

static const pj_str_t *pjsip_url_get_scheme( const pjsip_sip_uri* );
static const pj_str_t *pjsips_url_get_scheme( const pjsip_sip_uri* );
static const pj_str_t *pjsip_name_addr_get_scheme( const pjsip_name_addr * );
static void *pjsip_get_uri( pjsip_uri *uri );
static void *pjsip_name_addr_get_uri( pjsip_name_addr *name );

static pj_str_t sip_str = { "sip", 3 };
static pj_str_t sips_str = { "sips", 4 };

static pjsip_name_addr* pjsip_name_addr_clone( pj_pool_t *pool, 
					       const pjsip_name_addr *rhs);
static pj_ssize_t pjsip_name_addr_print(pjsip_uri_context_e context,
					const pjsip_name_addr *name, 
					char *buf, pj_size_t size);
static int pjsip_name_addr_compare(  pjsip_uri_context_e context,
				     const pjsip_name_addr *naddr1,
				     const pjsip_name_addr *naddr2);
static pj_ssize_t pjsip_url_print(  pjsip_uri_context_e context,
				    const pjsip_sip_uri *url, 
				    char *buf, pj_size_t size);
static int pjsip_url_compare( pjsip_uri_context_e context,
			      const pjsip_sip_uri *url1, 
			      const pjsip_sip_uri *url2);
static pjsip_sip_uri* pjsip_url_clone(pj_pool_t *pool, 
				      const pjsip_sip_uri *rhs);

typedef const pj_str_t* (*P_GET_SCHEME)(const void*);
typedef void* 		(*P_GET_URI)(void*);
typedef pj_ssize_t 	(*P_PRINT_URI)(pjsip_uri_context_e,const void *,
				       char*,pj_size_t);
typedef int 		(*P_CMP_URI)(pjsip_uri_context_e, const void*, 
				     const void*);
typedef void* 		(*P_CLONE)(pj_pool_t*, const void*);


static pjsip_uri_vptr sip_url_vptr = 
{
    (P_GET_SCHEME)	&pjsip_url_get_scheme,
    (P_GET_URI)		&pjsip_get_uri,
    (P_PRINT_URI) 	&pjsip_url_print,
    (P_CMP_URI) 	&pjsip_url_compare,
    (P_CLONE) 		&pjsip_url_clone
};

static pjsip_uri_vptr sips_url_vptr = 
{
    (P_GET_SCHEME)	&pjsips_url_get_scheme,
    (P_GET_URI)		&pjsip_get_uri,
    (P_PRINT_URI) 	&pjsip_url_print,
    (P_CMP_URI) 	&pjsip_url_compare,
    (P_CLONE) 		&pjsip_url_clone
};

static pjsip_uri_vptr name_addr_vptr = 
{
    (P_GET_SCHEME)	&pjsip_name_addr_get_scheme,
    (P_GET_URI)		&pjsip_name_addr_get_uri,
    (P_PRINT_URI) 	&pjsip_name_addr_print,
    (P_CMP_URI) 	&pjsip_name_addr_compare,
    (P_CLONE) 		&pjsip_name_addr_clone
};

static const pj_str_t *pjsip_url_get_scheme(const pjsip_sip_uri *url)
{
    PJ_UNUSED_ARG(url);
    return &sip_str;
}

static const pj_str_t *pjsips_url_get_scheme(const pjsip_sip_uri *url)
{
    PJ_UNUSED_ARG(url);
    return &sips_str;
}

static void *pjsip_get_uri( pjsip_uri *uri )
{
    return uri;
}

static void *pjsip_name_addr_get_uri( pjsip_name_addr *name )
{
    return pjsip_uri_get_uri(name->uri);
}

PJ_DEF(void) pjsip_sip_uri_set_secure( pjsip_sip_uri *url, 
				       pj_bool_t secure )
{
    url->vptr = secure ? &sips_url_vptr : &sip_url_vptr;
}

PJ_DEF(void) pjsip_sip_uri_init(pjsip_sip_uri *url, pj_bool_t secure)
{
    pj_bzero(url, sizeof(*url));
    url->ttl_param = -1;
    pjsip_sip_uri_set_secure(url, secure);
    pj_list_init(&url->other_param);
    pj_list_init(&url->header_param);
}

PJ_DEF(pjsip_sip_uri*) pjsip_sip_uri_create( pj_pool_t *pool, 
					     pj_bool_t secure )
{
    pjsip_sip_uri *url = PJ_POOL_ALLOC_T(pool, pjsip_sip_uri);
    pjsip_sip_uri_init(url, secure);
    return url;
}

static pj_ssize_t pjsip_url_print(  pjsip_uri_context_e context,
				    const pjsip_sip_uri *url, 
				    char *buf, pj_size_t size)
{
    int printed;
    char *startbuf = buf;
    char *endbuf = buf+size;
    const pj_str_t *scheme;
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    *buf = '\0';

    /* Print scheme ("sip:" or "sips:") */
    scheme = pjsip_uri_get_scheme(url);
    copy_advance_check(buf, *scheme);
    *buf++ = ':';

    /* Print "user:password@", if any. */
    if (url->user.slen) {
	copy_advance_escape(buf, url->user, pc->pjsip_USER_SPEC);
	if (url->passwd.slen) {
	    *buf++ = ':';
	    copy_advance_escape(buf, url->passwd, pc->pjsip_PASSWD_SPEC);
	}

	*buf++ = '@';
    }

    /* Print host. */
    pj_assert(url->host.slen != 0);
    /* Detect IPv6 IP address */
    if (pj_memchr(url->host.ptr, ':', url->host.slen)) {
	copy_advance_pair_quote_cond(buf, "", 0, url->host, '[', ']');
    } else {
	copy_advance_check(buf, url->host);
    }

    /* Only print port if it is explicitly specified. 
     * Port is not allowed in To and From header, see Table 1 in
     * RFC 3261 Section 19.1.1
     */
    /* Note: ticket #1141 adds run-time setting to allow port number to
     * appear in From/To header. Default is still false.
     */
    if (url->port &&
	(context != PJSIP_URI_IN_FROMTO_HDR ||
	 pjsip_cfg()->endpt.allow_port_in_fromto_hdr))
    {
	if (endbuf - buf < 10)
	    return -1;

	*buf++ = ':';
	printed = pj_utoa(url->port, buf);
	buf += printed;
    }

    /* User param is allowed in all contexes */
    copy_advance_pair_check(buf, ";user=", 6, url->user_param);

    /* Method param is only allowed in external/other context. */
    if (context == PJSIP_URI_IN_OTHER) {
	copy_advance_pair_escape(buf, ";method=", 8, url->method_param, 
				 pc->pjsip_PARAM_CHAR_SPEC);
    }

    /* Transport is not allowed in From/To header. */
    if (context != PJSIP_URI_IN_FROMTO_HDR) {
	copy_advance_pair_escape(buf, ";transport=", 11, url->transport_param,
				 pc->pjsip_PARAM_CHAR_SPEC);
    }

    /* TTL param is not allowed in From, To, Route, and Record-Route header. */
    if (url->ttl_param >= 0 && context != PJSIP_URI_IN_FROMTO_HDR &&
	context != PJSIP_URI_IN_ROUTING_HDR) 
    {
	if (endbuf - buf < 15)
	    return -1;
	pj_memcpy(buf, ";ttl=", 5);
	printed = pj_utoa(url->ttl_param, buf+5);
	buf += printed + 5;
    }

    /* maddr param is not allowed in From and To header. */
    if (context != PJSIP_URI_IN_FROMTO_HDR && url->maddr_param.slen) {
	/* Detect IPv6 IP address */
	if (pj_memchr(url->maddr_param.ptr, ':', url->maddr_param.slen)) {
	    copy_advance_pair_quote_cond(buf, ";maddr=", 7, url->maddr_param,
				         '[', ']');
	} else {
	    copy_advance_pair_escape(buf, ";maddr=", 7, url->maddr_param,
				     pc->pjsip_PARAM_CHAR_SPEC);
	}
    }

    /* lr param is not allowed in From, To, and Contact header. */
    if (url->lr_param && context != PJSIP_URI_IN_FROMTO_HDR &&
	context != PJSIP_URI_IN_CONTACT_HDR) 
    {
	pj_str_t lr = { ";lr", 3 };
	if (endbuf - buf < 3)
	    return -1;
	copy_advance_check(buf, lr);
    }

    /* Other param. */
    printed = pjsip_param_print_on(&url->other_param, buf, endbuf-buf, 
				   &pc->pjsip_PARAM_CHAR_SPEC, 
				   &pc->pjsip_PARAM_CHAR_SPEC, ';');
    if (printed < 0)
	return -1;
    buf += printed;

    /* Header param. 
     * Header param is only allowed in these contexts:
     *	- PJSIP_URI_IN_CONTACT_HDR
     *	- PJSIP_URI_IN_OTHER
     */
    if (context == PJSIP_URI_IN_CONTACT_HDR || context == PJSIP_URI_IN_OTHER) {
	printed = pjsip_param_print_on(&url->header_param, buf, endbuf-buf,
				       &pc->pjsip_HDR_CHAR_SPEC, 
				       &pc->pjsip_HDR_CHAR_SPEC, '?');
	if (printed < 0)
	    return -1;
	buf += printed;
    }

    *buf = '\0';
    return buf-startbuf;
}

static pj_status_t pjsip_url_compare( pjsip_uri_context_e context,
				      const pjsip_sip_uri *url1, 
				      const pjsip_sip_uri *url2)
{
    const pjsip_param *p1;

    /*
     * Compare two SIP URL's according to Section 19.1.4 of RFC 3261.
     */

    /* SIP and SIPS URI are never equivalent. 
     * Note: just compare the vptr to avoid string comparison. 
     *       Pretty neat huh!!
     */
    if (url1->vptr != url2->vptr)
	return PJSIP_ECMPSCHEME;

    /* Comparison of the userinfo of SIP and SIPS URIs is case-sensitive. 
     * This includes userinfo containing passwords or formatted as 
     * telephone-subscribers.
     */
    if (pj_strcmp(&url1->user, &url2->user) != 0)
	return PJSIP_ECMPUSER;
    if (pj_strcmp(&url1->passwd, &url2->passwd) != 0)
	return PJSIP_ECMPPASSWD;
    
    /* Comparison of all other components of the URI is
     * case-insensitive unless explicitly defined otherwise.
     */

    /* The ordering of parameters and header fields is not significant 
     * in comparing SIP and SIPS URIs.
     */

    /* Characters other than those in the reserved set (see RFC 2396 [5])
     * are equivalent to their encoding.
     */

    /* An IP address that is the result of a DNS lookup of a host name 
     * does not match that host name.
     */
    if (pj_stricmp(&url1->host, &url2->host) != 0)
	return PJSIP_ECMPHOST;

    /* A URI omitting any component with a default value will not match a URI
     * explicitly containing that component with its default value. 
     * For instance, a URI omitting the optional port component will not match
     * a URI explicitly declaring port 5060. 
     * The same is true for the transport-parameter, ttl-parameter, 
     * user-parameter, and method components.
     */

    /* Port is not allowed in To and From header.
     */
    if (context != PJSIP_URI_IN_FROMTO_HDR) {
	if (url1->port != url2->port)
	    return PJSIP_ECMPPORT;
    }
    /* Transport is not allowed in From/To header. */
    if (context != PJSIP_URI_IN_FROMTO_HDR) {
	if (pj_stricmp(&url1->transport_param, &url2->transport_param) != 0)
	    return PJSIP_ECMPTRANSPORTPRM;
    }
    /* TTL param is not allowed in From, To, Route, and Record-Route header. */
    if (context != PJSIP_URI_IN_FROMTO_HDR &&
	context != PJSIP_URI_IN_ROUTING_HDR)
    {
	if (url1->ttl_param != url2->ttl_param)
	    return PJSIP_ECMPTTLPARAM;
    }
    /* User param is allowed in all contexes */
    if (pj_stricmp(&url1->user_param, &url2->user_param) != 0)
	return PJSIP_ECMPUSERPARAM;
    /* Method param is only allowed in external/other context. */
    if (context == PJSIP_URI_IN_OTHER) {
	if (pj_stricmp(&url1->method_param, &url2->method_param) != 0)
	    return PJSIP_ECMPMETHODPARAM;
    }
    /* maddr param is not allowed in From and To header. */
    if (context != PJSIP_URI_IN_FROMTO_HDR) {
	if (pj_stricmp(&url1->maddr_param, &url2->maddr_param) != 0)
	    return PJSIP_ECMPMADDRPARAM;
    }

    /* lr parameter is ignored (?) */
    /* lr param is not allowed in From, To, and Contact header. */


    /* All other uri-parameters appearing in only one URI are ignored when 
     * comparing the URIs.
     */
    if (pjsip_param_cmp(&url1->other_param, &url2->other_param, 1)!=0)
	return PJSIP_ECMPOTHERPARAM;

    /* URI header components are never ignored. Any present header component
     * MUST be present in both URIs and match for the URIs to match. 
     * The matching rules are defined for each header field in Section 20.
     */
    p1 = url1->header_param.next;
    while (p1 != &url1->header_param) {
	const pjsip_param *p2;
	p2 = pjsip_param_find(&url2->header_param, &p1->name);
	if (p2) {
	    /* It seems too much to compare two header params according to
	     * the rule of each header. We'll just compare them string to
	     * string..
	     */
	    if (pj_stricmp(&p1->value, &p2->value) != 0)
		return PJSIP_ECMPHEADERPARAM;
	} else {
	    return PJSIP_ECMPHEADERPARAM;
	}
	p1 = p1->next;
    }

    /* Equal!! Pheuww.. */
    return PJ_SUCCESS;
}


PJ_DEF(void) pjsip_sip_uri_assign(pj_pool_t *pool, pjsip_sip_uri *url, 
				  const pjsip_sip_uri *rhs)
{
    pj_strdup( pool, &url->user, &rhs->user);
    pj_strdup( pool, &url->passwd, &rhs->passwd);
    pj_strdup( pool, &url->host, &rhs->host);
    url->port = rhs->port;
    pj_strdup( pool, &url->user_param, &rhs->user_param);
    pj_strdup( pool, &url->method_param, &rhs->method_param);
    pj_strdup( pool, &url->transport_param, &rhs->transport_param);
    url->ttl_param = rhs->ttl_param;
    pj_strdup( pool, &url->maddr_param, &rhs->maddr_param);
    pjsip_param_clone(pool, &url->other_param, &rhs->other_param);
    pjsip_param_clone(pool, &url->header_param, &rhs->header_param);
    url->lr_param = rhs->lr_param;
}

static pjsip_sip_uri* pjsip_url_clone(pj_pool_t *pool, const pjsip_sip_uri *rhs)
{
    pjsip_sip_uri *url = PJ_POOL_ALLOC_T(pool, pjsip_sip_uri);
    if (!url)
	return NULL;

    pjsip_sip_uri_init(url, IS_SIPS(rhs));
    pjsip_sip_uri_assign(pool, url, rhs);
    return url;
}

static const pj_str_t *pjsip_name_addr_get_scheme(const pjsip_name_addr *name)
{
    pj_assert(name->uri != NULL);
    return pjsip_uri_get_scheme(name->uri);
}

PJ_DEF(void) pjsip_name_addr_init(pjsip_name_addr *name)
{
    name->vptr = &name_addr_vptr;
    name->uri = NULL;
    name->display.slen = 0;
    name->display.ptr = NULL;
}

PJ_DEF(pjsip_name_addr*) pjsip_name_addr_create(pj_pool_t *pool)
{
    pjsip_name_addr *name_addr = PJ_POOL_ALLOC_T(pool, pjsip_name_addr);
    pjsip_name_addr_init(name_addr);
    return name_addr;
}

static pj_ssize_t pjsip_name_addr_print(pjsip_uri_context_e context,
					const pjsip_name_addr *name, 
					char *buf, pj_size_t size)
{
    int printed;
    char *startbuf = buf;
    char *endbuf = buf + size;
    pjsip_uri *uri;

    uri = (pjsip_uri*) pjsip_uri_get_uri(name->uri);
    pj_assert(uri != NULL);

    if (context != PJSIP_URI_IN_REQ_URI) {
	if (name->display.slen) {
	    if (endbuf-buf < 8) return -1;
	    *buf++ = '"';
	    copy_advance(buf, name->display);
	    *buf++ = '"';
	    *buf++ = ' ';
	}
	*buf++ = '<';
    }

    printed = pjsip_uri_print(context,uri, buf, size-(buf-startbuf));
    if (printed < 1)
	return -1;
    buf += printed;

    if (context != PJSIP_URI_IN_REQ_URI) {
	*buf++ = '>';
    }

    *buf = '\0';
    return buf-startbuf;
}

PJ_DEF(void) pjsip_name_addr_assign(pj_pool_t *pool, pjsip_name_addr *dst,
				    const pjsip_name_addr *src)
{
    pj_strdup( pool, &dst->display, &src->display);
    dst->uri = (pjsip_uri*) pjsip_uri_clone(pool, src->uri);
}

static pjsip_name_addr* pjsip_name_addr_clone( pj_pool_t *pool, 
					       const pjsip_name_addr *rhs)
{
    pjsip_name_addr *addr = PJ_POOL_ALLOC_T(pool, pjsip_name_addr);
    if (!addr)
	return NULL;

    pjsip_name_addr_init(addr);
    pjsip_name_addr_assign(pool, addr, rhs);
    return addr;
}

static int pjsip_name_addr_compare(  pjsip_uri_context_e context,
				     const pjsip_name_addr *naddr1,
				     const pjsip_name_addr *naddr2)
{
    int d;

    /* Check that naddr2 is also a name_addr */
    if (naddr1->vptr != naddr2->vptr)
	return -1;

    /* I'm not sure whether display name is included in the comparison. */
    if (pj_strcmp(&naddr1->display, &naddr2->display) != 0) {
	return -1;
    }

    pj_assert( naddr1->uri != NULL );
    pj_assert( naddr2->uri != NULL );

    /* Compare name-addr as URL */
    d = pjsip_uri_cmp( context, naddr1->uri, naddr2->uri);
    if (d)
	return d;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

static const pj_str_t *other_uri_get_scheme( const pjsip_other_uri*);
static void *other_uri_get_uri( pjsip_other_uri*);
static pj_ssize_t other_uri_print( pjsip_uri_context_e context,
				   const pjsip_other_uri *url, 
				   char *buf, pj_size_t size);
static int other_uri_cmp( pjsip_uri_context_e context,
			  const pjsip_other_uri *url1, 
			  const pjsip_other_uri *url2);
static pjsip_other_uri* other_uri_clone( pj_pool_t *pool, 
					 const pjsip_other_uri *rhs);

static pjsip_uri_vptr other_uri_vptr = 
{
    (P_GET_SCHEME)  &other_uri_get_scheme,
    (P_GET_URI)	    &other_uri_get_uri,
    (P_PRINT_URI)   &other_uri_print,
    (P_CMP_URI)	    &other_uri_cmp,
    (P_CLONE) 	    &other_uri_clone
};


PJ_DEF(pjsip_other_uri*) pjsip_other_uri_create(pj_pool_t *pool) 
{
    pjsip_other_uri *uri = PJ_POOL_ZALLOC_T(pool, pjsip_other_uri);
    uri->vptr = &other_uri_vptr;
    return uri;
}

static const pj_str_t *other_uri_get_scheme( const pjsip_other_uri *uri )
{
	return &uri->scheme;
}

static void *other_uri_get_uri( pjsip_other_uri *uri )
{
    return uri;
}

static pj_ssize_t other_uri_print(pjsip_uri_context_e context,
				  const pjsip_other_uri *uri, 
				  char *buf, pj_size_t size)
{
    char *startbuf = buf;
    char *endbuf = buf + size;
    
    PJ_UNUSED_ARG(context);
    
    if (uri->scheme.slen + uri->content.slen + 1 > (int)size)
	return -1;

    /* Print scheme. */
    copy_advance(buf, uri->scheme);
    *buf++ = ':';
    
    /* Print content. */
    copy_advance(buf, uri->content);
    
    return (buf - startbuf);
}

static int other_uri_cmp(pjsip_uri_context_e context,
			 const pjsip_other_uri *uri1,
			 const pjsip_other_uri *uri2)
{
    PJ_UNUSED_ARG(context);

    /* Check that uri2 is also an other_uri */
    if (uri1->vptr != uri2->vptr)
	return -1;
    
    /* Scheme must match. */
    if (pj_stricmp(&uri1->scheme, &uri2->scheme) != 0) {
	return PJSIP_ECMPSCHEME;
    }
    
    /* Content must match. */
    if(pj_stricmp(&uri1->content, &uri2->content) != 0) {
	return -1;
    }
    
    /* Equal. */
    return 0;
}

/* Clone *: URI */
static pjsip_other_uri* other_uri_clone(pj_pool_t *pool, 
					const pjsip_other_uri *rhs) 
{
    pjsip_other_uri *uri = pjsip_other_uri_create(pool);
    pj_strdup(pool, &uri->scheme, &rhs->scheme);
    pj_strdup(pool, &uri->content, &rhs->content);

    return uri;
}


/* $Id: sip_uri.h 4404 2013-02-27 14:47:37Z riza $ */
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
#ifndef __PJSIP_SIP_URI_H__
#define __PJSIP_SIP_URI_H__

/**
 * @file sip_uri.h
 * @brief SIP URL Structures and Manipulations
 */

#include <pjsip/sip_types.h>
#include <pjsip/sip_config.h>
#include <pj/list.h>
#include <pjlib-util/scanner.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_URI URI
 * @brief URI types and manipulations.
 * @ingroup PJSIP_MSG
 */

/**
 * @addtogroup PJSIP_URI_PARAM URI Parameter Container
 * @ingroup PJSIP_URI
 * @brief Generic parameter elements container.
 * @{
 */

/**
 * Generic parameter, normally used in other_param or header_param.
 */
typedef struct pjsip_param
{
    PJ_DECL_LIST_MEMBER(struct pjsip_param);	/**< Generic list member.   */
    pj_str_t	    name;			/**< Param/header name.	    */
    pj_str_t	    value;			/**< Param/header value.    */
} pjsip_param;


/**
 * Find the specified parameter name in the list. The name will be compared
 * in case-insensitive comparison.
 *
 * @param param_list	List of parameters to find.
 * @param name		Parameter/header name to find.
 *
 * @return		The parameter if found, or NULL.
 */
PJ_DECL(pjsip_param*) pjsip_param_find( const pjsip_param *param_list,
					const pj_str_t *name );


/**
 * Alias for pjsip_param_find()
 */
PJ_INLINE(pjsip_param*) pjsip_param_cfind(const pjsip_param *param_list,
					  const pj_str_t *name)
{
    return pjsip_param_find(param_list, name);
}

/**
 * Compare two parameter lists.
 *
 * @param param_list1	First parameter list.
 * @param param_list2	Second parameter list.
 * @param ig_nf		If set to 1, do not compare parameters that only
 * 			appear in one of the list.
 *
 * @return		Zero if the parameter list are equal, non-zero
 * 			otherwise.
 */
PJ_DECL(int) pjsip_param_cmp(const pjsip_param *param_list1,
			     const pjsip_param *param_list2,
			     pj_bool_t ig_nf);

/**
 * Duplicate the parameters.
 *
 * @param pool		Pool to allocate memory from.
 * @param dst_list	Destination list.
 * @param src_list	Source list.
 */
PJ_DECL(void) pjsip_param_clone(pj_pool_t *pool, pjsip_param *dst_list,
				const pjsip_param *src_list);

/**
 * Duplicate the parameters.
 *
 * @param pool		Pool to allocate memory from.
 * @param dst_list	Destination list.
 * @param src_list	Source list.
 */
PJ_DECL(void) pjsip_param_shallow_clone(pj_pool_t *pool, 
					pjsip_param *dst_list,
					const pjsip_param *src_list);

/**
 * Print parameters.
 *
 * @param param_list	The parameter list.
 * @param buf		Buffer.
 * @param size		Size of buffer.
 * @param pname_unres	Specification of allowed characters in pname.
 * @param pvalue_unres	Specification of allowed characters in pvalue.
 * @param sep		Separator character (either ';', ',', or '?').
 *			When separator is set to '?', this function will
 *			automatically adjust the separator character to
 *			'&' after the first parameter is printed.
 *
 * @return		The number of bytes printed, or -1 on errr.
 */
PJ_DECL(pj_ssize_t) pjsip_param_print_on(const pjsip_param *param_list,
					 char *buf, pj_size_t size,
					 const pj_cis_t *pname_unres,
					 const pj_cis_t *pvalue_unres,
					 int sep);

/**
 * @}
 */

/**
 * @defgroup PJSIP_URI_GENERIC Generic URI
 * @ingroup PJSIP_URI
 * @brief Generic representation for all types of URI.
 * @{
 */

/**
 * URI context.
 */
typedef enum pjsip_uri_context_e
{
    PJSIP_URI_IN_REQ_URI,	/**< The URI is in Request URI. */
    PJSIP_URI_IN_FROMTO_HDR,	/**< The URI is in From/To header. */
    PJSIP_URI_IN_CONTACT_HDR,	/**< The URI is in Contact header. */
    PJSIP_URI_IN_ROUTING_HDR,	/**< The URI is in Route/Record-Route header. */
    PJSIP_URI_IN_OTHER		/**< Other context (web page, business card, etc.) */
} pjsip_uri_context_e;

/**
 * URI 'virtual' function table.
 * All types of URI in this library (such as sip:, sips:, tel:, and name-addr) 
 * will have pointer to this table as their first struct member. This table
 * provides polimorphic behaviour to the URI.
 */
typedef struct pjsip_uri_vptr
{
    /** 
     * Get URI scheme. 
     * @param uri the URI (self).
     * @return the URI scheme.
     */
    const pj_str_t* (*p_get_scheme)(const void *uri);

    /**
     * Get the URI object contained by this URI, or the URI itself if
     * it doesn't contain another URI.
     * @param uri the URI (self).
     */
    void* (*p_get_uri)(void *uri);

    /**
     * Print URI components to the buffer, following the rule of which 
     * components are allowed for the context.
     * @param context the context where the URI will be placed.
     * @param uri the URI (self).
     * @param buf the buffer.
     * @param size the size of the buffer.
     * @return the length printed.
     */
    pj_ssize_t (*p_print)(pjsip_uri_context_e context,
			  const void *uri, 
		          char *buf, pj_size_t size);

    /** 
     * Compare two URIs according to the context.
     * @param context the context.
     * @param uri1 the first URI (self).
     * @param uri2 the second URI.
     * @return PJ_SUCCESS if equal, or otherwise the error status which
     *		    should point to the mismatch part.
     */
    pj_status_t	(*p_compare)(pjsip_uri_context_e context, 
			     const void *uri1, const void *uri2);

    /** 
     * Clone URI. 
     * @param pool the pool.
     * @param the URI to clone (self).
     * @return new URI.
     */
    void *(*p_clone)(pj_pool_t *pool, const void *uri);

} pjsip_uri_vptr;


/**
 * The declaration of 'base class' for all URI scheme.
 */
struct pjsip_uri
{
    /** All URIs must have URI virtual function table as their first member. */
    pjsip_uri_vptr *vptr;
};

/**
 * This macro checks that the URL is a "sip:" URL.
 * @param url The URL (pointer to)
 * @return non-zero if TRUE.
 */
#define PJSIP_URI_SCHEME_IS_SIP(url)	\
    (pj_stricmp2(pjsip_uri_get_scheme(url), "sip")==0)

/**
 * This macro checks that the URL is a "sips:" URL (not SIP).
 * @param url The URL (pointer to)
 * @return non-zero if TRUE.
 */
#define PJSIP_URI_SCHEME_IS_SIPS(url)	\
    (pj_stricmp2(pjsip_uri_get_scheme(url), "sips")==0)

/**
 * This macro checks that the URL is a "tel:" URL.
 * @param url The URL (pointer to)
 * @return non-zero if TRUE.
 */
#define PJSIP_URI_SCHEME_IS_TEL(url)	\
    (pj_stricmp2(pjsip_uri_get_scheme(url), "tel")==0)


/**
 * Generic function to get the URI scheme.
 * @param uri	    the URI object.
 * @return	    the URI scheme.
 */
PJ_INLINE(const pj_str_t*) pjsip_uri_get_scheme(const void *uri)
{
    return (*((pjsip_uri*)uri)->vptr->p_get_scheme)(uri);
}

/**
 * Generic function to get the URI object contained by this URI, or the URI 
 * itself if it doesn't contain another URI.
 *
 * @param uri	    the URI.
 * @return	    the URI.
 */
PJ_INLINE(void*) pjsip_uri_get_uri(const void *uri)
{
    return (*((pjsip_uri*)uri)->vptr->p_get_uri)((void*)uri);
}

/**
 * Generic function to compare two URIs.
 *
 * @param context   Comparison context.
 * @param uri1	    The first URI.
 * @param uri2	    The second URI.
 * @return	    PJ_SUCCESS if equal, or otherwise the error status which
 *		    should point to the mismatch part.
 */
PJ_INLINE(pj_status_t) pjsip_uri_cmp(pjsip_uri_context_e context, 
				     const void *uri1, const void *uri2)
{
    return (*((const pjsip_uri*)uri1)->vptr->p_compare)(context, uri1, uri2);
}

/**
 * Generic function to print an URI object.
 *
 * @param context   Print context.
 * @param uri	    The URI to print.
 * @param buf	    The buffer.
 * @param size	    Size of the buffer.
 * @return	    Length printed.
 */
PJ_INLINE(int) pjsip_uri_print(pjsip_uri_context_e context,
			       const void *uri,
			       char *buf, pj_size_t size)
{
    return (*((const pjsip_uri*)uri)->vptr->p_print)(context, uri, buf, size);
}

/**
 * Generic function to clone an URI object.
 *
 * @param pool	    Pool.
 * @param uri	    URI to clone.
 * @return	    New URI.
 */
PJ_INLINE(void*) pjsip_uri_clone( pj_pool_t *pool, const void *uri )
{
    return (*((const pjsip_uri*)uri)->vptr->p_clone)(pool, uri);
}



/**
 * @}
 */

/**
 * @defgroup PJSIP_SIP_URI SIP URI Scheme and Name address
 * @ingroup PJSIP_URI
 * @brief SIP URL structure ("sip:" and "sips:")
 * @{
 */


/**
 * SIP and SIPS URL scheme.
 */
typedef struct pjsip_sip_uri
{
    pjsip_uri_vptr *vptr;		/**< Pointer to virtual function table.*/
    pj_str_t	    user;		/**< Optional user part. */
    pj_str_t	    passwd;		/**< Optional password part. */
    pj_str_t	    host;		/**< Host part, always exists. */
    int		    port;		/**< Optional port number, or zero. */
    pj_str_t	    user_param;		/**< Optional user parameter */
    pj_str_t	    method_param;	/**< Optional method parameter. */
    pj_str_t	    transport_param;	/**< Optional transport parameter. */
    int		    ttl_param;		/**< Optional TTL param, or -1. */
    int		    lr_param;		/**< Optional loose routing param, or zero */
    pj_str_t	    maddr_param;	/**< Optional maddr param */
    pjsip_param	    other_param;	/**< Other parameters grouped together. */
    pjsip_param	    header_param;	/**< Optional header parameter. */
} pjsip_sip_uri;


/**
 * SIP name-addr, which typically appear in From, To, and Contact header.
 * The SIP name-addr contains a generic URI and a display name.
 */
typedef struct pjsip_name_addr
{
    /** Pointer to virtual function table. */
    pjsip_uri_vptr  *vptr;

    /** Optional display name. */
    pj_str_t	     display;

    /** URI part. */
    pjsip_uri	    *uri;

} pjsip_name_addr;


/**
 * Create new SIP URL and initialize all fields with zero or NULL.
 * @param pool	    The pool.
 * @param secure    Flag to indicate whether secure transport should be used.
 * @return SIP URL.
 */
PJ_DECL(pjsip_sip_uri*) pjsip_sip_uri_create( pj_pool_t *pool, 
					      pj_bool_t secure );

/**
 * Change the SIP URI scheme to sip or sips based on the secure flag.
 * This would not change anything except the scheme.
 * @param uri	    The URI
 * @param secure    Non-zero if sips is wanted.
 */
PJ_DECL(void) pjsip_sip_uri_set_secure( pjsip_sip_uri *uri, 
				        pj_bool_t secure );

/**
 * Initialize SIP URL (all fields are set to NULL or zero).
 * @param url	    The URL.
 * @param secure    Create sips URI?
 */
PJ_DECL(void)  pjsip_sip_uri_init(pjsip_sip_uri *url, pj_bool_t secure);

/**
 * Perform full assignment to the SIP URL.
 * @param pool	    The pool.
 * @param url	    Destination URL.
 * @param rhs	    The source URL.
 */
PJ_DECL(void)  pjsip_sip_uri_assign(pj_pool_t *pool, pjsip_sip_uri *url, 
				    const pjsip_sip_uri *rhs);

/**
 * Create new instance of name address and initialize all fields with zero or
 * NULL.
 * @param pool	    The pool.
 * @return	    New SIP name address.
 */
PJ_DECL(pjsip_name_addr*) pjsip_name_addr_create(pj_pool_t *pool);

/**
 * Initialize with default value.
 * @param name_addr The name address.
 */
PJ_DECL(void) pjsip_name_addr_init(pjsip_name_addr *name_addr);

/**
 * Perform full assignment to the name address.
 * @param pool	    The pool.
 * @param addr	    The destination name address.
 * @param rhs	    The source name address.
 */
PJ_DECL(void)  pjsip_name_addr_assign(pj_pool_t *pool, 
				      pjsip_name_addr *addr, 
				      const pjsip_name_addr *rhs);

/**
 * @}
 */

/**
 * @defgroup PJSIP_OTHER_URI Other URI schemes
 * @ingroup PJSIP_URI
 * @brief Container for non SIP/tel URI scheme (e.g. "http:", "mailto:")
 * @{
 */

/**
 * Generic URI container for non SIP/tel URI scheme.
 */
typedef struct pjsip_other_uri 
{
    pjsip_uri_vptr *vptr;	/**< Pointer to virtual function table.	*/
    pj_str_t scheme;		/**< The URI scheme (e.g. "mailto")	*/
    pj_str_t content;		/**< The whole URI content		*/
} pjsip_other_uri;


/**
 * Create a generic URI object.
 *
 * @param pool	    The pool to allocate memory from.
 *
 * @return	    The URI instance.
 */
PJ_DECL(pjsip_other_uri*) pjsip_other_uri_create(pj_pool_t *pool);


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_URL_H__ */


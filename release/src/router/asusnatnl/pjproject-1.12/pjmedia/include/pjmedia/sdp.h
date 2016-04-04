/* $Id: sdp.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_SDP_H__
#define __PJMEDIA_SDP_H__

/**
 * @file sdp.h
 * @brief SDP header file.
 */
#include <pjmedia/types.h>


/**
 * @defgroup PJMEDIA_SDP SDP Parsing and Data Structure
 * @ingroup PJMEDIA_SESSION
 * @brief SDP data structure representation and parsing
 * @{
 *
 * The basic SDP session descriptor and elements are described in header
 * file <b><pjmedia/sdp.h></b>. This file contains declaration for
 * SDP session descriptor and SDP media descriptor, along with their
 * attributes. This file also declares functions to parse SDP message.
 */


PJ_BEGIN_DECL

/**
 * The PJMEDIA_MAX_SDP_FMT macro defines maximum format in a media line.
 */
#ifndef PJMEDIA_MAX_SDP_FMT
#   define PJMEDIA_MAX_SDP_FMT		32
#endif

/**
 * The PJMEDIA_MAX_SDP_BANDW macro defines maximum bandwidth information
 * lines in a media line.
 */
#ifndef PJMEDIA_MAX_SDP_BANDW
#   define PJMEDIA_MAX_SDP_BANDW	4
#endif

/**
 * The PJMEDIA_MAX_SDP_ATTR macro defines maximum SDP attributes in media and
 * session descriptor.
 */
#ifndef PJMEDIA_MAX_SDP_ATTR
#   define PJMEDIA_MAX_SDP_ATTR		(PJMEDIA_MAX_SDP_FMT*2 + 4)
#endif

/**
 * The PJMEDIA_MAX_SDP_MEDIA macro defines maximum SDP media lines in a
 * SDP session descriptor.
 */
#ifndef PJMEDIA_MAX_SDP_MEDIA
#   define PJMEDIA_MAX_SDP_MEDIA	16
#endif


/* **************************************************************************
 * SDP ATTRIBUTES
 ***************************************************************************
 */

/** 
 * Generic representation of attribute.
 */
struct pjmedia_sdp_attr
{
    pj_str_t		name;	    /**< Attribute name.    */
    pj_str_t		value;	    /**< Attribute value.   */
};

/**
 * @see pjmedia_sdp_attr
 */
typedef struct pjmedia_sdp_attr pjmedia_sdp_attr;


/**
 * Create SDP attribute.
 *
 * @param pool		Pool to create the attribute.
 * @param name		Attribute name.
 * @param value		Optional attribute value.
 *
 * @return		The new SDP attribute.
 */
PJ_DECL(pjmedia_sdp_attr*) pjmedia_sdp_attr_create(pj_pool_t *pool,
						   const char *name,
						   const pj_str_t *value);

/** 
 * Clone attribute 
 *
 * @param pool		Pool to be used.
 * @param attr		The attribute to clone.
 *
 * @return		New attribute as cloned from the attribute.
 */
PJ_DECL(pjmedia_sdp_attr*) pjmedia_sdp_attr_clone(pj_pool_t *pool, 
						  const pjmedia_sdp_attr*attr);

/** 
 * Find the first attribute with the specified type.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param name		Attribute name to find.
 * @param fmt		Optional string to indicate which payload format
 *			to find for \a rtpmap and \a fmt attributes. For other
 *			types of attributes, the value should be NULL.
 *
 * @return		The specified attribute, or NULL if it can't be found.
 *
 * @see pjmedia_sdp_attr_find2, pjmedia_sdp_media_find_attr, 
 *	pjmedia_sdp_media_find_attr2
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_attr_find(unsigned count, 
		      pjmedia_sdp_attr *const attr_array[],
		      const pj_str_t *name, const pj_str_t *fmt);

/** 
 * Find the first attribute with the specified type.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param name		Attribute name to find.
 * @param fmt		Optional string to indicate which payload format
 *			to find for \a rtpmap and \a fmt attributes. For other
 *			types of attributes, the value should be NULL.
 *
 * @return		The specified attribute, or NULL if it can't be found.
 *
 * @see pjmedia_sdp_attr_find, pjmedia_sdp_media_find_attr,
 *	pjmedia_sdp_media_find_attr2
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_attr_find2(unsigned count, 
		       pjmedia_sdp_attr *const attr_array[],
		       const char *name, const pj_str_t *fmt);

/**
 * Add a new attribute to array of attributes.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param attr		The attribute to add.
 *
 * @return		PJ_SUCCESS or the error code.
 *
 * @see pjmedia_sdp_media_add_attr
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_add(unsigned *count,
					  pjmedia_sdp_attr *attr_array[],
					  pjmedia_sdp_attr *attr);

/**
 * Remove all attributes with the specified name in array of attributes.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param name		Attribute name to find.
 *
 * @return		Number of attributes removed.
 *
 * @see pjmedia_sdp_media_remove_all_attr
 */
PJ_DECL(unsigned) pjmedia_sdp_attr_remove_all(unsigned *count,
					      pjmedia_sdp_attr *attr_array[],
					      const char *name);


/**
 * Remove the specified attribute from the attribute array.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param attr		The attribute instance to remove.
 *
 * @return		PJ_SUCCESS when attribute has been removed, or 
 *			PJ_ENOTFOUND when the attribute can not be found.
 *
 * @see pjmedia_sdp_media_remove_attr
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_remove(unsigned *count,
					     pjmedia_sdp_attr *attr_array[],
					     pjmedia_sdp_attr *attr);


/**
 * This structure declares SDP \a rtpmap attribute.
 */
struct pjmedia_sdp_rtpmap
{
    pj_str_t		pt;	    /**< Payload type.	    */
    pj_str_t		enc_name;   /**< Encoding name.	    */
    unsigned		clock_rate; /**< Clock rate.	    */
    pj_str_t		param;	    /**< Parameter.	    */
};

/**
 * @see pjmedia_sdp_rtpmap
 */
typedef struct pjmedia_sdp_rtpmap pjmedia_sdp_rtpmap;


/**
 * Convert generic attribute to SDP \a rtpmap. This function allocates
 * a new attribute and call #pjmedia_sdp_attr_get_rtpmap().
 *
 * @param inst_id   The instance id of pjsua.
 * @param pool		Pool used to create the rtpmap attribute.
 * @param attr		Generic attribute to be converted to rtpmap, which
 *			name must be "rtpmap".
 * @param p_rtpmap	Pointer to receive SDP rtpmap attribute.
 *
 * @return		PJ_SUCCESS if the attribute can be successfully
 *			converted to \a rtpmap type.
 *
 * @see pjmedia_sdp_attr_get_rtpmap
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_to_rtpmap(int inst_id, 
						pj_pool_t *pool,
						const pjmedia_sdp_attr *attr,
						pjmedia_sdp_rtpmap **p_rtpmap);


/**
 * Get the rtpmap representation of the same SDP attribute.
 *
 * @param inst_id   The instance id of pjsua.
 * @param attr		Generic attribute to be converted to rtpmap, which
 *			name must be "rtpmap".
 * @param rtpmap	SDP \a rtpmap attribute to be initialized.
 *
 * @return		PJ_SUCCESS if the attribute can be successfully
 *			converted to \a rtpmap attribute.
 *
 * @see pjmedia_sdp_attr_to_rtpmap
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_get_rtpmap(int inst_id, 
						 const pjmedia_sdp_attr *attr,
						 pjmedia_sdp_rtpmap *rtpmap);


/**
 * Convert \a rtpmap attribute to generic attribute.
 *
 * @param pool		Pool to be used.
 * @param rtpmap	The \a rtpmap attribute.
 * @param p_attr	Pointer to receive the generic SDP attribute.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_rtpmap_to_attr( pj_pool_t *pool,
			    const pjmedia_sdp_rtpmap *rtpmap,
			    pjmedia_sdp_attr **p_attr);


/**
 * This structure describes SDP \a fmtp attribute.
 */
struct pjmedia_sdp_fmtp
{
    pj_str_t		fmt;	    /**< Format type.		    */
    pj_str_t		fmt_param;  /**< Format specific parameter. */
};


/**
 * @see pjmedia_sdp_fmtp
 */
typedef struct pjmedia_sdp_fmtp pjmedia_sdp_fmtp;



/**
 * Get the fmtp representation of the same SDP attribute.
 *
 * @param attr		Generic attribute to be converted to fmtp, which
 *			name must be "fmtp".
 * @param fmtp		SDP fmtp attribute to be initialized.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_get_fmtp(const pjmedia_sdp_attr *attr,
					       pjmedia_sdp_fmtp *fmtp);


/**
 * This structure describes SDP \a rtcp attribute.
 */
typedef struct pjmedia_sdp_rtcp_attr
{
    unsigned	port;	    /**< RTCP port number.	    */
    pj_str_t	net_type;   /**< Optional network type.	    */
    pj_str_t	addr_type;  /**< Optional address type.	    */
    pj_str_t	addr;	    /**< Optional address.	    */
} pjmedia_sdp_rtcp_attr;


/**
 * Parse a generic SDP attribute to get SDP rtcp attribute values.
 *
 * @param inst_id   The instance id of pjsua.
 * @param attr		Generic attribute to be converted to rtcp, which
 *			name must be "rtcp".
 * @param rtcp		SDP rtcp attribute to be initialized.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_get_rtcp(int inst_id, 
						   const pjmedia_sdp_attr *attr,
					       pjmedia_sdp_rtcp_attr *rtcp);


/**
 * Create a=rtcp attribute.
 *
 * @param pool		Pool to create the attribute.
 * @param a		Socket address.
 *
 * @return		SDP RTCP attribute.
 */
PJ_DECL(pjmedia_sdp_attr*) pjmedia_sdp_attr_create_rtcp(pj_pool_t *pool,
							const pj_sockaddr *a);


/* **************************************************************************
 * SDP CONNECTION INFO
 ****************************************************************************
 */

/**
 * This structure describes SDP connection info ("c=" line). 
 */
struct pjmedia_sdp_conn
{
    pj_str_t	net_type;	/**< Network type ("IN").		*/
    pj_str_t	addr_type;	/**< Address type ("IP4", "IP6").	*/
    pj_str_t	addr;		/**< The address.			*/
};


/**
 * @see pjmedia_sdp_conn
 */
typedef struct pjmedia_sdp_conn pjmedia_sdp_conn;


/** 
 * Clone connection info. 
 * 
 * @param pool	    Pool to allocate memory for the new connection info.
 * @param rhs	    The connection into to clone.
 *
 * @return	    The new connection info.
 */
PJ_DECL(pjmedia_sdp_conn*) pjmedia_sdp_conn_clone(pj_pool_t *pool, 
						  const pjmedia_sdp_conn *rhs);



/* **************************************************************************
 * SDP BANDWIDTH INFO
 ****************************************************************************
 */

/**
 * This structure describes SDP bandwidth info ("b=" line). 
 */
typedef struct pjmedia_sdp_bandw
{
    pj_str_t	modifier;	/**< Bandwidth modifier.		*/
    pj_uint32_t	value;	        /**< Bandwidth value.	                */
} pjmedia_sdp_bandw;


/** 
 * Clone bandwidth info. 
 * 
 * @param pool	    Pool to allocate memory for the new bandwidth info.
 * @param rhs	    The bandwidth into to clone.
 *
 * @return	    The new bandwidth info.
 */
PJ_DECL(pjmedia_sdp_bandw*)
pjmedia_sdp_bandw_clone(pj_pool_t *pool, const pjmedia_sdp_bandw *rhs);



/* **************************************************************************
 * SDP MEDIA INFO/LINE
 ****************************************************************************
 */

/**
 * This structure describes SDP media descriptor. A SDP media descriptor
 * starts with "m=" line and contains the media attributes and optional
 * connection line.
 */
struct pjmedia_sdp_media
{
    /** Media descriptor line ("m=" line) */
    struct
    {
	pj_str_t    media;		/**< Media type ("audio", "video", "application")  dean : application is WebRTC data channel*/ 
	pj_uint16_t port;		/**< Port number.		    */
	unsigned    port_count;		/**< Port count, used only when >2  */
	pj_str_t    transport;		/**< Transport ("RTP/AVP")	    */
	unsigned    fmt_count;		/**< Number of formats.		    */
	pj_str_t    fmt[PJMEDIA_MAX_SDP_FMT];       /**< Media formats.	    */
    } desc;

    pjmedia_sdp_conn   *conn;		/**< Optional connection info.	    */
    unsigned	        bandw_count;	/**< Number of bandwidth info.	    */
    pjmedia_sdp_bandw  *bandw[PJMEDIA_MAX_SDP_BANDW]; /**< Bandwidth info.  */
    unsigned	        attr_count;	/**< Number of attributes.	    */
    pjmedia_sdp_attr   *attr[PJMEDIA_MAX_SDP_ATTR];   /**< Attributes.	    */

};


/**
 * @see pjmedia_sdp_media
 */
typedef struct pjmedia_sdp_media pjmedia_sdp_media;


/** 
 * Clone SDP media description. 
 *
 * @param pool	    Pool to allocate memory for the new media description.
 * @param rhs	    The media descriptin to clone.
 *
 * @return	    New media description.
 */
PJ_DECL(pjmedia_sdp_media*) 
pjmedia_sdp_media_clone( pj_pool_t *pool, 
			 const pjmedia_sdp_media *rhs);

/**
 * Find the first occurence of the specified attribute name in the media 
 * descriptor. Optionally the format may be specified.
 *
 * @param m		The SDP media description.
 * @param name		Attribute name to find.
 * @param fmt		Optional payload type to match in the
 *			attribute list, when the attribute is \a rtpmap
 *			or \a fmtp. For other types of SDP attributes, this
 *			value should be NULL.
 *
 * @return		The first instance of the specified attribute or NULL.
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_media_find_attr(const pjmedia_sdp_media *m,
			    const pj_str_t *name, const pj_str_t *fmt);


/**
 * Find the first occurence of the specified attribute name in the SDP media 
 * descriptor. Optionally the format may be specified.
 *
 * @param m		The SDP media description.
 * @param name		Attribute name to find.
 * @param fmt		Optional payload type to match in the
 *			attribute list, when the attribute is \a rtpmap
 *			or \a fmtp. For other types of SDP attributes, this
 *			value should be NULL.
 *
 * @return		The first instance of the specified attribute or NULL.
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_media_find_attr2(const pjmedia_sdp_media *m,
			     const char *name, const pj_str_t *fmt);

/**
 * Add new attribute to the media descriptor.
 *
 * @param m		The SDP media description.
 * @param attr		Attribute to add.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_media_add_attr(pjmedia_sdp_media *m,
						pjmedia_sdp_attr *attr);

/**
 * Remove all attributes with the specified name from the SDP media
 * descriptor.
 *
 * @param m		The SDP media description.
 * @param name		Attribute name to remove.
 *
 * @return		The number of attributes removed.
 */
PJ_DECL(unsigned) 
pjmedia_sdp_media_remove_all_attr(pjmedia_sdp_media *m,
				  const char *name);


/**
 * Remove the occurence of the specified attribute from the SDP media
 * descriptor.
 *
 * @param m		The SDP media descriptor.
 * @param attr		The attribute to find and remove.
 *
 * @return		PJ_SUCCESS if the attribute can be found and has
 *			been removed from the array.
 */
PJ_DECL(pj_status_t)
pjmedia_sdp_media_remove_attr(pjmedia_sdp_media *m,
			      pjmedia_sdp_attr *attr);


/**
 * Compare two SDP media for equality.
 *
 * @param inst_id   The instance id of pjsua.
 * @param sd1	    The first SDP media to compare.
 * @param sd2	    The second SDP media to compare.
 * @param option    Comparison option, which should be zero for now.
 *
 * @return	    PJ_SUCCESS when both SDP medias are equal, or the
 *		    appropriate status code describing which part of
 *		    the descriptors that are not equal.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_media_cmp(int inst_id, 
					   const pjmedia_sdp_media *sd1,
					   const pjmedia_sdp_media *sd2,
					   unsigned option);


/**
 * Compare two media transports for compatibility.
 *
 * @param t1	    The first media transport to compare.
 * @param t2	    The second media transport to compare.
 *
 * @return	    PJ_SUCCESS when both media transports are compatible,
 *		    otherwise returns PJMEDIA_SDP_ETPORTNOTEQUAL.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_transport_cmp(const pj_str_t *t1,
					       const pj_str_t *t2);


/**
 * Deactivate SDP media.
 *
 * @param pool	    Memory pool to allocate memory from.
 * @param m	    The SDP media to deactivate.
 *
 * @return	    PJ_SUCCESS when SDP media successfully deactivated,
 *		    otherwise appropriate status code returned.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_media_deactivate(pj_pool_t *pool,
						  pjmedia_sdp_media *m);


/**
 * Clone SDP media description and deactivate the new SDP media.
 *
 * @param pool	    Memory pool to allocate memory for the clone.
 * @param rhs	    The SDP media to clone.
 *
 * @return	    New media descrption with deactivated indication.
 */
PJ_DECL(pjmedia_sdp_media*) pjmedia_sdp_media_clone_deactivate(
						pj_pool_t *pool,
						const pjmedia_sdp_media *rhs);


/* **************************************************************************
 * SDP SESSION DESCRIPTION
 ****************************************************************************
 */


/**
 * This structure describes SDP session description. A SDP session descriptor
 * contains complete information about a session, and normally is exchanged
 * with remote media peer using signaling protocol such as SIP.
 */
struct pjmedia_sdp_session
{
    /** Session origin (o= line) */
    struct
    {
	pj_str_t    user;	    /**< User 				*/
	pj_uint32_t id;		    /**< Session ID			*/
	pj_uint32_t version;	    /**< Session version		*/
	pj_str_t    net_type;	    /**< Network type ("IN")		*/
	pj_str_t    addr_type;	    /**< Address type ("IP4", "IP6")	*/
	pj_str_t    addr;	    /**< The address.			*/
    } origin;

    pj_str_t	     name;	    /**< Subject line (s=)		*/
    pjmedia_sdp_conn *conn;	    /**< Connection line (c=)		*/
    
    /** Session time (t= line)	*/
    struct
    {
	pj_uint32_t start;	    /**< Start time.			*/
	pj_uint32_t stop;	    /**< Stop time.			*/
    } time;

    unsigned	       attr_count;		/**< Number of attributes.  */
    pjmedia_sdp_attr  *attr[PJMEDIA_MAX_SDP_ATTR]; /**< Attributes array.   */

    unsigned	       media_count;		/**< Number of media.	    */
    pjmedia_sdp_media *media[PJMEDIA_MAX_SDP_MEDIA];	/**< Media array.   */

	int disable_compress;   /*DEAN 0: do bzip2 compress, 1: plain text*/

	pj_str_t           inv_cid; // INVITE CALL-ID for turn authenticate.
};

/**
 * @see pjmedia_sdp_session
 */
typedef struct pjmedia_sdp_session pjmedia_sdp_session;



/**
 * Parse SDP message.
 *
 * @param inst_id   The instance id of pjsua.
 * @param pool	    The pool to allocate SDP session description.
 * @param buf	    The message buffer.
 * @param len	    The length of the message.
 * @param p_sdp	    Pointer to receive the SDP session descriptor.
 *
 * @return	    PJ_SUCCESS if message was successfully parsed into
 *		    SDP session descriptor.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_parse( int inst_id,
					pj_pool_t *pool,
				    char **buf, pj_size_t *len, 
					pjmedia_sdp_session **p_sdp );

/**
 * Print SDP description to a buffer.
 *
 * @param sdp	    The SDP session description.
 * @param buf	    The buffer.
 * @param size	    The buffer length.
 *
 * @return	    the length printed, or -1 if the buffer is too
 *		    short.
 */
PJ_DECL(int) pjmedia_sdp_print( const pjmedia_sdp_session *sdp, 
				char *buf, pj_size_t size);


/**
 * Perform semantic validation for the specified SDP session descriptor.
 * This function perform validation beyond just syntactic verification,
 * such as to verify the value of network type and address type, check
 * the connection line, and verify that \a rtpmap attribute is present
 * when dynamic payload type is used.
 *
 * @param sdp	    The SDP session descriptor to validate.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_validate(const pjmedia_sdp_session *sdp);


/**
 * Clone SDP session descriptor.
 *
 * @param pool	    The pool used to clone the session.
 * @param sdp	    The SDP session to clone.
 *
 * @return	    New SDP session.
 */
PJ_DECL(pjmedia_sdp_session*) 
pjmedia_sdp_session_clone( pj_pool_t *pool,
			   const pjmedia_sdp_session *sdp);


/**
 * Compare two SDP session for equality.
 *
 * @param inst_id   The instance id of pjsua.
 * @param sd1	    The first SDP session to compare.
 * @param sd2	    The second SDP session to compare.
 * @param option    Must be zero for now.
 *
 * @return	    PJ_SUCCESS when both SDPs are equal, or otherwise
 *		    the status code indicates which part of the session
 *		    descriptors are not equal.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_session_cmp(int inst_id, 
						 const pjmedia_sdp_session *sd1,
					     const pjmedia_sdp_session *sd2,
					     unsigned option);


/**
 * Add new attribute to the session descriptor.
 *
 * @param s		The SDP session description.
 * @param attr		Attribute to add.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_session_add_attr(pjmedia_sdp_session *s,
						  pjmedia_sdp_attr *attr);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_SDP_H__ */


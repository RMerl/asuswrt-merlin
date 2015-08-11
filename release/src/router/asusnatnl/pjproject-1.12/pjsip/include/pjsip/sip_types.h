/* $Id: sip_types.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_TYPES_H__
#define __PJSIP_SIP_TYPES_H__


/*
 * My note: Doxygen PJSIP and PJSIP_CORE group is declared in
 *          sip_config.h
 */

/**
 * @file sip_types.h
 * @brief Basic PJSIP types.
 */

#include <pjsip/sip_config.h>
#include <pj/types.h>

/**
 * @addtogroup PJSIP_BASE
 */

/* @defgroup PJSIP_TYPES Basic Data Types
 * @ingroup PJSIP_BASE
 * @brief Basic data types.
 * @{
 */



/**
 * Forward declaration for SIP transport.
 */
typedef struct pjsip_transport pjsip_transport;

/**
 * Forward declaration for transport manager.
 */
typedef struct pjsip_tpmgr pjsip_tpmgr;

/**
 * Transport types.
 */
typedef enum pjsip_transport_type_e
{
    /** Unspecified. */
    PJSIP_TRANSPORT_UNSPECIFIED,

    /** UDP. */
    PJSIP_TRANSPORT_UDP,

    /** TCP. */
    PJSIP_TRANSPORT_TCP,

    /** TLS. */
    PJSIP_TRANSPORT_TLS,

    /** SCTP. */
    PJSIP_TRANSPORT_SCTP,

    /** Loopback (stream, reliable) */
    PJSIP_TRANSPORT_LOOP,

    /** Loopback (datagram, unreliable) */
    PJSIP_TRANSPORT_LOOP_DGRAM,

    /** Start of user defined transport */
    PJSIP_TRANSPORT_START_OTHER,

    /** Start of IPv6 transports */
    PJSIP_TRANSPORT_IPV6    = 128,

    /** UDP over IPv6 */
    PJSIP_TRANSPORT_UDP6 = PJSIP_TRANSPORT_UDP + PJSIP_TRANSPORT_IPV6,

    /** TCP over IPv6 */
    PJSIP_TRANSPORT_TCP6 = PJSIP_TRANSPORT_TCP + PJSIP_TRANSPORT_IPV6

} pjsip_transport_type_e;


/**
 * Forward declaration for endpoint (sip_endpoint.h).
 */
typedef struct pjsip_endpoint pjsip_endpoint;

/**
 * Forward declaration for transactions (sip_transaction.h).
 */
typedef struct pjsip_transaction pjsip_transaction;

/**
 * Forward declaration for events (sip_event.h).
 */
typedef struct pjsip_event pjsip_event;

/**
 * Forward declaration for transmit data/buffer (sip_transport.h).
 */
typedef struct pjsip_tx_data pjsip_tx_data;

/**
 * Forward declaration for receive data/buffer (sip_transport.h).
 */
typedef struct pjsip_rx_data pjsip_rx_data;

/**
 * Forward declaration for message (sip_msg.h).
 */
typedef struct pjsip_msg pjsip_msg;

/**
 * Forward declaration for message body (sip_msg.h).
 */
typedef struct pjsip_msg_body pjsip_msg_body;

/**
 * Forward declaration for header field (sip_msg.h).
 */
typedef struct pjsip_hdr pjsip_hdr;

/**
 * Forward declaration for URI (sip_uri.h).
 */
typedef struct pjsip_uri pjsip_uri;

/**
 * Forward declaration for SIP method (sip_msg.h)
 */
typedef struct pjsip_method pjsip_method;

/**
 * Opaque data type for the resolver engine (sip_resolve.h).
 */
typedef struct pjsip_resolver_t pjsip_resolver_t;

/**
 * Forward declaration for credential.
 */
typedef struct pjsip_cred_info pjsip_cred_info;


/**
 * Forward declaration for module (sip_module.h).
 */
typedef struct pjsip_module pjsip_module;


/** 
 * Forward declaration for user agent type (sip_ua_layer.h). 
 */
typedef pjsip_module pjsip_user_agent;

/**
 * Forward declaration for dialog (sip_dialog.h).
 */
typedef struct pjsip_dialog pjsip_dialog;

/**
 * Transaction role.
 */
typedef enum pjsip_role_e
{
    PJSIP_ROLE_UAC,	/**< Role is UAC. */
    PJSIP_ROLE_UAS,	/**< Role is UAS. */

    /* Alias: */

    PJSIP_UAC_ROLE = PJSIP_ROLE_UAC,	/**< Role is UAC. */
    PJSIP_UAS_ROLE = PJSIP_ROLE_UAS	/**< Role is UAS. */

} pjsip_role_e;


/**
 * General purpose buffer.
 */
typedef struct pjsip_buffer
{
    /** The start of the buffer. */
    char *start;

    /** Pointer to current end of the buffer, which also indicates the position
        of subsequent buffer write.
     */
    char *cur;

    /** The absolute end of the buffer. */
    char *end;

} pjsip_buffer;


/**
 * General host:port pair, used for example as Via sent-by.
 */
typedef struct pjsip_host_port
{
    pj_str_t host;	/**< Host part or IP address. */
    int	     port;	/**< Port number. */
} pjsip_host_port;

/**
 * Host information.
 */
typedef struct pjsip_host_info
{
    unsigned		    flag;   /**< Flags of pjsip_transport_flags_e. */
    pjsip_transport_type_e  type;   /**< Transport type. */
    pjsip_host_port	    addr;   /**< Address information. */
} pjsip_host_info;


/**
 * Convert exception ID into pj_status_t status.
 *
 * @param exception_id  Exception Id.
 *
 * @return              Error code for the specified exception Id.
 */
PJ_DECL(pj_status_t) pjsip_exception_to_status(int exception_id);

/**
 * Return standard pj_status_t status from current exception.
 */
#define PJSIP_RETURN_EXCEPTION() pjsip_exception_to_status(PJ_GET_EXCEPTION())

/**
 * Attributes to inform that the function may throw exceptions.
 */
#define PJSIP_THROW_SPEC(list)


/**
 * @}
 */

#endif	/* __PJSIP_SIP_TYPES_H__ */


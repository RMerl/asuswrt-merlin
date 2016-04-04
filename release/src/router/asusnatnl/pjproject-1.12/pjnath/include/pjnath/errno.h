/* $Id: errno.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_ERRNO_H__
#define __PJNATH_ERRNO_H__

/**
 * @file errno.h
 * @brief PJNATH specific error codes
 */

#include <pj/errno.h>

/**
 * @defgroup PJNATH_ERROR NAT Helper Library Error Codes
 * @brief PJNATH specific error code constants
 * @ingroup PJNATH_STUN_BASE
 * @{
 */

/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 * This value is 370000.
 */
#define PJNATH_ERRNO_START    (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*4)


/************************************************************
 * STUN MESSAGING ERRORS
 ***********************************************************/

/**
 * Map STUN error code (300-699) into pj_status_t error space.
 */
#define PJ_STATUS_FROM_STUN_CODE(code)	(PJNATH_ERRNO_START+code)

/**
 * @hideinitializer
 * Invalid STUN message
 */
#define PJNATH_EINSTUNMSG	    (PJNATH_ERRNO_START+1)  /* 370001 */
/**
 * @hideinitializer
 * Invalid STUN message length.
 */
#define PJNATH_EINSTUNMSGLEN	    (PJNATH_ERRNO_START+2)  /* 370002 */
/**
 * @hideinitializer
 * Invalid or unexpected STUN message type
 */
#define	PJNATH_EINSTUNMSGTYPE	    (PJNATH_ERRNO_START+3)  /* 370003 */
/**
 * @hideinitializer
 * STUN transaction has timed out
 */
#define PJNATH_ESTUNTIMEDOUT	    (PJNATH_ERRNO_START+4)  /* 370004 */
/**
 * @hideinitializer
 * Invalid DTLS record.
 */
#define PJNATH_EINDTLSRECORD	    (PJNATH_ERRNO_START+5)  /* 370005 */


/**
 * @hideinitializer
 * Too many STUN attributes.
 */
#define PJNATH_ESTUNTOOMANYATTR	    (PJNATH_ERRNO_START+21) /* 370021 */
/**
 * @hideinitializer
 * Invalid STUN attribute length.
 */
#define PJNATH_ESTUNINATTRLEN	    (PJNATH_ERRNO_START+22) /* 370022 */
/**
 * @hideinitializer
 * Found duplicate STUN attribute.
 */
#define PJNATH_ESTUNDUPATTR	    (PJNATH_ERRNO_START+23) /* 370023 */

/**
 * @hideinitializer
 * STUN FINGERPRINT verification failed
 */
#define PJNATH_ESTUNFINGERPRINT	    (PJNATH_ERRNO_START+30) /* 370030 */
/**
 * @hideinitializer
 * Invalid STUN attribute after MESSAGE-INTEGRITY.
 */
#define PJNATH_ESTUNMSGINTPOS	    (PJNATH_ERRNO_START+31) /* 370031 */
/**
 * @hideinitializer
 * Invalid STUN attribute after FINGERPRINT.
 */
#define PJNATH_ESTUNFINGERPOS	    (PJNATH_ERRNO_START+33) /* 370033 */


/**
 * @hideinitializer
 * STUN (XOR-)MAPPED-ADDRESS attribute not found
 */
#define PJNATH_ESTUNNOMAPPEDADDR    (PJNATH_ERRNO_START+40) /* 370040 */
/**
 * @hideinitializer
 * STUN IPv6 attribute not supported
 */
#define PJNATH_ESTUNIPV6NOTSUPP	    (PJNATH_ERRNO_START+41) /* 370041 */
/**
 * @hideinitializer
 * Invalid address family value in STUN message.
 */
#define PJNATH_EINVAF		    (PJNATH_ERRNO_START+42) /* 370042 */

/**
 * @hideinitializer
 * Invalid STUN server or server not configured.
 */
#define PJNATH_ESTUNINSERVER	    (PJNATH_ERRNO_START+50) /* 370050 */


/************************************************************
 * STUN SESSION/TRANSPORT ERROR CODES
 ***********************************************************/
/**
 * @hideinitializer
 * STUN object has been destoyed.
 */
#define PJNATH_ESTUNDESTROYED	    (PJNATH_ERRNO_START+60) /* 370060 */


/************************************************************
 * ICE ERROR CODES
 ***********************************************************/

/**
 * @hideinitializer
 * ICE session not available
 */
#define PJNATH_ENOICE		    (PJNATH_ERRNO_START+80) /* 370080 */
/**
 * @hideinitializer
 * ICE check is in progress
 */
#define PJNATH_EICEINPROGRESS	    (PJNATH_ERRNO_START+81) /* 370081 */
/**
 * @hideinitializer
 * This error indicates that ICE connectivity check has failed, because
 * there is at least one ICE component that does not have a valid check.
 * Normally this happens because the network topology had caused the
 * connectivity check to fail (e.g. no route between the two agents),
 * however other reasons may include software incompatibility between
 * the two agents, or incomplete candidates gathered by the agent(s).
 */
#define PJNATH_EICEFAILED	    (PJNATH_ERRNO_START+82) /* 370082 */
/**
 * @hideinitializer
 * Default destination does not match any ICE candidates
 */
#define PJNATH_EICEMISMATCH	    (PJNATH_ERRNO_START+83) /* 370083 */
/**
 * @hideinitializer
 * Invalid ICE component ID
 */
#define PJNATH_EICEINCOMPID	    (PJNATH_ERRNO_START+86) /* 370086 */
/**
 * @hideinitializer
 * Invalid ICE candidate ID
 */
#define PJNATH_EICEINCANDID	    (PJNATH_ERRNO_START+87) /* 370087 */
/**
 * @hideinitializer
 * Source address mismatch. This error occurs if the source address
 * of the response for ICE connectivity check is different than
 * the destination address of the request.
 */
#define PJNATH_EICEINSRCADDR	    (PJNATH_ERRNO_START+88) /* 370088 */
/**
 * @hideinitializer
 * Missing ICE SDP attribute
 */
#define PJNATH_EICEMISSINGSDP	    (PJNATH_ERRNO_START+90) /* 370090 */
/**
 * @hideinitializer
 * Invalid SDP "candidate" attribute
 */
#define PJNATH_EICEINCANDSDP	    (PJNATH_ERRNO_START+91) /* 370091 */
/**
 * @hideinitializer
 * No host candidate associated with srflx. This error occurs when
 * a server reflexive candidate is added without the matching 
 * host candidate.
 */
#define PJNATH_EICENOHOSTCAND	    (PJNATH_ERRNO_START+92) /* 370092 */
/**
 * @hideinitializer
 * Controlled agent timed-out in waiting for the controlling agent to 
 * send nominated check after all connectivity checks have completed.
 */
#define PJNATH_EICENOMTIMEOUT	    (PJNATH_ERRNO_START+93) /* 370093 */

/************************************************************
 * TURN ERROR CODES
 ***********************************************************/
/**
 * @hideinitializer
 * Invalid or unsupported TURN transport.
 */
#define PJNATH_ETURNINTP	    (PJNATH_ERRNO_START+120) /* 370120 */

/************************************************************
 * TCP ERROR CODES
 ***********************************************************/
/**
 * @hideinitializer
 * Invalid or unsupported TURN transport.
 */
#define PJNATH_ETCPINTP	    (PJNATH_ERRNO_START+320) /* 370320 */



/**
 * @}
 */

#endif	/* __PJNATH_ERRNO_H__ */

/* $Id: config.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_CONFIG_H__
#define __PJNATH_CONFIG_H__


/**
 * @file config.h
 * @brief Compile time settings
 */

#include <pj/types.h>

/**
 * @defgroup PJNATH_CONFIG Compile-time configurations
 * @brief Various compile time settings
 * @ingroup PJNATH_STUN_BASE
 * @{
 */


/* **************************************************************************
 * GENERAL
 */

/**
 * The log level for PJNATH error display.
 *
 * default 1
 */
#ifndef PJNATH_ERROR_LEVEL
#   define PJNATH_ERROR_LEVEL			    1
#endif


/* **************************************************************************
 * STUN CONFIGURATION
 */

/**
 * Maximum number of attributes in the STUN packet (for the new STUN
 * library).
 *
 * Default: 16
 */
#ifndef PJ_STUN_MAX_ATTR
#   define PJ_STUN_MAX_ATTR			    16
#endif

/**
 * The default initial STUN round-trip time estimation (the RTO value
 * in RFC 3489-bis), in miliseconds. 
 * This value is used to control the STUN request 
 * retransmit time. The initial value of retransmission interval 
 * would be set to this value, and will be doubled after each
 * retransmission.
 */
#ifndef PJ_STUN_RTO_VALUE
#   define PJ_STUN_RTO_VALUE			    100
#endif


/**
 * The STUN transaction timeout value, in miliseconds.
 * After the last retransmission is sent and if no response is received 
 * after this time, the STUN transaction will be considered to have failed.
 *
 * The default value is 16x RTO (as per RFC 3489-bis).
 */
#ifndef PJ_STUN_TIMEOUT_VALUE
#   define PJ_STUN_TIMEOUT_VALUE		    (16 * PJ_STUN_RTO_VALUE)
#endif


/**
 * Maximum number of STUN transmission count.
 *
 * Default: 7 (as per RFC 3489-bis)
 */
#ifndef PJ_STUN_MAX_TRANSMIT_COUNT
#   define PJ_STUN_MAX_TRANSMIT_COUNT		    7
#endif


/**
 * Duration to keep response in the cache, in msec.
 *
 * Default: 10000 (as per RFC 3489-bis)
 */
#ifndef PJ_STUN_RES_CACHE_DURATION
#   define PJ_STUN_RES_CACHE_DURATION		    10000
#endif


/**
 * Maximum size of STUN message.
 */
#ifndef PJ_STUN_MAX_PKT_LEN
#   define PJ_STUN_MAX_PKT_LEN			    800
#endif


/**
 * Default STUN port as defined by RFC 3489.
 */
#define PJ_STUN_PORT				    3478


/**
 * Padding character for string attributes.
 *
 * Default: ASCII 0
 */
#ifndef PJ_STUN_STRING_ATTR_PAD_CHR
#   define PJ_STUN_STRING_ATTR_PAD_CHR		    0
#endif


/**
 * Enable pre-RFC3489bis-07 style of STUN MESSAGE-INTEGRITY and FINGERPRINT
 * calculation. By default this should be disabled since the calculation is
 * not backward compatible with current STUN specification.
 */
#ifndef PJ_STUN_OLD_STYLE_MI_FINGERPRINT
#   define PJ_STUN_OLD_STYLE_MI_FINGERPRINT	    0
#endif


/* **************************************************************************
 * STUN TRANSPORT CONFIGURATION
 */

/**
 * The packet buffer size for the STUN transport.
 */
#ifndef PJ_STUN_SOCK_PKT_LEN
#   define PJ_STUN_SOCK_PKT_LEN			    2000
#endif


/**
 * The duration of the STUN keep-alive period, in seconds.
 */
#ifndef PJ_STUN_KEEP_ALIVE_SEC
#   define PJ_STUN_KEEP_ALIVE_SEC		    15
#endif


/* **************************************************************************
 * TURN CONFIGURATION
 */

/**
 * Maximum DNS SRV entries to be processed in the DNS SRV response
 */
#ifndef PJ_TURN_MAX_DNS_SRV_CNT
#   define PJ_TURN_MAX_DNS_SRV_CNT		    4
#endif


/**
 * Maximum TURN packet size to be supported.
 */
#ifndef PJ_TURN_MAX_PKT_LEN
#   define PJ_TURN_MAX_PKT_LEN			    3000
#endif


/**
 * The TURN permission lifetime setting. This value should be taken from the
 * TURN protocol specification.
 */
#ifndef PJ_TURN_PERM_TIMEOUT
#   define PJ_TURN_PERM_TIMEOUT			    300
#endif


/**
 * The TURN channel binding lifetime. This value should be taken from the
 * TURN protocol specification.
 */
#ifndef PJ_TURN_CHANNEL_TIMEOUT
#   define PJ_TURN_CHANNEL_TIMEOUT		    600
#endif


/**
 * Number of seconds to refresh the permission/channel binding before the 
 * permission/channel binding expires. This value should be greater than 
 * PJ_TURN_PERM_TIMEOUT setting.
 */
#ifndef PJ_TURN_REFRESH_SEC_BEFORE
#   define PJ_TURN_REFRESH_SEC_BEFORE		    60
#endif


/**
 * The TURN session timer heart beat interval. When this timer occurs, the 
 * TURN session will scan all the permissions/channel bindings to see which
 * need to be refreshed.
 */
#ifndef PJ_TURN_KEEP_ALIVE_SEC
#   define PJ_TURN_KEEP_ALIVE_SEC		    15
#endif

/* **************************************************************************
 * TCP TRANSPORT CONFIGURATION
 */

/**
 * Maximum TCP packet size to be supported.
 */
#ifndef PJ_TCP_MAX_PKT_LEN
#   define PJ_TCP_MAX_PKT_LEN			    3000
#endif

/**
 * Maximum DNS SRV entries to be processed in the DNS SRV response
 */
#ifndef PJ_TCP_MAX_DNS_SRV_CNT
#   define PJ_TCP_MAX_DNS_SRV_CNT		    4
#endif


/**
 * The TCP session timer heart beat interval. When this timer occurs, the 
 * TCP session will scan all the connected session to see which
 * need to be refreshed.
 */
#ifndef PJ_TCP_KEEP_ALIVE_SEC
#   define PJ_TCP_KEEP_ALIVE_SEC		    30
#endif


/* **************************************************************************
 * ICE CONFIGURATION
 */

/**
 * Maximum number of ICE candidates.
 *
 * Default: 16
 */
#ifndef PJ_ICE_MAX_CAND
#   define PJ_ICE_MAX_CAND			    16
#endif


/**
 * Maximum number of candidates for each ICE stream transport component.
 *
 * Default: 8
 */
#ifndef PJ_ICE_ST_MAX_CAND
#   define PJ_ICE_ST_MAX_CAND			    8
#endif


/**
 * The number of bits to represent component IDs. This will affect
 * the maximum number of components (PJ_ICE_MAX_COMP) value.
 */
#ifndef PJ_ICE_COMP_BITS
#   define PJ_ICE_COMP_BITS			    1
#endif


/**
 * Maximum number of ICE components.
 */
#define PJ_ICE_MAX_COMP				    (2<<PJ_ICE_COMP_BITS)

/**
 * Use the priority value according to the ice-draft.
 */
#ifndef PJNATH_ICE_PRIO_STD
#   define PJNATH_ICE_PRIO_STD			    1
#endif


/**
 * The number of bits to represent candidate type preference.
 */
#ifndef PJ_ICE_CAND_TYPE_PREF_BITS
#   if PJNATH_ICE_PRIO_STD
#	define PJ_ICE_CAND_TYPE_PREF_BITS	    8
#   else
#	define PJ_ICE_CAND_TYPE_PREF_BITS	    2
#   endif
#endif


/**
 * The number of bits to represent ICE candidate's local preference. The
 * local preference is used to specify preference among candidates with
 * the same type, and ICE draft suggests 65535 as the default local 
 * preference, which means we need 16 bits to represent the value. But 
 * since we don't have the facility to specify local preference, we'll
 * just disable this feature and let the preference sorted by the 
 * type only.
 *
 * Default: 0
 */
#ifndef PJ_ICE_LOCAL_PREF_BITS
#   define PJ_ICE_LOCAL_PREF_BITS		    0
#endif


/**
 * Maximum number of ICE checks.
 *
 * Default: 32
 */
#ifndef PJ_ICE_MAX_CHECKS
#   define PJ_ICE_MAX_CHECKS			    32
#endif


/**
 * Default timer interval (in miliseconds) for starting ICE periodic checks.
 *
 * Default: 20
 */
#ifndef PJ_ICE_TA_VAL
#   define PJ_ICE_TA_VAL			    20
#endif


/**
 * According to ICE Section 8.2. Updating States, if an In-Progress pair in 
 * the check list is for the same component as a nominated pair, the agent 
 * SHOULD cease retransmissions for its check if its pair priority is lower
 * than the lowest priority nominated pair for that component.
 *
 * If a higher priority check is In Progress, this rule would cause that
 * check to be performed even when it most likely will fail.
 *
 * The macro here controls if ICE session should cancel all In Progress 
 * checks for the same component regardless of its priority.
 *
 * Default: 1 (yes, cancel all)
 */
#ifndef PJ_ICE_CANCEL_ALL
#   define PJ_ICE_CANCEL_ALL			    1
#endif


/**
 * For a controlled agent, specify how long it wants to wait (in milliseconds)
 * for the controlling agent to complete sending connectivity check with
 * nominated flag set to true for all components after the controlled agent
 * has found that all connectivity checks in its checklist have been completed
 * and there is at least one successful (but not nominated) check for every
 * component.
 *
 * When selecting the value, bear in mind that the connectivity check from
 * controlling agent may be delayed because of delay in receiving SDP answer
 * from the controlled agent.
 *
 * Application may set this value to -1 to disable this timer.
 *
 * Default: 10000 (milliseconds)
 */
#ifndef ICE_CONTROLLED_AGENT_WAIT_NOMINATION_TIMEOUT
#   define ICE_CONTROLLED_AGENT_WAIT_NOMINATION_TIMEOUT	10000
#endif


/**
 * For controlling agent if it uses regular nomination, specify the delay to
 * perform nominated check (connectivity check with USE-CANDIDATE attribute)
 * after all components have a valid pair.
 *
 * Default: 4*PJ_STUN_RTO_VALUE (milliseconds)
 */
#ifndef PJ_ICE_NOMINATED_CHECK_DELAY
#   define PJ_ICE_NOMINATED_CHECK_DELAY		    (15*PJ_STUN_RTO_VALUE)
/**
 * if router is PPPoE, the TCP will take more time, so we need extend the delay time
 * Andrew Hung (2013/02/21)
 * set to original value
 * Dean Li (2013/10/22)
 * set to to 10*PJ_STUN_RTO_VALUE again
 * Dean Li (2013/12/29)
 */
//#   define PJ_ICE_NOMINATED_CHECK_DELAY		    (15*PJ_STUN_RTO_VALUE)
#endif


/**
 * Minimum interval value to be used for sending STUN keep-alive on the ICE
 * session, in seconds. This minimum interval, plus a random value
 * which maximum is PJ_ICE_SESS_KEEP_ALIVE_MAX_RAND, specify the actual interval
 * of the STUN keep-alive.
 *
 * Default: 15 seconds
 *
 * @see PJ_ICE_SESS_KEEP_ALIVE_MAX_RAND
 */
#ifndef PJ_ICE_SESS_KEEP_ALIVE_MIN
#   define PJ_ICE_SESS_KEEP_ALIVE_MIN		    20
#endif

/* Warn about deprecated macro */
#ifdef PJ_ICE_ST_KEEP_ALIVE_MIN
#   error PJ_ICE_ST_KEEP_ALIVE_MIN is deprecated
#endif

/**
 * To prevent STUN keep-alives to be sent simultaneously, application should
 * add random interval to minimum interval (PJ_ICE_SESS_KEEP_ALIVE_MIN). This
 * setting specifies the maximum random value to be added to the minimum
 * interval, in seconds.
 *
 * Default: 5 seconds
 *
 * @see PJ_ICE_SESS_KEEP_ALIVE_MIN
 */
#ifndef PJ_ICE_SESS_KEEP_ALIVE_MAX_RAND
#   define PJ_ICE_SESS_KEEP_ALIVE_MAX_RAND	    5
#endif

/* Warn about deprecated macro */
#ifdef PJ_ICE_ST_KEEP_ALIVE_MAX_RAND
#   error PJ_ICE_ST_KEEP_ALIVE_MAX_RAND is deprecated
#endif


/**
 * This constant specifies the length of random string generated for ICE
 * ufrag and password.
 *
 * Default: 8 (characters)
 */
#ifndef PJ_ICE_UFRAG_LEN
#   define PJ_ICE_UFRAG_LEN			    8
#endif


/** ICE session pool initial size. */
#ifndef PJNATH_POOL_LEN_ICE_SESS
#   define PJNATH_POOL_LEN_ICE_SESS		    512
#endif

/** ICE session pool increment size */
#ifndef PJNATH_POOL_INC_ICE_SESS
#   define PJNATH_POOL_INC_ICE_SESS		    512
#endif

/** ICE stream transport pool initial size. */
#ifndef PJNATH_POOL_LEN_ICE_STRANS
#   define PJNATH_POOL_LEN_ICE_STRANS		    1000
#endif

/** ICE stream transport pool increment size */
#ifndef PJNATH_POOL_INC_ICE_STRANS
#   define PJNATH_POOL_INC_ICE_STRANS		    512
#endif

/** NAT detect pool initial size */
#ifndef PJNATH_POOL_LEN_NATCK
#   define PJNATH_POOL_LEN_NATCK		    512
#endif

/** NAT detect pool increment size */
#ifndef PJNATH_POOL_INC_NATCK
#   define PJNATH_POOL_INC_NATCK		    512
#endif

/** STUN session pool initial size */
#ifndef PJNATH_POOL_LEN_STUN_SESS
#   define PJNATH_POOL_LEN_STUN_SESS		    1000
#endif

/** STUN session pool increment size */
#ifndef PJNATH_POOL_INC_STUN_SESS
#   define PJNATH_POOL_INC_STUN_SESS		    1000
#endif

/** STUN session transmit data pool initial size */
#ifndef PJNATH_POOL_LEN_STUN_TDATA
#   define PJNATH_POOL_LEN_STUN_TDATA		    1000
#endif

/** STUN session transmit data pool increment size */
#ifndef PJNATH_POOL_INC_STUN_TDATA
#   define PJNATH_POOL_INC_STUN_TDATA		    1000
#endif

/** TURN session initial pool size */
#ifndef PJNATH_POOL_LEN_TURN_SESS
#   define PJNATH_POOL_LEN_TURN_SESS		    1000
#endif

/** TURN session pool increment size */
#ifndef PJNATH_POOL_INC_TURN_SESS
#   define PJNATH_POOL_INC_TURN_SESS		    1000
#endif

/** TURN socket initial pool size */
#ifndef PJNATH_POOL_LEN_TURN_SOCK
#   define PJNATH_POOL_LEN_TURN_SOCK		    1000
#endif

/** TURN socket pool increment size */
#ifndef PJNATH_POOL_INC_TURN_SOCK
#   define PJNATH_POOL_INC_TURN_SOCK		    1000
#endif

/** TCP session initial pool size */
#ifndef PJNATH_POOL_LEN_TCP_SESS
#   define PJNATH_POOL_LEN_TCP_SESS		    1000
#endif

/** TCP session pool increment size */
#ifndef PJNATH_POOL_INC_TCP_SESS
#   define PJNATH_POOL_INC_TCP_SESS		    1000
#endif

/** TCP socket initial pool size */
#ifndef PJNATH_POOL_LEN_TCP_SOCK
#   define PJNATH_POOL_LEN_TCP_SOCK		    1000
#endif

/** TCP socket pool increment size */
#ifndef PJNATH_POOL_INC_TCP_SOCK
#   define PJNATH_POOL_INC_TCP_SOCK		    1000
#endif

/**
 * @}
 */

#endif	/* __PJNATH_CONFIG_H__ */


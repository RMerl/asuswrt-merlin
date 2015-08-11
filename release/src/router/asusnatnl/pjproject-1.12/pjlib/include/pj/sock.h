/* $Id: sock.h 3741 2011-08-30 01:45:07Z bennylp $ */
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
#ifndef __PJ_SOCK_H__
#define __PJ_SOCK_H__

/**
 * @file sock.h
 * @brief Socket Abstraction.
 */

#include <pj/types.h>

PJ_BEGIN_DECL 


/**
 * @defgroup PJ_SOCK Socket Abstraction
 * @ingroup PJ_IO
 * @{
 *
 * The PJLIB socket abstraction layer is a thin and very portable abstraction
 * for socket API. It provides API similar to BSD socket API. The abstraction
 * is needed because BSD socket API is not always available on all platforms,
 * therefore it wouldn't be possible to create a trully portable network
 * programs unless we provide such abstraction.
 *
 * Applications can use this API directly in their application, just
 * as they would when using traditional BSD socket API, provided they
 * call #pj_init() first.
 *
 * \section pj_sock_examples_sec Examples
 *
 * For some examples on how to use the socket API, please see:
 *
 *  - \ref page_pjlib_sock_test
 *  - \ref page_pjlib_select_test
 *  - \ref page_pjlib_sock_perf_test
 */


/**
 * Supported address families. 
 * APPLICATION MUST USE THESE VALUES INSTEAD OF NORMAL AF_*, BECAUSE
 * THE LIBRARY WILL DO TRANSLATION TO THE NATIVE VALUE.
 */

/** Address family is unspecified. @see pj_AF_UNSPEC() */
extern const pj_uint16_t PJ_AF_UNSPEC;

/** Unix domain socket.	@see pj_AF_UNIX() */
extern const pj_uint16_t PJ_AF_UNIX;

/** POSIX name for AF_UNIX	*/
#define PJ_AF_LOCAL	 PJ_AF_UNIX;

/** Internet IP protocol. @see pj_AF_INET() */
extern const pj_uint16_t PJ_AF_INET;

/** IP version 6. @see pj_AF_INET6() */
extern const pj_uint16_t PJ_AF_INET6;

/** Packet family. @see pj_AF_PACKET() */
extern const pj_uint16_t PJ_AF_PACKET;

/** IRDA sockets. @see pj_AF_IRDA() */
extern const pj_uint16_t PJ_AF_IRDA;

/*
 * Accessor functions for various address family constants. These
 * functions are provided because Symbian doesn't allow exporting
 * global variables from a DLL.
 */

#if defined(PJ_DLL)
    /** Get #PJ_AF_UNSPEC value */
    PJ_DECL(pj_uint16_t) pj_AF_UNSPEC(void);
    /** Get #PJ_AF_UNIX value. */
    PJ_DECL(pj_uint16_t) pj_AF_UNIX(void);
    /** Get #PJ_AF_INET value. */
    PJ_DECL(pj_uint16_t) pj_AF_INET(void);
    /** Get #PJ_AF_INET6 value. */
    PJ_DECL(pj_uint16_t) pj_AF_INET6(void);
    /** Get #PJ_AF_PACKET value. */
    PJ_DECL(pj_uint16_t) pj_AF_PACKET(void);
    /** Get #PJ_AF_IRDA value. */
    PJ_DECL(pj_uint16_t) pj_AF_IRDA(void);
#else
    /* When pjlib is not built as DLL, these accessor functions are
     * simply a macro to get their constants
     */
    /** Get #PJ_AF_UNSPEC value */
#   define pj_AF_UNSPEC()   PJ_AF_UNSPEC
    /** Get #PJ_AF_UNIX value. */
#   define pj_AF_UNIX()	    PJ_AF_UNIX
    /** Get #PJ_AF_INET value. */
#   define pj_AF_INET()	    PJ_AF_INET
    /** Get #PJ_AF_INET6 value. */
#   define pj_AF_INET6()    PJ_AF_INET6
    /** Get #PJ_AF_PACKET value. */
#   define pj_AF_PACKET()   PJ_AF_PACKET
    /** Get #PJ_AF_IRDA value. */
#   define pj_AF_IRDA()	    PJ_AF_IRDA
#endif


/**
 * Supported types of sockets.
 * APPLICATION MUST USE THESE VALUES INSTEAD OF NORMAL SOCK_*, BECAUSE
 * THE LIBRARY WILL TRANSLATE THE VALUE TO THE NATIVE VALUE.
 */

/** Sequenced, reliable, connection-based byte streams.
 *  @see pj_SOCK_STREAM() */
extern const pj_uint16_t PJ_SOCK_STREAM;

/** Connectionless, unreliable datagrams of fixed maximum lengths.
 *  @see pj_SOCK_DGRAM() */
extern const pj_uint16_t PJ_SOCK_DGRAM;

/** Raw protocol interface. @see pj_SOCK_RAW() */
extern const pj_uint16_t PJ_SOCK_RAW;

/** Reliably-delivered messages.  @see pj_SOCK_RDM() */
extern const pj_uint16_t PJ_SOCK_RDM;


/*
 * Accessor functions for various constants. These functions are provided
 * because Symbian doesn't allow exporting global variables from a DLL.
 */

#if defined(PJ_DLL)
    /** Get #PJ_SOCK_STREAM constant */
    PJ_DECL(int) pj_SOCK_STREAM(void);
    /** Get #PJ_SOCK_DGRAM constant */
    PJ_DECL(int) pj_SOCK_DGRAM(void);
    /** Get #PJ_SOCK_RAW constant */
    PJ_DECL(int) pj_SOCK_RAW(void);
    /** Get #PJ_SOCK_RDM constant */
    PJ_DECL(int) pj_SOCK_RDM(void);
#else
    /** Get #PJ_SOCK_STREAM constant */
#   define pj_SOCK_STREAM() PJ_SOCK_STREAM
    /** Get #PJ_SOCK_DGRAM constant */
#   define pj_SOCK_DGRAM()  PJ_SOCK_DGRAM
    /** Get #PJ_SOCK_RAW constant */
#   define pj_SOCK_RAW()    PJ_SOCK_RAW
    /** Get #PJ_SOCK_RDM constant */
#   define pj_SOCK_RDM()    PJ_SOCK_RDM
#endif


/**
 * Socket level specified in #pj_sock_setsockopt() or #pj_sock_getsockopt().
 * APPLICATION MUST USE THESE VALUES INSTEAD OF NORMAL SOL_*, BECAUSE
 * THE LIBRARY WILL TRANSLATE THE VALUE TO THE NATIVE VALUE.
 */
/** Socket level. @see pj_SOL_SOCKET() */
extern const pj_uint16_t PJ_SOL_SOCKET;
/** IP level. @see pj_SOL_IP() */
extern const pj_uint16_t PJ_SOL_IP;
/** TCP level. @see pj_SOL_TCP() */
extern const pj_uint16_t PJ_SOL_TCP;
/** UDP level. @see pj_SOL_UDP() */
extern const pj_uint16_t PJ_SOL_UDP;
/** IP version 6. @see pj_SOL_IPV6() */
extern const pj_uint16_t PJ_SOL_IPV6;

/*
 * Accessor functions for various constants. These functions are provided
 * because Symbian doesn't allow exporting global variables from a DLL.
 */

#if defined(PJ_DLL)
    /** Get #PJ_SOL_SOCKET constant */
    PJ_DECL(pj_uint16_t) pj_SOL_SOCKET(void);
    /** Get #PJ_SOL_IP constant */
    PJ_DECL(pj_uint16_t) pj_SOL_IP(void);
    /** Get #PJ_SOL_TCP constant */
    PJ_DECL(pj_uint16_t) pj_SOL_TCP(void);
    /** Get #PJ_SOL_UDP constant */
    PJ_DECL(pj_uint16_t) pj_SOL_UDP(void);
    /** Get #PJ_SOL_IPV6 constant */
    PJ_DECL(pj_uint16_t) pj_SOL_IPV6(void);
#else
    /** Get #PJ_SOL_SOCKET constant */
#   define pj_SOL_SOCKET()  PJ_SOL_SOCKET
    /** Get #PJ_SOL_IP constant */
#   define pj_SOL_IP()	    PJ_SOL_IP
    /** Get #PJ_SOL_TCP constant */
#   define pj_SOL_TCP()	    PJ_SOL_TCP
    /** Get #PJ_SOL_UDP constant */
#   define pj_SOL_UDP()	    PJ_SOL_UDP
    /** Get #PJ_SOL_IPV6 constant */
#   define pj_SOL_IPV6()    PJ_SOL_IPV6
#endif


/* IP_TOS 
 *
 * Note:
 *  TOS CURRENTLY DOES NOT WORK IN Windows 2000 and above!
 *  See http://support.microsoft.com/kb/248611
 */
/** IP_TOS optname in setsockopt(). @see pj_IP_TOS() */
extern const pj_uint16_t PJ_IP_TOS;

/*
 * IP TOS related constats.
 *
 * Note:
 *  TOS CURRENTLY DOES NOT WORK IN Windows 2000 and above!
 *  See http://support.microsoft.com/kb/248611
 */
/** Minimize delays. @see pj_IPTOS_LOWDELAY() */
extern const pj_uint16_t PJ_IPTOS_LOWDELAY;

/** Optimize throughput. @see pj_IPTOS_THROUGHPUT() */
extern const pj_uint16_t PJ_IPTOS_THROUGHPUT;

/** Optimize for reliability. @see pj_IPTOS_RELIABILITY() */
extern const pj_uint16_t PJ_IPTOS_RELIABILITY;

/** "filler data" where slow transmission does't matter.
 *  @see pj_IPTOS_MINCOST() */
extern const pj_uint16_t PJ_IPTOS_MINCOST;


#if defined(PJ_DLL)
    /** Get #PJ_IP_TOS constant */
    PJ_DECL(int) pj_IP_TOS(void);

    /** Get #PJ_IPTOS_LOWDELAY constant */
    PJ_DECL(int) pj_IPTOS_LOWDELAY(void);

    /** Get #PJ_IPTOS_THROUGHPUT constant */
    PJ_DECL(int) pj_IPTOS_THROUGHPUT(void);

    /** Get #PJ_IPTOS_RELIABILITY constant */
    PJ_DECL(int) pj_IPTOS_RELIABILITY(void);

    /** Get #PJ_IPTOS_MINCOST constant */
    PJ_DECL(int) pj_IPTOS_MINCOST(void);
#else
    /** Get #PJ_IP_TOS constant */
#   define pj_IP_TOS()		PJ_IP_TOS

    /** Get #PJ_IPTOS_LOWDELAY constant */
#   define pj_IPTOS_LOWDELAY()	PJ_IP_TOS_LOWDELAY

    /** Get #PJ_IPTOS_THROUGHPUT constant */
#   define pj_IPTOS_THROUGHPUT() PJ_IP_TOS_THROUGHPUT

    /** Get #PJ_IPTOS_RELIABILITY constant */
#   define pj_IPTOS_RELIABILITY() PJ_IP_TOS_RELIABILITY

    /** Get #PJ_IPTOS_MINCOST constant */
#   define pj_IPTOS_MINCOST()	PJ_IP_TOS_MINCOST
#endif


/**
 * Values to be specified as \c optname when calling #pj_sock_setsockopt() 
 * or #pj_sock_getsockopt().
 */

/** Socket type. @see pj_SO_TYPE() */
extern const pj_uint16_t PJ_SO_TYPE;

/** Buffer size for receive. @see pj_SO_RCVBUF() */
extern const pj_uint16_t PJ_SO_RCVBUF;

/** Buffer size for send. @see pj_SO_SNDBUF() */
extern const pj_uint16_t PJ_SO_SNDBUF;

/** Disables the Nagle algorithm for send coalescing. @see pj_TCP_NODELAY */
extern const pj_uint16_t PJ_TCP_NODELAY;

/** Allows the socket to be bound to an address that is already in use.
 *  @see pj_SO_REUSEADDR */
extern const pj_uint16_t PJ_SO_REUSEADDR;

/** Do not generate SIGPIPE. @see pj_SO_NOSIGPIPE */
extern const pj_uint16_t PJ_SO_NOSIGPIPE;

/** Set the protocol-defined priority for all packets to be sent on socket.
 */
extern const pj_uint16_t PJ_SO_PRIORITY;

/** Set boardcast mode.
 */
extern const pj_uint16_t PJ_SO_BROADCAST;

/** IP multicast interface. @see pj_IP_MULTICAST_IF() */
extern const pj_uint16_t PJ_IP_MULTICAST_IF;
 
/** IP multicast ttl. @see pj_IP_MULTICAST_TTL() */
extern const pj_uint16_t PJ_IP_MULTICAST_TTL;

/** IP multicast loopback. @see pj_IP_MULTICAST_LOOP() */
extern const pj_uint16_t PJ_IP_MULTICAST_LOOP;

/** Add an IP group membership. @see pj_IP_ADD_MEMBERSHIP() */
extern const pj_uint16_t PJ_IP_ADD_MEMBERSHIP;

/** Drop an IP group membership. @see pj_IP_DROP_MEMBERSHIP() */
extern const pj_uint16_t PJ_IP_DROP_MEMBERSHIP;

/** TCP keep-alive option. @see pj_SO_KEEPALIVE() */
extern const pj_uint16_t PJ_SO_KEEPALIVE;

/** The time for triggering to send keep-alive packet. @see pj_TCP_KEEPALIVE() */
extern const pj_uint16_t PJ_TCP_KEEPALIVE;

/** The time for triggering to send keep-alive packet. @see pj_TCP_KEEPIDLE() */
extern const pj_uint16_t PJ_TCP_KEEPIDLE;

/** TCP Keep Alive interval. @see pj_TCP_KEEPINTVL() */
extern const pj_uint16_t PJ_TCP_KEEPINTVL;

/** TCP Keep Alive packet sending count. @see pj_TCP_KEEPCNT() */
extern const pj_uint16_t PJ_TCP_KEEPCNT;


#if defined(PJ_DLL)
    /** Get #PJ_SO_TYPE constant */
    PJ_DECL(pj_uint16_t) pj_SO_TYPE(void);

    /** Get #PJ_SO_RCVBUF constant */
    PJ_DECL(pj_uint16_t) pj_SO_RCVBUF(void);

    /** Get #PJ_SO_SNDBUF constant */
    PJ_DECL(pj_uint16_t) pj_SO_SNDBUF(void);

    /** Get #PJ_TCP_NODELAY constant */
    PJ_DECL(pj_uint16_t) pj_TCP_NODELAY(void);

    /** Get #PJ_SO_REUSEADDR constant */
    PJ_DECL(pj_uint16_t) pj_SO_REUSEADDR(void);

    /** Get #PJ_SO_NOSIGPIPE constant */
    PJ_DECL(pj_uint16_t) pj_SO_NOSIGPIPE(void);

    /** Get #PJ_SO_PRIORITY constant */
	PJ_DECL(pj_uint16_t) pj_SO_PRIORITY(void);

	/** Get #PJ_SO_BROADCAST constant */
	PJ_DECL(pj_uint16_t) pj_PJ_SO_BROADCAST(void);

    /** Get #PJ_IP_MULTICAST_IF constant */
    PJ_DECL(pj_uint16_t) pj_IP_MULTICAST_IF(void);

    /** Get #PJ_IP_MULTICAST_TTL constant */
    PJ_DECL(pj_uint16_t) pj_IP_MULTICAST_TTL(void);

    /** Get #PJ_IP_MULTICAST_LOOP constant */
	PJ_DECL(pj_uint16_t) pj_IP_MULTICAST_LOOP(void);

	/** Get #PJ_IP_ADD_MEMBERSHIP constant */
	PJ_DECL(pj_uint16_t) pj_IP_ADD_MEMBERSHIP(void);

	/** Get #PJ_SO_KEEPALIVE constant */
	PJ_DECL(pj_uint16_t) pj_SO_KEEPALIVE(void);

	/** Get #PJ_TCP_KEEPALIVE constant */
	PJ_DECL(pj_uint16_t) pj_TCP_KEEPALIVE(void);

	/** Get #PJ_TCP_KEEPIDLE constant */
	PJ_DECL(pj_uint16_t) pj_TCP_KEEPIDLE(void);

	/** Get #PJ_TCP_KEEPINTVL constant */
	PJ_DECL(pj_uint16_t) pj_TCP_KEEPINTVL(void);

	/** Get #PJ_TCP_KEEPCNT constant */
	PJ_DECL(pj_uint16_t) pj_TCP_KEEPCNT(void);
#else
    /** Get #PJ_SO_TYPE constant */
#   define pj_SO_TYPE()	    PJ_SO_TYPE

    /** Get #PJ_SO_RCVBUF constant */
#   define pj_SO_RCVBUF()   PJ_SO_RCVBUF

    /** Get #PJ_SO_SNDBUF constant */
#   define pj_SO_SNDBUF()   PJ_SO_SNDBUF

    /** Get #PJ_TCP_NODELAY constant */
#   define pj_TCP_NODELAY() PJ_TCP_NODELAY

    /** Get #PJ_SO_REUSEADDR constant */
#   define pj_SO_REUSEADDR() PJ_SO_REUSEADDR

    /** Get #PJ_SO_NOSIGPIPE constant */
#   define pj_SO_NOSIGPIPE() PJ_SO_NOSIGPIPE

    /** Get #PJ_SO_PRIORITY constant */
#   define pj_SO_PRIORITY() PJ_SO_PRIORITY

    /** Get #PJ_SO_BROADCAST constant */
#   define pj_SO_BROADCAST() PJ_SO_BROADCAST

    /** Get #PJ_IP_MULTICAST_IF constant */
#   define pj_IP_MULTICAST_IF()    PJ_IP_MULTICAST_IF

    /** Get #PJ_IP_MULTICAST_TTL constant */
#   define pj_IP_MULTICAST_TTL()   PJ_IP_MULTICAST_TTL

    /** Get #PJ_IP_MULTICAST_LOOP constant */
#   define pj_IP_MULTICAST_LOOP()  PJ_IP_MULTICAST_LOOP

    /** Get #PJ_IP_ADD_MEMBERSHIP constant */
#   define pj_IP_ADD_MEMBERSHIP()  PJ_IP_ADD_MEMBERSHIP

/** Get #PJ_IP_DROP_MEMBERSHIP constant */
#   define pj_IP_DROP_MEMBERSHIP() PJ_IP_DROP_MEMBERSHIP

/** Get #PJ_SO_KEEPALIVE constant */
#   define pj_SO_KEEPALIVE() PJ_SO_KEEPALIVE

/** Get #PJ_TCP_KEEPALIVE constant */
#   define pj_TCP_KEEPALIVE() PJ_TCP_KEEPALIVE

/** Get #PJ_TCP_KEEPIDLE constant */
#   define pj_TCP_KEEPIDLE() PJ_TCP_KEEPIDLE

/** Get #PJ_TCP_KEEPINTVL constant */
#   define pj_TCP_KEEPINTVL() PJ_TCP_KEEPINTVL

/** Get #PJ_TCP_KEEPCNT constant */
#   define pj_TCP_KEEPCNT() PJ_TCP_KEEPCNT
#endif


/*
 * Flags to be specified in #pj_sock_recv, #pj_sock_send, etc.
 */

/** Out-of-band messages. @see pj_MSG_OOB() */
extern const int PJ_MSG_OOB;

/** Peek, don't remove from buffer. @see pj_MSG_PEEK() */
extern const int PJ_MSG_PEEK;

/** Don't route. @see pj_MSG_DONTROUTE() */
extern const int PJ_MSG_DONTROUTE;


#if defined(PJ_DLL)
    /** Get #PJ_MSG_OOB constant */
    PJ_DECL(int) pj_MSG_OOB(void);

    /** Get #PJ_MSG_PEEK constant */
    PJ_DECL(int) pj_MSG_PEEK(void);

    /** Get #PJ_MSG_DONTROUTE constant */
    PJ_DECL(int) pj_MSG_DONTROUTE(void);
#else
    /** Get #PJ_MSG_OOB constant */
#   define pj_MSG_OOB()		PJ_MSG_OOB

    /** Get #PJ_MSG_PEEK constant */
#   define pj_MSG_PEEK()	PJ_MSG_PEEK

    /** Get #PJ_MSG_DONTROUTE constant */
#   define pj_MSG_DONTROUTE()	PJ_MSG_DONTROUTE
#endif


/**
 * Flag to be specified in #pj_sock_shutdown().
 */
typedef enum pj_socket_sd_type
{
    PJ_SD_RECEIVE   = 0,    /**< No more receive.	    */
    PJ_SHUT_RD	    = 0,    /**< Alias for SD_RECEIVE.	    */
    PJ_SD_SEND	    = 1,    /**< No more sending.	    */
    PJ_SHUT_WR	    = 1,    /**< Alias for SD_SEND.	    */
    PJ_SD_BOTH	    = 2,    /**< No more send and receive.  */
    PJ_SHUT_RDWR    = 2     /**< Alias for SD_BOTH.	    */
} pj_socket_sd_type;



/** Address to accept any incoming messages. */
#define PJ_INADDR_ANY	    ((pj_uint32_t)0)

/** Address indicating an error return */
#define PJ_INADDR_NONE	    ((pj_uint32_t)0xffffffff)

/** Address to send to all hosts. */
#define PJ_INADDR_BROADCAST ((pj_uint32_t)0xffffffff)


/** 
 * Maximum length specifiable by #pj_sock_listen().
 * If the build system doesn't override this value, then the lowest 
 * denominator (five, in Win32 systems) will be used.
 */
#if !defined(PJ_SOMAXCONN)
#  define PJ_SOMAXCONN	5
#endif


/**
 * Constant for invalid socket returned by #pj_sock_socket() and
 * #pj_sock_accept().
 */
#define PJ_INVALID_SOCKET   (-1)

/* Must undefine s_addr because of pj_in_addr below */
#undef s_addr

/**
 * This structure describes Internet address.
 */
typedef struct pj_in_addr
{
    pj_uint32_t	s_addr;		/**< The 32bit IP address.	    */
} pj_in_addr;


/**
 * Maximum length of text representation of an IPv4 address.
 */
#define PJ_INET_ADDRSTRLEN	16

/**
 * Maximum length of text representation of an IPv6 address.
 */
#define PJ_INET6_ADDRSTRLEN	46

/**
 * The size of sin_zero field in pj_sockaddr_in structure. Most OSes
 * use 8, but others such as the BSD TCP/IP stack in eCos uses 24.
 */
#ifndef PJ_SOCKADDR_IN_SIN_ZERO_LEN
#   define PJ_SOCKADDR_IN_SIN_ZERO_LEN	8
#endif

/**
 * This structure describes Internet socket address.
 * If PJ_SOCKADDR_HAS_LEN is not zero, then sin_zero_len member is added
 * to this struct. As far the application is concerned, the value of
 * this member will always be zero. Internally, PJLIB may modify the value
 * before calling OS socket API, and reset the value back to zero before
 * returning the struct to application.
 */
struct pj_sockaddr_in
{
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
    pj_uint8_t  sin_zero_len;	/**< Just ignore this.		    */
    pj_uint8_t  sin_family;	/**< Address family.		    */
#else
    pj_uint16_t	sin_family;	/**< Address family.		    */
#endif
    pj_uint16_t	sin_port;	/**< Transport layer port number.   */
    pj_in_addr	sin_addr;	/**< IP address.		    */
    char	sin_zero[PJ_SOCKADDR_IN_SIN_ZERO_LEN]; /**< Padding.*/
};


#undef s6_addr

/**
 * This structure describes IPv6 address.
 */
typedef union pj_in6_addr
{
    /* This is the main entry */
    pj_uint8_t  s6_addr[16];   /**< 8-bit array */

    /* While these are used for proper alignment */
    pj_uint32_t	u6_addr32[4];

    /* Do not use this with Winsock2, as this will align pj_sockaddr_in6
     * to 64-bit boundary and Winsock2 doesn't like it!
     * Update 26/04/2010:
     *  This is now disabled, see http://trac.pjsip.org/repos/ticket/1058
     */
#if 0 && defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0 && \
    (!defined(PJ_WIN32) || PJ_WIN32==0)
    pj_int64_t	u6_addr64[2];
#endif

} pj_in6_addr;


/** Initializer value for pj_in6_addr. */
#define PJ_IN6ADDR_ANY_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } }

/** Initializer value for pj_in6_addr. */
#define PJ_IN6ADDR_LOOPBACK_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }

/**
 * This structure describes IPv6 socket address.
 * If PJ_SOCKADDR_HAS_LEN is not zero, then sin_zero_len member is added
 * to this struct. As far the application is concerned, the value of
 * this member will always be zero. Internally, PJLIB may modify the value
 * before calling OS socket API, and reset the value back to zero before
 * returning the struct to application.
 */
typedef struct pj_sockaddr_in6
{
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
    pj_uint8_t  sin6_zero_len;	    /**< Just ignore this.	   */
    pj_uint8_t  sin6_family;	    /**< Address family.	   */
#else
    pj_uint16_t	sin6_family;	    /**< Address family		    */
#endif
    pj_uint16_t	sin6_port;	    /**< Transport layer port number. */
    pj_uint32_t	sin6_flowinfo;	    /**< IPv6 flow information	    */
    pj_in6_addr sin6_addr;	    /**< IPv6 address.		    */
    pj_uint32_t sin6_scope_id;	    /**< Set of interfaces for a scope	*/
} pj_sockaddr_in6;


/**
 * This structure describes common attributes found in transport addresses.
 * If PJ_SOCKADDR_HAS_LEN is not zero, then sa_zero_len member is added
 * to this struct. As far the application is concerned, the value of
 * this member will always be zero. Internally, PJLIB may modify the value
 * before calling OS socket API, and reset the value back to zero before
 * returning the struct to application.
 */
typedef struct pj_addr_hdr
{
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
    pj_uint8_t  sa_zero_len;
    pj_uint8_t  sa_family;
#else
    pj_uint16_t	sa_family;	/**< Common data: address family.   */
#endif
} pj_addr_hdr;


/**
 * This union describes a generic socket address.
 */
typedef union pj_sockaddr
{
    pj_addr_hdr	    addr;	/**< Generic transport address.	    */
    pj_sockaddr_in  ipv4;	/**< IPv4 transport address.	    */
    pj_sockaddr_in6 ipv6;	/**< IPv6 transport address.	    */
} pj_sockaddr;


/**
 * This structure provides multicast group information for IPv4 addresses.
 */
typedef struct pj_ip_mreq {
    pj_in_addr imr_multiaddr;	/**< IP multicast address of group. */
    pj_in_addr imr_interface;	/**< local IP address of interface. */
} pj_ip_mreq;


/*****************************************************************************
 *
 * SOCKET ADDRESS MANIPULATION.
 *
 *****************************************************************************
 */

/**
 * Convert 16-bit value from network byte order to host byte order.
 *
 * @param netshort  16-bit network value.
 * @return	    16-bit host value.
 */
PJ_DECL(pj_uint16_t) pj_ntohs(pj_uint16_t netshort);

/**
 * Convert 16-bit value from host byte order to network byte order.
 *
 * @param hostshort 16-bit host value.
 * @return	    16-bit network value.
 */
PJ_DECL(pj_uint16_t) pj_htons(pj_uint16_t hostshort);

/**
 * Convert 32-bit value from network byte order to host byte order.
 *
 * @param netlong   32-bit network value.
 * @return	    32-bit host value.
 */
PJ_DECL(pj_uint32_t) pj_ntohl(pj_uint32_t netlong);

/**
 * Convert 32-bit value from host byte order to network byte order.
 *
 * @param hostlong  32-bit host value.
 * @return	    32-bit network value.
 */
PJ_DECL(pj_uint32_t) pj_htonl(pj_uint32_t hostlong);

/**
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 *
 * @param inaddr    The host address.
 * @return	    The string address.
 */
PJ_DECL(char*) pj_inet_ntoa(pj_in_addr inaddr);

/**
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 *
 * @param cp	IP address in standard numbers-and-dots notation.
 * @param inp	Structure that holds the output of the conversion.
 *
 * @return	nonzero if the address is valid, zero if not.
 */
PJ_DECL(int) pj_inet_aton(const pj_str_t *cp, struct pj_in_addr *inp);

/**
 * This function converts an address in its standard text presentation form
 * into its numeric binary form. It supports both IPv4 and IPv6 address
 * conversion.
 *
 * @param af	Specify the family of the address.  The PJ_AF_INET and 
 *		PJ_AF_INET6 address families shall be supported.  
 * @param src	Points to the string being passed in. 
 * @param dst	Points to a buffer into which the function stores the 
 *		numeric address; this shall be large enough to hold the
 *		numeric address (32 bits for PJ_AF_INET, 128 bits for
 *		PJ_AF_INET6).  
 *
 * @return	PJ_SUCCESS if conversion was successful.
 */
PJ_DECL(pj_status_t) pj_inet_pton(int af, const pj_str_t *src, void *dst);

/**
 * This function converts a numeric address into a text string suitable
 * for presentation. It supports both IPv4 and IPv6 address
 * conversion. 
 * @see pj_sockaddr_print()
 *
 * @param af	Specify the family of the address. This can be PJ_AF_INET
 *		or PJ_AF_INET6.
 * @param src	Points to a buffer holding an IPv4 address if the af argument
 *		is PJ_AF_INET, or an IPv6 address if the af argument is
 *		PJ_AF_INET6; the address must be in network byte order.  
 * @param dst	Points to a buffer where the function stores the resulting
 *		text string; it shall not be NULL.  
 * @param size	Specifies the size of this buffer, which shall be large 
 *		enough to hold the text string (PJ_INET_ADDRSTRLEN characters
 *		for IPv4, PJ_INET6_ADDRSTRLEN characters for IPv6).
 *
 * @return	PJ_SUCCESS if conversion was successful.
 */
PJ_DECL(pj_status_t) pj_inet_ntop(int af, const void *src,
				  char *dst, int size);

/**
 * Converts numeric address into its text string representation.
 * @see pj_sockaddr_print()
 *
 * @param af	Specify the family of the address. This can be PJ_AF_INET
 *		or PJ_AF_INET6.
 * @param src	Points to a buffer holding an IPv4 address if the af argument
 *		is PJ_AF_INET, or an IPv6 address if the af argument is
 *		PJ_AF_INET6; the address must be in network byte order.  
 * @param dst	Points to a buffer where the function stores the resulting
 *		text string; it shall not be NULL.  
 * @param size	Specifies the size of this buffer, which shall be large 
 *		enough to hold the text string (PJ_INET_ADDRSTRLEN characters
 *		for IPv4, PJ_INET6_ADDRSTRLEN characters for IPv6).
 *
 * @return	The address string or NULL if failed.
 */
PJ_DECL(char*) pj_inet_ntop2(int af, const void *src,
			     char *dst, int size);

/**
 * Print socket address.
 *
 * @param addr	The socket address.
 * @param buf	Text buffer.
 * @param size	Size of buffer.
 * @param flags	Bitmask combination of these value:
 *		  - 1: port number is included.
 *		  - 2: square bracket is included for IPv6 address.
 *
 * @return	The address string.
 */
PJ_DECL(char*) pj_sockaddr_print(const pj_sockaddr_t *addr,
				 char *buf, int size,
				 unsigned flags);

/**
 * Convert address string with numbers and dots to binary IP address.
 * 
 * @param cp	    The IP address in numbers and dots notation.
 * @return	    If success, the IP address is returned in network
 *		    byte order. If failed, PJ_INADDR_NONE will be
 *		    returned.
 * @remark
 * This is an obsolete interface to #pj_inet_aton(); it is obsolete
 * because -1 is a valid address (255.255.255.255), and #pj_inet_aton()
 * provides a cleaner way to indicate error return.
 */
PJ_DECL(pj_in_addr) pj_inet_addr(const pj_str_t *cp);

/**
 * Convert address string with numbers and dots to binary IP address.
 * 
 * @param cp	    The IP address in numbers and dots notation.
 * @return	    If success, the IP address is returned in network
 *		    byte order. If failed, PJ_INADDR_NONE will be
 *		    returned.
 * @remark
 * This is an obsolete interface to #pj_inet_aton(); it is obsolete
 * because -1 is a valid address (255.255.255.255), and #pj_inet_aton()
 * provides a cleaner way to indicate error return.
 */
PJ_DECL(pj_in_addr) pj_inet_addr2(const char *cp);

/**
 * Initialize IPv4 socket address based on the address and port info.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 *
 * @see pj_sockaddr_init()
 *
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    dotted numbers or a hostname to be resolved.
 * @param port	    The port number, in host byte order.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_sockaddr_in_init( pj_sockaddr_in *addr,
				          const pj_str_t *cp,
					  pj_uint16_t port);

/**
 * Initialize IP socket address based on the address and port info.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 *
 * @see pj_sockaddr_in_init()
 *
 * @param af	    Internet address family.
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    dotted numbers or a hostname to be resolved.
 * @param port	    The port number, in host byte order.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_sockaddr_init(int af, 
				      pj_sockaddr *addr,
				      const pj_str_t *cp,
				      pj_uint16_t port);

/**
 * Compare two socket addresses.
 *
 * @param addr1	    First address.
 * @param addr2	    Second address.
 *
 * @return	    Zero on equal, -1 if addr1 is less than addr2,
 *		    and +1 if addr1 is more than addr2.
 */
PJ_DECL(int) pj_sockaddr_cmp(const pj_sockaddr_t *addr1,
			     const pj_sockaddr_t *addr2);

/**
 * Get pointer to the address part of a socket address.
 * 
 * @param addr	    Socket address.
 *
 * @return	    Pointer to address part (sin_addr or sin6_addr,
 *		    depending on address family)
 */
PJ_DECL(void*) pj_sockaddr_get_addr(const pj_sockaddr_t *addr);

/**
 * Check that a socket address contains a non-zero address part.
 *
 * @param addr	    Socket address.
 *
 * @return	    Non-zero if address is set to non-zero.
 */
PJ_DECL(pj_bool_t) pj_sockaddr_has_addr(const pj_sockaddr_t *addr);

/**
 * Get the address part length of a socket address, based on its address
 * family. For PJ_AF_INET, the length will be sizeof(pj_in_addr), and
 * for PJ_AF_INET6, the length will be sizeof(pj_in6_addr).
 * 
 * @param addr	    Socket address.
 *
 * @return	    Length in bytes.
 */
PJ_DECL(unsigned) pj_sockaddr_get_addr_len(const pj_sockaddr_t *addr);

/**
 * Get the socket address length, based on its address
 * family. For PJ_AF_INET, the length will be sizeof(pj_sockaddr_in), and
 * for PJ_AF_INET6, the length will be sizeof(pj_sockaddr_in6).
 * 
 * @param addr	    Socket address.
 *
 * @return	    Length in bytes.
 */
PJ_DECL(unsigned) pj_sockaddr_get_len(const pj_sockaddr_t *addr);

/** 
 * Copy only the address part (sin_addr/sin6_addr) of a socket address.
 *
 * @param dst	    Destination socket address.
 * @param src	    Source socket address.
 *
 * @see @pj_sockaddr_cp()
 */
PJ_DECL(void) pj_sockaddr_copy_addr(pj_sockaddr *dst,
				    const pj_sockaddr *src);
/**
 * Copy socket address. This will copy the whole structure depending
 * on the address family of the source socket address.
 *
 * @param dst	    Destination socket address.
 * @param src	    Source socket address.
 *
 * @see @pj_sockaddr_copy_addr()
 */
PJ_DECL(void) pj_sockaddr_cp(pj_sockaddr_t *dst, const pj_sockaddr_t *src);

/**
 * Get the IP address of an IPv4 socket address.
 * The address is returned as 32bit value in host byte order.
 *
 * @param addr	    The IP socket address.
 * @return	    32bit address, in host byte order.
 */
PJ_DECL(pj_in_addr) pj_sockaddr_in_get_addr(const pj_sockaddr_in *addr);

/**
 * Set the IP address of an IPv4 socket address.
 *
 * @param addr	    The IP socket address.
 * @param hostaddr  The host address, in host byte order.
 */
PJ_DECL(void) pj_sockaddr_in_set_addr(pj_sockaddr_in *addr,
				      pj_uint32_t hostaddr);

/**
 * Set the IP address of an IP socket address from string address, 
 * with resolving the host if necessary. The string address may be in a
 * standard numbers and dots notation or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address.
 *
 * @see pj_sockaddr_set_str_addr()
 *
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    dotted numbers or a hostname to be resolved.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_sockaddr_in_set_str_addr( pj_sockaddr_in *addr,
					          const pj_str_t *cp);

/**
 * Set the IP address of an IPv4 or IPv6 socket address from string address,
 * with resolving the host if necessary. The string address may be in a
 * standard IPv6 or IPv6 address or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address according to the address family.
 *
 * @param af	    Address family.
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    IP numbers (IPv4 or IPv6) or a hostname to be resolved.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_sockaddr_set_str_addr(int af,
					      pj_sockaddr *addr,
					      const pj_str_t *cp);

/**
 * Get the port number of a socket address, in host byte order. 
 * This function can be used for both IPv4 and IPv6 socket address.
 * 
 * @param addr	    Socket address.
 *
 * @return	    Port number, in host byte order.
 */
PJ_DECL(pj_uint16_t) pj_sockaddr_get_port(const pj_sockaddr_t *addr);

/**
 * Get the transport layer port number of an Internet socket address.
 * The port is returned in host byte order.
 *
 * @param addr	    The IP socket address.
 * @return	    Port number, in host byte order.
 */
PJ_DECL(pj_uint16_t) pj_sockaddr_in_get_port(const pj_sockaddr_in *addr);

/**
 * Set the port number of an Internet socket address.
 *
 * @param addr	    The socket address.
 * @param hostport  The port number, in host byte order.
 */
PJ_DECL(pj_status_t) pj_sockaddr_set_port(pj_sockaddr *addr, 
					  pj_uint16_t hostport);

/**
 * Set the port number of an IPv4 socket address.
 *
 * @see pj_sockaddr_set_port()
 *
 * @param addr	    The IP socket address.
 * @param hostport  The port number, in host byte order.
 */
PJ_DECL(void) pj_sockaddr_in_set_port(pj_sockaddr_in *addr, 
				      pj_uint16_t hostport);

/**
 * Parse string containing IP address and optional port into socket address,
 * possibly also with address family detection. This function supports both
 * IPv4 and IPv6 parsing, however IPv6 parsing may only be done if IPv6 is
 * enabled during compilation.
 *
 * This function supports parsing several formats. Sample IPv4 inputs and
 * their default results::
 *  - "10.0.0.1:80": address 10.0.0.1 and port 80.
 *  - "10.0.0.1": address 10.0.0.1 and port zero.
 *  - "10.0.0.1:": address 10.0.0.1 and port zero.
 *  - "10.0.0.1:0": address 10.0.0.1 and port zero.
 *  - ":80": address 0.0.0.0 and port 80.
 *  - ":": address 0.0.0.0 and port 0.
 *  - "localhost": address 127.0.0.1 and port 0.
 *  - "localhost:": address 127.0.0.1 and port 0.
 *  - "localhost:80": address 127.0.0.1 and port 80.
 *
 * Sample IPv6 inputs and their default results:
 *  - "[fec0::01]:80": address fec0::01 and port 80
 *  - "[fec0::01]": address fec0::01 and port 0
 *  - "[fec0::01]:": address fec0::01 and port 0
 *  - "[fec0::01]:0": address fec0::01 and port 0
 *  - "fec0::01": address fec0::01 and port 0
 *  - "fec0::01:80": address fec0::01:80 and port 0
 *  - "::": address zero (::) and port 0
 *  - "[::]": address zero (::) and port 0
 *  - "[::]:": address zero (::) and port 0
 *  - ":::": address zero (::) and port 0
 *  - "[::]:80": address zero (::) and port 0
 *  - ":::80": address zero (::) and port 80
 *
 * Note: when the IPv6 socket address contains port number, the IP 
 * part of the socket address should be enclosed with square brackets, 
 * otherwise the port number will be included as part of the IP address
 * (see "fec0::01:80" example above).
 *
 * @param af	    Optionally specify the address family to be used. If the
 *		    address family is to be deducted from the input, specify
 *		    pj_AF_UNSPEC() here. Other supported values are
 *		    #pj_AF_INET() and #pj_AF_INET6()
 * @param options   Additional options to assist the parsing, must be zero
 *		    for now.
 * @param str	    The input string to be parsed.
 * @param addr	    Pointer to store the result.
 *
 * @return	    PJ_SUCCESS if the parsing is successful.
 *
 * @see pj_sockaddr_parse2()
 */
PJ_DECL(pj_status_t) pj_sockaddr_parse(int af, unsigned options,
				       const pj_str_t *str,
				       pj_sockaddr *addr);

/**
 * This function is similar to #pj_sockaddr_parse(), except that it will not
 * convert the hostpart into IP address (thus possibly resolving the hostname
 * into a #pj_sockaddr. 
 *
 * Unlike #pj_sockaddr_parse(), this function has a limitation that if port 
 * number is specified in an IPv6 input string, the IP part of the IPv6 socket
 * address MUST be enclosed in square brackets, otherwise the port number will
 * be considered as part of the IPv6 IP address.
 *
 * @param af	    Optionally specify the address family to be used. If the
 *		    address family is to be deducted from the input, specify
 *		    #pj_AF_UNSPEC() here. Other supported values are
 *		    #pj_AF_INET() and #pj_AF_INET6()
 * @param options   Additional options to assist the parsing, must be zero
 *		    for now.
 * @param str	    The input string to be parsed.
 * @param hostpart  Optional pointer to store the host part of the socket 
 *		    address, with any brackets removed.
 * @param port	    Optional pointer to store the port number. If port number
 *		    is not found, this will be set to zero upon return.
 * @param raf	    Optional pointer to store the detected address family of
 *		    the input address.
 *
 * @return	    PJ_SUCCESS if the parsing is successful.
 *
 * @see pj_sockaddr_parse()
 */
PJ_DECL(pj_status_t) pj_sockaddr_parse2(int af, unsigned options,
				        const pj_str_t *str,
				        pj_str_t *hostpart,
				        pj_uint16_t *port,
					int *raf);

/*****************************************************************************
 *
 * HOST NAME AND ADDRESS.
 *
 *****************************************************************************
 */

/**
 * Get system's host name.
 *
 * @return	    The hostname, or empty string if the hostname can not
 *		    be identified.
 */
PJ_DECL(const pj_str_t*) pj_gethostname(void);

/**
 * Get host's IP address, which the the first IP address that is resolved
 * from the hostname.
 *
 * @return	    The host's IP address, PJ_INADDR_NONE if the host
 *		    IP address can not be identified.
 */
PJ_DECL(pj_in_addr) pj_gethostaddr(void);


/*****************************************************************************
 *
 * SOCKET API.
 *
 *****************************************************************************
 */

/**
 * Create new socket/endpoint for communication.
 *
 * @param family    Specifies a communication domain; this selects the
 *		    protocol family which will be used for communication.
 * @param type	    The socket has the indicated type, which specifies the 
 *		    communication semantics.
 * @param protocol  Specifies  a  particular  protocol  to  be used with the
 *		    socket.  Normally only a single protocol exists to support 
 *		    a particular socket  type  within  a given protocol family, 
 *		    in which a case protocol can be specified as 0.
 * @param sock	    New socket descriptor, or PJ_INVALID_SOCKET on error.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_socket(int family, 
				    int type, 
				    int protocol,
				    pj_sock_t *sock);

/**
 * Close the socket descriptor.
 *
 * @param sockfd    The socket descriptor.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_close(pj_sock_t sockfd);


/**
 * This function gives the socket sockfd the local address my_addr. my_addr is
 * addrlen bytes long.  Traditionally, this is called assigning a name to
 * a socket. When a socket is created with #pj_sock_socket(), it exists in a
 * name space (address family) but has no name assigned.
 *
 * @param sockfd    The socket desriptor.
 * @param my_addr   The local address to bind the socket to.
 * @param addrlen   The length of the address.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_bind( pj_sock_t sockfd, 
				   const pj_sockaddr_t *my_addr,
				   int addrlen);

/**
 * Bind the IP socket sockfd to the given address and port.
 *
 * @param sockfd    The socket descriptor.
 * @param addr	    Local address to bind the socket to, in host byte order.
 * @param port	    The local port to bind the socket to, in host byte order.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_bind_in( pj_sock_t sockfd, 
				      pj_uint32_t addr,
				      pj_uint16_t port);

#if PJ_HAS_TCP
/**
 * Listen for incoming connection. This function only applies to connection
 * oriented sockets (such as PJ_SOCK_STREAM or PJ_SOCK_SEQPACKET), and it
 * indicates the willingness to accept incoming connections.
 *
 * @param sockfd	The socket descriptor.
 * @param backlog	Defines the maximum length the queue of pending
 *			connections may grow to.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_listen( pj_sock_t sockfd, 
				     int backlog );

/**
 * Accept new connection on the specified connection oriented server socket.
 *
 * @param serverfd  The server socket.
 * @param newsock   New socket on success, of PJ_INVALID_SOCKET if failed.
 * @param addr	    A pointer to sockaddr type. If the argument is not NULL,
 *		    it will be filled by the address of connecting entity.
 * @param addrlen   Initially specifies the length of the address, and upon
 *		    return will be filled with the exact address length.
 *
 * @return	    Zero on success, or the error number.
 */
PJ_DECL(pj_status_t) pj_sock_accept( pj_sock_t serverfd,
				     pj_sock_t *newsock,
				     pj_sockaddr_t *addr,
				     int *addrlen);
#endif

/**
 * The file descriptor sockfd must refer to a socket.  If the socket is of
 * type PJ_SOCK_DGRAM  then the serv_addr address is the address to which
 * datagrams are sent by default, and the only address from which datagrams
 * are received. If the socket is of type PJ_SOCK_STREAM or PJ_SOCK_SEQPACKET,
 * this call attempts to make a connection to another socket.  The
 * other socket is specified by serv_addr, which is an address (of length
 * addrlen) in the communications space of the  socket.  Each  communications
 * space interprets the serv_addr parameter in its own way.
 *
 * @param sockfd	The socket descriptor.
 * @param serv_addr	Server address to connect to.
 * @param addrlen	The length of server address.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_connect( pj_sock_t sockfd,
				      const pj_sockaddr_t *serv_addr,
				      int addrlen);

/**
 * Return the address of peer which is connected to socket sockfd.
 *
 * @param sockfd	The socket descriptor.
 * @param addr		Pointer to sockaddr structure to which the address
 *			will be returned.
 * @param namelen	Initially the length of the addr. Upon return the value
 *			will be set to the actual length of the address.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_getpeername(pj_sock_t sockfd,
					  pj_sockaddr_t *addr,
					  int *namelen);

/**
 * Return the current name of the specified socket.
 *
 * @param sockfd	The socket descriptor.
 * @param addr		Pointer to sockaddr structure to which the address
 *			will be returned.
 * @param namelen	Initially the length of the addr. Upon return the value
 *			will be set to the actual length of the address.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_getsockname( pj_sock_t sockfd,
					  pj_sockaddr_t *addr,
					  int *namelen);

/**
 * Get socket option associated with a socket. Options may exist at multiple
 * protocol levels; they are always present at the uppermost socket level.
 *
 * @param sockfd	The socket descriptor.
 * @param level		The level which to get the option from.
 * @param optname	The option name.
 * @param optval	Identifies the buffer which the value will be
 *			returned.
 * @param optlen	Initially contains the length of the buffer, upon
 *			return will be set to the actual size of the value.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_getsockopt( pj_sock_t sockfd,
					 pj_uint16_t level,
					 pj_uint16_t optname,
					 void *optval,
					 int *optlen);
/**
 * Manipulate the options associated with a socket. Options may exist at 
 * multiple protocol levels; they are always present at the uppermost socket 
 * level.
 *
 * @param sockfd	The socket descriptor.
 * @param level		The level which to get the option from.
 * @param optname	The option name.
 * @param optval	Identifies the buffer which contain the value.
 * @param optlen	The length of the value.
 *
 * @return		PJ_SUCCESS or the status code.
 */
PJ_DECL(pj_status_t) pj_sock_setsockopt( pj_sock_t sockfd,
					 pj_uint16_t level,
					 pj_uint16_t optname,
					 const void *optval,
					 int optlen);


/**
 * Receives data stream or message coming to the specified socket.
 *
 * @param sockfd	The socket descriptor.
 * @param buf		The buffer to receive the data or message.
 * @param len		On input, the length of the buffer. On return,
 *			contains the length of data received.
 * @param flags		Flags (such as pj_MSG_PEEK()).
 *
 * @return		PJ_SUCCESS or the error code.
 */
PJ_DECL(pj_status_t) pj_sock_recv(pj_sock_t sockfd,
				  void *buf,
				  pj_ssize_t *len,
				  unsigned flags);

/**
 * Receives data stream or message coming to the specified socket.
 *
 * @param sockfd	The socket descriptor.
 * @param buf		The buffer to receive the data or message.
 * @param len		On input, the length of the buffer. On return,
 *			contains the length of data received.
 * @param flags		Flags (such as pj_MSG_PEEK()).
 * @param from		If not NULL, it will be filled with the source
 *			address of the connection.
 * @param fromlen	Initially contains the length of from address,
 *			and upon return will be filled with the actual
 *			length of the address.
 *
 * @return		PJ_SUCCESS or the error code.
 */
PJ_DECL(pj_status_t) pj_sock_recvfrom( pj_sock_t sockfd,
				      void *buf,
				      pj_ssize_t *len,
				      unsigned flags,
				      pj_sockaddr_t *from,
				      int *fromlen);

/**
 * Transmit data to the socket.
 *
 * @param sockfd	Socket descriptor.
 * @param buf		Buffer containing data to be sent.
 * @param len		On input, the length of the data in the buffer.
 *			Upon return, it will be filled with the length
 *			of data sent.
 * @param flags		Flags (such as pj_MSG_DONTROUTE()).
 *
 * @return		PJ_SUCCESS or the status code.
 */
PJ_DECL(pj_status_t) pj_sock_send(pj_sock_t sockfd,
				  const void *buf,
				  pj_ssize_t *len,
				  unsigned flags);

/**
 * Transmit data to the socket to the specified address.
 *
 * @param sockfd	Socket descriptor.
 * @param buf		Buffer containing data to be sent.
 * @param len		On input, the length of the data in the buffer.
 *			Upon return, it will be filled with the length
 *			of data sent.
 * @param flags		Flags (such as pj_MSG_DONTROUTE()).
 * @param to		The address to send.
 * @param tolen		The length of the address in bytes.
 *
 * @return		PJ_SUCCESS or the status code.
 */
PJ_DECL(pj_status_t) pj_sock_sendto(pj_sock_t sockfd,
				    const void *buf,
				    pj_ssize_t *len,
				    unsigned flags,
				    const pj_sockaddr_t *to,
				    int tolen);

#if PJ_HAS_TCP
/**
 * The shutdown call causes all or part of a full-duplex connection on the
 * socket associated with sockfd to be shut down.
 *
 * @param sockfd	The socket descriptor.
 * @param how		If how is PJ_SHUT_RD, further receptions will be 
 *			disallowed. If how is PJ_SHUT_WR, further transmissions
 *			will be disallowed. If how is PJ_SHUT_RDWR, further 
 *			receptions andtransmissions will be disallowed.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_shutdown( pj_sock_t sockfd,
				       int how);
#endif

/**
 * Set TCP socket SO_KEEPALIVE option and tune the idle time to custom value.
 *
 * @param sock	The socket descriptor.
 * @param tcp_timeout		The time (in seconds) the connection needs to remain 
 *          idle before TCP starts sending keepalive probes, if the socket option 
 *          SO_KEEPALIVE has been set on this socket. This option should not be 
 *          used in code intended to be portable.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_sock_set_tcp_timeout(pj_sock_t *sock, 
											int tcp_timeout  //in milliseconds.
											);

/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJ_SOCK_H__ */


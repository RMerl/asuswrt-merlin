/* $Id: turn.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_TURN_SRV_TURN_H__
#define __PJ_TURN_SRV_TURN_H__

#include <pjlib.h>
#include <pjnath.h>

typedef struct pj_turn_relay_res    pj_turn_relay_res;
typedef struct pj_turn_listener	    pj_turn_listener;
typedef struct pj_turn_transport    pj_turn_transport;
typedef struct pj_turn_permission   pj_turn_permission;
typedef struct pj_turn_allocation   pj_turn_allocation;
typedef struct pj_turn_srv	    pj_turn_srv;
typedef struct pj_turn_pkt	    pj_turn_pkt;


#define PJ_TURN_INVALID_LIS_ID	    ((unsigned)-1)

/** 
 * Get transport type name string.
 */
PJ_DECL(const char*) pj_turn_tp_type_name(int tp_type);

/**
 * This structure describes TURN relay resource. An allocation allocates
 * one relay resource, and optionally it may reserve another resource.
 */
struct pj_turn_relay_res
{
    /** Hash table key */
    struct {
	/** Transport type. */
	int		    tp_type;

	/** Transport/relay address */
	pj_sockaddr	    addr;
    } hkey;

    /** Allocation who requested or reserved this resource. */
    pj_turn_allocation *allocation;

    /** Username used in credential */
    pj_str_t	    user;

    /** Realm used in credential. */
    pj_str_t	    realm;

    /** Lifetime, in seconds. */
    unsigned	    lifetime;

    /** Relay/allocation expiration time */
    pj_time_val	    expiry;

    /** Timeout timer entry */
    pj_timer_entry  timer;

    /** Transport. */
    struct {
	/** Transport/relay socket */
	pj_sock_t	    sock;

	/** Transport/relay ioqueue */
	pj_ioqueue_key_t    *key;

	/** Read operation key. */
	pj_ioqueue_op_key_t read_key;

	/** The incoming packet buffer */
	char		    rx_pkt[PJ_TURN_MAX_PKT_LEN];

	/** Source address of the packet. */
	pj_sockaddr	    src_addr;

	/** Source address length */
	int		    src_addr_len;

	/** The outgoing packet buffer. This must be 3wbit aligned. */
	char		    tx_pkt[PJ_TURN_MAX_PKT_LEN+4];
    } tp;
};


/****************************************************************************/
/*
 * TURN Allocation API
 */

/**
 * This structure describes key to lookup TURN allocations in the
 * allocation hash table.
 */
typedef struct pj_turn_allocation_key
{
    int		    tp_type;	/**< Transport type.	    */
    pj_sockaddr	    clt_addr;	/**< Client's address.	    */
} pj_turn_allocation_key;


/**
 * This structure describes TURN pj_turn_allocation session.
 */
struct pj_turn_allocation
{
    /** Hash table key to identify client. */
    pj_turn_allocation_key hkey;

    /** Pool for this allocation. */
    pj_pool_t		*pool;

    /** Object name for logging identification */
    char		*obj_name;

    /** Client info (IP address and port) */
    char		info[80];

    /** Mutex */
    pj_lock_t		*lock;

    /** Server instance. */
    pj_turn_srv		*server;

    /** Transport to send/receive packets to/from client. */
    pj_turn_transport	*transport;

    /** The relay resource for this allocation. */
    pj_turn_relay_res	relay;

    /** Relay resource reserved by this allocation, if any */
    pj_turn_relay_res	*resv;

    /** Requested bandwidth */
    unsigned		bandwidth;

    /** STUN session for this client */
    pj_stun_session	*sess;

    /** Credential for this STUN session. */
    pj_stun_auth_cred	 cred;

    /** Peer hash table (keyed by peer address) */
    pj_hash_table_t	*peer_table;

    /** Channel hash table (keyed by channel number) */
    pj_hash_table_t	*ch_table;
};


/**
 * This structure describes the hash table key to lookup TURN
 * permission.
 */
typedef struct pj_turn_permission_key
{
    /** Peer address. */
    pj_sockaddr		peer_addr;

} pj_turn_permission_key;


/**
 * This structure describes TURN pj_turn_permission or channel.
 */
struct pj_turn_permission
{
    /** Hash table key */
    pj_turn_permission_key hkey;

    /** TURN allocation that owns this permission/channel */
    pj_turn_allocation	*allocation;

    /** Optional channel number, or PJ_TURN_INVALID_CHANNEL if channel number
     *  is not requested for this permission. 
     */
    pj_uint16_t		channel;

    /** Permission expiration time. */
    pj_time_val		expiry;
};

/**
 * Create new allocation.
 */
PJ_DECL(pj_status_t) pj_turn_allocation_create(pj_turn_transport *transport,
					       const pj_sockaddr_t *src_addr,
					       unsigned src_addr_len,
					       const pj_stun_rx_data *rdata,
					       pj_stun_session *srv_sess,
					       pj_turn_allocation **p_alloc);
/**
 * Destroy allocation.
 */
PJ_DECL(void) pj_turn_allocation_destroy(pj_turn_allocation *alloc);


/**
 * Handle incoming packet from client.
 */
PJ_DECL(void) pj_turn_allocation_on_rx_client_pkt(pj_turn_allocation *alloc,
						  pj_turn_pkt *pkt);

/**
 * Handle transport closure.
 */
PJ_DECL(void) pj_turn_allocation_on_transport_closed(pj_turn_allocation *alloc,
						     pj_turn_transport *tp);

/****************************************************************************/
/*
 * TURN Listener API
 */

/**
 * This structure describes TURN listener socket. A TURN listener socket
 * listens for incoming connections from clients.
 */
struct pj_turn_listener
{
    /** Object name/identification */
    char		*obj_name;

    /** Slightly longer info about this listener */
    char		info[80];

    /** TURN server instance. */
    pj_turn_srv		*server;

    /** Listener index in the server */
    unsigned		id;

    /** Pool for this listener. */
    pj_pool_t	       *pool;

    /** Transport type. */
    int			tp_type;

    /** Bound address of this listener. */
    pj_sockaddr		addr;

    /** Socket. */
    pj_sock_t		sock;

    /** Flags. */
    unsigned		flags;

    /** Destroy handler */
    pj_status_t		(*destroy)(pj_turn_listener*);
};


/**
 * This structure describes TURN transport socket which is used to send and
 * receive packets towards client.
 */
struct pj_turn_transport
{
    /** Object name/identification */
    char		*obj_name;

    /** Slightly longer info about this listener */
    char		*info;

    /** Listener instance */
    pj_turn_listener	*listener;

    /** Sendto handler */
    pj_status_t		(*sendto)(pj_turn_transport *tp,
				  const void *packet,
				  pj_size_t size,
				  unsigned flag,
				  const pj_sockaddr_t *addr,
				  int addr_len);

    /** Addref handler */
    void		(*add_ref)(pj_turn_transport *tp,
				   pj_turn_allocation *alloc);

    /** Decref handler */
    void		(*dec_ref)(pj_turn_transport *tp,
				   pj_turn_allocation *alloc);

};


/**
 * An incoming packet.
 */
struct pj_turn_pkt
{
    /** Pool for this packet */
    pj_pool_t		    *pool;

    /** Transport where the packet was received. */
    pj_turn_transport	    *transport;

    /** Packet buffer (must be 32bit aligned). */
    pj_uint8_t		    pkt[PJ_TURN_MAX_PKT_LEN];

    /** Size of the packet */
    pj_size_t		    len;

    /** Arrival time. */
    pj_time_val		    rx_time;

    /** Source transport type and source address. */
    pj_turn_allocation_key   src;

    /** Source address length. */
    int			    src_addr_len;
};


/**
 * Create a UDP listener on the specified port.
 */
PJ_DECL(pj_status_t) pj_turn_listener_create_udp(pj_turn_srv *srv,
						 int af,
					         const pj_str_t *bound_addr,
					         unsigned port,
						 unsigned concurrency_cnt,
						 unsigned flags,
						 pj_turn_listener **p_lis);

/**
 * Create a TCP listener on the specified port.
 */
PJ_DECL(pj_status_t) pj_turn_listener_create_tcp(pj_turn_srv *srv,
						 int af,
					         const pj_str_t *bound_addr,
					         unsigned port,
						 unsigned concurrency_cnt,
						 unsigned flags,
						 pj_turn_listener **p_lis);

/**
 * Destroy listener.
 */
PJ_DECL(pj_status_t) pj_turn_listener_destroy(pj_turn_listener *listener);


/**
 * Add a reference to a transport.
 */
PJ_DECL(void) pj_turn_transport_add_ref(pj_turn_transport *transport,
					pj_turn_allocation *alloc);


/**
 * Decrement transport reference counter.
 */
PJ_DECL(void) pj_turn_transport_dec_ref(pj_turn_transport *transport,
					pj_turn_allocation *alloc);



/****************************************************************************/
/*
 * TURN Server API
 */
/**
 * This structure describes TURN pj_turn_srv instance.
 */
struct pj_turn_srv
{
    /** Object name */
    char	*obj_name;

    /** Core settings */
    struct {
	/** Pool factory */
	pj_pool_factory *pf;

	/** Pool for this server instance. */
	pj_pool_t       *pool;

	/** Global Ioqueue */
	pj_ioqueue_t    *ioqueue;

	/** Mutex */
	pj_lock_t	*lock;

	/** Global timer heap instance. */
	pj_timer_heap_t *timer_heap;

	/** Number of listeners */
	unsigned         lis_cnt;

	/** Array of listeners. */
	pj_turn_listener **listener;

	/** STUN session to handle initial Allocate request. */
	pj_stun_session	*stun_sess;

	/** Number of worker threads. */
	unsigned        thread_cnt;

	/** Array of worker threads. */
	pj_thread_t     **thread;

	/** Thread quit signal */
	pj_bool_t	quit;

	/** STUN config. */
	pj_stun_config	 stun_cfg;

	/** STUN auth credential. */
	pj_stun_auth_cred cred;

	/** Thread local ID for storing credential */
	long		 tls_key, tls_data;

    } core;

    
    /** Hash tables */
    struct {
	/** Allocations hash table, indexed by transport type and
	 *  client address. 
	 */
	pj_hash_table_t *alloc;

	/** Relay resource hash table, indexed by transport type and
	 *  relay address. 
	 */
	pj_hash_table_t *res;

    } tables;

    /** Ports settings */
    struct {
	/** Minimum UDP port number. */
	pj_uint16_t	    min_udp;

	/** Maximum UDP port number. */
	pj_uint16_t	    max_udp;

	/** Next UDP port number. */
	pj_uint16_t	    next_udp;


	/** Minimum TCP port number. */
	pj_uint16_t	    min_tcp;

	/** Maximum TCP port number. */
	pj_uint16_t	    max_tcp;

	/** Next TCP port number. */
	pj_uint16_t	    next_tcp;

    } ports;
};


/** 
 * Create server.
 */
PJ_DECL(pj_status_t) pj_turn_srv_create(pj_pool_factory *pf,
				        pj_turn_srv **p_srv);

/** 
 * Destroy server.
 */
PJ_DECL(pj_status_t) pj_turn_srv_destroy(pj_turn_srv *srv);

/** 
 * Add listener.
 */
PJ_DECL(pj_status_t) pj_turn_srv_add_listener(pj_turn_srv *srv,
					      pj_turn_listener *lis);

/**
 * Register an allocation.
 */
PJ_DECL(pj_status_t) pj_turn_srv_register_allocation(pj_turn_srv *srv,
						     pj_turn_allocation *alloc);

/**
 * Unregister an allocation.
 */
PJ_DECL(pj_status_t) pj_turn_srv_unregister_allocation(pj_turn_srv *srv,
						       pj_turn_allocation *alloc);

/**
 * This callback is called by UDP listener on incoming packet.
 */
PJ_DECL(void) pj_turn_srv_on_rx_pkt(pj_turn_srv *srv, 
				    pj_turn_pkt *pkt);


#endif	/* __PJ_TURN_SRV_TURN_H__ */


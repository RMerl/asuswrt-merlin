/* $Id: server.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_TEST_SERVER_H__
#define __PJNATH_TEST_SERVER_H__

#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>

#define DNS_SERVER_PORT	    55533
#define STUN_SERVER_PORT    33478
#define TURN_SERVER_PORT    33479

#define TURN_USERNAME	"auser"
#define TURN_PASSWD	"apass"

#define MAX_TURN_ALLOC	    16
#define MAX_TURN_PERM	    16

enum test_server_flags
{
    CREATE_DNS_SERVER		= (1 << 0),
    CREATE_A_RECORD_FOR_DOMAIN	= (1 << 1),

    CREATE_STUN_SERVER		= (1 << 5),
    CREATE_STUN_SERVER_DNS_SRV	= (1 << 6),

    CREATE_TURN_SERVER		= (1 << 10),
    CREATE_TURN_SERVER_DNS_SRV	= (1 << 11),

};

typedef struct test_server test_server;

/* TURN allocation */
typedef struct turn_allocation
{
    test_server		*test_srv;
    pj_pool_t		*pool;
    pj_activesock_t	*sock;
    pj_ioqueue_op_key_t	 send_key;
    pj_sockaddr		 client_addr;
    pj_sockaddr		 alloc_addr;
    unsigned		 perm_cnt;
    pj_sockaddr		 perm[MAX_TURN_PERM];
    unsigned		 chnum[MAX_TURN_PERM];
    pj_stun_msg		*data_ind;
} turn_allocation;

/*
 * Server installation for testing.
 * This comprises of DNS server, STUN server, and TURN server.
 */
struct test_server
{
    pj_pool_t		*pool;
    pj_uint32_t		 flags;
    pj_stun_config	*stun_cfg;
    pj_ioqueue_op_key_t	 send_key;

    pj_dns_server	*dns_server;

    pj_activesock_t	*stun_sock;

    pj_activesock_t	*turn_sock;
    unsigned		 turn_alloc_cnt;
    turn_allocation	 turn_alloc[MAX_TURN_ALLOC];
    pj_bool_t		 turn_respond_allocate;
    pj_bool_t		 turn_respond_refresh;

    struct turn_stat {
	unsigned	 rx_allocate_cnt;
	unsigned	 rx_refresh_cnt;
	unsigned	 rx_send_ind_cnt;
    } turn_stat;

    pj_str_t		 domain;
    pj_str_t		 username;
    pj_str_t		 passwd;

};


pj_status_t create_test_server(pj_stun_config *stun_cfg,
			       pj_uint32_t flags,
			       const char *domain,
			       test_server **p_test_srv);
void        destroy_test_server(test_server *test_srv);
void        test_server_poll_events(test_server *test_srv);


#endif	/* __PJNATH_TEST_SERVER_H__ */


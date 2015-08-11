/* $Id: main.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "turn.h"
#include "auth.h"

#define REALM		"pjsip.org"
//#define TURN_PORT	PJ_STUN_TURN_PORT
#define TURN_PORT	34780
#define LOG_LEVEL	4


static pj_caching_pool g_cp;

int err(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    printf("%s: %s\n", title, errmsg);
    return 1;
}

static void dump_status(pj_turn_srv *srv)
{
    char addr[80];
    pj_hash_iterator_t itbuf, *it;
    pj_time_val now;
    unsigned i;

    for (i=0; i<srv->core.lis_cnt; ++i) {
	pj_turn_listener *lis = srv->core.listener[i];
	printf("Server address : %s\n", lis->info);
    }

    printf("Worker threads : %d\n", srv->core.thread_cnt);
    printf("Total mem usage: %u.%03uMB\n", (unsigned)(g_cp.used_size / 1000000), 
	   (unsigned)((g_cp.used_size % 1000000)/1000));
    printf("UDP port range : %u %u %u (next/min/max)\n", srv->ports.next_udp,
	   srv->ports.min_udp, srv->ports.max_udp);
    printf("TCP port range : %u %u %u (next/min/max)\n", srv->ports.next_tcp,
	   srv->ports.min_tcp, srv->ports.max_tcp);
    printf("Clients #      : %u\n", pj_hash_count(srv->tables.alloc));

    puts("");

    if (pj_hash_count(srv->tables.alloc)==0) {
	return;
    }

    puts("#    Client addr.          Alloc addr.            Username Lftm Expy #prm #chl");
    puts("------------------------------------------------------------------------------");

    pj_gettimeofday(&now);

    it = pj_hash_first(srv->tables.alloc, &itbuf);
    i=1;
    while (it) {
	pj_turn_allocation *alloc = (pj_turn_allocation*) 
				    pj_hash_this(srv->tables.alloc, it);
	printf("%-3d %-22s %-22s %-8.*s %-4d %-4ld %-4d %-4d\n",
	       i,
	       alloc->info,
	       pj_sockaddr_print(&alloc->relay.hkey.addr, addr, sizeof(addr), 3),
	       (int)alloc->cred.data.static_cred.username.slen,
	       alloc->cred.data.static_cred.username.ptr,
	       alloc->relay.lifetime,
	       alloc->relay.expiry.sec - now.sec,
	       pj_hash_count(alloc->peer_table), 
	       pj_hash_count(alloc->ch_table));

	it = pj_hash_next(srv->tables.alloc, it);
	++i;
    }
}

static void menu(void)
{
    puts("");
    puts("Menu:");
    puts(" d   Dump status");
    puts(" q   Quit");
    printf(">> ");
}

static void console_main(pj_turn_srv *srv)
{
    pj_bool_t quit = PJ_FALSE;

    while (!quit) {
	char line[10];
	
	menu();
	    
	if (fgets(line, sizeof(line), stdin) == NULL)
	    break;

	switch (line[0]) {
	case 'd':
	    dump_status(srv);
	    break;
	case 'q':
	    quit = PJ_TRUE;
	    break;
	}
    }
}

int main()
{
    pj_turn_srv *srv;
    pj_turn_listener *listener;
    pj_status_t status;

    status = pj_init(0);
    if (status != PJ_SUCCESS)
	return err("pj_init() error", status);

    pjlib_util_init();
    pjnath_init();

    pj_caching_pool_init(0, &g_cp, NULL, 0);

    pj_turn_auth_init(REALM);

    status = pj_turn_srv_create(&g_cp.factory, &srv);
    if (status != PJ_SUCCESS)
	return err("Error creating server", status);

    status = pj_turn_listener_create_udp(srv, pj_AF_INET(), NULL, 
					 TURN_PORT, 1, 0, &listener);
    if (status != PJ_SUCCESS)
	return err("Error creating UDP listener", status);

#if PJ_HAS_TCP
    status = pj_turn_listener_create_tcp(srv, pj_AF_INET(), NULL, 
					 TURN_PORT, 1, 0, &listener);
    if (status != PJ_SUCCESS)
	return err("Error creating listener", status);
#endif

    status = pj_turn_srv_add_listener(srv, listener);
    if (status != PJ_SUCCESS)
	return err("Error adding listener", status);

    puts("Server is running");

    pj_log_set_level(0, LOG_LEVEL);

    console_main(srv);

    pj_turn_srv_destroy(srv);
    pj_caching_pool_destroy(&g_cp);
    pj_shutdown(0);

    return 0;
}


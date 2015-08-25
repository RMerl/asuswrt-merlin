/* $Id: client_main.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>


#define THIS_FILE	"client_main.c"
#define LOCAL_PORT	1998
#define BANDWIDTH	64		    /* -1 to disable */
#define LIFETIME	600		    /* -1 to disable */
#define REQ_TRANSPORT	-1		    /* 0: udp, 1: tcp, -1: disable */
#define REQ_PORT_PROPS	-1		    /* -1 to disable */
#define REQ_IP		0		    /* IP address string */

//#define OPTIONS		PJ_STUN_NO_AUTHENTICATE
#define OPTIONS		0


struct peer
{
    pj_stun_sock   *stun_sock;
    pj_sockaddr	    mapped_addr;
};


static struct global
{
    pj_caching_pool	 cp;
    pj_pool_t		*pool;
    pj_stun_config	 stun_config;
    pj_thread_t		*thread;
    pj_bool_t		 quit;

    pj_dns_resolver	*resolver;

    pj_turn_sock	*relay;
    pj_sockaddr		 relay_addr;

    struct peer		 peer[2];
} g;

static struct options
{
    pj_bool_t	 use_tcp;
    char	*srv_addr;
    char	*srv_port;
    char	*realm;
    char	*user_name;
    char	*password;
    pj_bool_t	 use_fingerprint;
    char	*stun_server;
    char	*nameserver;
} o;


static int worker_thread(void *unused);
static void turn_on_rx_data(pj_turn_sock *relay,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void turn_on_state(pj_turn_sock *relay, pj_turn_state_t old_state,
			  pj_turn_state_t new_state);
static pj_bool_t stun_sock_on_status(pj_stun_sock *stun_sock, 
				     pj_stun_sock_op op,
				     pj_status_t status);
static pj_bool_t stun_sock_on_rx_data(pj_stun_sock *stun_sock,
				      void *pkt,
				      unsigned pkt_len,
				      const pj_sockaddr_t *src_addr,
				      unsigned addr_len);


static void my_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(THIS_FILE, "%s: %s", title, errmsg));
}

#define CHECK(expr)	status=expr; \
			if (status!=PJ_SUCCESS) { \
			    my_perror(#expr, status); \
			    return status; \
			}

static int init()
{
    int i;
    pj_status_t status;

    CHECK( pj_init(0) );
    CHECK( pjlib_util_init() );
    CHECK( pjnath_init() );

    /* Check that server is specified */
    if (!o.srv_addr) {
	printf("Error: server must be specified\n");
	return PJ_EINVAL;
    }

    pj_caching_pool_init(0, &g.cp, &pj_pool_factory_default_policy, 0);

    g.pool = pj_pool_create(&g.cp.factory, "main", 1000, 1000, NULL);

    /* Init global STUN config */
    pj_stun_config_init(0, &g.stun_config, &g.cp.factory, 0, NULL, NULL);

    /* Create global timer heap */
    CHECK( pj_timer_heap_create(g.pool, 1000, &g.stun_config.timer_heap) );

    /* Create global ioqueue */
    CHECK( pj_ioqueue_create(g.pool, 16, &g.stun_config.ioqueue) );

    /* 
     * Create peers
     */
    for (i=0; i<(int)PJ_ARRAY_SIZE(g.peer); ++i) {
	pj_stun_sock_cb stun_sock_cb;
	char name[] = "peer0";
	pj_uint16_t port;
	pj_stun_sock_cfg ss_cfg;
	pj_str_t server;

	pj_bzero(&stun_sock_cb, sizeof(stun_sock_cb));
	stun_sock_cb.on_rx_data = &stun_sock_on_rx_data;
	stun_sock_cb.on_status = &stun_sock_on_status;

	g.peer[i].mapped_addr.addr.sa_family = pj_AF_INET();

	pj_stun_sock_cfg_default(&ss_cfg);
#if 1
	/* make reading the log easier */
	ss_cfg.ka_interval = 300;
#endif

	name[strlen(name)-1] = '0'+i;
	status = pj_stun_sock_create(&g.stun_config, name, pj_AF_INET(), 
				     &stun_sock_cb, &ss_cfg,
				     &g.peer[i], &g.peer[i].stun_sock);
	if (status != PJ_SUCCESS) {
	    my_perror("pj_stun_sock_create()", status);
	    return status;
	}

	if (o.stun_server) {
	    server = pj_str(o.stun_server);
	    port = PJ_STUN_PORT;
	} else {
	    server = pj_str(o.srv_addr);
	    port = (pj_uint16_t)(o.srv_port?atoi(o.srv_port):PJ_STUN_PORT);
	}
	status = pj_stun_sock_start(g.peer[i].stun_sock, &server, 
				    port,  NULL);
	if (status != PJ_SUCCESS) {
	    my_perror("pj_stun_sock_start()", status);
	    return status;
	}
    }

    /* Start the worker thread */
    CHECK( pj_thread_create(g.pool, "stun", &worker_thread, NULL, 0, 0, &g.thread) );


    return PJ_SUCCESS;
}


static int client_shutdown()
{
    unsigned i;

    if (g.thread) {
	g.quit = 1;
	pj_thread_join(g.thread);
	pj_thread_destroy(g.thread);
	g.thread = NULL;
    }
    if (g.relay) {
	pj_turn_sock_destroy(g.relay);
	g.relay = NULL;
    }
    for (i=0; i<PJ_ARRAY_SIZE(g.peer); ++i) {
	if (g.peer[i].stun_sock) {
	    pj_stun_sock_destroy(g.peer[i].stun_sock);
	    g.peer[i].stun_sock = NULL;
	}
    }
    if (g.stun_config.timer_heap) {
	pj_timer_heap_destroy(g.stun_config.timer_heap);
	g.stun_config.timer_heap = NULL;
    }
    if (g.stun_config.ioqueue) {
	pj_ioqueue_destroy(g.stun_config.ioqueue);
	g.stun_config.ioqueue = NULL;
    }
    if (g.pool) {
	pj_pool_release(g.pool);
	g.pool = NULL;
    }
    pj_pool_factory_dump(&g.cp.factory, PJ_TRUE);
    pj_caching_pool_destroy(&g.cp);

    return PJ_SUCCESS;
}


static int worker_thread(void *unused)
{
    PJ_UNUSED_ARG(unused);

    while (!g.quit) {
	const pj_time_val delay = {0, 10};

	/* Poll ioqueue for the TURN client */
	pj_ioqueue_poll(g.stun_config.ioqueue, &delay);

	/* Poll the timer heap */
	pj_timer_heap_poll(g.stun_config.timer_heap, NULL);

    }

    return 0;
}

static pj_status_t create_relay(void)
{
    pj_turn_sock_cb rel_cb;
    pj_stun_auth_cred cred;
    pj_str_t srv;
    pj_status_t status;

    if (g.relay) {
	PJ_LOG(1,(THIS_FILE, "Relay already created"));
	return -1;
    }

    /* Create DNS resolver if configured */
    if (o.nameserver) {
	pj_str_t ns = pj_str(o.nameserver);

	status = pj_dns_resolver_create(&g.cp.factory, "resolver", 0, 
					g.stun_config.timer_heap, 
					g.stun_config.ioqueue, &g.resolver);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "Error creating resolver (err=%d)", status));
	    return status;
	}

	status = pj_dns_resolver_set_ns(g.resolver, 1, &ns, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "Error configuring nameserver (err=%d)", status));
	    return status;
	}
    }

    pj_bzero(&rel_cb, sizeof(rel_cb));
    rel_cb.on_rx_data = &turn_on_rx_data;
    rel_cb.on_state = &turn_on_state;
    CHECK( pj_turn_sock_create(&g.stun_config, pj_AF_INET(), 
			       (o.use_tcp? PJ_TURN_TP_TCP : PJ_TURN_TP_UDP),
			       &rel_cb, 0,
			       NULL, &g.relay) );

    if (o.user_name) {
	pj_bzero(&cred, sizeof(cred));
	cred.type = PJ_STUN_AUTH_CRED_STATIC;
	cred.data.static_cred.realm = pj_str(o.realm);
	cred.data.static_cred.username = pj_str(o.user_name);
	cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
	cred.data.static_cred.data = pj_str(o.password);
	//cred.data.static_cred.nonce = pj_str(o.nonce);
    } else {
	PJ_LOG(2,(THIS_FILE, "Warning: no credential is set"));
    }

    srv = pj_str(o.srv_addr);
    CHECK(pj_turn_sock_alloc(g.relay,				 /* the relay */
			    &srv,				 /* srv addr */
			    (o.srv_port?atoi(o.srv_port):PJ_STUN_PORT),/* def port */
			    g.resolver,				 /* resolver */
			    (o.user_name?&cred:NULL),		 /* credential */
			    NULL)				 /* alloc param */
			    );

    return PJ_SUCCESS;
}

static void destroy_relay(void)
{
    if (g.relay) {
	pj_turn_sock_destroy(g.relay);
    }
}


static void turn_on_rx_data(pj_turn_sock *relay,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    char addrinfo[80];

    pj_sockaddr_print(peer_addr, addrinfo, sizeof(addrinfo), 3);

    PJ_LOG(3,(THIS_FILE, "Client received %d bytes data from %s: %.*s",
	      pkt_len, addrinfo, pkt_len, pkt));
}


static void turn_on_state(pj_turn_sock *relay, pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    PJ_LOG(3,(THIS_FILE, "State %s --> %s", pj_turn_state_name(old_state), 
	      pj_turn_state_name(new_state)));

    if (new_state == PJ_TURN_STATE_READY) {
	pj_turn_session_info info;
	pj_turn_sock_get_info(relay, &info);
	pj_memcpy(&g.relay_addr, &info.relay_addr, sizeof(pj_sockaddr));
    } else if (new_state > PJ_TURN_STATE_READY && g.relay) {
	PJ_LOG(3,(THIS_FILE, "Relay shutting down.."));
	g.relay = NULL;
    }
}

static pj_bool_t stun_sock_on_status(pj_stun_sock *stun_sock, 
				     pj_stun_sock_op op,
				     pj_status_t status)
{
    struct peer *peer = (struct peer*) pj_stun_sock_get_user_data(stun_sock);

    if (status == PJ_SUCCESS) {
	PJ_LOG(4,(THIS_FILE, "peer%d: %s success", peer-g.peer,
		  pj_stun_sock_op_name(op)));
    } else {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(1,(THIS_FILE, "peer%d: %s error: %s", peer-g.peer,
		  pj_stun_sock_op_name(op), errmsg));
	return PJ_FALSE;
    }

    if (op==PJ_STUN_SOCK_BINDING_OP || op==PJ_STUN_SOCK_KEEP_ALIVE_OP) {
	pj_stun_sock_info info;
	int cmp;

	pj_stun_sock_get_info(stun_sock, &info);
	cmp = pj_sockaddr_cmp(&info.mapped_addr, &peer->mapped_addr);

	if (cmp) {
	    char straddr[PJ_INET6_ADDRSTRLEN+10];

	    pj_sockaddr_cp(&peer->mapped_addr, &info.mapped_addr);
	    pj_sockaddr_print(&peer->mapped_addr, straddr, sizeof(straddr), 3);
	    PJ_LOG(3,(THIS_FILE, "peer%d: STUN mapped address is %s",
		      peer-g.peer, straddr));
	}
    }

    return PJ_TRUE;
}

static pj_bool_t stun_sock_on_rx_data(pj_stun_sock *stun_sock,
				      void *pkt,
				      unsigned pkt_len,
				      const pj_sockaddr_t *src_addr,
				      unsigned addr_len)
{
    struct peer *peer = (struct peer*) pj_stun_sock_get_user_data(stun_sock);
    char straddr[PJ_INET6_ADDRSTRLEN+10];

    ((char*)pkt)[pkt_len] = '\0';

    pj_sockaddr_print(src_addr, straddr, sizeof(straddr), 3);
    PJ_LOG(3,(THIS_FILE, "peer%d: received %d bytes data from %s: %s",
	      peer-g.peer, pkt_len, straddr, (char*)pkt));

    return PJ_TRUE;
}


static void menu(void)
{
    pj_turn_session_info info;
    char client_state[20], relay_addr[80], peer0_addr[80], peer1_addr[80];

    if (g.relay) {
	pj_turn_sock_get_info(g.relay, &info);
	strcpy(client_state, pj_turn_state_name(info.state));
	if (info.state >= PJ_TURN_STATE_READY)
	    pj_sockaddr_print(&info.relay_addr, relay_addr, sizeof(relay_addr), 3);
	else
	    strcpy(relay_addr, "0.0.0.0:0");
    } else {
	strcpy(client_state, "NULL");
	strcpy(relay_addr, "0.0.0.0:0");
    }

    pj_sockaddr_print(&g.peer[0].mapped_addr, peer0_addr, sizeof(peer0_addr), 3);
    pj_sockaddr_print(&g.peer[1].mapped_addr, peer1_addr, sizeof(peer1_addr), 3);


    puts("\n");
    puts("+=====================================================================+");
    puts("|             CLIENT                 |             PEER-0             |");
    puts("|                                    |                                |");
    printf("| State     : %-12s           | Address: %-21s |\n",
	   client_state, peer0_addr);
    printf("| Relay addr: %-21s  |                                |\n",
	   relay_addr);
    puts("|                                    | 0  Send data to relay address  |");
    puts("| a      Allocate relay              |                                |");
    puts("| p,pp   Set permission for peer 0/1 +--------------------------------+");
    puts("| s,ss   Send data to peer 0/1       |             PEER-1             |");
    puts("| b,bb   BindChannel to peer 0/1     |                                |");
    printf("| x      Delete allocation           | Address: %-21s |\n",
	  peer1_addr);
    puts("+------------------------------------+                                |");
    puts("| q  Quit                  d  Dump   | 1  Send data to relay adderss  |");
    puts("+------------------------------------+--------------------------------+");
    printf(">>> ");
    fflush(stdout);
}


static void console_main(void)
{
    while (!g.quit) {
	char input[32];
	struct peer *peer;
	pj_status_t status;

	menu();

	if (fgets(input, sizeof(input), stdin) == NULL)
	    break;
	
	switch (input[0]) {
	case 'a':
	    create_relay();
	    break;
	case 'd':
	    pj_pool_factory_dump(&g.cp.factory, PJ_TRUE);
	    break;
	case 's':
	    if (g.relay == NULL) {
		puts("Error: no relay");
		continue;
	    }
	    if (input[1]!='s')
		peer = &g.peer[0];
	    else
		peer = &g.peer[1];

	    strcpy(input, "Hello from client");
	    status = pj_turn_sock_sendto(g.relay, (const pj_uint8_t*)input, 
					strlen(input)+1, 
					&peer->mapped_addr, 
					pj_sockaddr_get_len(&peer->mapped_addr));
	    if (status != PJ_SUCCESS)
		my_perror("turn_udp_sendto() failed", status);
	    break;
	case 'b':
	    if (g.relay == NULL) {
		puts("Error: no relay");
		continue;
	    }
	    if (input[1]!='b')
		peer = &g.peer[0];
	    else
		peer = &g.peer[1];

	    status = pj_turn_sock_bind_channel(g.relay, &peer->mapped_addr,
					      pj_sockaddr_get_len(&peer->mapped_addr));
	    if (status != PJ_SUCCESS)
		my_perror("turn_udp_bind_channel() failed", status);
	    break;
	case 'p':
	    if (g.relay == NULL) {
		puts("Error: no relay");
		continue;
	    }
	    if (input[1]!='p')
		peer = &g.peer[0];
	    else
		peer = &g.peer[1];

	    status = pj_turn_sock_set_perm(g.relay, 1, &peer->mapped_addr, 1);
	    if (status != PJ_SUCCESS)
		my_perror("pj_turn_sock_set_perm() failed", status);
	    break;
	case 'x':
	    if (g.relay == NULL) {
		puts("Error: no relay");
		continue;
	    }
	    destroy_relay();
	    break;
	case '0':
	case '1':
	    if (g.relay == NULL) {
		puts("No relay");
		break;
	    }
	    peer = &g.peer[input[0]-'0'];
	    sprintf(input, "Hello from peer%d", input[0]-'0');
	    pj_stun_sock_sendto(peer->stun_sock, NULL, input, strlen(input)+1, 0,
				&g.relay_addr, pj_sockaddr_get_len(&g.relay_addr));
	    break;
	case 'q':
	    g.quit = PJ_TRUE;
	    break;
	}
    }
}


static void usage(void)
{
    puts("Usage: pjturn_client TURN-SERVER [OPTIONS]");
    puts("");
    puts("where TURN-SERVER is \"host[:port]\"");
    puts("");
    puts("and OPTIONS:");
    puts(" --tcp, -T             Use TCP to connect to TURN server");
    puts(" --realm, -r REALM     Set realm of the credential to REALM");
    puts(" --username, -u UID    Set username of the credential to UID");
    puts(" --password, -p PASSWD Set password of the credential to PASSWD");
    puts(" --fingerprint, -F     Use fingerprint for outgoing requests");
    puts(" --stun-srv, -S  NAME  Use this STUN srv instead of TURN for Binding discovery");
    puts(" --nameserver, -N IP   Activate DNS SRV, use this DNS server");
    puts(" --help, -h");
}

int main(int argc, char *argv[])
{
    struct pj_getopt_option long_options[] = {
	{ "realm",	1, 0, 'r'},
	{ "username",	1, 0, 'u'},
	{ "password",	1, 0, 'p'},
	{ "fingerprint",0, 0, 'F'},
	{ "tcp",        0, 0, 'T'},
	{ "help",	0, 0, 'h'},
	{ "stun-srv",   1, 0, 'S'},
	{ "nameserver", 1, 0, 'N'}
    };
    int c, opt_id;
    char *pos;
    pj_status_t status;

    while((c=pj_getopt_long(argc,argv, "r:u:p:S:N:hFT", long_options, &opt_id))!=-1) {
	switch (c) {
	case 'r':
	    o.realm = pj_optarg;
	    break;
	case 'u':
	    o.user_name = pj_optarg;
	    break;
	case 'p':
	    o.password = pj_optarg;
	    break;
	case 'h':
	    usage();
	    return 0;
	case 'F':
	    o.use_fingerprint = PJ_TRUE;
	    break;
	case 'T':
	    o.use_tcp = PJ_TRUE;
	    break;
	case 'S':
	    o.stun_server = pj_optarg;
	    break;
	case 'N':
	    o.nameserver = pj_optarg;
	    break;
	default:
	    printf("Argument \"%s\" is not valid. Use -h to see help",
		   argv[pj_optind]);
	    return 1;
	}
    }

    if (pj_optind == argc) {
	puts("Error: TARGET is needed");
	usage();
	return 1;
    }

    if ((pos=pj_ansi_strchr(argv[pj_optind], ':')) != NULL) {
	o.srv_addr = argv[pj_optind];
	*pos = '\0';
	o.srv_port = pos+1;
    } else {
	o.srv_addr = argv[pj_optind];
    }

    if ((status=init()) != 0)
	goto on_return;
    
    //if ((status=create_relay()) != 0)
    //	goto on_return;
    
    console_main();

on_return:
    client_shutdown();
    return status ? 1 : 0;
}


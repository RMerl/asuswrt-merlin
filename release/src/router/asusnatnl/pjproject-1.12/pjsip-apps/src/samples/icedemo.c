/* $Id: icedemo.c 4387 2013-02-27 10:16:08Z ming $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <stdio.h>
#include <stdlib.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>


#define THIS_FILE   "icedemo.c"

/* For this demo app, configure longer STUN keep-alive time
 * so that it does't clutter the screen output.
 */
#define KA_INTERVAL 300


/* This is our global variables */
static struct app_t
{
    /* Command line options are stored here */
    struct options
    {
	unsigned    comp_cnt;
	pj_str_t    ns;
	int	    max_host;
	pj_bool_t   regular;
	pj_str_t    stun_srv;
	pj_str_t    turn_srv;
	pj_bool_t   turn_tcp;
	pj_str_t    turn_username;
	pj_str_t    turn_password;
	pj_bool_t   turn_fingerprint;
	const char *log_file;
    } opt;

    /* Our global variables */
    pj_caching_pool	 cp;
    pj_pool_t		*pool;
    pj_thread_t		*thread;
    pj_bool_t		 thread_quit_flag;
    pj_ice_strans_cfg	 ice_cfg;
    pj_ice_strans	*icest;
    FILE		*log_fhnd;

    /* Variables to store parsed remote ICE info */
    struct rem_info
    {
	char		 ufrag[80];
	char		 pwd[80];
	unsigned	 comp_cnt;
	pj_sockaddr	 def_addr[PJ_ICE_MAX_COMP];
	unsigned	 cand_cnt;
	pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
    } rem;

} icedemo;

/* Utility to display error messages */
static void icedemo_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(THIS_FILE, "%s: %s", title, errmsg));
}

/* Utility: display error message and exit application (usually
 * because of fatal error.
 */
static void err_exit(const char *title, pj_status_t status)
{
    if (status != PJ_SUCCESS) {
	icedemo_perror(title, status);
    }
    PJ_LOG(3,(THIS_FILE, "Shutting down.."));

    if (icedemo.icest)
	pj_ice_strans_destroy(icedemo.icest);
    
    pj_thread_sleep(500);

    icedemo.thread_quit_flag = PJ_TRUE;
    if (icedemo.thread) {
	pj_thread_join(icedemo.thread);
	pj_thread_destroy(icedemo.thread);
    }

    if (icedemo.ice_cfg.stun_cfg.ioqueue)
	pj_ioqueue_destroy(icedemo.ice_cfg.stun_cfg.ioqueue);

    if (icedemo.ice_cfg.stun_cfg.timer_heap)
	pj_timer_heap_destroy(icedemo.ice_cfg.stun_cfg.timer_heap);

    pj_caching_pool_destroy(&icedemo.cp);

    pj_shutdown(0);

    if (icedemo.log_fhnd) {
	fclose(icedemo.log_fhnd);
	icedemo.log_fhnd = NULL;
    }

    exit(status != PJ_SUCCESS);
}

#define CHECK(expr)	status=expr; \
			if (status!=PJ_SUCCESS) { \
			    err_exit(#expr, status); \
			}

/*
 * This function checks for events from both timer and ioqueue (for
 * network events). It is invoked by the worker thread.
 */
static pj_status_t handle_events(unsigned max_msec, unsigned *p_count)
{
    enum { MAX_NET_EVENTS = 1 };
    pj_time_val max_timeout = {0, 0};
    pj_time_val timeout = { 0, 0};
    unsigned count = 0, net_event_count = 0;
    int c;

    max_timeout.msec = max_msec;

    /* Poll the timer to run it and also to retrieve the earliest entry. */
    timeout.sec = timeout.msec = 0;
    c = pj_timer_heap_poll( icedemo.ice_cfg.stun_cfg.timer_heap, &timeout );
    if (c > 0)
	count += c;

    /* timer_heap_poll should never ever returns negative value, or otherwise
     * ioqueue_poll() will block forever!
     */
    pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
    if (timeout.msec >= 1000) timeout.msec = 999;

    /* compare the value with the timeout to wait from timer, and use the 
     * minimum value. 
    */
    if (PJ_TIME_VAL_GT(timeout, max_timeout))
	timeout = max_timeout;

    /* Poll ioqueue. 
     * Repeat polling the ioqueue while we have immediate events, because
     * timer heap may process more than one events, so if we only process
     * one network events at a time (such as when IOCP backend is used),
     * the ioqueue may have trouble keeping up with the request rate.
     *
     * For example, for each send() request, one network event will be
     *   reported by ioqueue for the send() completion. If we don't poll
     *   the ioqueue often enough, the send() completion will not be
     *   reported in timely manner.
     */
    do {
	c = pj_ioqueue_poll( icedemo.ice_cfg.stun_cfg.ioqueue, &timeout);
	if (c < 0) {
	    pj_status_t err = pj_get_netos_error();
	    pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
	    if (p_count)
		*p_count = count;
	    return err;
	} else if (c == 0) {
	    break;
	} else {
	    net_event_count += c;
	    timeout.sec = timeout.msec = 0;
	}
    } while (c > 0 && net_event_count < MAX_NET_EVENTS);

    count += net_event_count;
    if (p_count)
	*p_count = count;

    return PJ_SUCCESS;

}

/*
 * This is the worker thread that polls event in the background.
 */
static int icedemo_worker_thread(void *unused)
{
    PJ_UNUSED_ARG(unused);

    while (!icedemo.thread_quit_flag) {
	handle_events(500, NULL);
    }

    return 0;
}

/*
 * This is the callback that is registered to the ICE stream transport to
 * receive notification about incoming data. By "data" it means application
 * data such as RTP/RTCP, and not packets that belong to ICE signaling (such
 * as STUN connectivity checks or TURN signaling).
 */
static void cb_on_rx_data(pj_ice_strans *ice_st,
			  unsigned comp_id, 
			  void *pkt, pj_size_t size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len)
{
    char ipstr[PJ_INET6_ADDRSTRLEN+10];

    PJ_UNUSED_ARG(ice_st);
    PJ_UNUSED_ARG(src_addr_len);
    PJ_UNUSED_ARG(pkt);

    // Don't do this! It will ruin the packet buffer in case TCP is used!
    //((char*)pkt)[size] = '\0';

    PJ_LOG(3,(THIS_FILE, "Component %d: received %d bytes data from %s: \"%.*s\"",
	      comp_id, size,
	      pj_sockaddr_print(src_addr, ipstr, sizeof(ipstr), 3),
	      (unsigned)size,
	      (char*)pkt));
}

/*
 * This is the callback that is registered to the ICE stream transport to
 * receive notification about ICE state progression.
 */
static void cb_on_ice_complete(pj_ice_strans *ice_st, 
			       pj_ice_strans_op op,
			       pj_status_t status)
{
    const char *opname = 
	(op==PJ_ICE_STRANS_OP_INIT? "initialization" :
	    (op==PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation" : "unknown_op"));

    if (status == PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE, "ICE %s successful", opname));
    } else {
	char errmsg[PJ_ERR_MSG_SIZE];

	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(1,(THIS_FILE, "ICE %s failed: %s", opname, errmsg));
	pj_ice_strans_destroy(ice_st);
	icedemo.icest = NULL;
    }
}

/* log callback to write to file */
static void log_func(int inst_id, int level, const char *data, int len)
{
    pj_log_write(inst_id, level, data, len, 0);
    if (icedemo.log_fhnd) {
	if (fwrite(data, len, 1, icedemo.log_fhnd) != 1)
	    return;
    }
}

/*
 * This is the main application initialization function. It is called
 * once (and only once) during application initialization sequence by 
 * main().
 */
static pj_status_t icedemo_init(void)
{
    pj_status_t status;

    if (icedemo.opt.log_file) {
	icedemo.log_fhnd = fopen(icedemo.opt.log_file, "a");
	pj_log_set_log_func(&log_func);
    }

    /* Initialize the libraries before anything else */
    CHECK( pj_init(0) );
    CHECK( pjlib_util_init() );
    CHECK( pjnath_init() );

    /* Must create pool factory, where memory allocations come from */
    pj_caching_pool_init(0, &icedemo.cp, NULL, 0);

    /* Init our ICE settings with null values */
    pj_ice_strans_cfg_default(&icedemo.ice_cfg);

    icedemo.ice_cfg.stun_cfg.pf = &icedemo.cp.factory;

    /* Create application memory pool */
    icedemo.pool = pj_pool_create(&icedemo.cp.factory, "icedemo", 
				  512, 512, NULL);

    /* Create timer heap for timer stuff */
    CHECK( pj_timer_heap_create(icedemo.pool, 100, 
				&icedemo.ice_cfg.stun_cfg.timer_heap) );

    /* and create ioqueue for network I/O stuff */
    CHECK( pj_ioqueue_create(icedemo.pool, 16, 
			     &icedemo.ice_cfg.stun_cfg.ioqueue) );

    /* something must poll the timer heap and ioqueue, 
     * unless we're on Symbian where the timer heap and ioqueue run
     * on themselves.
     */
    CHECK( pj_thread_create(icedemo.pool, "icedemo", &icedemo_worker_thread,
			    NULL, 0, 0, &icedemo.thread) );

    icedemo.ice_cfg.af = pj_AF_INET();

    /* Create DNS resolver if nameserver is set */
    if (icedemo.opt.ns.slen) {
	CHECK( pj_dns_resolver_create(&icedemo.cp.factory, 
				      "resolver", 
				      0, 
				      icedemo.ice_cfg.stun_cfg.timer_heap,
				      icedemo.ice_cfg.stun_cfg.ioqueue, 
				      &icedemo.ice_cfg.resolver) );

	CHECK( pj_dns_resolver_set_ns(icedemo.ice_cfg.resolver, 1, 
				      &icedemo.opt.ns, NULL) );
    }

    /* -= Start initializing ICE stream transport config =- */

    /* Maximum number of host candidates */
    if (icedemo.opt.max_host != -1)
	icedemo.ice_cfg.stun.max_host_cands = icedemo.opt.max_host;

    /* Nomination strategy */
    if (icedemo.opt.regular)
	icedemo.ice_cfg.opt.aggressive = PJ_FALSE;
    else
	icedemo.ice_cfg.opt.aggressive = PJ_TRUE;

    /* Configure STUN/srflx candidate resolution */
    if (icedemo.opt.stun_srv.slen) {
	char *pos;

	/* Command line option may contain port number */
	if ((pos=pj_strchr(&icedemo.opt.stun_srv, ':')) != NULL) {
	    icedemo.ice_cfg.stun.server.ptr = icedemo.opt.stun_srv.ptr;
	    icedemo.ice_cfg.stun.server.slen = (pos - icedemo.opt.stun_srv.ptr);

	    icedemo.ice_cfg.stun.port = (pj_uint16_t)atoi(pos+1);
	} else {
	    icedemo.ice_cfg.stun.server = icedemo.opt.stun_srv;
	    icedemo.ice_cfg.stun.port = PJ_STUN_PORT;
	}

	/* For this demo app, configure longer STUN keep-alive time
	 * so that it does't clutter the screen output.
	 */
	icedemo.ice_cfg.stun.cfg.ka_interval = KA_INTERVAL;
    }

    /* Configure TURN candidate */
    if (icedemo.opt.turn_srv.slen) {
	char *pos;

	/* Command line option may contain port number */
	if ((pos=pj_strchr(&icedemo.opt.turn_srv, ':')) != NULL) {
	    icedemo.ice_cfg.turn.server.ptr = icedemo.opt.turn_srv.ptr;
	    icedemo.ice_cfg.turn.server.slen = (pos - icedemo.opt.turn_srv.ptr);

	    icedemo.ice_cfg.turn.port = (pj_uint16_t)atoi(pos+1);
	} else {
	    icedemo.ice_cfg.turn.server = icedemo.opt.turn_srv;
	    icedemo.ice_cfg.turn.port = PJ_STUN_PORT;
	}

	/* TURN credential */
	icedemo.ice_cfg.turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	icedemo.ice_cfg.turn.auth_cred.data.static_cred.username = icedemo.opt.turn_username;
	icedemo.ice_cfg.turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
	icedemo.ice_cfg.turn.auth_cred.data.static_cred.data = icedemo.opt.turn_password;

	/* Connection type to TURN server */
	if (icedemo.opt.turn_tcp)
	    icedemo.ice_cfg.turn.conn_type = PJ_TURN_TP_TCP;
	else
	    icedemo.ice_cfg.turn.conn_type = PJ_TURN_TP_UDP;

	/* For this demo app, configure longer keep-alive time
	 * so that it does't clutter the screen output.
	 */
	icedemo.ice_cfg.turn.alloc_param.ka_interval = KA_INTERVAL;
    }

    /* -= That's it for now, initialization is complete =- */
    return PJ_SUCCESS;
}


/*
 * Create ICE stream transport instance, invoked from the menu.
 */
static void icedemo_create_instance(void)
{
    pj_ice_strans_cb icecb;
    pj_status_t status;

    if (icedemo.icest != NULL) {
	puts("ICE instance already created, destroy it first");
	return;
    }

    /* init the callback */
    pj_bzero(&icecb, sizeof(icecb));
    icecb.on_rx_data = cb_on_rx_data;
    icecb.on_ice_complete = cb_on_ice_complete;

    /* create the instance */
    status = pj_ice_strans_create("icedemo",		    /* object name  */
				&icedemo.ice_cfg,	    /* settings	    */
				icedemo.opt.comp_cnt,	    /* comp_cnt	    */
				NULL,			    /* user data    */
				&icecb,			    /* callback	    */
				&icedemo.icest)		    /* instance ptr */
				;
    if (status != PJ_SUCCESS)
	icedemo_perror("error creating ice", status);
    else
	PJ_LOG(3,(THIS_FILE, "ICE instance successfully created"));
}

/* Utility to nullify parsed remote info */
static void reset_rem_info(void)
{
    pj_bzero(&icedemo.rem, sizeof(icedemo.rem));
}


/*
 * Destroy ICE stream transport instance, invoked from the menu.
 */
static void icedemo_destroy_instance(void)
{
    if (icedemo.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    pj_ice_strans_destroy(icedemo.icest);
    icedemo.icest = NULL;

    reset_rem_info();

    PJ_LOG(3,(THIS_FILE, "ICE instance destroyed"));
}


/*
 * Create ICE session, invoked from the menu.
 */
static void icedemo_init_session(unsigned rolechar)
{
    pj_ice_sess_role role = (pj_tolower((pj_uint8_t)rolechar)=='o' ? 
				PJ_ICE_SESS_ROLE_CONTROLLING : 
				PJ_ICE_SESS_ROLE_CONTROLLED);
    pj_status_t status;

    if (icedemo.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    if (pj_ice_strans_has_sess(icedemo.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: Session already created"));
	return;
    }

    status = pj_ice_strans_init_ice(icedemo.icest, role, NULL, NULL);
    if (status != PJ_SUCCESS)
	icedemo_perror("error creating session", status);
    else
	PJ_LOG(3,(THIS_FILE, "ICE session created"));

    reset_rem_info();
}


/*
 * Stop/destroy ICE session, invoked from the menu.
 */
static void icedemo_stop_session(void)
{
    pj_status_t status;

    if (icedemo.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    if (!pj_ice_strans_has_sess(icedemo.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE session, initialize first"));
	return;
    }

    status = pj_ice_strans_stop_ice(icedemo.icest);
    if (status != PJ_SUCCESS)
	icedemo_perror("error stopping session", status);
    else
	PJ_LOG(3,(THIS_FILE, "ICE session stopped"));

    reset_rem_info();
}

#define PRINT(fmt, arg0, arg1, arg2, arg3, arg4, arg5)	    \
	printed = pj_ansi_snprintf(p, maxlen - (p-buffer),  \
				   fmt, arg0, arg1, arg2, arg3, arg4, arg5); \
	if (printed <= 0) return -PJ_ETOOSMALL; \
	p += printed


/* Utility to create a=candidate SDP attribute */
static int print_cand(char buffer[], unsigned maxlen,
		      const pj_ice_sess_cand *cand)
{
    char ipaddr[PJ_INET6_ADDRSTRLEN];
    char *p = buffer;
    int printed;

    PRINT("a=candidate:%.*s %u UDP %u %s %u typ ",
	  (int)cand->foundation.slen,
	  cand->foundation.ptr,
	  (unsigned)cand->comp_id,
	  cand->prio,
	  pj_sockaddr_print(&cand->addr, ipaddr, 
			    sizeof(ipaddr), 0),
	  (unsigned)pj_sockaddr_get_port(&cand->addr));

    PRINT("%s\n",
	  pj_ice_get_cand_type_name(cand->type),
	  0, 0, 0, 0, 0);

    if (p == buffer+maxlen)
	return -PJ_ETOOSMALL;

    *p = '\0';

    return p-buffer;
}

/* 
 * Encode ICE information in SDP.
 */
static int encode_session(char buffer[], unsigned maxlen)
{
    char *p = buffer;
    unsigned comp;
    int printed;
    pj_str_t local_ufrag, local_pwd;
    pj_status_t status;

    /* Write "dummy" SDP v=, o=, s=, and t= lines */
    PRINT("v=0\no=- 3414953978 3414953978 IN IP4 localhost\ns=ice\nt=0 0\n", 
	  0, 0, 0, 0, 0, 0);

    /* Get ufrag and pwd from current session */
    pj_ice_strans_get_ufrag_pwd(icedemo.icest, &local_ufrag, &local_pwd,
				NULL, NULL);

    /* Write the a=ice-ufrag and a=ice-pwd attributes */
    PRINT("a=ice-ufrag:%.*s\na=ice-pwd:%.*s\n",
	   (int)local_ufrag.slen,
	   local_ufrag.ptr,
	   (int)local_pwd.slen,
	   local_pwd.ptr, 
	   0, 0);

    /* Write each component */
    for (comp=0; comp<icedemo.opt.comp_cnt; ++comp) {
	unsigned j, cand_cnt;
	pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
	char ipaddr[PJ_INET6_ADDRSTRLEN];

	/* Get default candidate for the component */
	status = pj_ice_strans_get_def_cand(icedemo.icest, comp+1, &cand[0]);
	if (status != PJ_SUCCESS)
	    return -status;

	/* Write the default address */
	if (comp==0) {
	    /* For component 1, default address is in m= and c= lines */
	    PRINT("m=audio %d RTP/AVP 0\n"
		  "c=IN IP4 %s\n",
		  (int)pj_sockaddr_get_port(&cand[0].addr),
		  pj_sockaddr_print(&cand[0].addr, ipaddr,
				    sizeof(ipaddr), 0),
		  0, 0, 0, 0);
	} else if (comp==1) {
	    /* For component 2, default address is in a=rtcp line */
	    PRINT("a=rtcp:%d IN IP4 %s\n",
		  (int)pj_sockaddr_get_port(&cand[0].addr),
		  pj_sockaddr_print(&cand[0].addr, ipaddr,
				    sizeof(ipaddr), 0),
		  0, 0, 0, 0);
	} else {
	    /* For other components, we'll just invent this.. */
	    PRINT("a=Xice-defcand:%d IN IP4 %s\n",
		  (int)pj_sockaddr_get_port(&cand[0].addr),
		  pj_sockaddr_print(&cand[0].addr, ipaddr,
				    sizeof(ipaddr), 0),
		  0, 0, 0, 0);
	}

	/* Enumerate all candidates for this component */
	cand_cnt = PJ_ARRAY_SIZE(cand);
	status = pj_ice_strans_enum_cands(icedemo.icest, comp+1,
					  &cand_cnt, cand);
	if (status != PJ_SUCCESS)
	    return -status;

	/* And encode the candidates as SDP */
	for (j=0; j<cand_cnt; ++j) {
	    printed = print_cand(p, maxlen - (p-buffer), &cand[j]);
	    if (printed < 0)
		return -PJ_ETOOSMALL;
	    p += printed;
	}
    }

    if (p == buffer+maxlen)
	return -PJ_ETOOSMALL;

    *p = '\0';
    return p - buffer;
}


/*
 * Show information contained in the ICE stream transport. This is
 * invoked from the menu.
 */
static void icedemo_show_ice(void)
{
    static char buffer[1000];
    int len;

    if (icedemo.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    puts("General info");
    puts("---------------");
    printf("Component count    : %d\n", icedemo.opt.comp_cnt);
    printf("Status             : ");
    if (pj_ice_strans_sess_is_complete(icedemo.icest))
	puts("negotiation complete");
    else if (pj_ice_strans_sess_is_running(icedemo.icest))
	puts("negotiation is in progress");
    else if (pj_ice_strans_has_sess(icedemo.icest))
	puts("session ready");
    else
	puts("session not created");

    if (!pj_ice_strans_has_sess(icedemo.icest)) {
	puts("Create the session first to see more info");
	return;
    }

    printf("Negotiated comp_cnt: %d\n", 
	   pj_ice_strans_get_running_comp_cnt(icedemo.icest));
    printf("Role               : %s\n",
	   pj_ice_strans_get_role(icedemo.icest)==PJ_ICE_SESS_ROLE_CONTROLLED ?
	   "controlled" : "controlling");

    len = encode_session(buffer, sizeof(buffer));
    if (len < 0)
	err_exit("not enough buffer to show ICE status", -len);

    puts("");
    printf("Local SDP (paste this to remote host):\n"
	   "--------------------------------------\n"
	   "%s\n", buffer);


    puts("");
    puts("Remote info:\n"
	 "----------------------");
    if (icedemo.rem.cand_cnt==0) {
	puts("No remote info yet");
    } else {
	unsigned i;

	printf("Remote ufrag       : %s\n", icedemo.rem.ufrag);
	printf("Remote password    : %s\n", icedemo.rem.pwd);
	printf("Remote cand. cnt.  : %d\n", icedemo.rem.cand_cnt);

	for (i=0; i<icedemo.rem.cand_cnt; ++i) {
	    len = print_cand(buffer, sizeof(buffer), &icedemo.rem.cand[i]);
	    if (len < 0)
		err_exit("not enough buffer to show ICE status", -len);

	    printf("  %s", buffer);
	}
    }
}


/*
 * Input and parse SDP from the remote (containing remote's ICE information) 
 * and save it to global variables.
 */
static void icedemo_input_remote(void)
{
    char linebuf[80];
    unsigned media_cnt = 0;
    unsigned comp0_port = 0;
    char     comp0_addr[80];
    pj_bool_t done = PJ_FALSE;

    puts("Paste SDP from remote host, end with empty line");

    reset_rem_info();

    comp0_addr[0] = '\0';

    while (!done) {
	int len;
	char *line;

	printf(">");
	if (stdout) fflush(stdout);

	if (fgets(linebuf, sizeof(linebuf), stdin)==NULL)
	    break;

	len = strlen(linebuf);
	while (len && (linebuf[len-1] == '\r' || linebuf[len-1] == '\n'))
	    linebuf[--len] = '\0';

	line = linebuf;
	while (len && pj_isspace(*line))
	    ++line, --len;

	if (len==0)
	    break;

	/* Ignore subsequent media descriptors */
	if (media_cnt > 1)
	    continue;

	switch (line[0]) {
	case 'm':
	    {
		int cnt;
		char media[32], portstr[32];

		++media_cnt;
		if (media_cnt > 1) {
		    puts("Media line ignored");
		    break;
		}

		cnt = sscanf(line+2, "%s %s RTP/", media, portstr);
		if (cnt != 2) {
		    PJ_LOG(1,(THIS_FILE, "Error parsing media line"));
		    goto on_error;
		}

		comp0_port = atoi(portstr);
		
	    }
	    break;
	case 'c':
	    {
		int cnt;
		char c[32], net[32], ip[80];
		
		cnt = sscanf(line+2, "%s %s %s", c, net, ip);
		if (cnt != 3) {
		    PJ_LOG(1,(THIS_FILE, "Error parsing connection line"));
		    goto on_error;
		}

		strcpy(comp0_addr, ip);
	    }
	    break;
	case 'a':
	    {
		char *attr = strtok(line+2, ": \t\r\n");
		if (strcmp(attr, "ice-ufrag")==0) {
		    strcpy(icedemo.rem.ufrag, attr+strlen(attr)+1);
		} else if (strcmp(attr, "ice-pwd")==0) {
		    strcpy(icedemo.rem.pwd, attr+strlen(attr)+1);
		} else if (strcmp(attr, "rtcp")==0) {
		    char *val = attr+strlen(attr)+1;
		    int af, cnt;
		    int port;
		    char net[32], ip[64];
		    pj_str_t tmp_addr;
		    pj_status_t status;

		    cnt = sscanf(val, "%d IN %s %s", &port, net, ip);
		    if (cnt != 3) {
			PJ_LOG(1,(THIS_FILE, "Error parsing rtcp attribute"));
			goto on_error;
		    }

		    if (strchr(ip, ':'))
			af = pj_AF_INET6();
		    else
			af = pj_AF_INET();

		    pj_sockaddr_init(af, &icedemo.rem.def_addr[1], NULL, 0);
		    tmp_addr = pj_str(ip);
		    status = pj_sockaddr_set_str_addr(af, &icedemo.rem.def_addr[1],
						      &tmp_addr);
		    if (status != PJ_SUCCESS) {
			PJ_LOG(1,(THIS_FILE, "Invalid IP address"));
			goto on_error;
		    }
		    pj_sockaddr_set_port(&icedemo.rem.def_addr[1], (pj_uint16_t)port);

		} else if (strcmp(attr, "candidate")==0) {
		    char *sdpcand = attr+strlen(attr)+1;
		    int af, cnt;
		    char foundation[32], transport[12], ipaddr[80], type[32];
		    pj_str_t tmpaddr;
		    int comp_id, prio, port;
		    pj_ice_sess_cand *cand;
		    pj_status_t status;

		    cnt = sscanf(sdpcand, "%s %d %s %d %s %d typ %s",
				 foundation,
				 &comp_id,
				 transport,
				 &prio,
				 ipaddr,
				 &port,
				 type);
		    if (cnt != 7) {
			PJ_LOG(1, (THIS_FILE, "error: Invalid ICE candidate line"));
			goto on_error;
		    }

		    cand = &icedemo.rem.cand[icedemo.rem.cand_cnt];
		    pj_bzero(cand, sizeof(*cand));
		    
		    if (strcmp(type, "host")==0)
			cand->type = PJ_ICE_CAND_TYPE_HOST;
		    else if (strcmp(type, "srflx")==0)
			cand->type = PJ_ICE_CAND_TYPE_SRFLX;
		    else if (strcmp(type, "relay")==0)
			cand->type = PJ_ICE_CAND_TYPE_RELAYED;
		    else {
			PJ_LOG(1, (THIS_FILE, "Error: invalid candidate type '%s'", 
				   type));
			goto on_error;
		    }

		    cand->comp_id = (pj_uint8_t)comp_id;
		    pj_strdup2(icedemo.pool, &cand->foundation, foundation);
		    cand->prio = prio;
		    
		    if (strchr(ipaddr, ':'))
			af = pj_AF_INET6();
		    else
			af = pj_AF_INET();

		    tmpaddr = pj_str(ipaddr);
		    pj_sockaddr_init(af, &cand->addr, NULL, 0);
		    status = pj_sockaddr_set_str_addr(af, &cand->addr, &tmpaddr);
		    if (status != PJ_SUCCESS) {
			PJ_LOG(1,(THIS_FILE, "Error: invalid IP address '%s'",
				  ipaddr));
			goto on_error;
		    }

		    pj_sockaddr_set_port(&cand->addr, (pj_uint16_t)port);

		    ++icedemo.rem.cand_cnt;

		    if (cand->comp_id > icedemo.rem.comp_cnt)
			icedemo.rem.comp_cnt = cand->comp_id;
		}
	    }
	    break;
	}
    }

    if (icedemo.rem.cand_cnt==0 ||
	icedemo.rem.ufrag[0]==0 ||
	icedemo.rem.pwd[0]==0 ||
	icedemo.rem.comp_cnt == 0)
    {
	PJ_LOG(1, (THIS_FILE, "Error: not enough info"));
	goto on_error;
    }

    if (comp0_port==0 || comp0_addr[0]=='\0') {
	PJ_LOG(1, (THIS_FILE, "Error: default address for component 0 not found"));
	goto on_error;
    } else {
	int af;
	pj_str_t tmp_addr;
	pj_status_t status;

	if (strchr(comp0_addr, ':'))
	    af = pj_AF_INET6();
	else
	    af = pj_AF_INET();

	pj_sockaddr_init(af, &icedemo.rem.def_addr[0], NULL, 0);
	tmp_addr = pj_str(comp0_addr);
	status = pj_sockaddr_set_str_addr(af, &icedemo.rem.def_addr[0],
					  &tmp_addr);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "Invalid IP address in c= line"));
	    goto on_error;
	}
	pj_sockaddr_set_port(&icedemo.rem.def_addr[0], (pj_uint16_t)comp0_port);
    }

    PJ_LOG(3, (THIS_FILE, "Done, %d remote candidate(s) added", 
	       icedemo.rem.cand_cnt));
    return;

on_error:
    reset_rem_info();
}


/*
 * Start ICE negotiation! This function is invoked from the menu.
 */
static void icedemo_start_nego(void)
{
    pj_str_t rufrag, rpwd;
    pj_status_t status;

    if (icedemo.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    if (!pj_ice_strans_has_sess(icedemo.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE session, initialize first"));
	return;
    }

    if (icedemo.rem.cand_cnt == 0) {
	PJ_LOG(1,(THIS_FILE, "Error: No remote info, input remote info first"));
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Starting ICE negotiation.."));

    status = pj_ice_strans_start_ice(icedemo.icest, 
				     pj_cstr(&rufrag, icedemo.rem.ufrag),
				     pj_cstr(&rpwd, icedemo.rem.pwd),
				     icedemo.rem.cand_cnt,
				     icedemo.rem.cand);
    if (status != PJ_SUCCESS)
	icedemo_perror("Error starting ICE", status);
    else
	PJ_LOG(3,(THIS_FILE, "ICE negotiation started"));
}


/*
 * Send application data to remote agent.
 */
static void icedemo_send_data(unsigned comp_id, const char *data)
{
    pj_status_t status;

    if (icedemo.icest == NULL) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE instance, create it first"));
	return;
    }

    if (!pj_ice_strans_has_sess(icedemo.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: No ICE session, initialize first"));
	return;
    }

    /*
    if (!pj_ice_strans_sess_is_complete(icedemo.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: ICE negotiation has not been started or is in progress"));
	return;
    }
    */

    if (comp_id<1||comp_id>pj_ice_strans_get_running_comp_cnt(icedemo.icest)) {
	PJ_LOG(1,(THIS_FILE, "Error: invalid component ID"));
	return;
    }

    status = pj_ice_strans_sendto(icedemo.icest, comp_id, data, strlen(data),
				  &icedemo.rem.def_addr[comp_id-1],
				  pj_sockaddr_get_len(&icedemo.rem.def_addr[comp_id-1]));
    if (status != PJ_SUCCESS)
	icedemo_perror("Error sending data", status);
    else
	PJ_LOG(3,(THIS_FILE, "Data sent"));
}


/*
 * Display help for the menu.
 */
static void icedemo_help_menu(void)
{
    puts("");
    puts("-= Help on using ICE and this icedemo program =-");
    puts("");
    puts("This application demonstrates how to use ICE in pjnath without having\n"
	 "to use the SIP protocol. To use this application, you will need to run\n"
	 "two instances of this application, to simulate two ICE agents.\n");

    puts("Basic ICE flow:\n"
	 " create instance [menu \"c\"]\n"
	 " repeat these steps as wanted:\n"
	 "   - init session as offerer or answerer [menu \"i\"]\n"
	 "   - display our SDP [menu \"s\"]\n"
	 "   - \"send\" our SDP from the \"show\" output above to remote, by\n"
	 "     copy-pasting the SDP to the other icedemo application\n"
	 "   - parse remote SDP, by pasting SDP generated by the other icedemo\n"
	 "     instance [menu \"r\"]\n"
	 "   - begin ICE negotiation in our end [menu \"b\"], and \n"
	 "   - immediately begin ICE negotiation in the other icedemo instance\n"
	 "   - ICE negotiation will run, and result will be printed to screen\n"
	 "   - send application data to remote [menu \"x\"]\n"
	 "   - end/stop ICE session [menu \"e\"]\n"
	 " destroy instance [menu \"d\"]\n"
	 "");

    puts("");
    puts("This concludes the help screen.");
    puts("");
}


/*
 * Display console menu
 */
static void icedemo_print_menu(void)
{
    puts("");
    puts("+----------------------------------------------------------------------+");
    puts("|                    M E N U                                           |");
    puts("+---+------------------------------------------------------------------+");
    puts("| c | create           Create the instance                             |");
    puts("| d | destroy          Destroy the instance                            |");
    puts("| i | init o|a         Initialize ICE session as offerer or answerer   |");
    puts("| e | stop             End/stop ICE session                            |");
    puts("| s | show             Display local ICE info                          |");
    puts("| r | remote           Input remote ICE info                           |");
    puts("| b | start            Begin ICE negotiation                           |");
    puts("| x | send <compid> .. Send data to remote                             |");
    puts("+---+------------------------------------------------------------------+");
    puts("| h |  help            * Help! *                                       |");
    puts("| q |  quit            Quit                                            |");
    puts("+----------------------------------------------------------------------+");
}


/*
 * Main console loop.
 */
static void icedemo_console(void)
{
    pj_bool_t app_quit = PJ_FALSE;

    while (!app_quit) {
	char input[80], *cmd;
	const char *SEP = " \t\r\n";
	int len;

	icedemo_print_menu();

	printf("Input: ");
	if (stdout) fflush(stdout);

	pj_bzero(input, sizeof(input));
	if (fgets(input, sizeof(input), stdin) == NULL)
	    break;

	len = strlen(input);
	while (len && (input[len-1]=='\r' || input[len-1]=='\n'))
	    input[--len] = '\0';

	cmd = strtok(input, SEP);
	if (!cmd)
	    continue;

	if (strcmp(cmd, "create")==0 || strcmp(cmd, "c")==0) {

	    icedemo_create_instance();

	} else if (strcmp(cmd, "destroy")==0 || strcmp(cmd, "d")==0) {

	    icedemo_destroy_instance();

	} else if (strcmp(cmd, "init")==0 || strcmp(cmd, "i")==0) {

	    char *role = strtok(NULL, SEP);
	    if (role)
		icedemo_init_session(*role);
	    else
		puts("error: Role required");

	} else if (strcmp(cmd, "stop")==0 || strcmp(cmd, "e")==0) {

	    icedemo_stop_session();

	} else if (strcmp(cmd, "show")==0 || strcmp(cmd, "s")==0) {

	    icedemo_show_ice();

	} else if (strcmp(cmd, "remote")==0 || strcmp(cmd, "r")==0) {

	    icedemo_input_remote();

	} else if (strcmp(cmd, "start")==0 || strcmp(cmd, "b")==0) {

	    icedemo_start_nego();

	} else if (strcmp(cmd, "send")==0 || strcmp(cmd, "x")==0) {

	    char *comp = strtok(NULL, SEP);

	    if (!comp) {
		PJ_LOG(1,(THIS_FILE, "Error: component ID required"));
	    } else {
		char *data = comp + strlen(comp) + 1;
		if (!data)
		    data = "";
		icedemo_send_data(atoi(comp), data);
	    }

	} else if (strcmp(cmd, "help")==0 || strcmp(cmd, "h")==0) {

	    icedemo_help_menu();

	} else if (strcmp(cmd, "quit")==0 || strcmp(cmd, "q")==0) {

	    app_quit = PJ_TRUE;

	} else {

	    printf("Invalid command '%s'\n", cmd);

	}
    }
}


/*
 * Display program usage.
 */
static void icedemo_usage()
{
    puts("Usage: icedemo [optons]");
    printf("icedemo v%s by pjsip.org\n", pj_get_version());
    puts("");
    puts("General options:");
    puts(" --comp-cnt, -c N          Component count (default=1)");
    puts(" --nameserver, -n IP       Configure nameserver to activate DNS SRV");
    puts("                           resolution");
    puts(" --max-host, -H N          Set max number of host candidates to N");
    puts(" --regular, -R             Use regular nomination (default aggressive)");
    puts(" --log-file, -L FILE       Save output to log FILE");
    puts(" --help, -h                Display this screen.");
    puts("");
    puts("STUN related options:");
    puts(" --stun-srv, -s HOSTDOM    Enable srflx candidate by resolving to STUN server.");
    puts("                           HOSTDOM may be a \"host_or_ip[:port]\" or a domain");
    puts("                           name if DNS SRV resolution is used.");
    puts("");
    puts("TURN related options:");
    puts(" --turn-srv, -t HOSTDOM    Enable relayed candidate by using this TURN server.");
    puts("                           HOSTDOM may be a \"host_or_ip[:port]\" or a domain");
    puts("                           name if DNS SRV resolution is used.");
    puts(" --turn-tcp, -T            Use TCP to connect to TURN server");
    puts(" --turn-username, -u UID   Set TURN username of the credential to UID");
    puts(" --turn-password, -p PWD   Set password of the credential to WPWD");
    puts(" --turn-fingerprint, -F    Use fingerprint for outgoing TURN requests");
    puts("");
}


/*
 * And here's the main()
 */
int main(int argc, char *argv[])
{
    struct pj_getopt_option long_options[] = {
	{ "comp-cnt",           1, 0, 'c'},
	{ "nameserver",		1, 0, 'n'},
	{ "max-host",		1, 0, 'H'},
	{ "help",		0, 0, 'h'},
	{ "stun-srv",		1, 0, 's'},
	{ "turn-srv",		1, 0, 't'},
	{ "turn-tcp",		0, 0, 'T'},
	{ "turn-username",	1, 0, 'u'},
	{ "turn-password",	1, 0, 'p'},
	{ "turn-fingerprint",	0, 0, 'F'},
	{ "regular",		0, 0, 'R'},
	{ "log-file",		1, 0, 'L'},
    };
    int c, opt_id;
    pj_status_t status;

    icedemo.opt.comp_cnt = 1;
    icedemo.opt.max_host = -1;

    while((c=pj_getopt_long(argc,argv, "c:n:s:t:u:p:H:L:hTFR", long_options, &opt_id))!=-1) {
	switch (c) {
	case 'c':
	    icedemo.opt.comp_cnt = atoi(pj_optarg);
	    if (icedemo.opt.comp_cnt < 1 || icedemo.opt.comp_cnt >= PJ_ICE_MAX_COMP) {
		puts("Invalid component count value");
		return 1;
	    }
	    break;
	case 'n':
	    icedemo.opt.ns = pj_str(pj_optarg);
	    break;
	case 'H':
	    icedemo.opt.max_host = atoi(pj_optarg);
	    break;
	case 'h':
	    icedemo_usage();
	    return 0;
	case 's':
	    icedemo.opt.stun_srv = pj_str(pj_optarg);
	    break;
	case 't':
	    icedemo.opt.turn_srv = pj_str(pj_optarg);
	    break;
	case 'T':
	    icedemo.opt.turn_tcp = PJ_TRUE;
	    break;
	case 'u':
	    icedemo.opt.turn_username = pj_str(pj_optarg);
	    break;
	case 'p':
	    icedemo.opt.turn_password = pj_str(pj_optarg);
	    break;
	case 'F':
	    icedemo.opt.turn_fingerprint = PJ_TRUE;
	    break;
	case 'R':
	    icedemo.opt.regular = PJ_TRUE;
	    break;
	case 'L':
	    icedemo.opt.log_file = pj_optarg;
	    break;
	default:
	    printf("Argument \"%s\" is not valid. Use -h to see help",
		   argv[pj_optind]);
	    return 1;
	}
    }

    status = icedemo_init();
    if (status != PJ_SUCCESS)
	return 1;

    icedemo_console();

    err_exit("Quitting..", PJ_SUCCESS);
    return 0;
}

/* $Id: sipstateless.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/**
 * sipcore.c
 *
 * A simple program to respond any incoming requests (except ACK, of course!)
 * with any status code (taken from command line argument, with the default
 * is 501/Not Implemented).
 */


/* Include all headers. */
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>


/* If this macro is set, UDP transport will be initialized at port 5060 */
#define HAS_UDP_TRANSPORT

/* If this macro is set, TCP transport will be initialized at port 5060 */
#define HAS_TCP_TRANSPORT   (1 && PJ_HAS_TCP)

/* Log identification */
#define THIS_FILE	"sipstateless.c"


/* Global SIP endpoint */
static pjsip_endpoint *sip_endpt;

/* What response code to be sent (default is 501/Not Implemented) */
static int code = PJSIP_SC_NOT_IMPLEMENTED;

/* Additional header list */
struct pjsip_hdr hdr_list;

/* usage() */
static void usage(void)
{
    puts("Usage:");
    puts("  sipstateless [code] [-H HDR] ..");
    puts("");
    puts("Options:");
    puts("  code     SIP status code to send (default: 501/Not Implemented");
    puts("  -H HDR   Specify additional headers to send with response");
    puts("           This option may be specified more than once.");
    puts("           Example:");
    puts("              -H 'Expires: 300' -H 'Contact: <sip:localhost>'"); 
}


/* Callback to handle incoming requests. */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata )
{
    /* Respond (statelessly) all incoming requests (except ACK!) 
     * with 501 (Not Implemented)
     */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
	pjsip_endpt_respond_stateless( sip_endpt, rdata, 
				       code, NULL,
				       &hdr_list, NULL);
    }
    return PJ_TRUE;
}



/*
 * main()
 *
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pj_pool_t *pool = NULL;
    pjsip_module mod_app =
    {
	NULL, NULL,		    /* prev, next.		*/
	{ "mod-app", 7 },	    /* Name.			*/
	-1,				    /* Id		*/
	PJSIP_MOD_PRIORITY_APPLICATION, /* Priority		*/
	NULL,			    /* load()			*/
	NULL,			    /* start()			*/
	NULL,			    /* stop()			*/
	NULL,			    /* unload()			*/
	&on_rx_request,		    /* on_rx_request()		*/
	NULL,			    /* on_rx_response()		*/
	NULL,			    /* on_tx_request.		*/
	NULL,			    /* on_tx_response()		*/
	NULL,			    /* on_tsx_state()		*/
    };
    int c;
    pj_status_t status;
    
    /* Must init PJLIB first: */
    status = pj_init(0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Then init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(0, &cp, &pj_pool_factory_default_policy, 0);

    /* Create global endpoint: */
    {
	/* Endpoint MUST be assigned a globally unique name.
	 * Ideally we should put hostname or public IP address, but
	 * we'll just use an arbitrary name here.
	 */

	/* Create the endpoint: */
	status = pjsip_endpt_create(&cp.factory, "sipstateless",
				    &sip_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Parse arguments */
    pj_optind = 0;
    pj_list_init(&hdr_list);
    while ((c=pj_getopt(argc, argv , "H:")) != -1) {
	switch (c) {
	case 'H':
	    if (pool == NULL) {
		pool = pj_pool_create(&cp.factory, "sipstateless", 1000, 
				      1000, NULL);
	    } 
	    
	    if (pool) {
		char *name;
		name = strtok(pj_optarg, ":");
		if (name == NULL) {
		    puts("Error: invalid header format");
		    return 1;
		} else {
		    char *val = strtok(NULL, "\r\n");
		    pjsip_generic_string_hdr *h;
		    pj_str_t hname, hvalue;

		    hname = pj_str(name);
		    hvalue = pj_str(val);

		    h = pjsip_generic_string_hdr_create(pool, &hname, &hvalue);

		    pj_list_push_back(&hdr_list, h);

		    PJ_LOG(4,(THIS_FILE, "Header %s: %s added", name, val));
		}
	    }
	    break;
	default:
	    puts("Error: invalid argument");
	    usage();
	    return 1;
	}
    }

    if (pj_optind != argc) {
	code = atoi(argv[pj_optind]);
	if (code < 200 || code > 699) {
	    puts("Error: invalid status code");
	    usage();
	    return 1;
	}
    }

    PJ_LOG(4,(THIS_FILE, "Returning %d to incoming requests", code));


    /* 
     * Add UDP transport, with hard-coded port 
     */
#ifdef HAS_UDP_TRANSPORT
    {
	pj_sockaddr_in addr;

	addr.sin_family = pj_AF_INET();
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons(5060);

	status = pjsip_udp_transport_start( sip_endpt, &addr, NULL, 1, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,(THIS_FILE, 
		      "Error starting UDP transport (port in use?)"));
	    return 1;
	}
    }
#endif

#if HAS_TCP_TRANSPORT
    /* 
     * Add UDP transport, with hard-coded port 
     */
    {
	pj_sockaddr_in addr;

	addr.sin_family = pj_AF_INET();
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons(5060);

	status = pjsip_tcp_transport_start(sip_endpt, &addr, 1, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,(THIS_FILE, 
		      "Error starting TCP transport (port in use?)"));
	    return 1;
	}
    }
#endif

    /*
     * Register our module to receive incoming requests.
     */
    status = pjsip_endpt_register_module( sip_endpt, &mod_app);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Done. Loop forever to handle incoming events. */
    PJ_LOG(3,(THIS_FILE, "Press Ctrl-C to quit.."));

    for (;;) {
	pjsip_endpt_handle_events(sip_endpt, NULL);
    }
}

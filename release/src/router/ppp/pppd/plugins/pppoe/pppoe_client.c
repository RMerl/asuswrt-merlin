/* PPPoE support library "libpppoe"
 *
 * Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
 *		  Jamal Hadi Salim <hadi@cyberus.ca>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <pppoe.h>

int retry_num = 10;

static int std_rcv_pado(struct session* ses,
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){

    struct pppoe_tag *ac_name, *srv_name;
    char ac[1024], srv[1024];
    
    if(ses->state != PADO_CODE ){
	poe_error(ses,"Unexpected packet: %P",p_in);
	return 0;
    }
    
    if( verify_packet(ses, p_in) < 0)
	return 0;
    
	LOGX_INFO("Received PADO.");

    if (DEB_DISC2)
	poe_dbglog (ses,"PADO received: %P", p_in);
    
    memcpy(&ses->remote, &p_in->addr, sizeof(struct sockaddr_ll));
    memcpy(&ses->curr_pkt.addr, &ses->remote , sizeof(struct sockaddr_ll));
    
    ses->curr_pkt.hdr->code = PADR_CODE;
    
    /* The HOST_UNIQ has been verified already... there's no "if" about this */
    /* if(ses->filt->htag) */
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_HOST_UNIQ));	
    
    if (ses->filt->ntag) {
    	ses->curr_pkt.tags[TAG_AC_NAME]=NULL;
    }
    
    ac_name = get_tag(p_in->hdr,PTT_AC_NAME);
    srv_name = get_tag(p_in->hdr,PTT_SRV_NAME);

    memset(ac, 0, sizeof(ac)); 	 
    memset(srv, 0, sizeof(srv)); 	 
    
    strncpy(ac, ac_name->tag_data, ntohs(ac_name->tag_len));
    strncpy(srv, srv_name->tag_data, ntohs(srv_name->tag_len));

    script_setenv("AC_NAME", ac, 1);
    script_setenv("SRV_NAME", srv, 1);
    
    if(ses->filt->stag) {
    	ses->curr_pkt.tags[TAG_SRV_NAME]=NULL;
    }
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_SRV_NAME));
    
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_AC_COOKIE));
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_RELAY_SID));
    
    ses->state = PADS_CODE;
    
    ses->retransmits = 0;

	LOGX_INFO("Sending PADR.");
    
    send_disc(ses, &ses->curr_pkt);
    (*p_out) = &ses->curr_pkt;

    if (ses->np)
		return 1;
    
    return 0;
}

static int std_init_disc(struct session* ses,
			 struct pppoe_packet *p_in,
			 struct pppoe_packet **p_out){
    
    /* Check if already connected */
    if( ses->state != PADO_CODE ){
	return -1;
    }
    
    memset(&ses->curr_pkt,0, sizeof(struct pppoe_packet));

    ses->curr_pkt.hdr = (struct pppoe_hdr*) ses->curr_pkt.buf;
    ses->curr_pkt.hdr->ver  = 1;
    ses->curr_pkt.hdr->type = 1;
    ses->curr_pkt.hdr->code = PADI_CODE;
    
    
    memcpy( &ses->curr_pkt.addr, &ses->remote , sizeof(struct sockaddr_ll));
    
	LOGX_INFO("Sending PADI.");
    
    ses->retransmits = 0 ;
    
    if(ses->filt->ntag) {
	ses->curr_pkt.tags[TAG_AC_NAME]=ses->filt->ntag;
	poe_info(ses,"overriding AC name\n");
    }
    
    if(ses->filt->stag)
	ses->curr_pkt.tags[TAG_SRV_NAME]=ses->filt->stag;
    
    if(ses->filt->htag)
	ses->curr_pkt.tags[TAG_HOST_UNIQ]=ses->filt->htag;
    
    ses->retransmits = 0 ;
    
    send_disc(ses, &ses->curr_pkt);
    (*p_out)= &ses->curr_pkt;

    return 0;
}


static int std_rcv_pads(struct session* ses,
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){
    if(ses->state != PADS_CODE ){
		poe_error(ses,"Unexpected packet: %P",p_in);
		return 0;
    }
    
    if( verify_packet(ses, p_in) < 0)
		return 0;
    
    ses->sp.sa_family = AF_PPPOX;
    ses->sp.sa_protocol = PX_PROTO_OE;
    ses->sp.sa_addr.pppoe.sid = p_in->hdr->sid;
    memcpy(ses->sp.sa_addr.pppoe.dev,ses->name, IFNAMSIZ);
    memcpy(ses->sp.sa_addr.pppoe.remote, p_in->addr.sll_addr, ETH_ALEN);
    
    if (DEB_DISC)
		poe_dbglog (ses,"Got connection: %x %s <--->%E",  
			ses->sp.sa_addr.pppoe.sid, 
			ses->sp.sa_addr.pppoe.dev, ses->sp.sa_addr.pppoe.remote);
    
	LOGX_INFO("Received PADS. SID: 0x%04X", ses->sp.sa_addr.pppoe.sid);
    return 1;
}

static int std_rcv_padt(struct session* ses,
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){

	LOGX_INFO("Received PADT.");
    ses->state = PADO_CODE;
    return 0;
}


extern int disc_sock;

int client_init_ses (struct session *ses, char* devnam)
{
    int i=0;
    int retval;
    char dev[IFNAMSIZ+1];
    int addr[ETH_ALEN];
    int sid;
    
    /* Make socket if necessary */
    if (disc_sock < 0) {
		disc_sock = socket(PF_PACKET, SOCK_DGRAM, 0);
		if (disc_sock < 0){
		    poe_fatal(ses, "Cannot create PF_PACKET socket for PPPoE discovery\n");
		}
    }
    
    /* Check for long format */
    retval =sscanf(devnam, FMTSTRING(IFNAMSIZ),addr, addr+1, addr+2,
		   addr+3, addr+4, addr+5,&sid,dev);
    if( retval != 8 ){
	/* Verify the device name , construct ses->local */
	retval = get_sockaddr_ll(devnam,&ses->local);
	if (retval < 0)
	    poe_fatal(ses, "client_init_ses: "
		      "Cannot create PF_PACKET socket for PPPoE discovery\n");
	
	
	ses->state = PADO_CODE;
	memcpy(&ses->remote, &ses->local, sizeof(struct sockaddr_ll) );
	
	memset( ses->remote.sll_addr, 0xff, ETH_ALEN);
    }else{
	/* long form parsed */

	/* Verify the device name , construct ses->local */
	retval = get_sockaddr_ll(dev,&ses->local);
	if (retval < 0)
	    poe_fatal(ses,"client_init_ses(2): "
		      "Cannot create PF_PACKET socket for PPPoE discovery\n");
	ses->state = PADS_CODE;
	ses->sp.sa_family = AF_PPPOX;
	ses->sp.sa_protocol = PX_PROTO_OE;
	ses->sp.sa_addr.pppoe.sid = sid;
	
	memcpy(&ses->remote, &ses->local, sizeof(struct sockaddr_ll) );
	
	for(; i < ETH_ALEN ; ++i ){
	    ses->sp.sa_addr.pppoe.remote[i] = addr[i];
	    ses->remote.sll_addr[i]=addr[i];
	}
	memcpy(ses->sp.sa_addr.pppoe.dev, dev, IFNAMSIZ);
	
	
	
    }
    if( retval < 0 )
	error("bad device name: %s",devnam);


    if (DEB_DISC)
	poe_dbglog (ses,"Local ETH %E", ses->local.sll_addr);

    
    retval = bind( disc_sock ,
		   (struct sockaddr*)&ses->local,
		   sizeof(struct sockaddr_ll));
    
    
    if( retval < 0 ){
	error("bind to PF_PACKET socket failed: %m");
    }

    /* Make socket if necessary */
    if (ses->fd < 0) {
	    ses->fd = socket(AF_PPPOX,SOCK_STREAM,PX_PROTO_OE);
	    if(ses->fd < 0)
	    {
		poe_fatal(ses,"Failed to create PPPoE socket: %m");
	    }
    }
    
    
    ses->init_disc = std_init_disc;
    ses->rcv_pado  = std_rcv_pado;
    ses->rcv_pads  = std_rcv_pads;
    ses->rcv_padt  = std_rcv_padt;
    
    /* this should be filter overridable */
    ses->retries = retry_num;
    
    return ses->fd;
}


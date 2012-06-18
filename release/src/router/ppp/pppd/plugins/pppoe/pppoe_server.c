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
#include "pppoe.h"
#include <unistd.h>

static unsigned int pcid=1111;
static int srv_rcv_padi(struct session* ses, 
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){
    struct pppoe_con *newpc = NULL;
    struct pppoe_tag *tag;
    
    poe_dbglog(ses,"Srv Recv'd packet: %P\n",p_in);
    
    
    ses->curr_pkt.hdr = (struct pppoe_hdr*) ses->curr_pkt.buf;
    ses->curr_pkt.hdr->ver  = 1;
    ses->curr_pkt.hdr->type = 1;

    tag = get_tag(p_in->hdr,PTT_SRV_NAME);

    if(!tag )
	return 0;

    if( ntohs(tag->tag_len)==0 ){
	ses->curr_pkt.tags[TAG_SRV_NAME] = ses->filt->stag ;
    }else if( tag->tag_len != ses->filt->stag->tag_len
	      || !memcmp( tag+1, ses->filt->stag, ntohs(tag->tag_len)) ){
	return 0;
    }else{
	ses->curr_pkt.tags[TAG_SRV_NAME] = tag;
    }

    ses->curr_pkt.tags[ TAG_AC_NAME] = ses->filt->ntag ;
    ses->curr_pkt.tags[ TAG_HOST_UNIQ ] = get_tag(p_in->hdr,PTT_HOST_UNIQ);
    
    memcpy(&ses->remote, &p_in->addr, sizeof(struct sockaddr_ll));
    memcpy( &ses->curr_pkt.addr, &ses->remote , sizeof(struct sockaddr_ll));
    
    ses->curr_pkt.hdr->code =  PADO_CODE;
    
    
    ses->curr_pkt.tags[ TAG_RELAY_SID ] = get_tag(p_in->hdr,PTT_RELAY_SID);

    send_disc(ses, &ses->curr_pkt);
    poe_dbglog(ses,"Srv Sent packet: %P\n",&ses->curr_pkt);
    
    return 0;
}


static int srv_rcv_padr(struct session* ses, 
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){
    struct pppoe_tag *tag;

    poe_dbglog(ses,"Recv'd packet: %P\n",p_in);
    


    /* Run checks to ensure this packets asks for 
       what we're willing to offer */

    tag = get_tag(p_in->hdr,PTT_SRV_NAME);

    if(!tag || tag->tag_len == 0 ){
	p_in->tags[TAG_SRV_NAME] = ses->filt->stag;

    }else if( tag->tag_len != ses->filt->stag->tag_len
	     || !memcmp(tag + 1 , ses->filt->stag, ntohs(tag->tag_len)) ){
	return 0;
    }else{
	p_in->tags[TAG_SRV_NAME] = tag;
    }

    tag = get_tag(p_in->hdr,PTT_AC_NAME);
    if( !tag || tag->tag_len==0 ){
	p_in->tags[TAG_AC_NAME] = ses->filt->ntag;
    }else if( tag->tag_len != ses->filt->ntag->tag_len
	  || !memcmp(tag + 1, ses->filt->ntag, ntohs(tag->tag_len)) ){
	return 0;
    }else{
	p_in->tags[TAG_AC_NAME] = tag;
    }

    
    
    
    pcid = ++pcid & 0x0000ffff ;
    if(pcid == 0 ){
	pcid = 1111;
    }
    
    p_in->hdr->sid  = ntohs(pcid);
    
    p_in->hdr->code = PADS_CODE;
    send_disc(ses, p_in);
    
    poe_dbglog(ses,"Sent packet: %P\n",p_in);
    
    ses->sp.sa_family = AF_PPPOX;
    ses->sp.sa_protocol=PX_PROTO_OE;
    ses->sp.sa_addr.pppoe.sid = p_in->hdr->sid;
    memcpy(ses->sp.sa_addr.pppoe.dev, ses->name, IFNAMSIZ);
    memcpy(ses->sp.sa_addr.pppoe.remote, p_in->addr.sll_addr, ETH_ALEN);
    memcpy(&ses->remote, &p_in->addr, sizeof(struct sockaddr_ll));
    return 1;
}

static int srv_rcv_padt(struct session* ses, 
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){
    return 0;
}



int srv_init_ses(struct session *ses, char* from)
{
    int retval;
    retval = client_init_ses(ses, from);
    ses->init_disc = NULL;
    ses->rcv_pado  = NULL;
    ses->rcv_pads  = NULL;
    ses->rcv_padi  = srv_rcv_padi;
    ses->rcv_padr  = srv_rcv_padr;
    ses->rcv_padt  = srv_rcv_padt;

    /* retries forever */
    ses->retries   = -1;

    return retval;
}



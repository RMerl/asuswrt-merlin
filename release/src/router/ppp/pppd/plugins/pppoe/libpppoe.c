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
#include <sys/sysinfo.h>

static int tag_map[] = { PTT_SRV_NAME,
			 PTT_AC_NAME,
			 PTT_HOST_UNIQ,
			 PTT_AC_COOKIE,
			 PTT_VENDOR,
			 PTT_RELAY_SID,
			 PTT_SRV_ERR,
			 PTT_SYS_ERR,
			 PTT_GEN_ERR,
			 PTT_EOL
};

int disc_sock=-1;
int retransmit_time=10;
int redial_immediately=0;

int verify_packet( struct session *ses, struct pppoe_packet *p);

#define TAG_DATA(type,tag_ptr) ((type *) ((struct pppoe_tag*)tag_ptr)->tag_data)
extern int dial_cnt;

/***************************************************************************
 *
 * Return the location where the next tag can be pu
 *
 **************************************************************************/
static  struct pppoe_tag *next_tag(struct pppoe_hdr *ph)
{
    return (struct pppoe_tag *)
	(((char *) &ph->tag) + ntohs(ph->length));
}

/**************************************************************************
 *
 * Update header to reflect the addition of a new tag
 *
 **************************************************************************/
static  void add_tag(struct pppoe_hdr *ph, struct pppoe_tag *pt)
{
    int len = (ntohs(ph->length) +
	       ntohs(pt->tag_len) +
	       sizeof(struct pppoe_tag));

    if (pt != next_tag(ph))
	printf("PPPoE add_tag caller is buggy\n");

    ph->length = htons(len);
}

/*************************************************************************
 *
 * Look for a tag of a specific type
 *
 ************************************************************************/
struct pppoe_tag *get_tag(struct pppoe_hdr *ph, u_int16_t idx)
{
    char *end = (char *) next_tag(ph);
    char *ptn = NULL;
    struct pppoe_tag *pt = &ph->tag[0];

    /*
     * Keep processing tags while a tag header will still fit.
     *
     * This check will ensure that the entire tag header pointed
     * to by pt will fit inside the message, and thus it will be
     * valid to check the tag_type and tag_len fields.
     */
    while ((char *)(pt + 1) <= end) {
	/*
	 * If the tag data would go past the end of the packet, abort.
	 */
	ptn = (((char *) (pt + 1)) + ntohs(pt->tag_len));
	if (ptn > end)
	    return NULL;

	if (pt->tag_type == idx)
	    return pt;

	pt = (struct pppoe_tag *) ptn;
    }

    return NULL;
}

/* We want to use tag names to reference into arrays  containing the tag data.
   This takes an RFC 2516 tag identifier and maps it into a local one.
   The reverse mapping is accomplished via the tag_map array */
#define UNMAP_TAG(x) case PTT_##x : return TAG_##x
static inline int tag_index(int tag){
    switch(tag){
	UNMAP_TAG(SRV_NAME);
	UNMAP_TAG(AC_NAME);
	UNMAP_TAG(HOST_UNIQ);
	UNMAP_TAG(AC_COOKIE);
	UNMAP_TAG(VENDOR);
	UNMAP_TAG(RELAY_SID);
	UNMAP_TAG(SRV_ERR);
	UNMAP_TAG(SYS_ERR);
	UNMAP_TAG(GEN_ERR);
	UNMAP_TAG(EOL);
    };
    return -1;
}

/*************************************************************************
 *
 * Makes a copy of a tag into a PPPoE packe
 *
 ************************************************************************/
void copy_tag(struct pppoe_packet *dest, struct pppoe_tag *pt)
{
    struct pppoe_tag *end_tag = get_tag(dest->hdr, PTT_EOL);
    int tagid;
    int tag_len;
    if( !pt ) {
	return;
    }
    tagid = tag_index(pt->tag_type);

    tag_len = sizeof(struct pppoe_tag) + ntohs(pt->tag_len);

    if( end_tag ){
	memcpy(((char*)end_tag)+tag_len ,
	       end_tag, sizeof(struct pppoe_tag));

	dest->tags[tagid]=end_tag;
	dest->tags[TAG_EOL] = (struct pppoe_tag*)((char*)dest->tags[TAG_EOL] + tag_len);
	memcpy(end_tag, pt, tag_len);
	dest->hdr->length = htons(ntohs(dest->hdr->length) + tag_len);

    }else{
	memcpy(next_tag(dest->hdr),pt, tag_len);
	dest->tags[tagid]=next_tag(dest->hdr);
	add_tag(dest->hdr,next_tag(dest->hdr));
    }


}


/*************************************************************************
 *
 * Put tags from a packet into a nice array
 *
 ************************************************************************/
static void extract_tags(struct pppoe_hdr *ph, struct pppoe_tag** buf){
    int i=0;
    for(;i<MAX_TAGS;++i){
	buf[i] = get_tag(ph,tag_map[i]);
    }
}


/*************************************************************************
 *
 * Verify that a packet has a tag containint a specific value
 *
 ************************************************************************/
static int verify_tag(struct session* ses,
		      struct pppoe_packet* p,
		      unsigned short id,
		      char* data,
		      int data_len)
{
    int len;
    struct pppoe_tag *pt = p->tags[id];

    if( !pt ){
	poe_info(ses,"Missing tag %d. Expected %s\n",
		 id,data);
	return 0;
    }
    len = ntohs(pt->tag_len);
    if(len != data_len){
	poe_info(ses,"Length mismatch on tag %d: expect: %d got: %d\n",
		 id, data_len, len);
	return 0;
    }

    if( 0!=memcmp(pt->tag_data,data,data_len)){
	poe_info(ses,"Tag data mismatch on tag %d: expect: %s vs %s\n",
		 id, data,pt->tag_data);
	return 0;
    }
    return 1;
}


/*************************************************************************
 *
 * Verify the existence of an ethernet device.
 * Construct an AF_PACKET address struct to match.
 *
 ************************************************************************/
int get_sockaddr_ll(const char *devnam,struct sockaddr_ll* sll){
    struct ifreq ifr;
    int retval;

    if(disc_sock<0){

	disc_sock = socket(PF_PACKET, SOCK_DGRAM, 0);
	if( disc_sock < 0 ){
	    return -1;
	}
    }

    strncpy(ifr.ifr_name, devnam, sizeof(ifr.ifr_name));

    retval = ioctl( disc_sock , SIOCGIFINDEX, &ifr);

    if( retval < 0 ){
//	error("Bad device name: %s  (%m)",devnam);
	return 0;
    }

    if(sll) sll->sll_ifindex = ifr.ifr_ifindex;

    retval = ioctl (disc_sock, SIOCGIFHWADDR, &ifr);
    if( retval < 0 ){
//	error("Bad device name: %s  (%m)",devnam);
	return 0;
    }

    if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
	error("Interface %s is not Ethernet!", devnam);
	return 0;
    }
    if(sll){
	sll->sll_family	= AF_PACKET;
	sll->sll_protocol= ntohs(ETH_P_PPP_DISC);
	sll->sll_hatype	= ARPHRD_ETHER;
	sll->sll_pkttype = PACKET_BROADCAST;
	sll->sll_hatype	= ETH_ALEN;
	memcpy( sll->sll_addr , ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    }
    return 1;
}




/*************************************************************************
 *
 * Construct and send a discovery message.
 *
 ************************************************************************/
int send_disc(struct session *ses, struct pppoe_packet *p)
{
    char buf[MAX_PAYLOAD + sizeof(struct pppoe_hdr)];
    int data_len = sizeof(struct pppoe_hdr);

    struct pppoe_hdr *ph = NULL;
    struct pppoe_tag *tag = NULL;
    int i, err = 0;
    int got_host_uniq = 0;
    int got_srv_name = 0;
    int got_ac_name = 0;

    for (i = 0; i < MAX_TAGS; i++) {
	if (!p->tags[i])
	    continue;

	got_host_uniq |= (p->tags[i]->tag_type == PTT_HOST_UNIQ);

	/* Relay identifiers qualify as HOST_UNIQ's:
	   we need HOST_UNIQ to uniquely identify the packet,
	   PTT_RELAY_SID is sufficient for us for outgoing packets */
	got_host_uniq |= (p->tags[i]->tag_type == PTT_RELAY_SID);

	got_srv_name |= (p->tags[i]->tag_type == PTT_SRV_NAME);
	got_ac_name  |= (p->tags[i]->tag_type == PTT_AC_NAME);

	data_len += (ntohs(p->tags[i]->tag_len) +
		     sizeof(struct pppoe_tag));
    }

    ph = (struct pppoe_hdr *) buf;


    memcpy(ph, p->hdr, sizeof(struct pppoe_hdr));
    ph->length = __constant_htons(0);

    /* if no HOST_UNIQ tags --- add one with process id */
    if (!got_host_uniq){
	data_len += (sizeof(struct pppoe_tag) +
		     sizeof(struct session *));
	tag = next_tag(ph);
	tag->tag_type = PTT_HOST_UNIQ;
	tag->tag_len = htons(sizeof(struct session *));
	memcpy(tag->tag_data,
	       &ses,
	       sizeof(struct session *));

	add_tag(ph, tag);
    }

    if( !got_srv_name ){
	data_len += sizeof(struct pppoe_tag);
	tag = next_tag(ph);
	tag->tag_type = PTT_SRV_NAME;
	tag->tag_len = 0;
	add_tag(ph, tag);
    }

    if(!got_ac_name && ph->code==PADO_CODE){
	data_len += sizeof(struct pppoe_tag);
	tag = next_tag(ph);
	tag->tag_type = PTT_AC_NAME;
	tag->tag_len = 0;
	add_tag(ph, tag);
    }

    for (i = 0; i < MAX_TAGS; i++) {
	if (!p->tags[i])
	    continue;

	tag = next_tag(ph);
	memcpy(tag, p->tags[i],
	       sizeof(struct pppoe_tag) + ntohs(p->tags[i]->tag_len));

	add_tag(ph, tag);
    }

    /* Now fixup the packet struct to make sure all of its pointers
       are self-contained */
    memcpy( p->hdr , ph, data_len );
    extract_tags( p->hdr, p->tags);

    err = sendto(disc_sock, buf, data_len, 0,
		   (struct sockaddr*) &p->addr,
		   sizeof(struct sockaddr_ll));

    if(err < 0)
	poe_error(ses,"sendto returned: %m\n");

    return err;
}

/*************************************************************************
 *
 * Verify that a packet is legal
 *
 *************************************************************************/
int verify_packet( struct session *ses, struct pppoe_packet *p){
    struct session * hu_val;

    /* This code here should do all of the error checking and
       validation on the incoming packet */


    /* If we receive any error tags, abort */
#define CHECK_TAG(name, val)					\
    if((NULL==p->tags[name])== val){				\
	poe_error(ses,"Tag error: " #name );			\
	return -1;						\
    }


    /* If packet is not directed to our MAC address, forget it */
    if (ses->state == PADS_CODE) {
	    if (memcmp(p->addr.sll_addr, ses->remote.sll_addr, ETH_ALEN)) {
		poe_info(ses,"ETH_DEST mismatch: %E %E \n",p->addr.sll_addr, ses->remote.sll_addr);
		return -1;
	    }
    }

    CHECK_TAG(TAG_SRV_ERR,0);
    CHECK_TAG(TAG_SYS_ERR,0);
    CHECK_TAG(TAG_GEN_ERR,0);

    /* A HOST_UNIQ must be present */
    CHECK_TAG(TAG_HOST_UNIQ,1);

    hu_val = *TAG_DATA(struct session* ,p->tags[TAG_HOST_UNIQ]);

    if( hu_val != ses ){
	poe_info(ses,"HOST_UNIQ mismatch: %08x %i\n",(int)hu_val,getpid());
	return -1;
    }

    if(ses->filt->htag &&
       !verify_tag(ses,p,TAG_HOST_UNIQ,ses->filt->htag->tag_data,(int)ntohs(ses->filt->htag->tag_len))) {
	poe_info(ses,"HOST_UNIQ failure");
	return -1;
    }


    if(ses->filt->ntag && ses->state == PADO_CODE &&
       !verify_tag(ses,p,TAG_AC_NAME,ses->filt->ntag->tag_data,(int)ntohs(ses->filt->ntag->tag_len))){
	poe_info(ses,"AC_NAME failure");
	return -1;
    }

    if(ses->filt->stag &&
       !verify_tag(ses,p,TAG_SRV_NAME,ses->filt->stag->tag_data,(int)ntohs(ses->filt->stag->tag_len))){
	poe_info(ses,"SRV_NAME failure");
	return -1;
    }

    return 0;
}


/*************************************************************************
 *
 * Receive and verify an incoming packet.
 *
 *************************************************************************/
static int recv_disc( struct session *ses,
		      struct pppoe_packet *p){
    int error = 0;
    unsigned int from_len = sizeof(struct sockaddr_ll);

    p->hdr = (struct pppoe_hdr*)p->buf;

    error = recvfrom( disc_sock, p->buf, 1500, 0,
		      (struct sockaddr*)&p->addr, &from_len);

    if(error < 0) return error;

    extract_tags(p->hdr,p->tags);

    return 1;
}


/*************************************************************************
 *
 * Send a PADT
 *
 *************************************************************************/
int session_disconnect(struct session *ses){
    struct pppoe_packet padt;

    memset(&padt,0,sizeof(struct pppoe_packet));
    memcpy(&padt.addr, &ses->remote, sizeof(struct sockaddr_ll));

    padt.hdr = (struct pppoe_hdr*) ses->curr_pkt.buf;
    padt.hdr->ver  = 1;
    padt.hdr->type = 1;
    padt.hdr->code = PADT_CODE;
    padt.hdr->sid  = ses->sp.sa_addr.pppoe.sid;

	LOGX_INFO("Sending PADT.");

    send_disc(ses,&padt);
    ses->sp.sa_addr.pppoe.sid = 0 ;
    ses->state = PADO_CODE;
    return 0;

}


/*************************************************************************
 *
 * Make a connection -- behaviour depends on callbacks specified in "ses"
 *
 *************************************************************************/
int session_connect(struct session *ses)
{

    struct pppoe_packet *p_out=NULL;
    struct pppoe_packet rcv_packet;
    int ret;
	//int dial_cnt;

    if(ses->init_disc){
		ret = (*ses->init_disc)(ses, NULL, &p_out);
		if ( ret != 0 ) {
			LOGX_DEBUG("%s: ses->init_disc() == %d", __FUNCTION__, ret);
			return ret;
		}
    }

    /* main discovery loop */

	//dial_cnt = 0;
    //while(ses->retransmits < ses->retries || ses->retries==-1 ){
    while ((!idle_time_limit && dial_cnt < (3 - 1)) || (ses->retransmits < ses->retries || ses->retries==-1)) {
		fd_set in;
		struct timeval tv;

		if(ses->retransmits < 0) {
			FD_ZERO(&in);
			FD_SET(disc_sock,&in);
		    ret = select(disc_sock+1, &in, NULL, NULL, NULL);
		}
		else {
		    ++ses->retransmits;
		    //tv.tv_sec = 1 << ses->retransmits;
		    /*******************************************
		     * modify by tanghui @ 2006-03-27
		     * for random redial
		     *******************************************/
			if(!idle_time_limit) {
				if((ses->curr_pkt.hdr->code != PADI_CODE) || (dial_cnt < (3 - 1))) {
					tv.tv_sec = 10;
				}
				else {
					tv.tv_sec = (int)(3 + (((retransmit_time - 3.0) * rand())/ (RAND_MAX + 1.0)));
					if((tv.tv_sec > 93) && (tv.tv_sec > retransmit_time - 90)) {
						tv.tv_sec -= 90;
					}
					//tv.tv_sec = 3 + gen_random_int(retransmit_time - 3);
				}
			}
			else {
				tv.tv_sec = 10;
			}
			dial_cnt++;
		    /*******************************************/
			tv.tv_usec = 0;
again:
			FD_ZERO(&in);
			FD_SET(disc_sock,&in);
		    ret = select(disc_sock+1, &in, NULL, NULL, &tv);
		}

		if( ret < 0 && errno != EINTR){
			LOGX_DEBUG("%s: select() == %d", __FUNCTION__, ret);
		    return -1;
		}
		else if( ret == 0 ) {
	    	if (!((!idle_time_limit && dial_cnt < (3 - 1)) || (ses->retransmits < ses->retries || ses->retries==-1))) {
				redial_immediately = 1;
			}
		    if( DEB_DISC )
				poe_dbglog(ses, "Re-sending ...");

		    if( ses->timeout ) {
				ret = (*ses->timeout)(ses, NULL, &p_out);
				if ( ret != 0 ) {
					LOGX_DEBUG("%s: ses->timeout() == %d", __FUNCTION__, ret);
				    return ret;
				}
		    }
		    else if (p_out && !redial_immediately) {
				LOGX_INFO("Resending...");
				send_disc(ses,p_out);
			}
			
		    continue;
		}

		ret = recv_disc(ses, &rcv_packet);
		/* Should differentiate between system errors and
		   bad packets and the like... */
		if( ret < 0 && errno ) {
			LOGX_DEBUG("%s: recv_disc() == %d, errno == %d", __FUNCTION__, ret, errno);
		    return -1;
		}

#if 1	// see rc/redial	-- zzz
		FILE *f;
		struct sysinfo si;
		
		if ((f = fopen(pppoe_disc_file, "w")) != NULL) {
			sysinfo(&si);
			fwrite(&si.uptime, sizeof(si.uptime), 1, f);
			fclose(f);
		}
#endif

		switch (rcv_packet.hdr->code) {

		case PADI_CODE:
		{
		    if(ses->rcv_padi){
				ret = (*ses->rcv_padi)(ses,&rcv_packet,&p_out);
				if( ret != 0){
					LOGX_DEBUG("%s: ses->rcv_padi() == %d", __FUNCTION__, ret);
				    return ret;
				}
		    }
		    break;
		}

		case PADO_CODE:		/* wait for PADO */
		{
		    if (ses->rcv_pado) {
				ret = (*ses->rcv_pado)(ses,&rcv_packet,&p_out);
				if( ret != 0) {
					LOGX_DEBUG("%s: ses->rcv_pado() == %d", __FUNCTION__, ret);
			    	return ret;
				}
				else {
			   		goto again;
				}
		    }
		    break;
		}

		case PADR_CODE:
		{
		    if(ses->rcv_padr) {
				ret = (*ses->rcv_padr)(ses,&rcv_packet,&p_out);
				if ( ret != 0) {
					LOGX_DEBUG("%s: ses->rcv_padr() == %d", __FUNCTION__, ret);
				    return ret;
				}
		    }
		    break;
		}

		case PADS_CODE:		/* wait for PADS */
		{
		    if(ses->rcv_pads) {
				ret = (*ses->rcv_pads)(ses,&rcv_packet,&p_out);
				LOGX_DEBUG("%s: ses->rcv_pads() == %d", __FUNCTION__, ret);
				if ( ret != 0) {
				    return ret;
				}
				else {
				    goto again;
				}
		    }
		    break;
		}

		case PADT_CODE:
		{
		    if (rcv_packet.hdr->sid != ses->sp.sa_addr.pppoe.sid) {
#if 1			// probably from previous session, ignore	-- zzz
				LOGX_INFO("Received PADT for 0x%04X, expecting 0x%04X", rcv_packet.hdr->sid, ses->sp.sa_addr.pppoe.sid);
				goto again;
#else			
				redial_immediately = 1;
				return (0);
				//	--ses->retransmits;
				//	continue;
#endif
		    }
			
			//	checkme: ?? sid will never be the same here. Dead code ?? 	-- zzz
			
		    if(ses->rcv_padt){
				ret = (*ses->rcv_padt)(ses,&rcv_packet,&p_out);
				if( ret != 0){
					LOGX_DEBUG("%s: ses->rcv_padt() == %d", __FUNCTION__, ret);
					return ret;
				}
				else {
					redial_immediately = 1;
					LOGX_DEBUG("%s: PADT rcv_padt", __FUNCTION__);
					return (0);
				//    goto again;
				}
		    }
			else{
				LOGX_DEBUG("%s: PADT !rcv_padt", __FUNCTION__);
				poe_error (ses,"connection terminated");
				redial_immediately = 1;
				return (-1);
		    }
		    break;
		}
		default:
			LOGX_DEBUG("%s: invalid packet %d", __FUNCTION__, rcv_packet.hdr->code);
			goto again;
//		    return (-1);
		}
    }	// while

	LOGX_DEBUG("%s: return default", __FUNCTION__);
    return (-1);
}


/*************************************************************************
 *
 * Register an ethernet address as a client of relaying services.
 *
 *************************************************************************/
 /*
int add_client(char *addr)
{
    struct pppoe_con* pc = (struct pppoe_con*)malloc(sizeof(struct pppoe_con));
    int ret;
    if(!pc)
	return -ENOMEM;

    memset(pc, 0 , sizeof(struct pppoe_con));

    memcpy(pc->client,addr, ETH_ALEN);
    memcpy(pc->key, addr, ETH_ALEN);

    pc->key_len = ETH_ALEN;

    if( (ret=store_con(pc)) < 0 ){
	free(pc);
    }
    return ret;

}
*/

struct pppoe_tag *make_filter_tag(short type, short length, char* data){
    struct pppoe_tag *pt =
	(struct pppoe_tag* )malloc( sizeof(struct pppoe_tag) + length );

    if(pt == NULL) return NULL;

    pt->tag_len=htons(length);
    pt->tag_type=type;

    if(length>0 && data){
	memcpy( pt+1, data, length);
    }
    return pt;
}

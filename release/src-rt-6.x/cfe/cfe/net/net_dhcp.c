/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  DHCP Client 				File: net_dhcp.c
    *  
    *  This module contains a DHCP client.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "env_subr.h"

#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"
#include "cfe_error.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "cfe.h"
#include "bsp_config.h"

#include "net_api.h"

#if CFG_DHCP

/*  *********************************************************************
    *  Constants
    ********************************************************************* */


#define UDP_PORT_BOOTPS	67
#define UDP_PORT_BOOTPC	68

#define DHCP_REQ_TIMEOUT	(1*CFE_HZ)
#define DHCP_NUM_RETRIES	8

#define DHCP_OP_BOOTREQUEST	1
#define DHCP_OP_BOOTREPLY	2

#define DHCP_HWTYPE_ETHERNET	1

#define DHCP_TAG_FUNCTION	53
#define DHCP_FUNCTION_DISCOVER	1
#define DHCP_FUNCTION_OFFER	2
#define DHCP_FUNCTION_REQUEST	3
#define DHCP_FUNCTION_ACK	5

#define DHCP_TAG_NETMASK	1
#define DHCP_TAG_DOMAINNAME	0x0F
#define DHCP_TAG_GATEWAY	0x03
#define DHCP_TAG_NAMESERVER	0x06
#define DHCP_TAG_SWAPSERVER	0x10
#define DHCP_TAG_ROOTPATH	0x11
#define DHCP_TAG_EXTENSIONS	0x12
#define DHCP_TAG_SERVERIDENT	54
#define DHCP_TAG_PARAMLIST	55
#define DHCP_TAG_CLIENTID	61
#define DHCP_TAG_CLASSID	60
#define DHCP_TAG_REQADDR        50
#define DHCP_TAG_LEASE_TIME	51
#define DHCP_TAG_SCRIPT		130		/* CFE extended option */
#define DHCP_TAG_OPTIONS	131		/* CFE extended option */

#define DHCP_TAG_END		0xFF

#define DHCP_MAGIC_NUMBER	0x63825363

/*#define _DEBUG_*/


/*  *********************************************************************
    *  dhcp_set_envvars(reply)
    *  
    *  Using the supplied DHCP reply data, set environment variables
    *  to contain data from the reply.
    *  
    *  Input parameters: 
    *  	   reply - dhcp reply
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void dhcp_set_envvars(dhcpreply_t *reply)
{
    char buffer[50];

    env_delenv("BOOT_SERVER");
    env_delenv("BOOT_FILE");
    env_delenv("BOOT_SCRIPT");
    env_delenv("BOOT_OPTIONS");

    if (reply->dr_bootserver[0] | reply->dr_bootserver[1] |
	reply->dr_bootserver[2] | reply->dr_bootserver[3]) {
	sprintf(buffer,"%I",reply->dr_bootserver);
	env_setenv("BOOT_SERVER",buffer,ENV_FLG_BUILTIN);
	}

    if (reply->dr_bootfile && reply->dr_bootfile[0]) {
	env_setenv("BOOT_FILE",reply->dr_bootfile,ENV_FLG_BUILTIN);
	}

    if (reply->dr_script && reply->dr_script[0]) {
	env_setenv("BOOT_SCRIPT",reply->dr_script,ENV_FLG_BUILTIN);
	}

    if (reply->dr_options && reply->dr_options[0]) {
	env_setenv("BOOT_OPTIONS",reply->dr_options,ENV_FLG_BUILTIN);
	}
}

/*  *********************************************************************
    *  dhcp_dumptag(tag,len,buf)
    *  
    *  Dump out information from a DHCP tag
    *  
    *  Input parameters: 
    *  	   tag - tag ID
    *  	   len - length of data
    *  	   buf - data 
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#ifdef _DEBUG_
static void dhcp_dumptag(uint8_t tag,uint8_t len,uint8_t *buf)
{
    unsigned int idx;

    xprintf("DHCP: ");

    switch (tag) {
	case DHCP_TAG_FUNCTION:
	    xprintf("DHCP Function: %d\n",buf[0]);
	    break;
	case DHCP_TAG_LEASE_TIME:
	    idx = (((unsigned int) buf[0]) << 24) |
		(((unsigned int) buf[1]) << 16) |
		(((unsigned int) buf[2]) << 8) |
		(((unsigned int) buf[3]) << 0);
	    xprintf("Lease Time: %d seconds\n",idx);
	    break;
	case DHCP_TAG_SERVERIDENT:
	    xprintf("DHCP Server ID: %d.%d.%d.%d\n",
		   buf[0],buf[1],buf[2],buf[3]);
	    break;
	case DHCP_TAG_NETMASK:
	    xprintf("Netmask: %d.%d.%d.%d\n",buf[0],buf[1],buf[2],buf[3]);
	    break;
	case DHCP_TAG_DOMAINNAME:
	    buf[len] = 0;
	    xprintf("Domain: %s\n",buf);
	    break;
	case DHCP_TAG_GATEWAY:   
	    xprintf("Gateway: %d.%d.%d.%d\n",buf[0],buf[1],buf[2],buf[3]);
	    break;
	case DHCP_TAG_NAMESERVER:
	    xprintf("Nameserver: %d.%d.%d.%d\n",buf[0],buf[1],buf[2],buf[3]);
	    break;
	case DHCP_TAG_SWAPSERVER:
	    buf[len] = 0;
	    xprintf("Swapserver: %s\n",buf);
	    break;
	case DHCP_TAG_ROOTPATH:
	    buf[len] = 0;
	    xprintf("Rootpath: %s\n",buf);
	    break;
	case DHCP_TAG_SCRIPT:
	    buf[len] = 0;
	    xprintf("CFE Script: %s\n",buf);
	    break;
	case DHCP_TAG_OPTIONS:
	    buf[len] = 0;
	    xprintf("CFE Boot Options: %s\n",buf);
	    break;
	default:
	    xprintf("Tag %d len %d [",tag,len);
	    for (idx = 0; idx < len; idx++) {
		if ((buf[idx] >= 32) && (buf[idx] < 127)) xprintf("%c",buf[idx]);
		}
	    xprintf("]\n");
	    break;

	}
}
#endif

/*  *********************************************************************
    *  dhcp_free_reply(reply)
    *  
    *  Free memory associated with a DHCP reply.
    *  
    *  Input parameters: 
    *  	   reply - pointer to DHCP reply
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void dhcp_free_reply(dhcpreply_t *reply)
{
    if (reply->dr_hostname)   KFREE(reply->dr_hostname);
    if (reply->dr_domainname) KFREE(reply->dr_domainname);
    if (reply->dr_bootfile)   KFREE(reply->dr_bootfile);
    if (reply->dr_rootpath)   KFREE(reply->dr_rootpath);
    if (reply->dr_swapserver) KFREE(reply->dr_swapserver);
    if (reply->dr_script)     KFREE(reply->dr_script);
    if (reply->dr_options)    KFREE(reply->dr_options);
    KFREE(reply);
}

/*  *********************************************************************
    *  dhcp_build_discover()
    *  
    *  Build a DHCP DISCOVER packet
    *  
    *  Input parameters: 
    *  	   hwaddr - our hardware address
    *      idptr - pointer to int to receive the DHCP packet ID
    *      serveraddr - pointer to server address (REQUEST) or
    *                   NULL (DISCOVER)
    *      ebufptr - receives pointer to ebuf
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int dhcp_build_discover(uint8_t *hwaddr,
			       uint32_t id,
			       ebuf_t **ebufptr)
{
    uint8_t ipaddr[IP_ADDR_LEN];
    ebuf_t *buf;
    uint8_t junk[128];

    /*
     * Get a buffer and fill it in.
     */

    ipaddr[0] = 0; ipaddr[1] = 0; ipaddr[2] = 0; ipaddr[3] = 0;
    memset(junk,0,sizeof(junk));

    buf = udp_alloc();

    if (buf == NULL) {
	return CFE_ERR_NOMEM;
	}

    memset(buf->eb_ptr,0,548);

    ebuf_append_u8(buf,DHCP_OP_BOOTREQUEST);
    ebuf_append_u8(buf,DHCP_HWTYPE_ETHERNET);
    ebuf_append_u8(buf,ENET_ADDR_LEN);
    ebuf_append_u8(buf,0);			/* hops */
    ebuf_append_u32_be(buf,id);
    ebuf_append_u16_be(buf,0);			/* sec since boot */
    ebuf_append_u16_be(buf,0);			/* flags */

    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* ciaddr */
    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* yiaddr */
    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* siaddr */
    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* giaddr */

    ebuf_append_bytes(buf,hwaddr,ENET_ADDR_LEN); /* chaddr */
    ebuf_append_bytes(buf,junk,10);		 /* rest of chaddr */
    ebuf_append_bytes(buf,junk,64);		/* sname */
    ebuf_append_bytes(buf,junk,128);		/* file */

    ebuf_append_u32_be(buf,DHCP_MAGIC_NUMBER);

    ebuf_append_u8(buf,DHCP_TAG_FUNCTION);	/* function code */
    ebuf_append_u8(buf,1);
    ebuf_append_u8(buf,DHCP_FUNCTION_DISCOVER);

    ebuf_append_u8(buf,DHCP_TAG_PARAMLIST);
    ebuf_append_u8(buf,8);		/* count of tags that follow */
    ebuf_append_u8(buf,DHCP_TAG_NETMASK);
    ebuf_append_u8(buf,DHCP_TAG_DOMAINNAME);
    ebuf_append_u8(buf,DHCP_TAG_GATEWAY);
    ebuf_append_u8(buf,DHCP_TAG_NAMESERVER);
    ebuf_append_u8(buf,DHCP_TAG_SWAPSERVER);
    ebuf_append_u8(buf,DHCP_TAG_ROOTPATH);
    ebuf_append_u8(buf,DHCP_TAG_SCRIPT);
    ebuf_append_u8(buf,DHCP_TAG_OPTIONS);


    ebuf_append_u8(buf,DHCP_TAG_END);		/* terminator */
    
    /*
     * Return the packet
     */

    *ebufptr = buf;

    return 0;
}

/*  *********************************************************************
    *  dhcp_build_request()
    *  
    *  Build a DHCP DISCOVER or REQUEST packet
    *  
    *  Input parameters: 
    *  	   hwaddr - our hardware address
    *      idptr - pointer to int to receive the DHCP packet ID
    *      serveraddr - pointer to server address (REQUEST) or
    *                   NULL (DISCOVER)
    *      ebufptr - receives pointer to ebuf
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int dhcp_build_request(uint8_t *hwaddr,
			      uint32_t id,
			      uint8_t *serveraddr,
			      uint8_t *reqip,
			      ebuf_t **ebufptr)
{
    uint8_t ipaddr[IP_ADDR_LEN];
    ebuf_t *buf;
    uint8_t junk[128];

    /*
     * Get a buffer and fill it in.
     */

    ipaddr[0] = 0; ipaddr[1] = 0; ipaddr[2] = 0; ipaddr[3] = 0;
    memset(junk,0,sizeof(junk));

    buf = udp_alloc();

    if (buf == NULL) {
	return CFE_ERR_NOMEM;
	}

    memset(buf->eb_ptr,0,548);

    ebuf_append_u8(buf,DHCP_OP_BOOTREQUEST);
    ebuf_append_u8(buf,DHCP_HWTYPE_ETHERNET);
    ebuf_append_u8(buf,ENET_ADDR_LEN);
    ebuf_append_u8(buf,0);			/* hops */
    ebuf_append_u32_be(buf,id);
    ebuf_append_u16_be(buf,0);			/* sec since boot */
    ebuf_append_u16_be(buf,0);			/* flags */

    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* ciaddr */
    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* yiaddr */
    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* siaddr */
    ebuf_append_bytes(buf,ipaddr,IP_ADDR_LEN);	/* giaddr */

    ebuf_append_bytes(buf,hwaddr,ENET_ADDR_LEN); /* chaddr */
    ebuf_append_bytes(buf,junk,10);		 /* rest of chaddr */

    ebuf_append_bytes(buf,junk,64);		/* sname */

    ebuf_append_bytes(buf,junk,128);		/* file */

    ebuf_append_u32_be(buf,DHCP_MAGIC_NUMBER);

    ebuf_append_u8(buf,DHCP_TAG_FUNCTION);	/* function code */
    ebuf_append_u8(buf,1);

    ebuf_append_u8(buf,DHCP_FUNCTION_REQUEST);

    ebuf_append_u8(buf,DHCP_TAG_REQADDR);
    ebuf_append_u8(buf,IP_ADDR_LEN);
    ebuf_append_bytes(buf,reqip,IP_ADDR_LEN);

    ebuf_append_u8(buf,DHCP_TAG_SERVERIDENT); /* server ID */
    ebuf_append_u8(buf,IP_ADDR_LEN);
    ebuf_append_bytes(buf,serveraddr,IP_ADDR_LEN);

    ebuf_append_u8(buf,DHCP_TAG_CLIENTID);	/* client ID */
    ebuf_append_u8(buf,7);
    ebuf_append_u8(buf,1);
    ebuf_append_bytes(buf,hwaddr,ENET_ADDR_LEN); 

    ebuf_append_u8(buf,DHCP_TAG_PARAMLIST);
    ebuf_append_u8(buf,8);		/* count of tags that follow */
    ebuf_append_u8(buf,DHCP_TAG_NETMASK);
    ebuf_append_u8(buf,DHCP_TAG_DOMAINNAME);
    ebuf_append_u8(buf,DHCP_TAG_GATEWAY);
    ebuf_append_u8(buf,DHCP_TAG_NAMESERVER);
    ebuf_append_u8(buf,DHCP_TAG_SWAPSERVER);
    ebuf_append_u8(buf,DHCP_TAG_ROOTPATH);
    ebuf_append_u8(buf,DHCP_TAG_SCRIPT);
    ebuf_append_u8(buf,DHCP_TAG_OPTIONS);


    ebuf_append_u8(buf,DHCP_TAG_END);		/* terminator */
    
    /*
     * Return the packet
     */

    *ebufptr = buf;

    return 0;
}


/*  *********************************************************************
    *  dhcp_wait_reply(s,id,reply,serveraddr)
    *  
    *  Wait for a reply from the DHCP server
    *  
    *  Input parameters: 
    *  	   s - socket 
    *  	   id - ID of request we sent
    *  	   reply - structure to store results in
    *	   expfcode - expected DHCP_FUNCTION tag value
    *  	   
    *  Return value:
    *  	   0 if ok (reply found)
    *  	   else error
    ********************************************************************* */

static int dhcp_wait_reply(int s,uint32_t id,dhcpreply_t *reply,int expfcode)
{
    uint32_t tmpd;
    uint8_t tmpb;
    int64_t timer;
    int nres = 0;
    uint8_t ciaddr[IP_ADDR_LEN];
    uint8_t yiaddr[IP_ADDR_LEN];
    uint8_t siaddr[IP_ADDR_LEN];
    uint8_t giaddr[IP_ADDR_LEN];
    int8_t junk[128];
    char *hostname;
    char *bootfile;
    ebuf_t *buf;
    int fcode = -1;

    /*
     * Set a timer for the response
     */

    TIMER_SET(timer,DHCP_REQ_TIMEOUT);

    /*
     * Start waiting...
     */

    while (!TIMER_EXPIRED(timer)) {
	POLL();

	buf = udp_recv(s);
	if (!buf) continue;

	ebuf_get_u8(buf,tmpb);
	if (tmpb != DHCP_OP_BOOTREPLY) {
	    goto drop;
	    }

	ebuf_get_u8(buf,tmpb);
	if (tmpb != DHCP_HWTYPE_ETHERNET) {
	    goto drop;
	    }

	ebuf_get_u8(buf,tmpb);
	if (tmpb != ENET_ADDR_LEN) {
	    goto drop;
	    }

	ebuf_skip(buf,1);			/* hops */

	ebuf_get_u32_be(buf,tmpd);		/* check ID */
	if (tmpd != id) {
	    goto drop;
	    }

	ebuf_skip(buf,2);			/* seconds since boot */
	ebuf_skip(buf,2);			/* flags */

	ebuf_get_bytes(buf,ciaddr,IP_ADDR_LEN);
	ebuf_get_bytes(buf,yiaddr,IP_ADDR_LEN);
	ebuf_get_bytes(buf,siaddr,IP_ADDR_LEN);
	ebuf_get_bytes(buf,giaddr,IP_ADDR_LEN);

	ebuf_skip(buf,16);			/* hardware address */
	hostname = (char *)ebuf_ptr(buf);
	ebuf_skip(buf,64);
	bootfile = (char *)ebuf_ptr(buf);

	ebuf_skip(buf,128);

#ifdef _DEBUG_
	xprintf("Client  IP: %d.%d.%d.%d\n",ciaddr[0],ciaddr[1],ciaddr[2],ciaddr[3]);
	xprintf("Your    IP: %d.%d.%d.%d\n",yiaddr[0],yiaddr[1],yiaddr[2],yiaddr[3]);
	xprintf("Server  IP: %d.%d.%d.%d\n",siaddr[0],siaddr[1],siaddr[2],siaddr[3]);
	xprintf("Gateway IP: %d.%d.%d.%d\n",giaddr[0],giaddr[1],giaddr[2],giaddr[3]);
	xprintf("hostname: %s\n",hostname);
	xprintf("boot file: %s\n",bootfile);
#endif

	memcpy(reply->dr_ipaddr,yiaddr,IP_ADDR_LEN);
	memcpy(reply->dr_gateway,giaddr,IP_ADDR_LEN);
	memcpy(reply->dr_bootserver,siaddr,IP_ADDR_LEN);
	if (*hostname) reply->dr_hostname = strdup(hostname);
	if (*bootfile) reply->dr_bootfile = strdup(bootfile);

	/*
	 * Test for options - look for magic number
	 */

	ebuf_get_u32_be(buf,tmpd);

	memcpy(reply->dr_dhcpserver,buf->eb_usrptr,IP_ADDR_LEN);

	if (tmpd == DHCP_MAGIC_NUMBER) {
	    uint8_t tag;
	    uint8_t len;

	    while (buf->eb_length > 0) {
		ebuf_get_u8(buf,tag);
		if (tag == DHCP_TAG_END) break;
		ebuf_get_u8(buf,len);
		ebuf_get_bytes(buf,junk,len);

#ifdef _DEBUG_
		dhcp_dumptag(tag,len,junk);
#endif

		switch (tag) {
		    case DHCP_TAG_FUNCTION:
			fcode = (uint8_t) junk[0];
			break;
		    case DHCP_TAG_NETMASK:
			memcpy(reply->dr_netmask,junk,IP_ADDR_LEN);
			break;
		    case DHCP_TAG_GATEWAY:   
			memcpy(reply->dr_gateway,junk,IP_ADDR_LEN);
			break;
		    case DHCP_TAG_NAMESERVER:
			memcpy(reply->dr_nameserver,junk,IP_ADDR_LEN);
			break;
		    case DHCP_TAG_DOMAINNAME:
			junk[len] = 0;
			if (len) reply->dr_domainname = strdup(junk);
			break;
		    case DHCP_TAG_SWAPSERVER:
			junk[len] = 0;
			if (len) reply->dr_swapserver = strdup(junk);
			break;
		    case DHCP_TAG_SERVERIDENT:
			if (len == IP_ADDR_LEN) {
			    memcpy(reply->dr_dhcpserver,junk,len);
			    }
			break;
		    case DHCP_TAG_ROOTPATH:
			junk[len] = 0;
			if (len) reply->dr_rootpath = strdup(junk);
			break;
		    case DHCP_TAG_SCRIPT:
			junk[len] = 0;
			if (len) reply->dr_script = strdup(junk);
			break;
		    case DHCP_TAG_OPTIONS:
			junk[len] = 0;
			if (len) reply->dr_options = strdup(junk);
			break;
		    }
		}
	    }

	if (fcode != expfcode) {
	    goto drop;
	    }

	udp_free(buf);
	nres++;
	break;

    drop:
	udp_free(buf);
	}

    if (nres > 0) return 0;
    else return CFE_ERR_TIMEOUT;
}


/*  *********************************************************************
    *  dhcp_do_dhcpdiscover(s,hwaddr,reply)
    *  
    *  Request an IP address from the DHCP server.  On success, the
    *  dhcpreply_t structure will be filled in
    *  
    *  Input parameters: 
    *      s - udp socket
    *      hwaddr - our hardware address
    *  	   reply - pointer to reply buffer.
    *  	   
    *  Return value:
    *  	   0 if a response was received
    *  	   else error code
    ********************************************************************* */

static int dhcp_do_dhcpdiscover(int s,uint8_t *hwaddr,dhcpreply_t *reply)
{
    ebuf_t *buf;
    uint32_t id;
    uint8_t ipaddr[IP_ADDR_LEN];
    int res;

    /*
     * Packet ID is the current time
     */
 
    id = (uint32_t)cfe_ticks;

    /*
     * Build the DISCOVER request
     */

    res = dhcp_build_discover(hwaddr,id,&buf);
    if (res != 0) return res;

    /*
     * Send the packet to the IP broadcast (255.255.255.255)
     */

    ipaddr[0] = 0xFF; ipaddr[1] = 0xFF; ipaddr[2] = 0xFF; ipaddr[3] = 0xFF;
    udp_send(s,buf,ipaddr);

    /*
     * Wait for a reply
     */

    res = dhcp_wait_reply(s,id,reply,DHCP_FUNCTION_OFFER);

    return res;
}


/*  *********************************************************************
    *  dhcp_do_dhcprequest(s,hwaddr,reply,discover_reply)
    *  
    *  Request an IP address from the DHCP server.  On success, the
    *  dhcpreply_t structure will be filled in
    *  
    *  Input parameters: 
    *      s - udp socket
    *      hwaddr - our hardware address
    *  	   reply - pointer to reply buffer.
    *	   discover_reply - pointer to previously received DISCOVER data
    *  	   
    *  Return value:
    *  	   0 if a response was received
    *  	   else error code
    ********************************************************************* */

static int dhcp_do_dhcprequest(int s,uint8_t *hwaddr,
			       dhcpreply_t *reply,
			       dhcpreply_t *discover_reply)
{
    ebuf_t *buf;
    uint32_t id;
    uint8_t ipaddr[IP_ADDR_LEN];
    int res;

    /*
     * Packet ID is the current time
     */
 
    id = (uint32_t)cfe_ticks;
 
    /*
     * Build the DHCP REQUEST request
     */

    res = dhcp_build_request(hwaddr,
			     id,
			     discover_reply->dr_dhcpserver,
			     discover_reply->dr_ipaddr,
			     &buf);

    if (res != 0) return res;

    /*
     * Send the packet to the IP broadcast (255.255.255.255)
     */

    ipaddr[0] = 0xFF; ipaddr[1] = 0xFF; ipaddr[2] = 0xFF; ipaddr[3] = 0xFF;
    udp_send(s,buf,ipaddr);

    /*
     * Wait for a reply
     */

    res = dhcp_wait_reply(s,id,reply,DHCP_FUNCTION_ACK);

    return res;
}


/*  *********************************************************************
    *  dhcp_bootrequest(reply)
    *  
    *  Request an IP address from the DHCP server.  On success, the
    *  dhcpreply_t structure will be allocated.
    *  
    *  Input parameters: 
    *  	   reply - pointer to pointer to reply.
    *  	   
    *  Return value:
    *  	   0 if no responses received
    *     >0 for some responses received
    *  	   else error code
    ********************************************************************* */

int dhcp_bootrequest(dhcpreply_t **rep)
{
    uint8_t *hwaddr;
    int s;
    dhcpreply_t *discover_reply;
    dhcpreply_t *request_reply;
    int nres = 0;
    int retries;
    uint32_t id;

    id = (uint32_t) cfe_ticks;

    /*
     * Start with empty reply buffers.  Since we use a portion of the
     * discover reply in the request, we'll keep two of them.
     */

    discover_reply = KMALLOC(sizeof(dhcpreply_t),0);
    if (discover_reply == NULL) {
	return CFE_ERR_NOMEM;
	}
    memset(discover_reply,0,sizeof(dhcpreply_t));

    request_reply = KMALLOC(sizeof(dhcpreply_t),0);
    if (request_reply == NULL) {
	KFREE(discover_reply);
	return CFE_ERR_NOMEM;
	}
    memset(request_reply,0,sizeof(dhcpreply_t));


    /*
     * Get our hw addr
     */

    hwaddr = net_getparam(NET_HWADDR);
    if (!hwaddr) {
	KFREE(discover_reply);
	KFREE(request_reply);
	return CFE_ERR_NETDOWN;
	}

    /*
     * Open UDP port
     */

    s = udp_socket(UDP_PORT_BOOTPS);
    if (s < 0) {
	KFREE(discover_reply);
	KFREE(request_reply);
	return CFE_ERR_NOHANDLES;
	}

    udp_bind(s,UDP_PORT_BOOTPC);

    /*
     * Do the boot request.  Start by sending the OFFER message
     */

    nres = CFE_ERR_TIMEOUT;
    for (retries = 0; retries < DHCP_NUM_RETRIES; retries++) {
	nres = dhcp_do_dhcpdiscover(s,hwaddr,discover_reply);
	if (nres == 0) break;
	if (nres == CFE_ERR_TIMEOUT) continue;
	break;
	}

    /*
     * If someone sent us a response, send the REQUEST message
     * to get a lease.
     */

    if (nres == 0) {

	/*
	 * Now, send the REQUEST message and get a response.
	 */

	for (retries = 0; retries < DHCP_NUM_RETRIES; retries++) {
	    nres = dhcp_do_dhcprequest(s,hwaddr,
				       request_reply,
				       discover_reply);
	    if (nres == 0) break;
	    if (nres == CFE_ERR_TIMEOUT) continue;
	    break;
	    }
	}

    /*
     * All done with the discover reply.
     */

    dhcp_free_reply(discover_reply);

    /*
     * All done with UDP
     */

    udp_close(s);

    /*
     * Return the reply info.
     */

    if (nres == 0) {
	*rep = request_reply;
	}
    else {
	*rep = NULL;
	dhcp_free_reply(request_reply);
	}

    return nres;
}
#endif	/* CFG_DHCP */

/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Address Resolution Protocol		File: net_arp.c
    *  
    *  This module implements RFC826, the Address Resolution Protocol.
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

#include "net_ebuf.h"
#include "net_ether.h"

#include "net_ip.h"
#include "net_ip_internal.h"


/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static int arp_tx_query(ip_info_t *ipi,uint8_t *destaddr);
static void arp_delete(arpentry_t *ae);
static void arp_tx_waiting(arpentry_t *ae);
static arpentry_t *arp_new_entry(ip_info_t *ipi);
static arpentry_t *arp_find_entry(ip_info_t *ipi,uint8_t *ipaddr);
static int arp_rx_query(ip_info_t *ipi,uint8_t *srcaddr,
			uint8_t *targethw,uint8_t *targetip);
static int arp_rx_response(ip_info_t *ipi,uint8_t *senderhw,
			   uint8_t *senderip);
static int arp_rx_callback(ebuf_t *buf,void *ref);


/*  *********************************************************************
    *  arp_tx_query(ipi,destaddr)
    *  
    *  Transmit an ARP QUERY message.  ARP QUERY messages are sent
    *  to the Ethernet broadcast address.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   destaddr - IP address to query
    *  	   
    *  Return value:
    *  	   0 - success
    *  	   <0 - failure
    ********************************************************************* */

static int arp_tx_query(ip_info_t *ipi,uint8_t *destaddr)
{
    ebuf_t *buf;
    uint8_t hwaddr[ENET_ADDR_LEN];

    /*
     * Get a buffer.
     */

    buf = eth_alloc(ipi->eth_info,ipi->arp_port);
    if (!buf) return -1;

    /*
     * fill in the fields
     */

    ebuf_append_u16_be(buf,ARP_HWADDRSPACE_ETHERNET);
    ebuf_append_u16_be(buf,PROTOSPACE_IP);
    ebuf_append_u8(buf,ENET_ADDR_LEN);
    ebuf_append_u8(buf,IP_ADDR_LEN);
    ebuf_append_u16_be(buf,ARP_OPCODE_REQUEST);
    ebuf_append_bytes(buf,ipi->arp_hwaddr,ENET_ADDR_LEN);
    ebuf_append_bytes(buf,ipi->net_info.ip_addr,IP_ADDR_LEN);
    memset(hwaddr,0,ENET_ADDR_LEN);
    ebuf_append_bytes(buf,hwaddr,ENET_ADDR_LEN);
    ebuf_append_bytes(buf,destaddr,IP_ADDR_LEN);

    /*
     * Transmit the packet
     */

    eth_send(buf,(uint8_t *)eth_broadcast);
    eth_free(buf);

    return 0;
}


/*  *********************************************************************
    *  arp_delete(ae)
    *  
    *  Delete an ARP entry.  The usual reason for calling this routine
    *  is to reclaim unused ARP entries, but an ARP entry may be 
    *  manually deleted as well.
    *  
    *  Input parameters: 
    *  	   ae - arp entry 
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void arp_delete(arpentry_t *ae)
{
    ebuf_t *buf;

    /*
     * Free any buffers associated with the ARP entry.
     */
	
    while ((buf = (ebuf_t *) q_deqnext(&(ae->ae_txqueue)))) {
	eth_free(buf);
	}

    /*
     * Reset the important fields
     */

    ae->ae_timer = 0;
    ae->ae_usage = 0;
    ae->ae_retries = 0;
    ae->ae_state = ae_unused;
}


/*  *********************************************************************
    *  _arp_add(ipi,destip,desthw)
    *  
    *  Add a new ARP entry to the ARP table [internal routine].   If
    *  there is no room in the table, some other entry is kicked
    *  out.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   destip - target IP address
    *  	   desthw - target hardware address
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _arp_add(ip_info_t *ipi,uint8_t *destip,uint8_t *desthw)
{
    arpentry_t *ae;

    ae = arp_find_entry(ipi,desthw);
    if (!ae) {
	ae = arp_new_entry(ipi);
	}

    memcpy(ae->ae_ipaddr,destip,IP_ADDR_LEN);
    memcpy(ae->ae_ethaddr,desthw,ENET_ADDR_LEN);

    ae->ae_retries = 0;
    ae->ae_timer = 0;		/* keep forever */
    ae->ae_permanent = TRUE;
    ae->ae_state = ae_established;

    arp_tx_waiting(ae);
}


/*  *********************************************************************
    *  _arp_lookup(ipi,destip)
    *  
    *  Look up an ARP entry [internal routine].   Given an IP address,
    *  return the hardware address to send the packets to, or
    *  NULL if no ARP entry exists.
    *  
    *  Input parameters: 
    *  	   ipi - IP information 
    *  	   destip - destination IP address
    *  	   
    *  Return value:
    *  	   pointer to Ethernet hardware address, or NULL if not found
    ********************************************************************* */

uint8_t *_arp_lookup(ip_info_t *ipi,uint8_t *destip)
{
    arpentry_t *ae;

    ae = arp_find_entry(ipi,destip);
    if (ae == NULL) return NULL;

    return ae->ae_ethaddr;
}

/*  *********************************************************************
    *  _arp_lookup_and_send(ipi,buf,dest)
    *  
    *  Transmit a packet [internal routine].  This routine is called
    *  by the IP layer when it wants to send a packet.  We look
    *  through the ARP table to find a suitable destination host and
    *  transmit the packet.  If there is no ARP entry, an ARP request
    *  is transmitted and the packet is saved on a queue for when
    *  the address is resolved.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   buf - ebuf to transmit
    *  	   dest - destination IP address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   <0 if error
    ********************************************************************* */

int _arp_lookup_and_send(ip_info_t *ipi,ebuf_t *buf,uint8_t *dest)
{
    arpentry_t *ae;

    ae = arp_find_entry(ipi,dest);

    if (ae == NULL) {
	/*
	 * No ARP entry yet, create one and send the query
	 */
	ae = arp_new_entry(ipi);
	memcpy(ae->ae_ipaddr,dest,IP_ADDR_LEN);
	q_enqueue(&(ae->ae_txqueue),(queue_t *) buf);
	ae->ae_retries = ARP_QUERY_RETRIES;
	ae->ae_timer = ARP_QUERY_TIMER;
	ae->ae_state = ae_arping;
	arp_tx_query(ipi,ae->ae_ipaddr);
	}
    else {
	/*
	 * have an ARP entry.  If established, just send the
	 * packet now.  Otherwise, queue on arp queue if there's room.
	 */
	if (ae->ae_state == ae_established) {
	    ae->ae_usage++;
	    if (!ae->ae_permanent) {
		ae->ae_timer = ARP_KEEP_TIMER;
		}
	    eth_send(buf,ae->ae_ethaddr);
	    eth_free(buf);
	    }
	else {
	    if (q_count(&(ae->ae_txqueue)) < ARP_TXWAIT_MAX) {
		q_enqueue(&(ae->ae_txqueue),(queue_t *) buf);
		}
	    else {
		/* no room, silently drop */
		eth_free(buf);
		}
	    }
	}

    return 0;
}

/*  *********************************************************************
    *  arp_tx_waiting(ae)
    *  
    *  Transmit all pending packets on the specified ARP entry's 
    *  queue.  Packets get queued to an ARP entry when the address
    *  has not completed resolution.  Once resolved, this routine
    *  is called to flush the packets out.
    *  
    *  Input parameters: 
    *  	   ae - arp entry
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void arp_tx_waiting(arpentry_t *ae)
{
    ebuf_t *buf;

    while ((buf = (ebuf_t *) q_deqnext(&(ae->ae_txqueue)))) {
	eth_send(buf,ae->ae_ethaddr);
	eth_free(buf);
	}
}


/*  *********************************************************************
    *  arp_new_entry(ipi)
    *  
    *  Create a new ARP entry, deleting an active entry if necessary.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   
    *  Return value:
    *  	   arp entry pointer
    ********************************************************************* */

static arpentry_t *arp_new_entry(ip_info_t *ipi)
{
    arpentry_t *ae;
    arpentry_t *victim = NULL;
    int idx;
    int minusage = 0x7FFFFFFF;

    /*
     * First scan the table and find an empty entry.
     */

    ae = ipi->arp_table;
    for (idx = 0; idx < ARP_TABLE_SIZE; idx++,ae++) {
	if (ae->ae_state == ae_unused) {
	    return ae;
	    }
	}

    /*
     * If all entries are in use, pick the one with the
     * lowest usage count.  This isn't very scientific,
     * and perhaps should use a timer of some sort.
     */

    ae = ipi->arp_table;
    for (idx = 0; idx < ARP_TABLE_SIZE; idx++,ae++) {
	if (ae->ae_usage < minusage) {
	    victim = ae;
	    minusage = ae->ae_usage;
	    }
	}

    /*
     * In the highly unlikely event that all entries have
     * overflow values in their usage counters, just take the
     * first table entry.
     */

    if (victim == NULL) victim = ipi->arp_table;

    /*
     * Clear out the old entry and use it.
     */

    arp_delete(victim);

    return victim;
}

/*  *********************************************************************
    *  arp_find_entry(ipi,ipaddr)
    *  
    *  Find an ARP entry in the table.  Given an IP address, this
    *  routine locates the corresponding ARP table entry.  We also
    *  reset the expiration timer for the ARP entry, to prevent
    *  it from from being deleted.
    *  
    *  Input parameters: 
    *  	   ipi - IP info
    *  	   ipaddr - IP address we're looking for
    *  	   
    *  Return value:
    *  	   arp entry pointer, or NULL if not found
    ********************************************************************* */

static arpentry_t *arp_find_entry(ip_info_t *ipi,uint8_t *ipaddr)
{
    arpentry_t *ae;
    int idx;

    ae = ipi->arp_table;

    for (idx = 0; idx < ARP_TABLE_SIZE; idx++,ae++) {
	if (ae->ae_state != ae_unused) {
	    if (memcmp(ae->ae_ipaddr,ipaddr,IP_ADDR_LEN) == 0) {
		if (ae->ae_state == ae_established)
		    ae->ae_timer = ARP_KEEP_TIMER;
		return ae;
		}
	    }
	}
    return NULL;
}


/*  *********************************************************************
    *  arp_rx_query(ipi,srcaddr,targethw,targetip)
    *  
    *  Process a received ARP QUERY message.  When we get an ARP,
    *  transmit a reply to the sender.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   srcaddr - source IP address
    *  	   targethw - target hardware address
    *  	   targetip - target IP address (should be our address)
    *  	   
    *  Input parameters: 
    *  	   0 if ok
    *  	   else <0 = error
    ********************************************************************* */

static int arp_rx_query(ip_info_t *ipi,uint8_t *srcaddr,
			uint8_t *targethw,uint8_t *targetip)
{
    ebuf_t *txbuf;

    /*
     * Allocate a packet and form the reply
     */

    txbuf = eth_alloc(ipi->eth_info,ipi->arp_port);
    if (!txbuf) return -1;

    ebuf_append_u16_be(txbuf,ARP_HWADDRSPACE_ETHERNET);
    ebuf_append_u16_be(txbuf,PROTOSPACE_IP);
    ebuf_append_u8(txbuf,ENET_ADDR_LEN);
    ebuf_append_u8(txbuf,IP_ADDR_LEN);
    ebuf_append_u16_be(txbuf,ARP_OPCODE_REPLY);

    ebuf_append_bytes(txbuf,ipi->arp_hwaddr,ENET_ADDR_LEN);
    ebuf_append_bytes(txbuf,ipi->net_info.ip_addr,IP_ADDR_LEN);

    ebuf_append_bytes(txbuf,targethw,ENET_ADDR_LEN);
    ebuf_append_bytes(txbuf,targetip,IP_ADDR_LEN);

    eth_send(txbuf,srcaddr);
    eth_free(txbuf);

    return 0;
}

/*  *********************************************************************
    *  arp_rx_response(ipi,senderhw,senderip)
    *  
    *  Process a received ARP RESPONSE packet.  This packet contains
    *  the hardware address of some host we were querying.  Fill
    *  in the rest of the entries in the ARP table and
    *  transmit any pending packets.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   senderhw - sender's hardware address
    *  	   senderip - sender's IP address
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int arp_rx_response(ip_info_t *ipi,uint8_t *senderhw,uint8_t *senderip)
{
    int idx;
    arpentry_t *ae;

    ae = ipi->arp_table;

    for (idx = 0; idx < ARP_TABLE_SIZE; idx++,ae++) {
	if (ae->ae_state != ae_unused) {
	    if (memcmp(ae->ae_ipaddr,senderip,IP_ADDR_LEN) == 0) {
		memcpy(ae->ae_ethaddr,senderhw,ENET_ADDR_LEN);
		ae->ae_state = ae_established;
		ae->ae_timer = ARP_KEEP_TIMER;
		ae->ae_retries = 0;
		ae->ae_permanent = FALSE;
		arp_tx_waiting(ae);
		}
	    }
	}

    return 0;
}

/*  *********************************************************************
    *  arp_rx_callback(buf,ref)
    *  
    *  Callback for ARP protocol packets.  This routine is called
    *  by the datalink layer when we receive an ARP packet.  Parse
    *  the packet and call any packet-specific processing routines
    *  
    *  Input parameters: 
    *  	   buf - ebuf that we received
    *  	   ref - reference data when we opened the port.  This is
    *  	         our IP information structure
    *  	   
    *  Return value:
    *  	   ETH_DROP or ETH_KEEP.
    ********************************************************************* */

static int arp_rx_callback(ebuf_t *buf,void *ref)
{
    ip_info_t *ipi = ref;
    uint16_t t16;
    uint8_t t8;
    uint16_t opcode;
    uint8_t senderip[IP_ADDR_LEN];
    uint8_t senderhw[ENET_ADDR_LEN];
    uint8_t targetip[IP_ADDR_LEN];
    uint8_t targethw[ENET_ADDR_LEN];

    /*
     * ARP packets have to be at least 28 bytes 
     */

    if (ebuf_length(buf) < 28) goto drop;

    /*
     * We only do the Ethernet hardware space 
     */

    ebuf_get_u16_be(buf,t16);
    if (t16 != ARP_HWADDRSPACE_ETHERNET) goto drop;

    /*
     * We only do the IP protocol space
     */

    ebuf_get_u16_be(buf,t16);
    if (t16 != PROTOSPACE_IP) goto drop;

    /*
     * The IP and Ethernet address lengths had better be right.
     */

    ebuf_get_u8(buf,t8);
    if (t8 != ENET_ADDR_LEN) goto drop;

    ebuf_get_u8(buf,t8);
    if (t8 != IP_ADDR_LEN) goto drop;

    /*
     * Get the opcode and other fields.
     */

    ebuf_get_u16_be(buf,opcode);

    ebuf_get_bytes(buf,senderhw,ENET_ADDR_LEN);
    ebuf_get_bytes(buf,senderip,IP_ADDR_LEN);
    ebuf_get_bytes(buf,targethw,ENET_ADDR_LEN);
    ebuf_get_bytes(buf,targetip,IP_ADDR_LEN);

    /*
     * If it's not for us, just drop it.
     */

    if (memcmp(targetip,ipi->net_info.ip_addr,IP_ADDR_LEN) != 0) goto drop;

    /*
     * Dispatch to an appropriate routine.
     */

    switch (opcode) {
	case ARP_OPCODE_REQUEST:
	    arp_rx_query(ipi,ebuf_srcaddr(buf),senderhw,senderip);
	    break;
	case ARP_OPCODE_REPLY:
	    arp_rx_response(ipi,senderhw,senderip);
	    break;
	}

drop:
    return ETH_DROP;
}

/*  *********************************************************************
    *  _arp_timer_tick(ipi)
    *  
    *  ARP timer processing [internal routine].  This routine 
    *  counts down timer ticks in the ARP entries, causing retransmits
    *  or ARP entry expirations to happen.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _arp_timer_tick(ip_info_t *ipi)
{
    int idx;
    arpentry_t *ae;

    ae = ipi->arp_table;

    /*
     * Walk through the ARP table.
     */

    for (idx = 0; idx < ARP_TABLE_SIZE; idx++,ae++) {

	switch (ae->ae_state) {
	    case ae_unused:
		/*
		 * Unused entry.  Do nothing.
		 */
		break;

	    case ae_arping:
		/* 
		 * Entry is arping.  Count down the timer, and retransmit
		 * the ARP message.
		 */
		ae->ae_timer--;
		if (ae->ae_timer <= 0) {
		    if (ae->ae_retries == 0) {
			arp_delete(ae);
			}
		    else {
			ae->ae_retries--;
			ae->ae_timer = ARP_QUERY_TIMER;
			arp_tx_query(ipi,ae->ae_ipaddr);
			}
		    }
		break;

	    case ae_established:
		/*
		 * Established entry.  Count down the timer and
		 * delete the ARP entry.  If the timer is zero
		 * already, it's a permanent ARP entry.
		 */
		if (ae->ae_timer == 0) break;
		ae->ae_timer--;
		if (ae->ae_timer == 0) arp_delete(ae);
		break;
	    }
	}

}

/*  *********************************************************************
    *  _arp_send_gratuitous(ipi)
    *  
    *  Transmit the "gratuitous arp" (an ARP for our own IP address).
    *  This is done customarily when an interface is initialized.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _arp_send_gratuitous(ip_info_t *ipi)
{
    if (!ip_addriszero(ipi->net_info.ip_addr)) {
	arp_tx_query(ipi,ipi->net_info.ip_addr);
	}
}

/*  *********************************************************************
    *  _arp_init(ipi)
    *  
    *  Initialize the ARP layer [internal routine]
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _arp_init(ip_info_t *ipi)
{
    uint8_t arpproto[2];
    int idx;
    arpentry_t *ae;

    /*
     * Allocate space for the ARP table
     */

    ipi->arp_table = KMALLOC(ARP_TABLE_SIZE*sizeof(arpentry_t),0);

    if (ipi->arp_table == NULL) return NULL;

    /*
     * Initialize the ARP table.
     */

    ae = ipi->arp_table;
    for (idx = 0; idx < ARP_TABLE_SIZE; idx++) {
	ae->ae_state = ae_unused;
	ae->ae_timer = 0;
	ae->ae_usage = 0;
	ae->ae_retries = 0;
	ae->ae_permanent = 0;
	q_init(&(ae->ae_txqueue));
	ae++;
	}

    /*
     * Open the Ethernet portal for ARP packets
     */

    arpproto[0] = (PROTOSPACE_ARP >> 8) & 0xFF;
    arpproto[1] = (PROTOSPACE_ARP & 0xFF);
    ipi->arp_port = eth_open(ipi->eth_info,ETH_PTYPE_DIX,(int8_t *)arpproto,arp_rx_callback,ipi);

    if (ipi->arp_port < 0) {
	KFREE(ipi->arp_table);
	ipi->arp_table = NULL;
	return -1;
	}

    /*
     * Remember our hardware address
     */

    eth_gethwaddr(ipi->eth_info,ipi->arp_hwaddr);

    /* 
     * Send a query for ourselves if our IP address is set 
     */

    _arp_send_gratuitous(ipi);

    return 0;

}


/*  *********************************************************************
    *  _arp_uninit(ipi)
    *  
    *  Uninitialize the ARP interface.  This is called when the 
    *  network module is shut down.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _arp_uninit(ip_info_t *ipi)
{
    int idx;
    arpentry_t *ae;

    /*
     * Close the Ethernet portal
     */

    eth_close(ipi->eth_info,ipi->arp_port);

    /*
     * Clear out the ARP Table.
     */

    ae = ipi->arp_table;
    for (idx = 0; idx < ARP_TABLE_SIZE; idx++) {
	if (ae->ae_state != ae_unused) arp_delete(ae);
	ae++;
	}

    /*
     * Free up the memory.
     */

    KFREE(ipi->arp_table);
    ipi->arp_table = NULL;
    ipi->arp_port = -1;
}


/*  *********************************************************************
    *  _arp_enumerate(ipi,entrynum,ipaddr,hwaddr)
    *  
    *  Enumerate the ARP table.  This is used by user-interface
    *  routines to display the current contents of the ARP table.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   entrynum - entry index
    *  	   ipaddr - buffer to copy entry's IP address to
    *  	   hwaddr - buffer to copy entry's hardware address to
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _arp_enumerate(ip_info_t *ipi,int entrynum,uint8_t *ipaddr,uint8_t *hwaddr)
{
    arpentry_t *ae;
    int idx;

    ae = ipi->arp_table;
    for (idx = 0; idx < ARP_TABLE_SIZE; idx++) {
	if (ae->ae_state != ae_unused) {
	    if (entrynum == 0) {
		memcpy(ipaddr,ae->ae_ipaddr,IP_ADDR_LEN);
		memcpy(hwaddr,ae->ae_ethaddr,ENET_ADDR_LEN);
		return 0;
		}
	    entrynum--;
	    }
	ae++;
	}

    return -1;
}


/*  *********************************************************************
    *  _arp_delete(ipi,ipaddr)
    *  
    *  Delete an ARP entry.  This routine takes an IP address, looks
    *  up its ARP table entry, and removes it from the table.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   ipaddr - IP address whose entry to delete
    *  	   
    *  Return value:
    *  	   0 if entry was deleted
    *  	   <0 if entry was not found
    ********************************************************************* */

int _arp_delete(ip_info_t *ipi,uint8_t *ipaddr)
{
    arpentry_t *ae;

    ae = arp_find_entry(ipi,ipaddr);

    if (ae) {
	arp_delete(ae);
	return 0;
	}
    return -1;
}

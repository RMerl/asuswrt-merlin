/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Internal IP structures			File: net_ip_internal.h
    *  
    *  This module contains non-public IP stack constants, 
    *  structures, and function prototypes.
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


/*  *********************************************************************
    *  ARP Protocol
    ********************************************************************* */

#define ARP_OPCODE_REQUEST	1
#define ARP_OPCODE_REPLY	2

#define ARP_HWADDRSPACE_ETHERNET 1

#define PROTOSPACE_IP        0x0800
#define PROTOSPACE_ARP	     0x0806

#define ARP_KEEP_TIMER          60
#define ARP_QUERY_TIMER		1
#define ARP_QUERY_RETRIES	4
#define ARP_TXWAIT_MAX		2
#define ARP_TABLE_SIZE		8

typedef enum { ae_unused, ae_arping, ae_established } arpstate_t;

typedef struct arpentry_s {
    arpstate_t ae_state;
    int ae_usage;
    int ae_timer;
    int ae_retries;
    int ae_permanent;
    uint8_t ae_ipaddr[IP_ADDR_LEN];
    uint8_t ae_ethaddr[ENET_ADDR_LEN];
    queue_t ae_txqueue;
} arpentry_t;


/*  *********************************************************************
    *  IP Protocol
    ********************************************************************* */

#define IPHDR_VER_4	0x40
#define IPHDR_LEN_20	0x05
#define IPHDR_LENGTH	20
#define IPHDR_TOS_DEFAULT 0x00
#define IPHDR_TTL_DEFAULT 100

#define IPHDR_RESERVED 0x8000
#define IPHDR_DONTFRAGMENT 0x4000
#define IPHDR_MOREFRAGMENTS 0x2000
#define IPHDR_FRAGOFFSET 0x01FFF

typedef struct ip_protodisp_s {
    uint8_t protocol;
    int (*cb)(void *ref,ebuf_t *buf,uint8_t *dst,uint8_t *src);
    void *ref;
} ip_protodisp_t;

#define IP_MAX_PROTOCOLS	4


struct ip_info_s {
    net_info_t net_info;

    /* Ethernet info */
    ether_info_t *eth_info;

    /* Info specific to IP */
    uint16_t ip_id;
    int ip_port;

    /* IP protocol dispatch table */
    ip_protodisp_t ip_protocols[IP_MAX_PROTOCOLS];

    /* Info specific to ARP */
    arpentry_t *arp_table;
    int arp_port;
    uint8_t arp_hwaddr[ENET_ADDR_LEN];
};

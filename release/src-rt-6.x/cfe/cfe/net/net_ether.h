/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Ethernet protocol demux defns		File: net_ether.h
    *  
    *  constants and prototypes for the Ethernet datalink
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
    *  Constants
    ********************************************************************* */

#define ETH_PTYPE_NONE 0
#define ETH_PTYPE_DIX	1
#define ETH_PTYPE_802SAP	2
#define ETH_PTYPE_802SNAP	3

#define ETH_KEEP	1
#define ETH_DROP	0

/*  *********************************************************************
    *  types
    ********************************************************************* */

typedef struct ether_info_s ether_info_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int eth_open(ether_info_t *eth,int ptype,char *pdata,int (*cb)(ebuf_t *buf,void *ref),void *ref);
void eth_close(ether_info_t *eth,int port);
void eth_poll(ether_info_t *eth);
int eth_send(ebuf_t *buf,uint8_t *dest);
ebuf_t *eth_alloc(ether_info_t *eth,int port);
void eth_free(ebuf_t *buf);
ether_info_t *eth_init(char *devname);
void eth_uninit(ether_info_t *eth);
int eth_getmtu(ether_info_t *,int port);
void eth_gethwaddr(ether_info_t *,uint8_t *hwaddr);
void eth_sethwaddr(ether_info_t *,uint8_t *hwaddr);
int eth_getspeed(ether_info_t *,int *speed);
int eth_setspeed(ether_info_t *,int speed);
int eth_getloopback(ether_info_t *,int *loop);
int eth_setloopback(ether_info_t *,int loop);
extern const uint8_t eth_broadcast[ENET_ADDR_LEN];

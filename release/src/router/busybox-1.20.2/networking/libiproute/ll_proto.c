/* vi: set sw=4 ts=4: */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Authors: Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include "libbb.h"
#include "rt_names.h"
#include "utils.h"

#include <netinet/if_ether.h>

#if !ENABLE_WERROR
#warning de-bloat
#endif
/* Before re-enabling this, please (1) conditionalize exotic protocols
 * on CONFIG_something, and (2) decouple strings and numbers
 * (use llproto_ids[] = n,n,n..; and llproto_names[] = "loop\0" "pup\0" ...;)
 */

#define __PF(f,n) { ETH_P_##f, #n },
static struct {
	int id;
	const char *name;
} llproto_names[] = {
__PF(LOOP,loop)
__PF(PUP,pup)
#ifdef ETH_P_PUPAT
__PF(PUPAT,pupat)
#endif
__PF(IP,ip)
__PF(X25,x25)
__PF(ARP,arp)
__PF(BPQ,bpq)
#ifdef ETH_P_IEEEPUP
__PF(IEEEPUP,ieeepup)
#endif
#ifdef ETH_P_IEEEPUPAT
__PF(IEEEPUPAT,ieeepupat)
#endif
__PF(DEC,dec)
__PF(DNA_DL,dna_dl)
__PF(DNA_RC,dna_rc)
__PF(DNA_RT,dna_rt)
__PF(LAT,lat)
__PF(DIAG,diag)
__PF(CUST,cust)
__PF(SCA,sca)
__PF(RARP,rarp)
__PF(ATALK,atalk)
__PF(AARP,aarp)
__PF(IPX,ipx)
__PF(IPV6,ipv6)
#ifdef ETH_P_PPP_DISC
__PF(PPP_DISC,ppp_disc)
#endif
#ifdef ETH_P_PPP_SES
__PF(PPP_SES,ppp_ses)
#endif
#ifdef ETH_P_ATMMPOA
__PF(ATMMPOA,atmmpoa)
#endif
#ifdef ETH_P_ATMFATE
__PF(ATMFATE,atmfate)
#endif

__PF(802_3,802_3)
__PF(AX25,ax25)
__PF(ALL,all)
__PF(802_2,802_2)
__PF(SNAP,snap)
__PF(DDCMP,ddcmp)
__PF(WAN_PPP,wan_ppp)
__PF(PPP_MP,ppp_mp)
__PF(LOCALTALK,localtalk)
__PF(PPPTALK,ppptalk)
__PF(TR_802_2,tr_802_2)
__PF(MOBITEX,mobitex)
__PF(CONTROL,control)
__PF(IRDA,irda)
#ifdef ETH_P_ECONET
__PF(ECONET,econet)
#endif

{ 0x8100, "802.1Q" },
{ ETH_P_IP, "ipv4" },
};
#undef __PF


const char* FAST_FUNC ll_proto_n2a(unsigned short id, char *buf, int len)
{
	unsigned i;
	id = ntohs(id);
	for (i = 0; i < ARRAY_SIZE(llproto_names); i++) {
		 if (llproto_names[i].id == id)
			return llproto_names[i].name;
	}
	snprintf(buf, len, "[%d]", id);
	return buf;
}

int FAST_FUNC ll_proto_a2n(unsigned short *id, char *buf)
{
	unsigned i;
	for (i = 0; i < ARRAY_SIZE(llproto_names); i++) {
		 if (strcasecmp(llproto_names[i].name, buf) == 0) {
			 i = llproto_names[i].id;
			 goto good;
		 }
	}
	i = bb_strtou(buf, NULL, 0);
	if (errno || i > 0xffff)
		return -1;
 good:
	*id = htons(i);
	return 0;
}

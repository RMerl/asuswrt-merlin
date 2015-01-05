/*
**  igmpproxy - IGMP proxy based multicast router 
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, version 2
**  
**  mrouted 3.9-beta3 - COPYRIGHT 1989 by The Board of Trustees of 
**  Leland Stanford Junior University.
**  - Original license can be found in the Stanford.txt file.
**
*/
/**
*   igmp.h - Recieves IGMP requests, and handle them 
*            appropriately...
*/

#include "igmpproxy.h"
#include "igmpv3.h"
 
// Globals                  
uint32_t     allhosts_group;          /* All hosts addr in net order */
uint32_t     allrouters_group;          /* All hosts addr in net order */
uint32_t     alligmp3_group;          /* IGMPv3 addr in net order */
              
extern int MRouterFD;

/*
 * Open and initialize the igmp socket, and fill in the non-changing
 * IP header fields in the output packet buffer.
 */
void initIgmp() {
    struct ip *ip;

    recv_buf = malloc(RECV_BUF_SIZE);
    send_buf = malloc(RECV_BUF_SIZE);

    k_hdr_include(true);    /* include IP header when sending */
    k_set_rcvbuf(256*1024,48*1024); /* lots of input buffering        */
    k_set_ttl(1);       /* restrict multicasts to one hop */
    k_set_loop(false);      /* disable multicast loopback     */

    ip         = (struct ip *)send_buf;
    memset(ip, 0, sizeof(struct ip));
    /*
     * Fields zeroed that aren't filled in later:
     * - IP ID (let the kernel fill it in)
     * - Offset (we don't send fragments)
     * - Checksum (let the kernel fill it in)
     */
    ip->ip_v   = IPVERSION;
    ip->ip_hl  = (sizeof(struct ip) + 4) >> 2; /* +4 for Router Alert option */
    ip->ip_tos = 0xc0;      /* Internet Control */
    ip->ip_ttl = MAXTTL;    /* applies to unicasts only */
    ip->ip_p   = IPPROTO_IGMP;

    allhosts_group   = htonl(INADDR_ALLHOSTS_GROUP);
    allrouters_group = htonl(INADDR_ALLRTRS_GROUP);
    alligmp3_group   = htonl(INADDR_ALLIGMPV3_GROUP);
}

/**
*   Finds the textual name of the supplied IGMP request.
*/
char *igmpPacketKind(u_int type, u_int code) {
    static char unknown[20];

    switch (type) {
    case IGMP_MEMBERSHIP_QUERY:     return  "Membership query  ";
    case IGMP_V1_MEMBERSHIP_REPORT:  return "V1 member report  ";
    case IGMP_V2_MEMBERSHIP_REPORT:  return "V2 member report  ";
    case IGMP_V3_MEMBERSHIP_REPORT:  return "V3 member report  ";
    case IGMP_V2_LEAVE_GROUP:        return "Leave message     ";
    
    default:
        sprintf(unknown, "unk: 0x%02x/0x%02x    ", type, code);
        return unknown;
    }
}


/**
 * Process a newly received IGMP packet that is sitting in the input
 * packet buffer.
 */
void acceptIgmp(int recvlen) {
    register uint32_t src, dst, group;
    struct ip *ip;
    struct igmp *igmp;
    struct igmpv3_report *igmpv3;
    struct igmpv3_grec *grec;
    int ipdatalen, iphdrlen, ngrec, nsrcs;

    if (recvlen < sizeof(struct ip)) {
        my_log(LOG_WARNING, 0,
            "received packet too short (%u bytes) for IP header", recvlen);
        return;
    }

    ip        = (struct ip *)recv_buf;
    src       = ip->ip_src.s_addr;
    dst       = ip->ip_dst.s_addr;

    /* filter local multicast 239.255.255.250 */
    if (dst == htonl(0xEFFFFFFA))
    {
        my_log(LOG_NOTICE, 0, "The IGMP message was local multicast. Ignoring.");
        return;
    }

    /* 
     * this is most likely a message from the kernel indicating that
     * a new src grp pair message has arrived and so, it would be 
     * necessary to install a route into the kernel for this.
     */
    if (ip->ip_p == 0) {
        if (src == 0 || dst == 0) {
            my_log(LOG_WARNING, 0, "kernel request not accurate");
        }
        else {
            struct IfDesc *checkVIF;
            
            // Check if the source address matches a valid address on upstream vif.
            checkVIF = getIfByIx( upStreamVif );
            if(checkVIF == 0) {
                my_log(LOG_ERR, 0, "Upstream VIF was null.");
                return;
            } 
            else if(src == checkVIF->InAdr.s_addr) {
                my_log(LOG_NOTICE, 0, "Route activation request from %s for %s is from myself. Ignoring.",
                    inetFmt(src, s1), inetFmt(dst, s2));
                return;
            }
            else if(!isAdressValidForIf(checkVIF, src)) {
                my_log(LOG_WARNING, 0, "The source address %s for group %s, is not in any valid net for upstream VIF.",
                    inetFmt(src, s1), inetFmt(dst, s2));
                return;
            }
            
            // Activate the route.
            my_log(LOG_DEBUG, 0, "Route activate request from %s to %s",
		    inetFmt(src,s1), inetFmt(dst,s2));
            activateRoute(dst, src);
            

        }
        return;
    }

    iphdrlen  = ip->ip_hl << 2;
    ipdatalen = ip_data_len(ip);

    if (iphdrlen + ipdatalen != recvlen) {
        my_log(LOG_WARNING, 0,
            "received packet from %s shorter (%u bytes) than hdr+data length (%u+%u)",
            inetFmt(src, s1), recvlen, iphdrlen, ipdatalen);
        return;
    }

    igmp = (struct igmp *)(recv_buf + iphdrlen);
    if ((ipdatalen < IGMP_MINLEN) ||
        (igmp->igmp_type == IGMP_V3_MEMBERSHIP_REPORT && ipdatalen <= IGMPV3_MINLEN)) {
        my_log(LOG_WARNING, 0,
            "received IP data field too short (%u bytes) for IGMP, from %s",
            ipdatalen, inetFmt(src, s1));
        return;
    }

    my_log(LOG_NOTICE, 0, "RECV %s from %-15s to %s",
        igmpPacketKind(igmp->igmp_type, igmp->igmp_code),
        inetFmt(src, s1), inetFmt(dst, s2) );

    switch (igmp->igmp_type) {
    case IGMP_V1_MEMBERSHIP_REPORT:
    case IGMP_V2_MEMBERSHIP_REPORT:
        group = igmp->igmp_group.s_addr;
        acceptGroupReport(src, group, igmp->igmp_type);
        return;
    
    case IGMP_V3_MEMBERSHIP_REPORT:
        igmpv3 = (struct igmpv3_report *)(recv_buf + iphdrlen);
        grec = &igmpv3->igmp_grec[0];
        ngrec = ntohs(igmpv3->igmp_ngrec);
        while (ngrec--) {
            if ((uint8_t *)igmpv3 + ipdatalen < (uint8_t *)grec + sizeof(*grec))
                break;
            group = grec->grec_mca.s_addr;
            nsrcs = ntohs(grec->grec_nsrcs);
            switch (grec->grec_type) {
            case IGMPV3_MODE_IS_INCLUDE:
            case IGMPV3_CHANGE_TO_INCLUDE:
                if (nsrcs == 0) {
                    acceptLeaveMessage(src, group);
                    break;
                } /* else fall through */
            case IGMPV3_MODE_IS_EXCLUDE:
            case IGMPV3_CHANGE_TO_EXCLUDE:
            case IGMPV3_ALLOW_NEW_SOURCES:
                acceptGroupReport(src, group, IGMP_V2_MEMBERSHIP_REPORT);
                break;
            case IGMPV3_BLOCK_OLD_SOURCES:
                break;
            default:
                my_log(LOG_INFO, 0,
                    "ignoring unknown IGMPv3 group record type %x from %s to %s for %s",
                    grec->grec_type, inetFmt(src, s1), inetFmt(dst, s2),
                    inetFmt(group, s3));
                break;
            }
            grec = (struct igmpv3_grec *)
                (&grec->grec_src[nsrcs] + grec->grec_auxwords * 4);
        }
        return;
    
    case IGMP_V2_LEAVE_GROUP:
        group = igmp->igmp_group.s_addr;
        acceptLeaveMessage(src, group);
        return;
    
    case IGMP_MEMBERSHIP_QUERY:
        return;

    default:
        my_log(LOG_INFO, 0,
            "ignoring unknown IGMP message type %x from %s to %s",
            igmp->igmp_type, inetFmt(src, s1),
            inetFmt(dst, s2));
        return;
    }
}


/*
 * Construct an IGMP message in the output packet buffer.  The caller may
 * have already placed data in that buffer, of length 'datalen'.
 */
void buildIgmp(uint32_t src, uint32_t dst, int type, int code, uint32_t group, int datalen) {
    struct ip *ip;
    struct igmp *igmp;
    extern int curttl;

    ip                      = (struct ip *)send_buf;
    ip->ip_src.s_addr       = src;
    ip->ip_dst.s_addr       = dst;
    ip_set_len(ip, IP_HEADER_RAOPT_LEN + IGMP_MINLEN + datalen);

    if (IN_MULTICAST(ntohl(dst))) {
        ip->ip_ttl = curttl;
    } else {
        ip->ip_ttl = MAXTTL;
    }

    /* Add Router Alert option */
    ((u_char*)send_buf+MIN_IP_HEADER_LEN)[0] = IPOPT_RA;
    ((u_char*)send_buf+MIN_IP_HEADER_LEN)[1] = 0x04;
    ((u_char*)send_buf+MIN_IP_HEADER_LEN)[2] = 0x00;
    ((u_char*)send_buf+MIN_IP_HEADER_LEN)[3] = 0x00;

    igmp                    = (struct igmp *)(send_buf + IP_HEADER_RAOPT_LEN);
    igmp->igmp_type         = type;
    igmp->igmp_code         = code;
    igmp->igmp_group.s_addr = group;
    igmp->igmp_cksum        = 0;
    igmp->igmp_cksum        = inetChksum((u_short *)igmp,
                                         IP_HEADER_RAOPT_LEN + datalen);

}

/* 
 * Call build_igmp() to build an IGMP message in the output packet buffer.
 * Then send the message from the interface with IP address 'src' to
 * destination 'dst'.
 */
void sendIgmp(uint32_t src, uint32_t dst, int type, int code, uint32_t group, int datalen) {
    struct sockaddr_in sdst;
    int setloop = 0, setigmpsource = 0;

    buildIgmp(src, dst, type, code, group, datalen);

    if (IN_MULTICAST(ntohl(dst))) {
        k_set_if(src);
        setigmpsource = 1;
        if (type != IGMP_DVMRP || dst == allhosts_group) {
            setloop = 1;
            k_set_loop(true);
        }
    }

    memset(&sdst, 0, sizeof(sdst));
    sdst.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sdst.sin_len = sizeof(sdst);
#endif
    sdst.sin_addr.s_addr = dst;
    if (sendto(MRouterFD, send_buf,
               IP_HEADER_RAOPT_LEN + IGMP_MINLEN + datalen, 0,
               (struct sockaddr *)&sdst, sizeof(sdst)) < 0) {
        if (errno == ENETDOWN)
            my_log(LOG_ERR, errno, "Sender VIF was down.");
        else
            my_log(LOG_INFO, errno,
                "sendto to %s on %s",
                inetFmt(dst, s1), inetFmt(src, s2));
    }

    if(setigmpsource) {
        if (setloop) {
            k_set_loop(false);
        }
        // Restore original...
        k_set_if(INADDR_ANY);
    }

    my_log(LOG_DEBUG, 0, "SENT %s from %-15s to %s",
	    igmpPacketKind(type, code), src == INADDR_ANY ? "INADDR_ANY" :
	    inetFmt(src, s1), inetFmt(dst, s2));
}

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

#include "igmpproxy.h"

/*
 * Exported variables.
 */
char s1[19];        /* buffers to hold the string representations  */
char s2[19];        /* of IP addresses, to be passed to inet_fmt() */
char s3[19];        /* or inet_fmts().                             */
char s4[19];
            
/*
** Formats 'InAdr' into a dotted decimal string. 
**
** returns: - pointer to 'St'
**          
*/
char *fmtInAdr( char *St, struct in_addr InAdr ) {
    sprintf( St, "%u.%u.%u.%u", 
             ((uint8_t *)&InAdr.s_addr)[ 0 ],
             ((uint8_t *)&InAdr.s_addr)[ 1 ],
             ((uint8_t *)&InAdr.s_addr)[ 2 ],
             ((uint8_t *)&InAdr.s_addr)[ 3 ] );

    return St;
}

/*
 * Convert an IP address in u_long (network) format into a printable string.
 */
char *inetFmt(uint32_t addr, char *s) {
    register u_char *a;

    a = (u_char *)&addr;
    sprintf(s, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return(s);
}


/*
 * Convert an IP subnet number in u_long (network) format into a printable
 * string including the netmask as a number of bits.
 */
char *inetFmts(uint32_t addr, uint32_t mask, char *s) {
    register u_char *a, *m;
    int bits;

    if ((addr == 0) && (mask == 0)) {
        sprintf(s, "default");
        return(s);
    }
    a = (u_char *)&addr;
    m = (u_char *)&mask;
    bits = 33 - ffs(ntohl(mask));

    if (m[3] != 0) sprintf(s, "%u.%u.%u.%u/%d", a[0], a[1], a[2], a[3],
                           bits);
    else if (m[2] != 0) sprintf(s, "%u.%u.%u/%d",    a[0], a[1], a[2], bits);
    else if (m[1] != 0) sprintf(s, "%u.%u/%d",       a[0], a[1], bits);
    else                sprintf(s, "%u/%d",          a[0], bits);

    return(s);
}

/*
 * inet_cksum extracted from:
 *			P I N G . C
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * (ping.c) Status -
 *	Public Domain.  Distribution Unlimited.
 *
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
uint16_t inetChksum(uint16_t *addr, int len) {
    register int nleft = len;
    register uint16_t *w = addr;
    uint16_t answer = 0;
    register int32_t sum = 0;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(uint8_t *) (&answer) = *(uint8_t *)w ;
        sum += answer;
    }

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}




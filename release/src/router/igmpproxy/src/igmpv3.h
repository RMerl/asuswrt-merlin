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
*/
/**
*   igmpv3.h - Header file for common IGMPv3 includes.
*/

struct igmpv3_grec {
    u_int8_t grec_type;
    u_int8_t grec_auxwords;
    u_int16_t grec_nsrcs;
    struct in_addr grec_mca;
    struct in_addr grec_src[0];
};

struct igmpv3_report {
    u_int8_t igmp_type;
    u_int8_t igmp_resv1;
    u_int16_t igmp_cksum;
    u_int16_t igmp_resv2;
    u_int16_t igmp_ngrec;
    struct igmpv3_grec igmp_grec[0];
};

#define IGMPV3_MODE_IS_INCLUDE   1
#define IGMPV3_MODE_IS_EXCLUDE   2
#define IGMPV3_CHANGE_TO_INCLUDE 3
#define IGMPV3_CHANGE_TO_EXCLUDE 4
#define IGMPV3_ALLOW_NEW_SOURCES 5
#define IGMPV3_BLOCK_OLD_SOURCES 6

#define IGMPV3_MINLEN 12

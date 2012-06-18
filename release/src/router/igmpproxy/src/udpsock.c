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
*   udpsock.c contains function for creating a UDP socket.
*
*/

#include "igmpproxy.h"

/**
*  Creates and connects a simple UDP socket to the target 
*  'PeerInAdr':'PeerPort'
*
*   @param PeerInAdr - The address to connect to
*   @param PeerPort  - The port to connect to
*           
*/
int openUdpSocket( uint32_t PeerInAdr, uint16_t PeerPort ) {
    int Sock;
    struct sockaddr_in SockAdr;
    
    if( (Sock = socket( AF_INET, SOCK_RAW, IPPROTO_IGMP )) < 0 )
        my_log( LOG_ERR, errno, "UDP socket open" );
    
    memset( &SockAdr, 0, sizeof( SockAdr ) );
    SockAdr.sin_family      = AF_INET;
    SockAdr.sin_port        = htons(PeerPort);
    SockAdr.sin_addr.s_addr = htonl(PeerInAdr);
    
    if( bind( Sock, (struct sockaddr *)&SockAdr, sizeof( SockAdr ) ) )
        my_log( LOG_ERR, errno, "UDP socket bind" );
    
    return Sock;
}


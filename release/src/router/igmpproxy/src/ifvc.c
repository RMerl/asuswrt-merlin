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

struct IfDesc IfDescVc[ MAX_IF ], *IfDescEp = IfDescVc;

/*
** Builds up a vector with the interface of the machine. Calls to the other functions of 
** the module will fail if they are called before the vector is build.
**          
*/
void buildIfVc() {
    struct ifreq IfVc[ sizeof( IfDescVc ) / sizeof( IfDescVc[ 0 ] )  ];
    struct ifreq *IfEp;

    int Sock;

    if ( (Sock = socket( AF_INET, SOCK_DGRAM, 0 )) < 0 )
        my_log( LOG_ERR, errno, "RAW socket open" );

    /* get If vector
     */
    {
        struct ifconf IoCtlReq;

        IoCtlReq.ifc_buf = (void *)IfVc;
        IoCtlReq.ifc_len = sizeof( IfVc );

        if ( ioctl( Sock, SIOCGIFCONF, &IoCtlReq ) < 0 )
            my_log( LOG_ERR, errno, "ioctl SIOCGIFCONF" );

        IfEp = (void *)((char *)IfVc + IoCtlReq.ifc_len);
    }

    /* loop over interfaces and copy interface info to IfDescVc
     */
    {
        struct ifreq  *IfPt, *IfNext;

        // Temp keepers of interface params...
        uint32_t addr, subnet, mask;

        for ( IfPt = IfVc; IfPt < IfEp; IfPt = IfNext ) {
            struct ifreq IfReq;
            char FmtBu[ 32 ];

	    IfNext = (struct ifreq *)((char *)&IfPt->ifr_addr +
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
				    IfPt->ifr_addr.sa_len
#else
				    sizeof(struct sockaddr_in)
#endif
		    );
	    if (IfNext < IfPt + 1)
		    IfNext = IfPt + 1;

            strncpy( IfDescEp->Name, IfPt->ifr_name, sizeof( IfDescEp->Name ) );

            // Currently don't set any allowed nets...
            //IfDescEp->allowednets = NULL;

            // Set the index to -1 by default.
            IfDescEp->index = -1;

            /* don't retrieve more info for non-IP interfaces
             */
            if ( IfPt->ifr_addr.sa_family != AF_INET ) {
                IfDescEp->InAdr.s_addr = 0;  /* mark as non-IP interface */
                IfDescEp++;
                continue;
            }

            // Get the interface adress...
            IfDescEp->InAdr = ((struct sockaddr_in *)&IfPt->ifr_addr)->sin_addr;
            addr = IfDescEp->InAdr.s_addr;

            memcpy( IfReq.ifr_name, IfDescEp->Name, sizeof( IfReq.ifr_name ) );
            IfReq.ifr_addr.sa_family = AF_INET;
            ((struct sockaddr_in *)&IfReq.ifr_addr)->sin_addr.s_addr = addr;

            // Get the subnet mask...
            if (ioctl(Sock, SIOCGIFNETMASK, &IfReq ) < 0)
                my_log(LOG_ERR, errno, "ioctl SIOCGIFNETMASK for %s", IfReq.ifr_name);
            mask = ((struct sockaddr_in *)&IfReq.ifr_addr)->sin_addr.s_addr;
            subnet = addr & mask;

            /* get if flags
            **
            ** typical flags:
            ** lo    0x0049 -> Running, Loopback, Up
            ** ethx  0x1043 -> Multicast, Running, Broadcast, Up
            ** ipppx 0x0091 -> NoArp, PointToPoint, Up 
            ** grex  0x00C1 -> NoArp, Running, Up
            ** ipipx 0x00C1 -> NoArp, Running, Up
            */
            if ( ioctl( Sock, SIOCGIFFLAGS, &IfReq ) < 0 )
                my_log( LOG_ERR, errno, "ioctl SIOCGIFFLAGS" );

            IfDescEp->Flags = IfReq.ifr_flags;

            // Insert the verified subnet as an allowed net...
            IfDescEp->allowednets = (struct SubnetList *)malloc(sizeof(struct SubnetList));
            if(IfDescEp->allowednets == NULL) my_log(LOG_ERR, 0, "Out of memory !");
            
            // Create the network address for the IF..
            IfDescEp->allowednets->next = NULL;
            IfDescEp->allowednets->subnet_mask = mask;
            IfDescEp->allowednets->subnet_addr = subnet;

            // Set the default params for the IF...
            IfDescEp->state         = IF_STATE_DISABLED;
            IfDescEp->robustness    = DEFAULT_ROBUSTNESS;
            IfDescEp->threshold     = DEFAULT_THRESHOLD;   /* ttl limit */
            IfDescEp->ratelimit     = DEFAULT_RATELIMIT; 
            

            // Debug log the result...
            my_log( LOG_DEBUG, 0, "buildIfVc: Interface %s Addr: %s, Flags: 0x%04x, Network: %s",
                 IfDescEp->Name,
                 fmtInAdr( FmtBu, IfDescEp->InAdr ),
                 IfDescEp->Flags,
                 inetFmts(subnet,mask, s1));

            IfDescEp++;
        } 
    }

    close( Sock );
}

/*
** Returns a pointer to the IfDesc of the interface 'IfName'
**
** returns: - pointer to the IfDesc of the requested interface
**          - NULL if no interface 'IfName' exists
**          
*/
struct IfDesc *getIfByName( const char *IfName ) {
    struct IfDesc *Dp;

    for ( Dp = IfDescVc; Dp < IfDescEp; Dp++ )
        if ( ! strcmp( IfName, Dp->Name ) )
            return Dp;

    return NULL;
}

/*
** Returns a pointer to the IfDesc of the interface 'Ix'
**
** returns: - pointer to the IfDesc of the requested interface
**          - NULL if no interface 'Ix' exists
**          
*/
struct IfDesc *getIfByIx( unsigned Ix ) {
    struct IfDesc *Dp = &IfDescVc[ Ix ];
    return Dp < IfDescEp ? Dp : NULL;
}

/**
*   Returns a pointer to the IfDesc whose subnet matches
*   the supplied IP adress. The IP must match a interfaces
*   subnet, or any configured allowed subnet on a interface.
*/
struct IfDesc *getIfByAddress( uint32_t ipaddr ) {

    struct IfDesc       *Dp;
    struct SubnetList   *currsubnet;
    struct IfDesc       *res = NULL;
    uint32_t            last_subnet_mask = 0;

    for ( Dp = IfDescVc; Dp < IfDescEp; Dp++ ) {
        // Loop through all registered allowed nets of the VIF...
        for(currsubnet = Dp->allowednets; currsubnet != NULL; currsubnet = currsubnet->next) {
            // Check if the ip falls in under the subnet....
            if(currsubnet->subnet_mask > last_subnet_mask && (ipaddr & currsubnet->subnet_mask) == currsubnet->subnet_addr) {
                res = Dp;
                last_subnet_mask = currsubnet->subnet_mask;
            }
        }
    }
    return res;
}


/**
*   Returns a pointer to the IfDesc whose subnet matches
*   the supplied IP adress. The IP must match a interfaces
*   subnet, or any configured allowed subnet on a interface.
*/
struct IfDesc *getIfByVifIndex( unsigned vifindex ) {
    struct IfDesc       *Dp;
    if(vifindex>0) {
        for ( Dp = IfDescVc; Dp < IfDescEp; Dp++ ) {
            if(Dp->index == vifindex) {
                return Dp;
            }
        }
    }
    return NULL;
}


/**
*   Function that checks if a given ipaddress is a valid
*   address for the supplied VIF.
*/
int isAdressValidForIf( struct IfDesc* intrface, uint32_t ipaddr ) {
    struct SubnetList   *currsubnet;
    
    if(intrface == NULL) {
        return 0;
    }
    // Loop through all registered allowed nets of the VIF...
    for(currsubnet = intrface->allowednets; currsubnet != NULL; currsubnet = currsubnet->next) {
        // Check if the ip falls in under the subnet....
        if((ipaddr & currsubnet->subnet_mask) == currsubnet->subnet_addr) {
            return 1;
        }
    }
    return 0;
}



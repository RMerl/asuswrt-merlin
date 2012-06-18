/* $Id: minissdp.h,v 1.7 2008/10/06 13:20:56 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2007 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#ifndef __MINISSDP_H__
#define __MINISSDP_H__

/*#include "miniupnpdtypes.h"*/

int
OpenAndConfSSDPReceiveSocket();
/* OpenAndConfSSDPReceiveSocket(int n_lan_addr, struct lan_addr_s * lan_addr);*/

/*int
OpenAndConfSSDPNotifySocket(const char * addr);*/

int
OpenAndConfSSDPNotifySockets(int * sockets);
/*OpenAndConfSSDPNotifySockets(int * sockets,
                             struct lan_addr_s * lan_addr, int n_lan_addr);*/

/*void
SendSSDPNotifies(int s, const char * host, unsigned short port,
                 unsigned int lifetime);*/
void
SendSSDPNotifies2(int * sockets,
                  unsigned short port,
                  unsigned int lifetime);
/*SendSSDPNotifies2(int * sockets, struct lan_addr_s * lan_addr, int n_lan_addr,
                  unsigned short port,
                  unsigned int lifetime);*/

void
ProcessSSDPRequest(int s, unsigned short port);
/*ProcessSSDPRequest(int s, struct lan_addr_s * lan_addr, int n_lan_addr,
                   unsigned short port);*/

int
SendSSDPGoodbye(int * sockets, int n);

int
SubmitServicesToMiniSSDPD(const char * host, unsigned short port);

#endif


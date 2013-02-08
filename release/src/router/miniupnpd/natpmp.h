/* $Id: natpmp.h,v 1.9 2012/09/27 15:47:15 nanard Exp $ */
/* MiniUPnP project
 * author : Thomas Bernard
 * website : http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 */
#ifndef NATPMP_H_INCLUDED
#define NATPMP_H_INCLUDED

/* The NAT-PMP specification which can be found at the url :
 * http://files.dns-sd.org/draft-cheshire-nat-pmp.txt
 * draft version 3 of April 2008
 * define 5351 as listening port for the gateway,
 * and the 224.0.0.1 port 5350 as the local link
 * multicast address for address change announces.
 * Previous versions of the specification defined 5351
 * as the port for address change announces. */
#define NATPMP_PORT (5351)
#define NATPMP_NOTIF_PORT	(5350)
#define NATPMP_NOTIF_ADDR	("224.0.0.1")

int OpenAndConfNATPMPSockets(int * sockets);

void ProcessIncomingNATPMPPacket(int s);

#if 0
int ScanNATPMPforExpiration(void);

int CleanExpiredNATPMP(void);
#endif

void SendNATPMPPublicAddressChangeNotification(int * sockets, int n_sockets);

#endif


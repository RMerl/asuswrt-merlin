/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/* 
 * leases.c -- tools to manage DHCP leases 
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"
#include "arpping.h"

unsigned char blank_chaddr[] = {[0 ... 15] = 0};
struct dhcpOfferedAddr *static_lease;


/* clear every lease out that chaddr OR yiaddr matches and is nonzero */
void clear_lease(u_int8_t *chaddr, u_int32_t yiaddr)
{
	unsigned int i, j;
	
	//added by Joey to handle static lease
	static_lease = NULL;
	
	for (j = 0; j < 16 && !chaddr[j]; j++);
	
	for (i = 0; i < server_config.max_leases; i++)
	{
		// added by Joey to handle static lease
		//if (memcmp(leases[i].chaddr, chaddr, 16)==0 && leases[i].expires==0xffffffff)
		//	static_lease = &leases[i];

		if ((j != 16 && !memcmp(leases[i].chaddr, chaddr, 16)) ||
		    (yiaddr && leases[i].yiaddr == yiaddr)) 
		{	
			if (leases[i].expires==0xffffffff) static_lease=&leases[i];
			else memset(&(leases[i]), 0, sizeof(struct dhcpOfferedAddr));
		}
	}
}


/* add a lease into the table, clearing out any old ones */
struct dhcpOfferedAddr *add_lease(u_int8_t *chaddr, u_int32_t yiaddr, unsigned long lease)
{
	struct dhcpOfferedAddr *oldest;
	
	/* clean out any old ones */	
	clear_lease(chaddr, yiaddr);

	// added by Joey to handle static lease
	if (static_lease) return(static_lease);
		
	oldest = oldest_expired_lease();
	
	if (oldest) {
		memcpy(oldest->chaddr, chaddr, 16);
		oldest->yiaddr = yiaddr;
		if (lease==0xffffffff)
			oldest->expires = 0xffffffff;
		else
			oldest->expires = uptime() + lease;
	}
	
	return oldest;
}


/* true if a lease has expired */
int lease_expired(struct dhcpOfferedAddr *lease)
{
	return (lease->expires < (unsigned long) uptime());
}	


/* Find the oldest expired lease, NULL if there are no expired leases */
struct dhcpOfferedAddr *oldest_expired_lease(void)
{
	struct dhcpOfferedAddr *oldest = NULL;
	unsigned long oldest_lease = uptime();
	unsigned int i;

	
	for (i = 0; i < server_config.max_leases; i++)
		if (oldest_lease > leases[i].expires) {
			oldest_lease = leases[i].expires;
			oldest = &(leases[i]);
		}
	return oldest;
		
}


/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *find_lease_by_chaddr(u_int8_t *chaddr)
{
	unsigned int i;

	for (i = 0; i < server_config.max_leases; i++)
		//JYWeng: 20030701 for palm: chaddr error after first 6 bytes
		//if (!memcmp(leases[i].chaddr, chaddr, 16)) return &(leases[i]);
		if (!memcmp(leases[i].chaddr, chaddr, 6)) return &(leases[i]);
	
	return NULL;
}


/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr *find_lease_by_yiaddr(u_int32_t yiaddr)
{
	unsigned int i;

	for (i = 0; i < server_config.max_leases; i++)
		if (leases[i].yiaddr == yiaddr) return &(leases[i]);
	
	return NULL;
}


/* find an assignable address, it check_expired is true, we check all the expired leases as well.
 * Maybe this should try expired leases by age... */
u_int32_t find_address(u_int8_t *chaddr, int check_expired)
{
	u_int32_t start, addr, ret;
	struct dhcpOfferedAddr *lease = NULL;		
	int i;
	unsigned int j;

	/* hash hwaddr: use the SDBM hashing algorithm.  Seems to give good
	   dispersal even with similarly-valued "strings". */ 
//TODO: dhcp_packet.hlen should be used instead of hardcoded mac length
#define hlen 6
	for (j = 0, i = 0; i < hlen; i++)
		j += chaddr[i] + (j << 6) + (j << 16) - j;

	/* pick a seed based on hwaddr then iterate until we find a free address. */
	start = addr =  ntohl(server_config.start) + 
		(j % (1 + ntohl(server_config.end) - ntohl(server_config.start)));
	do {
		/* ie, 192.168.55.0 */
		if (!(addr & 0xFF)) goto next_addr;
 
		/* ie, 192.168.55.255 */
		if ((addr & 0xFF) == 0xFF) goto next_addr;

		/* check if an IP is used by LAN IP */
		if (addr == ntohl(server_config.server)) goto next_addr;

		/* lease is not taken */
		ret = htonl(addr);
		if ((!(lease = find_lease_by_yiaddr(ret)) ||

		     /* or it expired and we are checking for expired leases */
		     (check_expired  && lease_expired(lease))) &&

		     /* and it isn't on the network */
	    	     !check_ip(ret)) {
			return ret;
			break;
		}

	 next_addr:
		addr++;
		if (addr > ntohl(server_config.end))
			addr = ntohl(server_config.start);
	} while (addr != start);
	return 0;
}


/* check if an IP is taken, if it is, add it to the lease table */
int check_ip(u_int32_t addr)
{
	struct in_addr temp;
	
	if (arpping(addr, server_config.server, server_config.arp, server_config.interface) == 0) {
		temp.s_addr = addr;
	 	LOG(LOG_INFO, "%s belongs to someone, reserving it for %ld seconds", 
	 		inet_ntoa(temp), server_config.conflict_time);
		add_lease(blank_chaddr, addr, server_config.conflict_time);
		return 1;
	} else return 0;
}

/*
 * Miscellaneous services
 *
 * Copyright 2003, Broadcom Corporation
 * All Rights Reserved.		
 *				     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Broadcom Corporation.			    
 *
 * $Id: service.c,v 1.1.1.1 2008/07/21 09:17:45 james26_jang Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <bcmnvram.h>
#include <shutils.h>

#define WCLIENT 1

#define IFUP (IFF_UP|IFF_RUNNING|IFF_BROADCAST|IFF_MULTICAST)

int is_dhcpd_exist(void)
{
	time_t now;
	FILE *fp;
	unsigned int expire;

	time(&now);
	
	if (!(fp=fopen("/tmp/udhcpc.expires","r"))) return 0;
	fscanf(fp, "%d", &expire);
	fclose(fp);

	printf("check dhcp : %x %x\n", expire, (unsigned int)now);

	if (expire<=(unsigned int)now) return 0;
	else return 1;				
}

int
start_dhcpd(void)
{
	FILE *fp;
//	char name[100];
	char *lan_ifname=nvram_safe_get("lan_ifname");

	dprintf("%s %s %s %s\n",
		nvram_safe_get("lan_ifname"),
		nvram_safe_get("dhcp_start"),
		nvram_safe_get("dhcp_end"),
		nvram_safe_get("lan_lease"));

	if (nvram_invmatch("dhcp_enable", "1")) return -1;
	
	ifconfig(lan_ifname, IFUP, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));

	/* Touch leases file */
	if (!(fp = fopen("/tmp/udhcpd.leases", "a"))) {
		perror("/tmp/udhcpd.leases");
		return errno;
	}
	fclose(fp);

	/* Write configuration file based on current information */
	if (!(fp = fopen("/tmp/udhcpd.conf", "w"))) {
		perror("/tmp/udhcpd.conf");
		return errno;
	}
	fprintf(fp, "pidfile /var/run/udhcpd.pid\n");
	fprintf(fp, "start %s\n", nvram_safe_get("dhcp_start"));
	fprintf(fp, "end %s\n", nvram_safe_get("dhcp_end"));
	fprintf(fp, "interface %s\n", nvram_safe_get("lan_ifname"));
	fprintf(fp, "remaining yes\n");
	fprintf(fp, "lease_file /tmp/udhcpd.leases\n");
	fprintf(fp, "option subnet %s\n", nvram_safe_get("lan_netmask"));
	fprintf(fp, "option router %s\n", nvram_safe_get("lan_ipaddr"));
	//fprintf(fp, "option dns %s\n", nvram_safe_get("lan_ipaddr"));
	fprintf(fp, "option lease %s\n", nvram_safe_get("lan_lease"));
	//snprintf(name, sizeof(name), "%s_wins", nvram_safe_get("dhcp_wins"));
	//if (nvram_invmatch(name, ""))
	//	fprintf(fp, "option wins %s\n", nvram_get(name));
	//snprintf(name, sizeof(name), "%s_domain", nvram_safe_get("dhcp_domain"));
	if (nvram_invmatch("lan_domain", ""))
		fprintf(fp, "option domain %s\n", nvram_safe_get("lan_domain"));
	fclose(fp);

	system("udhcpd /tmp/udhcpd.conf");

	dprintf("done\n");
	return 0;
}

int
stop_dhcpd(void)
{
	int ret;
/*
* Process udhcpd handles two signals - SIGTERM and SIGUSR1
*
*  - SIGUSR1 saves all leases in /tmp/udhcpd.leases
*  - SIGTERM causes the process to be killed
*
* The SIGUSR1+SIGTERM behavior is what we like so that all current client
* leases will be honorred when the dhcpd restarts and all clients can extend
* their leases and continue their current IP addresses. Otherwise clients
* would get NAK'd when they try to extend/rebind their leases and they 
* would have to release current IP and to request a new one which causes 
* a no-IP gap in between.
*/
	doSystem("killall -%d udhcpd", SIGUSR1);
	ret = system("killall udhcpd");

	dprintf("done\n");
	return ret;
}


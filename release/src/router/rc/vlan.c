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
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <rc.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>															
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <wlutils.h>

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

/* The field index of subnet_rulelist */
enum {
	SUBNET_NAME = 0,
	//SUBNET_IFNAME,
	SUBNET_IP,
	SUBNET_MASK,
	SUBNET_DHCP_ENABLE,
	SUBNET_DHCP_START,
	SUBNET_DHCP_END,
	SUBNET_DHCP_LEASE
};

#define VLAN_START_IFNAMES_INDEX	1

char *get_info_by_subnet(char *subnet_name, int field, char *result)
{
	char *nv, *nvp, *b;
	//char *name, *ifname, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	char *name, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	int found = 0;
	nv = nvp = strdup(nvram_safe_get("subnet_rulelist"));

	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			//if ((vstrsep(b, ">", &name, &ifname, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 8))
			if ((vstrsep(b, ">", &name, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 7))
				continue;
	
			if (!strcmp(name, subnet_name)) {
				//if (field == SUBNET_IFNAME) sprintf(result, "%s", ifname);
				if (field == SUBNET_IP) sprintf(result, "%s", ip);
				else if (field == SUBNET_MASK) sprintf(result, "%s", netmask);
				else if (field == SUBNET_DHCP_ENABLE) sprintf(result, "%s", dhcp_enable);
				else if (field == SUBNET_DHCP_START) sprintf(result, "%s", dhcp_start);
				else if (field == SUBNET_DHCP_END) sprintf(result, "%s", dhcp_end);
				else if (field == SUBNET_DHCP_LEASE) sprintf(result, "%s", lease);
				
				found = 1;
				break;
			}
		}
		free(nv);
	}

	if (found) return result;

	if (field == SUBNET_IP || field == SUBNET_MASK) {
		sprintf(result, "0.0.0.0");
		return result;
	}

	return NULL;
}

int vlan_enable(void)
{
	char *nv, *nvp, *b;
	char *enable, *desc, *portset, *wlmap, *subnet_name, *intranet;
	int vlan_enable = 0;

	nv = nvp = strdup(nvram_safe_get("vlan_rulelist"));

	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &enable, &desc, &portset, &wlmap, &subnet_name, &intranet) != 6))
				continue;

			if (atoi(enable)) {
				vlan_enable = 1;
				break;
			}
		}

		free(nv);
	}

	return vlan_enable;
}

#if 0
void clean_vlan_ifnames(void)
{
	int i = 0;
	//int vlan_index = nvram_get_int("vlan_index");
	char nv[32];

	/* Check the ifname of lan_ifnames whether existing in other lanX_ifnames for vlan */
	for (i = 1; i <= VLAN_MAXVID; i++) {
		/* Unset lanX_ifname */
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_ifname", i);
		nvram_unset(nv);

		/* Unset lanX_ifnames */
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_ifnames", i);
		nvram_unset(nv);

		/* Unset lanX_subnet */
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_subnet", i);
		nvram_unset(nv);
	}

	nvram_unset("vlan_index");
	nvram_unset("lan_ifnames_t");
}
#endif

void set_vlan_ifnames(int index, int wlmap, char *subnet_name, char *vlan_if)
{
	int unit = 0, subunit = 0, subunit_x = 0;
	int max_mssid = num_of_mssid_support(unit);
	char brif[8] = "brXXX", tmp[100], lan_prefix[] = "lanXXXXXXXXX_";
	char word[256], *next;
	char wl_ifnames[32], nv[32];
	char wl_if_map = 0;
	int wl_radio = 0, wl_bss_enabled = 0;
	char *p;
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	char nic_lan_ifname[32];
#endif

	memset(wl_ifnames, 0x0, sizeof(wl_ifnames));
	p = wl_ifnames;

	snprintf(brif, sizeof(brif), "br%d", index);
	snprintf(lan_prefix, sizeof(lan_prefix), "lan%d_", index);
	nvram_set(strcat_r(lan_prefix, "ifname", tmp), brif);
	nvram_set(strcat_r(lan_prefix, "subnet", tmp), subnet_name);
	if (vlan_if != NULL) {
		nvram_set(strcat_r(lan_prefix, "ifnames", tmp), vlan_if);
		//sprintf(wl_ifnames, "%s", nvram_safe_get(strcat_r(lan_prefix, "ifnames", tmp)));
		p += sprintf(p, "%s ", nvram_safe_get(strcat_r(lan_prefix, "ifnames", tmp)));
	}

	foreach (word, nvram_safe_get("wl_ifnames"), next) {		
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "wl%d_radio", unit);
		wl_radio = nvram_get_int(nv);

		if (wl_radio) {
			wl_if_map = (wlmap >> (unit * 4)) & 0x1;
		
			/* Primary wl */
			if (wl_if_map) {
				p += sprintf(p, "%s ", word);
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
				if (strncmp(word, "rai", 3) == 0) {
					p += sprintf(p, "vlan%d ", PRIMARY_WL_VLAN_ID);
				}
#endif
			}

			/* Virtual wl */
			for (subunit = 1; subunit < max_mssid + 1; subunit++)
			{
				int wl_vif_map = (wlmap >> (unit * 4 + subunit)) & 0x1;
				subunit_x++;
				if (wl_vif_map) {
					memset(nv, 0x0, sizeof(nv));
					snprintf(nv, sizeof(nv), "wl%d.%d_bss_enabled", unit, subunit);
					wl_bss_enabled = nvram_get_int(nv);
	 
					if (wl_bss_enabled) {
						p += sprintf(p, "%s ", get_wlifname(unit, subunit, subunit_x, tmp));
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
						memset(nic_lan_ifname, 0x0, sizeof(nic_lan_ifname));
						sprintf(nic_lan_ifname, "%s", get_wlifname(unit, subunit, subunit_x, tmp));
						if (strncmp(nic_lan_ifname, "rai", 3) == 0) {
							p += sprintf(p, "vlan%d ", PRIMARY_WL_VLAN_ID + atoi(nic_lan_ifname + 3));
						}
#endif
					}
				}
			}
		}

		unit++;
		subunit_x = 0;
	}
	
	if (strlen(wl_ifnames)) {
		nvram_set(strcat_r(lan_prefix, "ifnames", tmp), wl_ifnames);
		nvram_set_int("vlan_index", index);
	}
}

int check_if_exist_vlan_ifnames(char *ifname)
{
	int i = 0;
	int vlan_index = nvram_get_int("vlan_index");
	char nv[32];

#if defined(RTN56U)	/* Pass to handle lan ifname for RT-N56U */
	if (!strcmp(ifname, "eth2"))
		return 0;
#endif

	/* Check the ifname of lan_ifnames whether existing in other lanX_ifnames for vlan */
	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_ifnames", i);

		if (strstr(nvram_safe_get(nv), ifname))
			return 1;
	}

	return 0;
}

/* Only for brcm solution */
void change_lan_ifnames(void)
{
	char word[256], *next;
	char lan_ifnames[32];
	char *p;

	if (!vlan_enable())
		return;

	nvram_set("lan_ifnames_t", nvram_get("lan_ifnames"));
	memset(lan_ifnames, 0x0, sizeof(lan_ifnames));
	p = lan_ifnames;

	foreach (word, nvram_safe_get("lan_ifnames"), next) {		
		if (strncmp(word, "vlan", 4)) {
			if (!check_if_exist_vlan_ifnames(word))
				p += sprintf(p, "%s ", word);
		}
		else
			p += sprintf(p, "%s ", word);
	}
	
	if (strlen(lan_ifnames))
		nvram_set("lan_ifnames", lan_ifnames);
}

/* Only for brcm solution */
#if 0
void revert_lan_ifnames(void)
{
	if (!vlan_enable())
		return;

	if (nvram_safe_get("lan_ifnames_t") != NULL)
		nvram_set("lan_ifnames", nvram_get("lan_ifnames_t"));
}
#endif

void start_vlan_ifnames(void)
{
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	if (!vlan_enable())
		return;

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

#if defined(RTN56U)
	char word[32], *next;
	char ifname[32];

	foreach (word, nvram_safe_get("vlan_ifnames"), next) {
		eval("vconfig", "add", "eth2", word);
		memset(ifname, 0x0, sizeof(ifname));
		sprintf(ifname, "eth2.%s", word);
		ifconfig(ifname, IFUP, NULL, NULL);
	}
#endif

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		char *lan_ifname;
		struct ifreq ifr;
		char *lan_ifnames, *ifname, *lan_subnet, *p;
		int hwaddrset;
		char eabuf[32];
		int sfd;
		char nv[32];
		char ip[32], mask[32];

		if ((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) return;

		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_ifname", i);

		lan_ifname = strdup(nvram_safe_get(nv));
		if (strncmp(lan_ifname, "br", 2) == 0) {
			_dprintf("%s: setting up the bridge %s\n", __FUNCTION__, lan_ifname);

			eval("brctl", "addbr", lan_ifname);
			eval("brctl", "setfd", lan_ifname, "0");
			if (is_routing_enabled())
				eval("brctl", "stp", lan_ifname, nvram_safe_get("lan_stp"));
			else
				eval("brctl", "stp", lan_ifname, "0");
#ifdef RTCONFIG_IPV6
			if ((get_ipv6_service() != IPV6_DISABLED) &&
				(!((nvram_get_int(ipv6_nvname("ipv6_accept_ra")) & 2) != 0 && !nvram_get_int(ipv6_nvname("ipv6_radvd")))))
			{
				ipv6_sysconf(lan_ifname, "accept_ra", 0);
				ipv6_sysconf(lan_ifname, "forwarding", 0);
			}
			set_intf_ipv6_dad(lan_ifname, 1, 1);
#endif
#ifdef RTCONFIG_EMF
			if (nvram_get_int("emf_enable")) {
				eval("emf", "add", "bridge", lan_ifname);
				eval("igs", "add", "bridge", lan_ifname);
			}
#endif

//			inet_aton(nvram_safe_get("lan_ipaddr"), (struct in_addr *)&ip);

			hwaddrset = 0;
			memset(nv, 0x0, sizeof(nv));
			snprintf(nv, sizeof(nv), "lan%d_ifnames", i);
			if ((lan_ifnames = strdup(nvram_safe_get(nv))) != NULL) {
				p = lan_ifnames;

				while ((ifname = strsep(&p, " ")) != NULL) {
					while (*ifname == ' ') ++ifname;
					if (*ifname == 0) break;

					// bring up interface
					if (strncmp(ifname, "vlan", 4) == 0) {
						if (ifconfig(ifname, IFUP, NULL, NULL) != 0)
							continue;
					}
#ifdef RTCONFIG_DSL	/* for DSL-N55U & DSL-N55U-B */
					if (strncmp(ifname, "eth2", 4) == 0) {
						if (ifconfig(ifname, IFUP, NULL, NULL) != 0)
							continue;
					}
#endif

					// set the logical bridge address to that of the first interface
					strlcpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
					if ((!hwaddrset) ||
					    (ioctl(sfd, SIOCGIFHWADDR, &ifr) == 0 &&
					    memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN) == 0)) {
						strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
						if (ioctl(sfd, SIOCGIFHWADDR, &ifr) == 0) {
							strlcpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
							ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
							_dprintf("%s: setting MAC of %s bridge to %s\n", __FUNCTION__,
								ifr.ifr_name, ether_etoa(ifr.ifr_hwaddr.sa_data, eabuf));
							ioctl(sfd, SIOCSIFHWADDR, &ifr);
							hwaddrset = 1;
						}
					}

					eval("brctl", "addif", lan_ifname, ifname);
#ifdef RTCONFIG_EMF
					if (nvram_get_int("emf_enable"))
						eval("emf", "add", "iface", lan_ifname, ifname);
#endif
				}

				free(lan_ifnames);
			}
		}
		// --- this shouldn't happen ---
		else if (*lan_ifname) {
			ifconfig(lan_ifname, IFUP, NULL, NULL);
		}
		else {
			close(sfd);
			free(lan_ifname);
			continue;
		}

		close(sfd);

		// bring up and configure LAN interface
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_subnet", i);
		lan_subnet = nvram_safe_get(nv);
		ifconfig(lan_ifname, IFUP, get_info_by_subnet(lan_subnet, SUBNET_IP, ip), get_info_by_subnet(lan_subnet, SUBNET_MASK, mask));
		
		free(lan_ifname);
	}

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

void stop_vlan_ifnames(void)
{
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	if (!vlan_enable())
		return;

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		char *lan_ifname;
		char *lan_ifnames, *ifname, *p;
		char nv[32];

		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_ifname", i);

		lan_ifname = strdup(nvram_safe_get(nv));

		if(is_routing_enabled())
			del_routes("lan_", "route", lan_ifname);	//del_lan_routes(lan_ifname);

		ifconfig(lan_ifname, 0, NULL, NULL);

		if (strncmp(lan_ifname, "br", 2) == 0) {

#ifdef RTCONFIG_EMF
			//stop_emf(lan_ifname);
			eval("emf", "stop", lan_ifname);
			eval("igs", "del", "bridge", lan_ifname);
			eval("emf", "del", "bridge", lan_ifname);
#endif

			memset(nv, 0x0, sizeof(nv));
			sprintf(nv, "lan%d_ifnames", i);
			if ((lan_ifnames = strdup(nvram_safe_get(nv))) != NULL) {
				p = lan_ifnames;

				while ((ifname = strsep(&p, " ")) != NULL) {
					while (*ifname == ' ') ++ifname;
					if (*ifname == 0) break;

#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_QTN
					if (strcmp(ifname, "wifi0"))
#endif
					{
						eval("wlconf", ifname, "down");
						eval("wl", "-i", ifname, "radio", "off");
					}
#elif defined RTCONFIG_RALINK
					if (!strncmp(ifname, "ra", 2))
						stop_wds_ra(lan_ifname, ifname);
#endif
					eval("brctl", "delif", lan_ifname, ifname);
					ifconfig(ifname, 0, NULL, NULL);
				}

				free(lan_ifnames);
			}
			eval("brctl", "delbr", lan_ifname);
		}
		else if (*lan_ifname) {
#ifdef CONFIG_BCMWL5
			eval("wlconf", lan_ifname, "down");
			eval("wl", "-i", lan_ifname, "radio", "off");
#endif
		}

		free(lan_ifname);
	}

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

void start_vlan_wl_ifnames(void)
{
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	if (!vlan_enable())
		return;

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		char *lan_ifname;
		char *wl_ifnames, *ifname, *p;
		char nv[32];

		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_ifname", i);

		lan_ifname = strdup(nvram_safe_get(nv));
		if (strncmp(lan_ifname, "br", 2) == 0) {
			memset(nv, 0x0, sizeof(nv));
			sprintf(nv, "lan%d_ifnames", i);
			if ((wl_ifnames = strdup(nvram_safe_get(nv))) != NULL) {
				p = wl_ifnames;
				while ((ifname = strsep(&p, " ")) != NULL) {
					while (*ifname == ' ') ++ifname;
					if (*ifname == 0) break;

					// bring up interface
					if (strncmp(ifname, "vlan", 4) == 0) {
						if (ifconfig(ifname, IFUP, NULL, NULL) != 0) {
#ifdef RTCONFIG_QTN
							if (strcmp(ifname, "wifi0"))
#endif
								continue;
						}
					}
#ifdef RTCONFIG_DSL	/* for DSL-N55U & DSL-N55U-B */
					if (strncmp(ifname, "eth2", 4) == 0) {
						if (ifconfig(ifname, IFUP, NULL, NULL) != 0)
							continue;
					}
#endif

					eval("brctl", "addif", lan_ifname, ifname);
#ifdef RTCONFIG_EMF
					if (nvram_get_int("emf_enable"))
						eval("emf", "add", "iface", lan_ifname, ifname);
#endif
				}

				free(wl_ifnames);
			}
		}

		free(lan_ifname);
	}

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

void stop_vlan_wl_ifnames(void)
{
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	if (!vlan_enable())
		return;

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		char *lan_ifname;
		char *wl_ifnames, *ifname, *p;
		char nv[32];
#ifdef CONFIG_BCMWL5
		int unit, subunit;
#endif

		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_ifname", i);

		lan_ifname = strdup(nvram_safe_get(nv));
		if (strncmp(lan_ifname, "br", 2) == 0) {
			memset(nv, 0x0, sizeof(nv));
			sprintf(nv, "lan%d_ifnames", i);
			if ((wl_ifnames = strdup(nvram_safe_get(nv))) != NULL) {
				p = wl_ifnames;
				while ((ifname = strsep(&p, " ")) != NULL) {
					while (*ifname == ' ') ++ifname;
					if (*ifname == 0) break;
#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_QTN
					if (!strcmp(ifname, "wifi0")) continue;
#endif
					if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
						if (get_ifname_unit(ifname, &unit, &subunit) < 0)
							continue;
					}
					else if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
						continue;

					eval("wlconf", ifname, "down");
					eval("wl", "-i", ifname, "radio", "off");
#elif defined RTCONFIG_RALINK
					if (!strncmp(ifname, "ra", 2))
						stop_wds_ra(lan_ifname, ifname);
#endif
#ifdef RTCONFIG_EMF
					eval("emf", "del", "iface", lan_ifname, ifname);
#endif
					eval("brctl", "delif", lan_ifname, ifname);
					ifconfig(ifname, 0, NULL, NULL);

#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
					{ // remove interface for iNIC packets
						char *nic_if, *nic_ifs, *nic_lan_ifnames;
						if((nic_lan_ifnames = strdup(nvram_safe_get("nic_lan_ifnames"))))
						{
							nic_ifs = nic_lan_ifnames;
							while ((nic_if = strsep(&nic_ifs, " ")) != NULL) {
								while (*nic_if == ' ')
									nic_if++;
								if (*nic_if == 0)
									break;
								if(strcmp(ifname, nic_if) == 0)
								{
									eval("vconfig", "rem", ifname);
									break;
								}
							}
							free(nic_lan_ifnames);
						}
					}
#endif
				}

				free(wl_ifnames);
			}
		}

		free(lan_ifname);
	}

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

int check_used_subnet(char *subnet_name, char *brif)
{
	int result = 0;
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		char *lan_subnet;
		char nv[32];

		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_subnet", i);
		lan_subnet = nvram_safe_get(nv);

		if (lan_subnet != NULL && strcmp(lan_subnet, "none") && strcmp(subnet_name, lan_subnet) == 0) {
			memset(nv, 0x0, sizeof(nv));
			sprintf(nv, "lan%d_ifname", i);
			sprintf(brif, "%s", nvram_safe_get(nv));
			result = 1;
			break;
		}
	}

	return result;
}

int check_intranet_only(char *name)
{
	char *nv, *nvp, *b;
	char *enable, *desc, *portset, *wlmap, *subnet_name, *intranet;
	int intranet_only = 0;

	nv = nvp = strdup(nvram_safe_get("vlan_rulelist"));

	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &enable, &desc, &portset, &wlmap, &subnet_name, &intranet) != 6))
				continue;

			if (!strcmp(name, subnet_name)) {
				if (strlen(intranet))
					intranet_only = atoi(intranet);
				break;
			}
		}

		free(nv);
	}

	return intranet_only;
}

void vlan_subnet_dnsmasq_conf(FILE *fp)
{
	char *nv, *nvp, *b;

	if (!vlan_enable())
		return;

	//char *name, *ifname, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	nv = nvp = strdup(nvram_safe_get("subnet_rulelist"));

	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			char brif[8];
			char count = 0;
			char *name = NULL, *ip = NULL, *netmask = NULL, *dhcp_enable = NULL, *dhcp_start = NULL, 
				*dhcp_end = NULL, *lease = NULL, *dns1 = NULL, *dns2 = NULL, *wins = NULL;

			//if ((vstrsep(b, ">", &name, &ifname, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 8))
			//if ((vstrsep(b, ">", &name, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 7))
			_dprintf("%s %d - %s\n", __FUNCTION__, __LINE__, b);

			count = vstrsep(b, ">", &name, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease, &dns1, &dns2, &wins);

			if (count < 7 || (count > 7 && count < 10))
				continue;

			_dprintf("%s %d - count (%d)\n", __FUNCTION__, __LINE__, count);

			memset(brif, 0x0, sizeof(brif));
			if (!strcmp(dhcp_enable, "1") && check_used_subnet(name, brif)) {
				fprintf(fp, "interface=%s\n", brif);
				fprintf(fp, "dhcp-range=%s,%s,%s,%s,%ss\n", brif, dhcp_start, dhcp_end, netmask, lease);
				fprintf(fp, "dhcp-option=%s,3,%s\n", brif, ip);

#ifdef RTCONFIG_SFEXPRESS
				if (count > 7) {
					/* dns server for dns1 and dns2 */
					if (*dns1 && inet_addr(dns1)) {
						fprintf(fp, "dhcp-option=%s,6,%s", brif, dns1);
						if (*dns2 && inet_addr(dns2))
							fprintf(fp, ",%s", dns2);
						fprintf(fp, "\n");
					}
					else if (*dns2 && inet_addr(dns2))
						fprintf(fp, "dhcp-option=%s,6,%s\n", brif, dns2);

					/* wins server */
					if (*wins)
						fprintf(fp, "dhcp-option=%s,44,%s\n", brif, wins);
				}
#endif
			}
		}
		free(nv);
	}
}

void vlan_subnet_filter_input(FILE *fp)
{
	char *nv, *nvp, *b;
	//char *name, *ifname, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	char *name, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;

	if (!vlan_enable())
		return;

	nv = nvp = strdup(nvram_safe_get("subnet_rulelist"));

	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			char brif[8];

			//if ((vstrsep(b, ">", &name, &ifname, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 8))
			if ((vstrsep(b, ">", &name, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 7))
				continue;
	
			memset(brif, 0x0, sizeof(brif));

			/* Deny brX to access DNS on the router */
			if (check_used_subnet(name, brif) && check_intranet_only(name)) {
				fprintf(fp, "-A INPUT -i %s -p udp --dport 53 -j DROP\n", brif);
				fprintf(fp, "-A INPUT -i %s -p tcp --dport 53 -j DROP\n", brif);
			}

			/* Access brX from accessing the router's local sockets */
			if (check_used_subnet(name, brif))
				//fprintf(fp, "-A INPUT -i %s -m state --state NEW -j ACCEPT\n", ifname);
				fprintf(fp, "-A INPUT -i %s -m state --state NEW -j ACCEPT\n", brif);
		}
		free(nv);

		//if (used_flag)
		//	fprintf(fp, "-A INPUT -i br+ -m state --state NEW -j ACCEPT\n");
	}
}

void vlan_subnet_filter_forward(FILE *fp, char *wan_if)
{
	char *nv, *nvp, *b;
	//char *name, *ifname, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	char *name, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	//int used_flag = 0;

	if (!vlan_enable())
		return;

	nv = nvp = strdup(nvram_safe_get("subnet_rulelist"));

	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			char brif[8];

			//if ((vstrsep(b, ">", &name, &ifname, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 8))
			if ((vstrsep(b, ">", &name, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 7))
				continue;

			memset(brif, 0x0, sizeof(brif));

			/* Access/Deny brX from accessing the WAN subnet (no internet access) */
			if (check_used_subnet(name, brif) && !check_intranet_only(name)) {
				fprintf(fp, "-A FORWARD -i %s -o %s -j ACCEPT\n", brif, wan_if);
			}
			else
			{
				if (strlen(brif))
					fprintf(fp, "-A FORWARD -i %s -o %s -j DROP\n", brif, wan_if);
			}
		}
		free(nv);

		//if (used_flag)
		//	fprintf(fp, "-A FORWARD -i br+ -o %s -j ACCEPT\n", wan_if);
	}
}

int check_exist_subnet_access_rule(int index, int subnet_group_tmp)
{
	int result = 0;
	char *nv, *nvp, *b;
	int i = 1;

	nv = nvp = strdup(nvram_safe_get("gvlan_rulelist"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			int subnet_group_all = 0;
			subnet_group_all = atoi(b);
			
			if ( (subnet_group_all & subnet_group_tmp) == subnet_group_tmp) {
				result = 1;
				break;
			}

			if (++i == index)
				break;
		}
		free(nv);
	}

	return result;
}

void group_subnet_access_forward(FILE *fp)
{
	char *nv, *nvp, *b;

	nv = nvp = strdup(nvram_safe_get("gvlan_rulelist"));

	if (nv) {
		int check_index = 1;
		while ((b = strsep(&nvp, "<")) != NULL) {
			int group = 0;
			int i, j;

			group = atoi(b);
			for (i = 0; i < 8; i++) {
				if ((group >> i) & 0x1) {
					for (j = (i + 1); j < 8; j++) {
						if ((group >> j) & 0x1) {
							char nvif1[8], nvif2[8], brif1[8], brif2[8];

							memset(nvif1, 0x0, sizeof(nvif1));
							memset(nvif2, 0x0, sizeof(nvif2));
							memset(brif1, 0x0, sizeof(brif1));
							memset(brif2, 0x0, sizeof(brif2));
							snprintf(nvif1, sizeof(nvif1), "LAN%d", i + 1);
							snprintf(nvif2, sizeof(nvif2), "LAN%d", j + 1);

							if (check_used_subnet(nvif1, brif1) && check_used_subnet(nvif2, brif2)) {
								int subnet_group = (1 << i) | (1 << j);
								if (check_index == 1 || !check_exist_subnet_access_rule(check_index, subnet_group)) {
									fprintf(fp, "-A FORWARD -i %s -o %s -j ACCEPT\n", brif1, brif2);
									fprintf(fp, "-A FORWARD -i %s -o %s -j ACCEPT\n", brif2, brif1);
								}
							}
						}
					}
				}
			}
			check_index++;
		}
		free(nv);
	}
}

void vlan_subnet_deny_forward(FILE *fp)
{
	char *nv, *nvp, *b;
	//char *name, *ifname, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	char *name, *ip, *netmask, *dhcp_enable, *dhcp_start, *dhcp_end, *lease;
	int used_flag = 0;

	if (!vlan_enable())
		return;

	group_subnet_access_forward(fp);

	nv = nvp = strdup(nvram_safe_get("subnet_rulelist"));

	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			char brif[8];

			//if ((vstrsep(b, ">", &name, &ifname, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 8))
			if ((vstrsep(b, ">", &name, &ip, &netmask, &dhcp_enable, &dhcp_start, &dhcp_end, &lease) != 7))
				continue;

			memset(brif, 0x0, sizeof(brif));
			if (check_used_subnet(name, brif)) {
				used_flag = 1;
				//fprintf(fp, "-A FORWARD -i ! %s -o %s -j DROP\n", brif, brif);
				fprintf(fp, "-A FORWARD -o %s ! -i %s -j DROP\n", brif, brif);
			}
		}
		free(nv);

		if (used_flag)
			//fprintf(fp, "-A FORWARD -i ! %s -o %s -j DROP\n", nvram_safe_get("lan_ifname"), nvram_safe_get("lan_ifname"));
			fprintf(fp, "-A FORWARD -o %s ! -i %s -j DROP\n", nvram_safe_get("lan_ifname"), nvram_safe_get("lan_ifname"));
	}
}

void start_vlan_wl(void)
{
#ifdef CONFIG_BCMWL5
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	if (!vlan_enable())
		return;

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		char *lan_ifname;
		char *lan_ifnames, *ifname, *p;
		char nv[32];
		int unit, subunit;
		int is_client = 0;
		char tmp[100], tmp2[100], prefix[] = "wlXXXXXXXXXXXXXX";

		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_ifname", i);

		lan_ifname = strdup(nvram_safe_get(nv));
		if (strncmp(lan_ifname, "br", 2) == 0) {
			memset(nv, 0x0, sizeof(nv));
			sprintf(nv, "lan%d_ifnames", i);
			if ((lan_ifnames = strdup(nvram_safe_get(nv))) != NULL) {
				p = lan_ifnames;
				while ((ifname = strsep(&p, " ")) != NULL) {
					while (*ifname == ' ') ++ifname;
					if (*ifname == 0) break;

					unit = -1; subunit = -1;

					// ignore disabled wl vifs
					if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
						char wl_nv[40];
						snprintf(wl_nv, sizeof(wl_nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
						if (!nvram_get_int(wl_nv))
							continue;
						if (get_ifname_unit(ifname, &unit, &subunit) < 0)
							continue;
					}
					// get the instance number of the wl i/f
					else if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
						continue;

					is_client |= wl_client(unit, subunit) && nvram_get_int(wl_nvname("radio", unit, 0));

#ifdef CONFIG_BCMWL5
					snprintf(prefix, sizeof(prefix), "wl%d_", unit);
					if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
					{
						nvram_set_int(strcat_r(prefix, "timesched", tmp2), 0);	// disable wifi time-scheduler
#ifdef RTCONFIG_QTN
						if (strcmp(ifname, "wifi0"))
#endif
						{
							eval("wlconf", ifname, "down");
							eval("wl", "-i", ifname, "radio", "off");
						}
					}
					else
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
					if (!is_psta(1 - unit))
#endif
#endif
					{
#ifdef RTCONFIG_QTN
						if (strcmp(ifname, "wifi0"))
#endif
						eval("wlconf", ifname, "start"); /* start wl iface */
					}
					wlconf_post(ifname);
#endif	// CONFIG_BCMWL5
				}
				free(lan_ifnames);
			}
		}
#ifdef CONFIG_BCMWL5
		else if (strcmp(lan_ifname, "")) {
			/* specific non-bridged lan iface */
			eval("wlconf", lan_ifname, "start");
		}
#endif	// CONFIG_BCMWL5

		free(lan_ifname);

		if (is_client)
			xstart("radio", "join");
	}
#endif /* CONFIG_BCMWL5 */

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

void restart_vlan_wl(void)
{
#ifdef CONFIG_BCMWL5
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	if (!vlan_enable())
		return;

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {
		char *lan_ifname;
		char *lan_ifnames, *ifname, *p;
		char nv[32];
		int unit, subunit;
		int is_client = 0;
		char tmp[100], tmp2[100], prefix[] = "wlXXXXXXXXXXXXXX";

		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_ifname", i);

		lan_ifname = strdup(nvram_safe_get(nv));
		if (strncmp(lan_ifname, "br", 2) == 0) {
			memset(nv, 0x0, sizeof(nv));
			sprintf(nv, "lan%d_ifnames", i);
			if ((lan_ifnames = strdup(nvram_safe_get(nv))) != NULL) {
				p = lan_ifnames;
				while ((ifname = strsep(&p, " ")) != NULL) {
					while (*ifname == ' ') ++ifname;
					if (*ifname == 0) break;

					unit = -1; subunit = -1;

					// ignore disabled wl vifs
					if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
						char wl_nv[40];
						snprintf(wl_nv, sizeof(wl_nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
						if (!nvram_get_int(wl_nv))
							continue;
						if (get_ifname_unit(ifname, &unit, &subunit) < 0)
							continue;
					}
					// get the instance number of the wl i/f
					else if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
						continue;

					is_client |= wl_client(unit, subunit) && nvram_get_int(wl_nvname("radio", unit, 0));

#ifdef CONFIG_BCMWL5
					snprintf(prefix, sizeof(prefix), "wl%d_", unit);
					if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
					{
						nvram_set_int(strcat_r(prefix, "timesched", tmp2), 0);	// disable wifi time-scheduler
#ifdef RTCONFIG_QTN
						if (strcmp(ifname, "wifi0"))
#endif
						{
							eval("wlconf", ifname, "down");
							eval("wl", "-i", ifname, "radio", "off");
						}
					}
					else
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
					if (!is_psta(1 - unit))
#endif
#endif
					{
#ifdef RTCONFIG_QTN
						if (strcmp(ifname, "wifi0"))
#endif
						eval("wlconf", ifname, "start"); /* start wl iface */
					}
					wlconf_post(ifname);
#endif	// CONFIG_BCMWL5
				}
				free(lan_ifnames);
			}
		}
#ifdef CONFIG_BCMWL5
		else if (strcmp(lan_ifname, "")) {
			/* specific non-bridged lan iface */
			eval("wlconf", lan_ifname, "start");
		}
#endif	// CONFIG_BCMWL5

		free(lan_ifname);

		if (is_client)
			xstart("radio", "join");
	}
#endif /* CONFIG_BCMWL5 */

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

void vlan_lanaccess_mssid_ban(const char *limited_ifname, char *ip, char *netmask)
{
	char lan_subnet[32];

	if (limited_ifname == NULL) return;

	if (nvram_get_int("sw_mode") != SW_MODE_ROUTER) return;

	eval("ebtables", "-A", "FORWARD", "-i", (char*)limited_ifname, "-j", "DROP"); //ebtables FORWARD: "for frames being forwarded by the bridge"
	eval("ebtables", "-A", "FORWARD", "-o", (char*)limited_ifname, "-j", "DROP"); // so that traffic via host and nat is passed

	if (strcmp(ip, "0.0.0.0") && strcmp(netmask, "0.0.0.0")) {
		snprintf(lan_subnet, sizeof(lan_subnet), "%s/%s", ip, netmask);
		eval("ebtables", "-t", "broute", "-A", "BROUTING", "-i", (char*)limited_ifname, "--ip-dst", lan_subnet, "--ip-proto", "tcp", "-j", "DROP");
	}
}

void vlan_lanaccess_wl(void)
{
	int i;
	int vlan_index = nvram_get_int("vlan_index");

	if (!vlan_enable())
		return;

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	for (i = VLAN_START_IFNAMES_INDEX; i <= vlan_index; i++) {

		char *p, *ifname;
		char *wl_ifnames;
#ifdef CONFIG_BCMWL5
		int unit, subunit;
#endif
		char nv[32];

		memset(nv, 0x0, sizeof(nv));
		sprintf(nv, "lan%d_ifnames", i);

		if ((wl_ifnames = strdup(nvram_safe_get(nv))) != NULL) {
			p = wl_ifnames;
			while ((ifname = strsep(&p, " ")) != NULL) {
				while (*ifname == ' ') ++ifname;
				if (*ifname == 0) break;
#ifdef CONFIG_BCMWL5
				if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
					if (get_ifname_unit(ifname, &unit, &subunit) < 0)
						continue;
				}
				else continue;

#ifdef RTCONFIG_WIRELESSREPEATER
				if(nvram_get_int("sw_mode")==SW_MODE_REPEATER && unit==nvram_get_int("wlc_band") && subunit==1) continue;
#endif
#elif defined RTCONFIG_RALINK
				if (!strncmp(ifname, "ra", 2) && !strchr(ifname, '0'))
					;
				else
					continue;
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
				if (strncmp(ifname, "rai", 3) == 0)
					continue;
#endif
				memset(nv, 0x0, sizeof(nv));
				snprintf(nv, sizeof(nv) - 1, "%s_lanaccess", wif_to_vif(ifname));
				if (!strcmp(nvram_safe_get(nv), "off")) {
					char *lan_subnet, ip[32], mask[32];
					memset(nv, 0x0, sizeof(nv));
					snprintf(nv, sizeof(nv), "lan%d_subnet", i);
					lan_subnet = nvram_safe_get(nv);
					vlan_lanaccess_mssid_ban(ifname, get_info_by_subnet(lan_subnet, SUBNET_IP, ip), get_info_by_subnet(lan_subnet, SUBNET_MASK, mask));
				}
			}
			free(wl_ifnames);
		}
	}
}


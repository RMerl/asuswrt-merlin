/*
 * WPS AP Set Selected Registrar
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_ssr.c 290904 2011-10-20 15:18:57Z $
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__ECOS)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __ECOS
#include <net/if.h>
#include <sys/param.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#endif /* __linux__ || __ECOS */

#ifdef WIN32
#include <windows.h>
#endif /* WIN32 */

#include <tutrace.h>
#include <wpscommon.h>
#include <wpsheaders.h>

#include <wps_apapi.h>
#include <sminfo.h>
#include <reg_proto.h>
#include <reg_prototlv.h>
#include <statemachine.h>
#include <wpsapi.h>
#ifdef WPS_UPNP_DEVICE
#include <ap_upnp_sm.h>
#endif
#include <ap_eap_sm.h>
#include <ap_ssr.h>


/* WSC 2.0 */
/*
 * We did not support/protect multiple Registrars SSR in WSC 1.0
 * But in WSC 2.0, we have to support it according to spec session 8.4.1.
 * So, we have to take case of upnpssr_time (it must be the first SSR time)
 * and use ap_ssr_scb instead of upnpssr_ipaddr(implemented in WSC 1.0),
 * and according to enrollee's MAC address to send message to correctly registrar.
 */
WPS_SSR_SCB *ap_ssr_scb = NULL;
char empty_ipaddr[sizeof("255.255.255.255")] = {0};

uint32
ap_ssr_init()
{
	return WPS_SUCCESS;
}

uint32
ap_ssr_deinit()
{
	return WPS_SUCCESS;
}

static WPS_SSR_SCB *
ap_ssr_find_scb(void *findobj, int type, int enter)
{
	int i;
	int found = 0;
	char *mac;
	WPS_SSR_SCB *scb = ap_ssr_scb;

	if (type != WPS_SSR_SCB_FIND_ANY && findobj == NULL)
		return NULL;

	switch (type) {
	case WPS_SSR_SCB_FIND_ANY:
		 /* Return first scb */
		 break;

	case WPS_SSR_SCB_FIND_IPADDR: /* String format */
		while (scb) {
			if (strcmp(scb->ipaddr, (char *)findobj) == 0)
				break; /* Found */
			scb = scb->next;
		}

		if (scb == NULL && enter == WPS_SSR_SCB_ENTER) {
			if ((scb = malloc(sizeof(WPS_SSR_SCB))) == NULL)
				return NULL;
			memset(scb, 0, sizeof(WPS_SSR_SCB));
			/* Append to head */
			scb->next = ap_ssr_scb;
			ap_ssr_scb = scb;
			/* Copy ipaddr string */
			wps_strncpy(scb->ipaddr, (char *)findobj, sizeof(scb->ipaddr));
		}
		break;

	case WPS_SSR_SCB_FIND_AUTHORIED_MAC: /* Mac format, <= 30B */
		while (scb) {
			for (i = 0; i < scb->authorizedMacs_len; i += SIZE_MAC_ADDR) {
				mac = &scb->authorizedMacs[i];
				if (memcmp(mac, (char *)findobj, SIZE_MAC_ADDR) == 0) {
					found = 1;
					break; /* Found */
				}
			}

			if (found)
				break;

			scb = scb->next;
		}
		break;

	case WPS_SSR_SCB_FIND_UUID_R: /* UUID format, = 16B */
		while (scb) {
			if (memcmp(scb->uuid_R, (char *)findobj, sizeof(scb->uuid_R)) == 0)
				break; /* Found */
			scb = scb->next;
		}
		break;

	case WPS_SSR_SCB_FIND_VERSION1:
		while (scb) {
			if (scb->version == *(unsigned char *)findobj)
				break; /* Found */
			scb = scb->next;
		}
		break;

	default:
		return NULL;
	}

	return scb;
}

static void
ap_ssr_sort_scb()
{
	WPS_SSR_SCB *loop, *first, *second;
	WPS_SSR_SCB scb;
	int swap_len = sizeof(scb)-sizeof(scb.next);

	if (!ap_ssr_scb)
		return;

	loop = ap_ssr_scb;
	second = ap_ssr_scb;

	for (; loop->next != NULL; loop = loop->next) {
		for (; second->next != NULL; ) {
			first = second;
			second = second->next;

			if (first->upd_time < second->upd_time) {
				memcpy(&scb, first, swap_len);
				memcpy(first, second, swap_len);
				memcpy(second, &scb, swap_len);
			}
		}

		first = ap_ssr_scb;
		second = ap_ssr_scb;
	}
}

/* Return original authorizedMacs_len if we do not add new_mac in list */
static int
_add_mac_in_list(char *authorizedMacs_buf, int authorizedMacs_len, char *new_mac)
{
	int i;
	int duplicated = 0;
	char *mac;

	/* Return original authorizedMacs_len when add nothing */
	if (!new_mac)
		return authorizedMacs_len;

	/* Check duplicate */
	if (authorizedMacs_len) {
		for (i = 0; i < authorizedMacs_len; i += SIZE_MAC_ADDR) {
			mac = &authorizedMacs_buf[i];
			if (memcmp(mac, new_mac, SIZE_MAC_ADDR) == 0) {
				duplicated = 1;
				break;
			}
		}
	}

	/* Return original authorizedMacs_len when duplicated */
	if (duplicated)
		return authorizedMacs_len;

	/* Copy new mac to list */
	memcpy(&authorizedMacs_buf[authorizedMacs_len], new_mac, SIZE_MAC_ADDR);

	return authorizedMacs_len + SIZE_MAC_ADDR;
}

/* Add/Remove a SSR */
int
ap_ssr_set_scb(char *ipaddr, CTlvSsrIE *ssrmsg, char *wps_ifname, unsigned long upd_time)
{
	WPS_SSR_SCB *scb, *ptr;


	if (!ipaddr || !ssrmsg)
		return -1;

	/* Find scb by ipaddr */
	scb = ap_ssr_find_scb(ipaddr, WPS_SSR_SCB_FIND_IPADDR,
		ssrmsg->selReg.m_data ? WPS_SSR_SCB_ENTER : WPS_SSR_SCB_SEARCH_ONLY);
	if (scb == NULL) {
		TUTRACE((TUTRACE_INFO, "Can not set SSR ipaddr, selReg %d!\n",
			ssrmsg->selReg.m_data));
		return -1;
	}

	/* Update information */
	if (ssrmsg->selReg.m_data) {
		scb->version = ssrmsg->version.m_data;
		scb->version2 = ssrmsg->version2.m_data;
		scb->scState = ssrmsg->scState.m_data;
		scb->upd_time = upd_time;
		if (wps_ifname)
			wps_strncpy(scb->wps_ifname, wps_ifname, sizeof(scb->wps_ifname));

		/* MAC format, check authorizedMacs_len */
		if (ssrmsg->authorizedMacs.subtlvbase.m_len) {
			scb->authorizedMacs_len = ssrmsg->authorizedMacs.subtlvbase.m_len;
			memcpy(scb->authorizedMacs, ssrmsg->authorizedMacs.m_data,
				ssrmsg->authorizedMacs.subtlvbase.m_len);
		}

		/* UUID format, check uuid_R_len */
		if (ssrmsg->uuid_R.tlvbase.m_len)
			memcpy(scb->uuid_R, ssrmsg->uuid_R.m_data, SIZE_UUID);

		scb->selRegCfgMethods = ssrmsg->selRegCfgMethods.m_data;
		scb->devPwdId = ssrmsg->devPwdId.m_data;
	} else {
		/* Remove this scb */
		if (scb == ap_ssr_scb) {
			ap_ssr_scb = ap_ssr_scb->next;
			free(scb);
		} else {
			ptr = ap_ssr_scb;
			while (ptr) {
				if (ptr->next == scb) {
					ptr->next = scb->next;
					free(scb);
					break;
				}
				ptr = ptr->next;
			}
			if (ptr == NULL) {
				TUTRACE((TUTRACE_INFO, "Can't remove scb from ap_ssr_scb list\n"));
				return -1;
			}
		}
	}

	return 0;
}

int
ap_ssr_get_scb_num()
{
	WPS_SSR_SCB *scb;
	int scb_num = 0;

	scb = ap_ssr_scb;
	while (scb) {
		scb_num++;
		scb = scb->next;
	}

	return scb_num;
}

/* Get all authorized MACs, most recently 5 mac */
int
ap_ssr_get_authorizedMacs(char *authorizedMacs_buf)
{
	int i;
	int full = 0;
	int authorizedMacs_len_max = SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM;
	int authorizedMacs_len = 0;
	char *mac;
	WPS_SSR_SCB *scb;

	if (authorizedMacs_buf == NULL)
		return 0;

	/* Sort scb by upd_time */
	ap_ssr_sort_scb();

	/* Add mac in list from recently SSR */
	scb = ap_ssr_scb;
	while (scb) {
		for (i = 0; i < scb->authorizedMacs_len; i += SIZE_MAC_ADDR) {
			mac = &scb->authorizedMacs[i];
			authorizedMacs_len = _add_mac_in_list(authorizedMacs_buf,
				authorizedMacs_len, mac);
			if (authorizedMacs_len >= authorizedMacs_len_max) {
				full = 1;
				break;
			}
		}

		if (full)
			break;

		scb = scb->next;
	}

	return authorizedMacs_len;
}

int
ap_ssr_get_union_attributes(CTlvSsrIE *ssrmsg, char *authorizedMacs_buf,
	int *authorizedMacs_len)
{
	int i, total_len = 0;
	int full = 0;
	int scb_num = 0;
	char *mac;
	unsigned short selRegCfgMethods = 0;
	WPS_SSR_SCB *scb;

	if (authorizedMacs_buf == NULL) {
		*authorizedMacs_len = 0;
		return 0;
	}

	/* Sort scb by upd_time */
	ap_ssr_sort_scb();

	/* Add mac in list from recently SSR */
	scb = ap_ssr_scb;
	while (scb) {
		for (i = 0; i < scb->authorizedMacs_len; i += SIZE_MAC_ADDR) {
			mac = &scb->authorizedMacs[i];
			total_len = _add_mac_in_list(authorizedMacs_buf, total_len, mac);
			if (total_len >= SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM) {
				full = 1;
				break;
			}
		}

		if (full)
			break;

		scb = scb->next;
	}

	/* Get union of selRegCfgMethods */
	scb = ap_ssr_scb;
	while (scb) {
		scb_num++;
		selRegCfgMethods |= scb->selRegCfgMethods;

		scb = scb->next;
	}

	/* Update ssrmsg's selRegCfgMethods */
	ssrmsg->selRegCfgMethods.m_data = selRegCfgMethods;
	*authorizedMacs_len = total_len;

	return scb_num;
}

/* Get first non-WPS V2 registrar's ipaddr when no uuid_r and enroll_mac arguments */
char *
ap_ssr_get_ipaddr(char* uuid_r, char *enroll_mac)
{
	void *findobj;
	int findtype;
	WPS_SSR_SCB *scb;

	if (!uuid_r && !enroll_mac) {
		findobj = NULL;
		findtype = WPS_SSR_SCB_FIND_ANY;
	} else {
		if (uuid_r) {
			findobj = uuid_r;
			findtype = WPS_SSR_SCB_FIND_UUID_R;
		}
		else {
			findobj = enroll_mac;
			findtype = WPS_SSR_SCB_FIND_AUTHORIED_MAC;
		}
	}

	/* Find scb by findobj */
	scb = ap_ssr_find_scb(findobj, findtype, WPS_SSR_SCB_SEARCH_ONLY);
	if (scb == NULL) {
		TUTRACE((TUTRACE_INFO, "Can not find scb\n"));
		return empty_ipaddr;
	}

	return scb->ipaddr;
}

CTlvSsrIE *
ap_ssr_get_scb_info(char *ipaddr, CTlvSsrIE *ssrmsg)
{
	WPS_SSR_SCB *scb;

	if (!ipaddr || !ssrmsg)
		return NULL;

	/* Find scb by ipaddr */
	scb = ap_ssr_find_scb(ipaddr, WPS_SSR_SCB_FIND_IPADDR, WPS_SSR_SCB_SEARCH_ONLY);
	if (scb == NULL) {
		TUTRACE((TUTRACE_INFO, "Can not find scb by %s\n", ipaddr));
		return NULL;
	}

	ssrmsg->version.m_data = scb->version;
	ssrmsg->scState.m_data = scb->scState;
	ssrmsg->selReg.m_data = 1;
	ssrmsg->devPwdId.m_data = scb->devPwdId;
	ssrmsg->version2.m_data = scb->version2;

	return ssrmsg;
}

/* Get newest scb's Device Password ID attribute */
CTlvSsrIE *
ap_ssr_get_recent_scb_info(CTlvSsrIE *ssrmsg)
{
	if (!ap_ssr_scb || !ssrmsg)
		return NULL;

	ssrmsg->version.m_data = ap_ssr_scb->version;
	ssrmsg->scState.m_data = ap_ssr_scb->scState;
	ssrmsg->selReg.m_data = 1;
	ssrmsg->devPwdId.m_data = ap_ssr_scb->devPwdId;
	ssrmsg->version2.m_data = ap_ssr_scb->version2;

	return ssrmsg;
}

char *
ap_ssr_get_wps_ifname(char *ipaddr, char *wps_ifname)
{
	WPS_SSR_SCB *scb;

	if (!ipaddr)
		return NULL;

	/* Find scb by ipaddr */
	scb = ap_ssr_find_scb(ipaddr, WPS_SSR_SCB_FIND_IPADDR, WPS_SSR_SCB_SEARCH_ONLY);
	if (scb == NULL) {
		TUTRACE((TUTRACE_INFO, "Can not find scb by %s\n", ipaddr));
		return NULL;
	}

	if (wps_ifname)
		strcpy(wps_ifname, scb->wps_ifname);

	return wps_ifname;
}

void
ap_ssr_free_all_scb()
{
	WPS_SSR_SCB *scb = ap_ssr_scb;

	while (ap_ssr_scb) {
		scb = ap_ssr_scb;
		ap_ssr_scb = ap_ssr_scb->next;
		free(scb);
	}
}

#ifdef _TUDEBUGTRACE
void
ap_ssr_dump_all_scb(char *title)
{
	int i;
	unsigned char *Puchar;
	WPS_SSR_SCB *scb = ap_ssr_scb;

	if (title)
		TUTRACE((TUTRACE_INFO, "Title: %s\n", title));

	while (scb) {
		/* print it */
		TUTRACE((TUTRACE_INFO, "version: %02x\n", scb->version));
		TUTRACE((TUTRACE_INFO, "version2: %02x\n", scb->version2));
		TUTRACE((TUTRACE_INFO, "scState: %02x\n", scb->scState));
		TUTRACE((TUTRACE_INFO, "upd_time: %ld\n", scb->upd_time));
		TUTRACE((TUTRACE_INFO, "ipaddr: %s\n", scb->ipaddr));
		if (scb->wps_ifname[0] != 0)
			TUTRACE((TUTRACE_INFO, "wps_ifname: %s\n", scb->wps_ifname));
		TUTRACE((TUTRACE_INFO, "authorizedMacs len %d\n", scb->authorizedMacs_len));
		for (i = 0; i < (scb->authorizedMacs_len/6); i++) {
			Puchar = (unsigned char*)&scb->authorizedMacs[i];
			TUTRACE((TUTRACE_INFO, "authorizedMacs %02x:%02x:%02x:%02x:%02x:%02x\n",
				Puchar[0], Puchar[1], Puchar[2], Puchar[3], Puchar[4], Puchar[5]));
		}
		Puchar = (unsigned char *)scb->uuid_R;
		TUTRACE((TUTRACE_INFO, "uuid_R: %02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x\n",
			Puchar[0], Puchar[1], Puchar[2], Puchar[3], Puchar[4],
			Puchar[5], Puchar[6], Puchar[7], Puchar[8], Puchar[9],
			Puchar[10], Puchar[11], Puchar[12], Puchar[13], Puchar[14], Puchar[15]));
		TUTRACE((TUTRACE_INFO, "selRegCfgMethods: 0x%04x\n", scb->selRegCfgMethods));
		TUTRACE((TUTRACE_INFO, "devPwdId: 0x%04x\n", scb->devPwdId));

		/* next one */
		scb = scb->next;
	}
}
#endif /*  _TUDEBUGTRACE */

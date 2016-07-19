/*
 * ppp scripts
 *
 * Copyright (C) 2009, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ppp.c,v 1.19 2009/12/02 20:07:37 Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <rc.h>

/*
* parse ifname to retrieve unit #
*/
int
ppp_ifunit(char *ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), ifname))
			return unit;
	}

	return -1;
}

int
ppp_linkunit(char *linkname)
{
	if (strncmp(linkname, "wan", 3))
		return -1;
	if (!isdigit(linkname[3]))
		return -1;
	return atoi(&linkname[3]);
}

/*
 * Called when link comes up
 */
int
ipup_main(int argc, char **argv)
{
	FILE *fp;
	char *wan_ifname = safe_getenv("IFNAME");
	char *wan_linkname = safe_getenv("LINKNAME");
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char buf[256], *value;
	int unit;

	_dprintf("%s():: %s\n", __FUNCTION__, argv[0]);

	/* Get unit from LINKNAME: ppp[UNIT] */
	if ((unit = ppp_linkunit(wan_linkname)) < 0)
		return 0;

	_dprintf("%s: unit=%d ifname=%s\n", __FUNCTION__, unit, wan_ifname);
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

#ifdef DEBUG_RCTEST // Left for UI debug
	int max_count = nvram_get_int("ppp_delay_sec");
	int count;
	if(max_count > 0){
		for(count = 1; count <= max_count; ++count){
			_dprintf("%s: unit=%d ifname=%s sleep %d seconds...\n", __FUNCTION__, unit, wan_ifname, count);
			sleep(1);
		}
	}
#endif

	/* Stop triggering demand connection */
	if (nvram_get_int(strcat_r(prefix, "pppoe_demand", tmp)))
		nvram_set_int(strcat_r(prefix, "pppoe_demand", tmp), 1);

#ifdef RTCONFIG_USB_MODEM
	// wanX_ifname is used for device for USB Modem
	if ((value = getenv("DEVICE")) &&
	    (isSerialNode(value) || isACMNode(value))){
		nvram_set(strcat_r(prefix, "ifname", tmp), value);
		nvram_set(strcat_r(prefix, "proto", tmp), "pppoe");
	}
#endif

	/* Touch connection file */
	if (!(fp = fopen(strcat_r("/tmp/ppp/link.", wan_ifname, tmp), "a"))) {
		perror(tmp);
		return errno;
	}
	fclose(fp);

	if ((value = getenv("IPLOCAL"))) {
		if (nvram_invmatch(strcat_r(prefix, "ipaddr", tmp), value))
			ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);
		_ifconfig(wan_ifname, IFUP, value, "255.255.255.255", getenv("IPREMOTE"), 0);
		nvram_set(strcat_r(prefix, "ipaddr", tmp), value);
		nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.255.255");
	}

	if ((value = getenv("IPREMOTE")))
		nvram_set(strcat_r(prefix, "gateway", tmp), value);

	strcpy(buf, "");
	if ((value = getenv("DNS1")))
		sprintf(buf, "%s", value);
	if ((value = getenv("DNS2")))
		sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? " " : "", value);

	/* empty DNS means they either were not requested or peer refused to send them.
	 * for this case static DNS can be used, if they are configured */
	if (strlen(buf) == 0 && !nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp))) {
		value = nvram_safe_get(strcat_r(prefix, "dns1_x", tmp));
		if (*value && inet_addr_(value) != INADDR_ANY)
			sprintf(buf, "%s", value);
		value = nvram_safe_get(strcat_r(prefix, "dns2_x", tmp));
		if (*value && inet_addr_(value) != INADDR_ANY)
			sprintf(buf + strlen(buf), "%s%s", *buf ? " " : "", value);
	}

	nvram_set(strcat_r(prefix, "dns", tmp), buf);

	wan_up(wan_ifname);

	_dprintf("%s:: done\n", __FUNCTION__);
	return 0;
}

/*
 * Called when link goes down
 */
int
ipdown_main(int argc, char **argv)
{
	char *wan_ifname = safe_getenv("IFNAME");
	char *wan_linkname = safe_getenv("LINKNAME");
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int unit;

	_dprintf("%s():: %s\n", __FUNCTION__, argv[0]);

	/* Get unit from LINKNAME: ppp[UNIT] */
	if ((unit = ppp_linkunit(wan_linkname)) < 0)
		return 0;

	_dprintf("%s: unit=%d ifname=%s\n", __FUNCTION__, unit, wan_ifname);
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	wan_down(wan_ifname);

	// override wan_state to get real reason
	update_wan_state(prefix, WAN_STATE_STOPPED, pppstatus(unit));

	unlink(strcat_r("/tmp/ppp/link.", wan_ifname, tmp));

	preset_wan_routes(wan_ifname);

	_dprintf("%s:: done\n", __FUNCTION__);
	return 0;
}

/*
 * Called when interface comes up
 */
int
ippreup_main(int argc, char **argv)
{
	char *wan_ifname = safe_getenv("IFNAME");
	char *wan_linkname = safe_getenv("LINKNAME");
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int unit;

	_dprintf("%s():: %s\n", __FUNCTION__, argv[0]);

	/* Get unit from LINKNAME: ppp[UNIT] */
	if ((unit = ppp_linkunit(wan_linkname)) < 0)
		return 0;

	_dprintf("%s: unit=%d ifname=%s\n", __FUNCTION__, unit, wan_ifname);
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	/* Set wanX_pppoe_ifname to real interface name */
	nvram_set(strcat_r(prefix, "pppoe_ifname", tmp), wan_ifname);

	/* Start triggering demand connection */
	if (nvram_get_int(strcat_r(prefix, "pppoe_demand", tmp)))
		nvram_set_int(strcat_r(prefix, "pppoe_demand", tmp), 2);

	_dprintf("%s:: done\n", __FUNCTION__);
	return 0;
}

#ifdef RTCONFIG_IPV6
int ip6up_main(int argc, char **argv)
{
	char *wan_ifname = safe_getenv("IFNAME");
	char *wan_linkname = safe_getenv("LINKNAME");
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int unit;

	if (!wan_ifname || strlen(wan_ifname) <= 0)
		return 0;

	/* Get unit from LINKNAME: ppp[UNIT] */
	if ((unit = ppp_linkunit(wan_linkname)) < 0)
		return 0;

	_dprintf("%s: unit=%d ifname=%s\n", __FUNCTION__, unit, wan_ifname);
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	/* share the same interface with pppoe ipv4 connection */
	nvram_set(strcat_r(prefix, "pppoe_ifname", tmp), wan_ifname);

	wan6_up(wan_ifname);

	return 0;
}

int ip6down_main(int argc, char **argv)
{
	char *wan_ifname = safe_getenv("IFNAME");

	wan6_down(wan_ifname);

	return 0;
}
#endif	// IPV6

/*
 * Called when link closing with auth fail
 */
int
authfail_main(int argc, char **argv)
{
	char *wan_ifname = safe_getenv("IFNAME");
	char *wan_linkname = safe_getenv("LINKNAME");
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int unit;

	_dprintf("%s():: %s\n", __FUNCTION__, argv[0]);

	/* Get unit from LINKNAME: ppp[UNIT] */
	if ((unit = ppp_linkunit(wan_linkname)) < 0)
		return 0;

	_dprintf("%s: unit=%d ifname=%s\n", __FUNCTION__, unit, wan_ifname);
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	/* Stop triggering demand connection */
	if (nvram_get_int(strcat_r(prefix, "pppoe_demand", tmp)))
		nvram_set_int(strcat_r(prefix, "pppoe_demand", tmp), 1);

	// override wan_state
	update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PPP_AUTH_FAIL);

	_dprintf("%s:: done\n", __FUNCTION__);
	return 0;
}

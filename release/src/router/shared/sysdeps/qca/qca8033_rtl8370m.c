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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <bcmnvram.h>

#include <utils.h>
#include <shutils.h>
#include <shared.h>
#ifdef RTCONFIG_QCA
#include <net/if.h>
#include <linux/mii.h>
#include <linux/sockios.h>
#endif

#define RTKSWITCH_DEV	"/dev/rtkswitch"

struct trafficCount_t
{
	long long rxByteCount;
	long long txByteCount;
};

int rtkswitch_ioctl(int val, int *val2)
{
	int fd;
	int value = 0;
	int *p = NULL, invalid_ioctl_cmd = 0;
	struct trafficCount_t portTraffic;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	switch (val) {
	case 0:		/* check WAN port phy status */
	case 3:		/* check LAN ports phy status */
	case 6:
	case 8:
	case 9:
	case 10:
	case 19:	/* Set WAN port txDelay */
	case 20:	/* Set WAN port rxDelay */
	case 22:
	case 23:
	case 24:
	case 25:
	case 36:	/* Set Vlan VID. */
	case 37:	/* Set Vlan PRIO. */
	case 38:	/* Initialize VLAN. Cherry Cho added in 2011/7/15. */
	case 39:	/* Create VLAN. Cherry Cho added in 2011/7/15. */
        case 40:
	case 44:	/* set WAN port */
	case 45:	/* set LAN trunk group */
	case 46:	/* set LAN trunk port mask */
	case 99:
	case 100:
	case 109:	/* Set specific ext port txDelay */
	case 110:	/* Set specific ext port rxDelay */
		p = val2;
		break;
	case 29:/* Set VoIP port. Cherry Cho added in 2011/6/30. */
		val = 34;	/* call ioctl 34 instead. */
		p = val2;
		break;
	case 2:		/* show state of RTL8367RB GMAC1 */
	case 7:
	case 11:
	case 14:	/* power up LAN port(s) */
	case 15:	/* power down LAN port(s) */
	case 16:	/* power up all ports */
	case 17:	/* power down all ports */
	case 21:
	case 27:
	case 41:	/* check realtek switch normal */
	case 90:	/* Reset SGMII link */
	case 91:	/* Read EXT port status */
	case 114:	/* power up WAN port(s) */
	case 115:	/* power down WAN port(s) */
		p = NULL;
		break;
	case 42:
	case 43:
		p = (int*) &portTraffic;
		break;

	default:
		invalid_ioctl_cmd = 1;
		break;
	}

	if (invalid_ioctl_cmd) {
		printf("wrong ioctl cmd: %d\n", val);
		close(fd);
		return 0;
	}

	if (ioctl(fd, val, p) < 0) {
		perror("rtkswitch ioctl");
		close(fd);
		return -1;
	}

	if (val == 0 || val == 3)
		printf("return: %x\n", value);
	else if(val == 42 || val == 43)
		printf("rx/tx: %lld/%lld\n", portTraffic.rxByteCount, portTraffic.txByteCount);

	close(fd);
	return 0;
}

int config_rtkswitch(int argc, char *argv[])
{
	int val;
	int val2 = 0;
	char *cmd = NULL;
	char *cmd2 = NULL;

	if (argc >= 2)
		cmd = argv[1];
	else
		return -1;
	if (argc >= 3)
		cmd2 = argv[2];

	val = (int) strtol(cmd, NULL, 0);
	if (cmd2)
		val2 = (int) strtol(cmd2, NULL, 0);

	return rtkswitch_ioctl(val, &val2);
}

int is_wan_unit_in_switch(int wan_unit)
{
	char word[32], *next, wans_dualwan[128];
	int unit = 0;

	strcpy(wans_dualwan, nvram_safe_get("wans_dualwan"));
	foreach (word, wans_dualwan, next) {
		if(strcmp(word, "lan") == 0)
		{
			if(unit == wan_unit)
				return 1;
			break;
		}
		unit++;
	}
	return 0;
}

/**
 * @wan_unit:	wan_unit at run-time.
 * 		If dual-wan is disabled, wan1_ifname may be empty string.
 * 		If dual-wan is enabled, wan0_ifname and wan1_ifname may be LAN or USB modem.
 */
unsigned int rtkswitch_wanPort_phyStatus(int wan_unit)
{
	int sw_mode = nvram_get_int("sw_mode");
	unsigned int value;
	char prefix[16], tmp[100], *ifname;

	if (sw_mode == SW_MODE_AP || sw_mode == SW_MODE_REPEATER)
		return 0;

	if(is_wan_unit_in_switch(wan_unit))
	{
		value = 0;
		if (rtkswitch_ioctl(0, &value) < 0)
			return -1;
		return value;
	}
	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
	ifname = nvram_get(strcat_r(prefix, "ifname", tmp));
	if (!ifname || *ifname == '\0')
		return 0;
	value = !!(mdio_read(ifname, MII_BMSR) & BMSR_LSTATUS);

	return value;
}

unsigned int rtkswitch_lanPorts_phyStatus(void)
{
	unsigned int value = 0;
	if(rtkswitch_ioctl(3, &value) < 0)
		return -1;

	return !!value;

}

unsigned int __rtkswitch_WanPort_phySpeed(int wan_unit)
{
	int v, sw_mode = nvram_get_int("sw_mode");
	unsigned int value = 0;
	char prefix[16], tmp[100], *ifname;

	if (sw_mode == SW_MODE_AP || sw_mode == SW_MODE_REPEATER)
		return 0;

	/* FIXME:
	 * if dual-wan is disabled, wan1_ifname may be empty string.
	 * if dual-wan is enabled, wan0_ifname and wan1_ifname may be LAN or USB modem.
	 */
	if(is_wan_unit_in_switch(wan_unit))
	{
		value = 0;
		if (rtkswitch_ioctl(13, &value) < 0)
			return -1;
		return value;
	}

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
	ifname = nvram_get(strcat_r(prefix, "ifname", tmp));
	if (!ifname || *ifname == '\0')
		return 10;
	v = mdio_read(ifname, MII_BMCR);
	if (v & BMCR_SPEED1000)
		value = 1000;
	else if (v & BMCR_SPEED100)
		value = 100;
	else
		value = 10;

	return value;
}

unsigned int rtkswitch_WanPort_phySpeed(void)
{
	return __rtkswitch_WanPort_phySpeed(0);
}

static void phy_link_ctrl(char *ifname, int onoff)
{
	int fd, v;
	struct ifreq ifr;

	if (!ifname || *ifname == '\0')
		return;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		_dprintf("%s: ifname %s SIOCGMIIPHY failed.\n",
			__func__, ifname);
		close(fd);
		return;
	}
	v = mdio_read(ifname, MII_BMCR);
	if (onoff)
		v &= ~BMCR_PDOWN;
	else
		v |= BMCR_PDOWN;
	mdio_write(ifr.ifr_name, MII_BMCR, v);
	close(fd);
}

int rtkswitch_WanPort_linkUp(void)
{
	phy_link_ctrl(nvram_get("wan0_ifname"), 1);

	return 0;
}

int rtkswitch_WanPort_linkDown(void)
{
	phy_link_ctrl(nvram_get("wan0_ifname"), 0);

	return 0;
}

int rtkswitch_LanPort_linkUp(void)
{
	eval("rtkswitch", "14");

	return 0;
}

int rtkswitch_LanPort_linkDown(void)
{
	eval("rtkswitch", "15");

	return 0;
}

int rtkswitch_AllPort_linkUp(void)
{
	phy_link_ctrl(nvram_get("wan0_ifname"), 1);
	eval("rtkswitch", "16");

	return 0;
}

int rtkswitch_AllPort_linkDown(void)
{
	phy_link_ctrl(nvram_get("wan0_ifname"), 0);
	eval("rtkswitch", "17");

	return 0;
}

int rtkswitch_Reset_Storm_Control(void)
{
	eval("rtkswitch", "21");

	return 0;
}

typedef struct {
	unsigned int link[5];
	unsigned int speed[5];
} phyState;

int rtkswitch_AllPort_phyState(void)
{
	char buf[32];
	int porder_56u[5] = {4,3,2,1,0};
	int *o = porder_56u;
	const char *portMark = "W0=%C;L1=%C;L2=%C;L3=%C;L4=%C;";
	phyState pS;

	pS.link[0] = pS.link[1] = pS.link[2] = pS.link[3] = pS.link[4] = 0;
	pS.speed[0] = pS.speed[1] = pS.speed[2] = pS.speed[3] = pS.speed[4] = 0;

	if (rtkswitch_ioctl(18, (int*) &pS) < 0)
		return -1;

	if (get_model() == MODEL_RTN65U)
		portMark = "W0=%C;L4=%C;L3=%C;L2=%C;L1=%C;";

	sprintf(buf, portMark,
		(pS.link[o[0]] == 1) ? (pS.speed[o[0]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[1]] == 1) ? (pS.speed[o[1]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[2]] == 1) ? (pS.speed[o[2]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[3]] == 1) ? (pS.speed[o[3]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[4]] == 1) ? (pS.speed[o[4]] == 2) ? 'G' : 'M': 'X');

	puts(buf);

	return 0;
}

#if 0
void usage(char *cmd)
{
	printf("Usage: %s 1 - set (SCK, SD) as (0, 1)\n", cmd);
	printf("       %s 2 - set (SCK, SD) as (1, 0)\n", cmd);
	printf("       %s 3 - set (SCK, SD) as (1, 1)\n", cmd);
	printf("       %s 0 - set (SCK, SD) as (0, 0)\n", cmd);
	printf("       %s 4 - init vlan\n", cmd);
	printf("       %s 5 - set cpu port 0 0\n", cmd);
	printf("       %s 6 - set cpu port 0 1\n", cmd);
	printf("       %s 7 - set cpu port 1 0\n", cmd);
	printf("       %s 8 - set cpu port 1 1\n", cmd);
	printf("       %s 10 - set vlan entry, no cpu port, but mbr\n", cmd);
	printf("       %s 11 - set vlan entry, no cpu port, no mbr\n", cmd);
	printf("       %s 15 - set vlan PVID, no cpu port\n", cmd);
	printf("       %s 20 - set vlan entry, with cpu port\n", cmd);
	printf("       %s 21 - set vlan entry, with cpu port and no cpu port in untag sets\n", cmd);
	printf("       %s 25 - set vlan PVID, with cpu port\n", cmd);
	printf("       %s 26 - set vlan PVID, not set cpu port\n", cmd);
	printf("       %s 90 - accept all frmaes\n", cmd);
	printf("       %s 66 - setup default vlan\n", cmd);
	printf("       %s 61 - setup vlan type1\n", cmd);
	printf("       %s 62 - setup vlan type2\n", cmd);
	printf("       %s 63 - setup vlan type3\n", cmd);
	printf("       %s 64 - setup vlan type4\n", cmd);
	printf("       %s 65 - setup vlan type34\n", cmd);
	printf("       %s 70 - disable multicast snooping\n", cmd);
	printf("       %s 81 - setRtctTesting on port x\n", cmd);
	printf("       %s 82 - getRtctResult on port x\n", cmd);
	printf("       %s 83 - setGreenEthernet x(green, powsav)\n", cmd);
	printf("       %s 84 - setAsicGreenFeature x(txGreen, rxGreen)\n", cmd);
	printf("       %s 85 - getAsicGreenFeature\n", cmd);
	printf("       %s 86 - enable GreenEthernet on port x\n", cmd);
	printf("       %s 87 - disable GreenEthernet on port x\n", cmd);
	printf("       %s 88 - getAsicPowerSaving x\n", cmd);
	printf("       %s 50 - getPortLinkStatus x\n", cmd);
	exit(0);
}
#endif

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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bcmnvram.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <shutils.h>
#include <shared.h>
#include <utils.h>
#include <qca.h>

#define NR_WANLAN_PORT	5
#define MAX_WANLAN_PORT	5

enum {
#if defined(RTAC58U)
	CPU_PORT=0,
	LAN1_PORT=4,
	LAN2_PORT=3,
	LAN3_PORT=2,
	LAN4_PORT=1,
	WAN_PORT=5,
	P6_PORT=5,
#elif defined(RTAC82U)
	CPU_PORT=0,
	LAN1_PORT=1,
	LAN2_PORT=2,
	LAN3_PORT=3,
	LAN4_PORT=4,
	WAN_PORT=5,
	P6_PORT=5,
#else
#error Define WAN/LAN ports!
#endif

};

//0:WAN, 1:LAN, lan_wan_partition[][0] is port0
static const int lan_wan_partition[9][NR_WANLAN_PORT] = {
	/* W, L1, L2, L3, L4 */
	{0,1,1,1,1}, //WLLLL
	{0,0,1,1,1}, //WWLLL
	{0,1,0,1,1}, //WLWLL
	{0,1,1,0,1}, //WLLWL
	{0,1,1,1,0}, //WLLLW
	{0,0,0,1,1}, //WWWLL
	{0,1,1,0,0}, //WLLWW
	{1,1,1,1,1}  //ALL
};

#define	CPU_PORT_WAN_MASK	(1U << CPU_PORT)
#define CPU_PORT_LAN_MASK	(1U << CPU_PORT)

#define WANLANPORTS_MASK	((1U << WAN_PORT) | (1U << LAN1_PORT) | (1U << LAN2_PORT) | (1U << LAN3_PORT) | (1U << LAN4_PORT))	/* ALL WAN/LAN port bit-mask */

/* Final model-specific LAN/WAN/WANS_LAN partition definitions.
 * bit0: P0, bit1: P1, bit2: P2, bit3: P3, bit4: P4, bit5: P5
 * ^^^^^^^^ P0 is not used by LAN/WAN port.
 */
static unsigned int lan_mask = 0;	/* LAN only. Exclude WAN, WANS_LAN, and generic IPTV port. */
static unsigned int wan_mask = 0;	/* wan_type = WANS_DUALWAN_IF_WAN. Include generic IPTV port. */
static unsigned int wans_lan_mask = 0;	/* wan_type = WANS_DUALWAN_IF_LAN. */

/* RT-N56U's P0, P1, P2, P3, P4 = LAN4, LAN3, LAN2, LAN1, WAN
 * ==> Model-specific port number.
 */
static int switch_port_mapping[] = {
	LAN4_PORT,	//0000 0000 0001 LAN4
	LAN3_PORT,	//0000 0000 0010 LAN3
	LAN2_PORT,	//0000 0000 0100 LAN2
	LAN1_PORT,	//0000 0000 1000 LAN1
	WAN_PORT,	//0000 0001 0000 WAN
	P6_PORT,	//0000 0010 0000 -
	P6_PORT,	//0000 0100 0000 -
	P6_PORT,	//0000 1000 0000 -
	P6_PORT,	//0001 0000 0000 -
	CPU_PORT,	//0010 0000 0000 CPU port
};

/* Model-specific LANx ==> Model-specific PortX mapping */
const int lan_id_to_port_mapping[NR_WANLAN_PORT] = {
	WAN_PORT,
	LAN1_PORT,
	LAN2_PORT,
	LAN3_PORT,
	LAN4_PORT,
};

void reset_qca_switch(void);

/* Model-specific LANx ==> Model-specific PortX */
static inline int lan_id_to_port_nr(int id)
{
	return lan_id_to_port_mapping[id];
}

/**
 * Get WAN port mask
 * @wan_unit:	wan_unit, if negative, select WANS_DUALWAN_IF_WAN
 * @return:	port bitmask
 */
static unsigned int get_wan_port_mask(int wan_unit)
{
	int sw_mode = nvram_get_int("sw_mode");
	char nv[] = "wanXXXports_maskXXXXXX";

	if (sw_mode == SW_MODE_AP || sw_mode == SW_MODE_REPEATER)
		return 0;

	if (wan_unit <= 0 || wan_unit >= WAN_UNIT_MAX)
		strcpy(nv, "wanports_mask");
	else
		sprintf(nv, "wan%dports_mask", wan_unit);

	return nvram_get_int(nv);
}

/**
 * Get LAN port mask
 * @return:	port bitmask
 */
static unsigned int get_lan_port_mask(void)
{
	int sw_mode = nvram_get_int("sw_mode");
	unsigned int m = nvram_get_int("lanports_mask");

	if (sw_mode == SW_MODE_AP || __mediabridge_mode(sw_mode))
		m = WANLANPORTS_MASK;

	return m;
}

/**
 * Set a VLAN
 * @vid:	VLAN ID
 * @prio:	VLAN Priority
 * @mbr:	VLAN members
 * @untag:	VLAN members that need untag
 *
 * @return
 * 	0:	success
 *     -1:	invalid parameter
 */
int ipq40xx_vlan_set(int vid, int prio, int mbr, int untag)
{
	int vlan_idx, i;
	unsigned int m, u;

	if (vid > 4095 || prio > 7)
		return -1;

	doSystem("ssdk_sh vlan entry create %d", vid);
	for (i = 0, m = mbr, u = untag; i < 6; ++i, m >>= 1, u >>=1) {
		if (m & 1) {
			if (u & 1) {
				doSystem("ssdk_sh vlan member add %d %d untagged", vid, i);
				doSystem("ssdk_sh portVlan defaultCVid set %d %d", i, vid);
				doSystem("ssdk_sh qos ptDefaultCpri set %d %d", i, prio);
			}
			else {
				doSystem("ssdk_sh vlan member add %d %d tagged", vid, i);
			}
			doSystem("ssdk_sh portVlan ingress set %d secure", i);
			doSystem("ssdk_sh portVlan egress set %d untagged", i);
		}
	}

	return 0;
}

/**
 * Get link status and/or phy speed of a port.
 * @link:	pointer to unsigned integer.
 * 		If link != NULL,
 * 			*link = 0 means link-down
 * 			*link = 1 means link-up.
 * @speed:	pointer to unsigned integer.
 * 		If speed != NULL,
 * 			*speed = 1 means 100Mbps
 * 			*speed = 2 means 1000Mbps
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *  otherwise:	fail
 */
static int get_ipq40xx_port_info(unsigned int port, unsigned int *link, unsigned int *speed)
{
        FILE *fp;
	size_t rlen;
	unsigned int l = 0, s = 0;
	char buf[512], *pt;

	if (port < 1 || port > 5 || (!link && !speed))
		return -1;

	if (link)
		*link = 0;
	if (speed)
		*speed = 0;

	sprintf(buf, "ssdk_sh port linkstatus get %d", port);
	if ((fp = popen(buf, "r")) == NULL) {
		_dprintf("%s: Run [%s] fail!\n", __func__, buf);
		return -2;
	}
	rlen = fread(buf, 1, sizeof(buf), fp);
	pclose(fp);
	if (rlen <= 1)
		return -3;

	buf[rlen-1] = '\0';
	if ((pt = strstr(buf, "[Status]:")) == NULL)
	{
#if defined(RTAC82U) //workaround for MALIBU
		if(link!=NULL)
			*link=1;  //linak up
		if(speed!=NULL)
			*speed=2; //1000M
		return 0;
#endif
		return -4;
	}
	pt += 9; // strlen of "[Status]:"
	if (!strncmp(pt, "ENABLE", 6)) {
		l = 1;

		sprintf(buf, "ssdk_sh port speed get %d", port);
		if ((fp = popen(buf, "r")) == NULL) {
			_dprintf("%s: Run [%s] fail!\n", __func__, buf);
			return -5;
		}
		rlen = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (rlen <= 1)
			return -6;

		buf[rlen-1] = '\0';
		if ((pt = strstr(buf, "[speed]:")) == NULL)
			return -7;

		pt += 8; // strlen of "[speed]:"
		if (!strncmp(pt, "1000", 4))
			s = 2;
		else
			s = 1;
	}

	if (link)
		*link = l;
	if (speed)
		*speed = s;

	return 0;
}

/**
 * Get linkstatus in accordance with port bit-mask.
 * @mask:	port bit-mask.
 * 		bit0 = P0, bit1 = P1, etc.
 * @linkStatus:	link status of all ports that is defined by mask.
 * 		If one port of mask is linked-up, linkStatus is true.
 */
static void get_ipq40xx_phy_linkStatus(unsigned int mask, unsigned int *linkStatus)
{
	int i;
	unsigned int value = 0, m;

	m = mask & WANLANPORTS_MASK;
	for (i = 0; m > 0 && !value; ++i, m >>= 1) {
		if (!(m & 1))
			continue;

		get_ipq40xx_port_info(i, &value, NULL);
		value &= 0x1;
	}
	*linkStatus = value;
}

static void build_wan_lan_mask(int stb)
{
	int i, unit;
	int wanscap_lan = get_wans_dualwan() & WANSCAP_LAN;
	int wans_lanport = nvram_get_int("wans_lanport");
	int sw_mode = nvram_get_int("sw_mode");
	char prefix[8], nvram_ports[20];

	if (sw_mode == SW_MODE_AP || sw_mode == SW_MODE_REPEATER)
		wanscap_lan = 0;

	if (stb == 100 && (sw_mode == SW_MODE_AP || __mediabridge_mode(sw_mode))) {
		stb = 7;	/* Don't create WAN port. */
		f_write_string("/proc/sys/net/edma/merge_wan_into_lan", "1", 0, 0);
	}
	else
		f_write_string("/proc/sys/net/edma/merge_wan_into_lan", "0", 0, 0);

#if 0	/* TODO: no WAN port */
	if ((get_wans_dualwan() & (WANSCAP_LAN | WANSCAP_WAN)) == 0)
		stb = 7; // no WAN?
#endif

	if (wanscap_lan && (wans_lanport < 0 || wans_lanport > 4)) {
		_dprintf("%s: invalid wans_lanport %d!\n", __func__, wans_lanport);
		wanscap_lan = 0;
	}

	lan_mask = wan_mask = wans_lan_mask = 0;
	for (i = 0; i < NR_WANLAN_PORT; ++i) {
		switch (lan_wan_partition[stb][i]) {
		case 0:
			wan_mask |= 1U << lan_id_to_port_nr(i);
			break;
		case 1:
			lan_mask |= 1U << lan_id_to_port_nr(i);
			break;
		default:
			_dprintf("%s: Unknown LAN/WAN port definition. (stb %d i %d val %d)\n",
				__func__, stb, i, lan_wan_partition[stb][i]);
		}
	}

	//DUALWAN
	if (wanscap_lan) {
		wans_lan_mask = 1U << lan_id_to_port_nr(wans_lanport);
		lan_mask &= ~wans_lan_mask;
	}

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
		sprintf(prefix, "%d", unit);
		sprintf(nvram_ports, "wan%sports_mask", (unit == WAN_UNIT_FIRST)?"":prefix);

		if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
			nvram_set_int(nvram_ports, wan_mask);
		}
		else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
			nvram_set_int(nvram_ports, wans_lan_mask);
		}
		else
			nvram_unset(nvram_ports);
	}
	nvram_set_int("lanports_mask", lan_mask);
}

/**
 * Configure LAN/WAN partition base on generic IPTV type.
 * @type:
 * 	0:	Default.
 * 	1:	LAN1
 * 	2:	LAN2
 * 	3:	LAN3
 * 	4:	LAN4
 * 	5:	LAN1+LAN2
 * 	6:	LAN3+LAN4
 */
static void config_ipq40xx_LANWANPartition(int type)
{
	int wanscap_wanlan = get_wans_dualwan() & (WANSCAP_WAN | WANSCAP_LAN);
	int wanscap_lan = wanscap_wanlan & WANSCAP_LAN;
	int wans_lanport = nvram_get_int("wans_lanport");
	int sw_mode = nvram_get_int("sw_mode");

	if (sw_mode == SW_MODE_AP || sw_mode == SW_MODE_REPEATER)
		wanscap_lan = 0;

	if (wanscap_lan && (wans_lanport < 0 || wans_lanport > 4)) {
		_dprintf("%s: invalid wans_lanport %d!\n", __func__, wans_lanport);
		wanscap_lan = 0;
		wanscap_wanlan &= ~WANSCAP_LAN;
	}

	build_wan_lan_mask(type);
	_dprintf("%s: LAN/WAN/WANS_LAN portmask %08x/%08x/%08x\n", __func__, lan_mask, wan_mask, wans_lan_mask);
	reset_qca_switch();

	// LAN 
	ipq40xx_vlan_set(1, 0, (lan_mask | CPU_PORT_LAN_MASK), lan_mask);

	// WAN & DUALWAN
	if (sw_mode == SW_MODE_ROUTER) {
		switch (wanscap_wanlan) {
		case WANSCAP_WAN | WANSCAP_LAN:
			ipq40xx_vlan_set(2, 0, (wan_mask      | CPU_PORT_WAN_MASK), wan_mask);
			ipq40xx_vlan_set(3, 0, (wans_lan_mask | CPU_PORT_WAN_MASK), wans_lan_mask);
			break;
		case WANSCAP_LAN:
			ipq40xx_vlan_set(2, 0, (wans_lan_mask | CPU_PORT_WAN_MASK), wans_lan_mask);
			break;
		case WANSCAP_WAN:
			ipq40xx_vlan_set(2, 0, (wan_mask      | CPU_PORT_WAN_MASK), wan_mask);
			break;
		default:
			_dprintf("%s: Unknown WANSCAP %x\n", __func__, wanscap_wanlan);
		}
	}
}

static void get_ipq40xx_WAN_Speed(unsigned int *speed)
{
	int i, v = -1, t;
	unsigned int m;

	m = (get_wan_port_mask(0) | get_wan_port_mask(1)) & WANLANPORTS_MASK;
	for (i = 0; m; ++i, m >>= 1) {
		if (!(m & 1))
			continue;

		get_ipq40xx_port_info(i, NULL, (unsigned int*) &t);
		t &= 0x3;
		if (t > v)
			v = t;
	}

	switch (v) {
	case 0x0:
		*speed = 10;
		break;
	case 0x1:
		*speed = 100;
		break;
	case 0x2:
		*speed = 1000;
		break;
	default:
		_dprintf("%s: invalid speed!\n", __func__);
	}
}

static void link_down_up_ipq40xx_PHY(unsigned int mask, int status)
{
	int i;
	char idx[2], action[9];
	unsigned int m;

	if (!status)		//power down PHY
		sprintf(action, "poweroff");
	else
		sprintf(action, "poweron");

	mask &= WANLANPORTS_MASK;
	for (i = 0, m = mask; m; ++i, m >>= 1) {
		if (!(m & 1))
			continue;
		sprintf(idx, "%d", i);
		eval("ssdk_sh", "port", action, "set", idx);
	}
}

void set_ipq40xx_broadcast_rate(int bsr)
{
#if 0
	if ((bsr < 0) || (bsr > 255))
		return;

	if (switch_init() < 0)
		return;

	printf("set broadcast strom control rate as: %d\n", bsr);
	switch_fini();
#endif
}

void reset_qca_switch(void)
{
	eval("ssdk_sh", "vlan", "entry", "flush"); // clear
	nvram_unset("vlan_idx");

	/* TX interrupt infinity */
	f_write_string("/proc/irq/97/smp_affinity", "4", 0, 0);
	f_write_string("/proc/irq/98/smp_affinity", "4", 0, 0);
	f_write_string("/proc/irq/99/smp_affinity", "4", 0, 0);
	f_write_string("/proc/irq/100/smp_affinity", "4", 0, 0);
	f_write_string("/proc/irq/101/smp_affinity", "8", 0, 0);
	f_write_string("/proc/irq/102/smp_affinity", "8", 0, 0);
	f_write_string("/proc/irq/103/smp_affinity", "8", 0, 0);
	f_write_string("/proc/irq/104/smp_affinity", "8", 0, 0);
	f_write_string("/proc/irq/105/smp_affinity", "1", 0, 0);
	f_write_string("/proc/irq/106/smp_affinity", "1", 0, 0);
	f_write_string("/proc/irq/107/smp_affinity", "1", 0, 0);
	f_write_string("/proc/irq/108/smp_affinity", "1", 0, 0);
	f_write_string("/proc/irq/109/smp_affinity", "2", 0, 0);
	f_write_string("/proc/irq/110/smp_affinity", "2", 0, 0);
	f_write_string("/proc/irq/111/smp_affinity", "2", 0, 0);
	f_write_string("/proc/irq/112/smp_affinity", "2", 0, 0);
	/* RX interrupt infinity */
	f_write_string("/proc/irq/272/smp_affinity", "1", 0, 0);
	f_write_string("/proc/irq/274/smp_affinity", "2", 0, 0);
	f_write_string("/proc/irq/276/smp_affinity", "4", 0, 0);
	f_write_string("/proc/irq/278/smp_affinity", "8", 0, 0);
	f_write_string("/proc/irq/273/smp_affinity", "1", 0, 0);
	f_write_string("/proc/irq/275/smp_affinity", "2", 0, 0);
	f_write_string("/proc/irq/277/smp_affinity", "4", 0, 0);
	f_write_string("/proc/irq/279/smp_affinity", "8", 0, 0);
	/* XPS/RFS configuration */
	f_write_string("/sys/class/net/eth0/queues/tx-0/xps_cpus", "1", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/tx-1/xps_cpus", "2", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/tx-2/xps_cpus", "4", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/tx-3/xps_cpus", "8", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-0/rps_cpus", "1", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-1/rps_cpus", "2", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-2/rps_cpus", "4", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-3/rps_cpus", "8", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/tx-0/xps_cpus", "1", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/tx-1/xps_cpus", "2", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/tx-2/xps_cpus", "4", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/tx-3/xps_cpus", "8", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-0/rps_cpus", "1", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-1/rps_cpus", "2", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-2/rps_cpus", "4", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-3/rps_cpus", "8", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-0/rps_flow_cnt", "256", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-1/rps_flow_cnt", "256", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-2/rps_flow_cnt", "256", 0, 0);
	f_write_string("/sys/class/net/eth0/queues/rx-3/rps_flow_cnt", "256", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-0/rps_flow_cnt", "256", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-1/rps_flow_cnt", "256", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-2/rps_flow_cnt", "256", 0, 0);
	f_write_string("/sys/class/net/eth1/queues/rx-3/rps_flow_cnt", "256", 0, 0);
	f_write_string("/proc/sys/net/core/rps_sock_flow_entries", "1024", 0, 0);
}

static void set_Vlan_VID(int vid)
{
	char tmp[8];

	sprintf(tmp, "%d", vid);
	nvram_set("vlan_vid", tmp);
}

static void set_Vlan_PRIO(int prio)
{
	char tmp[2];

	sprintf(tmp, "%d", prio);
	nvram_set("vlan_prio", tmp);
}

static int convert_n56u_to_qca_bitmask(int orig)
{
	int i, bit, bitmask;
	bitmask = 0;
	for(i = 0; i < ARRAY_SIZE(switch_port_mapping); i++) {
		bit = (1 << i);
		if (orig & bit)
			bitmask |= (1 << switch_port_mapping[i]);
	}
	return bitmask;
}

/**
 * @stb_bitmask:	bitmask of STB port(s)
 * 			e.g. bit0 = P0, bit1 = P1, etc.
 */
static void initialize_Vlan(int stb_bitmask)
{
	int wans_lan_vid = 3, wanscap_wanlan = get_wans_dualwan() & (WANSCAP_WAN | WANSCAP_LAN);
	int wanscap_lan = get_wans_dualwan() & WANSCAP_LAN;
	int wans_lanport = nvram_get_int("wans_lanport");
	int sw_mode = nvram_get_int("sw_mode");

	if (sw_mode == SW_MODE_AP || sw_mode == SW_MODE_REPEATER)
		wanscap_lan = 0;

	if (wanscap_lan && (wans_lanport < 0 || wans_lanport > 4)) {
		_dprintf("%s: invalid wans_lanport %d!\n", __func__, wans_lanport);
		wanscap_lan = 0;
		wanscap_wanlan &= ~WANSCAP_LAN;
	}

	build_wan_lan_mask(0);
	stb_bitmask = convert_n56u_to_qca_bitmask(stb_bitmask);
	lan_mask &= ~stb_bitmask;
	wan_mask |= stb_bitmask;
	_dprintf("%s: LAN/WAN/WANS_LAN portmask %08x/%08x/%08x\n", __func__, lan_mask, wan_mask, wans_lan_mask);

	if(wans_lan_mask & stb_bitmask)	// avoid using a port for two functions.
	{
		wanscap_lan = 0;
		wanscap_wanlan &= ~WANSCAP_LAN;
	}

	if (wanscap_lan && (!(get_wans_dualwan() & WANSCAP_WAN)) && !(!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", "")))
	{
		wans_lan_vid = 2;
	}

	reset_qca_switch();

	// LAN
	ipq40xx_vlan_set(1, 0, (lan_mask | CPU_PORT_LAN_MASK), lan_mask);

	// DUALWAN
	if (wanscap_lan) {
		ipq40xx_vlan_set(wans_lan_vid, 0, (wans_lan_mask | CPU_PORT_WAN_MASK), wans_lan_mask);
	}
}


static int wanmii_need_vlan_tag(void)
{
	int dualwan = get_wans_dualwan();

	if(!(dualwan & WANSCAP_WAN))								// none or one port for WAN
		return 0;
	if(dualwan & WANSCAP_LAN)								// dual port for WAN
		return 1;
	if(!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", ""))		// has IPTV ISP setting (which has vlan id)
		return 1;

	return 0;
}

static int handle_wanmii_untag(int mask)
{
	if((mask & CPU_PORT_WAN_MASK) && wanmii_need_vlan_tag())
		mask &= ~CPU_PORT_WAN_MASK;

	return mask;
}

/**
 * Create VLAN for LAN and/or WAN in accordance with bitmask parameter.
 * @bitmask:
 *  bit15~bit0:		member port bitmask.
 * 	bit0:		RT-N56U port0, LAN4
 * 	bit1:		RT-N56U port1, LAN3
 * 	bit2:		RT-N56U port2, LAN2
 * 	bit3:		RT-N56U port3, LAN1
 * 	bit4:		RT-N56U port4, WAN
 * 	bit8:		RT-N56U port8, LAN_CPU port
 * 	bit9:		RT-N56U port9, WAN_CPU port
 *  bit31~bit16:	untag port bitmask.
 * 	bit16:		RT-N56U port0, LAN4
 * 	bit17:		RT-N56U port1, LAN3
 * 	bit18:		RT-N56U port2, LAN2
 * 	bit19:		RT-N56U port3, LAN1
 * 	bit20:		RT-N56U port4, WAN
 * 	bit24:		RT-N56U port8, LAN_CPU port
 * 	bit25:		RT-N56U port9, WAN_CPU port
 * First Ralink-based model is RT-N56U.
 * Convert RT-N56U-specific bitmask to physical port of your model,
 * base on relationship between physical port and visual WAN/LAN1~4 of that model first.
 */
static void create_Vlan(int bitmask)
{
	int vid = atoi(nvram_safe_get("vlan_vid")) & 0xFFF;
	int prio = atoi(nvram_safe_get("vlan_prio")) & 0x7;
	int mbr = bitmask & 0xffff;
	int untag = (bitmask >> 16) & 0xffff;
	int mbr_qca, untag_qca;

	//convert port mapping
	mbr_qca   = convert_n56u_to_qca_bitmask(mbr);
	untag_qca = convert_n56u_to_qca_bitmask(untag);
	untag_qca = handle_wanmii_untag(untag_qca);

	ipq40xx_vlan_set(vid, prio, mbr_qca, untag_qca);
}

#if 0
static void is_singtel_mio(int is)
{
}
#endif

int ipq40xx_ioctl(int val, int val2)
{
//	int value = 0;
	unsigned int value2 = 0;
	int i, max_wan_unit = 0;

#if defined(RTCONFIG_DUALWAN)
	max_wan_unit = 1;
#endif

	switch (val) {
	case 0:
		value2 = rtkswitch_wanPort_phyStatus(-1);
		printf("WAN link status : %u\n", value2);
		break;
	case 3:
		value2 = rtkswitch_lanPorts_phyStatus();
		printf("LAN link status : %u\n", value2);
		break;
	case 8:
		config_ipq40xx_LANWANPartition(val2);
		break;
	case 13:
		get_ipq40xx_WAN_Speed(&value2);
		printf("WAN speed : %u Mbps\n", value2);
		break;
	case 14: // Link up LAN ports
		link_down_up_ipq40xx_PHY(get_lan_port_mask(), 1);
		break;
	case 15: // Link down LAN ports
		link_down_up_ipq40xx_PHY(get_lan_port_mask(), 0);
		break;
	case 16: // Link up ALL ports
		link_down_up_ipq40xx_PHY(WANLANPORTS_MASK, 1);
		break;
	case 17: // Link down ALL ports
		link_down_up_ipq40xx_PHY(WANLANPORTS_MASK, 0);
		break;
	case 21:
		break;
	case 22:
		break;
	case 23:
		break;
	case 24:
		break;
#if 0
	case 25:
		set_ipq40xx_broadcast_rate(val2);
		break;
#endif
	case 27:
		reset_qca_switch();
		break;
	case 36:
		set_Vlan_VID(val2);
		break;
	case 37:
		set_Vlan_PRIO(val2);
		break;
	case 38:
		initialize_Vlan(val2);
		break;
	case 39:
		create_Vlan(val2);
		break;
#if 0
	case 40:
		is_singtel_mio(val2);
		break;
	case 50:
		fix_up_hwnat_for_wifi();
		break;
#endif
	case 100:
		break;
	case 114: // link up WAN ports
		for (i = WAN_UNIT_FIRST; i <= max_wan_unit; ++i)
			link_down_up_ipq40xx_PHY(get_wan_port_mask(i), 1);
		break;
	case 115: // link down WAN ports
		for (i = WAN_UNIT_FIRST; i <= max_wan_unit; ++i)
			link_down_up_ipq40xx_PHY(get_wan_port_mask(i), 0);
		break;
	case 200:	/* set LAN port number that is used as WAN port */
		/* Nothing to do, nvram_get_int("wans_lanport ") is enough. */
		break;
	default:
		printf("wrong ioctl cmd: %d\n", val);
	}

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

	return ipq40xx_ioctl(val, val2);
}

unsigned int
rtkswitch_wanPort_phyStatus(int wan_unit)
{
	unsigned int status = 0;

	get_ipq40xx_phy_linkStatus(get_wan_port_mask(wan_unit), &status);

	return status;
}

unsigned int
rtkswitch_lanPorts_phyStatus(void)
{
	unsigned int status = 0;

	get_ipq40xx_phy_linkStatus(get_lan_port_mask(), &status);

	return status;
}

unsigned int
rtkswitch_WanPort_phySpeed(void)
{
	unsigned int speed;

	get_ipq40xx_WAN_Speed(&speed);

	return speed;
}

int rtkswitch_WanPort_linkUp(void)
{
	eval("rtkswitch", "114");

	return 0;
}

int rtkswitch_WanPort_linkDown(void)
{
	eval("rtkswitch", "115");

	return 0;
}

int
rtkswitch_LanPort_linkUp(void)
{
	system("rtkswitch 14");

	return 0;
}

int
rtkswitch_LanPort_linkDown(void)
{
	system("rtkswitch 15");

	return 0;
}

int
rtkswitch_AllPort_linkUp(void)
{
	system("rtkswitch 16");
        
	return 0;
}

int
rtkswitch_AllPort_linkDown(void)
{
	system("rtkswitch 17");

	return 0;
}

int
rtkswitch_Reset_Storm_Control(void)
{
	system("rtkswitch 21");

	return 0;
}

void ATE_port_status(void)
{
	int i;
	char buf[32];
	phyState pS;

	for (i = 0; i < NR_WANLAN_PORT; i++) {
		pS.link[i] = 0;
		pS.speed[i] = 0;
		get_ipq40xx_port_info(lan_id_to_port_nr(i), &pS.link[i], &pS.speed[i]);
	}

	sprintf(buf, "W0=%C;L1=%C;L2=%C;L3=%C;L4=%C;",
		(pS.link[0] == 1) ? (pS.speed[0] == 2) ? 'G' : 'M': 'X',
		(pS.link[1] == 1) ? (pS.speed[1] == 2) ? 'G' : 'M': 'X',
		(pS.link[2] == 1) ? (pS.speed[2] == 2) ? 'G' : 'M': 'X',
		(pS.link[3] == 1) ? (pS.speed[3] == 2) ? 'G' : 'M': 'X',
		(pS.link[4] == 1) ? (pS.speed[4] == 2) ? 'G' : 'M': 'X');
	puts(buf);
}

#ifdef RTCONFIG_LAN4WAN_LED
int led_ctrl(void)
{
        phyState pS;
	int led,i;
        for (i = 1; i < NR_WANLAN_PORT; i++) {
                led=LED_ID_MAX;
		pS.link[i] = 0;
                pS.speed[i] = 0;
                get_ipq40xx_port_info(lan_id_to_port_nr(i), &pS.link[i], &pS.speed[i]);
		switch(i)
		{
			case 1: led=LED_LAN1;
				break;
			case 2: led=LED_LAN2;
				break;
			case 3: led=LED_LAN3;
				break;
			case 4: led=LED_LAN4;
				break;
			default: break;		
		}
		if(pS.link[i]==1)
			led_control(led, LED_ON);
		else
			led_control(led, LED_OFF);
        }

	return 1;
}
#endif	/* LAN4WAN_LED*/


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

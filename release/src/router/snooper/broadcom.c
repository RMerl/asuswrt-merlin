/*
 * Ethernet Switch IGMP Snooper
 * Copyright (C) 2014 ASUSTeK Inc.
 * All Rights Reserved.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/if_vlan.h>

#include <typedefs.h>
#include <etioctl.h>

#include "snooper.h"

#ifdef DEBUG_SWITCH
#define log_switch(fmt, args...) log_debug("%s::" fmt, "switch", ##args)
#else
#define log_switch(...) do {} while (0)
#endif
#ifdef TEST
#define logger(level, fmt, args...) printf(fmt, ##args), printf("\n")
#endif

#ifndef IOV_ET_ROBO_DEVID
#define IOV_ET_ROBO_DEVID 3
#endif

#define REG_MII_PAGE		0x10 /* MII Page register */
#define REG_MII_ADDR		0x11 /* MII Address register */
#define REG_MII_DATA0		0x18 /* MII Data register 0 */
#define REG_MII_PAGE_ENABLE	1
#define REG_MII_ADDR_WRITE	1
#define REG_MII_ADDR_READ	2

#define ROBO_PHY_ADDR		0x1e /* Robo Switch PHY Address */
#define PAGE_CTRL		0x00 /* Control registers */
#define PAGE_MMR		0x02 /* Management/Mirroring Registers */
#define PAGE_VTBL		0x05 /* ARL Access Registers */
#define PAGE_VLAN		0x34 /* VLAN Registers */
#define REG_CTRL_MCAST		0x21 /* Multicast Control */
#define REG_VTBL_CTRL		0x00 /* ARL Read/Write Control */
#define REG_VTBL_MINDX		0x02 /* MAC Address Index */
#define REG_VTBL_VINDX		0x08 /* VID Table Address Index */
#define REG_VTBL_ACCESS		0x80 /* VLAN Table Access */
#define REG_VTBL_INDX		0x81 /* VLAN Table Address Index */
#define REG_VTBL_ENTRY		0x83 /* VLAN Table Entry */
#define REG_VLAN_ACCESS		0x06 /* VLAN Table Access register */
#define REG_VLAN_READ		0x0c /* VLAN Read register */

enum {
	SWITCH_UNKNOWN,
	SWITCH_BCM5325,
	SWITCH_BCM53115,
	SWITCH_BCM53125,
	SWITCH_BCM5301x,
} model;
static int fd;
static struct ifreq ifr;
static int vlan;
static int cpu_portmap;
static int lan_portmap;

static int robo_ioctl(int write, int page, int reg, uint16_t *value, int count)
{
	static const int __ioctl_args[2] = { SIOCGETCROBORD, SIOCSETCROBOWR };
	int ret, vecarg[5];
	uint16_t *valp;

	memset(&vecarg, 0, sizeof(vecarg));
	vecarg[0] = (page << 16) | reg;
	if (model == SWITCH_BCM5301x) {
		vecarg[1] = count * 2;
		valp = (uint16_t *) &vecarg[2];
	} else
		valp = (uint16_t *) &vecarg[1];

	if (write)
		memcpy(valp, value, count * 2);
	ifr.ifr_data = (caddr_t) vecarg;
	ret = ioctl(fd, __ioctl_args[write], (caddr_t) &ifr);
	if (ret < 0)
		log_switch("ioctl: %s", strerror(errno));
	else if (!write)
		memcpy(value, valp, count * 2);

	return ret;
}

static int phy_ioctl(int write, int phy, int reg, uint16_t *value)
{
	static const int __ioctl_args[2] = { SIOCGETCPHYRD2, SIOCSETCPHYWR2 };
	int ret, vecarg[2];

	if (model == SWITCH_BCM5301x)
		return robo_ioctl(write, 0x10 + phy, reg, value, 1);

	memset(&vecarg, 0, sizeof(vecarg));
	vecarg[0] = (phy << 16) | reg;
	if (write)
		vecarg[1] = *value;
	ifr.ifr_data = (caddr_t) vecarg;
	ret = ioctl(fd, __ioctl_args[write], (caddr_t) &ifr);
	if (ret < 0)
		log_switch("ioctl: %s", strerror(errno));
	else if (!write)
		*value = vecarg[1];

	return ret;
}

static int phy_select(int write, int page, int reg, int op)
{
	uint16_t value[2];
	int i = 5;

	if (write) {
		value[0] = (page << 8) | REG_MII_PAGE_ENABLE;
		value[1] = (reg << 8) | op;
	}
	if (phy_ioctl(write, ROBO_PHY_ADDR, REG_MII_PAGE, &value[0]) < 0 ||
	    phy_ioctl(write, ROBO_PHY_ADDR, REG_MII_ADDR, &value[1]) < 0)
		return -1;
	if (!write)
		return ((value[0] >> 8) == page && (value[1] >> 8) == reg);
	while (i--) {
		if (phy_ioctl(0, ROBO_PHY_ADDR, REG_MII_ADDR, &value[1]) < 0)
			return -1;
		if ((value[1] & 0x03) == 0)
			return 0;
		usleep(100);
	}

	return -1;
}

static int robo_read(int page, int reg, uint16_t *value, int count)
{
	int i;

	if (model == SWITCH_BCM5301x)
		return robo_ioctl(0, page, reg, value, count);

retry:
	if (phy_select(1, page, reg, REG_MII_ADDR_READ) < 0)
		return -1;
	for (i = 0; i < count; i++) {
		if (phy_select(0, page, reg, REG_MII_ADDR_READ) <= 0)
			goto retry;
		if (phy_ioctl(0, ROBO_PHY_ADDR, REG_MII_DATA0 + i, &value[i]) < 0)
			return -1;
	}

	return 0;
}

static inline uint16_t robo_read16(int page, int reg)
{
	uint16_t value = 0;
	robo_read(page, reg, &value, 1);
	return value;
}

static inline uint32_t robo_read32(int page, int reg)
{
	uint32_t value = 0;
	robo_read(page, reg, (uint16_t *) &value, 2);
	return value;
}

static int robo_write(int page, int reg, uint16_t *value, int count)
{
	int i;

	if (model == SWITCH_BCM5301x)
		return robo_ioctl(1, page, reg, value, count);

	for (i = 0; i < count; i++) {
		if (phy_ioctl(1, ROBO_PHY_ADDR, REG_MII_DATA0 + i, &value[i]) < 0)
			return -1;
	}
	return phy_select(1, page, reg, REG_MII_ADDR_WRITE);
}

static inline int robo_write16(int page, int reg, uint16_t value)
{
	return robo_write(page, reg, &value, 1);
}

static inline int robo_write32(int page, int reg, uint32_t value)
{
	return robo_write(page, reg, (uint16_t *) &value, 2);
}

static void ether_etohw(unsigned char *ea, uint8_t *hwaddr)
{
	hwaddr[0] = ea[5];
	hwaddr[1] = ea[4];
	hwaddr[2] = ea[3];
	hwaddr[3] = ea[2];
	hwaddr[4] = ea[1];
	hwaddr[5] = ea[0];
}

static int get_model(void)
{
	et_var_t var;
	uint32_t devid = 0;

	memset(&var, 0, sizeof(var));
	var.set = 0;
	var.cmd = IOV_ET_ROBO_DEVID;
	var.buf = &devid;
	var.len = sizeof(devid);

	ifr.ifr_data = (caddr_t) &var;
	if (ioctl(fd, SIOCSETGETVAR, (caddr_t) &ifr) < 0) {
		if (robo_read(PAGE_MMR, 0x30, (uint16_t *) &devid, 2) < 0)
			goto error;
		if (devid == 0)
			devid = 0x25;
	}

	log_switch("%-5s devid %08x", "read", devid);

	if (devid == 0x25)
		return SWITCH_BCM5325;
	else if (devid == 0x3115 || devid == 0x53115)
		return SWITCH_BCM53115;
	else if (devid == 0x3125 || devid == 0x53125)
		return SWITCH_BCM53125;
	else if ((devid & 0xfffffff0) == 0x53010)
		return SWITCH_BCM5301x;

error:
	return SWITCH_UNKNOWN;
}

static int read_entry(void *hwaddr, int vlan, uint16_t entry[6], int *index)
{
	int i, valid, portmap, vid;

	vlan &= (model == SWITCH_BCM5325) ? 0x0f : 0x0fff;

	robo_write(PAGE_VTBL, REG_VTBL_MINDX, hwaddr, 3);
	robo_write16(PAGE_VTBL, REG_VTBL_VINDX, vlan);

	robo_read(PAGE_VTBL, REG_VTBL_MINDX, &entry[0], 4);
	robo_read(PAGE_VTBL, REG_VTBL_VINDX, &entry[4], 2);
	robo_write16(PAGE_VTBL, REG_VTBL_CTRL, 0x81);

	for (i = 0; i < (model == SWITCH_BCM5325 ? 2 : 4); i++) {
		switch (model) {
		case SWITCH_BCM5325:
			robo_read(PAGE_VTBL, 0x10 + 0x08 * i, &entry[0], 4);
			robo_read(PAGE_VTBL, 0x30 + 0x02 * i, &entry[4], 1);
			valid = entry[3] & 0x8000;
			portmap = entry[3] & 0x7f;
			vid = entry[4] & 0x0f;
			break;
		case SWITCH_BCM53115:
		case SWITCH_BCM53125:
		case SWITCH_BCM5301x:
			robo_read(PAGE_VTBL, 0x10 + 0x10 * i, &entry[0], 4);
			robo_read(PAGE_VTBL, 0x18 + 0x10 * i, &entry[4], 2);
			valid = entry[5] & 0x1;
			portmap = entry[4] & 0x1ff;
			vid = entry[3] & 0x0fff;
			break;
		default:
			return -1;
		}
		if (valid && memcmp(entry, hwaddr, ETHER_ADDR_LEN) == 0 && vid == vlan) {
			*index = i;
			return portmap;
		} else if (!valid)
			*index = i;
	}

	memset(entry, 0, sizeof(entry));

	return -1;
}

static int write_entry(void *hwaddr, int vlan, uint16_t entry[6], int index, int portmap)
{
	memcpy(entry, hwaddr, ETHER_ADDR_LEN);
	switch (model) {
	case SWITCH_BCM5325:
		entry[4] = vlan & 0x0f;
		if (portmap >= 0) {
			entry[3] &= ~0x7f;
			entry[3] |= 0xc000 | (portmap & 0x7f);
		} else
			entry[3] &= ~0x8000;
		portmap = entry[3] & 0x7f;
		robo_write(PAGE_VTBL, 0x10 + 0x08 * index, &entry[0], 4);
		robo_write(PAGE_VTBL, 0x30 + 0x02 * index, &entry[4], 1);
		break;
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
	case SWITCH_BCM5301x:
		entry[3] = vlan & 0x0fff;
		if (portmap >= 0) {
			entry[4] &= ~0x1ff;
			entry[4] |= 0x8000 | (portmap & 0x1ff);
			entry[5] |= 0x01;
		} else
			entry[5] &= ~0x01;
		portmap = entry[4] & 0x1ff;
		robo_write(PAGE_VTBL, 0x10 + 0x10 * index, &entry[0], 4);
		robo_write(PAGE_VTBL, 0x18 + 0x10 * index, &entry[4], 2);
		break;
	default:
		return -1;
	}
	robo_write16(PAGE_VTBL, REG_VTBL_CTRL, 0x80);

	return portmap;
}

static int read_portmap(int vlan)
{
	uint32_t entry;
	int i, portmap = 0;

	switch (model) {
	case SWITCH_BCM5325:
		for (i = vlan; i < vlan + 16; i++) {
			robo_write16(PAGE_VLAN, REG_VLAN_ACCESS,
			    0x2000 | (vlan & 0x0ff0) | (i & 0x0f));
			entry = robo_read32(PAGE_VLAN, REG_VLAN_READ);
			if (((entry & 0x1000000)) &&
			    ((entry & 0x0fff000) >> 12) == (vlan & 0x0fff)) {
				portmap = entry & 0x3f;
				break;
			}
		}
		break;
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
	case SWITCH_BCM5301x:
		robo_write16(PAGE_VTBL, REG_VTBL_INDX, vlan & 0x0fff);
		robo_write16(PAGE_VTBL, REG_VTBL_ACCESS, 0x81);
		entry = robo_read32(PAGE_VTBL, REG_VTBL_ENTRY);
		portmap = entry & 0x1ff;
		break;
	default:
		return -1;
	}

	return portmap;
}

int switch_init(char *ifname, int vid, int cputrap)
{
	struct vlan_ioctl_args ifv;
	uint8_t hwaddr[ETHER_ADDR_LEN];
	uint16_t entry[6];
	int esw_portmap, index;

	fd = open_socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	memset(&ifv, 0, sizeof(ifv));
	strncpy(ifv.device1, ifname, sizeof(ifv.device1));
	ifv.cmd = GET_VLAN_REALDEV_NAME_CMD;
	if (ioctl(fd, SIOCGIFVLAN, &ifv) >= 0) {
		strncpy(ifr.ifr_name, ifv.u.device2, sizeof(ifr.ifr_name));
		ifv.cmd = GET_VLAN_VID_CMD;
		if (ioctl(fd, SIOCGIFVLAN, &ifv) >= 0)
			vlan = ifv.u.VID;
	} else if (sscanf(ifname, "vlan%u", &vlan) == 1) {
		strncpy(ifr.ifr_name, "eth0", sizeof(ifr.ifr_name));
	} else if (sscanf(ifname, "%16[^.].%u", ifr.ifr_name, &vlan) != 2) {
		strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		vlan = (vid < 0) ? 0 : vid;
	}

	model = get_model();
	switch (model) {
	case SWITCH_BCM5325:
		esw_portmap = 0x7f;
		break;
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
	case SWITCH_BCM5301x:
		esw_portmap = 0x1ff;
		break;
	default:
		log_error("unsupported switch: %s", strerror(errno));
		goto error;
	}

	lan_portmap = read_portmap(vlan);
	log_switch("%-5s map@vlan%u = " FMT_PORTS, "read",
	    vlan, ARG_PORTS(lan_portmap));

	ether_etohw(ifhwaddr, hwaddr);
	cpu_portmap = read_entry(hwaddr, vlan, entry, &index);
	if (cpu_portmap < 0)
		goto error;
	if (model == SWITCH_BCM5325 && cpu_portmap == 8)
		cpu_portmap = 5;
	cpu_portmap = (1 << cpu_portmap);
	log_switch("%-5s cpu@vlan%u = " FMT_PORTS, "init",
	    vlan, ARG_PORTS(cpu_portmap));

	lan_portmap &= esw_portmap & ~cpu_portmap;
	log_switch("%-5s lan@vlan%u = " FMT_PORTS, "init",
	    vlan, ARG_PORTS(lan_portmap));

	if (model == SWITCH_BCM5325)
		robo_write16(PAGE_CTRL, REG_CTRL_MCAST, 1);

	return 0;

error:
	if (fd >= 0)
		close(fd);
	return -1;
}

void switch_done(void)
{
	if (fd < 0)
		return;

	if (model == SWITCH_BCM5325)
		robo_write16(PAGE_CTRL, REG_CTRL_MCAST, 0);

	close(fd);
}

int switch_get_port(unsigned char *haddr)
{
	uint8_t hwaddr[ETHER_ADDR_LEN];
	uint16_t entry[6];
	int value, index;

	ether_etohw(haddr, hwaddr);

	value = read_entry(hwaddr, vlan, entry, &index);
	if (model == SWITCH_BCM5325 && value == 8)
		value = 5;

	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(haddr), ARG_PORTS(1 << value));

	return value;
}

int switch_add_portmap(unsigned char *maddr, int portmap)
{
	uint8_t hwaddr[ETHER_ADDR_LEN];
	uint16_t entry[6];
	int value, index = 0;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	ether_etohw(maddr, hwaddr);

	value = read_entry(hwaddr, vlan, entry, &index);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value < 0)
		value = 0;
	portmap = (value | portmap) | cpu_portmap;
	if (portmap != value) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(portmap));
		write_entry(hwaddr, vlan, entry, index, portmap);
	}

	return portmap;
}

int switch_del_portmap(unsigned char *maddr, int portmap)
{
	uint8_t hwaddr[ETHER_ADDR_LEN];
	uint16_t entry[6];
	int value, index = 0;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	ether_etohw(maddr, hwaddr);

	value = read_entry(hwaddr, vlan, entry, &index);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value < 0)
		value = 0;
	portmap = (value & ~portmap) | cpu_portmap;
	if (portmap != value) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(portmap));
		write_entry(hwaddr, vlan, entry, index, portmap);
	}

	return portmap;
}

int switch_clr_portmap(unsigned char *maddr)
{
	uint8_t hwaddr[ETHER_ADDR_LEN];
	uint16_t entry[6];
	int value, index = 0;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	ether_etohw(maddr, hwaddr);

	value = read_entry(hwaddr, vlan, entry, &index);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value >= 0) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "clear",
		    ARG_EA(maddr), ARG_PORTS(value));
		write_entry(hwaddr, vlan, entry, index, -1);
	}

	return value;
}

int switch_set_floodmap(unsigned char *raddr, int portmap)
{
	return -1;
}

int switch_clr_floodmap(unsigned char *raddr)
{
	return -1;
}

#ifdef TEST
int main(int argc, char *argv[])
{
	uint16_t value[4] = { 0 };
	int i, write, page, reg, count;

	if (!argv[1] || !argv[2] || !argv[3])
		return 0;

	write = (strncmp(argv[1], "robowr", 6) == 0 ? 1 :
		 strncmp(argv[1], "robord", 6) == 0 ? 0 : -1);
	if (write < 0 || (write && !argv[4]))
		return 0;
	count = atoi(argv[1] + 6);
	count = count ? count / 16 : 0;
	count = count ? : 1;

	if (switch_init("eth0", 0, 0) < 0) {
		perror("switch");
		return -1;
	}

	page = strtoul(argv[2], NULL, 0);
	reg = strtoul(argv[3], NULL, 0);

	if (write) {
		unsigned long long val = strtoull(argv[4], NULL, 0);
		memcpy(value, &val, count * 2);
		robo_write(page, reg, value, count);
	}

	robo_read(page, reg, value, count);
	printf("page %02x reg %02x = ", page, reg);
	for (i = 0; i < count * 2; i++)
		printf("%02x", ((unsigned char *) value)[i]);
	printf("\n");

	switch_done();

	return 0;
}
#endif

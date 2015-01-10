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
#define logger(level, fmt, args...) printf(fmt, ##args)
#endif

#define REG_MII_PAGE		0x10 /* MII Page register */
#define REG_MII_ADDR		0x11 /* MII Address register */
#define REG_MII_DATA0		0x18 /* MII Data register 0 */
#define REG_MII_PAGE_ENABLE	1
#define REG_MII_ADDR_WRITE	1
#define REG_MII_ADDR_READ	2

#define ROBO_PHY_ADDR		0x1e /* robo switch phy address */
#define ROBO_CTRL_PAGE		0x00 /* Control registers */
#define ROBO_IP_MULTICAST_CTRL	0x21 /* IP Multicast control */
#define ROBO_ARLIO_PAGE		0x05 /* ARL Access Registers */
#define ROBO_ARL_RW_CTRL	0x00 /* ARL Read/Write Control */
#define ROBO_ARL_MAC_ADDR_IDX	0x02 /* MAC Address Index */
#define ROBO_ARL_VID_TABLE_IDX	0x08 /* VID Table Address Index */

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
static int cpu_port;

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
	if (!write)
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
	if (!write)
		*value = vecarg[1];

	return ret;
}

static int phy_select(int page, int reg, int op)
{
	uint16_t value;
	int i = 3;

	value = (page << 8) | REG_MII_PAGE_ENABLE;
	phy_ioctl(1, ROBO_PHY_ADDR, REG_MII_PAGE, &value);
	value = (reg << 8) | op;
	phy_ioctl(1, ROBO_PHY_ADDR, REG_MII_ADDR, &value);
	while (i--) {
		if (phy_ioctl(0, ROBO_PHY_ADDR, REG_MII_ADDR, &value) == 0 && (value & 3) == 0)
			return 0;
	}

	return -1;
}

static int robo_read(int page, int reg, uint16_t *value, int count)
{
	int i, ret;

	if (model == SWITCH_BCM5301x)
		return robo_ioctl(0, page, reg, value, count);

	ret = phy_select(page, reg, REG_MII_ADDR_READ);
	for (i = 0; i < count; i++)
		phy_ioctl(0, ROBO_PHY_ADDR, REG_MII_DATA0 + i, &value[i]);

	return ret;
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

	for (i = 0; i < count; i++)
		phy_ioctl(1, ROBO_PHY_ADDR, REG_MII_DATA0 + i, &value[i]);
	return phy_select(page, reg, REG_MII_ADDR_WRITE);
}

static inline int robo_write16(int page, int reg, uint16_t value)
{
	return robo_write(page, reg, &value, 1);
}

static inline int robo_write32(int page, int reg, uint32_t value)
{
	return robo_write(page, reg, (uint16_t *) &value, 2);
}

static int get_model(void)
{
	et_var_t var;
	int ret, devid = 0;

	memset(&var, 0, sizeof(var));
	var.set = 0;
	var.cmd = IOV_ET_ROBO_DEVID;
	var.buf = &devid;
	var.len = sizeof(devid);

	ifr.ifr_data = (caddr_t) &var;
	ret = ioctl(fd, SIOCSETGETVAR, (caddr_t)&ifr);
	if (ret < 0)
		devid = robo_read16(0x02, 0x30);

	if (devid == 0x25)
		return SWITCH_BCM5325;
	else if (devid == 0x3115)
		return SWITCH_BCM53115;
	else if (devid == 0x3125)
		return SWITCH_BCM53125;
	else if ((devid & 0xfffffff0) == 0x53010)
		return SWITCH_BCM5301x;

	return SWITCH_UNKNOWN;
}

static int read_entry(uint16_t *hwaddr, int vlan, uint16_t entry[6], int *index)
{
	int i, valid, portmap, vid;

	vlan &= (model == SWITCH_BCM5325) ? 0x0f : 0x0fff;

	robo_write(ROBO_ARLIO_PAGE, ROBO_ARL_MAC_ADDR_IDX, hwaddr, 3);
	robo_write16(ROBO_ARLIO_PAGE, ROBO_ARL_VID_TABLE_IDX, vlan);

	robo_read(ROBO_ARLIO_PAGE, ROBO_ARL_MAC_ADDR_IDX, &entry[0], 4);
	robo_read(ROBO_ARLIO_PAGE, ROBO_ARL_VID_TABLE_IDX, &entry[4], 2);
	robo_write16(ROBO_ARLIO_PAGE, ROBO_ARL_RW_CTRL, 0x81);

	for (i = 0; i < (model == SWITCH_BCM5325 ? 2 : 4); i++) {
		switch (model) {
		case SWITCH_BCM5325:
			robo_read(ROBO_ARLIO_PAGE, 0x10 + 0x08 * i, &entry[0], 4);
			robo_read(ROBO_ARLIO_PAGE, 0x30 + 0x02 * i, &entry[4], 1);
			valid = entry[3] & 0x8000;
			portmap = entry[3] & 0x7f;
			vid = entry[4] & 0x0f;
			break;
		case SWITCH_BCM53115:
		case SWITCH_BCM53125:
		case SWITCH_BCM5301x:
			robo_read(ROBO_ARLIO_PAGE, 0x10 + 0x10 * i, &entry[0], 4);
			robo_read(ROBO_ARLIO_PAGE, 0x18 + 0x10 * i, &entry[4], 2);
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

static int write_entry(uint16_t hwaddr[3], int vid, uint16_t entry[6], int index, int portmap)
{
	memcpy(entry, hwaddr, ETHER_ADDR_LEN);
	switch (model) {
	case SWITCH_BCM5325:
		entry[4] = vid & 0x0f;
		if (portmap >= 0) {
			entry[3] &= ~0x7f;
			entry[3] |= 0xc000 | (portmap & 0x7f);
		} else
			entry[3] &= ~0x8000;
		portmap = entry[3] & 0x7f;
		robo_write(ROBO_ARLIO_PAGE, 0x10 + 0x08 * index, &entry[0], 4);
		robo_write(ROBO_ARLIO_PAGE, 0x30 + 0x02 * index, &entry[4], 1);
		break;
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
	case SWITCH_BCM5301x:
		entry[3] = vid & 0x0fff;
		if (portmap >= 0) {
			entry[4] &= ~0x1ff;
			entry[4] |= 0x8000 | (portmap & 0x1ff);
			entry[5] |= 0x01;
		} else
			entry[5] &= ~0x01;
		portmap = entry[4] & 0x1ff;
		robo_write(ROBO_ARLIO_PAGE, 0x10 + 0x10 * index, &entry[0], 4);
		robo_write(ROBO_ARLIO_PAGE, 0x18 + 0x10 * index, &entry[4], 2);
		break;
	default:
		return -1;
	}
	robo_write16(ROBO_ARLIO_PAGE, ROBO_ARL_RW_CTRL, 0x80);

	return portmap;
}

int switch_init(char *ifname)
{
	struct vlan_ioctl_args ifv;

#ifdef SOCK_CLOEXEC
	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0) {
		log_error("socket: %s", strerror(errno));
		return -1;
	}
#else
	int value;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		log_error("socket: %s", strerror(errno));
		return -1;
	}

	value = fcntl(fd, F_GETFD, 0);
	if (value < 0 || fcntl(fd, F_SETFD, value | FD_CLOEXEC) < 0) {
		log_error("fcntl::FD_CLOEXEC: %s", strerror(errno));
		goto error;
	}

	value = fcntl(fd, F_GETFL, 0);
	if (value < 0 || fcntl(fd, F_SETFL, value | O_NONBLOCK) < 0) {
		log_error("fcntl::O_NONBLOCK: %s", strerror(errno));
		goto error;
	}
#endif

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
		vlan = 0;
	}

	model = get_model();
	switch (model) {
	case SWITCH_BCM5325:
		robo_write16(ROBO_CTRL_PAGE, ROBO_IP_MULTICAST_CTRL, 1);
		cpu_port = 5;
		break;
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
	case SWITCH_BCM5301x:
		cpu_port = 8;
		break;
	default:
		log_error("unsupported switch: %s", strerror(errno));
		goto error;
	}

	return 0;

error:
	if (fd >= 0)
		close(fd);
	return -1;
}

void switch_done(void)
{
	if (fd >= 0)
		close(fd);
}

int switch_get_port(unsigned char *haddr)
{
	uint16_t hwaddr[3], entry[6];
	int value, index = 0;

	hwaddr[2] = (haddr[0] << 8) | haddr[1];
	hwaddr[1] = (haddr[2] << 8) | haddr[3];
	hwaddr[0] = (haddr[4] << 8) | haddr[5];

	value = read_entry(hwaddr, vlan, entry, &index);
	if (model == SWITCH_BCM5325 && value == 8)
		value = cpu_port;

	log_switch("%-5s [" FMT_EA "] = 0x%x", "read",
	    ARG_EA(haddr), value);

	return value;
}

int switch_add_portmap(unsigned char *maddr, int portmap)
{
	uint16_t hwaddr[3], entry[6];
	int value, index = 0;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	hwaddr[2] = (maddr[0] << 8) | maddr[1];
	hwaddr[1] = (maddr[2] << 8) | maddr[3];
	hwaddr[0] = (maddr[4] << 8) | maddr[5];

	value = read_entry(hwaddr, vlan, entry, &index);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value < 0)
		value = 0;
	portmap = (value | portmap) | (1 << cpu_port);
	if (portmap != value) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(portmap));
		write_entry(hwaddr, vlan, entry, index, portmap);
	}

	return portmap;
}

int switch_del_portmap(unsigned char *maddr, int portmap)
{
	uint16_t hwaddr[3], entry[6];
	int value, index = 0;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	hwaddr[2] = (maddr[0] << 8) | maddr[1];
	hwaddr[1] = (maddr[2] << 8) | maddr[3];
	hwaddr[0] = (maddr[4] << 8) | maddr[5];

	value = read_entry(hwaddr, vlan, entry, &index);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value < 0)
		value = 0;
	portmap = (value & ~portmap) | (1 << cpu_port);
	if (portmap != value) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(portmap));
		write_entry(hwaddr, vlan, entry, index, portmap);
	}

	return portmap;
}

int switch_clr_portmap(unsigned char *maddr)
{
	uint16_t hwaddr[3], entry[6];
	int value, index = 0;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	hwaddr[2] = (maddr[0] << 8) | maddr[1];
	hwaddr[1] = (maddr[2] << 8) | maddr[3];
	hwaddr[0] = (maddr[4] << 8) | maddr[5];

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

	if (switch_init("eth0") < 0) {
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

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
#include <asm/byteorder.h>

#include "snooper.h"

#ifdef DEBUG_SWITCH
#define log_switch(fmt, args...) log_debug("%s::" fmt, "switch", ##args)
#else
#define log_switch(...) do {} while (0)
#endif
#ifdef TEST
#define logger(level, fmt, args...) printf(fmt, ##args), printf("\n")
#endif

#define RAETH_ESW_REG_READ		0x89F1
#define RAETH_ESW_REG_WRITE		0x89F2
#define RAETH_MII_READ			0x89F3
#define RAETH_MII_WRITE			0x89F4

#define REG_ESW_PFC1			0x14
#define REG_ESW_TABLE_SEARCH		0x24
#define REG_ESW_TABLE_STATUS0		0x28
#define REG_ESW_TABLE_STATUS1		0x2C
#define REG_ESW_TABLE_STATUS2		0x30
#define REG_ESW_TABLE_SEARCH		0x24
#define REG_ESW_TABLE_SEARCH0		0x28
#define REG_ESW_TABLE_SEARCH1		0x2C
#define REG_ESW_TABLE_SEARCH2		0x30
#define REG_ESW_WT_MAC_AD0		0x34
#define REG_ESW_WT_MAC_AD1		0x38
#define REG_ESW_WT_MAC_AD2		0x3C
#define REG_ESW_VLAN_ID_BASE		0x50
#define REG_ESW_VLAN_MEMB_BASE		0x70
#define REG_ESW_SOCPC			0x8C

#define REG_ESW_PFC			0x04
#define REG_ESW_MFC			0x10
#define REG_ESW_IMC			0x1C
#define REG_ESW_WT_MAC_ATA1		0x74
#define REG_ESW_WT_MAC_ATA2		0x78
#define REG_ESW_WT_MAC_ATWD		0x7C
#define REG_ESW_WT_MAC_ATC		0x80
#define REG_ESW_TABLE_TSRA1		0x84
#define REG_ESW_TABLE_TSRA2		0x88
#define REG_ESW_TABLE_ATRD		0x8C
#define REG_ESW_VLAN_VTCR		0x90
#define REG_ESW_VLAN_VAWD1		0x94
#define REG_ESW_VLAN_VAWD2		0x98
#define REG_ESW_VLAN_VTIM_BASE		0x100
#define REG_ESW_PORT_PIC0		0x2008

typedef struct rt3052_esw_reg {
	unsigned int off;
	unsigned int val;
} esw_reg;

typedef struct ralink_mii_ioctl_data {
	uint32_t phy_id;
	uint32_t reg_num;
	uint32_t val_in;
	uint32_t val_out;
	uint32_t port_num;
	uint32_t dev_addr;
	uint32_t reg_addr;
} ra_mii_ioctl_data;

enum {
	SWITCH_UNKNOWN,
	SWITCH_RT3x5x,
	SWITCH_RT5350,
	SWITCH_MT7620,
	SWITCH_MT7621,
	SWITCH_MT7628,
} model;
static int fd;
static struct ifreq ifr;
static int vlan;
static int cpu_portmap;
static int lan_portmap;
static int cpu_forward;

static int esw_ioctl(int write, int offset, uint32_t *value)
{
	static const int __ioctl_args[2] = { RAETH_ESW_REG_READ, RAETH_ESW_REG_WRITE };
	esw_reg reg;
	int ret;

	memset(&reg, 0, sizeof(reg));
	reg.off = offset;
	if (write)
		reg.val = *value;
	ifr.ifr_data = (caddr_t) &reg;
	ret = ioctl(fd, __ioctl_args[write], (caddr_t) &ifr);
	if (ret < 0)
		log_switch("ioctl: %s", strerror(errno));
	else if (!write)
		*value = reg.val;

	return ret;
}

static int mii_ioctl(int write, int phy, int reg, uint32_t *value)
{
	static const int __ioctl_args[2] = { RAETH_MII_READ, RAETH_MII_WRITE };
	ra_mii_ioctl_data mii;
	int ret;

	memset(&mii, 0, sizeof(mii));
	mii.phy_id = phy;
	mii.reg_num = reg;
	if (write)
		mii.val_in = *value;
	ifr.ifr_data = (caddr_t) &mii;
	ret = ioctl(fd, __ioctl_args[write], (caddr_t) &ifr);
	if (ret < 0)
		log_switch("ioctl: %s", strerror(errno));
	else if (!write)
		*value = mii.val_out;

	return ret;
}

static int reg_read(int offset, void *value, int count)
{
	uint32_t val32;
	int ret;

	switch (model) {
	case SWITCH_RT3x5x:
	case SWITCH_RT5350:
	case SWITCH_MT7620:
	case SWITCH_MT7628:
		ret = esw_ioctl(0, offset, &val32);
		break;
	case SWITCH_MT7621:
		ret = mii_ioctl(0, 0x1f, offset, &val32);
		break;
	default:
		return -1;
	}

	if (ret == 0)
		memcpy(value, &val32, count);

	return ret;
}

static inline uint8_t reg_read8(int offset)
{
	uint8_t value = 0;
	reg_read(offset, &value, sizeof(value));
	return value;
}

static inline uint16_t reg_read16(int offset)
{
	uint16_t value = 0;
	reg_read(offset, &value, sizeof(value));
	return value;
}

static inline uint32_t reg_read32(int offset)
{
	uint32_t value = 0;
	reg_read(offset, &value, sizeof(value));
	return value;
}

static int reg_write(int offset, void *value, int count)
{
	uint32_t val32 = 0;

	memcpy(&val32, value, count);

	switch (model) {
	case SWITCH_RT3x5x:
	case SWITCH_RT5350:
	case SWITCH_MT7620:
	case SWITCH_MT7628:
		return esw_ioctl(1, offset, &val32);
	case SWITCH_MT7621:
		return mii_ioctl(1, 0x1f, offset, &val32);
	default:
		return -1;
	}
}

static inline int reg_write8(int offset, uint8_t value)
{
	return reg_write(offset, &value, sizeof(value));
}

static inline int reg_write16(int offset, uint16_t value)
{
	return reg_write(offset, &value, sizeof(value));
}

static inline int reg_write32(int offset, uint32_t value)
{
	return reg_write(offset, &value, sizeof(value));
}

static int reg_wait32(int offset, uint32_t mask, uint32_t match)
{
	uint32_t value;
	int ret, retry;

	for (retry = 0; retry < 20; retry++) {
		ret = reg_read(offset, &value, 4);
		if (ret < 0)
			return ret;
		if ((value & mask) == match)
			return 0;
		usleep(500);
	}

	return 1;
}

static void ether_etohw(unsigned char *ea, uint32_t *hwaddr)
{
	memcpy(&hwaddr[0], ea, ETHER_ADDR_LEN);
	hwaddr[0] = __cpu_to_be32(hwaddr[0]);
	hwaddr[1] = __cpu_to_be16(hwaddr[1]);
}

static int get_model(void)
{
	static const struct {
		const char *name;
		int model;
	} models[] = {
		{ "RT3050", SWITCH_RT3x5x }, /* 0x00c30800 */
		{ "RT3052", SWITCH_RT3x5x }, /* 0x00c30800 */
		{ "RT3350", SWITCH_RT3x5x }, /* 0x00c30800 */
		{ "RT3352", SWITCH_RT3x5x }, /* 0x00c30800 */
		{ "RT5350", SWITCH_RT5350 }, /* 0x00c30817 */
		{ "MT7620", SWITCH_MT7620 }, /* 0x03a2940d */
		{ "MT7621", SWITCH_MT7621 }, /* 0x03a29412 */
		{ "MT7628", SWITCH_MT7628 }  /* 0x03a29410 */
	};
	uint32_t value, phyid;
	char sysid[64];
	FILE *fp;
	int i;

	if (mii_ioctl(0, 0, 2, &phyid) < 0 ||
	    mii_ioctl(0, 0, 3, &value) < 0)
		goto error;
	phyid = (phyid << 16) | (value & 0xffff);

	log_switch("%-5s phyid %08x", "read", phyid);
	if (phyid == 0)
		goto error;

	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL)
		goto error;
	for (*sysid = '\0'; !feof(fp); *sysid = '\0') {
		if (fscanf(fp, "system type : %63[^\n]", sysid) == 1)
			break;
		fgets(sysid, sizeof(sysid), fp);
	}
	fclose(fp);

	log_switch("%-5s sysid '%s'", "read", sysid);

	for (i = sizeof(models)/sizeof(models[0]); i-- > 0;) {
		if (strstr(sysid, models[i].name) != NULL)
			return models[i].model;
	}

error:
	return SWITCH_UNKNOWN;
}

static int read_entry(uint32_t hwaddr[2], int vlan, int entry)
{
	uint32_t value, mac[2], search;
	int i, retry, valid, portmap, vid;

	switch (model) {
	case SWITCH_RT3x5x:
	case SWITCH_RT5350:
	case SWITCH_MT7628:
		reg_write32(REG_ESW_TABLE_SEARCH, 0x1);
		for (i = 0; i < 0x400; i++) {
			for (retry = 0; retry < 20; retry++) {
				usleep(500);
				if (reg_read(REG_ESW_TABLE_SEARCH0, &value, 4) < 0)
					return -1;
				if (value & 0x01) {
					valid = value & 0x70;
					portmap = (value >> 12) & 0x7f;
					vid = (value >> 7) & 0x0f;
					if (valid && vid == vlan) {
						if (reg_read(REG_ESW_TABLE_SEARCH2, &mac[0], 4) < 0 ||
						    reg_read(REG_ESW_TABLE_SEARCH1, &mac[1], 4) < 0)
							return -1;
						if (mac[0] == hwaddr[0] && mac[1] == hwaddr[1])
							return portmap;
					}
				}
				if (value & 0x02)
					return -1;
				if (value & 0x01)
					break;
			}
			reg_write32(REG_ESW_TABLE_SEARCH, 0x02);
		}
		break;
	case SWITCH_MT7620:
	case SWITCH_MT7621:
		if (!entry) {
			reg_write32(REG_ESW_WT_MAC_ATA2, vlan & 0x0fff);
			search = 0x8a04;
		} else
			search = 0x8604;
		reg_write32(REG_ESW_WT_MAC_ATC, search);
		for (i = 0; i < 0x800; i++) {
			for (retry = 0; retry < 20; retry++) {
				usleep(500);
				if (reg_read(REG_ESW_WT_MAC_ATC, &value, 4) < 0)
					return -1;
				if (value & 0x8000)
					continue;
				if (value & 0x2000) {
					if (reg_read(REG_ESW_TABLE_ATRD, &mac[0], 4) < 0)
						return -1;
					valid = (mac[0] >> 24);
					portmap = (mac[0] >> 4) & 0xff;
					if (valid) {
						if (reg_read(REG_ESW_TABLE_TSRA1, &mac[0], 4) < 0 ||
						    reg_read(REG_ESW_TABLE_TSRA2, &mac[1], 4) < 0)
							return -1;
						vid = mac[1] & 0x0fff, mac[1] >>= 16;
						if (vid == vlan && mac[0] == hwaddr[0] && mac[1] == hwaddr[1])
							return portmap;
					}
				}
				if (value & 0x4000)
					return -1;
				if (value & 0x2000)
					break;
			}
			reg_write32(REG_ESW_WT_MAC_ATC, search + 1);
		}
		break;
	default:
		return -1;
	}

	return -1;
}

static int write_entry(uint32_t hwaddr[2], int vlan, int portmap)
{
	uint32_t value;

	switch (model) {
	case SWITCH_RT3x5x:
	case SWITCH_RT5350:
	case SWITCH_MT7628:
		value = 0x0001 | ((vlan & 0x0f) << 7);
		if (portmap >= 0)
			value |= 0x0070 | ((portmap & 0x7f) << 12);
		reg_write32(REG_ESW_WT_MAC_AD2, hwaddr[0]);
		reg_write32(REG_ESW_WT_MAC_AD1, hwaddr[1]);
		reg_write32(REG_ESW_WT_MAC_AD0, value);
		if (reg_wait32(REG_ESW_WT_MAC_AD0, 0x02, 0x02) < 0)
			return -1;
		break;
	case SWITCH_MT7620:
	case SWITCH_MT7621:
		reg_write32(REG_ESW_WT_MAC_ATA1, hwaddr[0]);
		value = (hwaddr[1] << 16) | (1 << 15) | (vlan & 0x0fff);
		reg_write32(REG_ESW_WT_MAC_ATA2, value);
		if (portmap >= 0)
			value = (0xff << 24) | ((portmap & 0xff) << 4) | (0x03 << 2);
		else	value = 0;
		reg_write32(REG_ESW_WT_MAC_ATWD, value);
		reg_write32(REG_ESW_WT_MAC_ATC, 0x8001);
		if (reg_wait32(REG_ESW_WT_MAC_ATC, 0x8000, 0) < 0)
			return -1;
		break;
	default:
		return -1;
	}

	return -1;
}

static int read_portmap(int *vlan)
{
	uint32_t value;
	int i, portmap = 0;
	unsigned int index = *vlan & 0x0fff;

	switch (model) {
	case SWITCH_RT3x5x:
	case SWITCH_RT5350:
	case SWITCH_MT7628:
		for (i = 0; i < 16; i += 2) {
			if (reg_read(REG_ESW_VLAN_ID_BASE + (i/2)*4, &value, 4) < 0)
				return -1;
			if ((value & 0x0fff) == index)
				index = i;
			else if (((value >> 12) & 0x0fff) == index)
				index = i + 1;
			else
				continue;
			if (reg_read(REG_ESW_VLAN_MEMB_BASE + (i/4)*4, &value, 4) < 0)
				return -1;
			portmap = (value >> ((index % 4)*8)) & 0xff;
			*vlan = index;
			break;
		}
		break;
	case SWITCH_MT7620:
		for (i = 0; i < 16; i += 2) {
			if (reg_read(REG_ESW_VLAN_VTIM_BASE + (i/2)*4, &value, 4) < 0)
				return -1;
			if ((value & 0x0fff) == index)
				index = i;
			else if (((value >> 12) & 0x0fff) == index)
				index = i + 1;
			else
				continue;
			reg_write32(REG_ESW_VLAN_VTCR, (1 << 31) | index);
			if (reg_wait32(REG_ESW_VLAN_VTCR, (1 << 31), 0) < 0)
				return -1;
			if (reg_read(REG_ESW_VLAN_VAWD1, &value, 4) < 0)
				return -1;
			if (value & 1)
				portmap = (value >> 16) & 0xff;
			break;
		}
		break;
	case SWITCH_MT7621:
		reg_write32(REG_ESW_VLAN_VTCR, (1 << 31) | index);
		if (reg_wait32(REG_ESW_VLAN_VTCR, (1 << 31), 0) < 0)
			return -1;
		if (reg_read(REG_ESW_VLAN_VAWD1, &value, 4) < 0)
			return -1;
		if (value & 1)
			portmap = (value >> 16) & 0xff;
		break;
	default:
		return -1;
	}

	return portmap;
}

static int forward_to_cpu(int enable)
{
	uint32_t value, old;

	switch (model) {
	case SWITCH_RT5350:
	case SWITCH_MT7628:
		if (reg_read(REG_ESW_PFC1, &old, 4) < 0)
			return -1;
		value = old & ~(1 << 23);
		if (enable)
			value |= (1 << 23);
		if (value != old)
			reg_write32(REG_ESW_PFC1, value);
		break;
	case SWITCH_MT7620:
	case SWITCH_MT7621:
		if (reg_read(REG_ESW_IMC, &old, 4) < 0)
			return -1;
		value = old & ~(0x07 << 12);
		if (enable)
			value |= (0x06 << 12);
		if (value != old)
			reg_write32(REG_ESW_IMC, value);
		break;
	case SWITCH_RT3x5x:
	default:
		return -1;
	}

	return 0;
}

int switch_init(char *ifname, int vid, int cputrap)
{
	struct vlan_ioctl_args ifv;
	uint32_t hwaddr[2], val32;
	int esw_portmap;

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
		vlan = (vid < 0) ? 1 : vid;
	}

	model = get_model();
	switch (model) {
	case SWITCH_RT3x5x:
	case SWITCH_RT5350:
	case SWITCH_MT7628:
		esw_portmap = 0x7f;
		break;
	case SWITCH_MT7620:
		esw_portmap = 0xff;
		if (reg_read(REG_ESW_PFC, &val32, 4) < 0)
			goto error;
		esw_portmap &= ~((val32 & 0x08) ? (1 << (val32 & 0x07)) : 0);
		break;
	case SWITCH_MT7621:
		esw_portmap = 0x7f;
		if (reg_read(REG_ESW_PFC, &val32, 4) < 0)
			goto error;
		esw_portmap &= ~((val32 & 0x08) ? (1 << (val32 & 0x07)) : 0);
		break;
	default:
		log_error("unsupported switch: %s", strerror(errno));
		goto error;
	}

	lan_portmap = read_portmap(&vlan);
	log_switch("%-5s map@vlan%u = " FMT_PORTS, "read",
	    vlan, ARG_PORTS(lan_portmap));

	ether_etohw(ifhwaddr, hwaddr);
	cpu_portmap = read_entry(hwaddr, vlan, 0);
	if (cpu_portmap < 0)
		goto error;
	log_switch("%-5s cpu@vlan%u = " FMT_PORTS, "init",
	    vlan, ARG_PORTS(cpu_portmap));

	lan_portmap &= esw_portmap & ~cpu_portmap;
	log_switch("%-5s lan@vlan%u = " FMT_PORTS, "init",
	    vlan, ARG_PORTS(lan_portmap));

	cpu_forward = cputrap;

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

	close(fd);
}

int switch_get_port(unsigned char *haddr)
{
	uint32_t hwaddr[2];
	int value, port;

	ether_etohw(haddr, hwaddr);

	value = read_entry(hwaddr, vlan, 0);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(haddr), ARG_PORTS(value));

	for (port = -1; value > 0; value >>= 1) {
		port++;
		if (value & 1)
			break;
	}

	return port;
}

int switch_add_portmap(unsigned char *maddr, int portmap)
{
	uint32_t hwaddr[2];
	int value;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	ether_etohw(maddr, hwaddr);

	value = read_entry(hwaddr, vlan, 1);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value < 0)
		value = 0;
	portmap = (value | portmap) | cpu_portmap;
	if (portmap != value) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(portmap));
		write_entry(hwaddr, vlan, portmap);
	}

	return portmap;
}

int switch_del_portmap(unsigned char *maddr, int portmap)
{
	uint32_t hwaddr[2];
	int value;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	ether_etohw(maddr, hwaddr);

	value = read_entry(hwaddr, vlan, 1);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value < 0)
		value = 0;
	portmap = (value & ~portmap) | cpu_portmap;
	if (portmap != value) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(portmap));
		write_entry(hwaddr, vlan, portmap);
	}

	return portmap;
}

int switch_clr_portmap(unsigned char *maddr)
{
	uint32_t hwaddr[2];
	int value;

	if ((maddr[0] & 0x01) == 0)
		return -1;

	ether_etohw(maddr, hwaddr);

	value = read_entry(hwaddr, vlan, 1);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(maddr), ARG_PORTS(value));

	if (value >= 0) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "clear",
		    ARG_EA(maddr), ARG_PORTS(value));
		write_entry(hwaddr, vlan, -1);
	}

	return value;
}

int switch_set_floodmap(unsigned char *raddr, int portmap)
{
	int value;

	if (!cpu_forward)
		return 0;

	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "flood",
	    ARG_EA(raddr), ARG_PORTS(portmap));

	value = (portmap & lan_portmap) ? 0 : 1;
	if (forward_to_cpu(value) < 0)
		value = -1;

	return value;
}

int switch_clr_floodmap(unsigned char *raddr)
{
	if (!cpu_forward)
		return 0;

	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "flood",
	    ARG_EA(raddr), ARG_PORTS(-1));

	return forward_to_cpu(0);
}

#ifdef TEST
int main(int argc, char *argv[])
{
	uint32_t value;
	int write, offset;

	if (!argv[1] || !argv[2])
		return 0;

	write = (strncmp(argv[1], "regw", 4) == 0 ? 1 :
		 strncmp(argv[1], "regr", 4) == 0 ? 0 : -1);
	if (write < 0 || (write && !argv[3]))
		return 0;

	if (switch_init("eth2", 1, 0) < 0) {
		perror("switch");
		return -1;
	}

	offset = strtoul(argv[2], NULL, 0);

	if (write) {
		value = strtoul(argv[3], NULL, 0);
		reg_write32(offset, value);
	}

	value = reg_read32(offset);
	printf("offset %04x = %08x\n", offset, value);

	switch_done();

	return 0;
}
#endif

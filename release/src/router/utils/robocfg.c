/*
 * Broadcom BCM5325E/536x switch configuration utility
 *
 * Copyright (C) 2005 Oleg I. Vdovikin
 * Copyright (C) 2005 Dmitry 'dimss' Ivanov of "Telecentrs" (Riga, Latvia)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301, USA.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

/* linux stuff */
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/types.h>
#include <linux/mii.h>

#include "etc53xx.h"
#define ROBO_PHY_ADDR	0x1E	/* robo switch phy address */

/* MII registers */
#define REG_MII_PAGE	0x10	/* MII Page register */
#define REG_MII_ADDR	0x11	/* MII Address register */
#define REG_MII_DATA0	0x18	/* MII Data register 0 */

#define REG_MII_PAGE_ENABLE	1
#define REG_MII_ADDR_WRITE	1
#define REG_MII_ADDR_READ	2

/* Private et.o ioctls */
#define SIOCGETCPHYRD		(SIOCDEVPRIVATE + 9)
#define SIOCSETCPHYWR		(SIOCDEVPRIVATE + 10)
#define SIOCGETCPHYRD2		(SIOCDEVPRIVATE + 12)
#define SIOCSETCPHYWR2		(SIOCDEVPRIVATE + 13)
#define SIOCGETCROBORD		(SIOCDEVPRIVATE + 14)
#define SIOCSETCROBOWR		(SIOCDEVPRIVATE + 15)

#define ROBO_DEVICE_ID		0x30

typedef struct {
	struct ifreq ifr;
	int fd;
	u8 et;			/* use private ioctls */
	u8 gmii;		/* gigabit mii */
} robo_t;

#ifndef BCM5301X
static u16 __mdio_access(robo_t *robo, u16 phy_id, u8 reg, u16 val, u16 wr)
{
	static int __ioctl_args[2][3] = { {SIOCGETCPHYRD, SIOCGETCPHYRD2, SIOCGMIIREG},
					  {SIOCSETCPHYWR, SIOCSETCPHYWR2, SIOCSMIIREG} };

	if (robo->et) {
		int args[2] = { reg, val };
		int cmd = 0;

		if (phy_id != ROBO_PHY_ADDR) {
			cmd = 1;
			args[0] |= phy_id << 16;
		}
		robo->ifr.ifr_data = (caddr_t) args;
		if (ioctl(robo->fd, __ioctl_args[wr][cmd], (caddr_t)&robo->ifr) < 0) {
			if (phy_id != ROBO_PHY_ADDR) {
				fprintf(stderr,
					"Access to real 'phy' registers unavaliable.\n"
					"Upgrade kernel driver.\n");
				return 0xffff;
			}
			perror("ET ioctl");
			exit(1);
		}
		return args[1];
	} else {
		struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&robo->ifr.ifr_data;
		mii->phy_id = phy_id;
		mii->reg_num = reg;
		mii->val_in = val;
		if (ioctl(robo->fd, __ioctl_args[wr][2], &robo->ifr) < 0) {
			perror("MII ioctl");
			exit(1);
		}
		return mii->val_out;
	}
}

static inline u16 mdio_read(robo_t *robo, u16 phy_id, u8 reg)
{
	return __mdio_access(robo, phy_id, reg, 0, 0);
}

static inline void mdio_write(robo_t *robo, u16 phy_id, u8 reg, u16 val)
{
	__mdio_access(robo, phy_id, reg, val, 1);
}

static int _robo_reg(robo_t *robo, u8 page, u8 reg, u8 op)
{
	int i = 3;
	
	/* set page number */
	mdio_write(robo, ROBO_PHY_ADDR, REG_MII_PAGE, 
		(page << 8) | REG_MII_PAGE_ENABLE);
	
	/* set register address */
	mdio_write(robo, ROBO_PHY_ADDR, REG_MII_ADDR, 
		(reg << 8) | op);

	/* check if operation completed */
	while (i--) {
		if ((mdio_read(robo, ROBO_PHY_ADDR, REG_MII_ADDR) & 3) == 0)
			return 0;
	}

	return -1;
}

static int robo_reg(robo_t *robo, u8 page, u8 reg, u8 op)
{
	if (_robo_reg(robo, page, reg, op))
	{
		fprintf(stderr, "robo_reg: %x/%x timeout\n", page, reg);
		exit(1);
	}

	return 0;
}
#else

static u16 robo_read16(robo_t *robo, u8 page, u8 reg);
static u32 robo_read32(robo_t *robo, u8 page, u8 reg);
static void robo_write16(robo_t *robo, u8 page, u8 reg, u16 val16);
static void robo_write32(robo_t *robo, u8 page, u8 reg, u32 val32);

static inline u16 mdio_read(robo_t *robo, u16 phy_id, u8 reg)
{
	return robo_read16(robo, 0x10 + phy_id, reg);
}

static inline void mdio_write(robo_t *robo, u16 phy_id, u8 reg, u16 val)
{
	robo_write16(robo, 0x10 + phy_id, reg, val);
}

static int _robo_reg(robo_t *robo, u8 page, u8 reg, u8 op)
{
	return 0;
}
#endif

static void robo_read(robo_t *robo, u8 page, u8 reg, u16 *val, int count)
{
#ifdef BCM5301X
	int args[5];

	args[0] = (page << 16) | (reg & 0xffff);
	args[1] = count * 2;
	args[2] = 0;

	robo->ifr.ifr_data = (caddr_t) args;

	if (ioctl(robo->fd, SIOCGETCROBORD, (caddr_t)&robo->ifr) < 0)
		return;

	memcpy(val, &args[2], count * 2);
#else
	int i;
	
	robo_reg(robo, page, reg, REG_MII_ADDR_READ);
	
	for (i = 0; i < count; i++)
		val[i] = mdio_read(robo, ROBO_PHY_ADDR, REG_MII_DATA0 + i);
#endif
}

static u16 robo_read16(robo_t *robo, u8 page, u8 reg)
{
#ifdef BCM5301X
	u16 val16;

	robo_read(robo, page, reg, &val16, 1);
	return val16;
#else
	robo_reg(robo, page, reg, REG_MII_ADDR_READ);
	
	return mdio_read(robo, ROBO_PHY_ADDR, REG_MII_DATA0);
#endif
}

static u32 robo_read32(robo_t *robo, u8 page, u8 reg)
{
#ifdef BCM5301X
	u32 val32;

	robo_read(robo, page, reg, (u16 *) &val32, 2);
	return val32;
#else
	robo_reg(robo, page, reg, REG_MII_ADDR_READ);
	
	return ((u32 )mdio_read(robo, ROBO_PHY_ADDR, REG_MII_DATA0)) |
		((u32 )mdio_read(robo, ROBO_PHY_ADDR, REG_MII_DATA0 + 1) << 16);
#endif
}

static void robo_write(robo_t *robo, u8 page, u8 reg, u16 *val, int count)
{
#ifdef BCM5301X
	int args[5];

	args[0] = (page << 16) | (reg & 0xffff);
	args[1] = count * 2;
	memcpy(&args[2], val, count * 2);

	robo->ifr.ifr_data = (caddr_t) args;

	ioctl(robo->fd, SIOCSETCROBOWR, (caddr_t)&robo->ifr);
#else
	int i;

	for (i = 0; i < count; i++)
		mdio_write(robo, ROBO_PHY_ADDR, REG_MII_DATA0 + i, val[i]);

	robo_reg(robo, page, reg, REG_MII_ADDR_WRITE);
#endif
}

static void robo_write16(robo_t *robo, u8 page, u8 reg, u16 val16)
{
#ifdef BCM5301X
	robo_write(robo, page, reg, &val16, 1);
#else
	/* write data */
	mdio_write(robo, ROBO_PHY_ADDR, REG_MII_DATA0, val16);

	robo_reg(robo, page, reg, REG_MII_ADDR_WRITE);
#endif
}

static void robo_write32(robo_t *robo, u8 page, u8 reg, u32 val32)
{
#ifdef BCM5301X
	robo_write(robo, page, reg, (u16 *) &val32, 2);
#else
	/* write data */
	mdio_write(robo, ROBO_PHY_ADDR, REG_MII_DATA0, (u16 )(val32 & 0xFFFF));
	mdio_write(robo, ROBO_PHY_ADDR, REG_MII_DATA0 + 1, (u16 )(val32 >> 16));
	
	robo_reg(robo, page, reg, REG_MII_ADDR_WRITE);
#endif
}

/* checks that attached switch is 5325/5352/5354/5356/5357/53115/5301x */
static int robo_vlan535x(robo_t *robo, u32 phyid)
{
#ifdef BCM5301X
	if ((robo_read32(robo, ROBO_MGMT_PAGE, ROBO_DEVICE_ID) & 0xfffffff0) == 0x53010)
		return 5;
#else
	/* set vlan access id to 15 and read it back */
	u16 val16 = 15;
	robo_write16(robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS_5350, val16);
	
	/* 5365 will refuse this as it does not have this reg */
	if (robo_read16(robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS_5350) != val16)
		return 0;
	/* gigabit ? */
	if (robo->et != 1 && (mdio_read(robo, 0, ROBO_MII_STAT) & 0x0100))
		robo->gmii = ((mdio_read(robo, 0, 0x0f) & 0xf000) != 0);
	/* 53115 ? */
	if (robo->gmii && robo_read32(robo, ROBO_STAT_PAGE, ROBO_LSA_IM_PORT) != 0) {
		robo_write16(robo, ROBO_ARLIO_PAGE, ROBO_VTBL_INDX_5395, val16);
		robo_write16(robo, ROBO_ARLIO_PAGE, ROBO_VTBL_ACCESS_5395,
					 (1 << 7) /* start */ | 1 /* read */);
		if (robo_read16(robo, ROBO_ARLIO_PAGE, ROBO_VTBL_ACCESS_5395) == 1
		    && robo_read16(robo, ROBO_ARLIO_PAGE, ROBO_VTBL_INDX_5395) == val16)
			return 4;
	}
	/* dirty trick for 5356/5357 */
	if ((phyid & 0xfff0ffff ) == 0x5da00362 ||
	    (phyid & 0xfff0ffff ) == 0x5e000362)
		return 3;
	/* 5325/5352/5354*/
	return 1;
#endif
}

u8 port[9] = { 0, 1, 2, 3, 4, 8, 0, 0, 8};
char ports[] = "01234???5???????";
char *speed[4] = { "10", "100", "1000" , "4" };
char *rxtx[4] = { "enabled", "rx_disabled", "tx_disabled", "disabled" };
char *stp[8] = { "none", "disable", "block", "listen", "learn", "forward", "6", "7" };
char *jumbo[2] = { "off", "on" };

struct {
	char *name;
	u16 bmcr;
} media[7] = {
	{ "auto", BMCR_ANENABLE | BMCR_ANRESTART },
	{ "10HD", 0 },
	{ "10FD", BMCR_FULLDPLX },
	{ "100HD", BMCR_SPEED100 },
	{ "100FD", BMCR_SPEED100 | BMCR_FULLDPLX },
	{ "1000HD", BMCR_SPEED1000 },
	{ "1000FD", BMCR_SPEED1000 | BMCR_FULLDPLX }
};

struct {
	char *name;
	u16 value;
	u16 value1;
	u16 value2;
	u16 value3;
} mdix[3] = {
	{ "auto", 0x0000, 0x0000, 0x8207, 0x0000 },
	{ "on",   0x1800, 0x4000, 0x8007, 0x0080 },
	{ "off",  0x0800, 0x4000, 0x8007, 0x0000 }
};

void usage()
{
	fprintf(stderr, "Broadcom BCM5325/535x/536x/5311x switch configuration utility\n"
		"Copyright (C) 2005-2008 Oleg I. Vdovikin (oleg@cs.msu.su)\n"
		"Copyright (C) 2005 Dmitry 'dimss' Ivanov of \"Telecentrs\" (Riga, Latvia)\n\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
		"GNU General Public License for more details.\n\n");

	fprintf(stderr, "Usage: robocfg <op> ... <op>\n"
			"Operations are as below:\n"
			"\tshow -- show current config\n"
			"\tshowmacs -- show known MAC addresses\n"
			"\tshowports -- show only port config\n"
			"\tswitch <enable|disable>\n"
			"\tport <port_number> [state <%s|%s|%s|%s>]\n"
			"\t\t[stp %s|%s|%s|%s|%s|%s] [tag <vlan_tag>]\n"
			"\t\t[media %s|%s|%s|%s|%s|%s|%s]\n"
			"\t\t[mdi-x %s|%s|%s] [jumbo %s|%s]\n"
			"\tvlan <vlan_number> [ports <ports_list>]\n"
			"\tvlans <enable|disable|reset>\n\n"
			"\tports_list should be one argument, space separated, quoted if needed,\n"
			"\tport number could be followed by 't' to leave packet vlan tagged (CPU \n"
			"\tport default) or by 'u' to untag packet (other ports default) before \n"
			"\tbringing it to the port, '*' is ignored\n"
			"\nSamples:\n"
			"1) ASUS WL-500g Deluxe stock config (eth0 is WAN, eth0.1 is LAN):\n"
			"robocfg switch disable vlans enable reset vlan 0 ports \"0 5u\" vlan 1 ports \"1 2 3 4 5t\""
			" port 0 state enabled stp none switch enable\n"
			"2) WRT54g, WL-500g Deluxe OpenWRT config (vlan0 is LAN, vlan1 is WAN):\n"
			"robocfg switch disable vlans enable reset vlan 0 ports \"1 2 3 4 5t\" vlan 1 ports \"0 5t\""
			" port 0 state enabled stp none switch enable\n",
			rxtx[0], rxtx[1], rxtx[2], rxtx[3], stp[0], stp[1], stp[2], stp[3], stp[4], stp[5],
			media[0].name, media[1].name, media[2].name, media[3].name, media[4].name, media[5].name, media[6].name,
			mdix[0].name, mdix[1].name, mdix[2].name,
			jumbo[0], jumbo[1]);
}

int
main(int argc, char *argv[])
{
	u16 val16;
	u32 val32;
	u16 mac[3];
	int i = 0, j;
	int robo535x = 0; /* 0 - 5365, 1 - 5325/5352/5354, 3 - 5356, 4 - 53115, 5 - 5301x */
	u32 phyid;
	int novlan = 0;
	
	static robo_t robo;
	struct ethtool_drvinfo info;
	
	if ((robo.fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* the only interface for now */
	strcpy(robo.ifr.ifr_name, "eth0");

	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
	robo.ifr.ifr_data = (caddr_t)&info;
	if (ioctl(robo.fd, SIOCETHTOOL, (caddr_t)&robo.ifr) < 0) {
		perror("SIOCETHTOOL: your ethernet module is either unsupported or outdated");
		exit(1);
	} else
	if (strcmp(info.driver, "et0") && strcmp(info.driver, "et1") && strcmp(info.driver, "et2") &&
	    strcmp(info.driver, "b44")) {
		fprintf(stderr, "No suitable module found for %s (managed by %s)\n", 
			robo.ifr.ifr_name, info.driver);
		exit(1);
	}
	
	/* try access using MII ioctls - get phy address */
	if (ioctl(robo.fd, SIOCGMIIPHY, &robo.ifr) < 0) {
		int args[2] = { ROBO_PHY_ADDR << 16, 0x0 };

		robo.ifr.ifr_data = (caddr_t) args;
		if (ioctl(robo.fd, SIOCGETCPHYRD2, &robo.ifr) < 0)
			robo.et = 1;
		else	robo.et = 2;
	} else {
		/* got phy address check for robo address */
		struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&robo.ifr.ifr_data;
		if (mii->phy_id != ROBO_PHY_ADDR) {
			fprintf(stderr, "Invalid phy address (%d)\n", mii->phy_id);
			exit(1);
		}
	}

	phyid = mdio_read(&robo, ROBO_PHY_ADDR, 0x2) | 
		(mdio_read(&robo, ROBO_PHY_ADDR, 0x3) << 16);
	if (phyid == 0 && robo.et != 1)
	    phyid = mdio_read(&robo, 0, 0x2) | 
			(mdio_read(&robo, 0, 0x3) << 16);

	if (phyid == 0xffffffff || phyid == 0x55210022) {
		fprintf(stderr, "No Robo switch in managed mode found\n");
		exit(1);
	}
	
	robo535x = robo_vlan535x(&robo, phyid);
	/* fprintf(stderr, "phyid %08x id %d\n", phyid, robo535x); */
	
	for (i = 1; i < argc;) {
		if (strcasecmp(argv[i], "showmacs") == 0)
		{
			/* show MAC table of switch */
			u16 buf[6], r;
			int idx, off, base_vlan;

			base_vlan = 0; /*get_vid_by_idx(&robo, 0);*/

			printf(
				"--------------------------------------\n"
				"VLAN  MAC                Type     Port\n"
				"--------------------------------------\n");
			robo_write16(&robo, ROBO_ARLIO_PAGE, ROBO_ARL_RW_CTRL, 0x81);
			robo_write16(&robo, ROBO_ARLIO_PAGE, (robo535x >= 4) ?
			    ROBO_ARL_SEARCH_CTRL_53115 : ROBO_ARL_SEARCH_CTRL, 0x80);

			for (idx = 0; idx < ((robo535x >= 4) ?
				NUM_ARL_TABLE_ENTRIES_53115 : robo535x ?
				NUM_ARL_TABLE_ENTRIES_5350 : NUM_ARL_TABLE_ENTRIES); idx++)
			{
				if (robo535x >= 4) {
					off = (idx & 0x01) << 4;
					if (!off) {
						j = 0;
					again:
						r = robo_read16(&robo, ROBO_ARLIO_PAGE,
								ROBO_ARL_SEARCH_CTRL_53115);
						if ((r & 0x80) == 0) break;
						if ((r & 0x01) == 0) {
							if (++j >= 10) break;
							usleep(200);
							goto again;
						}
					}
					robo_read(&robo, ROBO_ARLIO_PAGE,
					    ROBO_ARL_SEARCH_RESULT_53115 + off, buf, 4);
					robo_read(&robo, ROBO_ARLIO_PAGE,
					    ROBO_ARL_SEARCH_RESULT_EXT_53115 + off, &buf[4], 2);
				} else {
					robo_read(&robo, ROBO_ARLIO_PAGE, ROBO_ARL_SEARCH_RESULT_EXT,  &buf[4], 1);
					robo_read(&robo, ROBO_ARLIO_PAGE, ROBO_ARL_SEARCH_RESULT,  buf, 4);
				}
				if ((robo535x >= 4) ? (buf[5] & 0x01) : (buf[3] & 0x8000) /* valid */)
				{
					printf("%04i  %02x:%02x:%02x:%02x:%02x:%02x  %-7s  ",
						(base_vlan | (robo535x >= 4) ?
						    (base_vlan | (buf[3] & 0xfff)) :
						    ((buf[3] >> 5) & 0x0f) |
							(robo535x ? 0 : ((buf[4] & 0x0f) << 4))),
						buf[2] >> 8, buf[2] & 255, 
						buf[1] >> 8, buf[1] & 255,
						buf[0] >> 8, buf[0] & 255,
						((robo535x >= 4 ?
						    (buf[4] & 0x8000) : (buf[3] & 0x4000)) ? "STATIC" : "DYNAMIC")
					);
					if (buf[2] & 0x100) {
						val16 = (robo535x >= 4) ? (buf[4] & 0x1ff) :
							(buf[3] & 0x1f) | ((buf[4] & 4) << 3);
						if (val16 == 0)
							printf("-");
						else
						for (j = 0; val16; val16 >>= 1, j++) {
							if (val16 & 1)
								printf("%d ", j);
						}
					} else
						printf("%d", (robo535x >= 4) ? buf[4] & 0x0f :
						       ports[buf[3] & 0x0f] - '0');
					printf("\n");
				}
			}
			i++;
		} else
		if (strcasecmp(argv[i], "port") == 0 && (i + 1) < argc)
		{
			int index = atoi(argv[++i]);
			/* read port specs */
			while (++i < argc) {
				if (strcasecmp(argv[i], "state") == 0 && ++i < argc) {
					for (j = 0; j < 4 && strcasecmp(argv[i], rxtx[j]); j++);
					if (j < 4) {
						/* change state */
						robo_write16(&robo,ROBO_CTRL_PAGE, port[index],
							(robo_read16(&robo, ROBO_CTRL_PAGE, port[index]) & ~(3 << 0)) | (j << 0));
					} else {
						fprintf(stderr, "Invalid state '%s'.\n", argv[i]);
						exit(1);
					}
				} else
				if (strcasecmp(argv[i], "stp") == 0 && ++i < argc) {
					for (j = 0; j < 8 && strcasecmp(argv[i], stp[j]); j++);
					if (j < 8) {
						/* change stp */
						robo_write16(&robo,ROBO_CTRL_PAGE, port[index],
							(robo_read16(&robo, ROBO_CTRL_PAGE, port[index]) & ~(7 << 5)) | (j << 5));
					} else {
						fprintf(stderr, "Invalid stp '%s'.\n", argv[i]);
						exit(1);
					}
				} else
				if (strcasecmp(argv[i], "media") == 0 && ++i < argc) {
					for (j = 0; j < 7 && strcasecmp(argv[i], media[j].name); j++);
					if (j < ((robo535x >= 4) ? 7 : 5)) {
						/* change media */
                                    		mdio_write(&robo, port[index], MII_BMCR, media[j].bmcr);
					} else {
						fprintf(stderr, "Invalid media '%s'.\n", argv[i]);
						exit(1);
					}
				} else
				if (strcasecmp(argv[i], "mdi-x") == 0 && ++i < argc) {
					for (j = 0; j < 3 && strcasecmp(argv[i], mdix[j].name); j++);
					if (j < 3) {
						/* change mdi-x */
						if (robo535x >= 4) {
							mdio_write(&robo, port[index], 0x10, mdix[j].value1 |
							    (mdio_read(&robo, port[index], 0x10) & ~0x4000));
							mdio_write(&robo, port[index], 0x18, 0x7007);
							mdio_write(&robo, port[index], 0x18, mdix[j].value2 |
							    (mdio_read(&robo, port[index], 0x18) & ~0x8207));
							mdio_write(&robo, port[index], 0x1e, mdix[j].value3 |
							    (mdio_read(&robo, port[index], 0x1e) & ~0x0080));
						} else
                                    		    mdio_write(&robo, port[index], 0x1c, mdix[j].value |
							(mdio_read(&robo, port[index], 0x1c) & ~0x1800));
					} else {
						fprintf(stderr, "Invalid mdi-x '%s'.\n", argv[i]);
						exit(1);
					}
				} else
				if (strcasecmp(argv[i], "tag") == 0 && ++i < argc) {
					j = atoi(argv[i]);
					/* change vlan tag */
					robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_PORT0_DEF_TAG + (index << 1), j);
				} else
				if (strcasecmp(argv[i], "jumbo") == 0 && ++i < argc) {
					for (j = 0; j < 2 && strcasecmp(argv[i], jumbo[j]); j++);
					if (robo535x >= 4 && j < 2) {
						/* change jumbo frame feature */
						robo_write32(&robo, ROBO_JUMBO_PAGE, ROBO_JUMBO_CTRL,
							(robo_read32(&robo, ROBO_JUMBO_PAGE, ROBO_JUMBO_CTRL) &
							~(1 << port[index])) | (j << port[index]));
					} else {
						fprintf(stderr, "Invalid jumbo state '%s'.\n", argv[i]);
						exit(1);
					}
				} else break;
			}
		} else
		if (strcasecmp(argv[i], "vlan") == 0 && (i + 1) < argc)
		{
			int vid = atoi(argv[++i]);
			while (++i < argc) {
				if (strcasecmp(argv[i], "ports") == 0 && ++i < argc) {
					char *ports = argv[i];
					int untag = 0;
					int member = 0;
					
					while (*ports >= '0' && *ports <= '9') {
						j = *ports++ - '0';
						member |= 1 << j;
						
						/* untag if needed, CPU port requires special handling */
						if (*ports == 'u' || (j != 5 && (*ports == ' ' || *ports == 0))) 
						{
							untag |= 1 << j;
							if (*ports) ports++;
							/* change default vlan tag */
							robo_write16(&robo, ROBO_VLAN_PAGE, 
								ROBO_VLAN_PORT0_DEF_TAG + (j << 1), vid);
						} else 
						if (*ports == '*' || *ports == 't' || *ports == ' ') ports++;
						else break;
						
						while (*ports == ' ') ports++;
					}
					
					if (*ports) {
						fprintf(stderr, "Invalid ports '%s'.\n", argv[i]);
						exit(1);
					} else {
						/* write config now */
						val16 = (vid) /* vlan */ | (1 << 12) /* write */ | (1 << 13) /* enable */;
						if (robo535x >= 4) {
							val32 = (untag << 9) | member;
							/* entry */
							robo_write32(&robo, ROBO_ARLIO_PAGE, ROBO_VTBL_ENTRY_5395, val32);
							/* index */
							robo_write16(&robo, ROBO_ARLIO_PAGE, ROBO_VTBL_INDX_5395, vid);
							/* access */
							robo_write16(&robo, ROBO_ARLIO_PAGE, ROBO_VTBL_ACCESS_5395,
										 (1 << 7) /* start */ | 0 /* write */);
						} else if (robo535x) {
							if (robo535x == 3)
								val32 = (1 << 24) /* valid */ | (untag << 6) | member | (vid << 12);
							else
								val32 = (1 << 20) /* valid */ | (untag << 6) | member | ((vid >> 4) << 12);
							robo_write32(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_WRITE_5350, val32);
							robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS_5350, val16);
						} else {
							robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_WRITE,
								(1 << 14)  /* valid */ | (untag << 7) | member);
							robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS, val16);
						}
					}
				} else break;
			}
		} else
		if (strcasecmp(argv[i], "switch") == 0 && (i + 1) < argc)
		{
			/* enable/disable switching */
			robo_write16(&robo, ROBO_CTRL_PAGE, ROBO_SWITCH_MODE,
				(robo_read16(&robo, ROBO_CTRL_PAGE, ROBO_SWITCH_MODE) & ~2) |
				(*argv[++i] == 'e' ? 2 : 0));
			i++;
		} else
		if (strcasecmp(argv[i], "vlans") == 0 && (i + 1) < argc)
		{
			while (++i < argc) {
				if (strcasecmp(argv[i], "reset") == 0) {
					if (robo535x >= 4) {
						robo_write16(&robo, ROBO_ARLIO_PAGE, ROBO_VTBL_ACCESS_5395,
									 (1 << 7) /* start */ | 2 /* flush */);
					} else
					/* reset vlan validity bit */
					for (j = 0; j <= ((robo535x == 1) ? VLAN_ID_MAX5350 : VLAN_ID_MAX); j++) 
					{
						/* write config now */
						val16 = (j) /* vlan */ | (1 << 12) /* write */ | (1 << 13) /* enable */;
						if (robo535x) {
							robo_write32(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_WRITE_5350, 0);
							robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS_5350, val16);
						} else {
							robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_WRITE, 0);
							robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS, val16);
						}
					}
				} else 
				if (strcasecmp(argv[i], "enable") == 0 || strcasecmp(argv[i], "disable") == 0) 
				{
					int disable = (*argv[i] == 'd') || (*argv[i] == 'D');
					/* enable/disable vlans */
					robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_CTRL0, disable ? 0 :
						(1 << 7) /* 802.1Q VLAN */ | (3 << 5) /* mac check and hash */);

					robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_CTRL1, disable ? 0 :
						(1 << 1) | (1 << 2) | (1 << 3) /* RSV multicast */);

					robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_CTRL4, disable ? 0 :
						(1 << 6) /* drop invalid VID frames */);

					robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_CTRL5, disable ? 0 :
						(1 << 3) /* drop miss V table frames */);

				} else break;
			}
		} else
		if (strcasecmp(argv[i], "show") == 0)
		{
			break;
		} else
		if (strcasecmp(argv[i], "showports") == 0)
		{
			novlan = 1;
			break;
		} else
		if (strncasecmp(argv[i], "robowr", 6) == 0 && (i + 2) < argc)
		{
			long pagereg = strtoul(argv[i + 1], NULL, 0);
			int size = strtoul(argv[i] + 6, NULL, 0);
			int k;
			unsigned long long int v;
			u16 buf[4];

			size = (size > 0 && size <= sizeof(buf) * 16) ? (size + 15) >> 4 : 1;

			v = strtoull(argv[i + 2], NULL, 0);
			for (k = 0; k < size; k++)
			{
				buf[k] = (u16 )(v & 0xFFFF);
				v >>= 16;
			}
			robo_write(&robo, pagereg >> 8, pagereg & 255, buf, size);

			printf("Page 0x%02x, Reg 0x%02x: ",
				(u16 )(pagereg >> 8), (u8 )(pagereg & 255));
			robo_read(&robo, pagereg >> 8, pagereg & 255, buf, size);
			while (size > 0)
				printf("%04x", buf[--size]);
			printf("\n");

			i += 3;
		} else
		if (strncasecmp(argv[i], "robord", 6) == 0 && (i + 1) < argc)
		{
			long pagereg = strtoul(argv[i + 1], NULL, 0);
			int size = strtoul(argv[i] + 6, NULL, 0);
			u16 buf[4];

			size = (size > 0 && size <= sizeof(buf) * 16) ? (size + 15) >> 4 : 1;

			printf("Page 0x%02x, Reg 0x%02x: ",
				(u16 )(pagereg >> 8), (u8 )(pagereg & 255));

			robo_read(&robo, pagereg >> 8, pagereg & 255, buf, size);
			while (size > 0)
				printf("%04x", buf[--size]);
			printf("\n");

			i += 2;
		} else
		if (strcasecmp(argv[i], "dump") == 0)
		{
			for (i = 0; i < 256; i++)
			{
				if (_robo_reg(&robo, i, 0, REG_MII_ADDR_READ))
					continue;

				printf("Page %02x\n", i);

				for (j = 0; j < 128; j++) {
					printf(" %04x%s",
						robo_read16(&robo, i, j), (j % 16) == 15 ? "\n" : "");
				}
			}

			i = 2;
		} else {
			fprintf(stderr, "Invalid option %s\n", argv[i]);
			usage();
			exit(1);
		}
	}

	if (i == argc) {
		if (argc == 1) usage();
		return 0;
	}
	
	/* show config */
		
	printf("Switch: %sabled %s\n", robo_read16(&robo, ROBO_CTRL_PAGE, ROBO_SWITCH_MODE) & 2 ? "en" : "dis",
		    robo.gmii ? "gigabit" : "");

	for (i = 0; i <= 8; i++) {
		if (i < 8 && ((robo535x < 4 && i > 4) || (robo535x < 5 && i > 5) || (robo535x == 5 && i == 6)))
			continue;
		printf(robo_read16(&robo, ROBO_STAT_PAGE, ROBO_LINK_STAT_SUMMARY) & (1 << i) ?
			"Port %d: %4s%s " : "Port %d:   DOWN ",
			(robo535x >= 4) ? i : ports[i] - '0',
			speed[(robo535x >= 4) ?
				(robo_read32(&robo, ROBO_STAT_PAGE, ROBO_SPEED_STAT_SUMMARY) >> i * 2) & 3 :
				(robo_read16(&robo, ROBO_STAT_PAGE, ROBO_SPEED_STAT_SUMMARY) >> i) & 1],
			robo_read16(&robo, ROBO_STAT_PAGE, (robo535x >= 4) ?
				ROBO_DUPLEX_STAT_SUMMARY_53115 : ROBO_DUPLEX_STAT_SUMMARY) & (1 << i) ? "FD" : "HD");

		val16 = robo_read16(&robo, ROBO_CTRL_PAGE, i);

		printf("%s stp: %s vlan: %d ", rxtx[val16 & 3], stp[(val16 >> 5) & 7],
			robo_read16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_PORT0_DEF_TAG +
				(((robo535x >= 4) ? i : ports[i] - '0') << 1)));

		if (robo535x >= 4)
			printf("jumbo: %s ", jumbo[(robo_read32(&robo, ROBO_JUMBO_PAGE, ROBO_JUMBO_CTRL) >> i) & 1]);

		robo_read(&robo, ROBO_STAT_PAGE, ROBO_LSA_PORT0 + i * 6, mac, 3);

		printf("mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			mac[2] >> 8, mac[2] & 255, mac[1] >> 8, mac[1] & 255, mac[0] >> 8, mac[0] & 255);
	}
	
	if (novlan) return (0);	// Only show ethernet port states, used by webui
	val16 = robo_read16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_CTRL0);
	
	printf("VLANs: %s %sabled%s%s\n", 
		(robo535x == 5) ? "BCM5301x" :
		(robo535x == 4) ? "BCM53115" : (robo535x ? "BCM5325/535x" : "BCM536x"),
		(val16 & (1 << 7)) ? "en" : "dis", 
		(val16 & (1 << 6)) ? " mac_check" : "", 
		(val16 & (1 << 5)) ? " mac_hash" : "");
	
	/* scan VLANs */
	for (i = 0; i <= ((robo535x >= 4) ? VLAN_ID_MAX5395 /* slow, needs rework, but how? */ :
			  (robo535x ? VLAN_ID_MAX5350 : VLAN_ID_MAX)); i++)
	{
		/* issue read */
		val16 = (i) /* vlan */ | (0 << 12) /* read */ | (1 << 13) /* enable */;
		
		if (robo535x >= 4) {
			/* index */
			robo_write16(&robo, ROBO_ARLIO_PAGE, ROBO_VTBL_INDX_5395, i);
			/* access */
			robo_write16(&robo, ROBO_ARLIO_PAGE, ROBO_VTBL_ACCESS_5395,
						 (1 << 7) /* start */ | 1 /* read */);
			/* actual read */
			val32 = robo_read32(&robo, ROBO_ARLIO_PAGE, ROBO_VTBL_ENTRY_5395);
			if ((val32)) {
				printf("%4d: vlan%d:", i, i);
				for (j = 0; j <= 8; j++) {
					if (val32 & (1 << j)) {
						printf(" %d%s", j, (val32 & (1 << (j + 9))) ? 
							(j == 8 ? "u" : "") : "t");
					}
				}
				printf("\n");
			}
		} else if (robo535x) {
			robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS_5350, val16);
			/* actual read */
			val32 = robo_read32(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_READ);
			if ((val32 & (robo535x == 3 ? (1 << 24) : (1 << 20))) /* valid */) {
				val16 = (robo535x == 3)
					? ((val32 & 0x00fff000) >> 12)
					: (((val32 & 0xff000) >> 12) << 4) | i;
				printf("%4d: vlan%d:", i, val16);
				for (j = 0; j < 6; j++) {
					if (val32 & (1 << j)) {
						printf(" %d%s", j, (val32 & (1 << (j + 6))) ? 
							(j == 5 ? "u" : "") : "t");
					}
				}
				printf("\n");
			}
		} else {
			robo_write16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_TABLE_ACCESS, val16);
			/* actual read */
			val16 = robo_read16(&robo, ROBO_VLAN_PAGE, ROBO_VLAN_READ);
			if ((val16 & (1 << 14)) /* valid */) {
				printf("%4d: vlan%d:", i, i);
				for (j = 0; j < 6; j++) {
					if (val16 & (1 << j)) {
						printf(" %d%s", j, (val16 & (1 << (j + 7))) ? 
							(j == 5 ? "u" : "") : "t");
					}
				}
				printf("\n");
			}
		}
	}
	
	return (0);
}

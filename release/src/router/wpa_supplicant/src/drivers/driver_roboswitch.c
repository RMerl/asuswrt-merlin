/*
 * WPA Supplicant - roboswitch driver interface
 * Copyright (c) 2008-2009 Jouke Witteveen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/mii.h>

#include "common.h"
#include "driver.h"
#include "l2_packet/l2_packet.h"

#ifndef ETH_P_EAPOL
#define ETH_P_EAPOL		0x888e
#endif

#define ROBO_PHY_ADDR		0x1e	/* RoboSwitch PHY address */

#define SIOCGETCPHYRD		(SIOCDEVPRIVATE + 9)
#define SIOCSETCPHYWR		(SIOCDEVPRIVATE + 10)
#define SIOCGETCPHYRD2		(SIOCDEVPRIVATE + 12)
#define SIOCSETCPHYWR2		(SIOCDEVPRIVATE + 13)
#define SIOCGETCROBORD		(SIOCDEVPRIVATE + 14)
#define SIOCSETCROBOWR		(SIOCDEVPRIVATE + 15)

/* MII access registers */
#define ROBO_MII_PAGE		0x10	/* MII page register */
#define ROBO_MII_ADDR		0x11	/* MII address register */
#define ROBO_MII_DATA_OFFSET	0x18	/* Start of MII data registers */

#define ROBO_MII_PAGE_ENABLE	0x01	/* MII page op code */
#define ROBO_MII_ADDR_WRITE	0x01	/* MII address write op code */
#define ROBO_MII_ADDR_READ	0x02	/* MII address read op code */
#define ROBO_MII_DATA_MAX	   4	/* Consecutive MII data registers */
#define ROBO_MII_RETRY_MAX	  10	/* Read attempts before giving up */

/* Page numbers */
#define ROBO_ARLCTRL_PAGE	0x04	/* ARL control page */
#define ROBO_VLAN_PAGE		0x34	/* VLAN page */

/* ARL control page registers */
#define ROBO_ARLCTRL_CONF	0x00	/* ARL configuration register */
#define ROBO_ARLCTRL_ADDR_1	0x10	/* Multiport address 1 */
#define ROBO_ARLCTRL_VEC_1	0x16	/* Multiport vector 1 */
#define ROBO_ARLCTRL_ADDR_2	0x20	/* Multiport address 2 */
#define ROBO_ARLCTRL_VEC_2	0x26	/* Multiport vector 2 */

/* VLAN page registers */
#define ROBO_VLAN_ACCESS	0x08	/* VLAN table access register */
#define ROBO_VLAN_ACCESS_5350	0x06	/* VLAN table access register (5350) */
#define ROBO_VLAN_READ		0x0c	/* VLAN read register */
#define ROBO_VLAN_MAX		0xff	/* Maximum number of VLANs */
#define ROBO_VLAN_MAX_5350	0x0f	/* Maximum number of VLANs (5350) */


/* RoboSwitch Models */
enum {
	BCM536x = 0,			/* 5365 */
	BCM535x = 1 << 0,		/* 5325, 5352, 5354 */
	BCM5356 = 1 << 1		/* 5356, 5357 */
};


static const u8 pae_group_addr[ETH_ALEN] =
{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03 };


struct wpa_driver_roboswitch_data {
	void *ctx;
	struct l2_packet_data *l2, *l2_vlan;
	char ifname[IFNAMSIZ + 1];
	u8 own_addr[ETH_ALEN];
	struct ifreq ifr;
	int fd, model, et;
	u16 ports;
};


/* Copied from the kernel-only part of mii.h. */
static inline struct mii_ioctl_data *if_mii(struct ifreq *rq)
{
	return (struct mii_ioctl_data *) &rq->ifr_ifru;
}


/*
 * RoboSwitch uses 16-bit Big Endian addresses.
 * The ordering of the words is reversed in the MII registers.
 */
static void wpa_driver_roboswitch_addr_be16(const u8 addr[ETH_ALEN], u16 *be)
{
	int i;
	for (i = 0; i < ETH_ALEN; i += 2)
		be[(ETH_ALEN - i) / 2 - 1] = WPA_GET_BE16(addr + i);
}


static u16 wpa_driver_roboswitch_mdio_access(
	struct wpa_driver_roboswitch_data *drv, u8 phy, u8 reg, u16 val, int op)
{
	if (drv->et) {
		static int et_ioctl[2][2] = {{ SIOCGETCPHYRD, SIOCSETCPHYWR },
					     { SIOCGETCPHYRD2, SIOCSETCPHYWR2 }};
		int args[2] = { reg, val };

		drv->ifr.ifr_data = (caddr_t) args;
		if (phy != ROBO_PHY_ADDR) {
			args[0] |= phy << 16;
			if (ioctl(drv->fd, et_ioctl[1][op], &drv->ifr) < 0)
				return 0xffff;
		} else
		if (ioctl(drv->fd, et_ioctl[0][op], &drv->ifr) < 0)
			return 0xffff;

		return op ? 0 : args[1];
	} else {
		static int mii_ioctl[2] = { SIOCGMIIREG, SIOCSMIIREG };
		struct mii_ioctl_data *mii = if_mii(&drv->ifr);

		mii->phy_id = phy;
		mii->reg_num = reg;
		mii->val_in = val;
		if (ioctl(drv->fd, mii_ioctl[op], &drv->ifr) < 0)
			return 0xffff;

		return op ? 0 : mii->val_out;
	}
}


static inline u16 wpa_driver_roboswitch_mdio_read(
	struct wpa_driver_roboswitch_data *drv, u8 reg)
{
	return wpa_driver_roboswitch_mdio_access(drv, ROBO_PHY_ADDR, reg, 0, 0);
}


static inline void wpa_driver_roboswitch_mdio_write(
	struct wpa_driver_roboswitch_data *drv, u8 reg, u16 val)
{
	wpa_driver_roboswitch_mdio_access(drv, ROBO_PHY_ADDR, reg, val, 1);
}


static int wpa_driver_roboswitch_reg(struct wpa_driver_roboswitch_data *drv,
				     u8 page, u8 reg, u8 op)
{
	int i;

	/* set page number */
	wpa_driver_roboswitch_mdio_write(drv, ROBO_MII_PAGE,
					 (page << 8) | ROBO_MII_PAGE_ENABLE);
	/* set register address */
	wpa_driver_roboswitch_mdio_write(drv, ROBO_MII_ADDR, (reg << 8) | op);

	/* check if operation completed */
	for (i = 0; i < ROBO_MII_RETRY_MAX; ++i) {
		if ((wpa_driver_roboswitch_mdio_read(drv,
				ROBO_MII_ADDR) & 3) == 0)
			return 0;
	}

	/* timeout */
	return -1;
}


static int wpa_driver_roboswitch_read(struct wpa_driver_roboswitch_data *drv,
				      u8 page, u8 reg, u16 *val, int len)
{
	int i;

	if (len > ROBO_MII_DATA_MAX ||
	    wpa_driver_roboswitch_reg(drv, page, reg, ROBO_MII_ADDR_READ) < 0)
		return -1;

	for (i = 0; i < len; ++i) {
		val[i] = wpa_driver_roboswitch_mdio_read(drv,
				ROBO_MII_DATA_OFFSET + i);
	}

	return 0;
}


static int wpa_driver_roboswitch_write(struct wpa_driver_roboswitch_data *drv,
				       u8 page, u8 reg, u16 *val, int len)
{
	int i;

	if (len > ROBO_MII_DATA_MAX) return -1;
	for (i = 0; i < len; ++i) {
		wpa_driver_roboswitch_mdio_write(drv, ROBO_MII_DATA_OFFSET + i,
						 val[i]);
	}
	return wpa_driver_roboswitch_reg(drv, page, reg, ROBO_MII_ADDR_WRITE);
}


static void wpa_driver_roboswitch_receive(void *priv, const u8 *src_addr,
					  const u8 *buf, size_t len)
{
	struct wpa_driver_roboswitch_data *drv = priv;

	if (len > 14 && WPA_GET_BE16(buf + 12) == ETH_P_EAPOL &&
	    (os_memcmp(buf, drv->own_addr, ETH_ALEN) == 0 ||
	     os_memcmp(buf, pae_group_addr, ETH_ALEN) == 0)) {
		wpa_supplicant_rx_eapol(drv->ctx, src_addr, buf + 14,
					len - 14);
	}
}


static int wpa_driver_roboswitch_send(void *priv, const u8 *dest, u16 proto,
				      const u8 *data, size_t data_len)
{
	struct wpa_driver_roboswitch_data *drv = priv;
	struct {
		struct l2_ethhdr eth;
		u8 data[0];
	} STRUCT_PACKED *msg;
	size_t msg_len;
	int res;

	if (drv->l2 == NULL)
		return -1;

	msg_len = sizeof(msg->eth) + data_len;
	msg = os_malloc(msg_len);
	if (msg == NULL)
		return -1;

	os_memset(&msg->eth, 0, sizeof(msg->eth));
	os_memcpy(msg->eth.h_dest, dest, ETH_ALEN);
	os_memcpy(msg->eth.h_source, drv->own_addr, ETH_ALEN);
	msg->eth.h_proto = host_to_be16(proto);
	os_memcpy(msg->data, data, data_len);

	res = l2_packet_send(drv->l2, dest, proto, (u8 *) msg, msg_len);
	os_free(msg);

	return res;
}


static int wpa_driver_roboswitch_get_ssid(void *priv, u8 *ssid)
{
	ssid[0] = 0;
	return 0;
}


static int wpa_driver_roboswitch_get_bssid(void *priv, u8 *bssid)
{
	/* Report PAE group address as the "BSSID" for wired connection. */
	os_memcpy(bssid, pae_group_addr, ETH_ALEN);
	return 0;
}


static const u8 * wpa_driver_roboswitch_get_mac_addr(void *priv)
{
	struct wpa_driver_roboswitch_data *drv = priv;
	return drv->own_addr;
}


static int wpa_driver_roboswitch_join(struct wpa_driver_roboswitch_data *drv,
				      u16 ports, const u8 *addr)
{
	u16 read1[3], read2[3], addr_be16[3];

	wpa_driver_roboswitch_addr_be16(addr, addr_be16);

	if (wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
				       ROBO_ARLCTRL_CONF, read1, 1) < 0)
		return -1;
	if (!(read1[0] & (1 << 4))) {
		/* multiport addresses are not yet enabled */
		read1[0] |= 1 << 4;
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_ADDR_1, addr_be16, 3);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_VEC_1, &ports, 1);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_ADDR_2, addr_be16, 3);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_VEC_2, &ports, 1);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_CONF, read1, 1);
	} else {
		/* if both multiport addresses are the same we can add */
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_ADDR_1, read1, 3);
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_ADDR_2, read2, 3);
		if (os_memcmp(read1, read2, 6) != 0)
			return -1;
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_VEC_1, read1, 1);
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_VEC_2, read2, 1);
		if (read1[0] != read2[0])
			return -1;
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_ADDR_1, addr_be16, 3);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_VEC_1, &ports, 1);
	}
	return 0;
}


static int wpa_driver_roboswitch_leave(struct wpa_driver_roboswitch_data *drv,
				       u16 ports, const u8 *addr)
{
	u16 _read, addr_be16[3], addr_read[3], ports_read;

	wpa_driver_roboswitch_addr_be16(addr, addr_be16);

	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE, ROBO_ARLCTRL_CONF,
				   &_read, 1);
	/* If ARL control is disabled, there is nothing to leave. */
	if (!(_read & (1 << 4))) return -1;

	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
				   ROBO_ARLCTRL_ADDR_1, addr_read, 3);
	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE, ROBO_ARLCTRL_VEC_1,
				   &ports_read, 1);
	/* check if we occupy multiport address 1 */
	if (os_memcmp(addr_read, addr_be16, 6) == 0 && ports_read == ports) {
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_ADDR_2, addr_read, 3);
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_VEC_2, &ports_read, 1);
		/* and multiport address 2 */
		if (os_memcmp(addr_read, addr_be16, 6) == 0 &&
		    ports_read == ports) {
			_read &= ~(1 << 4);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_CONF, &_read,
						    1);
		} else {
			wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
						   ROBO_ARLCTRL_ADDR_1,
						   addr_read, 3);
			wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
						   ROBO_ARLCTRL_VEC_1,
						   &ports_read, 1);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_ADDR_2,
						    addr_read, 3);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_VEC_2,
						    &ports_read, 1);
		}
	} else {
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_ADDR_2, addr_read, 3);
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_VEC_2, &ports_read, 1);
		/* or multiport address 2 */
		if (os_memcmp(addr_read, addr_be16, 6) == 0 &&
		    ports_read == ports) {
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_ADDR_1,
						    addr_read, 3);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_VEC_1,
						    &ports_read, 1);
		} else return -1;
	}
	return 0;
}


static void * wpa_driver_roboswitch_init(void *ctx, const char *ifname)
{
	struct wpa_driver_roboswitch_data *drv;
	struct vlan_ioctl_args ifv;
	u32 phyid;
	u16 vlan, i;
	union {
		u32 val32;
		u16 val16[2];
	} u;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL) return NULL;
	drv->ctx = ctx;

	drv->fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->fd < 0) {
		wpa_printf(MSG_INFO, "%s: Unable to create socket", __func__);
		os_free(drv);
		return NULL;
	}

	os_memset(&ifv, 0, sizeof(ifv));
	os_strlcpy(ifv.device1, ifname, sizeof(ifv.device1));
	ifv.cmd = GET_VLAN_REALDEV_NAME_CMD;
	if (ioctl(drv->fd, SIOCGIFVLAN, &ifv) >= 0) {
		os_strlcpy(drv->ifname, ifv.u.device2, sizeof(drv->ifname));
		ifv.cmd = GET_VLAN_VID_CMD;
		if (ioctl(drv->fd, SIOCGIFVLAN, &ifv) < 0) {
			perror("ioctl[SIOCGIFVLAN]");
			goto error;
		}
		vlan = ifv.u.VID;
	} else
	if (sscanf(ifname, "vlan%hu", &vlan) == 1) {
		os_strlcpy(drv->ifname, "eth0", sizeof(drv->ifname));
	} else
	if (sscanf(ifname, "%16[^.].%hu", drv->ifname, &vlan) != 2) {
		os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));
		vlan = (u16) -1;
	}

	os_memset(&drv->ifr, 0, sizeof(drv->ifr));
	os_strlcpy(drv->ifr.ifr_name, drv->ifname, IFNAMSIZ);
	if (ioctl(drv->fd, SIOCGMIIPHY, &drv->ifr) < 0) {
		drv->et = 1;
	} else
	if (if_mii(&drv->ifr)->phy_id != ROBO_PHY_ADDR) {
		wpa_printf(MSG_INFO, "%s: Invalid phy address (not a "
			   "RoboSwitch?)", __func__);
		goto error;
	}

	phyid = wpa_driver_roboswitch_mdio_read(drv, 0x03) << 16 |
		wpa_driver_roboswitch_mdio_read(drv, 0x02);
	if (phyid == 0)
		phyid = wpa_driver_roboswitch_mdio_access(drv, 0, 0x03, 0, 0) << 16 |
			wpa_driver_roboswitch_mdio_access(drv, 0, 0x02, 0, 0);
	if (phyid == 0xffffffff || phyid == 0x55210022) {
		wpa_printf(MSG_INFO, "%s: No RoboSwitch in managed "
			   "mode found", __func__);
		goto error;
	}

	/* set and read back to see if the register can be used */
	u.val16[0] = ROBO_VLAN_MAX;
	wpa_driver_roboswitch_write(drv, ROBO_VLAN_PAGE, ROBO_VLAN_ACCESS_5350,
				    &u.val16[0], 1);
	wpa_driver_roboswitch_read(drv, ROBO_VLAN_PAGE, ROBO_VLAN_ACCESS_5350,
				   &u.val16[1], 1);
	if (u.val16[0] == u.val16[1]) {
		drv->model = BCM535x;
		/* dirty trick for 5356/5357 */
		if ((phyid & 0xfff0ffff) == 0x5da00362 ||
		    (phyid & 0xfff0ffff) == 0x5e000362)
			drv->model |= BCM5356;
	} else
		drv->model = BCM536x;

	wpa_printf(MSG_INFO, "%s: RoboSwitch id 0x%x model 0x%x",
			     __func__, phyid, drv->model);

	if (vlan != (u16) -1 &&
	    vlan > ((drv->model == BCM536x) ? ROBO_VLAN_MAX
					    : ROBO_VLAN_MAX_5350)) {
		wpa_printf(MSG_INFO, "%s: VLAN %d out of range on interface "
				     "%s", __func__, vlan, drv->ifname);
		goto error;
	}

	i = (vlan == (u16) -1) ? 0 : vlan;
	for ( ; i <= ((drv->model == BCM536x) ? ROBO_VLAN_MAX
	    				      : ROBO_VLAN_MAX_5350); i++) {
		u.val16[0] = i;
		/* set the read bit */
		u.val16[0] |= (1 << 13);
		wpa_driver_roboswitch_write(drv, ROBO_VLAN_PAGE,
					    (drv->model == BCM536x) ? ROBO_VLAN_ACCESS
								    : ROBO_VLAN_ACCESS_5350,
					    &u.val16[0], 1);
		wpa_driver_roboswitch_read(drv, ROBO_VLAN_PAGE, ROBO_VLAN_READ,
					   &u.val16[0], (drv->model == BCM536x) ? 1 : 2);
		/* is vlan enabled */
		if (drv->model == BCM536x &&
		    u.val16[0] & (1 << 14)) {
			if (vlan != (u16) -1)
				break;
                        if (u.val16[0] & (1 << 5) && u.val16[0] & (1 << 12)) {
				vlan = i;
				break;
			}
		} else
		if (drv->model & BCM535x &&
		    u.val32 & (1 << ((drv->model & BCM5356) ? 24 : 20))) {
			if (vlan != (u16) -1)
				break;
			if (u.val32 & (1 << 5) && u.val32 & (1 << 11)) {
				vlan = i;
				vlan |= (drv->model & BCM5356) ?
					(u.val32 & 0xff000) >> 12 :
					(u.val32 & 0xff000) >> 8;
				break;
			}
		} else
		/* is vlan specified */
		if (vlan != (u16) -1) {
			wpa_printf(MSG_INFO, "%s: Could not get port information for "
					     "VLAN %d", __func__, vlan);
			goto error;
		}
	}

	if (vlan == (u16) -1) {
		wpa_printf(MSG_INFO, "%s: Unable to find VLAN for "
				     "interface %s", __func__, drv->ifname);
		goto error;
	}
	wpa_printf(MSG_DEBUG, "%s: Used VLAN %d ports on RoboSwitch interface "
			      "%s", __func__, vlan, drv->ifname);

	/* even if empty */
	drv->ports = u.val16[0] & 0x001f;
	/* add the MII port */
	drv->ports |= 1 << 8;

	drv->l2 = l2_packet_init(drv->ifname, NULL, ETH_P_EAPOL,
				 wpa_driver_roboswitch_receive, drv,
				 1);
	if (drv->l2 == NULL) {
		wpa_printf(MSG_INFO, "%s: Unable to listen on %s",
			   __func__, drv->ifname);
		goto error;
	}
	l2_packet_get_own_addr(drv->l2, drv->own_addr);

	if (os_strcmp(drv->ifname, ifname) != 0) {
		/* may fail for not existent vlanX or ethX.X */
		drv->l2_vlan = l2_packet_init(ifname, NULL, ETH_P_EAPOL,
					      wpa_driver_roboswitch_receive,
					      drv, 1);
		if (drv->l2_vlan)
			l2_packet_get_own_addr(drv->l2_vlan, drv->own_addr);
	}

	if (wpa_driver_roboswitch_join(drv, drv->ports, pae_group_addr) < 0) {
		wpa_printf(MSG_INFO, "%s: Unable to join PAE group", __func__);
		goto error;
	} else {
		wpa_printf(MSG_DEBUG, "%s: Added PAE group address to "
			   "RoboSwitch ARL", __func__);
	}

	return drv;

error:
	close(drv->fd);
	os_free(drv);
	return NULL;
}


static void wpa_driver_roboswitch_deinit(void *priv)
{
	struct wpa_driver_roboswitch_data *drv = priv;

	if (drv->l2) {
		l2_packet_deinit(drv->l2);
		drv->l2 = NULL;
	}
	if (drv->l2_vlan) {
		l2_packet_deinit(drv->l2_vlan);
		drv->l2_vlan = NULL;
	}
	if (wpa_driver_roboswitch_leave(drv, drv->ports, pae_group_addr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Unable to leave PAE group",
			   __func__);
	}

	close(drv->fd);
	os_free(drv);
}


const struct wpa_driver_ops wpa_driver_roboswitch_ops = {
	.name = "roboswitch",
	.desc = "wpa_supplicant roboswitch driver",
	.get_ssid = wpa_driver_roboswitch_get_ssid,
	.get_bssid = wpa_driver_roboswitch_get_bssid,
	.init = wpa_driver_roboswitch_init,
	.deinit = wpa_driver_roboswitch_deinit,
	.get_mac_addr = wpa_driver_roboswitch_get_mac_addr,
	.send_eapol = wpa_driver_roboswitch_send,
};

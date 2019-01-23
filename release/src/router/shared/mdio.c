/*
 * Copyright 2015, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/mii.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <bcmdevs.h>
#include <net/route.h>
#include <bcmdevs.h>

#include <linux/if_ether.h>
#include <linux/sockios.h>

#include "shared.h"

#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
/**
 * @ifname:
 * @location:
 * @return:
 *  < 0:	invalid parameter or failed.
 *  >= 0:	success, register content
 */
int mdio_read(char *ifname, int location)
{
	int fd, ret = 0;
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;

	if (!ifname || *ifname == '\0') {
		_dprintf("%s: Invalid ifname.\n", __func__);
		return -1;
	}
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -2;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	mii->reg_num = location;
	if (ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		ret = -3;
		goto exit_mdio_read;
	}
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990)
	mii->reg_num = location;
	if (ioctl(fd, SIOCGMIIREG, &ifr) < 0) {
		ret = -4;
		goto exit_mdio_read;
	}
#endif
	ret = mii->val_out;

exit_mdio_read:
	close(fd);

	return ret;
}

/**
 * @ifname:
 * @location:
 * @return:
 *  < 0:	invalid parameter or failed.
 *  = 0:	success
 */
int mdio_write(char *ifname, int location, int value)
{
	int fd, ret = 0;
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;

	if (!ifname || *ifname == '\0' || value < 0) {
		_dprintf("%s: Invalid parameter.\n", __func__);
		return -1;
	}
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -2;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		ret = -3;
		goto exit_mdio_write;
	}
	mii->reg_num = location;
	mii->val_in = value;
	if (ioctl(fd, SIOCSMIIREG, &ifr) < 0) {
		ret = -4;
		goto exit_mdio_write;
	}

exit_mdio_write:
	close(fd);

	return ret;
}

int mdio_phy_speed(char *ifname)
{
	int bmcr, bmcr2, bmsr, advert, lkpar, lpa2;
	int mask, mask2;

	if (!ifname || *ifname == '\0')
		return 0;

	bmcr = mdio_read(ifname, MII_BMCR);
	bmcr2 = mdio_read(ifname, MII_CTRL1000);
	bmsr = mdio_read(ifname, MII_BMSR);
	advert = mdio_read(ifname, MII_ADVERTISE);
	lkpar = mdio_read(ifname, MII_LPA);
	lpa2 = mdio_read(ifname, MII_STAT1000);

	if (!(bmcr & BMCR_ANENABLE)) {
		if (bmcr & BMCR_SPEED1000)
			return 1000;
		else if (bmcr & BMCR_SPEED100)
			return 100;
		else
			return 10;
	}

	if (!(bmsr & BMSR_ANEGCOMPLETE)) {
		// _dprintf("%s: %s autonegotiation restarted\n", __func__, ifname);
		return 0;
	}

	if (!(advert & lkpar)) {
		// _dprintf("%s: %s autonegotiation failed\n", __func__, ifname);
		return 0;
	}

	mask = advert & lkpar;
	mask2 = bmcr2 & (lpa2>>2);
	if (mask2 & (ADVERTISE_1000FULL | ADVERTISE_1000HALF))
		return 1000;
	else if (mask & (ADVERTISE_100FULL | ADVERTISE_100HALF))
		return 100;
	else if (mask & (ADVERTISE_10FULL | ADVERTISE_10HALF))
		return 10;

	return 0;
}
#endif

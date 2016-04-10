/*
 * Copyright (C) 2013 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _BMOCA_H_
#define _BMOCA_H_

#include <linux/if.h>
#include <linux/types.h>
#include <linux/ioctl.h>

/* NOTE: These need to match what is defined in the API template */
#define MOCA_IE_DRV_PRINTF	0xff00
#define MOCA_IE_WDT		0xff01

#define MOCA_BAND_HIGHRF	0
#define MOCA_BAND_MIDRF		1
#define MOCA_BAND_WANRF		2
#define MOCA_BAND_EXT_D		3
#define MOCA_BAND_D_LOW		4
#define MOCA_BAND_D_HIGH	5
#define MOCA_BAND_E		6
#define MOCA_BAND_F		7
#define MOCA_BAND_G		8
#define MOCA_BAND_H		9
#define MOCA_BAND_MAX		10

#define MOCA_BAND_NAMES { \
	"highrf", "midrf", "wanrf", \
	"ext_d", "d_low", "d_high", \
	"e", "f", "g", "h"\
}

#define MOCA_BOOT_FLAGS_BONDED	(1 << 0)

#define MOCA_IOC_MAGIC		'M'

#define MOCA_IOCTL_GET_DRV_INFO_V2	_IOR(MOCA_IOC_MAGIC, 0, \
	struct moca_kdrv_info_v2)

#define MOCA_IOCTL_START	_IOW(MOCA_IOC_MAGIC, 1, struct moca_start)
#define MOCA_IOCTL_STOP		_IO(MOCA_IOC_MAGIC, 2)
#define MOCA_IOCTL_READMEM	_IOR(MOCA_IOC_MAGIC, 3, struct moca_xfer)
#define MOCA_IOCTL_WRITEMEM	_IOR(MOCA_IOC_MAGIC, 4, struct moca_xfer)

#define MOCA_IOCTL_CHECK_FOR_DATA	_IOR(MOCA_IOC_MAGIC, 5, int)
#define MOCA_IOCTL_WOL		_IOW(MOCA_IOC_MAGIC, 6, int)
#define MOCA_IOCTL_GET_DRV_INFO	_IOR(MOCA_IOC_MAGIC, 0, struct moca_kdrv_info)
#define MOCA_IOCTL_SET_CPU_RATE	_IOR(MOCA_IOC_MAGIC, 7, unsigned int)
#define MOCA_IOCTL_SET_PHY_RATE	_IOR(MOCA_IOC_MAGIC, 8, unsigned int)
#define MOCA_IOCTL_GET_3450_REG	_IOR(MOCA_IOC_MAGIC, 9, unsigned int)
#define MOCA_IOCTL_SET_3450_REG	_IOR(MOCA_IOC_MAGIC, 10, unsigned int)
#define MOCA_IOCTL_PM_SUSPEND   _IO(MOCA_IOC_MAGIC, 11)
#define MOCA_IOCTL_PM_WOL	_IO(MOCA_IOC_MAGIC, 12)
#define MOCA_IOCTL_CLK_SSC	_IO(MOCA_IOC_MAGIC, 13)
#define MOCA_IOCTL_TEST         _IO(MOCA_IOC_MAGIC, 14)

#define MOCA_DEVICE_ID_UNREGISTERED  (-1)

/* this must match MoCAOS_IFNAMSIZE */
#define MOCA_IFNAMSIZ		16

/* ID value hinting ioctl caller to use returned IFNAME as is */
#define MOCA_IFNAME_USE_ID    0xffffffff

/* Legacy version of moca_kdrv_info */
struct moca_kdrv_info_v2 {
	__u32			version;
	__u32			build_number;
	__u32			builtin_fw;

	__u32			hw_rev;
	__u32			rf_band;

	__u32			uptime;
	__s32			refcount;
	__u32			gp1;

	__s8			enet_name[MOCA_IFNAMSIZ];
	__u32			enet_id;

	__u32			macaddr_hi;
	__u32			macaddr_lo;

	__u32			phy_freq;
	__u32			device_id;
};

/* this must match MoCAOS_DrvInfo */
struct moca_kdrv_info {
	__u32			version;
	__u32			build_number;
	__u32			builtin_fw;

	__u32			hw_rev;
	__u32			rf_band;

	__u32			uptime;
	__s32			refcount;
	__u32			gp1;

	__s8			enet_name[MOCA_IFNAMSIZ];
	__u32			enet_id;

	__u32			macaddr_hi;
	__u32			macaddr_lo;

	__u32			phy_freq;
	__u32			device_id;

	__u32			chip_id;
};

struct moca_xfer {
	__u64			buf;
	__u32			len;
	__u32			moca_addr;
};

struct moca_start {
	struct moca_xfer	x;
	__u32			boot_flags;
};

/* MoCA PM states */
enum moca_pm_states {
	MOCA_ACTIVE,
	MOCA_SUSPENDING,
	MOCA_SUSPENDING_WAITING_ACK,
	MOCA_SUSPENDING_GOT_ACK,
	MOCA_SUSPENDED,
	MOCA_RESUMING,
	MOCA_NONE
};

#ifdef __KERNEL__

static inline void mac_to_u32(uint32_t *hi, uint32_t *lo, const uint8_t *mac)
{
	*hi = (mac[0] << 24) | (mac[1] << 16) | (mac[2] << 8) | (mac[3] << 0);
	*lo = (mac[4] << 24) | (mac[5] << 16);
}

static inline void u32_to_mac(uint8_t *mac, uint32_t hi, uint32_t lo)
{
	mac[0] = (hi >> 24) & 0xff;
	mac[1] = (hi >> 16) & 0xff;
	mac[2] = (hi >>  8) & 0xff;
	mac[3] = (hi >>  0) & 0xff;
	mac[4] = (lo >> 24) & 0xff;
	mac[5] = (lo >> 16) & 0xff;
}

struct moca_platform_data {
	char			enet_name[IFNAMSIZ];
	unsigned int		enet_id;

	u32			macaddr_hi;
	u32			macaddr_lo;

	phys_addr_t		bcm3450_i2c_base;
	int			bcm3450_i2c_addr;

	u32			hw_rev;  /* this is the chip_id */
	u32			rf_band;

	int			use_dma;
	int			use_spi;
  int     devId;

	u32			chip_id;
};

enum {
	HWREV_MOCA_11		= 0x1100,
	HWREV_MOCA_11_LITE	= 0x1101,
	HWREV_MOCA_11_PLUS	= 0x1102,
	HWREV_MOCA_20_ALT	= 0x2000, /* for backward compatibility */
	HWREV_MOCA_20_GEN21	= 0x2001,
	HWREV_MOCA_20_GEN22	= 0x2002,
	HWREV_MOCA_20_GEN23	= 0x2003,
};


#define MOCA_PROTVER_11		0x1100
#define MOCA_PROTVER_20		0x2000
#define MOCA_PROTVER_MASK	0xff00

#endif /* __KERNEL__ */

#endif /* ! _BMOCA_H_ */

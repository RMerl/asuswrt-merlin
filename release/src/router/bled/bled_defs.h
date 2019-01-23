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
 *
 * Copyright 2014, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
#ifndef _BLED_DEFS_H_
#define _BLED_DEFS_H_

#include <rtconfig.h>

#if defined(__KERNEL__)
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>
#else
#ifndef IFNAMSIZ
#define	IFNAMSIZ	16
#endif
#endif

#define BLED_NAME		"bled"
#define BLED_DEVNAME	"/dev/" BLED_NAME

#define BLED_MAX_NR_NETDEV_IF		(8)
#define BLED_MAX_NR_SWPORTS		(8)
#define BLED_MAX_NR_USBBUS		(8)
#define BLED_MAX_NR_INTERRUPT		(8)

enum bled_state {
	BLED_STATE_STOP = 0,
	BLED_STATE_RUN,

	BLED_STATE_MAX
};

enum bled_bh_type {
	BLED_BHTYPE_TIMER = 0,			/* Use timer to handle bled. */
	BLED_BHTYPE_HYBRID,			/* Use workqueue to check traffic instead. (If check function needs to sleep, use this.)*/

	BLED_BHTYPE_MAX
};

enum bled_type {
	BLED_TYPE_NETDEV_BLED = 0,		/* interface(s). */
	BLED_TYPE_SWPORTS_BLED,			/* switch port(s). */
	BLED_TYPE_USBBUS_BLED,			/* USB bus(s). */
	BLED_TYPE_INTERRUPT_BLED,		/* interrupt(s) */

	BLED_TYPE_MAX
};

enum bled_mode {
	BLED_NORMAL_MODE = 0,			/* Normal mode. e.g., NETDEV_BLED, SWPORTS_BLED, or USBBUS_BLED. */
	BLED_UDEF_PATTERN_MODE,			/* User defined pattern mode. */

	BLED_MODE_MAX
};

enum gpio_api {
	GPIO_API_PLATFORM = 0,

	GPIO_API_MAX
};

#define BLED_MAX_NR_PATTERN		(100)
#define BLED_UDEF_PATTERN_MIN_INTERVAL	(50)	/* unit: ms */
#define BLED_UDEF_PATTERN_MAX_INTERVAL	(3000)	/* unit: ms */


struct bled_common {
	unsigned int gpio_nr;
	int active_low;
	enum gpio_api gpio_api_id;
	enum bled_state state;
	enum bled_bh_type bh_type;
	enum bled_mode mode;
	unsigned int min_blink_speed;	/* unit: KB/s */
	unsigned int interval;		/* unit: ms */
	unsigned int pattern_interval;	/* unit: ms (BLED_UDEF_PATTERN_MIN_INTERVAL ~ BLED_UDEF_PATTERN_MAX_INTERVAL) */
	unsigned int nr_pattern;	/* number of valid item in pattern[] */
	unsigned int pattern[BLED_MAX_NR_PATTERN];	/* 0: Turn off LED; otherwise: Turn on LED */
};

struct ndev_bled {
	struct bled_common bled;
	char ifname[IFNAMSIZ];
};

struct swport_bled {
	struct bled_common bled;
	unsigned int port_mask;		/* bit0=port0, bit1=port1, etc. port0~BLED_MAX_NR_SWPORTS-1 only */
};

struct usbbus_bled {
	struct bled_common bled;
	unsigned int bus_mask;		/* bit0=bus1, bit1=bus2, etc. bus1~BLED_MAX_NR_USBBUS only */
};

struct interrupt_bled {
	struct bled_common bled;
	unsigned int nr_interrupt;
	unsigned int interrupt[BLED_MAX_NR_INTERRUPT];	/* interrupt number. */
};

#define BLED_CTL_CHG_STATE		_IOW('B', 0, struct bled_common)	/* gpio_nr, state */
#define BLED_CTL_ADD_NETDEV_BLED	_IOW('B', 1, struct ndev_bled)		/* all fields; except mode, state */
#define BLED_CTL_DEL_BLED		_IOW('B', 2, struct bled_common)	/* gpio_nr */
#define BLED_CTL_ADD_NETDEV_IF		_IOW('B', 3, struct ndev_bled)		/* gpio_nr, ifname */
#define BLED_CTL_DEL_NETDEV_IF		_IOW('B', 4, struct ndev_bled)		/* gpio_nr, ifname */
#define BLED_CTL_ADD_SWPORTS_BLED	_IOW('B', 5, struct swport_bled)	/* all fields; except mode, state */
#define BLED_CTL_UPD_SWPORTS_MASK	_IOW('B', 6, struct swport_bled)	/* gpio_nr, port_mask */
#define BLED_CTL_ADD_USBBUS_BLED	_IOW('B', 7, struct usbbus_bled)	/* all fields; except mode, state */
#define BLED_CTL_GET_BLED_TYPE		_IOWR('B', 8, int)			/* input: gpio_nr; output: enum bled_type */
#define BLED_CTL_SET_UDEF_PATTERN	_IOW('B', 9, struct bled_common)	/* gpio_nr, pattern_interval, nr_pattern, pattern[] */
#define BLED_CTL_SET_MODE		_IOW('B',10, struct bled_common)	/* gpio_nr, mode */
#define BLED_CTL_ADD_INTERRUPT_BLED	_IOW('B',11, struct interrupt_bled)	/* all fields; except mode, state */

#define BLED_TIMER_CHECK_INTERVAL	(HZ / 5)				/* unit: jiffies */
#define BLED_HYBRID_CHECK_INTERVAL	(HZ / 2)				/* unit: jiffies */

/* If interface doesn't exist, check it per BLED_WAIT_IF_INTERVAL. */
#define BLED_WAIT_IF_INTERVAL		(5 * HZ)

#if defined(__KERNEL__)

//#define DEBUG_BLED
#define prn_bl_v(fmt, args...)		if (printk_ratelimit()) printk("bled: " fmt, ##args)
#if defined(DEBUG_BLED)
#define dbg_bl(fmt, args...)		printk("bled: " fmt, ##args)
#define dbg_bl_v(fmt, args...)		if (printk_ratelimit()) printk("bled: " fmt, ##args)
#else
#define dbg_bl(fmt, args...)		do {} while (0)
#define dbg_bl_v(fmt, args...)		do {} while (0)
#endif

#define BLED_FLAGS_DBG_CHECK_FUNC	(1U << 0)

#define TFMT	"%-30s: "

struct ndev_bled_ifstat {
	char ifname[IFNAMSIZ];
	unsigned long last_rx_bytes;
	unsigned long last_tx_bytes;
};

struct ndev_bled_priv {
	unsigned int nr_if;
	struct ndev_bled_ifstat ifstat[BLED_MAX_NR_NETDEV_IF];
};

struct swport_bled_ifstat {
	unsigned long last_rx_bytes;
	unsigned long last_tx_bytes;
};

struct swport_bled_priv {
	unsigned int nr_port;
	unsigned int port_mask;
	struct swport_bled_ifstat portstat[BLED_MAX_NR_SWPORTS];
	struct ndev_bled_priv ndev_priv;
};

struct usbbus_bled_stat {
	unsigned long last_rx_bytes;
	unsigned long last_tx_bytes;
};

struct usbbus_bled_priv {
	unsigned int nr_bus;
	unsigned int bus_mask;
	struct usbbus_bled_stat busstat[BLED_MAX_NR_USBBUS];
};

struct interrupt_bled_stat {
	unsigned int interrupt;
	unsigned long last_nr_interrupts;
};

struct interrupt_bled_priv {
	unsigned int nr_interrupt;
	struct interrupt_bled_stat interrupt_stat[BLED_MAX_NR_USBBUS];
};

struct udef_pattern_s {
	unsigned int curr, nr_pattern;
	unsigned char value[BLED_MAX_NR_PATTERN];		/* 0: Turn off LED; 1: Turn on LED. */
	unsigned long interval[BLED_MAX_NR_PATTERN];		/* pattern interval in jiffies. */
};

struct bled_priv {
	struct list_head list;
	unsigned int gpio_nr;
	int active_low;
	enum bled_state state;
	enum bled_type type;
	enum bled_bh_type bh_type;
	enum bled_mode mode;
	unsigned int value;					/* 0: LED OFF; 1: LED on */
	unsigned long next_blink_interval;			/* unit: jiffies */
	unsigned long next_blink_ts;				/* unit: jiffies */
	unsigned long next_check_interval;			/* unit: jiffies */
	unsigned long next_check_ts;				/* unit: jiffies */
	unsigned long threshold;				/* unit: bytes */
	unsigned long interval;					/* unit: jiffies */
	unsigned int flags;
	struct udef_pattern_s udef_pattern;			/* User defined pattern. */

	struct timer_list timer;
	struct delayed_work bled_work;
	
	void (*gpio_set) (int gpio_nr, int value);
	int (*gpio_get) (int gpio_nr);

	unsigned int (*check) (struct bled_priv *bled_priv);
	unsigned int (*check2) (struct bled_priv *bled_priv);
	int (*reset_check) (struct bled_priv *bled_priv);
	int (*reset_check2) (struct bled_priv *bled_priv);
	void *check_priv, *check2_priv;
};

static inline unsigned long bdiff(unsigned long b1, unsigned long b2)
{
	if (likely(b2 >= b1))
		return b2 - b1;
	else
		return (ULONG_MAX - b1 + 1 + b2);
}

#endif	/* __KERNEL__ */

#endif	/* !_BLED_DEFS_H_ */


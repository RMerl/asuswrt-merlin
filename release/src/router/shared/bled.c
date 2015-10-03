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
#include <rtconfig.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <bcmnvram.h>
#include <shared.h>
#include <bled_defs.h>
#include <stdio.h>
#include "shutils.h"

#if defined(RTCONFIG_BLINK_LED)
/**
 * Add/Strip GPIO_BLINK_LED flag to specified led_xxx_gpio.
 * @led_gpio:	pointer to name of "led_xxx_gpio"
 * @return:
 *  	0:	success
 *  otherwisze:	fail
 */
static inline int __update_gpio_nv_var(const char *led_gpio, int add)
{
	char *p;

	if (!led_gpio || !(p = nvram_get(led_gpio)))
		return -1;

	if (add)
		nvram_set_int(led_gpio, atoi(p) | GPIO_BLINK_LED);
	else
		nvram_set_int(led_gpio, atoi(p) & ~GPIO_BLINK_LED);
	return 0;
}

/**
 * Low-level function to add a LED which blinks in accordance with traffic of net_device
 * which is specified by ifname.
 * @led_gpio:		pointer to name of "led_xxx_gpio"
 * @ifname:		interface name, e.g., br0, vlan2, eth0, etc.
 * @min_blink_speeed:	blink led if speed greater than or equal to min_blink_speeed. (unit: KBytes/s)
 * @interval:		blinking interval. (unit: ms)
 * @return:
 */
int __config_netdev_bled(const char *led_gpio, const char *ifname, unsigned int min_blink_speed, unsigned int interval)
{
	int gpio_nr, fd, r, ret = 0;
	struct ndev_bled nl;
	struct bled_common *bl = &nl.bled;

	if (!ifname || *ifname == '\0')
		return -1;

	if (__update_gpio_nv_var(led_gpio, 1) < 0)
		return -2;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0) {
		ret = -3;
		goto exit___config_netdev_bled;
	}

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		ret = -4;
		goto exit___config_netdev_bled;
	}

	memset(&nl, 0, sizeof(nl));
	bl->gpio_nr = gpio_nr;
	bl->gpio_api_id = GPIO_API_PLATFORM;
	bl->active_low = !!(nvram_get_int(led_gpio) & GPIO_ACTIVE_LOW);
	bl->state = BLED_STATE_STOP;
	bl->bh_type = BLED_BHTYPE_TIMER;
	bl->mode = BLED_NORMAL_MODE;
	bl->min_blink_speed = min_blink_speed;
	bl->interval = interval;
	strcpy(nl.ifname, ifname);

	if ((r = ioctl(fd, BLED_CTL_ADD_NETDEV_BLED, &nl)) < 0 && errno != EEXIST) {
		_dprintf("%s: ioctl(BLED_CTL_ADD_NETDEV_BLED) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		ret = -5;
		goto exit___config_netdev_bled;
	}

	close(fd);

	return 0;

exit___config_netdev_bled:
	__update_gpio_nv_var(led_gpio, 0);
	return ret;
}

/**
 * Simpler interface to add a LED which blinks in accordance with traffic of net_device
 * which is specified by ifname.
 * @led_gpio:	pointer to name of "led_xxx_gpio"
 * @ifname:	interface name, e.g., br0, vlan2, eth0, etc.
 * @return:
 */
int config_netdev_bled(const char *led_gpio, const char *ifname)
{
	unsigned int min_blink_speed = 10;	/* KB/s */
	unsigned int interval = 100;		/* ms */

	return __config_netdev_bled(led_gpio, ifname, min_blink_speed, interval);
}

/**
 * Set user defined pattern to a bled.
 * @led_gpio:	pointer to name of "led_xxx_gpio", the bled must be configured first.
 * 		e.g., config_netdev_bled(), config_swports_bled(), and config_usbbus_bled()
 * @interval:	length of each item in @pattern. (unit: ms)
 * 		BLED_UDEF_PATTERN_MIN_INTERVAL ~ BLED_UDEF_PATTERN_MAX_INTERVAL
 * @pattern:	user defined pattern. e.g., "1 0 1 1 0 0"
 * 		0: turn off LED.
 * 		otherwise: turn on LED.
 * @return:
 */
int set_bled_udef_pattern(const char *led_gpio, unsigned int interval, const char *pattern)
{
	int gpio_nr, fd, r, ret = 0;
	char *str, *p, *token;
	struct bled_common bl_common, *bl = &bl_common;
	unsigned int curr, *patt = &bl->pattern[0];

	if (!led_gpio || *led_gpio == '\0' || !(nvram_get_int(led_gpio) & GPIO_BLINK_LED) ||
	    interval < BLED_UDEF_PATTERN_MIN_INTERVAL || interval > BLED_UDEF_PATTERN_MAX_INTERVAL ||
	    !pattern || *pattern == '\0')
		return -1;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0) {
		ret = -2;
		goto exit_set_bled_udef_pattern;
	}

	if (!(str = strdup(pattern))) {
		ret = -3;
		goto exit_set_bled_udef_pattern;
	}
	memset(bl, 0, sizeof(*bl));
	bl->gpio_nr = gpio_nr;
	bl->pattern_interval = interval;
	for (curr = 0, p = str; curr < BLED_MAX_NR_PATTERN; curr++, p = NULL) {
		if (!(token = strtok(p, " ")))
			break;
		*patt++ = !!atoi(token);
	}
	if (!curr) {
		_dprintf("%s: %s: pattern not found.\n", __func__, led_gpio);
		ret = -4;
		goto exit_set_bled_udef_pattern;
	}
	bl->nr_pattern = curr;

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		ret = -5;
		goto exit_set_bled_udef_pattern;
	}

	if ((r = ioctl(fd, BLED_CTL_SET_UDEF_PATTERN, bl)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_SET_UDEF_PATTERN) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		ret = -6;
		goto exit_set_bled_udef_pattern;
	}

	close(fd);
	free(str);

exit_set_bled_udef_pattern:

	return ret;
}

/**
 * Set operation mode of a bled.
 * @led_gpio:	pointer to name of "led_xxx_gpio", the bled must be configured first.
 * 		e.g., config_netdev_bled(), config_swports_bled(), and config_usbbus_bled()
 * @mode:	one of bled_mode enumeration, except BLED_MODE_MAX
 * @return:
 */
static int __set_bled_mode(const char *led_gpio, enum bled_mode bled_mode)
{
	int gpio_nr, fd, r, ret = 0;
	struct bled_common bl_common, *bl = &bl_common;

	if (!led_gpio || *led_gpio == '\0' || !(nvram_get_int(led_gpio) & GPIO_BLINK_LED) ||
	    bled_mode < 0 || bled_mode >= BLED_MODE_MAX)
		return -1;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0) {
		ret = -2;
		goto exit___set_bled_mode;
	}

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		ret = -3;
		goto exit___set_bled_mode;
	}

	memset(bl, 0, sizeof(*bl));
	bl->gpio_nr = gpio_nr;
	bl->mode = bled_mode;

	if ((r = ioctl(fd, BLED_CTL_SET_MODE, bl)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_SET_MODE) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		ret = -4;
		goto exit___set_bled_mode;
	}

	close(fd);

exit___set_bled_mode:

	return ret;
}

int set_bled_normal_mode(const char *led_gpio)
{
	return __set_bled_mode(led_gpio, BLED_NORMAL_MODE);
}

int set_bled_udef_pattern_mode(const char *led_gpio)
{
	return __set_bled_mode(led_gpio, BLED_UDEF_PATTERN_MODE);
}

/**
 * Low-level function to change state of bled.
 * @gpio_nr:	GPIO# of a LED
 * @state:
 * @return:
 */
static int chg_bled_state(unsigned int gpio_nr, enum bled_state state)
{
	int fd, r;
	struct bled_common bled;

	if (!(gpio_nr & GPIO_BLINK_LED))
		return 0;
	gpio_nr &= 0xFF;
	if (gpio_nr == 0xFF)
		return -1;
	if (state >= BLED_STATE_MAX || state < 0)
		return -2;

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		return -3;
	}

	memset(&bled, 0, sizeof(bled));
	bled.gpio_nr = gpio_nr;
	bled.state = state;
	if ((r = ioctl(fd, BLED_CTL_CHG_STATE, &bled)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_CHG_STATE) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		return -4;
	}
	close(fd);

	return 0;
}

/**
 * Request bled kernel module to start blink specified LED in accordance with checking result.
 * @gpio_nr:	GPIO# of a LED, include flags. (GPIO_BLINK_LED)
 * @return:
 */
int start_bled(unsigned int gpio_nr)
{
	return chg_bled_state(gpio_nr, BLED_STATE_RUN);
}

/**
 * Request bled kernel module to stop blink specified LED.
 * @gpio_nr:	GPIO# of a LED, include flags. (GPIO_BLINK_LED)
 * @return:
 */
int stop_bled(unsigned int gpio_nr)
{
	return chg_bled_state(gpio_nr, BLED_STATE_STOP);
}

/**
 * Delete a bled.
 * @gpio_nr:	GPIO# of a LED
 * @return:
 */
int del_bled(unsigned int gpio_nr)
{
	int fd, r;
	struct bled_common bled;

	gpio_nr &= 0xFF;
	if (gpio_nr == 0xFF)
		return -1;

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0)
		return -2;

	memset(&bled, 0, sizeof(bled));
	bled.gpio_nr = gpio_nr;
	if ((r = ioctl(fd, BLED_CTL_DEL_BLED, &bled)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_DEL_BLED) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		return -4;
	}
	close(fd);

	return 0;
}

/**
 * Append a new interface to a exist bled.
 * @led_gpio:	pointer to name of "led_xxx_gpio"
 * @ifname:	interface name, e.g., br0, vlan2, eth0, etc.
 * @return:
 */
int append_netdev_bled_if(const char *led_gpio, const char *ifname)
{
	int gpio_nr, fd, r;
	struct ndev_bled nl;
	struct bled_common *bl = &nl.bled;

	if (!led_gpio || *led_gpio == '\0' || !ifname || *ifname == '\0')
		return -1;

	if (!(nvram_get_int(led_gpio) & GPIO_BLINK_LED))
		return -2;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0)
		return -3;

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		return -4;
	}

	memset(&nl, 0, sizeof(nl));
	bl->gpio_nr = gpio_nr;
	strcpy(nl.ifname, ifname);

	if ((r = ioctl(fd, BLED_CTL_ADD_NETDEV_IF, &nl)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_ADD_NETDEV_IF) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		return -5;
	}

	close(fd);

	return 0;
}

/**
 * Remove a interface from a exist bled.
 * @led_gpio:	pointer to name of "led_xxx_gpio"
 * @ifname:	interface name, e.g., br0, vlan2, eth0, etc.
 * @return:
 */
int remove_netdev_bled_if(const char *led_gpio, const char *ifname)
{
	int gpio_nr, fd, r;
	struct ndev_bled nl;
	struct bled_common *bl = &nl.bled;

	if (!led_gpio || *led_gpio == '\0' || !ifname || *ifname == '\0')
		return -1;

	if (!(nvram_get_int(led_gpio) & GPIO_BLINK_LED))
		return -2;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0)
		return -3;

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		return -4;
	}

	memset(&nl, 0, sizeof(nl));
	bl->gpio_nr = gpio_nr;
	strcpy(nl.ifname, ifname);

	if ((r = ioctl(fd, BLED_CTL_DEL_NETDEV_IF, &nl)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_DEL_NETDEV_IF) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		return -5;
	}

	close(fd);

	return 0;
}

/**
 * Low-level function to add a LED which blinks in accordance with traffic of switch port(s).
 * which is specified by ifname.
 * @led_gpio:		pointer to name of "led_xxx_gpio"
 * @port_mask:		switch port mask
 * @min_blink_speeed:	blink led if speed greater than or equal to min_blink_speeed. (unit: KBytes/s)
 * @interval:		blinking interval. (unit: ms)
 * @sleep:		Run check function in process context or not.
 * 			If platform-specific switch's check function needs to take mutex or semaphore,
 * 			sleep variable must be non-zero value.
 * @return:
 */
int __config_swports_bled(const char *led_gpio, unsigned int port_mask, unsigned int min_blink_speed, unsigned int interval, int sleep)
{
	int gpio_nr, fd, r, ret = 0;
	struct swport_bled sl;
	struct bled_common *bl = &sl.bled;

	if (__update_gpio_nv_var(led_gpio, 1) < 0)
		return -1;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0) {
		ret = -2;
		goto exit___config_swports_bled;
	}

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		ret = -3;
		goto exit___config_swports_bled;
	}

	memset(&sl, 0, sizeof(sl));
	bl->gpio_nr = gpio_nr;
	bl->gpio_api_id = GPIO_API_PLATFORM;
	bl->active_low = !!(nvram_get_int(led_gpio) & GPIO_ACTIVE_LOW);
	bl->state = BLED_STATE_STOP;
	bl->bh_type = sleep? BLED_BHTYPE_HYBRID:BLED_BHTYPE_TIMER;
	bl->mode = BLED_NORMAL_MODE;
	bl->min_blink_speed = min_blink_speed;
	bl->interval = interval;
	sl.port_mask = port_mask;

	if ((r = ioctl(fd, BLED_CTL_ADD_SWPORTS_BLED, &sl)) < 0 && errno != EEXIST) {
		_dprintf("%s: ioctl(BLED_CTL_ADD_NETDEV_BLED) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		ret = -4;
		goto exit___config_swports_bled;
	}

	close(fd);

	return 0;

exit___config_swports_bled:

	__update_gpio_nv_var(led_gpio, 0);
	return ret;
}

/**
 * Update port mask of a bled
 * @led_gpio:	pointer to name of "led_xxx_gpio"
 * @port_mask:	port bit-mask
 * @return:
 */
int update_swports_bled(const char *led_gpio, unsigned int port_mask)
{
	int gpio_nr, fd, r;
	struct swport_bled sl;
	struct bled_common *bl = &sl.bled;

	if (!led_gpio || *led_gpio == '\0')
		return -1;

	if (!(nvram_get_int(led_gpio) & GPIO_BLINK_LED))
		return -2;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0)
		return -3;

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		return -4;
	}

	memset(&sl, 0, sizeof(sl));
	bl->gpio_nr = gpio_nr;
	sl.port_mask = port_mask;

	if ((r = ioctl(fd, BLED_CTL_UPD_SWPORTS_MASK, &sl)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_UPD_SWPORTS_MASK) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		return -5;
	}

	close(fd);

	return 0;
}

/**
 * Low-level function to add a LED which blinks in accordance with traffic of usb bus(s)
 * which is specified by bus_mask.
 * @led_gpio:		pointer to name of "led_xxx_gpio"
 * @bus_list:		bus list
 * @min_blink_speeed:	blink led if speed greater than or equal to min_blink_speeed. (unit: KBytes/s)
 * @interval:		blinking interval. (unit: ms)
 * @sleep:		Run check function in process context or not.
 * 			If platform-specific switch's check function needs to take mutex or semaphore,
 * 			sleep variable must be non-zero value.
 * @return:
 */
int __config_usbbus_bled(const char *led_gpio, char *bus_list, unsigned int min_blink_speed, unsigned int interval)
{
	int gpio_nr, fd, r, b, ret = 0;
	char word[100], *next;
	unsigned int bus_mask = 0;
	struct usbbus_bled ul;
	struct bled_common *bl = &ul.bled;

	if (__update_gpio_nv_var(led_gpio, 1) < 0)
		return -1;

	if ((gpio_nr = extract_gpio_pin(led_gpio)) < 0) {
		ret = -2;
		goto exit___config_usbbus_bled;
	}

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		ret = -3;
		goto exit___config_usbbus_bled;
	}

	foreach(word, bus_list, next) {
		b = atoi(word) - 1;	/* bit0 = bus1 */
		if (b < 0)
			continue;
		bus_mask |= (1U << b);
	}

	memset(&ul, 0, sizeof(ul));
	bl->gpio_nr = gpio_nr;
	bl->gpio_api_id = GPIO_API_PLATFORM;
	bl->active_low = !!(nvram_get_int(led_gpio) & GPIO_ACTIVE_LOW);
	bl->state = BLED_STATE_STOP;
	bl->bh_type = BLED_BHTYPE_TIMER;
	bl->mode = BLED_NORMAL_MODE;
	bl->min_blink_speed = min_blink_speed;
	bl->interval = interval;
	ul.bus_mask = bus_mask;

	if ((r = ioctl(fd, BLED_CTL_ADD_USBBUS_BLED, &ul)) < 0 && errno != EEXIST) {
		_dprintf("%s: ioctl(BLED_CTL_ADD_USBBUS_BLED) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		ret = -4;
		goto exit___config_usbbus_bled;
	}

	close(fd);

	return 0;

exit___config_usbbus_bled:

	__update_gpio_nv_var(led_gpio, 0);
	return ret;
}

/**
 * Return bled type.
 * @led_gpio:	pointer to name of "led_xxx_gpio"
 * @return:
 *  >= 0:	enum bled_type
 *  <  0:	fail
 */
static int get_bled_type(const char *led_gpio)
{
	int fd, param, r, v;

	if (!led_gpio || *led_gpio == '\0')
		return -1;
	v = nvram_get_int(led_gpio);
	if (v == 0xFF || !(v & GPIO_BLINK_LED))
		return -2;
	if ((param = extract_gpio_pin(led_gpio)) < 0)
		return -3;

	if ((fd = open(BLED_DEVNAME, O_RDWR)) < 0) {
		_dprintf("%s: open %s fail. (%s)\n", __func__, BLED_DEVNAME, strerror(errno));
		return -4;
	}

	if ((r = ioctl(fd, BLED_CTL_GET_BLED_TYPE, &param)) < 0) {
		_dprintf("%s: ioctl(BLED_CTL_GET_BLED_TYPE) fail, return %d errno %d (%s)\n",
			__func__, r, errno, strerror(errno));
		close(fd);
		return -5;
	}
	close(fd);

	return param;
}

/**
 * Checks whether a led_gpio is swports_bled or not.
 * @led_gpio:	pointer to name of "led_xxx_gpio"
 * @return:
 * 	0:	not swports bled
 *  otherwise:	swports bled
 */
int is_swports_bled(const char *led_gpio)
{
	return (get_bled_type(led_gpio) == BLED_TYPE_SWPORTS_BLED)? 1:0;
}


/*
 * model dependence
 */
#if (defined(PLN12) || defined(PLAC56))
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ary) (sizeof(ary) / sizeof((ary)[0]))
#endif

void set_wifiled(int mode)
{
	int i;
	struct wifi_led {
		char *gpio[2];
	} p = {
		.gpio = {
#if defined(PLN12)
			"led_2g_red_gpio",
			NULL
#elif defined(PLAC56)
			"led_2g_green_gpio",
			"led_5g_green_gpio"
#endif
		}
	};

	switch (mode) {
	case 1: /* on */
		for (i = 0; i < ARRAY_SIZE(p.gpio); ++i) {
			if (!p.gpio[i])
				break;

			set_bled_udef_pattern(p.gpio[i], 1000, "1 1");
			set_bled_udef_pattern_mode(p.gpio[i]);
		}
		break;
	case 2: /* off */
		for (i = 0; i < ARRAY_SIZE(p.gpio); ++i) {
			if (!p.gpio[i])
				break;

			set_bled_udef_pattern(p.gpio[i], 1000, "0 0");
			set_bled_udef_pattern_mode(p.gpio[i]);
		}
		break;
	case 3:	/* WPS */
		for (i = 0; i < ARRAY_SIZE(p.gpio); ++i) {
			if (!p.gpio[i])
				break;

			set_bled_udef_pattern(p.gpio[i], 50, "0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0");
			set_bled_udef_pattern_mode(p.gpio[i]);
		}
		break;
	case 4: /* WPS success */
		for (i = 0; i < ARRAY_SIZE(p.gpio); ++i) {
			if (!p.gpio[i])
				break;

			set_bled_udef_pattern(p.gpio[i], 100, "0 1");
			set_bled_udef_pattern_mode(p.gpio[i]);
		}
		break;
	case 5: /* press reset button */
		for (i = 0; i < ARRAY_SIZE(p.gpio); ++i) {
			if (!p.gpio[i])
				break;

			set_bled_udef_pattern(p.gpio[i], 500, "0 1");
			set_bled_udef_pattern_mode(p.gpio[i]);
		}
		break;
	case 6: /* firmware upgrade */
		for (i = 0; i < ARRAY_SIZE(p.gpio); ++i) {
			if (!p.gpio[i])
				break;

			set_bled_udef_pattern(p.gpio[i], 1000, "0 1");
			set_bled_udef_pattern_mode(p.gpio[i]);
		}
		break;
	default:
		/* normal mode */
		for (i = 0; i < ARRAY_SIZE(p.gpio); ++i) {
			if (!p.gpio[i])
				break;

			set_bled_normal_mode(p.gpio[i]);
		}
	}
}
#endif
#endif	/* RTCONFIG_BLINK_LED */

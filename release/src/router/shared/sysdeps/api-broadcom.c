#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bcmnvram.h>
#include <bcmdevs.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <wlutils.h>
#include <linux_gpio.h>
#include <etioctl.h>
#include "utils.h"
#include "shutils.h"
#include "shared.h"

#define swapportstatus(x) \
{ \
    unsigned int data = *(unsigned int*)&(x); \
    data = ((data & 0x000c0000) >> 18) |    \
           ((data & 0x00030000) >> 14) |    \
           ((data & 0x0000c000) >> 10) |    \
           ((data & 0x00003000) >>  6) |    \
	   ((data & 0x00000c00) >>  2);     \
    *(unsigned int*)&(x) = data;            \
}

uint32_t get_gpio(uint32_t gpio)
{
	uint32_t bit_value;
	uint32_t bit_mask;

	bit_mask = 1 << gpio;
	bit_value = gpio_read()&bit_mask;

	return bit_value == 0 ? 0 : 1;
}


uint32_t set_gpio(uint32_t gpio, uint32_t value)
{
	uint32_t bit;
/*
	bit = 1<< gpio;

	_dprintf("set_gpio: %d %d\n", bit, value);
	gpio_write(bit, value);
*/
	//_dprintf("set_gpio: %d %d\n", gpio, value);
	gpio_write(gpio, value);

	return 0;
}

// !0: connected
//  0: disconnected
uint32_t get_phy_status(uint32_t portmask)
{
	int fd, i, vecarg[2];
	struct ifreq ifr;
	uint32_t mask = 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0"); // is it always the same?
	ifr.ifr_data = (caddr_t) vecarg;

	for (i = 0; i < 5 && (portmask >> i); i++) {
		vecarg[0] = (i << 16) | 0x01;
		vecarg[1] = 0;
		if ((portmask & (1U << i)) == 0)
			continue;

		if (ioctl(fd, SIOCGETCPHYRD2, (caddr_t)&ifr) < 0)
			continue;
		/* link is down, but negotiation has started
		 * read register again, use previous value, if failed */
		if ((vecarg[1] & 0x22) == 0x20)
			ioctl(fd, SIOCGETCPHYRD2, (caddr_t)&ifr);

		if (vecarg[1] & (1U << 2))
			mask |= (1U << i);
	}
	close(fd);

	//_dprintf("get_phy_status %x %x\n", mask, portmask);

	return mask;
}

// 2bit per port (0-4(5)*2 shift)
// 0: 10 Mbps
// 1: 100 Mbps
// 2: 1000 Mbps
uint32_t get_phy_speed(uint32_t portmask)
{
	int fd, tmp;
	struct ifreq ifr;
	// page 0x01 and status register 0x04
	int vecarg[2] = { 0x01 << 16 | 0x04, 0 };

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0"); // is it always the same?
	ifr.ifr_data = (caddr_t) vecarg;

	if (ioctl(fd, SIOCGETCROBORD, (caddr_t)&ifr) < 0)
		vecarg[1] = 0;
	close(fd);

	/* 53115/53125, 2bit: 0=10 Mbps, 1=100Mbps, 2=1000Mbps */
	/* 5325E/535x,  1bit: 0=10 Mbps, 1=100Mbps */
	tmp = get_model();
	if (tmp != MODEL_RTN66U && tmp != MODEL_RTAC66U && tmp != MODEL_RTN16 &&
	    tmp != MODEL_RTN15U)
	{
		for (tmp = 0; vecarg[1]; vecarg[1] >>= 1) {
			tmp |= (vecarg[1] & 0x01);
			tmp <<= 2;
		}
		swapportstatus(tmp);
		vecarg[1] = tmp;
	}

	//_dprintf("get_phy_speed %x %x\n", vecarg[1], portmask);

	return (vecarg[1] & portmask);
}

uint32_t set_phy_ctrl(uint32_t portmask, uint32_t ctrl)
{
	int fd, i, vecarg[2];
	int model;
	struct ifreq ifr;
	uint32_t reg, mask, off;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0"); // is it always the same?
	ifr.ifr_data = (caddr_t) vecarg;

	model = get_model();
	/* 53115/53125E */
	/* TODO: check 5356,5357 models as they have same regs according SDK */
	if (model == MODEL_RTN16 || model == MODEL_RTN15U || model == MODEL_RTN66U || model == MODEL_RTAC66U) {
		reg = 0x00;
		mask= 0x083f;
		off = 0x0800;
	} else
	/* 5325E/535x */
	/* TODO: same as above, according SDK only 5325 */
	if (model == MODEL_RTN12 || model == MODEL_RTN10U || model == MODEL_RTN10D1 || model == MODEL_RTN53 
		|| model == MODEL_RTN12B1 || model == MODEL_RTN12C1 || model == MODEL_RTN12D1 || model == MODEL_RTN12HP) {
		reg = 0x1e;
		mask= 0x0608;
		off = 0x0008;
	}

	for (i = 0; i < 5 && (portmask >> i); i++) {
		vecarg[0] = (i << 16) | reg;
		vecarg[1] = 0;
		if ((portmask & (1U << i)) == 0)
			continue;
		if (ioctl(fd, SIOCGETCPHYRD2, (caddr_t)&ifr) < 0)
			continue;
		vecarg[1] &= ~mask;
		vecarg[1] |= ctrl ? 0 : off;
		ioctl(fd, SIOCSETCPHYWR2, (caddr_t)&ifr);
	}
	close(fd);

	return 0;
}

#define IMAGE_HEADER "HDR0"
#define MAX_VERSION_LEN 64
#define MAX_PID_LEN 12
#define MAX_HW_COUNT 4

/*
 * 0: illegal image
 * 1: legal image
 */

int check_imageheader(char *buf, long *filelen)
{
	long aligned;

	if(strncmp(buf, IMAGE_HEADER, sizeof(IMAGE_HEADER)) == 0)
	{
		memcpy(&aligned, buf + 4, sizeof(aligned));
		*filelen = aligned;
		_dprintf("image len: %x\n", aligned);
		return 1;
	}
	else return 0;
}

/*
 * 0: illegal image
 * 1: legal image
 *
 * check product id, crc ..
 */

int check_imagefile(char *fname)
{
	FILE *fp;
	struct version_t {
		uint8_t ver[4];			/* Firmware version */
		uint8_t pid[MAX_PID_LEN];	/* Product Id */
		uint8_t hw[MAX_HW_COUNT][4];	/* Compatible hw list lo maj.min, hi maj.min */
		uint8_t	pad[0];			/* Padding up to MAX_VERSION_LEN */
	} version;
	int i, model;

	fp = fopen(fname, "r");
	if (fp == NULL)
		return 0;

	fseek(fp, -MAX_VERSION_LEN, SEEK_END);
	fread(&version, 1, sizeof(version), fp);
	fclose(fp);

	_dprintf("productid field in image: %.12s\n", version.pid);

	for (i = 0; i < sizeof(version); i++)
		_dprintf("%02x ", ((uint8_t *)&version)[i]);
	_dprintf("\n");

	/* safe strip trailing spaces */
	for (i = 0; i < MAX_PID_LEN && version.pid[i] != '\0'; i++);
	for (i--; i >= 0 && version.pid[i] == '\x20'; i--)
		version.pid[i] = '\0';

	model = get_model();

	/* compare up to the first \0 or MAX_PID_LEN
	 * nvram productid or hw model's original productid */
	if (strncmp(nvram_safe_get("productid"), version.pid, MAX_PID_LEN) == 0 ||
	    strncmp(get_modelid(model), version.pid, MAX_PID_LEN) == 0)
		return 1;

	/* common RT-N12 productid FW image */
	if ((model == MODEL_RTN12B1 || model == MODEL_RTN12C1 ||
	     model == MODEL_RTN12D1 || model == MODEL_RTN12HP) &&
	    strncmp(get_modelid(MODEL_RTN12), version.pid, MAX_PID_LEN) == 0)
		return 1;

	return 0;
}

int get_radio(int unit, int subunit)
{
	int n = 0;

	//_dprintf("get radio %x %x %s\n", unit, subunit, nvram_safe_get(wl_nvname("ifname", unit, subunit)));

	return (wl_ioctl(nvram_safe_get(wl_nvname("ifname", unit, subunit)), WLC_GET_RADIO, &n, sizeof(n)) == 0) &&
		!(n & (WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE));
}

void set_radio(int on, int unit, int subunit)
{
	uint32 n;
	char tmpstr[32];
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

	//_dprintf("set radio %x %x %x %s\n", on, unit, subunit, nvram_safe_get(wl_nvname("ifname", unit, subunit)));

	if (subunit > 0)
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	//if (nvram_match(strcat_r(prefix, "radio", tmp), "0")) return;

#ifdef RTAC66U
	snprintf(tmp, sizeof(tmp), "%sradio", prefix);
	if(!strcmp(tmp, "wl1_radio")){
		if(on){
			nvram_set("led_5g", "1");
			led_control(LED_5G, LED_ON);
		}
		else{
			nvram_set("led_5g", "0");
			led_control(LED_5G, LED_OFF);
		}
	}
#endif

        if (nvram_get_int("led_disable")==1) {
                led_control(LED_2G, LED_OFF);
                led_control(LED_5G, LED_OFF);
	}

	if(subunit>0) {
		sprintf(tmpstr, "%d", subunit);
		if(on) eval("wl", "-i", nvram_safe_get(wl_nvname("ifname", unit, 0)), "bss", "-C", tmpstr, "up");
		else eval("wl", "-i", nvram_safe_get(wl_nvname("ifname", unit, 0)), "bss", "-C", tmpstr, "down");
		return;
	}

#ifndef WL_BSS_INFO_VERSION
#error WL_BSS_INFO_VERSION
#endif

#if WL_BSS_INFO_VERSION >= 108
	n = on ? (WL_RADIO_SW_DISABLE << 16) : ((WL_RADIO_SW_DISABLE << 16) | 1);
	wl_ioctl(nvram_safe_get(wl_nvname("ifname", unit, subunit)), WLC_SET_RADIO, &n, sizeof(n));
	if (!on) {
		//led(LED_WLAN, 0);
		//led(LED_DIAG, 0);
	}
#else
	n = on ? 0 : WL_RADIO_SW_DISABLE;
	wl_ioctl(nvram_safe_get(wl_nvname("ifname", unit, subunit)), WLC_SET_RADIO, &n, sizeof(n));
	if (!on) {
		//led(LED_DIAG, 0);
	}
#endif
}


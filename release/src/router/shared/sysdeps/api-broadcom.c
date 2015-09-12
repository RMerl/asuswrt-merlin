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
#include <trxhdr.h>
#include <bcmutils.h>
#include <bcmendian.h>

#ifdef RTCONFIG_QTN
#include "web-qtn.h"
#endif

uint32_t gpio_dir(uint32_t gpio, int dir)
{
	/* FIXME
	return bcmgpio_connect(gpio, dir);
	 */

	return 0;
}

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

extern uint32_t gpio_read(void);
extern void gpio_write(uint32_t bitvalue, int en);

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
/*
	uint32_t bit;
	bit = 1<< gpio;

	_dprintf("set_gpio: %d %d\n", bit, value);
	gpio_write(bit, value);
*/
	//_dprintf("set_gpio: %d %d\n", gpio, value);
	gpio_write(gpio, value);

	return 0;
}

#ifdef RTCONFIG_BCMFA
int get_fa_rev(void)
{
	int fd, ret;
	unsigned int rev;
	struct ifreq ifr;
	et_var_t var;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) goto skip;

	rev = 0;
	var.set = 0;
	var.cmd = IOV_FA_REV;
	var.buf = &rev;
	var.len = sizeof(rev);

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");
	ifr.ifr_data = (caddr_t) &var;

	ret = ioctl(fd, SIOCSETGETVAR, (caddr_t)&ifr);
	close(fd);
	if (ret < 0)
		goto skip;

	return rev;

skip:
	return 0;
}

int get_fa_dump(void)
{
	int fd, rev, ret;
	struct ifreq ifr;
	et_var_t var;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) goto skip;

	rev = 0;
	var.set = 0;
	var.cmd = IOV_FA_DUMP;
	var.buf = &rev;
	var.len = sizeof(rev);

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");
	ifr.ifr_data = (caddr_t) &var;

	ret = ioctl(fd, SIOCSETGETVAR, (caddr_t)&ifr);
	close(fd);
	if (ret < 0)
		goto skip;

	return rev;

skip:
	return 0;
}

#endif

int get_switch_model(void)
{
	int fd, devid, ret;
	struct ifreq ifr;
	et_var_t var;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) goto skip;

	devid = 0;
	var.set = 0;
	var.cmd = IOV_ET_ROBO_DEVID;
	var.buf = &devid;
	var.len = sizeof(devid);

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0"); // is it always the same?
	ifr.ifr_data = (caddr_t) &var;

	ret = ioctl(fd, SIOCSETGETVAR, (caddr_t)&ifr);
	close(fd);
	if (ret < 0)
		goto skip;

	if (devid == 0x25)
		return SWITCH_BCM5325;
	else if (devid == 0x3115)
		return SWITCH_BCM53115;
	else if (devid == 0x3125)
		return SWITCH_BCM53125;
	else if ((devid & 0xfffffff0) == 0x53010)
		return SWITCH_BCM5301x;

skip:
	return SWITCH_UNKNOWN;
}

#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR

uint32_t robo_ioctl_len(int fd, int write, int page, int reg, uint32_t *value, uint32_t len)
{
	static int __ioctl_args[2] = { SIOCGETCROBORD, SIOCSETCROBOWR };
	struct ifreq ifr;
	int ret, vecarg[4];
	int i;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");
	ifr.ifr_data = (caddr_t) vecarg;

	vecarg[0] = (page << 16) | reg;
	vecarg[1] = len;

	ret = ioctl(fd, __ioctl_args[write], (caddr_t)&ifr);

	*value = vecarg[2];

	return ret;
}

uint32_t traffic_wanlan(char *ifname, uint32_t *rx, uint32_t *tx)
{
	int fd, model;
	uint32_t value;
	char port_name[30] = {0};
	char port[30], *next;

	*rx = 0;
	*tx = 0;

	strcat_r(ifname, "ports", port_name);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return 0;

	/* RX */
	foreach (port, nvram_safe_get(port_name), next) {
		if(strncmp(port, CPU_PORT, 1) != 0
#ifdef RTAC87U
			&& strncmp(port, RGMII_PORT, 1) != 0
#endif
		){
			if (robo_ioctl_len(fd, 0 /* robord */, MIB_P0_PAGE + atoi(port), MIB_RX_REG, &value, 8) < 0)
				_dprintf("et ioctl SIOCGETCROBORD failed!\n");
			else{
				*rx = *rx + value;
			}
		}
	}

	/* TX */
	foreach (port, nvram_safe_get(port_name), next) {
		if(strncmp(port, CPU_PORT, 1) != 0
#ifdef RTAC87U
			&& strncmp(port, RGMII_PORT, 1) != 0
#endif
		){
			if (robo_ioctl_len(fd, 0 /* robord */, MIB_P0_PAGE + atoi(port), MIB_TX_REG, &value, 8) < 0)
				_dprintf("et ioctl SIOCGETCROBORD failed!\n");
			else{
				*tx = *tx  + value;
			}
		}
	}
	close(fd);
	return 1;
}
#endif	/* RTCONFIG_BCM5301X_TRAFFIC_MONITOR */

int robo_ioctl(int fd, int write, int page, int reg, uint32_t *value)
{
	static int __ioctl_args[2] = { SIOCGETCROBORD, SIOCSETCROBOWR };
	struct ifreq ifr;
	int ret, vecarg[4];

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0"); // is it always the same?
	ifr.ifr_data = (caddr_t) vecarg;

	vecarg[0] = (page << 16) | reg;
#ifdef BCM5301X
	vecarg[1] = 0;
	vecarg[2] = *value;
#else
	vecarg[1] = *value;
#endif
	ret = ioctl(fd, __ioctl_args[write], (caddr_t)&ifr);

#ifdef BCM5301X
	*value = vecarg[2];
#else
	*value = vecarg[1];
#endif

	return ret;
}

int phy_ioctl(int fd, int write, int phy, int reg, uint32_t *value)
{
#ifndef BCM5301X
	static int __ioctl_args[2] = { SIOCGETCPHYRD2, SIOCSETCPHYWR2 };
	struct ifreq ifr;
	int ret, vecarg[2];

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0"); // is it always the same?
	ifr.ifr_data = (caddr_t) vecarg;

	vecarg[0] = (phy << 16) | reg;
	vecarg[1] = *value;
	ret = ioctl(fd, __ioctl_args[write], (caddr_t)&ifr);

	*value = vecarg[1];

	return ret;
#else
	return robo_ioctl(fd, write, 0x10 + phy, reg, value);
#endif
}

// !0: connected
//  0: disconnected
uint32_t get_phy_status(uint32_t portmask)
{
	int fd, model;
	uint32_t value, mask = 0;
#ifndef BCM5301X
	int i;
#endif

	model = get_switch();
	if (model == SWITCH_UNKNOWN) return 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return 0;

	switch (model) {
#ifndef BCM5301X
	case SWITCH_BCM53125:
#ifdef RTCONFIG_LANWAN_LED
		/* N15U can't read link status from phy sometimes */
		if (get_model() == MODEL_RTN15U)
			goto case_SWITCH_ROBORD;
		/* fall through */
#endif
	case SWITCH_BCM53115:
	case SWITCH_BCM5325:
		for (i = 0; i < 5 && (portmask >> i); i++) {
			if ((portmask & (1U << i)) == 0)
				continue;

			if (phy_ioctl(fd, 0, i, 0x01, &value) < 0)
				continue;
			/* link is down, but negotiation has started
			 * read register again, use previous value, if failed */
			if ((value & 0x22) == 0x20)
				phy_ioctl(fd, 0, i, 0x01, &value);

			if (value & (1U << 2))
				mask |= (1U << i);
		}
		break;
#ifdef RTCONFIG_LANWAN_LED
	case_SWITCH_ROBORD:
		/* fall through */
#endif
#endif
	case SWITCH_BCM5301x:
		if (robo_ioctl(fd, 0, 0x01, 0x00, &value) < 0)
			_dprintf("et ioctl SIOCGETCROBORD failed!\n");
		mask = value & portmask & 0x1f;
		break;
	}
	close(fd);

	//_dprintf("# get_phy_status %x %x\n", mask, portmask);

	return mask;
}

// 2bit per port (0-4(5)*2 shift)
// 0: 10 Mbps
// 1: 100 Mbps
// 2: 1000 Mbps
uint32_t get_phy_speed(uint32_t portmask)
{
	int fd, model;
	uint32_t value, mask = 0;

	model = get_switch();
	if (model == SWITCH_UNKNOWN) return 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return 0;

	if (robo_ioctl(fd, 0, 0x01, 0x04, &value) < 0)
		value = 0;
	close(fd);

	switch (model) {
#ifndef BCM5301X
	case SWITCH_BCM5325:
		/* 5325E/535x, 1bit: 0=10 Mbps, 1=100Mbps */
		for (mask = 0; value & 0x1f; value >>= 1) {
			mask |= (value & 0x01);
			mask <<= 2;
		}
		swapportstatus(mask);
		break;
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
		/* fall through */
#endif
	case SWITCH_BCM5301x:
		/* 5301x/53115/53125, 2bit:00=10 Mbps,01=100Mbps,10=1000Mbps */
		mask = value & portmask & 0x3ff;
		break;
	}

	//_dprintf("get_phy_speed %x %x\n", vecarg[1], portmask);

	return mask;
}

uint32_t set_phy_ctrl(uint32_t portmask, int ctrl)
{
	int fd, i, model;
	uint32_t value;

	model = get_switch();
	if (model == SWITCH_UNKNOWN) return 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return 0;

	for (i = 0; i < 5 && (portmask >> i); i++) {
		if ((portmask & (1U << i)) == 0)
			continue;

		switch (model) {
#ifndef BCM5301X
		case SWITCH_BCM5325:
			if (phy_ioctl(fd, 0, i, 0x1e, &value) < 0)
				value = 0;
			value &= 0x0007;
			value |= ctrl ? 0 : 0x0008;
			phy_ioctl(fd, 1, i, 0x1e, &value);
			value = 0x3300;
			break;
		case SWITCH_BCM53115:
		case SWITCH_BCM53125:
			/* fall through */
#endif
		case SWITCH_BCM5301x:
			value = 0x1340;
			value |= ctrl ? 0 : 0x0800;
			break;
		default:
			continue;
		}

		/* issue write */
		phy_ioctl(fd, 1, i, 0, &value);
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
int
check_crc(char *fname)
{
	FILE *fp;
	int ret = 1;
	int first_read = 1;
	unsigned int len, count;

	struct trx_header trx;
	uint32 crc;
	static uint32 buf[16*1024];

	fp = fopen(fname, "r");
	if (fp == NULL)
	{
		_dprintf("Open trx fail!!!\n");
		return 0;
	}

	/* Read header */
	ret = fread((unsigned char *) &trx, 1, sizeof(struct trx_header), fp);
	if (ret != sizeof(struct trx_header)) {
		ret = 0;
		_dprintf("read header error!!!\n");
		goto done;
	}

	/* Checksum over header */
	crc = hndcrc32((uint8 *) &trx.flag_version,
		       sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
		       CRC32_INIT_VALUE);

	for (len = ltoh32(trx.len) - sizeof(struct trx_header); len; len -= count) {
		if (first_read) {
			count = MIN(len, sizeof(buf) - sizeof(struct trx_header));
			first_read = 0;
		} else
			count = MIN(len, sizeof(buf));

		/* Read data */
		ret = fread((unsigned char *) &buf, 1, count, fp);
		if (ret != count) {
			ret = 0;
			_dprintf("read error!\n");
			goto done;
		}

		/* Checksum over data */
		crc = hndcrc32((uint8 *) &buf, count, crc);
	}
	/* Verify checksum */
	//_dprintf("checksum: %u ? %u\n", ltoh32(trx.crc32), crc);
	if (ltoh32(trx.crc32) != crc) {
		ret = 0;
		goto done;
	}

done:
	fclose(fp);

	return ret;
}

/*
 * 0: illegal image
 * 1: legal image
 */

int check_imageheader(char *buf, long *filelen)
{
	long aligned;

	if (strncmp(buf, IMAGE_HEADER, sizeof(IMAGE_HEADER) - 1) == 0)
	{
		memcpy(&aligned, buf + sizeof(IMAGE_HEADER) - 1, sizeof(aligned));
		*filelen = aligned;
#ifdef RTCONFIG_DSL_TCLINUX
		*filelen+=0x790000;
#endif
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

	if (!check_crc(fname)) {
		_dprintf("check crc error!!!\n");
		return 0;
	}

	model = get_model();

	/* compare up to the first \0 or MAX_PID_LEN
	 * nvram productid or hw model's original productid */
	if (strncmp(nvram_safe_get("productid"), (char *) version.pid, MAX_PID_LEN) == 0 ||
	    strncmp(get_modelid(model), (char *) (char *) version.pid, MAX_PID_LEN) == 0)
	{
		return 1;
	}

	/* common RT-N12 productid FW image */
	if ((model == MODEL_RTN12B1 || model == MODEL_RTN12C1 ||
	     model == MODEL_RTN12D1 || model == MODEL_RTN12VP || model == MODEL_RTN12HP || model == MODEL_RTN12HP_B1 ||model == MODEL_APN12HP) &&
	     strncmp(get_modelid(MODEL_RTN12), (char *) version.pid, MAX_PID_LEN) == 0)
		return 1;

	return 0;
}

#ifdef RTCONFIG_QTN
char *wl_vifname_qtn(int unit, int subunit)
{
	static char tmp[128];

	if ((subunit > 0) && (subunit < 4))
	{
		sprintf(tmp, "wifi%d", subunit);
		return strdup(tmp);
	}
	else
		return strdup("");
}
#endif

int get_radio(int unit, int subunit)
{
	int n = 0;

	//_dprintf("get radio %x %x %s\n", unit, subunit, nvram_safe_get(wl_nvname("ifname", unit, subunit)));

#ifdef RTCONFIG_QTN
	int ret;
	char interface_status = 0;

	if (unit)
	{
		if(!rpc_qtn_ready())
			return -1;

		if (subunit > 0)
		{
			ret = qcsapi_interface_get_status(wl_vifname_qtn(unit, subunit), &interface_status);
//			if (ret < 0)
//				dbG("Qcsapi qcsapi_interface_get_status %s error, return: %d\n", wl_vifname_qtn(unit, subunit), ret);

			return interface_status;
		}
		else
		{
			ret = qcsapi_wifi_rfstatus((qcsapi_unsigned_int *) &n);
//			if (ret < 0)
//				dbG("Qcsapi qcsapi_wifi_rfstatus %s error, return: %d\n", wl_vifname_qtn(unit, subunit), ret);

			return n;
		}
	}
	else
#endif

	return (wl_ioctl(nvram_safe_get(wl_nvname("ifname", unit, subunit)), WLC_GET_RADIO, &n, sizeof(n)) == 0) &&
		!(n & (WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE));
}

void set_radio(int on, int unit, int subunit)
{
	uint32 n;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

#ifdef RTCONFIG_QTN
	if (unit) {
		if (!rpc_qtn_ready())
			return;

		rpc_set_radio(unit, subunit, on);

		return;
	}
#endif
	//_dprintf("set radio %x %x %x %s\n", on, unit, subunit, nvram_safe_get(wl_nvname("ifname", unit, subunit)));

	if (subunit > 0)
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	//if (nvram_match(strcat_r(prefix, "radio", tmp), "0")) return;

#if defined(RTAC66U) || defined(BCM4352)
	if ((unit == 1) & (subunit < 1)) {
		if (on) {
#ifndef RTCONFIG_LED_BTN
			if (!(nvram_get_int("sw_mode")==SW_MODE_AP && nvram_get_int("wlc_psta")==1 && nvram_get_int("wlc_band")==0)) {
				nvram_set("led_5g", "1");
				led_control(LED_5G, LED_ON);
			}
#else
			nvram_set("led_5g", "1");
			if (nvram_get_int("AllLED"))
				led_control(LED_5G, LED_ON);
#endif
		}
		else {
			nvram_set("led_5g", "0");
			led_control(LED_5G, LED_OFF);
		}
	}
#endif

	if (subunit > 0) {
		sprintf(tmp, "%d", subunit);
		if (on) eval("wl", "-i", nvram_safe_get(wl_nvname("ifname", unit, 0)), "bss", "-C", tmp, "up");
		else eval("wl", "-i", nvram_safe_get(wl_nvname("ifname", unit, 0)), "bss", "-C", tmp, "down");

		if (nvram_get_int("led_disable")==1) {
			led_control(LED_2G, LED_OFF);
			led_control(LED_5G, LED_OFF);
		}
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

	if (nvram_get_int("led_disable")==1) {
		led_control(LED_2G, LED_OFF);
		led_control(LED_5G, LED_OFF);
	}
}

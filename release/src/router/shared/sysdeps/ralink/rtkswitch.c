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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <bcmnvram.h>

#include <utils.h>
#include <shutils.h>
#include <shared.h>
#include <linux/config.h>
#include <ralink_gpio.h>
#include <ralink.h>

#define RTKSWITCH_DEV	"/dev/rtkswitch"

struct trafficCount_t
{
	long long rxByteCount;
	long long txByteCount;
};

int rtkswitch_ioctl(int val, int val2)
{
	int fd;
	int value = 0;
	int *p = NULL, invalid_ioctl_cmd = 0;
	struct trafficCount_t portTraffic;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	switch (val) {
	case 0:		/* check WAN port phy status */
	case 3:		/* check LAN ports phy status */
	case 6:
	case 8:
	case 9:
	case 10:
	case 19:	/* Set WAN port txDelay */
	case 20:	/* Set WAN port rxDelay */
	case 22:
	case 23:
	case 24:
	case 25:
	case 36:	/* Set Vlan VID. */
	case 37:	/* Set Vlan PRIO. */
	case 38:	/* Initialize VLAN. Cherry Cho added in 2011/7/15. */
	case 39:	/* Create VLAN. Cherry Cho added in 2011/7/15. */
        case 40:
	case 99:
	case 100:
	case 109:	/* Set specific ext port txDelay */
	case 110:	/* Set specific ext port rxDelay */
		p = &value;
		value = (unsigned int)val2;
		break;
	case 29:/* Set VoIP port. Cherry Cho added in 2011/6/30. */
		val = 34;	/* call ioctl 34 instead. */
		p = &value;
		value = (unsigned int)val2;
		break;
	case 2:		/* show state of RTL8367RB GMAC1 */
	case 7:
	case 11:
	case 14:	/* power up LAN port(s) */
	case 15:	/* power down LAN port(s) */
	case 16:	/* power up all ports */
	case 17:	/* power down all ports */
	case 21:
#ifdef RTN56U
	case 26:	/* WAN port force 1G mode */
#endif
	case 27:
	case 41:	/* check realtek switch normal */
	case 114:	/* power up WAN port(s) */
	case 115:	/* power down WAN port(s) */
		p = NULL;
		break;
	case 42:
	case 43:
		p = (int*) &portTraffic;
		break;

	default:
		invalid_ioctl_cmd = 1;
		break;
	}

	if (invalid_ioctl_cmd) {
		printf("wrong ioctl cmd: %d\n", val);
		close(fd);
		return 0;
	}

	if (ioctl(fd, val, p) < 0) {
		perror("rtkswitch ioctl");
		close(fd);
		return -1;
	}

	if (val == 0 || val == 3)
		printf("return: %x\n", value);
	else if(val == 42 || val == 43)
		printf("rx/tx: %lld/%lld\n", portTraffic.rxByteCount, portTraffic.txByteCount);

	close(fd);
	return 0;
}

int config_rtkswitch(int argc, char *argv[])
{
	int val;
	int val2 = 0;
	char *cmd = NULL;
	char *cmd2 = NULL;

	if (argc >= 2)
		cmd = argv[1];
	else
		return -1;
	if (argc >= 3)
		cmd2 = argv[2];

	val = (int) strtol(cmd, NULL, 0);
	if (cmd2)
		val2 = (int) strtol(cmd2, NULL, 0);

	return rtkswitch_ioctl(val, val2);
}

typedef struct {
	unsigned int idx;
	unsigned int value;
} asus_gpio_info;

int ralink_gpio_write_bit(int idx, int value)
{
	int fd;
	asus_gpio_info info;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	info.idx = idx;
	info.value = value;

	if (ioctl(fd, RALINK_GPIO_WRITE_BIT, &info) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int ralink_gpio_read_bit(int idx)
{
	int fd;
	int value;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	value = idx;

	if (ioctl(fd, RALINK_GPIO_READ_BIT, &value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	return value;
}

int ralink_gpio_init(unsigned int idx, int dir)
{
	int fd;
	asus_gpio_info info;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	info.idx = idx;
	info.value = dir;

	if (ioctl(fd, RALINK_GPIO_SET_DIR, &info) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

// a wrapper to broadcom world
// bit 1: LED_POWER
// bit 6: BTN_RESET
// bit 8: BTN_WPS

uint32_t ralink_gpio_read(void)
{
	uint32_t r = 0;

	if (ralink_gpio_read_bit(RA_BTN_RESET))
		r |= (1 << 6);
	if (ralink_gpio_read_bit(RA_BTN_WPS))
		r |= (1 << 8);

	return r;
}

int ralink_gpio_write(uint32_t bit, int en)
{
	if (bit & (1 << 1)) {
		if (!en)
			ralink_gpio_write_bit(RA_LED_POWER, RA_LED_OFF);
		else
			ralink_gpio_write_bit(RA_LED_POWER, RA_LED_ON);
	}

	return 0;
}

unsigned int rtkswitch_wanPort_phyStatus(int wan_unit)
{
	int fd;
	unsigned int value;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	if (ioctl(fd, 0, &value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	return value;
}

unsigned int rtkswitch_lanPorts_phyStatus(void)
{
	int fd;
	unsigned int value;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	if (ioctl(fd, 3, &value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	return value;
}

unsigned int rtkswitch_WanPort_phySpeed(void)
{
	int fd;
	unsigned int value;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	if (ioctl(fd, 13, &value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	return value;
}

int rtkswitch_WanPort_linkUp(void)
{
	eval("rtkswitch", "114");

	return 0;
}

int rtkswitch_WanPort_linkDown(void)
{
	eval("rtkswitch", "115");

	return 0;
}

int rtkswitch_LanPort_linkUp(void)
{
	eval("rtkswitch", "14");

	return 0;
}

int rtkswitch_LanPort_linkDown(void)
{
	eval("rtkswitch", "15");

	return 0;
}

int rtkswitch_AllPort_linkUp(void)
{
	eval("rtkswitch", "16");

	return 0;
}

int rtkswitch_AllPort_linkDown(void)
{
	eval("rtkswitch", "17");

	return 0;
}

int rtkswitch_Reset_Storm_Control(void)
{
	eval("rtkswitch", "21");

	return 0;
}

typedef struct {
	unsigned int link[5];
	unsigned int speed[5];
} phyState;

int rtkswitch_AllPort_phyState(void)
{
	int fd;
	char buf[32];
	int porder_56u[5] = {4,3,2,1,0};
	int *o = porder_56u;
	const char *portMark = "W0=%C;L1=%C;L2=%C;L3=%C;L4=%C;";

#ifdef RTCONFIG_DSL
	int porder_dsl[5] = {0,1,2,3,4};

	o = porder_dsl;
#endif

	if (get_model() == MODEL_RTN65U)
		portMark = "W0=%C;L4=%C;L3=%C;L2=%C;L1=%C;";

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	phyState pS;

	pS.link[0] = pS.link[1] = pS.link[2] = pS.link[3] = pS.link[4] = 0;
	pS.speed[0] = pS.speed[1] = pS.speed[2] = pS.speed[3] = pS.speed[4] = 0;

	if (ioctl(fd, 18, &pS) < 0) {
		perror("rtkswitch ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	sprintf(buf, portMark,
		(pS.link[o[0]] == 1) ? (pS.speed[o[0]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[1]] == 1) ? (pS.speed[o[1]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[2]] == 1) ? (pS.speed[o[2]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[3]] == 1) ? (pS.speed[o[3]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[4]] == 1) ? (pS.speed[o[4]] == 2) ? 'G' : 'M': 'X');

	puts(buf);

	return 0;
}

#if 0
void usage(char *cmd)
{
	printf("Usage: %s 1 - set (SCK, SD) as (0, 1)\n", cmd);
	printf("       %s 2 - set (SCK, SD) as (1, 0)\n", cmd);
	printf("       %s 3 - set (SCK, SD) as (1, 1)\n", cmd);
	printf("       %s 0 - set (SCK, SD) as (0, 0)\n", cmd);
	printf("       %s 4 - init vlan\n", cmd);
	printf("       %s 5 - set cpu port 0 0\n", cmd);
	printf("       %s 6 - set cpu port 0 1\n", cmd);
	printf("       %s 7 - set cpu port 1 0\n", cmd);
	printf("       %s 8 - set cpu port 1 1\n", cmd);
	printf("       %s 10 - set vlan entry, no cpu port, but mbr\n", cmd);
	printf("       %s 11 - set vlan entry, no cpu port, no mbr\n", cmd);
	printf("       %s 15 - set vlan PVID, no cpu port\n", cmd);
	printf("       %s 20 - set vlan entry, with cpu port\n", cmd);
	printf("       %s 21 - set vlan entry, with cpu port and no cpu port in untag sets\n", cmd);
	printf("       %s 25 - set vlan PVID, with cpu port\n", cmd);
	printf("       %s 26 - set vlan PVID, not set cpu port\n", cmd);
	printf("       %s 90 - accept all frmaes\n", cmd);
	printf("       %s 66 - setup default vlan\n", cmd);
	printf("       %s 61 - setup vlan type1\n", cmd);
	printf("       %s 62 - setup vlan type2\n", cmd);
	printf("       %s 63 - setup vlan type3\n", cmd);
	printf("       %s 64 - setup vlan type4\n", cmd);
	printf("       %s 65 - setup vlan type34\n", cmd);
	printf("       %s 70 - disable multicast snooping\n", cmd);
	printf("       %s 81 - setRtctTesting on port x\n", cmd);
	printf("       %s 82 - getRtctResult on port x\n", cmd);
	printf("       %s 83 - setGreenEthernet x(green, powsav)\n", cmd);
	printf("       %s 84 - setAsicGreenFeature x(txGreen, rxGreen)\n", cmd);
	printf("       %s 85 - getAsicGreenFeature\n", cmd);
	printf("       %s 86 - enable GreenEthernet on port x\n", cmd);
	printf("       %s 87 - disable GreenEthernet on port x\n", cmd);
	printf("       %s 88 - getAsicPowerSaving x\n", cmd);
	printf("       %s 50 - getPortLinkStatus x\n", cmd);
	exit(0);
}
#endif

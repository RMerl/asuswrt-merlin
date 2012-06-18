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

#define RTL8367R_DEV	"/dev/rtl8367r"

int rtl8367r_ioctl(int val, int val2)
{
	int fd;
	int value = 0;
	unsigned int value2 = 0;

	fd = open(RTL8367R_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTL8367R_DEV);
		return -1;
	}

	switch (val)
	{
	case 2:
		if(ioctl(fd, 2, 0) < 0){
			perror("rtl8367r ioctl");
			close(fd);
			return -1;
		}
		break;
        case 6:
		value = val2;
                if(ioctl(fd, 6, &value) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;
	case 7:
                if(ioctl(fd, 7, 0) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
		break;
        case 8:
		value = val2;
                if(ioctl(fd, 8, &value) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;
        case 9:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 9, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;
        case 10:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 10, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;
        
	case 11:
		if(ioctl(fd, 11, 0) < 0){
			perror("rtl8367r ioctl");
			close(fd);
			return -1;
		}
		break;

	case 14:
		if(ioctl(fd, 14, 0) < 0){
			perror("rtl8367r ioctl");
			close(fd);
			return -1;
		}
		break;

	case 15:
		if(ioctl(fd, 15, 0) < 0){
			perror("rtl8367r ioctl");
			close(fd);
			return -1;
		}
		break;

	case 16:
		if(ioctl(fd, 16, 0) < 0){
			perror("rtl8367r ioctl");
			close(fd);
			return -1;
		}
		break;

        case 17:
		if(ioctl(fd, 17, 0) < 0){
			perror("rtl8367r ioctl");
			close(fd);
			return -1;
		}
		break;

        case 19:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 19, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;

        case 20:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 20, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;

        case 21:
                if(ioctl(fd, 21, 0) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;

        case 22:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 22, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;

        case 23:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 23, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;

        case 24:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 24, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;

        case 25:
                value2 = (unsigned int) val2;
                if(ioctl(fd, 25, &value2) < 0){
                        perror("rtl8367r ioctl");
                        close(fd);
                        return -1;
                }
                break;

	default:
		printf("wrong ioctl cmd: %d\n", val);
	}

	close(fd);
	return 0;
}


int config8367r(int argc, char *argv[])
{
	char *cmd = NULL;
	char *cmd2 = NULL;

	if (argc >= 2)
		cmd = argv[1];
	else
		return;
	if (argc >= 3)
		cmd2 = argv[2];

	int val = atoi(cmd);
	int val2 = 0;

	if (cmd2)
		val2 = atoi(cmd2);

//	printf("config8367 %d %d\n",val,val2);
		
	rtl8367r_ioctl(val, val2);
	return 0;
}

typedef struct {
	unsigned int idx;
	unsigned int value;
} asus_gpio_info;

int
ralink_gpio_write_bit(int idx, int value)
{
	int fd;
	asus_gpio_info info;

	fd = open(RTL8367R_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTL8367R_DEV);
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

int
ralink_gpio_read_bit(int idx)
{
	int fd;
	int value;

	fd = open(RTL8367R_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTL8367R_DEV);
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

int
ralink_gpio_init(unsigned int idx, int dir)
{
	int fd;
	asus_gpio_info info;

	fd = open(RTL8367R_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTL8367R_DEV);
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
// DSLTODO
/*
uint32_t ralink_gpio_read(void)
{
	uint32_t r=0;

	if(ralink_gpio_read_bit(RA_BTN_RESET)) r|=(1<<6);
	if(ralink_gpio_read_bit(RA_BTN_WPS)) r|=(1<<8);
	
	return r;
}

int ralink_gpio_write(uint32_t bit, int en)
{
	if(bit&(1<<1))
	{
		if(!en) ralink_gpio_write_bit(RA_LED_POWER, RA_LED_OFF);
		else ralink_gpio_write_bit(RA_LED_POWER, RA_LED_ON);
	}

	return 0;
}
*/



int
rtl8367r_LanPort_linkUp()
{
	system("8367r 14");

	return 0;
}

int
rtl8367r_LanPort_linkDown()
{
	system("8367r 15");

	return 0;
}

int
rtl8367r_AllPort_linkUp()
{
	system("8367r 16");
        
	return 0;
}

int
rtl8367r_AllPort_linkDown()
{
	system("8367r 17");

	return 0;
}



typedef struct {
        unsigned int link[5];
        unsigned int speed[5];
} phyState;

// DSLTODO
// move to other file
void
rtl8367r_AllPort_phyState()
{
        int fd;
	char buf[32];

        fd = open(RTL8367R_DEV, O_RDONLY);
        if (fd < 0) {
                perror(RTL8367R_DEV);
                return -1;
        }
        
	phyState pS;

        pS.link[0] = pS.link[1] = pS.link[2] = pS.link[3] = pS.link[4] = 0;
        pS.speed[0] = pS.speed[1] = pS.speed[2] = pS.speed[3] = pS.speed[4] = 0;

	if (ioctl(fd, 18, &pS) < 0)
	{
		perror("rtl8367r ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	sprintf(buf, "W0=%C;L1=%C;L2=%C;L3=%C;L4=%C;",
		(pS.link[0] == 1) ? (pS.speed[0] == 2) ? 'G' : 'M': 'N',
		(pS.link[1] == 1) ? (pS.speed[1] == 2) ? 'G' : 'M': 'N',
		(pS.link[2] == 1) ? (pS.speed[2] == 2) ? 'G' : 'M': 'N',
		(pS.link[3] == 1) ? (pS.speed[3] == 2) ? 'G' : 'M': 'N',
		(pS.link[4] == 1) ? (pS.speed[4] == 2) ? 'G' : 'M': 'N');

	puts(buf);

        return 0;
}



unsigned int
rtl8367r_wanPort_phyStatus()
{
    int fd;
    unsigned int value;

    fd = open(RTL8367R_DEV, O_RDONLY);
    if (fd < 0) {
        perror(RTL8367R_DEV);
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

#ifdef RTCONFIG_DUALWAN
static int m_dualwan_ifname_values_loaded = 0;
static int m_primary_if;
static int m_secondary_if;

int get_dualwan_values_once(void)
{
	if (m_dualwan_ifname_values_loaded) return;
	m_dualwan_ifname_values_loaded = 1;
	m_primary_if = get_dualwan_primary();
	m_secondary_if = get_dualwan_secondary();	
}
#endif

int dsl_wanPort_phyStatus()
{
	return rtl8367r_wanPort_phyStatus();
}

#if 0

#include <stdio.h>	     
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/config.h>


#define RTL8367R_DEV	"/dev/rtl8367r"

// from linux/linux-2.6.21.x/drivers/char/ralink_gpio.h
#define RALINK_GPIO_SET_DIR             0x01
#define RALINK_GPIO_READ_BIT            0x04
#define RALINK_GPIO_WRITE_BIT           0x05


typedef struct {
	unsigned int idx;
	unsigned int value;
} asus_gpio_info;

int
ralink_gpio_write_bit(int idx, int value)
{
	int fd;
	asus_gpio_info info;

	fd = open(RTL8367R_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTL8367R_DEV);
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

int
ralink_gpio_read_bit(int idx)
{
	int fd;
	int value;

	fd = open(RTL8367R_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTL8367R_DEV);
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

int
ralink_gpio_init(unsigned int idx, int dir)
{
	int fd;
	asus_gpio_info info;

	fd = open(RTL8367R_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTL8367R_DEV);
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



int
rtl8367r_LanPort_linkUp()
{
	system("/usr/sbin/8367r 14");

	return 0;
}

int
rtl8367r_LanPort_linkDown()
{
	system("/usr/sbin/8367r 15");

	return 0;
}

int
rtl8367r_AllPort_linkUp()
{
	system("/usr/sbin/8367r 16");
        
	return 0;
}

int
rtl8367r_AllPort_linkDown()
{
	system("/usr/sbin/8367r 17");

	return 0;
}



typedef struct {
        unsigned int link[5];
        unsigned int speed[5];
} phyState;


void
rtl8367r_AllPort_phyState()
{
        int fd;
	char buf[32];

        fd = open(RTL8367R_DEV, O_RDONLY);
        if (fd < 0) {
                perror(RTL8367R_DEV);
                return -1;
        }
        
	phyState pS;

        pS.link[0] = pS.link[1] = pS.link[2] = pS.link[3] = pS.link[4] = 0;
        pS.speed[0] = pS.speed[1] = pS.speed[2] = pS.speed[3] = pS.speed[4] = 0;

	if (ioctl(fd, 18, &pS) < 0)
	{
		perror("rtl8367r ioctl");
		close(fd);
		return -1;
	}

	close(fd);

	sprintf(buf, "W0=%C;L1=%C;L2=%C;L3=%C;L4=%C;",
		(pS.link[0] == 1) ? (pS.speed[0] == 2) ? 'G' : 'M': 'N',
		(pS.link[1] == 1) ? (pS.speed[1] == 2) ? 'G' : 'M': 'N',
		(pS.link[2] == 1) ? (pS.speed[2] == 2) ? 'G' : 'M': 'N',
		(pS.link[3] == 1) ? (pS.speed[3] == 2) ? 'G' : 'M': 'N',
		(pS.link[4] == 1) ? (pS.speed[4] == 2) ? 'G' : 'M': 'N');

	puts(buf);

        return 0;
}

#endif



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

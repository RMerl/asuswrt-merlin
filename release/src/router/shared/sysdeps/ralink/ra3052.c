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
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/autoconf.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <shutils.h>
#include "ra_ioctl.h"
#include "ra3052.h"
#include <dual_wan.h>

int esw_fd;

/*
enum daul_wan_type	//-> p0~p4
{
	WLLLL,
	WLLLW,
	LWLLW,
	LLWLW,
	LLLWW,
	LLWWW
};
*/

int
switch_init(void)
{
        esw_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (esw_fd < 0) {
                perror("socket");
                return -1;
        }
        return 0;
}

void
switch_fini(void)
{
        close(esw_fd);
}

int ra3052_reg_read(int offset, int *value)
{
        struct ifreq ifr;
        esw_reg reg;

        if (value == NULL)
                return -1;
        reg.off = offset;
        strncpy(ifr.ifr_name, "eth2", 5);
        ifr.ifr_data = &reg;
        if (-1 == ioctl(esw_fd, RAETH_ESW_REG_READ, &ifr)) {
                perror("ioctl");
                close(esw_fd);
                return -1;
        }
        *value = reg.val;
        return 0;
}

int ra3052_reg_write(int offset, int value)
{
        struct ifreq ifr;
        esw_reg reg;

        reg.off = offset;
        reg.val = value;
        strncpy(ifr.ifr_name, "eth2", 5);
        ifr.ifr_data = &reg;
        if (-1 == ioctl(esw_fd, RAETH_ESW_REG_WRITE, &ifr)) {
                perror("ioctl");
                close(esw_fd);
                exit(0);
        }
        return 0;
}

int
config_3052(int type)
{
        if(switch_init() < 0)
                return -1;

        ra3052_reg_write(0x14, 0x405555);
        ra3052_reg_write(0x50, 0x2001);
        ra3052_reg_write(0x98, 0x7f3f);
        ra3052_reg_write(0xe4, 0x3f);	// double vlan tag

        /*LLLLW*/
	switch(type)
	{
	case WLLLL:
        	ra3052_reg_write(0x40, 0x1002);
        	ra3052_reg_write(0x44, 0x1001);
       		ra3052_reg_write(0x48, 0x1001);
        	ra3052_reg_write(0x70, 0xffff417e);
		break;
	case WLLLW:
        	ra3052_reg_write(0x40, 0x1002);
        	ra3052_reg_write(0x44, 0x1001);
       		ra3052_reg_write(0x48, 0x1002);
        	ra3052_reg_write(0x70, 0xffff516e);
		break;
	case LWLLW:
        	ra3052_reg_write(0x40, 0x2001);
        	ra3052_reg_write(0x44, 0x1001);
       		ra3052_reg_write(0x48, 0x1002);
        	ra3052_reg_write(0x70, 0xffff526d);
		break;
	case LLWLW:
        	ra3052_reg_write(0x40, 0x1001);
        	ra3052_reg_write(0x44, 0x1002);
       		ra3052_reg_write(0x48, 0x1002);
        	ra3052_reg_write(0x70, 0xffff546b);
		break;
	case LLLWW:
        	ra3052_reg_write(0x40, 0x1001);
        	ra3052_reg_write(0x44, 0x2001);
       		ra3052_reg_write(0x48, 0x1002);
        	ra3052_reg_write(0x70, 0xffff5867);
		break;
	case LLWWW:
        	ra3052_reg_write(0x40, 0x1001);
        	ra3052_reg_write(0x44, 0x2002);
       		ra3052_reg_write(0x48, 0x1002);
        	ra3052_reg_write(0x70, 0xffff5c63);
		break;
	default:	/*LLLLW*/
        	ra3052_reg_write(0x40, 0x1001);
        	ra3052_reg_write(0x44, 0x1001);
       		ra3052_reg_write(0x48, 0x1002);
        	ra3052_reg_write(0x70, 0xffff506f);
	}

        switch_fini();
        return 0;
}


int
restore_3052()
{
        if(switch_init() < 0)
                return -1;

        ra3052_reg_write(0x14, 0x5555);
        ra3052_reg_write(0x40, 0x1001);
        ra3052_reg_write(0x44, 0x1001);
        ra3052_reg_write(0x48, 0x1001);
        ra3052_reg_write(0x4c, 0x1);
        ra3052_reg_write(0x50, 0x2001);
        ra3052_reg_write(0x70, 0xffffffff);
        ra3052_reg_write(0x98, 0x7f7f);

        switch_fini();
        return 0;
}


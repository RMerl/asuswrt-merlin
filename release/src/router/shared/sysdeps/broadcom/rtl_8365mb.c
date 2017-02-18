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
#include <errno.h>

#include <linux/major.h>
#include <rtk_switch.h>
#include <rtk_types.h>

#define RTKSWITCH_DEV	"/dev/rtkswitch"

#define CASEID(a)	#a
char *rtk_switch_cmds[] = RTK_SWITCH_CMDS;
#undef CASEID
unsigned int rtk_cmds_pa[MAX_REQ];

void usage(char *cmd);

int rtkswitch_ioctl(int val, int val2, int val3)
{
	int fd;
	int value = 0;
	rtk_asic_t asics;
	void *p = NULL;

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	switch (val) {
	/* w/ no options */
	case INIT_SWITCH:
	case INIT_SWITCH_UP:
	case GET_EXT_TXRXDELAY:
	case GET_LANPORTS_LINK_STATUS:
	case BAD_ADDR_X:
	case POWERUP_LANPORTS:
	case POWERDOWN_LANPORTS:
	case GET_RTK_PHYSTATES:
	case SOFT_RESET:
	case GET_CPU:
	case GET_GREEN_ETHERNET:
		p = NULL;
		break;

	/* w/ 1 option */
	case RESET_PORT:
	case GET_PORT_STAT:
	case GET_PORT_SPEED:
	case SET_EXT_TXDELAY:
	case SET_EXT_RXDELAY:
	case GET_REG:
	case SET_EXT_MODE:
	case SET_CPU:
	case GET_TMODE:
	case GET_PHY_REG9:
	case SET_GREEN_ETHERNET:
		p = (void*)&value;
		value = (unsigned int)val2;
		break;

	/* w/ 2 options */
	case TEST_REG:
	case SET_REG:
	case SET_TMODE:
	case SET_PHY_REG9:
		p = (void*)&asics;
		asics.rtk_reg = (rtk_uint32)val2;
		asics.rtk_val = (rtk_uint32)val3;
		break;

	default:
		usage("rtkswitch");
		close(fd);
		return 0;
	}

	if (ioctl(fd, val, p) < 0) {
		perror("rtkswitch ioctl");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

int config_rtkswitch(int argc, char *argv[])
{
	int val;
	int val2 = 0;
	int val3 = 0;
	char *cmd = NULL;
	char *cmd2 = NULL;
	char *cmd3 = NULL;

	if (argc >= 2)
		cmd = argv[1];
	else {
		usage("rtkswitch");
		return -1;
	}

	if (argc >= 3) {
		cmd2 = argv[2];
		if(argc > 3)
			cmd3 = argv[3];
	}

	val = (int) strtol(cmd, NULL, 0);
	if (cmd2)
		val2 = (int) strtol(cmd2, NULL, 0);
	if (cmd3)
		val3 = (int) strtol(cmd3, NULL, 0);

	return rtkswitch_ioctl(val, val2, val3);
}

#if defined(RTCONFIG_EXT_RTL8370MB)
#define MAX_RTL_PORTS 8
#else
#define MAX_RTL_PORTS 4
#endif
typedef struct {
	unsigned int link[MAX_RTL_PORTS];
	unsigned int speed[MAX_RTL_PORTS];
} phyState;

int ext_rtk_phyState(int verbose, char* BCMPorts)
{
	int model;
	char buf[64];
	int *o;
#if defined(RTCONFIG_EXT_RTL8370MB)
#ifdef RTAC5300R
	/* L1=R2,L2=R3,L3=R4,L4=B3,L5=R0,L6=R1,L7=R5,L8=B4,T1=B1<>R7,T2=B2<>R6*/
	const char *portMark = "L1=%C;L2=%C;L3=%C;L4=%C;L5=%C;L6=%C;L7=%C;L8=%C;T1=%C;T2=%C;";
#else
	const char *portMark = "P0=%C;P1=%C;P2=%C;P3=%C;P4=%C;P5=%C;P6=%C;P7=%C;";
#endif
#else
	const char *portMark = "L5=%C;L6=%C;L7=%C;L8=%C;";
#endif
	int i, ret;
	int fd = open(RTKSWITCH_DEV, O_RDONLY);

	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return -1;
	}

	phyState pS;

	for(i = 0 ; i < MAX_RTL_PORTS ; i++){
		pS.link[i] = 0;
		pS.speed[i] = 0;
	}

        switch(model = get_model()) {
        case MODEL_RTAC5300:
        case MODEL_RTAC5300R:
		{
		/* RTK_LAN  BRCM_LAN  WAN  POWER */
		/* R0 R1 R2 R3 B4 B0 B1 B2 B3 */
		/* L8 L7 L6 L5 L4 L3 L2 L1 W0 */
		
#if defined(RTCONFIG_EXT_RTL8370MB)
		const int porder[MAX_RTL_PORTS] = {2,3,4,0,1,5,6,7};
		o = porder;
#else
		const int porder[MAX_RTL_PORTS] = {3,2,1,0};
		o = porder;
#endif

		break;
		}
        case MODEL_RTAC88U:
		{
		/* RTK_LAN  BRCM_LAN  WAN  POWER */
		/* R3 R2 R1 R0 B3 B2 B1 B0 B4 */
		/* L8 L7 L6 L5 L4 L3 L2 L1 W0 */
		
		const int porder[4] = {0,1,2,3};
		o = porder;

		break;
		}
	default:
		{	
		const int porder[4] = {0,1,2,3};
		o = porder;

		break;
		}
	}

	if (ioctl(fd, GET_RTK_PHYSTATES, &pS) < 0) {
		perror("rtkswitch ioctl");
		close(fd);
		return -1;
	}

	close(fd);

#if defined(RTCONFIG_EXT_RTL8370MB)
	sprintf(buf, portMark,
#ifndef RTAC5300R
		(pS.link[o[0]] == 1) ? (pS.speed[o[0]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[1]] == 1) ? (pS.speed[o[1]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[2]] == 1) ? (pS.speed[o[2]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[3]] == 1) ? (pS.speed[o[3]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[4]] == 1) ? (pS.speed[o[4]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[5]] == 1) ? (pS.speed[o[5]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[6]] == 1) ? (pS.speed[o[6]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[7]] == 1) ? (pS.speed[o[7]] == 2) ? 'G' : 'M': 'X'
#else
		(pS.link[o[0]] == 1) ? (pS.speed[o[0]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[1]] == 1) ? (pS.speed[o[1]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[2]] == 1) ? (pS.speed[o[2]] == 2) ? 'G' : 'M': 'X',
		BCMPorts[2],
		(pS.link[o[3]] == 1) ? (pS.speed[o[3]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[4]] == 1) ? (pS.speed[o[4]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[5]] == 1) ? (pS.speed[o[5]] == 2) ? 'G' : 'M': 'X',
		BCMPorts[1],
		(pS.link[o[7]] == 1) ? (BCMPorts[4] == 'G' && pS.speed[o[7]] == 2) ? 'G' : 'X': 'X', 	// T1: BCM_P1/RTL_P7
		(pS.link[o[6]] == 1) ? (BCMPorts[3] == 'G' && pS.speed[o[6]] == 2) ? 'G' : 'X': 'X'	// T2: BCM_P2/RTL_P6
#endif
		);
#else
	sprintf(buf, portMark,
		(pS.link[o[0]] == 1) ? (pS.speed[o[0]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[1]] == 1) ? (pS.speed[o[1]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[2]] == 1) ? (pS.speed[o[2]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[3]] == 1) ? (pS.speed[o[3]] == 2) ? 'G' : 'M': 'X');
#endif

	if(verbose)
		printf("%s\n", buf);

	ret = 0;
	for(i = 0; i < MAX_RTL_PORTS; i++){
#ifdef RTAC5300R
		if(i >= 6) continue;
#endif
		if(pS.link[o[i]] == 1){
			ret = 1;
			break;
		}
	}
	return ret;
}

void usage(char *cmd)
{
	int ci, pa;

	/* set pa */
	for(ci = 0; ci < MAX_REQ; ++ci) {
        	switch (ci) {
        	case GET_PORT_STAT:
		case GET_PORT_SPEED:
        	case RESET_PORT:
        	case SET_EXT_TXDELAY:
        	case SET_EXT_RXDELAY:
        	case GET_REG:
		case SET_EXT_MODE:
		case SET_CPU:
		case GET_PHY_REG9:
		case GET_TMODE:
		case SET_GREEN_ETHERNET:
			rtk_cmds_pa[ci] = 1;
                	break;

        	case TEST_REG:
        	case SET_REG:
		case SET_PHY_REG9:
		case SET_TMODE:
			rtk_cmds_pa[ci] = 2;
                	break;

		default:
			rtk_cmds_pa[ci] = 0;
		}
	}

	printf("Usage:\n");
	for(ci = 0; ci < MAX_REQ; ++ci)
		printf("  %s %d %s##( %s )##\n", cmd, ci, (pa=rtk_cmds_pa[ci])?pa==1?"\t[arg1] \t":"\t[arg1] [arg2] ":ci > 9?"\t\t":" \t\t", rtk_switch_cmds[ci]);
}

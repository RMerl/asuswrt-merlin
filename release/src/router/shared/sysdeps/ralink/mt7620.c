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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bcmnvram.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <shutils.h>
#include <shared.h>
#include <utils.h>
#include <linux/config.h>
#include <ralink.h>
#include "mt7620.h"
#include "ra_ioctl.h"

#define GPIO_DEV	"/dev/gpio0"

enum {
	gpio_in,
	gpio_out,
};

#if defined(RTN14U)
/// RT-N14U mapping
enum {
WAN_PORT=0,
LAN1_PORT=1,
LAN2_PORT=2,
LAN3_PORT=3,
LAN4_PORT=4,
P5_PORT=5,
CPU_PORT=6,
P7_PORT=7,
};
//0:WAN, 1:LAN, lan_wan_partition[][0] is port0
static int lan_wan_partition[9][5] = {	{0,1,1,1,1}, //WLLLL
					{0,0,1,1,1}, //WWLLL
					{0,1,0,1,1}, //WLWLL
					{0,1,1,0,1}, //WLLWL
					{0,1,1,1,0}, //WLLLW
					{0,0,0,1,1}, //WWWLL
					{0,1,1,0,0}, //WLLWW
					{1,1,1,1,1}}; // ALL
#elif defined(RTAC52U)
/// RT-AC52U mapping
enum {
WAN_PORT=0,
LAN1_PORT=3,
LAN2_PORT=4,
LAN3_PORT=2,
LAN4_PORT=1,
P5_PORT=5,
CPU_PORT=6,
P7_PORT=7,
};
//0:WAN, 1:LAN, lan_wan_partition[][0] is port0
static int lan_wan_partition[9][5] = {	{0,1,1,1,1}, //WLLLL
					{0,1,1,0,1}, //WLLWL  port3	--> port1
					{0,1,1,1,0}, //WLLLW  port4	--> port2
					{0,1,0,1,1}, //WLWLL  port2	--> port3
					{0,0,1,1,1}, //WWLLL  port1	--> port4
					{0,1,1,0,0}, //WLLWW  port3+4	--> port1+2
					{0,0,0,1,1}, //WWWLL  port1+2	--> port3+4
					{1,1,1,1,1}}; // ALL
#endif

static int switch_port_mapping[] = {
	LAN4_PORT,	//0000 0000 0001 LAN4
	LAN3_PORT,	//0000 0000 0010 LAN3
	LAN2_PORT,	//0000 0000 0100 LAN2
	LAN1_PORT,	//0000 0000 1000 LAN1
	WAN_PORT,	//0000 0001 0000 WAN
	P5_PORT,	//0000 0010 0000 -
	P5_PORT,	//0000 0100 0000 -
	P5_PORT,	//0000 1000 0000 -
	CPU_PORT,	//0001 0000 0000 RGMII LAN
	P7_PORT,	//0010 0000 0000 RGMII WAN
};

int esw_fd, esw_stb;

int switch_init(void)
{
	esw_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (esw_fd < 0) {
		perror("socket");
		return -1;
	}
	return 0;
}

void switch_fini(void)
{
	close(esw_fd);
}

int mt7620_reg_read(int offset, unsigned int *value)
{
	struct ifreq ifr;
	esw_reg reg;

	if (value == NULL)
		return -1;
	reg.off = offset;
	strncpy(ifr.ifr_name, "eth2", 5);
	ifr.ifr_data = (void*) &reg;
	if (-1 == ioctl(esw_fd, RAETH_ESW_REG_READ, &ifr)) {
		perror("ioctl");
		close(esw_fd);
		return -1;
	}
	*value = reg.val;

	//printf("mt7620_reg_read()...offset=%4x, value=%8x\n", offset, *value);

	return 0;
}


int mt7620_reg_write(int offset, int value)
{
	struct ifreq ifr;
	esw_reg reg;

	//printf("mt7620_reg_write()..offset=%4x, value=%8x\n", offset, value);

	reg.off = offset;
	reg.val = value;
	strncpy(ifr.ifr_name, "eth2", 5);
	ifr.ifr_data = (void*) &reg;
	if (-1 == ioctl(esw_fd, RAETH_ESW_REG_WRITE, &ifr)) {
		perror("ioctl");
		close(esw_fd);
		exit(0);
	}
	return 0;
}

static void write_VTCR(unsigned int value)
{
	int i;
	unsigned int check;

	value |= 0x80000000;
	mt7620_reg_write(REG_ESW_VLAN_VTCR, value);

	for (i = 0; i < 20; i++) {
		mt7620_reg_read(REG_ESW_VLAN_VTCR, &check);
		if ((check & 0x80000000) == 0 ) //table busy
			break;
		usleep(1000);
	}
	if (i == 20)
	{
		cprintf("VTCR timeout.\n");
	}
	else if(check & (1<<16))
	{
		cprintf("%s(%08x) invalid\n", __func__, value);
	}
}

int mt7620_vlan_set(int idx, int vid, char *portmap, int stag)
{
	unsigned int i, mbr, value;

	if(idx < 0)
	{ //auto
		for(i = 4; i < 16; i++) //skip vlan index 0~3
		{
			if ((i&1) == 0)
				mt7620_reg_read(REG_ESW_VLAN_ID_BASE + 4*(i>>1), &value);

			if ((i&1) == 0)
			{
				if ((value & 0xfff) != i+1)
					continue;
			}
			else
			{
				if (((value >> 12) & 0xfff) != i+1)
					continue;
			}
			value = (0x0 << 12) + i; //read VAWD#
			write_VTCR(value);
			mt7620_reg_read(REG_ESW_VLAN_VAWD1, &value);
			if((value & 1) == 0) //find inVALID
				break;
		}
		if (i == 16)
		{
			cprintf("no empty vlan entry\n");
			return -1;
		}
		idx = i;
	}

	//printf("mt7620_vlan_set()...idx=%d, vid=%d, portmap=%s, stag=%d\n", idx, vid, portmap, stag);

	mbr = 0;
	for (i = 0; i < 8; i++)
		mbr += (*(portmap + i) - '0') * (1 << i);
	//set vlan identifier
	mt7620_reg_read(REG_ESW_VLAN_ID_BASE + 4*(idx/2), &value);
	if (idx % 2 == 0) {
		value &= 0xfff000;
		value |= vid;
	}
	else {
		value &= 0xfff;
		value |= (vid << 12);
	}
	mt7620_reg_write(REG_ESW_VLAN_ID_BASE + 4*(idx/2), value);
	//set vlan member
	value = (mbr << 16);		//PORT_MEM
	value |= (1 << 30);		//IVL
	value |= (1 << 27);		//COPY_PRI
	value |= ((stag & 0xfff) << 4);	//S_TAG
	value |= 1;			//VALID
	mt7620_reg_write(REG_ESW_VLAN_VAWD1, value);

	value = (0x80001000 + idx); //w_vid_cmd
	write_VTCR(value);

	return 0;
}


#if defined(RTN14U) || defined(RTAC52U)
int mt7620_wan_bytecount(int dir, unsigned long *count)
{
   	int offset;
 
	if (switch_init() < 0)
		return -1;

	if(dir==0) //tx
		offset=0x4018;
	else if(dir==1) //rx
		offset=0x4028;
	else
	{   
	   	perror("invalid parameter!");
		return -1;
	}

	mt7620_reg_read(offset, (unsigned int*) count);
	switch_fini();
	return 0;
}   
#endif

static void get_mt7620_esw_WAN_linkStatus(int type, unsigned int *linkStatus)
{
	int i;
	unsigned int value = 0;
	const int sw_mode = nvram_get_int("sw_mode");

	if (sw_mode == SW_MODE_AP) {
		*linkStatus = 0;
		return;
	}

	if (switch_init() < 0)
		return;

	for (i = 4; i >= 0; i--) {
		if (lan_wan_partition[type][i] == 1)
			continue;
		mt7620_reg_read((REG_ESW_MAC_PMSR_P0 + 0x100*i), &value);
		value &= 0x1;
		if (value)
			break;
	}
	*linkStatus = value;

	switch_fini();
}

static void get_mt7620_esw_LAN_linkStatus(int type, unsigned int *linkStatus)
{
	int i;
	unsigned int value = 0;
	const int sw_mode = nvram_get_int("sw_mode");

	if (switch_init() < 0)
		return;

	for (i = 4; i >= 0; i--) {
		if (lan_wan_partition[type][i] == 0 && sw_mode != SW_MODE_AP)
			continue;
		mt7620_reg_read((REG_ESW_MAC_PMSR_P0 + 0x100*i), &value);
		value &= 0x1;
		if (value)
			break;
	}
	*linkStatus = value;

	switch_fini();
}

static void config_mt7620_esw_LANWANPartition(int type)
{
	int i;
	char portmap[16];

	if (switch_init() < 0)
		return;

	//LAN/WAN ports as security mode
	for (i = 0; i < 6; i++)
		mt7620_reg_write((REG_ESW_PORT_PCR_P0 + 0x100*i), 0xff0003);
	//LAN/WAN ports as transparent port
	for (i = 0; i < 6; i++) {
			mt7620_reg_write((REG_ESW_PORT_PVC_P0 + 0x100*i), 0x810000c2); //transparent port, admit untagged frames
	}

	//set CPU/P7 port as user port
	mt7620_reg_write((REG_ESW_PORT_PVC_P0 + 0x100*CPU_PORT), 0x81000000); //port6
	mt7620_reg_write((REG_ESW_PORT_PVC_P0 + 0x100*P7_PORT), 0x81000000); //port7

	mt7620_reg_write((REG_ESW_PORT_PCR_P0 + 0x100*CPU_PORT), 0x20ff0003); //port6, Egress VLAN Tag Attribution=tagged
	mt7620_reg_write((REG_ESW_PORT_PCR_P0 + 0x100*P7_PORT), 0x20ff0003); //port7, Egress VLAN Tag Attribution=tagged

	//set PVID
	for (i = 0; i < 5; i++) {
		if (lan_wan_partition[type][i] == 1)	//LAN
			mt7620_reg_write((REG_ESW_PORT_PPBVI_P0 + 0x100*i), 0x10001);
		else					//WAN
			mt7620_reg_write((REG_ESW_PORT_PPBVI_P0 + 0x100*i), 0x10002);
	}
	mt7620_reg_write((REG_ESW_PORT_PPBVI_P0 + 0x100*P5_PORT), 0x10001);
	//VLAN member port
	//LAN
	sprintf(portmap, "%d%d%d%d%d111", lan_wan_partition[type][0]
					, lan_wan_partition[type][1]
					, lan_wan_partition[type][2]
					, lan_wan_partition[type][3]
					, lan_wan_partition[type][4]);
	mt7620_vlan_set(0, 1, portmap, 0);
	//WAN
	sprintf(portmap, "%d%d%d%d%d011", !lan_wan_partition[type][0]
					, !lan_wan_partition[type][1]
					, !lan_wan_partition[type][2]
					, !lan_wan_partition[type][3]
					, !lan_wan_partition[type][4]);
	mt7620_vlan_set(1, 2, portmap, 0);
#if 0  // Bad workaround
//// MT7620N work around
	{
	unsigned int value = 0;
	mt7620_reg_read(REG_ESW_MAC_PMCR_P6, &value);
	value &= ~(0x30);
	mt7620_reg_write(REG_ESW_MAC_PMCR_P6, value); //disable port6 flow control
	printf("Set P6 to %x\n",value);
	}
////
#endif
	switch_fini();
}

static void get_mt7620_esw_WAN_Speed(unsigned int *speed)
{
	unsigned int value = 0;

	if (switch_init() < 0)
		return;

	mt7620_reg_read(REG_ESW_MAC_PMSR_P0, &value);
	value = (value >> 2) & 0x3;
	switch (value) {
	case 0x0:
		*speed = 10;
		break;
	case 0x1:
		*speed = 100;
		break;
	case 0x2:
		*speed = 1000;
		break;
	default:
		printf("invalid!\n");
	}

	switch_fini();
}

static void link_down_up_mt7620_PHY(int type, int status, int inverse)
{
	int i;
	char idx[2];
	char value[5] = "3300";	//power up PHY

	if (!status)		//power down PHY
		value[1] = '9';

	for (i = 0; i < 5; i++) {
		if (lan_wan_partition[type][i] == inverse)
			continue;
		sprintf(idx, "%d", i);
		eval("mii_mgr", "-s", "-p", idx, "-r", "0", "-v", value);
	}
}

void set_mt7620_esw_broadcast_rate(int bsr)
{
#if 0
	int i;
	unsigned int value;
#endif

	if ((bsr < 0) || (bsr > 255))
		return;

	if (switch_init() < 0)
		return;

	printf("set broadcast strom control rate as: %d\n", bsr);
#if 0
	for (i = 0; i < 5; i++) {
		mt7620_reg_read((REG_ESW_PORT_BSR_P0 + 0x100*i), &value);
		value |= 0x1 << 31; //STRM_MODE=Rate-based
		value |= (bsr << 16) | (bsr << 8) | bsr;
		mt7620_reg_write((REG_ESW_PORT_BSR_P0 + 0x100*i), value);
	}
#endif
	switch_fini();
}

void reset_mt7620_esw(void)
{
	unsigned int value;

	if (switch_init() < 0)
		return;

	//Reset ARL engine (clear vlan table)
	mt7620_reg_read(0xc, &value);
	value &= 0xfffffffe;
	mt7620_reg_write(0xc, value);
	usleep(3000);
	mt7620_reg_read(0xc, &value);
	value |= 0x1;
	mt7620_reg_write(0xc, value);

	//set to default value
	{
		int i;
		for(i = 0; i < 8; i++)
		{
			value = ((i<<1)+1) | (((i<<1)+2) << 12);
			mt7620_reg_write(REG_ESW_VLAN_ID_BASE + 4*i, value);
		}
		for(i = 0; i < 16; i++)
		{
			value = (0x80000000 | (2<<12)) + i;	//set VLAN to invalid
			write_VTCR(value);
		}
	}

	switch_fini();
}

static void set_Vlan_VID(int vid)
{
	char tmp[8];

	sprintf(tmp, "%d", vid);
	nvram_set("vlan_vid", tmp);
}

static void set_Vlan_PRIO(int prio)
{
	char tmp[2];

	sprintf(tmp, "%d", prio);
	nvram_set("vlan_prio", tmp);
}

static void initialize_Vlan(int stb_bitmask)
{
	char portmap[16];

	if (switch_init() < 0)
		return;

	//VLAN member port: LAN
	sprintf(portmap, "%d%d%d%d%d111", lan_wan_partition[esw_stb][0]
					, lan_wan_partition[esw_stb][1]
					, lan_wan_partition[esw_stb][2]
					, lan_wan_partition[esw_stb][3]
					, lan_wan_partition[esw_stb][4]);
	mt7620_vlan_set(0, 1, portmap, 1);

	switch_fini();
}

static void create_Vlan(int bitmask)
{
	unsigned int value;
	char portmap[16];
	int vid = atoi(nvram_safe_get("vlan_vid"));
	int prio = atoi(nvram_safe_get("vlan_prio"));
	int mbr = bitmask & 0xffff;
	int untag = (bitmask >> 16) & 0xffff;
	int i, mask;

	if (switch_init() < 0)
		return;

	//set PVID & VLAN member port
	value = (0x1 << 16) | (prio << 13) | vid;

	strcpy(portmap, "00000000"); // init
	//convert port mapping
	for(i = 0; i < 5; i++)
	{
		mask = (1 << i);
		if (mbr & mask)
			portmap[ switch_port_mapping[i] ]='1';
	}

	if (mbr & 0x200) {	//Internet && WAN port
		portmap[CPU_PORT]='1';
		portmap[P7_PORT]='1';
		mt7620_reg_write((REG_ESW_PORT_PCR_P0 + 0x100*WAN_PORT), 0x10ff0003); //Egress VLAN Tag Attribution=swap
		mt7620_reg_write((REG_ESW_PORT_PCR_P0 + 0x100*CPU_PORT), 0x10ff0003); //port6(CPU), Egress VLAN Tag Attribution=swap
		mt7620_reg_write((REG_ESW_PORT_PVC_P0 + 0x100*WAN_PORT), 0x81000000); //user port, admit all frames
		mt7620_vlan_set(1, 2, portmap, vid);
		mt7620_vlan_set(2, vid, portmap, 2);
	}
	else {	//IPTV, VoIP port
		for(i = 0; i < 4; i++) //LAN port only
		{
			mask = (1 << i);
			if (mbr & mask)
			{
				if ((untag & mask) == 0)
				{	//need tag
					mt7620_reg_write((REG_ESW_PORT_PCR_P0 + 0x100 * switch_port_mapping[i]), 0x20ff0003); //Egress VLAN Tag Attribution=tagged
					mt7620_reg_write((REG_ESW_PORT_PVC_P0 + 0x100 * switch_port_mapping[i]), 0x81000000); //user port, admit all frames
				}
				else
				{
					mt7620_reg_write((REG_ESW_PORT_PPBVI_P0 + 0x100 * switch_port_mapping[i]), value);
				}
			}
		}
		mt7620_vlan_set(-1, vid, portmap, vid);
	}
	switch_fini();
}

static void is_singtel_mio(int is)
{
	int i;
	unsigned int value;

	if (!is)
		return;

	if (switch_init() < 0)
		return;

	for (i = 0; i < 5; i++) { //WAN/LAN, admit all frames
		mt7620_reg_read((REG_ESW_PORT_PVC_P0 + 0x100*i), &value);
		value &= 0xfffffffc;
		mt7620_reg_write((REG_ESW_PORT_PVC_P0 + 0x100*i), value);
	}

	switch_fini();
}

int mt7620_ioctl(int val, int val2)
{
//	int value = 0;
	unsigned int value2 = 0;

	if(nvram_match("switch_stb_x", ""))
		esw_stb = 6;
	else
		esw_stb = atoi(nvram_safe_get("switch_stb_x"));

	switch (val) {
	case 0:
		get_mt7620_esw_WAN_linkStatus(esw_stb, &value2);
		printf("WAN link status : %u\n", value2);
		break;
	case 3:
		get_mt7620_esw_LAN_linkStatus(esw_stb, &value2);
		printf("LAN link status : %u\n", value2);
		break;
	case 8:
		config_mt7620_esw_LANWANPartition(val2);
		break;
	case 13:
		get_mt7620_esw_WAN_Speed(&value2);
		printf("WAN speed : %u Mbps\n", value2);
		break;
	case 14: // Link up LAN ports
		link_down_up_mt7620_PHY(esw_stb, 1, 0);
		break;
	case 15: // Link down LAN ports
		link_down_up_mt7620_PHY(esw_stb, 0, 0);
		break;
	case 16: // Link up ALL ports
		link_down_up_mt7620_PHY(7, 1, 0);
		break;
	case 17: // Link down ALL ports
		link_down_up_mt7620_PHY(7, 0, 0);
		break;
	case 21:
		break;
	case 22:
		break;
	case 23:
		break;
	case 24:
		break;
	case 25:
		set_mt7620_esw_broadcast_rate(val2);
		break;
	case 27:
		reset_mt7620_esw();
		break;
	case 36:
		set_Vlan_VID(val2);
		break;
	case 37:
		set_Vlan_PRIO(val2);
		break;
	case 38:
		initialize_Vlan(val2);
		break;
	case 39:
		create_Vlan(val2);
		break;
	case 40:
		is_singtel_mio(val2);
		break;
	case 114: // link up WAN ports
		link_down_up_mt7620_PHY(esw_stb, 1, 1);
		break;
	case 115: // link down WAN ports
		link_down_up_mt7620_PHY(esw_stb, 0, 1);
		break;
	default:
		printf("wrong ioctl cmd: %d\n", val);
	}

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

	return mt7620_ioctl(val, val2);
}

int
ralink_set_gpio_mode(unsigned int group)
{
	int fd;
 	unsigned long value;
	unsigned int req;
	
	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	req = RALINK_SET_GPIO_MODE;
	//_dprintf("set gpio group =%d  as gpio mode\n",group);
	req= req | (group<<24); 
	value=0;
	if (ioctl(fd, req, &value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
	
}

/*
typedef struct {
	unsigned int idx;
	unsigned int value;
} asus_gpio_info;
*/
int
ralink_gpio_write_bit(int idx, int value)
{
	int fd;
       unsigned int req;

	ralink_gpio_init(idx, gpio_out);
	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	
	if (idx<=23)
	{
		if(value)
		   req=RALINK_GPIO2300_SET;
		else
		   req=RALINK_GPIO2300_CLEAR;
	
	}   
	else if (idx>23 && idx<=39)
	{	
  	  	idx -=24;
		if(value)
		   req=RALINK_GPIO3924_SET;
		else
		   req=RALINK_GPIO3924_CLEAR;
	}	
	else if (idx>39 && idx <=71)
	{	
  	 	idx -=40;
		if(value)
		   req=RALINK_GPIO7140_SET;
		else
		   req=RALINK_GPIO7140_CLEAR;
	}	
	else if (idx==72) 
	{              
#if defined(RTN14U) || defined(RTAC52U)  //wlan led
		req=RALINK_ATE_GPIO72;
		idx=value;
#else
#error invalid product!!
#endif		
	}   
	else
		return -1;

	if (ioctl(fd, req, (1<<idx)) < 0) {
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
 	unsigned long value;
	int fd;
	unsigned int req;

     	ralink_gpio_init(idx,gpio_in);
	value = 0;
	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}


	if(idx<=23)   
		 req = RALINK_GPIO2300_READ;
	else if (idx>23 && idx<=39)
	{
		idx-=24;   
	   	 req= RALINK_GPIO3924_READ;
	}	 
	else if (idx>39 && idx <=71)
	{	 
	   	idx-=40;
   	 	req= RALINK_GPIO7140_READ;
	}	
	else
		return -1;

	if (ioctl(fd, req, &value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);

	value=(value>>idx)&1;
  	return value;
}

int
ralink_gpio_init(unsigned int idx, int dir)
{
	int fd, req;
	unsigned long arg;
	
#if defined(RTN14U) || defined(RTAC52U)
	if(idx==72) //discard gpio72
		return 0;
#endif

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (dir == gpio_in) {
		if (idx <= 23)  //gpio 0~23
		{   
			req = RALINK_GPIO2300_SET_DIR_IN;
			arg=1<<idx;
		}	
		else if ((idx > 23) && (idx <= 39))  //gpio 24~39
		{	
	   	 	  req = RALINK_GPIO3924_SET_DIR_IN;
			  arg=1<<(idx-24);
		}	  
		else if ((idx > 39) && (idx <= 71))  //gpio 40~71
		{	
	   		  req = RALINK_GPIO7140_SET_DIR_IN;
			  arg=1<<(idx-40);
		}	  
		else
			return -1;
	}
	else {
		if (idx <= 23)  //gpio 0~23
		{   
			req = RALINK_GPIO2300_SET_DIR_OUT;
			arg=1<<idx;
		}	
		else if ((idx > 23) && (idx <= 39))  //gpio 24~39
		{	
	   	 	 req = RALINK_GPIO3924_SET_DIR_OUT;
			 arg=1<<(idx-24);
		}	 
		else if ((idx > 39) && (idx <= 71))  //gpio 40~71
		{	
	   	 	  req = RALINK_GPIO7140_SET_DIR_OUT;
			  arg=1<<(idx-40);
		}		
		else
			return -1;
	}

	if (ioctl(fd, req, arg) < 0) {
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

unsigned int
rtkswitch_wanPort_phyStatus(void)
{
	int stb;
	unsigned int status;
	if(nvram_match("switch_stb_x", ""))
		stb = 6;
	else
		stb = atoi(nvram_safe_get("switch_stb_x"));

	get_mt7620_esw_WAN_linkStatus(stb, &status);

	return status;
}

unsigned int
rtkswitch_lanPorts_phyStatus(void)
{
	int stb;
	unsigned int status;
	if(nvram_match("switch_stb_x", ""))
		stb = 6;
	else
		stb = atoi(nvram_safe_get("switch_stb_x"));

	get_mt7620_esw_LAN_linkStatus(stb, &status);

	return status;
}

unsigned int
rtkswitch_WanPort_phySpeed(void)
{
	unsigned int speed;

	get_mt7620_esw_WAN_Speed(&speed);

	return speed;
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

int
rtkswitch_LanPort_linkUp(void)
{
	system("rtkswitch 14");

	return 0;
}

int
rtkswitch_LanPort_linkDown(void)
{
	system("rtkswitch 15");

	return 0;
}

int
rtkswitch_AllPort_linkUp(void)
{
	system("rtkswitch 16");
        
	return 0;
}

int
rtkswitch_AllPort_linkDown(void)
{
	system("rtkswitch 17");

	return 0;
}

int
rtkswitch_Reset_Storm_Control(void)
{
	system("rtkswitch 21");

	return 0;
}

void ATE_mt7620_esw_port_status(void)
{
	int i;
	unsigned int value;
	char buf[32];
	phyState pS;

	if (switch_init() < 0)
		return;

	for (i = 0; i < 5; i++) {
		pS.link[i] = 0;
		pS.speed[i] = 0;
		mt7620_reg_read((REG_ESW_MAC_PMSR_P0 + 0x100*i), &value);
		pS.link[i] = value & 0x1;
		pS.speed[i] = (value >> 2) & 0x3;
	}

#if defined(RTAC52U)
	sprintf(buf, "W0=%C;L4=%C;L3=%C;L2=%C;L1=%C;",
		(pS.link[0] == 1) ? (pS.speed[0] == 2) ? 'G' : 'M': 'X',
		(pS.link[1] == 1) ? (pS.speed[1] == 2) ? 'G' : 'M': 'X',
		(pS.link[2] == 1) ? (pS.speed[2] == 2) ? 'G' : 'M': 'X',
		(pS.link[4] == 1) ? (pS.speed[4] == 2) ? 'G' : 'M': 'X',
		(pS.link[3] == 1) ? (pS.speed[3] == 2) ? 'G' : 'M': 'X');
#else
	sprintf(buf, "W0=%C;L1=%C;L2=%C;L3=%C;L4=%C;",
		(pS.link[0] == 1) ? (pS.speed[0] == 2) ? 'G' : 'M': 'X',
		(pS.link[1] == 1) ? (pS.speed[1] == 2) ? 'G' : 'M': 'X',
		(pS.link[2] == 1) ? (pS.speed[2] == 2) ? 'G' : 'M': 'X',
		(pS.link[3] == 1) ? (pS.speed[3] == 2) ? 'G' : 'M': 'X',
		(pS.link[4] == 1) ? (pS.speed[4] == 2) ? 'G' : 'M': 'X');
#endif
	puts(buf);

	switch_fini();
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

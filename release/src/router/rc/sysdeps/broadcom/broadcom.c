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
#include <string.h>
#include <bcmnvram.h>
#include <net/if_arp.h>
#include <shutils.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <etioctl.h>
#include <rc.h>
typedef u_int64_t __u64;
typedef u_int32_t __u32;
typedef u_int16_t __u16;
typedef u_int8_t __u8;
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <ctype.h>
#include <wlutils.h>
#include <shared.h>
#include <wlscan.h>

//This define only used for switch 53125
#define SWITCH_PORT_0_UP	0x0001
#define SWITCH_PORT_1_UP	0x0002
#define SWITCH_PORT_2_UP	0x0004
#define SWITCH_PORT_3_UP	0x0008
#define SWITCH_PORT_4_UP	0x0010

#define SWITCH_PORT_0_GIGA	0x0002
#define SWITCH_PORT_1_GIGA	0x0008
#define SWITCH_PORT_2_GIGA	0x0020
#define SWITCH_PORT_3_GIGA	0x0080
#define SWITCH_PORT_4_GIGA	0x0200
//End

//Defined for switch 5325
//#define SWITCH_ACCESS_CMD		SIOCGETCROBORD
#define SWITCH_ACCESS_PAGE		"0x1"
#define SWITCH_ACCESS_REG_LINKSTATUS	"0x0"
#define SWITCH_ACCESS_REG_LINKSPEED	"0x4"

/* hardware-dependent */
#define ETH_WAN_PORT "4"
#define ETH_LAN1_PORT "3"
#define ETH_LAN2_PORT "2"
#define ETH_LAN3_PORT "1"
#define ETH_LAN4_PORT "0"

/* RT-N53 */
/* WAN Port=4 */
#define MASK_PHYPORT 0x0010

#define ETH_WAN_PORT_UP 0x0010
#define ETH_LAN1_PORT_UP 0x0001
#define ETH_LAN2_PORT_UP 0x0002
#define ETH_LAN3_PORT_UP 0x0004
#define ETH_LAN4_PORT_UP 0x0008

#define ETH_WAN_PORT_GIGA 0x0200
#define ETH_LAN1_PORT_GIGA 0x0002
#define ETH_LAN2_PORT_GIGA 0x0008
#define ETH_LAN3_PORT_GIGA 0x0020
#define ETH_LAN4_PORT_GIGA 0x0080

#define ETH_PHY_REG_LAN_ADDR "0x1e"
#define ETH_PHY_REG_LAN_DISCONN_VALUE "0x80a8"
#define ETH_PHY_REG_LAN_CONN_VALUE "0x80a0"
//End
char cmd[32];

#ifdef RTCONFIG_EXT_RTL8365MB
extern int ext_rtk_phyState(int v);
#endif

int
set40M_Channel_2G(char *channel)
{
	char str[8];

	if( channel==NULL || !isValidChannel(1, channel) )
		return 0;

#ifdef RTCONFIG_BCMWL6
	if (atoi(channel) >= 5) sprintf(str, "%su", channel);
	else sprintf(str, "%sl", channel);
	nvram_set("wl0_chanspec", str);
	nvram_set("wl0_bw_cap", "3");
#else
	nvram_set("wl0_channel", channel);
	nvram_set("wl0_nbw_cap", "1");
	nvram_set("wl0_nctrlsb", "lower");
#endif
	nvram_set("wl0_obss_coex", "0");
	eval("wlconf", "eth1", "down");
	eval("wlconf", "eth1", "up");
	eval("wlconf", "eth1", "start");
	puts("1");
	return 1;
}

int
set40M_Channel_5G(char *channel)
{
	char str[8];
	int ch = 0;

	if( channel==NULL || !isValidChannel(0, channel) )
		return 0;

#ifdef RTCONFIG_BCMWL6
	ch = atoi(channel);
	sprintf(str, "0");
	if (ch==40||ch==48||ch==56||ch==64||ch==104||ch==112||ch==120||ch==128||ch==136||ch==153||ch==161)
		sprintf(str, "%su", channel);
	else if (ch==36||ch==44||ch==52||ch==60||ch==100||ch==108||ch==116||ch==124||ch==132||ch==149||ch==157)
		sprintf(str, "%sl", channel);
	nvram_set("wl1_chanspec", str);
	nvram_set("wl1_bw_cap", "3");
#else
	nvram_set("wl1_channel", channel);
	nvram_set("wl1_nbw_cap", "1");
	nvram_set("wl1_nctrlsb", "lower");
#endif
	nvram_set("wl1_obss_coex", "0");
	eval("wlconf", "eth2", "down");
	eval("wlconf", "eth2", "up");
	eval("wlconf", "eth2", "start");
	puts("1");
	return 1;
}

int
set80M_Channel_5G(char *channel)
{
	char str[8];
	int ch = 0;

	if( channel==NULL || !isValidChannel(0, channel) )
		return 0;

#ifdef RTCONFIG_BCMWL6
	ch = atoi(channel);
	sprintf(str, "0");
	if (ch==36||ch==40||ch==44||ch==48||ch==52||ch==56||ch==60||ch==64||
		ch==100||ch==104||ch==108||ch==112||ch==149||ch==153||ch==157||ch==161)
		sprintf(str, "%s/80", channel);
	nvram_set("wl1_chanspec", str);
	nvram_set("wl1_bw_cap", "7");
#else
	nvram_set("wl1_channel", channel);
	nvram_set("wl1_nbw_cap", "1");
	nvram_set("wl1_nctrlsb", "lower");
#endif
	nvram_set("wl1_obss_coex", "0");
	eval("wlconf", "eth2", "down");
	eval("wlconf", "eth2", "up");
	eval("wlconf", "eth2", "start");
	puts("1");
	return 1;
}

int
ResetDefault(void)
{
#ifndef RTCONFIG_BCMARM
	eval("mtd-erase","-d","nvram");
	puts("1");
#else
	int ret=0;
	if (nvram_contains_word("rc_support", "nandflash"))	/* RT-AC56S,U/RT-AC68U/RT-N18U */
#ifdef RTAC87U
		ret = mtd_erase("nvram");
#else
		ret = eval("mtd-erase2", "nvram");
#endif
	else
#if defined(RTAC1200G) || defined(RTAC1200GP)
		ret = eval("mtd-erase2", "nvram");
#else
		ret = eval("mtd-erase","-d","nvram");
#endif
#ifdef RTAC87U
	if(ret == 0) {
		return 0;
	}else{
		return -1;
	}
#else
	if(ret >= 0) {
		sleep(3);
		puts("1");
	}
	else
		puts("0");
#endif
#endif
	return 0;
}

int
GetPhyStatus(int verbose)
{
	int ports[5];
#ifdef RTCONFIG_EXT_RTL8365MB
	int ext = 0;
#endif
	int i, ret, lret=0, model, mask;
	char out_buf[30];

	model = get_model();
	switch(model) {
	case MODEL_RTN14UHP:
		/* WAN L1 L2 L3 L4 */
		ports[0]=4; ports[1]=0; ports[2]=1, ports[3]=2; ports[4]=3;
		break;
	case MODEL_RTN53:
	case MODEL_RTN15U:
	case MODEL_RTN12:
	case MODEL_RTN12B1:
	case MODEL_RTN12C1:
	case MODEL_RTN12D1:
	case MODEL_RTN12VP:
	case MODEL_RTN12HP:
	case MODEL_RTN12HP_B1:
	case MODEL_APN12HP:
	case MODEL_RTN10P:
	case MODEL_RTN10D1:
	case MODEL_RTN10PV2:
		/* WAN L1 L2 L3 L4 */
		ports[0]=4; ports[1]=3; ports[2]=2, ports[3]=1; ports[4]=0;
		break;
	case MODEL_RTN16:
	case MODEL_RTN10U:
		/* WAN L1 L2 L3 L4 */
		ports[0]=0; ports[1]=4; ports[2]=3, ports[3]=2; ports[4]=1;
		break;
	case MODEL_RTAC88U:
	case MODEL_RTAC3100:
		/* WAN L1 L2 L3 L4 */
		ports[0]=4; ports[1]=3; ports[2]=2; ports[3]=1; ports[4]=0;
#ifdef RTCONFIG_EXT_RTL8365MB
		ext = 1;
#endif
		break;
	case MODEL_RTAC56S:
	case MODEL_RTAC56U:
		/* WAN L1 L2 L3 L4 */
		ports[0]=4; ports[1]=0; ports[2]=1; ports[3]=2; ports[4]=3;
		break;

	case MODEL_RTAC87U:
		/* WAN L1 L2 L3 L4 */
		ports[0]=0; ports[1]=5; ports[2]=3; ports[3]=2; ports[4]=1;
		break;

	case MODEL_DSLAC68U:
	case MODEL_RPAC68U:
	case MODEL_RTAC68U:
	case MODEL_RTAC3200:
	case MODEL_RTN18U:
	case MODEL_RTAC53U:
	case MODEL_RTN66U:
	case MODEL_RTAC66U:
	case MODEL_RTAC1200G:
	case MODEL_RTAC1200GP:
		/* WAN L1 L2 L3 L4 */
		ports[0]=0; ports[1]=1; ports[2]=2; ports[3]=3; ports[4]=4;
		break;
	case MODEL_RTAC5300:
		/* WAN L1 L2 L3 L4 */
		ports[0]=0; ports[1]=1; ports[2]=2; ports[3]=3; ports[4]=4;
#ifdef RTCONFIG_EXT_RTL8365MB
		ext = 1;
#endif
		break;
	}

	memset(out_buf, 0, 30);
	for(i=0; i<5; i++) {
		mask = 0;
		mask |= 0x0001<<ports[i];
		if(get_phy_status(mask)==0) {/*Disconnect*/
			if(i==0)
				sprintf(out_buf, "W0=X;");
			else
				sprintf(out_buf, "%sL%d=X;", out_buf, i);
		}
		else { /*Connect, keep check speed*/
			mask = 0;
			mask |= (0x0003<<(ports[i]*2));
			ret=get_phy_speed(mask);
			ret>>=(ports[i]*2);
			if(i==0)
				sprintf(out_buf, "W0=%s;", (ret & 2)? "G":"M");
			else {
				lret = 1;
				sprintf(out_buf, "%sL%d=%s;", out_buf, i, (ret & 2)? "G":"M");
			}
		}
	}

#ifdef RTCONFIG_QTN
	if ( model == MODEL_RTAC87U ){
		ports[1] = GetPhyStatus_qtn();
		if (ports[1] == 1000){
			out_buf[8] = 'G';
		}else if (ports[1] == 100){
			out_buf[8] = 'M';
		}else if (ports[1] == 10){
			out_buf[8] = 'M';
		}else{
			out_buf[8] = 'X';
		}
	}
#endif
	if(verbose)
#ifdef RTCONFIG_EXT_RTL8365MB
		printf("%s", out_buf);
#else
		puts(out_buf);
#endif

#ifdef RTCONFIG_EXT_RTL8365MB
	if(ext)
		lret |= ext_rtk_phyState(verbose);
#endif
	return lret;
}

#ifdef RTCONFIG_LAN4WAN_LED
int LanWanLedCtrl(void)
{
	int ports[5];
	int i, ret, model, mask;
	char out_buf[30];

	model = get_model();
	switch(model) {
	case MODEL_RTN14UHP:
		/* WAN L1 L2 L3 L4 */
		ports[0]=4; ports[1]=0; ports[2]=1, ports[3]=2; ports[4]=3;
		break;
	}

	memset(out_buf, 0, 30);
	for(i=0; i<5; i++) {
		mask = 0;
		mask |= 0x0001<<ports[i];
		if(get_phy_status(mask)==0) {/*Disconnect*/
			if(i==0){
				led_control(LED_WAN, LED_OFF);
			}else{
				if ( i == 1 ) led_control(LED_LAN1, LED_OFF);
				if ( i == 2 ) led_control(LED_LAN2, LED_OFF);
				if ( i == 3 ) led_control(LED_LAN3, LED_OFF);
				if ( i == 4 ) led_control(LED_LAN4, LED_OFF);
			}
		}
		else { /*Connect, keep check speed*/
			mask = 0;
			mask |= (0x0003<<(ports[i]*2));
			ret=get_phy_speed(mask);
			ret>>=(ports[i]*2);
			if(i==0){
				led_control(LED_WAN, LED_ON);
			}else{
				if ( i == 1 ) led_control(LED_LAN1, LED_ON);
				if ( i == 2 ) led_control(LED_LAN2, LED_ON);
				if ( i == 3 ) led_control(LED_LAN3, LED_ON);
				if ( i == 4 ) led_control(LED_LAN4, LED_ON);
			}
		}
	}
	return 1;
}
#endif	/* LAN4WAN_LED*/

int
setAllLedOn(void)
{
	int model;

	led_control(LED_POWER, LED_ON);

	// generate nvram nvram according to system setting
	model = get_model();
	switch(model) {
		case MODEL_RTN16:
		case MODEL_RTN66U:
		{
			/* LAN, WAN Led On */
			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("radio", "on"); /* wireless */
			led_control(LED_USB, LED_ON);
			break;
		}
		case MODEL_RTN18U:
		{
			led_control(LED_USB, LED_ON);
			led_control(LED_USB3, LED_ON);
			led_control(LED_POWER, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_LAN, LED_ON);
			eval("wl", "-i", "eth1", "ledbh", "10", "7");
			break;
		}
		case MODEL_DSLAC68U:
		{
			led_control(LED_USB3, LED_ON);
			led_control(LED_WAN, LED_ON);
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "ledbh", "10", "1");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "10", "1");	// wl 5G
			/* 4360's fake 5g led */
			led_control(LED_5G, LED_ON);
			eval("adslate", "led", "on");
			break;
		}
		case MODEL_RTAC87U:
		{
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "ledbh", "10", "1");			// wl 2.4G
			led_control(LED_WPS, LED_ON);
			led_control(LED_WAN, LED_ON);
#ifdef RTCONFIG_QTN
			setAllLedOn_qtn();
#endif
			break;
		}
		case MODEL_RPAC68U:
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");

			eval("wl", "ledbh", "0", "1");			// wl 2.4G
			eval("wl", "ledbh", "9", "1");			// wl 2.4G
			eval("wl", "ledbh", "10", "1");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "0", "1");	// wl 5G
			eval("wl", "-i", "eth2", "ledbh", "9", "1");	// wl 5G
			eval("wl", "-i", "eth2", "ledbh", "10", "1");	// wl 5G
			break;
		case MODEL_RTAC68U:
		case MODEL_RTAC3200:
		case MODEL_RTAC5300:
		case MODEL_RTAC88U:
		case MODEL_RTAC3100:
		{
#if defined(RTAC68U) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
			led_control(LED_USB, LED_ON);
			led_control(LED_USB3, LED_ON);
#endif
#ifdef RTCONFIG_TURBO
			led_control(LED_TURBO, LED_ON);
#endif
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
#if defined(RTAC3200)
			eval("wl", "ledbh", "10", "1");			// wl 5G low
			eval("wl", "-i", "eth2", "ledbh", "10", "1");	// wl 2.4G
			eval("wl", "-i", "eth3", "ledbh", "10", "1");	// wl 5G high
#elif defined(RTAC5300)
			eval("wl", "ledbh", "9", "1");			// wl 5G low
			eval("wl", "-i", "eth2", "ledbh", "9", "1");	// wl 2.4G
			eval("wl", "-i", "eth3", "ledbh", "9", "1");	// wl 5G high
#elif defined(RTAC88U) || defined(RTAC3100)
			eval("wl", "ledbh", "9", "1");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "9", "1");	// wl 5G
#else
			eval("wl", "ledbh", "10", "1");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "10", "1");	// wl 5G
#endif

#if defined(RTAC3200)
			led_control(LED_WPS, LED_ON);
			led_control(LED_WAN, LED_ON);
#elif defined (RTAC88U) || defined (RTAC3100) || defined (RTAC5300)
			led_control(LED_WPS, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_LAN, LED_ON);
#endif
			/* 4360's fake 5g led */
#ifdef RTAC68U
			led_control(LED_5G, LED_ON);
#endif
			break;
		}
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
		{
#ifdef RTCONFIG_LED_ALL
			led_control(LED_ALL, LED_ON);
#endif
			led_control(LED_USB, LED_ON);
			led_control(LED_USB3, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_LAN, LED_ON);
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "ledbh", "3", "1");			// wl 2.4G
			eval("wl", "-i", "eth2","ledbh", "10", "1");
			/* 4352's fake 5g led */
			led_control(LED_5G, LED_ON);
			break;
		}
		case MODEL_RTAC66U:
		{
			/* LAN, WAN Led On */
			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("radio", "on"); /* 2G led */
			led_control(LED_5G, LED_ON);
			led_control(LED_USB, LED_ON);
			break;
		}
		case MODEL_RTN14UHP:
		{
			led_control(LED_POWER, LED_ON);
			/* convert from shared, boardapi.c */
			led_control(LED_WPS, LED_ON);
			led_control(LED_USB, LED_ON);
			eval("radio", "on"); /* 2G led */
			if (nvram_contains_word("rc_support", "lanwan_led2")) {
				led_control(LED_WAN, LED_ON);
#ifdef RTCONFIG_LAN4WAN_LED
				led_control(LED_LAN1, LED_ON);
				led_control(LED_LAN2, LED_ON);
				led_control(LED_LAN3, LED_ON);
				led_control(LED_LAN4, LED_ON);
#endif
			}else{
				eval("et", "robowr", "00", "0x12", "0xfd55");
			}
			break;
		}
		case MODEL_APN12HP:
		{
			led_control(LED_POWER, LED_ON);
			/* convert from shared, boardapi.c */
			nvram_set_int("led_2g_gpio", 4099);
			led_control(LED_2G, LED_ON);
			led_control(LED_WAN, LED_ON);
			break;
		}
		case MODEL_RTN10P:
		case MODEL_RTN10D1:
		case MODEL_RTN10PV2:
		{
			led_control(LED_WPS, LED_ON);
		}
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12VP:
		case MODEL_RTN12HP:
		case MODEL_RTN12HP_B1:
		{
			eval("et", "robowr", "00", "0x12", "0xfd55");
			eval("radio", "on"); /* wireless */
			break;
		}
		case MODEL_RTN10U:
		{
			led_control(LED_WPS, LED_ON);
			led_control(LED_USB, LED_ON);
			eval("et", "robowr", "00", "0x12", "0xfd55");
			eval("radio", "on"); /* wireless */
			break;
		}
		case MODEL_RTN15U:
		{
			//LAN, WAN Led On
			led_control(LED_POWER, LED_ON);
			led_control(LED_LAN, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_USB, LED_ON);
			eval("radio", "on"); /* wireless */
			break;
		}
		case MODEL_RTN53:
		{
			//LAN, WAN Led On
			led_control(LED_LAN, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_2G, LED_ON);
			led_control(LED_5G, LED_ON);
			break;
		}
		case MODEL_RTAC53U:
		{
			led_control(LED_POWER, LED_ON);
			led_control(LED_LAN, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_USB, LED_ON);
			eval("wl", "-i", "eth1", "ledbh", "3", "1");	// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "9", "1");	// wl 5G
			break;
		}
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
		{
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "-i", "eth2", "ledbh", "3", "1");	// wl 2.4G
			eval("wl", "-i", "eth3", "ledbh", "11", "1");	// wl 5G
			led_control(LED_WPS, LED_ON);
			led_control(LED_USB, LED_ON);
			break;
	
		}
	}

	wan_red_led_control(LED_ON);

	puts("1");
	return 0;
}

int
setWlOffLed(void)
{
	int model;
	int wlon_unit = nvram_get_int("wlc_band");
#ifdef PXYSTA_DUALBAND
	int wlon_unit_ex = !nvram_match("dpsta_ifnames", "") ? nvram_get_int("wlc_band_ex") : -1 ;
#else
	int wlon_unit_ex = -1;
#endif

	model = get_model();
	switch(model) {
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
		{
			if (wlon_unit != 0) {
				eval("wl", "ledbh", "3", "0");			// wl 2.4G
			} else {
				eval("wl", "-i", "eth2", "ledbh", "10", "0");	// wl 5G
				led_control(LED_5G, LED_OFF);
			}
			break;
		}
		case MODEL_RPAC68U:
		case MODEL_RTAC68U:
			if (wlon_unit != 0) {
				eval("wl", "ledbh", "10", "0");			// wl 2.4G
#ifdef RTCONFIG_LEDARRAY
				eval("wl", "ledbh", "0", "0");			// wl 2.4G
				eval("wl", "ledbh", "9", "0");			// wl 2.4G
#endif
			} else {
				eval("wl", "-i", "eth2", "ledbh", "10", "0");	// wl 5G
#ifdef RTCONFIG_LEDARRAY
				eval("wl", "-i", "eth2", "ledbh", "0", "0");	// wl 5G
				eval("wl", "-i", "eth2", "ledbh", "9", "0");	// wl 5G
#endif
#ifndef RTCONFIG_LEDARRAY
				led_control(LED_5G, LED_OFF);
#endif
			}
			break;

		case MODEL_RTAC88U:
		case MODEL_RTAC3100:
			if (wlon_unit != 0) {
                                eval("wl", "ledbh", "9", "0");                 // wl 2.4G
                        } else {
                                eval("wl", "-i", "eth2", "ledbh", "9", "0");   // wl 5G
                                led_control(LED_5G, LED_OFF);
                        }
                        break;

		case MODEL_RTAC5300:
		{
			if (wlon_unit != 0 && wlon_unit_ex != 0)
				eval("wl", "-i", "eth1", "ledbh", "9", "0");	// wl 2.4G
			if (wlon_unit != 1 && wlon_unit_ex != 1)
				eval("wl", "-i", "eth2", "ledbh", "9", "0");	// wl 5G low
			if (wlon_unit != 2 && wlon_unit_ex != 2)
				eval("wl", "-i", "eth3", "ledbh", "9", "0");	// wl 5G high
			break;
		}
		case MODEL_RTAC3200:
		{
			if (wlon_unit != 0 && wlon_unit_ex != 0)
				eval("wl", "-i", "eth2", "ledbh", "10", "0");	// wl 2.4G
			if (wlon_unit != 1 && wlon_unit_ex != 1)
				eval("wl", "ledbh", "10", "0");			// wl 5G low
			if (wlon_unit != 2 && wlon_unit_ex != 2)
				eval("wl", "-i", "eth3", "ledbh", "10", "0");	// wl 5G high
			break;
		}
		case MODEL_RTAC53U:
		{
			if (wlon_unit != 0) {
				eval("wl", "-i", "eth1", "ledbh", "3", "0");	// wl 2.4G
			} else {
				eval("wl", "-i", "eth2", "ledbh", "9", "0");	// wl 5G
			}
			break;
		}
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
		{
			eval("wl", "ledbh", "10", "0"); // wl 2.4G
			led_control(LED_5G, LED_OFF);
			break;
		}
	}

	return 0;
}

int
setAllLedOff(void)
{
	int model;

	led_control(LED_POWER, LED_OFF);

	// generate nvram nvram according to system setting
	model = get_model();
	switch(model) {
		case MODEL_RTN16:
		case MODEL_RTN66U:
		{
			/* LAN, WAN Led Off */
			eval("et", "robowr", "0", "0x18", "0x01e0");
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("radio", "off"); /* wireless */
			led_control(LED_USB, LED_OFF);
			break;
		}
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
		{
#ifdef RTCONFIG_LED_ALL
			led_control(LED_ALL, LED_OFF);
#endif
			eval("et", "robowr", "0", "0x18", "0x01e0");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "ledbh", "3", "0");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "10", "0");
			/* 4352's fake 5g led */
			led_control(LED_5G, LED_OFF);
			break;
		}
		case MODEL_RTN18U:
		{
			led_control(LED_USB, LED_OFF);
			led_control(LED_USB3, LED_OFF);
			led_control(LED_POWER, LED_OFF);
			led_control(LED_WAN, LED_OFF);
			led_control(LED_LAN, LED_OFF);
			led_control(LED_2G, LED_OFF);
			eval("wl", "-i", "eth1", "ledbh", "10", "0");
			break;
		}
		case MODEL_DSLAC68U:
		{
			char *ledcmd_argv[] = {"adslate", "led", "off", NULL};
			led_control(LED_USB3, LED_OFF);
			led_control(LED_WAN, LED_OFF);
			eval("et", "robowr", "0", "0x18", "0x01e0");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "ledbh", "10", "0");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "10", "0");
			/* 4360's fake 5g led */
			led_control(LED_5G, LED_OFF);
			_eval(ledcmd_argv, NULL, 5, NULL);
			break;
		}
		case MODEL_RTAC87U:
		{
			eval("et", "robowr", "0", "0x18", "0x01e0");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "ledbh", "10", "0");			// wl 2.4G
			led_control(LED_WPS, LED_OFF);
			led_control(LED_WAN, LED_OFF);
#ifdef RTCONFIG_QTN
			setAllLedOff_qtn();
#endif
			break;
		}
		case MODEL_RPAC68U:
			eval("et", "robowr", "0", "0x18", "0x01e0");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");

			eval("wl", "ledbh", "10", "0");			// wl 2.4G
			eval("wl", "ledbh", "0", "0");			// wl 2.4G
			eval("wl", "ledbh", "9", "0");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "10", "0");	// wl 5G
			eval("wl", "-i", "eth2", "ledbh", "0", "0");	// wl 5G
			eval("wl", "-i", "eth2", "ledbh", "9", "0");	// wl 5G
			break;
		case MODEL_RTAC68U:
		case MODEL_RTAC3200:
		case MODEL_RTAC5300:
		case MODEL_RTAC88U:
		case MODEL_RTAC3100:
		{
#if defined(RTAC68U) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
			led_control(LED_USB, LED_OFF);
			led_control(LED_USB3, LED_OFF);
#endif
#ifdef RTCONFIG_TURBO
			led_control(LED_TURBO, LED_OFF);
#endif
			eval("et", "robowr", "0", "0x18", "0x01e0");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
#if defined(RTAC3200)
			eval("wl", "ledbh", "10", "0");			// wl 5G low
			eval("wl", "-i", "eth2", "ledbh", "10", "0");	// wl 2.4G
			eval("wl", "-i", "eth3", "ledbh", "10", "0");	// wl 5G high
#elif defined (RTAC5300)
			eval("wl", "ledbh", "9", "0");			// wl 5G low
			eval("wl", "-i", "eth2", "ledbh", "9", "0");	// wl 2.4G
			eval("wl", "-i", "eth3", "ledbh", "9", "0");	// wl 5G high
#elif defined (RTAC88U) || defined (RTAC3100)
			eval("wl", "ledbh", "9", "0");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "9", "0");	// wl 5G
#else
			eval("wl", "ledbh", "10", "0");			// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "10", "0");	// wl 5G
#endif

#if defined(RTAC3200)
			led_control(LED_WPS, LED_OFF);
			led_control(LED_WAN, LED_OFF);
#elif defined (RTAC88U) || defined (RTAC3100) || defined (RTAC5300)
			led_control(LED_WPS, LED_OFF);
			led_control(LED_WAN, LED_OFF);
			led_control(LED_LAN, LED_OFF);
#endif
			/* 4360's fake 5g led */
#ifdef RTAC68U
			led_control(LED_5G, LED_OFF);
#endif
#ifdef RTCONFIG_FAKE_ETLAN_LED
			led_control(LED_LAN, LED_OFF);
#endif
			break;
		}
		case MODEL_RTAC66U:
		{
			/* LAN, WAN Led Off */
			eval("et", "robowr", "0", "0x18", "0x01e0");
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("radio", "off"); /* 2G led*/
			led_control(LED_5G, LED_OFF);
			led_control(LED_USB, LED_OFF);
			break;
		}
		case MODEL_RTN14UHP:
		{
			led_control(LED_POWER, LED_OFF);
			/* convert from shared, boardapi.c */
			led_control(LED_WPS, LED_OFF);
			led_control(LED_USB, LED_OFF);
			eval("radio", "off"); /* 2G led */
			if (nvram_contains_word("rc_support", "lanwan_led2")) {
				led_control(LED_WAN, LED_OFF);
#ifdef RTCONFIG_LAN4WAN_LED
				led_control(LED_LAN1, LED_OFF);
				led_control(LED_LAN2, LED_OFF);
				led_control(LED_LAN3, LED_OFF);
				led_control(LED_LAN4, LED_OFF);
#endif
			}else{
				eval("et", "robowr", "00", "0x12", "0xf800");
			}
			break;
		}
		case MODEL_APN12HP:
		{
			led_control(LED_POWER, LED_OFF);
			/* convert from shared, boardapi.c */
			nvram_set_int("led_2g_gpio", 4099);
			led_control(LED_2G, LED_OFF);
			led_control(LED_WAN, LED_OFF);
			break;
		}
		case MODEL_RTN10P:
		case MODEL_RTN10D1:
		case MODEL_RTN10PV2:
		{
			led_control(LED_WPS, LED_OFF);
		}
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12VP:
		case MODEL_RTN12HP:
		case MODEL_RTN12HP_B1:
		{
			eval("et", "robowr", "00", "0x12", "0xf800");
			eval("radio", "off"); /* wireless */
			break;
		}
		case MODEL_RTN10U:
		{
			led_control(LED_WPS, LED_OFF);
			led_control(LED_USB, LED_OFF);
			eval("et", "robowr", "00", "0x12", "0xf800");
			eval("radio", "off"); /* wireless */
			break;
		}
		case MODEL_RTN15U:
		{
			//LAN, WAN Led Off
			led_control(LED_POWER, LED_OFF);
			led_control(LED_LAN, LED_OFF);
			led_control(LED_WAN, LED_OFF);
			led_control(LED_USB, LED_OFF);
			eval("radio", "off"); /* wireless */
			break;
		}
		case MODEL_RTN53:
		{
			//LAN, WAN Led Off
			led_control(LED_LAN, LED_OFF);
			led_control(LED_WAN, LED_OFF);
			led_control(LED_2G, LED_OFF);
			led_control(LED_5G, LED_OFF);
			break;
		}
		case MODEL_RTAC53U:
		{
			led_control(LED_POWER, LED_OFF);
			led_control(LED_LAN, LED_OFF);
			led_control(LED_WAN, LED_OFF);
			led_control(LED_USB, LED_OFF);
			eval("wl", "-i", "eth1", "ledbh", "3", "0");	// wl 2.4G
			eval("wl", "-i", "eth2", "ledbh", "9", "0");	// wl 5G
			break;
		}
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
		{
			eval("et", "robowr", "0", "0x18", "0x01e0");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("wl", "-i", "eth2", "ledbh", "3", "0");	// wl 2.4G
			eval("wl", "-i", "eth3", "ledbh", "11", "0");    // wl 5G
			led_control(LED_WPS, LED_OFF);
			led_control(LED_USB, LED_OFF);
			break;
	
		}
	}

	wan_red_led_control(LED_OFF);

	puts("1");
	return 0;
}

int
setATEModeLedOn(void){
	int model;

	led_control(LED_POWER, LED_ON);
	model = get_model();

	switch(model) {
		case MODEL_RTN16:
		case MODEL_RTN66U:
		{
			/* LAN, WAN Led On */
			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			led_control(LED_USB, LED_ON);
			break;
		}
		case MODEL_RTN18U:
		{
			led_control(LED_USB, LED_ON);
			led_control(LED_POWER, LED_ON);
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			break;
		}
		case MODEL_DSLAC68U:
		{
			led_control(LED_USB3, LED_ON);
			led_control(LED_WAN, LED_ON);
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			eval("adslate", "led", "on");
			break;
		}
		case MODEL_RTAC87U:
		{
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			led_control(LED_WPS, LED_ON);
#ifdef RTCONFIG_QTN
			setAllLedOn_qtn();
#endif
			break;
		}
		case MODEL_RTAC68U:
		case MODEL_RTAC3200:
		{
			led_control(LED_WPS, LED_ON);
                        led_control(LED_USB, LED_ON);
                        led_control(LED_USB3, LED_ON);
#ifdef RTCONFIG_TURBO
                        led_control(LED_TURBO, LED_ON);
#endif
                        eval("et", "robowr", "0", "0x18", "0x01ff");    // lan/wan ethernet/giga led
                        eval("et", "robowr", "0", "0x1a", "0x01e0");
                        break;
		}
		//case MODEL_RPAC68U:
		case MODEL_RTAC88U:
		case MODEL_RTAC3100:
		case MODEL_RTAC5300:
		{
                        led_control(LED_WPS, LED_ON);
                        led_control(LED_WAN, LED_ON);
                        led_control(LED_LAN, LED_ON);
			led_control(LED_USB, LED_ON);
			led_control(LED_USB3, LED_ON);

			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
#if defined(RTAC5300)
                        eval("wl", "ledbh", "9", "1");                  // wl 5G low
                        eval("wl", "-i", "eth2", "ledbh", "9", "1");    // wl 2.4G
                        eval("wl", "-i", "eth3", "ledbh", "9", "1");    // wl 5G high
#elif defined(RTAC88U) || defined(RTAC3100)
                        eval("wl", "ledbh", "9", "1");                  // wl 2.4G
                        eval("wl", "-i", "eth2", "ledbh", "9", "1");    // wl 5G
#endif
			break;
		}
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
		{
#ifdef RTCONFIG_LED_ALL
			led_control(LED_ALL, LED_ON);
#endif
			led_control(LED_USB, LED_ON);
			led_control(LED_USB3, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_LAN, LED_ON);
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			break;
		}
		case MODEL_RTAC66U:
		{
			/* LAN, WAN Led On */
			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			led_control(LED_USB, LED_ON);
			break;
		}
		case MODEL_RTN10P:
		case MODEL_RTN10D1:
		case MODEL_RTN10PV2:
		{
			led_control(LED_WPS, LED_ON);
			break;
		}
		case MODEL_APN12HP:
		{
			led_control(LED_POWER, LED_ON);
			/* convert from shared, boardapi.c */
			led_control(LED_WAN, LED_ON);
			break;
		}
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12VP:
		case MODEL_RTN12HP:
		case MODEL_RTN12HP_B1:
		{
			eval("et", "robowr", "00", "0x12", "0xfd55");
			break;
		}
		case MODEL_RTN10U:
		{
			led_control(LED_WPS, LED_ON);
			led_control(LED_USB, LED_ON);
			eval("et", "robowr", "00", "0x12", "0xfd55");
			break;
		}
		case MODEL_RTN53:
		{
			/* LAN, WAN Led On */
			led_control(LED_LAN, LED_ON);
			led_control(LED_WAN, LED_ON);
			break;
		}
		case MODEL_RTAC53U:
		{
			led_control(LED_POWER, LED_ON);
			led_control(LED_LAN, LED_ON);
			led_control(LED_WAN, LED_ON);
			led_control(LED_USB, LED_ON);
			break;
		}
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
		{
			eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
			eval("et", "robowr", "0", "0x1a", "0x01e0");
			led_control(LED_WPS, LED_ON);
			led_control(LED_USB, LED_ON);
			break;
	
		}
	}

	return 0;
}

#ifdef RTCONFIG_FANCTRL
int
setFanOn(void)
{
	led_control(FAN, FAN_ON);
	if( button_pressed(BTN_FAN) )
		puts("1");
	else
		puts("ATE_ERROR");
}

int
setFanOff(void)
{
	led_control(FAN, FAN_OFF);
	if( !button_pressed(BTN_FAN) )
		puts("1");
	else
		puts("ATE_ERROR");
}
#endif

int
setWiFi2G(const char *act)
{
	if( !strcmp(act, "on") )
		eval("wl", "radio", "on");
	else if (!strcmp(act, "off"))
		eval("wl", "radio", "off");
	else
		return 0;

	puts(act);
	return 1;
}

int
setWiFi5G(const char *act)
{
	if (!strcmp(act, "on"))
		eval("wl", "-i", "eth2", "radio", "on");
	else if (!strcmp(act, "off"))
		eval("wl", "-i", "eth2", "radio", "off");
	else
		return 0;
	puts(act);
	return 1;
}

int
getWiFiStatus(const char *ifc)
{
	FILE *fp;
	char buf[128], *line;
	int ret = 1;

	if (!strcmp(ifc, "2G"))
		sprintf(buf, "wl radio");
	else if (!strcmp(ifc, "5G"))
		sprintf(buf, "wl -i eth2 radio");
	else
		return 0;

	fp = popen(buf, "r");
	if (fp == NULL) {
		perror("popen");
		return 0;
	}

	line = fgets(buf, sizeof(buf), fp);
	if (line == NULL)
		ret = 0;
	else if (strstr(line, "0x0000"))
		puts("1");
	else if (strstr(line, "0x0001"))
		puts("0");
	else
		ret = 0;

	return ret;
}

/* The below macro handle endian mis-matches between wl utility and wl driver. */
static bool g_swap = FALSE;
#define htod32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define	IW_MAX_FREQUENCIES	32

int Get_channel_list(int unit)
{
	int i, retval = 0;
	int channels[MAXCHANNEL+1];
	wl_uint32_list_t *list = (wl_uint32_list_t *) channels;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	uint ch;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	memset(tmp, 0x0, sizeof(tmp));

	memset(channels, 0, sizeof(channels));
	list->count = htod32(MAXCHANNEL);
	if (wl_ioctl(name, WLC_GET_VALID_CHANNELS , channels, sizeof(channels)) < 0)
	{
		dbg("error doing WLC_GET_VALID_CHANNELS\n");
		sprintf(tmp, "%d", 0);
		goto ERROR;
	}

	if (dtoh32(list->count) == 0)
	{
		sprintf(tmp, "%d", 0);
		goto ERROR;
	}

	retval = 1;
	for (i = 0; i < dtoh32(list->count) && i < IW_MAX_FREQUENCIES; i++) {
		ch = dtoh32(list->element[i]);

		if (i == 0)
			sprintf(tmp, "%d", ch);
		else
			sprintf(tmp,  "%s, %d", tmp, ch);
	}
ERROR:
	puts(tmp);
	return retval;
}

int Get_ChannelList_2G(void)
{
#ifndef RTAC3200_INTF_ORDER
	return Get_channel_list(0);
#else
	return Get_channel_list(1);
#endif
}

int Get_ChannelList_5G(void)
{
#ifndef RTAC3200_INTF_ORDER
	return Get_channel_list(1);
#else
	return Get_channel_list(0);
#endif
}

#if defined(RTAC3200) || defined(RTAC5300)
int Get_ChannelList_5G_2(void)
{
	return Get_channel_list(2);
}
#endif

static const unsigned char WPA_OUT_TYPE[] = { 0x00, 0x50, 0xf2, 1 };

char *wlc_nvname(char *keyword)
{
	return(wl_nvname(keyword, nvram_get_int("wlc_band"), -1));
}

int wpa_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X, WPA_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_WPA_NONE_;
	return 0;
}

int rsn_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X2_;
	if (memcmp(s, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X, RSN_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK2_;
	return 0;
}

int wpa_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

int rsn_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

int wpa_parse_wpa_ie_wpa(const unsigned char *wpa_ie, size_t wpa_ie_len, struct wpa_ie_data *data)
{
	const struct wpa_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_WPA_;
	data->pairwise_cipher = WPA_CIPHER_TKIP_;
	data->group_cipher = WPA_CIPHER_TKIP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (wpa_ie_len == 0) {
		/* No WPA IE - fail silently */
		return -1;
	}

	if (wpa_ie_len < sizeof(struct wpa_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) wpa_ie_len);
		return -1;
	}

	hdr = (const struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != DOT11_MNG_WPA_ID ||
	    hdr->len != wpa_ie_len - 2 ||
	    memcmp(&hdr->oui, WPA_OUI_TYPE_ARR, WPA_SELECTOR_LEN) != 0 ||
	    WPA_GET_LE16(hdr->version) != WPA_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = wpa_ie_len - sizeof(*hdr);

	if (left >= WPA_SELECTOR_LEN) {
		data->group_cipher = wpa_selector_to_bitfield(pos);
		pos += WPA_SELECTOR_LEN;
		left -= WPA_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes", left);
		return -1;
	}

	return 0;
}

int wpa_parse_wpa_ie_rsn(const unsigned char *rsn_ie, size_t rsn_ie_len, struct wpa_ie_data *data)
{
	const struct rsn_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_RSN_;
	data->pairwise_cipher = WPA_CIPHER_CCMP_;
	data->group_cipher = WPA_CIPHER_CCMP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X2_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (rsn_ie_len == 0) {
		/* No RSN IE - fail silently */
		return -1;
	}

	if (rsn_ie_len < sizeof(struct rsn_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) rsn_ie_len);
		return -1;
	}

	hdr = (const struct rsn_ie_hdr *) rsn_ie;

	if (hdr->elem_id != DOT11_MNG_RSN_ID ||
	    hdr->len != rsn_ie_len - 2 ||
	    WPA_GET_LE16(hdr->version) != RSN_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = rsn_ie_len - sizeof(*hdr);

	if (left >= RSN_SELECTOR_LEN) {
		data->group_cipher = rsn_selector_to_bitfield(pos);
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left >= 2) {
		data->num_pmkid = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (left < data->num_pmkid * PMKID_LEN) {
//			fprintf(stderr, "PMKID underflow "
//				   "(num_pmkid=%d left=%d)", data->num_pmkid, left);
			data->num_pmkid = 0;
		} else {
			data->pmkid = pos;
			pos += data->num_pmkid * PMKID_LEN;
			left -= data->num_pmkid * PMKID_LEN;
		}
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes - ignored", left);
	}

	return 0;
}

int wpa_parse_wpa_ie(const unsigned char *wpa_ie, size_t wpa_ie_len,
		     struct wpa_ie_data *data)
{
	if (wpa_ie_len >= 1 && wpa_ie[0] == DOT11_MNG_RSN_ID)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	else
		return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}

static const char * wpa_key_mgmt_txt(int key_mgmt, int proto)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X_:
/*
		return proto == WPA_PROTO_RSN_ ?
			"WPA2/IEEE 802.1X/EAP" : "WPA/IEEE 802.1X/EAP";
*/
		return "WPA-Enterprise";
	case WPA_KEY_MGMT_IEEE8021X2_:
		return "WPA2-Enterprise";
	case WPA_KEY_MGMT_PSK_:
/*
		return proto == WPA_PROTO_RSN_ ?
			"WPA2-PSK" : "WPA-PSK";
*/
		return "WPA-Personal";
	case WPA_KEY_MGMT_PSK2_:
		return "WPA2-Personal";
	case WPA_KEY_MGMT_NONE_:
		return "NONE";
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA_:
//		return "IEEE 802.1X (no WPA)";
		return "IEEE 802.1X";
	default:
		return "Unknown";
	}
}

static const char * wpa_cipher_txt(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE_:
		return "NONE";
	case WPA_CIPHER_WEP40_:
		return "WEP-40";
	case WPA_CIPHER_WEP104_:
		return "WEP-104";
	case WPA_CIPHER_TKIP_:
		return "TKIP";
	case WPA_CIPHER_CCMP_:
//		return "CCMP";
		return "AES";
	case (WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_):
		return "TKIP+AES";
	default:
		return "Unknown";
	}
}


int wlcscan_core(char *ofile, char *wif)
{
	int ret, i, k, left, ht_extcha;
	int retval = 0, ap_count = 0, idx_same = -1, count = 0;
	unsigned char *bssidp;
	char *info_b;
	unsigned char rate;
	unsigned char bssid[6];
	char macstr[18];
	char ure_mac[18];
	char ssid_str[256];
	wl_scan_results_t *result;
	wl_bss_info_t *info;
	wl_bss_info_107_t *old_info;
	struct bss_ie_hdr *ie;
	NDIS_802_11_NETWORK_TYPE NetWorkType;
	struct maclist *authorized;
	int maclist_size;
	int max_sta_count = 128;
	int wl_authorized = 0;
	wl_scan_params_t *params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
	FILE *fp;
	int scanmode;

	retval = 0;

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL)
		return retval;

	scanmode = nvram_get_int("wlc_scan_mode");
	if ((scanmode != DOT11_SCANTYPE_ACTIVE) && (scanmode != DOT11_SCANTYPE_PASSIVE))
		scanmode = DOT11_SCANTYPE_ACTIVE;

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
//	params->scan_type = -1;
	params->scan_type = scanmode;
//	params->scan_type = DOT11_SCANTYPE_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	while ((ret = wl_ioctl(wif, WLC_SCAN, params, params_size)) < 0 &&
				count++ < 2){
		dbg("[rc] set scan command failed, retry %d\n", count);
		sleep(1);
	}

	free(params);

	nvram_set("ap_selecting", "1");
	dbg("[rc] Please wait 4 seconds ");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".\n\n");
	nvram_set("ap_selecting", "0");

	if (ret == 0){
		count = 0;

		result = (wl_scan_results_t *)buf;
		result->buflen = WLC_IOCTL_MAXLEN - sizeof(result);

		while ((ret = wl_ioctl(wif, WLC_SCAN_RESULTS, result, WLC_IOCTL_MAXLEN)) < 0 && count++ < 2)
		{
			dbg("[rc] set scan results command failed, retry %d\n", count);
			sleep(1);
		}

		if (ret == 0)
		{
			info = &(result->bss_info[0]);

			/* Convert version 107 to 109 */
			if (dtoh32(info->version) == LEGACY_WL_BSS_INFO_VERSION) {
				old_info = (wl_bss_info_107_t *)info;
				info->chanspec = CH20MHZ_CHSPEC(old_info->channel);
				info->ie_length = old_info->ie_length;
				info->ie_offset = sizeof(wl_bss_info_107_t);
			}

			info_b = (char *) info;

			for(i = 0; i < result->count; i++)
			{
				if (info->SSID_len > 32/* || info->SSID_len == 0*/)
					goto next_info;
				bssidp = (unsigned char *)&info->BSSID;
				sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X",
										(unsigned char)bssidp[0],
										(unsigned char)bssidp[1],
										(unsigned char)bssidp[2],
										(unsigned char)bssidp[3],
										(unsigned char)bssidp[4],
										(unsigned char)bssidp[5]);

				idx_same = -1;
				for (k = 0; k < ap_count; k++){
					/* deal with old version of Broadcom Multiple SSID
						(share the same BSSID) */
					if(strcmp(apinfos[k].BSSID, macstr) == 0 &&
						strcmp(apinfos[k].SSID, (const char *) info->SSID) == 0){
						idx_same = k;
						break;
					}
				}

				if (idx_same != -1)
				{
					if (info->RSSI >= -50)
						apinfos[idx_same].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[idx_same].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[idx_same].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[idx_same].RSSI_Quality = 0;
				}
				else
				{
					strcpy(apinfos[ap_count].BSSID, macstr);
//					strcpy(apinfos[ap_count].SSID, info->SSID);
					memset(apinfos[ap_count].SSID, 0x0, 33);
					memcpy(apinfos[ap_count].SSID, info->SSID, info->SSID_len);
					apinfos[ap_count].channel = (uint8)(info->chanspec & WL_CHANSPEC_CHAN_MASK);
					if ( info->ctl_ch == 0 )
					{
						apinfos[ap_count].ctl_ch = apinfos[ap_count].channel;
					}else
					{
						apinfos[ap_count].ctl_ch = info->ctl_ch;
					}

					if (info->RSSI >= -50)
						apinfos[ap_count].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[ap_count].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[ap_count].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[ap_count].RSSI_Quality = 0;

					if ((info->capability & 0x10) == 0x10)
						apinfos[ap_count].wep = 1;
					else
						apinfos[ap_count].wep = 0;
					apinfos[ap_count].wpa = 0;

/*
					unsigned char *RATESET = &info->rateset;
					for (k = 0; k < 18; k++)
						dbg("%02x ", (unsigned char)RATESET[k]);
					dbg("\n");
*/

					NetWorkType = Ndis802_11DS;
					if ((uint8)(info->chanspec & WL_CHANSPEC_CHAN_MASK) <= 14)
					{
						for (k = 0; k < info->rateset.count; k++)
						{
							rate = info->rateset.rates[k] & 0x7f;	// Mask out basic rate set bit
							if ((rate == 2) || (rate == 4) || (rate == 11) || (rate == 22))
								continue;
							else
							{
								NetWorkType = Ndis802_11OFDM24;
								break;
							}
						}
					}
					else
						NetWorkType = Ndis802_11OFDM5;

					if (info->n_cap)
					{
						if (NetWorkType == Ndis802_11OFDM5)
						{
#ifdef RTCONFIG_BCMWL6
							if (info->vht_cap)
								NetWorkType = Ndis802_11OFDM5_VHT;
							else
#endif
								NetWorkType = Ndis802_11OFDM5_N;
						}

						else
							NetWorkType = Ndis802_11OFDM24_N;
					}

					apinfos[ap_count].NetworkType = NetWorkType;

					ap_count++;
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // look for RSN IE first
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len))
				{
					if (ie->elem_id != DOT11_MNG_RSN_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count - 1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						goto next_info;
					}
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // then look for WPA IE
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len))
				{
					if (ie->elem_id != DOT11_MNG_WPA_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count-1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						break;
					}
				}

next_info:
				info = (wl_bss_info_t *) ((unsigned char *) info + info->length);
			}
		}
	}

	/* Print scanning result to console */
	if (ap_count == 0){
		dbg("[wlc] No AP found!\n");
	}else{
		dbg("%-4s%4s%-33s%-18s%-9s%-16s%-9s%8s%3s%3s\n",
				"idx", "CH ", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "CC", "EC");
		for (k = 0; k < ap_count; k++)
		{
			dbg("%2d. ", k + 1);
			dbg("%3d ", apinfos[k].ctl_ch);
			dbg("%-33s", apinfos[k].SSID);
			dbg("%-18s", apinfos[k].BSSID);

			if (apinfos[k].wpa == 1)
				dbg("%-9s%-16s", wpa_cipher_txt(apinfos[k].wid.pairwise_cipher), wpa_key_mgmt_txt(apinfos[k].wid.key_mgmt, apinfos[k].wid.proto));
			else if (apinfos[k].wep == 1)
				dbg("WEP      Unknown         ");
			else
				dbg("NONE     Open System     ");
			dbg("%9d ", apinfos[k].RSSI_Quality);

			if (apinfos[k].NetworkType == Ndis802_11FH || apinfos[k].NetworkType == Ndis802_11DS)
				dbg("%-7s", "11b");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5)
				dbg("%-7s", "11a");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5_VHT)
				dbg("%-7s", "11ac");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5_N)
				dbg("%-7s", "11a/n");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24)
				dbg("%-7s", "11b/g");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24_N)
				dbg("%-7s", "11b/g/n");
			else
				dbg("%-7s", "unknown");

			dbg("%3d", apinfos[k].ctl_ch);

			if (	((apinfos[k].NetworkType == Ndis802_11OFDM5_VHT) ||
				 (apinfos[k].NetworkType == Ndis802_11OFDM5_N) ||
				 (apinfos[k].NetworkType == Ndis802_11OFDM24_N)) &&
					(apinfos[k].channel != apinfos[k].ctl_ch)){
				if (apinfos[k].ctl_ch < apinfos[k].channel)
					ht_extcha = 1;
				else
					ht_extcha = 0;

				dbg("%3d", ht_extcha);
			}

			dbg("\n");
		}
	}

	ret = wl_ioctl(wif, WLC_GET_BSSID, bssid, sizeof(bssid));
	memset(ure_mac, 0x0, 18);
	if (!ret){
		if ( !(!bssid[0] && !bssid[1] && !bssid[2] && !bssid[3] && !bssid[4] && !bssid[5]) ){
			sprintf(ure_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
										(unsigned char)bssid[0],
										(unsigned char)bssid[1],
										(unsigned char)bssid[2],
										(unsigned char)bssid[3],
										(unsigned char)bssid[4],
										(unsigned char)bssid[5]);
		}
	}

	if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
		maclist_size = sizeof(authorized->count) + max_sta_count * sizeof(struct ether_addr);
		authorized = malloc(maclist_size);

		// query wl for authorized sta list
		strcpy((char*)authorized, "autho_sta_list");
		if (!wl_ioctl(wif, WLC_GET_VAR, authorized, maclist_size)){
			if (authorized->count > 0) wl_authorized = 1;
		}

		if (authorized) free(authorized);
	}

	/* Print scanning result to web format */
	if (ap_count > 0){
		/* write pid */
		if ((fp = fopen(ofile, "a")) == NULL){
			printf("[wlcscan] Output %s error\n", ofile);
		}else{
#if defined(RTAC3200) || defined(RTAC5300)
			int unit = 0;
			char prefix[] = "wlXXXXXXXXXX_", tmp[100];
			wl_ioctl(wif, WLC_GET_INSTANCE, &unit, sizeof(unit));
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#endif
			for (i = 0; i < ap_count; i++){
#if defined(RTAC3200) || defined(RTAC5300)
				if (!strcmp(wif, "eth1") && (apinfos[i].ctl_ch > 48))
					continue;
				if (!strcmp(wif, "eth3")) {
					if (nvram_match(strcat_r(prefix, "country_code", tmp), "E0") ||
					    nvram_match(strcat_r(prefix, "country_code", tmp), "JP")) {
						if (apinfos[i].ctl_ch < 100)
							continue;
					} else {
						if (apinfos[i].ctl_ch < 149)
							continue;
					}
				}
#endif
				/*if(apinfos[i].ctl_ch < 0 ){
					fprintf(fp, "\"ERR_BNAD\",");
				}else */if( apinfos[i].ctl_ch > 0 &&
							 apinfos[i].ctl_ch < 14){
					fprintf(fp, "\"2G\",");
				}else if( apinfos[i].ctl_ch > 14 &&
							 apinfos[i].ctl_ch < 166){
					fprintf(fp, "\"5G\",");
				}else{
					fprintf(fp, "\"ERR_BNAD\",");
				}

				if (strlen(apinfos[i].SSID) == 0){
					fprintf(fp, "\"\",");
				}else{
					memset(ssid_str, 0, sizeof(ssid_str));
					char_to_ascii(ssid_str, apinfos[i].SSID);
					fprintf(fp, "\"%s\",", ssid_str);
				}

				fprintf(fp, "\"%d\",", apinfos[i].ctl_ch);

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_)
						fprintf(fp, "\"%s\",", "WPA-Enterprise");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X2_)
						fprintf(fp, "\"%s\",", "WPA2-Enterprise");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK_)
						fprintf(fp, "\"%s\",", "WPA-Personal");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK2_)
						fprintf(fp, "\"%s\",", "WPA2-Personal");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_NONE_)
						fprintf(fp, "\"%s\",", "NONE");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA_)
						fprintf(fp, "\"%s\",", "IEEE 802.1X");
					else
						fprintf(fp, "\"%s\",", "Unknown");
				}else if (apinfos[i].wep == 1){
					fprintf(fp, "\"%s\",", "Unknown");
				}else{
					fprintf(fp, "\"%s\",", "Open System");
				}

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_NONE_)
						fprintf(fp, "\"%s\",", "NONE");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP40_)
						fprintf(fp, "\"%s\",", "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP104_)
						fprintf(fp, "\"%s\",", "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_)
						fprintf(fp, "\"%s\",", "TKIP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_CCMP_)
						fprintf(fp, "\"%s\",", "AES");
					else if (apinfos[i].wid.pairwise_cipher == (WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_))
						fprintf(fp, "\"%s\",", "TKIP+AES");
					else
						fprintf(fp, "\"%s\",", "Unknown");
				}else if (apinfos[i].wep == 1){
					fprintf(fp, "\"%s\",", "WEP");
				}else{
					fprintf(fp, "\"%s\",", "NONE");
				}

				fprintf(fp, "\"%d\",", apinfos[i].RSSI_Quality);
				fprintf(fp, "\"%s\",", apinfos[i].BSSID);

				if (apinfos[i].NetworkType == Ndis802_11FH || apinfos[i].NetworkType == Ndis802_11DS)
					fprintf(fp, "\"%s\",", "b");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5)
					fprintf(fp, "\"%s\",", "a");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5_N)
					fprintf(fp, "\"%s\",", "an");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5_VHT)
					fprintf(fp, "\"%s\",", "ac");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24)
					fprintf(fp, "\"%s\",", "bg");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24_N)
					fprintf(fp, "\"%s\",", "bgn");
				else
					fprintf(fp, "\"%s\",", "");

				if (strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (strcmp(apinfos[i].SSID, ""))
						fprintf(fp, "\"%s\"", "0");				// none
					else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						// hidden AP (null SSID)
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								fprintf(fp, "\"%s\"", "4");
							}else{
								// in profile, connecting
								fprintf(fp, "\"%s\"", "5");
							}
						}else{
							// in profile, connected
							fprintf(fp, "\"%s\"", "4");
						}
					}else{
						// hidden AP (null SSID)
						fprintf(fp, "\"%s\"", "0");				// none
					}
				}else if (!strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (!strlen(ure_mac)){
						// in profile, disconnected
						fprintf(fp, "\"%s\",", "1");
					}else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								fprintf(fp, "\"%s\"", "2");
							}else{
								// in profile, connecting
								fprintf(fp, "\"%s\"", "3");
							}
						}else{
							// in profile, connected
							fprintf(fp, "\"%s\"", "2");
						}
					}else{
						fprintf(fp, "\"%s\"", "0");				// impossible...
					}
				}else{
					// wl0_ssid is empty
					fprintf(fp, "\"%s\"", "0");
				}

				if (i == ap_count - 1){
					fprintf(fp, "\n");
				}else{
					fprintf(fp, "\n");
				}
			}	/* for */
			fclose(fp);
		}
	}	/* if */

	return retval;
}

#ifdef RTCONFIG_WIRELESSREPEATER
/*
 *  Return value:
 *  	2 = successfully connected to parent AP
 */
int get_wlc_status(char *wif)
{
	char ure_mac[18];
	unsigned char bssid[6];
	struct maclist *authorized;
	int maclist_size;
	int max_sta_count = 128;
	int wl_authorized = 0;
	int wl_associated = 0;
	int wl_psk = 0;
	wlc_ssid_t wst = {0, ""};

	wl_psk = strstr(nvram_safe_get(wlc_nvname("akm")), "psk") ? 1 : 0;

	if (wl_ioctl(wif, WLC_GET_SSID, &wst, sizeof(wst))){
		dbg("[wlc] WLC_GET_SSID error\n");
		goto wl_ioctl_error;
	}

	memset(ure_mac, 0x0, 18);
	if (!wl_ioctl(wif, WLC_GET_BSSID, bssid, sizeof(bssid))){
		if ( !(!bssid[0] && !bssid[1] && !bssid[2] &&
				!bssid[3] && !bssid[4] && !bssid[5]) ){
			wl_associated = 1;
			sprintf(ure_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					(unsigned char)bssid[0],
					(unsigned char)bssid[1],
					(unsigned char)bssid[2],
					(unsigned char)bssid[3],
					(unsigned char)bssid[4],
					(unsigned char)bssid[5]);
		}
	}else{
		dbg("[wlc] WLC_GET_BSSID error\n");
		goto wl_ioctl_error;
	}

	if (wl_psk){
		maclist_size = sizeof(authorized->count) +
							max_sta_count * sizeof(struct ether_addr);
		authorized = malloc(maclist_size);

		if (authorized){
			// query wl for authorized sta list
			strcpy((char*)authorized, "autho_sta_list");

			if (!wl_ioctl(wif, WLC_GET_VAR, authorized, maclist_size)){
				if (authorized->count > 0) wl_authorized = 1;
				free(authorized);
			}else{
				free(authorized);
				dbg("[wlc] Authorized failed\n");
				goto wl_ioctl_error;
			}
		}
	}

	if(!wl_associated){
		dbg("[wlc] not wl_associated\n");
	}

	dbg("[wlc] wl-associated [%d]\n", wl_associated);
	dbg("[wlc] %s\n", wst.SSID);
	dbg("[wlc] %s\n", nvram_safe_get(wlc_nvname("ssid")));

	if (wl_associated &&
		!strncmp((const char *) wst.SSID, nvram_safe_get(wlc_nvname("ssid")), wst.SSID_len)){
		if (wl_psk){
			if (wl_authorized){
				dbg("[wlc] wl_authorized\n");
				return 2;
			}else{
				dbg("[wlc] not wl_authorized\n");
				return 1;
			}
		}else{
			dbg("[wlc] wl_psk:[%d]\n", wl_psk);
			return 2;
		}
	}else{
		dbg("[wlc] Not associated\n");
		return 0;
	}

wl_ioctl_error:
	return 0;
}

// TODO: wlcconnect_main
//	wireless ap monitor to connect to ap
//	when wlc_list, then connect to it according to priority
int wlcconnect_core(void)
{
	int ret = 0;
	unsigned int count;
	char word[256], *next;
	unsigned char SEND_NULLDATA[]={ 0x73, 0x65, 0x6e, 0x64,
					0x5f, 0x6e, 0x75, 0x6c,
					0x6c, 0x64, 0x61, 0x74,
					0x61, 0x00, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff};
	unsigned char bssid[6];
	int unit;

	count = 0;
	unit = 0;
	/* return WLC connection status */
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		// only one client in a system
		if(is_ure(unit)) {
			//dbg("[rc] [%s] is URE mode\n", word);
			while(count<4){
				count++;

				memset(bssid, 0xff, 6);
				if (!wl_ioctl(word, WLC_GET_BSSID, bssid, sizeof(bssid)))
					memcpy(SEND_NULLDATA + 14, bssid, 6);

				// wl send_nulldata xx:xx:xx:xx:xx:xx
				wl_ioctl(word, WLC_SET_VAR, SEND_NULLDATA,
				sizeof(SEND_NULLDATA));
				sleep(1);
			}
			ret = get_wlc_status(word);
			dbg("[wlc][%s] get_wlc_status:[%d]\n", word, ret);
		}else{
			//dbg("[rc] [%s] is not URE mode\n", word);
		}
		unit++;
	}

	return ret;
}

#endif

#if 0
bool
wl_check_assoc_scb(char *ifname)
{
	bool connected = TRUE;
	int result = 0;
	int ret = 0;

	ret = wl_iovar_getint(ifname, "scb_assoced", &result);
	if (ret) {
		dbg("failed to get scb_assoced\n");
		return connected;
	}

	connected = dtoh32(result) ? TRUE : FALSE;
	return connected;
}

int
wl_phy_rssi_ant(char *ifname)
{
	char buf[WLC_IOCTL_MAXLEN];
	int ret = 0;
	uint i;
	wl_rssi_ant_t *rssi_ant_p;

	if (!ifname)
		return -1;

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "phy_rssi_ant");

	if ((ret = wl_ioctl(ifname, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	rssi_ant_p = (wl_rssi_ant_t *)buf;
	rssi_ant_p->version = dtoh32(rssi_ant_p->version);
	rssi_ant_p->count = dtoh32(rssi_ant_p->count);

	if (rssi_ant_p->count == 0) {
		dbg("not supported on this chip\n");
	} else {
		if ((rssi_ant_p->rssi_ant[0]) &&
		    (rssi_ant_p->rssi_ant[1] < -100) &&
		    ((rssi_ant_p->count > 2)?(rssi_ant_p->rssi_ant[2] < -100) : 1))
		{
			for (i = 0; i < rssi_ant_p->count; i++)
				dbg("rssi[%d] %d  ", i, rssi_ant_p->rssi_ant[i]);
			dbg("\n");
		}
	}

	return ret;
}
#endif

#ifdef RTAC3200
extern struct nvram_tuple router_defaults[];

void
bsd_defaults(void)
{
	char extendno_org[14];
	int ext_num;
	char ext_commit_str[8];
	struct nvram_tuple *t;

	if (!strlen(nvram_safe_get("extendno_org")) ||
		nvram_match("extendno_org", nvram_safe_get("extendno")))
		return;

	strcpy(extendno_org, nvram_safe_get("extendno_org"));
	if (!strlen(extendno_org) ||
		sscanf(extendno_org, "%d-g%s", &ext_num, ext_commit_str) != 2)
		return;

//	if (strcmp(rt_serialno, "378") || (ext_num >= 4120))
//		return;

	for (t = router_defaults; t->name; t++)
		if (strstr(t->name, "bsd"))
			nvram_set(t->name, t->value);
}
#endif

#ifdef RTCONFIG_BCMWL6
int
wl_check_chanspec()
{
	wl_uint32_list_t *list;
	chanspec_t c, chansp_40m;
	int ret = 0, i, count;
	char data_buf[WLC_IOCTL_MAXLEN];
	char chanbuf[CHANSPEC_STR_LEN];
	char word[256], *next;
	char tmp[256], tmp2[256], prefix[] = "wlXXXXXXXXXX_";
	int unit = 0;
	int match;
	int match_ctrl_ch;
	int match_40m_ch;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit++);
		c = 0;
		chansp_40m = 0;
		match = 0;
		match_ctrl_ch = 0;
		match_40m_ch = 0;

		if (!nvram_get_int(strcat_r(prefix, "chanspec", tmp)))
			continue;

		memset(data_buf, 0, WLC_IOCTL_MAXLEN);
		ret = wl_iovar_getbuf(word, "chanspecs", &c, sizeof(chanspec_t),
			data_buf, WLC_IOCTL_MAXLEN);
		if (ret < 0) {
			dbg("failed to get valid chanspec list\n");
			continue;
		}

		list = (wl_uint32_list_t *)data_buf;
		count = dtoh32(list->count);

		if (!count) {
			dbg("number of valid chanspec is 0\n");
			continue;
		} else
		for (i = 0; i < count; i++) {
			c = (chanspec_t)dtoh32(list->element[i]);

			if (!match &&
			   (c == wf_chspec_aton(nvram_safe_get(strcat_r(prefix, "chanspec", tmp))))) {
				match = 1;
				break;
			}

			if (wf_chspec_ctlchan(c) == wf_chspec_ctlchan(wf_chspec_aton(nvram_safe_get(strcat_r(prefix, "chanspec", tmp))))) {
				if (!match_ctrl_ch)
					match_ctrl_ch = 1;

				if (!match_40m_ch && CHSPEC_IS40(c)) {
					match_40m_ch = 1;
					chansp_40m = c;
				}
			}
		}

		if (!match) {
			dbg("chanspec %s is invalid\n", nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));

			if (match_40m_ch) {
				dbg("downgraded to 40M chanspec\n");
				nvram_set(strcat_r(prefix, "chanspec", tmp), wf_chspec_ntoa(chansp_40m, chanbuf));
			} else if (match_ctrl_ch) {
				dbg("downgraded to 20M chanspec\n");
				nvram_set_int(strcat_r(prefix, "chanspec", tmp), wf_chspec_ctlchan(wf_chspec_aton(nvram_safe_get(strcat_r(prefix, "chanspec", tmp2)))));
			} else {
				dbg("downgraded to auto chanspec\n");
				nvram_set_int(strcat_r(prefix, "chanspec", tmp), 0);
				nvram_set_int(strcat_r(prefix, "bw", tmp), 0);
			}
		}
	}

	return ret;
}
#endif

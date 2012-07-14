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
#include <bcmnvram.h>
#include <net/if_arp.h>
#include <shutils.h>
#include <sys/signal.h>
#include <semaphore_mfp.h>
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

//This define only used for switch 53125
#define SWITCH_PORT_0_UP	0x0001
#define SWITCH_PORT_1_UP	0x0002
#define SWITCH_PORT_2_UP        0x0004
#define SWITCH_PORT_3_UP        0x0008
#define SWITCH_PORT_4_UP        0x0010

#define SWITCH_PORT_0_GIGA	0x0002
#define SWITCH_PORT_1_GIGA      0x0008
#define SWITCH_PORT_2_GIGA      0x0020
#define SWITCH_PORT_3_GIGA      0x0080
#define SWITCH_PORT_4_GIGA      0x0200
//End

//Defined for switch 5325
#define SWITCH_ACCESS_CMD SIOCGETCROBORD
#define SWITCH_ACCESS_PAGE "0x1"
#define SWITCH_ACCESS_REG_LINKSTATUS "0x0"
#define SWITCH_ACCESS_REG_LINKSPEED "0x4"

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

int 
setCommit(void)
{
	FILE *fp;
	char line[32];
	eval("rm", "-f", "/var/log/commit_ret");
        eval("nvram", "set", "asuscfecommit=");
        eval("nvram", "commit");
	sleep(1);
	if((fp = fopen("/var/log/commit_ret", "r"))!=NULL) {
		while(fgets(line, sizeof(line), fp)){
			if(strstr(line, "OK")) {
				puts("1");
				return 1;
			}
		}
	}
	
	puts("0");

	return 1;
}

int
setMAC_2G(const char *mac)
{
	char cmd_l[64];
	int model;

	if( mac==NULL || !isValidMacAddr(mac) )
		return 0;

	// generate nvram nvram according to system setting
	model = get_model();

	eval("killall", "wsc");

	switch(model) {
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
		case MODEL_RTN10D:
		case MODEL_RTN16:
		{
			memset(cmd_l, 0, 64);
			sprintf(cmd_l, "asuscfeet0macaddr=%s", mac);
			eval("nvram", "set", cmd_l );
			sprintf(cmd_l, "asuscfesb/1/macaddr=%s", mac);
			eval("nvram", "set", cmd_l );
			puts(nvram_safe_get("et0macaddr"));
 			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			memset(cmd_l, 0, 64);
			sprintf(cmd_l, "asuscfeet0macaddr=%s", mac);
			eval("nvram", "set", cmd_l );
			sprintf(cmd_l, "asuscfepci/1/1/macaddr=%s", mac);
			eval("nvram", "set", cmd_l );
			puts(nvram_safe_get("et0macaddr"));
 			break;
		}
	}
	return 1;
}

int
setMAC_5G(const char *mac)
{
	char cmd_l[64];
	int model;

	if( mac==NULL || !isValidMacAddr(mac) )
		return 0;

	// generate nvram nvram according to system setting
	model = get_model();

	eval("killall", "wsc");

	switch(model) {
		case MODEL_RTN53:
		{
			memset(cmd_l, 0, 64);
			sprintf(cmd_l, "asuscfe0:macaddr=%s", mac);
			eval("nvram", "set", cmd_l );
			puts(nvram_safe_get("0:macaddr"));
 			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			memset(cmd_l, 0, 64);
			sprintf(cmd_l, "asuscfepci/2/1/macaddr=%s", mac);
			eval("nvram", "set", cmd_l );
			puts(nvram_safe_get("pci/2/1/macaddr"));
 			break;
		}
	}
	return 1;
}

int
setCountryCode_2G(const char *cc)
{
	if( cc==NULL || !isValidCountryCode(cc) )
		return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscferegulation_domain=%s", cc);
        eval("nvram", "set", cmd );
        puts(nvram_safe_get("regulation_domain"));
        return 1;
}

int
setCountryCode_5G(const char *cc)
{
	if( cc==NULL || !isValidCountryCode(cc) )
                return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscferegulation_domain_5G=%s", cc);
        eval("nvram", "set", cmd );
        puts(nvram_safe_get("regulation_domain_5G"));
        return 1;
}

int
setRegrev_2G(const char *regrev)
{
	int model;

	if( regrev==NULL || !isValidRegrev(regrev) )
		return 0;

	// generate nvram nvram according to system setting
	model = get_model();

	eval("killall", "wsc");

	switch(model) {
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
		case MODEL_RTN10D:
		case MODEL_RTN16:
		{
			memset(cmd, 0, 32);
			sprintf(cmd, "asuscfesb/1/regrev=%s", regrev);
			eval("nvram", "set", cmd );
			puts(nvram_safe_get("sb/1/regrev"));
			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			memset(cmd, 0, 32);
			sprintf(cmd, "asuscfepci/1/1/regrev=%s", regrev);
			eval("nvram", "set", cmd );
			puts(nvram_safe_get("pci/1/1/regrev"));
			break;
		}
	}
	return 1;
}

int
setRegrev_5G(const char *regrev)
{
	int model;

	if( regrev==NULL || !isValidRegrev(regrev) )
		return 0;

	// generate nvram nvram according to system setting
	model = get_model();

	eval("killall", "wsc");

	switch(model) {
		case MODEL_RTN53:
		{
			memset(cmd, 0, 32);
			sprintf(cmd, "asuscfe0:regrev=%s", regrev);
			eval("nvram", "set", cmd );
			puts(nvram_safe_get("0:regrev"));
			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			memset(cmd, 0, 32);
			sprintf(cmd, "asuscfepci/2/1/regrev=%s", regrev);
			eval("nvram", "set", cmd );
			puts(nvram_safe_get("pci/2/1/regrev"));
			break;
		}
	}
	return 1;
}

int
setPIN(const char *pin)
{
        if( pin==NULL ) 
		return 0;

	if( pincheck(pin) )
        {
                eval("killall", "wsc");
                memset(cmd, 0, 32);
                sprintf(cmd, "asuscfesecret_code=%s", pin);
                eval("nvram", "set", cmd );
                puts(nvram_safe_get("secret_code"));
                return 1;
	}
	else
		return 0; 
}

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
	eval("wlconf", "eth1", "down" );
	eval("wlconf", "eth1", "up" );
	eval("wlconf", "eth1", "start" );
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
        eval("wlconf", "eth2", "down" );
        eval("wlconf", "eth2", "up" );
        eval("wlconf", "eth2", "start" );
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
        eval("wlconf", "eth2", "down" );
        eval("wlconf", "eth2", "up" );
        eval("wlconf", "eth2", "start" );
	puts("1");
        return 1;
}

int
ResetDefault(void) {
	eval("mtd-erase","-d","nvram");
        puts("1");
	return 0;
}

static void
syserr(char *s)
{
        perror(s);
        exit(1);
}

static int
et_check(int s, struct ifreq *ifr)
{
        struct ethtool_drvinfo info;

        memset(&info, 0, sizeof(info));
        info.cmd = ETHTOOL_GDRVINFO;
        ifr->ifr_data = (caddr_t)&info;
        if (ioctl(s, SIOCETHTOOL, (caddr_t)ifr) < 0) {
                /* print a good diagnostic if not superuser */
                if (errno == EPERM)
                        syserr("siocethtool");
                return (-1);
        }

        if (!strncmp(info.driver, "et", 2))
                return (0);
        else if (!strncmp(info.driver, "bcm57", 5))
                return (0);

        return (-1);
}

static void
et_find(int s, struct ifreq *ifr)
{
        char proc_net_dev[] = "/proc/net/dev";
        FILE *fp;
        char buf[512], *c, *name;

        ifr->ifr_name[0] = '\0';

        /* eat first two lines */
        if (!(fp = fopen(proc_net_dev, "r")) ||
            !fgets(buf, sizeof(buf), fp) ||
            !fgets(buf, sizeof(buf), fp))
                return;

        while (fgets(buf, sizeof(buf), fp)) {
                c = buf;
                while (isspace(*c))
                        c++;
                if (!(name = strsep(&c, ":")))
                        continue;
                strncpy(ifr->ifr_name, name, IFNAMSIZ);
                if (et_check(s, ifr) == 0)
                        break;
                ifr->ifr_name[0] = '\0';
        }

        fclose(fp);
}

/*
int
Get53125Status(void)
{
        char switch_link_status[]="GGGGG";
        struct ifreq ifr;
        int vecarg[5];
        int s;
	char output[25];
        int model;

        // generate nvram nvram according to system setting
        model = get_model();

        if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
                syserr("socket");

        et_find(s, &ifr);

        if (!*ifr.ifr_name) {
                 printf("et interface not found\n");
                 return 0;
        }

	//Get Link Speed
        vecarg[0] = strtoul("0x1", NULL, 0) << 16;;
        vecarg[0] |= strtoul("0x4", NULL, 0) & 0xffff;

        ifr.ifr_data = (caddr_t) vecarg;
        if (ioctl(s, SIOCGETCROBORD, (caddr_t)&ifr) < 0)
                syserr("etcrobord");
        else {
                if( !(vecarg[1] & SWITCH_PORT_0_GIGA) )
                        memcpy(&switch_link_status[0], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_1_GIGA) )
                        memcpy(&switch_link_status[1], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_2_GIGA) )
                        memcpy(&switch_link_status[2], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_3_GIGA) )
                        memcpy(&switch_link_status[3], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_4_GIGA) )
                        memcpy(&switch_link_status[4], "M", 1);
        }

        //Get Link Status
        vecarg[0] = strtoul("0x1", NULL, 0) << 16;;
        vecarg[0] |= strtoul("0x0", NULL, 0) & 0xffff;

        ifr.ifr_data = (caddr_t) vecarg;
        if (ioctl(s, SIOCGETCROBORD, (caddr_t)&ifr) < 0)
                syserr("etcrobord");
        else {
                if( !(vecarg[1] & SWITCH_PORT_0_UP) )
                        memcpy(&switch_link_status[0], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_1_UP) )
                        memcpy(&switch_link_status[1], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_2_UP) )
                        memcpy(&switch_link_status[2], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_3_UP) )
                        memcpy(&switch_link_status[3], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_4_UP) )
                        memcpy(&switch_link_status[4], "X", 1);
        }

        switch(model) {
                case MODEL_RTN66U:
		case MODEL_RTAC66U:
                {
			sprintf(output, "W0=%c;L1=%c;L2=%c;L3=%c;L4=%c", switch_link_status[0],
				switch_link_status[1], switch_link_status[2],
				switch_link_status[3], switch_link_status[4]);
			break;
		}
	}
	puts(output);

        return 1;
}

int
Get5325Status(void)
{
	char phy_status[26], phy_port[2], tmp_status[6];
	struct ifreq ifr;
	int vecarg[5];
	int s, i;

	memset(phy_status, 0, 26);
	memset(phy_port, 0, 2);
	memset(tmp_status, 0, 6);
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		syserr("[rc] GetPhyStatus socket error");

	et_find(s, &ifr);

	if (!*ifr.ifr_name) {
		fprintf(stderr, "[rc] %s et interface not found\n", __FUNCTION__);
		return 0;
	}

	//Get Wan Port Status
	vecarg[0] = strtoul(ETH_WAN_PORT, NULL, 0) << 16;;
	vecarg[0] |= strtoul("0x01", NULL, 0) & 0xffff;

	ifr.ifr_data = (caddr_t) vecarg;
	if (ioctl(s, SIOCGETCPHYRD2, (caddr_t)&ifr) < 0)
		syserr("etcphyrd");
	else {
		if(vecarg[1] == 30729)
			strcpy(phy_status, "W0=X;");
		else
			strcpy(phy_status, "W0=M;");
	}

	//Get Lan port status
	for( i=1; i<=4; i++ ) {
		if( i == 1 )
			vecarg[0] = strtoul(ETH_LAN1_PORT, NULL, 0) << 16;
		else if( i == 2 )
			vecarg[0] = strtoul(ETH_LAN2_PORT, NULL, 0) << 16;
		else if( i == 3 )
			vecarg[0] = strtoul(ETH_LAN3_PORT, NULL, 0) << 16;
		else if( i == 4 )
			vecarg[0] = strtoul(ETH_LAN4_PORT, NULL, 0) << 16;
		else{
			fprintf(stderr, "[rc] error: ETH PORT is not defined\n");
		}
			
		vecarg[0] |= strtoul("0x01", NULL, 0) & 0xffff;

		ifr.ifr_data = (caddr_t) vecarg;
		if (ioctl(s, SIOCGETCPHYRD2, (caddr_t)&ifr) < 0){
			syserr("etcphyrd");
		}else {
			if(vecarg[1] == 30729)
				sprintf(tmp_status, "L%d=X;", i);
			else
				sprintf(tmp_status, "L%d=M;", i);
		}
		strcat(phy_status, tmp_status);
	}
	puts(phy_status);
	return 1;
}

int 
GetPhyStatus(void)
{
        int model;

        // generate nvram nvram according to system setting
        model = get_model();

	switch(model) {
                case MODEL_RTN12:
                case MODEL_RTN12B1:
                case MODEL_RTN12C1:
		case MODEL_RTN53:
		{
			return Get5325Status();
			break;
		}
		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			return Get53125Status();
			break;
		}
	}
	return 0;
}
*/

int
GetPhyStatus(void)
{
	int ports[5];
        int i, ret, model, mask;
        char out_buf[30];

        model = get_model();
        switch(model) {
                case MODEL_RTN12:
                case MODEL_RTN12B1:
                case MODEL_RTN12C1:
                case MODEL_RTN15U:
                case MODEL_RTN53:
                {       /* WAN L1 L2 L3 L4 */
                        ports[0]=4; ports[1]=3; ports[2]=2, ports[3]=1; ports[4]=0;
                        break;
                }
                case MODEL_RTN16:
                {       /* WAN L1 L2 L3 L4 */
			ports[0]=0; ports[1]=4; ports[2]=3, ports[3]=2; ports[4]=1;
                        break;
                }
                case MODEL_RTN10U:
                case MODEL_RTN10D:
                case MODEL_RTN66U:
                case MODEL_RTAC66U:
                {
			/* WAN L1 L2 L3 L4 */
			ports[0]=0; ports[1]=1;	ports[2]=2; ports[3]=3; ports[4]=4;
                        break;
                }
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
                        else 
                                sprintf(out_buf, "%sL%d=%s;", out_buf, i, (ret & 2)? "G":"M");

                }
	}

	puts(out_buf);
	return 1;
}

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
		case MODEL_RTAC66U:
		{
	        	/* LAN, WAN Led On */
        		eval("et", "robowr", "0", "0x18", "0x01ff");
        		eval("et", "robowr", "0", "0x1a", "0x01e0");
        		eval("radio", "on"); /* 2G led */
                        gpio_write(LED_5G, 1);
                        led_control(LED_5G, LED_ON);			
			led_control(LED_USB, LED_ON);
			break;
		}
                case MODEL_RTN53:
                {
			//LAN, WAN Led On
			led_control(LED_LAN, LED_ON);
			led_control(LED_WAN, LED_ON);
			gpio_write(LED_2G, 1);
			led_control(LED_2G, LED_ON);
			gpio_write(LED_5G, 1);
			led_control(LED_5G, LED_ON);
                        break;
                }
	}

	puts("1");
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
		case MODEL_RTAC66U:
                {
			/* LAN, WAN Led Off */
		        eval("et", "robowr", "0", "0x18", "0x01e0");
		        eval("et", "robowr", "0", "0x1a", "0x01e0");
                        eval("radio", "off"); /* 2G led*/
                        gpio_write(LED_5G, 1);
                        led_control(LED_5G, LED_OFF);
			led_control(LED_USB, LED_OFF);
                        break;
                }
                case MODEL_RTN53:
                {
                        //LAN, WAN Led Off
                        led_control(LED_LAN, LED_OFF);
                        led_control(LED_WAN, LED_OFF);
                        gpio_write(LED_2G, 1);
                        led_control(LED_2G, LED_OFF);
                        gpio_write(LED_5G, 1);
                        led_control(LED_5G, LED_OFF);
                        break;
                }
        }

	puts("1");
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
getMAC_2G() {
	puts(nvram_safe_get("et0macaddr"));
	return 0;
}

int
getMAC_5G() {
	int model;

	// generate nvram nvram according to system setting
	model = get_model();

	switch(model) {
		case MODEL_RTN53:
		{
			puts(nvram_safe_get("0:macaddr"));
			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			puts(nvram_safe_get("pci/2/1/macaddr"));
			break;
		}
	}
	return 0;
}

int 
getBootVer(void) {
	char buf[32];
	memset(buf, 0, 32);

	if(get_model()==MODEL_RTN53)
		strcpy(buf, nvram_safe_get("hardware_version"));
	else	
		sprintf(buf,"%s-%s",nvram_safe_get("productid"),nvram_safe_get("bl_version"));
	puts(buf);
	return 0;
}

int 
getPIN(void) {
	puts(nvram_safe_get("secret_code"));
	return 0;
}

int
getCountryCode_2G(void) {
	puts(nvram_safe_get("regulation_domain"));
	return 0;
}

int
getCountryCode_5G(void) {
        puts(nvram_safe_get("regulation_domain_5G"));
	return 0;
}

int 
getRegrev_2G(void) {
	int model;

	// generate nvram nvram according to system setting
	model = get_model();

	switch(model) {
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
		case MODEL_RTN10D:
		case MODEL_RTN16:
		{
			puts(nvram_safe_get("sb/1/regrev"));
			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			puts(nvram_safe_get("pci/1/1/regrev"));
			break;
		}
	}
	return 0;
}

int
getRegrev_5G(void) {
	int model;

	// generate nvram nvram according to system setting
	model = get_model();

	switch(model) {
		case MODEL_RTN53:
		{
			puts(nvram_safe_get("0:regrev"));
			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{
			puts(nvram_safe_get("pci/2/1/regrev"));
			break;
		}
	}
	return 0;
}

int setSN(const char *SN)
{
	char cmd_l[64];

	if(SN==NULL || !isValidSN(SN))
		return 0;

        memset(cmd_l, 0, 64);
        sprintf(cmd_l, "asuscfeserial_no=%s", SN);
        eval("nvram", "set", cmd_l );
        puts(nvram_safe_get("serial_no"));
        return 1;
}

int getSN(void)
{
	puts(nvram_safe_get("serial_no"));
	return 0;
}

void
Get_fail_ret(void)
{
	puts(nvram_safe_get("Ate_power_on_off_ret"));
}

void
Get_fail_reboot_log(void)
{
	puts(nvram_safe_get("Ate_fail_reboot_log"));
}

void
Get_fail_dev_log(void)
{
	puts(nvram_safe_get("Ate_fail_dev_log"));
}


void ate_commit_bootlog(char *err_code) {
	nvram_set("Ate_power_on_off_enable", err_code);
	nvram_commit();
	nvram_set("asuscfeAte_power_on_off_ret", err_code);
	nvram_set("asuscfeAte_fail_reboot_log", nvram_get("Ate_reboot_log"));
	nvram_set("asuscfeAte_fail_dev_log", nvram_get("Ate_dev_log"));
	nvram_set("asuscfecommit=", "1");
}

int Get_channel_list(int unit)
{
        int i, retval = 0;
        char buf[4096];
        wl_channels_in_country_t *cic = (wl_channels_in_country_t *)buf;
        char tmp[256], prefix[] = "wlXXXXXXXXXX_";
        char *country_code;
        char *name;
        channel_info_t ci;

        snprintf(prefix, sizeof(prefix), "wl%d_", unit);
        country_code = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
        name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

        cic->buflen = sizeof(buf);
        strcpy(cic->country_abbrev, country_code);
        if (!unit)
                cic->band = WLC_BAND_2G;
        else
                cic->band = WLC_BAND_5G;
        cic->count = 0;

        if (wl_ioctl(name, WLC_GET_CHANNELS_IN_COUNTRY, cic, cic->buflen) != 0)
                return retval;

        if (cic->count == 0)
                return retval;
        else
        {
                memset(tmp, 0x0, sizeof(tmp));

                for (i = 0; i < cic->count; i++)
                {
                        if (i == 0)
                                sprintf(tmp, "%d", cic->channel[i]);
                        else
                                sprintf(tmp,  "%s, %d", tmp, cic->channel[i]);
                }

                puts(tmp);
        }

        return 1;
}

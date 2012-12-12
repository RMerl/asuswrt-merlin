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
#include <shared.h>
#ifdef RTCONFIG_WIRELESSREPEATER
#include <wlscan.h>
#endif

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
				fclose(fp);
				return 1;
			}
		}
		fclose(fp);
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
		case MODEL_RTN12D1:
		case MODEL_RTN12HP:
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
		case MODEL_RTN10D1:
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
		case MODEL_RTN12D1:
		case MODEL_RTN12HP:
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
		case MODEL_RTN10D1:
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
                case MODEL_RTN12D1:
                case MODEL_RTN12HP:
                case MODEL_RTN15U:
                case MODEL_RTN53:
                {       /* WAN L1 L2 L3 L4 */
                        ports[0]=4; ports[1]=3; ports[2]=2, ports[3]=1; ports[4]=0;
                        break;
                }
                case MODEL_RTN16:
                case MODEL_RTN10U:
                {       /* WAN L1 L2 L3 L4 */
			ports[0]=0; ports[1]=4; ports[2]=3, ports[3]=2; ports[4]=1;
                        break;
                }
                case MODEL_RTN10D1:
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
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12HP:
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
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12HP:
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

	if(get_model()==MODEL_RTN53 ||
		get_model()==MODEL_RTN10U ||
		get_model()==MODEL_RTN12B1 || get_model()==MODEL_RTN12C1 ||
		get_model()==MODEL_RTN12D1 || get_model()==MODEL_RTN12HP ||
		get_model()==MODEL_RTN15U)
		strcpy(buf, nvram_safe_get("hardware_version"));
	else
		sprintf(buf,"%s-%s",get_productid(),nvram_safe_get("bl_version"));
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
		case MODEL_RTN12D1:
		case MODEL_RTN12HP:
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
		case MODEL_RTN10D1:
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
#ifdef RTCONFIG_ODMPID
int setMN(const char *MN)
{
	char cmd_l[64];

	if(MN==NULL || !is_valid_hostname(MN))
		return 0;

	memset(cmd_l, 0, 64);
	sprintf(cmd_l, "asuscfeodmpid=%s", MN);
	eval("nvram", "set", cmd_l );
	puts(nvram_safe_get("odmpid"));
	return 1;
}

int getMN(void)
{
	puts(nvram_safe_get("odmpid"));
	return 1;
}
#endif
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
	//nvram_set("asuscfeAte_fail_reboot_log", nvram_get("Ate_reboot_log"));
	//nvram_set("asuscfeAte_fail_dev_log", nvram_get("Ate_dev_log"));
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

#ifdef RTCONFIG_WIRELESSREPEATER
char *wlc_nvname(char *keyword) 
{
	return(wl_nvname(keyword, nvram_get_int("wlc_band"), -1));
}

static const unsigned char WPA_OUT_TYPE[] = { 0x00, 0x50, 0xf2, 1 };

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
	struct bss_ie_hdr *ie;
	NDIS_802_11_NETWORK_TYPE NetWorkType;
	struct maclist *authorized;
	int maclist_size;
	int max_sta_count = 128;
	int wl_authorized = 0;
	wl_scan_params_t *params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
	FILE *fp;

	retval = 0;

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL)
		return retval;

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
//	params->scan_type = -1;
	params->scan_type = DOT11_SCANTYPE_ACTIVE;
//	params->scan_type = DOT11_SCANTYPE_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	while ((ret = wl_ioctl(wif, WLC_SCAN, params, params_size)) < 0 &&
				count++ < 2){
		fprintf(stderr, "[rc] set scan command failed, retry %d\n", count);
		sleep(1);
	}

	free(params);

	nvram_set("ap_selecting", "1");
	fprintf(stderr, "[rc] Please wait 4 seconds\n\n");
	sleep(4);
	nvram_set("ap_selecting", "0");

	if (ret == 0){
		count = 0;

		result = (wl_scan_results_t *)buf;
		result->buflen = WLC_IOCTL_MAXLEN - sizeof(result);

		while ((ret = wl_ioctl(wif, WLC_SCAN_RESULTS, result, WLC_IOCTL_MAXLEN)) < 0 && count++ < 2)
		{
			fprintf(stderr, "[rc] set scan results command failed, retry %d\n", count);
			sleep(1);
		}

		if (ret == 0)
		{
			info = &(result->bss_info[0]);
			info_b = (unsigned char *)info;

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
						strcmp(apinfos[k].SSID, info->SSID) == 0){
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
//					apinfos[ap_count].channel = ((wl_bss_info_107_t *) info)->channel;
					apinfos[ap_count].channel = info->chanspec;
					apinfos[ap_count].ctl_ch = info->ctl_ch;

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
						fprintf(stderr, "%02x ", (unsigned char)RATESET[k]);
					fprintf(stderr, "\n");
*/

					NetWorkType = Ndis802_11DS;
//					if (((wl_bss_info_107_t *) info)->channel <= 14)
					if ((uint8)info->chanspec <= 14)
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
							NetWorkType = Ndis802_11OFDM5_N;
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
		fprintf(stderr, "[wlc] No AP found!\n");
	}else{
		fprintf(stderr, "%-4s%-3s%-33s%-18s%-9s%-16s%-9s%8s%3s%3s\n",
				"idx", "CH", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "CC", "EC");
		for (k = 0; k < ap_count; k++)
		{
			fprintf(stderr, "%2d. ", k + 1);
			fprintf(stderr, "%2d ", apinfos[k].channel);
			fprintf(stderr, "%-33s", apinfos[k].SSID);
			fprintf(stderr, "%-18s", apinfos[k].BSSID);

			if (apinfos[k].wpa == 1)
				fprintf(stderr, "%-9s%-16s", wpa_cipher_txt(apinfos[k].wid.pairwise_cipher), wpa_key_mgmt_txt(apinfos[k].wid.key_mgmt, apinfos[k].wid.proto));
			else if (apinfos[k].wep == 1)
				fprintf(stderr, "WEP      Unknown         ");
			else
				fprintf(stderr, "NONE     Open System     ");
			fprintf(stderr, "%9d ", apinfos[k].RSSI_Quality);

			if (apinfos[k].NetworkType == Ndis802_11FH || apinfos[k].NetworkType == Ndis802_11DS)
				fprintf(stderr, "%-7s", "11b");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5)
				fprintf(stderr, "%-7s", "11a");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5_N)
				fprintf(stderr, "%-7s", "11a/n");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24)
				fprintf(stderr, "%-7s", "11b/g");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24_N)
				fprintf(stderr, "%-7s", "11b/g/n");
			else
				fprintf(stderr, "%-7s", "unknown");

			fprintf(stderr, "%3d", apinfos[k].ctl_ch);

			if (	((apinfos[k].NetworkType == Ndis802_11OFDM5_N) ||
					 (apinfos[k].NetworkType == Ndis802_11OFDM24_N)) &&
					(apinfos[k].channel != apinfos[k].ctl_ch)	){
				if (apinfos[k].ctl_ch < apinfos[k].channel)
					ht_extcha = 1;
				else
					ht_extcha = 0;

				fprintf(stderr, "%3d", ht_extcha);
			}

			fprintf(stderr, "\n");
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
		spinlock_lock(SPINLOCK_SiteSurvey);
		if ((fp = fopen(ofile, "a")) == NULL){
			printf("[wlcscan] Output %s error\n", ofile);
		}else{
			for (i = 0; i < ap_count; i++){
				if(apinfos[i].channel < 0 ){
					fprintf(fp, "\"ERR_BNAD\",");
				}else if( apinfos[i].channel > 0 &&
							 apinfos[i].channel < 14){
					fprintf(fp, "\"2G\",");
				}else if( apinfos[i].channel > 14 &&
							 apinfos[i].channel < 166 ){
					fprintf(fp, "\"5G\",");
				}else{
					fprintf(fp, "\"ERR_BNAD\",");
				}

				if (strlen(apinfos[i].SSID) == 0){
					fprintf(fp, ",");
				}else{
					memset(ssid_str, 0, sizeof(ssid_str));
					char_to_ascii(ssid_str, apinfos[i].SSID);
					fprintf(fp, "\"%s\",", ssid_str);
				}

				fprintf(fp, "\"%d\",", apinfos[i].channel);

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_)
						fprintf(fp, "\"%s\",", "WPA");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X2_)
						fprintf(fp, "\"%s\",", "WPA2");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK_)
						fprintf(fp, "\"%s\",", "WPA-PSK");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK2_)
						fprintf(fp, "\"%s\",", "WPA2-PSK");
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
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_)
						fprintf(fp, "\"%s\",", "TKIP+AES");
					else
						fprintf(fp, "\"%\"s,", "Unknown");
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
		spinlock_unlock(SPINLOCK_SiteSurvey);
	}	/* if */

	return retval;
}

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
		fprintf(stderr, "[wlc] WLC_GET_SSID error\n");
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
		fprintf(stderr, "[wlc] WLC_GET_BSSID error\n");
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
				fprintf(stderr, "[wlc] Authorized failed\n");
				goto wl_ioctl_error;
			}
		}
	}

	if(!wl_associated){
		fprintf(stderr, "[wlc] not wl_associated\n");
	}

	fprintf(stderr, "[wlc] wl-associated [%d]\n", wl_associated);
	fprintf(stderr, "[wlc] %s\n", wst.SSID);
	fprintf(stderr, "[wlc] %s\n", nvram_safe_get(wlc_nvname("ssid")));

	if (wl_associated &&
		!strncmp(wst.SSID, nvram_safe_get(wlc_nvname("ssid")), wst.SSID_len)){
		if (wl_psk){
			if (wl_authorized){
				fprintf(stderr, "[wlc] wl_authorized\n");
				return 2;
			}else{
				fprintf(stderr, "[wlc] not wl_authorized\n");
				return 1;
			}
		}else{
			fprintf(stderr, "[wlc] wl_psk:[%d]\n", wl_psk);
			return 2;
		}
	}else{
		fprintf(stderr, "[wlc] Not associated\n");
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
	int ret;
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
	unit=0;
	/* return WLC connection status */
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		// only one client in a system
		if(is_ure(unit)) {
			fprintf(stderr, "[rc] [%s] is URE mode\n", word);
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
			fprintf(stderr, "[wlc] get_wlc_status:[%d]\n", ret);
		}else{
			fprintf(stderr, "[rc] [%s] is not URE mode\n", word);
		}
		unit++;
	}

	return ret;
}

#endif

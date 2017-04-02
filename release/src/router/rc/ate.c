#include <rc.h>
#include <shutils.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTN56UB2) || defined(RTAC54U)
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif
#endif
#ifdef RTCONFIG_QCA
#include <qca.h>
#endif
#include "ate.h"
#ifdef RTCONFIG_INTERNAL_GOBI
#include <at_cmd.h>
#endif
#ifdef RTCONFIG_QCA_PLC_UTILS
#include <plc_utils.h>
#endif


#define MULTICAST_BIT  0x0001
#define UNIQUE_OUI_BIT 0x0002

static int setAllSpecificColorLedOn(enum led_color color)
{
	int i, model = get_model();
	enum led_color rtk_switch_led_color = LED_COLOR_MAX;
	enum led_id *all_led[LED_COLOR_MAX], no_led = LED_ID_MAX, *p;
	enum led_fan_mode_id v;

	if (color < 0 || color >= LED_COLOR_MAX) {
		puts("0");
		return EINVAL;
	}

	for (i = 0; i < LED_COLOR_MAX; ++i)
		all_led[i] = &no_led;

	switch (model) {
#if defined(BRTAC828)
	case MODEL_BRTAC828:
		{
			static enum led_id white_led[] = {
				LED_POWER, LED_WAN, LED_WAN2,
				LED_USB, LED_USB3, LED_2G, LED_5G,
				LED_FAILOVER, LED_SATA,
				LED_ID_MAX
			};
			static enum led_id red_led[] = {
				LED_POWER_RED, LED_WAN_RED,
				LED_WAN2_RED,
				LED_ID_MAX
			};
			all_led[LED_COLOR_WHITE] = white_led;
			all_led[LED_COLOR_RED] = red_led;
			rtk_switch_led_color = LED_COLOR_WHITE;
		}
		break;
#endif
#if defined(RTAC82U)
	case MODEL_RTAC82U:
		{
			static enum led_id blue_led[] = {
				LED_POWER, LED_WAN, 
				LED_2G, LED_5G,
				LED_LAN1,LED_LAN2,
				LED_LAN3,LED_LAN4,
				LED_ID_MAX
			};
			static enum led_id red_led[] = {
				LED_WAN_RED,
				LED_ID_MAX
			};
			all_led[LED_COLOR_BLUE] = blue_led;
			all_led[LED_COLOR_RED] = red_led;
		}
		break;
#endif
	}

	for (i = 0; i < LED_COLOR_MAX; ++i) {
		p = all_led[i];
		v = (i == color)? LED_ON : LED_OFF;
		while (*p >= 0 && *p < LED_ID_MAX) {
			led_control(*p++, v);
		}
	}

	if (rtk_switch_led_color >= 0 && rtk_switch_led_color < LED_COLOR_MAX) {
		if (color == rtk_switch_led_color) {
			eval("rtkswitch", "100", "3");	/* Turn on GROUP0 LEDs */
		} else {
			eval("rtkswitch", "100", "2");	/* Turn off GROUP0 LEDs */
		}
	}


	puts("1");
	return 0;
}
int isValidMacAddr(const char* mac)
{
	int sec_byte;
	int i = 0, s = 0;

	if( strlen(mac)!=17 || !strcmp("00:00:00:00:00:00", mac) )
		return 0;

	while( *mac && i<12 ) {
		if( isxdigit(*mac) ) {
			if(i==1) {
				sec_byte= strtol(mac, NULL, 16);
				if((sec_byte & MULTICAST_BIT)||(sec_byte & UNIQUE_OUI_BIT))
					break;
			}
			i++;
		}
		else if( *mac==':') {
			if( i==0 || i/2-1!=s )
				break;
			++s;
		}
		++mac;
	}
	return( i==12 && s==5 );
}

int
isValidCountryCode(const char *Ccode)
{
	const char *c = Ccode;
	int i = 0;

	if(strlen(Ccode)==2) {
		while(i<2) { //0~9, A~F
			if( (*c>0x2F && *c<0x3A) || (*c>0x40 && *c<0x5B) ) {
				i++;
				c++;
			}
			else
				break;
		}
	}
	if( i == 2 )
		return 1;
	else
		return 0;
}

int
isNumber(const char *num)
{
	const char *c = num;
	int i = 0, len = 0;

	len = strlen(num);
	while(i<len) { //0~9
		if( (*c>='0' && *c<='9') ) {
			i++;
			c++;
		}
		else
			break;
	}
	if( i == len )
		return 1;
	else
		return 0;
}

int
isValidRegrev(const char *regrev) {
	char *c = (char *) regrev;
	int len, i = 0, ret=0;

	len = strlen(regrev);

	if( len==1 || len==2 || len ==3) {
		while(i<len) { //0~9
			if( (*c>0x2F && *c<0x3A) ) {
				i++;
				c++;
				ret = 1;
			}
			else {
				ret = 0;
				break;
			}
		}
	}

	return ret;
}

int
isValidChannel(int is_2G, char *channel)
{
	char *c = channel;
	int len, i = 0, ret=0;

	len = strlen(channel);

	if( (is_2G && (len==1 || len==2))
	||  (!is_2G && (len==2 || len==3)) ) {
		while(i<len) { //0~9
			if( (*c>0x2F && *c<0x3A) ) {
				i++;
				c++;
				ret = 1;
			}
			else {
				ret = 0;
				break;
			}
		}
	}

	return ret;
}

int
pincheck(const char *a)
{
	unsigned char *c = (unsigned char *) a;
	unsigned long int uiPINtemp = atoi(a);
	unsigned long int uiAccum = 0;
	int i = 0;

	for (;;) {
		if (*c>0x39 || *c<0x30)
			break;
		else
			i++;
		if (!*c++ || i == 8)
			break;
	}
	if(i == 8) {
		uiAccum += 3 * ((uiPINtemp / 10000000) % 10);
		uiAccum += 1 * ((uiPINtemp / 1000000) % 10);
		uiAccum += 3 * ((uiPINtemp / 100000) % 10);
		uiAccum += 1 * ((uiPINtemp / 10000) % 10);
		uiAccum += 3 * ((uiPINtemp / 1000) % 10);
		uiAccum += 1 * ((uiPINtemp / 100) % 10);
		uiAccum += 3 * ((uiPINtemp / 10) % 10);
		uiAccum += 1 * ((uiPINtemp / 1) % 10);
		if (0 != (uiAccum % 10)){
			return 0;
		}
		return 1;
	}
	else
		return 0;
}

int isValidSN(const char *sn)
{
	int i;
	unsigned char *c;

	if(strlen(sn) != SERIAL_NUMBER_LENGTH)
		return 0;

	c = (unsigned char *)sn;
	/* [1]year: C~Z (2012=C, 2013=D, ...) */
	if(*c<0x43 || *c>0x5A)
		return 0;
	c++;
	/* [2]month: 1~9 & ABC */
	if(!((*c>0x30 && *c<0x3A) || *c==0x41||*c==0x42||*c==0x43))
		return 0;
	c++;
	/* [3]WLAN & ADSL: I(aye) */
	if(*c!=0x49)
		return 0;
	c++;
	/* [4]Channel: AEJ0(zero) (A:11ch, E:13ch, J:14ch, 0:no ch) */
	if(*c!=0x41 && *c!=0x45 && *c!=0x4A && *c!=0x30)
		return 0;
	c++;
	/* [5]factory: 0~9 & A~Z, except I(aye) & O(oh) */
	if(!((*c>0x2F && *c<0x3A) || (*c>0x40 && *c<0x5B)) || *c==0x49 || *c==0x4F )
		return 0;
	c++;
	/* [6]model: 0~9 & A~Z */
	if(!((*c>0x2F && *c<0x3A) || (*c>0x40 && *c<0x5B)))
		return 0;
	c++;
	/* [7~12]serial: 0~9 */
	i=7;
	while(i<13) {
		if (*c<0x30 || *c>0x39)
			return 0;
		c++;
		i++;
	}

	return 1;
}

int
Get_USB_Port_Info(const char *port_x)
{
	char output_buf[16];
	char usb_pid[14];
	char usb_vid[14];

	sprintf(usb_pid, "usb_path%s_pid", port_x);
	sprintf(usb_vid, "usb_path%s_vid", port_x);

	if( strcmp(nvram_safe_get(usb_pid),"") && strcmp(nvram_safe_get(usb_vid),"") ) {
		sprintf(output_buf, "%s/%s",nvram_safe_get(usb_pid),nvram_safe_get(usb_vid));
		puts(output_buf);
	}
	else
		puts("N/A");

	return 1;
}

int
Get_USB_Port_Folder(const char *port_x)
{
	char usb_folder[19];
	sprintf(usb_folder, "usb_path%s_fs_path0", port_x);
	if( strcmp(nvram_safe_get(usb_folder),"") )
		puts(nvram_safe_get(usb_folder));
	else
		puts("N/A");

	return 1;
}

int
Get_USB_Port_DataRate(const char *port_x)
{
	char output_buf[16];
	char usb_speed[19];
	sprintf(usb_speed, "usb_path%s_speed", port_x);
	if( strcmp(nvram_safe_get(usb_speed),"") ) {
		sprintf(output_buf, "%sMbps", nvram_safe_get(usb_speed));
		puts(output_buf);
	}
	else
		puts("N/A");
	return 1;
}

int
Get_SD_Card_Info(void)
{
	char check_cmd[48];
	char sd_info_buf[128];
	int get_sd_card = 1;
	FILE *fp;

	if(!strcmp(nvram_safe_get("usb_path3_fs_path0"), "")){
		puts("0");
		return 1;
	}

	sprintf(check_cmd, "test_disk2 %s &> /var/sd_info.txt", nvram_safe_get("usb_path3_fs_path0"));
	system(check_cmd);

	if ((fp = fopen("/var/sd_info.txt", "r")) != NULL) {
		while(fgets(sd_info_buf, 128, fp)!=NULL) {
			if(strstr(sd_info_buf, "No partition")||strstr(sd_info_buf, "No disk"))
				get_sd_card=0;
		}
		if(get_sd_card)
			puts("1");
		else
			puts("0");
		fclose(fp);
		eval("rm", "-rf", "/var/sd_info.txt");
	}
	else
		puts("ATE_ERROR");

	return 1;
}

int
Get_SD_Card_Folder(void)
{
	if( strcmp(nvram_safe_get("usb_path3_fs_path0"),"") )
		puts(nvram_safe_get("usb_path3_fs_path0"));
	else
		puts("N/A");

	return 1;
}

int Ej_device(const char *dev_no)
{
	if( dev_no==NULL || *dev_no<'1' || *dev_no>'9' )
		return 0;
	else {
		eval("ejusb", (char*)dev_no);
		sleep(4);
		puts("1");
	}
	return 1;
}

#if defined(RTCONFIG_EJUSB_BTN)
#define MAX_NR_EJBTN	2
void get_usb_port_eject_button(unsigned int port)
{
	int i, ejbtn, found = 0;
	char *v, nv[sizeof("btn_ejusb1_gpioXXX")];
	char nv2[sizeof("btn_ejusb_btn1XXX")];

	if (!port || port > MAX_NR_EJBTN) {
		puts("0");
		return;
	}

	for (i = 1; i <= MAX_NR_EJBTN; ++i) {
		snprintf(nv, sizeof(nv), "btn_ejusb%d_gpio", i);
		v = nvram_get(nv);
		if (!v)
			continue;
		ejbtn = (atoi(v) & GPIO_EJUSB_MASK) >> GPIO_EJUSB_SHIFT;
		if (ejbtn && ejbtn != (1 << (port - 1)))
			continue;

		found = 1;
		snprintf(nv2, sizeof(nv2), "btn_ejusb_btn%d", i);
		puts(nvram_safe_get(nv2));
		break;
	}

	if (!found)
		puts("0");
}
#endif

int asus_ate_command(const char *command, const char *value, const char *value2)
{
	//_dprintf("===[ATE %s %s]===\n", command, value);
#ifdef RTCONFIG_QTN
	if(!nvram_match("qtn_ready", "1")){
		_dprintf("ATE Error: wireless 5G not ready\n");
		return 0;
	}
#endif
	/*** ATE Set function ***/
	if(!strcmp(command, "Set_StartATEMode")) {
		nvram_set("asus_mfg", "1");
		if(nvram_match("asus_mfg", "1")) {
			puts("1");
			stop_services_mfg();
		}
		else
			puts("ATE_ERROR");
		return 0;
	}
	else if (!strcmp(command, "Set_AllLedOn")) {
#ifdef RTCONFIG_QCA_PLC_UTILS
		set_plc_all_led_onoff(1);
#endif
		return setAllLedOn();
	}
	else if (!strcmp(command, "Set_AllLedOn2")) {
		return setAllLedOn2();
	}
	else if (!strcmp(command, "Set_AllLedOff")) {
#ifdef RTCONFIG_QCA_PLC_UTILS
		set_plc_all_led_onoff(0);
#endif
		return setAllLedOff();
	}
	else if (!strcmp(command, "Set_AllLedOn_Half")) {
		puts("ATE_ERROR"); //Need to implement for EA-N66U
		return EINVAL;
	}
	else if (!strcmp(command, "Set_AllWhiteLedOn")) {
		return setAllSpecificColorLedOn(LED_COLOR_WHITE);
	}
	else if (!strcmp(command, "Set_AllBlueLedOn")) {
		return setAllSpecificColorLedOn(LED_COLOR_BLUE);
	}
	else if (!strcmp(command, "Set_AllRedLedOn")) {
		return setAllSpecificColorLedOn(LED_COLOR_RED);
	}
#ifdef RTCONFIG_BCMARM
	else if (!strcmp(command, "Set_WanLedMode1")) {
		return setWanLedMode1();
	}
	else if (!strcmp(command, "Set_WanLedMode2")) {
		return setWanLedMode2();
	}
	else if (!strcmp(command, "Set_AteModeLedOn")) {
		return setATEModeLedOn();
	}
#endif
	else if (!strcmp(command, "Set_MacAddr_2G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif
		//Andy Chiu, 2016/02/04.
		const char *p = (char *) value;
		char UpperMac[20] = {0};
		int i;
		for(i = 0; p[i]; ++i)
		{
			UpperMac[i] = toupper(p[i]);
		}
#if 0
		char tmp[256];
		snprintf(tmp, sizeof(tmp), "<%s;%s>", value, UpperMac);
		puts(tmp);
#endif
		if( !setMAC_2G(UpperMac) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Set_MacAddr_5G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif
		//Andy Chiu, 2016/02/04.
		const char *p = (char *) value;
		char UpperMac[20] = {0};
		int i;
		for(i = 0; p[i]; ++i)
		{
			UpperMac[i] = toupper(p[i]);
		}

#ifdef RTCONFIG_QTN
		if( !setMAC_5G_qtn(UpperMac))
#else
		if( !setMAC_5G(UpperMac))
#endif
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#if defined(RTAC3200) || defined(RTAC5300)|| defined(RTAC5300R)
	else if (!strcmp(command, "Set_MacAddr_5G_2")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;

#endif
		//Andy Chiu, 2016/02/04.
		char *p = (char *) value;
		char UpperMac[20] = {0};
		int i;
		for(i = 0; p[i]; ++i)
		{
			UpperMac[i] = toupper(p[i]);
		}

		if( !setMAC_5G_2(UpperMac))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#endif
#endif	/* RTCONFIG_HAS_5G */
#if defined(RTN14U)
	else if (!strcmp(command, "eeprom")) {
		if ( !eeprom_upgrade(value, 1))
			return EINVAL;
		return 0;
	}
	else if (!strcmp(command, "eeover")) {
		if ( !eeprom_upgrade(value, 0))
			return EINVAL;
		return 0;
	}
#endif
#if defined(RTCONFIG_NEW_REGULATION_DOMAIN)
	else if (!strcmp(command, "Set_RegSpec")) {
		if (setRegSpec(value, 1) < 0)
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		getRegSpec();
		return 0;
	}
	else if (!strcmp(command, "Set_RegulationDomain_2G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif
		if (setRegDomain_2G(value) == -1) {
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		getRegDomain_2G();
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Set_RegulationDomain_5G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif
		if (setRegDomain_5G(value) == -1) {
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		getRegDomain_5G();
		return 0;
	}
#endif	/* RTCONFIG_HAS_5G */
#else	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
	else if (!strcmp(command, "Set_RegulationDomain_2G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif
		if ( !setCountryCode_2G(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
#ifdef RTCONFIG_QCA
		if ((value2==NULL) || strcmp(value2,"noctl"))
			setCTL(value);
#endif
		return 0;
	}
#endif /* RTCONFIG_NEW_REGULATION_DOMAIN */
#ifdef CONFIG_BCMWL5
	else if (!strcmp(command, "Set_RegulationDomain_5G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

#ifdef RTCONFIG_QTN
		if ( !setCountryCode_5G_qtn(value))
#else
		if ( !setCountryCode_5G(value))
#endif
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_Regrev_2G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

		if( !setRegrev_2G(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_Regrev_5G")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

#ifdef RTCONFIG_QTN
		if( !setRegrev_5G_qtn(value))
#else
		if( !setRegrev_5G(value))
#endif
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_Commit")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif
		setCommit();
		return 0;
	}
#endif
#if defined(RTN14U)
	else if (!strcmp(command, "pkt_flood")) {
		if (nvram_invmatch("asus_mfg", "0"))
		{
#if 0 // TBD
			struct sockaddr_ll dev,dev2;
			int fd,fd2,do_flag=3;
			unsigned char buffer[1514];
			fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
			dev.sll_family = AF_PACKET;
			dev.sll_protocol = htons(ETH_P_ALL);
			dev.sll_ifindex = 4; // LAN
			bind( fd, (struct sockaddr *) &dev, sizeof(dev));

			fd2 = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
			dev2.sll_family = AF_PACKET;
			dev2.sll_protocol = htons(ETH_P_ALL);
			dev2.sll_ifindex = 5; // WAN
			bind( fd2, (struct sockaddr *) &dev2, sizeof(dev2));

			if (value) {
				if(strcmp(value,"WAN")==0)
					do_flag = 2;
				else if(strcmp(value,"LAN")==0)
					do_flag = 1;
			}
			memset(buffer,0xff,6);
			FRead(buffer+6, OFFSET_MAC_ADDR_2G, 6);
			memset(buffer+12,0x55,1502);
			while(1)
			{
				if (do_flag & 1)
					send( fd, buffer, 1514, 0);
				if (do_flag & 2)
					send( fd2, buffer, 1514, 0);
			}
#endif
		}
		return 0;
 	}
#endif
	else if (!strcmp(command, "Set_SerialNumber")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

		if(!setSN(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
 	}
#ifdef RTCONFIG_ODMPID
	else if (!strcmp(command, "Set_ModelName")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

		if(!setMN(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#endif
	else if (!strcmp(command, "Set_PINCode")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

		if (!setPIN(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_40M_Channel_2G")) {
		if(!set40M_Channel_2G((char*)value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Set_40M_Channel_5G")) {
		if(!set40M_Channel_5G((char*)value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#endif	/* RTCONFIG_HAS_5G */
	else if (!strcmp(command, "Set_RestoreDefault")) {
#ifdef RTAC87U
		int ret_reset;
#endif
		nvram_set("restore_defaults", "1");
		nvram_set(ASUS_STOP_COMMIT, "1");
#ifdef RTAC87U
		ret_reset = ResetDefault();
		if(ret_reset == 0){
			logmessage("ATE", "Set_RestoreDefault OK");
			sleep(3);
			puts("1");
		}else{
			logmessage("ATE", "Set_RestoreDefault failed");
			sleep(3);
			puts("0");
		}
#else
		ResetDefault();
#endif
		return 0;
	}
	else if (!strcmp(command, "Set_Eject")) {
		if( !Ej_device(value)) {
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#ifdef RTCONFIG_FANCTRL
	else if (!strcmp(command, "Set_FanOn")) {
		setFanOn();
		return 0;
	}
	else if (!strcmp(command, "Set_FanOff")) {
		setFanOff();
		return 0;
	}
#endif
#ifdef CONFIG_BCMWL5
	else if (!strcmp(command, "Set_WaitTime")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

		if( !setWaitTime(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_WiFi_2G")) {
		if( !setWiFi2G(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Set_WiFi_5G")) {
		if( !setWiFi5G(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#endif	/* RTCONFIG_HAS_5G */
#endif
#ifdef RTCONFIG_RALINK
	else if (!strcmp(command, "Set_DevFlags")) {
		if( Set_Device_Flags(value) < 0 )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_WanToLan")) {
	   	set_wantolan();
		modprobe_r("hw_nat");
		modprobe("hw_nat");
		stop_wanduck();
		stop_udhcpc(-1);
		return 0;
	}
#if defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTN56UB2)
	else if (!strcmp(command, "Set_FixChannel")) {
		FWrite("1", OFFSET_FIX_CHANNEL, 1);
		puts("1");
		return 0;
	}
	else if (!strcmp(command, "Set_FreeChannel")) {
		FWrite("0", OFFSET_FIX_CHANNEL, 1);
		nvram_set("wl0_channel","0");
		nvram_set("wl1_channel","0");
		nvram_set("lan_stp","1");
		nvram_commit();
		puts("1");
		return 0;
	}
#endif
#endif
	else if (!strcmp(command, "Set_XSetting")) {
		if(value == NULL || strcmp(value, "1")) {
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		else {
			nvram_set("x_Setting", "1");
			puts(nvram_get("x_Setting"));
		}
		return 0;
	}
	/*** ATE Get functions ***/
	else if (!strcmp(command, "Get_FWVersion")) {
		char fwver[12];
		sprintf(fwver, "%s.%s", nvram_safe_get("firmver"), nvram_safe_get("buildno"));
		puts(fwver);
		return 0;
	}
	else if (!strcmp(command, "Get_BootLoaderVersion")) {
		getBootVer();
		return 0;
	}
	else if (!strcmp(command, "Get_ResetButtonStatus")) {
		puts(nvram_safe_get("btn_rst"));
		return 0;
	}
	else if (!strcmp(command, "Get_WpsButtonStatus")) {
		puts(nvram_safe_get("btn_ez"));
		return 0;
	}
#if defined(RTCONFIG_EJUSB_BTN)
	else if (!strcmp(command, "Get_EjectUsbButton1Status")) {
		puts(nvram_safe_get("btn_ejusb_btn1"));
		return 0;
	}
	else if (!strcmp(command, "Get_EjectUsbButton2Status")) {
		puts(nvram_safe_get("btn_ejusb_btn2"));
		return 0;
	}
	else if (!strcmp(command, "Get_UsbPort1EjectButtonStatus")) {
		get_usb_port_eject_button(1);
		return 0;
	}
	else if (!strcmp(command, "Get_UsbPort2EjectButtonStatus")) {
		get_usb_port_eject_button(2);
		return 0;
	}
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
	else if (!strcmp(command, "Get_WirelessButtonStatus")) {
		puts(nvram_safe_get("btn_wifi_toggle"));
		return 0;
	}
#endif
	else if (!strcmp(command, "Get_SSID_2G")) {
		puts(nvram_safe_get("wl0_ssid"));
		return 0;
	}
	else if (!strcmp(command, "Get_SSID_5G")) {
		puts(nvram_safe_get("wl1_ssid"));
		return 0;
	}
#ifdef RTCONFIG_SWMODE_SWITCH
#if defined(PLAC66U)
	else if (!strcmp(command, "Get_SwitchStatus")) {
		puts(nvram_safe_get("switch_mode"));
		return 0;
	}
#endif  /* Model */
#endif  /* RTCONFIG_SWMODE_SWITCH */
	else if (!strcmp(command, "Get_SWMode")) {
		puts(nvram_safe_get("sw_mode"));
		return 0;
	}
	else if (!strcmp(command, "Get_MacAddr_2G")) {
		getMAC_2G();
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Get_MacAddr_5G")) {
#ifdef RTCONFIG_QTN
		getMAC_5G_qtn();
#else
		getMAC_5G();
#endif
		return 0;
	}
#if defined(RTAC3200) || defined(RTAC5300)|| defined(RTAC5300R)
	else if (!strcmp(command, "Get_MacAddr_5G_2")) {
		getMAC_5G_2();
		return 0;
	}
#endif
#endif	/* RTCONFIG_HAS_5G */
	else if (!strcmp(command, "Get_Usb2p0_Port1_Infor") || !strcmp(command, "Get_Usb_Port1_Infor")) {
		Get_USB_Port_Info("1");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb2p0_Port1_Folder") || !strcmp(command, "Get_Usb_Port1_Folder")) {
		Get_USB_Port_Folder("1");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb2p0_Port2_Infor")) {
		Get_USB_Port_Info("2");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb2p0_Port2_Folder")) {
		Get_USB_Port_Folder("2");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb_Port1_DataRate")) {
		if (!Get_USB_Port_DataRate("1"))
			puts("ATE_ERROR");
		return 0;
	}
#if defined(RTCONFIG_M2_SSD)
	/* Because M.2 SSD is assigned to port 3 and BRT-AC828 doesn't have SD card.
	 * It's safe to call functions for SD card here.
	 */
	else if (!strcmp(command, "Get_M2Ssd_Infor")) {
		Get_SD_Card_Info();
		return 0;
	}
	else if (!strcmp(command, "Get_M2Ssd_Folder")) {
		Get_SD_Card_Folder();
		return 0;
	}
	else if (!strcmp(command, "Get_M2Ssd_DataRate")) {
		if (!Get_USB_Port_DataRate("3"))
			puts("ATE_ERROR");
		return 0;
	}
#endif
	else if (!strcmp(command, "Get_SD_Infor")) {
		Get_SD_Card_Info();
		return 0;
	}
	else if (!strcmp(command, "Get_SD_Folder")) {
		Get_SD_Card_Folder();
		return 0;
	}
#if defined(RTCONFIG_NEW_REGULATION_DOMAIN)
	else if (!strcmp(command, "Get_RegSpec")) {
		getRegSpec();
		return 0;
	}
	else if (!strcmp(command, "Get_RegulationDomain_2G")) {
		getRegDomain_2G();
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Get_RegulationDomain_5G")) {
		getRegDomain_5G();
		return 0;
	}
#endif	/* RTCONFIG_HAS_5G */
#else	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
	else if (!strcmp(command, "Get_RegulationDomain_2G")) {
		getCountryCode_2G();
		return 0;
	}
#endif	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
#ifdef CONFIG_BCMWL5
	else if (!strcmp(command, "Get_RegulationDomain_5G")) {
#ifdef RTCONFIG_QTN
		getCountryCode_5G_qtn();
#else
	   	getCountryCode_5G();
#endif
		return 0;
	}
	else if (!strcmp(command, "Get_Regrev_2G")) {
		getRegrev_2G();
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Get_Regrev_5G")) {
#ifdef RTCONFIG_QTN
		getRegrev_5G_qtn();
#else
		getRegrev_5G();
#endif
		return 0;
	}
#endif	/* RTCONFIG_HAS_5G */
#endif
	else if (!strcmp(command, "Get_SerialNumber")) {
		getSN();
		return 0;
	}
#ifdef RTCONFIG_ODMPID
	else if (!strcmp(command, "Get_ModelName")) {
		getMN();
		return 0;
	}
#endif
	else if (!strcmp(command, "Get_PINCode")) {
		getPIN();
		return 0;
	}
	else if (!strcmp(command, "Get_WanLanStatus")) {
#if defined(RTCONFIG_EXT_RTL8365MB) || defined(RTCONFIG_EXT_RTL8370MB)
		GetPhyStatus(1);
#else
		if( !GetPhyStatus(1))
			puts("ATE_ERROR");
#endif

		return 0;
	}
	else if (!strcmp(command, "Get_FwReadyStatus")) {
		puts(nvram_safe_get("success_start_service"));
		return 0;
	}
	else if (!strcmp(command, "Get_Build_Info")) {
		puts(nvram_safe_get("buildinfo"));
		return 0;
	}
#ifdef RTCONFIG_RALINK
	else if (!strcmp(command, "Get_RSSI_2G")) {
		getrssi(0);
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Get_RSSI_5G")) {
		getrssi(1);
		return 0;
	}
#endif	/* RTCONFIG_HAS_5G */
#endif
	else if (!strcmp(command, "Get_ChannelList_2G")) {
		if(!Get_ChannelList_2G())
			puts("ATE_ERROR");
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Get_ChannelList_5G")) {
#ifdef RTCONFIG_QTN
		if (!Get_ChannelList_5G_qtn())
#else
		if (!Get_ChannelList_5G())
#endif
			puts("ATE_ERROR");
		return 0;
	}
#if defined(RTAC3200) || defined(RTAC5300)|| defined(RTAC5300R)
	else if (!strcmp(command, "Get_ChannelList_5G_2")) {
		if (!Get_ChannelList_5G_2())
			puts("ATE_ERROR");
		return 0;
	}
#endif
#endif	/* RTCONFIG_HAS_5G */
#if defined(RTCONFIG_USB_XHCI)
	else if (!strcmp(command, "Get_Usb3p0_Port1_Infor")) {
		if (!Get_USB3_Port_Info("1"))
			puts("ATE_ERROR");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb3p0_Port2_Infor")) {
		if (!Get_USB3_Port_Info("2"))
			puts("ATE_ERROR");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb3p0_Port3_Infor")) {
		puts("ATE_ERROR"); //Need to implement
		return 0;
	}
	else if (!strcmp(command, "Get_Usb3p0_Port1_Folder")) {
		if (!Get_USB3_Port_Folder("1"))
			puts("ATE_ERROR");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb3p0_Port2_Folder")) {
		if (!Get_USB3_Port_Folder("2"))
			puts("ATE_ERROR");
		return 0;
	}
 	else if (!strcmp(command, "Get_Usb3p0_Port3_Folder")) {
		puts("ATE_ERROR"); //Need to implement
		return EINVAL;
	}
	else if (!strcmp(command, "Get_Usb3p0_Port1_DataRate")) {
		if (!Get_USB3_Port_DataRate("1"))
			puts("ATE_ERROR");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb3p0_Port2_DataRate")) {
		if (!Get_USB3_Port_DataRate("2"))
			puts("ATE_ERROR");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb3p0_Port3_DataRate")) {
		puts("ATE_ERROR"); //Need to implement
		return EINVAL;
	}
#endif	/* RTCONFIG_USB_XHCI */
	else if (!strcmp(command, "Get_fail_ret")) {
		Get_fail_ret();
		return 0;
	}
	else if (!strcmp(command, "Get_fail_reboot_log")) {
		Get_fail_reboot_log();
		return 0;
	}
	else if (!strcmp(command, "Get_fail_dev_log")) {
		Get_fail_dev_log();
		return 0;
	}
#ifdef RTCONFIG_RALINK
#if !defined(RTN14U) && !defined(RTAC52U) && !defined(RTAC51U) && !defined(RTN11P) && !defined(RTN300) && !defined(RTN54U) && !defined(RTAC1200HP) && !defined(RTN56UB1) && !defined(RTAC54U) && !defined(RTN56UB2)
	else if (!strcmp(command, "Ra_FWRITE")) {
		return FWRITE(value, value2);
	}
	else if (!strcmp(command, "Ra_Asuscfe_2G")) {
		return asuscfe(value, WIF_2G);
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Ra_Asuscfe_5G")) {
		return asuscfe(value, WIF_5G);
	}
#endif	/* RTCONFIG_HAS_5G */
	else if (!strcmp(command, "Set_SwitchPort_LEDs")) {
		if(Set_SwitchPort_LEDs(value, value2) < 0)
		{
			puts("ATE_ERROR");
			return EINVAL;
		}
		return 0;
	}
#endif
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
	else if (!strcmp(command, "Get_WifiSwStatus")) {
		puts(nvram_safe_get("btn_wifi_sw"));
		return 0;
	}
#endif

#ifdef RTCONFIG_WIFI_TOG_BTN
	else if (!strcmp(command, "Get_WifiButtonStatus")) {
		puts(nvram_safe_get("btn_wifi_toggle"));
		return 0;
	}
#endif
#ifdef RTCONFIG_LED_BTN
	else if (!strcmp(command, "Get_LedButtonStatus")) {
		puts(nvram_safe_get("btn_led"));
		return 0;
	}
#endif
#ifdef RTCONFIG_QTN
	else if (!strcmp(command, "Enable_Qtn_TelnetSrv")) {
		enable_qtn_telnetsrv(1);
		puts("1");
		return 0;
	}
	else if (!strcmp(command, "Disable_Qtn_TelnetSrv")) {
		enable_qtn_telnetsrv(0);
		puts("1");
		return 0;
	}
	else if (!strcmp(command, "Get_Qtn_TelnetSrv_Status")) {
		getstatus_qtn_telnetsrv();
		return 0;
	}
	else if (!strcmp(command, "Del_Qtn_Cal_Files")) {
		del_qtn_cal_files();
		puts("1");
		return 0;
	}
#endif
	else if (!strcmp(command, "Get_WiFiStatus_2G")) {
		if(get_radio(0, 0))
			puts("1");
		else
			puts("0");
		return 0;
	}
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Get_WiFiStatus_5G")) {
		if(get_radio(1, 0))
			puts("1");
		else
			puts("0");
		return 0;
	}
#endif
	else if (!strcmp(command, "Set_WiFiStatus_2G")) {
		int act = !strcmp(value, "on");

		if(!strcmp(value, "on") && !strcmp(value, "off"))
			puts("ATE_UNSUPPORT");

		set_radio(act, 0, 0);

		if(get_radio(0, 0)){
			if(act)
				puts("success=on");
			else
				puts("ATE_ERROR_INCORRECT_PARAMETER");
		} else{
			if(!act)
				puts("success=off");
			else
				puts("ATE_ERROR_INCORRECT_PARAMETER");
		}
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Set_WiFiStatus_5G")) {
		int act = !strcmp(value, "on");

		if(!strcmp(value, "on") && !strcmp(value, "off"))
			puts("ATE_UNSUPPORT");

		set_radio(act, 1, 0);

		if(get_radio(1, 0)){
			if(act)
				puts("success=on");
			else
				puts("ATE_ERROR_INCORRECT_PARAMETER");
		} else{
			if(!act)
				puts("success=off");
			else
				puts("ATE_ERROR_INCORRECT_PARAMETER");
		}
		return 0;
	}
#endif	/* RTCONFIG_HAS_5G */
	else if (!strcmp(command, "Get_ATEVersion")) {
		puts(nvram_safe_get("Ate_version"));
		return 0;
	}
	else if (!strcmp(command, "Get_XSetting")) {
		puts(nvram_safe_get("x_Setting"));
		return 0;
	}
	else if (!strcmp(command, "Get_WaitTime")) {
		puts(nvram_safe_get("wait_time"));
		return 0;
	}
	else if (!strcmp(command, "Get_ExtendNo")) {
		puts(nvram_safe_get("extendno"));
		return 0;
	}
#ifdef RTCONFIG_RALINK
	else if (!strcmp(command, "Get_DevFlags")) {
		if( Get_Device_Flags() < 0)
		{
			puts("ATE_ERROR");
			return EINVAL;
		}
		return 0;
	}
#endif
#ifdef RTCONFIG_QCA
#if defined(RTCONFIG_WIFI_QCA9557_QCA9882)
#if 0
	else if (!strcmp(command, "Set_ART2")) {
		Set_ART2();
		return 0;
	}
#endif
	else if (!strncmp(command, "Get_EEPROM_", 11)) {
		Get_EEPROM_X(command);
		return 0;
	}
	else if (!strcmp(command, "Get_CalCompare")) {
		Get_CalCompare();
		return 0;
	}
#elif defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994) || defined(RTCONFIG_SOC_IPQ40XX)
	else if (!strcmp(command, "Set_Qcmbr")) {
		Set_Qcmbr(value);
		return 0;
	}
#endif
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994) || defined(RTCONFIG_SOC_IPQ40XX)
	/* ATE Get_BData_2G / ATE Get_BData_5G
	 * To prevent whole ATE command strings exposed in rc binary,
	 * compare these commands in 3 steps instead.
	 */
	else if (!strncmp(command, "Get_", 4) && !strncmp(command + 4, "BData", 5) &&
		 *(command + 9) == '_') {
		Get_BData_X(command);
		return 0;
	}
#endif
#ifdef RTCONFIG_ATEUSB3_FORCE
	else if (!strcmp(command, "Set_ForceUSB3")) {
		if (setForceU3(value) < 0)
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		getForceU3();
		return 0;
	}
	else if (!strcmp(command, "Get_ForceUSB3")) {
		getForceU3();
		return 0;
	}
#endif
#endif	/* RTCONFIG_QCA */
#ifdef RTCONFIG_INTERNAL_GOBI
	else if(!strcmp(command, "Get_LteButtonStatus")) {
		puts(nvram_safe_get("btn_lte"));
		return 0;
	}
	else if(!strcmp(command, "Get_GobiSimCard")) {
		char line[128];
		if (!Gobi_SimCardReady(Gobi_SimCard(line, sizeof(line))))
		{
			puts("FAIL");
			return EINVAL;
		}
		puts("PASS");
	}
	else if(!strcmp(command, "Get_GobiIMEI")) {
		char line[128];
		const char *IMEI = Gobi_IMEI(line, sizeof(line));
		if (IMEI == NULL)
		{
			puts("FAIL");
			return EINVAL;
		}
		puts(IMEI);
	}
	else if(!strcmp(command, "Get_GobiConnectISP")) {
		char line[128];
		const char *ISP = Gobi_ConnectISP(line, sizeof(line));
		if (ISP == NULL)
		{
			puts("FAIL");
			return EINVAL;
		}
		puts(ISP);
	}
	else if(!strcmp(command, "Get_GobiConnectStatus")) {
		const char *status = Gobi_ConnectStatus_Str(Gobi_ConnectStatus_Int());
		if (status == NULL)
		{
			puts("FAIL");
			return EINVAL;
		}
		puts(status);
	}
	else if(!strcmp(command, "Get_GobiSignal_Percent")) {
		int percent = Gobi_SignalQuality_Percent(Gobi_SignalQuality_Int());
		if (percent < 0)
		{
			puts("FAIL");
			return EINVAL;
		}
		printf("%d\n", percent);
	}
	else if(!strcmp(command, "Get_GobiSignal_dbm")) {
		int dbm = Gobi_SignalLevel_Int();
		if (dbm >= 0)
		{
			puts("FAIL");
			return EINVAL;
		}
		printf("%d dBm\n", dbm);
	}
	else if(!strcmp(command, "Get_GobiVersion")) {
		char line[128];
		if (Gobi_FwVersion(line, sizeof(line)) == NULL)
		{
			puts("FAIL");
			return EINVAL;
		}
		printf("%s\n", line);
	}
	else if(!strcmp(command, "Get_GobiQcnVersion")) {
		char line[128];
		if (Gobi_QcnVersion(line, sizeof(line)) == NULL)
		{
			puts("FAIL");
			return EINVAL;
		}
		printf("%s\n", line);
	}
	else if(!strcmp(command, "Set_GobiBand")) {
		char line[128];
		if (Gobi_SelectBand(value, line, sizeof(line)) == NULL)
		{
			puts("FAIL");
			return EINVAL;
		}
		printf("FINISH\n");
	}
	else if(!strcmp(command, "Get_GobiBand")) {
		char line[128];
		char *band;
		if ((band = Gobi_BandChannel(line, sizeof(line))) == NULL)
		{
			puts("FAIL");
			return EINVAL;
		}
		cprintf("line(%p) band(%p)\n", line, band);
		printf("%s\n", band);
	}
#endif	/* RTCONFIG_INTERNAL_GOBI */
#if defined(RTCONFIG_TCODE)
	else if (!strcmp(command, "Set_TerritoryCode")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

		if (setTerritoryCode(value) < 0)
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
#ifndef CONFIG_BCMWL5
		getTerritoryCode();
#endif
		return 0;
	}
	else if (!strcmp(command, "Get_TerritoryCode")) {
		getTerritoryCode();
		return 0;
	}
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK)
	else if (!strcmp(command, "Set_PSK")) {
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
		if (!chk_envrams_proc())
			return EINVAL;
#endif

		if (setPSK(value) < 0)
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
#ifndef CONFIG_BCMWL5
		getPSK();
#endif
		return 0;
	}
	else if (!strcmp(command, "Get_PSK")) {
		getPSK();
		return 0;
	}
#endif
#endif
#ifdef RTCONFIG_QCA_PLC_UTILS
	else if(!strcmp(command, "Set_MacAddr_Plc")) {
		if (!setPLC_para(value, OFFSET_PLC_MAC))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if(!strcmp(command, "Get_MacAddr_Plc")) {
		getPLC_para(OFFSET_PLC_MAC);
		return 0;
	}
	else if(!strcmp(command, "Set_NMK_Plc")) {
		if (!setPLC_para(value, OFFSET_PLC_NMK))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if(!strcmp(command, "Get_NMK_Plc")) {
		getPLC_para(OFFSET_PLC_NMK);
		return 0;
	}
	else if(!strcmp(command, "Get_PWD_Plc")) {
		if (!getPLC_PWD()) {
			puts("ATE_ERROR");
			return EINVAL;
		}
		return 0;
	}
#endif
#ifdef RTCONFIG_DEFAULT_AP_MODE
	else if(!strcmp(command, "Set_ForceDisableDHCP")) {
		FWrite("1", OFFSET_FORCE_DISABLE_DHCP, 1);
		puts("1");
		return 0;
	}
	else if(!strcmp(command, "Set_FreeDisableDHCP")) {
		char buf[2];
		FWrite("0", OFFSET_FORCE_DISABLE_DHCP, 1);
		if (FRead(buf, OFFSET_FORCE_DISABLE_DHCP, 1) < 0)
			puts("ATE_ERROR");
		else {
			buf[1] = '\0';
			puts(buf);
		}
		return 0;
	}
#endif
#ifdef CONFIG_BCMWL5
	else if (!strcmp(command, "Get_SSID_2G")) {
		getSSID(0);
		return 0;
	}
	else if (!strcmp(command, "Get_SSID_5G")) {
		getSSID(1);
		return 0;
	}
	else if (!strcmp(command, "Get_SSID_5G_2")) {
		getSSID(2);
		return 0;
	}
#endif
	else
	{
		puts("ATE_UNSUPPORT");
		return EINVAL;
	}

	return 0;
}

int ate_dev_status(void)
{
	int ret = 1, wl_band=1;;
	char wl_dev_name[12], dev_chk_buf[19], word[256], *next;

	sprintf(wl_dev_name, nvram_safe_get("wl_ifnames"));
	if(switch_exist())
		sprintf(dev_chk_buf, "switch=O");
	else {
		sprintf(dev_chk_buf, "switch=X");
#ifdef CONFIG_BCMWL5	//broadcom platform need to shift the interface name
		sprintf(wl_dev_name, "eth0 eth1");
#endif
		ret = 0;
	}

	foreach (word, wl_dev_name, next) {
		if (wl_exist(word, wl_band)) {
			if(wl_band==1)
				sprintf(dev_chk_buf,"%s,2G=O",dev_chk_buf);
			else
				sprintf(dev_chk_buf,"%s,5G=O",dev_chk_buf);
		}
		else {
			if(wl_band==1)
				sprintf(dev_chk_buf,"%s,2G=X",dev_chk_buf);
			else
				sprintf(dev_chk_buf,"%s,5G=X",dev_chk_buf);
			ret = 0;
		}
			wl_band++;
	}
	nvram_set("Ate_dev_status",dev_chk_buf);
	return ret;
}

#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
int chk_envrams_proc(void)
{
	if (!pids("envrams"))
	{
		puts("ATE_ERROR_NOT_ALLOWED");
		return 0;
	}

	return 1;
}

int start_envrams(void){

	int ret=eval("/usr/sbin/envrams");
	return ret;
}

#endif

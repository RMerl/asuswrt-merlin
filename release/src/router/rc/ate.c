#include <rc.h>
#include <shutils.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U)
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif
#endif
#include "ate.h"

#define MULTICAST_BIT  0x0001
#define UNIQUE_OUI_BIT 0x0002

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
isValidRegrev(char *regrev) {
	char *c = regrev;
	int len, i = 0, ret=0;
	
	len = strlen(regrev);

	if( len==1 || len==2 ) {
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
	unsigned char *c = (char *) a;
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

	c = (char *)sn;
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

#if defined (RTCONFIG_USB_XHCI)
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
#endif	/* RTCONFIG_USB_XHCI */

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

int asus_ate_command(const char *command, const char *value, const char *value2)
{
	_dprintf("===[ATE %s %s]===\n", command, value);
	/*** ATE Set function ***/
	if(!strcmp(command, "Set_StartATEMode")) {
		nvram_set("asus_mfg", "1");
		if(nvram_match("asus_mfg", "1")) {
			puts("1");
#ifdef RTCONFIG_FANCTRL
			stop_phy_tempsense();
#endif
			stop_wpsaide();
			stop_wps();
#ifdef RTCONFIG_BCMWL6
			stop_acsd();
#endif
			stop_upnp();
			stop_lltd();
			stop_rstats();
			stop_wanduck();
			stop_logger();
			stop_wanduck();
#ifdef RTCONFIG_DNSMASQ
			stop_dnsmasq(0);
#else
			stop_dns();
			stop_dhcpd();
#endif
			stop_ots();
			stop_networkmap();
#ifdef RTCONFIG_USB
			stop_usbled();
#ifdef RTCONFIG_USB_PRINTER
			stop_lpd();
			stop_u2ec();
#endif
#endif
		}
		else
			puts("ATE_ERROR");
		return 0;
	}
	else if (!strcmp(command, "Set_AllLedOn")) {
		return setAllLedOn();
	}
	else if (!strcmp(command, "Set_AllLedOff")) {
		return setAllLedOff();
	}
	else if (!strcmp(command, "Set_AllLedOn_Half")) {
		puts("ATE_ERROR"); //Need to implement for EA-N66U
		return EINVAL;
	}
#ifdef RTCONFIG_BCMARM
	else if (!strcmp(command, "Set_AteModeLedOn")) {
		return setATEModeLedOn();
	}
#endif
	else if (!strcmp(command, "Set_MacAddr_2G")) {
		if( !setMAC_2G(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(command, "Set_MacAddr_5G")) {
#ifdef RTCONFIG_QTN
		if( !setMAC_5G_qtn(value))
#else
		if( !setMAC_5G(value))
#endif
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
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
		if (setRegSpec(value) < 0)
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		getRegSpec();
		return 0;
	}
	else if (!strcmp(command, "Set_RegulationDomain_2G")) {
		if (setRegDomain_2G(value) == -1) {
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		getRegDomain_2G();
		return 0;
	}
	else if (!strcmp(command, "Set_RegulationDomain_5G")) {
		if (setRegDomain_5G(value) == -1) {
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		getRegDomain_5G();
		return 0;
	}
#else	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
	else if (!strcmp(command, "Set_RegulationDomain_2G")) {
		if ( !setCountryCode_2G(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#endif /* RTCONFIG_NEW_REGULATION_DOMAIN */
#ifdef CONFIG_BCMWL5
	else if (!strcmp(command, "Set_RegulationDomain_5G")) {
		if ( !setCountryCode_5G(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_Regrev_2G")) {
		if( !setRegrev_2G(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_Regrev_5G")) {
		if( !setRegrev_5G(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_Commit")) {
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
		if(!setSN(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
 	}
#ifdef RTCONFIG_ODMPID
	else if (!strcmp(command, "Set_ModelName")) {
		if(!setMN(value))
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
#endif
	else if (!strcmp(command, "Set_PINCode")) {
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
		ResetDefault();
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
	else if (!strcmp(command, "Set_TelnetEnabled")) {
		if( !setTelnetEnable(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
	else if (!strcmp(command, "Set_WaitTime")) {
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
	else if (!strcmp(command, "Set_WiFi_5G")) {
		if( !setWiFi5G(value) )
		{
			puts("ATE_ERROR_INCORRECT_PARAMETER");
			return EINVAL;
		}
		return 0;
	}
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
#ifdef RTCONFIG_WIFI_TOG_BTN
	else if (!strcmp(command, "Get_WirelessButtonStatus")) {
		puts(nvram_safe_get("btn_wifi_toggle"));
		return 0;
	}
#endif
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
#endif	/* RTCONFIG_HAS_5G */
	else if (!strcmp(command, "Get_Usb2p0_Port1_Infor")) {
		Get_USB_Port_Info("1");
		return 0;
	}
	else if (!strcmp(command, "Get_Usb2p0_Port1_Folder")) {
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
	else if (!strcmp(command, "Get_RegulationDomain_5G")) {
		getRegDomain_5G();
		return 0;
	}
#else	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
	else if (!strcmp(command, "Get_RegulationDomain_2G")) {
		getCountryCode_2G();
		return 0;
	}
#endif	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
#ifdef CONFIG_BCMWL5
	else if (!strcmp(command, "Get_RegulationDomain_5G")) {
	   	getCountryCode_5G();
		return 0;
	}
	else if (!strcmp(command, "Get_Regrev_2G")) {
		getRegrev_2G();
		return 0;
	}
	else if (!strcmp(command, "Get_Regrev_5G")) {
		getRegrev_5G();
		return 0;
	}
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
		if( !GetPhyStatus())
			puts("ATE_ERROR");
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
		if (!Get_ChannelList_5G())
			puts("ATE_ERROR");
		return 0;
	}
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
#if !defined(RTN14U) && !defined(RTAC52U) && !defined(RTAC51U) && !defined(RTN11P)
	else if (!strcmp(command, "Ra_FWRITE")) {
		return FWRITE(value, value2);
	}
	else if (!strcmp(command, "Ra_Asuscfe_2G")) {
		return asuscfe(value, WIF_2G);
	}
	else if (!strcmp(command, "Ra_Asuscfe_5G")) {
		return asuscfe(value, WIF_5G);
	}
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
#ifdef RTCONFIG_DSL //For HW WiFi On/Off button check
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
#ifdef RTCONFIG_TURBO
	else if (!strcmp(command, "Get_Turbo")) {
		puts(nvram_safe_get("btn_turbo"));
		return 0;
	}
#endif
#ifdef RTCONFIG_LED_BTN
	else if (!strcmp(command, "Get_LedButtonStatus")) {
		puts(nvram_safe_get("btn_led"));
		return 0;
	}
#endif
#ifdef CONFIG_BCMWL5
	else if (!strcmp(command, "Get_WiFiStatus_2G")) {
		if(!getWiFiStatus("2G"))
			puts("ATE_ERROR_INCORRECT_PARAMETER");
		return 0;
	}
	else if (!strcmp(command, "Get_WiFiStatus_5G")) {
		if(!getWiFiStatus("5G"))
			puts("ATE_ERROR_INCORRECT_PARAMETER");
		return 0;
	}
#else
	else if (!strcmp(command, "Get_WiFiStatus_2G")) {
		if(get_radio(0, 0))
			puts("1");
		else
			puts("0");
		return 0;
	}
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
	else if (!strcmp(command, "Get_ATEVersion")) {
		puts(nvram_safe_get("Ate_version"));
		return 0;
	}
	else if (!strcmp(command, "Get_XSetting")) {
		puts(nvram_safe_get("x_Setting"));
		return 0;
	}
	else if (!strcmp(command, "Get_TelnetEnabled")) {
		puts(nvram_safe_get("Ate_telnet"));
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

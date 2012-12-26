#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>

#include "rc.h"
#include "dongles.h"

#include <bcmnvram.h>
#include <notify_rc.h>
#include <usb_info.h>

#define MAX_RETRY_LOCK 1

#ifdef RTCONFIG_USB_PRINTER
#define MAX_WAIT_PRINTER_MODULE 20
#define U2EC_FIFO "/var/u2ec_fifo"
#endif

#ifdef RTCONFIG_USB_MODEM
#define MODEM_SCRIPTS_DIR "/etc"

#define USB_MODESWITCH_CONF MODEM_SCRIPTS_DIR"/g3.conf"


// The dongles which need to create wcdma.cfg for switching.
int is_create_file_dongle(const char *vid, const char *pid){
	if((!strcmp(vid, "0408") && !strcmp(pid, "f000")) // Q110
			|| (!strcmp(vid, "07d1") && !strcmp(pid, "a800")) // DWM-156
			)
		return 1;

	return 0;
}

#ifdef RTCONFIG_USB_BECEEM
int is_beceem_dongle(const int mode, const char *vid, const char *pid){
	if(mode){ // modem mode.
		if(!strcmp(vid, "198f") // Yota
				|| (!strcmp(vid, "0b05") && (!strcmp(pid, "1780") || !strcmp(pid, "1781"))) // GMC
				|| (!strcmp(vid, "19d2") && (!strcmp(pid, "0172") || !strcmp(pid, "0044"))) // FreshTel or Giraffe
				)
			return 1;
	}
	else{ // zero-cd mode.
		if(!strcmp(vid, "198f") // Yota
				|| (!strcmp(vid, "0b05") && !strcmp(pid, "bccd")) // GMC
				|| (!strcmp(vid, "19d2") && (!strcmp(pid, "bccd") || !strcmp(pid, "0065"))) // FreshTel or Giraffe
				)
			return 1;
	}

	return 0;
}

int is_samsung_dongle(const int mode, const char *vid, const char *pid){
	if(mode){ // modem mode.
		if(!strcmp(vid, "04e8") && !strcmp(pid, "6761")
				)
			return 1;
	}
	else{ // zero-cd mode.
		if(!strcmp(vid, "04e8") && !strcmp(pid, "6761")
				)
			return 1;
	}

	return 0;
}

int is_gct_dongle(const int mode, const char *vid, const char *pid){
	if(mode){ // modem mode.
		if(!strcmp(vid, "1076") && !strcmp(pid, "7f00")
				)
			return 1;
	}
	else{ // zero-cd mode.
		if(!strcmp(vid, "1076") && !strcmp(pid, "7f40")
				)
			return 1;
	}

	return 0;
}
#endif

// if the MTP mode of the phone will hang the DUT, need add the VID/PID in this function.
int is_android_phone(const int mode, const char *vid, const char *pid){
	if(nvram_match("modem_android", "1"))
		return 1;
	else if(mode){ // modem mode.
		if(!strcmp(vid, "04e8") && !strcmp(pid, "6864")) // Samsung S3
			return 1;
	}
	else{ // MTP mode.
		if(!strcmp(vid, "04e8") && !strcmp(pid, "6860")) // Samsung S3
			return 1;
	}

	return 0;
}

int write_3g_conf(FILE *fp, int dno, int aut, char *vid, char *pid){
	switch(dno){
		case SN_MU_Q101:
			fprintf(fp, "DefaultVendor=  0x0408\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x0408\n");
			fprintf(fp, "TargetProduct=  0xea02\n");
			fprintf(fp, "MessageEndpoint=0x05\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_OPTION_ICON225:
			fprintf(fp, "DefaultVendor=  0x0af0\n");
			fprintf(fp, "DefaultProduct= 0x6971\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageEndpoint=0x05\n");
			fprintf(fp, "MessageContent=\"555342431223456780100000080000601000000000000000000000000000000\"\n");
			break;
		case SN_Option_GlobeSurfer_Icon:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x0af0\n");
			fprintf(fp, "TargetProduct=  0x6600\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000601000000000000000000000000000000\"\n");
			break;
		case SN_Option_GlobeSurfer_Icon72:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x0af0\n");
			fprintf(fp, "TargetProduct=  0x6901\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000601000000000000000000000000000000\"\n");
			break;
		case SN_Option_GlobeTrotter_GT_MAX36:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x0af0\n");
			fprintf(fp, "TargetProduct=  0x6600\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000601000000000000000000000000000000\"\n");
			fprintf(fp, "ResponseNeeded=1\n");
			break;
		case SN_Option_GlobeTrotter_GT_MAX72:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x0af0\n");
			fprintf(fp, "TargetProduct=  0x6701");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000601000000000000000000000000000000\"\n");
			break;
		case SN_Option_GlobeTrotter_EXPRESS72:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x0af0\n");
			fprintf(fp, "TargetProduct=  0x6701\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000601000000000000000000000000000000\"\n");
			fprintf(fp, "ResponseNeeded=1\n");
			break;
		case SN_Option_iCON210:
			fprintf(fp, "DefaultVendor=  0x1e0e\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x1e0e\n");
			fprintf(fp, "TargetProduct=  0x9000\n");
			fprintf(fp, "MessageContent=\"555342431234567800000000000006bd000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Option_GlobeTrotter_HSUPA_Modem:
			fprintf(fp, "DefaultVendor=  0x0af0\n");
			fprintf(fp, "DefaultProduct= 0x7011\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243785634120100000080000601000000000000000000000000000000\"\n");
			break;
		case SN_Option_iCON401:
			fprintf(fp, "DefaultVendor=  0x0af0\n");
			fprintf(fp, "DefaultProduct= 0x7401\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243785634120100000080000601000000000000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Vodafone_K3760:
			fprintf(fp, "DefaultVendor=  0x0af0\n");
			fprintf(fp, "DefaultProduct= 0x7501\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243785634120100000080000601000000000000000000000000000000\"\n");
			break;
		case SN_ATT_USBConnect_Quicksilver:
			fprintf(fp, "DefaultVendor=  0x0af0\n");
			fprintf(fp, "DefaultProduct= 0xd033\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243785634120100000080000601000000000000000000000000000000\"\n");
			break;
		case SN_Huawei_EC156:
                	fprintf(fp, "DefaultVendor=  0x12d1\n");
                	fprintf(fp, "DefaultProduct=0x1505\n");
                	fprintf(fp, "TargetVendor=  0x12d1\n");
                	fprintf(fp, "TargetProductList=\"140b,1506\"\n");
                	fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
                	fprintf(fp, "CheckSuccess=   20\n");
			break;
		case SN_Huawei_E169:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1001\n");
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_E220:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1003\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_E180:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1414\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_Exxx:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1c0b\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1c05\n");
			fprintf(fp, "MessageEndpoint=0x0f\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
			break;
		case SN_Huawei_E173:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x14b5\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x14a8\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
			break;
		case SN_Huawei_E630:
			fprintf(fp, "DefaultVendor=  0x1033\n");
			fprintf(fp, "DefaultProduct= 0x0035\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1003\n");
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_E270:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1446\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x14ac\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_Huawei_E1550:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1446\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1001\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_Huawei_E1612:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1446\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1406\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_Huawei_E1690:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1446\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x140c\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_Huawei_K3765:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1520\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1465\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_Huawei_K4505:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1521\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1464\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_ZTE_MF620:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0001\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000600000000000000000000000000000000\"\n");
			break;
		case SN_ZTE_MF622:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0002\n");
			fprintf(fp, "MessageContent=\"55534243f8f993882000000080000a85010101180101010101000000000000\"\n");
			break;
		case SN_ZTE_MF628:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0015\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000030000000000000000000000\"\n");
			fprintf(fp, "ResponseNeeded=1\n");
			break;
		case SN_ZTE_MF626:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0031\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ZTE_AC8710:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0xfff5\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0xffff\n");
			fprintf(fp, "MessageContent=\"5553424312345678c00000008000069f030000000000000000000000000000\"\n");
			break;
		case SN_ZTE_AC2710:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0xfff5\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0xffff\n");
			fprintf(fp, "MessageContent=\"5553424312345678c00000008000069f010000000000000000000000000000\"\n");
			break;
		case SN_ZTE6535_Z:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0052\n");
			fprintf(fp, "MessageContent=\"55534243123456782000000080000c85010101180101010101000000000000\"\n");
			break;
		case SN_ZTE_K3520_Z:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0055\n");
			fprintf(fp, "MessageContent=\"55534243123456782000000080000c85010101180101010101000000000000\"\n");
			break;
		case SN_ZTE_MF110:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0053\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0031\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"55534243876543212000000080000c85010101180101010101000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ZTE_K3565:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0063\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_ONDA_MT503HS:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0002\n");
			fprintf(fp, "MessageContent=\"55534243b0c8dc812000000080000a85010101180101010101000000000000\"\n");
			break;
		case SN_ONDA_MT505UP:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0002\n");
			fprintf(fp, "MessageContent=\"55534243123456780000010080000a28000000001c00002000000000000000\"\n");
			break;
		case SN_Novatel_Wireless_Ovation_MC950D:
			fprintf(fp, "DefaultVendor=  0x1410\n");
			fprintf(fp, "DefaultProduct= 0x5010\n");
			fprintf(fp, "TargetVendor=   0x1410\n");
			fprintf(fp, "TargetProduct=  0x4400\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Novatel_U727:
			fprintf(fp, "DefaultVendor=  0x1410\n");
			fprintf(fp, "DefaultProduct= 0x5010\n");
			fprintf(fp, "TargetVendor=   0x1410\n");
			fprintf(fp, "TargetProduct=  0x4100\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Novatel_MC990D:
			fprintf(fp, "DefaultVendor=  0x1410\n");
			fprintf(fp, "DefaultProduct= 0x5020\n");
			fprintf(fp, "Interface=      5\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Novatel_U760:
			fprintf(fp, "DefaultVendor=  0x1410\n");
			fprintf(fp, "DefaultProduct= 0x5030\n");
			fprintf(fp, "TargetVendor=   0x1410\n");
			fprintf(fp, "TargetProduct=  0x6000\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Alcatel_X020:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0x1001\n");
			fprintf(fp, "TargetVendor=   0x1c9e\n");
			fprintf(fp, "TargetProduct=  0x6061\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000606f50402527000000000000000000000\"\n");
			break;
		case SN_Alcatel_X200:
			fprintf(fp, "DefaultVendor=  0x1bbb\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x1bbb\n");
			fprintf(fp, "TargetProduct=  0x0000\n");
			fprintf(fp, "MessageContent=\"55534243123456788000000080000606f50402527000000000000000000000\"\n");
			break;
		case SN_Alcatel_X220L:
			fprintf(fp, "DefaultVendor=  0x1bbb\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x1bbb\n");
			fprintf(fp, "TargetProduct=  0x0017\n");
			fprintf(fp, "MessageContent=\"55534243123456788000000080000606f50402527000000000000000000000\"\n");
			break;
		case SN_AnyDATA_ADU_500A:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x16d5\n");
			fprintf(fp, "TargetProduct=  0x6502\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_BandLuxe_C120:
			fprintf(fp, "DefaultVendor=  0x1a8d\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x1a8d\n");
			fprintf(fp, "TargetProduct=  0x1002\n");
			fprintf(fp, "MessageContent=\"55534243123456781200000080000603000000020000000000000000000000\"\n");
			fprintf(fp, "ResponseNeeded=1\n");
			break;
		case SN_Solomon_S3Gm660:
			fprintf(fp, "DefaultVendor=  0x1dd6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x1dd6\n");
			fprintf(fp, "TargetProduct=  0x1002\n");
			fprintf(fp, "MessageContent=\"55534243123456781200000080000603000000020000000000000000000000\"\n");
			fprintf(fp, "ResponseNeeded=1\n");
			break;
		case SN_C_motechD50:
			fprintf(fp, "DefaultVendor=  0x16d8\n");
			fprintf(fp, "DefaultProduct= 0x6803\n");
			fprintf(fp, "TargetVendor=   0x16d8\n");
			fprintf(fp, "TargetProduct=  0x680a\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800008ff524445564348470000000000000000\"\n");
			break;
		case SN_C_motech_CGU628:
			fprintf(fp, "DefaultVendor=  0x16d8\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x16d8\n");
			fprintf(fp, "TargetProduct=  0x6006\n");
			fprintf(fp, "MessageContent=\"55534243d85dd88524000000800008ff524445564348470000000000000000\"\n");
			break;
		case SN_Toshiba_G450:
			fprintf(fp, "DefaultVendor=  0x0930\n");
			fprintf(fp, "DefaultProduct= 0x0d46\n");
			fprintf(fp, "TargetVendor=   0x0930\n");
			fprintf(fp, "TargetProduct=  0x0d45\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_UTStarcom_UM175:
			fprintf(fp, "DefaultVendor=  0x106c\n");
			fprintf(fp, "DefaultProduct= 0x3b03\n");
			fprintf(fp, "TargetVendor=   0x106c\n");
			fprintf(fp, "TargetProduct=  0x3715\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800008ff024445564348470000000000000000\"\n");
			break;
		case SN_Hummer_DTM5731:
			fprintf(fp, "DefaultVendor=  0x1ab7\n");
			fprintf(fp, "DefaultProduct= 0x5700\n");
			fprintf(fp, "TargetVendor=   0x1ab7\n");
			fprintf(fp, "TargetProduct=  0x5731\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_A_Link_3GU:
			fprintf(fp, "DefaultVendor=  0x1e0e\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x1e0e\n");
			fprintf(fp, "TargetProduct=  0x9200\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_Sierra_Wireless_Compass597:
			fprintf(fp, "DefaultVendor=  0x1199\n");
			fprintf(fp, "DefaultProduct= 0x0fff\n");
			fprintf(fp, "TargetVendor=   0x1199\n");
			fprintf(fp, "TargetProduct=  0x0023\n");
			fprintf(fp, "SierraMode=1\n");
			break;
		case SN_Sierra881U:
			fprintf(fp, "DefaultVendor=  0x1199\n");
			fprintf(fp, "DefaultProduct= 0x0fff\n");
			fprintf(fp, "TargetVendor=   0x1199\n");
			fprintf(fp, "TargetProduct=  0x6856\n");
			fprintf(fp, "SierraMode=1\n");
			break;
		case SN_Sony_Ericsson_MD400:
			fprintf(fp, "DefaultVendor=  0x0fce\n");
			fprintf(fp, "DefaultProduct= 0xd0e1\n");
			fprintf(fp, "TargetClass=    0x02\n");
			fprintf(fp, "SonyMode=1\n");
			fprintf(fp, "Configuration=2\n");
			break;
		case SN_LG_LDU_1900D:
			fprintf(fp, "DefaultVendor=  0x1004\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000aff554d53434847000000000000000000\"\n");
			break;
		case SN_Samsung_SGH_Z810:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x04e8\n");
			fprintf(fp, "TargetProduct=  0x6601\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000616000000000000000000000000000000\"\n");
			break;
		case SN_MobiData_MBD_200HU:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x1c9e\n");
			fprintf(fp, "TargetProduct=  0x9000\n");
			fprintf(fp, "MessageContent=\"55534243123456788000000080000606f50402527000000000000000000000\"\n");
			break;
		case SN_BSNL_310G:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x1c9e\n");
			fprintf(fp, "TargetProduct=  0x9605\n");
			fprintf(fp, "MessageContent=\"55534243123456788000000080000606f50402527000000000000000000000\"\n");
			break;
		case SN_BSNL_LW272:
			fprintf(fp, "DefaultVendor=  0x230d\n");
			fprintf(fp, "DefaultProduct= 0x0001\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "Configuration=  3\n");
			break;
		case SN_ST_Mobile:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x1c9e\n");
			fprintf(fp, "TargetProduct=  0x9063\n");
			fprintf(fp, "MessageContent=\"55534243123456788000000080000606f50402527000000000000000000000\"\n");
			break;
		case SN_MyWave_SW006:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0x9200\n");
			fprintf(fp, "TargetVendor=   0x1c9e\n");
			fprintf(fp, "TargetProduct=  0x9202\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000606f50402527000000000000000000000\"\n");
			break;
		case SN_Cricket_A600:
			fprintf(fp, "DefaultVendor=  0x1f28\n");
			fprintf(fp, "DefaultProduct= 0x0021\n");
			fprintf(fp, "TargetVendor=   0x1f28\n");
			fprintf(fp, "TargetProduct=  0x0020\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800108df200000000000000000000000000000\"\n");
			break;
		case SN_EpiValley_SEC7089:
			fprintf(fp, "DefaultVendor=  0x1b7d\n");
			fprintf(fp, "DefaultProduct= 0x0700\n");
			fprintf(fp, "TargetVendor=   0x1b7d\n");
			fprintf(fp, "TargetProduct=  0x0001\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800008FF05B112AEE102000000000000000000\"\n");
			break;
		case SN_Samsung_U209:
			fprintf(fp, "DefaultVendor=  0x04e8\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x04e8\n");
			fprintf(fp, "TargetProduct=  0x6601\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000616000000000000000000000000000000\"\n");
			break;
		case SN_D_Link_DWM162_U5:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x2001\n");
			fprintf(fp, "TargetVendor=   0x1e0e\n");
			fprintf(fp, "TargetProductList=\"ce16,cefe\"\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Novatel_MC760:
			fprintf(fp, "DefaultVendor=  0x1410\n");
			fprintf(fp, "DefaultProduct= 0x5031\n");
			fprintf(fp, "TargetVendor=   0x1410\n");
			fprintf(fp, "TargetProduct=  0x6002\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Philips_TalkTalk:
			fprintf(fp, "DefaultVendor=  0x0471\n");
			fprintf(fp, "DefaultProduct= 0x1237\n");
			fprintf(fp, "TargetVendor=   0x0471\n");
			fprintf(fp, "TargetProduct=  0x1234\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000030000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_HuaXing_E600:
			fprintf(fp, "DefaultVendor=  0x0471\n");
			fprintf(fp, "DefaultProduct= 0x1237\n");
			fprintf(fp, "TargetVendor=   0x0471\n");
			fprintf(fp, "TargetProduct=  0x1206\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			fprintf(fp, "Configuration=2\n");
			break;
		case SN_C_motech_CHU_629S:
			fprintf(fp, "DefaultVendor=  0x16d8\n");
			fprintf(fp, "DefaultProduct= 0x700a\n");
			fprintf(fp, "TargetClass=0xff\n");
			fprintf(fp, "MessageContent=\"55534243123456782400000080000dfe524445564348473d4e444953000000\"\n");
			break;
		case SN_Sagem9520:
			fprintf(fp, "DefaultVendor=  0x1076\n");
			fprintf(fp, "DefaultProduct= 0x7f40\n");
			fprintf(fp, "TargetVendor=   0x1076\n");
			fprintf(fp, "TargetProduct=  0x7f00\n");
			fprintf(fp, "GCTMode=1\n");
			break;
		case SN_Nokia_CS10:
			fprintf(fp, "DefaultVendor=  0x0421\n");
			fprintf(fp, "DefaultProduct= 0x060c\n");
			fprintf(fp, "TargetVendor=   0x0421\n");
			fprintf(fp, "TargetProduct=  0x060e\n");
			fprintf(fp, "CheckSuccess=   20\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_Nokia_CS15:
			fprintf(fp, "DefaultVendor=  0x0421\n");
			fprintf(fp, "DefaultProduct= 0x0610\n");
			fprintf(fp, "TargetVendor=   0x0421\n");
			fprintf(fp, "TargetProduct=  0x0612\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Nokia_CS17:
			fprintf(fp, "DefaultVendor=  0x0421\n");
			fprintf(fp, "DefaultProduct= 0x0622\n");
			fprintf(fp, "TargetVendor=   0x0421\n");
			fprintf(fp, "TargetProduct=  0x0623\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_Nokia_CS18:
			fprintf(fp, "DefaultVendor=  0x0421\n");
			fprintf(fp, "DefaultProduct= 0x0627\n");
			fprintf(fp, "TargetVendor=   0x0421\n");
			fprintf(fp, "TargetProduct=  0x0612\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_Vodafone_MD950:
			fprintf(fp, "DefaultVendor=  0x0471\n");
			fprintf(fp, "DefaultProduct= 0x1210\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Siptune_LM75:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x05c6\n");
			fprintf(fp, "TargetProduct=  0x9000\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
	/* 0715 add */
		case SN_SU9800:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0x9800\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243123456788000000080000606f50402527000000000000000000000\"\n");
			break;
		case SN_OPTION_ICON_461:
			fprintf(fp, "DefaultVendor=  0x0af0\n");
			fprintf(fp, "DefaultProduct= 0x7a05\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000601000000000000000000000000000000\"\n");
			break;
		case SN_Huawei_K3771:
			fprintf(fp, "DefaultVendor=   0x12d1\n");
			fprintf(fp, "DefaultProduct=  0x14c4\n");
			fprintf(fp, "TargetVendor=  0x12d1\n");
			fprintf(fp, "TargetProduct= 0x14ca\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
			break;
		case SN_Huawei_K3770:
			if(nvram_match("test_k3770", "1")){ // ACM mode
				fprintf(fp, "DefaultVendor=   0x12d1\n");
				fprintf(fp, "DefaultProduct=  0x14d1\n");
				fprintf(fp, "TargetVendor=  0x12d1\n");
				fprintf(fp, "TargetProduct= 0x1c05\n");
				fprintf(fp, "CheckSuccess=   20\n");
				fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			}
			else{
				fprintf(fp, "DefaultVendor=   0x12d1\n");
				fprintf(fp, "DefaultProduct=  0x14d1\n");
				fprintf(fp, "TargetVendor=  0x12d1\n");
				fprintf(fp, "TargetProduct= 0x14c9\n");
				fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
			}
			break;
		case SN_Mobile_Action:
			fprintf(fp, "DefaultVendor=  0x0df7\n");
			fprintf(fp, "DefaultProduct= 0x0800\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MobileActionMode=1\n");
			fprintf(fp, "NoDriverLoading=1\n");
			break;
		case SN_HP_P1102:
			fprintf(fp, "DefaultVendor=  0x03f0\n");
			fprintf(fp, "DefaultProduct= 0x002a\n");
			fprintf(fp, "TargetClass=    0x07\n");
			fprintf(fp, "MessageContent=\"555342431234567800000000000006d0000000000000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Visiontek_82GH:
			fprintf(fp, "DefaultVendor=  0x230d\n");
			fprintf(fp, "DefaultProduct= 0x0007\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "Configuration=  3\n");
			break;
		case SN_ZTE_MF190_var:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0149\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0124\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "MessageContent3=\"55534243123456702000000080000c85010101180101010101000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ZTE_MF192:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x1216\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x1218\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_ZTE_MF691:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x1201\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x1203\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_CHU_629S:
			fprintf(fp, "DefaultVendor=  0x16d8\n");
			fprintf(fp, "DefaultProduct= 0x700b\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243123456782400000080000dfe524445564348473d4e444953000000\"\n");
			break;
		case SN_JOA_LM_700r:
			fprintf(fp, "DefaultVendor=  0x198a\n");
			fprintf(fp, "DefaultProduct= 0x0003\n");
			fprintf(fp, "TargetVendor=   0x198a\n");
			fprintf(fp, "TargetProduct=  0x0002\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_ZTE_MF190:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x1224\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0082\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ZTE_ffe:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0xffe6\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0xffe5\n");
			fprintf(fp, "MessageContent=\"5553424330f4cf8124000000800108df200000000000000000000000000000\"\n");
			break;
		case SN_SE_MD400G:
			fprintf(fp, "DefaultVendor=  0x0fce\n");
			fprintf(fp, "DefaultProduct= 0xd103\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "SonyMode=       1\n");
			break;
		case SN_DLINK_DWM_156:
			fprintf(fp, "DefaultVendor=  0x07d1\n");
			fprintf(fp, "DefaultProduct= 0xa804\n");
			fprintf(fp, "TargetVendor=   0x07d1\n");
			fprintf(fp, "TargetProduct=  0x7e11\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_Huawei_U8220:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1030\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1034\n");
			fprintf(fp, "MessageContent=\"55534243123456780600000080010a11060000000000000000000000000000\"\n");
		/*fprintf(fp, "NoDriverLoading=1\n");*/
			break;
		case SN_Huawei_T_Mobile_NL:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x14fe\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1506\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
			break;
		case SN_ZTE_K3806Z:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0013\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0015\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Vibe_3G:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0x6061\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000606f50402527000000000000000000000\"\n");
			break;
		case SN_ZTE_MF637:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0110\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0121\n");
			fprintf(fp, "MessageContent=\"5553424302000000000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ONDA_MW836UP_K:
			fprintf(fp, "DefaultVendor=  0x1ee8\n");
			fprintf(fp, "DefaultProduct= 0x0040\n");
			fprintf(fp, "TargetVendor=   0x1ee8\n");
			fprintf(fp, "TargetProduct=  0x003e\n");
			fprintf(fp, "MessageContent=\"555342431234567800000000000010ff000000000000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Huawei_V725:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1009\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "HuaweiMode=     1\n");
			break;
		case SN_Huawei_ET8282:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1da1\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1d09\n");
			fprintf(fp, "HuaweiMode=     1\n");
			break;
		case SN_Huawei_E352:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1449\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1444\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
			break;
		case SN_Huawei_BM358:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x380b\n");
			fprintf(fp, "TargetClass=    0x02\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Haier_CE_100:
			fprintf(fp, "DefaultVendor=  0x201e\n");
			fprintf(fp, "DefaultProduct= 0x2009\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Franklin_Wireless_U210_var:
			fprintf(fp, "DefaultVendor=  0x1fac\n");
			fprintf(fp, "DefaultProduct= 0x0032\n");
			fprintf(fp, "Configuration=  2\n");
			break;
		case SN_DLINK_DWM_156_2:
			fprintf(fp, "DefaultVendor=  0x07d1\n");
			fprintf(fp, "DefaultProduct= 0xa800\n");
			fprintf(fp, "TargetVendor=   0x07d1\n");
			fprintf(fp, "TargetProduct=  0x3e02\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_Exiss_E_190:
			fprintf(fp, "DefaultVendor=  0x8888\n");
			fprintf(fp, "DefaultProduct= 0x6500\n");
			fprintf(fp, "TargetVendor=   0x16d8\n");
			fprintf(fp, "TargetProduct=  0x6533\n");
			fprintf(fp, "MessageContent=\"5553424398e2c4812400000080000bff524445564348473d43440000000000\"\n");
			break;
		case SN_dealextreme:
			fprintf(fp, "DefaultVendor=  0x05c6\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x05c6\n");
			fprintf(fp, "TargetProduct=  0x0015\n");
			fprintf(fp, "MessageContent=\"5553424368032c882400000080000612000000240000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			fprintf(fp, "CheckSuccess=   40\n");
			break;
		case SN_CHU_628S:
			fprintf(fp, "DefaultVendor=  0x16d8\n");
			fprintf(fp, "DefaultProduct= 0x6281\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800008ff524445564348470000000000000000\"\n");
			break;
		case SN_MediaTek_Wimax_USB:
			fprintf(fp, "DefaultVendor=  0x0e8d\n");
			fprintf(fp, "DefaultProduct= 0x7109\n");
			fprintf(fp, "TargetVendor=   0x0e8d\n");
			fprintf(fp, "TargetProduct=  0x7118\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NoDriverLoading=1\n");
			break;
		case SN_AirPlus_MCD_800:
			fprintf(fp, "DefaultVendor=  0x1edf\n");
			fprintf(fp, "DefaultProduct= 0x6003\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "Configuration=  3\n");
			break;
		case SN_UMW190:
			fprintf(fp, "DefaultVendor=  0x106c\n");
			fprintf(fp, "DefaultProduct= 0x3b05\n");
			fprintf(fp, "TargetVendor=   0x106c\n");
			fprintf(fp, "TargetProduct=  0x3716\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800008ff020000000000000000000000000000\"\n");
			break;
		case SN_LG_AD600:
			fprintf(fp, "DefaultVendor=  0x1004\n");
			fprintf(fp, "DefaultProduct= 0x6190\n");
			fprintf(fp, "TargetVendor=   0x1004\n");
			fprintf(fp, "TargetProduct=  0x61a7\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			break;
		case SN_GW_D301:
			fprintf(fp, "DefaultVendor=  0x0fd1\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "Configuration=  3\n");
			break;
		case SN_Qtronix_EVDO_3G:
			fprintf(fp, "DefaultVendor=  0x05c7\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x05c7\n");
			fprintf(fp, "TargetProduct=  0x6000\n");
			fprintf(fp, "MessageContent=\"5553424312345678c00000008000069f140000000000000000000000000000\"\n");
			break;
		case SN_Huawei_EC168C:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1446\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1412\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_Olicard_145:
			fprintf(fp, "DefaultVendor=  0x0b3c\n");
			fprintf(fp, "DefaultProduct= 0xf000\n");
			fprintf(fp, "TargetVendor=   0x0b3c\n");
			fprintf(fp, "TargetProduct=  0xc003\n");
			fprintf(fp, "MessageContent=\"5553424312345678c000000080010606f50402527000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ONDA_MW833UP:
			fprintf(fp, "DefaultVendor=  0x1ee8\n");
			fprintf(fp, "DefaultProduct= 0x0009\n");
			fprintf(fp, "TargetVendor=   0x1ee8\n");
			fprintf(fp, "TargetProduct=  0x000b\n");
			fprintf(fp, "MessageContent=\"555342431234567800000000000010ff000000000000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Kobil_mIdentity_3G_2:
			fprintf(fp, "DefaultVendor=  0x0d46\n");
			fprintf(fp, "DefaultProduct= 0x45a5\n");
			fprintf(fp, "TargetVendor=   0x0d46\n");
			fprintf(fp, "TargetProduct=  0x45ad\n");
			fprintf(fp, "KobilMode=      1\n");
			break;
		case SN_Kobil_mIdentity_3G_1:
			fprintf(fp, "DefaultVendor=  0x0d46\n");
			fprintf(fp, "DefaultProduct= 0x45a1\n");
			fprintf(fp, "TargetVendor=   0x0d46\n");
			fprintf(fp, "TargetProduct=  0x45a9\n");
			fprintf(fp, "KobilMode=      1\n");
			break;
		case SN_Samsung_GT_B3730:
			fprintf(fp, "DefaultVendor=  0x04e8\n");
			fprintf(fp, "DefaultProduct= 0x689a\n");
			fprintf(fp, "TargetVendor=   0x04e8\n");
			fprintf(fp, "TargetProduct=  0x6889\n");
			fprintf(fp, "MessageContent=\"55534243785634120100000080000601000000000000000000000000000000\"\n");
			break;
		case SN_BSNL_Capitel:
			fprintf(fp, "DefaultVendor=  0x1c9e\n");
			fprintf(fp, "DefaultProduct= 0x9e00\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000606f50402527000000000000000000000\"\n");
			break;
		case SN_ZTE_WCDMA_from_BNSL:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x2000\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0108\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Huawei_U8110:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1031\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1035\n");
			fprintf(fp, "CheckSuccess=   20\n");
			fprintf(fp, "MessageContent=\"55534243123456780600000080010a11060000000000000000000000000000\"\n");
		/*fprintf(fp, "NoDriverLoading=1\n");*/
			break;
		case SN_ONDA_MW833UP_2:
			fprintf(fp, "DefaultVendor=  0x1ee8\n");
			fprintf(fp, "DefaultProduct= 0x0013\n");
			fprintf(fp, "TargetVendor=   0x1ee8\n");
			fprintf(fp, "TargetProduct=  0x0012\n");
			fprintf(fp, "CheckSuccess=   20\n");
			fprintf(fp, "MessageContent=\"555342431234567800000000000010ff000000000000000000000000000000\"\n");
			fprintf(fp, "ResponseNeeded= 1\n");
			break;
		case SN_Netgear_WNDA3200:
			fprintf(fp, "DefaultVendor=  0x0cf3\n");
			fprintf(fp, "DefaultProduct= 0x20ff\n");
			fprintf(fp, "TargetVendor=   0x0cf3\n");
			fprintf(fp, "TargetProduct=  0x7010\n");
			fprintf(fp, "CheckSuccess=   10\n");
			fprintf(fp, "NoDriverLoading=1\n");
			fprintf(fp, "MessageContent=\"5553424329000000000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Huawei_R201:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x1523\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x1491\n");
			fprintf(fp, "CheckSuccess=   20\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_Huawei_K4605:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x14c1\n");
			fprintf(fp, "TargetVendor=   0x12d1\n");
			fprintf(fp, "TargetProduct=  0x14c6\n");
			fprintf(fp, "CheckSuccess=   20\n");
			fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
			break;
		case SN_LG_LUU_2100TI:
			fprintf(fp, "DefaultVendor=  0x1004\n");
			fprintf(fp, "DefaultProduct= 0x613f\n");
			fprintf(fp, "TargetVendor=   0x1004\n");
			fprintf(fp, "TargetProduct=  0x6141\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_LG_L_05A:
			fprintf(fp, "DefaultVendor=  0x1004\n");
			fprintf(fp, "DefaultProduct= 0x613a\n");
			fprintf(fp, "TargetVendor=   0x1004\n");
			fprintf(fp, "TargetProduct=  0x6124\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ZTE_MU351:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0003\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ZTE_MF110_var:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0083\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0124\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Olicard_100:
			fprintf(fp, "DefaultVendor=  0x0b3c\n");
			fprintf(fp, "DefaultProduct= 0xc700\n");
			fprintf(fp, "TargetVendor=   0x0b3c\n");
			fprintf(fp, "TargetProductList=\"c000,c001,c002\"\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000030000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_ZTE_MF112:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0103\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0031\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"55534243876543212000000080000c85010101180101010101000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Franklin_Wireless_U210:
			fprintf(fp, "DefaultVendor=  0x1fac\n");
			fprintf(fp, "DefaultProduct= 0x0130\n");
			fprintf(fp, "TargetVendor=   0x1fac\n");
			fprintf(fp, "TargetProduct=  0x0131\n");
			fprintf(fp, "CheckSuccess=   20\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800108df200000000000000000000000000000\"\n");
			break;
		case SN_ZTE_K3805_Z:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x1001\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x1003\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_SE_MD300:
			fprintf(fp, "DefaultVendor=  0x0fce\n");
			fprintf(fp, "DefaultProduct= 0xd0cf\n");
			fprintf(fp, "TargetClass=    0x02\n");
			fprintf(fp, "DetachStorageOnly=1\n");
			fprintf(fp, "Configuration=  3\n");
			break;
		case SN_Digicom_8E4455:
			fprintf(fp, "DefaultVendor=  0x1266\n");
			fprintf(fp, "DefaultProduct= 0x1000\n");
			fprintf(fp, "TargetVendor=   0x1266\n");
			fprintf(fp, "TargetProduct=  0x1009\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424387654321000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_Kyocera_W06K:
			fprintf(fp, "DefaultVendor=  0x0482\n");
			fprintf(fp, "DefaultProduct= 0x024d\n");
			fprintf(fp, "Configuration=  2\n");
			break;
		case SN_LG_HDM_2100:
			fprintf(fp, "DefaultVendor=  0x1004\n");
			fprintf(fp, "DefaultProduct= 0x607f\n");
			fprintf(fp, "TargetVendor=   0x1004\n");
			fprintf(fp, "TargetProduct=  0x6114\n");
			fprintf(fp, "MessageContent=\"1201100102000040041014610000010200018006000100001200\"\n");
			break;
		case SN_Beceem_BCSM250:
			fprintf(fp, "DefaultVendor=  0x198f\n");
			fprintf(fp, "DefaultProduct= 0xbccd\n");
			fprintf(fp, "TargetVendor=   0x198f\n");
			fprintf(fp, "TargetProduct=  0x0220\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800006bc626563240000000000000000000000\"\n");
			break;
		case SN_Beceem_BCSM250_ASUS:
			fprintf(fp, "DefaultVendor=  0x0b05\n");
			fprintf(fp, "DefaultProduct= 0xbccd\n");
			fprintf(fp, "TargetVendor=   0x0b05\n");
			fprintf(fp, "TargetProduct=  0x1781\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800006bc626563240000000000000000000000\"\n");
			break;
		case SN_ZTEAX226:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0xbccd\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0172\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800006bc626563240000000000000000000000\"\n");
			//fprintf(fp, "NoDriverLoading=1\n");
			break;
		case SN_Huawei_U7510:
			fprintf(fp, "DefaultVendor=  0x12d1\n");
			fprintf(fp, "DefaultProduct= 0x101e\n");
			fprintf(fp, "TargetClass=    0xff\n");
			fprintf(fp, "MessageContent=\"55534243123456780600000080000601000000000000000000000000000000\"\n");
			break;
		case SN_ZTE_AC581:
			fprintf(fp, "DefaultVendor=  0x19d2\n");
			fprintf(fp, "DefaultProduct= 0x0026\n");
			fprintf(fp, "TargetVendor=   0x19d2\n");
			fprintf(fp, "TargetProduct=  0x0094\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
			fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
			fprintf(fp, "NeedResponse=   1\n");
			break;
		case SN_UTStarcom_UM185E:
			fprintf(fp, "DefaultVendor=  0x106c\n");
			fprintf(fp, "DefaultProduct= 0x3b06\n");
			fprintf(fp, "TargetVendor=   0x106c\n");
			fprintf(fp, "TargetProduct=  0x3717\n");
			fprintf(fp, "MessageContent=\"555342431234567824000000800008ff020000000000000000000000000000\"\n");
			break;
		case SN_AVM_Fritz:
			fprintf(fp, "DefaultVendor=  0x057c\n");
			fprintf(fp, "DefaultProduct= 0x84ff\n");
			fprintf(fp, "TargetVendor=   0x057c\n");
			fprintf(fp, "TargetProduct=  0x8401\n");
			fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000ff0000000000000000000000\"\n");
			break;
        /* 0818 add */
        case SN_SU_8000U:
                fprintf(fp, "DefaultVendor=  0x2020\n");
                fprintf(fp, "DefaultProduct= 0xf00e\n");
                fprintf(fp, "TargetVendor=   0x2020\n");
                fprintf(fp, "TargetProduct=  0x1005\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                break;
        case SN_Cisco_AM10:
                fprintf(fp, "DefaultVendor=  0x1307\n");
                fprintf(fp, "DefaultProduct= 0x1169\n");
                fprintf(fp, "TargetVendor=   0x13b1\n");
                fprintf(fp, "TargetProduct=  0x0031\n");
                fprintf(fp, "CiscoMode=      1\n");
                break;
        case SN_Huawei_EC122:
                fprintf(fp, "DefaultVendor=  0x12d1\n");
                fprintf(fp, "DefaultProduct= 0x1446\n");
                fprintf(fp, "TargetVendor=   0x12d1\n");
                fprintf(fp, "TargetProduct=  0x140c\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000000000011060000000000000000000000000000\"\n");
                break;
        case SN_Huawei_EC1262:
                fprintf(fp, "DefaultVendor=  0x12d1\n");
                fprintf(fp, "DefaultProduct= 0x1446\n");
                fprintf(fp, "TargetVendor=   0x12d1\n");
                fprintf(fp, "TargetProduct= \"1001,1406,140b,140c,1412,141b,14ac\"\n");
                fprintf(fp, "CheckSuccess=   20\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
                break;
        case SN_HUAWEI_EC306:
                fprintf(fp, "DefaultVendor= 0x12d1\n");
                fprintf(fp, "DefaultProduct=0x1505\n");
                fprintf(fp, "TargetVendor=  0x12d1\n");
                fprintf(fp, "TargetProduct= 0x1506\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
                break;
        case SN_Sierra_U598:
                fprintf(fp, "DefaultVendor=  0x1199\n");
                fprintf(fp, "DefaultProduct= 0x0fff\n");
                fprintf(fp, "TargetVendor=   0x1199\n");
                fprintf(fp, "TargetProduct=  0x0025\n");
                fprintf(fp, "SierraMode=     1\n");
                break;
        case SN_NovatelU720:
                fprintf(fp, "DefaultVendor=  0x1410\n");
                fprintf(fp, "DefaultProduct= 0x2110\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "CheckSuccess=   20\n");            // tmp test
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_Pantech_UM150:                                  // tmp test
                fprintf(fp, "DefaultVendor=  0x106c\n");
                fprintf(fp, "DefaultProduct= 0x3711\n");
                fprintf(fp, "MessageContent=\"555342431234567824000000800008ff024445564348470000000000000000\"\n");
                break;
        case SN_Pantech_UM175:                                  // tmp test
                fprintf(fp, "DefaultVendor=  0x106c\n");
                fprintf(fp, "DefaultProduct= 0x3714\n");
                fprintf(fp, "MessageContent=\"555342431234567824000000800008ff024445564348470000000000000000\"\n");
                break;
        case SN_Sierra_U595:
                fprintf(fp, "DefaultVendor=  0x1199\n");
                fprintf(fp, "DefaultProduct= 0x0120\n");
                fprintf(fp, "CheckSuccess=   10\n");            // tmp test
                fprintf(fp, "SierraMode=     1\n");
                fprintf(fp, "NoDriverLoading=1\n");
                break;
	/* 120703 add */
        case SN_Qisda_H21:                                     
                fprintf(fp, "DefaultVendor=  0x1da5\n");
                fprintf(fp, "DefaultProduct= 0xf000\n");
                fprintf(fp, "TargetVendor=   0x1da5\n");
                fprintf(fp, "TargetProduct=  0x4512\n");
                fprintf(fp, "QisdaMode=1\n");
                break;
        case SN_StrongRising_Air_FlexiNet:
                fprintf(fp, "DefaultVendor=  0x21f5\n");
                fprintf(fp, "DefaultProduct= 0x1000\n");
                fprintf(fp, "TargetVendor=   0x21f5\n");
                fprintf(fp, "TargetProduct=  0x2008\n");
                fprintf(fp, "MessageContent=\"5553424312345678c000000080000671010000000000000000000000000000\"\n");
                break;
        case SN_BandLuxe_C339:
                fprintf(fp, "DefaultVendor=  0x1a8d\n");
                fprintf(fp, "DefaultProduct= 0x2000\n");
                fprintf(fp, "TargetVendor=   0x1a8d\n");
                fprintf(fp, "TargetProduct=  0x2006\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
                fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_Celot_CT680:
                fprintf(fp, "DefaultVendor=  0x05c6\n");
                fprintf(fp, "DefaultProduct= 0x1000\n");
                fprintf(fp, "TargetVendor=   0x211f\n");
                fprintf(fp, "TargetProduct=  0x6802\n");
                fprintf(fp, "MessageContent=\"5553424312345678c000000080000671010000000000000000000000000000\"\n");
                break;
        case SN_Huawei_E353:
                fprintf(fp, "DefaultVendor=  0x12d1\n");
                fprintf(fp, "DefaultProduct= 0x1f01\n");
                fprintf(fp, "TargetVendor=   0x12d1\n");
                fprintf(fp, "TargetProduct=  0x14db\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000000000a11062000000000000100000000000000\"\n");
                fprintf(fp, "NoDriverLoading=1\n");
                break;
        case SN_Haier_CE682:
                fprintf(fp, "DefaultVendor=  0x201e\n");
                fprintf(fp, "DefaultProduct= 0x1023\n");
                fprintf(fp, "TargetVendor=   0x201e\n");
                fprintf(fp, "TargetProduct=  0x1022\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000000000600000000000000000000000000000000\"\n");
                fprintf(fp, "MessageContent2=\"5553424312345679c000000080000671030000000000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_LG_L_02C_LTE:
                fprintf(fp, "DefaultVendor=  0x1004\n");
                fprintf(fp, "DefaultProduct= 0x61dd\n");
                fprintf(fp, "TargetVendor=   0x1004\n");
                fprintf(fp, "TargetProduct=  0x618f\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_LG_SD711:
                fprintf(fp, "DefaultVendor=  0x1004\n");
                fprintf(fp, "DefaultProduct= 0x61e7\n");
                fprintf(fp, "TargetVendor=   0x1004\n");
                fprintf(fp, "TargetProduct=  0x61e6\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                break;
        case SN_LG_L_08C:
                fprintf(fp, "DefaultVendor=  0x1004\n");
                fprintf(fp, "DefaultProduct= 0x61eb\n");
                fprintf(fp, "TargetVendor=   0x1004\n");
                fprintf(fp, "TargetProduct=  0x61ea\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_IODATA_WMX2_U:
                fprintf(fp, "DefaultVendor=  0x04bb\n");
                fprintf(fp, "DefaultProduct= 0xbccd\n");
                fprintf(fp, "TargetVendor=   0x04bb\n");
                fprintf(fp, "TargetProduct=  0x0949\n");
                fprintf(fp, "MessageContent=\"55534243f0298d8124000000800006bc626563240000000000000000000000\"\n");
                break;
        case SN_Option_GI1515:
                fprintf(fp, "DefaultVendor=  0x0af0\n");
                fprintf(fp, "DefaultProduct= 0xd001\n");
                fprintf(fp, "TargetVendor=   0x0af0\n");
                fprintf(fp, "TargetProduct=  0xd157\n");
                fprintf(fp, "MessageContent=\"55534243785634120100000080000601000000000000000000000000000000\"\n");
                break;
        case SN_LG_L_07A:
                fprintf(fp, "DefaultVendor=  0x1004\n");
                fprintf(fp, "DefaultProduct= 0x614e\n");
                fprintf(fp, "TargetVendor=   0x1004\n");
                fprintf(fp, "TargetProduct=  0x6135\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061e000000000000000000000000000000\"\n");
                fprintf(fp, "MessageContent2=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_ZTE_A371B:
                fprintf(fp, "DefaultVendor=  0x19d2\n");
                fprintf(fp, "DefaultProduct= 0x0169\n");
                fprintf(fp, "TargetVendor=   0x19d2\n");
                fprintf(fp, "TargetProduct=  0x0170\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_ZTE_MF652:
                fprintf(fp, "DefaultVendor=  0x19d2\n");
                fprintf(fp, "DefaultProduct= 0x1520\n");
                fprintf(fp, "TargetVendor=   0x19d2\n");
                fprintf(fp, "TargetProduct=  0x0142\n");
                fprintf(fp, "MessageContent=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_ZTE_MF652_VAR:
                fprintf(fp, "DefaultVendor=  0x19d2\n");
                fprintf(fp, "DefaultProduct= 0x0146\n");
                fprintf(fp, "TargetVendor=   0x19d2\n");
                fprintf(fp, "TargetProduct=  0x0143\n");
                fprintf(fp, "MessageContent=\"5553424312345679000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_T_W_WU160:
                fprintf(fp, "DefaultVendor=  0x2077\n");
                fprintf(fp, "DefaultProduct= 0xf000\n");
                fprintf(fp, "TargetVendor=   0x2077\n");
                fprintf(fp, "TargetProduct=  0x9000\n");
                fprintf(fp, "MessageContent=\"5553424308902082000000000000061b000000020000000000000000000000\"\n");
                break;
        case SN_Nokia_CS_21M_02:
                fprintf(fp, "DefaultVendor=  0x0421\n");
                fprintf(fp, "DefaultProduct= 0x0637\n");
                fprintf(fp, "TargetVendor=   0x0421\n");
                fprintf(fp, "TargetProduct=  0x0638\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                break;
        case SN_Telewell_TW_3G:
                fprintf(fp, "DefaultVendor=  0x1c9e\n");
                fprintf(fp, "DefaultProduct= 0x98ff\n");
                fprintf(fp, "TargetVendor=   0x1c9e\n");
                fprintf(fp, "TargetProduct=  0x9801\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000080000606f50402527000000000000000000000\"\n");
                break;
        case SN_ZTE_MF637_2:
                fprintf(fp, "DefaultVendor=  0x19d2\n");
                fprintf(fp, "DefaultProduct= 0x0031\n");
                fprintf(fp, "TargetVendor=   0x19d2\n");
                fprintf(fp, "TargetProduct=  0x0094\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_Samsung_GT_B1110:
                fprintf(fp, "DefaultVendor=  0x04e8\n");
                fprintf(fp, "DefaultProduct= 0x680c\n");
                fprintf(fp, "TargetVendor=   0x04e8\n");
                fprintf(fp, "TargetProduct=  0x6792\n");
                fprintf(fp, "MessageContent=\"0902200001010080fa0904000002080650000705010200020007058102000200\"\n");
                break;
        case SN_ZTE_MF192_VAR:
                fprintf(fp, "DefaultVendor=  0x19d2\n");
                fprintf(fp, "DefaultProduct= 0x1514\n");
                fprintf(fp, "TargetVendor=   0x19d2\n");
                fprintf(fp, "TargetProduct=  0x1515\n");
                fprintf(fp, "MessageContent=\"5553424348c4758600000000000010ff000000000000000000000000000000\"\n");
                break;
        case SN_MediaTek_MT6276M:
                fprintf(fp, "DefaultVendor=  0x0e8d\n");
                fprintf(fp, "DefaultProduct= 0x0002\n");
                fprintf(fp, "TargetVendor=   0x0e8d\n");
                fprintf(fp, "TargetProductList=\"00a1,00a2\"\n");
                fprintf(fp, "MessageContent=\"555342431234567800000000000006f0010300000000000000000000000000\"\n");
                break;
        case SN_Tata_Photon_plus:
                fprintf(fp, "DefaultVendor=  0x22f4\n");
                fprintf(fp, "DefaultProduct= 0x0021\n");
                fprintf(fp, "TargetClass=    0xff\n");
                fprintf(fp, "MessageContent=\"555342439f000000000000000000061b000000020000000000000000000000\"\n");
                break;
        case SN_Option_Globetrotter:
                fprintf(fp, "DefaultVendor=  0x0af0\n");
                fprintf(fp, "DefaultProduct= 0x8006\n");
                fprintf(fp, "TargetVendor=   0x0af0\n");
                fprintf(fp, "TargetProduct=  0x9100\n");
                fprintf(fp, "MessageContent=\"55534243785634120100000080000601000000000000000000000000000000\"\n");
                break;
        case SN_Option_ICON_711:
                fprintf(fp, "DefaultVendor=  0x0af0\n");
                fprintf(fp, "DefaultProduct= 0x4007\n");
                fprintf(fp, "TargetVendor=   0x0af0\n");
                fprintf(fp, "TargetProduct=  0x4005\n");
                fprintf(fp, "SierraMode=     1\n");
                break;
        case SN_Celot_K300:
                fprintf(fp, "DefaultVendor=  0x05c6\n");
                fprintf(fp, "DefaultProduct= 0x1000\n");
                fprintf(fp, "TargetVendor=   0x211f\n");
                fprintf(fp, "TargetProduct=  0x6801\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                break;
        case SN_HISENSE_E910:
                fprintf(fp, "DefaultVendor=  0x109b\n");
                fprintf(fp, "DefaultProduct= 0xf009\n");
                fprintf(fp, "TargetVendor=   0x109b\n");
                fprintf(fp, "TargetProduct=  0x9114\n");
                fprintf(fp, "MessageContent=\"5553424312345678000000000000061b000000020000000000000000000000\"\n");
                fprintf(fp, "NeedResponse=   1\n");
                break;
        case SN_Huawei_K5005:
                fprintf(fp, "DefaultVendor=  0x12d1\n");
                fprintf(fp, "DefaultProduct= 0x14c3\n");
                fprintf(fp, "TargetVendor=   0x12d1\n");
                fprintf(fp, "TargetProduct=  0x14c8\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
                break;
        case SN_Huawei_E173_MoiveStar:
                fprintf(fp, "DefaultVendor=  0x12d1\n");
                fprintf(fp, "DefaultProduct= 0x1c24\n");
                fprintf(fp, "TargetVendor=   0x12d1\n");
                fprintf(fp, "TargetProduct=  0x1c12\n");
                fprintf(fp, "MessageContent=\"55534243123456780000000000000011062000000100000000000000000000\"\n");
                break;
        case SN_Onda_MSA14_4:
                fprintf(fp, "DefaultVendor=  0x1ee8\n");
                fprintf(fp, "DefaultProduct= 0x0060\n");
                fprintf(fp, "TargetVendor=   0x1ee8\n");
                fprintf(fp, "TargetProduct=  0x005f\n");
                fprintf(fp, "MessageContent=\"555342431234567800000000000008ff00000000000003000000000000000\"\n");
                break;
	default:
			fprintf(fp, "\n");
			if(vid && pid){
				fprintf(fp, "DefaultVendor=  0x%s\n", vid);
				fprintf(fp, "DefaultProduct= 0x%s\n", pid);
				if(strcmp(vid, "12d1") == 0){    // Huawei
					fprintf(fp, "HuaweiMode=1\n");
				}
			}
			break;
	}

	return 0;
}

int init_3g_param(char *vid, char *pid, int port_num)
{
	FILE *fp;
	char conf_file[32];
	int asus_extra_auto = 0;

	//reset error variable generated by Generic_conn.scr...
	//ex. caused by pin code error
	nvram_set("simerr", "");
	nvram_set("connerr", "");
	nvram_set("pinerr", "");
	nvram_set("apnerr", "");
	nvram_set("g3err", "");

	memset(conf_file, 0, 32);
	sprintf(conf_file, "%s.%d", USB_MODESWITCH_CONF, port_num);
	if((fp = fopen(conf_file, "r")) != NULL){
		fclose(fp);
		usb_dbg("%s:%s Already had the usb_modeswitch file.\n", vid, pid);
		return 1;
	}

	fp = fopen(conf_file, "w+");
	if(!fp){
		usb_dbg("Couldn't write the config file(%s).\n", conf_file);
		return 0;
	}

	char dongle_name[32];

	memset(dongle_name, 0, 32);
	strncpy(dongle_name, nvram_safe_get("Dev3G"), 32);

	if(!strcmp(nvram_safe_get("Dev3G"), "Huawei-E160G")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E176")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-K3520")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-ET128")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E1800")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E172")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E372")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E122")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E160E")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E1552")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E1823")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E1762")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E1750C")
			|| !strcmp(nvram_safe_get("Dev3G"), "Huawei-E1752Cu")
			)
		asus_extra_auto = 1;

	if(nvram_match("Dev3G", "AUTO") || (asus_extra_auto == 1)){
		if(asus_extra_auto)
			nvram_set("d3g", nvram_safe_get("Dev3G"));
		else
			nvram_set("d3g", "usb_3g_dongle");
usb_dbg("3G: Auto setting.\n");

		if(!strcmp(vid, "0408") && (!strcmp(pid, "ea02") || !strcmp(pid, "1000")))
			write_3g_conf(fp, SN_MU_Q101, 1, vid, pid);
		else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "6971")==0))
		{
			nvram_set("d3g", "OPTION-ICON225");
			write_3g_conf(fp, SN_OPTION_ICON225, 1, vid, pid);
		}
		else if((strcmp(vid, "05c6")==0) && (strcmp(pid, "1000")==0)) // also Option-GlobeSurfer-Icon72(may have new fw setting, bug not included here), Option-GlobeTrotter-GT-MAX36.....Option-Globexx series, AnyDATA-ADU-500A, Samsung-SGH-Z810, Vertex Wireless 100 Series
			write_3g_conf(fp, SN_Option_GlobeSurfer_Icon, 1, vid, pid);
		else if((strcmp(vid, "1e0e")==0) && (strcmp(pid, "f000")==0))	// A-Link-3GU
			write_3g_conf(fp, SN_Option_iCON210, 1, vid, pid);
		else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "7011")==0))
		{
			nvram_set("d3g", "Option-GlobeTrotter-HSUPA-Modem");
			write_3g_conf(fp, SN_Option_GlobeTrotter_HSUPA_Modem, 1, vid, pid);
		}
		else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "7401")==0))
		{
			nvram_set("d3g", "Option-iCON-401");
			write_3g_conf(fp, SN_Option_iCON401, 1, vid, pid);
		}
		else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "7501")==0))
		{
			nvram_set("d3g", "Vodafone-K3760");
			write_3g_conf(fp, SN_Vodafone_K3760, 1, vid, pid);
		}
		else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "d033")==0))
		{
			nvram_set("d3g", "ATT-USBConnect-Quicksilver");
			write_3g_conf(fp, SN_ATT_USBConnect_Quicksilver, 1, vid, pid);
		}
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1505")==0))
			write_3g_conf(fp, SN_Huawei_EC156, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1001")==0))
			write_3g_conf(fp, SN_Huawei_E169, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1003")==0))
			write_3g_conf(fp, SN_Huawei_E220, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1414")==0))
			write_3g_conf(fp, SN_Huawei_E180, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1c0b")==0))   // tmp test
			write_3g_conf(fp, SN_Huawei_Exxx, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "14b5")==0))
			write_3g_conf(fp, SN_Huawei_E173, 1, vid, pid);
		else if((strcmp(vid, "1033")==0) && (strcmp(pid, "0035")==0))
			write_3g_conf(fp, SN_Huawei_E630, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1446")==0))	// E1550, E1612, E1690
			write_3g_conf(fp, SN_Huawei_E270, 1, vid, pid);
			//write_3g_conf(fp, SN_Huawei_E1550, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "2000")==0))	// also ZTE622, 628, 626, 6535-Z, K3520-Z, K3565, ONDA-MT503HS, ONDA-MT505UP
			write_3g_conf(fp, SN_ZTE_MF626, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "fff5")==0))
			write_3g_conf(fp, SN_ZTE_AC2710, 1, vid, pid);	// 2710
		else if((strcmp(vid, "1410")==0) && (strcmp(pid, "5010")==0))	// U727
			write_3g_conf(fp, SN_Novatel_Wireless_Ovation_MC950D, 1, vid, pid);
		else if((strcmp(vid, "1410")==0) && (strcmp(pid, "5020")==0))
			write_3g_conf(fp, SN_Novatel_MC990D, 1, vid, pid);
		else if((strcmp(vid, "1410")==0) && (strcmp(pid, "5030")==0))
			write_3g_conf(fp, SN_Novatel_U760, 1, vid, pid);
		else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "1001")==0))
			write_3g_conf(fp, SN_Alcatel_X020, 1, vid, pid);
		else if((strcmp(vid, "1bbb")==0) && (strcmp(pid, "f000")==0))
			write_3g_conf(fp, SN_Alcatel_X200, 1, vid, pid);
		//else if((strcmp(vid, "1a8d")==0) && (strcmp(pid, "1000")==0))
		//	write_3g_conf(fp, SN_BandLuxe_C120, 1, vid, pid);
		else if((strcmp(vid, "1dd6")==0) && (strcmp(pid, "1000")==0))
			write_3g_conf(fp, SN_Solomon_S3Gm660, 1, vid, pid);
		else if((strcmp(vid, "16d8")==0) && (strcmp(pid, "6803")==0))
			write_3g_conf(fp, SN_C_motechD50, 1, vid, pid);
		else if((strcmp(vid, "16d8")==0) && (strcmp(pid, "f000")==0))
			write_3g_conf(fp, SN_C_motech_CGU628, 1, vid, pid);
		else if((strcmp(vid, "0930")==0) && (strcmp(pid, "0d46")==0))
			write_3g_conf(fp, SN_Toshiba_G450, 1, vid, pid);
		else if((strcmp(vid, "106c")==0) && (strcmp(pid, "3b03")==0))
			write_3g_conf(fp, SN_UTStarcom_UM175, 1, vid, pid);
		else if((strcmp(vid, "1ab7")==0) && (strcmp(pid, "5700")==0))
			write_3g_conf(fp, SN_Hummer_DTM5731, 1, vid, pid);
		else if((strcmp(vid, "1199")==0) && (strcmp(pid, "0fff")==0))	// Sierra881U
			write_3g_conf(fp, SN_Sierra_Wireless_Compass597, 1, vid, pid);
		else if((strcmp(vid, "0fce")==0) && (strcmp(pid, "d0e1")==0))
			write_3g_conf(fp, SN_Sony_Ericsson_MD400, 1, vid, pid);
		else if((strcmp(vid, "1004")==0) && (strcmp(pid, "1000")==0))
			write_3g_conf(fp, SN_LG_LDU_1900D, 1, vid, pid);
		else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "f000")==0))	// ST-Mobile, MobiData MBD-200HU, // BSNL 310G
			write_3g_conf(fp, SN_BSNL_310G, 1, vid, pid);
		else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "9605")==0))	// chk BSNL 310G
			write_3g_conf(fp, SN_BSNL_310G, 1, vid, pid);
		else if((strcmp(vid, "230d")==0) && (strcmp(pid, "0001")==0))
			write_3g_conf(fp, SN_BSNL_LW272, 1, vid, pid);
		else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "9200")==0))
			write_3g_conf(fp, SN_MyWave_SW006, 1, vid, pid);
		else if((strcmp(vid, "1f28")==0) && (strcmp(pid, "0021")==0))
			write_3g_conf(fp, SN_Cricket_A600, 1, vid, pid);
		else if((strcmp(vid, "1b7d")==0) && (strcmp(pid, "0700")==0))
			write_3g_conf(fp, SN_EpiValley_SEC7089, 1, vid, pid);
		else if((strcmp(vid, "04e8")==0) && (strcmp(pid, "f000")==0))
			write_3g_conf(fp, SN_Samsung_U209, 1, vid, pid);
		else if((strcmp(vid, "05c6")==0) && (strcmp(pid, "2001")==0))
			write_3g_conf(fp, SN_D_Link_DWM162_U5, 1, vid, pid);
		else if((strcmp(vid, "1410")==0) && (strcmp(pid, "5031")==0))
			write_3g_conf(fp, SN_Novatel_MC760, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0053")==0))
			write_3g_conf(fp, SN_ZTE_MF110, 1, vid, pid);
		else if((strcmp(vid, "0471")==0) && (strcmp(pid, "1237")==0))	// HuaXing E600
			write_3g_conf(fp, SN_Philips_TalkTalk, 1, vid, pid);
		else if((strcmp(vid, "16d8")==0) && (strcmp(pid, "700a")==0))
			write_3g_conf(fp, SN_C_motech_CHU_629S, 1, vid, pid);
		else if((strcmp(vid, "0421")==0) && (strcmp(pid, "060c")==0))	
			write_3g_conf(fp, SN_Nokia_CS10, 1, vid, pid);
		else if((strcmp(vid, "0421")==0) && (strcmp(pid, "0610")==0))
			write_3g_conf(fp, SN_Nokia_CS15, 1, vid, pid);
		else if((strcmp(vid, "0421")==0) && (strcmp(pid, "0622")==0))	
			write_3g_conf(fp, SN_Nokia_CS17, 1, vid, pid);
		else if((strcmp(vid, "0421")==0) && (strcmp(pid, "0627")==0))	
			write_3g_conf(fp, SN_Nokia_CS18, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1520")==0))
			write_3g_conf(fp, SN_Huawei_K3765, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1521")==0))
			write_3g_conf(fp, SN_Huawei_K4505, 1, vid, pid);
		else if((strcmp(vid, "0471")==0) && (strcmp(pid, "1210")==0))
			write_3g_conf(fp, SN_Vodafone_MD950, 1, vid, pid);
		else if((strcmp(vid, "05c6")==0) && (strcmp(pid, "f000")==0))
			write_3g_conf(fp, SN_Siptune_LM75, 1, vid, pid);
		/* 0715 add */
		else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "9800")==0))	
			write_3g_conf(fp, SN_SU9800, 1, vid, pid);
		else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "7a05")==0))	
			write_3g_conf(fp, SN_OPTION_ICON_461, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "14c4")==0))	
			write_3g_conf(fp, SN_Huawei_K3771, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "14d1")==0))	
			write_3g_conf(fp, SN_Huawei_K3770, 1, vid, pid);
		else if((strcmp(vid, "0df7")==0) && (strcmp(pid, "0800")==0))	
			write_3g_conf(fp, SN_Mobile_Action, 1, vid, pid);
		else if((strcmp(vid, "03f0")==0) && (strcmp(pid, "002a")==0))	
			write_3g_conf(fp, SN_HP_P1102, 1, vid, pid);
		else if((strcmp(vid, "230d")==0) && (strcmp(pid, "0007")==0))	
			write_3g_conf(fp, SN_Visiontek_82GH, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0149")==0))	
			write_3g_conf(fp, SN_ZTE_MF190_var, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "1216")==0))	
			write_3g_conf(fp, SN_ZTE_MF192, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "1201")==0))	
			write_3g_conf(fp, SN_ZTE_MF691, 1, vid, pid);
		else if((strcmp(vid, "16d8")==0) && (strcmp(pid, "700b")==0))	
			write_3g_conf(fp, SN_CHU_629S, 1, vid, pid);
		else if((strcmp(vid, "198a")==0) && (strcmp(pid, "0003")==0))	
			write_3g_conf(fp, SN_JOA_LM_700r, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "1224")==0))	
			write_3g_conf(fp, SN_ZTE_MF190, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "ffe6")==0))	
			write_3g_conf(fp, SN_ZTE_ffe, 1, vid, pid);
		else if((strcmp(vid, "0fce")==0) && (strcmp(pid, "d103")==0))	
			write_3g_conf(fp, SN_SE_MD400G, 1, vid, pid);
		else if((strcmp(vid, "07d1")==0) && (strcmp(pid, "a804")==0))	
			write_3g_conf(fp, SN_DLINK_DWM_156, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1030")==0))	
			write_3g_conf(fp, SN_Huawei_U8220, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "14fe")==0))	
			write_3g_conf(fp, SN_Huawei_T_Mobile_NL, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0013")==0))	
			write_3g_conf(fp, SN_ZTE_K3806Z, 1, vid, pid);
		else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "6061")==0))	
			write_3g_conf(fp, SN_Vibe_3G, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0110")==0))	
			write_3g_conf(fp, SN_ZTE_MF637, 1, vid, pid);
		else if((strcmp(vid, "1ee8")==0) && (strcmp(pid, "0040")==0))	
			write_3g_conf(fp, SN_ONDA_MW836UP_K, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1009")==0))	
			write_3g_conf(fp, SN_Huawei_V725, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1da1")==0))	
			write_3g_conf(fp, SN_Huawei_ET8282, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1449")==0))	
			write_3g_conf(fp, SN_Huawei_E352, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "380b")==0))	
			write_3g_conf(fp, SN_Huawei_BM358, 1, vid, pid);
		else if((strcmp(vid, "201e")==0) && (strcmp(pid, "2009")==0))	
			write_3g_conf(fp, SN_Haier_CE_100, 1, vid, pid);
		else if((strcmp(vid, "1fac")==0) && (strcmp(pid, "0032")==0))	
			write_3g_conf(fp, SN_Franklin_Wireless_U210_var, 1, vid, pid);
		/*else if((strcmp(vid, "07d1")==0) && (strcmp(pid, "a800")==0))	
			write_3g_conf(fp, SN_DLINK_DWM_156_2, 1, vid, pid);//*/ // Couldn't usb usb_modeswitch.
		else if((strcmp(vid, "8888")==0) && (strcmp(pid, "6500")==0))	
			write_3g_conf(fp, SN_Exiss_E_190, 1, vid, pid);
		else if((strcmp(vid, "05c6")==0) && (strcmp(pid, "2000")==0))	
			write_3g_conf(fp, SN_dealextreme, 1, vid, pid);
		else if((strcmp(vid, "16d8")==0) && (strcmp(pid, "6281")==0))	
			write_3g_conf(fp, SN_CHU_628S, 1, vid, pid);
		/*else if((strcmp(vid, "0e8d")==0) && (strcmp(pid, "7109")==0))	
			write_3g_conf(fp, SN_MediaTek_Wimax_USB, 1, vid, pid);//*/ // Couldn't usb usb_modeswitch.
		else if((strcmp(vid, "1edf")==0) && (strcmp(pid, "6003")==0))	
			write_3g_conf(fp, SN_AirPlus_MCD_800, 1, vid, pid);
		else if((strcmp(vid, "106c")==0) && (strcmp(pid, "3b05")==0))	
			write_3g_conf(fp, SN_UMW190, 1, vid, pid);
		else if((strcmp(vid, "1004")==0) && (strcmp(pid, "6190")==0))	
			write_3g_conf(fp, SN_LG_AD600, 1, vid, pid);
		else if((strcmp(vid, "0fd1")==0) && (strcmp(pid, "1000")==0))	
			write_3g_conf(fp, SN_GW_D301, 1, vid, pid);
		else if((strcmp(vid, "05c7")==0) && (strcmp(pid, "1000")==0))	
			write_3g_conf(fp, SN_Qtronix_EVDO_3G, 1, vid, pid);
		else if((strcmp(vid, "0b3c")==0) && (strcmp(pid, "f000")==0))	
			write_3g_conf(fp, SN_Olicard_145, 1, vid, pid);
		else if((strcmp(vid, "1ee8")==0) && (strcmp(pid, "0009")==0))	
			write_3g_conf(fp, SN_ONDA_MW833UP, 1, vid, pid);
		else if((strcmp(vid, "0d46")==0) && (strcmp(pid, "45a5")==0))	
			write_3g_conf(fp, SN_Kobil_mIdentity_3G_2, 1, vid, pid);
		else if((strcmp(vid, "0d46")==0) && (strcmp(pid, "45a1")==0))	
			write_3g_conf(fp, SN_Kobil_mIdentity_3G_1, 1, vid, pid);
		else if((strcmp(vid, "04e8")==0) && (strcmp(pid, "689a")==0))	
			write_3g_conf(fp, SN_Samsung_GT_B3730, 1, vid, pid);
		else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "9e00")==0))	
			write_3g_conf(fp, SN_BSNL_Capitel, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1031")==0))	
			write_3g_conf(fp, SN_Huawei_U8110, 1, vid, pid);
		else if((strcmp(vid, "1ee8")==0) && (strcmp(pid, "0013")==0))	
			write_3g_conf(fp, SN_ONDA_MW833UP_2, 1, vid, pid);
		else if((strcmp(vid, "0cf3")==0) && (strcmp(pid, "20ff")==0))	
			write_3g_conf(fp, SN_Netgear_WNDA3200, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1523")==0))	
			write_3g_conf(fp, SN_Huawei_R201, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "14c1")==0))	
			write_3g_conf(fp, SN_Huawei_K4605, 1, vid, pid);
		else if((strcmp(vid, "1004")==0) && (strcmp(pid, "613f")==0))	
			write_3g_conf(fp, SN_LG_LUU_2100TI, 1, vid, pid);
		else if((strcmp(vid, "1004")==0) && (strcmp(pid, "613a")==0))	
			write_3g_conf(fp, SN_LG_L_05A, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0003")==0))	
			write_3g_conf(fp, SN_ZTE_MU351, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0083")==0))	
			write_3g_conf(fp, SN_ZTE_MF110_var, 1, vid, pid);
		else if((strcmp(vid, "0b3c")==0) && (strcmp(pid, "c700")==0))	
			write_3g_conf(fp, SN_Olicard_100, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0103")==0))	
			write_3g_conf(fp, SN_ZTE_MF112, 1, vid, pid);
		else if((strcmp(vid, "1fac")==0) && (strcmp(pid, "0130")==0))	
			write_3g_conf(fp, SN_Franklin_Wireless_U210, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "1001")==0))	
			write_3g_conf(fp, SN_ZTE_K3805_Z, 1, vid, pid);
		else if((strcmp(vid, "0fce")==0) && (strcmp(pid, "d0cf")==0))	
			write_3g_conf(fp, SN_SE_MD300, 1, vid, pid);
		else if((strcmp(vid, "1266")==0) && (strcmp(pid, "1000")==0))	
			write_3g_conf(fp, SN_Digicom_8E4455, 1, vid, pid);
		else if((strcmp(vid, "0482")==0) && (strcmp(pid, "024d")==0))	
			write_3g_conf(fp, SN_Kyocera_W06K, 1, vid, pid);
		else if((strcmp(vid, "1004")==0) && (strcmp(pid, "607f")==0))	
			write_3g_conf(fp, SN_LG_HDM_2100, 1, vid, pid);
		else if((strcmp(vid, "198f")==0) && (strcmp(pid, "bccd")==0))	
			write_3g_conf(fp, SN_Beceem_BCSM250, 1, vid, pid);
		else if((strcmp(vid, "0b05")==0) && (strcmp(pid, "bccd")==0))	
			write_3g_conf(fp, SN_Beceem_BCSM250_ASUS, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "bccd")==0))	
			write_3g_conf(fp, SN_ZTEAX226, 1, vid, pid);
		else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "101e")==0))	
			write_3g_conf(fp, SN_Huawei_U7510, 1, vid, pid);
		else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0026")==0))	
			write_3g_conf(fp, SN_ZTE_AC581, 1, vid, pid);
		else if((strcmp(vid, "106c")==0) && (strcmp(pid, "3b06")==0))	
			write_3g_conf(fp, SN_UTStarcom_UM185E, 1, vid, pid);
		else if((strcmp(vid, "057c")==0) && (strcmp(pid, "84ff")==0))	
			write_3g_conf(fp, SN_AVM_Fritz, 1, vid, pid);
                /* 0818 add */
                else if((strcmp(vid, "2020")==0) && (strcmp(pid, "f00e")==0))
                        write_3g_conf(fp, SN_SU_8000U, 1, vid, pid);
                else if((strcmp(vid, "1307")==0) && (strcmp(pid, "1169")==0))
                        write_3g_conf(fp, SN_Cisco_AM10, 1, vid, pid);
                else if((strcmp(vid, "1410")==0) && (strcmp(pid, "2110")==0))
                        write_3g_conf(fp, SN_NovatelU720, 1, vid, pid);
                else if((strcmp(vid, "106c")==0) && (strcmp(pid, "3711")==0))
                        write_3g_conf(fp, SN_Pantech_UM150, 1, vid, pid);
                else if((strcmp(vid, "106c")==0) && (strcmp(pid, "3714")==0))
                        write_3g_conf(fp, SN_Pantech_UM175, 1, vid, pid);
                else if((strcmp(vid, "1199")==0) && (strcmp(pid, "0120")==0))
                        write_3g_conf(fp, SN_Sierra_U595, 1, vid, pid);
                /* 120703 add */
                else if((strcmp(vid, "1da5")==0) && (strcmp(pid, "f000")==0))
                        write_3g_conf(fp, SN_Qisda_H21, 1, vid, pid);
                else if((strcmp(vid, "21f5")==0) && (strcmp(pid, "1000")==0))
                        write_3g_conf(fp, SN_StrongRising_Air_FlexiNet, 1, vid, pid);
                else if((strcmp(vid, "1a8d")==0) && (strcmp(pid, "2000")==0))
                        write_3g_conf(fp, SN_BandLuxe_C339, 1, vid, pid);
                else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1f01")==0))
                        write_3g_conf(fp, SN_Huawei_E353, 1, vid, pid);
                else if((strcmp(vid, "201e")==0) && (strcmp(pid, "1023")==0))
                        write_3g_conf(fp, SN_Haier_CE682, 1, vid, pid);
                else if((strcmp(vid, "1004")==0) && (strcmp(pid, "61dd")==0))
                        write_3g_conf(fp, SN_LG_L_02C_LTE, 1, vid, pid);
                else if((strcmp(vid, "1004")==0) && (strcmp(pid, "61e7")==0))
                        write_3g_conf(fp, SN_LG_SD711, 1, vid, pid);
                else if((strcmp(vid, "1004")==0) && (strcmp(pid, "61eb")==0))
                        write_3g_conf(fp, SN_LG_L_08C, 1, vid, pid);
                else if((strcmp(vid, "04bb")==0) && (strcmp(pid, "bccd")==0))
                        write_3g_conf(fp, SN_IODATA_WMX2_U, 1, vid, pid);
                else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "d001")==0))
                        write_3g_conf(fp, SN_Option_GI1515, 1, vid, pid);
                else if((strcmp(vid, "1004")==0) && (strcmp(pid, "614e")==0))
                        write_3g_conf(fp, SN_LG_L_07A, 1, vid, pid);
                else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0169")==0))
                        write_3g_conf(fp, SN_ZTE_A371B, 1, vid, pid);
                else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "1520")==0))
                        write_3g_conf(fp, SN_ZTE_MF652, 1, vid, pid);
                else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0146")==0))
                        write_3g_conf(fp, SN_ZTE_MF652_VAR, 1, vid, pid);
                else if((strcmp(vid, "2077")==0) && (strcmp(pid, "f000")==0))
                        write_3g_conf(fp, SN_T_W_WU160, 1, vid, pid);
                else if((strcmp(vid, "0421")==0) && (strcmp(pid, "0637")==0))
                        write_3g_conf(fp, SN_Nokia_CS_21M_02, 1, vid, pid);
                else if((strcmp(vid, "1c9e")==0) && (strcmp(pid, "98ff")==0))
                        write_3g_conf(fp, SN_Telewell_TW_3G, 1, vid, pid);
                else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "0031")==0))
                        write_3g_conf(fp, SN_ZTE_MF637_2, 1, vid, pid);
                else if((strcmp(vid, "04e8")==0) && (strcmp(pid, "680c")==0))
                        write_3g_conf(fp, SN_Samsung_GT_B1110, 1, vid, pid);
                else if((strcmp(vid, "19d2")==0) && (strcmp(pid, "1514")==0))
                        write_3g_conf(fp, SN_ZTE_MF192_VAR, 1, vid, pid);
                else if((strcmp(vid, "0e8d")==0) && (strcmp(pid, "0002")==0))
                        write_3g_conf(fp, SN_MediaTek_MT6276M, 1, vid, pid);
                else if((strcmp(vid, "22f4")==0) && (strcmp(pid, "0021")==0))
                        write_3g_conf(fp, SN_Tata_Photon_plus, 1, vid, pid);
                else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "8006")==0))
                        write_3g_conf(fp, SN_Option_Globetrotter, 1, vid, pid);
                else if((strcmp(vid, "0af0")==0) && (strcmp(pid, "4007")==0))
                        write_3g_conf(fp, SN_Option_ICON_711, 1, vid, pid);
                else if((strcmp(vid, "109b")==0) && (strcmp(pid, "f009")==0))
                        write_3g_conf(fp, SN_HISENSE_E910, 1, vid, pid);
                else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "14c3")==0))
                        write_3g_conf(fp, SN_Huawei_K5005, 1, vid, pid);
                else if((strcmp(vid, "12d1")==0) && (strcmp(pid, "1c24")==0))
                        write_3g_conf(fp, SN_Huawei_E173_MoiveStar, 1, vid, pid);
                else if((strcmp(vid, "1ee8")==0) && (strcmp(pid, "0060")==0))
                        write_3g_conf(fp, SN_Onda_MSA14_4, 1, vid, pid);
		/*else
			write_3g_conf(fp, UNKNOWNDEV, 1, vid, pid);//*/
		else{
			fclose(fp);
			unlink(conf_file);
			return 0;
		}
	}
	else	/* manaul setting */
	{
usb_dbg("3G: manaul setting.\n");
		nvram_set("d3g", nvram_safe_get("Dev3G"));

		if(strcmp(nvram_safe_get("Dev3G"), "MU-Q101") == 0){					// on list
			write_3g_conf(fp, SN_MU_Q101, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ASUS-T500") == 0 && !strcmp(vid, "0b05")){				// on list
			write_3g_conf(fp, UNKNOWNDEV, 0, vid, pid);
		//} else if (strcmp(nvram_safe_get("Dev3G"), "OPTION-ICON225") == 0){			// on list
		//	write_3g_conf(fp, SN_OPTION_ICON225, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-GlobeSurfer-Icon") == 0){
			write_3g_conf(fp, SN_Option_GlobeSurfer_Icon, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-GlobeSurfer-Icon-7.2") == 0){
			write_3g_conf(fp, SN_Option_GlobeSurfer_Icon72, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-GlobeTrotter-GT-MAX-3.6") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_GT_MAX36, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-GlobeTrotter-GT-MAX-7.2") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_GT_MAX72, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-GlobeTrotter-EXPRESS-7.2") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_EXPRESS72, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-iCON-210") == 0){
			write_3g_conf(fp, SN_Option_iCON210, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-GlobeTrotter-HSUPA-Modem") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_HSUPA_Modem, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Option-iCON-401") == 0){
			write_3g_conf(fp, SN_Option_iCON401, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Vodafone-K3760") == 0){
			write_3g_conf(fp, SN_Vodafone_K3760, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ATT-USBConnect-Quicksilver") == 0){
			write_3g_conf(fp, SN_ATT_USBConnect_Quicksilver, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E169") == 0){			// on list
			write_3g_conf(fp, SN_Huawei_E169, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E180") == 0){			// on list
			//write_3g_conf(fp, SN_Huawei_E180, 0, vid, pid);
			write_3g_conf(fp, SN_Huawei_E220, 1, vid, pid);		// E180:12d1/1003 (as E220)
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E220") == 0){			// on list
			write_3g_conf(fp, SN_Huawei_E220, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E630") == 0){
			write_3g_conf(fp, SN_Huawei_E630, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E270") == 0){
			write_3g_conf(fp, SN_Huawei_E270, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E1550") == 0){
			write_3g_conf(fp, SN_Huawei_E1550, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E161") == 0){
			write_3g_conf(fp, SN_Huawei_E1612, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E1612") == 0){
			write_3g_conf(fp, SN_Huawei_E1612, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E1690") == 0){
			write_3g_conf(fp, SN_Huawei_E1690, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-K3765") == 0){
			write_3g_conf(fp, SN_Huawei_K3765, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-K4505") == 0){
			write_3g_conf(fp, SN_Huawei_K4505, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E372") == 0){ // 20120919: From Lencer's test, must run EC156 to switch.
			write_3g_conf(fp, SN_Huawei_EC156, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Huawei-E173") == 0){
			write_3g_conf(fp, SN_Huawei_E173, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "MTS-AC2746") == 0){ // Couldn't switch with usb_modeswitch.
			fclose(fp);
			unlink(conf_file);
			return 0;
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-MF620") == 0){
			write_3g_conf(fp, SN_ZTE_MF620, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-MF622") == 0){
			write_3g_conf(fp, SN_ZTE_MF622, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-MF628") == 0){
			write_3g_conf(fp, SN_ZTE_MF628, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-MF626") == 0){
			write_3g_conf(fp, SN_ZTE_MF626, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-AC8710") == 0){
			write_3g_conf(fp, SN_ZTE_AC8710, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-AC2710") == 0){
			write_3g_conf(fp, SN_ZTE_AC2710, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-6535-Z") == 0){
			write_3g_conf(fp, SN_ZTE6535_Z, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-K3520-Z") == 0){
			write_3g_conf(fp, SN_ZTE_K3520_Z, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-MF110") == 0){
			write_3g_conf(fp, SN_ZTE_MF110, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ZTE-K3565") == 0){
			write_3g_conf(fp, SN_ZTE_K3565, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ONDA-MT503HS") == 0){
			write_3g_conf(fp, SN_ONDA_MT503HS, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ONDA-MT505UP") == 0){
			write_3g_conf(fp, SN_ONDA_MT505UP, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Novatel-Wireless-Ovation-MC950D-HSUPA") == 0){
			write_3g_conf(fp, SN_Novatel_Wireless_Ovation_MC950D, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Novatel-U727") == 0){
			write_3g_conf(fp, SN_Novatel_U727, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Novatel-MC990D") == 0){
			write_3g_conf(fp, SN_Novatel_MC990D, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Novatel-U760") == 0){
			write_3g_conf(fp, SN_Novatel_U760, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Alcatel-X020") == 0){
			write_3g_conf(fp, SN_Alcatel_X020, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Alcatel-X200") == 0){
			write_3g_conf(fp, SN_Alcatel_X200, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "AnyDATA-ADU-500A") == 0){
			write_3g_conf(fp, SN_AnyDATA_ADU_500A, 0, vid, pid);
		} else if (strncmp(nvram_safe_get("Dev3G"), "BandLuxe-", 9) == 0){			// on list
			fclose(fp);
			unlink(conf_file);
			return 0;
		} else if (strcmp(nvram_safe_get("Dev3G"), "Solomon-S3Gm-660") == 0){
			write_3g_conf(fp, SN_Solomon_S3Gm660, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "C-motechD-50") == 0){
			write_3g_conf(fp, SN_C_motechD50, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "C-motech-CGU-628") == 0){
			write_3g_conf(fp, SN_C_motech_CGU628, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Toshiba-G450") == 0){
			write_3g_conf(fp, SN_Toshiba_G450, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "UTStarcom-UM175") == 0){
			write_3g_conf(fp, SN_UTStarcom_UM175, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Hummer-DTM5731") == 0){
			write_3g_conf(fp, SN_Hummer_DTM5731, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "A-Link-3GU") == 0){
			write_3g_conf(fp, SN_A_Link_3GU, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Sierra-Wireless-Compass-597") == 0){
			write_3g_conf(fp, SN_Sierra_Wireless_Compass597, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Sierra-881U") == 0){
			write_3g_conf(fp, SN_Sierra881U, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Sony-Ericsson-MD400") == 0){
			write_3g_conf(fp, SN_Sony_Ericsson_MD400, 0, vid, pid);
		//} else if (strcmp(nvram_safe_get("Dev3G"), "Sony-Ericsson-W910i") == 0){		// on list
		//	write_3g_conf(fp, UNKNOWNDEV, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "LG-LDU-1900D") == 0){
			write_3g_conf(fp, SN_LG_LDU_1900D, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Samsung-SGH-Z810") == 0){
			write_3g_conf(fp, SN_Samsung_SGH_Z810, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "MobiData-MBD-200HU") == 0){
			write_3g_conf(fp, SN_MobiData_MBD_200HU, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "ST-Mobile") == 0){
			write_3g_conf(fp, SN_ST_Mobile, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "MyWave-SW006") == 0){
			write_3g_conf(fp, SN_MyWave_SW006, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Cricket-A600") == 0){
			write_3g_conf(fp, SN_Cricket_A600, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "EpiValley-SEC-7089") == 0){
			write_3g_conf(fp, SN_EpiValley_SEC7089, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Samsung-U209") == 0){
			write_3g_conf(fp, SN_Samsung_U209, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "D-Link-DWM-162-U5") == 0){
			write_3g_conf(fp, SN_D_Link_DWM162_U5, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Novatel-MC760") == 0){
			write_3g_conf(fp, SN_Novatel_MC760, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Philips-TalkTalk") == 0){
			write_3g_conf(fp, SN_Philips_TalkTalk, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "HuaXing-E600") == 0){
			write_3g_conf(fp, SN_HuaXing_E600, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "C-motech-CHU-629S") == 0){
			write_3g_conf(fp, SN_C_motech_CHU_629S, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Sagem-9520") == 0){
			write_3g_conf(fp, SN_Sagem9520, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Nokia-CS15") == 0){
			write_3g_conf(fp, SN_Nokia_CS15, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Nokia-CS17") == 0){
			write_3g_conf(fp, SN_Nokia_CS17, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Vodafone-MD950") == 0){
			write_3g_conf(fp, SN_Vodafone_MD950, 0, vid, pid);
		} else if (strcmp(nvram_safe_get("Dev3G"), "Siptune-LM-75") == 0){
			write_3g_conf(fp, SN_Siptune_LM75, 0, vid, pid);
                } else if (strcmp(nvram_safe_get("Dev3G"), "Celot_CT-680") == 0){               // 0703 add 
                        write_3g_conf(fp, SN_Celot_CT680, 0, vid, pid);
                } else if (strcmp(nvram_safe_get("Dev3G"), "Celot_CT-K300") == 0){              // 0703 add 
                        write_3g_conf(fp, SN_Celot_K300, 0, vid, pid);
		} else if (strncmp(nvram_safe_get("Dev3G"), "Huawei-", 7) == 0 && !strcmp(vid, "12d1")){
			write_3g_conf(fp, UNKNOWNDEV, 0, vid, pid);
		} else{
			nvram_set("d3g", "usb_3g_dongle"); // the plugged dongle was not the manual-setting one.
			fclose(fp);
			unlink(conf_file);
			return 0;
		}
	}
	fclose(fp);

	return 1;
}

int write_3g_ppp_conf(const char *modem_node){
	FILE *fp;
	char usb_node[8], vid[8], pid[8];
	int wan_unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int retry, lock;

	if(get_device_type_by_device(modem_node) != DEVICE_TYPE_MODEM){
		usb_dbg("(%s): test 1.\n", modem_node);
		return 0;
	}

#ifdef RTCONFIG_DUALWAN
	for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit)
		if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB)
			break;
	if(wan_unit == WAN_UNIT_MAX){
		usb_dbg("(%s): test 2.\n", modem_node);
		return 0;
	}
#else
	wan_unit = WAN_UNIT_SECOND;
#endif

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);

	retry = 0;
	while((lock = file_lock("3g")) == -1 && retry < MAX_WAIT_FILE)
		sleep(1);
	if(lock == -1){
		usb_dbg("(%s): test 3.\n", modem_node);
		return 0;
	}

	unlink(PPP_CONF_FOR_3G);
	if((fp = fopen(PPP_CONF_FOR_3G, "w+")) == NULL){
		eval("mkdir", "-p", PPP_DIR);

		if((fp = fopen(PPP_CONF_FOR_3G, "w+")) == NULL){
			usb_dbg("(%s): test 4.\n", modem_node);
			file_unlock(lock);
			return 0;
		}
	}

	// Get USB node.
	if(get_usb_node_by_device(modem_node, usb_node, 8) == NULL){
		usb_dbg("(%s): test 5.\n", modem_node);
		file_unlock(lock);
		return 0;
	}

	// Get VID.
	if(get_usb_vid(usb_node, vid, 8) == NULL){
		usb_dbg("(%s): test 6.\n", modem_node);
		file_unlock(lock);
		return 0;
	}

	// Get PID.
	if(get_usb_pid(usb_node, pid, 8) == NULL){
		usb_dbg("(%s): test 7.\n", modem_node);
		file_unlock(lock);
		return 0;
	}

	char *modem_enable = nvram_safe_get("modem_enable");
	char *user = nvram_safe_get("modem_user");
	char *pass = nvram_safe_get("modem_pass");
	char *isp = nvram_safe_get("modem_isp");
	char *baud = nvram_safe_get("modem_baud");

	fprintf(fp, "/dev/%s\n", modem_node);
	if(strlen(baud) > 0)
		fprintf(fp, "%s\n", baud);
	if(strlen(user) > 0)
		fprintf(fp, "user %s\n", user);
	if(strlen(pass) > 0)
		fprintf(fp, "password %s\n", pass);
	if(!strcmp(isp, "Virgin")){
		fprintf(fp, "refuse-chap\n");
		fprintf(fp, "refuse-mschap\n");
		fprintf(fp, "refuse-mschap-v2\n");
	}
	else if(!strcmp(isp, "CDMA-UA")){
		fprintf(fp, "refuse-chap\n");
		fprintf(fp, "refuse-mschap\n");
		fprintf(fp, "refuse-mschap-v2\n");
	}
	fprintf(fp, "linkname wan%d\n", wan_unit);
	fprintf(fp, "modem\n");
	fprintf(fp, "crtscts\n");
	fprintf(fp, "noauth\n");
	fprintf(fp, "noipdefault\n");
	fprintf(fp, "usepeerdns\n");
	fprintf(fp, "defaultroute\n");
	fprintf(fp, "nopcomp\n");
	fprintf(fp, "noaccomp\n");
	fprintf(fp, "novj\n");
	fprintf(fp, "nobsdcomp\n");
	fprintf(fp, "nodeflate\n");
	if(!strcmp(nvram_safe_get("modem_demand"), "1")){
		fprintf(fp, "demand\n");
		fprintf(fp, "idle %d\n", nvram_get_int("modem_idletime")?:30);
	}
	fprintf(fp, "persist\n");
	fprintf(fp, "holdoff %s\n", nvram_invmatch(strcat_r(prefix, "pppoe_holdoff", tmp), "")?nvram_safe_get(tmp):"10");
	if(!strcmp(modem_enable, "2")){
		fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/EVDO_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		fprintf(fp, "disconnect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/EVDO_disconn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
	}
	else if(!strcmp(modem_enable, "3")){
		fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/td_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		fprintf(fp, "disconnect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/Generic_disconn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
	}
	else{
		if(!strcmp(vid, "0b05") && !strcmp(pid, "0302")) // T500
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/t500_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		else if(!strcmp(vid, "0421") && !strcmp(pid, "0612")) // CS-15
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/t500_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		else if(!strcmp(vid, "106c") && !strcmp(pid, "3716"))
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/verizon_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		else if(!strcmp(vid, "1410") && !strcmp(pid, "4400"))
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/rogers_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		else
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/Generic_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		
		fprintf(fp, "disconnect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/Generic_disconn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
	}

	fclose(fp);
	file_unlock(lock);
	return 1;
}

#ifdef RTCONFIG_USB_BECEEM
int write_beceem_conf(const char *eth_node){
	FILE *fp;
	int retry, lock;

	retry = 0;
	while((lock = file_lock("3g")) == -1 && retry < MAX_WAIT_FILE)
		sleep(1);
	if(lock == -1){
		usb_dbg("(%s): test 1.\n", eth_node);
		return 0;
	}

	unlink(WIMAX_CONF);
	if((fp = fopen(WIMAX_CONF, "w+")) == NULL){
		usb_dbg("(%s): test 2.\n", eth_node);
		file_unlock(lock);
		return 0;
	}

	char *modem_enable = nvram_safe_get("modem_enable");
	char *isp = nvram_safe_get("modem_isp");
	char *user = nvram_safe_get("modem_user");
	char *pass = nvram_safe_get("modem_pass");
	char *ttlsid = nvram_safe_get("modem_ttlsid");

	if(strcmp(modem_enable, "4") || !strcmp(isp, "")){
		usb_dbg("(%s): test 3.\n", eth_node);
		file_unlock(lock);
		return 0;
	}

	fprintf(fp, "AuthEnabled                       Yes\n");
	fprintf(fp, "CACertPath                        '/lib/firmware'\n");
	fprintf(fp, "NetEntryIPRefreshEnabled          Yes\n");
	fprintf(fp, "IPRefreshCommand                  'udhcpc -i %s -p /var/run/udhcpc%d.pid -s /tmp/udhcpc &'\n", eth_node, get_wan_unit(eth_node));
	fprintf(fp, "FirmwareFileName                  '/lib/firmware/macxvi200.bin'\n");
	fprintf(fp, "ConfigFileName                    '/lib/firmware/macxvi.cfg'\n");
	fprintf(fp, "BandwidthMHz                      10\n");
	fprintf(fp, "NetworkSearchTimeoutSec           10\n");

	if(!strcmp(isp, "Yota")){
		fprintf(fp, "CenterFrequencyMHz                2505 2515 2525 2625\n");
		fprintf(fp, "EAPMethod                         4\n");
		fprintf(fp, "ValidateServerCert                Yes\n");
		fprintf(fp, "CACertFileName                    '/lib/firmware/Server_CA.pem'\n");
		fprintf(fp, "TLSDeviceCertFileName             'DeviceMemSlot3'\n");
		fprintf(fp, "TLSDevicePrivateKeyFileName       'DeviceMemSlot2'\n");
		fprintf(fp, "TLSDevicePrivateKeyPassword       'Motorola'\n");
		fprintf(fp, "TLSDeviceSubCA1CertFileName       'DeviceMemSlot1'\n");
		fprintf(fp, "TLSDeviceSubCA2CertFileName       ''\n");
	}
	else if(!strcmp(isp, "GMC")){
		fprintf(fp, "CenterFrequencyMHz                2600 2610 2620\n");
		fprintf(fp, "EAPMethod                         0\n");
		fprintf(fp, "ValidateServerCert                No\n");
		fprintf(fp, "UserIdentity                      '%s'\n", user);
		fprintf(fp, "UserPassword                      '%s'\n", pass);
		if(strlen(ttlsid) > 0)
			fprintf(fp, "TTLSAnonymousIdentity             '%s'\n", ttlsid);
	}
	else if(!strcmp(isp, "FreshTel")){
		fprintf(fp, "CenterFrequencyMHz                3405 3415 3425 3435 3445 3455 3465 3475 3485 3495 3505 3515 3525 3535 3545 3555 3565 3575 3585 3595 3417 3431 3459 3473 3487 3517 3531 3559 3573 3587\n");
		fprintf(fp, "EAPMethod                         0\n");
		fprintf(fp, "ValidateServerCert                No\n");
		fprintf(fp, "UserIdentity                      '%s'\n", user);
		fprintf(fp, "UserPassword                      '%s'\n", pass);
		if(strlen(ttlsid) > 0)
			fprintf(fp, "TTLSAnonymousIdentity             '%s'\n", ttlsid);
	}
	else if(!strcmp(isp, "Giraffe")){
		fprintf(fp, "CenterFrequencyMHz                2355 2365 2375 2385 2395\n");
		fprintf(fp, "EAPMethod                         0\n");
		fprintf(fp, "ValidateServerCert                No\n");
		fprintf(fp, "UserIdentity                      '%s'\n", user);
		fprintf(fp, "UserPassword                      '%s'\n", pass);
		if(strlen(ttlsid) > 0)
			fprintf(fp, "TTLSAnonymousIdentity             '%s'\n", ttlsid);
	}

	if(!strcmp(nvram_safe_get("modem_debug"), "1")){
		fprintf(fp, "CSCMDebugLogLevel                 4\n");
		fprintf(fp, "CSCMDebugLogFileName              '/lib/firmware/CM_Server_Debug.log'\n");
		fprintf(fp, "CSCMDebugLogFileMaxSizeMB         1\n");
		fprintf(fp, "AuthLogLevel                      4\n");
		fprintf(fp, "AuthLogFileName                   '/lib/firmware/CM_Auth.log'\n");
		fprintf(fp, "AuthLogFileMaxSizeMB              1\n");
		fprintf(fp, "EngineLoggingEnabled              Yes\n");
		fprintf(fp, "EngineLogFileName                 '/lib/firmware/CM_Engine.log'\n");
		fprintf(fp, "EngineLogFileMaxSizeMB            1\n");
	}

	fclose(fp);
	file_unlock(lock);
	return 1;
}

int write_gct_conf(){
	FILE *fp;
	int retry, lock;

	retry = 0;
	while((lock = file_lock("3g")) == -1 && retry < MAX_WAIT_FILE)
		sleep(1);
	if(lock == -1){
		usb_dbg("test 1.\n");
		return 0;
	}

	unlink(WIMAX_CONF);
	if((fp = fopen(WIMAX_CONF, "w+")) == NULL){
		usb_dbg("test 2.\n");
		file_unlock(lock);
		return 0;
	}

	char *modem_enable = nvram_safe_get("modem_enable");
	char *isp = nvram_safe_get("modem_isp");
	char *user = nvram_safe_get("modem_user");
	char *pass = nvram_safe_get("modem_pass");
	char *ttlsid = nvram_safe_get("modem_ttlsid");

	if(strcmp(modem_enable, "4") || !strcmp(isp, "")){
		usb_dbg("test 3.\n");
		file_unlock(lock);
		return 0;
	}

	if(!strcmp(isp, "FreshTel")){
		fprintf(fp, "nspid=50\n");
		fprintf(fp, "use_pkm=1\n");
		fprintf(fp, "eap_type=5\n");
		fprintf(fp, "use_nv=0\n");
		fprintf(fp, "identity=\"%s\"\n", user);
		fprintf(fp, "password=\"%s\"\n", pass);
		if(strlen(ttlsid) > 0)
			fprintf(fp, "anonymous_identity=\"%s\"\n", ttlsid);
		fprintf(fp, "ca_cert_null=1\n");
		fprintf(fp, "dev_cert_null=1\n");
		fprintf(fp, "cert_nv=0\n");
	}
	else if(!strcmp(isp, "Yes")){
		fprintf(fp, "nspid=0\n");
		fprintf(fp, "use_pkm=1\n");
		fprintf(fp, "eap_type=5\n");
		fprintf(fp, "use_nv=0\n");
		fprintf(fp, "identity=\"%s\"\n", user);
		fprintf(fp, "password=\"%s\"\n", pass);
		if(strlen(ttlsid) > 0)
			fprintf(fp, "anonymous_identity=\"%s\"\n", ttlsid);
		fprintf(fp, "ca_cert_null=1\n");
		fprintf(fp, "dev_cert_null=1\n");
		fprintf(fp, "cert_nv=0\n");
	}

	if(!strcmp(nvram_safe_get("modem_debug"), "1")){
		fprintf(fp, "wimax_verbose_level=1\n");
		fprintf(fp, "wpa_debug_level=3\n");
		fprintf(fp, "log_file=\"%s\"\n", WIMAX_LOG);
	}
	else
		fprintf(fp, "log_file=\"\"\n");
	fprintf(fp, "event_script=\"\"\n");

	fclose(fp);
	file_unlock(lock);
	return 1;
}
#endif
#endif // RTCONFIG_USB_MODEM

#ifdef RTCONFIG_USB
// 201102. James. Move the Jiahao's code from rc/service_ex.c. {
int
check_dev_sb_block_count(const char *dev_sd)
{
	FILE *procpt;
	char line[256], ptname[32];
	int ma, mi, sz;

	if ((procpt = fopen_or_warn("/proc/partitions", "r")) != NULL)
	{
		while (fgets(line, sizeof(line), procpt))
		{
			if (sscanf(line, " %d %d %d %[^\n ]", &ma, &mi, &sz, ptname) != 4)
				continue;

			if (!strcmp(dev_sd, ptname) && (sz > 1) )
			{
				fclose(procpt);
				return 1;
			}
		}

		fclose(procpt);
	}

	return 0;
}
// 201102. James. Move the Jiahao's code from rc/service_ex.c. }
#endif // RTCONFIG_USB

// 1: add, 0: remove.
int check_hotplug_action(const char *action){
	if(!strcmp(action, "remove"))
		return 0;
	else
		return 1;
}

int set_usb_common_nvram(const char *action, const char *usb_node, const char *known_type){
	char nvram_name[32], usb_port[8];
	char type[16], vid[8], pid[8], manufacturer[256], product[256], serial[256];
	char been_type[16];
	int partition_order;
	int port_num;

	// Get USB port.
	if(get_usb_port_by_string(usb_node, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("(%s): Fail to get usb port.\n", usb_node);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		return 0;
	}

	if(!check_hotplug_action(action)){
		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d", port_num);
		nvram_set(nvram_name, "");

		// Jerry5Chang added for unmount case. 2012.12.03
                memset(nvram_name, 0, 32);
                sprintf(nvram_name, "usb_path%d_removed", port_num);
                nvram_set(nvram_name, "0");

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_vid", port_num);
		nvram_set(nvram_name, "");

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_pid", port_num);
		nvram_set(nvram_name, "");

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_manufacturer", port_num);
		nvram_set(nvram_name, "");

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_product", port_num);
		nvram_set(nvram_name, "");

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_serial", port_num);
		nvram_set(nvram_name, "");

		partition_order = 0;
		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_fs_path%d", port_num, partition_order);
		if(strlen(nvram_safe_get(nvram_name)) > 0){
			nvram_unset(nvram_name);

#if 0
			for(partition_order = 1; partition_order < 16 ; ++partition_order){
				memset(nvram_name, 0, 32);
				sprintf(nvram_name, "usb_path%d_fs_path%d", port_num, partition_order);
				nvram_unset(nvram_name);
			}
#endif
		}
	}
	else{
		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d", port_num);
		memset(been_type, 0, 16);
		strcpy(been_type, nvram_safe_get(nvram_name));
		if(strlen(been_type) > 0){
#ifdef RTCONFIG_USB_PRINTER
			if(!strcmp(been_type, "printer")){ // Top priority
				return 0;
			}
			else
#endif
#ifdef RTCONFIG_USB_MODEM
			if(!strcmp(been_type, "modem")){ // 2nd priority
#ifdef RTCONFIG_USB_PRINTER
				if(strcmp(known_type, "printer"))
#endif
					return 0;
			}
			else
#endif
#ifdef RTCONFIG_USB
			if(!strcmp(been_type, "storage")){
#if defined(RTCONFIG_USB_PRINTER) || defined(RTCONFIG_USB_MODEM)
				if(
#ifdef RTCONFIG_USB_PRINTER
						strcmp(known_type, "printer")
#else
						1
#endif
					 	&&
#ifdef RTCONFIG_USB_MODEM
						strcmp(known_type, "modem")
#else
						1
#endif
						)
#endif
					return 0;
			}
			else
#endif // RTCONFIG_USB
			{ // unknown device.
				return 0;
			}
		}
		if(known_type != NULL)
			nvram_set(nvram_name, known_type);
		else if(get_device_type_by_node(usb_node, type, 16) != NULL)
			nvram_set(nvram_name, type);
		else // unknown device.
			return 0;

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_vid", port_num);
		if(get_usb_vid(usb_node, vid, 8) == NULL)
			nvram_set(nvram_name, "");
		else
			nvram_set(nvram_name, vid);

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_pid", port_num);
		if(get_usb_pid(usb_node, pid, 8) == NULL)
			nvram_set(nvram_name, "");
		else
			nvram_set(nvram_name, pid);

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_manufacturer", port_num);
		if(get_usb_manufacturer(usb_node, manufacturer, 256) == NULL)
			nvram_set(nvram_name, "");
		else
			nvram_set(nvram_name, manufacturer);

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_product", port_num);
		if(get_usb_product(usb_node, product, 256) == NULL)
			nvram_set(nvram_name, "");
		else
			nvram_set(nvram_name, product);

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_serial", port_num);
		if(get_usb_serial(usb_node, serial, 256) == NULL)
			nvram_set(nvram_name, "");
		else
			nvram_set(nvram_name, serial);
	}

	return 0;
}

int asus_sd(const char *device_name, const char *action){
#ifdef RTCONFIG_USB
	char usb_port[8], usb_node[8], vid[8];
	int retry, isLock;
	char nvram_name[32], nvram_value[32]; // 201102. James. Move the Jiahao's code from ~/drivers/usb/storage.
	int partition_order;
	int port_num;
	char env_dev[64], env_port[64];
	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_sd"), "1")){
		usb_dbg("(%s): stop_sd be set.\n", device_name);
		return 0;
	}

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_DISK){
		usb_dbg("(%s): The device is not a sd device.\n", device_name);
		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	// If remove the device?
	if(!check_hotplug_action(action)){
		memset(usb_port, 0, sizeof(usb_port));
		if(!strcmp(nvram_safe_get("usb_path1_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_1);

			nvram_set("usb_path1_act", "");
		}
		else if(!strcmp(nvram_safe_get("usb_path2_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_2);

			nvram_set("usb_path2_act", "");
		}

		if(strlen(usb_port) > 0)
			usb_dbg("(%s): Remove Storage on USB Port %s.\n", device_name, usb_port);
		else
			usb_dbg("(%s): Remove a unknown-port Storage.\n", device_name);

		putenv("INTERFACE=8/0/0");
		putenv("ACTION=remove");
		putenv("PRODUCT=asus_sd");
		memset(env_dev, 0, sizeof(env_dev));
		sprintf(env_dev, "DEVICENAME=%s", device_name);
		putenv(env_dev);
		putenv("SUBSYSTEM=block");
		putenv("MAJOR=8");
		putenv("PHYSDEVBUS=scsi");

		eval("hotplug", "block");

		unsetenv("INTERFACE");
		unsetenv("ACTION");
		unsetenv("PRODUCT");
		unsetenv("DEVICENAME");
		unsetenv("SUBSYSTEM");
		unsetenv("MAJOR");
		unsetenv("PHYSDEVBUS");

		file_unlock(isLock);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_device(device_name, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("Fail to get usb port: %s.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 8) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// Get VID.
	if(get_usb_vid(usb_node, vid, 8) == NULL){
		usb_dbg("(%s): Fail to get VID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

#ifdef RTCONFIG_USB_PRINTER
	// Wait if there is the printer interface.
	retry = 0;
	while(!hadPrinterModule() && retry < MAX_WAIT_PRINTER_MODULE){
		++retry;
		sleep(1); // Wait the printer module to be ready.
	}
#ifdef REMOVE
	// When the dongle was plugged off during changing the mode, this delay will confuse the procedure.
	// TODO: Find the strange printers which need this delay. Will let them be the special case.
	sleep(1); // Wait the printer interface to be ready.
#endif

	if(hadPrinterInterface(usb_node)){
		usb_dbg("(%s): Had Printer interface on Port %s.\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}
#endif

	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d", port_num);
	memset(nvram_value, 0, 32);
	strcpy(nvram_value, nvram_safe_get(nvram_name));
	if(strcmp(nvram_value, "") && strcmp(nvram_value, "storage")){
		usb_dbg("(%s): Had other interfaces(%s) on Port %s.\n", device_name, nvram_value, usb_port);
		file_unlock(isLock);
		return 0;
	}

// 201102. James. Move the Jiahao's code from ~/drivers/usb/storage. {
	// set USB common nvram.
	set_usb_common_nvram(action, usb_node, "storage");
// 201102. James. Move the Jiahao's code from ~/drivers/usb/storage. }

// 201102. James. Move the Jiahao's code from mdev. {
	if(strlen(device_name) == 3){ // sda, sdb, sdc...
		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_fs_path0", port_num);
		if(strlen(nvram_safe_get(nvram_name)) <= 0)
			nvram_set(nvram_name, device_name);

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d", port_num);
		if(!strcmp(nvram_safe_get(nvram_name), "storage")){
			memset(nvram_name, 0, 32);
			sprintf(nvram_name, "usb_path%d_act", port_num);
			nvram_set(nvram_name, device_name);
		}
	}
	else if(check_dev_sb_block_count(device_name)){
		partition_order = atoi(device_name+3);
		if((partition_order-1) == 0){ // for ATE test.
			sprintf(nvram_name, "usb_path%d_fs_path%d", port_num, partition_order-1);
			nvram_set(nvram_name, device_name);
		}
	}

	kill_pidfile_s("/var/run/usbled.pid", SIGUSR1);	// inform usbled to start blinking USB LED

	putenv("INTERFACE=8/0/0");
	putenv("ACTION=add");
	putenv("PRODUCT=asus_sd");
	memset(env_dev, 0, sizeof(env_dev));
	sprintf(env_dev, "DEVICENAME=%s", device_name);
	putenv(env_dev);
	putenv("SUBSYSTEM=block");
	memset(env_port, 0, sizeof(env_port));
	sprintf(env_port, "USBPORT=%s", usb_port);
	putenv(env_port);

	eval("hotplug", "block");

	kill_pidfile_s("/var/run/usbled.pid", SIGUSR2); // inform usbled to stop blinking USB LED

	unsetenv("INTERFACE");
	unsetenv("ACTION");
	unsetenv("PRODUCT");
	unsetenv("DEVICENAME");
	unsetenv("SUBSYSTEM");
	unsetenv("USBPORT");

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
	return 1;

#endif // RTCONFIG_USB
	return 0;
}

int asus_lp(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_PRINTER
	char usb_port[8], usb_node[8];
	int port_num, u2ec_fifo;
	int isLock;
	char nvram_name[32];
	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_lp"), "1")){
		usb_dbg("(%s): stop_lp be set.\n", device_name);
		return 0;
	}

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_PRINTER){
		usb_dbg("(%s): The device is not a printer.\n", device_name);
		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	// If remove the device?
	if(!check_hotplug_action(action)){
		memset(usb_port, 0, sizeof(usb_port));
		if(!strcmp(nvram_safe_get("usb_path1_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_1);

			nvram_set("usb_path1_act", "");
		}
		else if(!strcmp(nvram_safe_get("usb_path2_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_2);

			nvram_set("usb_path2_act", "");
		}

		if(strlen(usb_port) > 0){
			u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
			write(u2ec_fifo, "r", 1);
			close(u2ec_fifo);

			usb_dbg("(%s): Remove Printer on USB Port %s.\n", device_name, usb_port);
		}
		else
			usb_dbg("(%s): Remove a unknown-port Printer.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_device(device_name, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("Fail to get usb port: %s.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 8) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// set USB common nvram.
	set_usb_common_nvram(action, usb_node, "printer");

	// check the current working node of modem.
	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d_act", port_num);
	nvram_set(nvram_name, device_name);

	// Don't support the second printer device on a DUT.
	// Only see the other usb port.
	if((port_num == 1 && !strcmp(nvram_safe_get("usb_path2"), "printer"))
			|| (port_num == 2 && !strcmp(nvram_safe_get("usb_path1"), "printer"))
			){
		// We would show the second printer device but didn't let it work.
		// Because it didn't set the nvram: usb_path%d_act.
		usb_dbg("(%s): Already had the printer device in the other USB port!\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
	write(u2ec_fifo, "a", 1);
	close(u2ec_fifo);

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_PRINTER
	return 1;
}

int asus_sg(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_MODEM
	char usb_port[8], usb_node[8], vid[8], pid[8];
	int isLock;
	char eject_cmd[32];
	int port_num;
	char nvram_name[32], nvram_value[32];
	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_sg"), "1")){
		usb_dbg("(%s): stop_sg be set.\n", device_name);
		return 0;
	}

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_SG){
		usb_dbg("(%s): The device is not a sg one.\n", device_name);
		return 0;
	}

	if(hadSerialModule() || hadACMModule()){
		usb_dbg("(%s): Already had a running modem.\n", device_name);
		return 0;
	}

	// If remove the device?
	if(!check_hotplug_action(action)){
		usb_dbg("(%s): Remove sg device.\n", device_name);
		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_device(device_name, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("Fail to get usb port: %s.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 8) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d", port_num);
	memset(nvram_value, 0, 32);
	strcpy(nvram_value, nvram_safe_get(nvram_name));
	//if(!strcmp(nvram_value, "printer") || !strcmp(nvram_value, "modem")){
	if(strcmp(nvram_value, "")){
		usb_dbg("(%s): Already there was a other interface(%s).\n", usb_port, nvram_value);
		file_unlock(isLock);
		return 0;
	}

	// Get VID.
	if(get_usb_vid(usb_node, vid, 8) == NULL){
		usb_dbg("(%s): Fail to get VID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Get PID.
	if(get_usb_pid(usb_node, pid, 8) == NULL){
		usb_dbg("(%s): Fail to get PID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// initial the config file of usb_modeswitch.
	/*if(!strcmp(nvram_safe_get("Dev3G"), "AUTO")
			&& (!strcmp(vid, "19d2") || !strcmp(vid, "1a8d"))
			){
		modprobe("cdrom");
		modprobe("sr_mod");
		sleep(1); // wait the module be ready.
	}
	else//*/
#ifdef RTCONFIG_USB_BECEEM
	// Could use usb_modeswitch to switch the Beceem's dongles, besides ZTE's.
	if(!strcmp(nvram_safe_get("beceem_switch"), "1")
			&& is_beceem_dongle(0, vid, pid)){
		usb_dbg("(%s): Running switchmode...\n", device_name);
		xstart("switchmode");
	}
	else
	if(is_gct_dongle(0, vid, pid)){
		if(strcmp(nvram_safe_get("stop_sg_remove"), "1")){
			usb_dbg("(%s): Running gctwimax -D...\n", device_name);
			xstart("gctwimax", "-D");
		}
	}
	else
#endif
	if(!strcmp(vid, "1199"))
		; // had do usb_modeswitch before.
	else if(init_3g_param(vid, pid, port_num)){
		memset(eject_cmd, 0, 32);
		sprintf(eject_cmd, "%s.%d", USB_MODESWITCH_CONF, port_num);

		if(strcmp(nvram_safe_get("stop_sg_remove"), "1")){
			usb_dbg("(%s): Running usb_modeswitch...\n", device_name);
			xstart("usb_modeswitch", "-c", eject_cmd);
		}
	}
	else if(port_num != 3 // usb_path3 is worked for the built-in card reader.
			&& !is_create_file_dongle(vid, pid)
			){
		usb_dbg("(%s): insmod cdrom!\n", device_name);
		modprobe("cdrom");
		modprobe("sr_mod");
		sleep(1); // wait the module be ready.
	}

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_MODEM
	return 1;
}

int asus_sr(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_MODEM
	char usb_port[8], usb_node[8], vid[8], pid[8];
	int isLock;
	char eject_cmd[32];
	int port_num;
	char nvram_name[32], nvram_value[32];
	char sg_device[32];
	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_cd"), "1")){
		usb_dbg("(%s): stop_cd be set.\n", device_name);
		return 0;
	}

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_CD){
		usb_dbg("(%s): The device is not a CD one.\n", device_name);
		return 0;
	}

	// If remove the device?
	if(!check_hotplug_action(action)){
		usb_dbg("(%s): Remove CD device.\n", device_name);

		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_device(device_name, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("Fail to get usb port: %s.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d", port_num);
	memset(nvram_value, 0, 32);
	strcpy(nvram_value, nvram_safe_get(nvram_name));
	if(!strcmp(nvram_value, "printer") || !strcmp(nvram_value, "modem")){
		usb_dbg("(%s): Already there was a other interface(%s).\n", usb_port, nvram_value);
		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 8) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// Get VID.
	if(get_usb_vid(usb_node, vid, 8) == NULL){
		usb_dbg("(%s): Fail to get VID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Get PID.
	if(get_usb_pid(usb_node, pid, 8) == NULL){
		usb_dbg("(%s): Fail to get PID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

#ifdef RTCONFIG_USB_BECEEM
	if(is_gct_dongle(0, vid, pid)){
		if(strcmp(nvram_safe_get("stop_cd_remove"), "1")){
			usb_dbg("(%s): Running gctwimax -D...\n", device_name);
			xstart("gctwimax", "-D");
		}
	}
	else
#endif
	if(strcmp(nvram_safe_get("stop_cd_remove"), "1")){
		usb_dbg("(%s): Running sdparm...\n", device_name);

		memset(eject_cmd, 0, 32);
		sprintf(eject_cmd, "/dev/%s", device_name);
		eval("sdparm", "--command=eject", eject_cmd);
		sleep(1);

		if(find_sg_of_device(device_name, sg_device, sizeof(sg_device)) != NULL){
			memset(eject_cmd, 0, 32);
			sprintf(eject_cmd, "/dev/%s", sg_device);
			eval("sdparm", "--command=eject", eject_cmd);
			sleep(1);
		}

		modprobe_r("sr_mod");
		modprobe_r("cdrom");
	}

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_MODEM
	return 1;
}

int asus_tty(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_MODEM
	char *ptr, usb_port[8], usb_node[8], vid[8], pid[8], interface_name[16];
	int port_num = 0, got_Int_endpoint = 0;
	int isLock;
	char nvram_name[32], current_value[16];
	int cur_val, tmp_val;
	char pid_file[256], *value;
	int pppd_pid;
	int retry;
#ifndef RTCONFIG_USB_MODEM_PIN
	char cmd[32];
#endif
	int wan_unit;
	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_modem"), "1")){
		usb_dbg("(%s): stop_modem be set.\n", device_name);
		return 0;
	}

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_MODEM){
		usb_dbg("(%s): The device is not a Modem node.\n", device_name);
		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	// If remove the device?
	if(!check_hotplug_action(action)){
		memset(usb_port, 0, sizeof(usb_port));
		if(!strcmp(nvram_safe_get("usb_path1_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_1);
			port_num = 1;

			nvram_set("usb_path1_act", "");
			nvram_set("usb_path1_act_def", "");
		}
		else if(!strcmp(nvram_safe_get("usb_path2_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_2);
			port_num = 2;

			nvram_set("usb_path2_act", "");
			nvram_set("usb_path2_act_def", "");
		}

		if(strlen(usb_port) > 0){
			// Modem remove action.
			unlink(PPP_CONF_FOR_3G);

#ifdef RTCONFIG_DUALWAN
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit)
				if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB)
					break;
			if(wan_unit == WAN_UNIT_MAX){
				usb_dbg("(%s): in the current dual wan mode, didn't support the USB modem.\n", device_name);
				file_unlock(isLock);
				return 0;
			}
#else
			wan_unit = WAN_UNIT_SECOND;
#endif

			memset(pid_file, 0, 256);
			snprintf(pid_file, 256, "/var/run/ppp-wan%d.pid", wan_unit);

			retry = 0;
			if((value = file2str(pid_file)) != NULL && (pppd_pid = atoi(value)) > 1){
usb_dbg("%s: kill pppd(%d).\n", __FUNCTION__, pppd_pid);
				kill(pppd_pid, SIGHUP);
				sleep(1);
				while(check_process_exist(pppd_pid) && retry < MAX_WAIT_FILE){
usb_dbg("%s: kill pppd(%d).\n", __FUNCTION__, pppd_pid);
					++retry;
					kill(pppd_pid, SIGTERM);
					sleep(1);
				}

				if(check_process_exist(pppd_pid)){
					kill(pppd_pid, SIGKILL);
					sleep(1);
				}
			}
			if(value != NULL)
				free(value);
			killall_tk("usb_modeswitch");
			killall_tk("sdparm");

			retry = 0;
			while(hadOptionModule() && retry < 3){
				usb_dbg("(%s): Remove the option module.\n", device_name);
				modprobe_r("option");
				++retry;
			}

			retry = 0;
			while(hadSerialModule() && retry < 3){
				usb_dbg("(%s): Remove the serial module.\n", device_name);
				modprobe_r("usbserial");
				++retry;
			}

			retry = 0;
			while(hadACMModule() && retry < 3){
				usb_dbg("(%s): Remove the acm module.\n", device_name);
				modprobe_r("cdc-acm");
				++retry;
			}

			// Notify wanduck to switch the wan line to WAN port.
			kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);

			usb_dbg("(%s): Remove the modem node on USB port %s.\n", device_name, usb_port);
		}
		else
			usb_dbg("(%s): Remove a unknown-port Modem.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_device(device_name, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("Fail to get usb port: %s.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 8) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// Get VID.
	if(get_usb_vid(usb_node, vid, 8) == NULL){
		usb_dbg("(%s): Fail to get VID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Get PID.
	if(get_usb_pid(usb_node, pid, 8) == NULL){
		usb_dbg("(%s): Fail to get PID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Don't support the second modem device on a DUT.
	// Only see the other usb port, because in the same port there are more modem interfaces and they need to compare.
	if((port_num == 1 && !strcmp(nvram_safe_get("usb_path2"), "modem"))
			|| (port_num == 2 && !strcmp(nvram_safe_get("usb_path1"), "modem"))
			){
		// We would show the second modem device but didn't let it work.
		// Because it didn't set the nvram: usb_path%d_act.
		usb_dbg("(%s): Already had the modem device in the other USB port!\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// Find the control node of modem.
	// Get Interface name.
	if(get_interface_by_device(device_name, interface_name, sizeof(interface_name)) == NULL){
		usb_dbg("Fail to get usb port: %s.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	char nvram_def[32], current_def[16];

	// check the current working node of modem.
	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d_act", port_num);
	memset(current_value, 0, 16);
	strcpy(current_value, nvram_safe_get(nvram_name));
	memset(nvram_def, 0, 32);
	sprintf(nvram_def, "usb_path%d_act_def", port_num);
	memset(current_def, 0, 16);
	strcpy(current_def, nvram_safe_get(nvram_def));

	if(isSerialNode(device_name)){
		// Find the endpoint: 03(Int).
		usb_dbg("(%s): interface_name=%s.\n", device_name, interface_name);
		got_Int_endpoint = get_interface_Int_endpoint(interface_name);
		if(!got_Int_endpoint){
			// Set the default node be ttyUSB0, because there is no Int endpoint in some dongles.
			if(!strcmp(device_name, "ttyUSB0") && !strcmp(current_value, "")){
				usb_dbg("(%s): set_default_node!\n", device_name);
				nvram_set(nvram_def, "1");
				memset(current_def, 0, 16);
				strcpy(current_def, "1");
			}
			else{
				usb_dbg("(%s): No Int endpoint!\n", device_name);
				file_unlock(isLock);
				return 0;
			}
		}

		if(!strcmp(vid, "0f3d") && !strcmp(pid, "68aa")){
			if(!strcmp(device_name, "ttyUSB3"))
				nvram_set(nvram_name, device_name);
			else{
				file_unlock(isLock);
				return 0;
			}
		}
		else if(!strcmp(current_value, "")){
			nvram_set(nvram_name, device_name);
		}
		else{
			errno = 0;
			cur_val = strtol(&current_value[6], &ptr, 10);
			if(errno != 0 || &current_value[6] == ptr){
				cur_val = -1;
			}

			errno = 0;
			tmp_val = strtol(&device_name[6], &ptr, 10);
			if(errno != 0 || &device_name[6] == ptr){
				usb_dbg("(%s): Can't get the working node.\n", device_name);
				file_unlock(isLock);
				return 0;
			}
			else if(cur_val != -1 && cur_val <= tmp_val && strcmp(current_def, "1")){ // get the smaller node safely.
usb_dbg("(%s): cur_val=%d, tmp_val=%d, set_default_node=%s.\n", device_name, cur_val, tmp_val, current_def);
				usb_dbg("(%s): Skip to write PPP's conf.\n", device_name);
				file_unlock(isLock);
				return 0;
			}
			else{
usb_dbg("(%s): current_value=%s, device_name=%s.\n", device_name, current_value, device_name);
usb_dbg("(%s): cur_val=%d, tmp_val=%d.\n", device_name, cur_val, tmp_val);
				nvram_set(nvram_name, device_name);
				nvram_set(nvram_def, "");
				memset(current_def, 0, 16);
			}
		}

		// Write dial config file.
		if(!write_3g_ppp_conf(device_name)){
			usb_dbg("(%s): Fail to write PPP's conf for 3G process!\n", device_name);
			file_unlock(isLock);
			return 0;
		}
	}
	else{ // isACMNode(device_name).
		// Find the control interface of cdc-acm.
		if(!isACMInterface(interface_name)){
			usb_dbg("(%s): Not control interface!\n", device_name);
			file_unlock(isLock);
			return 0;
		}

		if(!strcmp(device_name, "ttyACM0")){
			nvram_set(nvram_name, device_name);

			// Write dial config file.
			if(!write_3g_ppp_conf(device_name)){
				usb_dbg("(%s): Fail to write PPP's conf for 3G process!\n", device_name);
				file_unlock(isLock);
				return 0;
			}
		}
		else{
			usb_dbg("(%s): Skip to write PPP's conf.\n", device_name);
			file_unlock(isLock);
			return 0;
		}
	}

#ifdef RTCONFIG_DUALWAN
	// If the node is the default node without the Int endpoint, skip to autorun restart_wan_if.
	if(!nvram_match("wans_mode", "off") && strcmp(current_def, "1")){
		for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit)
			if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB)
				break;

		if(wan_unit != WAN_UNIT_MAX){
#else
			wan_unit = WAN_UNIT_SECOND;
#endif

#ifndef RTCONFIG_USB_MODEM_PIN
			// If PIN enable, start_wan() here is too soon to work the modem.
usb_dbg("(%s): got tty nodes and notify restart wan(%d)...\n", device_name, wan_unit);
			memset(cmd, 0, 32);
			sprintf(cmd, "restart_wan_if %d", wan_unit);
			notify_rc_and_wait(cmd);
#endif

			// show the manual-setting dongle in Networkmap when it was plugged after reboot.
			init_3g_param(vid, pid, port_num);
#ifdef RTCONFIG_DUALWAN
		}
		else
usb_dbg("Didn't support the USB connection now...\n");
	}
#endif

	// Notify wanduck that DUT can switch the wan line to the USB Modem.
	kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_MODEM
	return 1;
}

// Beceem's driver: drxvi314 identifies only one dongle.
int asus_usbbcm(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_BECEEM
	char usb_port[8];
	int port_num = 0;
	int isLock;
	char nvram_name[32];
	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_modem"), "1")){
		usb_dbg("(%s): stop_modem be set.\n", device_name);
		return 0;
	}

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_BECEEM){
		usb_dbg("(%s): The device is not a Modem node.\n", device_name);
		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	// If remove the device?
	if(!check_hotplug_action(action)){
		memset(usb_port, 0, sizeof(usb_port));
		if(!strcmp(nvram_safe_get("usb_path1_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_1);
			port_num = 1;

			nvram_set("usb_path1_act", "");
		}
		else if(!strcmp(nvram_safe_get("usb_path2_act"), device_name)){
			strcpy(usb_port, USB_EHCI_PORT_2);
			port_num = 2;

			nvram_set("usb_path2_act", "");
		}

		if(strlen(usb_port) > 0){
			// Modem remove action.
			unlink(WIMAX_CONF);

			killall_tk("usb_modeswitch");

			usb_dbg("(%s): Remove the Beceem node on USB port %s.\n", device_name, usb_port);
		}
		else
			usb_dbg("(%s): Remove a unknown-port Modem.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_device(device_name, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("Fail to get usb port: %s.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	// Don't support the second modem device on a DUT.
	// Only see the other usb port, because in the same port there are more modem interfaces and they need to compare.
	if((port_num == 1 && !strcmp(nvram_safe_get("usb_path2"), "modem"))
			|| (port_num == 2 && !strcmp(nvram_safe_get("usb_path1"), "modem"))
			){
		// We would show the second modem device but didn't let it work.
		// Because it didn't set the nvram: usb_path%d_act.
		usb_dbg("(%s): Already had the modem device in the other USB port!\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// check the current working node of modem.
	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d_act", port_num);

	nvram_set(nvram_name, device_name);

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_BECEEM
	return 1;
}

int asus_usb_interface(const char *device_name, const char *action){
#if defined(RTCONFIG_USB) || defined(RTCONFIG_USB_PRINTER) || defined(RTCONFIG_USB_MODEM)
	char usb_port[8], usb_node[8];
	int port_num;
#ifdef RTCONFIG_USB_MODEM
	char vid[8], pid[8];
	char modem_cmd[64], buf[64];
#endif
	int retry, isLock;
	char device_type[16];
	char nvram_usb_path[16];
	char conf_file[32];
	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_ui"), "1")){
		usb_dbg("(%s): stop_ui be set.\n", device_name);
		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_string(device_name, usb_port, sizeof(usb_port)) == NULL){
		usb_dbg("(%s): Fail to get usb port.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("usb_port(%s) is not valid.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_string(device_name, usb_node, 8) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// If remove the device? Handle the remove hotplug of the printer and modem.
	if(!check_hotplug_action(action)){
		memset(nvram_usb_path, 0, 16);
		sprintf(nvram_usb_path, "usb_path%d", port_num);

#ifdef RTCONFIG_USB_PRINTER
		if(!strcmp(nvram_safe_get(nvram_usb_path), "printer"))
			strcpy(device_type, "printer");
		else
#endif
#ifdef RTCONFIG_USB_MODEM
		if(!strcmp(nvram_safe_get(nvram_usb_path), "modem")){
			strcpy(device_type, "modem");

			// remove the device between adding interface and adding tty.
			modprobe_r("option");
			modprobe_r("usbserial");
			modprobe_r("cdc-acm");
#ifdef RTCONFIG_USB_BECEEM
			modprobe_r("drxvi314");
#endif

			memset(conf_file, 0, 32);
			sprintf(conf_file, "%s.%d", USB_MODESWITCH_CONF, port_num);
			unlink(conf_file);

			memset(nvram_usb_path, 0, 16);
			sprintf(nvram_usb_path, "usb_path%d_act", port_num);

			// When ACM dongles are removed, there are no removed hotplugs of ttyACM nodes.
			if(!strncmp(nvram_safe_get(nvram_usb_path), "ttyACM", 6)){
				// No methods let DUT restore the normal state after removing the ACM dongle.
				notify_rc("reboot");
			}
		}
		else
#endif
#ifdef RTCONFIG_USB
		if(!strcmp(nvram_safe_get(nvram_usb_path), "storage"))
			strcpy(device_type, "storage");
		else
#endif
			strcpy(device_type, "");

		if(strlen(device_type) > 0){
			// Remove USB common nvram.
			set_usb_common_nvram(action, usb_node, NULL);

			usb_dbg("(%s): Remove %s interface on USB Port %s.\n", device_name, device_type, usb_port);
		}
		else
			usb_dbg("(%s): Remove a unknown-type interface.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

#ifdef RTCONFIG_USB_MODEM
	// Get VID.
	if(get_usb_vid(usb_node, vid, 8) == NULL){
		usb_dbg("(%s): Fail to get VID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Get PID.
	if(get_usb_pid(usb_node, pid, 8) == NULL){
		usb_dbg("(%s): Fail to get PID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// there is no any bounded drivers with Some Sierra dongles in the default state.
	if(!strcmp(vid, "1199") && isStorageInterface(device_name)){
		if(init_3g_param(vid, pid, port_num)){
			memset(modem_cmd, 0, 32);
			sprintf(modem_cmd, "%s.%d", USB_MODESWITCH_CONF, port_num);

			if(strcmp(nvram_safe_get("stop_ui_remove"), "1")){
				usb_dbg("(%s): Running usb_modeswitch...\n", device_name);
				xstart("usb_modeswitch", "-c", modem_cmd);
			}

			file_unlock(isLock);
			return 0;
		}
	}

	if(!isSerialInterface(device_name)
			&& !isACMInterface(device_name)
			&& !isRNDISInterface(device_name)
#ifdef RTCONFIG_USB_BECEEM
			&& !isGCTInterface(device_name)
#endif
			){
		usb_dbg("(%s): Not modem interface.\n", device_name);
		file_unlock(isLock);
		return 0;
	}
#endif

#ifdef RTCONFIG_USB_PRINTER
	// Wait if there is the printer interface.
	retry = 0;
	while(!hadPrinterModule() && retry < MAX_WAIT_PRINTER_MODULE){
		++retry;
		sleep(1); // Wait the printer module to be ready.
	}
	sleep(1); // Wait the printer interface to be ready.

	if(hadPrinterInterface(usb_node)){
		usb_dbg("(%s): Had Printer interface on Port %s.\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}
#endif

#ifdef RTCONFIG_USB_MODEM
	// set USB common nvram.
	set_usb_common_nvram(action, usb_node, "modem");

	// Don't support the second modem device on a DUT.
	if(hadSerialModule() || hadACMModule() || hadRNDISModule()
#ifdef RTCONFIG_USB_BECEEM
			|| hadBeceemModule()
			|| hadGCTModule()
#endif
			){
		usb_dbg("(%s): Had inserted the modem module.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	// Modem add action.
	// WiMAX
#ifdef RTCONFIG_USB_BECEEM
	if(is_beceem_dongle(1, vid, pid)){
		usb_dbg("(%s): Runing Beceem module...\n", device_name);

		char *isp = nvram_safe_get("modem_isp");

		unlink("/tmp/Beceem_firmware/macxvi.cfg");
		if(!strcmp(isp, "Yota")){
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi200.bin.normal", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi.cfg.yota", "/tmp/Beceem_firmware/macxvi.cfg");
		}
		else if(!strcmp(isp, "GMC")){
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi200.bin.normal", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi.cfg.gmc", "/tmp/Beceem_firmware/macxvi.cfg");
		}
		else if(!strcmp(isp, "FreshTel")){
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi200.bin.normal", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi.cfg.freshtel", "/tmp/Beceem_firmware/macxvi.cfg");
		}
		else if(!strcmp(isp, "Giraffe")){
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi200.bin.giraffe", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/tmp/Beceem_firmware/macxvi.cfg.giraffe", "/tmp/Beceem_firmware/macxvi.cfg");
		}
		else{
			usb_dbg("(%s): Didn't assign the ISP or it was not supported.\n", device_name);
			file_unlock(isLock);
			return 0;
		}

		//sleep(2); // sometimes the device is not ready.
		modprobe("drxvi314");
		sleep(1);

		retry = 0;
		while(!check_if_dir_exist("/sys/class/usb/usbbcm") && (retry++ < 3)){
			usb_dbg("(%s): wait %d second for binding...\n", device_name, retry);
			modprobe_r("drxvi314");

			modprobe("drxvi314");
			sleep(1);
		}
	}
	else if(is_samsung_dongle(1, vid, pid)){ // Samsung U200
		// need to run one time and fillfull the nvram: usb_path%d_act.
		usb_dbg("(%s): Runing madwimax...\n", device_name);
		modprobe("tun");
		sleep(1);

		retry = 0;
		while(retry < 3){
			if(pids("madwimax")){
				killall_tk("madwimax");
				sleep(1);

				++retry;
			}
			else
				break;
		}

		xstart("madwimax");
	}
	else if(is_gct_dongle(1, vid, pid)){
		// need to run one time and fillfull the nvram: usb_path%d_act.
		usb_dbg("(%s): Runing GCT dongle...\n", device_name);
		modprobe("tun");
		sleep(1);

		retry = 0;
		while(retry < 3){
			if(pids("gctwimax")){
				killall_tk("gctwimax");
				sleep(1);

				++retry;
			}
			else
				break;
		}

		write_gct_conf();

		xstart("gctwimax", "-C", WIMAX_CONF);
	}
	else
#endif
	if(is_android_phone(0, vid, pid)){
		usb_dbg("(%s): do nothing with the MTP mode.\n", device_name);
	}
	else if(isRNDISInterface(device_name)){
		usb_dbg("(%s): Runing RNDIS...\n", device_name);
	}
	else if(isSerialInterface(device_name)){
		usb_dbg("(%s): Runing USB serial with (0x%s/0x%s)...\n", device_name, vid, pid);
		sleep(1);
		modprobe("usbserial");
		memset(modem_cmd, 0, 64);
		sprintf(modem_cmd, "vendor=0x%s", vid);
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "product=0x%s", pid);
		eval("insmod", "option", modem_cmd, buf);
		sleep(1);
	}
	else{ // isACMInterface(device_name)
		usb_dbg("(%s): Runing USB ACM...\n", device_name);
		modprobe("cdc-acm");
		sleep(1);
	}
#endif // RTCONFIG_USB_MODEM

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif
	return 1;
}

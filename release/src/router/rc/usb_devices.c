#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mount.h>

#include "rc.h"
#include "dongles.h"

#ifdef RTCONFIG_BCMARM
#include <bcmgpio.h>
#endif
#include <notify_rc.h>
#include <usb_info.h>
#include <disk_initial.h>

#define MAX_RETRY_LOCK 1

#ifdef RTCONFIG_USB_PRINTER
#define MAX_WAIT_PRINTER_MODULE 20
#define U2EC_FIFO "/var/u2ec_fifo"
#endif

#ifdef RTCONFIG_USB_MODEM
#define MODEM_SCRIPTS_DIR "/etc"

#define USB_MODESWITCH_CONF MODEM_SCRIPTS_DIR"/g3.conf"


// The dongles which need to create wcdma.cfg for switching.
int is_create_file_dongle(const unsigned int vid, const unsigned int pid)
{
	if((vid == 0x0408 && pid == 0xf000) // Q110
			|| (vid == 0x07d1 && pid == 0xa800) // DWM-156 A3
			)
		return 1;

	return 0;
}

#ifdef RTCONFIG_USB_BECEEM
int is_beceem_dongle(const int mode, const unsigned int vid, const unsigned int pid)
{
	if(mode){ // modem mode.
		if(vid == 0x198f // Yota
				|| (vid == 0x0b05 && (pid == 0x1780 || pid == 0x1781)) // GMC
				|| (vid == 0x19d2 && (pid == 0x0172 || pid == 0x0044)) // FreshTel or Giraffe
				)
			return 1;
	}
	else{ // zero-cd mode.
		if((vid == 0x198f) // Yota
				|| (vid == 0x0b05 && pid == 0xbccd) // GMC
				|| (vid == 0x19d2 && (pid == 0xbccd || pid == 0x0065)) // FreshTel or Giraffe
				)
			return 1;
	}

	return 0;
}

int is_samsung_dongle(const int mode, const unsigned int vid, const unsigned int pid)
{
	if(mode){ // modem mode.
		if(vid == 0x04e8 && pid == 0x6761
				)
			return 1;
	}
	else{ // zero-cd mode.
		if(vid == 0x04e8 && pid == 0x6761
				)
			return 1;
	}

	return 0;
}

int is_gct_dongle(const int mode, const unsigned int vid, const unsigned int pid)
{
	if(mode){ // modem mode.
		if(vid == 0x1076 && pid == 0x7f00
				)
			return 1;
	}
	else{ // zero-cd mode.
		if(vid == 0x1076 && pid == 0x7f40
				)
			return 1;
	}

	return 0;
}
#endif

// if the MTP mode of the phone will hang the DUT, need add the VID/PID in this function.
int is_android_phone(const int mode, const unsigned int vid, const unsigned int pid)
{
	if(nvram_match("modem_android", "1"))
		return 1;
	else if(mode){ // modem mode.
		if(vid == 0x04e8 && pid == 0x6864) // Samsung S3
			return 1;
	}
	else{ // MTP mode.
		if(vid == 0x04e8 && pid == 0x6860) // Samsung S3
			return 1;
	}

	return 0;
}

int is_storage_cd(const int mode, const unsigned int vid, const unsigned int pid)
{
	if(vid == 0x0781 && pid == 0x540e) // SanDisk cruzer
			return 1;

	return 0;
}

int write_3g_conf(FILE *fp, int dno, int aut, const unsigned int vid, const unsigned int pid)
{
	switch(dno){
		case SN_MU_Q101:
			fprintf(fp, "DefaultVendor=0x%04x",	0x0408);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0408);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xea02);
			fprintf(fp, "MessageEndpoint=0x%02x\n",	0x05);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_OPTION_ICON225:
			fprintf(fp, "DefaultVendor=0x%04x",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6971);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageEndpoint=0x%02x\n", 0x05);
			fprintf(fp, "MessageContent=%s\n",	"555342431223456780100000080000601000000000000000000000000000000");
			break;
		case SN_Option_GlobeSurfer_Icon:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6600);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000601000000000000000000000000000000");
			break;
		case SN_Option_GlobeSurfer_Icon72:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6901);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000601000000000000000000000000000000");
			break;
		case SN_Option_GlobeTrotter_GT_MAX36:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6600);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000601000000000000000000000000000000");
		/*	fprintf(fp, "NeedResponse=1\n");	// was ResponseNeeded=1 */
			break;
		case SN_Option_GlobeTrotter_GT_MAX72:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\3",	0x6701);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000601000000000000000000000000000000");
			break;
		case SN_Option_GlobeTrotter_EXPRESS72:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6701);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000601000000000000000000000000000000");
		/*	fprintf(fp, "NeedResponse=1\n");	// was ResponseNeeded=1 */
			break;
		case SN_Option_iCON210:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1e0e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1e0e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9000);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000006bd000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Option_GlobeTrotter_HSUPA_Modem:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x7011);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243785634120100000080000601000000000000000000000000000000");
			break;
		case SN_Option_iCON401:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x7401);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243785634120100000080000601000000000000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Vodafone_K3760:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x7501);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243785634120100000080000601000000000000000000000000000000");
			break;
		case SN_ATT_USBConnect_Quicksilver:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xd033);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243785634120100000080000601000000000000000000000000000000");
			break;
		case SN_Huawei_EC156:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1505);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProductList=%s\n",	"140b,1506");
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			fprintf(fp, "CheckSuccess=%d\n",	20);
			break;
		case SN_Huawei_E169:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1001);
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_E220:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1003);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_E180:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1414);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_Exxx:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1c0b);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1c05);
			fprintf(fp, "MessageEndpoint=0x%02x\n",	0x0f);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_E173:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x14b5);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x14a8);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_E630:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1033);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0035);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1003);
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_E270:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1446);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x14ac);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Huawei_E1550:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1446);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1001);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Huawei_E1612:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1446);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1406);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Huawei_E1690:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1446);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x140c);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Huawei_K3765:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1520);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1465);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Huawei_K4505:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1521);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1464);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_ZTE_MF620:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0001);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000600000000000000000000000000000000");
			break;
		case SN_ZTE_MF622:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0002);
			fprintf(fp, "MessageContent=%s\n",	"55534243f8f993882000000080000a85010101180101010101000000000000");
			break;
		case SN_ZTE_MF628:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0015);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000030000000000000000000000");
		/*	fprintf(fp, "NeedResponse=1\n");	// was ResponseNeeded=1 */
			break;
		case SN_ZTE_MF626:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0031);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_AC8710:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xfff5);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xffff);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678c00000008000069f030000000000000000000000000000");
			break;
		case SN_ZTE_AC2710:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xfff5);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xffff);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678c00000008000069f010000000000000000000000000000");
			break;
		case SN_ZTE6535_Z:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0052);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456782000000080000c85010101180101010101000000000000");
			break;
		case SN_ZTE_K3520_Z:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0055);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456782000000080000c85010101180101010101000000000000");
			break;
		case SN_ZTE_MF110:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0053);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0031);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"55534243876543212000000080000c85010101180101010101000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_K3565:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0063);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_ONDA_MT503HS:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0002);
			fprintf(fp, "MessageContent=%s\n",	"55534243b0c8dc812000000080000a85010101180101010101000000000000");
			break;
		case SN_ONDA_MT505UP:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0002);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000010080000a28000000001c00002000000000000000");
			break;
		case SN_Novatel_Wireless_Ovation_MC950D:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1410);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x5010);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1410);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x4400);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Novatel_U727:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1410);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x5010);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1410);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x4100);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Novatel_MC990D:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1410);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x5020);
			fprintf(fp, "Interface=%x\n",		5);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Novatel_U760:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1410);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x5030);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1410);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6000);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Alcatel_X020:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1001);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6061);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000606f50402527000000000000000000000");
			break;
		case SN_Alcatel_X200:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1bbb);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1bbb);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0000);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456788000000080000606f50402527000000000000000000000");
			break;
		case SN_Alcatel_X220L:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1bbb);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1bbb);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0017);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456788000000080000606f50402527000000000000000000000");
			break;
		case SN_AnyDATA_ADU_500A:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x16d5);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6502);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_BandLuxe_C120:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1a8d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1a8d);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1002);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456781200000080000603000000020000000000000000000000");
		/*	fprintf(fp, "NeedResponse=1\n");	// was ResponseNeeded=1 */
			break;
		case SN_Solomon_S3Gm660:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1dd6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1dd6);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1002);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456781200000080000603000000020000000000000000000000");
		/*	fprintf(fp, "NeedResponse=1\n");	// was ResponseNeeded=1 */
			break;
		case SN_C_motechD50:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6803);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x680a);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008ff524445564348470000000000000000");
			break;
		case SN_C_motech_CGU628:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6006);
			fprintf(fp, "MessageContent=%s\n",	"55534243d85dd88524000000800008ff524445564348470000000000000000");
			break;
		case SN_Toshiba_G450:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0930);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0d46);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0930);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0d45);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_UTStarcom_UM175:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x106c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x3b03);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x106c);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x3715);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008ff024445564348470000000000000000");
			break;
		case SN_Hummer_DTM5731:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1ab7);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x5700);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1ab7);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x5731);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_A_Link_3GU:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1e0e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1e0e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9200);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Sierra_Wireless_Compass597:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1199);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0fff);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1199);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0023);
			fprintf(fp, "SierraMode=1\n");
			break;
		case SN_Sierra881U:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1199);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0fff);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1199);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6856);
			fprintf(fp, "SierraMode=1\n");
			break;
		case SN_Sony_Ericsson_MD400:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0fce);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xd0e1);
			fprintf(fp, "TargetClass=0x%02x\n",	0x02);
			fprintf(fp, "Configuration=%d\n",	2);
			fprintf(fp, "SonyMode=1\n");
			break;
		case SN_LG_LDU_1900D:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000aff554d53434847000000000000000000");
			break;
		case SN_Samsung_SGH_Z810:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x04e8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6601);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000616000000000000000000000000000000");
			break;
		case SN_MobiData_MBD_200HU:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9000);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456788000000080000606f50402527000000000000000000000");
			break;
		case SN_BSNL_310G:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9605);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456788000000080000606f50402527000000000000000000000");
			break;
		case SN_BSNL_LW272:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x230d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0001);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "Configuration=%d\n",	3);
			break;
		case SN_ST_Mobile:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9063);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456788000000080000606f50402527000000000000000000000");
			break;
		case SN_MyWave_SW006:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x9200);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9202);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000606f50402527000000000000000000000");
			break;
		case SN_Cricket_A600:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1f28);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0021);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1f28);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0020);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800108df200000000000000000000000000000");
			break;
		case SN_EpiValley_SEC7089:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1b7d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0700);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1b7d);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0001);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008FF05B112AEE102000000000000000000");
			break;
		case SN_Samsung_U209:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x04e8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x04e8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6601);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000616000000000000000000000000000000");
			break;
		case SN_D_Link_DWM162_U5:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2001);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1e0e);
			fprintf(fp, "TargetProductList=%s\n",	"ce16,cefe");
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Novatel_MC760:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1410);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x5031);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1410);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6002);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Philips_TalkTalk:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0471);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1237);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0471);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1234);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000030000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_HuaXing_E600:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0471);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1237);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0471);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1206);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "Configuration=%d\n",	2);
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_C_motech_CHU_629S:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x700a);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456782400000080000dfe524445564348473d4e444953000000");
			break;
		case SN_Sagem9520:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1076);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x7f40);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1076);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7f00);
			fprintf(fp, "GCTMode=1\n");
			break;
		case SN_Nokia_CS10:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0421);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x060c);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0421);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x060e);
			fprintf(fp, "CheckSuccess=%d\n",	20);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Nokia_CS15:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0421);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0610);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0421);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0612);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Nokia_CS17:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0421);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0622);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0421);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0623);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Nokia_CS18:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0421);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0627);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0421);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0612);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Vodafone_MD950:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0471);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1210);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Siptune_LM75:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9000);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
	/* 0715 add */
		case SN_SU9800:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x9800);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456788000000080000606f50402527000000000000000000000");
			break;
		case SN_OPTION_ICON_461:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x7a05);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000601000000000000000000000000000000");
			break;
		case SN_Huawei_K3771:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x14c4);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x14ca);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_K3770:
			if(nvram_match("test_k3770", "1")){ // ACM mode
				fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
				fprintf(fp, "DefaultProduct=0x%04x\n",	0x14d1);
				fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
				fprintf(fp, "TargetProduct=0x%04x\n",	0x1c05);
				fprintf(fp, "CheckSuccess=%d\n",	20);
				fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			}
			else{
				fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
				fprintf(fp, "DefaultProduct=0x%04x\n",	0x14d1);
				fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
				fprintf(fp, "TargetProduct=0x%04x\n",	0x14c9);
				fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			}
			break;
		case SN_Mobile_Action:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0df7);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0800);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MobileActionMode=1\n");
			fprintf(fp, "NoDriverLoading=1\n");
			break;
		case SN_HP_P1102:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x03f0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x002a);
			fprintf(fp, "TargetClass=0x%02x\n",	0x07);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000006d0000000000000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Visiontek_82GH:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x230d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0007);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "Configuration=%d\n",	3);
			break;
		case SN_ZTE_MF190_var:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0149);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0124);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "MessageContent3=%s\n",	"55534243123456702000000080000c85010101180101010101000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MF192:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1216);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1218);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_ZTE_MF691:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1201);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1203);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_CHU_629S:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x700b);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456782400000080000dfe524445564348473d4e444953000000");
			break;
		case SN_JOA_LM_700r:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x198a);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0003);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x198a);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0002);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_ZTE_MF190:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1224);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0082);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_ffe:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xffe6);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xffe5);
			fprintf(fp, "MessageContent=%s\n",	"5553424330f4cf8124000000800108df200000000000000000000000000000");
			break;
		case SN_SE_MD400G:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0fce);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xd103);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "SonyMode=1\n");
			break;
		case SN_DLINK_DWM_156_A1:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x07d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xa800);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x07d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x3e02);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_DLINK_DWM_156_A3:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x07d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xa804);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x07d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7e11);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_DLINK_DWM_156_A7_1:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2001);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xa706);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2001);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7d01);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_DLINK_DWM_156_A7_2:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2001);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xa707);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2001);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7d02);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_DLINK_DWM_156_A7_3:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2001);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xa708);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2001);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7d03);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Huawei_U8220:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1030);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1034);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780600000080010a11060000000000000000000000000000");
		/*	fprintf(fp, "NoDriverLoading=1\n");*/
			break;
		case SN_Huawei_T_Mobile_NL:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x14fe);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1506);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_E3276S150:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x157c);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1506);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_ZTE_K3806Z:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0013);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0015);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Vibe_3G:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6061);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000606f50402527000000000000000000000");
			break;
		case SN_ZTE_MF637:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0110);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0121);
			fprintf(fp, "MessageContent=%s\n",	"5553424302000000000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ONDA_MW836UP_K:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0040);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x003e);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000010ff000000000000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Huawei_V725:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1009);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_ET8282:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1da1);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1d09);
			fprintf(fp, "HuaweiMode=1\n");
			break;
		case SN_Huawei_E352:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1449);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1444);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_BM358:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x380b);
			fprintf(fp, "TargetClass=0x%02x\n",	0x02);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Haier_CE_100:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x201e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2009);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Franklin_Wireless_U210_var:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1fac);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0032);
			fprintf(fp, "Configuration=%d\n",	2);
			break;
		case SN_Exiss_E_190:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x8888);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6500);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6533);
			fprintf(fp, "MessageContent=%s\n",	"5553424398e2c4812400000080000bff524445564348473d43440000000000");
			break;
		case SN_dealextreme:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0015);
			fprintf(fp, "MessageContent=%s\n",	"5553424368032c882400000080000612000000240000000000000000000000");
			fprintf(fp, "CheckSuccess=%d\n",	40);
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_CHU_628S:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x16d8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6281);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008ff524445564348470000000000000000");
			break;
		case SN_MediaTek_Wimax_USB:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0e8d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x7109);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0e8d);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7118);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NoDriverLoading=1\n");
			break;
		case SN_AirPlus_MCD_800:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1edf);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6003);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "Configuration=%d\n",	3);
			break;
		case SN_UMW190:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x106c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x3b05);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x106c);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x3716);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008ff020000000000000000000000000000");
			break;
		case SN_LG_AD600:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6190);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x61a7);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_GW_D301:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0fd1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "Configuration=%d\n",	3);
			break;
		case SN_Qtronix_EVDO_3G:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c7);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x05c7);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6000);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678c00000008000069f140000000000000000000000000000");
			break;
		case SN_Huawei_EC168C:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1446);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1412);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Olicard_145:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0b3c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0b3c);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xc003);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678c000000080010606f50402527000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ONDA_MW833UP:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0009);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x000b);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000010ff000000000000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Kobil_mIdentity_3G_2:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0d46);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x45a5);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0d46);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x45ad);
			fprintf(fp, "KobilMode=1\n");
			break;
		case SN_Kobil_mIdentity_3G_1:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0d46);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x45a1);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0d46);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x45a9);
			fprintf(fp, "KobilMode=1\n");
			break;
		case SN_Samsung_GT_B3730:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x04e8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x689a);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x04e8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6889);
			fprintf(fp, "MessageContent=%s\n",	"55534243785634120100000080000601000000000000000000000000000000");
			break;
		case SN_BSNL_Capitel:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x9e00);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000606f50402527000000000000000000000");
			break;
		case SN_ZTE_WCDMA_from_BNSL:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0108);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Huawei_U8110:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1031);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1035);
			fprintf(fp, "CheckSuccess=%d\n",	20);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780600000080010a11060000000000000000000000000000");
			fprintf(fp, "NoDriverLoading=1\n");
			break;
		case SN_ONDA_MW833UP_2:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0013);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0012);
			fprintf(fp, "CheckSuccess=%d\n",	20);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000010ff000000000000000000000000000000");
		/*	fprintf(fp, "NeedResponse=1\n");	// was ResponseNeeded=1 */
			break;
		case SN_Netgear_WNDA3200:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0cf3);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x20ff);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0cf3);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7010);
			fprintf(fp, "CheckSuccess=%d\n",	10);
			fprintf(fp, "NoDriverLoading=1\n");
			fprintf(fp, "MessageContent=%s\n",	"5553424329000000000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Huawei_R201:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1523);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1491);
			fprintf(fp, "CheckSuccess=%d\n",	20);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Huawei_K4605:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x14c1);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x14c6);
			fprintf(fp, "CheckSuccess=%d\n",	20);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_LG_LUU_2100TI:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x613f);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6141);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_LG_L_05A:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x613a);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6124);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MU351:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0003);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MF110_var:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0083);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0124);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Olicard_100:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0b3c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xc700);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0b3c);
			fprintf(fp, "TargetProductList=%s\n",	"c000,c001,c002");
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000030000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MF112:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0103);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0031);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"55534243876543212000000080000c85010101180101010101000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Franklin_Wireless_U210:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1fac);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0130);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1fac);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0131);
			fprintf(fp, "CheckSuccess=%d\n",	20);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800108df200000000000000000000000000000");
			break;
		case SN_ZTE_K3805_Z:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1001);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1003);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_SE_MD300:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0fce);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xd0cf);
			fprintf(fp, "TargetClass=0x%02x\n",	0x02);
			fprintf(fp, "Configuration=%d\n",	3);
			fprintf(fp, "DetachStorageOnly=1\n");
			break;
		case SN_Digicom_8E4455:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1266);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1266);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1009);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424387654321000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Kyocera_W06K:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0482);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x024d);
			fprintf(fp, "Configuration=%d\n",	2);
			break;
		case SN_LG_HDM_2100:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x607f);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6114);
			fprintf(fp, "MessageContent=%s\n",	"1201100102000040041014610000010200018006000100001200");
			break;
		case SN_Beceem_BCSM250:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x198f);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xbccd);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x198f);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0220);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800006bc626563240000000000000000000000");
			break;
		case SN_Beceem_BCSM250_ASUS:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0b05);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xbccd);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0b05);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1781);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800006bc626563240000000000000000000000");
			break;
		case SN_ZTEAX226:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xbccd);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0172);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800006bc626563240000000000000000000000");
		/*	fprintf(fp, "NoDriverLoading=1\n");*/
			break;
		case SN_Huawei_U7510:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x101e);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780600000080000601000000000000000000000000000000");
			break;
		case SN_ZTE_AC581:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0026);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0094);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_UTStarcom_UM185E:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x106c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x3b06);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x106c);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x3717);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008ff020000000000000000000000000000");
			break;
		case SN_AVM_Fritz:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x057c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x84ff);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x057c);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x8401);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000ff0000000000000000000000");
			break;
	/* 0818 add */
		case SN_SU_8000U:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2020);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf00e);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2020);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1005);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Cisco_AM10:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1307);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1169);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x13b1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0031);
			fprintf(fp, "CiscoMode=1\n");
			break;
		case SN_Huawei_EC122:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1446);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x140c);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011060000000000000000000000000000");
			break;
		case SN_Huawei_EC1262:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1446);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct= \"1001,1406,140b,140c,1412,141b,14ac\"\n");
			fprintf(fp, "CheckSuccess=%d\n",	20);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_EC306:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1505);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1506);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Sierra_U598:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1199);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0fff);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1199);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0025);
			fprintf(fp, "SierraMode=1\n");
			break;
		case SN_NovatelU720:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1410);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2110);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "CheckSuccess=%d\n",	20);	// tmp test
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Pantech_UM150:					// tmp test
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x106c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x3711);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008ff024445564348470000000000000000");
			break;
		case SN_Pantech_UM175:					// tmp test
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x106c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x3714);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567824000000800008ff024445564348470000000000000000");
			break;
		case SN_Sierra_U595:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1199);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0120);
			fprintf(fp, "CheckSuccess=%d\n",	10);	// tmp test
			fprintf(fp, "SierraMode=1\n");
			fprintf(fp, "NoDriverLoading=1\n");
			break;
		/* 120703 add */
		case SN_Qisda_H21:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1da5);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1da5);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x4512);
			fprintf(fp, "QisdaMode=1\n");
			break;
		case SN_StrongRising_Air_FlexiNet:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x21f5);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x21f5);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x2008);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678c000000080000671010000000000000000000000000000");
			break;
		case SN_BandLuxe_C339:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1a8d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x2000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1a8d);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x2006);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Celot_CT680:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x211f);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6802);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678c000000080000671010000000000000000000000000000");
			break;
		case SN_Huawei_E353:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1f01);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x14db);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000a11062000000000000100000000000000");
			fprintf(fp, "NoDriverLoading=1\n");
			break;
		case SN_Haier_CE682:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x201e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1023);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x201e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1022);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000600000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679c000000080000671030000000000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_LG_L_02C_LTE:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x61dd);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x618f);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_LG_SD711:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x61e7);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x61e6);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_LG_L_08C:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x61eb);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x61ea);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_IODATA_WMX2_U:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x04bb);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xbccd);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x04bb);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0949);
			fprintf(fp, "MessageContent=%s\n",	"55534243f0298d8124000000800006bc626563240000000000000000000000");
			break;
		case SN_Option_GI1515:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xd001);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xd157);
			fprintf(fp, "MessageContent=%s\n",	"55534243785634120100000080000601000000000000000000000000000000");
			break;
		case SN_LG_L_07A:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1004);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x614e);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1004);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6135);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_A371B:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0169);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0170);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MF652:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1520);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0142);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MF652_VAR:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0146);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0143);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_T_W_WU160:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2077);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2077);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9000);
			fprintf(fp, "MessageContent=%s\n",	"5553424308902082000000000000061b000000020000000000000000000000");
			break;
		case SN_Nokia_CS_21M_02:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0421);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0637);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0421);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0638);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Telewell_TW_3G:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x98ff);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1c9e);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9801);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000080000606f50402527000000000000000000000");
			break;
		case SN_ZTE_MF637_2:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0031);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0094);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Samsung_GT_B1110:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x04e8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x680c);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x04e8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6792);
			fprintf(fp, "MessageContent=%s\n",	"0902200001010080fa0904000002080650000705010200020007058102000200");
			break;
		case SN_ZTE_MF192_VAR:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1514);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1515);
			fprintf(fp, "MessageContent=%s\n",	"5553424348c4758600000000000010ff000000000000000000000000000000");
			break;
		case SN_MediaTek_MT6276M:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0e8d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0002);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0e8d);
			fprintf(fp, "TargetProductList=%s\n",	"00a1,00a2");
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000006f0010300000000000000000000000000");
			break;
		case SN_Tata_Photon_plus:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x22f4);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0021);
			fprintf(fp, "TargetClass=0x%02x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"555342439f000000000000000000061b000000020000000000000000000000");
			break;
		case SN_Option_Globetrotter:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x8006);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9100);
			fprintf(fp, "MessageContent=%s\n",	"55534243785634120100000080000601000000000000000000000000000000");
			break;
		case SN_Option_ICON_711:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x4007);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0af0);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x4005);
			fprintf(fp, "SierraMode=1\n");
			break;
		case SN_Celot_K300:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x211f);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6801);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_HISENSE_E910:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x109b);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf009);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x109b);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9114);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Huawei_K5005:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x14c3);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x14c8);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_E173_MoiveStar:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1c24);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1c12);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Onda_MSA14_4:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0060);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x005f);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000008ff00000000000003000000000000000");
			break;
		case SN_WeTelecom_WMD300:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x22de);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6803);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x22de);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6801);
#if 0
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
#else
			fprintf(fp, "StandardEject=1\n");
#endif
			break;
		case SN_Huawei_K5150:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1f16);
#if 1
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProductList=%s\n", "14f8,1575");
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000101000100000000000000");
#else
			fprintf(fp, "Configuration=2\n");
#endif
			break;
		case SN_ZTE_MF823:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1225);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1405);
			fprintf(fp, "StandardEject=1\n");
			break;
		/* 141112 */
		case SN_Alcatel_TL131:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x9024);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9025);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_AVM_Fritz_V2:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x057c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x62ff);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x057c);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x8501);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000ff0000000000000000000000");
			break;
		case SN_Quanta_MobileGenie_4G_lte:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0408);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xea43);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0408);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xea47);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Pantech_LTE:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x10a9);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6080);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x10a9);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x6085);
			fprintf(fp, "PantechMode=1\n");
			break;
		case SN_BlackBerry_Q10:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0fca);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x8020);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0fca);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x8012);
			fprintf(fp, "BlackberryMode=1\n");
			break;
		case SN_Huawei_K4305:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1f15);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1400);
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000000000100000000000000");
			break;
		case SN_Huawei_E3276s:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x156a);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProductList=%s\n",	"156b,156c");
			fprintf(fp, "MessageContent=%s\n",	"55534243123456780000000000000011062000000100000000000000000000");
			break;
		case SN_Huawei_E3131:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x155b);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1506);
			fprintf(fp, "MessageContent=%s\n",	"55534243000000000000000000000011060000000100000000000000000000");
			break;
		case SN_Huawei_E3372:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x157d);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "TargetProductList=%s\n",	"14db,14dc");
			fprintf(fp, "HuaweiNewMode=1\n");
			break;
		case SN_Huawei_E303u:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x15ca);
			fprintf(fp, "HuaweiNewMode=1\n");
			break;
		case SN_Huawei_E3531s:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x12d1);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x15cd);
			fprintf(fp, "HuaweiNewMode=1\n");
			break;
		case SN_Teracom_LW272:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x230d);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0103);
			fprintf(fp, "Configuration=2\n");
			break;
		case SN_TP_Link_MA260:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2357);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xf000);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2357);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x9000);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Quanta_1K3_LTE:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0408);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xea25);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0408);
			fprintf(fp, "TargetProduct=0x%04x\n",	0xea26);
			fprintf(fp, "QuantaMode=1\n");
			break;
		case SN_WeTelecom_WMD200:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x22de);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x6801);
			fprintf(fp, "TargetClass=0x%04x\n",	0xff);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061e000000000000000000000000000000");
			fprintf(fp, "MessageContent2=%s\n",	"5553424312345679000000000000061b000000020000000000000000000000");
			break;
		case SN_ZTE_MF680:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1227);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1252);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MF656A:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0150);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0124);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_ZTE_MF196:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x1528);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x1527);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			fprintf(fp, "NeedResponse=1\n");
			break;
		case SN_Mediatek_MT6229:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2020);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0002);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2020);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x2000);
			fprintf(fp, "MessageContent=%s\n",	"555342430820298900000000000003f0010100000000000000000000000000");
			break;
		case SN_DLINK_DWR_510:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x2001);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0xa805);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x2001);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7e12);
			fprintf(fp, "MessageContent=%s\n",	"5553424308407086000000000000061b000000020000000000000000000000");
			break;
		case SN_Novatel_MC996D:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1410);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x5023);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1410);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x7030);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Pantech_UML290:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x106c);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x3b11);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x106c);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x3718);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Onda_MT8205_LTE:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0266);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x19d2);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0265);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Axesstel_Modems:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0010);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x05c6);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x00a0);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Nokia_CS_12:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x0421);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0618);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x0421);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0619);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Onda_WM301:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0068);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0069);
			fprintf(fp, "MessageContent=%s\n",	"5553424312345678000000000000061b000000020000000000000000000000");
			break;
		case SN_Onda_TM201_14_4:
			fprintf(fp, "DefaultVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "DefaultProduct=0x%04x\n",	0x0063);
			fprintf(fp, "TargetVendor=0x%04x\n",	0x1ee8);
			fprintf(fp, "TargetProduct=0x%04x\n",	0x0064);
			fprintf(fp, "MessageContent=%s\n",	"555342431234567800000000000008FF000000000000030000000000000000");
			break;
		default:
			fprintf(fp, "\n");
			if(vid != 0 && pid != 0){
				fprintf(fp, "DefaultVendor=0x%04x\n",	vid);
				fprintf(fp, "DefaultProduct=0x%04x\n",	pid);
				if(vid == 0x12d1){ // Huawei
					fprintf(fp, "HuaweiMode=1\n");
				}
			}
			break;
	}

	return 0;
}

int init_3g_param(const char *port_path, const unsigned int vid, const unsigned int pid)
{
	FILE *fp;
	char conf_file[32];
	char dongle_name[32];
	int asus_extra_auto = 0;

	memset(conf_file, 0, 32);
	sprintf(conf_file, "%s.%s", USB_MODESWITCH_CONF, port_path);
	if((fp = fopen(conf_file, "r")) != NULL){
		fclose(fp);
		usb_dbg("%04x:%04x Already had the usb_modeswitch file.\n", vid, pid);
#ifdef LINUX30
		return 0;
#else
		return 1;
#endif
	}

	fp = fopen(conf_file, "w+");
	if(!fp){
		usb_dbg("Couldn't write the config file(%s).\n", conf_file);
		return 0;
	}

	memset(dongle_name, 0, 32);
	strncpy(dongle_name, nvram_safe_get("Dev3G"), 32);

	if(!strcmp(dongle_name, "Huawei-E160G")
			|| !strcmp(dongle_name, "Huawei-E176")
			|| !strcmp(dongle_name, "Huawei-K3520")
			|| !strcmp(dongle_name, "Huawei-ET128")
			|| !strcmp(dongle_name, "Huawei-E1800")
			|| !strcmp(dongle_name, "Huawei-E172")
			|| !strcmp(dongle_name, "Huawei-E372")
			|| !strcmp(dongle_name, "Huawei-E122")
			|| !strcmp(dongle_name, "Huawei-E160E")
			|| !strcmp(dongle_name, "Huawei-E1552")
			|| !strcmp(dongle_name, "Huawei-E1823")
			|| !strcmp(dongle_name, "Huawei-E1762")
			|| !strcmp(dongle_name, "Huawei-E1750C")
			|| !strcmp(dongle_name, "Huawei-E1752Cu")
			)
		asus_extra_auto = 1;

	if(nvram_match("Dev3G", "AUTO") || (asus_extra_auto == 1)){
		if(asus_extra_auto)
			nvram_set("d3g", dongle_name);
		else
			nvram_set("d3g", "usb_3g_dongle");
usb_dbg("3G: Auto setting.\n");

		if(vid == 0x0408 && (pid == 0xea02 || pid == 0x1000))
			write_3g_conf(fp, SN_MU_Q101, 1, vid, pid);
		else if(vid == 0x0408 && pid == 0xea43)
			write_3g_conf(fp, SN_Quanta_MobileGenie_4G_lte, 1, vid, pid);
		else if(vid == 0x0408 && pid == 0xea25)
			write_3g_conf(fp, SN_Quanta_1K3_LTE, 1, vid, pid);
		else if(vid == 0x05c6 && pid == 0x1000) // also Option-GlobeSurfer-Icon72(may have new fw setting, bug not included here), Option-GlobeTrotter-GT-MAX36.....Option-Globexx series, AnyDATA-ADU-500A, Samsung-SGH-Z810, Vertex Wireless 100 Series
			write_3g_conf(fp, SN_Option_GlobeSurfer_Icon, 1, vid, pid);
		else if(vid == 0x05c6 && pid == 0x2001)
			write_3g_conf(fp, SN_D_Link_DWM162_U5, 1, vid, pid);
		else if(vid == 0x05c6 && pid == 0xf000)
			write_3g_conf(fp, SN_Siptune_LM75, 1, vid, pid);
		else if(vid == 0x05c6 && pid == 0x9024)
			write_3g_conf(fp, SN_Alcatel_TL131, 1, vid, pid);
		else if(vid == 0x05c6 && pid == 0x2000)
			write_3g_conf(fp, SN_dealextreme, 1, vid, pid);
		else if(vid == 0x05c6 && pid == 0x0010)
			write_3g_conf(fp, SN_Axesstel_Modems, 1, vid, pid);
		else if(vid == 0x1e0e && pid == 0xf000)	// A-Link-3GU
			write_3g_conf(fp, SN_Option_iCON210, 1, vid, pid);
		else if(vid == 0x0af0 && pid == 0x6971)
		{
			nvram_set("d3g", "OPTION-ICON225");
			write_3g_conf(fp, SN_OPTION_ICON225, 1, vid, pid);
		}
		else if(vid == 0x0af0 && pid == 0x7011)
		{
			nvram_set("d3g", "Option-GlobeTrotter-HSUPA-Modem");
			write_3g_conf(fp, SN_Option_GlobeTrotter_HSUPA_Modem, 1, vid, pid);
		}
		else if(vid == 0x0af0 && pid == 0x7401)
		{
			nvram_set("d3g", "Option-iCON-401");
			write_3g_conf(fp, SN_Option_iCON401, 1, vid, pid);
		}
		else if(vid == 0x0af0 && pid == 0x7501)
		{
			nvram_set("d3g", "Vodafone-K3760");
			write_3g_conf(fp, SN_Vodafone_K3760, 1, vid, pid);
		}
		else if(vid == 0x0af0 && pid == 0xd033)
		{
			nvram_set("d3g", "ATT-USBConnect-Quicksilver");
			write_3g_conf(fp, SN_ATT_USBConnect_Quicksilver, 1, vid, pid);
		}
		else if(vid == 0x0af0 && pid == 0x7a05)
			write_3g_conf(fp, SN_OPTION_ICON_461, 1, vid, pid);
		else if(vid == 0x0af0 && pid == 0xd001)
			write_3g_conf(fp, SN_Option_GI1515, 1, vid, pid);
		else if(vid == 0x0af0 && pid == 0x8006)
			write_3g_conf(fp, SN_Option_Globetrotter, 1, vid, pid);
		else if(vid == 0x0af0 && pid == 0x4007)
			write_3g_conf(fp, SN_Option_ICON_711, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1505)
			write_3g_conf(fp, SN_Huawei_EC156, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1001)
			write_3g_conf(fp, SN_Huawei_E169, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1003)
			write_3g_conf(fp, SN_Huawei_E220, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1414)
			write_3g_conf(fp, SN_Huawei_E180, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1c0b)	// tmp test
			write_3g_conf(fp, SN_Huawei_Exxx, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x14b5)
			write_3g_conf(fp, SN_Huawei_E173, 1, vid, pid);
		else if(vid == 0x1033 && pid == 0x0035)
			write_3g_conf(fp, SN_Huawei_E630, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1446)	// E1550, E1612, E1690
			write_3g_conf(fp, SN_Huawei_E270, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1520)
			write_3g_conf(fp, SN_Huawei_K3765, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1521)
			write_3g_conf(fp, SN_Huawei_K4505, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x14c4)
			write_3g_conf(fp, SN_Huawei_K3771, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x14d1)
			write_3g_conf(fp, SN_Huawei_K3770, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1030)
			write_3g_conf(fp, SN_Huawei_U8220, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x14fe)
			write_3g_conf(fp, SN_Huawei_T_Mobile_NL, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x157c)
			write_3g_conf(fp, SN_Huawei_E3276S150, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1009)
			write_3g_conf(fp, SN_Huawei_V725, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1da1)
			write_3g_conf(fp, SN_Huawei_ET8282, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1449)
			write_3g_conf(fp, SN_Huawei_E352, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x380b)
			write_3g_conf(fp, SN_Huawei_BM358, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1031)
			write_3g_conf(fp, SN_Huawei_U8110, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1523)
			write_3g_conf(fp, SN_Huawei_R201, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x14c1)
			write_3g_conf(fp, SN_Huawei_K4605, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x101e)
			write_3g_conf(fp, SN_Huawei_U7510, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1f01)
			write_3g_conf(fp, SN_Huawei_E353, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x14c3)
			write_3g_conf(fp, SN_Huawei_K5005, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1c24)
			write_3g_conf(fp, SN_Huawei_E173_MoiveStar, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1f16)
			write_3g_conf(fp, SN_Huawei_K5150, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x1f15)
			write_3g_conf(fp, SN_Huawei_K4305, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x156a)
			write_3g_conf(fp, SN_Huawei_E3276s, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x2000)	// also ZTE622, 628, 626, 6535-Z, K3520-Z, K3565, ONDA-MT503HS, ONDA-MT505UP
			write_3g_conf(fp, SN_ZTE_MF626, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0xfff5)
			write_3g_conf(fp, SN_ZTE_AC2710, 1, vid, pid);	// 2710
		else if(vid == 0x19d2 && pid == 0xbccd)
			write_3g_conf(fp, SN_ZTEAX226, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0026)
			write_3g_conf(fp, SN_ZTE_AC581, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0169)
			write_3g_conf(fp, SN_ZTE_A371B, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1520)
			write_3g_conf(fp, SN_ZTE_MF652, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0146)
			write_3g_conf(fp, SN_ZTE_MF652_VAR, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0031)
			write_3g_conf(fp, SN_ZTE_MF637_2, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1514)
			write_3g_conf(fp, SN_ZTE_MF192_VAR, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1225)
			write_3g_conf(fp, SN_ZTE_MF823, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0053)
			write_3g_conf(fp, SN_ZTE_MF110, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0149)
			write_3g_conf(fp, SN_ZTE_MF190_var, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1216)
			write_3g_conf(fp, SN_ZTE_MF192, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1201)
			write_3g_conf(fp, SN_ZTE_MF691, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1224)
			write_3g_conf(fp, SN_ZTE_MF190, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0xffe6)
			write_3g_conf(fp, SN_ZTE_ffe, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0013)
			write_3g_conf(fp, SN_ZTE_K3806Z, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0110)
			write_3g_conf(fp, SN_ZTE_MF637, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0003)
			write_3g_conf(fp, SN_ZTE_MU351, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0083)
			write_3g_conf(fp, SN_ZTE_MF110_var, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0103)
			write_3g_conf(fp, SN_ZTE_MF112, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1001)
			write_3g_conf(fp, SN_ZTE_K3805_Z, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1227)
			write_3g_conf(fp, SN_ZTE_MF680, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0150)
			write_3g_conf(fp, SN_ZTE_MF656A, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x1528)
			write_3g_conf(fp, SN_ZTE_MF196, 1, vid, pid);
		else if(vid == 0x19d2 && pid == 0x0266)
			write_3g_conf(fp, SN_Onda_MT8205_LTE, 1, vid, pid);
		else if(vid == 0x1410 && pid == 0x5010)	// U727
			write_3g_conf(fp, SN_Novatel_Wireless_Ovation_MC950D, 1, vid, pid);
		else if(vid == 0x1410 && pid == 0x5020)
			write_3g_conf(fp, SN_Novatel_MC990D, 1, vid, pid);
		else if(vid == 0x1410 && pid == 0x5030)
			write_3g_conf(fp, SN_Novatel_U760, 1, vid, pid);
		else if(vid == 0x1410 && pid == 0x5031)
			write_3g_conf(fp, SN_Novatel_MC760, 1, vid, pid);
		else if(vid == 0x1410 && pid == 0x2110)
			write_3g_conf(fp, SN_NovatelU720, 1, vid, pid);
		else if(vid == 0x1410 && pid == 0x5023)
			write_3g_conf(fp, SN_Novatel_MC996D, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0x1001)
			write_3g_conf(fp, SN_Alcatel_X020, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0xf000)	// ST-Mobile, MobiData MBD-200HU, // BSNL 310G
			write_3g_conf(fp, SN_BSNL_310G, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0x9605)	// chk BSNL 310G
			write_3g_conf(fp, SN_BSNL_310G, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0x9200)
			write_3g_conf(fp, SN_MyWave_SW006, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0x9800)
			write_3g_conf(fp, SN_SU9800, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0x6061)
			write_3g_conf(fp, SN_Vibe_3G, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0x9e00)
			write_3g_conf(fp, SN_BSNL_Capitel, 1, vid, pid);
		else if(vid == 0x1c9e && pid == 0x98ff)
			write_3g_conf(fp, SN_Telewell_TW_3G, 1, vid, pid);
		else if(vid == 0x1bbb && pid == 0xf000)
			write_3g_conf(fp, SN_Alcatel_X200, 1, vid, pid);
		else if(vid == 0x1dd6 && pid == 0x1000)
			write_3g_conf(fp, SN_Solomon_S3Gm660, 1, vid, pid);
		else if(vid == 0x16d8 && pid == 0x6803)
			write_3g_conf(fp, SN_C_motechD50, 1, vid, pid);
		else if(vid == 0x16d8 && pid == 0xf000)
			write_3g_conf(fp, SN_C_motech_CGU628, 1, vid, pid);
		else if(vid == 0x16d8 && pid == 0x700a)
			write_3g_conf(fp, SN_C_motech_CHU_629S, 1, vid, pid);
		else if(vid == 0x16d8 && pid == 0x700b)
			write_3g_conf(fp, SN_CHU_629S, 1, vid, pid);
		else if(vid == 0x16d8 && pid == 0x6281)
			write_3g_conf(fp, SN_CHU_628S, 1, vid, pid);
		else if(vid == 0x0930 && pid == 0x0d46)
			write_3g_conf(fp, SN_Toshiba_G450, 1, vid, pid);
		else if(vid == 0x106c && pid == 0x3b03)
			write_3g_conf(fp, SN_UTStarcom_UM175, 1, vid, pid);
		else if(vid == 0x106c && pid == 0x3b05)
			write_3g_conf(fp, SN_UMW190, 1, vid, pid);
		else if(vid == 0x106c && pid == 0x3b06)
			write_3g_conf(fp, SN_UTStarcom_UM185E, 1, vid, pid);
		else if(vid == 0x106c && pid == 0x3711)
			write_3g_conf(fp, SN_Pantech_UM150, 1, vid, pid);
		else if(vid == 0x106c && pid == 0x3714)
			write_3g_conf(fp, SN_Pantech_UM175, 1, vid, pid);
		else if(vid == 0x106c && pid == 0x3b11)
			write_3g_conf(fp, SN_Pantech_UML290, 1, vid, pid);
		else if(vid == 0x1ab7 && pid == 0x5700)
			write_3g_conf(fp, SN_Hummer_DTM5731, 1, vid, pid);
		else if(vid == 0x1199 && pid == 0x0fff)	// Sierra881U
			write_3g_conf(fp, SN_Sierra_Wireless_Compass597, 1, vid, pid);
		else if(vid == 0x1199 && pid == 0x0120)
			write_3g_conf(fp, SN_Sierra_U595, 1, vid, pid);
		else if(vid == 0x0fce && pid == 0xd0e1)
			write_3g_conf(fp, SN_Sony_Ericsson_MD400, 1, vid, pid);
		else if(vid == 0x0fce && pid == 0xd103)
			write_3g_conf(fp, SN_SE_MD400G, 1, vid, pid);
		else if(vid == 0x230d && pid == 0x0001)
			write_3g_conf(fp, SN_BSNL_LW272, 1, vid, pid);
		else if(vid == 0x230d && pid == 0x0103)
			write_3g_conf(fp, SN_Teracom_LW272, 1, vid, pid);
		else if(vid == 0x1f28 && pid == 0x0021)
			write_3g_conf(fp, SN_Cricket_A600, 1, vid, pid);
		else if(vid == 0x1b7d && pid == 0x0700)
			write_3g_conf(fp, SN_EpiValley_SEC7089, 1, vid, pid);
		else if(vid == 0x04e8 && pid == 0xf000)
			write_3g_conf(fp, SN_Samsung_U209, 1, vid, pid);
		else if(vid == 0x04e8 && pid == 0x689a)
			write_3g_conf(fp, SN_Samsung_GT_B3730, 1, vid, pid);
		else if(vid == 0x04e8 && pid == 0x680c)
			write_3g_conf(fp, SN_Samsung_GT_B1110, 1, vid, pid);
		else if(vid == 0x0471 && pid == 0x1237)	// HuaXing E600
			write_3g_conf(fp, SN_Philips_TalkTalk, 1, vid, pid);
		else if(vid == 0x0471 && pid == 0x1210)
			write_3g_conf(fp, SN_Vodafone_MD950, 1, vid, pid);
		else if(vid == 0x0421 && pid == 0x060c)
			write_3g_conf(fp, SN_Nokia_CS10, 1, vid, pid);
		else if(vid == 0x0421 && pid == 0x0610)
			write_3g_conf(fp, SN_Nokia_CS15, 1, vid, pid);
		else if(vid == 0x0421 && pid == 0x0622)
			write_3g_conf(fp, SN_Nokia_CS17, 1, vid, pid);
		else if(vid == 0x0421 && pid == 0x0627)
			write_3g_conf(fp, SN_Nokia_CS18, 1, vid, pid);
		else if(vid == 0x0421 && pid == 0x0637)
			write_3g_conf(fp, SN_Nokia_CS_21M_02, 1, vid, pid);
		else if(vid == 0x0421 && pid == 0x0618)
			write_3g_conf(fp, SN_Nokia_CS_12, 1, vid, pid);
		else if(vid == 0x0df7 && pid == 0x0800)
			write_3g_conf(fp, SN_Mobile_Action, 1, vid, pid);
		else if(vid == 0x03f0 && pid == 0x002a)
			write_3g_conf(fp, SN_HP_P1102, 1, vid, pid);
		else if(vid == 0x198a && pid == 0x0003)
			write_3g_conf(fp, SN_JOA_LM_700r, 1, vid, pid);
		else if(vid == 0x07d1 && pid == 0xa804)
			write_3g_conf(fp, SN_DLINK_DWM_156_A3, 1, vid, pid);
		else if(vid == 0x2001 && pid == 0xa706)
			write_3g_conf(fp, SN_DLINK_DWM_156_A7_1, 1, vid, pid);
		else if(vid == 0x2001 && pid == 0xa707)
			write_3g_conf(fp, SN_DLINK_DWM_156_A7_2, 1, vid, pid);
		else if(vid == 0x2001 && pid == 0xa708)
			write_3g_conf(fp, SN_DLINK_DWM_156_A7_3, 1, vid, pid);
		else if(vid == 0x2001 && pid == 0xa805)
			write_3g_conf(fp, SN_DLINK_DWR_510, 1, vid, pid);
		else if(vid == 0x1ee8 && pid == 0x0040)
			write_3g_conf(fp, SN_ONDA_MW836UP_K, 1, vid, pid);
		else if(vid == 0x1ee8 && pid == 0x0009)
			write_3g_conf(fp, SN_ONDA_MW833UP, 1, vid, pid);
		else if(vid == 0x1ee8 && pid == 0x0013)
			write_3g_conf(fp, SN_ONDA_MW833UP_2, 1, vid, pid);
		else if(vid == 0x1ee8 && pid == 0x0060)
			write_3g_conf(fp, SN_Onda_MSA14_4, 1, vid, pid);
		else if(vid == 0x1ee8 && pid == 0x0068)
			write_3g_conf(fp, SN_Onda_WM301, 1, vid, pid);
		else if(vid == 0x1ee8 && pid == 0x0063)
			write_3g_conf(fp, SN_Onda_TM201_14_4, 1, vid, pid);
		else if(vid == 0x201e && pid == 0x2009)
			write_3g_conf(fp, SN_Haier_CE_100, 1, vid, pid);
		else if(vid == 0x201e && pid == 0x1023)
			write_3g_conf(fp, SN_Haier_CE682, 1, vid, pid);
		else if(vid == 0x1fac && pid == 0x0032)
			write_3g_conf(fp, SN_Franklin_Wireless_U210_var, 1, vid, pid);
		else if(vid == 0x1fac && pid == 0x0130)
			write_3g_conf(fp, SN_Franklin_Wireless_U210, 1, vid, pid);
		else if(vid == 0x8888 && pid == 0x6500)
			write_3g_conf(fp, SN_Exiss_E_190, 1, vid, pid);
		else if(vid == 0x1edf && pid == 0x6003)
			write_3g_conf(fp, SN_AirPlus_MCD_800, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x1000)
			write_3g_conf(fp, SN_LG_LDU_1900D, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x6190)
			write_3g_conf(fp, SN_LG_AD600, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x613f)
			write_3g_conf(fp, SN_LG_LUU_2100TI, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x613a)
			write_3g_conf(fp, SN_LG_L_05A, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x607f)
			write_3g_conf(fp, SN_LG_HDM_2100, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x61dd)
			write_3g_conf(fp, SN_LG_L_02C_LTE, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x61e7)
			write_3g_conf(fp, SN_LG_SD711, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x61eb)
			write_3g_conf(fp, SN_LG_L_08C, 1, vid, pid);
		else if(vid == 0x1004 && pid == 0x614e)
			write_3g_conf(fp, SN_LG_L_07A, 1, vid, pid);
		else if(vid == 0x0fd1 && pid == 0x1000)
			write_3g_conf(fp, SN_GW_D301, 1, vid, pid);
		else if(vid == 0x05c7 && pid == 0x1000)
			write_3g_conf(fp, SN_Qtronix_EVDO_3G, 1, vid, pid);
		else if(vid == 0x0b3c && pid == 0xf000)
			write_3g_conf(fp, SN_Olicard_145, 1, vid, pid);
		else if(vid == 0x0b3c && pid == 0xc700)
			write_3g_conf(fp, SN_Olicard_100, 1, vid, pid);
		else if(vid == 0x0d46 && pid == 0x45a5)
			write_3g_conf(fp, SN_Kobil_mIdentity_3G_2, 1, vid, pid);
		else if(vid == 0x0d46 && pid == 0x45a1)
			write_3g_conf(fp, SN_Kobil_mIdentity_3G_1, 1, vid, pid);
		else if(vid == 0x0cf3 && pid == 0x20ff)
			write_3g_conf(fp, SN_Netgear_WNDA3200, 1, vid, pid);
		else if(vid == 0x0fce && pid == 0xd0cf)
			write_3g_conf(fp, SN_SE_MD300, 1, vid, pid);
		else if(vid == 0x1266 && pid == 0x1000)
			write_3g_conf(fp, SN_Digicom_8E4455, 1, vid, pid);
		else if(vid == 0x0482 && pid == 0x024d)
			write_3g_conf(fp, SN_Kyocera_W06K, 1, vid, pid);
		else if(vid == 0x198f && pid == 0xbccd)
			write_3g_conf(fp, SN_Beceem_BCSM250, 1, vid, pid);
		else if(vid == 0x0b05 && pid == 0xbccd)
			write_3g_conf(fp, SN_Beceem_BCSM250_ASUS, 1, vid, pid);
		else if(vid == 0x057c && pid == 0x84ff)
			write_3g_conf(fp, SN_AVM_Fritz, 1, vid, pid);
		else if(vid == 0x057c && pid == 0x62ff)
			write_3g_conf(fp, SN_AVM_Fritz_V2, 1, vid, pid);
		else if(vid == 0x2020 && pid == 0xf00e)
			write_3g_conf(fp, SN_SU_8000U, 1, vid, pid);
		else if(vid == 0x2020 && pid == 0x0002)
			write_3g_conf(fp, SN_Mediatek_MT6229, 1, vid, pid);
		else if(vid == 0x1307 && pid == 0x1169)
			write_3g_conf(fp, SN_Cisco_AM10, 1, vid, pid);
		else if(vid == 0x1da5 && pid == 0xf000)
			write_3g_conf(fp, SN_Qisda_H21, 1, vid, pid);
		else if(vid == 0x21f5 && pid == 0x1000)
			write_3g_conf(fp, SN_StrongRising_Air_FlexiNet, 1, vid, pid);
		else if(vid == 0x1a8d && pid == 0x2000)
			write_3g_conf(fp, SN_BandLuxe_C339, 1, vid, pid);
		else if(vid == 0x04bb && pid == 0xbccd)
			write_3g_conf(fp, SN_IODATA_WMX2_U, 1, vid, pid);
		else if(vid == 0x2077 && pid == 0xf000)
			write_3g_conf(fp, SN_T_W_WU160, 1, vid, pid);
		else if(vid == 0x0e8d && pid == 0x0002)
			write_3g_conf(fp, SN_MediaTek_MT6276M, 1, vid, pid);
		else if(vid == 0x22f4 && pid == 0x0021)
			write_3g_conf(fp, SN_Tata_Photon_plus, 1, vid, pid);
		else if(vid == 0x109b && pid == 0xf009)
			write_3g_conf(fp, SN_HISENSE_E910, 1, vid, pid);
		else if(vid == 0x22de && pid == 0x6803)
			write_3g_conf(fp, SN_WeTelecom_WMD300, 1, vid, pid);
		else if(vid == 0x22de && pid == 0x6801)
			write_3g_conf(fp, SN_WeTelecom_WMD200, 1, vid, pid);
		else if(vid == 0x10a9 && pid == 0x6080)
			write_3g_conf(fp, SN_Pantech_LTE, 1, vid, pid);
		else if(vid == 0x0fca && pid == 0x8020)
			write_3g_conf(fp, SN_BlackBerry_Q10, 1, vid, pid);
		else if(vid == 0x2357 && pid == 0xf000)
			write_3g_conf(fp, SN_TP_Link_MA260, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x155b)
			write_3g_conf(fp, SN_Huawei_E3131, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x157d)
			write_3g_conf(fp, SN_Huawei_E3372, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x15ca)
			write_3g_conf(fp, SN_Huawei_E303u, 1, vid, pid);
		else if(vid == 0x12d1 && pid == 0x15cd)
			write_3g_conf(fp, SN_Huawei_E3531s, 1, vid, pid);
		else if(vid == 0x12d1)
			write_3g_conf(fp, UNKNOWNDEV, 1, vid, pid);
		else{
			fclose(fp);
			unlink(conf_file);
			return 0;
		}
	}
	else	/* manaul setting */
	{
usb_dbg("3G: manaul setting.\n");
		nvram_set("d3g", dongle_name);

		if(strcmp(dongle_name, "MU-Q101") == 0){					// on list
			write_3g_conf(fp, SN_MU_Q101, 0, vid, pid);
		} else if (strcmp(dongle_name, "ASUS-T500") == 0 && vid == 0x0b05){				// on list
			write_3g_conf(fp, UNKNOWNDEV, 0, vid, pid);
		//} else if (strcmp(dongle_name, "OPTION-ICON225") == 0){			// on list
		//	write_3g_conf(fp, SN_OPTION_ICON225, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-GlobeSurfer-Icon") == 0){
			write_3g_conf(fp, SN_Option_GlobeSurfer_Icon, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-GlobeSurfer-Icon-7.2") == 0){
			write_3g_conf(fp, SN_Option_GlobeSurfer_Icon72, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-GlobeTrotter-GT-MAX-3.6") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_GT_MAX36, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-GlobeTrotter-GT-MAX-7.2") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_GT_MAX72, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-GlobeTrotter-EXPRESS-7.2") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_EXPRESS72, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-iCON-210") == 0){
			write_3g_conf(fp, SN_Option_iCON210, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-GlobeTrotter-HSUPA-Modem") == 0){
			write_3g_conf(fp, SN_Option_GlobeTrotter_HSUPA_Modem, 0, vid, pid);
		} else if (strcmp(dongle_name, "Option-iCON-401") == 0){
			write_3g_conf(fp, SN_Option_iCON401, 0, vid, pid);
		} else if (strcmp(dongle_name, "Vodafone-K3760") == 0){
			write_3g_conf(fp, SN_Vodafone_K3760, 0, vid, pid);
		} else if (strcmp(dongle_name, "ATT-USBConnect-Quicksilver") == 0){
			write_3g_conf(fp, SN_ATT_USBConnect_Quicksilver, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E169") == 0){			// on list
			write_3g_conf(fp, SN_Huawei_E169, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E180") == 0){			// on list
			//write_3g_conf(fp, SN_Huawei_E180, 0, vid, pid);
			write_3g_conf(fp, SN_Huawei_E220, 1, vid, pid);		// E180:12d1/1003 (as E220)
		} else if (strcmp(dongle_name, "Huawei-E220") == 0){			// on list
			write_3g_conf(fp, SN_Huawei_E220, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E630") == 0){
			write_3g_conf(fp, SN_Huawei_E630, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E270") == 0){
			write_3g_conf(fp, SN_Huawei_E270, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E1550") == 0){
			write_3g_conf(fp, SN_Huawei_E1550, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E161") == 0){
			write_3g_conf(fp, SN_Huawei_E1612, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E1612") == 0){
			write_3g_conf(fp, SN_Huawei_E1612, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E1690") == 0){
			write_3g_conf(fp, SN_Huawei_E1690, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-K3765") == 0){
			write_3g_conf(fp, SN_Huawei_K3765, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-K4505") == 0){
			write_3g_conf(fp, SN_Huawei_K4505, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E372") == 0){ // 20120919: From Lencer's test, must run EC156 to switch.
			write_3g_conf(fp, SN_Huawei_EC156, 0, vid, pid);
		} else if (strcmp(dongle_name, "Huawei-E173") == 0){
			write_3g_conf(fp, SN_Huawei_E173, 0, vid, pid);
		} else if (strcmp(dongle_name, "MTS-AC2746") == 0){ // Couldn't switch with usb_modeswitch.
			fclose(fp);
			unlink(conf_file);
			return 0;
		} else if (strcmp(dongle_name, "ZTE-MF620") == 0){
			write_3g_conf(fp, SN_ZTE_MF620, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-MF622") == 0){
			write_3g_conf(fp, SN_ZTE_MF622, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-MF628") == 0){
			write_3g_conf(fp, SN_ZTE_MF628, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-MF626") == 0){
			write_3g_conf(fp, SN_ZTE_MF626, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-AC8710") == 0){
			write_3g_conf(fp, SN_ZTE_AC8710, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-AC2710") == 0){
			write_3g_conf(fp, SN_ZTE_AC2710, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-6535-Z") == 0){
			write_3g_conf(fp, SN_ZTE6535_Z, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-K3520-Z") == 0){
			write_3g_conf(fp, SN_ZTE_K3520_Z, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-MF110") == 0){
			write_3g_conf(fp, SN_ZTE_MF110, 0, vid, pid);
		} else if (strcmp(dongle_name, "ZTE-K3565") == 0){
			write_3g_conf(fp, SN_ZTE_K3565, 0, vid, pid);
		} else if (strcmp(dongle_name, "ONDA-MT503HS") == 0){
			write_3g_conf(fp, SN_ONDA_MT503HS, 0, vid, pid);
		} else if (strcmp(dongle_name, "ONDA-MT505UP") == 0){
			write_3g_conf(fp, SN_ONDA_MT505UP, 0, vid, pid);
		} else if (strcmp(dongle_name, "Novatel-Wireless-Ovation-MC950D-HSUPA") == 0){
			write_3g_conf(fp, SN_Novatel_Wireless_Ovation_MC950D, 0, vid, pid);
		} else if (strcmp(dongle_name, "Novatel-U727") == 0){
			write_3g_conf(fp, SN_Novatel_U727, 0, vid, pid);
		} else if (strcmp(dongle_name, "Novatel-MC990D") == 0){
			write_3g_conf(fp, SN_Novatel_MC990D, 0, vid, pid);
		} else if (strcmp(dongle_name, "Novatel-U760") == 0){
			write_3g_conf(fp, SN_Novatel_U760, 0, vid, pid);
		} else if (strcmp(dongle_name, "Alcatel-X020") == 0){
			write_3g_conf(fp, SN_Alcatel_X020, 0, vid, pid);
		} else if (strcmp(dongle_name, "Alcatel-X200") == 0){
			write_3g_conf(fp, SN_Alcatel_X200, 0, vid, pid);
		} else if (strcmp(dongle_name, "AnyDATA-ADU-500A") == 0){
			write_3g_conf(fp, SN_AnyDATA_ADU_500A, 0, vid, pid);
		} else if (strncmp(dongle_name, "BandLuxe-", 9) == 0){			// on list
			fclose(fp);
			unlink(conf_file);
			return 0;
		} else if (strcmp(dongle_name, "Solomon-S3Gm-660") == 0){
			write_3g_conf(fp, SN_Solomon_S3Gm660, 0, vid, pid);
		} else if (strcmp(dongle_name, "C-motechD-50") == 0){
			write_3g_conf(fp, SN_C_motechD50, 0, vid, pid);
		} else if (strcmp(dongle_name, "C-motech-CGU-628") == 0){
			write_3g_conf(fp, SN_C_motech_CGU628, 0, vid, pid);
		} else if (strcmp(dongle_name, "Toshiba-G450") == 0){
			write_3g_conf(fp, SN_Toshiba_G450, 0, vid, pid);
		} else if (strcmp(dongle_name, "UTStarcom-UM175") == 0){
			write_3g_conf(fp, SN_UTStarcom_UM175, 0, vid, pid);
		} else if (strcmp(dongle_name, "Hummer-DTM5731") == 0){
			write_3g_conf(fp, SN_Hummer_DTM5731, 0, vid, pid);
		} else if (strcmp(dongle_name, "A-Link-3GU") == 0){
			write_3g_conf(fp, SN_A_Link_3GU, 0, vid, pid);
		} else if (strcmp(dongle_name, "Sierra-Wireless-Compass-597") == 0){
			write_3g_conf(fp, SN_Sierra_Wireless_Compass597, 0, vid, pid);
		} else if (strcmp(dongle_name, "Sierra-881U") == 0){
			write_3g_conf(fp, SN_Sierra881U, 0, vid, pid);
		} else if (strcmp(dongle_name, "Sony-Ericsson-MD400") == 0){
			write_3g_conf(fp, SN_Sony_Ericsson_MD400, 0, vid, pid);
		} else if (strcmp(dongle_name, "LG-LDU-1900D") == 0){
			write_3g_conf(fp, SN_LG_LDU_1900D, 0, vid, pid);
		} else if (strcmp(dongle_name, "Samsung-SGH-Z810") == 0){
			write_3g_conf(fp, SN_Samsung_SGH_Z810, 0, vid, pid);
		} else if (strcmp(dongle_name, "MobiData-MBD-200HU") == 0){
			write_3g_conf(fp, SN_MobiData_MBD_200HU, 0, vid, pid);
		} else if (strcmp(dongle_name, "ST-Mobile") == 0){
			write_3g_conf(fp, SN_ST_Mobile, 0, vid, pid);
		} else if (strcmp(dongle_name, "MyWave-SW006") == 0){
			write_3g_conf(fp, SN_MyWave_SW006, 0, vid, pid);
		} else if (strcmp(dongle_name, "Cricket-A600") == 0){
			write_3g_conf(fp, SN_Cricket_A600, 0, vid, pid);
		} else if (strcmp(dongle_name, "EpiValley-SEC-7089") == 0){
			write_3g_conf(fp, SN_EpiValley_SEC7089, 0, vid, pid);
		} else if (strcmp(dongle_name, "Samsung-U209") == 0){
			write_3g_conf(fp, SN_Samsung_U209, 0, vid, pid);
		} else if (strcmp(dongle_name, "D-Link-DWM-162-U5") == 0){
			write_3g_conf(fp, SN_D_Link_DWM162_U5, 0, vid, pid);
		} else if (strcmp(dongle_name, "Novatel-MC760") == 0){
			write_3g_conf(fp, SN_Novatel_MC760, 0, vid, pid);
		} else if (strcmp(dongle_name, "Philips-TalkTalk") == 0){
			write_3g_conf(fp, SN_Philips_TalkTalk, 0, vid, pid);
		} else if (strcmp(dongle_name, "HuaXing-E600") == 0){
			write_3g_conf(fp, SN_HuaXing_E600, 0, vid, pid);
		} else if (strcmp(dongle_name, "C-motech-CHU-629S") == 0){
			write_3g_conf(fp, SN_C_motech_CHU_629S, 0, vid, pid);
		} else if (strcmp(dongle_name, "Sagem-9520") == 0){
			write_3g_conf(fp, SN_Sagem9520, 0, vid, pid);
		} else if (strcmp(dongle_name, "Nokia-CS15") == 0){
			write_3g_conf(fp, SN_Nokia_CS15, 0, vid, pid);
		} else if (strcmp(dongle_name, "Nokia-CS17") == 0){
			write_3g_conf(fp, SN_Nokia_CS17, 0, vid, pid);
		} else if (strcmp(dongle_name, "Vodafone-MD950") == 0){
			write_3g_conf(fp, SN_Vodafone_MD950, 0, vid, pid);
		} else if (strcmp(dongle_name, "Siptune-LM-75") == 0){
			write_3g_conf(fp, SN_Siptune_LM75, 0, vid, pid);
		} else if (strcmp(dongle_name, "Celot_CT-680") == 0){               // 0703 add 
			write_3g_conf(fp, SN_Celot_CT680, 0, vid, pid);
		} else if (strcmp(dongle_name, "Celot_CT-K300") == 0){              // 0703 add 
			write_3g_conf(fp, SN_Celot_K300, 0, vid, pid);
		} else if (strncmp(dongle_name, "Huawei-", 7) == 0 && vid == 0x12d1){
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

int write_3g_ppp_conf(void){
	char modem_node[16];
	FILE *fp;
	char usb_node[32];
	unsigned int vid, pid;
	int wan_unit;
	char prefix[] = "wanXXXXXXXXXX_", tmp[100];
	int retry, lock;
#ifdef SET_USB_MODEM_MTU_PPP
	int modem_mtu;
#endif

	snprintf(modem_node, 16, "%s", nvram_safe_get("usb_modem_act_int"));
	if(strlen(modem_node) <= 0){
		usb_dbg(": Fail to get the act modem node.\n");
		return 0;
	}

	if(get_device_type_by_device(modem_node) != DEVICE_TYPE_MODEM){
		usb_dbg("(%s): test 1.\n", modem_node);
		return 0;
	}

	if ((wan_unit = get_usbif_dualwan_unit()) < 0) {
		usb_dbg("(%s): Don't enable the USB interface as the WAN type yet.\n", modem_node);
		return 0;
	}

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
	if(get_usb_node_by_device(modem_node, usb_node, 32) == NULL){
		usb_dbg("(%s): test 5.\n", modem_node);
		file_unlock(lock);
		return 0;
	}

	// Get VID.
	if((vid = get_usb_vid(usb_node)) == 0){
		usb_dbg("(%s): test 6.\n", modem_node);
		file_unlock(lock);
		return 0;
	}

	// Get PID.
	if((pid = get_usb_pid(usb_node)) == 0){
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
#ifdef SET_USB_MODEM_MTU_PPP
	modem_mtu = nvram_get_int("modem_mtu");
	if (modem_mtu >= 576)
		fprintf(fp, "mtu %d\n", modem_mtu);
#endif
	if(!strcmp(modem_enable, "2")){
		fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/EVDO_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		fprintf(fp, "disconnect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/EVDO_disconn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
	}
	else if(!strcmp(modem_enable, "3")){
		fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/td_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		fprintf(fp, "disconnect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/Generic_disconn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
	}
	else{
		if(vid == 0x0b05 && pid == 0x0302) // T500
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/t500_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		else if(vid == 0x0421 && pid == 0x0612) // CS-15
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/t500_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		else if(vid == 0x106c && pid == 0x3716)
			fprintf(fp, "connect \"/bin/comgt -d /dev/%s -s %s/ppp/3g/verizon_conn.scr\"\n", modem_node, MODEM_SCRIPTS_DIR);
		else if(vid == 0x1410 && pid == 0x4400)
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
	fprintf(fp, "IPRefreshCommand                  'udhcpc -i %s -p /var/run/udhcpc%d.pid -s /tmp/udhcpc &'\n", eth_node, get_wan_unit((char*)eth_node));
	fprintf(fp, "FirmwareFileName                  '/lib/firmware/macxvi200.bin'\n");
	fprintf(fp, "ConfigFileName                    '/lib/firmware/macxvi.cfg'\n");
	fprintf(fp, "BandwidthMHz                      10\n");
	fprintf(fp, "NetworkSearchTimeoutSec           10\n");

	if(!strcmp(isp, "Yota")){
		fprintf(fp, "CenterFrequencyMHz                2505 2515 2525 2625\n");
		fprintf(fp, "EAPMethod                         4\n");
		fprintf(fp, "ValidateServerCert                Yes\n");
		fprintf(fp, "CACertFileName                    '/tmp/Beceem_firmware/Server_CA.pem'\n");
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

int write_gct_conf(void)
{
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

#ifdef RTCONFIG_BCMARM

int hotplug_pid = -1;
FILE *fp = NULL;
#define hotplug_dbg(fmt, args...) (\
{ \
	char err_str[100] = {0}; \
	char err_str2[100] = {0}; \
	if (hotplug_pid == -1) hotplug_pid = getpid(); \
	if (!fp) fp = fopen("/tmp/usb_err", "a+"); \
	sprintf(err_str, fmt, ##args); \
	sprintf(err_str2, "PID:%d %s", hotplug_pid, err_str); \
	fwrite(err_str2, strlen(err_str2), 1,  fp); \
	fflush(fp); \
} \
)

#define LOCK_FILE      "/tmp/hotplug_block_lock"

int hotplug_usb_power(int port, int boolOn)
{
        char name[] = "usbport%d"; /* 1 ~ 99 ports */
        unsigned long gpiomap;
        int usb_gpio;

        if (port > MAX_USB_PORT)
                return -1;

        sprintf(name, "usbport%d", port);
        usb_gpio = bcmgpio_getpin(name, BCMGPIO_UNDEFINED);
        if (usb_gpio == BCMGPIO_UNDEFINED)
                return 0;
        if (bcmgpio_connect(usb_gpio, BCMGPIO_DIRN_OUT))
                return 0;

        gpiomap = (1 << usb_gpio);
        bcmgpio_out(gpiomap, boolOn? gpiomap: 0);
        bcmgpio_disconnect(usb_gpio);
        return 1;
}

/* Return number of usbports enabled */
int hotplug_usb_init(void)
{
        /* Enable VBUS via GPIOs for port1 and port2 */
        int i, count;

        for (count = 0, i = 1; i <= MAX_USB_PORT; i++) {
                if (hotplug_usb_power(i, TRUE) > 0)
                        count++;
        }
        return count;
}

#define PHYSDEVPATH_DEPTH_USB_LPORT     4       /* Depth is from 0 */

#if 0
static void
hotplug_usb_power_recover(void)
{
        char *physdevpath;
        char *tok = NULL;
        int depth = 0, root_hub_num = -1, usb_lport = -1, nv_port = -1;

        if ((physdevpath = getenv("PHYSDEVPATH")) == NULL)
                return;

	/* Get root_hub and logical port number.
	 * PHYSDEVPATH in sysfs is /devices/pci/hcd/root_hub/root_hub_num-lport/.....
	 */
        tok = strtok(physdevpath, "/");

        while (tok != NULL) {
                if (PHYSDEVPATH_DEPTH_USB_LPORT == depth) {
                        sscanf(tok, "%d-%d", &root_hub_num, &usb_lport);
                        break;
                }
                depth++;
                tok = strtok(NULL, "/");
        }

        if (tok && (root_hub_num != -1) && (usb_lport != -1)) {
                /* Translate logical port to nvram setting port */
                nv_port = (usb_lport == 1) ? 2 : 1;

                hotplug_usb_power(nv_port, FALSE);
                sleep(1);
                hotplug_usb_power(nv_port, TRUE);
        }
}
#endif

/* Optimize performance */
#define READ_AHEAD_KB_BUF       1024
#define READ_AHEAD_CONF "/sys/block/%s/queue/read_ahead_kb"

static void
optimize_block_device(const char *devname)
{
        char blkdev[8] = {0};
        char read_ahead_conf[64] = {0};
        char cmdbuf[64] = {0};
        int err;

        memset(blkdev, 0, sizeof(blkdev));
        strncpy(blkdev, devname, 3);
        sprintf(read_ahead_conf, READ_AHEAD_CONF, blkdev);
        sprintf(cmdbuf, "echo %d > %s", READ_AHEAD_KB_BUF, read_ahead_conf);
        err = system(cmdbuf);
        hotplug_dbg("err = %d\n", err);

        if (err) {
                hotplug_dbg("read ahead: unsuccess %d!\n", err);
        }
}

int
hotplug_block(void)
{
        char *action = NULL, *minor = NULL;
        char *major = NULL, *driver = NULL;
        int retry = 3, lock_fd = -1;
        struct flock lk_info = {0};

        if (!(action = getenv("ACTION")) ||
            !(minor = getenv("MINOR")) ||
            !(driver = getenv("PHYSDEVDRIVER")) ||
            !(major = getenv("MAJOR")))
        {
                return EINVAL;
        }

        hotplug_dbg("env %s %s!\n", action, driver);
        if (strncmp(driver, "sd", 2))
        {
                return EINVAL;
        }

        if ((lock_fd = open(LOCK_FILE, O_RDWR|O_CREAT, 0666)) < 0) {
                hotplug_dbg("Failed opening lock file LOCK_FILE: %s\n", strerror(errno));
                return -1;
        }

        while (--retry) {
                lk_info.l_type = F_WRLCK;
                lk_info.l_whence = SEEK_SET;
                lk_info.l_start = 0;
                lk_info.l_len = 0;
                if (!fcntl(lock_fd, F_SETLKW, &lk_info)) break;
        }

        if (!retry) {
                hotplug_dbg("Failed locking LOCK_FILE: %s\n", strerror(errno));
                return -1;
        }

        if (!strcmp(action, "add")) {
                hotplug_dbg("add block!\n");

        } else if (!strcmp(action, "remove")) {
                hotplug_dbg("usb power recover!\n");
#if 0
		hotplug_usb_power_recover(); // Mark off this. It will let the modem can't be switched.
#endif
        } else {
                hotplug_dbg("unsupported action!\n");
        }

        close(lock_fd);
        unlink(LOCK_FILE);
        return 0;
}
#endif  /* RTCONFIG_BCMARM */
#endif // RTCONFIG_USB

// 1: add, 0: remove.
int check_hotplug_action(const char *action){
	if(!strcmp(action, "remove"))
		return 0;
	else
		return 1;
}

int set_usb_common_nvram(const char *action, const char *device_name, const char *usb_node, const char *known_type){
	char type[16], vid[8], pid[8], manufacturer[256], product[256], serial[256], speed[256];
	char been_type[16];
	char port_path[8];
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	char *ptr;
	unsigned int val;

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		return 0;
	}

	if(!check_hotplug_action(action)){
		if(known_type == NULL){
			snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
			snprintf(been_type, 16, "%s", nvram_safe_get(prefix));

			if(strcmp(been_type, "storage") || strlen(nvram_safe_get(strcat_r(prefix, "_speed", tmp))) <= 0)
				nvram_unset(prefix);
			if(strlen(nvram_safe_get(strcat_r(prefix, "_vid", tmp))) > 0)
				nvram_unset(tmp);
			if(strlen(nvram_safe_get(strcat_r(prefix, "_pid", tmp))) > 0)
				nvram_unset(tmp);
			if(strlen(nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp))) > 0)
				nvram_unset(tmp);
			if(strlen(nvram_safe_get(strcat_r(prefix, "_product", tmp))) > 0)
				nvram_unset(tmp);
			if(strlen(nvram_safe_get(strcat_r(prefix, "_serial", tmp))) > 0)
				nvram_unset(tmp);
			if(strcmp(been_type, "storage") && strlen(nvram_safe_get(strcat_r(prefix, "_speed", tmp))) > 0)
				nvram_unset(tmp);
			if(strlen(nvram_safe_get(strcat_r(prefix, "_node", tmp))) > 0)
				nvram_unset(tmp);
			if(strlen(nvram_safe_get(strcat_r(prefix, "_act_def", tmp))) > 0)
				nvram_unset(tmp);
		}
		else{
			if(!strcmp(known_type, "storage")){
				ptr = (char *)device_name+strlen(device_name)-1;
				if(!isdigit(*ptr)){ // disk
					snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
					if(strlen(nvram_safe_get(strcat_r(prefix, "_vid", tmp))) <= 0)
						nvram_unset(prefix);
					// for ATE. {
					// Jerry5Chang added for unmount case. 2012.12.03
					nvram_unset(strcat_r(prefix, "_removed", tmp));
					nvram_unset(strcat_r(prefix, "_act", tmp)); // for DM.
					nvram_unset(strcat_r(prefix, "_fs_path0", tmp));
#ifdef RTCONFIG_DISK_MONITOR
					nvram_unset(strcat_r(prefix, "_pool_error", tmp));
#endif
					nvram_unset(strcat_r(prefix, "_speed", tmp));
					// for ATE. }
				}
				else if(!strncmp(device_name, "mmcblk", 6)){ // SD card.
					snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
					nvram_unset(prefix);
					nvram_unset(strcat_r(prefix, "_node", tmp));
					// for ATE. {
					// Jerry5Chang added for unmount case. 2012.12.03
					nvram_unset(strcat_r(prefix, "_removed", tmp));
					nvram_unset(strcat_r(prefix, "_act", tmp)); // for DM.
					nvram_unset(strcat_r(prefix, "_fs_path0", tmp));
#ifdef RTCONFIG_DISK_MONITOR
					nvram_unset(strcat_r(prefix, "_pool_error", tmp));
#endif
					nvram_unset(strcat_r(prefix, "_speed", tmp));
					// for ATE. }
				}
			}
		}
	}
	else{
		snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
		memset(been_type, 0, 16);
		strcpy(been_type, nvram_safe_get(prefix));
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
			{ // unknown device.
				return 0;
			}
		}
		if(known_type != NULL)
			nvram_set(prefix, known_type);
		else if(get_device_type_by_node(usb_node, type, 16) != NULL)
			nvram_set(prefix, type);
		else // unknown device.
			return 0;

		strcat_r(prefix, "_vid", tmp);
		if ((val = get_usb_vid(usb_node)) != 0) {
			snprintf(vid, sizeof(vid), "%04x", val);
			nvram_set(tmp, vid);
		}

		strcat_r(prefix, "_pid", tmp);
		if ((val = get_usb_pid(usb_node)) != 0) {
			snprintf(pid, sizeof(pid), "%04x", val);
			nvram_set(tmp, pid);
		}

		strcat_r(prefix, "_manufacturer", tmp);
		if(get_usb_manufacturer(usb_node, manufacturer, 256) != NULL && strlen(manufacturer) > 0)
			nvram_set(tmp, manufacturer);

		strcat_r(prefix, "_product", tmp);
		if(get_usb_product(usb_node, product, 256) != NULL && strlen(product) > 0)
			nvram_set(tmp, product);

		strcat_r(prefix, "_serial", tmp);
		if(get_usb_serial(usb_node, serial, 256) != NULL && strlen(serial) > 0)
			nvram_set(tmp, serial);

		strcat_r(prefix, "_speed", tmp);
		if(get_usb_speed(usb_node, speed, 256) != NULL && strlen(speed) > 0)
			nvram_set(tmp, speed);

		strcat_r(prefix, "_node", tmp);
		nvram_set(tmp, usb_node);
	}

	return 0;
}

#ifdef BCM_MMC
int asus_mmc(const char *device_name, const char *action){
#ifdef RTCONFIG_USB
	char usb_port[32];
	int isLock;
	char env_dev[64], env_major[64], env_port[64];
	char port_path[8];
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	char *ptr;

	usb_dbg("(%s): action=%s.\n", device_name, action);

	if(!strcmp(nvram_safe_get("stop_mmc"), "1")){
		usb_dbg("(%s): stop_mmc be set.\n", device_name);
		return 0;
	}

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_DISK){
		usb_dbg("(%s): The device is not a mmc device.\n", device_name);
		return 0;
	}

	// Check Lock.
	if((isLock = file_lock((char *)device_name)) == -1){
		usb_dbg("(%s): Can't set the file lock!\n", device_name);
		return 0;
	}

	ptr = (char *)device_name+strlen(device_name)-1;

	snprintf(usb_port, 32, "%s", SDCARD_PORT);

	if(!check_hotplug_action(action)){
		snprintf(prefix, sizeof(prefix), "usb_path_%s", device_name);
		nvram_unset(strcat_r(prefix, "_label", tmp));

		if(get_path_by_node(usb_port, port_path, 8) == NULL){
			usb_dbg("(%s): Fail to get usb path.\n", usb_port);
			file_unlock(isLock);
			return 0;
		}

		set_usb_common_nvram(action, device_name, usb_port, "storage");

		usb_dbg("(%s): Remove MMC.\n", device_name);

		putenv("INTERFACE=8/0/0");
		putenv("ACTION=remove");
		putenv("PRODUCT=asus_mmc");
		snprintf(env_dev, 64, "DEVICENAME=%s", device_name);
		putenv(env_dev);
		putenv("SUBSYSTEM=block");
		snprintf(env_major, 64, "MAJOR=%d", USB_DISK_MAJOR);
		putenv(env_major);
		putenv("PHYSDEVBUS=scsi");

		eval("hotplug", "block");

		unsetenv("INTERFACE");
		unsetenv("ACTION");
		unsetenv("PRODUCT");
		unsetenv("DEVICENAME");
		unsetenv("SUBSYSTEM");
		unsetenv("MAJOR");
		unsetenv("PHYSDEVBUS");

#ifdef RTCONFIG_MMC_LED
		led_control(LED_MMC, LED_OFF);
#endif

		file_unlock(isLock);
		return 0;
	}

	// there is a static usb node with the SD card, so don't need to set the usb_node record.
	if(get_path_by_node(usb_port, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

	// There is only a hotplug with the partition of the SD card.
	//if(!isdigit(*ptr)){ // disk
		// set USB common nvram.
		set_usb_common_nvram(action, device_name, usb_port, "storage");

		// for DM.
		if(!strcmp(nvram_safe_get(prefix), "storage")){
			nvram_set(strcat_r(prefix, "_act", tmp), device_name);
		}
	//}

	putenv("INTERFACE=8/0/0");
	putenv("ACTION=add");
	putenv("PRODUCT=asus_mmc");
	snprintf(env_dev, 64, "DEVICENAME=%s", device_name);
	putenv(env_dev);
	putenv("SUBSYSTEM=block");
	snprintf(env_port, 64, "USBPORT=%s", usb_port);
	putenv(env_port);
	snprintf(env_major, 64, "MAJOR=%d", USB_DISK_MAJOR);
	putenv(env_major);
	putenv("PHYSDEVBUS=scsi");

	eval("hotplug", "block");

	unsetenv("INTERFACE");
	unsetenv("ACTION");
	unsetenv("PRODUCT");
	unsetenv("DEVICENAME");
	unsetenv("SUBSYSTEM");
	unsetenv("USBPORT");
	unsetenv("MAJOR");
	unsetenv("PHYSDEVBUS");

#ifdef RTCONFIG_MMC_LED
	led_control(LED_MMC, LED_ON);
#endif

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);

#endif
	return 1;
}
#endif

int asus_sd(const char *device_name, const char *action){
#ifdef RTCONFIG_USB
	char usb_node[32], usb_port[32];
	int isLock;
	char nvram_value[32];
	char env_dev[64], env_major[64], env_port[64];
	char port_path[8];
	char buf1[32];
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
#ifdef RTCONFIG_XHCIMODE
	int usb2mode = 0;
	unsigned int vid, pid;
	char speed[256];
#endif
	char *ptr;
	int model = get_model();
#ifdef RTCONFIG_USB_PRINTER
	int retry;
#endif

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

	ptr = (char *)device_name+strlen(device_name)-1;

	// If remove the device?
	if(!check_hotplug_action(action)){
		memset(buf1, 0, 32);
		sprintf(buf1, "usb_path_%s", device_name);
		memset(usb_node, 0, 32);
		strcpy(usb_node, nvram_safe_get(buf1));
		nvram_unset(buf1);
		nvram_unset(strcat_r(buf1, "_label", tmp));

		snprintf(nvram_value, 32, "%s", nvram_safe_get("usb_modem_act_reset_path"));
		if(!strcmp(nvram_value, usb_node)){
			usb_dbg("(%s): the device is resetting...(%s)\n", device_name, action);
			file_unlock(isLock);
			return 0;
		}

		if(get_path_by_node(usb_node, port_path, 8) == NULL){
			usb_dbg("(%s): Fail to get usb path.\n", usb_node);
			file_unlock(isLock);
			return 0;
		}

		snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

#ifdef RTCONFIG_XHCIMODE
		if(atoi(port_path) == 1 && nvram_get_int("usb_usb3") != 1 && !isdigit(*ptr)){
			// Get the xhci mode.
			if(f_read_string("/sys/module/xhci_hcd/parameters/usb2mode", buf1, 32) <= 0){
				usb_dbg("(%s): Fail to get xhci mode.\n", device_name);
				file_unlock(isLock);
				return 0;
			}
			usb2mode = atoi(buf1);

			snprintf(nvram_value, 32, "%s", nvram_safe_get(strcat_r(prefix, "_speed", tmp)));
			if(usb2mode != 0 && strlen(nvram_value) > 0 && strcmp(nvram_value, "5000"))
				notify_rc("restart_xhcimode 0");
		}
#endif

#ifdef RTCONFIG_USB_MODEM
		snprintf(buf1, 32, "%s.%s", USB_MODESWITCH_CONF, port_path);
		unlink(buf1);
#endif

		if(strlen(usb_node) > 0){
			// for the storage interface of the second modem.
			if(!strcmp(nvram_safe_get(prefix), "modem") && strcmp(usb_node, nvram_safe_get("usb_modem_act_path"))){
				usb_dbg("(%s): Reset the usb path nvram.\n", device_name);
				set_usb_common_nvram(action, device_name, usb_node, NULL);
			}
			else if(!strcmp(nvram_safe_get(prefix), "storage") && !isdigit(*ptr))
				set_usb_common_nvram(action, device_name, usb_node, "storage");
			usb_dbg("(%s): Remove Storage on USB node %s.\n", device_name, usb_node);
		}
		else
			usb_dbg("(%s): Remove a unknown-port Storage.\n", device_name);

		putenv("INTERFACE=8/0/0");
		putenv("ACTION=remove");
		putenv("PRODUCT=asus_sd");
		snprintf(env_dev, 64, "DEVICENAME=%s", device_name);
		putenv(env_dev);
		putenv("SUBSYSTEM=block");
		snprintf(env_major, 64, "MAJOR=%d", USB_DISK_MAJOR);
		putenv(env_major);
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

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 32) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}
	else
		usb_dbg("(%s): Got usb node: %s.\n", device_name, usb_node);

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_string(usb_node, usb_port, 32) == NULL){
		usb_dbg("(%s): Fail to get usb port.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}
	else
		usb_dbg("(%s): Got usb port: %s.\n", device_name, usb_port);

	memset(buf1, 0, 32);
	sprintf(buf1, "usb_path_%s", device_name);
	nvram_set(buf1, usb_node);

#ifdef RTCONFIG_XHCIMODE
	if(get_usb_speed(usb_node, speed, 256) != NULL && strcmp(speed, "5000")){
		goto after_change_xhcimode;
	}

	// Get the xhci mode.
	if(f_read_string("/sys/module/xhci_hcd/parameters/usb2mode", buf1, 32) <= 0){
		usb_dbg("(%s): Fail to get xhci mode.\n", device_name);
		file_unlock(isLock);
		return 0;
	}
	usb2mode = atoi(buf1);

	if(nvram_get_int("stop_xhcimode") == 1)
		usb_dbg("(%s): stop the xhci mode.\n", device_name);
	else if(nvram_get_int("usb_usb3") != 1){
		// Get VID & PID
		if ((vid = get_usb_vid(usb_node)) == 0 ||
		    (pid = get_usb_pid(usb_node)) == 0) {
			usb_dbg("(%s): Fail to get VID/PID of USB(%s).\n", device_name, usb_node);
			file_unlock(isLock);
			return 0;
		}

		if((vid == 0x0bc2 && (pid == 0xa0a1 || pid == 0x5031))
				|| (vid == 0x174c && pid == 0x5106)
				){
			if(usb2mode != 1){
				if(!isdigit(*ptr))
					notify_rc("restart_xhcimode 1");
				file_unlock(isLock);
				return 0;
			}
		}
		else if(usb2mode == 0 || usb2mode == 1){
			if(!isdigit(*ptr))
				notify_rc("restart_xhcimode 2");
			file_unlock(isLock);
			return 0;
		}
	}
	else if(usb2mode != 0){
		if(!isdigit(*ptr))
			notify_rc("restart_xhcimode 0");
		file_unlock(isLock);
		return 0;
	}

after_change_xhcimode:
#endif

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

	snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

	memset(nvram_value, 0, 32);
	strcpy(nvram_value, nvram_safe_get(prefix));
	if(strcmp(nvram_value, "") && strcmp(nvram_value, "storage")){
		usb_dbg("(%s): Had other interfaces(%s) on Port %s.\n", device_name, nvram_value, usb_port);
		file_unlock(isLock);
		return 0;
	}

	if(!isdigit(*ptr)){ // disk
		// set USB common nvram.
		set_usb_common_nvram(action, device_name, usb_node, "storage");

		// for DM.
		if(!strcmp(nvram_safe_get(prefix), "storage")){
			nvram_set(strcat_r(prefix, "_act", tmp), device_name);
		}
	}

	if (have_usb3_led(model)) {
		if (is_usb3_port(usb_node))
			nvram_set_int("usb_led_id", LED_USB3);
		else
			nvram_set_int("usb_led_id", LED_USB);
	}

	kill_pidfile_s("/var/run/usbled.pid", SIGUSR1);	// inform usbled to start blinking USB LED

	putenv("INTERFACE=8/0/0");
	putenv("ACTION=add");
	putenv("PRODUCT=asus_sd");
	snprintf(env_dev, 64, "DEVICENAME=%s", device_name);
	putenv(env_dev);
	putenv("SUBSYSTEM=block");
	snprintf(env_port, 64, "USBPORT=%s", usb_port);
	putenv(env_port);

	eval("hotplug", "block");

	kill_pidfile_s("/var/run/usbled.pid", SIGUSR2); // inform usbled to stop blinking USB LED
	if (have_usb3_led(model))
		nvram_unset("usb_led_id");

	unsetenv("INTERFACE");
	unsetenv("ACTION");
	unsetenv("PRODUCT");
	unsetenv("DEVICENAME");
	unsetenv("SUBSYSTEM");
	unsetenv("USBPORT");

#ifdef RTCONFIG_BCMARM
	/* Optimize performance */
	optimize_block_device(device_name);
#endif

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);

#endif // RTCONFIG_USB
	return 1;
}

int asus_lp(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_PRINTER
	char usb_node[32];
	int u2ec_fifo;
	int isLock;
	char port_path[8];
	char buf1[32];
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
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
		memset(buf1, 0, 32);
		sprintf(buf1, "usb_path_%s", device_name);
		memset(usb_node, 0, 32);
		strcpy(usb_node, nvram_safe_get(buf1));
		nvram_unset(buf1);

		if(strlen(usb_node) > 0){
			u2ec_fifo = open(U2EC_FIFO, O_WRONLY | O_NONBLOCK);
			write(u2ec_fifo, "r", 1);
			close(u2ec_fifo);

			set_usb_common_nvram(action, device_name, usb_node, "printer");
			usb_dbg("(%s): Remove Printer on USB node %s.\n", device_name, usb_node);
		}
		else
			usb_dbg("(%s): Remove a unknown-port Printer.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 32) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	memset(buf1, 0, 32);
	sprintf(buf1, "usb_path_%s", device_name);
	nvram_set(buf1, usb_node);

	// set USB common nvram.
	set_usb_common_nvram(action, device_name, usb_node, "printer");

	snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

	// Don't support the second printer device on a DUT.
	// Only see the other usb port.
	memset(buf1, 0, 32);
	strncpy(buf1, nvram_safe_get("u2ec_serial"), 32);
	if(strlen(buf1) > 0 && strcmp(nvram_safe_get(strcat_r(prefix, "_serial", tmp)), buf1)){
		// We would show the second printer device but didn't let it work.
		// Because it didn't set the nvram: usb_path%d_act.
		usb_dbg("(%s): Already had the printer device in the other USB port!\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	u2ec_fifo = open(U2EC_FIFO, O_WRONLY | O_NONBLOCK);
	write(u2ec_fifo, "a", 1);
	close(u2ec_fifo);

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_PRINTER
	return 1;
}

int asus_sg(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_MODEM
	char usb_node[32];
	unsigned int vid, pid;
	int isLock;
	char switch_file[32];
	char port_path[8];
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

	// Don't support the second modem device on a DUT.
	snprintf(nvram_value, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(nvram_value) > 0 && strcmp(nvram_value, usb_node)){
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

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 32) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%s", port_path);
	memset(nvram_value, 0, 32);
	strcpy(nvram_value, nvram_safe_get(nvram_name));
	//if(!strcmp(nvram_value, "printer") || !strcmp(nvram_value, "modem")){
	memset(switch_file, 0, 32);
	sprintf(switch_file, "%s.%s", USB_MODESWITCH_CONF, port_path);
	if(strcmp(nvram_value, "") && check_if_file_exist(switch_file)){
		usb_dbg("(%s): Already there was a other interface(%s).\n", usb_node, nvram_value);
		file_unlock(isLock);
		return 0;
	}

	// Get VID & PID
	if ((vid = get_usb_vid(usb_node)) == 0 ||
	    (pid = get_usb_pid(usb_node)) == 0) {
		usb_dbg("(%s): Fail to get VID/PID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// initial the config file of usb_modeswitch.
	/*if(!strcmp(nvram_safe_get("Dev3G"), "AUTO")
			&& (vid == 0x19d2 || vid == 0x1a8d)
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
	if(vid == 0x1199)
		; // had do usb_modeswitch before.
	else if(vid == 0x19d2 && pid == 0x0167) // MF821's modem mode.
		; // do nothing.
	else if(is_create_file_dongle(vid, pid))
		; // do nothing.
	else if(init_3g_param(port_path, vid, pid)){
		if(strcmp(nvram_safe_get("stop_sg_remove"), "1")){
			usb_dbg("(%s): Running usb_modeswitch...\n", device_name);
			xstart("usb_modeswitch", "-c", switch_file);
		}
	}
	else if(atoi(port_path) == 3)
		; // usb_path3 is worked for the built-in card reader.
	else{
		if(strcmp(nvram_safe_get("stop_sg_remove"), "1"))
		{
			usb_dbg("(%s): insmod cdrom!\n", device_name);
			modprobe("cdrom");
			modprobe("sr_mod");
			sleep(1); // wait the module be ready.
		}
	}

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_MODEM
	return 1;
}

int asus_sr(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_MODEM
	char usb_node[32];
	unsigned int vid, pid;
	int isLock;
	char eject_cmd[128];
	char port_path[8];
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

	// Don't support the second modem device on a DUT.
	snprintf(nvram_value, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(nvram_value) > 0 && strcmp(nvram_value, usb_node)){
		usb_dbg("(%s): Already had a running modem.\n", device_name);
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

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 32) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	snprintf(nvram_name, 32, "usb_path%s", port_path);
	snprintf(nvram_value, 32, "%s", nvram_safe_get(nvram_name));

	// Storage interface is first up with some composite devices,
	// so needs to wait that other interfaces wake up.
	int i;
	for(i = 0; i < 3; ++i){
		if(strcmp(nvram_value, "printer") && strcmp(nvram_value, "modem")){
			usb_dbg("%s: wait for the printer/modem interface...\n", __FUNCTION__);
			sleep(1);
			snprintf(nvram_value, 32, "%s", nvram_safe_get(nvram_name));
		}
		else
			break;
	}
	if(!strcmp(nvram_value, "printer") || !strcmp(nvram_value, "modem")){
		usb_dbg("(%s): Already there was a other interface(%s).\n", usb_node, nvram_value);
		file_unlock(isLock);
		return 0;
	}

	// Get VID & PID
	if ((vid = get_usb_vid(usb_node)) == 0 ||
	    (pid = get_usb_pid(usb_node)) == 0) {
		usb_dbg("(%s): Fail to get VID/PID of USB(%s).\n", device_name, usb_node);
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
	if(is_storage_cd(0, vid, pid)){
		usb_dbg("(%s): skip to eject CD...\n", device_name);
	}
	else if(strcmp(nvram_safe_get("stop_cd_remove"), "1")){
		usb_dbg("(%s): Running sdparm...\n", device_name);

		snprintf(eject_cmd, 128, "/dev/%s", device_name);
		eval("sdparm", "--command=eject", eject_cmd);
		sleep(1);

		if(find_sg_of_device(device_name, sg_device, sizeof(sg_device)) != NULL){
			snprintf(eject_cmd, 128, "/dev/%s", sg_device);
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
	char usb_node[32];
	unsigned int vid, pid;
	char *ptr, interface_name[16];
	int got_Int_endpoint = 0;
	int isLock = -1, isLock_tty = -1;
	char current_act[16], current_def[16];
	int cur_val, tmp_val;
	int retry;
	int wan_unit;
	char port_path[8];
	char buf1[32];
	char act_dev[8];
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
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
		snprintf(current_act, 16, "%s", nvram_safe_get("usb_modem_act_path"));
		snprintf(buf1, 32, "usb_path_%s", device_name);
		snprintf(usb_node, 32, "%s", nvram_safe_get(buf1));
		nvram_unset(buf1);
		snprintf(act_dev, 8, "%s", nvram_safe_get("usb_modem_act_dev"));

		usb_dbg("(%s): usb_modem_act_path=%s.\n", usb_node, current_act);
		if(!strcmp(current_act, usb_node)){
			if(get_path_by_node(usb_node, port_path, 8) == NULL){
				usb_dbg("(%s): Fail to get usb path.\n", usb_node);
				file_unlock(isLock);
				return 0;
			}

			if(strncmp(act_dev, "usb", 3) && strncmp(act_dev, "eth", 3)){
				snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
				nvram_unset(strcat_r(prefix, "_act", tmp));
				nvram_unset(strcat_r(prefix, "_act_def", tmp));

				clean_modem_state(1);
			}
			else
				clean_modem_state(0);

#if 0
			// TODO: for the bad CTF. After updating CTF, need to mark these codes.
			if(nvram_match("ctf_disable_modem", "1")){
				set_usb_common_nvram(action, device_name, usb_node, "modem");

				nvram_set("ctf_disable_modem", "0");
				nvram_commit();
				notify_rc_and_wait("reboot");
				file_unlock(isLock);

				return 0;
			}
#endif
			// Modem remove action.
			unlink(PPP_CONF_FOR_3G);

			if ((wan_unit = get_usbif_dualwan_unit()) < 0) {
				usb_dbg("(%s): in the current dual wan mode, didn't support the USB modem.\n", device_name);
				file_unlock(isLock);
				return 0;
			}

			/* Stop pppd */
			stop_pppd(wan_unit);

			killall_tk("usb_modeswitch");
			killall_tk("sdparm");

			retry = 0;
			while(hadOptionModule() && retry < 3){
				usb_dbg("(%s): Remove the option module.\n", device_name);
				modprobe_r("option");
				++retry;
			}

#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(2,6,36)
			retry = 0;
			while(hadWWANModule() && retry < 3){
				usb_dbg("(%s): Remove the USB WWAN module.\n", device_name);
				modprobe_r("usb_wwan");
				++retry;
			}
#endif

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
		}

		if(strlen(usb_node) > 0){
			set_usb_common_nvram(action, device_name, usb_node, "modem");
			usb_dbg("(%s): Remove the modem node on USB node %s.\n", device_name, usb_node);
		}
		else
			usb_dbg("(%s): Remove a unknown-port Modem.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 32) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	memset(buf1, 0, 32);
	sprintf(buf1, "usb_path_%s", device_name);
	nvram_set(buf1, usb_node);

	// Get VID & PID
	if ((vid = get_usb_vid(usb_node)) == 0 ||
	    (pid = get_usb_pid(usb_node)) == 0) {
		usb_dbg("(%s): Fail to get VID/PID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Don't support the second modem device on a DUT.
	// Only see the other usb port, because in the same port there are more modem interfaces and they need to compare.
	snprintf(buf1, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(buf1) > 0 && strcmp(buf1, usb_node)){
		// We would show the second modem device but didn't let it work.
		// Because it didn't set the nvram: usb_path%d_act.
		usb_dbg("(%s): Already had the modem device in the other USB port!\n", device_name);
		file_unlock(isLock);
		return 0;
	}
	else if(strlen(buf1) <= 0){
		nvram_set("usb_modem_act_path", usb_node);
		snprintf(buf1, 32, "%u",  vid);
		nvram_set("usb_modem_act_vid", buf1);
		snprintf(buf1, 32, "%u",  pid);
		nvram_set("usb_modem_act_pid", buf1);
	}

	// Find the control node of modem.
	// Get Interface name.
	if(get_interface_by_device(device_name, interface_name, sizeof(interface_name)) == NULL){
		usb_dbg("(%s): Fail to get usb interface.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

	// Check Lock and wait
	while((isLock_tty = file_lock((char *)__FUNCTION__)) == -1){
		usb_dbg("(%s): lock and wait!\n", device_name);
		usleep(5000);
	}
	// check the current working node of modem.
	memset(current_act, 0, 16);
	strcpy(current_act, nvram_safe_get(strcat_r(prefix, "_act", tmp)));
	memset(current_def, 0, 16);
	strcpy(current_def, nvram_safe_get(strcat_r(prefix, "_act_def", tmp)));
	usb_dbg("(%s): interface_name=%s, current_act=%s, current_def=%s.\n", device_name, interface_name, current_act, current_def);

	if(strlen(current_act) > 0 && get_device_type_by_device(current_act) != DEVICE_TYPE_MODEM && get_device_type_by_device(current_act) != DEVICE_TYPE_DISK) // for ECM, NCM, QMI...etc.
		;
	else if(isSerialNode(device_name)){
		// Find the endpoint: 03(Int).
		got_Int_endpoint = get_interface_Int_endpoint(interface_name);
		usb_dbg("(%s): got_Int_endpoint(%d)!\n", device_name, got_Int_endpoint);
		if(!got_Int_endpoint){
			// Set the default node be ttyUSB0, because there is no Int endpoint in some dongles.
			if(strcmp(device_name, "ttyUSB0")){
				usb_dbg("(%s): No Int endpoint!\n", device_name);
				file_unlock(isLock_tty);
				file_unlock(isLock);
				return 0;
			}
			else if(!strcmp(current_act, "") || get_device_type_by_device(current_act) == DEVICE_TYPE_DISK){
				usb_dbg("(%s): set_default_node!\n", device_name);
				nvram_set(strcat_r(prefix, "_act_def", tmp), "1");
				memset(current_def, 0, 16);
				strcpy(current_def, "1");
			}
			else {
				usb_dbg("(%s): has act(%s) already!\n", device_name, current_act);
				file_unlock(isLock_tty);
				file_unlock(isLock);
				return 0;
			}
		}

		if(vid == 0x0f3d && pid == 0x68aa){
			if(!strcmp(device_name, "ttyUSB3")){
				nvram_set(strcat_r(prefix, "_act", tmp), device_name);
				memset(current_act, 0, 16);
				strncpy(current_act, device_name, 16);
			}
			else{
				file_unlock(isLock_tty);
				file_unlock(isLock);
				return 0;
			}
		}
		else if(!strcmp(current_act, "") || get_device_type_by_device(current_act) == DEVICE_TYPE_DISK){
			nvram_set(strcat_r(prefix, "_act", tmp), device_name);
			memset(current_act, 0, 16);
			strncpy(current_act, device_name, 16);
		}
		else{
			cur_val = -1;
			if(strlen(current_act) > 6) {
				errno = 0;
				cur_val = strtol(&current_act[6], &ptr, 10);
				if(errno != 0 || &current_act[6] == ptr){
					cur_val = -1;
				}
			}

			errno = 0;
			tmp_val = strtol(&device_name[6], &ptr, 10);
			if(errno != 0 || &device_name[6] == ptr){
				usb_dbg("(%s): Can't get the working node.\n", device_name);
				file_unlock(isLock_tty);
				file_unlock(isLock);
				return 0;
			}
			else if(cur_val != -1 && cur_val < tmp_val && strcmp(current_def, "1")){ // get the smaller node safely.
usb_dbg("(%s): cur_val=%d, tmp_val=%d, set_default_node=%s.\n", device_name, cur_val, tmp_val, current_def);
				usb_dbg("(%s): Skip to write PPP's conf.\n", device_name);
				file_unlock(isLock_tty);
				file_unlock(isLock);
				return 0;
			}
			else{
usb_dbg("(%s): current_act=%s, device_name=%s.\n", device_name, current_act, device_name);
usb_dbg("(%s): cur_val=%d, tmp_val=%d.\n", device_name, cur_val, tmp_val);
				nvram_set(strcat_r(prefix, "_act", tmp), device_name);
				memset(current_act, 0, 16);
				strncpy(current_act, device_name, 16);
				if(got_Int_endpoint){
					nvram_set(strcat_r(prefix, "_act_def", tmp), "");
					memset(current_def, 0, 16);
				}
			}
		}

		// Only let act node trigger the dial procedure and avoid the wrong node to write the PPP conf.
		if(strcmp(device_name, current_act)){
			usb_dbg("(%s): Success!\n", device_name);
			file_unlock(isLock_tty);
			file_unlock(isLock);
			return 0;
		}

		// Wait all ttyUSB nodes to ready.
		sleep(1);
	}
	else{ // isACMNode(device_name).
		// Find the control interface of cdc-acm.
		if(!isACMInterface(interface_name, 1, vid, pid)){
			usb_dbg("(%s): Not control interface!\n", device_name);
			file_unlock(isLock_tty);
			file_unlock(isLock);
			return 0;
		}

		if(!strcmp(device_name, "ttyACM0")){
			nvram_set(strcat_r(prefix, "_act", tmp), device_name);
		}
		else{
			file_unlock(isLock_tty);
			file_unlock(isLock);
			return 0;
		}
	}

#ifdef RTCONFIG_DUALWAN
	// If the node is the default node without the Int endpoint, skip to autorun restart_wan_if.
	if(!nvram_match("wans_mode", "off") && strcmp(current_def, "1")){
#endif
		if ((wan_unit = get_usbif_dualwan_unit()) >= 0) {
			// show the manual-setting dongle in Networkmap when it was plugged after reboot.
			init_3g_param(port_path, vid, pid);
#if 0
			// TODO: for the bad CTF. After updating CTF, need to mark these codes.
			if(nvram_invmatch("ctf_disable", "1") && nvram_invmatch("ctf_disable_modem", "1")){
				nvram_set("ctf_disable_modem", "1");
				nvram_commit();
				notify_rc_and_wait("reboot");
				file_unlock(isLock_tty);
				file_unlock(isLock);

				return 0;
			}
#endif
		}
#ifdef RTCONFIG_DUALWAN
		else
usb_dbg("Didn't support the USB connection now...\n");
	}
#endif

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock_tty);
	file_unlock(isLock);
#endif // RTCONFIG_USB_MODEM
	return 1;
}

// Beceem's driver: drxvi314 identifies only one dongle.
int asus_usbbcm(const char *device_name, const char *action){
#ifdef RTCONFIG_USB_BECEEM
	char usb_node[32];
	int isLock;
	char port_path[8];
	char buf1[32];
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
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
		memset(buf1, 0, 32);
		sprintf(buf1, "usb_path_%s", device_name);
		memset(usb_node, 0, 32);
		strncpy(usb_node, nvram_safe_get(buf1), 32);
		nvram_unset(buf1);

		if(get_path_by_node(usb_node, port_path, 8) == NULL){
			usb_dbg("(%s): Fail to get usb path.\n", usb_node);
			file_unlock(isLock);
			return 0;
		}

		snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
		if(!strcmp(nvram_safe_get(strcat_r(prefix, "_act", tmp)), device_name))
			nvram_unset(tmp);
			nvram_unset("usb_modem_act_path");

		if(strlen(usb_node) > 0){
			// Modem remove action.
			unlink(WIMAX_CONF);

			killall_tk("usb_modeswitch");

			set_usb_common_nvram(action, device_name, usb_node, "modem");
			usb_dbg("(%s): Remove the Beceem node on USB node %s.\n", device_name, usb_node);
		}
		else
			usb_dbg("(%s): Remove a unknown-port Modem.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

	// Get USB node.
	if(get_usb_node_by_device(device_name, usb_node, 32) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	memset(buf1, 0, 32);
	sprintf(buf1, "usb_path_%s", device_name);
	nvram_set(buf1, usb_node);

	// Don't support the second modem device on a DUT.
	// Only see the other usb port, because in the same port there are more modem interfaces and they need to compare.
	snprintf(buf1, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(buf1) > 0 && strcmp(buf1, usb_node)){
		// We would show the second modem device but didn't let it work.
		// Because it didn't set the nvram: usb_path%d_act.
		usb_dbg("(%s): Already had the modem device in the other USB port!\n", device_name);
		file_unlock(isLock);
		return 0;
	}
	else if(strlen(buf1) <= 0)
		nvram_set("usb_modem_act_path", usb_node);

	// check the current working node of modem.
	snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

	nvram_set(strcat_r(prefix, "_act", tmp), device_name);

	usb_dbg("(%s): Success!\n", device_name);
	file_unlock(isLock);
#endif // RTCONFIG_USB_BECEEM
	return 1;
}

int asus_usb_interface(const char *device_name, const char *action){
#if defined(RTCONFIG_USB) || defined(RTCONFIG_USB_PRINTER) || defined(RTCONFIG_USB_MODEM)
	char usb_node[32], usb_port[32];
#ifdef RTCONFIG_USB_MODEM
	unsigned int vid, pid;
	char modem_cmd[128], buf[128];
#endif
#if defined(RTCONFIG_USB_PRINTER) || defined(RTCONFIG_USB_BECEEM)
	int retry;
#endif
	int isLock;
	char device_type[16];
	char nvram_usb_path[32];
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	char conf_file[32];
	char port_path[8];
	int port_num;
	int turn_on_led = 1;
	char class_path[PATH_MAX], class[10] = "";

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

	// Get USB node.
	if(get_usb_node_by_string(device_name, usb_node, 32) == NULL){
		usb_dbg("(%s): Fail to get usb node.\n", device_name);
		file_unlock(isLock);
		return 0;
	}

	if(get_path_by_node(usb_node, port_path, 8) == NULL){
		usb_dbg("(%s): Fail to get usb path.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	// Get USB port.
	if(get_usb_port_by_string(usb_node, usb_port, 32) == NULL){
		usb_dbg("(%s): Fail to get usb port.\n", usb_node);
		file_unlock(isLock);
		return 0;
	}

	port_num = get_usb_port_number(usb_port);
	if(!port_num){
		usb_dbg("(%s): Fail to get usb port number.\n", usb_port);
		file_unlock(isLock);
		return 0;
	}

	// check the current working node of modem.
	snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

	// If remove the device? Handle the remove hotplug of the printer and modem.
	if(!check_hotplug_action(action)){
		snprintf(nvram_usb_path, 32, "usb_led%d", port_num);
		if (!strcmp(nvram_safe_get(nvram_usb_path), "1")) {
			int turn_off_led = 1;
			disk_info_t *disk_list, *disk_info;

			disk_list = read_disk_data();
			for (disk_info = disk_list; disk_info != NULL && turn_off_led; disk_info = disk_info->next) {
				if (port_num != atoi(disk_info->port))
					continue;

				turn_off_led = 0;
				_dprintf("Another disk exist on USB port %d, don't turn off USB LED\n", port_num);
			}
			free_disk_data(&disk_list);

			if (turn_off_led)
				nvram_unset(nvram_usb_path);
		}

		strcpy(device_type, nvram_safe_get(prefix));

#ifdef RTCONFIG_USB_MODEM
		snprintf(conf_file, 32, "%s.%s", USB_MODESWITCH_CONF, port_path);
		unlink(conf_file);

		if(!strcmp(device_type, "modem") && !strcmp(usb_node, nvram_safe_get("usb_modem_act_path"))){
			snprintf(buf, 128, "%s", nvram_safe_get(strcat_r(prefix, "_act", tmp)));

			// remove the device between adding interface and adding tty.
			modprobe_r("option");
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(2,6,36)
			modprobe_r("usb_wwan");
#endif
			modprobe_r("usbserial");
			modprobe_r("cdc-acm");
#ifdef RTCONFIG_USB_BECEEM
			modprobe_r("drxvi314");

			vid = atoi(nvram_safe_get("usb_modem_act_vid"));
			pid = atoi(nvram_safe_get("usb_modem_act_pid"));
			if(is_samsung_dongle(1, vid, pid) || is_gct_dongle(1, vid, pid)){
				nvram_unset("usb_modem_act_path");
				nvram_unset("usb_modem_act_vid");
				nvram_unset("usb_modem_act_pid");
				nvram_unset(tmp);
			}
#endif

#ifndef RT4GAC55U
			// When ACM dongles are removed, there are no removed hotplugs of ttyACM nodes.
			if(!strncmp(buf, "ttyACM", 6)){
				// No methods let DUT restore the normal state after removing the ACM dongle.
				notify_rc("reboot");
				file_unlock(isLock);
				return 0;
			}
#endif
		}
#endif

		if(strlen(device_type) > 0){
			// Remove USB common nvram.
			set_usb_common_nvram(action, device_name, usb_node, NULL);

			usb_dbg("(%s): Remove %s interface on USB node %s.\n", device_name, device_type, usb_node);
		}
		else
			usb_dbg("(%s): Remove a unknown-type interface.\n", device_name);

		file_unlock(isLock);
		return 0;
	}

	/* Don't turn on USB LED for USB HUB. */
	snprintf(class_path, sizeof(class_path), "/sys/bus/usb/devices/%s/%s",
		device_name, strchr(device_name, ':')? "bInterfaceClass" : "bDeviceClass");
	if (f_read_string(class_path, class, sizeof(class)) > 0 && atoi(class) == 9)
		turn_on_led = 0;

	snprintf(nvram_usb_path, 32, "usb_led%d", port_num);
#ifdef RT4GAC55U
	if(nvram_get_int("usb_buildin") == port_num)
		; //skip this LED
	else
#endif	/* RT4GAC55U */
	if (turn_on_led && strcmp(nvram_safe_get(nvram_usb_path), "1"))
		nvram_set(nvram_usb_path, "1");

#ifdef RTCONFIG_USB_MODEM
	// Get VID & PID
	if ((vid = get_usb_vid(usb_node)) == 0 ||
	    (pid = get_usb_pid(usb_node)) == 0) {
		usb_dbg("(%s): Fail to get VID/PID of USB(%s).\n", device_name, usb_node);
		file_unlock(isLock);
		return 0;
	}

	// there is no any bounded drivers with Some Sierra dongles in the default state.
	if(vid == 0x1199 && isStorageInterface(device_name)){
		if(init_3g_param(port_path, vid, pid)){

			if(strcmp(nvram_safe_get("stop_ui_remove"), "1")){
				usb_dbg("(%s): Running usb_modeswitch...\n", device_name);
				snprintf(modem_cmd, 128, "%s.%s", USB_MODESWITCH_CONF, port_path);
				xstart("usb_modeswitch", "-c", modem_cmd);
			}

			file_unlock(isLock);
			return 0;
		}
	}

	if(!isSerialInterface(device_name, 1, vid, pid)
			&& !isACMInterface(device_name, 1, vid, pid)
			&& !isRNDISInterface(device_name, vid, pid)
			&& !isCDCETHInterface(device_name)
			&& !isNCMInterface(device_name)
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
	set_usb_common_nvram(action, device_name, usb_node, "modem");

	// Don't support the second modem device on a DUT.
	snprintf(tmp, 100, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(tmp) > 0 && strcmp(tmp, usb_node)){
		usb_dbg("(%s): Already had the modem device in the other USB port!\n", device_name);
		file_unlock(isLock);
		return 0;
	}
#ifdef RT4GAC55U
	else if((nvram_get_int("usb_gobi") == 1 && strcmp(port_path, "2"))
			|| (nvram_get_int("usb_gobi") != 1 && !strcmp(port_path, "2"))
			){
		if(nvram_get_int("usb_gobi") == 1){
			usb_dbg("(%s): Just use the built-in Gobi and disable the USB modem.\n", device_name);
			file_unlock(isLock);
			return 0;
		}
		else
			usb_dbg("(%s): Just use the USB modem and disable the built-in Gobi.\n", device_name);
	}
#endif

	// Modem add action.
	// WiMAX
#ifdef RTCONFIG_USB_BECEEM
	if(is_beceem_dongle(1, vid, pid)){
		usb_dbg("(%s): Runing Beceem module...\n", device_name);

		char *isp = nvram_safe_get("modem_isp");

		eval("rm", "-rf", BECEEM_DIR);
		eval("mkdir", "-p", BECEEM_DIR);
		eval("ln", "-sf", "/rom/Beceem_firmware/RemoteProxy.cfg", "/tmp/Beceem_firmware/RemoteProxy.cfg");

		if(!strcmp(isp, "Yota")){
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi200.bin.normal", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi.cfg.yota", "/tmp/Beceem_firmware/macxvi.cfg");
			eval("ln", "-sf", "/rom/Beceem_firmware/Server_CA.pem.yota", "/tmp/Beceem_firmware/Server_CA.pem");
		}
		else if(!strcmp(isp, "GMC")){
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi200.bin.normal", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi.cfg.gmc", "/tmp/Beceem_firmware/macxvi.cfg");
		}
		else if(!strcmp(isp, "FreshTel")){
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi200.bin.normal", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi.cfg.freshtel", "/tmp/Beceem_firmware/macxvi.cfg");
		}
		else if(!strcmp(isp, "Giraffe")){
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi200.bin.giraffe", "/tmp/Beceem_firmware/macxvi200.bin");
			eval("ln", "-sf", "/rom/Beceem_firmware/macxvi.cfg.giraffe", "/tmp/Beceem_firmware/macxvi.cfg");
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

		nvram_set(strcat_r(prefix, "_act", tmp), "wimax0");
		nvram_set("usb_modem_act_path", usb_node);
		snprintf(buf, 128, "%u",  vid);
		nvram_set("usb_modem_act_vid", buf);
		snprintf(buf, 128, "%u",  pid);
		nvram_set("usb_modem_act_pid", buf);
	}
	else if(is_gct_dongle(1, vid, pid)){
		// need to run one time and fillfull the nvram: usb_path%d_act.
		usb_dbg("(%s): Runing GCT dongle...\n", device_name);

		nvram_set(strcat_r(prefix, "_act", tmp), "wimax0");
		nvram_set("usb_modem_act_path", usb_node);
		snprintf(buf, 128, "%u",  vid);
		nvram_set("usb_modem_act_vid", buf);
		snprintf(buf, 128, "%u",  pid);
		nvram_set("usb_modem_act_pid", buf);
	}
	else
#endif
	if(is_android_phone(0, vid, pid)){
		usb_dbg("(%s): do nothing with the MTP mode.\n", device_name);
	}
	else if(isRNDISInterface(device_name, vid, pid)){
		usb_dbg("(%s): Runing RNDIS...\n", device_name);
	}
	else if(isCDCETHInterface(device_name)){
		usb_dbg("(%s): Runing cdc_ether...\n", device_name);
	}
	else if(isNCMInterface(device_name)){
		usb_dbg("(%s): Runing cdc_ncm...\n", device_name);
	}
	else if(!strcmp(nvram_safe_get("stop_ui_insmod"), "1")){
		usb_dbg("(%s): Don't insmod the serial modules.\n", device_name);
	}
	else if(isSerialInterface(device_name, 1, vid, pid) && vid == 0x05c6 && pid == 0x9026 && nvram_get_int("usb_gobi") == 1){
		usb_dbg("(%s): Runing Gobi ...\n", device_name);
	}
	else if(!(vid == 0x05c6 && pid == 0x9026) &&
			(!strncmp(nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), "Android", 7) || !strncmp(nvram_safe_get(strcat_r(prefix, "_product", tmp)), "Android", 7))
			){
		usb_dbg("(%s): Android phone: Runing RNDIS...\n", device_name);
	}
	else if(isSerialInterface(device_name, 1, vid, pid)){
		usb_dbg("(%s): Runing USB serial with (0x%04x/0x%04x)...\n", device_name, vid, pid);
		sleep(1);
		modprobe("usbserial");
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(2,6,36)
		modprobe("usb_wwan");
#endif
		snprintf(modem_cmd, 128, "vendor=0x%04x", vid);
		snprintf(buf, 128, "product=0x%04x", pid);
		eval("insmod", "option", modem_cmd, buf);
		sleep(1);
	}
	else if(isACMInterface(device_name, 1, vid, pid)){
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

/*
  Mode switching tool for controlling mode of 'multi-state' USB devices
  Version 2.1.1, 2014/03/27

  Copyright (C) 2007 - 2014 Josua Dietze (mail to "usb_admin" at the domain
  of the home page; or write a personal message through the forum to "Josh".
  NO SUPPORT VIA E-MAIL - please use the forum for that)

  Major contributions:

  Command line parsing, decent usage/config output/handling, bugfixes and advanced
  options added by:
    Joakim Wennergren

  TargetClass parameter implementation to support new Option devices/firmware:
    Paul Hardwick (http://www.pharscape.org)

  Created with initial help from:
    "usbsnoop2libusb.pl" by Timo Lindfors (http://iki.fi/lindi/usb/usbsnoop2libusb.pl)

  Config file parsing stuff borrowed from:
    Guillaume Dargaud (http://www.gdargaud.net/Hack/SourceCode.html)

  Hexstr2bin function borrowed from:
    Jouni Malinen (http://hostap.epitest.fi/wpa_supplicant, from "common.c")

  Other contributions: see README

  Device information contributors are named in the "device_reference.txt" file. See
  homepage.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details:

  http://www.gnu.org/licenses/gpl.txt

*/

/* Recommended tab size: 4 */

#define VERSION "2.1.1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <getopt.h>
#include <syslog.h>
#include <unistd.h>

#include "usb_modeswitch.h"


/* libusb 1.0 wrappers, lazy leftover */

int usb_bulk_io(struct libusb_device_handle *handle, int ep, char *bytes,
	int size, int timeout)
{
	int actual_length;
	int r;
//	usbi_dbg("endpoint %x size %d timeout %d", ep, size, timeout);
	r = libusb_bulk_transfer(handle, ep & 0xff, (unsigned char *)bytes, size,
		&actual_length, timeout);
	
	/* if we timed out but did transfer some data, report as successful short
	 * read. FIXME: is this how libusb-0.1 works? */
	if (r == 0 || (r == LIBUSB_ERROR_TIMEOUT && actual_length > 0))
		return actual_length;

	return r;
}

static int usb_interrupt_io(libusb_device_handle *handle, int ep, char *bytes,
	int size, int timeout)
{
	int actual_length;
	int r;
//	usbi_dbg("endpoint %x size %d timeout %d", ep, size, timeout);
	r = libusb_interrupt_transfer(handle, ep & 0xff, (unsigned char *)bytes, size,
		&actual_length, timeout);
	
	/* if we timed out but did transfer some data, report as successful short
	 * read. FIXME: is this how libusb-0.1 works? */
	if (r == 0 || (r == LIBUSB_ERROR_TIMEOUT && actual_length > 0))
		return actual_length;

	return (r);
}


#define LINE_DIM 1024
#define MAXLINES 50
#define BUF_SIZE 4096
#define DESCR_MAX 129

#define SEARCH_DEFAULT 0
#define SEARCH_TARGET 1
#define SEARCH_BUSDEV 2

#define SWITCH_CONFIG_MAXTRIES   5

#define SHOW_PROGRESS if (show_progress) fprintf

char *TempPP=NULL;

static struct libusb_context *ctx = NULL;
static struct libusb_device *dev;
static struct libusb_device_handle *devh;
static struct libusb_config_descriptor *active_config = NULL;

int DefaultVendor=0, DefaultProduct=0, TargetVendor=0, TargetProduct=-1, TargetClass=0;
int MessageEndpoint=0, ResponseEndpoint=0, ReleaseDelay=0;
int targetDeviceCount=0, searchMode;
int devnum=-1, busnum=-1;
int ret;

unsigned int ModeMap = 0;
#define DETACHONLY_MODE		0x00000001
#define HUAWEI_MODE			0x00000002
#define SIERRA_MODE			0x00000004
#define SONY_MODE			0x00000008
#define GCT_MODE			0x00000010
#define KOBIL_MODE			0x00000020
#define SEQUANS_MODE		0x00000040
#define MOBILEACTION_MODE	0x00000080
#define CISCO_MODE			0x00000100
#define QISDA_MODE			0x00000200
#define QUANTA_MODE			0x00000400
#define BLACKBERRY_MODE		0x00000800
#define PANTECH_MODE		0x00001000

char verbose=0, show_progress=1, ResetUSB=0, CheckSuccess=0, config_read=0;
char NeedResponse=0, NoDriverLoading=0, InquireDevice=0, sysmode=0, mbim=0;
char StandardEject=0;

char imanufact[DESCR_MAX], iproduct[DESCR_MAX], iserial[DESCR_MAX];

char MessageContent[LINE_DIM];
char MessageContent2[LINE_DIM];
char MessageContent3[LINE_DIM];
char TargetProductList[LINE_DIM];
char DefaultProductList[5];
char ByteString[LINE_DIM/2];
char buffer[BUF_SIZE];

FILE *output;


/* Settable Interface and Configuration (for debugging mostly) (jmw) */
int Interface = -1, Configuration = 0, AltSetting = -1;


static struct option long_options[] = {
	{"help",				no_argument, 0, 'h'},
	{"version",				no_argument, 0, 'e'},
	{"default-vendor",		required_argument, 0, 'v'},
	{"default-product",		required_argument, 0, 'p'},
	{"target-vendor",		required_argument, 0, 'V'},
	{"target-product",		required_argument, 0, 'P'},
	{"target-class",		required_argument, 0, 'C'},
	{"message-endpoint",	required_argument, 0, 'm'},
	{"message-content",		required_argument, 0, 'M'},
	{"message-content2",	required_argument, 0, '2'},
	{"message-content3",	required_argument, 0, '3'},
	{"release-delay",		required_argument, 0, 'w'},
	{"response-endpoint",	required_argument, 0, 'r'},
	{"bus-num",				required_argument, 0, 'b'},
	{"device-num",			required_argument, 0, 'g'},
	{"detach-only",			no_argument, 0, 'd'},
	{"huawei-mode",			no_argument, 0, 'H'},
	{"sierra-mode",			no_argument, 0, 'S'},
	{"sony-mode",			no_argument, 0, 'O'},
	{"qisda-mode",			no_argument, 0, 'B'},
	{"quanta-mode",			no_argument, 0, 'E'},
	{"kobil-mode",			no_argument, 0, 'T'},
	{"gct-mode",			no_argument, 0, 'G'},
	{"sequans-mode",		no_argument, 0, 'N'},
	{"mobileaction-mode",	no_argument, 0, 'A'},
	{"cisco-mode",	        no_argument, 0, 'L'},
	{"blackberry-mode",		no_argument, 0, 'Z'},
	{"pantech-mode",		no_argument, 0, 'F'},
	{"std-eject",			no_argument, 0, 'K'},
	{"need-response",		no_argument, 0, 'n'},
	{"reset-usb",			no_argument, 0, 'R'},
	{"config-file",			required_argument, 0, 'c'},
	{"verbose",				no_argument, 0, 'W'},
	{"quiet",				no_argument, 0, 'Q'},
	{"sysmode",				no_argument, 0, 'D'},
	{"inquire",				no_argument, 0, 'I'},
	{"stdinput",			no_argument, 0, 't'},
	{"find-mbim",			no_argument, 0, 'j'},
	{"long-config",			required_argument, 0, 'f'},
	{"check-success",		required_argument, 0, 's'},
	{"interface",			required_argument, 0, 'i'},
	{"configuration",		required_argument, 0, 'u'},
	{"altsetting",			required_argument, 0, 'a'},
	{0, 0, 0, 0}
};


void readConfigFile(const char *configFilename)
{
	ParseParamHex(configFilename, TargetVendor);
	ParseParamHex(configFilename, TargetProduct);
	ParseParamString(configFilename, TargetProductList);
	ParseParamHex(configFilename, TargetClass);
	ParseParamHex(configFilename, DefaultVendor);
	ParseParamHex(configFilename, DefaultProduct);
	ParseParamBoolMap(configFilename, DetachStorageOnly, ModeMap, DETACHONLY_MODE);
	ParseParamBoolMap(configFilename, HuaweiMode, ModeMap, HUAWEI_MODE);
	ParseParamBoolMap(configFilename, SierraMode, ModeMap, SIERRA_MODE);
	ParseParamBoolMap(configFilename, SonyMode, ModeMap, SONY_MODE);
	ParseParamBoolMap(configFilename, GCTMode, ModeMap, GCT_MODE);
	ParseParamBoolMap(configFilename, KobilMode, ModeMap, KOBIL_MODE);
	ParseParamBoolMap(configFilename, SequansMode, ModeMap, SEQUANS_MODE);
	ParseParamBoolMap(configFilename, MobileActionMode, ModeMap, MOBILEACTION_MODE);
	ParseParamBoolMap(configFilename, CiscoMode, ModeMap, CISCO_MODE);
	ParseParamBoolMap(configFilename, QisdaMode, ModeMap, QISDA_MODE);
	ParseParamBoolMap(configFilename, QuantaMode, ModeMap, QUANTA_MODE);
	ParseParamBoolMap(configFilename, BlackberryMode, ModeMap, BLACKBERRY_MODE);
	ParseParamBoolMap(configFilename, PantechMode, ModeMap, PANTECH_MODE);
	ParseParamBool(configFilename, StandardEject);
	ParseParamBool(configFilename, NoDriverLoading);
	ParseParamHex(configFilename, MessageEndpoint);
	ParseParamString(configFilename, MessageContent);
	ParseParamString(configFilename, MessageContent2);
	ParseParamString(configFilename, MessageContent3);
	ParseParamInt(configFilename, ReleaseDelay);
	ParseParamHex(configFilename, NeedResponse);
	ParseParamHex(configFilename, ResponseEndpoint);
	ParseParamHex(configFilename, ResetUSB);
	ParseParamHex(configFilename, InquireDevice);
	ParseParamInt(configFilename, CheckSuccess);
	ParseParamHex(configFilename, Interface);
	ParseParamHex(configFilename, Configuration);
	ParseParamHex(configFilename, AltSetting);

	/* TargetProductList has priority over TargetProduct */
	if (TargetProduct != -1 && TargetProductList[0] != '\0') {
		TargetProduct = -1;
		SHOW_PROGRESS(output,"Warning: TargetProductList overrides TargetProduct!\n");
	}

	config_read = 1;
}


void printConfig()
{
	if ( DefaultVendor )
		fprintf (output,"DefaultVendor=  0x%04x\n",			DefaultVendor);
	if ( DefaultProduct )
		fprintf (output,"DefaultProduct= 0x%04x\n",			DefaultProduct);
	if ( TargetVendor )
		fprintf (output,"TargetVendor=   0x%04x\n",		TargetVendor);
	if ( TargetProduct > -1 )
		fprintf (output,"TargetProduct=  0x%04x\n",		TargetProduct);
	if ( TargetClass )
		fprintf (output,"TargetClass=    0x%02x\n",		TargetClass);
	if ( strlen(TargetProductList) )
		fprintf (output,"TargetProductList=\"%s\"\n",		TargetProductList);
	if (StandardEject)
		fprintf (output,"\nStandardEject=1\n");
	if (ModeMap & DETACHONLY_MODE)
		fprintf (output,"\nDetachStorageOnly=1\n");
	if (ModeMap & HUAWEI_MODE)
		fprintf (output,"HuaweiMode=1\n");
	if (ModeMap & SIERRA_MODE)
		fprintf (output,"SierraMode=1\n");
	if (ModeMap & SONY_MODE)
		fprintf (output,"SonyMode=1\n");
	if (ModeMap & QISDA_MODE)
		fprintf (output,"QisdaMode=1\n");
	if (ModeMap & QUANTA_MODE)
		fprintf (output,"QuantaMode=1\n");
	if (ModeMap & GCT_MODE)
		fprintf (output,"GCTMode=1\n");
	if (ModeMap & KOBIL_MODE)
		fprintf (output,"KobilMode=1\n");
	if (ModeMap & SEQUANS_MODE)
		fprintf (output,"SequansMode=1\n");
	if (ModeMap & MOBILEACTION_MODE)
		fprintf (output,"MobileActionMode=1\n");
	if (ModeMap & CISCO_MODE)
		fprintf (output,"CiscoMode=1\n");
	if (ModeMap & BLACKBERRY_MODE)
		fprintf (output,"BlackberryMode=1\n");
	if (ModeMap & PANTECH_MODE)
		fprintf (output,"PantechMode=1\n");
	if ( MessageEndpoint )
		fprintf (output,"MessageEndpoint=0x%02x\n",	MessageEndpoint);
	if ( strlen(MessageContent) )
		fprintf (output,"MessageContent=\"%s\"\n",	MessageContent);
	if ( strlen(MessageContent2) )
		fprintf (output,"MessageContent2=\"%s\"\n",	MessageContent2);
	if ( strlen(MessageContent3) )
		fprintf (output,"MessageContent3=\"%s\"\n",	MessageContent3);
	fprintf (output,"NeedResponse=%i\n",		(int)NeedResponse);
	if ( ResponseEndpoint )
		fprintf (output,"ResponseEndpoint=0x%02x\n",	ResponseEndpoint);
	if ( Interface > -1 )
		fprintf (output,"Interface=0x%02x\n",			Interface);
	if ( Configuration > 0 )
		fprintf (output,"Configuration=0x%02x\n",	Configuration);
	if ( AltSetting > -1 )
		fprintf (output,"AltSetting=0x%02x\n",	AltSetting);
	if ( InquireDevice )
		fprintf (output,"\nInquireDevice=1\n");
	if ( CheckSuccess )
		fprintf (output,"Success check enabled, max. wait time %d seconds\n", CheckSuccess);
	if ( sysmode )
		fprintf (output,"System integration mode enabled\n");
}


int readArguments(int argc, char **argv)
{
	int c, option_index = 0, count=0;
	char *longConfig = NULL;
	if (argc==1)
	{
		printHelp();
		printVersion();
		exit(1);
	}

	while (1)
	{
		c = getopt_long (argc, argv, "hejWQDndKHSOBEGTNALZFRItv:p:V:P:C:m:M:2:3:w:r:c:i:u:a:s:f:b:g:",
						long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;
		count++;
		switch (c)
		{
			case 'R': ResetUSB = 1; break;
			case 'v': DefaultVendor = strtol(optarg, NULL, 16); break;
			case 'p': DefaultProduct = strtol(optarg, NULL, 16); break;
			case 'V': TargetVendor = strtol(optarg, NULL, 16); break;
			case 'P': TargetProduct = strtol(optarg, NULL, 16); break;
			case 'C': TargetClass = strtol(optarg, NULL, 16); break;
			case 'm': MessageEndpoint = strtol(optarg, NULL, 16); break;
			case 'M': strncpy(MessageContent, optarg, LINE_DIM); break;
			case '2': strncpy(MessageContent2, optarg, LINE_DIM); break;
			case '3': strncpy(MessageContent3, optarg, LINE_DIM); break;
			case 'w': ReleaseDelay = strtol(optarg, NULL, 10); break;
			case 'n': NeedResponse = 1; break;
			case 'r': ResponseEndpoint = strtol(optarg, NULL, 16); break;
			case 'K': StandardEject = 1; break;
			case 'd': ModeMap = ModeMap + DETACHONLY_MODE; break;
			case 'H': ModeMap = ModeMap + HUAWEI_MODE; break;
			case 'S': ModeMap = ModeMap + SIERRA_MODE; break;
			case 'O': ModeMap = ModeMap + SONY_MODE; break;; break;
			case 'B': ModeMap = ModeMap + QISDA_MODE; break;
			case 'E': ModeMap = ModeMap + QUANTA_MODE; break;
			case 'G': ModeMap = ModeMap + GCT_MODE; break;
			case 'T': ModeMap = ModeMap + KOBIL_MODE; break;
			case 'N': ModeMap = ModeMap + SEQUANS_MODE; break;
			case 'A': ModeMap = ModeMap + MOBILEACTION_MODE; break;
			case 'L': ModeMap = ModeMap + CISCO_MODE; break;
			case 'Z': ModeMap = ModeMap + BLACKBERRY_MODE; break;
			case 'F': ModeMap = ModeMap + PANTECH_MODE; break;
			case 'c': readConfigFile(optarg); break;
			case 't': readConfigFile("stdin"); break;
			case 'W': verbose = 1; show_progress = 1; count--; break;
			case 'Q': show_progress = 0; verbose = 0; count--; break;
			case 'D': sysmode = 1; count--; break;
			case 's': CheckSuccess = strtol(optarg, NULL, 10); count--; break;
			case 'I': InquireDevice = 1; break;
			case 'b': busnum = strtol(optarg, NULL, 10); break;
			case 'g': devnum = strtol(optarg, NULL, 10); break;

			case 'i': Interface = strtol(optarg, NULL, 16); break;
			case 'u': Configuration = strtol(optarg, NULL, 16); break;
			case 'a': AltSetting = strtol(optarg, NULL, 16); break;
			case 'j': mbim = 1; break;

			case 'f':
				longConfig = malloc(strlen(optarg)+5);
				strcpy(longConfig,"##\n");
				strcat(longConfig,optarg);
				strcat(longConfig,"\n");
				readConfigFile(longConfig);
				free(longConfig);
				break;

			case 'e':
				printVersion();
				exit(0);
				break;
			case 'h':
				printVersion();
				printHelp();
				exit(0);
				break;

			default: /* Unsupported - error message has already been printed */
				fprintf (output,"\n");
				printHelp();
				exit(1);
		}
	}

	return count;
}


int main(int argc, char **argv)
{
	int numDefaults=0, sonySuccess=0;
	int currentConfig=0, defaultClass=0, interfaceClass=0;
	struct libusb_device_descriptor descriptor;

	/* Make sure we have empty strings even if not set by config */
	TargetProductList[0] = '\0';
	MessageContent[0] = '\0';
	MessageContent2[0] = '\0';
	MessageContent3[0] = '\0';
	DefaultProductList[0] = '\0';

	/* Useful for debugging during boot */
//	output=fopen("/dev/console", "w");
	output=stdout;

	signal(SIGTERM, release_usb_device);

	/*
	 * Parameter parsing, USB preparation/diagnosis, plausibility checks
	 */

	/* Check command arguments, use params instead of config file when given */
	switch (readArguments(argc, argv)) {
		case 0:						/* no argument or -W, -q or -s */
			break;
		default:					/* one or more arguments except -W, -q or -s */
			if (!config_read)		/* if arguments contain -c, the config file was already processed */
				if (verbose) fprintf(output,"Take all parameters from the command line\n\n");
	}

	if (verbose) {
		printVersion();
		printConfig();
		fprintf(output,"\n");
	}

	/* Some sanity checks. The default IDs are mandatory */
	if (!(DefaultVendor && DefaultProduct)) {
		SHOW_PROGRESS(output,"No default vendor/product ID given. Abort\n\n");
		exit(1);
	}

	if (strlen(MessageContent)) {
		if (strlen(MessageContent) % 2 != 0) {
			fprintf(stderr, "Error: MessageContent hex string has uneven length. Abort\n\n");
			exit(1);
		}
		if ( hexstr2bin(MessageContent, ByteString, strlen(MessageContent)/2) == -1) {
			fprintf(stderr, "Error: MessageContent %s\n is not a hex string. Abort\n\n", MessageContent);
			exit(1);
		}
	}

	if (devnum == -1) {
		searchMode = SEARCH_DEFAULT;
	} else {
		SHOW_PROGRESS(output,"Use given bus/device number: %03d/%03d ...\n", busnum, devnum);
		searchMode = SEARCH_BUSDEV;
	}

	if (show_progress)
		if (CheckSuccess && !(TargetVendor || TargetProduct > -1 || TargetProductList[0] != '\0') && !TargetClass)
			fprintf(output,"Note: No target parameter given; success check limited\n");

	if (TargetProduct > -1 && TargetProductList[0] == '\0') {
		sprintf(TargetProductList,"%04x",TargetProduct);
		TargetProduct = -1;
	}

	/* libusb initialization */
	libusb_init(&ctx);

	if (verbose)
		libusb_set_debug(ctx, 3);

	if (mbim) {
		printf("%d\n", findMBIMConfig(DefaultVendor, DefaultProduct, searchMode) );
		exit(0);
	}

	/* Count existing target devices, remember for success check */
	if (searchMode != SEARCH_BUSDEV && (TargetVendor || TargetClass)) {
		SHOW_PROGRESS(output,"Look for target devices ...\n");
		search_devices(&targetDeviceCount, TargetVendor, TargetProductList, TargetClass, 0, SEARCH_TARGET);
		if (targetDeviceCount) {
			SHOW_PROGRESS(output," Found devices in target mode or class (%d)\n", targetDeviceCount);
		} else
			SHOW_PROGRESS(output," No devices in target mode or class found\n");
	}

	/* Count default devices, get the last one found */
	SHOW_PROGRESS(output,"Look for default devices ...\n");

	sprintf(DefaultProductList,"%04x",DefaultProduct);
	dev = search_devices(&numDefaults, DefaultVendor, DefaultProductList, TargetClass, Configuration, searchMode);
	if (numDefaults) {
		SHOW_PROGRESS(output," Found devices in default mode (%d)\n", numDefaults);
	} else {
		SHOW_PROGRESS(output," No devices in default mode found. Nothing to do. Bye!\n\n");
		exit(0);
	}
	if (dev == NULL) {
		SHOW_PROGRESS(output," No bus/device match. Is device connected? Abort\n\n");
		exit(0);
	} else {
		if (devnum == -1) {
			devnum = libusb_get_device_address(dev);
			busnum = libusb_get_bus_number(dev);
			SHOW_PROGRESS(output,"Access device %03d on bus %03d\n", devnum, busnum);
		}
		libusb_open(dev, &devh);
		if (devh == NULL) {
			SHOW_PROGRESS(output,"Error opening the device. Abort\n\n");
			exit(1);
		}
	}
	libusb_get_active_config_descriptor(dev, &active_config);

	/* Get current configuration of default device if parameter is set */
	if (Configuration > -1) {
		currentConfig = active_config->bConfigurationValue;
		SHOW_PROGRESS(output,"Current configuration number is %d\n", currentConfig);
	} else
		currentConfig = 0;

	libusb_get_device_descriptor(dev, &descriptor);
	defaultClass = descriptor.bDeviceClass;
	if (Interface == -1)
		Interface = active_config->interface[0].altsetting[0].bInterfaceNumber;
	SHOW_PROGRESS(output,"Use interface number %d\n", Interface);

	/* Get class of default device/interface */
	interfaceClass = get_interface_class();

	/* Check or get endpoints */
	if (strlen(MessageContent) || StandardEject || InquireDevice || ModeMap & CISCO_MODE) {
		if (!MessageEndpoint)
			MessageEndpoint = find_first_bulk_endpoint(LIBUSB_ENDPOINT_OUT);
		if (!ResponseEndpoint)
			ResponseEndpoint = find_first_bulk_endpoint(LIBUSB_ENDPOINT_IN);
		libusb_free_config_descriptor(active_config);
		if (!MessageEndpoint) {
			fprintf(stderr,"Error: message endpoint not given or found. Abort\n\n");
			exit(1);
		}
		if (!ResponseEndpoint) {
			fprintf(stderr,"Error: response endpoint not given or found. Abort\n\n");
			exit(1);
		}
		SHOW_PROGRESS(output,"Use endpoints 0x%02x (out) and 0x%02x (in)\n", MessageEndpoint, ResponseEndpoint);
	} else
		libusb_free_config_descriptor(active_config);

	if (interfaceClass == -1) {
		fprintf(stderr, "Error: Could not get class of interface %d. Does it exist? Abort\n\n",Interface);
		exit(1);
	}

	if (defaultClass == 0)
		defaultClass = interfaceClass;
	else
		if (interfaceClass == 8 && defaultClass != 8) {
			/* Weird device with default class other than 0 and differing interface class */
			SHOW_PROGRESS(output,"Ambiguous Class/InterfaceClass: 0x%02x/0x08\n", defaultClass);
			defaultClass = 8;
		}

	if (strlen(MessageContent) && strncmp("55534243",MessageContent,8) == 0)
		if (defaultClass != 8) {
			fprintf(stderr, "Error: can't use storage command in MessageContent with interface %d;\n"
				"       interface class is %d, expected 8. Abort\n\n", Interface, defaultClass);
			exit(1);
		}

	if (InquireDevice && show_progress) {
		if (defaultClass == 0x08) {
			SHOW_PROGRESS(output,"Inquire device details; driver will be detached ...\n");
			detachDriver();
			if (deviceInquire() >= 0)
				InquireDevice = 2;
		} else
			SHOW_PROGRESS(output,"Not a storage device, skip SCSI inquiry\n");
	}

	deviceDescription();
	if (show_progress) {
		fprintf(output,"\nUSB description data (for identification)\n");
		fprintf(output,"-------------------------\n");
		fprintf(output,"Manufacturer: %s\n", imanufact);
		fprintf(output,"     Product: %s\n", iproduct);
		fprintf(output,"  Serial No.: %s\n", iserial);
		fprintf(output,"-------------------------\n");
	}

	/* Special modes are exclusive, so check for illegal combinations.
	 * More than one bit set?
	 */
	if ( ModeMap & (ModeMap-1) ) {
		fprintf(output,"Multiple special modes selected; check configuration. Abort\n\n");
		exit(1);
	}

	if ((strlen(MessageContent) || StandardEject) && ModeMap ) {
		MessageContent[0] = '\0';
		StandardEject = 0;
		fprintf(output,"Warning: MessageContent/StandardEject ignored; can't combine with special mode\n");
	}

	if (StandardEject && (strlen(MessageContent2) || strlen(MessageContent3))) {
		fprintf(output,"Warning: MessageContent2/3 ignored; only one allowed with StandardEject\n");
	}

	if ( !ModeMap && !strlen(MessageContent) && AltSetting == -1 && !Configuration && !StandardEject )
		SHOW_PROGRESS(output,"Warning: no switching method given. See documentation\n");

	/*
	 * The switching actions
	 */

	if (sysmode) {
		openlog("usb_modeswitch", 0, LOG_SYSLOG);
		syslog(LOG_NOTICE, "switch device %04x:%04x on %03d/%03d", DefaultVendor, DefaultProduct, busnum, devnum);
	}

	if (ModeMap & DETACHONLY_MODE) {
		SHOW_PROGRESS(output,"Detach storage driver as switching method ...\n");
		if (InquireDevice == 2) {
			SHOW_PROGRESS(output," Any driver was already detached for inquiry. Do nothing\n");
		} else {
			ret = detachDriver();
			if (ret == 2)
				SHOW_PROGRESS(output," You may want to remove the storage driver manually\n");
		}
	}

	if(ModeMap & HUAWEI_MODE) {
		switchHuaweiMode();
	}
	if(ModeMap & SIERRA_MODE) {
		switchSierraMode();
	}
	if(ModeMap & GCT_MODE) {
		detachDriver();
		switchGCTMode();
	}
	if(ModeMap & QISDA_MODE) {
		switchQisdaMode();
	}
	if(ModeMap & KOBIL_MODE) {
		detachDriver();
		switchKobilMode();
	}
	if(ModeMap & QUANTA_MODE) {
		switchQuantaMode();
	}
	if(ModeMap & SEQUANS_MODE) {
		switchSequansMode();
	}
	if(ModeMap & MOBILEACTION_MODE) {
		switchActionMode();
	}
	if(ModeMap & CISCO_MODE) {
		detachDriver();
		switchCiscoMode();
	}
	if(ModeMap & BLACKBERRY_MODE) {
		detachDriver();
	    switchBlackberryMode();
	}
	if(ModeMap & PANTECH_MODE) {
		detachDriver();
		switchPantechMode();
	}
	if(ModeMap & SONY_MODE) {
		if (CheckSuccess)
			SHOW_PROGRESS(output,"Note: CheckSuccess ignored; Sony mode does separate checks\n");
		CheckSuccess = 0; /* separate and implied success control */
		sonySuccess = switchSonyMode();
	}

	if (StandardEject) {
		SHOW_PROGRESS(output,"Sending standard EJECT sequence\n");
		detachDriver();
		if (MessageContent[0] != '\0')
			strcpy(MessageContent3, MessageContent);
		else
			MessageContent3[0] = '\0';

		strcpy(MessageContent,"5553424312345678000000000000061e000000000000000000000000000000");
		strcpy(MessageContent2,"5553424312345679000000000000061b000000020000000000000000000000");
		NeedResponse = 1;
		switchSendMessage();
	} else if (strlen(MessageContent)) {
		if (InquireDevice != 2)
			detachDriver();
		switchSendMessage();
	}

	if (Configuration > 0) {
		if (currentConfig != Configuration) {
			if (switchConfiguration()) {
				currentConfig = get_current_configuration(dev);
				if (currentConfig == Configuration) {
					SHOW_PROGRESS(output,"The configuration was set successfully\n");
				} else {
					SHOW_PROGRESS(output,"Changing the configuration has failed\n");
				}
			}
		} else {
			SHOW_PROGRESS(output,"Target configuration %d found. Do nothing\n", currentConfig);
		}
	}

	if (AltSetting != -1) {
		switchAltSetting();
	}

	/* No "removal" check if these are set */
	if ((Configuration > 0 || AltSetting > -1) && !ResetUSB) {
		libusb_close(devh);
		devh = 0;
	}

	if (ResetUSB) {
		resetUSB();
		devh = 0;
	}

	if (CheckSuccess) {
		if (searchMode == SEARCH_BUSDEV && sysmode) {
			SHOW_PROGRESS(output,"Bus/dev search active, refer success check to wrapper. Bye!\n\n");
			printf("ok:busdev\n");
			goto CLOSING;
		}
		if (checkSuccess()) {
			if (sysmode) {
				if (NoDriverLoading)
					printf("ok:\n");
				else
					if (TargetProduct < 1)
						printf("ok:no_data\n");
					else
						printf("ok:%04x:%04x\n", TargetVendor, TargetProduct);
			}
		} else
			if (sysmode)
				printf("fail:\n");
	} else {
		if (ModeMap & SONY_MODE)
			if (sonySuccess) {
				if (sysmode) {
					syslog(LOG_NOTICE, "switched S.E. MD400 to modem mode");
					printf("ok:\n"); /* ACM device, no driver action */
				}
				SHOW_PROGRESS(output,"-> device should be stable now. Bye!\n\n");
			} else {
				if (sysmode)
					printf("fail:\n");
				SHOW_PROGRESS(output,"-> switching was probably not completed. Bye!\n\n");
			}
		else
			SHOW_PROGRESS(output,"-> Run lsusb to note any changes. Bye!\n\n");
	}
CLOSING:
	if (sysmode)
		closelog();
	if (devh)
		libusb_close(devh);
	exit(0);
}


/* Get descriptor strings if available (identification details) */
void deviceDescription ()
{
	char* c;
	memset (imanufact, ' ', DESCR_MAX);
	memset (iproduct, ' ', DESCR_MAX);
	memset (iserial, ' ', DESCR_MAX);

	struct libusb_device_descriptor descriptor;
	libusb_get_device_descriptor(dev, &descriptor);

	int iManufacturer = descriptor.iManufacturer;
	int iProduct = descriptor.iProduct;
	int iSerialNumber = descriptor.iSerialNumber;

	if (iManufacturer) {
		ret = libusb_get_string_descriptor_ascii(devh, iManufacturer, (unsigned char *)imanufact, DESCR_MAX);
		if (ret < 0)
			fprintf(stderr, "Error: could not get description string \"manufacturer\"\n");
	} else
		strcpy(imanufact, "not provided");
	c = strstr(imanufact, "    ");
	if (c)
		memset((void*)c, '\0', 1);

	if (iProduct) {
		ret = libusb_get_string_descriptor_ascii(devh, iProduct, (unsigned char *)iproduct, DESCR_MAX);
		if (ret < 0)
			fprintf(stderr, "Error: could not get description string \"product\"\n");
	} else
		strcpy(iproduct, "not provided");
	c = strstr(iproduct, "    ");
	if (c)
		memset((void*)c, '\0', 1);

	if (iSerialNumber) {
		ret = libusb_get_string_descriptor_ascii(devh, iSerialNumber, (unsigned char *)iserial, DESCR_MAX);
		if (ret < 0)
			fprintf(stderr, "Error: could not get description string \"serial number\"\n");
	} else
		strcpy(iserial, "not provided");
	c = strstr(iserial, "    ");
	if (c)
		memset((void*)c, '\0', 1);

}

/* Print result of SCSI command INQUIRY (identification details) */
int deviceInquire ()
{
	const unsigned char inquire_msg[] = {
	  0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78,
	  0x24, 0x00, 0x00, 0x00, 0x80, 0x00, 0x06, 0x12,
	  0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	char *command;
	char data[36];
	int i;

	command = malloc(31);
	if (command == NULL) {
		ret = 1;
		goto out;
	}

	memcpy(command, inquire_msg, sizeof (inquire_msg));

	ret = libusb_claim_interface(devh, Interface);
	if (ret != 0) {
		SHOW_PROGRESS(output," Could not claim interface (error %d). Skip device inquiry\n", ret);
		goto out;
	}
	libusb_clear_halt(devh, MessageEndpoint);

	ret = usb_bulk_io(devh, MessageEndpoint, (char *)command, 31, 0);
	if (ret < 0) {
		SHOW_PROGRESS(output," INQUIRY message failed (error %d)\n", ret);
		goto out;
	}

	ret = usb_bulk_io(devh, ResponseEndpoint, data, 36, 0);
	if (ret < 0) {
		SHOW_PROGRESS(output," INQUIRY response failed (error %d)\n", ret);
		goto out;
	}

	i = usb_bulk_io(devh, ResponseEndpoint, command, 13, 0);

	fprintf(output,"\nSCSI inquiry data (for identification)\n");
	fprintf(output,"-------------------------\n");

	fprintf(output,"  Vendor String: ");
	for (i = 8; i < 16; i++) printf("%c",data[i]);
	fprintf(output,"\n");

	fprintf(output,"   Model String: ");
	for (i = 16; i < 32; i++) printf("%c",data[i]);
	fprintf(output,"\n");

	fprintf(output,"Revision String: ");
	for (i = 32; i < 36; i++) printf("%c",data[i]);

	fprintf(output,"\n-------------------------\n");

out:
	if (strlen(MessageContent) == 0) {
		libusb_clear_halt(devh, MessageEndpoint);
		libusb_release_interface(devh, Interface);
	}
	free(command);
	return ret;
}


/* Auxiliary function used by the wrapper */
int findMBIMConfig(int vendor, int product, int mode)
{
	struct libusb_device **devs;
	int resultConfig=0;
	int i=0, j;

	if (libusb_get_device_list(ctx, &devs) < 0) {
		perror("Libusb could not access USB. Abort");
		return 0;
	}

	SHOW_PROGRESS(output,"Search USB devices ...\n");
	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor descriptor;
		libusb_get_device_descriptor(dev, &descriptor);

		if (mode == SEARCH_BUSDEV) {
			if ((libusb_get_bus_number(dev) != busnum) ||
				(libusb_get_device_address(dev) != devnum)) {
				continue;
			} else {
				if (descriptor.idVendor != vendor)
					continue;
				if (product != descriptor.idProduct)
					continue;
			}
		}
		SHOW_PROGRESS(output,"Found device, search for MBIM configuration...\n");

		// No check if there is only one configuration
		if (descriptor.bNumConfigurations < 2)
			return -1;

		// Checking all interfaces of all configurations
		for (j=0; j<descriptor.bNumConfigurations; j++) {
			struct libusb_config_descriptor *config;

			libusb_get_config_descriptor(dev, j, &config);
			resultConfig = config->bConfigurationValue;
			for (i=0; i<config->bNumInterfaces; i++) {
				if ( config->interface[i].altsetting[0].bInterfaceClass == 2 )
					if ( config->interface[i].altsetting[0].bInterfaceSubClass == 0x0e ) {
						// found MBIM interface in this configuration
						libusb_free_config_descriptor(config);
						return resultConfig;
					}
			}
			libusb_free_config_descriptor(config);
		}
		return -1;
	}
	return 0;
}

void resetUSB ()
{
	int success;
	int bpoint = 0;

	if (!devh) {
		fprintf(output,"Device handle empty, skip USB reset\n");
		return;
	}
	if (show_progress) {
		fprintf(output,"Reset USB device ");
		fflush(output);
	}
	sleep( 1 );
	do {
		success = libusb_reset_device(devh);
		if ( ((bpoint % 10) == 0) && show_progress ) {
			fprintf(output,".");
			fflush(output);
		}
		bpoint++;
		if (bpoint > 100)
			success = 1;
	} while (success < 0);

	if ( success ) {
		SHOW_PROGRESS(output,"\n Device reset failed.\n");
	} else
		SHOW_PROGRESS(output,"\n Device was reset\n");
}


int switchSendMessage ()
{
	const char* cmdHead = "55534243";
	int ret, i;
	char* msg[3];
	msg[0] = MessageContent;
	msg[1] = MessageContent2;
	msg[2] = MessageContent3;

	/* May be activated in future versions */
//	if (MessageContent2[0] != '\0' || MessageContent3[0] != '\0')
//		NeedResponse = 1;

	SHOW_PROGRESS(output,"Set up interface %d\n", Interface);
	if (InquireDevice != 2) {
		ret = libusb_claim_interface(devh, Interface);
		if (ret != 0) {
			SHOW_PROGRESS(output," Could not claim interface (error %d). Skip message sending\n", ret);
			return 0;
		}
	}
	libusb_clear_halt(devh, MessageEndpoint);
	SHOW_PROGRESS(output,"Use endpoint 0x%02x for message sending ...\n", MessageEndpoint);
	if (show_progress)
		fflush(stdout);

	for (i=0; i<3; i++) {
		if ( strlen(msg[i]) == 0)
			continue;

		if ( sendMessage(msg[i], i+1) )
		goto skip;

		if (NeedResponse) {
			if ( strstr(msg[i],cmdHead) != NULL ) {
				// UFI command
				SHOW_PROGRESS(output,"Read the response to message %d (CSW) ...\n", i+1);
				ret = read_bulk(ResponseEndpoint, ByteString, 13);
			} else {
				// Other bulk transfer
				SHOW_PROGRESS(output,"Read the response to message %d ...\n", i+1);
				ret = read_bulk(ResponseEndpoint, ByteString, strlen(msg[i])/2 );
			}
			if (ret < 0)
				goto skip;
		}
	}

	SHOW_PROGRESS(output,"Reset response endpoint 0x%02x\n", ResponseEndpoint);
	ret = libusb_clear_halt(devh, ResponseEndpoint);
	if (ret)
		SHOW_PROGRESS(output," Could not reset endpoint (probably harmless): %d\n", ret);
	SHOW_PROGRESS(output,"Reset message endpoint 0x%02x\n", MessageEndpoint);
	ret = libusb_clear_halt(devh, MessageEndpoint);
	if (ret)
		SHOW_PROGRESS(output," Could not reset endpoint (probably harmless): %d\n", ret);
	usleep(50000);

	if (ReleaseDelay) {
		SHOW_PROGRESS(output,"Wait for %d ms before releasing interface ...\n", ReleaseDelay);
		usleep(ReleaseDelay*1000);
	}
	ret = libusb_release_interface(devh, Interface);
	if (ret)
		goto skip;
	return 1;

skip:
	SHOW_PROGRESS(output," Device is gone, skip any further commands\n");
	libusb_close(devh);
	devh = 0;
	return 2;
}


int switchConfiguration ()
{
	int count = SWITCH_CONFIG_MAXTRIES; 

	SHOW_PROGRESS(output,"Change configuration to %i ...\n", Configuration);
	while (((ret = libusb_set_configuration(devh, Configuration)) < 0) && --count) {
		SHOW_PROGRESS(output," Device is busy, try to detach kernel driver\n");
		detachDriver();
	}
	if (ret < 0 ) {
		SHOW_PROGRESS(output," Changing the configuration failed (error %d). Try to continue\n", ret);
		return 0;
	} else {
		SHOW_PROGRESS(output," OK, configuration set\n");
		return 1;
	}
}

int switchAltSetting ()
{

	SHOW_PROGRESS(output,"Change to alt setting %i ...\n", AltSetting);
	ret = libusb_claim_interface(devh, Interface);
	ret = libusb_set_interface_alt_setting(devh, Interface, AltSetting);
	libusb_release_interface(devh, Interface);
	if (ret < 0) {
		SHOW_PROGRESS(output," Change to alt setting returned error %d. Try to continue\n", ret);
		return 0;
	} else
		return 1;
}


void switchHuaweiMode ()
{

	SHOW_PROGRESS(output,"Send old Huawei control message ...\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE, \
		LIBUSB_REQUEST_SET_FEATURE, 00000001, 0, (unsigned char *)buffer, 0, 1000);
	if (ret != 0) {
		fprintf(stderr, "Error: Huawei control message failed (error %d). Abort\n\n", ret);
		exit(1);
	}
}


void switchSierraMode ()
{
	SHOW_PROGRESS(output,"Send Sierra control message\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR, 0x0b, 00000001, 0, (unsigned char *)buffer, 0, 1000);
	if (ret == LIBUSB_ERROR_PIPE) {
		SHOW_PROGRESS(output," communication with device stopped. May have switched modes anyway\n");
	    return;
	}
	if (ret < 0) {
		fprintf(stderr, "Error: Sierra control message failed (error %d). Abort\n\n", ret);
	    exit(1);
	}
}


void switchGCTMode ()
{
	ret = libusb_claim_interface(devh, Interface);
	if (ret != 0) {
		SHOW_PROGRESS(output," Could not claim interface (error %d). Skip GCT sequence\n", ret);
		return;
	}
	SHOW_PROGRESS(output,"Send GCT control message 1 ...\n");
	ret = libusb_control_transfer(devh, 0xa1, 0xa0, 0, Interface, (unsigned char *)buffer, 1, 1000);
	if (ret < 0) {
		SHOW_PROGRESS(output," GCT control message 1 failed (error %d), continue anyway ...\n", ret);
	}
	SHOW_PROGRESS(output,"Send GCT control message 2 ...\n");
	ret = libusb_control_transfer(devh, 0xa1, 0xfe, 0, Interface, (unsigned char *)buffer, 1, 1000);
	if (ret < 0) {
		SHOW_PROGRESS(output," GCT control message 2 failed (error %d). Abort\n\n", ret);
	}
	libusb_release_interface(devh, Interface);
	if (ret < 0)
		exit(1);
}


void switchKobilMode() {
	SHOW_PROGRESS(output,"Send Kobil control message ...\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
			0x88, 0, 0, (unsigned char *)buffer, 8, 1000);
	if (ret < 0) {
		fprintf(stderr, "Error: Kobil control message failed (error %d). Abort\n\n", ret);
		exit(1);
	}
}


void switchQisdaMode () {
	SHOW_PROGRESS(output,"Sending Qisda control message ...\n");
	memcpy(buffer, "\x05\x8c\x04\x08\xa0\xee\x20\x00\x5c\x01\x04\x08\x98\xcd\xea\xbf", 16);
	ret = libusb_control_transfer(devh, 0x40, 0x04, 0, 0, (unsigned char *)buffer, 16, 1000);
	if (ret < 0) {
		fprintf(stderr, "Error: Qisda control message failed (error %d). Abort\n\n", ret);
		exit(1);
	}
}


void switchQuantaMode() {
	SHOW_PROGRESS(output,"Send Quanta control message ...\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
			0xff, 0, 0, (unsigned char *)buffer, 0, 1000);
	if (ret < 0) {
		SHOW_PROGRESS(output,"Error: Quanta control message failed (error %d). Abort\n\n", ret);
		exit(1);
	}
}


void switchBlackberryMode ()
{
	SHOW_PROGRESS(output,"Send Blackberry control message 1 ...\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
			0xb1, 0x0000, 0, (unsigned char *)buffer, 8, 1000);
	if (ret != 8) {
		fprintf(stderr, "Error: Blackberry control message 1 failed (result %d)\n", ret);
	}
	SHOW_PROGRESS(output,"Send Blackberry control message 2 ...\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
			0xa9, 0x000e, 0, (unsigned char *)buffer, 2, 1000);
	if (ret != 2) {
		fprintf(stderr, "Error: Blackberry control message 2 failed (result %d). Abort\n\n", ret);
		exit(1);
	}
}


void switchPantechMode()
{
	SHOW_PROGRESS(output,"Send Pantech control message ...\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, 0x70, 2, 0, (unsigned char *)buffer, 0, 1000);
	if (ret < 0) {
		SHOW_PROGRESS(output," Error: Pantech control message failed (error %d). Abort\n\n", ret);
		exit(1);
	}
}


#define EP_OUT 0x02
#define EP_IN 0x81
#define SIZE 0x08

#define MOBILE_ACTION_READLOOP1 63
#define MOBILE_ACTION_READLOOP2 73

/* If anyone can test the MobileAction cable - I bet this
 * function (which is confirmed working) can be greatly
 * simplified ...
 */

void switchActionMode ()
{
	int i;
	SHOW_PROGRESS(output,"Send MobileAction control sequence ...\n");
	memcpy(buffer, "\xb0\x04\x00\x00\x02\x90\x26\x86", SIZE);
	libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_CLASS + LIBUSB_RECIPIENT_INTERFACE, 0x09, 0x0300, 0, (unsigned char *)buffer, SIZE, 1000);
	memcpy(buffer, "\xb0\x04\x00\x00\x02\x90\x26\x86", SIZE);
	libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_CLASS + LIBUSB_RECIPIENT_INTERFACE, 0x09, 0x0300, 0, (unsigned char *)buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	memcpy(buffer, "\x37\x01\xfe\xdb\xc1\x33\x1f\x83", SIZE);
	usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	memcpy(buffer, "\x37\x0e\xb5\x9d\x3b\x8a\x91\x51", SIZE);
	usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	memcpy(buffer, "\x34\x87\xba\x0d\xfc\x8a\x91\x51", SIZE);
	usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	for (i=0; i < MOBILE_ACTION_READLOOP1; i++) {
		usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	}
	memcpy(buffer, "\x37\x01\xfe\xdb\xc1\x33\x1f\x83", SIZE);
	usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	memcpy(buffer, "\x37\x0e\xb5\x9d\x3b\x8a\x91\x51", SIZE);
	usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	memcpy(buffer, "\x34\x87\xba\x0d\xfc\x8a\x91\x51", SIZE);
	usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	for (i=0; i < MOBILE_ACTION_READLOOP2; i++) {
		usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	}
	memcpy(buffer, "\x33\x04\xfe\x00\xf4\x6c\x1f\xf0", SIZE);
	usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	memcpy(buffer, "\x32\x07\xfe\xf0\x29\xb9\x3a\xf0", SIZE);
	ret = usb_interrupt_io(devh, EP_OUT, buffer, SIZE, 1000);
	usb_interrupt_io(devh, EP_IN, buffer, SIZE, 1000);
	if (ret < 0) {
		SHOW_PROGRESS(output," MobileAction control sequence did not complete\n Last error was %d\n",ret);
	} else {
		SHOW_PROGRESS(output," MobileAction control sequence complete\n");
	}
}


#define SQN_SET_DEVICE_MODE_REQUEST		0x0b
#define SQN_GET_DEVICE_MODE_REQUEST		0x0a

#define SQN_DEFAULT_DEVICE_MODE			0x00
#define SQN_MASS_STORAGE_MODE			0x01
#define SQN_CUSTOM_DEVICE_MODE			0x02

void switchSequansMode() {

	SHOW_PROGRESS(output,"Send Sequans control message\n");
	ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SQN_SET_DEVICE_MODE_REQUEST, SQN_CUSTOM_DEVICE_MODE, 0, (unsigned char *)buffer, 0, 1000);
	if (ret < 0) {
		fprintf(stderr, "Error: Sequans request failed (error %d). Abort\n\n", ret);
	    exit(1);
	}
}

void switchCiscoMode() {
	int i;
	char* msg[11];

	msg[0] = "55534243f83bcd810002000080000afd000000030000000100000000000000";
	msg[1] = "55534243984300820002000080000afd000000070000000100000000000000";
	msg[2] = "55534243984300820000000000000afd000100071000000000000000000000";
	msg[3] = "55534243984300820002000080000afd000200230000000100000000000000";
	msg[4] = "55534243984300820000000000000afd000300238200000000000000000000";
	msg[5] = "55534243984300820002000080000afd000200260000000100000000000000";
	msg[6] = "55534243984300820000000000000afd00030026c800000000000000000000";
	msg[7] = "55534243d84c04820002000080000afd000010730000000100000000000000";
	msg[8] = "55534243d84c04820002000080000afd000200240000000100000000000000";
	msg[9] = "55534243d84c04820000000000000afd000300241300000000000000000000";
	msg[10] = "55534243d84c04820000000000000afd000110732400000000000000000000";

	SHOW_PROGRESS(output,"Set up Cisco interface %d\n", Interface);
	ret = libusb_claim_interface(devh, Interface);
	if (ret < 0) {
		SHOW_PROGRESS(output," Could not claim interface (error %d). Abort\n", ret);
		exit(1);
	}
//	libusb_clear_halt(devh, MessageEndpoint);
	if (show_progress)
		fflush(output);

	for (i=0; i<11; i++) {
		if ( sendMessage(msg[i], i+1) )
			goto skip;

		SHOW_PROGRESS(output," Read the response (CSW) to bulk message %d ...\n",i+1);
		ret = read_bulk(ResponseEndpoint, ByteString, 13);
		if (ret < 0)
			goto skip;
	}

	if (ReleaseDelay) {
		SHOW_PROGRESS(output,"Wait for %d ms before releasing interface ...\n", ReleaseDelay);
		usleep(ReleaseDelay*1000);
	}
	ret = libusb_release_interface(devh, Interface);
	if (ret < 0)
		goto skip;
	return;

skip:
	SHOW_PROGRESS(output,"Device returned error %d, skip further commands\n", ret);
	libusb_close(devh);
	devh = 0;
}


int switchSonyMode ()
{
	int i, found;
	detachDriver();

	if (CheckSuccess) {
		CheckSuccess = 0;
	}

	SHOW_PROGRESS(output,"Send Sony control message\n");
	ret = libusb_control_transfer(devh, 0xc0, 0x11, 2, 0, (unsigned char *)buffer, 3, 100);
	if (ret < 0) {
		fprintf(stderr, "Error: Sony control message failed (error %d). Abort\n\n", ret);
		exit(1);
	} else
		SHOW_PROGRESS(output," OK, control message sent, wait for device to return ...\n");

	libusb_close(devh);
	devh = 0;

	/* Now waiting for the device to reappear */
	devnum=-1;
	busnum=-1;
	i=0;
	dev = 0;
	while ( dev == 0 && i < 30 ) {
		if ( i > 5 ) {
			dev = search_devices(&found, DefaultVendor, DefaultProductList, TargetClass, 0, SEARCH_TARGET);
		}
		if ( dev != 0 )
			break;
		sleep(1);
		if (show_progress) {
			fprintf(output,"#");
			fflush(stdout);
		}
		i++;
	}
	SHOW_PROGRESS(output,"\n After %d seconds:",i);
	if ( dev ) {
		SHOW_PROGRESS(output," device came back, proceed\n");
		libusb_open(dev, &devh);
		if (devh == 0) {
			fprintf(stderr, "Error: could not get handle on device\n");
			return 0;
		}
	} else {
		SHOW_PROGRESS(output," device still gone, abort\n");
		return 0;
	}
	sleep(1);

	SHOW_PROGRESS(output,"Send Sony control message again ...\n");
	ret = libusb_control_transfer(devh, 0xc0, 0x11, 2, 0, (unsigned char *)buffer, 3, 100);
	if (ret < 0) {
		fprintf(stderr, "Error: Sony control message (2) failed (error %d)\n", ret);
		return 0;
	}
	SHOW_PROGRESS(output," OK, control message sent\n");
	return 1;
}


/* Detach driver
 */
int detachDriver()
{

	// Driver already detached during SCSI inquiry ?
	if (InquireDevice == 2)
		return 1;
	SHOW_PROGRESS(output,"Looking for active driver ...\n");
	ret = libusb_kernel_driver_active(devh, 0);
	if (ret == LIBUSB_ERROR_NOT_SUPPORTED) {
		fprintf(output," Can't do driver detection on this platform.\n");
		return 2;
	}
	if (ret < 0) {
		fprintf(output," Driver check failed with error %d. Try to continue\n", ret);
		return 2;
	}
	if (ret == 0) {
		SHOW_PROGRESS(output," No active driver found. Detached before or never attached\n");
		return 1;
	}

	ret = libusb_detach_kernel_driver(devh, Interface);
	if (ret == LIBUSB_ERROR_NOT_SUPPORTED) {
		fprintf(output," Can't do driver detaching on this platform.\n");
		return 2;
	}
	if (ret == 0) {
		SHOW_PROGRESS(output," OK, driver detached\n");
	} else
		SHOW_PROGRESS(output," Driver detach failed (error %d). Try to continue\n", ret);
	return 1;
}


int sendMessage(char* message, int count)
{
	int message_length;

	if (strlen(message) % 2 != 0) {
		fprintf(stderr, "Error: MessageContent %d hex string has uneven length. Skipping ...\n", count);
		return 1;
	}
	message_length = strlen(message) / 2;
	if ( hexstr2bin(message, ByteString, message_length) == -1) {
		fprintf(stderr, "Error: MessageContent %d %s\n is not a hex string. Skipping ...\n", count, MessageContent);
		return 1;
	}
	SHOW_PROGRESS(output,"Trying to send message %d to endpoint 0x%02x ...\n", count, MessageEndpoint);
	fflush(output);
	ret = write_bulk(MessageEndpoint, ByteString, message_length);
	if (ret == LIBUSB_ERROR_NO_DEVICE)
		return 1;

	return 0;
}


int checkSuccess()
{
	int i=0;
	int newTargetCount, success=0;

	SHOW_PROGRESS(output,"\nCheck for mode switch (max. %d times, once per second) ...\n", CheckSuccess);
	sleep(1);

	/* If target parameters are given, don't check for vanished device
	 * Changed for Cisco AM10 where a new device is added while the install
	 * storage device stays active
	 */
	if ((TargetVendor || TargetClass) && devh) {
		libusb_close(devh);
		devh = 0;
	}

	/* if target ID is not given but target class is, assign default as target;
	 * it will be needed for sysmode output
	 */
	if (!TargetVendor && TargetClass) {
		TargetVendor = DefaultVendor;
		TargetProduct = DefaultProduct;
	}

	/* devh is 0 if device vanished during command transmission or if target params were given
	 */
	if (devh)
		for (i=0; i < CheckSuccess; i++) {

			/* Test if default device still can be accessed; positive result does
			 * not necessarily mean failure
			 */
			SHOW_PROGRESS(output," Wait for original device to vanish ...\n");

			ret = libusb_claim_interface(devh, Interface);
			libusb_release_interface(devh, Interface);
			if (ret < 0) {
				SHOW_PROGRESS(output," Original device can't be accessed anymore. Good.\n");
				libusb_close(devh);
				devh = 0;
				break;
			}
			if (i == CheckSuccess-1) {
				SHOW_PROGRESS(output," Original device still present after the timeout\n\nMode switch most likely failed. Bye!\n\n");
			} else
				sleep(1);
		}

	if ( TargetVendor && (TargetProduct > -1 || TargetProductList[0] != '\0') ) {

		/* Recount target devices (compare with previous count) if target data is given.
		 * Target device on the same bus with higher device number is returned,
		 * description is read for syslog message
		 */
		for (i=i; i < CheckSuccess; i++) {
			SHOW_PROGRESS(output," Search for target devices ...\n");
			dev = search_devices(&newTargetCount, TargetVendor, TargetProductList, TargetClass, 0, SEARCH_TARGET);
			if (dev && (newTargetCount > targetDeviceCount)) {
				fprintf(output,"\nFound target device, open it\n");
				libusb_open(dev, &devh);
				deviceDescription();
				libusb_close(devh);
				devh = 0;
				if (verbose) {
					fprintf(output,"\nFound target device %03d on bus %03d\n", \
					libusb_get_device_address(dev), libusb_get_bus_number(dev));
					fprintf(output,"\nTarget device description data\n");
					fprintf(output,"-------------------------\n");
					fprintf(output,"Manufacturer: %s\n", imanufact);
					fprintf(output,"     Product: %s\n", iproduct);
					fprintf(output,"  Serial No.: %s\n", iserial);
					fprintf(output,"-------------------------\n");
				}
				SHOW_PROGRESS(output," Found correct target device\n\nMode switch succeeded. Bye!\n\n");
				success = 2;
				break;
			}
			if (i == CheckSuccess-1) {
				SHOW_PROGRESS(output," No new devices in target mode or class found\n\nMode switch has failed. Bye!\n\n");
			} else
				sleep(1);
		}
	} else
		/* No target data given, rely on the vanished device */
		if (!devh) {
			SHOW_PROGRESS(output," (For a better success check provide target IDs or class)\n");
			SHOW_PROGRESS(output," Original device vanished after switching\n\nMode switch most likely succeeded. Bye!\n\n");
			success = 1;
		}

	switch (success) {
		case 3: 
			if (sysmode)
				syslog(LOG_NOTICE, "switched to new device, but hit libusb1 bug");
			TargetProduct = -1;
			success = 1;
			break;
		case 2: 
			if (sysmode)
				syslog(LOG_NOTICE, "switched to %04x:%04x on %03d/%03d", TargetVendor, TargetProduct, busnum, devnum);
			success = 1;
			break;
		case 1:
			if (sysmode)
				syslog(LOG_NOTICE, "device seems to have switched");
		default:
			;
	}
	if (sysmode)
		closelog();

	return success;

}


int write_bulk(int endpoint, char *message, int length)
{
	ret = usb_bulk_io(devh, endpoint, message, length, 3000);
	if (ret >= 0 ) {
		SHOW_PROGRESS(output," OK, message successfully sent\n");
	} else
		if (ret == LIBUSB_ERROR_NO_DEVICE) {
			SHOW_PROGRESS(output," Device seems to have vanished right after sending. Good.\n");
		} else
			SHOW_PROGRESS(output," Sending the message returned error %d. Try to continue\n", ret);
	return ret;

}

int read_bulk(int endpoint, char *buffer, int length)
{
	ret = usb_bulk_io(devh, endpoint, buffer, length, 3000);
	if (ret >= 0 ) {
		SHOW_PROGRESS(output," Response successfully read (%d bytes).\n", ret);
	} else
		if (ret == LIBUSB_ERROR_NO_DEVICE) {
			SHOW_PROGRESS(output," Device seems to have vanished after reading. Good.\n");
		} else
			SHOW_PROGRESS(output," Response reading failed (error %d)\n", ret);
	return ret;

}

void release_usb_device(int __attribute__((unused)) dummy) {
	SHOW_PROGRESS(output,"Program cancelled by system. Bye!\n\n");
	if (devh) {
		libusb_release_interface(devh, Interface);
		libusb_close(devh);
	}
	if (sysmode)
		closelog();
	exit(0);

}


/* Iterates over busses and devices, counts the ones which match the given
 * parameters and returns the last one of them
*/
struct libusb_device* search_devices( int *numFound, int vendor, char* productList, int targetClass, int configuration, int mode)
{
	char *listcopy=NULL, *token, buffer[2];
	int devClass, product;
	struct libusb_device* right_dev = NULL;
//	struct libusb_device_handle *testdevh;
	struct libusb_device **devs;
	int i=0;

	/* only target class given, target vendor and product assumed unchanged */
	if ( targetClass && !(vendor || strlen(productList)) ) {
		vendor = DefaultVendor;
		productList = DefaultProductList;
	}
	*numFound = 0;

	/* Sanity check */
	if (!vendor || productList == '\0')
		return NULL;

	listcopy = malloc(strlen(productList)+1);

	if (libusb_get_device_list(ctx, &devs) < 0) {
		perror("Libusb failed to get USB access!");
		return 0;
	}

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor descriptor;
		libusb_get_device_descriptor(dev, &descriptor);

		if (mode == SEARCH_BUSDEV) {
			if ((libusb_get_bus_number(dev) != busnum) ||
				(libusb_get_device_address(dev) != devnum))
				continue;
			else
				SHOW_PROGRESS(output," bus/device number matched\n");
		}

		if (verbose)
			fprintf (output,"  found USB ID %04x:%04x\n",
					descriptor.idVendor, descriptor.idProduct);
		if (descriptor.idVendor != vendor)
			continue;
		if (verbose)
			fprintf (output,"   vendor ID matched\n");

		strcpy(listcopy, productList);
		token = strtok(listcopy, ",");
		while (token != NULL) {
			if (strlen(token) != 4) {
				SHOW_PROGRESS(output,"Error: entry in product ID list has wrong length: %s. Ignored\n", token);
				goto NextToken;
			}
			if ( hexstr2bin(token, buffer, strlen(token)/2) == -1) {
				SHOW_PROGRESS(output,"Error: entry in product ID list is not a hex string: %s. Ignored\n", token);
				goto NextToken;
			}
			product = 0;
			product += (unsigned char)buffer[0];
			product <<= 8;
			product += (unsigned char)buffer[1];
			if (product == descriptor.idProduct) {
				SHOW_PROGRESS(output,"   product ID matched\n");

				if (targetClass != 0) {
					// TargetClass is set, check class of first interface
					struct libusb_device_descriptor descriptor;
					libusb_get_device_descriptor(dev, &descriptor);
					devClass = descriptor.bDeviceClass;
					struct libusb_config_descriptor *config;
					libusb_get_config_descriptor(dev, 0, &config);
					int ifaceClass = config->interface[0].altsetting[0].bInterfaceClass;
					libusb_free_config_descriptor(config);
					if (devClass == 0)
						devClass = ifaceClass;
					else
						/* Check for some quirky devices */
						if (devClass != ifaceClass)
							devClass = ifaceClass;
					if (devClass == targetClass) {
						if (verbose)
							fprintf (output,"   target class %02x matches\n", targetClass);
						if (mode == SEARCH_TARGET) {
							(*numFound)++;
							right_dev = dev;
							if (verbose)
								fprintf (output,"   count device\n");
						} else
							if (verbose)
								fprintf (output,"   device not counted, target class reached\n");
					} else {
						if (verbose)
							fprintf (output,"   device class %02x not matching target\n", devClass);
						if (mode == SEARCH_DEFAULT || mode == SEARCH_BUSDEV) {
							(*numFound)++;
							right_dev = dev;
							if (verbose)
								fprintf (output,"   count device\n");
						}
					}
				} else if (configuration > 0) {
					// Configuration parameter is set, check device configuration
					int testconfig = get_current_configuration(dev);
					if (testconfig != configuration) {
						if (verbose)
							fprintf (output,"   device configuration %d not matching target\n", testconfig);
						(*numFound)++;
						right_dev = dev;
						if (verbose)
							fprintf (output,"   count device\n");
					} else
						if (verbose)
							fprintf (output,"   device not counted, target configuration reached\n");
				} else {
					// Neither TargetClass nor Configuration are set
					(*numFound)++;
					right_dev = dev;
					if (mode == SEARCH_BUSDEV)
						break;
				}
			}

			NextToken:
			token = strtok(NULL, ",");
		}
	}
	if (listcopy != NULL)
		free(listcopy);
	return right_dev;
}


#define USB_DIR_OUT 0x00
#define USB_DIR_IN  0x80

/* Autodetect bulk endpoints (ab) */

int find_first_bulk_endpoint(int direction)
{
	int i, j;
	const struct libusb_interface_descriptor *alt;
	const struct libusb_endpoint_descriptor *ep;

	for (j=0; j < active_config->bNumInterfaces; j++) {
		alt = &(active_config->interface[j].altsetting[0]);
		if (alt->bInterfaceNumber == Interface) {
			for(i=0; i < alt->bNumEndpoints; i++) {
				ep=&(alt->endpoint[i]);
				if( ( (ep->bmAttributes & LIBUSB_ENDPOINT_ADDRESS_MASK) == LIBUSB_TRANSFER_TYPE_BULK) &&
				    ( (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == direction ) ) {
					return ep->bEndpointAddress;
				}
			}
		}
	}
	return 0;
}

int get_current_configuration()
{
	int cfg;
	SHOW_PROGRESS(output,"Get the current device configuration ...\n");
	if (active_config == NULL)
		libusb_get_active_config_descriptor(dev, &active_config);

	cfg = active_config->bConfigurationValue;
	libusb_free_config_descriptor(active_config);
	if (ret < 0)
		exit(1);
	else
		return cfg;
}

int get_interface_class()
{
	int i;
	for (i=0; i < active_config->bNumInterfaces; i++) {
		if (active_config->interface[i].altsetting[0].bInterfaceNumber == Interface)
			return active_config->interface[i].altsetting[0].bInterfaceClass;
	}
	return -1;
}

/* Parameter parsing */

char* ReadParseParam(const char* FileName, char *VariableName)
{
	static int numLines = 0;
	static char* ConfigBuffer[MAXLINES];
	char *VarName, *Comment=NULL, *Equal=NULL;
	char *FirstQuote, *LastQuote, *P1, *P2;
	int Line=0;
	unsigned Len=0, Pos=0;
	char Str[LINE_DIM], *token, *configPos;
	FILE *file = NULL;

	// Reading and storing input during the first call
	if (numLines==0) {
		if (strncmp(FileName,"##",2) == 0) {
			if (verbose) fprintf(output,"\nRead long config from command line\n");
			// "Embedded" configuration data
			configPos = (char*)FileName;
			token = strtok(configPos, "\n");
			strncpy(Str,token,LINE_DIM-1);
		} else {
			if (strcmp(FileName, "stdin")==0) {
				if (verbose) fprintf(output,"\nRead long config from stdin\n");
				file = stdin;
			} else {
				if (verbose) fprintf(output,"\nRead config file: %s\n", FileName);
				file=fopen(FileName, "r");
			}
			if (file==NULL) {
				fprintf(stderr, "Error: Could not find file %s. Abort\n\n", FileName);
				exit(1);
			} else {
				token = fgets(Str, LINE_DIM-1, file);
			}
		}
		while (token != NULL && numLines < MAXLINES) {
//			Line++;
			Len=strlen(Str);
			if (Len==0)
				goto NextLine;
			if (Str[Len-1]=='\n' or Str[Len-1]=='\r')
				Str[--Len]='\0';
			Equal = strchr (Str, '=');			// search for equal sign
			Pos = strcspn (Str, ";#!");			// search for comment
			Comment = (Pos==Len) ? NULL : Str+Pos;
			if (Equal==NULL or ( Comment!=NULL and Comment<=Equal))
				goto NextLine;	// Comment or irrelevant, don't save
			Len=strlen(Str)+1;
			ConfigBuffer[numLines] = malloc(Len*sizeof(char));
			strcpy(ConfigBuffer[numLines],Str);
			numLines++;
		NextLine:
			if (file == NULL) {
				token = strtok(NULL, "\n");
				if (token != NULL)
					strncpy(Str,token,LINE_DIM-1);
			} else
				token = fgets(Str, LINE_DIM-1, file);
		}
		if (file != NULL)
			fclose(file);
	}

	// Now checking for parameters
	Line=0;
	while (Line < numLines) {
		strcpy(Str,ConfigBuffer[Line]);
		Equal = strchr (Str, '=');			// search for equal sign
		*Equal++ = '\0';

		// String
		FirstQuote=strchr (Equal, '"');		// search for double quote char
		LastQuote=strrchr (Equal, '"');
		if (FirstQuote!=NULL) {
			if (LastQuote==NULL) {
				fprintf(stderr, "Error reading parameters from file %s - Missing end quote:\n%s\n", FileName, Str);
				goto Next;
			}
			*FirstQuote=*LastQuote='\0';
			Equal=FirstQuote+1;
		}

		// removes leading/trailing spaces
		Pos=strspn (Str, " \t");
		if (Pos==strlen(Str)) {
			fprintf(stderr, "Error reading parameters from file %s - Missing variable name:\n%s\n", FileName, Str);
			goto Next;
		}
		while ((P1=strrchr(Str, ' '))!=NULL or (P2=strrchr(Str, '\t'))!=NULL)
			if (P1!=NULL) *P1='\0';
			else if (P2!=NULL) *P2='\0';
		VarName=Str+Pos;

		Pos=strspn (Equal, " \t");
		if (Pos==strlen(Equal)) {
			fprintf(stderr, "Error reading parameter from file %s - Missing value:\n%s\n", FileName, Str);
			goto Next;
		}
		Equal+=Pos;

		if (strcmp(VarName, VariableName)==0) {		// Found it
			return Equal;
		}
	Next:
		Line++;
	}

	return NULL;
}


int hex2num(char c)
{
	if (c >= '0' && c <= '9')
	return c - '0';
	if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
	return -1;
}


int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int hexstr2bin(const char *hex, char *buffer, int len)
{
	int i;
	int a;
	const char *ipos = hex;
	char *opos = buffer;

	for (i = 0; i < len; i++) {
	a = hex2byte(ipos);
	if (a < 0)
		return -1;
	*opos++ = a;
	ipos += 2;
	}
	return 0;
}

void printVersion()
{
	char* version = VERSION;
	fprintf(output,"\n * usb_modeswitch: handle USB devices with multiple modes\n"
		" * Version %s (C) Josua Dietze 2014\n"
		" * Based on libusb1/libusbx\n\n"
		" ! PLEASE REPORT NEW CONFIGURATIONS !\n\n", version);
}

void printHelp()
{
	fprintf(output,"\nUsage: usb_modeswitch [<params>] [-c filename]\n\n"
	" -h, --help                    this help\n"
	" -e, --version                 print version information and exit\n"
	" -j, --find-mbim               return config no. with MBIM interface, exit\n\n"
	" -v, --default-vendor NUM      vendor ID of original mode (mandatory)\n"
	" -p, --default-product NUM     product ID of original mode (mandatory)\n"
	" -V, --target-vendor NUM       target mode vendor ID (optional)\n"
	" -P, --target-product NUM      target mode product ID (optional)\n"
	" -C, --target-class NUM        target mode device class (optional)\n"
	" -b, --bus-num NUM             system bus number of device (for hard ID)\n"
	" -g, --device-num NUM          system device number (for hard ID)\n"
	" -m, --message-endpoint NUM    direct the message transfer there (optional)\n"
	" -M, --message-content <msg>   message to send (hex number as string)\n"
	" -2 <msg>, -3 <msg>            additional messages to send (-n recommended)\n"
	" -n, --need-response           read response to the message transfer (CSW)\n"
	" -r, --response-endpoint NUM   read response from there (optional)\n"
	" -K, --std-eject               send standard EJECT sequence\n"
	" -d, --detach-only             detach the active driver, no further action\n"
	" -H, --huawei-mode             apply a special procedure\n"
	" -S, --sierra-mode             apply a special procedure\n"
	" -O, --sony-mode               apply a special procedure\n"
	" -G, --gct-mode                apply a special procedure\n"
	" -N, --sequans-mode            apply a special procedure\n"
	" -A, --mobileaction-mode       apply a special procedure\n"
	" -T, --kobil-mode              apply a special procedure\n"
	" -L, --cisco-mode              apply a special procedure\n"
	" -B, --qisda-mode              apply a special procedure\n"
	" -E, --quanta-mode             apply a special procedure\n"
	" -R, --reset-usb               reset the device after all other actions\n"
	" -Q, --quiet                   don't show progress or error messages\n"
	" -W, --verbose                 print all settings and debug output\n"
	" -D, --sysmode                 specific result and syslog message\n"
	" -s, --success <seconds>       switching result check with timeout\n"
	" -I, --inquire                 retrieve SCSI attributes initially\n\n"
	" -c, --config-file <filename>  load long configuration from file\n\n"
	" -t, --stdinput                read long configuration from stdin\n\n"
	" -f, --long-config <text>      get long configuration from string\n\n"
	" -i, --interface NUM           select initial USB interface (default 0)\n"
	" -u, --configuration NUM       select USB configuration\n"
	" -a, --altsetting NUM          select alternative USB interface setting\n\n");
}

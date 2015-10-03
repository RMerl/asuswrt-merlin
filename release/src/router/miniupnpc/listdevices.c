/* $Id: listdevices.c,v 1.2 2014/11/17 09:50:56 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2013-2014 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution. */

#include <stdio.h>
#include <string.h>
#include "miniupnpc.h"

int main(int argc, char * * argv)
{
	const char * searched_device = NULL;
	const char * multicastif = 0;
	const char * minissdpdpath = 0;
	int ipv6 = 0;
	int error = 0;
	struct UPNPDev * devlist = 0;
	struct UPNPDev * dev;
	int i;

	for(i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-6") == 0)
			ipv6 = 1;
		else if(strcmp(argv[i], "-d") == 0) {
			if(++i >= argc) {
				fprintf(stderr, "-d option needs one argument\n");
				return 1;
			}
			searched_device = argv[i];
		} else if(strcmp(argv[i], "-m") == 0) {
			if(++i >= argc) {
				fprintf(stderr, "-m option needs one argument\n");
				return 1;
			}
			multicastif = argv[i];
		} else {
			printf("usage : %s [options]\n", argv[0]);
			printf("options :\n");
			printf("   -6 : use IPv6\n");
			printf("   -d <device string> : search only for this type of device\n");
			printf("   -m address/ifname : network interface to use for multicast\n");
			printf("   -h : this help\n");
			return 1;
		}
	}

	if(searched_device) {
		printf("searching UPnP device type %s\n", searched_device);
		devlist = upnpDiscoverDevice(searched_device,
		                             2000, multicastif, minissdpdpath,
		                             0/*sameport*/, ipv6, &error);
	} else {
		printf("searching all UPnP devices\n");
		devlist = upnpDiscoverAll(2000, multicastif, minissdpdpath,
		                             0/*sameport*/, ipv6, &error);
	}
	if(devlist) {
		for(dev = devlist; dev != NULL; dev = dev->pNext) {
			printf("%-48s\t%s\n", dev->st, dev->descURL);
		}
		freeUPNPDevlist(devlist);
	} else {
		printf("no device found.\n");
	}

	return 0;
}


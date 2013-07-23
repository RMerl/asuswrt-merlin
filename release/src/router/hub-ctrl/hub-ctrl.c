/*
* Copyright (C) 2006 Free Software Initiative of Japan
*
* Author: NIIBE Yutaka  <gniibe at fsij.org>
* with changes by rss 14.03.2010
*
* This file can be distributed under the terms and conditions of the
* GNU General Public License version 2 (or later).
*
*/

#include <errno.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USB_RT_HUB			(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE)
#define USB_RT_PORT			(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_OTHER)
#define USB_PORT_FEAT_POWER		8
#define USB_PORT_FEAT_INDICATOR         22

#ifndef LIBUSB_DT_SUPERSPEED_HUB
#define LIBUSB_DT_SUPERSPEED_HUB 0x2a
#endif

#define COMMAND_SET_NONE  0
#define COMMAND_SET_LED   1
#define COMMAND_SET_POWER 2
#define HUB_LED_GREEN 2

static struct libusb_context *ctx = NULL;

static void
usage (const char *progname)
{
	fprintf (stderr,
		"Usage: %s [{-h HUBNUM | -b BUSNUM -d DEVNUM}] \\\n"
		"          [-P PORT] [{-p [VALUE]|-l [VALUE]}]\n"
		"Examples:\n"
		" *   # hub-ctrl -v                 // List hubs available\n"
		" *   # hub-ctrl -P 1               // Power off at port 1\n"
		" *   # hub-ctrl -P 1 -p 1          // Power on at port 1\n"
		" *   # hub-ctrl -P 2 -l            // LED on at port 2\n"
		, progname);
}

static void
exit_with_usage (const char *progname)
{
	usage (progname);
	exit (1);
}

#define HUB_CHAR_LPSM		0x0003
#define HUB_CHAR_PORTIND        0x0080

struct usb_hub_descriptor {
	unsigned char bDescLength;
	unsigned char bDescriptorType;
	unsigned char bNbrPorts;
	unsigned char wHubCharacteristics[2];
	unsigned char bPwrOn2PwrGood;
	unsigned char bHubContrCurrent;
	unsigned char data[0];
};

#define CTRL_TIMEOUT 1000
#define USB_STATUS_SIZE 4

#define MAX_HUBS 128
struct hub_info {
	int busnum, devnum;
	struct libusb_device *dev;
	int nport;
	int indicator_support;
	int usb3;
};

static struct hub_info hubs[MAX_HUBS];
static int number_of_hubs_with_feature;

static void
hub_port_status (libusb_device_handle *uh, int nport, int is3)
{
	int i;

	printf(" Hub Port Status: (%d)\n", nport);
	for (i = 0; i < nport; i++)
	{
		unsigned char buf[USB_STATUS_SIZE];
		int ret;

		ret = libusb_control_transfer (uh,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_OTHER,
			LIBUSB_REQUEST_GET_STATUS, 
			0, i + 1,
			buf, USB_STATUS_SIZE,
			CTRL_TIMEOUT);
		if (ret < 0)
		{
			fprintf (stderr,
				"Error %d, cannot read port %d status, %s (%d)\n",
				ret, i + 1, strerror(errno), errno);
			//	  break;
			continue;
		}

		printf("   Port %d: %02x%02x.%02x%02x", i + 1,
			buf[3], buf [2],
			buf[1], buf [0]);

		printf("%s%s%s%s%s%s%s%s",
		 is3 && (buf[2] & 0x80) ? " C_CONFIG_ERROR" : "",
		 is3 && (buf[2] & 0x40) ? " C_LINK_STATE" : "",
		 is3 && (buf[2] & 0x20) ? " C_BH_RESET" : "",
			(buf[2] & 0x10) ? " C_RESET" : "",
			(buf[2] & 0x08) ? " C_OC" : "",
			(buf[2] & 0x04) ? " C_SUSPEND" : "",
			(buf[2] & 0x02) ? " C_ENABLE" : "",
			(buf[2] & 0x01) ? " C_CONNECT" : "");

		printf("%s%s%s%s%s%s%s%s%s%s%s%s\n",
		 is3 && (buf[0] & 0x02) &&
			(buf[1] & 0x1c) == 0 ? " 5gbps" : "",
		 is3 && (buf[1] & 0x02) ? " power" : "",
		!is3 && (buf[1] & 0x10) ? " indicator" : "",
		!is3 && (buf[1] & 0x08) ? " test" : "",
		!is3 && (buf[1] & 0x04) ? " highspeed" : "",
		!is3 && (buf[1] & 0x02) ? " lowspeed" : "",
		!is3 && (buf[1] & 0x01) ? " power" : "",
			(buf[0] & 0x10) ? " reset" : "",
			(buf[0] & 0x08) ? " oc" : "",
		!is3 && (buf[0] & 0x04) ? " suspend" : "",
			(buf[0] & 0x02) ? " enable" : "",
			(buf[0] & 0x01) ? " connect" : "");
	}
}

static int
usb_find_hubs (int listing, int verbose, int busnum, int devnum, int hub)
{

	struct libusb_device **devs;
	struct libusb_device *dev;
	int i = 0;
	int r;
	number_of_hubs_with_feature = 0;

	if (libusb_get_device_list( ctx, &devs ) < 0){
		perror ("failed to access USB");
		return 0;
	}

	while ( (dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		unsigned short dev_vid, dev_pid, dev_bcd;

		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			continue;
		}
		dev_vid = libusb_le16_to_cpu(desc.idVendor);
		dev_pid = libusb_le16_to_cpu(desc.idProduct);
		dev_bcd = libusb_le16_to_cpu(desc.bcdUSB);
		//wmlog_msg(1, "Bus %03d Device %03d: ID %04x:%04x", libusb_get_bus_number(dev), libusb_get_device_address(dev), dev_vid, dev_pid);

		libusb_device_handle *uh;
		int print = 0;
		int usb3 = (dev_bcd >= 0x0300);

		//printf("found dev %d\n", dev->descriptor.bDeviceClass);
		if (desc.bDeviceClass != LIBUSB_CLASS_HUB)
			continue;


		if (listing
			|| (verbose ))
			//		  && ((atoi (bus->dirname) == busnum && dev->devnum == devnum)
			//		      || hub == number_of_hubs_with_feature)))
			print = 1;

		if( libusb_open (dev, &uh) != 0 )
			continue;

/*		if(libusb_detach_kernel_driver( uh, 0 ))
			perror("libusb_detach_kernel_driver");

		if(libusb_claim_interface( uh, 0 ))
			perror("libusb_claim_interface");*/

		if ( uh != NULL )
		{	    

			if (print)
				printf ("Hub #%d at \tBUS:DEV\t\t%03d:%03d\n\t\tUSB VEND:PROD: \t%04x:%04x\n",
				number_of_hubs_with_feature,
				libusb_get_bus_number( dev ), 
				libusb_get_device_address( dev ),
				dev_vid, dev_pid
				);

			char buf[ sizeof(struct usb_hub_descriptor) + 2*4 ];
			int len;
			int nport;
			struct usb_hub_descriptor *uhd = (struct usb_hub_descriptor *)buf;

			if ( (len = libusb_control_transfer ( uh,
				LIBUSB_ENDPOINT_IN | USB_RT_HUB,
				LIBUSB_REQUEST_GET_DESCRIPTOR,
				(usb3 ? LIBUSB_DT_SUPERSPEED_HUB : LIBUSB_DT_HUB)<<8, 0,
				buf, (int)sizeof ( buf ),
				CTRL_TIMEOUT ) )
//			if( libusb_get_descriptor( uh, LIBUSB_DT_HUB, 0, buf, sizeof(buf) )
			    >(int)sizeof (struct usb_hub_descriptor) )
			{
				if (!(!(uhd->wHubCharacteristics[0] & HUB_CHAR_PORTIND)
					&& (uhd->wHubCharacteristics[0] & HUB_CHAR_LPSM) >= 2)){

						switch ((uhd->wHubCharacteristics[0] & HUB_CHAR_LPSM))
						{
						case 0:
							if (print)
								fprintf (stderr, " INFO: ganged switching.\n");
							break;
						case 1:
							if (print)
								fprintf (stderr, " INFO: individual power switching.\n");
							break;
						case 2:
						case 3:
							if (print)
								fprintf (stderr, " WARN: No power switching.\n");
							break;
						}

						if (print
							&& !(uhd->wHubCharacteristics[0] & HUB_CHAR_PORTIND))
							fprintf (stderr, " WARN: Port indicators are NOT supported.\n");
				}
			}
			else
			{
				perror ("Can't get hub descriptor.");
				printf( "%d\n",len );
			}

			if( len > 0 )
			{
				nport = buf[2];
				hubs[number_of_hubs_with_feature].busnum = libusb_get_bus_number( dev );
				hubs[number_of_hubs_with_feature].devnum = libusb_get_device_address( dev );
				hubs[number_of_hubs_with_feature].dev = dev;
				hubs[number_of_hubs_with_feature].indicator_support =
					(uhd->wHubCharacteristics[0] & HUB_CHAR_PORTIND)? 1 : 0;
				hubs[number_of_hubs_with_feature].nport = nport;
				hubs[number_of_hubs_with_feature].usb3 = usb3;

				number_of_hubs_with_feature++;

				if (verbose)
					hub_port_status (uh, nport, usb3);
			}
/*
			libusb_release_interface( uh,0 );
			libusb_attach_kernel_driver( uh, 0); */

			libusb_close (uh);
		}
	}
	libusb_free_device_list( devs, 1 );


	return number_of_hubs_with_feature;
}

int
get_hub (int busnum, int devnum)
{
	int i;

	for (i = 0; i < number_of_hubs_with_feature; i++)
		if (hubs[i].busnum == busnum && hubs[i].devnum == devnum)
			return i;

	return -1;
}

/*
* HUB-CTRL  -  program to control port power/led of USB hub
*
*   # hub-ctrl                    // List hubs available
*   # hub-ctrl -P 1               // Power off at port 1
*   # hub-ctrl -P 1 -p 1          // Power on at port 1
*   # hub-ctrl -P 2 -l            // LED on at port 2
*
* Requirement: USB hub which implements port power control / indicator control
*
*      Work fine:
*         Elecom's U2H-G4S: www.elecom.co.jp (indicator depends on power)
*         04b4:6560
*
*	   Sanwa Supply's USB-HUB14GPH: www.sanwa.co.jp (indicators don't)
*
*	   Targus, Inc.'s PAUH212: www.targus.com (indicators don't)
*         04cc:1521
*
*	   Hawking Technology's UH214: hawkingtech.com (indicators don't)
*
*/

int
main (int argc, const char *argv[])
{
	int busnum = 0, devnum = 0;
	int cmd = COMMAND_SET_NONE;
	int port = 1;
	int value = 0;
	int request, feature, index;
	int result = 0;
	int listing = 0;
	int verbose = 0;
	int hub = -1;
	libusb_device_handle *uh = NULL;
	int i;

	if (argc == 1)
		listing = 1;

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1])
		{
			case 'h':
				if (++i >= argc || busnum > 0 || devnum > 0)
					exit_with_usage (argv[0]);
				hub = atoi (argv[i]);
				break;

			case 'b':
				if (++i >= argc || hub >= 0)
					exit_with_usage (argv[0]);
				busnum = atoi (argv[i]);
				break;

			case 'd':
				if (++i >= argc || hub >= 0)
					exit_with_usage (argv[0]);
				devnum = atoi (argv[i]);
				break;

			case 'P':
				if (++i >= argc)
					exit_with_usage (argv[0]);
				port = atoi (argv[i]);
				break;

			case 'l':
				if (cmd != COMMAND_SET_NONE)
					exit_with_usage (argv[0]);
				if (++i < argc)
					value = atoi (argv[i]);
				else
					value = HUB_LED_GREEN;
				cmd = COMMAND_SET_LED;
				break;

			case 'p':
				if (cmd != COMMAND_SET_NONE)
					exit_with_usage (argv[0]);
				if (++i < argc)
					value = atoi (argv[i]);
				else
					value= 0;
				cmd = COMMAND_SET_POWER;
				break;

			case 'v':
				verbose = 1;
				if (argc == 2)
					listing = 1;
				break;

			default:
				exit_with_usage (argv[0]);
		}
		else
			exit_with_usage (argv[0]);

	if ((busnum > 0 && devnum <= 0) || (busnum <= 0 && devnum > 0))
		/* BUS is specified, but DEV is'nt, or ... */
		exit_with_usage (argv[0]);

	/* Default is the hub #0 */
	if (hub < 0 && busnum == 0)
		hub = 0;

	/* Default is POWER */
	if (cmd == COMMAND_SET_NONE)
		cmd = COMMAND_SET_POWER;

	libusb_init ( &ctx );
	libusb_set_debug( ctx, 3 );

	if (usb_find_hubs (listing, verbose, busnum, devnum, hub) <= 0)
	{
		fprintf (stderr, "No hubs found.\n");
		libusb_exit( ctx );
		exit (1);
	}

	if (listing){
		libusb_exit( ctx );
		exit (0);
	}

	if (hub < 0)
		hub = get_hub (busnum, devnum);

	if (hub >= 0 && hub < number_of_hubs_with_feature){
		if ( libusb_open (hubs[hub].dev, &uh ) || uh == NULL)
		{
			fprintf (stderr, "Device not found.\n");
			result = 1;
		}
		else
		{
			if (cmd == COMMAND_SET_POWER){
				if (value){
					request = LIBUSB_REQUEST_SET_FEATURE;
					feature = USB_PORT_FEAT_POWER;
					index = port;
				}else{
					request = LIBUSB_REQUEST_CLEAR_FEATURE;
					feature = USB_PORT_FEAT_POWER;
					index = port;
				}
			}else{
				request = LIBUSB_REQUEST_SET_FEATURE;
				feature = USB_PORT_FEAT_INDICATOR;
				index = (value << 8) | port;
			}

			if (verbose)
				printf ("Send control message (REQUEST=%d, FEATURE=%d, INDEX=%d)\n",
				request, feature, index);
/*
			if(libusb_detach_kernel_driver( uh, 0 ))
				perror("libusb_detach_kernel_driver");
			if(libusb_claim_interface( uh, 0 ))
				perror("libusb_claim_interface");
*/
			if (libusb_control_transfer (uh, USB_RT_PORT, request, feature, index,
				NULL, 0, CTRL_TIMEOUT) < 0)
			{
				perror ("failed to control.\n");
				result = 1;
			}

			if (verbose)
				hub_port_status (uh, hubs[hub].nport, hubs[hub].usb3);
/*
			libusb_release_interface( uh,0 );
			libusb_attach_kernel_driver( uh, 0); 
*/

			libusb_close (uh);
		}
	}
	libusb_exit( ctx );
	exit (result);
}

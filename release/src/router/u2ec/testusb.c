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
#include <ctype.h>
#include <string.h>
#include <usb.h>
#include "typeconvert.h"
#include "usbsock.h"

#define USBLP_FIRST_PROTOCOL	1
#define USBLP_LAST_PROTOCOL	3

struct usb_bus *bus;
struct usb_device *dev;
char str_product[128];
char str_vidpid[128];
char str_mfg[128];
int verbose = 1;
char *EndpointType[4] = { "Control", "Isochronous", "Bulk", "Interrupt" };

int Is_HP_LaserJet = 0;
int Is_HP_OfficeJet = 0;
int Is_HP_LaserJet_MFP = 0;

int Is_Canon = 0;
int Is_EPSON = 0;
int Is_HP = 0;
int Is_Lexmark = 0;
int Is_General = 0;
int USB_Ver_N20 = 0;

extern int flag_canon_new_printer_cmd;

void print_endpoint(struct usb_endpoint_descriptor *endpoint)
{
	PDEBUG("      bLength:          %d\n", endpoint->bLength);
	PDEBUG("      bDescriptorType:  %d\n", endpoint->bDescriptorType);
	PDEBUG("      bEndpointAddress: %02xh (%s)\n", endpoint->bEndpointAddress, (endpoint->bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL ? "i/o" : (endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) ? "in" : "out");
	PDEBUG("      bmAttributes:     %02xh (%s)\n", endpoint->bmAttributes, EndpointType[USB_ENDPOINT_TYPE_MASK & endpoint->bmAttributes]);
	PDEBUG("      wMaxPacketSize:   %d\n", endpoint->wMaxPacketSize);
	PDEBUG("      bInterval:        %d\n", endpoint->bInterval);
	PDEBUG("      bRefresh:         %d\n", endpoint->bRefresh);
	PDEBUG("      bSynchAddress:    %d\n", endpoint->bSynchAddress);
/*
	usb_dev_handle *udev = usb_open(dev);
	if (udev)
	{
		usb_clear_halt(udev, endpoint->bEndpointAddress);
		usb_resetep(udev, endpoint->bEndpointAddress);
		usb_close(udev);
	}
*/
}

void print_altsetting(struct usb_interface_descriptor *interface)
{
	int i;

	PDEBUG("    bLength:            %d\n", interface->bLength);
	PDEBUG("    bDescriptorType:    %d\n", interface->bDescriptorType);
	PDEBUG("    bInterfaceNumber:   %d\n", interface->bInterfaceNumber);
	PDEBUG("    bAlternateSetting:  %d\n", interface->bAlternateSetting);
	PDEBUG("    bNumEndpoints:      %d\n", interface->bNumEndpoints);
	PDEBUG("    bInterface Class:SubClass:Protocol: %02x:%02x:%02x\n", interface->bInterfaceClass, interface->bInterfaceSubClass, interface->bInterfaceProtocol);
//	PDEBUG("    bInterfaceClass:    %d\n", interface->bInterfaceClass);
//	PDEBUG("    bInterfaceSubClass: %d\n", interface->bInterfaceSubClass);
//	PDEBUG("    bInterfaceProtocol: %d\n", interface->bInterfaceProtocol);
	PDEBUG("    iInterface:         %d\n", interface->iInterface);

	for (i = 0; i < interface->bNumEndpoints; i++)
	{
		PDEBUG("%.*s    Endpoint %d:\n", 0 * 2, "                    ", i);
		print_endpoint(&interface->endpoint[i]);
	}
}

void print_interface(struct usb_interface *interface)
{
	int i;

	for (i = 0; i < interface->num_altsetting; i++)
	{
		PDEBUG("%.*s  Alternate Setting %d:\n", 0 * 2, "                    ", i);
		print_altsetting(&interface->altsetting[i]);
	}
}

void print_configuration(struct usb_config_descriptor *config)
{
	int i;
	PDEBUG("    bLength:              %d\n", config->bLength);
	PDEBUG("    bDescriptorType:      %d\n", config->bDescriptorType);
	PDEBUG("    wTotalLength:         %d\n", config->wTotalLength);
	PDEBUG("    bNumInterfaces:       %d\n", config->bNumInterfaces);
	PDEBUG("    bConfigurationValue:  %d\n", config->bConfigurationValue);
	PDEBUG("    iConfiguration:       %d\n", config->iConfiguration);
	PDEBUG("    bmAttributes:         %02xh\n", config->bmAttributes);
	PDEBUG("    MaxPower:             %dmA\n", config->MaxPower*2);

	for (i = 0; i < config->bNumInterfaces; i++)
	{
		PDEBUG("%.*s  Interface %d:\n", 0 * 2, "                    ", i);
		print_interface(&config->interface[i]);
	}
}

int print_device(struct usb_device *dev, int level)
{
	usb_dev_handle *udev;
	char description[256];
	char string[256];
	int ret, i, j, k, l;
	struct usb_endpoint_descriptor *epd, *epwrite, *epread;
	int flag_found_printer = 0;

	for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
	{
		for (j = 0; j < dev->config[i].bNumInterfaces; j++)
		{
			for (k = 0; k < dev->config[i].interface[j].num_altsetting; k++)
			{
				if (dev->config[i].interface[j].altsetting[k].bInterfaceClass != 7 ||
				    dev->config[i].interface[j].altsetting[k].bInterfaceSubClass != 1)
					continue;
				if (dev->config[i].interface[j].altsetting[k].bInterfaceProtocol < USBLP_FIRST_PROTOCOL ||
		    		    dev->config[i].interface[j].altsetting[k].bInterfaceProtocol > USBLP_LAST_PROTOCOL)
					continue;
				epwrite = epread = 0;
				for (l = 0; l < dev->config[i].interface[j].altsetting[k].bNumEndpoints; l++)
				{
					epd = &dev->config[i].interface[j].altsetting[k].endpoint[l];

					if ((epd->bmAttributes&USB_ENDPOINT_TYPE_MASK) != USB_ENDPOINT_TYPE_BULK)
						continue;

					if (!(epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK))
					{
						if (!epwrite)
							epwrite = epd;

					} 
					else
					{
						if (!epread)
							epread = epd;
					}
				}
				/* Ignore buggy hardware without the right endpoints. */
				if (!epwrite || (dev->config[i].interface[j].altsetting[k].bInterfaceProtocol > 1 && !epread))
					continue;
				else
				{
					flag_found_printer = 1;
					PDEBUG("[Found a supported printer]\n");
					break;
				}
			}
		}
	}

	if (!flag_found_printer)
	{
//		PDEBUG("[It is not a supported device]\n");
		return 1;
	}

	udev = usb_open(dev);
	if (udev) 
	{
		if (dev->descriptor.iManufacturer) 
		{
			ret = usb_get_string_simple(udev, dev->descriptor.iManufacturer, string, sizeof(string));
      			if (ret > 0)
			{
				snprintf(description, sizeof(description), "%s - ", string);
				strcpy(str_mfg, string);
				nvram_set("u2ec_mfg", str_mfg);
			}
			else
			{
				snprintf(description, sizeof(description), "%04X - ", dev->descriptor.idVendor);
				sprintf(str_mfg, "USB Vendor %04X", dev->descriptor.idVendor);
				nvram_set("u2ec_mfg", str_mfg);
			}
    		}
    		else
		{
      			snprintf(description, sizeof(description), "%04X - ", dev->descriptor.idVendor);
			sprintf(str_mfg, "USB Vendor %04X", dev->descriptor.idVendor);
			nvram_set("u2ec_mfg", str_mfg);
		}

    		if (dev->descriptor.iProduct) 
    		{
      			ret = usb_get_string_simple(udev, dev->descriptor.iProduct, string, sizeof(string));
      			if (ret > 0)
      			{
				snprintf(description + strlen(description), sizeof(description) - strlen(description), "%s", string);
				strcpy(str_product, string);
				nvram_set("u2ec_device", str_product);
			}
			else
			{
				snprintf(description + strlen(description), sizeof(description) - strlen(description), "%04X", dev->descriptor.idProduct);
				sprintf(str_product, "USB Device %04x:%04x", dev->descriptor.idVendor, dev->descriptor.idProduct);
				nvram_set("u2ec_device", str_product);
			}
		}
		else
		{
      			snprintf(description + strlen(description), sizeof(description) - strlen(description), "%04X", dev->descriptor.idProduct);
      			sprintf(str_product, "USB Device %04x:%04x", dev->descriptor.idVendor, dev->descriptor.idProduct);
      			nvram_set("u2ec_device", str_product);
      		}

	} 
	else
		snprintf(description, sizeof(description), "%04X - %04X", dev->descriptor.idVendor, dev->descriptor.idProduct);

	PDEBUG("%.*sDev #%d: %s\n", level * 2, "                    ", dev->devnum, description);

	if (udev)
	{
		if (dev->descriptor.iSerialNumber)
		{
			ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber, string, sizeof(string));
			if (ret > 0)
			{
				PDEBUG("%.*s  - Serial Number    : %s\n", level * 2, "                    ", string);
				nvram_set("u2ec_serial", string);
			}
			else
				nvram_set("u2ec_serial", "");
    		}

    		usb_close(udev);
	}

	sprintf(str_vidpid, "%04x%04x", dev->descriptor.idVendor, dev->descriptor.idProduct);
	nvram_set("u2ec_vidpid", str_vidpid);

	PDEBUG("%.*s  - Length	   : %2d%s\n", level * 2, "                    ", dev->descriptor.bLength, dev->descriptor.bLength == USB_DT_DEVICE_SIZE ? "" : " (!!!)");
	PDEBUG("%.*s  - DescriptorType   : %02x\n", level * 2, "                    ", dev->descriptor.bDescriptorType);
	PDEBUG("%.*s  - USB version      : %x.%02x\n", level * 2, "                    ", dev->descriptor.bcdUSB >> 8, dev->descriptor.bcdUSB & 0xff);
	if (( dev->descriptor.bcdUSB >> 8) < 2){
		USB_Ver_N20 = 1;
	//	PDEBUG("\n\t USB_Ver_N20 = %d\n", USB_Ver_N20);
	}
	else
		USB_Ver_N20 = 0;
	PDEBUG("%.*s  - Vendor:Product   : %04x:%04x\n", level * 2, "                    ", dev->descriptor.idVendor, dev->descriptor.idProduct);
	PDEBUG("%.*s  - MaxPacketSize0   : %d\n", level * 2, "                    ", dev->descriptor.bMaxPacketSize0);
//	PDEBUG("%.*s  - NumConfigurations: %d\n", level * 2, "                    ", dev->descriptor.bNumConfigurations);
	PDEBUG("%.*s  - Device version   : %x.%02x\n", level * 2, "                    ", dev->descriptor.bcdDevice >> 8, dev->descriptor.bcdDevice & 0xff);
	PDEBUG("%.*s  - Device Class:SubClass:Protocol: %02x:%02x:%02x\n", level * 2, "                    ", dev->descriptor.bDeviceClass, dev->descriptor.bDeviceSubClass, dev->descriptor.bDeviceProtocol);

	switch (dev->descriptor.bDeviceClass)
	{
		case 0:
			PDEBUG("%.*s    Per-interface classes\n", level * 2, "                    ");
			break;
		case USB_CLASS_AUDIO:
			PDEBUG("%.*s    Audio device class\n", level * 2, "                    ");
			break;
		case USB_CLASS_COMM:
			PDEBUG("%.*s    Communications class\n", level * 2, "                    ");
			break;
		case USB_CLASS_HID:
			PDEBUG("%.*s    Human Interface Devices class\n", level * 2, "                    ");
			break;
		case USB_CLASS_PRINTER:
			PDEBUG("%.*s    Printer device class\n", level * 2, "                    ");
			break;
		case USB_CLASS_MASS_STORAGE:
			PDEBUG("%.*s    Mass Storage device class\n", level * 2, "                    ");
			break;
		case USB_CLASS_HUB:
			PDEBUG("%.*s    Hub device class\n", level * 2, "                    ");
			break;
		case USB_CLASS_VENDOR_SPEC:
			PDEBUG("%.*s    Vendor class\n", level * 2, "                    ");
			break;
		default:
			PDEBUG("%.*s    Unknown class\n", level * 2, "                    ");
	}

	if (!dev->config)
	{
		PDEBUG("  Couldn't retrieve descriptors\n");
		return 1;
	}

	PDEBUG("%.*s  - Configuration    :\n", level * 2, "                    ");
	for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
		print_configuration(&dev->config[i]);

	return 0;
}

void teststr_product()
{
	char product[128];
	int i;

	Is_HP_LaserJet = 0;
	Is_HP_OfficeJet = 0;
	Is_HP_LaserJet_MFP = 0;

	strcpy(product, str_product);
	for (i=0; i<strlen(product); i++)
		product[i] = toupper(product[i]);

	if (strstr(product, "LASERJET") && strstr(product, "MFP"))
		Is_HP_LaserJet_MFP = 1;
	else if (strstr(product, "LASERJET"))
		Is_HP_LaserJet = 1;
	else if (strstr(product, "OFFICEJET"))
		Is_HP_OfficeJet = 1;
}

void testvendor()
{
	Is_Canon = 0;
	Is_EPSON = 0;
	Is_HP = 0;
	Is_Lexmark = 0;
	Is_General = 0;

	if (dev->descriptor.idVendor == 0x04a9)		// Canon
	{
		Is_Canon = 1;
		flag_canon_new_printer_cmd = 0;
	}
	else if (dev->descriptor.idVendor == 0x04b8)	// EPSON
		Is_EPSON = 1;
	else if (dev->descriptor.idVendor == 0x03f0)	// HP
		Is_HP = 1;	
	else if (dev->descriptor.idVendor == 0x043d)	// Lexmark
		Is_Lexmark = 1;
	else						// for general printers
		Is_General = 1;	
}


int testusb()
{
	int retry = 2;

RETRY:
	usb_init();

	usb_find_busses();
	usb_find_devices();

	strcpy(str_product, "Unknown device");

	for (bus = usb_busses; bus; bus = bus->next)
	{
		for (dev = bus->devices; dev; dev = dev->next)
		{
			if (!print_device(dev, 0))
			{
				teststr_product();
				testvendor();
				return 0;
			}
		}
	}

	if (retry)
	{
		retry--;
		goto RETRY;
	}

	dev = NULL;
	nvram_set("u2ec_device", "");
	nvram_set("u2ec_serial", "");
	nvram_set("u2ec_vidpid", "");
	nvram_set("u2ec_mfg", "");
	return 1;
}

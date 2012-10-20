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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "typeconvert.h"
#include "usbsock.h"
#include "wdm-MJMN.h"
#include "usbdi.h"
#include "usb.h"
#include "decode.h"
#include "urb64.h"

#define dbg(fmt, args...) fprintf(stderr, fmt, ## args)
#define dbG(fmt, args...) dbg("" fmt , __FUNCTION__ , ## args)

char *MajorFunctionString (PIRP_SAVE irp);
char *MinorFunctionString (PIRP_SAVE pirp);

#if defined(U2EC_ONPC)
#	define MAX_PACKET_SIZE MAX_BUF_LEN
#else
#	define MAX_PACKET_SIZE 0
#	undef  MAX_BUFFER_SIZE
#	define MAX_BUFFER_SIZE 0
#endif

unsigned char packets[MAX_PACKET_SIZE];
int plen;
char pstr[MAX_BUFFER_SIZE];
char pstr_ascii[MAX_BUFFER_SIZE];
//char pktname[1024];

#define IS_IP 	(packets[U2EC_L2_TYPE]==U2EC_L2_TYPE_IP)
#define IS_TCP 	(packets[U2EC_L3_TYPE]==U2EC_L3_TYPE_TCP)
//#define IS_U2EC_IRP_CLIENT	(packets[U2EC_L3_DST_PORT]==0x0d&&(packets[U2EC_L3_DST_PORT+1]==0xa2||packets[U2EC_L3_DST_PORT+1]==0x42)) 
//#define IS_U2EC_IRP_SERVER	(packets[U2EC_L3_SRC_PORT]==0x0d&&(packets[U2EC_L3_SRC_PORT+1]==0xa2||packets[U2EC_L3_SRC_PORT+1]==0x42))
#define IS_U2EC_IRP_CLIENT	(packets[U2EC_L3_SRC_PORT]==0xcc && packets[U2EC_L3_DST_PORT]==0x0d)
#define IS_U2EC_IRP_SERVER	(packets[U2EC_L3_SRC_PORT]==0x0d && packets[U2EC_L3_DST_PORT]==0xcc)
#define IS_U2EC_IRP		(IS_U2EC_IRP_CLIENT||IS_U2EC_IRP_SERVER)
#define IS_U2EC_IRP_CONTENT	(plen >= 158)

static unsigned int irp_g = 0;

void decodeBuf(char *sp, char *buf, int len)
{
	int i;

	printf("%sBuffer[%x]:", sp, len);
#if 0
	for (i=0;i<len;i++)
#else
	for(i=0;i<len&&i<1460;i++)
#endif
	{
		if (i%8==0) printf("\n%s%02x", sp, (unsigned char)buf[i]);
		else printf(" %02x", (unsigned char)buf[i]);
	}
	printf("\n");
}

void inline ppktbuf(char *buf, int size)
{
	if (size <= 0)
		return;
	int pos;
	for (pos=0; pos<size; pos++) {
		if (pos%64 == 0) PDEBUG("\t  ");
		PDEBUG("%c", (isprint((char)*(buf + pos))) ? (char)*(buf + pos) : '.');
		if (pos%64 == 63) PDEBUG("\n");
	}
	PDEBUG("\n");
	for (pos=0; pos<size; pos++) {
		if (pos%16 == 0) PDEBUG("\t");
		if (pos%8 == 0) PDEBUG("  ");
		PDEBUG("%2.2x ", (unsigned char)*(buf + pos));
		if (pos%16 == 15) PDEBUG("\n");
	}
	PDEBUG("\n\n");
}

void inline print_urb(PURB purb)
{
	int count;
	int i;
	unsigned char *ptr, *urb_ptr;
	USBD_INTERFACE_INFORMATION *itf;

	PDEBUG("\tURB:\n");
	PDEBUG("\t  length:	0x%x \n",		purb->UrbHeader.Length);
	PDEBUG("\t  function:	0x%x \n",		purb->UrbHeader.Function);
	PDEBUG("\t  status:	0x%x \n",		purb->UrbHeader.Status);
	PDEBUG("\t  usbdflags:	0x%x \n",		purb->UrbHeader.UsbdFlags);

	switch (purb->UrbHeader.Function){
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		PDEBUG("\t  URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE\n");
		PDEBUG("\t    UrbControlDescriptorRequest.TransferBufferLength: %x\n", purb->UrbControlDescriptorRequest.TransferBufferLength);
    		PDEBUG("\t    UrbControlDescriptorRequest.Index: %x\n", purb->UrbControlDescriptorRequest.Index);
    		PDEBUG("\t    UrbControlDescriptorRequest.DescriptorType: %x\n", purb->UrbControlDescriptorRequest.DescriptorType);
    		PDEBUG("\t    UrbControlDescriptorRequest.LanguageId: %x\n", purb->UrbControlDescriptorRequest.LanguageId);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved: %x\n", (int)purb->UrbControlDescriptorRequest.Reserved);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved0: %x\n", purb->UrbControlDescriptorRequest.Reserved0);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved1: %x\n", purb->UrbControlDescriptorRequest.Reserved1);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved2: %x\n", purb->UrbControlDescriptorRequest.Reserved2);
    		PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdEndpoint: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdEndpoint);
    		PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdIrp: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdIrp);
    		PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer);
    		PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdExtension: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdExtension);
		break; // end of URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_SELECT_CONFIGURATION:   
		PDEBUG("\t  URB_FUNCTION_SELECT_CONFIGURATION\n");	

#if 0		
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bLength: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor->bLength);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bDescriptorType: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bDescriptorType);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.wTotalLength: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.wTotalLength);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bNumInterfaces: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bNumInterfaces);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bConfigurationValue: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bConfigurationValue);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.iConfiguration: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.iConfiguration);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bmAttributes: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bmAttributes);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.MaxPower: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.MaxPower);
#else
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor: %x\n", (int)purb->UrbSelectConfiguration.ConfigurationDescriptor);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationHandle: %x\n", (int)purb->UrbSelectConfiguration.ConfigurationHandle);
#endif

		urb_ptr = (unsigned char*)&purb->UrbSelectConfiguration.ConfigurationDescriptor;
		ptr = (unsigned char *)&purb->UrbSelectConfiguration.Interface.Length;
		for (i=0; (ptr-urb_ptr+sizeof(purb->UrbHeader)) < purb->UrbHeader.Length;i++) {
			itf = (USBD_INTERFACE_INFORMATION *)ptr;
			ptr += itf->Length;
			PDEBUG("\t    UrbSelectConfiguration.Interface.Length: %x\n", itf->Length);
			PDEBUG("\t    UrbSelectConfiguration.Interface.InterfaceNumber: %x\n", itf->InterfaceNumber);
			PDEBUG("\t    UrbSelectConfiguration.Interface.AlternateSetting: %x\n", itf->AlternateSetting);
			PDEBUG("\t    UrbSelectConfiguration.Interface.Class: %x\n", itf->Class);
			PDEBUG("\t    UrbSelectConfiguration.Interface.SubClass: %x\n", itf->SubClass);
			PDEBUG("\t    UrbSelectConfiguration.Interface.Protocol: %x\n", itf->Protocol);
			PDEBUG("\t    UrbSelectConfiguration.Interface.Reserved: %x\n", itf->Reserved);
			PDEBUG("\t    UrbSelectConfiguration.Interface.InterfaceHandle: %x\n", (int)itf->InterfaceHandle);
			PDEBUG("\t    UrbSelectConfiguration.Interface.NumberOfPipes: %x\n", itf->NumberOfPipes);

			for (count=0;count<itf->NumberOfPipes;count++)
			{
				PDEBUG("\t  Pipe : %x\n", count);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].MaximumPacketSize: %x\n", count, itf->Pipes[count].MaximumPacketSize);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].EndpointAddress: %x\n", count, itf->Pipes[count].EndpointAddress);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].Interval: %x\n", count, itf->Pipes[count].Interval);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].PipeType: %x\n", count, itf->Pipes[count].PipeType);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].PipeHandle: %x\n", count, (int)itf->Pipes[count].PipeHandle);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].MaximumTransferSize: %x\n", count, itf->Pipes[count].MaximumTransferSize);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].PipeFlags: %x\n", count, itf->Pipes[count].PipeFlags);
			}
		}
		break; // end of URB_FUNCTION_SELECT_CONFIGURATION  

	case 0x2a:
		PDEBUG("\t    URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR(0x2a)\n");	
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Reserved: %x\n", (int)purb->UrbControlDescriptorRequest.Reserved);	
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Reserved0: %x\n", purb->UrbControlDescriptorRequest.Reserved0);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.TransferBufferLength: %x\n", purb->UrbControlDescriptorRequest.TransferBufferLength);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.TransferBuffer: %x\n", (int)purb->UrbControlDescriptorRequest.TransferBuffer);	
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.TransferBufferMDL: %x\n", (int)purb->UrbControlDescriptorRequest.TransferBufferMDL);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Recipient & Reserved1 & Reserved 2: %x\n", purb->UrbControlDescriptorRequest.Reserved1);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.InterfaceNumber: %x\n", purb->UrbControlDescriptorRequest.Index);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.MS_PageIdex: %x\n", purb->UrbControlDescriptorRequest.DescriptorType);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.MS_FeatureDescriptorIndex: %x\n", purb->UrbControlDescriptorRequest.LanguageId);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Reserved3: %x\n", purb->UrbControlDescriptorRequest.Reserved2);	
		break;

	case URB_FUNCTION_SELECT_INTERFACE:     // 0x0001
		PDEBUG("\t  URB_FUNCTION_SELECT_INTERFACE\n");	
		PDEBUG("\t    UrbSelectInterface.Interface.Length: %x\n", purb->UrbSelectInterface.Interface.Length);
		PDEBUG("\t    UrbSelectInterface.Interface.InterfaceNumber: %x\n", purb->UrbSelectInterface.Interface.InterfaceNumber);
		PDEBUG("\t    UrbSelectInterface.Interface.AlternateSetting: %x\n", purb->UrbSelectInterface.Interface.AlternateSetting);
		PDEBUG("\t    UrbSelectInterface.Interface.Class: %x\n", purb->UrbSelectInterface.Interface.Class);
		PDEBUG("\t    UrbSelectInterface.Interface.SubClass: %x\n", purb->UrbSelectInterface.Interface.SubClass);
		PDEBUG("\t    UrbSelectInterface.Interface.Protocol: %x\n", purb->UrbSelectInterface.Interface.Protocol);
		PDEBUG("\t    UrbSelectInterface.Interface.Reserved: %x\n", purb->UrbSelectInterface.Interface.Reserved);
		PDEBUG("\t    UrbSelectInterface.Interface.InterfaceHandle: %x\n", (int)purb->UrbSelectInterface.Interface.InterfaceHandle);
		PDEBUG("\t    UrbSelectInterface.Interface.NumberOfPipes: %x\n", purb->UrbSelectInterface.Interface.NumberOfPipes);

		for (count=0;count<purb->UrbSelectInterface.Interface.NumberOfPipes;count++)
		{
			PDEBUG("\t  Pipe : %x\n", count);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].PipeType: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].PipeType);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].PipeHandle: %x\n", count, (int)purb->UrbSelectInterface.Interface.Pipes[count].PipeHandle);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].MaximumTransferSize: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].MaximumTransferSize);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].PipeFlags: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].PipeFlags);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].MaximumPacketSize: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].MaximumPacketSize);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].EndpointAddress: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].EndpointAddress);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].Interval: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].Interval);
		}
		break; // URB_FUNCTION_SELECT_INTERFACE		0x0001

	case URB_FUNCTION_CONTROL_TRANSFER:     // 0x0008
		PDEBUG("\t  URB_FUNCTION_CONTROL_TRANSFER\n");	
		PDEBUG("\t    UrbControlTransfer.PipeHandle: %x\n", (int)purb->UrbControlTransfer.PipeHandle);
		PDEBUG("\t    UrbControlTransfer.TransferFlags: %x\n", purb->UrbControlTransfer.TransferFlags);
		PDEBUG("\t    UrbControlTransfer.TransferBufferLength: %x\n", purb->UrbControlTransfer.TransferBufferLength);
		PDEBUG("\t    UrbControlTransfer.TransferBuffer: %x\n", (int)purb->UrbControlTransfer.TransferBuffer);
		PDEBUG("\t    UrbControlTransfer.TransferBufferMDL: %x\n", (int)purb->UrbControlTransfer.TransferBufferMDL);	
		PDEBUG("\t    UrbControlTransfer.UrbLink: %x\n", (int)purb->UrbControlTransfer.UrbLink);
		PDEBUG("\t    UrbControlTransfer.SetupPacket:");


		for (count=0;count<8;count++)
		{
			PDEBUG("%02x", purb->UrbControlTransfer.SetupPacket[count]);
		}
		PDEBUG("\n");
		break;

	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:     // 0x0009
		PDEBUG("\t  URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER\n");	
		PDEBUG("\t    UrbBulkOrInterruptTransfer.PipeHandle: %x\n", (int)purb->UrbBulkOrInterruptTransfer.PipeHandle);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferFlags: %x\n", purb->UrbBulkOrInterruptTransfer.TransferFlags);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferBufferLength: %x\n", purb->UrbBulkOrInterruptTransfer.TransferBufferLength);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferBuffer: %x\n", (int)purb->UrbBulkOrInterruptTransfer.TransferBuffer);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferBufferMDL: %x\n", (int)purb->UrbBulkOrInterruptTransfer.TransferBufferMDL);	
		PDEBUG("\t    UrbBulkOrInterruptTransfer.UrbLink: %x\n", (int)purb->UrbBulkOrInterruptTransfer.UrbLink);
    		PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdEndpoint: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint);
    		PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdIrp: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdIrp);
    		PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer);
    		PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdExtension: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdExtension);
		break;

	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER:
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_DEVICE)
			PDEBUG("\t      URB_FUNCTION_VENDOR_DEVICE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_INTERFACE)
			PDEBUG("\t      URB_FUNCTION_VENDOR_INTERFACE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_ENDPOINT)
			PDEBUG("\t      URB_FUNCTION_VENDOR_ENDPOINT\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_OTHER)
			PDEBUG("\t      URB_FUNCTION_VENDOR_OTHER\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_DEVICE)
			PDEBUG("\t      URB_FUNCTION_CLASS_DEVICE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_INTERFACE)
			PDEBUG("\t      URB_FUNCTION_CLASS_INTERFACE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_ENDPOINT)
			PDEBUG("\t      URB_FUNCTION_CLASS_ENDPOINT\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_OTHER)
			PDEBUG("\t      URB_FUNCTION_CLASS_OTHER\n");	
		PDEBUG("\t    UrbControlVendorClassRequest.Reserved: %x\n", (int)purb->UrbControlVendorClassRequest.Reserved);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferFlags: %x\n", purb->UrbControlVendorClassRequest.TransferFlags);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferBufferLength: %x\n", purb->UrbControlVendorClassRequest.TransferBufferLength);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferBuffer: %x\n", (int)purb->UrbControlVendorClassRequest.TransferBuffer);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferBufferMDL: %x\n", (int)purb->UrbControlVendorClassRequest.TransferBufferMDL);
		PDEBUG("\t    UrbControlVendorClassRequest.UrbLink: %x\n", (int)purb->UrbControlVendorClassRequest.UrbLink);
		PDEBUG("\t    UrbControlVendorClassRequest.RequestTypeReservedBits: %x\n", purb->UrbControlVendorClassRequest.RequestTypeReservedBits);
		PDEBUG("\t    UrbControlVendorClassRequest.Request: %x\n", purb->UrbControlVendorClassRequest.Request);
		PDEBUG("\t    UrbControlVendorClassRequest.Value: %x\n", purb->UrbControlVendorClassRequest.Value);
		PDEBUG("\t    UrbControlVendorClassRequest.Index: %x\n", purb->UrbControlVendorClassRequest.Index);
		PDEBUG("\t    UrbControlVendorClassRequest.Reserved1: %x\n", purb->UrbControlVendorClassRequest.Reserved1);
		break;	

	default:
		PDEBUG("\t  Unknown URB(%x)\n", purb->UrbHeader.Function);
		break;
	}
}

void inline print_urb_64(PURB_64 purb)
{
	int count;
	int i;
	unsigned char *ptr, *urb_ptr;
	USBD_INTERFACE_INFORMATION_64 *itf;

	PDEBUG("\tURB_64:\n");
	PDEBUG("\t  length:	0x%x \n",		purb->UrbHeader.Length);
	PDEBUG("\t  function:	0x%x \n",		purb->UrbHeader.Function);
	PDEBUG("\t  status:	0x%x \n",		purb->UrbHeader.Status);
	PDEBUG("\t  usbdflags:	0x%x \n",		purb->UrbHeader.UsbdFlags);

	switch (purb->UrbHeader.Function){
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		PDEBUG("\t  URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE\n");
		PDEBUG("\t    UrbControlDescriptorRequest.TransferBufferLength: %x\n", purb->UrbControlDescriptorRequest.TransferBufferLength);
    		PDEBUG("\t    UrbControlDescriptorRequest.Index: %x\n", purb->UrbControlDescriptorRequest.Index);
    		PDEBUG("\t    UrbControlDescriptorRequest.DescriptorType: %x\n", purb->UrbControlDescriptorRequest.DescriptorType);
    		PDEBUG("\t    UrbControlDescriptorRequest.LanguageId: %x\n", purb->UrbControlDescriptorRequest.LanguageId);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved: %x\n", (int)purb->UrbControlDescriptorRequest.Reserved);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved0: %x\n", purb->UrbControlDescriptorRequest.Reserved0);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved1: %x\n", purb->UrbControlDescriptorRequest.Reserved1);
    		PDEBUG("\t    UrbControlDescriptorRequest.Reserved2: %x\n", purb->UrbControlDescriptorRequest.Reserved2);
    		//PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdEndpoint: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdEndpoint);
    		//PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdIrp: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdIrp);
    		//PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer);
    		//PDEBUG("\t    UrbControlDescriptorRequest.hca.HcdExtension: %x\n", (int)purb->UrbControlDescriptorRequest.hca.HcdExtension);
		break; // end of URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_SELECT_CONFIGURATION:   
		PDEBUG("\t  URB_FUNCTION_SELECT_CONFIGURATION\n");	

#if 0		
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bLength: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor->bLength);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bDescriptorType: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bDescriptorType);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.wTotalLength: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.wTotalLength);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bNumInterfaces: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bNumInterfaces);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bConfigurationValue: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bConfigurationValue);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.iConfiguration: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.iConfiguration);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.bmAttributes: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.bmAttributes);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor.MaxPower: %x\n", purb->UrbSelectConfiguration.ConfigurationDescriptor.MaxPower);
#else
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationDescriptor: %x\n", (int)purb->UrbSelectConfiguration.ConfigurationDescriptor);
		PDEBUG("\t    UrbSelectConfiguration.ConfigurationHandle: %x\n", (int)purb->UrbSelectConfiguration.ConfigurationHandle);
#endif

		urb_ptr = (unsigned char*)&purb->UrbSelectConfiguration.ConfigurationDescriptor;
		ptr = (unsigned char *)&purb->UrbSelectConfiguration.Interface.Length;
		for (i=0; (ptr-urb_ptr+sizeof(purb->UrbHeader)) < purb->UrbHeader.Length;i++) {
			itf = (USBD_INTERFACE_INFORMATION_64 *)ptr;
			ptr += itf->Length;
			PDEBUG("\t    UrbSelectConfiguration.Interface.Length: %x\n", itf->Length);
			PDEBUG("\t    UrbSelectConfiguration.Interface.InterfaceNumber: %x\n", itf->InterfaceNumber);
			PDEBUG("\t    UrbSelectConfiguration.Interface.AlternateSetting: %x\n", itf->AlternateSetting);
			PDEBUG("\t    UrbSelectConfiguration.Interface.Class: %x\n", itf->Class);
			PDEBUG("\t    UrbSelectConfiguration.Interface.SubClass: %x\n", itf->SubClass);
			PDEBUG("\t    UrbSelectConfiguration.Interface.Protocol: %x\n", itf->Protocol);
			PDEBUG("\t    UrbSelectConfiguration.Interface.Reserved: %x\n", itf->Reserved);
			PDEBUG("\t    UrbSelectConfiguration.Interface.InterfaceHandle: %x\n", (int)itf->InterfaceHandle);
			PDEBUG("\t    UrbSelectConfiguration.Interface.NumberOfPipes: %x\n", itf->NumberOfPipes);

			for (count=0;count<itf->NumberOfPipes;count++)
			{
				PDEBUG("\t  Pipe : %x\n", count);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].MaximumPacketSize: %x\n", count, itf->Pipes[count].MaximumPacketSize);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].EndpointAddress: %x\n", count, itf->Pipes[count].EndpointAddress);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].Interval: %x\n", count, itf->Pipes[count].Interval);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].PipeType: %x\n", count, itf->Pipes[count].PipeType);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].PipeHandle: %x\n", count, (int)itf->Pipes[count].PipeHandle);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].MaximumTransferSize: %x\n", count, itf->Pipes[count].MaximumTransferSize);
				PDEBUG("\t    UrbSelectConfiguration.Interface.Pipes[%x].PipeFlags: %x\n", count, itf->Pipes[count].PipeFlags);
			}
		}
		break; // end of URB_FUNCTION_SELECT_CONFIGURATION  

	case 0x2a:
		PDEBUG("\t    URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR(0x2a)\n");	
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Reserved: %x\n", (int)purb->UrbControlDescriptorRequest.Reserved);	
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Reserved0: %x\n", purb->UrbControlDescriptorRequest.Reserved0);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.TransferBufferLength: %x\n", purb->UrbControlDescriptorRequest.TransferBufferLength);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.TransferBuffer: %x\n", (int)purb->UrbControlDescriptorRequest.TransferBuffer);	
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.TransferBufferMDL: %x\n", (int)purb->UrbControlDescriptorRequest.TransferBufferMDL);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Recipient & Reserved1 & Reserved 2: %x\n", purb->UrbControlDescriptorRequest.Reserved1);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.InterfaceNumber: %x\n", purb->UrbControlDescriptorRequest.Index);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.MS_PageIdex: %x\n", purb->UrbControlDescriptorRequest.DescriptorType);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.MS_FeatureDescriptorIndex: %x\n", purb->UrbControlDescriptorRequest.LanguageId);
		PDEBUG("\t      UrbOsFeatureDescriptorRequest.Reserved3: %x\n", purb->UrbControlDescriptorRequest.Reserved2);	
		break;

	case URB_FUNCTION_SELECT_INTERFACE:     // 0x0001
		PDEBUG("\t  URB_FUNCTION_SELECT_INTERFACE\n");	
		PDEBUG("\t    UrbSelectInterface.Interface.Length: %x\n", purb->UrbSelectInterface.Interface.Length);
		PDEBUG("\t    UrbSelectInterface.Interface.InterfaceNumber: %x\n", purb->UrbSelectInterface.Interface.InterfaceNumber);
		PDEBUG("\t    UrbSelectInterface.Interface.AlternateSetting: %x\n", purb->UrbSelectInterface.Interface.AlternateSetting);
		PDEBUG("\t    UrbSelectInterface.Interface.Class: %x\n", purb->UrbSelectInterface.Interface.Class);
		PDEBUG("\t    UrbSelectInterface.Interface.SubClass: %x\n", purb->UrbSelectInterface.Interface.SubClass);
		PDEBUG("\t    UrbSelectInterface.Interface.Protocol: %x\n", purb->UrbSelectInterface.Interface.Protocol);
		PDEBUG("\t    UrbSelectInterface.Interface.Reserved: %x\n", purb->UrbSelectInterface.Interface.Reserved);
		PDEBUG("\t    UrbSelectInterface.Interface.InterfaceHandle: %x\n", (int)purb->UrbSelectInterface.Interface.InterfaceHandle);
		PDEBUG("\t    UrbSelectInterface.Interface.NumberOfPipes: %x\n", purb->UrbSelectInterface.Interface.NumberOfPipes);

		for (count=0;count<purb->UrbSelectInterface.Interface.NumberOfPipes;count++)
		{
			PDEBUG("\t  Pipe : %x\n", count);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].PipeType: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].PipeType);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].PipeHandle: %x\n", count, (int)purb->UrbSelectInterface.Interface.Pipes[count].PipeHandle);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].MaximumTransferSize: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].MaximumTransferSize);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].PipeFlags: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].PipeFlags);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].MaximumPacketSize: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].MaximumPacketSize);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].EndpointAddress: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].EndpointAddress);
			PDEBUG("\t    UrbSelectInterface.Interface.Pipes[%x].Interval: %x\n", count, purb->UrbSelectInterface.Interface.Pipes[count].Interval);
		}
		break; // URB_FUNCTION_SELECT_INTERFACE		0x0001

	case URB_FUNCTION_CONTROL_TRANSFER:     // 0x0008
		PDEBUG("\t  URB_FUNCTION_CONTROL_TRANSFER\n");	
		PDEBUG("\t    UrbControlTransfer.PipeHandle: %x\n", (int)purb->UrbControlTransfer.PipeHandle);
		PDEBUG("\t    UrbControlTransfer.TransferFlags: %x\n", purb->UrbControlTransfer.TransferFlags);
		PDEBUG("\t    UrbControlTransfer.TransferBufferLength: %x\n", purb->UrbControlTransfer.TransferBufferLength);
		PDEBUG("\t    UrbControlTransfer.TransferBuffer: %x\n", (int)purb->UrbControlTransfer.TransferBuffer);
		PDEBUG("\t    UrbControlTransfer.TransferBufferMDL: %x\n", (int)purb->UrbControlTransfer.TransferBufferMDL);	
		PDEBUG("\t    UrbControlTransfer.UrbLink: %x\n", (int)purb->UrbControlTransfer.UrbLink);
		PDEBUG("\t    UrbControlTransfer.SetupPacket:");


		for (count=0;count<8;count++)
		{
			PDEBUG("%02x", purb->UrbControlTransfer.SetupPacket[count]);
		}
		PDEBUG("\n");
		break;

	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:     // 0x0009
		PDEBUG("\t  URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER\n");	
		PDEBUG("\t    UrbBulkOrInterruptTransfer.PipeHandle: %x\n", (int)purb->UrbBulkOrInterruptTransfer.PipeHandle);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferFlags: %x\n", purb->UrbBulkOrInterruptTransfer.TransferFlags);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferBufferLength: %x\n", purb->UrbBulkOrInterruptTransfer.TransferBufferLength);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferBuffer: %x\n", (int)purb->UrbBulkOrInterruptTransfer.TransferBuffer);
		PDEBUG("\t    UrbBulkOrInterruptTransfer.TransferBufferMDL: %x\n", (int)purb->UrbBulkOrInterruptTransfer.TransferBufferMDL);	
		PDEBUG("\t    UrbBulkOrInterruptTransfer.UrbLink: %x\n", (int)purb->UrbBulkOrInterruptTransfer.UrbLink);
    		//PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdEndpoint: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint);
    		//PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdIrp: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdIrp);
    		//PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer);
    		//PDEBUG("\t    UrbBulkOrInterruptTransfer.hca.HcdExtension: %x\n", (int)purb->UrbBulkOrInterruptTransfer.hca.HcdExtension);
		break;

	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER:
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_DEVICE)
			PDEBUG("\t      URB_FUNCTION_VENDOR_DEVICE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_INTERFACE)
			PDEBUG("\t      URB_FUNCTION_VENDOR_INTERFACE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_ENDPOINT)
			PDEBUG("\t      URB_FUNCTION_VENDOR_ENDPOINT\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_OTHER)
			PDEBUG("\t      URB_FUNCTION_VENDOR_OTHER\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_DEVICE)
			PDEBUG("\t      URB_FUNCTION_CLASS_DEVICE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_INTERFACE)
			PDEBUG("\t      URB_FUNCTION_CLASS_INTERFACE\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_ENDPOINT)
			PDEBUG("\t      URB_FUNCTION_CLASS_ENDPOINT\n");	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_OTHER)
			PDEBUG("\t      URB_FUNCTION_CLASS_OTHER\n");	
		PDEBUG("\t    UrbControlVendorClassRequest.Reserved: %x\n", (int)purb->UrbControlVendorClassRequest.Reserved);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferFlags: %x\n", purb->UrbControlVendorClassRequest.TransferFlags);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferBufferLength: %x\n", purb->UrbControlVendorClassRequest.TransferBufferLength);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferBuffer: %x\n", (int)purb->UrbControlVendorClassRequest.TransferBuffer);
		PDEBUG("\t    UrbControlVendorClassRequest.TransferBufferMDL: %x\n", (int)purb->UrbControlVendorClassRequest.TransferBufferMDL);
		PDEBUG("\t    UrbControlVendorClassRequest.UrbLink: %x\n", (int)purb->UrbControlVendorClassRequest.UrbLink);
		PDEBUG("\t    UrbControlVendorClassRequest.RequestTypeReservedBits: %x\n", purb->UrbControlVendorClassRequest.RequestTypeReservedBits);
		PDEBUG("\t    UrbControlVendorClassRequest.Request: %x\n", purb->UrbControlVendorClassRequest.Request);
		PDEBUG("\t    UrbControlVendorClassRequest.Value: %x\n", purb->UrbControlVendorClassRequest.Value);
		PDEBUG("\t    UrbControlVendorClassRequest.Index: %x\n", purb->UrbControlVendorClassRequest.Index);
		PDEBUG("\t    UrbControlVendorClassRequest.Reserved1: %x\n", purb->UrbControlVendorClassRequest.Reserved1);
		break;	

	default:
		PDEBUG("\t  Unknown URB(%x)\n", purb->UrbHeader.Function);
		break;
	}
}



void decodeURB(PURB purb, char *sp)
{
	int count;
	int i;
	unsigned char *ptr, *urb_ptr;
	USBD_INTERFACE_INFORMATION *itf;

	printf("%sURB Len:%x\n", sp, purb->UrbHeader.Length);
	printf("%sURB Fun:%x\n", sp, purb->UrbHeader.Function);
	printf("%sURB Sts:%x\n", sp, purb->UrbHeader.Status);
	printf("%sURB DevHnd:%x\n", sp, (int)purb->UrbHeader.UsbdDeviceHandle);
	printf("%sURB Flg:%x\n", sp, purb->UrbHeader.UsbdFlags);
	
	switch (purb->UrbHeader.Function){
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		printf("\n%sURB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE\n", sp);
		printf("%sUrbControlDescriptorRequest.TransferBufferLength: %x\n", sp, purb->UrbControlDescriptorRequest.TransferBufferLength);
    		printf("%sUrbControlDescriptorRequest.Index: %x\n", sp, purb->UrbControlDescriptorRequest.Index);
    		printf("%sUrbControlDescriptorRequest.DescriptorType: %x\n", sp, purb->UrbControlDescriptorRequest.DescriptorType);
    		printf("%sUrbControlDescriptorRequest.LanguageId: %x\n", sp, purb->UrbControlDescriptorRequest.LanguageId);
    		printf("%sUrbControlDescriptorRequest.Reserved: %x\n", sp, (int)purb->UrbControlDescriptorRequest.Reserved);
    		printf("%sUrbControlDescriptorRequest.Reserved0: %x\n", sp, purb->UrbControlDescriptorRequest.Reserved0);
    		printf("%sUrbControlDescriptorRequest.Reserved1: %x\n", sp, purb->UrbControlDescriptorRequest.Reserved1);
    		printf("%sUrbControlDescriptorRequest.Reserved2: %x\n", sp, purb->UrbControlDescriptorRequest.Reserved2);
    		printf("%sUrbControlDescriptorRequest.hca.HcdEndpoint: %x\n", sp, (int)purb->UrbControlDescriptorRequest.hca.HcdEndpoint);
    		printf("%sUrbControlDescriptorRequest.hca.HcdIrp: %x\n", sp, (int)purb->UrbControlDescriptorRequest.hca.HcdIrp);
    		printf("%sUrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer: %x\n", sp, (int)purb->UrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer);
    		printf("%sUrbControlDescriptorRequest.hca.HcdExtension: %x\n", sp, (int)purb->UrbControlDescriptorRequest.hca.HcdExtension);
		break; // end of URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_SELECT_CONFIGURATION:   
		printf("\n%sURB_FUNCTION_SELECT_CONFIGURATION\n", sp);	

#if 0		
		printf("\n%sUrbSelectConfiguration.ConfigurationDescriptor.bLength: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor->bLength);
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor.bDescriptorType: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor.bDescriptorType);
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor.wTotalLength: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor.wTotalLength);
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor.bNumInterfaces: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor.bNumInterfaces);
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor.bConfigurationValue: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor.bConfigurationValue);
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor.iConfiguration: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor.iConfiguration);
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor.bmAttributes: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor.bmAttributes);
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor.MaxPower: %x\n", sp, purb->UrbSelectConfiguration.ConfigurationDescriptor.MaxPower);
#else
		printf("%sUrbSelectConfiguration.ConfigurationDescriptor: %x\n", sp, (int)purb->UrbSelectConfiguration.ConfigurationDescriptor);
		printf("%sUrbSelectConfiguration.ConfigurationHandle: %x\n", sp, (int)purb->UrbSelectConfiguration.ConfigurationHandle);
#endif

		if (!purb->UrbSelectConfiguration.ConfigurationDescriptor)
		{
			printf("NULL UrbSelectConfiguration.ConfigurationDescriptor\n");
			break;
		}

		urb_ptr = (unsigned char*)&purb->UrbSelectConfiguration.ConfigurationDescriptor;
		ptr = (unsigned char *)&purb->UrbSelectConfiguration.Interface.Length;
		for (i=0; (ptr-urb_ptr+sizeof(purb->UrbHeader)) < purb->UrbHeader.Length;i++) {
			itf = (USBD_INTERFACE_INFORMATION *)ptr;
			ptr += itf->Length;
			printf("\n%sUrbSelectConfiguration.Interface.Length: %x\n", sp, itf->Length);
			printf("%sUrbSelectConfiguration.Interface.InterfaceNumber: %x\n", sp, itf->InterfaceNumber);
			printf("%sUrbSelectConfiguration.Interface.AlternateSetting: %x\n", sp, itf->AlternateSetting);
			printf("%sUrbSelectConfiguration.Interface.Class: %x\n", sp, itf->Class);
			printf("%sUrbSelectConfiguration.Interface.SubClass: %x\n", sp, itf->SubClass);
			printf("%sUrbSelectConfiguration.Interface.Protocol: %x\n", sp, itf->Protocol);
			printf("%sUrbSelectConfiguration.Interface.Reserved: %x\n", sp, itf->Reserved);
			printf("%sUrbSelectConfiguration.Interface.InterfaceHandle: %x\n", sp, (int)itf->InterfaceHandle);
			printf("%sUrbSelectConfiguration.Interface.NumberOfPipes: %x\n", sp, itf->NumberOfPipes);

			for (count=0;count<itf->NumberOfPipes;count++)
			{
				printf("\n%sPipe : %x\n", sp, count);
				printf("%sUrbSelectConfiguration.Interface.Pipes[%x].MaximumPacketSize: %x\n", sp, count, itf->Pipes[count].MaximumPacketSize);
				printf("%sUrbSelectConfiguration.Interface.Pipes[%x].EndpointAddress: %x\n", sp, count, itf->Pipes[count].EndpointAddress);
				printf("%sUrbSelectConfiguration.Interface.Pipes[%x].Interval: %x\n", sp, count, itf->Pipes[count].Interval);
				printf("%sUrbSelectConfiguration.Interface.Pipes[%x].PipeType: %x\n", sp, count, itf->Pipes[count].PipeType);
				printf("%sUrbSelectConfiguration.Interface.Pipes[%x].PipeHandle: %x\n", sp, count, (int)itf->Pipes[count].PipeHandle);
				printf("%sUrbSelectConfiguration.Interface.Pipes[%x].MaximumTransferSize: %x\n", sp, count, itf->Pipes[count].MaximumTransferSize);
				printf("%sUrbSelectConfiguration.Interface.Pipes[%x].PipeFlags: %x\n", sp, count, itf->Pipes[count].PipeFlags);
			}
		}
		break; // end of URB_FUNCTION_SELECT_CONFIGURATION  

	case 0x2a:
		printf("\n%sURB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR(0x2a)\n", sp);	
		printf("\n%sUrbOsFeatureDescriptorRequest.Reserved: %x\n", sp, (int)purb->UrbControlDescriptorRequest.Reserved);	
		printf("\n%sUrbOsFeatureDescriptorRequest.Reserved0: %x\n", sp, purb->UrbControlDescriptorRequest.Reserved0);
		printf("\n%sUrbOsFeatureDescriptorRequest.TransferBufferLength: %x\n", sp, purb->UrbControlDescriptorRequest.TransferBufferLength);
		printf("\n%sUrbOsFeatureDescriptorRequest.TransferBuffer: %x\n", sp, (int)purb->UrbControlDescriptorRequest.TransferBuffer);	
		printf("\n%sUrbOsFeatureDescriptorRequest.TransferBufferMDL: %x\n", sp, (int)purb->UrbControlDescriptorRequest.TransferBufferMDL);
		printf("\n%sUrbOsFeatureDescriptorRequest.Recipient & Reserved1 & Reserved 2: %x\n", sp, purb->UrbControlDescriptorRequest.Reserved1);
		printf("\n%sUrbOsFeatureDescriptorRequest.InterfaceNumber: %x\n", sp, purb->UrbControlDescriptorRequest.Index);
		printf("\n%sUrbOsFeatureDescriptorRequest.MS_PageIdex: %x\n", sp, purb->UrbControlDescriptorRequest.DescriptorType);
		printf("\n%sUrbOsFeatureDescriptorRequest.MS_FeatureDescriptorIndex: %x\n", sp, purb->UrbControlDescriptorRequest.LanguageId);
		printf("\n%sUrbOsFeatureDescriptorRequest.Reserved3: %x\n", sp, purb->UrbControlDescriptorRequest.Reserved2);	
		break;

	case URB_FUNCTION_SELECT_INTERFACE:     // 0x0001
		printf("\n%sURB_FUNCTION_SELECT_INTERFACE\n", sp);	
		printf("\n%sUrbSelectInterface.Interface.Length: %x\n", sp, purb->UrbSelectInterface.Interface.Length);
		printf("%sUrbSelectInterface.Interface.InterfaceNumber: %x\n", sp, purb->UrbSelectInterface.Interface.InterfaceNumber);
		printf("%sUrbSelectInterface.Interface.AlternateSetting: %x\n", sp, purb->UrbSelectInterface.Interface.AlternateSetting);
		printf("%sUrbSelectInterface.Interface.Class: %x\n", sp, purb->UrbSelectInterface.Interface.Class);
		printf("%sUrbSelectInterface.Interface.SubClass: %x\n", sp, purb->UrbSelectInterface.Interface.SubClass);
		printf("%sUrbSelectInterface.Interface.Protocol: %x\n", sp, purb->UrbSelectInterface.Interface.Protocol);
		printf("%sUrbSelectInterface.Interface.Reserved: %x\n", sp, purb->UrbSelectInterface.Interface.Reserved);
		printf("%sUrbSelectInterface.Interface.InterfaceHandle: %x\n", sp, (int)purb->UrbSelectInterface.Interface.InterfaceHandle);
		printf("%sUrbSelectInterface.Interface.NumberOfPipes: %x\n", sp, purb->UrbSelectInterface.Interface.NumberOfPipes);

		for (count=0;count<purb->UrbSelectInterface.Interface.NumberOfPipes;count++)
		{
			printf("\n%sPipe : %x\n", sp, count);
			printf("%sUrbSelectInterface.Interface.Pipes[%x].PipeType: %x\n", sp, count, purb->UrbSelectInterface.Interface.Pipes[count].PipeType);
			printf("%sUrbSelectInterface.Interface.Pipes[%x].PipeHandle: %x\n", sp, count, (int)purb->UrbSelectInterface.Interface.Pipes[count].PipeHandle);
			printf("%sUrbSelectInterface.Interface.Pipes[%x].MaximumTransferSize: %x\n", sp, count, purb->UrbSelectInterface.Interface.Pipes[count].MaximumTransferSize);
			printf("%sUrbSelectInterface.Interface.Pipes[%x].PipeFlags: %x\n", sp, count, purb->UrbSelectInterface.Interface.Pipes[count].PipeFlags);
			printf("%sUrbSelectInterface.Interface.Pipes[%x].MaximumPacketSize: %x\n", sp, count, purb->UrbSelectInterface.Interface.Pipes[count].MaximumPacketSize);
			printf("%sUrbSelectInterface.Interface.Pipes[%x].EndpointAddress: %x\n", sp, count, purb->UrbSelectInterface.Interface.Pipes[count].EndpointAddress);
			printf("%sUrbSelectInterface.Interface.Pipes[%x].Interval: %x\n", sp, count, purb->UrbSelectInterface.Interface.Pipes[count].Interval);
		}
		break; // URB_FUNCTION_SELECT_INTERFACE		0x0001

	case URB_FUNCTION_CONTROL_TRANSFER:     // 0x0008
		printf("\n%sURB_FUNCTION_CONTROL_TRANSFER\n", sp);	
		printf("%sUrbControlTransfer.PipeHandle: %x\n", sp, (int)purb->UrbControlTransfer.PipeHandle);
		printf("%sUrbControlTransfer.TransferFlags: %x\n", sp, purb->UrbControlTransfer.TransferFlags);
		printf("%sUrbControlTransfer.TransferBufferLength: %x\n", sp, purb->UrbControlTransfer.TransferBufferLength);
		printf("%sUrbControlTransfer.TransferBuffer: %x\n", sp, (int)purb->UrbControlTransfer.TransferBuffer);
		printf("%sUrbControlTransfer.TransferBufferMDL: %x\n", sp, (int)purb->UrbControlTransfer.TransferBufferMDL);	
		printf("%sUrbControlTransfer.UrbLink: %x\n", sp, (int)purb->UrbControlTransfer.UrbLink);
		printf("%sUrbControlTransfer.SetupPacket:", sp);


		for (count=0;count<8;count++)
		{
			printf("%02x", purb->UrbControlTransfer.SetupPacket[count]);
		}
		printf("\n");
		break;

	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:     // 0x0009
		printf("\n%sURB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER\n", sp);	
		printf("%sUrbBulkOrInterruptTransfer.PipeHandle: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.PipeHandle);
		printf("%sUrbBulkOrInterruptTransfer.TransferFlags: %x\n", sp, purb->UrbBulkOrInterruptTransfer.TransferFlags);
		printf("%sUrbBulkOrInterruptTransfer.TransferBufferLength: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.TransferBufferLength);
		printf("%sUrbBulkOrInterruptTransfer.TransferBuffer: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.TransferBuffer);
		printf("%sUrbBulkOrInterruptTransfer.TransferBufferMDL: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.TransferBufferMDL);	
		printf("%sUrbBulkOrInterruptTransfer.UrbLink: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.UrbLink);
    		printf("%sUrbBulkOrInterruptTransfer.hca.HcdEndpoint: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint);
    		printf("%sUrbBulkOrInterruptTransfer.hca.HcdIrp: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.hca.HcdIrp);
    		printf("%sUrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer);
    		printf("%sUrbBulkOrInterruptTransfer.hca.HcdExtension: %x\n", sp, (int)purb->UrbBulkOrInterruptTransfer.hca.HcdExtension);
		break;

	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER:
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_DEVICE)
			printf("\n%sURB_FUNCTION_VENDOR_DEVICE\n", sp);	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_INTERFACE)
			printf("\n%sURB_FUNCTION_VENDOR_INTERFACE\n", sp);	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_ENDPOINT)
			printf("\n%sURB_FUNCTION_VENDOR_ENDPOINT\n", sp);	
		if (purb->UrbHeader.Function==URB_FUNCTION_VENDOR_OTHER)
			printf("\n%sURB_FUNCTION_VENDOR_OTHER\n", sp);	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_DEVICE)
			printf("\n%sURB_FUNCTION_CLASS_DEVICE\n", sp);	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_INTERFACE)
			printf("\n%sURB_FUNCTION_CLASS_INTERFACE\n", sp);	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_ENDPOINT)
			printf("\n%sURB_FUNCTION_CLASS_ENDPOINT\n", sp);	
		if (purb->UrbHeader.Function==URB_FUNCTION_CLASS_OTHER)
			printf("\n%sURB_FUNCTION_CLASS_OTHER\n", sp);	
		printf("%sUrbControlVendorClassRequest.Reserved: %x\n", sp, (int)purb->UrbControlVendorClassRequest.Reserved);
		printf("%sUrbControlVendorClassRequest.TransferFlags: %x\n", sp, purb->UrbControlVendorClassRequest.TransferFlags);
		printf("%sUrbControlVendorClassRequest.TransferBufferLength: %x\n", sp, purb->UrbControlVendorClassRequest.TransferBufferLength);
		printf("%sUrbControlVendorClassRequest.TransferBuffer: %x\n", sp, (int)purb->UrbControlVendorClassRequest.TransferBuffer);
		printf("%sUrbControlVendorClassRequest.TransferBufferMDL: %x\n", sp, (int)purb->UrbControlVendorClassRequest.TransferBufferMDL);
		printf("%sUrbControlVendorClassRequest.UrbLink: %x\n", sp, (int)purb->UrbControlVendorClassRequest.UrbLink);
		printf("%sUrbControlVendorClassRequest.RequestTypeReservedBits: %x\n", sp, purb->UrbControlVendorClassRequest.RequestTypeReservedBits);
		printf("%sUrbControlVendorClassRequest.Request: %x\n", sp, purb->UrbControlVendorClassRequest.Request);
		printf("%sUrbControlVendorClassRequest.Value: %x\n", sp, purb->UrbControlVendorClassRequest.Value);
		printf("%sUrbControlVendorClassRequest.Index: %x\n", sp, purb->UrbControlVendorClassRequest.Index);
		printf("%sUrbControlVendorClassRequest.Reserved1: %x\n", sp, purb->UrbControlVendorClassRequest.Reserved1);
		break;	

	default:
		printf("%sUnknown URB(%x)\n", sp, purb->UrbHeader.Function);
		break;
	} // end of switch urb function
}

void inline print_irp(PIRP_SAVE pirp_save, int flag)
{
	if (flag)
		PDEBUG("\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
	else
		PDEBUG("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	PDEBUG("usb_connection received IRP: irp number: %x\n", pirp_save->Irp);
	PDEBUG("\tIRP_SAVE:\n");
	PDEBUG("\t  Size:	\t%x\n",	pirp_save->Size);
	PDEBUG("\t  NeedSize:	%x\n",		pirp_save->NeedSize);
	PDEBUG("\t  Device:	%llx\n",	pirp_save->Device);
	PDEBUG("\t  res1:	\t%x\n",	pirp_save->Res1);
	PDEBUG("\t  irp number:	%x\n",		pirp_save->Irp);
	PDEBUG("\t  Status:	%x\n",		pirp_save->Status);
	PDEBUG("\t  Information:\t%llx\n",	pirp_save->Information);
	PDEBUG("\t  BufferSize:	%x\n",		pirp_save->BufferSize);
	PDEBUG("\t  Reserv:	%x\n",		pirp_save->Reserv);
	PDEBUG("\t  empty: 	%llx\n",	pirp_save->StackLocation.empty);
	PDEBUG("\t  major function:  %x\n",	pirp_save->StackLocation.MajorFunction);
	PDEBUG("\t  minor function:  %x\n",	pirp_save->StackLocation.MinorFunction);
	PDEBUG("\t  Argument1:	%llx\n",	pirp_save->StackLocation.Parameters.Others.Argument1);
	PDEBUG("\t  Argument2:	%llx\n",	pirp_save->StackLocation.Parameters.Others.Argument2);
	PDEBUG("\t  Argument3:	%llx\n",	pirp_save->StackLocation.Parameters.Others.Argument3);
	PDEBUG("\t  Argument4:	%llx\n",	pirp_save->StackLocation.Parameters.Others.Argument4);
	PDEBUG("\n");

	if (pirp_save->BufferSize) {
		PDEBUG("\tIRP_SAVE buffer:\n");
		if (pirp_save->StackLocation.MajorFunction==0x0f) {
			PDEBUG("\n");
			PDEBUG("\tURB buffer:\n");
			if (pirp_save->Is64 == 0){
				PURB purb = (PURB)pirp_save->Buffer;
				print_urb(purb);
				if (pirp_save->BufferSize > purb->UrbHeader.Length)
					ppktbuf((char*)pirp_save->Buffer + purb->UrbHeader.Length,
						pirp_save->BufferSize - purb->UrbHeader.Length);
			} else {
				PURB_64 purb_64 = (PURB_64)pirp_save->Buffer;
				print_urb_64(purb_64);
				if (pirp_save->BufferSize > purb_64->UrbHeader.Length)
					ppktbuf((char*)pirp_save->Buffer + purb_64->UrbHeader.Length,
						pirp_save->BufferSize - purb_64->UrbHeader.Length);
			}
		}
		else 
			ppktbuf((char*)pirp_save->Buffer, pirp_save->BufferSize);
	}
}

int decodeIRP(char *sp, char *sp2)
{
	PIRP_SAVE pirp;
	PURB purb;
	int i, j;

	pirp = (PIRP_SAVE)&packets[U2EC_L4_IRP_SAVE];

	if (!pirp->Size && !pirp->NeedSize)
	{
		return -1;
	}

	if (pirp->Size > MAX_PACKET_SIZE || pirp->NeedSize > MAX_PACKET_SIZE)
	{
		return -1;
	}

	if (!pirp->Irp && (!pirp->Size || !pirp->NeedSize))
	{
		return -1;
	}

	if (!pirp->Irp && (pirp->Size < pirp->NeedSize))
	{
		return -1;
	}

	if (pirp->Irp > irp_g + 15)
	{
		return -1;
	}

	printf("%s", sp2);

	printf("%sSize	:%x\n", sp, pirp->Size);
	printf("%sNeedSize:%x\n", sp, pirp->NeedSize);
	printf("%sDevice	:%llx\n", sp, pirp->Device);
	printf("%sIs64	:%x\n", sp, pirp->Is64);
	printf("%sIsIsoch	:%x\n", sp, pirp->IsIsoch);
	printf("%sRes1	:%x\n", sp, pirp->Res1);
	printf("%sIrp	:%x\n", sp, pirp->Irp);
	printf("%sStatus	:%x\n", sp, pirp->Status);
	printf("%sInfo	:%llx\n", sp, pirp->Information);
	printf("%sCancel	:%x\n\n", sp, pirp->Cancel);

	printf("%sIO empty:%llx\n", sp, pirp->StackLocation.empty);
	printf("%sIO Major:%s(%x)\n", sp, MajorFunctionString(pirp), pirp->StackLocation.MajorFunction);
	printf("%sIO Minor:%s(%x)\n", sp, MinorFunctionString(pirp), pirp->StackLocation.MinorFunction);
	printf("%sIO Argu1:%llx\n", sp, pirp->StackLocation.Parameters.Others.Argument1);
	printf("%sIO Argu2:%llx\n", sp, pirp->StackLocation.Parameters.Others.Argument2);
	printf("%sIO Argu3:%llx\n", sp, pirp->StackLocation.Parameters.Others.Argument3);
	printf("%sIO Argu4:%llx\n", sp, pirp->StackLocation.Parameters.Others.Argument4);

	printf("\n%sBuffSize:%x\n", sp, pirp->BufferSize);
	printf("%sReserv	:%x\n", sp, pirp->Reserv);

	irp_g = pirp->Irp;

	if (pirp->BufferSize)
	{
		if (pirp->StackLocation.MajorFunction==0x0f)
		{
			purb = (PURB)pirp->Buffer;
			decodeURB(purb, sp);
			if (pirp->BufferSize>purb->UrbHeader.Length)
				decodeBuf(sp, (char*)&pirp->Buffer[purb->UrbHeader.Length], pirp->BufferSize-purb->UrbHeader.Length);
		}
		else 
		{
			j=0;
			for (i=0;i<pirp->BufferSize&&i<512;i++)
			{
				if (i%8==0)
				{	
					if (i==0)
						printf("\n%sIRP:%02x", sp, (unsigned char)pirp->Buffer[i]);
					else printf("\n%s    %02x", sp, (unsigned char)pirp->Buffer[i]); 
				}
				else printf("%02x", (unsigned char)pirp->Buffer[i]);

				if (i%2==0)
					pstr_ascii[j++]=pirp->Buffer[i];
			}
			printf("\n%s    %s\n", sp, pstr_ascii);
		}
	}

	return 0;
}


void decodeU2EC()
{
	dbG("");

	if (IS_IP&&IS_TCP&&IS_U2EC_IRP&&IS_U2EC_IRP_CONTENT)
	{
		if (IS_U2EC_IRP_CLIENT)
		{
#if 0
			printf("\n\n--------------->\n");
			printf("\t%s\n", pktname);
			decodeIRP("");
#else
			decodeIRP("", "\n\n--------------->\n");
#endif
		}
		else if (IS_U2EC_IRP_SERVER)
		{
#if 0
			printf("\n\n\t\t<---------------\n");
			printf("\t\t%s\n", pktname);
			decodeIRP("\t\t\t");
#else
			decodeIRP("\t\t\t", "\n\n\t\t<---------------\n");
#endif
		}
	}	
}

int getpacket(FILE *fp, unsigned char *packets, int *size)
{
	char buffer[MAX_BUFFER_SIZE];
	char hex[10];
	int j;

	while (fgets(buffer, MAX_BUFFER_SIZE, fp))
	{
		if (strcmp(buffer, "\n")==0)
		{
		}
		else if (strstr(buffer, "pkt")) // start to capture
		{
//			strcpy(pktname, buffer);
			*size = 0;
		}
		else
		{
			for (j=0;j<8;j++)
			{
				strncpy(hex, buffer+j*6, 4);
				hex[5] = 0;
				packets[(*size)++] = strtol(hex, NULL, 16);
				if (*(buffer+j*6+5)=='}') 
				{
					return 1;
				}
			}
		}
	}
	return 0;
}

void decode(char *file)
{
	FILE *fp;

	printf("decode %s\n", file);

	fp = fopen(file, "r+");
	while (getpacket(fp, packets, &plen))
	{
		decodeU2EC();
	}
	fclose(fp);
}

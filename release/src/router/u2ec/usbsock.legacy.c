/*
 * U2EC usbsock source
 *
 * Copyright (C) 2008 ASUSTeK Corporation
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/param.h>
#include <asm/byteorder.h>

#ifdef	SUPPORT_LPRng
#include <semaphore_mfp.h>
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
#include <bcmnvram.h>
#else
#include "bcmnvram.h" //2009.03.03 Yau add to use lib nvram
#endif

#include "typeconvert.h"
#include "usbsock.h"
#include "wdm-MJMN.h"
#include "usbdi.h"
#include "usb.h"
#include "urb64.h"

void decode(char *file);
int testusb();
void inline ppktbuf(char *buf, int size);
void inline print_irp(PIRP_SAVE pirp_save, int flag);
void hotplug_print(const char *format);

#if defined(U2EC_DEBUG) && defined(U2EC_ONPC)
FILE	*fp;	// Log file pointer for debug
#endif

int 	count_bulk_write = 0;
int 	count_bulk_read = 0;
int	count_bulk_read_ret0_1 = 0;
int	count_bulk_read_ret0_2 = 0;
int 	count_bulk_read_epson = 0;
int 	count_int_epson = 0;
int 	flag_monitor_epson = 0;

char EPSON_BR_STR1_09[]={0x00, 0x00, 0x00, 0x09, 0x01, 0x00, 0x80, 0x00, 0x10};
char EPSON_BR_STR2_19[]={0x00, 0x00, 0x00, 0x13, 0x01, 0x00, 0x89, 0x00, 0x40, 0x45, 0x50, 0x53, 0x4f, 0x4e, 0x2d, 0x44, 0x41, 0x54, 0x41};
char EPSON_BR_STR3_19[]={0x00, 0x00, 0x00, 0x13, 0x01, 0x00, 0x89, 0x00, 0x02, 0x45, 0x50, 0x53, 0x4f, 0x4e, 0x2d, 0x43, 0x54, 0x52, 0x4c};

#define USBLP_REQ_GET_STATUS	0x01	/* from src/linux/linux/driver/usb/printer.c */
#define LP_POUTPA		0x20	/* from src/linux/linux/include/linux/lp.h, unchanged input, active high */
#define LP_PSELECD		0x10	/* from src/linux/linux/include/linux/lp.h, unchanged input, active high */
#define LP_PERRORP		0x08	/* from src/linux/linux/include/linux/lp.h, unchanged input, active low */

#if defined(U2EC_DEBUG) 
//static char *usblp_messages[] = { "ok", "out of paper", "off-line", "on fire" };
static char *usblp_messages_canon[] = { "ok", "op call", "ink low?" };
#endif

int	count_err=0;
int	timeout_control_msg, timeout_bulk_read_msg, timeout_bulk_write_msg;
char	data_buf[MAX_BUF_LEN];
char	data_bufrw[MAX_BUF_LEN];

char	data_canon[MAX_BUF_LEN];
int 	flag_canon_state = 0;
int 	flag_canon_ok = 0;
int	data_canon_done = 0;
int 	flag_canon_monitor = 0;
int	flag_canon_new_printer_cmd = 0;

extern char str_product[128];
extern int Is_HP_LaserJet;
extern int Is_HP_OfficeJet;
extern int Is_HP_LaserJet_MFP;
extern int Is_Canon;
extern int Is_EPSON;
extern int Is_HP;
extern int Is_Lexmark;
extern int Is_General;
extern int USB_Ver_N20;
extern struct usb_bus		*bus;
extern struct usb_device	*dev;
usb_dev_handle			*udev;
struct timeval			tv_ref, tv_now;
PCONNECTION_INFO		last_busy_conn;

U2EC_LIST_HEAD(conn_info_list);
struct in_addr ip_monopoly;
time_t time_monopoly;
time_t time_monopoly_old;
time_t time_add_device;
time_t time_remove_device;

char	fd_in_use[32768];

/*
 * Handle urbs in irp_saverw.Buffer.
 */
int handleURB(PIRP_SAVE pirp_saverw, struct u2ec_list_head *curt_pos)
{
	PURB		purb;
	unsigned char*	pbuf;
	USHORT		UrbLength;
	int		size, i, j;
	int		ret = 0;
	int		length = 0;
	int		tmpL = 0;
	int		tmpReq = 0;
	int		tmpReqType = 0;

	u_int32_t tmp_config;
	u_int32_t tmp_int;
	u_int32_t tmp_alt;
	u_int32_t tmp_ep;
	u_int32_t tmp_intclass;

	int timeout_bulk_read_msg_tmp;
	int timeout_bulk_write_msg_tmp;
//	unsigned char status;
	int	err_canon;

	purb = (PURB)pirp_saverw->Buffer;
	UrbLength = purb->UrbHeader.Length;

	if (Is_HP && purb->UrbHeader.Function != URB_FUNCTION_CLASS_INTERFACE)
		((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
	else if (	Is_Canon &&
			purb->UrbHeader.Function != URB_FUNCTION_CLASS_INTERFACE &&
			purb->UrbHeader.Function != URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER
	)
		((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;

	// action differs to functions
	switch (purb->UrbHeader.Function) {
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		size = pirp_saverw->BufferSize - (u_int32_t)UrbLength;
		pbuf = pirp_saverw->Buffer + UrbLength;

		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x00000022;	
		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbControlTransfer.PipeHandle = (void*)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
		purb->UrbControlTransfer.TransferFlags = 0xb;
		purb->UrbControlTransfer.hca.HcdEndpoint = (void*)0xffffffff;
		purb->UrbControlTransfer.hca.HcdIrp = (void*)0xdeadf00d;
		purb->UrbControlTransfer.SetupPacket[0] = (USB_TYPE_STANDARD | USB_DIR_IN | USB_RECIP_INTERFACE);
		purb->UrbControlTransfer.SetupPacket[1] = USB_REQ_GET_DESCRIPTOR;
		purb->UrbControlTransfer.SetupPacket[6] = (unsigned char )(purb->UrbControlDescriptorRequest.TransferBufferLength&0x00ff);
		purb->UrbControlTransfer.SetupPacket[7] = (unsigned char )((purb->UrbControlDescriptorRequest.TransferBufferLength&0xff00)>>8);
		purb->UrbControlDescriptorRequest.TransferBuffer = pbuf;
//		purb->UrbControlDescriptorRequest.Reserved1 = 0x0680;
//		purb->UrbControlDescriptorRequest.Reserved2 = size;	// TransferBufferLength;

		if (dev) {
			ret = 0;
			udev = usb_open(dev);
			if (udev) {
				ret = usb_control_msg(udev,	(USB_TYPE_STANDARD | USB_DIR_IN | USB_RECIP_INTERFACE),
								USB_REQ_GET_DESCRIPTOR,
								(USB_DT_REPORT << 8),
								purb->UrbControlGetInterfaceRequest.Interface-1,
								(char*)pbuf,
								purb->UrbControlDescriptorRequest.TransferBufferLength,
								timeout_control_msg);
				usb_close(udev);
			}
		}
		else
			break;

		if (ret >= 0) {
			purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		break;	// URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE

	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:	// 0x000B , get Device OR Configuration descriptors
		size = pirp_saverw->BufferSize - (u_int32_t)UrbLength;
		pbuf = pirp_saverw->Buffer + UrbLength;

		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x00000022;
		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbControlTransfer.PipeHandle = (void*)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
		purb->UrbControlTransfer.hca.HcdEndpoint = (void*)0xffffffff;
		purb->UrbControlTransfer.hca.HcdIrp = (void*)0xdeadf00d;
		purb->UrbControlDescriptorRequest.Reserved1 = 0x0680;
		purb->UrbControlDescriptorRequest.Reserved2 = size;
		purb->UrbControlDescriptorRequest.TransferBuffer = pbuf;

		if (purb->UrbControlDescriptorRequest.DescriptorType == 1) {		// device descriptor
			bzero(pbuf, size);
			if (dev) {
				udev = usb_open(dev);
				if (udev) {
					ret = usb_get_descriptor(udev, USB_DT_DEVICE, purb->UrbControlDescriptorRequest.Index,
								pbuf, purb->UrbControlDescriptorRequest.TransferBufferLength);
					usb_close(udev);
				}
			}
			else
				break;

			if (ret > 0)
				purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			else
				purb->UrbControlDescriptorRequest.TransferBufferLength = 0;

			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		else if (purb->UrbControlDescriptorRequest.DescriptorType == 2) {	// configuration descriptor
			purb->UrbControlTransfer.TransferFlags = 0xb;
			bzero(pbuf, size);
			if (dev) {
				udev = usb_open(dev);
				if (udev) {
					ret = usb_get_descriptor(udev, USB_DT_CONFIG, purb->UrbControlDescriptorRequest.Index,
								pbuf, purb->UrbControlDescriptorRequest.TransferBufferLength);
					usb_close(udev);
				}
			}
			else
				break;

			if (ret > 0)
				purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			else
				purb->UrbControlDescriptorRequest.TransferBufferLength = 0;

			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		else if (purb->UrbControlDescriptorRequest.DescriptorType == 3) {		// string descriptor
//			purb->UrbControlTransfer.TransferFlags = 0x0b;
			ret = 0;
			if (dev) {
				udev = usb_open(dev);
				if (udev) {
					ret = usb_get_string(udev, purb->UrbControlDescriptorRequest.Index,
								   purb->UrbControlDescriptorRequest.LanguageId, (char*)pbuf,
								   purb->UrbControlDescriptorRequest.TransferBufferLength);
					usb_close(udev);
				}
			}
			else
				break;

			if (ret > 0)
				purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			else
				purb->UrbControlDescriptorRequest.TransferBufferLength = 0;

			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		else 
			PDEBUG("handleURB: descriptor type = 0x%x, not supported\n", purb->UrbControlDescriptorRequest.DescriptorType);
		break;	// URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:

	case URB_FUNCTION_SELECT_CONFIGURATION:	// 0x0000
		// we must set configurationHandle & interfaceHandle & pipeHandle
		pirp_saverw->BufferSize = purb->UrbHeader.Length;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		if (dev) {
			if (purb->UrbSelectConfiguration.ConfigurationDescriptor == 0x0)
				break;
			else {
				purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;

				tmp_config = *(pirp_saverw->Buffer + UrbLength + 5) - 1;
				// *(pirp_saverw->Buffer + UrbLength + 5) is bConfigurationVaule field of Configuration Descriptor
				purb->UrbSelectConfiguration.ConfigurationHandle = (void*)0x80000000+0x1000*(tmp_config+1);

				char *tmp = (char*)&(purb->UrbSelectConfiguration.Interface);
				int tmp_len = purb->UrbHeader.Length - 24;
				
				for (i = 0; tmp_len > 0; i++) {
					int tmp_int = (*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceNumber;
					int tmp_alt = (*(USBD_INTERFACE_INFORMATION*)tmp).AlternateSetting;

//					(*(USBD_INTERFACE_INFORMATION*)tmp).Length = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bLength;
//					(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceNumber = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceNumber;
//					(*(USBD_INTERFACE_INFORMATION*)tmp).AlternateSetting = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bAlternateSetting;
					(*(USBD_INTERFACE_INFORMATION*)tmp).Class = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceClass;
					(*(USBD_INTERFACE_INFORMATION*)tmp).SubClass = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceSubClass;
					(*(USBD_INTERFACE_INFORMATION*)tmp).Protocol = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceProtocol;
//					(*(USBD_INTERFACE_INFORMATION*)tmp).Reserved = 0x00;
					(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle = (void*)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1);
//					(*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bNumEndpoints;
					for (j = 0; j < (*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes; j++) {
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].wMaxPacketSize;
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].EndpointAddress = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bEndpointAddress;
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].Interval = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bInterval;
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeType = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bmAttributes & USB_ENDPOINT_TYPE_MASK;
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle = (void*)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1)+0x1*(j+2);
						if ((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize > MAX_BUFFER_SIZE)
							(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize = MAX_BUFFER_SIZE;
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags = 0;
					}
					tmp_len -= (*(USBD_INTERFACE_INFORMATION*)tmp).Length;
					tmp += (*(USBD_INTERFACE_INFORMATION*)tmp).Length;
				}
			}
		}
		break;	// URB_FUNCTION_SELECT_CONFIGURATION

	case 0x2a: // _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL, added in U2EC, it's the same size as _URB_CONTROL_DESCRIPTOR_REQUEST, 
		// so we can handle it as URB_CONTROL_DESCRIPTOR_REQUEST
		pirp_saverw->BufferSize = purb->UrbHeader.Length;
		pirp_saverw->Status = 0xc0000010;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
		purb->UrbHeader.Status = 0x80000300;
		purb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		// it's MS_FeatureDescriptorIndex in _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL
//		purb->UrbControlDescriptorRequest.LanguageId = 0x0004;
		break;	// 0x2a

	case URB_FUNCTION_SELECT_INTERFACE:	// 0x0001
		pirp_saverw->BufferSize = purb->UrbHeader.Length;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		if (dev) {
			purb->UrbHeader.Status = 0;
			purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
			purb->UrbHeader.UsbdFlags = 0;

			tmp_config = (((u_int32_t)purb->UrbSelectInterface.ConfigurationHandle)<<16)>>28;
			tmp_config--;
			char *tmp = (char*)&(purb->UrbSelectInterface.Interface);
			int tmp_int = (*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceNumber;
			int tmp_alt = (*(USBD_INTERFACE_INFORMATION*)tmp).AlternateSetting;

//			purb->UrbSelectInterface.ConfigurationHandle = 0x80000000+0x1000*(tmp_config+1);

//			(*(USBD_INTERFACE_INFORMATION*)tmp).Length = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bLength;
//			(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceNumber = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceNumber;
//			(*(USBD_INTERFACE_INFORMATION*)tmp).AlternateSetting = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bAlternateSetting;
			(*(USBD_INTERFACE_INFORMATION*)tmp).Class = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceClass;
			(*(USBD_INTERFACE_INFORMATION*)tmp).SubClass = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceSubClass;
			(*(USBD_INTERFACE_INFORMATION*)tmp).Protocol = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceProtocol;
			(*(USBD_INTERFACE_INFORMATION*)tmp).Reserved = 0x00;
			(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle = (void*)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1);
			(*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bNumEndpoints;

			for (j = 0; j < (*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes; j++) {
				(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].wMaxPacketSize;
				(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].EndpointAddress = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bEndpointAddress;
				(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].Interval = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bInterval;
				(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeType = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bmAttributes & USB_ENDPOINT_TYPE_MASK;
				(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle = (void*)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1)+0x1*(j+2);
				if ((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize > MAX_BUFFER_SIZE)
					(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize = MAX_BUFFER_SIZE;
				(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags = 0;
			}
		}
		break;	// URB_FUNCTION_SELECT_INTERFACE 0x0001

	case URB_FUNCTION_CLASS_OTHER:		// 0x001F
		length = tmpL = tmpReq = tmpReqType = 0;

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				tmpL = purb->UrbControlVendorClassRequest.TransferBufferLength;
				tmpReq = purb->UrbControlVendorClassRequest.Request;
				if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(purb->UrbBulkOrInterruptTransfer.TransferFlags)) {
					tmpReqType = (USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_OTHER);
					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					ret = usb_control_msg(udev, (USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_OTHER),
							purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);
//					PDEBUG("usb_control_msg(), direction: out, return: %d\n", ret);
				} else {
					tmpReqType = (USB_TYPE_CLASS | USB_DIR_IN | USB_RECIP_OTHER);
					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					ret = usb_control_msg(udev, (USB_TYPE_CLASS | USB_DIR_IN | USB_RECIP_OTHER),
							purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);
//					PDEBUG("usb_control_msg(), direction: in, return: %d\n", ret);
				}
				usb_close(udev);
				if (ret < 0)
					break;
				else length = ret;
			}
			else
				break;
		}
		else
			break;

		pbuf = pirp_saverw->Buffer + UrbLength;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x22;
		
		purb->UrbControlTransfer.PipeHandle = (void*)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
		purb->UrbControlTransfer.TransferFlags = 0x0a;
		purb->UrbControlTransfer.TransferBufferLength = length;
		purb->UrbControlTransfer.UrbLink = 0;
		purb->UrbControlTransfer.SetupPacket[0] = tmpReqType;
		purb->UrbControlTransfer.SetupPacket[1] = tmpReq;
		purb->UrbControlTransfer.SetupPacket[6] = (unsigned char )(tmpL&0x00ff);
		purb->UrbControlTransfer.SetupPacket[7] = (unsigned char )((tmpL&0xff00)>>8);
		pirp_saverw->BufferSize = purb->UrbControlTransfer.TransferBufferLength + purb->UrbHeader.Length;
		break;	// URB_FUNCTION_CLASS_OTHER

	case URB_FUNCTION_CLASS_INTERFACE:
		length = tmpL = tmpReq = tmpReqType = 0;
		ret = -1;

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				tmpL = purb->UrbControlVendorClassRequest.TransferBufferLength;
				tmpReq = purb->UrbControlVendorClassRequest.Request;
				if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(purb->UrbBulkOrInterruptTransfer.TransferFlags)) {

					PDEBUG("OUT ");
					PDEBUG("req: %x ", purb->UrbControlVendorClassRequest.Request);
					PDEBUG("value: %x ", purb->UrbControlVendorClassRequest.Value);
					PDEBUG("index: %x ", purb->UrbControlVendorClassRequest.Index);
					PDEBUG("len: %x\n", purb->UrbControlVendorClassRequest.TransferBufferLength);

					if (Is_Canon && flag_canon_state != 0)
					{
						if (	purb->UrbControlVendorClassRequest.Request == 0x2 &&
							purb->UrbControlVendorClassRequest.Value == 0x0 &&
							purb->UrbControlVendorClassRequest.Index == 0x100
						)
						{
							PDEBUG("user cancel printing...\n");
							flag_canon_state = 0;
							flag_canon_ok = 0;
						}
					}

					tmpReqType = (USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_INTERFACE);
					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					ret = usb_control_msg(udev, tmpReqType, purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);
//					PDEBUG("usb_control_msg() out return: %d\n", ret);
					((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
				} else {

					PDEBUG("IN ");
					PDEBUG("req: %x ", purb->UrbControlVendorClassRequest.Request);
					PDEBUG("value: %x ", purb->UrbControlVendorClassRequest.Value);
					PDEBUG("index: %x ", purb->UrbControlVendorClassRequest.Index);
					PDEBUG("len: %x\n", purb->UrbControlVendorClassRequest.TransferBufferLength);

					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					tmpReqType = (USB_TYPE_CLASS | USB_DIR_IN | USB_RECIP_INTERFACE);
					ret = usb_control_msg(udev, tmpReqType, purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);

					if (	Is_Canon &&
						purb->UrbControlVendorClassRequest.Request == 0x0 &&
						purb->UrbControlVendorClassRequest.Value == 0x0 &&
						purb->UrbControlVendorClassRequest.Index == 0x100
					)
						flag_canon_monitor = 1;
					else if (Is_Canon)
					{
						flag_canon_monitor = 0;
						((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
					}
					else if (Is_HP && ((PCONNECTION_INFO)curt_pos)->count_class_int_in < 3)	// HP Deskjet 3920 
						((PCONNECTION_INFO)curt_pos)->count_class_int_in++;

/*
					if (	Is_Canon &&
						purb->UrbControlVendorClassRequest.Request == USBLP_REQ_GET_STATUS &&
						purb->UrbControlVendorClassRequest.Value == 0x0 &&
						purb->UrbControlVendorClassRequest.TransferBufferLength == 0x1
					)
					{
						status = *pbuf;
						err_canon = 0;
						if (~status & LP_PERRORP)
							err_canon = 3;
						if (status & LP_POUTPA)
							err_canon = 1;
						if (~status & LP_PSELECD)
							err_canon = 2;

						PDEBUG("state: %d err: %d\n", flag_canon_state, err_canon);

						if (flag_canon_state == 1 && err_canon != 0)
							flag_canon_state = 2;		// "out of paper"
						else if (flag_canon_state == 1 && flag_canon_ok++ > 4 && err_canon == 0)
							flag_canon_state = 3;		// warm-up
						else if (flag_canon_state == 2 && err_canon == 0)
						{					// "out of paper" => "ok"
							flag_canon_state = 3;
							flag_canon_ok = 0;
						}
						else if (flag_canon_state == 3 && err_canon == 0)
							flag_canon_ok++;	

						PDEBUG("status: %s, ok:%d\n", usblp_messages[err_canon], flag_canon_ok);
					}
*/

//					PDEBUG("usb_control_msg() in return: %d\n", ret);
				}
				usb_close(udev);
				if (ret < 0)
					break;	
				else length = ret;
			}
			else
				break;
		}
		else
			break;

		pbuf = pirp_saverw->Buffer + UrbLength;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x22;
		
		purb->UrbControlTransfer.PipeHandle = (void*)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
		purb->UrbControlTransfer.TransferFlags = 0x0b;
		purb->UrbControlTransfer.TransferBufferLength = length;
		purb->UrbControlTransfer.UrbLink = 0;
		purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint = (void*)0xffffffff;
		purb->UrbBulkOrInterruptTransfer.hca.HcdIrp = (void*)0xdeadf00d;
		purb->UrbControlTransfer.SetupPacket[0] = tmpReqType;
		purb->UrbControlTransfer.SetupPacket[1] = tmpReq;
		purb->UrbControlTransfer.SetupPacket[6] = (unsigned char )(tmpL&0x00ff);
		purb->UrbControlTransfer.SetupPacket[7] = (unsigned char )((tmpL&0xff00)>>8);
		pirp_saverw->BufferSize = purb->UrbControlTransfer.TransferBufferLength + purb->UrbHeader.Length;
		break;	// URB_FUNCTION_CLASS_INTERFACE

	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:	// 0x0009
		pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;

		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.Status = 0;
		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x22;
		purb->UrbBulkOrInterruptTransfer.UrbLink = 0;
		purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint = (void*)0xffffffff;
		purb->UrbBulkOrInterruptTransfer.hca.HcdIrp = (void*)0xdeadf00d;
		purb->UrbControlDescriptorRequest.TransferBuffer = pbuf;

//		PDEBUG("PipeHandle: 0x%x\n", purb->UrbBulkOrInterruptTransfer.PipeHandle);
		tmp_config = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<16)>>28;
		tmp_int = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<20)>>28;
		tmp_alt = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<24)>>28;
		tmp_ep = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<28)>>28;
		tmp_config--;
		tmp_int--;
		tmp_alt--;
		tmp_ep -= 2;
		PDEBUG("config, int, alt, ep: %x %x %x %x\n", tmp_config, tmp_int, tmp_alt, tmp_ep);

		if (dev) {
			tmp_intclass = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceClass;
			if (tmp_intclass == 8) {
				timeout_bulk_read_msg_tmp = 1000;
				timeout_bulk_write_msg_tmp = 1000;
			} else if (Is_EPSON && tmp_intclass != 7)
			{
				timeout_bulk_read_msg_tmp = timeout_bulk_read_msg;
				timeout_bulk_write_msg_tmp = 30000;
			}
			else
			{
				timeout_bulk_read_msg_tmp = timeout_bulk_read_msg;
				timeout_bulk_write_msg_tmp = timeout_bulk_write_msg;
			}

			if (Is_Canon && tmp_intclass != 7)
			{
				flag_canon_state = 0;
				flag_canon_ok = 0;
			}

			udev = usb_open(dev);
			if (udev) {	
				if ((USB_ENDPOINT_TYPE_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bmAttributes) == USB_ENDPOINT_TYPE_BULK) {
					count_int_epson = 0;
					if ((USB_ENDPOINT_DIR_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress) == USB_ENDPOINT_OUT) {
						purb->UrbBulkOrInterruptTransfer.TransferFlags = 2;
						gettimeofday(&tv_ref, NULL);
						alarm(0);
						if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
							purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
						if (Is_Canon && tmp_intclass == 7)
						{
							if (flag_canon_state == 3)
							{
								PDEBUG("remain: %d\n", purb->UrbBulkOrInterruptTransfer.TransferBufferLength - data_canon_done);
								ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)(pbuf + data_canon_done), purb->UrbBulkOrInterruptTransfer.TransferBufferLength - data_canon_done, 30000);
								if (ret > 0)
								{
									ret += data_canon_done;
									data_canon_done = 0;
								}
								else
									flag_canon_ok = 0;
							}
							else
							{
								data_canon_done = 0;
								if (flag_canon_new_printer_cmd)
									ret = usb_bulk_write_sp(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 32767, &data_canon_done, 4096);
								else
									ret = usb_bulk_write_sp(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 5000, &data_canon_done, USB_Ver_N20?128:512);
								if (ret < 0 && data_canon_done > 0)
									PDEBUG("done: %d\n", data_canon_done);
							}
						}
						else if (Is_HP && (Is_HP_LaserJet || Is_HP_LaserJet_MFP) && tmp_intclass == 7)
							ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 500);
						else
							ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_write_msg_tmp);
						alarm(1);
						gettimeofday(&tv_now, NULL);

						if (Is_EPSON)
						{
							if (ret == 0 && purb->UrbBulkOrInterruptTransfer.TransferBufferLength == 2)
							{
								PDEBUG("retry...");
								ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_write_msg_tmp);
							}
							else if (ret >= 4096)
							{
								count_bulk_read_epson = 0;
								flag_monitor_epson = 0;
							}
						}

						PDEBUG("usb_bulk_write() return: %d\n", ret);

						count_bulk_write ++;
						count_bulk_read_ret0_1 = 0;

						if (ret < 0) {
							if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
							else
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);

							if (Is_Canon && tmp_intclass == 7 && !flag_canon_new_printer_cmd)
							{
								if (flag_canon_state == 0 && ret == -ETIMEDOUT)
								{
									PDEBUG("copy data_buf !!!\n");

									flag_canon_state = 1;
									flag_canon_ok = 0;
									memcpy(data_canon, data_buf, MAX_BUF_LEN);

									usb_close(udev);
									pirp_saverw->BufferSize = purb->UrbHeader.Length;
									pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
									return 0;
								}
								else if (flag_canon_state != 1)
								{
									PDEBUG("skip...\n");
	
									usb_close(udev);
									pirp_saverw->BufferSize = purb->UrbHeader.Length;
									pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
									return 0;
								}
							}
						}
						else if (Is_Canon && tmp_intclass == 7)	// ret >= 0
						{
							flag_canon_state = 0;
							flag_canon_ok = 0;
							((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
						}
					} else {
						purb->UrbBulkOrInterruptTransfer.TransferFlags = 3;
						gettimeofday(&tv_ref, NULL);
						alarm(0);
						if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
							purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
						if (Is_HP && Is_HP_LaserJet_MFP && tmp_intclass == 7)
							ret = usb_bulk_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 500);
						else
							ret = usb_bulk_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_read_msg_tmp);
						alarm(1);
						gettimeofday(&tv_now, NULL);

						if (Is_Canon && tmp_intclass == 7)
						{
							if (ret > 64 && ret < 256 && memcmp(pbuf+2, "BST:", 4) == 0)
							{
								if (flag_canon_monitor == 1 && ((PCONNECTION_INFO)curt_pos)->count_class_int_in < 3)
								{
									flag_canon_monitor = 0;
									((PCONNECTION_INFO)curt_pos)->count_class_int_in++;

									PDEBUG("mon: [%d] ", ((PCONNECTION_INFO)curt_pos)->count_class_int_in);
								}

								if (*(pbuf+7) == '0')			// ok
									err_canon = 0;
								else if (*(pbuf+7) == '8')		// operation call
									err_canon = 1;
								else					// unknown error
									err_canon = 2;

//								if (flag_canon_state == 1 && err_canon != 0)
								if (flag_canon_state == 1 && err_canon == 1)
								{					// "out of paper"
									flag_canon_state = 2;
									flag_canon_ok = 0;
								}
//								else if (flag_canon_state == 1 && err_canon == 0 && ++flag_canon_ok > 5)
								else if (flag_canon_state == 1 && err_canon != 1 && ++flag_canon_ok > 5)
									flag_canon_state = 3;		// warm-up
//								else if (flag_canon_state == 2 && err_canon == 0)
								else if (flag_canon_state == 2 && err_canon != 1)
								{					// "out of paper" => "ok"
									flag_canon_state = 3;
									flag_canon_ok = 0;
								}
//								else if (flag_canon_state == 3 && err_canon == 0)
								else if (flag_canon_state == 3 && err_canon != 1)
									flag_canon_ok++;	

								PDEBUG("status:[%s] state:[%d] ok:[%d] ", usblp_messages_canon[err_canon], flag_canon_state, flag_canon_ok);
							}
							else if (!flag_canon_new_printer_cmd && ret > 256 && memcmp(pbuf+2, "xml ", 4) == 0)
							{
								PDEBUG("found new printer cmd!\n");
								flag_canon_new_printer_cmd = 1;
							}
						}
						else
						{
							flag_canon_monitor = 0;
							((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
						}

						PDEBUG("usb_bulk_read() return: %d\n", ret);

						if (Is_HP) {	// HP, if this connection only includes HP ToolBox
							if (ret == 0) {
								count_bulk_read_ret0_1 ++;
								if (count_bulk_read_ret0_1 >= 30) {
									count_bulk_write = 0;
									count_bulk_read = 0;
									count_bulk_read_ret0_1 = 0;
									count_bulk_read_ret0_2 = 0;
								}
								if (Is_HP_LaserJet) {						// laserjet 33xx series
									if (count_bulk_write == 1 && count_bulk_read == 1)
										count_bulk_read_ret0_2 ++;
								}
								else if (Is_HP_OfficeJet) {					// officejet 56xx series
									if (count_bulk_write == 2 && count_bulk_read == 2)
										count_bulk_read_ret0_2 ++;
								}
							}
							else if (ret > 0) {
								count_bulk_read ++;
								count_bulk_read_ret0_1 = 0;
							}
						}
						else if (Is_EPSON) {
							if (ret == 9 && count_bulk_read_epson == 0 && memcmp(EPSON_BR_STR1_09, pbuf, 9) == 0)
								count_bulk_read_epson = 1;
							else if (ret == 19) {
								if (count_bulk_read_epson == 1 && memcmp(EPSON_BR_STR2_19, pbuf, 19) == 0)
									count_bulk_read_epson = 2;
								else if (count_bulk_read_epson == 2 && memcmp(EPSON_BR_STR3_19, pbuf, 19) == 0)
									count_bulk_read_epson = 3;
							}
						}

						if (ret < 0) {
							count_bulk_read_epson = -1;
							flag_monitor_epson = 0;

							if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
							else
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);
						}

/*						if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength != 0 && ret == 0)
							pirp_saverw->Status = 0xC0000004;	// STATUS_INFO_LENGTH_MISMATCH
						else if (ret < 0)
							pirp_saverw->Status = 0xC00000AE;	// STATUS_PIPE_BUSY
*/
					}
				} else {
					PDEBUG("USB_ENDPOINT_TYPE_INTERRUPT\n");

					if (!Is_EPSON)
					{
						usb_close(udev);
						pirp_saverw->BufferSize = purb->UrbHeader.Length;
						pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
						return 0;
					}
					else
					{
						if ((USB_ENDPOINT_DIR_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress) == USB_ENDPOINT_OUT) {
							PDEBUG("USB_ENDPOINT_OUT\n");
	
							purb->UrbBulkOrInterruptTransfer.TransferFlags = 2;
	
							gettimeofday(&tv_ref, NULL);
							alarm(0);
							if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
								purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
							ret = usb_interrupt_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_write_msg_tmp);
							alarm(1);
							gettimeofday(&tv_now, NULL);
	
							PDEBUG("usb_interrupt_write() return: %d\n", ret);
	
							if (ret < 0) {
								if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
								else
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);
							}
						} else {
							PDEBUG("USB_ENDPOINT_IN\n");
	
							purb->UrbBulkOrInterruptTransfer.TransferFlags=3;
	
							gettimeofday(&tv_ref, NULL);
							alarm(0);
							if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
								purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
//							ret = usb_interrupt_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_read_msg_tmp);
							if (count_int_epson)
								ret = usb_interrupt_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 5000);
							else								
								ret = usb_interrupt_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 1);
							alarm(1);
							gettimeofday(&tv_now, NULL);

							PDEBUG("usb_interrupt_read() return: %d\n", ret);

							if (ret < 0) {
								if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
								else
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);
							}
						}

						if (!count_int_epson)
							count_int_epson = 1;
					}
				}

				if (ret < 0) {
					ret = 0;
					count_err++;
				}
				else
					count_err = 0;

				if (count_err > 255)
					pirp_saverw->Status = 0xC00000AD;	// STATUS_INVALID_PIPE_STATE

				usb_close(udev);
			}
			else
				ret = 0;
		}
		else
			ret = 0;

		purb->UrbBulkOrInterruptTransfer.TransferBufferLength = ret;

		if ((USB_ENDPOINT_DIR_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress) == USB_ENDPOINT_OUT)
			pirp_saverw->BufferSize = purb->UrbHeader.Length;
		else
			pirp_saverw->BufferSize = purb->UrbHeader.Length + purb->UrbBulkOrInterruptTransfer.TransferBufferLength;
		break;	// URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER 0x0009

	case URB_FUNCTION_ABORT_PIPE:
		PDEBUG("\n\t URB_FUNCTION_ABORT_PIPE");

		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;	// according to packet
		purb->UrbHeader.UsbdFlags = 0x10;

//		PDEBUG("\n\t PipeHandle: 0x%x", purb->UrbPipeRequest.PipeHandle);
		tmp_config = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<16)>>28;
		tmp_int = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<20)>>28;
		tmp_alt = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<24)>>28;
		tmp_ep = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<28)>>28;
		tmp_config--;
		tmp_int--;
		tmp_alt--;
		tmp_ep -= 2;
//		PDEBUG("\n\t config, int, alt, ep: %x %x %x %x", tmp_config, tmp_int, tmp_alt, tmp_ep);

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				usb_clear_halt(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress);
				usb_close(udev);
			}
		}
		break;

	case URB_FUNCTION_RESET_PIPE:
		PDEBUG("\n\t URB_FUNCTION_RESET_PIPE");

		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;	// according to packet
		purb->UrbHeader.UsbdFlags = 0x10;

//		PDEBUG("\n\t PipeHandle: 0x%x", purb->UrbPipeRequest.PipeHandle);
		tmp_config = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<16)>>28;
		tmp_int = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<20)>>28;
		tmp_alt = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<24)>>28;
		tmp_ep = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<28)>>28;
		tmp_config--;
		tmp_int--;
		tmp_alt--;
		tmp_ep -= 2;
//		PDEBUG("\n\t config, int, alt, ep: %x %x %x %x", tmp_config, tmp_int, tmp_alt, tmp_ep);

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				usb_resetep(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress);
				usb_close(udev);
			}
		}
		break;

	default:
		PDEBUG("\n !!! Unsupported URB_FUNCTION: 0x%x\n\n", purb->UrbHeader.Function);
		break;
	} // end of switch urb function

	pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;
	return 1;

} // end of handleURB32

/*
 * Handle urbs in irp_saverw.Buffer.
 */
int handleURB_64(PIRP_SAVE pirp_saverw, struct u2ec_list_head *curt_pos)
{
	PURB_64		purb;
	unsigned char*	pbuf;
	USHORT		UrbLength;
	int		size, i, j;
	int		ret = 0;
	int		length = 0;
	int		tmpL = 0;
	int		tmpReq = 0;
	int		tmpReqType = 0;

	u_int64_t tmp_config;
	u_int64_t tmp_int;
	u_int64_t tmp_alt;
	u_int64_t tmp_ep;
	u_int64_t tmp_intclass;

	int timeout_bulk_read_msg_tmp;
	int timeout_bulk_write_msg_tmp;
//	unsigned char status;
	int	err_canon;

	purb = (PURB_64)pirp_saverw->Buffer;
	UrbLength = purb->UrbHeader.Length;

	PDEBUG("\n into handleURB_64\n");

	if (Is_HP && purb->UrbHeader.Function != URB_FUNCTION_CLASS_INTERFACE)
		((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
	else if (	Is_Canon &&
			purb->UrbHeader.Function != URB_FUNCTION_CLASS_INTERFACE &&
			purb->UrbHeader.Function != URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER
	)
		((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;

	// action differs to functions
	switch (purb->UrbHeader.Function) {
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		size = pirp_saverw->BufferSize - (u_int32_t)UrbLength;
		pbuf = pirp_saverw->Buffer + UrbLength;

		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x00000022;	
		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbControlTransfer.PipeHandle = (u_int64_t)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
		purb->UrbControlTransfer.TransferFlags = 0xb;
//		purb->UrbControlTransfer.hca.HcdEndpoint = (u_int64_t)0xffffffff;
//		purb->UrbControlTransfer.hca.HcdIrp = (u_int64_t)0xdeadf00d;
		purb->UrbControlTransfer.SetupPacket[0] = (USB_TYPE_STANDARD | USB_DIR_IN | USB_RECIP_INTERFACE);
		purb->UrbControlTransfer.SetupPacket[1] = USB_REQ_GET_DESCRIPTOR;
		purb->UrbControlTransfer.SetupPacket[6] = (unsigned char )(purb->UrbControlDescriptorRequest.TransferBufferLength&0x00ff);
		purb->UrbControlTransfer.SetupPacket[7] = (unsigned char )((purb->UrbControlDescriptorRequest.TransferBufferLength&0xff00)>>8);
		purb->UrbControlDescriptorRequest.TransferBuffer = (u_int64_t)(u_int32_t)pbuf;
//		purb->UrbControlDescriptorRequest.Reserved1 = 0x0680;
//		purb->UrbControlDescriptorRequest.Reserved2 = size;	// TransferBufferLength;

		if (dev) {
			ret = 0;
			udev = usb_open(dev);
			if (udev) {
				ret = usb_control_msg(udev,	(USB_TYPE_STANDARD | USB_DIR_IN | USB_RECIP_INTERFACE),
								USB_REQ_GET_DESCRIPTOR,
								(USB_DT_REPORT << 8),
								purb->UrbControlGetInterfaceRequest.Interface-1,
								(char*)pbuf,
								purb->UrbControlDescriptorRequest.TransferBufferLength,
								timeout_control_msg);
				usb_close(udev);
			}
		}
		else
			break;

		if (ret >= 0) {
			purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		break;	// URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE

	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:	// 0x000B, get Device OR Configuration descriptors
		size = pirp_saverw->BufferSize - (u_int32_t)UrbLength;
		pbuf = pirp_saverw->Buffer + UrbLength;

		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x00000022;
		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbControlTransfer.PipeHandle = (u_int64_t)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
//		purb->UrbControlTransfer.hca.HcdEndpoint = (u_int64_t)0xffffffff;
//		purb->UrbControlTransfer.hca.HcdIrp = (u_int64_t)0xdeadf00d;
		purb->UrbControlDescriptorRequest.Reserved1 = 0x0680;
		purb->UrbControlDescriptorRequest.Reserved2 = size;
		purb->UrbControlDescriptorRequest.TransferBuffer = (u_int64_t)(u_int32_t)pbuf;

		if (purb->UrbControlDescriptorRequest.DescriptorType == 1) {		// device descriptor
			bzero(pbuf, size);
			if (dev) {
				udev = usb_open(dev);
				if (udev) {
					ret = usb_get_descriptor(udev, USB_DT_DEVICE, purb->UrbControlDescriptorRequest.Index,
								pbuf, purb->UrbControlDescriptorRequest.TransferBufferLength);
					usb_close(udev);
				}
			}
			else
				break;

			if (ret > 0)
				purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			else
				purb->UrbControlDescriptorRequest.TransferBufferLength = 0;

			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		else if (purb->UrbControlDescriptorRequest.DescriptorType == 2) {	// configuration descriptor
			purb->UrbControlTransfer.TransferFlags = 0xb;
			bzero(pbuf, size);
			if (dev) {
				udev = usb_open(dev);
				if (udev) {
					ret = usb_get_descriptor(udev, USB_DT_CONFIG, purb->UrbControlDescriptorRequest.Index,
								pbuf, purb->UrbControlDescriptorRequest.TransferBufferLength);
					usb_close(udev);
				}
			}
			else
				break;

			if (ret > 0)
				purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			else
				purb->UrbControlDescriptorRequest.TransferBufferLength = 0;

			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		else if (purb->UrbControlDescriptorRequest.DescriptorType == 3) {		// string descriptor
//			purb->UrbControlTransfer.TransferFlags = 0x0b;
			ret = 0;
			if (dev) {
				udev = usb_open(dev);
				if (udev) {
					ret = usb_get_string(udev, purb->UrbControlDescriptorRequest.Index,
								   purb->UrbControlDescriptorRequest.LanguageId, (char*)pbuf,
								   purb->UrbControlDescriptorRequest.TransferBufferLength);
					usb_close(udev);
				}
			}
			else
				break;

			if (ret > 0)
				purb->UrbControlDescriptorRequest.TransferBufferLength = ret;
			else
				purb->UrbControlDescriptorRequest.TransferBufferLength = 0;

			pirp_saverw->BufferSize = purb->UrbControlDescriptorRequest.TransferBufferLength + purb->UrbHeader.Length;
		}
		else 
			PDEBUG("handleURB64: descriptor type = 0x%x, not supported\n", purb->UrbControlDescriptorRequest.DescriptorType);
		break;	// URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:

	case URB_FUNCTION_SELECT_CONFIGURATION:	// 0x0000
		// we must set configurationHandle & interfaceHandle & pipeHandle
		pirp_saverw->BufferSize = purb->UrbHeader.Length;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;
		pbuf = pirp_saverw->Buffer + UrbLength;

		if (dev) {
			if (purb->UrbSelectConfiguration.ConfigurationDescriptor == 0x0)
				break;
			else {
				purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;

				tmp_config = (*(USB_CONFIGURATION_DESCRIPTOR_64*)pbuf).bConfigurationValue- 1;
				// *(pirp_saverw->Buffer + UrbLength + 5) is bConfigurationVaule field of Configuration Descriptor

				purb->UrbSelectConfiguration.ConfigurationHandle = (u_int64_t)0x80000000+0x1000*(tmp_config+1);

				char *tmp = (char*)&(purb->UrbSelectConfiguration.Interface);
				int tmp_len = purb->UrbHeader.Length - 0x28;

				for (i = 0; tmp_len > 0; i++) {
					int tmp_int=(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceNumber;
					int tmp_alt=(*(USBD_INTERFACE_INFORMATION_64*)tmp).AlternateSetting;

//					(*(USBD_INTERFACE_INFORMATION_64*)tmp).Length = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bLength;
//					(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceNumber = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceNumber;
//					(*(USBD_INTERFACE_INFORMATION_64*)tmp).AlternateSetting = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bAlternateSetting;
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).Class = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceClass;
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).SubClass = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceSubClass;
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).Protocol = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceProtocol;
//					(*(USBD_INTERFACE_INFORMATION_64*)tmp).Reserved = 0x00;
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle = (u_int64_t)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1);
//					(*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bNumEndpoints;
					for (j = 0; j < (*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes; j++) {
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].wMaxPacketSize;
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].EndpointAddress = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bEndpointAddress;
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].Interval = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bInterval;
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bmAttributes & USB_ENDPOINT_TYPE_MASK;
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle = (u_int64_t)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1)+0x1*(j+2);
						if ((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize > MAX_BUFFER_SIZE)
							(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize = MAX_BUFFER_SIZE;
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags = 0;
					}
					tmp_len -= (*(USBD_INTERFACE_INFORMATION_64*)tmp).Length;
					tmp += (*(USBD_INTERFACE_INFORMATION_64*)tmp).Length;
				}
			}
		}
		break;	// URB_FUNCTION_SELECT_CONFIGURATION

	case 0x2a: // _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL, added in U2EC, it's the same size as _URB_CONTROL_DESCRIPTOR_REQUEST, 
		// so we can handle it as URB_CONTROL_DESCRIPTOR_REQUEST
		pirp_saverw->BufferSize = purb->UrbHeader.Length;
		pirp_saverw->Status = 0xc0000010;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
		purb->UrbHeader.Status = 0x80000300;
		purb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		// it's MS_FeatureDescriptorIndex in _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL
//		purb->UrbControlDescriptorRequest.LanguageId = 0x0004;
		break;	// 0x2a

	case URB_FUNCTION_SELECT_INTERFACE:	// 0x0001
		pirp_saverw->BufferSize = purb->UrbHeader.Length;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		if (dev) {
			purb->UrbHeader.Status = 0;
			purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
			purb->UrbHeader.UsbdFlags = 0;

			tmp_config = (((u_int32_t)purb->UrbSelectInterface.ConfigurationHandle)<<16)>>28;
			tmp_config--;
			char *tmp = (char*)&(purb->UrbSelectInterface.Interface);
			int tmp_int = (*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceNumber;
			int tmp_alt = (*(USBD_INTERFACE_INFORMATION_64*)tmp).AlternateSetting;

//			purb->UrbSelectInterface.ConfigurationHandle = 0x80000000+0x1000*(tmp_config+1);

//			(*(USBD_INTERFACE_INFORMATION_64*)tmp).Length = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bLength;
//			(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceNumber = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceNumber;
//			(*(USBD_INTERFACE_INFORMATION_64*)tmp).AlternateSetting = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bAlternateSetting;
			(*(USBD_INTERFACE_INFORMATION_64*)tmp).Class = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceClass;
			(*(USBD_INTERFACE_INFORMATION_64*)tmp).SubClass = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceSubClass;
			(*(USBD_INTERFACE_INFORMATION_64*)tmp).Protocol = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceProtocol;
			(*(USBD_INTERFACE_INFORMATION_64*)tmp).Reserved = 0x00;
			(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle = (u_int64_t)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1);
			(*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bNumEndpoints;

			for (j = 0; j < (*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes; j++) {
				(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].wMaxPacketSize;
				(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].EndpointAddress = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bEndpointAddress;
				(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].Interval = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bInterval;
				(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[j].bmAttributes & USB_ENDPOINT_TYPE_MASK;
				(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle = (u_int64_t)0x80000000+0x1000*(tmp_config+1)+0x100*(tmp_int+1)+0x10*(tmp_alt+1)+0x1*(j+2);
				if ((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize > MAX_BUFFER_SIZE)
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize = MAX_BUFFER_SIZE;
				(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags = 0;
			}
		}
		break;	// URB_FUNCTION_SELECT_INTERFACE 0x0001

	case URB_FUNCTION_CLASS_OTHER:		// 0x001F
		length = tmpL = tmpReq = tmpReqType = 0;

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				tmpL = purb->UrbControlVendorClassRequest.TransferBufferLength;
				tmpReq = purb->UrbControlVendorClassRequest.Request;
				if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(purb->UrbBulkOrInterruptTransfer.TransferFlags)) {
					tmpReqType = (USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_OTHER);
					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					ret = usb_control_msg(udev, (USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_OTHER),
							purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);
//					PDEBUG("usb_control_msg(), direction: out, return: %d\n", ret);
				} else {
					tmpReqType = (USB_TYPE_CLASS | USB_DIR_IN | USB_RECIP_OTHER);
					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					ret = usb_control_msg(udev, (USB_TYPE_CLASS | USB_DIR_IN | USB_RECIP_OTHER),
							purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);
//					PDEBUG("usb_control_msg(), direction: in, return: %d\n", ret);
				}
				usb_close(udev);
				if (ret < 0)
					break;
				else length = ret;
			}
			else
				break;
		}
		else
			break;

		pbuf = pirp_saverw->Buffer + UrbLength;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x22;
		
		purb->UrbControlTransfer.PipeHandle = (u_int64_t)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
		purb->UrbControlTransfer.TransferFlags = 0x0a;
		purb->UrbControlTransfer.TransferBufferLength = length;
		purb->UrbControlTransfer.UrbLink = 0;
		purb->UrbControlTransfer.SetupPacket[0] = tmpReqType;
		purb->UrbControlTransfer.SetupPacket[1] = tmpReq;
		purb->UrbControlTransfer.SetupPacket[6] = (unsigned char )(tmpL&0x00ff);
		purb->UrbControlTransfer.SetupPacket[7] = (unsigned char )((tmpL&0xff00)>>8);
		pirp_saverw->BufferSize = purb->UrbControlTransfer.TransferBufferLength + purb->UrbHeader.Length;
		break;	// URB_FUNCTION_CLASS_OTHER

	case URB_FUNCTION_CLASS_INTERFACE:
		length = tmpL = tmpReq = tmpReqType = 0;
		ret = -1;

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				tmpL = purb->UrbControlVendorClassRequest.TransferBufferLength;
				tmpReq = purb->UrbControlVendorClassRequest.Request;
				if (USBD_TRANSFER_DIRECTION_OUT == USBD_TRANSFER_DIRECTION(purb->UrbBulkOrInterruptTransfer.TransferFlags)) {
/*
					PDEBUG("OUT ");
					PDEBUG("req: %x ", purb->UrbControlVendorClassRequest.Request);
					PDEBUG("value: %x ", purb->UrbControlVendorClassRequest.Value);
					PDEBUG("index: %x ", purb->UrbControlVendorClassRequest.Index);
					PDEBUG("len: %x\n", purb->UrbControlVendorClassRequest.TransferBufferLength);
*/
					if (Is_Canon && flag_canon_state != 0)
					{
						if (	purb->UrbControlVendorClassRequest.Request == 0x2 &&
							purb->UrbControlVendorClassRequest.Value == 0x0 &&
							purb->UrbControlVendorClassRequest.Index == 0x100
						)
						{
							PDEBUG("user cancel printing...\n");
							flag_canon_state = 0;
							flag_canon_ok = 0;
						}
					}

					tmpReqType = (USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_INTERFACE);
					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					ret = usb_control_msg(udev, tmpReqType, purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);
//					PDEBUG("usb_control_msg() out return: %d\n", ret);
					((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
				} else {
/*
					PDEBUG("IN ");
					PDEBUG("req: %x ", purb->UrbControlVendorClassRequest.Request);
					PDEBUG("value: %x ", purb->UrbControlVendorClassRequest.Value);
					PDEBUG("index: %x ", purb->UrbControlVendorClassRequest.Index);
					PDEBUG("len: %x\n", purb->UrbControlVendorClassRequest.TransferBufferLength);
*/
					pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;
					memset(pbuf, 0x0, purb->UrbControlVendorClassRequest.TransferBufferLength);
					tmpReqType = (USB_TYPE_CLASS | USB_DIR_IN | USB_RECIP_INTERFACE);
					ret = usb_control_msg(udev, tmpReqType, purb->UrbControlVendorClassRequest.Request,
							purb->UrbControlVendorClassRequest.Value,
							purb->UrbControlVendorClassRequest.Index, (char*)pbuf,
							purb->UrbControlVendorClassRequest.TransferBufferLength,
							timeout_control_msg);

					if (	Is_Canon &&
						purb->UrbControlVendorClassRequest.Request == 0x0 &&
						purb->UrbControlVendorClassRequest.Value == 0x0 &&
						purb->UrbControlVendorClassRequest.Index == 0x100
					)
						flag_canon_monitor = 1;
					else if (Is_Canon)
					{
						flag_canon_monitor = 0;
						((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
					}
					else if (Is_HP && ((PCONNECTION_INFO)curt_pos)->count_class_int_in < 3)	// HP Deskjet 3920 
						((PCONNECTION_INFO)curt_pos)->count_class_int_in++;

//					PDEBUG("usb_control_msg() in return: %d\n", ret);
				}
				usb_close(udev);
				if (ret < 0)
					break;	
				else length = ret;
			}
			else
				break;
		}
		else
			break;

		pbuf = pirp_saverw->Buffer + UrbLength;
		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.Function = 0x08;
		purb->UrbHeader.Status = 0;
		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x22;
		
		purb->UrbControlTransfer.PipeHandle = (u_int64_t)0x80000000+0x1000*(0+1)+0x100*(0+1)+0x10*(0+1)+0x1*1;
		purb->UrbControlTransfer.TransferFlags = 0x0b;
		purb->UrbControlTransfer.TransferBufferLength = length;
		purb->UrbControlTransfer.UrbLink = 0;
		//purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint = (u_int64_t)0xffffffff;
		//purb->UrbBulkOrInterruptTransfer.hca.HcdIrp = (u_int64_t)0xdeadf00d;
		purb->UrbControlTransfer.SetupPacket[0] = tmpReqType;
		purb->UrbControlTransfer.SetupPacket[1] = tmpReq;
		purb->UrbControlTransfer.SetupPacket[6] = (unsigned char )(tmpL&0x00ff);
		purb->UrbControlTransfer.SetupPacket[7] = (unsigned char )((tmpL&0xff00)>>8);
		pirp_saverw->BufferSize = purb->UrbControlTransfer.TransferBufferLength + purb->UrbHeader.Length;
		break;	// URB_FUNCTION_CLASS_INTERFACE

	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:	// 0x0009
		pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;

		pirp_saverw->Status = 0;
		pirp_saverw->Information = 0;
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.Status = 0;
		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
		purb->UrbHeader.UsbdFlags = 0x22;
		purb->UrbBulkOrInterruptTransfer.UrbLink = 0;
		//purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint = (u_int64_t)0xffffffff;
		//purb->UrbBulkOrInterruptTransfer.hca.HcdIrp = (u_int64_t)0xdeadf00d;
		purb->UrbControlDescriptorRequest.TransferBuffer = (u_int64_t)(u_int32_t)pbuf;

//		PDEBUG("PipeHandle: 0x%x\n", purb->UrbBulkOrInterruptTransfer.PipeHandle);
		tmp_config = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<16)>>28;
		tmp_int = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<20)>>28;
		tmp_alt = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<24)>>28;
		tmp_ep = (((u_int32_t)purb->UrbBulkOrInterruptTransfer.PipeHandle)<<28)>>28;
		tmp_config--;
		tmp_int--;
		tmp_alt--;
		tmp_ep -= 2;
//		PDEBUG("config, int, alt, ep: %x %x %x %x\n", tmp_config, tmp_int, tmp_alt, tmp_ep);

		if (dev) {
			tmp_intclass = dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].bInterfaceClass;
			if (tmp_intclass == 8) {
				timeout_bulk_read_msg_tmp = 1000;
				timeout_bulk_write_msg_tmp = 1000;
			} else if (Is_EPSON && tmp_intclass != 7)
			{
				timeout_bulk_read_msg_tmp = timeout_bulk_read_msg;
				timeout_bulk_write_msg_tmp = 30000;
			}
			else
			{
				timeout_bulk_read_msg_tmp = timeout_bulk_read_msg;
				timeout_bulk_write_msg_tmp = timeout_bulk_write_msg;
			}

			if (Is_Canon && tmp_intclass != 7)
			{
				flag_canon_state = 0;
				flag_canon_ok = 0;
			}

			udev = usb_open(dev);
			if (udev) {	
				if ((USB_ENDPOINT_TYPE_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bmAttributes) == USB_ENDPOINT_TYPE_BULK) {
					count_int_epson = 0;
					if ((USB_ENDPOINT_DIR_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress) == USB_ENDPOINT_OUT) {
						purb->UrbBulkOrInterruptTransfer.TransferFlags = 2;
						gettimeofday(&tv_ref, NULL);
						alarm(0);
						if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
							purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
						if (Is_Canon && tmp_intclass == 7)
						{
							if (flag_canon_state == 3)
							{
								PDEBUG("remain: %d\n", purb->UrbBulkOrInterruptTransfer.TransferBufferLength - data_canon_done);
								ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)(pbuf + data_canon_done), purb->UrbBulkOrInterruptTransfer.TransferBufferLength - data_canon_done, 30000);
								if (ret > 0)
								{
									ret += data_canon_done;
									data_canon_done = 0;
								}
								else
									flag_canon_ok = 0;
							}
							else
							{
								data_canon_done = 0;
								if (flag_canon_new_printer_cmd)
									ret = usb_bulk_write_sp(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 32767, &data_canon_done, 4096);
								else
									ret = usb_bulk_write_sp(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 5000, &data_canon_done, USB_Ver_N20?128:512);
								if (ret < 0 && data_canon_done > 0)
									PDEBUG("done: %d\n", data_canon_done);
							}
						}
						else if (Is_HP && (Is_HP_LaserJet || Is_HP_LaserJet_MFP) && tmp_intclass == 7)
							ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 500);
						else
							ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_write_msg_tmp);
						alarm(1);
						gettimeofday(&tv_now, NULL);

						if (Is_EPSON)
						{
							if (ret == 0 && purb->UrbBulkOrInterruptTransfer.TransferBufferLength == 2)
							{
								PDEBUG("retry...");
								ret = usb_bulk_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_write_msg_tmp);
							}
							else if (ret >= 4096)
							{
								count_bulk_read_epson = 0;
								flag_monitor_epson = 0;
							}
						}

						PDEBUG("usb_bulk_write() return: %d\n", ret);

						count_bulk_write ++;
						count_bulk_read_ret0_1 = 0;

						if (ret < 0) {
							if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
							else
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);

							if (Is_Canon && tmp_intclass == 7 && !flag_canon_new_printer_cmd)
							{
								if (flag_canon_state == 0 && ret == -ETIMEDOUT)
								{
									PDEBUG("copy data_buf !!!\n");

									flag_canon_state = 1;
									flag_canon_ok = 0;
									memcpy(data_canon, data_buf, MAX_BUF_LEN);

									usb_close(udev);
									pirp_saverw->BufferSize = purb->UrbHeader.Length;
									pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
									return 0;
								}
								else if (flag_canon_state != 1)
								{
									PDEBUG("skip...\n");
	
									usb_close(udev);
									pirp_saverw->BufferSize = purb->UrbHeader.Length;
									pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
									return 0;
								}
							}
						}
						else if (Is_Canon && tmp_intclass == 7)	// ret >= 0
						{
							flag_canon_state = 0;
							flag_canon_ok = 0;
							((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
						}
					} else {
						purb->UrbBulkOrInterruptTransfer.TransferFlags = 3;
						gettimeofday(&tv_ref, NULL);
						alarm(0);
						if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
							purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
						if (Is_HP && Is_HP_LaserJet_MFP && tmp_intclass == 7)
							ret = usb_bulk_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 500);
						else
							ret = usb_bulk_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_read_msg_tmp);
						alarm(1);
						gettimeofday(&tv_now, NULL);

						if (Is_Canon && tmp_intclass == 7)
						{
							if (ret > 64 && ret < 256 && memcmp(pbuf+2, "BST:", 4) == 0)
							{
								if (flag_canon_monitor == 1 && ((PCONNECTION_INFO)curt_pos)->count_class_int_in < 3)
								{
									flag_canon_monitor = 0;
									((PCONNECTION_INFO)curt_pos)->count_class_int_in++;

									PDEBUG("mon: [%d] ", ((PCONNECTION_INFO)curt_pos)->count_class_int_in);
								}

								if (*(pbuf+7) == '0')			// ok
									err_canon = 0;
								else if (*(pbuf+7) == '8')		// operation call
									err_canon = 1;
								else					// unknown error
									err_canon = 2;

//								if (flag_canon_state == 1 && err_canon != 0)
								if (flag_canon_state == 1 && err_canon == 1)
								{					// "out of paper"
									flag_canon_state = 2;
									flag_canon_ok = 0;
								}
//								else if (flag_canon_state == 1 && err_canon == 0 && ++flag_canon_ok > 5)
								else if (flag_canon_state == 1 && err_canon != 1 && ++flag_canon_ok > 5)
									flag_canon_state = 3;		// warm-up
//								else if (flag_canon_state == 2 && err_canon == 0)
								else if (flag_canon_state == 2 && err_canon != 1)
								{					// "out of paper" => "ok"
									flag_canon_state = 3;
									flag_canon_ok = 0;
								}
//								else if (flag_canon_state == 3 && err_canon == 0)
								else if (flag_canon_state == 3 && err_canon != 1)
									flag_canon_ok++;	

								PDEBUG("status:[%s] state:[%d] ok:[%d] ", usblp_messages_canon[err_canon], flag_canon_state, flag_canon_ok);
							}
							else if (!flag_canon_new_printer_cmd && ret > 256 && memcmp(pbuf+2, "xml ", 4) == 0)
							{
								PDEBUG("found new printer cmd!\n");
								flag_canon_new_printer_cmd = 1;
							}
						}
						else
						{
							flag_canon_monitor = 0;
							((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;
						}

						PDEBUG("usb_bulk_read() return: %d\n", ret);

						if (Is_HP) {	// HP, if this connection only includes HP ToolBox
							if (ret == 0) {
								count_bulk_read_ret0_1 ++;
								if (count_bulk_read_ret0_1 >= 30) {
									count_bulk_write = 0;
									count_bulk_read = 0;
									count_bulk_read_ret0_1 = 0;
									count_bulk_read_ret0_2 = 0;
								}
								if (Is_HP_LaserJet) {						// laserjet 33xx series
									if (count_bulk_write == 1 && count_bulk_read == 1)
										count_bulk_read_ret0_2 ++;
								}
								else if (Is_HP_OfficeJet) {					// officejet 56xx series
									if (count_bulk_write == 2 && count_bulk_read == 2)
										count_bulk_read_ret0_2 ++;
								}
							}
							else if (ret > 0) {
								count_bulk_read ++;
								count_bulk_read_ret0_1 = 0;
							}
						}
						else if (Is_EPSON) {
							if (ret == 9 && count_bulk_read_epson == 0 && memcmp(EPSON_BR_STR1_09, pbuf, 9) == 0)
								count_bulk_read_epson = 1;
							else if (ret == 19) {
								if (count_bulk_read_epson == 1 && memcmp(EPSON_BR_STR2_19, pbuf, 19) == 0)
									count_bulk_read_epson = 2;
								else if (count_bulk_read_epson == 2 && memcmp(EPSON_BR_STR3_19, pbuf, 19) == 0)
									count_bulk_read_epson = 3;
							}
						}

						if (ret < 0) {
							count_bulk_read_epson = -1;
							flag_monitor_epson = 0;

							if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
							else
								PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);
						}

/*						if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength != 0 && ret == 0)
							pirp_saverw->Status = 0xC0000004;	// STATUS_INFO_LENGTH_MISMATCH
						else if (ret < 0)
							pirp_saverw->Status = 0xC00000AE;	// STATUS_PIPE_BUSY
*/
					}
				} else {
					PDEBUG("USB_ENDPOINT_TYPE_INTERRUPT\n");

					if (!Is_EPSON)
					{
						usb_close(udev);
						pirp_saverw->BufferSize = purb->UrbHeader.Length;
						pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
						return 0;
					}
					else
					{
						if ((USB_ENDPOINT_DIR_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress) == USB_ENDPOINT_OUT) {
							PDEBUG("USB_ENDPOINT_OUT\n");
	
							purb->UrbBulkOrInterruptTransfer.TransferFlags = 2;
	
							gettimeofday(&tv_ref, NULL);
							alarm(0);
							if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
								purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
							ret = usb_interrupt_write(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_write_msg_tmp);
							alarm(1);
							gettimeofday(&tv_now, NULL);
	
							PDEBUG("usb_interrupt_write() return: %d\n", ret);
	
							if (ret < 0) {
								if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
								else
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);
							}
						} else {
							PDEBUG("USB_ENDPOINT_IN\n");
	
							purb->UrbBulkOrInterruptTransfer.TransferFlags=3;
	
							gettimeofday(&tv_ref, NULL);
							alarm(0);
							if (purb->UrbBulkOrInterruptTransfer.TransferBufferLength > MAX_BUFFER_SIZE)
								purb->UrbBulkOrInterruptTransfer.TransferBufferLength = MAX_BUFFER_SIZE;
//							ret = usb_interrupt_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, timeout_bulk_read_msg_tmp);
							if (count_int_epson)
								ret = usb_interrupt_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 5000);
							else								
								ret = usb_interrupt_read(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress, (char*)pbuf, purb->UrbBulkOrInterruptTransfer.TransferBufferLength, 1);
							alarm(1);
							gettimeofday(&tv_now, NULL);

							PDEBUG("usb_interrupt_read() return: %d\n", ret);

							if (ret < 0) {
								if ((tv_now.tv_usec - tv_ref.tv_usec) >= 0)
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec, (tv_now.tv_usec-tv_ref.tv_usec)/1000);
								else
									PDEBUG("sec: %ld, msec: %ld\n", tv_now.tv_sec-tv_ref.tv_sec-1, (1000000+tv_now.tv_usec-tv_ref.tv_usec)/1000);
							}
						}

						if (!count_int_epson)
							count_int_epson = 1;
					}
					
				}

				if (ret < 0) {
					ret = 0;
					count_err++;
				} else
					count_err = 0;

				if (count_err > 255)
					pirp_saverw->Status = 0xC00000AD;	// STATUS_INVALID_PIPE_STATE

				usb_close(udev);
			}
			else
				ret = 0;
		}
		else
			ret = 0;

		purb->UrbBulkOrInterruptTransfer.TransferBufferLength = ret;

		if ((USB_ENDPOINT_DIR_MASK & dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress) == USB_ENDPOINT_OUT)
			pirp_saverw->BufferSize = purb->UrbHeader.Length;
		else
			pirp_saverw->BufferSize = purb->UrbHeader.Length + purb->UrbBulkOrInterruptTransfer.TransferBufferLength;
		break;	// URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER 0x0009

	case URB_FUNCTION_ABORT_PIPE:
		PDEBUG("\n\t URB_FUNCTION_ABORT_PIPE");

		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;	// according to packet
		purb->UrbHeader.UsbdFlags = 0x10;

//		PDEBUG("\n\t PipeHandle: 0x%x", purb->UrbPipeRequest.PipeHandle);
		tmp_config = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<16)>>28;
		tmp_int = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<20)>>28;
		tmp_alt = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<24)>>28;
		tmp_ep = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<28)>>28;
		tmp_config--;
		tmp_int--;
		tmp_alt--;
		tmp_ep -= 2;
//		PDEBUG("\n\t config, int, alt, ep: %lld %lld %lld %lld in Dec", tmp_config, tmp_int, tmp_alt, tmp_ep);

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				usb_clear_halt(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress);
				usb_close(udev);
			}
		}
		break;

	case URB_FUNCTION_RESET_PIPE:
		PDEBUG("\n\t URB_FUNCTION_RESET_PIPE");

		pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

		purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;	// according to packet
		purb->UrbHeader.UsbdFlags = 0x10;

//		PDEBUG("\n\t PipeHandle: 0x%x", purb->UrbPipeRequest.PipeHandle);
		tmp_config = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<16)>>28;
		tmp_int = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<20)>>28;
		tmp_alt = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<24)>>28;
		tmp_ep = (((u_int32_t)purb->UrbPipeRequest.PipeHandle)<<28)>>28;
		tmp_config--;
		tmp_int--;
		tmp_alt--;
		tmp_ep -= 2;
//		PDEBUG("\n\t config, int, alt, ep: %lld %lld %lld %lld in Dec", tmp_config, tmp_int, tmp_alt, tmp_ep);

		if (dev) {
			udev = usb_open(dev);
			if (udev) {
				usb_resetep(udev, dev->config[tmp_config].interface[tmp_int].altsetting[tmp_alt].endpoint[tmp_ep].bEndpointAddress);
				usb_close(udev);
			}
		}
		break;

	default:
		PDEBUG("\n !!! Unsupported URB_FUNCTION: 0x%x\n\n", purb->UrbHeader.Function);
		break;
	} // end of switch urb function

	pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;
	return 1;

} // end of handleURB64

/*
 * Handle urbs in irp_saverw.Buffer for free (non-busy) clients
 */
void handleURB_fake(PIRP_SAVE pirp_saverw)
{
	PURB		purb;
	unsigned char*	pbuf;

	purb = (PURB)pirp_saverw->Buffer;
	pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;

	pirp_saverw->Status = 0;
	pirp_saverw->Information = 0;
	pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
	pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
	pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
	pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

	purb->UrbHeader.Status = 0;
	purb->UrbHeader.UsbdDeviceHandle = (void*)0x80000000;
	purb->UrbHeader.UsbdFlags = 0x22;
	purb->UrbBulkOrInterruptTransfer.UrbLink = 0;
	purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint = (void*)0xffffffff;
	purb->UrbBulkOrInterruptTransfer.hca.HcdIrp = (void*)0xdeadf00d;
	purb->UrbControlDescriptorRequest.TransferBuffer = pbuf;

	purb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
	pirp_saverw->BufferSize = purb->UrbHeader.Length;
	pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;
	pirp_saverw->Status = 0xC00000AE;	// STATUS_PIPE_BUSY
}

/*
 * Handle urbs in irp_saverw.Buffer for free (non-busy) clients
 */
void handleURB_64_fake(PIRP_SAVE pirp_saverw)
{
	PURB_64		purb;
	unsigned char*	pbuf;

	purb = (PURB_64)pirp_saverw->Buffer;
	pbuf = pirp_saverw->Buffer + purb->UrbHeader.Length;

	pirp_saverw->Status = 0;
	pirp_saverw->Information = 0;
	pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
	pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
	pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
	pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;

	purb->UrbHeader.Status = 0;
	purb->UrbHeader.UsbdDeviceHandle = (u_int64_t)0x80000000;
	purb->UrbHeader.UsbdFlags = 0x22;
	purb->UrbBulkOrInterruptTransfer.UrbLink = 0;
	purb->UrbControlDescriptorRequest.TransferBuffer = (u_int64_t)(u_int32_t)pbuf;

	purb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
	pirp_saverw->BufferSize = purb->UrbHeader.Length;
	pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;
	pirp_saverw->Status = 0xC00000AE;	// STATUS_PIPE_BUSY
}

/*
 * Do actions according to MJ function and MN function.
 */
int exchangeIRP(PIRP_SAVE pirp_saver, PIRP_SAVE pirp_saverw, struct u2ec_list_head *curt_pos)
{
	WCHAR		wcbuf[256];
	char		pbuf[256];
	int		i, n, ret;

	bzero(wcbuf, sizeof(wcbuf));
	bzero(pbuf, 256);

	if (pirp_saver->StackLocation.MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL)
		((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;

	switch(pirp_saver->StackLocation.MajorFunction) {
	case IRP_MJ_PNP:
		switch (pirp_saver->StackLocation.MinorFunction) {
		case IRP_MN_QUERY_ID:				//13
			switch (pirp_saver->StackLocation.Parameters.Others.Argument1) {
			case 0x0:
				if (dev)
					sprintf(pbuf, "USB\\Vid_%04x&Pid_%04x", dev->descriptor.idVendor, dev->descriptor.idProduct);
				else break;

				for (n = 0; n < 23; n++)
#if __BYTE_ORDER == __LITTLE_ENDIAN
					wcbuf[n] = (WCHAR)*(pbuf + n);
#else
					wcbuf[n] = SWAP_BACK16((WCHAR)*(pbuf + n));
#endif
				memcpy(pirp_saverw->Buffer, wcbuf, 2*n);
				
				pirp_saverw->BufferSize = 2*n;
				pirp_saverw->Status = 0;
				pirp_saverw->Information = ((u_int64_t)(unsigned int)pirp_saverw->Buffer)<<32;
				pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
				break;

			case 0x1:
			case (u_int64_t)4294967296://0x00100000:
				if (dev) {					
					sprintf(pbuf, "USB\\Vid_%04x&Pid_%04x&Rev_%04x",
						dev->descriptor.idVendor, dev->descriptor.idProduct, dev->descriptor.bcdDevice);
					sprintf(pbuf+strlen(pbuf)+1, "USB\\Vid_%04x&Pid_%04x",
						dev->descriptor.idVendor, dev->descriptor.idProduct);
				}
				else break;
			
				for (n = 0; n < 54; n++)
#if __BYTE_ORDER == __LITTLE_ENDIAN
					wcbuf[n] = (WCHAR)*(pbuf + n);
#else
					wcbuf[n] = SWAP_BACK16((WCHAR)*(pbuf + n));
#endif
				memcpy(pirp_saverw->Buffer, wcbuf, 2*n);
				
				pirp_saverw->BufferSize = 2*n ;
				pirp_saverw->Status = 0;
				pirp_saverw->Information = ((u_int64_t)(unsigned int)pirp_saverw->Buffer)<<32;
				pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
				pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;
				break;

			case 0x2:
			case (u_int64_t)8589934592://0x00020000:
				if (dev) {
					if (dev->config[0].bNumInterfaces > 1) 	{
						strcpy(pbuf, "USB\\DevClass_00&SubClass_00&Prot_00");	//pbuf 0~35
						strcpy(pbuf + 36, "USB\\DevClass_00&SubClass_00");	//pbuf 36~63 
						strcpy(pbuf + 64, "USB\\DevClass_00");			//pbuf 64~79
						strcpy(pbuf + 80, "USB\\COMPOSITE");			//pbuf 80~93
						*(pbuf + 94) = 0;
					} else {	// ex: "USB\\Class_07&SubClass_01&Prot_02 USB\\Class_07&SubClass_01 USB\\Class_07"
						sprintf(pbuf, "USB\\Class_%02x&SubClass_%02x&Prot_%02x",
							dev->config[0].interface[0].altsetting[0].bInterfaceClass,
							dev->config[0].interface[0].altsetting[0].bInterfaceSubClass,
							dev->config[0].interface[0].altsetting[0].bInterfaceProtocol);	//pbuf 0~32
						sprintf(pbuf + 33, "USB\\Class_%02x&SubClass_%02x",
							dev->config[0].interface[0].altsetting[0].bInterfaceClass,
							dev->config[0].interface[0].altsetting[0].bInterfaceSubClass);	//pbuf 33~57
						sprintf(pbuf + 58, "USB\\Class_%02x",
							dev->config[0].interface[0].altsetting[0].bInterfaceClass);	//pbuf 58~70
						*(pbuf + 71) = 0;
					}
				}
				else break;

				if (!(dev->config[0].bNumInterfaces > 1))
					i = 72;
				else
					i = 95;

				for (n = 0; n < i; n++)
#if __BYTE_ORDER == __LITTLE_ENDIAN
					wcbuf[n] = (WCHAR)*(pbuf + n);
#else
					wcbuf[n] = SWAP_BACK16((WCHAR)*(pbuf + n));
#endif
				memcpy(pirp_saverw->Buffer, wcbuf, 2*n);
				
				pirp_saverw->BufferSize = 2*n;
				pirp_saverw->Status = 0;
				pirp_saverw->Information = ((u_int64_t)(unsigned int)pirp_saverw->Buffer)<<32; 
				pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
				pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;
				break;

			case 0x3:
			case (u_int64_t)12884901888://0x00030000:
				if (dev) {
					ret = 0;
					udev = usb_open(dev);
					if (udev) {
						if (dev->descriptor.iSerialNumber)
							ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber, pbuf, 256);
						else
						{
							*pbuf = 0x31;
							ret = 1;
						}
						ret++;
						usb_close(udev);
					}
				}
				else break;
			
				for (n = 0; n < ret; n++)
#if __BYTE_ORDER == __LITTLE_ENDIAN
					wcbuf[n] = *(pbuf + n);
#else
					wcbuf[n] = SWAP_BACK16(*(pbuf + n));
#endif
				memcpy(pirp_saverw->Buffer, wcbuf, 2*n);
				
				pirp_saverw->BufferSize = 2*n;
				pirp_saverw->Status = 0;
				pirp_saverw->Information = ((u_int64_t)(unsigned int)pirp_saverw->Buffer)<<32;
				pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
				pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;
				break;
			
			default:
				PDEBUG("IRP_MN_QUERY_ID: argument not supported.\n");
				return -1;
				break;
			}
			break;//end of query id

		case IRP_MN_QUERY_DEVICE_TEXT:			// 0c
			switch (pirp_saver->StackLocation.Parameters.Others.Argument1) {
			case 0x0:
				if (dev) {
					ret = 0;
					udev = usb_open(dev);
					if (udev) {
						if (dev->descriptor.iProduct)
							ret = usb_get_string_simple(udev, dev->descriptor.iProduct, pbuf, 256);
						ret++;
						usb_close(udev);
					}
				}
				else break;

				for (n = 0; n < ret; n++)
#if __BYTE_ORDER == __LITTLE_ENDIAN
					wcbuf[n] = *(pbuf + n);
#else
					wcbuf[n] = SWAP_BACK16(*(pbuf + n));
#endif
				memcpy(pirp_saverw->Buffer, wcbuf, 2*n);
				
				pirp_saverw->BufferSize = 2*n;
				pirp_saverw->Status = 0;
				pirp_saverw->Information = ((u_int64_t)(unsigned int)pirp_saverw->Buffer)<<32;
				pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
				pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;
				break;

			case 0x1:
			case 0x100000000:
//			case (u_int64_t)4294967296://0x00001000:
				if (dev) {
					ret=0;
					udev = usb_open(dev);
					if (udev) {
						if (dev->descriptor.iProduct)
							ret = usb_get_string_simple(udev, dev->descriptor.iProduct, pbuf, 256);
						ret++;
						usb_close(udev);
					}
				}
				else break;

				for (n = 0; n < ret; n++)
#if __BYTE_ORDER == __LITTLE_ENDIAN
					wcbuf[n] = *(pbuf + n);
#else
					wcbuf[n] = SWAP_BACK16(*(pbuf + n));
#endif
				memcpy(pirp_saverw->Buffer, wcbuf, 2*n);
				
				pirp_saverw->BufferSize = 2*n;
				pirp_saverw->Status = 0;
				pirp_saverw->Information = ((u_int64_t)(unsigned int)pirp_saverw->Buffer)<<32;
				pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
				pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
				pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;
				break;
			
			default:
				PDEBUG("IRP_MN_QUERY_DEVICE_TEXT: argument not supported.\n");
				return -1;
			}
			break;	//end of query device text
		
		case IRP_MN_QUERY_CAPABILITIES:			// 09
			*pbuf = 0x40;
			*(pbuf + 2) = 0x01;
			*(pbuf + 4) = 0x50;
//			*(pbuf + 8) = 0x02;	// for Lexmark X5470
			*(pbuf + 8) = 0x01;	// for HP Officejet 5610, EPSON TX200
			*(pbuf + 20) = 0x01;
			*(pbuf + 24) = 0x04;
			*(pbuf + 28) = 0x04;
			*(pbuf + 32) = 0x04;
			*(pbuf + 36) = 0x04;
			*(pbuf + 40) = 0x04;
//			*(pbuf + 44) = 0x04;	// for Lexmark X5470, HP Officejet 5610
			*(pbuf + 44) = 0x01;	// for EPSON TX200
			*(pbuf + 48) = 0x01;

			n = 64;
			memcpy(pirp_saverw->Buffer, pbuf, n);
			pirp_saverw->BufferSize = n;
			pirp_saverw->Status = 0;
			pirp_saverw->Information = 0;
			pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize;
			pirp_saverw->StackLocation.Parameters.Others.Argument1 = (u_int64_t)0;
			pirp_saverw->StackLocation.Parameters.Others.Argument2 = (u_int64_t)0;
			pirp_saverw->StackLocation.Parameters.Others.Argument3 = (u_int64_t)0;
			pirp_saverw->StackLocation.Parameters.Others.Argument4 = (u_int64_t)0;					
			break;

		case 0x18:
		case IRP_MN_QUERY_RESOURCES:			// 0a
		case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:	// 0x0D
			pirp_saverw->BufferSize = 0;
			pirp_saverw->Information = 0;
			pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;	// 104
			break;

		case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:	// 0b
		case IRP_MN_START_DEVICE:			// 0x00
			pirp_saverw->BufferSize = 0;
			pirp_saverw->Status = 0;
			pirp_saverw->Information = 0;
			pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;	// 104
			break;

		case IRP_MN_QUERY_PNP_DEVICE_STATE:		// 0x14
			pirp_saverw->Status = 0;
			pirp_saverw->Information = 0;
//			pirp_saverw->Reserv = 0x50;
			break;

		case IRP_MN_QUERY_DEVICE_RELATIONS:		// 0x07
			pirp_saverw->Status = 0;
			pirp_saverw->Information = 0;
			break;

		case IRP_MN_QUERY_BUS_INFORMATION:		// 15
			*pbuf = 0xbc;
			*(pbuf + 1) = 0xeb;
			*(pbuf + 2) = 0x7d;
			*(pbuf + 3) = 0x9d;
			*(pbuf + 4) = 0x5d;
			*(pbuf + 5) = 0xc8;
			*(pbuf + 6) = 0xd1;
			*(pbuf + 7) = 0x11;
			*(pbuf + 8) = 0x9e;
			*(pbuf + 9) = 0xb4;
			*(pbuf + 10) = 0x00;
			*(pbuf + 11) = 0x60;
			*(pbuf + 12) = 0x08;
			*(pbuf + 13) = 0xc3;
			*(pbuf + 14) = 0xa1;
			*(pbuf + 15) = 0x9a;
			*(pbuf + 16) = 0x0f;

			n = 24;
			memcpy(pirp_saverw->Buffer, pbuf, n);

			pirp_saverw->BufferSize = n ;
			pirp_saverw->Status = 0;
			pirp_saverw->Information = ((u_int64_t)(unsigned int)pirp_saverw->Buffer)<<32;
			pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;
			break;

		case IRP_MN_QUERY_INTERFACE:	// 0x08
			PDEBUG("\n\t IRP_MN_QUERY_INTERFACE");	// for Lexmark X5470
			pirp_saverw->BufferSize = 0;
			pirp_saverw->Status = 0xC0000022;
			pirp_saverw->Information = 0;
			pirp_saverw->NeedSize = sizeof(IRP_SAVE) + pirp_saverw->BufferSize ;	// 104
			break;

		default:
			PDEBUG("Mirnor function not supported.\n");
		}//end of switch MN
		break;

	case IRP_MJ_INTERNAL_DEVICE_CONTROL:			// 0x0f
		// handle urbs in irp_saverw.Buffer
		if (pirp_saver->Is64 == 0) {
			if (pirp_saver->NeedSize < sizeof(IRP_SAVE) - 8 + sizeof(struct _URB_HEADER))
				return 1;
			if (handleURB(pirp_saverw, (struct u2ec_list_head *)curt_pos) != 1) {
				PDEBUG("HandleURB not successed.\n");
				return 0;
			}
		} else {
			if (pirp_saver->NeedSize < sizeof(IRP_SAVE) - 8 + sizeof(struct _URB_HEADER_64))
				return 1;
			if (handleURB_64(pirp_saverw, (struct u2ec_list_head *)curt_pos) != 1) {
				PDEBUG("HandleURB_64 not successed.\n");
				return 0;
			}
		}
		break;

	default:
		PDEBUG("\n!!! Unsupported IRP major: %x\n", pirp_saver->StackLocation.MinorFunction);
	}//end of switch MJ

#if defined(U2EC_DEBUG)
	if (pirp_saver->StackLocation.MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL)
		PDEBUG("\n");
#endif

	return 1;
}//end of exchangeIRP

/*
 * Initialize a Datagram listener
 * for find_all.
 */
int dgram_sock_init(int port)
{
	int			sockfd;
	struct sockaddr_in	serv_addr;
	int			broadcast = 1;;
	int			yes = 1;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		PERROR("socket");
		return -1;
	}

#ifdef	SUPPORT_LPRng
	struct ifreq		ifr;
	bzero((char *)&ifr, sizeof(ifr));
#if __BYTE_ORDER == __LITTLE_ENDIAN
	strncpy(ifr.ifr_name, nvram_safe_get("lan_ifname"), strlen(nvram_safe_get("lan_ifname")));
#else
	strncpy(ifr.ifr_name, "br0", strlen("br0"));
#endif

	if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr)) < 0) {
		perror("setsockopt: bind to device");
		return -1;
	}
#endif

	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast, sizeof(broadcast)) < 0) {
		perror("setsockopt: bin to broadcast");
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		PERROR("setsockopt");
		return -1;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(port);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind");
		return -1;
	}

	return sockfd;
}

/*
 * Initialize a stream listener
 * for add_remote_device and establish_connection.
 */
int stream_sock_init(int port)
{
	int			sockfd;
	struct sockaddr_in	serv_addr;
	int			yes = 1;
	int			no_delay = 1;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		PERROR("socket");
		return -1;
	}

	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) < 0) {
		PERROR("setsockopt");
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		PERROR("setsockopt");
		return -1;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(port);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		PERROR("bind");
		return -1;
	}

	if (listen(sockfd, BACKLOG) < 0) {
		PERROR("listen");
		return -1;
	}

	return sockfd;
}

/*
 * Response GETIP GETCONFIG and GETNAME from client.
 */
void add_remote_device()
{
	int			find_fd, conf_fd, new_fd;
	fd_set			master_fds, read_fds;
	int			fdmax, ret;
	struct sockaddr_in	cli_addr;
	int			addr_len = sizeof(cli_addr);
	char			message[192];

	FD_ZERO(&master_fds);
	FD_ZERO(&read_fds);

	/* The listener for find all. */
	if ((find_fd = dgram_sock_init(uTcpUdp)) < 0) {
		PERROR("find all: can't bind to any address\n");
		return;
	}

	/* The listener for get config & get name. */
	if ((conf_fd = stream_sock_init(uTcpPortConfig)) < 0) {
		PERROR("get config and name: can't bind to any address\n");
		return;
	}

	FD_SET(find_fd, &master_fds);
	FD_SET(conf_fd, &master_fds);
	fdmax = find_fd > conf_fd ? find_fd : conf_fd;

	for (;;) {
		read_fds = master_fds;
		while ((ret = select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) && (errno == EINTR));
		if (ret == -1) {
			PERROR("select");
			return;
		}

		/* handle GETIP. */
		if (FD_ISSET(find_fd, &read_fds)) {
			bzero(message, 192);
			if ((recvfrom(find_fd, message, 5, 0, (struct sockaddr*)&cli_addr, &addr_len) == 5) &&
			    (strncmp(message, "GETIP", 5) == 0)) {
				cli_addr.sin_port = htons(uTcpUdp+1);	
				if (sendto(find_fd, message, 5, 0, (struct sockaddr*)&cli_addr, addr_len) < 0)
					perror("sendto");
			}
		}
		/* handle GETCONFIG & GETNAME. */
		if (FD_ISSET(conf_fd, &read_fds)) {
			if ((new_fd = accept(conf_fd, (struct sockaddr *)&cli_addr, &addr_len)) < 0) {
				PERROR("accept");
				continue;
			}
	
			bzero(message, 192);
			if ((ret = recv(new_fd, message, 9, 0)) <= 0) {
				PERROR("recv");
				goto next;
			}
			message[ret] = '\0';
			PSNDRECV("add_remote_device from fd: %d received: %s\n", new_fd, message);
			if (strncmp(message, "GETCONFIG", 9) == 0) {
				sprintf(message, "RootHub#1|Port1:%d!", uTCPUSB);
				if (send(new_fd, message, strlen(message), 0) < 0) {
					PERROR("send");
					goto next;
				}
				PSNDRECV("add_remote_device GETCONFIG sent: %s\n", message);
			}
			else if (strncmp(message, "GETNAME", 7) == 0) {
				if (dev)
					sprintf(message, "RootHub#1|Port1(%s)!", str_product);
				else
					sprintf(message, "RootHub#1|Port1!");
				if (send(new_fd, message, strlen(message), 0) < 0) {
					PERROR("send");
					goto next;
				}
				PSNDRECV("add_remote_device GETNAME sent: %s\n", message);
			}
			else
				PDEBUG("get: unknown command:%s\n", message);
next:
			usleep(100000);
			close(new_fd);
		}
	}
}

/*
 * Synchronize nvram "MFP_busy" as a flag.
 * @param	state	MFP_GET_STATE: get the flag from nvram, othere: set state to nvram.
 * @return	value of get from nvram or set to nvram.
 */
int MFP_state(int state)
{
	static int MFP_is_busy = MFP_IS_IDLE;
	if (state == MFP_GET_STATE) {
#ifdef	SUPPORT_LPRng
		char *value = nvram_safe_get("MFP_busy");
		if (strcmp(value, "0") == 0)
			return MFP_is_busy = MFP_IS_IDLE;
		else if (strcmp(value, "1") == 0)
			return MFP_is_busy = MFP_IN_LPRNG;
		else if (strcmp(value, "2") == 0)
			return MFP_is_busy = MFP_IN_U2EC;
		else
#endif
			return MFP_is_busy;
	} else {
#ifdef	SUPPORT_LPRng
		if (state == MFP_IS_IDLE)
			nvram_set("MFP_busy", "0");
		else if (state == MFP_IN_U2EC)
			nvram_set("MFP_busy", "2");
#if __BYTE_ORDER == __BIG_ENDIAN
		nvram_commit();
#endif
#endif
		return MFP_is_busy = state;
	}
	return -1;
}

/*
 * Handle exceptional device.
 * Set the connection state to CONN_IS_WAITING, move it to tail, and then reset the connection.
 * @return	0: nothing, 1: force to ignore the pkg.
 */
int except(struct u2ec_list_head *curt_pos, PIRP_SAVE pirp_save, int flag_test)
{
	PURB		purb = (PURB)pirp_save->Buffer;
	PURB_64		purb_64 = (PURB_64)pirp_save->Buffer;
	static time_t	toolbox_time = 0;
	static u_long	except_ip = 0;
	static u_long	except_ip_epson = 0;

	if (Is_HP) {
		if (except_ip == ((PCONNECTION_INFO)curt_pos)->ip.s_addr && time((time_t*)NULL) - toolbox_time < 2) {
			((PCONNECTION_INFO)curt_pos)->busy = CONN_IS_WAITING;
			return 1;
		}
	
		if (count_bulk_read_ret0_2 >= 29) {
			count_bulk_read = 0;
			count_bulk_write = 0;
			if (Is_HP_LaserJet) {			// HP LaserJet 33xx series
				PDEBUG("HP LaserJet 3390 minotor pkt: detecting...\n");
				except_ip = ((PCONNECTION_INFO)curt_pos)->ip.s_addr;
				toolbox_time = time((time_t*)NULL);
	
				if (!flag_test)
				{
					((PCONNECTION_INFO)curt_pos)->busy = CONN_IS_WAITING;
					u2ec_list_del(curt_pos);
					u2ec_list_add_tail(curt_pos, &conn_info_list);
	
					int u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
					write(u2ec_fifo, "c", 1);
					close(u2ec_fifo);
				}
	
				return 1;
			}
			else if (Is_HP_OfficeJet) {		// HP OfficeJet 56xx series
				count_bulk_read_ret0_1 = 0;
				count_bulk_read_ret0_2 = 0;
				return 3;
			}
		}
	}

	if (	pirp_save->StackLocation.MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
		(	(	pirp_save->Is64 == 0 &&
				purb->UrbHeader.Function == URB_FUNCTION_CLASS_INTERFACE &&
				USBD_TRANSFER_DIRECTION(purb->UrbBulkOrInterruptTransfer.TransferFlags) == USBD_TRANSFER_DIRECTION_IN
			)	||
			(	pirp_save->Is64 == 1 &&
				purb_64->UrbHeader.Function == URB_FUNCTION_CLASS_INTERFACE &&
				USBD_TRANSFER_DIRECTION(purb_64->UrbBulkOrInterruptTransfer.TransferFlags) == USBD_TRANSFER_DIRECTION_IN
			)
		)
	) {
		if (((PCONNECTION_INFO)curt_pos)->count_class_int_in >= 3) {
			if (Is_HP)	// HP DeskJet 39xx series
			{
				PDEBUG("bypass class int. in pkt!\n");
				return 4;
			}
			else if (Is_Canon)
			{
				PDEBUG("Canon minotor pkt: got it!\n");
				flag_canon_monitor = 0;
				((PCONNECTION_INFO)curt_pos)->count_class_int_in = 0;

				if (!flag_test)
				{
					((PCONNECTION_INFO)curt_pos)->busy = CONN_IS_WAITING;
					u2ec_list_del(curt_pos);
					u2ec_list_add_tail(curt_pos, &conn_info_list);

					int u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
					write(u2ec_fifo, "c", 1);
					close(u2ec_fifo);
				}

				return 1;
			}
		}

		if (Is_EPSON && count_bulk_read_epson == 3) {
			if (((PCONNECTION_INFO)curt_pos)->busy != CONN_IS_BUSY)
				return 0;
			if (except_ip_epson != ((PCONNECTION_INFO)curt_pos)->ip.s_addr) {
				except_ip_epson = ((PCONNECTION_INFO)curt_pos)->ip.s_addr;
				count_bulk_read_epson = 0;
				flag_monitor_epson = -1;
			}
			PDEBUG("EPSON minotor pkt: detecting...\n");
			if (++flag_monitor_epson == 4) {
				PDEBUG("EPSON minotor pkt: got it!\n");
				count_bulk_read_epson = 0;
				flag_monitor_epson = 0;

				if (!flag_test)
				{
					((PCONNECTION_INFO)curt_pos)->busy = CONN_IS_WAITING;
					u2ec_list_del(curt_pos);
					u2ec_list_add_tail(curt_pos, &conn_info_list);

					int u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
					write(u2ec_fifo, "c", 1);
					close(u2ec_fifo);
				}

				return 1;
			}
		} else
			count_bulk_read_epson = 0;
	}
	else if ( 	(Is_HP || Is_Canon) &&
			pirp_save->StackLocation.MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
			(	(pirp_save->Is64 == 0 && purb->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER) ||
				(pirp_save->Is64 == 1 && purb_64->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
			)
	)	return 2;

	return 0;
}

/*
 * Response USB connections from client.
 * TCP protocol.
 */
int usb_connection(int sockfd)
{
	TCP_PACKAGE		tcp_pack;
	PIRP_SAVE		pirp_save, pirp_saverw;
	PIRP_SAVE_SWAP		pirp_save_swap;
	PCONNECTION_INFO	conn_curt, conn_busy;
	struct u2ec_list_head	*pos;
	int			i, j, except_flag = 0, except_flag_1client = 0, u2ec_fifo;
#if __BYTE_ORDER == __BIG_ENDIAN
	PURB		purb;
	PURB_64		purb_64;

	char *tmp;
	int tmp_len;
#endif

	if (!sockfd || !fd_in_use[sockfd])
	{
		PDEBUG("closed connection, return\n");
		return -1;
	}

	conn_curt = conn_busy = (PCONNECTION_INFO)&conn_info_list;
	u2ec_list_for_each(pos, &conn_info_list) {
		if (((PCONNECTION_INFO)pos)->sockfd == sockfd)
			conn_curt = (PCONNECTION_INFO)pos;
		if (((PCONNECTION_INFO)pos)->busy == CONN_IS_BUSY)
			conn_busy = (PCONNECTION_INFO)pos;
	}

	if (conn_curt->ip.s_addr == 0)
	{
		PDEBUG("empty connection, ip:%s\n", inet_ntoa(conn_curt->ip));
		goto CLOSE_CONN;
	}

	if (!dev)
	{
		PDEBUG("no device\n");
		goto CLOSE_CONN;
	}

	/* Recv tcp_pack and close the socket if necessary. */
	bzero(&tcp_pack, sizeof(tcp_pack));
	if ((i = recv(sockfd, (char *)&tcp_pack, sizeof(tcp_pack), 0)) == sizeof(tcp_pack)) {
#if __BYTE_ORDER == __BIG_ENDIAN
		tcp_pack.Type = SWAP32(tcp_pack.Type);
		tcp_pack.SizeBuffer = SWAP32(tcp_pack.SizeBuffer);
#endif
		PSNDRECV("\nReceived from fd: %d tcp_pack: type %d , size %d.\n", sockfd, tcp_pack.Type, tcp_pack.SizeBuffer);
	} else {	// got error or connection closed by client
		if (i < 0)
			PERROR("recv");
		else if (i == 0) // connection closed
			PDEBUG("usb_connection: socket %d hung up\n", sockfd);
		goto CLOSE_CONN;
	}

	switch (tcp_pack.Type) {
	case ControlPing:
		break;
	case IrpToTcp:
		bzero(data_buf, MAX_BUF_LEN);
		pirp_save = (PIRP_SAVE)data_buf;
		pirp_save_swap = (PIRP_SAVE)data_buf;
		for (i = 0, j = 0; i < tcp_pack.SizeBuffer; i += j)
			j = recv(sockfd, (char *)pirp_save + i, tcp_pack.SizeBuffer - i, 0);

#if __BYTE_ORDER == __BIG_ENDIAN
		pirp_save->Size = SWAP32(pirp_save->Size);
		pirp_save->NeedSize = SWAP32(pirp_save->NeedSize);
		pirp_save->Device = SWAP64(pirp_save->Device);
		pirp_save_swap->BYTE = pirp_save_swap->BYTE;
		pirp_save->Irp = SWAP32(pirp_save->Irp);
		pirp_save->Status = SWAP32(pirp_save->Status);
		pirp_save->Information = SWAP64(pirp_save->Information);
		pirp_save->Cancel = SWAP32(pirp_save->Cancel);
		pirp_save->StackLocation.empty = SWAP64(pirp_save->StackLocation.empty);
		pirp_save->StackLocation.Parameters.Others.Argument1 = SWAP64(pirp_save->StackLocation.Parameters.Others.Argument1);
		pirp_save->StackLocation.Parameters.Others.Argument2 = SWAP64(pirp_save->StackLocation.Parameters.Others.Argument2);
		pirp_save->StackLocation.Parameters.Others.Argument3 = SWAP64(pirp_save->StackLocation.Parameters.Others.Argument3);
		pirp_save->StackLocation.Parameters.Others.Argument4 = SWAP64(pirp_save->StackLocation.Parameters.Others.Argument4);
		pirp_save->BufferSize = SWAP32(pirp_save->BufferSize);
		pirp_save->Reserv = SWAP32(pirp_save->Reserv);

		if (pirp_save->StackLocation.MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL)
		{
			if (pirp_save->Is64 == 0)
			{
				purb = (PURB)pirp_save->Buffer;
				purb->UrbHeader.Length = SWAP16(purb->UrbHeader.Length);
				purb->UrbHeader.Function = SWAP16(purb->UrbHeader.Function);
				purb->UrbHeader.Status = SWAP32(purb->UrbHeader.Status);
				purb->UrbHeader.UsbdDeviceHandle = SWAP32(purb->UrbHeader.UsbdDeviceHandle);
				purb->UrbHeader.UsbdFlags = SWAP32(purb->UrbHeader.UsbdFlags);

				if (purb->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE ||
				    purb->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE ||
				    purb->UrbHeader.Function == 0x2a	// _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL
				)
				{
					purb->UrbControlDescriptorRequest.Reserved = SWAP32(purb->UrbControlDescriptorRequest.Reserved);
					purb->UrbControlDescriptorRequest.Reserved0 = SWAP32(purb->UrbControlDescriptorRequest.Reserved0);
					purb->UrbControlDescriptorRequest.TransferBufferLength = SWAP32(purb->UrbControlDescriptorRequest.TransferBufferLength);
					purb->UrbControlDescriptorRequest.TransferBuffer = SWAP32(purb->UrbControlDescriptorRequest.TransferBuffer);
					purb->UrbControlDescriptorRequest.TransferBufferMDL = SWAP32(purb->UrbControlDescriptorRequest.TransferBufferMDL);
					purb->UrbControlDescriptorRequest.UrbLink = SWAP32(purb->UrbControlDescriptorRequest.UrbLink);

					purb->UrbControlDescriptorRequest.hca.HcdEndpoint = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdEndpoint);
					purb->UrbControlDescriptorRequest.hca.HcdIrp = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdIrp);
					purb->UrbControlDescriptorRequest.hca.HcdListEntry.Flink = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdListEntry.Flink);
					purb->UrbControlDescriptorRequest.hca.HcdListEntry.Blink = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdListEntry.Blink);
					purb->UrbControlDescriptorRequest.hca.HcdListEntry2.Flink = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdListEntry2.Flink);
					purb->UrbControlDescriptorRequest.hca.HcdListEntry2.Blink = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdListEntry2.Blink);
					purb->UrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdCurrentIoFlushPointer);
					purb->UrbControlDescriptorRequest.hca.HcdExtension = SWAP32(purb->UrbControlDescriptorRequest.hca.HcdExtension);

					purb->UrbControlDescriptorRequest.Reserved1 = SWAP16(purb->UrbControlDescriptorRequest.Reserved1);
					purb->UrbControlDescriptorRequest.Reserved2 = SWAP16(purb->UrbControlDescriptorRequest.Reserved2);
					purb->UrbControlDescriptorRequest.LanguageId = SWAP16(purb->UrbControlDescriptorRequest.LanguageId);
				}
				else if (purb->UrbHeader.Function == URB_FUNCTION_SELECT_CONFIGURATION)
				{
					purb->UrbSelectConfiguration.ConfigurationDescriptor = SWAP32(purb->UrbSelectConfiguration.ConfigurationDescriptor);
					purb->UrbSelectConfiguration.ConfigurationHandle = SWAP32(purb->UrbSelectConfiguration.ConfigurationHandle);

					if (purb->UrbSelectConfiguration.ConfigurationDescriptor != 0x0)
					{
						tmp = (char*)&(purb->UrbSelectConfiguration.Interface);
						tmp_len = purb->UrbHeader.Length - 24;
				
						for (i = 0; tmp_len > 0; i++) {
							(*(USBD_INTERFACE_INFORMATION*)tmp).Length = SWAP16((*(USBD_INTERFACE_INFORMATION*)tmp).Length);
							(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle);
							(*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes);

							for (j = 0; j < (*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes; j++) {
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize = SWAP16((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize);
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle);
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize);
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags);
							}
							tmp_len -= (*(USBD_INTERFACE_INFORMATION*)tmp).Length;
							tmp += (*(USBD_INTERFACE_INFORMATION*)tmp).Length;
						}
					}
				}
				else if (purb->UrbHeader.Function == URB_FUNCTION_SELECT_INTERFACE)
				{
					purb->UrbSelectInterface.ConfigurationHandle = SWAP32(purb->UrbSelectInterface.ConfigurationHandle);
					tmp = (char*)&(purb->UrbSelectInterface.Interface);

					(*(USBD_INTERFACE_INFORMATION*)tmp).Length = SWAP16((*(USBD_INTERFACE_INFORMATION*)tmp).Length);
					(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle);
					(*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes);

					for (j = 0; j < (*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes; j++) {
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize = SWAP16((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize);
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle);
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize);
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags = SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags);
					}
				}
				else if (purb->UrbHeader.Function == URB_FUNCTION_CLASS_INTERFACE ||
					 purb->UrbHeader.Function == URB_FUNCTION_CLASS_OTHER
				)
				{
					purb->UrbControlVendorClassRequest.Reserved = SWAP32(purb->UrbControlVendorClassRequest.Reserved);
					purb->UrbControlVendorClassRequest.TransferFlags = SWAP32(purb->UrbControlVendorClassRequest.TransferFlags);
					purb->UrbControlVendorClassRequest.TransferBufferLength = SWAP32(purb->UrbControlVendorClassRequest.TransferBufferLength);
					purb->UrbControlVendorClassRequest.TransferBuffer = SWAP32(purb->UrbControlVendorClassRequest.TransferBuffer);
					purb->UrbControlVendorClassRequest.TransferBufferMDL = SWAP32(purb->UrbControlVendorClassRequest.TransferBufferMDL);
					purb->UrbControlVendorClassRequest.UrbLink = SWAP32(purb->UrbControlVendorClassRequest.UrbLink);

					purb->UrbControlVendorClassRequest.hca.HcdEndpoint = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdEndpoint);
					purb->UrbControlVendorClassRequest.hca.HcdIrp = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdIrp);
					purb->UrbControlVendorClassRequest.hca.HcdListEntry.Flink = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdListEntry.Flink);
					purb->UrbControlVendorClassRequest.hca.HcdListEntry.Blink = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdListEntry.Blink);
					purb->UrbControlVendorClassRequest.hca.HcdListEntry2.Flink = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdListEntry2.Flink);
					purb->UrbControlVendorClassRequest.hca.HcdListEntry2.Blink = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdListEntry2.Blink);
					purb->UrbControlVendorClassRequest.hca.HcdCurrentIoFlushPointer = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdCurrentIoFlushPointer);
					purb->UrbControlVendorClassRequest.hca.HcdExtension = SWAP32(purb->UrbControlVendorClassRequest.hca.HcdExtension);

					purb->UrbControlVendorClassRequest.Value = SWAP16(purb->UrbControlVendorClassRequest.Value);
					purb->UrbControlVendorClassRequest.Index = SWAP16(purb->UrbControlVendorClassRequest.Index);
					purb->UrbControlVendorClassRequest.Reserved1 = SWAP16(purb->UrbControlVendorClassRequest.Reserved1);
				}
				else if (purb->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
				{
					purb->UrbBulkOrInterruptTransfer.PipeHandle = SWAP32(purb->UrbBulkOrInterruptTransfer.PipeHandle);
					purb->UrbBulkOrInterruptTransfer.TransferFlags = SWAP32(purb->UrbBulkOrInterruptTransfer.TransferFlags);
					purb->UrbBulkOrInterruptTransfer.TransferBufferLength = SWAP32(purb->UrbBulkOrInterruptTransfer.TransferBufferLength);
					purb->UrbBulkOrInterruptTransfer.TransferBuffer = SWAP32(purb->UrbBulkOrInterruptTransfer.TransferBuffer);
					purb->UrbBulkOrInterruptTransfer.TransferBufferMDL = SWAP32(purb->UrbBulkOrInterruptTransfer.TransferBufferMDL);
					purb->UrbBulkOrInterruptTransfer.UrbLink = SWAP32(purb->UrbBulkOrInterruptTransfer.UrbLink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint);
					purb->UrbBulkOrInterruptTransfer.hca.HcdIrp = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdIrp);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Flink = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Flink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Blink = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Blink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Flink = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Flink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Blink = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Blink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer);
					purb->UrbBulkOrInterruptTransfer.hca.HcdExtension = SWAP32(purb->UrbBulkOrInterruptTransfer.hca.HcdExtension);
				}
				else if (purb->UrbHeader.Function == URB_FUNCTION_ABORT_PIPE ||
					 purb->UrbHeader.Function == URB_FUNCTION_RESET_PIPE
				)
				{
					purb->UrbPipeRequest.PipeHandle = SWAP32(purb->UrbPipeRequest.PipeHandle);
				}
			}
			else
			{
				purb_64 = (PURB_64)pirp_save->Buffer;

				purb_64->UrbHeader.Length = SWAP16(purb_64->UrbHeader.Length);
				purb_64->UrbHeader.Function = SWAP16(purb_64->UrbHeader.Function);
				purb_64->UrbHeader.Status = SWAP32(purb_64->UrbHeader.Status);
				purb_64->UrbHeader.UsbdDeviceHandle = SWAP64(purb_64->UrbHeader.UsbdDeviceHandle);
				purb_64->UrbHeader.UsbdFlags = SWAP32(purb_64->UrbHeader.UsbdFlags);

				if (purb_64->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE ||
				    purb_64->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE ||
				    purb_64->UrbHeader.Function == 0x2a	// _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL
				)
				{
					purb_64->UrbControlDescriptorRequest.Reserved = SWAP64(purb_64->UrbControlDescriptorRequest.Reserved);
					purb_64->UrbControlDescriptorRequest.Reserved0 = SWAP32(purb_64->UrbControlDescriptorRequest.Reserved0);
					purb_64->UrbControlDescriptorRequest.TransferBufferLength = SWAP32(purb_64->UrbControlDescriptorRequest.TransferBufferLength);
					purb_64->UrbControlDescriptorRequest.TransferBuffer = SWAP64(purb_64->UrbControlDescriptorRequest.TransferBuffer);
					purb_64->UrbControlDescriptorRequest.TransferBufferMDL = SWAP64(purb_64->UrbControlDescriptorRequest.TransferBufferMDL);
					purb_64->UrbControlDescriptorRequest.UrbLink = SWAP64(purb_64->UrbControlDescriptorRequest.UrbLink);

					purb_64->UrbControlDescriptorRequest.hca.Reserved8[0] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[0]);
					purb_64->UrbControlDescriptorRequest.hca.Reserved8[1] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[1]);
					purb_64->UrbControlDescriptorRequest.hca.Reserved8[2] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[2]);
					purb_64->UrbControlDescriptorRequest.hca.Reserved8[3] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[3]);
					purb_64->UrbControlDescriptorRequest.hca.Reserved8[4] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[4]);
					purb_64->UrbControlDescriptorRequest.hca.Reserved8[5] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[5]);
					purb_64->UrbControlDescriptorRequest.hca.Reserved8[6] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[6]);
					purb_64->UrbControlDescriptorRequest.hca.Reserved8[7] = SWAP64(purb_64->UrbControlDescriptorRequest.hca.Reserved8[7]);

					purb_64->UrbControlDescriptorRequest.Reserved1 = SWAP16(purb_64->UrbControlDescriptorRequest.Reserved1);
					purb_64->UrbControlDescriptorRequest.Reserved2 = SWAP16(purb_64->UrbControlDescriptorRequest.Reserved2);
					purb_64->UrbControlDescriptorRequest.LanguageId = SWAP16(purb_64->UrbControlDescriptorRequest.LanguageId);
				}
				else if (purb_64->UrbHeader.Function == URB_FUNCTION_SELECT_CONFIGURATION)
				{
					purb_64->UrbSelectConfiguration.ConfigurationDescriptor = SWAP64(purb_64->UrbSelectConfiguration.ConfigurationDescriptor);
					purb_64->UrbSelectConfiguration.ConfigurationHandle = SWAP64(purb_64->UrbSelectConfiguration.ConfigurationHandle);

					if (purb_64->UrbSelectConfiguration.ConfigurationDescriptor != 0x0)
					{
						tmp = (char*)&(purb_64->UrbSelectConfiguration.Interface);
						tmp_len = purb_64->UrbHeader.Length - 0x28;

						for (i = 0; tmp_len > 0; i++) {
							(*(USBD_INTERFACE_INFORMATION_64*)tmp).Length = SWAP16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Length);
							(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle = SWAP64((*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle);
							(*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes);
							for (j = 0; j < (*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes; j++) {
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize = SWAP16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle = SWAP64((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags);
							}
							tmp_len -= (*(USBD_INTERFACE_INFORMATION_64*)tmp).Length;
							tmp += (*(USBD_INTERFACE_INFORMATION_64*)tmp).Length;
						}
					}
				}
				else if (purb_64->UrbHeader.Function == URB_FUNCTION_SELECT_INTERFACE)
				{
					purb_64->UrbSelectInterface.ConfigurationHandle = SWAP64(purb_64->UrbSelectInterface.ConfigurationHandle);
					tmp = (char*)&(purb_64->UrbSelectInterface.Interface);

					(*(USBD_INTERFACE_INFORMATION_64*)tmp).Length = SWAP16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Length);
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle = SWAP64((*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle);
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes);

					for (j = 0; j < (*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes; j++) {
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize = SWAP16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle = SWAP64((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags = SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags);
					}
				}
				else if (purb_64->UrbHeader.Function == URB_FUNCTION_CLASS_INTERFACE ||
					 purb_64->UrbHeader.Function == URB_FUNCTION_CLASS_OTHER
				)
				{
					purb_64->UrbControlVendorClassRequest.Reserved = SWAP64(purb_64->UrbControlVendorClassRequest.Reserved);
					purb_64->UrbControlVendorClassRequest.TransferFlags = SWAP32(purb_64->UrbControlVendorClassRequest.TransferFlags);
					purb_64->UrbControlVendorClassRequest.TransferBufferLength = SWAP32(purb_64->UrbControlVendorClassRequest.TransferBufferLength);
					purb_64->UrbControlVendorClassRequest.TransferBuffer = SWAP64(purb_64->UrbControlVendorClassRequest.TransferBuffer);
					purb_64->UrbControlVendorClassRequest.TransferBufferMDL = SWAP64(purb_64->UrbControlVendorClassRequest.TransferBufferMDL);
					purb_64->UrbControlVendorClassRequest.UrbLink = SWAP64(purb_64->UrbControlVendorClassRequest.UrbLink);

					purb_64->UrbControlVendorClassRequest.hca.Reserved8[0] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[0]);
					purb_64->UrbControlVendorClassRequest.hca.Reserved8[1] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[1]);
					purb_64->UrbControlVendorClassRequest.hca.Reserved8[2] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[2]);
					purb_64->UrbControlVendorClassRequest.hca.Reserved8[3] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[3]);
					purb_64->UrbControlVendorClassRequest.hca.Reserved8[4] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[4]);
					purb_64->UrbControlVendorClassRequest.hca.Reserved8[5] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[5]);
					purb_64->UrbControlVendorClassRequest.hca.Reserved8[6] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[6]);
					purb_64->UrbControlVendorClassRequest.hca.Reserved8[7] = SWAP64(purb_64->UrbControlVendorClassRequest.hca.Reserved8[7]);

					purb_64->UrbControlVendorClassRequest.Value = SWAP16(purb_64->UrbControlVendorClassRequest.Value);
					purb_64->UrbControlVendorClassRequest.Index = SWAP16(purb_64->UrbControlVendorClassRequest.Index);
					purb_64->UrbControlVendorClassRequest.Reserved1 = SWAP16(purb_64->UrbControlVendorClassRequest.Reserved1);
				}
				else if (purb_64->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
				{
					purb_64->UrbBulkOrInterruptTransfer.PipeHandle = SWAP64(purb_64->UrbBulkOrInterruptTransfer.PipeHandle);
					purb_64->UrbBulkOrInterruptTransfer.TransferFlags = SWAP32(purb_64->UrbBulkOrInterruptTransfer.TransferFlags);
					purb_64->UrbBulkOrInterruptTransfer.TransferBufferLength = SWAP32(purb_64->UrbBulkOrInterruptTransfer.TransferBufferLength);
					purb_64->UrbBulkOrInterruptTransfer.TransferBuffer = SWAP64(purb_64->UrbBulkOrInterruptTransfer.TransferBuffer);
					purb_64->UrbBulkOrInterruptTransfer.TransferBufferMDL = SWAP64(purb_64->UrbBulkOrInterruptTransfer.TransferBufferMDL);
					purb_64->UrbBulkOrInterruptTransfer.UrbLink = SWAP64(purb_64->UrbBulkOrInterruptTransfer.UrbLink);					
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[0] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[0]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[1] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[1]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[2] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[2]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[3] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[3]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[4] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[4]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[5] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[5]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[6] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[6]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[7] = SWAP64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[7]);
				}
				else if (purb_64->UrbHeader.Function == URB_FUNCTION_ABORT_PIPE ||
					 purb_64->UrbHeader.Function == URB_FUNCTION_RESET_PIPE
				)
				{
					purb_64->UrbPipeRequest.PipeHandle = SWAP64(purb_64->UrbPipeRequest.PipeHandle);
				}
			}
		}
#endif

		PDEBUG("(%d/%d) ", sockfd, pirp_save->Irp);

		conn_curt->irp = pirp_save->Irp;

		if (ip_monopoly.s_addr)
		{
			if (conn_curt->ip.s_addr == ip_monopoly.s_addr)
			{
				time_monopoly = time((time_t*)NULL);
				if (conn_busy != conn_curt)
				{
					nvram_set("u2ec_busyip", inet_ntoa(conn_curt->ip));
					last_busy_conn = conn_curt;
					conn_busy->busy = CONN_IS_IDLE;
					conn_curt->busy = CONN_IS_BUSY;
					conn_busy = conn_curt;
				}
			}
			else
			{
				PDEBUG("ignore pkt for monopoly\n");

				u2ec_list_for_each(pos, &conn_info_list)
				{
					if (ip_monopoly.s_addr && ip_monopoly.s_addr == ((PCONNECTION_INFO)pos)->ip.s_addr)
						PDEBUG("ip %s, sock %d, busy %d. Monopoly.\n", inet_ntoa(((PCONNECTION_INFO)pos)->ip), ((PCONNECTION_INFO)pos)->sockfd, ((PCONNECTION_INFO)pos)->busy);
					else
						PDEBUG("ip %s, sock %d, busy %d.\n", inet_ntoa(((PCONNECTION_INFO)pos)->ip), ((PCONNECTION_INFO)pos)->sockfd, ((PCONNECTION_INFO)pos)->busy);
				}
				PDEBUG("\n");

				conn_curt->busy = CONN_IS_WAITING;
				return 0;
			}
		}

		if (conn_info_list.next->next != &conn_info_list && !ip_monopoly.s_addr) {
			if ((except_flag = except((struct u2ec_list_head *)conn_curt, pirp_save, 0)) == 1)
				return 0;
			else if (except_flag == 4)
				goto EXCHANGE_IRP;
		}
		else if ((except_flag = except((struct u2ec_list_head *)conn_curt, pirp_save, 1)) == 1 || except_flag == 4)
		{
			except_flag_1client = 1;
			except_flag = 0;
		}
		else
			except_flag = 0;

		conn_curt->time = time((time_t*)NULL);
		if (conn_busy != conn_curt) {
			semaphore_wait();
			if ((i = MFP_state(MFP_GET_STATE))) {
				semaphore_post();
				if (i == MFP_IN_LPRNG)
					alarm(1);
				if (except_flag == 2) {
					conn_curt->busy = CONN_IS_RETRY;
					except_flag += 100;
					goto EXCHANGE_IRP;
				}
				if (conn_curt->busy != CONN_IS_RETRY)
					conn_curt->busy = CONN_IS_WAITING;
				return 0;
			} else {
				nvram_set("u2ec_busyip", inet_ntoa(conn_curt->ip));
				semaphore_post();
				last_busy_conn = conn_curt;
				conn_curt->busy = CONN_IS_BUSY;
				if ((struct u2ec_list_head*)conn_busy != &conn_info_list)
					conn_busy->busy = CONN_IS_IDLE;
			}
		}
		semaphore_wait();
		if (!MFP_state(MFP_GET_STATE) && pirp_save->Irp > 0) {
			if (!except_flag_1client)
			{				
				MFP_state(MFP_IN_U2EC);
				nvram_set("u2ec_busyip", inet_ntoa(conn_curt->ip));
				semaphore_post();
			}
			else
				semaphore_post();
			alarm(1);
		} else if (MFP_state(MFP_GET_STATE) == MFP_IN_U2EC && except_flag_1client) {
			MFP_state(MFP_IS_IDLE);
			nvram_set("u2ec_busyip", "");
			semaphore_post();
		} else
			semaphore_post();
EXCHANGE_IRP:
		/* Fill pirp_save depend on mj & mn function in IRP. */
		pirp_saverw = (PIRP_SAVE)data_bufrw;
		memcpy(pirp_saverw, pirp_save, MAX_BUF_LEN);

		PDECODE_IRP(pirp_save, 0);
		if (except_flag < 100) {
			if ((i = exchangeIRP(pirp_save, pirp_saverw, (struct u2ec_list_head *)conn_curt)) == 0)
				return 0;
			else if (i == -1)
				goto CLOSE_CONN;
		}
		else if (except_flag == 102) {
			if (pirp_save->Is64 == 0)
				handleURB_fake(pirp_saverw);
			else
				handleURB_64_fake(pirp_saverw);
			usleep(20000);
		}
		PDECODE_IRP(pirp_saverw, 1);

		if (except_flag == 3)
			pirp_saverw->Status = 0xC00000AE;	// STATUS_PIPE_BUSY

		/* Send tcp_pack IrpAnswerTcp. */
#if __BYTE_ORDER == __LITTLE_ENDIAN
		tcp_pack.Type = IrpAnswerTcp;
		tcp_pack.SizeBuffer = pirp_saverw->NeedSize;
#else
		tcp_pack.Type = SWAP_BACK32(IrpAnswerTcp);
		tcp_pack.SizeBuffer = SWAP_BACK32(pirp_saverw->NeedSize);
#endif
		send(sockfd, (char *)&tcp_pack, sizeof(tcp_pack), 0);
		PSNDRECV("usb_connection: sent tcp pack, size:%d.\n", sizeof(tcp_pack));

		/* Send irp_save. */
#if __BYTE_ORDER == __LITTLE_ENDIAN
		send(sockfd, (char *)pirp_saverw, pirp_saverw->NeedSize, 0);
		PSNDRECV("usb_connection: sent pirp_saverw, size:%d.\n", pirp_saverw->NeedSize);
#else
		pirp_saverw->Size = SWAP_BACK32(pirp_saverw->Size);
		pirp_saverw->NeedSize = SWAP_BACK32(pirp_saverw->NeedSize);
		pirp_saverw->Device = SWAP_BACK64(pirp_saverw->Device);
		pirp_saverw->Irp = SWAP_BACK32(pirp_saverw->Irp);
		pirp_saverw->Status = SWAP_BACK32(pirp_saverw->Status);
		pirp_saverw->Information = SWAP_BACK64(pirp_saverw->Information);
		pirp_saverw->Cancel = SWAP_BACK32(pirp_saverw->Cancel);
		pirp_saverw->StackLocation.Parameters.Others.Argument1 = SWAP_BACK64(pirp_saverw->StackLocation.Parameters.Others.Argument1);
		pirp_saverw->StackLocation.Parameters.Others.Argument2 = SWAP_BACK64(pirp_saverw->StackLocation.Parameters.Others.Argument2);
		pirp_saverw->StackLocation.Parameters.Others.Argument3 = SWAP_BACK64(pirp_saverw->StackLocation.Parameters.Others.Argument3);
		pirp_saverw->StackLocation.Parameters.Others.Argument4 = SWAP_BACK64(pirp_saverw->StackLocation.Parameters.Others.Argument4);
		pirp_saverw->BufferSize = SWAP_BACK32(pirp_saverw->BufferSize);
		pirp_saverw->Reserv = SWAP_BACK32(pirp_saverw->Reserv);

		if (pirp_save->StackLocation.MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL)
		{
			if (pirp_save->Is64 == 0)
			{
				purb = (PURB)pirp_saverw->Buffer;
				purb->UrbHeader.Length = SWAP_BACK16(purb->UrbHeader.Length);
				purb->UrbHeader.Function = SWAP_BACK16(purb->UrbHeader.Function);
				purb->UrbHeader.Status = SWAP_BACK32(purb->UrbHeader.Status);
				purb->UrbHeader.UsbdDeviceHandle = SWAP_BACK32(purb->UrbHeader.UsbdDeviceHandle);
				purb->UrbHeader.UsbdFlags = SWAP_BACK32(purb->UrbHeader.UsbdFlags);

				if (((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE ||
				    ((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE ||
				    ((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_CLASS_INTERFACE ||
				    ((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_CLASS_OTHER ||
				    ((PURB)pirp_save->Buffer)->UrbHeader.Function == 0x2a	// _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL
				)
				{
					purb->UrbControlTransfer.PipeHandle = SWAP_BACK32(purb->UrbControlTransfer.PipeHandle);
					purb->UrbControlTransfer.TransferFlags = SWAP_BACK32(purb->UrbControlTransfer.TransferFlags);
					purb->UrbControlTransfer.TransferBufferLength = SWAP_BACK32(purb->UrbControlTransfer.TransferBufferLength);
					purb->UrbControlTransfer.TransferBuffer = SWAP_BACK32(purb->UrbControlTransfer.TransferBuffer);
					purb->UrbControlTransfer.TransferBufferMDL = SWAP_BACK32(purb->UrbControlTransfer.TransferBufferMDL);
					purb->UrbControlTransfer.UrbLink = SWAP_BACK32(purb->UrbControlTransfer.UrbLink);

					purb->UrbControlTransfer.hca.HcdEndpoint = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdEndpoint);
					purb->UrbControlTransfer.hca.HcdIrp = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdIrp);
					purb->UrbControlTransfer.hca.HcdListEntry.Flink = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdListEntry.Flink);
					purb->UrbControlTransfer.hca.HcdListEntry.Blink = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdListEntry.Blink);
					purb->UrbControlTransfer.hca.HcdListEntry2.Flink = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdListEntry2.Flink);
					purb->UrbControlTransfer.hca.HcdListEntry2.Blink = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdListEntry2.Blink);
					purb->UrbControlTransfer.hca.HcdCurrentIoFlushPointer = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdCurrentIoFlushPointer);
					purb->UrbControlTransfer.hca.HcdExtension = SWAP_BACK32(purb->UrbControlTransfer.hca.HcdExtension);

					purb->UrbControlDescriptorRequest.Reserved1 = SWAP_BACK16(purb->UrbControlDescriptorRequest.Reserved1);
					purb->UrbControlDescriptorRequest.Reserved2 = SWAP_BACK16(purb->UrbControlDescriptorRequest.Reserved2);
					purb->UrbControlDescriptorRequest.LanguageId = SWAP_BACK16(purb->UrbControlDescriptorRequest.LanguageId);
				}
				else if (((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_SELECT_CONFIGURATION)
				{
					purb->UrbSelectConfiguration.ConfigurationDescriptor = SWAP_BACK32(purb->UrbSelectConfiguration.ConfigurationDescriptor);
					purb->UrbSelectConfiguration.ConfigurationHandle = SWAP_BACK32(purb->UrbSelectConfiguration.ConfigurationHandle);

					if (SWAP32(purb->UrbSelectConfiguration.ConfigurationDescriptor) != 0x0)
					{
						tmp = (char*)&(purb->UrbSelectConfiguration.Interface);
						tmp_len = SWAP16(purb->UrbHeader.Length) - 24;

						for (i = 0; tmp_len > 0; i++) {
							(*(USBD_INTERFACE_INFORMATION*)tmp).Length = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION*)tmp).Length);
							(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle);
							(*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes);

							for (j = 0; j < SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes); j++) {
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize);
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle);
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize);
								(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags);
							}
							tmp_len -= SWAP16((*(USBD_INTERFACE_INFORMATION*)tmp).Length);
							tmp += SWAP16((*(USBD_INTERFACE_INFORMATION*)tmp).Length);
						}
					}
				}
				else if (((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_SELECT_INTERFACE)
				{
					purb->UrbSelectInterface.ConfigurationHandle = SWAP_BACK32(purb->UrbSelectInterface.ConfigurationHandle);
					tmp = (char*)&(purb->UrbSelectInterface.Interface);

					(*(USBD_INTERFACE_INFORMATION*)tmp).Length = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION*)tmp).Length);
					(*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).InterfaceHandle);
					(*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes);

					for (j = 0; j < SWAP32((*(USBD_INTERFACE_INFORMATION*)tmp).NumberOfPipes); j++) {
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumPacketSize);
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeHandle);
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].MaximumTransferSize);
						(*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION*)tmp).Pipes[j].PipeFlags);
					}
				}
				else if (((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
				{
					purb->UrbBulkOrInterruptTransfer.PipeHandle = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.PipeHandle);
					purb->UrbBulkOrInterruptTransfer.TransferFlags = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.TransferFlags);
					purb->UrbBulkOrInterruptTransfer.TransferBufferLength = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.TransferBufferLength);
					purb->UrbBulkOrInterruptTransfer.TransferBuffer = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.TransferBuffer);
					purb->UrbBulkOrInterruptTransfer.TransferBufferMDL = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.TransferBufferMDL);
					purb->UrbBulkOrInterruptTransfer.UrbLink = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.UrbLink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdEndpoint);
					purb->UrbBulkOrInterruptTransfer.hca.HcdIrp = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdIrp);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Flink = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Flink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Blink = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry.Blink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Flink = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Flink);
					purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Blink = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdListEntry2.Blink);					
					purb->UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdCurrentIoFlushPointer);
					purb->UrbBulkOrInterruptTransfer.hca.HcdExtension = SWAP_BACK32(purb->UrbBulkOrInterruptTransfer.hca.HcdExtension);
				}
				else if (((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_ABORT_PIPE ||
					 ((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_RESET_PIPE
				)
				{
					purb->UrbPipeRequest.PipeHandle = SWAP_BACK32(purb->UrbPipeRequest.PipeHandle);
				}
			}
			else
			{
				purb_64 = (PURB_64)pirp_saverw->Buffer;

				purb_64->UrbHeader.Length = SWAP_BACK16(purb_64->UrbHeader.Length);
				purb_64->UrbHeader.Function = SWAP_BACK16(purb_64->UrbHeader.Function);
				purb_64->UrbHeader.Status = SWAP_BACK32(purb_64->UrbHeader.Status);
				purb_64->UrbHeader.UsbdDeviceHandle = SWAP_BACK64(purb_64->UrbHeader.UsbdDeviceHandle);
				purb_64->UrbHeader.UsbdFlags = SWAP_BACK32(purb_64->UrbHeader.UsbdFlags);

				if (((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE ||
				    ((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE ||
				    ((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_CLASS_INTERFACE ||
				    ((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_CLASS_OTHER ||
				    ((PURB_64)pirp_save->Buffer)->UrbHeader.Function == 0x2a	// _URB_OS_FEATURE_DESCRIPTOR_REQUEST_FULL
				)
				{
					purb_64->UrbControlTransfer.PipeHandle = SWAP_BACK64(purb_64->UrbControlTransfer.PipeHandle);
					purb_64->UrbControlTransfer.TransferFlags = SWAP_BACK32(purb_64->UrbControlTransfer.TransferFlags);
					purb_64->UrbControlTransfer.TransferBufferLength = SWAP_BACK32(purb_64->UrbControlTransfer.TransferBufferLength);
					purb_64->UrbControlTransfer.TransferBuffer = SWAP_BACK64(purb_64->UrbControlTransfer.TransferBuffer);
					purb_64->UrbControlTransfer.TransferBufferMDL = SWAP_BACK64(purb_64->UrbControlTransfer.TransferBufferMDL);
					purb_64->UrbControlTransfer.UrbLink = SWAP_BACK64(purb_64->UrbControlTransfer.UrbLink);

					purb_64->UrbControlTransfer.hca.Reserved8[0] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[0]);
					purb_64->UrbControlTransfer.hca.Reserved8[1] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[1]);
					purb_64->UrbControlTransfer.hca.Reserved8[2] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[2]);
					purb_64->UrbControlTransfer.hca.Reserved8[3] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[3]);
					purb_64->UrbControlTransfer.hca.Reserved8[4] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[4]);
					purb_64->UrbControlTransfer.hca.Reserved8[5] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[5]);
					purb_64->UrbControlTransfer.hca.Reserved8[6] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[6]);
					purb_64->UrbControlTransfer.hca.Reserved8[7] = SWAP_BACK64(purb_64->UrbControlTransfer.hca.Reserved8[7]);
				}
				else if (((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_SELECT_CONFIGURATION)
				{
					purb_64->UrbSelectConfiguration.ConfigurationDescriptor = SWAP_BACK64(purb_64->UrbSelectConfiguration.ConfigurationDescriptor);
					purb_64->UrbSelectConfiguration.ConfigurationHandle = SWAP_BACK64(purb_64->UrbSelectConfiguration.ConfigurationHandle);

					if (purb_64->UrbSelectConfiguration.ConfigurationDescriptor != 0x0)
					{
						tmp = (char*)&(purb_64->UrbSelectConfiguration.Interface);
						tmp_len = SWAP16(purb_64->UrbHeader.Length) - 0x28;

						for (i = 0; tmp_len > 0; i++) {
							(*(USBD_INTERFACE_INFORMATION_64*)tmp).Length = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Length);
							(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle = SWAP_BACK64((*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle);
							(*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes);
							for (j = 0; j < SWAP32((*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes); j++) {
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle = SWAP_BACK64((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize);
								(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags);
							}
							tmp_len -= SWAP16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Length);
							tmp += SWAP16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Length);
						}
					}
				}
				else if (((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_SELECT_INTERFACE)
				{
					purb_64->UrbSelectInterface.ConfigurationHandle = SWAP_BACK64(purb_64->UrbSelectInterface.ConfigurationHandle);
					tmp = (char*)&(purb_64->UrbSelectInterface.Interface);

					(*(USBD_INTERFACE_INFORMATION_64*)tmp).Length = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Length);
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle = SWAP_BACK64((*(USBD_INTERFACE_INFORMATION_64*)tmp).InterfaceHandle);
					(*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes);

					for (j = 0; j < (*(USBD_INTERFACE_INFORMATION_64*)tmp).NumberOfPipes; j++) {
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize = SWAP_BACK16((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumPacketSize);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeType);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle = SWAP_BACK64((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeHandle);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].MaximumTransferSize);
						(*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags = SWAP_BACK32((*(USBD_INTERFACE_INFORMATION_64*)tmp).Pipes[j].PipeFlags);
					}
				}
				else if (((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
				{
					purb_64->UrbBulkOrInterruptTransfer.PipeHandle = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.PipeHandle);
					purb_64->UrbBulkOrInterruptTransfer.TransferFlags = SWAP_BACK32(purb_64->UrbBulkOrInterruptTransfer.TransferFlags);
					purb_64->UrbBulkOrInterruptTransfer.TransferBufferLength = SWAP_BACK32(purb_64->UrbBulkOrInterruptTransfer.TransferBufferLength);
					purb_64->UrbBulkOrInterruptTransfer.TransferBuffer = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.TransferBuffer);
					purb_64->UrbBulkOrInterruptTransfer.TransferBufferMDL = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.TransferBufferMDL);
					purb_64->UrbBulkOrInterruptTransfer.UrbLink = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.UrbLink);					
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[0] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[0]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[1] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[1]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[2] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[2]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[3] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[3]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[4] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[4]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[5] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[5]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[6] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[6]);
					purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[7] = SWAP_BACK64(purb_64->UrbBulkOrInterruptTransfer.hca.Reserved8[7]);
				}
				else if (((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_ABORT_PIPE ||
					 ((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_RESET_PIPE
				)
				{
					purb_64->UrbPipeRequest.PipeHandle = SWAP_BACK32(purb_64->UrbPipeRequest.PipeHandle);
				}
			}
		}

		send(sockfd, (char *)pirp_saverw, SWAP32(pirp_saverw->NeedSize), 0);
		PSNDRECV("usb_connection: sent pirp_saverw, size:%d.\n", SWAP32(pirp_saverw->NeedSize));
#endif

		if (	Is_Canon && flag_canon_state == 3 && flag_canon_ok > 3 &&
			pirp_save->StackLocation.MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL &&
			(	(	pirp_save->Is64 == 0 &&
					((PURB)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER
				)	||
				(	pirp_save->Is64 == 1 &&
					((PURB_64)pirp_save->Buffer)->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER
				)
			)
		)
		{
			memcpy(pirp_save, data_canon, MAX_BUF_LEN);
			PDEBUG("retry...\n");
			goto EXCHANGE_IRP;
		}

		break;
	}
	return 1;

CLOSE_CONN:
	if (ip_monopoly.s_addr && conn_curt->ip.s_addr == ip_monopoly.s_addr)
	{
		PDEBUG("disable monopoly for closing connection\n");
		ip_monopoly.s_addr = 0;
		nvram_set("mfp_ip_monopoly", "");
	}

	if (conn_busy == conn_curt) {
		alarm(0);

		count_bulk_read = 0;
		count_bulk_write = 0;
		count_bulk_read_ret0_1 = 0;
		count_bulk_read_ret0_2 = 0;
		count_bulk_read_epson = 0;
		flag_monitor_epson = 0;
		flag_canon_state = 0;
		flag_canon_ok = 0;

		u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
		write(u2ec_fifo, "c", 1);
		close(u2ec_fifo);
	}

	conn_curt->ip.s_addr = 0;
	conn_curt->count_class_int_in = 0;
	conn_curt->irp = 0;
	conn_curt->sockfd = 0;

	u2ec_list_del((struct u2ec_list_head*)conn_curt);
	free(conn_curt);
	close(sockfd);	// bye!
	fd_in_use[sockfd] = 0;
	return -sockfd;
}

/*
 * Handle new connections.
 * accept them.
 */
int establish_connection(int sockfd)
{
	int			new_fd;
	struct sockaddr_in	cli_addr;
	int			addr_len = sizeof(cli_addr);
	PCONNECTION_INFO	new_conn;

	if ((new_fd = accept(sockfd, (struct sockaddr *)&cli_addr, &addr_len)) < 0) {
		PERROR("accept");
		return -1;
	}

	if (send(new_fd, "\0\0", 2, 0) < 0) {
		PERROR("send");
		return -1;
	}

	PSNDRECV("establish_connection sent successful.\n");

	if (!dev)
	{
		PERROR("no device");
		close(new_fd);
		return -1;
	}

	new_conn = malloc(sizeof(CONNECTION_INFO));
	bzero((char *)new_conn, sizeof(CONNECTION_INFO));
	new_conn->sockfd = new_fd;
	new_conn->busy = CONN_IS_IDLE;
	new_conn->time = time((time_t*)NULL);
	new_conn->ip = cli_addr.sin_addr;
	new_conn->count_class_int_in = 0;
	new_conn->irp = 0;

	u2ec_list_add_tail((struct u2ec_list_head*)new_conn, &conn_info_list);

	PDEBUG("establish_connection: new connection from %s on socket %d\n",
		inet_ntoa(cli_addr.sin_addr), new_fd);

	return new_fd;
}

static void update_device()
{
	int i;
	int single_interface = 1;

	testusb();

	if (dev) {
		for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
			if (dev->config[i].bNumInterfaces > 1) {
				single_interface = 0;
				break;
			}
#if __BYTE_ORDER == __LITTLE_ENDIAN
#ifdef	SUPPORT_LPRng
		if (nvram_match("usr_timeout", "1")) {
			int timeout;
			timeout = atoi(nvram_safe_get("usr_timeout_ctrl"));
			timeout_control_msg = timeout <= 0 ? 1000 : timeout;
			timeout = atoi(nvram_safe_get("usr_timeout_br"));
			timeout_bulk_read_msg = timeout <= 0 ? 5000 : timeout;
			timeout = atoi(nvram_safe_get("usr_timeout_bw"));
			timeout_bulk_write_msg = timeout <= 0 ? 5000 : timeout;
		} else
#endif
#endif
		if (Is_EPSON) {					// EPSON
			timeout_control_msg = 100;
			if (single_interface)
				timeout_bulk_read_msg = 5000;
			else
				timeout_bulk_read_msg = 30000;
			timeout_bulk_write_msg = 5000;
		}
		else if (Is_Lexmark) {				// Lexmark
			timeout_control_msg = 10;
			timeout_bulk_read_msg = 100;
			timeout_bulk_write_msg = 100;
		}
		else if (Is_HP) {				// HP
			timeout_control_msg = 1000;
			if (dev->descriptor.idProduct == 0x4117)// HP LaserJet 1018 
				timeout_bulk_read_msg =1000;
			else if (single_interface)
				timeout_bulk_read_msg = 5000;
			else
				timeout_bulk_read_msg = 15000;

//			timeout_bulk_write_msg = 5000;
			timeout_bulk_write_msg = 32767;		// magic number for no timeout
		}
		else if (Is_Canon) {				// Canon
			timeout_control_msg = 100;
			timeout_bulk_read_msg = 30000;
			timeout_bulk_write_msg = 1000;
		}
		else {						// for general printers
			timeout_control_msg = 1000;
			if (single_interface)
				timeout_bulk_read_msg = 5000;
			else
				timeout_bulk_read_msg = 10000;
			timeout_bulk_write_msg = 5000;
		}
	}
	PDEBUG("\ntimeout_control_msg:%d\ntimeout_bulk_read_msg:%d\ntimeout_bulk_write_msg:%d\n",
		timeout_control_msg, timeout_bulk_read_msg, timeout_bulk_write_msg);
}

/*
 * alarm handle.
 * Only there is a busy connection, the alarm runs.
 */
static void multi_client(int sig)
{
	struct u2ec_list_head	*pos;
	u2ec_list_for_each(pos, &conn_info_list)
	{
		if (ip_monopoly.s_addr && ip_monopoly.s_addr == ((PCONNECTION_INFO)pos)->ip.s_addr)
			PDEBUG("ip %s, sock %d, busy %d. Monopoly.\n", inet_ntoa(((PCONNECTION_INFO)pos)->ip), ((PCONNECTION_INFO)pos)->sockfd, ((PCONNECTION_INFO)pos)->busy);
		else
			PDEBUG("ip %s, sock %d, busy %d.\n", inet_ntoa(((PCONNECTION_INFO)pos)->ip), ((PCONNECTION_INFO)pos)->sockfd, ((PCONNECTION_INFO)pos)->busy);
	}
	PDEBUG("\n");

	alarm(1);
	if (time((time_t*)NULL) - last_busy_conn->time > U2EC_TIMEOUT) {
		PDEBUG(" U2EC TIMEOUT!\n");

		alarm(0);
		int u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
		write(u2ec_fifo, "c", 1);
		close(u2ec_fifo);
	}
}

/*
 * Handle fifo used by hotplug_usb and multi_client.
 * modify fd_set master_fds if necessary.
 */
static int handle_fifo(int *fd, fd_set *pfds, int *pfdm, int conn_fd)
{
	struct u2ec_list_head	*pos, *tmp;
	char			c;
	int			rtvl = 0;
	int			need_to_udpate_device = 0;

	/* Read & reset fifo. */
	read(*fd, &c, 1);
	close(*fd);
	FD_CLR(*fd, pfds);

	semaphore_wait();
	/* Handle hotplug usb. */
	if (c == 'a') {		// add usb device.
		if (time((time_t*)NULL) - time_add_device < 1)
		{
			PDEBUG("time interval for updating is too small!\n");
			goto CANCEL;
		}
		else
			time_add_device = time((time_t*)NULL);

		hotplug_debug("u2ec adding...\n");
		MFP_state(MFP_IS_IDLE);
		nvram_set("u2ec_busyip", "");
		update_device();
		semaphore_post();
		FD_SET(conn_fd, pfds);
		*pfdm = conn_fd > *pfdm ? conn_fd : *pfdm;
		rtvl = 1;
	}
	else if (c == 'r') {	// remove usb device.
		if (time((time_t*)NULL) - time_remove_device < 1)
		{
			PDEBUG("time interval for updating is too small!\n");
			goto CANCEL;
		}
		else
			time_remove_device = time((time_t*)NULL);

		hotplug_debug("u2ec removing...\n");
		alarm(0);
		MFP_state(MFP_IS_IDLE);
		nvram_set("u2ec_busyip", "");
		update_device();
		semaphore_post();
		if (!dev)
		{
			PDEBUG("\nReset connection for removing\n");
			u2ec_list_for_each_safe(pos, tmp, &conn_info_list) {
				close(((PCONNECTION_INFO)pos)->sockfd);
				fd_in_use[((PCONNECTION_INFO)pos)->sockfd] = 0;
				((PCONNECTION_INFO)pos)->sockfd = 0;
				((PCONNECTION_INFO)pos)->ip.s_addr = 0;
				u2ec_list_del(pos);
				free(pos);
			}
		}
		FD_ZERO(pfds);
		*pfdm = 0;
		rtvl = 1;
	}
	/* Handle multi client. */
	else if (c == 'c') {	// connect reset.
		switch(MFP_state(MFP_GET_STATE)) {
		case MFP_IN_LPRNG:
			semaphore_post();
			alarm(1);
			break;
		case MFP_IN_U2EC:
			MFP_state(MFP_IS_IDLE);
			nvram_set("u2ec_busyip", "");
			semaphore_post();
			u2ec_list_for_each(pos, &conn_info_list) {
				if (((PCONNECTION_INFO)pos)->busy == CONN_IS_BUSY)
					((PCONNECTION_INFO)pos)->busy = CONN_IS_IDLE;
			}
		case MFP_IS_IDLE:
			u2ec_list_for_each_safe(pos, tmp, &conn_info_list) {
				if (((PCONNECTION_INFO)pos)->busy == CONN_IS_RETRY) {
					nvram_set("u2ec_busyip", inet_ntoa(((PCONNECTION_INFO)pos)->ip));
					semaphore_post();
					last_busy_conn = (PCONNECTION_INFO)pos;
					((PCONNECTION_INFO)pos)->busy = CONN_IS_BUSY;
					break;
				}
				else if (((PCONNECTION_INFO)pos)->busy == CONN_IS_WAITING) {
					semaphore_post();
					PDEBUG("\nReset connection.\n");
					count_bulk_read = 0;
					count_bulk_write = 0;
					count_bulk_read_ret0_1 = 0;
					count_bulk_read_ret0_2 = 0;
					count_bulk_read_epson = 0;
					flag_monitor_epson = 0;
					flag_canon_state = 0;
					flag_canon_ok = 0;
					((PCONNECTION_INFO)pos)->count_class_int_in = 0;
					((PCONNECTION_INFO)pos)->ip.s_addr = 0;
					((PCONNECTION_INFO)pos)->irp = 0;
					FD_CLR(((PCONNECTION_INFO)pos)->sockfd, pfds);
					close(((PCONNECTION_INFO)pos)->sockfd);
					fd_in_use[((PCONNECTION_INFO)pos)->sockfd] = 0;
					((PCONNECTION_INFO)pos)->sockfd = 0;
					u2ec_list_del(pos);
					free(pos);
					break;
				}
			}
			semaphore_post();
		}
#if 0
		u2ec_list_for_each_safe(pos, tmp, &conn_info_list) {
			if (((PCONNECTION_INFO)pos)->irp == 0xc && (time((time_t*)NULL) - ((PCONNECTION_INFO)pos)->time) > U2EC_TIMEOUT) {
				if (!need_to_udpate_device)
					need_to_udpate_device = 1;

				PDEBUG("\nReset connection for irp number 0xc.\n");
				count_bulk_read = 0;
				count_bulk_write = 0;
				count_bulk_read_ret0_1 = 0;
				count_bulk_read_ret0_2 = 0;
				count_bulk_read_epson = 0;
				flag_monitor_epson = 0;
				flag_canon_state = 0;
				flag_canon_ok = 0;
				((PCONNECTION_INFO)pos)->count_class_int_in = 0;
				((PCONNECTION_INFO)pos)->ip.s_addr = 0;
				((PCONNECTION_INFO)pos)->irp = 0;
				FD_CLR(((PCONNECTION_INFO)pos)->sockfd, pfds);
				close(((PCONNECTION_INFO)pos)->sockfd);
				fd_in_use[((PCONNECTION_INFO)pos)->sockfd] = 0;
				((PCONNECTION_INFO)pos)->sockfd = 0;
				u2ec_list_del(pos);
				free(pos);
			}
		}
#endif
		if (need_to_udpate_device)
		{
			hotplug_debug("u2ec updating device for irp number 0xc...\n");
			update_device();
		}
	}
	/* Requeue such that the connection from specific ip address could be next busy one. */
#ifdef	SUPPORT_LPRng
	else if (c == 'q') {
		int count_move = 0;
		struct in_addr ip_requeue;
		struct u2ec_list_head	*pos_tmp, *pos_first = NULL, *pos_with_specific_ip = NULL;
	
		if (nvram_match("mfp_ip_requeue", ""))
			PDEBUG("ip for requeuing: invalid ip address!\n");
		else {
			PDEBUG("ip for requeuing: %s\n", nvram_safe_get("mfp_ip_requeue"));
			inet_aton(nvram_safe_get("mfp_ip_requeue"), &ip_requeue);
		}
	
		u2ec_list_for_each(pos, &conn_info_list) {
			if (((PCONNECTION_INFO)pos)->ip.s_addr == ip_requeue.s_addr) {
				pos_with_specific_ip = pos;
				break;
			}
		}
	
		if (pos_with_specific_ip != NULL) {
			u2ec_list_for_each(pos, &conn_info_list) {
				if (++count_move == 1) {
					pos_first = pos;
					if (((PCONNECTION_INFO)pos)->ip.s_addr == ip_requeue.s_addr)
						break;
				}
				else if (pos_first == pos)
					break;
	
				if (((PCONNECTION_INFO)pos)->busy != CONN_IS_BUSY &&
				    ((PCONNECTION_INFO)pos)->ip.s_addr != ip_requeue.s_addr) {	// requeue
					pos_tmp = pos->next;
					u2ec_list_del(pos);
					u2ec_list_add_tail(pos, &conn_info_list);
					pos = pos_tmp->prev;
				}
			}
	
			PDEBUG("************* new list *************\n");
			u2ec_list_for_each(pos, &conn_info_list) {
				PDEBUG("ip %s\n", inet_ntoa(((PCONNECTION_INFO)pos)->ip));
				PDEBUG("busy %d\n", ((PCONNECTION_INFO)pos)->busy);
			}
		}
	}
	else if (c == 'm') {
		if (time((time_t*)NULL) - time_monopoly_old < 3)
		{
			PDEBUG("time interval is too small!\n");
			goto CANCEL;
		}

		if (nvram_match("mfp_ip_monopoly", ""))
			PDEBUG("ip for monopoly: invalid ip address!\n");
		else {
			PDEBUG("ip for monopoly: %s\n", nvram_safe_get("mfp_ip_monopoly"));
			inet_aton(nvram_safe_get("mfp_ip_monopoly"), &ip_monopoly);
			time_monopoly = time_monopoly_old = time((time_t*)NULL);
		}

		if (MFP_state(MFP_GET_STATE) == MFP_IN_U2EC)
		{
			MFP_state(MFP_IS_IDLE);
			nvram_set("u2ec_busyip", "");
		}
		semaphore_post();
		alarm(0);

		u2ec_list_for_each_safe(pos, tmp, &conn_info_list) 
			if (((PCONNECTION_INFO)pos)->ip.s_addr != ip_monopoly.s_addr || ((PCONNECTION_INFO)pos)->irp <= 0xc)
			{
				PDEBUG("\nReset connection for monopoly, ip: %s, socket: %d\n", inet_ntoa(((PCONNECTION_INFO)pos)->ip), ((PCONNECTION_INFO)pos)->sockfd);
				count_bulk_read = 0;
				count_bulk_write = 0;
				count_bulk_read_ret0_1 = 0;
				count_bulk_read_ret0_2 = 0;
				count_bulk_read_epson = 0;
				flag_monitor_epson = 0;
				flag_canon_state = 0;
				flag_canon_ok = 0;
				((PCONNECTION_INFO)pos)->count_class_int_in = 0;
				((PCONNECTION_INFO)pos)->ip.s_addr = 0;
				((PCONNECTION_INFO)pos)->irp = 0;
				FD_CLR(((PCONNECTION_INFO)pos)->sockfd, pfds);
				close(((PCONNECTION_INFO)pos)->sockfd);
				fd_in_use[((PCONNECTION_INFO)pos)->sockfd] = 0;
				((PCONNECTION_INFO)pos)->sockfd = 0;
				u2ec_list_del(pos);
				free(pos);
			}
	}
CANCEL:
#endif
	*fd = open(U2EC_FIFO, O_RDONLY|O_NONBLOCK);
	FD_SET(*fd, pfds);
	*pfdm = *fd > *pfdm ? *fd : *pfdm;

	return rtvl;
}

int main(int argc, char *argv[])
{
	pthread_t	find_thread;		// a thread for get config and get name
	int		fifo_fd;		// fifo fd
	int		conn_fd, new_fd;	// socket fd
	fd_set		master_fds;		// master file descriptor list
	fd_set		read_fds;		// temp file descriptor list for select()
	int		fdmax;			// maximum file descriptor number
	int		index, rtvl;
	FILE		*fp;
	sigset_t sigs_to_catch;

#if defined(U2EC_DEBUG) && defined(U2EC_ONPC)
	/* Decode packets. */
	if (argc > 1) {
		if (argc > 2)
			freopen(argv[2], "w", stdout);
		decode(argv[1]);
		return 0;
	}

	/* Open log file. */
	if ((fp = fopen("U2EC_log.txt", "w")) == NULL) {
		printf("U2EC: can't open log file!\n");
		fp = stderr;
	} else
		PDEBUG(" ***** U2EC_log.txt *****\n\n");
#endif

	printf("U2EC starting ...\n");

	semaphore_create();
#ifdef	SUPPORT_LPRng
	semaphore_wait();
	nvram_set("MFP_busy", "0");
	nvram_set("u2ec_device", "");
	nvram_set("u2ec_busyip", "");
#if __BYTE_ORDER == __BIG_ENDIAN
	nvram_commit();
#endif
	semaphore_post();
#endif
	FD_ZERO(&master_fds);
	FD_ZERO(&read_fds);

	hotplug_debug("u2ec self updating...\n");
	update_device();

	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGALRM, multi_client);

	/* Handle get ip, get config and get name. */
	pthread_create(&find_thread, NULL, (void *)add_remote_device, NULL);

	/* Fifo used by service_ex.c:hotplug_usb(). */
	unlink(U2EC_FIFO);
#if __BYTE_ORDER == __BIG_ENDIAN
	umask(0000);
#endif
	mkfifo(U2EC_FIFO, 0666);
	fifo_fd = open(U2EC_FIFO, O_RDONLY|O_NONBLOCK);
	FD_SET(fifo_fd, &master_fds);
	fdmax = fifo_fd;
	fd_in_use[fifo_fd] = 1;
	ip_monopoly.s_addr = 0;

	/* The listener for establish_connection. */
	if ((conn_fd = stream_sock_init(uTCPUSB)) < 0) {
		PERROR("connection: can't bind to any address\n");
		return 1;
	}
	else
		fd_in_use[conn_fd] = 1;

	/* write pid */
	if ((fp=fopen("/var/run/u2ec.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	if (dev) {
		FD_SET(conn_fd, &master_fds);
		fdmax = conn_fd > fdmax ? conn_fd : fdmax;
	}

	PDEBUG("\nU2EC initialized, listening...\n");

	for (;;) {
		read_fds = master_fds;
		while ((rtvl = select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) && (errno == EINTR));
		if (rtvl == -1) {
			PERROR("select");
			return 1;
		}

		if (ip_monopoly.s_addr && (time((time_t*)NULL) - time_monopoly) > U2EC_TIMEOUT_MONO)
		{
			PDEBUG("disable monopoly for timeout\n");
			ip_monopoly.s_addr = 0;
			nvram_set("mfp_ip_monopoly", "");

			int u2ec_fifo = open(U2EC_FIFO, O_WRONLY|O_NONBLOCK);
			write(u2ec_fifo, "c", 1);
			close(u2ec_fifo);
		}

		// run through the existing connections looking for data to read
		for (index = 0; index <= fdmax; index++) {
			if (!FD_ISSET(index, &read_fds))
				continue;

			if (index == fifo_fd) {
				if (handle_fifo(&fifo_fd, &master_fds, &fdmax, conn_fd) == 1)
					break;
			} else if (index == conn_fd) {
				if (!dev)
				{
					FD_CLR(index, &master_fds);
					continue;
				}

				if ((new_fd = establish_connection(conn_fd)) < 0)
					continue;

				fd_in_use[new_fd] = 1;
				FD_SET(new_fd, &master_fds);
				fdmax = new_fd > fdmax ? new_fd : fdmax;
			} else if (!fd_in_use[index] || usb_connection(index) < 0) {
				FD_CLR(index, &master_fds);	// remove from master set
			}
		}
	}

#if defined(U2EC_DEBUG) && defined(U2EC_ONPC)
	fclose(fp);
#endif

	return 0;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define LOG_FILE "/tmp/hotplug_usb"
#else
#define LOG_FILE "/var/hotplug_usb"
#endif
#define MAX_LOGFILE_LINES 512
static char *logFilePath = LOG_FILE;
static char logFilePathBackup[64];
static int logFileLineCount = 0;

void hotplug_print(const char *format)
{
	FILE *fp;

	if (logFileLineCount == MAX_LOGFILE_LINES)
	{
		logFileLineCount = 0;
		sprintf(logFilePathBackup,"%s-1",logFilePath);
		unlink(logFilePathBackup);
		rename(logFilePath, logFilePathBackup);
	}
	logFileLineCount++;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	if ((fp = fopen(logFilePath, "a")))
#else
	if ((fp = fopen(logFilePath, "a")))
#endif
	{
		fprintf(fp, format);
		fclose(fp);
	}
}

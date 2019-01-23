#ifndef _USB_BUS_STATS_H_
#define _USB_BUS_STATS_H_

#ifndef USB_MAXBUS
#define USB_MAXBUS		64
#endif

struct usb_bus_stat_s {
	unsigned long tx_bytes;		/* host -> device bytes. */
	unsigned long rx_bytes;		/* device -> host bytes. (URB_DIR_IN) */
};

extern struct usb_bus_stat_s usb_bus_stat[USB_MAXBUS];

#endif	/* !_USB_BUS_STATS_H_ */

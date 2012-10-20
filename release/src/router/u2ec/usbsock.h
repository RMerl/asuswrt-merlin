/*
 * U2EC usbsock header
 *
 * Copyright (C) 2008 ASUSTeK Corporation
 *
 */

#ifndef   __USBSOCK_H__
#define   __USBSOCK_H__

#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <u2ec_list.h>

#define SWAP8(x) \
({ \
        __u8 __x = (x); \
        ((__u8)( \
                (((__u8)(__x) & (__u8)0x80) >> 7) | \
                (((__u8)(__x) & (__u8)0x40) >> 5) | \
                (((__u8)(__x) & (__u8)0x20) >> 3) | \
                (((__u8)(__x) & (__u8)0x10) >> 1) | \
                (((__u8)(__x) & (__u8)0x08) << 1) | \
                (((__u8)(__x) & (__u8)0x04) << 3) | \
                (((__u8)(__x) & (__u8)0x02) << 5) | \
                (((__u8)(__x) & (__u8)0x01) << 7) )); \
})

#define SWAP16(x) \
({ \
        __u16 __x = (x); \
        ((__u16)( \
                (((__u16)(__x) & (__u16)0x00ffU) << 8) | \
                (((__u16)(__x) & (__u16)0xff00U) >> 8) )); \
})

#define SWAP32(x) \
        ((__u32)( \
                (((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
                (((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
                (((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
                (((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

#define SWAP64(x) \
({ \
        __u64 __x = (x); \
        ((__u64)( \
                (__u64)(((__u64)(__x) & (__u64)0x00000000000000ffULL) << 56) | \
                (__u64)(((__u64)(__x) & (__u64)0x000000000000ff00ULL) << 40) | \
                (__u64)(((__u64)(__x) & (__u64)0x0000000000ff0000ULL) << 24) | \
                (__u64)(((__u64)(__x) & (__u64)0x00000000ff000000ULL) <<  8) | \
                (__u64)(((__u64)(__x) & (__u64)0x000000ff00000000ULL) >>  8) | \
                (__u64)(((__u64)(__x) & (__u64)0x0000ff0000000000ULL) >> 24) | \
                (__u64)(((__u64)(__x) & (__u64)0x00ff000000000000ULL) >> 40) | \
                (__u64)(((__u64)(__x) & (__u64)0xff00000000000000ULL) >> 56) )); \
})

#define SWAP_BACK8(x) \
({ \
        __u8 __x = (x); \
        ((__u8)( \
                (((__u8)(__x) & (__u8)0x80) >> 7) | \
                (((__u8)(__x) & (__u8)0x40) >> 5) | \
                (((__u8)(__x) & (__u8)0x20) >> 3) | \
                (((__u8)(__x) & (__u8)0x10) >> 1) | \
                (((__u8)(__x) & (__u8)0x08) << 1) | \
                (((__u8)(__x) & (__u8)0x04) << 3) | \
                (((__u8)(__x) & (__u8)0x02) << 5) | \
                (((__u8)(__x) & (__u8)0x01) << 7) )); \
})

#define SWAP_BACK16(x) \
        ((__u16)( \
                (((__u16)(x) & (__u16)0x00ffU) << 8) | \
                (((__u16)(x) & (__u16)0xff00U) >> 8) ))
#define SWAP_BACK32(x) \
        ((__u32)( \
                (((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
                (((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
                (((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
                (((__u32)(x) & (__u32)0xff000000UL) >> 24) ))
#define SWAP_BACK64(x) \
        ((__u64)( \
                (__u64)(((__u64)(x) & (__u64)0x00000000000000ffULL) << 56) | \
                (__u64)(((__u64)(x) & (__u64)0x000000000000ff00ULL) << 40) | \
                (__u64)(((__u64)(x) & (__u64)0x0000000000ff0000ULL) << 24) | \
                (__u64)(((__u64)(x) & (__u64)0x00000000ff000000ULL) <<  8) | \
                (__u64)(((__u64)(x) & (__u64)0x000000ff00000000ULL) >>  8) | \
                (__u64)(((__u64)(x) & (__u64)0x0000ff0000000000ULL) >> 24) | \
                (__u64)(((__u64)(x) & (__u64)0x00ff000000000000ULL) >> 40) | \
                (__u64)(((__u64)(x) & (__u64)0xff00000000000000ULL) >> 56) ))

#define U2EC_FIFO	"/var/u2ec_fifo"	// used by handle_fifo

#define MFP_IS_IDLE	0			// used by MFP_state
#define MFP_IN_LPRNG	1
#define MFP_IN_U2EC	2
#define MFP_GET_STATE	7

#define U2EC_TIMEOUT	6
#define U2EC_TIMEOUT_MONO	300
#define MAX_BUFFER_SIZE	262144 - 104 - 136 - 54	// irp: 104 urb: 136 tcp: 54
#define MAX_BUF_LEN	MAX_BUFFER_SIZE	+ 104 + 136
#define BACKLOG		15			// how many pending connections queue will hold

#define uTcpPortConfig	5473
#define uTcpUdp		5474
#define uTcpControl	5475
#define uTCPUSB		3394			// server shared TCP port, set when server share device

#define USB_DIR_OUT	USB_ENDPOINT_OUT	// to device
#define USB_DIR_IN	USB_ENDPOINT_IN		// to host

#define USBLP_REQ_GET_ID			0x00
#define USBLP_REQ_GET_STATUS			0x01
#define USBLP_REQ_RESET				0x02
#define USBLP_REQ_HP_CHANNEL_CHANGE_REQUEST	0x00	// HP Vendor-specific

#define USBLP_DEVICE_ID_SIZE	1024

#define FILE_DEVICE_UNKNOWN	0x00000022
#define METHOD_NEITHER		3
#define FILE_ANY_ACCESS		0

#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#define IOCTL_INTERNAL_USB_SUBMIT_URB  CTL_CODE(FILE_DEVICE_USB,  \
                                                USB_SUBMIT_URB,  \
                                                METHOD_NEITHER,  \
                                                FILE_ANY_ACCESS)

enum TypeTcpPackage {
	IrpToTcp,
	IrpAnswerTcp,
	ControlPing
};

typedef struct _CONNECTION_INFO {
	struct u2ec_list_head	list;		// descriptor list head
	int			sockfd;		// connection socket fd
	time_t			time;		// receive last packet from the socket
	struct in_addr		ip;		// client ip address
	int			busy;		// connection state defined as below
	int			count_class_int_in;	// toolbox threshold for HP DeskJet series
	LONG			irp;		// irp number of last packet
#define	CONN_IS_IDLE		0
#define	CONN_IS_BUSY		1
#define	CONN_IS_WAITING		2
#define	CONN_IS_RETRY		3
}CONNECTION_INFO, *PCONNECTION_INFO;

typedef struct _TCP_PACKAGE {
	LONG	Type;
	LONG	SizeBuffer;
}TCP_PACKAGE, *PTCP_PACKAGE;

typedef struct _IO_STACK_LOCATION_SAVE {
	ULONG64	empty;
	UCHAR	MajorFunction;
	UCHAR	MinorFunction;

	union {
		struct {
			ULONG64 Argument1;
			ULONG64 Argument2;
			ULONG64 Argument3;
			ULONG64 Argument4;
		} Others;
/*
        	struct {
            		ULONG64 OutputBufferLength;
            		ULONG64 InputBufferLength;
            		ULONG64 IoControlCode;
            		ULONG64 Type3InputBuffer;
        	} DeviceIoControl;
*/
	} Parameters;

}__attribute((aligned (8))) IO_STACK_LOCATION_SAVE, *PIO_STACK_LOCATION_SAVE;

typedef struct _IRP_SAVE
{
	LONG			Size;		// size current buffer
	LONG			NeedSize;	// need size buffer
	LONG64			Device;		// Indefication of device
	BYTE			Is64:1;		// Detect 64
	BYTE			IsIsoch:1;      // isoch data
	BYTE			Res1:6;         // Reserv
	BYTE			empty[3];
	LONG			Irp;		// Number Iro
	NTSTATUS		Status;		// current status Irp
	ULONG64   		Information;
	BOOL			Cancel;		// Cancel irp flag
	IO_STACK_LOCATION_SAVE	StackLocation;	// Stack location info
	LONG			BufferSize;
	LONG			Reserv;
	BYTE			Buffer [8];	// Data
}__attribute((packed)) IRP_SAVE, *PIRP_SAVE;

typedef struct _IRP_SAVE_SWAP
{
	LONG			Size;		// size current buffer
	LONG			NeedSize;	// need size buffer
	LONG64			Device;		// Indefication of device
	BYTE			BYTE;
	BYTE			empty[3];
	LONG			Irp;		// Number Iro
	NTSTATUS		Status;		// current status Irp
	ULONG64   		Information;
	BOOL			Cancel;		// Cancel irp flag
	IO_STACK_LOCATION_SAVE	StackLocation;	// Stack location info
	LONG			BufferSize;
	LONG			Reserv;
	BYTE			Buffer [8];	// Data
}__attribute((packed)) IRP_SAVE_SWAP, *PIRP_SAVE_SWAP;

#ifdef	SUPPORT_LPRng
#	define	semaphore_create()	semaphore_create()
#	define	semaphore_wait()	semaphore_wait()
#	define	semaphore_post()	semaphore_post()
#	define	semaphore_close()	semaphore_close()
#else
#	define	semaphore_create()
#	define	semaphore_wait()
#	define	semaphore_post()
#	define	semaphore_close()
#endif

// Print log on console or file or nothing.
#ifdef	PDEBUG_SENDSECV
#	define	PSNDRECV(fmt, args...) PDEBUG(fmt, ## args)
#else
#	define	PSNDRECV(fmt, args...)
#endif

#ifdef	PDEBUG_DECODE
#	define	PDECODE_IRP(arg1, arg2) print_irp(arg1, arg2)
#else
#	define	PDECODE_IRP(arg1, arg2)
#endif

#undef PDEBUG
#ifdef U2EC_DEBUG
#	define hotplug_debug(arg) hotplug_print(arg)
#	ifdef U2EC_ONPC
		extern FILE *fp;
//#		define PDEBUG(fmt, args...) fprintf(fp, fmt, ## args)
#		define PDEBUG(fmt, args...) printf(fmt, ## args)
#	else
//#		define PDEBUG(fmt, args...) printf(fmt, ## args)
#		define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
//#		define PDEBUG(fmt, args...) do { \
//			FILE *fp = fopen("/dev/console", "w"); \
//			if (fp) { \
//				fprintf(fp, fmt, ## args); \
//				fclose(fp); \
//			} \
//		} while (0)
#	endif
#	define PERROR(message) PDEBUG("sock error: %s.\n", message)
#else
#	define hotplug_debug(arg)
#	define PDEBUG(fmt, args...)
#	define PERROR perror
#endif	// U2EC_DEBUG

#endif /*  __USBSOCK_H__ */

#ifndef __WSC_COMMON_H__
#define __WSC_COMMON_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>


#define WSC_VERSION   "0.1.1"


typedef unsigned char 	uint8;
typedef unsigned short	uint16;
typedef unsigned int 	uint32;
typedef signed char 	int8;
typedef signed short	int16;
typedef signed int		int32;

#ifndef PACKED
#define PACKED  __attribute__ ((packed))
#endif

#ifndef IFLA_IFNAME
#define IFLA_IFNAME 3
#endif
#ifndef IFLA_WIRELESS
#define IFLA_WIRELESS 11
#endif

#ifndef ASSERT
#define ASSERT(expr)	\
	do{\
		if(!(expr)) \
			printf("%s(%d): ASSERTION Error!\n", __FUNCTION__, __LINE__); \
	}while(0);
#endif

#ifndef RT_DEBUG
#define DBGPRINTF(args...) do{}while(0)
#else
//void DBGPRINTF(int level, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void DBGPRINTF(int level, char *fmt, ...);
#endif


#ifndef IFNAMSIZ
#define	IFNAMSIZ	16
#endif

#define MAC_ADDR_LEN 			6
#define LENGTH_802_1_H			8


#define BIT(x)	(1<<x)

extern int WscUPnPOpMode;

extern char WSC_IOCTL_IF[IFNAMSIZ];
extern unsigned char HostMacAddr[MAC_ADDR_LEN];	// Used to save the MAC address of local host.


#define DEFAULT_PID_FILE_PATH		"/var/run/wscd.pid"

#define USE_XML_TEMPLATE
#define DEFAULT_WEB_ROOT_DIR	"/etc_ro/xml/"
#define DEFAULT_DESC_FILE_NAME	"WFADeviceDesc.xml"


#define WSC_SYS_ERROR	(-1)
#define WSC_SYS_SUCCESS 0

typedef enum{
	UPNP_OPMODE_DISABLE = 0,
	UPNP_OPMODE_DEV = 1,
	UPNP_OPMODE_CP = 2,
	UPNP_OPMODE_BOTH = 3
}WSC_UPNP_OPMODE;


typedef enum{
	RT_DBG_OFF		= 0,
	RT_DBG_ERROR	= 1,
	RT_DBG_PKT		= 2,
	RT_DBG_INFO		= 3,
	RT_DBG_ALL
}WSC_DEBUG_LEVEL;

// 802.1x authentication format
#define IEEE8021X_FRAME_VERSION		1
#define IEEE8021X_FRAME_TYPE_EAP	0
typedef	struct	PACKED _IEEE8021X_FRAME{
	uint8	Version;					// 1.0
	uint8	Type;						// 0 = EAP Packet
	uint16	Length;
}IEEE8021X_FRAME, *PIEEE8021X_FRAME;


// EAP frame format
typedef enum{
	EAP_FRAME_CODE_REQUEST = 0x1,
	EAP_FRAME_CODE_RESPONSE = 0x2
}EAP_FRAME_CODE;

typedef enum{
	EAP_FRAME_TYPE_IDENTITY = 0x1,
	EAP_FRAME_TYPE_WSC = 0xfe,
}EAP_FRAME_TYPE;

// EAP frame format
typedef	struct PACKED _EAP_FRAME{
	uint8	Code;						// 1 = Request, 2 = Response
	uint8	Id;
	uint16	Length;
	uint8	Type;						// 1 = Identity, 0xfe = reserved, used by WSC
}EAP_FRAME, *PEAP_FRAME;


// KernelSpace 2 UserSpace msg header
#define WSC_K2UMSG_FLAG_SUCCESS		BIT(0)
#define WSC_K2UMSG_FLAG_ERROR		BIT(1)
#define WSC_K2UMSG_FLAG_

#define RTMP_WSC_NLMSG_HDR_LEN		30		//signature(8) + envID(4) + ackID(4) + msgLen(4) + Flag(2) + segLen(2) + devAddr(6)
typedef struct PACKED _RTMP_WSC_NLMSG_HDR{
	char	signature[8];	/* Signature used to identify that this's a Ralink specific NETLINK message. 
								MUST be "RAWSCMSG" currently.
							*/
	uint32	envID;			// Unique event Identification assigned by sender.
	uint32	ackID;			// Notify that this message is a repsone for the message whose event identifier is "ackID".
	uint32	msgLen;			// Totally length for this message. This message may seperate in serveral packets.
	uint16	flags;			
	uint16	segLen;			/* The "segLen" means the actual data length in this one msg packet.
								Because the NETLINK socket just support 256bytes for "IWCUSTOM" typed message, so we may 
								need to do fragement for our msg. If one message was fragemented as serveral pieces, the 
								user space receiver need to re-assemble it.
							 */
#ifdef MULTIPLE_CARD_SUPPORT
	unsigned char devAddr[MAC_ADDR_LEN];		// MAC address who send this netlink msg.
#endif // MULTIPLE_CARD_SUPPORT //
}RTMP_WSC_NLMSG_HDR;

#define RTMP_WSC_MSG_HDR_LEN		12	//msgType(2) + msgSubType(2) + ipAddr(4) + len(4)
typedef struct PACKED _RTMP_WSC_MSG_HDR{
	uint16	msgType;
	uint16	msgSubType;
	uint32	ipAddr;
	uint32	msgLen;		//Not include this header.
}RTMP_WSC_MSG_HDR;

/*
  1. This data structure used for UPnP daeom send WSC_MSG to wireless driver in Kernel space.
  2. This data structure must sync with Ralink wireless driver( defined in "wsc.h").
  3. The size of this structure is equal to the (802.11 header+802.1h header+802.1x header+EAP header).
  4. The Addr1 must set as all zero for notify the kernel driver that this packet was sent by UPnP daemon.
	  (Because it's imposssible that AP receive a wireless packet from station whose addr1=0)
  5. Please don't use sizeof(struct _WscIoctlMsgHdr) unless you "PACK" this data structure in kernel and here.
*/
#define WSC_U2KMSG_HDR_LEN	41	// HeaderLen = LENGTH_802_11(24) + LENGTH_802_1_H(8) + IEEE8021X_FRAME_HDR(4) + EAP_FRAME_HDR(5)
typedef	struct PACKED _WSC_U2KMSG_HDR{
	uint32				envID;					//Event ID.
	char				Addr1[MAC_ADDR_LEN];	//RA, should be the MAC address of the AP.
	char				Addr2[MAC_ADDR_LEN];	//TA, should be the ipAddress of remote UPnP Device/CotrnolPoint.
	char				Addr3[MAC_ADDR_LEN];	//DA, Not used now.
	char				rsvWLHdr[2];			//Reserved space for remained 802.11 hdr content.
	char				rsv1HHdr[LENGTH_802_1_H];//Reserved space for 802.1h header
	IEEE8021X_FRAME 	IEEE8021XHdr;			//802.1X header
	EAP_FRAME			EAPHdr;					//EAP frame header.
}RTMP_WSC_U2KMSG_HDR;


void wsc_hexdump(char *title, char *ptr, int len);

extern int ioctl_sock;
extern int stopThread;

int ILibBase64Encode(unsigned char* input, const int inputlen, unsigned char** output);
int ILibBase64Decode(unsigned char* input, const int inputlen, unsigned char** output);


int wscK2UModuleInit(void);
int wscU2KModuleInit(void);
int wsc_set_oid(uint16 oid, char *data, int len);
int wsc_get_oid(uint16 oid, char *data, int *len);

#endif


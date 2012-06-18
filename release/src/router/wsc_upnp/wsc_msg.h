#ifndef __WSC_MSG_H__
#define __WSC_MSG_H__

#include "wsc_common.h"

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#define WSC_OPCODE_UPNP_MASK	0x10
#define WSC_OPCODE_UPNP_DATA	0x11
#define WSC_OPCODE_UPNP_MGMT	0x12
#define WSC_OPCODE_UPNP_CTRL	0x13

#define WSC_UPNP_MGMT_SUB_PROBE_REQ		0x01
#define WSC_UPNP_MGMT_SUB_CONFIG_REQ	0x02
#define WSC_UPNP_MGMT_SUB_REG_SELECT	0x03

//patch for Atheros External registrar
#define WSC_UPNP_DATA_SUB_INCLUDE_MAC	0x0100

#define WSC_UPNP_DATA_SUB_NORMAL		0x00
#define WSC_UPNP_DATA_SUB_TO_ALL		0x01
#define WSC_UPNP_DATA_SUB_TO_ONE		0x02
#define WSC_UPNP_DATA_SUB_ACK			0x03
#define WSC_UPNP_DATA_SUB_M1			0x04
#define WSC_UPNP_DATA_SUB_M2			0x05
#define WSC_UPNP_DATA_SUB_M2D			0x06
#define WSC_UPNP_DATA_SUB_M3			0x07
#define WSC_UPNP_DATA_SUB_M4			0x08
#define WSC_UPNP_DATA_SUB_M5			0x09
#define WSC_UPNP_DATA_SUB_M6			0x0A
#define WSC_UPNP_DATA_SUB_M7			0x0B
#define WSC_UPNP_DATA_SUB_M8			0x0C
#define WSC_UPNP_DATA_SUB_WSC_ACK		0x0D
#define WSC_UPNP_DATA_SUB_WSC_NACK		0x0E
#define WSC_UPNP_DATA_SUB_WSC_DONE		0x0F
#define WSC_UPNP_DATA_SUB_WSC_UNKNOWN	0xff

#define WSC_ENVELOPE_NONE			0x0
#define WSC_ENVELOPE_SUCCESS		0x1
#define WSC_ENVELOPE_MEM_FAILED		0x2
#define WSC_ENVELOPE_TIME_OUT		0x3
/*
	This data structrue used for communication between UPnP threads and netlink thread.
	The UPnP thread create a 
*/
#define Q_TYPE_ACTIVE	1
#define Q_TYPE_PASSIVE	2

#define DEF_WSC_ENVELOPE_TIME_OUT	30
typedef int (*envCallBack)(char *msg, int msgLen);

typedef struct _WscEnvelope{
	uint32		envID;			//envID = 0 was reserved for msg directly send from kernel space. other value means send by upnp daemon.
	void 		*pMsgPtr;
	int 		msgLen;
	int 		actLen;
	int 		flag;
	int			timerCount;		
	envCallBack callBack;		//Used for UPnP Control Point.
	pthread_cond_t condition;	//Used for UPnP Device.
	pthread_mutex_t mutex;
}WscEnvelope;

typedef struct _WscU2KMsgQ{
	WscEnvelope *pEnvelope;
	int used;
}WscU2KMsgQ;

typedef int (*msgHandleCallback)(char *pMsgBuf, int msgLen);
typedef struct wscMsgHandle{
	uint32 msgType;
	msgHandleCallback handle;
}WscMsgHandle;

int wscEnvelopeFree(IN WscEnvelope *envelope);

WscEnvelope *wscEnvelopeCreate(void);

int wscEnvelopeRemove(
	IN WscEnvelope *envelope,
	OUT int qIdx);

int wscMsgQInsert(
	IN WscEnvelope *envelope,
	OUT int *pQIdx);

int wscMsgQRemove(
	IN int qType, 
	IN int qIdx);

int WscRTMPMsgHandler(
	IN char *pBuf,
	IN int len);

int wscMsgSubSystemStop(void);

int wscMsgSubSystemInit(void);

#endif //endif of "#ifndef __WSC_MSG_H__"

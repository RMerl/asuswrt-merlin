/*


*/
#ifndef __WSC_UPNP_H__
#define __WSC_UPNP_H__

#ifdef __cplusplus
extern "C" {
#endif


#define WSC_UPNP_DESC_URL_LEN	200	

extern char HostDescURL[WSC_UPNP_DESC_URL_LEN];	// Used to save the DescriptionURL of local host.
extern unsigned int HostIPAddr;						// Used to save the binded IP address of local host.

/* 
  Wsc Event format and Service State variable names, values, and defaults.
	WLANEvent: 
		This state variable event indicates that a WCN-NET Probe-Request or an 802.1X/WCN-NET 
		EAP frame was received on the 802.11 network. The proxy service issues the event.
	APStatus:
		This state variable event indicates that the AP has either had a configuration change 
		or that the AP has had too many failed authentications against its PIN/password and has 
		locked itself out of Enrollee mode.
	STAStatus:
		This state variable event indicates that the STA has either had a configuration change 
		or that the STA has had too many failed authentications against its PIN/password and 
		has locked itself out of Enrollee mode.
*/
#define WSC_STATE_VAR_COUNT		3
#define WSC_STATE_VAR_MAXVARS	WSC_STATE_VAR_COUNT
#define WSC_STATE_VAR_MAX_STR_LEN 1024	// Max value length. TODO: shiang: shall we need this??

#define WSC_EVENT_WLANEVENT		0		// Index of "WLANEvent" State variable
#define WSC_EVENT_APSTATUS		1		// Index of "APStatus" State variable
#define WSC_EVENT_STASTATUS		2		// Index of "STAStatus" State variable


/*****************************************************************************
	Wsc Control Point Related data structures.
 *****************************************************************************/
#define NAME_SIZE  256	//refer to UPnP Library
struct upnpService{
	char ServiceId[NAME_SIZE];			//Service identification
	char ServiceType[NAME_SIZE];		//Service type.
	char SCPDURL[NAME_SIZE];			//URL to service description
	char EventURL[NAME_SIZE];			//URL for eventing
	char ControlURL[NAME_SIZE];			//URL for control
    char SID[NAME_SIZE];				//Subscribe Identifier
	char *StateVarVal[WSC_STATE_VAR_COUNT];
	struct upnpService *next;			//Pointer to next service.
};

/*
	The following data structure used to save the UPnP Device which
	support WSC.
*/
struct upnpDevice
{
	char UDN[NAME_SIZE];				//uuid
	char DescDocURL[NAME_SIZE];			//URL of the device description file
	char FriendlyName[NAME_SIZE];		//short user-friendly title
	char PresURL[NAME_SIZE];			//URL for presentation
	int  AdvrTimeOut;					//Device Advertisement timeout
	int timeCount;
	unsigned int ipAddr;				//IP address of this device
	struct upnpService services;		//Wsc Device Service related info
};

struct upnpDeviceNode {
	struct upnpDevice device;
	struct upnpDeviceNode *next;
};


/*
	The following data structure used to save the UPnP Contorl Point 
	which subscribe to us.
*/
struct upnpCtrlPoint
{
	char SubURL[NAME_SIZE];				//URL for subscription
	char SID[NAME_SIZE];				//Subscription ID of the Control Point
	int  SubTimeOut;					//Subscription time-out
	unsigned int ipAddr;				//IP address of this control point
};

struct upnpCPNode {
	struct upnpCtrlPoint device;
	struct upnpCPNode *next;
};


/* Device type for wsc device  */
#define WscDeviceTypeStr	"urn:schemas-wifialliance-org:device:WFADevice:1"

/* Service type for wsc services */
#define WscServiceTypeStr	"urn:schemas-wifialliance-org:service:WFAWLANConfig:1"

/* Service Id for wsc services */
#define WscServiceIDStr		"urn:wifialliance-org:serviceId:WFAWLANConfig1"

typedef enum{
	ERR_UPNP_WSC_ACTION_ACT = 401,
	ERR_UPNP_WSC_ACTION_ARG = 402,
	ERR_UPNP_WSC_ACTION_VAR = 403,
	ERR_UPNP_WSC_ACTION_FAILED = 501,
}WSC_UPNP_ACTION_ERRCODE;


int WscEventCtrlMsgRecv(
	IN char *pBuf,
	IN int  bufLen);

int WscEventDataMsgRecv(
	IN char *pBuf,
	IN int  bufLen);

int WscEventMgmtMsgRecv(
	IN char *pBuf,
	IN int  bufLen);

int wscU2KMsgCreate(
	INOUT char **dstPtr,
	IN char *srcPtr,
	IN int msgLen,
	IN int EAPType);


int WscUPnPDevStop(void);
int WscUPnPDevStart(
	IN char *ipAddr,
	IN unsigned short port,
	IN char *descDoc,
	IN char *webRootDir);


int WscUPnPCPStop(void);
int WscUPnPCPStart(
	IN char *ip_address,
	IN unsigned short port);

#ifdef __cplusplus
}
#endif

#endif

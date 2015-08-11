#ifndef __WS_API_H__
#define __WS_API_H__

#define TRANSFER_TYPE		"https://"
//#define SERVER 				"aae-spweb.asus.com"
#define SERVER 				"aae-spweb.asuscloud.com"
#define GET_SERVICE_AREA	"/aae/getservicearea"
#define LOGIN				"/aae/login"
#define QUERY_FRIEND		"/aae/queryfriend"
#define LIST_PROFILE		"/aae/listprofile"
#define UPDATE_PROFILE		"/aae/updateprofile"
#define LOGOUT				"/aae/logout"
#define GET_WEB_PATH		"/aae/getwebpath"
#define CREATE_PIN			"/aae/createpin"
#define QUERY_PIN			"/aae/querypin"
#define UNREGISTER			"/aae/unregister"
#define UPDATE_ICE_INFO		"/aae/updateiceinfo"
#define KEEP_ALIVE		"/aae/keepalive"
#define PUSH_SENDMSG	"/aae/wlpush_sendmsg"


#define MAX_STATUS_LEN 	32
#define MAX_URL_LEN		128
#define MAX_TIME_LEN	64
#define MAX_USER_LEVEL_LEN 	64
#define MAX_CUSID_LEN		64	
#define MAX_TICKET_LEN		64
#define MAX_NICKNAME_LEN	64
#define MAX_DEVICEID_LEN	64
#define MAX_DEVTICKET_LEN	64
#define MAX_RELAY_INFO_LEN	MAX_URL_LEN
#define MAX_RELAY_IP_LEN	MAX_URL_LEN
#define MAX_STUN_INFO_LEN	MAX_URL_LEN
#define MAX_TURN_INFO_LEN	MAX_URL_LEN
#define MAX_DEV_TICKET_EXP_LEN 	64
#define MAX_ID_LEN		64
#define MAX_STATUS_LEN		32
#define MAX_DESC_LEN		64
#define MAX_PIN_LEN		64
#define MAX_IP_LEN		128 	//32

typedef struct _GetServiceArea
{
	char 	status[MAX_STATUS_LEN];
	char 	servicearea[MAX_URL_LEN];
	char	time[MAX_TIME_LEN];   
}GetServiceArea, *pGetServiceArea;

typedef struct _SrvInfo
{
	struct _SrvInfo* next;
	char	srv_ip[MAX_IP_LEN];
} SrvInfo, *pSrvInfo;

typedef struct _Login{
	char 	status[MAX_STATUS_LEN];
	char	usersvclevel[MAX_USER_LEVEL_LEN];
	char	cusid[MAX_CUSID_LEN];
	char	userticket[MAX_TICKET_LEN]; 
	char	usernickname[MAX_NICKNAME_LEN];
	char	ssoflag[2];
	char	deviceid[MAX_DEVICEID_LEN];
	char	deviceticket[MAX_DEVTICKET_LEN];
//	char	relayinfo[MAX_RELAY_INFO_LEN];
//	char	relayip[MAX_RELAY_IP_LEN];
//	char	stuninfo[MAX_STUN_INFO_LEN];
//	char	turninfo[MAX_TURN_INFO_LEN];
	pSrvInfo	relayinfoList;
	pSrvInfo	stuninfoList;
	pSrvInfo	turninfoList;
	char	deviceticketexpiretime[MAX_DEV_TICKET_EXP_LEN];
	char 	time[MAX_TIME_LEN];
} Login, *pLogin;

typedef struct _Friends{
	struct _Friends* 	next;
	char 		userid[MAX_ID_LEN];
	char 		cusid[MAX_ID_LEN];
	char		nickname[MAX_ID_LEN];
} Friends, *pFriends;

typedef struct _QueryFriend{
	char 		status[MAX_STATUS_LEN];
	char		time[MAX_TIME_LEN];
	pFriends	FriendList;
}QueryFriend, *p_QueryFriend;

typedef struct _Profile{
	struct _Profile*	next;
	char	deviceid[MAX_DEVICEID_LEN];
	char	devicestatus[MAX_STATUS_LEN];
	char	devicename[MAX_DEVICEID_LEN];
	char	deviceservice[MAX_DEVICEID_LEN];
	char	devicenat[MAX_DEVICEID_LEN];
	char	devicedesc[MAX_DESC_LEN];
} Profile, *pProfile;

typedef struct _ListProfile{
	char 		status[MAX_STATUS_LEN];
	char		time[MAX_TIME_LEN];
	pProfile	pProfileList;
}ListProfile, *pListProfile;

typedef struct _UpdateProfile
{
	char	status[MAX_STATUS_LEN];
	char	deviceticketexpiretime[MAX_TIME_LEN];
	char	time[MAX_TIME_LEN];
}UpdateProfile, *pUpdateProfile;

typedef struct _Logout{
	char	status[MAX_STATUS_LEN];
	char	time[MAX_TIME_LEN];
}Logout, *pLogout;

typedef struct _Getwebpath{
	char	status[MAX_STATUS_LEN];
	char	url[MAX_URL_LEN];
	char	time[MAX_TIME_LEN];
}Getwebpath, *pGetwebpath;

typedef struct _CreatePin{
	char	status[MAX_STATUS_LEN];
	char	pin[MAX_PIN_LEN];
	char	deviceticketexpiretime[MAX_TIME_LEN];
	char	time[MAX_TIME_LEN];	
}CreatePin, *pCreatePin;

typedef struct _QueryPin{
	char	status[MAX_STATUS_LEN];
	char	deviceid[MAX_DEVICEID_LEN];
	char	devicestatus[MAX_STATUS_LEN];
	char	devicename[MAX_ID_LEN];
	char	deviceservice[MAX_ID_LEN];
	char	deviceticketexpiretime[MAX_DEV_TICKET_EXP_LEN];
	char	time[MAX_TIME_LEN];
}QueryPin, *pQueryPin;

typedef struct _UnregisterDevice{
	char	status[MAX_STATUS_LEN];
	char	time[MAX_TIME_LEN];
}UnregisterDevice, *pUnregisterDevice;

typedef struct _Updateiceinfo{
	char	status[MAX_STATUS_LEN];
	char	deviceticketexpiretime[MAX_DEV_TICKET_EXP_LEN];
	char	time[MAX_TIME_LEN];
}Updateiceinfo, *pUpdateiceinfo;

typedef struct _Keepalive{
	char	status[MAX_STATUS_LEN];
	//char	deviceticketexpiretime[MAX_DEV_TICKET_EXP_LEN];
	char	time[MAX_TIME_LEN];
}Keepalive, *pKeepalive;

typedef struct _Push_Msg
{
	char 	status[MAX_STATUS_LEN];
	char	time[MAX_TIME_LEN];
} Push_Msg;

typedef enum _ws_status_code
{
	Success,
	Authentication_Fail,
	Invalid_Service_ID,
	No_Device_Exist,
	No_Right_to_Access,
	No_User_Exist,
	Service_error,
	Invalid_xml_document
}ws_status_code;

int send_loginbyticket_req(
	const char* server,
//	const char* userid, 
	const char* userticket,
//	const char* passwd,
	const char* devicemd5mac,
	const char*	devicename,
	const char*	deviceservice,
	const char* devicetype,
	const char*	permission,
	Login*		pLogin
);

int send_getservicearea_req(
	const char* server, 
	const char* userid, 
	const char* passwd,
	GetServiceArea* pGSA//out put
	);

int send_login_req(
	const char* server,
	const char* userid, 
	const char* passwd,
	const char* devicemd5mac,
	const char*	devicename,
	const char*	deviceservice,
	const char* devicetype,
	const char*	permission,
	Login*		pLogin
	);

int send_query_friend_req(
	const char* server,
	const char* user_ticket,
	QueryFriend*	fd_list
	);

int send_list_profile_req(
	const char* 	server,
	const char*	cusid,
	const char*	userticket,
	const char*	deviceticket,
	const char*	friendid,
	const char*	deviceid,
	pListProfile	p_ListProfile); //out

int send_update_profile_req(
	const char* server,
	const char* cusid,
	const char* deviceid,
	const char* deviceticket,
	const char* devicename,
	const char* deviceservice,
	const char* devicestatus,
	const char* permission,
	const char* devicenat,
	const char* devicedesc,
	pUpdateProfile pup);

int send_logout_req(
	const char* 	server, 
	const char* 	cusid,
	const char* 	deviceid,
	const char* 	deviceticket,
	Logout*		plgo
);

int send_create_pin_req(
	const char* 	server,
	const char*	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	Getwebpath*	pgw
);

int send_query_pin_req(
	const char*	server,
	const char*	cusid,
	const char*	deviceticket,
	const char*	pin,
	QueryPin*	pqp
);

int send_unregister_device_req(
	const char* 	server,
	const char* 	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	UnregisterDevice*	pud
);

int send_updateiceinfo_req(
	const char*	server,
	const char*	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	const char*	iceinfo,
	Updateiceinfo*	pui
);

int send_keepalive_req(
	const char*	server,
	const char*	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	Keepalive*	pka
);

int send_push_msg_req(
	const char *server,
	const char *mac,
	const char *token,
	const char *msg,
	Push_Msg *pPm
);
#endif

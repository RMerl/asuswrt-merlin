#ifndef __WB_H__
#define __WB_H__

#include <stdlib.h>
#define SKIP_PEER_VERIFICATION		// define for the ssl settings
#define SKIP_HOSTNAME_VERFICATION	// define for the ssl settings
#include <ws_api.h>

typedef enum _ws_device_status
{
	Online=2,
	Idle=1,
	Offline=0,
	Unregistered=-1
}ws_device_status;

int ws_getservicearea(char* input, char* webpath);
int ws_login(char* input, char* webpath);
int ws_queryfriend(char* input, char* webpath);
int ws_listprofile(char* input, char* webpath);
int ws_updateprofile(char* input, char* webpath);
int ws_logout(char* input, char* webpath);
int ws_getwebpath(char* input, char* webpath);
int ws_createpin(char* input, char* webpath);
int ws_querypin(char* input, char* webpath);
int ws_unregister(char* input, char* webpath);
int ws_updateiceinfo(char* input, char* webpath);

typedef enum _WS_ID
{
	e_getservicearea,
	e_login,
	e_queryfriend,
	e_listprofile,
	e_updateprofile,
	e_logout,
	e_getwebpath,
	e_createpin,
	e_querypin,
	e_unregister,
	e_updateiceinfo,
	e_keepalive,
	e_pushsendmsg
}WS_ID;

typedef struct _WS_MANAGER{
	WS_ID 	ws_id;
	void*	ws_storage;
	size_t	ws_storage_size;
	char*	response_buff;
	size_t	response_size;
} WS_MANAGER;

// INPUT STRUCT

typedef struct _getservicearea_in{
	char*	userid;
	char*	passwd;
}getservicearea_in;

typedef struct _login_in{
	char*	userid;
	char*	passwd;
	char*	devicemd5mac;
	char*	devicename;
	char*	deviceservice;
	char*	devicetype;
	char*	permission;
}login_in;

typedef struct _queryfriend_in{	
	char*	userticket;	
}queryfriend_in;

typedef struct _listprofile_in{
	char*	cusid;
	char*	userticket;
	char*	deviceticket;
	char*	friendid;
	char*	deviceid;
}listprofile_in;

typedef struct _updateprofile_in{
	char*	cusid;
	char*	deviceid;
	char*	deviceticket;
	char*	devicename;
	char*	deviceservice;
	char*	devicestatus;
	char*	permission;
	char*	devicenat;
	char*	devicedesc;
}updateprofile_in;

typedef struct _logout_in{
	char* 	cusid;
	char*	deviceid;
	char*	deviceticket;	
}logout_in;

typedef struct _getwebpath_in{
	char*	userticket;
	char*	url;
}getwebpath_in;

typedef struct _createpin_in{
	char* 	cusid;
	char*	deviceid;
	char*	deviceticket;
}createpin_in;

typedef struct _querypin_in{
	char* 	cusid;
	char*	deviceticket;
	char*	pin;
}querypin_in;

typedef struct _unregister_in{
	char*	cusid;
	char*	deviceid;
	char*	deviceticket;
}unregister_in;

typedef struct _updateiceinfo_in{
	char*	cusid;
	char*	deviceid;
	char*	deviceticket;
	char*	iceinfo;
}updateiceinfo_in;


/*
int send_getservicearea_req		(const char* input_template, getservicearea_in* pGetSrvArea );
int send_login_req				(const char* input_template, login_in* pLogin );
int send_queryfriend_req		(const char* input_template, queryfriend_in* pQueryFriend );
int send_listprofile_req		(const char* input_template, listprofile_in* pListProfile );
int send_updateprofile_req		(const char* input_template, updateprofile_in* pUpdateProfile );
int send_logout_req				(const char* input_template, logout_in* pLogout );
int send_getwebpath_req			(const char* input_template, getwebpath_in* pGetwebpath );
int send_createpin_req			(const char* input_template, createpin_in* pCreatepin );
int send_querypin_req			(const char* input_template, query_in* pQuerypin );
int send_unregister_req			(const char* input_template, unregister_in* pUnregister );
int send_updateiceinfo_req		(const char* input_template, updateiceinfo_in* pUpdateiceinfo );
#else
int send_http_req				(const char* input_template, void* pData );
*/

#endif

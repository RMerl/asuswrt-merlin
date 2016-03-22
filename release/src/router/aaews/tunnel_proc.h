#ifndef _TUNNEL_PROC_H
#define _TUNNEL_PROC_H
#include <natnl_lib.h>
#include <natnl_event.h>
#include <ws_api.h>
typedef struct  _USER_PORTS{
   char local_data[6];    // the user agent's local data port
   char external_data[6]; // the external port for the incoming packet to local data port
   char local_ctl[6];     // the user agent's local control port
   char external_ctl[6];  // the external control port for the incoming packet to local port
}USER_PORTS; // user selected port pair

typedef enum _UA_ERROR{
	UNKOWN_ERROR = -1000,
	NOT_FOUND_SDK_LIB_ERROR,
	SET_ACCOUNT_ERROR ,
	SET_DEVICE_INFO_ERROR ,
	SET_MEDIA_ERROR ,
	SET_LOG_ERROR,
	SET_UPNP_ERROR,
	SET_ICE_ERROR,
	SET_SIP_ERROR,
	SET_STUN_ERROR,
	SET_TURN_ERROR,
	AAE_LOGIN_ERROR,
	AAE_LOGOUT_ERROR,
	AAE_KA_ERROR,
	AAE_UP_ERROR,
	AAE_LISTPF_ERROR,
	AAE_UNKNOWN_ERROR,
	TUNNEL_INIT_ERROR,
	SEGMENTATION_FAULT,
	SIGABRT_GET,
}UA_ERROR;


typedef struct  _LOG_CFG
{
	int			log_level;
	char* 			log_filename;
	int			log_file_flags;
	int			syslog_facility;
} LOG_CFG;	  

typedef struct _ACCOUNT_CFG
{
	char*	account;
	char*	password;
}ACCOUNT_CFG;

typedef struct _DEVICE_INFO_CFG
{
	char*	device_id;
	char*	device_pwd;
}DEVICE_INFO_CFG;

typedef struct _MEDIA_CFG
{
	int		max_calls;
	int		tnl_timeout_sec;
	int		disable_sdp_compress;
	int		bandwidth_KBs_limit;	
	int		is_server_side_app;  // 0 : the app is NOT server role (android or ios phone etc.), 1 : the app is server role. (default: 0)
}MEDIA_CFG;

typedef struct _UPNP_CFG 
{
		int flag;		      // 0 : don't use upnp, 1 : use upnp with SDK selected port, 2 : use user selected port
		int user_port_count;  // user port count;
		struct {
			char local_data[6];    // the user agent's local data port
			char external_data[6]; // the external port for the incoming packet to local data port
			char local_ctl[6];     // the user agent's local control port
			char external_ctl[6];  // the external control port for the incoming packet to local port
		} user_ports[MAX_USER_PORT_COUNT]; // user selected port pair

}UPNP_CFG;

typedef struct _ICE_CFG
{
	int		use_turn;
	int		use_stun;
	int		force_to_use_ice; 
}ICE_CFG;

typedef struct _SIP_CFG
{
	int			sip_srv_cnt;
	SrvInfo*	sip_srv;
}SIP_CFG;

typedef struct _STUN_CFG
{
	int			stun_srv_cnt;
	SrvInfo*	stun_srv;
}STUN_CFG;

typedef struct _TURN_CFG
{
	int			turn_srv_cnt;
	SrvInfo*	turn_srv;
}TURN_CFG;

typedef int (*SET_ACCOUNT_CFG)(ACCOUNT_CFG* account_cfg);
typedef int (*SET_DEVICE_INFO_CFG)(DEVICE_INFO_CFG* device_info_cfg);
typedef int (*SET_LOG_CFG)(LOG_CFG*	log_cfg);
typedef int (*SET_MEDIA_CFG)(MEDIA_CFG* media_cfg);
typedef int (*SET_UPNP_CFG)(UPNP_CFG* upnp_cfg);
typedef int (*SET_ICE_CFG)(ICE_CFG* ice_cfg);
typedef int (*SET_SIP_CFG)(SIP_CFG* sip_cfg);
typedef int (*SET_STUN_CFG)(STUN_CFG* stun_cfg);
typedef int (*SET_TURN_CFG)(TURN_CFG* turn_cfg);
typedef void (*TNL_CB)(struct natnl_tnl_event* tnl_event); 


#ifdef TNL_CALLBACK_ENABLE
int InitPeer(
		SET_ACCOUNT_CFG		fp_set_account,
		SET_DEVICE_INFO_CFG fp_set_device_info,
		SET_LOG_CFG			fp_set_log,
		SET_MEDIA_CFG		fp_set_media,
		SET_UPNP_CFG		fp_set_upnp,
		SET_ICE_CFG			fp_set_ice,
		SET_SIP_CFG			fp_set_sip,
		SET_STUN_CFG		fp_set_stun,
		SET_TURN_CFG		fp_set_turn,
		TNL_CB				fp_tnl_cb,
		struct natnl_tnl_event* tnl_event,
		int					enable_ws
	  );
#else
int InitPeer(
		SET_ACCOUNT_CFG		fp_set_account,
		SET_DEVICE_INFO_CFG fp_set_device_info,
		SET_LOG_CFG			fp_set_log,
		SET_MEDIA_CFG		fp_set_media,
		SET_UPNP_CFG		fp_set_upnp,
		SET_ICE_CFG			fp_set_ice,
		SET_SIP_CFG			fp_set_sip,
		SET_STUN_CFG		fp_set_stun,
		SET_TURN_CFG		fp_set_turn,
		int					enable_ws
	  );

#endif

int DeletePeer();

#endif

#ifndef __NATNL_LIB_H__
#define __NATNL_LIB_H__

#include <natnl_event.h>


#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_SIP_SERVER_COUNT
#define MAX_SIP_SERVER_COUNT 8
#endif
#ifndef MAX_STUN_SERVER_COUNT
#define MAX_STUN_SERVER_COUNT 8
#endif
#ifndef MAX_TURN_SERVER_COUNT
#define MAX_TURN_SERVER_COUNT 8
#endif
#define MAX_USER_PORT_COUNT 15
#define MAX_TUNNEL_PORT_COUNT 8
#define MAX_TEXT_LEN 64 
#define MAX_PORT_LEN 6 
#define MAX_IP_LEN 64 

#define MAX_CALLS_COUNT 4


/* 
The structure is used to make a instant message port pair between local device and remote device.
When the tunnel is built. The application can connect to 127.0.0.1:lport for 
accessing the service of remote device.
*/
struct natnl_im_port {
	// dest_device_id. The device_id of destination for sending message.
	char dest_device_id[128];  

	// Local device to listen 127.0.0.1:lport. If assign 0, it will be chosen by operating system.
	char lport[MAX_PORT_LEN]; 

	// Remote device to connect to 127.0.0.1:rport.
	char rport[MAX_PORT_LEN]; 

	// The timeout value for instant message.
	int timeout_sec;
};

typedef struct natnl_im_port natnl_im_port;

/*
This structure should be passed in natnl_dll_init API for initialization of SDK.
*/
struct natnl_config {
	// Device id.
	char device_id[128];  

	// The password of device for sip authentication.
	char device_pwd[128]; 

	// The number of sip server.
	int sip_srv_cnt;      

    // The ip or domain name of sip Server.
	char sip_srv[MAX_SIP_SERVER_COUNT][128];    

	// The number of stun server.
	int stun_srv_cnt;

	// The ip or domain name of STUN Server.
	char stun_srv[MAX_STUN_SERVER_COUNT][128];   

	// Reserved for future, discarded.
	int force_to_use_ice; 

	// 0 : don't use turn server. 
	// 1 : UAC and UAS use respective turn server,  
	// 2 : UAC and UAS both use UAC's turn server.
	// This flag also can perform OR bitwise operation with following value.
	// 4 : Use tcp turn.
	int use_turn;         

	// The number of turn server.
	int turn_srv_cnt;     

	// The ip or domain name of TURN Server.
	char turn_srv[MAX_TURN_SERVER_COUNT][128];   
	
	 // Log config.
	struct {
		// It can be assigned value 0~6, 
		// 6=very detailed.
		// 1=error only. 
		// 0=disabled. 
		// Default is 4.
		int log_level;		    

		// Save log to filename with path.
		char log_filename[256]; 

		// Log file flags. 
		// 0x1108: append log to tail of existing log file. 
		// 0x1110: write log to syslog (only available for linux platform).
		unsigned log_file_flags;

		// Only available when 0x1110 is set to log_file_flags. 
		// Same as the facility argument of openlog(). (default: LOG_USER)
		int syslog_facility;

		// The maximum size (KBytes unit) of the log file. Set 0 to disable log rotate mechanism.
		int log_file_size;

		// The number of rotate log file.
		int log_rotate_number;

		// The flag file to enable log dynamically.
		char log_flag_file[256]; 

		// Disable console log or not, 0: enable, 1: disable.
		int disable_console_log;
		                        
	} log_cfg;			 

	// UPnP config.
	struct {
		// 0 : don't use UPnP. 
		// 1 : use UPnP with SDK selected port. 
		// 2 : use user selected port.
		int flag;		      

		// User port count.
		int user_port_count;  
		struct {
			// The user agent's local data port.
			char local_data[6];    

			// The external port for the incoming packet to local data port.
			char external_data[6]; 

			// The user agent's local control port.
			char local_ctl[6];     

			// The external control port for the incoming packet to local port.
			char external_ctl[6];  
		} user_ports[MAX_USER_PORT_COUNT]; // user selected port pair.
	} upnp_cfg; 

	// 0 : don't use stun candidate. 
	// 1 : use stun candidate.
	int use_stun;         

	// Maximum calls to support (default: 4). The maximum value is 15.
	unsigned max_calls;	  

	// Use TLS transport (Discarded).
	int use_tls;		  

	// Verify SIP server's certificate, since v1.1.0.9.
	int verify_server;    

	// Tunnel's timeout value in seconds, since v1.1.0.8. (default and maximum : 180, minimum : 0)
	int tnl_timeout_sec;  

	// Disable sdp compress.
	int disable_sdp_compress; 

	// Bandwidth limit of tunnel in KB/s unit. 
	// 0 : no limit. (default is 0)
	int bandwidth_KBs_limit; 
	                     
	// 0 : the app is NOT server role (android or ios phone etc.), 
	// 1 : the app is server role. (default: 0)
	int is_server_side_app;  
	                         
	// The timeout value of no data through tunnel in seconds, since v1.6.1.1. (default : 0 (no timeout))
	int idle_timeout_sec; 

	// WARNING!!! This option ONLY can be used on client side app. 
	// 0 : full initialization, 
	// 1 : Do faster initialization. 
	//     Discard the sip registration, NAT type detection and UPnP setting in initialization stage.
	int fast_init; 

	// 0 : Don't encrypt data transferred by tunnel, 
	// 1 : Encrypt data transferred by tunnel. (default: 0)
	int enable_secure_data;

	// The certificate file path of server side peer (used by UAS).
	char cert[256];

	// The private key file path of server side peer (used by UAS).
	char cert_pkey[256];

	// The file path of trusted ca certificate list on client side peer (used by UAC).
	char trusted_ca_certs[256];

	// Used by UAC
	// 0 : Don't verify the certificate of server side peer, 
	// 1 : Verify the certificate of server side peer. (default: 0) 
	//     In this case, the certificate of server side or the file path 
	//     of trusted CA certificate list must be assigned to trusted_ca_certs.
	int verify_server_peer;

	// Use SCTP for packet control.
	// 0 : Use UDT for packet control.
	// 1 : Use SCTP for packet control.
	int use_sctp;

	// The number of instant message port.
	int im_port_count;

	// The array of instant message port.
	natnl_im_port im_ports[MAX_USER_PORT_COUNT];
};

//Global declare, nantl_config structure basic config
extern struct natnl_config natnl_config;

/* 
The structure is used to make a tunnel between local device and remote device.
When the tunnel is built. The application can connect to 127.0.0.1:lport for 
accessing the service of remote device.
*/
struct natnl_srv_port {
	// Local device to listen 127.0.0.1:lport. If assign 0, it will be chosen by operating system.
	char lport[MAX_PORT_LEN];

	// Remote device to connect to 127.0.0.1:rport.
	char rport[MAX_PORT_LEN];

	// 0~99, 
	// 0 (Default) : the highest priority.
	// 99 : the lowest priority.
	int qos_priority;

	// 0 (Default) : Enable UDT or SCTP flow control.
	// 1 : Disable UDT or SCTP flow control.
	int disable_flow_control;

	// The speed limit in KBytes/s unit. The valid range is 0~65535.
	// 0 (Default) : unlimited.
	int speed_limit;

	// IP address of Remote device. If not assigned, 127.0.0.1 will be used.
	char rip[MAX_IP_LEN];
};

typedef struct natnl_srv_port natnl_tnl_port;

/*
The structure to be passed in on_natnl_tnl_state callback.
*/
struct natnl_tnl_event {
	// The call handle that is output by natnl_make_call.
	int                  call_id;

	// The callback event.
	natnl_event          event_code;                    
	
	// The string of event, might be empty string by case.
    char                 event_text[MAX_TEXT_LEN]; 
	
	// The callback status code.
	natnl_status_code    status_code;              
	
	// The string of error, might be empty string by case.
	char                 status_text[MAX_TEXT_LEN];

	// The user agent type 
	// 0 is unknown
	// 1 is UAC, 
	// 2 is UAS*/                  
	int                  ua_type;

	// The session id of tunnel.
	char                 session_id[MAX_TEXT_LEN]; 
	
	// The NAT type, since v1.1.0.6
	// 0 is UNKNOWN. 
	// 1 is ERR_UNKNOWN. 
	// 2 is OPEN. 
	// 3 is BLOCKED. 
	// 4 is SYMMETRIC UDP. 
	// 5 is FULL_CONE. 
	// 6 is SYMMETRIC. 
	// 7 is RESTRICTED. 
	// 8 is PORT RESTRICTED.
	int                  nat_type;
	
	// The tunnel type, since v1.1.0.6.
	// 0 is UNKNOWN, 
	// 1 is TCP, 
	// 2 is TURN. 
	// 3 is UDP.
	int                  tnl_type;
	
	// valid if event_code is NATNL_INV_EVENT_CONFIRMED.
	struct {
		struct {
			// The user_id of remote user agent.
			char                 user_id[64];
			
			// The device_id of remote user agent.
			char                 device_id[MAX_TEXT_LEN];
			
			// The version of remote user agent.
			char                 version[MAX_TEXT_LEN];
		} remote_info;

		struct {
			// The version of local user agent.
			char                 version[MAX_TEXT_LEN]; 
		} local_info;
	} para;
	 
	// The local ip of the device. 
	// This is filled by NATNL_TNL_EVENT_NAT_TYPE_DETECTED event.
	char                 local_ip[MAX_TEXT_LEN];

	// The public ip of the device. 
	// This is filled by NATNL_TNL_EVENT_NAT_TYPE_DETECTED event.
	char                 public_ip[MAX_TEXT_LEN];

	// The UPnP port for every call slot. 
	// The number of available upnp_port is equal natnl_config.max_calls setting. 
	// If SDK failed to set UPnP port mapping, the port number will be 0.
	// This is filled by NATNL_TNL_EVENT_NAT_TYPE_DETECTED event.
	int                  upnp_port[MAX_CALLS_COUNT];

	// The mac address of the device. 
	// This is filled by NATNL_TNL_EVENT_NAT_TYPE_DETECTED event.
	char                 mac_address[MAX_TEXT_LEN]; 

	// The instance id of SDK that is output by natnl_lib_init_with_inst_id.
	int                  inst_id;

	// The app_data that came from natnl_lib_init2 or natnl_lib_init_with_inst_id2.
	void *               app_data;

	// If tunnel is TURN and event code is NATNL_TNL_EVENT_MAKECALL_OK, 
	// this field will contain the ip:port of client side that is responsible 
	// for communication with TURN server.
	char                 turn_mapped_address[MAX_TEXT_LEN]; 
};

/**
* (since 1.8.0.0)
 * This enumeration describes tunnel state and 
 * it is used on nantl_tnl_info structure.
 */
typedef enum natnl_tnl_state {
	// Unknown state.
	TNL_STATE_UNKNOWN = 0,   

	// Tunnel is working normally.
	TNL_STATE_INACTIVE = 1,  

	// Tunnel is down.
	TNL_STATE_ACTIVE = 2,    
} natnl_tnl_state;

/**
 * (since 1.8.0.0)
 * The structure to be used in natnl_read_tnl_info API.
 */
struct natnl_tnl_info {
	// The instance id of SDK that is output by natnl_lib_init_with_inst_id.
	int                  inst_id;

	//  The call handle that is output by natnl_make_call.
	int                  call_id;

	// The natnl_event enumeration.
	natnl_tnl_state      state;    

	// The natnl_status_code enumeration that is defined in natnl_event.h.
	natnl_status_code    status_code;  

	// The user agent type. 
	// 0 is unknown.
	// 1 is UAC. 
	// 2 is UAS.                  
	int                  ua_type;

	// The session id of tunnel.
	char                 session_id[MAX_TEXT_LEN]; 

	// The tunnel type, since v1.1.0.6.
	// 0 is UNKNOWN, 
	// 1 is TCP. 
	// 2 is TURN. 
	// 3 is UDP.
	int                  tnl_type;

	struct {
		struct {
			// The device_id of remote user agent.
			char                 device_id[MAX_TEXT_LEN];

			// The version of remote user agent.
			char                 version[MAX_TEXT_LEN];
		} remote_info;

		struct {
			// The device_id of local user agent.
			char                 device_id[MAX_TEXT_LEN];

			// The version of local user agent.
			char                 version[MAX_TEXT_LEN]; 
		} local_info;
	} para;

	// The app_data that came from natnl_lib_init2 or natnl_lib_init_with_inst_id2.
	void				*app_data;

	// If tunnel is TURN and event code is NATNL_TNL_EVENT_MAKECALL_OK, 
	// this field will contain the ip:port of client side that is responsible 
	// for communication with TURN server.
	char                 turn_mapped_address[MAX_TEXT_LEN]; 

	// The number of current used tunnel port pair. The maximum value is defined in MAX_TUNNEL_PORT_COUNT.
	int                  tnl_port_cnt;

	// The array of current used tunnel port pair. The maximum size of this array is defined in MAX_TUNNEL_PORT_COUNT. 
	natnl_tnl_port tnl_ports[MAX_TUNNEL_PORT_COUNT];
};

/*
The structure defines the callback function of asusnatnl library.
*/
struct natnl_callback {
	/* The callback function will be called 
	when the tunnel state changed or error occur. */
    void (*on_natnl_tnl_event)(struct natnl_tnl_event *tnl_event);
};

/*
The structure is used in natnl_read_tnl_transfer_speed and 
natnl_tnl_transfer_speed_with_instant_id APIs.
*/
struct natnl_tnl_transfer_speed {
	int rx_speed;   // the download speed.
	int tx_speed;   // the upload speed.
};


//Global declare, natnl_callback structure.
extern struct natnl_callback natnl_callback;


#if defined(NATNL_LIB) || defined(__APPLE__)
    
#ifdef WIN32

	#define WINAPI __stdcall
	#ifdef DLL_EXPORTS
		#define NATNL_LIB_API __declspec(dllexport)
	#else
		#define NATNL_LIB_API __declspec(dllimport)
	#endif
#else
// linux define 
#define WINAPI
#define NATNL_LIB_API
#endif

/**
 * Set the maximum number of instances.
 * @param [in]  max_instance. The maximum number of instances.
 * @return 0 : For success.
 *        -1 : Less than the minimum number of instances.
 *        -2 : Exceed the maximum number of instances.
 *  non-zero : For error code.
 */
NATNL_LIB_API int WINAPI natnl_set_max_instances(int max_instance);

/**
 * Initialize SDK
 * @param [in]  cfg. The natnl_config structure for basic setting.
 * @return 0 : For success. 
 *  60000004 : The instance of SDK was already initialized.
 *  60000008 : There are too many instances.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_init(struct natnl_config *cfg);

/**^M
 * Initialize SDK^M
 * @param [in]  cfg. The natnl_config structure for basic setting.
 * @param [in]  app_data. The app data for instance identification. 
 * @return 0 : For success. 
 *  60000004 : The instance of SDK was already initialized.
 *  60000008 : There are too many instances.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_init2(struct natnl_config *cfg, void *app_data);

/**^M
 * Initialize SDK^M
 * @param [in]  cfg. The natnl_config structure for basic setting.
 * @param [in]  app_data. The app data for instance identification. 
 * @param [in]  natnl_cb. The callback structure for event notification to application.
 * @return 0 : For success. 
 *  60000004 : The instance of SDK was already initialized.
 *  60000008 : There are too many instances.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_init3(struct natnl_config *cfg, struct natnl_callback *natnl_cb, void *app_data);

/**
 * Initialize SDK
 * @param [in]  cfg.     The natnl_config structure for basic setting.
 * @param [out] inst_id. A int pointer for instance id output. 
 * @return 0 : For success. 
 *  60000004 : SDK was initialized.
 *  60000008 : There are too many instances.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_init_with_inst_id(struct natnl_config *cfg, int *inst_id);

/**

 * Initialize SDK
 * @param [in]  cfg.     The natnl_config structure for basic setting.
 * @param [out] inst_id. A int pointer for instance id output. 
 * @param [in]  app_data. The app data for instance identification. 
 * @return 0 : For success. 
 *  60000004 : SDK was initialized.
 *  60000008 : There are too many instances.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_init_with_inst_id2(struct natnl_config *cfg, int *inst_id, void *app_data);

/**
 * Initialize SDK
 * @param [in]  cfg.     The natnl_config structure for basic setting.
 * @param [out] inst_id. A int pointer for instance id output. 
 * @param [in]  app_data. The app data for instance identification. 
 * @param [in]  natnl_cb. The callback structure for event notification to application.
 * @return 0 : For success. 
 *  60000004 : SDK was initialized.
 *  60000008 : There are too many instances.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_init_with_inst_id3(struct natnl_config *cfg, int *inst_id, struct natnl_callback *natnl_cb, void *app_data);

NATNL_LIB_API int WINAPI natnl_pool_dump(int detail);

NATNL_LIB_API int WINAPI natnl_pool_dump_with_inst_id(int detail, int inst_id);

NATNL_LIB_API void WINAPI natnl_dump_version(int argc, char *argv[]);

NATNL_LIB_API void WINAPI natnl_dump_version_with_inst_id(int argc, char *argv[], int inst_id);

/**
 * De-initialize one instance of SDK
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : SDK isn't initialized.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_deinit(void);

/**
 * De-initialize one instance of SDK
 * @param [in] inst_id.  The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                         Default is 1.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : The instance of SDK isn't initialized.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_deinit_with_inst_id(int inst_id);

/**
 * De-initialize all instances of SDK
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : SDK isn't initialized.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_lib_deinit_all(void);

/**
 * Build the tunnel with remote device.
 * @param [in]  device_id.           The device_id of remote device.
 * @param [in]  tnl_port_cnt.        The number of tunnel port pair (lport,rport,qos_priority). The maximum value is defined in MAX_TUNNEL_PORT_COUNT.
 * @param [in]  tnl_ports.           The array of tunnel port pair.
 * @param [in]  user_id (Discarded). The user_id is sent to UAS for identity authentication.
 * @param [in]  timeout_sec.         The timeout value in seconds. If call isn't made in timeout_sec, 
                                         SDK will notify application NATNL_TNL_EVENT_MAKECALL_FAILED event with 
							             NATNL_SC_MAKE_CALL_TIMEOUT status code. 
							         If this value is 0, it represents that wait forever.
 * @param [in]  use_sctp.            Indicate to use UDT or SCTP as flow control. 0 : use UDT, 1 : use SCTP.
 * @param [out]	tnl_info.		     The structure to be used in natnl_read_tnl_info API.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_make_call(char *device_id, int tnl_port_cnt, 
                                natnl_tnl_port tnl_ports[], char *user_id,
								int timeout_sec, int use_sctp, struct natnl_tnl_info *tnl_info);

/**
 * Build the tunnel with remote device.
 * @param [in]  device_id.           The device_id of remote device.
 * @param [in]  tnl_port_cnt.        The number of tunnel port pair (lport,rport). The maximum value is defined in MAX_TUNNEL_PORT_COUNT.
 * @param [in]  tnl_ports.           The array of tunnel port pair.
 * @param [in]  user_id (Discarded). The user_id is sent to UAS for identity authentication.
 * @param [in]  timeout_sec.         The timeout value in seconds. If call isn't made in timeout_sec, 
                                         SDK will notify application NATNL_TNL_EVENT_MAKECALL_FAILED event with 
                                         NATNL_SC_MAKE_CALL_TIMEOUT status code. 
                                     If this value is 0, it represents that wait forever.
 * @param [in]  use_sctp.            Indicate to use UDT or SCTP as flow control. 0 : use UDT, 1 : use SCTP.
 * @param [in]  inst_id.             The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                                     Default is 1.
 * @param [out]	tnl_info.		 The structure to be used in natnl_read_tnl_info API.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_make_call_with_inst_id(char *device_id, int tnl_port_cnt, 
                                natnl_tnl_port tnl_ports[], char *user_id,
								int timeout_sec, int use_sctp, int inst_id, struct natnl_tnl_info *tnl_info);

/**
 * Build the tunnel with remote device.
 * @param [in]  device_id.         The device_id of remote device.
 * @param [in]  tnl_port_cnt.      The number of tunnel port pair (lport,rport). The maximum value is defined in MAX_TUNNEL_PORT_COUNT.
 * @param [in]  tnl_ports.         The array of tunnel port pair.
 * @param [in]  user_id.           The user_id is sent to UAS for identity authentication.
 * @param [in]  timeout_sec.       The timeout value in seconds. If call isn't made in timeout_sec, 
                                       SDK will notify application NATNL_TNL_EVENT_MAKECALL_FAILED event with 
							           NATNL_SC_MAKE_CALL_TIMEOUT status code. 
							       If this value is 0, it represents that wait forever.
 * @param [in]  use_sctp.          Indicate to use UDT or SCTP as flow control. 0 : use UDT, 1 : use SCTP.
 * @param [in]  inst_id.           The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                                   Default is 1.
 * @param [in]  caller_device_pwd. The caller device password, this value must be set when use fase_init mode.
 * @param [out]	tnl_info.		 The structure to be used in natnl_read_tnl_info API.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_make_call_with_inst_id2(char *device_id, int tnl_port_cnt, 
                                natnl_tnl_port tnl_ports[], char *user_id,
								int timeout_sec, int use_sctp, int inst_id, 
								char *caller_device_pwd, struct natnl_tnl_info *tnl_info);

/**
 * Terminate the tunnel with remote device.
 * @param [in]  call_id.  The call handle that is output by natnl_make_call.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_hangup_call(int call_id);

/**
 * Terminate the tunnel with remote device.
 * @param [in]  call_id. The call handle that is output by natnl_make_call.
 * @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                         Default is 1.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_hangup_call_with_inst_id(int call_id, int inst_id);

/**
* Register device to SIP server, this is for future use.
* @return 0 : For success. 
*     70004 : Invalid argument.
*     70011 : Busy, another registration is under progress.
*  60000005 : SDK isn't initialized, please initialize it first.
*  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_reg_device(void);

/**
 * Register device to SIP server, this is for future use.
 * @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                         Default is 1.
 * @return 0 : For success, 
 *     70004 : Invalid argument.
 *     70011 : busy, another registration is under progress.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_reg_device_with_inst_id(int inst_id);

/**
 * Un-Register device from SIP server, this is for future use.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *     70011 : Busy, another registration is under progress.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_unreg_device(void);

/**
 * Un-Register device from SIP server, this is for future use.
 * @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                         Default is 1.
 * @return 0 : For success. 
 *     70004 : Invalid argument.
 *     70011 : Busy, another registration is under progress.
 *  60000005 : SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_unreg_device_with_inst_id(int inst_id);

/**
 * Update config dynamically, this is for future use.
 * @param [in] cfg. The natnl_config structure for updating setting.
                    Currently it only supports to reconfigure log_cfg 
					(e.g. natnl_config.log_cfg.log_level etc) dynamically.
 * @return 0 : For success.
 *  non-zero : For error code.
 */
NATNL_LIB_API int WINAPI natnl_update_config(struct natnl_config *cfg);

/**
 * Update config dynamically, this is for future use.
 * @param [in] cfg. The natnl_config structure for updating setting.
 * @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                         Default is 1.
 * @return 0 : For success.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_update_config_with_inst_id(struct natnl_config *cfg, int inst_id);

/**
 * For internal test use.
 * @param [in]  call_id. The call handle that is output by natnl_make_call.
 * @return 0 : For success.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_call_reinvite(int call_id);

/**
 * For internal test use.
 * @param [in]  call_id. The call handle that is output by natnl_make_call.
 * @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                         Default is 1.
 * @return 0 : For success. 
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_call_reinvite_with_inst_id(int call_id, int inst_id);

/**
 * Adjust tunnel port.
 * @param [in]  call_id.        The call handle that is output by natnl_make_call. 
                                If there is no tunnel please give -1.
 * @param [in]  action.         The adjustment type. 1 : add tunnel port, 2 : remove tunnel port.
 * @param [in]  tnl_port_cnt.   The number of tunnel port pair (lport,rport).
 * @param [in]  tnl_ports.      The array of tunnel port pair.
 * @return 0 : for success. 
 *        -1 : exceed number limit of tunnel port if action is 1.
 *        -2 : unknown action.
 *  60000005 : the instance of SDK isn't initialized, please initialize it first.
 *  non-zero : for another error.
 */
NATNL_LIB_API int WINAPI natnl_tunnel_port(int call_id, 
						int action, 
						int tnl_port_cnt, 
						natnl_tnl_port tnl_ports[]);

/**
 * Adjust tunnel port.
 * @param [in]  call_id.        The call handle that is output by natnl_make_call. 
                                If there is no tunnel please give -1.
 * @param [in]  action.         The adjustment type. 1 : add tunnel port, 2 : remove tunnel port.
 * @param [in]  tnl_port_cnt.   The number of tunnel port pair (lport,rport).
 * @param [in]  tnl_ports.      The array of tunnel port pair.
 * @param [in]  inst_id.        The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                                Default is 1.
 * @return 0 : For success. 
 *        -1 : Exceed number limit of tunnel port if action is 1.
 *        -2 : Unknown action.
 *  60000005 : The instance of SDK isn't initialized, please initialize it first.
 *  non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_tunnel_port_with_inst_id(int call_id, 
						int action, int tnl_port_cnt, 
						natnl_tnl_port tnl_ports[],
						int inst_id);

/**
* Adjust im lport/rport pair.
* @param [in]  action.         The adjustment type. 1 : add im port, 2 : remove im port.
* @param [in]  im_port_cnt.    The number of im port pair (lport,rport).
* @param [in]  im_ports.       The array of im port pair.
 * @return 0 : for success. 
 *        -1 : exceed number limit of tunnel port if action is 1.
 *        -2 : unknown action.
 *  60000005 : the instance of SDK isn't initialized, please initialize it first.
 *  non-zero : for another error.
 */
NATNL_LIB_API int WINAPI natnl_instant_msg_port(int action, 
						int im_port_cnt, 
						natnl_im_port im_ports[]);

/**
 * Adjust im lport/rport pair.
 * @param [in]  action.         The adjustment type. 1 : add im port, 2 : remove im port.
 * @param [in]  im_port_cnt.    The number of im port pair (lport,rport).
 * @param [in]  im_ports.       The array of im port pair.
 * @param [in]  inst_id.        The instance id of SDK that is output by natnl_lib_init_with_inst_id. Default is 1.
 * @return 0 : for success. 
 *        -1 : exceed number limit of tunnel port if action is 1.
 *        -2 : unknown action.
 *  60000005 : the instance of SDK isn't initialized, please initialize it first.
 *  non-zero : for another error.
 */
NATNL_LIB_API int WINAPI natnl_instant_msg_port_with_inst_id(int action, 
						int im_port_cnt, 
						natnl_im_port im_ports[],
						int inst_id);

/**
 * Send instant message to remote device_id.
 * @param [in]  dest_device_id. The device_id of destination for sending message.
 * @param [in]  msg_len.        The length of message content.
 * @param [in]  msg_content.    The message content.
 * @param [in]  rport.          The message receiving port of remote ua.
 * @param [in, out]  resp_len.  The length of response message.
 * @param [out]  resp_msg.      The buffer response message.
 * @return   0 : For success, the resp_len will carry the length 
 *                   of resp_msg and the resp_msg will carry response message, 
 *       70019(PJ_ETOOSMALL) : 
 *               The resp_msg is too small to carry response message and 
 *                   the resp_len will carry the length that resp_msg needed,
 *    60000005 : The instance of SDK isn't initialized, please initialize it first.
 *    60000014 : The instant message is too long. The maximum length is 1024 bytes.
 *    non-zero : For another error.
 */
NATNL_LIB_API int WINAPI natnl_send_instant_msg(
							char *dest_device_id,
							int msg_len,
							char *msg_content,
							int rport,
							int *resp_len,
							char *resp_msg);

/**
 * Send instant message to remote device_id.
 * @param [in]  dest_device_id. The device_id of destination for sending message.
 * @param [in]  msg_content.    The message content.
 * @param [in]  inst_id.        The instance id of SDK, default is 1.
 * @param [in]  rport.          The message receiving port of remote ua.
 * @param [in, out]  resp_len.  The length of response message .
 * @param [out]  resp_msg.      The buffer of response message.
 * @param [in]  inst_id.        The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                                Default is 1.
 * @return   0 : For success, the resp_len will carry the length 
 *               of resp_msg and the resp_msg will carry response message, 
 *       70019(PJ_ETOOSMALL) : 
 *               The resp_msg is too small to carry response message and 
 *               the resp_len will carry the length that resp_msg needed,
 *    60000005 : The instance of SDK isn't initialized, please initialize it first.
 *    non-zero : For another error. 
 */
NATNL_LIB_API int WINAPI natnl_send_instant_msg_with_inst_id(
						char *dest_device_id,
						int msg_len,
						char *msg_content,
						int rport,
						int *resp_len,
						char *resp_msg,
						int inst_id);

/**
 * Read the tunnel status. The function will block until tunnel is 
 * closed, broken or idle a while(idle_timeout_sec is set).
 *
 * @param [in]  call_id. The call handle that is output by natnl_make_call.
 * @return   0 : For success. 
 *         200 : Local or remote ua hangup the tunnel.
 *       70004 : The parameters are invalid.
 *    60000005 : The instance of SDK isn't initialized, please initialize it first.
 *    60000009 : Tunnel timeout and call was hung by SDK.
 *    60000010 : Idle timeout but tunnel is still active. 
                 APP may do something (eg. Hangup the tunnel).
 *    non-zero : For another error. 
 */
NATNL_LIB_API int WINAPI natnl_read_tnl_status(
						int call_id);

/**
* Read the tunnel status. The function will block until tunnel is 
* closed, broken or idle a while(idle_timeout_sec is set).
*
* @param [in]  call_id. The call handle that is output by natnl_make_call.
* @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                        Default is 1.
* @return   0 : For success. 
*         200 : local or remote ua hangup the tunnel.
*       70004 : Invalid argument.
*    60000005 : The instance of SDK isn't initialized, please initialize it first.
*    60000009 : Tunnel timeout and call was hung by local.
*    60000010 : Idle timeout and tunnel is still active.
*    non-zero : For another error. 
 */
NATNL_LIB_API int WINAPI natnl_read_tnl_status_with_inst_id(
						int call_id,
						int inst_id);

/**
* Read tunnel information.
* @param [in]  call_id.    The call handle that is output by natnl_make_call.
* @param [out]  tnl_info.  A natnl_tnl_info structure. When the API return with success, 
                             the tunnel info will be put into this parameter.
 * @return   0 : For success. 
 *       70004 : Invalid argument.
 *    60000005 : SDK isn't initialized, please initialize it first.
 *    non-zero : For another error. 
 */
NATNL_LIB_API int WINAPI natnl_read_tnl_info(
						int call_id,
						struct natnl_tnl_info *tnl_info);
/**
 * Read tunnel information with instance id.
 * @param [in]  call_id.  The call handle that is output by natnl_make_call.
 * @param [in]  tnl_info. A natnl_tnl_info structure. When the API return with success, 
                            the tunnel info will be  put into this parameter.
 * @param [in]  inst_id.  The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                          Default is 1.
 * @return   0 : For success, 
 *       70004 : Invalid argument.
 *    60000005 : SDK isn't initialized, please initialize it first.
 *    non-zero : For another error. 
 */
NATNL_LIB_API int WINAPI natnl_read_tnl_info_with_inst_id(
						int call_id,
						struct natnl_tnl_info *tnl_info,
						int inst_id);

/**
* Read current tunnel transfer speed in Bytes/s unit. 
*
* @param [in]  call_id. The call handle that is output by natnl_make_call.
* @param [in]  transfer_speed. The structure of natnl_tnl_transfer_speed.  
*                         This parameter will carry the rx/tx speed when the function returned.
* @return   0 : For success, 
*      70004 : Invalid argument.
*   60000005 : SDK isn't initialized, please initialize it first.
 */
NATNL_LIB_API int WINAPI natnl_read_tnl_transfer_speed(
						int call_id,
						struct natnl_tnl_transfer_speed *transfer_speed);

/**
* Read current tunnel transfer speed in Bytes/s unit with instance id. 
*
* @param [in]  call_id. The call handle that is output by natnl_make_call.
* @param [in]  transfer_speed. The structure of natnl_tnl_transfer_speed.  
*                         This parameter will carry the rx/tx speed when the function returned.
* @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
                        Default is 1.
* @return   0 : For success, 
 *      70004 : Invalid argument.
 *   60000005 : SDK isn't initialized, please initialize it first.
 */
NATNL_LIB_API int WINAPI natnl_read_tnl_transfer_speed_with_inst_id(
						int call_id,
						struct natnl_tnl_transfer_speed *transfer_speed,
						int inst_id);

/**
* Set speed limit in Bytes/s unit. 
*
* @param [in]  call_id. The call handle that is output by natnl_make_call.
* @param [in]  limit_speed. The speed limit of natnl_tnl_transfer_speed structure in Bytes/s unit. {0,0} represents no limit.
* @return   0 : For success, 
*      70004 : Invalid argument.
*   60000005 : SDK isn't initialized, please initialize it first.
 */
NATNL_LIB_API int WINAPI natnl_set_tnl_transfer_speed_limit (
						int call_id,
						struct natnl_tnl_transfer_speed limit_speed);

/**
* Set speed limit in Bytes/s unit. 
*
* @param [in]  call_id. The call handle that is output by natnl_make_call.
* @param [in]  limit_speed. The speed limit of natnl_tnl_transfer_speed structure in Bytes/s unit. {0,0} represents no limit.
* @param [in]  inst_id. The instance id of SDK that is output by natnl_lib_init_with_inst_id. 
* @return   0 : For success, 
*      70004 : Invalid argument.
*   60000005 : SDK isn't initialized, please initialize it first.
 */
NATNL_LIB_API int WINAPI natnl_set_tnl_transfer_speed_limit_with_inst_id (
						int call_id,
						struct natnl_tnl_transfer_speed limit_speed,
						int inst_id);

/**
 * !!! NOTICE !!! Please don't call this in callback function directly or deadlock will occur.
 * Detect NAT type
 * @param [in]  stun_srv. The stun server which is used to detect NAT type.
 * @return   NAT type
			 0 is UNKNOWN, 
			 1 is ERR_ UNKNOWN, 
			 2 is OPEN, 
			 3 is BLOCKED, 
			 4 is SYMMETRIC UDP, 
			 5 is FULL_CONE, 
			 6 is SYMMETRIC, 
			 7 is RESTRICTED, 
			 8 is PORT RESTRICTED
 *			-1 Error
 */
NATNL_LIB_API int WINAPI natnl_detect_nat_type(
	char *stun_srv);

#if 0 // unpublished
/**
 * Resolve indicated address's mac address by ARP.
 *
 * @param [in] targe_addr.          The address will be resolved in string format.
 * @param [out] target_mac.         The resolved mac address string buffer.
 * @param [in, out] target_mac_len. The length of resolved mac address string buffer.
 * @param [in]timeout.	            The timeout value. We ignore this value on windows platform.
 * @return     0 : for success, 
 *         70004 (PJ_EINVAL) : Invalid argument.
 *         70009 (PJ_ETIMEDOUT) : Timed out to resolve mac.
 *         70019 (PJ_ETOOSMALL) : The target_mac_len is too small. It must be equal or greater than 17.
 *      non-zero : for another error. 
 */
NATNL_LIB_API int WINAPI natnl_resolve_mac_by_arp(char *targe_address, 
												  char *target_mac, 
												  int *target_mac_len, 
												  int timeout);
#endif

/**
 * Get version of SDK
 * @return   the version string.
 */
NATNL_LIB_API char * WINAPI natnl_lib_version(void);
#endif


#ifdef __cplusplus
}
#endif


#endif	/* __NATNL_LIB_H__ */

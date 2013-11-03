//#include <wb.h>					// include the header of web service 
#include <ws_api.h>					// include the header of web service 
//#include <ws_api.h>	<--- use for release lib
//#include <wb_util.h>
#include <openssl/md5.h>
#include <ssl_api.h>
#include <log.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#if NVRAM
#ifdef __cplusplus  
extern "C" {  
#endif
#include <bcmnvram.h>
#ifdef __cplusplus  
}
#endif
#include <shared.h>
#endif

//#define ASUS_VIP_ID 	"jackie_yu@asus.com.tw"
// ID PWD will get from NVRAM
#define ASUS_VIP_ID 	"fineart.douglas@gmail.com"
#define ASUS_VIP_PWD	"11111111"
#define APP_DBG 1

// debug NATNL
#define INT_NATNL 1 
#define USE_WEB_API 1

#if INT_NATNL
#include <dlfcn.h>
#include <natnl_lib.h>
#include <natapi.h>
#define GetCurrTID() ((unsigned int)pthread_self())

enum CALL_STATE
{
	Logined,
	Initializing,
	Deinitializing,
	Ready,
	MakingCall,
	OnCall,
	EndingCall,
	Unknown
	
};

enum AAE_STATE
{
	StartService,
	EndService
};

/*
 * Callback function for call state changed
 * @call_state The natnl_call_state structure
 */
int current_callid;
int event_code=0;
char* event_text=NULL;
void on_natnl_tnl_event(struct natnl_tnl_event *tnl_event) {
	int		status =-1;
	char *tnl_type;
#ifdef WIN32
	HANDLE        hThread;
	int iThreadId2, iThreadId3;
#endif
	event_code = tnl_event->event_code;
	event_text = tnl_event->event_text;
	if (tnl_event->event_code == NATNL_INV_EVENT_NULL){
		Cdbg(APP_DBG,"[main.c] event null. ThreadId=[%08x], status=%d\n", GetCurrTID(), tnl_event->status_code);
	}else if (tnl_event->event_code == NATNL_TNL_EVENT_START)
		current_callid = tnl_event->call_id;

	if (tnl_event->tnl_type == 0)
		tnl_type = "UNKNOWN";
	else if (tnl_event->tnl_type == 1)
		tnl_type = "TCP";
	else if (tnl_event->tnl_type == 2)
		tnl_type = "TURN";
	else if (tnl_event->tnl_type == 3)
		tnl_type = "UDP";
	printf("!!!!!!!!!!!!![main.c] on_natnl_tnl_event call_id=%d, "
		"event_code=%d, event_text=%s, status_code=%d, status_text=%s, "
		"ua_type=%d, nat_type=%d, tnl_type=%s\n", 
		tnl_event->call_id,
		tnl_event->event_code,
		tnl_event->event_text,
		tnl_event->status_code,
		tnl_event->status_text,
		tnl_event->ua_type,
		tnl_event->nat_type,
		tnl_type);

	if (tnl_event->event_code == NATNL_TNL_EVENT_INIT_OK) {
		// init done
		Cdbg(APP_DBG,"[main.c] natnl_lib_init ok. ThreadId=[%08X]\n", GetCurrTID());
		 
	} else if (tnl_event->event_code == NATNL_TNL_EVENT_INIT_FAILED) {
		// init failed 
		Cdbg(APP_DBG,"[main.c] natnl_lib_init failed. ThreadId=[%08X], status=%d\n", GetCurrTID(), tnl_event->status_code);
//		status = lib_unload();
	} else if (tnl_event->event_code == NATNL_TNL_EVENT_MAKECALL_OK) {
		printf("********* TUNNEL START *********\n");
		printf("tunnel type : %s\n", tnl_type);
		printf("********* TUNNEL START *********\n");
		printf("[main.c] natnl_make_call ok. ThreadId=[%08X]\n", GetCurrTID());
	} else if (tnl_event->event_code == NATNL_TNL_EVENT_MAKECALL_FAILED) {
		printf("[main.c] natnl_make_call make call failed. ThreadId=[%08X], status=%d\n", GetCurrTID(), tnl_event->status_code);
	} else if (tnl_event->event_code == NATNL_TNL_EVENT_HANGUP_OK) {
		printf("[main.c] natnl_hangup_call ok. ThreadId=[%08X]\n", GetCurrTID());

//		hangup_ok = 1;
	} else if (tnl_event->event_code == NATNL_TNL_EVENT_HANGUP_FAILED) {
		printf("[main.c] natnl_hangup_call failed. ThreadId=[%08X], status=%d\n", GetCurrTID(), tnl_event->status_code);
	} else if (tnl_event->event_code == NATNL_TNL_EVENT_DEINIT_OK) {
		printf("[main.c] natnl_lib_deinit ok. ThreadId=[%08X]\n", GetCurrTID());
	} else if (tnl_event->event_code == NATNL_TNL_EVENT_DEINIT_FAILED) {
		printf("[main.c] natnl_lib_deinit failed. ThreadId=[%08X], status=%d\n", GetCurrTID(), tnl_event->status_code);
	}  else if (tnl_event->event_code == NATNL_TNL_EVENT_KA_TIMEOUT) {
		printf("[main.c] natnl tunnel timeout. ThreadId=[%08X], status=%d\n", GetCurrTID(), tnl_event->status_code);
		//hangup_ok = 0;
		//hThread = CreateThread(NULL, 0, hang_up,
		//	current_callid, 0, &iThreadId2);
		//while(!hangup_ok) {
		//	printf("\n");
		//	Sleep(1);
		//}
		//hThread = CreateThread(NULL, 0, natnl_deinit,
		//	0, 0, &iThreadId3);
	} 
}

//	AAE_STATE as = getfromNVRAM;
	AAE_STATE as = EndService; // neeed to improve
CALL_STATE st = Unknown;
NAT_INIT		nat_init;
NAT_DEINIT		nat_deinit;
NAT_MAKECALL	nat_makecall;
NAT_HANG_UP		nat_hangup;
NAT_POOL_DUMP	nat_dump;

struct natnl_config gNatnl_cfg;
struct natnl_srv_port natnl_srv_ports[MAX_SRV_PORT_COUNT];
int natnl_srv_port_count; 
struct natnl_callback gNatCallback;

typedef struct  _USER_PORTS{
   char local_data[6];    // the user agent's local data port
   char external_data[6]; // the external port for the incoming packet to local data port
   char local_ctl[6];     // the user agent's local control port
   char external_ctl[6];  // the external control port for the incoming packet to local port
}USER_PORTS; // user selected port pair


int nat_setcfg(
		char*		device_id,
		char*		device_pwd,
//		int			sip_cnt,
		SrvInfo*	sip_srvs,
//		char*		sip_srv,
//		int			stun_cnt,
		SrvInfo*	stun_srvs,
//		char*		stun_srv,
		int			use_turn,
//		int			turn_cnt,
		SrvInfo*	turn_srvs,
//		char*		turn_srv,
		//log_cfg*	pLogCfg,
		int			log_level,
		char*		log_filename,
		unsigned	log_file_flags,
		//upnp_cfg*	pUpnp_cfg,
		int			flag,
		int			user_port_count,
		USER_PORTS*	user_ports,
		int			use_stun,
		unsigned	max_calls,
		int			use_tls,
		int			verify_server,
		int			tnl_timeout_sec,
		struct natnl_srv_port*	pNatSrvPort,
		struct natnl_srv_port*	srv_port_setting,	
		int*					pNatSrvPortCnt,
		int						natnl_srv_port_count,
		struct natnl_config*	pNatCfg
	  )
{
	int i =0;
	Cdbg(APP_DBG, "...............1");	
	strcpy(pNatCfg->device_id	, device_id);
	strcpy(pNatCfg->device_pwd,device_pwd);
/*	
	strcpy(pNatCfg->sip_srv	, sip_srv);
	strcpy(pNatCfg->stun_srv	, stun_srv);
	strcpy(pNatCfg->turn_srv	, turn_srv);
	*/
	//copy sip server, assign cnt
	i =0; int sip_cnt=0;
	for(SrvInfo* p=sip_srvs;p;p=p->next){
		strcpy(pNatCfg->sip_srv[sip_cnt],sip_srvs->srv_ip);
		sip_cnt++; 
	} pNatCfg->sip_srv_cnt = sip_cnt;
	//copy stun server, assign cnt
	i =0; int stun_cnt=0;
	for(SrvInfo* p=stun_srvs;p;p=p->next){
		strcpy(pNatCfg->stun_srv[stun_cnt],stun_srvs->srv_ip);
		stun_cnt++;
	} pNatCfg->stun_srv_cnt = stun_cnt;
	//copy turn server, assign cnt	
	i =0; int turn_cnt=0;
	for(SrvInfo* p=turn_srvs;p;p=p->next){
		strcpy(pNatCfg->turn_srv[turn_cnt],turn_srvs->srv_ip);
		turn_cnt++;
	} pNatCfg->turn_srv_cnt = turn_cnt;

	pNatCfg->use_turn	= use_turn;
	pNatCfg->log_cfg.log_level = log_level;
	strcpy(pNatCfg->log_cfg.log_filename, log_filename);
	pNatCfg->log_cfg.log_file_flags = log_file_flags;
//	memcpy(pNatCfg->upnp_cfg, pUpnp_cfg, sizeof(pNatCfg.upnp_cfg));
	pNatCfg->upnp_cfg.flag = flag;
	pNatCfg->upnp_cfg.user_port_count = user_port_count;
	Cdbg(APP_DBG, "...............2");	
	for(i = 0; i<pNatCfg->upnp_cfg.user_port_count;i++){
	Cdbg(APP_DBG, "...............2-%d",i);	
		strcpy(pNatCfg->upnp_cfg.user_ports[i].local_data	,user_ports[i].local_data);
		strcpy(pNatCfg->upnp_cfg.user_ports[i].external_data	,user_ports[i].external_data);
		strcpy(pNatCfg->upnp_cfg.user_ports[i].local_ctl		,user_ports[i].local_ctl);
		strcpy(pNatCfg->upnp_cfg.user_ports[i].external_ctl	,user_ports[i].external_ctl);
	}
	Cdbg(APP_DBG, "...............3-0");	

	pNatCfg->use_stun		= use_stun;
	pNatCfg->max_calls		= max_calls;
	pNatCfg->use_tls			= use_tls;
	pNatCfg->verify_server	= verify_server;
	pNatCfg->tnl_timeout_sec	= tnl_timeout_sec;
//	memcpy(pNatSrvPort);	
	*pNatSrvPortCnt = natnl_srv_port_count;

	Cdbg(APP_DBG, "...............3");	
	for (i = 0; i<*pNatSrvPortCnt; i++){
		strcpy(pNatSrvPort[i].lport, srv_port_setting[i].lport);
		strcpy(pNatSrvPort[i].rport, srv_port_setting[i].rport);
	}
}


int NatReady(
	char* sip_acc, 
	char* sip_pwd,
	SrvInfo*	pSip_srv,
	SrvInfo*	pStun_srv,
	SrvInfo*	pTurn_srv
	//  char*	sip_srv,
	//char* stun_srv,
	//char* turn_srv
	  )
{
	int status =-1;
	Cdbg(APP_DBG, "Start to Set Config");	
	// read user ports setting from user define
	gNatCallback.on_natnl_tnl_event = &on_natnl_tnl_event; 
	USER_PORTS usr_ports[2]; memset(usr_ports, 0 , 2*sizeof (USER_PORTS));
	strcpy(usr_ports[0].local_data,		"4002");
	strcpy(usr_ports[0].external_data,	"4002");
	strcpy(usr_ports[0].local_ctl	,	"4003");
	strcpy(usr_ports[0].external_ctl,	"4003");

	strcpy(usr_ports[1].local_data,		"4004");
	strcpy(usr_ports[1].external_data,	"4004");
	strcpy(usr_ports[1].local_ctl	,	"4005");
	strcpy(usr_ports[1].external_ctl,	"4005");

	// read tunnel ports setting from user define
	struct natnl_srv_port NatSrvPort[2];
	memset(NatSrvPort,0, sizeof(NatSrvPort));
	strcpy(NatSrvPort[0].lport,	"6666");
	strcpy(NatSrvPort[0].rport,	"8888");
	strcpy(NatSrvPort[1].lport,	"7777");
	strcpy(NatSrvPort[1].rport, "8082");
	natnl_srv_port_count=2;

//	Cdbg(APP_DBG, "devticket =%s, userticket=%s", lg.deviceid, lg.userticket);

	Cdbg(APP_DBG, "Start to to set natnl_config");	
	// login sip server
	char sip_device_id[128]={0};
//	sprintf(sip_device_id,"%s@%s",lg.deviceid,"aaerelay.asuscomm.com");
	sprintf(sip_device_id,"%s@%s",sip_acc,"aaerelay.asuscomm.com");
	
#if 0
	char* pch = strstr(sip_device_id,":");
	int nbyte=0;
	if(pch) {
	   char tmp [128]={0};
	   nbyte = pch-sip_device_id;
	   strncpy(tmp, sip_device_id, nbyte);
	   memset(sip_device_id, 0, 128);
	   strcpy(sip_device_id, tmp);
	}
#endif
	Cdbg(APP_DBG, "Start to to set natnl_config>>>>>>>>>>>>>>>> sip reg id =%s", sip_device_id);	
	// read the tunnel settings from user 
	nat_setcfg(
		sip_device_id,
		sip_pwd,
//		lg.relayinfo,
		pSip_srv,
//		"54.251.34.52:5062",
//		lg.stuninfo,
		pStun_srv,
		2,
//		lg.turninfo, // 
		pTurn_srv, // 
		4, // debug level
		"", // log path
		0, // log flag
		1, // upnp flag
		2, // user port cnt
		//USER_PORTS*	user_ports,
		usr_ports,
		1, // use stun
		1,//	max_calls,
		0, //use_tls,
		0,//	verify_server,
		100,//		tnl_timeout_sec,
		natnl_srv_ports,
		NatSrvPort,	
		&natnl_srv_port_count,
		2,
		&gNatnl_cfg
	  );
//	Cdbg(APP_DBG, "Start to to load library");	
//	status = lib_load();
//	if(status){
//		Cdbg(APP_DBG, " load lib failed");	
//	}else{
		//Cdbg(APP_DBG, " load lib successfully call init func=%p", natnl_lib_init);	
		//status = natnl_lib_init(&gNatnl_cfg, &natnl_callback);
//		status = natnl_lib_init(&gNatnl_cfg, &gNatCallback);
		Cdbg(APP_DBG, "nat_init p =%p", nat_init);
		status = nat_init(&gNatnl_cfg, &gNatCallback);
//		status = natnl_lib_deinit();

//		status = natnl_lib_init(&gNatnl_cfg, &on_natnl_tnl_event);
//		Cdbg(APP_DBG, " init natnl lib done status=%d", status);	
//	}
//	status = lib_unload();
	return 0;
}



void parse_argv(char* argv[])
{
	/*
	if(argc != 2) return ;

	if( !strcmp(argv[1], "--init-websrv") ){


	}else if( !strcmp(argv[1], "--init-nat") ){

	}else if( !strcmp(argv[1], "--deinit-websrv")){

	}else if( !strcmp(argv[1], "--deinit-nat") ){
	
	}else{	// others input
		Cdbg(APP_DBG, "input arguments are invalid !!");
	}*/
	return ;
}
#endif
#if NVRAM
int nvram_is_aae_enable()
{
	int enable=0;
	char* aae_enable = nvram_get("aae_enable");
	if(aae_enable) {
		enable = atoi(aae_enable);	
	}
	return enable;
}

int nvram_set_aae_status(const char* aae_status )
{
	//const char* var = nvram_get("aae_status");
	return nvram_set("aae_status", aae_status);
}

int nvram_get_aae_pwd(char** aae_pwd)
{
	const char* var = nvram_get("aae_password");
	if(var){
		size_t pwdlen = strlen(var)+1;	
		*aae_pwd = (char*)malloc(pwdlen); 
		memset(*aae_pwd, 0 , pwdlen);
		strcpy(*aae_pwd, var);
		return 0;
	}else return -1;
}


int nvram_get_aae_username(char** aae_username)
{
	const char* var = nvram_get("aae_username");
	if(var){
		size_t usrlen = strlen(var)+1;	
		*aae_username = (char*)malloc(usrlen); 
		memset(*aae_username, 0 , usrlen);
		strcpy(*aae_username, var);
		return 0;
	}else return -1;
}
#endif

GetServiceArea	gsa; 
Login			lg; 
ListProfile		lp; 
pProfile		pP; 

int st_KeepAlive()
{
	int status;
	Keepalive ka;
	status = send_keepalive_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket, &ka);
	nvram_set_aae_status(ka.status);
	return status;
}

int st_UpdateProfile()
{
	int status;
	UpdateProfile up; memset(&up, 0, sizeof(up));
	status = send_update_profile_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket,  		
				pP->devicename, pP->deviceservice, "2",
				"", "","", &up);  
	nvram_set_aae_status(up.status);
	return status;
}

int st_ListProfile()
{
	int status;
	memset(&lp, 0, sizeof(lp));
	status = send_list_profile_req(gsa.servicearea, lg.cusid, lg.userticket, lg.deviceticket, "", "", &lp);
	// update profile list
	Cdbg(APP_DBG, "start update profile list");	
	pP = lp.pProfileList;
	nvram_set_aae_status(lp.status);
	return status;
}

int st_Logout()
{
	int status;
	Logout lgo; memset(&lgo, 0 , sizeof(lgo));
	status = send_logout_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket, &lgo);
	Cdbg(APP_DBG, "send logout req");
	nvram_set_aae_status(lgo.status);
	return status;
}

int st_Login(char* vip_id, char* vip_pwd)
{
	int				status =-1;

	memset(&gsa, 0, sizeof(gsa));
	status = send_getservicearea_req(
		SERVER,	//
		vip_id,
		vip_pwd,
		&gsa);	
	nvram_set_aae_status(gsa.status);
	if(strcmp (gsa.status, "0")){
		Cdbg(APP_DBG, "Get Service Area failed status value=%s", gsa.status);
		return -1;
	}
   //	call_state= GotServiceArea;
   // login webservice	
   Cdbg(APP_DBG, "start login...");	
   char md5string[MD_STR_LEN] = {0};	
   get_md5_string("00:0c:29:62:72:68", md5string);	
   char* device_type="05"; // 	Linux PC
   char* permission="1"; 	//	public:0, private:1
   char* devicename="rt-n16";
   char* deviceservice="0001";


   Cdbg(APP_DBG, "Start to >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Login, server=%s", gsa.servicearea);	
   // login service area
   memset(&lg, 0, sizeof(lg));
   status = send_login_req(gsa.servicearea, ASUS_VIP_ID, ASUS_VIP_PWD, md5string,
		 devicename, deviceservice, device_type, permission, &lg);
	nvram_set_aae_status(lg.status);
   if(strcmp(lg.status, "0")){
	  Cdbg(APP_DBG, "Login failed status =%s", lg.status);
		return -1;
	}
	return 0;
}

int go_state_machine()
{
	char*			aae_pwd	=NULL;
	char*			aae_id	=NULL;
#if NVRAM
	if( nvram_get_aae_username(&aae_id )){
		return -1;
	}	
	if( nvram_get_aae_pwd(&aae_pwd)){
		return -1;
	}	
#endif
	while(1)
	{
#if NVARM
		if(!nvram_is_aae_enable()) {
			nat_deinit(); 
		//	as = EndService;
			st_Logout();
			if(aae_id)	free(aae_id);
			if(aae_pwd) free(aae_pwd);
			break;
		}
#else
		/*
		if(as == EndService) {
			nat_deinit(); break;
		}*/
#endif
		Cdbg(APP_DBG, "event_text = %s", event_text);
		if(st == Unknown){
#if NVRAM
			st_Login(aae_id, aae_pwd);
#else
			st_Login(ASUS_VIP_ID, ASUS_VIP_PWD);
#endif
			st = Logined;
//			break;
		}else if(st == Logined){ // go to init
			Cdbg(APP_DBG, "Start to init NAT");
			NatReady(
				lg.deviceid, 
				lg.deviceticket,
				lg.relayinfoList,
				lg.stuninfoList,
				lg.turninfoList
			  );
//			st = Ready;
			st = Initializing; 
			// Waiting tnl_event for init done
		}else if(st == Initializing){ // go to ready 
			Cdbg(APP_DBG, "NAT event_code =%d", event_code);
			if (event_code == NATNL_TNL_EVENT_INIT_OK){
				st_ListProfile();
				st_UpdateProfile();
				st = Ready;
			}
			else if (event_code == NATNL_TNL_EVENT_INIT_FAILED){
				// go deinit
				nat_deinit();
				st = Deinitializing;
			}else{
				break;
			}
		}else if(st == Ready){ 	  // goto makecall, onCall, 
			// waiting for infinit
			if(event_code == NATNL_TNL_EVENT_MAKECALL_OK) st = OnCall;
			else st_KeepAlive();
		}else if(st == Deinitializing){  
			if(event_code == NATNL_TNL_EVENT_DEINIT_OK) st = Logined;
		}else if(st == OnCall){	// goto endcall
			if(event_code == NATNL_TNL_EVENT_HANGUP_OK) st = Ready;
			if(event_code == NATNL_TNL_EVENT_KA_TIMEOUT) st = EndingCall;
		}else if(st == EndingCall)   {
			if(event_code == NATNL_TNL_EVENT_HANGUP_OK) st = Ready;
		}
		sleep(1);
	}
	return 0;
}
int main (int argc, char* argv[])
{
	int status	= -1;
	
	if(status = init_natnl_api(&nat_init, &nat_deinit, &nat_makecall, &nat_hangup, &nat_dump))
	   return status;
	// Create a thread to watch NVRAM 
	while(1){
		Cdbg(APP_DBG, "pooling nvram aae_enable.....");
//		if(as != StartService) {
			if(nvram_is_aae_enable()){
		//		as = StartService;
				Cdbg(APP_DBG, "go State Machine");		
				go_state_machine();
			}
			//break;
//		}
		sleep(1);
	}
	//if(as == StartService) // may replace with if(aae_started)
//		go_state_machine();
		
#if 0
	// Start to Connet WB
	GetServiceArea	gsa; memset(&gsa, 0, sizeof(gsa));
	status = send_getservicearea_req(
		SERVER,	//
		ASUS_VIP_ID,
		ASUS_VIP_PWD,
		&gsa);	
	if(strcmp (gsa.status, "0")){
		Cdbg(APP_DBG, "Get Service Area failed status value=%s", gsa.status);
		return -1;
	}
//	call_state= GotServiceArea;
	// login webservice	
	Cdbg(APP_DBG, "start login...");	
	char md5string[MD_STR_LEN] = {0};	
	get_md5_string("00:0c:29:62:72:68", md5string);	
	char* device_type="05"; // 	Linux PC
	char* permission="1"; 	//	public:0, private:1
	char* devicename="rt-n16";
	char* deviceservice="0001";
	

	Cdbg(APP_DBG, "Start to >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Login, server=%s", gsa.servicearea);	
	// login service area
	Login			lg; memset(&lg, 0, sizeof(lg));
	status = send_login_req(gsa.servicearea, ASUS_VIP_ID, ASUS_VIP_PWD, md5string,
		devicename, deviceservice, device_type, permission, &lg);
	if(strcmp(lg.status, "0")){
		Cdbg(APP_DBG, "Login failed status =%s", lg.status);
		return -1;
	}
	st = Logined;
	// Start to go state machine
#endif	
//	nat_state_machine();	
	Cdbg(APP_DBG, "END..........");
	return 0;
}

#if 0
int main(int argc, char* argv[])
{
	int				status		= -1;
	GetServiceArea	gsa;
	Login			lg;//memset(&lg, 0, sizeof(lg));
	Login			lg_byticket;
		
	parse_argv(argv);

	Cdbg(APP_DBG, "start get service area...");	
	// get service area ...		
	status = send_getservicearea_req(
		SERVER,	//
		ASUS_VIP_ID,
		ASUS_VIP_PWD,
		&gsa);	
	if(strcmp (gsa.status, "0")){
		Cdbg(APP_DBG, "Get Service Area failed status value=%s", gsa.status);
		return -1;
	}
	call_state= GotServiceArea;
	// login webservice	
	Cdbg(APP_DBG, "start login...");	
	char md5string[MD_STR_LEN] = {0};	
	get_md5_string("00:0c:29:62:72:68", md5string);	
	char* device_type="05"; // 	Linux PC
	char* permission="1"; 	//	public:0, private:1
	char* devicename="rt-n16";
	char* deviceservice="0001";
	

	Cdbg(APP_DBG, "Start to >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Login, server=%s", gsa.servicearea);	
	// login service area
	status = send_login_req(gsa.servicearea, ASUS_VIP_ID, ASUS_VIP_PWD, md5string,
		devicename, deviceservice, device_type, permission, &lg);
	if(strcmp(lg.status, "0")){
		Cdbg(APP_DBG, "Login failed status =%s", lg.status);
		return -1;
	}
#if LOGIN_BY_TICKET
	char md5string_ticket[MD_STR_LEN] = {0};
	get_md5_string("00:aa:29:bb:72:68", md5string_ticket);	
	status = send_loginbyticket_req(gsa.servicearea,lg.userticket,  md5string_ticket,
		devicename, deviceservice, device_type, permission, &lg_byticket);
	Cdbg(APP_DBG, "test for >>>>> type =%s, permission=%s, dev=%s, devsvc=%s", 
		 device_type, permission, devicename, deviceservice );
	if(strcmp(lg_byticket.status, "0")){
		Cdbg(APP_DBG, "Login failed status =%s", lg_byticket.status);
		return -1;
	}
#endif
	call_state = Logined;
#if INT_NATNL
	Cdbg(APP_DBG, "go NatReady cusid = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.cusid);
	Cdbg(APP_DBG, "go NatReady userticket = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.userticket);
	Cdbg(APP_DBG, "go NatReady usernickname = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.usernickname);
	Cdbg(APP_DBG, "go NatReady deviceid = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.deviceid);
	Cdbg(APP_DBG, "go NatReady deviceticket = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.deviceticket);
	Cdbg(APP_DBG, "go NatReady relayinfo = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.relayinfoList->srv_ip);
	Cdbg(APP_DBG, "go NatReady stuninfo = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.stuninfoList->srv_ip);
	Cdbg(APP_DBG, "go NatReady turninfo = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.turninfoList->srv_ip);
	Cdbg(APP_DBG, "go NatReady expiretime = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.deviceticketexpiretime);
	Cdbg(APP_DBG, "go NatReady time = %s<<<<<<<<<<<<<<<<<<<<<<<<<<", lg.time);
	status = NatReady(
		lg.deviceid, 
		lg.deviceticket,
		lg.relayinfoList->srv_ip,
		lg.stuninfoList->srv_ip,
		lg.turninfoList->srv_ip
		);
#endif	
#if USE_WEB_API
	//get friend list	
	Cdbg(APP_DBG, "start query friend list");	
	QueryFriend qf;memset(&qf, 0, sizeof(qf));
	send_query_friend_req(gsa.servicearea, lg.userticket, &qf);
	Cdbg(APP_DBG, ">>>>>>>>>>>>1, qf time =%s", qf.time);
	// release list later 
	// List profile
	Cdbg(APP_DBG, "start get profile list");	
	// test for firt node.
//	char* cusid 		= qf.FriendList->cusid;
	char* cusid 		= lg.cusid;
	char* userticket	= lg.userticket;
	char* deviceticket 	= lg.deviceticket;
	char* friendid		= "";
	char* deviceid		= "";
	Cdbg(APP_DBG, "cusid=%s, userticket=%s, deviceticket=%s, friendid=%s, deviceid=%s",
		cusid, userticket, deviceticket, friendid, deviceid);
	ListProfile lp; memset(&lp, 0, sizeof(lp));
	send_list_profile_req(gsa.servicearea, cusid, userticket, deviceticket, friendid, deviceid, &lp);

	
	// update profile list
	Cdbg(APP_DBG, "start update profile list");	
	pProfile pP = lp.pProfileList;
#if 0 
//	send_update_profile_req(gsa.servicearea, lg.cusid, "0010011300000000018850", lg.deviceticket,  		
	send_update_profile_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket,  		
				"ME301T", "", "",
				"", "8","");  
#else
	UpdateProfile up; memset(&up, 0, sizeof(up));
	send_update_profile_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket,  		
//				pP->devicename, pP->deviceservice, pP->devicestatus,
				pP->devicename, pP->deviceservice, "2",
				"", "","", &up);  
#endif
	/*
	// get webpath ,  NOT DONE!!!!!!!!!!!!!!!!! still need to develop
	Cdbg(APP_DBG, "start get webpath");	
	Getwebpath gw; memset(&gw, 0, sizeof(gw));
//	send_getwebpath_req(gsa.servicearea, gw.userticket, "http://xxx/xxx/xxx" );

	// Create Pin
	Cdbg(APP_DBG, "start create pin ");	
	CreatePin cp; memset(&cp, 0, sizeof(cp));
	send_create_pin_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket, &cp);
	
	// query pin
	Cdbg(APP_DBG, "start create pin ");
	QueryPin qp; memset(&qp, 0, sizeof(qp));
	send_query_pin_req(gsa.servicearea, lg.cusid, lg.deviceticket, cp.pin, &qp);	

	Keepalive ka;
	send_keepalive_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket, &ka);

	// Updateiceinfo 
//	Updateiceinfo ui;
//	send_updateiceinfo_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket,"", &ui);	
	*/
#endif

#if 0 // No need to logout 
	// logout
	Cdbg(APP_DBG, "start logout");	
	Logout lgo; memset(&lgo, 0 , sizeof(lgo));
	send_logout_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket, &lgo);
	Cdbg(APP_DBG, "send logout req");
#endif
#if 0
	// Chris: not sure when to use
	// Unregister  Device
	UnregisterDevice ud; memset(&ud, 0, sizeof(ud));
 	send_unregister_device_req(gsa.servicearea, lg.cusid, lg.deviceid, lg.deviceticket, &ud);
#endif
	while(1){
		sleep(10);	
		Cdbg(APP_DBG, "=========================> call state =%d", call_state);
	}

	return 0;
}
#endif

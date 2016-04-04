#ifndef _WS_CALLER_H
#define _WS_CALLER_H
#include <ws_api.h>					// include the header of web service 
int st_KeepAlive(GetServiceArea* gsa, Login* lg);
//int st_UpdateProfile(const char* update_status, char* local_ip, char* public_ip, char* mac_addr, GetServiceArea* gsa, Login* lg);
int st_UpdateProfile(const char* update_status, int tnl_type, char* mac_addr, char* tnl_sdk_version, GetServiceArea* gsa, Login* lg);
int st_ListProfile( GetServiceArea* gsa, Login* lg, ListProfile* lp, pProfile* pP );
int st_Logout(GetServiceArea* gsa, Login* lg);
int st_Login(GetServiceArea* gsa, Login* lg, char* vip_id, char* vip_pwd);
int st_KeepAlive_loop(int sec, int* is_terminate);
int st_KeepAlive_threading(int sec, int* is_terminate);
int st_KeepAlive_thread_exit();
#endif

#ifndef _NATAPI_H
#define _NATAPI_H
#include <dlfcn.h>
#include <natnl_lib.h>
int lib_load(void** handle, const char* lib_path);
int lib_unload	(void* handle);
int lib_get_func(void* handle, const char* func_name, void** func_sym);


// function pointer
typedef char* (*NAT_VERSION)	(void);
#ifdef TNL_CALLBACK_ENABLE
typedef int (*NAT_INIT3)	(struct natnl_config *cfg, struct natnl_callback *natnl_cb, void *app_data);
#else
typedef int (*NAT_INIT)		(struct natnl_config *cfg);
#endif
typedef int (*NAT_DEINIT)	(void);
typedef int (*NAT_MAKECALL)	(char *device_id, int srv_port_count,
								struct natnl_srv_port srv_port[], int *call_id,
							    char *user_id, int timeout_sec);
typedef int (*NAT_HANG_UP)	(int call_id);

typedef int (*NAT_POOL_DUMP)(int detail);

typedef int (*NAT_DETECT) (char *stun_srv);

int init_natnl_api(NAT_INIT3* nat_int3, NAT_DEINIT* nat_deinit, NAT_MAKECALL* nat_makecall, NAT_HANG_UP* nat_hangup, NAT_POOL_DUMP* nat_dump, NAT_DETECT* nat_detect, NAT_VERSION* nat_version);
#endif

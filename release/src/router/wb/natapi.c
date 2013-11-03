#include <natapi.h>
#include <stdio.h>
#include <log.h>

#define NATAPI_DBG 1
int lib_get_func(void* handle, const char* func_name, void** func_sym);
// function pointer
int (*natnl_lib_init)	(struct natnl_config *cfg, struct natnl_callback *natnl_cb);
int (*natnl_lib_deinit)	(void);
int (*natnl_make_call)	(char *device_id, int srv_port_count,
								struct natnl_srv_port srv_port[], int *call_id,
							    char *user_id, int timeout_sec);
int (*natnl_hangup_call)	(int call_id);

int (*natnl_pool_dump)	(int detail);



// global variable declare
void* lib_handle;

int lib_unload(void* handle)
{
	dlclose(handle);
}

int lib_load(void** handle, const char* lib_path)
{
	*handle = NULL;
	*handle = dlopen(lib_path, RTLD_LAZY);
	if(!*handle){
		Cdbg(NATAPI_DBG, "dll get functions error =%s", dlerror());
		return -1;	
	}
	else return 0;
}

int lib_get_func(void* handle, const char* func_name, void** func_sym)
{
	if(!handle) return -1;
	*func_sym = dlsym(handle, func_name);
//	if(!*func_sym) return dlerror();
	if(!*func_sym) {
		Cdbg(NATAPI_DBG, "dll get functions error =%s", dlerror());
		return -1;
	}
	return 0;
}

int deinit_natnl_api()
{
	return 	lib_unload(lib_handle);
}

int init_natnl_api(NAT_INIT* nat_init, NAT_DEINIT* nat_deinit, NAT_MAKECALL* nat_makecall, NAT_HANG_UP* nat_hangup, NAT_POOL_DUMP* nat_dump )
{	
	int err = -1;
	char* error;	
	err = lib_load(&lib_handle, "./natlib/libasusnatnl.so");
	//	void *handle=NULL;
	Cdbg(NATAPI_DBG, "dlopen handle =%p\n", lib_handle);
	if (!lib_handle) {
	    fprintf(stderr, "%s\n", dlerror());

//		exit(EXIT_FAILURE);
	} else {
#if 0
	    *(void **) (&natnl_lib_init)     =   dlsym(lib_handle, "natnl_lib_init");
	//	natnl_lib_init     = (int (*)(struct natnl_config*, struct natnl_callback*) )  dlsym(handle, "natnl_lib_init");
	    *(void **) (&natnl_lib_deinit)   =   dlsym(lib_handle, "natnl_lib_deinit");
	    *(void **) (&natnl_make_call)    =   dlsym(lib_handle, "natnl_make_call");
	    *(void **) (&natnl_hangup_call)  =   dlsym(lib_handle, "natnl_hangup_call");
		*(void **) (&natnl_pool_dump)	=   dlsym(lib_handle, "natnl_pool_dump");
		Cdbg(NATAPI_DBG, "init =%p\n, deinit=%p\n, makecall=%p\n, pool_dump=%p\n", natnl_lib_init,
			  natnl_lib_deinit, natnl_make_call, natnl_pool_dump);	
#else 
	    *nat_init	=   (NAT_INIT)		dlsym(lib_handle, "natnl_lib_init");
	    *nat_deinit	=   (NAT_DEINIT)	dlsym(lib_handle, "natnl_lib_deinit");
	    *nat_makecall=   (NAT_MAKECALL)	dlsym(lib_handle, "natnl_make_call");
		*nat_hangup	=   (NAT_HANG_UP)	dlsym(lib_handle, "natnl_hangup_call");
		*nat_dump	=   (NAT_POOL_DUMP)	dlsym(lib_handle, "natnl_pool_dump");
		Cdbg(NATAPI_DBG, "init =%p\n, deinit=%p\n, makecall=%p\n, pool_dump=%p\n", *nat_init,
			  *nat_deinit, *nat_makecall, *nat_dump);	
#endif
	    if ((error = dlerror()) != NULL)  {
			fprintf(stderr, "%s\n", error);
			err =-1;
		//	exit(EXIT_FAILURE);
		}
	}
//	dlclose(handle);
	return err;
}

#if 0
int lib_load() {
//	void *handle=NULL;
	char *error;
//	handle = dlopen("/home/charles0000/wb/linux-natnl/libasusnatnl.so", RTLD_LAZY);
	handle = dlopen("libasusnatnl.so", RTLD_LAZY);
	Cdbg(APP_DBG, "dlopen handle =%p\n", handle);
	if (!handle) {
	    fprintf(stderr, "%s\n", dlerror());
		exit(EXIT_FAILURE);
	} else {
	    *(void **) (&natnl_lib_init)     =   dlsym(handle, "natnl_lib_init");
	//	natnl_lib_init     = (int (*)(struct natnl_config*, struct natnl_callback*) )  dlsym(handle, "natnl_lib_init");
	    *(void **) (&natnl_lib_deinit)   =   dlsym(handle, "natnl_lib_deinit");
	    *(void **) (&natnl_make_call)    =   dlsym(handle, "natnl_make_call");
	    *(void **) (&natnl_hangup_call)  =   dlsym(handle, "natnl_hangup_call");
		*(void **) (&natnl_pool_dump)	=   dlsym(handle, "natnl_pool_dump");
		Cdbg(APP_DBG, "init =%p\n, deinit=%p\n, makecall=%p\n, pool_dump=%p\n", natnl_lib_init,
			  natnl_lib_deinit, natnl_make_call, natnl_pool_dump);	
	    if ((error = dlerror()) != NULL)  {
			fprintf(stderr, "%s\n", error);
			exit(EXIT_FAILURE);
		}
	}
//	dlclose(handle);
	return 0;
}

#endif

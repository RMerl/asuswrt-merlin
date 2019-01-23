
#ifndef IM_IPC_H
#define IM_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(WIN32) && !defined(PJ_ANDROID)
#include <sys/ipc.h>
#include <sys/shm.h> 
#include <signal.h>
#endif

#define MAX_IM_MSG_SIZE 1024

/**
	The data type that is stored in shared memory.
 */
typedef enum im_shm_type {
	IM_SHM_TYPE_REQUEST,
	IM_SHM_TYPE_RESPONSE
} im_shm_type;

/**
	The data structure of shared memroy.
 */
typedef struct im_shm_data {
	im_shm_type type;
	int data_len;
	char data[MAX_IM_MSG_SIZE];
} im_shm_data;

#define MAX_IM_SHM_DATA_SIZE sizeof(im_shm_data)

// Signal defined
#ifdef PJ_CONFIG_IPHONE
#define IM_MSG_SIG_REQ SIGUSR2
#else
#define IM_MSG_SIG_REQ __SIGRTMAX
#endif
//#define IM_MSG_SIG_RES SIGRTMIN+1

// Shared memory key defined
#define IM_MSG_SHM_KEY 62281

/**
	- The function is for ANT SDK used.
	- When the server-side ANT receive the im message, 
	  it should call the function to wirte im message to shared memory 
	  and send the IM_MSG_SHM_KEY signal to the specific process.
	@param:
		arg : The pointer to a ANT's natnl_im_data structure.
	@param:
		msg_buf : The message buffer to read im message.
	@return
		 0 : Success.
		-1 : The process not found.
		-2 : Create shared memory failed.
		-3 : Attach to shared memroy failed.
 */
int send_im_by_sig(void *arg);

/**
	- The function is for server-side application used.
	- When the server-side application receive the IM_MSG_SIG_REQ signal, 
	  it should call the function to read im message from shared memory 
	  and perform relative action.
	@param:
		msg_buf : The message buffer to read im message. 
				  !!!IMPORTANT!!! The caller must be responsible for freeing it.
	@return
		 0 : Success.
		-1 : Locate shared memory failed.
		-2 : Attach to shared memroy failed.
 */
int read_im_msg_from_shm(char **msg_buf);

/**
	- The function is for server-side application used.
	- When the application handled the im message completely,
	  it should call this function to write a response message 
	  back the shared memory.
	- The ANT SDK will send back the response message to the client-side 
	application.
	@param:
		res_msg : The response message. It should less than 
				  MAX_IM_MSG_SIZE bytes.
	@return:
		 0 : Success.
		-1 : Invalid argument.
		-2 : Locate shared memory failed.
		-3 : Attach to shared memroy failed.
 */
int write_im_resp_to_shm(char *resp_msg);

#ifdef __cplusplus
}
#endif

#endif

#include <stdio.h>
#include <stdlib.h>
#include <push_log.h>

#ifndef APP_IPKG
#include <bcmnvram.h>
#endif


#if 0
#include <ws_api.h>

int main(int argc, char *argv[]){
	int ret;
	Push_Msg psm;

	// get push_sendmsg...	
	ret = send_push_msg_req(SERVER, nvram_safe_get("et0macaddr"), "", "HTTP login: James test.", &psm);

	printf("psm.status=%d.\n", atoi(psm.status));

	return ret;
}
#else
#define NOTIFYLOG_FILE "/tmp/notifylog.log"

int main(int argc, char *argv[]){
	int ret;

#if 0
	if(argc != 1 && argc != 2){
		printf("Usage: push_notify [interval seconds]\n");
		return -1;
	}

	if(argc == 1)
		ret = getlogbyinterval(NOTIFYLOG_FILE, 0, 0);
	else
		ret = getlogbyinterval(NOTIFYLOG_FILE, atoi(argv[1]), 0);
#else
	if(argc != 2){
		printf("Usage: push_notify [time string]\n");
		return -1;
	}

	ret = getlogbytimestr(NOTIFYLOG_FILE, argv[1], 0);
#endif

	return ret;
}
#endif


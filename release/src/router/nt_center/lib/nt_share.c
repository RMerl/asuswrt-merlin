#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <libnt.h>
#include <nt_eInfo.h>

int GetDebugValue(char *path)
{
	FILE *fp;
	char *np;
	char val[8];
	fp = fopen(path, "r");
	if(fp) {
		fgets(val,3,fp);
		np = strrchr(val, '\n');
		if (np) *np='\0';
		fclose(fp);
		return atoi(val);
	} else {
		return 0;
	}
}

int eInfo_get_idx_by_evalue(int e){
	int i;
	for(i=0; mapInfo[i].value != 0; i++) {
		if( mapInfo[i].value == e)
			return i;
	}
	return -1;
}

char *eInfo_get_eName(int e)
{
	int idx;
	idx = eInfo_get_idx_by_evalue(e);
	if (idx < 0)
		return NULL;
	else
		return mapInfo[idx].eName;
}

NOTIFY_EVENT_T *initial_nt_event()
{
	NOTIFY_EVENT_T *e;
	e = malloc(sizeof(NOTIFY_EVENT_T));
	if (!e) 
		return NULL;
	memset(e, 0, sizeof(NOTIFY_EVENT_T));
	return e;
}

void nt_event_free(NOTIFY_EVENT_T *e)
{
	if(e) free(e);
}

int send_notify_event(void *data, char *socketpath)
{
	struct    sockaddr_un addr;
	int       sockfd, n;
	
	if ( (sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		printf("[%s:(%d)] ERROR socket.\n", __FUNCTION__, __LINE__);
		perror("socket error");
		return 0;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socketpath, sizeof(addr.sun_path)-1);
	
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("[%s:(%d)] ERROR connecting:%s.\n", __FUNCTION__, __LINE__, strerror(errno));
		perror("connect error");
		close(sockfd);
		return 0;
	}

	n = write(sockfd, (NOTIFY_EVENT_T *)data, sizeof(NOTIFY_EVENT_T));
	
	close(sockfd);
	
	if(n < 0) {
		printf("[%s:(%d)] ERROR writing:%s.\n", __FUNCTION__, __LINE__, strerror(errno));
		perror("writing error");
		return 0;
	}
	
	return 1;
}

int send_trigger_event(NOTIFY_EVENT_T *event)
{
	//TODO: Check EVENT ID
	return send_notify_event(event, NOTIFY_CENTER_SOCKET_PATH);
}


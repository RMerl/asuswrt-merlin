#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <math.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <protect_srv.h>

#ifndef RTCONFIG_NOTIFICATION_CENTER
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
		return (int)strtol(val, NULL, 10);
	} else {
		return 0;
	}
}

void Debug2Console(const char * format, ...)
{
	FILE *f;
	int nfd;
	va_list args;
	
	if (((nfd = open("/dev/console", O_WRONLY | O_NONBLOCK)) > 0) &&
	    (f = fdopen(nfd, "w")))
	{
		va_start(args, format);
		vfprintf(f, format, args);
		va_end(args);
		fclose(f);
	}
	else
	{
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
	}
	
	if (nfd != -1) close(nfd);
}

int isFileExist(char *fname)
{
	struct stat fstat;
	
	if (lstat(fname,&fstat)==-1)
		return 0;
	if (S_ISREG(fstat.st_mode))
		return 1;
	
	return 0;
}
#endif

static PTCSRV_STATE_REPORT_T *initial_ptcsrv_event()
{
	PTCSRV_STATE_REPORT_T *e;
	e = malloc(sizeof(PTCSRV_STATE_REPORT_T));
	if (!e) 
		return NULL;
	memset(e, 0, sizeof(PTCSRV_STATE_REPORT_T));
	return e;
}

static void ptcsrv_event_free(PTCSRV_STATE_REPORT_T *e)
{
	if(e) free(e);
}

static int send_ptcsrv_event(void *data, char *socketpath)
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
	
	n = write(sockfd, (PTCSRV_STATE_REPORT_T *)data, sizeof(PTCSRV_STATE_REPORT_T));
	
	close(sockfd);
	
	if(n < 0) {
		printf("[%s:(%d)] ERROR writing:%s.\n", __FUNCTION__, __LINE__, strerror(errno));
		perror("writing error");
		return 0;
	}
	
	return 1;
}

void SEND_PTCSRV_EVENT(int s_type, const char *addr, const char *msg)
{
	/* Do Initial first */
	PTCSRV_STATE_REPORT_T *state_t = initial_ptcsrv_event();
	
	state_t->s_type = s_type;
	snprintf(state_t->addr, sizeof(state_t->addr), addr);
	snprintf(state_t->msg, sizeof(state_t->msg), msg);
	
	/* Send Event to Protection Server */
	send_ptcsrv_event(state_t, PROTECT_SRV_SOCKET_PATH);
	
	/* Free malloc via initial_ptcsrv_event() */
	ptcsrv_event_free(state_t);
}


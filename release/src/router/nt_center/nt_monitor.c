#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <libnt.h>

#define NORMAL_STATUS_INTERVAL	10
#define NULL_STATUS_INTERVAL	60
#define ONCE_CHECK_INTERVAL	1

#define NT_DAEMON_NUM	3
#define MyDBG(fmt,args...) \
	if(isFileExist(NOTIFY_CENTER_MONITOR_DEBUG) > 0) { \
		Debug2Console("[NCM][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}

typedef enum {
	STATUS_NULL = 0,
	STATUS_NORMAL,
	STATUS_RESTART,
	STATUS_RESTARTING,
	STATUS_STOP,
	STATUS_ERROR
} NTM_STATUS_T;

typedef enum {
	SIG_NULL = 0,
	SIG_RESTART,
	SIG_STOP
} NTM_SIG_T;

NTM_SIG_T m_sig = SIG_NULL;
NTM_STATUS_T m_status = STATUS_RESTART;

int miss_count = 0;
int CTRL_SEC = NORMAL_STATUS_INTERVAL;

static void handlesignal(int signum) 
{
	if (signum == SIGUSR1)
	{
		m_sig = SIG_RESTART;
		CTRL_SEC = ONCE_CHECK_INTERVAL; /* need check status immeditaely */
	}
	else if (signum == SIGUSR2)
	{
		m_sig = SIG_STOP;
		CTRL_SEC = ONCE_CHECK_INTERVAL; /* need check status immeditaely */
	}
	else
		printf("Unknown SIGNAL\n");
}

static void signal_register(void) {
	struct sigaction sa;
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler =  &handlesignal;
	sigaction(SIGUSR1, &sa, NULL); /* restart */
	sigaction(SIGUSR2, &sa, NULL); /* stop    */
}

static void generate_pid_file()
{
	FILE *fp;
	
	fp = fopen(NOTIFY_CENTER_MONITOR_PID_PATH, "wt");
	if (fp) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
}

int main(int argc, char* argv[])
{
	generate_pid_file();
	signal_register();
	
	/* start Mail server, NOT monitor on it. */
	system("nt_actMail &");
	
	do
	{
		MyDBG("Daemon Num :[%d]\n", get_pid_num_by_name("nt_center"));
		if (m_status == STATUS_RESTART)
		{
			/* just in case, kill nt_center again */
			if (get_pid_num_by_name("nt_center") > 0)
			{
				system("killall nt_center");
				usleep(5000);
				MyDBG("KILL AGAIN\n");
				continue;
			}
			
			system("nt_center &");
			
			MyDBG("STATUS CHANGE TO : [STATUS_RESTARTING]\n");
			m_status = STATUS_RESTARTING;
			miss_count = 0;
			continue;
		}
		else if (m_status == STATUS_RESTARTING)
		{
			if (get_pid_num_by_name("nt_center") < NT_DAEMON_NUM)
			{
				/* something wrong, restart again */
				if (miss_count == 2)
				{
					system("killall nt_center");
					sleep(1);
					MyDBG("STATUS CHANGE TO : [STATUS_RESTART]\n");
					m_status = STATUS_RESTART;
					miss_count = 0;
				}
				else  /* if it's in restarting status, wait 2 seconds before we try to restart again */
				{
					miss_count++;
					MyDBG("RETRY CNT:[%d]\n", miss_count);
					sleep(2);
					continue;
				}
			}
			else
			{
				MyDBG("STATUS CHANGE TO : [STATUS_NORMAL]\n");
				m_status = STATUS_NORMAL;
				miss_count = 0;
				CTRL_SEC = NORMAL_STATUS_INTERVAL;
			}
		}
		else if (m_status == STATUS_STOP)
		{
			MyDBG("PRECESSING : [STATUS_STOP]\n");
			system("killall nt_center");
			sleep(1);
			m_status = STATUS_NULL;
		}
		else if (m_status == STATUS_ERROR)
		{
			MyDBG("PRECESSING : [STATUS_ERROR]\n");
			system("killall nt_center");
			sleep(1);
			MyDBG("STATUS CHANGE TO : [STATUS_RESTART]\n");
			m_status = STATUS_RESTART;
			continue;
		}
		else if (m_status == STATUS_NULL)
		{
			MyDBG("PRECESSING : [STATUS_NULL]\n");
		
		}
		else  /* STATUS_NORMAL */
		{
			if (get_pid_num_by_name("nt_center") < NT_DAEMON_NUM || get_pid_num_by_name("nt_center") > NT_DAEMON_NUM)
			{
				m_status = STATUS_ERROR;
				MyDBG("STATUS CHANGE TO : [STATUS_ERROR]\n");
				continue;
			}
		}
		
		// Check signal
		if (m_sig == SIG_RESTART)
		{
			system("killall nt_center");
			m_status = STATUS_RESTART;
			MyDBG("[SIG] STATUS CHANGE TO : [STATUS_RESTART]\n");
			m_sig = SIG_NULL;
			sleep(1);
		}
		else if (m_sig == SIG_STOP)
		{
			m_status = STATUS_STOP;
			MyDBG("[SIG] STATUS CHANGE TO : [STATUS_STOP]\n");
			m_sig = SIG_NULL;
		}
		
		sleep(CTRL_SEC);
	}while(1);
	
	return 0;
}


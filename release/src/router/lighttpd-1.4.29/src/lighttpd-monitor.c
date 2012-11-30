#include "log.h"
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#ifdef EMBEDDED_EANBLE 
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#include "nvram_control.h"
#endif

#define DBE 1
#define LIGHTTPD_PID_FILE_PATH	"/tmp/lighttpd/lighttpd.pid"
#define LIGHTTPD_EXE_CMD	"lighttpd -f /tmp/lighttpd.conf -D &"

#define LIGHTTPD_MONITOR_PID_FILE_PATH	"/tmp/lighttpd/lighttpd-monitor.pid"

static siginfo_t last_sigterm_info;
static siginfo_t last_sighup_info;

static volatile sig_atomic_t start_process    = 1;
static volatile sig_atomic_t graceful_restart = 0;
static volatile pid_t pid = -1;

#define BINPATH SBIN_DIR"/lighttpd-monitor"
#define UNUSED(x) ( (void)(x) )

int is_shutdown = 0;

static void sigaction_handler(int sig, siginfo_t *si, void *context) {
	static siginfo_t empty_siginfo;
	UNUSED(context);
	
	if (!si) si = &empty_siginfo;

	switch (sig) {
	case SIGTERM:		
		is_shutdown = 1;
		break;
	case SIGINT:
		break;
	case SIGALRM: 
		break;
	case SIGHUP:
		break;
	case SIGCHLD:
		break;
	}
}

void stop_arpping_process()
{
	FILE *fp;
    	char buf[256];
    	pid_t pid = 0;
    	int n;

	if ((fp = fopen("/tmp/lighttpd/lighttpd-arpping.pid", "r")) != NULL) {
		if (fgets(buf, sizeof(buf), fp) != NULL)
	            pid = strtoul(buf, NULL, 0);
	        fclose(fp);
	}
	
	if (pid > 1 && kill(pid, SIGTERM) == 0) {
		n = 10;	       
	       while ((kill(pid, SIGTERM) == 0) && (n-- > 0)) {
	            Cdbg(DBE,"Mod_smbdav: %s: waiting pid=%d n=%d\n", __FUNCTION__, pid, n);
	            usleep(100 * 1000);
	        }
	}

	unlink("/tmp/lighttpd/lighttpd-arpping.pid");
}

int main(int argc, char **argv) {
	
	UNUSED(argc);

	//- Check if same process is running.
	FILE *fp = fopen(LIGHTTPD_MONITOR_PID_FILE_PATH, "r");
	if (fp) {
		fclose(fp);
		return 0;
	}
	
	//- Write PID file
	pid_t pid = getpid();
	fp = fopen(LIGHTTPD_MONITOR_PID_FILE_PATH, "w");
	if (!fp) {
		exit(EXIT_FAILURE);
	}
	fprintf(fp, "%d\n", pid);
	fclose(fp);
	
#if EMBEDDED_EANBLE
	sigset_t sigs_to_catch;

	/* set the signal handler */
    	sigemptyset(&sigs_to_catch);
    	sigaddset(&sigs_to_catch, SIGTERM);
    	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

    	signal(SIGTERM, sigaction_handler);  
#else
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);

	act.sa_sigaction = sigaction_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);	
#endif

	time_t prv_ts = time(NULL);

	int stop_arp_count = 0;
	int commit_count = 0;
	
	while (!is_shutdown) {
		
		sleep(10);

		int start_lighttpd = 0;
		int start_lighttpd_arp = 0;
	
		time_t cur_ts = time(NULL);
		
	#if EMBEDDED_EANBLE
  		if (!pids("lighttpd")){
			start_lighttpd = 1;
  		}

		if (!pids("lighttpd-arpping")){
			start_lighttpd_arp = 1;
  		}
	#else
		if (!system("pidof lighttpd")){
			start_lighttpd = 1;
		}

		if (!system("pidof lighttpd-arpping")){
			start_lighttpd_arp = 1;
		}
	#endif
		
		//-every 30 sec 
		if(cur_ts - prv_ts >= 30){
	
			if(start_lighttpd){
				Cdbg(DBE, "try to start lighttpd....");

			#if EMBEDDED_EANBLE
				system(LIGHTTPD_EXE_CMD);
			#else
				system("./_inst/sbin/lighttpd -f lighttpd.conf &");
			#endif
			}

			if(start_lighttpd_arp){
				Cdbg(DBE, "try to start lighttpd arpping....");
				
			#if EMBEDDED_EANBLE
				system("lighttpd-arpping -f br0 &");
			#else
				system("./_inst/sbin/lighttpd-arpping -f eth0 &");
			#endif
			}

			//-every 2 hour
			if(stop_arp_count>=240){
				stop_arpping_process();
				stop_arp_count=0;
			}

			//-every 12 hour
			if(commit_count>=1440){				

				#if EMBEDDED_EANBLE
				int i, act;
				for (i = 30; i > 0; --i) {
			    	if (((act = check_action()) == ACT_IDLE) || (act == ACT_REBOOT)) break;
			        fprintf(stderr, "Busy with %d. Waiting before shutdown... %d", act, i);
			        sleep(1);
			    }
					
				nvram_do_commit();
				#endif
				
				commit_count=0;
			}
			
			prv_ts = cur_ts;
			stop_arp_count++;
			commit_count++;
		}
	}

	fprintf(stderr, "Success to terminate lighttpd-monitor.....\n");
	
	exit(EXIT_SUCCESS);
	
	return 0;
}


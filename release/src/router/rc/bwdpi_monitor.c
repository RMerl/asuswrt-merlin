/*
	bwdpi_monitor.c for record bwdpi traffic log
*/

#include <rc.h>
#include <bwdpi.h>

// TEST 1 : every 30 secs
// TEST 0 : every hour
#define TEST 0

static int sig_cur = -1;

static void save_traffic_record()
{
	system("bwdpi_sqlite -e");
}

static void catch_sig(int sig)
{
	sig_cur = sig;

	if (sig == SIGALRM)
	{
		save_traffic_record();
	}
	else if (sig == SIGTERM)
	{
		remove("/var/run/bwdpi_monitor.pid");
		exit(0);
	}
}

int bwdpi_monitor_main(int argc, char **argv)
{
	printf("starting bwdpi monitor...\n");

	FILE *fp;
	sigset_t sigs_to_catch;
#ifdef TEST
	debug = 1;
#else
	debug = nvram_get_int("bwdpi_debug");
#endif
	
	/* write pid */
	if ((fp = fopen("/var/run/bwdpi_monitor.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGTERM, catch_sig);
	signal(SIGALRM, catch_sig);

	while(1)
	{
		if (nvram_match("svc_ready", "1"))
		{
			struct tm local;
			time_t now;
			int diff_sec = 0;

			time(&now);
			localtime_r(&now, &local);
			if(debug) dbg("[bwdpi monitor]: %d-%d-%d, %d:%d:%d\n",
				local.tm_year+1900, local.tm_mon+1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

#if TEST
			/* every 30 secs */
			if(local.tm_sec < 30 && local.tm_sec >= 0)
				diff_sec = 30 - local.tm_sec;
			else if(local.tm_sec < 60 && local.tm_sec >= 30)
				diff_sec = 60 - local.tm_sec;
			alarm(diff_sec);
			pause();
#else
			/* every hour */
			if((local.tm_min != 0) || (local.tm_sec != 0)){
				diff_sec = 3600 - (local.tm_min * 60 + local.tm_sec);
				alarm(diff_sec);
				pause();
			}
			else{
				alarm(3600);
				pause();
			}
#endif
		}
		else
		{
			if(debug) dbg("[bwdpi monitor] ntp not sync, can't record bwdpi log\n");
			pause();
		}
	}

	return 0;
}


/*
	bwdpi_monitor.c for record bwdpi traffic database
*/

#include <rc.h>
#include <bwdpi.h>

static int sig_cur = -1;

static void save_traffic_record()
{
	char *db_type;
	char buf[120]; // path buff = 120
	int db_mode;

	debug = nvram_get_int("sqlite_debug");

	if(!f_exists("/dev/detector") || !f_exists("/dev/idpfw")){
		printf("DPI engine doesn't exist, not to save traffic record\n");
		return;
	}

	memset(buf, 0, sizeof(buf));

	// bwdpi_db_type
	db_type = nvram_safe_get("bwdpi_db_type");
	if(!strcmp(db_type, "") || db_type == NULL)
		db_mode = 0;
	else
		db_mode = atoi(db_type);

	if(db_mode == 0)
	{
		sprintf(buf, "bwdpi_sqlite -e -s NULL");
	}
	else if(db_mode == 1)
	{
		if(!strcmp(nvram_safe_get("bwdpi_db_path"), ""))
			sprintf(buf, "bwdpi_sqlite -e -s NULL");
		else
			sprintf(buf, "bwdpi_sqlite -e -p %s -s NULL", nvram_safe_get("bwdpi_db_path"));
	}
	else if(db_mode == 2)
	{
		// cloud server : not ready
		printf("traffic statistics not support cloud server yet!!\n");
	}
	else
	{
		printf("Not such database type!!\n");
		return;
	}

	if(debug) dbg("db_mode = %d, buf = %s\n", db_mode, buf);

	system(buf);
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
	int mode;
	char *db_debug;

	debug = nvram_get_int("sqlite_debug");

	// bwdpi_sqlite_enable != 1
	if(strcmp(nvram_safe_get("bwdpi_db_enable"), "1")){
		printf("bwdpi_monitor is disabled!!\n");
		exit(0);
	}

	// bwdpi_monitor != 1
	db_debug = nvram_safe_get("bwdpi_db_debug");
	if(!strcmp(db_debug, "") || db_debug == NULL)
		mode = 0;
	else if(!strcmp(db_debug, "1"))
		mode = 1;
	else
		mode = 0;
	
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
		if (nvram_get_int("ntp_ready"))
		{
			struct tm local;
			time_t now;
			int diff_sec = 0;

			time(&now);
			localtime_r(&now, &local);
			if(debug) dbg("[bwdpi monitor]: %d-%d-%d, %d:%d:%d\n",
				local.tm_year+1900, local.tm_mon+1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

			if(mode){
				/* every 30 secs */
				if(local.tm_sec < 30 && local.tm_sec >= 0)
					diff_sec = 30 - local.tm_sec;
				else if(local.tm_sec < 60 && local.tm_sec >= 30)
					diff_sec = 60 - local.tm_sec;
				alarm(diff_sec);
			}
			else{
				/* every hour */
				if((local.tm_min != 0) || (local.tm_sec != 0)){
					diff_sec = 3600 - (local.tm_min * 60 + local.tm_sec);
					alarm(diff_sec);
				}
				else{
					alarm(3600);
				}
			}
			pause();
		}
		else
		{
			if(debug) dbg("[bwdpi monitor] ntp not sync, can't record bwdpi log\n");
			exit(0);
		}
	}

	return 0;
}


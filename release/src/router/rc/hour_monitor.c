/*
	hour_monitor.c for do something at hours
*/

#include <rc.h>

// define function bit, you can define more functions as below
#define TRAFFIC_CONTROL		0x01
#define TRAFFIC_ANALYZER	0x02
#define NETWORKMAP		0x04

static int sig_cur = -1;
static int debug = 0;
static int value = 0;
static int is_first = 1;

void hm_traffic_analyzer_save()
{
	char *db_type;
	char buf[128]; // path buff = 128
	int db_mode;

	if(nvram_get_int("hour_monitor_debug") || nvram_get_int("sqlite_debug"))
		debug = 1;
	else
		debug = 0;

	if(!f_exists("/dev/detector") || !f_exists("/dev/idpfw")){
		_dprintf("%s : dpi engine doesn't exist, not to save any database\n", __FUNCTION__);
		logmessage("hour monitor", "dpi engine doesn't exist");
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
		_dprintf("traffic statistics not support cloud server yet!!\n");
	}
	else
	{
		_dprintf("Not such database type!!\n");
		return;
	}

	if(debug) {
		logmessage("hour monitor", "buf=%s", buf);
		dbg("%s : db_mode = %d, buf = %s\n", __FUNCTION__, db_mode, buf);
	}

	system(buf);
}

void hm_traffic_control_save()
{
	if(nvram_get_int("hour_monitor_debug") || nvram_get_int("traffic_control_debug"))
		debug = 1;
	else
		debug = 0;
	
	if(debug) dbg("%s : traffic_control is saving ... \n", __FUNCTION__);

	eval("traffic_control", "-w");
}

static void hm_networkmap_rescan()
{
	nvram_set("client_info_tmp", "");
	nvram_set("refresh_networkmap", "1");
	eval("killall", "-SIGUSR1", "networkmap");
}

int hour_monitor_function_check()
{
	debug = nvram_get_int("hour_monitor_debug");

	// intial global variable
	value = 0;

	// traffic control
	if(nvram_get_int("traffic_control_enable"))
		value |= TRAFFIC_CONTROL;

	// traffic analyzer
	if(nvram_get_int("bwdpi_db_enable"))
		value |= TRAFFIC_ANALYZER;

	// networkmap
	if(pidof("networkmap") != -1)
		value |= NETWORKMAP;
	
	if(debug)
	{
		dbg("%s : value = %d(0x%x)\n", __FUNCTION__, value, value);
		logmessage("hour monitor", "value = %d(0x%x)", value, value);
	}

	return value;
}

static void hour_monitor_call_fucntion()
{
	// check function enable or not
	if(!hour_monitor_function_check()) exit(0);

	if((value & TRAFFIC_CONTROL) != 0)
		hm_traffic_control_save();

	if((value & TRAFFIC_ANALYZER) != 0)
		hm_traffic_analyzer_save();

	if((value & NETWORKMAP) != 0)
		hm_networkmap_rescan();
}

static void hour_monitor_save_database()
{
	/* use for save database only */

	// check function enable or not
	if(!hour_monitor_function_check()) exit(0);

	if((value & TRAFFIC_CONTROL) != 0)
		hm_traffic_control_save();

	if((value & TRAFFIC_ANALYZER) != 0)
		hm_traffic_analyzer_save();
}

static void catch_sig(int sig)
{
	sig_cur = sig;

	if (sig == SIGALRM)
	{
		hour_monitor_call_fucntion();
	}
	else if (sig == SIGTERM)
	{
		hour_monitor_save_database(); /* use for save database only */
		remove("/var/run/hour_monitor.pid");
		exit(0);
	}
}

int hour_monitor_main(int argc, char **argv)
{
	FILE *fp;
	sigset_t sigs_to_catch;

	debug = nvram_get_int("hour_monitor_debug");

	/* starting message */
	if(debug) _dprintf("%s: daemong is starting ... \n", __FUNCTION__);
	logmessage("hour monitor", "daemon is starting");

	/* check need to enable monitor fucntion or not */
	if(!hour_monitor_function_check()){
		if(debug) _dprintf("%s: terminate ... \n", __FUNCTION__);
		logmessage("hour monitor", "daemon terminates");
		exit(0);
	}

	/* write pid */
	if ((fp = fopen("/var/run/hour_monitor.pid", "w")) != NULL)
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

	if(debug) {
		_dprintf("%s: ntp_ready=%d, DST=%s\n", __FUNCTION__, nvram_get_int("ntp_ready"), nvram_safe_get("time_zone_x"));
		logmessage("hour monitor", "ntp_ready=%d, DST=%s", nvram_get_int("ntp_ready"), nvram_safe_get("time_zone_x"));
	}

	while(1)
	{
		if (nvram_get_int("ntp_ready"))
		{
			struct tm local;
			time_t now;
			int diff_sec = 0;

			time(&now);
			localtime_r(&now, &local);
			if(debug) dbg("%s: %d-%d-%d, %d:%d:%d\n", __FUNCTION__,
				local.tm_year+1900, local.tm_mon+1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
			if(debug) logmessage("hour_monitor", "%d-%d-%d, %d:%d:%d\n", 
				local.tm_year+1900, local.tm_mon+1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

			/* every hour */
			if((local.tm_min != 0) || (local.tm_sec != 0)){
				diff_sec = 3600 - (local.tm_min * 60 + local.tm_sec);

				// delay 15 sec to wait for DST NTP sync done
				if (strstr(nvram_safe_get("time_zone_x"), "DST")) diff_sec += 15;
			}
			else{
				diff_sec = 3600;
			}

			if(is_first){
				diff_sec = 120;
				is_first = 0;
			}
			
			if(debug) {
				_dprintf("%s: diff_sec=%d\n", __FUNCTION__, diff_sec);
				logmessage("hour monitor", "diff_sec=%d", diff_sec);
			}

			alarm(diff_sec);
			pause();
		}
		else
		{
			if(debug) _dprintf("%s: ntp is not syn ... \n", __FUNCTION__);
			logmessage("hour monitor", "ntp is not syn");
			exit(0);
		}
	}

	return 0;
}

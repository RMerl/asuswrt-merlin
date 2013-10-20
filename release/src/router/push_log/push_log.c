#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <shared.h>
#include <shutils.h>
#include <push_log.h>


int trans_mon(char *target){
	char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
	int i;

	for(i = 0; months[i] != NULL; ++i){
		if(!strcmp(target, months[i]))
			return i;
	}

	return 0;
}

time_t trans_timenum(char *target){
	time_t now, after;
	struct tm *ptr;
	char sec[4], min[4], hour[4], interval[10], mday[4], mon[4];
	char word[PATH_MAX], *next_word;
	int shift;

	shift = 0;
	foreach(word, target, next_word){
		if(shift == 0)
			strcpy(mon, word);
		else if(shift == 1)
			strcpy(mday, word);
		else if(shift == 2){
			strcpy(interval, word);
			break;
		}

		++shift;
	}

	shift = 0;
	foreach_58(word, interval, next_word){
		if(shift == 0)
			strcpy(hour, word);
		else if(shift == 1)
			strcpy(min, word);
		else if(shift == 2){
			strcpy(sec, word);
			break;
		}

		++shift;
	}

	now = time((time_t*)0);
	ptr = localtime(&now);
	ptr->tm_sec = atoi(sec);
	ptr->tm_min = atoi(min);
	ptr->tm_hour = atoi(hour);
	ptr->tm_mday = atoi(mday);
	ptr->tm_mon = trans_mon(mon);

	after = mktime(ptr);

	return after;
}

int getlogbyinterval(const char *target_file, int interval){
	FILE *fp, *notify;
	time_t now, stamp = 0;
	char buf[PATH_MAX], time_buf[logtime_len+1], *des;
	char header[PATH_MAX], *next_word;
	int print_log, print_httplogin, print_diskmonitor;

	if((fp = fopen(SYSLOG_FILE, "r")) == NULL){
		printf("Can't open the syslog!\n");
		return -1;
	}

	if((notify = fopen(target_file, "w")) == NULL){
		printf("Can't open the notifylog!\n");
		return -1;
	}

	print_httplogin = nvram_get_int("pushnotify_httplogin");
	print_diskmonitor = nvram_get_int("pushnotify_diskmonitor");

	now = time((time_t*)0);

	memset(time_buf, 0, logtime_len+1);
	while(fgets(buf, PATH_MAX, fp)){
		des = buf+logtime_len+1;

		if(strncmp(buf, time_buf, logtime_len)){
			strncpy(time_buf, buf, logtime_len);
			stamp = trans_timenum(time_buf);
		}

		if(interval != 0 && now-stamp > interval)
			continue;

		foreach_58(header, des, next_word){
			break;
		}

		print_log = 0;
		if(print_httplogin && !strcmp(header, HEAD_HTTP_LOGIN))
			print_log = 1;
		if(print_diskmonitor && !strcmp(header, HEAD_DISK_MONITOR))
			print_log = 1;

		if(print_log){
			//printf("stamp=%lu, %s, %s", stamp, header, des);
			fputs(buf, notify);
		}
	}

	fclose(fp);
	fclose(notify);

	return 0;	
}

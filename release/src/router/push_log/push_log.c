#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef APP_IPKG
#include <shutils.h>
#include <bcmnvram.h>
#endif

#include <ws_api.h>
#include <push_log.h>

#ifdef APP_IPKG

#include <unistd.h>
#include <syslog.h>
#include <string.h>
/* Copy each token in wordlist delimited by space into word */
#define foreach(word, wordlist, next) \
	for (next = &wordlist[strspn(wordlist, " ")], \
	     strncpy(word, next, sizeof(word)), \
	     word[strcspn(word, " ")] = '\0', \
	     word[sizeof(word) - 1] = '\0', \
	     next = strchr(next, ' '); \
	     strlen(word); \
	     next = next ? &next[strspn(next, " ")] : "", \
	     strncpy(word, next, sizeof(word)), \
	     word[strcspn(word, " ")] = '\0', \
	     word[sizeof(word) - 1] = '\0', \
	     next = strchr(next, ' '))

/* Copy each token in wordlist delimited by ascii_58 into word */
#define foreach_58(word, wordlist, next) \
		for (next = &wordlist[strspn(wordlist, ":")], \
				strncpy(word, next, sizeof(word)), \
				word[strcspn(word, ":")] = '\0', \
				word[sizeof(word) - 1] = '\0', \
				next = strchr(next, ':'); \
				strlen(word); \
				next = next ? &next[strspn(next, ":")] : "", \
				strncpy(word, next, sizeof(word)), \
				word[strcspn(word, ":")] = '\0', \
				word[sizeof(word) - 1] = '\0', \
				next = strchr(next, ':'))

char * nvram_get(const char *name)
{

        char tmp_name[256]="/opt/etc/asus_script/aicloud_nvram_check.sh";
        char *cmd_name;
        cmd_name=(char *)malloc(sizeof(char)*(strlen(tmp_name)+strlen(name)+2));
        memset(cmd_name,0,sizeof(cmd_name));
        sprintf(cmd_name,"%s %s",tmp_name,name);
        system(cmd_name);
        free(cmd_name);

        while(-1!=access("/tmp/aicloud_check.control",F_OK))
            usleep(50);


    FILE *fp;
    if((fp=fopen("/tmp/webDAV.conf","r+"))==NULL)
    {
        return NULL;
    }
    char *value = NULL;

    char tmp[256]={0};
    while(!feof(fp)){
        memset(tmp,'\0',sizeof(tmp));
        fgets(tmp,sizeof(tmp),fp);
        if(strncmp(tmp,name,strlen(name))==0)
        {
            if(tmp[strlen(tmp)-1] == 10)
            {
                tmp[strlen(tmp)-1]='\0';
            }
            char *p=NULL;
            p=strchr(tmp,'=');
            p++;
            if(p == NULL || strlen(p) == 0)
            {
                fclose(fp);
                return NULL;
            }
            else
            {
            value=(char *)malloc(strlen(p)+1);
            memset(value,'\0',sizeof(value));
            strcpy(value,p);
            if(value[strlen(value)-1]=='\n')
                value[strlen(value)-1]='\0';
        }

    }
    fclose(fp);
    return value;
}
}

char * nvram_safe_get(const char *name)
{
	char *p = nvram_get(name);
	return p ? p : "";
}

int nvram_get_int(const char *key)
{
	return atoi(nvram_safe_get(key));
}

int nvram_set(const char *name, const char *value)
{
    char *cmd;

    if(value == NULL)
        cmd=(char *)malloc(sizeof(char)*(64+strlen(name)));
    else
        cmd=(char *)malloc(sizeof(char)*(64+strlen(name)+strlen(value)));

    memset(cmd,0,sizeof(cmd));

    sprintf(cmd,"nvram set \"%s=%s\"",name,value);

    system(cmd);

    free(cmd);
    return 0;
}

int nvram_set_int(const char *key, int value)
{
	char nvramstr[16];

	snprintf(nvramstr, sizeof(nvramstr), "%d", value);
	return nvram_set(key, nvramstr);
}

#endif

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

int if_push_log(const char *header){
	int print_httplogin, print_diskmonitor;

	print_httplogin = nvram_get_int("pushnotify_httplogin");
	print_diskmonitor = nvram_get_int("pushnotify_diskmonitor");

	if(print_httplogin && !strcmp(header, HEAD_HTTP_LOGIN))
		return 1;

	if(print_diskmonitor && !strcmp(header, HEAD_DISK_MONITOR))
		return 1;

	return 0;
}

// type: 0, push mail; 1, push APP.
int getlogbyinterval(const char *target_file, int interval, int type){
	FILE *fp, *notify;
	time_t now, stamp = 0;
	char buf[PATH_MAX], time_buf[LOGTIME_STRLEN+1], *des;
	char header[PATH_MAX], *next_word;
	int got_log, push_log;

	if(target_file == NULL){
		printf("Wrong Input!\n");
		return -1;
	}

	if((fp = fopen(SYSLOG_FILE, "r")) == NULL){
		printf("Can't open the syslog!\n");
		return -1;
	}

	if((notify = fopen(target_file, "w")) == NULL){
		printf("Can't open the notifylog!\n");
		return -1;
	}

	now = time((time_t*)0);

	got_log = 0;
	memset(time_buf, 0, LOGTIME_STRLEN+1);
	while(fgets(buf, PATH_MAX, fp)){
		des = buf+LOGTIME_STRLEN+1;

		if(!got_log){
			if(strncmp(buf, time_buf, LOGTIME_STRLEN)){
				strncpy(time_buf, buf, LOGTIME_STRLEN);
				stamp = trans_timenum(time_buf);
			}

			if(interval != 0 && now-stamp > interval)
				continue;

			got_log = 1;
		}

		foreach_58(header, des, next_word){
			break;
		}

		push_log = if_push_log(header);
		if(push_log){
			fputs(buf, notify);
		}
	}

	fclose(fp);
	fclose(notify);

	if(type == PUSHTYPE_APP){
		nvram_set("pushnotify_lognum", "1");
	}

	return 0;	
}

// type: 0, push mail; 1, push APP.
int getlogbytimestr(const char *target_file, const char *time_str, int type){
	FILE *fp, *notify;
	time_t time_stamp, log_stamp = 0;
	char buf[PATH_MAX], time_buf[LOGTIME_STRLEN+1], *des;
	char header[PATH_MAX], *next_word;
	int got_log, push_log;

	if(target_file == NULL || time_str == NULL){
		printf("Wrong Input!\n");
		return -1;
	}

	if((fp = fopen(SYSLOG_FILE, "r")) == NULL){
		printf("Can't open the syslog!\n");
		return -1;
	}

	if((notify = fopen(target_file, "w")) == NULL){
		printf("Can't open the notifylog!\n");
		return -1;
	}

	time_stamp = trans_timenum(time_str);

	got_log = 0;
	memset(time_buf, 0, LOGTIME_STRLEN+1);
	while(fgets(buf, PATH_MAX, fp)){
		des = buf+LOGTIME_STRLEN+1;

		if(!got_log){
			if(strncmp(buf, time_buf, LOGTIME_STRLEN)){
				strncpy(time_buf, buf, LOGTIME_STRLEN);
				log_stamp = trans_timenum(time_buf);
			}

			if(log_stamp < time_stamp)
				continue;

			got_log = 1;
		}

		foreach_58(header, des, next_word){
			break;
		}

		push_log = if_push_log(header);
		if(push_log){
			fputs(buf, notify);
		}
	}

	fclose(fp);
	fclose(notify);

	if(type == PUSHTYPE_APP){
		nvram_set("pushnotify_lognum", "1");
	}

	return 0;	
}

void logmessage_push(char *logheader, char *fmt, ...){
  va_list args;
  char buf[512];

  va_start(args, fmt);

  vsnprintf(buf, sizeof(buf), fmt, args);

  openlog(logheader, 0, 0);
  syslog(0, buf);
  closelog();
  va_end(args);

	char buf2[1024], mac[18];
	Push_Msg psm;
	int push_log, ret;
  int count;

	memset(mac, 0, 18);
	strncpy(mac, nvram_safe_get("et0macaddr"), 18);

	push_log = if_push_log(logheader);
	if(push_log){
		count = nvram_get_int("pushnotify_lognum");
		if(count <= 0)
			count = 1;

		memset(buf2, 0, 1024);
		sprintf(buf2, "%s: %d: %s: %s", mac, count, logheader, buf);

		++count;
		nvram_set_int("pushnotify_lognum", count);

		ret = send_push_msg_req(SERVER, mac, "", buf2, &psm);
	}
}

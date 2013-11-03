#include <time.h>
#include <stdarg.h>

#define SYSLOG_FILE "/tmp/syslog.log"
#define LOGTIME_STRLEN 15

#define HEAD_HTTP_LOGIN "HTTP login"
#define HEAD_DISK_MONITOR "disk monitor" // test.

enum {
	PUSHTYPE_MAIL = 0,
	PUSHTYPE_APP,
	PUSHTYPE_MAX
};

#ifdef RTCONFIG_EMAIL
extern int trans_mon(char *target);
extern time_t trans_timenum(char *target);
extern int if_push_log(const char *header);
extern int getlogbyinterval(const char *target_file, int interval, int type);
extern int getlogbytimestr(const char *target_file, const char *time_str, int type);
extern void logmessage_push(char *logheader, char *fmt, ...);
#endif

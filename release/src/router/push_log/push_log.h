#include <time.h>

#define SYSLOG_FILE "/tmp/syslog.log"
#define logtime_len 15

#define HEAD_HTTP_LOGIN "HTTP login"
#define HEAD_DISK_MONITOR "disk monitor" // test.


extern int trans_mon(char *target);
extern time_t trans_timenum(char *target);
int getlogbyinterval(const char *target_file, int interval);

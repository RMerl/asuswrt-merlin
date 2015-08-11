
#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <sys/timeb.h>

#include "log.h"

#define BUFSIZE		4096

#ifdef _WIN32
#define snprintf	_snprintf
#endif

FILE *logFile = NULL;
int logLevel = 20;

const char *logTime()
{
	static char buf[128];
	char *p = 0;
	time_t lTime;
	struct tm *tm1;

	time(&lTime);	
	tm1 = localtime(&lTime);	
	strcpy(buf, asctime(tm1));
	p = strchr(buf, '\n');

	if (p) *p = '\0';
    return buf;
}

void log_open(const char *file, int level)
{
	logLevel = level;

	if (logFile)
		log_close();

	if (file && *file)
		logFile = fopen(file, "a+");
	if (!logFile) 
	{
		logFile = stderr;
		if (file && *file)
			LOG(1) ("Error open log file: %s\n", file);
	}
	//for test only!!!
	//logFile = stderr;
}

void log_close()
{
	if (logFile && logFile != stderr) {
		fclose(logFile);
		logFile = NULL;
	}
}

void log_print(const char *fmt, ...)
{
	char buf[BUFSIZE];
	
	va_list args;
	va_start(args, fmt);

	snprintf(buf, sizeof(buf), "%s| %s", logTime(), fmt);
  if(logFile) {
    vfprintf(logFile, buf, args);
    fflush(logFile);
  }

	va_end(args);
}

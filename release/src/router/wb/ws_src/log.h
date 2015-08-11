#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
// Enable the debugging for the category.
#define WB_DBG	1
//#define NDEBUG
//void dprintf_impl(const char* date,const char* time,const char* file,const char* func, size_t line, int enable, const char* fmt, ...);

// steam type could be 
// 	0 	for syslog
//	1.	for stderr
//	2	for file
#define SYSLOG_TYPE 	0
#define STD_ERR		1
#define FILE_TYPE	2


void dprintf_impl(const char* file,const char* func, size_t line, int enable, const char* fmt, ...);
int open_log(const char* log_path, int stream_type);
//void closefp(FILE* fp);
void close_log();
extern FILE* gfp;
//FILE* gfp =NULL;
#define WHERESTR "[%32s][%25s] <<%38s>>, line %i: "
#define WHEREARG  __FILE__,__func__,__LINE__

#ifndef NDEBUG
//#define WHERESTR "[%s][%s][%s] <<%s>>, line %i: "
//#define WHEREARG  __DATE__,__TIME__,__FILE__,__func__,__LINE__
//#define Cdbg(fmt, ...)  fprintf(stderr,WHERESTR  fmt "\n", WHEREARG	, __VA_ARGS__)

//#define Cdbg(enable, ...) dprintf_impl(WHEREARG, enable, 0,  __VA_ARGS__)
#define Cdbg(enable, ...) dprintf_impl(WHEREARG, enable, __VA_ARGS__)
#else
#define Cdbg(enable, ...) // define to nothing in release mode
#endif

//#define CF(enable, path, ...) dprintf_impl(WHEREARG, enable, 1,  __VA_ARGS__)
#define CF_OPEN(path, type)				open_log(path, type);
#define CF_CLOSE()				close_log();	
#endif


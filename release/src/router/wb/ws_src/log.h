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
void dprintf_impl(const char* file,const char* func, size_t line, int enable, const char* fmt, ...);

#ifndef NDEBUG
//#define WHERESTR "[%s][%s][%s] <<%s>>, line %i: "
#define WHERESTR "[%25s] <<%38s>>, line %i: "
//#define WHEREARG  __DATE__,__TIME__,__FILE__,__func__,__LINE__
#define WHEREARG  __FILE__,__func__,__LINE__
//#define Cdbg(fmt, ...)  fprintf(stderr,WHERESTR  fmt "\n", WHEREARG	, __VA_ARGS__)

#define Cdbg(enable, ...) dprintf_impl(WHEREARG, enable, __VA_ARGS__)
#else
#define Cdbg(enable, ...) // define to nothing in release mode
#endif

#endif


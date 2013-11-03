#ifndef NDEBUG
#include <log.h>
//void dprintf_impl(const char* date,const char* time,const char* file,const char* func, size_t line, int enable, const char* fmt, ...)
void dprintf_impl(const char* file,const char* func, size_t line, int enable, const char* fmt, ...)
{
    va_list ap;
    if (enable) {
//	   fprintf(stderr, WHERESTR,date, time, file, func, line);
		fprintf(stderr, WHERESTR, file, func, line);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
}
#endif


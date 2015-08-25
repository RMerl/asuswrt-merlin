/**
 * wrapper for LogMsg to use built-in error logging
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "err.h"

void LogMsg(const char *format, ...) {
    unsigned char buffer[512];
    va_list ptr;
    va_start(ptr,format);
    memset(buffer,0,sizeof(buffer));
    vsnprintf((char*)buffer, sizeof(buffer), format, ptr);
    va_end(ptr);
    DPRINTF(E_WARN,L_REND,"%s\n",buffer);
}

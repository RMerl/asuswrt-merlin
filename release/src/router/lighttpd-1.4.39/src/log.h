#ifndef _LOG_H_
#define _LOG_H_

#include "server.h"

ssize_t write_all(int fd, const void* buf, size_t count);

/* Close fd and _try_ to get a /dev/null for it instead.
 * Returns 0 on success and -1 on failure (fd gets closed in all cases)
 */
int openDevNull(int fd);

int open_logfile_or_pipe(server *srv, const char* logfile);

int log_error_open(server *srv);
int log_error_close(server *srv);
int log_error_write(server *srv, const char *filename, unsigned int line, const char *fmt, ...);
int log_error_write_multiline_buffer(server *srv, const char *filename, unsigned int line, buffer *multiline, const char *fmt, ...);
int log_error_cycle(server *srv);

//- Sungmin add
int log_sys_open(server *srv);
int log_sys_close(server *srv);
int log_sys_write(server *srv, const char *fmt, ...);

//void dprintf_impl(const char* date,const char* time,const char* file,const char* func, size_t line, int enable, const char* fmt, ...);
void dprintf_impl(const char* file,const char* func, size_t line, int enable, const char* fmt, ...);
#define NDEBUG
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

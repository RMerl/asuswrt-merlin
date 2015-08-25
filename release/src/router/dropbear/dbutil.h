/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#ifndef DROPBEAR_DBUTIL_H_

#define DROPBEAR_DBUTIL_H_

#include "includes.h"
#include "buffer.h"
#include "queue.h"

#ifndef DISABLE_SYSLOG
void startsyslog();
#endif

#ifdef __GNUC__
#define ATTRIB_PRINTF(fmt,args) __attribute__((format(printf, fmt, args))) 
#define ATTRIB_NORETURN __attribute__((noreturn))
#define ATTRIB_SENTINEL __attribute__((sentinel))
#else
#define ATTRIB_PRINTF(fmt,args)
#define ATTRIB_NORETURN
#define ATTRIB_SENTINEL
#endif

extern void (*_dropbear_exit)(int exitcode, const char* format, va_list param) ATTRIB_NORETURN;
extern void (*_dropbear_log)(int priority, const char* format, va_list param);

void dropbear_exit(const char* format, ...) ATTRIB_PRINTF(1,2) ATTRIB_NORETURN;

void dropbear_close(const char* format, ...) ATTRIB_PRINTF(1,2) ;
void dropbear_log(int priority, const char* format, ...) ATTRIB_PRINTF(2,3) ;

void fail_assert(const char* expr, const char* file, int line) ATTRIB_NORETURN;

#ifdef DEBUG_TRACE
void dropbear_trace(const char* format, ...) ATTRIB_PRINTF(1,2);
void dropbear_trace2(const char* format, ...) ATTRIB_PRINTF(1,2);
void printhex(const char * label, const unsigned char * buf, int len);
void printmpint(const char *label, mp_int *mp);
void debug_start_net();
extern int debug_trace;
#endif

char * stripcontrol(const char * text);

int spawn_command(void(*exec_fn)(void *user_data), void *exec_data,
		int *writefd, int *readfd, int *errfd, pid_t *pid);
void run_shell_command(const char* cmd, unsigned int maxfd, char* usershell);
#ifdef ENABLE_CONNECT_UNIX
int connect_unix(const char* addr);
#endif
int buf_readfile(buffer* buf, const char* filename);
int buf_getline(buffer * line, FILE * authfile);

void m_close(int fd);
void * m_malloc(size_t size);
void * m_strdup(const char * str);
void * m_realloc(void* ptr, size_t size);
#define m_free(X) do {free(X); (X) = NULL;} while (0)
void m_burn(void* data, unsigned int len);
void setnonblocking(int fd);
void disallow_core();
int m_str_to_uint(const char* str, unsigned int *val);

/* Used to force mp_ints to be initialised */
#define DEF_MP_INT(X) mp_int X = {0, 0, 0, NULL}

/* Dropbear assertion */
#define dropbear_assert(X) do { if (!(X)) { fail_assert(#X, __FILE__, __LINE__); } } while (0)

/* Returns 0 if a and b have the same contents */
int constant_time_memcmp(const void* a, const void *b, size_t n);

/* Returns a time in seconds that doesn't go backwards - does not correspond to
a real-world clock */
time_t monotonic_now();

char * expand_homedir_path(const char *inpath);

#endif /* DROPBEAR_DBUTIL_H_ */

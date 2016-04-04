#include "base.h"
#include "log.h"
#include "array.h"

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <stdarg.h>
#include <stdio.h>

#ifdef HAVE_SYSLOG_H
# include <syslog.h>
#endif

#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

/* retry write on EINTR or when not all data was written */
ssize_t write_all(int fd, const void* buf, size_t count) {
	ssize_t written = 0;

	while (count > 0) {
		ssize_t r = write(fd, buf, count);
		if (r < 0) {
			switch (errno) {
			case EINTR:
				/* try again */
				break;
			default:
				/* fail - repeating probably won't help */
				return -1;
			}
		} else if (0 == r) {
			/* really shouldn't happen... */
			errno = EIO;
			return -1;
		} else {
			force_assert(r <= (ssize_t) count);
			written += r;
			buf = r + (char const*) buf;
			count -= r;
		}
	}

	return written;
}

/* Close fd and _try_ to get a /dev/null for it instead.
 * close() alone may trigger some bugs when a
 * process opens another file and gets fd = STDOUT_FILENO or STDERR_FILENO
 * and later tries to just print on stdout/stderr
 *
 * Returns 0 on success and -1 on failure (fd gets closed in all cases)
 */
int openDevNull(int fd) {
	int tmpfd;
	close(fd);
#if defined(__WIN32)
	/* Cygwin should work with /dev/null */
	tmpfd = open("nul", O_RDWR);
#else
	tmpfd = open("/dev/null", O_RDWR);
#endif
	if (tmpfd != -1 && tmpfd != fd) {
		dup2(tmpfd, fd);
		close(tmpfd);
	}
	return (tmpfd != -1) ? 0 : -1;
}

int open_logfile_or_pipe(server *srv, const char* logfile) {
	int fd;

	if (logfile[0] == '|') {
#ifdef HAVE_FORK
		/* create write pipe and spawn process */

		int to_log_fds[2];

		if (pipe(to_log_fds)) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "pipe failed: ", strerror(errno));
			return -1;
		}

		/* fork, execve */
		switch (fork()) {
		case 0:
			/* child */
			close(STDIN_FILENO);

			/* dup the filehandle to STDIN */
			if (to_log_fds[0] != STDIN_FILENO) {
				if (STDIN_FILENO != dup2(to_log_fds[0], STDIN_FILENO)) {
					log_error_write(srv, __FILE__, __LINE__, "ss",
						"dup2 failed: ", strerror(errno));
					exit(-1);
				}
				close(to_log_fds[0]);
			}
			close(to_log_fds[1]);

#ifndef FD_CLOEXEC
			{
				int i;
				/* we don't need the client socket */
				for (i = 3; i < 256; i++) {
					close(i);
				}
			}
#endif

			/* close old stderr */
			openDevNull(STDERR_FILENO);

			/* exec the log-process (skip the | ) */
			execl("/bin/sh", "sh", "-c", logfile + 1, NULL);
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"spawning log process failed: ", strerror(errno),
					logfile + 1);

			exit(-1);
			break;
		case -1:
			/* error */
			log_error_write(srv, __FILE__, __LINE__, "ss", "fork failed: ", strerror(errno));
			return -1;
		default:
			close(to_log_fds[0]);
			fd = to_log_fds[1];
			break;
		}

#else
		return -1;
#endif
	} else if (-1 == (fd = open(logfile, O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644))) {
		log_error_write(srv, __FILE__, __LINE__, "SSSS",
				"opening errorlog '", logfile,
				"' failed: ", strerror(errno));

		return -1;
	}

	fd_close_on_exec(fd);

	return fd;
}


/**
 * open the errorlog
 *
 * we have 4 possibilities:
 * - stderr (default)
 * - syslog
 * - logfile
 * - pipe
 *
 * if the open failed, report to the user and die
 *
 */

int log_error_open(server *srv) {
#ifdef HAVE_SYSLOG_H
	/* perhaps someone wants to use syslog() */
	openlog("lighttpd", LOG_CONS | LOG_PID, LOG_DAEMON);
#endif

	srv->errorlog_mode = ERRORLOG_FD;
	srv->errorlog_fd = STDERR_FILENO;

	if (srv->srvconf.errorlog_use_syslog) {
		srv->errorlog_mode = ERRORLOG_SYSLOG;
	} else if (!buffer_string_is_empty(srv->srvconf.errorlog_file)) {
		const char *logfile = srv->srvconf.errorlog_file->ptr;

		if (-1 == (srv->errorlog_fd = open_logfile_or_pipe(srv, logfile))) {
			return -1;
		}
		srv->errorlog_mode = (logfile[0] == '|') ? ERRORLOG_PIPE : ERRORLOG_FILE;
	}

	log_error_write(srv, __FILE__, __LINE__, "s", "server started");

	if (srv->errorlog_mode == ERRORLOG_FD && !srv->srvconf.dont_daemonize) {
		/* We can only log to stderr in dont-daemonize mode;
		 * if we do daemonize and no errorlog file is specified, we log into /dev/null
		 */
		srv->errorlog_fd = -1;
	}

	if (!buffer_string_is_empty(srv->srvconf.breakagelog_file)) {
		int breakage_fd;
		const char *logfile = srv->srvconf.breakagelog_file->ptr;

		if (srv->errorlog_mode == ERRORLOG_FD) {
			srv->errorlog_fd = dup(STDERR_FILENO);
			fd_close_on_exec(srv->errorlog_fd);
		}

		if (-1 == (breakage_fd = open_logfile_or_pipe(srv, logfile))) {
			return -1;
		}

		if (STDERR_FILENO != breakage_fd) {
			dup2(breakage_fd, STDERR_FILENO);
			close(breakage_fd);
		}
	} else if (!srv->srvconf.dont_daemonize) {
		/* move stderr to /dev/null */
		openDevNull(STDERR_FILENO);
	}
	return 0;
}

/**
 * open the errorlog
 *
 * if the open failed, report to the user and die
 * if no filename is given, use syslog instead
 *
 */

int log_error_cycle(server *srv) {
	/* only cycle if the error log is a file */

	if (srv->errorlog_mode == ERRORLOG_FILE) {
		const char *logfile = srv->srvconf.errorlog_file->ptr;
		/* already check of opening time */

		int new_fd;

		if (-1 == (new_fd = open_logfile_or_pipe(srv, logfile))) {
			/* write to old log */
			log_error_write(srv, __FILE__, __LINE__, "SSSSS",
					"cycling errorlog '", logfile,
					"' failed: ", strerror(errno),
					", falling back to syslog()");

			close(srv->errorlog_fd);
			srv->errorlog_fd = -1;
#ifdef HAVE_SYSLOG_H
			srv->errorlog_mode = ERRORLOG_SYSLOG;
#endif
		} else {
			/* ok, new log is open, close the old one */
			close(srv->errorlog_fd);
			srv->errorlog_fd = new_fd;
			fd_close_on_exec(srv->errorlog_fd);
		}
	}

	return 0;
}

int log_error_close(server *srv) {
	switch(srv->errorlog_mode) {
	case ERRORLOG_PIPE:
	case ERRORLOG_FILE:
	case ERRORLOG_FD:
		if (-1 != srv->errorlog_fd) {
			/* don't close STDERR */
			if (STDERR_FILENO != srv->errorlog_fd)
				close(srv->errorlog_fd);
			srv->errorlog_fd = -1;
		}
		break;
	case ERRORLOG_SYSLOG:
#ifdef HAVE_SYSLOG_H
		closelog();
#endif
		break;
	}

	return 0;
}

/* lowercase: append space, uppercase: don't */
static void log_buffer_append_printf(buffer *out, const char *fmt, va_list ap) {
	for(; *fmt; fmt++) {
		int d;
		char *s;
		buffer *b;
		off_t o;

		switch(*fmt) {
		case 's':           /* string */
			s = va_arg(ap, char *);
			buffer_append_string_c_escaped(out, s, (NULL != s) ? strlen(s) : 0);
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'b':           /* buffer */
			b = va_arg(ap, buffer *);
			buffer_append_string_c_escaped(out, CONST_BUF_LEN(b));
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'd':           /* int */
			d = va_arg(ap, int);
			buffer_append_int(out, d);
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'o':           /* off_t */
			o = va_arg(ap, off_t);
			buffer_append_int(out, o);
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'x':           /* int (hex) */
			d = va_arg(ap, int);
			buffer_append_string_len(out, CONST_STR_LEN("0x"));
			buffer_append_uint_hex(out, d);
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'S':           /* string */
			s = va_arg(ap, char *);
			buffer_append_string_c_escaped(out, s, (NULL != s) ? strlen(s) : 0);
			break;
		case 'B':           /* buffer */
			b = va_arg(ap, buffer *);
			buffer_append_string_c_escaped(out, CONST_BUF_LEN(b));
			break;
		case 'D':           /* int */
			d = va_arg(ap, int);
			buffer_append_int(out, d);
			break;
		case 'O':           /* off_t */
			o = va_arg(ap, off_t);
			buffer_append_int(out, o);
			break;
		case 'X':           /* int (hex) */
			d = va_arg(ap, int);
			buffer_append_string_len(out, CONST_STR_LEN("0x"));
			buffer_append_uint_hex(out, d);
			break;
		case '(':
		case ')':
		case '<':
		case '>':
		case ',':
		case ' ':
			buffer_append_string_len(out, fmt, 1);
			break;
		}
	}
}

static int log_buffer_prepare(buffer *b, server *srv, const char *filename, unsigned int line) {
	switch(srv->errorlog_mode) {
	case ERRORLOG_PIPE:
	case ERRORLOG_FILE:
	case ERRORLOG_FD:
		if (-1 == srv->errorlog_fd) return -1;
		/* cache the generated timestamp */
		if (srv->cur_ts != srv->last_generated_debug_ts) {
			buffer_string_prepare_copy(srv->ts_debug_str, 255);
			buffer_append_strftime(srv->ts_debug_str, "%Y-%m-%d %H:%M:%S", localtime(&(srv->cur_ts)));

			srv->last_generated_debug_ts = srv->cur_ts;
		}

		buffer_copy_buffer(b, srv->ts_debug_str);
		buffer_append_string_len(b, CONST_STR_LEN(": ("));
		break;
	case ERRORLOG_SYSLOG:
		/* syslog is generating its own timestamps */
		buffer_copy_string_len(b, CONST_STR_LEN("("));
		break;
	}

	buffer_append_string(b, filename);
	buffer_append_string_len(b, CONST_STR_LEN("."));
	buffer_append_int(b, line);
	buffer_append_string_len(b, CONST_STR_LEN(") "));

	return 0;
}

static void log_write(server *srv, buffer *b) {
	switch(srv->errorlog_mode) {
	case ERRORLOG_PIPE:
	case ERRORLOG_FILE:
	case ERRORLOG_FD:
		buffer_append_string_len(b, CONST_STR_LEN("\n"));
		write_all(srv->errorlog_fd, CONST_BUF_LEN(b));
		break;
	case ERRORLOG_SYSLOG:
		syslog(LOG_ERR, "%s", b->ptr);
		break;
	}
}

int log_error_write(server *srv, const char *filename, unsigned int line, const char *fmt, ...) {
	va_list ap;

	if (-1 == log_buffer_prepare(srv->errorlog_buf, srv, filename, line)) return 0;

	va_start(ap, fmt);
	log_buffer_append_printf(srv->errorlog_buf, fmt, ap);
	va_end(ap);

	log_write(srv, srv->errorlog_buf);

	return 0;
}

int log_error_write_multiline_buffer(server *srv, const char *filename, unsigned int line, buffer *multiline, const char *fmt, ...) {
	va_list ap;
	size_t prefix_len;
	buffer *b = srv->errorlog_buf;
	char *pos, *end, *current_line;

	if (buffer_string_is_empty(multiline)) return 0;

	if (-1 == log_buffer_prepare(b, srv, filename, line)) return 0;

	va_start(ap, fmt);
	log_buffer_append_printf(b, fmt, ap);
	va_end(ap);

	prefix_len = buffer_string_length(b);

	current_line = pos = multiline->ptr;
	end = multiline->ptr + buffer_string_length(multiline);

	for ( ; pos <= end ; ++pos) {
		switch (*pos) {
		case '\n':
		case '\r':
		case '\0': /* handles end of string */
			if (current_line < pos) {
				/* truncate to prefix */
				buffer_string_set_length(b, prefix_len);

				buffer_append_string_len(b, current_line, pos - current_line);
				log_write(srv, b);
			}
			current_line = pos + 1;
			break;
		default:
			break;
		}
	}

	return 0;
}

//- Sungmin add
/**
 * open the syslog
 *
 * if the open failed, report to the user and die
 *
 */
int log_sys_open(server *srv) {
	
	srv->syslog_fd = -1;

	if (!buffer_is_empty(srv->srvconf.syslog_file)) {
		const char *logfile = srv->srvconf.syslog_file->ptr;		
		if (-1 == (srv->syslog_fd = open_logfile_or_pipe(srv, logfile))) {
			return -1;
		}

		//log_sys_write(srv, "s", "Start syslog...");
	}

	return 0;
}

int log_sys_close(server *srv) {
	if (-1 != srv->syslog_fd) {
		if (STDERR_FILENO != srv->syslog_fd){
			close(srv->syslog_fd);
			srv->syslog_fd = -1;
		}
	}
	return 0;
}

int log_sys_write(server *srv, const char *fmt, ...) {
	va_list ap;
	
	if (-1 == srv->syslog_fd) return 0;

	buffer* sys_time_str = buffer_init();
	buffer_string_prepare_copy(sys_time_str, 255);

#if EMBEDDED_EANBLE
#ifndef APP_IPKG
	setenv("TZ", nvram_get_time_zone(), 1);
#else
	char *time_zone=nvram_get_time_zone();
	setenv("TZ", time_zone, 1);
	free(time_zone);
#endif
#endif
	strftime(sys_time_str->ptr, sys_time_str->size - 1, "%b  %d %H:%M:%S", localtime(&(srv->cur_ts)));

	buffer_copy_string(srv->syslog_buf, sys_time_str->ptr);	
	buffer_free(sys_time_str);

	buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN(" webdav: "));
	
	for(va_start(ap, fmt); *fmt; fmt++) {
		int d;
		char *s;
		buffer *b;
		off_t o;

		switch(*fmt) {
		case 's':           /* string */
			s = va_arg(ap, char *);
			buffer_append_string(srv->syslog_buf, s);
			buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN(" "));
			break;
		case 'b':           /* buffer */
			b = va_arg(ap, buffer *);
			buffer* tmp = buffer_init();
			buffer_reset(tmp);
			buffer_copy_buffer(tmp, b);
			buffer_urldecode_path(tmp);
			buffer_append_string_buffer(srv->syslog_buf, tmp);
			buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN(" "));
			buffer_free(tmp);			
			break;
		case 'd':           /* int */
			d = va_arg(ap, int);
			//buffer_append_long(srv->syslog_buf, d);
			buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN(" "));
			break;
		case 'o':           /* off_t */
			o = va_arg(ap, off_t);
			//buffer_append_off_t(srv->syslog_buf, o);
			buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN(" "));
			break;
		case 'x':           /* int (hex) */
			d = va_arg(ap, int);
			buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN("0x"));
			buffer_append_uint_hex(srv->syslog_buf, d);
			buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN(" "));
			break;
		case 'S':           /* string */
			s = va_arg(ap, char *);
			buffer_append_string(srv->syslog_buf, s);
			break;
		case 'B':           /* buffer */
			b = va_arg(ap, buffer *);
			buffer_append_string_buffer(srv->syslog_buf, b);
			break;
		case 'D':           /* int */
			d = va_arg(ap, int);
			//buffer_append_long(srv->syslog_buf, d);
			break;
		case 'O':           /* off_t */
			o = va_arg(ap, off_t);
			//buffer_append_off_t(srv->syslog_buf, o);
			break;
		case 'X':           /* int (hex) */
			d = va_arg(ap, int);
			buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN("0x"));
			buffer_append_uint_hex(srv->syslog_buf, d);
			break;
		case '(':
		case ')':
		case '<':
		case '>':
		case ',':
		case ' ':
			buffer_append_string_len(srv->syslog_buf, fmt, 1);
			break;
		}
	}
	va_end(ap);

	struct stat st;
	stat(srv->srvconf.syslog_file->ptr, &st);
	int file_size = st.st_size;
	if( file_size >= 1048576 ){//- 1M
		unlink(srv->srvconf.syslog_file->ptr);
		if (-1 == log_sys_open(srv)) 
			return 0;
	}
	
	buffer_append_string_len(srv->syslog_buf, CONST_STR_LEN("\n"));
	write(srv->syslog_fd, srv->syslog_buf->ptr, srv->syslog_buf->used - 1);
	
	return 0;
}

#ifndef NDEBUG
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


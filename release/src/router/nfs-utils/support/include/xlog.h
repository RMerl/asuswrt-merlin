/*
 * xlog		Logging functionality
 *
 * Copyright (C) 1995 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef XLOG_H
#define XLOG_H

#include <stdarg.h>

/* These are logged always. L_FATAL also does exit(1) */
#define L_FATAL		0x0100
#define L_ERROR		0x0200
#define L_WARNING	0x0400
#define L_NOTICE	0x0800
#define L_ALL		0xFF00

/* These are logged if enabled with xlog_[s]config */
/* NB: code does not expect ORing together D_ and L_ */
#define D_GENERAL	0x0001		/* general debug info */
#define D_CALL		0x0002
#define D_AUTH		0x0004
#define D_FAC3		0x0008
#define D_FAC4		0x0010
#define D_FAC5		0x0020
#define D_PARSE		0x0040
#define D_FAC7		0x0080
#define D_ALL		0x00FF

/* This can be used to define symbolic log names that can be passed to
 * xlog_config. */
struct xlog_debugfac {
	char		*df_name;
	int		df_fac;
};

void			xlog_open(char *progname);
void			xlog_stderr(int on);
void			xlog_syslog(int on);
void			xlog_config(int fac, int on);
void			xlog_sconfig(char *, int on);
int			xlog_enabled(int fac);
void			xlog(int fac, const char *fmt, ...);
void			xlog_warn(const char *fmt, ...);
void			xlog_err(const char *fmt, ...);
void			xlog_backend(int fac, const char *fmt, va_list args);

#endif /* XLOG_H */

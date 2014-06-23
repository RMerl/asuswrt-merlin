/*
 *
 *   Authors:
 *    Lars Fenneberg		<lf@elemental.net>	 
 *
 *   This software is Copyright 1996,1997 by the above mentioned author(s), 
 *   All Rights Reserved.
 *
 *   The license which is distributed with this software in the file COPYRIGHT
 *   applies to this software. If your distribution is missing this file, you
 *   may request it from <reubenhwk@gmail.com>.
 *
 */

#define	L_NONE		0
#define L_SYSLOG	1
#define L_STDERR	2
#define L_STDERR_SYSLOG	3
#define L_LOGFILE	4

#define LOG_TIME_FORMAT "%b %d %H:%M:%S"

int log_open(int, char *, char *, int);
void flog(int, char *, ...) __attribute__ ((format (printf, 2, 3)));
void dlog(int, int, char *, ...) __attribute__ ((format (printf, 3, 4)));
int log_close(void);
int log_reopen(void);
void set_debuglevel(int);
int get_debuglevel(void);

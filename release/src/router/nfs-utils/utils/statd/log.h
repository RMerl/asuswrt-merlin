/*
 * Copyright (C) 1995 Olaf Kirch
 * Modified by Jeffrey A. Uphoff, 1996, 1997, 1999.
 * Modified by Lon Hohberger, Oct. 2000
 *
 * NSM for Linux.
 */

/*
 * 	logging functionality
 *	260295	okir
 */

#ifndef _LOCKD_LOG_H_
#define _LOCKD_LOG_H_

#include <syslog.h>

void	log_init(void);
void	log_background(void);
void	log_enable(int facility);
int	log_enabled(int facility);
void	note(int level, char *fmt, ...);
void	die(char *fmt, ...);

/*
 * Map per-application severity to system severity. What's fatal for
 * lockd is merely an itching spot from the universe's point of view.
 */
#define N_CRIT		LOG_CRIT
#define N_FATAL		LOG_ERR
#define N_ERROR		LOG_WARNING
#define N_WARNING	LOG_NOTICE
#define N_DEBUG		LOG_DEBUG

#ifdef DEBUG
#define dprintf		note
#else
#define dprintf		if (run_mode & MODE_LOG_STDERR) note
#endif

#endif /* _LOCKD_LOG_H_ */

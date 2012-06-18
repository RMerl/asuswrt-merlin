/*
 * our_syslog.h
 *
 * Syslog replacement functions
 *
 * $Id: our_syslog.h,v 1.1.1.1 2002/06/21 08:52:00 fenix_nl Exp $
 */

#ifndef _PPTPD_SYSLOG_H
#define _PPTPD_SYSLOG_H

/*
 *	only enable this if you are debugging and running by hand
 *	If init runs us you may not have an fd-2,  and thus your write all over
 *	someones FD and the die :-(
 */
#undef USE_STDERR

#ifdef USE_STDERR

/*
 *	Send all errors to stderr
 */

#define openlog(a,b,c) ({})
#define syslog(a,b,c...) ({fprintf(stderr, "pptpd syslog: " b "\n" , ## c);})
#define closelog() ({})

#define syslog_perror	perror

#else

/*
 * Send all errors to syslog
 */

#include <errno.h>
#include <syslog.h>

#define syslog_perror(s)	syslog(LOG_ERR, "%s: %s", s, strerror(errno))

#endif

#endif	/* !_PPTPD_SYSLOG_H */

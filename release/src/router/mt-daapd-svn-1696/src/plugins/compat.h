#ifndef _COMPAT_H_
#define _COMPAT_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_STRCASESTR
extern char *strcasestr(char* haystack, char* needle);
#endif

#ifndef HAVE_STRPTIME
extern char *strptime(char *buf, char *fmt, struct tm *tm );
#endif

#ifndef HAVE_STRTOK_R
#undef strtok_r /* defend against win32 pthreads */
extern char *strtok_r(char *s, char *delim, char **last);
#endif

#ifndef HAVE_TIMEGM
extern time_t timegm(struct tm *tm);
#endif

#endif /* _COMPAT_H_ */

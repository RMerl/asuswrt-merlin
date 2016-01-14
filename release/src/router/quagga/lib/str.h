/*
 * $Id: str.h,v 1.4 2005/09/19 09:53:21 hasso Exp $
 */

#ifndef _ZEBRA_STR_H
#define _ZEBRA_STR_H

#ifndef HAVE_SNPRINTF
extern int snprintf(char *, size_t, const char *, ...);
#endif

#ifndef HAVE_VSNPRINTF
#define vsnprintf(buf, size, format, args) vsprintf(buf, format, args)
#endif

#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCAT
extern size_t strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRNLEN
extern size_t strnlen(const char *s, size_t maxlen);
#endif

#ifndef HAVE_STRNDUP
extern char * strndup (const char *, size_t);
#endif

#endif /* _ZEBRA_STR_H */


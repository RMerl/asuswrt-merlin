#ifndef MY_DEBUG_H_
#define MY_DEBUG_H_

#ifndef MAX_DEBUG_LEVEL
# define MAX_DEBUG_LEVEL	LOG_NOTICE
#endif

/* First include stdio.h, which may contain the prototype for the external dprintf.
 * We do not want that. We redefine dprintf to our local implementation. */
#include <stdio.h>
#define dprintf(level, args...) \
	( { if ((level) <= MAX_DEBUG_LEVEL) my_dprintf((level), args); } )

#ifndef __P
# define __P(x) x
#endif
extern void my_dprintf __P((int, const char *, const char *, ...));

#endif /* MY_DEBUG_H_ */

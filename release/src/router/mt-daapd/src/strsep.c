/* Compliments of Jay Freeman <saurik@saurik.com> */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#if !HAVE_STRSEP
char *strsep(char **stringp, const char *delim) {
	char *ret = *stringp;
	if (ret == NULL) return(NULL); /* grrr */
	if ((*stringp = strpbrk(*stringp, delim)) != NULL) {
		*((*stringp)++) = '\0';
	}
	return(ret);
}
#endif /* !HAVE_STRSEP */

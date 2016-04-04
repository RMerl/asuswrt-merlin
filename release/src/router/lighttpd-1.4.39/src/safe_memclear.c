#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "settings.h"

#include "safe_memclear.h"

#include <string.h>

#if !defined(HAVE_MEMSET_S) && !defined(HAVE_EXPLICIT_BZERO)

#  if defined(HAVE_WEAK_SYMBOLS)
/* it seems weak functions are never inlined, even for static builds */
__attribute__((weak)) void __li_safe_memset_hook(void *buf, size_t len);

void __li_safe_memset_hook(void *buf, size_t len)
{
	UNUSED(buf);
	UNUSED(len);
}
#  endif /* HAVE_WEAK_SYMBOLS */

static void* safe_memset(void *s, int c, size_t n)
{
	if (n > 0) {
		volatile unsigned volatile_zero = 0;
		volatile unsigned char *vs = (volatile unsigned char*)s;

		do {
			memset(s, c, n);
		} while (vs[volatile_zero] != (unsigned char)c);
#  if defined(HAVE_WEAK_SYMBOLS)
		__li_safe_memset_hook(s, n);
#  endif /* HAVE_WEAK_SYMBOLS */
	}

	return s;
}
#endif /* !defined(HAVE_MEMSET_S) && !defined(HAVE_EXPLICIT_BZERO) */


void safe_memclear(void *s, size_t n) {
#if defined(HAVE_MEMSET_S)
	memset_s(s, n, 0, n);
#elif defined(HAVE_EXPLICIT_BZERO)
	explicit_bzero(s, n);
#else
	safe_memset(s, 0, n);
#endif
}

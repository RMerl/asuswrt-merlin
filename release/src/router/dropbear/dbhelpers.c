#include "dbhelpers.h"
#include "includes.h"

/* Erase data */
void m_burn(void *data, unsigned int len) {

#if defined(HAVE_MEMSET_S)
	memset_s(data, len, 0x0, len);
#elif defined(HAVE_EXPLICIT_BZERO)
	explicit_bzero(data, len);
#else
/* Based on the method in David Wheeler's
 * "Secure Programming for Linux and Unix HOWTO". May not be safe
 * against link-time optimisation. */
	volatile char *p = data;

	if (data == NULL)
		return;
	while (len--) {
		*p++ = 0x0;
	}
#endif
}



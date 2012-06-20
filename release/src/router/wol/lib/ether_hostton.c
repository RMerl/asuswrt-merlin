#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if !HAVE_ETHER_HOSTTON

#include "ether.h"

int
ether_hostton (const char *str, struct ether_addr *ea)
{
	/* not implemented yet */
	return 1;
}

#endif /* !HAVE_ETHER_HOSTTON */

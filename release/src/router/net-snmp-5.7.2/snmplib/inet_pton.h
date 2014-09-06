#ifndef _INET_PTON_H
#define _INET_PTON_H

#include <net-snmp/net-snmp-config.h>

#ifndef HAVE_INET_PTON
int             inet_pton(int af, const char *src, void *dst);
#endif /*HAVE_INET_PTON */

#endif /*_INET_PTON_H*/

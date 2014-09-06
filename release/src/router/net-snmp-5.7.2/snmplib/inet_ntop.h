#ifndef _INET_NTOP_H
#define _INET_NTOP_H

#include <net-snmp/net-snmp-config.h>

#ifndef HAVE_INET_NTOP
const char     *inet_ntop(int af, const void *src, char *dst, size_t size);
#endif /*HAVE_INET_NTOP */

#endif /*_INET_NTOP_H*/

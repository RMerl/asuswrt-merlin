/* misc.c - Miscellaneous library functions */

/* Written 1997-2000 by Werner Almesberger, EPFL-ICA/ICA */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h> /* for htons */

#include <atm.h>
#include <atmsap.h>


int __atmlib_fetch(const char **pos,...)
{
    const char *value;
    int ref_len,best_len,len;
    int i,best;
    va_list ap;

    va_start(ap,pos);
    ref_len = strlen(*pos);
    best_len = 0;
    best = -1;
    for (i = 0; (value = va_arg(ap,const char *)); i++) {
	len = strlen(value);
	if (*value != '!' && len <= ref_len && len > best_len &&
	  !strncasecmp(*pos,value,len)) {
	    best = i;
	    best_len = len;
	}
    }
    va_end(ap);
    if (best > -1) (*pos) += best_len;
    return best;
}


void atm_tcpip_port_mapping(char *vs_id,uint8_t protocol,uint16_t port)
{
    memcpy(vs_id,ATM_FORUM_OUI "\x01",4);
    vs_id[4] = protocol; /* e.g. IP_TCP or IP_UDP; from netinet/protocols.h */
    vs_id[5] = (htons(port) >> 8) & 255;
    vs_id[6] = htons(port) & 255;
}

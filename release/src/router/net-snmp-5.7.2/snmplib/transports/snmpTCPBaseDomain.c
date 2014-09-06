#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmpTCPBaseDomain.h>

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>

#include <net-snmp/library/snmp_transport.h>

/*
 * You can write something into opaque that will subsequently get passed back 
 * to your send function if you like.  For instance, you might want to
 * remember where a PDU came from, so that you can send a reply there...  
 */

int netsnmp_tcpbase_recv(netsnmp_transport *t, void *buf, int size,
                         void **opaque, int *olength)
{
    int rc = -1;

    if (t != NULL && t->sock >= 0) {
	while (rc < 0) {
	    rc = recvfrom(t->sock, buf, size, 0, NULL, NULL);
	    if (rc < 0 && errno != EINTR) {
		DEBUGMSGTL(("netsnmp_tcpbase", "recv fd %d err %d (\"%s\")\n",
			    t->sock, errno, strerror(errno)));
		break;
	    }
	    DEBUGMSGTL(("netsnmp_tcpbase", "recv fd %d got %d bytes\n",
			t->sock, rc));
	}
    } else {
        return -1;
    }

    if (opaque != NULL && olength != NULL) {
        if (t->data_length > 0) {
            if ((*opaque = malloc(t->data_length)) != NULL) {
                memcpy(*opaque, t->data, t->data_length);
                *olength = t->data_length;
            } else {
                *olength = 0;
            }
        } else {
            *opaque = NULL;
            *olength = 0;
        }
    }

    return rc;
}

int netsnmp_tcpbase_send(netsnmp_transport *t, void *buf, int size,
                         void **opaque, int *olength) {
    int rc = -1;

    if (t != NULL && t->sock >= 0) {
	while (rc < 0) {
	    rc = sendto(t->sock, buf, size, 0, NULL, 0);
	    if (rc < 0 && errno != EINTR) {
		break;
	    }
	}
    }
    return rc;
}

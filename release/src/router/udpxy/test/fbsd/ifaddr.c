/* @(#) interface/address conversion */

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <limits.h>

#include "osdef.h"
#include "ifaddr.h"

/* DEBUG */
#include <stdio.h>
/* DEBUG */


/* check if ifr contains info on the desired ifname
 */
static int
chkifr( const struct ifreq* ifr, const char* ifname,
        const size_t addrlen, size_t* offset )
{
    size_t sa_len = 0;

    assert(ifr && ifname && offset);

#ifdef NO_SOCKADDR_SA_LEN
    switch( ifr->ifr_addr.sa_family )
    {
    #ifndef NO_INET6_SUPPORT
        case AF_INET6: sa_len = sizeof(struct sockaddr_in6); break;
   #endif
        case AF_INET: sa_len = sizeof(struct sockaddr); break;
        default: sa_len = 0; break;
    }
#else
    sa_len = ifr->ifr_addr.sa_len;
#endif

    if( sa_len > 0 ) {
        if ( (ifr->ifr_addr.sa_family == AF_INET) &&
            (0 == strncmp(ifname, ifr->ifr_name, sizeof(struct ifreq))) &&
            (addrlen >= sa_len) ) {
            *offset = sa_len;
            return 0;
        }
    }

#if defined(__linux)
    *offset = sizeof(*ifr);
#else
    *offset = (sa_len + sizeof( ifr->ifr_name ));
    /* the above is per R. Stevens' book and not working on 64-bit Linux */
#endif

    return -1;
}



/* retrieve IPv4 address of the given network interface
 */
int
if2addr( const char* ifname,
             struct sockaddr *addr, size_t addrlen )
{
    int rc, sockfd;
    char *buf, *rec;
    size_t buflen, offset;
    int last_len;
    struct ifconf  ifc;
    struct ifreq   ifr;

    int maxi = 50;
    FILE *fp_ifc = NULL;
    size_t nsz = 0;

    static size_t IFC_TABLE_SIZE;

    static const size_t IFC_ENTRIES = 32;
    static const size_t MAX_IFCBUF_SIZE = (1024 * 256);

    IFC_TABLE_SIZE = sizeof(struct ifreq) * IFC_ENTRIES;

    assert( ifname && addr && addrlen );
    rc = 0;

    /* acquire the list of network interfaces */

    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
    if( -1 == sockfd ) return -1;

    buf = NULL; buflen = IFC_TABLE_SIZE; last_len = 0;
    for( ; buflen < MAX_IFCBUF_SIZE; buflen += IFC_TABLE_SIZE ) {
        if( NULL == (buf = malloc( buflen )) ) {
            rc = -1;
            break;
        }

        ifc.ifc_len = buflen;
        ifc.ifc_buf = buf;
        if( ioctl( sockfd, SIOCGIFCONF, &ifc ) < 0 ) {
            if( (EINVAL != errno) || (last_len != 0) ) {
                rc = errno;
                break;
            }
        }
        else {
            if( ifc.ifc_len == last_len )
                break;
            else
                last_len = ifc.ifc_len;
        }

        free( buf );
        buf = NULL;
    } /* for */

    (void) close( sockfd );
    if( buflen > MAX_IFCBUF_SIZE ) rc = -1;

    if( 0 != rc ) {
        if( NULL != buf ) free( buf );
        return rc;
    }

    assert( ifc.ifc_buf );
    /* DEBUG */
        fp_ifc = fopen("ifcbuf.dat","w");
        if (NULL==fp_ifc) {
            perror("fopen ifcbuf");
            exit(1);
        }
        nsz = fwrite(ifc.ifc_buf, ifc.ifc_len, 1, fp_ifc );
        (void) fprintf(stderr, "%ld record(s) wtitten to ifcbuf.dat\n",
                        (long)nsz );
        fclose(fp_ifc);
    /* DEBUG */

    /* look for ifname in the list */

    for( rec = ifc.ifc_buf; rec < (ifc.ifc_buf + ifc.ifc_len); ) {
        (void) memcpy( &ifr, rec, sizeof(struct ifreq) );

        (void) fprintf(stderr, "DEBUG1: rec=%p, max=%p\n",
            (void*)rec, (void*)(ifc.ifc_buf + ifc.ifc_len) );
        if( --maxi <= 0 ) break;

        offset = 0;
        rc = chkifr( &ifr, ifname, addrlen, &offset );
        if ( 0 == rc ) {
            (void) memcpy( addr, &(ifr.ifr_addr), offset );
            break;
        }

        if( 0 == offset ) break;
        rec += offset;
    } /* for */

    if( rec >= (buf + ifc.ifc_len) ) {
        rc = -1;
    }

    free( buf );
    return rc;
}


/* convert input parameter into an IPv4-address string
 */
int
get_ipv4_address( const char* s, char* buf, size_t len )
{
    struct sockaddr_in saddr;
    int rc = 0;

    assert( s && buf && len );

    if( 1 == inet_aton(s, &(saddr.sin_addr)) ) {
        (void) strncpy( buf, s, len );
    }
    else {
        rc = if2addr( s, (struct sockaddr*)&saddr, sizeof(saddr) );
        if( 0 != rc ) return rc;

        (void) strncpy( buf, inet_ntoa(saddr.sin_addr), len );
    }

    buf[ len - 1 ] = 0;
    return rc;
}


/* split input string into IP address and port
 */
int
get_addrport( const char* s, char* addr, size_t len, int* port )
{
    struct sockaddr_in saddr;
    size_t i = 0;
    int iport = 0;

    static const int ERR_NOPORT     = -1;
    static const int ERR_BADADDR    = -2;
    static const int ERR_BADPORT    = -3;
    static const int ERR_OVERFLOW   = -4;

    assert( s && addr && len && port );

    for( i = 0; (i < len) && s[i] && (':' != s[i]); ++i )
        addr[ i ] = s[ i ];
    if( i >= len )
        return ERR_OVERFLOW;
    else
        addr[i] = '\0';

    /* IP address is not followed by port */
    if( ':' != s[ i ] ) return ERR_NOPORT;

    if( 1 != inet_aton( addr, &(saddr.sin_addr)) )
        return ERR_BADADDR;

    ++i;
    if( i >= len || !s[i] ) return ERR_NOPORT;

    errno = 0;
    iport = atoi( s + i );
    if( errno || (iport <= 0) || (iport > (int)USHRT_MAX) )
        return ERR_BADPORT;

    *port = iport;
    return 0;
}


/* __EOF__ */


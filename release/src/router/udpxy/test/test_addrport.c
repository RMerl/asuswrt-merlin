/* @(#) unit test for get_addrport */

#include <stdio.h>

#include "ifaddr.h"

int main( int argc, char* const argv[] )
{
    int rc = 0;
    int i = 0, port = -1;
    char ipaddr[ 16 ] = {'\0'};

    if( argc < 2 ) {
        (void) fprintf( stderr, "Usage: %s aport [aport] ...\n",
                        argv[0] );
        return 1;
    }

    for( i = 1; i < argc; ++i ) {
        rc = get_addrport( argv[i], ipaddr, sizeof(ipaddr), &port );
        if( 0 != rc ) break;

        (void) printf( "%d. [%s] - addr [%s:%d]\n",
                        i, argv[i], ipaddr, port );
    }

    if( 0 != rc ) {
        (void) fprintf( stderr, "Error [%d] parsing [%s]\n",
                        rc, argv[i] );
    }

    return rc;
}

/* __EOF__ */


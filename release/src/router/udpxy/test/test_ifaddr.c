/* @(#) unit test for if2addr */

#include <stdio.h>

#include "ifaddr.h"

int main( int argc, char* const argv[] )
{
    int i = 0, rc = 0;
    char ipaddr[ 16 ] = {'\0'};

    if( argc < 2 ) {
        (void) fprintf( stderr, "Usage: %s ifc1 [ifc2] ...\n",
                        argv[0] );
        return 1;
    }

    for( i = 1; i < argc; ++i ) {
        rc = get_ipv4_address( argv[i], ipaddr, sizeof(ipaddr) );
        if( 0 != rc ) {
            (void) fprintf( stderr, "%d. [%s] - error [%d]\n",
                    i, argv[i], rc );
            continue;
        }

        (void) printf( "%d. [%s] - addr [%s]\n",
                        i, argv[i], ipaddr );
    }

    return rc;
}

/* __EOF__ */


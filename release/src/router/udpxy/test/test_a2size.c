/* @(#) unit test for a2time */

#include <stdio.h>
#include <sys/types.h>

#include "util.h"

int main( int argc, char* const argv[] )
{
    int rc = 0;
    ssize_t len = 0;
    int i = 0;

    if( argc < 2 ) {
        (void) fprintf( stderr, "Usage: %s sizespec [sizespec] ...\n",
                        argv[0] );
        return 1;
    }

    for( i = 1; i < argc; ++i ) {
        rc = a2size( argv[i], &len );
        if( 0 != rc ) break;

        (void) printf( "%d. size [%s] = (%ld) bytes\n",
                        i, argv[i], (long)len );
    }

    if( 0 != rc ) {
        (void) fprintf( stderr, "Error [%d] parsing [%s]\n",
                        rc, argv[i] );
    }

    return rc;
}

/* __EOF__ */


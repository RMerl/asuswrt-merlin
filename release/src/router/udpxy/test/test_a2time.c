/* @(#) unit test for a2time */

#include <stdio.h>
#include <time.h>

#include "util.h"

int main( int argc, char* const argv[] )
{
    int rc = 0;
    time_t tm = (time_t)0;
    int i = 0;

    if( argc < 2 ) {
        (void) fprintf( stderr, "Usage: %s timespec [timespec] ...\n",
                        argv[0] );
        return 1;
    }

    for( i = 1; i < argc; ++i ) {
        rc = a2time( argv[i], &tm, time(NULL) );
        if( 0 != rc ) break;

        (void) printf( "%d. time [%s] = time_t(%ld) [%s]\n",
                        i, argv[i], (long)tm,
                        Zasctime(localtime(&tm)) );
    }

    if( 0 != rc ) {
        (void) fprintf( stderr, "Error [%d] parsing [%s]\n",
                        rc, argv[i] );
    }

    return rc;
}

/* __EOF__ */


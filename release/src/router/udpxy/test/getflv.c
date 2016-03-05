/* @(#) test get_flagval method */

#include <stdio.h>
#include "util.h"

int
main( int argc, char* const argv[] )
{
    int flag = 0;
    const char* varname = NULL;

    if ( argc < 2 ) {
        (void) fprintf( stderr, "Usage: %s var\n", argv[0] );
        return 1;
    }

    varname = argv[1];
    flag = get_flagval( varname, 0 );
    (void) printf( "flag [%s] in %s\n", varname,
            (flag ? "ON" : "OFF") );
    return 0;
}

/* __EOF__ */


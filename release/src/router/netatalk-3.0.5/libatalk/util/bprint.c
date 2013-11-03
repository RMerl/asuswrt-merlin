#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <atalk/util.h>

static const char	hexdig[] = "0123456789ABCDEF";

#define BPXLEN	50
#define BPALEN	18

void bprint( data, len )
    char	*data;
    int		len;
{
    char	xout[ BPXLEN ], aout[ BPALEN ];
    int         i;

    memset( xout, 0, BPXLEN );
    memset( aout, 0, BPALEN );

    for ( i = 0; len; len--, data++, i++ ) {
	if ( i == 16 ) {
	    printf( "%-48s\t%-16s\n", xout, aout );
	    memset( xout, 0, BPXLEN );
	    memset( aout, 0, BPALEN );
	    i = 0;
	}

	if (isprint( (unsigned char)*data )) {
	    aout[ i ] = *data;
	} else {
	    aout[ i ] = '.';
	}

	xout[ (i*3) ] = hexdig[ ( *data & 0xf0 ) >> 4 ];
	xout[ (i*3) + 1 ] = hexdig[ *data & 0x0f ];
	xout[ (i*3) + 2 ] = ' ';
    }

    if ( i ) 	
	printf( "%-48s\t%-16s\n", xout, aout );

    printf("(end)\n");
}
#endif

/* @(#) udpxy multiple file-format converter
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include "udpxy.h"
#include "dpkt.h"
#include "util.h"
#include "mtrace.h"

#define MAX_REC_SIZE    1501

static const char appname[] = "upxc";

/* convert source file into destination format
 */
static int
convert2( const char* srcfile, const char* dstfile,
          upxfmt_t dfmt, FILE* log )
{
    char data[ MAX_REC_SIZE ];
    ssize_t nrd = -1, nwr = -1;

    int sfd = -1, dfd = -1;
    upxfmt_t sfmt = DT_UNKNOWN;

    double  rrec = 0, wrec = 0,
            rtotal = 0, wtotal = 0;

    assert( srcfile && log );
    assert( ETHERNET_MTU < MAX_REC_SIZE );

    sfd = open( srcfile, O_RDONLY );
    if( -1 == sfd ) {
        mperror( log, errno, "srcfile - open" );
        return -1;
    }

    if( dstfile ) {
        dfd = creat( dstfile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
        if( -1 == dfd ) {
            mperror( log, errno, "dstfile - open" );
            return -1;
        }
    }

    if( dfmt == sfmt ) {
        (void) tmfprintf( log, "%s: Same format for source and destination: "
                "writing will be skipped\n", appname);
        nwr = 0;    /* set to 0 not to confuse with an error condition */
    }

    do {
        nrd = read_frecord( sfd, data, ETHERNET_MTU, &sfmt, log );
        /*
        TRACE( (void)tmfprintf( log, "%s:R nrd=[%ld], nwr=[%ld]\n",
                    __func__, (long)nrd, (long)nwr) );
        */
        if( nrd <= 0 ) break;

        ++rrec;
        rtotal += (double)nrd;

        TRACE( (void) tmfprintf( log, "Read record [%ld] of [%ld] bytes\n",
                    (long)rrec, (long)nrd ) );

        if( (dfd <= 0) || (sfmt == dfmt) )
            continue;

        nwr = write_frecord( dfd, data, nrd, sfmt, dfmt, log );
        if( nwr <= 0 ) break;

        ++wrec;
        wtotal += (double)nwr;

        TRACE( (void) tmfprintf( log, "Wrote record [%ld] of [%ld] bytes\n",
                    (long)wrec, (long)nwr ) );
    } while( 1 );

    if( dfd > 0 ) (void) close( dfd );
    if( sfd > 0 ) (void) close( sfd );

    (void) tmfprintf( log, "Read records=[%.f], bytes=[%.f], "
            "wrote records=[%.f], bytes=[%.f]\n",
            rrec, rtotal, wrec, wtotal );

    if( (nrd < 0) || (nwr < 0)) {
        TRACE( (void)tmfprintf( log, "%s: nrd=[%ld], nwr=[%ld]\n",
                    __func__, (long)nrd, (long)nwr) );
        return -1;
    }

    return 0;
}


static void
usage()
{
    (void) tmfprintf( stderr, "Usage: %s -[dtu] -i srcfile "
            "-o dstfile\n", appname );
    return;
}


int
main( int argc, char* const argv[] )
{
    static const char* clopt = "i:o:dtr";

    char srcfile[ MAXPATHLEN ] = { '\0' };
    char dstfile[ MAXPATHLEN ] = { '\0' };

    upxfmt_t dst_fmt = DT_UNKNOWN;

    int ch, rc = 0, debug = 0;
    FILE* flog = NULL;

    while( -1 != (ch = getopt(argc, argv, clopt)) ) {
        switch( ch ) {
            case 'i':
                (void)strncpy( srcfile, optarg, sizeof(srcfile) - 1 );
                break;

            case 'o':
                (void)strncpy( dstfile, optarg, sizeof(dstfile) - 1 );
                break;

            case 't':
                dst_fmt = DT_TS;
                break;

            case 'u':
                dst_fmt = DT_UDS;
                break;

            case 'd':
                debug = 1;
                break;

            default:
                (void) tmfprintf( stderr, "%s: Unrecognized option [%c]\n",
                        appname, (char)ch );
                usage();
                return ERR_PARAM;
        } /* switch */
    } /* getopt loop */

    if( !srcfile[0] ) {
        (void) tmfprintf( stderr, "%s: Missing source-file parameter\n",
                appname );
        usage();
        return ERR_PARAM;
    }
    else {
        if( 0 != (rc = access( srcfile, R_OK)) ) {
            perror( "srcfile - access" );
            return ERR_PARAM;
        }
    }

    flog = debug ? stderr : fopen( "/dev/null", "a" );
    if( NULL == flog ) {
        perror( "fopen" );
        return ERR_INTERNAL;
    }

    TRACE( (void)tmfprintf( flog, "%s: destination format = [%s]\n",
                appname, fmt2str(dst_fmt)) );
    if( DT_UNKNOWN == dst_fmt ) {
        (void)tmfprintf( flog, "%s: destination format must be specified\n",
                appname );
        return ERR_PARAM;
    }

    rc = convert2( srcfile, dstfile, dst_fmt, flog );

    if( (NULL != flog) && (stderr != flog) )
        (void) fclose(flog);

    return (rc ? ERR_INTERNAL : 0);
}


/* __EOF__ */


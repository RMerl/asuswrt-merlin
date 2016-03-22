/* @(#) main module: dispatches command to apps within udpxy
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

#include <string.h>
#include <libgen.h>
#include <stdio.h>

static const char UDPXY[]   = "udpxy";
extern int udpxy_main( int argc, char* const argv[] );

#ifdef UDPXREC_MOD
static const char UDPXREC[] = "udpxrec";
extern int udpxrec_main( int argc, char* const argv[] );
#endif

int
main( int argc, char* const argv[] )
{
    const char* app = basename(argv[0]);

    if( 0 == strncmp( UDPXY, app, sizeof(UDPXY) ) )
        return udpxy_main( argc, argv );
#ifdef UDPXREC_MOD
    else if( 0 == strncmp( UDPXREC, app, sizeof(UDPXREC) ) )
        return udpxrec_main( argc, argv );
#endif

    (void)fprintf( stderr, "Unsupported udpxy module [%s]\n", app);
    return 1;
}

/* __EOF__ */

/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 *
 * modified from main.c. this handles afp options.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <atalk/logger.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#ifdef ADMIN_GRP
#include <grp.h>
#include <sys/types.h>
#endif /* ADMIN_GRP */

#include <atalk/paths.h>
#include <atalk/util.h>
#include <atalk/compat.h>
#include <atalk/globals.h>
#include <atalk/fce_api.h>
#include <atalk/errchk.h>

#include "status.h"
#include "auth.h"
#include "dircache.h"

#define LENGTH 512

//Change some path by Edison in 20130923
#define T_PATH_CONFDIR		"/usr/etc/"
#define T_PATH_STATEDIR		"/tmp/netatalk/"
#define T_PATH_AFPDUAMPATH	"/usr/lib/netatalk/"

/*
 * Show version information about afpd.
 * Used by "afp -v".
 */
static void show_version( void )
{
	int num, i;

	printf( "afpd %s - Apple Filing Protocol (AFP) daemon of Netatalk\n\n", VERSION );

	puts( "This program is free software; you can redistribute it and/or modify it under" );
	puts( "the terms of the GNU General Public License as published by the Free Software" );
	puts( "Foundation; either version 2 of the License, or (at your option) any later" );
	puts( "version. Please see the file COPYING for further information and details.\n" );

	puts( "afpd has been compiled with support for these features:\n" );

	num = sizeof( afp_versions ) / sizeof( afp_versions[ 0 ] );
	printf( "          AFP versions:\t" );
	for ( i = 0; i < num; i++ ) {
		printf( "%d.%d ", afp_versions[ i ].av_number/10, afp_versions[ i ].av_number%10);
	}
	puts( "" );

	printf( "         CNID backends:\t" );
#ifdef CNID_BACKEND_CDB
	printf( "cdb ");
#endif
#ifdef CNID_BACKEND_DB3
	printf( "db3 " );
#endif
#ifdef CNID_BACKEND_DBD
#ifdef CNID_BACKEND_DBD_TXN
	printf( "dbd-txn " );
#else
	printf( "dbd " );
#endif
#endif
#ifdef CNID_BACKEND_HASH
	printf( "hash " );
#endif
#ifdef CNID_BACKEND_LAST
	printf( "last " );
#endif
#ifdef CNID_BACKEND_MTAB
	printf( "mtab " );
#endif
#ifdef CNID_BACKEND_TDB
	printf( "tdb " );
#endif
	puts( "" );
}

/*
 * Show extended version information about afpd and Netatalk.
 * Used by "afp -V".
 */
static void show_version_extended(void )
{
	show_version( );

	printf( "      Zeroconf support:\t" );
#if defined (HAVE_MDNS)
	puts( "mDNSResponder" );
#elif defined (HAVE_AVAHI)
	puts( "Avahi" );
#else
	puts( "No" );
#endif

	printf( "  TCP wrappers support:\t" );
#ifdef TCPWRAP
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "         Quota support:\t" );
#ifndef NO_QUOTA_SUPPORT
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "   Admin group support:\t" );
#ifdef ADMIN_GRP
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "    Valid shell checks:\t" );
#ifndef DISABLE_SHELLCHECK
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "      cracklib support:\t" );
#ifdef USE_CRACKLIB
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "            EA support:\t" );
	puts( EA_MODULES );

	printf( "           ACL support:\t" );
#ifdef HAVE_ACLS
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "          LDAP support:\t" );
#ifdef HAVE_LDAP
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "         D-Bus support:\t" );
#ifdef HAVE_DBUS_GLIB
	puts( "Yes" );
#else
	puts( "No" );
#endif

	printf( "         DTrace probes:\t" );
#ifdef WITH_DTRACE
	puts( "Yes" );
#else
	puts( "No" );
#endif
}

/*
 * Display compiled-in default paths
 */

//Edison modify 20130923
static void show_paths( void )
{
	printf( "              afp.conf:\t%s\n", T_PATH_CONFDIR "afp.conf");
	printf( "           extmap.conf:\t%s\n", T_PATH_CONFDIR "extmap.conf");
	printf( "       state directory:\t%s\n", T_PATH_STATEDIR);
	printf( "    afp_signature.conf:\t%s\n", T_PATH_STATEDIR "afp_signature.conf");
	printf( "      afp_voluuid.conf:\t%s\n", T_PATH_STATEDIR "afp_voluuid.conf");
	printf( "       UAM search path:\t%s\n", T_PATH_AFPDUAMPATH );
	printf( "  Server messages path:\t%s\n", SERVERTEXT);
}

/*
 * Display usage information about afpd.
 */
static void show_usage(void)
{
	fprintf( stderr, "Usage:\tafpd [-d] [-F configfile]\n");
	fprintf( stderr, "\tafpd -h|-v|-V\n");
}

void afp_options_parse_cmdline(AFPObj *obj, int ac, char **av)
{
    int c, err = 0;

    while (EOF != ( c = getopt( ac, av, "dF:vVh" )) ) {
        switch ( c ) {
        case 'd':
            obj->cmdlineflags |= OPTION_DEBUG;
            break;
        case 'F':
            obj->cmdlineconfigfile = strdup(optarg);
            break;
        case 'v':	/* version */
            show_version( ); puts( "" );
            show_paths( ); puts( "" );
            exit( 0 );
            break;
        case 'V':	/* extended version */
            show_version_extended( ); puts( "" );
            show_paths( ); puts( "" );
            exit( 0 );
            break;
        case 'h':	/* usage */
            show_usage();
            exit( 0 );
            break;
        default :
            err++;
        }
    }
    if ( err || optind != ac ) {
        show_usage();
        exit( 2 );
    }

    return;
}

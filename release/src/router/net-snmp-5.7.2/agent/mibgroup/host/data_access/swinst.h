/*
 * swinst data access header
 *
 * $Id: swinst.h 15346 2006-09-26 23:34:50Z rstory $
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#ifndef NETSNMP_ACCESS_SWINST_CONFIG_H
#define NETSNMP_ACCESS_SWINST_CONFIG_H

/*
 * all platforms use this generic code
 */
config_exclude(host/hr_swinst)

/*
 * select the appropriate architecture-specific interface code
 */
#if   defined( darwin )
    config_require(host/data_access/swinst_darwin)
#elif defined( HAVE_DPKG_QUERY )
    config_require(host/data_access/swinst_apt)
#elif defined( HAVE_LIBRPM ) && defined( linux )
    config_require(host/data_access/swinst_rpm)
#elif defined( HAVE_PKGLOCS_H ) || defined( hpux9 ) || defined( hpux10 ) || defined( hpux11 ) || defined( freebsd2 ) || defined( linux ) || defined( openbsd )
    config_require(host/data_access/swinst_pkginfo)
#else
    config_warning(This platform does not yet support hrSWInstalledTable rewrites)
    config_require(host/data_access/swinst_null)
#endif

void init_swinst( void );
void shutdown_swinst( void );

#endif /* NETSNMP_ACCESS_SWINST_CONFIG_H */

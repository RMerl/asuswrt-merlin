/*
 * sd-idle-2.6.c
 *
 * Copyright (C) 2010 Jeff Gibbons All Rights Reserved
 * 
 * Author: Jeff Gibbons aka karog
 * Date:   Oct 1, 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version. See the GNU General Public License
 * for more details at:
 *
 *   http://www.gnu.org/licenses/gpl.html
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * This program manages devices /dev/sdX 'a'<=X<='z' watching for idleness
 * and spinning them down. It runs as a daemon. For usage do:
 *
 *   sd-idle-2.6 --help
 * 
 * Requires linux 2.6.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define VERSION "1.00"

#define IDLETIME_DEFAULT 900
#define IDLETIME_MIN     300

#define CHECKTIME_DEFAULT 30
#define CHECKTIME_MIN      5

// log to stdout if non-zero else to syslog
static
int
_log_stdout;

// time in seconds a disk must be idle before being spun down
static
int
_idletime = IDLETIME_DEFAULT;

// time in seconds disks have been idle
static
int
_idletimes [ 26 ];

// time in seconds to sleep between idle checks
static
int
_checktime = CHECKTIME_DEFAULT;

// time in seconds between transitions, up and down
static
time_t
_transtimes [ 26 ];

#define TRANSTIME_BUF_SIZE 128

#define _bit_clear(    bits, b ) bits &= ~( 1 << ( b ) )
#define _bit_set(      bits, b ) bits |=  ( 1 << ( b ) )
#define _is_bit_clear( bits, b ) ( 0 == ( bits & ( 1 << ( b ) ) ) )
#define _is_bit_set(   bits, b ) ( 0 != ( bits & ( 1 << ( b ) ) ) )

// bits indicating if a disk is being managed
static
uint32_t
_managing = -1;

// bits indicating if a disk is spinning
static
uint32_t
_spinning = -1;

// I/O stats for disks
static
char
_diskstats [ 26 ] [ 160 ];

static void _check_linux_version ( void );

static void _parse_options ( int argc, char * argv [ ] );

static void _manage ( void );

static void _log ( 	int level, const char * fmt, ... );

int
main (
    int    argc,
    char * argv [ ] )
{
	openlog ( basename ( argv [ 0 ] ), LOG_PID, LOG_USER );

	_check_linux_version();

    _parse_options ( argc, argv );

    daemon ( 0, 0 );

    _log ( LOG_INFO, "initialized" );

    _manage ( );

    return EXIT_SUCCESS;
}

static
void
_log (
	int          level,
	const char * fmt,
	... )
{
	va_list arg_list;

	va_start ( arg_list, fmt );

	if ( _log_stdout )
		vprintf ( fmt, arg_list );
	else
		vsyslog ( level, fmt, arg_list );

	va_end ( arg_list );
}

static
void
_check_linux_version (
	void )
{
	struct utsname name;

	if ( 0 > uname ( &name ) )
	{
		_log ( LOG_ERR, "Failure to get linux version number: %s", strerror ( errno ) );
		exit ( EXIT_FAILURE );
	}

	switch ( name.release [ 2 ] )
	{
	case '6':
		return;
	default:
		_log ( LOG_ERR, "Wrong linux version: %s; must be 2.6; exiting", name.release );
		exit ( EXIT_FAILURE );
	}
}

static
void
_usage (
    char * progname,
    int    exit_status )
{
	progname = basename ( progname );

    _log ( LOG_INFO, "Usage: ( runs as a daemon )\n" );
    _log ( LOG_INFO, "  %s [ -d devices ] [ -i idletime ] [ -c checktime ] [ -h --help ] [ -v --version ]\n", progname);
    _log ( LOG_INFO, "    -d  [a-z]+   include where a => /dev/sda, b => /dev/sdb (default is all disks)\n" );
    _log ( LOG_INFO, "       ![a-z]+   exclude\n" );
    _log ( LOG_INFO, "    -i n         n seconds a disk must be idle to spin it down (default %d, min %d)\n",
                            IDLETIME_DEFAULT, IDLETIME_MIN );
    _log ( LOG_INFO, "    -c n         n seconds to sleep between idle checks (default %d, min %d)\n",
                            CHECKTIME_DEFAULT, CHECKTIME_MIN);
    _log ( LOG_INFO, "    -h --help    usage\n" );
    _log ( LOG_INFO, "    -v --version version\n" );
    _log ( LOG_INFO, "  for example:\n" );
    _log ( LOG_INFO, "    %s        will manage all disks with default times\n", progname );
    _log ( LOG_INFO, "    %s -d bc  will manage /dev/sdb, /dev/sdc with default times\n", progname );
    _log ( LOG_INFO, "    %s -d !bc will manage all disks except /dev/sdb, /dev/sdc with default times\n", progname );
    _log ( LOG_INFO, "    %s -i 600 will manage all disks spinning down after 600 seconds or 10 minutes\n", progname );

    exit ( exit_status );
}

static
char *
_get_param (
    int    argc,
    char * argv [ ],
    int    a,
    char * option )
{
    if ( a < argc )
        return argv [ a ];
    _log ( LOG_ERR, "Missing parameter for option: %s; exiting", option );
    exit ( EXIT_FAILURE );
}

static
void
_set_disks (
    char * disks )
{
    int include = 1;

    switch ( *disks )
    {
    case 0:   // empty so default to all
        return;
    case '!': // exclude
        include = 0;
        disks++;
        break;
    default:  // include
        _managing = 0;
        break;
    }

    for ( ; 0 != *disks; disks++ )
    {
        if ( ( *disks < 'a' ) || ( 'z' < *disks ) )
        {
            _log ( LOG_WARNING, "Invalid disk: %c; must be among [a-z]; ignoring", *disks );
            continue;
        }
        if ( include )
            _bit_set   ( _managing, *disks - 'a' );
        else
            _bit_clear ( _managing, *disks - 'a' );
    }

    if ( 0 == _managing )
    {
        _log ( LOG_ERR, "No disks specified to be managed; exiting" );
        exit ( 1 );
    }
}

static
void
_set_idletime (
    char * idletime )
{
    char * endp;

    _idletime = strtol ( idletime, &endp, 10 );
    if ( ( 0 != *idletime ) && ( 0 == *endp ) )
    {
        if ( _idletime < IDLETIME_MIN )
        {
            _log ( LOG_WARNING, "IDLETIME set too small which is bad for disks: %d; setting to minimum %d",
                    _idletime, IDLETIME_MIN );
            _idletime = IDLETIME_MIN;
        }
        return;
    }

    _log ( LOG_WARNING, "Invalid idletime: %s; using default %d", idletime, IDLETIME_DEFAULT );
    _idletime = IDLETIME_DEFAULT;
}

static
void
_set_checktime (
    char * checktime )
{
    char * endp;
    _checktime = strtol ( checktime, &endp, 10 );
    if ( ( 0 != *checktime ) && ( 0 == *endp ) )
    {
        if ( _checktime < CHECKTIME_MIN )
        {
            _log ( LOG_WARNING, "CHECKTIME set too small: %d; setting to minimum %d",
                    _checktime, CHECKTIME_MIN );
            _checktime = CHECKTIME_MIN;
        }
        return;
    }

    _log ( LOG_WARNING, "Invalid checktime: %s; using default %d", checktime, CHECKTIME_DEFAULT );
    _checktime = CHECKTIME_DEFAULT;
}

static
void
_parse_options (
    int    argc,
    char * argv [ ] )
{
    int    a;

    for ( a = 1; a < argc; a++ )
    {
        if ( !strcmp ( "-h", argv [ a ] ) || !strcmp ( "--help", argv [ a ] ) )
        {
        	_log_stdout = 1;
            _usage ( argv [ 0 ], EXIT_SUCCESS );
        }

        if ( !strcmp ( "-v", argv [ a ] ) || !strcmp ( "--version", argv [ a ] ) )
        {
        	_log_stdout = 1;
        	_log ( LOG_INFO, "%s version %s\n", basename ( argv [ 0 ] ), VERSION );
            exit ( EXIT_SUCCESS );
        }
    }

    for ( a = 1; a < argc; a++ )
    {
        if ( !strcmp ( "-d", argv [ a ] ) )
        {
            _set_disks ( _get_param ( argc, argv, ++a, "-d" ) );
            continue;
        }
        if ( !strcmp ( "-i", argv [ a ] ) )
        {
            _set_idletime ( _get_param ( argc, argv, ++a, "-i" ) );
            continue;
        }
        if ( !strcmp ( "-c", argv [ a ] ) )
        {
            _set_checktime ( _get_param ( argc, argv, ++a, "-c" ) );
            continue;
        }

        _log ( LOG_ERR, "Unknown option: %s; exiting", argv [ a ] );
        _usage ( argv [ 0 ], EXIT_FAILURE );
    }
}

static
void
_disk_stop (
    int disk )
{
    sg_io_hdr_t   sg_io_hdr;
    unsigned char sense_data [ 255 ];
    char          device [ 16 ];
    int           fd;

    memset( &sg_io_hdr, 0, sizeof ( sg_io_hdr ) );

    sg_io_hdr.interface_id    = 'S';
    sg_io_hdr.dxfer_direction = SG_DXFER_NONE;

    // STOP cmd
    sg_io_hdr.cmdp    = ( unsigned char [ ] ) { START_STOP, 0x0, 0x0, 0x0, 0x0, 0x0 };
    sg_io_hdr.cmd_len = 6;

    // sense data
    sg_io_hdr.sbp       = sense_data;
    sg_io_hdr.mx_sb_len = sizeof ( sense_data );

    sprintf ( device, "/dev/sd%c", 'a' + disk );

    if ( 0 > (fd = open ( device, O_RDONLY ) ) )
    {
        _log ( LOG_WARNING, "Failure to open %s for spinning down: %s", device, strerror ( errno ) );
        return;
    }

    if ( 0 > ioctl ( fd, SG_IO, &sg_io_hdr ) )
    {
        _log ( LOG_WARNING, "Failure to spin down %s: %s", device, strerror ( errno ) );
        _log ( LOG_WARNING, "Sense data: %s", sense_data );
    }

    if ( 0 != sg_io_hdr.status )
        _log ( LOG_WARNING, "Non-zero status spinning down %s: %d", device, sg_io_hdr.status );

    close ( fd );
}

static
char *
_transition_message (
    int    disk,
    char * direction,
    char * buf)
{
    time_t then, now;
    int    days, hours, mins, secs;
    char * bufp;

    bufp = buf + sprintf ( buf, "spinning %s /dev/sd%c", direction, 'a' + disk );

    then = _transtimes [ disk ];
    _transtimes [ disk ] = time ( &now );

	// if this is the first time, no interval to include in message
    if ( 0 == then )
        return buf;

    bufp += sprintf ( bufp, " after " );

    secs   = now - then;
    days   = secs  / 86400;
    secs  -= days  * 86400;
    hours  = secs  / 3600;
    secs  -= hours * 3600;
    mins   = secs  / 60;
    secs  -= mins  * 60;
    if ( days  ) bufp += sprintf ( bufp, "%d days " , days  );
    if ( hours ) bufp += sprintf ( bufp, "%d hours ", hours );
    if ( mins  ) bufp += sprintf ( bufp, "%d mins " , mins  );
    if ( secs  ) bufp += sprintf ( bufp, "%d secs " , secs  );
    return buf;
}

static
void
_disk_idle (
    int disk )
{
    char msg_buf [ TRANSTIME_BUF_SIZE ];

	// increment idle time
	_idletimes [ disk ] += _checktime;

	// if was not spinning or not enough ilde time
    if ( _is_bit_clear ( _spinning, disk ) || ( _idletime > _idletimes [ disk ] ) )
        return;

    _log ( LOG_INFO, "%s", _transition_message ( disk, "down", msg_buf ) );

    _disk_stop ( disk );

    _bit_clear ( _spinning, disk );
}

static
void
_disk_active (
    int    disk,
    char * diskstats_line )
{
    char msg_buf [ TRANSTIME_BUF_SIZE ];

    // if was not spinning, inform spinning up
    if ( _is_bit_clear ( _spinning, disk ) )
    {
        _log ( LOG_INFO, "%s", _transition_message ( disk, "up", msg_buf ) );
        _bit_set ( _spinning, disk );
    }

    _idletimes [ disk ] = 0;
    strcpy ( _diskstats [ disk ], diskstats_line );
}

static
void
_manage_diskstats_line (
    char * diskstats_line )
{
    char * sd;
    char * io;
    int    disk;
    int    i;

    // look for " sdX " with 'a' <= X <= 'z'
    if ( NULL == ( sd = strstr ( diskstats_line, " sd" ) ) || ( ' ' != sd [ 4 ] ) ||
         ( ( disk = sd [ 3 ] ) < 'a' ) || ( 'z' < disk ) )
        return;

    disk -= 'a';
    if ( _is_bit_clear ( _managing, disk ) )
        return;

    if ( strlen ( diskstats_line ) >= sizeof ( _diskstats [ 0 ] ) )
    {
        _log ( LOG_WARNING, "/proc/diskstats line too long: %s; ignoring", diskstats_line );
        return;
    }

    // remove prefix through device name
    diskstats_line = sd + 5;

    // find I/O in progress
    for ( i = 0, io = diskstats_line; ( i < 8 ) && ( NULL != ( io = strchr ( ++io, ' ' ) ) ); i++ );
    if ( NULL == io )
    {
        _log ( LOG_WARNING, "Missing I/O in progress stat for device: sd%c", 'a' + disk );
        return;
    }

    // check for idleness
    if ( ( '0' == *++io ) &&  !strcmp ( _diskstats [ disk ], diskstats_line ) )
        _disk_idle   ( disk );
    else
        _disk_active ( disk, diskstats_line );
}

static
void
_manage (
    void )
{
    FILE * proc_diskstats;
    char   diskstats [ 256 ];

    for ( ; ; )
    {
        // open /proc/diskstats
        proc_diskstats = fopen ( "/proc/diskstats", "r" );
        if ( NULL == proc_diskstats )
        {
            _log ( LOG_ERR, "Failure to open /proc/diskstats: %d - %s", errno, strerror ( errno ) );
            exit ( EXIT_FAILURE );
        }

        // process each line
        while ( NULL != fgets ( diskstats, sizeof ( diskstats ), proc_diskstats ) )
            _manage_diskstats_line ( diskstats );

        fclose ( proc_diskstats );

        sleep ( _checktime );
    }
}

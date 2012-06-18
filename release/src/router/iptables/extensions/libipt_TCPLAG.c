/* libipt_TCPLAG.c -- module for iptables to interface with TCPLAG target
 * Copyright (C) 2002 Telford Tendys <telford@triode.net.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Shared library add-on to iptables for TCPLAG target control
 *
 * This allows installation and removal of the TCPLAG target
 * Note that there is a lot more commentary in this file than
 * the average libipt target (i.e. more than none) but these
 * are just my deductions based on examination of the source
 * and 
 */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_TCPLAG.h>

/*
 * This merely dumps out text for the user
 * (saves keeping the manpage up to date)
 */
static void help( void )
{
	printf( "TCPLAG options:\n"
			" --log-level=n    Set the syslog level to n (integer 0 to 7)\n\n"
			" --log-prefix=xx  Prefix log messages with xx\n" );
}

/*
 * See "man getopt_long" for an explanation of this structure
 *
 * If one of our options DOES happen to come up then we get
 * a callback into parse(), our vals must not overlap with any
 * normal iptables short options (I think) because there is only
 * one actual options handler and it can't tell whose options it
 * is really looking at unless they are all distinct.
 *
 * These are exactly the same as the LOG target options
 * and have the same purpose.
 */
static const struct option opts[] =
{
	{ "log-level",     1, 0, '!' },
	{ "log-prefix",    1, 0, '#' },
	{ 0 }
};

/*
 * This gives us a chance to install some initial values in
 * our own private data structure (which is at t->data).
 * Probably we could fiddle with t->tflags too but there is
 * no great advantage in doing so.
 */
static void init( struct ipt_entry_target *t, unsigned int *nfcache )
{
	struct ipt_tcplag *el = (struct ipt_tcplag *)t->data;
	memset( el, 0, sizeof( struct ipt_tcplag ));
	el->level = 4; /* Default to warning level */
	strcpy( el->prefix, "TCPLAG:" ); /* Give a reasonable default prefix */
}

/*
 * It doesn't take much thought to see how little thought has gone into
 * this particular API. However, to add to that I'd just like to say that
 * it can be made to work and small miracles are still miracles.
 *
 * The input parameters are as follows:
 * 
 *  c      --  the 'val' from opts[] above, could possibly be something
 *             we cannot recognise in which case return(0).
 *             If we do recognise it then return(1).
 *
 *  argv   --  in case we want to take parameters from the command line,
 *             not sure how to safely ensure that the parameter that
 *             we want to take will really exist, presumably getopt_long()
 *             will have already checked such things (what about optional
 *             parameters huh?).
 *
 *  invert --  if the option parameter had '!' in front of it, usually this
 *             would inversion of the matching sense but I don't think it
 *             is useful in the case of targets.
 *
 *  flags  --  always (*target)->tflags for those who feel it is better
 *             to access this field indirectly <shrug> starts of
 *             zero for a fresh target, gets fed into final_check().
 *
 *  entry  --  apparently useless
 *
 *  target --  the record that holds data about this target,
 *             most importantly, our private data is (*target)->data
 *             (this has already been malloced for us).
 */
static int parse( int c, char **argv, int invert, unsigned int *flags,
				  const struct ipt_entry *entry, struct ipt_entry_target **target )
{
	struct ipt_tcplag *el = (struct ipt_tcplag *)( *target )->data;
/*
 * Yeah, we could complain about options being issued twice but
 * is it really worth the trouble? Will it make the world a better place?
 */
	switch( c )
	{
/*
 * I really can't be bothered with the syslog naming convention,
 * it isn't terribly useful anyhow.
 */
		case '!':
			el->level = strtol( optarg, 0, 10 );
			return( 1 );
/*
 * 15 chars should be plenty
 */
		case '#':
			strncpy( el->prefix, optarg, 15 );
			el->prefix[ 14 ] = 0; /* Force termination */
			return( 1 );
	}
	return( 0 );
}

/*
 * This gets given the (*target)->tflags value from
 * the parse() above and it gets called after all the
 * parsing of options is completed. Thus if one option
 * requires another option you can test the flags and
 * decide whether everything is in order.
 *
 * If there is a problem then do something like:
 *		exit_error( PARAMETER_PROBLEM, "foobar parameters detected in TCPLAG target");
 *
 * In this case, no errors are possible
 */
static void final_check( unsigned int flags ) { }
/*
 * This print is for the purpose of user-readable display
 * such as what "iptables -L" would give. The notes in
 * iptables.h say that target could possibly be a null pointer
 * but coding of the various libipt_XX.c modules suggests
 * that it is safe to presume target is correctly initialised.
 */
static void print(const struct ipt_ip *ip, const struct ipt_entry_target *target, int numeric)
{
	const struct ipt_tcplag *el = (const struct ipt_tcplag *)target->data;
	printf("TCPLAG <%d>", el->level );
	if( el->prefix[ 0 ])
	{
		printf( "%s", el->prefix );
	}
}

/*
 * As above but command-line style printout
 * (machine-readable for restoring table)
 */
static void save( const struct ipt_ip *ip, const struct ipt_entry_target *target )
{
	const struct ipt_tcplag *el = (const struct ipt_tcplag *)target->data;
	printf("TCPLAG --log-level=%d", el->level );
	if( el->prefix[ 0 ])
	{
/*
 * FIXME: Should have smarter quoting
 */
		printf( " --log-prefix='%s'", el->prefix );
	}
}

/*
 * The version must match the iptables version exactly
 * which is a big pain, could use `iptables -V` in makefile
 * but we can't guarantee compatibility with all iptables
 * so we are stuck with only supporting one particular version.
 */
static struct iptables_target targ =
{
next:	          0,
name:             "TCPLAG",
version:          IPTABLES_VERSION,
size:             IPT_ALIGN( sizeof( struct ipt_tcplag )),
userspacesize:    IPT_ALIGN( sizeof( struct ipt_tcplag )),
help:             &help,
init:             &init,
parse:            &parse,
final_check:      &final_check,
print:            &print,
save:             &save,
extra_opts:       opts
};

/*
 * Always nervous trusting _init() but oh well that is the standard
 * so have to go ahead and use it. This registers your target into
 * the list of available targets so that your options become available.
 */
void _init( void ) { register_target( &targ ); }

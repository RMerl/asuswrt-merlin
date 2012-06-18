/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: gethostinfo.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";

/********************************************************************
 * char *get_fqdn (char *shorthost)
 * get the fully-qualified domain name for a host.
 *
 * NOTE: get_fqdn returns a pointer to static data, so copy the result!!
 * i.e.-  strcpy (fqhostname, get_fqdn (hostname));
 * 
 ********************************************************************/

#include "lp.h"
#include "gethostinfo.h"
#include "linksupport.h"
#include "getqueue.h"
#include "globmatch.h"
#if defined(HAVE_ARPA_NAMESER_H)
# include <arpa/nameser.h>
#endif
#if defined(HAVE_RESOLV_H)
# include <resolv.h>
#endif
/**** ENDINCLUDE ****/

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

#undef HAVE_INNETGR 0

void Clear_host_information( struct host_information *info )
{
	Free_line_list( &info->host_names );
	Free_line_list( &info->h_addr_list );
	if( info->shorthost ) free(info->shorthost ); info->shorthost = 0;
	if( info->fqdn ) free(info->fqdn ); info->fqdn = 0;
}

void Clear_all_host_information(void)
{
	Clear_host_information( &Localhost_IP );	/* IP from localhost lookup */
	Clear_host_information( &Host_IP );	/* IP from localhost lookup */
	Clear_host_information( &RemoteHost_IP );	/* IP from localhost lookup */
	Clear_host_information( &LookupHost_IP );	/* IP from localhost lookup */
	Clear_host_information( &PermHost_IP );	/* IP from localhost lookup */
}

/***************************************************************************
 * void Check_for_dns_hack( struct hostent *h_ent )
 * Check to see that you do not have some whacko type returned by DNS
 ***************************************************************************/
void Check_for_dns_hack( struct hostent *h_ent )
{
	int count = 1;
	switch( h_ent->h_addrtype ){
	case AF_INET:
		count = (h_ent->h_length != sizeof(struct in_addr )); break;
#if defined(IPV6)
	case AF_INET6:
		count = (h_ent->h_length != sizeof(struct in6_addr)); break;
#endif
	}
	if( count ){
		FATAL(LOG_ALERT)
		"Check_for_dns_hack: HACKER ALERT! DNS address length wrong, prot %d len %d",
			h_ent->h_addrtype, h_ent->h_length );
	}
}

/********************************************************************
 * char *Find_fqdn (
 * struct host_information *info - we put information here
 * char *shorthost - hostname
 *
 * Finds IP address and fully qualified domain name for a host
 *
 * ASSUMES: shorthost name is shorter than LINEBUFFER
 * RETURNS: FQDN if found, 0 if not found
 ********************************************************************/
char *Find_fqdn( struct host_information *info, const char *shorthost )
{
	struct hostent *host_ent = 0;

	DEBUG3( "Find_fqdn: host '%s'", shorthost );
	Clear_host_information( info );

	if( shorthost == 0 || *shorthost == 0 ){
		LOGMSG( LOG_ALERT) "Find_fqdn: called with '%s', HACKER ALERT",
			shorthost );
		return(0);
	}
	if( safestrlen(shorthost) > 64 ){
		FATAL(LOG_ALERT) "Find_fqdn: hostname too long, HACKER ALERT '%s'",
			shorthost );
	}
#if defined(HAVE_GETHOSTBYNAME2)
	if( host_ent == 0 ){
		host_ent = gethostbyname2( shorthost, AF_Protocol() );
		DEBUG3( "Find_fqdn: gethostbyname2 returned 0x%lx", Cast_ptr_to_long(host_ent));
	}
#endif
	if( host_ent == 0 ){
		host_ent = gethostbyname( shorthost );
		DEBUG3( "Find_fqdn: gethostbyname returned 0x%lx", Cast_ptr_to_long(host_ent));
	}
	if( host_ent == 0 ){
		DEBUG3( "Find_fqdn: no entry for host '%s'", shorthost );
		return( 0 );
	}
	return( Fixup_fqdn( shorthost, info, host_ent) );
}

char *Fixup_fqdn( const char *shorthost, struct host_information *info,
	struct hostent *host_ent )
{
	char **list, *s, *fqdn = 0;
	/* sigh... */

	Check_for_dns_hack(host_ent);

	/* problem: some gethostbyname() implementations do not return FQDN
	 * apparently the gethostbyaddr does... This is really quite silly,
	 * but here is a work around
	 * LINUX BRAIN DAMAGE - as of Version 4.0 RedHat, Jan 2, 1997
	 * it has been observed that the LINUX gethostbyname() clobbers
	 * buffers returned by gethostbyaddr() BEFORE they are
	 * used and this is not documented.  This has the side effect that using
	 * buffers returned by gethostbyname to gethostbyaddr() calls will
	 * get erroneous results,  and in addition will also modify the original
	 * values in the structures pointed to by gethostbyaddr.
	 *
	 * After the call to gethostbyaddr,  you will need to REPEAT the call to
	 * gethostbyname()...
	 *
	 * This implementation of gethostbyname()/gethostbyaddr() violates an
	 * important principle of library design, which is functions should NOT
	 * interact, or if they do, they should be CLEARLY documented.
	 *
	 * To say that I am not impressed with this is a severe understatement.
	 * Patrick Powell, Jan 29, 1997
	 */
	fqdn = 0;
	if( safestrchr( host_ent->h_name, '.' ) ){
		fqdn = (char *)host_ent->h_name;
	} else if( (list = host_ent->h_aliases) ){
		for( ; *list && !safestrchr(*list,'.'); ++list );
		fqdn = *list;
	}
	if( fqdn == 0 ){
		char buffer[64];
		struct sockaddr temp_sockaddr;
		/* this will fit as sockaddr contains subfields */
		memcpy( &temp_sockaddr, *host_ent->h_addr_list, host_ent->h_length );
		DEBUG3("Fixup_fqdn: using gethostbyaddr for host '%s', addr '%s'",
			host_ent->h_name, inet_ntop( host_ent->h_addrtype,
			*host_ent->h_addr_list, buffer, sizeof(buffer)) );
		host_ent = gethostbyaddr( (void *)&temp_sockaddr,
			host_ent->h_length, host_ent->h_addrtype );
		if( host_ent ){
			/* sigh... */
			Check_for_dns_hack(host_ent);
			DEBUG3("Fixup_fqdn: gethostbyaddr found host '%s', addr '%s'",
				host_ent->h_name,
				inet_ntop( host_ent->h_addrtype,
				*host_ent->h_addr_list, buffer,
				sizeof(buffer)) );
		} else { /* this failed */
		/* we have to do the lookup AGAIN */
#if defined(HAVE_GETHOSTBYNAME2)
			host_ent = gethostbyname2( shorthost, AF_Protocol() );
#else
			host_ent = gethostbyname( shorthost );
#endif
			if( host_ent == 0 ){
				FATAL(LOG_ERR) "Fixup_fqdn: 2nd search failed for host '%s'",
					shorthost );
			}
			/* sigh... */
			Check_for_dns_hack(host_ent);
		}
	}

	if( fqdn == 0 ){
		if( safestrchr( host_ent->h_name, '.' ) ){
			fqdn = (char *)host_ent->h_name;
		} else if( (list = host_ent->h_aliases) ){
			for( ; *list && !safestrchr(*list,'.'); ++list );
			fqdn = *list;
		}
		if( fqdn == 0 ) fqdn = (char *)host_ent->h_name;
	}

	info->h_addrtype = host_ent->h_addrtype;
	info->h_length = host_ent->h_length;
	/* put the names in the database */
	fqdn = info->fqdn = safestrdup(fqdn,__FILE__,__LINE__);
	info->shorthost = safestrdup(fqdn,__FILE__,__LINE__);
	if( (s = safestrchr(info->shorthost,'.')) ) *s = 0;

	Add_line_list(&info->host_names,(char *)host_ent->h_name,0,0,0 );
	for( list = host_ent->h_aliases; list && (s = *list); ++list ){
		Add_line_list(&info->host_names,s,0,0,0 );
	}

	for( list = host_ent->h_addr_list; list && *list; ++list ){
		s = malloc_or_die(info->h_length,__FILE__,__LINE__);
		memcpy(s, *list, info->h_length );
		Check_max( &info->h_addr_list, 2 );
		info->h_addr_list.list[ info->h_addr_list.count++ ] = s;
		info->h_addr_list.list[ info->h_addr_list.count ] = 0;
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3) Dump_host_information( "Fixup_fqdn", info );
#endif

	DEBUG2("Fixup_fqdn '%s': returning '%s'", shorthost, fqdn );
	return(fqdn);
}

/***************************************************************************
 * char *Get_local_host()
 * Get the fully-qualified hostname of the local host.
 * Tricky this; needs to be run after the config file has been read;
 * also, it depends a lot on the local DNS/NIS/hosts-file/etc. setup.
 *
 * Patrick Powell Fri Apr  7 07:47:23 PDT 1995
 * 1. we use the gethostname() call if it is available
 *    If we have the sysinfo call, we use it instead.
 * 2. we use the uname() call if it is available
 * 3. we get the $HOST environment variable
 ***************************************************************************/

#if defined(HAVE_SYS_SYSTEMINFO_H)
# include <sys/systeminfo.h>
#endif

#if !defined(HAVE_GETHOSTNAME_DEF)
 extern int gethostname (char *nbuf, long nsiz);
#endif
#if !defined(HAVE_GETHOSTNAME)
 int gethostname( char *nbuf, long nsiz )
{
# if defined(HAVE_SYSINFO)
	int i;
	i = sysinfo(SI_HOSTNAME,nbuf, nsiz );
	DEBUG4("gethostname: using sysinfo '%s'", nbuf );
	return( i );
# else
#   ifdef HAVE_UNAME
#     if defined(HAVE_SYS_UTSNAME_H)
#       include <sys/utsname.h>
#     endif
#     if defined(HAVE_UTSNAME_H)
#       include <utsname.h>
#     endif
#
	struct utsname unamestuff;  /* structure to catch data from uname */
	if (uname (&unamestuff) < 0) {
		return -1;
	}
	(void) mystrncpy (nbuf, unamestuff.nodename, nsiz);
	return( 0 );
#   else
	char *name;
	name = getenv( "HOST" );
	if( name == 0 ){
		return( -1 );
	}
	(void) mystrncpy (nbuf, name, nsiz);
	return( 0 );
#   endif /* HAVE_UNAME */
# endif /* HAVE_SYSINFO */
}
# endif /* HAVE_GETHOSTNAME */


/****************************************************************************
 * void Get_local_host()
 * 1. We try the usual method of getting the host name.
 *    This may fail on a PC based system where the host name is usually
 *    not available.
 * 2. If we have no host name,  then we try to use the IP address
 *    This will almost always work on a system with a single interface.
 ****************************************************************************/

void Get_local_host( void )
{
	char host[LINEBUFFER];
	char *fqdn;
	 /*
	  * get the Host computer Name
	  */
	host[0] = 0;
#if 0
	if( gethostname (host, sizeof(host)) < 0 
		|| host[0] == 0 ) {
		FATAL(LOG_ERR) "Get_local_fqdn: no host name" );
	}
	fqdn = Find_fqdn( &Host_IP, host );
#else
Check_max(&(Host_IP.host_names), 2);
Check_max(&(Host_IP.h_addr_list), 2);
{
char q0[16], q1[16], q2[16], q3[16], hsh[32], hfq[32];
strcpy(q0, "127");
strcpy(q1, "0");
strcpy(q2, "0");
strcpy(q3, "1");
strcpy(hsh, "localhost");
strcpy(hfq, "localhost.localdomain");
Host_IP.shorthost=&hsh[0];
Host_IP.fqdn=&hfq[0];
*(Host_IP.host_names.list)=Host_IP.fqdn;
Host_IP.host_names.count=2;
Host_IP.host_names.max=102;
Host_IP.h_addrtype=2;
Host_IP.h_length=4;
*(Host_IP.h_addr_list.list+0)=&q0[0];
*(Host_IP.h_addr_list.list+1)=&q1[0];
*(Host_IP.h_addr_list.list+2)=&q2[0];
*(Host_IP.h_addr_list.list+3)=&q3[0];
Host_IP.h_addr_list.count=1;
Host_IP.h_addr_list.max=102;
}
#endif
#if 0
	DEBUG3("Get_local_host: fqdn=%s", fqdn);
	if( fqdn == 0 ){
		FATAL(LOG_ERR) "Get_local_host: hostname '%s' bad", host );
	}
	Set_DYN( &FQDNHost_FQDN, Host_IP.fqdn );
	Set_DYN( &ShortHost_FQDN, Host_IP.shorthost );
	DEBUG1("Get_local_host: ShortHost_FQDN=%s, FQDNHost_FQDN=%s",
		ShortHost_FQDN, FQDNHost_FQDN);
    if( Find_fqdn( &Localhost_IP, LOCALHOST) == 0 ){
        FATAL(LOG_ERR) "Get_local_host: 'localhost' IP address not available!");
    }
#else
Localhost_IP=Host_IP;
printf("Localhost_IP.shorthost=%s\n", Localhost_IP.shorthost);
printf("Localhost_IP.fqdn=%s\n", Localhost_IP.fqdn);
printf("Localhost_IP.host_names.list=%s\n", *(Localhost_IP.host_names.list));
printf("Localhost_IP.host_names.count=%d\n", Localhost_IP.host_names.count);
printf("Localhost_IP.host_names.max=%d\n", Localhost_IP.host_names.max);
printf("Localhost_IP.h_addrtype=%d\n", Localhost_IP.h_addrtype);
printf("Localhost_IP.h_length=%d\n", Localhost_IP.h_length);
printf("Localhost_IP.h_addr_list.list1=%s\n", *(Localhost_IP.h_addr_list.list+0));
printf("Localhost_IP.h_addr_list.list2=%s\n", *(Localhost_IP.h_addr_list.list+1));
printf("Localhost_IP.h_addr_list.list3=%s\n", *(Localhost_IP.h_addr_list.list+2));
printf("Localhost_IP.h_addr_list.list4=%s\n", *(Localhost_IP.h_addr_list.list+3));
printf("Localhost_IP.h_addr_list.count=%d\n", Localhost_IP.h_addr_list.count);
printf("Localhost_IP.h_addr_list.max=%d\n", Localhost_IP.h_addr_list.max);
#endif
}

/***************************************************************************
 * void Get_remote_hostbyaddr( struct sockaddr *sin );
 * 1. look up the address using gethostbyaddr()
 * 2. if not found, we have problems
 ***************************************************************************/
 
char *Get_hostinfo_byaddr( struct host_information *info,
	struct sockaddr *sinaddr, int addr_only )
{
	struct hostent *host_ent = 0;
	void *addr = 0;
	int len = 0; 
	char *fqdn = 0;
	char *s;
	char buffer[64];
	const char *const_s;

	DEBUG3("Get_remote_hostbyaddr: %s",
		inet_ntop_sockaddr( sinaddr, buffer, sizeof(buffer) ) );
	Clear_host_information( info );
	if( sinaddr->sa_family == AF_INET ){
		addr = &((struct sockaddr_in *)sinaddr)->sin_addr;
		len = sizeof( ((struct sockaddr_in *)sinaddr)->sin_addr );
#if defined(IPV6)
	} else if( sinaddr->sa_family == AF_INET6 ){
		addr = &((struct sockaddr_in6 *)sinaddr)->sin6_addr;
		len = sizeof( ((struct sockaddr_in6 *)sinaddr)->sin6_addr );
#endif
	} else {
		FATAL(LOG_ERR) "Get_remote_hostbyaddr: bad family '%d'",
			sinaddr->sa_family);
	}
	if( !addr_only ){
		host_ent = gethostbyaddr( addr, len, sinaddr->sa_family );
	}
	if( host_ent ){
		fqdn = Fixup_fqdn( host_ent->h_name, info, host_ent );
	} else {
		/* We will need to create a dummy record. - no host */
		info->h_addrtype = sinaddr->sa_family;
		info->h_length = len;
		s = malloc_or_die( len,__FILE__,__LINE__ );
		memcpy( s, addr, len );
		Check_max( &info->h_addr_list, 2);
		info->h_addr_list.list[info->h_addr_list.count++] = s;
		info->h_addr_list.list[info->h_addr_list.count] = 0;

		const_s = inet_ntop_sockaddr( sinaddr, buffer, sizeof(buffer) );
		fqdn = info->fqdn = safestrdup(const_s,__FILE__,__LINE__);
		info->shorthost = safestrdup(fqdn,__FILE__,__LINE__);
		Add_line_list( &info->host_names,info->fqdn,0,0,0);
	}
	return( fqdn );
}

char *Get_remote_hostbyaddr( struct host_information *info,
	struct sockaddr *sinaddr, int force_ip_addr_use )
{
	char *fqdn;
	fqdn = Get_hostinfo_byaddr( info, sinaddr, force_ip_addr_use );
	DEBUG3("Get_remote_hostbyaddr: %s", fqdn );
	Set_DYN( &FQDNRemote_FQDN, info->fqdn );
	Set_DYN( &ShortRemote_FQDN, info->shorthost );
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4) Dump_host_information( "Get_remote_hostbyaddr", info );
#endif
	return( fqdn );
}

/***************************************************************************
 * int Same_host( struct host_information *h1, *h2 )
 *  returns 1 on failure, 0 on success
 *  - compares the host addresses for an identical one
 ***************************************************************************/
int Same_host( struct host_information *host,
	struct host_information *remote )
{
	int i, j;
	char **hl1, **hl2;
	unsigned char *h1, *h2 ;
	int c1, c2, l1, l2;
	int result = 1;

	if( host && remote ){
		hl1 = host->h_addr_list.list;
		c1 = host->h_addr_list.count;
		l1 = host->h_length;
		hl2 = remote->h_addr_list.list;
		c2 = remote->h_addr_list.count;
		l2 = remote->h_length;
		if( l1 == l2 ){ 
			for( i = 0; result && i < c1; ++i ){
				h1 = (unsigned char *)(hl1[i]);
				for( j = 0; result && j < c2; ++j ){
					h2 = (unsigned char *)(hl2[j]);
					result = memcmp( h1, h2, l1 );
					if(DEBUGL4){
						char ls[64], rs[64];
						int n;
						ls[0] = 0; rs[0] = 0;
						for( n = 0; n < l1; ++n ){
							SNPRINTF( ls + safestrlen(ls), 3) "%02x", h1[n] );
						}
						for( n = 0; n < l1; ++n ){
							SNPRINTF( rs + safestrlen(rs), 3) "%02x", h2[n] );
						}
						LOGDEBUG("Same_host: comparing %s to %s, result %d",
							ls, rs, result );
					}
				}
			}
		}
	}
	return( result != 0 );
}

#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * Dump_host_information( char *title, struct host_information *info )
 * Dump file information
 ***************************************************************************/

void Dump_host_information( char *title,  struct host_information *info )
{
	int i, j;
	char **list;
	char *s;
	if( title ) LOGDEBUG( "*** %s (0x%lx) ***", title, Cast_ptr_to_long(info) );
	if( info ){
		LOGDEBUG( "  info name count %d", info->host_names.count );
		list = info->host_names.list;
		for( i = 0; i < info->host_names.count; ++i ){
			LOGDEBUG( "    [%d] '%s'", i, list[i] );
		}
		LOGDEBUG( "  address type %d, length %d count %d",
				info->h_addrtype, info->h_length,
				info->h_addr_list.count );
		for( i = 0; i < info->h_addr_list.count; ++i ){
			char msg[64];
			int len;
			SNPRINTF( msg, sizeof(msg)) "    [%d] 0x", i );
			s = info->h_addr_list.list[i];
			for( j = 0; j < info->h_length; ++j ){
				len = safestrlen( msg );
				SNPRINTF( msg+len, sizeof(msg)-len) "%02x",((unsigned char *)s)[j] );
			}
			LOGDEBUG( "%s", msg );
		}
	}
}
#endif

/***************************************************************************
 * void form_addr_and_mask( char *v, *addr, *mask, int addrlen, int family)
 *		form address and mask from string
 *      with the format:  IPADDR/MASK, mask is x.x.x.x or n (length)
 ***************************************************************************/
void form_addr_and_mask(char *v, char *addr,char *mask,
	int addrlen, int family )
{
	int result = 1;
	char *s, *t;
	int i, m, bytecount, bitcount;
	char buffer[SMALLBUFFER];

	if( v == 0 ) return;
	
	DEBUG5("form_addr_and_mask: '%s'", v );
	if( 4*addrlen+1 >= (int)(sizeof(buffer)) ){
		FATAL(LOG_ERR)"form_addr_and_mask: addrlen too large - hacker attack?");
	}
	memset( addr, 0, addrlen );
	memset( mask, ~0, addrlen );
	if( (s = safestrchr( v, '/' )) ) *s = 0;
	inet_pton(family, v, addr );
	if( s ){
		*s++ = '/';
		t = 0;
		m = strtol( s, &t, 0 );
		if( t == 0 || *t ){
			result = inet_pton(family, s, mask );
		} else if( m >= 0 ){
			memset( mask, 0, addrlen );
			bytecount = m/8;
			bitcount = m & 0x7;
			DEBUG6("form_addr_and_mask: m '%s' %d, bytecount %d, bitcount %d",
				s, m, bytecount, bitcount );
			if( bytecount >= addrlen){
				bytecount = addrlen;
				bitcount = 0;
			}
			t = buffer;
			*t = 0;
			for( i = 0; i < bytecount; ++i ){
				if( buffer[0] ) *t++ = '.';
				strcpy(t,"255");
				t += safestrlen(t);
			}
			if( bitcount && i < addrlen ){
				if( buffer[0] ) *t++ = '.';
				SNPRINTF(t,6) "%d", (~((1<<(8-bitcount))-1))&0xFF);
				t += safestrlen(t);
				++i;
			}
			for( ; i < addrlen; ++i ){
				if( buffer[0] ) *t++ = '.';
				strcpy(t,"0");
				t += safestrlen(t);
			}
			DEBUG6("form_addr_and_mask: len %d '%s'", m, buffer );
			result = inet_pton(family, buffer, mask );
		}
	}
	if(DEBUGL5){
		LOGDEBUG("form_addr_and_mask: addr '%s'",
			inet_ntop( family, addr, buffer, sizeof(buffer) ) );
		LOGDEBUG("form_addr_and_mask: mask '%s'",
			inet_ntop( family, mask, buffer, sizeof(buffer) ) );
	}
}

/*
 * cmp_ip_addr()
 * do a masked string compare
 */

int cmp_ip_addr( char *h, char *a, char *m, int len )
{
    int match = 0, i;

	if( len == 0 ) match = 1;
    for( i = 0; match == 0 && i < len; ++i ){
        DEBUG5("cmp_ip_addr: [%d] mask 0x%02x addr 0x%02x host 0x%02x",
            i, ((unsigned char *)m)[i],
			((unsigned char *)a)[i],
			((unsigned char *)h)[i] );
        match = (m[i] & ( a[i] ^ h[i] )) & 0xFF;
    }
    DEBUG5("cmp_ip_addr: result %d", match );
    return( match );
}


/*
 * int Match_ipaddr_value( char *str, struct host_information *host )
 *  str has format addr,addr,addr
 *   addr is @netgroup
 *           *globmatch* to the host FQDN or alias names
 *           nn.nn.nn/mak
 * Match the indicated address against the host
 *
 *  returns: 0 if match
 *           1 if no match
 */

int Match_ipaddr_value( struct line_list *list, struct host_information *host )
{
	int result = 1, i, j, invert = 0;
	char *str, *addr, *mask;

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGF(DDB1)("Match_ipaddr_value: host %s", host?host->fqdn:0 );
	DEBUGFC(DDB1)Dump_host_information("Match_ipaddr_value - host ", host );
#endif
	if( host == 0 || host->fqdn == 0 ) return(result);
	addr = malloc_or_die(host->h_length,__FILE__,__LINE__);
	mask = malloc_or_die(host->h_length,__FILE__,__LINE__);
	for( i = 0;  result && i < list->count; ++i ){
		if( !(str = list->list[i]) ) continue;
		if( cval(str) == '!' ){
			invert = 1;
			++str;
		}
		if( cval(str) == '@' ) {	/* look up host in netgroup */
#ifdef HAVE_INNETGR
			result = !innetgr( str+1, host->shorthost, NULL, NULL );
			if( result ) result = !innetgr( str+1, host->fqdn, NULL, NULL );
#else /* HAVE_INNETGR */
			DEBUGF(DDB3)("match: no innetgr() call, netgroups not permitted");
#endif /* HAVE_INNETGR */
		} else if( str[0] == '<' && str[1] == '/' ){
			struct line_list users;
			Init_line_list(&users);
			Get_file_image_and_split(str+1,0,0,&users,Whitespace,
				0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGFC(DDB3)Dump_line_list("Match_ipaddr_value- file contents'", &users );
#endif
			result = Match_ipaddr_value( &users,host);
			Free_line_list(&users);
		} else {
			lowercase(str);
			for( j = 0; result && j < host->host_names.count; ++j ){
				lowercase(host->host_names.list[j]);
				result = Globmatch( str, host->host_names.list[j] );
			}
			if( result ){
				DEBUGF(DDB2)("Match_ipaddr_value: mask '%s'",
					str );
				form_addr_and_mask(str,addr,mask,host->h_length,
					host->h_addrtype );
				for( j = 0; result && j < host->h_addr_list.count; ++j ){
					char *v;
					v = host->h_addr_list.list[j];
					result = cmp_ip_addr( v, addr, mask, host->h_length );
				}
			}
		DEBUGF(DDB2)("Match_ipaddr_value: checked '%s', result %d",
			str, result);
		}
		if( invert ) result = !result;
	}
	DEBUGF(DDB2)("Match_ipaddr_value: result %d", result );
	if(addr) free(addr); addr = 0;
	if(mask) free(mask); mask = 0;
	return( result );
}

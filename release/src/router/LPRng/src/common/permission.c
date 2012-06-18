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
"$Id: permission.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "fileopen.h"
#include "globmatch.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "permission.h"
#include "linksupport.h"

#undef HAVE_INNETGR

/**** ENDINCLUDE ****/
 struct keywords permwords[] = {

{"ACCEPT", 0, P_ACCEPT,0,0,0,0},
{"AUTH", 0, P_AUTH,0,0,0,0},
{"AUTHFROM", 0, P_AUTHFROM,0,0,0,0},
{"AUTHJOB", 0, P_AUTHJOB,0,0,0,0},
{"AUTHSAMEUSER", 0, P_AUTHSAMEUSER,0,0,0,0},
{"AUTHTYPE", 0, P_AUTHTYPE,0,0,0,0},
{"AUTHUSER", 0, P_AUTHUSER,0,0,0,0},
{"CONTROLLINE", 0, P_CONTROLLINE,0,0,0,0},
{"DEFAULT", 0, P_DEFAULT,0,0,0,0},
{"FORWARD", 0, P_FORWARD,0,0,0,0},
{"GROUP", 0, P_GROUP,0,0,0,0},
{"HOST", 0, P_HOST,0,0,0,0},
{"IFIP", 0, P_IFIP,0,0,0,0},
{"IP", 0, P_IP,0,0,0,0},
{"LPC", 0, P_LPC,0,0,0,0},
{"NOMATCHFOUND", 0, 0,0,0,0,0},
{"NOT", 0, P_NOT,0,0,0,0},
{"PORT", 0, P_PORT,0,0,0,0},
{"PRINTER", 0, P_PRINTER,0,0,0,0},
{"REJECT", 0, P_REJECT,0,0,0,0},
{"REMOTEGROUP", 0, P_REMOTEGROUP,0,0,0,0},
{"REMOTEHOST", 0, P_REMOTEHOST,0,0,0,0},
{"REMOTEIP", 0, P_REMOTEIP,0,0,0,0},
{"REMOTEPORT", 0, P_REMOTEPORT,0,0,0,0},
{"REMOTEUSER", 0, P_REMOTEUSER,0,0,0,0},
{"SAMEHOST", 0, P_SAMEHOST,0,0,0,0},
{"SAMEUSER", 0, P_SAMEUSER,0,0,0,0},
{"SERVER", 0, P_SERVER,0,0,0,0},
{"SERVICE", 0, P_SERVICE,0,0,0,0},
{"USER", 0, P_USER,0,0,0,0},
{"UNIXSOCKET", 0, P_UNIXSOCKET,0,0,0,0},
{"AUTHCA", 0, P_AUTHCA,0,0,0,0},

{0,0,0,0,0,0,0}
};

char *perm_str( int n )
{
	return(Get_keystr(n,permwords));
}
int perm_val( char *s )
{
	if( !s )return(0);
	if( safestrlen(s) == 1 && isupper(cval(s)) ){
		return( P_CONTROLLINE );
	}
	return(Get_keyval(s,permwords));
}


/***************************************************************************
 * Perms_check( struct line_list *perms, struct perm_check );
 * - run down the list of permissions
 * - do the check on each of them
 * - if you get a distinct fail or success, return
 * 1. the P_NOT field inverts the result of the next test
 * 2. if one test fails,  then we go to the next line
 * 3. The entire set of tests is accepted if all pass, i.e. none fail
 ***************************************************************************/

int Perms_check( struct line_list *perms, struct perm_check *check,
	struct job *job, int job_check )
{
	int j, c, linecount, valuecount, key;
	int invert = 0;
	int result = 0, m = 0;
	struct line_list values, args;
	char *s, *t;					/* string */
	int last_default_perm;
	char buffer[4];

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DDB1)Dump_perm_check( "Perms_check - checking", check );
	DEBUGFC(DDB1)Dump_line_list( "Perms_check - permissions", perms );
#endif
	Init_line_list(&values);
	Init_line_list(&args);
	last_default_perm = perm_val( Default_permission_DYN );
	DEBUGF(DDB1)("Perms_check: last_default_perm '%s', Default_perm '%s'",
		perm_str( last_default_perm ), Default_permission_DYN );
	if( check == 0 || perms == 0 ){
		return( last_default_perm );
	}
	for( linecount = 0; result == 0 && linecount < perms->count; ++linecount ){
		DEBUGF(DDB2)("Perms_check: line [%d]='%s'", linecount,
			perms->list[linecount]);
		Free_line_list(&values);
		Split(&values,perms->list[linecount],Whitespace,0,0,0,0,0,0);
		if( values.count == 0 ) continue;
		result = 0; m = 0; invert = 0;
		for( valuecount = 0; m == 0 && valuecount < values.count;
				++valuecount ){
			DEBUGF(DDB2)("Perms_check: [%d]='%s'",valuecount,
				values.list[valuecount] );
			Free_line_list(&args);
			Split(&args,values.list[valuecount],Perm_sep,0,0,0,0,0,0);
			if( args.count == 0 ) continue;
			if( invert > 0 ){
				invert = -1;
			} else {
				invert = 0;
			}
			key = perm_val( args.list[0] );
			if( key == 0 ){
				m = 1;
				break;
			}
			/* we remove the key entry */
			Remove_line_list( &args, 0 );
			DEBUGF(DDB2)("Perms_check: before doing %s, result %d, %s",
				perm_str(key), result, perm_str(result) );
			switch( key ){
			case P_NOT:
				invert = 1;
				continue;
			case P_REJECT: result = P_REJECT; m = 0; break;
			case P_ACCEPT: result = P_ACCEPT; m = 0; break;
			case P_USER:
				m = 1;
				if( !job_check ){ m = 0; }
				else switch (check->service){
				case 'X': break;
				default:
					m = match( &args, check->user, invert );
					break;
				}
				break;
			case P_LPC:
				m = 1;
				switch (check->service){
				case 'X': break;
				case 'C':
					m = match( &args, check->lpc, invert );
					break;
				}
				break;

			case P_IP:
			case P_HOST:
				m = 1;
				if( !job_check ){ m = 0; }
				else switch (check->service){
				case 'X': break;
				default:
					m = match_host( &args, check->host, invert );
					break;
				}
				break;

			case P_GROUP:
				m = 1;
				if( !job_check ){ m = 0; }
				else switch (check->service){
				case 'X': break;
				default:
					m = match_group( &args, check->user, invert );
					break;
				}
				break;

			case P_REMOTEPORT:
			case P_PORT:
				m = 1;
				switch (check->service){
				case 'X': case 'M': case 'C':
					m = match_range( &args, check->port, invert );
					break;
				}
				break;
			case P_REMOTEUSER:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					m = match( &args, check->remoteuser, invert );
					break;
				}
				break;
			case P_REMOTEGROUP:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					m = match_group( &args, check->remoteuser, invert );
					break;
				}
				break;

			case P_IFIP:
			case P_REMOTEHOST:
			case P_REMOTEIP:
				m = match_host( &args, check->remotehost, invert );
				break;

			case P_AUTH:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					DEBUGF(DDB3)(
						"Perms_check: P_AUTH authuser '%s'", check->authuser );
					m = !check->authuser;
					if( invert ) m = !m;
				}
				break;

			case P_AUTHTYPE:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					DEBUGF(DDB3)(
						"Perms_check: P_AUTHTYPE authtype '%s'", check->authtype );
					m = match( &args, check->authtype, invert );
				}
				break;

			case P_AUTHFROM:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					m = match( &args, check->authfrom, invert );
				}
				break;

			case P_AUTHCA:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					m = match( &args, check->authca, invert );
				}
				break;

			case P_AUTHUSER:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					m = match( &args, check->authuser, invert );
				}
				break;

			case P_CONTROLLINE:
				/* check to see if we have control line */
				m = 1;
				if( !job_check ){ m = 0; }
				for( j = 0; m && j < args.count; ++j ){
					if( !(t = args.list[j]) ) continue;
					c = cval(t);
					buffer[1] = 0; buffer[0] = c;
					if( isupper(c) && (s = Find_str_value(&job->info,buffer,0))){
						/* we do a glob match against line */
						m = Globmatch( t+1, s );
					}
				}
				if( invert ) m = !m;
				break;
			case P_PRINTER:
				m = 1;
				switch (check->service){
				case 'X': break;
				default:
					m = match( &args, check->printer, invert );
					break;
				}
				break;
			case P_SERVICE:
				m = match_char( &args, check->service, invert );
				break;
			case P_FORWARD:
			case P_SAMEHOST:
				m = 1;
				if( !job_check ){ m = 0; }
				else switch (check->service){
				default:
					/* P_SAMEHOST check succeeds if P_REMOTEIP == P_IP */
					m = Same_host(check->host, check->remotehost);
					if( m ){
					/* check to see if both remote and local are server */
					int r, h;
					r = Same_host(check->remotehost,&Host_IP);
					if( r ) r = Same_host(check->remotehost,&Localhost_IP);
					h = Same_host(check->host,&Host_IP);
					if( h ) h = Same_host(check->host,&Localhost_IP);
					DEBUGF(DDB3)(
						"Perms_check: P_SAMEHOST server name check r=%d,h=%d",
						r, h );
					if( h == 0 && r == 0 ){
						m = 0;
					}
					}
					if( invert ) m = !m;
					break;
				}
				if( key == P_FORWARD ) m = !m;
				break;
			case P_SAMEUSER:
				m = 1;
				if( !job_check ){ m = 0; }
				else switch (check->service){
				default: break;
				case 'Q': case 'M': case 'C':
					/* check succeeds if remoteuser == user */
					m = (safestrcmp( check->user, check->remoteuser ) != 0);
					if( invert ) m = !m;
					DEBUGF(DDB3)(
					"Perms_check: P_SAMEUSER '%s' == remote '%s', rslt %d",
					check->user, check->remoteuser, m );
					break;
				}
				break;

			case P_AUTHSAMEUSER:
				m = 1;
				if( !job_check ){ m = 0; }
				else switch (check->service){
				default: break;
				case 'Q': case 'M': case 'C':
					/* check succeeds if remoteuser == user */
					t = Find_str_value(&job->info,AUTHUSER,Value_sep);
					m = (safestrcmp( check->authuser, t ) != 0);
					if( invert ) m = !m;
					DEBUGF(DDB3)(
					"Perms_check: P_AUTHSAMEUSER job authinfo '%s' == auth_id '%s', rslt %d",
					t, check->authuser, m );
					break;
				}
				break;

			case P_AUTHJOB:
				m = 1;
				switch (check->service){
				default: break;
				case 'Q': case 'M': case 'C':
					/* check succeeds if authinfo present */
					t = Find_str_value(&job->info,AUTHUSER,Value_sep);
					m = !t;
					if( invert ) m = !m;
					DEBUGF(DDB3)(
					"Perms_check: P_AUTHJOB job authinfo '%s', rslt %d", t, m );
					break;
				}
				break;

			case P_SERVER:
				m = 1;
				/* check succeeds if remote P_IP and server P_IP == P_IP */
				m = Same_host(check->remotehost,&Host_IP);
				if( m ) m = Same_host(check->remotehost,&Localhost_IP);
				if( invert ) m = !m;
				break;

			case P_UNIXSOCKET:
				m = 1;
				/* check succeeds if connection via unix socket */
				m = !check->unix_socket;
				if( invert ) m = !m;
				break;

			case P_DEFAULT:
				
				DEBUGF(DDB3)("Perms_check: DEFAULT - %d, values.count %d",
					valuecount, values.count );
				m = 1;
				if( values.count == 2 ){
					switch( perm_val( values.list[1]) ){
					case P_REJECT: last_default_perm =  P_REJECT; break;
					case P_ACCEPT: last_default_perm =  P_ACCEPT; break;
					}
				}
			}
			DEBUGF(DDB2)("Perms_check: match %d, result '%s' default now '%s'",
				m, perm_str(result), perm_str(last_default_perm) );
		}
		if( m ){
			result = 0;
		} else if( result == 0 ){
			result = last_default_perm;
		}
		DEBUGF(DDB1)("Perms_check: '%s' - match %d, result '%s' default now '%s'",
			perms->list[linecount],
			m, perm_str(result), perm_str(last_default_perm) );
	}
	if( result == 0 ){
		result = last_default_perm;
	}
	DEBUGF(DDB1)("Perms_check: final result %d '%s'",
				result, perm_str( result ) );
	Free_line_list(&values);
	Free_line_list(&args);

	return( result );
}


/***************************************************************************
 * static int match( char **val, char *str );
 *  returns 1 on failure, 0 on success
 *  - match the string against the list of options
 *    options are glob type regular expressions;  we implement this
 *    currently using the most crude of pattern matching
 *  - if string is null or pattern list is null, then match fails
 *    if both are null, then match succeeds
 ***************************************************************************/

int match( struct line_list *list, const char *str, int invert )
{
 	int result = 1, i, c;
	char *s;
 	DEBUGF(DDB3)("match: str '%s', invert %d", str, invert );
 	if(str)for( i = 0; result && i < list->count; ++i ){
		if( !(s = list->list[i])) continue;
		DEBUGF(DDB3)("match: str '%s' to '%s'", str, s );
 		/* now do the match */
		c = cval(s);
		if( c == '@' ) {	/* look up host in netgroup */
#ifdef HAVE_INNETGR
			result = !innetgr( s+1, (char *)str, 0, 0 );
#else /* HAVE_INNETGR */
			DEBUGF(DDB3)("match: no innetgr() call, netgroups not permitted");
#endif /* HAVE_INNETGR */
		} else if( c == '<' && cval(s+1) == '/' ){
			struct line_list users;
			Init_line_list(&users);
			Get_file_image_and_split(s+1,0,0,&users,Whitespace,
				0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGFC(DDB3)Dump_line_list("match- file contents'", &users );
#endif
			result = match( &users,str,0);
			Free_line_list(&users);
		} else {
	 		result = Globmatch( s, str );
		}
		DEBUGF(DDB3)("match: list[%d]='%s', result %d", i, s,  result );
	}
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match: str '%s' final result %d", str, result );
	return( result );
}

/***************************************************************************
 * static int match_host( char **list, char *host );
 *  returns 1 on failure, 0 on success
 *  - match the hostname/printer strings against the list of options
 *    options are glob type regular expressions;  we implement this
 *    currently using the most crude of pattern matching
 *  - if string is null or pattern list is null, then match fails
 *    if both are null, then match succeeds
 ***************************************************************************/

int match_host( struct line_list *list, struct host_information *host,
	int invert )
{
 	int result = Match_ipaddr_value(list,host);
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match_host: host '%s' final result %d", host?host->fqdn:0,
		result );
	return( result );
}
/***************************************************************************
 * static int match_range( char **list, int port );
 * check the port number and/or range
 * entry has the format:  number     number-number
 ***************************************************************************/

int portmatch( char *val, int port )
{
	int low, high, err;
	char *end;
	int result = 1;
	char *s, *t, *tend;

	err = 0;
	s = safestrchr( val, '-' );
	if( s ){
		*s = 0;
	}
	end = val;
	low = strtol( val, &end, 10 );
	if( end == val || *end ) err = 1;

	high = low;
	if( s ){
		tend = t = s+1;
		high = strtol( t, &tend, 10 );
		if( t == tend || *tend ) err = 1;
		*s = '-';
	}
	if( err ){
		LOGMSG( LOG_ERR) "portmatch: bad port range '%s'", val );
	}
	if( high < low ){
		err = high;
		high = low;
		low = err;
	}
	result = !( port >= low && port <= high );
	DEBUGF(DDB3)("portmatch: low %d, high %d, port %d, result %d",
		low, high, port, result );
	return( result );
}

int match_range( struct line_list *list, int port, int invert )
{
	int result = 1;
	int i;
	char *s;

	DEBUGF(DDB3)("match_range: port '0x%x'", port );
	for( i = 0; result && i < list->count; ++i ){
		/* now do the match */
		if( !(s = list->list[i]) ) continue;
		result = portmatch( s, port );
	}
	if( invert ) result = !result;
	DEBUGF(DDB3)("match_range: port '%d' result %d", port, result );
	return( result );
}

/***************************************************************************
 * static int match_char( char **list, int value );
 * check for the character value in one of the option strings
 * entry has the format:  string
 ***************************************************************************/

int match_char( struct line_list *list, int value, int invert )
{
	int result = 1;
	int i;
	char *s;

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGF(DDB3)("match_char: value '0x%x' '%c'", value, value );
	DEBUGFC(DDB3)Dump_line_list("match_char - lines", list );
#endif
	for( i = 0; result && i < list->count; ++i ){
		if( !(s = list->list[i]) ) continue;
		result = (safestrchr( s, value ) == 0) && (safestrchr(s,'*') == 0) ;
		DEBUGF(DDB3)("match_char: val %c, str '%s', match %d",
			value, s, result);
	}
	if( invert ) result = !result;
	DEBUGF(DDB3)("match_char: value '%c' result %d", value, result );
	return( result );
}


/***************************************************************************
 * static int match_group( char **list, char *str );
 *  returns 1 on failure, 0 on success
 *  - get the UID for the named user
 *  - scan the listed groups to see if there is a group
 *    check to see if user is in group
 ***************************************************************************/

int match_group( struct line_list *list, const char *str, int invert )
{
 	int result = 1;
 	int i;
	char *s;

 	DEBUGF(DDB3)("match_group: str '%s'", str );
 	for( i = 0; str && result && i < list->count; ++i ){
 		/* now do the match */
		if( !(s = list->list[i]) ) continue;
 		result = ingroup( s, str );
	}
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match: str '%s' value %d", str, result );
	return( result );
}

/***************************************************************************
 * static int ingroup( char* *group, char *user );
 *  returns 1 on failure, 0 on success
 *  scan group for user name
 * Note: we first check for the group.  If there is none, we check for
 *  wildcard (*) in group name, and then scan only if we need to
 ***************************************************************************/

int ingroup( char *group, const char *user )
{
	struct group *grent;
	struct passwd *pwent;
	char **members;
	int result = 1;

	DEBUGF(DDB3)("ingroup: checking '%s' for membership in group '%s'", user, group);
	if( group == 0 || user == 0 ){
		return( result );
	}
	/* first try getgrnam, see if it is a group */
	pwent = getpwnam(user);
	if( group[0] == '@' ) {	/* look up user in netgroup */
#ifdef HAVE_INNETGR
		if( !innetgr( group+1, 0, (char *)user, 0 ) ) {
			DEBUGF(DDB3)( "ingroup: user %s P_NOT in netgroup %s", user, group+1 );
		} else {
			DEBUGF(DDB3)( "ingroup: user %s in netgroup %s", user, group+1 );
			result = 0;
		}
#else /* HAVE_INNETGR */
		DEBUGF(DDB3)( "ingroup: no innetgr() call, netgroups not permitted" );
#endif /* HAVE_INNETGR */
	} else if( group[0] == '<' && group[1] == '/' ){
		struct line_list users;
		Init_line_list(&users);
		Get_file_image_and_split(group+1,0,0,&users,Whitespace,
			0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DDB3)Dump_line_list("match- file contents'", &users );
#endif
		result = match_group( &users,user,0);
		Free_line_list(&users);
	} else if( (grent = getgrnam( group )) ){
		DEBUGF(DDB3)("ingroup: group id: %d\n", grent->gr_gid);
		if( pwent && ((int)pwent->pw_gid == (int)grent->gr_gid) ){
			DEBUGF(DDB3)("ingroup: user default group id: %d\n", pwent->pw_gid);
			result = 0;
		} else for( members = grent->gr_mem; result && *members; ++members ){
			DEBUGF(DDB3)("ingroup: member '%s'", *members);
			result = (safestrcmp( user, *members ) != 0);
		}
	} else if( safestrpbrk( group, "*[]") ){
		/* wildcard in group name, scan through all groups */
		setgrent();
		while( result && (grent = getgrent()) ){
			DEBUGF(DDB3)("ingroup: group name '%s'", grent->gr_name);
			/* now do match against group */
			if( Globmatch( group, grent->gr_name ) == 0 ){
				if( pwent && ((int)pwent->pw_gid == (int)grent->gr_gid) ){
					DEBUGF(DDB3)("ingroup: user default group id: %d\n",
					pwent->pw_gid);
					result = 0;
				} else {
					DEBUGF(DDB3)("ingroup: found '%s'", grent->gr_name);
					for( members = grent->gr_mem; result && *members; ++members ){
						DEBUGF(DDB3)("ingroup: member '%s'", *members);
						result = (safestrcmp( user, *members ) != 0);
					}
				}
			}
		}
		endgrent();
	}
	DEBUGF(DDB3)("ingroup: result: %d", result );
	return( result );
}

#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * Dump_perm_check( char *title, struct perm_check *check )
 * Dump perm_check information
 ***************************************************************************/

void Dump_perm_check( char *title,  struct perm_check *check )
{
	char buffer[SMALLBUFFER];
	if( title ) LOGDEBUG( "*** perm_check %s ***", title );
	buffer[0] = 0;
	if( check ){
		LOGDEBUG(
		"  user '%s', rmtuser '%s', printer '%s', service '%c', lpc '%s'",
		check->user, check->remoteuser, check->printer, check->service, check->lpc );
#ifdef ORIGINAL_DEBUG//JY@1020
		Dump_host_information( "  host", check->host );
		Dump_host_information( "  remotehost", check->remotehost );
#endif
/*
		LOGDEBUG( "  ip '%s' port %d, unix_socket %d",
			inet_ntop_sockaddr( &check->addr, buffer, sizeof(buffer)),
			check->port, check->unix_socket );
*/
		LOGDEBUG( "  port %d, unix_socket %d",
			check->port, check->unix_socket );
		LOGDEBUG( " authtype '%s', authfrom '%s', authuser '%s', authca '%s'",
			check->authtype, check->authfrom, check->authuser, check->authca );
	}
}
#endif

/***************************************************************************
 * Perm_check_to_list( struct line_list *list, struct perm_check *check )
 *  Put perm_check information in list
 ***************************************************************************/

void Perm_check_to_list( struct line_list *list, struct perm_check *check )
{
	char buffer[SMALLBUFFER];
	Set_str_value( list, USER, check->user );
	Set_str_value( list, REMOTEUSER, check->remoteuser );
	Set_str_value( list, PRINTER, check->printer );
	SNPRINTF(buffer,sizeof(buffer))"%c",check->service);
	Set_str_value( list, SERVICE, buffer );
	Set_str_value( list, LPC, check->lpc );
	if( check->host ){
		Set_str_value( list, HOST, check->host->fqdn );
	}
	if( check->remotehost ){
		Set_str_value( list, HOST, check->remotehost->fqdn );
	}
	Set_decimal_value( list, PORT, check->port );
	Set_str_value( list, AUTHTYPE, check->authtype );
	Set_str_value( list, AUTHFROM, check->authfrom );
	Set_str_value( list, AUTHUSER, check->authuser );
	Set_str_value( list, AUTHCA, check->authca );
}

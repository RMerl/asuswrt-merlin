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
"$Id: lpstat.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpstat.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *   lpstat  [-d]  [-r]  [-R]  [-s]  [-t]  [-a  [list
 *  ]  ]  [-c  [list] ]  [-f  [list]  [-l] ]  [-o  [
 *   list] ]  [-p   [list]  [-D]  [-l] ]  [-S  [list
 *  ]   [-l] ]  [-u  [login- ID -list ] ]  [-v  [list]
 *  ]
 *
 *    lpstat 
 * DESCRIPTION
 *   lpstat sends a status request to lpd(8)
 * See lpstat for details on the way that status is gathered.
 *   The lpstat version simulates the lpstat operation,
 * using the information returned by LPRng in response to a verbose query.
 * 
 ****************************************************************************
 *
 */

#include "lp.h"

#include "child.h"
#include "getopt.h"
#include "getprinter.h"
#include "initialize.h"
#include "linksupport.h"
#include "patchlevel.h"
#include "sendreq.h"

/**** ENDINCLUDE ****/


#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lpstat.h"
/**** ENDINCLUDE ****/

 int A_flag, P_flag, R_flag, S_flag, a_flag, c_flag, d_flag, f_flag, l_flag, n_flag,
	o_flag, p_flag, r_flag, s_flag, t_flag, u_flag, v_flag, flag_count,
	D_flag, Found_flag;
 char *S_val, *a_val, *c_val, *f_val, *o_val, *p_val, *u_val, *v_val;
 struct line_list S_list, f_list;

 struct line_list Lpq_options;
 int Rawformat;

#define MAX_SHORT_STATUS 6

/***************************************************************************
 * main()
 * - top level of LPQ
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	int i;
	struct line_list l, options, request_list;
	char msg[SMALLBUFFER], *s;

	Init_line_list(&l);
	Init_line_list(&options);
	Init_line_list(&request_list);

	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);
	(void) signal(SIGCHLD, SIG_DFL);
	(void) signal(SIGPIPE, SIG_IGN);

	/*
	 * set up the user state
	 */

	Status_line_count = 1;

#ifndef NODEBUG
	Debug = 0;
#endif

	Displayformat = REQ_DLONG;

	Initialize(argc, argv, envp, 'T' );
	Setup_configuration();
	Get_parms(argc, argv );      /* scan input args */
	if( A_flag && !getenv( "AUTH" ) ){
		FPRINTF(STDERR,"lpstat: requested authenticated transfer (-A) and AUTH environment variable not set");
		usage();
	}

	/* set up configuration */
	Get_printer();
	Fix_Rm_Rp_info(0,0);
	Get_all_printcap_entries();

	/* check on printing scheduler is running */
	if( t_flag ){
		All_printers = 1;
		r_flag = d_flag = p_flag = o_flag = 1;
		s_flag = 0;
	}
	if( s_flag ){
		/* a_flag = 1; */
		r_flag = 1;
		d_flag = 1;
		v_flag = 1;
		All_printers = 1;
	}

	if( All_printers ){
		Merge_line_list( &request_list, &All_line_list,0,0,0);
	}
	Merge_line_list( &request_list, &Printer_list,0,0,0);
	Check_max(&options,2);
	if( options.count ){
		for( i = options.count; i > 0 ; --i ){
			options.list[i] = options.list[i-1];
		}
		options.list[0] = safestrdup(Logname_DYN,__FILE__,__LINE__);
		++options.count;
	}
	options.list[options.count] = 0;

	if( Found_flag == 0 ){
		if( request_list.count == 0 ){
			Split(&request_list,Printer_DYN,", ",1,0,1,1,0,0);
		}
		o_flag = 1;
		flag_count = 1;
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL1)Dump_line_list("lpstat - printer request list", &request_list);
	if(DEBUGL1)Dump_line_list("lpstat - options", &options);
#endif

	if( r_flag ){
		Write_fd_str(1,"scheduler is running\n");
	}
	if( d_flag ){
		if( Printer_DYN == 0 ){
			Write_fd_str(1,"no system default destination\n");
		} else {
			SNPRINTF(msg,sizeof(msg))
				"system default destination: %s\n", Printer_DYN);
			Write_fd_str(1,msg);
		}
	}
	if( v_flag ){
		for( i = 0; i < request_list.count; ++i ){
			Set_DYN(&Printer_DYN,request_list.list[i] );
			Fix_Rm_Rp_info(0,0);
			SNPRINTF(msg,sizeof(msg)) "system for %s: %s\n", Printer_DYN, RemoteHost_DYN);
			Write_fd_str(1,msg);
		}
	}

	/* see if additional status required */

	Free_line_list( &Printer_list );

	for( i = 0; i < request_list.count; ++i ){
		s = request_list.list[i];
		Set_DYN(&Printer_DYN,s );
		Show_status(options.list, 0);
	}

	Free_line_list( &Printer_list );
	if( flag_count ){
		for( i = 0; i < request_list.count; ++i ){
			s = request_list.list[i];
			Set_DYN(&Printer_DYN,s );
			Show_status(options.list, 1);
		}
	}

	DEBUG1("lpstat: done");
	Remove_tempfiles();
	DEBUG1("lpstat: tempfiles removed");

	Errorcode = 0;
	DEBUG1("lpstat: cleaning up");
	return(0);
}

void Show_status(char **argv, int display_format)
{
	int fd;
	char msg[LINEBUFFER];

	DEBUG1("Show_status: start");
	/* set up configuration */
	Fix_Rm_Rp_info(0,0);

	if( Check_for_rg_group( Logname_DYN ) ){
		SNPRINTF( msg, sizeof(msg))
			"  Printer: %s - cannot use printer, not in privileged group\n", Printer_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}
	if( A_flag ){
		Set_DYN(&Auth_DYN, getenv("AUTH"));
	}
	if( Direct_DYN && Lp_device_DYN ){
		SNPRINTF( msg, sizeof(msg))
			_(" Printer: %s - direct connection to device '%s'\n"),
			Printer_DYN, Lp_device_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}
	fd = Send_request( 'Q', Displayformat,
		0, Connect_timeout_DYN, Send_query_rw_timeout_DYN, 1 );
	if( fd >= 0 ){
		/* shutdown( fd, 1 ); */
		Read_status_info( RemoteHost_DYN, fd,
			1, Send_query_rw_timeout_DYN, display_format,
			Status_line_count );
		close(fd); fd = -1;
	}
	DEBUG1("Show_status: end");
}


/***************************************************************************
 *int Read_status_info( int ack, int fd, int timeout );
 * ack = ack character from remote site
 * sock  = fd to read status from
 * char *host = host we are reading from
 * int output = output fd
 *  We read the input in blocks,  split up into lines.
 *  We run the status through the SNPRINTF() routine,  which will
 *   rip out any unprintable characters.  This will prevent magic escape
 *   string attacks by users putting codes in job names, etc.
 ***************************************************************************/

int Read_status_info( char *host, int sock,
	int output, int timeout, int display_format,
	int status_line_count )
{
	int i, j, len, n, status, count, index_list, same;
	char buffer[SMALLBUFFER], header[SMALLBUFFER];
	char *s, *t;
	struct line_list l;
	int look_for_pr = 0;

	Init_line_list(&l);

	header[0] = 0;
	status = count = 0;
	/* long status - trim lines */
	DEBUG1("Read_status_info: output %d, timeout %d, dspfmt %d",
		output, timeout, display_format );
	DEBUG1("Read_status_info: status_line_count %d", status_line_count );
	buffer[0] = 0;
	do {
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUG1("Read_status_info: look_for_pr %d, in buffer already '%s'", look_for_pr, buffer );
		if( DEBUGL2 )Dump_line_list("Read_status_info - starting list", &l );
#endif
		count = safestrlen(buffer);
		n = sizeof(buffer)-count-1;
		status = 1;
		if( n > 0 ){
			status = Link_read( host, &sock, timeout,
				buffer+count, &n );
			DEBUG1("Read_status_info: Link_read status %d, read %d", status, n );
		}
		if( status || n <= 0 ){
			status = 1;
			buffer[count] = 0;
		} else {
			buffer[count+n] = 0;
		}
		DEBUG1("Read_status_info: got '%s'", buffer );
		/* now we have to split line up */
		if( (s = safestrrchr(buffer,'\n')) ){
			*s++ = 0;
			/* add the lines */
			Split(&l,buffer,Line_ends,0,0,0,0,0,0);
			memmove(buffer,s,safestrlen(s)+1);
		}
#ifdef ORIGINAL_DEBUG//JY@1020
		if( DEBUGL2 )Dump_line_list("Read_status_info - status after splitting", &l );
#endif
		if( status ){
			if( buffer[0] ){
				Add_line_list(&l,buffer,0,0,0);
				buffer[0] = 0;
			}
			Check_max(&l,1);
			l.list[l.count++] = 0;
		}
		index_list = 0;
 again:
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUG2("Read_status_info: look_for_pr '%d'", look_for_pr );
		if( DEBUGL2 )Dump_line_list("Read_status_info - starting, Printer_list",
			&Printer_list);
#endif
		while( look_for_pr && index_list < l.count ){
			s = l.list[index_list];
			if( s == 0 || isspace(cval(s)) || !(t = strstr(s,"Printer:"))
				|| Find_exists_value(&Printer_list,t,0) ){
				++index_list;
			} else {
				DEBUG2("Read_status_info: printer found [%d] '%s', total %d", index_list, s, l.count );
				look_for_pr = 0;
			}
		}
		while( index_list < l.count && (s = l.list[index_list]) ){
			DEBUG2("Read_status_info: checking [%d] '%s', total %d", index_list, s, l.count );
			if( s && !isspace(cval(s)) && (t = strstr(s,"Printer:")) ){
				DEBUG2("Read_status_info: printer heading '%s'", t );
				if( Find_exists_value(&Printer_list,t,0) ){
					DEBUG2("Read_status_info: already done '%s'", t );
					++index_list;
					look_for_pr = 1;
					goto again;
				}
				Add_line_list(&Printer_list,t,0,1,0);
				/* parse the printer line */
				if( (t = strchr(t, ':')) ){
					++t;
					while( isspace(cval(t)) ) ++t;
					Set_DYN(&Printer_DYN,t );
					for( t = Printer_DYN; !ISNULL(t) && !isspace(cval(t)); ++t );
					if( isspace(cval(t)) ) *t = 0;
				}
				if( display_format == 0 ){
					char msg[SMALLBUFFER];
					int nospool, noprint; 
					nospool = (strstr( s, "(spooling disabled)") != 0);
					noprint = (strstr( s, "(printing disabled)") != 0);
					/* Write_fd_str( output, "ANALYZE " );
					if( Write_fd_str( output, s ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
					*/
					if( a_flag ){
						if( !nospool ){
							SNPRINTF(msg,sizeof(msg))
							"%s accepting requests since %s\n",
							Printer_DYN, Time_str(0,0) );
						} else {
							SNPRINTF(msg,sizeof(msg))
							"%s not accepting requests since %s -\n\tunknown reason\n",
							Printer_DYN, Time_str(0,0) );
						}
						if( Write_fd_str( output, msg ) < 0 ) return(1);
					}
					if( p_flag ){
						SNPRINTF(msg,sizeof(msg))
						"printer %s unknown state. %s since %s. available\n",
						Printer_DYN, noprint?"disabled":"enabled",
						Pretty_time(0));
						if( Write_fd_str( output, msg ) < 0 ) return(1);
					}
					if( p_flag && D_flag ){
						SNPRINTF(msg,sizeof(msg))
							"\tDescription: %s@%s\n",
									RemotePrinter_DYN, RemoteHost_DYN ); 
						if( Write_fd_str( output, msg ) < 0 ) return(1);
					}
				} else {
					if( Write_fd_str( output, s ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
				}
				++index_list;
				continue;
			}
			if( display_format == 0 ){
				++index_list;
				continue;
			}
			if( status_line_count > 0 ){
				/*
				 * starting at line_index, we take this as the header.
				 * then we check to see if there are at least status_line_count there.
				 * if there are,  we will increment status_line_count until we get to
				 * the end of the reading (0 string value) or end of list.
				 */
				header[0] = 0;
				strncpy( header, s, sizeof(header)-1);
				header[sizeof(header)-1] = 0;
				if( (s = strchr(header,':')) ){
					*++s = 0;
				}
				len = safestrlen(header);
				/* find the last status_line_count lines */
				same = 1;
				for( i = index_list+1; i < l.count ; ++i ){
					same = !safestrncmp(l.list[i],header,len);
					DEBUG2("Read_status_info: header '%s', len %d, to '%s', same %d",
						header, len, l.list[i], same );
					if( !same ){
						break;
					}
				}
				/* if they are all the same,  then we save for next pass */
				/* we print the i - status_line count to i - 1 lines */
				j = i - status_line_count;
				if( index_list < j ) index_list = j;
				DEBUG2("Read_status_info: header '%s', index_list %d, last %d, same %d",
					header, index_list, i, same );
				if( same ) break;
				while( index_list < i ){
					DEBUG2("Read_status_info: writing [%d] '%s'",
						index_list, l.list[index_list]);
					if( Write_fd_str( output, l.list[index_list] ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
					++index_list;
				}
			} else {
				if( Write_fd_str( output, s ) < 0
					|| Write_fd_str( output, "\n" ) < 0 ) return(1);
				++index_list;
			}
		}
		DEBUG2("Read_status_info: at end index_list %d, count %d", index_list, l.count );
		for( i = 0; i < l.count && i < index_list; ++i ){
			if( l.list[i] ) free( l.list[i] ); l.list[i] = 0;
		}
		for( i = 0; index_list < l.count ; ++i, ++index_list ){
			l.list[i] = l.list[index_list];
		}
		l.count = i;
	} while( status == 0 );
	Free_line_list(&l);
	DEBUG1("Read_status_info: done" );
	return(0);
}
int Add_val( char **var, char *val )
{
	int c = 0;
	if( val && cval(val) != '-' ){
		c = 1;
		if( *var ){
			*var = safeextend3(*var,",",val,__FILE__,__LINE__);
		} else {
			*var = safestrdup(val,__FILE__,__LINE__);
		}
	}
	return(c);
}

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/


void Get_parms(int argc, char *argv[] )
{
	int i;
	char *name, *s;

	if( argv[0] && (name = strrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	Name = name;

/*
 SYNOPSIS
     lpstat [-A] [ -d ] [ -r ] [ -R ] [ -s ] [ -t ] [ -a [list] ]
          [ -c [list] ] [ -f [list] [ -l ] ] [ -o [list] ]
          [ -p [list] [ -D] [ -l ] ] [ -P ] [ -S [list] [ -l ] ]
          [ -u [login-ID-list] ] [ -v [list] ] [-n linecount] [-Tdebug]
*/
	flag_count = 0;
	for( i = 1; i < argc; ++i ){
		s = argv[i];
		if( cval(s) == '-' ){
			Found_flag = 1;
			switch( cval(s+1) ){

			case 'd':               d_flag = 1; if(cval(s+2)) usage(); break;
			case 'D':               D_flag = 1; if(cval(s+2)) usage(); break;
			case 'r':               r_flag = 1; if(cval(s+2)) usage(); break;
			case 'A':               A_flag = 1; if(cval(s+2)) usage(); break;
			case 'R':               R_flag = 1; if(cval(s+2)) usage(); break;
			case 's':               s_flag = 1; if(cval(s+2)) usage(); break;
			case 't': ++flag_count; t_flag = 1; if(cval(s+2)) usage(); break;
			case 'l':               l_flag = 1; if(cval(s+2)) usage(); break;
			case 'n':               n_flag += 1; if(cval(s+2)) usage(); break;
			case 'N':               n_flag = -1; if(cval(s+2)) usage(); break;
			case 'P':               P_flag = 1; if(cval(s+2)) usage(); break;
			case 'a':               a_flag = 1; if( cval(s+2) ) Add_val(&a_val,s+2); else { i += Add_val(&a_val,argv[i+1]); } break;
			case 'c': ++flag_count; c_flag = 1; if( cval(s+2) ) Add_val(&c_val,s+2); else { i += Add_val(&c_val,argv[i+1]); } break;
			case 'f': ++flag_count; f_flag = 1; if( cval(s+2) ) Add_val(&f_val,s+2); else { i += Add_val(&f_val,argv[i+1]); } break;
			case 'o': ++flag_count; o_flag = 1; if( cval(s+2) ) Add_val(&o_val,s+2); else { i += Add_val(&o_val,argv[i+1]); } break;
			case 'p': ++flag_count; p_flag = 1; if( cval(s+2) ) Add_val(&p_val,s+2); else { i += Add_val(&p_val,argv[i+1]); } break;
			case 'S': ++flag_count; S_flag = 1; if( cval(s+2) ) Add_val(&S_val,s+2); else { i += Add_val(&S_val,argv[i+1]); } break;
			case 'u': ++flag_count; u_flag = 1; if( cval(s+2) ) Add_val(&u_val,s+2); else { i += Add_val(&u_val,argv[i+1]); } break;
			case 'v':               v_flag = 1; if( cval(s+2) ) Add_val(&v_val,s+2); else { i += Add_val(&v_val,argv[i+1]); } break;
			case 'V': Verbose = 1; break;
			case 'T': if(!cval(s+2)) usage(); Parse_debug( s+2, 1 ); break;
			default: usage(); break;

			}
		} else {
			break;
		}
		if(DEBUGL1){
			LOGDEBUG("A_flag %d, D_flag %d, d_flag %d, r_flag %d, R_flag %d, s_flag %d, t_flag %d, l_flag %d, P_flag %d",
				A_flag, D_flag, d_flag, r_flag, R_flag, s_flag, t_flag, l_flag, P_flag );
			LOGDEBUG("a_flag %d, a_val '%s'", a_flag, a_val );
			LOGDEBUG("c_flag %d, c_val '%s'", c_flag, c_val );
			LOGDEBUG("f_flag %d, f_val '%s'", f_flag, f_val );
			LOGDEBUG("o_flag %d, o_val '%s'", o_flag, o_val );
			LOGDEBUG("p_flag %d, p_val '%s'", p_flag, p_val );
			LOGDEBUG("S_flag %d, S_val '%s'", S_flag, S_val );
			LOGDEBUG("u_flag %d, u_val '%s'", u_flag, u_val );
			LOGDEBUG("v_flag %d, v_val '%s'", v_flag, v_val );
			LOGDEBUG("n_flag %d", n_flag );
		}
	}
	for( ; i < argc; ++i ){
		Add_line_list( &Printer_list, argv[i], 0, 1, 1 );
	}

#undef SX
#define SX(X,Y,Z) \
	if((X)&&!(Y))Y="all"; Split(&Z,Y,", ",1,0,1,1,0,0);
	SX(a_flag,a_val,Printer_list);
	SX(c_flag,c_val,Printer_list);
	SX(f_flag,f_val,f_list);
	SX(o_flag,o_val,Printer_list);
	SX(p_flag,p_val,Printer_list);
	SX(S_flag,S_val,S_list);
	SX(u_flag,u_val,Printer_list);
	SX(v_flag,v_val,Printer_list);
#undef SX

	if( n_flag ){
		if( n_flag < 0 ){
			Status_line_count = -1;
		} else {
			Status_line_count = (1 << n_flag);
		}
	}
	if( Verbose ) {
		FPRINTF( STDOUT, "%s\n", Version );
		if( Verbose > 1 ) Printlist( Copyright, 1 );
	}
	for( i = 0; !All_printers && i < Printer_list.count; ++i ){
		s = Printer_list.list[i];
		All_printers = !safestrcasecmp(s,"all");
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL1){
		LOGDEBUG("All_printers %d, D_flag %d, d_flag %d, r_flag %d, R_flag %d, s_flag %d, t_flag %d, l_flag %d, P_flag %d",
			All_printers, D_flag,  d_flag, r_flag, R_flag, s_flag, t_flag, l_flag, P_flag );
		LOGDEBUG("a_flag %d, a_val '%s'", a_flag, a_val );
		LOGDEBUG("c_flag %d, c_val '%s'", c_flag, c_val );
		LOGDEBUG("f_flag %d, f_val '%s'", f_flag, f_val );
		LOGDEBUG("o_flag %d, o_val '%s'", o_flag, o_val );
		LOGDEBUG("p_flag %d, p_val '%s'", p_flag, p_val );
		LOGDEBUG("S_flag %d, S_val '%s'", S_flag, S_val );
		LOGDEBUG("u_flag %d, u_val '%s'", u_flag, u_val );
		LOGDEBUG("v_flag %d, v_val '%s'", v_flag, v_val );
		Dump_line_list("lpstat - Printer_list", &Printer_list);
	}
#endif
}

 char *lpstat_msg =
"usage: %s [-A] [-d] [-l] [-r] [-R] [-s] [-t] [-a [list]]\n\
  [-c [list]] [-f [list]] [-o [list]]\n\
  [-p [list]] [-P] [-S [list]] [list]\n\
  [-u [login-ID-list]] [-v [list]] [-V] [-n] [-Tdbgflags]\n\
 list is a list of print queues\n\
 -A        use authentication specified by AUTH environment variable\n\
 -a [list] destination status *\n\
 -c [list] class status *\n\
 -d        print default destination\n\
 -f [list] forms status *\n\
 -o [list] job or printer status *\n\
 -n        each -n increases number of status lines (default 1) *\n\
 -N        maximum number of status lines *\n\
 -p [list] printer status *\n\
 -P        paper types - ignored\n\
 -r        scheduler status\n\
 -s        summary status information - short format\n\
 -S [list] character set - ignored\n\
 -t        all status information - long format\n\
 -u [joblist] job status information\n\
 -v [list] printer mapping *\n\
 -V        verbose mode \n\
 -Tdbgflags debug flags\n\
    * - long status format produced\n";


void usage(void)
{
	FPRINTF( STDERR, lpstat_msg, Name );
	Parse_debug("=",-1);
	FPRINTF( STDOUT, "%s\n", Version );
	exit(1);
}

#if 0

#include "permission.h"
#include "lpd.h"
#include "lpd_status.h"
/* int Send_request( */
	int class,					/* 'Q'= LPQ, 'C'= LPC, M = lprm */
	int format,					/* X for option */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output					/* output on this FD */
	)
{
	int i, n;
	int socket = 1;
	char cmd[SMALLBUFFER];

	cmd[0] = format;
	cmd[1] = 0;
	SNPRINTF(cmd+1, sizeof(cmd)-1, RemotePrinter_DYN);
	for( i = 0; options[i]; ++i ){
		n = safestrlen(cmd);
		SNPRINTF(cmd+n,sizeof(cmd)-n," %s",options[i] );
	}
	Perm_check.remoteuser = "papowell";
	Perm_check.user = "papowell";
	Is_server = 1;
	Job_status(&socket,cmd);
	return(-1);
}

#endif

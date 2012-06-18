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
"$Id: initialize.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";

#include "lp.h"
#include "defs.h"
#include "initialize.h"
#include "getopt.h"
#include "child.h"
#include "gethostinfo.h"
#include "proctitle.h"
#include "getqueue.h"
#include "errorcodes.h"
/**** ENDINCLUDE ****/

#ifdef IS_AUX
# include <compat.h>
#endif
#if defined (HAVE_LOCALE_H)
# include <locale.h>
#endif

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif


/***************************************************************************
 * general initialization.
 * This should NOT do any network operations
 ***************************************************************************/

void Initialize(int argc,  char *argv[], char *envp[], int debugchar )
{
	char *s;
	int fd;


	/* the gettext facility has been shown to be able to be used to
	 * compromize setuid programs.
	 * remove the slightest possibility of NLSPATH being used in a root
	 * environment
	 */
	if( getuid() == ROOTUID || geteuid() == ROOTUID ){
#if defined(HAVE_UNSETENV)
		unsetenv("NLSPATH");
#elif defined(HAVE_SETENV)
		setenv("NLSPATH","",1);
#elif defined(HAVE_PUTENV)
		putenv("NLSPATH=");
#else
# error require one of unsetenv(), setenv(), or putenv()
#endif
	}

	DEBUG1("Initialize: starting");

	if( argc > 1 ){
		s = argv[1];
		if( s[0] == '-' && s[1] == debugchar ){
			if( s[2] ){
				Parse_debug(s+2,1);
			} else {
				Parse_debug(argv[2],1);
			}
		}
	}

    if(DEBUGL3){
		struct stat statb;
		int i;
        LOGDEBUG("Initialize: starting with open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                LOGDEBUG("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
            }
        }
    }
	/*
		open /dev/null on fd 0, 1, 2, 3, 4, if neccessary
		This must be done before using any other database access
		functions,  as they may open a socket and leave it open.
	*/
	if( (fd = open( "/dev/null", O_RDWR, 0600 )) < 0 ){
		LOGERR_DIE(LOG_CRIT) "Initialize: cannot open '/dev/null'" );
	}
	Max_open(fd);
	DEBUG1("Initialize: /dev/null fd %d", fd );
	if( Is_server ) while( fd < 5 ){
		if( (fd = dup(fd)) < 0 ){
			LOGERR_DIE(LOG_CRIT) "Initialize: main cannot dup '/dev/null'" );
		}
		Max_open(fd);
	}
	close(fd);

	initsetproctitle( argc, argv, envp );
	Name = "UNKNOWN";
	if( argv && argv[0] ){
		Name = argv[0];
		if( (s = strrchr(Name,'/')) ) Name = s+1;
	}
	/* set the umask so that you create safe files */
	umask( 0077 );

#ifdef IS_AUX
	/********************************************
	 * Apparently this needs to be done for AUX
	 *******************************************/
	/* A/UX needs this to be more BSD-compatible. */
	setcompat (COMPAT_BSDPROT);
	set42sig();
#endif

	/* set suid information */
	Setup_uid();

    if(DEBUGL3){
		struct stat statb;
		int i;
        LOGDEBUG("Initialize: before setlocale");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                LOGDEBUG("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
            }
        }
    }
#if defined (HAVE_LOCALE_H)
	setlocale(LC_ALL, "");
#endif
	/* FPRINTF(STDERR,"LOCALEDIR '" LOCALEDIR "'\n"); FPRINTF(STDERR,"PACKAGE '" PACKAGE "'\n"); */
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

    if(DEBUGL3){
		struct stat statb;
		int i;
        LOGDEBUG("Initialize: ending with open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                LOGDEBUG("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
            }
        }
    }

}

void Setup_configuration()
{
	char *s;
	struct line_list raw;
	struct line_list order;

	/* Get default configuration file information */
#ifdef DMALLOC
	extern int _dmalloc_outfile_fd;
	extern char *_dmalloc_logpath;
	char buffer[SMALLBUFFER];

	safestrdup("DMALLOC",__FILE__,__LINE__);
	if( _dmalloc_logpath && _dmalloc_outfile_fd < 0 ){
		_dmalloc_outfile_fd = open( _dmalloc_logpath,  O_WRONLY | O_CREAT | O_TRUNC, 0666);
		Max_open(_dmalloc_outfile_fd);
	}
	SNPRINTF(buffer,sizeof(buffer))"*** Setup_configuration: pid %d\n", getpid() );
	Write_fd_str(_dmalloc_outfile_fd,buffer);
	DEBUG1("Setup_configuration: _dmalloc_outfile fd %d", _dmalloc_outfile_fd);
#endif

	Init_line_list(&raw);
	Init_line_list(&order);
	Clear_config();


	DEBUG1("Setup_configuration: starting, Allow_getenv %d",
		Allow_getenv_DYN );

    /* get the configuration file information if there is any */
    if( Allow_getenv_DYN ){
		if( getuid() == ROOTUID || geteuid() == ROOTUID ){
			FPRINTF( STDERR,
			"%s: WARNING- LPD_CONF environment variable option enabled\n"
			"  and running as root!  You have an exposed security breach!\n"
			"  Recompile without -DGETENV or do not run clients as ROOT\n",
			Name );
			exit(1);
		}
		if( (s = getenv( LPD_CONF )) ){
			Set_DYN(&Config_file_DYN, s);
		}
    }

    DEBUG1("Setup_configuration: Configuration file '%s'", Config_file_DYN );
    DEBUG1("Setup_configuration: Require_configfiles_DYN '%d'",
		Require_configfiles_DYN );

	Get_config( Is_server || Require_configfiles_DYN, Config_file_DYN );

	Reset_daemonuid();
	if( Is_server ){
		Setdaemon_group();
		To_daemon();
	} else {
		s = Get_user_information();
		Set_DYN( &Logname_DYN, s );
		if(s) free(s); s = 0;
	}
	DEBUG1( "Is_server %d, DaemonUID %d, DaemonGID %d, UID %d, EUID %d, GID %d, EGID %d",
		Is_server, DaemonUID, DaemonGID,
		getuid(), geteuid(), getgid(), getegid() );

	DEBUG1("Setup_configuration: Host '%s', ShortHost '%s', user '%s'",
		FQDNHost_FQDN, ShortHost_FQDN, Logname_DYN );

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL2) Dump_parms( "Setup_configuration - final values", Pc_var_list );
#endif

	if( Is_server ){
		DEBUG2("Setup_configuration: Printcap_path '%s'", Printcap_path_DYN );
		Getprintcap_pathlist( 1, &raw, &PC_filters_line_list,
			Printcap_path_DYN );
		DEBUG2("Setup_configuration: Lpd_printcap_path '%s'", Lpd_printcap_path_DYN );
		Getprintcap_pathlist( 0, &raw, &PC_filters_line_list,
			Lpd_printcap_path_DYN );
		DEBUG2("Setup_configuration: Printer_perms_path '%s'", Printer_perms_path_DYN );
		Getprintcap_pathlist( 1, &RawPerm_line_list, &Perm_filters_line_list,
			Printer_perms_path_DYN );
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
	} else {
		DEBUG2("Setup_configuration: Printcap_path '%s'", Printcap_path_DYN );
		Getprintcap_pathlist( Require_configfiles_DYN,
			&raw, &PC_filters_line_list,
			Printcap_path_DYN );
	}
	Build_printcap_info( &PC_names_line_list, &PC_order_line_list,
		&PC_info_line_list, &raw, &Host_IP );
	/* now we can free up the raw list */
	Free_line_list( &raw );

	/* now we get the user level information */
	DEBUG2("Setup_configuration: User_printcap '%s'", User_printcap_DYN );
	if( !Is_server && User_printcap_DYN && (s = getenv("HOME")) ){
		s = Make_pathname( s, User_printcap_DYN );
		DEBUG2("Setup_configuration: User_printcap '%s'", s );
		Getprintcap_pathlist( 0, &raw, 0, s );
		Build_printcap_info( &PC_names_line_list, &order,
			&PC_info_line_list, &raw, &Host_IP );
		Free_line_list( &raw );
		if( s ) free(s); s = 0;
		/* now we append the names from the order list */
		if( order.count > 0 ){
			int i;
			/* append the found ones, remove duplicates */
			for( i = 0; i < PC_order_line_list.count; ++i ){
				int j;
				int found = 0;
				s = PC_order_line_list.list[i];
				for( j = 0; !found && j < order.count; ++j ){
					found = !strcmp(s, order.list[j]);
				}
				if( !found ){
					Add_line_list(&order,s,0,0,0);
				}
			}
			Free_line_list( &PC_order_line_list );
			for( i = 0; i < order.count; ++i ){
				s = order.list[i];
				Add_line_list(&PC_order_line_list,s,0,0,0);
			}
		}
		Free_line_list( &order );
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3){
		Dump_line_list("Setup_configuration: PC names", &PC_names_line_list );
		Dump_line_list("Setup_configuration: PC order", &PC_order_line_list );
		Dump_line_list("Setup_configuration: PC info", &PC_info_line_list );
	/*
		Dump_line_list("Setup_configuration: User_PC names", &User_PC_names_line_list );
		Dump_line_list("Setup_configuration: User_PC order", &User_PC_order_line_list );
		Dump_line_list("Setup_configuration: User_PC info", &User_PC_info_line_list );
	*/
		Dump_line_list("Setup_configuration: Raw Perms", &RawPerm_line_list );
		Dump_line_list("Setup_configuration: Perms", &Perm_line_list );
	}
#endif
}

/*
 * char *Get_user_information(void)
 * OUTPUT: dynamic alloc string
 *  - returns user name
 */
char *Get_user_information( void )
{
	char *name = 0;
	char uid_msg[32];
	uid_t uid = OriginalRUID;

	struct passwd *pw_ent;

	/* get the password file entry */
    if( (pw_ent = getpwuid( uid )) ){
		name =  pw_ent->pw_name;
	}
	if( name == 0 ) name = getenv( "LOGNAME" );
	if( name == 0 ) name = getenv( "USER" );
	if( name == 0 ){
		SNPRINTF( uid_msg, sizeof(uid_msg)) "UID_%d", uid );
		name = uid_msg;
	}
	name = safestrdup(name,__FILE__,__LINE__);
    return( name );
}

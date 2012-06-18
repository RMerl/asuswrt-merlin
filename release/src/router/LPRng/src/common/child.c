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
"$Id: child.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


#include "lp.h"
#include "getqueue.h"
#include "getopt.h"
#include "gethostinfo.h"
#include "proctitle.h"
#include "linksupport.h"
#include "child.h"
/**** ENDINCLUDE ****/

/*
 * Historical compatibility
 */
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

/*
 * This avoids some problems
 * when you have things redefined in system include files.
 * Don't even think about how this came about - simply mutter
 * the mantra 'Portablility at any cost...' and be happy.
 */

#if !defined(TIOCNOTTY) && defined(HAVE_SYS_TTOLD_H) && !defined(IRIX)
#  include <sys/ttold.h>
#endif


/*
 * Patrick Powell
 *  We will normally want to wait for children explitly.
 *  Most of the time they can die, but we do not care.
 *  However, some times we want them to cause a BREAK condition
 *  on a system call.
 */

pid_t plp_waitpid (pid_t pid, plp_status_t *statusPtr, int options)
{
	int report;
	memset(statusPtr,0,sizeof(statusPtr[0]));
	DEBUG2("plp_waitpid: pid %d, options %d", pid, options );
	report = waitpid(pid, statusPtr, options );
	DEBUG2("plp_waitpid: returned %d, status %s", report,
		Decode_status( statusPtr ) );
	return report;
}

#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * Commentary:
 * When we fork a child, then we need to clean it up.
 * Note that the forking operation should either be to create
 * a subchild which will be in the process group, or one
 * that will be in the same process group
 *
 * We will keep a list of the children which will be in the new
 * process group.  When we exit, during cleanup,  we will
 * kill all of these off.  Note that we may have to dynamically allocate
 * memory to keep the list.
 *
 * Killchildren( signal ) - kill all children of this process
 ***************************************************************************/

void Dump_pinfo( char *title, struct line_list *p ) 
{
	int i, pid;
	LOGDEBUG("*** Dump_pinfo %s - count %d ***", title, p->count );
	for( i = 0; i < p->count; ++i ){
		pid = Cast_ptr_to_int(p->list[i]);
		LOGDEBUG("  pid %d", pid );
	}
	LOGDEBUG("*** done ***");
}
#endif

int Countpid(void)
{
	int i, j, pid;

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_pinfo("Countpid - before",&Process_list);
#endif
	for( i = j = 0; i < Process_list.count; ++i ){
		pid = Cast_ptr_to_int(Process_list.list[i]);
		if( kill(pid, 0) == 0 ){
			DEBUG4("Countpid: pid %d active", pid );
			Process_list.list[j++] = Cast_int_to_voidstar(pid);
		}
	}
	Process_list.count = j;
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_pinfo("Countpid - after", &Process_list);
#endif
	return( Process_list.count );
}

void Killchildren( int sig )
{
	int pid, i, j;
	
	DEBUG2("Killchildren: pid %d, signal %s, count %d",
			(int)getpid(),Sigstr(sig), Process_list.count );

	for( i = j = 0; i < Process_list.count; ++i ){
		pid = Cast_ptr_to_int(Process_list.list[i]);
		DEBUG2("Killchildren: pid %d, signal '%s'", pid, Sigstr(sig) );
		killpg(pid,sig);
		killpg(pid,SIGCONT);
		kill(pid,sig);
		kill(pid,SIGCONT);
		if( kill(pid, sig) == 0 ){
			DEBUG4("Killchildren: pid %d still active", pid );
			Process_list.list[j++] = Cast_int_to_voidstar(pid);
		}
	}
	Process_list.count = j;
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL2)Dump_pinfo("Killchildren - after",&Process_list);
#endif
}

/*
 * dofork: fork a process and set it up as a process group leader
 */

pid_t dofork( int new_process_group )
{
	pid_t pid;
	int i;
	char *s;

	pid = fork();
	if( pid == 0 ){
		/* you MUST put the process in another process group;
		 * if you have a filter, and it does a 'killpg()' signal,
		 * if you do not have it in a separate group the effects
		 * are catastrophic.  Believe me on this one...
		 */

		if( new_process_group ){
#if defined(HAVE_SETSID)
		i =	setsid();
		s = "setsid()";
#else
# if defined(HAVE_SETPGID)
		i =	setpgid(0,getpid());
		s =	"setpgid(0,getpid())";
# else
#   if defined(HAVE_SETPGRP_0)
		i =	setpgrp(0,getpid());
		s =	"setpgrp(0,getpid())";
#   else
	    i =	setpgrp();
	    s =	"setpgrp()";
#   endif
# endif
#endif
		if( i < 0 ){
			LOGERR_DIE(LOG_ERR) "dofork: %s failed", s );
		}
#ifdef TIOCNOTTY
		/* BSD: non-zero process group id, so it cannot get control terminal */
		/* you MUST be able to turn off the control terminal this way */
		if ((i = open ("/dev/tty", O_RDWR, 0600 )) >= 0) {
			if( ioctl (i, TIOCNOTTY, (void *)0 ) < 0 ){
				LOGERR_DIE(LOG_ERR) "dofork: TIOCNOTTY failed" );
			}
			(void)close(i);
		}
#endif
		/* BSD: We have lost the controlling terminal at this point;
		 *  we are now the process group leader and cannot get one
		 *  unless we use the TIOCTTY ioctl call
		 * Solaris && SYSV: We have lost the controlling terminal at this point;
		 *  we are now the process group leader,
		 *  we may get control tty unless we open devices with
		 *	the O_NOCTTY flag;  if you do not have this open flag
		 *	you are in trouble.
		 * See: UNIX Network Programming, W. Richard Stevens,
		 *		 Section 2.6, Daemon Processes
		 *		Advanced Programming in the UNIX environment,
		 *		 W. Richard Stevens, Section 9.5
		 */
		}
		/* we do not want to copy our parent's exit jobs or temp files */
		Process_list.count = 0;
		Free_line_list( &Process_list );
		Clear_tempfile_list();

		/*
		 * We need to make sure that LPD forked processes do not have blocked
		 * signals.  The only two that I block are SIGCHLD and SIGUSR1.
		 * These,  unfortunately,  are precisely the ones that cause problems.
		 * Also,  SIGALRM is used,  but it is never blocked.
		 */
		if( Is_server ){
			plp_block_mask oblock; plp_unblock_all_signals( &oblock );
		}
	} else if( pid != -1 ){
		Check_max(&Process_list,1);
		Process_list.list[Process_list.count++] = Cast_int_to_voidstar(pid);
	}
	return( pid );
}
/*
 * routines to call on exit
 */

plp_signal_t cleanup_USR1 (int passed_signal)
{
	DEBUG4("cleanup_USR1: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGUSR1);
}
plp_signal_t cleanup_HUP (int passed_signal)
{
	DEBUG4("cleanup_HUP: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGHUP);
}
plp_signal_t cleanup_INT (int passed_signal)
{
	DEBUG4("cleanup_INT: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGINT);
}
plp_signal_t cleanup_QUIT (int passed_signal)
{
	DEBUG4("cleanup_QUIT: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGQUIT);
}
plp_signal_t cleanup_TERM (int passed_signal)
{
	DEBUG4("cleanup_TERM: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGTERM);
}

void Max_open( int fd )
{
	if( fd > 0 ){
#if 0
		if( fd > Max_fd+10 ){
			FATAL(LOG_ERR) "Max_open: fd %d and old Max_fd %d", fd, Max_fd);
		}
#endif
		if( fd > Max_fd ) Max_fd = fd;
	}
}

plp_signal_t cleanup (int passed_signal)
{
	plp_block_mask oblock;
	int i;
	int signalv = passed_signal;

	plp_block_all_signals( &oblock ); /**/

	DEBUG2("cleanup: signal %s, Errorcode %d",
		Sigstr(passed_signal), Errorcode );
#if defined(__CYGWIN__)
	if( getpid() == Server_pid ) {
		char *path = safestrdup3( Lockfile_DYN,".", Lpd_port_DYN, __FILE__, __LINE__ );
        unlink(path);
		if( path ) free(path); path = 0;
	}
#endif

	/* shut down all logging stuff */
	Doing_cleanup = 1;
	/* first we try to close all the output ports */
	for( i = 3; i < Max_fd; ++i ){
#ifdef DMALLOC
		extern int _dmalloc_outfile_fd;
		if( i == _dmalloc_outfile_fd ) continue;
#endif
		close(i);
	}

	Remove_tempfiles();
	/* then we do exit cleanup */

	if( passed_signal == 0 ){
		signalv = SIGINT;
	} else if( passed_signal == SIGUSR1 ){
		passed_signal = 0;
		signalv = SIGINT;
		Errorcode = 0;
	}

	/* kill children of this process */
	Killchildren( signalv );
	Killchildren( SIGINT );
	Killchildren( SIGHUP );
	Killchildren( SIGQUIT );
	Process_list.count = 0;
	DEBUG1("cleanup: done, exit(%d)", Errorcode);

	if( Errorcode == 0 ){
		Errorcode = passed_signal;
	}

	Dump_unfreed_mem("**** cleanup");

	exit(Errorcode);
}

void Dump_unfreed_mem(char *title)
{
	char buffer[SMALLBUFFER];
	buffer[0] = 0;
	SNPRINTF(buffer,sizeof(buffer))"*** Dump_unfreed_mem: %s, pid %d\n",
		title, getpid() );
#if defined(DMALLOC)
	{
	extern int _dmalloc_outfile_fd;
	extern char *_dmalloc_logpath;

	if( _dmalloc_logpath && _dmalloc_outfile_fd < 0 ){
		_dmalloc_outfile_fd = open( _dmalloc_logpath,  O_WRONLY | O_CREAT | O_TRUNC, 0666);
		Max_open( _dmalloc_outfile_fd );
	}
	SNPRINTF(buffer,sizeof(buffer))"*** Dump_unfreed_mem: %s, pid %d\n",
		title, getpid() );
	Write_fd_str(_dmalloc_outfile_fd, buffer );
	if(Outbuf) free(Outbuf); Outbuf = 0;
	if(Inbuf) free(Inbuf); Inbuf = 0;
	Clear_tempfile_list();
	{
		struct line_list **l;
		for( l = Allocs; *l; ++l ) Free_line_list(*l);
	}
	Process_list.count = 0;
	Free_line_list(&Process_list);
	Clear_all_host_information();
    Clear_var_list( Pc_var_list, 0 );
    Clear_var_list( DYN_var_list, 0 );
	dmalloc_log_unfreed();
	Write_fd_str(_dmalloc_outfile_fd, "***\n" );
	exit(Errorcode);
	}
#endif
}

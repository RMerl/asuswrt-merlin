/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   connection claim routines
   Copyright (C) Andrew Tridgell 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"


extern fstring remote_machine;

extern int DEBUGLEVEL;

#ifdef WITH_UTMP
static void utmp_yield(pid_t pid, const connection_struct *conn, int i);
static void utmp_claim(const struct connect_record *crec, connection_struct *conn, int i);
#endif

/****************************************************************************
simple routines to do connection counting
****************************************************************************/
BOOL yield_connection(connection_struct *conn,char *name,int max_connections)
{
	struct connect_record crec;
	pstring fname;
	int fd;
	pid_t mypid = getpid();
	int i;

	DEBUG(3,("Yielding connection to %s\n",name));

	if (max_connections <= 0)
		return(True);

	memset((char *)&crec,'\0',sizeof(crec));

	pstrcpy(fname,lp_lockdir());
	trim_string(fname,"","/");

	pstrcat(fname,"/");
	pstrcat(fname,name);
	pstrcat(fname,".LCK");

	fd = sys_open(fname,O_RDWR,0);
	if (fd == -1) {
		DEBUG(2,("Couldn't open lock file %s (%s)\n",fname,strerror(errno)));
		return(False);
	}

	if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_WRLCK)==False) {
		DEBUG(0,("ERROR: can't get lock on %s\n", fname));
		return False;
	}

	/* find the right spot */
	for (i=0;i<max_connections;i++) {
		if (read(fd, &crec,sizeof(crec)) != sizeof(crec)) {
			DEBUG(2,("Entry not found in lock file %s\n",fname));
			if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_UNLCK)==False) {
				DEBUG(0,("ERROR: can't release lock on %s\n", fname));
			}
			close(fd);
			return(False);
		}
		if (crec.pid == mypid && crec.cnum == conn->cnum)
			break;
	}

	if (crec.pid != mypid || crec.cnum != conn->cnum) {
		if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_UNLCK)==False) {
			DEBUG(0,("ERROR: can't release lock on %s\n", fname));
		}
		close(fd);
		DEBUG(2,("Entry not found in lock file %s\n",fname));
		return(False);
	}

	memset((void *)&crec,'\0',sizeof(crec));
  
	/* remove our mark */
	if (sys_lseek(fd,i*sizeof(crec),SEEK_SET) != i*sizeof(crec) ||
	    write(fd, &crec,sizeof(crec)) != sizeof(crec)) {
		DEBUG(2,("Couldn't update lock file %s (%s)\n",fname,strerror(errno)));
		if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_UNLCK)==False) {
			DEBUG(0,("ERROR: can't release lock on %s\n", fname));
		}
		close(fd);
		return(False);
	}

	if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_UNLCK)==False) {
		DEBUG(0,("ERROR: can't release lock on %s\n", fname));
	}

	DEBUG(3,("Yield successful\n"));

	close(fd);

#ifdef WITH_UTMP
	utmp_yield(mypid, conn, i);
#endif

	return(True);
}


/****************************************************************************
simple routines to do connection counting
****************************************************************************/
BOOL claim_connection(connection_struct *conn,char *name,int max_connections,BOOL Clear)
{
	extern int Client;
	struct connect_record crec;
	pstring fname;
	int fd=-1;
	int i,foundi= -1;
	int total_recs;
	
	if (max_connections <= 0)
		return(True);
	
	DEBUG(5,("trying claim %s %s %d\n",lp_lockdir(),name,max_connections));
	
	pstrcpy(fname,lp_lockdir());
	trim_string(fname,"","/");
	
	if (!directory_exist(fname,NULL))
		mkdir(fname,0755);
	
	pstrcat(fname,"/");
	pstrcat(fname,name);
	pstrcat(fname,".LCK");
	
	if (!file_exist(fname,NULL)) {
		fd = sys_open(fname,O_RDWR|O_CREAT|O_EXCL, 0644);
	}

	if (fd == -1) {
		fd = sys_open(fname,O_RDWR,0);
	}
	
	if (fd == -1) {
		DEBUG(1,("couldn't open lock file %s\n",fname));
		return(False);
	}

	if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_WRLCK)==False) {
		DEBUG(0,("ERROR: can't get lock on %s\n", fname));
		return False;
	}

	total_recs = get_file_size(fname) / sizeof(crec);
			
	/* find a free spot */
	for (i=0;i<max_connections;i++) {
		if (i>=total_recs || 
		    sys_lseek(fd,i*sizeof(crec),SEEK_SET) != i*sizeof(crec) ||
		    read(fd,&crec,sizeof(crec)) != sizeof(crec)) {
			if (foundi < 0) foundi = i;
			break;
		}
		
		if (Clear && crec.pid && !process_exists(crec.pid)) {
			if(sys_lseek(fd,i*sizeof(crec),SEEK_SET) != i*sizeof(crec)) {
              DEBUG(0,("claim_connection: ERROR: sys_lseek failed to seek \
to %d\n", (int)(i*sizeof(crec)) ));
              continue;
            }
			memset((void *)&crec,'\0',sizeof(crec));
			write(fd, &crec,sizeof(crec));
			if (foundi < 0) foundi = i;
			continue;
		}
		if (foundi < 0 && (!crec.pid || !process_exists(crec.pid))) {
			foundi=i;
			if (!Clear) break;
		}
	}  
	
	if (foundi < 0) {
		DEBUG(3,("no free locks in %s\n",fname));
		if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_UNLCK)==False) {
			DEBUG(0,("ERROR: can't release lock on %s\n", fname));
		}
		close(fd);
		return(False);
	}      
	
	/* fill in the crec */
	memset((void *)&crec,'\0',sizeof(crec));
	crec.magic = 0x280267;
	crec.pid = getpid();
	if (conn) {
		crec.cnum = conn->cnum;
		crec.uid = conn->uid;
		crec.gid = conn->gid;
		StrnCpy(crec.name,
			lp_servicename(SNUM(conn)),sizeof(crec.name)-1);
	} else {
		crec.cnum = -1;
	}
	crec.start = time(NULL);
	
	StrnCpy(crec.machine,remote_machine,sizeof(crec.machine)-1);
	StrnCpy(crec.addr,client_addr(Client),sizeof(crec.addr)-1);
	
	/* make our mark */
	if (sys_lseek(fd,foundi*sizeof(crec),SEEK_SET) != foundi*sizeof(crec) ||
	    write(fd, &crec,sizeof(crec)) != sizeof(crec)) {
		if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_UNLCK)==False) {
			DEBUG(0,("ERROR: can't release lock on %s\n", fname));
		}
		close(fd);
		return(False);
	}

	if (fcntl_lock(fd,SMB_F_SETLKW,0,1,F_UNLCK)==False) {
		DEBUG(0,("ERROR: can't release lock on %s\n", fname));
	}
	
	close(fd);

#ifdef WITH_UTMP
	utmp_claim(&crec, conn, foundi);
#endif

	return(True);
}

#ifdef WITH_UTMP

/****************************************************************************
Reflect connection status in utmp/wtmp files.
	T.D.Lee@durham.ac.uk  September 1999

Hints for porting:
	o Always attempt to use programmatic interface (pututline() etc.)
	o The "x" (utmpx/wtmpx; HAVE_UTMPX_H) seems preferable.

OS status:
	Solaris 2.x:  Tested on 2.6 and 2.7; should be OK on other flavours.
		T.D.Lee@durham.ac.uk
	HPUX 9.x:  Not tested.  Appears not to have "x".
	IRIX 6.5:  Not tested.  Appears to have "x".

OS observations:
	Almost every OS seems to have its own quirks.

Notes:
	The 4 byte 'ut_id' component is vital to distinguish connections,
	of which there could be several hundered or even thousand.
	Entries seem to be printable characters, with optional NULL pads.

	We need to be distinct from other entries in utmp/wtmp.

	Observed things: therefore avoid them.  Add to this list please.
	From Solaris 2.x (because that's what I have):
		'sN'	: run-levels; N: [0-9]
		'co'	: console
		'CC'	: arbitrary things;  C: [a-z]
		'rXNN'	: rlogin;  N: [0-9]; X: [0-9a-z]
		'tXNN'	: rlogin;  N: [0-9]; X: [0-9a-z]
		'/NNN'	: Solaris CDE
		'ftpZ'	: ftp (Z is the number 255, aka 0377, aka 0xff)
	Mostly a record uses the same 'ut_id' in both "utmp" and "wtmp",
	but differences have been seen.

	Arbitrarily I have chosen to use a distinctive 'SM' for the
	first two bytes.

	The remaining two encode the connection number used in samba locking
	functions "claim_connection() and "yield_connection()".  This seems
	to be a "nicely behaved" number: starting from 0 then working up
	looking for an available slot.

****************************************************************************/

#include <utmp.h>

/*
 * Apparently AIX has utmpx.h but doesn't implement it.
 * The test for this ought to be (a) more automatic (b) elsewhere.
 */
#if defined (AIX)
#undef HAVE_UTMPX_H
#endif

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif

static const char *ut_id_encstr =
	"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static
int
ut_id_encode(int i, char *fourbyte)
{
	int nbase;

	fourbyte[0] = 'S';
	fourbyte[1] = 'M';

/*
 * Encode remaining 2 bytes from 'i'.
 * 'ut_id_encstr' is the character set on which modulo arithmetic is done.
 * Example: digits would produce the base-10 numbers from '001'.
 */
	nbase = strlen(ut_id_encstr);

	fourbyte[3] = ut_id_encstr[i % nbase];
	i /= nbase;
	fourbyte[2] = ut_id_encstr[i % nbase];
	i /= nbase;

	return(i);	/* 0: good; else overflow */
}

/*
 * Fill in a utmp (not utmpx) template
 */
static int utmp_fill(struct utmp *u, const connection_struct *conn, pid_t pid,
  int i, pstring host)
{
#if defined(HAVE_UT_UT_TIME)
	struct timeval timeval;
#endif /* defined(HAVE_UT_UT_TIME) */
	int rc = 0;

#if defined(HAVE_UT_UT_USER)
	pstrcpy(u->ut_user, conn->user);
#endif /* defined(HAVE_UT_UT_USER) */

#if defined(HAVE_UT_UT_NAME)
	pstrcpy(u->ut_name, conn->user);
#endif /* defined(HAVE_UT_UT_NAME) */

	slprintf(u->ut_line, 12, "smb/%d", i);

	u->ut_pid = pid;

#if defined(HAVE_UT_UT_TIME)
	gettimeofday(&timeval, NULL);
	u->ut_time = timeval.tv_sec;
#endif /* defined(HAVE_UT_UT_TIME) */

#if defined(HAVE_UT_UT_TV)
	gettimeofday(&timeval, NULL);
	u->ut_tv = timeval;
#endif /* defined(HAVE_UT_UT_TV) */

#if defined(HAVE_UT_UT_HOST)
	if (host) {
		pstrcpy(u->ut_host, host);
	}
#endif /* defined(HAVE_UT_UT_HOST) */

#if defined(HAVE_UT_UT_ID)
	rc = ut_id_encode(i, u->ut_id);
#endif /* defined(HAVE_UT_UT_ID) */

	return(rc);
}

/*
 * Default paths to various {u,w}tmp{,x} files
 */
#ifdef	HAVE_UTMPX_H

static char *ux_pathname =
# if defined (UTMPX_FILE)
	UTMPX_FILE ;
# elif defined (_UTMPX_FILE)
	_UTMPX_FILE ;
# elif defined (_PATH_UTMPX)
	_PATH_UTMPX ;
# else
	"" ;
# endif

static char *wx_pathname =
# if defined (WTMPX_FILE)
	WTMPX_FILE ;
# elif defined (_WTMPX_FILE)
	_WTMPX_FILE ;
# elif defined (_PATH_WTMPX)
	_PATH_WTMPX ;
# else
	"" ;
# endif

#endif	/* HAVE_UTMPX_H */

static char *ut_pathname =
# if defined (UTMP_FILE)
	UTMP_FILE ;
# elif defined (_UTMP_FILE)
	_UTMP_FILE ;
# elif defined (_PATH_UTMP)
	_PATH_UTMP ;
# else
	"" ;
# endif

static char *wt_pathname =
# if defined (WTMP_FILE)
	WTMP_FILE ;
# elif defined (_WTMP_FILE)
	_WTMP_FILE ;
# elif defined (_PATH_WTMP)
	_PATH_WTMP ;
# else
	"" ;
# endif

/*
 * Get name of {u,w}tmp{,x} file.
 *	return: fname contains filename
 *		Possibly empty if this code not yet ported to this system.
 *
 * utmp{,x}:  try "utmp dir", then default (a define)
 * wtmp{,x}:  try "wtmp dir", then "utmp dir", then default (a define)
 */
static void uw_pathname(pstring fname, const char *uw_name, const char *uw_default)
{
	pstring dirname;

	pstrcpy(dirname, "");

	/* For w-files, first look for explicit "wtmp dir" */
	if (uw_name[0] == 'w') {
		pstrcpy(dirname,lp_wtmpdir());
		trim_string(dirname,"","/");
	}

	/* For u-files and non-explicit w-dir, look for "utmp dir" */
	if (dirname == 0 || strlen(dirname) == 0) {
		pstrcpy(dirname,lp_utmpdir());
		trim_string(dirname,"","/");
	}

	/* If explicit directory above, use it */
	if (dirname != 0 && strlen(dirname) != 0) {
		pstrcpy(fname, dirname);
		pstrcat(fname, "/");
		pstrcat(fname, uw_name);
		return;
	}

	/* No explicit directory: attempt to use default paths */
	if (strlen(uw_default) == 0) {
		/* No explicit setting, no known default.
		 * Has it yet been ported to this OS?
		 */
		DEBUG(2,("uw_pathname: unable to determine pathname\n"));
	}
	pstrcpy(fname, uw_default);
}

static void utmp_update(const struct utmp *u, pstring host)
{
	pstring fname;

#ifdef HAVE_UTMPX_H
	struct utmpx ux, *uxrc;

	getutmpx(u, &ux);
	if (host) {
#if defined(HAVE_UX_UT_SYSLEN)
		ux.ut_syslen = strlen(host);
#endif /* defined(HAVE_UX_UT_SYSLEN) */
		pstrcpy(ux.ut_host, host);
	}

	uw_pathname(fname, "utmpx", ux_pathname);
	DEBUG(2,("utmp_update: fname:%s\n", fname));
	if (strlen(fname) != 0) {
		utmpxname(fname);
	}
	setutxent();
	uxrc = pututxline(&ux);
	endutxent();
	if (uxrc == NULL) {
		DEBUG(2,("utmp_update: pututxline() failed\n"));
		return;
	}

	uw_pathname(fname, "wtmpx", wx_pathname);
	DEBUG(2,("utmp_update: fname:%s\n", fname));
	if (strlen(fname) != 0) {
		updwtmpx(fname, &ux);
	}
#else
	uw_pathname(fname, "utmp", ut_pathname);
	DEBUG(2,("utmp_update: fname:%s\n", fname));
	if (strlen(fname) != 0) {
		utmpname(fname);
	}
	setutent();
	pututline(u);
	endutent();

	uw_pathname(fname, "wtmp", wt_pathname);

	/* *** Hmmm.  Appending wtmp (as distinct from overwriting utmp) has
	me baffled.  How is it to be done? *** */
#endif
}

static void utmp_yield(pid_t pid, const connection_struct *conn, int i)
{
	struct utmp u;
	int nopen;

	if (! lp_utmp(SNUM(conn))) {
		DEBUG(2,("utmp_yield: lp_utmp() NULL\n"));
		return;
	}

	nopen = conn_num_open();
	DEBUG(2,("utmp_yield: conn: user:%s cnum:%d i:%d (nopen:%d)\n",
	  conn->user, conn->cnum, i, nopen));

	if (lp_utmp_consolidate() && nopen > 1) {
		DEBUG(2,("utmp_yield: utmp consolidate: %d entries open\n", nopen));
		return;
	}

	memset((char *)&u, '\0', sizeof(struct utmp));
	u.ut_type = DEAD_PROCESS;
	u.ut_exit.e_termination = 0;
	u.ut_exit.e_exit = 0;
	if (utmp_fill(&u, conn, pid, i, NULL) == 0) {
		utmp_update(&u, NULL);
	}
}

static void utmp_claim(const struct connect_record *crec, connection_struct *conn, int i)
{
	struct utmp u;
	pstring host;
	int nopen;

	if (conn == NULL) {
		DEBUG(2,("utmp_claim: conn NULL\n"));
		return;
	}

	if (! lp_utmp(SNUM(conn))) {
		DEBUG(2,("utmp_claim: lp_utmp() NULL\n"));
		return;
	}

	nopen = conn_num_open();
	if (lp_utmp_consolidate() && nopen > 1) {
		DEBUG(2,("utmp_claim: utmp consolidate: %d entries open\n", nopen));
		return;
	}

	pstrcpy(host, lp_utmp_hostname());
	if (host == 0 || strlen(host) == 0) {
		pstrcpy(host, crec->machine);
	}
	else {
		/* explicit "utmp host": expand for any "%" variables */
		standard_sub(conn, host);
	}

	nopen = conn_num_open();
	DEBUG(2,("utmp_claim: conn: user:%s cnum:%d i:%d (nopen:%d)\n",
	  conn->user, conn->cnum, i, nopen));
	DEBUG(2,("utmp_claim: crec: pid:%d, cnum:%d name:%s addr:%s mach:%s host:%s\n",
	  crec->pid, crec->cnum, crec->name, crec->addr, crec->machine, host));


	memset((char *)&u, '\0', sizeof(struct utmp));
	u.ut_type = USER_PROCESS;
	if (utmp_fill(&u, conn, crec->pid, i, host) == 0) {
		utmp_update(&u, host);
	}
}

#endif

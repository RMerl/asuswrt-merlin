/* 
   Unix SMB/CIFS implementation.
   status reporting
   Copyright (C) Andrew Tridgell 1994-1998
   
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

   Revision History:

   12 aug 96: Erik.Devriendt@te6.siemens.be
   added support for shared memory implementation of share mode locking

   21-Jul-1998: rsharpe@ns.aus.com (Richard Sharpe)
   Added -L (locks only) -S (shares only) flags and code

*/

/*
 * This program reports current SMB connections
 */

#include "includes.h"

#define SMB_MAXPIDS		2048
static uid_t 		Ucrit_uid = 0;               /* added by OH */
static pid_t		Ucrit_pid[SMB_MAXPIDS];  /* Ugly !!! */   /* added by OH */
static int		Ucrit_MaxPid=0;                    /* added by OH */
static unsigned int	Ucrit_IsActive = 0;                /* added by OH */

static int verbose, brief;
static int            shares_only = 0;            /* Added by RJS */
static int            locks_only  = 0;            /* Added by RJS */
static BOOL processes_only=False;
static int show_brl;
static BOOL numeric_only = False;

const char *username = NULL;

extern BOOL status_profile_dump(BOOL be_verbose);
extern BOOL status_profile_rates(BOOL be_verbose);

/* added by OH */
static void Ucrit_addUid(uid_t uid)
{
	Ucrit_uid = uid;
	Ucrit_IsActive = 1;
}

static unsigned int Ucrit_checkUid(uid_t uid)
{
	if ( !Ucrit_IsActive ) 
		return 1;
	
	if ( uid == Ucrit_uid ) 
		return 1;
	
	return 0;
}

static unsigned int Ucrit_checkPid(pid_t pid)
{
	int i;
	
	if ( !Ucrit_IsActive ) 
		return 1;
	
	for (i=0;i<Ucrit_MaxPid;i++) {
		if( pid == Ucrit_pid[i] ) 
			return 1;
	}
	
	return 0;
}

static BOOL Ucrit_addPid( pid_t pid )
{
	if ( !Ucrit_IsActive )
		return True;

	if ( Ucrit_MaxPid >= SMB_MAXPIDS ) {
		d_printf("ERROR: More than %d pids for user %s!\n",
			 SMB_MAXPIDS, uidtoname(Ucrit_uid));

		return False;
	}

	Ucrit_pid[Ucrit_MaxPid++] = pid;
	
	return True;
}

static void print_share_mode(const struct share_mode_entry *e,
			     const char *sharepath,
			     const char *fname,
			     void *dummy)
{
	static int count;

	if (!is_valid_share_mode_entry(e)) {
		return;
	}

	if (count==0) {
		d_printf("Locked files:\n");
		d_printf("Pid          Uid        DenyMode   Access      R/W        Oplock           SharePath   Name   Time\n");
		d_printf("--------------------------------------------------------------------------------------------------\n");
	}
	count++;

	if (Ucrit_checkPid(procid_to_pid(&e->pid))) {
		d_printf("%-11s  ",procid_str_static(&e->pid));
		d_printf("%-9u  ", (unsigned int)e->uid);
		switch (map_share_mode_to_deny_mode(e->share_access,
						    e->private_options)) {
			case DENY_NONE: d_printf("DENY_NONE  "); break;
			case DENY_ALL:  d_printf("DENY_ALL   "); break;
			case DENY_DOS:  d_printf("DENY_DOS   "); break;
			case DENY_READ: d_printf("DENY_READ  "); break;
			case DENY_WRITE:printf("DENY_WRITE "); break;
			case DENY_FCB:  d_printf("DENY_FCB "); break;
			default: {
				d_printf("unknown-please report ! "
					 "e->share_access = 0x%x, "
					 "e->private_options = 0x%x\n",
					 (unsigned int)e->share_access,
					 (unsigned int)e->private_options );
				break;
			}
		}
		d_printf("0x%-8x  ",(unsigned int)e->access_mask);
		if ((e->access_mask & (FILE_READ_DATA|FILE_WRITE_DATA))==
				(FILE_READ_DATA|FILE_WRITE_DATA)) {
			d_printf("RDWR       ");
		} else if (e->access_mask & FILE_WRITE_DATA) {
			d_printf("WRONLY     ");
		} else {
			d_printf("RDONLY     ");
		}

		if((e->op_type & (EXCLUSIVE_OPLOCK|BATCH_OPLOCK)) == 
					(EXCLUSIVE_OPLOCK|BATCH_OPLOCK)) {
			d_printf("EXCLUSIVE+BATCH ");
		} else if (e->op_type & EXCLUSIVE_OPLOCK) {
			d_printf("EXCLUSIVE       ");
		} else if (e->op_type & BATCH_OPLOCK) {
			d_printf("BATCH           ");
		} else if (e->op_type & LEVEL_II_OPLOCK) {
			d_printf("LEVEL_II        ");
		} else {
			d_printf("NONE            ");
		}

		d_printf(" %s   %s   %s",sharepath, fname, time_to_asc((time_t)e->time.tv_sec));
	}
}

static void print_brl(SMB_DEV_T dev,
			SMB_INO_T ino,
			struct process_id pid, 
			enum brl_type lock_type,
			enum brl_flavour lock_flav,
			br_off start,
			br_off size)
{
	static int count;
	if (count==0) {
		d_printf("Byte range locks:\n");
		d_printf("   Pid     dev:inode  R/W      start        size\n");
		d_printf("------------------------------------------------\n");
	}
	count++;

	d_printf("%8s   %05x:%05x    %s  %9.0f   %9.0f\n", 
	       procid_str_static(&pid), (int)dev, (int)ino, 
	       lock_type==READ_LOCK?"R":"W",
	       (double)start, (double)size);
}

static int traverse_fn1(TDB_CONTEXT *tdb, TDB_DATA kbuf, TDB_DATA dbuf, void *state)
{
	struct connections_data crec;

	if (dbuf.dsize != sizeof(crec))
		return 0;

	memcpy(&crec, dbuf.dptr, sizeof(crec));

	if (crec.cnum == -1)
		return 0;

	if (!process_exists(crec.pid) || !Ucrit_checkUid(crec.uid)) {
		return 0;
	}

	d_printf("%-10s   %s   %-12s  %s",
	       crec.servicename,procid_str_static(&crec.pid),
	       crec.machine,
	       time_to_asc(crec.start));

	return 0;
}

static int traverse_sessionid(TDB_CONTEXT *tdb, TDB_DATA kbuf, TDB_DATA dbuf, void *state)
{
	struct sessionid sessionid;
	fstring uid_str, gid_str;

	if (dbuf.dsize != sizeof(sessionid))
		return 0;

	memcpy(&sessionid, dbuf.dptr, sizeof(sessionid));

	if (!process_exists_by_pid(sessionid.pid) || !Ucrit_checkUid(sessionid.uid)) {
		return 0;
	}

	Ucrit_addPid( sessionid.pid );

	fstr_sprintf(uid_str, "%d", sessionid.uid);
	fstr_sprintf(gid_str, "%d", sessionid.gid);

	d_printf("%5d   %-12s  %-12s  %-12s (%s)\n",
		 (int)sessionid.pid,
		 numeric_only ? uid_str : uidtoname(sessionid.uid),
		 numeric_only ? gid_str : gidtoname(sessionid.gid), 
		 sessionid.remote_machine, sessionid.hostname);
	
	return 0;
}




 int main(int argc, char *argv[])
{
	int c;
	int profile_only = 0;
	TDB_CONTEXT *tdb;
	BOOL show_processes, show_locks, show_shares;
	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"processes",	'p', POPT_ARG_NONE,	&processes_only, 'p', "Show processes only" },
		{"verbose",	'v', POPT_ARG_NONE, &verbose, 'v', "Be verbose" },
		{"locks",	'L', POPT_ARG_NONE,	&locks_only, 'L', "Show locks only" },
		{"shares",	'S', POPT_ARG_NONE,	&shares_only, 'S', "Show shares only" },
		{"user", 	'u', POPT_ARG_STRING,	&username, 'u', "Switch to user" },
		{"brief",	'b', POPT_ARG_NONE, 	&brief, 'b', "Be brief" },
		{"profile",     'P', POPT_ARG_NONE, NULL, 'P', "Do profiling" },
		{"profile-rates", 'R', POPT_ARG_NONE, NULL, 'R', "Show call rates" },
		{"byterange",	'B', POPT_ARG_NONE,	&show_brl, 'B', "Include byte range locks"},
		{"numeric",	'n', POPT_ARG_NONE,	&numeric_only, 'n', "Numeric uid/gid"},
		POPT_COMMON_SAMBA
		POPT_TABLEEND
	};

	sec_init();
	load_case_tables();

	setup_logging(argv[0],True);
	
	dbf = x_stderr;
	
	if (getuid() != geteuid()) {
		d_printf("smbstatus should not be run setuid\n");
		return(1);
	}

	pc = poptGetContext(NULL, argc, (const char **) argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);
	
	while ((c = poptGetNextOpt(pc)) != -1) {
		switch (c) {
		case 'u':                                      
			Ucrit_addUid(nametouid(poptGetOptArg(pc)));
			break;
		case 'P':
		case 'R':
			profile_only = c;
		}
	}

	/* setup the flags based on the possible combincations */

	show_processes = !(shares_only || locks_only || profile_only) || processes_only;
	show_locks     = !(shares_only || processes_only || profile_only) || locks_only;
	show_shares    = !(processes_only || locks_only || profile_only) || shares_only;

	if ( username )
		Ucrit_addUid( nametouid(username) );

	if (verbose) {
		d_printf("using configfile = %s\n", dyn_CONFIGFILE);
	}

	if (!lp_load(dyn_CONFIGFILE,False,False,False,True)) {
		fprintf(stderr, "Can't load %s - run testparm to debug it\n", dyn_CONFIGFILE);
		return (-1);
	}

	switch (profile_only) {
		case 'P':
			/* Dump profile data */
			return status_profile_dump(verbose);
		case 'R':
			/* Continuously display rate-converted data */
			return status_profile_rates(verbose);
		default:
			break;
	}

	if ( show_processes ) {
		tdb = tdb_open_log(lock_path("sessionid.tdb"), 0, TDB_DEFAULT, O_RDONLY, 0);
		if (!tdb) {
			d_printf("sessionid.tdb not initialised\n");
		} else {
			d_printf("\nSamba version %s\n",SAMBA_VERSION_STRING);
			d_printf("PID     Username      Group         Machine                        \n");
			d_printf("-------------------------------------------------------------------\n");

			tdb_traverse(tdb, traverse_sessionid, NULL);
			tdb_close(tdb);
		}

		if (processes_only) 
			exit(0);	
	}
  
	if ( show_shares ) {
		tdb = tdb_open_log(lock_path("connections.tdb"), 0, TDB_DEFAULT, O_RDONLY, 0);
		if (!tdb) {
			d_printf("%s not initialised\n", lock_path("connections.tdb"));
			d_printf("This is normal if an SMB client has never connected to your server.\n");
		}  else  {
			if (verbose) {
				d_printf("Opened %s\n", lock_path("connections.tdb"));
			}

			if (brief) 
				exit(0);
		
			d_printf("\nService      pid     machine       Connected at\n");
			d_printf("-------------------------------------------------------\n");
	
			tdb_traverse(tdb, traverse_fn1, NULL);
			tdb_close(tdb);

			d_printf("\n");
		}

		if ( shares_only )
			exit(0);
	}

	if ( show_locks ) {
		int ret;

		tdb = tdb_open_log(lock_path("locking.tdb"), 0, TDB_DEFAULT, O_RDONLY, 0);

		if (!tdb) {
			d_printf("%s not initialised\n", lock_path("locking.tdb"));
			d_printf("This is normal if an SMB client has never connected to your server.\n");
			exit(0);
		} else {
			tdb_close(tdb);
		}

		if (!locking_init(1)) {
			d_printf("Can't initialise locking module - exiting\n");
			exit(1);
		}
		
		ret = share_mode_forall(print_share_mode, NULL);

		if (ret == 0) {
			d_printf("No locked files\n");
		} else if (ret == -1) {
			d_printf("locked file list truncated\n");
		}
		
		d_printf("\n");

		if (show_brl) {
			brl_forall(print_brl);
		}
		
		locking_end();
	}

	return (0);
}

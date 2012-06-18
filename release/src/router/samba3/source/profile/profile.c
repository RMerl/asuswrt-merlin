/* 
   Unix SMB/CIFS implementation.
   store smbd profiling information in shared memory
   Copyright (C) Andrew Tridgell 1999
   Copyright (C) James Peach 2006

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

#ifdef WITH_PROFILE
#define IPC_PERMS ((S_IRUSR | S_IWUSR) | S_IRGRP | S_IROTH)
#endif /* WITH_PROFILE */

#ifdef WITH_PROFILE
static int shm_id;
static BOOL read_only;
#if defined(HAVE_CLOCK_GETTIME)
clockid_t __profile_clock;
BOOL have_profiling_clock = False;
#endif
#endif

struct profile_header *profile_h;
struct profile_stats *profile_p;

BOOL do_profile_flag = False;
BOOL do_profile_times = False;

/****************************************************************************
Set a profiling level.
****************************************************************************/
void set_profile_level(int level, struct process_id src)
{
#ifdef WITH_PROFILE
	switch (level) {
	case 0:		/* turn off profiling */
		do_profile_flag = False;
		do_profile_times = False;
		DEBUG(1,("INFO: Profiling turned OFF from pid %d\n",
			 (int)procid_to_pid(&src)));
		break;
	case 1:		/* turn on counter profiling only */
		do_profile_flag = True;
		do_profile_times = False;
		DEBUG(1,("INFO: Profiling counts turned ON from pid %d\n",
			 (int)procid_to_pid(&src)));
		break;
	case 2:		/* turn on complete profiling */

#if defined(HAVE_CLOCK_GETTIME)
		if (!have_profiling_clock) {
			do_profile_flag = True;
			do_profile_times = False;
			DEBUG(1,("INFO: Profiling counts turned ON from "
				"pid %d\n", (int)procid_to_pid(&src)));
			DEBUGADD(1,("INFO: Profiling times disabled "
				"due to lack of a suitable clock\n"));
			break;
		}
#endif

		do_profile_flag = True;
		do_profile_times = True;
		DEBUG(1,("INFO: Full profiling turned ON from pid %d\n",
			 (int)procid_to_pid(&src)));
		break;
	case 3:		/* reset profile values */
		memset((char *)profile_p, 0, sizeof(*profile_p));
		DEBUG(1,("INFO: Profiling values cleared from pid %d\n",
			 (int)procid_to_pid(&src)));
		break;
	}
#else /* WITH_PROFILE */
	DEBUG(1,("INFO: Profiling support unavailable in this build.\n"));
#endif /* WITH_PROFILE */
}

/****************************************************************************
receive a set profile level message
****************************************************************************/
void profile_message(int msg_type, struct process_id src, void *buf, size_t len, void *private_data)
{
        int level;

	memcpy(&level, buf, sizeof(int));
	set_profile_level(level, src);
}

/****************************************************************************
receive a request profile level message
****************************************************************************/
void reqprofile_message(int msg_type, struct process_id src,
			void *buf, size_t len, void *private_data)
{
        int level;

#ifdef WITH_PROFILE
	level = 1 + (do_profile_flag?2:0) + (do_profile_times?4:0);
#else
	level = 0;
#endif
	DEBUG(1,("INFO: Received REQ_PROFILELEVEL message from PID %u\n",
		 (unsigned int)procid_to_pid(&src)));
	message_send_pid(src, MSG_PROFILELEVEL, &level, sizeof(int), True);
}

/*******************************************************************
  open the profiling shared memory area
  ******************************************************************/
#ifdef WITH_PROFILE

#ifdef HAVE_CLOCK_GETTIME

/* Find a clock. Just because the definition for a particular clock ID is
 * present doesn't mean the system actually supports it.
 */
static void init_clock_gettime(void)
{
	struct timespec ts;

	have_profiling_clock = False;

#ifdef HAVE_CLOCK_PROCESS_CPUTIME_ID
	/* CLOCK_PROCESS_CPUTIME_ID is sufficiently fast that the
	 * always profiling times is plausible. Unfortunately on Linux
	 * it is only accurate if we can guarantee we will not be scheduled
	 * scheduled onto a different CPU between samples. Until there is
	 * some way to set processor affinity, we can only use this on
	 * uniprocessors.
	 */
	if (!this_is_smp()) {
	    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
		    DEBUG(10, ("Using CLOCK_PROCESS_CPUTIME_ID "
				"for profile_clock\n"));
		    __profile_clock = CLOCK_PROCESS_CPUTIME_ID;
		    have_profiling_clock = True;
	    }
	}
#endif

#ifdef HAVE_CLOCK_MONOTONIC
	if (!have_profiling_clock &&
	    clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		DEBUG(10, ("Using CLOCK_MONOTONIC for profile_clock\n"));
		__profile_clock = CLOCK_MONOTONIC;
		have_profiling_clock = True;
	}
#endif

#ifdef HAVE_CLOCK_REALTIME
	/* POSIX says that CLOCK_REALTIME should be defined everywhere
	 * where we have clock_gettime...
	 */
	if (!have_profiling_clock &&
	    clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		__profile_clock = CLOCK_REALTIME;
		have_profiling_clock = True;

		SMB_WARN(__profile_clock != CLOCK_REALTIME,
			("forced to use a slow profiling clock"));
	}

#endif

	SMB_WARN(have_profiling_clock == True,
		("could not find a working clock for profiling"));
	return;
}
#endif

BOOL profile_setup(BOOL rdonly)
{
	struct shmid_ds shm_ds;

	read_only = rdonly;

#ifdef HAVE_CLOCK_GETTIME
	init_clock_gettime();
#endif

 again:
	/* try to use an existing key */
	shm_id = shmget(PROF_SHMEM_KEY, 0, 0);
	
	/* if that failed then create one. There is a race condition here
	   if we are running from inetd. Bad luck. */
	if (shm_id == -1) {
		if (read_only) return False;
		shm_id = shmget(PROF_SHMEM_KEY, sizeof(*profile_h), 
				IPC_CREAT | IPC_EXCL | IPC_PERMS);
	}
	
	if (shm_id == -1) {
		DEBUG(0,("Can't create or use IPC area. Error was %s\n", 
			 strerror(errno)));
		return False;
	}   
	
	
	profile_h = (struct profile_header *)shmat(shm_id, 0, 
						   read_only?SHM_RDONLY:0);
	if ((long)profile_p == -1) {
		DEBUG(0,("Can't attach to IPC area. Error was %s\n", 
			 strerror(errno)));
		return False;
	}

	/* find out who created this memory area */
	if (shmctl(shm_id, IPC_STAT, &shm_ds) != 0) {
		DEBUG(0,("ERROR shmctl : can't IPC_STAT. Error was %s\n", 
			 strerror(errno)));
		return False;
	}

	if (shm_ds.shm_perm.cuid != sec_initial_uid() ||
	    shm_ds.shm_perm.cgid != sec_initial_gid()) {
		DEBUG(0,("ERROR: we did not create the shmem "
			 "(owned by another user, uid %u, gid %u)\n",
			 shm_ds.shm_perm.cuid,
			 shm_ds.shm_perm.cgid));
		return False;
	}

	if (shm_ds.shm_segsz != sizeof(*profile_h)) {
		DEBUG(0,("WARNING: profile size is %d (expected %lu). Deleting\n",
			 (int)shm_ds.shm_segsz, sizeof(*profile_h)));
		if (shmctl(shm_id, IPC_RMID, &shm_ds) == 0) {
			goto again;
		} else {
			return False;
		}
	}

	if (!read_only && (shm_ds.shm_nattch == 1)) {
		memset((char *)profile_h, 0, sizeof(*profile_h));
		profile_h->prof_shm_magic = PROF_SHM_MAGIC;
		profile_h->prof_shm_version = PROF_SHM_VERSION;
		DEBUG(3,("Initialised profile area\n"));
	}

	profile_p = &profile_h->stats;
	message_register(MSG_PROFILE, profile_message, NULL);
	message_register(MSG_REQ_PROFILELEVEL, reqprofile_message, NULL);
	return True;
}

 const char * profile_value_name(enum profile_stats_values val)
{
	static const char * valnames[PR_VALUE_MAX + 1] =
	{
	    "smbd_idle",		/* PR_VALUE_SMBD_IDLE */
	    "syscall_opendir",		/* PR_VALUE_SYSCALL_OPENDIR */
	    "syscall_readdir",		/* PR_VALUE_SYSCALL_READDIR */
	    "syscall_seekdir",		/* PR_VALUE_SYSCALL_SEEKDIR */
	    "syscall_telldir",		/* PR_VALUE_SYSCALL_TELLDIR */
	    "syscall_rewinddir",	/* PR_VALUE_SYSCALL_REWINDDIR */
	    "syscall_mkdir",		/* PR_VALUE_SYSCALL_MKDIR */
	    "syscall_rmdir",		/* PR_VALUE_SYSCALL_RMDIR */
	    "syscall_closedir",		/* PR_VALUE_SYSCALL_CLOSEDIR */
	    "syscall_open",		/* PR_VALUE_SYSCALL_OPEN */
	    "syscall_close",		/* PR_VALUE_SYSCALL_CLOSE */
	    "syscall_read",		/* PR_VALUE_SYSCALL_READ */
	    "syscall_pread",		/* PR_VALUE_SYSCALL_PREAD */
	    "syscall_write",		/* PR_VALUE_SYSCALL_WRITE */
	    "syscall_pwrite",		/* PR_VALUE_SYSCALL_PWRITE */
	    "syscall_lseek",		/* PR_VALUE_SYSCALL_LSEEK */
	    "syscall_sendfile",		/* PR_VALUE_SYSCALL_SENDFILE */
	    "syscall_rename",		/* PR_VALUE_SYSCALL_RENAME */
	    "syscall_fsync",		/* PR_VALUE_SYSCALL_FSYNC */
	    "syscall_stat",		/* PR_VALUE_SYSCALL_STAT */
	    "syscall_fstat",		/* PR_VALUE_SYSCALL_FSTAT */
	    "syscall_lstat",		/* PR_VALUE_SYSCALL_LSTAT */
	    "syscall_unlink",		/* PR_VALUE_SYSCALL_UNLINK */
	    "syscall_chmod",		/* PR_VALUE_SYSCALL_CHMOD */
	    "syscall_fchmod",		/* PR_VALUE_SYSCALL_FCHMOD */
	    "syscall_chown",		/* PR_VALUE_SYSCALL_CHOWN */
	    "syscall_fchown",		/* PR_VALUE_SYSCALL_FCHOWN */
	    "syscall_chdir",		/* PR_VALUE_SYSCALL_CHDIR */
	    "syscall_getwd",		/* PR_VALUE_SYSCALL_GETWD */
	    "syscall_ntimes",		/* PR_VALUE_SYSCALL_NTIMES */
	    "syscall_ftruncate",	/* PR_VALUE_SYSCALL_FTRUNCATE */
	    "syscall_fcntl_lock",	/* PR_VALUE_SYSCALL_FCNTL_LOCK */
	    "syscall_kernel_flock",     /* PR_VALUE_SYSCALL_KERNEL_FLOCK */
	    "syscall_linux_setlease",   /* PR_VALUE_SYSCALL_LINUX_SETLEASE */
	    "syscall_fcntl_getlock",	/* PR_VALUE_SYSCALL_FCNTL_GETLOCK */
	    "syscall_readlink",		/* PR_VALUE_SYSCALL_READLINK */
	    "syscall_symlink",		/* PR_VALUE_SYSCALL_SYMLINK */
	    "syscall_link",		/* PR_VALUE_SYSCALL_LINK */
	    "syscall_mknod",		/* PR_VALUE_SYSCALL_MKNOD */
	    "syscall_realpath",		/* PR_VALUE_SYSCALL_REALPATH */
	    "syscall_get_quota",	/* PR_VALUE_SYSCALL_GET_QUOTA */
	    "syscall_set_quota",	/* PR_VALUE_SYSCALL_SET_QUOTA */
	    "SMBmkdir",		/* PR_VALUE_SMBMKDIR */
	    "SMBrmdir",		/* PR_VALUE_SMBRMDIR */
	    "SMBopen",		/* PR_VALUE_SMBOPEN */
	    "SMBcreate",	/* PR_VALUE_SMBCREATE */
	    "SMBclose",		/* PR_VALUE_SMBCLOSE */
	    "SMBflush",		/* PR_VALUE_SMBFLUSH */
	    "SMBunlink",	/* PR_VALUE_SMBUNLINK */
	    "SMBmv",		/* PR_VALUE_SMBMV */
	    "SMBgetatr",	/* PR_VALUE_SMBGETATR */
	    "SMBsetatr",	/* PR_VALUE_SMBSETATR */
	    "SMBread",		/* PR_VALUE_SMBREAD */
	    "SMBwrite",		/* PR_VALUE_SMBWRITE */
	    "SMBlock",		/* PR_VALUE_SMBLOCK */
	    "SMBunlock",	/* PR_VALUE_SMBUNLOCK */
	    "SMBctemp",		/* PR_VALUE_SMBCTEMP */
	    "SMBmknew",		/* PR_VALUE_SMBMKNEW */
	    "SMBcheckpath",	/* PR_VALUE_SMBCHECKPATH */
	    "SMBexit",		/* PR_VALUE_SMBEXIT */
	    "SMBlseek",		/* PR_VALUE_SMBLSEEK */
	    "SMBlockread",		/* PR_VALUE_SMBLOCKREAD */
	    "SMBwriteunlock",		/* PR_VALUE_SMBWRITEUNLOCK */
	    "SMBreadbraw",		/* PR_VALUE_SMBREADBRAW */
	    "SMBreadBmpx",		/* PR_VALUE_SMBREADBMPX */
	    "SMBreadBs",		/* PR_VALUE_SMBREADBS */
	    "SMBwritebraw",		/* PR_VALUE_SMBWRITEBRAW */
	    "SMBwriteBmpx",		/* PR_VALUE_SMBWRITEBMPX */
	    "SMBwriteBs",		/* PR_VALUE_SMBWRITEBS */
	    "SMBwritec",		/* PR_VALUE_SMBWRITEC */
	    "SMBsetattrE",		/* PR_VALUE_SMBSETATTRE */
	    "SMBgetattrE",		/* PR_VALUE_SMBGETATTRE */
	    "SMBlockingX",		/* PR_VALUE_SMBLOCKINGX */
	    "SMBtrans",		/* PR_VALUE_SMBTRANS */
	    "SMBtranss",	/* PR_VALUE_SMBTRANSS */
	    "SMBioctl",		/* PR_VALUE_SMBIOCTL */
	    "SMBioctls",	/* PR_VALUE_SMBIOCTLS */
	    "SMBcopy",		/* PR_VALUE_SMBCOPY */
	    "SMBmove",		/* PR_VALUE_SMBMOVE */
	    "SMBecho",		/* PR_VALUE_SMBECHO */
	    "SMBwriteclose",	/* PR_VALUE_SMBWRITECLOSE */
	    "SMBopenX",		/* PR_VALUE_SMBOPENX */
	    "SMBreadX",		/* PR_VALUE_SMBREADX */
	    "SMBwriteX",	/* PR_VALUE_SMBWRITEX */
	    "SMBtrans2",	/* PR_VALUE_SMBTRANS2 */
	    "SMBtranss2",	/* PR_VALUE_SMBTRANSS2 */
	    "SMBfindclose",	/* PR_VALUE_SMBFINDCLOSE */
	    "SMBfindnclose",	/* PR_VALUE_SMBFINDNCLOSE */
	    "SMBtcon",		/* PR_VALUE_SMBTCON */
	    "SMBtdis",		/* PR_VALUE_SMBTDIS */
	    "SMBnegprot",	/* PR_VALUE_SMBNEGPROT */
	    "SMBsesssetupX",	/* PR_VALUE_SMBSESSSETUPX */
	    "SMBulogoffX",	/* PR_VALUE_SMBULOGOFFX */
	    "SMBtconX",		/* PR_VALUE_SMBTCONX */
	    "SMBdskattr",		/* PR_VALUE_SMBDSKATTR */
	    "SMBsearch",		/* PR_VALUE_SMBSEARCH */
	    "SMBffirst",		/* PR_VALUE_SMBFFIRST */
	    "SMBfunique",		/* PR_VALUE_SMBFUNIQUE */
	    "SMBfclose",		/* PR_VALUE_SMBFCLOSE */
	    "SMBnttrans",		/* PR_VALUE_SMBNTTRANS */
	    "SMBnttranss",		/* PR_VALUE_SMBNTTRANSS */
	    "SMBntcreateX",		/* PR_VALUE_SMBNTCREATEX */
	    "SMBntcancel",		/* PR_VALUE_SMBNTCANCEL */
	    "SMBntrename",		/* PR_VALUE_SMBNTRENAME */
	    "SMBsplopen",		/* PR_VALUE_SMBSPLOPEN */
	    "SMBsplwr",			/* PR_VALUE_SMBSPLWR */
	    "SMBsplclose",		/* PR_VALUE_SMBSPLCLOSE */
	    "SMBsplretq",		/* PR_VALUE_SMBSPLRETQ */
	    "SMBsends",			/* PR_VALUE_SMBSENDS */
	    "SMBsendb",			/* PR_VALUE_SMBSENDB */
	    "SMBfwdname",		/* PR_VALUE_SMBFWDNAME */
	    "SMBcancelf",		/* PR_VALUE_SMBCANCELF */
	    "SMBgetmac",		/* PR_VALUE_SMBGETMAC */
	    "SMBsendstrt",		/* PR_VALUE_SMBSENDSTRT */
	    "SMBsendend",		/* PR_VALUE_SMBSENDEND */
	    "SMBsendtxt",		/* PR_VALUE_SMBSENDTXT */
	    "SMBinvalid",		/* PR_VALUE_SMBINVALID */
	    "pathworks_setdir",		/* PR_VALUE_PATHWORKS_SETDIR */
	    "Trans2_open",		/* PR_VALUE_TRANS2_OPEN */
	    "Trans2_findfirst",		/* PR_VALUE_TRANS2_FINDFIRST */
	    "Trans2_findnext",		/* PR_VALUE_TRANS2_FINDNEXT */
	    "Trans2_qfsinfo",		/* PR_VALUE_TRANS2_QFSINFO */
	    "Trans2_setfsinfo",		/* PR_VALUE_TRANS2_SETFSINFO */
	    "Trans2_qpathinfo",		/* PR_VALUE_TRANS2_QPATHINFO */
	    "Trans2_setpathinfo",	/* PR_VALUE_TRANS2_SETPATHINFO */
	    "Trans2_qfileinfo",		/* PR_VALUE_TRANS2_QFILEINFO */
	    "Trans2_setfileinfo",	/* PR_VALUE_TRANS2_SETFILEINFO */
	    "Trans2_fsctl",		/* PR_VALUE_TRANS2_FSCTL */
	    "Trans2_ioctl",		/* PR_VALUE_TRANS2_IOCTL */
	    "Trans2_findnotifyfirst",	/* PR_VALUE_TRANS2_FINDNOTIFYFIRST */
	    "Trans2_findnotifynext",	/* PR_VALUE_TRANS2_FINDNOTIFYNEXT */
	    "Trans2_mkdir",		/* PR_VALUE_TRANS2_MKDIR */
	    "Trans2_session_setup",	/* PR_VALUE_TRANS2_SESSION_SETUP */
	    "Trans2_get_dfs_referral",	/* PR_VALUE_TRANS2_GET_DFS_REFERRAL */
	    "Trans2_report_dfs_inconsistancy",	/* PR_VALUE_TRANS2_REPORT_DFS_INCONSISTANCY */
	    "NT_transact_create",	/* PR_VALUE_NT_TRANSACT_CREATE */
	    "NT_transact_ioctl",	/* PR_VALUE_NT_TRANSACT_IOCTL */
	    "NT_transact_set_security_desc",	/* PR_VALUE_NT_TRANSACT_SET_SECURITY_DESC */
	    "NT_transact_notify_change",/* PR_VALUE_NT_TRANSACT_NOTIFY_CHANGE */
	    "NT_transact_rename",	/* PR_VALUE_NT_TRANSACT_RENAME */
	    "NT_transact_query_security_desc",	/* PR_VALUE_NT_TRANSACT_QUERY_SECURITY_DESC */
	    "NT_transact_get_user_quota",/* PR_VALUE_NT_TRANSACT_GET_USER_QUOTA */
	    "NT_transact_set_user_quota",/* PR_VALUE_NT_TRANSACT_SET_USER_QUOTA */
	    "get_nt_acl",		/* PR_VALUE_GET_NT_ACL */
	    "fget_nt_acl",		/* PR_VALUE_FGET_NT_ACL */
	    "set_nt_acl",		/* PR_VALUE_SET_NT_ACL */
	    "fset_nt_acl",		/* PR_VALUE_FSET_NT_ACL */
	    "chmod_acl",		/* PR_VALUE_CHMOD_ACL */
	    "fchmod_acl",		/* PR_VALUE_FCHMOD_ACL */
	    "name_release",		/* PR_VALUE_NAME_RELEASE */
	    "name_refresh",		/* PR_VALUE_NAME_REFRESH */
	    "name_registration",	/* PR_VALUE_NAME_REGISTRATION */
	    "node_status",		/* PR_VALUE_NODE_STATUS */
	    "name_query",		/* PR_VALUE_NAME_QUERY */
	    "host_announce",		/* PR_VALUE_HOST_ANNOUNCE */
	    "workgroup_announce",	/* PR_VALUE_WORKGROUP_ANNOUNCE */
	    "local_master_announce",	/* PR_VALUE_LOCAL_MASTER_ANNOUNCE */
	    "master_browser_announce",	/* PR_VALUE_MASTER_BROWSER_ANNOUNCE */
	    "lm_host_announce",		/* PR_VALUE_LM_HOST_ANNOUNCE */
	    "get_backup_list",		/* PR_VALUE_GET_BACKUP_LIST */
	    "reset_browser",		/* PR_VALUE_RESET_BROWSER */
	    "announce_request",		/* PR_VALUE_ANNOUNCE_REQUEST */
	    "lm_announce_request",	/* PR_VALUE_LM_ANNOUNCE_REQUEST */
	    "domain_logon",		/* PR_VALUE_DOMAIN_LOGON */
	    "sync_browse_lists",	/* PR_VALUE_SYNC_BROWSE_LISTS */
	    "run_elections",		/* PR_VALUE_RUN_ELECTIONS */
	    "election",			/* PR_VALUE_ELECTION */
	    "" /* PR_VALUE_MAX */
	};

	SMB_ASSERT(val >= 0);
	SMB_ASSERT(val < PR_VALUE_MAX);
	return valnames[val];
}

#endif /* WITH_PROFILE */

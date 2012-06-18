/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   file opening and share modes
   Copyright (C) Andrew Tridgell 1992-1998
   
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

extern int DEBUGLEVEL;

extern pstring sesssetup_user;
extern uint16 global_oplock_port;
extern BOOL global_client_failed_oplock_break;

/****************************************************************************
fd support routines - attempt to do a dos_open
****************************************************************************/

static int fd_attempt_open(char *fname, int flags, mode_t mode)
{
  int fd = dos_open(fname,flags,mode);

  /* Fix for files ending in '.' */
  if((fd == -1) && (errno == ENOENT) &&
     (strchr(fname,'.')==NULL))
    {
      pstrcat(fname,".");
      fd = dos_open(fname,flags,mode);
    }

#if (defined(ENAMETOOLONG) && defined(HAVE_PATHCONF))
  if ((fd == -1) && (errno == ENAMETOOLONG))
    {
      int max_len;
      char *p = strrchr(fname, '/');

      if (p == fname)   /* name is "/xxx" */
        {
          max_len = pathconf("/", _PC_NAME_MAX);
          p++;
        }
      else if ((p == NULL) || (p == fname))
        {
          p = fname;
          max_len = pathconf(".", _PC_NAME_MAX);
        }
      else
        {
          *p = '\0';
          max_len = pathconf(fname, _PC_NAME_MAX);
          *p = '/';
          p++;
        }
      if (strlen(p) > max_len)
        {
          char tmp = p[max_len];

          p[max_len] = '\0';
          if ((fd = dos_open(fname,flags,mode)) == -1)
            p[max_len] = tmp;
        }
    }
#endif
  return fd;
}

/****************************************************************************
Cache a uid_t currently with this file open. This is an optimization only
used when multiple sessionsetup's have been done to one smbd.
****************************************************************************/

void fd_add_to_uid_cache(file_fd_struct *fd_ptr, uid_t u)
{
  if(fd_ptr->uid_cache_count >= sizeof(fd_ptr->uid_users_cache)/sizeof(uid_t))
    return;
  fd_ptr->uid_users_cache[fd_ptr->uid_cache_count++] = u;
}

/****************************************************************************
Remove a uid_t that currently has this file open. This is an optimization only
used when multiple sessionsetup's have been done to one smbd.
****************************************************************************/

static void fd_remove_from_uid_cache(file_fd_struct *fd_ptr, uid_t u)
{
  int i;
  for(i = 0; i < fd_ptr->uid_cache_count; i++)
    if(fd_ptr->uid_users_cache[i] == u) {
      if(i < (fd_ptr->uid_cache_count-1))
        memmove((char *)&fd_ptr->uid_users_cache[i], (char *)&fd_ptr->uid_users_cache[i+1],
               sizeof(uid_t)*(fd_ptr->uid_cache_count-1-i) );
      fd_ptr->uid_cache_count--;
    }
  return;
}

/****************************************************************************
Check if a uid_t that currently has this file open is present. This is an
optimization only used when multiple sessionsetup's have been done to one smbd.
****************************************************************************/

static BOOL fd_is_in_uid_cache(file_fd_struct *fd_ptr, uid_t u)
{
  int i;
  for(i = 0; i < fd_ptr->uid_cache_count; i++)
    if(fd_ptr->uid_users_cache[i] == u)
      return True;
  return False;
}

/****************************************************************************
fd support routines - attempt to re-open an already open fd as O_RDWR.
Save the already open fd (we cannot close due to POSIX file locking braindamage.
****************************************************************************/

static void fd_attempt_reopen(char *fname, mode_t mode, file_fd_struct *fd_ptr)
{
  int fd = dos_open( fname, O_RDWR, mode);

  if(fd == -1)
    return;

  if(fd_ptr->real_open_flags == O_RDONLY)
    fd_ptr->fd_readonly = fd_ptr->fd;
  if(fd_ptr->real_open_flags == O_WRONLY)
    fd_ptr->fd_writeonly = fd_ptr->fd;

  fd_ptr->fd = fd;
  fd_ptr->real_open_flags = O_RDWR;
}

/****************************************************************************
fd support routines - attempt to close the file referenced by this fd.
Decrements the ref_count and returns it.
****************************************************************************/

uint16 fd_attempt_close(file_fd_struct *fd_ptr, int *err_ret)
{
  extern struct current_user current_user;
  uint16 ret_ref = fd_ptr->ref_count;

  *err_ret = 0;

  DEBUG(3,("fd_attempt_close fd = %d, dev = %x, inode = %.0f, open_flags = %d, ref_count = %d.\n",
          fd_ptr->fd, (unsigned int)fd_ptr->dev, (double)fd_ptr->inode,
          fd_ptr->real_open_flags,
          fd_ptr->ref_count));

  SMB_ASSERT(fd_ptr->ref_count != 0);

  fd_ptr->ref_count--;
  ret_ref = fd_ptr->ref_count;

  if(fd_ptr->ref_count == 0) {

    if(fd_ptr->fd != -1) {
      if(close(fd_ptr->fd) < 0)
        *err_ret = errno;
	}

    if(fd_ptr->fd_readonly != -1) {
      if(close(fd_ptr->fd_readonly) < 0) {
        if(*err_ret == 0)
          *err_ret = errno;
      }
	}

    if(fd_ptr->fd_writeonly != -1) {
      if( close(fd_ptr->fd_writeonly) < 0) {
        if(*err_ret == 0)
          *err_ret = errno;
      }
	}

    /*
     * Delete this fd_ptr.
     */
    fd_ptr_free(fd_ptr);
  } else {
    fd_remove_from_uid_cache(fd_ptr, (uid_t)current_user.uid);
  }

  return ret_ref;
}

/****************************************************************************
 Shutdown a half-open filestruct. This allows open_file_shared to back out
 on error.
****************************************************************************/

static void close_on_error(files_struct *fsp, connection_struct *conn)
{
  int err;

  close_filestruct(fsp);

  fd_attempt_close(fsp->fd_ptr,&err);

  fsp->fd_ptr = NULL;

  if (fsp->fsp_name) {
    string_free(&fsp->fsp_name);
  }
}

/****************************************************************************
fd support routines - check that current user has permissions
to open this file. Used when uid not found in optimization cache.
This is really ugly code, as due to POSIX locking braindamage we must
fork and then attempt to open the file, and return success or failure
via an exit code.
****************************************************************************/

static BOOL check_access_allowed_for_current_user( char *fname, int accmode )
{
  pid_t child_pid;

  /*
   * We need to temporarily stop CatchChild from eating 
   * SIGCLD signals as it also eats the exit status code. JRA.
   */

  CatchChildLeaveStatus();

  if((child_pid = fork()) < 0) {
    DEBUG(0,("check_access_allowed_for_current_user: fork failed.\n"));
    CatchChild();
    return False;
  }

  if(child_pid) {
    /*
     * Parent.
     */
    pid_t wpid;
    int status_code;

    while ((wpid = sys_waitpid(child_pid, &status_code, 0)) < 0) {
      if(errno == EINTR) {
        errno = 0;
        continue;
      }
      DEBUG(0,("check_access_allowed_for_current_user: The process \
is no longer waiting ! Error = %s\n", strerror(errno) ));
      CatchChild();
      return(False);
    }

	/*
	 * Go back to ignoring children.
	 */
	CatchChild();

    if (child_pid != wpid) {
      DEBUG(0,("check_access_allowed_for_current_user: We were waiting for the wrong process ID\n"));
      return(False);
    }
#if defined(WIFEXITED) && defined(WEXITSTATUS)
    if (WIFEXITED(status_code) == 0) {
      DEBUG(0,("check_access_allowed_for_current_user: The process exited while we were waiting\n"));
      return(False);
    }
    if (WEXITSTATUS(status_code) != 0) {
      DEBUG(9,("check_access_allowed_for_current_user: The status of the process exiting was %d. Returning access denied.\n", status_code));
      return(False);
    }
#else /* defined(WIFEXITED) && defined(WEXITSTATUS) */
    if(status_code != 0) {
      DEBUG(9,("check_access_allowed_for_current_user: The status of the process exiting was %d. Returning access denied.\n", status_code));
      return(False);
    }
#endif /* defined(WIFEXITED) && defined(WEXITSTATUS) */

    /*
     * Success - the child could open the file.
     */
    DEBUG(9,("check_access_allowed_for_current_user: The status of the process exiting was %d. Returning access allowed.\n", status_code));
    return True;
  } else {
    /*
     * Child.
     */
    int fd;
    DEBUG(9,("check_access_allowed_for_current_user: Child - attempting to open %s with mode %d.\n", fname, accmode ));
    if((fd = fd_attempt_open( fname, accmode, 0)) < 0) {
      /* Access denied. */
      _exit(EACCES);
    }
    close(fd);
    DEBUG(9,("check_access_allowed_for_current_user: Child - returning ok.\n"));
    _exit(0);
  }

  return False;
}

/****************************************************************************
check a filename for the pipe string
****************************************************************************/

static void check_for_pipe(char *fname)
{
	/* special case of pipe opens */
	char s[10];
	StrnCpy(s,fname,sizeof(s)-1);
	strlower(s);
	if (strstr(s,"pipe/")) {
		DEBUG(3,("Rejecting named pipe open for %s\n",fname));
		unix_ERR_class = ERRSRV;
		unix_ERR_code = ERRaccess;
	}
}

/****************************************************************************
open a file
****************************************************************************/

static void open_file(files_struct *fsp,connection_struct *conn,
		      char *fname1,int flags,mode_t mode, SMB_STRUCT_STAT *sbuf)
{
  extern struct current_user current_user;
  pstring fname;
  SMB_STRUCT_STAT statbuf;
  file_fd_struct *fd_ptr;
  int accmode = (flags & O_ACCMODE);

  fsp->open = False;
  fsp->fd_ptr = 0;
  fsp->oplock_type = NO_OPLOCK;
  errno = EPERM;

  pstrcpy(fname,fname1);

  /* check permissions */

  /*
   * This code was changed after seeing a client open request 
   * containing the open mode of (DENY_WRITE/read-only) with
   * the 'create if not exist' bit set. The previous code
   * would fail to open the file read only on a read-only share
   * as it was checking the flags parameter  directly against O_RDONLY,
   * this was failing as the flags parameter was set to O_RDONLY|O_CREAT.
   * JRA.
   */

  if (!CAN_WRITE(conn) && !conn->printer) {
    /* It's a read-only share - fail if we wanted to write. */
    if(accmode != O_RDONLY) {
      DEBUG(3,("Permission denied opening %s\n",fname));
      check_for_pipe(fname);
      return;
    } else if(flags & O_CREAT) {
      /* We don't want to write - but we must make sure that O_CREAT
         doesn't create the file if we have write access into the
         directory.
       */
      flags &= ~O_CREAT;
    }
  }

  /* this handles a bug in Win95 - it doesn't say to create the file when it 
     should */
  if (conn->printer) {
	  flags |= (O_CREAT|O_EXCL);
  }

/*
  if (flags == O_WRONLY)
    DEBUG(3,("Bug in client? Set O_WRONLY without O_CREAT\n"));
*/

  /*
   * Ensure we have a valid struct stat so we can search the
   * open fd table.
   */
  if(sbuf == 0) {
    if(dos_stat(fname, &statbuf) < 0) {
      if(errno != ENOENT) {
        DEBUG(3,("Error doing stat on file %s (%s)\n",
                 fname,strerror(errno)));

        check_for_pipe(fname);
        return;
      }
      sbuf = 0;
    } else {
      sbuf = &statbuf;
    }
  }

  /*
   * Check to see if we have this file already
   * open. If we do, just use the already open fd and increment the
   * reference count (fd_get_already_open increments the ref_count).
   */
  if((fd_ptr = fd_get_already_open(sbuf))!= 0) {
    /*
     * File was already open.
     */

    /* 
     * Check it wasn't open for exclusive use.
     */
    if((flags & O_CREAT) && (flags & O_EXCL)) {
      fd_ptr->ref_count--;
      errno = EEXIST;
      return;
    }

    /*
     * Ensure that the user attempting to open
     * this file has permissions to do so, if
     * the user who originally opened the file wasn't
     * the same as the current user.
     */

    if(!fd_is_in_uid_cache(fd_ptr, (uid_t)current_user.uid)) {
      if(!check_access_allowed_for_current_user( fname, accmode )) {
        /* Error - permission denied. */
        DEBUG(3,("Permission denied opening file %s (flags=%d, accmode = %d)\n",
              fname, flags, accmode));
        /* Ensure the ref_count is decremented. */
        fd_ptr->ref_count--;
        fd_remove_from_uid_cache(fd_ptr, (uid_t)current_user.uid);
        errno = EACCES;
        return;
      }
    }

    fd_add_to_uid_cache(fd_ptr, (uid_t)current_user.uid);

    /* 
     * If not opened O_RDWR try
     * and do that here - a chmod may have been done
     * between the last open and now. 
     */
    if(fd_ptr->real_open_flags != O_RDWR)
      fd_attempt_reopen(fname, mode, fd_ptr);

    /*
     * Ensure that if we wanted write access
     * it has been opened for write, and if we wanted read it
     * was open for read. 
     */
    if(((accmode == O_WRONLY) && (fd_ptr->real_open_flags == O_RDONLY)) ||
       ((accmode == O_RDONLY) && (fd_ptr->real_open_flags == O_WRONLY)) ||
       ((accmode == O_RDWR) && (fd_ptr->real_open_flags != O_RDWR))) {
      DEBUG(3,("Error opening (already open for flags=%d) file %s (%s) (flags=%d)\n",
               fd_ptr->real_open_flags, fname,strerror(EACCES),flags));
      check_for_pipe(fname);
      fd_remove_from_uid_cache(fd_ptr, (uid_t)current_user.uid);
      fd_ptr->ref_count--;
      return;
    }

  } else {
    int open_flags;
    /* We need to allocate a new file_fd_struct (this increments the
       ref_count). */
    if((fd_ptr = fd_get_new()) == 0)
      return;
    /*
     * Whatever the requested flags, attempt read/write access,
     * as we don't know what flags future file opens may require.
     * If this fails, try again with the required flags. 
     * Even if we open read/write when only read access was 
     * requested the setting of the can_write flag in
     * the file_struct will protect us from errant
     * write requests. We never need to worry about O_APPEND
     * as this is not set anywhere in Samba.
     */
    fd_ptr->real_open_flags = O_RDWR;
    /* Set the flags as needed without the read/write modes. */
    open_flags = flags & ~(O_RDWR|O_WRONLY|O_RDONLY);
    fd_ptr->fd = fd_attempt_open(fname, open_flags|O_RDWR, mode);
    /*
     * On some systems opening a file for R/W access on a read only
     * filesystems sets errno to EROFS.
     */
#ifdef EROFS
    if((fd_ptr->fd == -1) && ((errno == EACCES) || (errno == EROFS))) {
#else /* No EROFS */
    if((fd_ptr->fd == -1) && (errno == EACCES)) {
#endif /* EROFS */
      if(accmode != O_RDWR) {
        fd_ptr->fd = fd_attempt_open(fname, open_flags|accmode, mode);
        fd_ptr->real_open_flags = accmode;
      }
    }
  }

  if ((fd_ptr->fd >=0) && 
      conn->printer && lp_minprintspace(SNUM(conn))) {
    pstring dname;
    SMB_BIG_UINT dum1,dum2,dum3;
    char *p;
    pstrcpy(dname,fname);
    p = strrchr(dname,'/');
    if (p) *p = 0;
    if (sys_disk_free(dname,False,&dum1,&dum2,&dum3) < (SMB_BIG_UINT)lp_minprintspace(SNUM(conn))) {
      int err;
      if(fd_attempt_close(fd_ptr, &err) == 0)
        dos_unlink(fname);
      fsp->fd_ptr = 0;
      errno = ENOSPC;
      return;
    }
  }
    
  if (fd_ptr->fd < 0)
  {
    int err;
    DEBUG(3,("Error opening file %s (%s) (flags=%d)\n",
      fname,strerror(errno),flags));
    /* Ensure the ref_count is decremented. */
    fd_attempt_close(fd_ptr,&err);
    check_for_pipe(fname);
    return;
  }

  if (fd_ptr->fd >= 0)
  {
    if(sbuf == 0) {
      /* Do the fstat */
      if(sys_fstat(fd_ptr->fd, &statbuf) == -1) {
        int err;
        /* Error - backout !! */
        DEBUG(3,("Error doing fstat on fd %d, file %s (%s)\n",
                 fd_ptr->fd, fname,strerror(errno)));
        /* Ensure the ref_count is decremented. */
        fd_attempt_close(fd_ptr,&err);
        return;
      }
      sbuf = &statbuf;
    }

    /* Set the correct entries in fd_ptr. */
    fd_ptr->dev = sbuf->st_dev;
    fd_ptr->inode = sbuf->st_ino;

    fsp->fd_ptr = fd_ptr;
    conn->num_files_open++;
    fsp->mode = sbuf->st_mode;
    GetTimeOfDay(&fsp->open_time);
    fsp->vuid = current_user.vuid;
    fsp->size = 0;
    fsp->pos = -1;
    fsp->open = True;
    fsp->can_lock = True;
    fsp->can_read = ((flags & O_WRONLY)==0);
    fsp->can_write = ((flags & (O_WRONLY|O_RDWR))!=0);
    fsp->share_mode = 0;
    fsp->print_file = conn->printer;
    fsp->modified = False;
    fsp->oplock_type = NO_OPLOCK;
    fsp->sent_oplock_break = NO_BREAK_SENT;
    fsp->is_directory = False;
    fsp->stat_open = False;
    fsp->directory_delete_on_close = False;
    fsp->conn = conn;
    /*
     * Note that the file name here is the *untranslated* name
     * ie. it is still in the DOS codepage sent from the client.
     * All use of this filename will pass though the sys_xxxx
     * functions which will do the dos_to_unix translation before
     * mapping into a UNIX filename. JRA.
     */
    string_set(&fsp->fsp_name,fname);
    fsp->wbmpx_ptr = NULL;      
    fsp->wcp = NULL; /* Write cache pointer. */

    /*
     * If the printer is marked as postscript output a leading
     * file identifier to ensure the file is treated as a raw
     * postscript file.
     * This has a similar effect as CtrlD=0 in WIN.INI file.
     * tim@fsg.com 09/06/94
     */
    if (fsp->print_file && lp_postscript(SNUM(conn)) && fsp->can_write) {
	    DEBUG(3,("Writing postscript line\n"));
	    write_file(fsp,"%!\n",-1,3);
    }
      
    DEBUG(2,("%s opened file %s read=%s write=%s (numopen=%d)\n",
	     *sesssetup_user ? sesssetup_user : conn->user,fsp->fsp_name,
	     BOOLSTR(fsp->can_read), BOOLSTR(fsp->can_write),
	     conn->num_files_open));

  }
}

/****************************************************************************
  C. Hoch 11/22/95
  Helper for open_file_shared. 
  Truncate a file after checking locking; close file if locked.
  **************************************************************************/

static void truncate_unless_locked(files_struct *fsp, connection_struct *conn, int token, 
				   BOOL *share_locked)
{
	if (fsp->can_write){
		SMB_OFF_T mask2 = ((SMB_OFF_T)0x3) << (SMB_OFF_T_BITS-4);
		SMB_OFF_T mask = (mask2<<2);
		
		if (is_locked(fsp,conn,~mask,0,F_WRLCK)){
			/* If share modes are in force for this connection we
			   have the share entry locked. Unlock it before closing. */
			if (*share_locked && lp_share_modes(SNUM(conn)))
				unlock_share_entry( conn, fsp->fd_ptr->dev, 
						    fsp->fd_ptr->inode, token);
			close_on_error(fsp,conn);   
			/* Share mode no longer locked. */
			*share_locked = False;
			errno = EACCES;
			unix_ERR_class = ERRDOS;
		  unix_ERR_code = ERRlock;
		} else {
			sys_ftruncate(fsp->fd_ptr->fd,0); 
		}
	}
}


/*******************************************************************
return True if the filename is one of the special executable types
********************************************************************/
static BOOL is_executable(char *fname)
{
	if ((fname = strrchr(fname,'.'))) {
		if (strequal(fname,".com") ||
		    strequal(fname,".dll") ||
		    strequal(fname,".exe") ||
		    strequal(fname,".sym")) {
			return True;
		}
	}
	return False;
}

enum {AFAIL,AREAD,AWRITE,AALL};

/*******************************************************************
reproduce the share mode access table
this is horrendoously complex, and really can't be justified on any
rational grounds except that this is _exactly_ what NT does. See
the DENY1 and DENY2 tests in smbtorture for a comprehensive set of
test routines.
********************************************************************/
static int access_table(int new_deny,int old_deny,int old_mode,
			BOOL same_pid, BOOL isexe)
{
	  if (new_deny == DENY_ALL || old_deny == DENY_ALL) return(AFAIL);

	  if (same_pid) {
		  if (isexe && old_mode == DOS_OPEN_RDONLY && 
		      old_deny == DENY_DOS && new_deny == DENY_READ) {
			  return AFAIL;
		  }
		  if (!isexe && old_mode == DOS_OPEN_RDONLY && 
		      old_deny == DENY_DOS && new_deny == DENY_DOS) {
			  return AREAD;
		  }
		  if (new_deny == DENY_FCB && old_deny == DENY_DOS) {
			  if (isexe) return AFAIL;
			  if (old_mode == DOS_OPEN_RDONLY) return AFAIL;
			  return AALL;
		  }
		  if (old_mode == DOS_OPEN_RDONLY && old_deny == DENY_DOS) {
			  if (new_deny == DENY_FCB || new_deny == DENY_READ) {
				  if (isexe) return AREAD;
				  return AFAIL;
			  }
		  }
		  if (old_deny == DENY_FCB) {
			  if (new_deny == DENY_DOS || new_deny == DENY_FCB) return AALL;
			  return AFAIL;
		  }
	  }

	  if (old_deny == DENY_DOS || new_deny == DENY_DOS || 
	      old_deny == DENY_FCB || new_deny == DENY_FCB) {
		  if (isexe) {
			  if (old_deny == DENY_FCB || new_deny == DENY_FCB) {
				  return AFAIL;
			  }
			  if (old_deny == DENY_DOS) {
				  if (new_deny == DENY_READ && 
				      (old_mode == DOS_OPEN_RDONLY || 
				       old_mode == DOS_OPEN_RDWR)) {
					  return AFAIL;
				  }
				  if (new_deny == DENY_WRITE && 
				      (old_mode == DOS_OPEN_WRONLY || 
				       old_mode == DOS_OPEN_RDWR)) {
					  return AFAIL;
				  }
				  return AALL;
			  }
			  if (old_deny == DENY_NONE) return AALL;
			  if (old_deny == DENY_READ) return AWRITE;
			  if (old_deny == DENY_WRITE) return AREAD;
		  }
		  /* it isn't a exe, dll, sym or com file */
		  if (old_deny == new_deny && same_pid)
			  return(AALL);    

		  if (old_deny == DENY_READ || new_deny == DENY_READ) return AFAIL;
		  if (old_mode == DOS_OPEN_RDONLY) return(AREAD);
		  
		  return(AFAIL);
	  }
	  
	  switch (new_deny) 
		  {
		  case DENY_WRITE:
			  if (old_deny==DENY_WRITE && old_mode==DOS_OPEN_RDONLY) return(AREAD);
			  if (old_deny==DENY_READ && old_mode==DOS_OPEN_RDONLY) return(AWRITE);
			  if (old_deny==DENY_NONE && old_mode==DOS_OPEN_RDONLY) return(AALL);
			  return(AFAIL);
		  case DENY_READ:
			  if (old_deny==DENY_WRITE && old_mode==DOS_OPEN_WRONLY) return(AREAD);
			  if (old_deny==DENY_READ && old_mode==DOS_OPEN_WRONLY) return(AWRITE);
			  if (old_deny==DENY_NONE && old_mode==DOS_OPEN_WRONLY) return(AALL);
			  return(AFAIL);
		  case DENY_NONE:
			  if (old_deny==DENY_WRITE) return(AREAD);
			  if (old_deny==DENY_READ) return(AWRITE);
			  if (old_deny==DENY_NONE) return(AALL);
			  return(AFAIL);      
		  }
	  return(AFAIL);      
}


/****************************************************************************
check if we can open a file with a share mode
****************************************************************************/

static int check_share_mode( share_mode_entry *share, int deny_mode, 
			     char *fname,
			     BOOL fcbopen, int *flags)
{
  int old_open_mode = GET_OPEN_MODE(share->share_mode);
  int old_deny_mode = GET_DENY_MODE(share->share_mode);

  /*
   * Don't allow any open once the delete on close flag has been
   * set.
   */

  if(GET_DELETE_ON_CLOSE_FLAG(share->share_mode))
  {
    DEBUG(5,("check_share_mode: Failing open on file %s as delete on close flag is set.\n",
          fname ));
    unix_ERR_class = ERRDOS;
    unix_ERR_code = ERRnoaccess;
    return False;
  }

  {
    int access_allowed = access_table(deny_mode,old_deny_mode,old_open_mode,
				      (share->pid == getpid()),is_executable(fname));

    if ((access_allowed == AFAIL) ||
        (!fcbopen && (access_allowed == AREAD && *flags == O_RDWR)) ||
        (access_allowed == AREAD && *flags != O_RDONLY) ||
        (access_allowed == AWRITE && *flags != O_WRONLY))
    {
      DEBUG(2,("Share violation on file (%d,%d,%d,%d,%s,fcbopen = %d, flags = %d) = %d\n",
                deny_mode,old_deny_mode,old_open_mode,
                (int)share->pid,fname, fcbopen, *flags, access_allowed));

      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadshare;

      return False;
    }

    if (access_allowed == AREAD)
      *flags = O_RDONLY;

    if (access_allowed == AWRITE)
      *flags = O_WRONLY;

  }

  return True;
}

/****************************************************************************
open a file with a share mode
****************************************************************************/

void open_file_shared(files_struct *fsp,connection_struct *conn,char *fname,int share_mode,int ofun,
		      mode_t mode,int oplock_request, int *Access,int *action)
{
  int flags=0;
  int flags2=0;
  int deny_mode = GET_DENY_MODE(share_mode);
  BOOL allow_share_delete = GET_ALLOW_SHARE_DELETE(share_mode);
  SMB_STRUCT_STAT sbuf;
  BOOL file_existed = dos_file_exist(fname,&sbuf);
  BOOL share_locked = False;
  BOOL fcbopen = False;
  int token = 0;
  SMB_DEV_T dev = 0;
  SMB_INO_T inode = 0;
  int num_share_modes = 0;
  int oplock_contention_count = 0;
  BOOL all_current_opens_are_level_II = False;
  fsp->open = False;
  fsp->fd_ptr = 0;

  DEBUG(10,("open_file_shared: fname = %s, share_mode = %x, ofun = %x, mode = %o, oplock request = %d\n",
        fname, share_mode, ofun, (int)mode,  oplock_request ));

  if (!check_name(fname,conn)) {
      return;
  }

  /* ignore any oplock requests if oplocks are disabled */
  if (!lp_oplocks(SNUM(conn)) || global_client_failed_oplock_break) {
	  oplock_request = 0;
  }

  /* this is for OS/2 EAs - try and say we don't support them */
  if (strstr(fname,".+,;=[].")) 
  {
    unix_ERR_class = ERRDOS;
    /* OS/2 Workplace shell fix may be main code stream in a later release. */ 
#if 1 /* OS2_WPS_FIX - Recent versions of OS/2 need this. */
    unix_ERR_code = ERRcannotopen;
#else /* OS2_WPS_FIX */
    unix_ERR_code = ERROR_EAS_NOT_SUPPORTED;
#endif /* OS2_WPS_FIX */

    DEBUG(5,("open_file_shared: OS/2 EA's are not supported.\n"));
    return;
  }

  if ((GET_FILE_OPEN_DISPOSITION(ofun) == FILE_EXISTS_FAIL) && file_existed)  
  {
    DEBUG(5,("open_file_shared: create new requested for file %s and file already exists.\n",
          fname ));
    errno = EEXIST;
    return;
  }
      
  if (GET_FILE_CREATE_DISPOSITION(ofun) == FILE_CREATE_IF_NOT_EXIST)
    flags2 |= O_CREAT;

  if (GET_FILE_OPEN_DISPOSITION(ofun) == FILE_EXISTS_TRUNCATE)
    flags2 |= O_TRUNC;

  if (GET_FILE_OPEN_DISPOSITION(ofun) == FILE_EXISTS_FAIL)
    flags2 |= O_EXCL;

  /* note that we ignore the append flag as 
     append does not mean the same thing under dos and unix */

  switch (GET_OPEN_MODE(share_mode))
  {
    case DOS_OPEN_WRONLY: 
      flags = O_WRONLY; 
      break;
    case DOS_OPEN_FCB: 
      fcbopen = True;
      flags = O_RDWR; 
      break;
    case DOS_OPEN_RDWR: 
      flags = O_RDWR; 
      break;
    default:
      flags = O_RDONLY;
      break;
  }

#if defined(O_SYNC)
  if (GET_FILE_SYNC_OPENMODE(share_mode)) {
	  flags2 |= O_SYNC;
  }
#endif /* O_SYNC */
  
  if (flags != O_RDONLY && file_existed && 
      (!CAN_WRITE(conn) || IS_DOS_READONLY(dos_mode(conn,fname,&sbuf)))) 
  {
    if (!fcbopen) 
    {
      DEBUG(5,("open_file_shared: read/write access requested for file %s on read only %s\n",
            fname, !CAN_WRITE(conn) ? "share" : "file" ));
      errno = EACCES;
      return;
    }
    flags = O_RDONLY;
  }

  if (deny_mode > DENY_NONE && deny_mode!=DENY_FCB) 
  {
    DEBUG(2,("Invalid deny mode %d on file %s\n",deny_mode,fname));
    errno = EINVAL;
    return;
  }

  if (lp_share_modes(SNUM(conn))) 
  {
    int i;
    share_mode_entry *old_shares = 0;

    if (file_existed)
    {
      dev = sbuf.st_dev;
      inode = sbuf.st_ino;
      lock_share_entry(conn, dev, inode, &token);
      share_locked = True;
      num_share_modes = get_share_modes(conn, token, dev, inode, &old_shares);
    }

    /*
     * Check if the share modes will give us access.
     */

    if(share_locked && (num_share_modes != 0))
    {
      BOOL broke_oplock;

      do
      {

        broke_oplock = False;
        all_current_opens_are_level_II = True;

        for(i = 0; i < num_share_modes; i++)
        {
          share_mode_entry *share_entry = &old_shares[i];

          /* 
           * By observation of NetBench, oplocks are broken *before* share
           * modes are checked. This allows a file to be closed by the client
           * if the share mode would deny access and the client has an oplock. 
           * Check if someone has an oplock on this file. If so we must break 
           * it before continuing. 
           */
          if((oplock_request && EXCLUSIVE_OPLOCK_TYPE(share_entry->op_type)) ||
             (!oplock_request && (share_entry->op_type != NO_OPLOCK)))
          {

            DEBUG(5,("open_file_shared: breaking oplock (%x) on file %s, \
dev = %x, inode = %.0f\n", share_entry->op_type, fname, (unsigned int)dev, (double)inode));

            /* Oplock break.... */
            unlock_share_entry(conn, dev, inode, token);
            if(request_oplock_break(share_entry, dev, inode) == False)
            {
              free((char *)old_shares);

              DEBUG(0,("open_file_shared: FAILED when breaking oplock (%x) on file %s, \
dev = %x, inode = %.0f\n", old_shares[i].op_type, fname, (unsigned int)dev, (double)inode));

              errno = EACCES;
              unix_ERR_class = ERRDOS;
              unix_ERR_code = ERRbadshare;
              return;
            }
            lock_share_entry(conn, dev, inode, &token);
            broke_oplock = True;
            all_current_opens_are_level_II = False;
            break;
          } else if (!LEVEL_II_OPLOCK_TYPE(share_entry->op_type)) {
            all_current_opens_are_level_II = False;
          }

          /* someone else has a share lock on it, check to see 
             if we can too */
          if(check_share_mode(share_entry, deny_mode, fname, fcbopen, &flags) == False)
          {
            free((char *)old_shares);
            unlock_share_entry(conn, dev, inode, token);
            errno = EACCES;
            return;
          }

        } /* end for */

        if(broke_oplock)
        {
          free((char *)old_shares);
          num_share_modes = get_share_modes(conn, token, dev, inode, &old_shares);
          oplock_contention_count++;
        }
      } while(broke_oplock);
    }

    if(old_shares != 0)
      free((char *)old_shares);
  }

  /*
   * Refuse to grant an oplock in case the contention limit is
   * reached when going through the lock list multiple times.
   */

  if(oplock_contention_count >= lp_oplock_contention_limit(SNUM(conn)))
  {
    oplock_request = 0;
    DEBUG(4,("open_file_shared: oplock contention = %d. Not granting oplock.\n",
          oplock_contention_count ));
  }

  DEBUG(4,("calling open_file with flags=0x%X flags2=0x%X mode=0%o\n",
	   flags,flags2,(int)mode));

  open_file(fsp,conn,fname,flags|(flags2&~(O_TRUNC)),mode,file_existed ? &sbuf : 0);
  if (!fsp->open && flags==O_RDWR && errno!=ENOENT && fcbopen) 
  {
    flags = O_RDONLY;
    open_file(fsp,conn,fname,flags,mode,file_existed ? &sbuf : 0 );
  }

  if (fsp->open) 
  {
    int open_mode=0;

    if((share_locked == False) && lp_share_modes(SNUM(conn)))
    {
      /* We created the file - thus we must now lock the share entry before creating it. */
      dev = fsp->fd_ptr->dev;
      inode = fsp->fd_ptr->inode;
      lock_share_entry(conn, dev, inode, &token);
      share_locked = True;
    }

    switch (flags) 
    {
      case O_RDONLY:
        open_mode = DOS_OPEN_RDONLY;
        break;
      case O_RDWR:
        open_mode = DOS_OPEN_RDWR;
        break;
      case O_WRONLY:
        open_mode = DOS_OPEN_WRONLY;
        break;
    }

    fsp->share_mode = SET_DENY_MODE(deny_mode) | 
                      SET_OPEN_MODE(open_mode) | 
                      SET_ALLOW_SHARE_DELETE(allow_share_delete);

    if (Access)
      (*Access) = open_mode;

    if (action) 
    {
      if (file_existed && !(flags2 & O_TRUNC)) *action = FILE_WAS_OPENED;
      if (!file_existed) *action = FILE_WAS_CREATED;
      if (file_existed && (flags2 & O_TRUNC)) *action = FILE_WAS_OVERWRITTEN;
    }
    /* We must create the share mode entry before truncate as
       truncate can fail due to locking and have to close the
       file (which expects the share_mode_entry to be there).
     */
    if (lp_share_modes(SNUM(conn)))
    {
      uint16 port = 0;

      /* 
       * Setup the oplock info in both the shared memory and
       * file structs.
       */

      if(oplock_request && (num_share_modes == 0) && 
	      !IS_VETO_OPLOCK_PATH(conn,fname) && set_file_oplock(fsp, oplock_request) ) {
        port = global_oplock_port;
      } else if (oplock_request && all_current_opens_are_level_II) {
        port = global_oplock_port;
        oplock_request = LEVEL_II_OPLOCK;
        set_file_oplock(fsp, oplock_request);
      } else {
        port = 0;
        oplock_request = 0;
      }

      set_share_mode(token, fsp, port, oplock_request);
    }

    if ((flags2&O_TRUNC) && file_existed)
      truncate_unless_locked(fsp,conn,token,&share_locked);
  }

  if (share_locked && lp_share_modes(SNUM(conn)))
    unlock_share_entry( conn, dev, inode, token);
}

/****************************************************************************
 Open a file for permissions read only. Return a pseudo file entry
 with the 'stat_open' flag set and a fd_ptr of NULL.
****************************************************************************/

int open_file_stat(files_struct *fsp,connection_struct *conn,
		   char *fname, int smb_ofun, SMB_STRUCT_STAT *pst, int *action)
{
	extern struct current_user current_user;

	if(dos_stat(fname, pst) < 0) {
		DEBUG(0,("open_file_stat: unable to stat name = %s. Error was %s\n",
			 fname, strerror(errno) ));
		return -1;
	}

	if(S_ISDIR(pst->st_mode)) {
		DEBUG(0,("open_file_stat: %s is a directory !\n", fname ));
		return -1;
	}

	*action = FILE_WAS_OPENED;
	
	DEBUG(5,("open_file_stat: opening file %s as a stat entry\n", fname));

	/*
	 * Setup the files_struct for it.
	 */
	
	fsp->fd_ptr = NULL;
	conn->num_files_open++;
	fsp->mode = 0;
	GetTimeOfDay(&fsp->open_time);
	fsp->vuid = current_user.vuid;
	fsp->size = 0;
	fsp->pos = -1;
	fsp->open = True;
	fsp->can_lock = False;
	fsp->can_read = False;
	fsp->can_write = False;
	fsp->share_mode = 0;
	fsp->print_file = False;
	fsp->modified = False;
	fsp->oplock_type = NO_OPLOCK;
	fsp->sent_oplock_break = NO_BREAK_SENT;
	fsp->is_directory = False;
	fsp->stat_open = True;
	fsp->directory_delete_on_close = False;
	fsp->conn = conn;
	/*
	 * Note that the file name here is the *untranslated* name
	 * ie. it is still in the DOS codepage sent from the client.
	 * All use of this filename will pass though the sys_xxxx
	 * functions which will do the dos_to_unix translation before
	 * mapping into a UNIX filename. JRA.
	 */
	string_set(&fsp->fsp_name,fname);
	fsp->wbmpx_ptr = NULL;
    fsp->wcp = NULL; /* Write cache pointer. */

	return 0;
}

/****************************************************************************
 Open a directory from an NT SMB call.
****************************************************************************/

int open_directory(files_struct *fsp,connection_struct *conn,
		   char *fname, int smb_ofun, mode_t unixmode, int *action)
{
	extern struct current_user current_user;
	SMB_STRUCT_STAT st;
	BOOL got_stat = False;

	if(dos_stat(fname, &st) == 0) {
		got_stat = True;
	}

	if (got_stat && (GET_FILE_OPEN_DISPOSITION(smb_ofun) == FILE_EXISTS_FAIL)) {
		errno = EEXIST; /* Setup so correct error is returned to client. */
		return -1;
	}

	if (GET_FILE_CREATE_DISPOSITION(smb_ofun) == FILE_CREATE_IF_NOT_EXIST) {

		if (got_stat) {

			if(!S_ISDIR(st.st_mode)) {
				DEBUG(0,("open_directory: %s is not a directory !\n", fname ));
				errno = EACCES;
				return -1;
			}
			*action = FILE_WAS_OPENED;

		} else {

			/*
			 * Try and create the directory.
			 */

			if(!CAN_WRITE(conn)) {
				DEBUG(2,("open_directory: failing create on read-only share\n"));
				errno = EACCES;
				return -1;
			}

			if(dos_mkdir(fname, unix_mode(conn,aDIR, fname)) < 0) {
				DEBUG(0,("open_directory: unable to create %s. Error was %s\n",
					 fname, strerror(errno) ));
				return -1;
			}
			*action = FILE_WAS_CREATED;

		}
	} else {

		/*
		 * Don't create - just check that it *was* a directory.
		 */

		if(!got_stat) {
			DEBUG(0,("open_directory: unable to stat name = %s. Error was %s\n",
				 fname, strerror(errno) ));
			return -1;
		}

		if(!S_ISDIR(st.st_mode)) {
			DEBUG(0,("open_directory: %s is not a directory !\n", fname ));
			return -1;
		}

		*action = FILE_WAS_OPENED;
	}
	
	DEBUG(5,("open_directory: opening directory %s\n",
		 fname));

	/*
	 * Setup the files_struct for it.
	 */
	
	fsp->fd_ptr = NULL;
	conn->num_files_open++;
	fsp->mode = 0;
	GetTimeOfDay(&fsp->open_time);
	fsp->vuid = current_user.vuid;
	fsp->size = 0;
	fsp->pos = -1;
	fsp->open = True;
	fsp->can_lock = True;
	fsp->can_read = False;
	fsp->can_write = False;
	fsp->share_mode = 0;
	fsp->print_file = False;
	fsp->modified = False;
	fsp->oplock_type = NO_OPLOCK;
	fsp->sent_oplock_break = NO_BREAK_SENT;
	fsp->is_directory = True;
	fsp->directory_delete_on_close = False;
	fsp->conn = conn;
	/*
	 * Note that the file name here is the *untranslated* name
	 * ie. it is still in the DOS codepage sent from the client.
	 * All use of this filename will pass though the sys_xxxx
	 * functions which will do the dos_to_unix translation before
	 * mapping into a UNIX filename. JRA.
	 */
	string_set(&fsp->fsp_name,fname);
	fsp->wbmpx_ptr = NULL;

	return 0;
}

/*******************************************************************
 Check if the share mode on a file allows it to be deleted or unlinked.
 Return True if sharing doesn't prevent the operation.
********************************************************************/

BOOL check_file_sharing(connection_struct *conn,char *fname, BOOL rename_op)
{
  int i;
  int ret = False;
  share_mode_entry *old_shares = 0;
  int num_share_modes;
  SMB_STRUCT_STAT sbuf;
  int token;
  pid_t pid = getpid();
  SMB_DEV_T dev;
  SMB_INO_T inode;

  if(!lp_share_modes(SNUM(conn)))
    return True;

  if (dos_stat(fname,&sbuf) == -1) return(True);

  dev = sbuf.st_dev;
  inode = sbuf.st_ino;

  lock_share_entry(conn, dev, inode, &token);
  num_share_modes = get_share_modes(conn, token, dev, inode, &old_shares);

  /*
   * Check if the share modes will give us access.
   */

  if(num_share_modes != 0)
  {
    BOOL broke_oplock;

    do
    {

      broke_oplock = False;
      for(i = 0; i < num_share_modes; i++)
      {
        share_mode_entry *share_entry = &old_shares[i];

        /* 
         * Break oplocks before checking share modes. See comment in
         * open_file_shared for details. 
         * Check if someone has an oplock on this file. If so we must 
         * break it before continuing. 
         */
        if(BATCH_OPLOCK_TYPE(share_entry->op_type))
        {

#if 0

/* JRA. Try removing this code to see if the new oplock changes
   fix the problem. I'm dubious, but Andrew is recommending we
   try this....
*/

          /*
           * It appears that the NT redirector may have a bug, in that
           * it tries to do an SMBmv on a file that it has open with a
           * batch oplock, and then fails to respond to the oplock break
           * request. This only seems to occur when the client is doing an
           * SMBmv to the smbd it is using - thus we try and detect this
           * condition by checking if the file being moved is open and oplocked by
           * this smbd process, and then not sending the oplock break in this
           * special case. If the file was open with a deny mode that 
           * prevents the move the SMBmv will fail anyway with a share
           * violation error. JRA.
           */
          if(rename_op && (share_entry->pid == pid))
          {

            DEBUG(0,("check_file_sharing: NT redirector workaround - rename attempted on \
batch oplocked file %s, dev = %x, inode = %.0f\n", fname, (unsigned int)dev, (double)inode));

            /* 
             * This next line is a test that allows the deny-mode
             * processing to be skipped. This seems to be needed as
             * NT insists on the rename succeeding (in Office 9x no less !).
             * This should be removed as soon as (a) MS fix the redirector
             * bug or (b) NT SMB support in Samba makes NT not issue the
             * call (as is my fervent hope). JRA.
             */ 
            continue;
          }
          else
#endif /* 0 */
          {

            DEBUG(5,("check_file_sharing: breaking oplock (%x) on file %s, \
dev = %x, inode = %.0f\n", share_entry->op_type, fname, (unsigned int)dev, (double)inode));

            /* Oplock break.... */
            unlock_share_entry(conn, dev, inode, token);
            if(request_oplock_break(share_entry, dev, inode) == False)
            {
              free((char *)old_shares);

              DEBUG(0,("check_file_sharing: FAILED when breaking oplock (%x) on file %s, \
dev = %x, inode = %.0f\n", old_shares[i].op_type, fname, (unsigned int)dev, (double)inode));

              return False;
            }
            lock_share_entry(conn, dev, inode, &token);
            broke_oplock = True;
            break;
          }
        }

        /* 
         * If this is a delete request and ALLOW_SHARE_DELETE is set then allow 
         * this to proceed. This takes precedence over share modes.
         */

        if(!rename_op && GET_ALLOW_SHARE_DELETE(share_entry->share_mode))
          continue;

        /* 
         * Someone else has a share lock on it, check to see 
         * if we can too.
         */

        if ((GET_DENY_MODE(share_entry->share_mode) != DENY_DOS) || (share_entry->pid != pid))
          goto free_and_exit;

      } /* end for */

      if(broke_oplock)
      {
        free((char *)old_shares);
        num_share_modes = get_share_modes(conn, token, dev, inode, &old_shares);
      }
    } while(broke_oplock);
  }

  /* XXXX exactly what share mode combinations should be allowed for
     deleting/renaming? */
  /* 
   * If we got here then either there were no share modes or
   * all share modes were DENY_DOS and the pid == getpid() or
   * delete access was requested and all share modes had the
   * ALLOW_SHARE_DELETE bit set (takes precedence over other
   * share modes).
   */

  ret = True;

free_and_exit:

  unlock_share_entry(conn, dev, inode, token);
  if(old_shares != NULL)
    free((char *)old_shares);
  return(ret);
}

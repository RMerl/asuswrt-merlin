/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   slow (lockfile) locking implementation
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

   Revision History:

   12 aug 96: Erik.Devriendt@te6.siemens.be
   added support for shared memory implementation of share mode locking

   May 1997. Jeremy Allison (jallison@whistle.com). Modified share mode
   locking to deal with multiple share modes per open file.

   September 1997. Jeremy Allison (jallison@whistle.com). Added oplock
   support.

   October 1997 - split into separate file (tridge)
*/

#include "includes.h"

#ifndef FAST_SHARE_MODES

extern int DEBUGLEVEL;

/* 
 * Locking file header lengths & offsets. 
 */
#define SMF_VERSION_OFFSET 0
#define SMF_NUM_ENTRIES_OFFSET 4
#define SMF_FILENAME_LEN_OFFSET 8
#define SMF_HEADER_LENGTH 10

#define SMF_ENTRY_LENGTH 20

/*
 * Share mode record offsets.
 */

#define SME_SEC_OFFSET 0
#define SME_USEC_OFFSET 4
#define SME_SHAREMODE_OFFSET 8
#define SME_PID_OFFSET 12
#define SME_PORT_OFFSET 16
#define SME_OPLOCK_TYPE_OFFSET 18

/* we need world read for smbstatus to function correctly */
#ifdef SECURE_SHARE_MODES
#define SHARE_FILE_MODE 0600
#else
#define SHARE_FILE_MODE 0644
#endif

static int read_only;

/*******************************************************************
  deinitialize share_mode management 
  ******************************************************************/
static BOOL slow_stop_share_mode_mgmt(void)
{
   return True;
}


/*******************************************************************
  name a share file
  ******************************************************************/
static BOOL share_name(connection_struct *conn, 
		       SMB_DEV_T dev, SMB_INO_T inode, char *name)
{
	int len;
	pstrcpy(name,lp_lockdir());
	trim_string(name,"","/");
	if (!*name) return(False);
	len = strlen(name);
	name += len;
	
#ifdef LARGE_SMB_INO_T
	slprintf(name, sizeof(pstring) - len - 1, "/share.%u.%.0f",(unsigned int)dev,(double)inode);
#else /* LARGE_SMB_INO_T */
	slprintf(name, sizeof(pstring) - len - 1, "/share.%u.%lu",(unsigned int)dev,(unsigned long)inode);
#endif /* LARGE_SMB_INO_T */
	return(True);
}

/*******************************************************************
Force a share file to be deleted.
********************************************************************/
static int delete_share_file(connection_struct *conn, char *fname )
{
  if (read_only) return -1;

  /* the share file could be owned by anyone, so do this as root */
  become_root(False);

  if(unlink(fname) != 0)
  {
    DEBUG(0,("delete_share_file: Can't delete share file %s (%s)\n",
            fname, strerror(errno)));
  } 
  else 
  {
    DEBUG(5,("delete_share_file: Deleted share file %s\n", fname));
  }

  /* return to our previous privilege level */
  unbecome_root(False);

  return 0;
}

/*******************************************************************
  lock a share mode file.
  ******************************************************************/
static BOOL slow_lock_share_entry(connection_struct *conn,
				  SMB_DEV_T dev, SMB_INO_T inode, int *ptok)
{
  pstring fname;
  int fd;
  int ret = True;

  *ptok = (int)-1;

  if(!share_name(conn, dev, inode, fname))
    return False;

  if (read_only) return True;

  /* we need to do this as root */
  become_root(False);

  {
    BOOL gotlock = False;
    /*
     * There was a race condition in the original slow share mode code.
     * A smbd could open a share mode file, and before getting
     * the lock, another smbd could delete the last entry for
     * the share mode file and delete the file entry from the
     * directory. Thus this smbd would be left with a locked
     * share mode fd attached to a file that no longer had a
     * directory entry. Thus another smbd would think that
     * there were no outstanding opens on the file. To fix
     * this we now check we can do a stat() call on the filename
     * before allowing the lock to proceed, and back out completely
     * and try the open again if we cannot.
     * Jeremy Allison (jallison@whistle.com).
     */

    do
    {
      SMB_STRUCT_STAT dummy_stat;

      fd = sys_open(fname,read_only?O_RDONLY:(O_RDWR|O_CREAT), SHARE_FILE_MODE);

      if(fd < 0)
      {
        DEBUG(0,("ERROR lock_share_entry: failed to open share file %s. Error was %s\n",
                  fname, strerror(errno)));
        ret = False;
        break;
      }

       /* At this point we have an open fd to the share mode file. 
         Lock the first byte exclusively to signify a lock. */
      if(fcntl_lock(fd, SMB_F_SETLKW, 0, 1, F_WRLCK) == False)
      {
        DEBUG(0,("ERROR lock_share_entry: fcntl_lock on file %s failed with %s\n",
                  fname, strerror(errno)));   
        close(fd);
        ret = False;
        break;
      }

      /* 
       * If we cannot stat the filename, the file was deleted between
       * the open and the lock call. Back out and try again.
       */

      if(sys_stat(fname, &dummy_stat)!=0)
      {
        DEBUG(2,("lock_share_entry: Re-issuing open on %s to fix race. Error was %s\n",
                fname, strerror(errno)));
        close(fd);
      }
      else
        gotlock = True;
    } while(!gotlock);

    /*
     * We have to come here if any of the above calls fail
     * as we don't want to return and leave ourselves running
     * as root !
     */
  }

  *ptok = (int)fd;

  /* return to our previous privilege level */
  unbecome_root(False);

  return ret;
}

/*******************************************************************
  unlock a share mode file.
  ******************************************************************/
static BOOL slow_unlock_share_entry(connection_struct *conn, 
				    SMB_DEV_T dev, SMB_INO_T inode, int token)
{
  int fd = token;
  int ret = True;
  SMB_STRUCT_STAT sb;
  pstring fname;

  if (read_only) return True;

  /* Fix for zero length share files from
     Gerald Werner <wernerg@mfldclin.edu> */
    
  share_name(conn, dev, inode, fname);

  /* get the share mode file size */
  if(sys_fstat((int)token, &sb) != 0)
  {
    DEBUG(0,("ERROR: unlock_share_entry: Failed to do stat on share file %s (%s)\n",
              fname, strerror(errno)));
    sb.st_size = 1;
    ret = False;
  }

  /* If the file was zero length, we must delete before
     doing the unlock to avoid a race condition (see
     the code in lock_share_mode_entry for details.
   */

  /* remove the share file if zero length */    
  if(sb.st_size == 0)  
    delete_share_file(conn, fname);

  /* token is the fd of the open share mode file. */
  /* Unlock the first byte. */
  if(fcntl_lock(fd, SMB_F_SETLKW, 0, 1, F_UNLCK) == False)
   { 
      DEBUG(0,("ERROR unlock_share_entry: fcntl_lock failed with %s\n",
                      strerror(errno)));   
      ret = False;
   }
 
  close(fd);
  return ret;
}

/*******************************************************************
Read a share file into a buffer.
********************************************************************/
static int read_share_file(connection_struct *conn, int fd, char *fname, char **out, BOOL *p_new_file)
{
  SMB_STRUCT_STAT sb;
  char *buf;
  SMB_OFF_T size;

  *out = 0;
  *p_new_file = False;

  if(sys_fstat(fd, &sb) != 0)
  {
    DEBUG(0,("ERROR: read_share_file: Failed to do stat on share file %s (%s)\n",
                  fname, strerror(errno)));
    return -1;
  }

  if(sb.st_size == 0)
  {
     *p_new_file = True;
     return 0;
  }

  /* Allocate space for the file */
  if((buf = (char *)malloc((size_t)sb.st_size)) == NULL)
  {
    DEBUG(0,("read_share_file: malloc for file size %d fail !\n", 
	     (int)sb.st_size));
    return -1;
  }
  
  if(sys_lseek(fd, (SMB_OFF_T)0, SEEK_SET) != 0)
  {
    DEBUG(0,("ERROR: read_share_file: Failed to reset position to 0 \
for share file %s (%s)\n", fname, strerror(errno)));
    if(buf)
      free(buf);
    return -1;
  }
  
  if (read(fd,buf,(size_t)sb.st_size) != (size_t)sb.st_size)
  {
    DEBUG(0,("ERROR: read_share_file: Failed to read share file %s (%s)\n",
               fname, strerror(errno)));
    if(buf)
      free(buf);
    return -1;
  }
  
  if (IVAL(buf,SMF_VERSION_OFFSET) != LOCKING_VERSION) {
    DEBUG(0,("ERROR: read_share_file: share file %s has incorrect \
locking version (was %d, should be %d).\n",fname, 
                    IVAL(buf,SMF_VERSION_OFFSET), LOCKING_VERSION));
   if(buf)
      free(buf);
    delete_share_file(conn, fname);
    return -1;
  }

  /* Sanity check for file contents */
  size = sb.st_size;
  size -= SMF_HEADER_LENGTH; /* Remove the header */

  /* Remove the filename component. */
  size -= SVAL(buf, SMF_FILENAME_LEN_OFFSET);

  /* The remaining size must be a multiple of SMF_ENTRY_LENGTH - error if not. */
  if((size % SMF_ENTRY_LENGTH) != 0)
  {
    DEBUG(0,("ERROR: read_share_file: share file %s is an incorrect length - \
deleting it.\n", fname));
    if(buf)
      free(buf);
    delete_share_file(conn, fname);
    return -1;
  }

  *out = buf;
  return 0;
}

/*******************************************************************
get all share mode entries in a share file for a dev/inode pair.
********************************************************************/
static int slow_get_share_modes(connection_struct *conn, int token, 
                SMB_DEV_T dev, SMB_INO_T inode, 
				share_mode_entry **old_shares)
{
  int fd = token;
  pstring fname;
  int i;
  int num_entries;
  int num_entries_copied;
  int newsize;
  share_mode_entry *share_array;
  char *buf = 0;
  char *base = 0;
  BOOL new_file;

  *old_shares = 0;

  /* Read the share file header - this is of the form:
     0   -  locking version.
     4   -  number of share mode entries.
     8   -  2 byte name length
     [n bytes] file name (zero terminated).

   Followed by <n> share mode entries of the form :

     0   -  tv_sec
     4   -  tv_usec
     8   -  share_mode
    12   -  pid
    16   -  oplock port (if oplocks in use) - 2 bytes.
  */

  share_name(conn, dev, inode, fname);

  if(read_share_file( conn, fd, fname, &buf, &new_file) != 0)
  {
    DEBUG(0,("ERROR: get_share_modes: Failed to read share file %s\n",
                  fname));
    return 0;
  }

  if(new_file == True)
    return 0;

  num_entries = IVAL(buf,SMF_NUM_ENTRIES_OFFSET);

  DEBUG(5,("get_share_modes: share file %s has %d share mode entries.\n",
            fname, num_entries));

  /* PARANOIA TEST */
  if(num_entries < 0)
  {
    DEBUG(0,("PANIC ERROR:get_share_mode: num_share_mode_entries < 0 (%d) \
for share file %s\n", num_entries, fname));
    return 0;
  }

  if(num_entries)
  {
    *old_shares = share_array = (share_mode_entry *)
                 malloc(num_entries * sizeof(share_mode_entry));
    if(*old_shares == 0)
    {
      DEBUG(0,("get_share_modes: malloc fail !\n"));
      return 0;
    }
  } 
  else
  {
    /* No entries - just delete the file. */
    DEBUG(0,("get_share_modes: share file %s has no share mode entries - deleting.\n",
              fname));
    if(buf)
      free(buf);
    delete_share_file(conn, fname);
    return 0;
  }

  num_entries_copied = 0;
  base = buf + SMF_HEADER_LENGTH + SVAL(buf,SMF_FILENAME_LEN_OFFSET);

  for( i = 0; i < num_entries; i++)
  {
    pid_t pid;
    char *p = base + (i*SMF_ENTRY_LENGTH);

    pid = (pid_t)IVAL(p,SME_PID_OFFSET);

    if(!process_exists(pid))
    {
      DEBUG(0,("get_share_modes: process %d no longer exists and \
it left a share mode entry with mode 0x%X in share file %s\n",
            (int)pid, IVAL(p,SME_SHAREMODE_OFFSET), fname));
      continue;
    }
    share_array[num_entries_copied].time.tv_sec = IVAL(p,SME_SEC_OFFSET);
    share_array[num_entries_copied].time.tv_usec = IVAL(p,SME_USEC_OFFSET);
    share_array[num_entries_copied].share_mode = IVAL(p,SME_SHAREMODE_OFFSET);
    share_array[num_entries_copied].pid = pid;
    share_array[num_entries_copied].op_port = SVAL(p,SME_PORT_OFFSET);
    share_array[num_entries_copied].op_type = SVAL(p,SME_OPLOCK_TYPE_OFFSET);

    num_entries_copied++;
  }

  if(num_entries_copied == 0)
  {
    /* Delete the whole file. */
    DEBUG(0,("get_share_modes: share file %s had no valid entries - deleting it !\n",
             fname));
    if(*old_shares)
      free((char *)*old_shares);
    *old_shares = 0;
    if(buf)
      free(buf);
    delete_share_file(conn, fname);
    return 0;
  }

  /* If we deleted some entries we need to re-write the whole number of
     share mode entries back into the file. */

  if(num_entries_copied != num_entries)
  {
    if(sys_lseek(fd, (SMB_OFF_T)0, SEEK_SET) != 0)
    {
      DEBUG(0,("ERROR: get_share_modes: lseek failed to reset to \
position 0 for share mode file %s (%s)\n", fname, strerror(errno)));
      if(*old_shares)
        free((char *)*old_shares);
      *old_shares = 0;
      if(buf)
        free(buf);
      return 0;
    }

    SIVAL(buf, SMF_NUM_ENTRIES_OFFSET, num_entries_copied);
    for( i = 0; i < num_entries_copied; i++)
    {
      char *p = base + (i*SMF_ENTRY_LENGTH);

      SIVAL(p,SME_PID_OFFSET,(uint32)share_array[i].pid);
      SIVAL(p,SME_SHAREMODE_OFFSET,share_array[i].share_mode);
      SIVAL(p,SME_SEC_OFFSET,share_array[i].time.tv_sec);
      SIVAL(p,SME_USEC_OFFSET,share_array[i].time.tv_usec);
      SSVAL(p,SME_PORT_OFFSET,share_array[i].op_port);
      SSVAL(p,SME_OPLOCK_TYPE_OFFSET,share_array[i].op_type);
    }

    newsize = (base - buf) + (SMF_ENTRY_LENGTH*num_entries_copied);
    if(write(fd, buf, newsize) != newsize)
    {
      DEBUG(0,("ERROR: get_share_modes: failed to re-write share \
mode file %s (%s)\n", fname, strerror(errno)));
      if(*old_shares)
        free((char *)*old_shares);
      *old_shares = 0;
      if(buf)
        free(buf);
      return 0;
    }
    /* Now truncate the file at this point. */
#ifdef FTRUNCATE_NEEDS_ROOT
    become_root(False);
#endif /* FTRUNCATE_NEEDS_ROOT */

    if(sys_ftruncate(fd, (SMB_OFF_T)newsize)!= 0)
    {
#ifdef FTRUNCATE_NEEDS_ROOT
      int saved_errno = errno;
      unbecome_root(False);
      errno = saved_errno;
#endif /* FTRUNCATE_NEEDS_ROOT */

      DEBUG(0,("ERROR: get_share_modes: failed to ftruncate share \
mode file %s to size %d (%s)\n", fname, newsize, strerror(errno)));
      if(*old_shares)
        free((char *)*old_shares);
      *old_shares = 0;
      if(buf)
        free(buf);
      return 0;
    }
  }

#ifdef FTRUNCATE_NEEDS_ROOT
      unbecome_root(False);
#endif /* FTRUNCATE_NEEDS_ROOT */

  if(buf)
    free(buf);

  DEBUG(5,("get_share_modes: Read share file %s returning %d entries\n",fname,
            num_entries_copied));

  return num_entries_copied;
}

/*******************************************************************
del a share mode from a share mode file.
********************************************************************/
static void slow_del_share_mode(int token, files_struct *fsp)
{
  pstring fname;
  int fd = (int)token;
  char *buf = 0;
  char *base = 0;
  int num_entries;
  int newsize;
  int i;
  pid_t pid;
  BOOL deleted = False;
  BOOL new_file;

  share_name(fsp->conn, fsp->fd_ptr->dev, 
                       fsp->fd_ptr->inode, fname);

  if(read_share_file( fsp->conn, fd, fname, &buf, &new_file) != 0)
  {
    DEBUG(0,("ERROR: del_share_mode: Failed to read share file %s\n",
                  fname));
    return;
  }

  if(new_file == True)
  {
    DEBUG(0,("ERROR:del_share_mode: share file %s is new (size zero), deleting it.\n",
              fname));
    delete_share_file(fsp->conn, fname);
    return;
  }

  num_entries = IVAL(buf,SMF_NUM_ENTRIES_OFFSET);

  DEBUG(5,("del_share_mode: share file %s has %d share mode entries.\n",
            fname, num_entries));

  /* PARANOIA TEST */
  if(num_entries < 0)
  {
    DEBUG(0,("PANIC ERROR:del_share_mode: num_share_mode_entries < 0 (%d) \
for share file %s\n", num_entries, fname));
    return;
  }

  if(num_entries == 0)
  {
    /* No entries - just delete the file. */
    DEBUG(0,("del_share_mode: share file %s has no share mode entries - deleting.\n",
              fname));
    if(buf)
      free(buf);
    delete_share_file(fsp->conn, fname);
    return;
  }

  pid = getpid();

  /* Go through the entries looking for the particular one
     we have set - delete it.
  */

  base = buf + SMF_HEADER_LENGTH + SVAL(buf,SMF_FILENAME_LEN_OFFSET);

  for(i = 0; i < num_entries; i++)
  {
    char *p = base + (i*SMF_ENTRY_LENGTH);

    if((IVAL(p,SME_SEC_OFFSET) != fsp->open_time.tv_sec) || 
       (IVAL(p,SME_USEC_OFFSET) != fsp->open_time.tv_usec) ||
       (IVAL(p,SME_SHAREMODE_OFFSET) != fsp->share_mode) || 
       (((pid_t)IVAL(p,SME_PID_OFFSET)) != pid))
      continue;

    DEBUG(5,("del_share_mode: deleting entry number %d (of %d) from the share file %s\n",
             i, num_entries, fname));

    /* Remove this entry. */
    if(i != num_entries - 1)
      memcpy(p, p + SMF_ENTRY_LENGTH, (num_entries - i - 1)*SMF_ENTRY_LENGTH);

    deleted = True;
    break;
  }

  if(!deleted)
  {
    DEBUG(0,("del_share_mode: entry not found in share file %s\n", fname));
    if(buf)
      free(buf);
    return;
  }

  num_entries--;
  SIVAL(buf,SMF_NUM_ENTRIES_OFFSET, num_entries);

  if(num_entries == 0)
  {
    /* Deleted the last entry - remove the file. */
    DEBUG(5,("del_share_mode: removed last entry in share file - deleting share file %s\n",
             fname));
    if(buf)
      free(buf);
    delete_share_file(fsp->conn,fname);
    return;
  }

  /* Re-write the file - and truncate it at the correct point. */
  if(sys_lseek(fd, (SMB_OFF_T)0, SEEK_SET) != 0)
  {
    DEBUG(0,("ERROR: del_share_mode: lseek failed to reset to \
position 0 for share mode file %s (%s)\n", fname, strerror(errno)));
    if(buf)
      free(buf);
    return;
  }

  newsize = (base - buf) + (SMF_ENTRY_LENGTH*num_entries);
  if(write(fd, buf, newsize) != newsize)
  {
    DEBUG(0,("ERROR: del_share_mode: failed to re-write share \
mode file %s (%s)\n", fname, strerror(errno)));
    if(buf)
      free(buf);
    return;
  }

  /* Now truncate the file at this point. */
#ifdef FTRUNCATE_NEEDS_ROOT
  become_root(False);
#endif /* FTRUNCATE_NEEDS_ROOT */

  if(sys_ftruncate(fd, (SMB_OFF_T)newsize) != 0)
  {
#ifdef FTRUNCATE_NEEDS_ROOT
    int saved_errno = errno;
    unbecome_root(False);
    errno = saved_errno;
#endif /* FTRUNCATE_NEEDS_ROOT */


    DEBUG(0,("ERROR: del_share_mode: failed to ftruncate share \
mode file %s to size %d (%s)\n", fname, newsize, strerror(errno)));
    if(buf)
      free(buf);
    return;
  }

#ifdef FTRUNCATE_NEEDS_ROOT
  unbecome_root(False);
#endif /* FTRUNCATE_NEEDS_ROOT */

}
  
/*******************************************************************
set the share mode of a file
********************************************************************/
static BOOL slow_set_share_mode(int token,files_struct *fsp, uint16 port, uint16 op_type)
{
  pstring fname;
  int fd = (int)token;
  pid_t pid = getpid();
  SMB_STRUCT_STAT sb;
  char *buf;
  int num_entries;
  int header_size;
  char *p;

  share_name(fsp->conn, fsp->fd_ptr->dev,
                       fsp->fd_ptr->inode, fname);

  if(sys_fstat(fd, &sb) != 0)
  {
    DEBUG(0,("ERROR: set_share_mode: Failed to do stat on share file %s\n",
                  fname));
    return False;
  }

  /* Sanity check for file contents (if it's not a new share file). */
  if(sb.st_size != 0)
  {
    SMB_OFF_T size = sb.st_size;

    /* Allocate space for the file plus one extra entry */
    if((buf = (char *)malloc((size_t)(sb.st_size + SMF_ENTRY_LENGTH))) == NULL)
    {
      DEBUG(0,("set_share_mode: malloc for file size %d fail !\n", 
	       (int)(sb.st_size + SMF_ENTRY_LENGTH)));
      return False;
    }
 
    if(sys_lseek(fd, (SMB_OFF_T)0, SEEK_SET) != 0)
    {
      DEBUG(0,("ERROR: set_share_mode: Failed to reset position \
to 0 for share file %s (%s)\n", fname, strerror(errno)));
      if(buf)
        free(buf);
      return False;
    }

    if (read(fd,buf,(size_t)sb.st_size) != (size_t)sb.st_size)
    {
      DEBUG(0,("ERROR: set_share_mode: Failed to read share file %s (%s)\n",
                  fname, strerror(errno)));
      if(buf)
        free(buf);
      return False;
    }   
  
    if (IVAL(buf,SMF_VERSION_OFFSET) != LOCKING_VERSION) 
    {
      DEBUG(0,("ERROR: set_share_mode: share file %s has incorrect \
locking version (was %d, should be %d).\n",fname, IVAL(buf,SMF_VERSION_OFFSET), 
                    LOCKING_VERSION));
      if(buf)
        free(buf);
      delete_share_file(fsp->conn, fname);
      return False;
    }   

    size -= (SMF_HEADER_LENGTH + SVAL(buf, SMF_FILENAME_LEN_OFFSET)); /* Remove the header */

    /* The remaining size must be a multiple of SMF_ENTRY_LENGTH - error if not. */
    if((size % SMF_ENTRY_LENGTH) != 0)
    {
      DEBUG(0,("ERROR: set_share_mode: share file %s is an incorrect length - \
deleting it.\n", fname));
      if(buf)
        free(buf);
      delete_share_file(fsp->conn, fname);
      return False;
    }

  }
  else
  {
    /* New file - just use a single_entry. */
    if((buf = (char *)malloc(SMF_HEADER_LENGTH + 
                  strlen(fsp->fsp_name) + strlen(fsp->conn->connectpath) + 2 + SMF_ENTRY_LENGTH)) == NULL)
    {
      DEBUG(0,("ERROR: set_share_mode: malloc failed for single entry.\n"));
      return False;
    }
    SIVAL(buf,SMF_VERSION_OFFSET,LOCKING_VERSION);
    SIVAL(buf,SMF_NUM_ENTRIES_OFFSET,0);
    SSVAL(buf,SMF_FILENAME_LEN_OFFSET,strlen(fsp->fsp_name) + strlen(fsp->conn->connectpath) + 2);
    pstrcpy(buf + SMF_HEADER_LENGTH, fsp->conn->connectpath);
    pstrcat(buf + SMF_HEADER_LENGTH, "/");
    pstrcat(buf + SMF_HEADER_LENGTH, fsp->fsp_name);
  }

  num_entries = IVAL(buf,SMF_NUM_ENTRIES_OFFSET);
  header_size = SMF_HEADER_LENGTH + SVAL(buf,SMF_FILENAME_LEN_OFFSET);
  p = buf + header_size + (num_entries * SMF_ENTRY_LENGTH);
  SIVAL(p,SME_SEC_OFFSET,fsp->open_time.tv_sec);
  SIVAL(p,SME_USEC_OFFSET,fsp->open_time.tv_usec);
  SIVAL(p,SME_SHAREMODE_OFFSET,fsp->share_mode);
  SIVAL(p,SME_PID_OFFSET,(uint32)pid);
  SSVAL(p,SME_PORT_OFFSET,port);
  SSVAL(p,SME_OPLOCK_TYPE_OFFSET,op_type);

  num_entries++;

  SIVAL(buf,SMF_NUM_ENTRIES_OFFSET,num_entries);

  if(sys_lseek(fd, (SMB_OFF_T)0, SEEK_SET) != 0)
  {
    DEBUG(0,("ERROR: set_share_mode: (1) Failed to reset position to \
0 for share file %s (%s)\n", fname, strerror(errno)));
    if(buf)
      free(buf);
    return False;
  }

  if (write(fd,buf,header_size + (num_entries*SMF_ENTRY_LENGTH)) != 
                       (header_size + (num_entries*SMF_ENTRY_LENGTH))) 
  {
    DEBUG(2,("ERROR: set_share_mode: Failed to write share file %s - \
deleting it (%s).\n",fname, strerror(errno)));
    delete_share_file(fsp->conn, fname);
    if(buf)
      free(buf);
    return False;
  }

  /* Now truncate the file at this point - just for safety. */

#ifdef FTRUNCATE_NEEDS_ROOT
  become_root(False);
#endif /* FTRUNCATE_NEEDS_ROOT */

  if(sys_ftruncate(fd, (SMB_OFF_T)(header_size + (SMF_ENTRY_LENGTH*num_entries)))!= 0)
  {

#ifdef FTRUNCATE_NEEDS_ROOT
    int saved_errno = errno;
    unbecome_root(False);
    errno = saved_errno;
#endif /* FTRUNCATE_NEEDS_ROOT */

    DEBUG(0,("ERROR: set_share_mode: failed to ftruncate share \
mode file %s to size %d (%s)\n", fname, header_size + (SMF_ENTRY_LENGTH*num_entries), 
                strerror(errno)));
    if(buf)
      free(buf);
    return False;
  }

#ifdef FTRUNCATE_NEEDS_ROOT
  unbecome_root(False);
#endif /* FTRUNCATE_NEEDS_ROOT */

  if(buf)
    free(buf);

  DEBUG(3,("set_share_mode: Created share file %s with \
mode 0x%X pid=%d\n",fname,fsp->share_mode,(int)pid));

  return True;
}

/*******************************************************************
 Call a generic modify function for a share mode entry.
********************************************************************/

static BOOL slow_mod_share_entry(int token, files_struct *fsp,
                                void (*mod_fn)(share_mode_entry *, SMB_DEV_T, SMB_INO_T, void *),
                                void *param)
{
  pstring fname;
  int fd = (int)token;
  char *buf = 0;
  char *base = 0;
  int num_entries;
  int fsize;
  int i;
  pid_t pid;
  BOOL found = False;
  BOOL new_file;
  share_mode_entry entry;

  share_name(fsp->conn, fsp->fd_ptr->dev, 
                       fsp->fd_ptr->inode, fname);

  if(read_share_file( fsp->conn, fd, fname, &buf, &new_file) != 0)
  {
    DEBUG(0,("ERROR: slow_mod_share_entry: Failed to read share file %s\n",
                  fname));
    return False;
  }

  if(new_file == True)
  {
    DEBUG(0,("ERROR: slow_mod_share_entry: share file %s is new (size zero), \
deleting it.\n", fname));
    delete_share_file(fsp->conn, fname);
    return False;
  }

  num_entries = IVAL(buf,SMF_NUM_ENTRIES_OFFSET);

  DEBUG(5,("slow_mod_share_entry: share file %s has %d share mode entries.\n",
            fname, num_entries));

  /* PARANOIA TEST */
  if(num_entries < 0)
  {
    DEBUG(0,("PANIC ERROR:slow_mod_share_entry: num_share_mode_entries < 0 (%d) \
for share file %s\n", num_entries, fname));
    return False;
  }

  if(num_entries == 0)
  {
    /* No entries - just delete the file. */
    DEBUG(0,("slow_mod_share_entry: share file %s has no share mode entries - deleting.\n",
              fname));
    if(buf)
      free(buf);
    delete_share_file(fsp->conn, fname);
    return False;
  }

  pid = getpid();

  base = buf + SMF_HEADER_LENGTH + SVAL(buf,SMF_FILENAME_LEN_OFFSET);

  for(i = 0; i < num_entries; i++)
  {
    char *p = base + (i*SMF_ENTRY_LENGTH);

    if((IVAL(p,SME_SEC_OFFSET) != fsp->open_time.tv_sec) || 
       (IVAL(p,SME_USEC_OFFSET) != fsp->open_time.tv_usec) ||
       (IVAL(p,SME_SHAREMODE_OFFSET) != fsp->share_mode) || 
       (((pid_t)IVAL(p,SME_PID_OFFSET)) != pid))
      continue;

    DEBUG(5,("slow_mod_share_entry: Calling generic function to modify entry number %d (of %d) \
from the share file %s\n", i, num_entries, fname));

    /*
     * Copy into the share_mode_entry structure and then call 
     * the generic function with the given parameter.
     */

    entry.pid = (pid_t)IVAL(p,SME_PID_OFFSET);
    entry.op_port = SVAL(p,SME_PORT_OFFSET);
    entry.op_type = SVAL(p,SME_OPLOCK_TYPE_OFFSET);
    entry.share_mode = IVAL(p,SME_SHAREMODE_OFFSET);
    entry.time.tv_sec = IVAL(p,SME_SEC_OFFSET);
    entry.time.tv_usec = IVAL(p,SME_USEC_OFFSET);

    (*mod_fn)( &entry, fsp->fd_ptr->dev, fsp->fd_ptr->inode, param);

    /*
     * Now copy any changes the function made back into the buffer.
     */

    SIVAL(p,SME_PID_OFFSET, (uint32)entry.pid);
    SSVAL(p,SME_PORT_OFFSET,entry.op_port);
    SSVAL(p,SME_OPLOCK_TYPE_OFFSET,entry.op_type);
    SIVAL(p,SME_SHAREMODE_OFFSET,entry.share_mode);
    SIVAL(p,SME_SEC_OFFSET,entry.time.tv_sec);
    SIVAL(p,SME_USEC_OFFSET,entry.time.tv_usec);

    found = True;
    break;
  }

  if(!found)
  {
    DEBUG(0,("slow_mod_share_entry: entry not found in share file %s\n", fname));
    if(buf)
      free(buf);
    return False;
  }

  /* Re-write the file - and truncate it at the correct point. */
  if(sys_lseek(fd, (SMB_OFF_T)0, SEEK_SET) != 0)
  {
    DEBUG(0,("ERROR: slow_mod_share_entry: lseek failed to reset to \
position 0 for share mode file %s (%s)\n", fname, strerror(errno)));
    if(buf)
      free(buf);
    return False;
  }

  fsize = (base - buf) + (SMF_ENTRY_LENGTH*num_entries);
  if(write(fd, buf, fsize) != fsize)
  {
    DEBUG(0,("ERROR: slow_mod_share_entry: failed to re-write share \
mode file %s (%s)\n", fname, strerror(errno)));
    if(buf)
      free(buf);
    return False;
  }

  return True;
}



/*******************************************************************
call the specified function on each entry under management by the
share mode system
********************************************************************/
static int slow_share_forall(void (*fn)(share_mode_entry *, char *))
{
	int i, count=0;
	DIR *dir;
	char *s;
	share_mode_entry e;

	dir = opendir(lp_lockdir());
	if (!dir) {
		return(0);
	}

	while ((s=readdirname(dir))) {
		char *buf;
		char *base;
		int fd;
		pstring lname;
		SMB_DEV_T dev;
        SMB_INO_T inode;
		BOOL new_file;
		pstring fname;

#ifdef LARGE_SMB_INO_T
        double inode_ascii;
		if (sscanf(s,"share.%u.%lf",&dev,&inode_ascii)!=2) continue;
        inode = (SMB_INO_T)inode_ascii;
#else /* LARGE_SMB_INO_T */
        unsigned long inode_long;
		if (sscanf(s,"share.%u.%lu",&dev,&inode_long)!=2) continue;
        inode = (SMB_INO_T)inode_long;
#endif /* LARGE_SMB_INO_T */       

		pstrcpy(lname,lp_lockdir());
		trim_string(lname,NULL,"/");
		pstrcat(lname,"/");
		pstrcat(lname,s);
       
		fd = sys_open(lname,read_only?O_RDONLY:O_RDWR,0);
		if (fd < 0) {
			continue;
		}

		/* Lock the share mode file while we read it. */
		if(!read_only &&
		   fcntl_lock(fd, SMB_F_SETLKW, 0, 1, F_WRLCK) == False) {
			close(fd);
			continue;
		}

		if(read_share_file( 0, fd, lname, &buf, &new_file)) {
			close(fd);
			continue;
		} 

		if(new_file == True) {
			close(fd);
			continue;
		} 

		pstrcpy( fname, &buf[10]);
		close(fd);
      
		base = buf + SMF_HEADER_LENGTH + 
			SVAL(buf,SMF_FILENAME_LEN_OFFSET); 
		for( i = 0; i < IVAL(buf, SMF_NUM_ENTRIES_OFFSET); i++) {
			char *p = base + (i*SMF_ENTRY_LENGTH);
			e.pid = (pid_t)IVAL(p,SME_PID_OFFSET);
			e.share_mode = IVAL(p,SME_SHAREMODE_OFFSET);
			e.time.tv_sec = IVAL(p,SME_SEC_OFFSET);
			e.time.tv_usec = IVAL(p,SME_USEC_OFFSET);
			e.op_port = SVAL(p,SME_PORT_OFFSET);
			e.op_type = SVAL(p,SME_OPLOCK_TYPE_OFFSET);

			if (process_exists(e.pid)) {
				fn(&e, fname);
				count++;
			}
		} /* end for i */

		if(buf)
			free(buf);
		base = 0;
	} /* end while */
	closedir(dir);

	return count;
}


/*******************************************************************
dump the state of the system
********************************************************************/
static void slow_share_status(FILE *f)
{
	
}


static struct share_ops share_ops = {
	slow_stop_share_mode_mgmt,
	slow_lock_share_entry,
	slow_unlock_share_entry,
	slow_get_share_modes,
	slow_del_share_mode,
	slow_set_share_mode,
	slow_mod_share_entry,
	slow_share_forall,
	slow_share_status,
};

/*******************************************************************
  initialize the slow share_mode management 
  ******************************************************************/
struct share_ops *locking_slow_init(int ronly)
{

	read_only = ronly;

	if (!directory_exist(lp_lockdir(),NULL)) {
		if (!read_only)
			mkdir(lp_lockdir(),0755);
		if (!directory_exist(lp_lockdir(),NULL))
			return NULL;
	}

	return &share_ops;
}
#else
 int locking_slow_dummy_procedure(void);
 int locking_slow_dummy_procedure(void) {return 0;}
#endif /* !FAST_SHARE_MODES */

/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Locking functions
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

*/

#include "includes.h"
extern int DEBUGLEVEL;

static struct share_ops *share_ops;

/****************************************************************************
 Utility function to map a lock type correctly depending on the real open
 mode of a file.
****************************************************************************/

static int map_lock_type( files_struct *fsp, int lock_type)
{
  if((lock_type == F_WRLCK) && (fsp->fd_ptr->real_open_flags == O_RDONLY)) {
    /*
     * Many UNIX's cannot get a write lock on a file opened read-only.
     * Win32 locking semantics allow this.
     * Do the best we can and attempt a read-only lock.
     */
    DEBUG(10,("map_lock_type: Downgrading write lock to read due to read-only file.\n"));
    return F_RDLCK;
  } else if( (lock_type == F_RDLCK) && (fsp->fd_ptr->real_open_flags == O_WRONLY)) {
    /*
     * Ditto for read locks on write only files.
     */
    DEBUG(10,("map_lock_type: Changing read lock to write due to write-only file.\n"));
    return F_WRLCK;
  }

  /*
   * This return should be the most normal, as we attempt
   * to always open files read/write.
   */

  return lock_type;
}

/****************************************************************************
 Utility function called to see if a file region is locked.
****************************************************************************/
BOOL is_locked(files_struct *fsp,connection_struct *conn,
	       SMB_OFF_T count,SMB_OFF_T offset, int lock_type)
{
	int snum = SNUM(conn);

	if (count == 0)
		return(False);

	if (!lp_locking(snum) || !lp_strict_locking(snum))
		return(False);

	/*
	 * Note that most UNIX's can *test* for a write lock on
	 * a read-only fd, just not *set* a write lock on a read-only
	 * fd. So we don't need to use map_lock_type here.
	 */
	
	return(fcntl_lock(fsp->fd_ptr->fd,SMB_F_GETLK,offset,count,lock_type));
}


/****************************************************************************
 Utility function called by locking requests.
****************************************************************************/
BOOL do_lock(files_struct *fsp,connection_struct *conn,
             SMB_OFF_T count,SMB_OFF_T offset,int lock_type,
             int *eclass,uint32 *ecode)
{
  BOOL ok = False;

  if (!lp_locking(SNUM(conn)))
    return(True);

  if (count == 0) {
    *eclass = ERRDOS;
    *ecode = ERRnoaccess;
    return False;
  }

  DEBUG(10,("do_lock: lock type %d start=%.0f len=%.0f requested for file %s\n",
        lock_type, (double)offset, (double)count, fsp->fsp_name ));

  if (OPEN_FSP(fsp) && fsp->can_lock && (fsp->conn == conn))
    ok = fcntl_lock(fsp->fd_ptr->fd,SMB_F_SETLK,offset,count,
                    map_lock_type(fsp,lock_type));

  if (!ok) {
    *eclass = ERRDOS;
    *ecode = ERRlock;
    return False;
  }
  return True; /* Got lock */
}


/****************************************************************************
 Utility function called by unlocking requests.
****************************************************************************/
BOOL do_unlock(files_struct *fsp,connection_struct *conn,
               SMB_OFF_T count,SMB_OFF_T offset,int *eclass,uint32 *ecode)
{
  BOOL ok = False;

  if (!lp_locking(SNUM(conn)))
    return(True);

  DEBUG(10,("do_unlock: unlock start=%.0f len=%.0f requested for file %s\n",
        (double)offset, (double)count, fsp->fsp_name ));

  if (OPEN_FSP(fsp) && fsp->can_lock && (fsp->conn == conn))
    ok = fcntl_lock(fsp->fd_ptr->fd,SMB_F_SETLK,offset,count,F_UNLCK);
   
  if (!ok) {
    *eclass = ERRDOS;
    *ecode = ERRlock;
    return False;
  }
  return True; /* Did unlock */
}

/****************************************************************************
 Initialise the locking functions.
****************************************************************************/

BOOL locking_init(int read_only)
{
	if (share_ops)
		return True;

#ifdef FAST_SHARE_MODES
	share_ops = locking_shm_init(read_only);
	if (!share_ops && read_only && (getuid() == 0)) {
		/* this may be the first time the share modes code has
                   been run. Initialise it now by running it read-write */
		share_ops = locking_shm_init(0);
	}
#else
	share_ops = locking_slow_init(read_only);
#endif

	if (!share_ops) {
		DEBUG(0,("ERROR: Failed to initialise share modes\n"));
		return False;
	}
	
	return True;
}

/*******************************************************************
 Deinitialize the share_mode management.
******************************************************************/

BOOL locking_end(void)
{
	if (share_ops)
		return share_ops->stop_mgmt();
	return True;
}


/*******************************************************************
 Lock a hash bucket entry.
******************************************************************/
BOOL lock_share_entry(connection_struct *conn,
		      SMB_DEV_T dev, SMB_INO_T inode, int *ptok)
{
	return share_ops->lock_entry(conn, dev, inode, ptok);
}

/*******************************************************************
 Unlock a hash bucket entry.
******************************************************************/
BOOL unlock_share_entry(connection_struct *conn,
			SMB_DEV_T dev, SMB_INO_T inode, int token)
{
	return share_ops->unlock_entry(conn, dev, inode, token);
}

/*******************************************************************
 Get all share mode entries for a dev/inode pair.
********************************************************************/
int get_share_modes(connection_struct *conn, 
		    int token, SMB_DEV_T dev, SMB_INO_T inode, 
		    share_mode_entry **shares)
{
	return share_ops->get_entries(conn, token, dev, inode, shares);
}

/*******************************************************************
 Del the share mode of a file.
********************************************************************/
void del_share_mode(int token, files_struct *fsp)
{
	share_ops->del_entry(token, fsp);
}

/*******************************************************************
 Set the share mode of a file. Return False on fail, True on success.
********************************************************************/
BOOL set_share_mode(int token, files_struct *fsp, uint16 port, uint16 op_type)
{
	return share_ops->set_entry(token, fsp, port, op_type);
}

/*******************************************************************
 Static function that actually does the work for the generic function
 below.
********************************************************************/

static void remove_share_oplock_fn(share_mode_entry *entry, SMB_DEV_T dev, SMB_INO_T inode, 
                                   void *param)
{
  DEBUG(10,("remove_share_oplock_fn: removing oplock info for entry dev=%x ino=%.0f\n",
        (unsigned int)dev, (double)inode ));
  /* Delete the oplock info. */
  entry->op_port = 0;
  entry->op_type = NO_OPLOCK;
}

/*******************************************************************
 Remove an oplock port and mode entry from a share mode.
********************************************************************/

BOOL remove_share_oplock(int token, files_struct *fsp)
{
	return share_ops->mod_entry(token, fsp, remove_share_oplock_fn, NULL);
}

/*******************************************************************
 Static function that actually does the work for the generic function
 below.
********************************************************************/

static void downgrade_share_oplock_fn(share_mode_entry *entry, SMB_DEV_T dev, SMB_INO_T inode, 
                                   void *param)
{
  DEBUG(10,("downgrade_share_oplock_fn: downgrading oplock info for entry dev=%x ino=%.0f\n",
        (unsigned int)dev, (double)inode ));
  entry->op_type = LEVEL_II_OPLOCK;
}

/*******************************************************************
 Downgrade a oplock type from exclusive to level II.
********************************************************************/

BOOL downgrade_share_oplock(int token, files_struct *fsp)
{
    return share_ops->mod_entry(token, fsp, downgrade_share_oplock_fn, NULL);
}

/*******************************************************************
 Static function that actually does the work for the generic function
 below.
********************************************************************/

struct mod_val {
	int new_share_mode;
	uint16 new_oplock;
};

static void modify_share_mode_fn(share_mode_entry *entry, SMB_DEV_T dev, SMB_INO_T inode, 
                                   void *param)
{
	struct mod_val *mvp = (struct mod_val *)param;

	DEBUG(10,("modify_share_mode_fn: changing share mode info from %x to %x for entry dev=%x ino=%.0f\n",
        entry->share_mode, mvp->new_share_mode, (unsigned int)dev, (double)inode ));
	DEBUG(10,("modify_share_mode_fn: changing oplock state from %x to %x for entry dev=%x ino=%.0f\n",
        entry->op_type, (int)mvp->new_oplock, (unsigned int)dev, (double)inode ));
	/* Change the share mode info. */
	entry->share_mode = mvp->new_share_mode;
	entry->op_type = mvp->new_oplock;
}

/*******************************************************************
 Modify a share mode on a file. Used by the delete open file code.
 Return False on fail, True on success.
********************************************************************/

BOOL modify_share_mode(int token, files_struct *fsp, int new_mode, uint16 new_oplock)
{
	struct mod_val mv;

	mv.new_share_mode = new_mode;
    mv.new_oplock = new_oplock;

	return share_ops->mod_entry(token, fsp, modify_share_mode_fn, (void *)&mv);
}

/*******************************************************************
 Call the specified function on each entry under management by the
 share mode system.
********************************************************************/

int share_mode_forall(void (*fn)(share_mode_entry *, char *))
{
	if (!share_ops) return 0;
	return share_ops->forall(fn);
}

/*******************************************************************
 Dump the state of the system.
********************************************************************/

void share_status(FILE *f)
{
	share_ops->status(f);
}

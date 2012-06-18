/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Files[] structure handling
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

extern int DEBUGLEVEL;

static int real_max_open_files;

#define VALID_FNUM(fnum)   (((fnum) >= 0) && ((fnum) < real_max_open_files))

#define FILE_HANDLE_OFFSET 0x1000

static struct bitmap *file_bmap;

static files_struct *Files;
 
/* a fsp to use when chaining */
static files_struct *chain_fsp = NULL;
/* a fsp to use to save when breaking an oplock. */
static files_struct *oplock_save_chain_fsp = NULL;

/*
 * Indirection for file fd's. Needed as POSIX locking
 * is based on file/process, not fd/process.
 */
static file_fd_struct *FileFd;

static int files_used, fd_ptr_used;

/****************************************************************************
  find first available file slot
****************************************************************************/
files_struct *file_new(void )
{
	int i;
	static int first_file;
	files_struct *fsp, *next;

	/* we want to give out file handles differently on each new
	   connection because of a common bug in MS clients where they try to
	   reuse a file descriptor from an earlier smb connection. This code
	   increases the chance that the errant client will get an error rather
	   than causing corruption */
	if (first_file == 0) {
		first_file = (getpid() ^ (int)time(NULL)) % real_max_open_files;
	}

	i = bitmap_find(file_bmap, first_file);
	if (i == -1) {
		/* 
		 * Before we give up, go through the open files 
		 * and see if there are any files opened with a
		 * batch oplock. If so break the oplock and then
		 * re-use that entry (if it becomes closed).
		 * This may help as NT/95 clients tend to keep
		 * files batch oplocked for quite a long time
		 * after they have finished with them.
		 */
		for (fsp=Files;fsp;fsp=next) {
			next=fsp->next;
			if (attempt_close_oplocked_file(fsp)) {
				return file_new();
			}
		}

		DEBUG(0,("ERROR! Out of file structures\n"));
		return NULL;
	}

	fsp = (files_struct *)malloc(sizeof(*fsp));
	if (!fsp) return NULL;

	ZERO_STRUCTP(fsp);

	first_file = (i+1) % real_max_open_files;

	bitmap_set(file_bmap, i);
	files_used++;

	fsp->fnum = i + FILE_HANDLE_OFFSET;
	string_set(&fsp->fsp_name,"");
	
	DLIST_ADD(Files, fsp);

	DEBUG(5,("allocated file structure %d, fnum = %d (%d used)\n",
		 i, fsp->fnum, files_used));

	chain_fsp = fsp;
	
	return fsp;
}



/****************************************************************************
fd support routines - attempt to find an already open file by dev
and inode - increments the ref_count of the returned file_fd_struct *.
****************************************************************************/
file_fd_struct *fd_get_already_open(SMB_STRUCT_STAT *sbuf)
{
	file_fd_struct *fd_ptr;

	if(!sbuf) return NULL;

	for (fd_ptr=FileFd;fd_ptr;fd_ptr=fd_ptr->next) {
		if ((fd_ptr->ref_count > 0) &&
		    (sbuf->st_dev == fd_ptr->dev) &&
		    (sbuf->st_ino == fd_ptr->inode)) {
			fd_ptr->ref_count++;

			DEBUG(3,("Re-used file_fd_struct dev = %x, inode = %.0f, ref_count = %d\n",
				 (unsigned int)fd_ptr->dev, (double)fd_ptr->inode, 
				 fd_ptr->ref_count));

			return fd_ptr;
		}
	}

	return NULL;
}



/****************************************************************************
fd support routines - attempt to find a empty slot in the FileFd array.
Increments the ref_count of the returned entry.
****************************************************************************/
file_fd_struct *fd_get_new(void)
{
	extern struct current_user current_user;
	file_fd_struct *fd_ptr;

	fd_ptr = (file_fd_struct *)malloc(sizeof(*fd_ptr));
	if (!fd_ptr) {
          DEBUG(0,("ERROR! malloc fail for file_fd struct.\n"));
          return NULL;
	}
	
	ZERO_STRUCTP(fd_ptr);
	
	fd_ptr->dev = (SMB_DEV_T)-1;
	fd_ptr->inode = (SMB_INO_T)-1;
	fd_ptr->fd = -1;
	fd_ptr->fd_readonly = -1;
	fd_ptr->fd_writeonly = -1;
	fd_ptr->real_open_flags = -1;
	fd_add_to_uid_cache(fd_ptr, (uid_t)current_user.uid);
	fd_ptr->ref_count++;

	fd_ptr_used++;

	DLIST_ADD(FileFd, fd_ptr);

	DEBUG(5,("allocated fd_ptr structure (%d used)\n", fd_ptr_used));

	return fd_ptr;
}


/****************************************************************************
close all open files for a connection
****************************************************************************/
void file_close_conn(connection_struct *conn)
{
	files_struct *fsp, *next;
	
	for (fsp=Files;fsp;fsp=next) {
		next = fsp->next;
		if (fsp->conn == conn && fsp->open) {
			close_file(fsp,False); 
		}
	}
}

/****************************************************************************
initialise file structures
****************************************************************************/

#define MAX_OPEN_FUDGEFACTOR 10

void file_init(void)
{
        int request_max_open_files = lp_max_open_files();
	int real_lim;

	/*
	 * Set the max_open files to be the requested
	 * max plus a fudgefactor to allow for the extra
	 * fd's we need such as log files etc...
	 */
	real_lim = set_maxfiles(request_max_open_files + MAX_OPEN_FUDGEFACTOR);

	real_max_open_files = real_lim - MAX_OPEN_FUDGEFACTOR;

        if(real_max_open_files != request_max_open_files) {
        	DEBUG(1,("file_init: Information only: requested %d \
open files, %d are available.\n", request_max_open_files, real_max_open_files));
	}

	file_bmap = bitmap_allocate(real_max_open_files);
	
	if (!file_bmap) {
		exit_server("out of memory in file_init");
	}
	
	/*
	 * Ensure that pipe_handle_oppset is set correctly.
	 */
	set_pipe_handle_offset(real_max_open_files);
}


/****************************************************************************
close files open by a specified vuid
****************************************************************************/
void file_close_user(int vuid)
{
	files_struct *fsp, *next;

	for (fsp=Files;fsp;fsp=next) {
		next=fsp->next;
		if ((fsp->vuid == vuid) && fsp->open) {
			close_file(fsp,False);
		}
	}
}


/****************************************************************************
 Find a fsp given a device, inode and timevalue
 If this is from a kernel oplock break request then tval may be NULL.
****************************************************************************/

files_struct *file_find_dit(SMB_DEV_T dev, SMB_INO_T inode, struct timeval *tval)
{
	int count=0;
	files_struct *fsp;

	for (fsp=Files;fsp;fsp=fsp->next,count++) {
		if (fsp->open && 
			fsp->fd_ptr != NULL &&
		    fsp->fd_ptr->dev == dev && 
		    fsp->fd_ptr->inode == inode &&
		    (tval ? (fsp->open_time.tv_sec == tval->tv_sec) : True ) &&
		    (tval ? (fsp->open_time.tv_usec == tval->tv_usec) : True )) {
			if (count > 10) {
				DLIST_PROMOTE(Files, fsp);
			}
			return fsp;
		}
	}

	return NULL;
}

/****************************************************************************
 Check if an fsp still exists.
****************************************************************************/

files_struct *file_find_fsp(files_struct *orig_fsp)
{
	files_struct *fsp;

	for (fsp=Files;fsp;fsp=fsp->next) {
		if (fsp == orig_fsp)
			return fsp;
	}

	return NULL;
}

/****************************************************************************
 Find the first fsp given a device and inode.
****************************************************************************/

files_struct *file_find_di_first(SMB_DEV_T dev, SMB_INO_T inode)
{
    files_struct *fsp;

    for (fsp=Files;fsp;fsp=fsp->next) {
        if (fsp->open &&
			fsp->fd_ptr != NULL &&
            fsp->fd_ptr->dev == dev &&
            fsp->fd_ptr->inode == inode )
            return fsp;
    }

    return NULL;
}

/****************************************************************************
 Find the next fsp having the same device and inode.
****************************************************************************/

files_struct *file_find_di_next(files_struct *start_fsp)
{
    files_struct *fsp;

    for (fsp = start_fsp->next;fsp;fsp=fsp->next) {
        if (fsp->open &&
			fsp->fd_ptr != NULL &&
            fsp->fd_ptr->dev == start_fsp->fd_ptr->dev &&
            fsp->fd_ptr->inode == start_fsp->fd_ptr->inode )
            return fsp;
    }

    return NULL;
}

/****************************************************************************
find a fsp that is open for printing
****************************************************************************/
files_struct *file_find_print(void)
{
	files_struct *fsp;

	for (fsp=Files;fsp;fsp=fsp->next) {
		if (fsp->open && fsp->print_file) return fsp;
	} 

	return NULL;
}


/****************************************************************************
sync open files on a connection
****************************************************************************/
void file_sync_all(connection_struct *conn)
{
	files_struct *fsp, *next;

	for (fsp=Files;fsp;fsp=next) {
		next=fsp->next;
		if (fsp->open && (conn == fsp->conn) && (fsp->fd_ptr != NULL)) {
			sync_file(conn,fsp);
		}
	}
}


/****************************************************************************
free up a fd_ptr
****************************************************************************/
void fd_ptr_free(file_fd_struct *fd_ptr)
{
	DLIST_REMOVE(FileFd, fd_ptr);

	fd_ptr_used--;

	DEBUG(5,("freed fd_ptr structure (%d used)\n", fd_ptr_used));

	/* paranoia */
	ZERO_STRUCTP(fd_ptr);

	free(fd_ptr);
}


/****************************************************************************
free up a fsp
****************************************************************************/
void file_free(files_struct *fsp)
{
	DLIST_REMOVE(Files, fsp);

	string_free(&fsp->fsp_name);

	if ((fsp->fd_ptr != NULL) && fsp->fd_ptr->ref_count == 0) {
		fd_ptr_free(fsp->fd_ptr);
	}

	bitmap_clear(file_bmap, fsp->fnum - FILE_HANDLE_OFFSET);
	files_used--;

	DEBUG(5,("freed files structure %d (%d used)\n",
		 fsp->fnum, files_used));

	/* this is paranoia, just in case someone tries to reuse the 
	   information */
	ZERO_STRUCTP(fsp);

	if (fsp == chain_fsp) chain_fsp = NULL;

	free(fsp);
}


/****************************************************************************
get a fsp from a packet given the offset of a 16 bit fnum
****************************************************************************/
files_struct *file_fsp(char *buf, int where)
{
	int fnum, count=0;
	files_struct *fsp;

	if (chain_fsp) return chain_fsp;

	fnum = SVAL(buf, where);

	for (fsp=Files;fsp;fsp=fsp->next, count++) {
		if (fsp->fnum == fnum) {
			chain_fsp = fsp;
			if (count > 10) {
				DLIST_PROMOTE(Files, fsp);
			}
			return fsp;
		}
	}
	return NULL;
}

/****************************************************************************
 Reset the chained fsp - done at the start of a packet reply
****************************************************************************/

void file_chain_reset(void)
{
	chain_fsp = NULL;
}

/****************************************************************************
Save the chained fsp - done when about to do an oplock break.
****************************************************************************/

void file_chain_save(void)
{
	oplock_save_chain_fsp = chain_fsp;
}

/****************************************************************************
Restore the chained fsp - done after an oplock break.
****************************************************************************/
void file_chain_restore(void)
{
	chain_fsp = oplock_save_chain_fsp;
}

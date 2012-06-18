/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   file closing
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

/****************************************************************************
run a file if it is a magic script
****************************************************************************/
static void check_magic(files_struct *fsp,connection_struct *conn)
{
  if (!*lp_magicscript(SNUM(conn)))
    return;

  DEBUG(5,("checking magic for %s\n",fsp->fsp_name));

  {
    char *p;
    if (!(p = strrchr(fsp->fsp_name,'/')))
      p = fsp->fsp_name;
    else
      p++;

    if (!strequal(lp_magicscript(SNUM(conn)),p))
      return;
  }

  {
    int ret;
    pstring magic_output;
    pstring fname;
    pstrcpy(fname,fsp->fsp_name);

    if (*lp_magicoutput(SNUM(conn)))
      pstrcpy(magic_output,lp_magicoutput(SNUM(conn)));
    else
      slprintf(magic_output,sizeof(fname)-1, "%s.out",fname);

    chmod(fname,0755);
    ret = smbrun(fname,magic_output,False);
    DEBUG(3,("Invoking magic command %s gave %d\n",fname,ret));
    unlink(fname);
  }
}

/****************************************************************************
  Common code to close a file or a directory.
****************************************************************************/

void close_filestruct(files_struct *fsp)
{   
	connection_struct *conn = fsp->conn;
    
    flush_write_cache(fsp, CLOSE_FLUSH);

	fsp->open = False;
	fsp->is_directory = False; 
    
	conn->num_files_open--;
	if(fsp->wbmpx_ptr) {  
		free((char *)fsp->wbmpx_ptr);
		fsp->wbmpx_ptr = NULL; 
	}  
}    

/****************************************************************************
 Close a file - possibly invalidating the read prediction.

 If normal_close is 1 then this came from a normal SMBclose (or equivalent)
 operation otherwise it came as the result of some other operation such as
 the closing of the connection. In the latter case printing and
 magic scripts are not run.
****************************************************************************/

static int close_normal_file(files_struct *fsp, BOOL normal_close)
{
	SMB_DEV_T dev = fsp->fd_ptr->dev;
	SMB_INO_T inode = fsp->fd_ptr->inode;
	int token;
	BOOL last_reference = False;
	BOOL delete_on_close = fsp->fd_ptr->delete_on_close;
	connection_struct *conn = fsp->conn;
	int err = 0;

	remove_pending_lock_requests_by_fid(fsp);

	close_filestruct(fsp);

#if USE_READ_PREDICTION
	invalidate_read_prediction(fsp->fd_ptr->fd);
#endif

	if (lp_share_modes(SNUM(conn))) {
		lock_share_entry(conn, dev, inode, &token);
		del_share_mode(token, fsp);
		unlock_share_entry(conn, dev, inode, token);
	}

	if(EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))
		release_file_oplock(fsp);

	if(fd_attempt_close(fsp->fd_ptr,&err) == 0)
		last_reference = True;

    fsp->fd_ptr = NULL;
#ifdef PRINTING
	/* NT uses smbclose to start a print - weird */
	if (normal_close && fsp->print_file)
		print_file(conn, fsp);
#endif
	/* check for magic scripts */
	if (normal_close) {
		check_magic(fsp,conn);
	}

	/*
	 * NT can set delete_on_close of the last open
	 * reference to a file.
	 */

    if (normal_close && last_reference && delete_on_close) {
        DEBUG(5,("close_file: file %s. Delete on close was set - deleting file.\n",
	    fsp->fsp_name));
		if(dos_unlink(fsp->fsp_name) != 0) {

          /*
           * This call can potentially fail as another smbd may have
           * had the file open with delete on close set and deleted
           * it when its last reference to this file went away. Hence
           * we log this but not at debug level zero.
           */

          DEBUG(5,("close_file: file %s. Delete on close was set and unlink failed \
with error %s\n", fsp->fsp_name, strerror(errno) ));
        }
    }

	DEBUG(2,("%s closed file %s (numopen=%d) %s\n",
		 conn->user,fsp->fsp_name,
		 conn->num_files_open, err ? strerror(err) : ""));

	if (fsp->fsp_name) {
		string_free(&fsp->fsp_name);
	}

	file_free(fsp);

	return err;
}

/****************************************************************************
 Close a directory opened by an NT SMB call. 
****************************************************************************/
  
static int close_directory(files_struct *fsp, BOOL normal_close)
{
	remove_pending_change_notify_requests_by_fid(fsp);

	/*
	 * NT can set delete_on_close of the last open
	 * reference to a directory also.
	 */

	if (normal_close && fsp->directory_delete_on_close) {
		BOOL ok = rmdir_internals(fsp->conn, fsp->fsp_name);
		DEBUG(5,("close_directory: %s. Delete on close was set - deleting directory %s.\n",
			fsp->fsp_name, ok ? "succeeded" : "failed" ));

		/*
		 * Ensure we remove any change notify requests that would
		 * now fail as the directory has been deleted.
		 */

		if(ok)
			remove_pending_change_notify_requests_by_filename(fsp);
    }

	/*
	 * Do the code common to files and directories.
	 */
	close_filestruct(fsp);
	
	if (fsp->fsp_name)
		string_free(&fsp->fsp_name);
	
	file_free(fsp);

	return 0;
}

/****************************************************************************
 Close a file opened with null permissions in order to read permissions.
****************************************************************************/

static int close_statfile(files_struct *fsp, BOOL normal_close)
{
	close_filestruct(fsp);
	
	if (fsp->fsp_name)
		string_free(&fsp->fsp_name);
	
	file_free(fsp);

	return 0;
}

/****************************************************************************
 Close a directory opened by an NT SMB call. 
****************************************************************************/
  
int close_file(files_struct *fsp, BOOL normal_close)
{
	if(fsp->is_directory)
		return close_directory(fsp, normal_close);
	else if(fsp->stat_open)
		return close_statfile(fsp, normal_close);
	return close_normal_file(fsp, normal_close);
}

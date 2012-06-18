/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   dos mode handling functions
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
  change a dos mode to a unix mode
    base permission for files:
         if inheriting
           apply read/write bits from parent directory.
         else   
           everybody gets read bit set
         dos readonly is represented in unix by removing everyone's write bit
         dos archive is represented in unix by the user's execute bit
         dos system is represented in unix by the group's execute bit
         dos hidden is represented in unix by the other's execute bit
         if !inheriting {
           Then apply create mask,
           then add force bits.
         }
    base permission for directories:
         dos directory is represented in unix by unix's dir bit and the exec bit
         if !inheriting {
           Then apply create mask,
           then add force bits.
         }
****************************************************************************/
mode_t unix_mode(connection_struct *conn,int dosmode,const char *fname)
{
  mode_t result = (S_IRUSR | S_IRGRP | S_IROTH);
  mode_t dir_mode = 0; /* Mode of the parent directory if inheriting. */

  if ( !IS_DOS_READONLY(dosmode) )
    result |= (S_IWUSR | S_IWGRP | S_IWOTH);

  if (fname && lp_inherit_perms(SNUM(conn))) {
    char *dname;
    SMB_STRUCT_STAT sbuf;

    dname = parent_dirname(fname);
    DEBUG(2,("unix_mode(%s) inheriting from %s\n",fname,dname));
    if (dos_stat(dname,&sbuf) != 0) {
      DEBUG(4,("unix_mode(%s) failed, [dir %s]: %s\n",fname,dname,strerror(errno)));
      return(0);      /* *** shouldn't happen! *** */
    }

    /* Save for later - but explicitly remove setuid bit for safety. */
    dir_mode = sbuf.st_mode & ~S_ISUID;
    DEBUG(2,("unix_mode(%s) inherit mode %o\n",fname,(int)dir_mode));
    /* Clear "result" */
    result = 0;
  } 

  if (IS_DOS_DIR(dosmode)) {
    /* We never make directories read only for the owner as under DOS a user
       can always create a file in a read-only directory. */
    result |= (S_IFDIR | S_IWUSR);

    if (dir_mode) {
      /* Inherit mode of parent directory. */
      result |= dir_mode;
    } else {
      /* Provisionally add all 'x' bits */
      result |= (S_IXUSR | S_IXGRP | S_IXOTH);                 

      /* Apply directory mask */
      result &= lp_dir_mask(SNUM(conn));
      /* Add in force bits */
      result |= lp_force_dir_mode(SNUM(conn));
    }
  } else { 
    if (lp_map_archive(SNUM(conn)) && IS_DOS_ARCHIVE(dosmode))
      result |= S_IXUSR;

    if (lp_map_system(SNUM(conn)) && IS_DOS_SYSTEM(dosmode))
      result |= S_IXGRP;
 
    if (lp_map_hidden(SNUM(conn)) && IS_DOS_HIDDEN(dosmode))
      result |= S_IXOTH;  

    if (dir_mode) {
      /* Inherit 666 component of parent directory mode */
      result |= dir_mode
        &  (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
    } else {
      /* Apply mode mask */
      result &= lp_create_mask(SNUM(conn));
      /* Add in force bits */
      result |= lp_force_create_mode(SNUM(conn));
    }
  }
  return(result);
}


/****************************************************************************
  change a unix mode to a dos mode
****************************************************************************/
int dos_mode(connection_struct *conn,char *path,SMB_STRUCT_STAT *sbuf)
{
  int result = 0;

  DEBUG(8,("dos_mode: %s\n", path));

  if ((sbuf->st_mode & S_IWUSR) == 0)
      result |= aRONLY;

  if (MAP_ARCHIVE(conn) && ((sbuf->st_mode & S_IXUSR) != 0))
    result |= aARCH;

  if (MAP_SYSTEM(conn) && ((sbuf->st_mode & S_IXGRP) != 0))
    result |= aSYSTEM;

  if (MAP_HIDDEN(conn) && ((sbuf->st_mode & S_IXOTH) != 0))
    result |= aHIDDEN;   
  
  if (S_ISDIR(sbuf->st_mode))
    result = aDIR | (result & aRONLY);

#ifdef S_ISLNK
#if LINKS_READ_ONLY
  if (S_ISLNK(sbuf->st_mode) && S_ISDIR(sbuf->st_mode))
    result |= aRONLY;
#endif
#endif

  /* hide files with a name starting with a . */
  if (lp_hide_dot_files(SNUM(conn)))
    {
      char *p = strrchr(path,'/');
      if (p)
	p++;
      else
	p = path;
      
      if (p[0] == '.' && p[1] != '.' && p[1] != 0)
	result |= aHIDDEN;
    }

  /* Optimization : Only call is_hidden_path if it's not already
     hidden. */
  if (!(result & aHIDDEN) && IS_HIDDEN_PATH(conn,path))
  {
    result |= aHIDDEN;
  }

  DEBUG(8,("dos_mode returning "));

  if (result & aHIDDEN) DEBUG(8, ("h"));
  if (result & aRONLY ) DEBUG(8, ("r"));
  if (result & aSYSTEM) DEBUG(8, ("s"));
  if (result & aDIR   ) DEBUG(8, ("d"));
  if (result & aARCH  ) DEBUG(8, ("a"));

  DEBUG(8,("\n"));

  return(result);
}

/*******************************************************************
chmod a file - but preserve some bits
********************************************************************/
int file_chmod(connection_struct *conn,char *fname,int dosmode,SMB_STRUCT_STAT *st)
{
  SMB_STRUCT_STAT st1;
  int mask=0;
  mode_t tmp;
  mode_t unixmode;

  if (!st) {
    st = &st1;
    if (dos_stat(fname,st)) return(-1);
  }

  if (S_ISDIR(st->st_mode)) dosmode |= aDIR;

  if (dos_mode(conn,fname,st) == dosmode) return(0);

  unixmode = unix_mode(conn,dosmode,fname);

  /* preserve the s bits */
  mask |= (S_ISUID | S_ISGID);

  /* preserve the t bit */
#ifdef S_ISVTX
  mask |= S_ISVTX;
#endif

  /* possibly preserve the x bits */
  if (!MAP_ARCHIVE(conn)) mask |= S_IXUSR;
  if (!MAP_SYSTEM(conn)) mask |= S_IXGRP;
  if (!MAP_HIDDEN(conn)) mask |= S_IXOTH;

  unixmode |= (st->st_mode & mask);

  /* if we previously had any r bits set then leave them alone */
  if ((tmp = st->st_mode & (S_IRUSR|S_IRGRP|S_IROTH))) {
    unixmode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
    unixmode |= tmp;
  }

  /* if we previously had any w bits set then leave them alone 
   whilst adding in the new w bits, if the new mode is not rdonly */
  if (!IS_DOS_READONLY(dosmode)) {
    unixmode |= (st->st_mode & (S_IWUSR|S_IWGRP|S_IWOTH));
  }

  return(dos_chmod(fname,unixmode));
}


/*******************************************************************
Wrapper around dos_utime that possibly allows DOS semantics rather
than POSIX.
*******************************************************************/
int file_utime(connection_struct *conn, char *fname, struct utimbuf *times)
{
  extern struct current_user current_user;
  SMB_STRUCT_STAT sb;
  int ret = -1;

  errno = 0;

  if(dos_utime(fname, times) == 0)
    return 0;

  if((errno != EPERM) && (errno != EACCES))
    return -1;

  if(!lp_dos_filetimes(SNUM(conn)))
    return -1;

  /* We have permission (given by the Samba admin) to
     break POSIX semantics and allow a user to change
     the time on a file they don't own but can write to
     (as DOS does).
   */

  if(dos_stat(fname,&sb) != 0)
    return -1;

  /* Check if we have write access. */
  if (CAN_WRITE(conn)) {
	  if (((sb.st_mode & S_IWOTH) ||
	       conn->admin_user ||
	       ((sb.st_mode & S_IWUSR) && current_user.uid==sb.st_uid) ||
	       ((sb.st_mode & S_IWGRP) &&
		in_group(sb.st_gid,current_user.gid,
			 current_user.ngroups,current_user.groups)))) {
		  /* We are allowed to become root and change the filetime. */
		  become_root(False);
		  ret = dos_utime(fname, times);
		  unbecome_root(False);
	  }
  }

  return ret;
}
  
/*******************************************************************
Change a filetime - possibly allowing DOS semantics.
*******************************************************************/
BOOL set_filetime(connection_struct *conn, char *fname, time_t mtime)
{
  struct utimbuf times;

  if (null_mtime(mtime)) return(True);

  times.modtime = times.actime = mtime;

  if (file_utime(conn, fname, &times)) {
    DEBUG(4,("set_filetime(%s) failed: %s\n",fname,strerror(errno)));
    return False;
  }
  
  return(True);
} 

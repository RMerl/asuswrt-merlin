/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
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

/*
   netatalk.c : routines for improving interaction between Samba and netatalk.
   Copyright (C) John D. Blair <jdblair@cobaltnet.com> 1998
                 Cobalt Networks, Inc.
*/

#include "includes.h"

#ifdef WITH_NETATALK
 
extern int DEBUGLEVEL;

/*****************
   ntalk_resourcepath: creates the path to the netatalk resource fork for
                        a given file

   fname:       normal filename
   doublename:  buffer that will contain the location of the resource fork
   length:      length of this buffer (to prevent overflows)

   NOTE: doesn't currently gracefully deal with buffer overflows- it just
         doesn't allow them to occur.
******************/
void ntalk_resourcepath(const char *fname, char *doublename, const int length)
{
  int i;
  int charnum;
  int lastslash;
  char appledouble[] = APPLEDOUBLE;

  /* copy fname to doublename and find last slash */
  for (i = 0; (fname[i] != 0) && (i <= length); i++) {
    if (fname[i] == '/') {
      lastslash = i;
    }
    doublename[i] = fname[i];
  }
  lastslash++; /* location just after last slash */

  /* insert .AppleDouble */
  charnum = lastslash;
  for (i = 0; (appledouble[i] != 0) && (i <= length); i++) {
    doublename[charnum] = appledouble[i];
    charnum++;
  }

  /* append last part of file name */
  for (i = lastslash; (fname[i] != 0) && (i <= length); i++) {
    doublename[charnum] = fname[i];
    charnum++;
  }
  
  doublename[charnum] = 0;
}

/**********************
 ntalk_mkresdir: creates a new .AppleDouble directory (if necessary)
                 for the resource fork of a specified file
**********************/
int ntalk_mkresdir(const char *fname)
{
  char fdir[255];
  int i;
  int lastslash;
  SMB_STRUCT_STAT dirstats;
  char appledouble[] = APPLEDOUBLE;

  /* find directory containing fname */
  for (i = 0; (fname[i] != 0) && (i <= 254); i++) {
    fdir[i] = fname[i];
    if (fdir[i] == '/') {
      lastslash = i;
    }
  }
  lastslash++;
  fdir[lastslash] = 0;
  sys_lstat(fdir, &dirstats);

  /* append .AppleDouble */
  for (i = 0; (appledouble[i] != 0) && (lastslash <= 254); i++) {
    fdir[lastslash] = appledouble[i];
    lastslash++;
  }
  fdir[lastslash] = 0;

  /* create this directory */
  /* silently ignore EEXIST error */
  if ((mkdir(fdir, dirstats.st_mode) < 0) && (errno != EEXIST)) {
    return errno;
  }

  /* set ownership of this dir to the same as its parent */
  /* holy race condition, batman! */
  /* warning: this doesn't check for errors */
  chown(fdir, dirstats.st_uid, dirstats.st_gid);

  printf("%s\n", fdir);

  return 1;
}

/**********************
 ntalk_unlink: unlink a file and its resource fork
**********************/
int ntalk_unlink(const char *fname)
{
  char buf[255];

  ntalk_resourcepath(fname, buf, 255);
  unlink(buf);
  return unlink(fname);
}

/**********************
 ntalk_chown: chown a file and its resource fork
**********************/
int ntalk_chown(const char *fname, const uid_t uid, const gid_t gid)
{
  char buf[255];

  ntalk_resourcepath(fname, buf, 255);
  chown(buf, uid, gid);
  return chown(fname, uid, gid);
}

/**********************
 ntalk_chmod: chmod a file and its resource fork
**********************/
int ntalk_chmod(const char *fname, mode_t perms)
{
  char buf[255];

  ntalk_resourcepath(fname, buf, 255);
  chmod(buf, perms);
  return chmod(fname, perms);
}


#endif /* WITH_NETATALK */

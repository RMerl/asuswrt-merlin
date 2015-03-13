/* Declarations for FTP support.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software Foundation,
   Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef FTP_H
#define FTP_H

#include "host.h"

/* System types. */
enum stype
{
  ST_UNIX,
  ST_VMS,
  ST_WINNT,
  ST_MACOS,
  ST_OS400,
  ST_OTHER
};

/* Extensions of the ST_UNIX */
enum ustype
{
  UST_TYPE_L8,
  UST_MULTINET,
  UST_OTHER
};

extern char ftp_last_respline[];

uerr_t ftp_response (int, char **);
uerr_t ftp_login (int, const char *, const char *);
uerr_t ftp_port (int, int *);
uerr_t ftp_pasv (int, ip_address *, int *);
#ifdef ENABLE_IPV6
uerr_t ftp_lprt (int, int *);
uerr_t ftp_lpsv (int, ip_address *, int *);
uerr_t ftp_eprt (int, int *);
uerr_t ftp_epsv (int, ip_address *, int *);
#endif
uerr_t ftp_type (int, int);
uerr_t ftp_cwd (int, const char *);
uerr_t ftp_retr (int, const char *);
uerr_t ftp_rest (int, wgint);
uerr_t ftp_list (int, const char *, bool, bool, bool *);
uerr_t ftp_syst (int, enum stype *, enum ustype *);
uerr_t ftp_pwd (int, char **);
uerr_t ftp_size (int, const char *, wgint *);

#ifdef ENABLE_OPIE
const char *skey_response (int, const char *, const char *);
#endif

struct url;

/* File types.  */
enum ftype
{
  FT_PLAINFILE,
  FT_DIRECTORY,
  FT_SYMLINK,
  FT_UNKNOWN
};


/* Globbing (used by ftp_retrieve_glob).  */
enum
{
  GLOB_GLOBALL, GLOB_GETALL, GLOB_GETONE
};

/* Used by to test if time parsed includes hours and minutes. */
enum parsetype
{
  TT_HOUR_MIN, TT_DAY
};


/* Information about one filename in a linked list.  */
struct fileinfo
{
  enum ftype type;          /* file type */
  char *name;               /* file name */
  wgint size;               /* file size */
  long tstamp;              /* time-stamp */
  enum parsetype ptype;     /* time parsing */
  int perms;                /* file permissions */
  char *linkto;             /* link to which file points */
  struct fileinfo *prev;    /* previous... */
  struct fileinfo *next;    /* ...and next structure. */
};

/* Commands for FTP functions.  */
enum wget_ftp_command
{
  DO_LOGIN      = 0x0001,   /* Connect and login to the server.  */
  DO_CWD        = 0x0002,   /* Change current directory.  */
  DO_RETR       = 0x0004,   /* Retrieve the file.  */
  DO_LIST       = 0x0008,   /* Retrieve the directory list.  */
  LEAVE_PENDING = 0x0010    /* Do not close the socket.  */
};

enum wget_ftp_fstatus
{
  NOTHING       = 0x0000,   /* Nothing done yet.  */
  ON_YOUR_OWN   = 0x0001,   /* The ftp_loop_internal sets the
                               defaults.  */
  DONE_CWD      = 0x0002,   /* The current working directory is
                               correct.  */

  /* 2013-10-17 Andrea Urbani (matfanjol)
     For more information about the following entries, please,
     look at ftp.c, function getftp, text "__LIST_A_EXPLANATION__". */
  AVOID_LIST_A  = 0x0004,   /* It tells us if during this
                               session we have to avoid the use
                               of "LIST -a".*/
  AVOID_LIST    = 0x0008,   /* It tells us if during this
                               session we have to avoid to use
                               "LIST". */
  LIST_AFTER_LIST_A_CHECK_DONE  = 0x0010
                            /* It tells us if we have already
                               checked "LIST" after the first
                               "LIST -a" to handle the case of
                               file/folders named "-a". */
};

struct fileinfo *ftp_parse_ls (const char *, const enum stype);
uerr_t ftp_loop (struct url *, char **, int *, struct url *, bool, bool);

uerr_t ftp_index (const char *, struct url *, struct fileinfo *);

char ftp_process_type (const char *);


#endif /* FTP_H */

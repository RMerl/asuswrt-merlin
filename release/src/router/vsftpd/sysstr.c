/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * sysstr.c
 *
 * This file basically wraps system functions so that we can deal in our
 * nice abstracted string buffer objects.
 */

#include <stdio.h>
#include "sysstr.h"
#include "str.h"
#include "secbuf.h"
#include "sysutil.h"
#include "defs.h"
#include "utility.h"
#include "tunables.h"

void
str_getcwd(struct mystr* p_str)
{
  static char* p_getcwd_buf;
  char* p_ret;
  //if (p_getcwd_buf == 0)
  if(p_getcwd_buf == NULL)
  {
    vsf_secbuf_alloc(&p_getcwd_buf, VSFTP_PATH_MAX);
  }
  /* In case getcwd() fails */
  str_empty(p_str);
  p_ret = vsf_sysutil_getcwd(p_getcwd_buf, VSFTP_PATH_MAX);
  if (p_ret != 0)
  {
    str_alloc_text(p_str, p_getcwd_buf);
  }
}

int
str_write_loop(const struct mystr* p_str, const int fd)
{
  return vsf_sysutil_write_loop(fd, str_getbuf(p_str), str_getlen(p_str));
}

int
str_read_loop(struct mystr* p_str, const int fd)
{
  return vsf_sysutil_read_loop(
    fd, (char*) str_getbuf(p_str), str_getlen(p_str));
}

int
str_mkdir(const struct mystr* p_str, const unsigned int mode)
{
  return vsf_sysutil_mkdir(str_getbuf(p_str), mode);
}

int
str_rmdir(const struct mystr* p_str)
{
  return vsf_sysutil_rmdir(str_getbuf(p_str));
}

int
str_unlink(const struct mystr* p_str)
{
  return vsf_sysutil_unlink(str_getbuf(p_str));
}

int
str_chdir(const struct mystr* p_str)
{
  return vsf_sysutil_chdir(str_getbuf(p_str));
}

int
str_open(const struct mystr* p_str, const enum EVSFSysStrOpenMode mode)
{
  enum EVSFSysUtilOpenMode open_mode = kVSFSysStrOpenUnknown;
  switch (mode)
  {
    case kVSFSysStrOpenReadOnly:
      open_mode = kVSFSysUtilOpenReadOnly;
      break;
    default:
      bug("unknown mode value in str_open");
      break;
  }
  return vsf_sysutil_open_file(str_getbuf(p_str), open_mode);
}

int
str_stat(const struct mystr* p_str, struct vsf_sysutil_statbuf** p_ptr)
{
  return vsf_sysutil_stat(str_getbuf(p_str), p_ptr);
}

int
str_lstat(const struct mystr* p_str, struct vsf_sysutil_statbuf** p_ptr)
{
  return vsf_sysutil_lstat(str_getbuf(p_str), p_ptr);
}

int
str_create(const struct mystr* p_str)
{
  return vsf_sysutil_create_file(str_getbuf(p_str));
}

int
str_create_overwrite(const struct mystr* p_str)
{
  return vsf_sysutil_create_overwrite_file(str_getbuf(p_str));
}

int
str_create_append(const struct mystr* p_str)
{
  return vsf_sysutil_create_or_open_file(
      str_getbuf(p_str), tunable_file_open_mode);
}

int
str_chmod(const struct mystr* p_str, unsigned int mode)
{
  return vsf_sysutil_chmod(str_getbuf(p_str), mode);
}

int
str_rename(const struct mystr* p_from_str, const struct mystr* p_to_str)
{
  return vsf_sysutil_rename(str_getbuf(p_from_str), str_getbuf(p_to_str));
}

struct vsf_sysutil_dir*
str_opendir(const struct mystr* p_str)
{
  return vsf_sysutil_opendir(str_getbuf(p_str));
}

// James
void str_next_dirent(const char *session_user, const char *base_dir, struct mystr *p_filename_str, struct vsf_sysutil_dir *p_dir)
{
  const char* p_filename = vsf_sysutil_next_dirent(session_user, base_dir, p_dir);
  str_empty(p_filename_str);
  if (p_filename != 0)
  {
    str_alloc_text(p_filename_str, p_filename);
  }
}

int
str_readlink(struct mystr* p_str, const struct mystr* p_filename_str)
{
  static char* p_readlink_buf;
  int retval;
  if (p_readlink_buf == 0)
  {
    vsf_secbuf_alloc(&p_readlink_buf, VSFTP_PATH_MAX);
  }
  /* In case readlink() fails */
  str_empty(p_str);
  /* Note: readlink(2) does not NULL terminate, but our wrapper does */
  retval = vsf_sysutil_readlink(str_getbuf(p_filename_str), p_readlink_buf,
                                VSFTP_PATH_MAX);
  if (vsf_sysutil_retval_is_error(retval))
  {
    return retval;
  }
  str_alloc_text(p_str, p_readlink_buf);
  return 0;
}

struct vsf_sysutil_user*
str_getpwnam(const struct mystr* p_user_str)
{
  return vsf_sysutil_getpwnam(str_getbuf(p_user_str));
}

void
str_syslog(const struct mystr* p_str, int severe)
{
  vsf_sysutil_syslog(str_getbuf(p_str), severe);
}


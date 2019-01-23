/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * filestr.c
 *
 * This file contains extensions to the string/buffer API, to load a file
 * into a buffer. 
 */

#include "filestr.h"
/* Get access to "private" functions */
#define VSFTP_STRING_HELPER
#include "str.h"
#include "sysutil.h"
#include "secbuf.h"
#include "utility.h"

int
str_fileread(struct mystr* p_str, const char* p_filename, unsigned int maxsize)
{
  int fd;
  int retval = 0;
  filesize_t size;
  char* p_sec_buf = 0;
  struct vsf_sysutil_statbuf* p_stat = 0;
  /* In case we fail, make sure we return an empty string */
  str_empty(p_str);
  fd = vsf_sysutil_open_file(p_filename, kVSFSysUtilOpenReadOnly);
  if (vsf_sysutil_retval_is_error(fd))
  {
    return fd;
  }
  vsf_sysutil_fstat(fd, &p_stat);
  if (vsf_sysutil_statbuf_is_regfile(p_stat))
  {
    size = vsf_sysutil_statbuf_get_size(p_stat);
    if (size > maxsize)
    {
      size = maxsize;
    }
    vsf_secbuf_alloc(&p_sec_buf, (unsigned int) size);

    retval = vsf_sysutil_read_loop(fd, p_sec_buf, (unsigned int) size);
    if (vsf_sysutil_retval_is_error(retval))
    {
      goto free_out;
    }
    else if ((unsigned int) retval != size)
    {
      die("read size mismatch");
    }
    str_alloc_memchunk(p_str, p_sec_buf, (unsigned int) size);
  }
free_out:
  vsf_sysutil_free(p_stat);
  vsf_secbuf_free(&p_sec_buf);
  vsf_sysutil_close(fd);
  return retval;
}


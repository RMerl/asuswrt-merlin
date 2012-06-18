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
 * netstr.c
 *
 * The netstr interface extends the standard string interface, adding
 * functions which can cope safely with building strings from the network,
 * and send them out too.
 */

#include "netstr.h"
#include "str.h"
#include "sysstr.h"
#include "utility.h"
#include "sysutil.h"

void
str_netfd_alloc(struct mystr* p_str, int fd, char term, char* p_readbuf,
                unsigned int maxlen)
{
  int retval;
  unsigned int bytes_read;
  unsigned int i;
  char* p_readpos = p_readbuf;
  unsigned int left = maxlen;
  while (1)
  {
    if (p_readpos + left != p_readbuf + maxlen)
    {
      bug("poor buffer accounting in str_netfd_alloc");
    }
    /* Did we hit the max? */
    if (left == 0)
    {
      str_empty(p_str);
      return;
    }
    retval = vsf_sysutil_recv_peek(fd, p_readpos, left);
    if (vsf_sysutil_retval_is_error(retval))
    {
      die("vsf_sysutil_recv_peek");
    }
    else if (retval == 0)
    {
      die("vsf_sysutil_recv_peek: no data");
    }
    bytes_read = (unsigned int) retval;
    /* Search for the terminator */
    for (i=0; i < bytes_read; i++)
    {
      if (p_readpos[i] == term)
      {
        /* Got it! */
        retval = vsf_sysutil_read_loop(fd, p_readpos, i + 1);
        if (vsf_sysutil_retval_is_error(retval) ||
            (unsigned int) retval != i + 1)
        {
          die("vsf_sysutil_read_loop");
        }
        if (p_readpos[i] != term)
        {
          die("missing terminator in str_netfd_alloc");
        }
        str_alloc_alt_term(p_str, p_readbuf, term);
        return;
      }
    }
    /* Not found in this read chunk, so consume the data and re-loop */
    if (bytes_read > left)
    {
      bug("bytes_read > left in str_netfd_alloc");
    }
    left -= bytes_read;
    retval = vsf_sysutil_read_loop(fd, p_readpos, bytes_read);
    if (vsf_sysutil_retval_is_error(retval) ||
        (unsigned int) retval != bytes_read)
    {
      die("vsf_sysutil_read_loop");
    }
    p_readpos += bytes_read;
  } /* END: while(1) */
}

int
str_netfd_write(const struct mystr* p_str, int fd)
{
  int ret = 0;
  int retval;
  unsigned int str_len = str_getlen(p_str);
  if (str_len == 0)
  {
    bug("zero str_len in str_netfd_write");
  }
  retval = str_write_loop(p_str, fd);
  if (vsf_sysutil_retval_is_error(retval) || (unsigned int) retval != str_len)
  {
    ret = -1;
  }
  return ret;
}

int
str_netfd_read(struct mystr* p_str, int fd, unsigned int len)
{
  int retval;
  str_reserve(p_str, len);
  str_trunc(p_str, len);
  retval = str_read_loop(p_str, fd);
  if (vsf_sysutil_retval_is_error(retval) || (unsigned int) retval != len)
  {
    return -1;
  }
  return retval;
}


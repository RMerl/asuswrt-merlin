/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * ascii.c
 *
 * Routines to handle ASCII mode tranfers. Yuk.
 */

#include "ascii.h"

struct ascii_to_bin_ret
vsf_ascii_ascii_to_bin(char* p_buf, unsigned int in_len, int prev_cr)
{
  /* Task: translate all \r\n into plain \n. A plain \r not followed by \n must
   * not be removed.
   */
  struct ascii_to_bin_ret ret;
  unsigned int indexx = 0;
  unsigned int written = 0;
  char* p_out = p_buf + 1;
  ret.last_was_cr = 0;
  if (prev_cr && (!in_len || p_out[0] != '\n'))
  {
    p_buf[0] = '\r';
    ret.p_buf = p_buf;
    written++;
  }
  else
  {
    ret.p_buf = p_out;
  }
  while (indexx < in_len)
  {
    char the_char = p_buf[indexx + 1];
    if (the_char != '\r')
    {
      *p_out++ = the_char;
      written++;
    }
    else if (indexx == in_len - 1)
    {
      ret.last_was_cr = 1;
    }
    else if (p_buf[indexx + 2] != '\n')
    {
      *p_out++ = the_char;
      written++;
    }
    indexx++;
  }
  ret.stored = written;
  return ret;
}

struct bin_to_ascii_ret
vsf_ascii_bin_to_ascii(const char* p_in,
                       char* p_out,
                       unsigned int in_len,
                       int prev_cr)
{
  /* Task: translate all \n not preceeded by \r into \r\n.
   * Note that \r\n stays as \r\n. We used to map it to \r\r\n like wu-ftpd
   * but have switched to leaving it, like the more popular proftpd.
   */
  struct bin_to_ascii_ret ret = { 0, 0 };
  unsigned int indexx = 0;
  unsigned int written = 0;
  char last_char = 0;
  if (prev_cr)
  {
    last_char = '\r';
    ret.last_was_cr = 1;
  }
  while (indexx < in_len)
  {
    char the_char = p_in[indexx];
    if (the_char == '\n' && last_char != '\r')
    {
      *p_out++ = '\r';
      written++;
    }
    *p_out++ = the_char;
    written++;
    indexx++;
    last_char = the_char;
    if (the_char == '\r')
    {
      ret.last_was_cr = 1;
    }
    else
    {
      ret.last_was_cr = 0;
    }
  }
  ret.stored = written;
  return ret;
}


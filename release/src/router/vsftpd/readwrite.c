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
 * readwrite.c
 *
 * Routines to encapsulate the underlying read / write mechanism (OpenSSL vs.
 * plain read()/write()).
 */

#include "readwrite.h"
#include "session.h"
#include "netstr.h"
#include "ssl.h"
#include "privsock.h"
#include "defs.h"
#include "sysutil.h"

int
ftp_write_str(const struct vsf_session* p_sess, const struct mystr* p_str,
              enum EVSFRWTarget target)
{
  if (target == kVSFRWData)
  {
    if (p_sess->data_use_ssl)
    {
      return ssl_write_str(p_sess->p_data_ssl, p_str);
    }
    else
    {
      return str_netfd_write(p_str, p_sess->data_fd);
    }
  }
  else
  {
    if (p_sess->control_use_ssl && p_sess->ssl_slave_active)
    {
      priv_sock_send_cmd(p_sess->ssl_consumer_fd, PRIV_SOCK_WRITE_USER_RESP);
      priv_sock_send_str(p_sess->ssl_consumer_fd, p_str);
      return priv_sock_get_int(p_sess->ssl_consumer_fd);
    }
    else if (p_sess->control_use_ssl)
    {
      return ssl_write_str(p_sess->p_control_ssl, p_str);
    }
    else
    {
      return str_netfd_write(p_str, VSFTP_COMMAND_FD);
    }
  }
}

int
ftp_read_data(const struct vsf_session* p_sess, char* p_buf, unsigned int len)
{
  if (p_sess->data_use_ssl)
  {
    return ssl_read(p_sess->p_data_ssl, p_buf, len);
  }
  else
  {
    return vsf_sysutil_read(p_sess->data_fd, p_buf, len);
  }
}

int
ftp_write_data(const struct vsf_session* p_sess, const char* p_buf,
               unsigned int len)
{
  if (p_sess->data_use_ssl)
  {
    return ssl_write(p_sess->p_data_ssl, p_buf, len);
  }
  else
  {
    return vsf_sysutil_write_loop(p_sess->data_fd, p_buf, len);
  }
}

void
ftp_getline(const struct vsf_session* p_sess, struct mystr* p_str, char* p_buf)
{
  if (p_sess->control_use_ssl && p_sess->ssl_slave_active)
  {
    priv_sock_send_cmd(p_sess->ssl_consumer_fd, PRIV_SOCK_GET_USER_CMD);
    priv_sock_get_str(p_sess->ssl_consumer_fd, p_str);
  }
  else if (p_sess->control_use_ssl)
  {
    ssl_getline(p_sess, p_str, '\n', p_buf, VSFTP_MAX_COMMAND_LINE);
  }
  else
  {
    str_netfd_alloc(
      p_str, VSFTP_COMMAND_FD, '\n', p_buf, VSFTP_MAX_COMMAND_LINE);
  }
}


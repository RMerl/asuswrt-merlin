/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * sslslave.c
 */

#include "sslslave.h"
#include "session.h"
#include "privsock.h"
#include "tunables.h"
#include "sysutil.h"
#include "sysdeputil.h"
#include "utility.h"
#include "ssl.h"
#include "readwrite.h"
#include "defs.h"

void
ssl_slave(struct vsf_session* p_sess)
{
  struct mystr data_str = INIT_MYSTR;
  str_reserve(&data_str, VSFTP_DATA_BUFSIZE);
  /* Before becoming the slave, clear the alarm for the FTP protocol. */
  vsf_sysutil_clear_alarm();
  /* No need for any further communications with the privileged parent. */
  priv_sock_set_parent_context(p_sess);
  if (tunable_setproctitle_enable)
  {
    vsf_sysutil_setproctitle("SSL handler");
  }
  while (1)
  {
    char cmd = priv_sock_get_cmd(p_sess->ssl_slave_fd);
    int ret;
    if (cmd == PRIV_SOCK_GET_USER_CMD)
    {
      ret = ftp_getline(p_sess, &p_sess->ftp_cmd_str,
                        p_sess->p_control_line_buf);
      priv_sock_send_int(p_sess->ssl_slave_fd, ret);
      if (ret >= 0)
      {
        priv_sock_send_str(p_sess->ssl_slave_fd, &p_sess->ftp_cmd_str);
      }
    }
    else if (cmd == PRIV_SOCK_WRITE_USER_RESP)
    {
      priv_sock_get_str(p_sess->ssl_slave_fd, &p_sess->ftp_cmd_str);
      ret = ftp_write_str(p_sess, &p_sess->ftp_cmd_str, kVSFRWControl);
      priv_sock_send_int(p_sess->ssl_slave_fd, ret);
    }
    else if (cmd == PRIV_SOCK_DO_SSL_HANDSHAKE)
    {
      char result = PRIV_SOCK_RESULT_BAD;
      if (p_sess->data_fd != -1 || p_sess->p_data_ssl != 0)
      {
        bug("state not clean");
      }
      p_sess->data_fd = priv_sock_recv_fd(p_sess->ssl_slave_fd);
      ret = ssl_accept(p_sess, p_sess->data_fd);
      if (ret == 1)
      {
        result = PRIV_SOCK_RESULT_OK;
      }
      else
      {
        vsf_sysutil_close(p_sess->data_fd);
        p_sess->data_fd = -1;
      }
      priv_sock_send_result(p_sess->ssl_slave_fd, result);
    }
    else if (cmd == PRIV_SOCK_DO_SSL_READ)
    {
      int size = priv_sock_get_int(p_sess->ssl_slave_fd);
      if (size <= 0 || size > VSFTP_DATA_BUFSIZE)
      {
        bug("bad size");
      }
      if (p_sess->data_fd == -1 || p_sess->p_data_ssl == 0)
      {
        bug("invalid state");
      }
      str_trunc(&data_str, (unsigned int) size);
      ret = ssl_read_into_str(p_sess, p_sess->p_data_ssl, &data_str);
      priv_sock_send_int(p_sess->ssl_slave_fd, ret);
      priv_sock_send_str(p_sess->ssl_slave_fd, &data_str);
    }
    else if (cmd == PRIV_SOCK_DO_SSL_WRITE)
    {
      if (p_sess->data_fd == -1 || p_sess->p_data_ssl == 0)
      {
        bug("invalid state");
      }
      priv_sock_get_str(p_sess->ssl_slave_fd, &data_str);
      ret = ssl_write(p_sess->p_data_ssl,
                      str_getbuf(&data_str),
                      str_getlen(&data_str));
      priv_sock_send_int(p_sess->ssl_slave_fd, ret);
    }
    else if (cmd == PRIV_SOCK_DO_SSL_CLOSE)
    {
      char result = PRIV_SOCK_RESULT_BAD;
      if (p_sess->data_fd == -1 && p_sess->p_data_ssl == 0)
      {
        result = PRIV_SOCK_RESULT_OK;
      }
      else
      {
        if (p_sess->data_fd == -1 || p_sess->p_data_ssl == 0)
        {
          bug("invalid state");
        }
        ret = ssl_data_close(p_sess);
        if (ret == 1)
        {
          result = PRIV_SOCK_RESULT_OK;
        }
        vsf_sysutil_close(p_sess->data_fd);
        p_sess->data_fd = -1;
      }
      priv_sock_send_result(p_sess->ssl_slave_fd, result);
    }
    else
    {
      die("bad request in process_ssl_slave_req");
    }
  }
}

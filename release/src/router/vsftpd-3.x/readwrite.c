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

static int plain_peek_adapter(struct vsf_session* p_sess,
                              char* p_buf,
                              unsigned int len);
static int plain_read_adapter(struct vsf_session* p_sess,
                              char* p_buf,
                              unsigned int len);
static int ssl_peek_adapter(struct vsf_session* p_sess,
                            char* p_buf,
                            unsigned int len);
static int ssl_read_adapter(struct vsf_session* p_sess,
                            char* p_buf,
                            unsigned int len);

int
ftp_write_str(const struct vsf_session* p_sess, const struct mystr* p_str,
              enum EVSFRWTarget target)
{
  if (target == kVSFRWData)
  {
    if (p_sess->data_use_ssl && p_sess->ssl_slave_active)
    {
      int ret = -1;
      int written;
      priv_sock_send_cmd(p_sess->ssl_consumer_fd, PRIV_SOCK_DO_SSL_WRITE);
      priv_sock_send_str(p_sess->ssl_consumer_fd, p_str);
      written = priv_sock_get_int(p_sess->ssl_consumer_fd);
      if (written > 0 && written == (int) str_getlen(p_str))
      {
        ret = 0;
      }
      return ret;
    }
    else if (p_sess->data_use_ssl)
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
ftp_read_data(struct vsf_session* p_sess, char* p_buf, unsigned int len)
{
  if (p_sess->data_use_ssl && p_sess->ssl_slave_active)
  {
    int ret;
    priv_sock_send_cmd(p_sess->ssl_consumer_fd, PRIV_SOCK_DO_SSL_READ);
    priv_sock_send_int(p_sess->ssl_consumer_fd, len);
    ret = priv_sock_get_int(p_sess->ssl_consumer_fd);
    priv_sock_recv_buf(p_sess->ssl_consumer_fd, p_buf, len);
    /* Need to do this here too because it is useless in the slave process. */
    vsf_sysutil_check_pending_actions(kVSFSysUtilIO, ret, p_sess->data_fd);
    return ret;
  }
  else if (p_sess->data_use_ssl)
  {
    return ssl_read(p_sess, p_sess->p_data_ssl, p_buf, len);
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
  if (p_sess->data_use_ssl && p_sess->ssl_slave_active)
  {
    int ret;
    priv_sock_send_cmd(p_sess->ssl_consumer_fd, PRIV_SOCK_DO_SSL_WRITE);
    priv_sock_send_buf(p_sess->ssl_consumer_fd, p_buf, len);
    ret = priv_sock_get_int(p_sess->ssl_consumer_fd);
    /* Need to do this here too because it is useless in the slave process. */
    vsf_sysutil_check_pending_actions(kVSFSysUtilIO, ret, p_sess->data_fd);
    return ret;
  }
  else if (p_sess->data_use_ssl)
  {
    return ssl_write(p_sess->p_data_ssl, p_buf, len);
  }
  else
  {
    return vsf_sysutil_write_loop(p_sess->data_fd, p_buf, len);
  }
}

int
ftp_getline(struct vsf_session* p_sess, struct mystr* p_str, char* p_buf)
{
  if (p_sess->control_use_ssl && p_sess->ssl_slave_active)
  {
    int ret;
    priv_sock_send_cmd(p_sess->ssl_consumer_fd, PRIV_SOCK_GET_USER_CMD);
    ret = priv_sock_get_int(p_sess->ssl_consumer_fd);
    if (ret >= 0)
    {
      priv_sock_get_str(p_sess->ssl_consumer_fd, p_str);
    }
    return ret;
  }
  else
  {
    str_netfd_read_t p_peek = plain_peek_adapter;
    str_netfd_read_t p_read = plain_read_adapter;
    if (p_sess->control_use_ssl)
    {
      p_peek = ssl_peek_adapter;
      p_read = ssl_read_adapter;
    }
    return str_netfd_alloc(p_sess,
                           p_str,
                           '\n',
                           p_buf,
                           VSFTP_MAX_COMMAND_LINE,
                           p_peek,
                           p_read);
  }
}

static int
plain_peek_adapter(struct vsf_session* p_sess, char* p_buf, unsigned int len)
{
  (void) p_sess;
  return vsf_sysutil_recv_peek(VSFTP_COMMAND_FD, p_buf, len);
}

static int
plain_read_adapter(struct vsf_session* p_sess, char* p_buf, unsigned int len)
{
  (void) p_sess;
  return vsf_sysutil_read_loop(VSFTP_COMMAND_FD, p_buf, len);
}

static int
ssl_peek_adapter(struct vsf_session* p_sess, char* p_buf, unsigned int len)
{
  return ssl_peek(p_sess, p_sess->p_control_ssl, p_buf, len);
}

static int
ssl_read_adapter(struct vsf_session* p_sess, char* p_buf, unsigned int len)
{
  return ssl_read(p_sess, p_sess->p_control_ssl, p_buf, len);
}

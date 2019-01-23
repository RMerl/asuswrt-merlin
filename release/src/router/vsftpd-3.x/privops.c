/*
 * Part of Very Secure FTPd
 * License: GPL v2
 * Author: Chris Evans
 * privops.c
 *
 * Code implementing the privileged operations that the unprivileged client
 * might request.
 * Look for suitable paranoia in this file.
 */

#include "privops.h"
#include "session.h"
#include "sysdeputil.h"
#include "sysutil.h"
#include "utility.h"
#include "str.h"
#include "tunables.h"
#include "defs.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <bcmnvram.h>

/* File private functions */
static enum EVSFPrivopLoginResult handle_anonymous_login(
  struct vsf_session* p_sess, const struct mystr* p_pass_str);
static enum EVSFPrivopLoginResult handle_local_login(
  struct vsf_session* p_sess, struct mystr* p_user_str,
  const struct mystr* p_pass_str);
static void setup_username_globals(struct vsf_session* p_sess,
                                   const struct mystr* p_str);
static enum EVSFPrivopLoginResult handle_login(
  struct vsf_session* p_sess, struct mystr* p_user_str,
  const struct mystr* p_pass_str);

int
vsf_privop_get_ftp_port_sock(struct vsf_session* p_sess,
                             unsigned short remote_port,
                             int use_port_sockaddr)
{
  static struct vsf_sysutil_sockaddr* p_sockaddr;
  const struct vsf_sysutil_sockaddr* p_connect_to;
  int retval;
  int i;
  int s = vsf_sysutil_get_ipsock(p_sess->p_local_addr);
  unsigned short port = 0;
  if (p_sess->pasv_listen_fd != -1)
  {
    die("listed fd is active?");
  }
  if (vsf_sysutil_is_port_reserved(remote_port))
  {
    die("Illegal port request");
  }
  if (tunable_connect_from_port_20)
  {
    port = (unsigned short) tunable_ftp_data_port;
  }
  vsf_sysutil_activate_reuseaddr(s);
  /* A report of failure here on Solaris, presumably buggy address reuse
   * support? We'll retry.
   */
  for (i = 0; i < 2; ++i)
  {
    double sleep_for;
    vsf_sysutil_sockaddr_clone(&p_sockaddr, p_sess->p_local_addr);
    vsf_sysutil_sockaddr_set_port(p_sockaddr, port);
    retval = vsf_sysutil_bind(s, p_sockaddr);
    if (retval == 0)
    {
      break;
    }
    if (vsf_sysutil_get_error() != kVSFSysUtilErrADDRINUSE || i == 1)
    {
      die("vsf_sysutil_bind");
    }
    sleep_for = vsf_sysutil_get_random_byte();
    sleep_for /= 256.0;
    sleep_for += 1.0;
    vsf_sysutil_sleep(sleep_for);
  }
  if (use_port_sockaddr)
  {
    p_connect_to = p_sess->p_port_sockaddr;
  }
  else
  {
    vsf_sysutil_sockaddr_set_port(p_sess->p_remote_addr, remote_port);
    p_connect_to = p_sess->p_remote_addr;
  }
  retval = vsf_sysutil_connect_timeout(s, p_connect_to,
                                       tunable_connect_timeout);
  if (vsf_sysutil_retval_is_error(retval))
  {
    vsf_sysutil_close(s);
    s = -1;
  }
  return s;
}

void
vsf_privop_pasv_cleanup(struct vsf_session* p_sess)
{
  if (p_sess->pasv_listen_fd != -1)
  {
    vsf_sysutil_close(p_sess->pasv_listen_fd);
    p_sess->pasv_listen_fd = -1;
  }
}

int
vsf_privop_pasv_active(struct vsf_session* p_sess)
{
  if (p_sess->pasv_listen_fd != -1)
  {
    return 1;
  }
  return 0;
}

unsigned short
vsf_privop_pasv_listen(struct vsf_session* p_sess)
{
  static struct vsf_sysutil_sockaddr* s_p_sockaddr;
  int bind_retries = 10;
  unsigned short the_port;
  /* IPPORT_RESERVED */
  unsigned short min_port = 1024;
  unsigned short max_port = 65535;
  int is_ipv6 = vsf_sysutil_sockaddr_is_ipv6(p_sess->p_local_addr);
  if (p_sess->pasv_listen_fd != -1)
  {
    die("listed fd already active");
  }

  if (tunable_pasv_min_port > min_port && tunable_pasv_min_port <= max_port)
  {
    min_port = (unsigned short) tunable_pasv_min_port;
  }
  if (tunable_pasv_max_port >= min_port && tunable_pasv_max_port < max_port)
  {
    max_port = (unsigned short) tunable_pasv_max_port;
  }

  while (--bind_retries)
  {
    int retval;
    double scaled_port;
    the_port = vsf_sysutil_get_random_byte();
    the_port = (unsigned short) (the_port << 8);
    the_port = (unsigned short) (the_port | vsf_sysutil_get_random_byte());
    scaled_port = (double) min_port;
    scaled_port += ((double) the_port / (double) 65536) *
                   ((double) max_port - min_port + 1);
    the_port = (unsigned short) scaled_port;
    if (is_ipv6)
    {
      p_sess->pasv_listen_fd = vsf_sysutil_get_ipv6_sock();
    }
    else
    {
      p_sess->pasv_listen_fd = vsf_sysutil_get_ipv4_sock();
    }
    vsf_sysutil_activate_reuseaddr(p_sess->pasv_listen_fd);
    vsf_sysutil_sockaddr_clone(&s_p_sockaddr, p_sess->p_local_addr);
    vsf_sysutil_sockaddr_set_port(s_p_sockaddr, the_port);
    retval = vsf_sysutil_bind(p_sess->pasv_listen_fd, s_p_sockaddr);
    if (!vsf_sysutil_retval_is_error(retval))
    {
      retval = vsf_sysutil_listen(p_sess->pasv_listen_fd, 1);
      if (!vsf_sysutil_retval_is_error(retval))
      {
        break;
      }
    }
    /* SELinux systems can give you an inopportune EACCES, it seems. */
    if (vsf_sysutil_get_error() == kVSFSysUtilErrADDRINUSE ||
        vsf_sysutil_get_error() == kVSFSysUtilErrACCES)
    {
      vsf_sysutil_close(p_sess->pasv_listen_fd);
      p_sess->pasv_listen_fd = -1;
      continue;
    }
    die("vsf_sysutil_bind / listen");
  }
  if (!bind_retries)
  {
    die("vsf_sysutil_bind");
  }
  return the_port;
}

int
vsf_privop_accept_pasv(struct vsf_session* p_sess)
{
  struct vsf_sysutil_sockaddr* p_accept_addr = 0;
  int remote_fd;
  if (p_sess->pasv_listen_fd == -1)
  {
    die("listed fd not active");
  }
  vsf_sysutil_sockaddr_alloc(&p_accept_addr);
  remote_fd = vsf_sysutil_accept_timeout(p_sess->pasv_listen_fd, p_accept_addr,
                                         tunable_accept_timeout);
  if (vsf_sysutil_retval_is_error(remote_fd))
  {
    vsf_sysutil_sockaddr_clear(&p_accept_addr);
    return -1;
  }
  /* SECURITY:
   * Reject the connection if it wasn't from the same IP as the
   * control connection.
   */
  if (!tunable_pasv_promiscuous)
  {
    if (!vsf_sysutil_sockaddr_addr_equal(p_sess->p_remote_addr, p_accept_addr))
    {
      vsf_sysutil_close(remote_fd);
      vsf_sysutil_sockaddr_clear(&p_accept_addr);
      return -2;
    }
  }
  vsf_sysutil_sockaddr_clear(&p_accept_addr);
  return remote_fd;
}

void
vsf_privop_do_file_chown(struct vsf_session* p_sess, int fd)
{
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  vsf_sysutil_fstat(fd, &s_p_statbuf);
  /* Do nothing if it is already owned by the desired user. */
  if (vsf_sysutil_statbuf_get_uid(s_p_statbuf) ==
      p_sess->anon_upload_chown_uid)
  {
    return;
  }
  /* Drop it like a hot potato unless it's a regular file owned by
   * the the anonymous ftp user
   */
  if (p_sess->anon_upload_chown_uid == -1 ||
      !vsf_sysutil_statbuf_is_regfile(s_p_statbuf) ||
      (vsf_sysutil_statbuf_get_uid(s_p_statbuf) != p_sess->anon_ftp_uid &&
       vsf_sysutil_statbuf_get_uid(s_p_statbuf) != p_sess->guest_user_uid))
  {
    die("invalid fd in cmd_process_chown");
  }
  /* SECURITY! You need an OS which strips SUID/SGID bits on chown(),
   * otherwise a compromise of the FTP user will lead to compromise of
   * the "anon_upload_chown_uid" user (think chmod +s).
   */
  vsf_sysutil_fchown(fd, p_sess->anon_upload_chown_uid, -1);
}

enum EVSFPrivopLoginResult
vsf_privop_do_login(struct vsf_session* p_sess,
                    const struct mystr* p_pass_str)
{
  enum EVSFPrivopLoginResult result =
    handle_login(p_sess, &p_sess->user_str, p_pass_str);
  vsf_log_start_entry(p_sess, kVSFLogEntryLogin);
  if (result == kVSFLoginFail)
  {
    vsf_log_do_log(p_sess, 0);
    if (tunable_delay_failed_login)
    {
      vsf_sysutil_sleep((double) tunable_delay_failed_login);
    }
  }
  else
  {
    vsf_log_do_log(p_sess, 1);
    if (tunable_delay_successful_login)
    {
      vsf_sysutil_sleep((double) tunable_delay_successful_login);
    }
  }
  return result;
}

static enum EVSFPrivopLoginResult
handle_login(struct vsf_session* p_sess, struct mystr* p_user_str,
             const struct mystr* p_pass_str)
{
  /* Do not assume PAM can cope with dodgy input, even though it
   * almost certainly can.
   */
  int anonymous_login = 0;
  char first_char;
  unsigned int len = str_getlen(p_user_str);
  if (len == 0 || len > VSFTP_USERNAME_MAX)
  {
    return kVSFLoginFail;
  }
  /* Throw out dodgy start characters */
  first_char = str_get_char_at(p_user_str, 0);
  if (!vsf_sysutil_isalnum(first_char) &&
      first_char != '_' &&
      first_char != '.')
  {
    return kVSFLoginFail;
  }
  /* Throw out non-printable characters and space in username */
  if (str_contains_space(p_user_str) ||
      str_contains_unprintable(p_user_str))
  {
    return kVSFLoginFail;
  }
  /* Throw out excessive length passwords */
  len = str_getlen(p_pass_str);
  if (len > VSFTP_PASSWORD_MAX)
  {
    return kVSFLoginFail;
  }
  /* Check for an anonymous login or "real" login */
  if (tunable_anonymous_enable)
  {
    struct mystr upper_str = INIT_MYSTR;
    str_copy(&upper_str, p_user_str);
    str_upper(&upper_str);
    if (str_equal_text(&upper_str, "ANONYMOUS"))
    {
      anonymous_login = 1;
    }
    str_free(&upper_str);
  }
  {
    enum EVSFPrivopLoginResult result = kVSFLoginFail;
    if (anonymous_login)
    {
      result = handle_anonymous_login(p_sess, p_pass_str);
    }
    else
    {
      if (!tunable_local_enable)
      {
        die("unexpected local login in handle_login");
      }
      result = handle_local_login(p_sess, p_user_str, p_pass_str);
    }
    return result;
  }
}

static enum EVSFPrivopLoginResult
handle_anonymous_login(struct vsf_session* p_sess,
                       const struct mystr* p_pass_str)
{
  if (!str_isempty(&p_sess->banned_email_str) &&
      str_contains_line(&p_sess->banned_email_str, p_pass_str))
  {
    return kVSFLoginFail;
  }
  if (!str_isempty(&p_sess->email_passwords_str) &&
      (!str_contains_line(&p_sess->email_passwords_str, p_pass_str) ||
       str_isempty(p_pass_str)))
  {
    return kVSFLoginFail;
  }
  /* Store the anonymous identity string */
  str_copy(&p_sess->anon_pass_str, p_pass_str);
  if (str_isempty(&p_sess->anon_pass_str))
  {
    str_alloc_text(&p_sess->anon_pass_str, "?");
  }
  /* "Fix" any characters which might upset the log processing */
  str_replace_char(&p_sess->anon_pass_str, ' ', '_');
  str_replace_char(&p_sess->anon_pass_str, '\n', '?');
  {
    struct mystr ftp_username_str = INIT_MYSTR;
    if (tunable_ftp_username)
    {
      str_alloc_text(&ftp_username_str, tunable_ftp_username);
    }
    setup_username_globals(p_sess, &ftp_username_str);
    str_free(&ftp_username_str);
  }
  str_free(&p_sess->banned_email_str);
  str_free(&p_sess->email_passwords_str);
  return kVSFLoginAnon;
}

static enum EVSFPrivopLoginResult
handle_local_login(struct vsf_session* p_sess,
                   struct mystr* p_user_str,
                   const struct mystr* p_pass_str)
{
  if (!vsf_sysdep_check_auth(p_user_str, p_pass_str, &p_sess->remote_ip_str))
  {
    return kVSFLoginFail;
  }
  setup_username_globals(p_sess, p_user_str);
  return kVSFLoginReal;
}

static void
setup_username_globals(struct vsf_session* p_sess, const struct mystr* p_str)
{
  str_copy(&p_sess->user_str, p_str);
  if (tunable_setproctitle_enable)
  {
    struct mystr prefix_str = INIT_MYSTR;
    str_copy(&prefix_str, &p_sess->remote_ip_str);
    str_append_char(&prefix_str, '/');
    str_append_str(&prefix_str, p_str);
    vsf_sysutil_set_proctitle_prefix(&prefix_str);
    str_free(&prefix_str);
  }
  p_sess->write_enable = 1;
}


/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * prelogin.c
 *
 * Code to parse the FTP protocol prior to a successful login.
 */

#include "prelogin.h"
#include "ftpcmdio.h"
#include "ftpcodes.h"
#include "str.h"
#include "vsftpver.h"
#include "tunables.h"
#include "oneprocess.h"
#include "twoprocess.h"
#include "sysdeputil.h"
#include "sysutil.h"
#include "session.h"
#include "banner.h"
#include "logging.h"
#include "ssl.h"
#include "features.h"
#include "defs.h"
#include "opts.h"

/* Functions used */
static void check_limits(struct vsf_session* p_sess);
static void emit_greeting(struct vsf_session* p_sess);
static void parse_username_password(struct vsf_session* p_sess);
static void handle_user_command(struct vsf_session* p_sess);
static void handle_pass_command(struct vsf_session* p_sess);
static void handle_get(struct vsf_session* p_sess);
static void check_login_delay();
static void check_login_fails(struct vsf_session* p_sess);

void
init_connection(struct vsf_session* p_sess)
{
  if (tunable_setproctitle_enable)
  {
    vsf_sysutil_setproctitle("not logged in");
  }
  /* Before we talk to the remote, make sure an alarm is set up in case
   * writing the initial greetings should block.
   */
  vsf_cmdio_set_alarm(p_sess);
  /* Check limits before doing an implicit SSL handshake, to avoid DoS
   * attacks. This will result in plain text messages being sent to the SSL
   * client, but we can live with that.
   */
  check_limits(p_sess);
  if (tunable_ssl_enable && tunable_implicit_ssl)
  {
    ssl_control_handshake(p_sess);
  }
  if (tunable_ftp_enable)
  {
    emit_greeting(p_sess);
  }
  parse_username_password(p_sess);
}

static void
check_limits(struct vsf_session* p_sess)
{
  struct mystr str_log_line = INIT_MYSTR;
  /* Check for client limits (standalone mode only) */
  if (tunable_max_clients > 0 &&
      p_sess->num_clients > tunable_max_clients)
  {
    str_alloc_text(&str_log_line, "Connection refused: too many sessions.");
    vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
    vsf_cmdio_write_exit(p_sess, FTP_TOO_MANY_USERS,
      "There are too many connected users, please try later.", 1);
  }
  if (tunable_max_per_ip > 0 &&
      p_sess->num_this_ip > tunable_max_per_ip)
  {
    str_alloc_text(&str_log_line,
                   "Connection refused: too many sessions for this address.");
    vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
    vsf_cmdio_write_exit(p_sess, FTP_IP_LIMIT,
      "There are too many connections from your internet address.", 1);
  }
  if (!p_sess->tcp_wrapper_ok)
  {
    str_alloc_text(&str_log_line,
                   "Connection refused: tcp_wrappers denial.");
    vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
    vsf_cmdio_write_exit(p_sess, FTP_IP_DENY, "Service not available.", 1);
  }
  vsf_log_line(p_sess, kVSFLogEntryConnection, &str_log_line);
}

static void
emit_greeting(struct vsf_session* p_sess)
{
  if (!str_isempty(&p_sess->banner_str))
  {
    vsf_banner_write(p_sess, &p_sess->banner_str, FTP_GREET);
    str_free(&p_sess->banner_str);
    vsf_cmdio_write(p_sess, FTP_GREET, "");
  }
  else if (tunable_ftpd_banner == 0)
  {
    vsf_cmdio_write(p_sess, FTP_GREET, "(vsFTPd " VSF_VERSION 
                    ")");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_GREET, tunable_ftpd_banner);
  }
}

static void
parse_username_password(struct vsf_session* p_sess)
{
  while (1)
  {
    vsf_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str,
                              &p_sess->ftp_arg_str, 1);
    if (tunable_ftp_enable)
    {
      if (str_equal_text(&p_sess->ftp_cmd_str, "USER"))
      {
        handle_user_command(p_sess);
      }
      else if (str_equal_text(&p_sess->ftp_cmd_str, "PASS"))
      {
        handle_pass_command(p_sess);
      }
      else if (str_equal_text(&p_sess->ftp_cmd_str, "QUIT"))
      {
        vsf_cmdio_write_exit(p_sess, FTP_GOODBYE, "Goodbye.", 0);
      }
      else if (str_equal_text(&p_sess->ftp_cmd_str, "FEAT"))
      {
        handle_feat(p_sess);
      }
      else if (str_equal_text(&p_sess->ftp_cmd_str, "OPTS"))
      {
        handle_opts(p_sess);
      }
      else if (tunable_ssl_enable &&
               str_equal_text(&p_sess->ftp_cmd_str, "AUTH") &&
               !p_sess->control_use_ssl)
      {
        handle_auth(p_sess);
      }
      else if (tunable_ssl_enable &&
               str_equal_text(&p_sess->ftp_cmd_str, "PBSZ"))
      {
        handle_pbsz(p_sess);
      }
      else if (tunable_ssl_enable &&
               str_equal_text(&p_sess->ftp_cmd_str, "PROT"))
      {
        handle_prot(p_sess);
      }
      else if (str_isempty(&p_sess->ftp_cmd_str) &&
               str_isempty(&p_sess->ftp_arg_str))
      {
        /* Deliberately ignore to avoid NAT device bugs, as per ProFTPd. */
      }
      else
      {
        vsf_cmdio_write(p_sess, FTP_LOGINERR,
                        "Please login with USER and PASS.");
      }
    }
    else if (tunable_http_enable)
    {
      if (str_equal_text(&p_sess->ftp_cmd_str, "GET"))
      {
        handle_get(p_sess);
      }
      else
      {
        vsf_cmdio_write(p_sess, FTP_LOGINERR, "Bad HTTP verb.");
      }
      vsf_sysutil_exit(0);
    }
  }
}

static void
handle_get(struct vsf_session* p_sess)
{
  p_sess->is_http = 1;
  str_copy(&p_sess->http_get_arg, &p_sess->ftp_arg_str);
  str_alloc_text(&p_sess->user_str, "FTP");
  str_alloc_text(&p_sess->ftp_arg_str, "<http>");
  handle_pass_command(p_sess);
}

static void
handle_user_command(struct vsf_session* p_sess)
{
  /* SECURITY: If we're in anonymous only-mode, immediately reject
   * non-anonymous usernames in the hope we save passwords going plaintext
   * over the network
   */
  int is_anon = 1;
  str_copy(&p_sess->user_str, &p_sess->ftp_arg_str);
  str_upper(&p_sess->ftp_arg_str);
  if (!str_equal_text(&p_sess->ftp_arg_str, "ANONYMOUS"))
  {
    is_anon = 0;
  }
  if (!tunable_local_enable && !is_anon)
  {
    vsf_cmdio_write(
      p_sess, FTP_LOGINERR, "This FTP server is anonymous only.");
    str_empty(&p_sess->user_str);
    return;
  }
  if (is_anon && p_sess->control_use_ssl && !tunable_allow_anon_ssl &&
      !tunable_force_anon_logins_ssl)
  {
    vsf_cmdio_write(
      p_sess, FTP_LOGINERR, "Anonymous sessions may not use encryption.");
    str_empty(&p_sess->user_str);
    return;
  }
  if (tunable_ssl_enable && !is_anon && !p_sess->control_use_ssl &&
      tunable_force_local_logins_ssl)
  {
    vsf_cmdio_write_exit(
      p_sess, FTP_LOGINERR, "Non-anonymous sessions must use encryption.", 1);
  }
  if (tunable_ssl_enable && is_anon && !p_sess->control_use_ssl &&
      tunable_force_anon_logins_ssl)
  { 
    vsf_cmdio_write_exit(
      p_sess, FTP_LOGINERR, "Anonymous sessions must use encryption.", 1);
  }
  if (tunable_userlist_enable)
  {
    int located = str_contains_line(&p_sess->userlist_str, &p_sess->user_str);
    if ((located && tunable_userlist_deny) ||
        (!located && !tunable_userlist_deny))
    {
      check_login_delay();
      vsf_cmdio_write(p_sess, FTP_LOGINERR, "Permission denied.");
      check_login_fails(p_sess);
      str_empty(&p_sess->user_str);
      return;
    }
  }

  if (str_equal_text(&p_sess->ftp_arg_str, "ROOT"))
  {
    vsf_cmdio_write(p_sess, FTP_LOGINERR, "Permission denied.");
    str_empty(&p_sess->user_str);
    return;
  }

  if (is_anon && tunable_no_anon_password)
  {
    /* Fake a password */
    str_alloc_text(&p_sess->ftp_arg_str, "<no password>");
    handle_pass_command(p_sess);
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_GIVEPWORD, "Please specify the password.");
  }
}

static void
handle_pass_command(struct vsf_session* p_sess)
{
  if (str_isempty(&p_sess->user_str))
  {
    vsf_cmdio_write(p_sess, FTP_NEEDUSER, "Login with USER first.");
    return;
  }
  /* These login calls never return if successful */
  if (tunable_one_process_model)
  {
    vsf_one_process_login(p_sess, &p_sess->ftp_arg_str);
  }
  else
  {
    vsf_two_process_login(p_sess, &p_sess->ftp_arg_str);
  }
  vsf_cmdio_write(p_sess, FTP_LOGINERR, "Login incorrect.");
  check_login_fails(p_sess);
  str_empty(&p_sess->user_str);
  /* FALLTHRU if login fails */
}

static void check_login_delay()
{
  if (tunable_delay_failed_login)
  {
    vsf_sysutil_sleep((double) tunable_delay_failed_login);
  }
}

static void check_login_fails(struct vsf_session* p_sess)
{
  if (++p_sess->login_fails >= tunable_max_login_fails)
  {
    vsf_sysutil_shutdown_failok(VSFTP_COMMAND_FD);
    vsf_sysutil_exit(1);
  }
}

/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * ftpcmdio.c
 *
 * Routines applicable to reading and writing the FTP command stream.
 */

#include "ftpcmdio.h"
#include "ftpcodes.h"
#include "str.h"
#include "netstr.h"
#include "sysutil.h"
#include "tunables.h"
#include "defs.h"
#include "secbuf.h"
#include "utility.h"
#include "logging.h"
#include "session.h"
#include "readwrite.h"
#include "utility.h"
#include <stdlib.h>
#include <string.h>

/* Internal functions */
static int control_getline(struct mystr* p_str, struct vsf_session* p_sess);
static void ftp_write_text_common(struct vsf_session* p_sess, int status,
                                  const char* p_text, char sep);
static void ftp_write_str_common(struct vsf_session* p_sess, int status,
                                 char sep, const struct mystr* p_str);
static void handle_alarm_timeout(void* p_private);

void
vsf_cmdio_sock_setup(void)
{
  vsf_sysutil_activate_keepalive(VSFTP_COMMAND_FD);
  vsf_sysutil_set_nodelay(VSFTP_COMMAND_FD);
  vsf_sysutil_activate_oobinline(VSFTP_COMMAND_FD);
}

static void
handle_alarm_timeout(void* p_private)
{
  struct vsf_session* p_sess = (struct vsf_session*) p_private;
  p_sess->idle_timeout = 1;
  vsf_sysutil_activate_noblock(VSFTP_COMMAND_FD);
  vsf_sysutil_shutdown_read_failok(VSFTP_COMMAND_FD);
}

void
vsf_cmdio_write(struct vsf_session* p_sess, int status, const char* p_text)
{
  ftp_write_text_common(p_sess, status, p_text, ' ');
}

void
vsf_cmdio_write_hyphen(struct vsf_session* p_sess, int status,
                       const char* p_text)
{
  ftp_write_text_common(p_sess, status, p_text, '-');
}

void
vsf_cmdio_write_raw(struct vsf_session* p_sess, const char* p_text)
{
  static struct mystr s_the_str;
  int retval;
  str_alloc_text(&s_the_str, p_text);
  if (tunable_log_ftp_protocol)
  {
    vsf_log_line(p_sess, kVSFLogEntryFTPOutput, &s_the_str);
  }
  retval = ftp_write_str(p_sess, &s_the_str, kVSFRWControl);
  if (retval != 0)
  {
    die("ftp_write_str");
  }
}

void
vsf_cmdio_write_exit(struct vsf_session* p_sess, int status, const char* p_text,
                     int exit_val)
{
  /* Unblock any readers on the dying control channel. This is needed for SSL
   * connections, where the SSL control channel slave is in a separate
   * process.
   */
  vsf_sysutil_activate_noblock(VSFTP_COMMAND_FD);
  vsf_sysutil_shutdown_read_failok(VSFTP_COMMAND_FD);
  vsf_cmdio_write(p_sess, status, p_text);
  vsf_sysutil_shutdown_failok(VSFTP_COMMAND_FD);
  vsf_sysutil_exit(exit_val);
}

static void
ftp_write_text_common(struct vsf_session* p_sess, int status,
                      const char* p_text, char sep)
{
  /* XXX - could optimize */
  static struct mystr s_the_str;
  str_alloc_text(&s_the_str, p_text);
  ftp_write_str_common(p_sess, status, sep, &s_the_str);
}

void
vsf_cmdio_write_str_hyphen(struct vsf_session* p_sess, int status,
                           const struct mystr* p_str)
{
  ftp_write_str_common(p_sess, status, '-', p_str);
}

void
vsf_cmdio_write_str(struct vsf_session* p_sess, int status,
                    const struct mystr* p_str)
{
  ftp_write_str_common(p_sess, status, ' ', p_str);
}

static void
ftp_write_str_common(struct vsf_session* p_sess, int status, char sep,
                     const struct mystr* p_str)
{
  static struct mystr s_write_buf_str;
  static struct mystr s_text_mangle_str;
  int retval;
  if (tunable_log_ftp_protocol)
  {
    str_alloc_ulong(&s_write_buf_str, (unsigned long) status);
    str_append_char(&s_write_buf_str, sep);
    str_append_str(&s_write_buf_str, p_str);
    vsf_log_line(p_sess, kVSFLogEntryFTPOutput, &s_write_buf_str);
  }
  str_copy(&s_text_mangle_str, p_str);
  /* Process the output response according to the specifications.. */
  /* Escape telnet characters properly */
  str_replace_text(&s_text_mangle_str, "\377", "\377\377");
  /* Change \n for \0 in response */
  str_replace_char(&s_text_mangle_str, '\n', '\0');
  /* Build string to squirt down network */
  str_alloc_ulong(&s_write_buf_str, (unsigned long) status);
  str_append_char(&s_write_buf_str, sep);
  str_append_str(&s_write_buf_str, &s_text_mangle_str);
  str_append_text(&s_write_buf_str, "\r\n");
  retval = ftp_write_str(p_sess, &s_write_buf_str, kVSFRWControl);
  if (retval != 0)
  {
    die("ftp_write");
  }
}

void
vsf_cmdio_set_alarm(struct vsf_session* p_sess)
{
  if (tunable_idle_session_timeout > 0)
  {
    vsf_sysutil_install_sighandler(kVSFSysUtilSigALRM,
                                   handle_alarm_timeout,
                                   p_sess,
                                   1);
    vsf_sysutil_set_alarm(tunable_idle_session_timeout);
  }
}

void
vsf_cmdio_get_cmd_and_arg(struct vsf_session* p_sess, struct mystr* p_cmd_str,
                          struct mystr* p_arg_str, int set_alarm)
{
  int ret;
  /* Prepare an alarm to timeout the session.. */
  if (set_alarm)
  {
    vsf_cmdio_set_alarm(p_sess);
  }
  /* Blocks */
  ret = control_getline(p_cmd_str, p_sess);
  if (p_sess->idle_timeout)
  {
    vsf_cmdio_write_exit(p_sess, FTP_IDLE_TIMEOUT, "Timeout.", 1);
  }
  if (ret == 0)
  {
    /* Remote end hung up without a polite QUIT. The shutdown is to make
     * sure buggy clients don't ever see an OOPS message.
     */
    vsf_sysutil_shutdown_failok(VSFTP_COMMAND_FD);
    vsf_sysutil_exit(1);
  }
  /* View a single space as a command of " ", which although a useless command,
   * permits the caller to distinguish input of "" from " ".
   */
  if (str_getlen(p_cmd_str) == 1 && str_get_char_at(p_cmd_str, 0) == ' ')
  {
    str_empty(p_arg_str);
  }
  else
  {
    str_split_char(p_cmd_str, p_arg_str, ' ');
  }
  str_upper(p_cmd_str);
  if (!str_isempty(p_arg_str)) {
    char *tmp_str;
    tmp_str = remote2local(str_getbuf(p_arg_str));
    if (tmp_str != NULL) {
      str_empty(p_arg_str);
      str_append_text(p_arg_str, tmp_str);
      vsf_sysutil_free(tmp_str);
    }
  }
  if (tunable_log_ftp_protocol)
  {
    static struct mystr s_log_str;
    if (str_equal_text(p_cmd_str, "PASS"))
    {
      str_alloc_text(&s_log_str, "PASS <password>");
    }
    else
    {
      str_copy(&s_log_str, p_cmd_str);
      if (!str_isempty(p_arg_str))
      {
        str_append_char(&s_log_str, ' ');
        str_append_str(&s_log_str, p_arg_str);
      }
    }
    vsf_log_line(p_sess, kVSFLogEntryFTPInput, &s_log_str);
  }
}

static int
control_getline(struct mystr* p_str, struct vsf_session* p_sess)
{
  int ret;
  if (p_sess->p_control_line_buf == 0)
  {
    vsf_secbuf_alloc(&p_sess->p_control_line_buf, VSFTP_MAX_COMMAND_LINE);
  }
  ret = ftp_getline(p_sess, p_str, p_sess->p_control_line_buf);
  if (ret == 0)
  {
    return ret;
  }
  else if (ret < 0)
  {
    vsf_cmdio_write_exit(p_sess, FTP_BADCMD, "Input line too long.", 1);
  }
  /* As mandated by the FTP specifications.. */
  str_replace_char(p_str, '\0', '\n');
  /* If the last character is a \r, strip it */
  {
    unsigned int len = str_getlen(p_str);
    while (len > 0 && str_get_char_at(p_str, len - 1) == '\r')
    {
      str_trunc(p_str, len - 1);
      --len;
    }
  }
  return 1;
}


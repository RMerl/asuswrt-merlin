/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 *
 * logging.c
 */

#include "logging.h"
#include "tunables.h"
#include "utility.h"
#include "str.h"
#include "sysutil.h"
#include "sysstr.h"
#include "session.h"

/* File local functions */
static int vsf_log_type_is_transfer(enum EVSFLogEntryType type);
static void vsf_log_common(struct vsf_session* p_sess, int succeeded,
                           enum EVSFLogEntryType what,
                           const struct mystr* p_str);
static void vsf_log_do_log_vsftpd_format(struct vsf_session* p_sess,
                                         struct mystr* p_str, int succeeded,
                                         enum EVSFLogEntryType what,
                                         const struct mystr* p_log_str);
static void vsf_log_do_log_wuftpd_format(struct vsf_session* p_sess,
                                         struct mystr* p_str, int succeeded);
static void vsf_log_do_log_to_file(int fd, struct mystr* p_str);

void
vsf_log_init(struct vsf_session* p_sess)
{
  if (tunable_syslog_enable || tunable_tcp_wrappers)
  {
    vsf_sysutil_openlog(1);
  }
  if (!tunable_xferlog_enable && !tunable_dual_log_enable)
  {
    return;
  }
  if (tunable_dual_log_enable || tunable_xferlog_std_format)
  {
    int retval = -1;
    if (tunable_xferlog_file)
    {
      retval = vsf_sysutil_create_or_open_file_append(tunable_xferlog_file,
                                                      0600);
    }
    if (vsf_sysutil_retval_is_error(retval))
    {
      die2("failed to open xferlog log file:", tunable_xferlog_file);
    }
    p_sess->xferlog_fd = retval;
  }
  if (tunable_dual_log_enable || !tunable_xferlog_std_format)
  {
    if (!tunable_syslog_enable)
    {
      int retval = -1;
      if (tunable_vsftpd_log_file)
      {
        retval = vsf_sysutil_create_or_open_file_append(tunable_vsftpd_log_file,
                                                        0600);
      }
      if (vsf_sysutil_retval_is_error(retval))
      {
        die2("failed to open vsftpd log file:", tunable_vsftpd_log_file);
      }
      p_sess->vsftpd_log_fd = retval;
    }
  }
}

static int
vsf_log_type_is_transfer(enum EVSFLogEntryType type)
{
  return (type == kVSFLogEntryDownload || type == kVSFLogEntryUpload);
}

void
vsf_log_start_entry(struct vsf_session* p_sess, enum EVSFLogEntryType what)
{
  if (p_sess->log_type != 0)
  {
    bug("non null log_type in vsf_log_start_entry");
  }
  p_sess->log_type = (unsigned long) what;
  p_sess->log_start_sec = 0;
  p_sess->log_start_usec = 0;
  p_sess->transfer_size = 0;
  str_empty(&p_sess->log_str);
  if (vsf_log_type_is_transfer(what))
  {
    p_sess->log_start_sec = vsf_sysutil_get_time_sec();
    p_sess->log_start_usec = vsf_sysutil_get_time_usec();
  }
}

void
vsf_log_line(struct vsf_session* p_sess, enum EVSFLogEntryType what,
             struct mystr* p_str)
{
  vsf_log_common(p_sess, 1, what, p_str);
}

int
vsf_log_entry_pending(struct vsf_session* p_sess)
{
  if (p_sess->log_type == 0)
  {
    return 0;
  }
  return 1;
}

void
vsf_log_clear_entry(struct vsf_session* p_sess)
{
  p_sess->log_type = 0;
}

void
vsf_log_do_log(struct vsf_session* p_sess, int succeeded)
{
  vsf_log_common(p_sess, succeeded, (enum EVSFLogEntryType) p_sess->log_type,
                 &p_sess->log_str);
  p_sess->log_type = 0;
}

static void
vsf_log_common(struct vsf_session* p_sess, int succeeded,
               enum EVSFLogEntryType what, const struct mystr* p_str)
{
  static struct mystr s_log_str;
  /* Handle xferlog line if appropriate */
  if (p_sess->xferlog_fd != -1 && vsf_log_type_is_transfer(what))
  {
    vsf_log_do_log_wuftpd_format(p_sess, &s_log_str, succeeded);
    vsf_log_do_log_to_file(p_sess->xferlog_fd, &s_log_str);
  }
  /* Handle vsftpd.log line if appropriate */
  if (p_sess->vsftpd_log_fd != -1)
  {
    vsf_log_do_log_vsftpd_format(p_sess, &s_log_str, succeeded, what, p_str);
    vsf_log_do_log_to_file(p_sess->vsftpd_log_fd, &s_log_str);
  }
  /* Handle syslog() line if appropriate */
  if (tunable_syslog_enable)
  {
    int severe = 0;
    vsf_log_do_log_vsftpd_format(p_sess, &s_log_str, succeeded, what, p_str);
    if (what == kVSFLogEntryLogin && !succeeded)
    {
      severe = 1;
    }
    str_syslog(&s_log_str, severe);
  }
}

static void
vsf_log_do_log_to_file(int fd, struct mystr* p_str)
{
  if (!tunable_no_log_lock)
  {
    int retval = vsf_sysutil_lock_file_write(fd);
    if (vsf_sysutil_retval_is_error(retval))
    {
      return;
    }
  }
  str_replace_unprintable(p_str, '?');
  str_append_char(p_str, '\n');
  /* Ignore write failure; maybe the disk filled etc. */
  (void) str_write_loop(p_str, fd);
  if (!tunable_no_log_lock)
  {
    vsf_sysutil_unlock_file(fd);
  }
}

static void
vsf_log_do_log_wuftpd_format(struct vsf_session* p_sess, struct mystr* p_str,
                             int succeeded)
{
  static struct mystr s_filename_str;
  long delta_sec;
  enum EVSFLogEntryType what = (enum EVSFLogEntryType) p_sess->log_type;
  /* Date - vsf_sysutil_get_current_date updates cached time */
  str_alloc_text(p_str, vsf_sysutil_get_current_date());
  str_append_char(p_str, ' ');
  /* Transfer time (in seconds) */
  delta_sec = vsf_sysutil_get_time_sec() - p_sess->log_start_sec;
  if (delta_sec <= 0)
  {
    delta_sec = 1;
  }
  str_append_ulong(p_str, (unsigned long) delta_sec);
  str_append_char(p_str, ' ');
  /* Remote host name */
  str_append_str(p_str, &p_sess->remote_ip_str);
  str_append_char(p_str, ' ');
  /* Bytes transferred */
  str_append_filesize_t(p_str, p_sess->transfer_size);
  str_append_char(p_str, ' ');
  /* Filename */
  str_copy(&s_filename_str, &p_sess->log_str);
  str_replace_char(&s_filename_str, ' ', '_');
  str_append_str(p_str, &s_filename_str);
  str_append_char(p_str, ' ');
  /* Transfer type (ascii/binary) */
  if (p_sess->is_ascii)
  {
    str_append_text(p_str, "a ");
  }
  else
  {
    str_append_text(p_str, "b ");
  }
  /* Special action flag - tar, gzip etc. */
  str_append_text(p_str, "_ ");
  /* Direction of transfer */
  if (what == kVSFLogEntryUpload)
  {
    str_append_text(p_str, "i ");
  }
  else
  {
    str_append_text(p_str, "o ");
  }
  /* Access mode: anonymous/real user, and identity */
  if (p_sess->is_anonymous && !p_sess->is_guest)
  {
    str_append_text(p_str, "a ");
    str_append_str(p_str, &p_sess->anon_pass_str);
  }
  else
  {
    if (p_sess->is_guest)
    {
      str_append_text(p_str, "g ");
    } 
    else
    {
      str_append_text(p_str, "r ");
    }
    str_append_str(p_str, &p_sess->user_str);
  }
  str_append_char(p_str, ' ');
  /* Service name, authentication method, authentication user id */
  str_append_text(p_str, "ftp 0 * ");
  /* Completion status */
  if (succeeded)
  {
    str_append_char(p_str, 'c');
  }
  else
  {
    str_append_char(p_str, 'i');
  }
}

static void
vsf_log_do_log_vsftpd_format(struct vsf_session* p_sess, struct mystr* p_str,
                             int succeeded, enum EVSFLogEntryType what,
                             const struct mystr* p_log_str)
{
  str_empty(p_str);
  if (!tunable_syslog_enable)
  {
    /* Date - vsf_sysutil_get_current_date updates cached time */
    str_append_text(p_str, vsf_sysutil_get_current_date());
    /* Pid */
    str_append_text(p_str, " [pid ");
    str_append_ulong(p_str, vsf_sysutil_getpid());
    str_append_text(p_str, "] ");
  }
  /* User */
  if (!str_isempty(&p_sess->user_str))
  {
    str_append_char(p_str, '[');
    str_append_str(p_str, &p_sess->user_str);
    str_append_text(p_str, "] ");
  }
  /* And the action */
  if (what != kVSFLogEntryFTPInput && what != kVSFLogEntryFTPOutput &&
      what != kVSFLogEntryConnection && what != kVSFLogEntryDebug)
  {
    if (succeeded)
    {
      str_append_text(p_str, "OK ");
    }
    else
    {
      str_append_text(p_str, "FAIL ");
    }
  }
  switch (what)
  {
    case kVSFLogEntryDownload:
      str_append_text(p_str, "DOWNLOAD");
      break;
    case kVSFLogEntryUpload:
      str_append_text(p_str, "UPLOAD");
      break;
    case kVSFLogEntryMkdir:
      str_append_text(p_str, "MKDIR");
      break;
    case kVSFLogEntryLogin:
      str_append_text(p_str, "LOGIN");
      break;
    case kVSFLogEntryFTPInput:
      str_append_text(p_str, "FTP command");
      break;
    case kVSFLogEntryFTPOutput:
      str_append_text(p_str, "FTP response");
      break;
    case kVSFLogEntryConnection:
      str_append_text(p_str, "CONNECT");
      break;
    case kVSFLogEntryDelete:
      str_append_text(p_str, "DELETE");
      break;
    case kVSFLogEntryRename:
      str_append_text(p_str, "RENAME");
      break;
    case kVSFLogEntryRmdir:
      str_append_text(p_str, "RMDIR");
      break;
    case kVSFLogEntryChmod:
      str_append_text(p_str, "CHMOD");
      break;
    case kVSFLogEntryDebug:
      str_append_text(p_str, "DEBUG");
      break;
    case kVSFLogEntryNull:
      /* Fall through */
    default:
      bug("bad entry_type in vsf_log_do_log");
      break;
  }
  str_append_text(p_str, ": Client \"");
  str_append_str(p_str, &p_sess->remote_ip_str);
  str_append_char(p_str, '"');
  if (what == kVSFLogEntryLogin && !str_isempty(&p_sess->anon_pass_str))
  {
    str_append_text(p_str, ", anon password \"");
    str_append_str(p_str, &p_sess->anon_pass_str);
    str_append_char(p_str, '"');
  }
  if (!str_isempty(p_log_str))
  {
    str_append_text(p_str, ", \"");
    str_append_str(p_str, p_log_str);
    str_append_char(p_str, '"');
  }
  if (what != kVSFLogEntryFTPInput && what != kVSFLogEntryFTPOutput &&
      what != kVSFLogEntryDebug)
  {
    if (p_sess->transfer_size)
    {
      str_append_text(p_str, ", ");
      str_append_filesize_t(p_str, p_sess->transfer_size);
      str_append_text(p_str, " bytes");
    }
    if (vsf_log_type_is_transfer(what))
    {
      long delta_sec = vsf_sysutil_get_time_sec() - p_sess->log_start_sec;
      long delta_usec = vsf_sysutil_get_time_usec() - p_sess->log_start_usec;
      double time_delta = (double) delta_sec + ((double) delta_usec /
                                                (double) 1000000);
      double kbyte_rate;
      if (time_delta <= 0)
      {
        time_delta = 0.1;
      }
      kbyte_rate =
        ((double) p_sess->transfer_size / time_delta) / (double) 1024;
      str_append_text(p_str, ", ");
      str_append_double(p_str, kbyte_rate);
      str_append_text(p_str, "Kbyte/sec");
    }
  }
}


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
  struct vsf_session* p_sess, const struct mystr* p_user_str,
  const struct mystr* p_pass_str);
static void setup_username_globals(struct vsf_session* p_sess,
                                   const struct mystr* p_str);
static enum EVSFPrivopLoginResult handle_login(
  struct vsf_session* p_sess, const struct mystr* p_user_str,
  const struct mystr* p_pass_str);

int
vsf_privop_get_ftp_port_sock(struct vsf_session* p_sess)
{
  static struct vsf_sysutil_sockaddr* p_sockaddr;
  int retval;
  int s = vsf_sysutil_get_ipsock(p_sess->p_local_addr);
  vsf_sysutil_activate_reuseaddr(s);
  vsf_sysutil_sockaddr_clone(&p_sockaddr, p_sess->p_local_addr);
  vsf_sysutil_sockaddr_set_port(p_sockaddr, tunable_ftp_data_port);
  retval = vsf_sysutil_bind(s, p_sockaddr);
  if (retval != 0)
  {
    die("vsf_sysutil_bind");
  }
  return s;
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
  }
  else
  {
    vsf_log_do_log(p_sess, 1);
  }
  return result;
}

static enum EVSFPrivopLoginResult
handle_login(struct vsf_session* p_sess, const struct mystr* p_user_str,
             const struct mystr* p_pass_str)
{
  /* Do not assume PAM can cope with dodgy input, even though it
   * almost certainly can.
   */
  int anonymous_login = 0;
  unsigned int len = str_getlen(p_user_str);
  if (len == 0 || len > VSFTP_USERNAME_MAX)
  {
    return kVSFLoginFail;
  }
  /* Throw out dodgy start characters */
  if (!vsf_sysutil_isalnum(str_get_char_at(p_user_str, 0)))
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
    if (
//    	str_equal_text(&upper_str, "FTP") ||	// Jiahao
        str_equal_text(&upper_str, "ANONYMOUS")
       )
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
    str_alloc_text(&ftp_username_str, tunable_ftp_username);
    setup_username_globals(p_sess, &ftp_username_str);
    str_free(&ftp_username_str);
  }
  str_free(&p_sess->banned_email_str);
  str_free(&p_sess->email_passwords_str);
  return kVSFLoginAnon;
}

static enum EVSFPrivopLoginResult
handle_local_login(struct vsf_session* p_sess,
                   const struct mystr* p_user_str,
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
// 2007.08 James {
  //int snum, unum, i, j;
  //char user[64], user1[64], wright[384], tmpstr[64];
// 2007.08 James }

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
  
// 2007.08 James {
  p_sess->write_enable = 1;
  /*char cwdpath[VSFTP_PATH_MAX];
  vsf_sysutil_getcwd(cwdpath, VSFTP_PATH_MAX);

	if (nvram_match("st_ftp_mode", "1"))
	{
		p_sess->write_enable = 1;
		return;
	}

	unum = atoi(nvram_safe_get("acc_num"));
	for(i=-1;i<unum;i++)
	{
		if (i==-1)
		{
			strcpy(user, "Guest");
			strcpy(user1, "anonymous");
		}
		else
		{
			sprintf(tmpstr, "acc_username%d", i);
			strcpy(user, nvram_safe_get(tmpstr));	
			strcpy(user1, user);
		}
		
		if(strcmp(str_getbuf(p_str), user1)==0)
		{
			snum = atoi(nvram_safe_get("sh_num"));

			for(j=0;j<snum;j++)
			{
				sprintf(tmpstr, "sh_wright%d", j);
				strcpy(wright, nvram_safe_get(tmpstr));

				if (test_user(wright, user)==1)
				{
					p_sess->write_enable = 1;
					return;
				}
			}
			
			p_sess->write_enable = 0;
			return;
		}
	}
	
	p_sess->write_enable = 0;*/
// 2007.08 James }
	return;
  
}


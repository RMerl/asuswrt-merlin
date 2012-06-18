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
 * postlogin.c
 */

#include "postlogin.h"
#include "session.h"
#include "oneprocess.h"
#include "twoprocess.h"
#include "ftpcodes.h"
#include "ftpcmdio.h"
#include "ftpdataio.h"
#include "utility.h"
#include "tunables.h"
#include "str.h"
#include "sysstr.h"
#include "banner.h"
#include "sysutil.h"
#include "logging.h"
#include "sysdeputil.h"
#include "ipaddrparse.h"
#include "access.h"
#include "features.h"
#include "ssl.h"
#include "vsftpver.h"
#include <stdio.h>
#include "utility.h"	// Jiahao
#include <stdlib.h>
#include <string.h>
// 2007.05 James {
#include <unistd.h>
#include <bcmnvram.h>
#include <limits.h>
#include "defs.h"
#include <usb_info.h>
#include <disk_io_tools.h>
#include <disk_share.h>
// 2007.05 James }
#include "opts.h"

/* Private local functions */
static void handle_pwd(struct vsf_session* p_sess);
static void handle_cwd(struct vsf_session* p_sess);
static void handle_pasv(struct vsf_session* p_sess, int is_epsv);
static void handle_retr(struct vsf_session* p_sess);
static void handle_cdup(struct vsf_session* p_sess);
static void handle_list(struct vsf_session* p_sess);
static void handle_type(struct vsf_session* p_sess);
static void handle_port(struct vsf_session* p_sess);
static void handle_stor(struct vsf_session* p_sess);
static void handle_mkd(struct vsf_session* p_sess);
static void handle_rmd(struct vsf_session* p_sess);
static void handle_dele(struct vsf_session* p_sess);
static void handle_rest(struct vsf_session* p_sess);
static void handle_rnfr(struct vsf_session* p_sess);
static void handle_rnto(struct vsf_session* p_sess);
static void handle_nlst(struct vsf_session* p_sess);
static void handle_size(struct vsf_session* p_sess);
static void handle_site(struct vsf_session* p_sess);
static void handle_appe(struct vsf_session* p_sess);
static void handle_mdtm(struct vsf_session* p_sess);
static void handle_site_chmod(struct vsf_session* p_sess,
                              struct mystr* p_arg_str);
static void handle_site_umask(struct vsf_session* p_sess,
                              struct mystr* p_arg_str);
static void handle_eprt(struct vsf_session* p_sess);
static void handle_help(struct vsf_session* p_sess);
static void handle_stou(struct vsf_session* p_sess);
static void handle_stat(struct vsf_session* p_sess);
static void handle_stat_file(struct vsf_session* p_sess);

static int pasv_active(struct vsf_session* p_sess);
static int port_active(struct vsf_session* p_sess);
static void pasv_cleanup(struct vsf_session* p_sess);
static void port_cleanup(struct vsf_session* p_sess);
static void handle_dir_common(struct vsf_session* p_sess, int full_details,
                              int stat_cmd);
static void prepend_path_to_filename(struct mystr* p_str);
static int get_remote_transfer_fd(struct vsf_session* p_sess,
                                  const char* p_status_msg);
static void check_abor(struct vsf_session* p_sess);
static void handle_sigurg(void* p_private);
static void handle_upload_common(struct vsf_session* p_sess, int is_append,
                                 int is_unique);
static void get_unique_filename(struct mystr* p_outstr,
                                const struct mystr* p_base);
static int data_transfer_checks_ok(struct vsf_session* p_sess);
static void resolve_tilde(struct mystr* p_str, struct vsf_session* p_sess);

void
process_post_login(struct vsf_session* p_sess)
{
  str_getcwd(&p_sess->home_str);
  if (p_sess->is_anonymous)
  {
    vsf_sysutil_set_umask(tunable_anon_umask);
    p_sess->bw_rate_max = tunable_anon_max_rate;
  }
  else
  {
    vsf_sysutil_set_umask(tunable_local_umask);
    p_sess->bw_rate_max = tunable_local_max_rate;
  }
  if (tunable_async_abor_enable)
  {
    vsf_sysutil_install_sighandler(kVSFSysUtilSigURG, handle_sigurg, p_sess);
    vsf_sysutil_activate_sigurg(VSFTP_COMMAND_FD);
  }
  /* Handle any login message */
  vsf_banner_dir_changed(p_sess, FTP_LOGINOK);
  vsf_cmdio_write(p_sess, FTP_LOGINOK, "Login successful.");
  int cmd_ok = 1;
  while(1)
  {
    cmd_ok = 1;
    if (tunable_setproctitle_enable)
    {
      vsf_sysutil_setproctitle("IDLE");
    }
    /* Blocks */
    vsf_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str,
                              &p_sess->ftp_arg_str, 1);
    if (tunable_setproctitle_enable)
    {
      struct mystr proctitle_str = INIT_MYSTR;
      str_copy(&proctitle_str, &p_sess->ftp_cmd_str);
      if (!str_isempty(&p_sess->ftp_arg_str))
      {
        str_append_char(&proctitle_str, ' ');
        str_append_str(&proctitle_str, &p_sess->ftp_arg_str);
      }
      /* Suggestion from Solar */
      str_replace_unprintable(&proctitle_str, '?');
      vsf_sysutil_setproctitle_str(&proctitle_str);
      str_free(&proctitle_str);
    }
    /* Test command against the allowed list.. */
    if (tunable_cmds_allowed)
    {
      static struct mystr s_src_str;
      static struct mystr s_rhs_str;
      str_alloc_text(&s_src_str, tunable_cmds_allowed);
      while (1)
      {
        str_split_char(&s_src_str, &s_rhs_str, ',');
        if (str_isempty(&s_src_str))
        {
          cmd_ok = 0;
          break;
        }
        else if (str_equal(&s_src_str, &p_sess->ftp_cmd_str))
        {
          break;
        }
        str_copy(&s_src_str, &s_rhs_str);
      }
    }
    
// 2007.06 James {
	int layer = 0;
	int user_right = 0;

	char *cmds_about_shared_folder[] = {
                        "MKD", "XMKD",
                        "RNFR", "RNTO",
                        "RMD", "XRMD",
                        0};

	char *need_read_right_commands[] = {
                        "LIST", "NLST",
                        "PWD", "XPWD",
                        "CWD", "XCWD",
                        "CDUP", "XCUP",
                        0};

	char *limited_in_upload_mode_commands[] = {
                        "RETR",
                        0};

	char *need_write_right_commands[] = {
                        "MKD", "XMKD",
                        "STOR", "STOU", "APPE",
                        0};

	char *need_delete_right_commands[] = {
                        "RNFR",
                        "RMD", "XRMD",
                        "DELE",
                        0};

	char *arg_buf;
	char **target_cmd;
	char fullpath[PATH_MAX];
	char *mount_path = NULL, *share_name = NULL;
	int len = 0;
	int chk_path = 0;

	chk_path = 0;
	for(target_cmd = cmds_about_shared_folder; *target_cmd; ++target_cmd){
		if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
			chk_path = 1;
			break;
		}
	}

	if(!chk_path)
		for(target_cmd = need_read_right_commands; *target_cmd; ++target_cmd){
			if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
				chk_path = 1;
				break;
			}
		}

	if(!chk_path)
		for(target_cmd = limited_in_upload_mode_commands; *target_cmd; ++target_cmd){
			if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
				chk_path = 1;
				break;
			}
		}

	if(!chk_path)
		for(target_cmd = need_write_right_commands; *target_cmd; ++target_cmd){
			if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
				chk_path = 1;
				break;
			}
		}

	if(!chk_path)
		for(target_cmd = need_delete_right_commands; *target_cmd; ++target_cmd){
			if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
				chk_path = 1;
				break;
			}
		}
//ftp_dbg("1. cmd=%s, arg=%s, path=%s.\n", str_getbuf(&p_sess->ftp_cmd_str), str_getbuf(&p_sess->ftp_arg_str), str_getbuf(&p_sess->full_path));

	if(!chk_path)
		goto VSFTPD_CMD;

// 2008.05 James. for hiding the true path. {
	arg_buf = (char *)str_getbuf(&p_sess->ftp_arg_str);
	if(chk_path && arg_buf[0] != '-'){
		if(str_getlen(&p_sess->ftp_arg_str) > 0 && arg_buf[0] == '/'){
			str_alloc_text(&p_sess->full_path, POOL_MOUNT_ROOT);
			if(str_getlen(&p_sess->ftp_arg_str) != 1)
				str_append_text(&p_sess->full_path, arg_buf);
		}
		else{
			getcwd(fullpath, PATH_MAX);
			if(str_getlen(&p_sess->ftp_arg_str) == 0){
				str_alloc_text(&p_sess->full_path, fullpath);

				len = strlen(fullpath)-strlen(POOL_MOUNT_ROOT);
				if(len < 0){
					cmd_ok = 0;
					goto VSFTPD_CMD;
				}

				if(len > 0)
					str_alloc_text(&p_sess->ftp_arg_str, fullpath+strlen(POOL_MOUNT_ROOT));
				else
					str_alloc_text(&p_sess->ftp_arg_str, "/");
			}
			else{
				str_alloc_text(&p_sess->full_path, fullpath);
				str_append_text(&p_sess->full_path, "/");
				str_append_str(&p_sess->full_path, &p_sess->ftp_arg_str);

				// Get the true path.
				realpath(str_getbuf(&p_sess->full_path), fullpath);
				str_alloc_text(&p_sess->full_path, fullpath);

				memset(fullpath, 0, PATH_MAX);
				strncpy(fullpath, str_getbuf(&p_sess->full_path), PATH_MAX);

				str_split_text(&p_sess->full_path, &p_sess->ftp_arg_str, POOL_MOUNT_ROOT);

				str_alloc_text(&p_sess->full_path, fullpath);
			}
		}
//ftp_dbg("2. cmd=%s, arg=%s, path=%s.\n", str_getbuf(&p_sess->ftp_cmd_str), str_getbuf(&p_sess->ftp_arg_str), str_getbuf(&p_sess->full_path));

		// remove the char '/' in the end of p_sess->ftp_arg_str.
		arg_buf = (char *)str_getbuf(&p_sess->ftp_arg_str);
		len = str_getlen(&p_sess->ftp_arg_str);
		if(arg_buf != NULL && len > 1 && arg_buf[len-1] == '/'){
			memset(fullpath, 0, PATH_MAX);
			strncpy(fullpath, arg_buf, len-1);
			str_alloc_text(&p_sess->ftp_arg_str, fullpath);
//ftp_dbg("3. cmd=%s, arg=%s, path=%s.\n", str_getbuf(&p_sess->ftp_cmd_str), str_getbuf(&p_sess->ftp_arg_str), str_getbuf(&p_sess->full_path));
		}

		// remove the char '/' in the end of p_sess->full_path.
		arg_buf = (char *)str_getbuf(&p_sess->full_path);
		len = str_getlen(&p_sess->full_path);
		if(arg_buf != NULL && len > 1 && arg_buf[len-1] == '/'){
			str_trunc(&p_sess->full_path, len-1);
//ftp_dbg("4. cmd=%s, arg=%s, path=%s.\n", str_getbuf(&p_sess->ftp_cmd_str), str_getbuf(&p_sess->ftp_arg_str), str_getbuf(&p_sess->full_path));
		}
	}
// 2008.05 James. }

	layer = how_many_layer(str_getbuf(&p_sess->full_path), &mount_path, &share_name);

	if(str_equal_text(&p_sess->ftp_cmd_str, "CDUP") || str_equal_text(&p_sess->ftp_cmd_str, "XCUP"))
		--layer;

//ftp_dbg("layer=%d.\n", layer);
	if(layer < BASE_LAYER){
		cmd_ok = 0;

		if(mount_path != NULL)
			free(mount_path);
		if(share_name != NULL)
			free(share_name);

		goto VSFTPD_CMD;
	}
	p_sess->layer = layer;

	if(layer == SHARE_LAYER && p_sess->is_anonymous){
		for(target_cmd = cmds_about_shared_folder; *target_cmd; ++target_cmd){
			if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd))
				goto VSFTPD_CMD;
		}
	}

	for(target_cmd = need_read_right_commands; *target_cmd; ++target_cmd){
		if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
			if(layer >= SHARE_LAYER && !p_sess->is_anonymous){
				user_right = get_permission(str_getbuf(&p_sess->user_str), mount_path, share_name, "ftp");
				if(user_right < 1)
					cmd_ok = 0;
			}

			goto VSFTPD_CMD;
		}
	}

	for(target_cmd = limited_in_upload_mode_commands; *target_cmd; ++target_cmd){
		if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
			if(layer < SHARE_LAYER)
				cmd_ok = 0;
			else if(!p_sess->is_anonymous){
				user_right = get_permission(str_getbuf(&p_sess->user_str), mount_path, share_name, "ftp");
				if(user_right == 2) // only upload mode
					cmd_ok = 0;
			}

			goto VSFTPD_CMD;
		}
	}

	for(target_cmd = need_write_right_commands; *target_cmd; ++target_cmd){
		if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
			// Can't execute when no right or no record.
			if(layer < SHARE_LAYER)
				cmd_ok = 0;
			else if(!p_sess->is_anonymous){
				user_right = get_permission(str_getbuf(&p_sess->user_str), mount_path, share_name, "ftp");
				if(user_right < 2)
					cmd_ok = 0;
			}

			goto VSFTPD_CMD;
		}
	}

	for(target_cmd = need_delete_right_commands; *target_cmd; ++target_cmd){
		if(str_equal_text(&p_sess->ftp_cmd_str, *target_cmd)){
			if(layer >= SHARE_LAYER && !p_sess->is_anonymous){
				user_right = get_permission(str_getbuf(&p_sess->user_str), mount_path, share_name, "ftp");
                                //user_right = 3;
				if(user_right < 3)
				{
					cmd_ok = 0;
				}
			}
			
			// Can't execute when no right or no record.
			if(layer < SHARE_LAYER)
			{
				cmd_ok = 0;
			}

			goto VSFTPD_CMD;
		}
	}

VSFTPD_CMD:
	if(mount_path != NULL)
		free(mount_path);
	if(share_name != NULL)
		free(share_name);

// 2007.06 James }*/
    if(!cmd_ok)
    {
      vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied (restricted operation).");
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "QUIT"))
    {
      vsf_cmdio_write(p_sess, FTP_GOODBYE, "Goodbye.");
      vsf_sysutil_exit(0);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "PWD") ||
             str_equal_text(&p_sess->ftp_cmd_str, "XPWD"))
    {
      handle_pwd(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "ICNV"))	// Jiahao
    {
      str_upper(&p_sess->ftp_arg_str);
      if (str_equal_text(&p_sess->ftp_arg_str, "ON"))
      {
        tunable_enable_iconv = 1;
        vsf_cmdio_write(p_sess, FTP_CWDOK, "enable iconv().");
      }
      else if (str_equal_text(&p_sess->ftp_arg_str, "OFF"))
      {
        tunable_enable_iconv = 0;
        vsf_cmdio_write(p_sess, FTP_CWDOK, "disable iconv().");
      }
      else {
        if (tunable_enable_iconv) {
          vsf_cmdio_write(p_sess, FTP_CWDOK, "iconv() enabled.");
        }
        else
        {
          vsf_cmdio_write(p_sess, FTP_CWDOK, "iconv() disabled.");
        }
      }
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "CWD") ||
             str_equal_text(&p_sess->ftp_cmd_str, "XCWD"))
    {
      handle_cwd(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "CDUP") ||
             str_equal_text(&p_sess->ftp_cmd_str, "XCUP"))
    {
      handle_cdup(p_sess);
    }
    else if (tunable_pasv_enable &&
             !p_sess->epsv_all &&
             (str_equal_text(&p_sess->ftp_cmd_str, "PASV") ||
              str_equal_text(&p_sess->ftp_cmd_str, "P@SW")))
    {
      handle_pasv(p_sess, 0);
    }
    else if (tunable_pasv_enable &&
             str_equal_text(&p_sess->ftp_cmd_str, "EPSV"))
    {
      handle_pasv(p_sess, 1);
    }
    else if (tunable_download_enable &&
             str_equal_text(&p_sess->ftp_cmd_str, "RETR"))
    {
      handle_retr(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "NOOP"))
    {
      vsf_cmdio_write(p_sess, FTP_NOOPOK, "NOOP ok.");
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "SYST"))
    {
      vsf_cmdio_write(p_sess, FTP_SYSTOK, "UNIX Type: L8");
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "HELP"))
    {
      handle_help(p_sess);
    }
    else if (tunable_dirlist_enable &&
             str_equal_text(&p_sess->ftp_cmd_str, "LIST"))
    {
      handle_list(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "TYPE"))
    {
      handle_type(p_sess);
    }
    else if (tunable_port_enable &&
             !p_sess->epsv_all &&
             str_equal_text(&p_sess->ftp_cmd_str, "PORT"))
    {
      handle_port(p_sess);
    }
    else if (tunable_write_enable &&
             (tunable_anon_upload_enable || !p_sess->is_anonymous) &&
             str_equal_text(&p_sess->ftp_cmd_str, "STOR"))
    {
      handle_stor(p_sess);
    }
    else if (tunable_write_enable &&
             (tunable_anon_mkdir_write_enable || !p_sess->is_anonymous) &&
             (str_equal_text(&p_sess->ftp_cmd_str, "MKD") ||
              str_equal_text(&p_sess->ftp_cmd_str, "XMKD")))
    {
      handle_mkd(p_sess);
    }
    else if (tunable_write_enable &&
             (tunable_anon_other_write_enable || !p_sess->is_anonymous) &&
             (str_equal_text(&p_sess->ftp_cmd_str, "RMD") ||
              str_equal_text(&p_sess->ftp_cmd_str, "XRMD")))
    {
      handle_rmd(p_sess);
    }
    else if (tunable_write_enable &&
             (tunable_anon_other_write_enable || !p_sess->is_anonymous) &&
             str_equal_text(&p_sess->ftp_cmd_str, "DELE"))
    {
      handle_dele(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "REST"))
    {
      handle_rest(p_sess);
    }
    else if (tunable_write_enable &&
             (tunable_anon_other_write_enable || !p_sess->is_anonymous) &&
             str_equal_text(&p_sess->ftp_cmd_str, "RNFR"))
    {
      handle_rnfr(p_sess);
    }
    else if (tunable_write_enable &&
             (tunable_anon_other_write_enable || !p_sess->is_anonymous) &&
             str_equal_text(&p_sess->ftp_cmd_str, "RNTO"))
    {
      handle_rnto(p_sess);
    }
    else if (tunable_dirlist_enable &&
             str_equal_text(&p_sess->ftp_cmd_str, "NLST"))
    {
      handle_nlst(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "SIZE"))
    {
      handle_size(p_sess);
    }
    else if (!p_sess->is_anonymous &&
             str_equal_text(&p_sess->ftp_cmd_str, "SITE"))
    {
      handle_site(p_sess);
    }
    /* Note - the weird ABOR string is checking for an async ABOR arriving
     * without a SIGURG condition.
     */
    else if (str_equal_text(&p_sess->ftp_cmd_str, "ABOR") ||
             str_equal_text(&p_sess->ftp_cmd_str, "\377\364\377\362ABOR"))
    {
      vsf_cmdio_write(p_sess, FTP_ABOR_NOCONN, "No transfer to ABOR.");
    }
    else if (tunable_write_enable &&
             (tunable_anon_other_write_enable || !p_sess->is_anonymous) &&
             str_equal_text(&p_sess->ftp_cmd_str, "APPE"))
    {
      handle_appe(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "MDTM"))
    {
      handle_mdtm(p_sess);
    }
    else if (tunable_port_enable &&
             str_equal_text(&p_sess->ftp_cmd_str, "EPRT"))
    {
      handle_eprt(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "STRU"))
    {
      str_upper(&p_sess->ftp_arg_str);
      if (str_equal_text(&p_sess->ftp_arg_str, "F"))
      {
        vsf_cmdio_write(p_sess, FTP_STRUOK, "Structure set to F.");
      }
      else
      {
        vsf_cmdio_write(p_sess, FTP_BADSTRU, "Bad STRU command.");
      }
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "MODE"))
    {
      str_upper(&p_sess->ftp_arg_str);
      if (str_equal_text(&p_sess->ftp_arg_str, "S"))
      {
        vsf_cmdio_write(p_sess, FTP_MODEOK, "Mode set to S.");
      }
      else
      {
        vsf_cmdio_write(p_sess, FTP_BADMODE, "Bad MODE command.");
      }
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "STOU"))
    {
      handle_stou(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "ALLO"))
    {
      vsf_cmdio_write(p_sess, FTP_ALLOOK, "ALLO command ignored.");
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "REIN"))
    {
      vsf_cmdio_write(p_sess, FTP_COMMANDNOTIMPL, "REIN not implemented.");
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "ACCT"))
    {
      vsf_cmdio_write(p_sess, FTP_COMMANDNOTIMPL, "ACCT not implemented.");
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "SMNT"))
    {
      vsf_cmdio_write(p_sess, FTP_COMMANDNOTIMPL, "SMNT not implemented.");
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "FEAT"))
    {
      handle_feat(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "OPTS"))
    {
      handle_opts(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "STAT") &&
             str_isempty(&p_sess->ftp_arg_str))
    {
      handle_stat(p_sess);
    }
    else if (tunable_dirlist_enable &&
             str_equal_text(&p_sess->ftp_cmd_str, "STAT"))
    {
      handle_stat_file(p_sess);
    }
    else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "PBSZ"))
    {
      handle_pbsz(p_sess);
    }
    else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "PROT"))
    {
      handle_prot(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "PASV") ||
             str_equal_text(&p_sess->ftp_cmd_str, "PORT") ||
             str_equal_text(&p_sess->ftp_cmd_str, "STOR") ||
             str_equal_text(&p_sess->ftp_cmd_str, "MKD") ||
             str_equal_text(&p_sess->ftp_cmd_str, "XMKD") ||
             str_equal_text(&p_sess->ftp_cmd_str, "RMD") ||
             str_equal_text(&p_sess->ftp_cmd_str, "XRMD") ||
             str_equal_text(&p_sess->ftp_cmd_str, "DELE") ||
             str_equal_text(&p_sess->ftp_cmd_str, "RNFR") ||
             str_equal_text(&p_sess->ftp_cmd_str, "RNTO") ||
             str_equal_text(&p_sess->ftp_cmd_str, "SITE") ||
             str_equal_text(&p_sess->ftp_cmd_str, "APPE") ||
             str_equal_text(&p_sess->ftp_cmd_str, "EPSV") ||
             str_equal_text(&p_sess->ftp_cmd_str, "EPRT") ||
             str_equal_text(&p_sess->ftp_cmd_str, "RETR") ||
             str_equal_text(&p_sess->ftp_cmd_str, "LIST") ||
             str_equal_text(&p_sess->ftp_cmd_str, "NLST") ||
             str_equal_text(&p_sess->ftp_cmd_str, "STOU") ||
             str_equal_text(&p_sess->ftp_cmd_str, "ALLO") ||
             str_equal_text(&p_sess->ftp_cmd_str, "REIN") ||
             str_equal_text(&p_sess->ftp_cmd_str, "ACCT") ||
             str_equal_text(&p_sess->ftp_cmd_str, "SMNT") ||
             str_equal_text(&p_sess->ftp_cmd_str, "FEAT") ||
             str_equal_text(&p_sess->ftp_cmd_str, "OPTS") ||
             str_equal_text(&p_sess->ftp_cmd_str, "STAT") ||
             str_equal_text(&p_sess->ftp_cmd_str, "PBSZ") ||
             str_equal_text(&p_sess->ftp_cmd_str, "PROT"))
    {
      vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    }
    else
    {
      vsf_cmdio_write(p_sess, FTP_BADCMD, "Unknown command.");
    }
    if (vsf_log_entry_pending(p_sess))
    {
      vsf_log_do_log(p_sess, 0);
    }
  }

  str_free(&p_sess->full_path);
}

static void
handle_pwd(struct vsf_session* p_sess)
{
	char *tmp_str;	// Jiahao
	static struct mystr s_cwd_buf_mangle_str;
	static struct mystr s_pwd_res_str;
	
	str_getcwd(&s_cwd_buf_mangle_str);
	
// 2008.05 James. for hiding the true path. {
	static struct mystr temp_cwd_buf_mangle_str;
	char *follow_info;
	int BASE_PATH_LEN = strlen(POOL_MOUNT_ROOT);
	
	follow_info = strstr(str_getbuf(&s_cwd_buf_mangle_str), POOL_MOUNT_ROOT);
	if((int)str_getlen(&s_cwd_buf_mangle_str) > BASE_PATH_LEN){
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_p_buf = (char *)malloc(sizeof(char)*s_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_alloc_bytes-BASE_PATH_LEN);
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_len = s_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_len-BASE_PATH_LEN;
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_alloc_bytes = s_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_alloc_bytes-BASE_PATH_LEN;
		
		strcpy(temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_p_buf, follow_info+BASE_PATH_LEN);
	}
	else{
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_p_buf = (char *)malloc(sizeof(char)*2);
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_len = 1;
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_alloc_bytes = 2;
		
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_p_buf[0] = '/';
		temp_cwd_buf_mangle_str.PRIVATE_HANDS_OFF_p_buf[1] = 0;
	}
// 2008.05 James. }
	
	/* Double up any double-quotes in the pathname! */
	//str_replace_text(&s_cwd_buf_mangle_str, "\"", "\"\"");
	str_replace_text(&temp_cwd_buf_mangle_str, "\"", "\"\"");	// 2008.05 James.
	/* Enclose pathname in quotes */
	str_alloc_text(&s_pwd_res_str, "\"");
//  str_append_str(&s_pwd_res_str, &s_cwd_buf_mangle_str);	// Jiahao
	//tmp_str = local2remote(str_getbuf(&s_cwd_buf_mangle_str));
	tmp_str = local2remote(str_getbuf(&temp_cwd_buf_mangle_str));	// 2008.05 James.
	
	if (tmp_str == NULL)
		//str_append_str(&s_pwd_res_str, &s_cwd_buf_mangle_str);
		str_append_str(&s_pwd_res_str, &temp_cwd_buf_mangle_str);	// 2008.05 James.
	else {
		str_append_text(&s_pwd_res_str, tmp_str);
		vsf_sysutil_free(tmp_str);
	}
	str_append_text(&s_pwd_res_str, "\"");
	
	vsf_cmdio_write_str(p_sess, FTP_PWDOK, &s_pwd_res_str);
}

static void
handle_cwd(struct vsf_session* p_sess)
{
  int retval;
  resolve_tilde(&p_sess->full_path, p_sess);
  if (!vsf_access_check_file(&p_sess->full_path))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  retval = str_chdir(&p_sess->full_path);
  if (retval == 0)
  {
    /* Handle any messages */
    vsf_banner_dir_changed(p_sess, FTP_CWDOK);
    vsf_cmdio_write(p_sess, FTP_CWDOK, "Directory successfully changed.");
  }
  else
  {
    if (tunable_enable_iconv) {	// Jiahao
      char *tmp_str;

      tmp_str = local2remote(str_getbuf(&p_sess->full_path));
      if (tmp_str != NULL) 
      {
        str_empty(&p_sess->full_path);
        str_append_text(&p_sess->full_path, tmp_str);
        vsf_sysutil_free(tmp_str);

        retval = str_chdir(&p_sess->full_path);
        if (retval == 0)
        {
          /* Handle any messages */
          vsf_banner_dir_changed(p_sess, FTP_CWDOK);
          vsf_cmdio_write(p_sess, FTP_CWDOK, "Directory successfully changed.");
          return;
        }
      }
    }
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to change directory.");
  }
}

static void
handle_cdup(struct vsf_session* p_sess)
{
	char *ptr = rindex(str_getbuf(&p_sess->full_path), '/');

	if(ptr == NULL){
		vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to change the upper directory.");
		return;
	}

	*ptr = 0;

  handle_cwd(p_sess);
}

static int
port_active(struct vsf_session* p_sess)
{
  int ret = 0;
  if (p_sess->p_port_sockaddr != 0)
  {
    ret = 1;
    if (pasv_active(p_sess))
    {
      bug("port and pasv both active");
    }
  }
  return ret;
}

static int
pasv_active(struct vsf_session* p_sess)
{
  int ret = 0;
  if (p_sess->pasv_listen_fd != -1)
  {
    ret = 1;
    if (port_active(p_sess))
    {
      bug("pasv and port both active");
    }
  }
  return ret;
}

static void
port_cleanup(struct vsf_session* p_sess)
{
  vsf_sysutil_sockaddr_clear(&p_sess->p_port_sockaddr);
}

static void
pasv_cleanup(struct vsf_session* p_sess)
{
  if (p_sess->pasv_listen_fd != -1)
  {
    vsf_sysutil_close(p_sess->pasv_listen_fd);
    p_sess->pasv_listen_fd = -1;
  }
}

static void
handle_pasv(struct vsf_session* p_sess, int is_epsv)
{
  static struct mystr s_pasv_res_str;
  static struct vsf_sysutil_sockaddr* s_p_sockaddr;
  int bind_retries = 10;
  unsigned short the_port = 0;
  int is_ipv6 = vsf_sysutil_sockaddr_is_ipv6(p_sess->p_local_addr);
  if (is_epsv && !str_isempty(&p_sess->ftp_arg_str))
  {
    int argval;
    str_upper(&p_sess->ftp_arg_str);
    if (str_equal_text(&p_sess->ftp_arg_str, "ALL"))
    {
      p_sess->epsv_all = 1;
      vsf_cmdio_write(p_sess, FTP_EPSVALLOK, "EPSV ALL ok.");
      return;
    }
    argval = vsf_sysutil_atoi(str_getbuf(&p_sess->ftp_arg_str));
    if (argval < 1 || argval > 2 || (!is_ipv6 && argval == 2))
    {
      vsf_cmdio_write(p_sess, FTP_EPSVBAD, "Bad network protocol.");
      return;
    }
  }
  pasv_cleanup(p_sess);
  port_cleanup(p_sess);
  if (is_ipv6)
  {
    p_sess->pasv_listen_fd = vsf_sysutil_get_ipv6_sock();
  }
  else
  {
    p_sess->pasv_listen_fd = vsf_sysutil_get_ipv4_sock();
  }
  vsf_sysutil_activate_reuseaddr(p_sess->pasv_listen_fd);
  while (--bind_retries)
  {
    int retval;
    double scaled_port;
    /* IPPORT_RESERVED */
    unsigned short min_port = 1024;
    unsigned short max_port = 65535;
    if (tunable_pasv_min_port > min_port && tunable_pasv_min_port <= max_port)
    {
      min_port = tunable_pasv_min_port;
    }
    if (tunable_pasv_max_port >= min_port && tunable_pasv_max_port < max_port)
    {
      max_port = tunable_pasv_max_port;
    }
    the_port = vsf_sysutil_get_random_byte();
    the_port <<= 8;
    the_port |= vsf_sysutil_get_random_byte();
    scaled_port = (double) min_port;
    scaled_port += ((double) the_port / (double) 65536) *
                   ((double) max_port - min_port + 1);
    the_port = (unsigned short) scaled_port;
    vsf_sysutil_sockaddr_clone(&s_p_sockaddr, p_sess->p_local_addr);
    vsf_sysutil_sockaddr_set_port(s_p_sockaddr, the_port);
    retval = vsf_sysutil_bind(p_sess->pasv_listen_fd, s_p_sockaddr);
    if (!vsf_sysutil_retval_is_error(retval))
    {
      break;
    }
    if (vsf_sysutil_get_error() == kVSFSysUtilErrADDRINUSE)
    {
      continue;
    }
    die("vsf_sysutil_bind");
  }
  if (!bind_retries)
  {
    die("vsf_sysutil_bind");
  }
  vsf_sysutil_listen(p_sess->pasv_listen_fd, 1);
  if (is_epsv)
  {
    str_alloc_text(&s_pasv_res_str, "Entering Extended Passive Mode (|||");
    str_append_ulong(&s_pasv_res_str, (unsigned long) the_port);
    str_append_text(&s_pasv_res_str, "|)");
    vsf_cmdio_write_str(p_sess, FTP_EPSVOK, &s_pasv_res_str);
    return;
  }
  if (tunable_pasv_address != 0)
  {
    /* Report passive address as specified in configuration */
    if (vsf_sysutil_inet_aton(tunable_pasv_address, s_p_sockaddr) == 0)
    {
      die("invalid pasv_address");
    }
  }
  str_alloc_text(&s_pasv_res_str, "Entering Passive Mode (");
  if (!is_ipv6)
  {
    str_append_text(&s_pasv_res_str, vsf_sysutil_inet_ntop(s_p_sockaddr));
  }
  else
  {
    const void* p_v4addr = vsf_sysutil_sockaddr_ipv6_v4(s_p_sockaddr);
    if (p_v4addr)
    {
      str_append_text(&s_pasv_res_str, vsf_sysutil_inet_ntoa(p_v4addr));
    }
    else
    {
      str_append_text(&s_pasv_res_str, "0,0,0,0");
    }
  }
  str_replace_char(&s_pasv_res_str, '.', ',');
  str_append_text(&s_pasv_res_str, ",");
  str_append_ulong(&s_pasv_res_str, the_port >> 8);
  str_append_text(&s_pasv_res_str, ",");
  str_append_ulong(&s_pasv_res_str, the_port & 255);
  str_append_text(&s_pasv_res_str, ")");
  vsf_cmdio_write_str(p_sess, FTP_PASVOK, &s_pasv_res_str);
}

static void
handle_retr(struct vsf_session* p_sess)
{
  char *tmp_filename = NULL;	// Jiahao
  static struct mystr s_mark_str;
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  struct vsf_transfer_ret trans_ret;
  int remote_fd;
  int opened_file;
  int is_ascii = 0;

  filesize_t offset = p_sess->restart_pos;
  p_sess->restart_pos = 0;
  if (!data_transfer_checks_ok(p_sess))
  {
    return;
  }
  if (p_sess->is_ascii && offset != 0)
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "No support for resume of ASCII transfer.");
    return;
  }
  resolve_tilde(&p_sess->full_path, p_sess);
  vsf_log_start_entry(p_sess, kVSFLogEntryDownload);
  str_copy(&p_sess->log_str, &p_sess->full_path);
  prepend_path_to_filename(&p_sess->log_str);
  if (!vsf_access_check_file(&p_sess->full_path))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  opened_file = str_open(&p_sess->full_path, kVSFSysStrOpenReadOnly);
  if (vsf_sysutil_retval_is_error(opened_file))
  {
//    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");	// Jiahao
//    return;
    if (tunable_enable_iconv) {
      char *tmp_str;

      tmp_str = local2remote(str_getbuf(&p_sess->full_path));
      if (tmp_str != NULL) {
        str_empty(&p_sess->full_path);
        str_append_text(&p_sess->full_path, tmp_str);
        vsf_sysutil_free(tmp_str);
        opened_file = str_open(&p_sess->full_path, kVSFSysStrOpenReadOnly);
        if (vsf_sysutil_retval_is_error(opened_file)) {
          vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");
          return;
        }
      }
      else {
        vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");
        return;
      }
    }
    else {
      vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");
      return;
    }
  }

  /* Lock file if required */
  if (tunable_lock_upload_files)
  {
    vsf_sysutil_lock_file_read(opened_file);
  }
  vsf_sysutil_fstat(opened_file, &s_p_statbuf);

  /* No games please */
  if (!vsf_sysutil_statbuf_is_regfile(s_p_statbuf))
  {
    /* Note - pretend open failed */
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");
    goto file_close_out;
  }

  /* Optionally, we'll be paranoid and only serve publicly readable stuff */
  if (p_sess->is_anonymous && tunable_anon_world_readable_only &&
      !vsf_sysutil_statbuf_is_readable_other(s_p_statbuf))
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");
    goto file_close_out;
  }

  /* Set the download offset (from REST) if any */
  if (offset != 0)
  {
    vsf_sysutil_lseek_to(opened_file, offset);
  }
  str_alloc_text(&s_mark_str, "Opening ");

  if (tunable_ascii_download_enable && p_sess->is_ascii)
  {
    str_append_text(&s_mark_str, "ASCII");
    is_ascii = 1;
  }
  else
  {
    str_append_text(&s_mark_str, "BINARY");
  }
  str_append_text(&s_mark_str, " mode data connection for ");
//  str_append_str(&s_mark_str, &p_sess->full_path);	// Jiahao
	tmp_filename = local2remote(str_getbuf(&p_sess->full_path));
  if (tmp_filename == NULL)
    str_append_str(&s_mark_str, &p_sess->full_path);
  else {
    str_append_text(&s_mark_str, tmp_filename);
    vsf_sysutil_free(tmp_filename);
  }

  str_append_text(&s_mark_str, " (");
  str_append_filesize_t(&s_mark_str,
                        vsf_sysutil_statbuf_get_size(s_p_statbuf));
  str_append_text(&s_mark_str, " bytes).");
  remote_fd = get_remote_transfer_fd(p_sess, str_getbuf(&s_mark_str));
  if (vsf_sysutil_retval_is_error(remote_fd))
  {
    goto port_pasv_cleanup_out;
  }
  trans_ret = vsf_ftpdataio_transfer_file(p_sess, remote_fd,
                                          opened_file, 0, is_ascii);
  vsf_ftpdataio_dispose_transfer_fd(p_sess);
  p_sess->transfer_size = trans_ret.transferred;
  /* Log _after_ the blocking dispose call, so we get transfer times right */
  if (trans_ret.retval == 0)
  {
    vsf_log_do_log(p_sess, 1);
  }

  /* Emit status message _after_ blocking dispose call to avoid buggy FTP
   * clients truncating the transfer.
   */
  if (trans_ret.retval == -1)
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDFILE, "Failure reading local file.");
  }
  else if (trans_ret.retval == -2)
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDNET, "Failure writing network stream.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_TRANSFEROK, "File send OK.");
  }
  check_abor(p_sess);
port_pasv_cleanup_out:
  port_cleanup(p_sess);
  pasv_cleanup(p_sess);
file_close_out:
  vsf_sysutil_close(opened_file);
}

static void
handle_list(struct vsf_session* p_sess)
{
  handle_dir_common(p_sess, 1, 0);
}

static void
handle_dir_common(struct vsf_session* p_sess, int full_details, int stat_cmd)
{
  static struct mystr s_option_str;
  static struct mystr s_filter_str;
  static struct mystr s_dir_name_str;
  static struct vsf_sysutil_statbuf* s_p_dirstat;
  int dir_allow_read = 1;
  struct vsf_sysutil_dir* p_dir = 0;
  int retval = 0;
  int use_control = 0;
  
  str_empty(&s_option_str);
  str_empty(&s_filter_str);
  /* By default open the current directory */
  str_alloc_text(&s_dir_name_str, ".");
  if (!stat_cmd && !data_transfer_checks_ok(p_sess))
  {
    return;
  }
  /* Do we have an option? Going to be strict here - the option must come
   * first. e.g. "ls -a .." fine, "ls .. -a" not fine
   */
  if (!str_isempty(&p_sess->ftp_arg_str) &&
      str_get_char_at(&p_sess->ftp_arg_str, 0) == '-')
  {
    /* Chop off the '-' */
    str_mid_to_end(&p_sess->ftp_arg_str, &s_option_str, 1);
    /* A space will separate options from filter (if any) */
    str_split_char(&s_option_str, &s_filter_str, ' ');
  }
  else
  {
    /* The argument, if any, is just a filter */
    //str_copy(&s_filter_str, &p_sess->ftp_arg_str);
    str_append_str(&s_filter_str, &p_sess->full_path);
  }

  if (!str_isempty(&s_filter_str))
  {
    resolve_tilde(&s_filter_str, p_sess);
    if (!vsf_access_check_file(&s_filter_str))
    {
      vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
      return;
    }
    /* First check - is it an outright directory, as in "ls /pub" */
    p_dir = str_opendir(&s_filter_str);
    if (p_dir != 0)
    {
      /* Listing a directory! */
      str_copy(&s_dir_name_str, &s_filter_str);
      //str_free(&s_filter_str);
    }
    else
    {
      struct str_locate_result locate_result = str_locate_char(&s_filter_str, '/');
      if (locate_result.found)
      {
        /* Includes a path! Reverse scan for / in the arg, to get the
         * base directory and filter (if any)
         */
        str_copy(&s_dir_name_str, &s_filter_str);
        str_split_char_reverse(&s_dir_name_str, &s_filter_str, '/');
        /* If we have e.g. "ls /.message", we just ripped off the leading
         * slash because it is the only one!
         */
        if (str_isempty(&s_dir_name_str))
        {
          str_alloc_text(&s_dir_name_str, "/");
        }
      }
    }
  }

  if (p_dir == 0)
  {
    /* NOTE - failure check done below, it's not forgotten */
    p_dir = str_opendir(&s_dir_name_str);
  }
  /* Fine, do it */
  if (stat_cmd)
  {
    use_control = 1;
    str_append_char(&s_option_str, 'a');
    vsf_cmdio_write_hyphen(p_sess, FTP_STATFILE_OK, "Status follows:");
  }
  else
  {
    int remote_fd = get_remote_transfer_fd(p_sess, "Here comes the directory listing.");
    if (vsf_sysutil_retval_is_error(remote_fd))
    {
      goto dir_close_out;
    }
  }

  if (p_sess->is_anonymous && p_dir && tunable_anon_world_readable_only)
  {
    vsf_sysutil_dir_stat(p_dir, &s_p_dirstat);
    if (!vsf_sysutil_statbuf_is_readable_other(s_p_dirstat))
    {
      dir_allow_read = 0;
    }
  }
	
  if (p_dir != 0 && dir_allow_read)
  {
      retval = vsf_ftpdataio_transfer_dir(p_sess, use_control, p_dir,
                                          &s_dir_name_str, &s_option_str,
                                          &s_filter_str, full_details);
  }

  if (stat_cmd)
  {
    vsf_cmdio_write(p_sess, FTP_STATFILE_OK, "End of status");
  }
  else
  {
    vsf_ftpdataio_dispose_transfer_fd(p_sess);
  }
  
  if (p_dir == 0 || !dir_allow_read)
  {
    vsf_cmdio_write(p_sess, FTP_TRANSFEROK, "Transfer done (but failed to open directory).");
  }
  else if (retval == 0)
  {
    vsf_cmdio_write(p_sess, FTP_TRANSFEROK, "Directory send OK.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDNET, "Failure writing network stream.");
  }

  check_abor(p_sess);
dir_close_out:

  if (p_dir)
  {
    vsf_sysutil_closedir(p_dir);
  }

  if (!stat_cmd)
  {
    port_cleanup(p_sess);
    pasv_cleanup(p_sess);
  }
}

static void
handle_type(struct vsf_session* p_sess)
{
  str_upper(&p_sess->ftp_arg_str);
  if (str_equal_text(&p_sess->ftp_arg_str, "I") ||
      str_equal_text(&p_sess->ftp_arg_str, "L8") ||
      str_equal_text(&p_sess->ftp_arg_str, "L 8"))
  {
    p_sess->is_ascii = 0;
    vsf_cmdio_write(p_sess, FTP_TYPEOK, "Switching to Binary mode.");
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "A") ||
           str_equal_text(&p_sess->ftp_arg_str, "A N"))
  {
    p_sess->is_ascii = 1;
    vsf_cmdio_write(p_sess, FTP_TYPEOK, "Switching to ASCII mode.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_BADCMD, "Unrecognised TYPE command.");
  }
}

static void
handle_port(struct vsf_session* p_sess)
{
  unsigned short the_port;
  unsigned char vals[6];
  const unsigned char* p_raw;
  pasv_cleanup(p_sess);
  port_cleanup(p_sess);
  p_raw = vsf_sysutil_parse_uchar_string_sep(&p_sess->ftp_arg_str, ',', vals,
                                             sizeof(vals));
  if (p_raw == 0)
  {
    vsf_cmdio_write(p_sess, FTP_BADCMD, "Illegal PORT command.");
    return;
  }
  the_port = vals[4] << 8;
  the_port |= vals[5];
  vsf_sysutil_sockaddr_clone(&p_sess->p_port_sockaddr, p_sess->p_local_addr);
  vsf_sysutil_sockaddr_set_ipv4addr(p_sess->p_port_sockaddr, vals);
  vsf_sysutil_sockaddr_set_port(p_sess->p_port_sockaddr, the_port);
  /* SECURITY:
   * 1) Reject requests not connecting to the control socket IP
   * 2) Reject connects to privileged ports
   */
  if (!tunable_port_promiscuous)
  {
    if (!vsf_sysutil_sockaddr_addr_equal(p_sess->p_remote_addr,
                                         p_sess->p_port_sockaddr) ||
        vsf_sysutil_is_port_reserved(the_port))
    {
      vsf_cmdio_write(p_sess, FTP_BADCMD, "Illegal PORT command.");
      port_cleanup(p_sess);
      return;
    }
  }
  vsf_cmdio_write(p_sess, FTP_PORTOK,
                  "PORT command successful. Consider using PASV.");
}

static void
handle_stor(struct vsf_session* p_sess)
{
  handle_upload_common(p_sess, 0, 0);
}

static void
handle_upload_common(struct vsf_session* p_sess, int is_append, int is_unique)
{
  static struct mystr s_filename;
  struct mystr* p_filename;
  struct vsf_transfer_ret trans_ret;
  int new_file_fd;
  int remote_fd;

  filesize_t offset = p_sess->restart_pos;
  p_sess->restart_pos = 0;
  if (!data_transfer_checks_ok(p_sess))
  {
    return;
  }
  resolve_tilde(&p_sess->full_path, p_sess);
  p_filename = &p_sess->full_path;
  if (is_unique)
  {
    get_unique_filename(&s_filename, p_filename);
    p_filename = &s_filename;
  }
  vsf_log_start_entry(p_sess, kVSFLogEntryUpload);
  str_copy(&p_sess->log_str, &p_sess->full_path);
  prepend_path_to_filename(&p_sess->log_str);
  if (!vsf_access_check_file(p_filename))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  
  if (p_sess->write_enable==0)	// Jiahao
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  
  /* NOTE - actual file permissions will be governed by the tunable umask */
  /* XXX - do we care about race between create and chown() of anonymous
   * upload?
   */
  if (is_unique || (p_sess->is_anonymous && !tunable_anon_other_write_enable))
  {
	  new_file_fd = str_create(p_filename);
  }
  else
  {
    /* For non-anonymous, allow open() to overwrite or append existing files */
    if (!is_append && offset == 0)
    {
		new_file_fd = str_create_overwrite(p_filename);

    }
    else
    {
		new_file_fd = str_create_append(p_filename);
    }
  }
  
  if (vsf_sysutil_retval_is_error(new_file_fd))
  {
    vsf_cmdio_write(p_sess, FTP_UPLOADFAIL, "Could not create file.");
    return;
  }
  
  /* Are we required to chown() this file for security? */
  if (p_sess->is_anonymous && tunable_chown_uploads)
  {
    vsf_sysutil_fchmod(new_file_fd, 0600);
    if (tunable_one_process_model)
    {
      vsf_one_process_chown_upload(p_sess, new_file_fd);
    }
    else
    {
      vsf_two_process_chown_upload(p_sess, new_file_fd);
    }
  }
  
  /* Are we required to lock this file? */
  if (tunable_lock_upload_files)
  {
    vsf_sysutil_lock_file_write(new_file_fd);
  }
  
  if (!is_append && offset != 0)
  {
    /* XXX - warning, allows seek past end of file! Check for seek > size? */
    vsf_sysutil_lseek_to(new_file_fd, offset);
  }
  
  if (is_unique)
  {
    struct mystr resp_str = INIT_MYSTR;
    str_alloc_text(&resp_str, "FILE: ");
    str_append_str(&resp_str, p_filename);
    vsf_cmdio_write_str(p_sess, FTP_DATACONN, &resp_str);
    remote_fd = get_remote_transfer_fd(p_sess, str_getbuf(&resp_str));
    str_free(&resp_str);
  }
  else
  {
    remote_fd = get_remote_transfer_fd(p_sess, "Ok to send data.");
  }
  
  if (vsf_sysutil_retval_is_error(remote_fd))
  {
    goto port_pasv_cleanup_out;
  }
  
  if (tunable_ascii_upload_enable && p_sess->is_ascii)
  {
    trans_ret = vsf_ftpdataio_transfer_file(p_sess, remote_fd,
                                            new_file_fd, 1, 1);
  }
  else
  {
    trans_ret = vsf_ftpdataio_transfer_file(p_sess, remote_fd,
                                            new_file_fd, 1, 0);
  }
  vsf_ftpdataio_dispose_transfer_fd(p_sess);
  p_sess->transfer_size = trans_ret.transferred;
  /* XXX - handle failure, delete file? */
  if (trans_ret.retval == 0)
  {
    vsf_log_do_log(p_sess, 1);
  }
  
  if (trans_ret.retval == -1)
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDFILE, "Failure writing to local file.");
  }
  else if (trans_ret.retval == -2)
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDNET, "Failure reading network stream.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_TRANSFEROK, "File receive OK.");
  }
  check_abor(p_sess);
port_pasv_cleanup_out:
  port_cleanup(p_sess);
  pasv_cleanup(p_sess);
  vsf_sysutil_close(new_file_fd);
}

static void
handle_mkd(struct vsf_session* p_sess)
{
  char *tmp_dirname=NULL;	// Jiahao
  int retval = -1;
  resolve_tilde(&p_sess->full_path, p_sess);
  vsf_log_start_entry(p_sess, kVSFLogEntryMkdir);
  str_copy(&p_sess->log_str, &p_sess->full_path);
  prepend_path_to_filename(&p_sess->log_str);

  if (!vsf_access_check_file(&p_sess->full_path))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  if (p_sess->write_enable==0)	// Jiahao
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }

  /* NOTE! Actual permissions will be governed by the tunable umask */
// 2007.08 James {
  if(p_sess->layer != SHARE_LAYER || !p_sess->is_anonymous){
    retval = str_mkdir(&p_sess->full_path, 0777);
  }
  else{
    char *mount_path, *new_folder, backup;

    mount_path = (char *)str_getbuf(&p_sess->full_path);

    new_folder = rindex(str_getbuf(&p_sess->full_path), '/');
    if(new_folder == NULL){
      vsf_cmdio_write(p_sess, FTP_FILEFAIL, "1. Create directory operation failed.");
      return;
    }

    backup = *new_folder;
    *new_folder = 0;

	  retval = add_folder(NULL, mount_path, new_folder+1);

    *new_folder = backup;
  }
// 2007.08 James }

  if (retval != 0)
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "2. Create directory operation failed.");
    return;
  }
  vsf_log_do_log(p_sess, 1);
  {
    static struct mystr s_mkd_res;
    static struct mystr s_tmp_str;
    str_copy(&s_tmp_str, &p_sess->full_path);
    prepend_path_to_filename(&s_tmp_str);
    /* Double up double quotes */
    str_replace_text(&s_tmp_str, "\"", "\"\"");
    /* Build result string */
    str_alloc_text(&s_mkd_res, "\"");

    tmp_dirname = local2remote(str_getbuf(&s_tmp_str));
    if (tmp_dirname == NULL)
      str_append_str(&s_mkd_res, &s_tmp_str);
    else {
      str_append_text(&s_mkd_res, tmp_dirname);
      vsf_sysutil_free(tmp_dirname);
    }

    char *ptr = strstr(str_getbuf(&s_mkd_res), POOL_MOUNT_ROOT);
    if(ptr != NULL)
      str_alloc_text(&s_mkd_res, ptr+strlen(POOL_MOUNT_ROOT));

    str_append_text(&s_mkd_res, "\" created");
    vsf_cmdio_write_str(p_sess, FTP_MKDIROK, &s_mkd_res);
  }
}

static void
handle_rmd(struct vsf_session* p_sess)
{
  int retval = -1;

  resolve_tilde(&p_sess->full_path, p_sess);
  vsf_log_start_entry(p_sess, kVSFLogEntryRmdir);
  str_copy(&p_sess->log_str, &p_sess->full_path);
  prepend_path_to_filename(&p_sess->log_str);
  if (!vsf_access_check_file(&p_sess->full_path))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  if (p_sess->write_enable==0)	// Jiahao
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }

// 2007.08 James {
  if(p_sess->layer != SHARE_LAYER || !p_sess->is_anonymous){
    retval = str_rmdir(&p_sess->full_path);
  }
  else{
    char *mount_path, *folder, backup;

    mount_path = (char *)str_getbuf(&p_sess->full_path);

    folder = rindex(mount_path, '/');
    if(folder == NULL){
      vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Remove directory operation failed (001).");
      return;
    }

    backup = *folder;
    *folder = 0;

    retval = del_folder(mount_path, folder+1);

    *folder = backup;
  }
// 2007.08 James }

  if (retval != 0)
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL,
                    "Remove directory operation failed (002).");
  }
  else
  {
    vsf_log_do_log(p_sess, 1);
    vsf_cmdio_write(p_sess, FTP_RMDIROK,
                    "Remove directory operation successful.");
  }
}

static void
handle_dele(struct vsf_session* p_sess)
{
  int retval;
  resolve_tilde(&p_sess->full_path, p_sess);
  vsf_log_start_entry(p_sess, kVSFLogEntryDelete);
  str_copy(&p_sess->log_str, &p_sess->full_path);
  prepend_path_to_filename(&p_sess->log_str);
  
  if (!vsf_access_check_file(&p_sess->full_path))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  
  if (p_sess->write_enable==0)	// Jiahao
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  	retval = str_unlink(&p_sess->full_path);
  if (retval != 0)
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Delete operation failed.");
  }
  else
  {
    vsf_log_do_log(p_sess, 1);
    vsf_cmdio_write(p_sess, FTP_DELEOK, "Delete operation successful.");
  }
}

static void
handle_rest(struct vsf_session* p_sess)
{
  static struct mystr s_rest_str;
  filesize_t val = str_a_to_filesize_t(&p_sess->ftp_arg_str);
  if (val < 0)
  {
    val = 0;
  }
  p_sess->restart_pos = val;
  str_alloc_text(&s_rest_str, "Restart position accepted (");
  str_append_filesize_t(&s_rest_str, val);
  str_append_text(&s_rest_str, ").");
  vsf_cmdio_write_str(p_sess, FTP_RESTOK, &s_rest_str);
}

static void
handle_rnfr(struct vsf_session* p_sess)
{
  static struct vsf_sysutil_statbuf* p_statbuf;
  int retval;

  /* Clear old value */
  str_free(&p_sess->rnfr_filename_str);
  resolve_tilde(&p_sess->full_path, p_sess);

// 2007.08 James {
  int len;

  len = str_getlen(&p_sess->full_path);

  if (!vsf_access_check_file(&p_sess->full_path))
// 2007.08 James }
  {
    vsf_log_start_entry(p_sess, kVSFLogEntryRename);
    str_copy(&p_sess->log_str, &p_sess->full_path);
    prepend_path_to_filename(&p_sess->log_str);
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  if (p_sess->write_enable==0)	// Jiahao
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  /* Does it exist? */
// 2007.08 James {
  retval = str_stat(&p_sess->full_path, &p_statbuf);
// 2007.08 James }
  if (retval == 0)
  {
    /* Yes */
    str_copy(&p_sess->rnfr_filename_str, &p_sess->full_path);
    vsf_cmdio_write(p_sess, FTP_RNFROK, "Ready for RNTO.");
  }
  else
  {
    vsf_log_start_entry(p_sess, kVSFLogEntryRename);
    str_copy(&p_sess->log_str, &p_sess->full_path);
    prepend_path_to_filename(&p_sess->log_str);
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "RNFR command failed.");
  }
}

static void
handle_rnto(struct vsf_session* p_sess)
{
  static struct mystr s_tmp_str;
  int retval;

  /* If we didn't get a RNFR, throw a wobbly */
  if (str_isempty(&p_sess->rnfr_filename_str))
  {
    vsf_cmdio_write(p_sess, FTP_NEEDRNFR,
                    "RNFR required first.");
    return;
  }
  resolve_tilde(&p_sess->full_path, p_sess);
  vsf_log_start_entry(p_sess, kVSFLogEntryRename);
  str_copy(&p_sess->log_str, &p_sess->rnfr_filename_str);
  prepend_path_to_filename(&p_sess->log_str);
  str_append_char(&p_sess->log_str, ' ');
  str_copy(&s_tmp_str, &p_sess->full_path);
  prepend_path_to_filename(&s_tmp_str);
  str_append_str(&p_sess->log_str, &s_tmp_str);
  if (!vsf_access_check_file(&p_sess->full_path))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  if (p_sess->write_enable==0)	// Jiahao
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }

  /* NOTE - might overwrite destination file. Not a concern because the same
   * could be accomplished with DELE.
   */
// 2007.08 James {
  if(p_sess->layer != SHARE_LAYER || !p_sess->is_anonymous){
    retval = str_rename(&p_sess->rnfr_filename_str, &p_sess->full_path);
  }
  else{
    char *mount_path, *old_folder, backup;
    char *new_folder;

    mount_path = (char *)str_getbuf(&p_sess->rnfr_filename_str);

    new_folder = rindex(str_getbuf(&p_sess->full_path), '/');
    if(new_folder == NULL){
      str_free(&p_sess->rnfr_filename_str);
      vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Rename failed 1.");
      return;
    }

    old_folder = rindex(mount_path, '/');
    if(old_folder == NULL){
      str_free(&p_sess->rnfr_filename_str);
      vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Rename failed 2.");
      return;
    }

    backup = *old_folder;
    *old_folder = 0;

    retval = mod_folder(mount_path, old_folder+1, new_folder+1);

    *old_folder = backup;
  }
// 2007.08 James }

  /* Clear the RNFR filename; start the two stage process again! */
  str_free(&p_sess->rnfr_filename_str);
  if (retval == 0)
  {
    vsf_log_do_log(p_sess, 1);
    vsf_cmdio_write(p_sess, FTP_RENAMEOK, "Rename successful.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Rename failed.");
  }
}

static void
handle_nlst(struct vsf_session* p_sess)
{
  handle_dir_common(p_sess, 0, 0);
}

static void
prepend_path_to_filename(struct mystr* p_str)
{
  static struct mystr s_tmp_str;
  /* Only prepend current working directory if the incoming filename is
   * relative
   */
  str_empty(&s_tmp_str);
  if (str_isempty(p_str) || str_get_char_at(p_str, 0) != '/')
  {
    str_getcwd(&s_tmp_str);
    /* Careful to not emit // if we are in directory / (common with chroot) */
    if (str_isempty(&s_tmp_str) ||
        str_get_char_at(&s_tmp_str, str_getlen(&s_tmp_str) - 1) != '/')
    {
      str_append_char(&s_tmp_str, '/');
    }
  }
  str_append_str(&s_tmp_str, p_str);
  str_copy(p_str, &s_tmp_str);
}


static void
handle_sigurg(void* p_private)
{
  struct mystr async_cmd_str = INIT_MYSTR;
  struct mystr async_arg_str = INIT_MYSTR;
  struct mystr real_cmd_str = INIT_MYSTR;
  unsigned int len;
  struct vsf_session* p_sess = (struct vsf_session*) p_private;
  /* Did stupid client sent something OOB without a data connection? */
  if (p_sess->data_fd == -1)
  {
    return;
  }
  /* Get the async command - blocks (use data timeout alarm) */
  vsf_cmdio_get_cmd_and_arg(p_sess, &async_cmd_str, &async_arg_str, 0);
  /* Chop off first four characters; they are telnet characters. The client
   * should have sent the first two normally and the second two as urgent
   * data.
   */
  len = str_getlen(&async_cmd_str);
  if (len >= 4)
  {
    str_right(&async_cmd_str, &real_cmd_str, len - 4);
  }
  if (str_equal_text(&real_cmd_str, "ABOR"))
  {
    p_sess->abor_received = 1;
    /* This is failok because of a small race condition; the SIGURG might
     * be raised after the data socket is closed, but before data_fd is
     * set to -1.
     */
    vsf_sysutil_shutdown_failok(p_sess->data_fd);
  }
  else
  {
    /* Sorry! */
    vsf_cmdio_write(p_sess, FTP_BADCMD, "Unknown command.");
  }
  str_free(&async_cmd_str);
  str_free(&async_arg_str);
  str_free(&real_cmd_str);
}

static int
get_remote_transfer_fd(struct vsf_session* p_sess, const char* p_status_msg)
{
  int remote_fd;
  if (!pasv_active(p_sess) && !port_active(p_sess))
  {
    bug("neither PORT nor PASV active in get_remote_transfer_fd");
  }
  p_sess->abor_received = 0;
  if (pasv_active(p_sess))
  {
    remote_fd = vsf_ftpdataio_get_pasv_fd(p_sess);
  }
  else
  {
    remote_fd = vsf_ftpdataio_get_port_fd(p_sess);
  }
  if (vsf_sysutil_retval_is_error(remote_fd))
  {
    return remote_fd;
  }
  vsf_cmdio_write(p_sess, FTP_DATACONN, p_status_msg);
  if (vsf_ftpdataio_post_mark_connect(p_sess) != 1)
  {
    vsf_ftpdataio_dispose_transfer_fd(p_sess);
    return -1;
  }
  return remote_fd;
}

static void
check_abor(struct vsf_session* p_sess)
{
  /* If the client sent ABOR, respond to it here */
  if (p_sess->abor_received)
  {
    p_sess->abor_received = 0;
    vsf_cmdio_write(p_sess, FTP_ABOROK, "ABOR successful.");
  }
}

static void
handle_size(struct vsf_session* p_sess)
{
  /* Note - in ASCII mode, are supposed to return the size after taking into
   * account ASCII linefeed conversions. At least this is what wu-ftpd does in
   * version 2.6.1. Proftpd-1.2.0pre fails to do this.
   * I will not do it because it is a potential I/O DoS.
   */
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  int retval;
  resolve_tilde(&p_sess->ftp_arg_str, p_sess);
  if (!vsf_access_check_file(&p_sess->ftp_arg_str))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  retval = str_stat(&p_sess->ftp_arg_str, &s_p_statbuf);
  if (retval != 0 || !vsf_sysutil_statbuf_is_regfile(s_p_statbuf))
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Could not get file size.");
  }
  else
  {
    static struct mystr s_size_res_str;
    str_alloc_filesize_t(&s_size_res_str,
                         vsf_sysutil_statbuf_get_size(s_p_statbuf));
    vsf_cmdio_write_str(p_sess, FTP_SIZEOK, &s_size_res_str);
  }
}

static void
handle_site(struct vsf_session* p_sess)
{
  static struct mystr s_site_args_str;
  /* What SITE sub-command is it? */
  str_split_char(&p_sess->ftp_arg_str, &s_site_args_str, ' ');
  str_upper(&p_sess->ftp_arg_str);
  if (tunable_write_enable &&
      tunable_chmod_enable &&
      str_equal_text(&p_sess->ftp_arg_str, "CHMOD"))
  {
    handle_site_chmod(p_sess, &s_site_args_str);
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "UMASK"))
  {
    handle_site_umask(p_sess, &s_site_args_str);
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "HELP"))
  {
    vsf_cmdio_write(p_sess, FTP_SITEHELP, "CHMOD UMASK HELP");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_BADCMD, "Unknown SITE command.");
  }
}

static void
handle_site_chmod(struct vsf_session* p_sess, struct mystr* p_arg_str)
{
  static struct mystr s_chmod_file_str;
  unsigned int perms;
  int retval;
  if (str_isempty(p_arg_str))
  {
    vsf_cmdio_write(p_sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
    return;
  }
  str_split_char(p_arg_str, &s_chmod_file_str, ' ');
  if (str_isempty(&s_chmod_file_str))
  {
    vsf_cmdio_write(p_sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
    return;
  }
  resolve_tilde(&s_chmod_file_str, p_sess);
  vsf_log_start_entry(p_sess, kVSFLogEntryChmod);
  str_copy(&p_sess->log_str, &s_chmod_file_str);
  prepend_path_to_filename(&p_sess->log_str);
  str_append_char(&p_sess->log_str, ' ');
  str_append_str(&p_sess->log_str, p_arg_str);
  if (!vsf_access_check_file(&s_chmod_file_str))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  if (p_sess->write_enable==0)	// Jiahao
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  /* Don't worry - our chmod() implementation only allows 0 - 0777 */
  perms = str_octal_to_uint(p_arg_str);
  retval = str_chmod(&s_chmod_file_str, perms);
  if (vsf_sysutil_retval_is_error(retval))
  {
    vsf_cmdio_write(p_sess, FTP_FILEFAIL, "SITE CHMOD command failed.");
  }
  else
  {
    vsf_log_do_log(p_sess, 1);
    vsf_cmdio_write(p_sess, FTP_CHMODOK, "SITE CHMOD command ok.");
  }
}

static void
handle_site_umask(struct vsf_session* p_sess, struct mystr* p_arg_str)
{
  static struct mystr s_umask_resp_str;
  if (str_isempty(p_arg_str))
  {
    /* Empty arg => report current umask */
    str_alloc_text(&s_umask_resp_str, "Your current UMASK is ");
    str_append_text(&s_umask_resp_str,
                    vsf_sysutil_uint_to_octal(vsf_sysutil_get_umask()));
  }
  else
  {
// 2008.05 James. {
    /* Set current umask */
    /*unsigned int new_umask = str_octal_to_uint(p_arg_str);
    vsf_sysutil_set_umask(new_umask);
    str_alloc_text(&s_umask_resp_str, "UMASK set to ");
    str_append_text(&s_umask_resp_str,
                    vsf_sysutil_uint_to_octal(vsf_sysutil_get_umask()));//*/
		str_alloc_text(&s_umask_resp_str, "Don't allow to operate UMASK!");
// 2008.05 James. }
  }
  vsf_cmdio_write_str(p_sess, FTP_UMASKOK, &s_umask_resp_str);
}

static void
handle_appe(struct vsf_session* p_sess)
{
  handle_upload_common(p_sess, 1, 0);
}

static void
handle_mdtm(struct vsf_session* p_sess)
{
  static struct mystr s_filename_str;
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  int do_write = 0;
  long modtime = 0;
  struct str_locate_result loc = str_locate_char(&p_sess->ftp_arg_str, ' ');
  int retval = str_stat(&p_sess->ftp_arg_str, &s_p_statbuf);
  if (tunable_mdtm_write && retval != 0 && loc.found &&
      vsf_sysutil_isdigit(str_get_char_at(&p_sess->ftp_arg_str, 0)))
  {
    if (loc.index == 8 || loc.index == 14 ||
        (loc.index > 15 && str_get_char_at(&p_sess->ftp_arg_str, 14) == '.'))
    {
      do_write = 1;
    }
  }
  if (do_write != 0)
  {
    str_split_char(&p_sess->ftp_arg_str, &s_filename_str, ' ');
    modtime = vsf_sysutil_parse_time(str_getbuf(&p_sess->ftp_arg_str));
    str_copy(&p_sess->ftp_arg_str, &s_filename_str);
  }
  resolve_tilde(&p_sess->ftp_arg_str, p_sess);
  if (!vsf_access_check_file(&p_sess->ftp_arg_str))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  if (do_write && tunable_write_enable &&
      (tunable_anon_other_write_enable || !p_sess->is_anonymous))
  {
    if (p_sess->write_enable==0)	// Jiahao
    {
      vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
      return;
    }
    retval = str_stat(&p_sess->ftp_arg_str, &s_p_statbuf);
    if (retval != 0 || !vsf_sysutil_statbuf_is_regfile(s_p_statbuf))
    {
      vsf_cmdio_write(p_sess, FTP_FILEFAIL,
                      "Could not set file modification time.");
    }
    else
    {
      retval = vsf_sysutil_setmodtime(
        str_getbuf(&p_sess->ftp_arg_str), modtime, tunable_use_localtime);
      if (retval != 0)
      {
        vsf_cmdio_write(p_sess, FTP_FILEFAIL,
                        "Could not set file modification time.");
      }
      else
      {
        vsf_cmdio_write(p_sess, FTP_MDTMOK,
                        "File modification time set.");
      }
    }
  }
  else
  {
    if (retval != 0 || !vsf_sysutil_statbuf_is_regfile(s_p_statbuf))
    {
      vsf_cmdio_write(p_sess, FTP_FILEFAIL,
                      "Could not get file modification time.");
    }
    else
    {
      static struct mystr s_mdtm_res_str;
      str_alloc_text(&s_mdtm_res_str,
                     vsf_sysutil_statbuf_get_numeric_date(
                       s_p_statbuf, tunable_use_localtime));
      vsf_cmdio_write_str(p_sess, FTP_MDTMOK, &s_mdtm_res_str);
    }
  }
}

static void
handle_eprt(struct vsf_session* p_sess)
{
  static struct mystr s_part1_str;
  static struct mystr s_part2_str;
  int proto;
  int port;
  const unsigned char* p_raw_addr;
  int is_ipv6 = vsf_sysutil_sockaddr_is_ipv6(p_sess->p_local_addr);
  
  port_cleanup(p_sess);
  pasv_cleanup(p_sess);
  str_copy(&s_part1_str, &p_sess->ftp_arg_str);
  str_split_char(&s_part1_str, &s_part2_str, '|');
  if (!str_isempty(&s_part1_str))
  {
    goto bad_eprt;
  }
  /* Split out the protocol and check it */
  str_split_char(&s_part2_str, &s_part1_str, '|');
  proto = str_atoi(&s_part2_str);
  if (proto < 1 || proto > 2 || (!is_ipv6 && proto == 2))
  {
    vsf_cmdio_write(p_sess, FTP_BADCMD, "Bad EPRT protocol.");
    return;
  }
  /* Split out address and parse it */
  str_split_char(&s_part1_str, &s_part2_str, '|');
  if (proto == 2)
  {
    p_raw_addr = vsf_sysutil_parse_ipv6(&s_part1_str);
  }
  else
  {
    p_raw_addr = vsf_sysutil_parse_ipv4(&s_part1_str);
  }
  if (!p_raw_addr)
  {
    goto bad_eprt;
  }
  /* Split out port and parse it */
  str_split_char(&s_part2_str, &s_part1_str, '|');
  if (!str_isempty(&s_part1_str) || str_isempty(&s_part2_str))
  {
    goto bad_eprt;
  }
  port = str_atoi(&s_part2_str);
  if (port < 0 || port > 65535)
  {
    goto bad_eprt;
  }
  vsf_sysutil_sockaddr_clone(&p_sess->p_port_sockaddr, p_sess->p_local_addr);
  if (proto == 2)
  {
    vsf_sysutil_sockaddr_set_ipv6addr(p_sess->p_port_sockaddr, p_raw_addr);
  }
  else
  {
    vsf_sysutil_sockaddr_set_ipv4addr(p_sess->p_port_sockaddr, p_raw_addr);
  }
  vsf_sysutil_sockaddr_set_port(p_sess->p_port_sockaddr, (unsigned short) port);
  /* SECURITY:
   * 1) Reject requests not connecting to the control socket IP
   * 2) Reject connects to privileged ports
   */
  if (!tunable_port_promiscuous)
  {
    if (!vsf_sysutil_sockaddr_addr_equal(p_sess->p_remote_addr,
                                         p_sess->p_port_sockaddr) ||
        vsf_sysutil_is_port_reserved(port))
    {
      vsf_cmdio_write(p_sess, FTP_BADCMD, "Illegal EPRT command.");
      port_cleanup(p_sess);
      return;
    }
  }
  vsf_cmdio_write(p_sess, FTP_EPRTOK,
                  "EPRT command successful. Consider using EPSV.");
  return;
bad_eprt:
  vsf_cmdio_write(p_sess, FTP_BADCMD, "Bad EPRT command.");
}

/* XXX - add AUTH etc. */
static void
handle_help(struct vsf_session* p_sess)
{
  vsf_cmdio_write_hyphen(p_sess, FTP_HELP,
                         "The following commands are recognized.");
  vsf_cmdio_write_raw(p_sess,
" ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n");
  vsf_cmdio_write_raw(p_sess,
" MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n");
  vsf_cmdio_write_raw(p_sess,
" RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n");
  vsf_cmdio_write_raw(p_sess,
//" XPWD XRMD\r\n");	// Jiahao
  "XPWD XRMD ICNV\r\n");
  vsf_cmdio_write(p_sess, FTP_HELP, "Help OK.");
}

static void
handle_stou(struct vsf_session* p_sess)
{
  handle_upload_common(p_sess, 0, 1);
}

static void
get_unique_filename(struct mystr* p_outstr, const struct mystr* p_base_str)
{
  /* Use silly wu-ftpd algorithm for compatibility */
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  unsigned int suffix = 1;
  int retval;
  while (1)
  {
    str_copy(p_outstr, p_base_str);
    str_append_char(p_outstr, '.');
    str_append_ulong(p_outstr, suffix);
    retval = str_stat(p_outstr, &s_p_statbuf);
    if (vsf_sysutil_retval_is_error(retval))
    {
      return;
    }
    ++suffix;
  }
}

static void
handle_stat(struct vsf_session* p_sess)
{
  vsf_cmdio_write_hyphen(p_sess, FTP_STATOK, "FTP server status:");
  vsf_cmdio_write_raw(p_sess, "     Connected to ");
  vsf_cmdio_write_raw(p_sess, str_getbuf(&p_sess->remote_ip_str));
  vsf_cmdio_write_raw(p_sess, "\r\n");
  vsf_cmdio_write_raw(p_sess, "     Logged in as ");
  vsf_cmdio_write_raw(p_sess, str_getbuf(&p_sess->user_str));
  vsf_cmdio_write_raw(p_sess, "\r\n");
  vsf_cmdio_write_raw(p_sess, "     TYPE: ");
  if (p_sess->is_ascii)
  {
    vsf_cmdio_write_raw(p_sess, "ASCII\r\n");
  }
  else
  {
    vsf_cmdio_write_raw(p_sess, "BINARY\r\n");
  }
  if (p_sess->bw_rate_max == 0)
  {
    vsf_cmdio_write_raw(p_sess, "     No session bandwidth limit\r\n");
  }
  else
  {
    vsf_cmdio_write_raw(p_sess, "     Session bandwidth limit in byte/s is ");
    vsf_cmdio_write_raw(p_sess, vsf_sysutil_ulong_to_str(p_sess->bw_rate_max));
    vsf_cmdio_write_raw(p_sess, "\r\n");
  }
  if (tunable_idle_session_timeout == 0)
  {
    vsf_cmdio_write_raw(p_sess, "     No session timeout\r\n");
  }
  else
  {
    vsf_cmdio_write_raw(p_sess, "     Session timeout in seconds is ");
    vsf_cmdio_write_raw(p_sess,
      vsf_sysutil_ulong_to_str(tunable_idle_session_timeout));
    vsf_cmdio_write_raw(p_sess, "\r\n");
  }
  if (p_sess->control_use_ssl)
  {
    vsf_cmdio_write_raw(p_sess, "     Control connection is encrypted\r\n"); 
  }
  else
  {
    vsf_cmdio_write_raw(p_sess, "     Control connection is plain text\r\n"); 
  }
  if (p_sess->data_use_ssl)
  {
    vsf_cmdio_write_raw(p_sess, "     Data connections will be encrypted\r\n"); 
  }
  else
  {
    vsf_cmdio_write_raw(p_sess, "     Data connections will be plain text\r\n");
  }
  if (p_sess->num_clients > 0)
  {
    vsf_cmdio_write_raw(p_sess, "     At session startup, client count was ");
    vsf_cmdio_write_raw(p_sess, vsf_sysutil_ulong_to_str(p_sess->num_clients));
    vsf_cmdio_write_raw(p_sess, "\r\n");
  }
  vsf_cmdio_write_raw(p_sess,
    "     vsFTPd " VSF_VERSION " - secure, fast, stable\r\n");
  vsf_cmdio_write(p_sess, FTP_STATOK, "End of status");
}

static void
handle_stat_file(struct vsf_session* p_sess)
{
  handle_dir_common(p_sess, 1, 1);
}

static int
data_transfer_checks_ok(struct vsf_session* p_sess)
{
  if (!pasv_active(p_sess) && !port_active(p_sess))
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
    return 0;
  }
  if (tunable_ssl_enable && !p_sess->data_use_ssl &&
      ((tunable_force_local_data_ssl && !p_sess->is_anonymous) ||
       (tunable_force_anon_data_ssl && p_sess->is_anonymous)))
  {
    vsf_cmdio_write(
      p_sess, FTP_NEEDENCRYPT, "Data connections must be encrypted.");
    return 0;
  }
  return 1;
}

static void
resolve_tilde(struct mystr* p_str, struct vsf_session* p_sess)
{
  unsigned int len = str_getlen(p_str);
  if (len > 0 && str_get_char_at(p_str, 0) == '~')
  {
    static struct mystr s_rhs_str;
    if (len == 1 || str_get_char_at(p_str, 1) == '/')
    {
      str_split_char(p_str, &s_rhs_str, '~');
      str_copy(p_str, &p_sess->home_str);
      str_append_str(p_str, &s_rhs_str);
    }
    else if (tunable_tilde_user_enable && len > 1)
    {
      static struct mystr s_user_str;
      struct vsf_sysutil_user* p_user;
      str_copy(&s_rhs_str, p_str);
      str_split_char(&s_rhs_str, &s_user_str, '~');
      str_split_char(&s_user_str, &s_rhs_str, '/');
      p_user = str_getpwnam(&s_user_str);
      if (p_user != 0)
      {
        str_alloc_text(p_str, vsf_sysutil_user_get_homedir(p_user));
        if (!str_isempty(&s_rhs_str))
        {
          str_append_char(p_str, '/');
          str_append_str(p_str, &s_rhs_str);
        }
      }
    }
  }
}

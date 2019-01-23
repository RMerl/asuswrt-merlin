/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * parseconf.c
 *
 * Routines and support to load in a file full of tunable variables and
 * settings, populating corresponding runtime variables.
 */

#include "parseconf.h"
#include "tunables.h"
#include "str.h"
#include "filestr.h"
#include "defs.h"
#include "sysutil.h"
#include "utility.h"

static const char* s_p_saved_filename;

/* Tables mapping setting names to runtime variables */
/* Boolean settings */
static struct parseconf_bool_setting
{
  const char* p_setting_name;
  int* p_variable;
}
parseconf_bool_array[] =
{
  { "anonymous_enable", &tunable_anonymous_enable },
  { "local_enable", &tunable_local_enable },
  { "pasv_enable", &tunable_pasv_enable },
  { "port_enable", &tunable_port_enable },
  { "chroot_local_user", &tunable_chroot_local_user },
  { "write_enable", &tunable_write_enable },
  { "anon_upload_enable", &tunable_anon_upload_enable },
  { "anon_mkdir_write_enable", &tunable_anon_mkdir_write_enable },
  { "anon_other_write_enable", &tunable_anon_other_write_enable },
  { "chown_uploads", &tunable_chown_uploads },
  { "connect_from_port_20", &tunable_connect_from_port_20 },
  { "xferlog_enable", &tunable_xferlog_enable },
  { "dirmessage_enable", &tunable_dirmessage_enable },
  { "anon_world_readable_only", &tunable_anon_world_readable_only },
  { "async_abor_enable", &tunable_async_abor_enable },
  { "ascii_upload_enable", &tunable_ascii_upload_enable },
  { "ascii_download_enable", &tunable_ascii_download_enable },
  { "one_process_model", &tunable_one_process_model },
  { "xferlog_std_format", &tunable_xferlog_std_format },
  { "pasv_promiscuous", &tunable_pasv_promiscuous },
  { "deny_email_enable", &tunable_deny_email_enable },
  { "chroot_list_enable", &tunable_chroot_list_enable },
  { "setproctitle_enable", &tunable_setproctitle_enable },
  { "text_userdb_names", &tunable_text_userdb_names },
  { "ls_recurse_enable", &tunable_ls_recurse_enable },
  { "log_ftp_protocol", &tunable_log_ftp_protocol },
  { "guest_enable", &tunable_guest_enable },
  { "userlist_enable", &tunable_userlist_enable },
  { "userlist_deny", &tunable_userlist_deny },
  { "use_localtime", &tunable_use_localtime },
  { "check_shell", &tunable_check_shell },
  { "hide_ids", &tunable_hide_ids },
  { "listen", &tunable_listen },
  { "port_promiscuous", &tunable_port_promiscuous },
  { "passwd_chroot_enable", &tunable_passwd_chroot_enable },
  { "no_anon_password", &tunable_no_anon_password },
  { "tcp_wrappers", &tunable_tcp_wrappers },
  { "use_sendfile", &tunable_use_sendfile },
  { "force_dot_files", &tunable_force_dot_files },
  { "listen_ipv6", &tunable_listen_ipv6 },
  { "dual_log_enable", &tunable_dual_log_enable },
  { "syslog_enable", &tunable_syslog_enable },
  { "background", &tunable_background },
  { "virtual_use_local_privs", &tunable_virtual_use_local_privs },
  { "session_support", &tunable_session_support },
  { "download_enable", &tunable_download_enable },
  { "dirlist_enable", &tunable_dirlist_enable },
  { "chmod_enable", &tunable_chmod_enable },
  { "secure_email_list_enable", &tunable_secure_email_list_enable },
  { "run_as_launching_user", &tunable_run_as_launching_user },
  { "no_log_lock", &tunable_no_log_lock },
  { "ssl_enable", &tunable_ssl_enable },
  { "allow_anon_ssl", &tunable_allow_anon_ssl },
  { "force_local_logins_ssl", &tunable_force_local_logins_ssl },
  { "force_local_data_ssl", &tunable_force_local_data_ssl },
  { "ssl_sslv2", &tunable_sslv2 },
  { "ssl_sslv3", &tunable_sslv3 },
  { "ssl_tlsv1", &tunable_tlsv1 },
  { "tilde_user_enable", &tunable_tilde_user_enable },
  { "enable_iconv", &tunable_enable_iconv },
  { "force_anon_logins_ssl", &tunable_force_anon_logins_ssl },
  { "force_anon_data_ssl", &tunable_force_anon_data_ssl },
  { "mdtm_write", &tunable_mdtm_write },
  { "lock_upload_files", &tunable_lock_upload_files },
  { "pasv_addr_resolve", &tunable_pasv_addr_resolve },
  { "debug_ssl", &tunable_debug_ssl },
  { "require_cert", &tunable_require_cert },
  { "validate_cert", &tunable_validate_cert },
  { "strict_ssl_read_eof", &tunable_strict_ssl_read_eof },
  { "strict_ssl_write_shutdown", &tunable_strict_ssl_write_shutdown },
  { "ssl_request_cert", &tunable_ssl_request_cert },
  { "delete_failed_uploads", &tunable_delete_failed_uploads },
  { "implicit_ssl", &tunable_implicit_ssl },
  { "ptrace_sandbox", &tunable_ptrace_sandbox },
  { "require_ssl_reuse", &tunable_require_ssl_reuse },
  { "isolate", &tunable_isolate },
  { "isolate_network", &tunable_isolate_network },
  { "ftp_enable", &tunable_ftp_enable },
  { "http_enable", &tunable_http_enable },
  { "seccomp_sandbox", &tunable_seccomp_sandbox },
  { "allow_writeable_chroot", &tunable_allow_writeable_chroot },
  { 0, 0 }
};

static struct parseconf_uint_setting
{
  const char* p_setting_name;
  unsigned int* p_variable;
}
parseconf_uint_array[] =
{
  { "accept_timeout", &tunable_accept_timeout },
  { "connect_timeout", &tunable_connect_timeout },
  { "local_umask", &tunable_local_umask },
  { "anon_umask", &tunable_anon_umask },
  { "ftp_data_port", &tunable_ftp_data_port },
  { "idle_session_timeout", &tunable_idle_session_timeout },
  { "data_connection_timeout", &tunable_data_connection_timeout },
  { "pasv_min_port", &tunable_pasv_min_port },
  { "pasv_max_port", &tunable_pasv_max_port },
  { "anon_max_rate", &tunable_anon_max_rate },
  { "local_max_rate", &tunable_local_max_rate },
  { "listen_port", &tunable_listen_port },
  { "max_clients", &tunable_max_clients },
  { "file_open_mode", &tunable_file_open_mode },
  { "max_per_ip", &tunable_max_per_ip },
  { "trans_chunk_size", &tunable_trans_chunk_size },
  { "delay_failed_login", &tunable_delay_failed_login },
  { "delay_successful_login", &tunable_delay_successful_login },
  { "max_login_fails", &tunable_max_login_fails },
  { "chown_upload_mode", &tunable_chown_upload_mode },
  { 0, 0 }
};

static struct parseconf_str_setting
{
  const char* p_setting_name;
  const char** p_variable;
}
parseconf_str_array[] =
{
  { "secure_chroot_dir", &tunable_secure_chroot_dir },
  { "ftp_username", &tunable_ftp_username },
  { "chown_username", &tunable_chown_username },
  { "xferlog_file", &tunable_xferlog_file },
  { "vsftpd_log_file", &tunable_vsftpd_log_file },
  { "message_file", &tunable_message_file },
  { "nopriv_user", &tunable_nopriv_user },
  { "ftpd_banner", &tunable_ftpd_banner },
  { "banned_email_file", &tunable_banned_email_file },
  { "chroot_list_file", &tunable_chroot_list_file },
  { "pam_service_name", &tunable_pam_service_name },
  { "guest_username", &tunable_guest_username },
  { "userlist_file", &tunable_userlist_file },
  { "anon_root", &tunable_anon_root },
  { "local_root", &tunable_local_root },
  { "banner_file", &tunable_banner_file },
  { "pasv_address", &tunable_pasv_address },
  { "listen_address", &tunable_listen_address },
  { "user_config_dir", &tunable_user_config_dir },
  { "listen_address6", &tunable_listen_address6 },
  { "cmds_allowed", &tunable_cmds_allowed },
  { "hide_file", &tunable_hide_file },
  { "deny_file", &tunable_deny_file },
  { "user_sub_token", &tunable_user_sub_token },
  { "email_password_file", &tunable_email_password_file },
  { "rsa_cert_file", &tunable_rsa_cert_file },
  { "dsa_cert_file", &tunable_dsa_cert_file },
  { "ssl_ciphers", &tunable_ssl_ciphers },
  { "local_charset", &tunable_local_charset },
  { "remote_charset", &tunable_remote_charset },
  { "rsa_private_key_file", &tunable_rsa_private_key_file },
  { "dsa_private_key_file", &tunable_dsa_private_key_file },
  { "ca_certs_file", &tunable_ca_certs_file },
  { "cmds_denied", &tunable_cmds_denied },
  { 0, 0 }
};

void
vsf_parseconf_load_file(const char* p_filename, int errs_fatal)
{
  struct mystr config_file_str = INIT_MYSTR;
  struct mystr config_setting_str = INIT_MYSTR;
  struct mystr config_value_str = INIT_MYSTR;
  unsigned int str_pos = 0;
  int retval;
  if (!p_filename)
  {
    p_filename = s_p_saved_filename;
  }
  else
  {
    if (s_p_saved_filename)
    {
      vsf_sysutil_free((char*)s_p_saved_filename);
    }
    s_p_saved_filename = vsf_sysutil_strdup(p_filename);
  }
  if (!p_filename)
  {
    bug("null filename in vsf_parseconf_load_file");
  }
  retval = str_fileread(&config_file_str, p_filename, VSFTP_CONF_FILE_MAX);
  if (vsf_sysutil_retval_is_error(retval))
  {
    if (errs_fatal)
    {
      die2("cannot read config file: ", p_filename);
    }
    else
    {
      str_free(&config_file_str);
      return;
    }
  }
  {
    struct vsf_sysutil_statbuf* p_statbuf = 0;
    retval = vsf_sysutil_stat(p_filename, &p_statbuf);
    /* Security: check current user owns the config file. These are sanity
     * checks for the admin, and are NOT designed to be checks safe from
     * race conditions.
     */
    if (vsf_sysutil_retval_is_error(retval) ||
        vsf_sysutil_statbuf_get_uid(p_statbuf) != vsf_sysutil_getuid() ||
        !vsf_sysutil_statbuf_is_regfile(p_statbuf))
    {
      die("config file not owned by correct user, or not a file");
    }
    vsf_sysutil_free(p_statbuf);
  }
  while (str_getline(&config_file_str, &config_setting_str, &str_pos))
  {
    if (str_isempty(&config_setting_str) ||
        str_get_char_at(&config_setting_str, 0) == '#' ||
        str_all_space(&config_setting_str))
    {
      continue;
    }
    vsf_parseconf_load_setting(str_getbuf(&config_setting_str), errs_fatal);
  }
  str_free(&config_file_str);
  str_free(&config_setting_str);
  str_free(&config_value_str);
}

void
vsf_parseconf_load_setting(const char* p_setting, int errs_fatal)
{
  static struct mystr s_setting_str;
  static struct mystr s_value_str;
  while (vsf_sysutil_isspace(*p_setting))
  {
    p_setting++;
  }
  str_alloc_text(&s_setting_str, p_setting);
  str_split_char(&s_setting_str, &s_value_str, '=');
  /* Is it a string setting? */
  {
    const struct parseconf_str_setting* p_str_setting = parseconf_str_array;
    while (p_str_setting->p_setting_name != 0)
    {
      if (str_equal_text(&s_setting_str, p_str_setting->p_setting_name))
      {
        /* Got it */
        const char** p_curr_setting = p_str_setting->p_variable;
        if (*p_curr_setting)
        {
          vsf_sysutil_free((char*) *p_curr_setting);
        }
        if (str_isempty(&s_value_str))
        {
          *p_curr_setting = 0;
        }
        else
        {
          *p_curr_setting = str_strdup(&s_value_str);
        }
        return;
      }
      p_str_setting++;
    }
  }
  if (str_isempty(&s_value_str))
  {
    if (errs_fatal)
    {
      die2("missing value in config file for: ", str_getbuf(&s_setting_str));
    }
    else
    {
      return;
    }
  }
  /* Is it a boolean value? */
  {
    const struct parseconf_bool_setting* p_bool_setting = parseconf_bool_array;
    while (p_bool_setting->p_setting_name != 0)
    {
      if (str_equal_text(&s_setting_str, p_bool_setting->p_setting_name))
      {
        /* Got it */
        str_upper(&s_value_str);
        if (str_equal_text(&s_value_str, "YES") ||
            str_equal_text(&s_value_str, "TRUE") ||
            str_equal_text(&s_value_str, "1"))
        {
          *(p_bool_setting->p_variable) = 1;
        }
        else if (str_equal_text(&s_value_str, "NO") ||
                 str_equal_text(&s_value_str, "FALSE") ||
                 str_equal_text(&s_value_str, "0"))
        {
          *(p_bool_setting->p_variable) = 0;
        }
        else if (errs_fatal)
        {
          die2("bad bool value in config file for: ",
               str_getbuf(&s_setting_str));
        }
        return;
      }
      p_bool_setting++;
    }
  }
  /* Is it an unsigned integer setting? */
  {
    const struct parseconf_uint_setting* p_uint_setting = parseconf_uint_array;
    while (p_uint_setting->p_setting_name != 0)
    {
      if (str_equal_text(&s_setting_str, p_uint_setting->p_setting_name))
      {
        /* Got it */
        /* If the value starts with 0, assume it's an octal value */
        if (!str_isempty(&s_value_str) &&
            str_get_char_at(&s_value_str, 0) == '0')
        {
          *(p_uint_setting->p_variable) = str_octal_to_uint(&s_value_str);
        }
        else
        {
          /* TODO: we could reject negatives instead of converting them? */
          *(p_uint_setting->p_variable) = (unsigned int) str_atoi(&s_value_str);
        }
        return;
      }
      p_uint_setting++;
    }
  }
  if (errs_fatal)
  {
    die2("unrecognised variable in config file: ", str_getbuf(&s_setting_str));
  }
}

/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * tunables.c
 */

#include "tunables.h"
#include "sysutil.h"

int tunable_anonymous_enable;
int tunable_local_enable;
int tunable_pasv_enable;
int tunable_port_enable;
int tunable_chroot_local_user;
int tunable_write_enable;
int tunable_anon_upload_enable;
int tunable_anon_mkdir_write_enable;
int tunable_anon_other_write_enable;
int tunable_chown_uploads;
int tunable_connect_from_port_20;
int tunable_xferlog_enable;
int tunable_dirmessage_enable;
int tunable_anon_world_readable_only;
int tunable_async_abor_enable;
int tunable_ascii_upload_enable;
int tunable_ascii_download_enable;
int tunable_one_process_model;
int tunable_xferlog_std_format;
int tunable_pasv_promiscuous;
int tunable_deny_email_enable;
int tunable_chroot_list_enable;
int tunable_setproctitle_enable;
int tunable_text_userdb_names;
int tunable_ls_recurse_enable;
int tunable_log_ftp_protocol;
int tunable_guest_enable;
int tunable_userlist_enable;
int tunable_userlist_deny;
int tunable_use_localtime;
int tunable_check_shell;
int tunable_hide_ids;
int tunable_listen;
int tunable_port_promiscuous;
int tunable_passwd_chroot_enable;
int tunable_no_anon_password;
int tunable_tcp_wrappers;
int tunable_use_sendfile;
int tunable_force_dot_files;
int tunable_listen_ipv6;
int tunable_dual_log_enable;
int tunable_syslog_enable;
int tunable_background;
int tunable_virtual_use_local_privs;
int tunable_session_support;
int tunable_download_enable;
int tunable_dirlist_enable;
int tunable_chmod_enable;
int tunable_secure_email_list_enable;
int tunable_run_as_launching_user;
int tunable_no_log_lock;
int tunable_ssl_enable;
int tunable_allow_anon_ssl;
int tunable_force_local_logins_ssl;
int tunable_force_local_data_ssl;
int tunable_sslv2;
int tunable_sslv3;
int tunable_tlsv1;
int tunable_tilde_user_enable;
int tunable_force_anon_logins_ssl;
int tunable_force_anon_data_ssl;
int tunable_mdtm_write;
int tunable_lock_upload_files;
int tunable_pasv_addr_resolve;
int tunable_debug_ssl;
int tunable_require_cert;
int tunable_validate_cert;
int tunable_strict_ssl_read_eof;
int tunable_strict_ssl_write_shutdown;
int tunable_ssl_request_cert;
int tunable_delete_failed_uploads;
int tunable_implicit_ssl;
int tunable_ptrace_sandbox;
int tunable_require_ssl_reuse;
int tunable_isolate;
int tunable_isolate_network;
int tunable_ftp_enable;
int tunable_http_enable;
int tunable_seccomp_sandbox;
int tunable_allow_writeable_chroot;
int tunable_enable_iconv;

unsigned int tunable_accept_timeout;
unsigned int tunable_connect_timeout;
unsigned int tunable_local_umask;
unsigned int tunable_anon_umask;
unsigned int tunable_ftp_data_port;
unsigned int tunable_idle_session_timeout;
unsigned int tunable_data_connection_timeout;
unsigned int tunable_pasv_min_port;
unsigned int tunable_pasv_max_port;
unsigned int tunable_anon_max_rate;
unsigned int tunable_local_max_rate;
unsigned int tunable_listen_port;
unsigned int tunable_max_clients;
unsigned int tunable_file_open_mode;
unsigned int tunable_max_per_ip;
unsigned int tunable_trans_chunk_size;
unsigned int tunable_delay_failed_login;
unsigned int tunable_delay_successful_login;
unsigned int tunable_max_login_fails;
unsigned int tunable_chown_upload_mode;

const char* tunable_secure_chroot_dir;
const char* tunable_ftp_username;
const char* tunable_chown_username;
const char* tunable_xferlog_file;
const char* tunable_vsftpd_log_file;
const char* tunable_message_file;
const char* tunable_nopriv_user;
const char* tunable_ftpd_banner;
const char* tunable_banned_email_file;
const char* tunable_chroot_list_file;
const char* tunable_pam_service_name;
const char* tunable_guest_username;
const char* tunable_userlist_file;
const char* tunable_anon_root;
const char* tunable_local_root;
const char* tunable_banner_file;
const char* tunable_pasv_address;
const char* tunable_listen_address;
const char* tunable_user_config_dir;
const char* tunable_listen_address6;
const char* tunable_cmds_allowed;
const char* tunable_cmds_denied;
const char* tunable_hide_file;
const char* tunable_deny_file;
const char* tunable_user_sub_token;
const char* tunable_email_password_file;
const char* tunable_rsa_cert_file;
const char* tunable_dsa_cert_file;
const char* tunable_ssl_ciphers;
const char* tunable_rsa_private_key_file;
const char* tunable_dsa_private_key_file;
const char* tunable_ca_certs_file;
const char* tunable_local_charset;
const char* tunable_remote_charset;

static void install_str_setting(const char* p_value, const char** p_storage);

void
tunables_load_defaults()
{
  tunable_anonymous_enable = 1;
  tunable_local_enable = 0;
  tunable_pasv_enable = 1;
  tunable_port_enable = 1;
  tunable_chroot_local_user = 0;
  tunable_write_enable = 0;
  tunable_anon_upload_enable = 0;
  tunable_anon_mkdir_write_enable = 0;
  tunable_anon_other_write_enable = 0;
  tunable_chown_uploads = 0;
  tunable_connect_from_port_20 = 0;
  tunable_xferlog_enable = 0;
  tunable_dirmessage_enable = 0;
  tunable_anon_world_readable_only = 1;
  tunable_async_abor_enable = 0;
  tunable_ascii_upload_enable = 0;
  tunable_ascii_download_enable = 0;
  tunable_one_process_model = 0;
  tunable_xferlog_std_format = 0;
  tunable_pasv_promiscuous = 0;
  tunable_deny_email_enable = 0;
  tunable_chroot_list_enable = 0;
  tunable_setproctitle_enable = 0;
  tunable_text_userdb_names = 0;
  tunable_ls_recurse_enable = 0;
  tunable_log_ftp_protocol = 0;
  tunable_guest_enable = 0;
  tunable_userlist_enable = 0;
  tunable_userlist_deny = 1;
  tunable_use_localtime = 0;
  tunable_check_shell = 1;
  tunable_hide_ids = 0;
  tunable_listen = 1;
  tunable_port_promiscuous = 0;
  tunable_passwd_chroot_enable = 0;
  tunable_no_anon_password = 0;
  tunable_tcp_wrappers = 0;
  tunable_use_sendfile = 1;
  tunable_force_dot_files = 0;
  tunable_listen_ipv6 = 0;
  tunable_dual_log_enable = 0;
  tunable_syslog_enable = 0;
  tunable_background = 0;
  tunable_virtual_use_local_privs = 0;
  tunable_session_support = 0;
  tunable_download_enable = 1;
  tunable_dirlist_enable = 1;
  tunable_chmod_enable = 1;
  tunable_secure_email_list_enable = 0;
  tunable_run_as_launching_user = 0;
  tunable_no_log_lock = 0;
  tunable_ssl_enable = 0;
  tunable_allow_anon_ssl = 0;
  tunable_force_local_logins_ssl = 1;
  tunable_force_local_data_ssl = 1;
  tunable_sslv2 = 0;
  tunable_sslv3 = 0;
  tunable_tlsv1 = 1;
  tunable_tilde_user_enable = 0;
  tunable_force_anon_logins_ssl = 0;
  tunable_force_anon_data_ssl = 0;
  tunable_mdtm_write = 1;
  tunable_lock_upload_files = 1;
  tunable_pasv_addr_resolve = 0;
  tunable_debug_ssl = 0;
  tunable_require_cert = 0;
  tunable_validate_cert = 0;
  tunable_strict_ssl_read_eof = 0;
  tunable_strict_ssl_write_shutdown = 0;
  tunable_ssl_request_cert = 1;
  tunable_delete_failed_uploads = 0;
  tunable_implicit_ssl = 0;
  tunable_ptrace_sandbox = 0;
  tunable_require_ssl_reuse = 1;
  tunable_isolate = 1;
  tunable_isolate_network = 1;
  tunable_ftp_enable = 1;
  tunable_http_enable = 0;
  tunable_seccomp_sandbox = 1;
  tunable_allow_writeable_chroot = 0;
  tunable_enable_iconv = 0;

  tunable_accept_timeout = 60;
  tunable_connect_timeout = 60;
  tunable_local_umask = 000;
  tunable_anon_umask = 000;
  tunable_ftp_data_port = 20;
  tunable_idle_session_timeout = 300;
  tunable_data_connection_timeout = 300;
  /* IPPORT_USERRESERVED + 1 */
  tunable_pasv_min_port = 5001;
  tunable_pasv_max_port = 0;
  tunable_anon_max_rate = 0;
  tunable_local_max_rate = 0;
  /* IPPORT_FTP */
  tunable_listen_port = 21;
  tunable_max_clients = 2000;
  /* -rw-rw-rw- */
  tunable_file_open_mode = 0666;
  tunable_max_per_ip = 50;
  tunable_trans_chunk_size = 0;
  tunable_delay_failed_login = 1;
  tunable_delay_successful_login = 0;
  tunable_max_login_fails = 3;
  /* -rw------- */
  tunable_chown_upload_mode = 0600;

  install_str_setting("/tmp", &tunable_secure_chroot_dir);
  install_str_setting("utf8", &tunable_local_charset);
  install_str_setting("utf8", &tunable_remote_charset);
  install_str_setting("ftp", &tunable_ftp_username);
  install_str_setting("root", &tunable_chown_username);
  install_str_setting("/var/log/xferlog", &tunable_xferlog_file);
  install_str_setting("/var/log/vsftpd.log", &tunable_vsftpd_log_file);
  install_str_setting(".message", &tunable_message_file);
  install_str_setting("nobody", &tunable_nopriv_user);
  install_str_setting(0, &tunable_ftpd_banner);
  install_str_setting("/etc/vsftpd.banned_emails", &tunable_banned_email_file);
  install_str_setting("/etc/vsftpd.chroot_list", &tunable_chroot_list_file);
  install_str_setting("ftp", &tunable_pam_service_name);
  install_str_setting("ftp", &tunable_guest_username);
  install_str_setting("/etc/vsftpd.user_list", &tunable_userlist_file);
  install_str_setting(0, &tunable_anon_root);
  install_str_setting(0, &tunable_local_root);
  install_str_setting(0, &tunable_banner_file);
  install_str_setting(0, &tunable_pasv_address);
  install_str_setting(0, &tunable_listen_address);
  install_str_setting(0, &tunable_user_config_dir);
  install_str_setting(0, &tunable_listen_address6);
  install_str_setting(0, &tunable_cmds_allowed);
  install_str_setting(0, &tunable_cmds_denied);
  install_str_setting(0, &tunable_hide_file);
  install_str_setting(0, &tunable_deny_file);
  install_str_setting(0, &tunable_user_sub_token);
  install_str_setting("/etc/vsftpd.email_passwords",
                      &tunable_email_password_file);
  install_str_setting("/usr/share/ssl/certs/vsftpd.pem",
                      &tunable_rsa_cert_file);
  install_str_setting(0, &tunable_dsa_cert_file);
  install_str_setting("AES128-SHA:DES-CBC3-SHA", &tunable_ssl_ciphers);
  install_str_setting(0, &tunable_rsa_private_key_file);
  install_str_setting(0, &tunable_dsa_private_key_file);
  install_str_setting(0, &tunable_ca_certs_file);
}

void
install_str_setting(const char* p_value, const char** p_storage)
{
  char* p_curr_val = (char*) *p_storage;
  if (p_curr_val != 0)
  {
    vsf_sysutil_free(p_curr_val);
  }
  if (p_value != 0)
  {
    p_value = vsf_sysutil_strdup(p_value);
  }
  *p_storage = p_value;
}

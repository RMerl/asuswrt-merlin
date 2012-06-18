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
 * tunables.c
 */

#include "tunables.h"

int tunable_anonymous_enable = 1;
int tunable_local_enable = 0;
int tunable_pasv_enable = 1;
int tunable_port_enable = 1;
int tunable_chroot_local_user = 0;
int tunable_write_enable = 0;
int tunable_anon_upload_enable = 0;
int tunable_anon_mkdir_write_enable = 0;
int tunable_anon_other_write_enable = 0;
int tunable_chown_uploads = 0;
int tunable_connect_from_port_20 = 0;
int tunable_xferlog_enable = 0;
int tunable_dirmessage_enable = 0;
int tunable_anon_world_readable_only = 1;
int tunable_async_abor_enable = 0;
int tunable_ascii_upload_enable = 0;
int tunable_ascii_download_enable = 0;
int tunable_one_process_model = 0;
int tunable_xferlog_std_format = 0;
int tunable_pasv_promiscuous = 0;
int tunable_deny_email_enable = 0;
int tunable_chroot_list_enable = 0;
int tunable_setproctitle_enable = 0;
int tunable_text_userdb_names = 0;
int tunable_ls_recurse_enable = 0;
int tunable_log_ftp_protocol = 0;
int tunable_guest_enable = 0;
int tunable_userlist_enable = 0;
int tunable_userlist_deny = 1;
int tunable_use_localtime = 0;
int tunable_check_shell = 1;
int tunable_hide_ids = 0;
int tunable_listen = 0;
int tunable_port_promiscuous = 0;
int tunable_passwd_chroot_enable = 0;
int tunable_no_anon_password = 0;
int tunable_tcp_wrappers = 0;
int tunable_use_sendfile = 1;
int tunable_force_dot_files = 0;
int tunable_listen_ipv6 = 0;
int tunable_dual_log_enable = 0;
int tunable_syslog_enable = 0;
int tunable_background = 0;
int tunable_virtual_use_local_privs = 0;
int tunable_session_support = 0;
int tunable_download_enable = 1;
int tunable_dirlist_enable = 1;
int tunable_chmod_enable = 1;
int tunable_secure_email_list_enable = 0;
int tunable_run_as_launching_user = 0;
int tunable_no_log_lock = 0;
int tunable_ssl_enable = 0;
int tunable_allow_anon_ssl = 0;
int tunable_force_local_logins_ssl = 1;
int tunable_force_local_data_ssl = 1;
int tunable_sslv2 = 0;
int tunable_sslv3 = 0;
int tunable_tlsv1 = 1;
int tunable_tilde_user_enable = 0;
int tunable_enable_iconv = 0;	// Jiahao
int tunable_force_anon_logins_ssl = 0;
int tunable_force_anon_data_ssl = 0;
int tunable_mdtm_write = 1;
int tunable_lock_upload_files = 1;
int tunable_pasv_addr_resolve = 0;

unsigned int tunable_accept_timeout = 60;
unsigned int tunable_connect_timeout = 60;
// 2008.05 James. {
/*unsigned int tunable_local_umask = 077;
unsigned int tunable_anon_umask = 077;//*/
unsigned int tunable_local_umask = 000;
unsigned int tunable_anon_umask = 000;//*/
// 2008.05 James. }
unsigned int tunable_ftp_data_port = 20;
unsigned int tunable_idle_session_timeout = 300;
unsigned int tunable_data_connection_timeout = 300;
/* IPPORT_USERRESERVED + 1 */
unsigned int tunable_pasv_min_port = 5001;
unsigned int tunable_pasv_max_port = 0;
unsigned int tunable_anon_max_rate = 0;
unsigned int tunable_local_max_rate = 0;
/* IPPORT_FTP */
unsigned int tunable_listen_port = 21;
unsigned int tunable_max_clients = 0;
/* -rw-rw-rw- */
unsigned int tunable_file_open_mode = 0666;
unsigned int tunable_max_per_ip = 0;
unsigned int tunable_trans_chunk_size = 0;

const char* tunable_secure_chroot_dir = "/tmp";	//Yen
const char* tunable_ftp_username = "ftp";
const char* tunable_chown_username = "root";
const char* tunable_xferlog_file = "/var/log/xferlog";
const char* tunable_vsftpd_log_file = "/var/log/vsftpd.log";
const char* tunable_message_file = ".message";
const char* tunable_nopriv_user = "nobody";
const char* tunable_ftpd_banner = 0;
const char* tunable_banned_email_file = "/etc/vsftpd.banned_emails";
const char* tunable_chroot_list_file = "/etc/vsftpd.chroot_list";
const char* tunable_pam_service_name = "ftp";
const char* tunable_guest_username = "ftp";
const char* tunable_userlist_file = "/etc/vsftpd.user_list";
const char* tunable_anon_root = 0;
const char* tunable_local_root = 0;
const char* tunable_banner_file = 0;
const char* tunable_pasv_address = 0;
const char* tunable_listen_address = 0;
const char* tunable_user_config_dir = 0;
const char* tunable_listen_address6 = 0;
const char* tunable_cmds_allowed = 0;
const char* tunable_hide_file = 0;
const char* tunable_deny_file = 0;
const char* tunable_user_sub_token = 0;
const char* tunable_email_password_file = "/etc/vsftpd.email_passwords";
const char* tunable_rsa_cert_file = "/usr/share/ssl/certs/vsftpd.pem";
const char* tunable_dsa_cert_file = 0;
const char* tunable_ssl_ciphers = "DES-CBC3-SHA";
const char* tunable_local_charset = "utf8";	// Jiahao
//const char* tunable_remote_charset = "cp950";
const char* tunable_remote_charset = "utf8";
const char* tunable_rsa_private_key_file = 0;
const char* tunable_dsa_private_key_file = 0;


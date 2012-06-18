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
 * main.c
 */

#include "session.h"
#include "utility.h"
#include "tunables.h"
#include "logging.h"
#include "str.h"
#include "filestr.h"
#include "ftpcmdio.h"
#include "sysutil.h"
#include "sysdeputil.h"
#include "defs.h"
#include "parseconf.h"
#include "oneprocess.h"
#include "twoprocess.h"
#include "standalone.h"
#include "tcpwrap.h"
#include "vsftpver.h"
#include "ssl.h"
#include <stdio.h>

/*
 * Forward decls of helper functions
 */
static void die_unless_privileged(void);
static void do_sanity_checks(void);
static void session_init(struct vsf_session* p_sess);
static void env_init(void);

int
main(int argc, const char* argv[])
{
  struct vsf_session the_session =
  {
    /* Control connection */
    0, 0, 0,
    /* Data connection */
    -1, 0, -1, 0, 0, 0, 0,
    /* Login */
    1, INIT_MYSTR, INIT_MYSTR,
    /* Protocol state */
    0, 1, INIT_MYSTR, 0, 0,
    /* Session state */
    0,
    /* Userids */
    -1, -1, -1,
    /* Pre-chroot() cache */
    INIT_MYSTR, INIT_MYSTR, INIT_MYSTR, INIT_MYSTR, 1,
    /* Logging */
    -1, -1, INIT_MYSTR, 0, 0, 0, INIT_MYSTR, 0,
    /* Buffers */
    INIT_MYSTR, INIT_MYSTR, 0, INIT_MYSTR,
    /* Parent <-> child comms */
    -1, -1,
    /* Number of clients */
    0, 0,
    /* Home directory */
    INIT_MYSTR,
    /* Secure connection state */
    0, 0, 0, 0, 0, 0, -1, -1,
    /* write_enable */
    0
  };
  int config_specified = 0;
  const char* p_config_name = VSFTP_DEFAULT_CONFIG;
  /* Zero or one argument supported. If one argument is passed, it is the
   * path to the config file
   */
  if (argc > 2)
  {
    die("vsftpd: too many arguments (I take an optional config file only)");
  }
  else if (argc == 0)
  {
    die("vsftpd: missing argv[0]");
  }
  if (argc == 2)
  {
    if (!vsf_sysutil_strcmp(argv[1], "-v"))
    {
      vsf_exit("vsftpd: version " VSF_VERSION "\n");
    }
    p_config_name = argv[1];
    config_specified = 1;
  }
  /* This might need to open /dev/zero on systems lacking MAP_ANON. Needs
   * to be done early (i.e. before config file parse, which may use
   * anonymous pages
   */
  vsf_sysutil_map_anon_pages_init();
  /* Parse config file if it's there */
  {
    struct vsf_sysutil_statbuf* p_statbuf = 0;
    int retval = vsf_sysutil_stat(p_config_name, &p_statbuf);
    if (!vsf_sysutil_retval_is_error(retval))
    {
      vsf_parseconf_load_file(p_config_name, 1);
    }
    else if (config_specified)
    {
      die2("vsftpd: cannot open config file:", p_config_name);
    }
    vsf_sysutil_free(p_statbuf);
  }
  /* Resolve pasv_address if required */
  if (tunable_pasv_address && tunable_pasv_addr_resolve)
  {
    struct vsf_sysutil_sockaddr* p_addr = 0;
    const char* p_numeric_addr;
    vsf_sysutil_dns_resolve(&p_addr, tunable_pasv_address);
    vsf_sysutil_free((char*) tunable_pasv_address);
    p_numeric_addr = vsf_sysutil_inet_ntop(p_addr);
    tunable_pasv_address = vsf_sysutil_strdup(p_numeric_addr);
    vsf_sysutil_free(p_addr);
  }
  if (!tunable_run_as_launching_user)
  {
    /* Just get out unless we start with requisite privilege */
    die_unless_privileged();
  }
  if (tunable_setproctitle_enable)
  {
    /* Warning -- warning -- may nuke argv, environ */
    vsf_sysutil_setproctitle_init(argc, argv);
  }
  /* Initialize the SSL system here if needed - saves the overhead of each
   *  child doing this itself.
   */
  if (tunable_ssl_enable)
  {
    ssl_init(&the_session);
  }
  if (tunable_listen || tunable_listen_ipv6)
  {
    /* Standalone mode */
    struct vsf_client_launch ret = vsf_standalone_main();
    the_session.num_clients = ret.num_children;
    the_session.num_this_ip = ret.num_this_ip;
  }
  if (tunable_tcp_wrappers)
  {
    the_session.tcp_wrapper_ok = vsf_tcp_wrapper_ok(VSFTP_COMMAND_FD);
  }
  {
    const char* p_load_conf = vsf_sysutil_getenv("VSFTPD_LOAD_CONF");
    if (p_load_conf)
    {
      vsf_parseconf_load_file(p_load_conf, 1);
    }
  }
  /* Sanity checks - exit with a graceful error message if our STDIN is not
   * a socket. Also check various config options don't collide.
   */
  do_sanity_checks();
  /* Initializes session globals - e.g. IP addr's etc. */
  session_init(&the_session);
  /* Set up "environment", e.g. process group etc. */
  env_init();
  /* Set up logging - must come after global init because we need the remote
   * address to convert into text
   */
  vsf_log_init(&the_session);
  str_alloc_text(&the_session.remote_ip_str,
                 vsf_sysutil_inet_ntop(the_session.p_remote_addr));
  /* Set up options on the command socket */
  vsf_cmdio_sock_setup();
  if (tunable_setproctitle_enable)
  {
    vsf_sysutil_set_proctitle_prefix(&the_session.remote_ip_str);
    vsf_sysutil_setproctitle("connected");
  }
  /* We might chroot() very soon (one process model), so we need to open
   * any required config files here.
   */
  /* SSL may have been enabled by a per-IP configuration.. */
  if (tunable_ssl_enable)
  {
    ssl_init(&the_session);
  }
  if (tunable_deny_email_enable)
  {
    int retval = str_fileread(&the_session.banned_email_str,
                              tunable_banned_email_file, VSFTP_CONF_FILE_MAX);
    if (vsf_sysutil_retval_is_error(retval))
    {
      die2("cannot open anon e-mail list file:", tunable_banned_email_file);
    }
  }
  if (tunable_banner_file)
  {
    int retval = str_fileread(&the_session.banner_str, tunable_banner_file,
                              VSFTP_CONF_FILE_MAX);
    if (vsf_sysutil_retval_is_error(retval))
    {
      die2("cannot open banner file:", tunable_banner_file);
    }
  }
  if (tunable_secure_email_list_enable)
  {
    int retval = str_fileread(&the_session.email_passwords_str,
                              tunable_email_password_file,
                              VSFTP_CONF_FILE_MAX);
    if (vsf_sysutil_retval_is_error(retval))
    {
      die2("cannot open email passwords file:", tunable_email_password_file);
    }
  }
  /* Special case - can force one process model if we've got a setup
   * needing _no_ privs
   */
  if (!tunable_local_enable && !tunable_connect_from_port_20 &&
      !tunable_chown_uploads)
  {
    tunable_one_process_model = 1;
  }
  if (tunable_run_as_launching_user)
  {
    tunable_one_process_model = 1;
    if (!vsf_sysutil_running_as_root())
    {
      tunable_connect_from_port_20 = 0;
      tunable_chown_uploads = 0;
    }
  }

  if (tunable_one_process_model)
  {
    vsf_one_process_start(&the_session);
  }
  else
  {
    vsf_two_process_start(&the_session);
  }
  /* NOTREACHED */
  bug("should not get here: main");
  return 1;
}

static void
die_unless_privileged(void)
{
  if (!vsf_sysutil_running_as_root())
  {
    die("vsftpd: must be started as root (see run_as_launching_user option)");
  }
}

static void
do_sanity_checks(void)
{
  {
    struct vsf_sysutil_statbuf* p_statbuf = 0;
    vsf_sysutil_fstat(VSFTP_COMMAND_FD, &p_statbuf);
    if (!vsf_sysutil_statbuf_is_socket(p_statbuf))
    {
      die("vsftpd: not configured for standalone, must be started from inetd");
    }
    vsf_sysutil_free(p_statbuf);
  }
  if (tunable_one_process_model)
  {
    if (tunable_local_enable)
    {
      die("vsftpd: security: 'one_process_model' is anonymous only");
    }
    if (!vsf_sysdep_has_capabilities_as_non_root())
    {
      die("vsftpd: security: 'one_process_model' needs a better OS");
    }
  }
  if (!tunable_local_enable && !tunable_anonymous_enable)
  {
    die("vsftpd: both local and anonymous access disabled!");
  }
}

static void
env_init(void)
{
  vsf_sysutil_make_session_leader();
  /* Set up a secure umask - we'll set the proper one after login */
  vsf_sysutil_set_umask(VSFTP_SECURE_UMASK);
  /* Fire up libc's timezone initialisation, before we chroot()! */
  vsf_sysutil_tzset();
  /* Signals. We'll always take -EPIPE rather than a rude signal, thanks */
  vsf_sysutil_install_null_sighandler(kVSFSysUtilSigPIPE);
}

static void
session_init(struct vsf_session* p_sess)
{
  /* Get the addresses of the control connection */
  vsf_sysutil_getpeername(VSFTP_COMMAND_FD, &p_sess->p_remote_addr);
  vsf_sysutil_getsockname(VSFTP_COMMAND_FD, &p_sess->p_local_addr);
  
  /* If anonymous mode is active, fetch the uid of the anonymous user */
  if (tunable_anonymous_enable)
  {
    const struct vsf_sysutil_user* p_user = vsf_sysutil_getpwnam(tunable_ftp_username);
    
    if (p_user == 0)
    {
      die2("vsftpd: cannot locate user specified in 'ftp_username':",
           tunable_ftp_username);
    }
    p_sess->anon_ftp_uid = vsf_sysutil_user_getuid(p_user);
  }
  
  if (tunable_guest_enable)
  {
    const struct vsf_sysutil_user* p_user = vsf_sysutil_getpwnam(tunable_guest_username);
    
    if (p_user == 0)
    {
      die2("vsftpd: cannot locate user specified in 'guest_username':",
           tunable_guest_username);
    }
    p_sess->guest_user_uid = vsf_sysutil_user_getuid(p_user);
  }
  
  if (tunable_chown_uploads)
  {
    const struct vsf_sysutil_user* p_user = vsf_sysutil_getpwnam(tunable_chown_username);
    
    if (p_user == 0)
    {
      die2("vsftpd: cannot locate user specified in 'chown_username':",
           tunable_chown_username);
    }
    p_sess->anon_upload_chown_uid = vsf_sysutil_user_getuid(p_user);
  }
}

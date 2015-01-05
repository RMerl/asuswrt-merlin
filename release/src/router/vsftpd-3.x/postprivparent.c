/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * postprivparent.c
 *
 * This file contains all privileged parent services offered after logging
 * in. This includes e.g. chown() of uploaded files, issuing of port 20
 * sockets.
 */

#include "postprivparent.h"
#include "session.h"
#include "privops.h"
#include "privsock.h"
#include "utility.h"
#include "tunables.h"
#include "defs.h"
#include "sysutil.h"
#include "str.h"
#include "secutil.h"
#include "sysstr.h"
#include "sysdeputil.h"
#include "seccompsandbox.h"

static void minimize_privilege(struct vsf_session* p_sess);
static void process_post_login_req(struct vsf_session* p_sess);
static void cmd_process_chown(struct vsf_session* p_sess);
static void cmd_process_get_data_sock(struct vsf_session* p_sess);
static void cmd_process_pasv_cleanup(struct vsf_session* p_sess);
static void cmd_process_pasv_active(struct vsf_session* p_sess);
static void cmd_process_pasv_listen(struct vsf_session* p_sess);
static void cmd_process_pasv_accept(struct vsf_session* p_sess);

void
vsf_priv_parent_postlogin(struct vsf_session* p_sess)
{
  minimize_privilege(p_sess);
  /* We're still here... */
  while (1)
  {
    process_post_login_req(p_sess);
  }
}

static void
process_post_login_req(struct vsf_session* p_sess)
{
  char cmd;
  /* Blocks */
  cmd = priv_sock_get_cmd(p_sess->parent_fd);
  if (tunable_chown_uploads && cmd == PRIV_SOCK_CHOWN)
  {
    cmd_process_chown(p_sess);
  }
  else if (cmd == PRIV_SOCK_GET_DATA_SOCK)
  {
    cmd_process_get_data_sock(p_sess);
  }
  else if (cmd == PRIV_SOCK_PASV_CLEANUP)
  {
    cmd_process_pasv_cleanup(p_sess);
  }
  else if (cmd == PRIV_SOCK_PASV_ACTIVE)
  {
    cmd_process_pasv_active(p_sess);
  }
  else if (cmd == PRIV_SOCK_PASV_LISTEN)
  {
    cmd_process_pasv_listen(p_sess);
  }
  else if (cmd == PRIV_SOCK_PASV_ACCEPT)
  {
    cmd_process_pasv_accept(p_sess);
  }
  else
  {
    die("bad request in process_post_login_req");
  }
}

static void
minimize_privilege(struct vsf_session* p_sess)
{
  /* So, we logged in and forked a totally unprivileged child. Our job
   * now is to minimize the privilege we need in order to act as a helper
   * to the child.
   */
  if (!p_sess->is_anonymous && tunable_session_support)
  {
    /* Need to hang around to update logs, utmp, wtmp etc. on logout.
     * Need to keep privs to do this. */
    return;
  }
  {
    unsigned int caps = 0;
    struct mystr user_str = INIT_MYSTR;
    struct mystr dir_str = INIT_MYSTR;
    if (tunable_nopriv_user)
    {
      str_alloc_text(&user_str, tunable_nopriv_user);
    }
    if (tunable_secure_chroot_dir)
    {
      str_alloc_text(&dir_str, tunable_secure_chroot_dir);
    }
    if (tunable_chown_uploads)
    {
      caps |= kCapabilityCAP_CHOWN;
    }
    if (tunable_connect_from_port_20)
    {
      caps |= kCapabilityCAP_NET_BIND_SERVICE;
    }
    vsf_secutil_change_credentials(&user_str, &dir_str, 0, caps,
                                   VSF_SECUTIL_OPTION_CHROOT);
    str_free(&user_str);
    str_free(&dir_str);
  }
  seccomp_sandbox_init();
  seccomp_sandbox_setup_postlogin_broker();
  seccomp_sandbox_lockdown();
}

static void
cmd_process_chown(struct vsf_session* p_sess)
{
  int the_fd = priv_sock_recv_fd(p_sess->parent_fd);
  vsf_privop_do_file_chown(p_sess, the_fd);
  vsf_sysutil_close(the_fd);
  priv_sock_send_result(p_sess->parent_fd, PRIV_SOCK_RESULT_OK);
}

static void
cmd_process_get_data_sock(struct vsf_session* p_sess)
{
  unsigned short port = (unsigned short) priv_sock_get_int(p_sess->parent_fd);
  int sock_fd = vsf_privop_get_ftp_port_sock(p_sess, port, 0);
  if (sock_fd == -1)
  {
    priv_sock_send_result(p_sess->parent_fd, PRIV_SOCK_RESULT_BAD);
    return;
  }
  priv_sock_send_result(p_sess->parent_fd, PRIV_SOCK_RESULT_OK);
  priv_sock_send_fd(p_sess->parent_fd, sock_fd);
  vsf_sysutil_close(sock_fd);
}

static void
cmd_process_pasv_cleanup(struct vsf_session* p_sess)
{
  vsf_privop_pasv_cleanup(p_sess);
  priv_sock_send_result(p_sess->parent_fd, PRIV_SOCK_RESULT_OK);
}

static void
cmd_process_pasv_active(struct vsf_session* p_sess)
{
  int active = vsf_privop_pasv_active(p_sess);
  priv_sock_send_int(p_sess->parent_fd, active);
}

static void
cmd_process_pasv_listen(struct vsf_session* p_sess)
{
  unsigned short port = vsf_privop_pasv_listen(p_sess);
  priv_sock_send_int(p_sess->parent_fd, port);
}

static void
cmd_process_pasv_accept(struct vsf_session* p_sess)
{
  int fd = vsf_privop_accept_pasv(p_sess);
  if (fd < 0)
  {
    priv_sock_send_result(p_sess->parent_fd, PRIV_SOCK_RESULT_BAD);
    priv_sock_send_int(p_sess->parent_fd, fd);
    return;
  }
  priv_sock_send_result(p_sess->parent_fd, PRIV_SOCK_RESULT_OK);
  priv_sock_send_fd(p_sess->parent_fd, fd);
  vsf_sysutil_close(fd);
}


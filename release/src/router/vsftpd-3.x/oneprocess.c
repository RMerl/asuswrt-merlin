/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * oneprocess.c
 *
 * Code for the "one process" security model. The one process security model
 * is born for the purposes of raw speed at the expense of compromising the
 * purity of the security model.
 * The one process model will typically be disabled, for security reasons.
 * Only sites with huge numbers of concurrent users are likely to feel the
 * pain of two processes per session.
 */

#include "prelogin.h"
#include "postlogin.h"
#include "privops.h"
#include "session.h"
#include "secutil.h"
#include "str.h"
#include "tunables.h"
#include "utility.h"
#include "sysstr.h"
#include "sysdeputil.h"
#include "sysutil.h"
#include "ptracesandbox.h"
#include "ftppolicy.h"
#include "seccompsandbox.h"

static void one_process_start(void* p_arg);

void
vsf_one_process_start(struct vsf_session* p_sess)
{
  if (tunable_ptrace_sandbox)
  {
    struct pt_sandbox* p_sandbox = ptrace_sandbox_alloc();
    if (p_sandbox == 0)
    {
      die("could not allocate sandbox (only works for 32-bit builds)");
    }
    policy_setup(p_sandbox, p_sess);
    if (ptrace_sandbox_launch_process(p_sandbox,
                                      one_process_start,
                                      (void*) p_sess) <= 0)
    {
      die("could not launch sandboxed child");
    }
    /* TODO - could drop privs here. For now, run as root as the attack surface
     * is negligible, and running as root permits us to correctly deliver the
     * parent death signal upon unexpected crash.
     */
    (void) ptrace_sandbox_run_processes(p_sandbox);
    ptrace_sandbox_free(p_sandbox);
    vsf_sysutil_exit(0);
  }
  else
  {
    one_process_start((void*) p_sess);
  }
}

static void
one_process_start(void* p_arg)
{
  struct vsf_session* p_sess = (struct vsf_session*) p_arg;
  unsigned int caps = 0;
  if (tunable_chown_uploads)
  {
    caps |= kCapabilityCAP_CHOWN;
  }
  if (tunable_connect_from_port_20)
  {
    caps |= kCapabilityCAP_NET_BIND_SERVICE;
  }
  {
    struct mystr user_name = INIT_MYSTR;
    struct mystr chdir_str = INIT_MYSTR;
    if (tunable_ftp_username)
    {
      str_alloc_text(&user_name, tunable_ftp_username);
    }
    if (tunable_anon_root)
    {
      str_alloc_text(&chdir_str, tunable_anon_root);
    }
    if (tunable_run_as_launching_user)
    {
      if (!str_isempty(&chdir_str))
      {
        (void) str_chdir(&chdir_str);
      }
    }
    else
    {
      vsf_secutil_change_credentials(&user_name, 0, &chdir_str, caps,
          VSF_SECUTIL_OPTION_CHROOT |
          VSF_SECUTIL_OPTION_USE_GROUPS |
          VSF_SECUTIL_OPTION_NO_PROCS);
    }
    str_free(&user_name);
    str_free(&chdir_str);
  }
  if (tunable_ptrace_sandbox)
  {
    ptrace_sandbox_attach_point();
  }
  seccomp_sandbox_init();
  seccomp_sandbox_setup_postlogin(p_sess);
  seccomp_sandbox_lockdown();
  init_connection(p_sess);
}

void
vsf_one_process_login(struct vsf_session* p_sess,
                      const struct mystr* p_pass_str)
{
  enum EVSFPrivopLoginResult login_result =
    vsf_privop_do_login(p_sess, p_pass_str);
  switch (login_result)
  {
    case kVSFLoginFail:
      return;
      break;
    case kVSFLoginAnon:
      p_sess->is_anonymous = 1;
      process_post_login(p_sess);
      break;
    case kVSFLoginNull:
      /* Fall through. */
    case kVSFLoginReal:
      /* Fall through. */
    default:
      bug("bad state in vsf_one_process_login");
      break;
  }
}

int
vsf_one_process_get_priv_data_sock(struct vsf_session* p_sess)
{
  unsigned short port = vsf_sysutil_sockaddr_get_port(p_sess->p_port_sockaddr);
  return vsf_privop_get_ftp_port_sock(p_sess, port, 1);
}

void
vsf_one_process_pasv_cleanup(struct vsf_session* p_sess)
{
  vsf_privop_pasv_cleanup(p_sess);
}

int
vsf_one_process_pasv_active(struct vsf_session* p_sess)
{
  return vsf_privop_pasv_active(p_sess);
}

unsigned short
vsf_one_process_listen(struct vsf_session* p_sess)
{
  return vsf_privop_pasv_listen(p_sess);
}

int
vsf_one_process_get_pasv_fd(struct vsf_session* p_sess)
{
  return vsf_privop_accept_pasv(p_sess);
}

void
vsf_one_process_chown_upload(struct vsf_session* p_sess, int fd)
{
  vsf_privop_do_file_chown(p_sess, fd);
}


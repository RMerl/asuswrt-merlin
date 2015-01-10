/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * ftppolicy.c
 *
 * Code to build a sandbox policy based on current session options.
 */

#include "ftppolicy.h"
#include "ptracesandbox.h"
#include "tunables.h"
#include "session.h"
#include "sysutil.h"

/* For AF_INET etc. network constants. */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

static int socket_validator(struct pt_sandbox* p_sandbox, void* p_arg);
static int connect_validator(struct pt_sandbox* p_sandbox, void* p_arg);
static int getsockopt_validator(struct pt_sandbox* p_sandbox, void* p_arg);
static int setsockopt_validator(struct pt_sandbox* p_sandbox, void* p_arg);

void
policy_setup(struct pt_sandbox* p_sandbox, const struct vsf_session* p_sess)
{
  int is_anon = p_sess->is_anonymous;
  /* Always need to be able to exit! */
  ptrace_sandbox_permit_exit(p_sandbox);
  /* Needed for memory management. */
  ptrace_sandbox_permit_mmap(p_sandbox);
  ptrace_sandbox_permit_mprotect(p_sandbox);
  ptrace_sandbox_permit_brk(p_sandbox);
  /* Simple reads and writes are required. Permitting write does not imply
   * filesystem write access because access control is done at open time.
   */
  ptrace_sandbox_permit_read(p_sandbox);
  ptrace_sandbox_permit_write(p_sandbox);
  /* Reading FTP commands from the network. */
  ptrace_sandbox_permit_recv(p_sandbox);
  /* Querying time is harmless; used for log timestamps and internally to
   * OpenSSL
   */
  ptrace_sandbox_permit_query_time(p_sandbox);

  /* Typically post-login things follow. */
  /* Since we're in a chroot(), we can just blanket allow filesystem readonly
   * open.
   */
  ptrace_sandbox_permit_open(p_sandbox, 0);
  ptrace_sandbox_permit_close(p_sandbox);
  /* Other pathname-based metadata queries. */
  ptrace_sandbox_permit_file_stats(p_sandbox);
  ptrace_sandbox_permit_readlink(p_sandbox);
  /* Querying, reading and changing directory. */
  ptrace_sandbox_permit_getcwd(p_sandbox);
  ptrace_sandbox_permit_chdir(p_sandbox);
  ptrace_sandbox_permit_getdents(p_sandbox);
  /* Simple fd-based operations. */
  ptrace_sandbox_permit_fd_stats(p_sandbox);
  ptrace_sandbox_permit_seek(p_sandbox);
  ptrace_sandbox_permit_shutdown(p_sandbox);
  ptrace_sandbox_permit_fcntl(p_sandbox);
  ptrace_sandbox_permit_setsockopt(p_sandbox);
  ptrace_sandbox_set_setsockopt_validator(p_sandbox, setsockopt_validator, 0);
  /* Misc */
  /* Setting umask. */
  ptrace_sandbox_permit_umask(p_sandbox);
  /* Select for data connection readyness. */
  ptrace_sandbox_permit_select(p_sandbox);
  /* Always need ability to take signals (SIGPIPE) */
  ptrace_sandbox_permit_sigreturn(p_sandbox);
  /* Sleeping (bandwidth limit, connect retires, anon login fails) */
  ptrace_sandbox_permit_sleep(p_sandbox);
  /* High-speed transfers... */
  ptrace_sandbox_permit_sendfile(p_sandbox);
  /* TODO - Grrrr! nscd cache access is leaking into child. Need to find out
   * out how to disable that. Also means that text_userdb_names loads values
   * from the real system data.
   */
  if (tunable_text_userdb_names)
  {
    ptrace_sandbox_permit_mremap(p_sandbox);
  }
  /* May need ability to install signal handlers. */
  if (tunable_async_abor_enable ||
      tunable_idle_session_timeout > 0 ||
      tunable_data_connection_timeout > 0)
  {
    ptrace_sandbox_permit_sigaction(p_sandbox);
  }
  /* May need ability to set up timeout alarms. */
  if (tunable_idle_session_timeout > 0 || tunable_data_connection_timeout > 0)
  {
    ptrace_sandbox_permit_alarm(p_sandbox);
  }
  /* Set up network permissions according to config and session. */
  ptrace_sandbox_permit_socket(p_sandbox);
  ptrace_sandbox_set_socket_validator(p_sandbox,
                                      socket_validator,
                                      (void*) p_sess);
  ptrace_sandbox_permit_bind(p_sandbox);
  /* Yes, reuse of the connect validator is intentional. */
  ptrace_sandbox_set_bind_validator(p_sandbox,
                                    connect_validator,
                                    (void*) p_sess);
  if (tunable_port_enable)
  {
    ptrace_sandbox_permit_connect(p_sandbox);
    ptrace_sandbox_set_connect_validator(p_sandbox,
                                         connect_validator,
                                         (void*) p_sess);
    ptrace_sandbox_permit_getsockopt(p_sandbox);
    ptrace_sandbox_set_getsockopt_validator(p_sandbox, getsockopt_validator, 0);
  }
  if (tunable_pasv_enable)
  {
    ptrace_sandbox_permit_listen(p_sandbox);
    ptrace_sandbox_permit_accept(p_sandbox);
  }
  /* Set up write permissions according to config and session. */
  if (tunable_write_enable)
  {
    if (!is_anon || tunable_anon_upload_enable)
    {
      ptrace_sandbox_permit_open(p_sandbox, 1);
    }
    if (!is_anon || tunable_anon_mkdir_write_enable)
    {
      ptrace_sandbox_permit_mkdir(p_sandbox);
    }
    if (!is_anon || tunable_anon_other_write_enable)
    {
      ptrace_sandbox_permit_unlink(p_sandbox);
      ptrace_sandbox_permit_rmdir(p_sandbox);
      ptrace_sandbox_permit_rename(p_sandbox);
      ptrace_sandbox_permit_ftruncate(p_sandbox);
      if (tunable_mdtm_write)
      {
        ptrace_sandbox_permit_utime(p_sandbox);
      }
    }
    if (!is_anon && tunable_chmod_enable)
    {
      ptrace_sandbox_permit_chmod(p_sandbox);
    }
    if (is_anon && tunable_chown_uploads)
    {
      ptrace_sandbox_permit_fchmod(p_sandbox);
      ptrace_sandbox_permit_fchown(p_sandbox);
    }
  }
}

static int
socket_validator(struct pt_sandbox* p_sandbox, void* p_arg)
{
  int ret;
  struct vsf_session* p_sess = (struct vsf_session*) p_arg;
  unsigned long arg1;
  unsigned long arg2;
  unsigned long expected_family = AF_INET;
  if (vsf_sysutil_sockaddr_is_ipv6(p_sess->p_local_addr))
  {
    expected_family = AF_INET6;
  }
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 0, &arg1);
  if (ret != 0)
  {
    return ret;
  }
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 1, &arg2);
  if (ret != 0)
  {
    return ret;
  }
  if (arg1 != expected_family || arg2 != SOCK_STREAM)
  {
    return -1;
  }
  return 0;
}

static int
connect_validator(struct pt_sandbox* p_sandbox, void* p_arg)
{
  int ret;
  struct vsf_session* p_sess = (struct vsf_session*) p_arg;
  unsigned long arg2;
  unsigned long arg3;
  unsigned long expected_family = AF_INET;
  unsigned long expected_len = sizeof(struct sockaddr_in);
  void* p_buf = 0;
  struct sockaddr* p_sockaddr;
  static struct vsf_sysutil_sockaddr* p_sockptr;
  if (vsf_sysutil_sockaddr_is_ipv6(p_sess->p_local_addr))
  {
    expected_family = AF_INET6;
    expected_len = sizeof(struct sockaddr_in6);
  }
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 1, &arg2);
  if (ret != 0)
  {
    return ret;
  }
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 2, &arg3);
  if (ret != 0)
  {
    return ret;
  }
  if (arg3 != expected_len)
  {
    return -1;
  }
  p_buf = vsf_sysutil_malloc((int) expected_len);
  ret = ptrace_sandbox_get_buf(p_sandbox, arg2, expected_len, p_buf);
  if (ret != 0)
  {
    vsf_sysutil_free(p_buf);
    return -2;
  }
  p_sockaddr = (struct sockaddr*) p_buf;
  if (p_sockaddr->sa_family != expected_family)
  {
    vsf_sysutil_free(p_buf);
    return -3;
  }
  if (expected_family == AF_INET)
  {
    struct sockaddr_in* p_sockaddr_in = (struct sockaddr_in*) p_sockaddr;
    vsf_sysutil_sockaddr_alloc_ipv4(&p_sockptr);
    vsf_sysutil_sockaddr_set_ipv4addr(p_sockptr,
                                      (const unsigned char*)
                                          &p_sockaddr_in->sin_addr);
  }
  else
  {
    struct sockaddr_in6* p_sockaddr_in6 = (struct sockaddr_in6*) p_sockaddr;
    vsf_sysutil_sockaddr_alloc_ipv6(&p_sockptr);
    vsf_sysutil_sockaddr_set_ipv6addr(p_sockptr,
                                      (const unsigned char*)
                                          &p_sockaddr_in6->sin6_addr);
  }
  if (!vsf_sysutil_sockaddr_addr_equal(p_sess->p_remote_addr, p_sockptr))
  {
    vsf_sysutil_free(p_buf);
    return -4;
  }
  vsf_sysutil_free(p_buf);
  return 0;
}

static int
getsockopt_validator(struct pt_sandbox* p_sandbox, void* p_arg)
{
  int ret;
  unsigned long arg2;
  unsigned long arg3;
  (void) p_arg;
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 1, &arg2);
  if (ret != 0)
  {
    return ret;
  }
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 2, &arg3);
  if (ret != 0)
  {
    return ret;
  }
  if (arg2 != SOL_SOCKET || arg3 != SO_ERROR)
  {
    return -1;
  }
  return 0;
}

static int
setsockopt_validator(struct pt_sandbox* p_sandbox, void* p_arg)
{
  int ret;
  unsigned long arg2;
  unsigned long arg3;
  (void) p_arg;
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 1, &arg2);
  if (ret != 0)
  {
    return ret;
  }
  ret = ptrace_sandbox_get_socketcall_arg(p_sandbox, 2, &arg3);
  if (ret != 0)
  {
    return ret;
  }
  if (arg2 == SOL_SOCKET)
  {
    if (arg3 != SO_KEEPALIVE &&
        arg3 != SO_REUSEADDR &&
        arg3 != SO_OOBINLINE &&
        arg3 != SO_LINGER)
    {
      return -1;
    }
  }
  else if (arg2 == IPPROTO_TCP)
  {
    if (arg3 != TCP_NODELAY)
    {
      return -2;
    }
  }
  else if (arg2 == IPPROTO_IP)
  {
    if (arg3 != IP_TOS)
    {
      return -3;
    }
  }
  else
  {
    return -4;
  }
  return 0;
}

/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * ptracesandbox.c
 *
 * Generic routines to setup and run a process under a restrictive ptrace()
 * based sandbox.
 * Note that the style in this file is to not go via the helper functions in
 * sysutil.c, but instead hit the system APIs directly. This is because I may
 * very well release just this file to the public domain, and do not want
 * dependencies on other parts of vsftpd.
 */

#include "ptracesandbox.h"

#if defined(__linux__) && defined(__i386__)

#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
/* For AF_MAX (NPROTO is defined to this) */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include <asm/unistd.h>

#ifndef __NR_sendfile64
  #define __NR_sendfile64 239
#endif

#ifndef __NR_exit_group
  #define __NR_exit_group 252
#endif

#ifndef __NR_utimes
  #define __NR_utimes 271
#endif

/* For the socketcall() multiplex args. */
#include <linux/net.h>

#ifndef PTRACE_SETOPTIONS
  #define PTRACE_SETOPTIONS 0x4200
#endif

#ifndef PTRACE_O_TRACESYSGOOD
  #define PTRACE_O_TRACESYSGOOD 1
#endif

#ifndef PTRACE_O_TRACEFORK
  #define PTRACE_O_TRACEFORK 2
#endif

#ifndef PTRACE_O_TRACEVFORK
  #define PTRACE_O_TRACEVFORK 4
#endif

#ifndef PTRACE_O_TRACECLONE
  #define PTRACE_O_TRACECLONE 8
#endif

#ifndef O_DIRECT
  #define O_DIRECT 040000
#endif

static void sanitize_child();
static int get_action(struct pt_sandbox* p_sandbox);

static int validate_mmap2(struct pt_sandbox* p_sandbox, void* p_arg);
static int validate_open_default(struct pt_sandbox* p_sandbox, void* p_arg);
static int validate_open_readonly(struct pt_sandbox* p_sandbox, void* p_arg);
static int validate_fcntl(struct pt_sandbox* p_sandbox, void* p_arg);
static int validate_socketcall(struct pt_sandbox* p_sandbox, void* p_arg);
static void install_socketcall(struct pt_sandbox* p_sandbox);

#define MAX_SYSCALL 300

struct pt_sandbox
{
  int read_event_fd;
  int write_event_fd;
  pid_t pid;
  int is_allowed[MAX_SYSCALL];
  ptrace_sandbox_validator_t validator[MAX_SYSCALL];
  void* validator_arg[MAX_SYSCALL];
  int is_exit;
  struct user_regs_struct regs;
  int is_socketcall_allowed[NPROTO];
  ptrace_sandbox_validator_t socketcall_validator[NPROTO];
  void* socketcall_validator_arg[NPROTO];
};

static int s_sigchld_fd = -1;

void
handle_sigchld(int sig)
{
  int ret;
  if (sig != SIGCHLD)
  {
    _exit(1);
  }
  if (s_sigchld_fd != -1)
  {
    do
    {
      static const char zero = '\0';
      ret = write(s_sigchld_fd, &zero, sizeof(zero));
    } while (ret == -1 && errno == EINTR);
    if (ret != 1)
    {
      _exit(2);
    }
  }
}

struct pt_sandbox*
ptrace_sandbox_alloc()
{
  int i;
  struct sigaction sigact;
  struct pt_sandbox* ret = malloc(sizeof(struct pt_sandbox));
  if (ret == NULL)
  {
    return NULL;
  }
  ret->pid = -1;
  ret->read_event_fd = -1;
  ret->write_event_fd = -1;
  ret->is_exit = 0;
  memset(&ret->regs, '\0', sizeof(ret->regs));
  for (i = 0; i < MAX_SYSCALL; ++i)
  {
    ret->is_allowed[i] = 0;
    ret->validator[i] = 0;
    ret->validator_arg[i] = 0;
  }
  for (i = 0; i < NPROTO; ++i)
  {
    ret->is_socketcall_allowed[i] = 0;
    ret->socketcall_validator[i] = 0;
    ret->socketcall_validator_arg[i] = 0;
  }
  memset((void*) &sigact, '\0', sizeof(sigact));
  sigact.sa_handler = handle_sigchld;
  if (sigaction(SIGCHLD, &sigact, NULL) != 0)
  {
    goto err_out;
  }
  return ret;
err_out:
  ptrace_sandbox_free(ret);
  return NULL;
}

void
ptrace_sandbox_free(struct pt_sandbox* p_sandbox)
{
  if (p_sandbox->pid != -1)
  {
    warnx("bug: pid active in ptrace_sandbox_free");
    /* We'll kill it for you so it doesn't escape the sandbox totally, but
     * we won't reap the zombie.
     * Killing it like this is a risk: if it's stopped in syscall entry,
     * that syscall will execute before the pending kill takes effect.
     * If that pending syscall were to be a fork(), there could be trouble.
     */
    (void) kill(p_sandbox->pid, SIGKILL);
  }
  if (p_sandbox->read_event_fd != -1)
  {
    s_sigchld_fd = -1;
    close(p_sandbox->read_event_fd);
    close(p_sandbox->write_event_fd);
  }
  free(p_sandbox);
}

void
ptrace_sandbox_attach_point()
{
  long pt_ret;
  int ret;
  pid_t pid = getpid();
  if (pid <= 1)
  {
    warnx("weird pid");
    _exit(1);
  }
  /* You don't have to use PTRACE_TRACEME, but if you don't, a rogue SIGCONT
   * might wake you up from the STOP below before the tracer has attached.
   */
  pt_ret = ptrace(PTRACE_TRACEME, 0, 0, 0);
  if (pt_ret != 0)
  {
    warn("PTRACE_TRACEME failed");
    _exit(2);
  }
  ret = kill(pid, SIGSTOP);
  if (ret != 0)
  {
    warn("kill SIGSTOP failed");
    _exit(3);
  }
}

int
ptrace_sandbox_launch_process(struct pt_sandbox* p_sandbox,
                              void (*p_func)(void*),
                              void* p_arg)
{
  long pt_ret;
  pid_t ret;
  int status;
  if (p_sandbox->pid != -1)
  {
    warnx("bug: process already active");
    return -1;
  }
  ret = fork();
  if (ret < 0)
  {
    return -1;
  }
  else if (ret == 0)
  {
    /* Child context. */
    sanitize_child();
    (*p_func)(p_arg);
    _exit(0);
  }
  /* Parent context */
  p_sandbox->pid = ret;
  do
  {
    ret = waitpid(p_sandbox->pid, &status, 0);
  } while (ret == -1 && errno == EINTR);
  if (ret == -1)
  {
    warn("waitpid failed");
    goto kill_out;
  }
  else if (ret != p_sandbox->pid)
  {
    warnx("unknown pid %d", ret);
    goto kill_out;
  }
  if (!WIFSTOPPED(status))
  {
    warnx("not stopped status %d\n", status);
    goto kill_out;
  }
  if (WSTOPSIG(status) != SIGSTOP)
  {
    warnx("not SIGSTOP status %d\n", status);
    goto kill_out;
  }
  /* The fork, etc. tracing options are worth a bit of explanation. We don't
   * permit process launching syscalls at all as they are dangerous. But
   * there's a small race if the untrusted process attempts a denied fork()
   * and then takes a rouge SIGKILL before the supervisor gets a chance to
   * clear the orig_eax register. In this case the syscall will still execute.
   * (Policies may not include signal sending capabilities, thus mitigating this
   * direct attack, however a rogue SIGKILL may come from a non-malicious
   * source). Therefore, we'd rather any fork()ed process starts off traced,
   * just in case this tiny race condition triggers.
   */
  pt_ret = ptrace(PTRACE_SETOPTIONS,
                  p_sandbox->pid,
                  0,
                  PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK |
                      PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE);
  if (pt_ret != 0)
  {
    warn("PTRACE_SETOPTIONS failure");
    goto kill_out;
  }
  return p_sandbox->pid;
kill_out:
  (void) kill(p_sandbox->pid, SIGKILL);
  p_sandbox->pid = -1;
  return -1;
}

int
ptrace_sandbox_continue_process(struct pt_sandbox* p_sandbox, int sig)
{
  long pt_ret = ptrace(PTRACE_SYSCALL, p_sandbox->pid, 0, sig);
  if (pt_ret != 0)
  {
    warn("PTRACE_SYSCALL failure");
    if (errno == ESRCH)
    {
      return PTRACE_SANDBOX_ERR_DEAD;
    }
    return PTRACE_SANDBOX_ERR_PTRACE;
  }
  return 0;
}

int
ptrace_sandbox_get_event_fd(struct pt_sandbox* p_sandbox)
{
  /* TODO: allocate pipe fds */
  (void) p_sandbox;
  return -1;
}

int
ptrace_sandbox_get_event(struct pt_sandbox* p_sandbox, int* status, int block)
{
  pid_t pid;
  int options = 0;
  if (!block)
  {
    options = WNOHANG;
  }
  do
  {
    pid = waitpid(p_sandbox->pid, status, options);
  } while (pid == -1 && errno == EINTR);
  if (pid == -1)
  {
    warn("waitpid failure");
    if (errno == ECHILD)
    {
      return PTRACE_SANDBOX_ERR_DEAD;
    }
    return PTRACE_SANDBOX_ERR_WAITPID;
  }
  return pid;
}

int
ptrace_sandbox_handle_event(struct pt_sandbox* p_sandbox, int status)
{
  int sig;
  int action;
  if (WIFEXITED(status) || WIFSIGNALED(status))
  {
    p_sandbox->pid = -1;
    return 1;
  }
  if (!WIFSTOPPED(status))
  {
    warnx("weird status: %d\n", status);
    return PTRACE_SANDBOX_ERR_WAIT_STATUS;
  }
  sig = WSTOPSIG(status);
  if (sig >= 0 && sig < 0x80)
  {
    /* It's a normal signal; deliver it right on. SIGSTOP / SIGCONT handling
     * are buggy in the kernel and I'm not sure it's safe to pass either on,
     * so the signal becomes a little more... robust :)
     */
    if (sig == SIGSTOP || sig == SIGCONT)
    {
      sig = SIGKILL;
    }
    return ptrace_sandbox_continue_process(p_sandbox, sig);
  }
  if (!(sig & 0x80))
  {
    warnx("weird status: %d\n", status);
    return PTRACE_SANDBOX_ERR_WAIT_STATUS;
  }
  /* Syscall trap. */
  if (p_sandbox->is_exit)
  {
    p_sandbox->is_exit = 0;
  }
  else
  {
    p_sandbox->is_exit = 1;
    action = get_action(p_sandbox);
    if (action != 0)
    {
      return action;
    }
  }
  return ptrace_sandbox_continue_process(p_sandbox, 0);
}

int
ptrace_sandbox_run_processes(struct pt_sandbox* p_sandbox)
{
  if (ptrace_sandbox_continue_process(p_sandbox, 0) != 0)
  {
    goto kill_out;
  }
  while (1)
  {
    int status;
    int ret = ptrace_sandbox_get_event(p_sandbox, &status, 1);
    if (ret <= 0)
    {
      goto kill_out;
    }
    ret = ptrace_sandbox_handle_event(p_sandbox, status);
    if (ret < 0)
    {
      warnx("couldn't handle sandbox event");
      goto kill_out;
    }
    if (ret == 1)
    {
      return 0;
    }
  }
kill_out:
  ptrace_sandbox_kill_processes(p_sandbox);
  return -1;
}

void
ptrace_sandbox_kill_processes(struct pt_sandbox* p_sandbox)
{
  long pt_ret;
  struct user_regs_struct regs;
  pid_t pid = p_sandbox->pid;
  if (pid == -1)
  {
    return;
  }
  p_sandbox->pid = -1;
  pt_ret = ptrace(PTRACE_GETREGS, pid, 0, &regs);
  if (pt_ret != 0)
  {
    warn("PTRACE_GETREGS failure");
    /* This API is supposed to be called with the process stopped; but if it
     * is still running, we can at least help a bit. See security related
     * comment in ptrace_sandbox_free(), though.
     */
    (void) kill(pid, SIGKILL);
    return;
  }
  /* Kind of nasty, but the only way of stopping a started syscall from
   * executing is to rewrite the registers to execute a different syscall.
   */
  regs.orig_eax = __NR_exit_group;
  regs.eip = 0xffffffff;
  pt_ret = ptrace(PTRACE_SETREGS, pid, 0, &regs);
  if (pt_ret != 0)
  {
    warn("PTRACE_SETREGS failure");
    /* Deliberate fall-thru. */
  }
  pt_ret = ptrace(PTRACE_KILL, pid, 0, 0);
  if (pt_ret != 0)
  {
    warn("PTRACE_KILL failure");
    /* Deliberate fall-thru. */
  }
  /* Just to make ourselves clear. */
  (void) kill(pid, SIGKILL);
  /* So the GETREGS succeeded, so the process definitely _was_ there. We can
   * safely wait for it to reap the zombie.
   */
  (void) waitpid(pid, NULL, 0);
}

int
ptrace_sandbox_get_arg(struct pt_sandbox* p_sandbox,
                       int arg,
                       unsigned long* p_out)
{
  long ret = 0;
  struct user_regs_struct* p_regs = &p_sandbox->regs;
  if (p_regs->orig_eax == 0)
  {
    return PTRACE_SANDBOX_ERR_API_ABUSE_STOPIT;
  }
  if (arg < 0 || arg > 5)
  {
    return PTRACE_SANDBOX_ERR_API_ABUSE_STOPIT;
  }
  switch (arg)
  {
  case 0:
    ret = p_regs->ebx;
    break;
  case 1:
    ret = p_regs->ecx;
    break;
  case 2:
    ret = p_regs->edx;
    break;
  case 3:
    ret = p_regs->esi;
    break;
  case 4:
    ret = p_regs->edi;
    break;
  case 5:
    ret = p_regs->ebp;
    break;
  }
  *p_out = ret;
  return 0;
}

int
ptrace_sandbox_get_socketcall_arg(struct pt_sandbox* p_sandbox,
                                  int arg,
                                  unsigned long* p_out)
{
  unsigned long ptr;
  int ret;
  struct user_regs_struct* p_regs = &p_sandbox->regs;
  if (p_regs->orig_eax == 0)
  {
    return PTRACE_SANDBOX_ERR_API_ABUSE_STOPIT;
  }
  if (arg < 0 || arg > 2)
  {
    return PTRACE_SANDBOX_ERR_API_ABUSE_STOPIT;
  }
  ret = ptrace_sandbox_get_arg(p_sandbox, 1, &ptr);
  if (ret != 0)
  {
    return ret;
  }
  ptr += (arg * 4);
  ret = ptrace_sandbox_get_long(p_sandbox, ptr, p_out);
  return ret;
}

int
ptrace_sandbox_get_long(struct pt_sandbox* p_sandbox,
                        unsigned long ptr,
                        unsigned long* p_out)
{
  return ptrace_sandbox_get_buf(p_sandbox, ptr, sizeof(long), (void*) p_out);
}

int
ptrace_sandbox_get_buf(struct pt_sandbox* p_sandbox,
                       unsigned long ptr,
                       unsigned long len,
                       void* p_buf)
{
  long pt_ret;
  char* p_out = (char*) p_buf;
  for (; len > 0; len -= sizeof(long))
  {
    errno = 0;
    pt_ret = ptrace(PTRACE_PEEKDATA, p_sandbox->pid, (void*) ptr, 0);
    if (pt_ret == -1 && errno != 0)
    {
      warn("PTRACE_GETREGS failure");
      if (errno == ESRCH)
      {
        return PTRACE_SANDBOX_ERR_DEAD;
      }
      return PTRACE_SANDBOX_ERR_PTRACE;
    }
    if (len >= sizeof(long))
    {
      memcpy(p_out, &pt_ret, sizeof(long));
    }
    else
    {
      memcpy(p_out, &pt_ret, len);
    }
    p_out += sizeof(long);
    ptr += sizeof(long);
  }
  return 0;
}

static void
sanitize_child()
{
  /* Ensure that if our sandbox supervisor goes down, so do we. */
  int ret = prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
  if (ret != 0)
  {
    _exit(3);
  }
}

static int
get_action(struct pt_sandbox* p_sandbox)
{
  int ret;
  int call;
  int cs;
  long pt_ret = ptrace(PTRACE_GETREGS, p_sandbox->pid, 0, &(p_sandbox->regs));
  if (pt_ret != 0)
  {
    warn("PTRACE_GETREGS failure");
    if (errno == ESRCH)
    {
      return PTRACE_SANDBOX_ERR_DEAD;
    }
    return PTRACE_SANDBOX_ERR_PTRACE;
  }
  /* We need to be sure that the child is attempting a syscall against the
   * 32-bit syscall table, otherwise they can bypass the policy by abusing the
   * fact that e.g. syscall 200 is getgid32() on 32-bit but tkill() on 64-bit.
   * If the syscall instruct was int80 or sysenter, is it guaranteed to hit
   * the 32-bit table. If it is syscall, the current CS selector determines
   * the table. Therefore, we can check the current CS selector references a
   * known system-only selector that is guaranteed 32-bit (not long mode).
   */
  cs = p_sandbox->regs.xcs;
  if (cs != 0x73 && cs != 0x23)
  {
    warnx("bad CS %d", cs);
    ret = PTRACE_SANDBOX_ERR_BAD_SYSCALL;
    goto out;
  }
  call = (int) p_sandbox->regs.orig_eax;
  if (call < 0 || call >= MAX_SYSCALL)
  {
    warnx("syscall %d out of bounds", call);
    ret = PTRACE_SANDBOX_ERR_BAD_SYSCALL;
    goto out;
  }
  if (p_sandbox->is_allowed[call] != 1)
  {
    syslog(LOG_LOCAL0 | LOG_DEBUG, "syscall not permitted: %d", call);
    warnx("syscall not permitted: %d", call);
    ret = PTRACE_SANDBOX_ERR_POLICY_SYSCALL;
    goto out;
  }
  if (p_sandbox->validator[call])
  {
    ptrace_sandbox_validator_t p_validate = p_sandbox->validator[call];
    int validate_ret = (*p_validate)(p_sandbox, p_sandbox->validator_arg[call]);
    if (validate_ret != 0)
    {
      syslog(LOG_LOCAL0 | LOG_DEBUG,
             "syscall validate fail: %d (%d)",
             call,
             validate_ret);
      warnx("syscall validate failed: %d (%d)", call, validate_ret);
      ret = PTRACE_SANDBOX_ERR_POLICY_ARGS;
      goto out;
    }
  }
  ret = 0;
out:
  memset(&p_sandbox->regs, '\0', sizeof(p_sandbox->regs));
  return ret;
}

void
ptrace_sandbox_permit_exit(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_exit] = 1;
  p_sandbox->is_allowed[__NR_exit_group] = 1;
}

void
ptrace_sandbox_permit_read(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_read] = 1;
}

void
ptrace_sandbox_permit_write(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_write] = 1;
}

void
ptrace_sandbox_permit_sigaction(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_sigaction] = 1;
  p_sandbox->is_allowed[__NR_rt_sigaction] = 1;
}

void
ptrace_sandbox_permit_alarm(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_alarm] = 1;
}

void
ptrace_sandbox_permit_query_time(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_gettimeofday] = 1;
  p_sandbox->is_allowed[__NR_time] = 1;
}

void
ptrace_sandbox_permit_mmap(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_mmap2] = 1;
  p_sandbox->validator[__NR_mmap2] = validate_mmap2;
}

static int
validate_mmap2(struct pt_sandbox* p_sandbox, void* p_arg)
{
  unsigned long arg4;
  int ret = ptrace_sandbox_get_arg(p_sandbox, 3, &arg4);
  (void) p_arg;
  if (ret != 0)
  {
    return ret;
  }
  if (arg4 & MAP_SHARED)
  {
    return -1;
  }
  return 0;
}

void
ptrace_sandbox_permit_mprotect(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_mprotect] = 1;
}

void
ptrace_sandbox_permit_file_stats(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_stat] = 1;
  p_sandbox->is_allowed[__NR_stat64] = 1;
  p_sandbox->is_allowed[__NR_lstat] = 1;
  p_sandbox->is_allowed[__NR_lstat64] = 1;
}

void
ptrace_sandbox_permit_fd_stats(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_fstat] = 1;
  p_sandbox->is_allowed[__NR_fstat64] = 1;
}

void
ptrace_sandbox_permit_getcwd(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_getcwd] = 1;
}

void
ptrace_sandbox_permit_chdir(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_chdir] = 1;
}

void
ptrace_sandbox_permit_umask(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_umask] = 1;
}

void
ptrace_sandbox_permit_open(struct pt_sandbox* p_sandbox, int writeable)
{
  p_sandbox->is_allowed[__NR_open] = 1;
  if (writeable == 1)
  {
    p_sandbox->validator[__NR_open] = validate_open_default;
  }
  else
  {
    p_sandbox->validator[__NR_open] = validate_open_readonly;
  }
}

static int
validate_open_default(struct pt_sandbox* p_sandbox, void* p_arg)
{
  unsigned long arg2;
  int ret = ptrace_sandbox_get_arg(p_sandbox, 1, &arg2);
  (void) p_arg;
  if (ret != 0)
  {
    return ret;
  }
  if (arg2 & (O_ASYNC | O_DIRECT | O_SYNC))
  {
    return -1;
  }
  return 0;
}

static int
validate_open_readonly(struct pt_sandbox* p_sandbox, void* p_arg)
{
  unsigned long arg2;
  int ret = validate_open_default(p_sandbox, p_arg);
  if (ret != 0)
  {
    return ret;
  }
  ret = ptrace_sandbox_get_arg(p_sandbox, 1, &arg2);
  if (ret != 0)
  {
    return ret;
  }
  if ((arg2 & O_ACCMODE) != O_RDONLY)
  {
    return -1;
  }
  return 0;
}

void
ptrace_sandbox_permit_close(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_close] = 1;
}

void
ptrace_sandbox_permit_getdents(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_getdents] = 1;
  p_sandbox->is_allowed[__NR_getdents64] = 1;
}

void
ptrace_sandbox_permit_fcntl(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_fcntl] = 1;
  p_sandbox->validator[__NR_fcntl] = validate_fcntl;
  p_sandbox->is_allowed[__NR_fcntl64] = 1;
  p_sandbox->validator[__NR_fcntl64] = validate_fcntl;
}

static int
validate_fcntl(struct pt_sandbox* p_sandbox, void* p_arg)
{
  unsigned long arg2;
  unsigned long arg3;
  int ret = ptrace_sandbox_get_arg(p_sandbox, 1, &arg2);
  (void) p_arg;
  if (ret != 0)
  {
    return ret;
  }
  ret = ptrace_sandbox_get_arg(p_sandbox, 2, &arg3);
  if (ret != 0)
  {
    return ret;
  }
  if (arg2 != F_GETFL &&
      arg2 != F_SETFL &&
      arg2 != F_SETOWN &&
      arg2 != F_SETLK &&
      arg2 != F_SETLKW &&
      arg2 != F_SETLK64 &&
      arg2 != F_SETLKW64 &&
      arg2 != F_SETFD &&
      arg2 != F_GETFD)
  {
    syslog(LOG_LOCAL0 | LOG_DEBUG, "fcntl not permitted: %ld", arg2);
    warnx("fcntl not permitted: %ld", arg2);
    return -1;
  }
  if (arg2 == F_SETFL && (arg3 & (O_ASYNC | O_DIRECT)))
  {
    return -2;
  }
  if (arg2 == F_SETOWN && (int) arg3 != p_sandbox->pid)
  {
    return -3;
  }
  return 0;
}

void
ptrace_sandbox_permit_sendfile(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_sendfile] = 1;
  p_sandbox->is_allowed[__NR_sendfile64] = 1;
}

void
ptrace_sandbox_permit_seek(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_lseek] = 1;
  p_sandbox->is_allowed[__NR__llseek] = 1;
}

void
ptrace_sandbox_permit_select(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_select] = 1;
  p_sandbox->is_allowed[__NR__newselect] = 1;
}

void
ptrace_sandbox_permit_unlink(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_unlink] = 1;
}

void
ptrace_sandbox_permit_mkdir(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_mkdir] = 1;
}

void
ptrace_sandbox_permit_rmdir(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_rmdir] = 1;
}

void
ptrace_sandbox_permit_rename(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_rename] = 1;
}

void
ptrace_sandbox_permit_utime(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_utime] = 1;
  p_sandbox->is_allowed[__NR_utimes] = 1;
}

void
ptrace_sandbox_permit_sigreturn(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_sigreturn] = 1;
}

void
ptrace_sandbox_permit_recv(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_RECV] = 1;
}

static void
install_socketcall(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_socketcall] = 1;
  p_sandbox->validator[__NR_socketcall] = validate_socketcall;
}

static int
validate_socketcall(struct pt_sandbox* p_sandbox, void* p_arg)
{
  unsigned long arg1;
  int ret = ptrace_sandbox_get_arg(p_sandbox, 0, &arg1);
  (void) p_arg;
  if (ret != 0)
  {
    return ret;
  }
  if (arg1 < 1 || arg1 >= NPROTO)
  {
    return -1;
  }
  if (p_sandbox->is_socketcall_allowed[arg1] != 1)
  {
    syslog(LOG_LOCAL0 | LOG_DEBUG, "socketcall not permitted: %ld", arg1);
    warnx("socketcall not permitted: %ld", arg1);
    return -2;
  }
  if (p_sandbox->socketcall_validator[arg1])
  {
    ptrace_sandbox_validator_t p_val = p_sandbox->socketcall_validator[arg1];
    ret = (*p_val)(p_sandbox, p_sandbox->socketcall_validator_arg[arg1]);
    if (ret != 0)
    {
      syslog(LOG_LOCAL0 | LOG_DEBUG,
             "socketcall validate fail: %ld (%d)",
             arg1,
             ret);
      warnx("socketcall validate fail: %ld (%d)", arg1, ret);
      return -3;
    }
  }
  return 0;
}

void
ptrace_sandbox_permit_readlink(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_readlink] = 1;
}

void
ptrace_sandbox_permit_brk(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_brk] = 1;
}

void
ptrace_sandbox_permit_sleep(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_nanosleep] = 1;
}

void
ptrace_sandbox_permit_fchmod(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_fchmod] = 1;
}

void
ptrace_sandbox_permit_chmod(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_chmod] = 1;
}

void
ptrace_sandbox_permit_fchown(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_fchown] = 1;
  p_sandbox->is_allowed[__NR_fchown32] = 1;
}

void
ptrace_sandbox_permit_mremap(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_mremap] = 1;
}

void
ptrace_sandbox_permit_ftruncate(struct pt_sandbox* p_sandbox)
{
  p_sandbox->is_allowed[__NR_ftruncate] = 1;
  p_sandbox->is_allowed[__NR_ftruncate64] = 1;
}

void
ptrace_sandbox_permit_socket(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_SOCKET] = 1;
}

void
ptrace_sandbox_set_socket_validator(struct pt_sandbox* p_sandbox,
                                    ptrace_sandbox_validator_t val,
                                    void* p_arg)
{
  p_sandbox->socketcall_validator[SYS_SOCKET] = val;
  p_sandbox->socketcall_validator_arg[SYS_SOCKET] = p_arg;
}

void
ptrace_sandbox_permit_bind(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_BIND] = 1;
}

void
ptrace_sandbox_set_bind_validator(struct pt_sandbox* p_sandbox,
                                  ptrace_sandbox_validator_t val,
                                  void* p_arg)
{
  p_sandbox->socketcall_validator[SYS_BIND] = val;
  p_sandbox->socketcall_validator_arg[SYS_BIND] = p_arg;
}

void
ptrace_sandbox_permit_connect(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_CONNECT] = 1;
}

void
ptrace_sandbox_set_connect_validator(struct pt_sandbox* p_sandbox,
                                     ptrace_sandbox_validator_t val,
                                     void* p_arg)
{
  p_sandbox->socketcall_validator[SYS_CONNECT] = val;
  p_sandbox->socketcall_validator_arg[SYS_CONNECT] = p_arg;
}

void
ptrace_sandbox_permit_listen(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_LISTEN] = 1;
}

void
ptrace_sandbox_permit_accept(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_ACCEPT] = 1;
}

void
ptrace_sandbox_permit_setsockopt(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_SETSOCKOPT] = 1;
}

void
ptrace_sandbox_set_setsockopt_validator(struct pt_sandbox* p_sandbox,
                                        ptrace_sandbox_validator_t val,
                                        void* p_arg)
{
  p_sandbox->socketcall_validator[SYS_SETSOCKOPT] = val;
  p_sandbox->socketcall_validator_arg[SYS_SETSOCKOPT] = p_arg;
}

void
ptrace_sandbox_permit_getsockopt(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_GETSOCKOPT] = 1;
}

void
ptrace_sandbox_set_getsockopt_validator(struct pt_sandbox* p_sandbox,
                                        ptrace_sandbox_validator_t val,
                                        void* p_arg)
{
  p_sandbox->socketcall_validator[SYS_GETSOCKOPT] = val;
  p_sandbox->socketcall_validator_arg[SYS_GETSOCKOPT] = p_arg;
}

void
ptrace_sandbox_permit_shutdown(struct pt_sandbox* p_sandbox)
{
  install_socketcall(p_sandbox);
  p_sandbox->is_socketcall_allowed[SYS_SHUTDOWN] = 1;
}

#else /* __linux__ && __i386__ */

struct pt_sandbox*
ptrace_sandbox_alloc()
{
  return 0;
}

void
ptrace_sandbox_free(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

int
ptrace_sandbox_launch_process(struct pt_sandbox* p_sandbox,
                              void (*p_func)(void*),
                              void* p_arg)
{
  (void) p_sandbox;
  (void) p_func;
  (void) p_arg;
  return -1;
}

int
ptrace_sandbox_run_processes(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
  return -1;
}

void
ptrace_sandbox_attach_point(void)
{
}

void
ptrace_sandbox_permit_exit(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_read(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_write(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_sigaction(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_alarm(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_query_time(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_mmap(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_mprotect(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_file_stats(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_fd_stats(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_getcwd(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_chdir(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_umask(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_open(struct pt_sandbox* p_sandbox, int writeable)
{
  (void) p_sandbox;
  (void) writeable;
}

void
ptrace_sandbox_permit_close(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_getdents(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_fcntl(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_sendfile(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_seek(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_select(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_unlink(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_mkdir(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_rmdir(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_rename(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_utime(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_utimes(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_sigreturn(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_recv(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_kill_processes(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

int
ptrace_sandbox_get_arg(struct pt_sandbox* p_sandbox,
                       int arg,
                       unsigned long* p_out)
{
  (void) p_sandbox;
  (void) arg;
  (void) p_out;
  return -1;
}

int
ptrace_sandbox_get_socketcall_arg(struct pt_sandbox* p_sandbox,
                                  int arg,
                                  unsigned long* p_out)
{
  (void) p_sandbox;
  (void) arg;
  (void) p_out;
  return -1;
}

int
ptrace_sandbox_get_long(struct pt_sandbox* p_sandbox,
                        unsigned long ptr,
                        unsigned long* p_out)
{
  (void) p_sandbox;
  (void) ptr;
  (void) p_out;
  return -1;
}

int
ptrace_sandbox_get_buf(struct pt_sandbox* p_sandbox,
                       unsigned long ptr,
                       unsigned long len,
                       void* p_buf)
{
  (void) p_sandbox;
  (void) ptr;
  (void) len;
  (void) p_buf;
  return -1;
}

void
ptrace_sandbox_permit_readlink(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_brk(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_sleep(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_fchmod(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_chmod(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_fchown(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_mremap(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_ftruncate(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_socket(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_set_socket_validator(struct pt_sandbox* p_sandbox,
                                    ptrace_sandbox_validator_t val,
                                    void* p_arg)
{
  (void) p_sandbox;
  (void) val;
  (void) p_arg;
}

void
ptrace_sandbox_permit_bind(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_set_bind_validator(struct pt_sandbox* p_sandbox,
                                  ptrace_sandbox_validator_t val,
                                  void* p_arg)
{
  (void) p_sandbox;
  (void) val;
  (void) p_arg;
}

void
ptrace_sandbox_permit_connect(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_set_connect_validator(struct pt_sandbox* p_sandbox,
                                     ptrace_sandbox_validator_t val,
                                     void* p_arg)
{
  (void) p_sandbox;
  (void) val;
  (void) p_arg;
}

void
ptrace_sandbox_permit_listen(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_accept(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_permit_setsockopt(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_set_setsockopt_validator(struct pt_sandbox* p_sandbox,
                                        ptrace_sandbox_validator_t val,
                                        void* p_arg)
{
  (void) p_sandbox;
  (void) val;
  (void) p_arg;
}

void
ptrace_sandbox_permit_getsockopt(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

void
ptrace_sandbox_set_getsockopt_validator(struct pt_sandbox* p_sandbox,
                                        ptrace_sandbox_validator_t val,
                                        void* p_arg)
{
  (void) p_sandbox;
  (void) val;
  (void) p_arg;
}

void
ptrace_sandbox_permit_shutdown(struct pt_sandbox* p_sandbox)
{
  (void) p_sandbox;
}

#endif /* __linux__ && __i386__ */

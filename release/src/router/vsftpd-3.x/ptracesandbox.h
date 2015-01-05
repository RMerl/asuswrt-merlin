#ifndef VSF_PTRACESANDBOX_H
#define VSF_PTRACESANDBOX_H

/* Forward delcarations */
struct pt_sandbox;

typedef int (*ptrace_sandbox_validator_t)(struct pt_sandbox*, void*);

/* ptrace_sandbox_alloc()
 * PURPOSE
 * Allocates a ptrace sandbox object which is needed for the rest of the API.
 * RETURNS
 * NULL on failure, otherwise an opaque handle.
 * TODO
 * Only one per process supported at this time.
 */
struct pt_sandbox* ptrace_sandbox_alloc();

/* ptrace_sandbox_free()
 * PURPOSE
 * Frees the sandbox object.
 * PARAMETERS
 * p_sandbox        - the sandbox handle to free
 */
void ptrace_sandbox_free(struct pt_sandbox* p_sandbox);

/* ptrace_sandbox_launch_process()
 * PURPOSE
 * Launches a new process and attaches the sandbox to it when it stops.
 * PARAMETERS
 * p_sandbox        - the sandbox handle
 * p_func           - the function to call at the start of the new process
 * p_arg            - an argument to pass to the function
 * RETURNS
 * -1 on failure, otherwise an id for the created process. Not necessarily a
 * "pid", please treat is as opaque!
 * TODO
 * Only one call to this per sandbox object is supported at this time.
 */
int ptrace_sandbox_launch_process(struct pt_sandbox* p_sandbox,
                                  void (*p_func)(void*),
                                  void* p_arg);

/* ptrace_sandbox_run_processes()
 * PURPOSE
 * Runs sandboxed children until they exit or are killed.
 * PARAMETERS
 * p_sandbox        - the sandbox handle
 * RETURNS
 * 0 on normal exit or death of processes.
 * -1 if any process breached the policy.
 */
int ptrace_sandbox_run_processes(struct pt_sandbox* p_sandbox);

/* ptrace_sandbox_kill_processes()
 * PURPOSE
 * Safely kills off all sandboxed processes.
 * PARAMETERS
 * p_sandbox        - the sandbox handle
 */
void ptrace_sandbox_kill_processes(struct pt_sandbox* p_sandbox);

/* ptrace_sandbox_get_arg()
 * PURPOSE
 * Gets a syscall argument value for a process stopped in syscall entry.
 * PARAMETERS
 * p_sandbox        - the sandbox handle
 * arg              - the arg number to get (zero-based)
 * p_out            - the result is written here
 * RETURNS
 * 0 on success; otherwise it's a failure.
 */
int ptrace_sandbox_get_arg(struct pt_sandbox* p_sandbox,
                           int arg,
                           unsigned long* p_out);

/* ptrace_sandbox_get_socketcall_arg()
 * PURPOSE
 * Gets a syscall argument value for a process stopped in syscall entry, where
 * the system call is a socket-related one. On some architectures (e.g. i386,
 * socket calls are in fact multiplexed and store the arguments in a struct
 * in user space, hence the need for abstraction.
 * PARAMETERS
 * p_sandbox        - the sandbox handle
 * arg              - the arg number to get (zero-based)
 * p_out            - the result is written here
 * RETURNS
 * 0 on success; otherwise it's a failure.
 */
int ptrace_sandbox_get_socketcall_arg(struct pt_sandbox* p_sandbox,
                                      int arg,
                                      unsigned long* p_out);

/* ptrace_sandbox_get_long()
 * PURPOSE
 * Gets a long from the address space of the process stopped in syscall entry.
 * PARAMETERS
 * p_sandbox        - the sandbox handle
 * ptr              - the address to read the long from
 * p_out            - the result is written here
 * RETURNS
 * 0 on success; otherwise it's a failure.
 */
int ptrace_sandbox_get_long(struct pt_sandbox* p_sandbox,
                            unsigned long ptr,
                            unsigned long* p_out);

/* ptrace_sandbox_get_buf()
 * PURPOSE
 * Gets a piece of memory from the address space of the process stopped in
 * syscall entry.
 * PARAMETERS
 * p_sandbox        - the sandbox handle
 * ptr              - the address to read the buffer from
 * len              - the length of the buffer
 * p_buf            - the result is written here
 * RETURNS
 * 0 on success; otherwise it's a failure.
 */
int ptrace_sandbox_get_buf(struct pt_sandbox* p_sandbox,
                           unsigned long ptr,
                           unsigned long len,
                           void* p_buf);

/* ptrace_sandbox_attach_point()
 * PURPOSE
 * Used by the sandbox child code to stop and indicate it is ready to be
 * attached to.
 * NOTES
 * In the event of error trying to stop, the process is forcibly killed as a
 * security measure.
 */
void ptrace_sandbox_attach_point(void);

/* POLICY EDIT: permits exit() and exit_group() */
void ptrace_sandbox_permit_exit(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits read() */
void ptrace_sandbox_permit_read(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits write() */
void ptrace_sandbox_permit_write(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits sigaction() and rt_sigaction() */
void ptrace_sandbox_permit_sigaction(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits alarm() */
void ptrace_sandbox_permit_alarm(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits time() and gettimeofday() */
void ptrace_sandbox_permit_query_time(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits mmap2() (but not the MAP_SHARED flag) */
void ptrace_sandbox_permit_mmap(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits mprotect() */
void ptrace_sandbox_permit_mprotect(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits stat(), stat64(), lstat(), lstat64() */
void ptrace_sandbox_permit_file_stats(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits fstat(), fstat64() */
void ptrace_sandbox_permit_fd_stats(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits getcwd() */
void ptrace_sandbox_permit_getcwd(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits chdir() */
void ptrace_sandbox_permit_chdir(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits umask() */
void ptrace_sandbox_permit_umask(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits open(), except O_ASYNC and O_DIRECT. Only O_RDONLY
 * allowed unless writeable is 1
 */
void ptrace_sandbox_permit_open(struct pt_sandbox* p_sandbox, int writeable);
/* POLICY EDIT: permits close() */
void ptrace_sandbox_permit_close(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits getdents(), getdents64() */
void ptrace_sandbox_permit_getdents(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits fcntl(), fcntl64() for file locking, safe F_SETFL flag
 * setting (no O_ASYNC, O_DIRECT), F_SETOWN for your own pid and F_SETFD.
 */
void ptrace_sandbox_permit_fcntl(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits sendfile(), sendfile64() */
void ptrace_sandbox_permit_sendfile(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits lseek(), llseek() */
void ptrace_sandbox_permit_seek(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits select(), newselect() */
void ptrace_sandbox_permit_select(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits unlink() */
void ptrace_sandbox_permit_unlink(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits mkdir() */
void ptrace_sandbox_permit_mkdir(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits rmdir() */
void ptrace_sandbox_permit_rmdir(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits rename() */
void ptrace_sandbox_permit_rename(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits utime(), utimes() */
void ptrace_sandbox_permit_utime(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits sigreturn() */
void ptrace_sandbox_permit_sigreturn(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits recv() */
void ptrace_sandbox_permit_recv(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits readlink() */
void ptrace_sandbox_permit_readlink(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits brk() */
void ptrace_sandbox_permit_brk(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits nanosleep() */
void ptrace_sandbox_permit_sleep(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits fchmod() */
void ptrace_sandbox_permit_fchmod(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits chmod() */
void ptrace_sandbox_permit_chmod(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits fchown(), fchown32() */
void ptrace_sandbox_permit_fchown(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits mremap() */
void ptrace_sandbox_permit_mremap(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits ftruncate(), ftruncate64() */
void ptrace_sandbox_permit_ftruncate(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits socket() */
void ptrace_sandbox_permit_socket(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: set validator for socket() */
void ptrace_sandbox_set_socket_validator(struct pt_sandbox* p_sandbox,
                                         ptrace_sandbox_validator_t val,
                                         void* p_arg);
/* POLICY EDIT: permits bind() */
void ptrace_sandbox_permit_bind(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: set validator for bind() */
void ptrace_sandbox_set_bind_validator(struct pt_sandbox* p_sandbox,
                                       ptrace_sandbox_validator_t val,
                                       void* p_arg);
/* POLICY EDIT: permits connect() */
void ptrace_sandbox_permit_connect(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: set validator for connect() */
void ptrace_sandbox_set_connect_validator(struct pt_sandbox* p_sandbox,
                                          ptrace_sandbox_validator_t val,
                                          void* p_arg);
/* POLICY EDIT: permits listen() */
void ptrace_sandbox_permit_listen(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits accept() */
void ptrace_sandbox_permit_accept(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: permits setsockopt() */
void ptrace_sandbox_permit_setsockopt(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: set validator for setsockopt() */
void ptrace_sandbox_set_setsockopt_validator(struct pt_sandbox* p_sandbox,
                                             ptrace_sandbox_validator_t val,
                                             void* p_arg);
/* POLICY EDIT: permits getsockopt() */
void ptrace_sandbox_permit_getsockopt(struct pt_sandbox* p_sandbox);
/* POLICY EDIT: set validator for getsockopt() */
void ptrace_sandbox_set_getsockopt_validator(struct pt_sandbox* p_sandbox,
                                             ptrace_sandbox_validator_t val,
                                             void* p_arg);
/* POLICY EDIT: permits shutdown() */
void ptrace_sandbox_permit_shutdown(struct pt_sandbox* p_sandbox);

/* The traced process is unexpectedly dead; probably an external SIGKILL */
#define PTRACE_SANDBOX_ERR_DEAD              -1
/* An unexpected error from ptrace() */
#define PTRACE_SANDBOX_ERR_PTRACE            -2
/* An unexpected error from waitpid() */
#define PTRACE_SANDBOX_ERR_WAITPID           -3
/* An unexpected waitpid() status was returned */
#define PTRACE_SANDBOX_ERR_WAIT_STATUS       -4
/* A syscall not in the policy was attempted */
#define PTRACE_SANDBOX_ERR_POLICY_SYSCALL    -5
/* A "bad" syscall was attemped: out-of-bounds, 64-bit in a 32-bit child etc. */
#define PTRACE_SANDBOX_ERR_BAD_SYSCALL       -6
/* Bad arguments to a generally accepted syscall */
#define PTRACE_SANDBOX_ERR_POLICY_ARGS       -7
/* Abuse of our API */
#define PTRACE_SANDBOX_ERR_API_ABUSE_STOPIT  -8

#endif /* VSF_PTRACESANDBOX_H */


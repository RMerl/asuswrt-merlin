#ifndef VSF_SYSUTIL_H
#define VSF_SYSUTIL_H

#include <string.h>
#include "shared.h"

#define DENIED_DIR "...DENIED_DIR"

/* TODO: these functions need proper documenting! */

#ifndef VSF_FILESIZE_H
#include "filesize.h"
#endif

/* Return value queries */
int vsf_sysutil_retval_is_error(int retval);
enum EVSFSysUtilError
{
  kVSFSysUtilErrUnknown = 1,
  kVSFSysUtilErrADDRINUSE,
  kVSFSysUtilErrNOSYS,
  kVSFSysUtilErrINTR,
  kVSFSysUtilErrINVAL,
  kVSFSysUtilErrOPNOTSUPP,
  kVSFSysUtilErrACCES,
  kVSFSysUtilErrNOENT
};
enum EVSFSysUtilError vsf_sysutil_get_error(void);

/* Signal handling utility functions */
enum EVSFSysUtilSignal
{
  kVSFSysUtilSigALRM = 1,
  kVSFSysUtilSigTERM,
  kVSFSysUtilSigCHLD,
  kVSFSysUtilSigPIPE,
  kVSFSysUtilSigURG,
  kVSFSysUtilSigHUP
};
enum EVSFSysUtilInterruptContext
{
  kVSFSysUtilUnknown,
  kVSFSysUtilIO
};
typedef void (*vsf_sighandle_t)(void*);
typedef void (*vsf_async_sighandle_t)(int);
typedef void (*vsf_context_io_t)(int, int, void*);

void vsf_sysutil_install_null_sighandler(const enum EVSFSysUtilSignal sig);
void vsf_sysutil_install_sighandler(const enum EVSFSysUtilSignal,
                                    vsf_sighandle_t handler,
                                    void* p_private,
                                    int use_alarm);
void vsf_sysutil_install_async_sighandler(const enum EVSFSysUtilSignal sig,
                                          vsf_async_sighandle_t handler);
void vsf_sysutil_default_sig(const enum EVSFSysUtilSignal sig);
void vsf_sysutil_install_io_handler(vsf_context_io_t handler, void* p_private);
void vsf_sysutil_uninstall_io_handler(void);
void vsf_sysutil_check_pending_actions(
  const enum EVSFSysUtilInterruptContext context, int retval, int fd);
void vsf_sysutil_block_sig(const enum EVSFSysUtilSignal sig);
void vsf_sysutil_unblock_sig(const enum EVSFSysUtilSignal sig);

/* Alarm setting/clearing utility functions */
void vsf_sysutil_set_alarm(const unsigned int trigger_seconds);
void vsf_sysutil_clear_alarm(void);

/* Directory related things */
char* vsf_sysutil_getcwd(char* p_dest, const unsigned int buf_size);
int vsf_sysutil_mkdir(const char* p_dirname, const unsigned int mode);
int vsf_sysutil_rmdir(const char* p_dirname);
int vsf_sysutil_chdir(const char* p_dirname);
int vsf_sysutil_rename(const char* p_from, const char* p_to);

struct vsf_sysutil_dir;
struct vsf_sysutil_dir* vsf_sysutil_opendir(const char* p_dirname);
void vsf_sysutil_closedir(struct vsf_sysutil_dir* p_dir);
const char* vsf_sysutil_next_dirent(const char* session_user, const char *base_dir, struct vsf_sysutil_dir* p_dir);

/* File create/open/close etc. */
enum EVSFSysUtilOpenMode
{
  kVSFSysUtilOpenReadOnly = 1,
  kVSFSysUtilOpenWriteOnly,
  kVSFSysUtilOpenReadWrite
};
int vsf_sysutil_open_file(const char* p_filename,
                          const enum EVSFSysUtilOpenMode);
/* Fails if file already exists */
int vsf_sysutil_create_file_exclusive(const char* p_filename);
/* Creates file or appends if already exists */
int vsf_sysutil_create_or_open_file_append(const char* p_filename,
                                           unsigned int mode);
/* Creates or appends */
int vsf_sysutil_create_or_open_file(const char* p_filename, unsigned int mode);
void vsf_sysutil_dupfd2(int old_fd, int new_fd);
void vsf_sysutil_close(int fd);
int vsf_sysutil_close_failok(int fd);
int vsf_sysutil_unlink(const char* p_dead);
int vsf_sysutil_write_access(const char* p_filename);
void vsf_sysutil_ftruncate(int fd);

/* Reading and writing */
void vsf_sysutil_lseek_to(const int fd, filesize_t seek_pos);
void vsf_sysutil_lseek_end(const int fd);
filesize_t vsf_sysutil_get_file_offset(const int file_fd);
int vsf_sysutil_read(const int fd, void* p_buf, const unsigned int size);
int vsf_sysutil_write(const int fd, const void* p_buf,
                      const unsigned int size);
/* Reading and writing, with handling of interrupted system calls and partial
 * reads/writes. Slightly more usable than the standard UNIX API!
 */
int vsf_sysutil_read_loop(const int fd, void* p_buf, unsigned int size);
int vsf_sysutil_write_loop(const int fd, const void* p_buf, unsigned int size);

struct vsf_sysutil_statbuf;
int vsf_sysutil_stat(const char* p_name, struct vsf_sysutil_statbuf** p_ptr);
int vsf_sysutil_lstat(const char* p_name, struct vsf_sysutil_statbuf** p_ptr);
void vsf_sysutil_fstat(int fd, struct vsf_sysutil_statbuf** p_ptr);
void vsf_sysutil_dir_stat(const struct vsf_sysutil_dir* p_dir,
                          struct vsf_sysutil_statbuf** p_ptr);
int vsf_sysutil_statbuf_is_regfile(const struct vsf_sysutil_statbuf* p_stat);
int vsf_sysutil_statbuf_is_symlink(const struct vsf_sysutil_statbuf* p_stat);
int vsf_sysutil_statbuf_is_socket(const struct vsf_sysutil_statbuf* p_stat);
int vsf_sysutil_statbuf_is_dir(const struct vsf_sysutil_statbuf* p_stat);
filesize_t vsf_sysutil_statbuf_get_size(
  const struct vsf_sysutil_statbuf* p_stat);
const char* vsf_sysutil_statbuf_get_perms(
  const struct vsf_sysutil_statbuf* p_stat);
const char* vsf_sysutil_statbuf_get_date(
  const struct vsf_sysutil_statbuf* p_stat, int use_localtime, long curr_time);
const char* vsf_sysutil_statbuf_get_numeric_date(
  const struct vsf_sysutil_statbuf* p_stat, int use_localtime);
unsigned int vsf_sysutil_statbuf_get_links(
  const struct vsf_sysutil_statbuf* p_stat);
int vsf_sysutil_statbuf_get_uid(const struct vsf_sysutil_statbuf* p_stat);
int vsf_sysutil_statbuf_get_gid(const struct vsf_sysutil_statbuf* p_stat);
int vsf_sysutil_statbuf_is_readable_other(
  const struct vsf_sysutil_statbuf* p_stat);
const char* vsf_sysutil_statbuf_get_sortkey_mtime(
  const struct vsf_sysutil_statbuf* p_stat);

int vsf_sysutil_chmod(const char* p_filename, unsigned int mode);
void vsf_sysutil_fchown(const int fd, const int uid, const int gid);
void vsf_sysutil_fchmod(const int fd, unsigned int mode);
int vsf_sysutil_readlink(const char* p_filename, char* p_dest,
                         unsigned int bufsiz);

/* Get / unget various locks. Lock gets are blocking. Write locks are
 * exclusive; read locks are shared.
 */
int vsf_sysutil_lock_file_write(int fd);
int vsf_sysutil_lock_file_read(int fd);
void vsf_sysutil_unlock_file(int fd);

/* Mapping/unmapping */
enum EVSFSysUtilMapPermission
{
  kVSFSysUtilMapProtReadOnly = 1,
  kVSFSysUtilMapProtNone
};
void vsf_sysutil_memprotect(void* p_addr, unsigned int len,
                            const enum EVSFSysUtilMapPermission perm);
void vsf_sysutil_memunmap(void* p_start, unsigned int length);

/* Memory allocating/freeing */
void* vsf_sysutil_malloc(unsigned int size);
void* vsf_sysutil_realloc(void* p_ptr, unsigned int size);
void vsf_sysutil_free(void* p_ptr);

/* Process creation/exit/process handling */
unsigned int vsf_sysutil_getpid(void);
void vsf_sysutil_post_fork(void);
int vsf_sysutil_fork(void);
int vsf_sysutil_fork_failok(void);
void vsf_sysutil_exit(int exit_code);
struct vsf_sysutil_wait_retval
{
  int PRIVATE_HANDS_OFF_syscall_retval;
  int PRIVATE_HANDS_OFF_exit_status;
};
struct vsf_sysutil_wait_retval vsf_sysutil_wait(void);
int vsf_sysutil_wait_reap_one(void);
int vsf_sysutil_wait_get_retval(
  const struct vsf_sysutil_wait_retval* p_waitret);
int vsf_sysutil_wait_exited_normally(
  const struct vsf_sysutil_wait_retval* p_waitret);
int vsf_sysutil_wait_get_exitcode(
  const struct vsf_sysutil_wait_retval* p_waitret);

/* Various string functions */
unsigned int vsf_sysutil_strlen(const char* p_text);
char* vsf_sysutil_strdup(const char* p_str);
void vsf_sysutil_memclr(void* p_dest, unsigned int size);
void vsf_sysutil_memcpy(void* p_dest, const void* p_src,
                        const unsigned int size);
void vsf_sysutil_strcpy(char* p_dest, const char* p_src, unsigned int maxsize);
int vsf_sysutil_memcmp(const void* p_src1, const void* p_src2,
                       unsigned int size);
int vsf_sysutil_strcmp(const char* p_src1, const char* p_src2);
int vsf_sysutil_atoi(const char* p_str);
filesize_t vsf_sysutil_a_to_filesize_t(const char* p_str);
const char* vsf_sysutil_ulong_to_str(unsigned long the_ulong);
const char* vsf_sysutil_filesize_t_to_str(filesize_t the_filesize);
const char* vsf_sysutil_double_to_str(double the_double);
const char* vsf_sysutil_uint_to_octal(unsigned int the_uint);
unsigned int vsf_sysutil_octal_to_uint(const char* p_str);
int vsf_sysutil_toupper(int the_char);
int vsf_sysutil_isspace(int the_char);
int vsf_sysutil_isprint(int the_char);
int vsf_sysutil_isalnum(int the_char);
int vsf_sysutil_isdigit(int the_char);

/* Socket handling */
struct vsf_sysutil_sockaddr;
struct vsf_sysutil_socketpair_retval
{
  int socket_one;
  int socket_two;
};
void vsf_sysutil_sockaddr_alloc(struct vsf_sysutil_sockaddr** p_sockptr);
void vsf_sysutil_sockaddr_clear(struct vsf_sysutil_sockaddr** p_sockptr);
void vsf_sysutil_sockaddr_alloc_ipv4(struct vsf_sysutil_sockaddr** p_sockptr);
void vsf_sysutil_sockaddr_alloc_ipv6(struct vsf_sysutil_sockaddr** p_sockptr);
void vsf_sysutil_sockaddr_clone(
  struct vsf_sysutil_sockaddr** p_sockptr,
  const struct vsf_sysutil_sockaddr* p_src);
int vsf_sysutil_sockaddr_addr_equal(const struct vsf_sysutil_sockaddr* p1,
                                    const struct vsf_sysutil_sockaddr* p2);
int vsf_sysutil_sockaddr_is_ipv6(
  const struct vsf_sysutil_sockaddr* p_sockaddr);
void vsf_sysutil_sockaddr_set_ipv4addr(struct vsf_sysutil_sockaddr* p_sockptr,
                                       const unsigned char* p_raw);
void vsf_sysutil_sockaddr_set_ipv6addr(struct vsf_sysutil_sockaddr* p_sockptr,
                                       const unsigned char* p_raw);
void vsf_sysutil_sockaddr_set_any(struct vsf_sysutil_sockaddr* p_sockaddr);
unsigned short vsf_sysutil_sockaddr_get_port(
    const struct vsf_sysutil_sockaddr* p_sockptr);
void vsf_sysutil_sockaddr_set_port(struct vsf_sysutil_sockaddr* p_sockptr,
                                   unsigned short the_port);
int vsf_sysutil_is_port_reserved(unsigned short port);
int vsf_sysutil_get_ipsock(const struct vsf_sysutil_sockaddr* p_sockaddr);
unsigned int vsf_sysutil_get_ipaddr_size(void);
void* vsf_sysutil_sockaddr_get_raw_addr(
  struct vsf_sysutil_sockaddr* p_sockaddr);
const void* vsf_sysutil_sockaddr_ipv6_v4(
  const struct vsf_sysutil_sockaddr* p_sockaddr);
const void* vsf_sysutil_sockaddr_ipv4_v6(
  const struct vsf_sysutil_sockaddr* p_sockaddr);
int vsf_sysutil_get_ipv4_sock(void);
int vsf_sysutil_get_ipv6_sock(void);
struct vsf_sysutil_socketpair_retval
  vsf_sysutil_unix_stream_socketpair(void);
int vsf_sysutil_bind(int fd, const struct vsf_sysutil_sockaddr* p_sockptr);
int vsf_sysutil_listen(int fd, const unsigned int backlog);
void vsf_sysutil_getsockname(int fd, struct vsf_sysutil_sockaddr** p_sockptr);
void vsf_sysutil_getpeername(int fd, struct vsf_sysutil_sockaddr** p_sockptr);
int vsf_sysutil_accept_timeout(int fd, struct vsf_sysutil_sockaddr* p_sockaddr,
                               unsigned int wait_seconds);
int vsf_sysutil_connect_timeout(int fd,
                                const struct vsf_sysutil_sockaddr* p_sockaddr,
                                unsigned int wait_seconds);
void vsf_sysutil_dns_resolve(struct vsf_sysutil_sockaddr** p_sockptr,
                             const char* p_name);
/* Option setting on sockets */
void vsf_sysutil_activate_keepalive(int fd);
void vsf_sysutil_set_iptos_throughput(int fd);
void vsf_sysutil_activate_reuseaddr(int fd);
void vsf_sysutil_set_nodelay(int fd);
#ifdef RTCONFIG_SHP
void vsf_sysutil_set_lfp(int fd);
#else
static inline void vsf_sysutil_set_lfp(int fd) { }
#endif
void vsf_sysutil_activate_sigurg(int fd);
void vsf_sysutil_activate_oobinline(int fd);
void vsf_sysutil_activate_linger(int fd);
void vsf_sysutil_deactivate_linger_failok(int fd);
void vsf_sysutil_activate_noblock(int fd);
void vsf_sysutil_deactivate_noblock(int fd);
/* This does SHUT_RDWR */
void vsf_sysutil_shutdown_failok(int fd);
/* And this does SHUT_RD */
void vsf_sysutil_shutdown_read_failok(int fd);
int vsf_sysutil_recv_peek(const int fd, void* p_buf, unsigned int len);

const char* vsf_sysutil_inet_ntop(
  const struct vsf_sysutil_sockaddr* p_sockptr);
const char* vsf_sysutil_inet_ntoa(const void* p_raw_addr);
int vsf_sysutil_inet_aton(
  const char* p_text, struct vsf_sysutil_sockaddr* p_addr);

/* User database queries etc. */
struct vsf_sysutil_user;
struct vsf_sysutil_group;

struct vsf_sysutil_user* vsf_sysutil_getpwuid(const int uid);
struct vsf_sysutil_user* vsf_sysutil_getpwnam(const char* p_user);
const char* vsf_sysutil_user_getname(const struct vsf_sysutil_user* p_user);
const char* vsf_sysutil_user_get_homedir(
  const struct vsf_sysutil_user* p_user);
int vsf_sysutil_user_getuid(const struct vsf_sysutil_user* p_user);
int vsf_sysutil_user_getgid(const struct vsf_sysutil_user* p_user);

struct vsf_sysutil_group* vsf_sysutil_getgrgid(const int gid);
const char* vsf_sysutil_group_getname(const struct vsf_sysutil_group* p_group);

/* More random things */
unsigned int vsf_sysutil_getpagesize(void);
unsigned char vsf_sysutil_get_random_byte(void);
unsigned int vsf_sysutil_get_umask(void);
void vsf_sysutil_set_umask(unsigned int umask);
void vsf_sysutil_make_session_leader(void);
void vsf_sysutil_reopen_standard_fds(void);
void vsf_sysutil_tzset(void);
const char* vsf_sysutil_get_current_date(void);
void vsf_sysutil_qsort(void* p_base, unsigned int num_elem,
                       unsigned int elem_size,
                       int (*p_compar)(const void *, const void *));
char* vsf_sysutil_getenv(const char* p_var);
typedef void (*exitfunc_t)(void);
void vsf_sysutil_set_exit_func(exitfunc_t exitfunc);
int vsf_sysutil_getuid(void);

/* Syslogging (bah) */
void vsf_sysutil_openlog(int force);
void vsf_sysutil_syslog(const char* p_text, int severe);
void vsf_sysutil_closelog(void);

/* Credentials handling */
int vsf_sysutil_running_as_root(void);
void vsf_sysutil_setuid(const struct vsf_sysutil_user* p_user);
void vsf_sysutil_setgid(const struct vsf_sysutil_user* p_user);
void vsf_sysutil_setuid_numeric(int uid);
void vsf_sysutil_setgid_numeric(int gid);
int vsf_sysutil_geteuid(void);
int vsf_sysutil_getegid(void);
void vsf_sysutil_seteuid(const struct vsf_sysutil_user* p_user);
void vsf_sysutil_setegid(const struct vsf_sysutil_user* p_user);
void vsf_sysutil_seteuid_numeric(int uid);
void vsf_sysutil_setegid_numeric(int gid);
void vsf_sysutil_clear_supp_groups(void);
void vsf_sysutil_initgroups(const struct vsf_sysutil_user* p_user);
void vsf_sysutil_chroot(const char* p_root_path);

/* Time handling */
/* Do not call get_time_usec() without calling get_time_sec()
 * first otherwise you will get stale data.
 */
long vsf_sysutil_get_time_sec(void);
long vsf_sysutil_get_time_usec(void);
long vsf_sysutil_parse_time(const char* p_text);
void vsf_sysutil_sleep(double seconds);
int vsf_sysutil_setmodtime(const char* p_file, long the_time, int is_localtime);

/* Limits */
void vsf_sysutil_set_address_space_limit(unsigned long bytes);
void vsf_sysutil_set_no_fds(void);
void vsf_sysutil_set_no_procs(void);

int test_user(char *target, char *pattern);
#endif /* VSF_SYSUTIL_H */


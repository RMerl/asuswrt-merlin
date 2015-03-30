/* Automatically generated with
        gen_linux_syscalls.pl /usr/include/asm/unistd*.h
   Do not edit.
 */
static const struct {
  int syscall_num; const char *syscall_name;
} SYSCALLS_BY_NUMBER[] = {
#ifdef __NR__llseek
  { __NR__llseek, "_llseek" },
#endif
#ifdef __NR__newselect
  { __NR__newselect, "_newselect" },
#endif
#ifdef __NR__sysctl
  { __NR__sysctl, "_sysctl" },
#endif
#ifdef __NR_accept
  { __NR_accept, "accept" },
#endif
#ifdef __NR_accept4
  { __NR_accept4, "accept4" },
#endif
#ifdef __NR_access
  { __NR_access, "access" },
#endif
#ifdef __NR_acct
  { __NR_acct, "acct" },
#endif
#ifdef __NR_add_key
  { __NR_add_key, "add_key" },
#endif
#ifdef __NR_adjtimex
  { __NR_adjtimex, "adjtimex" },
#endif
#ifdef __NR_afs_syscall
  { __NR_afs_syscall, "afs_syscall" },
#endif
#ifdef __NR_alarm
  { __NR_alarm, "alarm" },
#endif
#ifdef __NR_arch_prctl
  { __NR_arch_prctl, "arch_prctl" },
#endif
#ifdef __NR_bdflush
  { __NR_bdflush, "bdflush" },
#endif
#ifdef __NR_bind
  { __NR_bind, "bind" },
#endif
#ifdef __NR_break
  { __NR_break, "break" },
#endif
#ifdef __NR_brk
  { __NR_brk, "brk" },
#endif
#ifdef __NR_capget
  { __NR_capget, "capget" },
#endif
#ifdef __NR_capset
  { __NR_capset, "capset" },
#endif
#ifdef __NR_chdir
  { __NR_chdir, "chdir" },
#endif
#ifdef __NR_chmod
  { __NR_chmod, "chmod" },
#endif
#ifdef __NR_chown
  { __NR_chown, "chown" },
#endif
#ifdef __NR_chown32
  { __NR_chown32, "chown32" },
#endif
#ifdef __NR_chroot
  { __NR_chroot, "chroot" },
#endif
#ifdef __NR_clock_adjtime
  { __NR_clock_adjtime, "clock_adjtime" },
#endif
#ifdef __NR_clock_getres
  { __NR_clock_getres, "clock_getres" },
#endif
#ifdef __NR_clock_gettime
  { __NR_clock_gettime, "clock_gettime" },
#endif
#ifdef __NR_clock_nanosleep
  { __NR_clock_nanosleep, "clock_nanosleep" },
#endif
#ifdef __NR_clock_settime
  { __NR_clock_settime, "clock_settime" },
#endif
#ifdef __NR_clone
  { __NR_clone, "clone" },
#endif
#ifdef __NR_close
  { __NR_close, "close" },
#endif
#ifdef __NR_connect
  { __NR_connect, "connect" },
#endif
#ifdef __NR_creat
  { __NR_creat, "creat" },
#endif
#ifdef __NR_create_module
  { __NR_create_module, "create_module" },
#endif
#ifdef __NR_delete_module
  { __NR_delete_module, "delete_module" },
#endif
#ifdef __NR_dup
  { __NR_dup, "dup" },
#endif
#ifdef __NR_dup2
  { __NR_dup2, "dup2" },
#endif
#ifdef __NR_dup3
  { __NR_dup3, "dup3" },
#endif
#ifdef __NR_epoll_create
  { __NR_epoll_create, "epoll_create" },
#endif
#ifdef __NR_epoll_create1
  { __NR_epoll_create1, "epoll_create1" },
#endif
#ifdef __NR_epoll_ctl
  { __NR_epoll_ctl, "epoll_ctl" },
#endif
#ifdef __NR_epoll_ctl_old
  { __NR_epoll_ctl_old, "epoll_ctl_old" },
#endif
#ifdef __NR_epoll_pwait
  { __NR_epoll_pwait, "epoll_pwait" },
#endif
#ifdef __NR_epoll_wait
  { __NR_epoll_wait, "epoll_wait" },
#endif
#ifdef __NR_epoll_wait_old
  { __NR_epoll_wait_old, "epoll_wait_old" },
#endif
#ifdef __NR_eventfd
  { __NR_eventfd, "eventfd" },
#endif
#ifdef __NR_eventfd2
  { __NR_eventfd2, "eventfd2" },
#endif
#ifdef __NR_execve
  { __NR_execve, "execve" },
#endif
#ifdef __NR_exit
  { __NR_exit, "exit" },
#endif
#ifdef __NR_exit_group
  { __NR_exit_group, "exit_group" },
#endif
#ifdef __NR_faccessat
  { __NR_faccessat, "faccessat" },
#endif
#ifdef __NR_fadvise64
  { __NR_fadvise64, "fadvise64" },
#endif
#ifdef __NR_fadvise64_64
  { __NR_fadvise64_64, "fadvise64_64" },
#endif
#ifdef __NR_fallocate
  { __NR_fallocate, "fallocate" },
#endif
#ifdef __NR_fanotify_init
  { __NR_fanotify_init, "fanotify_init" },
#endif
#ifdef __NR_fanotify_mark
  { __NR_fanotify_mark, "fanotify_mark" },
#endif
#ifdef __NR_fchdir
  { __NR_fchdir, "fchdir" },
#endif
#ifdef __NR_fchmod
  { __NR_fchmod, "fchmod" },
#endif
#ifdef __NR_fchmodat
  { __NR_fchmodat, "fchmodat" },
#endif
#ifdef __NR_fchown
  { __NR_fchown, "fchown" },
#endif
#ifdef __NR_fchown32
  { __NR_fchown32, "fchown32" },
#endif
#ifdef __NR_fchownat
  { __NR_fchownat, "fchownat" },
#endif
#ifdef __NR_fcntl
  { __NR_fcntl, "fcntl" },
#endif
#ifdef __NR_fcntl64
  { __NR_fcntl64, "fcntl64" },
#endif
#ifdef __NR_fdatasync
  { __NR_fdatasync, "fdatasync" },
#endif
#ifdef __NR_fgetxattr
  { __NR_fgetxattr, "fgetxattr" },
#endif
#ifdef __NR_finit_module
  { __NR_finit_module, "finit_module" },
#endif
#ifdef __NR_flistxattr
  { __NR_flistxattr, "flistxattr" },
#endif
#ifdef __NR_flock
  { __NR_flock, "flock" },
#endif
#ifdef __NR_fork
  { __NR_fork, "fork" },
#endif
#ifdef __NR_fremovexattr
  { __NR_fremovexattr, "fremovexattr" },
#endif
#ifdef __NR_fsetxattr
  { __NR_fsetxattr, "fsetxattr" },
#endif
#ifdef __NR_fstat
  { __NR_fstat, "fstat" },
#endif
#ifdef __NR_fstat64
  { __NR_fstat64, "fstat64" },
#endif
#ifdef __NR_fstatat64
  { __NR_fstatat64, "fstatat64" },
#endif
#ifdef __NR_fstatfs
  { __NR_fstatfs, "fstatfs" },
#endif
#ifdef __NR_fstatfs64
  { __NR_fstatfs64, "fstatfs64" },
#endif
#ifdef __NR_fsync
  { __NR_fsync, "fsync" },
#endif
#ifdef __NR_ftime
  { __NR_ftime, "ftime" },
#endif
#ifdef __NR_ftruncate
  { __NR_ftruncate, "ftruncate" },
#endif
#ifdef __NR_ftruncate64
  { __NR_ftruncate64, "ftruncate64" },
#endif
#ifdef __NR_futex
  { __NR_futex, "futex" },
#endif
#ifdef __NR_futimesat
  { __NR_futimesat, "futimesat" },
#endif
#ifdef __NR_get_kernel_syms
  { __NR_get_kernel_syms, "get_kernel_syms" },
#endif
#ifdef __NR_get_mempolicy
  { __NR_get_mempolicy, "get_mempolicy" },
#endif
#ifdef __NR_get_robust_list
  { __NR_get_robust_list, "get_robust_list" },
#endif
#ifdef __NR_get_thread_area
  { __NR_get_thread_area, "get_thread_area" },
#endif
#ifdef __NR_getcpu
  { __NR_getcpu, "getcpu" },
#endif
#ifdef __NR_getcwd
  { __NR_getcwd, "getcwd" },
#endif
#ifdef __NR_getdents
  { __NR_getdents, "getdents" },
#endif
#ifdef __NR_getdents64
  { __NR_getdents64, "getdents64" },
#endif
#ifdef __NR_getegid
  { __NR_getegid, "getegid" },
#endif
#ifdef __NR_getegid32
  { __NR_getegid32, "getegid32" },
#endif
#ifdef __NR_geteuid
  { __NR_geteuid, "geteuid" },
#endif
#ifdef __NR_geteuid32
  { __NR_geteuid32, "geteuid32" },
#endif
#ifdef __NR_getgid
  { __NR_getgid, "getgid" },
#endif
#ifdef __NR_getgid32
  { __NR_getgid32, "getgid32" },
#endif
#ifdef __NR_getgroups
  { __NR_getgroups, "getgroups" },
#endif
#ifdef __NR_getgroups32
  { __NR_getgroups32, "getgroups32" },
#endif
#ifdef __NR_getitimer
  { __NR_getitimer, "getitimer" },
#endif
#ifdef __NR_getpeername
  { __NR_getpeername, "getpeername" },
#endif
#ifdef __NR_getpgid
  { __NR_getpgid, "getpgid" },
#endif
#ifdef __NR_getpgrp
  { __NR_getpgrp, "getpgrp" },
#endif
#ifdef __NR_getpid
  { __NR_getpid, "getpid" },
#endif
#ifdef __NR_getpmsg
  { __NR_getpmsg, "getpmsg" },
#endif
#ifdef __NR_getppid
  { __NR_getppid, "getppid" },
#endif
#ifdef __NR_getpriority
  { __NR_getpriority, "getpriority" },
#endif
#ifdef __NR_getresgid
  { __NR_getresgid, "getresgid" },
#endif
#ifdef __NR_getresgid32
  { __NR_getresgid32, "getresgid32" },
#endif
#ifdef __NR_getresuid
  { __NR_getresuid, "getresuid" },
#endif
#ifdef __NR_getresuid32
  { __NR_getresuid32, "getresuid32" },
#endif
#ifdef __NR_getrlimit
  { __NR_getrlimit, "getrlimit" },
#endif
#ifdef __NR_getrusage
  { __NR_getrusage, "getrusage" },
#endif
#ifdef __NR_getsid
  { __NR_getsid, "getsid" },
#endif
#ifdef __NR_getsockname
  { __NR_getsockname, "getsockname" },
#endif
#ifdef __NR_getsockopt
  { __NR_getsockopt, "getsockopt" },
#endif
#ifdef __NR_gettid
  { __NR_gettid, "gettid" },
#endif
#ifdef __NR_gettimeofday
  { __NR_gettimeofday, "gettimeofday" },
#endif
#ifdef __NR_getuid
  { __NR_getuid, "getuid" },
#endif
#ifdef __NR_getuid32
  { __NR_getuid32, "getuid32" },
#endif
#ifdef __NR_getxattr
  { __NR_getxattr, "getxattr" },
#endif
#ifdef __NR_gtty
  { __NR_gtty, "gtty" },
#endif
#ifdef __NR_idle
  { __NR_idle, "idle" },
#endif
#ifdef __NR_init_module
  { __NR_init_module, "init_module" },
#endif
#ifdef __NR_inotify_add_watch
  { __NR_inotify_add_watch, "inotify_add_watch" },
#endif
#ifdef __NR_inotify_init
  { __NR_inotify_init, "inotify_init" },
#endif
#ifdef __NR_inotify_init1
  { __NR_inotify_init1, "inotify_init1" },
#endif
#ifdef __NR_inotify_rm_watch
  { __NR_inotify_rm_watch, "inotify_rm_watch" },
#endif
#ifdef __NR_io_cancel
  { __NR_io_cancel, "io_cancel" },
#endif
#ifdef __NR_io_destroy
  { __NR_io_destroy, "io_destroy" },
#endif
#ifdef __NR_io_getevents
  { __NR_io_getevents, "io_getevents" },
#endif
#ifdef __NR_io_setup
  { __NR_io_setup, "io_setup" },
#endif
#ifdef __NR_io_submit
  { __NR_io_submit, "io_submit" },
#endif
#ifdef __NR_ioctl
  { __NR_ioctl, "ioctl" },
#endif
#ifdef __NR_ioperm
  { __NR_ioperm, "ioperm" },
#endif
#ifdef __NR_iopl
  { __NR_iopl, "iopl" },
#endif
#ifdef __NR_ioprio_get
  { __NR_ioprio_get, "ioprio_get" },
#endif
#ifdef __NR_ioprio_set
  { __NR_ioprio_set, "ioprio_set" },
#endif
#ifdef __NR_ipc
  { __NR_ipc, "ipc" },
#endif
#ifdef __NR_kcmp
  { __NR_kcmp, "kcmp" },
#endif
#ifdef __NR_kexec_load
  { __NR_kexec_load, "kexec_load" },
#endif
#ifdef __NR_keyctl
  { __NR_keyctl, "keyctl" },
#endif
#ifdef __NR_kill
  { __NR_kill, "kill" },
#endif
#ifdef __NR_lchown
  { __NR_lchown, "lchown" },
#endif
#ifdef __NR_lchown32
  { __NR_lchown32, "lchown32" },
#endif
#ifdef __NR_lgetxattr
  { __NR_lgetxattr, "lgetxattr" },
#endif
#ifdef __NR_link
  { __NR_link, "link" },
#endif
#ifdef __NR_linkat
  { __NR_linkat, "linkat" },
#endif
#ifdef __NR_listen
  { __NR_listen, "listen" },
#endif
#ifdef __NR_listxattr
  { __NR_listxattr, "listxattr" },
#endif
#ifdef __NR_llistxattr
  { __NR_llistxattr, "llistxattr" },
#endif
#ifdef __NR_lock
  { __NR_lock, "lock" },
#endif
#ifdef __NR_lookup_dcookie
  { __NR_lookup_dcookie, "lookup_dcookie" },
#endif
#ifdef __NR_lremovexattr
  { __NR_lremovexattr, "lremovexattr" },
#endif
#ifdef __NR_lseek
  { __NR_lseek, "lseek" },
#endif
#ifdef __NR_lsetxattr
  { __NR_lsetxattr, "lsetxattr" },
#endif
#ifdef __NR_lstat
  { __NR_lstat, "lstat" },
#endif
#ifdef __NR_lstat64
  { __NR_lstat64, "lstat64" },
#endif
#ifdef __NR_madvise
  { __NR_madvise, "madvise" },
#endif
#ifdef __NR_mbind
  { __NR_mbind, "mbind" },
#endif
#ifdef __NR_migrate_pages
  { __NR_migrate_pages, "migrate_pages" },
#endif
#ifdef __NR_mincore
  { __NR_mincore, "mincore" },
#endif
#ifdef __NR_mkdir
  { __NR_mkdir, "mkdir" },
#endif
#ifdef __NR_mkdirat
  { __NR_mkdirat, "mkdirat" },
#endif
#ifdef __NR_mknod
  { __NR_mknod, "mknod" },
#endif
#ifdef __NR_mknodat
  { __NR_mknodat, "mknodat" },
#endif
#ifdef __NR_mlock
  { __NR_mlock, "mlock" },
#endif
#ifdef __NR_mlockall
  { __NR_mlockall, "mlockall" },
#endif
#ifdef __NR_mmap
  { __NR_mmap, "mmap" },
#endif
#ifdef __NR_mmap2
  { __NR_mmap2, "mmap2" },
#endif
#ifdef __NR_modify_ldt
  { __NR_modify_ldt, "modify_ldt" },
#endif
#ifdef __NR_mount
  { __NR_mount, "mount" },
#endif
#ifdef __NR_move_pages
  { __NR_move_pages, "move_pages" },
#endif
#ifdef __NR_mprotect
  { __NR_mprotect, "mprotect" },
#endif
#ifdef __NR_mpx
  { __NR_mpx, "mpx" },
#endif
#ifdef __NR_mq_getsetattr
  { __NR_mq_getsetattr, "mq_getsetattr" },
#endif
#ifdef __NR_mq_notify
  { __NR_mq_notify, "mq_notify" },
#endif
#ifdef __NR_mq_open
  { __NR_mq_open, "mq_open" },
#endif
#ifdef __NR_mq_timedreceive
  { __NR_mq_timedreceive, "mq_timedreceive" },
#endif
#ifdef __NR_mq_timedsend
  { __NR_mq_timedsend, "mq_timedsend" },
#endif
#ifdef __NR_mq_unlink
  { __NR_mq_unlink, "mq_unlink" },
#endif
#ifdef __NR_mremap
  { __NR_mremap, "mremap" },
#endif
#ifdef __NR_msgctl
  { __NR_msgctl, "msgctl" },
#endif
#ifdef __NR_msgget
  { __NR_msgget, "msgget" },
#endif
#ifdef __NR_msgrcv
  { __NR_msgrcv, "msgrcv" },
#endif
#ifdef __NR_msgsnd
  { __NR_msgsnd, "msgsnd" },
#endif
#ifdef __NR_msync
  { __NR_msync, "msync" },
#endif
#ifdef __NR_munlock
  { __NR_munlock, "munlock" },
#endif
#ifdef __NR_munlockall
  { __NR_munlockall, "munlockall" },
#endif
#ifdef __NR_munmap
  { __NR_munmap, "munmap" },
#endif
#ifdef __NR_name_to_handle_at
  { __NR_name_to_handle_at, "name_to_handle_at" },
#endif
#ifdef __NR_nanosleep
  { __NR_nanosleep, "nanosleep" },
#endif
#ifdef __NR_newfstatat
  { __NR_newfstatat, "newfstatat" },
#endif
#ifdef __NR_nfsservctl
  { __NR_nfsservctl, "nfsservctl" },
#endif
#ifdef __NR_nice
  { __NR_nice, "nice" },
#endif
#ifdef __NR_oldfstat
  { __NR_oldfstat, "oldfstat" },
#endif
#ifdef __NR_oldlstat
  { __NR_oldlstat, "oldlstat" },
#endif
#ifdef __NR_oldolduname
  { __NR_oldolduname, "oldolduname" },
#endif
#ifdef __NR_oldstat
  { __NR_oldstat, "oldstat" },
#endif
#ifdef __NR_olduname
  { __NR_olduname, "olduname" },
#endif
#ifdef __NR_open
  { __NR_open, "open" },
#endif
#ifdef __NR_open_by_handle_at
  { __NR_open_by_handle_at, "open_by_handle_at" },
#endif
#ifdef __NR_openat
  { __NR_openat, "openat" },
#endif
#ifdef __NR_pause
  { __NR_pause, "pause" },
#endif
#ifdef __NR_perf_event_open
  { __NR_perf_event_open, "perf_event_open" },
#endif
#ifdef __NR_personality
  { __NR_personality, "personality" },
#endif
#ifdef __NR_pipe
  { __NR_pipe, "pipe" },
#endif
#ifdef __NR_pipe2
  { __NR_pipe2, "pipe2" },
#endif
#ifdef __NR_pivot_root
  { __NR_pivot_root, "pivot_root" },
#endif
#ifdef __NR_poll
  { __NR_poll, "poll" },
#endif
#ifdef __NR_ppoll
  { __NR_ppoll, "ppoll" },
#endif
#ifdef __NR_prctl
  { __NR_prctl, "prctl" },
#endif
#ifdef __NR_pread64
  { __NR_pread64, "pread64" },
#endif
#ifdef __NR_preadv
  { __NR_preadv, "preadv" },
#endif
#ifdef __NR_prlimit64
  { __NR_prlimit64, "prlimit64" },
#endif
#ifdef __NR_process_vm_readv
  { __NR_process_vm_readv, "process_vm_readv" },
#endif
#ifdef __NR_process_vm_writev
  { __NR_process_vm_writev, "process_vm_writev" },
#endif
#ifdef __NR_prof
  { __NR_prof, "prof" },
#endif
#ifdef __NR_profil
  { __NR_profil, "profil" },
#endif
#ifdef __NR_pselect6
  { __NR_pselect6, "pselect6" },
#endif
#ifdef __NR_ptrace
  { __NR_ptrace, "ptrace" },
#endif
#ifdef __NR_putpmsg
  { __NR_putpmsg, "putpmsg" },
#endif
#ifdef __NR_pwrite64
  { __NR_pwrite64, "pwrite64" },
#endif
#ifdef __NR_pwritev
  { __NR_pwritev, "pwritev" },
#endif
#ifdef __NR_query_module
  { __NR_query_module, "query_module" },
#endif
#ifdef __NR_quotactl
  { __NR_quotactl, "quotactl" },
#endif
#ifdef __NR_read
  { __NR_read, "read" },
#endif
#ifdef __NR_readahead
  { __NR_readahead, "readahead" },
#endif
#ifdef __NR_readdir
  { __NR_readdir, "readdir" },
#endif
#ifdef __NR_readlink
  { __NR_readlink, "readlink" },
#endif
#ifdef __NR_readlinkat
  { __NR_readlinkat, "readlinkat" },
#endif
#ifdef __NR_readv
  { __NR_readv, "readv" },
#endif
#ifdef __NR_reboot
  { __NR_reboot, "reboot" },
#endif
#ifdef __NR_recvfrom
  { __NR_recvfrom, "recvfrom" },
#endif
#ifdef __NR_recvmmsg
  { __NR_recvmmsg, "recvmmsg" },
#endif
#ifdef __NR_recvmsg
  { __NR_recvmsg, "recvmsg" },
#endif
#ifdef __NR_remap_file_pages
  { __NR_remap_file_pages, "remap_file_pages" },
#endif
#ifdef __NR_removexattr
  { __NR_removexattr, "removexattr" },
#endif
#ifdef __NR_rename
  { __NR_rename, "rename" },
#endif
#ifdef __NR_renameat
  { __NR_renameat, "renameat" },
#endif
#ifdef __NR_request_key
  { __NR_request_key, "request_key" },
#endif
#ifdef __NR_restart_syscall
  { __NR_restart_syscall, "restart_syscall" },
#endif
#ifdef __NR_rmdir
  { __NR_rmdir, "rmdir" },
#endif
#ifdef __NR_rt_sigaction
  { __NR_rt_sigaction, "rt_sigaction" },
#endif
#ifdef __NR_rt_sigpending
  { __NR_rt_sigpending, "rt_sigpending" },
#endif
#ifdef __NR_rt_sigprocmask
  { __NR_rt_sigprocmask, "rt_sigprocmask" },
#endif
#ifdef __NR_rt_sigqueueinfo
  { __NR_rt_sigqueueinfo, "rt_sigqueueinfo" },
#endif
#ifdef __NR_rt_sigreturn
  { __NR_rt_sigreturn, "rt_sigreturn" },
#endif
#ifdef __NR_rt_sigsuspend
  { __NR_rt_sigsuspend, "rt_sigsuspend" },
#endif
#ifdef __NR_rt_sigtimedwait
  { __NR_rt_sigtimedwait, "rt_sigtimedwait" },
#endif
#ifdef __NR_rt_tgsigqueueinfo
  { __NR_rt_tgsigqueueinfo, "rt_tgsigqueueinfo" },
#endif
#ifdef __NR_sched_get_priority_max
  { __NR_sched_get_priority_max, "sched_get_priority_max" },
#endif
#ifdef __NR_sched_get_priority_min
  { __NR_sched_get_priority_min, "sched_get_priority_min" },
#endif
#ifdef __NR_sched_getaffinity
  { __NR_sched_getaffinity, "sched_getaffinity" },
#endif
#ifdef __NR_sched_getparam
  { __NR_sched_getparam, "sched_getparam" },
#endif
#ifdef __NR_sched_getscheduler
  { __NR_sched_getscheduler, "sched_getscheduler" },
#endif
#ifdef __NR_sched_rr_get_interval
  { __NR_sched_rr_get_interval, "sched_rr_get_interval" },
#endif
#ifdef __NR_sched_setaffinity
  { __NR_sched_setaffinity, "sched_setaffinity" },
#endif
#ifdef __NR_sched_setparam
  { __NR_sched_setparam, "sched_setparam" },
#endif
#ifdef __NR_sched_setscheduler
  { __NR_sched_setscheduler, "sched_setscheduler" },
#endif
#ifdef __NR_sched_yield
  { __NR_sched_yield, "sched_yield" },
#endif
#ifdef __NR_security
  { __NR_security, "security" },
#endif
#ifdef __NR_select
  { __NR_select, "select" },
#endif
#ifdef __NR_semctl
  { __NR_semctl, "semctl" },
#endif
#ifdef __NR_semget
  { __NR_semget, "semget" },
#endif
#ifdef __NR_semop
  { __NR_semop, "semop" },
#endif
#ifdef __NR_semtimedop
  { __NR_semtimedop, "semtimedop" },
#endif
#ifdef __NR_sendfile
  { __NR_sendfile, "sendfile" },
#endif
#ifdef __NR_sendfile64
  { __NR_sendfile64, "sendfile64" },
#endif
#ifdef __NR_sendmmsg
  { __NR_sendmmsg, "sendmmsg" },
#endif
#ifdef __NR_sendmsg
  { __NR_sendmsg, "sendmsg" },
#endif
#ifdef __NR_sendto
  { __NR_sendto, "sendto" },
#endif
#ifdef __NR_set_mempolicy
  { __NR_set_mempolicy, "set_mempolicy" },
#endif
#ifdef __NR_set_robust_list
  { __NR_set_robust_list, "set_robust_list" },
#endif
#ifdef __NR_set_thread_area
  { __NR_set_thread_area, "set_thread_area" },
#endif
#ifdef __NR_set_tid_address
  { __NR_set_tid_address, "set_tid_address" },
#endif
#ifdef __NR_setdomainname
  { __NR_setdomainname, "setdomainname" },
#endif
#ifdef __NR_setfsgid
  { __NR_setfsgid, "setfsgid" },
#endif
#ifdef __NR_setfsgid32
  { __NR_setfsgid32, "setfsgid32" },
#endif
#ifdef __NR_setfsuid
  { __NR_setfsuid, "setfsuid" },
#endif
#ifdef __NR_setfsuid32
  { __NR_setfsuid32, "setfsuid32" },
#endif
#ifdef __NR_setgid
  { __NR_setgid, "setgid" },
#endif
#ifdef __NR_setgid32
  { __NR_setgid32, "setgid32" },
#endif
#ifdef __NR_setgroups
  { __NR_setgroups, "setgroups" },
#endif
#ifdef __NR_setgroups32
  { __NR_setgroups32, "setgroups32" },
#endif
#ifdef __NR_sethostname
  { __NR_sethostname, "sethostname" },
#endif
#ifdef __NR_setitimer
  { __NR_setitimer, "setitimer" },
#endif
#ifdef __NR_setns
  { __NR_setns, "setns" },
#endif
#ifdef __NR_setpgid
  { __NR_setpgid, "setpgid" },
#endif
#ifdef __NR_setpriority
  { __NR_setpriority, "setpriority" },
#endif
#ifdef __NR_setregid
  { __NR_setregid, "setregid" },
#endif
#ifdef __NR_setregid32
  { __NR_setregid32, "setregid32" },
#endif
#ifdef __NR_setresgid
  { __NR_setresgid, "setresgid" },
#endif
#ifdef __NR_setresgid32
  { __NR_setresgid32, "setresgid32" },
#endif
#ifdef __NR_setresuid
  { __NR_setresuid, "setresuid" },
#endif
#ifdef __NR_setresuid32
  { __NR_setresuid32, "setresuid32" },
#endif
#ifdef __NR_setreuid
  { __NR_setreuid, "setreuid" },
#endif
#ifdef __NR_setreuid32
  { __NR_setreuid32, "setreuid32" },
#endif
#ifdef __NR_setrlimit
  { __NR_setrlimit, "setrlimit" },
#endif
#ifdef __NR_setsid
  { __NR_setsid, "setsid" },
#endif
#ifdef __NR_setsockopt
  { __NR_setsockopt, "setsockopt" },
#endif
#ifdef __NR_settimeofday
  { __NR_settimeofday, "settimeofday" },
#endif
#ifdef __NR_setuid
  { __NR_setuid, "setuid" },
#endif
#ifdef __NR_setuid32
  { __NR_setuid32, "setuid32" },
#endif
#ifdef __NR_setxattr
  { __NR_setxattr, "setxattr" },
#endif
#ifdef __NR_sgetmask
  { __NR_sgetmask, "sgetmask" },
#endif
#ifdef __NR_shmat
  { __NR_shmat, "shmat" },
#endif
#ifdef __NR_shmctl
  { __NR_shmctl, "shmctl" },
#endif
#ifdef __NR_shmdt
  { __NR_shmdt, "shmdt" },
#endif
#ifdef __NR_shmget
  { __NR_shmget, "shmget" },
#endif
#ifdef __NR_shutdown
  { __NR_shutdown, "shutdown" },
#endif
#ifdef __NR_sigaction
  { __NR_sigaction, "sigaction" },
#endif
#ifdef __NR_sigaltstack
  { __NR_sigaltstack, "sigaltstack" },
#endif
#ifdef __NR_signal
  { __NR_signal, "signal" },
#endif
#ifdef __NR_signalfd
  { __NR_signalfd, "signalfd" },
#endif
#ifdef __NR_signalfd4
  { __NR_signalfd4, "signalfd4" },
#endif
#ifdef __NR_sigpending
  { __NR_sigpending, "sigpending" },
#endif
#ifdef __NR_sigprocmask
  { __NR_sigprocmask, "sigprocmask" },
#endif
#ifdef __NR_sigreturn
  { __NR_sigreturn, "sigreturn" },
#endif
#ifdef __NR_sigsuspend
  { __NR_sigsuspend, "sigsuspend" },
#endif
#ifdef __NR_socket
  { __NR_socket, "socket" },
#endif
#ifdef __NR_socketcall
  { __NR_socketcall, "socketcall" },
#endif
#ifdef __NR_socketpair
  { __NR_socketpair, "socketpair" },
#endif
#ifdef __NR_splice
  { __NR_splice, "splice" },
#endif
#ifdef __NR_ssetmask
  { __NR_ssetmask, "ssetmask" },
#endif
#ifdef __NR_stat
  { __NR_stat, "stat" },
#endif
#ifdef __NR_stat64
  { __NR_stat64, "stat64" },
#endif
#ifdef __NR_statfs
  { __NR_statfs, "statfs" },
#endif
#ifdef __NR_statfs64
  { __NR_statfs64, "statfs64" },
#endif
#ifdef __NR_stime
  { __NR_stime, "stime" },
#endif
#ifdef __NR_stty
  { __NR_stty, "stty" },
#endif
#ifdef __NR_swapoff
  { __NR_swapoff, "swapoff" },
#endif
#ifdef __NR_swapon
  { __NR_swapon, "swapon" },
#endif
#ifdef __NR_symlink
  { __NR_symlink, "symlink" },
#endif
#ifdef __NR_symlinkat
  { __NR_symlinkat, "symlinkat" },
#endif
#ifdef __NR_sync
  { __NR_sync, "sync" },
#endif
#ifdef __NR_sync_file_range
  { __NR_sync_file_range, "sync_file_range" },
#endif
#ifdef __NR_syncfs
  { __NR_syncfs, "syncfs" },
#endif
#ifdef __NR_sysfs
  { __NR_sysfs, "sysfs" },
#endif
#ifdef __NR_sysinfo
  { __NR_sysinfo, "sysinfo" },
#endif
#ifdef __NR_syslog
  { __NR_syslog, "syslog" },
#endif
#ifdef __NR_tee
  { __NR_tee, "tee" },
#endif
#ifdef __NR_tgkill
  { __NR_tgkill, "tgkill" },
#endif
#ifdef __NR_time
  { __NR_time, "time" },
#endif
#ifdef __NR_timer_create
  { __NR_timer_create, "timer_create" },
#endif
#ifdef __NR_timer_delete
  { __NR_timer_delete, "timer_delete" },
#endif
#ifdef __NR_timer_getoverrun
  { __NR_timer_getoverrun, "timer_getoverrun" },
#endif
#ifdef __NR_timer_gettime
  { __NR_timer_gettime, "timer_gettime" },
#endif
#ifdef __NR_timer_settime
  { __NR_timer_settime, "timer_settime" },
#endif
#ifdef __NR_timerfd_create
  { __NR_timerfd_create, "timerfd_create" },
#endif
#ifdef __NR_timerfd_gettime
  { __NR_timerfd_gettime, "timerfd_gettime" },
#endif
#ifdef __NR_timerfd_settime
  { __NR_timerfd_settime, "timerfd_settime" },
#endif
#ifdef __NR_times
  { __NR_times, "times" },
#endif
#ifdef __NR_tkill
  { __NR_tkill, "tkill" },
#endif
#ifdef __NR_truncate
  { __NR_truncate, "truncate" },
#endif
#ifdef __NR_truncate64
  { __NR_truncate64, "truncate64" },
#endif
#ifdef __NR_tuxcall
  { __NR_tuxcall, "tuxcall" },
#endif
#ifdef __NR_ugetrlimit
  { __NR_ugetrlimit, "ugetrlimit" },
#endif
#ifdef __NR_ulimit
  { __NR_ulimit, "ulimit" },
#endif
#ifdef __NR_umask
  { __NR_umask, "umask" },
#endif
#ifdef __NR_umount
  { __NR_umount, "umount" },
#endif
#ifdef __NR_umount2
  { __NR_umount2, "umount2" },
#endif
#ifdef __NR_uname
  { __NR_uname, "uname" },
#endif
#ifdef __NR_unlink
  { __NR_unlink, "unlink" },
#endif
#ifdef __NR_unlinkat
  { __NR_unlinkat, "unlinkat" },
#endif
#ifdef __NR_unshare
  { __NR_unshare, "unshare" },
#endif
#ifdef __NR_uselib
  { __NR_uselib, "uselib" },
#endif
#ifdef __NR_ustat
  { __NR_ustat, "ustat" },
#endif
#ifdef __NR_utime
  { __NR_utime, "utime" },
#endif
#ifdef __NR_utimensat
  { __NR_utimensat, "utimensat" },
#endif
#ifdef __NR_utimes
  { __NR_utimes, "utimes" },
#endif
#ifdef __NR_vfork
  { __NR_vfork, "vfork" },
#endif
#ifdef __NR_vhangup
  { __NR_vhangup, "vhangup" },
#endif
#ifdef __NR_vm86
  { __NR_vm86, "vm86" },
#endif
#ifdef __NR_vm86old
  { __NR_vm86old, "vm86old" },
#endif
#ifdef __NR_vmsplice
  { __NR_vmsplice, "vmsplice" },
#endif
#ifdef __NR_vserver
  { __NR_vserver, "vserver" },
#endif
#ifdef __NR_wait4
  { __NR_wait4, "wait4" },
#endif
#ifdef __NR_waitid
  { __NR_waitid, "waitid" },
#endif
#ifdef __NR_waitpid
  { __NR_waitpid, "waitpid" },
#endif
#ifdef __NR_write
  { __NR_write, "write" },
#endif
#ifdef __NR_writev
  { __NR_writev, "writev" },
#endif
  {0, NULL}
};


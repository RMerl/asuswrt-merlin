/*
 * Copyright (c) 2004-2009 Atmel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id$
 */

	{ 0,	0,	sys_setup,		"setup"		}, /* 0 */
	{ 1,	TP,	sys_exit,		"_exit"		}, /* 1 */
	{ 0,	TP,	sys_fork,		"fork"		}, /* 2 */
	{ 3,	TD,	sys_read,		"read"		}, /* 3 */
	{ 3,	TD,	sys_write,		"write"		}, /* 4 */
	{ 3,	TD|TF,	sys_open,		"open"		}, /* 5 */
	{ 1,	TD,	sys_close,		"close"		}, /* 6 */
	{ 1,	0,	sys_umask,		"umask"		}, /* 7 */
	{ 2,	TD|TF,	sys_creat,		"creat"		}, /* 8 */
	{ 2,	TF,	sys_link,		"link"		}, /* 9 */
	{ 1,	TF,	sys_unlink,		"unlink"	}, /* 10 */
	{ 3,	TF|TP,	sys_execve,		"execve"	}, /* 11 */
	{ 1,	TF,	sys_chdir,		"chdir"		}, /* 12 */
	{ 1,	0,	sys_time,		"time"		}, /* 13 */
	{ 3,	TF,	sys_mknod,		"mknod"		}, /* 14 */
	{ 2,	TF,	sys_chmod,		"chmod"		}, /* 15 */
	{ 3,	TF,	sys_chown,		"chown"		}, /* 16 */
	{ 3,	TF,	sys_chown,		"lchown"	}, /* 17 */
	{ 3,	TD,	sys_lseek,		"lseek"		}, /* 18 */
	{ 5,	TD,	sys_llseek,		"_llseek"	}, /* 19 */
	{ 0,	0,	sys_getpid,		"getpid"	}, /* 20 */
	{ 5,	TF,	sys_mount,		"mount"		}, /* 21 */
	{ 2,	TF,	sys_umount,		"umount"	}, /* 22 */
	{ 1,	0,	sys_setuid,		"setuid"	}, /* 23 */
	{ 0,	NF,	sys_getuid,		"getuid"	}, /* 24 */
	{ 1,	0,	sys_stime,		"stime"		}, /* 25 */
	{ 4,	0,	sys_ptrace,		"ptrace"	}, /* 26 */
	{ 1,	0,	sys_alarm,		"alarm"		}, /* 27 */
	{ 0,	TS,	sys_pause,		"pause"		}, /* 28 */
	{ 2,	TF,	sys_utime,		"utime"		}, /* 29 */
	{ 2,	TF,	sys_stat,		"stat"		}, /* 30 */
	{ 2,	TD,	sys_fstat,		"fstat"		}, /* 31 */
	{ 2,	TF,	sys_lstat,		"lstat"		}, /* 32 */
	{ 2,	TF,	sys_access,		"access"	}, /* 33 */
	{ 1,	TF,	sys_chroot,		"chroot"	}, /* 34 */
	{ 0,	0,	sys_sync,		"sync"		}, /* 35 */
	{ 1,	TD,	sys_fsync,		"fsync"		}, /* 36 */
	{ 2,	TS,	sys_kill,		"kill"		}, /* 37 */
	{ 2,	TF,	sys_rename,		"rename"	}, /* 38 */
	{ 2,	TF,	sys_mkdir,		"mkdir"		}, /* 39 */
	{ 1,	TF,	sys_rmdir,		"rmdir"		}, /* 40 */
	{ 1,	TD,	sys_dup,		"dup"		}, /* 41 */
	{ 1,	TD,	sys_pipe,		"pipe"		}, /* 42 */
	{ 1,	0,	sys_times,		"times"		}, /* 43 */
	{ 5,	TP,	sys_clone,		"clone"		}, /* 44 */
	{ 1,	0,	sys_brk,		"brk"		}, /* 45 */
	{ 1,	0,	sys_setgid,		"setgid"	}, /* 46 */
	{ 0,	NF,	sys_getgid,		"getgid"	}, /* 47 */
	{ 2,	TF,	sys_getcwd,		"getcwd"	}, /* 48 */
	{ 0,	NF,	sys_geteuid,		"geteuid"	}, /* 49 */
	{ 0,	NF,	sys_getegid,		"getegid"	}, /* 50 */
	{ 1,	TF,	sys_acct,		"acct"		}, /* 51 */
	{ 1,	NF,	sys_setfsuid,		"setfsuid"	}, /* 52 */
	{ 1,	NF,	sys_setfsgid,		"setfsgid"	}, /* 53 */
	{ 3,	TD,	sys_ioctl,		"ioctl"		}, /* 54 */
	{ 3,	TD,	sys_fcntl,		"fcntl"		}, /* 55 */
	{ 2,	0,	sys_setpgid,		"setpgid"	}, /* 56 */
	{ 5,	0,	sys_mremap,		"mremap"	}, /* 57 */
	{ 3,	0,	sys_setresuid,		"setresuid"	}, /* 58 */
	{ 3,	0,	sys_getresuid,		"getresuid"	}, /* 59 */
	{ 2,	0,	sys_setreuid,		"setreuid"	}, /* 60 */
	{ 2,	0,	sys_setregid,		"setregid"	}, /* 61 */
	{ 2,	0,	sys_ustat,		"ustat"		}, /* 62 */
	{ 2,	TD,	sys_dup2,		"dup2"		}, /* 63 */
	{ 0,	0,	sys_getppid,		"getppid"	}, /* 64 */
	{ 0,	0,	sys_getpgrp,		"getpgrp"	}, /* 65 */
	{ 0,	0,	sys_setsid,		"setsid"	}, /* 66 */
	{ 4,	TS,	sys_rt_sigaction,	"rt_sigaction"  }, /* 67 */
	{ 1,	TS,	printargs,		"rt_sigreturn"	}, /* 68 */
	{ 4,	TS,	sys_rt_sigprocmask,	"rt_sigprocmask"}, /* 69 */
	{ 2,	TS,	sys_rt_sigpending,	"rt_sigpending"	}, /* 70 */
	{ 4,	TS,	sys_rt_sigtimedwait,	"rt_sigtimedwait"}, /* 71 */
	{ 3,	TS,	sys_rt_sigqueueinfo,    "rt_sigqueueinfo"}, /* 72 */
	{ 2,	TS,	sys_rt_sigsuspend,	"rt_sigsuspend"	}, /* 73 */
	{ 2,	0,	sys_sethostname,	"sethostname"	}, /* 74 */
	{ 2,	0,	sys_setrlimit,		"setrlimit"	}, /* 75 */
	{ 2,	0,	sys_getrlimit,		"old_getrlimit"	}, /* 76 */
	{ 2,	0,	sys_getrusage,		"getrusage"	}, /* 77 */
	{ 2,	0,	sys_gettimeofday,	"gettimeofday"	}, /* 78 */
	{ 2,	0,	sys_settimeofday,	"settimeofday"	}, /* 79 */
	{ 2,	0,	sys_getgroups,		"getgroups"	}, /* 80 */
	{ 2,	0,	sys_setgroups,		"setgroups"	}, /* 81 */
	{ 5,	TD,	sys_select,		"select"	}, /* 82 */
	{ 2,	TF,	sys_symlink,		"symlink"	}, /* 83 */
	{ 1,	TD,	sys_fchdir,		"fchdir"	}, /* 84 */
	{ 3,	TF,	sys_readlink,		"readlink"	}, /* 85 */
	{ 5,	TD,	sys_pread,		"pread"		}, /* 86 */
	{ 5,	TD,	sys_pwrite,		"pwrite"	}, /* 87 */
	{ 1,	TF,	sys_swapon,		"swapon"	}, /* 88 */
	{ 3,	0,	sys_reboot,		"reboot"	}, /* 89 */
	{ 6,	TD,	sys_mmap,		"mmap"		}, /* 90 */
	{ 2,	0,	sys_munmap,		"munmap"	}, /* 91 */
	{ 2,	TF,	sys_truncate,		"truncate"	}, /* 92 */
	{ 2,	TD,	sys_ftruncate,		"ftruncate"	}, /* 93 */
	{ 2,	TD,	sys_fchmod,		"fchmod"	}, /* 94 */
	{ 3,	TD,	sys_fchown,		"fchown"	}, /* 95 */
	{ 2,	0,	sys_getpriority,	"getpriority"	}, /* 96 */
	{ 3,	0,	sys_setpriority,	"setpriority"	}, /* 97 */
	{ 4,	TP,	sys_wait4,		"wait4"		}, /* 98 */
	{ 2,	TF,	sys_statfs,		"statfs"	}, /* 99 */
	{ 2,	TD,	sys_fstatfs,		"fstatfs"	}, /* 100 */
	{ 0,	0,	sys_vhangup,		"vhangup"	}, /* 101 */
	{ 2,	TS,	sys_sigaltstack,	"sigaltstack"	}, /* 102 */
	{ 3,	0,	sys_syslog,		"syslog"	}, /* 103 */
	{ 3,	0,	sys_setitimer,		"setitimer"	}, /* 104 */
	{ 2,	0,	sys_getitimer,		"getitimer"	}, /* 105 */
	{ 1,	TF,	sys_swapoff,		"swapoff"	}, /* 106 */
	{ 1,	0,	sys_sysinfo,		"sysinfo"	}, /* 107 */
	{ 6,	0,	sys_ipc,		"ipc"		}, /* 108 */
	{ 4,	TD|TN,	sys_sendfile,		"sendfile"	}, /* 109 */
	{ 2,	0,	sys_setdomainname,	"setdomainname"	}, /* 110 */
	{ 1,	0,	sys_uname,		"uname"		}, /* 111 */
	{ 1,	0,	sys_adjtimex,		"adjtimex"	}, /* 112 */
	{ 3,	0,	sys_mprotect,		"mprotect"	}, /* 113 */
	{ 0,	TP,	sys_vfork,		"vfork"		}, /* 114 */
	{ 3,	0,	sys_init_module,	"init_module"	}, /* 115 */
	{ 2,	0,	sys_delete_module,	"delete_module"	}, /* 116 */
	{ 4,	0,	sys_quotactl,		"quotactl"	}, /* 117 */
	{ 1,	0,	sys_getpgid,		"getpgid"	}, /* 118 */
	{ 0,	0,	sys_bdflush,		"bdflush"	}, /* 119 */
	{ 3,	0,	sys_sysfs,		"sysfs"		}, /* 120 */
	{ 1,	0,	sys_personality,	"personality"	}, /* 121 */
	{ 5,	0,	sys_afs_syscall,	"afs_syscall"	}, /* 122 */
	{ 3,	TD,	sys_getdents,		"getdents"	}, /* 123 */
	{ 2,	TD,	sys_flock,		"flock"		}, /* 124 */
	{ 3,	0,	sys_msync,		"msync"		}, /* 125 */
	{ 3,	TD,	sys_readv,		"readv"		}, /* 126 */
	{ 3,	TD,	sys_writev,		"writev"	}, /* 127 */
	{ 1,	0,	sys_getsid,		"getsid"	}, /* 128 */
	{ 1,	TD,	sys_fdatasync,		"fdatasync"	}, /* 129 */
	{ 1,	0,	sys_sysctl,		"_sysctl"	}, /* 130 */
	{ 2,	0,	sys_mlock,		"mlock"		}, /* 131 */
	{ 2,	0,	sys_munlock,		"munlock"	}, /* 132 */
	{ 2,	0,	sys_mlockall,		"mlockall"	}, /* 133 */
	{ 0,	0,	sys_munlockall,		"munlockall"	}, /* 134 */
	{ 0,	0,	sys_sched_setparam,	"sched_setparam"}, /* 135 */
	{ 2,	0,	sys_sched_getparam,	"sched_getparam"}, /* 136 */
	{ 3,	0,	sys_sched_setscheduler,	"sched_setscheduler"}, /* 137 */
	{ 1,	0,	sys_sched_getscheduler,	"sched_getscheduler"}, /* 138 */
	{ 0,	0,	sys_sched_yield,	"sched_yield"}, /* 139 */
	{ 1,	0,	sys_sched_get_priority_max,"sched_get_priority_max"}, /* 140 */
	{ 1,	0,	sys_sched_get_priority_min,"sched_get_priority_min"}, /* 141 */
	{ 2,	0,	sys_sched_rr_get_interval,"sched_rr_get_interval"}, /* 142 */
	{ 2,	0,	sys_nanosleep,		"nanosleep"	}, /* 143 */
	{ 3,	TD,	sys_poll,		"poll"		}, /* 144 */
	{ 3,	0,	printargs,		"nfsservctl"	}, /* 145 */
	{ 3,	0,	sys_setresgid,		"setresgid"	}, /* 146 */
	{ 3,	0,	sys_getresgid,		"getresgid"	}, /* 147 */
	{ 5,	0,	sys_prctl,		"prctl"		}, /* 148 */
	{ 3,	TN,	sys_socket,		"socket"	}, /* 149 */
	{ 3,	TN,	sys_bind,		"bind"		}, /* 150 */
	{ 3,	TN,	sys_connect,		"connect"	}, /* 151 */
	{ 2,	TN,	sys_listen,		"listen"	}, /* 152 */
	{ 3,	TN,	sys_accept,		"accept"	}, /* 153 */
	{ 3,	TN,	sys_getsockname,	"getsockname"	}, /* 154 */
	{ 3,	TN,	sys_getpeername,	"getpeername"	}, /* 155 */
	{ 4,	TN,	sys_socketpair,		"socketpair"	}, /* 156 */
	{ 4,	TN,	sys_send,		"send"		}, /* 157 */
	{ 4,	TN,	sys_recv,		"recv"		}, /* 158 */
	{ 6,	TN,	sys_sendto,		"sendto"	}, /* 159 */
	{ 6,	TN,	sys_recvfrom,		"recvfrom"	}, /* 160 */
	{ 2,	TN,	sys_shutdown,		"shutdown"	}, /* 161 */
	{ 5,	TN,	sys_setsockopt,		"setsockopt"	}, /* 162 */
	{ 5,	TN,	sys_getsockopt,		"getsockopt"	}, /* 163 */
	{ 5,	TN,	sys_sendmsg,		"sendmsg"	}, /* 164 */
	{ 5,	TN,	sys_recvmsg,		"recvmsg"	}, /* 165 */
	{ 3,	TF,	sys_truncate64,		"truncate64"	}, /* 166 */
	{ 3,	TD,	sys_ftruncate64,	"ftruncate64"	}, /* 167 */
	{ 2,	TF,	sys_stat64,		"stat64"	}, /* 168 */
	{ 2,	TF,	sys_lstat64,		"lstat64"	}, /* 169 */
	{ 2,	TD,	sys_fstat64,		"fstat64"	}, /* 170 */
	{ 2,	TF,	sys_pivotroot,		"pivot_root"	}, /* 171 */
	{ 3,	0,	printargs,		"mincore"	}, /* 172 */
	{ 3,	0,	sys_madvise,		"madvise"	}, /* 173 */
	{ 3,	TD,	sys_getdents64,		"getdents64"	}, /* 174 */
	{ 3,	TD,	sys_fcntl,		"fcntl64"	}, /* 175 */
	{ 0,	0,	printargs,		"gettid"	}, /* 176 */
	{ 4,	TD,	sys_readahead,		"readahead"	}, /* 177 */
	{ 5,	TF,	sys_setxattr,		"setxattr"	}, /* 178 */
	{ 5,	TF,	sys_setxattr,		"lsetxattr"	}, /* 179 */
	{ 5,	TD,	sys_fsetxattr,		"fsetxattr"	}, /* 180 */
	{ 4,	TF,	sys_getxattr,		"getxattr"	}, /* 181 */
	{ 4,	TF,	sys_getxattr,		"lgetxattr"	}, /* 182 */
	{ 4,	TD,	sys_fgetxattr,		"fgetxattr"	}, /* 183 */
	{ 3,	TF,	sys_listxattr,		"listxattr"	}, /* 184 */
	{ 3,	TF,	sys_listxattr,		"llistxattr"	}, /* 185 */
	{ 3,	TD,	sys_flistxattr,		"flistxattr"	}, /* 186 */
	{ 2,	TF,	sys_removexattr,	"removexattr"	}, /* 187 */
	{ 2,	TF,	sys_removexattr,	"lremovexattr"	}, /* 188 */
	{ 2,	TD,	sys_fremovexattr,	"fremovexattr"	}, /* 189 */
	{ 2,	0,	sys_kill,		"tkill"		}, /* 190 */
	{ 4,	TD|TN,	sys_sendfile64,		"sendfile64"	}, /* 191 */
	{ 6,	0,	sys_futex,		"futex"		}, /* 192 */
	{ 3,	0,	sys_sched_setaffinity,	"sched_setaffinity" },/* 193 */
	{ 3,	0,	sys_sched_getaffinity,	"sched_getaffinity" },/* 194 */
	{ 2,	0,	sys_capget,		"capget"	}, /* 195 */
	{ 2,	0,	sys_capset,		"capset"	}, /* 196 */
	{ 2,	0,	sys_io_setup,		"io_setup"	}, /* 197 */
	{ 1,	0,	sys_io_destroy,		"io_destroy"	}, /* 198 */
	{ 5,	0,	sys_io_getevents,	"io_getevents"	}, /* 199 */
	{ 3,	0,	sys_io_submit,		"io_submit"	}, /* 200 */
	{ 3,	0,	sys_io_cancel,		"io_cancel"	}, /* 201 */
	{ 5,	TD,	sys_fadvise64,		"fadvise64"	}, /* 202 */
	{ 1,	TP,	sys_exit,		"exit_group"	}, /* 203 */
	{ 4,	0,	printargs,		"lookup_dcookie"}, /* 204 */
	{ 1,	TD,	sys_epoll_create,	"epoll_create"	}, /* 205 */
	{ 4,	TD,	sys_epoll_ctl,		"epoll_ctl"	}, /* 206 */
	{ 4,	TD,	sys_epoll_wait,		"epoll_wait"	}, /* 207 */
	{ 5,	0,	sys_remap_file_pages,	"remap_file_pages"}, /* 208 */
	{ 1,	0,	printargs,		"set_tid_address"}, /* 209 */
	{ 3,	0,	sys_timer_create,	"timer_create"	}, /* 210 */
	{ 4,	0,	sys_timer_settime,	"timer_settime"	}, /* 211 */
	{ 2,	0,	sys_timer_gettime,	"timer_gettime"	}, /* 212 */
	{ 1,	0,	sys_timer_getoverrun,	"timer_getoverrun"}, /* 213 */
	{ 1,	0,	sys_timer_delete,	"timer_delete"	}, /* 214 */
	{ 2,	0,	sys_clock_settime,	"clock_settime"	}, /* 215 */
	{ 2,	0,	sys_clock_gettime,	"clock_gettime"	}, /* 216 */
	{ 2,	0,	sys_clock_getres,	"clock_getres"	}, /* 217 */
	{ 4,	0,	sys_clock_nanosleep,	"clock_nanosleep"}, /* 218 */
	{ 3,	TF,	sys_statfs64,		"statfs64"	}, /* 219 */
	{ 3,	TD,	sys_fstatfs64,		"fstatfs64"	}, /* 220 */
	{ 3,	TS,	sys_tgkill,		"tgkill"	}, /* 221 */
	{ 5,	0,	printargs,		"SYS_222"	}, /* 222 */
	{ 2,	TF,	sys_utimes,		"utimes"	}, /* 223 */
	{ 6,	TD,	sys_fadvise64_64,	"fadvise64_64"	}, /* 224 */
	{ 3,	0,	printargs,		"cacheflush"	}, /* 225 */
	{ 5,	0,	printargs,		"vserver"	}, /* 226 */
	{ 4,	0,	sys_mq_open,		"mq_open"	}, /* 227 */
	{ 1,	0,	sys_mq_unlink,		"mq_unlink"	}, /* 228 */
	{ 5,	0,	sys_mq_timedsend,	"mq_timedsend"	}, /* 229 */
	{ 5,	0,	sys_mq_timedreceive,	"mq_timedreceive" }, /* 230 */
	{ 2,	0,	sys_mq_notify,		"mq_notify"	}, /* 231 */
	{ 3,	0,	sys_mq_getsetattr,	"mq_getsetattr" }, /* 232 */
	{ 5,	0,	printargs,		"kexec_load"	}, /* 233 */
	{ 5,	TP,	sys_waitid,		"waitid"	}, /* 234 */
	{ 5,	0,	printargs,		"add_key"	}, /* 235 */
	{ 4,	0,	printargs,		"request_key"	}, /* 236 */
	{ 5,	0,	printargs,		"keyctl"	}, /* 237 */
	{ 3,	0,	printargs,		"ioprio_set"	}, /* 238 */
	{ 2,	0,	printargs,		"ioprio_get"	}, /* 239 */
	{ 0,	0,	printargs,		"inotify_init"	}, /* 240 */
	{ 3,	TD,	sys_inotify_add_watch,	"inotify_add_watch" }, /* 241 */
	{ 2,	TD,	sys_inotify_rm_watch,	"inotify_rm_watch" }, /* 242 */
	{ 4,	TD|TF,	sys_openat,		"openat"	}, /* 243 */
	{ 3,	TD|TF,	sys_mkdirat,		"mkdirat"	}, /* 244 */
	{ 4,	TD|TF,	sys_mknodat,		"mknodat"	}, /* 245 */
	{ 5,	TD|TF,	sys_fchownat,		"fchownat"	}, /* 246 */
	{ 3,	TD|TF,	sys_futimesat,		"futimesat"	}, /* 247 */
	{ 4,	TD|TF,	printargs,		"fstatat64"	}, /* 248 */
	{ 3,	TD|TF,	sys_unlinkat,		"unlinkat"	}, /* 249 */
	{ 4,	TD|TF,	sys_renameat,		"renameat"	}, /* 250 */
	{ 5,	TD|TF,	sys_linkat,		"linkat"	}, /* 251 */
	{ 3,	TD|TF,	sys_symlinkat,		"symlinkat"	}, /* 252 */
	{ 4,	TD|TF,	sys_readlinkat,		"readlinkat"	}, /* 253 */
	{ 3,	TD|TF,	sys_fchmodat,		"fchmodat"	}, /* 254 */
	{ 3,	TD|TF,	sys_faccessat,		"faccessat"	}, /* 255 */
	{ 6,	TD,	sys_pselect6,		"pselect6"	}, /* 256 */
	{ 5,	TD,	sys_ppoll,		"ppoll"		}, /* 257 */
	{ 1,	TD,	sys_unshare,		"unshare"	}, /* 258 */
	{ 2,	0,	printargs,		"set_robust_list" }, /* 259 */
	{ 3,	0,	printargs,		"get_robust_list" }, /* 260 */
	{ 6,	TD,	printargs,		"splice"	}, /* 261 */
	{ 4,	TD,	printargs,		"sync_file_range" }, /* 262 */
	{ 4,	TD,	printargs,		"tee"		}, /* 263 */
	{ 4,	TD,	printargs,		"vmsplice"	}, /* 264 */
	{ 5,	TD,	sys_epoll_pwait,	"epoll_pwait"   }, /* 265 */
	{ 4,	TI,	sys_msgget,		"msgget"	}, /* 266 */
	{ 4,	TI,	sys_msgsnd,		"msgsnd"	}, /* 267 */
	{ 5,	TI,	sys_msgrcv,		"msgrcv"	}, /* 268 */
	{ 3,	TI,	sys_msgctl,		"msgctl"	}, /* 269 */
	{ 4,	TI,	sys_semget,		"semget"	}, /* 270 */
	{ 4,	TI,	sys_semop,		"semop"		}, /* 271 */
	{ 4,	TI,	sys_semctl,		"semctl"	}, /* 272 */
	{ 5,	TI,	sys_semtimedop,		"semtimedop"	}, /* 273 */
	{ 4,	TI,	sys_shmat,		"shmat"		}, /* 274 */
	{ 4,	TI,	sys_shmget,		"shmget"	}, /* 275 */
	{ 4,	TI,	sys_shmdt,		"shmdt"		}, /* 276 */
	{ 4,	TI,	sys_shmctl,		"shmctl"	}, /* 277 */
	{ 4,	TD|TF,	sys_utimensat,		"utimensat"	}, /* 278 */
	{ 3,	TD|TS,	sys_signalfd,		"signalfd"	}, /* 279 */
	{ 2,	TD,	sys_timerfd,		"timerfd_create" }, /* 280 */
	{ 1,	TD,	sys_eventfd,		"eventfd"	}, /* 281 */

/*
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995 Rick Sladkey <jrs@world.std.com>
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
	{ 3,	TP,	sys_waitpid,		"waitpid"	}, /* 7 */
	{ 2,	TD|TF,	sys_creat,		"creat"		}, /* 8 */
	{ 2,	TF,	sys_link,		"link"		}, /* 9 */
	{ 1,	TF,	sys_unlink,		"unlink"	}, /* 10 */
	{ 3,	TF|TP,	sys_execve,		"execve"	}, /* 11 */
	{ 1,	TF,	sys_chdir,		"chdir"		}, /* 12 */
	{ 1,	0,	sys_time,		"time"		}, /* 13 */
	{ 3,	TF,	sys_mknod,		"mknod"		}, /* 14 */
	{ 2,	TF,	sys_chmod,		"chmod"		}, /* 15 */
	{ 3,	TF,	sys_chown,		"lchown"	}, /* 16 */
	{ 0,	0,	sys_break,		"break"		}, /* 17 */
	{ 2,	TF,	sys_oldstat,		"oldstat"	}, /* 18 */
	{ 3,	TD,	sys_lseek,		"lseek"		}, /* 19 */
	{ 0,	0,	sys_getpid,		"getpid"	}, /* 20 */
	{ 5,	TF,	sys_mount,		"mount"		}, /* 21 */
	{ 1,	TF,	sys_umount,		"oldumount"	}, /* 22 */
	{ 1,	0,	sys_setuid,		"setuid"	}, /* 23 */
	{ 0,	NF,	sys_getuid,		"getuid"	}, /* 24 */
	{ 1,	0,	sys_stime,		"stime"		}, /* 25 */
	{ 4,	0,	sys_ptrace,		"ptrace"	}, /* 26 */
	{ 1,	0,	sys_alarm,		"alarm"		}, /* 27 */
	{ 2,	TD,	sys_oldfstat,		"oldfstat"	}, /* 28 */
	{ 0,	TS,	sys_pause,		"pause"		}, /* 29 */
	{ 2,	TF,	sys_utime,		"utime"		}, /* 30 */
	{ 2,	0,	sys_stty,		"stty"		}, /* 31 */
	{ 2,	0,	sys_gtty,		"gtty"		}, /* 32 */
	{ 2,	TF,	sys_access,		"access"	}, /* 33 */
	{ 1,	0,	sys_nice,		"nice"		}, /* 34 */
	{ 0,	0,	sys_ftime,		"ftime"		}, /* 35 */
	{ 0,	0,	sys_sync,		"sync"		}, /* 36 */
	{ 2,	TS,	sys_kill,		"kill"		}, /* 37 */
	{ 2,	TF,	sys_rename,		"rename"	}, /* 38 */
	{ 2,	TF,	sys_mkdir,		"mkdir"		}, /* 39 */
	{ 1,	TF,	sys_rmdir,		"rmdir"		}, /* 40 */
	{ 1,	TD,	sys_dup,		"dup"		}, /* 41 */
	{ 1,	TD,	sys_pipe,		"pipe"		}, /* 42 */
	{ 1,	0,	sys_times,		"times"		}, /* 43 */
	{ 0,	0,	sys_prof,		"prof"		}, /* 44 */
	{ 1,	0,	sys_brk,		"brk"		}, /* 45 */
	{ 1,	0,	sys_setgid,		"setgid"	}, /* 46 */
	{ 0,	NF,	sys_getgid,		"getgid"	}, /* 47 */
	{ 3,	TS,	sys_signal,		"signal"	}, /* 48 */
	{ 0,	NF,	sys_geteuid,		"geteuid"	}, /* 49 */
	{ 0,	NF,	sys_getegid,		"getegid"	}, /* 50 */
	{ 1,	TF,	sys_acct,		"acct"		}, /* 51 */
	{ 2,	TF,	sys_umount2,		"umount"	}, /* 52 */
	{ 0,	0,	sys_lock,		"lock"		}, /* 53 */
	{ 3,	TD,	sys_ioctl,		"ioctl"		}, /* 54 */
	{ 3,	TD,	sys_fcntl,		"fcntl"		}, /* 55 */
	{ 0,	0,	sys_mpx,		"mpx"		}, /* 56 */
	{ 2,	0,	sys_setpgid,		"setpgid"	}, /* 57 */
	{ 2,	0,	sys_ulimit,		"ulimit"	}, /* 58 */
	{ 1,	0,	sys_oldolduname,	"oldolduname"	}, /* 59 */
	{ 1,	0,	sys_umask,		"umask"		}, /* 60 */
	{ 1,	TF,	sys_chroot,		"chroot"	}, /* 61 */
	{ 2,	0,	sys_ustat,		"ustat"		}, /* 62 */
	{ 2,	TD,	sys_dup2,		"dup2"		}, /* 63 */
	{ 0,	0,	sys_getppid,		"getppid"	}, /* 64 */
	{ 0,	0,	sys_getpgrp,		"getpgrp"	}, /* 65 */
	{ 0,	0,	sys_setsid,		"setsid"	}, /* 66 */
	{ 3,	TS,	sys_sigaction,		"sigaction"	}, /* 67 */
	{ 0,	TS,	sys_siggetmask,		"siggetmask"	}, /* 68 */
	{ 1,	TS,	sys_sigsetmask,		"sigsetmask"	}, /* 69 */
	{ 2,	0,	sys_setreuid,		"setreuid"	}, /* 70 */
	{ 2,	0,	sys_setregid,		"setregid"	}, /* 71 */
	{ 3,	TS,	sys_sigsuspend,		"sigsuspend"	}, /* 72 */
	{ 1,	TS,	sys_sigpending,		"sigpending"	}, /* 73 */
	{ 2,	0,	sys_sethostname,	"sethostname"	}, /* 74 */
	{ 2,	0,	sys_setrlimit,		"setrlimit"	}, /* 75 */
	{ 2,	0,	sys_getrlimit,		"getrlimit"	}, /* 76 */
	{ 2,	0,	sys_getrusage,		"getrusage"	}, /* 77 */
	{ 2,	0,	sys_gettimeofday,	"gettimeofday"	}, /* 78 */
	{ 2,	0,	sys_settimeofday,	"settimeofday"	}, /* 79 */
	{ 2,	0,	sys_getgroups,		"getgroups"	}, /* 80 */
	{ 2,	0,	sys_setgroups,		"setgroups"	}, /* 81 */
	{ 1,	TD,	sys_oldselect,		"oldselect"	}, /* 82 */
	{ 2,	TF,	sys_symlink,		"symlink"	}, /* 83 */
	{ 2,	TF,	sys_oldlstat,		"oldlstat"	}, /* 84 */
	{ 3,	TF,	sys_readlink,		"readlink"	}, /* 85 */
	{ 1,	TF,	sys_uselib,		"uselib"	}, /* 86 */
	{ 1,	TF,	sys_swapon,		"swapon"	}, /* 87 */
	{ 3,	0,	sys_reboot,		"reboot"	}, /* 88 */
	{ 3,	TD,	sys_readdir,		"readdir"	}, /* 89 */
	{ 6,	TD,	sys_old_mmap,		"old_mmap"	}, /* 90 */
	{ 2,	0,	sys_munmap,		"munmap"	}, /* 91 */
	{ 2,	TF,	sys_truncate,		"truncate"	}, /* 92 */
	{ 2,	TD,	sys_ftruncate,		"ftruncate"	}, /* 93 */
	{ 2,	TD,	sys_fchmod,		"fchmod"	}, /* 94 */
	{ 3,	TD,	sys_fchown,		"fchown"	}, /* 95 */
	{ 2,	0,	sys_getpriority,	"getpriority"	}, /* 96 */
	{ 3,	0,	sys_setpriority,	"setpriority"	}, /* 97 */
	{ 4,	0,	sys_profil,		"profil"	}, /* 98 */
	{ 2,	TF,	sys_statfs,		"statfs"	}, /* 99 */
	{ 2,	TD,	sys_fstatfs,		"fstatfs"	}, /* 100 */
	{ 3,	0,	sys_ioperm,		"ioperm"	}, /* 101 */
	{ 2,	TD,	sys_socketcall,		"socketcall"	}, /* 102 */
	{ 3,	0,	sys_syslog,		"syslog"	}, /* 103 */
	{ 3,	0,	sys_setitimer,		"setitimer"	}, /* 104 */
	{ 2,	0,	sys_getitimer,		"getitimer"	}, /* 105 */
	{ 2,	TF,	sys_stat,		"stat"		}, /* 106 */
	{ 2,	TF,	sys_lstat,		"lstat"		}, /* 107 */
	{ 2,	TD,	sys_fstat,		"fstat"		}, /* 108 */
	{ 1,	0,	sys_olduname,		"olduname"	}, /* 109 */
	{ 1,	0,	sys_iopl,		"iopl"		}, /* 110 */
	{ 0,	0,	sys_vhangup,		"vhangup"	}, /* 111 */
	{ 0,	0,	sys_idle,		"idle"		}, /* 112 */
	{ 1,	0,	printargs,		"SYS_113"	}, /* 113 */
	{ 4,	TP,	sys_wait4,		"wait4"		}, /* 114 */
	{ 1,	TF,	sys_swapoff,		"swapoff"	}, /* 115 */
	{ 1,	0,	sys_sysinfo,		"sysinfo"	}, /* 116 */
	{ 5,	0,	sys_ipc,		"ipc"		}, /* 117 */
	{ 1,	TD,	sys_fsync,		"fsync"		}, /* 118 */
	{ 1,	TS,	sys_sigreturn,		"sigreturn"	}, /* 119 */
	{ 5,	TP,	sys_clone,		"clone"		}, /* 120 */
	{ 2,	0,	sys_setdomainname,	"setdomainname"	}, /* 121 */
	{ 1,	0,	sys_uname,		"uname"		}, /* 122 */
	{ 3,	0,	printargs,		"cacheflush"	}, /* 123 */
	{ 1,	0,	sys_adjtimex,		"adjtimex"	}, /* 124 */
	{ 3,	0,	sys_mprotect,		"mprotect"	}, /* 125 */
	{ 3,	TS,	sys_sigprocmask,	"sigprocmask"	}, /* 126 */
	{ 2,	0,	sys_create_module,	"create_module"	}, /* 127 */
	{ 3,	0,	sys_init_module,	"init_module"	}, /* 128 */
	{ 2,	0,	sys_delete_module,	"delete_module"	}, /* 129 */
	{ 1,	0,	sys_get_kernel_syms,	"get_kernel_syms"}, /* 130 */
	{ 4,	0,	sys_quotactl,		"quotactl"	}, /* 131 */
	{ 1,	0,	sys_getpgid,		"getpgid"	}, /* 132 */
	{ 1,	TD,	sys_fchdir,		"fchdir"	}, /* 133 */
	{ 0,	0,	sys_bdflush,		"bdflush"	}, /* 134 */
	{ 3,	0,	sys_sysfs,		"sysfs"		}, /* 135 */
	{ 1,	0,	sys_personality,	"personality"	}, /* 136 */
	{ 5,	0,	sys_afs_syscall,	"afs_syscall"	}, /* 137 */
	{ 1,	NF,	sys_setfsuid,		"setfsuid"	}, /* 138 */
	{ 1,	NF,	sys_setfsgid,		"setfsgid"	}, /* 139 */
	{ 5,	TD,	sys_llseek,		"_llseek"	}, /* 140 */
	{ 3,	TD,	sys_getdents,		"getdents"	}, /* 141 */
	{ 5,	TD,	sys_select,		"select"	}, /* 142 */
	{ 2,	TD,	sys_flock,		"flock"		}, /* 143 */
	{ 3,	0,	sys_msync,		"msync"		}, /* 144 */
	{ 3,	TD,	sys_readv,		"readv"		}, /* 145 */
	{ 3,	TD,	sys_writev,		"writev"	}, /* 146 */
	{ 1,	0,	sys_getsid,		"getsid"	}, /* 147 */
	{ 1,	TD,	sys_fdatasync,		"fdatasync"	}, /* 148 */
	{ 1,	0,	sys_sysctl,		"_sysctl"	}, /* 149 */
	{ 1,	0,	sys_mlock,		"mlock"		}, /* 150 */
	{ 2,	0,	sys_munlock,		"munlock"	}, /* 151 */
	{ 2,	0,	sys_mlockall,		"mlockall"	}, /* 152 */
	{ 0,	0,	sys_munlockall,		"munlockall"	}, /* 153 */
	{ 0,	0,	sys_sched_setparam,	"sched_setparam"}, /* 154 */
	{ 2,	0,	sys_sched_getparam,	"sched_getparam"}, /* 155 */
	{ 3,	0,	sys_sched_setscheduler,	"sched_setscheduler"}, /* 156 */
	{ 1,	0,	sys_sched_getscheduler,	"sched_getscheduler"}, /* 157 */
	{ 0,	0,	sys_sched_yield,	"sched_yield"}, /* 158 */
	{ 1,	0,	sys_sched_get_priority_max,"sched_get_priority_max"}, /* 159 */
	{ 1,	0,	sys_sched_get_priority_min,"sched_get_priority_min"}, /* 160 */
	{ 2,	0,	sys_sched_rr_get_interval,"sched_rr_get_interval"}, /* 161 */
	{ 2,	0,	sys_nanosleep,		"nanosleep"	}, /* 162 */
	{ 5,	0,	sys_mremap,		"mremap"	}, /* 163 */
	{ 3,	0,	sys_setresuid,		"setresuid"	}, /* 164 */
	{ 3,	0,	sys_getresuid,		"getresuid"	}, /* 165 */
	{ 5,	0,	printargs,		"SYS_166"	}, /* 166 */
	{ 5,	0,	sys_query_module,	"query_module"	}, /* 167 */
	{ 3,	TD,	sys_poll,		"poll"		}, /* 168 */
	{ 3,	0,	printargs,		"nfsservctl"	}, /* 169 */
	{ 3,	0,	sys_setresgid,		"setresgid"	}, /* 170 */
	{ 3,	0,	sys_getresgid,		"getresgid"	}, /* 171 */
	{ 5,	0,	sys_prctl,		"prctl"		}, /* 172 */
	{ 1,	TS,	printargs,		"rt_sigreturn"	}, /* 173 */
	{ 4,	TS,	sys_rt_sigaction,	"rt_sigaction"  }, /* 174 */
	{ 4,	TS,	sys_rt_sigprocmask,	"rt_sigprocmask"}, /* 175 */
	{ 2,	TS,	sys_rt_sigpending,	"rt_sigpending"	}, /* 176 */
	{ 4,	TS,	sys_rt_sigtimedwait,	"rt_sigtimedwait"}, /* 177 */
	{ 3,	TS,	sys_rt_sigqueueinfo,    "rt_sigqueueinfo"}, /* 178 */
	{ 2,	TS,	sys_rt_sigsuspend,	"rt_sigsuspend"	}, /* 179 */
	{ 5,	TD,	sys_pread,		"pread"		}, /* 180 */
	{ 5,	TD,	sys_pwrite,		"pwrite"	}, /* 181 */
	{ 3,	TF,	sys_chown,		"chown"		}, /* 182 */
	{ 2,	TF,	sys_getcwd,		"getcwd"	}, /* 183 */
	{ 2,	0,	sys_capget,		"capget"	}, /* 184 */
	{ 2,	0,	sys_capset,		"capset"	}, /* 185 */
	{ 2,	TS,	sys_sigaltstack,	"sigaltstack"	}, /* 186 */
	{ 4,	TD|TN,	sys_sendfile,		"sendfile"	}, /* 187 */
	{ 5,	0,	printargs,		"SYS_188"	}, /* 188 */
	{ 5,	0,	printargs,		"SYS_189"	}, /* 189 */
	{ 0,	TP,	sys_vfork,		"vfork"		}, /* 190 */
	{ 2,	0,	printargs,		"getrlimit"	}, /* 191 */
	{ 6,	TD,	sys_mmap,		"mmap2"		}, /* 192 */
	{ 2,	TF,	sys_truncate64,		"truncate64"	}, /* 193 */
	{ 2,	TD,	sys_ftruncate64,	"ftruncate64"	}, /* 194 */
	{ 2,	TF,	sys_stat64,		"stat64"	}, /* 195 */
	{ 2,	TF,	sys_lstat64,		"lstat64"	}, /* 196 */
	{ 2,	TD,	sys_fstat64,		"fstat64"	}, /* 197 */
	{ 3,	TF,	sys_chown,		"lchown32"	}, /* 198 */
	{ 0,	NF,	sys_getuid,		"getuid32"	}, /* 199 */
	{ 0,	0,	printargs,		"getgid32"	}, /* 200 */
	{ 0,	0,	printargs,		"geteuid32"	}, /* 201 */
	{ 0,	0,	printargs,		"getegid32"	}, /* 202 */
	{ 2,	0,	printargs,		"setreuid32"	}, /* 203 */
	{ 2,	0,	printargs,		"setregid32"	}, /* 204 */
	{ 2,	0,	sys_getgroups32,	"getgroups32"	}, /* 205 */
	{ 2,	0,	sys_setgroups32,	"setgroups32"	}, /* 206 */
	{ 3,	0,	printargs,		"fchown32"	}, /* 207 */
	{ 3,	0,	printargs,		"setresuid32"	}, /* 208 */
	{ 3,	0,	printargs,		"getresuid32"	}, /* 209 */
	{ 3,	0,	printargs,		"setresgid32"	}, /* 210 */
	{ 3,	0,	printargs,		"getresgid32"	}, /* 211 */
	{ 3,	TF,	printargs,		"chown32"	}, /* 212 */
	{ 1,	0,	printargs,		"setuid32"	}, /* 213 */
	{ 1,	0,	printargs,		"setgid32"	}, /* 214 */
	{ 1,	0,	printargs,		"setfsuid32"	}, /* 215 */
	{ 1,	0,	printargs,		"setfsgid32"	}, /* 216 */
	{ 2,	TF,	sys_pivotroot,		"pivot_root"	}, /* 217 */
	{ 3,	0,	printargs,		"mincore"	}, /* 218 */
	{ 3,	0,	sys_madvise,		"madvise"	}, /* 219 */
	{ 3,	TN,	sys_socket,		"socket"	}, /* 220 */
	{ 3,	TN,	sys_bind,		"bind"		}, /* 221 */
	{ 3,	TN,	sys_connect,		"connect"	}, /* 222 */
	{ 2,	TN,	sys_listen,		"listen"	}, /* 223 */
	{ 3,	TN,	sys_accept,		"accept"	}, /* 224 */
	{ 3,	TN,	sys_getsockname,	"getsockname"	}, /* 225 */
	{ 3,	TN,	sys_getpeername,	"getpeername"	}, /* 226 */
	{ 4,	TN,	sys_socketpair,		"socketpair"	}, /* 227 */
	{ 4,	TN,	sys_send,		"send"		}, /* 228 */
	{ 6,	TN,	sys_sendto,		"sendto"	}, /* 229 */
	{ 4,	TN,	sys_recv,		"recv"		}, /* 230 */
	{ 6,	TN,	sys_recvfrom,		"recvfrom"	}, /* 231 */
	{ 2,	TN,	sys_shutdown,		"shutdown"	}, /* 232 */
	{ 5,	TN,	sys_setsockopt,		"setsockopt"	}, /* 233 */
	{ 5,	TN,	sys_getsockopt,		"getsockopt"	}, /* 234 */
	{ 5,	TN,	sys_sendmsg,		"sendmsg"	}, /* 235 */
	{ 5,	TN,	sys_recvmsg,		"recvmsg"	}, /* 236 */
	{ 4,	TI,	sys_semop,		"semop"		}, /* 237 */
	{ 4,	TI,	sys_semget,		"semget"	}, /* 238 */
	{ 4,	TI,	sys_semctl,		"semctl"	}, /* 239 */
	{ 4,	TI,	sys_msgsnd,		"msgsnd"	}, /* 240 */
	{ 4,	TI,	sys_msgrcv,		"msgrcv"	}, /* 241 */
	{ 4,	TI,	sys_msgget,		"msgget"	}, /* 242 */
	{ 4,	TI,	sys_msgctl,		"msgctl"	}, /* 243 */
	{ 4,	TI,	sys_shmat,		"shmat"		}, /* 244 */
	{ 4,	TI,	sys_shmdt,		"shmdt"		}, /* 245 */
	{ 4,	TI,	sys_shmget,		"shmget"	}, /* 246 */
	{ 4,	TI,	sys_shmctl,		"shmctl"	}, /* 247 */
	{ 3,	TD,	sys_getdents64,		"getdents64"	}, /* 248 */
	{ 3,	TD,	sys_fcntl,		"fcntl64"	}, /* 249 */
	{ 4,	0,	printargs,		"SYS_250"	}, /* 250 */
	{ 4,	0,	printargs,		"SYS_251"	}, /* 251 */
	{ 4,	0,	printargs,		"gettid"	}, /* 252 */
	{ 5,	TD,	sys_readahead,		"readahead"	}, /* 253 */
	{ 5,	TF,	sys_setxattr,		"setxattr"	}, /* 254 */
	{ 5,	TF,	sys_setxattr,		"lsetxattr"	}, /* 255 */
	{ 5,	TD,	sys_fsetxattr,		"fsetxattr"	}, /* 256 */
	{ 4,	TF,	sys_getxattr,		"getxattr"	}, /* 257 */
	{ 4,	TF,	sys_getxattr,		"lgetxattr"	}, /* 258 */
	{ 4,	TD,	sys_fgetxattr,		"fgetxattr"	}, /* 259 */
	{ 3,	TF,	sys_listxattr,		"listxattr"	}, /* 260 */
	{ 3,	TF,	sys_listxattr,		"llistxattr"	}, /* 261 */
	{ 3,	TD,	sys_flistxattr,		"flistxattr"	}, /* 262 */
	{ 2,	TF,	sys_removexattr,	"removexattr"	}, /* 263 */
	{ 2,	TF,	sys_removexattr,	"lremovexattr"	}, /* 264 */
	{ 2,	TD,	sys_fremovexattr,	"fremovexattr"	}, /* 265 */
	{ 2,	TD,	sys_kill,		"tkill"		}, /* 266 */
	{ 5,	TD|TN,	sys_sendfile64,		"sendfile64"	}, /* 267 */
	{ 6,	0,	sys_futex,		"futex"		}, /* 268 */
	{ 3,	0,	sys_sched_setaffinity,	"sched_setaffinity"}, /* 269 */
	{ 3,	0,	sys_sched_getaffinity,	"sched_getaffinity"}, /* 270 */
	{ 5,	0,	printargs,		"SYS_271"	}, /* 271 */
	{ 5,	0,	printargs,		"SYS_272"	}, /* 272 */
	{ 2,	0,	sys_io_setup,		"io_setup"	}, /* 273 */
	{ 1,	0,	sys_io_destroy,		"io_destroy"	}, /* 274 */
	{ 5,	0,	sys_io_getevents,	"io_getevents"	}, /* 275 */
	{ 3,	0,	sys_io_submit,		"io_submit"	}, /* 276 */
	{ 3,	0,	sys_io_cancel,		"io_cancel"	}, /* 277 */
	{ 5,	TD,	sys_fadvise64,		"fadvise64"	}, /* 278 */
	{ 5,	0,	printargs,		"SYS_279"	}, /* 279 */
	{ 1,	TP,	sys_exit,		"exit_group"	}, /* 280 */
	{ 4,	0,	printargs,		"lookup_dcookie"}, /* 281 */
	{ 1,	TD,	sys_epoll_create,	"epoll_create"	}, /* 282 */
	{ 4,	TD,	sys_epoll_ctl,		"epoll_ctl"	}, /* 283 */
	{ 3,	TD,	sys_epoll_wait,		"epoll_wait"	}, /* 284 */
	{ 5,	0,	sys_remap_file_pages,	"remap_file_pages"}, /* 285 */
	{ 1,	0,	printargs,		"set_tid_address"}, /* 286 */
	{ 3,	0,	sys_timer_create,	"timer_create"	}, /* 287 */
	{ 4,	0,	sys_timer_settime,	"timer_settime"	}, /* 288 */
	{ 2,	0,	sys_timer_gettime,	"timer_gettime"	}, /* 289 */
	{ 1,	0,	sys_timer_getoverrun,	"timer_getoverrun"}, /* 290 */
	{ 1,	0,	sys_timer_delete,	"timer_delete"	}, /* 291 */
	{ 2,	0,	sys_clock_settime,	"clock_settime"	}, /* 292 */
	{ 2,	0,	sys_clock_gettime,	"clock_gettime"	}, /* 293 */
	{ 2,	0,	sys_clock_getres,	"clock_getres"	}, /* 294 */
	{ 4,	0,	sys_clock_nanosleep,	"clock_nanosleep"}, /* 295 */
	{ 3,	TF,	sys_statfs64,		"statfs64"	}, /* 296 */
	{ 2,	TD,	sys_fstatfs64,		"fstatfs64"	}, /* 297 */
	{ 3,	TS,	sys_tgkill,		"tgkill"	}, /* 298 */
	{ 2,	TF,	sys_utimes,		"utimes"	}, /* 299 */
	{ 6,	TD,	sys_fadvise64_64,	"fadvise64_64"	}, /* 300 */
	{ 0,	0,	printargs,		"SYS_301"	}, /* 301 */
	{ 4,	0,	sys_mbind,		"mbind"		}, /* 302 */
	{ 5,	0,	sys_get_mempolicy,	"get_mempolicy"	}, /* 303 */
	{ 3,	0,	sys_set_mempolicy,	"set_mempolicy"	}, /* 304 */
	{ 4,	0,	sys_mq_open,		"mq_open"	}, /* 305 */
	{ 1,	0,	sys_mq_unlink,		"mq_unlink"	}, /* 306 */
	{ 5,	0,	sys_mq_timedsend,	"mq_timedsend"	}, /* 307 */
	{ 5,	0,	sys_mq_timedreceive,	"mq_timedreceive"}, /* 308 */
	{ 2,	0,	sys_mq_notify,		"mq_notify"	}, /* 309 */
	{ 3,	0,	sys_mq_getsetattr,	"mq_getsetattr"	}, /* 310 */
	{ 5,	0,	printargs,		"kexec_load"	}, /* 311 */
	{ 5,	TP,	sys_waitid,		"waitid"	}, /* 312 */
	{ 5,	0,	printargs,		"add_key"	}, /* 313 */
	{ 4,	0,	printargs,		"request_key"	}, /* 314 */
	{ 5,	0,	printargs,		"keyctl"	}, /* 315 */
	{ 3,	0,	printargs,		"ioprio_set"	}, /* 316 */
	{ 2,	0,	printargs,		"ioprio_get"	}, /* 317 */
	{ 0,	TD,	printargs,		"inotify_init"	}, /* 318 */
	{ 3,	TD,	printargs,		"inotify_add_watch"}, /* 319 */
	{ 2,	TD,	printargs,		"inotify_rm_watch"}, /* 320 */
	{ 5,	0,	printargs,		"SYS_321"	}, /* 321 */
	{ 4,	0,	printargs,		"migrate_pages"	}, /* 322 */
	{ 4,	TD|TF,	sys_openat,		"openat"	}, /* 323 */
	{ 3,	TD|TF,	sys_mkdirat,		"mkdirat"	}, /* 324 */
	{ 4,	TD|TF,	sys_mknodat,		"mknodat"	}, /* 325 */
	{ 5,	TD|TF,	sys_fchownat,		"fchownat"	}, /* 326 */
	{ 3,	TD|TF,	sys_futimesat,		"futimesat"	}, /* 327 */
	{ 4,	TD|TF,	sys_newfstatat,		"newfstatat"	}, /* 328 */
	{ 3,	TD|TF,	sys_unlinkat,		"unlinkat"	}, /* 329 */
	{ 4,	TD|TF,	sys_renameat,		"renameat"	}, /* 330 */
	{ 5,	TD|TF,	sys_linkat,		"linkat"	}, /* 331 */
	{ 3,	TD|TF,	sys_symlinkat,		"symlinkat"	}, /* 332 */
	{ 4,	TD|TF,	sys_readlinkat,		"readlinkat"	}, /* 333 */
	{ 3,	TD|TF,	sys_fchmodat,		"fchmodat"	}, /* 334 */
	{ 3,	TD|TF,	sys_faccessat,		"faccessat"	}, /* 335 */
	{ 6,	TD,	sys_pselect6,		"pselect6"	}, /* 336 */
	{ 5,	TD,	sys_ppoll,		"ppoll"		}, /* 337 */
	{ 1,	TP,	sys_unshare,		"unshare"	}, /* 338 */
	{ 2,	0,	printargs,		"set_robust_list"}, /* 339 */
	{ 3,	0,	printargs,		"get_robust_list"}, /* 340 */
	{ 6,	TD,	printargs,		"splice"	}, /* 341 */
	{ 4,	TD,	printargs,		"sync_file_range"}, /* 342 */
	{ 4,	TD,	printargs,		"tee"		}, /* 343 */
	{ 5,	TD,	printargs,		"vmsplice"	}, /* 344 */
	{ 6,	0,	sys_move_pages,		"move_pages"	}, /* 345 */
	{ 3,	0,	sys_getcpu,		"getcpu"	}, /* 346 */
	{ 5,	TD,	sys_epoll_pwait,	"epoll_pwait"	}, /* 347 */
	{ 4,	TD|TF,	sys_utimensat,		"utimensat"	}, /* 348 */
	{ 3,	TD|TS,	sys_signalfd,		"signalfd"	}, /* 349 */
	{ 4,	TD,	sys_timerfd,		"timerfd"	}, /* 350 */
	{ 1,	TD,	sys_eventfd,		"eventfd"	}, /* 351 */
	{ 6,	TD,	sys_fallocate,		"fallocate"	}, /* 352 */
	{ 4,	TD,	sys_timerfd_settime,	"timerfd_settime"}, /* 353 */
	{ 2,	TD,	sys_timerfd_gettime,	"timerfd_gettime"}, /* 354 */
	{ 4,	TD|TS,	sys_signalfd4,		"signalfd4"	}, /* 355 */
	{ 2,	TD,	sys_eventfd2,		"eventfd2"	}, /* 356 */
	{ 1,	TD,	sys_epoll_create1,	"epoll_create1"	}, /* 357 */
	{ 3,	TD,	sys_dup3,		"dup3"		}, /* 358 */
	{ 2,	TD,	sys_pipe2,		"pipe2"		}, /* 359 */
	{ 1,	TD,	sys_inotify_init1,	"inotify_init1"	}, /* 360 */
	{ 5,	TD,	printargs,		"preadv"	}, /* 361 */
	{ 5,	TD,	printargs,		"pwritev"	}, /* 362 */
	{ 4,	TP|TS,	printargs,		"rt_tgsigqueueinfo"}, /* 363 */
	{ 5,	TD,	printargs,		"perf_event_open"}, /* 364 */
	{ 5,	TN,	sys_recvmmsg,		"recvmmsg"	}, /* 365 */
	{ 4,	TN,	sys_accept4,		"accept4"	}, /* 366 */
	{ 2,	TD,	printargs,		"fanotify_init"	}, /* 367 */
	{ 5,	TD|TF,	printargs,		"fanotify_mark"	}, /* 368 */
	{ 4,	0,	printargs,		"prlimit64"	}, /* 369 */
	{ 5,	0,	printargs,		"SYS_370"	}, /* 370 */
	{ 5,	0,	printargs,		"SYS_371"	}, /* 371 */
	{ 5,	0,	printargs,		"SYS_372"	}, /* 372 */
	{ 5,	0,	printargs,		"SYS_373"	}, /* 373 */
	{ 5,	0,	printargs,		"SYS_374"	}, /* 374 */
	{ 5,	0,	printargs,		"SYS_375"	}, /* 375 */
	{ 5,	0,	printargs,		"SYS_376"	}, /* 376 */
	{ 5,	0,	printargs,		"SYS_377"	}, /* 377 */
	{ 5,	0,	printargs,		"SYS_378"	}, /* 378 */
	{ 5,	0,	printargs,		"SYS_379"	}, /* 379 */
	{ 5,	0,	printargs,		"SYS_380"	}, /* 380 */
	{ 5,	0,	printargs,		"SYS_381"	}, /* 381 */
	{ 5,	0,	printargs,		"SYS_382"	}, /* 382 */
	{ 5,	0,	printargs,		"SYS_383"	}, /* 383 */
	{ 5,	0,	printargs,		"SYS_384"	}, /* 384 */
	{ 5,	0,	printargs,		"SYS_385"	}, /* 385 */
	{ 5,	0,	printargs,		"SYS_386"	}, /* 386 */
	{ 5,	0,	printargs,		"SYS_387"	}, /* 387 */
	{ 5,	0,	printargs,		"SYS_388"	}, /* 388 */
	{ 5,	0,	printargs,		"SYS_389"	}, /* 389 */
	{ 5,	0,	printargs,		"SYS_390"	}, /* 390 */
	{ 5,	0,	printargs,		"SYS_391"	}, /* 391 */
	{ 5,	0,	printargs,		"SYS_392"	}, /* 392 */
	{ 5,	0,	printargs,		"SYS_393"	}, /* 393 */
	{ 5,	0,	printargs,		"SYS_394"	}, /* 394 */
	{ 5,	0,	printargs,		"SYS_395"	}, /* 395 */
	{ 5,	0,	printargs,		"SYS_396"	}, /* 396 */
	{ 5,	0,	printargs,		"SYS_397"	}, /* 397 */
	{ 5,	0,	printargs,		"SYS_398"	}, /* 398 */
	{ 5,	0,	printargs,		"SYS_399"	}, /* 399 */

#if SYS_socket_subcall != 400
 #error fix me
#endif
	{ 8,	0,	printargs,		"socket_subcall"}, /* 400 */
	{ 3,	TN,	sys_socket,		"socket"	}, /* 401 */
	{ 3,	TN,	sys_bind,		"bind"		}, /* 402 */
	{ 3,	TN,	sys_connect,		"connect"	}, /* 403 */
	{ 2,	TN,	sys_listen,		"listen"	}, /* 404 */
	{ 3,	TN,	sys_accept,		"accept"	}, /* 405 */
	{ 3,	TN,	sys_getsockname,	"getsockname"	}, /* 406 */
	{ 3,	TN,	sys_getpeername,	"getpeername"	}, /* 407 */
	{ 4,	TN,	sys_socketpair,		"socketpair"	}, /* 408 */
	{ 4,	TN,	sys_send,		"send"		}, /* 409 */
	{ 4,	TN,	sys_recv,		"recv"		}, /* 410 */
	{ 6,	TN,	sys_sendto,		"sendto"	}, /* 411 */
	{ 6,	TN,	sys_recvfrom,		"recvfrom"	}, /* 412 */
	{ 2,	TN,	sys_shutdown,		"shutdown"	}, /* 413 */
	{ 5,	TN,	sys_setsockopt,		"setsockopt"	}, /* 414 */
	{ 5,	TN,	sys_getsockopt,		"getsockopt"	}, /* 415 */
	{ 5,	TN,	sys_sendmsg,		"sendmsg"	}, /* 416 */
	{ 5,	TN,	sys_recvmsg,		"recvmsg"	}, /* 417 */
	{ 4,	TN,	sys_accept4,		"accept4"	}, /* 418 */
	{ 5,	TN,	sys_recvmmsg,		"recvmmsg"	}, /* 419 */

#if SYS_ipc_subcall != 420
 #error fix me
#endif
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 420 */

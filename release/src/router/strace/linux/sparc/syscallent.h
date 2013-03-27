	{ 0,	0,	sys_restart_syscall, "restart_syscall" },/* 0 */
	{ 1,	TP,	sys_exit,	"exit" },		/* 1 */
	{ 0,	TP,	sys_fork,	"fork" },		/* 2 */
	{ 3,	TD,	sys_read,	"read" },		/* 3 */
	{ 3,	TD,	sys_write,	"write" },		/* 4 */
	{ 3,	TD|TF,	sys_open,	"open" },		/* 5 */
	{ 1,	TD,	sys_close,	"close" },		/* 6 */
	{ 4,	TP,	sys_wait4,	"wait4" },		/* 7 */
	{ 2,	TD|TF,	sys_creat,	"creat" },		/* 8 */
	{ 2,	TF,	sys_link,	"link" },		/* 9 */
	{ 1,	TF,	sys_unlink,	"unlink" },		/* 10 */
	{ 2,    TF|TP,	sys_execv,	"execv" },		/* 11 */
	{ 1,	TF,	sys_chdir,	"chdir" },		/* 12 */
	{ 3,    TF,     sys_chown,      "chown"},		/* 13 */
	{ 3,	TF,	sys_mknod,	"mknod" },		/* 14 */
	{ 2,	TF,	sys_chmod,	"chmod" },		/* 15 */
	{ 3,	TF,	sys_chown,	"lchown" },		/* 16 */
	{ 1,	0,	sys_brk,	"brk" },		/* 17 */
	{ 4,	0,	printargs,	"perfctr" },		/* 18 */
	{ 3,	TD,	sys_lseek,	"lseek" },		/* 19 */
	{ 0,	0,	sys_getpid,	"getpid" },		/* 20 */
	{ 2,	0,	sys_capget,	"capget" },		/* 21 */
	{ 2,	0,	sys_capset,	"capset" },		/* 22 */
	{ 1,	0,	sys_setuid,	"setuid" },		/* 23 */
	{ 0,	NF,	sys_getuid,	"getuid" },		/* 24 */
	{ 1,	0,	sys_time,	"time" },		/* 25 */
	{ 5,	0,	sys_ptrace,	"ptrace" },		/* 26 */
	{ 1,	0,	sys_alarm,	"alarm" },		/* 27 */
	{ 2,	TS,	sys_sigaltstack,"sigaltstack" },	/* 28 */
	{ 0,	TS,	sys_pause,	"pause" },		/* 29 */
	{ 2,	TF,	sys_utime,	"utime" },		/* 30 */
	{ 3,	TF,	sys_chown,	"lchown32" },		/* 31 */
	{ 3,	TD,	sys_fchown,	"fchown32" },		/* 32 */
	{ 2,	TF,	sys_access,	"access" },		/* 33 */
	{ 1,	0,	sys_nice,	"nice" },		/* 34 */
	{ 3,	TF,	sys_chown,	"chown32" },		/* 35 */
	{ 0,	0,	sys_sync,	"sync" },		/* 36 */
	{ 2,	TS,	sys_kill,	"kill" },		/* 37 */
	{ 2,	TF,	sys_stat,	"stat" },		/* 38 */
	{ 4,	TD|TN,	sys_sendfile,	"sendfile" },		/* 39 */
	{ 2,	TF,	sys_lstat,	"lstat" },		/* 40 */
	{ 2,	TD,	sys_dup,	"dup" },		/* 41 */
	{ 0,	TD,	sys_pipe,	"pipe" },		/* 42 */
	{ 1,	0,	sys_times,	"times" },		/* 43 */
	{ 0,	NF,	sys_getuid,	"getuid32" },		/* 44 */
	{ 2,	TF,	sys_umount2,	"umount" },		/* 45 */
	{ 1,	0,	sys_setgid,	"setgid" },		/* 46 */
	{ 0,	NF,	sys_getgid,	"getgid" },		/* 47 */
	{ 3,	TS,	sys_signal,	"signal" },		/* 48 */
	{ 0,	NF,	sys_geteuid,	"geteuid" },		/* 49 */
	{ 0,	NF,	sys_getegid,	"getegid" },		/* 50 */
	{ 1,	TF,	sys_acct,	"acct" },		/* 51 */
	{ 2,	0,	printargs,	"memory_ordering" },	/* 52 */
	{ 0,	NF,	sys_getgid,	"getgid32" },		/* 53 */
	{ 3,	TD,	sys_ioctl,	"ioctl" },		/* 54 */
	{ 3,	0,	sys_reboot,	"reboot" },		/* 55 */
	{ 6,	TD,	sys_mmap,	"mmap2" },		/* 56 */
	{ 2,	TF,	sys_symlink,	"symlink" },		/* 57 */
	{ 3,	TF,	sys_readlink,	"readlink" },		/* 58 */
	{ 3,	TF|TP,	sys_execve,	"execve" },		/* 59 */
	{ 1,	0,	sys_umask,	"umask" },		/* 60 */
	{ 1,	TF,	sys_chroot,	"chroot" },		/* 61 */
	{ 2,	TD,	sys_fstat,	"fstat" },		/* 62 */
	{ 2,	TD,	sys_fstat64,	"fstat64" },		/* 63 */
	{ 0,	0,	sys_getpagesize,"getpagesize" },	/* 64 */
	{ 3,	0,	sys_msync,	"msync" },		/* 65 */
	{ 0,	TP,	sys_vfork,	"vfork" },		/* 66 */
	{ 5,	TD,	sys_pread,	"pread" },		/* 67 */
	{ 5,	TD,	sys_pwrite,	"pwrite" },		/* 68 */
	{ 0,    NF,	sys_geteuid,	"geteuid32" },		/* 69 */
	{ 0,	NF,	sys_getegid,	"getegid32" },		/* 70 */
	{ 6,	TD,	sys_mmap,	"mmap" },		/* 71 */
	{ 2,	0,	sys_setreuid,	"setreuid32" },		/* 72 */
	{ 2,	0,	sys_munmap,	"munmap" },		/* 73 */
	{ 3,	0,	sys_mprotect,	"mprotect" },		/* 74 */
	{ 3,	0,	sys_madvise,	"madvise" },		/* 75 */
	{ 0,	0,	sys_vhangup,	"vhangup" },		/* 76 */
	{ 3,	TF,	sys_truncate64,	"truncate64" },		/* 77 */
	{ 3,	0,	sys_mincore,	"mincore" },		/* 78 */
	{ 2,	0,	sys_getgroups,	"getgroups" },		/* 79 */
	{ 2,	0,	sys_setgroups,	"setgroups" },		/* 80 */
	{ 0,	0,	sys_getpgrp,	"getpgrp" },		/* 81 */
	{ 2,	0,	sys_setgroups32,"setgroups32" },	/* 82 */
	{ 3,	0,	sys_setitimer,	"setitimer" },		/* 83 */
	{ 2,	TD,	sys_ftruncate,	"ftruncate64" },	/* 84 */
	{ 1,	TF,	sys_swapon,	"swapon" },		/* 85 */
	{ 2,	0,	sys_getitimer,	"getitimer" },		/* 86 */
	{ 1,	0,	sys_setuid,	"setuid32" },		/* 87 */
	{ 2,	0,	sys_sethostname,"sethostname" },	/* 88 */
	{ 1,	0,	sys_setgid,	"setgid32" },		/* 89 */
	{ 2,	TD,	sys_dup2,	"dup2" },		/* 90 */
	{ 1,	NF,	sys_setfsuid,	"setfsuid32" },		/* 91 */
	{ 3,	TD,	sys_fcntl,	"fcntl" },		/* 92 */
	{ 5,	TD,	sys_select,	"select" },		/* 93 */
	{ 1,	NF,	sys_setfsgid,	"setfsgid32" },		/* 94 */
	{ 1,	TD,	sys_fsync,	"fsync" },		/* 95 */
	{ 3,	0,	sys_setpriority,"setpriority" },	/* 96 */
	{ 3,	TN,	sys_socket,	"socket" },		/* 97 */
	{ 3,	TN,	sys_connect,	"connect" },		/* 98 */
	{ 3,	TN,	sys_accept,	"accept" },		/* 99 */
	{ 2,	0,	sys_getpriority,"getpriority" },	/* 100 */
	{ 1,	TS,	printargs,	"rt_sigreturn" },	/* 101 */
	{ 4,	TS,	sys_rt_sigaction,"rt_sigaction" },	/* 102 */
	{ 4,	TS,	sys_rt_sigprocmask,"rt_sigprocmask" },	/* 103 */
	{ 2,	TS,	sys_rt_sigpending,"rt_sigpending" },	/* 104 */
	{ 4,	TS,	sys_rt_sigtimedwait,"rt_sigtimedwait" },/* 105 */
	{ 3,	TS,	sys_rt_sigqueueinfo,"rt_sigqueueinfo" },/* 106 */
	{ 2,	TS,	sys_rt_sigsuspend,"rt_sigsuspend" },	/* 107 */
	{ 3,	TS,	sys_setresuid,	"setresuid" },		/* 108 */
	{ 3,    TS,	sys_getresuid,	"getresuid" },		/* 109 */
	{ 3,	TS,	sys_setresgid,	"setresgid" },		/* 110 */
	{ 3,	TS,	sys_getresgid,	"getresgid" },		/* 111 */
	{ 2,	TS,	sys_setresgid,	"setresgid32" },	/* 112 */
	{ 5,	TN,	sys_recvmsg,	"recvmsg" },		/* 113 */
	{ 5,	TN,	sys_sendmsg,	"sendmsg" },		/* 114 */
	{ 2,	0,	sys_getgroups32,"getgroups32" },	/* 115 */
	{ 2,	0,	sys_gettimeofday,"gettimeofday" },	/* 116 */
	{ 2,	0,	sys_getrusage,	"getrusage" },		/* 117 */
	{ 5,	TN,	sys_getsockopt,	"getsockopt" },		/* 118 */
	{ 2,	TF,	sys_getcwd,	"getcwd" },		/* 119 */
	{ 3,	TD,	sys_readv,	"readv" },		/* 120 */
	{ 3,	TD,	sys_writev,	"writev" },		/* 121 */
	{ 2,	0,	sys_settimeofday,"settimeofday" },	/* 122 */
	{ 3,	TD,	sys_fchown,	"fchown" },		/* 123 */
	{ 2,	TD,	sys_fchmod,	"fchmod" },		/* 124 */
	{ 6,	TN,	sys_recvfrom,	"recvfrom" },		/* 125 */
	{ 2,	0,	sys_setreuid,	"setreuid" },		/* 126 */
	{ 2,	0,	sys_setregid,	"setregid" },		/* 127 */
	{ 2,	TF,	sys_rename,	"rename" },		/* 128 */
	{ 2,	TF,	sys_truncate,	"truncate" },		/* 129 */
	{ 2,	TD,	sys_ftruncate,	"ftruncate" },		/* 130 */
	{ 2,	TD,	sys_flock,	"flock" },		/* 131 */
	{ 2,	TF,	sys_lstat64,	"lstat64" },		/* 132 */
	{ 6,	TN,	sys_sendto,	"sendto" },		/* 133 */
	{ 2,	TN,	sys_shutdown,	"shutdown" },		/* 134 */
	{ 4,	TN,	sys_socketpair,	"socketpair" },		/* 135 */
	{ 2,	TF,	sys_mkdir,	"mkdir" },		/* 136 */
	{ 1,	TF,	sys_rmdir,	"rmdir" },		/* 137 */
	{ 2,	TF,	sys_utimes,	"utimes" },		/* 138 */
	{ 2,	TF,	sys_stat64,	"stat64" },		/* 139 */
	{ 4,    TD|TN,	sys_sendfile64,	"sendfile64" },		/* 140 */
	{ 3,	TN,	sys_getpeername,"getpeername" },	/* 141 */
	{ 6,    0,	sys_futex,	"futex" },		/* 142 */
	{ 0,	0,	printargs,	"gettid" },		/* 143 */
	{ 2,	0,	sys_getrlimit,	"getrlimit" },		/* 144 */
	{ 2,	0,	sys_setrlimit,	"setrlimit" },		/* 145 */
	{ 2,	TF,	sys_pivotroot,	"pivot_root" },		/* 146 */
	{ 5,	0,	sys_prctl,	"prctl" },		/* 147 */
	{ 5,	0,	printargs,	"pciconfig_read" },	/* 148 */
	{ 5,	0,	printargs,	"pciconfig_write" },	/* 149 */
	{ 3,	TN,	sys_getsockname,"getsockname" },	/* 150 */
	{ 4,	TN,	sys_getmsg,	"getmsg" },		/* 151 */
	{ 4,	TN,	sys_putmsg,	"putmsg" },		/* 152 */
	{ 3,	TD,	sys_poll,	"poll" },		/* 153 */
	{ 3,	TD,	sys_getdents64,	"getdents64" },		/* 154 */
	{ 3,	TD,	sys_fcntl,	"fcntl64" },		/* 155 */
	{ 4,	0,	printargs,	"getdirentries" },	/* 156 */
	{ 2,	TF,	sys_statfs,	"statfs" },		/* 157 */
	{ 2,	TD,	sys_fstatfs,	"fstatfs" },		/* 158 */
	{ 1,	TF,	sys_umount,	"oldumount" },		/* 159 */
	{ 3,	0,	sys_sched_setaffinity,	"sched_setaffinity" },/* 160 */
	{ 3,	0,	sys_sched_getaffinity,	"sched_getaffinity" },/* 161 */
	{ 2,	0,	printargs,	"getdomainname" },	/* 162 */
	{ 2,	0,	sys_setdomainname,"setdomainname" },	/* 163 */
	{ 5,	0,	printargs,	"utrap_install" },	/* 164 */
	{ 4,	0,	sys_quotactl,	"quotactl" },		/* 165 */
	{ 1,	0,	printargs,	"set_tid_address" },	/* 166 */
	{ 5,	TF,	sys_mount,	"mount" },		/* 167 */
	{ 2,	0,	sys_ustat,	"ustat" },		/* 168 */
	{ 5,	TF,	sys_setxattr,	"setxattr" },		/* 169 */
	{ 5,	TF,	sys_setxattr,	"lsetxattr" },		/* 170 */
	{ 5,	TD,	sys_fsetxattr,	"fsetxattr" },		/* 171 */
	{ 4,	TF,	sys_getxattr,	"getxattr" },		/* 172 */
	{ 4,	TF,	sys_getxattr,	"lgetxattr" },		/* 173 */
	{ 3,	TD,	sys_getdents,	"getdents" },		/* 174 */
	{ 0,	0,	sys_setsid,	"setsid" },		/* 175 */
	{ 1,	TD,	sys_fchdir,	"fchdir" },		/* 176 */
	{ 4,    TD,	sys_fgetxattr,	"fgetxattr" },		/* 177 */
	{ 3,	TF,	sys_listxattr,	"listxattr" },		/* 178 */
	{ 3,	TF,	sys_listxattr,	"llistxattr" },		/* 179 */
	{ 3,	TD,	sys_flistxattr,	"flistxattr" },		/* 180 */
	{ 2,	TF,	sys_removexattr,"removexattr" },	/* 181 */
	{ 2,	TF,	sys_removexattr,"lremovexattr" },	/* 182 */
	{ 1,	TS,	sys_sigpending,	"sigpending" },		/* 183 */
	{ 5,	0,	sys_query_module,"query_module" },	/* 184 */
	{ 2,	0,	sys_setpgid,	"setpgid" },		/* 185 */
	{ 2,	TD,	sys_fremovexattr,"fremovexattr" },	/* 186 */
	{ 2,	TS,	sys_kill,	"tkill" },		/* 187 */
	{ 1,	TP,	sys_exit,	"exit_group" },		/* 188 */
	{ 1,	0,	sys_uname,	"uname" },		/* 189 */
	{ 3,	0,	sys_init_module,"init_module" },	/* 190 */
	{ 1,	0,	sys_personality,"personality" },	/* 191 */
	{ 5,	0,	sys_remap_file_pages,"remap_file_pages" },/* 192 */
	{ 1,	TD,	sys_epoll_create,"epoll_create" },	/* 193 */
	{ 4,	TD,	sys_epoll_ctl,	"epoll_ctl" },		/* 194 */
	{ 4,	TD,	sys_epoll_wait,	"epoll_wait" },		/* 195 */
	{ 2,	0,	sys_ulimit,	"ulimit" },		/* 196 */
	{ 0,	0,	sys_getppid,	"getppid" },		/* 197 */
	{ 3,	TS,	sys_sigaction,	"sigaction" },		/* 198 */
	{ 5,	0,	printargs,	"sgetmask" },		/* 199 */
	{ 5,	0,	printargs,	"ssetmask" },		/* 200 */
	{ 3,	TS,	sys_sigsuspend,	"sigsuspend" },		/* 201 */
	{ 2,	TF,	sys_lstat,	"lstat" },		/* 202 */
	{ 1,	TF,	sys_uselib,	"uselib" },		/* 203 */
	{ 3,	TD,	sys_readdir,	"readdir" },		/* 204 */
	{ 4,	TD,	sys_readahead,	"readahead" },		/* 205 */
	{ 2,	TD,	sys_socketcall,	"socketcall" },		/* 206 */
	{ 3,	0,	sys_syslog,	"syslog" },		/* 207 */
	{ 4,	0,	printargs,	"lookup_dcookie" },	/* 208 */
	{ 6,	TD,	printargs,	"fadvise64" },		/* 209 */
	{ 6,	TD,	printargs,	"fadvise64_64" },	/* 210 */
	{ 3,	TS,	sys_tgkill,	"tgkill" },		/* 211 */
	{ 3,	TP,	sys_waitpid,	"waitpid" },		/* 212 */
	{ 1,	TF,	sys_swapoff,	"swapoff" },		/* 213 */
	{ 1,	0,	sys_sysinfo,	"sysinfo" },		/* 214 */
	{ 5,	0,	sys_ipc,	"ipc" },		/* 215 */
	{ 1,	TS,	sys_sigreturn,	"sigreturn" },		/* 216 */
	{ 5,	TP,	sys_clone,	"clone" },		/* 217 */
	{ 3,	0,	sys_modify_ldt,	"modify_ldt" },		/* 218 */
	{ 1,	0,	sys_adjtimex,	"adjtimex" },		/* 219 */
	{ 3,	TS,	sys_sigprocmask,"sigprocmask" },	/* 220 */
	{ 2,	0,	sys_create_module,"create_module" },	/* 221 */
	{ 2,	0,	sys_delete_module,"delete_module" },
	{ 1,	0,	sys_get_kernel_syms,"get_kernel_syms"},	/* 223 */
	{ 1,	0,	sys_getpgid,	"getpgid" },		/* 224 */
	{ 0,	0,	sys_bdflush,	"bdflush" },		/* 225 */
	{ 3,	0,	sys_sysfs,	"sysfs" },		/* 226 */
	{ 5,	0,	sys_afs_syscall,"afs_syscall" },	/* 227 */
	{ 1,	NF,	sys_setfsuid,	"setfsuid" },		/* 228 */
	{ 1,	NF,	sys_setfsgid,	"setfsgid" },		/* 229 */
	{ 5,	TD,	sys_select,	"select" },		/* 230 */
	{ 1,	0,	sys_time,	"time" },		/* 231 */
	{ 2,	TF,	sys_stat,	"stat" },		/* 232 */
	{ 1,	0,	sys_stime,	"stime" },		/* 233 */
	{ 3,	TF,	sys_statfs64,	"statfs64" },		/* 234 */
	{ 3,	TD,	sys_fstatfs64,	"fstatfs64" },		/* 235 */
	{ 5,	TD,	sys_llseek,	"_llseek" },		/* 236 */
	{ 2,	0,	sys_mlock,	"mlock" },		/* 237 */
	{ 2,	0,	sys_munlock,	"munlock" },		/* 238 */
	{ 2,	0,	sys_mlockall,	"mlockall" },		/* 239 */
	{ 0,	0,	sys_munlockall,	"munlockall" },		/* 240 */
	{ 2,	0,	sys_sched_setparam,"sched_setparam"},	/* 241 */
	{ 2,	0,	sys_sched_getparam,"sched_getparam"},	/* 242 */
	{ 3,	0,	sys_sched_setscheduler,"sched_setscheduler"},/* 243 */
	{ 1,	0,	sys_sched_getscheduler,"sched_getscheduler"},/* 244 */
	{ 0,	0,	sys_sched_yield,"sched_yield" },	/* 245 */
	{ 1,0,sys_sched_get_priority_max,"sched_get_priority_max"},/* 246 */
	{ 1,0,sys_sched_get_priority_min,"sched_get_priority_min"},/* 247 */
	{ 2,	0,sys_sched_rr_get_interval,"sched_rr_get_interval"},/* 248 */
	{ 2,	0,	sys_nanosleep,	"nanosleep" },		/* 249 */
	{ 5,	0,	sys_mremap,	"mremap" },		/* 250 */
	{ 1,	0,	sys_sysctl,	"_sysctl" },		/* 251 */
	{ 1,	0,	sys_getsid,	"getsid" },		/* 252 */
	{ 1,	TD,	sys_fdatasync,	"fdatasync" },		/* 253 */
	{ 3,	0,	printargs,	"nfsservctl" },		/* 254 */
	{ 5,	0,	printargs,	"aplib" },		/* 255 */
	{ 2,	0,	sys_clock_settime,"clock_settime" },	/* 256 */
	{ 2,	0,	sys_clock_gettime,"clock_gettime" },	/* 257 */
	{ 2,	0,	sys_clock_getres,"clock_getres" },	/* 258 */
	{ 4,	0,	sys_clock_nanosleep,"clock_nanosleep" },/* 259 */
	{ 3,	0,	sys_sched_setaffinity,"sched_setaffinity" },/* 260 */
	{ 3,	0,	sys_sched_getaffinity,"sched_getaffinity" },/* 261 */
	{ 4,	0,	sys_timer_settime,"timer_settime" },	/* 262 */
	{ 2,	0,	sys_timer_gettime,"timer_gettime" },	/* 263 */
	{ 1,	0,	sys_timer_getoverrun,"timer_getoverrun" },/* 264 */
	{ 1,	0,	sys_timer_delete,"timer_delete" },	/* 265 */
	{ 3,	0,	sys_timer_create,"timer_create" },	/* 266 */
	{ 5,	0,	printargs,	"SYS_267" },		/* 267 */
	{ 2,	0,	sys_io_setup,		"io_setup"	}, /* 268 */
	{ 1,	0,	sys_io_destroy,		"io_destroy"	}, /* 269 */
	{ 3,	0,	sys_io_submit,		"io_submit"	}, /* 270 */
	{ 3,	0,	sys_io_cancel,		"io_cancel"	}, /* 271 */
	{ 5,	0,	sys_io_getevents,	"io_getevents"	}, /* 272 */
	{ 4,	0,	sys_mq_open,		"mq_open"	}, /* 273 */
	{ 1,	0,	sys_mq_unlink,		"mq_unlink"	}, /* 274 */
	{ 5,	0,	sys_mq_timedsend,	"mq_timedsend"	}, /* 275 */
	{ 5,	0,	sys_mq_timedreceive,	"mq_timedreceive" }, /* 276 */
	{ 2,	0,	sys_mq_notify,		"mq_notify"	}, /* 277 */
	{ 3,	0,	sys_mq_getsetattr,	"mq_getsetattr"	}, /* 278 */
	{ 5,	TP,	sys_waitid,		"waitid"	}, /* 279 */
	{ 4,	TD,	printargs,		"tee"		}, /* 280 */
	{ 5,	0,	printargs,		"add_key"	}, /* 281 */
	{ 4,	0,	printargs,		"request_key"	}, /* 282 */
	{ 5,	0,	printargs,		"keyctl"	}, /* 283 */
	{ 4,	TD|TF,	sys_openat,		"openat"	}, /* 284 */
	{ 3,	TD|TF,	sys_mkdirat,		"mkdirat"	}, /* 285 */
	{ 4,	TD|TF,	sys_mknodat,		"mknodat"	}, /* 286 */
	{ 5,	TD|TF,	sys_fchownat,		"fchownat"	}, /* 287 */
	{ 3,	TD|TF,	sys_futimesat,		"futimesat"	}, /* 288 */
	{ 4,	TD|TF,	sys_newfstatat,		"fstatat64"	}, /* 289 */
	{ 3,	TD|TF,	sys_unlinkat,		"unlinkat"	}, /* 290 */
	{ 4,	TD|TF,	sys_renameat,		"renameat"	}, /* 291 */
	{ 5,	TD|TF,	sys_linkat,		"linkat"	}, /* 292 */
	{ 3,	TD|TF,	sys_symlinkat,		"symlinkat"	}, /* 293 */
	{ 4,	TD|TF,	sys_readlinkat,		"readlinkat"	}, /* 294 */
	{ 3,	TD|TF,	sys_fchmodat,		"fchmodat"	}, /* 295 */
	{ 3,	TD|TF,	sys_faccessat,		"faccessat"	}, /* 296 */
	{ 6,	TD,	sys_pselect6,		"pselect6"	}, /* 297 */
	{ 5,	TD,	sys_ppoll,		"ppoll"		}, /* 298 */
	{ 1,	TP,	sys_unshare,		"unshare"	}, /* 299 */
	{ 2,	0,	printargs,		"set_robust_list" }, /* 300 */
	{ 3,	0,	printargs,		"get_robust_list" }, /* 301 */
	{ 4,	0,	printargs,		"migrate_pages"	}, /* 302 */
	{ 6,	0,	sys_mbind,		"mbind"		}, /* 303 */
	{ 5,	0,	sys_get_mempolicy,	"get_mempolicy"	}, /* 304 */
	{ 3,	0,	sys_set_mempolicy,	"set_mempolicy"	}, /* 305 */
	{ 5,	0,	printargs,		"kexec_load"	}, /* 306 */
	{ 6,	0,	sys_move_pages,		"move_pages"	}, /* 307 */
	{ 3,	0,	sys_getcpu,		"getcpu"	}, /* 308 */
	{ 5,	TD,	sys_epoll_pwait,	"epoll_pwait"	}, /* 309 */
	{ 4,	TD|TF,	sys_utimensat,		"utimensat"	}, /* 310 */
	{ 3,	TD|TS,	sys_signalfd,		"signalfd"	}, /* 311 */
	{ 2,	TD,	sys_timerfd_create,	"timerfd_create"}, /* 312 */
	{ 1,	TD,	sys_eventfd,		"eventfd"	}, /* 313 */
	{ 6,	TD,	sys_fallocate,		"fallocate"	}, /* 314 */
	{ 4,	TD,	sys_timerfd_settime,	"timerfd_settime"}, /* 315 */
	{ 2,	TD,	sys_timerfd_gettime,	"timerfd_gettime"}, /* 316 */
	{ 4,	TD|TS,	sys_signalfd4,		"signalfd4"	}, /* 317 */
	{ 2,	TD,	sys_eventfd2,		"eventfd2"	}, /* 318 */
	{ 1,	TD,	sys_epoll_create1,	"epoll_create1"	}, /* 319 */
	{ 3,	TD,	sys_dup3,		"dup3"		}, /* 320 */
	{ 2,	TD,	sys_pipe2,		"pipe2"		}, /* 321 */
	{ 1,	TD,	sys_inotify_init1,	"inotify_init1"	}, /* 322 */
	{ 4,	TN,	sys_accept4,		"accept4"	}, /* 323 */
	{ 5,	TD,	printargs,		"preadv"	}, /* 324 */
	{ 5,	TD,	printargs,		"pwritev"	}, /* 325 */
	{ 4,	TP|TS,	printargs,		"rt_tgsigqueueinfo"}, /* 326 */
	{ 5,	TD,	printargs,		"perf_event_open"}, /* 327 */
	{ 5,	TN,	sys_recvmmsg,		"recvmmsg"	}, /* 328 */
	{ 2,	TD,	printargs,		"fanotify_init"	}, /* 329 */
	{ 5,	TD|TF,	printargs,		"fanotify_mark"	}, /* 330 */
	{ 4,	0,	printargs,		"prlimit64"	}, /* 331 */
	{ 5,	0,	printargs,	"SYS_332" },		/* 332 */
	{ 5,	0,	printargs,	"SYS_333" },		/* 333 */
	{ 5,	0,	printargs,	"SYS_334" },		/* 334 */
	{ 5,	0,	printargs,	"SYS_335" },		/* 335 */
	{ 5,	0,	printargs,	"SYS_336" },		/* 336 */
	{ 5,	0,	printargs,	"SYS_337" },		/* 337 */
	{ 5,	0,	printargs,	"SYS_338" },		/* 338 */
	{ 5,	0,	printargs,	"SYS_339" },		/* 339 */
	{ 5,	0,	printargs,	"SYS_340" },		/* 340 */
	{ 5,	0,	printargs,	"SYS_341" },		/* 341 */
	{ 5,	0,	printargs,	"SYS_342" },		/* 342 */
	{ 5,	0,	printargs,	"SYS_343" },		/* 343 */
	{ 5,	0,	printargs,	"SYS_344" },		/* 344 */
	{ 5,	0,	printargs,	"SYS_345" },		/* 345 */
	{ 5,	0,	printargs,	"SYS_346" },		/* 346 */
	{ 5,	0,	printargs,	"SYS_347" },		/* 347 */
	{ 5,	0,	printargs,	"SYS_348" },		/* 348 */
	{ 5,	0,	printargs,	"SYS_349" },		/* 349 */
	{ 5,	0,	printargs,	"SYS_350" },		/* 350 */
	{ 5,	0,	printargs,	"SYS_351" },		/* 351 */
	{ 5,	0,	printargs,	"SYS_352" },		/* 352 */
#if SYS_socket_subcall != 353
 #error fix me
#endif
	{ 8,	0,	printargs,		"socket_subcall"}, /* 353 */
	{ 3,	TN,	sys_socket,		"socket"	}, /* 354 */
	{ 3,	TN,	sys_bind,		"bind"		}, /* 355 */
	{ 3,	TN,	sys_connect,		"connect"	}, /* 356 */
	{ 2,	TN,	sys_listen,		"listen"	}, /* 357 */
	{ 3,	TN,	sys_accept,		"accept"	}, /* 358 */
	{ 3,	TN,	sys_getsockname,	"getsockname"	}, /* 359 */
	{ 3,	TN,	sys_getpeername,	"getpeername"	}, /* 360 */
	{ 4,	TN,	sys_socketpair,		"socketpair"	}, /* 361 */
	{ 4,	TN,	sys_send,		"send"		}, /* 362 */
	{ 4,	TN,	sys_recv,		"recv"		}, /* 363 */
	{ 6,	TN,	sys_sendto,		"sendto"	}, /* 364 */
	{ 6,	TN,	sys_recvfrom,		"recvfrom"	}, /* 365 */
	{ 2,	TN,	sys_shutdown,		"shutdown"	}, /* 366 */
	{ 5,	TN,	sys_setsockopt,		"setsockopt"	}, /* 367 */
	{ 5,	TN,	sys_getsockopt,		"getsockopt"	}, /* 368 */
	{ 5,	TN,	sys_sendmsg,		"sendmsg"	}, /* 369 */
	{ 5,	TN,	sys_recvmsg,		"recvmsg"	}, /* 370 */
	{ 4,	TN,	sys_accept4,		"accept4"	}, /* 371 */
	{ 5,	TN,	sys_recvmmsg,		"recvmmsg"	}, /* 372 */
#if SYS_ipc_subcall != 373
 #error fix me
#endif
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 373 */
	{ 4,	TI,	printargs,		"semop"		}, /* 374 */
	{ 4,	TI,	sys_semget,		"semget"	}, /* 375 */
	{ 4,	TI,	sys_semctl,		"semctl"	}, /* 376 */
	{ 5,	TI,	sys_semtimedop,		"semtimedop"	}, /* 377 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 378 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 379 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 380 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 381 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 382 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 383 */
	{ 4,	TI,	sys_msgsnd,		"msgsnd"	}, /* 384 */
	{ 4,	TI,	sys_msgrcv,		"msgrcv"	}, /* 385 */
	{ 4,	TI,	sys_msgget,		"msgget"	}, /* 386 */
	{ 4,	TI,	sys_msgctl,		"msgctl"	}, /* 387 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 388 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 389 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 390 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 391 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 392 */
	{ 4,	0,	printargs,		"ipc_subcall"	}, /* 393 */
	{ 4,	TI,	sys_shmat,		"shmat"		}, /* 394 */
	{ 4,	TI,	sys_shmdt,		"shmdt"		}, /* 395 */
	{ 4,	TI,	sys_shmget,		"shmget"	}, /* 396 */
	{ 4,	TI,	sys_shmctl,		"shmctl"	}, /* 397 */
	{ 5,	0,	printargs,		"SYS_397"	}, /* 398 */
	{ 5,	0,	printargs,		"SYS_398"	}, /* 399 */
	{ 5,	0,	printargs,		"SYS_399"	}, /* 400 */
	{ 5,	0,	printargs,		"SYS_400"	}, /* 401 */

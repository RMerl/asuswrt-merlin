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

	{ 6,	0,	printargs,		"osf_syscall"		}, /* 0, not implemented */
	{ 1,	TP,	sys_exit,		"exit"			}, /* 1 */
	{ 0,	TP,	sys_fork,		"fork"			}, /* 2 */
	{ 3,	TD,	sys_read,		"read"			}, /* 3 */
	{ 3,	TD,	sys_write,		"write"			}, /* 4 */
	{ 5,	0,	printargs,		"osf_old_open"		}, /* 5, not implemented */
	{ 1,	TD,	sys_close,		"close"			}, /* 6 */
	{ 4,	TP,	sys_osf_wait4,		"osf_wait4"		}, /* 7 */
	{ 5,	0,	printargs,		"osf_old_creat"		}, /* 8, not implemented */
	{ 2,	TF,	sys_link,		"link"			}, /* 9 */
	{ 1,	TF,	sys_unlink,		"unlink"		}, /* 10 */
	{ 5,	0,	printargs,		"osf_execve"		}, /* 11, not implemented */
	{ 1,	TF,	sys_chdir,		"chdir"			}, /* 12 */
	{ 1,	TF,	sys_fchdir,		"fchdir"		}, /* 13 */
	{ 3,	TF,	sys_mknod,		"mknod"			}, /* 14 */
	{ 2,	TF,	sys_chmod,		"chmod"			}, /* 15 */
	{ 3,	TF,	sys_chown,		"chown"			}, /* 16 */
	{ 1,	0,	sys_brk,		"brk"			}, /* 17 */
	{ 5,	0,	printargs,		"osf_getfsstat"		}, /* 18, not implemented */
	{ 3,	TF,	sys_lseek,		"lseek"			}, /* 19 */
	{ 0,	0,	sys_getpid,		"getxpid"		}, /* 20 */
	{ 4,	0,	printargs,		"osf_mount"		}, /* 21 */
	{ 2,	0,	sys_umount2,		"umount"		}, /* 22 */
	{ 1,	0,	sys_setuid,		"setuid"		}, /* 23 */
	{ 0,	NF,	sys_getuid,		"getxuid"		}, /* 24 */
	{ 5,	0,	printargs,		"exec_with_loader"	}, /* 25, not implemented */
	{ 4,	0,	sys_ptrace,		"ptrace"		}, /* 26 */
	{ 5,	0,	printargs,		"osf_nrecvmsg"		}, /* 27, not implemented */
	{ 5,	0,	printargs,		"osf_nsendmsg"		}, /* 28, not implemented */
	{ 5,	0,	printargs,		"osf_nrecvfrom"		}, /* 29, not implemented */
	{ 5,	0,	printargs,		"osf_naccept"		}, /* 30, not implemented */
	{ 5,	0,	printargs,		"osf_ngetpeername"	}, /* 31, not implemented */
	{ 5,	0,	printargs,		"osf_ngetsockname"	}, /* 32, not implemented */
	{ 2,	TF,	sys_access,		"access"		}, /* 33 */
	{ 5,	0,	printargs,		"osf_chflags"		}, /* 34, not implemented */
	{ 5,	0,	printargs,		"osf_fchflags"		}, /* 35, not implemented */
	{ 0,	0,	sys_sync,		"sync"			}, /* 36 */
	{ 2,	TS,	sys_kill,		"kill"			}, /* 37 */
	{ 5,	0,	printargs,		"osf_old_stat"		}, /* 38, not implemented */
	{ 2,	0,	sys_setpgid,		"setpgid"		}, /* 39 */
	{ 5,	0,	printargs,		"osf_old_lstat"		}, /* 40, not implemented */
	{ 1,	TD,	sys_dup,		"dup"			}, /* 41 */
	{ 1,	TD,	sys_pipe,		"pipe"			}, /* 42 */
	{ 5,	0,	printargs,		"osf_set_program_attributes"	}, /* 43 */
	{ 5,	0,	printargs,		"osf_profil"		}, /* 44, not implemented */
	{ 3,	TD|TF,	sys_open,		"open"			}, /* 45 */
	{ 5,	0,	printargs,		"osf_old_sigaction"	}, /* 46, not implemented */
	{ 1,	NF,	sys_getgid,		"getxgid"		}, /* 47 */
	{ 3,	TS,	printargs,		"osf_sigprocmask"	}, /* 48 */
	{ 5,	0,	printargs,		"osf_getlogin"		}, /* 49, not implemented */
	{ 5,	0,	printargs,		"osf_setlogin"		}, /* 50, not implemented */
	{ 1,	TF,	sys_acct,		"acct"			}, /* 51 */
	{ 1,	TS,	sys_sigpending,		"sigpending"		}, /* 52 */
	{ 5,	0,	printargs,		"SYS_53"		}, /* 53 */
	{ 3,	TD,	sys_ioctl,		"ioctl"			}, /* 54 */
	{ 5,	0,	printargs,		"osf_reboot"		}, /* 55, not implemented */
	{ 5,	0,	printargs,		"osf_revoke"		}, /* 56, not implemented */
	{ 2,	TF,	sys_symlink,		"symlink"		}, /* 57 */
	{ 3,	TF,	sys_readlink,		"readlink"		}, /* 58 */
	{ 3,	TF|TP,	sys_execve,		"execve"		}, /* 59 */
	{ 1,	0,	sys_umask,		"umask"			}, /* 60 */
	{ 1,	TF,	sys_chroot,		"chroot"		}, /* 61 */
	{ 5,	0,	printargs,		"osf_old_fstat"		}, /* 62, not implemented */
	{ 0,	0,	sys_getpgrp,		"getpgrp"		}, /* 63 */
	{ 0,	0,	sys_getpagesize,	"getpagesize"		}, /* 64 */
	{ 5,	0,	printargs,		"osf_mremap"		}, /* 65, not implemented */
	{ 0,	TP,	sys_fork,		"vfork"			}, /* 66 */
	{ 2,	TF,	sys_stat,		"stat"			}, /* 67 */
	{ 2,	TF,	sys_lstat,		"lstat"			}, /* 68 */
	{ 5,	0,	printargs,		"osf_sbrk"		}, /* 69, not implemented */
	{ 5,	0,	printargs,		"osf_sstk"		}, /* 70, not implemented */
	{ 6,	TD,	sys_mmap,		"mmap"			}, /* 71 */
	{ 5,	0,	printargs,		"osf_old_vadvise"	}, /* 72, not implemented */
	{ 2,	0,	sys_munmap,		"munmap"		}, /* 73 */
	{ 3,	0,	sys_mprotect,		"mprotect"		}, /* 74 */
	{ 3,	0,	sys_madvise,		"madvise"		}, /* 75 */
	{ 0,	0,	sys_vhangup,		"vhangup"		}, /* 76 */
	{ 5,	0,	printargs,		"osf_kmodcall"		}, /* 77, not implemented */
	{ 5,	0,	printargs,		"osf_mincore"		}, /* 78, not implemented */
	{ 2,	0,	sys_getgroups,		"getgroups"		}, /* 79 */
	{ 2,	0,	sys_setgroups,		"setgroups"		}, /* 80 */
	{ 5,	0,	printargs,		"osf_old_getpgrp"	}, /* 81, not implemented */
	{ 2,	0,	sys_setpgrp,		"setpgrp"		}, /* 82 */
	{ 3,	0,	sys_osf_setitimer,	"osf_setitimer"		}, /* 83 */
	{ 5,	0,	printargs,		"osf_old_wait"		}, /* 84, not implemented */
	{ 5,	0,	printargs,		"osf_table"		}, /* 85, not implemented */
	{ 2,	0,	sys_osf_getitimer,	"osf_getitimer"		}, /* 86 */
	{ 2,	0,	sys_gethostname,	"gethostname"		}, /* 87 */
	{ 2,	0,	sys_sethostname,	"sethostname"		}, /* 88 */
	{ 0,	0,	sys_getdtablesize,	"getdtablesize"		}, /* 89 */
	{ 2,	TD,	sys_dup2,		"dup2"			}, /* 90 */
	{ 2,	TD,	sys_fstat,		"fstat"			}, /* 91 */
	{ 3,	TD,	sys_fcntl,		"fcntl"			}, /* 92 */
	{ 5,	0,	sys_osf_select,		"osf_select"		}, /* 93 */
	{ 3,	TD,	sys_poll,		"poll"			}, /* 94 */
	{ 1,	TD,	sys_fsync,		"fsync"			}, /* 95 */
	{ 3,	0,	sys_setpriority,	"setpriority"		}, /* 96 */
	{ 3,	TN,	sys_socket,		"socket"		}, /* 97 */
	{ 3,	TN,	sys_connect,		"connect"		}, /* 98 */
	{ 3,	TN,	sys_accept,		"accept"		}, /* 99 */
	{ 2,	0,	sys_getpriority,	"osf_getpriority"	}, /* 100 */
	{ 4,	TN,	sys_send,		"send"			}, /* 101 */
	{ 4,	TN,	sys_recv,		"recv"			}, /* 102 */
	{ 1,	TS,	sys_sigreturn,		"sigreturn"		}, /* 103 */
	{ 3,	TN,	sys_bind,		"bind"			}, /* 104 */
	{ 5,	TN,	sys_setsockopt,		"setsockopt"		}, /* 105 */
	{ 2,	TN,	sys_listen,		"listen"		}, /* 106 */
	{ 5,	0,	printargs,		"osf_plock"		}, /* 107, not implemented */
	{ 5,	0,	printargs,		"osf_old_sigvec"	}, /* 108, not implemented */
	{ 5,	0,	printargs,		"osf_old_sigblock"	}, /* 109, not implemented */
	{ 5,	0,	printargs,		"osf_old_sigsetmask"	}, /* 110, not implemented */
	{ 3,	TS,	sys_sigsuspend,		"sigsuspend"		}, /* 111 */
	{ 5,	0,	printargs,		"sigstack"		}, /* 112 */
	{ 3,	TN,	sys_recvmsg,		"recvmsg"		}, /* 113 */
	{ 3,	TN,	sys_sendmsg,		"sendmsg"		}, /* 114 */
	{ 5,	0,	printargs,		"osf_old_vtrace"	}, /* 115, not implemented */
	{ 2,	0,	sys_osf_gettimeofday,	"osf_gettimeofday"	}, /* 116 */
	{ 2,	0,	sys_osf_getrusage,	"osf_getrusage"		}, /* 117 */
	{ 5,	TN,	sys_getsockopt,		"getsockopt"		}, /* 118 */
	{ 5,	0,	printargs,		"SYS_119"		}, /* 119 */
	{ 3,	TD,	sys_readv,		"readv"			}, /* 120 */
	{ 3,	TD,	sys_writev,		"writev"		}, /* 121 */
	{ 2,	0,	sys_osf_settimeofday,	"osf_settimeofday"	}, /* 122 */
	{ 3,	TD,	sys_fchown,		"fchown"		}, /* 123 */
	{ 2,	TD,	sys_fchmod,		"fchmod"		}, /* 124 */
	{ 6,	TN,	sys_recvfrom,		"recvfrom"		}, /* 125 */
	{ 2,	0,	sys_setreuid,		"setreuid"		}, /* 126 */
	{ 2,	0,	sys_setregid,		"setregid"		}, /* 127 */
	{ 2,	TF,	sys_rename,		"rename"		}, /* 128 */
	{ 2,	TF,	sys_truncate,		"truncate"		}, /* 129 */
	{ 2,	TD,	sys_ftruncate,		"ftruncate"		}, /* 130 */
	{ 2,	TD,	sys_flock,		"flock"			}, /* 131 */
	{ 1,	0,	sys_setgid,		"setgid"		}, /* 132 */
	{ 6,	TN,	sys_sendto,		"sendto"		}, /* 133 */
	{ 2,	TN,	sys_shutdown,		"shutdown"		}, /* 134 */
	{ 4,	TN,	sys_socketpair,		"socketpair"		}, /* 135 */
	{ 2,	TF,	sys_mkdir,		"mkdir"			}, /* 136 */
	{ 1,	TF,	sys_rmdir,		"rmdir"			}, /* 137 */
	{ 2,	0,	sys_osf_utimes,		"osf_utimes"		}, /* 138 */
	{ 5,	0,	printargs,		"osf_old_sigreturn"	}, /* 139 */
	{ 5,	0,	printargs,		"osf_adjtime"		}, /* 140, not implemented */
	{ 3,	TN,	sys_getpeername,	"getpeername"		}, /* 141 */
	{ 5,	0,	printargs,		"osf_gethostid"		}, /* 142, not implemented */
	{ 5,	0,	printargs,		"osf_sethostid"		}, /* 143, not implemented */
	{ 2,	0,	sys_getrlimit,		"getrlimit"		}, /* 144 */
	{ 2,	0,	sys_setrlimit,		"setrlimit"		}, /* 145 */
	{ 5,	0,	printargs,		"osf_old_killpg"	}, /* 146, not implemented */
	{ 0,	0,	sys_setsid,		"setsid"		}, /* 147 */
	{ 4,	0,	sys_quotactl,		"quotactl"		}, /* 148 */
	{ 5,	0,	printargs,		"osf_oldquota"		}, /* 149, not implemented */
	{ 3,	TN,	sys_getsockname,	"getsockname"		}, /* 150 */
	{ 5,	0,	printargs,		"SYS_151"		}, /* 151 */
	{ 5,	0,	printargs,		"SYS_152"		}, /* 152 */
	{ 5,	0,	printargs,		"osf_pid_block"		}, /* 153, not implemented */
	{ 5,	0,	printargs,		"osf_pid_unblock"	}, /* 154, not implemented */
	{ 5,	0,	printargs,		"SYS_155"		}, /* 155 */
	{ 3,	TS,	sys_sigaction,		"sigaction"		}, /* 156 */
	{ 5,	0,	printargs,		"osf_sigwaitprim"	}, /* 157, not implemented */
	{ 5,	0,	printargs,		"osf_nfssvc"		}, /* 158, not implemented */
	{ 4,	0,	printargs,		"osf_getdirentries"	}, /* 159 */
	{ 3,	0,	osf_statfs,		"osf_statfs"		}, /* 160 */
	{ 3,	0,	osf_fstatfs,		"osf_fstatfs"		}, /* 161 */
	{ 5,	0,	printargs,		"SYS_162"		}, /* 162 */
	{ 5,	0,	printargs,		"osf_asynch_daemon"	}, /* 163, not implemented */
	{ 5,	0,	printargs,		"osf_getfh"		}, /* 164, not implemented */
	{ 2,	0,	printargs,		"osf_getdomainname"	}, /* 165 */
	{ 2,	0,	sys_setdomainname,	"setdomainname"		}, /* 166 */
	{ 5,	0,	printargs,		"SYS_167"		}, /* 167 */
	{ 5,	0,	printargs,		"SYS_168"		}, /* 168 */
	{ 5,	0,	printargs,		"osf_exportfs"		}, /* 169, not implemented */
	{ 5,	0,	printargs,		"SYS_170"		}, /* 170 */
	{ 5,	0,	printargs,		"SYS_171"		}, /* 171 */
	{ 5,	0,	printargs,		"SYS_172"		}, /* 172 */
	{ 5,	0,	printargs,		"SYS_173"		}, /* 173 */
	{ 5,	0,	printargs,		"SYS_174"		}, /* 174 */
	{ 5,	0,	printargs,		"SYS_175"		}, /* 175 */
	{ 5,	0,	printargs,		"SYS_176"		}, /* 176 */
	{ 5,	0,	printargs,		"SYS_177"		}, /* 177 */
	{ 5,	0,	printargs,		"SYS_178"		}, /* 178 */
	{ 5,	0,	printargs,		"SYS_179"		}, /* 179 */
	{ 5,	0,	printargs,		"SYS_180"		}, /* 180 */
	{ 5,	0,	printargs,		"osf_alt_plock"		}, /* 181, not implemented */
	{ 5,	0,	printargs,		"SYS_182"		}, /* 182 */
	{ 5,	0,	printargs,		"SYS_183"		}, /* 183 */
	{ 5,	0,	printargs,		"osf_getmnt"		}, /* 184, not implemented */
	{ 5,	0,	printargs,		"SYS_185"		}, /* 185 */
	{ 5,	0,	printargs,		"SYS_186"		}, /* 186 */
	{ 5,	0,	printargs,		"osf_alt_sigpending"	}, /* 187, not implemented */
	{ 5,	0,	printargs,		"osf_alt_setsid"	}, /* 188, not implemented */
	{ 5,	0,	printargs,		"SYS_189"		}, /* 189 */
	{ 5,	0,	printargs,		"SYS_190"		}, /* 190 */
	{ 5,	0,	printargs,		"SYS_191"		}, /* 191 */
	{ 5,	0,	printargs,		"SYS_192"		}, /* 192 */
	{ 5,	0,	printargs,		"SYS_193"		}, /* 193 */
	{ 5,	0,	printargs,		"SYS_194"		}, /* 194 */
	{ 5,	0,	printargs,		"SYS_195"		}, /* 195 */
	{ 5,	0,	printargs,		"SYS_196"		}, /* 196 */
	{ 5,	0,	printargs,		"SYS_197"		}, /* 197 */
	{ 5,	0,	printargs,		"SYS_198"		}, /* 198 */
	{ 4,	0,	printargs,		"osf_swapon"		}, /* 199 */
	{ 4,	TI,	sys_msgctl,		"msgctl"		}, /* 200 */
	{ 4,	TI,	sys_msgget,		"msgget"		}, /* 201 */
	{ 4,	TI,	sys_msgrcv,		"msgrcv"		}, /* 202 */
	{ 4,	TI,	sys_msgsnd,		"msgsnd"		}, /* 203 */
	{ 4,	TI,	sys_semctl,		"semctl"		}, /* 204 */
	{ 4,	TI,	sys_semget,		"semget"		}, /* 205 */
	{ 4,	TI,	printargs,		"semop"			}, /* 206 */
	{ 1,	0,	printargs,		"osf_utsname"		}, /* 207 */
	{ 3,	TF,	sys_chown,		"lchown"		}, /* 208 */
	{ 3,	TI,	printargs,		"osf_shmat"		}, /* 209 */
	{ 4,	TI,	sys_shmctl,		"shmctl"		}, /* 210 */
	{ 4,	TI,	sys_shmdt,		"shmdt"			}, /* 211 */
	{ 4,	TI,	sys_shmget,		"shmget"		}, /* 212 */
	{ 5,	0,	printargs,		"osf_mvalid"		}, /* 213, not implemented */
	{ 5,	0,	printargs,		"osf_getaddressconf"	}, /* 214, not implemented */
	{ 5,	0,	printargs,		"osf_msleep"		}, /* 215, not implemented */
	{ 5,	0,	printargs,		"osf_mwakeup"		}, /* 216, not implemented */
	{ 3,	0,	sys_msync,		"msync"			}, /* 217 */
	{ 5,	0,	printargs,		"osf_signal"		}, /* 218, not implemented */
	{ 5,	0,	printargs,		"osf_utc_gettime"	}, /* 219, not implemented */
	{ 5,	0,	printargs,		"osf_utc_adjtime"	}, /* 220, not implemented */
	{ 5,	0,	printargs,		"SYS_221"		}, /* 221 */
	{ 5,	0,	printargs,		"osf_security"		}, /* 222, not implemented */
	{ 5,	0,	printargs,		"osf_kloadcall"		}, /* 223, not implemented */
	{ 5,	0,	printargs,		"SYS_224"		}, /* 224 */
	{ 5,	0,	printargs,		"SYS_225"		}, /* 225 */
	{ 5,	0,	printargs,		"SYS_226"		}, /* 226 */
	{ 5,	0,	printargs,		"SYS_227"		}, /* 227 */
	{ 5,	0,	printargs,		"SYS_228"		}, /* 228 */
	{ 5,	0,	printargs,		"SYS_229"		}, /* 229 */
	{ 5,	0,	printargs,		"SYS_230"		}, /* 230 */
	{ 5,	0,	printargs,		"SYS_231"		}, /* 231 */
	{ 5,	0,	printargs,		"SYS_232"		}, /* 232 */
	{ 1,	0,	sys_getpgid,		"getpgid"		}, /* 233 */
	{ 1,	0,	sys_getsid,		"getsid"		}, /* 234 */
	{ 5,	0,	sys_sigaltstack,	"sigaltstack"		}, /* 235 */
	{ 5,	0,	printargs,		"osf_waitid"		}, /* 236, not implemented */
	{ 5,	0,	printargs,		"osf_priocntlset"	}, /* 237, not implemented */
	{ 5,	0,	printargs,		"osf_sigsendset"	}, /* 238, not implemented */
	{ 5,	0,	printargs,		"osf_set_speculative"	}, /* 239, not implemented */
	{ 5,	0,	printargs,		"osf_msfs_syscall"	}, /* 240, not implemented */
	{ 5,	0,	printargs,		"osf_sysinfo"		}, /* 241 */
	{ 5,	0,	printargs,		"osf_uadmin"		}, /* 242, not implemented */
	{ 5,	0,	printargs,		"osf_fuser"		}, /* 243, not implemented */
	{ 2,	0,	printargs,		"osf_proplist_syscall"	}, /* 244 */
	{ 5,	0,	printargs,		"osf_ntp_adjtime"	}, /* 245, not implemented */
	{ 5,	0,	printargs,		"osf_ntp_gettime"	}, /* 246, not implemented */
	{ 5,	0,	printargs,		"osf_pathconf"		}, /* 247, not implemented */
	{ 5,	0,	printargs,		"osf_fpathconf"		}, /* 248, not implemented */
	{ 5,	0,	printargs,		"SYS_249"		}, /* 249 */
	{ 5,	0,	printargs,		"osf_uswitch"		}, /* 250, not implemented */
	{ 2,	0,	printargs,		"osf_usleep_thread"	}, /* 251 */
	{ 5,	0,	printargs,		"osf_audcntl"		}, /* 252, not implemented */
	{ 5,	0,	printargs,		"osf_audgen"		}, /* 253, not implemented */
	{ 5,	0,	sys_sysfs,		"sysfs"			}, /* 254 */
	{ 5,	0,	printargs,		"osf_subsysinfo"	}, /* 255, not implemented */
	{ 5,	0,	printargs,		"osf_getsysinfo"	}, /* 256 */
	{ 5,	0,	printargs,		"osf_setsysinfo"	}, /* 257 */
	{ 5,	0,	printargs,		"osf_afs_syscall"	}, /* 258, not implemented */
	{ 5,	0,	printargs,		"osf_swapctl"		}, /* 259, not implemented */
	{ 5,	0,	printargs,		"osf_memcntl"		}, /* 260, not implemented */
	{ 5,	0,	printargs,		"osf_fdatasync"		}, /* 261, not implemented */
	{ 5,	0,	printargs,		"SYS_262"		}, /* 262 */
	{ 5,	0,	printargs,		"SYS_263"		}, /* 263 */
	{ 5,	0,	printargs,		"SYS_264"		}, /* 264 */
	{ 5,	0,	printargs,		"SYS_265"		}, /* 265 */
	{ 5,	0,	printargs,		"SYS_266"		}, /* 266 */
	{ 5,	0,	printargs,		"SYS_267"		}, /* 267 */
	{ 5,	0,	printargs,		"SYS_268"		}, /* 268 */
	{ 5,	0,	printargs,		"SYS_269"		}, /* 269 */
	{ 5,	0,	printargs,		"SYS_270"		}, /* 270 */
	{ 5,	0,	printargs,		"SYS_271"		}, /* 271 */
	{ 5,	0,	printargs,		"SYS_272"		}, /* 272 */
	{ 5,	0,	printargs,		"SYS_273"		}, /* 273 */
	{ 5,	0,	printargs,		"SYS_274"		}, /* 274 */
	{ 5,	0,	printargs,		"SYS_275"		}, /* 275 */
	{ 5,	0,	printargs,		"SYS_276"		}, /* 276 */
	{ 5,	0,	printargs,		"SYS_277"		}, /* 277 */
	{ 5,	0,	printargs,		"SYS_278"		}, /* 278 */
	{ 5,	0,	printargs,		"SYS_279"		}, /* 279 */
	{ 5,	0,	printargs,		"SYS_280"		}, /* 280 */
	{ 5,	0,	printargs,		"SYS_281"		}, /* 281 */
	{ 5,	0,	printargs,		"SYS_282"		}, /* 282 */
	{ 5,	0,	printargs,		"SYS_283"		}, /* 283 */
	{ 5,	0,	printargs,		"SYS_284"		}, /* 284 */
	{ 5,	0,	printargs,		"SYS_285"		}, /* 285 */
	{ 5,	0,	printargs,		"SYS_286"		}, /* 286 */
	{ 5,	0,	printargs,		"SYS_287"		}, /* 287 */
	{ 5,	0,	printargs,		"SYS_288"		}, /* 288 */
	{ 5,	0,	printargs,		"SYS_289"		}, /* 289 */
	{ 5,	0,	printargs,		"SYS_290"		}, /* 290 */
	{ 5,	0,	printargs,		"SYS_291"		}, /* 291 */
	{ 5,	0,	printargs,		"SYS_292"		}, /* 292 */
	{ 5,	0,	printargs,		"SYS_293"		}, /* 293 */
	{ 5,	0,	printargs,		"SYS_294"		}, /* 294 */
	{ 5,	0,	printargs,		"SYS_295"		}, /* 295 */
	{ 5,	0,	printargs,		"SYS_296"		}, /* 296 */
	{ 5,	0,	printargs,		"SYS_297"		}, /* 297 */
	{ 5,	0,	printargs,		"SYS_298"		}, /* 298 */
	{ 5,	0,	printargs,		"SYS_299"		}, /* 299 */
	{ 0,	0,	sys_bdflush,		"bdflush"		}, /* 300 */
	{ 3,	0,	printargs,		"sethae"		}, /* 301 */
	{ 5,	TF,	sys_mount,		"mount"			}, /* 302 */
	{ 1,	0,	sys_adjtimex,		"adjtimex32"		}, /* 303 */
	{ 1,	TF,	sys_swapoff,		"swapoff"		}, /* 304 */
	{ 3,	TD,	sys_getdents,		"getdents"		}, /* 305 */
	{ 2,	0,	sys_create_module,	"create_module"		}, /* 306 */
	{ 4,	0,	sys_init_module,	"init_module"		}, /* 307 */
	{ 2,	0,	sys_delete_module,	"delete_module"		}, /* 308 */
	{ 1,	0,	sys_get_kernel_syms,	"get_kernel_syms"	}, /* 309 */
	{ 3,	0,	sys_syslog,		"syslog"		}, /* 310 */
	{ 3,	0,	sys_reboot,		"reboot"		}, /* 311 */
	{ 5,	TP,	sys_clone,		"clone"			}, /* 312 */
	{ 1,	0,	sys_uselib,		"uselib"		}, /* 313 */
	{ 2,	0,	sys_mlock,		"mlock"			}, /* 314 */
	{ 2,	0,	sys_munlock,		"munlock"		}, /* 315 */
	{ 1,	0,	sys_mlockall,		"mlockall"		}, /* 316 */
	{ 0,	0,	sys_munlockall,		"munlockall"		}, /* 317 */
	{ 1,	0,	sys_sysinfo,		"sysinfo"		}, /* 318 */
	{ 1,	0,	sys_sysctl,		"sysctl"		}, /* 319 */
	{ 0,	0,	sys_idle,		"idle"			}, /* 320 */
	{ 1,	0,	sys_umount,		"oldumount"		}, /* 321 */
	{ 1,	0,	sys_swapon,		"swapon"		}, /* 322 */
	{ 1,	0,	sys_times,		"times"			}, /* 323 */
	{ 1,	0,	sys_personality,	"personality"		}, /* 324 */
	{ 1,	NF,	sys_setfsuid,		"setfsuid"		}, /* 325 */
	{ 1,	NF,	sys_setfsgid,		"setfsgid"		}, /* 326 */
	{ 2,	0,	sys_ustat,		"ustat"			}, /* 327 */
	{ 2,	TF,	sys_statfs,		"statfs"		}, /* 328 */
	{ 2,	TD,	sys_fstatfs,		"fstatfs"		}, /* 329 */
	{ 2,	0,	sys_sched_setparam,	"sched_setparam"	}, /* 330 */
	{ 2,	0,	sys_sched_getparam,	"sched_getparam"	}, /* 331 */
	{ 3,	0,	sys_sched_setscheduler,	"sched_setscheduler"	}, /* 332 */
	{ 2,	0,	sys_sched_getscheduler,	"sched_getscheduler"	}, /* 333 */
	{ 0,	0,	sys_sched_yield,	"sched_yield"		}, /* 334 */
	{ 1,	0,	sys_sched_get_priority_max,"sched_get_priority_max"	}, /* 335 */
	{ 1,	0,	sys_sched_get_priority_min,"sched_get_priority_min"	}, /* 336 */
	{ 2,	0,	sys_sched_rr_get_interval,"sched_rr_get_interval"	}, /* 337 */
	{ 5,	0,	sys_afs_syscall,	"afs_syscall"		}, /* 338 */
	{ 1,	0,	sys_uname,		"uname"			}, /* 339 */
	{ 2,	0,	sys_nanosleep,		"nanosleep"		}, /* 340 */
	{ 5,	0,	sys_mremap,		"mremap"		}, /* 341 */
	{ 5,	0,	printargs,		"nfsservctl"		}, /* 342 */
	{ 3,	0,	sys_setresuid,		"setresuid"		}, /* 343 */
	{ 3,	0,	sys_getresuid,		"getresuid"		}, /* 344 */
	{ 5,	0,	printargs,		"pciconfig_read"	}, /* 345 */
	{ 5,	0,	printargs,		"pciconfig_write"	}, /* 346 */
	{ 5,	0,	sys_query_module,	"query_module"		}, /* 347 */
	{ 5,	0,	sys_prctl,		"prctl"			}, /* 348 */
	{ 5,	TD,	sys_pread,		"pread"			}, /* 349 */
	{ 5,	TD,	sys_pwrite,		"pwrite"		}, /* 350 */
	{ 1,	TS,	printargs,		"rt_sigreturn"		}, /* 351 */
	{ 4,	TS,	sys_rt_sigaction,	"rt_sigaction"		}, /* 352 */
	{ 4,	TS,	sys_rt_sigprocmask,	"rt_sigprocmask"	}, /* 353 */
	{ 2,	TS,	sys_rt_sigpending,	"rt_sigpending"		}, /* 354 */
	{ 4,	TS,	sys_rt_sigtimedwait,	"rt_sigtimedwait"	}, /* 355 */
	{ 3,	TS,	sys_rt_sigqueueinfo,	"rt_sigqueueinfo"	}, /* 356 */
	{ 2,	TS,	sys_rt_sigsuspend,	"rt_sigsuspend"		}, /* 357 */
	{ 5,	TD,	sys_select,		"select"		}, /* 358 */
	{ 2,	0,	sys_gettimeofday,	"gettimeofday"		}, /* 359 */
	{ 3,	0,	sys_settimeofday,	"settimeofday"		}, /* 360 */
	{ 2,	0,	sys_getitimer,		"getitimer"		}, /* 361 */
	{ 3,	0,	sys_setitimer,		"setitimer"		}, /* 362 */
	{ 2,	0,	sys_utimes,		"utimes"		}, /* 363 */
	{ 2,	0,	sys_getrusage,		"getrusage"		}, /* 364 */
	{ 4,	TP,	sys_wait4,		"wait4"			}, /* 365 */
	{ 1,	0,	sys_adjtimex,		"adjtimex"		}, /* 366 */
	{ 2,	0,	sys_getcwd,		"getcwd"		}, /* 367 */
	{ 2,	0,	sys_capget,		"capget"		}, /* 368 */
	{ 2,	0,	sys_capset,		"capset"		}, /* 369 */
	{ 4,	TD|TN,	sys_sendfile,		"sendfile"		}, /* 370 */
	{ 3,	0,	sys_setresgid,		"setresgid"		}, /* 371 */
	{ 3,	0,	sys_getresgid,		"getresgid"		}, /* 372 */
	{ 4,	0,	printargs,		"dipc"			}, /* 373, not implemented */
	{ 2,	TF,	sys_pivotroot,		"pivot_root"		}, /* 374 */
	{ 3,	0,	sys_mincore,		"mincore"		}, /* 375 */
	{ 3,	0,	printargs,		"pciconfig_iobase"	}, /* 376 */
	{ 3,	TD,	sys_getdents64,		"getdents64"		}, /* 377 */
	{ 0,	0,	printargs,		"gettid"		}, /* 378 */
	{ 4,	TD,	sys_readahead,		"readahead"		}, /* 379 */
	{ 5,	0,	printargs,		"SYS_380"		}, /* 380 */
	{ 2,	TS,	sys_kill,		"tkill"			}, /* 381 */
	{ 5,	TF,	sys_setxattr,		"setxattr"		}, /* 382 */
	{ 5,	TF,	sys_setxattr,		"lsetxattr"		}, /* 383 */
	{ 5,	TD,	sys_fsetxattr,		"fsetxattr"		}, /* 384 */
	{ 4,	TF,	sys_getxattr,		"getxattr"		}, /* 385 */
	{ 4,	TF,	sys_getxattr,		"lgetxattr"		}, /* 386 */
	{ 4,	TD,	sys_fgetxattr,		"fgetxattr"		}, /* 387 */
	{ 3,	TF,	sys_listxattr,		"listxattr"		}, /* 388 */
	{ 3,	TF,	sys_listxattr,		"llistxattr"		}, /* 389 */
	{ 3,	TD,	sys_flistxattr,		"flistxattr"		}, /* 390 */
	{ 2,	TF,	sys_removexattr,	"removexattr"		}, /* 391 */
	{ 2,	TF,	sys_removexattr,	"lremovexattr"		}, /* 392 */
	{ 2,	TD,	sys_fremovexattr,	"fremovexattr"		}, /* 393 */
	{ 6,	0,	sys_futex,		"futex"			}, /* 394 */
	{ 3,	0,	sys_sched_setaffinity,	"sched_setaffinity"	}, /* 395 */
	{ 3,	0,	sys_sched_getaffinity,	"sched_getaffinity"	}, /* 396 */
	{ 5,	0,	printargs,		"tuxcall"		}, /* 397 */
	{ 2,	0,	sys_io_setup,		"io_setup"		}, /* 398 */
	{ 1,	0,	sys_io_destroy,		"io_destroy"		}, /* 399 */
	{ 5,	0,	sys_io_getevents,		"io_getevents"		}, /* 400 */
	{ 3,	0,	sys_io_submit,		"io_submit"		}, /* 401 */
	{ 3,	0,	sys_io_cancel,		"io_cancel"		}, /* 402 */
	{ 5,	0,	printargs,		"SYS_403"		}, /* 403 */
	{ 5,	0,	printargs,		"SYS_404"		}, /* 404 */
	{ 1,	TP,	sys_exit,		"exit_group"		}, /* 405 */
	{ 4,	0,	printargs,		"lookup_dcookie"	}, /* 406 */
	{ 1,	TD,	sys_epoll_create,	"epoll_create"		}, /* 407 */
	{ 4,	TD,	sys_epoll_ctl,		"epoll_ctl"		}, /* 408 */
	{ 4,	TD,	sys_epoll_wait,		"epoll_wait"		}, /* 409 */
	{ 5,	0,	sys_remap_file_pages,	"remap_file_pages"	}, /* 410 */
	{ 1,	0,	printargs,		"set_tid_address"	}, /* 411 */
	{ 0,	0,	sys_restart_syscall,	"restart_syscall"	}, /* 412 */
	{ 4,	TD,	printargs,		"fadvise"		}, /* 413 */
	{ 3,	0,	sys_timer_create,	"timer_create"		}, /* 414 */
	{ 4,	0,	sys_timer_settime,	"timer_settime"		}, /* 415 */
	{ 2,	0,	sys_timer_gettime,	"timer_gettime"		}, /* 416 */
	{ 1,	0,	sys_timer_getoverrun,	"timer_getoverrun"	}, /* 417 */
	{ 1,	0,	sys_timer_delete,	"timer_delete"		}, /* 418 */
	{ 2,	0,	sys_clock_settime,	"clock_settime"		}, /* 419 */
	{ 2,	0,	sys_clock_gettime,	"clock_gettime"		}, /* 420 */
	{ 2,	0,	sys_clock_getres,	"clock_getres"		}, /* 421 */
	{ 4,	0,	sys_clock_nanosleep,	"clock_nanosleep"	}, /* 422 */
	{ 5,	TI,	sys_semtimedop,		"semtimedop"		}, /* 423 */
	{ 3,	TS,	sys_tgkill,		"tgkill"		}, /* 424 */
	{ 2,	TF,	sys_stat64,		"stat64"		}, /* 425 */
	{ 2,	TF,	sys_lstat64,		"lstat64"		}, /* 426 */
	{ 2,	TD,	sys_fstat64,		"fstat64"		}, /* 427 */
	{ 5,	0,	printargs,		"vserver"		}, /* 428 ??? */
	{ 5,	0,	printargs,		"mbind"			}, /* 429 ??? */
	{ 5,	0,	printargs,		"get_mempolicy"		}, /* 430 ??? */
	{ 5,	0,	printargs,		"set_mempolicy"		}, /* 431 ??? */
	{ 4,	0,	sys_mq_open,		"mq_open"		}, /* 432 */
	{ 1,	0,	sys_mq_unlink,		"mq_unlink"		}, /* 433 */
	{ 5,	0,	sys_mq_timedsend,	"mq_timedsend"		}, /* 434 */
	{ 5,	0,	sys_mq_timedreceive,	"mq_timedreceive"	}, /* 435 */
	{ 2,	0,	sys_mq_notify,		"mq_notify"		}, /* 436 */
	{ 3,	0,	sys_mq_getsetattr,	"mq_getsetattr"		}, /* 437 */
	{ 4,	0,	printargs,		"waitid"		}, /* 438 */
	{ 5,	0,	printargs,		"add_key"		}, /* 439 */
	{ 4,	0,	printargs,		"request_key"		}, /* 440 */
	{ 5,	0,	printargs,		"keyctl"		}, /* 441 */
	{ 3,	0,	printargs,		"ioprio_set"		}, /* 442 */
	{ 2,	0,	printargs,		"ioprio_get"		}, /* 443 */
	{ 0,	TD,	printargs,		"inotify_init"		}, /* 444 */
	{ 3,	TD,	sys_inotify_add_watch,	"inotify_add_watch"	}, /* 445 */
	{ 2,	TD,	sys_inotify_rm_watch,	"inotify_rm_watch"	}, /* 446 */
	{ 1,	TD,	sys_fdatasync,		"fdatasync"		}, /* 447 */
	{ 5,	0,	printargs,		"kexec_load"		}, /* 448 */
	{ 4,	0,	printargs,		"migrate_pages"		}, /* 449 */
	{ 4,	TD|TF,	sys_openat,		"openat"		}, /* 450 */
	{ 3,	TD|TF,	sys_mkdirat,		"mkdirat"		}, /* 451 */
	{ 4,	TD|TF,	sys_mknodat,		"mknodat"		}, /* 452 */
	{ 5,	TD|TF,	sys_fchownat,		"fchownat"		}, /* 453 */
	{ 3,	TD|TF,	sys_futimesat,		"futimesat"		}, /* 454 */
	{ 4,	TD|TF,	sys_newfstatat,		"newfstatat"		}, /* 455 */
	{ 3,	TD|TF,	sys_unlinkat,		"unlinkat"		}, /* 456 */
	{ 4,	TD|TF,	sys_renameat,		"renameat"		}, /* 457 */
	{ 5,	TD|TF,	sys_linkat,		"linkat"		}, /* 458 */
	{ 3,	TD|TF,	sys_symlinkat,		"symlinkat"		}, /* 459 */
	{ 4,	TD|TF,	sys_readlinkat,		"readlinkat"		}, /* 460 */
	{ 3,	TD|TF,	sys_fchmodat,		"fchmodat"		}, /* 461 */
	{ 3,	TD|TF,	sys_faccessat,		"faccessat"		}, /* 462 */
	{ 6,	TD,	sys_pselect6,		"pselect6"		}, /* 463 */
	{ 5,	TD,	sys_ppoll,		"ppoll"			}, /* 464 */
	{ 1,	TP,	sys_unshare,		"unshare"		}, /* 465 */
	{ 2,	0,	printargs,		"set_robust_list"	}, /* 466 */
	{ 3,	0,	printargs,		"get_robust_list"	}, /* 467 */
	{ 6,	TD,	printargs,		"splice"		}, /* 468 */
	{ 4,	TD,	printargs,		"sync_file_range"	}, /* 469 */
	{ 4,	TD,	printargs,		"tee"			}, /* 470 */
	{ 4,	TD,	printargs,		"vmsplice"		}, /* 471 */
	{ 6,	0,	sys_move_pages,		"move_pages"		}, /* 472 */
	{ 3,	0,	sys_getcpu,		"getcpu"		}, /* 473 */
	{ 5,	TD,	sys_epoll_pwait,	"epoll_pwait"		}, /* 474 */
	{ 4,	TD|TF,	sys_utimensat,		"utimensat"		}, /* 475 */
	{ 3,	TD|TS,	sys_signalfd,		"signalfd"		}, /* 476 */
	{ 4,	TD,	sys_timerfd,		"timerfd"		}, /* 477 */
	{ 1,	TD,	sys_eventfd,		"eventfd"		}, /* 478 */
	{ 5,	TN,	sys_recvmmsg,		"recvmmsg"		}, /* 479 */
	{ 6,	TD,	sys_fallocate,		"fallocate"		}, /* 480 */
	{ 2,	TD,	sys_timerfd_create,	"timerfd_create"	}, /* 481 */
	{ 4,	TD,	sys_timerfd_settime,	"timerfd_settime"	}, /* 482 */
	{ 2,	TD,	sys_timerfd_gettime,	"timerfd_gettime"	}, /* 483 */
	{ 4,	TD|TS,	sys_signalfd4,		"signalfd4"		}, /* 484 */
	{ 2,	TD,	sys_eventfd2,		"eventfd2"		}, /* 485 */
	{ 1,	TD,	sys_epoll_create1,	"epoll_create1"		}, /* 486 */
	{ 3,	TD,	sys_dup3,		"dup3"			}, /* 487 */
	{ 2,	TD,	sys_pipe2,		"pipe2"			}, /* 488 */
	{ 1,	TD,	sys_inotify_init1,	"inotify_init1"		}, /* 489 */
	{ 5,	TD,	printargs,		"preadv"		}, /* 490 */
	{ 5,	TD,	printargs,		"pwritev"		}, /* 491 */
	{ 4,	TP|TS,	printargs,		"rt_tgsigqueueinfo"	}, /* 492 */
	{ 5,	TD,	printargs,		"perf_event_open"	}, /* 493 */
	{ 2,	TD,	printargs,		"fanotify_init"		}, /* 494 */
	{ 5,	TD|TF,	printargs,		"fanotify_mark"		}, /* 495 */
	{ 4,	0,	printargs,		"prlimit64"		}, /* 496 */

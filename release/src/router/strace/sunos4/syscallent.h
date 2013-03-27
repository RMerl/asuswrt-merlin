/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
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

	{ 1,	0,	sys_indir,		"indir"		}, /* 0 */
	{ 1,	TP,	sys_exit,		"_exit"		}, /* 1 */
	{ 0,	TP,	sys_fork,		"fork"		}, /* 2 */
	{ 3,	TD,	sys_read,		"read"		}, /* 3 */
	{ 3,	TD,	sys_write,		"write"		}, /* 4 */
	{ 3,	TD|TF,	sys_open,		"open"		}, /* 5 */
	{ 1,	TD,	sys_close,		"close"		}, /* 6 */
	{ 4,	TP,	sys_wait4,		"wait4"		}, /* 7 */
	{ 2,	TD|TF,	sys_creat,		"creat"		}, /* 8 */
	{ 2,	TF,	sys_link,		"link"		}, /* 9 */
	{ 1,	TF,	sys_unlink,		"unlink"	}, /* 10 */
	{ 2,	TF|TP,	sys_execv,		"execv"		}, /* 11 */
	{ 1,	TF,	sys_chdir,		"chdir"		}, /* 12 */
	{ 0,	0,	sys_otime,		"otime"		}, /* 13 */
	{ 3,	TF,	sys_mknod,		"mknod"		}, /* 14 */
	{ 2,	TF,	sys_chmod,		"chmod"		}, /* 15 */
	{ 3,	TF,	sys_chown,		"chown"		}, /* 16 */
	{ 1,	0,	sys_brk,		"brk"		}, /* 17 */
	{ 2,	TF,	sys_stat,		"stat"		}, /* 18 */
	{ 3,	TD,	sys_lseek,		"lseek"		}, /* 19 */
	{ 0,	0,	sys_getpid,		"getpid"	}, /* 20 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 21 */
	{ 1,	TF,	sys_umount,		"umount"	}, /* 22 */
	{ 1,	0,	sys_osetuid,		"osetuid"	}, /* 23 */
	{ 0,	0,	sys_getuid,		"getuid"	}, /* 24 */
	{ 1,	0,	sys_ostime,		"ostime"	}, /* 25 */
	{ 5,	0,	sys_ptrace,		"ptrace"	}, /* 26 */
	{ 1,	0,	sys_oalarm,		"oalarm"	}, /* 27 */
	{ 2,	0,	sys_ofstat,		"ofstat"	}, /* 28 */
	{ 0,	0,	sys_opause,		"opause"	}, /* 29 */
	{ 2,	TF,	sys_outime,		"outime"	}, /* 30 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 31 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 32 */
	{ 2,	TF,	sys_access,		"access"	}, /* 33 */
	{ 1,	0,	sys_onice,		"onice"		}, /* 34 */
	{ 1,	0,	sys_oftime,		"oftime"	}, /* 35 */
	{ 0,	0,	sys_sync,		"sync"		}, /* 36 */
	{ 2,	TS,	sys_kill,		"kill"		}, /* 37 */
	{ 2,	TF,	sys_stat,		"stat"		}, /* 38 */
	{ 2,	0,	sys_osetpgrp,		"osetpgrp"	}, /* 39 */
	{ 2,	TF,	sys_lstat,		"lstat"		}, /* 40 */
	{ 2,	TD,	sys_dup,		"dup"		}, /* 41 */
	{ 0,	TD,	sys_pipe,		"pipe"		}, /* 42 */
	{ 1,	0,	sys_otimes,		"otimes"	}, /* 43 */
	{ 4,	0,	sys_profil,		"profil"	}, /* 44 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 45 */
	{ 1,	0,	sys_osetgid,		"osetgid"	}, /* 46 */
	{ 0,	0,	sys_getgid,		"getgid"	}, /* 47 */
	{ 2,	0,	sys_ossig,		"ossig"		}, /* 48 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 49 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 50 */
	{ 1,	0,	sys_sysacct,		"sysacct"	}, /* 51 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 52 */
	{ 4,	0,	sys_mctl,		"mctl"		}, /* 53 */
	{ 3,	TD,	sys_ioctl,		"ioctl"		}, /* 54 */
	{ 2,	0,	sys_reboot,		"reboot"	}, /* 55 */
	{ 3,	TP,	sys_owait3,		"owait3"	}, /* 56 */
	{ 2,	TF,	sys_symlink,		"symlink"	}, /* 57 */
	{ 3,	TF,	sys_readlink,		"readlink"	}, /* 58 */
	{ 3,	TF|TP,	sys_execve,		"execve"	}, /* 59 */
	{ 1,	0,	sys_umask,		"umask"		}, /* 60 */
	{ 1,	TF,	sys_chroot,		"chroot"	}, /* 61 */
	{ 2,	TD,	sys_fstat,		"fstat"		}, /* 62 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 63 */
	{ 1,	0,	sys_getpagesize,	"getpagesize"	}, /* 64 */
	{ 3,	0,	sys_omsync,		"omsync"	}, /* 65 */
	{ 0,	TP,	sys_vfork,		"vfork"		}, /* 66 */
	{ 0,	TD,	sys_read,		"read"		}, /* 67 */
	{ 0,	TD,	sys_write,		"write"		}, /* 68 */
	{ 1,	0,	sys_sbrk,		"sbrk"		}, /* 69 */
	{ 1,	0,	sys_sstk,		"sstk"		}, /* 70 */
	{ 6,	0,	sys_mmap,		"mmap"		}, /* 71 */
	{ 1,	0,	sys_ovadvise,		"ovadvise"	}, /* 72 */
	{ 2,	0,	sys_munmap,		"munmap"	}, /* 73 */
	{ 3,	0,	sys_mprotect,		"mprotect"	}, /* 74 */
	{ 3,	0,	sys_omadvise,		"omadvise"	}, /* 75 */
	{ 1,	0,	sys_vhangup,		"vhangup"	}, /* 76 */
	{ 2,	0,	sys_ovlimit,		"ovlimit"	}, /* 77 */
	{ 3,	0,	sys_mincore,		"mincore"	}, /* 78 */
	{ 2,	0,	sys_getgroups,		"getgroups"	}, /* 79 */
	{ 2,	0,	sys_setgroups,		"setgroups"	}, /* 80 */
	{ 1,	0,	sys_getpgrp,		"getpgrp"	}, /* 81 */
	{ 2,	0,	sys_setpgrp,		"setpgrp"	}, /* 82 */
	{ 3,	0,	sys_setitimer,		"setitimer"	}, /* 83 */
	{ 0,	TP,	sys_owait,		"owait"		}, /* 84 */
	{ 1,	TF,	sys_swapon,		"swapon"	}, /* 85 */
	{ 2,	0,	sys_getitimer,		"getitimer"	}, /* 86 */
	{ 2,	0,	sys_gethostname,	"gethostname"	}, /* 87 */
	{ 2,	0,	sys_sethostname,	"sethostname"	}, /* 88 */
	{ 0,	0,	sys_getdtablesize,	"getdtablesize"	}, /* 89 */
	{ 2,	TD,	sys_dup2,		"dup2"		}, /* 90 */
	{ 2,	0,	sys_getdopt,		"getdopt"	}, /* 91 */
	{ 3,	TD,	sys_fcntl,		"fcntl"		}, /* 92 */
	{ 5,	TD,	sys_select,		"select"	}, /* 93 */
	{ 2,	0,	sys_setdopt,		"setdopt"	}, /* 94 */
	{ 1,	TD,	sys_fsync,		"fsync"		}, /* 95 */
	{ 3,	0,	sys_setpriority,	"setpriority"	}, /* 96 */
	{ 3,	TN,	sys_socket,		"socket"	}, /* 97 */
	{ 3,	TN,	sys_connect,		"connect"	}, /* 98 */
	{ 3,	TN,	sys_accept,		"accept"	}, /* 99 */
	{ 2,	0,	sys_getpriority,	"getpriority"	}, /* 100 */
	{ 4,	TN,	sys_send,		"send"		}, /* 101 */
	{ 4,	TN,	sys_recv,		"recv"		}, /* 102 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 103 */
	{ 3,	TN,	sys_bind,		"bind"		}, /* 104 */
	{ 5,	TN,	sys_setsockopt,		"setsockopt"	}, /* 105 */
	{ 2,	TN,	sys_listen,		"listen"	}, /* 106 */
	{ 2,	0,	sys_ovtimes,		"ovtimes"	}, /* 107 */
	{ 3,	TS,	sys_sigvec,		"sigvec"	}, /* 108 */
	{ 1,	TS,	sys_sigblock,		"sigblock"	}, /* 109 */
	{ 1,	TS,	sys_sigsetmask,		"sigsetmask"	}, /* 110 */
	{ 1,	TS,	sys_sigpause,		"sigpause"	}, /* 111 */
	{ 2,	TS,	sys_sigstack,		"sigstack"	}, /* 112 */
	{ 3,	TN,	sys_recvmsg,		"recvmsg"	}, /* 113 */
	{ 3,	TN,	sys_sendmsg,		"sendmsg"	}, /* 114 */
	{ 3,	0,	sys_vtrace,		"vtrace"	}, /* 115 */
	{ 2,	0,	sys_gettimeofday,	"gettimeofday"	}, /* 116 */
	{ 2,	0,	sys_getrusage,		"getrusage"	}, /* 117 */
	{ 5,	TN,	sys_getsockopt,		"getsockopt"	}, /* 118 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 119 */
	{ 3,	TD,	sys_readv,		"readv"		}, /* 120 */
	{ 3,	TD,	sys_writev,		"writev"	}, /* 121 */
	{ 2,	0,	sys_settimeofday,	"settimeofday"	}, /* 122 */
	{ 3,	TD,	sys_fchown,		"fchown"	}, /* 123 */
	{ 2,	TD,	sys_fchmod,		"fchmod"	}, /* 124 */
	{ 6,	TN,	sys_recvfrom,		"recvfrom"	}, /* 125 */
	{ 2,	0,	sys_setreuid,		"setreuid"	}, /* 126 */
	{ 2,	0,	sys_setregid,		"setregid"	}, /* 127 */
	{ 2,	TF,	sys_rename,		"rename"	}, /* 128 */
	{ 2,	TF,	sys_truncate,		"truncate"	}, /* 129 */
	{ 2,	TD,	sys_ftruncate,		"ftruncate"	}, /* 130 */
	{ 2,	TD,	sys_flock,		"flock"		}, /* 131 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 132 */
	{ 6,	TN,	sys_sendto,		"sendto"	}, /* 133 */
	{ 2,	TN,	sys_shutdown,		"shutdown"	}, /* 134 */
	{ 5,	TN,	sys_socketpair,		"socketpair"	}, /* 135 */
	{ 2,	TF,	sys_mkdir,		"mkdir"		}, /* 136 */
	{ 1,	TF,	sys_rmdir,		"rmdir"		}, /* 137 */
	{ 2,	TF,	sys_utimes,		"utimes"	}, /* 138 */
	{ 0,	TS,	sys_sigcleanup,		"sigcleanup"	}, /* 139 */
	{ 2,	0,	sys_adjtime,		"adjtime"	}, /* 140 */
	{ 3,	TN,	sys_getpeername,	"getpeername"	}, /* 141 */
	{ 2,	0,	sys_gethostid,		"gethostid"	}, /* 142 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 143 */
	{ 2,	0,	sys_getrlimit,		"getrlimit"	}, /* 144 */
	{ 2,	0,	sys_setrlimit,		"setrlimit"	}, /* 145 */
	{ 2,	TS,	sys_killpg,		"killpg"	}, /* 146 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 147 */
	{ 0,	0,	sys_oldquota,		"oldquota"	}, /* 148 */
	{ 0,	0,	sys_oldquota,		"oldquota"	}, /* 149 */
	{ 3,	TN,	sys_getsockname,	"getsockname"	}, /* 150 */
	{ 4,	TN,	sys_getmsg,		"getmsg"	}, /* 151 */
	{ 4,	TN,	sys_putmsg,		"putmsg"	}, /* 152 */
	{ 3,	TN,	sys_poll,		"poll"		}, /* 153 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 154 */
	{ 1,	0,	sys_nfs_svc,		"nfs_svc"	}, /* 155 */
	{ 4,	0,	sys_getdirentries,	"getdirentries"	}, /* 156 */
	{ 2,	TF,	sys_statfs,		"statfs"	}, /* 157 */
	{ 2,	TD,	sys_fstatfs,		"fstatfs"	}, /* 158 */
	{ 1,	TF,	sys_unmount,		"unmount"	}, /* 159 */
	{ 0,	0,	sys_async_daemon,	"async_daemon"	}, /* 160 */
	{ 2,	0,	sys_nfs_getfh,		"nfs_getfh"	}, /* 161 */
	{ 2,	0,	sys_getdomainname,	"getdomainname"	}, /* 162 */
	{ 2,	0,	sys_setdomainname,	"setdomainname"	}, /* 163 */
	{ 5,	0,	sys_rtschedule,		"rtschedule"	}, /* 164 */
	{ 4,	0,	sys_quotactl,		"quotactl"	}, /* 165 */
	{ 2,	0,	sys_exportfs,		"exportfs"	}, /* 166 */
	{ 4,	TF,	sys_mount,		"mount"		}, /* 167 */
	{ 2,	0,	sys_ustat,		"ustat"		}, /* 168 */
	{ 5,	TI,	sys_semsys,		"semsys"	}, /* 169 */
	{ 6,	TI,	sys_msgsys,		"msgsys"	}, /* 170 */
	{ 4,	TI,	sys_shmsys,		"shmsys"	}, /* 171 */
	{ 4,	0,	sys_auditsys,		"auditsys"	}, /* 172 */
	{ 5,	0,	sys_rfssys,		"rfssys"	}, /* 173 */
	{ 3,	TD,	sys_getdents,		"getdents"	}, /* 174 */
	{ 1,	0,	sys_sys_setsid,		"sys_setsid"	}, /* 175 */
	{ 1,	TD,	sys_fchdir,		"fchdir"	}, /* 176 */
	{ 1,	0,	sys_fchroot,		"fchroot"	}, /* 177 */
	{ 2,	0,	sys_vpixsys,		"vpixsys"	}, /* 178 */
	{ 6,	0,	sys_aioread,		"aioread"	}, /* 179 */
	{ 6,	0,	sys_aiowrite,		"aiowrite"	}, /* 180 */
	{ 1,	0,	sys_aiowait,		"aiowait"	}, /* 181 */
	{ 1,	0,	sys_aiocancel,		"aiocancel"	}, /* 182 */
	{ 1,	TS,	sys_sigpending,		"sigpending"	}, /* 183 */
	{ 0,	0,	sys_errsys,		"errsys"	}, /* 184 */
	{ 2,	0,	sys_setpgid,		"setpgid"	}, /* 185 */
	{ 2,	TF,	sys_pathconf,		"pathconf"	}, /* 186 */
	{ 2,	0,	sys_fpathconf,		"fpathconf"	}, /* 187 */
	{ 1,	0,	sys_sysconf,		"sysconf"	}, /* 188 */
	{ 1,	0,	sys_uname,		"uname"		}, /* 189 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 190 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 191 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 192 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 193 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 194 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 195 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 196 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 197 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 198 */
	{ 0,	0,	sys_nosys,		"nosys"		}, /* 199 */
	{ 4,	TI,	sys_semctl,		"semctl"	}, /* 200 */
	{ 4,	TI,	sys_semget,		"semget"	}, /* 201 */
	{ 4,	TI,	sys_semop,		"semop" 	}, /* 202 */
	{ 5,	TI,	sys_msgget,		"msgget"	}, /* 203 */
	{ 5,	TI,	sys_msgctl,		"msgctl"	}, /* 204 */
	{ 5,	TI,	sys_msgrcv,		"msgrcv"	}, /* 205 */
	{ 5,	TI,	sys_msgsnd,		"msgsnd"	}, /* 206 */
	{ 3,	TI,	sys_shmat,		"shmat" 	}, /* 207 */
	{ 3,	TI,	sys_shmctl,		"shmctl"	}, /* 208 */
	{ 3,	TI,	sys_shmdt,		"shmdt" 	}, /* 209 */
	{ 3,	TI,	sys_shmget,		"shmget"	}, /* 210 */

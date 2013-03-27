/*
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

#ifdef MIPS
	{ -1,	0,	printargs,		"SYS_-1"	}, /* -1 */
#endif /* MIPS */
	{ -1,	0,	sys_syscall,		"syscall"	}, /* 0 */
	{ -1,	TP,	sys_exit,		"_exit"		}, /* 1 */
	{ -1,	TP,	sys_fork,		"fork"		}, /* 2 */
	{ -1,	TD,	sys_read,		"read"		}, /* 3 */
	{ -1,	TD,	sys_write,		"write"		}, /* 4 */
	{ -1,	TD|TF,	sys_open,		"open"		}, /* 5 */
	{ -1,	TD,	sys_close,		"close"		}, /* 6 */
	{ -1,	TP,	sys_wait,		"wait"		}, /* 7 */
	{ -1,	TD|TF,	sys_creat,		"creat"		}, /* 8 */
	{ -1,	TF,	sys_link,		"link"		}, /* 9 */
	{ -1,	TF,	sys_unlink,		"unlink"	}, /* 10 */
	{ -1,	TF|TP,	sys_exec,		"exec"		}, /* 11 */
	{ -1,	TF,	sys_chdir,		"chdir"		}, /* 12 */
	{ -1,	0,	sys_time,		"time"		}, /* 13 */
	{ -1,	TF,	sys_mknod,		"mknod"		}, /* 14 */
	{ -1,	TF,	sys_chmod,		"chmod"		}, /* 15 */
	{ -1,	TF,	sys_chown,		"chown"		}, /* 16 */
	{ -1,	0,	sys_brk,		"brk"		}, /* 17 */
	{ -1,	TF,	sys_stat,		"stat"		}, /* 18 */
	{ -1,	TD,	sys_lseek,		"lseek"		}, /* 19 */
	{ -1,	0,	sys_getpid,		"getpid"	}, /* 20 */
	{ -1,	TF,	sys_mount,		"mount"		}, /* 21 */
	{ -1,	TF,	sys_umount,		"umount"	}, /* 22 */
	{ -1,	0,	sys_setuid,		"setuid"	}, /* 23 */
	{ -1,	0,	sys_getuid,		"getuid"	}, /* 24 */
	{ -1,	0,	sys_stime,		"stime"		}, /* 25 */
	{ -1,	0,	sys_ptrace,		"ptrace"	}, /* 26 */
	{ -1,	0,	sys_alarm,		"alarm"		}, /* 27 */
	{ -1,	TD,	sys_fstat,		"fstat"		}, /* 28 */
	{ -1,	TS,	sys_pause,		"pause"		}, /* 29 */
	{ -1,	TF,	sys_utime,		"utime"		}, /* 30 */
	{ -1,	0,	sys_stty,		"stty"		}, /* 31 */
	{ -1,	0,	sys_gtty,		"gtty"		}, /* 32 */
	{ -1,	TF,	sys_access,		"access"	}, /* 33 */
	{ -1,	0,	sys_nice,		"nice"		}, /* 34 */
	{ -1,	TF,	sys_statfs,		"statfs"	}, /* 35 */
	{ -1,	0,	sys_sync,		"sync"		}, /* 36 */
	{ -1,	TS,	sys_kill,		"kill"		}, /* 37 */
	{ -1,	TD,	sys_fstatfs,		"fstatfs"	}, /* 38 */
#ifdef MIPS
	{ -1,	0,	sys_setpgrp,		"setpgrp"	}, /* 39 */
#else /* !MIPS */
	{ -1,	0,	sys_pgrpsys,		"pgrpsys"	}, /* 39 */
#endif /* !MIPS */
#ifdef MIPS
	{ -1,	0,	sys_syssgi,		"syssgi"	}, /* 40 */
#else /* !MIPS */
	{ -1,	0,	sys_xenix,		"xenix"		}, /* 40 */
#endif /* !MIPS */
	{ -1,	TD,	sys_dup,		"dup"		}, /* 41 */
	{ -1,	TD,	sys_pipe,		"pipe"		}, /* 42 */
	{ -1,	0,	sys_times,		"times"		}, /* 43 */
	{ -1,	0,	sys_profil,		"profil"	}, /* 44 */
	{ -1,	0,	sys_plock,		"plock"		}, /* 45 */
	{ -1,	0,	sys_setgid,		"setgid"	}, /* 46 */
	{ -1,	0,	sys_getgid,		"getgid"	}, /* 47 */
	{ -1,	0,	sys_sigcall,		"sigcall"	}, /* 48 */
	{ -1,	TI,	sys_msgsys,		"msgsys"	}, /* 49 */
#ifdef SPARC
	{ -1,	0,	sys_syssun,		"syssun"	}, /* 50 */
#else /* !SPARC */
#ifdef I386
	{ -1,	0,	sys_sysi86,		"sysi86"	}, /* 50 */
#else /* !I386 */
#ifdef MIPS
	{ -1,	0,	sys_sysmips,		"sysmips"	}, /* 50 */
#else /* !MIPS */
	{ -1,	0,	sys_sysmachine,		"sysmachine"	}, /* 50 */
#endif /* !MIPS */
#endif /* !I386 */
#endif /* !SPARC */
	{ -1,	TF,	sys_acct,		"acct"		}, /* 51 */
	{ -1,	TI,	sys_shmsys,		"shmsys"	}, /* 52 */
	{ -1,	TI,	sys_semsys,		"semsys"	}, /* 53 */
	{ -1,	TD,	sys_ioctl,		"ioctl"		}, /* 54 */
	{ -1,	0,	sys_uadmin,		"uadmin"	}, /* 55 */
	{ -1,	0,	sys_sysmp,		"sysmp"		}, /* 56 */
	{ -1,	0,	sys_utssys,		"utssys"	}, /* 57 */
	{ -1,	0,	sys_fdsync,		"fdsync"	}, /* 58 */
	{ -1,	TF|TP,	sys_execve,		"execve"	}, /* 59 */
	{ -1,	0,	sys_umask,		"umask"		}, /* 60 */
	{ -1,	TF,	sys_chroot,		"chroot"	}, /* 61 */
	{ -1,	TD,	sys_fcntl,		"fcntl"		}, /* 62 */
	{ -1,	0,	sys_ulimit,		"ulimit"	}, /* 63 */
	{ -1,	0,	printargs,		"SYS_64"	}, /* 64 */
	{ -1,	0,	printargs,		"SYS_65"	}, /* 65 */
	{ -1,	0,	printargs,		"SYS_66"	}, /* 66 */
	{ -1,	0,	printargs,		"SYS_67"	}, /* 67 */
	{ -1,	0,	printargs,		"SYS_68"	}, /* 68 */
	{ -1,	0,	printargs,		"SYS_69"	}, /* 69 */
	{ -1,	0,	printargs,		"SYS_70"	}, /* 70 */
	{ -1,	0,	printargs,		"SYS_71"	}, /* 71 */
	{ -1,	0,	printargs,		"SYS_72"	}, /* 72 */
	{ -1,	0,	printargs,		"SYS_73"	}, /* 73 */
	{ -1,	0,	printargs,		"SYS_74"	}, /* 74 */
	{ -1,	0,	printargs,		"SYS_75"	}, /* 75 */
	{ -1,	0,	printargs,		"SYS_76"	}, /* 76 */
	{ -1,	0,	printargs,		"SYS_77"	}, /* 77 */
	{ -1,	0,	printargs,		"SYS_78"	}, /* 78 */
	{ -1,	TF,	sys_rmdir,		"rmdir"		}, /* 79 */
	{ -1,	TF,	sys_mkdir,		"mkdir"		}, /* 80 */
	{ -1,	TD,	sys_getdents,		"getdents"	}, /* 81 */
	{ -1,	0,	sys_sginap,		"sginap"	}, /* 82 */
	{ -1,	0,	sys_sgikopt,		"sgikopt"	}, /* 83 */
	{ -1,	0,	sys_sysfs,		"sysfs"		}, /* 84 */
	{ -1,	TN,	sys_getmsg,		"getmsg"	}, /* 85 */
	{ -1,	TN,	sys_putmsg,		"putmsg"	}, /* 86 */
	{ -1,	TN,	sys_poll,		"poll"		}, /* 87 */
#ifdef MIPS
	{ -1,	TS,	sys_sigreturn,		"sigreturn"	}, /* 88 */
	{ -1,	TN,	sys_accept,		"accept"	}, /* 89 */
	{ -1,	TN,	sys_bind,		"bind"		}, /* 90 */
	{ -1,	TN,	sys_connect,		"connect"	}, /* 91 */
	{ -1,	0,	sys_gethostid,		"gethostid"	}, /* 92 */
	{ -1,	TN,	sys_getpeername,	"getpeername"	}, /* 93 */
	{ -1,	TN,	sys_getsockname,	"getsockname"	}, /* 94 */
	{ -1,	TN,	sys_getsockopt,		"getsockopt"	}, /* 95 */
	{ -1,	TN,	sys_listen,		"listen"	}, /* 96 */
	{ -1,	TN,	sys_recv,		"recv"		}, /* 97 */
	{ -1,	TN,	sys_recvfrom,		"recvfrom"	}, /* 98 */
	{ -1,	TN,	sys_recvmsg,		"recvmsg"	}, /* 99 */
	{ -1,	TD,	sys_select,		"select"	}, /* 100 */
	{ -1,	TN,	sys_send,		"send"		}, /* 101 */
	{ -1,	TN,	sys_sendmsg,		"sendmsg"	}, /* 102 */
	{ -1,	TN,	sys_sendto,		"sendto"	}, /* 103 */
	{ -1,	0,	sys_sethostid,		"sethostid"	}, /* 104 */
	{ -1,	TN,	sys_setsockopt,		"setsockopt"	}, /* 105 */
	{ -1,	TN,	sys_shutdown,		"shutdown"	}, /* 106 */
	{ -1,	TN,	sys_socket,		"socket"	}, /* 107 */
	{ -1,	0,	sys_gethostname,	"gethostname"	}, /* 108 */
	{ -1,	0,	sys_sethostname,	"sethostname"	}, /* 109 */
	{ -1,	0,	sys_getdomainname,	"getdomainname"	}, /* 110 */
	{ -1,	0,	sys_setdomainname,	"setdomainname"	}, /* 111 */
	{ -1,	TF,	sys_truncate,		"truncate"	}, /* 112 */
	{ -1,	TD,	sys_ftruncate,		"ftruncate"	}, /* 113 */
	{ -1,	TF,	sys_rename,		"rename"	}, /* 114 */
	{ -1,	TF,	sys_symlink,		"symlink"	}, /* 115 */
	{ -1,	TF,	sys_readlink,		"readlink"	}, /* 116 */
	{ -1,	0,	printargs,		"SYS_117"	}, /* 117 */
	{ -1,	0,	printargs,		"SYS_118"	}, /* 118 */
	{ -1,	0,	sys_nfssvc,		"nfssvc"	}, /* 119 */
	{ -1,	0,	sys_getfh,		"getfh"		}, /* 120 */
	{ -1,	0,	sys_async_daemon,	"async_daemon"	}, /* 121 */
	{ -1,	0,	sys_exportfs,		"exportfs"	}, /* 122 */
	{ -1,	0,	sys_setregid,		"setregid"	}, /* 123 */
	{ -1,	0,	sys_setreuid,		"setreuid"	}, /* 124 */
	{ -1,	0,	sys_getitimer,		"getitimer"	}, /* 125 */
	{ -1,	0,	sys_setitimer,		"setitimer"	}, /* 126 */
	{ -1,	0,	sys_adjtime,		"adjtime"	}, /* 127 */
	{ -1,	0,	sys_BSD_getime,		"BSD_getime"	}, /* 128 */
	{ -1,	0,	sys_sproc,		"sproc"		}, /* 129 */
	{ -1,	0,	sys_prctl,		"prctl"		}, /* 130 */
	{ -1,	0,	sys_procblk,		"procblk"	}, /* 131 */
	{ -1,	0,	sys_sprocsp,		"sprocsp"	}, /* 132 */
	{ -1,	0,	printargs,		"SYS_133"	}, /* 133 */
	{ -1,	0,	sys_mmap,		"mmap"		}, /* 134 */
	{ -1,	0,	sys_munmap,		"munmap"	}, /* 135 */
	{ -1,	0,	sys_mprotect,		"mprotect"	}, /* 136 */
	{ -1,	0,	sys_msync,		"msync"		}, /* 137 */
	{ -1,	0,	sys_madvise,		"madvise"	}, /* 138 */
	{ -1,	0,	sys_pagelock,		"pagelock"	}, /* 139 */
	{ -1,	0,	sys_getpagesize,	"getpagesize"	}, /* 140 */
	{ -1,	0,	sys_quotactl,		"quotactl"	}, /* 141 */
	{ -1,	0,	printargs,		"SYS_142"	}, /* 142 */
	{ -1,	0,	sys_BSDgetpgrp,		"BSDgetpgrp"	}, /* 143 */
	{ -1,	0,	sys_BSDsetpgrp,		"BSDsetpgrp"	}, /* 144 */
	{ -1,	0,	sys_vhangup,		"vhangup"	}, /* 145 */
	{ -1,	TD,	sys_fsync,		"fsync"		}, /* 146 */
	{ -1,	TD,	sys_fchdir,		"fchdir"	}, /* 147 */
	{ -1,	0,	sys_getrlimit,		"getrlimit"	}, /* 148 */
	{ -1,	0,	sys_setrlimit,		"setrlimit"	}, /* 149 */
	{ -1,	0,	sys_cacheflush,		"cacheflush"	}, /* 150 */
	{ -1,	0,	sys_cachectl,		"cachectl"	}, /* 151 */
	{ -1,	TD,	sys_fchown,		"fchown"	}, /* 152 */
	{ -1,	TD,	sys_fchmod,		"fchmod"	}, /* 153 */
	{ -1,	0,	printargs,		"SYS_154"	}, /* 154 */
	{ -1,	TN,	sys_socketpair,		"socketpair"	}, /* 155 */
	{ -1,	0,	sys_sysinfo,		"sysinfo"	}, /* 156 */
	{ -1,	0,	sys_nuname,		"nuname"	}, /* 157 */
	{ -1,	TF,	sys_xstat,		"xstat"		}, /* 158 */
	{ -1,	TF,	sys_lxstat,		"lxstat"	}, /* 159 */
	{ -1,	0,	sys_fxstat,		"fxstat"	}, /* 160 */
	{ -1,	TF,	sys_xmknod,		"xmknod"	}, /* 161 */
	{ -1,	TS,	sys_ksigaction,		"sigaction"	}, /* 162 */
	{ -1,	TS,	sys_sigpending,		"sigpending"	}, /* 163 */
	{ -1,	TS,	sys_sigprocmask,	"sigprocmask"	}, /* 164 */
	{ -1,	TS,	sys_sigsuspend,		"sigsuspend"	}, /* 165 */
	{ -1,	TS,	sys_sigpoll,		"sigpoll"	}, /* 166 */
	{ -1,	0,	sys_swapctl,		"swapctl"	}, /* 167 */
	{ -1,	0,	sys_getcontext,		"getcontext"	}, /* 168 */
	{ -1,	0,	sys_setcontext,		"setcontext"	}, /* 169 */
	{ -1,	TP,	sys_waitid,		"waitid"	}, /* 170 */
	{ -1,	TS,	sys_sigstack,		"sigstack"	}, /* 171 */
	{ -1,	TS,	sys_sigaltstack,	"sigaltstack"	}, /* 172 */
	{ -1,	TS,	sys_sigsendset,		"sigsendset"	}, /* 173 */
	{ -1,	TF,	sys_statvfs,		"statvfs"	}, /* 174 */
	{ -1,	0,	sys_fstatvfs,		"fstatvfs"	}, /* 175 */
	{ -1,	TN,	sys_getpmsg,		"getpmsg"	}, /* 176 */
	{ -1,	TN,	sys_putpmsg,		"putpmsg"	}, /* 177 */
	{ -1,	TF,	sys_lchown,		"lchown"	}, /* 178 */
	{ -1,	0,	sys_priocntl,		"priocntl"	}, /* 179 */
	{ -1,	TS,	sys_ksigqueue,		"ksigqueue"	}, /* 180 */
	{ -1,	0,	printargs,		"SYS_181"	}, /* 181 */
	{ -1,	0,	printargs,		"SYS_182"	}, /* 182 */
	{ -1,	0,	printargs,		"SYS_183"	}, /* 183 */
	{ -1,	0,	printargs,		"SYS_184"	}, /* 184 */
	{ -1,	0,	printargs,		"SYS_185"	}, /* 185 */
	{ -1,	0,	printargs,		"SYS_186"	}, /* 186 */
	{ -1,	0,	printargs,		"SYS_187"	}, /* 187 */
	{ -1,	0,	printargs,		"SYS_188"	}, /* 188 */
	{ -1,	0,	printargs,		"SYS_189"	}, /* 189 */
	{ -1,	0,	printargs,		"SYS_190"	}, /* 190 */
	{ -1,	0,	printargs,		"SYS_191"	}, /* 191 */
	{ -1,	0,	printargs,		"SYS_192"	}, /* 192 */
	{ -1,	0,	printargs,		"SYS_193"	}, /* 193 */
	{ -1,	0,	printargs,		"SYS_194"	}, /* 194 */
	{ -1,	0,	printargs,		"SYS_195"	}, /* 195 */
	{ -1,	0,	printargs,		"SYS_196"	}, /* 196 */
	{ -1,	0,	printargs,		"SYS_197"	}, /* 197 */
	{ -1,	0,	printargs,		"SYS_198"	}, /* 198 */
	{ -1,	0,	printargs,		"SYS_199"	}, /* 199 */
	{ -1,	0,	printargs,		"SYS_200"	}, /* 200 */
	{ -1,	0,	printargs,		"SYS_201"	}, /* 201 */
	{ -1,	0,	printargs,		"SYS_202"	}, /* 202 */
	{ -1,	0,	printargs,		"SYS_203"	}, /* 203 */
	{ -1,	0,	printargs,		"SYS_204"	}, /* 204 */
	{ -1,	0,	printargs,		"SYS_205"	}, /* 205 */
	{ -1,	0,	printargs,		"SYS_206"	}, /* 206 */
	{ -1,	0,	printargs,		"SYS_207"	}, /* 207 */
	{ -1,	0,	printargs,		"SYS_208"	}, /* 208 */
	{ -1,	0,	printargs,		"SYS_209"	}, /* 209 */
	{ -1,	0,	printargs,		"SYS_210"	}, /* 210 */
	{ -1,	0,	printargs,		"SYS_211"	}, /* 211 */
	{ -1,	0,	printargs,		"SYS_212"	}, /* 212 */
	{ -1,	0,	printargs,		"SYS_213"	}, /* 213 */
	{ -1,	0,	printargs,		"SYS_214"	}, /* 214 */
	{ -1,	0,	printargs,		"SYS_215"	}, /* 215 */
	{ -1,	0,	printargs,		"SYS_216"	}, /* 216 */
	{ -1,	0,	printargs,		"SYS_217"	}, /* 217 */
	{ -1,	0,	printargs,		"SYS_218"	}, /* 218 */
	{ -1,	0,	printargs,		"SYS_219"	}, /* 219 */
	{ -1,	0,	printargs,		"SYS_220"	}, /* 220 */
	{ -1,	0,	printargs,		"SYS_221"	}, /* 221 */
	{ -1,	0,	printargs,		"SYS_222"	}, /* 222 */
	{ -1,	0,	printargs,		"SYS_223"	}, /* 223 */
	{ -1,	0,	printargs,		"SYS_224"	}, /* 224 */
	{ -1,	0,	printargs,		"SYS_225"	}, /* 225 */
	{ -1,	0,	printargs,		"SYS_226"	}, /* 226 */
	{ -1,	0,	printargs,		"SYS_227"	}, /* 227 */
	{ -1,	0,	printargs,		"SYS_228"	}, /* 228 */
	{ -1,	0,	printargs,		"SYS_229"	}, /* 229 */
	{ -1,	0,	printargs,		"SYS_230"	}, /* 230 */
	{ -1,	0,	printargs,		"SYS_231"	}, /* 231 */
	{ -1,	0,	printargs,		"SYS_232"	}, /* 232 */
	{ -1,	0,	printargs,		"SYS_233"	}, /* 233 */
	{ -1,	0,	printargs,		"SYS_234"	}, /* 234 */
	{ -1,	0,	printargs,		"SYS_235"	}, /* 235 */
	{ -1,	0,	printargs,		"SYS_236"	}, /* 236 */
	{ -1,	0,	printargs,		"SYS_237"	}, /* 237 */
	{ -1,	0,	printargs,		"SYS_238"	}, /* 238 */
	{ -1,	0,	printargs,		"SYS_239"	}, /* 239 */
	{ -1,	0,	printargs,		"SYS_240"	}, /* 240 */
	{ -1,	0,	printargs,		"SYS_241"	}, /* 241 */
	{ -1,	0,	printargs,		"SYS_242"	}, /* 242 */
	{ -1,	0,	printargs,		"SYS_243"	}, /* 243 */
	{ -1,	0,	printargs,		"SYS_244"	}, /* 244 */
	{ -1,	0,	printargs,		"SYS_245"	}, /* 245 */
	{ -1,	0,	printargs,		"SYS_246"	}, /* 246 */
	{ -1,	0,	printargs,		"SYS_247"	}, /* 247 */
	{ -1,	0,	printargs,		"SYS_248"	}, /* 248 */
	{ -1,	0,	printargs,		"SYS_249"	}, /* 249 */
	{ -1,	0,	printargs,		"SYS_250"	}, /* 250 */
	{ -1,	0,	printargs,		"SYS_251"	}, /* 251 */
	{ -1,	0,	printargs,		"SYS_252"	}, /* 252 */
	{ -1,	0,	printargs,		"SYS_253"	}, /* 253 */
	{ -1,	0,	printargs,		"SYS_254"	}, /* 254 */
	{ -1,	0,	printargs,		"SYS_255"	}, /* 255 */
#else /* !MIPS */
	{ -1,	TF,	sys_lstat,		"lstat"		}, /* 88 */
	{ -1,	TF,	sys_symlink,		"symlink"	}, /* 89 */
	{ -1,	TF,	sys_readlink,		"readlink"	}, /* 90 */
	{ -1,	0,	sys_setgroups,		"setgroups"	}, /* 91 */
	{ -1,	0,	sys_getgroups,		"getgroups"	}, /* 92 */
	{ -1,	TD,	sys_fchmod,		"fchmod"	}, /* 93 */
	{ -1,	TD,	sys_fchown,		"fchown"	}, /* 94 */
	{ -1,	TS,	sys_sigprocmask,	"sigprocmask"	}, /* 95 */
	{ -1,	TS,	sys_sigsuspend,		"sigsuspend"	}, /* 96 */
	{ -1,	TS,	sys_sigaltstack,	"sigaltstack"	}, /* 97 */
	{ -1,	TS,	sys_sigaction,		"sigaction"	}, /* 98 */
	{ -1,	0,	sys_spcall,		"spcall"	}, /* 99 */
	{ -1,	0,	sys_context,		"context"	}, /* 100 */
	{ -1,	0,	sys_evsys,		"evsys"		}, /* 101 */
	{ -1,	0,	sys_evtrapret,		"evtrapret"	}, /* 102 */
	{ -1,	TF,	sys_statvfs,		"statvfs"	}, /* 103 */
	{ -1,	0,	sys_fstatvfs,		"fstatvfs"	}, /* 104 */
	{ -1,	0,	printargs,		"SYS_105"	}, /* 105 */
	{ -1,	0,	sys_nfssys,		"nfssys"	}, /* 106 */
#if UNIXWARE
	{ -1,	TP,	sys_waitsys,		"waitsys"	}, /* 107 */
#else
	{ -1,	TP,	sys_waitid,		"waitid"	}, /* 107 */
#endif
	{ -1,	0,	sys_sigsendsys,		"sigsendsys"	}, /* 108 */
	{ -1,	0,	sys_hrtsys,		"hrtsys"	}, /* 109 */
	{ -1,	0,	sys_acancel,		"acancel"	}, /* 110 */
	{ -1,	0,	sys_async,		"async"		}, /* 111 */
	{ -1,	0,	sys_priocntlsys,	"priocntlsys"	}, /* 112 */
	{ -1,	TF,	sys_pathconf,		"pathconf"	}, /* 113 */
	{ -1,	0,	sys_mincore,		"mincore"	}, /* 114 */
	{ -1,	0,	sys_mmap,		"mmap"		}, /* 115 */
	{ -1,	0,	sys_mprotect,		"mprotect"	}, /* 116 */
	{ -1,	0,	sys_munmap,		"munmap"	}, /* 117 */
	{ -1,	0,	sys_fpathconf,		"fpathconf"	}, /* 118 */
	{ -1,	TP,	sys_vfork,		"vfork"		}, /* 119 */
	{ -1,	TD,	sys_fchdir,		"fchdir"	}, /* 120 */
	{ -1,	TD,	sys_readv,		"readv"		}, /* 121 */
	{ -1,	TD,	sys_writev,		"writev"	}, /* 122 */
	{ -1,	TF,	sys_xstat,		"xstat"		}, /* 123 */
	{ -1,	TF,	sys_lxstat,		"lxstat"	}, /* 124 */
	{ -1,	0,	sys_fxstat,		"fxstat"	}, /* 125 */
	{ -1,	TF,	sys_xmknod,		"xmknod"	}, /* 126 */
	{ -1,	0,	sys_clocal,		"clocal"	}, /* 127 */
	{ -1,	0,	sys_setrlimit,		"setrlimit"	}, /* 128 */
	{ -1,	0,	sys_getrlimit,		"getrlimit"	}, /* 129 */
	{ -1,	TF,	sys_lchown,		"lchown"	}, /* 130 */
	{ -1,	0,	sys_memcntl,		"memcntl"	}, /* 131 */
	{ -1,	TN,	sys_getpmsg,		"getpmsg"	}, /* 132 */
	{ -1,	TN,	sys_putpmsg,		"putpmsg"	}, /* 133 */
	{ -1,	TF,	sys_rename,		"rename"	}, /* 134 */
	{ -1,	0,	sys_uname,		"uname"		}, /* 135 */
	{ -1,	0,	sys_setegid,		"setegid"	}, /* 136 */
	{ -1,	0,	sys_sysconfig,		"sysconfig"	}, /* 137 */
	{ -1,	0,	sys_adjtime,		"adjtime"	}, /* 138 */
	{ -1,	0,	sys_sysinfo,		"sysinfo"	}, /* 139 */
	{ -1,	0,	printargs,		"SYS_140"	}, /* 140 */
#if UNIXWARE >= 2
	{ -1,	0,	sys_seteuid,		"seteuid"	}, /* 141 */
	{ -1,	0, 	printargs,		"SYS_142"	}, /* 142 */
	{ -1,	0,	sys_keyctl,		"keyctl"	}, /* 143 */
	{ -1,	0,	sys_secsys,		"secsys"	}, /* 144 */
	{ -1,	0,	sys_filepriv,		"filepriv"	}, /* 145 */
	{ -1,	0,	sys_procpriv,		"procpriv"	}, /* 146 */
	{ -1,	0,	sys_devstat,		"devstat"	}, /* 147 */
	{ -1,	0,	sys_aclipc,		"aclipc"	}, /* 148 */
	{ -1,	0,	sys_fdevstat,		"fdevstat"	}, /* 149 */
	{ -1,	0,	sys_flvlfile,		"flvlfile"	}, /* 150 */
	{ -1,	0,	sys_lvlfile,		"lvlfile"	}, /* 151 */
	{ -1,	0, 	printargs,		"SYS_152"	}, /* 152 */
	{ -1,	0,	sys_lvlequal,		"lvlequal"	}, /* 153 */
	{ -1,	0,	sys_lvlproc,		"lvlproc"	}, /* 154 */
	{ -1,	0, 	printargs,		"SYS_155"	}, /* 155 */
	{ -1,	0,	sys_lvlipc,		"lvlipc"	}, /* 156 */
	{ -1,	0,	sys_acl,		"acl"		}, /* 157 */
	{ -1,	0,	sys_auditevt,		"auditevt"	}, /* 158 */
	{ -1,	0,	sys_auditctl,		"auditctl"	}, /* 159 */
	{ -1,	0,	sys_auditdmp,		"auditdmp"	}, /* 160 */
	{ -1,	0,	sys_auditlog,		"auditlog"	}, /* 161 */
	{ -1,	0,	sys_auditbuf,		"auditbuf"	}, /* 162 */
	{ -1,	0,	sys_lvldom,		"lvldom"	}, /* 163 */
	{ -1,	0,	sys_lvlvfs,		"lvlvfs"	}, /* 164 */
	{ -1,	0,	sys_mkmld,		"mkmld"		}, /* 165 */
	{ -1,	0,	sys_mldmode,		"mldmode"	}, /* 166 */
	{ -1,	0,	sys_secadvise,		"secadvise"	}, /* 167 */
	{ -1,	0,	sys_online,		"online"	}, /* 168 */
	{ -1,	0,	sys_setitimer,		"setitimer"	}, /* 169 */
	{ -1,	0,	sys_getitimer,		"getitimer"	}, /* 170 */
	{ -1,	0,	sys_gettimeofday,	"gettimeofday"	}, /* 171 */
	{ -1,	0,	sys_settimeofday,	"settimeofday"	}, /* 172 */
	{ -1,	0,	sys_lwp_create,		"lwpcreate"	}, /* 173 */
	{ -1,	0,	sys_lwp_exit,		"lwpexit"	}, /* 174 */
	{ -1,	0,	sys_lwp_wait,		"lwpwait"	}, /* 175 */
	{ -1,	0,	sys_lwp_self,		"lwpself"	}, /* 176 */
	{ -1,	0,	sys_lwpinfo,		"lwpinfo"	}, /* 177 */
	{ -1,	0,	sys_lwpprivate,		"lwpprivate"	}, /* 178 */
	{ -1,	0,	sys_processor_bind,	"processor_bind"}, /* 179 */
	{ -1,	0,	sys_processor_exbind,	"processor_exbind"}, /* 180 */
	{ -1,	0, 	printargs,		"SYS_181"	}, /* 181 */
	{ -1,	0, 	printargs,		"SYS_182"	}, /* 182 */
	{ -1,	0,	sys_prepblock,		"prepblock"	}, /* 183 */
	{ -1,	0,	sys_block,		"block"		}, /* 184 */
	{ -1,	0,	sys_rdblock,		"rdblock"	}, /* 185 */
	{ -1,	0,	sys_unblock,		"unblock"	}, /* 186 */
	{ -1,	0,	sys_cancelblock,	"cancelblock"	}, /* 187 */
	{ -1,	0, 	printargs,		"SYS_188"	}, /* 188 */
	{ -1,	TD,	sys_pread,		"pread"		}, /* 189 */
	{ -1,	TD,	sys_pwrite,		"pwrite"	}, /* 190 */
	{ -1,	TF,	sys_truncate,		"truncate"	}, /* 191 */
	{ -1,	TD,	sys_ftruncate,		"ftruncate"	}, /* 192 */
	{ -1,	0,	sys_lwpkill,		"lwpkill"	}, /* 193 */
	{ -1,	0,	sys_sigwait,		"sigwait"	}, /* 194 */
	{ -1,	0,	sys_fork1,		"fork1"		}, /* 195 */
	{ -1,	0,	sys_forkall,		"forkall"	}, /* 196 */
	{ -1,	0,	sys_modload,		"modload"	}, /* 197 */
	{ -1,	0,	sys_moduload,		"moduload"	}, /* 198 */
	{ -1,	0,	sys_modpath,		"modpath"	}, /* 199 */
	{ -1,	0,	sys_modstat,		"modstat"	}, /* 200 */
	{ -1,	0,	sys_modadm,		"modadm"	}, /* 201 */
	{ -1,	0,	sys_getksym,		"getksym"	}, /* 202 */
	{ -1,	0,	sys_lwpsuspend,		"lwpsuspend"	}, /* 203 */
	{ -1,	0,	sys_lwpcontinue,	"lwpcontinue"	}, /* 204 */
	{ -1,	0,	sys_priocntllst,	"priocntllst"	}, /* 205 */
	{ -1,	0,	sys_sleep,		"sleep"		}, /* 206 */
	{ -1,	0,	sys_lwp_sema_wait,	"lwp_sema_wait"	}, /* 207 */
	{ -1,	0,	sys_lwp_sema_post,	"lwp_sema_post"	}, /* 208 */
	{ -1,	0,	sys_lwp_sema_trywait,	"lwp_sema_trywait"}, /* 209 */
	{ -1,	0,	printargs,		"SYS_210"	}, /* 210 */
	{ -1,	0,	printargs,		"SYS_211"	}, /* 211 */
	{ -1,	0,	printargs,		"SYS_212"	}, /* 212 */
	{ -1,	0,	printargs,		"SYS_213"	}, /* 213 */
	{ -1,	0,	printargs,		"SYS_214"	}, /* 214 */
	{ -1,	0,	printargs,		"SYS_215"	}, /* 215 */
#if UNIXWARE >= 7
	{ -1,	0,	sys_fstatvfs64,		"fstatvfs64"	}, /* 216 */
	{ -1,	TF,	sys_statvfs64,		"statvfs64"	}, /* 217 */
	{ -1,	TD,	sys_ftruncate64,	"ftruncate64"	}, /* 218 */
	{ -1,	TF,	sys_truncate64,		"truncate64"	}, /* 219 */
	{ -1,	0,	sys_getrlimit64,	"getrlimit64"	}, /* 220 */
	{ -1,	0,	sys_setrlimit64,	"setrlimit64"	}, /* 221 */
	{ -1,	TF,	sys_lseek64,		"lseek64"	}, /* 222 */
	{ -1,	TF,	sys_mmap64,		"mmap64"	}, /* 223 */
	{ -1,	TF,	sys_pread64,		"pread64"	}, /* 224 */
	{ -1,	TF,	sys_pwrite64,		"pwrite64"	}, /* 225 */
	{ -1,	TD|TF,	sys_creat64,		"creat64"	}, /* 226 */
	{ -1,	0,	sys_dshmsys,		"dshmsys"	}, /* 227 */
	{ -1,	0,	sys_invlpg,		"invlpg"	}, /* 228 */
	{ -1,	0,	sys_rfork1,		"rfork1"	}, /* 229 */
	{ -1,	0,	sys_rforkall,		"rforkall"	}, /* 230 */
	{ -1,	0,	sys_rexecve,		"rexecve"	}, /* 231 */
	{ -1,	0,	sys_migrate,		"migrate"	}, /* 232 */
	{ -1,	0,	sys_kill3,		"kill3"		}, /* 233 */
	{ -1,	0,	sys_ssisys,		"ssisys"	}, /* 234 */
	{ -1,	TN,	sys_xaccept,		"xaccept"	}, /* 235 */
	{ -1,	TN,	sys_xbind,		"xbind"		}, /* 236 */
	{ -1,	TN,	sys_xbindresvport,	"xbindresvport"	}, /* 237 */
	{ -1,	TN,	sys_xconnect,		"xconnect"	}, /* 238 */
	{ -1,	TN,	sys_xgetsockaddr,	"xgetsockaddr"	}, /* 239 */
	{ -1,	TN,	sys_xgetsockopt,	"xgetsockopt"	}, /* 240 */
	{ -1,	TN,	sys_xlisten,		"xlisten"	}, /* 241 */
	{ -1,	TN,	sys_xrecvmsg,		"xrecvmsg"	}, /* 242 */
	{ -1,	TN,	sys_xsendmsg,		"xsendmsg"	}, /* 243 */
	{ -1,	TN,	sys_xsetsockaddr,	"xsetsockaddr"	}, /* 244 */
	{ -1,	TN,	sys_xsetsockopt,	"xsetsockopt"	}, /* 245 */
	{ -1,	TN,	sys_xshutdown,		"xshutdown"	}, /* 246 */
	{ -1,	TN,	sys_xsocket,		"xsocket"	}, /* 247 */
	{ -1,	TN,	sys_xsocketpair,	"xsocketpair"	}, /* 248 */
#else	/* UNIXWARE 2 */
	{ -1,	0,	printargs,		"SYS_216"	}, /* 216 */
	{ -1,	0,	printargs,		"SYS_217"	}, /* 217 */
	{ -1,	0,	printargs,		"SYS_218"	}, /* 218 */
	{ -1,	0,	printargs,		"SYS_219"	}, /* 219 */
	{ -1,	0,	printargs,		"SYS_220"	}, /* 220 */
	{ -1,	0,	printargs,		"SYS_221"	}, /* 221 */
	{ -1,	0,	printargs,		"SYS_222"	}, /* 222 */
	{ -1,	0,	printargs,		"SYS_223"	}, /* 223 */
	{ -1,	0,	printargs,		"SYS_224"	}, /* 224 */
	{ -1,	0,	printargs,		"SYS_225"	}, /* 225 */
	{ -1,	0,	printargs,		"SYS_226"	}, /* 226 */
	{ -1,	0,	printargs,		"SYS_227"	}, /* 227 */
	{ -1,	0,	printargs,		"SYS_228"	}, /* 228 */
	{ -1,	0,	printargs,		"SYS_229"	}, /* 229 */
	{ -1,	0,	printargs,		"SYS_230"	}, /* 230 */
	{ -1,	0,	printargs,		"SYS_231"	}, /* 231 */
	{ -1,	0,	printargs,		"SYS_232"	}, /* 232 */
	{ -1,	0,	printargs,		"SYS_233"	}, /* 233 */
	{ -1,	0,	printargs,		"SYS_234"	}, /* 234 */
	{ -1,	0,	printargs,		"SYS_235"	}, /* 235 */
	{ -1,	0,	printargs,		"SYS_236"	}, /* 236 */
	{ -1,	0,	printargs,		"SYS_237"	}, /* 237 */
	{ -1,	0,	printargs,		"SYS_238"	}, /* 238 */
	{ -1,	0,	printargs,		"SYS_239"	}, /* 239 */
	{ -1,	0,	printargs,		"SYS_240"	}, /* 240 */
	{ -1,	0,	printargs,		"SYS_241"	}, /* 241 */
	{ -1,	0,	printargs,		"SYS_242"	}, /* 242 */
	{ -1,	0,	printargs,		"SYS_243"	}, /* 243 */
	{ -1,	0,	printargs,		"SYS_244"	}, /* 244 */
	{ -1,	0,	printargs,		"SYS_245"	}, /* 245 */
	{ -1,	0,	printargs,		"SYS_246"	}, /* 246 */
	{ -1,	0,	printargs,		"SYS_247"	}, /* 247 */
	{ -1,	0,	printargs,		"SYS_248"	}, /* 248 */
#endif	/* UNIXWARE 2 */
	{ -1,	0,	printargs,		"SYS_249"	}, /* 249 */
	{ -1,	0,	printargs,		"SYS_250"	}, /* 250 */
	{ -1,	0,	printargs,		"SYS_251"	}, /* 251 */
	{ -1,	0,	printargs,		"SYS_252"	}, /* 252 */
	{ -1,	0,	printargs,		"SYS_253"	}, /* 253 */
	{ -1,	0,	printargs,		"SYS_254"	}, /* 254 */
	{ -1,	0,	printargs,		"SYS_255"	}, /* 255 */
#else   /* !UNIXWARE */
	{ -1,	0,	sys_seteuid,		"seteuid"	}, /* 141 */
	{ -1,	0,	sys_vtrace,		"vtrace"	}, /* 142 */
	{ -1,	TP,	sys_fork1,		"fork1"		}, /* 143 */
	{ -1,	TS,	sys_sigtimedwait,	"sigtimedwait"	}, /* 144 */
	{ -1,	0,	sys_lwp_info,		"lwp_info"	}, /* 145 */
	{ -1,	0,	sys_yield,		"yield"		}, /* 146 */
	{ -1,	0,	sys_lwp_sema_wait,	"lwp_sema_wait"	}, /* 147 */
	{ -1,	0,	sys_lwp_sema_post,	"lwp_sema_post"	}, /* 148 */
	{ -1,	0,	sys_lwp_sema_trywait,"lwp_sema_trywait"	}, /* 149 */
	{ -1,	0,	printargs,		"SYS_150"	}, /* 150 */
	{ -1,	0,	printargs,		"SYS_151"	}, /* 151 */
	{ -1,	0,	sys_modctl,		"modctl"	}, /* 152 */
	{ -1,	0,	sys_fchroot,		"fchroot"	}, /* 153 */
	{ -1,	TF,	sys_utimes,		"utimes"	}, /* 154 */
	{ -1,	0,	sys_vhangup,		"vhangup"	}, /* 155 */
	{ -1,	0,	sys_gettimeofday,	"gettimeofday"	}, /* 156 */
	{ -1,	0,	sys_getitimer,		"getitimer"	}, /* 157 */
	{ -1,	0,	sys_setitimer,		"setitimer"	}, /* 158 */
	{ -1,	0,	sys_lwp_create,		"lwp_create"	}, /* 159 */
	{ -1,	0,	sys_lwp_exit,		"lwp_exit"	}, /* 160 */
	{ -1,	0,	sys_lwp_suspend,	"lwp_suspend"	}, /* 161 */
	{ -1,	0,	sys_lwp_continue,	"lwp_continue"	}, /* 162 */
	{ -1,	0,	sys_lwp_kill,		"lwp_kill"	}, /* 163 */
	{ -1,	0,	sys_lwp_self,		"lwp_self"	}, /* 164 */
	{ -1,	0,	sys_lwp_setprivate,	"lwp_setprivate"}, /* 165 */
	{ -1,	0,	sys_lwp_getprivate,	"lwp_getprivate"}, /* 166 */
	{ -1,	0,	sys_lwp_wait,		"lwp_wait"	}, /* 167 */
	{ -1,	0,	sys_lwp_mutex_unlock,	"lwp_mutex_unlock"}, /* 168 */
	{ -1,	0,	sys_lwp_mutex_lock,	"lwp_mutex_lock"}, /* 169 */
	{ -1,	0,	sys_lwp_cond_wait,	"lwp_cond_wait"}, /* 170 */
	{ -1,	0,	sys_lwp_cond_signal,	"lwp_cond_signal"}, /* 171 */
	{ -1,	0,	sys_lwp_cond_broadcast,	"lwp_cond_broadcast"}, /* 172 */
	{ -1,	TD,	sys_pread,		"pread"		}, /* 173 */
	{ -1,	TD,	sys_pwrite,		"pwrite"	}, /* 174 */
	{ -1,	TD,	sys_llseek,		"llseek"	}, /* 175 */
	{ -1,	0,	sys_inst_sync,		"inst_sync"	}, /* 176 */
	{ -1,	0,	printargs,		"srmlimitsys"	}, /* 177 */
	{ -1,	0,	sys_kaio,		"kaio"		}, /* 178 */
	{ -1,	0,	printargs,		"cpc"		}, /* 179 */
	{ -1,	0,	printargs,		"SYS_180"	}, /* 180 */
	{ -1,	0,	printargs,		"SYS_181"	}, /* 181 */
	{ -1,	0,	printargs,		"SYS_182"	}, /* 182 */
	{ -1,	0,	printargs,		"SYS_183"	}, /* 183 */
	{ -1,	0,	sys_tsolsys,		"tsolsys"	}, /* 184 */
#ifdef HAVE_SYS_ACL_H
	{ -1,	TF,	sys_acl,		"acl"		}, /* 185 */
#else
	{ -1,	0,	printargs,		"SYS_185"	}, /* 185 */
#endif
	{ -1,	0,	sys_auditsys,		"auditsys"	}, /* 186 */
	{ -1,	0,	sys_processor_bind,	"processor_bind"}, /* 187 */
	{ -1,	0,	sys_processor_info,	"processor_info"}, /* 188 */
	{ -1,	0,	sys_p_online,		"p_online"	}, /* 189 */
	{ -1,	0,	sys_sigqueue,		"sigqueue"	}, /* 190 */
	{ -1,	0,	sys_clock_gettime,	"clock_gettime"	}, /* 191 */
	{ -1,	0,	sys_clock_settime,	"clock_settime"	}, /* 192 */
	{ -1,	0,	sys_clock_getres,	"clock_getres"	}, /* 193 */
	{ -1,	0,	sys_timer_create,	"timer_create"	}, /* 194 */
	{ -1,	0,	sys_timer_delete,	"timer_delete"	}, /* 195 */
	{ -1,	0,	sys_timer_settime,	"timer_settime"	}, /* 196 */
	{ -1,	0,	sys_timer_gettime,	"timer_gettime"	}, /* 197 */
	{ -1,	0,	sys_timer_getoverrun,	"timer_getoverrun"}, /* 198 */
	{ -1,	0,	sys_nanosleep,		"nanosleep"	}, /* 199 */
#ifdef HAVE_SYS_ACL_H
	{ -1,	0,	sys_facl,		"facl"		}, /* 200 */
#else
	{ -1,	0,	printargs,		"SYS_200"	}, /* 200 */
#endif
#ifdef HAVE_SYS_DOOR_H
	{ -1,	0,	sys_door,		"door"		}, /* 201 */
#else
	{ -1,	0,	printargs,		"SYS_201"	}, /* 201 */
#endif
	{ -1,	0,	sys_setreuid,		"setreuid"	}, /* 202 */
	{ -1,	0,	sys_setregid,		"setregid"	}, /* 203 */
	{ -1,	0,	sys_install_utrap,	"install_utrap"	}, /* 204 */
	{ -1,	0,	sys_signotify,		"signotify"	}, /* 205 */
	{ -1,	0,	sys_schedctl,		"schedctl"	}, /* 206 */
	{ -1,	0,	sys_pset,		"pset"	}, /* 207 */
	{ -1,	0,	printargs,		"__sparc_utrap_install"	}, /* 208 */
	{ -1,	0,	sys_resolvepath,	"resolvepath"	}, /* 209 */
	{ -1,	0,	sys_signotifywait,	"signotifywait"	}, /* 210 */
	{ -1,	0,	sys_lwp_sigredirect,	"lwp_sigredirect"	}, /* 211 */
	{ -1,	0,	sys_lwp_alarm,		"lwp_alarm"	}, /* 212 */
	{ -1,	TD,	sys_getdents64,		"getdents64"	}, /* 213 */
	{ -1,	0,	sys_mmap64,		"mmap64"	}, /* 214 */
	{ -1,	0,	sys_stat64,		"stat64"	}, /* 215 */
	{ -1,	0,	sys_lstat64,		"lstat64"	}, /* 216 */
	{ -1,	TD,	sys_fstat64,		"fstat64"	}, /* 217 */
	{ -1,	0,	sys_statvfs64,		"statvfs64"	}, /* 218 */
	{ -1,	0,	sys_fstatvfs64,		"fstatvfs64"	}, /* 219 */
	{ -1,	0,	sys_setrlimit64,	"setrlimit64"	}, /* 220 */
	{ -1,	0,	sys_getrlimit64,	"getrlimit64"	}, /* 221 */
	{ -1,	TD,	sys_pread64,		"pread64"	}, /* 222 */
	{ -1,	TD,	sys_pwrite64,		"pwrite64"	}, /* 223 */
	{ -1,	0,	sys_creat64,		"creat64"	}, /* 224 */
	{ -1,	0,	sys_open64,		"open64"	}, /* 225 */
	{ -1,	0,	sys_rpcsys,		"rpcsys"	}, /* 226 */
	{ -1,	0,	printargs,		"SYS_227"	}, /* 227 */
	{ -1,	0,	printargs,		"SYS_228"	}, /* 228 */
	{ -1,	0,	printargs,		"SYS_229"	}, /* 229 */
	{ -1,	TN,	sys_so_socket,		"so_socket"	}, /* 230 */
	{ -1,	TN,	sys_so_socketpair,	"so_socketpair"	}, /* 231 */
	{ -1,	TN,	sys_bind,		"bind"		}, /* 232 */
	{ -1,	TN,	sys_listen,		"listen"	}, /* 233 */
	{ -1,	TN,	sys_accept,		"accept"	}, /* 234 */
	{ -1,	TN,	sys_connect,		"connect"	}, /* 235 */
	{ -1,	TN,	sys_shutdown,		"shutdown"	}, /* 236 */
	{ -1,	TN,	sys_recv,		"recv"		}, /* 237 */
	{ -1,	TN,	sys_recvfrom,		"recvfrom"	}, /* 238 */
	{ -1,	TN,	sys_recvmsg,		"recvmsg"	}, /* 239 */
	{ -1,	TN,	sys_send,		"send"		}, /* 240 */
	{ -1,	TN,	sys_sendmsg,		"sendmsg"	}, /* 241 */
	{ -1,	TN,	sys_sendto,		"sendto"	}, /* 242 */
	{ -1,	TN,	sys_getpeername,	"getpeername"	}, /* 243 */
	{ -1,	TN,	sys_getsockname,	"getsockname"	}, /* 244 */
	{ -1,	TN,	sys_getsockopt,		"getsockopt"	}, /* 245 */
	{ -1,	TN,	sys_setsockopt,		"setsockopt"	}, /* 246 */
	{ -1,	TN,	sys_sockconfig,		"sockconfig"	}, /* 247 */
	{ -1,	0,	sys_ntp_gettime,	"ntp_gettime"	}, /* 248 */
	{ -1,	0,	sys_ntp_adjtime,	"ntp_adjtime"	}, /* 249 */
	{ -1,	0,	printargs,		"lwp_mutex_unlock"	}, /* 250 */
	{ -1,	0,	printargs,		"lwp_mutex_trylock"	}, /* 251 */
	{ -1,	0,	printargs,		"lwp_mutex_init"	}, /* 252 */
	{ -1,	0,	printargs,		"cladm"		}, /* 253 */
	{ -1,	0,	printargs,		"lwp_sig_timedwait"	}, /* 254 */
	{ -1,	0,	printargs,		"umount2"	}, /* 255 */
#endif /* !UNIXWARE */
#endif /* !MIPS */
	{ -1,	0,	printargs,		"SYS_256"	}, /* 256 */
	{ -1,	0,	printargs,		"SYS_257"	}, /* 257 */
	{ -1,	0,	printargs,		"SYS_258"	}, /* 258 */
	{ -1,	0,	printargs,		"SYS_259"	}, /* 259 */
	{ -1,	0,	printargs,		"SYS_260"	}, /* 260 */
	{ -1,	0,	printargs,		"SYS_261"	}, /* 261 */
	{ -1,	0,	printargs,		"SYS_262"	}, /* 262 */
	{ -1,	0,	printargs,		"SYS_263"	}, /* 263 */
	{ -1,	0,	printargs,		"SYS_264"	}, /* 264 */
	{ -1,	0,	printargs,		"SYS_265"	}, /* 265 */
	{ -1,	0,	printargs,		"SYS_266"	}, /* 266 */
	{ -1,	0,	printargs,		"SYS_267"	}, /* 267 */
	{ -1,	0,	printargs,		"SYS_268"	}, /* 268 */
	{ -1,	0,	printargs,		"SYS_269"	}, /* 269 */
	{ -1,	0,	printargs,		"SYS_270"	}, /* 270 */
	{ -1,	0,	printargs,		"SYS_271"	}, /* 271 */
	{ -1,	0,	printargs,		"SYS_272"	}, /* 272 */
	{ -1,	0,	printargs,		"SYS_273"	}, /* 273 */
	{ -1,	0,	printargs,		"SYS_274"	}, /* 274 */
	{ -1,	0,	printargs,		"SYS_275"	}, /* 275 */
	{ -1,	0,	printargs,		"SYS_276"	}, /* 276 */
	{ -1,	0,	printargs,		"SYS_277"	}, /* 277 */
	{ -1,	0,	printargs,		"SYS_278"	}, /* 278 */
	{ -1,	0,	printargs,		"SYS_279"	}, /* 279 */
	{ -1,	0,	printargs,		"SYS_280"	}, /* 280 */
	{ -1,	0,	printargs,		"SYS_281"	}, /* 281 */
	{ -1,	0,	printargs,		"SYS_282"	}, /* 282 */
	{ -1,	0,	printargs,		"SYS_283"	}, /* 283 */
	{ -1,	0,	printargs,		"SYS_284"	}, /* 284 */
	{ -1,	0,	printargs,		"SYS_285"	}, /* 285 */
	{ -1,	0,	printargs,		"SYS_286"	}, /* 286 */
	{ -1,	0,	printargs,		"SYS_287"	}, /* 287 */
	{ -1,	0,	printargs,		"SYS_288"	}, /* 288 */
	{ -1,	0,	printargs,		"SYS_289"	}, /* 289 */
	{ -1,	0,	printargs,		"SYS_290"	}, /* 290 */
	{ -1,	0,	printargs,		"SYS_291"	}, /* 291 */
	{ -1,	0,	printargs,		"SYS_292"	}, /* 292 */
	{ -1,	0,	printargs,		"SYS_293"	}, /* 293 */
	{ -1,	0,	printargs,		"SYS_294"	}, /* 294 */
	{ -1,	0,	printargs,		"SYS_295"	}, /* 295 */
	{ -1,	0,	printargs,		"SYS_296"	}, /* 296 */
	{ -1,	0,	printargs,		"SYS_297"	}, /* 297 */
	{ -1,	0,	printargs,		"SYS_298"	}, /* 298 */
	{ -1,	0,	printargs,		"SYS_299"	}, /* 299 */

	{ -1,	0,	sys_getpgrp,		"getpgrp"	}, /* 300 */
	{ -1,	0,	sys_setpgrp,		"setpgrp"	}, /* 301 */
	{ -1,	0,	sys_getsid,		"getsid"	}, /* 302 */
	{ -1,	0,	sys_setsid,		"setsid"	}, /* 303 */
	{ -1,	0,	sys_getpgid,		"getpgid"	}, /* 304 */
	{ -1,	0,	sys_setpgid,		"setpgid"	}, /* 305 */
	{ -1,	0,	printargs,		"SYS_306"	}, /* 306 */
	{ -1,	0,	printargs,		"SYS_307"	}, /* 307 */
	{ -1,	0,	printargs,		"SYS_308"	}, /* 308 */
	{ -1,	0,	printargs,		"SYS_309"	}, /* 309 */

	{ -1,	TS,	sys_signal,		"signal"	}, /* 310 */
	{ -1,	TS,	sys_sigset,		"sigset"	}, /* 311 */
	{ -1,	TS,	sys_sighold,		"sighold"	}, /* 312 */
	{ -1,	TS,	sys_sigrelse,		"sigrelse"	}, /* 313 */
	{ -1,	TS,	sys_sigignore,		"sigignore"	}, /* 314 */
	{ -1,	TS,	sys_sigpause,		"sigpause"	}, /* 315 */
	{ -1,	0,	printargs,		"SYS_316"	}, /* 316 */
	{ -1,	0,	printargs,		"SYS_317"	}, /* 317 */
	{ -1,	0,	printargs,		"SYS_318"	}, /* 318 */
	{ -1,	0,	printargs,		"SYS_319"	}, /* 319 */

	{ -1,	TI,	sys_msgget,		"msgget"	}, /* 320 */
	{ -1,	TI,	sys_msgctl,		"msgctl"	}, /* 321 */
	{ -1,	TI,	sys_msgrcv,		"msgrcv"	}, /* 322 */
	{ -1,	TI,	sys_msgsnd,		"msgsnd"	}, /* 323 */
	{ -1,	0,	printargs,		"SYS_324"	}, /* 324 */
	{ -1,	0,	printargs,		"SYS_325"	}, /* 325 */
	{ -1,	0,	printargs,		"SYS_326"	}, /* 326 */
	{ -1,	0,	printargs,		"SYS_327"	}, /* 327 */
	{ -1,	0,	printargs,		"SYS_328"	}, /* 328 */
	{ -1,	0,	printargs,		"SYS_329"	}, /* 329 */

	{ -1,	TI,	sys_shmat,		"shmat"		}, /* 330 */
	{ -1,	TI,	sys_shmctl,		"shmctl"	}, /* 331 */
	{ -1,	TI,	sys_shmdt,		"shmdt"		}, /* 332 */
	{ -1,	TI,	sys_shmget,		"shmget"	}, /* 333 */
	{ -1,	0,	printargs,		"SYS_334"	}, /* 334 */
	{ -1,	0,	printargs,		"SYS_335"	}, /* 335 */
	{ -1,	0,	printargs,		"SYS_336"	}, /* 336 */
	{ -1,	0,	printargs,		"SYS_337"	}, /* 337 */
	{ -1,	0,	printargs,		"SYS_338"	}, /* 338 */
	{ -1,	0,	printargs,		"SYS_339"	}, /* 339 */

	{ -1,	TI,	sys_semctl,		"semctl"	}, /* 340 */
	{ -1,	TI,	sys_semget,		"semget"	}, /* 341 */
	{ -1,	TI,	sys_semop,		"semop"		}, /* 342 */
	{ -1,	0,	printargs,		"SYS_343"	}, /* 343 */
	{ -1,	0,	printargs,		"SYS_344"	}, /* 344 */
	{ -1,	0,	printargs,		"SYS_345"	}, /* 345 */
	{ -1,	0,	printargs,		"SYS_346"	}, /* 346 */
	{ -1,	0,	printargs,		"SYS_347"	}, /* 347 */
	{ -1,	0,	printargs,		"SYS_348"	}, /* 348 */
	{ -1,	0,	printargs,		"SYS_349"	}, /* 349 */

	{ -1,	0,	sys_olduname,		"olduname"	}, /* 350 */
	{ -1,	0,	printargs,		"utssys1"	}, /* 351 */
	{ -1,	0,	sys_ustat,		"ustat"		}, /* 352 */
	{ -1,	0,	sys_fusers,		"fusers"	}, /* 353 */
	{ -1,	0,	printargs,		"SYS_354"	}, /* 354 */
	{ -1,	0,	printargs,		"SYS_355"	}, /* 355 */
	{ -1,	0,	printargs,		"SYS_356"	}, /* 356 */
	{ -1,	0,	printargs,		"SYS_357"	}, /* 357 */
	{ -1,	0,	printargs,		"SYS_358"	}, /* 358 */
	{ -1,	0,	printargs,		"SYS_359"	}, /* 359 */

	{ -1,	0,	printargs,		"sysfs0"	}, /* 360 */
	{ -1,	0,	sys_sysfs1,		"sysfs1"	}, /* 361 */
	{ -1,	0,	sys_sysfs2,		"sysfs2"	}, /* 362 */
	{ -1,	0,	sys_sysfs3,		"sysfs3"	}, /* 363 */
	{ -1,	0,	printargs,		"SYS_364"	}, /* 364 */
	{ -1,	0,	printargs,		"SYS_365"	}, /* 365 */
	{ -1,	0,	printargs,		"SYS_366"	}, /* 366 */
	{ -1,	0,	printargs,		"SYS_367"	}, /* 367 */
	{ -1,	0,	printargs,		"SYS_368"	}, /* 368 */
	{ -1,	0,	printargs,		"SYS_369"	}, /* 369 */

	{ -1,	0,	printargs,		"spcall0"	}, /* 370 */
	{ -1,	TS,	sys_sigpending,		"sigpending"	}, /* 371 */
	{ -1,	TS,	sys_sigfillset,		"sigfillset"	}, /* 372 */
	{ -1,	0,	printargs,		"SYS_373"	}, /* 373 */
	{ -1,	0,	printargs,		"SYS_374"	}, /* 374 */
	{ -1,	0,	printargs,		"SYS_375"	}, /* 375 */
	{ -1,	0,	printargs,		"SYS_376"	}, /* 376 */
	{ -1,	0,	printargs,		"SYS_377"	}, /* 377 */
	{ -1,	0,	printargs,		"SYS_378"	}, /* 378 */
	{ -1,	0,	printargs,		"SYS_379"	}, /* 379 */

	{ -1,	0,	sys_getcontext,		"getcontext"	}, /* 380 */
	{ -1,	0,	sys_setcontext,		"setcontext"	}, /* 381 */
	{ -1,	0,	printargs,		"SYS_382"	}, /* 382 */
	{ -1,	0,	printargs,		"SYS_383"	}, /* 383 */
	{ -1,	0,	printargs,		"SYS_384"	}, /* 384 */
	{ -1,	0,	printargs,		"SYS_385"	}, /* 385 */
	{ -1,	0,	printargs,		"SYS_386"	}, /* 386 */
	{ -1,	0,	printargs,		"SYS_387"	}, /* 387 */
	{ -1,	0,	printargs,		"SYS_388"	}, /* 388 */
	{ -1,	0,	printargs,		"SYS_389"	}, /* 389 */

	{ -1,	0,	printargs,		"door_create"	}, /* 390 */
	{ -1,	0,	printargs,		"door_revoke"	}, /* 391 */
	{ -1,	0,	printargs,		"door_info"	}, /* 392 */
	{ -1,	0,	printargs,		"door_call"	}, /* 393 */
	{ -1,	0,	printargs,		"door_return"	}, /* 394 */
	{ -1,	0,	printargs,		"door_cred"	}, /* 395 */
	{ -1,	0,	printargs,		"SYS_396"	}, /* 396 */
	{ -1,	0,	printargs,		"SYS_397"	}, /* 397 */
	{ -1,	0,	printargs,		"SYS_398"	}, /* 398 */
	{ -1,	0,	printargs,		"SYS_399"	}, /* 399 */

#ifdef HAVE_SYS_AIO_H
	{ -1,	TF,	sys_aioread,		"aioread"	}, /* 400 */
	{ -1,	TF,	sys_aiowrite,		"aiowrite"	}, /* 401 */
	{ -1,	TF,	sys_aiowait,		"aiowait"	}, /* 402 */
	{ -1,	TF,	sys_aiocancel,		"aiocancel"	}, /* 403 */
	{ -1,	TF,	sys_aionotify,		"aionotify"	}, /* 404 */
	{ -1,	TF,	sys_aioinit,		"aioinit"	}, /* 405 */
	{ -1,	TF,	sys_aiostart,		"aiostart"	}, /* 406 */
	{ -1,	TF,	sys_aiolio,		"aiolio"	}, /* 407 */
	{ -1,	TF,	sys_aiosuspend,		"aiosuspend"	}, /* 408 */
	{ -1,	TF,	sys_aioerror,		"aioerror"	}, /* 409 */
	{ -1,	TF,	sys_aioliowait,		"aioliowait"	}, /* 410 */
	{ -1,	TF,	sys_aioaread,		"aioaread"	}, /* 411 */
	{ -1,	TF,	sys_aioawrite,		"aioawrite"	}, /* 412 */
	{ -1,	TF,	sys_aiolio64,		"aiolio64"	}, /* 413 */
	{ -1,	TF,	sys_aiosuspend64,	"aiosuspend64"	}, /* 414 */
	{ -1,	TF,	sys_aioerror64,		"aioerror64"	}, /* 415 */
	{ -1,	TF,	sys_aioliowait64,	"aioliowait64"	}, /* 416 */
	{ -1,	TF,	sys_aioaread64,		"aioaread64"	}, /* 417 */
	{ -1,	TF,	sys_aioawrite64,	"aioawrite64"	}, /* 418 */
	{ -1,	TF,	sys_aiocancel64,	"aiocancel64"	}, /* 419 */
	{ -1,	TF,	sys_aiofsync,		"aiofsync"	}, /* 420 */
#endif

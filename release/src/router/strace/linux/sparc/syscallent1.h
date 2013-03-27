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

	{ 6,	0,	solaris_syscall,	"syscall"	}, /* 0 */
	{ 6,	TP,	solaris_exit,		"_exit"		}, /* 1 */
	{ 6,	TP,	solaris_fork,		"fork"		}, /* 2 */
	{ 6,	0,	solaris_read,		"read"		}, /* 3 */
	{ 6,	0,	solaris_write,		"write"		}, /* 4 */
	{ 6,	TF,	solaris_open,		"open"		}, /* 5 */
	{ 6,	0,	solaris_close,		"close"		}, /* 6 */
	{ 6,	TP,	solaris_wait,		"wait"		}, /* 7 */
	{ 6,	TF,	solaris_creat,		"creat"		}, /* 8 */
	{ 6,	TF,	solaris_link,		"link"		}, /* 9 */
	{ 6,	TF,	solaris_unlink,		"unlink"	}, /* 10 */
	{ 6,	TF|TP,	solaris_exec,		"exec"		}, /* 11 */
	{ 6,	TF,	solaris_chdir,		"chdir"		}, /* 12 */
	{ 6,	0,	solaris_time,		"time"		}, /* 13 */
	{ 6,	TF,	solaris_mknod,		"mknod"		}, /* 14 */
	{ 6,	TF,	solaris_chmod,		"chmod"		}, /* 15 */
	{ 6,	TF,	solaris_chown,		"chown"		}, /* 16 */
	{ 6,	0,	solaris_brk,		"brk"		}, /* 17 */
	{ 6,	TF,	solaris_stat,		"stat"		}, /* 18 */
	{ 6,	0,	solaris_lseek,		"lseek"		}, /* 19 */
	{ 6,	0,	solaris_getpid,		"getpid"	}, /* 20 */
	{ 6,	TF,	solaris_mount,		"mount"		}, /* 21 */
	{ 6,	TF,	solaris_umount,		"umount"	}, /* 22 */
	{ 6,	0,	solaris_setuid,		"setuid"	}, /* 23 */
	{ 6,	0,	solaris_getuid,		"getuid"	}, /* 24 */
	{ 6,	0,	solaris_stime,		"stime"		}, /* 25 */
	{ 6,	0,	solaris_ptrace,		"ptrace"	}, /* 26 */
	{ 6,	0,	solaris_alarm,		"alarm"		}, /* 27 */
	{ 6,	0,	solaris_fstat,		"fstat"		}, /* 28 */
	{ 6,	TS,	solaris_pause,		"pause"		}, /* 29 */
	{ 6,	TF,	solaris_utime,		"utime"		}, /* 30 */
	{ 6,	0,	solaris_stty,		"stty"		}, /* 31 */
	{ 6,	0,	solaris_gtty,		"gtty"		}, /* 32 */
	{ 6,	TF,	solaris_access,		"access"	}, /* 33 */
	{ 6,	0,	solaris_nice,		"nice"		}, /* 34 */
	{ 6,	TF,	solaris_statfs,		"statfs"	}, /* 35 */
	{ 6,	0,	solaris_sync,		"sync"		}, /* 36 */
	{ 6,	TS,	solaris_kill,		"kill"		}, /* 37 */
	{ 6,	0,	solaris_fstatfs,	"fstatfs"	}, /* 38 */
	{ 6,	0,	solaris_pgrpsys,	"pgrpsys"	}, /* 39 */
	{ 6,	0,	solaris_xenix,		"xenix"		}, /* 40 */
	{ 6,	0,	solaris_dup,		"dup"		}, /* 41 */
	{ 6,	0,	solaris_pipe,		"pipe"		}, /* 42 */
	{ 6,	0,	solaris_times,		"times"		}, /* 43 */
	{ 6,	0,	solaris_profil,		"profil"	}, /* 44 */
	{ 6,	0,	solaris_plock,		"plock"		}, /* 45 */
	{ 6,	0,	solaris_setgid,		"setgid"	}, /* 46 */
	{ 6,	0,	solaris_getgid,		"getgid"	}, /* 47 */
	{ 6,	0,	solaris_sigcall,	"sigcall"	}, /* 48 */
	{ 6,	TI,	solaris_msgsys,		"msgsys"	}, /* 49 */
	{ 6,	0,	solaris_syssun,		"syssun"	}, /* 50 */
	{ 6,	TF,	solaris_acct,		"acct"		}, /* 51 */
	{ 6,	TI,	solaris_shmsys,		"shmsys"	}, /* 52 */
	{ 6,	TI,	solaris_semsys,		"semsys"	}, /* 53 */
	{ 6,	0,	solaris_ioctl,		"ioctl"		}, /* 54 */
	{ 6,	0,	solaris_uadmin,		"uadmin"	}, /* 55 */
	{ 6,	0,	solaris_sysmp,		"sysmp"		}, /* 56 */
	{ 6,	0,	solaris_utssys,		"utssys"	}, /* 57 */
	{ 6,	0,	solaris_fdsync,		"fdsync"	}, /* 58 */
	{ 6,	TF|TP,	solaris_execve,		"execve"	}, /* 59 */
	{ 6,	0,	solaris_umask,		"umask"		}, /* 60 */
	{ 6,	TF,	solaris_chroot,		"chroot"	}, /* 61 */
	{ 6,	0,	solaris_fcntl,		"fcntl"		}, /* 62 */
	{ 6,	0,	solaris_ulimit,		"ulimit"	}, /* 63 */
	{ 6,	0,	printargs,		"SYS_64"	}, /* 64 */
	{ 6,	0,	printargs,		"SYS_65"	}, /* 65 */
	{ 6,	0,	printargs,		"SYS_66"	}, /* 66 */
	{ 6,	0,	printargs,		"SYS_67"	}, /* 67 */
	{ 6,	0,	printargs,		"SYS_68"	}, /* 68 */
	{ 6,	0,	printargs,		"SYS_69"	}, /* 69 */
	{ 6,	0,	printargs,		"SYS_70"	}, /* 70 */
	{ 6,	0,	printargs,		"SYS_71"	}, /* 71 */
	{ 6,	0,	printargs,		"SYS_72"	}, /* 72 */
	{ 6,	0,	printargs,		"SYS_73"	}, /* 73 */
	{ 6,	0,	printargs,		"SYS_74"	}, /* 74 */
	{ 6,	0,	printargs,		"SYS_75"	}, /* 75 */
	{ 6,	0,	printargs,		"SYS_76"	}, /* 76 */
	{ 6,	0,	printargs,		"SYS_77"	}, /* 77 */
	{ 6,	0,	printargs,		"SYS_78"	}, /* 78 */
	{ 6,	TF,	solaris_rmdir,		"rmdir"		}, /* 79 */
	{ 6,	TF,	solaris_mkdir,		"mkdir"		}, /* 80 */
	{ 6,	0,	solaris_getdents,	"getdents"	}, /* 81 */
	{ 6,	0,	solaris_sginap,		"sginap"	}, /* 82 */
	{ 6,	0,	solaris_sgikopt,	"sgikopt"	}, /* 83 */
	{ 6,	0,	solaris_sysfs,		"sysfs"		}, /* 84 */
	{ 6,	TN,	sys_getmsg,		"getmsg"	}, /* 85 */
	{ 6,	TN,	sys_putmsg,		"putmsg"	}, /* 86 */
	{ 6,	TN,	solaris_poll,		"poll"		}, /* 87 */
	{ 6,	TF,	solaris_lstat,		"lstat"		}, /* 88 */
	{ 6,	TF,	solaris_symlink,	"symlink"	}, /* 89 */
	{ 6,	TF,	solaris_readlink,	"readlink"	}, /* 90 */
	{ 6,	0,	solaris_setgroups,	"setgroups"	}, /* 91 */
	{ 6,	0,	solaris_getgroups,	"getgroups"	}, /* 92 */
	{ 6,	0,	solaris_fchmod,		"fchmod"	}, /* 93 */
	{ 6,	0,	solaris_fchown,		"fchown"	}, /* 94 */
	{ 6,	TS,	solaris_sigprocmask,	"sigprocmask"	}, /* 95 */
	{ 6,	TS,	solaris_sigsuspend,	"sigsuspend"	}, /* 96 */
	{ 6,	TS,	solaris_sigaltstack,	"sigaltstack"	}, /* 97 */
	{ 6,	TS,	solaris_sigaction,	"sigaction"	}, /* 98 */
	{ 6,	0,	solaris_spcall,		"spcall"	}, /* 99 */
	{ 6,	0,	solaris_context,	"context"	}, /* 100 */
	{ 6,	0,	solaris_evsys,		"evsys"		}, /* 101 */
	{ 6,	0,	solaris_evtrapret,	"evtrapret"	}, /* 102 */
	{ 6,	TF,	solaris_statvfs,	"statvfs"	}, /* 103 */
	{ 6,	0,	solaris_fstatvfs,	"fstatvfs"	}, /* 104 */
	{ 6,	0,	printargs,		"SYS_105"	}, /* 105 */
	{ 6,	0,	solaris_nfssys,		"nfssys"	}, /* 106 */
	{ 6,	TP,	solaris_waitid,		"waitid"	}, /* 107 */
	{ 6,	0,	solaris_sigsendsys,	"sigsendsys"	}, /* 108 */
	{ 6,	0,	solaris_hrtsys,		"hrtsys"	}, /* 109 */
	{ 6,	0,	solaris_acancel,	"acancel"	}, /* 110 */
	{ 6,	0,	solaris_async,		"async"		}, /* 111 */
	{ 6,	0,	solaris_priocntlsys,	"priocntlsys"	}, /* 112 */
	{ 6,	TF,	solaris_pathconf,	"pathconf"	}, /* 113 */
	{ 6,	0,	solaris_mincore,	"mincore"	}, /* 114 */
	{ 6,	TD,	solaris_mmap,		"mmap"		}, /* 115 */
	{ 6,	0,	solaris_mprotect,	"mprotect"	}, /* 116 */
	{ 6,	0,	solaris_munmap,		"munmap"	}, /* 117 */
	{ 6,	0,	solaris_fpathconf,	"fpathconf"	}, /* 118 */
	{ 6,	TP,	solaris_vfork,		"vfork"		}, /* 119 */
	{ 6,	0,	solaris_fchdir,		"fchdir"	}, /* 120 */
	{ 6,	0,	solaris_readv,		"readv"		}, /* 121 */
	{ 6,	0,	solaris_writev,		"writev"	}, /* 122 */
	{ 6,	TF,	solaris_xstat,		"xstat"		}, /* 123 */
	{ 6,	TF,	solaris_lxstat,		"lxstat"	}, /* 124 */
	{ 6,	0,	solaris_fxstat,		"fxstat"	}, /* 125 */
	{ 6,	TF,	solaris_xmknod,		"xmknod"	}, /* 126 */
	{ 6,	0,	solaris_clocal,		"clocal"	}, /* 127 */
	{ 6,	0,	solaris_setrlimit,	"setrlimit"	}, /* 128 */
	{ 6,	0,	solaris_getrlimit,	"getrlimit"	}, /* 129 */
	{ 6,	TF,	solaris_lchown,		"lchown"	}, /* 130 */
	{ 6,	0,	solaris_memcntl,	"memcntl"	}, /* 131 */
	{ 6,	TN,	solaris_getpmsg,	"getpmsg"	}, /* 132 */
	{ 6,	TN,	solaris_putpmsg,	"putpmsg"	}, /* 133 */
	{ 6,	TF,	solaris_rename,		"rename"	}, /* 134 */
	{ 6,	0,	solaris_uname,		"uname"		}, /* 135 */
	{ 6,	0,	solaris_setegid,	"setegid"	}, /* 136 */
	{ 6,	0,	solaris_sysconfig,	"sysconfig"	}, /* 137 */
	{ 6,	0,	solaris_adjtime,	"adjtime"	}, /* 138 */
	{ 6,	0,	solaris_sysinfo,	"sysinfo"	}, /* 139 */
	{ 6,	0,	printargs,		"SYS_140"	}, /* 140 */
	{ 6,	0,	solaris_seteuid,	"seteuid"	}, /* 141 */
	{ 6,	0,	solaris_vtrace,		"vtrace"	}, /* 142 */
	{ 6,	TP,	solaris_fork1,		"fork1"		}, /* 143 */
	{ 6,	TS,	solaris_sigtimedwait,	"sigtimedwait"	}, /* 144 */
	{ 6,	0,	solaris_lwp_info,	"lwp_info"	}, /* 145 */
	{ 6,	0,	solaris_yield,		"yield"		}, /* 146 */
	{ 6,	0,	solaris_lwp_sema_wait,	"lwp_sema_wait"	}, /* 147 */
	{ 6,	0,	solaris_lwp_sema_post,	"lwp_sema_post"	}, /* 148 */
	{ 6,	0,	printargs,		"SYS_149"	}, /* 149 */
	{ 6,	0,	printargs,		"SYS_150"	}, /* 150 */
	{ 6,	0,	printargs,		"SYS_151"	}, /* 151 */
	{ 6,	0,	solaris_modctl,		"modctl"	}, /* 152 */
	{ 6,	0,	solaris_fchroot,	"fchroot"	}, /* 153 */
	{ 6,	TF,	solaris_utimes,		"utimes"	}, /* 154 */
	{ 6,	0,	solaris_vhangup,	"vhangup"	}, /* 155 */
	{ 6,	0,	solaris_gettimeofday,	"gettimeofday"	}, /* 156 */
	{ 6,	0,	solaris_getitimer,	"getitimer"	}, /* 157 */
	{ 6,	0,	solaris_setitimer,	"setitimer"	}, /* 158 */
	{ 6,	0,	solaris_lwp_create,	"lwp_create"	}, /* 159 */
	{ 6,	0,	solaris_lwp_exit,	"lwp_exit"	}, /* 160 */
	{ 6,	0,	solaris_lwp_suspend,	"lwp_suspend"	}, /* 161 */
	{ 6,	0,	solaris_lwp_continue,	"lwp_continue"	}, /* 162 */
	{ 6,	0,	solaris_lwp_kill,	"lwp_kill"	}, /* 163 */
	{ 6,	0,	solaris_lwp_self,	"lwp_self"	}, /* 164 */
	{ 6,	0,	solaris_lwp_setprivate,	"lwp_setprivate"}, /* 165 */
	{ 6,	0,	solaris_lwp_getprivate,	"lwp_getprivate"}, /* 166 */
	{ 6,	0,	solaris_lwp_wait,	"lwp_wait"	}, /* 167 */
	{ 6,	0,	solaris_lwp_mutex_unlock,"lwp_mutex_unlock"}, /* 168 */
	{ 6,	0,	solaris_lwp_mutex_lock,	"lwp_mutex_lock"}, /* 169 */
	{ 6,	0,	solaris_lwp_cond_wait,	"lwp_cond_wait"}, /* 170 */
	{ 6,	0,	solaris_lwp_cond_signal,"lwp_cond_signal"}, /* 171 */
	{ 6,	0,	solaris_lwp_cond_broadcast,"lwp_cond_broadcast"}, /* 172 */
	{ 6,	0,	solaris_pread,		"pread"		}, /* 173 */
	{ 6,	0,	solaris_pwrite,		"pwrite"	}, /* 174 */
	{ 6,	0,	solaris_llseek,		"llseek"	}, /* 175 */
	{ 6,	0,	solaris_inst_sync,	"inst_sync"	}, /* 176 */
	{ 6,	0,	printargs,		"SYS_177"	}, /* 177 */
	{ 6,	0,	printargs,		"SYS_178"	}, /* 178 */
	{ 6,	0,	printargs,		"SYS_179"	}, /* 179 */
	{ 6,	0,	printargs,		"SYS_180"	}, /* 180 */
	{ 6,	0,	printargs,		"SYS_181"	}, /* 181 */
	{ 6,	0,	printargs,		"SYS_182"	}, /* 182 */
	{ 6,	0,	printargs,		"SYS_183"	}, /* 183 */
	{ 6,	0,	printargs,		"SYS_184"	}, /* 184 */
	{ 6,	0,	printargs,		"SYS_185"	}, /* 185 */
	{ 6,	0,	solaris_auditsys,	"auditsys"	}, /* 186 */
	{ 6,	0,	solaris_processor_bind,	"processor_bind"}, /* 187 */
	{ 6,	0,	solaris_processor_info,	"processor_info"}, /* 188 */
	{ 6,	0,	solaris_p_online,	"p_online"	}, /* 189 */
	{ 6,	0,	solaris_sigqueue,	"sigqueue"	}, /* 190 */
	{ 6,	0,	solaris_clock_gettime,	"clock_gettime"	}, /* 191 */
	{ 6,	0,	solaris_clock_settime,	"clock_settime"	}, /* 192 */
	{ 6,	0,	solaris_clock_getres,	"clock_getres"	}, /* 193 */
	{ 6,	0,	solaris_timer_create,	"timer_create"	}, /* 194 */
	{ 6,	0,	solaris_timer_delete,	"timer_delete"	}, /* 195 */
	{ 6,	0,	solaris_timer_settime,	"timer_settime"	}, /* 196 */
	{ 6,	0,	solaris_timer_gettime,	"timer_gettime"	}, /* 197 */
	{ 6,	0,	solaris_timer_getoverrun,"timer_getoverrun"}, /* 198 */
	{ 6,	0,	solaris_nanosleep,	"nanosleep"	}, /* 199 */
	{ 6,	0,	printargs,		"SYS_200"	}, /* 200 */
	{ 6,	0,	printargs,		"SYS_201"	}, /* 201 */
	{ 6,	0,	printargs,		"SYS_202"	}, /* 202 */
	{ 6,	0,	printargs,		"SYS_203"	}, /* 203 */
	{ 6,	0,	printargs,		"SYS_204"	}, /* 204 */
	{ 6,	0,	printargs,		"SYS_205"	}, /* 205 */
	{ 6,	0,	printargs,		"SYS_206"	}, /* 206 */
	{ 6,	0,	printargs,		"SYS_207"	}, /* 207 */
	{ 6,	0,	printargs,		"SYS_208"	}, /* 208 */
	{ 6,	0,	printargs,		"SYS_209"	}, /* 209 */
	{ 6,	0,	printargs,		"SYS_210"	}, /* 210 */
	{ 6,	0,	printargs,		"SYS_211"	}, /* 211 */
	{ 6,	0,	printargs,		"SYS_212"	}, /* 212 */
	{ 6,	0,	printargs,		"SYS_213"	}, /* 213 */
	{ 6,	0,	printargs,		"SYS_214"	}, /* 214 */
	{ 6,	0,	printargs,		"SYS_215"	}, /* 215 */
	{ 6,	0,	printargs,		"SYS_216"	}, /* 216 */
	{ 6,	0,	printargs,		"SYS_217"	}, /* 217 */
	{ 6,	0,	printargs,		"SYS_218"	}, /* 218 */
	{ 6,	0,	printargs,		"SYS_219"	}, /* 219 */
	{ 6,	0,	printargs,		"SYS_220"	}, /* 220 */
	{ 6,	0,	printargs,		"SYS_221"	}, /* 221 */
	{ 6,	0,	printargs,		"SYS_222"	}, /* 222 */
	{ 6,	0,	printargs,		"SYS_223"	}, /* 223 */
	{ 6,	0,	printargs,		"SYS_224"	}, /* 224 */
	{ 6,	0,	printargs,		"SYS_225"	}, /* 225 */
	{ 6,	0,	printargs,		"SYS_226"	}, /* 226 */
	{ 6,	0,	printargs,		"SYS_227"	}, /* 227 */
	{ 6,	0,	printargs,		"SYS_228"	}, /* 228 */
	{ 6,	0,	printargs,		"SYS_229"	}, /* 229 */
	{ 6,	0,	printargs,		"SYS_230"	}, /* 230 */
	{ 6,	0,	printargs,		"SYS_231"	}, /* 231 */
	{ 6,	0,	printargs,		"SYS_232"	}, /* 232 */
	{ 6,	0,	printargs,		"SYS_233"	}, /* 233 */
	{ 6,	0,	printargs,		"SYS_234"	}, /* 234 */
	{ 6,	0,	printargs,		"SYS_235"	}, /* 235 */
	{ 6,	0,	printargs,		"SYS_236"	}, /* 236 */
	{ 6,	0,	printargs,		"SYS_237"	}, /* 237 */
	{ 6,	0,	printargs,		"SYS_238"	}, /* 238 */
	{ 6,	0,	printargs,		"SYS_239"	}, /* 239 */
	{ 6,	0,	printargs,		"SYS_240"	}, /* 240 */
	{ 6,	0,	printargs,		"SYS_241"	}, /* 241 */
	{ 6,	0,	printargs,		"SYS_242"	}, /* 242 */
	{ 6,	0,	printargs,		"SYS_243"	}, /* 243 */
	{ 6,	0,	printargs,		"SYS_244"	}, /* 244 */
	{ 6,	0,	printargs,		"SYS_245"	}, /* 245 */
	{ 6,	0,	printargs,		"SYS_246"	}, /* 246 */
	{ 6,	0,	printargs,		"SYS_247"	}, /* 247 */
	{ 6,	0,	printargs,		"SYS_248"	}, /* 248 */
	{ 6,	0,	printargs,		"SYS_249"	}, /* 249 */
	{ 6,	0,	printargs,		"SYS_250"	}, /* 250 */
	{ 6,	0,	printargs,		"SYS_251"	}, /* 251 */
	{ 6,	0,	printargs,		"SYS_252"	}, /* 252 */
	{ 6,	0,	printargs,		"SYS_253"	}, /* 253 */
	{ 6,	0,	printargs,		"SYS_254"	}, /* 254 */
	{ 6,	0,	printargs,		"SYS_255"	}, /* 255 */
	{ 6,	0,	printargs,		"SYS_256"	}, /* 256 */
	{ 6,	0,	printargs,		"SYS_257"	}, /* 257 */
	{ 6,	0,	printargs,		"SYS_258"	}, /* 258 */
	{ 6,	0,	printargs,		"SYS_259"	}, /* 259 */
	{ 6,	0,	printargs,		"SYS_260"	}, /* 260 */
	{ 6,	0,	printargs,		"SYS_261"	}, /* 261 */
	{ 6,	0,	printargs,		"SYS_262"	}, /* 262 */
	{ 6,	0,	printargs,		"SYS_263"	}, /* 263 */
	{ 6,	0,	printargs,		"SYS_264"	}, /* 264 */
	{ 6,	0,	printargs,		"SYS_265"	}, /* 265 */
	{ 6,	0,	printargs,		"SYS_266"	}, /* 266 */
	{ 6,	0,	printargs,		"SYS_267"	}, /* 267 */
	{ 6,	0,	printargs,		"SYS_268"	}, /* 268 */
	{ 6,	0,	printargs,		"SYS_269"	}, /* 269 */
	{ 6,	0,	printargs,		"SYS_270"	}, /* 270 */
	{ 6,	0,	printargs,		"SYS_271"	}, /* 271 */
	{ 6,	0,	printargs,		"SYS_272"	}, /* 272 */
	{ 6,	0,	printargs,		"SYS_273"	}, /* 273 */
	{ 6,	0,	printargs,		"SYS_274"	}, /* 274 */
	{ 6,	0,	printargs,		"SYS_275"	}, /* 275 */
	{ 6,	0,	printargs,		"SYS_276"	}, /* 276 */
	{ 6,	0,	printargs,		"SYS_277"	}, /* 277 */
	{ 6,	0,	printargs,		"SYS_278"	}, /* 278 */
	{ 6,	0,	printargs,		"SYS_279"	}, /* 279 */
	{ 6,	0,	printargs,		"SYS_280"	}, /* 280 */
	{ 6,	0,	printargs,		"SYS_281"	}, /* 281 */
	{ 6,	0,	printargs,		"SYS_282"	}, /* 282 */
	{ 6,	0,	printargs,		"SYS_283"	}, /* 283 */
	{ 6,	0,	printargs,		"SYS_284"	}, /* 284 */
	{ 6,	0,	printargs,		"SYS_285"	}, /* 285 */
	{ 6,	0,	printargs,		"SYS_286"	}, /* 286 */
	{ 6,	0,	printargs,		"SYS_287"	}, /* 287 */
	{ 6,	0,	printargs,		"SYS_288"	}, /* 288 */
	{ 6,	0,	printargs,		"SYS_289"	}, /* 289 */
	{ 6,	0,	printargs,		"SYS_290"	}, /* 290 */
	{ 6,	0,	printargs,		"SYS_291"	}, /* 291 */
	{ 6,	0,	printargs,		"SYS_292"	}, /* 292 */
	{ 6,	0,	printargs,		"SYS_293"	}, /* 293 */
	{ 6,	0,	printargs,		"SYS_294"	}, /* 294 */
	{ 6,	0,	printargs,		"SYS_295"	}, /* 295 */
	{ 6,	0,	printargs,		"SYS_296"	}, /* 296 */
	{ 6,	0,	printargs,		"SYS_297"	}, /* 297 */
	{ 6,	0,	printargs,		"SYS_298"	}, /* 298 */
	{ 6,	0,	printargs,		"SYS_299"	}, /* 299 */

	{ 6,	0,	solaris_getpgrp,	"getpgrp"	}, /* 300 */
	{ 6,	0,	solaris_setpgrp,	"setpgrp"	}, /* 301 */
	{ 6,	0,	solaris_getsid,		"getsid"	}, /* 302 */
	{ 6,	0,	solaris_setsid,		"setsid"	}, /* 303 */
	{ 6,	0,	solaris_getpgid,	"getpgid"	}, /* 304 */
	{ 6,	0,	solaris_setpgid,	"setpgid"	}, /* 305 */
	{ 6,	0,	printargs,		"SYS_306"	}, /* 306 */
	{ 6,	0,	printargs,		"SYS_307"	}, /* 307 */
	{ 6,	0,	printargs,		"SYS_308"	}, /* 308 */
	{ 6,	0,	printargs,		"SYS_309"	}, /* 309 */

	{ 6,	TS,	solaris_signal,		"signal"	}, /* 310 */
	{ 6,	TS,	solaris_sigset,		"sigset"	}, /* 311 */
	{ 6,	TS,	solaris_sighold,	"sighold"	}, /* 312 */
	{ 6,	TS,	solaris_sigrelse,	"sigrelse"	}, /* 313 */
	{ 6,	TS,	solaris_sigignore,	"sigignore"	}, /* 314 */
	{ 6,	TS,	solaris_sigpause,	"sigpause"	}, /* 315 */
	{ 6,	0,	printargs,		"SYS_316"	}, /* 316 */
	{ 6,	0,	printargs,		"SYS_317"	}, /* 317 */
	{ 6,	0,	printargs,		"SYS_318"	}, /* 318 */
	{ 6,	0,	printargs,		"SYS_319"	}, /* 319 */

	{ 6,	TI,	solaris_msgget,		"msgget"	}, /* 320 */
	{ 6,	TI,	solaris_msgctl,		"msgctl"	}, /* 321 */
	{ 6,	TI,	solaris_msgrcv,		"msgrcv"	}, /* 322 */
	{ 6,	TI,	solaris_msgsnd,		"msgsnd"	}, /* 323 */
	{ 6,	0,	printargs,		"SYS_324"	}, /* 324 */
	{ 6,	0,	printargs,		"SYS_325"	}, /* 325 */
	{ 6,	0,	printargs,		"SYS_326"	}, /* 326 */
	{ 6,	0,	printargs,		"SYS_327"	}, /* 327 */
	{ 6,	0,	printargs,		"SYS_328"	}, /* 328 */
	{ 6,	0,	printargs,		"SYS_329"	}, /* 329 */

	{ 6,	TI,	solaris_shmat,		"shmat"		}, /* 330 */
	{ 6,	TI,	solaris_shmctl,		"shmctl"	}, /* 331 */
	{ 6,	TI,	solaris_shmdt,		"shmdt"		}, /* 332 */
	{ 6,	TI,	solaris_shmget,		"shmget"	}, /* 333 */
	{ 6,	0,	printargs,		"SYS_334"	}, /* 334 */
	{ 6,	0,	printargs,		"SYS_335"	}, /* 335 */
	{ 6,	0,	printargs,		"SYS_336"	}, /* 336 */
	{ 6,	0,	printargs,		"SYS_337"	}, /* 337 */
	{ 6,	0,	printargs,		"SYS_338"	}, /* 338 */
	{ 6,	0,	printargs,		"SYS_339"	}, /* 339 */

	{ 6,	TI,	solaris_semctl,		"semctl"	}, /* 340 */
	{ 6,	TI,	solaris_semget,		"semget"	}, /* 341 */
	{ 6,	TI,	solaris_semop,		"semop"		}, /* 342 */
	{ 6,	0,	printargs,		"SYS_343"	}, /* 343 */
	{ 6,	0,	printargs,		"SYS_344"	}, /* 344 */
	{ 6,	0,	printargs,		"SYS_345"	}, /* 345 */
	{ 6,	0,	printargs,		"SYS_346"	}, /* 346 */
	{ 6,	0,	printargs,		"SYS_347"	}, /* 347 */
	{ 6,	0,	printargs,		"SYS_348"	}, /* 348 */
	{ 6,	0,	printargs,		"SYS_349"	}, /* 349 */

	{ 6,	0,	solaris_olduname,	"olduname"	}, /* 350 */
	{ 6,	0,	printargs,		"utssys1"	}, /* 351 */
	{ 6,	0,	solaris_ustat,		"ustat"		}, /* 352 */
	{ 6,	0,	solaris_fusers,		"fusers"	}, /* 353 */
	{ 6,	0,	printargs,		"SYS_354"	}, /* 354 */
	{ 6,	0,	printargs,		"SYS_355"	}, /* 355 */
	{ 6,	0,	printargs,		"SYS_356"	}, /* 356 */
	{ 6,	0,	printargs,		"SYS_357"	}, /* 357 */
	{ 6,	0,	printargs,		"SYS_358"	}, /* 358 */
	{ 6,	0,	printargs,		"SYS_359"	}, /* 359 */

	{ 6,	0,	printargs,		"sysfs0"	}, /* 360 */
	{ 6,	0,	solaris_sysfs1,		"sysfs1"	}, /* 361 */
	{ 6,	0,	solaris_sysfs2,		"sysfs2"	}, /* 362 */
	{ 6,	0,	solaris_sysfs3,		"sysfs3"	}, /* 363 */
	{ 6,	0,	printargs,		"SYS_364"	}, /* 364 */
	{ 6,	0,	printargs,		"SYS_365"	}, /* 365 */
	{ 6,	0,	printargs,		"SYS_366"	}, /* 366 */
	{ 6,	0,	printargs,		"SYS_367"	}, /* 367 */
	{ 6,	0,	printargs,		"SYS_368"	}, /* 368 */
	{ 6,	0,	printargs,		"SYS_369"	}, /* 369 */

	{ 6,	0,	printargs,		"spcall0"	}, /* 370 */
	{ 6,	TS,	solaris_sigpending,	"sigpending"	}, /* 371 */
	{ 6,	TS,	solaris_sigfillset,	"sigfillset"	}, /* 372 */
	{ 6,	0,	printargs,		"SYS_373"	}, /* 373 */
	{ 6,	0,	printargs,		"SYS_374"	}, /* 374 */
	{ 6,	0,	printargs,		"SYS_375"	}, /* 375 */
	{ 6,	0,	printargs,		"SYS_376"	}, /* 376 */
	{ 6,	0,	printargs,		"SYS_377"	}, /* 377 */
	{ 6,	0,	printargs,		"SYS_378"	}, /* 378 */
	{ 6,	0,	printargs,		"SYS_379"	}, /* 379 */

	{ 6,	0,	solaris_getcontext,	"getcontext"	}, /* 380 */
	{ 6,	0,	solaris_setcontext,	"setcontext"	}, /* 381 */
	{ 6,	0,	printargs,		"SYS_382"	}, /* 382 */
	{ 6,	0,	printargs,		"SYS_383"	}, /* 383 */
	{ 6,	0,	printargs,		"SYS_384"	}, /* 384 */
	{ 6,	0,	printargs,		"SYS_385"	}, /* 385 */
	{ 6,	0,	printargs,		"SYS_386"	}, /* 386 */
	{ 6,	0,	printargs,		"SYS_387"	}, /* 387 */
	{ 6,	0,	printargs,		"SYS_388"	}, /* 388 */
	{ 6,	0,	printargs,		"SYS_389"	}, /* 389 */

	{ 6,	0,	printargs,		"SYS_390"	}, /* 390 */
	{ 6,	0,	printargs,		"SYS_391"	}, /* 391 */
	{ 6,	0,	printargs,		"SYS_392"	}, /* 392 */
	{ 6,	0,	printargs,		"SYS_393"	}, /* 393 */
	{ 6,	0,	printargs,		"SYS_394"	}, /* 394 */
	{ 6,	0,	printargs,		"SYS_395"	}, /* 395 */
	{ 6,	0,	printargs,		"SYS_396"	}, /* 396 */
	{ 6,	0,	printargs,		"SYS_397"	}, /* 397 */
	{ 6,	0,	printargs,		"SYS_398"	}, /* 398 */
	{ 6,	0,	printargs,		"SYS_399"	}, /* 399 */

/* vi: set sw=4 ts=4: */
/*
 * ash shell port for busybox
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Original BSD copyright notice is retained at the end of this file.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/*
 * The following should be set to reflect the type of system you have:
 *      JOBS -> 1 if you have Berkeley job control, 0 otherwise.
 *      define SYSV if you are running under System V.
 *      define DEBUG=1 to compile in debugging ('set -o debug' to turn on)
 *      define DEBUG=2 to compile in and turn on debugging.
 *
 * When debugging is on, debugging info will be written to ./trace and
 * a quit signal will generate a core dump.
 */
#define DEBUG 0
/* Tweak debug output verbosity here */
#define DEBUG_TIME 0
#define DEBUG_PID 1
#define DEBUG_SIG 1

#define PROFILE 0

#define JOBS ENABLE_ASH_JOB_CONTROL

#include <paths.h>
#include <setjmp.h>
#include <fnmatch.h>
#include <sys/times.h>

#include "busybox.h" /* for applet_names */
#include "unicode.h"

#include "shell_common.h"
#if ENABLE_SH_MATH_SUPPORT
# include "math.h"
#endif
#if ENABLE_ASH_RANDOM_SUPPORT
# include "random.h"
#else
# define CLEAR_RANDOM_T(rnd) ((void)0)
#endif

#include "NUM_APPLETS.h"
#if NUM_APPLETS == 1
/* STANDALONE does not make sense, and won't compile */
# undef CONFIG_FEATURE_SH_STANDALONE
# undef ENABLE_FEATURE_SH_STANDALONE
# undef IF_FEATURE_SH_STANDALONE
# undef IF_NOT_FEATURE_SH_STANDALONE
# define ENABLE_FEATURE_SH_STANDALONE 0
# define IF_FEATURE_SH_STANDALONE(...)
# define IF_NOT_FEATURE_SH_STANDALONE(...) __VA_ARGS__
#endif

#ifndef PIPE_BUF
# define PIPE_BUF 4096           /* amount of buffering in a pipe */
#endif

#if !BB_MMU
# error "Do not even bother, ash will not run on NOMMU machine"
#endif

//config:config ASH
//config:	bool "ash"
//config:	default y
//config:	depends on !NOMMU
//config:	help
//config:	  Tha 'ash' shell adds about 60k in the default configuration and is
//config:	  the most complete and most pedantically correct shell included with
//config:	  busybox. This shell is actually a derivative of the Debian 'dash'
//config:	  shell (by Herbert Xu), which was created by porting the 'ash' shell
//config:	  (written by Kenneth Almquist) from NetBSD.
//config:
//config:config ASH_BASH_COMPAT
//config:	bool "bash-compatible extensions"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable bash-compatible extensions.
//config:
//config:config ASH_IDLE_TIMEOUT
//config:	bool "Idle timeout variable"
//config:	default n
//config:	depends on ASH
//config:	help
//config:	  Enables bash-like auto-logout after $TMOUT seconds of idle time.
//config:
//config:config ASH_JOB_CONTROL
//config:	bool "Job control"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable job control in the ash shell.
//config:
//config:config ASH_ALIAS
//config:	bool "Alias support"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable alias support in the ash shell.
//config:
//config:config ASH_GETOPTS
//config:	bool "Builtin getopt to parse positional parameters"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable support for getopts builtin in ash.
//config:
//config:config ASH_BUILTIN_ECHO
//config:	bool "Builtin version of 'echo'"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable support for echo builtin in ash.
//config:
//config:config ASH_BUILTIN_PRINTF
//config:	bool "Builtin version of 'printf'"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable support for printf builtin in ash.
//config:
//config:config ASH_BUILTIN_TEST
//config:	bool "Builtin version of 'test'"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable support for test builtin in ash.
//config:
//config:config ASH_CMDCMD
//config:	bool "'command' command to override shell builtins"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable support for the ash 'command' builtin, which allows
//config:	  you to run the specified command with the specified arguments,
//config:	  even when there is an ash builtin command with the same name.
//config:
//config:config ASH_MAIL
//config:	bool "Check for new mail on interactive shells"
//config:	default n
//config:	depends on ASH
//config:	help
//config:	  Enable "check for new mail" function in the ash shell.
//config:
//config:config ASH_OPTIMIZE_FOR_SIZE
//config:	bool "Optimize for size instead of speed"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Compile ash for reduced size at the price of speed.
//config:
//config:config ASH_RANDOM_SUPPORT
//config:	bool "Pseudorandom generator and $RANDOM variable"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  Enable pseudorandom generator and dynamic variable "$RANDOM".
//config:	  Each read of "$RANDOM" will generate a new pseudorandom value.
//config:	  You can reset the generator by using a specified start value.
//config:	  After "unset RANDOM" the generator will switch off and this
//config:	  variable will no longer have special treatment.
//config:
//config:config ASH_EXPAND_PRMT
//config:	bool "Expand prompt string"
//config:	default y
//config:	depends on ASH
//config:	help
//config:	  "PS#" may contain volatile content, such as backquote commands.
//config:	  This option recreates the prompt string from the environment
//config:	  variable each time it is displayed.
//config:

//applet:IF_ASH(APPLET(ash, BB_DIR_BIN, BB_SUID_DROP))
//applet:IF_FEATURE_SH_IS_ASH(APPLET_ODDNAME(sh, ash, BB_DIR_BIN, BB_SUID_DROP, sh))
//applet:IF_FEATURE_BASH_IS_ASH(APPLET_ODDNAME(bash, ash, BB_DIR_BIN, BB_SUID_DROP, bash))

//kbuild:lib-$(CONFIG_ASH) += ash.o ash_ptr_hack.o shell_common.o
//kbuild:lib-$(CONFIG_ASH_RANDOM_SUPPORT) += random.o


/* ============ Hash table sizes. Configurable. */

#define VTABSIZE 39
#define ATABSIZE 39
#define CMDTABLESIZE 31         /* should be prime */


/* ============ Shell options */

static const char *const optletters_optnames[] = {
	"e"   "errexit",
	"f"   "noglob",
	"I"   "ignoreeof",
	"i"   "interactive",
	"m"   "monitor",
	"n"   "noexec",
	"s"   "stdin",
	"x"   "xtrace",
	"v"   "verbose",
	"C"   "noclobber",
	"a"   "allexport",
	"b"   "notify",
	"u"   "nounset",
	"\0"  "vi"
#if ENABLE_ASH_BASH_COMPAT
	,"\0"  "pipefail"
#endif
#if DEBUG
	,"\0"  "nolog"
	,"\0"  "debug"
#endif
};

#define optletters(n)  optletters_optnames[n][0]
#define optnames(n)   (optletters_optnames[n] + 1)

enum { NOPTS = ARRAY_SIZE(optletters_optnames) };


/* ============ Misc data */

#define msg_illnum "Illegal number: %s"

/*
 * We enclose jmp_buf in a structure so that we can declare pointers to
 * jump locations.  The global variable handler contains the location to
 * jump to when an exception occurs, and the global variable exception_type
 * contains a code identifying the exception.  To implement nested
 * exception handlers, the user should save the value of handler on entry
 * to an inner scope, set handler to point to a jmploc structure for the
 * inner scope, and restore handler on exit from the scope.
 */
struct jmploc {
	jmp_buf loc;
};

struct globals_misc {
	/* pid of main shell */
	int rootpid;
	/* shell level: 0 for the main shell, 1 for its children, and so on */
	int shlvl;
#define rootshell (!shlvl)
	char *minusc;  /* argument to -c option */

	char *curdir; // = nullstr;     /* current working directory */
	char *physdir; // = nullstr;    /* physical working directory */

	char *arg0; /* value of $0 */

	struct jmploc *exception_handler;

	volatile int suppress_int; /* counter */
	volatile /*sig_atomic_t*/ smallint pending_int; /* 1 = got SIGINT */
	/* last pending signal */
	volatile /*sig_atomic_t*/ smallint pending_sig;
	smallint exception_type; /* kind of exception (0..5) */
	/* exceptions */
#define EXINT 0         /* SIGINT received */
#define EXERROR 1       /* a generic error */
#define EXSHELLPROC 2   /* execute a shell procedure */
#define EXEXEC 3        /* command execution failed */
#define EXEXIT 4        /* exit the shell */
#define EXSIG 5         /* trapped signal in wait(1) */

	smallint isloginsh;
	char nullstr[1];        /* zero length string */

	char optlist[NOPTS];
#define eflag optlist[0]
#define fflag optlist[1]
#define Iflag optlist[2]
#define iflag optlist[3]
#define mflag optlist[4]
#define nflag optlist[5]
#define sflag optlist[6]
#define xflag optlist[7]
#define vflag optlist[8]
#define Cflag optlist[9]
#define aflag optlist[10]
#define bflag optlist[11]
#define uflag optlist[12]
#define viflag optlist[13]
#if ENABLE_ASH_BASH_COMPAT
# define pipefail optlist[14]
#else
# define pipefail 0
#endif
#if DEBUG
# define nolog optlist[14 + ENABLE_ASH_BASH_COMPAT]
# define debug optlist[15 + ENABLE_ASH_BASH_COMPAT]
#endif

	/* trap handler commands */
	/*
	 * Sigmode records the current value of the signal handlers for the various
	 * modes.  A value of zero means that the current handler is not known.
	 * S_HARD_IGN indicates that the signal was ignored on entry to the shell.
	 */
	char sigmode[NSIG - 1];
#define S_DFL      1            /* default signal handling (SIG_DFL) */
#define S_CATCH    2            /* signal is caught */
#define S_IGN      3            /* signal is ignored (SIG_IGN) */
#define S_HARD_IGN 4            /* signal is ignored permenantly */

	/* indicates specified signal received */
	uint8_t gotsig[NSIG - 1]; /* offset by 1: "signal" 0 is meaningless */
	uint8_t may_have_traps; /* 0: definitely no traps are set, 1: some traps may be set */
	char *trap[NSIG];
	char **trap_ptr;        /* used only by "trap hack" */

	/* Rarely referenced stuff */
#if ENABLE_ASH_RANDOM_SUPPORT
	random_t random_gen;
#endif
	pid_t backgndpid;        /* pid of last background process */
	smallint job_warning;    /* user was warned about stopped jobs (can be 2, 1 or 0). */
};
extern struct globals_misc *const ash_ptr_to_globals_misc;
#define G_misc (*ash_ptr_to_globals_misc)
#define rootpid     (G_misc.rootpid    )
#define shlvl       (G_misc.shlvl      )
#define minusc      (G_misc.minusc     )
#define curdir      (G_misc.curdir     )
#define physdir     (G_misc.physdir    )
#define arg0        (G_misc.arg0       )
#define exception_handler (G_misc.exception_handler)
#define exception_type    (G_misc.exception_type   )
#define suppress_int      (G_misc.suppress_int     )
#define pending_int       (G_misc.pending_int      )
#define pending_sig       (G_misc.pending_sig      )
#define isloginsh   (G_misc.isloginsh  )
#define nullstr     (G_misc.nullstr    )
#define optlist     (G_misc.optlist    )
#define sigmode     (G_misc.sigmode    )
#define gotsig      (G_misc.gotsig     )
#define may_have_traps    (G_misc.may_have_traps   )
#define trap        (G_misc.trap       )
#define trap_ptr    (G_misc.trap_ptr   )
#define random_gen  (G_misc.random_gen )
#define backgndpid  (G_misc.backgndpid )
#define job_warning (G_misc.job_warning)
#define INIT_G_misc() do { \
	(*(struct globals_misc**)&ash_ptr_to_globals_misc) = xzalloc(sizeof(G_misc)); \
	barrier(); \
	curdir = nullstr; \
	physdir = nullstr; \
	trap_ptr = trap; \
} while (0)


/* ============ DEBUG */
#if DEBUG
static void trace_printf(const char *fmt, ...);
static void trace_vprintf(const char *fmt, va_list va);
# define TRACE(param)    trace_printf param
# define TRACEV(param)   trace_vprintf param
# define close(fd) do { \
	int dfd = (fd); \
	if (close(dfd) < 0) \
		bb_error_msg("bug on %d: closing %d(0x%x)", \
			__LINE__, dfd, dfd); \
} while (0)
#else
# define TRACE(param)
# define TRACEV(param)
#endif


/* ============ Utility functions */
#define xbarrier() do { __asm__ __volatile__ ("": : :"memory"); } while (0)

static int isdigit_str9(const char *str)
{
	int maxlen = 9 + 1; /* max 9 digits: 999999999 */
	while (--maxlen && isdigit(*str))
		str++;
	return (*str == '\0');
}

static const char *var_end(const char *var)
{
	while (*var)
		if (*var++ == '=')
			break;
	return var;
}


/* ============ Interrupts / exceptions */

static void exitshell(void) NORETURN;

/*
 * These macros allow the user to suspend the handling of interrupt signals
 * over a period of time.  This is similar to SIGHOLD or to sigblock, but
 * much more efficient and portable.  (But hacking the kernel is so much
 * more fun than worrying about efficiency and portability. :-))
 */
#define INT_OFF do { \
	suppress_int++; \
	xbarrier(); \
} while (0)

/*
 * Called to raise an exception.  Since C doesn't include exceptions, we
 * just do a longjmp to the exception handler.  The type of exception is
 * stored in the global variable "exception_type".
 */
static void raise_exception(int) NORETURN;
static void
raise_exception(int e)
{
#if DEBUG
	if (exception_handler == NULL)
		abort();
#endif
	INT_OFF;
	exception_type = e;
	longjmp(exception_handler->loc, 1);
}
#if DEBUG
#define raise_exception(e) do { \
	TRACE(("raising exception %d on line %d\n", (e), __LINE__)); \
	raise_exception(e); \
} while (0)
#endif

/*
 * Called from trap.c when a SIGINT is received.  (If the user specifies
 * that SIGINT is to be trapped or ignored using the trap builtin, then
 * this routine is not called.)  Suppressint is nonzero when interrupts
 * are held using the INT_OFF macro.  (The test for iflag is just
 * defensive programming.)
 */
static void raise_interrupt(void) NORETURN;
static void
raise_interrupt(void)
{
	int ex_type;

	pending_int = 0;
	/* Signal is not automatically unmasked after it is raised,
	 * do it ourself - unmask all signals */
	sigprocmask_allsigs(SIG_UNBLOCK);
	/* pending_sig = 0; - now done in signal_handler() */

	ex_type = EXSIG;
	if (gotsig[SIGINT - 1] && !trap[SIGINT]) {
		if (!(rootshell && iflag)) {
			/* Kill ourself with SIGINT */
			signal(SIGINT, SIG_DFL);
			raise(SIGINT);
		}
		ex_type = EXINT;
	}
	raise_exception(ex_type);
	/* NOTREACHED */
}
#if DEBUG
#define raise_interrupt() do { \
	TRACE(("raising interrupt on line %d\n", __LINE__)); \
	raise_interrupt(); \
} while (0)
#endif

static IF_ASH_OPTIMIZE_FOR_SIZE(inline) void
int_on(void)
{
	xbarrier();
	if (--suppress_int == 0 && pending_int) {
		raise_interrupt();
	}
}
#define INT_ON int_on()
static IF_ASH_OPTIMIZE_FOR_SIZE(inline) void
force_int_on(void)
{
	xbarrier();
	suppress_int = 0;
	if (pending_int)
		raise_interrupt();
}
#define FORCE_INT_ON force_int_on()

#define SAVE_INT(v) ((v) = suppress_int)

#define RESTORE_INT(v) do { \
	xbarrier(); \
	suppress_int = (v); \
	if (suppress_int == 0 && pending_int) \
		raise_interrupt(); \
} while (0)


/* ============ Stdout/stderr output */

static void
outstr(const char *p, FILE *file)
{
	INT_OFF;
	fputs(p, file);
	INT_ON;
}

static void
flush_stdout_stderr(void)
{
	INT_OFF;
	fflush_all();
	INT_ON;
}

static void
outcslow(int c, FILE *dest)
{
	INT_OFF;
	putc(c, dest);
	fflush(dest);
	INT_ON;
}

static int out1fmt(const char *, ...) __attribute__((__format__(__printf__,1,2)));
static int
out1fmt(const char *fmt, ...)
{
	va_list ap;
	int r;

	INT_OFF;
	va_start(ap, fmt);
	r = vprintf(fmt, ap);
	va_end(ap);
	INT_ON;
	return r;
}

static int fmtstr(char *, size_t, const char *, ...) __attribute__((__format__(__printf__,3,4)));
static int
fmtstr(char *outbuf, size_t length, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	INT_OFF;
	ret = vsnprintf(outbuf, length, fmt, ap);
	va_end(ap);
	INT_ON;
	return ret;
}

static void
out1str(const char *p)
{
	outstr(p, stdout);
}

static void
out2str(const char *p)
{
	outstr(p, stderr);
	flush_stdout_stderr();
}


/* ============ Parser structures */

/* control characters in argument strings */
#define CTL_FIRST CTLESC
#define CTLESC       ((unsigned char)'\201')    /* escape next character */
#define CTLVAR       ((unsigned char)'\202')    /* variable defn */
#define CTLENDVAR    ((unsigned char)'\203')
#define CTLBACKQ     ((unsigned char)'\204')
#define CTLQUOTE 01             /* ored with CTLBACKQ code if in quotes */
/*      CTLBACKQ | CTLQUOTE == '\205' */
#define CTLARI       ((unsigned char)'\206')    /* arithmetic expression */
#define CTLENDARI    ((unsigned char)'\207')
#define CTLQUOTEMARK ((unsigned char)'\210')
#define CTL_LAST CTLQUOTEMARK

/* variable substitution byte (follows CTLVAR) */
#define VSTYPE  0x0f            /* type of variable substitution */
#define VSNUL   0x10            /* colon--treat the empty string as unset */
#define VSQUOTE 0x80            /* inside double quotes--suppress splitting */

/* values of VSTYPE field */
#define VSNORMAL        0x1     /* normal variable:  $var or ${var} */
#define VSMINUS         0x2     /* ${var-text} */
#define VSPLUS          0x3     /* ${var+text} */
#define VSQUESTION      0x4     /* ${var?message} */
#define VSASSIGN        0x5     /* ${var=text} */
#define VSTRIMRIGHT     0x6     /* ${var%pattern} */
#define VSTRIMRIGHTMAX  0x7     /* ${var%%pattern} */
#define VSTRIMLEFT      0x8     /* ${var#pattern} */
#define VSTRIMLEFTMAX   0x9     /* ${var##pattern} */
#define VSLENGTH        0xa     /* ${#var} */
#if ENABLE_ASH_BASH_COMPAT
#define VSSUBSTR        0xc     /* ${var:position:length} */
#define VSREPLACE       0xd     /* ${var/pattern/replacement} */
#define VSREPLACEALL    0xe     /* ${var//pattern/replacement} */
#endif

static const char dolatstr[] ALIGN1 = {
	CTLVAR, VSNORMAL|VSQUOTE, '@', '=', '\0'
};

#define NCMD      0
#define NPIPE     1
#define NREDIR    2
#define NBACKGND  3
#define NSUBSHELL 4
#define NAND      5
#define NOR       6
#define NSEMI     7
#define NIF       8
#define NWHILE    9
#define NUNTIL   10
#define NFOR     11
#define NCASE    12
#define NCLIST   13
#define NDEFUN   14
#define NARG     15
#define NTO      16
#if ENABLE_ASH_BASH_COMPAT
#define NTO2     17
#endif
#define NCLOBBER 18
#define NFROM    19
#define NFROMTO  20
#define NAPPEND  21
#define NTOFD    22
#define NFROMFD  23
#define NHERE    24
#define NXHERE   25
#define NNOT     26
#define N_NUMBER 27

union node;

struct ncmd {
	smallint type; /* Nxxxx */
	union node *assign;
	union node *args;
	union node *redirect;
};

struct npipe {
	smallint type;
	smallint pipe_backgnd;
	struct nodelist *cmdlist;
};

struct nredir {
	smallint type;
	union node *n;
	union node *redirect;
};

struct nbinary {
	smallint type;
	union node *ch1;
	union node *ch2;
};

struct nif {
	smallint type;
	union node *test;
	union node *ifpart;
	union node *elsepart;
};

struct nfor {
	smallint type;
	union node *args;
	union node *body;
	char *var;
};

struct ncase {
	smallint type;
	union node *expr;
	union node *cases;
};

struct nclist {
	smallint type;
	union node *next;
	union node *pattern;
	union node *body;
};

struct narg {
	smallint type;
	union node *next;
	char *text;
	struct nodelist *backquote;
};

/* nfile and ndup layout must match!
 * NTOFD (>&fdnum) uses ndup structure, but we may discover mid-flight
 * that it is actually NTO2 (>&file), and change its type.
 */
struct nfile {
	smallint type;
	union node *next;
	int fd;
	int _unused_dupfd;
	union node *fname;
	char *expfname;
};

struct ndup {
	smallint type;
	union node *next;
	int fd;
	int dupfd;
	union node *vname;
	char *_unused_expfname;
};

struct nhere {
	smallint type;
	union node *next;
	int fd;
	union node *doc;
};

struct nnot {
	smallint type;
	union node *com;
};

union node {
	smallint type;
	struct ncmd ncmd;
	struct npipe npipe;
	struct nredir nredir;
	struct nbinary nbinary;
	struct nif nif;
	struct nfor nfor;
	struct ncase ncase;
	struct nclist nclist;
	struct narg narg;
	struct nfile nfile;
	struct ndup ndup;
	struct nhere nhere;
	struct nnot nnot;
};

/*
 * NODE_EOF is returned by parsecmd when it encounters an end of file.
 * It must be distinct from NULL.
 */
#define NODE_EOF ((union node *) -1L)

struct nodelist {
	struct nodelist *next;
	union node *n;
};

struct funcnode {
	int count;
	union node n;
};

/*
 * Free a parse tree.
 */
static void
freefunc(struct funcnode *f)
{
	if (f && --f->count < 0)
		free(f);
}


/* ============ Debugging output */

#if DEBUG

static FILE *tracefile;

static void
trace_printf(const char *fmt, ...)
{
	va_list va;

	if (debug != 1)
		return;
	if (DEBUG_TIME)
		fprintf(tracefile, "%u ", (int) time(NULL));
	if (DEBUG_PID)
		fprintf(tracefile, "[%u] ", (int) getpid());
	if (DEBUG_SIG)
		fprintf(tracefile, "pending s:%d i:%d(supp:%d) ", pending_sig, pending_int, suppress_int);
	va_start(va, fmt);
	vfprintf(tracefile, fmt, va);
	va_end(va);
}

static void
trace_vprintf(const char *fmt, va_list va)
{
	if (debug != 1)
		return;
	if (DEBUG_TIME)
		fprintf(tracefile, "%u ", (int) time(NULL));
	if (DEBUG_PID)
		fprintf(tracefile, "[%u] ", (int) getpid());
	if (DEBUG_SIG)
		fprintf(tracefile, "pending s:%d i:%d(supp:%d) ", pending_sig, pending_int, suppress_int);
	vfprintf(tracefile, fmt, va);
}

static void
trace_puts(const char *s)
{
	if (debug != 1)
		return;
	fputs(s, tracefile);
}

static void
trace_puts_quoted(char *s)
{
	char *p;
	char c;

	if (debug != 1)
		return;
	putc('"', tracefile);
	for (p = s; *p; p++) {
		switch ((unsigned char)*p) {
		case '\n': c = 'n'; goto backslash;
		case '\t': c = 't'; goto backslash;
		case '\r': c = 'r'; goto backslash;
		case '\"': c = '\"'; goto backslash;
		case '\\': c = '\\'; goto backslash;
		case CTLESC: c = 'e'; goto backslash;
		case CTLVAR: c = 'v'; goto backslash;
		case CTLVAR+CTLQUOTE: c = 'V'; goto backslash;
		case CTLBACKQ: c = 'q'; goto backslash;
		case CTLBACKQ+CTLQUOTE: c = 'Q'; goto backslash;
 backslash:
			putc('\\', tracefile);
			putc(c, tracefile);
			break;
		default:
			if (*p >= ' ' && *p <= '~')
				putc(*p, tracefile);
			else {
				putc('\\', tracefile);
				putc((*p >> 6) & 03, tracefile);
				putc((*p >> 3) & 07, tracefile);
				putc(*p & 07, tracefile);
			}
			break;
		}
	}
	putc('"', tracefile);
}

static void
trace_puts_args(char **ap)
{
	if (debug != 1)
		return;
	if (!*ap)
		return;
	while (1) {
		trace_puts_quoted(*ap);
		if (!*++ap) {
			putc('\n', tracefile);
			break;
		}
		putc(' ', tracefile);
	}
}

static void
opentrace(void)
{
	char s[100];
#ifdef O_APPEND
	int flags;
#endif

	if (debug != 1) {
		if (tracefile)
			fflush(tracefile);
		/* leave open because libedit might be using it */
		return;
	}
	strcpy(s, "./trace");
	if (tracefile) {
		if (!freopen(s, "a", tracefile)) {
			fprintf(stderr, "Can't re-open %s\n", s);
			debug = 0;
			return;
		}
	} else {
		tracefile = fopen(s, "a");
		if (tracefile == NULL) {
			fprintf(stderr, "Can't open %s\n", s);
			debug = 0;
			return;
		}
	}
#ifdef O_APPEND
	flags = fcntl(fileno(tracefile), F_GETFL);
	if (flags >= 0)
		fcntl(fileno(tracefile), F_SETFL, flags | O_APPEND);
#endif
	setlinebuf(tracefile);
	fputs("\nTracing started.\n", tracefile);
}

static void
indent(int amount, char *pfx, FILE *fp)
{
	int i;

	for (i = 0; i < amount; i++) {
		if (pfx && i == amount - 1)
			fputs(pfx, fp);
		putc('\t', fp);
	}
}

/* little circular references here... */
static void shtree(union node *n, int ind, char *pfx, FILE *fp);

static void
sharg(union node *arg, FILE *fp)
{
	char *p;
	struct nodelist *bqlist;
	unsigned char subtype;

	if (arg->type != NARG) {
		out1fmt("<node type %d>\n", arg->type);
		abort();
	}
	bqlist = arg->narg.backquote;
	for (p = arg->narg.text; *p; p++) {
		switch ((unsigned char)*p) {
		case CTLESC:
			p++;
			putc(*p, fp);
			break;
		case CTLVAR:
			putc('$', fp);
			putc('{', fp);
			subtype = *++p;
			if (subtype == VSLENGTH)
				putc('#', fp);

			while (*p != '=') {
				putc(*p, fp);
				p++;
			}

			if (subtype & VSNUL)
				putc(':', fp);

			switch (subtype & VSTYPE) {
			case VSNORMAL:
				putc('}', fp);
				break;
			case VSMINUS:
				putc('-', fp);
				break;
			case VSPLUS:
				putc('+', fp);
				break;
			case VSQUESTION:
				putc('?', fp);
				break;
			case VSASSIGN:
				putc('=', fp);
				break;
			case VSTRIMLEFT:
				putc('#', fp);
				break;
			case VSTRIMLEFTMAX:
				putc('#', fp);
				putc('#', fp);
				break;
			case VSTRIMRIGHT:
				putc('%', fp);
				break;
			case VSTRIMRIGHTMAX:
				putc('%', fp);
				putc('%', fp);
				break;
			case VSLENGTH:
				break;
			default:
				out1fmt("<subtype %d>", subtype);
			}
			break;
		case CTLENDVAR:
			putc('}', fp);
			break;
		case CTLBACKQ:
		case CTLBACKQ|CTLQUOTE:
			putc('$', fp);
			putc('(', fp);
			shtree(bqlist->n, -1, NULL, fp);
			putc(')', fp);
			break;
		default:
			putc(*p, fp);
			break;
		}
	}
}

static void
shcmd(union node *cmd, FILE *fp)
{
	union node *np;
	int first;
	const char *s;
	int dftfd;

	first = 1;
	for (np = cmd->ncmd.args; np; np = np->narg.next) {
		if (!first)
			putc(' ', fp);
		sharg(np, fp);
		first = 0;
	}
	for (np = cmd->ncmd.redirect; np; np = np->nfile.next) {
		if (!first)
			putc(' ', fp);
		dftfd = 0;
		switch (np->nfile.type) {
		case NTO:      s = ">>"+1; dftfd = 1; break;
		case NCLOBBER: s = ">|"; dftfd = 1; break;
		case NAPPEND:  s = ">>"; dftfd = 1; break;
#if ENABLE_ASH_BASH_COMPAT
		case NTO2:
#endif
		case NTOFD:    s = ">&"; dftfd = 1; break;
		case NFROM:    s = "<"; break;
		case NFROMFD:  s = "<&"; break;
		case NFROMTO:  s = "<>"; break;
		default:       s = "*error*"; break;
		}
		if (np->nfile.fd != dftfd)
			fprintf(fp, "%d", np->nfile.fd);
		fputs(s, fp);
		if (np->nfile.type == NTOFD || np->nfile.type == NFROMFD) {
			fprintf(fp, "%d", np->ndup.dupfd);
		} else {
			sharg(np->nfile.fname, fp);
		}
		first = 0;
	}
}

static void
shtree(union node *n, int ind, char *pfx, FILE *fp)
{
	struct nodelist *lp;
	const char *s;

	if (n == NULL)
		return;

	indent(ind, pfx, fp);

	if (n == NODE_EOF) {
		fputs("<EOF>", fp);
		return;
	}

	switch (n->type) {
	case NSEMI:
		s = "; ";
		goto binop;
	case NAND:
		s = " && ";
		goto binop;
	case NOR:
		s = " || ";
 binop:
		shtree(n->nbinary.ch1, ind, NULL, fp);
		/* if (ind < 0) */
			fputs(s, fp);
		shtree(n->nbinary.ch2, ind, NULL, fp);
		break;
	case NCMD:
		shcmd(n, fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist; lp; lp = lp->next) {
			shtree(lp->n, 0, NULL, fp);
			if (lp->next)
				fputs(" | ", fp);
		}
		if (n->npipe.pipe_backgnd)
			fputs(" &", fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	default:
		fprintf(fp, "<node type %d>", n->type);
		if (ind >= 0)
			putc('\n', fp);
		break;
	}
}

static void
showtree(union node *n)
{
	trace_puts("showtree called\n");
	shtree(n, 1, NULL, stderr);
}

#endif /* DEBUG */


/* ============ Parser data */

/*
 * ash_vmsg() needs parsefile->fd, hence parsefile definition is moved up.
 */
struct strlist {
	struct strlist *next;
	char *text;
};

struct alias;

struct strpush {
	struct strpush *prev;   /* preceding string on stack */
	char *prev_string;
	int prev_left_in_line;
#if ENABLE_ASH_ALIAS
	struct alias *ap;       /* if push was associated with an alias */
#endif
	char *string;           /* remember the string since it may change */
};

struct parsefile {
	struct parsefile *prev; /* preceding file on stack */
	int linno;              /* current line */
	int pf_fd;              /* file descriptor (or -1 if string) */
	int left_in_line;       /* number of chars left in this line */
	int left_in_buffer;     /* number of chars left in this buffer past the line */
	char *next_to_pgetc;    /* next char in buffer */
	char *buf;              /* input buffer */
	struct strpush *strpush; /* for pushing strings at this level */
	struct strpush basestrpush; /* so pushing one is fast */
};

static struct parsefile basepf;        /* top level input file */
static struct parsefile *g_parsefile = &basepf;  /* current input file */
static int startlinno;                 /* line # where last token started */
static char *commandname;              /* currently executing command */
static struct strlist *cmdenviron;     /* environment for builtin command */
static uint8_t exitstatus;             /* exit status of last command */


/* ============ Message printing */

static void
ash_vmsg(const char *msg, va_list ap)
{
	fprintf(stderr, "%s: ", arg0);
	if (commandname) {
		if (strcmp(arg0, commandname))
			fprintf(stderr, "%s: ", commandname);
		if (!iflag || g_parsefile->pf_fd > 0)
			fprintf(stderr, "line %d: ", startlinno);
	}
	vfprintf(stderr, msg, ap);
	outcslow('\n', stderr);
}

/*
 * Exverror is called to raise the error exception.  If the second argument
 * is not NULL then error prints an error message using printf style
 * formatting.  It then raises the error exception.
 */
static void ash_vmsg_and_raise(int, const char *, va_list) NORETURN;
static void
ash_vmsg_and_raise(int cond, const char *msg, va_list ap)
{
#if DEBUG
	if (msg) {
		TRACE(("ash_vmsg_and_raise(%d, \"", cond));
		TRACEV((msg, ap));
		TRACE(("\") pid=%d\n", getpid()));
	} else
		TRACE(("ash_vmsg_and_raise(%d, NULL) pid=%d\n", cond, getpid()));
	if (msg)
#endif
		ash_vmsg(msg, ap);

	flush_stdout_stderr();
	raise_exception(cond);
	/* NOTREACHED */
}

static void ash_msg_and_raise_error(const char *, ...) NORETURN;
static void
ash_msg_and_raise_error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	ash_vmsg_and_raise(EXERROR, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}

static void raise_error_syntax(const char *) NORETURN;
static void
raise_error_syntax(const char *msg)
{
	ash_msg_and_raise_error("syntax error: %s", msg);
	/* NOTREACHED */
}

static void ash_msg_and_raise(int, const char *, ...) NORETURN;
static void
ash_msg_and_raise(int cond, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	ash_vmsg_and_raise(cond, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}

/*
 * error/warning routines for external builtins
 */
static void
ash_msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ash_vmsg(fmt, ap);
	va_end(ap);
}

/*
 * Return a string describing an error.  The returned string may be a
 * pointer to a static buffer that will be overwritten on the next call.
 * Action describes the operation that got the error.
 */
static const char *
errmsg(int e, const char *em)
{
	if (e == ENOENT || e == ENOTDIR) {
		return em;
	}
	return strerror(e);
}


/* ============ Memory allocation */

#if 0
/* I consider these wrappers nearly useless:
 * ok, they return you to nearest exception handler, but
 * how much memory do you leak in the process, making
 * memory starvation worse?
 */
static void *
ckrealloc(void * p, size_t nbytes)
{
	p = realloc(p, nbytes);
	if (!p)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	return p;
}

static void *
ckmalloc(size_t nbytes)
{
	return ckrealloc(NULL, nbytes);
}

static void *
ckzalloc(size_t nbytes)
{
	return memset(ckmalloc(nbytes), 0, nbytes);
}

static char *
ckstrdup(const char *s)
{
	char *p = strdup(s);
	if (!p)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	return p;
}
#else
/* Using bbox equivalents. They exit if out of memory */
# define ckrealloc xrealloc
# define ckmalloc  xmalloc
# define ckzalloc  xzalloc
# define ckstrdup  xstrdup
#endif

/*
 * It appears that grabstackstr() will barf with such alignments
 * because stalloc() will return a string allocated in a new stackblock.
 */
#define SHELL_ALIGN(nbytes) (((nbytes) + SHELL_SIZE) & ~SHELL_SIZE)
enum {
	/* Most machines require the value returned from malloc to be aligned
	 * in some way.  The following macro will get this right
	 * on many machines.  */
	SHELL_SIZE = sizeof(union { int i; char *cp; double d; }) - 1,
	/* Minimum size of a block */
	MINSIZE = SHELL_ALIGN(504),
};

struct stack_block {
	struct stack_block *prev;
	char space[MINSIZE];
};

struct stackmark {
	struct stack_block *stackp;
	char *stacknxt;
	size_t stacknleft;
	struct stackmark *marknext;
};


struct globals_memstack {
	struct stack_block *g_stackp; // = &stackbase;
	struct stackmark *markp;
	char *g_stacknxt; // = stackbase.space;
	char *sstrend; // = stackbase.space + MINSIZE;
	size_t g_stacknleft; // = MINSIZE;
	int    herefd; // = -1;
	struct stack_block stackbase;
};
extern struct globals_memstack *const ash_ptr_to_globals_memstack;
#define G_memstack (*ash_ptr_to_globals_memstack)
#define g_stackp     (G_memstack.g_stackp    )
#define markp        (G_memstack.markp       )
#define g_stacknxt   (G_memstack.g_stacknxt  )
#define sstrend      (G_memstack.sstrend     )
#define g_stacknleft (G_memstack.g_stacknleft)
#define herefd       (G_memstack.herefd      )
#define stackbase    (G_memstack.stackbase   )
#define INIT_G_memstack() do { \
	(*(struct globals_memstack**)&ash_ptr_to_globals_memstack) = xzalloc(sizeof(G_memstack)); \
	barrier(); \
	g_stackp = &stackbase; \
	g_stacknxt = stackbase.space; \
	g_stacknleft = MINSIZE; \
	sstrend = stackbase.space + MINSIZE; \
	herefd = -1; \
} while (0)


#define stackblock()     ((void *)g_stacknxt)
#define stackblocksize() g_stacknleft

/*
 * Parse trees for commands are allocated in lifo order, so we use a stack
 * to make this more efficient, and also to avoid all sorts of exception
 * handling code to handle interrupts in the middle of a parse.
 *
 * The size 504 was chosen because the Ultrix malloc handles that size
 * well.
 */
static void *
stalloc(size_t nbytes)
{
	char *p;
	size_t aligned;

	aligned = SHELL_ALIGN(nbytes);
	if (aligned > g_stacknleft) {
		size_t len;
		size_t blocksize;
		struct stack_block *sp;

		blocksize = aligned;
		if (blocksize < MINSIZE)
			blocksize = MINSIZE;
		len = sizeof(struct stack_block) - MINSIZE + blocksize;
		if (len < blocksize)
			ash_msg_and_raise_error(bb_msg_memory_exhausted);
		INT_OFF;
		sp = ckmalloc(len);
		sp->prev = g_stackp;
		g_stacknxt = sp->space;
		g_stacknleft = blocksize;
		sstrend = g_stacknxt + blocksize;
		g_stackp = sp;
		INT_ON;
	}
	p = g_stacknxt;
	g_stacknxt += aligned;
	g_stacknleft -= aligned;
	return p;
}

static void *
stzalloc(size_t nbytes)
{
	return memset(stalloc(nbytes), 0, nbytes);
}

static void
stunalloc(void *p)
{
#if DEBUG
	if (!p || (g_stacknxt < (char *)p) || ((char *)p < g_stackp->space)) {
		write(STDERR_FILENO, "stunalloc\n", 10);
		abort();
	}
#endif
	g_stacknleft += g_stacknxt - (char *)p;
	g_stacknxt = p;
}

/*
 * Like strdup but works with the ash stack.
 */
static char *
ststrdup(const char *p)
{
	size_t len = strlen(p) + 1;
	return memcpy(stalloc(len), p, len);
}

static void
setstackmark(struct stackmark *mark)
{
	mark->stackp = g_stackp;
	mark->stacknxt = g_stacknxt;
	mark->stacknleft = g_stacknleft;
	mark->marknext = markp;
	markp = mark;
}

static void
popstackmark(struct stackmark *mark)
{
	struct stack_block *sp;

	if (!mark->stackp)
		return;

	INT_OFF;
	markp = mark->marknext;
	while (g_stackp != mark->stackp) {
		sp = g_stackp;
		g_stackp = sp->prev;
		free(sp);
	}
	g_stacknxt = mark->stacknxt;
	g_stacknleft = mark->stacknleft;
	sstrend = mark->stacknxt + mark->stacknleft;
	INT_ON;
}

/*
 * When the parser reads in a string, it wants to stick the string on the
 * stack and only adjust the stack pointer when it knows how big the
 * string is.  Stackblock (defined in stack.h) returns a pointer to a block
 * of space on top of the stack and stackblocklen returns the length of
 * this block.  Growstackblock will grow this space by at least one byte,
 * possibly moving it (like realloc).  Grabstackblock actually allocates the
 * part of the block that has been used.
 */
static void
growstackblock(void)
{
	size_t newlen;

	newlen = g_stacknleft * 2;
	if (newlen < g_stacknleft)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	if (newlen < 128)
		newlen += 128;

	if (g_stacknxt == g_stackp->space && g_stackp != &stackbase) {
		struct stack_block *oldstackp;
		struct stackmark *xmark;
		struct stack_block *sp;
		struct stack_block *prevstackp;
		size_t grosslen;

		INT_OFF;
		oldstackp = g_stackp;
		sp = g_stackp;
		prevstackp = sp->prev;
		grosslen = newlen + sizeof(struct stack_block) - MINSIZE;
		sp = ckrealloc(sp, grosslen);
		sp->prev = prevstackp;
		g_stackp = sp;
		g_stacknxt = sp->space;
		g_stacknleft = newlen;
		sstrend = sp->space + newlen;

		/*
		 * Stack marks pointing to the start of the old block
		 * must be relocated to point to the new block
		 */
		xmark = markp;
		while (xmark != NULL && xmark->stackp == oldstackp) {
			xmark->stackp = g_stackp;
			xmark->stacknxt = g_stacknxt;
			xmark->stacknleft = g_stacknleft;
			xmark = xmark->marknext;
		}
		INT_ON;
	} else {
		char *oldspace = g_stacknxt;
		size_t oldlen = g_stacknleft;
		char *p = stalloc(newlen);

		/* free the space we just allocated */
		g_stacknxt = memcpy(p, oldspace, oldlen);
		g_stacknleft += newlen;
	}
}

static void
grabstackblock(size_t len)
{
	len = SHELL_ALIGN(len);
	g_stacknxt += len;
	g_stacknleft -= len;
}

/*
 * The following routines are somewhat easier to use than the above.
 * The user declares a variable of type STACKSTR, which may be declared
 * to be a register.  The macro STARTSTACKSTR initializes things.  Then
 * the user uses the macro STPUTC to add characters to the string.  In
 * effect, STPUTC(c, p) is the same as *p++ = c except that the stack is
 * grown as necessary.  When the user is done, she can just leave the
 * string there and refer to it using stackblock().  Or she can allocate
 * the space for it using grabstackstr().  If it is necessary to allow
 * someone else to use the stack temporarily and then continue to grow
 * the string, the user should use grabstack to allocate the space, and
 * then call ungrabstr(p) to return to the previous mode of operation.
 *
 * USTPUTC is like STPUTC except that it doesn't check for overflow.
 * CHECKSTACKSPACE can be called before USTPUTC to ensure that there
 * is space for at least one character.
 */
static void *
growstackstr(void)
{
	size_t len = stackblocksize();
	if (herefd >= 0 && len >= 1024) {
		full_write(herefd, stackblock(), len);
		return stackblock();
	}
	growstackblock();
	return (char *)stackblock() + len;
}

/*
 * Called from CHECKSTRSPACE.
 */
static char *
makestrspace(size_t newlen, char *p)
{
	size_t len = p - g_stacknxt;
	size_t size = stackblocksize();

	for (;;) {
		size_t nleft;

		size = stackblocksize();
		nleft = size - len;
		if (nleft >= newlen)
			break;
		growstackblock();
	}
	return (char *)stackblock() + len;
}

static char *
stack_nputstr(const char *s, size_t n, char *p)
{
	p = makestrspace(n, p);
	p = (char *)memcpy(p, s, n) + n;
	return p;
}

static char *
stack_putstr(const char *s, char *p)
{
	return stack_nputstr(s, strlen(s), p);
}

static char *
_STPUTC(int c, char *p)
{
	if (p == sstrend)
		p = growstackstr();
	*p++ = c;
	return p;
}

#define STARTSTACKSTR(p)        ((p) = stackblock())
#define STPUTC(c, p)            ((p) = _STPUTC((c), (p)))
#define CHECKSTRSPACE(n, p) do { \
	char *q = (p); \
	size_t l = (n); \
	size_t m = sstrend - q; \
	if (l > m) \
		(p) = makestrspace(l, q); \
} while (0)
#define USTPUTC(c, p)           (*(p)++ = (c))
#define STACKSTRNUL(p) do { \
	if ((p) == sstrend) \
		(p) = growstackstr(); \
	*(p) = '\0'; \
} while (0)
#define STUNPUTC(p)             (--(p))
#define STTOPC(p)               ((p)[-1])
#define STADJUST(amount, p)     ((p) += (amount))

#define grabstackstr(p)         stalloc((char *)(p) - (char *)stackblock())
#define ungrabstackstr(s, p)    stunalloc(s)
#define stackstrend()           ((void *)sstrend)


/* ============ String helpers */

/*
 * prefix -- see if pfx is a prefix of string.
 */
static char *
prefix(const char *string, const char *pfx)
{
	while (*pfx) {
		if (*pfx++ != *string++)
			return NULL;
	}
	return (char *) string;
}

/*
 * Check for a valid number.  This should be elsewhere.
 */
static int
is_number(const char *p)
{
	do {
		if (!isdigit(*p))
			return 0;
	} while (*++p != '\0');
	return 1;
}

/*
 * Convert a string of digits to an integer, printing an error message on
 * failure.
 */
static int
number(const char *s)
{
	if (!is_number(s))
		ash_msg_and_raise_error(msg_illnum, s);
	return atoi(s);
}

/*
 * Produce a possibly single quoted string suitable as input to the shell.
 * The return string is allocated on the stack.
 */
static char *
single_quote(const char *s)
{
	char *p;

	STARTSTACKSTR(p);

	do {
		char *q;
		size_t len;

		len = strchrnul(s, '\'') - s;

		q = p = makestrspace(len + 3, p);

		*q++ = '\'';
		q = (char *)memcpy(q, s, len) + len;
		*q++ = '\'';
		s += len;

		STADJUST(q - p, p);

		if (*s != '\'')
			break;
		len = 0;
		do len++; while (*++s == '\'');

		q = p = makestrspace(len + 3, p);

		*q++ = '"';
		q = (char *)memcpy(q, s - len, len) + len;
		*q++ = '"';

		STADJUST(q - p, p);
	} while (*s);

	USTPUTC('\0', p);

	return stackblock();
}


/* ============ nextopt */

static char **argptr;                  /* argument list for builtin commands */
static char *optionarg;                /* set by nextopt (like getopt) */
static char *optptr;                   /* used by nextopt */

/*
 * XXX - should get rid of. Have all builtins use getopt(3).
 * The library getopt must have the BSD extension static variable
 * "optreset", otherwise it can't be used within the shell safely.
 *
 * Standard option processing (a la getopt) for builtin routines.
 * The only argument that is passed to nextopt is the option string;
 * the other arguments are unnecessary. It returns the character,
 * or '\0' on end of input.
 */
static int
nextopt(const char *optstring)
{
	char *p;
	const char *q;
	char c;

	p = optptr;
	if (p == NULL || *p == '\0') {
		/* We ate entire "-param", take next one */
		p = *argptr;
		if (p == NULL)
			return '\0';
		if (*p != '-')
			return '\0';
		if (*++p == '\0') /* just "-" ? */
			return '\0';
		argptr++;
		if (LONE_DASH(p)) /* "--" ? */
			return '\0';
		/* p => next "-param" */
	}
	/* p => some option char in the middle of a "-param" */
	c = *p++;
	for (q = optstring; *q != c;) {
		if (*q == '\0')
			ash_msg_and_raise_error("illegal option -%c", c);
		if (*++q == ':')
			q++;
	}
	if (*++q == ':') {
		if (*p == '\0') {
			p = *argptr++;
			if (p == NULL)
				ash_msg_and_raise_error("no arg for -%c option", c);
		}
		optionarg = p;
		p = NULL;
	}
	optptr = p;
	return c;
}


/* ============ Shell variables */

/*
 * The parsefile structure pointed to by the global variable parsefile
 * contains information about the current file being read.
 */
struct shparam {
	int nparam;             /* # of positional parameters (without $0) */
#if ENABLE_ASH_GETOPTS
	int optind;             /* next parameter to be processed by getopts */
	int optoff;             /* used by getopts */
#endif
	unsigned char malloced; /* if parameter list dynamically allocated */
	char **p;               /* parameter list */
};

/*
 * Free the list of positional parameters.
 */
static void
freeparam(volatile struct shparam *param)
{
	if (param->malloced) {
		char **ap, **ap1;
		ap = ap1 = param->p;
		while (*ap)
			free(*ap++);
		free(ap1);
	}
}

#if ENABLE_ASH_GETOPTS
static void FAST_FUNC getoptsreset(const char *value);
#endif

struct var {
	struct var *next;               /* next entry in hash list */
	int flags;                      /* flags are defined above */
	const char *var_text;           /* name=value */
	void (*var_func)(const char *) FAST_FUNC; /* function to be called when  */
					/* the variable gets set/unset */
};

struct localvar {
	struct localvar *next;          /* next local variable in list */
	struct var *vp;                 /* the variable that was made local */
	int flags;                      /* saved flags */
	const char *text;               /* saved text */
};

/* flags */
#define VEXPORT         0x01    /* variable is exported */
#define VREADONLY       0x02    /* variable cannot be modified */
#define VSTRFIXED       0x04    /* variable struct is statically allocated */
#define VTEXTFIXED      0x08    /* text is statically allocated */
#define VSTACK          0x10    /* text is allocated on the stack */
#define VUNSET          0x20    /* the variable is not set */
#define VNOFUNC         0x40    /* don't call the callback function */
#define VNOSET          0x80    /* do not set variable - just readonly test */
#define VNOSAVE         0x100   /* when text is on the heap before setvareq */
#if ENABLE_ASH_RANDOM_SUPPORT
# define VDYNAMIC       0x200   /* dynamic variable */
#else
# define VDYNAMIC       0
#endif


/* Need to be before varinit_data[] */
#if ENABLE_LOCALE_SUPPORT
static void FAST_FUNC
change_lc_all(const char *value)
{
	if (value && *value != '\0')
		setlocale(LC_ALL, value);
}
static void FAST_FUNC
change_lc_ctype(const char *value)
{
	if (value && *value != '\0')
		setlocale(LC_CTYPE, value);
}
#endif
#if ENABLE_ASH_MAIL
static void chkmail(void);
static void changemail(const char *var_value) FAST_FUNC;
#else
# define chkmail()  ((void)0)
#endif
static void changepath(const char *) FAST_FUNC;
#if ENABLE_ASH_RANDOM_SUPPORT
static void change_random(const char *) FAST_FUNC;
#endif

static const struct {
	int flags;
	const char *var_text;
	void (*var_func)(const char *) FAST_FUNC;
} varinit_data[] = {
	{ VSTRFIXED|VTEXTFIXED       , defifsvar   , NULL            },
#if ENABLE_ASH_MAIL
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "MAIL"      , changemail      },
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "MAILPATH"  , changemail      },
#endif
	{ VSTRFIXED|VTEXTFIXED       , bb_PATH_root_path, changepath },
	{ VSTRFIXED|VTEXTFIXED       , "PS1=$ "    , NULL            },
	{ VSTRFIXED|VTEXTFIXED       , "PS2=> "    , NULL            },
	{ VSTRFIXED|VTEXTFIXED       , "PS4=+ "    , NULL            },
#if ENABLE_ASH_GETOPTS
	{ VSTRFIXED|VTEXTFIXED       , "OPTIND=1"  , getoptsreset    },
#endif
#if ENABLE_ASH_RANDOM_SUPPORT
	{ VSTRFIXED|VTEXTFIXED|VUNSET|VDYNAMIC, "RANDOM", change_random },
#endif
#if ENABLE_LOCALE_SUPPORT
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "LC_ALL"    , change_lc_all   },
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "LC_CTYPE"  , change_lc_ctype },
#endif
#if ENABLE_FEATURE_EDITING_SAVEHISTORY
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "HISTFILE"  , NULL            },
#endif
};

struct redirtab;

struct globals_var {
	struct shparam shellparam;      /* $@ current positional parameters */
	struct redirtab *redirlist;
	int g_nullredirs;
	int preverrout_fd;   /* save fd2 before print debug if xflag is set. */
	struct var *vartab[VTABSIZE];
	struct var varinit[ARRAY_SIZE(varinit_data)];
};
extern struct globals_var *const ash_ptr_to_globals_var;
#define G_var (*ash_ptr_to_globals_var)
#define shellparam    (G_var.shellparam   )
//#define redirlist     (G_var.redirlist    )
#define g_nullredirs  (G_var.g_nullredirs )
#define preverrout_fd (G_var.preverrout_fd)
#define vartab        (G_var.vartab       )
#define varinit       (G_var.varinit      )
#define INIT_G_var() do { \
	unsigned i; \
	(*(struct globals_var**)&ash_ptr_to_globals_var) = xzalloc(sizeof(G_var)); \
	barrier(); \
	for (i = 0; i < ARRAY_SIZE(varinit_data); i++) { \
		varinit[i].flags    = varinit_data[i].flags; \
		varinit[i].var_text = varinit_data[i].var_text; \
		varinit[i].var_func = varinit_data[i].var_func; \
	} \
} while (0)

#define vifs      varinit[0]
#if ENABLE_ASH_MAIL
# define vmail    (&vifs)[1]
# define vmpath   (&vmail)[1]
# define vpath    (&vmpath)[1]
#else
# define vpath    (&vifs)[1]
#endif
#define vps1      (&vpath)[1]
#define vps2      (&vps1)[1]
#define vps4      (&vps2)[1]
#if ENABLE_ASH_GETOPTS
# define voptind  (&vps4)[1]
# if ENABLE_ASH_RANDOM_SUPPORT
#  define vrandom (&voptind)[1]
# endif
#else
# if ENABLE_ASH_RANDOM_SUPPORT
#  define vrandom (&vps4)[1]
# endif
#endif

/*
 * The following macros access the values of the above variables.
 * They have to skip over the name.  They return the null string
 * for unset variables.
 */
#define ifsval()        (vifs.var_text + 4)
#define ifsset()        ((vifs.flags & VUNSET) == 0)
#if ENABLE_ASH_MAIL
# define mailval()      (vmail.var_text + 5)
# define mpathval()     (vmpath.var_text + 9)
# define mpathset()     ((vmpath.flags & VUNSET) == 0)
#endif
#define pathval()       (vpath.var_text + 5)
#define ps1val()        (vps1.var_text + 4)
#define ps2val()        (vps2.var_text + 4)
#define ps4val()        (vps4.var_text + 4)
#if ENABLE_ASH_GETOPTS
# define optindval()    (voptind.var_text + 7)
#endif

#if ENABLE_ASH_GETOPTS
static void FAST_FUNC
getoptsreset(const char *value)
{
	shellparam.optind = number(value);
	shellparam.optoff = -1;
}
#endif

/* math.h has these, otherwise define our private copies */
#if !ENABLE_SH_MATH_SUPPORT
#define is_name(c)      ((c) == '_' || isalpha((unsigned char)(c)))
#define is_in_name(c)   ((c) == '_' || isalnum((unsigned char)(c)))
/*
 * Return the pointer to the first char which is not part of a legal variable name
 * (a letter or underscore followed by letters, underscores, and digits).
 */
static const char*
endofname(const char *name)
{
	if (!is_name(*name))
		return name;
	while (*++name) {
		if (!is_in_name(*name))
			break;
	}
	return name;
}
#endif

/*
 * Compares two strings up to the first = or '\0'.  The first
 * string must be terminated by '='; the second may be terminated by
 * either '=' or '\0'.
 */
static int
varcmp(const char *p, const char *q)
{
	int c, d;

	while ((c = *p) == (d = *q)) {
		if (!c || c == '=')
			goto out;
		p++;
		q++;
	}
	if (c == '=')
		c = '\0';
	if (d == '=')
		d = '\0';
 out:
	return c - d;
}

/*
 * Find the appropriate entry in the hash table from the name.
 */
static struct var **
hashvar(const char *p)
{
	unsigned hashval;

	hashval = ((unsigned char) *p) << 4;
	while (*p && *p != '=')
		hashval += (unsigned char) *p++;
	return &vartab[hashval % VTABSIZE];
}

static int
vpcmp(const void *a, const void *b)
{
	return varcmp(*(const char **)a, *(const char **)b);
}

/*
 * This routine initializes the builtin variables.
 */
static void
initvar(void)
{
	struct var *vp;
	struct var *end;
	struct var **vpp;

	/*
	 * PS1 depends on uid
	 */
#if ENABLE_FEATURE_EDITING && ENABLE_FEATURE_EDITING_FANCY_PROMPT
	vps1.var_text = "PS1=\\w \\$ ";
#else
	if (!geteuid())
		vps1.var_text = "PS1=# ";
#endif
	vp = varinit;
	end = vp + ARRAY_SIZE(varinit);
	do {
		vpp = hashvar(vp->var_text);
		vp->next = *vpp;
		*vpp = vp;
	} while (++vp < end);
}

static struct var **
findvar(struct var **vpp, const char *name)
{
	for (; *vpp; vpp = &(*vpp)->next) {
		if (varcmp((*vpp)->var_text, name) == 0) {
			break;
		}
	}
	return vpp;
}

/*
 * Find the value of a variable.  Returns NULL if not set.
 */
static const char* FAST_FUNC
lookupvar(const char *name)
{
	struct var *v;

	v = *findvar(hashvar(name), name);
	if (v) {
#if ENABLE_ASH_RANDOM_SUPPORT
	/*
	 * Dynamic variables are implemented roughly the same way they are
	 * in bash. Namely, they're "special" so long as they aren't unset.
	 * As soon as they're unset, they're no longer dynamic, and dynamic
	 * lookup will no longer happen at that point. -- PFM.
	 */
		if (v->flags & VDYNAMIC)
			v->var_func(NULL);
#endif
		if (!(v->flags & VUNSET))
			return var_end(v->var_text);
	}
	return NULL;
}

/*
 * Search the environment of a builtin command.
 */
static const char *
bltinlookup(const char *name)
{
	struct strlist *sp;

	for (sp = cmdenviron; sp; sp = sp->next) {
		if (varcmp(sp->text, name) == 0)
			return var_end(sp->text);
	}
	return lookupvar(name);
}

/*
 * Same as setvar except that the variable and value are passed in
 * the first argument as name=value.  Since the first argument will
 * be actually stored in the table, it should not be a string that
 * will go away.
 * Called with interrupts off.
 */
static void
setvareq(char *s, int flags)
{
	struct var *vp, **vpp;

	vpp = hashvar(s);
	flags |= (VEXPORT & (((unsigned) (1 - aflag)) - 1));
	vp = *findvar(vpp, s);
	if (vp) {
		if ((vp->flags & (VREADONLY|VDYNAMIC)) == VREADONLY) {
			const char *n;

			if (flags & VNOSAVE)
				free(s);
			n = vp->var_text;
			ash_msg_and_raise_error("%.*s: is read only", strchrnul(n, '=') - n, n);
		}

		if (flags & VNOSET)
			return;

		if (vp->var_func && !(flags & VNOFUNC))
			vp->var_func(var_end(s));

		if (!(vp->flags & (VTEXTFIXED|VSTACK)))
			free((char*)vp->var_text);

		flags |= vp->flags & ~(VTEXTFIXED|VSTACK|VNOSAVE|VUNSET);
	} else {
		/* variable s is not found */
		if (flags & VNOSET)
			return;
		vp = ckzalloc(sizeof(*vp));
		vp->next = *vpp;
		/*vp->func = NULL; - ckzalloc did it */
		*vpp = vp;
	}
	if (!(flags & (VTEXTFIXED|VSTACK|VNOSAVE)))
		s = ckstrdup(s);
	vp->var_text = s;
	vp->flags = flags;
}

/*
 * Set the value of a variable.  The flags argument is ored with the
 * flags of the variable.  If val is NULL, the variable is unset.
 */
static void
setvar(const char *name, const char *val, int flags)
{
	const char *q;
	char *p;
	char *nameeq;
	size_t namelen;
	size_t vallen;

	q = endofname(name);
	p = strchrnul(q, '=');
	namelen = p - name;
	if (!namelen || p != q)
		ash_msg_and_raise_error("%.*s: bad variable name", namelen, name);
	vallen = 0;
	if (val == NULL) {
		flags |= VUNSET;
	} else {
		vallen = strlen(val);
	}

	INT_OFF;
	nameeq = ckmalloc(namelen + vallen + 2);
	p = memcpy(nameeq, name, namelen) + namelen;
	if (val) {
		*p++ = '=';
		p = memcpy(p, val, vallen) + vallen;
	}
	*p = '\0';
	setvareq(nameeq, flags | VNOSAVE);
	INT_ON;
}

static void FAST_FUNC
setvar2(const char *name, const char *val)
{
	setvar(name, val, 0);
}

#if ENABLE_ASH_GETOPTS
/*
 * Safe version of setvar, returns 1 on success 0 on failure.
 */
static int
setvarsafe(const char *name, const char *val, int flags)
{
	int err;
	volatile int saveint;
	struct jmploc *volatile savehandler = exception_handler;
	struct jmploc jmploc;

	SAVE_INT(saveint);
	if (setjmp(jmploc.loc))
		err = 1;
	else {
		exception_handler = &jmploc;
		setvar(name, val, flags);
		err = 0;
	}
	exception_handler = savehandler;
	RESTORE_INT(saveint);
	return err;
}
#endif

/*
 * Unset the specified variable.
 */
static int
unsetvar(const char *s)
{
	struct var **vpp;
	struct var *vp;
	int retval;

	vpp = findvar(hashvar(s), s);
	vp = *vpp;
	retval = 2;
	if (vp) {
		int flags = vp->flags;

		retval = 1;
		if (flags & VREADONLY)
			goto out;
#if ENABLE_ASH_RANDOM_SUPPORT
		vp->flags &= ~VDYNAMIC;
#endif
		if (flags & VUNSET)
			goto ok;
		if ((flags & VSTRFIXED) == 0) {
			INT_OFF;
			if ((flags & (VTEXTFIXED|VSTACK)) == 0)
				free((char*)vp->var_text);
			*vpp = vp->next;
			free(vp);
			INT_ON;
		} else {
			setvar(s, 0, 0);
			vp->flags &= ~VEXPORT;
		}
 ok:
		retval = 0;
	}
 out:
	return retval;
}

/*
 * Process a linked list of variable assignments.
 */
static void
listsetvar(struct strlist *list_set_var, int flags)
{
	struct strlist *lp = list_set_var;

	if (!lp)
		return;
	INT_OFF;
	do {
		setvareq(lp->text, flags);
		lp = lp->next;
	} while (lp);
	INT_ON;
}

/*
 * Generate a list of variables satisfying the given conditions.
 */
static char **
listvars(int on, int off, char ***end)
{
	struct var **vpp;
	struct var *vp;
	char **ep;
	int mask;

	STARTSTACKSTR(ep);
	vpp = vartab;
	mask = on | off;
	do {
		for (vp = *vpp; vp; vp = vp->next) {
			if ((vp->flags & mask) == on) {
				if (ep == stackstrend())
					ep = growstackstr();
				*ep++ = (char*)vp->var_text;
			}
		}
	} while (++vpp < vartab + VTABSIZE);
	if (ep == stackstrend())
		ep = growstackstr();
	if (end)
		*end = ep;
	*ep++ = NULL;
	return grabstackstr(ep);
}


/* ============ Path search helper
 *
 * The variable path (passed by reference) should be set to the start
 * of the path before the first call; path_advance will update
 * this value as it proceeds.  Successive calls to path_advance will return
 * the possible path expansions in sequence.  If an option (indicated by
 * a percent sign) appears in the path entry then the global variable
 * pathopt will be set to point to it; otherwise pathopt will be set to
 * NULL.
 */
static const char *pathopt;     /* set by path_advance */

static char *
path_advance(const char **path, const char *name)
{
	const char *p;
	char *q;
	const char *start;
	size_t len;

	if (*path == NULL)
		return NULL;
	start = *path;
	for (p = start; *p && *p != ':' && *p != '%'; p++)
		continue;
	len = p - start + strlen(name) + 2;     /* "2" is for '/' and '\0' */
	while (stackblocksize() < len)
		growstackblock();
	q = stackblock();
	if (p != start) {
		memcpy(q, start, p - start);
		q += p - start;
		*q++ = '/';
	}
	strcpy(q, name);
	pathopt = NULL;
	if (*p == '%') {
		pathopt = ++p;
		while (*p && *p != ':')
			p++;
	}
	if (*p == ':')
		*path = p + 1;
	else
		*path = NULL;
	return stalloc(len);
}


/* ============ Prompt */

static smallint doprompt;                   /* if set, prompt the user */
static smallint needprompt;                 /* true if interactive and at start of line */

#if ENABLE_FEATURE_EDITING
static line_input_t *line_input_state;
static const char *cmdedit_prompt;
static void
putprompt(const char *s)
{
	if (ENABLE_ASH_EXPAND_PRMT) {
		free((char*)cmdedit_prompt);
		cmdedit_prompt = ckstrdup(s);
		return;
	}
	cmdedit_prompt = s;
}
#else
static void
putprompt(const char *s)
{
	out2str(s);
}
#endif

#if ENABLE_ASH_EXPAND_PRMT
/* expandstr() needs parsing machinery, so it is far away ahead... */
static const char *expandstr(const char *ps);
#else
#define expandstr(s) s
#endif

static void
setprompt_if(smallint do_set, int whichprompt)
{
	const char *prompt;
	IF_ASH_EXPAND_PRMT(struct stackmark smark;)

	if (!do_set)
		return;

	needprompt = 0;

	switch (whichprompt) {
	case 1:
		prompt = ps1val();
		break;
	case 2:
		prompt = ps2val();
		break;
	default:                        /* 0 */
		prompt = nullstr;
	}
#if ENABLE_ASH_EXPAND_PRMT
	setstackmark(&smark);
	stalloc(stackblocksize());
#endif
	putprompt(expandstr(prompt));
#if ENABLE_ASH_EXPAND_PRMT
	popstackmark(&smark);
#endif
}


/* ============ The cd and pwd commands */

#define CD_PHYSICAL 1
#define CD_PRINT 2

static int
cdopt(void)
{
	int flags = 0;
	int i, j;

	j = 'L';
	while ((i = nextopt("LP")) != '\0') {
		if (i != j) {
			flags ^= CD_PHYSICAL;
			j = i;
		}
	}

	return flags;
}

/*
 * Update curdir (the name of the current directory) in response to a
 * cd command.
 */
static const char *
updatepwd(const char *dir)
{
	char *new;
	char *p;
	char *cdcomppath;
	const char *lim;

	cdcomppath = ststrdup(dir);
	STARTSTACKSTR(new);
	if (*dir != '/') {
		if (curdir == nullstr)
			return 0;
		new = stack_putstr(curdir, new);
	}
	new = makestrspace(strlen(dir) + 2, new);
	lim = (char *)stackblock() + 1;
	if (*dir != '/') {
		if (new[-1] != '/')
			USTPUTC('/', new);
		if (new > lim && *lim == '/')
			lim++;
	} else {
		USTPUTC('/', new);
		cdcomppath++;
		if (dir[1] == '/' && dir[2] != '/') {
			USTPUTC('/', new);
			cdcomppath++;
			lim++;
		}
	}
	p = strtok(cdcomppath, "/");
	while (p) {
		switch (*p) {
		case '.':
			if (p[1] == '.' && p[2] == '\0') {
				while (new > lim) {
					STUNPUTC(new);
					if (new[-1] == '/')
						break;
				}
				break;
			}
			if (p[1] == '\0')
				break;
			/* fall through */
		default:
			new = stack_putstr(p, new);
			USTPUTC('/', new);
		}
		p = strtok(0, "/");
	}
	if (new > lim)
		STUNPUTC(new);
	*new = 0;
	return stackblock();
}

/*
 * Find out what the current directory is. If we already know the current
 * directory, this routine returns immediately.
 */
static char *
getpwd(void)
{
	char *dir = getcwd(NULL, 0); /* huh, using glibc extension? */
	return dir ? dir : nullstr;
}

static void
setpwd(const char *val, int setold)
{
	char *oldcur, *dir;

	oldcur = dir = curdir;

	if (setold) {
		setvar("OLDPWD", oldcur, VEXPORT);
	}
	INT_OFF;
	if (physdir != nullstr) {
		if (physdir != oldcur)
			free(physdir);
		physdir = nullstr;
	}
	if (oldcur == val || !val) {
		char *s = getpwd();
		physdir = s;
		if (!val)
			dir = s;
	} else
		dir = ckstrdup(val);
	if (oldcur != dir && oldcur != nullstr) {
		free(oldcur);
	}
	curdir = dir;
	INT_ON;
	setvar("PWD", dir, VEXPORT);
}

static void hashcd(void);

/*
 * Actually do the chdir.  We also call hashcd to let the routines in exec.c
 * know that the current directory has changed.
 */
static int
docd(const char *dest, int flags)
{
	const char *dir = NULL;
	int err;

	TRACE(("docd(\"%s\", %d) called\n", dest, flags));

	INT_OFF;
	if (!(flags & CD_PHYSICAL)) {
		dir = updatepwd(dest);
		if (dir)
			dest = dir;
	}
	err = chdir(dest);
	if (err)
		goto out;
	setpwd(dir, 1);
	hashcd();
 out:
	INT_ON;
	return err;
}

static int FAST_FUNC
cdcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	const char *dest;
	const char *path;
	const char *p;
	char c;
	struct stat statb;
	int flags;

	flags = cdopt();
	dest = *argptr;
	if (!dest)
		dest = bltinlookup("HOME");
	else if (LONE_DASH(dest)) {
		dest = bltinlookup("OLDPWD");
		flags |= CD_PRINT;
	}
	if (!dest)
		dest = nullstr;
	if (*dest == '/')
		goto step7;
	if (*dest == '.') {
		c = dest[1];
 dotdot:
		switch (c) {
		case '\0':
		case '/':
			goto step6;
		case '.':
			c = dest[2];
			if (c != '.')
				goto dotdot;
		}
	}
	if (!*dest)
		dest = ".";
	path = bltinlookup("CDPATH");
	if (!path) {
 step6:
 step7:
		p = dest;
		goto docd;
	}
	do {
		c = *path;
		p = path_advance(&path, dest);
		if (stat(p, &statb) >= 0 && S_ISDIR(statb.st_mode)) {
			if (c && c != ':')
				flags |= CD_PRINT;
 docd:
			if (!docd(p, flags))
				goto out;
			break;
		}
	} while (path);
	ash_msg_and_raise_error("can't cd to %s", dest);
	/* NOTREACHED */
 out:
	if (flags & CD_PRINT)
		out1fmt("%s\n", curdir);
	return 0;
}

static int FAST_FUNC
pwdcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int flags;
	const char *dir = curdir;

	flags = cdopt();
	if (flags) {
		if (physdir == nullstr)
			setpwd(dir, 0);
		dir = physdir;
	}
	out1fmt("%s\n", dir);
	return 0;
}


/* ============ ... */


#define IBUFSIZ (ENABLE_FEATURE_EDITING ? CONFIG_FEATURE_EDITING_MAX_LEN : 1024)

/* Syntax classes */
#define CWORD     0             /* character is nothing special */
#define CNL       1             /* newline character */
#define CBACK     2             /* a backslash character */
#define CSQUOTE   3             /* single quote */
#define CDQUOTE   4             /* double quote */
#define CENDQUOTE 5             /* a terminating quote */
#define CBQUOTE   6             /* backwards single quote */
#define CVAR      7             /* a dollar sign */
#define CENDVAR   8             /* a '}' character */
#define CLP       9             /* a left paren in arithmetic */
#define CRP      10             /* a right paren in arithmetic */
#define CENDFILE 11             /* end of file */
#define CCTL     12             /* like CWORD, except it must be escaped */
#define CSPCL    13             /* these terminate a word */
#define CIGN     14             /* character should be ignored */

#define PEOF     256
#if ENABLE_ASH_ALIAS
# define PEOA    257
#endif

#define USE_SIT_FUNCTION ENABLE_ASH_OPTIMIZE_FOR_SIZE

#if ENABLE_SH_MATH_SUPPORT
# define SIT_ITEM(a,b,c,d) (a | (b << 4) | (c << 8) | (d << 12))
#else
# define SIT_ITEM(a,b,c,d) (a | (b << 4) | (c << 8))
#endif
static const uint16_t S_I_T[] = {
#if ENABLE_ASH_ALIAS
	SIT_ITEM(CSPCL   , CIGN     , CIGN , CIGN   ),    /* 0, PEOA */
#endif
	SIT_ITEM(CSPCL   , CWORD    , CWORD, CWORD  ),    /* 1, ' ' */
	SIT_ITEM(CNL     , CNL      , CNL  , CNL    ),    /* 2, \n */
	SIT_ITEM(CWORD   , CCTL     , CCTL , CWORD  ),    /* 3, !*-/:=?[]~ */
	SIT_ITEM(CDQUOTE , CENDQUOTE, CWORD, CWORD  ),    /* 4, '"' */
	SIT_ITEM(CVAR    , CVAR     , CWORD, CVAR   ),    /* 5, $ */
	SIT_ITEM(CSQUOTE , CWORD    , CENDQUOTE, CWORD),  /* 6, "'" */
	SIT_ITEM(CSPCL   , CWORD    , CWORD, CLP    ),    /* 7, ( */
	SIT_ITEM(CSPCL   , CWORD    , CWORD, CRP    ),    /* 8, ) */
	SIT_ITEM(CBACK   , CBACK    , CCTL , CBACK  ),    /* 9, \ */
	SIT_ITEM(CBQUOTE , CBQUOTE  , CWORD, CBQUOTE),    /* 10, ` */
	SIT_ITEM(CENDVAR , CENDVAR  , CWORD, CENDVAR),    /* 11, } */
#if !USE_SIT_FUNCTION
	SIT_ITEM(CENDFILE, CENDFILE , CENDFILE, CENDFILE),/* 12, PEOF */
	SIT_ITEM(CWORD   , CWORD    , CWORD, CWORD  ),    /* 13, 0-9A-Za-z */
	SIT_ITEM(CCTL    , CCTL     , CCTL , CCTL   )     /* 14, CTLESC ... */
#endif
#undef SIT_ITEM
};
/* Constants below must match table above */
enum {
#if ENABLE_ASH_ALIAS
	CSPCL_CIGN_CIGN_CIGN               , /*  0 */
#endif
	CSPCL_CWORD_CWORD_CWORD            , /*  1 */
	CNL_CNL_CNL_CNL                    , /*  2 */
	CWORD_CCTL_CCTL_CWORD              , /*  3 */
	CDQUOTE_CENDQUOTE_CWORD_CWORD      , /*  4 */
	CVAR_CVAR_CWORD_CVAR               , /*  5 */
	CSQUOTE_CWORD_CENDQUOTE_CWORD      , /*  6 */
	CSPCL_CWORD_CWORD_CLP              , /*  7 */
	CSPCL_CWORD_CWORD_CRP              , /*  8 */
	CBACK_CBACK_CCTL_CBACK             , /*  9 */
	CBQUOTE_CBQUOTE_CWORD_CBQUOTE      , /* 10 */
	CENDVAR_CENDVAR_CWORD_CENDVAR      , /* 11 */
	CENDFILE_CENDFILE_CENDFILE_CENDFILE, /* 12 */
	CWORD_CWORD_CWORD_CWORD            , /* 13 */
	CCTL_CCTL_CCTL_CCTL                , /* 14 */
};

/* c in SIT(c, syntax) must be an *unsigned char* or PEOA or PEOF,
 * caller must ensure proper cast on it if c is *char_ptr!
 */
/* Values for syntax param */
#define BASESYNTAX 0    /* not in quotes */
#define DQSYNTAX   1    /* in double quotes */
#define SQSYNTAX   2    /* in single quotes */
#define ARISYNTAX  3    /* in arithmetic */
#define PSSYNTAX   4    /* prompt. never passed to SIT() */

#if USE_SIT_FUNCTION

static int
SIT(int c, int syntax)
{
	static const char spec_symbls[] ALIGN1 = "\t\n !\"$&'()*-/:;<=>?[\\]`|}~";
# if ENABLE_ASH_ALIAS
	static const uint8_t syntax_index_table[] ALIGN1 = {
		1, 2, 1, 3, 4, 5, 1, 6,         /* "\t\n !\"$&'" */
		7, 8, 3, 3, 3, 3, 1, 1,         /* "()*-/:;<" */
		3, 1, 3, 3, 9, 3, 10, 1,        /* "=>?[\\]`|" */
		11, 3                           /* "}~" */
	};
# else
	static const uint8_t syntax_index_table[] ALIGN1 = {
		0, 1, 0, 2, 3, 4, 0, 5,         /* "\t\n !\"$&'" */
		6, 7, 2, 2, 2, 2, 0, 0,         /* "()*-/:;<" */
		2, 0, 2, 2, 8, 2, 9, 0,         /* "=>?[\\]`|" */
		10, 2                           /* "}~" */
	};
# endif
	const char *s;
	int indx;

	if (c == PEOF)
		return CENDFILE;
# if ENABLE_ASH_ALIAS
	if (c == PEOA)
		indx = 0;
	else
# endif
	{
		/* Cast is purely for paranoia here,
		 * just in case someone passed signed char to us */
		if ((unsigned char)c >= CTL_FIRST
		 && (unsigned char)c <= CTL_LAST
		) {
			return CCTL;
		}
		s = strchrnul(spec_symbls, c);
		if (*s == '\0')
			return CWORD;
		indx = syntax_index_table[s - spec_symbls];
	}
	return (S_I_T[indx] >> (syntax*4)) & 0xf;
}

#else   /* !USE_SIT_FUNCTION */

static const uint8_t syntax_index_table[] = {
	/* BASESYNTAX_DQSYNTAX_SQSYNTAX_ARISYNTAX */
	/*   0      */ CWORD_CWORD_CWORD_CWORD,
	/*   1      */ CWORD_CWORD_CWORD_CWORD,
	/*   2      */ CWORD_CWORD_CWORD_CWORD,
	/*   3      */ CWORD_CWORD_CWORD_CWORD,
	/*   4      */ CWORD_CWORD_CWORD_CWORD,
	/*   5      */ CWORD_CWORD_CWORD_CWORD,
	/*   6      */ CWORD_CWORD_CWORD_CWORD,
	/*   7      */ CWORD_CWORD_CWORD_CWORD,
	/*   8      */ CWORD_CWORD_CWORD_CWORD,
	/*   9 "\t" */ CSPCL_CWORD_CWORD_CWORD,
	/*  10 "\n" */ CNL_CNL_CNL_CNL,
	/*  11      */ CWORD_CWORD_CWORD_CWORD,
	/*  12      */ CWORD_CWORD_CWORD_CWORD,
	/*  13      */ CWORD_CWORD_CWORD_CWORD,
	/*  14      */ CWORD_CWORD_CWORD_CWORD,
	/*  15      */ CWORD_CWORD_CWORD_CWORD,
	/*  16      */ CWORD_CWORD_CWORD_CWORD,
	/*  17      */ CWORD_CWORD_CWORD_CWORD,
	/*  18      */ CWORD_CWORD_CWORD_CWORD,
	/*  19      */ CWORD_CWORD_CWORD_CWORD,
	/*  20      */ CWORD_CWORD_CWORD_CWORD,
	/*  21      */ CWORD_CWORD_CWORD_CWORD,
	/*  22      */ CWORD_CWORD_CWORD_CWORD,
	/*  23      */ CWORD_CWORD_CWORD_CWORD,
	/*  24      */ CWORD_CWORD_CWORD_CWORD,
	/*  25      */ CWORD_CWORD_CWORD_CWORD,
	/*  26      */ CWORD_CWORD_CWORD_CWORD,
	/*  27      */ CWORD_CWORD_CWORD_CWORD,
	/*  28      */ CWORD_CWORD_CWORD_CWORD,
	/*  29      */ CWORD_CWORD_CWORD_CWORD,
	/*  30      */ CWORD_CWORD_CWORD_CWORD,
	/*  31      */ CWORD_CWORD_CWORD_CWORD,
	/*  32  " " */ CSPCL_CWORD_CWORD_CWORD,
	/*  33  "!" */ CWORD_CCTL_CCTL_CWORD,
	/*  34  """ */ CDQUOTE_CENDQUOTE_CWORD_CWORD,
	/*  35  "#" */ CWORD_CWORD_CWORD_CWORD,
	/*  36  "$" */ CVAR_CVAR_CWORD_CVAR,
	/*  37  "%" */ CWORD_CWORD_CWORD_CWORD,
	/*  38  "&" */ CSPCL_CWORD_CWORD_CWORD,
	/*  39  "'" */ CSQUOTE_CWORD_CENDQUOTE_CWORD,
	/*  40  "(" */ CSPCL_CWORD_CWORD_CLP,
	/*  41  ")" */ CSPCL_CWORD_CWORD_CRP,
	/*  42  "*" */ CWORD_CCTL_CCTL_CWORD,
	/*  43  "+" */ CWORD_CWORD_CWORD_CWORD,
	/*  44  "," */ CWORD_CWORD_CWORD_CWORD,
	/*  45  "-" */ CWORD_CCTL_CCTL_CWORD,
	/*  46  "." */ CWORD_CWORD_CWORD_CWORD,
	/*  47  "/" */ CWORD_CCTL_CCTL_CWORD,
	/*  48  "0" */ CWORD_CWORD_CWORD_CWORD,
	/*  49  "1" */ CWORD_CWORD_CWORD_CWORD,
	/*  50  "2" */ CWORD_CWORD_CWORD_CWORD,
	/*  51  "3" */ CWORD_CWORD_CWORD_CWORD,
	/*  52  "4" */ CWORD_CWORD_CWORD_CWORD,
	/*  53  "5" */ CWORD_CWORD_CWORD_CWORD,
	/*  54  "6" */ CWORD_CWORD_CWORD_CWORD,
	/*  55  "7" */ CWORD_CWORD_CWORD_CWORD,
	/*  56  "8" */ CWORD_CWORD_CWORD_CWORD,
	/*  57  "9" */ CWORD_CWORD_CWORD_CWORD,
	/*  58  ":" */ CWORD_CCTL_CCTL_CWORD,
	/*  59  ";" */ CSPCL_CWORD_CWORD_CWORD,
	/*  60  "<" */ CSPCL_CWORD_CWORD_CWORD,
	/*  61  "=" */ CWORD_CCTL_CCTL_CWORD,
	/*  62  ">" */ CSPCL_CWORD_CWORD_CWORD,
	/*  63  "?" */ CWORD_CCTL_CCTL_CWORD,
	/*  64  "@" */ CWORD_CWORD_CWORD_CWORD,
	/*  65  "A" */ CWORD_CWORD_CWORD_CWORD,
	/*  66  "B" */ CWORD_CWORD_CWORD_CWORD,
	/*  67  "C" */ CWORD_CWORD_CWORD_CWORD,
	/*  68  "D" */ CWORD_CWORD_CWORD_CWORD,
	/*  69  "E" */ CWORD_CWORD_CWORD_CWORD,
	/*  70  "F" */ CWORD_CWORD_CWORD_CWORD,
	/*  71  "G" */ CWORD_CWORD_CWORD_CWORD,
	/*  72  "H" */ CWORD_CWORD_CWORD_CWORD,
	/*  73  "I" */ CWORD_CWORD_CWORD_CWORD,
	/*  74  "J" */ CWORD_CWORD_CWORD_CWORD,
	/*  75  "K" */ CWORD_CWORD_CWORD_CWORD,
	/*  76  "L" */ CWORD_CWORD_CWORD_CWORD,
	/*  77  "M" */ CWORD_CWORD_CWORD_CWORD,
	/*  78  "N" */ CWORD_CWORD_CWORD_CWORD,
	/*  79  "O" */ CWORD_CWORD_CWORD_CWORD,
	/*  80  "P" */ CWORD_CWORD_CWORD_CWORD,
	/*  81  "Q" */ CWORD_CWORD_CWORD_CWORD,
	/*  82  "R" */ CWORD_CWORD_CWORD_CWORD,
	/*  83  "S" */ CWORD_CWORD_CWORD_CWORD,
	/*  84  "T" */ CWORD_CWORD_CWORD_CWORD,
	/*  85  "U" */ CWORD_CWORD_CWORD_CWORD,
	/*  86  "V" */ CWORD_CWORD_CWORD_CWORD,
	/*  87  "W" */ CWORD_CWORD_CWORD_CWORD,
	/*  88  "X" */ CWORD_CWORD_CWORD_CWORD,
	/*  89  "Y" */ CWORD_CWORD_CWORD_CWORD,
	/*  90  "Z" */ CWORD_CWORD_CWORD_CWORD,
	/*  91  "[" */ CWORD_CCTL_CCTL_CWORD,
	/*  92  "\" */ CBACK_CBACK_CCTL_CBACK,
	/*  93  "]" */ CWORD_CCTL_CCTL_CWORD,
	/*  94  "^" */ CWORD_CWORD_CWORD_CWORD,
	/*  95  "_" */ CWORD_CWORD_CWORD_CWORD,
	/*  96  "`" */ CBQUOTE_CBQUOTE_CWORD_CBQUOTE,
	/*  97  "a" */ CWORD_CWORD_CWORD_CWORD,
	/*  98  "b" */ CWORD_CWORD_CWORD_CWORD,
	/*  99  "c" */ CWORD_CWORD_CWORD_CWORD,
	/* 100  "d" */ CWORD_CWORD_CWORD_CWORD,
	/* 101  "e" */ CWORD_CWORD_CWORD_CWORD,
	/* 102  "f" */ CWORD_CWORD_CWORD_CWORD,
	/* 103  "g" */ CWORD_CWORD_CWORD_CWORD,
	/* 104  "h" */ CWORD_CWORD_CWORD_CWORD,
	/* 105  "i" */ CWORD_CWORD_CWORD_CWORD,
	/* 106  "j" */ CWORD_CWORD_CWORD_CWORD,
	/* 107  "k" */ CWORD_CWORD_CWORD_CWORD,
	/* 108  "l" */ CWORD_CWORD_CWORD_CWORD,
	/* 109  "m" */ CWORD_CWORD_CWORD_CWORD,
	/* 110  "n" */ CWORD_CWORD_CWORD_CWORD,
	/* 111  "o" */ CWORD_CWORD_CWORD_CWORD,
	/* 112  "p" */ CWORD_CWORD_CWORD_CWORD,
	/* 113  "q" */ CWORD_CWORD_CWORD_CWORD,
	/* 114  "r" */ CWORD_CWORD_CWORD_CWORD,
	/* 115  "s" */ CWORD_CWORD_CWORD_CWORD,
	/* 116  "t" */ CWORD_CWORD_CWORD_CWORD,
	/* 117  "u" */ CWORD_CWORD_CWORD_CWORD,
	/* 118  "v" */ CWORD_CWORD_CWORD_CWORD,
	/* 119  "w" */ CWORD_CWORD_CWORD_CWORD,
	/* 120  "x" */ CWORD_CWORD_CWORD_CWORD,
	/* 121  "y" */ CWORD_CWORD_CWORD_CWORD,
	/* 122  "z" */ CWORD_CWORD_CWORD_CWORD,
	/* 123  "{" */ CWORD_CWORD_CWORD_CWORD,
	/* 124  "|" */ CSPCL_CWORD_CWORD_CWORD,
	/* 125  "}" */ CENDVAR_CENDVAR_CWORD_CENDVAR,
	/* 126  "~" */ CWORD_CCTL_CCTL_CWORD,
	/* 127  del */ CWORD_CWORD_CWORD_CWORD,
	/* 128 0x80 */ CWORD_CWORD_CWORD_CWORD,
	/* 129 CTLESC       */ CCTL_CCTL_CCTL_CCTL,
	/* 130 CTLVAR       */ CCTL_CCTL_CCTL_CCTL,
	/* 131 CTLENDVAR    */ CCTL_CCTL_CCTL_CCTL,
	/* 132 CTLBACKQ     */ CCTL_CCTL_CCTL_CCTL,
	/* 133 CTLQUOTE     */ CCTL_CCTL_CCTL_CCTL,
	/* 134 CTLARI       */ CCTL_CCTL_CCTL_CCTL,
	/* 135 CTLENDARI    */ CCTL_CCTL_CCTL_CCTL,
	/* 136 CTLQUOTEMARK */ CCTL_CCTL_CCTL_CCTL,
	/* 137      */ CWORD_CWORD_CWORD_CWORD,
	/* 138      */ CWORD_CWORD_CWORD_CWORD,
	/* 139      */ CWORD_CWORD_CWORD_CWORD,
	/* 140      */ CWORD_CWORD_CWORD_CWORD,
	/* 141      */ CWORD_CWORD_CWORD_CWORD,
	/* 142      */ CWORD_CWORD_CWORD_CWORD,
	/* 143      */ CWORD_CWORD_CWORD_CWORD,
	/* 144      */ CWORD_CWORD_CWORD_CWORD,
	/* 145      */ CWORD_CWORD_CWORD_CWORD,
	/* 146      */ CWORD_CWORD_CWORD_CWORD,
	/* 147      */ CWORD_CWORD_CWORD_CWORD,
	/* 148      */ CWORD_CWORD_CWORD_CWORD,
	/* 149      */ CWORD_CWORD_CWORD_CWORD,
	/* 150      */ CWORD_CWORD_CWORD_CWORD,
	/* 151      */ CWORD_CWORD_CWORD_CWORD,
	/* 152      */ CWORD_CWORD_CWORD_CWORD,
	/* 153      */ CWORD_CWORD_CWORD_CWORD,
	/* 154      */ CWORD_CWORD_CWORD_CWORD,
	/* 155      */ CWORD_CWORD_CWORD_CWORD,
	/* 156      */ CWORD_CWORD_CWORD_CWORD,
	/* 157      */ CWORD_CWORD_CWORD_CWORD,
	/* 158      */ CWORD_CWORD_CWORD_CWORD,
	/* 159      */ CWORD_CWORD_CWORD_CWORD,
	/* 160      */ CWORD_CWORD_CWORD_CWORD,
	/* 161      */ CWORD_CWORD_CWORD_CWORD,
	/* 162      */ CWORD_CWORD_CWORD_CWORD,
	/* 163      */ CWORD_CWORD_CWORD_CWORD,
	/* 164      */ CWORD_CWORD_CWORD_CWORD,
	/* 165      */ CWORD_CWORD_CWORD_CWORD,
	/* 166      */ CWORD_CWORD_CWORD_CWORD,
	/* 167      */ CWORD_CWORD_CWORD_CWORD,
	/* 168      */ CWORD_CWORD_CWORD_CWORD,
	/* 169      */ CWORD_CWORD_CWORD_CWORD,
	/* 170      */ CWORD_CWORD_CWORD_CWORD,
	/* 171      */ CWORD_CWORD_CWORD_CWORD,
	/* 172      */ CWORD_CWORD_CWORD_CWORD,
	/* 173      */ CWORD_CWORD_CWORD_CWORD,
	/* 174      */ CWORD_CWORD_CWORD_CWORD,
	/* 175      */ CWORD_CWORD_CWORD_CWORD,
	/* 176      */ CWORD_CWORD_CWORD_CWORD,
	/* 177      */ CWORD_CWORD_CWORD_CWORD,
	/* 178      */ CWORD_CWORD_CWORD_CWORD,
	/* 179      */ CWORD_CWORD_CWORD_CWORD,
	/* 180      */ CWORD_CWORD_CWORD_CWORD,
	/* 181      */ CWORD_CWORD_CWORD_CWORD,
	/* 182      */ CWORD_CWORD_CWORD_CWORD,
	/* 183      */ CWORD_CWORD_CWORD_CWORD,
	/* 184      */ CWORD_CWORD_CWORD_CWORD,
	/* 185      */ CWORD_CWORD_CWORD_CWORD,
	/* 186      */ CWORD_CWORD_CWORD_CWORD,
	/* 187      */ CWORD_CWORD_CWORD_CWORD,
	/* 188      */ CWORD_CWORD_CWORD_CWORD,
	/* 189      */ CWORD_CWORD_CWORD_CWORD,
	/* 190      */ CWORD_CWORD_CWORD_CWORD,
	/* 191      */ CWORD_CWORD_CWORD_CWORD,
	/* 192      */ CWORD_CWORD_CWORD_CWORD,
	/* 193      */ CWORD_CWORD_CWORD_CWORD,
	/* 194      */ CWORD_CWORD_CWORD_CWORD,
	/* 195      */ CWORD_CWORD_CWORD_CWORD,
	/* 196      */ CWORD_CWORD_CWORD_CWORD,
	/* 197      */ CWORD_CWORD_CWORD_CWORD,
	/* 198      */ CWORD_CWORD_CWORD_CWORD,
	/* 199      */ CWORD_CWORD_CWORD_CWORD,
	/* 200      */ CWORD_CWORD_CWORD_CWORD,
	/* 201      */ CWORD_CWORD_CWORD_CWORD,
	/* 202      */ CWORD_CWORD_CWORD_CWORD,
	/* 203      */ CWORD_CWORD_CWORD_CWORD,
	/* 204      */ CWORD_CWORD_CWORD_CWORD,
	/* 205      */ CWORD_CWORD_CWORD_CWORD,
	/* 206      */ CWORD_CWORD_CWORD_CWORD,
	/* 207      */ CWORD_CWORD_CWORD_CWORD,
	/* 208      */ CWORD_CWORD_CWORD_CWORD,
	/* 209      */ CWORD_CWORD_CWORD_CWORD,
	/* 210      */ CWORD_CWORD_CWORD_CWORD,
	/* 211      */ CWORD_CWORD_CWORD_CWORD,
	/* 212      */ CWORD_CWORD_CWORD_CWORD,
	/* 213      */ CWORD_CWORD_CWORD_CWORD,
	/* 214      */ CWORD_CWORD_CWORD_CWORD,
	/* 215      */ CWORD_CWORD_CWORD_CWORD,
	/* 216      */ CWORD_CWORD_CWORD_CWORD,
	/* 217      */ CWORD_CWORD_CWORD_CWORD,
	/* 218      */ CWORD_CWORD_CWORD_CWORD,
	/* 219      */ CWORD_CWORD_CWORD_CWORD,
	/* 220      */ CWORD_CWORD_CWORD_CWORD,
	/* 221      */ CWORD_CWORD_CWORD_CWORD,
	/* 222      */ CWORD_CWORD_CWORD_CWORD,
	/* 223      */ CWORD_CWORD_CWORD_CWORD,
	/* 224      */ CWORD_CWORD_CWORD_CWORD,
	/* 225      */ CWORD_CWORD_CWORD_CWORD,
	/* 226      */ CWORD_CWORD_CWORD_CWORD,
	/* 227      */ CWORD_CWORD_CWORD_CWORD,
	/* 228      */ CWORD_CWORD_CWORD_CWORD,
	/* 229      */ CWORD_CWORD_CWORD_CWORD,
	/* 230      */ CWORD_CWORD_CWORD_CWORD,
	/* 231      */ CWORD_CWORD_CWORD_CWORD,
	/* 232      */ CWORD_CWORD_CWORD_CWORD,
	/* 233      */ CWORD_CWORD_CWORD_CWORD,
	/* 234      */ CWORD_CWORD_CWORD_CWORD,
	/* 235      */ CWORD_CWORD_CWORD_CWORD,
	/* 236      */ CWORD_CWORD_CWORD_CWORD,
	/* 237      */ CWORD_CWORD_CWORD_CWORD,
	/* 238      */ CWORD_CWORD_CWORD_CWORD,
	/* 239      */ CWORD_CWORD_CWORD_CWORD,
	/* 230      */ CWORD_CWORD_CWORD_CWORD,
	/* 241      */ CWORD_CWORD_CWORD_CWORD,
	/* 242      */ CWORD_CWORD_CWORD_CWORD,
	/* 243      */ CWORD_CWORD_CWORD_CWORD,
	/* 244      */ CWORD_CWORD_CWORD_CWORD,
	/* 245      */ CWORD_CWORD_CWORD_CWORD,
	/* 246      */ CWORD_CWORD_CWORD_CWORD,
	/* 247      */ CWORD_CWORD_CWORD_CWORD,
	/* 248      */ CWORD_CWORD_CWORD_CWORD,
	/* 249      */ CWORD_CWORD_CWORD_CWORD,
	/* 250      */ CWORD_CWORD_CWORD_CWORD,
	/* 251      */ CWORD_CWORD_CWORD_CWORD,
	/* 252      */ CWORD_CWORD_CWORD_CWORD,
	/* 253      */ CWORD_CWORD_CWORD_CWORD,
	/* 254      */ CWORD_CWORD_CWORD_CWORD,
	/* 255      */ CWORD_CWORD_CWORD_CWORD,
	/* PEOF */     CENDFILE_CENDFILE_CENDFILE_CENDFILE,
# if ENABLE_ASH_ALIAS
	/* PEOA */     CSPCL_CIGN_CIGN_CIGN,
# endif
};

# define SIT(c, syntax) ((S_I_T[syntax_index_table[c]] >> ((syntax)*4)) & 0xf)

#endif  /* !USE_SIT_FUNCTION */


/* ============ Alias handling */

#if ENABLE_ASH_ALIAS

#define ALIASINUSE 1
#define ALIASDEAD  2

struct alias {
	struct alias *next;
	char *name;
	char *val;
	int flag;
};


static struct alias **atab; // [ATABSIZE];
#define INIT_G_alias() do { \
	atab = xzalloc(ATABSIZE * sizeof(atab[0])); \
} while (0)


static struct alias **
__lookupalias(const char *name) {
	unsigned int hashval;
	struct alias **app;
	const char *p;
	unsigned int ch;

	p = name;

	ch = (unsigned char)*p;
	hashval = ch << 4;
	while (ch) {
		hashval += ch;
		ch = (unsigned char)*++p;
	}
	app = &atab[hashval % ATABSIZE];

	for (; *app; app = &(*app)->next) {
		if (strcmp(name, (*app)->name) == 0) {
			break;
		}
	}

	return app;
}

static struct alias *
lookupalias(const char *name, int check)
{
	struct alias *ap = *__lookupalias(name);

	if (check && ap && (ap->flag & ALIASINUSE))
		return NULL;
	return ap;
}

static struct alias *
freealias(struct alias *ap)
{
	struct alias *next;

	if (ap->flag & ALIASINUSE) {
		ap->flag |= ALIASDEAD;
		return ap;
	}

	next = ap->next;
	free(ap->name);
	free(ap->val);
	free(ap);
	return next;
}

static void
setalias(const char *name, const char *val)
{
	struct alias *ap, **app;

	app = __lookupalias(name);
	ap = *app;
	INT_OFF;
	if (ap) {
		if (!(ap->flag & ALIASINUSE)) {
			free(ap->val);
		}
		ap->val = ckstrdup(val);
		ap->flag &= ~ALIASDEAD;
	} else {
		/* not found */
		ap = ckzalloc(sizeof(struct alias));
		ap->name = ckstrdup(name);
		ap->val = ckstrdup(val);
		/*ap->flag = 0; - ckzalloc did it */
		/*ap->next = NULL;*/
		*app = ap;
	}
	INT_ON;
}

static int
unalias(const char *name)
{
	struct alias **app;

	app = __lookupalias(name);

	if (*app) {
		INT_OFF;
		*app = freealias(*app);
		INT_ON;
		return 0;
	}

	return 1;
}

static void
rmaliases(void)
{
	struct alias *ap, **app;
	int i;

	INT_OFF;
	for (i = 0; i < ATABSIZE; i++) {
		app = &atab[i];
		for (ap = *app; ap; ap = *app) {
			*app = freealias(*app);
			if (ap == *app) {
				app = &ap->next;
			}
		}
	}
	INT_ON;
}

static void
printalias(const struct alias *ap)
{
	out1fmt("%s=%s\n", ap->name, single_quote(ap->val));
}

/*
 * TODO - sort output
 */
static int FAST_FUNC
aliascmd(int argc UNUSED_PARAM, char **argv)
{
	char *n, *v;
	int ret = 0;
	struct alias *ap;

	if (!argv[1]) {
		int i;

		for (i = 0; i < ATABSIZE; i++) {
			for (ap = atab[i]; ap; ap = ap->next) {
				printalias(ap);
			}
		}
		return 0;
	}
	while ((n = *++argv) != NULL) {
		v = strchr(n+1, '=');
		if (v == NULL) { /* n+1: funny ksh stuff */
			ap = *__lookupalias(n);
			if (ap == NULL) {
				fprintf(stderr, "%s: %s not found\n", "alias", n);
				ret = 1;
			} else
				printalias(ap);
		} else {
			*v++ = '\0';
			setalias(n, v);
		}
	}

	return ret;
}

static int FAST_FUNC
unaliascmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int i;

	while ((i = nextopt("a")) != '\0') {
		if (i == 'a') {
			rmaliases();
			return 0;
		}
	}
	for (i = 0; *argptr; argptr++) {
		if (unalias(*argptr)) {
			fprintf(stderr, "%s: %s not found\n", "unalias", *argptr);
			i = 1;
		}
	}

	return i;
}

#endif /* ASH_ALIAS */


/* ============ jobs.c */

/* Mode argument to forkshell.  Don't change FORK_FG or FORK_BG. */
#define FORK_FG    0
#define FORK_BG    1
#define FORK_NOJOB 2

/* mode flags for showjob(s) */
#define SHOW_ONLY_PGID  0x01    /* show only pgid (jobs -p) */
#define SHOW_PIDS       0x02    /* show individual pids, not just one line per job */
#define SHOW_CHANGED    0x04    /* only jobs whose state has changed */

/*
 * A job structure contains information about a job.  A job is either a
 * single process or a set of processes contained in a pipeline.  In the
 * latter case, pidlist will be non-NULL, and will point to a -1 terminated
 * array of pids.
 */
struct procstat {
	pid_t   ps_pid;         /* process id */
	int     ps_status;      /* last process status from wait() */
	char    *ps_cmd;        /* text of command being run */
};

struct job {
	struct procstat ps0;    /* status of process */
	struct procstat *ps;    /* status or processes when more than one */
#if JOBS
	int stopstatus;         /* status of a stopped job */
#endif
	uint32_t
		nprocs: 16,     /* number of processes */
		state: 8,
#define JOBRUNNING      0       /* at least one proc running */
#define JOBSTOPPED      1       /* all procs are stopped */
#define JOBDONE         2       /* all procs are completed */
#if JOBS
		sigint: 1,      /* job was killed by SIGINT */
		jobctl: 1,      /* job running under job control */
#endif
		waited: 1,      /* true if this entry has been waited for */
		used: 1,        /* true if this entry is in used */
		changed: 1;     /* true if status has changed */
	struct job *prev_job;   /* previous job */
};

static struct job *makejob(/*union node *,*/ int);
static int forkshell(struct job *, union node *, int);
static int waitforjob(struct job *);

#if !JOBS
enum { doing_jobctl = 0 };
#define setjobctl(on) do {} while (0)
#else
static smallint doing_jobctl; //references:8
static void setjobctl(int);
#endif

/*
 * Ignore a signal.
 */
static void
ignoresig(int signo)
{
	/* Avoid unnecessary system calls. Is it already SIG_IGNed? */
	if (sigmode[signo - 1] != S_IGN && sigmode[signo - 1] != S_HARD_IGN) {
		/* No, need to do it */
		signal(signo, SIG_IGN);
	}
	sigmode[signo - 1] = S_HARD_IGN;
}

/*
 * Only one usage site - in setsignal()
 */
static void
signal_handler(int signo)
{
	gotsig[signo - 1] = 1;

	if (signo == SIGINT && !trap[SIGINT]) {
		if (!suppress_int) {
			pending_sig = 0;
			raise_interrupt(); /* does not return */
		}
		pending_int = 1;
	} else {
		pending_sig = signo;
	}
}

/*
 * Set the signal handler for the specified signal.  The routine figures
 * out what it should be set to.
 */
static void
setsignal(int signo)
{
	char *t;
	char cur_act, new_act;
	struct sigaction act;

	t = trap[signo];
	new_act = S_DFL;
	if (t != NULL) { /* trap for this sig is set */
		new_act = S_CATCH;
		if (t[0] == '\0') /* trap is "": ignore this sig */
			new_act = S_IGN;
	}

	if (rootshell && new_act == S_DFL) {
		switch (signo) {
		case SIGINT:
			if (iflag || minusc || sflag == 0)
				new_act = S_CATCH;
			break;
		case SIGQUIT:
#if DEBUG
			if (debug)
				break;
#endif
			/* man bash:
			 * "In all cases, bash ignores SIGQUIT. Non-builtin
			 * commands run by bash have signal handlers
			 * set to the values inherited by the shell
			 * from its parent". */
			new_act = S_IGN;
			break;
		case SIGTERM:
			if (iflag)
				new_act = S_IGN;
			break;
#if JOBS
		case SIGTSTP:
		case SIGTTOU:
			if (mflag)
				new_act = S_IGN;
			break;
#endif
		}
	}
//TODO: if !rootshell, we reset SIGQUIT to DFL,
//whereas we have to restore it to what shell got on entry
//from the parent. See comment above

	t = &sigmode[signo - 1];
	cur_act = *t;
	if (cur_act == 0) {
		/* current setting is not yet known */
		if (sigaction(signo, NULL, &act)) {
			/* pretend it worked; maybe we should give a warning,
			 * but other shells don't. We don't alter sigmode,
			 * so we retry every time.
			 * btw, in Linux it never fails. --vda */
			return;
		}
		if (act.sa_handler == SIG_IGN) {
			cur_act = S_HARD_IGN;
			if (mflag
			 && (signo == SIGTSTP || signo == SIGTTIN || signo == SIGTTOU)
			) {
				cur_act = S_IGN;   /* don't hard ignore these */
			}
		}
	}
	if (cur_act == S_HARD_IGN || cur_act == new_act)
		return;

	act.sa_handler = SIG_DFL;
	switch (new_act) {
	case S_CATCH:
		act.sa_handler = signal_handler;
		break;
	case S_IGN:
		act.sa_handler = SIG_IGN;
		break;
	}

	/* flags and mask matter only if !DFL and !IGN, but we do it
	 * for all cases for more deterministic behavior:
	 */
	act.sa_flags = 0;
	sigfillset(&act.sa_mask);

	sigaction_set(signo, &act);

	*t = new_act;
}

/* mode flags for set_curjob */
#define CUR_DELETE 2
#define CUR_RUNNING 1
#define CUR_STOPPED 0

/* mode flags for dowait */
#define DOWAIT_NONBLOCK WNOHANG
#define DOWAIT_BLOCK    0

#if JOBS
/* pgrp of shell on invocation */
static int initialpgrp; //references:2
static int ttyfd = -1; //5
#endif
/* array of jobs */
static struct job *jobtab; //5
/* size of array */
static unsigned njobs; //4
/* current job */
static struct job *curjob; //lots
/* number of presumed living untracked jobs */
static int jobless; //4

static void
set_curjob(struct job *jp, unsigned mode)
{
	struct job *jp1;
	struct job **jpp, **curp;

	/* first remove from list */
	jpp = curp = &curjob;
	while (1) {
		jp1 = *jpp;
		if (jp1 == jp)
			break;
		jpp = &jp1->prev_job;
	}
	*jpp = jp1->prev_job;

	/* Then re-insert in correct position */
	jpp = curp;
	switch (mode) {
	default:
#if DEBUG
		abort();
#endif
	case CUR_DELETE:
		/* job being deleted */
		break;
	case CUR_RUNNING:
		/* newly created job or backgrounded job,
		   put after all stopped jobs. */
		while (1) {
			jp1 = *jpp;
#if JOBS
			if (!jp1 || jp1->state != JOBSTOPPED)
#endif
				break;
			jpp = &jp1->prev_job;
		}
		/* FALLTHROUGH */
#if JOBS
	case CUR_STOPPED:
#endif
		/* newly stopped job - becomes curjob */
		jp->prev_job = *jpp;
		*jpp = jp;
		break;
	}
}

#if JOBS || DEBUG
static int
jobno(const struct job *jp)
{
	return jp - jobtab + 1;
}
#endif

/*
 * Convert a job name to a job structure.
 */
#if !JOBS
#define getjob(name, getctl) getjob(name)
#endif
static struct job *
getjob(const char *name, int getctl)
{
	struct job *jp;
	struct job *found;
	const char *err_msg = "%s: no such job";
	unsigned num;
	int c;
	const char *p;
	char *(*match)(const char *, const char *);

	jp = curjob;
	p = name;
	if (!p)
		goto currentjob;

	if (*p != '%')
		goto err;

	c = *++p;
	if (!c)
		goto currentjob;

	if (!p[1]) {
		if (c == '+' || c == '%') {
 currentjob:
			err_msg = "No current job";
			goto check;
		}
		if (c == '-') {
			if (jp)
				jp = jp->prev_job;
			err_msg = "No previous job";
 check:
			if (!jp)
				goto err;
			goto gotit;
		}
	}

	if (is_number(p)) {
		num = atoi(p);
		if (num < njobs) {
			jp = jobtab + num - 1;
			if (jp->used)
				goto gotit;
			goto err;
		}
	}

	match = prefix;
	if (*p == '?') {
		match = strstr;
		p++;
	}

	found = NULL;
	while (jp) {
		if (match(jp->ps[0].ps_cmd, p)) {
			if (found)
				goto err;
			found = jp;
			err_msg = "%s: ambiguous";
		}
		jp = jp->prev_job;
	}
	if (!found)
		goto err;
	jp = found;

 gotit:
#if JOBS
	err_msg = "job %s not created under job control";
	if (getctl && jp->jobctl == 0)
		goto err;
#endif
	return jp;
 err:
	ash_msg_and_raise_error(err_msg, name);
}

/*
 * Mark a job structure as unused.
 */
static void
freejob(struct job *jp)
{
	struct procstat *ps;
	int i;

	INT_OFF;
	for (i = jp->nprocs, ps = jp->ps; --i >= 0; ps++) {
		if (ps->ps_cmd != nullstr)
			free(ps->ps_cmd);
	}
	if (jp->ps != &jp->ps0)
		free(jp->ps);
	jp->used = 0;
	set_curjob(jp, CUR_DELETE);
	INT_ON;
}

#if JOBS
static void
xtcsetpgrp(int fd, pid_t pgrp)
{
	if (tcsetpgrp(fd, pgrp))
		ash_msg_and_raise_error("can't set tty process group (%m)");
}

/*
 * Turn job control on and off.
 *
 * Note:  This code assumes that the third arg to ioctl is a character
 * pointer, which is true on Berkeley systems but not System V.  Since
 * System V doesn't have job control yet, this isn't a problem now.
 *
 * Called with interrupts off.
 */
static void
setjobctl(int on)
{
	int fd;
	int pgrp;

	if (on == doing_jobctl || rootshell == 0)
		return;
	if (on) {
		int ofd;
		ofd = fd = open(_PATH_TTY, O_RDWR);
		if (fd < 0) {
	/* BTW, bash will try to open(ttyname(0)) if open("/dev/tty") fails.
	 * That sometimes helps to acquire controlling tty.
	 * Obviously, a workaround for bugs when someone
	 * failed to provide a controlling tty to bash! :) */
			fd = 2;
			while (!isatty(fd))
				if (--fd < 0)
					goto out;
		}
		fd = fcntl(fd, F_DUPFD, 10);
		if (ofd >= 0)
			close(ofd);
		if (fd < 0)
			goto out;
		/* fd is a tty at this point */
		close_on_exec_on(fd);
		while (1) { /* while we are in the background */
			pgrp = tcgetpgrp(fd);
			if (pgrp < 0) {
 out:
				ash_msg("can't access tty; job control turned off");
				mflag = on = 0;
				goto close;
			}
			if (pgrp == getpgrp())
				break;
			killpg(0, SIGTTIN);
		}
		initialpgrp = pgrp;

		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
		pgrp = rootpid;
		setpgid(0, pgrp);
		xtcsetpgrp(fd, pgrp);
	} else {
		/* turning job control off */
		fd = ttyfd;
		pgrp = initialpgrp;
		/* was xtcsetpgrp, but this can make exiting ash
		 * loop forever if pty is already deleted */
		tcsetpgrp(fd, pgrp);
		setpgid(0, pgrp);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
 close:
		if (fd >= 0)
			close(fd);
		fd = -1;
	}
	ttyfd = fd;
	doing_jobctl = on;
}

static int FAST_FUNC
killcmd(int argc, char **argv)
{
	if (argv[1] && strcmp(argv[1], "-l") != 0) {
		int i = 1;
		do {
			if (argv[i][0] == '%') {
				/*
				 * "kill %N" - job kill
				 * Converting to pgrp / pid kill
				 */
				struct job *jp;
				char *dst;
				int j, n;

				jp = getjob(argv[i], 0);
				/*
				 * In jobs started under job control, we signal
				 * entire process group by kill -PGRP_ID.
				 * This happens, f.e., in interactive shell.
				 *
				 * Otherwise, we signal each child via
				 * kill PID1 PID2 PID3.
				 * Testcases:
				 * sh -c 'sleep 1|sleep 1 & kill %1'
				 * sh -c 'true|sleep 2 & sleep 1; kill %1'
				 * sh -c 'true|sleep 1 & sleep 2; kill %1'
				 */
				n = jp->nprocs; /* can't be 0 (I hope) */
				if (jp->jobctl)
					n = 1;
				dst = alloca(n * sizeof(int)*4);
				argv[i] = dst;
				for (j = 0; j < n; j++) {
					struct procstat *ps = &jp->ps[j];
					/* Skip non-running and not-stopped members
					 * (i.e. dead members) of the job
					 */
					if (ps->ps_status != -1 && !WIFSTOPPED(ps->ps_status))
						continue;
					/*
					 * kill_main has matching code to expect
					 * leading space. Needed to not confuse
					 * negative pids with "kill -SIGNAL_NO" syntax
					 */
					dst += sprintf(dst, jp->jobctl ? " -%u" : " %u", (int)ps->ps_pid);
				}
				*dst = '\0';
			}
		} while (argv[++i]);
	}
	return kill_main(argc, argv);
}

static void
showpipe(struct job *jp /*, FILE *out*/)
{
	struct procstat *ps;
	struct procstat *psend;

	psend = jp->ps + jp->nprocs;
	for (ps = jp->ps + 1; ps < psend; ps++)
		printf(" | %s", ps->ps_cmd);
	outcslow('\n', stdout);
	flush_stdout_stderr();
}


static int
restartjob(struct job *jp, int mode)
{
	struct procstat *ps;
	int i;
	int status;
	pid_t pgid;

	INT_OFF;
	if (jp->state == JOBDONE)
		goto out;
	jp->state = JOBRUNNING;
	pgid = jp->ps[0].ps_pid;
	if (mode == FORK_FG)
		xtcsetpgrp(ttyfd, pgid);
	killpg(pgid, SIGCONT);
	ps = jp->ps;
	i = jp->nprocs;
	do {
		if (WIFSTOPPED(ps->ps_status)) {
			ps->ps_status = -1;
		}
		ps++;
	} while (--i);
 out:
	status = (mode == FORK_FG) ? waitforjob(jp) : 0;
	INT_ON;
	return status;
}

static int FAST_FUNC
fg_bgcmd(int argc UNUSED_PARAM, char **argv)
{
	struct job *jp;
	int mode;
	int retval;

	mode = (**argv == 'f') ? FORK_FG : FORK_BG;
	nextopt(nullstr);
	argv = argptr;
	do {
		jp = getjob(*argv, 1);
		if (mode == FORK_BG) {
			set_curjob(jp, CUR_RUNNING);
			printf("[%d] ", jobno(jp));
		}
		out1str(jp->ps[0].ps_cmd);
		showpipe(jp /*, stdout*/);
		retval = restartjob(jp, mode);
	} while (*argv && *++argv);
	return retval;
}
#endif

static int
sprint_status(char *s, int status, int sigonly)
{
	int col;
	int st;

	col = 0;
	if (!WIFEXITED(status)) {
#if JOBS
		if (WIFSTOPPED(status))
			st = WSTOPSIG(status);
		else
#endif
			st = WTERMSIG(status);
		if (sigonly) {
			if (st == SIGINT || st == SIGPIPE)
				goto out;
#if JOBS
			if (WIFSTOPPED(status))
				goto out;
#endif
		}
		st &= 0x7f;
//TODO: use bbox's get_signame? strsignal adds ~600 bytes to text+rodata
		col = fmtstr(s, 32, strsignal(st));
		if (WCOREDUMP(status)) {
			col += fmtstr(s + col, 16, " (core dumped)");
		}
	} else if (!sigonly) {
		st = WEXITSTATUS(status);
		if (st)
			col = fmtstr(s, 16, "Done(%d)", st);
		else
			col = fmtstr(s, 16, "Done");
	}
 out:
	return col;
}

static int
dowait(int wait_flags, struct job *job)
{
	int pid;
	int status;
	struct job *jp;
	struct job *thisjob;
	int state;

	TRACE(("dowait(0x%x) called\n", wait_flags));

	/* Do a wait system call. If job control is compiled in, we accept
	 * stopped processes. wait_flags may have WNOHANG, preventing blocking.
	 * NB: _not_ safe_waitpid, we need to detect EINTR */
	if (doing_jobctl)
		wait_flags |= WUNTRACED;
	pid = waitpid(-1, &status, wait_flags);
	TRACE(("wait returns pid=%d, status=0x%x, errno=%d(%s)\n",
				pid, status, errno, strerror(errno)));
	if (pid <= 0)
		return pid;

	INT_OFF;
	thisjob = NULL;
	for (jp = curjob; jp; jp = jp->prev_job) {
		struct procstat *ps;
		struct procstat *psend;
		if (jp->state == JOBDONE)
			continue;
		state = JOBDONE;
		ps = jp->ps;
		psend = ps + jp->nprocs;
		do {
			if (ps->ps_pid == pid) {
				TRACE(("Job %d: changing status of proc %d "
					"from 0x%x to 0x%x\n",
					jobno(jp), pid, ps->ps_status, status));
				ps->ps_status = status;
				thisjob = jp;
			}
			if (ps->ps_status == -1)
				state = JOBRUNNING;
#if JOBS
			if (state == JOBRUNNING)
				continue;
			if (WIFSTOPPED(ps->ps_status)) {
				jp->stopstatus = ps->ps_status;
				state = JOBSTOPPED;
			}
#endif
		} while (++ps < psend);
		if (thisjob)
			goto gotjob;
	}
#if JOBS
	if (!WIFSTOPPED(status))
#endif
		jobless--;
	goto out;

 gotjob:
	if (state != JOBRUNNING) {
		thisjob->changed = 1;

		if (thisjob->state != state) {
			TRACE(("Job %d: changing state from %d to %d\n",
				jobno(thisjob), thisjob->state, state));
			thisjob->state = state;
#if JOBS
			if (state == JOBSTOPPED) {
				set_curjob(thisjob, CUR_STOPPED);
			}
#endif
		}
	}

 out:
	INT_ON;

	if (thisjob && thisjob == job) {
		char s[48 + 1];
		int len;

		len = sprint_status(s, status, 1);
		if (len) {
			s[len] = '\n';
			s[len + 1] = '\0';
			out2str(s);
		}
	}
	return pid;
}

static int
blocking_wait_with_raise_on_sig(void)
{
	pid_t pid = dowait(DOWAIT_BLOCK, NULL);
	if (pid <= 0 && pending_sig)
		raise_exception(EXSIG);
	return pid;
}

#if JOBS
static void
showjob(FILE *out, struct job *jp, int mode)
{
	struct procstat *ps;
	struct procstat *psend;
	int col;
	int indent_col;
	char s[80];

	ps = jp->ps;

	if (mode & SHOW_ONLY_PGID) { /* jobs -p */
		/* just output process (group) id of pipeline */
		fprintf(out, "%d\n", ps->ps_pid);
		return;
	}

	col = fmtstr(s, 16, "[%d]   ", jobno(jp));
	indent_col = col;

	if (jp == curjob)
		s[col - 3] = '+';
	else if (curjob && jp == curjob->prev_job)
		s[col - 3] = '-';

	if (mode & SHOW_PIDS)
		col += fmtstr(s + col, 16, "%d ", ps->ps_pid);

	psend = ps + jp->nprocs;

	if (jp->state == JOBRUNNING) {
		strcpy(s + col, "Running");
		col += sizeof("Running") - 1;
	} else {
		int status = psend[-1].ps_status;
		if (jp->state == JOBSTOPPED)
			status = jp->stopstatus;
		col += sprint_status(s + col, status, 0);
	}
	/* By now, "[JOBID]*  [maybe PID] STATUS" is printed */

	/* This loop either prints "<cmd1> | <cmd2> | <cmd3>" line
	 * or prints several "PID             | <cmdN>" lines,
	 * depending on SHOW_PIDS bit.
	 * We do not print status of individual processes
	 * between PID and <cmdN>. bash does it, but not very well:
	 * first line shows overall job status, not process status,
	 * making it impossible to know 1st process status.
	 */
	goto start;
	do {
		/* for each process */
		s[0] = '\0';
		col = 33;
		if (mode & SHOW_PIDS)
			col = fmtstr(s, 48, "\n%*c%d ", indent_col, ' ', ps->ps_pid) - 1;
 start:
		fprintf(out, "%s%*c%s%s",
				s,
				33 - col >= 0 ? 33 - col : 0, ' ',
				ps == jp->ps ? "" : "| ",
				ps->ps_cmd
		);
	} while (++ps != psend);
	outcslow('\n', out);

	jp->changed = 0;

	if (jp->state == JOBDONE) {
		TRACE(("showjob: freeing job %d\n", jobno(jp)));
		freejob(jp);
	}
}

/*
 * Print a list of jobs.  If "change" is nonzero, only print jobs whose
 * statuses have changed since the last call to showjobs.
 */
static void
showjobs(FILE *out, int mode)
{
	struct job *jp;

	TRACE(("showjobs(0x%x) called\n", mode));

	/* Handle all finished jobs */
	while (dowait(DOWAIT_NONBLOCK, NULL) > 0)
		continue;

	for (jp = curjob; jp; jp = jp->prev_job) {
		if (!(mode & SHOW_CHANGED) || jp->changed) {
			showjob(out, jp, mode);
		}
	}
}

static int FAST_FUNC
jobscmd(int argc UNUSED_PARAM, char **argv)
{
	int mode, m;

	mode = 0;
	while ((m = nextopt("lp")) != '\0') {
		if (m == 'l')
			mode |= SHOW_PIDS;
		else
			mode |= SHOW_ONLY_PGID;
	}

	argv = argptr;
	if (*argv) {
		do
			showjob(stdout, getjob(*argv, 0), mode);
		while (*++argv);
	} else {
		showjobs(stdout, mode);
	}

	return 0;
}
#endif /* JOBS */

/* Called only on finished or stopped jobs (no members are running) */
static int
getstatus(struct job *job)
{
	int status;
	int retval;
	struct procstat *ps;

	/* Fetch last member's status */
	ps = job->ps + job->nprocs - 1;
	status = ps->ps_status;
	if (pipefail) {
		/* "set -o pipefail" mode: use last _nonzero_ status */
		while (status == 0 && --ps >= job->ps)
			status = ps->ps_status;
	}

	retval = WEXITSTATUS(status);
	if (!WIFEXITED(status)) {
#if JOBS
		retval = WSTOPSIG(status);
		if (!WIFSTOPPED(status))
#endif
		{
			/* XXX: limits number of signals */
			retval = WTERMSIG(status);
#if JOBS
			if (retval == SIGINT)
				job->sigint = 1;
#endif
		}
		retval += 128;
	}
	TRACE(("getstatus: job %d, nproc %d, status 0x%x, retval 0x%x\n",
		jobno(job), job->nprocs, status, retval));
	return retval;
}

static int FAST_FUNC
waitcmd(int argc UNUSED_PARAM, char **argv)
{
	struct job *job;
	int retval;
	struct job *jp;

	if (pending_sig)
		raise_exception(EXSIG);

	nextopt(nullstr);
	retval = 0;

	argv = argptr;
	if (!*argv) {
		/* wait for all jobs */
		for (;;) {
			jp = curjob;
			while (1) {
				if (!jp) /* no running procs */
					goto ret;
				if (jp->state == JOBRUNNING)
					break;
				jp->waited = 1;
				jp = jp->prev_job;
			}
			blocking_wait_with_raise_on_sig();
	/* man bash:
	 * "When bash is waiting for an asynchronous command via
	 * the wait builtin, the reception of a signal for which a trap
	 * has been set will cause the wait builtin to return immediately
	 * with an exit status greater than 128, immediately after which
	 * the trap is executed."
	 *
	 * blocking_wait_with_raise_on_sig raises signal handlers
	 * if it gets no pid (pid < 0). However,
	 * if child sends us a signal *and immediately exits*,
	 * blocking_wait_with_raise_on_sig gets pid > 0
	 * and does not handle pending_sig. Check this case: */
			if (pending_sig)
				raise_exception(EXSIG);
		}
	}

	retval = 127;
	do {
		if (**argv != '%') {
			pid_t pid = number(*argv);
			job = curjob;
			while (1) {
				if (!job)
					goto repeat;
				if (job->ps[job->nprocs - 1].ps_pid == pid)
					break;
				job = job->prev_job;
			}
		} else {
			job = getjob(*argv, 0);
		}
		/* loop until process terminated or stopped */
		while (job->state == JOBRUNNING)
			blocking_wait_with_raise_on_sig();
		job->waited = 1;
		retval = getstatus(job);
 repeat: ;
	} while (*++argv);

 ret:
	return retval;
}

static struct job *
growjobtab(void)
{
	size_t len;
	ptrdiff_t offset;
	struct job *jp, *jq;

	len = njobs * sizeof(*jp);
	jq = jobtab;
	jp = ckrealloc(jq, len + 4 * sizeof(*jp));

	offset = (char *)jp - (char *)jq;
	if (offset) {
		/* Relocate pointers */
		size_t l = len;

		jq = (struct job *)((char *)jq + l);
		while (l) {
			l -= sizeof(*jp);
			jq--;
#define joff(p) ((struct job *)((char *)(p) + l))
#define jmove(p) (p) = (void *)((char *)(p) + offset)
			if (joff(jp)->ps == &jq->ps0)
				jmove(joff(jp)->ps);
			if (joff(jp)->prev_job)
				jmove(joff(jp)->prev_job);
		}
		if (curjob)
			jmove(curjob);
#undef joff
#undef jmove
	}

	njobs += 4;
	jobtab = jp;
	jp = (struct job *)((char *)jp + len);
	jq = jp + 3;
	do {
		jq->used = 0;
	} while (--jq >= jp);
	return jp;
}

/*
 * Return a new job structure.
 * Called with interrupts off.
 */
static struct job *
makejob(/*union node *node,*/ int nprocs)
{
	int i;
	struct job *jp;

	for (i = njobs, jp = jobtab; ; jp++) {
		if (--i < 0) {
			jp = growjobtab();
			break;
		}
		if (jp->used == 0)
			break;
		if (jp->state != JOBDONE || !jp->waited)
			continue;
#if JOBS
		if (doing_jobctl)
			continue;
#endif
		freejob(jp);
		break;
	}
	memset(jp, 0, sizeof(*jp));
#if JOBS
	/* jp->jobctl is a bitfield.
	 * "jp->jobctl |= jobctl" likely to give awful code */
	if (doing_jobctl)
		jp->jobctl = 1;
#endif
	jp->prev_job = curjob;
	curjob = jp;
	jp->used = 1;
	jp->ps = &jp->ps0;
	if (nprocs > 1) {
		jp->ps = ckmalloc(nprocs * sizeof(struct procstat));
	}
	TRACE(("makejob(%d) returns %%%d\n", nprocs,
				jobno(jp)));
	return jp;
}

#if JOBS
/*
 * Return a string identifying a command (to be printed by the
 * jobs command).
 */
static char *cmdnextc;

static void
cmdputs(const char *s)
{
	static const char vstype[VSTYPE + 1][3] = {
		"", "}", "-", "+", "?", "=",
		"%", "%%", "#", "##"
		IF_ASH_BASH_COMPAT(, ":", "/", "//")
	};

	const char *p, *str;
	char cc[2];
	char *nextc;
	unsigned char c;
	unsigned char subtype = 0;
	int quoted = 0;

	cc[1] = '\0';
	nextc = makestrspace((strlen(s) + 1) * 8, cmdnextc);
	p = s;
	while ((c = *p++) != '\0') {
		str = NULL;
		switch (c) {
		case CTLESC:
			c = *p++;
			break;
		case CTLVAR:
			subtype = *p++;
			if ((subtype & VSTYPE) == VSLENGTH)
				str = "${#";
			else
				str = "${";
			if (!(subtype & VSQUOTE) == !(quoted & 1))
				goto dostr;
			quoted ^= 1;
			c = '"';
			break;
		case CTLENDVAR:
			str = "\"}" + !(quoted & 1);
			quoted >>= 1;
			subtype = 0;
			goto dostr;
		case CTLBACKQ:
			str = "$(...)";
			goto dostr;
		case CTLBACKQ+CTLQUOTE:
			str = "\"$(...)\"";
			goto dostr;
#if ENABLE_SH_MATH_SUPPORT
		case CTLARI:
			str = "$((";
			goto dostr;
		case CTLENDARI:
			str = "))";
			goto dostr;
#endif
		case CTLQUOTEMARK:
			quoted ^= 1;
			c = '"';
			break;
		case '=':
			if (subtype == 0)
				break;
			if ((subtype & VSTYPE) != VSNORMAL)
				quoted <<= 1;
			str = vstype[subtype & VSTYPE];
			if (subtype & VSNUL)
				c = ':';
			else
				goto checkstr;
			break;
		case '\'':
		case '\\':
		case '"':
		case '$':
			/* These can only happen inside quotes */
			cc[0] = c;
			str = cc;
			c = '\\';
			break;
		default:
			break;
		}
		USTPUTC(c, nextc);
 checkstr:
		if (!str)
			continue;
 dostr:
		while ((c = *str++) != '\0') {
			USTPUTC(c, nextc);
		}
	} /* while *p++ not NUL */

	if (quoted & 1) {
		USTPUTC('"', nextc);
	}
	*nextc = 0;
	cmdnextc = nextc;
}

/* cmdtxt() and cmdlist() call each other */
static void cmdtxt(union node *n);

static void
cmdlist(union node *np, int sep)
{
	for (; np; np = np->narg.next) {
		if (!sep)
			cmdputs(" ");
		cmdtxt(np);
		if (sep && np->narg.next)
			cmdputs(" ");
	}
}

static void
cmdtxt(union node *n)
{
	union node *np;
	struct nodelist *lp;
	const char *p;

	if (!n)
		return;
	switch (n->type) {
	default:
#if DEBUG
		abort();
#endif
	case NPIPE:
		lp = n->npipe.cmdlist;
		for (;;) {
			cmdtxt(lp->n);
			lp = lp->next;
			if (!lp)
				break;
			cmdputs(" | ");
		}
		break;
	case NSEMI:
		p = "; ";
		goto binop;
	case NAND:
		p = " && ";
		goto binop;
	case NOR:
		p = " || ";
 binop:
		cmdtxt(n->nbinary.ch1);
		cmdputs(p);
		n = n->nbinary.ch2;
		goto donode;
	case NREDIR:
	case NBACKGND:
		n = n->nredir.n;
		goto donode;
	case NNOT:
		cmdputs("!");
		n = n->nnot.com;
 donode:
		cmdtxt(n);
		break;
	case NIF:
		cmdputs("if ");
		cmdtxt(n->nif.test);
		cmdputs("; then ");
		if (n->nif.elsepart) {
			cmdtxt(n->nif.ifpart);
			cmdputs("; else ");
			n = n->nif.elsepart;
		} else {
			n = n->nif.ifpart;
		}
		p = "; fi";
		goto dotail;
	case NSUBSHELL:
		cmdputs("(");
		n = n->nredir.n;
		p = ")";
		goto dotail;
	case NWHILE:
		p = "while ";
		goto until;
	case NUNTIL:
		p = "until ";
 until:
		cmdputs(p);
		cmdtxt(n->nbinary.ch1);
		n = n->nbinary.ch2;
		p = "; done";
 dodo:
		cmdputs("; do ");
 dotail:
		cmdtxt(n);
		goto dotail2;
	case NFOR:
		cmdputs("for ");
		cmdputs(n->nfor.var);
		cmdputs(" in ");
		cmdlist(n->nfor.args, 1);
		n = n->nfor.body;
		p = "; done";
		goto dodo;
	case NDEFUN:
		cmdputs(n->narg.text);
		p = "() { ... }";
		goto dotail2;
	case NCMD:
		cmdlist(n->ncmd.args, 1);
		cmdlist(n->ncmd.redirect, 0);
		break;
	case NARG:
		p = n->narg.text;
 dotail2:
		cmdputs(p);
		break;
	case NHERE:
	case NXHERE:
		p = "<<...";
		goto dotail2;
	case NCASE:
		cmdputs("case ");
		cmdputs(n->ncase.expr->narg.text);
		cmdputs(" in ");
		for (np = n->ncase.cases; np; np = np->nclist.next) {
			cmdtxt(np->nclist.pattern);
			cmdputs(") ");
			cmdtxt(np->nclist.body);
			cmdputs(";; ");
		}
		p = "esac";
		goto dotail2;
	case NTO:
		p = ">";
		goto redir;
	case NCLOBBER:
		p = ">|";
		goto redir;
	case NAPPEND:
		p = ">>";
		goto redir;
#if ENABLE_ASH_BASH_COMPAT
	case NTO2:
#endif
	case NTOFD:
		p = ">&";
		goto redir;
	case NFROM:
		p = "<";
		goto redir;
	case NFROMFD:
		p = "<&";
		goto redir;
	case NFROMTO:
		p = "<>";
 redir:
		cmdputs(utoa(n->nfile.fd));
		cmdputs(p);
		if (n->type == NTOFD || n->type == NFROMFD) {
			cmdputs(utoa(n->ndup.dupfd));
			break;
		}
		n = n->nfile.fname;
		goto donode;
	}
}

static char *
commandtext(union node *n)
{
	char *name;

	STARTSTACKSTR(cmdnextc);
	cmdtxt(n);
	name = stackblock();
	TRACE(("commandtext: name %p, end %p\n\t\"%s\"\n",
			name, cmdnextc, cmdnextc));
	return ckstrdup(name);
}
#endif /* JOBS */

/*
 * Fork off a subshell.  If we are doing job control, give the subshell its
 * own process group.  Jp is a job structure that the job is to be added to.
 * N is the command that will be evaluated by the child.  Both jp and n may
 * be NULL.  The mode parameter can be one of the following:
 *      FORK_FG - Fork off a foreground process.
 *      FORK_BG - Fork off a background process.
 *      FORK_NOJOB - Like FORK_FG, but don't give the process its own
 *                   process group even if job control is on.
 *
 * When job control is turned off, background processes have their standard
 * input redirected to /dev/null (except for the second and later processes
 * in a pipeline).
 *
 * Called with interrupts off.
 */
/*
 * Clear traps on a fork.
 */
static void
clear_traps(void)
{
	char **tp;

	for (tp = trap; tp < &trap[NSIG]; tp++) {
		if (*tp && **tp) {      /* trap not NULL or "" (SIG_IGN) */
			INT_OFF;
			if (trap_ptr == trap)
				free(*tp);
			/* else: it "belongs" to trap_ptr vector, don't free */
			*tp = NULL;
			if ((tp - trap) != 0)
				setsignal(tp - trap);
			INT_ON;
		}
	}
	may_have_traps = 0;
}

/* Lives far away from here, needed for forkchild */
static void closescript(void);

/* Called after fork(), in child */
static NOINLINE void
forkchild(struct job *jp, union node *n, int mode)
{
	int oldlvl;

	TRACE(("Child shell %d\n", getpid()));
	oldlvl = shlvl;
	shlvl++;

	/* man bash: "Non-builtin commands run by bash have signal handlers
	 * set to the values inherited by the shell from its parent".
	 * Do we do it correctly? */

	closescript();

	if (mode == FORK_NOJOB          /* is it `xxx` ? */
	 && n && n->type == NCMD        /* is it single cmd? */
	/* && n->ncmd.args->type == NARG - always true? */
	 && n->ncmd.args && strcmp(n->ncmd.args->narg.text, "trap") == 0
	 && n->ncmd.args->narg.next == NULL /* "trap" with no arguments */
	/* && n->ncmd.args->narg.backquote == NULL - do we need to check this? */
	) {
		TRACE(("Trap hack\n"));
		/* Awful hack for `trap` or $(trap).
		 *
		 * http://www.opengroup.org/onlinepubs/009695399/utilities/trap.html
		 * contains an example where "trap" is executed in a subshell:
		 *
		 * save_traps=$(trap)
		 * ...
		 * eval "$save_traps"
		 *
		 * Standard does not say that "trap" in subshell shall print
		 * parent shell's traps. It only says that its output
		 * must have suitable form, but then, in the above example
		 * (which is not supposed to be normative), it implies that.
		 *
		 * bash (and probably other shell) does implement it
		 * (traps are reset to defaults, but "trap" still shows them),
		 * but as a result, "trap" logic is hopelessly messed up:
		 *
		 * # trap
		 * trap -- 'echo Ho' SIGWINCH  <--- we have a handler
		 * # (trap)        <--- trap is in subshell - no output (correct, traps are reset)
		 * # true | trap   <--- trap is in subshell - no output (ditto)
		 * # echo `true | trap`    <--- in subshell - output (but traps are reset!)
		 * trap -- 'echo Ho' SIGWINCH
		 * # echo `(trap)`         <--- in subshell in subshell - output
		 * trap -- 'echo Ho' SIGWINCH
		 * # echo `true | (trap)`  <--- in subshell in subshell in subshell - output!
		 * trap -- 'echo Ho' SIGWINCH
		 *
		 * The rules when to forget and when to not forget traps
		 * get really complex and nonsensical.
		 *
		 * Our solution: ONLY bare $(trap) or `trap` is special.
		 */
		/* Save trap handler strings for trap builtin to print */
		trap_ptr = memcpy(xmalloc(sizeof(trap)), trap, sizeof(trap));
		/* Fall through into clearing traps */
	}
	clear_traps();
#if JOBS
	/* do job control only in root shell */
	doing_jobctl = 0;
	if (mode != FORK_NOJOB && jp->jobctl && oldlvl == 0) {
		pid_t pgrp;

		if (jp->nprocs == 0)
			pgrp = getpid();
		else
			pgrp = jp->ps[0].ps_pid;
		/* this can fail because we are doing it in the parent also */
		setpgid(0, pgrp);
		if (mode == FORK_FG)
			xtcsetpgrp(ttyfd, pgrp);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
	} else
#endif
	if (mode == FORK_BG) {
		/* man bash: "When job control is not in effect,
		 * asynchronous commands ignore SIGINT and SIGQUIT" */
		ignoresig(SIGINT);
		ignoresig(SIGQUIT);
		if (jp->nprocs == 0) {
			close(0);
			if (open(bb_dev_null, O_RDONLY) != 0)
				ash_msg_and_raise_error("can't open '%s'", bb_dev_null);
		}
	}
	if (oldlvl == 0) {
		if (iflag) { /* why if iflag only? */
			setsignal(SIGINT);
			setsignal(SIGTERM);
		}
		/* man bash:
		 * "In all cases, bash ignores SIGQUIT. Non-builtin
		 * commands run by bash have signal handlers
		 * set to the values inherited by the shell
		 * from its parent".
		 * Take care of the second rule: */
		setsignal(SIGQUIT);
	}
#if JOBS
	if (n && n->type == NCMD
	 && n->ncmd.args && strcmp(n->ncmd.args->narg.text, "jobs") == 0
	) {
		TRACE(("Job hack\n"));
		/* "jobs": we do not want to clear job list for it,
		 * instead we remove only _its_ own_ job from job list.
		 * This makes "jobs .... | cat" more useful.
		 */
		freejob(curjob);
		return;
	}
#endif
	for (jp = curjob; jp; jp = jp->prev_job)
		freejob(jp);
	jobless = 0;
}

/* Called after fork(), in parent */
#if !JOBS
#define forkparent(jp, n, mode, pid) forkparent(jp, mode, pid)
#endif
static void
forkparent(struct job *jp, union node *n, int mode, pid_t pid)
{
	TRACE(("In parent shell: child = %d\n", pid));
	if (!jp) {
		while (jobless && dowait(DOWAIT_NONBLOCK, NULL) > 0)
			continue;
		jobless++;
		return;
	}
#if JOBS
	if (mode != FORK_NOJOB && jp->jobctl) {
		int pgrp;

		if (jp->nprocs == 0)
			pgrp = pid;
		else
			pgrp = jp->ps[0].ps_pid;
		/* This can fail because we are doing it in the child also */
		setpgid(pid, pgrp);
	}
#endif
	if (mode == FORK_BG) {
		backgndpid = pid;               /* set $! */
		set_curjob(jp, CUR_RUNNING);
	}
	if (jp) {
		struct procstat *ps = &jp->ps[jp->nprocs++];
		ps->ps_pid = pid;
		ps->ps_status = -1;
		ps->ps_cmd = nullstr;
#if JOBS
		if (doing_jobctl && n)
			ps->ps_cmd = commandtext(n);
#endif
	}
}

static int
forkshell(struct job *jp, union node *n, int mode)
{
	int pid;

	TRACE(("forkshell(%%%d, %p, %d) called\n", jobno(jp), n, mode));
	pid = fork();
	if (pid < 0) {
		TRACE(("Fork failed, errno=%d", errno));
		if (jp)
			freejob(jp);
		ash_msg_and_raise_error("can't fork");
	}
	if (pid == 0) {
		CLEAR_RANDOM_T(&random_gen); /* or else $RANDOM repeats in child */
		forkchild(jp, n, mode);
	} else {
		forkparent(jp, n, mode, pid);
	}
	return pid;
}

/*
 * Wait for job to finish.
 *
 * Under job control we have the problem that while a child process
 * is running interrupts generated by the user are sent to the child
 * but not to the shell.  This means that an infinite loop started by
 * an interactive user may be hard to kill.  With job control turned off,
 * an interactive user may place an interactive program inside a loop.
 * If the interactive program catches interrupts, the user doesn't want
 * these interrupts to also abort the loop.  The approach we take here
 * is to have the shell ignore interrupt signals while waiting for a
 * foreground process to terminate, and then send itself an interrupt
 * signal if the child process was terminated by an interrupt signal.
 * Unfortunately, some programs want to do a bit of cleanup and then
 * exit on interrupt; unless these processes terminate themselves by
 * sending a signal to themselves (instead of calling exit) they will
 * confuse this approach.
 *
 * Called with interrupts off.
 */
static int
waitforjob(struct job *jp)
{
	int st;

	TRACE(("waitforjob(%%%d) called\n", jobno(jp)));

	INT_OFF;
	while (jp->state == JOBRUNNING) {
		/* In non-interactive shells, we _can_ get
		 * a keyboard signal here and be EINTRed,
		 * but we just loop back, waiting for command to complete.
		 *
		 * man bash:
		 * "If bash is waiting for a command to complete and receives
		 * a signal for which a trap has been set, the trap
		 * will not be executed until the command completes."
		 *
		 * Reality is that even if trap is not set, bash
		 * will not act on the signal until command completes.
		 * Try this. sleep5intoff.c:
		 * #include <signal.h>
		 * #include <unistd.h>
		 * int main() {
		 *         sigset_t set;
		 *         sigemptyset(&set);
		 *         sigaddset(&set, SIGINT);
		 *         sigaddset(&set, SIGQUIT);
		 *         sigprocmask(SIG_BLOCK, &set, NULL);
		 *         sleep(5);
		 *         return 0;
		 * }
		 * $ bash -c './sleep5intoff; echo hi'
		 * ^C^C^C^C <--- pressing ^C once a second
		 * $ _
		 * $ bash -c './sleep5intoff; echo hi'
		 * ^\^\^\^\hi <--- pressing ^\ (SIGQUIT)
		 * $ _
		 */
		dowait(DOWAIT_BLOCK, jp);
	}
	INT_ON;

	st = getstatus(jp);
#if JOBS
	if (jp->jobctl) {
		xtcsetpgrp(ttyfd, rootpid);
		/*
		 * This is truly gross.
		 * If we're doing job control, then we did a TIOCSPGRP which
		 * caused us (the shell) to no longer be in the controlling
		 * session -- so we wouldn't have seen any ^C/SIGINT.  So, we
		 * intuit from the subprocess exit status whether a SIGINT
		 * occurred, and if so interrupt ourselves.  Yuck.  - mycroft
		 */
		if (jp->sigint) /* TODO: do the same with all signals */
			raise(SIGINT); /* ... by raise(jp->sig) instead? */
	}
	if (jp->state == JOBDONE)
#endif
		freejob(jp);
	return st;
}

/*
 * return 1 if there are stopped jobs, otherwise 0
 */
static int
stoppedjobs(void)
{
	struct job *jp;
	int retval;

	retval = 0;
	if (job_warning)
		goto out;
	jp = curjob;
	if (jp && jp->state == JOBSTOPPED) {
		out2str("You have stopped jobs.\n");
		job_warning = 2;
		retval++;
	}
 out:
	return retval;
}


/* ============ redir.c
 *
 * Code for dealing with input/output redirection.
 */

#undef EMPTY
#undef CLOSED
#define EMPTY -2                /* marks an unused slot in redirtab */
#define CLOSED -3               /* marks a slot of previously-closed fd */

/*
 * Open a file in noclobber mode.
 * The code was copied from bash.
 */
static int
noclobberopen(const char *fname)
{
	int r, fd;
	struct stat finfo, finfo2;

	/*
	 * If the file exists and is a regular file, return an error
	 * immediately.
	 */
	r = stat(fname, &finfo);
	if (r == 0 && S_ISREG(finfo.st_mode)) {
		errno = EEXIST;
		return -1;
	}

	/*
	 * If the file was not present (r != 0), make sure we open it
	 * exclusively so that if it is created before we open it, our open
	 * will fail.  Make sure that we do not truncate an existing file.
	 * Note that we don't turn on O_EXCL unless the stat failed -- if the
	 * file was not a regular file, we leave O_EXCL off.
	 */
	if (r != 0)
		return open(fname, O_WRONLY|O_CREAT|O_EXCL, 0666);
	fd = open(fname, O_WRONLY|O_CREAT, 0666);

	/* If the open failed, return the file descriptor right away. */
	if (fd < 0)
		return fd;

	/*
	 * OK, the open succeeded, but the file may have been changed from a
	 * non-regular file to a regular file between the stat and the open.
	 * We are assuming that the O_EXCL open handles the case where FILENAME
	 * did not exist and is symlinked to an existing file between the stat
	 * and open.
	 */

	/*
	 * If we can open it and fstat the file descriptor, and neither check
	 * revealed that it was a regular file, and the file has not been
	 * replaced, return the file descriptor.
	 */
	if (fstat(fd, &finfo2) == 0
	 && !S_ISREG(finfo2.st_mode)
	 && finfo.st_dev == finfo2.st_dev
	 && finfo.st_ino == finfo2.st_ino
	) {
		return fd;
	}

	/* The file has been replaced.  badness. */
	close(fd);
	errno = EEXIST;
	return -1;
}

/*
 * Handle here documents.  Normally we fork off a process to write the
 * data to a pipe.  If the document is short, we can stuff the data in
 * the pipe without forking.
 */
/* openhere needs this forward reference */
static void expandhere(union node *arg, int fd);
static int
openhere(union node *redir)
{
	int pip[2];
	size_t len = 0;

	if (pipe(pip) < 0)
		ash_msg_and_raise_error("pipe call failed");
	if (redir->type == NHERE) {
		len = strlen(redir->nhere.doc->narg.text);
		if (len <= PIPE_BUF) {
			full_write(pip[1], redir->nhere.doc->narg.text, len);
			goto out;
		}
	}
	if (forkshell((struct job *)NULL, (union node *)NULL, FORK_NOJOB) == 0) {
		/* child */
		close(pip[0]);
		ignoresig(SIGINT);  //signal(SIGINT, SIG_IGN);
		ignoresig(SIGQUIT); //signal(SIGQUIT, SIG_IGN);
		ignoresig(SIGHUP);  //signal(SIGHUP, SIG_IGN);
		ignoresig(SIGTSTP); //signal(SIGTSTP, SIG_IGN);
		signal(SIGPIPE, SIG_DFL);
		if (redir->type == NHERE)
			full_write(pip[1], redir->nhere.doc->narg.text, len);
		else /* NXHERE */
			expandhere(redir->nhere.doc, pip[1]);
		_exit(EXIT_SUCCESS);
	}
 out:
	close(pip[1]);
	return pip[0];
}

static int
openredirect(union node *redir)
{
	char *fname;
	int f;

	switch (redir->nfile.type) {
	case NFROM:
		fname = redir->nfile.expfname;
		f = open(fname, O_RDONLY);
		if (f < 0)
			goto eopen;
		break;
	case NFROMTO:
		fname = redir->nfile.expfname;
		f = open(fname, O_RDWR|O_CREAT, 0666);
		if (f < 0)
			goto ecreate;
		break;
	case NTO:
#if ENABLE_ASH_BASH_COMPAT
	case NTO2:
#endif
		/* Take care of noclobber mode. */
		if (Cflag) {
			fname = redir->nfile.expfname;
			f = noclobberopen(fname);
			if (f < 0)
				goto ecreate;
			break;
		}
		/* FALLTHROUGH */
	case NCLOBBER:
		fname = redir->nfile.expfname;
		f = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (f < 0)
			goto ecreate;
		break;
	case NAPPEND:
		fname = redir->nfile.expfname;
		f = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);
		if (f < 0)
			goto ecreate;
		break;
	default:
#if DEBUG
		abort();
#endif
		/* Fall through to eliminate warning. */
/* Our single caller does this itself */
//	case NTOFD:
//	case NFROMFD:
//		f = -1;
//		break;
	case NHERE:
	case NXHERE:
		f = openhere(redir);
		break;
	}

	return f;
 ecreate:
	ash_msg_and_raise_error("can't create %s: %s", fname, errmsg(errno, "nonexistent directory"));
 eopen:
	ash_msg_and_raise_error("can't open %s: %s", fname, errmsg(errno, "no such file"));
}

/*
 * Copy a file descriptor to be >= to.  Returns -1
 * if the source file descriptor is closed, EMPTY if there are no unused
 * file descriptors left.
 */
/* 0x800..00: bit to set in "to" to request dup2 instead of fcntl(F_DUPFD).
 * old code was doing close(to) prior to copyfd() to achieve the same */
enum {
	COPYFD_EXACT   = (int)~(INT_MAX),
	COPYFD_RESTORE = (int)((unsigned)COPYFD_EXACT >> 1),
};
static int
copyfd(int from, int to)
{
	int newfd;

	if (to & COPYFD_EXACT) {
		to &= ~COPYFD_EXACT;
		/*if (from != to)*/
			newfd = dup2(from, to);
	} else {
		newfd = fcntl(from, F_DUPFD, to);
	}
	if (newfd < 0) {
		if (errno == EMFILE)
			return EMPTY;
		/* Happens when source fd is not open: try "echo >&99" */
		ash_msg_and_raise_error("%d: %m", from);
	}
	return newfd;
}

/* Struct def and variable are moved down to the first usage site */
struct two_fd_t {
	int orig, copy;
};
struct redirtab {
	struct redirtab *next;
	int nullredirs;
	int pair_count;
	struct two_fd_t two_fd[];
};
#define redirlist (G_var.redirlist)

static int need_to_remember(struct redirtab *rp, int fd)
{
	int i;

	if (!rp) /* remembering was not requested */
		return 0;

	for (i = 0; i < rp->pair_count; i++) {
		if (rp->two_fd[i].orig == fd) {
			/* already remembered */
			return 0;
		}
	}
	return 1;
}

/* "hidden" fd is a fd used to read scripts, or a copy of such */
static int is_hidden_fd(struct redirtab *rp, int fd)
{
	int i;
	struct parsefile *pf;

	if (fd == -1)
		return 0;
	/* Check open scripts' fds */
	pf = g_parsefile;
	while (pf) {
		/* We skip pf_fd == 0 case because of the following case:
		 * $ ash  # running ash interactively
		 * $ . ./script.sh
		 * and in script.sh: "exec 9>&0".
		 * Even though top-level pf_fd _is_ 0,
		 * it's still ok to use it: "read" builtin uses it,
		 * why should we cripple "exec" builtin?
		 */
		if (pf->pf_fd > 0 && fd == pf->pf_fd) {
			return 1;
		}
		pf = pf->prev;
	}

	if (!rp)
		return 0;
	/* Check saved fds of redirects */
	fd |= COPYFD_RESTORE;
	for (i = 0; i < rp->pair_count; i++) {
		if (rp->two_fd[i].copy == fd) {
			return 1;
		}
	}
	return 0;
}

/*
 * Process a list of redirection commands.  If the REDIR_PUSH flag is set,
 * old file descriptors are stashed away so that the redirection can be
 * undone by calling popredir.
 */
/* flags passed to redirect */
#define REDIR_PUSH    01        /* save previous values of file descriptors */
#define REDIR_SAVEFD2 03        /* set preverrout */
static void
redirect(union node *redir, int flags)
{
	struct redirtab *sv;
	int sv_pos;
	int i;
	int fd;
	int newfd;
	int copied_fd2 = -1;

	g_nullredirs++;
	if (!redir) {
		return;
	}

	sv = NULL;
	sv_pos = 0;
	INT_OFF;
	if (flags & REDIR_PUSH) {
		union node *tmp = redir;
		do {
			sv_pos++;
#if ENABLE_ASH_BASH_COMPAT
			if (tmp->nfile.type == NTO2)
				sv_pos++;
#endif
			tmp = tmp->nfile.next;
		} while (tmp);
		sv = ckmalloc(sizeof(*sv) + sv_pos * sizeof(sv->two_fd[0]));
		sv->next = redirlist;
		sv->pair_count = sv_pos;
		redirlist = sv;
		sv->nullredirs = g_nullredirs - 1;
		g_nullredirs = 0;
		while (sv_pos > 0) {
			sv_pos--;
			sv->two_fd[sv_pos].orig = sv->two_fd[sv_pos].copy = EMPTY;
		}
	}

	do {
		int right_fd = -1;
		fd = redir->nfile.fd;
		if (redir->nfile.type == NTOFD || redir->nfile.type == NFROMFD) {
			right_fd = redir->ndup.dupfd;
			//bb_error_msg("doing %d > %d", fd, right_fd);
			/* redirect from/to same file descriptor? */
			if (right_fd == fd)
				continue;
			/* "echo >&10" and 10 is a fd opened to a sh script? */
			if (is_hidden_fd(sv, right_fd)) {
				errno = EBADF; /* as if it is closed */
				ash_msg_and_raise_error("%d: %m", right_fd);
			}
			newfd = -1;
		} else {
			newfd = openredirect(redir); /* always >= 0 */
			if (fd == newfd) {
				/* Descriptor wasn't open before redirect.
				 * Mark it for close in the future */
				if (need_to_remember(sv, fd)) {
					goto remember_to_close;
				}
				continue;
			}
		}
#if ENABLE_ASH_BASH_COMPAT
 redirect_more:
#endif
		if (need_to_remember(sv, fd)) {
			/* Copy old descriptor */
			/* Careful to not accidentally "save"
			 * to the same fd as right side fd in N>&M */
			int minfd = right_fd < 10 ? 10 : right_fd + 1;
			i = fcntl(fd, F_DUPFD, minfd);
/* You'd expect copy to be CLOEXECed. Currently these extra "saved" fds
 * are closed in popredir() in the child, preventing them from leaking
 * into child. (popredir() also cleans up the mess in case of failures)
 */
			if (i == -1) {
				i = errno;
				if (i != EBADF) {
					/* Strange error (e.g. "too many files" EMFILE?) */
					if (newfd >= 0)
						close(newfd);
					errno = i;
					ash_msg_and_raise_error("%d: %m", fd);
					/* NOTREACHED */
				}
				/* EBADF: it is not open - good, remember to close it */
 remember_to_close:
				i = CLOSED;
			} else { /* fd is open, save its copy */
				/* "exec fd>&-" should not close fds
				 * which point to script file(s).
				 * Force them to be restored afterwards */
				if (is_hidden_fd(sv, fd))
					i |= COPYFD_RESTORE;
			}
			if (fd == 2)
				copied_fd2 = i;
			sv->two_fd[sv_pos].orig = fd;
			sv->two_fd[sv_pos].copy = i;
			sv_pos++;
		}
		if (newfd < 0) {
			/* NTOFD/NFROMFD: copy redir->ndup.dupfd to fd */
			if (redir->ndup.dupfd < 0) { /* "fd>&-" */
				/* Don't want to trigger debugging */
				if (fd != -1)
					close(fd);
			} else {
				copyfd(redir->ndup.dupfd, fd | COPYFD_EXACT);
			}
		} else if (fd != newfd) { /* move newfd to fd */
			copyfd(newfd, fd | COPYFD_EXACT);
#if ENABLE_ASH_BASH_COMPAT
			if (!(redir->nfile.type == NTO2 && fd == 2))
#endif
				close(newfd);
		}
#if ENABLE_ASH_BASH_COMPAT
		if (redir->nfile.type == NTO2 && fd == 1) {
			/* We already redirected it to fd 1, now copy it to 2 */
			newfd = 1;
			fd = 2;
			goto redirect_more;
		}
#endif
	} while ((redir = redir->nfile.next) != NULL);

	INT_ON;
	if ((flags & REDIR_SAVEFD2) && copied_fd2 >= 0)
		preverrout_fd = copied_fd2;
}

/*
 * Undo the effects of the last redirection.
 */
static void
popredir(int drop, int restore)
{
	struct redirtab *rp;
	int i;

	if (--g_nullredirs >= 0)
		return;
	INT_OFF;
	rp = redirlist;
	for (i = 0; i < rp->pair_count; i++) {
		int fd = rp->two_fd[i].orig;
		int copy = rp->two_fd[i].copy;
		if (copy == CLOSED) {
			if (!drop)
				close(fd);
			continue;
		}
		if (copy != EMPTY) {
			if (!drop || (restore && (copy & COPYFD_RESTORE))) {
				copy &= ~COPYFD_RESTORE;
				/*close(fd);*/
				copyfd(copy, fd | COPYFD_EXACT);
			}
			close(copy & ~COPYFD_RESTORE);
		}
	}
	redirlist = rp->next;
	g_nullredirs = rp->nullredirs;
	free(rp);
	INT_ON;
}

/*
 * Undo all redirections.  Called on error or interrupt.
 */

/*
 * Discard all saved file descriptors.
 */
static void
clearredir(int drop)
{
	for (;;) {
		g_nullredirs = 0;
		if (!redirlist)
			break;
		popredir(drop, /*restore:*/ 0);
	}
}

static int
redirectsafe(union node *redir, int flags)
{
	int err;
	volatile int saveint;
	struct jmploc *volatile savehandler = exception_handler;
	struct jmploc jmploc;

	SAVE_INT(saveint);
	/* "echo 9>/dev/null; echo >&9; echo result: $?" - result should be 1, not 2! */
	err = setjmp(jmploc.loc); // huh?? was = setjmp(jmploc.loc) * 2;
	if (!err) {
		exception_handler = &jmploc;
		redirect(redir, flags);
	}
	exception_handler = savehandler;
	if (err && exception_type != EXERROR)
		longjmp(exception_handler->loc, 1);
	RESTORE_INT(saveint);
	return err;
}


/* ============ Routines to expand arguments to commands
 *
 * We have to deal with backquotes, shell variables, and file metacharacters.
 */

#if ENABLE_SH_MATH_SUPPORT
static arith_t
ash_arith(const char *s)
{
	arith_state_t math_state;
	arith_t result;

	math_state.lookupvar = lookupvar;
	math_state.setvar    = setvar2;
	//math_state.endofname = endofname;

	INT_OFF;
	result = arith(&math_state, s);
	if (math_state.errmsg)
		ash_msg_and_raise_error(math_state.errmsg);
	INT_ON;

	return result;
}
#endif

/*
 * expandarg flags
 */
#define EXP_FULL        0x1     /* perform word splitting & file globbing */
#define EXP_TILDE       0x2     /* do normal tilde expansion */
#define EXP_VARTILDE    0x4     /* expand tildes in an assignment */
#define EXP_REDIR       0x8     /* file glob for a redirection (1 match only) */
#define EXP_CASE        0x10    /* keeps quotes around for CASE pattern */
#define EXP_RECORD      0x20    /* need to record arguments for ifs breakup */
#define EXP_VARTILDE2   0x40    /* expand tildes after colons only */
#define EXP_WORD        0x80    /* expand word in parameter expansion */
#define EXP_QWORD       0x100   /* expand word in quoted parameter expansion */
/*
 * rmescape() flags
 */
#define RMESCAPE_ALLOC  0x1     /* Allocate a new string */
#define RMESCAPE_GLOB   0x2     /* Add backslashes for glob */
#define RMESCAPE_QUOTED 0x4     /* Remove CTLESC unless in quotes */
#define RMESCAPE_GROW   0x8     /* Grow strings instead of stalloc */
#define RMESCAPE_HEAP   0x10    /* Malloc strings instead of stalloc */

/*
 * Structure specifying which parts of the string should be searched
 * for IFS characters.
 */
struct ifsregion {
	struct ifsregion *next; /* next region in list */
	int begoff;             /* offset of start of region */
	int endoff;             /* offset of end of region */
	int nulonly;            /* search for nul bytes only */
};

struct arglist {
	struct strlist *list;
	struct strlist **lastp;
};

/* output of current string */
static char *expdest;
/* list of back quote expressions */
static struct nodelist *argbackq;
/* first struct in list of ifs regions */
static struct ifsregion ifsfirst;
/* last struct in list */
static struct ifsregion *ifslastp;
/* holds expanded arg list */
static struct arglist exparg;

/*
 * Our own itoa().
 */
#if !ENABLE_SH_MATH_SUPPORT
/* cvtnum() is used even if math support is off (to prepare $? values and such) */
typedef long arith_t;
# define ARITH_FMT "%ld"
#endif
static int
cvtnum(arith_t num)
{
	int len;

	expdest = makestrspace(32, expdest);
	len = fmtstr(expdest, 32, ARITH_FMT, num);
	STADJUST(len, expdest);
	return len;
}

static size_t
esclen(const char *start, const char *p)
{
	size_t esc = 0;

	while (p > start && (unsigned char)*--p == CTLESC) {
		esc++;
	}
	return esc;
}

/*
 * Remove any CTLESC characters from a string.
 */
static char *
rmescapes(char *str, int flag)
{
	static const char qchars[] ALIGN1 = { CTLESC, CTLQUOTEMARK, '\0' };

	char *p, *q, *r;
	unsigned inquotes;
	unsigned protect_against_glob;
	unsigned globbing;

	p = strpbrk(str, qchars);
	if (!p)
		return str;

	q = p;
	r = str;
	if (flag & RMESCAPE_ALLOC) {
		size_t len = p - str;
		size_t fulllen = len + strlen(p) + 1;

		if (flag & RMESCAPE_GROW) {
			int strloc = str - (char *)stackblock();
			r = makestrspace(fulllen, expdest);
			/* p and str may be invalidated by makestrspace */
			str = (char *)stackblock() + strloc;
			p = str + len;
		} else if (flag & RMESCAPE_HEAP) {
			r = ckmalloc(fulllen);
		} else {
			r = stalloc(fulllen);
		}
		q = r;
		if (len > 0) {
			q = (char *)memcpy(q, str, len) + len;
		}
	}

	inquotes = (flag & RMESCAPE_QUOTED) ^ RMESCAPE_QUOTED;
	globbing = flag & RMESCAPE_GLOB;
	protect_against_glob = globbing;
	while (*p) {
		if ((unsigned char)*p == CTLQUOTEMARK) {
// TODO: if no RMESCAPE_QUOTED in flags, inquotes never becomes 0
// (alternates between RMESCAPE_QUOTED and ~RMESCAPE_QUOTED). Is it ok?
// Note: both inquotes and protect_against_glob only affect whether
// CTLESC,<ch> gets converted to <ch> or to \<ch>
			inquotes = ~inquotes;
			p++;
			protect_against_glob = globbing;
			continue;
		}
		if (*p == '\\') {
			/* naked back slash */
			protect_against_glob = 0;
			goto copy;
		}
		if ((unsigned char)*p == CTLESC) {
			p++;
			if (protect_against_glob && inquotes && *p != '/') {
				*q++ = '\\';
			}
		}
		protect_against_glob = globbing;
 copy:
		*q++ = *p++;
	}
	*q = '\0';
	if (flag & RMESCAPE_GROW) {
		expdest = r;
		STADJUST(q - r + 1, expdest);
	}
	return r;
}
#define pmatch(a, b) !fnmatch((a), (b), 0)

/*
 * Prepare a pattern for a expmeta (internal glob(3)) call.
 *
 * Returns an stalloced string.
 */
static char *
preglob(const char *pattern, int quoted, int flag)
{
	flag |= RMESCAPE_GLOB;
	if (quoted) {
		flag |= RMESCAPE_QUOTED;
	}
	return rmescapes((char *)pattern, flag);
}

/*
 * Put a string on the stack.
 */
static void
memtodest(const char *p, size_t len, int syntax, int quotes)
{
	char *q = expdest;

	q = makestrspace(quotes ? len * 2 : len, q);

	while (len--) {
		unsigned char c = *p++;
		if (c == '\0')
			continue;
		if (quotes) {
			int n = SIT(c, syntax);
			if (n == CCTL || n == CBACK)
				USTPUTC(CTLESC, q);
		}
		USTPUTC(c, q);
	}

	expdest = q;
}

static void
strtodest(const char *p, int syntax, int quotes)
{
	memtodest(p, strlen(p), syntax, quotes);
}

/*
 * Record the fact that we have to scan this region of the
 * string for IFS characters.
 */
static void
recordregion(int start, int end, int nulonly)
{
	struct ifsregion *ifsp;

	if (ifslastp == NULL) {
		ifsp = &ifsfirst;
	} else {
		INT_OFF;
		ifsp = ckzalloc(sizeof(*ifsp));
		/*ifsp->next = NULL; - ckzalloc did it */
		ifslastp->next = ifsp;
		INT_ON;
	}
	ifslastp = ifsp;
	ifslastp->begoff = start;
	ifslastp->endoff = end;
	ifslastp->nulonly = nulonly;
}

static void
removerecordregions(int endoff)
{
	if (ifslastp == NULL)
		return;

	if (ifsfirst.endoff > endoff) {
		while (ifsfirst.next) {
			struct ifsregion *ifsp;
			INT_OFF;
			ifsp = ifsfirst.next->next;
			free(ifsfirst.next);
			ifsfirst.next = ifsp;
			INT_ON;
		}
		if (ifsfirst.begoff > endoff) {
			ifslastp = NULL;
		} else {
			ifslastp = &ifsfirst;
			ifsfirst.endoff = endoff;
		}
		return;
	}

	ifslastp = &ifsfirst;
	while (ifslastp->next && ifslastp->next->begoff < endoff)
		ifslastp = ifslastp->next;
	while (ifslastp->next) {
		struct ifsregion *ifsp;
		INT_OFF;
		ifsp = ifslastp->next->next;
		free(ifslastp->next);
		ifslastp->next = ifsp;
		INT_ON;
	}
	if (ifslastp->endoff > endoff)
		ifslastp->endoff = endoff;
}

static char *
exptilde(char *startp, char *p, int flags)
{
	unsigned char c;
	char *name;
	struct passwd *pw;
	const char *home;
	int quotes = flags & (EXP_FULL | EXP_CASE | EXP_REDIR);
	int startloc;

	name = p + 1;

	while ((c = *++p) != '\0') {
		switch (c) {
		case CTLESC:
			return startp;
		case CTLQUOTEMARK:
			return startp;
		case ':':
			if (flags & EXP_VARTILDE)
				goto done;
			break;
		case '/':
		case CTLENDVAR:
			goto done;
		}
	}
 done:
	*p = '\0';
	if (*name == '\0') {
		home = lookupvar("HOME");
	} else {
		pw = getpwnam(name);
		if (pw == NULL)
			goto lose;
		home = pw->pw_dir;
	}
	if (!home || !*home)
		goto lose;
	*p = c;
	startloc = expdest - (char *)stackblock();
	strtodest(home, SQSYNTAX, quotes);
	recordregion(startloc, expdest - (char *)stackblock(), 0);
	return p;
 lose:
	*p = c;
	return startp;
}

/*
 * Execute a command inside back quotes.  If it's a builtin command, we
 * want to save its output in a block obtained from malloc.  Otherwise
 * we fork off a subprocess and get the output of the command via a pipe.
 * Should be called with interrupts off.
 */
struct backcmd {                /* result of evalbackcmd */
	int fd;                 /* file descriptor to read from */
	int nleft;              /* number of chars in buffer */
	char *buf;              /* buffer */
	struct job *jp;         /* job structure for command */
};

/* These forward decls are needed to use "eval" code for backticks handling: */
static uint8_t back_exitstatus; /* exit status of backquoted command */
#define EV_EXIT 01              /* exit after evaluating tree */
static void evaltree(union node *, int);

static void FAST_FUNC
evalbackcmd(union node *n, struct backcmd *result)
{
	int saveherefd;

	result->fd = -1;
	result->buf = NULL;
	result->nleft = 0;
	result->jp = NULL;
	if (n == NULL)
		goto out;

	saveherefd = herefd;
	herefd = -1;

	{
		int pip[2];
		struct job *jp;

		if (pipe(pip) < 0)
			ash_msg_and_raise_error("pipe call failed");
		jp = makejob(/*n,*/ 1);
		if (forkshell(jp, n, FORK_NOJOB) == 0) {
			FORCE_INT_ON;
			close(pip[0]);
			if (pip[1] != 1) {
				/*close(1);*/
				copyfd(pip[1], 1 | COPYFD_EXACT);
				close(pip[1]);
			}
			eflag = 0;
			evaltree(n, EV_EXIT); /* actually evaltreenr... */
			/* NOTREACHED */
		}
		close(pip[1]);
		result->fd = pip[0];
		result->jp = jp;
	}
	herefd = saveherefd;
 out:
	TRACE(("evalbackcmd done: fd=%d buf=0x%x nleft=%d jp=0x%x\n",
		result->fd, result->buf, result->nleft, result->jp));
}

/*
 * Expand stuff in backwards quotes.
 */
static void
expbackq(union node *cmd, int quoted, int quotes)
{
	struct backcmd in;
	int i;
	char buf[128];
	char *p;
	char *dest;
	int startloc;
	int syntax = quoted ? DQSYNTAX : BASESYNTAX;
	struct stackmark smark;

	INT_OFF;
	setstackmark(&smark);
	dest = expdest;
	startloc = dest - (char *)stackblock();
	grabstackstr(dest);
	evalbackcmd(cmd, &in);
	popstackmark(&smark);

	p = in.buf;
	i = in.nleft;
	if (i == 0)
		goto read;
	for (;;) {
		memtodest(p, i, syntax, quotes);
 read:
		if (in.fd < 0)
			break;
		i = nonblock_immune_read(in.fd, buf, sizeof(buf), /*loop_on_EINTR:*/ 1);
		TRACE(("expbackq: read returns %d\n", i));
		if (i <= 0)
			break;
		p = buf;
	}

	free(in.buf);
	if (in.fd >= 0) {
		close(in.fd);
		back_exitstatus = waitforjob(in.jp);
	}
	INT_ON;

	/* Eat all trailing newlines */
	dest = expdest;
	for (; dest > (char *)stackblock() && dest[-1] == '\n';)
		STUNPUTC(dest);
	expdest = dest;

	if (quoted == 0)
		recordregion(startloc, dest - (char *)stackblock(), 0);
	TRACE(("evalbackq: size:%d:'%.*s'\n",
		(int)((dest - (char *)stackblock()) - startloc),
		(int)((dest - (char *)stackblock()) - startloc),
		stackblock() + startloc));
}

#if ENABLE_SH_MATH_SUPPORT
/*
 * Expand arithmetic expression.  Backup to start of expression,
 * evaluate, place result in (backed up) result, adjust string position.
 */
static void
expari(int quotes)
{
	char *p, *start;
	int begoff;
	int flag;
	int len;

	/* ifsfree(); */

	/*
	 * This routine is slightly over-complicated for
	 * efficiency.  Next we scan backwards looking for the
	 * start of arithmetic.
	 */
	start = stackblock();
	p = expdest - 1;
	*p = '\0';
	p--;
	while (1) {
		int esc;

		while ((unsigned char)*p != CTLARI) {
			p--;
#if DEBUG
			if (p < start) {
				ash_msg_and_raise_error("missing CTLARI (shouldn't happen)");
			}
#endif
		}

		esc = esclen(start, p);
		if (!(esc % 2)) {
			break;
		}

		p -= esc + 1;
	}

	begoff = p - start;

	removerecordregions(begoff);

	flag = p[1];

	expdest = p;

	if (quotes)
		rmescapes(p + 2, 0);

	len = cvtnum(ash_arith(p + 2));

	if (flag != '"')
		recordregion(begoff, begoff + len, 0);
}
#endif

/* argstr needs it */
static char *evalvar(char *p, int flags, struct strlist *var_str_list);

/*
 * Perform variable and command substitution.  If EXP_FULL is set, output CTLESC
 * characters to allow for further processing.  Otherwise treat
 * $@ like $* since no splitting will be performed.
 *
 * var_str_list (can be NULL) is a list of "VAR=val" strings which take precedence
 * over shell varables. Needed for "A=a B=$A; echo $B" case - we use it
 * for correct expansion of "B=$A" word.
 */
static void
argstr(char *p, int flags, struct strlist *var_str_list)
{
	static const char spclchars[] ALIGN1 = {
		'=',
		':',
		CTLQUOTEMARK,
		CTLENDVAR,
		CTLESC,
		CTLVAR,
		CTLBACKQ,
		CTLBACKQ | CTLQUOTE,
#if ENABLE_SH_MATH_SUPPORT
		CTLENDARI,
#endif
		'\0'
	};
	const char *reject = spclchars;
	int quotes = flags & (EXP_FULL | EXP_CASE | EXP_REDIR); /* do CTLESC */
	int breakall = flags & EXP_WORD;
	int inquotes;
	size_t length;
	int startloc;

	if (!(flags & EXP_VARTILDE)) {
		reject += 2;
	} else if (flags & EXP_VARTILDE2) {
		reject++;
	}
	inquotes = 0;
	length = 0;
	if (flags & EXP_TILDE) {
		char *q;

		flags &= ~EXP_TILDE;
 tilde:
		q = p;
		if ((unsigned char)*q == CTLESC && (flags & EXP_QWORD))
			q++;
		if (*q == '~')
			p = exptilde(p, q, flags);
	}
 start:
	startloc = expdest - (char *)stackblock();
	for (;;) {
		unsigned char c;

		length += strcspn(p + length, reject);
		c = p[length];
		if (c) {
			if (!(c & 0x80)
			IF_SH_MATH_SUPPORT(|| c == CTLENDARI)
			) {
				/* c == '=' || c == ':' || c == CTLENDARI */
				length++;
			}
		}
		if (length > 0) {
			int newloc;
			expdest = stack_nputstr(p, length, expdest);
			newloc = expdest - (char *)stackblock();
			if (breakall && !inquotes && newloc > startloc) {
				recordregion(startloc, newloc, 0);
			}
			startloc = newloc;
		}
		p += length + 1;
		length = 0;

		switch (c) {
		case '\0':
			goto breakloop;
		case '=':
			if (flags & EXP_VARTILDE2) {
				p--;
				continue;
			}
			flags |= EXP_VARTILDE2;
			reject++;
			/* fall through */
		case ':':
			/*
			 * sort of a hack - expand tildes in variable
			 * assignments (after the first '=' and after ':'s).
			 */
			if (*--p == '~') {
				goto tilde;
			}
			continue;
		}

		switch (c) {
		case CTLENDVAR: /* ??? */
			goto breakloop;
		case CTLQUOTEMARK:
			/* "$@" syntax adherence hack */
			if (!inquotes
			 && memcmp(p, dolatstr, 4) == 0
			 && (  p[4] == (char)CTLQUOTEMARK
			    || (p[4] == (char)CTLENDVAR && p[5] == (char)CTLQUOTEMARK)
			    )
			) {
				p = evalvar(p + 1, flags, /* var_str_list: */ NULL) + 1;
				goto start;
			}
			inquotes = !inquotes;
 addquote:
			if (quotes) {
				p--;
				length++;
				startloc++;
			}
			break;
		case CTLESC:
			startloc++;
			length++;
			goto addquote;
		case CTLVAR:
			p = evalvar(p, flags, var_str_list);
			goto start;
		case CTLBACKQ:
			c = '\0';
		case CTLBACKQ|CTLQUOTE:
			expbackq(argbackq->n, c, quotes);
			argbackq = argbackq->next;
			goto start;
#if ENABLE_SH_MATH_SUPPORT
		case CTLENDARI:
			p--;
			expari(quotes);
			goto start;
#endif
		}
	}
 breakloop: ;
}

static char *
scanleft(char *startp, char *rmesc, char *rmescend UNUSED_PARAM,
		char *pattern, int quotes, int zero)
{
	char *loc, *loc2;
	char c;

	loc = startp;
	loc2 = rmesc;
	do {
		int match;
		const char *s = loc2;

		c = *loc2;
		if (zero) {
			*loc2 = '\0';
			s = rmesc;
		}
		match = pmatch(pattern, s);

		*loc2 = c;
		if (match)
			return loc;
		if (quotes && (unsigned char)*loc == CTLESC)
			loc++;
		loc++;
		loc2++;
	} while (c);
	return NULL;
}

static char *
scanright(char *startp, char *rmesc, char *rmescend,
		char *pattern, int quotes, int match_at_start)
{
#if !ENABLE_ASH_OPTIMIZE_FOR_SIZE
	int try2optimize = match_at_start;
#endif
	int esc = 0;
	char *loc;
	char *loc2;

	/* If we called by "${v/pattern/repl}" or "${v//pattern/repl}":
	 * startp="escaped_value_of_v" rmesc="raw_value_of_v"
	 * rmescend=""(ptr to NUL in rmesc) pattern="pattern" quotes=match_at_start=1
	 * Logic:
	 * loc starts at NUL at the end of startp, loc2 starts at the end of rmesc,
	 * and on each iteration they go back two/one char until they reach the beginning.
	 * We try to find a match in "raw_value_of_v", "raw_value_of_", "raw_value_of" etc.
	 */
	/* TODO: document in what other circumstances we are called. */

	for (loc = pattern - 1, loc2 = rmescend; loc >= startp; loc2--) {
		int match;
		char c = *loc2;
		const char *s = loc2;
		if (match_at_start) {
			*loc2 = '\0';
			s = rmesc;
		}
		match = pmatch(pattern, s);
		//bb_error_msg("pmatch(pattern:'%s',s:'%s'):%d", pattern, s, match);
		*loc2 = c;
		if (match)
			return loc;
#if !ENABLE_ASH_OPTIMIZE_FOR_SIZE
		if (try2optimize) {
			/* Maybe we can optimize this:
			 * if pattern ends with unescaped *, we can avoid checking
			 * shorter strings: if "foo*" doesnt match "raw_value_of_v",
			 * it wont match truncated "raw_value_of_" strings too.
			 */
			unsigned plen = strlen(pattern);
			/* Does it end with "*"? */
			if (plen != 0 && pattern[--plen] == '*') {
				/* "xxxx*" is not escaped */
				/* "xxx\*" is escaped */
				/* "xx\\*" is not escaped */
				/* "x\\\*" is escaped */
				int slashes = 0;
				while (plen != 0 && pattern[--plen] == '\\')
					slashes++;
				if (!(slashes & 1))
					break; /* ends with unescaped "*" */
			}
			try2optimize = 0;
		}
#endif
		loc--;
		if (quotes) {
			if (--esc < 0) {
				esc = esclen(startp, loc);
			}
			if (esc % 2) {
				esc--;
				loc--;
			}
		}
	}
	return NULL;
}

static void varunset(const char *, const char *, const char *, int) NORETURN;
static void
varunset(const char *end, const char *var, const char *umsg, int varflags)
{
	const char *msg;
	const char *tail;

	tail = nullstr;
	msg = "parameter not set";
	if (umsg) {
		if ((unsigned char)*end == CTLENDVAR) {
			if (varflags & VSNUL)
				tail = " or null";
		} else {
			msg = umsg;
		}
	}
	ash_msg_and_raise_error("%.*s: %s%s", (int)(end - var - 1), var, msg, tail);
}

#if ENABLE_ASH_BASH_COMPAT
static char *
parse_sub_pattern(char *arg, int varflags)
{
	char *idx, *repl = NULL;
	unsigned char c;

	//char *org_arg = arg;
	//bb_error_msg("arg:'%s' varflags:%x", arg, varflags);
	idx = arg;
	while (1) {
		c = *arg;
		if (!c)
			break;
		if (c == '/') {
			/* Only the first '/' seen is our separator */
			if (!repl) {
				repl = idx + 1;
				c = '\0';
			}
		}
		*idx++ = c;
		arg++;
		/*
		 * Example: v='ab\c'; echo ${v/\\b/_\\_\z_}
		 * The result is a_\_z_c (not a\_\_z_c)!
		 *
		 * Enable debug prints in this function and you'll see:
		 * ash: arg:'\\b/_\\_z_' varflags:d
		 * ash: pattern:'\\b' repl:'_\_z_'
		 * That is, \\b is interpreted as \\b, but \\_ as \_!
		 * IOW: search pattern and replace string treat backslashes
		 * differently! That is the reason why we check repl below:
		 */
		if (c == '\\' && *arg == '\\' && repl && !(varflags & VSQUOTE))
			arg++; /* skip both '\', not just first one */
	}
	*idx = c; /* NUL */
	//bb_error_msg("pattern:'%s' repl:'%s'", org_arg, repl);

	return repl;
}
#endif /* ENABLE_ASH_BASH_COMPAT */

static const char *
subevalvar(char *p, char *varname, int strloc, int subtype,
		int startloc, int varflags, int quotes, struct strlist *var_str_list)
{
	struct nodelist *saveargbackq = argbackq;
	char *startp;
	char *loc;
	char *rmesc, *rmescend;
	char *str;
	IF_ASH_BASH_COMPAT(const char *repl = NULL;)
	IF_ASH_BASH_COMPAT(int pos, len, orig_len;)
	int saveherefd = herefd;
	int amount, workloc, resetloc;
	int zero;
	char *(*scan)(char*, char*, char*, char*, int, int);

	//bb_error_msg("subevalvar(p:'%s',varname:'%s',strloc:%d,subtype:%d,startloc:%d,varflags:%x,quotes:%d)",
	//		p, varname, strloc, subtype, startloc, varflags, quotes);

	herefd = -1;
	argstr(p, (subtype != VSASSIGN && subtype != VSQUESTION) ? EXP_CASE : 0,
			var_str_list);
	STPUTC('\0', expdest);
	herefd = saveherefd;
	argbackq = saveargbackq;
	startp = (char *)stackblock() + startloc;

	switch (subtype) {
	case VSASSIGN:
		setvar(varname, startp, 0);
		amount = startp - expdest;
		STADJUST(amount, expdest);
		return startp;

	case VSQUESTION:
		varunset(p, varname, startp, varflags);
		/* NOTREACHED */

#if ENABLE_ASH_BASH_COMPAT
	case VSSUBSTR:
		loc = str = stackblock() + strloc;
		/* Read POS in ${var:POS:LEN} */
		pos = atoi(loc); /* number(loc) errors out on "1:4" */
		len = str - startp - 1;

		/* *loc != '\0', guaranteed by parser */
		if (quotes) {
			char *ptr;

			/* Adjust the length by the number of escapes */
			for (ptr = startp; ptr < (str - 1); ptr++) {
				if ((unsigned char)*ptr == CTLESC) {
					len--;
					ptr++;
				}
			}
		}
		orig_len = len;

		if (*loc++ == ':') {
			/* ${var::LEN} */
			len = number(loc);
		} else {
			/* Skip POS in ${var:POS:LEN} */
			len = orig_len;
			while (*loc && *loc != ':') {
				/* TODO?
				 * bash complains on: var=qwe; echo ${var:1a:123}
				if (!isdigit(*loc))
					ash_msg_and_raise_error(msg_illnum, str);
				 */
				loc++;
			}
			if (*loc++ == ':') {
				len = number(loc);
			}
		}
		if (pos >= orig_len) {
			pos = 0;
			len = 0;
		}
		if (len > (orig_len - pos))
			len = orig_len - pos;

		for (str = startp; pos; str++, pos--) {
			if (quotes && (unsigned char)*str == CTLESC)
				str++;
		}
		for (loc = startp; len; len--) {
			if (quotes && (unsigned char)*str == CTLESC)
				*loc++ = *str++;
			*loc++ = *str++;
		}
		*loc = '\0';
		amount = loc - expdest;
		STADJUST(amount, expdest);
		return loc;
#endif
	}

	resetloc = expdest - (char *)stackblock();

	/* We'll comeback here if we grow the stack while handling
	 * a VSREPLACE or VSREPLACEALL, since our pointers into the
	 * stack will need rebasing, and we'll need to remove our work
	 * areas each time
	 */
 IF_ASH_BASH_COMPAT(restart:)

	amount = expdest - ((char *)stackblock() + resetloc);
	STADJUST(-amount, expdest);
	startp = (char *)stackblock() + startloc;

	rmesc = startp;
	rmescend = (char *)stackblock() + strloc;
	if (quotes) {
		rmesc = rmescapes(startp, RMESCAPE_ALLOC | RMESCAPE_GROW);
		if (rmesc != startp) {
			rmescend = expdest;
			startp = (char *)stackblock() + startloc;
		}
	}
	rmescend--;
	str = (char *)stackblock() + strloc;
	preglob(str, varflags & VSQUOTE, 0);
	workloc = expdest - (char *)stackblock();

#if ENABLE_ASH_BASH_COMPAT
	if (subtype == VSREPLACE || subtype == VSREPLACEALL) {
		char *idx, *end;

		if (!repl) {
			repl = parse_sub_pattern(str, varflags);
			//bb_error_msg("repl:'%s'", repl);
			if (!repl)
				repl = nullstr;
		}

		/* If there's no pattern to match, return the expansion unmolested */
		if (str[0] == '\0')
			return NULL;

		len = 0;
		idx = startp;
		end = str - 1;
		while (idx < end) {
 try_to_match:
			loc = scanright(idx, rmesc, rmescend, str, quotes, 1);
			//bb_error_msg("scanright('%s'):'%s'", str, loc);
			if (!loc) {
				/* No match, advance */
				char *restart_detect = stackblock();
 skip_matching:
				STPUTC(*idx, expdest);
				if (quotes && (unsigned char)*idx == CTLESC) {
					idx++;
					len++;
					STPUTC(*idx, expdest);
				}
				if (stackblock() != restart_detect)
					goto restart;
				idx++;
				len++;
				rmesc++;
				/* continue; - prone to quadratic behavior, smarter code: */
				if (idx >= end)
					break;
				if (str[0] == '*') {
					/* Pattern is "*foo". If "*foo" does not match "long_string",
					 * it would never match "ong_string" etc, no point in trying.
					 */
					goto skip_matching;
				}
				goto try_to_match;
			}

			if (subtype == VSREPLACEALL) {
				while (idx < loc) {
					if (quotes && (unsigned char)*idx == CTLESC)
						idx++;
					idx++;
					rmesc++;
				}
			} else {
				idx = loc;
			}

			//bb_error_msg("repl:'%s'", repl);
			for (loc = (char*)repl; *loc; loc++) {
				char *restart_detect = stackblock();
				if (quotes && *loc == '\\') {
					STPUTC(CTLESC, expdest);
					len++;
				}
				STPUTC(*loc, expdest);
				if (stackblock() != restart_detect)
					goto restart;
				len++;
			}

			if (subtype == VSREPLACE) {
				//bb_error_msg("tail:'%s', quotes:%x", idx, quotes);
				while (*idx) {
					char *restart_detect = stackblock();
					STPUTC(*idx, expdest);
					if (stackblock() != restart_detect)
						goto restart;
					len++;
					idx++;
				}
				break;
			}
		}

		/* We've put the replaced text into a buffer at workloc, now
		 * move it to the right place and adjust the stack.
		 */
		STPUTC('\0', expdest);
		startp = (char *)stackblock() + startloc;
		memmove(startp, (char *)stackblock() + workloc, len + 1);
		//bb_error_msg("startp:'%s'", startp);
		amount = expdest - (startp + len);
		STADJUST(-amount, expdest);
		return startp;
	}
#endif /* ENABLE_ASH_BASH_COMPAT */

	subtype -= VSTRIMRIGHT;
#if DEBUG
	if (subtype < 0 || subtype > 7)
		abort();
#endif
	/* zero = (subtype == VSTRIMLEFT || subtype == VSTRIMLEFTMAX) */
	zero = subtype >> 1;
	/* VSTRIMLEFT/VSTRIMRIGHTMAX -> scanleft */
	scan = (subtype & 1) ^ zero ? scanleft : scanright;

	loc = scan(startp, rmesc, rmescend, str, quotes, zero);
	if (loc) {
		if (zero) {
			memmove(startp, loc, str - loc);
			loc = startp + (str - loc) - 1;
		}
		*loc = '\0';
		amount = loc - expdest;
		STADJUST(amount, expdest);
	}
	return loc;
}

/*
 * Add the value of a specialized variable to the stack string.
 * name parameter (examples):
 * ash -c 'echo $1'      name:'1='
 * ash -c 'echo $qwe'    name:'qwe='
 * ash -c 'echo $$'      name:'$='
 * ash -c 'echo ${$}'    name:'$='
 * ash -c 'echo ${$##q}' name:'$=q'
 * ash -c 'echo ${#$}'   name:'$='
 * note: examples with bad shell syntax:
 * ash -c 'echo ${#$1}'  name:'$=1'
 * ash -c 'echo ${#1#}'  name:'1=#'
 */
static NOINLINE ssize_t
varvalue(char *name, int varflags, int flags, struct strlist *var_str_list)
{
	const char *p;
	int num;
	int i;
	int sepq = 0;
	ssize_t len = 0;
	int subtype = varflags & VSTYPE;
	int quotes = flags & (EXP_FULL | EXP_CASE | EXP_REDIR);
	int quoted = varflags & VSQUOTE;
	int syntax = quoted ? DQSYNTAX : BASESYNTAX;

	switch (*name) {
	case '$':
		num = rootpid;
		goto numvar;
	case '?':
		num = exitstatus;
		goto numvar;
	case '#':
		num = shellparam.nparam;
		goto numvar;
	case '!':
		num = backgndpid;
		if (num == 0)
			return -1;
 numvar:
		len = cvtnum(num);
		goto check_1char_name;
	case '-':
		expdest = makestrspace(NOPTS, expdest);
		for (i = NOPTS - 1; i >= 0; i--) {
			if (optlist[i]) {
				USTPUTC(optletters(i), expdest);
				len++;
			}
		}
 check_1char_name:
#if 0
		/* handles cases similar to ${#$1} */
		if (name[2] != '\0')
			raise_error_syntax("bad substitution");
#endif
		break;
	case '@': {
		char **ap;
		int sep;

		if (quoted && (flags & EXP_FULL)) {
			/* note: this is not meant as PEOF value */
			sep = 1 << CHAR_BIT;
			goto param;
		}
		/* fall through */
	case '*':
		sep = ifsset() ? (unsigned char)(ifsval()[0]) : ' ';
		i = SIT(sep, syntax);
		if (quotes && (i == CCTL || i == CBACK))
			sepq = 1;
 param:
		ap = shellparam.p;
		if (!ap)
			return -1;
		while ((p = *ap++) != NULL) {
			size_t partlen;

			partlen = strlen(p);
			len += partlen;

			if (!(subtype == VSPLUS || subtype == VSLENGTH))
				memtodest(p, partlen, syntax, quotes);

			if (*ap && sep) {
				char *q;

				len++;
				if (subtype == VSPLUS || subtype == VSLENGTH) {
					continue;
				}
				q = expdest;
				if (sepq)
					STPUTC(CTLESC, q);
				/* note: may put NUL despite sep != 0
				 * (see sep = 1 << CHAR_BIT above) */
				STPUTC(sep, q);
				expdest = q;
			}
		}
		return len;
	} /* case '@' and '*' */
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		num = atoi(name); /* number(name) fails on ${N#str} etc */
		if (num < 0 || num > shellparam.nparam)
			return -1;
		p = num ? shellparam.p[num - 1] : arg0;
		goto value;
	default:
		/* NB: name has form "VAR=..." */

		/* "A=a B=$A" case: var_str_list is a list of "A=a" strings
		 * which should be considered before we check variables. */
		if (var_str_list) {
			unsigned name_len = (strchrnul(name, '=') - name) + 1;
			p = NULL;
			do {
				char *str, *eq;
				str = var_str_list->text;
				eq = strchr(str, '=');
				if (!eq) /* stop at first non-assignment */
					break;
				eq++;
				if (name_len == (unsigned)(eq - str)
				 && strncmp(str, name, name_len) == 0
				) {
					p = eq;
					/* goto value; - WRONG! */
					/* think "A=1 A=2 B=$A" */
				}
				var_str_list = var_str_list->next;
			} while (var_str_list);
			if (p)
				goto value;
		}
		p = lookupvar(name);
 value:
		if (!p)
			return -1;

		len = strlen(p);
		if (!(subtype == VSPLUS || subtype == VSLENGTH))
			memtodest(p, len, syntax, quotes);
		return len;
	}

	if (subtype == VSPLUS || subtype == VSLENGTH)
		STADJUST(-len, expdest);
	return len;
}

/*
 * Expand a variable, and return a pointer to the next character in the
 * input string.
 */
static char *
evalvar(char *p, int flags, struct strlist *var_str_list)
{
	char varflags;
	char subtype;
	char quoted;
	char easy;
	char *var;
	int patloc;
	int startloc;
	ssize_t varlen;

	varflags = (unsigned char) *p++;
	subtype = varflags & VSTYPE;
	quoted = varflags & VSQUOTE;
	var = p;
	easy = (!quoted || (*var == '@' && shellparam.nparam));
	startloc = expdest - (char *)stackblock();
	p = strchr(p, '=') + 1; //TODO: use var_end(p)?

 again:
	varlen = varvalue(var, varflags, flags, var_str_list);
	if (varflags & VSNUL)
		varlen--;

	if (subtype == VSPLUS) {
		varlen = -1 - varlen;
		goto vsplus;
	}

	if (subtype == VSMINUS) {
 vsplus:
		if (varlen < 0) {
			argstr(
				p,
				flags | (quoted ? EXP_TILDE|EXP_QWORD : EXP_TILDE|EXP_WORD),
				var_str_list
			);
			goto end;
		}
		if (easy)
			goto record;
		goto end;
	}

	if (subtype == VSASSIGN || subtype == VSQUESTION) {
		if (varlen < 0) {
			if (subevalvar(p, var, /* strloc: */ 0,
					subtype, startloc, varflags,
					/* quotes: */ 0,
					var_str_list)
			) {
				varflags &= ~VSNUL;
				/*
				 * Remove any recorded regions beyond
				 * start of variable
				 */
				removerecordregions(startloc);
				goto again;
			}
			goto end;
		}
		if (easy)
			goto record;
		goto end;
	}

	if (varlen < 0 && uflag)
		varunset(p, var, 0, 0);

	if (subtype == VSLENGTH) {
		cvtnum(varlen > 0 ? varlen : 0);
		goto record;
	}

	if (subtype == VSNORMAL) {
		if (easy)
			goto record;
		goto end;
	}

#if DEBUG
	switch (subtype) {
	case VSTRIMLEFT:
	case VSTRIMLEFTMAX:
	case VSTRIMRIGHT:
	case VSTRIMRIGHTMAX:
#if ENABLE_ASH_BASH_COMPAT
	case VSSUBSTR:
	case VSREPLACE:
	case VSREPLACEALL:
#endif
		break;
	default:
		abort();
	}
#endif

	if (varlen >= 0) {
		/*
		 * Terminate the string and start recording the pattern
		 * right after it
		 */
		STPUTC('\0', expdest);
		patloc = expdest - (char *)stackblock();
		if (NULL == subevalvar(p, /* varname: */ NULL, patloc, subtype,
				startloc, varflags,
				/* quotes: */ flags & (EXP_FULL | EXP_CASE | EXP_REDIR),
				var_str_list)
		) {
			int amount = expdest - (
				(char *)stackblock() + patloc - 1
			);
			STADJUST(-amount, expdest);
		}
		/* Remove any recorded regions beyond start of variable */
		removerecordregions(startloc);
 record:
		recordregion(startloc, expdest - (char *)stackblock(), quoted);
	}

 end:
	if (subtype != VSNORMAL) {      /* skip to end of alternative */
		int nesting = 1;
		for (;;) {
			unsigned char c = *p++;
			if (c == CTLESC)
				p++;
			else if (c == CTLBACKQ || c == (CTLBACKQ|CTLQUOTE)) {
				if (varlen >= 0)
					argbackq = argbackq->next;
			} else if (c == CTLVAR) {
				if ((*p++ & VSTYPE) != VSNORMAL)
					nesting++;
			} else if (c == CTLENDVAR) {
				if (--nesting == 0)
					break;
			}
		}
	}
	return p;
}

/*
 * Break the argument string into pieces based upon IFS and add the
 * strings to the argument list.  The regions of the string to be
 * searched for IFS characters have been stored by recordregion.
 */
static void
ifsbreakup(char *string, struct arglist *arglist)
{
	struct ifsregion *ifsp;
	struct strlist *sp;
	char *start;
	char *p;
	char *q;
	const char *ifs, *realifs;
	int ifsspc;
	int nulonly;

	start = string;
	if (ifslastp != NULL) {
		ifsspc = 0;
		nulonly = 0;
		realifs = ifsset() ? ifsval() : defifs;
		ifsp = &ifsfirst;
		do {
			p = string + ifsp->begoff;
			nulonly = ifsp->nulonly;
			ifs = nulonly ? nullstr : realifs;
			ifsspc = 0;
			while (p < string + ifsp->endoff) {
				q = p;
				if ((unsigned char)*p == CTLESC)
					p++;
				if (!strchr(ifs, *p)) {
					p++;
					continue;
				}
				if (!nulonly)
					ifsspc = (strchr(defifs, *p) != NULL);
				/* Ignore IFS whitespace at start */
				if (q == start && ifsspc) {
					p++;
					start = p;
					continue;
				}
				*q = '\0';
				sp = stzalloc(sizeof(*sp));
				sp->text = start;
				*arglist->lastp = sp;
				arglist->lastp = &sp->next;
				p++;
				if (!nulonly) {
					for (;;) {
						if (p >= string + ifsp->endoff) {
							break;
						}
						q = p;
						if ((unsigned char)*p == CTLESC)
							p++;
						if (strchr(ifs, *p) == NULL) {
							p = q;
							break;
						}
						if (strchr(defifs, *p) == NULL) {
							if (ifsspc) {
								p++;
								ifsspc = 0;
							} else {
								p = q;
								break;
							}
						} else
							p++;
					}
				}
				start = p;
			} /* while */
			ifsp = ifsp->next;
		} while (ifsp != NULL);
		if (nulonly)
			goto add;
	}

	if (!*start)
		return;

 add:
	sp = stzalloc(sizeof(*sp));
	sp->text = start;
	*arglist->lastp = sp;
	arglist->lastp = &sp->next;
}

static void
ifsfree(void)
{
	struct ifsregion *p;

	INT_OFF;
	p = ifsfirst.next;
	do {
		struct ifsregion *ifsp;
		ifsp = p->next;
		free(p);
		p = ifsp;
	} while (p);
	ifslastp = NULL;
	ifsfirst.next = NULL;
	INT_ON;
}

/*
 * Add a file name to the list.
 */
static void
addfname(const char *name)
{
	struct strlist *sp;

	sp = stzalloc(sizeof(*sp));
	sp->text = ststrdup(name);
	*exparg.lastp = sp;
	exparg.lastp = &sp->next;
}

/*
 * Do metacharacter (i.e. *, ?, [...]) expansion.
 */
static void
expmeta(char *expdir, char *enddir, char *name)
{
	char *p;
	const char *cp;
	char *start;
	char *endname;
	int metaflag;
	struct stat statb;
	DIR *dirp;
	struct dirent *dp;
	int atend;
	int matchdot;

	metaflag = 0;
	start = name;
	for (p = name; *p; p++) {
		if (*p == '*' || *p == '?')
			metaflag = 1;
		else if (*p == '[') {
			char *q = p + 1;
			if (*q == '!')
				q++;
			for (;;) {
				if (*q == '\\')
					q++;
				if (*q == '/' || *q == '\0')
					break;
				if (*++q == ']') {
					metaflag = 1;
					break;
				}
			}
		} else if (*p == '\\')
			p++;
		else if (*p == '/') {
			if (metaflag)
				goto out;
			start = p + 1;
		}
	}
 out:
	if (metaflag == 0) {    /* we've reached the end of the file name */
		if (enddir != expdir)
			metaflag++;
		p = name;
		do {
			if (*p == '\\')
				p++;
			*enddir++ = *p;
		} while (*p++);
		if (metaflag == 0 || lstat(expdir, &statb) >= 0)
			addfname(expdir);
		return;
	}
	endname = p;
	if (name < start) {
		p = name;
		do {
			if (*p == '\\')
				p++;
			*enddir++ = *p++;
		} while (p < start);
	}
	if (enddir == expdir) {
		cp = ".";
	} else if (enddir == expdir + 1 && *expdir == '/') {
		cp = "/";
	} else {
		cp = expdir;
		enddir[-1] = '\0';
	}
	dirp = opendir(cp);
	if (dirp == NULL)
		return;
	if (enddir != expdir)
		enddir[-1] = '/';
	if (*endname == 0) {
		atend = 1;
	} else {
		atend = 0;
		*endname++ = '\0';
	}
	matchdot = 0;
	p = start;
	if (*p == '\\')
		p++;
	if (*p == '.')
		matchdot++;
	while (!pending_int && (dp = readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.' && !matchdot)
			continue;
		if (pmatch(start, dp->d_name)) {
			if (atend) {
				strcpy(enddir, dp->d_name);
				addfname(expdir);
			} else {
				for (p = enddir, cp = dp->d_name; (*p++ = *cp++) != '\0';)
					continue;
				p[-1] = '/';
				expmeta(expdir, p, endname);
			}
		}
	}
	closedir(dirp);
	if (!atend)
		endname[-1] = '/';
}

static struct strlist *
msort(struct strlist *list, int len)
{
	struct strlist *p, *q = NULL;
	struct strlist **lpp;
	int half;
	int n;

	if (len <= 1)
		return list;
	half = len >> 1;
	p = list;
	for (n = half; --n >= 0;) {
		q = p;
		p = p->next;
	}
	q->next = NULL;                 /* terminate first half of list */
	q = msort(list, half);          /* sort first half of list */
	p = msort(p, len - half);               /* sort second half */
	lpp = &list;
	for (;;) {
#if ENABLE_LOCALE_SUPPORT
		if (strcoll(p->text, q->text) < 0)
#else
		if (strcmp(p->text, q->text) < 0)
#endif
						{
			*lpp = p;
			lpp = &p->next;
			p = *lpp;
			if (p == NULL) {
				*lpp = q;
				break;
			}
		} else {
			*lpp = q;
			lpp = &q->next;
			q = *lpp;
			if (q == NULL) {
				*lpp = p;
				break;
			}
		}
	}
	return list;
}

/*
 * Sort the results of file name expansion.  It calculates the number of
 * strings to sort and then calls msort (short for merge sort) to do the
 * work.
 */
static struct strlist *
expsort(struct strlist *str)
{
	int len;
	struct strlist *sp;

	len = 0;
	for (sp = str; sp; sp = sp->next)
		len++;
	return msort(str, len);
}

static void
expandmeta(struct strlist *str /*, int flag*/)
{
	static const char metachars[] ALIGN1 = {
		'*', '?', '[', 0
	};
	/* TODO - EXP_REDIR */

	while (str) {
		char *expdir;
		struct strlist **savelastp;
		struct strlist *sp;
		char *p;

		if (fflag)
			goto nometa;
		if (!strpbrk(str->text, metachars))
			goto nometa;
		savelastp = exparg.lastp;

		INT_OFF;
		p = preglob(str->text, 0, RMESCAPE_ALLOC | RMESCAPE_HEAP);
		{
			int i = strlen(str->text);
			expdir = ckmalloc(i < 2048 ? 2048 : i); /* XXX */
		}
		expmeta(expdir, expdir, p);
		free(expdir);
		if (p != str->text)
			free(p);
		INT_ON;
		if (exparg.lastp == savelastp) {
			/*
			 * no matches
			 */
 nometa:
			*exparg.lastp = str;
			rmescapes(str->text, 0);
			exparg.lastp = &str->next;
		} else {
			*exparg.lastp = NULL;
			*savelastp = sp = expsort(*savelastp);
			while (sp->next != NULL)
				sp = sp->next;
			exparg.lastp = &sp->next;
		}
		str = str->next;
	}
}

/*
 * Perform variable substitution and command substitution on an argument,
 * placing the resulting list of arguments in arglist.  If EXP_FULL is true,
 * perform splitting and file name expansion.  When arglist is NULL, perform
 * here document expansion.
 */
static void
expandarg(union node *arg, struct arglist *arglist, int flag)
{
	struct strlist *sp;
	char *p;

	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	ifsfirst.next = NULL;
	ifslastp = NULL;
	argstr(arg->narg.text, flag,
			/* var_str_list: */ arglist ? arglist->list : NULL);
	p = _STPUTC('\0', expdest);
	expdest = p - 1;
	if (arglist == NULL) {
		return;                 /* here document expanded */
	}
	p = grabstackstr(p);
	exparg.lastp = &exparg.list;
	/*
	 * TODO - EXP_REDIR
	 */
	if (flag & EXP_FULL) {
		ifsbreakup(p, &exparg);
		*exparg.lastp = NULL;
		exparg.lastp = &exparg.list;
		expandmeta(exparg.list /*, flag*/);
	} else {
		if (flag & EXP_REDIR) /*XXX - for now, just remove escapes */
			rmescapes(p, 0);
		sp = stzalloc(sizeof(*sp));
		sp->text = p;
		*exparg.lastp = sp;
		exparg.lastp = &sp->next;
	}
	if (ifsfirst.next)
		ifsfree();
	*exparg.lastp = NULL;
	if (exparg.list) {
		*arglist->lastp = exparg.list;
		arglist->lastp = exparg.lastp;
	}
}

/*
 * Expand shell variables and backquotes inside a here document.
 */
static void
expandhere(union node *arg, int fd)
{
	herefd = fd;
	expandarg(arg, (struct arglist *)NULL, 0);
	full_write(fd, stackblock(), expdest - (char *)stackblock());
}

/*
 * Returns true if the pattern matches the string.
 */
static int
patmatch(char *pattern, const char *string)
{
	return pmatch(preglob(pattern, 0, 0), string);
}

/*
 * See if a pattern matches in a case statement.
 */
static int
casematch(union node *pattern, char *val)
{
	struct stackmark smark;
	int result;

	setstackmark(&smark);
	argbackq = pattern->narg.backquote;
	STARTSTACKSTR(expdest);
	ifslastp = NULL;
	argstr(pattern->narg.text, EXP_TILDE | EXP_CASE,
			/* var_str_list: */ NULL);
	STACKSTRNUL(expdest);
	result = patmatch(stackblock(), val);
	popstackmark(&smark);
	return result;
}


/* ============ find_command */

struct builtincmd {
	const char *name;
	int (*builtin)(int, char **) FAST_FUNC;
	/* unsigned flags; */
};
#define IS_BUILTIN_SPECIAL(b) ((b)->name[0] & 1)
/* "regular" builtins always take precedence over commands,
 * regardless of PATH=....%builtin... position */
#define IS_BUILTIN_REGULAR(b) ((b)->name[0] & 2)
#define IS_BUILTIN_ASSIGN(b)  ((b)->name[0] & 4)

struct cmdentry {
	smallint cmdtype;       /* CMDxxx */
	union param {
		int index;
		/* index >= 0 for commands without path (slashes) */
		/* (TODO: what exactly does the value mean? PATH position?) */
		/* index == -1 for commands with slashes */
		/* index == (-2 - applet_no) for NOFORK applets */
		const struct builtincmd *cmd;
		struct funcnode *func;
	} u;
};
/* values of cmdtype */
#define CMDUNKNOWN      -1      /* no entry in table for command */
#define CMDNORMAL       0       /* command is an executable program */
#define CMDFUNCTION     1       /* command is a shell function */
#define CMDBUILTIN      2       /* command is a shell builtin */

/* action to find_command() */
#define DO_ERR          0x01    /* prints errors */
#define DO_ABS          0x02    /* checks absolute paths */
#define DO_NOFUNC       0x04    /* don't return shell functions, for command */
#define DO_ALTPATH      0x08    /* using alternate path */
#define DO_ALTBLTIN     0x20    /* %builtin in alt. path */

static void find_command(char *, struct cmdentry *, int, const char *);


/* ============ Hashing commands */

/*
 * When commands are first encountered, they are entered in a hash table.
 * This ensures that a full path search will not have to be done for them
 * on each invocation.
 *
 * We should investigate converting to a linear search, even though that
 * would make the command name "hash" a misnomer.
 */

struct tblentry {
	struct tblentry *next;  /* next entry in hash chain */
	union param param;      /* definition of builtin function */
	smallint cmdtype;       /* CMDxxx */
	char rehash;            /* if set, cd done since entry created */
	char cmdname[1];        /* name of command */
};

static struct tblentry **cmdtable;
#define INIT_G_cmdtable() do { \
	cmdtable = xzalloc(CMDTABLESIZE * sizeof(cmdtable[0])); \
} while (0)

static int builtinloc = -1;     /* index in path of %builtin, or -1 */


static void
tryexec(IF_FEATURE_SH_STANDALONE(int applet_no,) char *cmd, char **argv, char **envp)
{
#if ENABLE_FEATURE_SH_STANDALONE
	if (applet_no >= 0) {
		if (APPLET_IS_NOEXEC(applet_no)) {
			clearenv();
			while (*envp)
				putenv(*envp++);
			run_applet_no_and_exit(applet_no, argv);
		}
		/* re-exec ourselves with the new arguments */
		execve(bb_busybox_exec_path, argv, envp);
		/* If they called chroot or otherwise made the binary no longer
		 * executable, fall through */
	}
#endif

 repeat:
#ifdef SYSV
	do {
		execve(cmd, argv, envp);
	} while (errno == EINTR);
#else
	execve(cmd, argv, envp);
#endif
	if (cmd == (char*) bb_busybox_exec_path) {
		/* We already visited ENOEXEC branch below, don't do it again */
//TODO: try execve(initial_argv0_of_shell, argv, envp) before giving up?
		free(argv);
		return;
	}
	if (errno == ENOEXEC) {
		/* Run "cmd" as a shell script:
		 * http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
		 * "If the execve() function fails with ENOEXEC, the shell
		 * shall execute a command equivalent to having a shell invoked
		 * with the command name as its first operand,
		 * with any remaining arguments passed to the new shell"
		 *
		 * That is, do not use $SHELL, user's shell, or /bin/sh;
		 * just call ourselves.
		 *
		 * Note that bash reads ~80 chars of the file, and if it sees
		 * a zero byte before it sees newline, it doesn't try to
		 * interpret it, but fails with "cannot execute binary file"
		 * message and exit code 126. For one, this prevents attempts
		 * to interpret foreign ELF binaries as shell scripts.
		 */
		char **ap;
		char **new;

		for (ap = argv; *ap; ap++)
			continue;
		new = ckmalloc((ap - argv + 2) * sizeof(new[0]));
		new[0] = (char*) "ash";
		new[1] = cmd;
		ap = new + 2;
		while ((*ap++ = *++argv) != NULL)
			continue;
		cmd = (char*) bb_busybox_exec_path;
		argv = new;
		goto repeat;
	}
}

/*
 * Exec a program.  Never returns.  If you change this routine, you may
 * have to change the find_command routine as well.
 */
static void shellexec(char **, const char *, int) NORETURN;
static void
shellexec(char **argv, const char *path, int idx)
{
	char *cmdname;
	int e;
	char **envp;
	int exerrno;
	int applet_no = -1; /* used only by FEATURE_SH_STANDALONE */

	clearredir(/*drop:*/ 1);
	envp = listvars(VEXPORT, VUNSET, /*end:*/ NULL);
	if (strchr(argv[0], '/') != NULL
#if ENABLE_FEATURE_SH_STANDALONE
	 || (applet_no = find_applet_by_name(argv[0])) >= 0
#endif
	) {
		tryexec(IF_FEATURE_SH_STANDALONE(applet_no,) argv[0], argv, envp);
		if (applet_no >= 0) {
			/* We tried execing ourself, but it didn't work.
			 * Maybe /proc/self/exe doesn't exist?
			 * Try $PATH search.
			 */
			goto try_PATH;
		}
		e = errno;
	} else {
 try_PATH:
		e = ENOENT;
		while ((cmdname = path_advance(&path, argv[0])) != NULL) {
			if (--idx < 0 && pathopt == NULL) {
				tryexec(IF_FEATURE_SH_STANDALONE(-1,) cmdname, argv, envp);
				if (errno != ENOENT && errno != ENOTDIR)
					e = errno;
			}
			stunalloc(cmdname);
		}
	}

	/* Map to POSIX errors */
	switch (e) {
	case EACCES:
		exerrno = 126;
		break;
	case ENOENT:
		exerrno = 127;
		break;
	default:
		exerrno = 2;
		break;
	}
	exitstatus = exerrno;
	TRACE(("shellexec failed for %s, errno %d, suppress_int %d\n",
		argv[0], e, suppress_int));
	ash_msg_and_raise(EXEXEC, "%s: %s", argv[0], errmsg(e, "not found"));
	/* NOTREACHED */
}

static void
printentry(struct tblentry *cmdp)
{
	int idx;
	const char *path;
	char *name;

	idx = cmdp->param.index;
	path = pathval();
	do {
		name = path_advance(&path, cmdp->cmdname);
		stunalloc(name);
	} while (--idx >= 0);
	out1fmt("%s%s\n", name, (cmdp->rehash ? "*" : nullstr));
}

/*
 * Clear out command entries.  The argument specifies the first entry in
 * PATH which has changed.
 */
static void
clearcmdentry(int firstchange)
{
	struct tblentry **tblp;
	struct tblentry **pp;
	struct tblentry *cmdp;

	INT_OFF;
	for (tblp = cmdtable; tblp < &cmdtable[CMDTABLESIZE]; tblp++) {
		pp = tblp;
		while ((cmdp = *pp) != NULL) {
			if ((cmdp->cmdtype == CMDNORMAL &&
			     cmdp->param.index >= firstchange)
			 || (cmdp->cmdtype == CMDBUILTIN &&
			     builtinloc >= firstchange)
			) {
				*pp = cmdp->next;
				free(cmdp);
			} else {
				pp = &cmdp->next;
			}
		}
	}
	INT_ON;
}

/*
 * Locate a command in the command hash table.  If "add" is nonzero,
 * add the command to the table if it is not already present.  The
 * variable "lastcmdentry" is set to point to the address of the link
 * pointing to the entry, so that delete_cmd_entry can delete the
 * entry.
 *
 * Interrupts must be off if called with add != 0.
 */
static struct tblentry **lastcmdentry;

static struct tblentry *
cmdlookup(const char *name, int add)
{
	unsigned int hashval;
	const char *p;
	struct tblentry *cmdp;
	struct tblentry **pp;

	p = name;
	hashval = (unsigned char)*p << 4;
	while (*p)
		hashval += (unsigned char)*p++;
	hashval &= 0x7FFF;
	pp = &cmdtable[hashval % CMDTABLESIZE];
	for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
		if (strcmp(cmdp->cmdname, name) == 0)
			break;
		pp = &cmdp->next;
	}
	if (add && cmdp == NULL) {
		cmdp = *pp = ckzalloc(sizeof(struct tblentry)
				+ strlen(name)
				/* + 1 - already done because
				 * tblentry::cmdname is char[1] */);
		/*cmdp->next = NULL; - ckzalloc did it */
		cmdp->cmdtype = CMDUNKNOWN;
		strcpy(cmdp->cmdname, name);
	}
	lastcmdentry = pp;
	return cmdp;
}

/*
 * Delete the command entry returned on the last lookup.
 */
static void
delete_cmd_entry(void)
{
	struct tblentry *cmdp;

	INT_OFF;
	cmdp = *lastcmdentry;
	*lastcmdentry = cmdp->next;
	if (cmdp->cmdtype == CMDFUNCTION)
		freefunc(cmdp->param.func);
	free(cmdp);
	INT_ON;
}

/*
 * Add a new command entry, replacing any existing command entry for
 * the same name - except special builtins.
 */
static void
addcmdentry(char *name, struct cmdentry *entry)
{
	struct tblentry *cmdp;

	cmdp = cmdlookup(name, 1);
	if (cmdp->cmdtype == CMDFUNCTION) {
		freefunc(cmdp->param.func);
	}
	cmdp->cmdtype = entry->cmdtype;
	cmdp->param = entry->u;
	cmdp->rehash = 0;
}

static int FAST_FUNC
hashcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	struct tblentry **pp;
	struct tblentry *cmdp;
	int c;
	struct cmdentry entry;
	char *name;

	if (nextopt("r") != '\0') {
		clearcmdentry(0);
		return 0;
	}

	if (*argptr == NULL) {
		for (pp = cmdtable; pp < &cmdtable[CMDTABLESIZE]; pp++) {
			for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
				if (cmdp->cmdtype == CMDNORMAL)
					printentry(cmdp);
			}
		}
		return 0;
	}

	c = 0;
	while ((name = *argptr) != NULL) {
		cmdp = cmdlookup(name, 0);
		if (cmdp != NULL
		 && (cmdp->cmdtype == CMDNORMAL
		     || (cmdp->cmdtype == CMDBUILTIN && builtinloc >= 0))
		) {
			delete_cmd_entry();
		}
		find_command(name, &entry, DO_ERR, pathval());
		if (entry.cmdtype == CMDUNKNOWN)
			c = 1;
		argptr++;
	}
	return c;
}

/*
 * Called when a cd is done.  Marks all commands so the next time they
 * are executed they will be rehashed.
 */
static void
hashcd(void)
{
	struct tblentry **pp;
	struct tblentry *cmdp;

	for (pp = cmdtable; pp < &cmdtable[CMDTABLESIZE]; pp++) {
		for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
			if (cmdp->cmdtype == CMDNORMAL
			 || (cmdp->cmdtype == CMDBUILTIN
			     && !IS_BUILTIN_REGULAR(cmdp->param.cmd)
			     && builtinloc > 0)
			) {
				cmdp->rehash = 1;
			}
		}
	}
}

/*
 * Fix command hash table when PATH changed.
 * Called before PATH is changed.  The argument is the new value of PATH;
 * pathval() still returns the old value at this point.
 * Called with interrupts off.
 */
static void FAST_FUNC
changepath(const char *new)
{
	const char *old;
	int firstchange;
	int idx;
	int idx_bltin;

	old = pathval();
	firstchange = 9999;     /* assume no change */
	idx = 0;
	idx_bltin = -1;
	for (;;) {
		if (*old != *new) {
			firstchange = idx;
			if ((*old == '\0' && *new == ':')
			 || (*old == ':' && *new == '\0')
			) {
				firstchange++;
			}
			old = new;      /* ignore subsequent differences */
		}
		if (*new == '\0')
			break;
		if (*new == '%' && idx_bltin < 0 && prefix(new + 1, "builtin"))
			idx_bltin = idx;
		if (*new == ':')
			idx++;
		new++;
		old++;
	}
	if (builtinloc < 0 && idx_bltin >= 0)
		builtinloc = idx_bltin;             /* zap builtins */
	if (builtinloc >= 0 && idx_bltin < 0)
		firstchange = 0;
	clearcmdentry(firstchange);
	builtinloc = idx_bltin;
}

#define TEOF 0
#define TNL 1
#define TREDIR 2
#define TWORD 3
#define TSEMI 4
#define TBACKGND 5
#define TAND 6
#define TOR 7
#define TPIPE 8
#define TLP 9
#define TRP 10
#define TENDCASE 11
#define TENDBQUOTE 12
#define TNOT 13
#define TCASE 14
#define TDO 15
#define TDONE 16
#define TELIF 17
#define TELSE 18
#define TESAC 19
#define TFI 20
#define TFOR 21
#define TIF 22
#define TIN 23
#define TTHEN 24
#define TUNTIL 25
#define TWHILE 26
#define TBEGIN 27
#define TEND 28
typedef smallint token_id_t;

/* first char is indicating which tokens mark the end of a list */
static const char *const tokname_array[] = {
	"\1end of file",
	"\0newline",
	"\0redirection",
	"\0word",
	"\0;",
	"\0&",
	"\0&&",
	"\0||",
	"\0|",
	"\0(",
	"\1)",
	"\1;;",
	"\1`",
#define KWDOFFSET 13
	/* the following are keywords */
	"\0!",
	"\0case",
	"\1do",
	"\1done",
	"\1elif",
	"\1else",
	"\1esac",
	"\1fi",
	"\0for",
	"\0if",
	"\0in",
	"\1then",
	"\0until",
	"\0while",
	"\0{",
	"\1}",
};

/* Wrapper around strcmp for qsort/bsearch/... */
static int
pstrcmp(const void *a, const void *b)
{
	return strcmp((char*) a, (*(char**) b) + 1);
}

static const char *const *
findkwd(const char *s)
{
	return bsearch(s, tokname_array + KWDOFFSET,
			ARRAY_SIZE(tokname_array) - KWDOFFSET,
			sizeof(tokname_array[0]), pstrcmp);
}

/*
 * Locate and print what a word is...
 */
static int
describe_command(char *command, int describe_command_verbose)
{
	struct cmdentry entry;
	struct tblentry *cmdp;
#if ENABLE_ASH_ALIAS
	const struct alias *ap;
#endif
	const char *path = pathval();

	if (describe_command_verbose) {
		out1str(command);
	}

	/* First look at the keywords */
	if (findkwd(command)) {
		out1str(describe_command_verbose ? " is a shell keyword" : command);
		goto out;
	}

#if ENABLE_ASH_ALIAS
	/* Then look at the aliases */
	ap = lookupalias(command, 0);
	if (ap != NULL) {
		if (!describe_command_verbose) {
			out1str("alias ");
			printalias(ap);
			return 0;
		}
		out1fmt(" is an alias for %s", ap->val);
		goto out;
	}
#endif
	/* Then check if it is a tracked alias */
	cmdp = cmdlookup(command, 0);
	if (cmdp != NULL) {
		entry.cmdtype = cmdp->cmdtype;
		entry.u = cmdp->param;
	} else {
		/* Finally use brute force */
		find_command(command, &entry, DO_ABS, path);
	}

	switch (entry.cmdtype) {
	case CMDNORMAL: {
		int j = entry.u.index;
		char *p;
		if (j < 0) {
			p = command;
		} else {
			do {
				p = path_advance(&path, command);
				stunalloc(p);
			} while (--j >= 0);
		}
		if (describe_command_verbose) {
			out1fmt(" is%s %s",
				(cmdp ? " a tracked alias for" : nullstr), p
			);
		} else {
			out1str(p);
		}
		break;
	}

	case CMDFUNCTION:
		if (describe_command_verbose) {
			out1str(" is a shell function");
		} else {
			out1str(command);
		}
		break;

	case CMDBUILTIN:
		if (describe_command_verbose) {
			out1fmt(" is a %sshell builtin",
				IS_BUILTIN_SPECIAL(entry.u.cmd) ?
					"special " : nullstr
			);
		} else {
			out1str(command);
		}
		break;

	default:
		if (describe_command_verbose) {
			out1str(": not found\n");
		}
		return 127;
	}
 out:
	out1str("\n");
	return 0;
}

static int FAST_FUNC
typecmd(int argc UNUSED_PARAM, char **argv)
{
	int i = 1;
	int err = 0;
	int verbose = 1;

	/* type -p ... ? (we don't bother checking for 'p') */
	if (argv[1] && argv[1][0] == '-') {
		i++;
		verbose = 0;
	}
	while (argv[i]) {
		err |= describe_command(argv[i++], verbose);
	}
	return err;
}

#if ENABLE_ASH_CMDCMD
static int FAST_FUNC
commandcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int c;
	enum {
		VERIFY_BRIEF = 1,
		VERIFY_VERBOSE = 2,
	} verify = 0;

	while ((c = nextopt("pvV")) != '\0')
		if (c == 'V')
			verify |= VERIFY_VERBOSE;
		else if (c == 'v')
			verify |= VERIFY_BRIEF;
#if DEBUG
		else if (c != 'p')
			abort();
#endif
	/* Mimic bash: just "command -v" doesn't complain, it's a nop */
	if (verify && (*argptr != NULL)) {
		return describe_command(*argptr, verify - VERIFY_BRIEF);
	}

	return 0;
}
#endif


/* ============ eval.c */

static int funcblocksize;       /* size of structures in function */
static int funcstringsize;      /* size of strings in node */
static void *funcblock;         /* block to allocate function from */
static char *funcstring;        /* block to allocate strings from */

/* flags in argument to evaltree */
#define EV_EXIT    01           /* exit after evaluating tree */
#define EV_TESTED  02           /* exit status is checked; ignore -e flag */
#define EV_BACKCMD 04           /* command executing within back quotes */

static const uint8_t nodesize[N_NUMBER] = {
	[NCMD     ] = SHELL_ALIGN(sizeof(struct ncmd)),
	[NPIPE    ] = SHELL_ALIGN(sizeof(struct npipe)),
	[NREDIR   ] = SHELL_ALIGN(sizeof(struct nredir)),
	[NBACKGND ] = SHELL_ALIGN(sizeof(struct nredir)),
	[NSUBSHELL] = SHELL_ALIGN(sizeof(struct nredir)),
	[NAND     ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NOR      ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NSEMI    ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NIF      ] = SHELL_ALIGN(sizeof(struct nif)),
	[NWHILE   ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NUNTIL   ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NFOR     ] = SHELL_ALIGN(sizeof(struct nfor)),
	[NCASE    ] = SHELL_ALIGN(sizeof(struct ncase)),
	[NCLIST   ] = SHELL_ALIGN(sizeof(struct nclist)),
	[NDEFUN   ] = SHELL_ALIGN(sizeof(struct narg)),
	[NARG     ] = SHELL_ALIGN(sizeof(struct narg)),
	[NTO      ] = SHELL_ALIGN(sizeof(struct nfile)),
#if ENABLE_ASH_BASH_COMPAT
	[NTO2     ] = SHELL_ALIGN(sizeof(struct nfile)),
#endif
	[NCLOBBER ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NFROM    ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NFROMTO  ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NAPPEND  ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NTOFD    ] = SHELL_ALIGN(sizeof(struct ndup)),
	[NFROMFD  ] = SHELL_ALIGN(sizeof(struct ndup)),
	[NHERE    ] = SHELL_ALIGN(sizeof(struct nhere)),
	[NXHERE   ] = SHELL_ALIGN(sizeof(struct nhere)),
	[NNOT     ] = SHELL_ALIGN(sizeof(struct nnot)),
};

static void calcsize(union node *n);

static void
sizenodelist(struct nodelist *lp)
{
	while (lp) {
		funcblocksize += SHELL_ALIGN(sizeof(struct nodelist));
		calcsize(lp->n);
		lp = lp->next;
	}
}

static void
calcsize(union node *n)
{
	if (n == NULL)
		return;
	funcblocksize += nodesize[n->type];
	switch (n->type) {
	case NCMD:
		calcsize(n->ncmd.redirect);
		calcsize(n->ncmd.args);
		calcsize(n->ncmd.assign);
		break;
	case NPIPE:
		sizenodelist(n->npipe.cmdlist);
		break;
	case NREDIR:
	case NBACKGND:
	case NSUBSHELL:
		calcsize(n->nredir.redirect);
		calcsize(n->nredir.n);
		break;
	case NAND:
	case NOR:
	case NSEMI:
	case NWHILE:
	case NUNTIL:
		calcsize(n->nbinary.ch2);
		calcsize(n->nbinary.ch1);
		break;
	case NIF:
		calcsize(n->nif.elsepart);
		calcsize(n->nif.ifpart);
		calcsize(n->nif.test);
		break;
	case NFOR:
		funcstringsize += strlen(n->nfor.var) + 1;
		calcsize(n->nfor.body);
		calcsize(n->nfor.args);
		break;
	case NCASE:
		calcsize(n->ncase.cases);
		calcsize(n->ncase.expr);
		break;
	case NCLIST:
		calcsize(n->nclist.body);
		calcsize(n->nclist.pattern);
		calcsize(n->nclist.next);
		break;
	case NDEFUN:
	case NARG:
		sizenodelist(n->narg.backquote);
		funcstringsize += strlen(n->narg.text) + 1;
		calcsize(n->narg.next);
		break;
	case NTO:
#if ENABLE_ASH_BASH_COMPAT
	case NTO2:
#endif
	case NCLOBBER:
	case NFROM:
	case NFROMTO:
	case NAPPEND:
		calcsize(n->nfile.fname);
		calcsize(n->nfile.next);
		break;
	case NTOFD:
	case NFROMFD:
		calcsize(n->ndup.vname);
		calcsize(n->ndup.next);
	break;
	case NHERE:
	case NXHERE:
		calcsize(n->nhere.doc);
		calcsize(n->nhere.next);
		break;
	case NNOT:
		calcsize(n->nnot.com);
		break;
	};
}

static char *
nodeckstrdup(char *s)
{
	char *rtn = funcstring;

	strcpy(funcstring, s);
	funcstring += strlen(s) + 1;
	return rtn;
}

static union node *copynode(union node *);

static struct nodelist *
copynodelist(struct nodelist *lp)
{
	struct nodelist *start;
	struct nodelist **lpp;

	lpp = &start;
	while (lp) {
		*lpp = funcblock;
		funcblock = (char *) funcblock + SHELL_ALIGN(sizeof(struct nodelist));
		(*lpp)->n = copynode(lp->n);
		lp = lp->next;
		lpp = &(*lpp)->next;
	}
	*lpp = NULL;
	return start;
}

static union node *
copynode(union node *n)
{
	union node *new;

	if (n == NULL)
		return NULL;
	new = funcblock;
	funcblock = (char *) funcblock + nodesize[n->type];

	switch (n->type) {
	case NCMD:
		new->ncmd.redirect = copynode(n->ncmd.redirect);
		new->ncmd.args = copynode(n->ncmd.args);
		new->ncmd.assign = copynode(n->ncmd.assign);
		break;
	case NPIPE:
		new->npipe.cmdlist = copynodelist(n->npipe.cmdlist);
		new->npipe.pipe_backgnd = n->npipe.pipe_backgnd;
		break;
	case NREDIR:
	case NBACKGND:
	case NSUBSHELL:
		new->nredir.redirect = copynode(n->nredir.redirect);
		new->nredir.n = copynode(n->nredir.n);
		break;
	case NAND:
	case NOR:
	case NSEMI:
	case NWHILE:
	case NUNTIL:
		new->nbinary.ch2 = copynode(n->nbinary.ch2);
		new->nbinary.ch1 = copynode(n->nbinary.ch1);
		break;
	case NIF:
		new->nif.elsepart = copynode(n->nif.elsepart);
		new->nif.ifpart = copynode(n->nif.ifpart);
		new->nif.test = copynode(n->nif.test);
		break;
	case NFOR:
		new->nfor.var = nodeckstrdup(n->nfor.var);
		new->nfor.body = copynode(n->nfor.body);
		new->nfor.args = copynode(n->nfor.args);
		break;
	case NCASE:
		new->ncase.cases = copynode(n->ncase.cases);
		new->ncase.expr = copynode(n->ncase.expr);
		break;
	case NCLIST:
		new->nclist.body = copynode(n->nclist.body);
		new->nclist.pattern = copynode(n->nclist.pattern);
		new->nclist.next = copynode(n->nclist.next);
		break;
	case NDEFUN:
	case NARG:
		new->narg.backquote = copynodelist(n->narg.backquote);
		new->narg.text = nodeckstrdup(n->narg.text);
		new->narg.next = copynode(n->narg.next);
		break;
	case NTO:
#if ENABLE_ASH_BASH_COMPAT
	case NTO2:
#endif
	case NCLOBBER:
	case NFROM:
	case NFROMTO:
	case NAPPEND:
		new->nfile.fname = copynode(n->nfile.fname);
		new->nfile.fd = n->nfile.fd;
		new->nfile.next = copynode(n->nfile.next);
		break;
	case NTOFD:
	case NFROMFD:
		new->ndup.vname = copynode(n->ndup.vname);
		new->ndup.dupfd = n->ndup.dupfd;
		new->ndup.fd = n->ndup.fd;
		new->ndup.next = copynode(n->ndup.next);
		break;
	case NHERE:
	case NXHERE:
		new->nhere.doc = copynode(n->nhere.doc);
		new->nhere.fd = n->nhere.fd;
		new->nhere.next = copynode(n->nhere.next);
		break;
	case NNOT:
		new->nnot.com = copynode(n->nnot.com);
		break;
	};
	new->type = n->type;
	return new;
}

/*
 * Make a copy of a parse tree.
 */
static struct funcnode *
copyfunc(union node *n)
{
	struct funcnode *f;
	size_t blocksize;

	funcblocksize = offsetof(struct funcnode, n);
	funcstringsize = 0;
	calcsize(n);
	blocksize = funcblocksize;
	f = ckmalloc(blocksize + funcstringsize);
	funcblock = (char *) f + offsetof(struct funcnode, n);
	funcstring = (char *) f + blocksize;
	copynode(n);
	f->count = 0;
	return f;
}

/*
 * Define a shell function.
 */
static void
defun(char *name, union node *func)
{
	struct cmdentry entry;

	INT_OFF;
	entry.cmdtype = CMDFUNCTION;
	entry.u.func = copyfunc(func);
	addcmdentry(name, &entry);
	INT_ON;
}

/* Reasons for skipping commands (see comment on breakcmd routine) */
#define SKIPBREAK      (1 << 0)
#define SKIPCONT       (1 << 1)
#define SKIPFUNC       (1 << 2)
#define SKIPFILE       (1 << 3)
#define SKIPEVAL       (1 << 4)
static smallint evalskip;       /* set to SKIPxxx if we are skipping commands */
static int skipcount;           /* number of levels to skip */
static int funcnest;            /* depth of function calls */
static int loopnest;            /* current loop nesting level */

/* Forward decl way out to parsing code - dotrap needs it */
static int evalstring(char *s, int mask);

/* Called to execute a trap.
 * Single callsite - at the end of evaltree().
 * If we return non-zero, evaltree raises EXEXIT exception.
 *
 * Perhaps we should avoid entering new trap handlers
 * while we are executing a trap handler. [is it a TODO?]
 */
static int
dotrap(void)
{
	uint8_t *g;
	int sig;
	uint8_t savestatus;

	savestatus = exitstatus;
	pending_sig = 0;
	xbarrier();

	TRACE(("dotrap entered\n"));
	for (sig = 1, g = gotsig; sig < NSIG; sig++, g++) {
		int want_exexit;
		char *t;

		if (*g == 0)
			continue;
		t = trap[sig];
		/* non-trapped SIGINT is handled separately by raise_interrupt,
		 * don't upset it by resetting gotsig[SIGINT-1] */
		if (sig == SIGINT && !t)
			continue;

		TRACE(("sig %d is active, will run handler '%s'\n", sig, t));
		*g = 0;
		if (!t)
			continue;
		want_exexit = evalstring(t, SKIPEVAL);
		exitstatus = savestatus;
		if (want_exexit) {
			TRACE(("dotrap returns %d\n", want_exexit));
			return want_exexit;
		}
	}

	TRACE(("dotrap returns 0\n"));
	return 0;
}

/* forward declarations - evaluation is fairly recursive business... */
static void evalloop(union node *, int);
static void evalfor(union node *, int);
static void evalcase(union node *, int);
static void evalsubshell(union node *, int);
static void expredir(union node *);
static void evalpipe(union node *, int);
static void evalcommand(union node *, int);
static int evalbltin(const struct builtincmd *, int, char **);
static void prehash(union node *);

/*
 * Evaluate a parse tree.  The value is left in the global variable
 * exitstatus.
 */
static void
evaltree(union node *n, int flags)
{
	struct jmploc *volatile savehandler = exception_handler;
	struct jmploc jmploc;
	int checkexit = 0;
	void (*evalfn)(union node *, int);
	int status;
	int int_level;

	SAVE_INT(int_level);

	if (n == NULL) {
		TRACE(("evaltree(NULL) called\n"));
		goto out1;
	}
	TRACE(("evaltree(%p: %d, %d) called\n", n, n->type, flags));

	exception_handler = &jmploc;
	{
		int err = setjmp(jmploc.loc);
		if (err) {
			/* if it was a signal, check for trap handlers */
			if (exception_type == EXSIG) {
				TRACE(("exception %d (EXSIG) in evaltree, err=%d\n",
						exception_type, err));
				goto out;
			}
			/* continue on the way out */
			TRACE(("exception %d in evaltree, propagating err=%d\n",
					exception_type, err));
			exception_handler = savehandler;
			longjmp(exception_handler->loc, err);
		}
	}

	switch (n->type) {
	default:
#if DEBUG
		out1fmt("Node type = %d\n", n->type);
		fflush_all();
		break;
#endif
	case NNOT:
		evaltree(n->nnot.com, EV_TESTED);
		status = !exitstatus;
		goto setstatus;
	case NREDIR:
		expredir(n->nredir.redirect);
		status = redirectsafe(n->nredir.redirect, REDIR_PUSH);
		if (!status) {
			evaltree(n->nredir.n, flags & EV_TESTED);
			status = exitstatus;
		}
		popredir(/*drop:*/ 0, /*restore:*/ 0 /* not sure */);
		goto setstatus;
	case NCMD:
		evalfn = evalcommand;
 checkexit:
		if (eflag && !(flags & EV_TESTED))
			checkexit = ~0;
		goto calleval;
	case NFOR:
		evalfn = evalfor;
		goto calleval;
	case NWHILE:
	case NUNTIL:
		evalfn = evalloop;
		goto calleval;
	case NSUBSHELL:
	case NBACKGND:
		evalfn = evalsubshell;
		goto calleval;
	case NPIPE:
		evalfn = evalpipe;
		goto checkexit;
	case NCASE:
		evalfn = evalcase;
		goto calleval;
	case NAND:
	case NOR:
	case NSEMI: {

#if NAND + 1 != NOR
#error NAND + 1 != NOR
#endif
#if NOR + 1 != NSEMI
#error NOR + 1 != NSEMI
#endif
		unsigned is_or = n->type - NAND;
		evaltree(
			n->nbinary.ch1,
			(flags | ((is_or >> 1) - 1)) & EV_TESTED
		);
		if (!exitstatus == is_or)
			break;
		if (!evalskip) {
			n = n->nbinary.ch2;
 evaln:
			evalfn = evaltree;
 calleval:
			evalfn(n, flags);
			break;
		}
		break;
	}
	case NIF:
		evaltree(n->nif.test, EV_TESTED);
		if (evalskip)
			break;
		if (exitstatus == 0) {
			n = n->nif.ifpart;
			goto evaln;
		}
		if (n->nif.elsepart) {
			n = n->nif.elsepart;
			goto evaln;
		}
		goto success;
	case NDEFUN:
		defun(n->narg.text, n->narg.next);
 success:
		status = 0;
 setstatus:
		exitstatus = status;
		break;
	}

 out:
	exception_handler = savehandler;

 out1:
	/* Order of checks below is important:
	 * signal handlers trigger before exit caused by "set -e".
	 */
	if (pending_sig && dotrap())
		goto exexit;
	if (checkexit & exitstatus)
		evalskip |= SKIPEVAL;

	if (flags & EV_EXIT) {
 exexit:
		raise_exception(EXEXIT);
	}

	RESTORE_INT(int_level);
	TRACE(("leaving evaltree (no interrupts)\n"));
}

#if !defined(__alpha__) || (defined(__GNUC__) && __GNUC__ >= 3)
static
#endif
void evaltreenr(union node *, int) __attribute__ ((alias("evaltree"),__noreturn__));

static void
evalloop(union node *n, int flags)
{
	int status;

	loopnest++;
	status = 0;
	flags &= EV_TESTED;
	for (;;) {
		int i;

		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip) {
 skipping:
			if (evalskip == SKIPCONT && --skipcount <= 0) {
				evalskip = 0;
				continue;
			}
			if (evalskip == SKIPBREAK && --skipcount <= 0)
				evalskip = 0;
			break;
		}
		i = exitstatus;
		if (n->type != NWHILE)
			i = !i;
		if (i != 0)
			break;
		evaltree(n->nbinary.ch2, flags);
		status = exitstatus;
		if (evalskip)
			goto skipping;
	}
	loopnest--;
	exitstatus = status;
}

static void
evalfor(union node *n, int flags)
{
	struct arglist arglist;
	union node *argp;
	struct strlist *sp;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.list = NULL;
	arglist.lastp = &arglist.list;
	for (argp = n->nfor.args; argp; argp = argp->narg.next) {
		expandarg(argp, &arglist, EXP_FULL | EXP_TILDE | EXP_RECORD);
		/* XXX */
		if (evalskip)
			goto out;
	}
	*arglist.lastp = NULL;

	exitstatus = 0;
	loopnest++;
	flags &= EV_TESTED;
	for (sp = arglist.list; sp; sp = sp->next) {
		setvar(n->nfor.var, sp->text, 0);
		evaltree(n->nfor.body, flags);
		if (evalskip) {
			if (evalskip == SKIPCONT && --skipcount <= 0) {
				evalskip = 0;
				continue;
			}
			if (evalskip == SKIPBREAK && --skipcount <= 0)
				evalskip = 0;
			break;
		}
	}
	loopnest--;
 out:
	popstackmark(&smark);
}

static void
evalcase(union node *n, int flags)
{
	union node *cp;
	union node *patp;
	struct arglist arglist;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.list = NULL;
	arglist.lastp = &arglist.list;
	expandarg(n->ncase.expr, &arglist, EXP_TILDE);
	exitstatus = 0;
	for (cp = n->ncase.cases; cp && evalskip == 0; cp = cp->nclist.next) {
		for (patp = cp->nclist.pattern; patp; patp = patp->narg.next) {
			if (casematch(patp, arglist.list->text)) {
				if (evalskip == 0) {
					evaltree(cp->nclist.body, flags);
				}
				goto out;
			}
		}
	}
 out:
	popstackmark(&smark);
}

/*
 * Kick off a subshell to evaluate a tree.
 */
static void
evalsubshell(union node *n, int flags)
{
	struct job *jp;
	int backgnd = (n->type == NBACKGND);
	int status;

	expredir(n->nredir.redirect);
	if (!backgnd && (flags & EV_EXIT) && !may_have_traps)
		goto nofork;
	INT_OFF;
	jp = makejob(/*n,*/ 1);
	if (forkshell(jp, n, backgnd) == 0) {
		/* child */
		INT_ON;
		flags |= EV_EXIT;
		if (backgnd)
			flags &= ~EV_TESTED;
 nofork:
		redirect(n->nredir.redirect, 0);
		evaltreenr(n->nredir.n, flags);
		/* never returns */
	}
	status = 0;
	if (!backgnd)
		status = waitforjob(jp);
	exitstatus = status;
	INT_ON;
}

/*
 * Compute the names of the files in a redirection list.
 */
static void fixredir(union node *, const char *, int);
static void
expredir(union node *n)
{
	union node *redir;

	for (redir = n; redir; redir = redir->nfile.next) {
		struct arglist fn;

		fn.list = NULL;
		fn.lastp = &fn.list;
		switch (redir->type) {
		case NFROMTO:
		case NFROM:
		case NTO:
#if ENABLE_ASH_BASH_COMPAT
		case NTO2:
#endif
		case NCLOBBER:
		case NAPPEND:
			expandarg(redir->nfile.fname, &fn, EXP_TILDE | EXP_REDIR);
#if ENABLE_ASH_BASH_COMPAT
 store_expfname:
#endif
			redir->nfile.expfname = fn.list->text;
			break;
		case NFROMFD:
		case NTOFD: /* >& */
			if (redir->ndup.vname) {
				expandarg(redir->ndup.vname, &fn, EXP_FULL | EXP_TILDE);
				if (fn.list == NULL)
					ash_msg_and_raise_error("redir error");
#if ENABLE_ASH_BASH_COMPAT
//FIXME: we used expandarg with different args!
				if (!isdigit_str9(fn.list->text)) {
					/* >&file, not >&fd */
					if (redir->nfile.fd != 1) /* 123>&file - BAD */
						ash_msg_and_raise_error("redir error");
					redir->type = NTO2;
					goto store_expfname;
				}
#endif
				fixredir(redir, fn.list->text, 1);
			}
			break;
		}
	}
}

/*
 * Evaluate a pipeline.  All the processes in the pipeline are children
 * of the process creating the pipeline.  (This differs from some versions
 * of the shell, which make the last process in a pipeline the parent
 * of all the rest.)
 */
static void
evalpipe(union node *n, int flags)
{
	struct job *jp;
	struct nodelist *lp;
	int pipelen;
	int prevfd;
	int pip[2];

	TRACE(("evalpipe(0x%lx) called\n", (long)n));
	pipelen = 0;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next)
		pipelen++;
	flags |= EV_EXIT;
	INT_OFF;
	jp = makejob(/*n,*/ pipelen);
	prevfd = -1;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next) {
		prehash(lp->n);
		pip[1] = -1;
		if (lp->next) {
			if (pipe(pip) < 0) {
				close(prevfd);
				ash_msg_and_raise_error("pipe call failed");
			}
		}
		if (forkshell(jp, lp->n, n->npipe.pipe_backgnd) == 0) {
			INT_ON;
			if (pip[1] >= 0) {
				close(pip[0]);
			}
			if (prevfd > 0) {
				dup2(prevfd, 0);
				close(prevfd);
			}
			if (pip[1] > 1) {
				dup2(pip[1], 1);
				close(pip[1]);
			}
			evaltreenr(lp->n, flags);
			/* never returns */
		}
		if (prevfd >= 0)
			close(prevfd);
		prevfd = pip[0];
		/* Don't want to trigger debugging */
		if (pip[1] != -1)
			close(pip[1]);
	}
	if (n->npipe.pipe_backgnd == 0) {
		exitstatus = waitforjob(jp);
		TRACE(("evalpipe:  job done exit status %d\n", exitstatus));
	}
	INT_ON;
}

/*
 * Controls whether the shell is interactive or not.
 */
static void
setinteractive(int on)
{
	static smallint is_interactive;

	if (++on == is_interactive)
		return;
	is_interactive = on;
	setsignal(SIGINT);
	setsignal(SIGQUIT);
	setsignal(SIGTERM);
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
	if (is_interactive > 1) {
		/* Looks like they want an interactive shell */
		static smallint did_banner;

		if (!did_banner) {
			/* note: ash and hush share this string */
			out1fmt("\n\n%s %s\n"
				"Enter 'help' for a list of built-in commands."
				"\n\n",
				bb_banner,
				"built-in shell (ash)"
			);
			did_banner = 1;
		}
	}
#endif
}

static void
optschanged(void)
{
#if DEBUG
	opentrace();
#endif
	setinteractive(iflag);
	setjobctl(mflag);
#if ENABLE_FEATURE_EDITING_VI
	if (viflag)
		line_input_state->flags |= VI_MODE;
	else
		line_input_state->flags &= ~VI_MODE;
#else
	viflag = 0; /* forcibly keep the option off */
#endif
}

static struct localvar *localvars;

/*
 * Called after a function returns.
 * Interrupts must be off.
 */
static void
poplocalvars(void)
{
	struct localvar *lvp;
	struct var *vp;

	while ((lvp = localvars) != NULL) {
		localvars = lvp->next;
		vp = lvp->vp;
		TRACE(("poplocalvar %s\n", vp ? vp->var_text : "-"));
		if (vp == NULL) {       /* $- saved */
			memcpy(optlist, lvp->text, sizeof(optlist));
			free((char*)lvp->text);
			optschanged();
		} else if ((lvp->flags & (VUNSET|VSTRFIXED)) == VUNSET) {
			unsetvar(vp->var_text);
		} else {
			if (vp->var_func)
				vp->var_func(var_end(lvp->text));
			if ((vp->flags & (VTEXTFIXED|VSTACK)) == 0)
				free((char*)vp->var_text);
			vp->flags = lvp->flags;
			vp->var_text = lvp->text;
		}
		free(lvp);
	}
}

static int
evalfun(struct funcnode *func, int argc, char **argv, int flags)
{
	volatile struct shparam saveparam;
	struct localvar *volatile savelocalvars;
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
	int e;

	saveparam = shellparam;
	savelocalvars = localvars;
	e = setjmp(jmploc.loc);
	if (e) {
		goto funcdone;
	}
	INT_OFF;
	savehandler = exception_handler;
	exception_handler = &jmploc;
	localvars = NULL;
	shellparam.malloced = 0;
	func->count++;
	funcnest++;
	INT_ON;
	shellparam.nparam = argc - 1;
	shellparam.p = argv + 1;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	evaltree(&func->n, flags & EV_TESTED);
 funcdone:
	INT_OFF;
	funcnest--;
	freefunc(func);
	poplocalvars();
	localvars = savelocalvars;
	freeparam(&shellparam);
	shellparam = saveparam;
	exception_handler = savehandler;
	INT_ON;
	evalskip &= ~SKIPFUNC;
	return e;
}

#if ENABLE_ASH_CMDCMD
static char **
parse_command_args(char **argv, const char **path)
{
	char *cp, c;

	for (;;) {
		cp = *++argv;
		if (!cp)
			return 0;
		if (*cp++ != '-')
			break;
		c = *cp++;
		if (!c)
			break;
		if (c == '-' && !*cp) {
			argv++;
			break;
		}
		do {
			switch (c) {
			case 'p':
				*path = bb_default_path;
				break;
			default:
				/* run 'typecmd' for other options */
				return 0;
			}
			c = *cp++;
		} while (c);
	}
	return argv;
}
#endif

/*
 * Make a variable a local variable.  When a variable is made local, it's
 * value and flags are saved in a localvar structure.  The saved values
 * will be restored when the shell function returns.  We handle the name
 * "-" as a special case.
 */
static void
mklocal(char *name)
{
	struct localvar *lvp;
	struct var **vpp;
	struct var *vp;

	INT_OFF;
	lvp = ckzalloc(sizeof(struct localvar));
	if (LONE_DASH(name)) {
		char *p;
		p = ckmalloc(sizeof(optlist));
		lvp->text = memcpy(p, optlist, sizeof(optlist));
		vp = NULL;
	} else {
		char *eq;

		vpp = hashvar(name);
		vp = *findvar(vpp, name);
		eq = strchr(name, '=');
		if (vp == NULL) {
			if (eq)
				setvareq(name, VSTRFIXED);
			else
				setvar(name, NULL, VSTRFIXED);
			vp = *vpp;      /* the new variable */
			lvp->flags = VUNSET;
		} else {
			lvp->text = vp->var_text;
			lvp->flags = vp->flags;
			vp->flags |= VSTRFIXED|VTEXTFIXED;
			if (eq)
				setvareq(name, 0);
		}
	}
	lvp->vp = vp;
	lvp->next = localvars;
	localvars = lvp;
	INT_ON;
}

/*
 * The "local" command.
 */
static int FAST_FUNC
localcmd(int argc UNUSED_PARAM, char **argv)
{
	char *name;

	argv = argptr;
	while ((name = *argv++) != NULL) {
		mklocal(name);
	}
	return 0;
}

static int FAST_FUNC
falsecmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	return 1;
}

static int FAST_FUNC
truecmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	return 0;
}

static int FAST_FUNC
execcmd(int argc UNUSED_PARAM, char **argv)
{
	if (argv[1]) {
		iflag = 0;              /* exit on error */
		mflag = 0;
		optschanged();
		shellexec(argv + 1, pathval(), 0);
	}
	return 0;
}

/*
 * The return command.
 */
static int FAST_FUNC
returncmd(int argc UNUSED_PARAM, char **argv)
{
	/*
	 * If called outside a function, do what ksh does;
	 * skip the rest of the file.
	 */
	evalskip = funcnest ? SKIPFUNC : SKIPFILE;
	return argv[1] ? number(argv[1]) : exitstatus;
}

/* Forward declarations for builtintab[] */
static int breakcmd(int, char **) FAST_FUNC;
static int dotcmd(int, char **) FAST_FUNC;
static int evalcmd(int, char **) FAST_FUNC;
static int exitcmd(int, char **) FAST_FUNC;
static int exportcmd(int, char **) FAST_FUNC;
#if ENABLE_ASH_GETOPTS
static int getoptscmd(int, char **) FAST_FUNC;
#endif
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
static int helpcmd(int, char **) FAST_FUNC;
#endif
#if ENABLE_SH_MATH_SUPPORT
static int letcmd(int, char **) FAST_FUNC;
#endif
static int readcmd(int, char **) FAST_FUNC;
static int setcmd(int, char **) FAST_FUNC;
static int shiftcmd(int, char **) FAST_FUNC;
static int timescmd(int, char **) FAST_FUNC;
static int trapcmd(int, char **) FAST_FUNC;
static int umaskcmd(int, char **) FAST_FUNC;
static int unsetcmd(int, char **) FAST_FUNC;
static int ulimitcmd(int, char **) FAST_FUNC;

#define BUILTIN_NOSPEC          "0"
#define BUILTIN_SPECIAL         "1"
#define BUILTIN_REGULAR         "2"
#define BUILTIN_SPEC_REG        "3"
#define BUILTIN_ASSIGN          "4"
#define BUILTIN_SPEC_ASSG       "5"
#define BUILTIN_REG_ASSG        "6"
#define BUILTIN_SPEC_REG_ASSG   "7"

/* Stubs for calling non-FAST_FUNC's */
#if ENABLE_ASH_BUILTIN_ECHO
static int FAST_FUNC echocmd(int argc, char **argv)   { return echo_main(argc, argv); }
#endif
#if ENABLE_ASH_BUILTIN_PRINTF
static int FAST_FUNC printfcmd(int argc, char **argv) { return printf_main(argc, argv); }
#endif
#if ENABLE_ASH_BUILTIN_TEST
static int FAST_FUNC testcmd(int argc, char **argv)   { return test_main(argc, argv); }
#endif

/* Keep these in proper order since it is searched via bsearch() */
static const struct builtincmd builtintab[] = {
	{ BUILTIN_SPEC_REG      "."       , dotcmd     },
	{ BUILTIN_SPEC_REG      ":"       , truecmd    },
#if ENABLE_ASH_BUILTIN_TEST
	{ BUILTIN_REGULAR       "["       , testcmd    },
#if ENABLE_ASH_BASH_COMPAT
	{ BUILTIN_REGULAR       "[["      , testcmd    },
#endif
#endif
#if ENABLE_ASH_ALIAS
	{ BUILTIN_REG_ASSG      "alias"   , aliascmd   },
#endif
#if JOBS
	{ BUILTIN_REGULAR       "bg"      , fg_bgcmd   },
#endif
	{ BUILTIN_SPEC_REG      "break"   , breakcmd   },
	{ BUILTIN_REGULAR       "cd"      , cdcmd      },
	{ BUILTIN_NOSPEC        "chdir"   , cdcmd      },
#if ENABLE_ASH_CMDCMD
	{ BUILTIN_REGULAR       "command" , commandcmd },
#endif
	{ BUILTIN_SPEC_REG      "continue", breakcmd   },
#if ENABLE_ASH_BUILTIN_ECHO
	{ BUILTIN_REGULAR       "echo"    , echocmd    },
#endif
	{ BUILTIN_SPEC_REG      "eval"    , evalcmd    },
	{ BUILTIN_SPEC_REG      "exec"    , execcmd    },
	{ BUILTIN_SPEC_REG      "exit"    , exitcmd    },
	{ BUILTIN_SPEC_REG_ASSG "export"  , exportcmd  },
	{ BUILTIN_REGULAR       "false"   , falsecmd   },
#if JOBS
	{ BUILTIN_REGULAR       "fg"      , fg_bgcmd   },
#endif
#if ENABLE_ASH_GETOPTS
	{ BUILTIN_REGULAR       "getopts" , getoptscmd },
#endif
	{ BUILTIN_NOSPEC        "hash"    , hashcmd    },
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
	{ BUILTIN_NOSPEC        "help"    , helpcmd    },
#endif
#if JOBS
	{ BUILTIN_REGULAR       "jobs"    , jobscmd    },
	{ BUILTIN_REGULAR       "kill"    , killcmd    },
#endif
#if ENABLE_SH_MATH_SUPPORT
	{ BUILTIN_NOSPEC        "let"     , letcmd     },
#endif
	{ BUILTIN_ASSIGN        "local"   , localcmd   },
#if ENABLE_ASH_BUILTIN_PRINTF
	{ BUILTIN_REGULAR       "printf"  , printfcmd  },
#endif
	{ BUILTIN_NOSPEC        "pwd"     , pwdcmd     },
	{ BUILTIN_REGULAR       "read"    , readcmd    },
	{ BUILTIN_SPEC_REG_ASSG "readonly", exportcmd  },
	{ BUILTIN_SPEC_REG      "return"  , returncmd  },
	{ BUILTIN_SPEC_REG      "set"     , setcmd     },
	{ BUILTIN_SPEC_REG      "shift"   , shiftcmd   },
#if ENABLE_ASH_BASH_COMPAT
	{ BUILTIN_SPEC_REG      "source"  , dotcmd     },
#endif
#if ENABLE_ASH_BUILTIN_TEST
	{ BUILTIN_REGULAR       "test"    , testcmd    },
#endif
	{ BUILTIN_SPEC_REG      "times"   , timescmd   },
	{ BUILTIN_SPEC_REG      "trap"    , trapcmd    },
	{ BUILTIN_REGULAR       "true"    , truecmd    },
	{ BUILTIN_NOSPEC        "type"    , typecmd    },
	{ BUILTIN_NOSPEC        "ulimit"  , ulimitcmd  },
	{ BUILTIN_REGULAR       "umask"   , umaskcmd   },
#if ENABLE_ASH_ALIAS
	{ BUILTIN_REGULAR       "unalias" , unaliascmd },
#endif
	{ BUILTIN_SPEC_REG      "unset"   , unsetcmd   },
	{ BUILTIN_REGULAR       "wait"    , waitcmd    },
};

/* Should match the above table! */
#define COMMANDCMD (builtintab + \
	2 + \
	1 * ENABLE_ASH_BUILTIN_TEST + \
	1 * ENABLE_ASH_BUILTIN_TEST * ENABLE_ASH_BASH_COMPAT + \
	1 * ENABLE_ASH_ALIAS + \
	1 * ENABLE_ASH_JOB_CONTROL + \
	3)
#define EXECCMD (builtintab + \
	2 + \
	1 * ENABLE_ASH_BUILTIN_TEST + \
	1 * ENABLE_ASH_BUILTIN_TEST * ENABLE_ASH_BASH_COMPAT + \
	1 * ENABLE_ASH_ALIAS + \
	1 * ENABLE_ASH_JOB_CONTROL + \
	3 + \
	1 * ENABLE_ASH_CMDCMD + \
	1 + \
	ENABLE_ASH_BUILTIN_ECHO + \
	1)

/*
 * Search the table of builtin commands.
 */
static struct builtincmd *
find_builtin(const char *name)
{
	struct builtincmd *bp;

	bp = bsearch(
		name, builtintab, ARRAY_SIZE(builtintab), sizeof(builtintab[0]),
		pstrcmp
	);
	return bp;
}

/*
 * Execute a simple command.
 */
static int
isassignment(const char *p)
{
	const char *q = endofname(p);
	if (p == q)
		return 0;
	return *q == '=';
}
static int FAST_FUNC
bltincmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	/* Preserve exitstatus of a previous possible redirection
	 * as POSIX mandates */
	return back_exitstatus;
}
static void
evalcommand(union node *cmd, int flags)
{
	static const struct builtincmd null_bltin = {
		"\0\0", bltincmd /* why three NULs? */
	};
	struct stackmark smark;
	union node *argp;
	struct arglist arglist;
	struct arglist varlist;
	char **argv;
	int argc;
	const struct strlist *sp;
	struct cmdentry cmdentry;
	struct job *jp;
	char *lastarg;
	const char *path;
	int spclbltin;
	int status;
	char **nargv;
	struct builtincmd *bcmd;
	smallint cmd_is_exec;
	smallint pseudovarflag = 0;

	/* First expand the arguments. */
	TRACE(("evalcommand(0x%lx, %d) called\n", (long)cmd, flags));
	setstackmark(&smark);
	back_exitstatus = 0;

	cmdentry.cmdtype = CMDBUILTIN;
	cmdentry.u.cmd = &null_bltin;
	varlist.lastp = &varlist.list;
	*varlist.lastp = NULL;
	arglist.lastp = &arglist.list;
	*arglist.lastp = NULL;

	argc = 0;
	if (cmd->ncmd.args) {
		bcmd = find_builtin(cmd->ncmd.args->narg.text);
		pseudovarflag = bcmd && IS_BUILTIN_ASSIGN(bcmd);
	}

	for (argp = cmd->ncmd.args; argp; argp = argp->narg.next) {
		struct strlist **spp;

		spp = arglist.lastp;
		if (pseudovarflag && isassignment(argp->narg.text))
			expandarg(argp, &arglist, EXP_VARTILDE);
		else
			expandarg(argp, &arglist, EXP_FULL | EXP_TILDE);

		for (sp = *spp; sp; sp = sp->next)
			argc++;
	}

	argv = nargv = stalloc(sizeof(char *) * (argc + 1));
	for (sp = arglist.list; sp; sp = sp->next) {
		TRACE(("evalcommand arg: %s\n", sp->text));
		*nargv++ = sp->text;
	}
	*nargv = NULL;

	lastarg = NULL;
	if (iflag && funcnest == 0 && argc > 0)
		lastarg = nargv[-1];

	preverrout_fd = 2;
	expredir(cmd->ncmd.redirect);
	status = redirectsafe(cmd->ncmd.redirect, REDIR_PUSH | REDIR_SAVEFD2);

	path = vpath.var_text;
	for (argp = cmd->ncmd.assign; argp; argp = argp->narg.next) {
		struct strlist **spp;
		char *p;

		spp = varlist.lastp;
		expandarg(argp, &varlist, EXP_VARTILDE);

		/*
		 * Modify the command lookup path, if a PATH= assignment
		 * is present
		 */
		p = (*spp)->text;
		if (varcmp(p, path) == 0)
			path = p;
	}

	/* Print the command if xflag is set. */
	if (xflag) {
		int n;
		const char *p = " %s" + 1;

		fdprintf(preverrout_fd, p, expandstr(ps4val()));
		sp = varlist.list;
		for (n = 0; n < 2; n++) {
			while (sp) {
				fdprintf(preverrout_fd, p, sp->text);
				sp = sp->next;
				p = " %s";
			}
			sp = arglist.list;
		}
		safe_write(preverrout_fd, "\n", 1);
	}

	cmd_is_exec = 0;
	spclbltin = -1;

	/* Now locate the command. */
	if (argc) {
		const char *oldpath;
		int cmd_flag = DO_ERR;

		path += 5;
		oldpath = path;
		for (;;) {
			find_command(argv[0], &cmdentry, cmd_flag, path);
			if (cmdentry.cmdtype == CMDUNKNOWN) {
				flush_stdout_stderr();
				status = 127;
				goto bail;
			}

			/* implement bltin and command here */
			if (cmdentry.cmdtype != CMDBUILTIN)
				break;
			if (spclbltin < 0)
				spclbltin = IS_BUILTIN_SPECIAL(cmdentry.u.cmd);
			if (cmdentry.u.cmd == EXECCMD)
				cmd_is_exec = 1;
#if ENABLE_ASH_CMDCMD
			if (cmdentry.u.cmd == COMMANDCMD) {
				path = oldpath;
				nargv = parse_command_args(argv, &path);
				if (!nargv)
					break;
				argc -= nargv - argv;
				argv = nargv;
				cmd_flag |= DO_NOFUNC;
			} else
#endif
				break;
		}
	}

	if (status) {
		/* We have a redirection error. */
		if (spclbltin > 0)
			raise_exception(EXERROR);
 bail:
		exitstatus = status;
		goto out;
	}

	/* Execute the command. */
	switch (cmdentry.cmdtype) {
	default: {

#if ENABLE_FEATURE_SH_NOFORK
/* (1) BUG: if variables are set, we need to fork, or save/restore them
 *     around run_nofork_applet() call.
 * (2) Should this check also be done in forkshell()?
 *     (perhaps it should, so that "VAR=VAL nofork" at least avoids exec...)
 */
		/* find_command() encodes applet_no as (-2 - applet_no) */
		int applet_no = (- cmdentry.u.index - 2);
		if (applet_no >= 0 && APPLET_IS_NOFORK(applet_no)) {
			listsetvar(varlist.list, VEXPORT|VSTACK);
			/* run <applet>_main() */
			exitstatus = run_nofork_applet(applet_no, argv);
			break;
		}
#endif
		/* Can we avoid forking off? For example, very last command
		 * in a script or a subshell does not need forking,
		 * we can just exec it.
		 */
		if (!(flags & EV_EXIT) || may_have_traps) {
			/* No, forking off a child is necessary */
			INT_OFF;
			jp = makejob(/*cmd,*/ 1);
			if (forkshell(jp, cmd, FORK_FG) != 0) {
				/* parent */
				exitstatus = waitforjob(jp);
				INT_ON;
				TRACE(("forked child exited with %d\n", exitstatus));
				break;
			}
			/* child */
			FORCE_INT_ON;
			/* fall through to exec'ing external program */
		}
		listsetvar(varlist.list, VEXPORT|VSTACK);
		shellexec(argv, path, cmdentry.u.index);
		/* NOTREACHED */
	} /* default */
	case CMDBUILTIN:
		cmdenviron = varlist.list;
		if (cmdenviron) {
			struct strlist *list = cmdenviron;
			int i = VNOSET;
			if (spclbltin > 0 || argc == 0) {
				i = 0;
				if (cmd_is_exec && argc > 1)
					i = VEXPORT;
			}
			listsetvar(list, i);
		}
		/* Tight loop with builtins only:
		 * "while kill -0 $child; do true; done"
		 * will never exit even if $child died, unless we do this
		 * to reap the zombie and make kill detect that it's gone: */
		dowait(DOWAIT_NONBLOCK, NULL);

		if (evalbltin(cmdentry.u.cmd, argc, argv)) {
			int exit_status;
			int i = exception_type;
			if (i == EXEXIT)
				goto raise;
			exit_status = 2;
			if (i == EXINT)
				exit_status = 128 + SIGINT;
			if (i == EXSIG)
				exit_status = 128 + pending_sig;
			exitstatus = exit_status;
			if (i == EXINT || spclbltin > 0) {
 raise:
				longjmp(exception_handler->loc, 1);
			}
			FORCE_INT_ON;
		}
		break;

	case CMDFUNCTION:
		listsetvar(varlist.list, 0);
		/* See above for the rationale */
		dowait(DOWAIT_NONBLOCK, NULL);
		if (evalfun(cmdentry.u.func, argc, argv, flags))
			goto raise;
		break;

	} /* switch */

 out:
	popredir(/*drop:*/ cmd_is_exec, /*restore:*/ cmd_is_exec);
	if (lastarg) {
		/* dsl: I think this is intended to be used to support
		 * '_' in 'vi' command mode during line editing...
		 * However I implemented that within libedit itself.
		 */
		setvar("_", lastarg, 0);
	}
	popstackmark(&smark);
}

static int
evalbltin(const struct builtincmd *cmd, int argc, char **argv)
{
	char *volatile savecmdname;
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
	int i;

	savecmdname = commandname;
	i = setjmp(jmploc.loc);
	if (i)
		goto cmddone;
	savehandler = exception_handler;
	exception_handler = &jmploc;
	commandname = argv[0];
	argptr = argv + 1;
	optptr = NULL;                  /* initialize nextopt */
	exitstatus = (*cmd->builtin)(argc, argv);
	flush_stdout_stderr();
 cmddone:
	exitstatus |= ferror(stdout);
	clearerr(stdout);
	commandname = savecmdname;
	exception_handler = savehandler;

	return i;
}

static int
goodname(const char *p)
{
	return endofname(p)[0] == '\0';
}


/*
 * Search for a command.  This is called before we fork so that the
 * location of the command will be available in the parent as well as
 * the child.  The check for "goodname" is an overly conservative
 * check that the name will not be subject to expansion.
 */
static void
prehash(union node *n)
{
	struct cmdentry entry;

	if (n->type == NCMD && n->ncmd.args && goodname(n->ncmd.args->narg.text))
		find_command(n->ncmd.args->narg.text, &entry, 0, pathval());
}


/* ============ Builtin commands
 *
 * Builtin commands whose functions are closely tied to evaluation
 * are implemented here.
 */

/*
 * Handle break and continue commands.  Break, continue, and return are
 * all handled by setting the evalskip flag.  The evaluation routines
 * above all check this flag, and if it is set they start skipping
 * commands rather than executing them.  The variable skipcount is
 * the number of loops to break/continue, or the number of function
 * levels to return.  (The latter is always 1.)  It should probably
 * be an error to break out of more loops than exist, but it isn't
 * in the standard shell so we don't make it one here.
 */
static int FAST_FUNC
breakcmd(int argc UNUSED_PARAM, char **argv)
{
	int n = argv[1] ? number(argv[1]) : 1;

	if (n <= 0)
		ash_msg_and_raise_error(msg_illnum, argv[1]);
	if (n > loopnest)
		n = loopnest;
	if (n > 0) {
		evalskip = (**argv == 'c') ? SKIPCONT : SKIPBREAK;
		skipcount = n;
	}
	return 0;
}


/* ============ input.c
 *
 * This implements the input routines used by the parser.
 */

enum {
	INPUT_PUSH_FILE = 1,
	INPUT_NOFILE_OK = 2,
};

static smallint checkkwd;
/* values of checkkwd variable */
#define CHKALIAS        0x1
#define CHKKWD          0x2
#define CHKNL           0x4

/*
 * Push a string back onto the input at this current parsefile level.
 * We handle aliases this way.
 */
#if !ENABLE_ASH_ALIAS
#define pushstring(s, ap) pushstring(s)
#endif
static void
pushstring(char *s, struct alias *ap)
{
	struct strpush *sp;
	int len;

	len = strlen(s);
	INT_OFF;
	if (g_parsefile->strpush) {
		sp = ckzalloc(sizeof(*sp));
		sp->prev = g_parsefile->strpush;
	} else {
		sp = &(g_parsefile->basestrpush);
	}
	g_parsefile->strpush = sp;
	sp->prev_string = g_parsefile->next_to_pgetc;
	sp->prev_left_in_line = g_parsefile->left_in_line;
#if ENABLE_ASH_ALIAS
	sp->ap = ap;
	if (ap) {
		ap->flag |= ALIASINUSE;
		sp->string = s;
	}
#endif
	g_parsefile->next_to_pgetc = s;
	g_parsefile->left_in_line = len;
	INT_ON;
}

static void
popstring(void)
{
	struct strpush *sp = g_parsefile->strpush;

	INT_OFF;
#if ENABLE_ASH_ALIAS
	if (sp->ap) {
		if (g_parsefile->next_to_pgetc[-1] == ' '
		 || g_parsefile->next_to_pgetc[-1] == '\t'
		) {
			checkkwd |= CHKALIAS;
		}
		if (sp->string != sp->ap->val) {
			free(sp->string);
		}
		sp->ap->flag &= ~ALIASINUSE;
		if (sp->ap->flag & ALIASDEAD) {
			unalias(sp->ap->name);
		}
	}
#endif
	g_parsefile->next_to_pgetc = sp->prev_string;
	g_parsefile->left_in_line = sp->prev_left_in_line;
	g_parsefile->strpush = sp->prev;
	if (sp != &(g_parsefile->basestrpush))
		free(sp);
	INT_ON;
}

//FIXME: BASH_COMPAT with "...&" does TWO pungetc():
//it peeks whether it is &>, and then pushes back both chars.
//This function needs to save last *next_to_pgetc to buf[0]
//to make two pungetc() reliable. Currently,
// pgetc (out of buf: does preadfd), pgetc, pungetc, pungetc won't work...
static int
preadfd(void)
{
	int nr;
	char *buf = g_parsefile->buf;

	g_parsefile->next_to_pgetc = buf;
#if ENABLE_FEATURE_EDITING
 retry:
	if (!iflag || g_parsefile->pf_fd != STDIN_FILENO)
		nr = nonblock_immune_read(g_parsefile->pf_fd, buf, IBUFSIZ - 1, /*loop_on_EINTR:*/ 1);
	else {
		int timeout = -1;
# if ENABLE_ASH_IDLE_TIMEOUT
		if (iflag) {
			const char *tmout_var = lookupvar("TMOUT");
			if (tmout_var) {
				timeout = atoi(tmout_var) * 1000;
				if (timeout <= 0)
					timeout = -1;
			}
		}
# endif
# if ENABLE_FEATURE_TAB_COMPLETION
		line_input_state->path_lookup = pathval();
# endif
		/* Unicode support should be activated even if LANG is set
		 * _during_ shell execution, not only if it was set when
		 * shell was started. Therefore, re-check LANG every time:
		 */
		reinit_unicode(lookupvar("LANG"));
		nr = read_line_input(line_input_state, cmdedit_prompt, buf, IBUFSIZ, timeout);
		if (nr == 0) {
			/* Ctrl+C pressed */
			if (trap[SIGINT]) {
				buf[0] = '\n';
				buf[1] = '\0';
				raise(SIGINT);
				return 1;
			}
			goto retry;
		}
		if (nr < 0) {
			if (errno == 0) {
				/* Ctrl+D pressed */
				nr = 0;
			}
# if ENABLE_ASH_IDLE_TIMEOUT
			else if (errno == EAGAIN && timeout > 0) {
				printf("\007timed out waiting for input: auto-logout\n");
				exitshell();
			}
# endif
		}
	}
#else
	nr = nonblock_immune_read(g_parsefile->pf_fd, buf, IBUFSIZ - 1, /*loop_on_EINTR:*/ 1);
#endif

#if 0 /* disabled: nonblock_immune_read() handles this problem */
	if (nr < 0) {
		if (parsefile->fd == 0 && errno == EWOULDBLOCK) {
			int flags = fcntl(0, F_GETFL);
			if (flags >= 0 && (flags & O_NONBLOCK)) {
				flags &= ~O_NONBLOCK;
				if (fcntl(0, F_SETFL, flags) >= 0) {
					out2str("sh: turning off NDELAY mode\n");
					goto retry;
				}
			}
		}
	}
#endif
	return nr;
}

/*
 * Refill the input buffer and return the next input character:
 *
 * 1) If a string was pushed back on the input, pop it;
 * 2) If an EOF was pushed back (g_parsefile->left_in_line < -BIGNUM)
 *    or we are reading from a string so we can't refill the buffer,
 *    return EOF.
 * 3) If there is more stuff in this buffer, use it else call read to fill it.
 * 4) Process input up to the next newline, deleting nul characters.
 */
//#define pgetc_debug(...) bb_error_msg(__VA_ARGS__)
#define pgetc_debug(...) ((void)0)
static int
preadbuffer(void)
{
	char *q;
	int more;

	while (g_parsefile->strpush) {
#if ENABLE_ASH_ALIAS
		if (g_parsefile->left_in_line == -1
		 && g_parsefile->strpush->ap
		 && g_parsefile->next_to_pgetc[-1] != ' '
		 && g_parsefile->next_to_pgetc[-1] != '\t'
		) {
			pgetc_debug("preadbuffer PEOA");
			return PEOA;
		}
#endif
		popstring();
		/* try "pgetc" now: */
		pgetc_debug("preadbuffer internal pgetc at %d:%p'%s'",
				g_parsefile->left_in_line,
				g_parsefile->next_to_pgetc,
				g_parsefile->next_to_pgetc);
		if (--g_parsefile->left_in_line >= 0)
			return (unsigned char)(*g_parsefile->next_to_pgetc++);
	}
	/* on both branches above g_parsefile->left_in_line < 0.
	 * "pgetc" needs refilling.
	 */

	/* -90 is our -BIGNUM. Below we use -99 to mark "EOF on read",
	 * pungetc() may increment it a few times.
	 * Assuming it won't increment it to less than -90.
	 */
	if (g_parsefile->left_in_line < -90 || g_parsefile->buf == NULL) {
		pgetc_debug("preadbuffer PEOF1");
		/* even in failure keep left_in_line and next_to_pgetc
		 * in lock step, for correct multi-layer pungetc.
		 * left_in_line was decremented before preadbuffer(),
		 * must inc next_to_pgetc: */
		g_parsefile->next_to_pgetc++;
		return PEOF;
	}

	more = g_parsefile->left_in_buffer;
	if (more <= 0) {
		flush_stdout_stderr();
 again:
		more = preadfd();
		if (more <= 0) {
			/* don't try reading again */
			g_parsefile->left_in_line = -99;
			pgetc_debug("preadbuffer PEOF2");
			g_parsefile->next_to_pgetc++;
			return PEOF;
		}
	}

	/* Find out where's the end of line.
	 * Set g_parsefile->left_in_line
	 * and g_parsefile->left_in_buffer acordingly.
	 * NUL chars are deleted.
	 */
	q = g_parsefile->next_to_pgetc;
	for (;;) {
		char c;

		more--;

		c = *q;
		if (c == '\0') {
			memmove(q, q + 1, more);
		} else {
			q++;
			if (c == '\n') {
				g_parsefile->left_in_line = q - g_parsefile->next_to_pgetc - 1;
				break;
			}
		}

		if (more <= 0) {
			g_parsefile->left_in_line = q - g_parsefile->next_to_pgetc - 1;
			if (g_parsefile->left_in_line < 0)
				goto again;
			break;
		}
	}
	g_parsefile->left_in_buffer = more;

	if (vflag) {
		char save = *q;
		*q = '\0';
		out2str(g_parsefile->next_to_pgetc);
		*q = save;
	}

	pgetc_debug("preadbuffer at %d:%p'%s'",
			g_parsefile->left_in_line,
			g_parsefile->next_to_pgetc,
			g_parsefile->next_to_pgetc);
	return (unsigned char)*g_parsefile->next_to_pgetc++;
}

#define pgetc_as_macro() \
	(--g_parsefile->left_in_line >= 0 \
	? (unsigned char)*g_parsefile->next_to_pgetc++ \
	: preadbuffer() \
	)

static int
pgetc(void)
{
	pgetc_debug("pgetc_fast at %d:%p'%s'",
			g_parsefile->left_in_line,
			g_parsefile->next_to_pgetc,
			g_parsefile->next_to_pgetc);
	return pgetc_as_macro();
}

#if ENABLE_ASH_OPTIMIZE_FOR_SIZE
# define pgetc_fast() pgetc()
#else
# define pgetc_fast() pgetc_as_macro()
#endif

#if ENABLE_ASH_ALIAS
static int
pgetc_without_PEOA(void)
{
	int c;
	do {
		pgetc_debug("pgetc_fast at %d:%p'%s'",
				g_parsefile->left_in_line,
				g_parsefile->next_to_pgetc,
				g_parsefile->next_to_pgetc);
		c = pgetc_fast();
	} while (c == PEOA);
	return c;
}
#else
# define pgetc_without_PEOA() pgetc()
#endif

/*
 * Read a line from the script.
 */
static char *
pfgets(char *line, int len)
{
	char *p = line;
	int nleft = len;
	int c;

	while (--nleft > 0) {
		c = pgetc_without_PEOA();
		if (c == PEOF) {
			if (p == line)
				return NULL;
			break;
		}
		*p++ = c;
		if (c == '\n')
			break;
	}
	*p = '\0';
	return line;
}

/*
 * Undo the last call to pgetc.  Only one character may be pushed back.
 * PEOF may be pushed back.
 */
static void
pungetc(void)
{
	g_parsefile->left_in_line++;
	g_parsefile->next_to_pgetc--;
	pgetc_debug("pushed back to %d:%p'%s'",
			g_parsefile->left_in_line,
			g_parsefile->next_to_pgetc,
			g_parsefile->next_to_pgetc);
}

/*
 * To handle the "." command, a stack of input files is used.  Pushfile
 * adds a new entry to the stack and popfile restores the previous level.
 */
static void
pushfile(void)
{
	struct parsefile *pf;

	pf = ckzalloc(sizeof(*pf));
	pf->prev = g_parsefile;
	pf->pf_fd = -1;
	/*pf->strpush = NULL; - ckzalloc did it */
	/*pf->basestrpush.prev = NULL;*/
	g_parsefile = pf;
}

static void
popfile(void)
{
	struct parsefile *pf = g_parsefile;

	INT_OFF;
	if (pf->pf_fd >= 0)
		close(pf->pf_fd);
	free(pf->buf);
	while (pf->strpush)
		popstring();
	g_parsefile = pf->prev;
	free(pf);
	INT_ON;
}

/*
 * Return to top level.
 */
static void
popallfiles(void)
{
	while (g_parsefile != &basepf)
		popfile();
}

/*
 * Close the file(s) that the shell is reading commands from.  Called
 * after a fork is done.
 */
static void
closescript(void)
{
	popallfiles();
	if (g_parsefile->pf_fd > 0) {
		close(g_parsefile->pf_fd);
		g_parsefile->pf_fd = 0;
	}
}

/*
 * Like setinputfile, but takes an open file descriptor.  Call this with
 * interrupts off.
 */
static void
setinputfd(int fd, int push)
{
	close_on_exec_on(fd);
	if (push) {
		pushfile();
		g_parsefile->buf = NULL;
	}
	g_parsefile->pf_fd = fd;
	if (g_parsefile->buf == NULL)
		g_parsefile->buf = ckmalloc(IBUFSIZ);
	g_parsefile->left_in_buffer = 0;
	g_parsefile->left_in_line = 0;
	g_parsefile->linno = 1;
}

/*
 * Set the input to take input from a file.  If push is set, push the
 * old input onto the stack first.
 */
static int
setinputfile(const char *fname, int flags)
{
	int fd;
	int fd2;

	INT_OFF;
	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		if (flags & INPUT_NOFILE_OK)
			goto out;
		ash_msg_and_raise_error("can't open '%s'", fname);
	}
	if (fd < 10) {
		fd2 = copyfd(fd, 10);
		close(fd);
		if (fd2 < 0)
			ash_msg_and_raise_error("out of file descriptors");
		fd = fd2;
	}
	setinputfd(fd, flags & INPUT_PUSH_FILE);
 out:
	INT_ON;
	return fd;
}

/*
 * Like setinputfile, but takes input from a string.
 */
static void
setinputstring(char *string)
{
	INT_OFF;
	pushfile();
	g_parsefile->next_to_pgetc = string;
	g_parsefile->left_in_line = strlen(string);
	g_parsefile->buf = NULL;
	g_parsefile->linno = 1;
	INT_ON;
}


/* ============ mail.c
 *
 * Routines to check for mail.
 */

#if ENABLE_ASH_MAIL

#define MAXMBOXES 10

/* times of mailboxes */
static time_t mailtime[MAXMBOXES];
/* Set if MAIL or MAILPATH is changed. */
static smallint mail_var_path_changed;

/*
 * Print appropriate message(s) if mail has arrived.
 * If mail_var_path_changed is set,
 * then the value of MAIL has mail_var_path_changed,
 * so we just update the values.
 */
static void
chkmail(void)
{
	const char *mpath;
	char *p;
	char *q;
	time_t *mtp;
	struct stackmark smark;
	struct stat statb;

	setstackmark(&smark);
	mpath = mpathset() ? mpathval() : mailval();
	for (mtp = mailtime; mtp < mailtime + MAXMBOXES; mtp++) {
		p = path_advance(&mpath, nullstr);
		if (p == NULL)
			break;
		if (*p == '\0')
			continue;
		for (q = p; *q; q++)
			continue;
#if DEBUG
		if (q[-1] != '/')
			abort();
#endif
		q[-1] = '\0';                   /* delete trailing '/' */
		if (stat(p, &statb) < 0) {
			*mtp = 0;
			continue;
		}
		if (!mail_var_path_changed && statb.st_mtime != *mtp) {
			fprintf(
				stderr, "%s\n",
				pathopt ? pathopt : "you have mail"
			);
		}
		*mtp = statb.st_mtime;
	}
	mail_var_path_changed = 0;
	popstackmark(&smark);
}

static void FAST_FUNC
changemail(const char *val UNUSED_PARAM)
{
	mail_var_path_changed = 1;
}

#endif /* ASH_MAIL */


/* ============ ??? */

/*
 * Set the shell parameters.
 */
static void
setparam(char **argv)
{
	char **newparam;
	char **ap;
	int nparam;

	for (nparam = 0; argv[nparam]; nparam++)
		continue;
	ap = newparam = ckmalloc((nparam + 1) * sizeof(*ap));
	while (*argv) {
		*ap++ = ckstrdup(*argv++);
	}
	*ap = NULL;
	freeparam(&shellparam);
	shellparam.malloced = 1;
	shellparam.nparam = nparam;
	shellparam.p = newparam;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
}

/*
 * Process shell options.  The global variable argptr contains a pointer
 * to the argument list; we advance it past the options.
 *
 * SUSv3 section 2.8.1 "Consequences of Shell Errors" says:
 * For a non-interactive shell, an error condition encountered
 * by a special built-in ... shall cause the shell to write a diagnostic message
 * to standard error and exit as shown in the following table:
 * Error                                           Special Built-In
 * ...
 * Utility syntax error (option or operand error)  Shall exit
 * ...
 * However, in bug 1142 (http://busybox.net/bugs/view.php?id=1142)
 * we see that bash does not do that (set "finishes" with error code 1 instead,
 * and shell continues), and people rely on this behavior!
 * Testcase:
 * set -o barfoo 2>/dev/null
 * echo $?
 *
 * Oh well. Let's mimic that.
 */
static int
plus_minus_o(char *name, int val)
{
	int i;

	if (name) {
		for (i = 0; i < NOPTS; i++) {
			if (strcmp(name, optnames(i)) == 0) {
				optlist[i] = val;
				return 0;
			}
		}
		ash_msg("illegal option %co %s", val ? '-' : '+', name);
		return 1;
	}
	for (i = 0; i < NOPTS; i++) {
		if (val) {
			out1fmt("%-16s%s\n", optnames(i), optlist[i] ? "on" : "off");
		} else {
			out1fmt("set %co %s\n", optlist[i] ? '-' : '+', optnames(i));
		}
	}
	return 0;
}
static void
setoption(int flag, int val)
{
	int i;

	for (i = 0; i < NOPTS; i++) {
		if (optletters(i) == flag) {
			optlist[i] = val;
			return;
		}
	}
	ash_msg_and_raise_error("illegal option %c%c", val ? '-' : '+', flag);
	/* NOTREACHED */
}
static int
options(int cmdline)
{
	char *p;
	int val;
	int c;

	if (cmdline)
		minusc = NULL;
	while ((p = *argptr) != NULL) {
		c = *p++;
		if (c != '-' && c != '+')
			break;
		argptr++;
		val = 0; /* val = 0 if c == '+' */
		if (c == '-') {
			val = 1;
			if (p[0] == '\0' || LONE_DASH(p)) {
				if (!cmdline) {
					/* "-" means turn off -x and -v */
					if (p[0] == '\0')
						xflag = vflag = 0;
					/* "--" means reset params */
					else if (*argptr == NULL)
						setparam(argptr);
				}
				break;    /* "-" or "--" terminates options */
			}
		}
		/* first char was + or - */
		while ((c = *p++) != '\0') {
			/* bash 3.2 indeed handles -c CMD and +c CMD the same */
			if (c == 'c' && cmdline) {
				minusc = p;     /* command is after shell args */
			} else if (c == 'o') {
				if (plus_minus_o(*argptr, val)) {
					/* it already printed err message */
					return 1; /* error */
				}
				if (*argptr)
					argptr++;
			} else if (cmdline && (c == 'l')) { /* -l or +l == --login */
				isloginsh = 1;
			/* bash does not accept +-login, we also won't */
			} else if (cmdline && val && (c == '-')) { /* long options */
				if (strcmp(p, "login") == 0)
					isloginsh = 1;
				break;
			} else {
				setoption(c, val);
			}
		}
	}
	return 0;
}

/*
 * The shift builtin command.
 */
static int FAST_FUNC
shiftcmd(int argc UNUSED_PARAM, char **argv)
{
	int n;
	char **ap1, **ap2;

	n = 1;
	if (argv[1])
		n = number(argv[1]);
	if (n > shellparam.nparam)
		n = 0; /* bash compat, was = shellparam.nparam; */
	INT_OFF;
	shellparam.nparam -= n;
	for (ap1 = shellparam.p; --n >= 0; ap1++) {
		if (shellparam.malloced)
			free(*ap1);
	}
	ap2 = shellparam.p;
	while ((*ap2++ = *ap1++) != NULL)
		continue;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	INT_ON;
	return 0;
}

/*
 * POSIX requires that 'set' (but not export or readonly) output the
 * variables in lexicographic order - by the locale's collating order (sigh).
 * Maybe we could keep them in an ordered balanced binary tree
 * instead of hashed lists.
 * For now just roll 'em through qsort for printing...
 */
static int
showvars(const char *sep_prefix, int on, int off)
{
	const char *sep;
	char **ep, **epend;

	ep = listvars(on, off, &epend);
	qsort(ep, epend - ep, sizeof(char *), vpcmp);

	sep = *sep_prefix ? " " : sep_prefix;

	for (; ep < epend; ep++) {
		const char *p;
		const char *q;

		p = strchrnul(*ep, '=');
		q = nullstr;
		if (*p)
			q = single_quote(++p);
		out1fmt("%s%s%.*s%s\n", sep_prefix, sep, (int)(p - *ep), *ep, q);
	}
	return 0;
}

/*
 * The set command builtin.
 */
static int FAST_FUNC
setcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int retval;

	if (!argv[1])
		return showvars(nullstr, 0, VUNSET);

	INT_OFF;
	retval = options(/*cmdline:*/ 0);
	if (retval == 0) { /* if no parse error... */
		optschanged();
		if (*argptr != NULL) {
			setparam(argptr);
		}
	}
	INT_ON;
	return retval;
}

#if ENABLE_ASH_RANDOM_SUPPORT
static void FAST_FUNC
change_random(const char *value)
{
	uint32_t t;

	if (value == NULL) {
		/* "get", generate */
		t = next_random(&random_gen);
		/* set without recursion */
		setvar(vrandom.var_text, utoa(t), VNOFUNC);
		vrandom.flags &= ~VNOFUNC;
	} else {
		/* set/reset */
		t = strtoul(value, NULL, 10);
		INIT_RANDOM_T(&random_gen, (t ? t : 1), t);
	}
}
#endif

#if ENABLE_ASH_GETOPTS
static int
getopts(char *optstr, char *optvar, char **optfirst, int *param_optind, int *optoff)
{
	char *p, *q;
	char c = '?';
	int done = 0;
	int err = 0;
	char s[12];
	char **optnext;

	if (*param_optind < 1)
		return 1;
	optnext = optfirst + *param_optind - 1;

	if (*param_optind <= 1 || *optoff < 0 || (int)strlen(optnext[-1]) < *optoff)
		p = NULL;
	else
		p = optnext[-1] + *optoff;
	if (p == NULL || *p == '\0') {
		/* Current word is done, advance */
		p = *optnext;
		if (p == NULL || *p != '-' || *++p == '\0') {
 atend:
			p = NULL;
			done = 1;
			goto out;
		}
		optnext++;
		if (LONE_DASH(p))        /* check for "--" */
			goto atend;
	}

	c = *p++;
	for (q = optstr; *q != c;) {
		if (*q == '\0') {
			if (optstr[0] == ':') {
				s[0] = c;
				s[1] = '\0';
				err |= setvarsafe("OPTARG", s, 0);
			} else {
				fprintf(stderr, "Illegal option -%c\n", c);
				unsetvar("OPTARG");
			}
			c = '?';
			goto out;
		}
		if (*++q == ':')
			q++;
	}

	if (*++q == ':') {
		if (*p == '\0' && (p = *optnext) == NULL) {
			if (optstr[0] == ':') {
				s[0] = c;
				s[1] = '\0';
				err |= setvarsafe("OPTARG", s, 0);
				c = ':';
			} else {
				fprintf(stderr, "No arg for -%c option\n", c);
				unsetvar("OPTARG");
				c = '?';
			}
			goto out;
		}

		if (p == *optnext)
			optnext++;
		err |= setvarsafe("OPTARG", p, 0);
		p = NULL;
	} else
		err |= setvarsafe("OPTARG", nullstr, 0);
 out:
	*optoff = p ? p - *(optnext - 1) : -1;
	*param_optind = optnext - optfirst + 1;
	fmtstr(s, sizeof(s), "%d", *param_optind);
	err |= setvarsafe("OPTIND", s, VNOFUNC);
	s[0] = c;
	s[1] = '\0';
	err |= setvarsafe(optvar, s, 0);
	if (err) {
		*param_optind = 1;
		*optoff = -1;
		flush_stdout_stderr();
		raise_exception(EXERROR);
	}
	return done;
}

/*
 * The getopts builtin.  Shellparam.optnext points to the next argument
 * to be processed.  Shellparam.optptr points to the next character to
 * be processed in the current argument.  If shellparam.optnext is NULL,
 * then it's the first time getopts has been called.
 */
static int FAST_FUNC
getoptscmd(int argc, char **argv)
{
	char **optbase;

	if (argc < 3)
		ash_msg_and_raise_error("usage: getopts optstring var [arg]");
	if (argc == 3) {
		optbase = shellparam.p;
		if (shellparam.optind > shellparam.nparam + 1) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	} else {
		optbase = &argv[3];
		if (shellparam.optind > argc - 2) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	}

	return getopts(argv[1], argv[2], optbase, &shellparam.optind,
			&shellparam.optoff);
}
#endif /* ASH_GETOPTS */


/* ============ Shell parser */

struct heredoc {
	struct heredoc *next;   /* next here document in list */
	union node *here;       /* redirection node */
	char *eofmark;          /* string indicating end of input */
	smallint striptabs;     /* if set, strip leading tabs */
};

static smallint tokpushback;           /* last token pushed back */
static smallint parsebackquote;        /* nonzero if we are inside backquotes */
static smallint quoteflag;             /* set if (part of) last token was quoted */
static token_id_t lasttoken;           /* last token read (integer id Txxx) */
static struct heredoc *heredoclist;    /* list of here documents to read */
static char *wordtext;                 /* text of last word returned by readtoken */
static struct nodelist *backquotelist;
static union node *redirnode;
static struct heredoc *heredoc;

static const char *
tokname(char *buf, int tok)
{
	if (tok < TSEMI)
		return tokname_array[tok] + 1;
	sprintf(buf, "\"%s\"", tokname_array[tok] + 1);
	return buf;
}

/* raise_error_unexpected_syntax:
 * Called when an unexpected token is read during the parse.  The argument
 * is the token that is expected, or -1 if more than one type of token can
 * occur at this point.
 */
static void raise_error_unexpected_syntax(int) NORETURN;
static void
raise_error_unexpected_syntax(int token)
{
	char msg[64];
	char buf[16];
	int l;

	l = sprintf(msg, "unexpected %s", tokname(buf, lasttoken));
	if (token >= 0)
		sprintf(msg + l, " (expecting %s)", tokname(buf, token));
	raise_error_syntax(msg);
	/* NOTREACHED */
}

#define EOFMARKLEN 79

/* parsing is heavily cross-recursive, need these forward decls */
static union node *andor(void);
static union node *pipeline(void);
static union node *parse_command(void);
static void parseheredoc(void);
static char peektoken(void);
static int readtoken(void);

static union node *
list(int nlflag)
{
	union node *n1, *n2, *n3;
	int tok;

	checkkwd = CHKNL | CHKKWD | CHKALIAS;
	if (nlflag == 2 && peektoken())
		return NULL;
	n1 = NULL;
	for (;;) {
		n2 = andor();
		tok = readtoken();
		if (tok == TBACKGND) {
			if (n2->type == NPIPE) {
				n2->npipe.pipe_backgnd = 1;
			} else {
				if (n2->type != NREDIR) {
					n3 = stzalloc(sizeof(struct nredir));
					n3->nredir.n = n2;
					/*n3->nredir.redirect = NULL; - stzalloc did it */
					n2 = n3;
				}
				n2->type = NBACKGND;
			}
		}
		if (n1 == NULL) {
			n1 = n2;
		} else {
			n3 = stzalloc(sizeof(struct nbinary));
			n3->type = NSEMI;
			n3->nbinary.ch1 = n1;
			n3->nbinary.ch2 = n2;
			n1 = n3;
		}
		switch (tok) {
		case TBACKGND:
		case TSEMI:
			tok = readtoken();
			/* fall through */
		case TNL:
			if (tok == TNL) {
				parseheredoc();
				if (nlflag == 1)
					return n1;
			} else {
				tokpushback = 1;
			}
			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			if (peektoken())
				return n1;
			break;
		case TEOF:
			if (heredoclist)
				parseheredoc();
			else
				pungetc();              /* push back EOF on input */
			return n1;
		default:
			if (nlflag == 1)
				raise_error_unexpected_syntax(-1);
			tokpushback = 1;
			return n1;
		}
	}
}

static union node *
andor(void)
{
	union node *n1, *n2, *n3;
	int t;

	n1 = pipeline();
	for (;;) {
		t = readtoken();
		if (t == TAND) {
			t = NAND;
		} else if (t == TOR) {
			t = NOR;
		} else {
			tokpushback = 1;
			return n1;
		}
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		n2 = pipeline();
		n3 = stzalloc(sizeof(struct nbinary));
		n3->type = t;
		n3->nbinary.ch1 = n1;
		n3->nbinary.ch2 = n2;
		n1 = n3;
	}
}

static union node *
pipeline(void)
{
	union node *n1, *n2, *pipenode;
	struct nodelist *lp, *prev;
	int negate;

	negate = 0;
	TRACE(("pipeline: entered\n"));
	if (readtoken() == TNOT) {
		negate = !negate;
		checkkwd = CHKKWD | CHKALIAS;
	} else
		tokpushback = 1;
	n1 = parse_command();
	if (readtoken() == TPIPE) {
		pipenode = stzalloc(sizeof(struct npipe));
		pipenode->type = NPIPE;
		/*pipenode->npipe.pipe_backgnd = 0; - stzalloc did it */
		lp = stzalloc(sizeof(struct nodelist));
		pipenode->npipe.cmdlist = lp;
		lp->n = n1;
		do {
			prev = lp;
			lp = stzalloc(sizeof(struct nodelist));
			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			lp->n = parse_command();
			prev->next = lp;
		} while (readtoken() == TPIPE);
		lp->next = NULL;
		n1 = pipenode;
	}
	tokpushback = 1;
	if (negate) {
		n2 = stzalloc(sizeof(struct nnot));
		n2->type = NNOT;
		n2->nnot.com = n1;
		return n2;
	}
	return n1;
}

static union node *
makename(void)
{
	union node *n;

	n = stzalloc(sizeof(struct narg));
	n->type = NARG;
	/*n->narg.next = NULL; - stzalloc did it */
	n->narg.text = wordtext;
	n->narg.backquote = backquotelist;
	return n;
}

static void
fixredir(union node *n, const char *text, int err)
{
	int fd;

	TRACE(("Fix redir %s %d\n", text, err));
	if (!err)
		n->ndup.vname = NULL;

	fd = bb_strtou(text, NULL, 10);
	if (!errno && fd >= 0)
		n->ndup.dupfd = fd;
	else if (LONE_DASH(text))
		n->ndup.dupfd = -1;
	else {
		if (err)
			raise_error_syntax("bad fd number");
		n->ndup.vname = makename();
	}
}

/*
 * Returns true if the text contains nothing to expand (no dollar signs
 * or backquotes).
 */
static int
noexpand(const char *text)
{
	unsigned char c;

	while ((c = *text++) != '\0') {
		if (c == CTLQUOTEMARK)
			continue;
		if (c == CTLESC)
			text++;
		else if (SIT(c, BASESYNTAX) == CCTL)
			return 0;
	}
	return 1;
}

static void
parsefname(void)
{
	union node *n = redirnode;

	if (readtoken() != TWORD)
		raise_error_unexpected_syntax(-1);
	if (n->type == NHERE) {
		struct heredoc *here = heredoc;
		struct heredoc *p;
		int i;

		if (quoteflag == 0)
			n->type = NXHERE;
		TRACE(("Here document %d\n", n->type));
		if (!noexpand(wordtext) || (i = strlen(wordtext)) == 0 || i > EOFMARKLEN)
			raise_error_syntax("illegal eof marker for << redirection");
		rmescapes(wordtext, 0);
		here->eofmark = wordtext;
		here->next = NULL;
		if (heredoclist == NULL)
			heredoclist = here;
		else {
			for (p = heredoclist; p->next; p = p->next)
				continue;
			p->next = here;
		}
	} else if (n->type == NTOFD || n->type == NFROMFD) {
		fixredir(n, wordtext, 0);
	} else {
		n->nfile.fname = makename();
	}
}

static union node *
simplecmd(void)
{
	union node *args, **app;
	union node *n = NULL;
	union node *vars, **vpp;
	union node **rpp, *redir;
	int savecheckkwd;
#if ENABLE_ASH_BASH_COMPAT
	smallint double_brackets_flag = 0;
#endif

	args = NULL;
	app = &args;
	vars = NULL;
	vpp = &vars;
	redir = NULL;
	rpp = &redir;

	savecheckkwd = CHKALIAS;
	for (;;) {
		int t;
		checkkwd = savecheckkwd;
		t = readtoken();
		switch (t) {
#if ENABLE_ASH_BASH_COMPAT
		case TAND: /* "&&" */
		case TOR: /* "||" */
			if (!double_brackets_flag) {
				tokpushback = 1;
				goto out;
			}
			wordtext = (char *) (t == TAND ? "-a" : "-o");
#endif
		case TWORD:
			n = stzalloc(sizeof(struct narg));
			n->type = NARG;
			/*n->narg.next = NULL; - stzalloc did it */
			n->narg.text = wordtext;
#if ENABLE_ASH_BASH_COMPAT
			if (strcmp("[[", wordtext) == 0)
				double_brackets_flag = 1;
			else if (strcmp("]]", wordtext) == 0)
				double_brackets_flag = 0;
#endif
			n->narg.backquote = backquotelist;
			if (savecheckkwd && isassignment(wordtext)) {
				*vpp = n;
				vpp = &n->narg.next;
			} else {
				*app = n;
				app = &n->narg.next;
				savecheckkwd = 0;
			}
			break;
		case TREDIR:
			*rpp = n = redirnode;
			rpp = &n->nfile.next;
			parsefname();   /* read name of redirection file */
			break;
		case TLP:
			if (args && app == &args->narg.next
			 && !vars && !redir
			) {
				struct builtincmd *bcmd;
				const char *name;

				/* We have a function */
				if (readtoken() != TRP)
					raise_error_unexpected_syntax(TRP);
				name = n->narg.text;
				if (!goodname(name)
				 || ((bcmd = find_builtin(name)) && IS_BUILTIN_SPECIAL(bcmd))
				) {
					raise_error_syntax("bad function name");
				}
				n->type = NDEFUN;
				checkkwd = CHKNL | CHKKWD | CHKALIAS;
				n->narg.next = parse_command();
				return n;
			}
			/* fall through */
		default:
			tokpushback = 1;
			goto out;
		}
	}
 out:
	*app = NULL;
	*vpp = NULL;
	*rpp = NULL;
	n = stzalloc(sizeof(struct ncmd));
	n->type = NCMD;
	n->ncmd.args = args;
	n->ncmd.assign = vars;
	n->ncmd.redirect = redir;
	return n;
}

static union node *
parse_command(void)
{
	union node *n1, *n2;
	union node *ap, **app;
	union node *cp, **cpp;
	union node *redir, **rpp;
	union node **rpp2;
	int t;

	redir = NULL;
	rpp2 = &redir;

	switch (readtoken()) {
	default:
		raise_error_unexpected_syntax(-1);
		/* NOTREACHED */
	case TIF:
		n1 = stzalloc(sizeof(struct nif));
		n1->type = NIF;
		n1->nif.test = list(0);
		if (readtoken() != TTHEN)
			raise_error_unexpected_syntax(TTHEN);
		n1->nif.ifpart = list(0);
		n2 = n1;
		while (readtoken() == TELIF) {
			n2->nif.elsepart = stzalloc(sizeof(struct nif));
			n2 = n2->nif.elsepart;
			n2->type = NIF;
			n2->nif.test = list(0);
			if (readtoken() != TTHEN)
				raise_error_unexpected_syntax(TTHEN);
			n2->nif.ifpart = list(0);
		}
		if (lasttoken == TELSE)
			n2->nif.elsepart = list(0);
		else {
			n2->nif.elsepart = NULL;
			tokpushback = 1;
		}
		t = TFI;
		break;
	case TWHILE:
	case TUNTIL: {
		int got;
		n1 = stzalloc(sizeof(struct nbinary));
		n1->type = (lasttoken == TWHILE) ? NWHILE : NUNTIL;
		n1->nbinary.ch1 = list(0);
		got = readtoken();
		if (got != TDO) {
			TRACE(("expecting DO got '%s' %s\n", tokname_array[got] + 1,
					got == TWORD ? wordtext : ""));
			raise_error_unexpected_syntax(TDO);
		}
		n1->nbinary.ch2 = list(0);
		t = TDONE;
		break;
	}
	case TFOR:
		if (readtoken() != TWORD || quoteflag || !goodname(wordtext))
			raise_error_syntax("bad for loop variable");
		n1 = stzalloc(sizeof(struct nfor));
		n1->type = NFOR;
		n1->nfor.var = wordtext;
		checkkwd = CHKKWD | CHKALIAS;
		if (readtoken() == TIN) {
			app = &ap;
			while (readtoken() == TWORD) {
				n2 = stzalloc(sizeof(struct narg));
				n2->type = NARG;
				/*n2->narg.next = NULL; - stzalloc did it */
				n2->narg.text = wordtext;
				n2->narg.backquote = backquotelist;
				*app = n2;
				app = &n2->narg.next;
			}
			*app = NULL;
			n1->nfor.args = ap;
			if (lasttoken != TNL && lasttoken != TSEMI)
				raise_error_unexpected_syntax(-1);
		} else {
			n2 = stzalloc(sizeof(struct narg));
			n2->type = NARG;
			/*n2->narg.next = NULL; - stzalloc did it */
			n2->narg.text = (char *)dolatstr;
			/*n2->narg.backquote = NULL;*/
			n1->nfor.args = n2;
			/*
			 * Newline or semicolon here is optional (but note
			 * that the original Bourne shell only allowed NL).
			 */
			if (lasttoken != TNL && lasttoken != TSEMI)
				tokpushback = 1;
		}
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		if (readtoken() != TDO)
			raise_error_unexpected_syntax(TDO);
		n1->nfor.body = list(0);
		t = TDONE;
		break;
	case TCASE:
		n1 = stzalloc(sizeof(struct ncase));
		n1->type = NCASE;
		if (readtoken() != TWORD)
			raise_error_unexpected_syntax(TWORD);
		n1->ncase.expr = n2 = stzalloc(sizeof(struct narg));
		n2->type = NARG;
		/*n2->narg.next = NULL; - stzalloc did it */
		n2->narg.text = wordtext;
		n2->narg.backquote = backquotelist;
		do {
			checkkwd = CHKKWD | CHKALIAS;
		} while (readtoken() == TNL);
		if (lasttoken != TIN)
			raise_error_unexpected_syntax(TIN);
		cpp = &n1->ncase.cases;
 next_case:
		checkkwd = CHKNL | CHKKWD;
		t = readtoken();
		while (t != TESAC) {
			if (lasttoken == TLP)
				readtoken();
			*cpp = cp = stzalloc(sizeof(struct nclist));
			cp->type = NCLIST;
			app = &cp->nclist.pattern;
			for (;;) {
				*app = ap = stzalloc(sizeof(struct narg));
				ap->type = NARG;
				/*ap->narg.next = NULL; - stzalloc did it */
				ap->narg.text = wordtext;
				ap->narg.backquote = backquotelist;
				if (readtoken() != TPIPE)
					break;
				app = &ap->narg.next;
				readtoken();
			}
			//ap->narg.next = NULL;
			if (lasttoken != TRP)
				raise_error_unexpected_syntax(TRP);
			cp->nclist.body = list(2);

			cpp = &cp->nclist.next;

			checkkwd = CHKNL | CHKKWD;
			t = readtoken();
			if (t != TESAC) {
				if (t != TENDCASE)
					raise_error_unexpected_syntax(TENDCASE);
				goto next_case;
			}
		}
		*cpp = NULL;
		goto redir;
	case TLP:
		n1 = stzalloc(sizeof(struct nredir));
		n1->type = NSUBSHELL;
		n1->nredir.n = list(0);
		/*n1->nredir.redirect = NULL; - stzalloc did it */
		t = TRP;
		break;
	case TBEGIN:
		n1 = list(0);
		t = TEND;
		break;
	case TWORD:
	case TREDIR:
		tokpushback = 1;
		return simplecmd();
	}

	if (readtoken() != t)
		raise_error_unexpected_syntax(t);

 redir:
	/* Now check for redirection which may follow command */
	checkkwd = CHKKWD | CHKALIAS;
	rpp = rpp2;
	while (readtoken() == TREDIR) {
		*rpp = n2 = redirnode;
		rpp = &n2->nfile.next;
		parsefname();
	}
	tokpushback = 1;
	*rpp = NULL;
	if (redir) {
		if (n1->type != NSUBSHELL) {
			n2 = stzalloc(sizeof(struct nredir));
			n2->type = NREDIR;
			n2->nredir.n = n1;
			n1 = n2;
		}
		n1->nredir.redirect = redir;
	}
	return n1;
}

#if ENABLE_ASH_BASH_COMPAT
static int decode_dollar_squote(void)
{
	static const char C_escapes[] ALIGN1 = "nrbtfav""x\\01234567";
	int c, cnt;
	char *p;
	char buf[4];

	c = pgetc();
	p = strchr(C_escapes, c);
	if (p) {
		buf[0] = c;
		p = buf;
		cnt = 3;
		if ((unsigned char)(c - '0') <= 7) { /* \ooo */
			do {
				c = pgetc();
				*++p = c;
			} while ((unsigned char)(c - '0') <= 7 && --cnt);
			pungetc();
		} else if (c == 'x') { /* \xHH */
			do {
				c = pgetc();
				*++p = c;
			} while (isxdigit(c) && --cnt);
			pungetc();
			if (cnt == 3) { /* \x but next char is "bad" */
				c = 'x';
				goto unrecognized;
			}
		} else { /* simple seq like \\ or \t */
			p++;
		}
		*p = '\0';
		p = buf;
		c = bb_process_escape_sequence((void*)&p);
	} else { /* unrecognized "\z": print both chars unless ' or " */
		if (c != '\'' && c != '"') {
 unrecognized:
			c |= 0x100; /* "please encode \, then me" */
		}
	}
	return c;
}
#endif

/*
 * If eofmark is NULL, read a word or a redirection symbol.  If eofmark
 * is not NULL, read a here document.  In the latter case, eofmark is the
 * word which marks the end of the document and striptabs is true if
 * leading tabs should be stripped from the document.  The argument c
 * is the first character of the input token or document.
 *
 * Because C does not have internal subroutines, I have simulated them
 * using goto's to implement the subroutine linkage.  The following macros
 * will run code that appears at the end of readtoken1.
 */
#define CHECKEND()      {goto checkend; checkend_return:;}
#define PARSEREDIR()    {goto parseredir; parseredir_return:;}
#define PARSESUB()      {goto parsesub; parsesub_return:;}
#define PARSEBACKQOLD() {oldstyle = 1; goto parsebackq; parsebackq_oldreturn:;}
#define PARSEBACKQNEW() {oldstyle = 0; goto parsebackq; parsebackq_newreturn:;}
#define PARSEARITH()    {goto parsearith; parsearith_return:;}
static int
readtoken1(int c, int syntax, char *eofmark, int striptabs)
{
	/* NB: syntax parameter fits into smallint */
	/* c parameter is an unsigned char or PEOF or PEOA */
	char *out;
	int len;
	char line[EOFMARKLEN + 1];
	struct nodelist *bqlist;
	smallint quotef;
	smallint dblquote;
	smallint oldstyle;
	smallint prevsyntax; /* syntax before arithmetic */
#if ENABLE_ASH_EXPAND_PRMT
	smallint pssyntax;   /* we are expanding a prompt string */
#endif
	int varnest;         /* levels of variables expansion */
	int arinest;         /* levels of arithmetic expansion */
	int parenlevel;      /* levels of parens in arithmetic */
	int dqvarnest;       /* levels of variables expansion within double quotes */

	IF_ASH_BASH_COMPAT(smallint bash_dollar_squote = 0;)

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &out;
	(void) &quotef;
	(void) &dblquote;
	(void) &varnest;
	(void) &arinest;
	(void) &parenlevel;
	(void) &dqvarnest;
	(void) &oldstyle;
	(void) &prevsyntax;
	(void) &syntax;
#endif
	startlinno = g_parsefile->linno;
	bqlist = NULL;
	quotef = 0;
	prevsyntax = 0;
#if ENABLE_ASH_EXPAND_PRMT
	pssyntax = (syntax == PSSYNTAX);
	if (pssyntax)
		syntax = DQSYNTAX;
#endif
	dblquote = (syntax == DQSYNTAX);
	varnest = 0;
	arinest = 0;
	parenlevel = 0;
	dqvarnest = 0;

	STARTSTACKSTR(out);
 loop:
	/* For each line, until end of word */
	CHECKEND();     /* set c to PEOF if at end of here document */
	for (;;) {      /* until end of line or end of word */
		CHECKSTRSPACE(4, out);  /* permit 4 calls to USTPUTC */
		switch (SIT(c, syntax)) {
		case CNL:       /* '\n' */
			if (syntax == BASESYNTAX)
				goto endword;   /* exit outer loop */
			USTPUTC(c, out);
			g_parsefile->linno++;
			setprompt_if(doprompt, 2);
			c = pgetc();
			goto loop;              /* continue outer loop */
		case CWORD:
			USTPUTC(c, out);
			break;
		case CCTL:
			if (eofmark == NULL || dblquote)
				USTPUTC(CTLESC, out);
#if ENABLE_ASH_BASH_COMPAT
			if (c == '\\' && bash_dollar_squote) {
				c = decode_dollar_squote();
				if (c & 0x100) {
					USTPUTC('\\', out);
					c = (unsigned char)c;
				}
			}
#endif
			USTPUTC(c, out);
			break;
		case CBACK:     /* backslash */
			c = pgetc_without_PEOA();
			if (c == PEOF) {
				USTPUTC(CTLESC, out);
				USTPUTC('\\', out);
				pungetc();
			} else if (c == '\n') {
				setprompt_if(doprompt, 2);
			} else {
#if ENABLE_ASH_EXPAND_PRMT
				if (c == '$' && pssyntax) {
					USTPUTC(CTLESC, out);
					USTPUTC('\\', out);
				}
#endif
				/* Backslash is retained if we are in "str" and next char isn't special */
				if (dblquote
				 && c != '\\'
				 && c != '`'
				 && c != '$'
				 && (c != '"' || eofmark != NULL)
				) {
					USTPUTC(CTLESC, out);
					USTPUTC('\\', out);
				}
				if (SIT(c, SQSYNTAX) == CCTL)
					USTPUTC(CTLESC, out);
				USTPUTC(c, out);
				quotef = 1;
			}
			break;
		case CSQUOTE:
			syntax = SQSYNTAX;
 quotemark:
			if (eofmark == NULL) {
				USTPUTC(CTLQUOTEMARK, out);
			}
			break;
		case CDQUOTE:
			syntax = DQSYNTAX;
			dblquote = 1;
			goto quotemark;
		case CENDQUOTE:
			IF_ASH_BASH_COMPAT(bash_dollar_squote = 0;)
			if (eofmark != NULL && arinest == 0
			 && varnest == 0
			) {
				USTPUTC(c, out);
			} else {
				if (dqvarnest == 0) {
					syntax = BASESYNTAX;
					dblquote = 0;
				}
				quotef = 1;
				goto quotemark;
			}
			break;
		case CVAR:      /* '$' */
			PARSESUB();             /* parse substitution */
			break;
		case CENDVAR:   /* '}' */
			if (varnest > 0) {
				varnest--;
				if (dqvarnest > 0) {
					dqvarnest--;
				}
				c = CTLENDVAR;
			}
			USTPUTC(c, out);
			break;
#if ENABLE_SH_MATH_SUPPORT
		case CLP:       /* '(' in arithmetic */
			parenlevel++;
			USTPUTC(c, out);
			break;
		case CRP:       /* ')' in arithmetic */
			if (parenlevel > 0) {
				parenlevel--;
			} else {
				if (pgetc() == ')') {
					if (--arinest == 0) {
						syntax = prevsyntax;
						dblquote = (syntax == DQSYNTAX);
						c = CTLENDARI;
					}
				} else {
					/*
					 * unbalanced parens
					 * (don't 2nd guess - no error)
					 */
					pungetc();
				}
			}
			USTPUTC(c, out);
			break;
#endif
		case CBQUOTE:   /* '`' */
			PARSEBACKQOLD();
			break;
		case CENDFILE:
			goto endword;           /* exit outer loop */
		case CIGN:
			break;
		default:
			if (varnest == 0) {
#if ENABLE_ASH_BASH_COMPAT
				if (c == '&') {
					if (pgetc() == '>')
						c = 0x100 + '>'; /* flag &> */
					pungetc();
				}
#endif
				goto endword;   /* exit outer loop */
			}
			IF_ASH_ALIAS(if (c != PEOA))
				USTPUTC(c, out);
		}
		c = pgetc_fast();
	} /* for (;;) */
 endword:

#if ENABLE_SH_MATH_SUPPORT
	if (syntax == ARISYNTAX)
		raise_error_syntax("missing '))'");
#endif
	if (syntax != BASESYNTAX && !parsebackquote && eofmark == NULL)
		raise_error_syntax("unterminated quoted string");
	if (varnest != 0) {
		startlinno = g_parsefile->linno;
		/* { */
		raise_error_syntax("missing '}'");
	}
	USTPUTC('\0', out);
	len = out - (char *)stackblock();
	out = stackblock();
	if (eofmark == NULL) {
		if ((c == '>' || c == '<' IF_ASH_BASH_COMPAT( || c == 0x100 + '>'))
		 && quotef == 0
		) {
			if (isdigit_str9(out)) {
				PARSEREDIR(); /* passed as params: out, c */
				lasttoken = TREDIR;
				return lasttoken;
			}
			/* else: non-number X seen, interpret it
			 * as "NNNX>file" = "NNNX >file" */
		}
		pungetc();
	}
	quoteflag = quotef;
	backquotelist = bqlist;
	grabstackblock(len);
	wordtext = out;
	lasttoken = TWORD;
	return lasttoken;
/* end of readtoken routine */

/*
 * Check to see whether we are at the end of the here document.  When this
 * is called, c is set to the first character of the next input line.  If
 * we are at the end of the here document, this routine sets the c to PEOF.
 */
checkend: {
	if (eofmark) {
#if ENABLE_ASH_ALIAS
		if (c == PEOA)
			c = pgetc_without_PEOA();
#endif
		if (striptabs) {
			while (c == '\t') {
				c = pgetc_without_PEOA();
			}
		}
		if (c == *eofmark) {
			if (pfgets(line, sizeof(line)) != NULL) {
				char *p, *q;

				p = line;
				for (q = eofmark + 1; *q && *p == *q; p++, q++)
					continue;
				if (*p == '\n' && *q == '\0') {
					c = PEOF;
					g_parsefile->linno++;
					needprompt = doprompt;
				} else {
					pushstring(line, NULL);
				}
			}
		}
	}
	goto checkend_return;
}

/*
 * Parse a redirection operator.  The variable "out" points to a string
 * specifying the fd to be redirected.  The variable "c" contains the
 * first character of the redirection operator.
 */
parseredir: {
	/* out is already checked to be a valid number or "" */
	int fd = (*out == '\0' ? -1 : atoi(out));
	union node *np;

	np = stzalloc(sizeof(struct nfile));
	if (c == '>') {
		np->nfile.fd = 1;
		c = pgetc();
		if (c == '>')
			np->type = NAPPEND;
		else if (c == '|')
			np->type = NCLOBBER;
		else if (c == '&')
			np->type = NTOFD;
			/* it also can be NTO2 (>&file), but we can't figure it out yet */
		else {
			np->type = NTO;
			pungetc();
		}
	}
#if ENABLE_ASH_BASH_COMPAT
	else if (c == 0x100 + '>') { /* this flags &> redirection */
		np->nfile.fd = 1;
		pgetc(); /* this is '>', no need to check */
		np->type = NTO2;
	}
#endif
	else { /* c == '<' */
		/*np->nfile.fd = 0; - stzalloc did it */
		c = pgetc();
		switch (c) {
		case '<':
			if (sizeof(struct nfile) != sizeof(struct nhere)) {
				np = stzalloc(sizeof(struct nhere));
				/*np->nfile.fd = 0; - stzalloc did it */
			}
			np->type = NHERE;
			heredoc = stzalloc(sizeof(struct heredoc));
			heredoc->here = np;
			c = pgetc();
			if (c == '-') {
				heredoc->striptabs = 1;
			} else {
				/*heredoc->striptabs = 0; - stzalloc did it */
				pungetc();
			}
			break;

		case '&':
			np->type = NFROMFD;
			break;

		case '>':
			np->type = NFROMTO;
			break;

		default:
			np->type = NFROM;
			pungetc();
			break;
		}
	}
	if (fd >= 0)
		np->nfile.fd = fd;
	redirnode = np;
	goto parseredir_return;
}

/*
 * Parse a substitution.  At this point, we have read the dollar sign
 * and nothing else.
 */

/* is_special(c) evaluates to 1 for c in "!#$*-0123456789?@"; 0 otherwise
 * (assuming ascii char codes, as the original implementation did) */
#define is_special(c) \
	(((unsigned)(c) - 33 < 32) \
			&& ((0xc1ff920dU >> ((unsigned)(c) - 33)) & 1))
parsesub: {
	unsigned char subtype;
	int typeloc;
	int flags;

	c = pgetc();
	if (c > 255 /* PEOA or PEOF */
	 || (c != '(' && c != '{' && !is_name(c) && !is_special(c))
	) {
#if ENABLE_ASH_BASH_COMPAT
		if (c == '\'')
			bash_dollar_squote = 1;
		else
#endif
			USTPUTC('$', out);
		pungetc();
	} else if (c == '(') {
		/* $(command) or $((arith)) */
		if (pgetc() == '(') {
#if ENABLE_SH_MATH_SUPPORT
			PARSEARITH();
#else
			raise_error_syntax("you disabled math support for $((arith)) syntax");
#endif
		} else {
			pungetc();
			PARSEBACKQNEW();
		}
	} else {
		/* $VAR, $<specialchar>, ${...}, or PEOA/PEOF */
		USTPUTC(CTLVAR, out);
		typeloc = out - (char *)stackblock();
		USTPUTC(VSNORMAL, out);
		subtype = VSNORMAL;
		if (c == '{') {
			c = pgetc();
			if (c == '#') {
				c = pgetc();
				if (c == '}')
					c = '#'; /* ${#} - same as $# */
				else
					subtype = VSLENGTH; /* ${#VAR} */
			} else {
				subtype = 0;
			}
		}
		if (c <= 255 /* not PEOA or PEOF */ && is_name(c)) {
			/* $[{[#]]NAME[}] */
			do {
				STPUTC(c, out);
				c = pgetc();
			} while (c <= 255 /* not PEOA or PEOF */ && is_in_name(c));
		} else if (isdigit(c)) {
			/* $[{[#]]NUM[}] */
			do {
				STPUTC(c, out);
				c = pgetc();
			} while (isdigit(c));
		} else if (is_special(c)) {
			/* $[{[#]]<specialchar>[}] */
			USTPUTC(c, out);
			c = pgetc();
		} else {
 badsub:
			raise_error_syntax("bad substitution");
		}
		if (c != '}' && subtype == VSLENGTH) {
			/* ${#VAR didn't end with } */
			goto badsub;
		}

		STPUTC('=', out);
		flags = 0;
		if (subtype == 0) {
			/* ${VAR...} but not $VAR or ${#VAR} */
			/* c == first char after VAR */
			switch (c) {
			case ':':
				c = pgetc();
#if ENABLE_ASH_BASH_COMPAT
				if (c == ':' || c == '$' || isdigit(c)) {
//TODO: support more general format ${v:EXPR:EXPR},
// where EXPR follows $(()) rules
					subtype = VSSUBSTR;
					pungetc();
					break; /* "goto do_pungetc" is bigger (!) */
				}
#endif
				flags = VSNUL;
				/*FALLTHROUGH*/
			default: {
				static const char types[] ALIGN1 = "}-+?=";
				const char *p = strchr(types, c);
				if (p == NULL)
					goto badsub;
				subtype = p - types + VSNORMAL;
				break;
			}
			case '%':
			case '#': {
				int cc = c;
				subtype = (c == '#' ? VSTRIMLEFT : VSTRIMRIGHT);
				c = pgetc();
				if (c != cc)
					goto do_pungetc;
				subtype++;
				break;
			}
#if ENABLE_ASH_BASH_COMPAT
			case '/':
				/* ${v/[/]pattern/repl} */
//TODO: encode pattern and repl separately.
// Currently ${v/$var_with_slash/repl} is horribly broken
				subtype = VSREPLACE;
				c = pgetc();
				if (c != '/')
					goto do_pungetc;
				subtype++; /* VSREPLACEALL */
				break;
#endif
			}
		} else {
 do_pungetc:
			pungetc();
		}
		if (dblquote || arinest)
			flags |= VSQUOTE;
		((unsigned char *)stackblock())[typeloc] = subtype | flags;
		if (subtype != VSNORMAL) {
			varnest++;
			if (dblquote || arinest) {
				dqvarnest++;
			}
		}
	}
	goto parsesub_return;
}

/*
 * Called to parse command substitutions.  Newstyle is set if the command
 * is enclosed inside $(...); nlpp is a pointer to the head of the linked
 * list of commands (passed by reference), and savelen is the number of
 * characters on the top of the stack which must be preserved.
 */
parsebackq: {
	struct nodelist **nlpp;
	smallint savepbq;
	union node *n;
	char *volatile str;
	struct jmploc jmploc;
	struct jmploc *volatile savehandler;
	size_t savelen;
	smallint saveprompt = 0;

#ifdef __GNUC__
	(void) &saveprompt;
#endif
	savepbq = parsebackquote;
	if (setjmp(jmploc.loc)) {
		free(str);
		parsebackquote = 0;
		exception_handler = savehandler;
		longjmp(exception_handler->loc, 1);
	}
	INT_OFF;
	str = NULL;
	savelen = out - (char *)stackblock();
	if (savelen > 0) {
		str = ckmalloc(savelen);
		memcpy(str, stackblock(), savelen);
	}
	savehandler = exception_handler;
	exception_handler = &jmploc;
	INT_ON;
	if (oldstyle) {
		/* We must read until the closing backquote, giving special
		   treatment to some slashes, and then push the string and
		   reread it as input, interpreting it normally.  */
		char *pout;
		size_t psavelen;
		char *pstr;

		STARTSTACKSTR(pout);
		for (;;) {
			int pc;

			setprompt_if(needprompt, 2);
			pc = pgetc();
			switch (pc) {
			case '`':
				goto done;

			case '\\':
				pc = pgetc();
				if (pc == '\n') {
					g_parsefile->linno++;
					setprompt_if(doprompt, 2);
					/*
					 * If eating a newline, avoid putting
					 * the newline into the new character
					 * stream (via the STPUTC after the
					 * switch).
					 */
					continue;
				}
				if (pc != '\\' && pc != '`' && pc != '$'
				 && (!dblquote || pc != '"')
				) {
					STPUTC('\\', pout);
				}
				if (pc <= 255 /* not PEOA or PEOF */) {
					break;
				}
				/* fall through */

			case PEOF:
			IF_ASH_ALIAS(case PEOA:)
				startlinno = g_parsefile->linno;
				raise_error_syntax("EOF in backquote substitution");

			case '\n':
				g_parsefile->linno++;
				needprompt = doprompt;
				break;

			default:
				break;
			}
			STPUTC(pc, pout);
		}
 done:
		STPUTC('\0', pout);
		psavelen = pout - (char *)stackblock();
		if (psavelen > 0) {
			pstr = grabstackstr(pout);
			setinputstring(pstr);
		}
	}
	nlpp = &bqlist;
	while (*nlpp)
		nlpp = &(*nlpp)->next;
	*nlpp = stzalloc(sizeof(**nlpp));
	/* (*nlpp)->next = NULL; - stzalloc did it */
	parsebackquote = oldstyle;

	if (oldstyle) {
		saveprompt = doprompt;
		doprompt = 0;
	}

	n = list(2);

	if (oldstyle)
		doprompt = saveprompt;
	else if (readtoken() != TRP)
		raise_error_unexpected_syntax(TRP);

	(*nlpp)->n = n;
	if (oldstyle) {
		/*
		 * Start reading from old file again, ignoring any pushed back
		 * tokens left from the backquote parsing
		 */
		popfile();
		tokpushback = 0;
	}
	while (stackblocksize() <= savelen)
		growstackblock();
	STARTSTACKSTR(out);
	if (str) {
		memcpy(out, str, savelen);
		STADJUST(savelen, out);
		INT_OFF;
		free(str);
		str = NULL;
		INT_ON;
	}
	parsebackquote = savepbq;
	exception_handler = savehandler;
	if (arinest || dblquote)
		USTPUTC(CTLBACKQ | CTLQUOTE, out);
	else
		USTPUTC(CTLBACKQ, out);
	if (oldstyle)
		goto parsebackq_oldreturn;
	goto parsebackq_newreturn;
}

#if ENABLE_SH_MATH_SUPPORT
/*
 * Parse an arithmetic expansion (indicate start of one and set state)
 */
parsearith: {
	if (++arinest == 1) {
		prevsyntax = syntax;
		syntax = ARISYNTAX;
		USTPUTC(CTLARI, out);
		if (dblquote)
			USTPUTC('"', out);
		else
			USTPUTC(' ', out);
	} else {
		/*
		 * we collapse embedded arithmetic expansion to
		 * parenthesis, which should be equivalent
		 */
		USTPUTC('(', out);
	}
	goto parsearith_return;
}
#endif

} /* end of readtoken */

/*
 * Read the next input token.
 * If the token is a word, we set backquotelist to the list of cmds in
 *      backquotes.  We set quoteflag to true if any part of the word was
 *      quoted.
 * If the token is TREDIR, then we set redirnode to a structure containing
 *      the redirection.
 * In all cases, the variable startlinno is set to the number of the line
 *      on which the token starts.
 *
 * [Change comment:  here documents and internal procedures]
 * [Readtoken shouldn't have any arguments.  Perhaps we should make the
 *  word parsing code into a separate routine.  In this case, readtoken
 *  doesn't need to have any internal procedures, but parseword does.
 *  We could also make parseoperator in essence the main routine, and
 *  have parseword (readtoken1?) handle both words and redirection.]
 */
#define NEW_xxreadtoken
#ifdef NEW_xxreadtoken
/* singles must be first! */
static const char xxreadtoken_chars[7] ALIGN1 = {
	'\n', '(', ')', /* singles */
	'&', '|', ';',  /* doubles */
	0
};

#define xxreadtoken_singles 3
#define xxreadtoken_doubles 3

static const char xxreadtoken_tokens[] ALIGN1 = {
	TNL, TLP, TRP,          /* only single occurrence allowed */
	TBACKGND, TPIPE, TSEMI, /* if single occurrence */
	TEOF,                   /* corresponds to trailing nul */
	TAND, TOR, TENDCASE     /* if double occurrence */
};

static int
xxreadtoken(void)
{
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	setprompt_if(needprompt, 2);
	startlinno = g_parsefile->linno;
	for (;;) {                      /* until token or start of word found */
		c = pgetc_fast();
		if (c == ' ' || c == '\t' IF_ASH_ALIAS( || c == PEOA))
			continue;

		if (c == '#') {
			while ((c = pgetc()) != '\n' && c != PEOF)
				continue;
			pungetc();
		} else if (c == '\\') {
			if (pgetc() != '\n') {
				pungetc();
				break; /* return readtoken1(...) */
			}
			startlinno = ++g_parsefile->linno;
			setprompt_if(doprompt, 2);
		} else {
			const char *p;

			p = xxreadtoken_chars + sizeof(xxreadtoken_chars) - 1;
			if (c != PEOF) {
				if (c == '\n') {
					g_parsefile->linno++;
					needprompt = doprompt;
				}

				p = strchr(xxreadtoken_chars, c);
				if (p == NULL)
					break; /* return readtoken1(...) */

				if ((int)(p - xxreadtoken_chars) >= xxreadtoken_singles) {
					int cc = pgetc();
					if (cc == c) {    /* double occurrence? */
						p += xxreadtoken_doubles + 1;
					} else {
						pungetc();
#if ENABLE_ASH_BASH_COMPAT
						if (c == '&' && cc == '>') /* &> */
							break; /* return readtoken1(...) */
#endif
					}
				}
			}
			lasttoken = xxreadtoken_tokens[p - xxreadtoken_chars];
			return lasttoken;
		}
	} /* for (;;) */

	return readtoken1(c, BASESYNTAX, (char *) NULL, 0);
}
#else /* old xxreadtoken */
#define RETURN(token)   return lasttoken = token
static int
xxreadtoken(void)
{
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	setprompt_if(needprompt, 2);
	startlinno = g_parsefile->linno;
	for (;;) {      /* until token or start of word found */
		c = pgetc_fast();
		switch (c) {
		case ' ': case '\t':
		IF_ASH_ALIAS(case PEOA:)
			continue;
		case '#':
			while ((c = pgetc()) != '\n' && c != PEOF)
				continue;
			pungetc();
			continue;
		case '\\':
			if (pgetc() == '\n') {
				startlinno = ++g_parsefile->linno;
				setprompt_if(doprompt, 2);
				continue;
			}
			pungetc();
			goto breakloop;
		case '\n':
			g_parsefile->linno++;
			needprompt = doprompt;
			RETURN(TNL);
		case PEOF:
			RETURN(TEOF);
		case '&':
			if (pgetc() == '&')
				RETURN(TAND);
			pungetc();
			RETURN(TBACKGND);
		case '|':
			if (pgetc() == '|')
				RETURN(TOR);
			pungetc();
			RETURN(TPIPE);
		case ';':
			if (pgetc() == ';')
				RETURN(TENDCASE);
			pungetc();
			RETURN(TSEMI);
		case '(':
			RETURN(TLP);
		case ')':
			RETURN(TRP);
		default:
			goto breakloop;
		}
	}
 breakloop:
	return readtoken1(c, BASESYNTAX, (char *)NULL, 0);
#undef RETURN
}
#endif /* old xxreadtoken */

static int
readtoken(void)
{
	int t;
#if DEBUG
	smallint alreadyseen = tokpushback;
#endif

#if ENABLE_ASH_ALIAS
 top:
#endif

	t = xxreadtoken();

	/*
	 * eat newlines
	 */
	if (checkkwd & CHKNL) {
		while (t == TNL) {
			parseheredoc();
			t = xxreadtoken();
		}
	}

	if (t != TWORD || quoteflag) {
		goto out;
	}

	/*
	 * check for keywords
	 */
	if (checkkwd & CHKKWD) {
		const char *const *pp;

		pp = findkwd(wordtext);
		if (pp) {
			lasttoken = t = pp - tokname_array;
			TRACE(("keyword '%s' recognized\n", tokname_array[t] + 1));
			goto out;
		}
	}

	if (checkkwd & CHKALIAS) {
#if ENABLE_ASH_ALIAS
		struct alias *ap;
		ap = lookupalias(wordtext, 1);
		if (ap != NULL) {
			if (*ap->val) {
				pushstring(ap->val, ap);
			}
			goto top;
		}
#endif
	}
 out:
	checkkwd = 0;
#if DEBUG
	if (!alreadyseen)
		TRACE(("token '%s' %s\n", tokname_array[t] + 1, t == TWORD ? wordtext : ""));
	else
		TRACE(("reread token '%s' %s\n", tokname_array[t] + 1, t == TWORD ? wordtext : ""));
#endif
	return t;
}

static char
peektoken(void)
{
	int t;

	t = readtoken();
	tokpushback = 1;
	return tokname_array[t][0];
}

/*
 * Read and parse a command.  Returns NODE_EOF on end of file.
 * (NULL is a valid parse tree indicating a blank line.)
 */
static union node *
parsecmd(int interact)
{
	int t;

	tokpushback = 0;
	doprompt = interact;
	setprompt_if(doprompt, doprompt);
	needprompt = 0;
	t = readtoken();
	if (t == TEOF)
		return NODE_EOF;
	if (t == TNL)
		return NULL;
	tokpushback = 1;
	return list(1);
}

/*
 * Input any here documents.
 */
static void
parseheredoc(void)
{
	struct heredoc *here;
	union node *n;

	here = heredoclist;
	heredoclist = NULL;

	while (here) {
		setprompt_if(needprompt, 2);
		readtoken1(pgetc(), here->here->type == NHERE ? SQSYNTAX : DQSYNTAX,
				here->eofmark, here->striptabs);
		n = stzalloc(sizeof(struct narg));
		n->narg.type = NARG;
		/*n->narg.next = NULL; - stzalloc did it */
		n->narg.text = wordtext;
		n->narg.backquote = backquotelist;
		here->here->nhere.doc = n;
		here = here->next;
	}
}


/*
 * called by editline -- any expansions to the prompt should be added here.
 */
#if ENABLE_ASH_EXPAND_PRMT
static const char *
expandstr(const char *ps)
{
	union node n;

	/* XXX Fix (char *) cast. It _is_ a bug. ps is variable's value,
	 * and token processing _can_ alter it (delete NULs etc). */
	setinputstring((char *)ps);
	readtoken1(pgetc(), PSSYNTAX, nullstr, 0);
	popfile();

	n.narg.type = NARG;
	n.narg.next = NULL;
	n.narg.text = wordtext;
	n.narg.backquote = backquotelist;

	expandarg(&n, NULL, 0);
	return stackblock();
}
#endif

/*
 * Execute a command or commands contained in a string.
 */
static int
evalstring(char *s, int mask)
{
	union node *n;
	struct stackmark smark;
	int skip;

	setinputstring(s);
	setstackmark(&smark);

	skip = 0;
	while ((n = parsecmd(0)) != NODE_EOF) {
		evaltree(n, 0);
		popstackmark(&smark);
		skip = evalskip;
		if (skip)
			break;
	}
	popfile();

	skip &= mask;
	evalskip = skip;
	return skip;
}

/*
 * The eval command.
 */
static int FAST_FUNC
evalcmd(int argc UNUSED_PARAM, char **argv)
{
	char *p;
	char *concat;

	if (argv[1]) {
		p = argv[1];
		argv += 2;
		if (argv[0]) {
			STARTSTACKSTR(concat);
			for (;;) {
				concat = stack_putstr(p, concat);
				p = *argv++;
				if (p == NULL)
					break;
				STPUTC(' ', concat);
			}
			STPUTC('\0', concat);
			p = grabstackstr(concat);
		}
		evalstring(p, ~SKIPEVAL);
	}
	return exitstatus;
}

/*
 * Read and execute commands.
 * "Top" is nonzero for the top level command loop;
 * it turns on prompting if the shell is interactive.
 */
static int
cmdloop(int top)
{
	union node *n;
	struct stackmark smark;
	int inter;
	int numeof = 0;

	TRACE(("cmdloop(%d) called\n", top));
	for (;;) {
		int skip;

		setstackmark(&smark);
#if JOBS
		if (doing_jobctl)
			showjobs(stderr, SHOW_CHANGED);
#endif
		inter = 0;
		if (iflag && top) {
			inter++;
			chkmail();
		}
		n = parsecmd(inter);
#if DEBUG
		if (DEBUG > 2 && debug && (n != NODE_EOF))
			showtree(n);
#endif
		if (n == NODE_EOF) {
			if (!top || numeof >= 50)
				break;
			if (!stoppedjobs()) {
				if (!Iflag)
					break;
				out2str("\nUse \"exit\" to leave shell.\n");
			}
			numeof++;
		} else if (nflag == 0) {
			/* job_warning can only be 2,1,0. Here 2->1, 1/0->0 */
			job_warning >>= 1;
			numeof = 0;
			evaltree(n, 0);
		}
		popstackmark(&smark);
		skip = evalskip;

		if (skip) {
			evalskip = 0;
			return skip & SKIPEVAL;
		}
	}
	return 0;
}

/*
 * Take commands from a file.  To be compatible we should do a path
 * search for the file, which is necessary to find sub-commands.
 */
static char *
find_dot_file(char *name)
{
	char *fullname;
	const char *path = pathval();
	struct stat statb;

	/* don't try this for absolute or relative paths */
	if (strchr(name, '/'))
		return name;

	/* IIRC standards do not say whether . is to be searched.
	 * And it is even smaller this way, making it unconditional for now:
	 */
	if (1) { /* ENABLE_ASH_BASH_COMPAT */
		fullname = name;
		goto try_cur_dir;
	}

	while ((fullname = path_advance(&path, name)) != NULL) {
 try_cur_dir:
		if ((stat(fullname, &statb) == 0) && S_ISREG(statb.st_mode)) {
			/*
			 * Don't bother freeing here, since it will
			 * be freed by the caller.
			 */
			return fullname;
		}
		if (fullname != name)
			stunalloc(fullname);
	}

	/* not found in the PATH */
	ash_msg_and_raise_error("%s: not found", name);
	/* NOTREACHED */
}

static int FAST_FUNC
dotcmd(int argc, char **argv)
{
	char *fullname;
	struct strlist *sp;
	volatile struct shparam saveparam;

	for (sp = cmdenviron; sp; sp = sp->next)
		setvareq(ckstrdup(sp->text), VSTRFIXED | VTEXTFIXED);

	if (!argv[1]) {
		/* bash says: "bash: .: filename argument required" */
		return 2; /* bash compat */
	}

	/* "false; . empty_file; echo $?" should print 0, not 1: */
	exitstatus = 0;

	fullname = find_dot_file(argv[1]);

	argv += 2;
	argc -= 2;
	if (argc) { /* argc > 0, argv[0] != NULL */
		saveparam = shellparam;
		shellparam.malloced = 0;
		shellparam.nparam = argc;
		shellparam.p = argv;
	};

	setinputfile(fullname, INPUT_PUSH_FILE);
	commandname = fullname;
	cmdloop(0);
	popfile();

	if (argc) {
		freeparam(&shellparam);
		shellparam = saveparam;
	};

	return exitstatus;
}

static int FAST_FUNC
exitcmd(int argc UNUSED_PARAM, char **argv)
{
	if (stoppedjobs())
		return 0;
	if (argv[1])
		exitstatus = number(argv[1]);
	raise_exception(EXEXIT);
	/* NOTREACHED */
}

/*
 * Read a file containing shell functions.
 */
static void
readcmdfile(char *name)
{
	setinputfile(name, INPUT_PUSH_FILE);
	cmdloop(0);
	popfile();
}


/* ============ find_command inplementation */

/*
 * Resolve a command name.  If you change this routine, you may have to
 * change the shellexec routine as well.
 */
static void
find_command(char *name, struct cmdentry *entry, int act, const char *path)
{
	struct tblentry *cmdp;
	int idx;
	int prev;
	char *fullname;
	struct stat statb;
	int e;
	int updatetbl;
	struct builtincmd *bcmd;

	/* If name contains a slash, don't use PATH or hash table */
	if (strchr(name, '/') != NULL) {
		entry->u.index = -1;
		if (act & DO_ABS) {
			while (stat(name, &statb) < 0) {
#ifdef SYSV
				if (errno == EINTR)
					continue;
#endif
				entry->cmdtype = CMDUNKNOWN;
				return;
			}
		}
		entry->cmdtype = CMDNORMAL;
		return;
	}

/* #if ENABLE_FEATURE_SH_STANDALONE... moved after builtin check */

	updatetbl = (path == pathval());
	if (!updatetbl) {
		act |= DO_ALTPATH;
		if (strstr(path, "%builtin") != NULL)
			act |= DO_ALTBLTIN;
	}

	/* If name is in the table, check answer will be ok */
	cmdp = cmdlookup(name, 0);
	if (cmdp != NULL) {
		int bit;

		switch (cmdp->cmdtype) {
		default:
#if DEBUG
			abort();
#endif
		case CMDNORMAL:
			bit = DO_ALTPATH;
			break;
		case CMDFUNCTION:
			bit = DO_NOFUNC;
			break;
		case CMDBUILTIN:
			bit = DO_ALTBLTIN;
			break;
		}
		if (act & bit) {
			updatetbl = 0;
			cmdp = NULL;
		} else if (cmdp->rehash == 0)
			/* if not invalidated by cd, we're done */
			goto success;
	}

	/* If %builtin not in path, check for builtin next */
	bcmd = find_builtin(name);
	if (bcmd) {
		if (IS_BUILTIN_REGULAR(bcmd))
			goto builtin_success;
		if (act & DO_ALTPATH) {
			if (!(act & DO_ALTBLTIN))
				goto builtin_success;
		} else if (builtinloc <= 0) {
			goto builtin_success;
		}
	}

#if ENABLE_FEATURE_SH_STANDALONE
	{
		int applet_no = find_applet_by_name(name);
		if (applet_no >= 0) {
			entry->cmdtype = CMDNORMAL;
			entry->u.index = -2 - applet_no;
			return;
		}
	}
#endif

	/* We have to search path. */
	prev = -1;              /* where to start */
	if (cmdp && cmdp->rehash) {     /* doing a rehash */
		if (cmdp->cmdtype == CMDBUILTIN)
			prev = builtinloc;
		else
			prev = cmdp->param.index;
	}

	e = ENOENT;
	idx = -1;
 loop:
	while ((fullname = path_advance(&path, name)) != NULL) {
		stunalloc(fullname);
		/* NB: code below will still use fullname
		 * despite it being "unallocated" */
		idx++;
		if (pathopt) {
			if (prefix(pathopt, "builtin")) {
				if (bcmd)
					goto builtin_success;
				continue;
			}
			if ((act & DO_NOFUNC)
			 || !prefix(pathopt, "func")
			) {     /* ignore unimplemented options */
				continue;
			}
		}
		/* if rehash, don't redo absolute path names */
		if (fullname[0] == '/' && idx <= prev) {
			if (idx < prev)
				continue;
			TRACE(("searchexec \"%s\": no change\n", name));
			goto success;
		}
		while (stat(fullname, &statb) < 0) {
#ifdef SYSV
			if (errno == EINTR)
				continue;
#endif
			if (errno != ENOENT && errno != ENOTDIR)
				e = errno;
			goto loop;
		}
		e = EACCES;     /* if we fail, this will be the error */
		if (!S_ISREG(statb.st_mode))
			continue;
		if (pathopt) {          /* this is a %func directory */
			stalloc(strlen(fullname) + 1);
			/* NB: stalloc will return space pointed by fullname
			 * (because we don't have any intervening allocations
			 * between stunalloc above and this stalloc) */
			readcmdfile(fullname);
			cmdp = cmdlookup(name, 0);
			if (cmdp == NULL || cmdp->cmdtype != CMDFUNCTION)
				ash_msg_and_raise_error("%s not defined in %s", name, fullname);
			stunalloc(fullname);
			goto success;
		}
		TRACE(("searchexec \"%s\" returns \"%s\"\n", name, fullname));
		if (!updatetbl) {
			entry->cmdtype = CMDNORMAL;
			entry->u.index = idx;
			return;
		}
		INT_OFF;
		cmdp = cmdlookup(name, 1);
		cmdp->cmdtype = CMDNORMAL;
		cmdp->param.index = idx;
		INT_ON;
		goto success;
	}

	/* We failed.  If there was an entry for this command, delete it */
	if (cmdp && updatetbl)
		delete_cmd_entry();
	if (act & DO_ERR)
		ash_msg("%s: %s", name, errmsg(e, "not found"));
	entry->cmdtype = CMDUNKNOWN;
	return;

 builtin_success:
	if (!updatetbl) {
		entry->cmdtype = CMDBUILTIN;
		entry->u.cmd = bcmd;
		return;
	}
	INT_OFF;
	cmdp = cmdlookup(name, 1);
	cmdp->cmdtype = CMDBUILTIN;
	cmdp->param.cmd = bcmd;
	INT_ON;
 success:
	cmdp->rehash = 0;
	entry->cmdtype = cmdp->cmdtype;
	entry->u = cmdp->param;
}


/* ============ trap.c */

/*
 * The trap builtin.
 */
static int FAST_FUNC
trapcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char *action;
	char **ap;
	int signo, exitcode;

	nextopt(nullstr);
	ap = argptr;
	if (!*ap) {
		for (signo = 0; signo < NSIG; signo++) {
			char *tr = trap_ptr[signo];
			if (tr) {
				/* note: bash adds "SIG", but only if invoked
				 * as "bash". If called as "sh", or if set -o posix,
				 * then it prints short signal names.
				 * We are printing short names: */
				out1fmt("trap -- %s %s\n",
						single_quote(tr),
						get_signame(signo));
		/* trap_ptr != trap only if we are in special-cased `trap` code.
		 * In this case, we will exit very soon, no need to free(). */
				/* if (trap_ptr != trap && tp[0]) */
				/*	free(tr); */
			}
		}
		/*
		if (trap_ptr != trap) {
			free(trap_ptr);
			trap_ptr = trap;
		}
		*/
		return 0;
	}

	action = NULL;
	if (ap[1])
		action = *ap++;
	exitcode = 0;
	while (*ap) {
		signo = get_signum(*ap);
		if (signo < 0) {
			/* Mimic bash message exactly */
			ash_msg("%s: invalid signal specification", *ap);
			exitcode = 1;
			goto next;
		}
		INT_OFF;
		if (action) {
			if (LONE_DASH(action))
				action = NULL;
			else
				action = ckstrdup(action);
		}
		free(trap[signo]);
		if (action)
			may_have_traps = 1;
		trap[signo] = action;
		if (signo != 0)
			setsignal(signo);
		INT_ON;
 next:
		ap++;
	}
	return exitcode;
}


/* ============ Builtins */

#if !ENABLE_FEATURE_SH_EXTRA_QUIET
/*
 * Lists available builtins
 */
static int FAST_FUNC
helpcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	unsigned col;
	unsigned i;

	out1fmt(
		"Built-in commands:\n"
		"------------------\n");
	for (col = 0, i = 0; i < ARRAY_SIZE(builtintab); i++) {
		col += out1fmt("%c%s", ((col == 0) ? '\t' : ' '),
					builtintab[i].name + 1);
		if (col > 60) {
			out1fmt("\n");
			col = 0;
		}
	}
#if ENABLE_FEATURE_SH_STANDALONE
	{
		const char *a = applet_names;
		while (*a) {
			col += out1fmt("%c%s", ((col == 0) ? '\t' : ' '), a);
			if (col > 60) {
				out1fmt("\n");
				col = 0;
			}
			a += strlen(a) + 1;
		}
	}
#endif
	out1fmt("\n\n");
	return EXIT_SUCCESS;
}
#endif /* FEATURE_SH_EXTRA_QUIET */

/*
 * The export and readonly commands.
 */
static int FAST_FUNC
exportcmd(int argc UNUSED_PARAM, char **argv)
{
	struct var *vp;
	char *name;
	const char *p;
	char **aptr;
	int flag = argv[0][0] == 'r' ? VREADONLY : VEXPORT;

	if (nextopt("p") != 'p') {
		aptr = argptr;
		name = *aptr;
		if (name) {
			do {
				p = strchr(name, '=');
				if (p != NULL) {
					p++;
				} else {
					vp = *findvar(hashvar(name), name);
					if (vp) {
						vp->flags |= flag;
						continue;
					}
				}
				setvar(name, p, flag);
			} while ((name = *++aptr) != NULL);
			return 0;
		}
	}
	showvars(argv[0], flag, 0);
	return 0;
}

/*
 * Delete a function if it exists.
 */
static void
unsetfunc(const char *name)
{
	struct tblentry *cmdp;

	cmdp = cmdlookup(name, 0);
	if (cmdp != NULL && cmdp->cmdtype == CMDFUNCTION)
		delete_cmd_entry();
}

/*
 * The unset builtin command.  We unset the function before we unset the
 * variable to allow a function to be unset when there is a readonly variable
 * with the same name.
 */
static int FAST_FUNC
unsetcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char **ap;
	int i;
	int flag = 0;
	int ret = 0;

	while ((i = nextopt("vf")) != 0) {
		flag = i;
	}

	for (ap = argptr; *ap; ap++) {
		if (flag != 'f') {
			i = unsetvar(*ap);
			ret |= i;
			if (!(i & 2))
				continue;
		}
		if (flag != 'v')
			unsetfunc(*ap);
	}
	return ret & 1;
}

static const unsigned char timescmd_str[] ALIGN1 = {
	' ',  offsetof(struct tms, tms_utime),
	'\n', offsetof(struct tms, tms_stime),
	' ',  offsetof(struct tms, tms_cutime),
	'\n', offsetof(struct tms, tms_cstime),
	0
};
static int FAST_FUNC
timescmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	unsigned long clk_tck, s, t;
	const unsigned char *p;
	struct tms buf;

	clk_tck = sysconf(_SC_CLK_TCK);
	times(&buf);

	p = timescmd_str;
	do {
		t = *(clock_t *)(((char *) &buf) + p[1]);
		s = t / clk_tck;
		t = t % clk_tck;
		out1fmt("%lum%lu.%03lus%c",
			s / 60, s % 60,
			(t * 1000) / clk_tck,
			p[0]);
		p += 2;
	} while (*p);

	return 0;
}

#if ENABLE_SH_MATH_SUPPORT
/*
 * The let builtin. Partially stolen from GNU Bash, the Bourne Again SHell.
 * Copyright (C) 1987, 1989, 1991 Free Software Foundation, Inc.
 *
 * Copyright (C) 2003 Vladimir Oleynik <dzo@simtreas.ru>
 */
static int FAST_FUNC
letcmd(int argc UNUSED_PARAM, char **argv)
{
	arith_t i;

	argv++;
	if (!*argv)
		ash_msg_and_raise_error("expression expected");
	do {
		i = ash_arith(*argv);
	} while (*++argv);

	return !i;
}
#endif

/*
 * The read builtin. Options:
 *      -r              Do not interpret '\' specially
 *      -s              Turn off echo (tty only)
 *      -n NCHARS       Read NCHARS max
 *      -p PROMPT       Display PROMPT on stderr (if input is from tty)
 *      -t SECONDS      Timeout after SECONDS (tty or pipe only)
 *      -u FD           Read from given FD instead of fd 0
 * This uses unbuffered input, which may be avoidable in some cases.
 * TODO: bash also has:
 *      -a ARRAY        Read into array[0],[1],etc
 *      -d DELIM        End on DELIM char, not newline
 *      -e              Use line editing (tty only)
 */
static int FAST_FUNC
readcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char *opt_n = NULL;
	char *opt_p = NULL;
	char *opt_t = NULL;
	char *opt_u = NULL;
	int read_flags = 0;
	const char *r;
	int i;

	while ((i = nextopt("p:u:rt:n:s")) != '\0') {
		switch (i) {
		case 'p':
			opt_p = optionarg;
			break;
		case 'n':
			opt_n = optionarg;
			break;
		case 's':
			read_flags |= BUILTIN_READ_SILENT;
			break;
		case 't':
			opt_t = optionarg;
			break;
		case 'r':
			read_flags |= BUILTIN_READ_RAW;
			break;
		case 'u':
			opt_u = optionarg;
			break;
		default:
			break;
		}
	}

	r = shell_builtin_read(setvar2,
		argptr,
		bltinlookup("IFS"), /* can be NULL */
		read_flags,
		opt_n,
		opt_p,
		opt_t,
		opt_u
	);

	if ((uintptr_t)r > 1)
		ash_msg_and_raise_error(r);

	return (uintptr_t)r;
}

static int FAST_FUNC
umaskcmd(int argc UNUSED_PARAM, char **argv)
{
	static const char permuser[3] ALIGN1 = "ugo";
	static const char permmode[3] ALIGN1 = "rwx";
	static const short permmask[] ALIGN2 = {
		S_IRUSR, S_IWUSR, S_IXUSR,
		S_IRGRP, S_IWGRP, S_IXGRP,
		S_IROTH, S_IWOTH, S_IXOTH
	};

	/* TODO: use bb_parse_mode() instead */

	char *ap;
	mode_t mask;
	int i;
	int symbolic_mode = 0;

	while (nextopt("S") != '\0') {
		symbolic_mode = 1;
	}

	INT_OFF;
	mask = umask(0);
	umask(mask);
	INT_ON;

	ap = *argptr;
	if (ap == NULL) {
		if (symbolic_mode) {
			char buf[18];
			char *p = buf;

			for (i = 0; i < 3; i++) {
				int j;

				*p++ = permuser[i];
				*p++ = '=';
				for (j = 0; j < 3; j++) {
					if ((mask & permmask[3 * i + j]) == 0) {
						*p++ = permmode[j];
					}
				}
				*p++ = ',';
			}
			*--p = 0;
			puts(buf);
		} else {
			out1fmt("%.4o\n", mask);
		}
	} else {
		if (isdigit((unsigned char) *ap)) {
			mask = 0;
			do {
				if (*ap >= '8' || *ap < '0')
					ash_msg_and_raise_error(msg_illnum, argv[1]);
				mask = (mask << 3) + (*ap - '0');
			} while (*++ap != '\0');
			umask(mask);
		} else {
			mask = ~mask & 0777;
			if (!bb_parse_mode(ap, &mask)) {
				ash_msg_and_raise_error("illegal mode: %s", ap);
			}
			umask(~mask & 0777);
		}
	}
	return 0;
}

static int FAST_FUNC
ulimitcmd(int argc UNUSED_PARAM, char **argv)
{
	return shell_builtin_ulimit(argv);
}

/* ============ main() and helpers */

/*
 * Called to exit the shell.
 */
static void
exitshell(void)
{
	struct jmploc loc;
	char *p;
	int status;

#if ENABLE_FEATURE_EDITING_SAVE_ON_EXIT
	save_history(line_input_state);
#endif

	status = exitstatus;
	TRACE(("pid %d, exitshell(%d)\n", getpid(), status));
	if (setjmp(loc.loc)) {
		if (exception_type == EXEXIT)
/* dash bug: it just does _exit(exitstatus) here
 * but we have to do setjobctl(0) first!
 * (bug is still not fixed in dash-0.5.3 - if you run dash
 * under Midnight Commander, on exit from dash MC is backgrounded) */
			status = exitstatus;
		goto out;
	}
	exception_handler = &loc;
	p = trap[0];
	if (p) {
		trap[0] = NULL;
		evalstring(p, 0);
		free(p);
	}
	flush_stdout_stderr();
 out:
	setjobctl(0);
	_exit(status);
	/* NOTREACHED */
}

static void
init(void)
{
	/* from input.c: */
	/* we will never free this */
	basepf.next_to_pgetc = basepf.buf = ckmalloc(IBUFSIZ);

	/* from trap.c: */
	signal(SIGCHLD, SIG_DFL);
	/* bash re-enables SIGHUP which is SIG_IGNed on entry.
	 * Try: "trap '' HUP; bash; echo RET" and type "kill -HUP $$"
	 */
	signal(SIGHUP, SIG_DFL);

	/* from var.c: */
	{
		char **envp;
		const char *p;
		struct stat st1, st2;

		initvar();
		for (envp = environ; envp && *envp; envp++) {
			if (strchr(*envp, '=')) {
				setvareq(*envp, VEXPORT|VTEXTFIXED);
			}
		}

		setvar("PPID", utoa(getppid()), 0);

		p = lookupvar("PWD");
		if (p) {
			if (*p != '/' || stat(p, &st1) || stat(".", &st2)
			 || st1.st_dev != st2.st_dev || st1.st_ino != st2.st_ino
			) {
				p = '\0';
			}
		}
		setpwd(p, 0);
	}
}


//usage:#define ash_trivial_usage
//usage:	"[-/+OPTIONS] [-/+o OPT]... [-c 'SCRIPT' [ARG0 [ARGS]] / FILE [ARGS]]"
//usage:#define ash_full_usage "\n\n"
//usage:	"Unix shell interpreter"

//usage:#if ENABLE_FEATURE_SH_IS_ASH
//usage:# define sh_trivial_usage ash_trivial_usage
//usage:# define sh_full_usage    ash_full_usage
//usage:#endif
//usage:#if ENABLE_FEATURE_BASH_IS_ASH
//usage:# define bash_trivial_usage ash_trivial_usage
//usage:# define bash_full_usage    ash_full_usage
//usage:#endif

/*
 * Process the shell command line arguments.
 */
static void
procargs(char **argv)
{
	int i;
	const char *xminusc;
	char **xargv;

	xargv = argv;
	arg0 = xargv[0];
	/* if (xargv[0]) - mmm, this is always true! */
		xargv++;
	for (i = 0; i < NOPTS; i++)
		optlist[i] = 2;
	argptr = xargv;
	if (options(/*cmdline:*/ 1)) {
		/* it already printed err message */
		raise_exception(EXERROR);
	}
	xargv = argptr;
	xminusc = minusc;
	if (*xargv == NULL) {
		if (xminusc)
			ash_msg_and_raise_error(bb_msg_requires_arg, "-c");
		sflag = 1;
	}
	if (iflag == 2 && sflag == 1 && isatty(0) && isatty(1))
		iflag = 1;
	if (mflag == 2)
		mflag = iflag;
	for (i = 0; i < NOPTS; i++)
		if (optlist[i] == 2)
			optlist[i] = 0;
#if DEBUG == 2
	debug = 1;
#endif
	/* POSIX 1003.2: first arg after -c cmd is $0, remainder $1... */
	if (xminusc) {
		minusc = *xargv++;
		if (*xargv)
			goto setarg0;
	} else if (!sflag) {
		setinputfile(*xargv, 0);
 setarg0:
		arg0 = *xargv++;
		commandname = arg0;
	}

	shellparam.p = xargv;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	/* assert(shellparam.malloced == 0 && shellparam.nparam == 0); */
	while (*xargv) {
		shellparam.nparam++;
		xargv++;
	}
	optschanged();
}

/*
 * Read /etc/profile or .profile.
 */
static void
read_profile(const char *name)
{
	int skip;

	if (setinputfile(name, INPUT_PUSH_FILE | INPUT_NOFILE_OK) < 0)
		return;
	skip = cmdloop(0);
	popfile();
	if (skip)
		exitshell();
}

/*
 * This routine is called when an error or an interrupt occurs in an
 * interactive shell and control is returned to the main command loop.
 */
static void
reset(void)
{
	/* from eval.c: */
	evalskip = 0;
	loopnest = 0;
	/* from input.c: */
	g_parsefile->left_in_buffer = 0;
	g_parsefile->left_in_line = 0;      /* clear input buffer */
	popallfiles();
	/* from parser.c: */
	tokpushback = 0;
	checkkwd = 0;
	/* from redir.c: */
	clearredir(/*drop:*/ 0);
}

#if PROFILE
static short profile_buf[16384];
extern int etext();
#endif

/*
 * Main routine.  We initialize things, parse the arguments, execute
 * profiles if we're a login shell, and then call cmdloop to execute
 * commands.  The setjmp call sets up the location to jump to when an
 * exception occurs.  When an exception occurs the variable "state"
 * is used to figure out how far we had gotten.
 */
int ash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ash_main(int argc UNUSED_PARAM, char **argv)
{
	const char *shinit;
	volatile smallint state;
	struct jmploc jmploc;
	struct stackmark smark;

	/* Initialize global data */
	INIT_G_misc();
	INIT_G_memstack();
	INIT_G_var();
#if ENABLE_ASH_ALIAS
	INIT_G_alias();
#endif
	INIT_G_cmdtable();

#if PROFILE
	monitor(4, etext, profile_buf, sizeof(profile_buf), 50);
#endif

#if ENABLE_FEATURE_EDITING
	line_input_state = new_line_input_t(FOR_SHELL | WITH_PATH_LOOKUP);
#endif
	state = 0;
	if (setjmp(jmploc.loc)) {
		smallint e;
		smallint s;

		reset();

		e = exception_type;
		if (e == EXERROR)
			exitstatus = 2;
		s = state;
		if (e == EXEXIT || s == 0 || iflag == 0 || shlvl) {
			exitshell();
		}
		if (e == EXINT) {
			outcslow('\n', stderr);
		}

		popstackmark(&smark);
		FORCE_INT_ON; /* enable interrupts */
		if (s == 1)
			goto state1;
		if (s == 2)
			goto state2;
		if (s == 3)
			goto state3;
		goto state4;
	}
	exception_handler = &jmploc;
#if DEBUG
	opentrace();
	TRACE(("Shell args: "));
	trace_puts_args(argv);
#endif
	rootpid = getpid();

	init();
	setstackmark(&smark);
	procargs(argv);

#if ENABLE_FEATURE_EDITING_SAVEHISTORY
	if (iflag) {
		const char *hp = lookupvar("HISTFILE");
		if (!hp) {
			hp = lookupvar("HOME");
			if (hp) {
				char *defhp = concat_path_file(hp, ".ash_history");
				setvar("HISTFILE", defhp, 0);
				free(defhp);
			}
		}
	}
#endif
	if (argv[0] && argv[0][0] == '-')
		isloginsh = 1;
	if (isloginsh) {
		state = 1;
		read_profile("/etc/profile");
 state1:
		state = 2;
		read_profile(".profile");
	}
 state2:
	state = 3;
	if (
#ifndef linux
	 getuid() == geteuid() && getgid() == getegid() &&
#endif
	 iflag
	) {
		shinit = lookupvar("ENV");
		if (shinit != NULL && *shinit != '\0') {
			read_profile(shinit);
		}
	}
 state3:
	state = 4;
	if (minusc) {
		/* evalstring pushes parsefile stack.
		 * Ensure we don't falsely claim that 0 (stdin)
		 * is one of stacked source fds.
		 * Testcase: ash -c 'exec 1>&0' must not complain. */
		// if (!sflag) g_parsefile->pf_fd = -1;
		// ^^ not necessary since now we special-case fd 0
		// in is_hidden_fd() to not be considered "hidden fd"
		evalstring(minusc, 0);
	}

	if (sflag || minusc == NULL) {
#if MAX_HISTORY > 0 && ENABLE_FEATURE_EDITING_SAVEHISTORY
		if (iflag) {
			const char *hp = lookupvar("HISTFILE");
			if (hp)
				line_input_state->hist_file = hp;
# if ENABLE_FEATURE_SH_HISTFILESIZE
			hp = lookupvar("HISTFILESIZE");
			line_input_state->max_history = size_from_HISTFILESIZE(hp);
# endif
		}
#endif
 state4: /* XXX ??? - why isn't this before the "if" statement */
		cmdloop(1);
	}
#if PROFILE
	monitor(0);
#endif
#ifdef GPROF
	{
		extern void _mcleanup(void);
		_mcleanup();
	}
#endif
	TRACE(("End of main reached\n"));
	exitshell();
	/* NOTREACHED */
}


/*-
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

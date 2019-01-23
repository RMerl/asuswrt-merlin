/* vi: set sw=4 ts=4: */
/*
 * A prototype Bourne shell grammar parser.
 * Intended to follow the original Thompson and Ritchie
 * "small and simple is beautiful" philosophy, which
 * incidentally is a good match to today's BusyBox.
 *
 * Copyright (C) 2000,2001  Larry Doolittle <larry@doolittle.boa.org>
 * Copyright (C) 2008,2009  Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Credits:
 *      The parser routines proper are all original material, first
 *      written Dec 2000 and Jan 2001 by Larry Doolittle.  The
 *      execution engine, the builtins, and much of the underlying
 *      support has been adapted from busybox-0.49pre's lash, which is
 *      Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *      written by Erik Andersen <andersen@codepoet.org>.  That, in turn,
 *      is based in part on ladsh.c, by Michael K. Johnson and Erik W.
 *      Troan, which they placed in the public domain.  I don't know
 *      how much of the Johnson/Troan code has survived the repeated
 *      rewrites.
 *
 * Other credits:
 *      o_addchr derived from similar w_addchar function in glibc-2.2.
 *      parse_redirect, redirect_opt_num, and big chunks of main
 *      and many builtins derived from contributions by Erik Andersen.
 *      Miscellaneous bugfixes from Matt Kraai.
 *
 * There are two big (and related) architecture differences between
 * this parser and the lash parser.  One is that this version is
 * actually designed from the ground up to understand nearly all
 * of the Bourne grammar.  The second, consequential change is that
 * the parser and input reader have been turned inside out.  Now,
 * the parser is in control, and asks for input as needed.  The old
 * way had the input reader in control, and it asked for parsing to
 * take place as needed.  The new way makes it much easier to properly
 * handle the recursion implicit in the various substitutions, especially
 * across continuation lines.
 *
 * TODOs:
 *      grep for "TODO" and fix (some of them are easy)
 *      special variables (done: PWD, PPID, RANDOM)
 *      tilde expansion
 *      aliases
 *      follow IFS rules more precisely, including update semantics
 *      builtins mandated by standards we don't support:
 *          [un]alias, command, fc, getopts, newgrp, readonly, times
 *      make complex ${var%...} constructs support optional
 *      make here documents optional
 *
 * Bash compat TODO:
 *      redirection of stdout+stderr: &> and >&
 *      reserved words: function select
 *      advanced test: [[ ]]
 *      process substitution: <(list) and >(list)
 *      =~: regex operator
 *      let EXPR [EXPR...]
 *          Each EXPR is an arithmetic expression (ARITHMETIC EVALUATION)
 *          If the last arg evaluates to 0, let returns 1; 0 otherwise.
 *          NB: let `echo 'a=a + 1'` - error (IOW: multi-word expansion is used)
 *      ((EXPR))
 *          The EXPR is evaluated according to ARITHMETIC EVALUATION.
 *          This is exactly equivalent to let "EXPR".
 *      $[EXPR]: synonym for $((EXPR))
 *
 * Won't do:
 *      In bash, export builtin is special, its arguments are assignments
 *          and therefore expansion of them should be "one-word" expansion:
 *              $ export i=`echo 'a  b'` # export has one arg: "i=a  b"
 *          compare with:
 *              $ ls i=`echo 'a  b'`     # ls has two args: "i=a" and "b"
 *              ls: cannot access i=a: No such file or directory
 *              ls: cannot access b: No such file or directory
 *          Note1: same applies to local builtin.
 *          Note2: bash 3.2.33(1) does this only if export word itself
 *          is not quoted:
 *              $ export i=`echo 'aaa  bbb'`; echo "$i"
 *              aaa  bbb
 *              $ "export" i=`echo 'aaa  bbb'`; echo "$i"
 *              aaa
 */
#if !(defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
	|| defined(__APPLE__) \
    )
# include <malloc.h>   /* for malloc_trim */
#endif
#include <glob.h>
/* #include <dmalloc.h> */
#if ENABLE_HUSH_CASE
# include <fnmatch.h>
#endif
#include <sys/utsname.h> /* for setting $HOSTNAME */

#include "busybox.h"  /* for APPLET_IS_NOFORK/NOEXEC */
#include "unicode.h"
#include "shell_common.h"
#include "math.h"
#include "match.h"
#if ENABLE_HUSH_RANDOM_SUPPORT
# include "random.h"
#else
# define CLEAR_RANDOM_T(rnd) ((void)0)
#endif
#ifndef PIPE_BUF
# define PIPE_BUF 4096  /* amount of buffering in a pipe */
#endif

//config:config HUSH
//config:	bool "hush"
//config:	default y
//config:	help
//config:	  hush is a small shell (25k). It handles the normal flow control
//config:	  constructs such as if/then/elif/else/fi, for/in/do/done, while loops,
//config:	  case/esac. Redirections, here documents, $((arithmetic))
//config:	  and functions are supported.
//config:
//config:	  It will compile and work on no-mmu systems.
//config:
//config:	  It does not handle select, aliases, tilde expansion,
//config:	  &>file and >&file redirection of stdout+stderr.
//config:
//config:config HUSH_BASH_COMPAT
//config:	bool "bash-compatible extensions"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable bash-compatible extensions.
//config:
//config:config HUSH_BRACE_EXPANSION
//config:	bool "Brace expansion"
//config:	default y
//config:	depends on HUSH_BASH_COMPAT
//config:	help
//config:	  Enable {abc,def} extension.
//config:
//config:config HUSH_HELP
//config:	bool "help builtin"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable help builtin in hush. Code size + ~1 kbyte.
//config:
//config:config HUSH_INTERACTIVE
//config:	bool "Interactive mode"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable interactive mode (prompt and command editing).
//config:	  Without this, hush simply reads and executes commands
//config:	  from stdin just like a shell script from a file.
//config:	  No prompt, no PS1/PS2 magic shell variables.
//config:
//config:config HUSH_SAVEHISTORY
//config:	bool "Save command history to .hush_history"
//config:	default y
//config:	depends on HUSH_INTERACTIVE && FEATURE_EDITING_SAVEHISTORY
//config:	help
//config:	  Enable history saving in hush.
//config:
//config:config HUSH_JOB
//config:	bool "Job control"
//config:	default y
//config:	depends on HUSH_INTERACTIVE
//config:	help
//config:	  Enable job control: Ctrl-Z backgrounds, Ctrl-C interrupts current
//config:	  command (not entire shell), fg/bg builtins work. Without this option,
//config:	  "cmd &" still works by simply spawning a process and immediately
//config:	  prompting for next command (or executing next command in a script),
//config:	  but no separate process group is formed.
//config:
//config:config HUSH_TICK
//config:	bool "Process substitution"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable process substitution `command` and $(command) in hush.
//config:
//config:config HUSH_IF
//config:	bool "Support if/then/elif/else/fi"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable if/then/elif/else/fi in hush.
//config:
//config:config HUSH_LOOPS
//config:	bool "Support for, while and until loops"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable for, while and until loops in hush.
//config:
//config:config HUSH_CASE
//config:	bool "Support case ... esac statement"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable case ... esac statement in hush. +400 bytes.
//config:
//config:config HUSH_FUNCTIONS
//config:	bool "Support funcname() { commands; } syntax"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable support for shell functions in hush. +800 bytes.
//config:
//config:config HUSH_LOCAL
//config:	bool "Support local builtin"
//config:	default y
//config:	depends on HUSH_FUNCTIONS
//config:	help
//config:	  Enable support for local variables in functions.
//config:
//config:config HUSH_RANDOM_SUPPORT
//config:	bool "Pseudorandom generator and $RANDOM variable"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  Enable pseudorandom generator and dynamic variable "$RANDOM".
//config:	  Each read of "$RANDOM" will generate a new pseudorandom value.
//config:
//config:config HUSH_EXPORT_N
//config:	bool "Support 'export -n' option"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  export -n unexports variables. It is a bash extension.
//config:
//config:config HUSH_MODE_X
//config:	bool "Support 'hush -x' option and 'set -x' command"
//config:	default y
//config:	depends on HUSH
//config:	help
//config:	  This instructs hush to print commands before execution.
//config:	  Adds ~300 bytes.
//config:
//config:config MSH
//config:	bool "msh (deprecated: aliased to hush)"
//config:	default n
//config:	select HUSH
//config:	help
//config:	  msh is deprecated and will be removed, please migrate to hush.
//config:

//applet:IF_HUSH(APPLET(hush, BB_DIR_BIN, BB_SUID_DROP))
//applet:IF_MSH(APPLET(msh, BB_DIR_BIN, BB_SUID_DROP))
//applet:IF_FEATURE_SH_IS_HUSH(APPLET_ODDNAME(sh, hush, BB_DIR_BIN, BB_SUID_DROP, sh))
//applet:IF_FEATURE_BASH_IS_HUSH(APPLET_ODDNAME(bash, hush, BB_DIR_BIN, BB_SUID_DROP, bash))

//kbuild:lib-$(CONFIG_HUSH) += hush.o match.o shell_common.o
//kbuild:lib-$(CONFIG_HUSH_RANDOM_SUPPORT) += random.o

/* -i (interactive) and -s (read stdin) are also accepted,
 * but currently do nothing, therefore aren't shown in help.
 * NOMMU-specific options are not meant to be used by users,
 * therefore we don't show them either.
 */
//usage:#define hush_trivial_usage
//usage:	"[-nxl] [-c 'SCRIPT' [ARG0 [ARGS]] / FILE [ARGS]]"
//usage:#define hush_full_usage "\n\n"
//usage:	"Unix shell interpreter"

//usage:#define msh_trivial_usage hush_trivial_usage
//usage:#define msh_full_usage hush_full_usage

//usage:#if ENABLE_FEATURE_SH_IS_HUSH
//usage:# define sh_trivial_usage hush_trivial_usage
//usage:# define sh_full_usage    hush_full_usage
//usage:#endif
//usage:#if ENABLE_FEATURE_BASH_IS_HUSH
//usage:# define bash_trivial_usage hush_trivial_usage
//usage:# define bash_full_usage    hush_full_usage
//usage:#endif


/* Build knobs */
#define LEAK_HUNTING 0
#define BUILD_AS_NOMMU 0
/* Enable/disable sanity checks. Ok to enable in production,
 * only adds a bit of bloat. Set to >1 to get non-production level verbosity.
 * Keeping 1 for now even in released versions.
 */
#define HUSH_DEBUG 1
/* Slightly bigger (+200 bytes), but faster hush.
 * So far it only enables a trick with counting SIGCHLDs and forks,
 * which allows us to do fewer waitpid's.
 * (we can detect a case where neither forks were done nor SIGCHLDs happened
 * and therefore waitpid will return the same result as last time)
 */
#define ENABLE_HUSH_FAST 0
/* TODO: implement simplified code for users which do not need ${var%...} ops
 * So far ${var%...} ops are always enabled:
 */
#define ENABLE_HUSH_DOLLAR_OPS 1


#if BUILD_AS_NOMMU
# undef BB_MMU
# undef USE_FOR_NOMMU
# undef USE_FOR_MMU
# define BB_MMU 0
# define USE_FOR_NOMMU(...) __VA_ARGS__
# define USE_FOR_MMU(...)
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

#if !ENABLE_HUSH_INTERACTIVE
# undef ENABLE_FEATURE_EDITING
# define ENABLE_FEATURE_EDITING 0
# undef ENABLE_FEATURE_EDITING_FANCY_PROMPT
# define ENABLE_FEATURE_EDITING_FANCY_PROMPT 0
# undef ENABLE_FEATURE_EDITING_SAVE_ON_EXIT
# define ENABLE_FEATURE_EDITING_SAVE_ON_EXIT 0
#endif

/* Do we support ANY keywords? */
#if ENABLE_HUSH_IF || ENABLE_HUSH_LOOPS || ENABLE_HUSH_CASE
# define HAS_KEYWORDS 1
# define IF_HAS_KEYWORDS(...) __VA_ARGS__
# define IF_HAS_NO_KEYWORDS(...)
#else
# define HAS_KEYWORDS 0
# define IF_HAS_KEYWORDS(...)
# define IF_HAS_NO_KEYWORDS(...) __VA_ARGS__
#endif

/* If you comment out one of these below, it will be #defined later
 * to perform debug printfs to stderr: */
#define debug_printf(...)        do {} while (0)
/* Finer-grained debug switches */
#define debug_printf_parse(...)  do {} while (0)
#define debug_print_tree(a, b)   do {} while (0)
#define debug_printf_exec(...)   do {} while (0)
#define debug_printf_env(...)    do {} while (0)
#define debug_printf_jobs(...)   do {} while (0)
#define debug_printf_expand(...) do {} while (0)
#define debug_printf_varexp(...) do {} while (0)
#define debug_printf_glob(...)   do {} while (0)
#define debug_printf_list(...)   do {} while (0)
#define debug_printf_subst(...)  do {} while (0)
#define debug_printf_clean(...)  do {} while (0)

#define ERR_PTR ((void*)(long)1)

#define JOB_STATUS_FORMAT    "[%d] %-22s %.40s\n"

#define _SPECIAL_VARS_STR     "_*@$!?#"
#define SPECIAL_VARS_STR     ("_*@$!?#" + 1)
#define NUMERIC_SPECVARS_STR ("_*@$!?#" + 3)
#if ENABLE_HUSH_BASH_COMPAT
/* Support / and // replace ops */
/* Note that // is stored as \ in "encoded" string representation */
# define VAR_ENCODED_SUBST_OPS      "\\/%#:-=+?"
# define VAR_SUBST_OPS             ("\\/%#:-=+?" + 1)
# define MINUS_PLUS_EQUAL_QUESTION ("\\/%#:-=+?" + 5)
#else
# define VAR_ENCODED_SUBST_OPS      "%#:-=+?"
# define VAR_SUBST_OPS              "%#:-=+?"
# define MINUS_PLUS_EQUAL_QUESTION ("%#:-=+?" + 3)
#endif

#define SPECIAL_VAR_SYMBOL   3

struct variable;

static const char hush_version_str[] ALIGN1 = "HUSH_VERSION="BB_VER;

/* This supports saving pointers malloced in vfork child,
 * to be freed in the parent.
 */
#if !BB_MMU
typedef struct nommu_save_t {
	char **new_env;
	struct variable *old_vars;
	char **argv;
	char **argv_from_re_execing;
} nommu_save_t;
#endif

enum {
	RES_NONE  = 0,
#if ENABLE_HUSH_IF
	RES_IF    ,
	RES_THEN  ,
	RES_ELIF  ,
	RES_ELSE  ,
	RES_FI    ,
#endif
#if ENABLE_HUSH_LOOPS
	RES_FOR   ,
	RES_WHILE ,
	RES_UNTIL ,
	RES_DO    ,
	RES_DONE  ,
#endif
#if ENABLE_HUSH_LOOPS || ENABLE_HUSH_CASE
	RES_IN    ,
#endif
#if ENABLE_HUSH_CASE
	RES_CASE  ,
	/* three pseudo-keywords support contrived "case" syntax: */
	RES_CASE_IN,   /* "case ... IN", turns into RES_MATCH when IN is observed */
	RES_MATCH ,    /* "word)" */
	RES_CASE_BODY, /* "this command is inside CASE" */
	RES_ESAC  ,
#endif
	RES_XXXX  ,
	RES_SNTX
};

typedef struct o_string {
	char *data;
	int length; /* position where data is appended */
	int maxlen;
	int o_expflags;
	/* At least some part of the string was inside '' or "",
	 * possibly empty one: word"", wo''rd etc. */
	smallint has_quoted_part;
	smallint has_empty_slot;
	smallint o_assignment; /* 0:maybe, 1:yes, 2:no */
} o_string;
enum {
	EXP_FLAG_SINGLEWORD     = 0x80, /* must be 0x80 */
	EXP_FLAG_GLOB           = 0x2,
	/* Protect newly added chars against globbing
	 * by prepending \ to *, ?, [, \ */
	EXP_FLAG_ESC_GLOB_CHARS = 0x1,
};
enum {
	MAYBE_ASSIGNMENT      = 0,
	DEFINITELY_ASSIGNMENT = 1,
	NOT_ASSIGNMENT        = 2,
	/* Not an assignment, but next word may be: "if v=xyz cmd;" */
	WORD_IS_KEYWORD       = 3,
};
/* Used for initialization: o_string foo = NULL_O_STRING; */
#define NULL_O_STRING { NULL }

#ifndef debug_printf_parse
static const char *const assignment_flag[] = {
	"MAYBE_ASSIGNMENT",
	"DEFINITELY_ASSIGNMENT",
	"NOT_ASSIGNMENT",
	"WORD_IS_KEYWORD",
};
#endif

typedef struct in_str {
	const char *p;
	/* eof_flag=1: last char in ->p is really an EOF */
	char eof_flag; /* meaningless if ->p == NULL */
	char peek_buf[2];
#if ENABLE_HUSH_INTERACTIVE
	smallint promptmode; /* 0: PS1, 1: PS2 */
#endif
	int last_char;
	FILE *file;
	int (*get) (struct in_str *) FAST_FUNC;
	int (*peek) (struct in_str *) FAST_FUNC;
} in_str;
#define i_getch(input) ((input)->get(input))
#define i_peek(input) ((input)->peek(input))

/* The descrip member of this structure is only used to make
 * debugging output pretty */
static const struct {
	int mode;
	signed char default_fd;
	char descrip[3];
} redir_table[] = {
	{ O_RDONLY,                  0, "<"  },
	{ O_CREAT|O_TRUNC|O_WRONLY,  1, ">"  },
	{ O_CREAT|O_APPEND|O_WRONLY, 1, ">>" },
	{ O_CREAT|O_RDWR,            1, "<>" },
	{ O_RDONLY,                  0, "<<" },
/* Should not be needed. Bogus default_fd helps in debugging */
/*	{ O_RDONLY,                 77, "<<" }, */
};

struct redir_struct {
	struct redir_struct *next;
	char *rd_filename;          /* filename */
	int rd_fd;                  /* fd to redirect */
	/* fd to redirect to, or -3 if rd_fd is to be closed (n>&-) */
	int rd_dup;
	smallint rd_type;           /* (enum redir_type) */
	/* note: for heredocs, rd_filename contains heredoc delimiter,
	 * and subsequently heredoc itself; and rd_dup is a bitmask:
	 * bit 0: do we need to trim leading tabs?
	 * bit 1: is heredoc quoted (<<'delim' syntax) ?
	 */
};
typedef enum redir_type {
	REDIRECT_INPUT     = 0,
	REDIRECT_OVERWRITE = 1,
	REDIRECT_APPEND    = 2,
	REDIRECT_IO        = 3,
	REDIRECT_HEREDOC   = 4,
	REDIRECT_HEREDOC2  = 5, /* REDIRECT_HEREDOC after heredoc is loaded */

	REDIRFD_CLOSE      = -3,
	REDIRFD_SYNTAX_ERR = -2,
	REDIRFD_TO_FILE    = -1,
	/* otherwise, rd_fd is redirected to rd_dup */

	HEREDOC_SKIPTABS = 1,
	HEREDOC_QUOTED   = 2,
} redir_type;


struct command {
	pid_t pid;                  /* 0 if exited */
	int assignment_cnt;         /* how many argv[i] are assignments? */
	smallint cmd_type;          /* CMD_xxx */
#define CMD_NORMAL   0
#define CMD_SUBSHELL 1
#if ENABLE_HUSH_BASH_COMPAT
/* used for "[[ EXPR ]]" */
# define CMD_SINGLEWORD_NOGLOB 2
#endif
#if ENABLE_HUSH_FUNCTIONS
# define CMD_FUNCDEF 3
#endif

	smalluint cmd_exitcode;
	/* if non-NULL, this "command" is { list }, ( list ), or a compound statement */
	struct pipe *group;
#if !BB_MMU
	char *group_as_string;
#endif
#if ENABLE_HUSH_FUNCTIONS
	struct function *child_func;
/* This field is used to prevent a bug here:
 * while...do f1() {a;}; f1; f1() {b;}; f1; done
 * When we execute "f1() {a;}" cmd, we create new function and clear
 * cmd->group, cmd->group_as_string, cmd->argv[0].
 * When we execute "f1() {b;}", we notice that f1 exists,
 * and that its "parent cmd" struct is still "alive",
 * we put those fields back into cmd->xxx
 * (struct function has ->parent_cmd ptr to facilitate that).
 * When we loop back, we can execute "f1() {a;}" again and set f1 correctly.
 * Without this trick, loop would execute a;b;b;b;...
 * instead of correct sequence a;b;a;b;...
 * When command is freed, it severs the link
 * (sets ->child_func->parent_cmd to NULL).
 */
#endif
	char **argv;                /* command name and arguments */
/* argv vector may contain variable references (^Cvar^C, ^C0^C etc)
 * and on execution these are substituted with their values.
 * Substitution can make _several_ words out of one argv[n]!
 * Example: argv[0]=='.^C*^C.' here: echo .$*.
 * References of the form ^C`cmd arg^C are `cmd arg` substitutions.
 */
	struct redir_struct *redirects; /* I/O redirections */
};
/* Is there anything in this command at all? */
#define IS_NULL_CMD(cmd) \
	(!(cmd)->group && !(cmd)->argv && !(cmd)->redirects)

struct pipe {
	struct pipe *next;
	int num_cmds;               /* total number of commands in pipe */
	int alive_cmds;             /* number of commands running (not exited) */
	int stopped_cmds;           /* number of commands alive, but stopped */
#if ENABLE_HUSH_JOB
	int jobid;                  /* job number */
	pid_t pgrp;                 /* process group ID for the job */
	char *cmdtext;              /* name of job */
#endif
	struct command *cmds;       /* array of commands in pipe */
	smallint followup;          /* PIPE_BG, PIPE_SEQ, PIPE_OR, PIPE_AND */
	IF_HAS_KEYWORDS(smallint pi_inverted;) /* "! cmd | cmd" */
	IF_HAS_KEYWORDS(smallint res_word;) /* needed for if, for, while, until... */
};
typedef enum pipe_style {
	PIPE_SEQ = 1,
	PIPE_AND = 2,
	PIPE_OR  = 3,
	PIPE_BG  = 4,
} pipe_style;
/* Is there anything in this pipe at all? */
#define IS_NULL_PIPE(pi) \
	((pi)->num_cmds == 0 IF_HAS_KEYWORDS( && (pi)->res_word == RES_NONE))

/* This holds pointers to the various results of parsing */
struct parse_context {
	/* linked list of pipes */
	struct pipe *list_head;
	/* last pipe (being constructed right now) */
	struct pipe *pipe;
	/* last command in pipe (being constructed right now) */
	struct command *command;
	/* last redirect in command->redirects list */
	struct redir_struct *pending_redirect;
#if !BB_MMU
	o_string as_string;
#endif
#if HAS_KEYWORDS
	smallint ctx_res_w;
	smallint ctx_inverted; /* "! cmd | cmd" */
#if ENABLE_HUSH_CASE
	smallint ctx_dsemicolon; /* ";;" seen */
#endif
	/* bitmask of FLAG_xxx, for figuring out valid reserved words */
	int old_flag;
	/* group we are enclosed in:
	 * example: "if pipe1; pipe2; then pipe3; fi"
	 * when we see "if" or "then", we malloc and copy current context,
	 * and make ->stack point to it. then we parse pipeN.
	 * when closing "then" / fi" / whatever is found,
	 * we move list_head into ->stack->command->group,
	 * copy ->stack into current context, and delete ->stack.
	 * (parsing of { list } and ( list ) doesn't use this method)
	 */
	struct parse_context *stack;
#endif
};

/* On program start, environ points to initial environment.
 * putenv adds new pointers into it, unsetenv removes them.
 * Neither of these (de)allocates the strings.
 * setenv allocates new strings in malloc space and does putenv,
 * and thus setenv is unusable (leaky) for shell's purposes */
#define setenv(...) setenv_is_leaky_dont_use()
struct variable {
	struct variable *next;
	char *varstr;        /* points to "name=" portion */
#if ENABLE_HUSH_LOCAL
	unsigned func_nest_level;
#endif
	int max_len;         /* if > 0, name is part of initial env; else name is malloced */
	smallint flg_export; /* putenv should be done on this var */
	smallint flg_read_only;
};

enum {
	BC_BREAK = 1,
	BC_CONTINUE = 2,
};

#if ENABLE_HUSH_FUNCTIONS
struct function {
	struct function *next;
	char *name;
	struct command *parent_cmd;
	struct pipe *body;
# if !BB_MMU
	char *body_as_string;
# endif
};
#endif


/* set -/+o OPT support. (TODO: make it optional)
 * bash supports the following opts:
 * allexport       off
 * braceexpand     on
 * emacs           on
 * errexit         off
 * errtrace        off
 * functrace       off
 * hashall         on
 * histexpand      off
 * history         on
 * ignoreeof       off
 * interactive-comments    on
 * keyword         off
 * monitor         on
 * noclobber       off
 * noexec          off
 * noglob          off
 * nolog           off
 * notify          off
 * nounset         off
 * onecmd          off
 * physical        off
 * pipefail        off
 * posix           off
 * privileged      off
 * verbose         off
 * vi              off
 * xtrace          off
 */
static const char o_opt_strings[] ALIGN1 =
	"pipefail\0"
	"noexec\0"
#if ENABLE_HUSH_MODE_X
	"xtrace\0"
#endif
	;
enum {
	OPT_O_PIPEFAIL,
	OPT_O_NOEXEC,
#if ENABLE_HUSH_MODE_X
	OPT_O_XTRACE,
#endif
	NUM_OPT_O
};


/* "Globals" within this file */
/* Sorted roughly by size (smaller offsets == smaller code) */
struct globals {
	/* interactive_fd != 0 means we are an interactive shell.
	 * If we are, then saved_tty_pgrp can also be != 0, meaning
	 * that controlling tty is available. With saved_tty_pgrp == 0,
	 * job control still works, but terminal signals
	 * (^C, ^Z, ^Y, ^\) won't work at all, and background
	 * process groups can only be created with "cmd &".
	 * With saved_tty_pgrp != 0, hush will use tcsetpgrp()
	 * to give tty to the foreground process group,
	 * and will take it back when the group is stopped (^Z)
	 * or killed (^C).
	 */
#if ENABLE_HUSH_INTERACTIVE
	/* 'interactive_fd' is a fd# open to ctty, if we have one
	 * _AND_ if we decided to act interactively */
	int interactive_fd;
	const char *PS1;
	const char *PS2;
# define G_interactive_fd (G.interactive_fd)
#else
# define G_interactive_fd 0
#endif
#if ENABLE_FEATURE_EDITING
	line_input_t *line_input_state;
#endif
	pid_t root_pid;
	pid_t root_ppid;
	pid_t last_bg_pid;
#if ENABLE_HUSH_RANDOM_SUPPORT
	random_t random_gen;
#endif
#if ENABLE_HUSH_JOB
	int run_list_level;
	int last_jobid;
	pid_t saved_tty_pgrp;
	struct pipe *job_list;
# define G_saved_tty_pgrp (G.saved_tty_pgrp)
#else
# define G_saved_tty_pgrp 0
#endif
	char o_opt[NUM_OPT_O];
#if ENABLE_HUSH_MODE_X
# define G_x_mode (G.o_opt[OPT_O_XTRACE])
#else
# define G_x_mode 0
#endif
	smallint flag_SIGINT;
#if ENABLE_HUSH_LOOPS
	smallint flag_break_continue;
#endif
#if ENABLE_HUSH_FUNCTIONS
	/* 0: outside of a function (or sourced file)
	 * -1: inside of a function, ok to use return builtin
	 * 1: return is invoked, skip all till end of func
	 */
	smallint flag_return_in_progress;
#endif
	smallint exiting; /* used to prevent EXIT trap recursion */
	/* These four support $?, $#, and $1 */
	smalluint last_exitcode;
	/* are global_argv and global_argv[1..n] malloced? (note: not [0]) */
	smalluint global_args_malloced;
	/* how many non-NULL argv's we have. NB: $# + 1 */
	int global_argc;
	char **global_argv;
#if !BB_MMU
	char *argv0_for_re_execing;
#endif
#if ENABLE_HUSH_LOOPS
	unsigned depth_break_continue;
	unsigned depth_of_loop;
#endif
	const char *ifs;
	const char *cwd;
	struct variable *top_var;
	char **expanded_assignments;
#if ENABLE_HUSH_FUNCTIONS
	struct function *top_func;
# if ENABLE_HUSH_LOCAL
	struct variable **shadowed_vars_pp;
	unsigned func_nest_level;
# endif
#endif
	/* Signal and trap handling */
#if ENABLE_HUSH_FAST
	unsigned count_SIGCHLD;
	unsigned handled_SIGCHLD;
	smallint we_have_children;
#endif
	/* Which signals have non-DFL handler (even with no traps set)?
	 * Set at the start to:
	 * (SIGQUIT + maybe SPECIAL_INTERACTIVE_SIGS + maybe SPECIAL_JOBSTOP_SIGS)
	 * SPECIAL_INTERACTIVE_SIGS are cleared after fork.
	 * The rest is cleared right before execv syscalls.
	 * Other than these two times, never modified.
	 */
	unsigned special_sig_mask;
#if ENABLE_HUSH_JOB
	unsigned fatal_sig_mask;
# define G_fatal_sig_mask G.fatal_sig_mask
#else
# define G_fatal_sig_mask 0
#endif
	char **traps; /* char *traps[NSIG] */
	sigset_t pending_set;
#if HUSH_DEBUG
	unsigned long memleak_value;
	int debug_indent;
#endif
	struct sigaction sa;
	char user_input_buf[ENABLE_FEATURE_EDITING ? CONFIG_FEATURE_EDITING_MAX_LEN : 2];
};
#define G (*ptr_to_globals)
/* Not #defining name to G.name - this quickly gets unwieldy
 * (too many defines). Also, I actually prefer to see when a variable
 * is global, thus "G." prefix is a useful hint */
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	/* memset(&G.sa, 0, sizeof(G.sa)); */  \
	sigfillset(&G.sa.sa_mask); \
	G.sa.sa_flags = SA_RESTART; \
} while (0)


/* Function prototypes for builtins */
static int builtin_cd(char **argv) FAST_FUNC;
static int builtin_echo(char **argv) FAST_FUNC;
static int builtin_eval(char **argv) FAST_FUNC;
static int builtin_exec(char **argv) FAST_FUNC;
static int builtin_exit(char **argv) FAST_FUNC;
static int builtin_export(char **argv) FAST_FUNC;
#if ENABLE_HUSH_JOB
static int builtin_fg_bg(char **argv) FAST_FUNC;
static int builtin_jobs(char **argv) FAST_FUNC;
#endif
#if ENABLE_HUSH_HELP
static int builtin_help(char **argv) FAST_FUNC;
#endif
#if MAX_HISTORY && ENABLE_FEATURE_EDITING
static int builtin_history(char **argv) FAST_FUNC;
#endif
#if ENABLE_HUSH_LOCAL
static int builtin_local(char **argv) FAST_FUNC;
#endif
#if HUSH_DEBUG
static int builtin_memleak(char **argv) FAST_FUNC;
#endif
#if ENABLE_PRINTF
static int builtin_printf(char **argv) FAST_FUNC;
#endif
static int builtin_pwd(char **argv) FAST_FUNC;
static int builtin_read(char **argv) FAST_FUNC;
static int builtin_set(char **argv) FAST_FUNC;
static int builtin_shift(char **argv) FAST_FUNC;
static int builtin_source(char **argv) FAST_FUNC;
static int builtin_test(char **argv) FAST_FUNC;
static int builtin_trap(char **argv) FAST_FUNC;
static int builtin_type(char **argv) FAST_FUNC;
static int builtin_true(char **argv) FAST_FUNC;
static int builtin_umask(char **argv) FAST_FUNC;
static int builtin_unset(char **argv) FAST_FUNC;
static int builtin_wait(char **argv) FAST_FUNC;
#if ENABLE_HUSH_LOOPS
static int builtin_break(char **argv) FAST_FUNC;
static int builtin_continue(char **argv) FAST_FUNC;
#endif
#if ENABLE_HUSH_FUNCTIONS
static int builtin_return(char **argv) FAST_FUNC;
#endif

/* Table of built-in functions.  They can be forked or not, depending on
 * context: within pipes, they fork.  As simple commands, they do not.
 * When used in non-forking context, they can change global variables
 * in the parent shell process.  If forked, of course they cannot.
 * For example, 'unset foo | whatever' will parse and run, but foo will
 * still be set at the end. */
struct built_in_command {
	const char *b_cmd;
	int (*b_function)(char **argv) FAST_FUNC;
#if ENABLE_HUSH_HELP
	const char *b_descr;
# define BLTIN(cmd, func, help) { cmd, func, help }
#else
# define BLTIN(cmd, func, help) { cmd, func }
#endif
};

static const struct built_in_command bltins1[] = {
	BLTIN("."        , builtin_source  , "Run commands in a file"),
	BLTIN(":"        , builtin_true    , NULL),
#if ENABLE_HUSH_JOB
	BLTIN("bg"       , builtin_fg_bg   , "Resume a job in the background"),
#endif
#if ENABLE_HUSH_LOOPS
	BLTIN("break"    , builtin_break   , "Exit from a loop"),
#endif
	BLTIN("cd"       , builtin_cd      , "Change directory"),
#if ENABLE_HUSH_LOOPS
	BLTIN("continue" , builtin_continue, "Start new loop iteration"),
#endif
	BLTIN("eval"     , builtin_eval    , "Construct and run shell command"),
	BLTIN("exec"     , builtin_exec    , "Execute command, don't return to shell"),
	BLTIN("exit"     , builtin_exit    , "Exit"),
	BLTIN("export"   , builtin_export  , "Set environment variables"),
#if ENABLE_HUSH_JOB
	BLTIN("fg"       , builtin_fg_bg   , "Bring job into the foreground"),
#endif
#if ENABLE_HUSH_HELP
	BLTIN("help"     , builtin_help    , NULL),
#endif
#if MAX_HISTORY && ENABLE_FEATURE_EDITING
	BLTIN("history"  , builtin_history , "Show command history"),
#endif
#if ENABLE_HUSH_JOB
	BLTIN("jobs"     , builtin_jobs    , "List jobs"),
#endif
#if ENABLE_HUSH_LOCAL
	BLTIN("local"    , builtin_local   , "Set local variables"),
#endif
#if HUSH_DEBUG
	BLTIN("memleak"  , builtin_memleak , NULL),
#endif
	BLTIN("read"     , builtin_read    , "Input into variable"),
#if ENABLE_HUSH_FUNCTIONS
	BLTIN("return"   , builtin_return  , "Return from a function"),
#endif
	BLTIN("set"      , builtin_set     , "Set/unset positional parameters"),
	BLTIN("shift"    , builtin_shift   , "Shift positional parameters"),
#if ENABLE_HUSH_BASH_COMPAT
	BLTIN("source"   , builtin_source  , "Run commands in a file"),
#endif
	BLTIN("trap"     , builtin_trap    , "Trap signals"),
	BLTIN("true"     , builtin_true    , NULL),
	BLTIN("type"     , builtin_type    , "Show command type"),
	BLTIN("ulimit"   , shell_builtin_ulimit  , "Control resource limits"),
	BLTIN("umask"    , builtin_umask   , "Set file creation mask"),
	BLTIN("unset"    , builtin_unset   , "Unset variables"),
	BLTIN("wait"     , builtin_wait    , "Wait for process"),
};
/* For now, echo and test are unconditionally enabled.
 * Maybe make it configurable? */
static const struct built_in_command bltins2[] = {
	BLTIN("["        , builtin_test    , NULL),
	BLTIN("echo"     , builtin_echo    , NULL),
#if ENABLE_PRINTF
	BLTIN("printf"   , builtin_printf  , NULL),
#endif
	BLTIN("pwd"      , builtin_pwd     , NULL),
	BLTIN("test"     , builtin_test    , NULL),
};


/* Debug printouts.
 */
#if HUSH_DEBUG
/* prevent disasters with G.debug_indent < 0 */
# define indent() fdprintf(2, "%*s", (G.debug_indent * 2) & 0xff, "")
# define debug_enter() (G.debug_indent++)
# define debug_leave() (G.debug_indent--)
#else
# define indent()      ((void)0)
# define debug_enter() ((void)0)
# define debug_leave() ((void)0)
#endif

#ifndef debug_printf
# define debug_printf(...) (indent(), fdprintf(2, __VA_ARGS__))
#endif

#ifndef debug_printf_parse
# define debug_printf_parse(...) (indent(), fdprintf(2, __VA_ARGS__))
#endif

#ifndef debug_printf_exec
#define debug_printf_exec(...) (indent(), fdprintf(2, __VA_ARGS__))
#endif

#ifndef debug_printf_env
# define debug_printf_env(...) (indent(), fdprintf(2, __VA_ARGS__))
#endif

#ifndef debug_printf_jobs
# define debug_printf_jobs(...) (indent(), fdprintf(2, __VA_ARGS__))
# define DEBUG_JOBS 1
#else
# define DEBUG_JOBS 0
#endif

#ifndef debug_printf_expand
# define debug_printf_expand(...) (indent(), fdprintf(2, __VA_ARGS__))
# define DEBUG_EXPAND 1
#else
# define DEBUG_EXPAND 0
#endif

#ifndef debug_printf_varexp
# define debug_printf_varexp(...) (indent(), fdprintf(2, __VA_ARGS__))
#endif

#ifndef debug_printf_glob
# define debug_printf_glob(...) (indent(), fdprintf(2, __VA_ARGS__))
# define DEBUG_GLOB 1
#else
# define DEBUG_GLOB 0
#endif

#ifndef debug_printf_list
# define debug_printf_list(...) (indent(), fdprintf(2, __VA_ARGS__))
#endif

#ifndef debug_printf_subst
# define debug_printf_subst(...) (indent(), fdprintf(2, __VA_ARGS__))
#endif

#ifndef debug_printf_clean
# define debug_printf_clean(...) (indent(), fdprintf(2, __VA_ARGS__))
# define DEBUG_CLEAN 1
#else
# define DEBUG_CLEAN 0
#endif

#if DEBUG_EXPAND
static void debug_print_strings(const char *prefix, char **vv)
{
	indent();
	fdprintf(2, "%s:\n", prefix);
	while (*vv)
		fdprintf(2, " '%s'\n", *vv++);
}
#else
# define debug_print_strings(prefix, vv) ((void)0)
#endif


/* Leak hunting. Use hush_leaktool.sh for post-processing.
 */
#if LEAK_HUNTING
static void *xxmalloc(int lineno, size_t size)
{
	void *ptr = xmalloc((size + 0xff) & ~0xff);
	fdprintf(2, "line %d: malloc %p\n", lineno, ptr);
	return ptr;
}
static void *xxrealloc(int lineno, void *ptr, size_t size)
{
	ptr = xrealloc(ptr, (size + 0xff) & ~0xff);
	fdprintf(2, "line %d: realloc %p\n", lineno, ptr);
	return ptr;
}
static char *xxstrdup(int lineno, const char *str)
{
	char *ptr = xstrdup(str);
	fdprintf(2, "line %d: strdup %p\n", lineno, ptr);
	return ptr;
}
static void xxfree(void *ptr)
{
	fdprintf(2, "free %p\n", ptr);
	free(ptr);
}
# define xmalloc(s)     xxmalloc(__LINE__, s)
# define xrealloc(p, s) xxrealloc(__LINE__, p, s)
# define xstrdup(s)     xxstrdup(__LINE__, s)
# define free(p)        xxfree(p)
#endif


/* Syntax and runtime errors. They always abort scripts.
 * In interactive use they usually discard unparsed and/or unexecuted commands
 * and return to the prompt.
 * HUSH_DEBUG >= 2 prints line number in this file where it was detected.
 */
#if HUSH_DEBUG < 2
# define die_if_script(lineno, ...)             die_if_script(__VA_ARGS__)
# define syntax_error(lineno, msg)              syntax_error(msg)
# define syntax_error_at(lineno, msg)           syntax_error_at(msg)
# define syntax_error_unterm_ch(lineno, ch)     syntax_error_unterm_ch(ch)
# define syntax_error_unterm_str(lineno, s)     syntax_error_unterm_str(s)
# define syntax_error_unexpected_ch(lineno, ch) syntax_error_unexpected_ch(ch)
#endif

static void die_if_script(unsigned lineno, const char *fmt, ...)
{
	va_list p;

#if HUSH_DEBUG >= 2
	bb_error_msg("hush.c:%u", lineno);
#endif
	va_start(p, fmt);
	bb_verror_msg(fmt, p, NULL);
	va_end(p);
	if (!G_interactive_fd)
		xfunc_die();
}

static void syntax_error(unsigned lineno UNUSED_PARAM, const char *msg)
{
	if (msg)
		bb_error_msg("syntax error: %s", msg);
	else
		bb_error_msg("syntax error");
}

static void syntax_error_at(unsigned lineno UNUSED_PARAM, const char *msg)
{
	bb_error_msg("syntax error at '%s'", msg);
}

static void syntax_error_unterm_str(unsigned lineno UNUSED_PARAM, const char *s)
{
	bb_error_msg("syntax error: unterminated %s", s);
}

static void syntax_error_unterm_ch(unsigned lineno, char ch)
{
	char msg[2] = { ch, '\0' };
	syntax_error_unterm_str(lineno, msg);
}

static void syntax_error_unexpected_ch(unsigned lineno UNUSED_PARAM, int ch)
{
	char msg[2];
	msg[0] = ch;
	msg[1] = '\0';
	bb_error_msg("syntax error: unexpected %s", ch == EOF ? "EOF" : msg);
}

#if HUSH_DEBUG < 2
# undef die_if_script
# undef syntax_error
# undef syntax_error_at
# undef syntax_error_unterm_ch
# undef syntax_error_unterm_str
# undef syntax_error_unexpected_ch
#else
# define die_if_script(...)             die_if_script(__LINE__, __VA_ARGS__)
# define syntax_error(msg)              syntax_error(__LINE__, msg)
# define syntax_error_at(msg)           syntax_error_at(__LINE__, msg)
# define syntax_error_unterm_ch(ch)     syntax_error_unterm_ch(__LINE__, ch)
# define syntax_error_unterm_str(s)     syntax_error_unterm_str(__LINE__, s)
# define syntax_error_unexpected_ch(ch) syntax_error_unexpected_ch(__LINE__, ch)
#endif


#if ENABLE_HUSH_INTERACTIVE
static void cmdedit_update_prompt(void);
#else
# define cmdedit_update_prompt() ((void)0)
#endif


/* Utility functions
 */
/* Replace each \x with x in place, return ptr past NUL. */
static char *unbackslash(char *src)
{
	char *dst = src = strchrnul(src, '\\');
	while (1) {
		if (*src == '\\')
			src++;
		if ((*dst++ = *src++) == '\0')
			break;
	}
	return dst;
}

static char **add_strings_to_strings(char **strings, char **add, int need_to_dup)
{
	int i;
	unsigned count1;
	unsigned count2;
	char **v;

	v = strings;
	count1 = 0;
	if (v) {
		while (*v) {
			count1++;
			v++;
		}
	}
	count2 = 0;
	v = add;
	while (*v) {
		count2++;
		v++;
	}
	v = xrealloc(strings, (count1 + count2 + 1) * sizeof(char*));
	v[count1 + count2] = NULL;
	i = count2;
	while (--i >= 0)
		v[count1 + i] = (need_to_dup ? xstrdup(add[i]) : add[i]);
	return v;
}
#if LEAK_HUNTING
static char **xx_add_strings_to_strings(int lineno, char **strings, char **add, int need_to_dup)
{
	char **ptr = add_strings_to_strings(strings, add, need_to_dup);
	fdprintf(2, "line %d: add_strings_to_strings %p\n", lineno, ptr);
	return ptr;
}
#define add_strings_to_strings(strings, add, need_to_dup) \
	xx_add_strings_to_strings(__LINE__, strings, add, need_to_dup)
#endif

/* Note: takes ownership of "add" ptr (it is not strdup'ed) */
static char **add_string_to_strings(char **strings, char *add)
{
	char *v[2];
	v[0] = add;
	v[1] = NULL;
	return add_strings_to_strings(strings, v, /*dup:*/ 0);
}
#if LEAK_HUNTING
static char **xx_add_string_to_strings(int lineno, char **strings, char *add)
{
	char **ptr = add_string_to_strings(strings, add);
	fdprintf(2, "line %d: add_string_to_strings %p\n", lineno, ptr);
	return ptr;
}
#define add_string_to_strings(strings, add) \
	xx_add_string_to_strings(__LINE__, strings, add)
#endif

static void free_strings(char **strings)
{
	char **v;

	if (!strings)
		return;
	v = strings;
	while (*v) {
		free(*v);
		v++;
	}
	free(strings);
}


/* Helpers for setting new $n and restoring them back
 */
typedef struct save_arg_t {
	char *sv_argv0;
	char **sv_g_argv;
	int sv_g_argc;
	smallint sv_g_malloced;
} save_arg_t;

static void save_and_replace_G_args(save_arg_t *sv, char **argv)
{
	int n;

	sv->sv_argv0 = argv[0];
	sv->sv_g_argv = G.global_argv;
	sv->sv_g_argc = G.global_argc;
	sv->sv_g_malloced = G.global_args_malloced;

	argv[0] = G.global_argv[0]; /* retain $0 */
	G.global_argv = argv;
	G.global_args_malloced = 0;

	n = 1;
	while (*++argv)
		n++;
	G.global_argc = n;
}

static void restore_G_args(save_arg_t *sv, char **argv)
{
	char **pp;

	if (G.global_args_malloced) {
		/* someone ran "set -- arg1 arg2 ...", undo */
		pp = G.global_argv;
		while (*++pp) /* note: does not free $0 */
			free(*pp);
		free(G.global_argv);
	}
	argv[0] = sv->sv_argv0;
	G.global_argv = sv->sv_g_argv;
	G.global_argc = sv->sv_g_argc;
	G.global_args_malloced = sv->sv_g_malloced;
}


/* Basic theory of signal handling in shell
 * ========================================
 * This does not describe what hush does, rather, it is current understanding
 * what it _should_ do. If it doesn't, it's a bug.
 * http://www.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#trap
 *
 * Signals are handled only after each pipe ("cmd | cmd | cmd" thing)
 * is finished or backgrounded. It is the same in interactive and
 * non-interactive shells, and is the same regardless of whether
 * a user trap handler is installed or a shell special one is in effect.
 * ^C or ^Z from keyboard seems to execute "at once" because it usually
 * backgrounds (i.e. stops) or kills all members of currently running
 * pipe.
 *
 * Wait builtin is interruptible by signals for which user trap is set
 * or by SIGINT in interactive shell.
 *
 * Trap handlers will execute even within trap handlers. (right?)
 *
 * User trap handlers are forgotten when subshell ("(cmd)") is entered,
 * except for handlers set to '' (empty string).
 *
 * If job control is off, backgrounded commands ("cmd &")
 * have SIGINT, SIGQUIT set to SIG_IGN.
 *
 * Commands which are run in command substitution ("`cmd`")
 * have SIGTTIN, SIGTTOU, SIGTSTP set to SIG_IGN.
 *
 * Ordinary commands have signals set to SIG_IGN/DFL as inherited
 * by the shell from its parent.
 *
 * Signals which differ from SIG_DFL action
 * (note: child (i.e., [v]forked) shell is not an interactive shell):
 *
 * SIGQUIT: ignore
 * SIGTERM (interactive): ignore
 * SIGHUP (interactive):
 *    send SIGCONT to stopped jobs, send SIGHUP to all jobs and exit
 * SIGTTIN, SIGTTOU, SIGTSTP (if job control is on): ignore
 *    Note that ^Z is handled not by trapping SIGTSTP, but by seeing
 *    that all pipe members are stopped. Try this in bash:
 *    while :; do :; done - ^Z does not background it
 *    (while :; do :; done) - ^Z backgrounds it
 * SIGINT (interactive): wait for last pipe, ignore the rest
 *    of the command line, show prompt. NB: ^C does not send SIGINT
 *    to interactive shell while shell is waiting for a pipe,
 *    since shell is bg'ed (is not in foreground process group).
 *    Example 1: this waits 5 sec, but does not execute ls:
 *    "echo $$; sleep 5; ls -l" + "kill -INT <pid>"
 *    Example 2: this does not wait and does not execute ls:
 *    "echo $$; sleep 5 & wait; ls -l" + "kill -INT <pid>"
 *    Example 3: this does not wait 5 sec, but executes ls:
 *    "sleep 5; ls -l" + press ^C
 *    Example 4: this does not wait and does not execute ls:
 *    "sleep 5 & wait; ls -l" + press ^C
 *
 * (What happens to signals which are IGN on shell start?)
 * (What happens with signal mask on shell start?)
 *
 * Old implementation
 * ==================
 * We use in-kernel pending signal mask to determine which signals were sent.
 * We block all signals which we don't want to take action immediately,
 * i.e. we block all signals which need to have special handling as described
 * above, and all signals which have traps set.
 * After each pipe execution, we extract any pending signals via sigtimedwait()
 * and act on them.
 *
 * unsigned special_sig_mask: a mask of such "special" signals
 * sigset_t blocked_set:  current blocked signal set
 *
 * "trap - SIGxxx":
 *    clear bit in blocked_set unless it is also in special_sig_mask
 * "trap 'cmd' SIGxxx":
 *    set bit in blocked_set (even if 'cmd' is '')
 * after [v]fork, if we plan to be a shell:
 *    unblock signals with special interactive handling
 *    (child shell is not interactive),
 *    unset all traps except '' (note: regardless of child shell's type - {}, (), etc)
 * after [v]fork, if we plan to exec:
 *    POSIX says fork clears pending signal mask in child - no need to clear it.
 *    Restore blocked signal set to one inherited by shell just prior to exec.
 *
 * Note: as a result, we do not use signal handlers much. The only uses
 * are to count SIGCHLDs
 * and to restore tty pgrp on signal-induced exit.
 *
 * Note 2 (compat):
 * Standard says "When a subshell is entered, traps that are not being ignored
 * are set to the default actions". bash interprets it so that traps which
 * are set to '' (ignore) are NOT reset to defaults. We do the same.
 *
 * Problem: the above approach makes it unwieldy to catch signals while
 * we are in read builtin, or while we read commands from stdin:
 * masked signals are not visible!
 *
 * New implementation
 * ==================
 * We record each signal we are interested in by installing signal handler
 * for them - a bit like emulating kernel pending signal mask in userspace.
 * We are interested in: signals which need to have special handling
 * as described above, and all signals which have traps set.
 * Signals are recorded in pending_set.
 * After each pipe execution, we extract any pending signals
 * and act on them.
 *
 * unsigned special_sig_mask: a mask of shell-special signals.
 * unsigned fatal_sig_mask: a mask of signals on which we restore tty pgrp.
 * char *traps[sig] if trap for sig is set (even if it's '').
 * sigset_t pending_set: set of sigs we received.
 *
 * "trap - SIGxxx":
 *    if sig is in special_sig_mask, set handler back to:
 *        record_pending_signo, or to IGN if it's a tty stop signal
 *    if sig is in fatal_sig_mask, set handler back to sigexit.
 *    else: set handler back to SIG_DFL
 * "trap 'cmd' SIGxxx":
 *    set handler to record_pending_signo.
 * "trap '' SIGxxx":
 *    set handler to SIG_IGN.
 * after [v]fork, if we plan to be a shell:
 *    set signals with special interactive handling to SIG_DFL
 *    (because child shell is not interactive),
 *    unset all traps except '' (note: regardless of child shell's type - {}, (), etc)
 * after [v]fork, if we plan to exec:
 *    POSIX says fork clears pending signal mask in child - no need to clear it.
 *
 * To make wait builtin interruptible, we handle SIGCHLD as special signal,
 * otherwise (if we leave it SIG_DFL) sigsuspend in wait builtin will not wake up on it.
 *
 * Note (compat):
 * Standard says "When a subshell is entered, traps that are not being ignored
 * are set to the default actions". bash interprets it so that traps which
 * are set to '' (ignore) are NOT reset to defaults. We do the same.
 */
enum {
	SPECIAL_INTERACTIVE_SIGS = 0
		| (1 << SIGTERM)
		| (1 << SIGINT)
		| (1 << SIGHUP)
		,
	SPECIAL_JOBSTOP_SIGS = 0
#if ENABLE_HUSH_JOB
		| (1 << SIGTTIN)
		| (1 << SIGTTOU)
		| (1 << SIGTSTP)
#endif
		,
};

static void record_pending_signo(int sig)
{
	sigaddset(&G.pending_set, sig);
#if ENABLE_HUSH_FAST
	if (sig == SIGCHLD) {
		G.count_SIGCHLD++;
//bb_error_msg("[%d] SIGCHLD_handler: G.count_SIGCHLD:%d G.handled_SIGCHLD:%d", getpid(), G.count_SIGCHLD, G.handled_SIGCHLD);
	}
#endif
}

static sighandler_t install_sighandler(int sig, sighandler_t handler)
{
	struct sigaction old_sa;

	/* We could use signal() to install handlers... almost:
	 * except that we need to mask ALL signals while handlers run.
	 * I saw signal nesting in strace, race window isn't small.
	 * SA_RESTART is also needed, but in Linux, signal()
	 * sets SA_RESTART too.
	 */
	/* memset(&G.sa, 0, sizeof(G.sa)); - already done */
	/* sigfillset(&G.sa.sa_mask);      - already done */
	/* G.sa.sa_flags = SA_RESTART;     - already done */
	G.sa.sa_handler = handler;
	sigaction(sig, &G.sa, &old_sa);
	return old_sa.sa_handler;
}

static void hush_exit(int exitcode) NORETURN;
static void fflush_and__exit(void) NORETURN;
static void restore_ttypgrp_and__exit(void) NORETURN;

static void restore_ttypgrp_and__exit(void)
{
	/* xfunc has failed! die die die */
	/* no EXIT traps, this is an escape hatch! */
	G.exiting = 1;
	hush_exit(xfunc_error_retval);
}

/* Needed only on some libc:
 * It was observed that on exit(), fgetc'ed buffered data
 * gets "unwound" via lseek(fd, -NUM, SEEK_CUR).
 * With the net effect that even after fork(), not vfork(),
 * exit() in NOEXECed applet in "sh SCRIPT":
 *	noexec_applet_here
 *	echo END_OF_SCRIPT
 * lseeks fd in input FILE object from EOF to "e" in "echo END_OF_SCRIPT".
 * This makes "echo END_OF_SCRIPT" executed twice.
 * Similar problems can be seen with die_if_script() -> xfunc_die()
 * and in `cmd` handling.
 * If set as die_func(), this makes xfunc_die() exit via _exit(), not exit():
 */
static void fflush_and__exit(void)
{
	fflush_all();
	_exit(xfunc_error_retval);
}

#if ENABLE_HUSH_JOB

/* After [v]fork, in child: do not restore tty pgrp on xfunc death */
# define disable_restore_tty_pgrp_on_exit() (die_func = fflush_and__exit)
/* After [v]fork, in parent: restore tty pgrp on xfunc death */
# define enable_restore_tty_pgrp_on_exit()  (die_func = restore_ttypgrp_and__exit)

/* Restores tty foreground process group, and exits.
 * May be called as signal handler for fatal signal
 * (will resend signal to itself, producing correct exit state)
 * or called directly with -EXITCODE.
 * We also call it if xfunc is exiting.
 */
static void sigexit(int sig) NORETURN;
static void sigexit(int sig)
{
	/* Careful: we can end up here after [v]fork. Do not restore
	 * tty pgrp then, only top-level shell process does that */
	if (G_saved_tty_pgrp && getpid() == G.root_pid) {
		/* Disable all signals: job control, SIGPIPE, etc.
		 * Mostly paranoid measure, to prevent infinite SIGTTOU.
		 */
		sigprocmask_allsigs(SIG_BLOCK);
		tcsetpgrp(G_interactive_fd, G_saved_tty_pgrp);
	}

	/* Not a signal, just exit */
	if (sig <= 0)
		_exit(- sig);

	kill_myself_with_sig(sig); /* does not return */
}
#else

# define disable_restore_tty_pgrp_on_exit() ((void)0)
# define enable_restore_tty_pgrp_on_exit()  ((void)0)

#endif

static sighandler_t pick_sighandler(unsigned sig)
{
	sighandler_t handler = SIG_DFL;
	if (sig < sizeof(unsigned)*8) {
		unsigned sigmask = (1 << sig);

#if ENABLE_HUSH_JOB
		/* is sig fatal? */
		if (G_fatal_sig_mask & sigmask)
			handler = sigexit;
		else
#endif
		/* sig has special handling? */
		if (G.special_sig_mask & sigmask) {
			handler = record_pending_signo;
			/* TTIN/TTOU/TSTP can't be set to record_pending_signo
			 * in order to ignore them: they will be raised
			 * in an endless loop when we try to do some
			 * terminal ioctls! We do have to _ignore_ these.
			 */
			if (SPECIAL_JOBSTOP_SIGS & sigmask)
				handler = SIG_IGN;
		}
	}
	return handler;
}

/* Restores tty foreground process group, and exits. */
static void hush_exit(int exitcode)
{
#if ENABLE_FEATURE_EDITING_SAVE_ON_EXIT
	save_history(G.line_input_state);
#endif

	fflush_all();
	if (G.exiting <= 0 && G.traps && G.traps[0] && G.traps[0][0]) {
		char *argv[3];
		/* argv[0] is unused */
		argv[1] = G.traps[0];
		argv[2] = NULL;
		G.exiting = 1; /* prevent EXIT trap recursion */
		/* Note: G.traps[0] is not cleared!
		 * "trap" will still show it, if executed
		 * in the handler */
		builtin_eval(argv);
	}

#if ENABLE_FEATURE_CLEAN_UP
	{
		struct variable *cur_var;
		if (G.cwd != bb_msg_unknown)
			free((char*)G.cwd);
		cur_var = G.top_var;
		while (cur_var) {
			struct variable *tmp = cur_var;
			if (!cur_var->max_len)
				free(cur_var->varstr);
			cur_var = cur_var->next;
			free(tmp);
		}
	}
#endif

	fflush_all();
#if ENABLE_HUSH_JOB
	sigexit(- (exitcode & 0xff));
#else
	_exit(exitcode);
#endif
}


//TODO: return a mask of ALL handled sigs?
static int check_and_run_traps(void)
{
	int last_sig = 0;

	while (1) {
		int sig;

		if (sigisemptyset(&G.pending_set))
			break;
		sig = 0;
		do {
			sig++;
			if (sigismember(&G.pending_set, sig)) {
				sigdelset(&G.pending_set, sig);
				goto got_sig;
			}
		} while (sig < NSIG);
		break;
 got_sig:
		if (G.traps && G.traps[sig]) {
			if (G.traps[sig][0]) {
				/* We have user-defined handler */
				smalluint save_rcode;
				char *argv[3];
				/* argv[0] is unused */
				argv[1] = G.traps[sig];
				argv[2] = NULL;
				save_rcode = G.last_exitcode;
				builtin_eval(argv);
				G.last_exitcode = save_rcode;
				last_sig = sig;
			} /* else: "" trap, ignoring signal */
			continue;
		}
		/* not a trap: special action */
		switch (sig) {
		case SIGINT:
			/* Builtin was ^C'ed, make it look prettier: */
			bb_putchar('\n');
			G.flag_SIGINT = 1;
			last_sig = sig;
			break;
#if ENABLE_HUSH_JOB
		case SIGHUP: {
			struct pipe *job;
			/* bash is observed to signal whole process groups,
			 * not individual processes */
			for (job = G.job_list; job; job = job->next) {
				if (job->pgrp <= 0)
					continue;
				debug_printf_exec("HUPing pgrp %d\n", job->pgrp);
				if (kill(- job->pgrp, SIGHUP) == 0)
					kill(- job->pgrp, SIGCONT);
			}
			sigexit(SIGHUP);
		}
#endif
#if ENABLE_HUSH_FAST
		case SIGCHLD:
			G.count_SIGCHLD++;
//bb_error_msg("[%d] check_and_run_traps: G.count_SIGCHLD:%d G.handled_SIGCHLD:%d", getpid(), G.count_SIGCHLD, G.handled_SIGCHLD);
			/* Note:
			 * We dont do 'last_sig = sig' here -> NOT returning this sig.
			 * This simplifies wait builtin a bit.
			 */
			break;
#endif
		default: /* ignored: */
			/* SIGTERM, SIGQUIT, SIGTTIN, SIGTTOU, SIGTSTP */
			/* Note:
			 * We dont do 'last_sig = sig' here -> NOT returning this sig.
			 * Example: wait is not interrupted by TERM
			 * in interactive shell, because TERM is ignored.
			 */
			break;
		}
	}
	return last_sig;
}


static const char *get_cwd(int force)
{
	if (force || G.cwd == NULL) {
		/* xrealloc_getcwd_or_warn(arg) calls free(arg),
		 * we must not try to free(bb_msg_unknown) */
		if (G.cwd == bb_msg_unknown)
			G.cwd = NULL;
		G.cwd = xrealloc_getcwd_or_warn((char *)G.cwd);
		if (!G.cwd)
			G.cwd = bb_msg_unknown;
	}
	return G.cwd;
}


/*
 * Shell and environment variable support
 */
static struct variable **get_ptr_to_local_var(const char *name, unsigned len)
{
	struct variable **pp;
	struct variable *cur;

	pp = &G.top_var;
	while ((cur = *pp) != NULL) {
		if (strncmp(cur->varstr, name, len) == 0 && cur->varstr[len] == '=')
			return pp;
		pp = &cur->next;
	}
	return NULL;
}

static const char* FAST_FUNC get_local_var_value(const char *name)
{
	struct variable **vpp;
	unsigned len = strlen(name);

	if (G.expanded_assignments) {
		char **cpp = G.expanded_assignments;
		while (*cpp) {
			char *cp = *cpp;
			if (strncmp(cp, name, len) == 0 && cp[len] == '=')
				return cp + len + 1;
			cpp++;
		}
	}

	vpp = get_ptr_to_local_var(name, len);
	if (vpp)
		return (*vpp)->varstr + len + 1;

	if (strcmp(name, "PPID") == 0)
		return utoa(G.root_ppid);
	// bash compat: UID? EUID?
#if ENABLE_HUSH_RANDOM_SUPPORT
	if (strcmp(name, "RANDOM") == 0)
		return utoa(next_random(&G.random_gen));
#endif
	return NULL;
}

/* str holds "NAME=VAL" and is expected to be malloced.
 * We take ownership of it.
 * flg_export:
 *  0: do not change export flag
 *     (if creating new variable, flag will be 0)
 *  1: set export flag and putenv the variable
 * -1: clear export flag and unsetenv the variable
 * flg_read_only is set only when we handle -R var=val
 */
#if !BB_MMU && ENABLE_HUSH_LOCAL
/* all params are used */
#elif BB_MMU && ENABLE_HUSH_LOCAL
#define set_local_var(str, flg_export, local_lvl, flg_read_only) \
	set_local_var(str, flg_export, local_lvl)
#elif BB_MMU && !ENABLE_HUSH_LOCAL
#define set_local_var(str, flg_export, local_lvl, flg_read_only) \
	set_local_var(str, flg_export)
#elif !BB_MMU && !ENABLE_HUSH_LOCAL
#define set_local_var(str, flg_export, local_lvl, flg_read_only) \
	set_local_var(str, flg_export, flg_read_only)
#endif
static int set_local_var(char *str, int flg_export, int local_lvl, int flg_read_only)
{
	struct variable **var_pp;
	struct variable *cur;
	char *free_me = NULL;
	char *eq_sign;
	int name_len;

	eq_sign = strchr(str, '=');
	if (!eq_sign) { /* not expected to ever happen? */
		free(str);
		return -1;
	}

	name_len = eq_sign - str + 1; /* including '=' */
	var_pp = &G.top_var;
	while ((cur = *var_pp) != NULL) {
		if (strncmp(cur->varstr, str, name_len) != 0) {
			var_pp = &cur->next;
			continue;
		}

		/* We found an existing var with this name */
		if (cur->flg_read_only) {
#if !BB_MMU
			if (!flg_read_only)
#endif
				bb_error_msg("%s: readonly variable", str);
			free(str);
			return -1;
		}
		if (flg_export == -1) { // "&& cur->flg_export" ?
			debug_printf_env("%s: unsetenv '%s'\n", __func__, str);
			*eq_sign = '\0';
			unsetenv(str);
			*eq_sign = '=';
		}
#if ENABLE_HUSH_LOCAL
		if (cur->func_nest_level < local_lvl) {
			/* New variable is declared as local,
			 * and existing one is global, or local
			 * from enclosing function.
			 * Remove and save old one: */
			*var_pp = cur->next;
			cur->next = *G.shadowed_vars_pp;
			*G.shadowed_vars_pp = cur;
			/* bash 3.2.33(1) and exported vars:
			 * # export z=z
			 * # f() { local z=a; env | grep ^z; }
			 * # f
			 * z=a
			 * # env | grep ^z
			 * z=z
			 */
			if (cur->flg_export)
				flg_export = 1;
			break;
		}
#endif
		if (strcmp(cur->varstr + name_len, eq_sign + 1) == 0) {
 free_and_exp:
			free(str);
			goto exp;
		}
		if (cur->max_len != 0) {
			if (cur->max_len >= strlen(str)) {
				/* This one is from startup env, reuse space */
				strcpy(cur->varstr, str);
				goto free_and_exp;
			}
			/* Can't reuse */
			cur->max_len = 0;
			goto set_str_and_exp;
		}
		/* max_len == 0 signifies "malloced" var, which we can
		 * (and have to) free. But we can't free(cur->varstr) here:
		 * if cur->flg_export is 1, it is in the environment.
		 * We should either unsetenv+free, or wait until putenv,
		 * then putenv(new)+free(old).
		 */
		free_me = cur->varstr;
		goto set_str_and_exp;
	}

	/* Not found - create new variable struct */
	cur = xzalloc(sizeof(*cur));
#if ENABLE_HUSH_LOCAL
	cur->func_nest_level = local_lvl;
#endif
	cur->next = *var_pp;
	*var_pp = cur;

 set_str_and_exp:
	cur->varstr = str;
#if !BB_MMU
	cur->flg_read_only = flg_read_only;
#endif
 exp:
	if (flg_export == 1)
		cur->flg_export = 1;
	if (name_len == 4 && cur->varstr[0] == 'P' && cur->varstr[1] == 'S')
		cmdedit_update_prompt();
	if (cur->flg_export) {
		if (flg_export == -1) {
			cur->flg_export = 0;
			/* unsetenv was already done */
		} else {
			int i;
			debug_printf_env("%s: putenv '%s'\n", __func__, cur->varstr);
			i = putenv(cur->varstr);
			/* only now we can free old exported malloced string */
			free(free_me);
			return i;
		}
	}
	free(free_me);
	return 0;
}

/* Used at startup and after each cd */
static void set_pwd_var(int exp)
{
	set_local_var(xasprintf("PWD=%s", get_cwd(/*force:*/ 1)),
		/*exp:*/ exp, /*lvl:*/ 0, /*ro:*/ 0);
}

static int unset_local_var_len(const char *name, int name_len)
{
	struct variable *cur;
	struct variable **var_pp;

	if (!name)
		return EXIT_SUCCESS;
	var_pp = &G.top_var;
	while ((cur = *var_pp) != NULL) {
		if (strncmp(cur->varstr, name, name_len) == 0 && cur->varstr[name_len] == '=') {
			if (cur->flg_read_only) {
				bb_error_msg("%s: readonly variable", name);
				return EXIT_FAILURE;
			}
			*var_pp = cur->next;
			debug_printf_env("%s: unsetenv '%s'\n", __func__, cur->varstr);
			bb_unsetenv(cur->varstr);
			if (name_len == 3 && cur->varstr[0] == 'P' && cur->varstr[1] == 'S')
				cmdedit_update_prompt();
			if (!cur->max_len)
				free(cur->varstr);
			free(cur);
			return EXIT_SUCCESS;
		}
		var_pp = &cur->next;
	}
	return EXIT_SUCCESS;
}

static int unset_local_var(const char *name)
{
	return unset_local_var_len(name, strlen(name));
}

static void unset_vars(char **strings)
{
	char **v;

	if (!strings)
		return;
	v = strings;
	while (*v) {
		const char *eq = strchrnul(*v, '=');
		unset_local_var_len(*v, (int)(eq - *v));
		v++;
	}
	free(strings);
}

static void FAST_FUNC set_local_var_from_halves(const char *name, const char *val)
{
	char *var = xasprintf("%s=%s", name, val);
	set_local_var(var, /*flags:*/ 0, /*lvl:*/ 0, /*ro:*/ 0);
}


/*
 * Helpers for "var1=val1 var2=val2 cmd" feature
 */
static void add_vars(struct variable *var)
{
	struct variable *next;

	while (var) {
		next = var->next;
		var->next = G.top_var;
		G.top_var = var;
		if (var->flg_export) {
			debug_printf_env("%s: restoring exported '%s'\n", __func__, var->varstr);
			putenv(var->varstr);
		} else {
			debug_printf_env("%s: restoring variable '%s'\n", __func__, var->varstr);
		}
		var = next;
	}
}

static struct variable *set_vars_and_save_old(char **strings)
{
	char **s;
	struct variable *old = NULL;

	if (!strings)
		return old;
	s = strings;
	while (*s) {
		struct variable *var_p;
		struct variable **var_pp;
		char *eq;

		eq = strchr(*s, '=');
		if (eq) {
			var_pp = get_ptr_to_local_var(*s, eq - *s);
			if (var_pp) {
				/* Remove variable from global linked list */
				var_p = *var_pp;
				debug_printf_env("%s: removing '%s'\n", __func__, var_p->varstr);
				*var_pp = var_p->next;
				/* Add it to returned list */
				var_p->next = old;
				old = var_p;
			}
			set_local_var(*s, /*exp:*/ 1, /*lvl:*/ 0, /*ro:*/ 0);
		}
		s++;
	}
	return old;
}


/*
 * Unicode helper
 */
static void reinit_unicode_for_hush(void)
{
	/* Unicode support should be activated even if LANG is set
	 * _during_ shell execution, not only if it was set when
	 * shell was started. Therefore, re-check LANG every time:
	 */
	if (ENABLE_FEATURE_CHECK_UNICODE_IN_ENV
	 || ENABLE_UNICODE_USING_LOCALE
        ) {
		const char *s = get_local_var_value("LC_ALL");
		if (!s) s = get_local_var_value("LC_CTYPE");
		if (!s) s = get_local_var_value("LANG");
		reinit_unicode(s);
	}
}


/*
 * in_str support
 */
static int FAST_FUNC static_get(struct in_str *i)
{
	int ch = *i->p;
	if (ch != '\0') {
		i->p++;
		i->last_char = ch;
		return ch;
	}
	return EOF;
}

static int FAST_FUNC static_peek(struct in_str *i)
{
	return *i->p;
}

#if ENABLE_HUSH_INTERACTIVE

static void cmdedit_update_prompt(void)
{
	if (ENABLE_FEATURE_EDITING_FANCY_PROMPT) {
		G.PS1 = get_local_var_value("PS1");
		if (G.PS1 == NULL)
			G.PS1 = "\\w \\$ ";
		G.PS2 = get_local_var_value("PS2");
	} else {
		G.PS1 = NULL;
	}
	if (G.PS2 == NULL)
		G.PS2 = "> ";
}

static const char *setup_prompt_string(int promptmode)
{
	const char *prompt_str;
	debug_printf("setup_prompt_string %d ", promptmode);
	if (!ENABLE_FEATURE_EDITING_FANCY_PROMPT) {
		/* Set up the prompt */
		if (promptmode == 0) { /* PS1 */
			free((char*)G.PS1);
			/* bash uses $PWD value, even if it is set by user.
			 * It uses current dir only if PWD is unset.
			 * We always use current dir. */
			G.PS1 = xasprintf("%s %c ", get_cwd(0), (geteuid() != 0) ? '$' : '#');
			prompt_str = G.PS1;
		} else
			prompt_str = G.PS2;
	} else
		prompt_str = (promptmode == 0) ? G.PS1 : G.PS2;
	debug_printf("result '%s'\n", prompt_str);
	return prompt_str;
}

static void get_user_input(struct in_str *i)
{
	int r;
	const char *prompt_str;

	prompt_str = setup_prompt_string(i->promptmode);
# if ENABLE_FEATURE_EDITING
	/* Enable command line editing only while a command line
	 * is actually being read */
	do {
		reinit_unicode_for_hush();
		G.flag_SIGINT = 0;
		/* buglet: SIGINT will not make new prompt to appear _at once_,
		 * only after <Enter>. (^C will work) */
		r = read_line_input(G.line_input_state, prompt_str, G.user_input_buf, CONFIG_FEATURE_EDITING_MAX_LEN-1, /*timeout*/ -1);
		/* catch *SIGINT* etc (^C is handled by read_line_input) */
		check_and_run_traps();
	} while (r == 0 || G.flag_SIGINT); /* repeat if ^C or SIGINT */
	i->eof_flag = (r < 0);
	if (i->eof_flag) { /* EOF/error detected */
		G.user_input_buf[0] = EOF; /* yes, it will be truncated, it's ok */
		G.user_input_buf[1] = '\0';
	}
# else
	do {
		G.flag_SIGINT = 0;
		if (i->last_char == '\0' || i->last_char == '\n') {
			/* Why check_and_run_traps here? Try this interactively:
			 * $ trap 'echo INT' INT; (sleep 2; kill -INT $$) &
			 * $ <[enter], repeatedly...>
			 * Without check_and_run_traps, handler never runs.
			 */
			check_and_run_traps();
			fputs(prompt_str, stdout);
		}
		fflush_all();
		G.user_input_buf[0] = r = fgetc(i->file);
		/*G.user_input_buf[1] = '\0'; - already is and never changed */
	} while (G.flag_SIGINT);
	i->eof_flag = (r == EOF);
# endif
	i->p = G.user_input_buf;
}

#endif  /* INTERACTIVE */

/* This is the magic location that prints prompts
 * and gets data back from the user */
static int FAST_FUNC file_get(struct in_str *i)
{
	int ch;

	/* If there is data waiting, eat it up */
	if (i->p && *i->p) {
#if ENABLE_HUSH_INTERACTIVE
 take_cached:
#endif
		ch = *i->p++;
		if (i->eof_flag && !*i->p)
			ch = EOF;
		/* note: ch is never NUL */
	} else {
		/* need to double check i->file because we might be doing something
		 * more complicated by now, like sourcing or substituting. */
#if ENABLE_HUSH_INTERACTIVE
		if (G_interactive_fd && i->file == stdin) {
			do {
				get_user_input(i);
			} while (!*i->p); /* need non-empty line */
			i->promptmode = 1; /* PS2 */
			goto take_cached;
		}
#endif
		do ch = fgetc(i->file); while (ch == '\0');
	}
	debug_printf("file_get: got '%c' %d\n", ch, ch);
	i->last_char = ch;
	return ch;
}

/* All callers guarantee this routine will never
 * be used right after a newline, so prompting is not needed.
 */
static int FAST_FUNC file_peek(struct in_str *i)
{
	int ch;
	if (i->p && *i->p) {
		if (i->eof_flag && !i->p[1])
			return EOF;
		return *i->p;
		/* note: ch is never NUL */
	}
	do ch = fgetc(i->file); while (ch == '\0');
	i->eof_flag = (ch == EOF);
	i->peek_buf[0] = ch;
	i->peek_buf[1] = '\0';
	i->p = i->peek_buf;
	debug_printf("file_peek: got '%c' %d\n", ch, ch);
	return ch;
}

static void setup_file_in_str(struct in_str *i, FILE *f)
{
	memset(i, 0, sizeof(*i));
	i->peek = file_peek;
	i->get = file_get;
	/* i->promptmode = 0; - PS1 (memset did it) */
	i->file = f;
	/* i->p = NULL; */
}

static void setup_string_in_str(struct in_str *i, const char *s)
{
	memset(i, 0, sizeof(*i));
	i->peek = static_peek;
	i->get = static_get;
	/* i->promptmode = 0; - PS1 (memset did it) */
	i->p = s;
	/* i->eof_flag = 0; */
}


/*
 * o_string support
 */
#define B_CHUNK  (32 * sizeof(char*))

static void o_reset_to_empty_unquoted(o_string *o)
{
	o->length = 0;
	o->has_quoted_part = 0;
	if (o->data)
		o->data[0] = '\0';
}

static void o_free(o_string *o)
{
	free(o->data);
	memset(o, 0, sizeof(*o));
}

static ALWAYS_INLINE void o_free_unsafe(o_string *o)
{
	free(o->data);
}

static void o_grow_by(o_string *o, int len)
{
	if (o->length + len > o->maxlen) {
		o->maxlen += (2*len > B_CHUNK ? 2*len : B_CHUNK);
		o->data = xrealloc(o->data, 1 + o->maxlen);
	}
}

static void o_addchr(o_string *o, int ch)
{
	debug_printf("o_addchr: '%c' o->length=%d o=%p\n", ch, o->length, o);
	o_grow_by(o, 1);
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
}

static void o_addblock(o_string *o, const char *str, int len)
{
	o_grow_by(o, len);
	memcpy(&o->data[o->length], str, len);
	o->length += len;
	o->data[o->length] = '\0';
}

static void o_addstr(o_string *o, const char *str)
{
	o_addblock(o, str, strlen(str));
}

#if !BB_MMU
static void nommu_addchr(o_string *o, int ch)
{
	if (o)
		o_addchr(o, ch);
}
#else
# define nommu_addchr(o, str) ((void)0)
#endif

static void o_addstr_with_NUL(o_string *o, const char *str)
{
	o_addblock(o, str, strlen(str) + 1);
}

/*
 * HUSH_BRACE_EXPANSION code needs corresponding quoting on variable expansion side.
 * Currently, "v='{q,w}'; echo $v" erroneously expands braces in $v.
 * Apparently, on unquoted $v bash still does globbing
 * ("v='*.txt'; echo $v" prints all .txt files),
 * but NOT brace expansion! Thus, there should be TWO independent
 * quoting mechanisms on $v expansion side: one protects
 * $v from brace expansion, and other additionally protects "$v" against globbing.
 * We have only second one.
 */

#if ENABLE_HUSH_BRACE_EXPANSION
# define MAYBE_BRACES "{}"
#else
# define MAYBE_BRACES ""
#endif

/* My analysis of quoting semantics tells me that state information
 * is associated with a destination, not a source.
 */
static void o_addqchr(o_string *o, int ch)
{
	int sz = 1;
	char *found = strchr("*?[\\" MAYBE_BRACES, ch);
	if (found)
		sz++;
	o_grow_by(o, sz);
	if (found) {
		o->data[o->length] = '\\';
		o->length++;
	}
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
}

static void o_addQchr(o_string *o, int ch)
{
	int sz = 1;
	if ((o->o_expflags & EXP_FLAG_ESC_GLOB_CHARS)
	 && strchr("*?[\\" MAYBE_BRACES, ch)
	) {
		sz++;
		o->data[o->length] = '\\';
		o->length++;
	}
	o_grow_by(o, sz);
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
}

static void o_addqblock(o_string *o, const char *str, int len)
{
	while (len) {
		char ch;
		int sz;
		int ordinary_cnt = strcspn(str, "*?[\\" MAYBE_BRACES);
		if (ordinary_cnt > len) /* paranoia */
			ordinary_cnt = len;
		o_addblock(o, str, ordinary_cnt);
		if (ordinary_cnt == len)
			return; /* NUL is already added by o_addblock */
		str += ordinary_cnt;
		len -= ordinary_cnt + 1; /* we are processing + 1 char below */

		ch = *str++;
		sz = 1;
		if (ch) { /* it is necessarily one of "*?[\\" MAYBE_BRACES */
			sz++;
			o->data[o->length] = '\\';
			o->length++;
		}
		o_grow_by(o, sz);
		o->data[o->length] = ch;
		o->length++;
	}
	o->data[o->length] = '\0';
}

static void o_addQblock(o_string *o, const char *str, int len)
{
	if (!(o->o_expflags & EXP_FLAG_ESC_GLOB_CHARS)) {
		o_addblock(o, str, len);
		return;
	}
	o_addqblock(o, str, len);
}

static void o_addQstr(o_string *o, const char *str)
{
	o_addQblock(o, str, strlen(str));
}

/* A special kind of o_string for $VAR and `cmd` expansion.
 * It contains char* list[] at the beginning, which is grown in 16 element
 * increments. Actual string data starts at the next multiple of 16 * (char*).
 * list[i] contains an INDEX (int!) into this string data.
 * It means that if list[] needs to grow, data needs to be moved higher up
 * but list[i]'s need not be modified.
 * NB: remembering how many list[i]'s you have there is crucial.
 * o_finalize_list() operation post-processes this structure - calculates
 * and stores actual char* ptrs in list[]. Oh, it NULL terminates it as well.
 */
#if DEBUG_EXPAND || DEBUG_GLOB
static void debug_print_list(const char *prefix, o_string *o, int n)
{
	char **list = (char**)o->data;
	int string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);
	int i = 0;

	indent();
	fdprintf(2, "%s: list:%p n:%d string_start:%d length:%d maxlen:%d glob:%d quoted:%d escape:%d\n",
			prefix, list, n, string_start, o->length, o->maxlen,
			!!(o->o_expflags & EXP_FLAG_GLOB),
			o->has_quoted_part,
			!!(o->o_expflags & EXP_FLAG_ESC_GLOB_CHARS));
	while (i < n) {
		indent();
		fdprintf(2, " list[%d]=%d '%s' %p\n", i, (int)(uintptr_t)list[i],
				o->data + (int)(uintptr_t)list[i] + string_start,
				o->data + (int)(uintptr_t)list[i] + string_start);
		i++;
	}
	if (n) {
		const char *p = o->data + (int)(uintptr_t)list[n - 1] + string_start;
		indent();
		fdprintf(2, " total_sz:%ld\n", (long)((p + strlen(p) + 1) - o->data));
	}
}
#else
# define debug_print_list(prefix, o, n) ((void)0)
#endif

/* n = o_save_ptr_helper(str, n) "starts new string" by storing an index value
 * in list[n] so that it points past last stored byte so far.
 * It returns n+1. */
static int o_save_ptr_helper(o_string *o, int n)
{
	char **list = (char**)o->data;
	int string_start;
	int string_len;

	if (!o->has_empty_slot) {
		string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);
		string_len = o->length - string_start;
		if (!(n & 0xf)) { /* 0, 0x10, 0x20...? */
			debug_printf_list("list[%d]=%d string_start=%d (growing)\n", n, string_len, string_start);
			/* list[n] points to string_start, make space for 16 more pointers */
			o->maxlen += 0x10 * sizeof(list[0]);
			o->data = xrealloc(o->data, o->maxlen + 1);
			list = (char**)o->data;
			memmove(list + n + 0x10, list + n, string_len);
			o->length += 0x10 * sizeof(list[0]);
		} else {
			debug_printf_list("list[%d]=%d string_start=%d\n",
					n, string_len, string_start);
		}
	} else {
		/* We have empty slot at list[n], reuse without growth */
		string_start = ((n+1 + 0xf) & ~0xf) * sizeof(list[0]); /* NB: n+1! */
		string_len = o->length - string_start;
		debug_printf_list("list[%d]=%d string_start=%d (empty slot)\n",
				n, string_len, string_start);
		o->has_empty_slot = 0;
	}
	o->has_quoted_part = 0;
	list[n] = (char*)(uintptr_t)string_len;
	return n + 1;
}

/* "What was our last o_save_ptr'ed position (byte offset relative o->data)?" */
static int o_get_last_ptr(o_string *o, int n)
{
	char **list = (char**)o->data;
	int string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);

	return ((int)(uintptr_t)list[n-1]) + string_start;
}

#if ENABLE_HUSH_BRACE_EXPANSION
/* There in a GNU extension, GLOB_BRACE, but it is not usable:
 * first, it processes even {a} (no commas), second,
 * I didn't manage to make it return strings when they don't match
 * existing files. Need to re-implement it.
 */

/* Helper */
static int glob_needed(const char *s)
{
	while (*s) {
		if (*s == '\\') {
			if (!s[1])
				return 0;
			s += 2;
			continue;
		}
		if (*s == '*' || *s == '[' || *s == '?' || *s == '{')
			return 1;
		s++;
	}
	return 0;
}
/* Return pointer to next closing brace or to comma */
static const char *next_brace_sub(const char *cp)
{
	unsigned depth = 0;
	cp++;
	while (*cp != '\0') {
		if (*cp == '\\') {
			if (*++cp == '\0')
				break;
			cp++;
			continue;
		}
		if ((*cp == '}' && depth-- == 0) || (*cp == ',' && depth == 0))
			break;
		if (*cp++ == '{')
			depth++;
	}

	return *cp != '\0' ? cp : NULL;
}
/* Recursive brace globber. Note: may garble pattern[]. */
static int glob_brace(char *pattern, o_string *o, int n)
{
	char *new_pattern_buf;
	const char *begin;
	const char *next;
	const char *rest;
	const char *p;
	size_t rest_len;

	debug_printf_glob("glob_brace('%s')\n", pattern);

	begin = pattern;
	while (1) {
		if (*begin == '\0')
			goto simple_glob;
		if (*begin == '{') {
			/* Find the first sub-pattern and at the same time
			 * find the rest after the closing brace */
			next = next_brace_sub(begin);
			if (next == NULL) {
				/* An illegal expression */
				goto simple_glob;
			}
			if (*next == '}') {
				/* "{abc}" with no commas - illegal
				 * brace expr, disregard and skip it */
				begin = next + 1;
				continue;
			}
			break;
		}
		if (*begin == '\\' && begin[1] != '\0')
			begin++;
		begin++;
	}
	debug_printf_glob("begin:%s\n", begin);
	debug_printf_glob("next:%s\n", next);

	/* Now find the end of the whole brace expression */
	rest = next;
	while (*rest != '}') {
		rest = next_brace_sub(rest);
		if (rest == NULL) {
			/* An illegal expression */
			goto simple_glob;
		}
		debug_printf_glob("rest:%s\n", rest);
	}
	rest_len = strlen(++rest) + 1;

	/* We are sure the brace expression is well-formed */

	/* Allocate working buffer large enough for our work */
	new_pattern_buf = xmalloc(strlen(pattern));

	/* We have a brace expression.  BEGIN points to the opening {,
	 * NEXT points past the terminator of the first element, and REST
	 * points past the final }.  We will accumulate result names from
	 * recursive runs for each brace alternative in the buffer using
	 * GLOB_APPEND.  */

	p = begin + 1;
	while (1) {
		/* Construct the new glob expression */
		memcpy(
			mempcpy(
				mempcpy(new_pattern_buf,
					/* We know the prefix for all sub-patterns */
					pattern, begin - pattern),
				p, next - p),
			rest, rest_len);

		/* Note: glob_brace() may garble new_pattern_buf[].
		 * That's why we re-copy prefix every time (1st memcpy above).
		 */
		n = glob_brace(new_pattern_buf, o, n);
		if (*next == '}') {
			/* We saw the last entry */
			break;
		}
		p = next + 1;
		next = next_brace_sub(next);
	}
	free(new_pattern_buf);
	return n;

 simple_glob:
	{
		int gr;
		glob_t globdata;

		memset(&globdata, 0, sizeof(globdata));
		gr = glob(pattern, 0, NULL, &globdata);
		debug_printf_glob("glob('%s'):%d\n", pattern, gr);
		if (gr != 0) {
			if (gr == GLOB_NOMATCH) {
				globfree(&globdata);
				/* NB: garbles parameter */
				unbackslash(pattern);
				o_addstr_with_NUL(o, pattern);
				debug_printf_glob("glob pattern '%s' is literal\n", pattern);
				return o_save_ptr_helper(o, n);
			}
			if (gr == GLOB_NOSPACE)
				bb_error_msg_and_die(bb_msg_memory_exhausted);
			/* GLOB_ABORTED? Only happens with GLOB_ERR flag,
			 * but we didn't specify it. Paranoia again. */
			bb_error_msg_and_die("glob error %d on '%s'", gr, pattern);
		}
		if (globdata.gl_pathv && globdata.gl_pathv[0]) {
			char **argv = globdata.gl_pathv;
			while (1) {
				o_addstr_with_NUL(o, *argv);
				n = o_save_ptr_helper(o, n);
				argv++;
				if (!*argv)
					break;
			}
		}
		globfree(&globdata);
	}
	return n;
}
/* Performs globbing on last list[],
 * saving each result as a new list[].
 */
static int perform_glob(o_string *o, int n)
{
	char *pattern, *copy;

	debug_printf_glob("start perform_glob: n:%d o->data:%p\n", n, o->data);
	if (!o->data)
		return o_save_ptr_helper(o, n);
	pattern = o->data + o_get_last_ptr(o, n);
	debug_printf_glob("glob pattern '%s'\n", pattern);
	if (!glob_needed(pattern)) {
		/* unbackslash last string in o in place, fix length */
		o->length = unbackslash(pattern) - o->data;
		debug_printf_glob("glob pattern '%s' is literal\n", pattern);
		return o_save_ptr_helper(o, n);
	}

	copy = xstrdup(pattern);
	/* "forget" pattern in o */
	o->length = pattern - o->data;
	n = glob_brace(copy, o, n);
	free(copy);
	if (DEBUG_GLOB)
		debug_print_list("perform_glob returning", o, n);
	return n;
}

#else /* !HUSH_BRACE_EXPANSION */

/* Helper */
static int glob_needed(const char *s)
{
	while (*s) {
		if (*s == '\\') {
			if (!s[1])
				return 0;
			s += 2;
			continue;
		}
		if (*s == '*' || *s == '[' || *s == '?')
			return 1;
		s++;
	}
	return 0;
}
/* Performs globbing on last list[],
 * saving each result as a new list[].
 */
static int perform_glob(o_string *o, int n)
{
	glob_t globdata;
	int gr;
	char *pattern;

	debug_printf_glob("start perform_glob: n:%d o->data:%p\n", n, o->data);
	if (!o->data)
		return o_save_ptr_helper(o, n);
	pattern = o->data + o_get_last_ptr(o, n);
	debug_printf_glob("glob pattern '%s'\n", pattern);
	if (!glob_needed(pattern)) {
 literal:
		/* unbackslash last string in o in place, fix length */
		o->length = unbackslash(pattern) - o->data;
		debug_printf_glob("glob pattern '%s' is literal\n", pattern);
		return o_save_ptr_helper(o, n);
	}

	memset(&globdata, 0, sizeof(globdata));
	/* Can't use GLOB_NOCHECK: it does not unescape the string.
	 * If we glob "*.\*" and don't find anything, we need
	 * to fall back to using literal "*.*", but GLOB_NOCHECK
	 * will return "*.\*"!
	 */
	gr = glob(pattern, 0, NULL, &globdata);
	debug_printf_glob("glob('%s'):%d\n", pattern, gr);
	if (gr != 0) {
		if (gr == GLOB_NOMATCH) {
			globfree(&globdata);
			goto literal;
		}
		if (gr == GLOB_NOSPACE)
			bb_error_msg_and_die(bb_msg_memory_exhausted);
		/* GLOB_ABORTED? Only happens with GLOB_ERR flag,
		 * but we didn't specify it. Paranoia again. */
		bb_error_msg_and_die("glob error %d on '%s'", gr, pattern);
	}
	if (globdata.gl_pathv && globdata.gl_pathv[0]) {
		char **argv = globdata.gl_pathv;
		/* "forget" pattern in o */
		o->length = pattern - o->data;
		while (1) {
			o_addstr_with_NUL(o, *argv);
			n = o_save_ptr_helper(o, n);
			argv++;
			if (!*argv)
				break;
		}
	}
	globfree(&globdata);
	if (DEBUG_GLOB)
		debug_print_list("perform_glob returning", o, n);
	return n;
}

#endif /* !HUSH_BRACE_EXPANSION */

/* If o->o_expflags & EXP_FLAG_GLOB, glob the string so far remembered.
 * Otherwise, just finish current list[] and start new */
static int o_save_ptr(o_string *o, int n)
{
	if (o->o_expflags & EXP_FLAG_GLOB) {
		/* If o->has_empty_slot, list[n] was already globbed
		 * (if it was requested back then when it was filled)
		 * so don't do that again! */
		if (!o->has_empty_slot)
			return perform_glob(o, n); /* o_save_ptr_helper is inside */
	}
	return o_save_ptr_helper(o, n);
}

/* "Please convert list[n] to real char* ptrs, and NULL terminate it." */
static char **o_finalize_list(o_string *o, int n)
{
	char **list;
	int string_start;

	n = o_save_ptr(o, n); /* force growth for list[n] if necessary */
	if (DEBUG_EXPAND)
		debug_print_list("finalized", o, n);
	debug_printf_expand("finalized n:%d\n", n);
	list = (char**)o->data;
	string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);
	list[--n] = NULL;
	while (n) {
		n--;
		list[n] = o->data + (int)(uintptr_t)list[n] + string_start;
	}
	return list;
}

static void free_pipe_list(struct pipe *pi);

/* Returns pi->next - next pipe in the list */
static struct pipe *free_pipe(struct pipe *pi)
{
	struct pipe *next;
	int i;

	debug_printf_clean("free_pipe (pid %d)\n", getpid());
	for (i = 0; i < pi->num_cmds; i++) {
		struct command *command;
		struct redir_struct *r, *rnext;

		command = &pi->cmds[i];
		debug_printf_clean("  command %d:\n", i);
		if (command->argv) {
			if (DEBUG_CLEAN) {
				int a;
				char **p;
				for (a = 0, p = command->argv; *p; a++, p++) {
					debug_printf_clean("   argv[%d] = %s\n", a, *p);
				}
			}
			free_strings(command->argv);
			//command->argv = NULL;
		}
		/* not "else if": on syntax error, we may have both! */
		if (command->group) {
			debug_printf_clean("   begin group (cmd_type:%d)\n",
					command->cmd_type);
			free_pipe_list(command->group);
			debug_printf_clean("   end group\n");
			//command->group = NULL;
		}
		/* else is crucial here.
		 * If group != NULL, child_func is meaningless */
#if ENABLE_HUSH_FUNCTIONS
		else if (command->child_func) {
			debug_printf_exec("cmd %p releases child func at %p\n", command, command->child_func);
			command->child_func->parent_cmd = NULL;
		}
#endif
#if !BB_MMU
		free(command->group_as_string);
		//command->group_as_string = NULL;
#endif
		for (r = command->redirects; r; r = rnext) {
			debug_printf_clean("   redirect %d%s",
					r->rd_fd, redir_table[r->rd_type].descrip);
			/* guard against the case >$FOO, where foo is unset or blank */
			if (r->rd_filename) {
				debug_printf_clean(" fname:'%s'\n", r->rd_filename);
				free(r->rd_filename);
				//r->rd_filename = NULL;
			}
			debug_printf_clean(" rd_dup:%d\n", r->rd_dup);
			rnext = r->next;
			free(r);
		}
		//command->redirects = NULL;
	}
	free(pi->cmds);   /* children are an array, they get freed all at once */
	//pi->cmds = NULL;
#if ENABLE_HUSH_JOB
	free(pi->cmdtext);
	//pi->cmdtext = NULL;
#endif

	next = pi->next;
	free(pi);
	return next;
}

static void free_pipe_list(struct pipe *pi)
{
	while (pi) {
#if HAS_KEYWORDS
		debug_printf_clean("pipe reserved word %d\n", pi->res_word);
#endif
		debug_printf_clean("pipe followup code %d\n", pi->followup);
		pi = free_pipe(pi);
	}
}


/*** Parsing routines ***/

#ifndef debug_print_tree
static void debug_print_tree(struct pipe *pi, int lvl)
{
	static const char *const PIPE[] = {
		[PIPE_SEQ] = "SEQ",
		[PIPE_AND] = "AND",
		[PIPE_OR ] = "OR" ,
		[PIPE_BG ] = "BG" ,
	};
	static const char *RES[] = {
		[RES_NONE ] = "NONE" ,
# if ENABLE_HUSH_IF
		[RES_IF   ] = "IF"   ,
		[RES_THEN ] = "THEN" ,
		[RES_ELIF ] = "ELIF" ,
		[RES_ELSE ] = "ELSE" ,
		[RES_FI   ] = "FI"   ,
# endif
# if ENABLE_HUSH_LOOPS
		[RES_FOR  ] = "FOR"  ,
		[RES_WHILE] = "WHILE",
		[RES_UNTIL] = "UNTIL",
		[RES_DO   ] = "DO"   ,
		[RES_DONE ] = "DONE" ,
# endif
# if ENABLE_HUSH_LOOPS || ENABLE_HUSH_CASE
		[RES_IN   ] = "IN"   ,
# endif
# if ENABLE_HUSH_CASE
		[RES_CASE ] = "CASE" ,
		[RES_CASE_IN ] = "CASE_IN" ,
		[RES_MATCH] = "MATCH",
		[RES_CASE_BODY] = "CASE_BODY",
		[RES_ESAC ] = "ESAC" ,
# endif
		[RES_XXXX ] = "XXXX" ,
		[RES_SNTX ] = "SNTX" ,
	};
	static const char *const CMDTYPE[] = {
		"{}",
		"()",
		"[noglob]",
# if ENABLE_HUSH_FUNCTIONS
		"func()",
# endif
	};

	int pin, prn;

	pin = 0;
	while (pi) {
		fdprintf(2, "%*spipe %d res_word=%s followup=%d %s\n", lvl*2, "",
				pin, RES[pi->res_word], pi->followup, PIPE[pi->followup]);
		prn = 0;
		while (prn < pi->num_cmds) {
			struct command *command = &pi->cmds[prn];
			char **argv = command->argv;

			fdprintf(2, "%*s cmd %d assignment_cnt:%d",
					lvl*2, "", prn,
					command->assignment_cnt);
			if (command->group) {
				fdprintf(2, " group %s: (argv=%p)%s%s\n",
						CMDTYPE[command->cmd_type],
						argv
# if !BB_MMU
						, " group_as_string:", command->group_as_string
# else
						, "", ""
# endif
				);
				debug_print_tree(command->group, lvl+1);
				prn++;
				continue;
			}
			if (argv) while (*argv) {
				fdprintf(2, " '%s'", *argv);
				argv++;
			}
			fdprintf(2, "\n");
			prn++;
		}
		pi = pi->next;
		pin++;
	}
}
#endif /* debug_print_tree */

static struct pipe *new_pipe(void)
{
	struct pipe *pi;
	pi = xzalloc(sizeof(struct pipe));
	/*pi->followup = 0; - deliberately invalid value */
	/*pi->res_word = RES_NONE; - RES_NONE is 0 anyway */
	return pi;
}

/* Command (member of a pipe) is complete, or we start a new pipe
 * if ctx->command is NULL.
 * No errors possible here.
 */
static int done_command(struct parse_context *ctx)
{
	/* The command is really already in the pipe structure, so
	 * advance the pipe counter and make a new, null command. */
	struct pipe *pi = ctx->pipe;
	struct command *command = ctx->command;

	if (command) {
		if (IS_NULL_CMD(command)) {
			debug_printf_parse("done_command: skipping null cmd, num_cmds=%d\n", pi->num_cmds);
			goto clear_and_ret;
		}
		pi->num_cmds++;
		debug_printf_parse("done_command: ++num_cmds=%d\n", pi->num_cmds);
		//debug_print_tree(ctx->list_head, 20);
	} else {
		debug_printf_parse("done_command: initializing, num_cmds=%d\n", pi->num_cmds);
	}

	/* Only real trickiness here is that the uncommitted
	 * command structure is not counted in pi->num_cmds. */
	pi->cmds = xrealloc(pi->cmds, sizeof(*pi->cmds) * (pi->num_cmds+1));
	ctx->command = command = &pi->cmds[pi->num_cmds];
 clear_and_ret:
	memset(command, 0, sizeof(*command));
	return pi->num_cmds; /* used only for 0/nonzero check */
}

static void done_pipe(struct parse_context *ctx, pipe_style type)
{
	int not_null;

	debug_printf_parse("done_pipe entered, followup %d\n", type);
	/* Close previous command */
	not_null = done_command(ctx);
	ctx->pipe->followup = type;
#if HAS_KEYWORDS
	ctx->pipe->pi_inverted = ctx->ctx_inverted;
	ctx->ctx_inverted = 0;
	ctx->pipe->res_word = ctx->ctx_res_w;
#endif

	/* Without this check, even just <enter> on command line generates
	 * tree of three NOPs (!). Which is harmless but annoying.
	 * IOW: it is safe to do it unconditionally. */
	if (not_null
#if ENABLE_HUSH_IF
	 || ctx->ctx_res_w == RES_FI
#endif
#if ENABLE_HUSH_LOOPS
	 || ctx->ctx_res_w == RES_DONE
	 || ctx->ctx_res_w == RES_FOR
	 || ctx->ctx_res_w == RES_IN
#endif
#if ENABLE_HUSH_CASE
	 || ctx->ctx_res_w == RES_ESAC
#endif
	) {
		struct pipe *new_p;
		debug_printf_parse("done_pipe: adding new pipe: "
				"not_null:%d ctx->ctx_res_w:%d\n",
				not_null, ctx->ctx_res_w);
		new_p = new_pipe();
		ctx->pipe->next = new_p;
		ctx->pipe = new_p;
		/* RES_THEN, RES_DO etc are "sticky" -
		 * they remain set for pipes inside if/while.
		 * This is used to control execution.
		 * RES_FOR and RES_IN are NOT sticky (needed to support
		 * cases where variable or value happens to match a keyword):
		 */
#if ENABLE_HUSH_LOOPS
		if (ctx->ctx_res_w == RES_FOR
		 || ctx->ctx_res_w == RES_IN)
			ctx->ctx_res_w = RES_NONE;
#endif
#if ENABLE_HUSH_CASE
		if (ctx->ctx_res_w == RES_MATCH)
			ctx->ctx_res_w = RES_CASE_BODY;
		if (ctx->ctx_res_w == RES_CASE)
			ctx->ctx_res_w = RES_CASE_IN;
#endif
		ctx->command = NULL; /* trick done_command below */
		/* Create the memory for command, roughly:
		 * ctx->pipe->cmds = new struct command;
		 * ctx->command = &ctx->pipe->cmds[0];
		 */
		done_command(ctx);
		//debug_print_tree(ctx->list_head, 10);
	}
	debug_printf_parse("done_pipe return\n");
}

static void initialize_context(struct parse_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->pipe = ctx->list_head = new_pipe();
	/* Create the memory for command, roughly:
	 * ctx->pipe->cmds = new struct command;
	 * ctx->command = &ctx->pipe->cmds[0];
	 */
	done_command(ctx);
}

/* If a reserved word is found and processed, parse context is modified
 * and 1 is returned.
 */
#if HAS_KEYWORDS
struct reserved_combo {
	char literal[6];
	unsigned char res;
	unsigned char assignment_flag;
	int flag;
};
enum {
	FLAG_END   = (1 << RES_NONE ),
# if ENABLE_HUSH_IF
	FLAG_IF    = (1 << RES_IF   ),
	FLAG_THEN  = (1 << RES_THEN ),
	FLAG_ELIF  = (1 << RES_ELIF ),
	FLAG_ELSE  = (1 << RES_ELSE ),
	FLAG_FI    = (1 << RES_FI   ),
# endif
# if ENABLE_HUSH_LOOPS
	FLAG_FOR   = (1 << RES_FOR  ),
	FLAG_WHILE = (1 << RES_WHILE),
	FLAG_UNTIL = (1 << RES_UNTIL),
	FLAG_DO    = (1 << RES_DO   ),
	FLAG_DONE  = (1 << RES_DONE ),
	FLAG_IN    = (1 << RES_IN   ),
# endif
# if ENABLE_HUSH_CASE
	FLAG_MATCH = (1 << RES_MATCH),
	FLAG_ESAC  = (1 << RES_ESAC ),
# endif
	FLAG_START = (1 << RES_XXXX ),
};

static const struct reserved_combo* match_reserved_word(o_string *word)
{
	/* Mostly a list of accepted follow-up reserved words.
	 * FLAG_END means we are done with the sequence, and are ready
	 * to turn the compound list into a command.
	 * FLAG_START means the word must start a new compound list.
	 */
	static const struct reserved_combo reserved_list[] = {
# if ENABLE_HUSH_IF
		{ "!",     RES_NONE,  NOT_ASSIGNMENT  , 0 },
		{ "if",    RES_IF,    MAYBE_ASSIGNMENT, FLAG_THEN | FLAG_START },
		{ "then",  RES_THEN,  MAYBE_ASSIGNMENT, FLAG_ELIF | FLAG_ELSE | FLAG_FI },
		{ "elif",  RES_ELIF,  MAYBE_ASSIGNMENT, FLAG_THEN },
		{ "else",  RES_ELSE,  MAYBE_ASSIGNMENT, FLAG_FI   },
		{ "fi",    RES_FI,    NOT_ASSIGNMENT  , FLAG_END  },
# endif
# if ENABLE_HUSH_LOOPS
		{ "for",   RES_FOR,   NOT_ASSIGNMENT  , FLAG_IN | FLAG_DO | FLAG_START },
		{ "while", RES_WHILE, MAYBE_ASSIGNMENT, FLAG_DO | FLAG_START },
		{ "until", RES_UNTIL, MAYBE_ASSIGNMENT, FLAG_DO | FLAG_START },
		{ "in",    RES_IN,    NOT_ASSIGNMENT  , FLAG_DO   },
		{ "do",    RES_DO,    MAYBE_ASSIGNMENT, FLAG_DONE },
		{ "done",  RES_DONE,  NOT_ASSIGNMENT  , FLAG_END  },
# endif
# if ENABLE_HUSH_CASE
		{ "case",  RES_CASE,  NOT_ASSIGNMENT  , FLAG_MATCH | FLAG_START },
		{ "esac",  RES_ESAC,  NOT_ASSIGNMENT  , FLAG_END  },
# endif
	};
	const struct reserved_combo *r;

	for (r = reserved_list; r < reserved_list + ARRAY_SIZE(reserved_list); r++) {
		if (strcmp(word->data, r->literal) == 0)
			return r;
	}
	return NULL;
}
/* Return 0: not a keyword, 1: keyword
 */
static int reserved_word(o_string *word, struct parse_context *ctx)
{
# if ENABLE_HUSH_CASE
	static const struct reserved_combo reserved_match = {
		"",        RES_MATCH, NOT_ASSIGNMENT , FLAG_MATCH | FLAG_ESAC
	};
# endif
	const struct reserved_combo *r;

	if (word->has_quoted_part)
		return 0;
	r = match_reserved_word(word);
	if (!r)
		return 0;

	debug_printf("found reserved word %s, res %d\n", r->literal, r->res);
# if ENABLE_HUSH_CASE
	if (r->res == RES_IN && ctx->ctx_res_w == RES_CASE_IN) {
		/* "case word IN ..." - IN part starts first MATCH part */
		r = &reserved_match;
	} else
# endif
	if (r->flag == 0) { /* '!' */
		if (ctx->ctx_inverted) { /* bash doesn't accept '! ! true' */
			syntax_error("! ! command");
			ctx->ctx_res_w = RES_SNTX;
		}
		ctx->ctx_inverted = 1;
		return 1;
	}
	if (r->flag & FLAG_START) {
		struct parse_context *old;

		old = xmalloc(sizeof(*old));
		debug_printf_parse("push stack %p\n", old);
		*old = *ctx;   /* physical copy */
		initialize_context(ctx);
		ctx->stack = old;
	} else if (/*ctx->ctx_res_w == RES_NONE ||*/ !(ctx->old_flag & (1 << r->res))) {
		syntax_error_at(word->data);
		ctx->ctx_res_w = RES_SNTX;
		return 1;
	} else {
		/* "{...} fi" is ok. "{...} if" is not
		 * Example:
		 * if { echo foo; } then { echo bar; } fi */
		if (ctx->command->group)
			done_pipe(ctx, PIPE_SEQ);
	}

	ctx->ctx_res_w = r->res;
	ctx->old_flag = r->flag;
	word->o_assignment = r->assignment_flag;
	debug_printf_parse("word->o_assignment='%s'\n", assignment_flag[word->o_assignment]);

	if (ctx->old_flag & FLAG_END) {
		struct parse_context *old;

		done_pipe(ctx, PIPE_SEQ);
		debug_printf_parse("pop stack %p\n", ctx->stack);
		old = ctx->stack;
		old->command->group = ctx->list_head;
		old->command->cmd_type = CMD_NORMAL;
# if !BB_MMU
		/* At this point, the compound command's string is in
		 * ctx->as_string... except for the leading keyword!
		 * Consider this example: "echo a | if true; then echo a; fi"
		 * ctx->as_string will contain "true; then echo a; fi",
		 * with "if " remaining in old->as_string!
		 */
		{
			char *str;
			int len = old->as_string.length;
			/* Concatenate halves */
			o_addstr(&old->as_string, ctx->as_string.data);
			o_free_unsafe(&ctx->as_string);
			/* Find where leading keyword starts in first half */
			str = old->as_string.data + len;
			if (str > old->as_string.data)
				str--; /* skip whitespace after keyword */
			while (str > old->as_string.data && isalpha(str[-1]))
				str--;
			/* Ugh, we're done with this horrid hack */
			old->command->group_as_string = xstrdup(str);
			debug_printf_parse("pop, remembering as:'%s'\n",
					old->command->group_as_string);
		}
# endif
		*ctx = *old;   /* physical copy */
		free(old);
	}
	return 1;
}
#endif /* HAS_KEYWORDS */

/* Word is complete, look at it and update parsing context.
 * Normal return is 0. Syntax errors return 1.
 * Note: on return, word is reset, but not o_free'd!
 */
static int done_word(o_string *word, struct parse_context *ctx)
{
	struct command *command = ctx->command;

	debug_printf_parse("done_word entered: '%s' %p\n", word->data, command);
	if (word->length == 0 && !word->has_quoted_part) {
		debug_printf_parse("done_word return 0: true null, ignored\n");
		return 0;
	}

	if (ctx->pending_redirect) {
		/* We do not glob in e.g. >*.tmp case. bash seems to glob here
		 * only if run as "bash", not "sh" */
		/* http://www.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html
		 * "2.7 Redirection
		 * ...the word that follows the redirection operator
		 * shall be subjected to tilde expansion, parameter expansion,
		 * command substitution, arithmetic expansion, and quote
		 * removal. Pathname expansion shall not be performed
		 * on the word by a non-interactive shell; an interactive
		 * shell may perform it, but shall do so only when
		 * the expansion would result in one word."
		 */
		ctx->pending_redirect->rd_filename = xstrdup(word->data);
		/* Cater for >\file case:
		 * >\a creates file a; >\\a, >"\a", >"\\a" create file \a
		 * Same with heredocs:
		 * for <<\H delim is H; <<\\H, <<"\H", <<"\\H" - \H
		 */
		if (ctx->pending_redirect->rd_type == REDIRECT_HEREDOC) {
			unbackslash(ctx->pending_redirect->rd_filename);
			/* Is it <<"HEREDOC"? */
			if (word->has_quoted_part) {
				ctx->pending_redirect->rd_dup |= HEREDOC_QUOTED;
			}
		}
		debug_printf_parse("word stored in rd_filename: '%s'\n", word->data);
		ctx->pending_redirect = NULL;
	} else {
#if HAS_KEYWORDS
# if ENABLE_HUSH_CASE
		if (ctx->ctx_dsemicolon
		 && strcmp(word->data, "esac") != 0 /* not "... pattern) cmd;; esac" */
		) {
			/* already done when ctx_dsemicolon was set to 1: */
			/* ctx->ctx_res_w = RES_MATCH; */
			ctx->ctx_dsemicolon = 0;
		} else
# endif
		if (!command->argv /* if it's the first word... */
# if ENABLE_HUSH_LOOPS
		 && ctx->ctx_res_w != RES_FOR /* ...not after FOR or IN */
		 && ctx->ctx_res_w != RES_IN
# endif
# if ENABLE_HUSH_CASE
		 && ctx->ctx_res_w != RES_CASE
# endif
		) {
			int reserved = reserved_word(word, ctx);
			debug_printf_parse("checking for reserved-ness: %d\n", reserved);
			if (reserved) {
				o_reset_to_empty_unquoted(word);
				debug_printf_parse("done_word return %d\n",
						(ctx->ctx_res_w == RES_SNTX));
				return (ctx->ctx_res_w == RES_SNTX);
			}
# if ENABLE_HUSH_BASH_COMPAT
			if (strcmp(word->data, "[[") == 0) {
				command->cmd_type = CMD_SINGLEWORD_NOGLOB;
			}
			/* fall through */
# endif
		}
#endif
		if (command->group) {
			/* "{ echo foo; } echo bar" - bad */
			syntax_error_at(word->data);
			debug_printf_parse("done_word return 1: syntax error, "
					"groups and arglists don't mix\n");
			return 1;
		}

		/* If this word wasn't an assignment, next ones definitely
		 * can't be assignments. Even if they look like ones. */
		if (word->o_assignment != DEFINITELY_ASSIGNMENT
		 && word->o_assignment != WORD_IS_KEYWORD
		) {
			word->o_assignment = NOT_ASSIGNMENT;
		} else {
			if (word->o_assignment == DEFINITELY_ASSIGNMENT) {
				command->assignment_cnt++;
				debug_printf_parse("++assignment_cnt=%d\n", command->assignment_cnt);
			}
			debug_printf_parse("word->o_assignment was:'%s'\n", assignment_flag[word->o_assignment]);
			word->o_assignment = MAYBE_ASSIGNMENT;
		}
		debug_printf_parse("word->o_assignment='%s'\n", assignment_flag[word->o_assignment]);

		if (word->has_quoted_part
		 /* optimization: and if it's ("" or '') or ($v... or `cmd`...): */
		 && (word->data[0] == '\0' || word->data[0] == SPECIAL_VAR_SYMBOL)
		 /* (otherwise it's known to be not empty and is already safe) */
		) {
			/* exclude "$@" - it can expand to no word despite "" */
			char *p = word->data;
			while (p[0] == SPECIAL_VAR_SYMBOL
			    && (p[1] & 0x7f) == '@'
			    && p[2] == SPECIAL_VAR_SYMBOL
			) {
				p += 3;
			}
		}
		command->argv = add_string_to_strings(command->argv, xstrdup(word->data));
		debug_print_strings("word appended to argv", command->argv);
	}

#if ENABLE_HUSH_LOOPS
	if (ctx->ctx_res_w == RES_FOR) {
		if (word->has_quoted_part
		 || !is_well_formed_var_name(command->argv[0], '\0')
		) {
			/* bash says just "not a valid identifier" */
			syntax_error("not a valid identifier in for");
			return 1;
		}
		/* Force FOR to have just one word (variable name) */
		/* NB: basically, this makes hush see "for v in ..."
		 * syntax as if it is "for v; in ...". FOR and IN become
		 * two pipe structs in parse tree. */
		done_pipe(ctx, PIPE_SEQ);
	}
#endif
#if ENABLE_HUSH_CASE
	/* Force CASE to have just one word */
	if (ctx->ctx_res_w == RES_CASE) {
		done_pipe(ctx, PIPE_SEQ);
	}
#endif

	o_reset_to_empty_unquoted(word);

	debug_printf_parse("done_word return 0\n");
	return 0;
}


/* Peek ahead in the input to find out if we have a "&n" construct,
 * as in "2>&1", that represents duplicating a file descriptor.
 * Return:
 * REDIRFD_CLOSE if >&- "close fd" construct is seen,
 * REDIRFD_SYNTAX_ERR if syntax error,
 * REDIRFD_TO_FILE if no & was seen,
 * or the number found.
 */
#if BB_MMU
#define parse_redir_right_fd(as_string, input) \
	parse_redir_right_fd(input)
#endif
static int parse_redir_right_fd(o_string *as_string, struct in_str *input)
{
	int ch, d, ok;

	ch = i_peek(input);
	if (ch != '&')
		return REDIRFD_TO_FILE;

	ch = i_getch(input);  /* get the & */
	nommu_addchr(as_string, ch);
	ch = i_peek(input);
	if (ch == '-') {
		ch = i_getch(input);
		nommu_addchr(as_string, ch);
		return REDIRFD_CLOSE;
	}
	d = 0;
	ok = 0;
	while (ch != EOF && isdigit(ch)) {
		d = d*10 + (ch-'0');
		ok = 1;
		ch = i_getch(input);
		nommu_addchr(as_string, ch);
		ch = i_peek(input);
	}
	if (ok) return d;

//TODO: this is the place to catch ">&file" bashism (redirect both fd 1 and 2)

	bb_error_msg("ambiguous redirect");
	return REDIRFD_SYNTAX_ERR;
}

/* Return code is 0 normal, 1 if a syntax error is detected
 */
static int parse_redirect(struct parse_context *ctx,
		int fd,
		redir_type style,
		struct in_str *input)
{
	struct command *command = ctx->command;
	struct redir_struct *redir;
	struct redir_struct **redirp;
	int dup_num;

	dup_num = REDIRFD_TO_FILE;
	if (style != REDIRECT_HEREDOC) {
		/* Check for a '>&1' type redirect */
		dup_num = parse_redir_right_fd(&ctx->as_string, input);
		if (dup_num == REDIRFD_SYNTAX_ERR)
			return 1;
	} else {
		int ch = i_peek(input);
		dup_num = (ch == '-'); /* HEREDOC_SKIPTABS bit is 1 */
		if (dup_num) { /* <<-... */
			ch = i_getch(input);
			nommu_addchr(&ctx->as_string, ch);
			ch = i_peek(input);
		}
	}

	if (style == REDIRECT_OVERWRITE && dup_num == REDIRFD_TO_FILE) {
		int ch = i_peek(input);
		if (ch == '|') {
			/* >|FILE redirect ("clobbering" >).
			 * Since we do not support "set -o noclobber" yet,
			 * >| and > are the same for now. Just eat |.
			 */
			ch = i_getch(input);
			nommu_addchr(&ctx->as_string, ch);
		}
	}

	/* Create a new redir_struct and append it to the linked list */
	redirp = &command->redirects;
	while ((redir = *redirp) != NULL) {
		redirp = &(redir->next);
	}
	*redirp = redir = xzalloc(sizeof(*redir));
	/* redir->next = NULL; */
	/* redir->rd_filename = NULL; */
	redir->rd_type = style;
	redir->rd_fd = (fd == -1) ? redir_table[style].default_fd : fd;

	debug_printf_parse("redirect type %d %s\n", redir->rd_fd,
				redir_table[style].descrip);

	redir->rd_dup = dup_num;
	if (style != REDIRECT_HEREDOC && dup_num != REDIRFD_TO_FILE) {
		/* Erik had a check here that the file descriptor in question
		 * is legit; I postpone that to "run time"
		 * A "-" representation of "close me" shows up as a -3 here */
		debug_printf_parse("duplicating redirect '%d>&%d'\n",
				redir->rd_fd, redir->rd_dup);
	} else {
		/* Set ctx->pending_redirect, so we know what to do at the
		 * end of the next parsed word. */
		ctx->pending_redirect = redir;
	}
	return 0;
}

/* If a redirect is immediately preceded by a number, that number is
 * supposed to tell which file descriptor to redirect.  This routine
 * looks for such preceding numbers.  In an ideal world this routine
 * needs to handle all the following classes of redirects...
 *     echo 2>foo     # redirects fd  2 to file "foo", nothing passed to echo
 *     echo 49>foo    # redirects fd 49 to file "foo", nothing passed to echo
 *     echo -2>foo    # redirects fd  1 to file "foo",    "-2" passed to echo
 *     echo 49x>foo   # redirects fd  1 to file "foo",   "49x" passed to echo
 *
 * http://www.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html
 * "2.7 Redirection
 * ... If n is quoted, the number shall not be recognized as part of
 * the redirection expression. For example:
 * echo \2>a
 * writes the character 2 into file a"
 * We are getting it right by setting ->has_quoted_part on any \<char>
 *
 * A -1 return means no valid number was found,
 * the caller should use the appropriate default for this redirection.
 */
static int redirect_opt_num(o_string *o)
{
	int num;

	if (o->data == NULL)
		return -1;
	num = bb_strtou(o->data, NULL, 10);
	if (errno || num < 0)
		return -1;
	o_reset_to_empty_unquoted(o);
	return num;
}

#if BB_MMU
#define fetch_till_str(as_string, input, word, skip_tabs) \
	fetch_till_str(input, word, skip_tabs)
#endif
static char *fetch_till_str(o_string *as_string,
		struct in_str *input,
		const char *word,
		int heredoc_flags)
{
	o_string heredoc = NULL_O_STRING;
	unsigned past_EOL;
	int prev = 0; /* not \ */
	int ch;

	goto jump_in;

	while (1) {
		ch = i_getch(input);
		if (ch != EOF)
			nommu_addchr(as_string, ch);
		if ((ch == '\n' || ch == EOF)
		 && ((heredoc_flags & HEREDOC_QUOTED) || prev != '\\')
		) {
			if (strcmp(heredoc.data + past_EOL, word) == 0) {
				heredoc.data[past_EOL] = '\0';
				debug_printf_parse("parsed heredoc '%s'\n", heredoc.data);
				return heredoc.data;
			}
			while (ch == '\n') {
				o_addchr(&heredoc, ch);
				prev = ch;
 jump_in:
				past_EOL = heredoc.length;
				do {
					ch = i_getch(input);
					if (ch != EOF)
						nommu_addchr(as_string, ch);
				} while ((heredoc_flags & HEREDOC_SKIPTABS) && ch == '\t');
			}
		}
		if (ch == EOF) {
			o_free_unsafe(&heredoc);
			return NULL;
		}
		o_addchr(&heredoc, ch);
		nommu_addchr(as_string, ch);
		if (prev == '\\' && ch == '\\')
			/* Correctly handle foo\\<eol> (not a line cont.) */
			prev = 0; /* not \ */
		else
			prev = ch;
	}
}

/* Look at entire parse tree for not-yet-loaded REDIRECT_HEREDOCs
 * and load them all. There should be exactly heredoc_cnt of them.
 */
static int fetch_heredocs(int heredoc_cnt, struct parse_context *ctx, struct in_str *input)
{
	struct pipe *pi = ctx->list_head;

	while (pi && heredoc_cnt) {
		int i;
		struct command *cmd = pi->cmds;

		debug_printf_parse("fetch_heredocs: num_cmds:%d cmd argv0:'%s'\n",
				pi->num_cmds,
				cmd->argv ? cmd->argv[0] : "NONE");
		for (i = 0; i < pi->num_cmds; i++) {
			struct redir_struct *redir = cmd->redirects;

			debug_printf_parse("fetch_heredocs: %d cmd argv0:'%s'\n",
					i, cmd->argv ? cmd->argv[0] : "NONE");
			while (redir) {
				if (redir->rd_type == REDIRECT_HEREDOC) {
					char *p;

					redir->rd_type = REDIRECT_HEREDOC2;
					/* redir->rd_dup is (ab)used to indicate <<- */
					p = fetch_till_str(&ctx->as_string, input,
							redir->rd_filename, redir->rd_dup);
					if (!p) {
						syntax_error("unexpected EOF in here document");
						return 1;
					}
					free(redir->rd_filename);
					redir->rd_filename = p;
					heredoc_cnt--;
				}
				redir = redir->next;
			}
			cmd++;
		}
		pi = pi->next;
	}
#if 0
	/* Should be 0. If it isn't, it's a parse error */
	if (heredoc_cnt)
		bb_error_msg_and_die("heredoc BUG 2");
#endif
	return 0;
}


static int run_list(struct pipe *pi);
#if BB_MMU
#define parse_stream(pstring, input, end_trigger) \
	parse_stream(input, end_trigger)
#endif
static struct pipe *parse_stream(char **pstring,
		struct in_str *input,
		int end_trigger);


#if !ENABLE_HUSH_FUNCTIONS
#define parse_group(dest, ctx, input, ch) \
	parse_group(ctx, input, ch)
#endif
static int parse_group(o_string *dest, struct parse_context *ctx,
	struct in_str *input, int ch)
{
	/* dest contains characters seen prior to ( or {.
	 * Typically it's empty, but for function defs,
	 * it contains function name (without '()'). */
	struct pipe *pipe_list;
	int endch;
	struct command *command = ctx->command;

	debug_printf_parse("parse_group entered\n");
#if ENABLE_HUSH_FUNCTIONS
	if (ch == '(' && !dest->has_quoted_part) {
		if (dest->length)
			if (done_word(dest, ctx))
				return 1;
		if (!command->argv)
			goto skip; /* (... */
		if (command->argv[1]) { /* word word ... (... */
			syntax_error_unexpected_ch('(');
			return 1;
		}
		/* it is "word(..." or "word (..." */
		do
			ch = i_getch(input);
		while (ch == ' ' || ch == '\t');
		if (ch != ')') {
			syntax_error_unexpected_ch(ch);
			return 1;
		}
		nommu_addchr(&ctx->as_string, ch);
		do
			ch = i_getch(input);
		while (ch == ' ' || ch == '\t' || ch == '\n');
		if (ch != '{') {
			syntax_error_unexpected_ch(ch);
			return 1;
		}
		nommu_addchr(&ctx->as_string, ch);
		command->cmd_type = CMD_FUNCDEF;
		goto skip;
	}
#endif

#if 0 /* Prevented by caller */
	if (command->argv /* word [word]{... */
	 || dest->length /* word{... */
	 || dest->has_quoted_part /* ""{... */
	) {
		syntax_error(NULL);
		debug_printf_parse("parse_group return 1: "
			"syntax error, groups and arglists don't mix\n");
		return 1;
	}
#endif

#if ENABLE_HUSH_FUNCTIONS
 skip:
#endif
	endch = '}';
	if (ch == '(') {
		endch = ')';
		command->cmd_type = CMD_SUBSHELL;
	} else {
		/* bash does not allow "{echo...", requires whitespace */
		ch = i_getch(input);
		if (ch != ' ' && ch != '\t' && ch != '\n') {
			syntax_error_unexpected_ch(ch);
			return 1;
		}
		nommu_addchr(&ctx->as_string, ch);
	}

	{
#if BB_MMU
# define as_string NULL
#else
		char *as_string = NULL;
#endif
		pipe_list = parse_stream(&as_string, input, endch);
#if !BB_MMU
		if (as_string)
			o_addstr(&ctx->as_string, as_string);
#endif
		/* empty ()/{} or parse error? */
		if (!pipe_list || pipe_list == ERR_PTR) {
			/* parse_stream already emitted error msg */
			if (!BB_MMU)
				free(as_string);
			debug_printf_parse("parse_group return 1: "
				"parse_stream returned %p\n", pipe_list);
			return 1;
		}
		command->group = pipe_list;
#if !BB_MMU
		as_string[strlen(as_string) - 1] = '\0'; /* plink ')' or '}' */
		command->group_as_string = as_string;
		debug_printf_parse("end of group, remembering as:'%s'\n",
				command->group_as_string);
#endif
#undef as_string
	}
	debug_printf_parse("parse_group return 0\n");
	return 0;
	/* command remains "open", available for possible redirects */
}

#if ENABLE_HUSH_TICK || ENABLE_SH_MATH_SUPPORT || ENABLE_HUSH_DOLLAR_OPS
/* Subroutines for copying $(...) and `...` things */
static int add_till_backquote(o_string *dest, struct in_str *input, int in_dquote);
/* '...' */
static int add_till_single_quote(o_string *dest, struct in_str *input)
{
	while (1) {
		int ch = i_getch(input);
		if (ch == EOF) {
			syntax_error_unterm_ch('\'');
			return 0;
		}
		if (ch == '\'')
			return 1;
		o_addchr(dest, ch);
	}
}
/* "...\"...`..`...." - do we need to handle "...$(..)..." too? */
static int add_till_double_quote(o_string *dest, struct in_str *input)
{
	while (1) {
		int ch = i_getch(input);
		if (ch == EOF) {
			syntax_error_unterm_ch('"');
			return 0;
		}
		if (ch == '"')
			return 1;
		if (ch == '\\') {  /* \x. Copy both chars. */
			o_addchr(dest, ch);
			ch = i_getch(input);
		}
		o_addchr(dest, ch);
		if (ch == '`') {
			if (!add_till_backquote(dest, input, /*in_dquote:*/ 1))
				return 0;
			o_addchr(dest, ch);
			continue;
		}
		//if (ch == '$') ...
	}
}
/* Process `cmd` - copy contents until "`" is seen. Complicated by
 * \` quoting.
 * "Within the backquoted style of command substitution, backslash
 * shall retain its literal meaning, except when followed by: '$', '`', or '\'.
 * The search for the matching backquote shall be satisfied by the first
 * backquote found without a preceding backslash; during this search,
 * if a non-escaped backquote is encountered within a shell comment,
 * a here-document, an embedded command substitution of the $(command)
 * form, or a quoted string, undefined results occur. A single-quoted
 * or double-quoted string that begins, but does not end, within the
 * "`...`" sequence produces undefined results."
 * Example                               Output
 * echo `echo '\'TEST\`echo ZZ\`BEST`    \TESTZZBEST
 */
static int add_till_backquote(o_string *dest, struct in_str *input, int in_dquote)
{
	while (1) {
		int ch = i_getch(input);
		if (ch == '`')
			return 1;
		if (ch == '\\') {
			/* \x. Copy both unless it is \`, \$, \\ and maybe \" */
			ch = i_getch(input);
			if (ch != '`'
			 && ch != '$'
			 && ch != '\\'
			 && (!in_dquote || ch != '"')
			) {
				o_addchr(dest, '\\');
			}
		}
		if (ch == EOF) {
			syntax_error_unterm_ch('`');
			return 0;
		}
		o_addchr(dest, ch);
	}
}
/* Process $(cmd) - copy contents until ")" is seen. Complicated by
 * quoting and nested ()s.
 * "With the $(command) style of command substitution, all characters
 * following the open parenthesis to the matching closing parenthesis
 * constitute the command. Any valid shell script can be used for command,
 * except a script consisting solely of redirections which produces
 * unspecified results."
 * Example                              Output
 * echo $(echo '(TEST)' BEST)           (TEST) BEST
 * echo $(echo 'TEST)' BEST)            TEST) BEST
 * echo $(echo \(\(TEST\) BEST)         ((TEST) BEST
 *
 * Also adapted to eat ${var%...} and $((...)) constructs, since ... part
 * can contain arbitrary constructs, just like $(cmd).
 * In bash compat mode, it needs to also be able to stop on ':' or '/'
 * for ${var:N[:M]} and ${var/P[/R]} parsing.
 */
#define DOUBLE_CLOSE_CHAR_FLAG 0x80
static int add_till_closing_bracket(o_string *dest, struct in_str *input, unsigned end_ch)
{
	int ch;
	char dbl = end_ch & DOUBLE_CLOSE_CHAR_FLAG;
# if ENABLE_HUSH_BASH_COMPAT
	char end_char2 = end_ch >> 8;
# endif
	end_ch &= (DOUBLE_CLOSE_CHAR_FLAG - 1);

	while (1) {
		ch = i_getch(input);
		if (ch == EOF) {
			syntax_error_unterm_ch(end_ch);
			return 0;
		}
		if (ch == end_ch  IF_HUSH_BASH_COMPAT( || ch == end_char2)) {
			if (!dbl)
				break;
			/* we look for closing )) of $((EXPR)) */
			if (i_peek(input) == end_ch) {
				i_getch(input); /* eat second ')' */
				break;
			}
		}
		o_addchr(dest, ch);
		if (ch == '(' || ch == '{') {
			ch = (ch == '(' ? ')' : '}');
			if (!add_till_closing_bracket(dest, input, ch))
				return 0;
			o_addchr(dest, ch);
			continue;
		}
		if (ch == '\'') {
			if (!add_till_single_quote(dest, input))
				return 0;
			o_addchr(dest, ch);
			continue;
		}
		if (ch == '"') {
			if (!add_till_double_quote(dest, input))
				return 0;
			o_addchr(dest, ch);
			continue;
		}
		if (ch == '`') {
			if (!add_till_backquote(dest, input, /*in_dquote:*/ 0))
				return 0;
			o_addchr(dest, ch);
			continue;
		}
		if (ch == '\\') {
			/* \x. Copy verbatim. Important for  \(, \) */
			ch = i_getch(input);
			if (ch == EOF) {
				syntax_error_unterm_ch(')');
				return 0;
			}
			o_addchr(dest, ch);
			continue;
		}
	}
	return ch;
}
#endif /* ENABLE_HUSH_TICK || ENABLE_SH_MATH_SUPPORT || ENABLE_HUSH_DOLLAR_OPS */

/* Return code: 0 for OK, 1 for syntax error */
#if BB_MMU
#define parse_dollar(as_string, dest, input, quote_mask) \
	parse_dollar(dest, input, quote_mask)
#define as_string NULL
#endif
static int parse_dollar(o_string *as_string,
		o_string *dest,
		struct in_str *input, unsigned char quote_mask)
{
	int ch = i_peek(input);  /* first character after the $ */

	debug_printf_parse("parse_dollar entered: ch='%c'\n", ch);
	if (isalpha(ch)) {
		ch = i_getch(input);
		nommu_addchr(as_string, ch);
 make_var:
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		while (1) {
			debug_printf_parse(": '%c'\n", ch);
			o_addchr(dest, ch | quote_mask);
			quote_mask = 0;
			ch = i_peek(input);
			if (!isalnum(ch) && ch != '_')
				break;
			ch = i_getch(input);
			nommu_addchr(as_string, ch);
		}
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
	} else if (isdigit(ch)) {
 make_one_char_var:
		ch = i_getch(input);
		nommu_addchr(as_string, ch);
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		debug_printf_parse(": '%c'\n", ch);
		o_addchr(dest, ch | quote_mask);
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
	} else switch (ch) {
	case '$': /* pid */
	case '!': /* last bg pid */
	case '?': /* last exit code */
	case '#': /* number of args */
	case '*': /* args */
	case '@': /* args */
		goto make_one_char_var;
	case '{': {
		o_addchr(dest, SPECIAL_VAR_SYMBOL);

		ch = i_getch(input); /* eat '{' */
		nommu_addchr(as_string, ch);

		ch = i_getch(input); /* first char after '{' */
		/* It should be ${?}, or ${#var},
		 * or even ${?+subst} - operator acting on a special variable,
		 * or the beginning of variable name.
		 */
		if (ch == EOF
		 || (!strchr(_SPECIAL_VARS_STR, ch) && !isalnum(ch)) /* not one of those */
		) {
 bad_dollar_syntax:
			syntax_error_unterm_str("${name}");
			debug_printf_parse("parse_dollar return 0: unterminated ${name}\n");
			return 0;
		}
		nommu_addchr(as_string, ch);
		ch |= quote_mask;

		/* It's possible to just call add_till_closing_bracket() at this point.
		 * However, this regresses some of our testsuite cases
		 * which check invalid constructs like ${%}.
		 * Oh well... let's check that the var name part is fine... */

		while (1) {
			unsigned pos;

			o_addchr(dest, ch);
			debug_printf_parse(": '%c'\n", ch);

			ch = i_getch(input);
			nommu_addchr(as_string, ch);
			if (ch == '}')
				break;

			if (!isalnum(ch) && ch != '_') {
				unsigned end_ch;
				unsigned char last_ch;
				/* handle parameter expansions
				 * http://www.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html#tag_02_06_02
				 */
				if (!strchr(VAR_SUBST_OPS, ch)) /* ${var<bad_char>... */
					goto bad_dollar_syntax;

				/* Eat everything until closing '}' (or ':') */
				end_ch = '}';
				if (ENABLE_HUSH_BASH_COMPAT
				 && ch == ':'
				 && !strchr(MINUS_PLUS_EQUAL_QUESTION, i_peek(input))
				) {
					/* It's ${var:N[:M]} thing */
					end_ch = '}' * 0x100 + ':';
				}
				if (ENABLE_HUSH_BASH_COMPAT
				 && ch == '/'
				) {
					/* It's ${var/[/]pattern[/repl]} thing */
					if (i_peek(input) == '/') { /* ${var//pattern[/repl]}? */
						i_getch(input);
						nommu_addchr(as_string, '/');
						ch = '\\';
					}
					end_ch = '}' * 0x100 + '/';
				}
				o_addchr(dest, ch);
 again:
				if (!BB_MMU)
					pos = dest->length;
#if ENABLE_HUSH_DOLLAR_OPS
				last_ch = add_till_closing_bracket(dest, input, end_ch);
				if (last_ch == 0) /* error? */
					return 0;
#else
#error Simple code to only allow ${var} is not implemented
#endif
				if (as_string) {
					o_addstr(as_string, dest->data + pos);
					o_addchr(as_string, last_ch);
				}

				if (ENABLE_HUSH_BASH_COMPAT && (end_ch & 0xff00)) {
					/* close the first block: */
					o_addchr(dest, SPECIAL_VAR_SYMBOL);
					/* while parsing N from ${var:N[:M]}
					 * or pattern from ${var/[/]pattern[/repl]} */
					if ((end_ch & 0xff) == last_ch) {
						/* got ':' or '/'- parse the rest */
						end_ch = '}';
						goto again;
					}
					/* got '}' */
					if (end_ch == '}' * 0x100 + ':') {
						/* it's ${var:N} - emulate :999999999 */
						o_addstr(dest, "999999999");
					} /* else: it's ${var/[/]pattern} */
				}
				break;
			}
		}
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		break;
	}
#if ENABLE_SH_MATH_SUPPORT || ENABLE_HUSH_TICK
	case '(': {
		unsigned pos;

		ch = i_getch(input);
		nommu_addchr(as_string, ch);
# if ENABLE_SH_MATH_SUPPORT
		if (i_peek(input) == '(') {
			ch = i_getch(input);
			nommu_addchr(as_string, ch);
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			o_addchr(dest, /*quote_mask |*/ '+');
			if (!BB_MMU)
				pos = dest->length;
			if (!add_till_closing_bracket(dest, input, ')' | DOUBLE_CLOSE_CHAR_FLAG))
				return 0; /* error */
			if (as_string) {
				o_addstr(as_string, dest->data + pos);
				o_addchr(as_string, ')');
				o_addchr(as_string, ')');
			}
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			break;
		}
# endif
# if ENABLE_HUSH_TICK
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		o_addchr(dest, quote_mask | '`');
		if (!BB_MMU)
			pos = dest->length;
		if (!add_till_closing_bracket(dest, input, ')'))
			return 0; /* error */
		if (as_string) {
			o_addstr(as_string, dest->data + pos);
			o_addchr(as_string, ')');
		}
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
# endif
		break;
	}
#endif
	case '_':
		ch = i_getch(input);
		nommu_addchr(as_string, ch);
		ch = i_peek(input);
		if (isalnum(ch)) { /* it's $_name or $_123 */
			ch = '_';
			goto make_var;
		}
		/* else: it's $_ */
	/* TODO: $_ and $-: */
	/* $_ Shell or shell script name; or last argument of last command
	 * (if last command wasn't a pipe; if it was, bash sets $_ to "");
	 * but in command's env, set to full pathname used to invoke it */
	/* $- Option flags set by set builtin or shell options (-i etc) */
	default:
		o_addQchr(dest, '$');
	}
	debug_printf_parse("parse_dollar return 1 (ok)\n");
	return 1;
#undef as_string
}

#if BB_MMU
# if ENABLE_HUSH_BASH_COMPAT
#define encode_string(as_string, dest, input, dquote_end, process_bkslash) \
	encode_string(dest, input, dquote_end, process_bkslash)
# else
/* only ${var/pattern/repl} (its pattern part) needs additional mode */
#define encode_string(as_string, dest, input, dquote_end, process_bkslash) \
	encode_string(dest, input, dquote_end)
# endif
#define as_string NULL

#else /* !MMU */

# if ENABLE_HUSH_BASH_COMPAT
/* all parameters are needed, no macro tricks */
# else
#define encode_string(as_string, dest, input, dquote_end, process_bkslash) \
	encode_string(as_string, dest, input, dquote_end)
# endif
#endif
static int encode_string(o_string *as_string,
		o_string *dest,
		struct in_str *input,
		int dquote_end,
		int process_bkslash)
{
#if !ENABLE_HUSH_BASH_COMPAT
	const int process_bkslash = 1;
#endif
	int ch;
	int next;

 again:
	ch = i_getch(input);
	if (ch != EOF)
		nommu_addchr(as_string, ch);
	if (ch == dquote_end) { /* may be only '"' or EOF */
		debug_printf_parse("encode_string return 1 (ok)\n");
		return 1;
	}
	/* note: can't move it above ch == dquote_end check! */
	if (ch == EOF) {
		syntax_error_unterm_ch('"');
		return 0; /* error */
	}
	next = '\0';
	if (ch != '\n') {
		next = i_peek(input);
	}
	debug_printf_parse("\" ch=%c (%d) escape=%d\n",
			ch, ch, !!(dest->o_expflags & EXP_FLAG_ESC_GLOB_CHARS));
	if (process_bkslash && ch == '\\') {
		if (next == EOF) {
			syntax_error("\\<eof>");
			xfunc_die();
		}
		/* bash:
		 * "The backslash retains its special meaning [in "..."]
		 * only when followed by one of the following characters:
		 * $, `, ", \, or <newline>.  A double quote may be quoted
		 * within double quotes by preceding it with a backslash."
		 * NB: in (unquoted) heredoc, above does not apply to ",
		 * therefore we check for it by "next == dquote_end" cond.
		 */
		if (next == dquote_end || strchr("$`\\\n", next)) {
			ch = i_getch(input); /* eat next */
			if (ch == '\n')
				goto again; /* skip \<newline> */
		} /* else: ch remains == '\\', and we double it below: */
		o_addqchr(dest, ch); /* \c if c is a glob char, else just c */
		nommu_addchr(as_string, ch);
		goto again;
	}
	if (ch == '$') {
		if (!parse_dollar(as_string, dest, input, /*quote_mask:*/ 0x80)) {
			debug_printf_parse("encode_string return 0: "
					"parse_dollar returned 0 (error)\n");
			return 0;
		}
		goto again;
	}
#if ENABLE_HUSH_TICK
	if (ch == '`') {
		//unsigned pos = dest->length;
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		o_addchr(dest, 0x80 | '`');
		if (!add_till_backquote(dest, input, /*in_dquote:*/ dquote_end == '"'))
			return 0; /* error */
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		//debug_printf_subst("SUBST RES3 '%s'\n", dest->data + pos);
		goto again;
	}
#endif
	o_addQchr(dest, ch);
	goto again;
#undef as_string
}

/*
 * Scan input until EOF or end_trigger char.
 * Return a list of pipes to execute, or NULL on EOF
 * or if end_trigger character is met.
 * On syntax error, exit if shell is not interactive,
 * reset parsing machinery and start parsing anew,
 * or return ERR_PTR.
 */
static struct pipe *parse_stream(char **pstring,
		struct in_str *input,
		int end_trigger)
{
	struct parse_context ctx;
	o_string dest = NULL_O_STRING;
	int heredoc_cnt;

	/* Single-quote triggers a bypass of the main loop until its mate is
	 * found.  When recursing, quote state is passed in via dest->o_expflags.
	 */
	debug_printf_parse("parse_stream entered, end_trigger='%c'\n",
			end_trigger ? end_trigger : 'X');
	debug_enter();

	/* If very first arg is "" or '', dest.data may end up NULL.
	 * Preventing this: */
	o_addchr(&dest, '\0');
	dest.length = 0;

	/* We used to separate words on $IFS here. This was wrong.
	 * $IFS is used only for word splitting when $var is expanded,
	 * here we should use blank chars as separators, not $IFS
	 */

	if (MAYBE_ASSIGNMENT != 0)
		dest.o_assignment = MAYBE_ASSIGNMENT;
	initialize_context(&ctx);
	heredoc_cnt = 0;
	while (1) {
		const char *is_blank;
		const char *is_special;
		int ch;
		int next;
		int redir_fd;
		redir_type redir_style;

		ch = i_getch(input);
		debug_printf_parse(": ch=%c (%d) escape=%d\n",
				ch, ch, !!(dest.o_expflags & EXP_FLAG_ESC_GLOB_CHARS));
		if (ch == EOF) {
			struct pipe *pi;

			if (heredoc_cnt) {
				syntax_error_unterm_str("here document");
				goto parse_error;
			}
			/* end_trigger == '}' case errors out earlier,
			 * checking only ')' */
			if (end_trigger == ')') {
				syntax_error_unterm_ch('(');
				goto parse_error;
			}

			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
			o_free(&dest);
			done_pipe(&ctx, PIPE_SEQ);
			pi = ctx.list_head;
			/* If we got nothing... */
			/* (this makes bare "&" cmd a no-op.
			 * bash says: "syntax error near unexpected token '&'") */
			if (pi->num_cmds == 0
			IF_HAS_KEYWORDS(&& pi->res_word == RES_NONE)
			) {
				free_pipe_list(pi);
				pi = NULL;
			}
#if !BB_MMU
			debug_printf_parse("as_string1 '%s'\n", ctx.as_string.data);
			if (pstring)
				*pstring = ctx.as_string.data;
			else
				o_free_unsafe(&ctx.as_string);
#endif
			debug_leave();
			debug_printf_parse("parse_stream return %p\n", pi);
			return pi;
		}
		nommu_addchr(&ctx.as_string, ch);

		next = '\0';
		if (ch != '\n')
			next = i_peek(input);

		is_special = "{}<>;&|()#'" /* special outside of "str" */
				"\\$\"" IF_HUSH_TICK("`"); /* always special */
		/* Are { and } special here? */
		if (ctx.command->argv /* word [word]{... - non-special */
		 || dest.length       /* word{... - non-special */
		 || dest.has_quoted_part     /* ""{... - non-special */
		 || (next != ';'             /* }; - special */
		    && next != ')'           /* }) - special */
		    && next != '&'           /* }& and }&& ... - special */
		    && next != '|'           /* }|| ... - special */
		    && !strchr(defifs, next) /* {word - non-special */
		    )
		) {
			/* They are not special, skip "{}" */
			is_special += 2;
		}
		is_special = strchr(is_special, ch);
		is_blank = strchr(defifs, ch);

		if (!is_special && !is_blank) { /* ordinary char */
 ordinary_char:
			o_addQchr(&dest, ch);
			if ((dest.o_assignment == MAYBE_ASSIGNMENT
			    || dest.o_assignment == WORD_IS_KEYWORD)
			 && ch == '='
			 && is_well_formed_var_name(dest.data, '=')
			) {
				dest.o_assignment = DEFINITELY_ASSIGNMENT;
				debug_printf_parse("dest.o_assignment='%s'\n", assignment_flag[dest.o_assignment]);
			}
			continue;
		}

		if (is_blank) {
			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
			if (ch == '\n') {
				/* Is this a case when newline is simply ignored?
				 * Some examples:
				 * "cmd | <newline> cmd ..."
				 * "case ... in <newline> word) ..."
				 */
				if (IS_NULL_CMD(ctx.command)
				 && dest.length == 0 && !dest.has_quoted_part
				) {
					/* This newline can be ignored. But...
					 * Without check #1, interactive shell
					 * ignores even bare <newline>,
					 * and shows the continuation prompt:
					 * ps1_prompt$ <enter>
					 * ps2> _   <=== wrong, should be ps1
					 * Without check #2, "cmd & <newline>"
					 * is similarly mistreated.
					 * (BTW, this makes "cmd & cmd"
					 * and "cmd && cmd" non-orthogonal.
					 * Really, ask yourself, why
					 * "cmd && <newline>" doesn't start
					 * cmd but waits for more input?
					 * No reason...)
					 */
					struct pipe *pi = ctx.list_head;
					if (pi->num_cmds != 0       /* check #1 */
					 && pi->followup != PIPE_BG /* check #2 */
					) {
						continue;
					}
				}
				/* Treat newline as a command separator. */
				done_pipe(&ctx, PIPE_SEQ);
				debug_printf_parse("heredoc_cnt:%d\n", heredoc_cnt);
				if (heredoc_cnt) {
					if (fetch_heredocs(heredoc_cnt, &ctx, input)) {
						goto parse_error;
					}
					heredoc_cnt = 0;
				}
				dest.o_assignment = MAYBE_ASSIGNMENT;
				debug_printf_parse("dest.o_assignment='%s'\n", assignment_flag[dest.o_assignment]);
				ch = ';';
				/* note: if (is_blank) continue;
				 * will still trigger for us */
			}
		}

		/* "cmd}" or "cmd }..." without semicolon or &:
		 * } is an ordinary char in this case, even inside { cmd; }
		 * Pathological example: { ""}; } should exec "}" cmd
		 */
		if (ch == '}') {
			if (!IS_NULL_CMD(ctx.command) /* cmd } */
			 || dest.length != 0 /* word} */
			 || dest.has_quoted_part    /* ""} */
			) {
				goto ordinary_char;
			}
			if (!IS_NULL_PIPE(ctx.pipe)) /* cmd | } */
				goto skip_end_trigger;
			/* else: } does terminate a group */
		}

		if (end_trigger && end_trigger == ch
		 && (ch != ';' || heredoc_cnt == 0)
#if ENABLE_HUSH_CASE
		 && (ch != ')'
		    || ctx.ctx_res_w != RES_MATCH
		    || (!dest.has_quoted_part && strcmp(dest.data, "esac") == 0)
		    )
#endif
		) {
			if (heredoc_cnt) {
				/* This is technically valid:
				 * { cat <<HERE; }; echo Ok
				 * heredoc
				 * heredoc
				 * HERE
				 * but we don't support this.
				 * We require heredoc to be in enclosing {}/(),
				 * if any.
				 */
				syntax_error_unterm_str("here document");
				goto parse_error;
			}
			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
			done_pipe(&ctx, PIPE_SEQ);
			dest.o_assignment = MAYBE_ASSIGNMENT;
			debug_printf_parse("dest.o_assignment='%s'\n", assignment_flag[dest.o_assignment]);
			/* Do we sit outside of any if's, loops or case's? */
			if (!HAS_KEYWORDS
			IF_HAS_KEYWORDS(|| (ctx.ctx_res_w == RES_NONE && ctx.old_flag == 0))
			) {
				o_free(&dest);
#if !BB_MMU
				debug_printf_parse("as_string2 '%s'\n", ctx.as_string.data);
				if (pstring)
					*pstring = ctx.as_string.data;
				else
					o_free_unsafe(&ctx.as_string);
#endif
				debug_leave();
				debug_printf_parse("parse_stream return %p: "
						"end_trigger char found\n",
						ctx.list_head);
				return ctx.list_head;
			}
		}
 skip_end_trigger:
		if (is_blank)
			continue;

		/* Catch <, > before deciding whether this word is
		 * an assignment. a=1 2>z b=2: b=2 is still assignment */
		switch (ch) {
		case '>':
			redir_fd = redirect_opt_num(&dest);
			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
			redir_style = REDIRECT_OVERWRITE;
			if (next == '>') {
				redir_style = REDIRECT_APPEND;
				ch = i_getch(input);
				nommu_addchr(&ctx.as_string, ch);
			}
#if 0
			else if (next == '(') {
				syntax_error(">(process) not supported");
				goto parse_error;
			}
#endif
			if (parse_redirect(&ctx, redir_fd, redir_style, input))
				goto parse_error;
			continue; /* back to top of while (1) */
		case '<':
			redir_fd = redirect_opt_num(&dest);
			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
			redir_style = REDIRECT_INPUT;
			if (next == '<') {
				redir_style = REDIRECT_HEREDOC;
				heredoc_cnt++;
				debug_printf_parse("++heredoc_cnt=%d\n", heredoc_cnt);
				ch = i_getch(input);
				nommu_addchr(&ctx.as_string, ch);
			} else if (next == '>') {
				redir_style = REDIRECT_IO;
				ch = i_getch(input);
				nommu_addchr(&ctx.as_string, ch);
			}
#if 0
			else if (next == '(') {
				syntax_error("<(process) not supported");
				goto parse_error;
			}
#endif
			if (parse_redirect(&ctx, redir_fd, redir_style, input))
				goto parse_error;
			continue; /* back to top of while (1) */
		case '#':
			if (dest.length == 0 && !dest.has_quoted_part) {
				/* skip "#comment" */
				while (1) {
					ch = i_peek(input);
					if (ch == EOF || ch == '\n')
						break;
					i_getch(input);
					/* note: we do not add it to &ctx.as_string */
				}
				nommu_addchr(&ctx.as_string, '\n');
				continue; /* back to top of while (1) */
			}
			break;
		case '\\':
			if (next == '\n') {
				/* It's "\<newline>" */
#if !BB_MMU
				/* Remove trailing '\' from ctx.as_string */
				ctx.as_string.data[--ctx.as_string.length] = '\0';
#endif
				ch = i_getch(input); /* eat it */
				continue; /* back to top of while (1) */
			}
			break;
		}

		if (dest.o_assignment == MAYBE_ASSIGNMENT
		 /* check that we are not in word in "a=1 2>word b=1": */
		 && !ctx.pending_redirect
		) {
			/* ch is a special char and thus this word
			 * cannot be an assignment */
			dest.o_assignment = NOT_ASSIGNMENT;
			debug_printf_parse("dest.o_assignment='%s'\n", assignment_flag[dest.o_assignment]);
		}

		/* Note: nommu_addchr(&ctx.as_string, ch) is already done */

		switch (ch) {
		case '#': /* non-comment #: "echo a#b" etc */
			o_addQchr(&dest, ch);
			break;
		case '\\':
			if (next == EOF) {
				syntax_error("\\<eof>");
				xfunc_die();
			}
			ch = i_getch(input);
			/* note: ch != '\n' (that case does not reach this place) */
			o_addchr(&dest, '\\');
			/*nommu_addchr(&ctx.as_string, '\\'); - already done */
			o_addchr(&dest, ch);
			nommu_addchr(&ctx.as_string, ch);
			/* Example: echo Hello \2>file
			 * we need to know that word 2 is quoted */
			dest.has_quoted_part = 1;
			break;
		case '$':
			if (!parse_dollar(&ctx.as_string, &dest, input, /*quote_mask:*/ 0)) {
				debug_printf_parse("parse_stream parse error: "
					"parse_dollar returned 0 (error)\n");
				goto parse_error;
			}
			break;
		case '\'':
			dest.has_quoted_part = 1;
			if (next == '\'' && !ctx.pending_redirect) {
 insert_empty_quoted_str_marker:
				nommu_addchr(&ctx.as_string, next);
				i_getch(input); /* eat second ' */
				o_addchr(&dest, SPECIAL_VAR_SYMBOL);
				o_addchr(&dest, SPECIAL_VAR_SYMBOL);
			} else {
				while (1) {
					ch = i_getch(input);
					if (ch == EOF) {
						syntax_error_unterm_ch('\'');
						goto parse_error;
					}
					nommu_addchr(&ctx.as_string, ch);
					if (ch == '\'')
						break;
					o_addqchr(&dest, ch);
				}
			}
			break;
		case '"':
			dest.has_quoted_part = 1;
			if (next == '"' && !ctx.pending_redirect)
				goto insert_empty_quoted_str_marker;
			if (dest.o_assignment == NOT_ASSIGNMENT)
				dest.o_expflags |= EXP_FLAG_ESC_GLOB_CHARS;
			if (!encode_string(&ctx.as_string, &dest, input, '"', /*process_bkslash:*/ 1))
				goto parse_error;
			dest.o_expflags &= ~EXP_FLAG_ESC_GLOB_CHARS;
			break;
#if ENABLE_HUSH_TICK
		case '`': {
			USE_FOR_NOMMU(unsigned pos;)

			o_addchr(&dest, SPECIAL_VAR_SYMBOL);
			o_addchr(&dest, '`');
			USE_FOR_NOMMU(pos = dest.length;)
			if (!add_till_backquote(&dest, input, /*in_dquote:*/ 0))
				goto parse_error;
# if !BB_MMU
			o_addstr(&ctx.as_string, dest.data + pos);
			o_addchr(&ctx.as_string, '`');
# endif
			o_addchr(&dest, SPECIAL_VAR_SYMBOL);
			//debug_printf_subst("SUBST RES3 '%s'\n", dest.data + pos);
			break;
		}
#endif
		case ';':
#if ENABLE_HUSH_CASE
 case_semi:
#endif
			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
			done_pipe(&ctx, PIPE_SEQ);
#if ENABLE_HUSH_CASE
			/* Eat multiple semicolons, detect
			 * whether it means something special */
			while (1) {
				ch = i_peek(input);
				if (ch != ';')
					break;
				ch = i_getch(input);
				nommu_addchr(&ctx.as_string, ch);
				if (ctx.ctx_res_w == RES_CASE_BODY) {
					ctx.ctx_dsemicolon = 1;
					ctx.ctx_res_w = RES_MATCH;
					break;
				}
			}
#endif
 new_cmd:
			/* We just finished a cmd. New one may start
			 * with an assignment */
			dest.o_assignment = MAYBE_ASSIGNMENT;
			debug_printf_parse("dest.o_assignment='%s'\n", assignment_flag[dest.o_assignment]);
			break;
		case '&':
			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
			if (next == '&') {
				ch = i_getch(input);
				nommu_addchr(&ctx.as_string, ch);
				done_pipe(&ctx, PIPE_AND);
			} else {
				done_pipe(&ctx, PIPE_BG);
			}
			goto new_cmd;
		case '|':
			if (done_word(&dest, &ctx)) {
				goto parse_error;
			}
#if ENABLE_HUSH_CASE
			if (ctx.ctx_res_w == RES_MATCH)
				break; /* we are in case's "word | word)" */
#endif
			if (next == '|') { /* || */
				ch = i_getch(input);
				nommu_addchr(&ctx.as_string, ch);
				done_pipe(&ctx, PIPE_OR);
			} else {
				/* we could pick up a file descriptor choice here
				 * with redirect_opt_num(), but bash doesn't do it.
				 * "echo foo 2| cat" yields "foo 2". */
				done_command(&ctx);
			}
			goto new_cmd;
		case '(':
#if ENABLE_HUSH_CASE
			/* "case... in [(]word)..." - skip '(' */
			if (ctx.ctx_res_w == RES_MATCH
			 && ctx.command->argv == NULL /* not (word|(... */
			 && dest.length == 0 /* not word(... */
			 && dest.has_quoted_part == 0 /* not ""(... */
			) {
				continue;
			}
#endif
		case '{':
			if (parse_group(&dest, &ctx, input, ch) != 0) {
				goto parse_error;
			}
			goto new_cmd;
		case ')':
#if ENABLE_HUSH_CASE
			if (ctx.ctx_res_w == RES_MATCH)
				goto case_semi;
#endif
		case '}':
			/* proper use of this character is caught by end_trigger:
			 * if we see {, we call parse_group(..., end_trigger='}')
			 * and it will match } earlier (not here). */
			syntax_error_unexpected_ch(ch);
			goto parse_error;
		default:
			if (HUSH_DEBUG)
				bb_error_msg_and_die("BUG: unexpected %c\n", ch);
		}
	} /* while (1) */

 parse_error:
	{
		struct parse_context *pctx;
		IF_HAS_KEYWORDS(struct parse_context *p2;)

		/* Clean up allocated tree.
		 * Sample for finding leaks on syntax error recovery path.
		 * Run it from interactive shell, watch pmap `pidof hush`.
		 * while if false; then false; fi; do break; fi
		 * Samples to catch leaks at execution:
		 * while if (true | {true;}); then echo ok; fi; do break; done
		 * while if (true | {true;}); then echo ok; fi; do (if echo ok; break; then :; fi) | cat; break; done
		 */
		pctx = &ctx;
		do {
			/* Update pipe/command counts,
			 * otherwise freeing may miss some */
			done_pipe(pctx, PIPE_SEQ);
			debug_printf_clean("freeing list %p from ctx %p\n",
					pctx->list_head, pctx);
			debug_print_tree(pctx->list_head, 0);
			free_pipe_list(pctx->list_head);
			debug_printf_clean("freed list %p\n", pctx->list_head);
#if !BB_MMU
			o_free_unsafe(&pctx->as_string);
#endif
			IF_HAS_KEYWORDS(p2 = pctx->stack;)
			if (pctx != &ctx) {
				free(pctx);
			}
			IF_HAS_KEYWORDS(pctx = p2;)
		} while (HAS_KEYWORDS && pctx);

		o_free(&dest);
		G.last_exitcode = 1;
#if !BB_MMU
		if (pstring)
			*pstring = NULL;
#endif
		debug_leave();
		return ERR_PTR;
	}
}


/*** Execution routines ***/

/* Expansion can recurse, need forward decls: */
#if !ENABLE_HUSH_BASH_COMPAT
/* only ${var/pattern/repl} (its pattern part) needs additional mode */
#define expand_string_to_string(str, do_unbackslash) \
	expand_string_to_string(str)
#endif
static char *expand_string_to_string(const char *str, int do_unbackslash);
#if ENABLE_HUSH_TICK
static int process_command_subs(o_string *dest, const char *s);
#endif

/* expand_strvec_to_strvec() takes a list of strings, expands
 * all variable references within and returns a pointer to
 * a list of expanded strings, possibly with larger number
 * of strings. (Think VAR="a b"; echo $VAR).
 * This new list is allocated as a single malloc block.
 * NULL-terminated list of char* pointers is at the beginning of it,
 * followed by strings themselves.
 * Caller can deallocate entire list by single free(list). */

/* A horde of its helpers come first: */

static void o_addblock_duplicate_backslash(o_string *o, const char *str, int len)
{
	while (--len >= 0) {
		char c = *str++;

#if ENABLE_HUSH_BRACE_EXPANSION
		if (c == '{' || c == '}') {
			/* { -> \{, } -> \} */
			o_addchr(o, '\\');
			/* And now we want to add { or } and continue:
			 *  o_addchr(o, c);
			 *  continue;
			 * luckily, just falling throught achieves this.
			 */
		}
#endif
		o_addchr(o, c);
		if (c == '\\') {
			/* \z -> \\\z; \<eol> -> \\<eol> */
			o_addchr(o, '\\');
			if (len) {
				len--;
				o_addchr(o, '\\');
				o_addchr(o, *str++);
			}
		}
	}
}

/* Store given string, finalizing the word and starting new one whenever
 * we encounter IFS char(s). This is used for expanding variable values.
 * End-of-string does NOT finalize word: think about 'echo -$VAR-'.
 * Return in *ended_with_ifs:
 * 1 - ended with IFS char, else 0 (this includes case of empty str).
 */
static int expand_on_ifs(int *ended_with_ifs, o_string *output, int n, const char *str)
{
	int last_is_ifs = 0;

	while (1) {
		int word_len;

		if (!*str)  /* EOL - do not finalize word */
			break;
		word_len = strcspn(str, G.ifs);
		if (word_len) {
			/* We have WORD_LEN leading non-IFS chars */
			if (!(output->o_expflags & EXP_FLAG_GLOB)) {
				o_addblock(output, str, word_len);
			} else {
				/* Protect backslashes against globbing up :)
				 * Example: "v='\*'; echo b$v" prints "b\*"
				 * (and does not try to glob on "*")
				 */
				o_addblock_duplicate_backslash(output, str, word_len);
				/*/ Why can't we do it easier? */
				/*o_addblock(output, str, word_len); - WRONG: "v='\*'; echo Z$v" prints "Z*" instead of "Z\*" */
				/*o_addqblock(output, str, word_len); - WRONG: "v='*'; echo Z$v" prints "Z*" instead of Z* files */
			}
			last_is_ifs = 0;
			str += word_len;
			if (!*str)  /* EOL - do not finalize word */
				break;
		}

		/* We know str here points to at least one IFS char */
		last_is_ifs = 1;
		str += strspn(str, G.ifs); /* skip IFS chars */
		if (!*str)  /* EOL - do not finalize word */
			break;

		/* Start new word... but not always! */
		/* Case "v=' a'; echo ''$v": we do need to finalize empty word: */
		if (output->has_quoted_part
		/* Case "v=' a'; echo $v":
		 * here nothing precedes the space in $v expansion,
		 * therefore we should not finish the word
		 * (IOW: if there *is* word to finalize, only then do it):
		 */
		 || (n > 0 && output->data[output->length - 1])
		) {
			o_addchr(output, '\0');
			debug_print_list("expand_on_ifs", output, n);
			n = o_save_ptr(output, n);
		}
	}

	if (ended_with_ifs)
		*ended_with_ifs = last_is_ifs;
	debug_print_list("expand_on_ifs[1]", output, n);
	return n;
}

/* Helper to expand $((...)) and heredoc body. These act as if
 * they are in double quotes, with the exception that they are not :).
 * Just the rules are similar: "expand only $var and `cmd`"
 *
 * Returns malloced string.
 * As an optimization, we return NULL if expansion is not needed.
 */
#if !ENABLE_HUSH_BASH_COMPAT
/* only ${var/pattern/repl} (its pattern part) needs additional mode */
#define encode_then_expand_string(str, process_bkslash, do_unbackslash) \
	encode_then_expand_string(str)
#endif
static char *encode_then_expand_string(const char *str, int process_bkslash, int do_unbackslash)
{
	char *exp_str;
	struct in_str input;
	o_string dest = NULL_O_STRING;

	if (!strchr(str, '$')
	 && !strchr(str, '\\')
#if ENABLE_HUSH_TICK
	 && !strchr(str, '`')
#endif
	) {
		return NULL;
	}

	/* We need to expand. Example:
	 * echo $(($a + `echo 1`)) $((1 + $((2)) ))
	 */
	setup_string_in_str(&input, str);
	encode_string(NULL, &dest, &input, EOF, process_bkslash);
//TODO: error check (encode_string returns 0 on error)?
	//bb_error_msg("'%s' -> '%s'", str, dest.data);
	exp_str = expand_string_to_string(dest.data, /*unbackslash:*/ do_unbackslash);
	//bb_error_msg("'%s' -> '%s'", dest.data, exp_str);
	o_free_unsafe(&dest);
	return exp_str;
}

#if ENABLE_SH_MATH_SUPPORT
static arith_t expand_and_evaluate_arith(const char *arg, const char **errmsg_p)
{
	arith_state_t math_state;
	arith_t res;
	char *exp_str;

	math_state.lookupvar = get_local_var_value;
	math_state.setvar = set_local_var_from_halves;
	//math_state.endofname = endofname;
	exp_str = encode_then_expand_string(arg, /*process_bkslash:*/ 1, /*unbackslash:*/ 1);
	res = arith(&math_state, exp_str ? exp_str : arg);
	free(exp_str);
	if (errmsg_p)
		*errmsg_p = math_state.errmsg;
	if (math_state.errmsg)
		die_if_script(math_state.errmsg);
	return res;
}
#endif

#if ENABLE_HUSH_BASH_COMPAT
/* ${var/[/]pattern[/repl]} helpers */
static char *strstr_pattern(char *val, const char *pattern, int *size)
{
	while (1) {
		char *end = scan_and_match(val, pattern, SCAN_MOVE_FROM_RIGHT + SCAN_MATCH_LEFT_HALF);
		debug_printf_varexp("val:'%s' pattern:'%s' end:'%s'\n", val, pattern, end);
		if (end) {
			*size = end - val;
			return val;
		}
		if (*val == '\0')
			return NULL;
		/* Optimization: if "*pat" did not match the start of "string",
		 * we know that "tring", "ring" etc will not match too:
		 */
		if (pattern[0] == '*')
			return NULL;
		val++;
	}
}
static char *replace_pattern(char *val, const char *pattern, const char *repl, char exp_op)
{
	char *result = NULL;
	unsigned res_len = 0;
	unsigned repl_len = strlen(repl);

	while (1) {
		int size;
		char *s = strstr_pattern(val, pattern, &size);
		if (!s)
			break;

		result = xrealloc(result, res_len + (s - val) + repl_len + 1);
		memcpy(result + res_len, val, s - val);
		res_len += s - val;
		strcpy(result + res_len, repl);
		res_len += repl_len;
		debug_printf_varexp("val:'%s' s:'%s' result:'%s'\n", val, s, result);

		val = s + size;
		if (exp_op == '/')
			break;
	}
	if (val[0] && result) {
		result = xrealloc(result, res_len + strlen(val) + 1);
		strcpy(result + res_len, val);
		debug_printf_varexp("val:'%s' result:'%s'\n", val, result);
	}
	debug_printf_varexp("result:'%s'\n", result);
	return result;
}
#endif

/* Helper:
 * Handles <SPECIAL_VAR_SYMBOL>varname...<SPECIAL_VAR_SYMBOL> construct.
 */
static NOINLINE const char *expand_one_var(char **to_be_freed_pp, char *arg, char **pp)
{
	const char *val = NULL;
	char *to_be_freed = NULL;
	char *p = *pp;
	char *var;
	char first_char;
	char exp_op;
	char exp_save = exp_save; /* for compiler */
	char *exp_saveptr; /* points to expansion operator */
	char *exp_word = exp_word; /* for compiler */
	char arg0;

	*p = '\0'; /* replace trailing SPECIAL_VAR_SYMBOL */
	var = arg;
	exp_saveptr = arg[1] ? strchr(VAR_ENCODED_SUBST_OPS, arg[1]) : NULL;
	arg0 = arg[0];
	first_char = arg[0] = arg0 & 0x7f;
	exp_op = 0;

	if (first_char == '#'      /* ${#... */
	 && arg[1] && !exp_saveptr /* not ${#} and not ${#<op_char>...} */
	) {
		/* It must be length operator: ${#var} */
		var++;
		exp_op = 'L';
	} else {
		/* Maybe handle parameter expansion */
		if (exp_saveptr /* if 2nd char is one of expansion operators */
		 && strchr(NUMERIC_SPECVARS_STR, first_char) /* 1st char is special variable */
		) {
			/* ${?:0}, ${#[:]%0} etc */
			exp_saveptr = var + 1;
		} else {
			/* ${?}, ${var}, ${var:0}, ${var[:]%0} etc */
			exp_saveptr = var+1 + strcspn(var+1, VAR_ENCODED_SUBST_OPS);
		}
		exp_op = exp_save = *exp_saveptr;
		if (exp_op) {
			exp_word = exp_saveptr + 1;
			if (exp_op == ':') {
				exp_op = *exp_word++;
//TODO: try ${var:} and ${var:bogus} in non-bash config
				if (ENABLE_HUSH_BASH_COMPAT
				 && (!exp_op || !strchr(MINUS_PLUS_EQUAL_QUESTION, exp_op))
				) {
					/* oops... it's ${var:N[:M]}, not ${var:?xxx} or some such */
					exp_op = ':';
					exp_word--;
				}
			}
			*exp_saveptr = '\0';
		} /* else: it's not an expansion op, but bare ${var} */
	}

	/* Look up the variable in question */
	if (isdigit(var[0])) {
		/* parse_dollar should have vetted var for us */
		int n = xatoi_positive(var);
		if (n < G.global_argc)
			val = G.global_argv[n];
		/* else val remains NULL: $N with too big N */
	} else {
		switch (var[0]) {
		case '$': /* pid */
			val = utoa(G.root_pid);
			break;
		case '!': /* bg pid */
			val = G.last_bg_pid ? utoa(G.last_bg_pid) : "";
			break;
		case '?': /* exitcode */
			val = utoa(G.last_exitcode);
			break;
		case '#': /* argc */
			val = utoa(G.global_argc ? G.global_argc-1 : 0);
			break;
		default:
			val = get_local_var_value(var);
		}
	}

	/* Handle any expansions */
	if (exp_op == 'L') {
		reinit_unicode_for_hush();
		debug_printf_expand("expand: length(%s)=", val);
		val = utoa(val ? unicode_strlen(val) : 0);
		debug_printf_expand("%s\n", val);
	} else if (exp_op) {
		if (exp_op == '%' || exp_op == '#') {
			/* Standard-mandated substring removal ops:
			 * ${parameter%word} - remove smallest suffix pattern
			 * ${parameter%%word} - remove largest suffix pattern
			 * ${parameter#word} - remove smallest prefix pattern
			 * ${parameter##word} - remove largest prefix pattern
			 *
			 * Word is expanded to produce a glob pattern.
			 * Then var's value is matched to it and matching part removed.
			 */
			if (val && val[0]) {
				char *t;
				char *exp_exp_word;
				char *loc;
				unsigned scan_flags = pick_scan(exp_op, *exp_word);
				if (exp_op == *exp_word)  /* ## or %% */
					exp_word++;
				exp_exp_word = encode_then_expand_string(exp_word, /*process_bkslash:*/ 1, /*unbackslash:*/ 1);
				if (exp_exp_word)
					exp_word = exp_exp_word;
				/* HACK ALERT. We depend here on the fact that
				 * G.global_argv and results of utoa and get_local_var_value
				 * are actually in writable memory:
				 * scan_and_match momentarily stores NULs there. */
				t = (char*)val;
				loc = scan_and_match(t, exp_word, scan_flags);
				//bb_error_msg("op:%c str:'%s' pat:'%s' res:'%s'",
				//		exp_op, t, exp_word, loc);
				free(exp_exp_word);
				if (loc) { /* match was found */
					if (scan_flags & SCAN_MATCH_LEFT_HALF) /* #[#] */
						val = loc; /* take right part */
					else /* %[%] */
						val = to_be_freed = xstrndup(val, loc - val); /* left */
				}
			}
		}
#if ENABLE_HUSH_BASH_COMPAT
		else if (exp_op == '/' || exp_op == '\\') {
			/* It's ${var/[/]pattern[/repl]} thing.
			 * Note that in encoded form it has TWO parts:
			 * var/pattern<SPECIAL_VAR_SYMBOL>repl<SPECIAL_VAR_SYMBOL>
			 * and if // is used, it is encoded as \:
			 * var\pattern<SPECIAL_VAR_SYMBOL>repl<SPECIAL_VAR_SYMBOL>
			 */
			/* Empty variable always gives nothing: */
			// "v=''; echo ${v/*/w}" prints "", not "w"
			if (val && val[0]) {
				/* pattern uses non-standard expansion.
				 * repl should be unbackslashed and globbed
				 * by the usual expansion rules:
				 * >az; >bz;
				 * v='a bz'; echo "${v/a*z/a*z}" prints "a*z"
				 * v='a bz'; echo "${v/a*z/\z}"  prints "\z"
				 * v='a bz'; echo ${v/a*z/a*z}   prints "az"
				 * v='a bz'; echo ${v/a*z/\z}    prints "z"
				 * (note that a*z _pattern_ is never globbed!)
				 */
				char *pattern, *repl, *t;
				pattern = encode_then_expand_string(exp_word, /*process_bkslash:*/ 0, /*unbackslash:*/ 0);
				if (!pattern)
					pattern = xstrdup(exp_word);
				debug_printf_varexp("pattern:'%s'->'%s'\n", exp_word, pattern);
				*p++ = SPECIAL_VAR_SYMBOL;
				exp_word = p;
				p = strchr(p, SPECIAL_VAR_SYMBOL);
				*p = '\0';
				repl = encode_then_expand_string(exp_word, /*process_bkslash:*/ arg0 & 0x80, /*unbackslash:*/ 1);
				debug_printf_varexp("repl:'%s'->'%s'\n", exp_word, repl);
				/* HACK ALERT. We depend here on the fact that
				 * G.global_argv and results of utoa and get_local_var_value
				 * are actually in writable memory:
				 * replace_pattern momentarily stores NULs there. */
				t = (char*)val;
				to_be_freed = replace_pattern(t,
						pattern,
						(repl ? repl : exp_word),
						exp_op);
				if (to_be_freed) /* at least one replace happened */
					val = to_be_freed;
				free(pattern);
				free(repl);
			}
		}
#endif
		else if (exp_op == ':') {
#if ENABLE_HUSH_BASH_COMPAT && ENABLE_SH_MATH_SUPPORT
			/* It's ${var:N[:M]} bashism.
			 * Note that in encoded form it has TWO parts:
			 * var:N<SPECIAL_VAR_SYMBOL>M<SPECIAL_VAR_SYMBOL>
			 */
			arith_t beg, len;
			const char *errmsg;

			beg = expand_and_evaluate_arith(exp_word, &errmsg);
			if (errmsg)
				goto arith_err;
			debug_printf_varexp("beg:'%s'=%lld\n", exp_word, (long long)beg);
			*p++ = SPECIAL_VAR_SYMBOL;
			exp_word = p;
			p = strchr(p, SPECIAL_VAR_SYMBOL);
			*p = '\0';
			len = expand_and_evaluate_arith(exp_word, &errmsg);
			if (errmsg)
				goto arith_err;
			debug_printf_varexp("len:'%s'=%lld\n", exp_word, (long long)len);
			if (len >= 0) { /* bash compat: len < 0 is illegal */
				if (beg < 0) /* bash compat */
					beg = 0;
				debug_printf_varexp("from val:'%s'\n", val);
				if (len == 0 || !val || beg >= strlen(val)) {
 arith_err:
					val = NULL;
				} else {
					/* Paranoia. What if user entered 9999999999999
					 * which fits in arith_t but not int? */
					if (len >= INT_MAX)
						len = INT_MAX;
					val = to_be_freed = xstrndup(val + beg, len);
				}
				debug_printf_varexp("val:'%s'\n", val);
			} else
#endif
			{
				die_if_script("malformed ${%s:...}", var);
				val = NULL;
			}
		} else { /* one of "-=+?" */
			/* Standard-mandated substitution ops:
			 * ${var?word} - indicate error if unset
			 *      If var is unset, word (or a message indicating it is unset
			 *      if word is null) is written to standard error
			 *      and the shell exits with a non-zero exit status.
			 *      Otherwise, the value of var is substituted.
			 * ${var-word} - use default value
			 *      If var is unset, word is substituted.
			 * ${var=word} - assign and use default value
			 *      If var is unset, word is assigned to var.
			 *      In all cases, final value of var is substituted.
			 * ${var+word} - use alternative value
			 *      If var is unset, null is substituted.
			 *      Otherwise, word is substituted.
			 *
			 * Word is subjected to tilde expansion, parameter expansion,
			 * command substitution, and arithmetic expansion.
			 * If word is not needed, it is not expanded.
			 *
			 * Colon forms (${var:-word}, ${var:=word} etc) do the same,
			 * but also treat null var as if it is unset.
			 */
			int use_word = (!val || ((exp_save == ':') && !val[0]));
			if (exp_op == '+')
				use_word = !use_word;
			debug_printf_expand("expand: op:%c (null:%s) test:%i\n", exp_op,
					(exp_save == ':') ? "true" : "false", use_word);
			if (use_word) {
				to_be_freed = encode_then_expand_string(exp_word, /*process_bkslash:*/ 1, /*unbackslash:*/ 1);
				if (to_be_freed)
					exp_word = to_be_freed;
				if (exp_op == '?') {
					/* mimic bash message */
					die_if_script("%s: %s",
						var,
						exp_word[0] ? exp_word : "parameter null or not set"
					);
//TODO: how interactive bash aborts expansion mid-command?
				} else {
					val = exp_word;
				}

				if (exp_op == '=') {
					/* ${var=[word]} or ${var:=[word]} */
					if (isdigit(var[0]) || var[0] == '#') {
						/* mimic bash message */
						die_if_script("$%s: cannot assign in this way", var);
						val = NULL;
					} else {
						char *new_var = xasprintf("%s=%s", var, val);
						set_local_var(new_var, /*exp:*/ 0, /*lvl:*/ 0, /*ro:*/ 0);
					}
				}
			}
		} /* one of "-=+?" */

		*exp_saveptr = exp_save;
	} /* if (exp_op) */

	arg[0] = arg0;

	*pp = p;
	*to_be_freed_pp = to_be_freed;
	return val;
}

/* Expand all variable references in given string, adding words to list[]
 * at n, n+1,... positions. Return updated n (so that list[n] is next one
 * to be filled). This routine is extremely tricky: has to deal with
 * variables/parameters with whitespace, $* and $@, and constructs like
 * 'echo -$*-'. If you play here, you must run testsuite afterwards! */
static NOINLINE int expand_vars_to_list(o_string *output, int n, char *arg)
{
	/* output->o_expflags & EXP_FLAG_SINGLEWORD (0x80) if we are in
	 * expansion of right-hand side of assignment == 1-element expand.
	 */
	char cant_be_null = 0; /* only bit 0x80 matters */
	int ended_in_ifs = 0;  /* did last unquoted expansion end with IFS chars? */
	char *p;

	debug_printf_expand("expand_vars_to_list: arg:'%s' singleword:%x\n", arg,
			!!(output->o_expflags & EXP_FLAG_SINGLEWORD));
	debug_print_list("expand_vars_to_list", output, n);
	n = o_save_ptr(output, n);
	debug_print_list("expand_vars_to_list[0]", output, n);

	while ((p = strchr(arg, SPECIAL_VAR_SYMBOL)) != NULL) {
		char first_ch;
		char *to_be_freed = NULL;
		const char *val = NULL;
#if ENABLE_HUSH_TICK
		o_string subst_result = NULL_O_STRING;
#endif
#if ENABLE_SH_MATH_SUPPORT
		char arith_buf[sizeof(arith_t)*3 + 2];
#endif

		if (ended_in_ifs) {
			o_addchr(output, '\0');
			n = o_save_ptr(output, n);
			ended_in_ifs = 0;
		}

		o_addblock(output, arg, p - arg);
		debug_print_list("expand_vars_to_list[1]", output, n);
		arg = ++p;
		p = strchr(p, SPECIAL_VAR_SYMBOL);

		/* Fetch special var name (if it is indeed one of them)
		 * and quote bit, force the bit on if singleword expansion -
		 * important for not getting v=$@ expand to many words. */
		first_ch = arg[0] | (output->o_expflags & EXP_FLAG_SINGLEWORD);

		/* Is this variable quoted and thus expansion can't be null?
		 * "$@" is special. Even if quoted, it can still
		 * expand to nothing (not even an empty string),
		 * thus it is excluded. */
		if ((first_ch & 0x7f) != '@')
			cant_be_null |= first_ch;

		switch (first_ch & 0x7f) {
		/* Highest bit in first_ch indicates that var is double-quoted */
		case '*':
		case '@': {
			int i;
			if (!G.global_argv[1])
				break;
			i = 1;
			cant_be_null |= first_ch; /* do it for "$@" _now_, when we know it's not empty */
			if (!(first_ch & 0x80)) { /* unquoted $* or $@ */
				while (G.global_argv[i]) {
					n = expand_on_ifs(NULL, output, n, G.global_argv[i]);
					debug_printf_expand("expand_vars_to_list: argv %d (last %d)\n", i, G.global_argc - 1);
					if (G.global_argv[i++][0] && G.global_argv[i]) {
						/* this argv[] is not empty and not last:
						 * put terminating NUL, start new word */
						o_addchr(output, '\0');
						debug_print_list("expand_vars_to_list[2]", output, n);
						n = o_save_ptr(output, n);
						debug_print_list("expand_vars_to_list[3]", output, n);
					}
				}
			} else
			/* If EXP_FLAG_SINGLEWORD, we handle assignment 'a=....$@.....'
			 * and in this case should treat it like '$*' - see 'else...' below */
			if (first_ch == ('@'|0x80)  /* quoted $@ */
			 && !(output->o_expflags & EXP_FLAG_SINGLEWORD) /* not v="$@" case */
			) {
				while (1) {
					o_addQstr(output, G.global_argv[i]);
					if (++i >= G.global_argc)
						break;
					o_addchr(output, '\0');
					debug_print_list("expand_vars_to_list[4]", output, n);
					n = o_save_ptr(output, n);
				}
			} else { /* quoted $* (or v="$@" case): add as one word */
				while (1) {
					o_addQstr(output, G.global_argv[i]);
					if (!G.global_argv[++i])
						break;
					if (G.ifs[0])
						o_addchr(output, G.ifs[0]);
				}
				output->has_quoted_part = 1;
			}
			break;
		}
		case SPECIAL_VAR_SYMBOL: /* <SPECIAL_VAR_SYMBOL><SPECIAL_VAR_SYMBOL> */
			/* "Empty variable", used to make "" etc to not disappear */
			output->has_quoted_part = 1;
			arg++;
			cant_be_null = 0x80;
			break;
#if ENABLE_HUSH_TICK
		case '`': /* <SPECIAL_VAR_SYMBOL>`cmd<SPECIAL_VAR_SYMBOL> */
			*p = '\0'; /* replace trailing <SPECIAL_VAR_SYMBOL> */
			arg++;
			/* Can't just stuff it into output o_string,
			 * expanded result may need to be globbed
			 * and $IFS-splitted */
			debug_printf_subst("SUBST '%s' first_ch %x\n", arg, first_ch);
			G.last_exitcode = process_command_subs(&subst_result, arg);
			debug_printf_subst("SUBST RES:%d '%s'\n", G.last_exitcode, subst_result.data);
			val = subst_result.data;
			goto store_val;
#endif
#if ENABLE_SH_MATH_SUPPORT
		case '+': { /* <SPECIAL_VAR_SYMBOL>+cmd<SPECIAL_VAR_SYMBOL> */
			arith_t res;

			arg++; /* skip '+' */
			*p = '\0'; /* replace trailing <SPECIAL_VAR_SYMBOL> */
			debug_printf_subst("ARITH '%s' first_ch %x\n", arg, first_ch);
			res = expand_and_evaluate_arith(arg, NULL);
			debug_printf_subst("ARITH RES '"ARITH_FMT"'\n", res);
			sprintf(arith_buf, ARITH_FMT, res);
			val = arith_buf;
			break;
		}
#endif
		default:
			val = expand_one_var(&to_be_freed, arg, &p);
 IF_HUSH_TICK(store_val:)
			if (!(first_ch & 0x80)) { /* unquoted $VAR */
				debug_printf_expand("unquoted '%s', output->o_escape:%d\n", val,
						!!(output->o_expflags & EXP_FLAG_ESC_GLOB_CHARS));
				if (val && val[0]) {
					n = expand_on_ifs(&ended_in_ifs, output, n, val);
					val = NULL;
				}
			} else { /* quoted $VAR, val will be appended below */
				output->has_quoted_part = 1;
				debug_printf_expand("quoted '%s', output->o_escape:%d\n", val,
						!!(output->o_expflags & EXP_FLAG_ESC_GLOB_CHARS));
			}
			break;
		} /* switch (char after <SPECIAL_VAR_SYMBOL>) */

		if (val && val[0]) {
			o_addQstr(output, val);
		}
		free(to_be_freed);

		/* Restore NULL'ed SPECIAL_VAR_SYMBOL.
		 * Do the check to avoid writing to a const string. */
		if (*p != SPECIAL_VAR_SYMBOL)
			*p = SPECIAL_VAR_SYMBOL;

#if ENABLE_HUSH_TICK
		o_free(&subst_result);
#endif
		arg = ++p;
	} /* end of "while (SPECIAL_VAR_SYMBOL is found) ..." */

	if (arg[0]) {
		if (ended_in_ifs) {
			o_addchr(output, '\0');
			n = o_save_ptr(output, n);
		}
		debug_print_list("expand_vars_to_list[a]", output, n);
		/* this part is literal, and it was already pre-quoted
		 * if needed (much earlier), do not use o_addQstr here! */
		o_addstr_with_NUL(output, arg);
		debug_print_list("expand_vars_to_list[b]", output, n);
	} else if (output->length == o_get_last_ptr(output, n) /* expansion is empty */
	 && !(cant_be_null & 0x80) /* and all vars were not quoted. */
	) {
		n--;
		/* allow to reuse list[n] later without re-growth */
		output->has_empty_slot = 1;
	} else {
		o_addchr(output, '\0');
	}

	return n;
}

static char **expand_variables(char **argv, unsigned expflags)
{
	int n;
	char **list;
	o_string output = NULL_O_STRING;

	output.o_expflags = expflags;

	n = 0;
	while (*argv) {
		n = expand_vars_to_list(&output, n, *argv);
		argv++;
	}
	debug_print_list("expand_variables", &output, n);

	/* output.data (malloced in one block) gets returned in "list" */
	list = o_finalize_list(&output, n);
	debug_print_strings("expand_variables[1]", list);
	return list;
}

static char **expand_strvec_to_strvec(char **argv)
{
	return expand_variables(argv, EXP_FLAG_GLOB | EXP_FLAG_ESC_GLOB_CHARS);
}

#if ENABLE_HUSH_BASH_COMPAT
static char **expand_strvec_to_strvec_singleword_noglob(char **argv)
{
	return expand_variables(argv, EXP_FLAG_SINGLEWORD);
}
#endif

/* Used for expansion of right hand of assignments,
 * $((...)), heredocs, variable espansion parts.
 *
 * NB: should NOT do globbing!
 * "export v=/bin/c*; env | grep ^v=" outputs "v=/bin/c*"
 */
static char *expand_string_to_string(const char *str, int do_unbackslash)
{
#if !ENABLE_HUSH_BASH_COMPAT
	const int do_unbackslash = 1;
#endif
	char *argv[2], **list;

	debug_printf_expand("string_to_string<='%s'\n", str);
	/* This is generally an optimization, but it also
	 * handles "", which otherwise trips over !list[0] check below.
	 * (is this ever happens that we actually get str="" here?)
	 */
	if (!strchr(str, SPECIAL_VAR_SYMBOL) && !strchr(str, '\\')) {
		//TODO: Can use on strings with \ too, just unbackslash() them?
		debug_printf_expand("string_to_string(fast)=>'%s'\n", str);
		return xstrdup(str);
	}

	argv[0] = (char*)str;
	argv[1] = NULL;
	list = expand_variables(argv, do_unbackslash
			? EXP_FLAG_ESC_GLOB_CHARS | EXP_FLAG_SINGLEWORD
			: EXP_FLAG_SINGLEWORD
	);
	if (HUSH_DEBUG)
		if (!list[0] || list[1])
			bb_error_msg_and_die("BUG in varexp2");
	/* actually, just move string 2*sizeof(char*) bytes back */
	overlapping_strcpy((char*)list, list[0]);
	if (do_unbackslash)
		unbackslash((char*)list);
	debug_printf_expand("string_to_string=>'%s'\n", (char*)list);
	return (char*)list;
}

/* Used for "eval" builtin */
static char* expand_strvec_to_string(char **argv)
{
	char **list;

	list = expand_variables(argv, EXP_FLAG_SINGLEWORD);
	/* Convert all NULs to spaces */
	if (list[0]) {
		int n = 1;
		while (list[n]) {
			if (HUSH_DEBUG)
				if (list[n-1] + strlen(list[n-1]) + 1 != list[n])
					bb_error_msg_and_die("BUG in varexp3");
			/* bash uses ' ' regardless of $IFS contents */
			list[n][-1] = ' ';
			n++;
		}
	}
	overlapping_strcpy((char*)list, list[0]);
	debug_printf_expand("strvec_to_string='%s'\n", (char*)list);
	return (char*)list;
}

static char **expand_assignments(char **argv, int count)
{
	int i;
	char **p;

	G.expanded_assignments = p = NULL;
	/* Expand assignments into one string each */
	for (i = 0; i < count; i++) {
		G.expanded_assignments = p = add_string_to_strings(p, expand_string_to_string(argv[i], /*unbackslash:*/ 1));
	}
	G.expanded_assignments = NULL;
	return p;
}


static void switch_off_special_sigs(unsigned mask)
{
	unsigned sig = 0;
	while ((mask >>= 1) != 0) {
		sig++;
		if (!(mask & 1))
			continue;
		if (G.traps) {
			if (G.traps[sig] && !G.traps[sig][0])
				/* trap is '', has to remain SIG_IGN */
				continue;
			free(G.traps[sig]);
			G.traps[sig] = NULL;
		}
		/* We are here only if no trap or trap was not '' */
		install_sighandler(sig, SIG_DFL);
	}
}

#if BB_MMU
/* never called */
void re_execute_shell(char ***to_free, const char *s,
		char *g_argv0, char **g_argv,
		char **builtin_argv) NORETURN;

static void reset_traps_to_defaults(void)
{
	/* This function is always called in a child shell
	 * after fork (not vfork, NOMMU doesn't use this function).
	 */
	unsigned sig;
	unsigned mask;

	/* Child shells are not interactive.
	 * SIGTTIN/SIGTTOU/SIGTSTP should not have special handling.
	 * Testcase: (while :; do :; done) + ^Z should background.
	 * Same goes for SIGTERM, SIGHUP, SIGINT.
	 */
	mask = (G.special_sig_mask & SPECIAL_INTERACTIVE_SIGS) | G_fatal_sig_mask;
	if (!G.traps && !mask)
		return; /* already no traps and no special sigs */

	/* Switch off special sigs */
	switch_off_special_sigs(mask);
#if ENABLE_HUSH_JOB
	G_fatal_sig_mask = 0;
#endif
	G.special_sig_mask &= ~SPECIAL_INTERACTIVE_SIGS;
	/* SIGQUIT,SIGCHLD and maybe SPECIAL_JOBSTOP_SIGS
	 * remain set in G.special_sig_mask */

	if (!G.traps)
		return;

	/* Reset all sigs to default except ones with empty traps */
	for (sig = 0; sig < NSIG; sig++) {
		if (!G.traps[sig])
			continue; /* no trap: nothing to do */
		if (!G.traps[sig][0])
			continue; /* empty trap: has to remain SIG_IGN */
		/* sig has non-empty trap, reset it: */
		free(G.traps[sig]);
		G.traps[sig] = NULL;
		/* There is no signal for trap 0 (EXIT) */
		if (sig == 0)
			continue;
		install_sighandler(sig, pick_sighandler(sig));
	}
}

#else /* !BB_MMU */

static void re_execute_shell(char ***to_free, const char *s,
		char *g_argv0, char **g_argv,
		char **builtin_argv) NORETURN;
static void re_execute_shell(char ***to_free, const char *s,
		char *g_argv0, char **g_argv,
		char **builtin_argv)
{
# define NOMMU_HACK_FMT ("-$%x:%x:%x:%x:%x:%llx" IF_HUSH_LOOPS(":%x"))
	/* delims + 2 * (number of bytes in printed hex numbers) */
	char param_buf[sizeof(NOMMU_HACK_FMT) + 2 * (sizeof(int)*6 + sizeof(long long)*1)];
	char *heredoc_argv[4];
	struct variable *cur;
# if ENABLE_HUSH_FUNCTIONS
	struct function *funcp;
# endif
	char **argv, **pp;
	unsigned cnt;
	unsigned long long empty_trap_mask;

	if (!g_argv0) { /* heredoc */
		argv = heredoc_argv;
		argv[0] = (char *) G.argv0_for_re_execing;
		argv[1] = (char *) "-<";
		argv[2] = (char *) s;
		argv[3] = NULL;
		pp = &argv[3]; /* used as pointer to empty environment */
		goto do_exec;
	}

	cnt = 0;
	pp = builtin_argv;
	if (pp) while (*pp++)
		cnt++;

	empty_trap_mask = 0;
	if (G.traps) {
		int sig;
		for (sig = 1; sig < NSIG; sig++) {
			if (G.traps[sig] && !G.traps[sig][0])
				empty_trap_mask |= 1LL << sig;
		}
	}

	sprintf(param_buf, NOMMU_HACK_FMT
			, (unsigned) G.root_pid
			, (unsigned) G.root_ppid
			, (unsigned) G.last_bg_pid
			, (unsigned) G.last_exitcode
			, cnt
			, empty_trap_mask
			IF_HUSH_LOOPS(, G.depth_of_loop)
			);
# undef NOMMU_HACK_FMT
	/* 1:hush 2:-$<pid>:<pid>:<exitcode>:<etc...> <vars...> <funcs...>
	 * 3:-c 4:<cmd> 5:<arg0> <argN...> 6:NULL
	 */
	cnt += 6;
	for (cur = G.top_var; cur; cur = cur->next) {
		if (!cur->flg_export || cur->flg_read_only)
			cnt += 2;
	}
# if ENABLE_HUSH_FUNCTIONS
	for (funcp = G.top_func; funcp; funcp = funcp->next)
		cnt += 3;
# endif
	pp = g_argv;
	while (*pp++)
		cnt++;
	*to_free = argv = pp = xzalloc(sizeof(argv[0]) * cnt);
	*pp++ = (char *) G.argv0_for_re_execing;
	*pp++ = param_buf;
	for (cur = G.top_var; cur; cur = cur->next) {
		if (strcmp(cur->varstr, hush_version_str) == 0)
			continue;
		if (cur->flg_read_only) {
			*pp++ = (char *) "-R";
			*pp++ = cur->varstr;
		} else if (!cur->flg_export) {
			*pp++ = (char *) "-V";
			*pp++ = cur->varstr;
		}
	}
# if ENABLE_HUSH_FUNCTIONS
	for (funcp = G.top_func; funcp; funcp = funcp->next) {
		*pp++ = (char *) "-F";
		*pp++ = funcp->name;
		*pp++ = funcp->body_as_string;
	}
# endif
	/* We can pass activated traps here. Say, -Tnn:trap_string
	 *
	 * However, POSIX says that subshells reset signals with traps
	 * to SIG_DFL.
	 * I tested bash-3.2 and it not only does that with true subshells
	 * of the form ( list ), but with any forked children shells.
	 * I set trap "echo W" WINCH; and then tried:
	 *
	 * { echo 1; sleep 20; echo 2; } &
	 * while true; do echo 1; sleep 20; echo 2; break; done &
	 * true | { echo 1; sleep 20; echo 2; } | cat
	 *
	 * In all these cases sending SIGWINCH to the child shell
	 * did not run the trap. If I add trap "echo V" WINCH;
	 * _inside_ group (just before echo 1), it works.
	 *
	 * I conclude it means we don't need to pass active traps here.
	 */
	*pp++ = (char *) "-c";
	*pp++ = (char *) s;
	if (builtin_argv) {
		while (*++builtin_argv)
			*pp++ = *builtin_argv;
		*pp++ = (char *) "";
	}
	*pp++ = g_argv0;
	while (*g_argv)
		*pp++ = *g_argv++;
	/* *pp = NULL; - is already there */
	pp = environ;

 do_exec:
	debug_printf_exec("re_execute_shell pid:%d cmd:'%s'\n", getpid(), s);
	/* Don't propagate SIG_IGN to the child */
	if (SPECIAL_JOBSTOP_SIGS != 0)
		switch_off_special_sigs(G.special_sig_mask & SPECIAL_JOBSTOP_SIGS);
	execve(bb_busybox_exec_path, argv, pp);
	/* Fallback. Useful for init=/bin/hush usage etc */
	if (argv[0][0] == '/')
		execve(argv[0], argv, pp);
	xfunc_error_retval = 127;
	bb_error_msg_and_die("can't re-execute the shell");
}
#endif  /* !BB_MMU */


static int run_and_free_list(struct pipe *pi);

/* Executing from string: eval, sh -c '...'
 *          or from file: /etc/profile, . file, sh <script>, sh (intereactive)
 * end_trigger controls how often we stop parsing
 * NUL: parse all, execute, return
 * ';': parse till ';' or newline, execute, repeat till EOF
 */
static void parse_and_run_stream(struct in_str *inp, int end_trigger)
{
	/* Why we need empty flag?
	 * An obscure corner case "false; ``; echo $?":
	 * empty command in `` should still set $? to 0.
	 * But we can't just set $? to 0 at the start,
	 * this breaks "false; echo `echo $?`" case.
	 */
	bool empty = 1;
	while (1) {
		struct pipe *pipe_list;

#if ENABLE_HUSH_INTERACTIVE
		if (end_trigger == ';')
			inp->promptmode = 0; /* PS1 */
#endif
		pipe_list = parse_stream(NULL, inp, end_trigger);
		if (!pipe_list || pipe_list == ERR_PTR) { /* EOF/error */
			/* If we are in "big" script
			 * (not in `cmd` or something similar)...
			 */
			if (pipe_list == ERR_PTR && end_trigger == ';') {
				/* Discard cached input (rest of line) */
				int ch = inp->last_char;
				while (ch != EOF && ch != '\n') {
					//bb_error_msg("Discarded:'%c'", ch);
					ch = i_getch(inp);
				}
				/* Force prompt */
				inp->p = NULL;
				/* This stream isn't empty */
				empty = 0;
				continue;
			}
			if (!pipe_list && empty)
				G.last_exitcode = 0;
			break;
		}
		debug_print_tree(pipe_list, 0);
		debug_printf_exec("parse_and_run_stream: run_and_free_list\n");
		run_and_free_list(pipe_list);
		empty = 0;
#if ENABLE_HUSH_FUNCTIONS
		if (G.flag_return_in_progress == 1)
			break;
#endif
	}
}

static void parse_and_run_string(const char *s)
{
	struct in_str input;
	setup_string_in_str(&input, s);
	parse_and_run_stream(&input, '\0');
}

static void parse_and_run_file(FILE *f)
{
	struct in_str input;
	setup_file_in_str(&input, f);
	parse_and_run_stream(&input, ';');
}

#if ENABLE_HUSH_TICK
static FILE *generate_stream_from_string(const char *s, pid_t *pid_p)
{
	pid_t pid;
	int channel[2];
# if !BB_MMU
	char **to_free = NULL;
# endif

	xpipe(channel);
	pid = BB_MMU ? xfork() : xvfork();
	if (pid == 0) { /* child */
		disable_restore_tty_pgrp_on_exit();
		/* Process substitution is not considered to be usual
		 * 'command execution'.
		 * SUSv3 says ctrl-Z should be ignored, ctrl-C should not.
		 */
		bb_signals(0
			+ (1 << SIGTSTP)
			+ (1 << SIGTTIN)
			+ (1 << SIGTTOU)
			, SIG_IGN);
		CLEAR_RANDOM_T(&G.random_gen); /* or else $RANDOM repeats in child */
		close(channel[0]); /* NB: close _first_, then move fd! */
		xmove_fd(channel[1], 1);
		/* Prevent it from trying to handle ctrl-z etc */
		IF_HUSH_JOB(G.run_list_level = 1;)
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
		s = skip_whitespace(s);
		if (is_prefixed_with(s, "trap")
		 && skip_whitespace(s + 4)[0] == '\0'
		) {
			static const char *const argv[] = { NULL, NULL };
			builtin_trap((char**)argv);
			fflush_all(); /* important */
			_exit(0);
		}
# if BB_MMU
		reset_traps_to_defaults();
		parse_and_run_string(s);
		_exit(G.last_exitcode);
# else
	/* We re-execute after vfork on NOMMU. This makes this script safe:
	 * yes "0123456789012345678901234567890" | dd bs=32 count=64k >BIG
	 * huge=`cat BIG` # was blocking here forever
	 * echo OK
	 */
		re_execute_shell(&to_free,
				s,
				G.global_argv[0],
				G.global_argv + 1,
				NULL);
# endif
	}

	/* parent */
	*pid_p = pid;
# if ENABLE_HUSH_FAST
	G.count_SIGCHLD++;
//bb_error_msg("[%d] fork in generate_stream_from_string:"
//		" G.count_SIGCHLD:%d G.handled_SIGCHLD:%d",
//		getpid(), G.count_SIGCHLD, G.handled_SIGCHLD);
# endif
	enable_restore_tty_pgrp_on_exit();
# if !BB_MMU
	free(to_free);
# endif
	close(channel[1]);
	close_on_exec_on(channel[0]);
	return xfdopen_for_read(channel[0]);
}

/* Return code is exit status of the process that is run. */
static int process_command_subs(o_string *dest, const char *s)
{
	FILE *fp;
	struct in_str pipe_str;
	pid_t pid;
	int status, ch, eol_cnt;

	fp = generate_stream_from_string(s, &pid);

	/* Now send results of command back into original context */
	setup_file_in_str(&pipe_str, fp);
	eol_cnt = 0;
	while ((ch = i_getch(&pipe_str)) != EOF) {
		if (ch == '\n') {
			eol_cnt++;
			continue;
		}
		while (eol_cnt) {
			o_addchr(dest, '\n');
			eol_cnt--;
		}
		o_addQchr(dest, ch);
	}

	debug_printf("done reading from `cmd` pipe, closing it\n");
	fclose(fp);
	/* We need to extract exitcode. Test case
	 * "true; echo `sleep 1; false` $?"
	 * should print 1 */
	safe_waitpid(pid, &status, 0);
	debug_printf("child exited. returning its exitcode:%d\n", WEXITSTATUS(status));
	return WEXITSTATUS(status);
}
#endif /* ENABLE_HUSH_TICK */


static void setup_heredoc(struct redir_struct *redir)
{
	struct fd_pair pair;
	pid_t pid;
	int len, written;
	/* the _body_ of heredoc (misleading field name) */
	const char *heredoc = redir->rd_filename;
	char *expanded;
#if !BB_MMU
	char **to_free;
#endif

	expanded = NULL;
	if (!(redir->rd_dup & HEREDOC_QUOTED)) {
		expanded = encode_then_expand_string(heredoc, /*process_bkslash:*/ 1, /*unbackslash:*/ 1);
		if (expanded)
			heredoc = expanded;
	}
	len = strlen(heredoc);

	close(redir->rd_fd); /* often saves dup2+close in xmove_fd */
	xpiped_pair(pair);
	xmove_fd(pair.rd, redir->rd_fd);

	/* Try writing without forking. Newer kernels have
	 * dynamically growing pipes. Must use non-blocking write! */
	ndelay_on(pair.wr);
	while (1) {
		written = write(pair.wr, heredoc, len);
		if (written <= 0)
			break;
		len -= written;
		if (len == 0) {
			close(pair.wr);
			free(expanded);
			return;
		}
		heredoc += written;
	}
	ndelay_off(pair.wr);

	/* Okay, pipe buffer was not big enough */
	/* Note: we must not create a stray child (bastard? :)
	 * for the unsuspecting parent process. Child creates a grandchild
	 * and exits before parent execs the process which consumes heredoc
	 * (that exec happens after we return from this function) */
#if !BB_MMU
	to_free = NULL;
#endif
	pid = xvfork();
	if (pid == 0) {
		/* child */
		disable_restore_tty_pgrp_on_exit();
		pid = BB_MMU ? xfork() : xvfork();
		if (pid != 0)
			_exit(0);
		/* grandchild */
		close(redir->rd_fd); /* read side of the pipe */
#if BB_MMU
		full_write(pair.wr, heredoc, len); /* may loop or block */
		_exit(0);
#else
		/* Delegate blocking writes to another process */
		xmove_fd(pair.wr, STDOUT_FILENO);
		re_execute_shell(&to_free, heredoc, NULL, NULL, NULL);
#endif
	}
	/* parent */
#if ENABLE_HUSH_FAST
	G.count_SIGCHLD++;
//bb_error_msg("[%d] fork in setup_heredoc: G.count_SIGCHLD:%d G.handled_SIGCHLD:%d", getpid(), G.count_SIGCHLD, G.handled_SIGCHLD);
#endif
	enable_restore_tty_pgrp_on_exit();
#if !BB_MMU
	free(to_free);
#endif
	close(pair.wr);
	free(expanded);
	wait(NULL); /* wait till child has died */
}

/* squirrel != NULL means we squirrel away copies of stdin, stdout,
 * and stderr if they are redirected. */
static int setup_redirects(struct command *prog, int squirrel[])
{
	int openfd, mode;
	struct redir_struct *redir;

	for (redir = prog->redirects; redir; redir = redir->next) {
		if (redir->rd_type == REDIRECT_HEREDOC2) {
			/* rd_fd<<HERE case */
			if (squirrel && redir->rd_fd < 3
			 && squirrel[redir->rd_fd] < 0
			) {
				squirrel[redir->rd_fd] = dup(redir->rd_fd);
			}
			/* for REDIRECT_HEREDOC2, rd_filename holds _contents_
			 * of the heredoc */
			debug_printf_parse("set heredoc '%s'\n",
					redir->rd_filename);
			setup_heredoc(redir);
			continue;
		}

		if (redir->rd_dup == REDIRFD_TO_FILE) {
			/* rd_fd<*>file case (<*> is <,>,>>,<>) */
			char *p;
			if (redir->rd_filename == NULL) {
				/* Something went wrong in the parse.
				 * Pretend it didn't happen */
				bb_error_msg("bug in redirect parse");
				continue;
			}
			mode = redir_table[redir->rd_type].mode;
			p = expand_string_to_string(redir->rd_filename, /*unbackslash:*/ 1);
			openfd = open_or_warn(p, mode);
			free(p);
			if (openfd < 0) {
			/* this could get lost if stderr has been redirected, but
			 * bash and ash both lose it as well (though zsh doesn't!) */
//what the above comment tries to say?
				return 1;
			}
		} else {
			/* rd_fd<*>rd_dup or rd_fd<*>- cases */
			openfd = redir->rd_dup;
		}

		if (openfd != redir->rd_fd) {
			if (squirrel && redir->rd_fd < 3
			 && squirrel[redir->rd_fd] < 0
			) {
				squirrel[redir->rd_fd] = dup(redir->rd_fd);
			}
			if (openfd == REDIRFD_CLOSE) {
				/* "n>-" means "close me" */
				close(redir->rd_fd);
			} else {
				xdup2(openfd, redir->rd_fd);
				if (redir->rd_dup == REDIRFD_TO_FILE)
					close(openfd);
			}
		}
	}
	return 0;
}

static void restore_redirects(int squirrel[])
{
	int i, fd;
	for (i = 0; i < 3; i++) {
		fd = squirrel[i];
		if (fd != -1) {
			/* We simply die on error */
			xmove_fd(fd, i);
		}
	}
}

static char *find_in_path(const char *arg)
{
	char *ret = NULL;
	const char *PATH = get_local_var_value("PATH");

	if (!PATH)
		return NULL;

	while (1) {
		const char *end = strchrnul(PATH, ':');
		int sz = end - PATH; /* must be int! */

		free(ret);
		if (sz != 0) {
			ret = xasprintf("%.*s/%s", sz, PATH, arg);
		} else {
			/* We have xxx::yyyy in $PATH,
			 * it means "use current dir" */
			ret = xstrdup(arg);
		}
		if (access(ret, F_OK) == 0)
			break;

		if (*end == '\0') {
			free(ret);
			return NULL;
		}
		PATH = end + 1;
	}

	return ret;
}

static const struct built_in_command *find_builtin_helper(const char *name,
		const struct built_in_command *x,
		const struct built_in_command *end)
{
	while (x != end) {
		if (strcmp(name, x->b_cmd) != 0) {
			x++;
			continue;
		}
		debug_printf_exec("found builtin '%s'\n", name);
		return x;
	}
	return NULL;
}
static const struct built_in_command *find_builtin1(const char *name)
{
	return find_builtin_helper(name, bltins1, &bltins1[ARRAY_SIZE(bltins1)]);
}
static const struct built_in_command *find_builtin(const char *name)
{
	const struct built_in_command *x = find_builtin1(name);
	if (x)
		return x;
	return find_builtin_helper(name, bltins2, &bltins2[ARRAY_SIZE(bltins2)]);
}

#if ENABLE_HUSH_FUNCTIONS
static struct function **find_function_slot(const char *name)
{
	struct function **funcpp = &G.top_func;
	while (*funcpp) {
		if (strcmp(name, (*funcpp)->name) == 0) {
			break;
		}
		funcpp = &(*funcpp)->next;
	}
	return funcpp;
}

static const struct function *find_function(const char *name)
{
	const struct function *funcp = *find_function_slot(name);
	if (funcp)
		debug_printf_exec("found function '%s'\n", name);
	return funcp;
}

/* Note: takes ownership on name ptr */
static struct function *new_function(char *name)
{
	struct function **funcpp = find_function_slot(name);
	struct function *funcp = *funcpp;

	if (funcp != NULL) {
		struct command *cmd = funcp->parent_cmd;
		debug_printf_exec("func %p parent_cmd %p\n", funcp, cmd);
		if (!cmd) {
			debug_printf_exec("freeing & replacing function '%s'\n", funcp->name);
			free(funcp->name);
			/* Note: if !funcp->body, do not free body_as_string!
			 * This is a special case of "-F name body" function:
			 * body_as_string was not malloced! */
			if (funcp->body) {
				free_pipe_list(funcp->body);
# if !BB_MMU
				free(funcp->body_as_string);
# endif
			}
		} else {
			debug_printf_exec("reinserting in tree & replacing function '%s'\n", funcp->name);
			cmd->argv[0] = funcp->name;
			cmd->group = funcp->body;
# if !BB_MMU
			cmd->group_as_string = funcp->body_as_string;
# endif
		}
	} else {
		debug_printf_exec("remembering new function '%s'\n", name);
		funcp = *funcpp = xzalloc(sizeof(*funcp));
		/*funcp->next = NULL;*/
	}

	funcp->name = name;
	return funcp;
}

static void unset_func(const char *name)
{
	struct function **funcpp = find_function_slot(name);
	struct function *funcp = *funcpp;

	if (funcp != NULL) {
		debug_printf_exec("freeing function '%s'\n", funcp->name);
		*funcpp = funcp->next;
		/* funcp is unlinked now, deleting it.
		 * Note: if !funcp->body, the function was created by
		 * "-F name body", do not free ->body_as_string
		 * and ->name as they were not malloced. */
		if (funcp->body) {
			free_pipe_list(funcp->body);
			free(funcp->name);
# if !BB_MMU
			free(funcp->body_as_string);
# endif
		}
		free(funcp);
	}
}

# if BB_MMU
#define exec_function(to_free, funcp, argv) \
	exec_function(funcp, argv)
# endif
static void exec_function(char ***to_free,
		const struct function *funcp,
		char **argv) NORETURN;
static void exec_function(char ***to_free,
		const struct function *funcp,
		char **argv)
{
# if BB_MMU
	int n = 1;

	argv[0] = G.global_argv[0];
	G.global_argv = argv;
	while (*++argv)
		n++;
	G.global_argc = n;
	/* On MMU, funcp->body is always non-NULL */
	n = run_list(funcp->body);
	fflush_all();
	_exit(n);
# else
	re_execute_shell(to_free,
			funcp->body_as_string,
			G.global_argv[0],
			argv + 1,
			NULL);
# endif
}

static int run_function(const struct function *funcp, char **argv)
{
	int rc;
	save_arg_t sv;
	smallint sv_flg;

	save_and_replace_G_args(&sv, argv);

	/* "we are in function, ok to use return" */
	sv_flg = G.flag_return_in_progress;
	G.flag_return_in_progress = -1;
# if ENABLE_HUSH_LOCAL
	G.func_nest_level++;
# endif

	/* On MMU, funcp->body is always non-NULL */
# if !BB_MMU
	if (!funcp->body) {
		/* Function defined by -F */
		parse_and_run_string(funcp->body_as_string);
		rc = G.last_exitcode;
	} else
# endif
	{
		rc = run_list(funcp->body);
	}

# if ENABLE_HUSH_LOCAL
	{
		struct variable *var;
		struct variable **var_pp;

		var_pp = &G.top_var;
		while ((var = *var_pp) != NULL) {
			if (var->func_nest_level < G.func_nest_level) {
				var_pp = &var->next;
				continue;
			}
			/* Unexport */
			if (var->flg_export)
				bb_unsetenv(var->varstr);
			/* Remove from global list */
			*var_pp = var->next;
			/* Free */
			if (!var->max_len)
				free(var->varstr);
			free(var);
		}
		G.func_nest_level--;
	}
# endif
	G.flag_return_in_progress = sv_flg;

	restore_G_args(&sv, argv);

	return rc;
}
#endif /* ENABLE_HUSH_FUNCTIONS */


#if BB_MMU
#define exec_builtin(to_free, x, argv) \
	exec_builtin(x, argv)
#else
#define exec_builtin(to_free, x, argv) \
	exec_builtin(to_free, argv)
#endif
static void exec_builtin(char ***to_free,
		const struct built_in_command *x,
		char **argv) NORETURN;
static void exec_builtin(char ***to_free,
		const struct built_in_command *x,
		char **argv)
{
#if BB_MMU
	int rcode;
	fflush_all();
	rcode = x->b_function(argv);
	fflush_all();
	_exit(rcode);
#else
	fflush_all();
	/* On NOMMU, we must never block!
	 * Example: { sleep 99 | read line; } & echo Ok
	 */
	re_execute_shell(to_free,
			argv[0],
			G.global_argv[0],
			G.global_argv + 1,
			argv);
#endif
}


static void execvp_or_die(char **argv) NORETURN;
static void execvp_or_die(char **argv)
{
	debug_printf_exec("execing '%s'\n", argv[0]);
	/* Don't propagate SIG_IGN to the child */
	if (SPECIAL_JOBSTOP_SIGS != 0)
		switch_off_special_sigs(G.special_sig_mask & SPECIAL_JOBSTOP_SIGS);
	execvp(argv[0], argv);
	bb_perror_msg("can't execute '%s'", argv[0]);
	_exit(127); /* bash compat */
}

#if ENABLE_HUSH_MODE_X
static void dump_cmd_in_x_mode(char **argv)
{
	if (G_x_mode && argv) {
		/* We want to output the line in one write op */
		char *buf, *p;
		int len;
		int n;

		len = 3;
		n = 0;
		while (argv[n])
			len += strlen(argv[n++]) + 1;
		buf = xmalloc(len);
		buf[0] = '+';
		p = buf + 1;
		n = 0;
		while (argv[n])
			p += sprintf(p, " %s", argv[n++]);
		*p++ = '\n';
		*p = '\0';
		fputs(buf, stderr);
		free(buf);
	}
}
#else
# define dump_cmd_in_x_mode(argv) ((void)0)
#endif

#if BB_MMU
#define pseudo_exec_argv(nommu_save, argv, assignment_cnt, argv_expanded) \
	pseudo_exec_argv(argv, assignment_cnt, argv_expanded)
#define pseudo_exec(nommu_save, command, argv_expanded) \
	pseudo_exec(command, argv_expanded)
#endif

/* Called after [v]fork() in run_pipe, or from builtin_exec.
 * Never returns.
 * Don't exit() here.  If you don't exec, use _exit instead.
 * The at_exit handlers apparently confuse the calling process,
 * in particular stdin handling. Not sure why? -- because of vfork! (vda)
 */
static void pseudo_exec_argv(nommu_save_t *nommu_save,
		char **argv, int assignment_cnt,
		char **argv_expanded) NORETURN;
static NOINLINE void pseudo_exec_argv(nommu_save_t *nommu_save,
		char **argv, int assignment_cnt,
		char **argv_expanded)
{
	char **new_env;

	new_env = expand_assignments(argv, assignment_cnt);
	dump_cmd_in_x_mode(new_env);

	if (!argv[assignment_cnt]) {
		/* Case when we are here: ... | var=val | ...
		 * (note that we do not exit early, i.e., do not optimize out
		 * expand_assignments(): think about ... | var=`sleep 1` | ...
		 */
		free_strings(new_env);
		_exit(EXIT_SUCCESS);
	}

#if BB_MMU
	set_vars_and_save_old(new_env);
	free(new_env); /* optional */
	/* we can also destroy set_vars_and_save_old's return value,
	 * to save memory */
#else
	nommu_save->new_env = new_env;
	nommu_save->old_vars = set_vars_and_save_old(new_env);
#endif

	if (argv_expanded) {
		argv = argv_expanded;
	} else {
		argv = expand_strvec_to_strvec(argv + assignment_cnt);
#if !BB_MMU
		nommu_save->argv = argv;
#endif
	}
	dump_cmd_in_x_mode(argv);

#if ENABLE_FEATURE_SH_STANDALONE || BB_MMU
	if (strchr(argv[0], '/') != NULL)
		goto skip;
#endif

	/* Check if the command matches any of the builtins.
	 * Depending on context, this might be redundant.  But it's
	 * easier to waste a few CPU cycles than it is to figure out
	 * if this is one of those cases.
	 */
	{
		/* On NOMMU, it is more expensive to re-execute shell
		 * just in order to run echo or test builtin.
		 * It's better to skip it here and run corresponding
		 * non-builtin later. */
		const struct built_in_command *x;
		x = BB_MMU ? find_builtin(argv[0]) : find_builtin1(argv[0]);
		if (x) {
			exec_builtin(&nommu_save->argv_from_re_execing, x, argv);
		}
	}
#if ENABLE_HUSH_FUNCTIONS
	/* Check if the command matches any functions */
	{
		const struct function *funcp = find_function(argv[0]);
		if (funcp) {
			exec_function(&nommu_save->argv_from_re_execing, funcp, argv);
		}
	}
#endif

#if ENABLE_FEATURE_SH_STANDALONE
	/* Check if the command matches any busybox applets */
	{
		int a = find_applet_by_name(argv[0]);
		if (a >= 0) {
# if BB_MMU /* see above why on NOMMU it is not allowed */
			if (APPLET_IS_NOEXEC(a)) {
				debug_printf_exec("running applet '%s'\n", argv[0]);
				run_applet_no_and_exit(a, argv);
			}
# endif
			/* Re-exec ourselves */
			debug_printf_exec("re-execing applet '%s'\n", argv[0]);
			/* Don't propagate SIG_IGN to the child */
			if (SPECIAL_JOBSTOP_SIGS != 0)
				switch_off_special_sigs(G.special_sig_mask & SPECIAL_JOBSTOP_SIGS);
			execv(bb_busybox_exec_path, argv);
			/* If they called chroot or otherwise made the binary no longer
			 * executable, fall through */
		}
	}
#endif

#if ENABLE_FEATURE_SH_STANDALONE || BB_MMU
 skip:
#endif
	execvp_or_die(argv);
}

/* Called after [v]fork() in run_pipe
 */
static void pseudo_exec(nommu_save_t *nommu_save,
		struct command *command,
		char **argv_expanded) NORETURN;
static void pseudo_exec(nommu_save_t *nommu_save,
		struct command *command,
		char **argv_expanded)
{
	if (command->argv) {
		pseudo_exec_argv(nommu_save, command->argv,
				command->assignment_cnt, argv_expanded);
	}

	if (command->group) {
		/* Cases when we are here:
		 * ( list )
		 * { list } &
		 * ... | ( list ) | ...
		 * ... | { list } | ...
		 */
#if BB_MMU
		int rcode;
		debug_printf_exec("pseudo_exec: run_list\n");
		reset_traps_to_defaults();
		rcode = run_list(command->group);
		/* OK to leak memory by not calling free_pipe_list,
		 * since this process is about to exit */
		_exit(rcode);
#else
		re_execute_shell(&nommu_save->argv_from_re_execing,
				command->group_as_string,
				G.global_argv[0],
				G.global_argv + 1,
				NULL);
#endif
	}

	/* Case when we are here: ... | >file */
	debug_printf_exec("pseudo_exec'ed null command\n");
	_exit(EXIT_SUCCESS);
}

#if ENABLE_HUSH_JOB
static const char *get_cmdtext(struct pipe *pi)
{
	char **argv;
	char *p;
	int len;

	/* This is subtle. ->cmdtext is created only on first backgrounding.
	 * (Think "cat, <ctrl-z>, fg, <ctrl-z>, fg, <ctrl-z>...." here...)
	 * On subsequent bg argv is trashed, but we won't use it */
	if (pi->cmdtext)
		return pi->cmdtext;
	argv = pi->cmds[0].argv;
	if (!argv || !argv[0]) {
		pi->cmdtext = xzalloc(1);
		return pi->cmdtext;
	}

	len = 0;
	do {
		len += strlen(*argv) + 1;
	} while (*++argv);
	p = xmalloc(len);
	pi->cmdtext = p;
	argv = pi->cmds[0].argv;
	do {
		len = strlen(*argv);
		memcpy(p, *argv, len);
		p += len;
		*p++ = ' ';
	} while (*++argv);
	p[-1] = '\0';
	return pi->cmdtext;
}

static void insert_bg_job(struct pipe *pi)
{
	struct pipe *job, **jobp;
	int i;

	/* Linear search for the ID of the job to use */
	pi->jobid = 1;
	for (job = G.job_list; job; job = job->next)
		if (job->jobid >= pi->jobid)
			pi->jobid = job->jobid + 1;

	/* Add job to the list of running jobs */
	jobp = &G.job_list;
	while ((job = *jobp) != NULL)
		jobp = &job->next;
	job = *jobp = xmalloc(sizeof(*job));

	*job = *pi; /* physical copy */
	job->next = NULL;
	job->cmds = xzalloc(sizeof(pi->cmds[0]) * pi->num_cmds);
	/* Cannot copy entire pi->cmds[] vector! This causes double frees */
	for (i = 0; i < pi->num_cmds; i++) {
		job->cmds[i].pid = pi->cmds[i].pid;
		/* all other fields are not used and stay zero */
	}
	job->cmdtext = xstrdup(get_cmdtext(pi));

	if (G_interactive_fd)
		printf("[%d] %d %s\n", job->jobid, job->cmds[0].pid, job->cmdtext);
	G.last_jobid = job->jobid;
}

static void remove_bg_job(struct pipe *pi)
{
	struct pipe *prev_pipe;

	if (pi == G.job_list) {
		G.job_list = pi->next;
	} else {
		prev_pipe = G.job_list;
		while (prev_pipe->next != pi)
			prev_pipe = prev_pipe->next;
		prev_pipe->next = pi->next;
	}
	if (G.job_list)
		G.last_jobid = G.job_list->jobid;
	else
		G.last_jobid = 0;
}

/* Remove a backgrounded job */
static void delete_finished_bg_job(struct pipe *pi)
{
	remove_bg_job(pi);
	free_pipe(pi);
}
#endif /* JOB */

/* Check to see if any processes have exited -- if they
 * have, figure out why and see if a job has completed */
static int checkjobs(struct pipe *fg_pipe)
{
	int attributes;
	int status;
#if ENABLE_HUSH_JOB
	struct pipe *pi;
#endif
	pid_t childpid;
	int rcode = 0;

	debug_printf_jobs("checkjobs %p\n", fg_pipe);

	attributes = WUNTRACED;
	if (fg_pipe == NULL)
		attributes |= WNOHANG;

	errno = 0;
#if ENABLE_HUSH_FAST
	if (G.handled_SIGCHLD == G.count_SIGCHLD) {
//bb_error_msg("[%d] checkjobs: G.count_SIGCHLD:%d G.handled_SIGCHLD:%d children?:%d fg_pipe:%p",
//getpid(), G.count_SIGCHLD, G.handled_SIGCHLD, G.we_have_children, fg_pipe);
		/* There was neither fork nor SIGCHLD since last waitpid */
		/* Avoid doing waitpid syscall if possible */
		if (!G.we_have_children) {
			errno = ECHILD;
			return -1;
		}
		if (fg_pipe == NULL) { /* is WNOHANG set? */
			/* We have children, but they did not exit
			 * or stop yet (we saw no SIGCHLD) */
			return 0;
		}
		/* else: !WNOHANG, waitpid will block, can't short-circuit */
	}
#endif

/* Do we do this right?
 * bash-3.00# sleep 20 | false
 * <ctrl-Z pressed>
 * [3]+  Stopped          sleep 20 | false
 * bash-3.00# echo $?
 * 1   <========== bg pipe is not fully done, but exitcode is already known!
 * [hush 1.14.0: yes we do it right]
 */
 wait_more:
	while (1) {
		int i;
		int dead;

#if ENABLE_HUSH_FAST
		i = G.count_SIGCHLD;
#endif
		childpid = waitpid(-1, &status, attributes);
		if (childpid <= 0) {
			if (childpid && errno != ECHILD)
				bb_perror_msg("waitpid");
#if ENABLE_HUSH_FAST
			else { /* Until next SIGCHLD, waitpid's are useless */
				G.we_have_children = (childpid == 0);
				G.handled_SIGCHLD = i;
//bb_error_msg("[%d] checkjobs: waitpid returned <= 0, G.count_SIGCHLD:%d G.handled_SIGCHLD:%d", getpid(), G.count_SIGCHLD, G.handled_SIGCHLD);
			}
#endif
			break;
		}
		dead = WIFEXITED(status) || WIFSIGNALED(status);

#if DEBUG_JOBS
		if (WIFSTOPPED(status))
			debug_printf_jobs("pid %d stopped by sig %d (exitcode %d)\n",
					childpid, WSTOPSIG(status), WEXITSTATUS(status));
		if (WIFSIGNALED(status))
			debug_printf_jobs("pid %d killed by sig %d (exitcode %d)\n",
					childpid, WTERMSIG(status), WEXITSTATUS(status));
		if (WIFEXITED(status))
			debug_printf_jobs("pid %d exited, exitcode %d\n",
					childpid, WEXITSTATUS(status));
#endif
		/* Were we asked to wait for fg pipe? */
		if (fg_pipe) {
			i = fg_pipe->num_cmds;
			while (--i >= 0) {
				debug_printf_jobs("check pid %d\n", fg_pipe->cmds[i].pid);
				if (fg_pipe->cmds[i].pid != childpid)
					continue;
				if (dead) {
					int ex;
					fg_pipe->cmds[i].pid = 0;
					fg_pipe->alive_cmds--;
					ex = WEXITSTATUS(status);
					/* bash prints killer signal's name for *last*
					 * process in pipe (prints just newline for SIGINT/SIGPIPE).
					 * Mimic this. Example: "sleep 5" + (^\ or kill -QUIT)
					 */
					if (WIFSIGNALED(status)) {
						int sig = WTERMSIG(status);
						if (i == fg_pipe->num_cmds-1)
							/* TODO: use strsignal() instead for bash compat? but that's bloat... */
							puts(sig == SIGINT || sig == SIGPIPE ? "" : get_signame(sig));
						/* TODO: if (WCOREDUMP(status)) + " (core dumped)"; */
						/* TODO: MIPS has 128 sigs (1..128), what if sig==128 here?
						 * Maybe we need to use sig | 128? */
						ex = sig + 128;
					}
					fg_pipe->cmds[i].cmd_exitcode = ex;
				} else {
					fg_pipe->stopped_cmds++;
				}
				debug_printf_jobs("fg_pipe: alive_cmds %d stopped_cmds %d\n",
						fg_pipe->alive_cmds, fg_pipe->stopped_cmds);
				if (fg_pipe->alive_cmds == fg_pipe->stopped_cmds) {
					/* All processes in fg pipe have exited or stopped */
					i = fg_pipe->num_cmds;
					while (--i >= 0) {
						rcode = fg_pipe->cmds[i].cmd_exitcode;
						/* usually last process gives overall exitstatus,
						 * but with "set -o pipefail", last *failed* process does */
						if (G.o_opt[OPT_O_PIPEFAIL] == 0 || rcode != 0)
							break;
					}
					IF_HAS_KEYWORDS(if (fg_pipe->pi_inverted) rcode = !rcode;)
/* Note: *non-interactive* bash does not continue if all processes in fg pipe
 * are stopped. Testcase: "cat | cat" in a script (not on command line!)
 * and "killall -STOP cat" */
					if (G_interactive_fd) {
#if ENABLE_HUSH_JOB
						if (fg_pipe->alive_cmds != 0)
							insert_bg_job(fg_pipe);
#endif
						return rcode;
					}
					if (fg_pipe->alive_cmds == 0)
						return rcode;
				}
				/* There are still running processes in the fg pipe */
				goto wait_more; /* do waitpid again */
			}
			/* it wasnt fg_pipe, look for process in bg pipes */
		}

#if ENABLE_HUSH_JOB
		/* We asked to wait for bg or orphaned children */
		/* No need to remember exitcode in this case */
		for (pi = G.job_list; pi; pi = pi->next) {
			for (i = 0; i < pi->num_cmds; i++) {
				if (pi->cmds[i].pid == childpid)
					goto found_pi_and_prognum;
			}
		}
		/* Happens when shell is used as init process (init=/bin/sh) */
		debug_printf("checkjobs: pid %d was not in our list!\n", childpid);
		continue; /* do waitpid again */

 found_pi_and_prognum:
		if (dead) {
			/* child exited */
			pi->cmds[i].pid = 0;
			pi->alive_cmds--;
			if (!pi->alive_cmds) {
				if (G_interactive_fd)
					printf(JOB_STATUS_FORMAT, pi->jobid,
							"Done", pi->cmdtext);
				delete_finished_bg_job(pi);
			}
		} else {
			/* child stopped */
			pi->stopped_cmds++;
		}
#endif
	} /* while (waitpid succeeds)... */

	return rcode;
}

#if ENABLE_HUSH_JOB
static int checkjobs_and_fg_shell(struct pipe *fg_pipe)
{
	pid_t p;
	int rcode = checkjobs(fg_pipe);
	if (G_saved_tty_pgrp) {
		/* Job finished, move the shell to the foreground */
		p = getpgrp(); /* our process group id */
		debug_printf_jobs("fg'ing ourself: getpgrp()=%d\n", (int)p);
		tcsetpgrp(G_interactive_fd, p);
	}
	return rcode;
}
#endif

/* Start all the jobs, but don't wait for anything to finish.
 * See checkjobs().
 *
 * Return code is normally -1, when the caller has to wait for children
 * to finish to determine the exit status of the pipe.  If the pipe
 * is a simple builtin command, however, the action is done by the
 * time run_pipe returns, and the exit code is provided as the
 * return value.
 *
 * Returns -1 only if started some children. IOW: we have to
 * mask out retvals of builtins etc with 0xff!
 *
 * The only case when we do not need to [v]fork is when the pipe
 * is single, non-backgrounded, non-subshell command. Examples:
 * cmd ; ...   { list } ; ...
 * cmd && ...  { list } && ...
 * cmd || ...  { list } || ...
 * If it is, then we can run cmd as a builtin, NOFORK,
 * or (if SH_STANDALONE) an applet, and we can run the { list }
 * with run_list. If it isn't one of these, we fork and exec cmd.
 *
 * Cases when we must fork:
 * non-single:   cmd | cmd
 * backgrounded: cmd &     { list } &
 * subshell:     ( list ) [&]
 */
#if !ENABLE_HUSH_MODE_X
#define redirect_and_varexp_helper(new_env_p, old_vars_p, command, squirrel, argv_expanded) \
	redirect_and_varexp_helper(new_env_p, old_vars_p, command, squirrel)
#endif
static int redirect_and_varexp_helper(char ***new_env_p,
		struct variable **old_vars_p,
		struct command *command,
		int squirrel[3],
		char **argv_expanded)
{
	/* setup_redirects acts on file descriptors, not FILEs.
	 * This is perfect for work that comes after exec().
	 * Is it really safe for inline use?  Experimentally,
	 * things seem to work. */
	int rcode = setup_redirects(command, squirrel);
	if (rcode == 0) {
		char **new_env = expand_assignments(command->argv, command->assignment_cnt);
		*new_env_p = new_env;
		dump_cmd_in_x_mode(new_env);
		dump_cmd_in_x_mode(argv_expanded);
		if (old_vars_p)
			*old_vars_p = set_vars_and_save_old(new_env);
	}
	return rcode;
}
static NOINLINE int run_pipe(struct pipe *pi)
{
	static const char *const null_ptr = NULL;

	int cmd_no;
	int next_infd;
	struct command *command;
	char **argv_expanded;
	char **argv;
	/* it is not always needed, but we aim to smaller code */
	int squirrel[] = { -1, -1, -1 };
	int rcode;

	debug_printf_exec("run_pipe start: members:%d\n", pi->num_cmds);
	debug_enter();

	/* Testcase: set -- q w e; (IFS='' echo "$*"; IFS=''; echo "$*"); echo "$*"
	 * Result should be 3 lines: q w e, qwe, q w e
	 */
	G.ifs = get_local_var_value("IFS");
	if (!G.ifs)
		G.ifs = defifs;

	IF_HUSH_JOB(pi->pgrp = -1;)
	pi->stopped_cmds = 0;
	command = &pi->cmds[0];
	argv_expanded = NULL;

	if (pi->num_cmds != 1
	 || pi->followup == PIPE_BG
	 || command->cmd_type == CMD_SUBSHELL
	) {
		goto must_fork;
	}

	pi->alive_cmds = 1;

	debug_printf_exec(": group:%p argv:'%s'\n",
		command->group, command->argv ? command->argv[0] : "NONE");

	if (command->group) {
#if ENABLE_HUSH_FUNCTIONS
		if (command->cmd_type == CMD_FUNCDEF) {
			/* "executing" func () { list } */
			struct function *funcp;

			funcp = new_function(command->argv[0]);
			/* funcp->name is already set to argv[0] */
			funcp->body = command->group;
# if !BB_MMU
			funcp->body_as_string = command->group_as_string;
			command->group_as_string = NULL;
# endif
			command->group = NULL;
			command->argv[0] = NULL;
			debug_printf_exec("cmd %p has child func at %p\n", command, funcp);
			funcp->parent_cmd = command;
			command->child_func = funcp;

			debug_printf_exec("run_pipe: return EXIT_SUCCESS\n");
			debug_leave();
			return EXIT_SUCCESS;
		}
#endif
		/* { list } */
		debug_printf("non-subshell group\n");
		rcode = 1; /* exitcode if redir failed */
		if (setup_redirects(command, squirrel) == 0) {
			debug_printf_exec(": run_list\n");
			rcode = run_list(command->group) & 0xff;
		}
		restore_redirects(squirrel);
		IF_HAS_KEYWORDS(if (pi->pi_inverted) rcode = !rcode;)
		debug_leave();
		debug_printf_exec("run_pipe: return %d\n", rcode);
		return rcode;
	}

	argv = command->argv ? command->argv : (char **) &null_ptr;
	{
		const struct built_in_command *x;
#if ENABLE_HUSH_FUNCTIONS
		const struct function *funcp;
#else
		enum { funcp = 0 };
#endif
		char **new_env = NULL;
		struct variable *old_vars = NULL;

		if (argv[command->assignment_cnt] == NULL) {
			/* Assignments, but no command */
			/* Ensure redirects take effect (that is, create files).
			 * Try "a=t >file" */
#if 0 /* A few cases in testsuite fail with this code. FIXME */
			rcode = redirect_and_varexp_helper(&new_env, /*old_vars:*/ NULL, command, squirrel, /*argv_expanded:*/ NULL);
			/* Set shell variables */
			if (new_env) {
				argv = new_env;
				while (*argv) {
					set_local_var(*argv, /*exp:*/ 0, /*lvl:*/ 0, /*ro:*/ 0);
					/* Do we need to flag set_local_var() errors?
					 * "assignment to readonly var" and "putenv error"
					 */
					argv++;
				}
			}
			/* Redirect error sets $? to 1. Otherwise,
			 * if evaluating assignment value set $?, retain it.
			 * Try "false; q=`exit 2`; echo $?" - should print 2: */
			if (rcode == 0)
				rcode = G.last_exitcode;
			/* Exit, _skipping_ variable restoring code: */
			goto clean_up_and_ret0;

#else /* Older, bigger, but more correct code */

			rcode = setup_redirects(command, squirrel);
			restore_redirects(squirrel);
			/* Set shell variables */
			if (G_x_mode)
				bb_putchar_stderr('+');
			while (*argv) {
				char *p = expand_string_to_string(*argv, /*unbackslash:*/ 1);
				if (G_x_mode)
					fprintf(stderr, " %s", p);
				debug_printf_exec("set shell var:'%s'->'%s'\n",
						*argv, p);
				set_local_var(p, /*exp:*/ 0, /*lvl:*/ 0, /*ro:*/ 0);
				/* Do we need to flag set_local_var() errors?
				 * "assignment to readonly var" and "putenv error"
				 */
				argv++;
			}
			if (G_x_mode)
				bb_putchar_stderr('\n');
			/* Redirect error sets $? to 1. Otherwise,
			 * if evaluating assignment value set $?, retain it.
			 * Try "false; q=`exit 2`; echo $?" - should print 2: */
			if (rcode == 0)
				rcode = G.last_exitcode;
			IF_HAS_KEYWORDS(if (pi->pi_inverted) rcode = !rcode;)
			debug_leave();
			debug_printf_exec("run_pipe: return %d\n", rcode);
			return rcode;
#endif
		}

		/* Expand the rest into (possibly) many strings each */
#if ENABLE_HUSH_BASH_COMPAT
		if (command->cmd_type == CMD_SINGLEWORD_NOGLOB) {
			argv_expanded = expand_strvec_to_strvec_singleword_noglob(argv + command->assignment_cnt);
		} else
#endif
		{
			argv_expanded = expand_strvec_to_strvec(argv + command->assignment_cnt);
		}

		/* if someone gives us an empty string: `cmd with empty output` */
		if (!argv_expanded[0]) {
			free(argv_expanded);
			debug_leave();
			return G.last_exitcode;
		}

		x = find_builtin(argv_expanded[0]);
#if ENABLE_HUSH_FUNCTIONS
		funcp = NULL;
		if (!x)
			funcp = find_function(argv_expanded[0]);
#endif
		if (x || funcp) {
			if (!funcp) {
				if (x->b_function == builtin_exec && argv_expanded[1] == NULL) {
					debug_printf("exec with redirects only\n");
					rcode = setup_redirects(command, NULL);
					goto clean_up_and_ret1;
				}
			}
			rcode = redirect_and_varexp_helper(&new_env, &old_vars, command, squirrel, argv_expanded);
			if (rcode == 0) {
				if (!funcp) {
					debug_printf_exec(": builtin '%s' '%s'...\n",
						x->b_cmd, argv_expanded[1]);
					fflush_all();
					rcode = x->b_function(argv_expanded) & 0xff;
					fflush_all();
				}
#if ENABLE_HUSH_FUNCTIONS
				else {
# if ENABLE_HUSH_LOCAL
					struct variable **sv;
					sv = G.shadowed_vars_pp;
					G.shadowed_vars_pp = &old_vars;
# endif
					debug_printf_exec(": function '%s' '%s'...\n",
						funcp->name, argv_expanded[1]);
					rcode = run_function(funcp, argv_expanded) & 0xff;
# if ENABLE_HUSH_LOCAL
					G.shadowed_vars_pp = sv;
# endif
				}
#endif
			}
 clean_up_and_ret:
			unset_vars(new_env);
			add_vars(old_vars);
/* clean_up_and_ret0: */
			restore_redirects(squirrel);
 clean_up_and_ret1:
			free(argv_expanded);
			IF_HAS_KEYWORDS(if (pi->pi_inverted) rcode = !rcode;)
			debug_leave();
			debug_printf_exec("run_pipe return %d\n", rcode);
			return rcode;
		}

		if (ENABLE_FEATURE_SH_NOFORK) {
			int n = find_applet_by_name(argv_expanded[0]);
			if (n >= 0 && APPLET_IS_NOFORK(n)) {
				rcode = redirect_and_varexp_helper(&new_env, &old_vars, command, squirrel, argv_expanded);
				if (rcode == 0) {
					debug_printf_exec(": run_nofork_applet '%s' '%s'...\n",
						argv_expanded[0], argv_expanded[1]);
					rcode = run_nofork_applet(n, argv_expanded);
				}
				goto clean_up_and_ret;
			}
		}
		/* It is neither builtin nor applet. We must fork. */
	}

 must_fork:
	/* NB: argv_expanded may already be created, and that
	 * might include `cmd` runs! Do not rerun it! We *must*
	 * use argv_expanded if it's non-NULL */

	/* Going to fork a child per each pipe member */
	pi->alive_cmds = 0;
	next_infd = 0;

	cmd_no = 0;
	while (cmd_no < pi->num_cmds) {
		struct fd_pair pipefds;
#if !BB_MMU
		volatile nommu_save_t nommu_save;
		nommu_save.new_env = NULL;
		nommu_save.old_vars = NULL;
		nommu_save.argv = NULL;
		nommu_save.argv_from_re_execing = NULL;
#endif
		command = &pi->cmds[cmd_no];
		cmd_no++;
		if (command->argv) {
			debug_printf_exec(": pipe member '%s' '%s'...\n",
					command->argv[0], command->argv[1]);
		} else {
			debug_printf_exec(": pipe member with no argv\n");
		}

		/* pipes are inserted between pairs of commands */
		pipefds.rd = 0;
		pipefds.wr = 1;
		if (cmd_no < pi->num_cmds)
			xpiped_pair(pipefds);

		command->pid = BB_MMU ? fork() : vfork();
		if (!command->pid) { /* child */
#if ENABLE_HUSH_JOB
			disable_restore_tty_pgrp_on_exit();
			CLEAR_RANDOM_T(&G.random_gen); /* or else $RANDOM repeats in child */

			/* Every child adds itself to new process group
			 * with pgid == pid_of_first_child_in_pipe */
			if (G.run_list_level == 1 && G_interactive_fd) {
				pid_t pgrp;
				pgrp = pi->pgrp;
				if (pgrp < 0) /* true for 1st process only */
					pgrp = getpid();
				if (setpgid(0, pgrp) == 0
				 && pi->followup != PIPE_BG
				 && G_saved_tty_pgrp /* we have ctty */
				) {
					/* We do it in *every* child, not just first,
					 * to avoid races */
					tcsetpgrp(G_interactive_fd, pgrp);
				}
			}
#endif
			if (pi->alive_cmds == 0 && pi->followup == PIPE_BG) {
				/* 1st cmd in backgrounded pipe
				 * should have its stdin /dev/null'ed */
				close(0);
				if (open(bb_dev_null, O_RDONLY))
					xopen("/", O_RDONLY);
			} else {
				xmove_fd(next_infd, 0);
			}
			xmove_fd(pipefds.wr, 1);
			if (pipefds.rd > 1)
				close(pipefds.rd);
			/* Like bash, explicit redirects override pipes,
			 * and the pipe fd is available for dup'ing. */
			if (setup_redirects(command, NULL))
				_exit(1);

			/* Stores to nommu_save list of env vars putenv'ed
			 * (NOMMU, on MMU we don't need that) */
			/* cast away volatility... */
			pseudo_exec((nommu_save_t*) &nommu_save, command, argv_expanded);
			/* pseudo_exec() does not return */
		}

		/* parent or error */
#if ENABLE_HUSH_FAST
		G.count_SIGCHLD++;
//bb_error_msg("[%d] fork in run_pipe: G.count_SIGCHLD:%d G.handled_SIGCHLD:%d", getpid(), G.count_SIGCHLD, G.handled_SIGCHLD);
#endif
		enable_restore_tty_pgrp_on_exit();
#if !BB_MMU
		/* Clean up after vforked child */
		free(nommu_save.argv);
		free(nommu_save.argv_from_re_execing);
		unset_vars(nommu_save.new_env);
		add_vars(nommu_save.old_vars);
#endif
		free(argv_expanded);
		argv_expanded = NULL;
		if (command->pid < 0) { /* [v]fork failed */
			/* Clearly indicate, was it fork or vfork */
			bb_perror_msg(BB_MMU ? "vfork"+1 : "vfork");
		} else {
			pi->alive_cmds++;
#if ENABLE_HUSH_JOB
			/* Second and next children need to know pid of first one */
			if (pi->pgrp < 0)
				pi->pgrp = command->pid;
#endif
		}

		if (cmd_no > 1)
			close(next_infd);
		if (cmd_no < pi->num_cmds)
			close(pipefds.wr);
		/* Pass read (output) pipe end to next iteration */
		next_infd = pipefds.rd;
	}

	if (!pi->alive_cmds) {
		debug_leave();
		debug_printf_exec("run_pipe return 1 (all forks failed, no children)\n");
		return 1;
	}

	debug_leave();
	debug_printf_exec("run_pipe return -1 (%u children started)\n", pi->alive_cmds);
	return -1;
}

/* NB: called by pseudo_exec, and therefore must not modify any
 * global data until exec/_exit (we can be a child after vfork!) */
static int run_list(struct pipe *pi)
{
#if ENABLE_HUSH_CASE
	char *case_word = NULL;
#endif
#if ENABLE_HUSH_LOOPS
	struct pipe *loop_top = NULL;
	char **for_lcur = NULL;
	char **for_list = NULL;
#endif
	smallint last_followup;
	smalluint rcode;
#if ENABLE_HUSH_IF || ENABLE_HUSH_CASE
	smalluint cond_code = 0;
#else
	enum { cond_code = 0 };
#endif
#if HAS_KEYWORDS
	smallint rword;      /* RES_foo */
	smallint last_rword; /* ditto */
#endif

	debug_printf_exec("run_list start lvl %d\n", G.run_list_level);
	debug_enter();

#if ENABLE_HUSH_LOOPS
	/* Check syntax for "for" */
	{
		struct pipe *cpipe;
		for (cpipe = pi; cpipe; cpipe = cpipe->next) {
			if (cpipe->res_word != RES_FOR && cpipe->res_word != RES_IN)
				continue;
			/* current word is FOR or IN (BOLD in comments below) */
			if (cpipe->next == NULL) {
				syntax_error("malformed for");
				debug_leave();
				debug_printf_exec("run_list lvl %d return 1\n", G.run_list_level);
				return 1;
			}
			/* "FOR v; do ..." and "for v IN a b; do..." are ok */
			if (cpipe->next->res_word == RES_DO)
				continue;
			/* next word is not "do". It must be "in" then ("FOR v in ...") */
			if (cpipe->res_word == RES_IN /* "for v IN a b; not_do..."? */
			 || cpipe->next->res_word != RES_IN /* FOR v not_do_and_not_in..."? */
			) {
				syntax_error("malformed for");
				debug_leave();
				debug_printf_exec("run_list lvl %d return 1\n", G.run_list_level);
				return 1;
			}
		}
	}
#endif

	/* Past this point, all code paths should jump to ret: label
	 * in order to return, no direct "return" statements please.
	 * This helps to ensure that no memory is leaked. */

#if ENABLE_HUSH_JOB
	G.run_list_level++;
#endif

#if HAS_KEYWORDS
	rword = RES_NONE;
	last_rword = RES_XXXX;
#endif
	last_followup = PIPE_SEQ;
	rcode = G.last_exitcode;

	/* Go through list of pipes, (maybe) executing them. */
	for (; pi; pi = IF_HUSH_LOOPS(rword == RES_DONE ? loop_top : ) pi->next) {
		if (G.flag_SIGINT)
			break;

		IF_HAS_KEYWORDS(rword = pi->res_word;)
		debug_printf_exec(": rword=%d cond_code=%d last_rword=%d\n",
				rword, cond_code, last_rword);
#if ENABLE_HUSH_LOOPS
		if ((rword == RES_WHILE || rword == RES_UNTIL || rword == RES_FOR)
		 && loop_top == NULL /* avoid bumping G.depth_of_loop twice */
		) {
			/* start of a loop: remember where loop starts */
			loop_top = pi;
			G.depth_of_loop++;
		}
#endif
		/* Still in the same "if...", "then..." or "do..." branch? */
		if (IF_HAS_KEYWORDS(rword == last_rword &&) 1) {
			if ((rcode == 0 && last_followup == PIPE_OR)
			 || (rcode != 0 && last_followup == PIPE_AND)
			) {
				/* It is "<true> || CMD" or "<false> && CMD"
				 * and we should not execute CMD */
				debug_printf_exec("skipped cmd because of || or &&\n");
				last_followup = pi->followup;
				goto dont_check_jobs_but_continue;
			}
		}
		last_followup = pi->followup;
		IF_HAS_KEYWORDS(last_rword = rword;)
#if ENABLE_HUSH_IF
		if (cond_code) {
			if (rword == RES_THEN) {
				/* if false; then ... fi has exitcode 0! */
				G.last_exitcode = rcode = EXIT_SUCCESS;
				/* "if <false> THEN cmd": skip cmd */
				continue;
			}
		} else {
			if (rword == RES_ELSE || rword == RES_ELIF) {
				/* "if <true> then ... ELSE/ELIF cmd":
				 * skip cmd and all following ones */
				break;
			}
		}
#endif
#if ENABLE_HUSH_LOOPS
		if (rword == RES_FOR) { /* && pi->num_cmds - always == 1 */
			if (!for_lcur) {
				/* first loop through for */

				static const char encoded_dollar_at[] ALIGN1 = {
					SPECIAL_VAR_SYMBOL, '@' | 0x80, SPECIAL_VAR_SYMBOL, '\0'
				}; /* encoded representation of "$@" */
				static const char *const encoded_dollar_at_argv[] = {
					encoded_dollar_at, NULL
				}; /* argv list with one element: "$@" */
				char **vals;

				vals = (char**)encoded_dollar_at_argv;
				if (pi->next->res_word == RES_IN) {
					/* if no variable values after "in" we skip "for" */
					if (!pi->next->cmds[0].argv) {
						G.last_exitcode = rcode = EXIT_SUCCESS;
						debug_printf_exec(": null FOR: exitcode EXIT_SUCCESS\n");
						break;
					}
					vals = pi->next->cmds[0].argv;
				} /* else: "for var; do..." -> assume "$@" list */
				/* create list of variable values */
				debug_print_strings("for_list made from", vals);
				for_list = expand_strvec_to_strvec(vals);
				for_lcur = for_list;
				debug_print_strings("for_list", for_list);
			}
			if (!*for_lcur) {
				/* "for" loop is over, clean up */
				free(for_list);
				for_list = NULL;
				for_lcur = NULL;
				break;
			}
			/* Insert next value from for_lcur */
			/* note: *for_lcur already has quotes removed, $var expanded, etc */
			set_local_var(xasprintf("%s=%s", pi->cmds[0].argv[0], *for_lcur++), /*exp:*/ 0, /*lvl:*/ 0, /*ro:*/ 0);
			continue;
		}
		if (rword == RES_IN) {
			continue; /* "for v IN list;..." - "in" has no cmds anyway */
		}
		if (rword == RES_DONE) {
			continue; /* "done" has no cmds too */
		}
#endif
#if ENABLE_HUSH_CASE
		if (rword == RES_CASE) {
			case_word = expand_strvec_to_string(pi->cmds->argv);
			continue;
		}
		if (rword == RES_MATCH) {
			char **argv;

			if (!case_word) /* "case ... matched_word) ... WORD)": we executed selected branch, stop */
				break;
			/* all prev words didn't match, does this one match? */
			argv = pi->cmds->argv;
			while (*argv) {
				char *pattern = expand_string_to_string(*argv, /*unbackslash:*/ 1);
				/* TODO: which FNM_xxx flags to use? */
				cond_code = (fnmatch(pattern, case_word, /*flags:*/ 0) != 0);
				free(pattern);
				if (cond_code == 0) { /* match! we will execute this branch */
					free(case_word); /* make future "word)" stop */
					case_word = NULL;
					break;
				}
				argv++;
			}
			continue;
		}
		if (rword == RES_CASE_BODY) { /* inside of a case branch */
			if (cond_code != 0)
				continue; /* not matched yet, skip this pipe */
		}
#endif
		/* Just pressing <enter> in shell should check for jobs.
		 * OTOH, in non-interactive shell this is useless
		 * and only leads to extra job checks */
		if (pi->num_cmds == 0) {
			if (G_interactive_fd)
				goto check_jobs_and_continue;
			continue;
		}

		/* After analyzing all keywords and conditions, we decided
		 * to execute this pipe. NB: have to do checkjobs(NULL)
		 * after run_pipe to collect any background children,
		 * even if list execution is to be stopped. */
		debug_printf_exec(": run_pipe with %d members\n", pi->num_cmds);
		{
			int r;
#if ENABLE_HUSH_LOOPS
			G.flag_break_continue = 0;
#endif
			rcode = r = run_pipe(pi); /* NB: rcode is a smallint */
			if (r != -1) {
				/* We ran a builtin, function, or group.
				 * rcode is already known
				 * and we don't need to wait for anything. */
				G.last_exitcode = rcode;
				debug_printf_exec(": builtin/func exitcode %d\n", rcode);
				check_and_run_traps();
#if ENABLE_HUSH_LOOPS
				/* Was it "break" or "continue"? */
				if (G.flag_break_continue) {
					smallint fbc = G.flag_break_continue;
					/* We might fall into outer *loop*,
					 * don't want to break it too */
					if (loop_top) {
						G.depth_break_continue--;
						if (G.depth_break_continue == 0)
							G.flag_break_continue = 0;
						/* else: e.g. "continue 2" should *break* once, *then* continue */
					} /* else: "while... do... { we are here (innermost list is not a loop!) };...done" */
					if (G.depth_break_continue != 0 || fbc == BC_BREAK) {
						checkjobs(NULL);
						break;
					}
					/* "continue": simulate end of loop */
					rword = RES_DONE;
					continue;
				}
#endif
#if ENABLE_HUSH_FUNCTIONS
				if (G.flag_return_in_progress == 1) {
					checkjobs(NULL);
					break;
				}
#endif
			} else if (pi->followup == PIPE_BG) {
				/* What does bash do with attempts to background builtins? */
				/* even bash 3.2 doesn't do that well with nested bg:
				 * try "{ { sleep 10; echo DEEP; } & echo HERE; } &".
				 * I'm NOT treating inner &'s as jobs */
				check_and_run_traps();
#if ENABLE_HUSH_JOB
				if (G.run_list_level == 1)
					insert_bg_job(pi);
#endif
				/* Last command's pid goes to $! */
				G.last_bg_pid = pi->cmds[pi->num_cmds - 1].pid;
				G.last_exitcode = rcode = EXIT_SUCCESS;
				debug_printf_exec(": cmd&: exitcode EXIT_SUCCESS\n");
			} else {
#if ENABLE_HUSH_JOB
				if (G.run_list_level == 1 && G_interactive_fd) {
					/* Waits for completion, then fg's main shell */
					rcode = checkjobs_and_fg_shell(pi);
					debug_printf_exec(": checkjobs_and_fg_shell exitcode %d\n", rcode);
					check_and_run_traps();
				} else
#endif
				{ /* This one just waits for completion */
					rcode = checkjobs(pi);
					debug_printf_exec(": checkjobs exitcode %d\n", rcode);
					check_and_run_traps();
				}
				G.last_exitcode = rcode;
			}
		}

		/* Analyze how result affects subsequent commands */
#if ENABLE_HUSH_IF
		if (rword == RES_IF || rword == RES_ELIF)
			cond_code = rcode;
#endif
 check_jobs_and_continue:
		checkjobs(NULL);
 dont_check_jobs_but_continue: ;
#if ENABLE_HUSH_LOOPS
		/* Beware of "while false; true; do ..."! */
		if (pi->next
		 && (pi->next->res_word == RES_DO || pi->next->res_word == RES_DONE)
		 /* check for RES_DONE is needed for "while ...; do \n done" case */
		) {
			if (rword == RES_WHILE) {
				if (rcode) {
					/* "while false; do...done" - exitcode 0 */
					G.last_exitcode = rcode = EXIT_SUCCESS;
					debug_printf_exec(": while expr is false: breaking (exitcode:EXIT_SUCCESS)\n");
					break;
				}
			}
			if (rword == RES_UNTIL) {
				if (!rcode) {
					debug_printf_exec(": until expr is true: breaking\n");
					break;
				}
			}
		}
#endif
	} /* for (pi) */

#if ENABLE_HUSH_JOB
	G.run_list_level--;
#endif
#if ENABLE_HUSH_LOOPS
	if (loop_top)
		G.depth_of_loop--;
	free(for_list);
#endif
#if ENABLE_HUSH_CASE
	free(case_word);
#endif
	debug_leave();
	debug_printf_exec("run_list lvl %d return %d\n", G.run_list_level + 1, rcode);
	return rcode;
}

/* Select which version we will use */
static int run_and_free_list(struct pipe *pi)
{
	int rcode = 0;
	debug_printf_exec("run_and_free_list entered\n");
	if (!G.o_opt[OPT_O_NOEXEC]) {
		debug_printf_exec(": run_list: 1st pipe with %d cmds\n", pi->num_cmds);
		rcode = run_list(pi);
	}
	/* free_pipe_list has the side effect of clearing memory.
	 * In the long run that function can be merged with run_list,
	 * but doing that now would hobble the debugging effort. */
	free_pipe_list(pi);
	debug_printf_exec("run_and_free_list return %d\n", rcode);
	return rcode;
}


static void install_sighandlers(unsigned mask)
{
	sighandler_t old_handler;
	unsigned sig = 0;
	while ((mask >>= 1) != 0) {
		sig++;
		if (!(mask & 1))
			continue;
		old_handler = install_sighandler(sig, pick_sighandler(sig));
		/* POSIX allows shell to re-enable SIGCHLD
		 * even if it was SIG_IGN on entry.
		 * Therefore we skip IGN check for it:
		 */
		if (sig == SIGCHLD)
			continue;
		if (old_handler == SIG_IGN) {
			/* oops... restore back to IGN, and record this fact */
			install_sighandler(sig, old_handler);
			if (!G.traps)
				G.traps = xzalloc(sizeof(G.traps[0]) * NSIG);
			free(G.traps[sig]);
			G.traps[sig] = xzalloc(1); /* == xstrdup(""); */
		}
	}
}

/* Called a few times only (or even once if "sh -c") */
static void install_special_sighandlers(void)
{
	unsigned mask;

	/* Which signals are shell-special? */
	mask = (1 << SIGQUIT) | (1 << SIGCHLD);
	if (G_interactive_fd) {
		mask |= SPECIAL_INTERACTIVE_SIGS;
		if (G_saved_tty_pgrp) /* we have ctty, job control sigs work */
			mask |= SPECIAL_JOBSTOP_SIGS;
	}
	/* Careful, do not re-install handlers we already installed */
	if (G.special_sig_mask != mask) {
		unsigned diff = mask & ~G.special_sig_mask;
		G.special_sig_mask = mask;
		install_sighandlers(diff);
	}
}

#if ENABLE_HUSH_JOB
/* helper */
/* Set handlers to restore tty pgrp and exit */
static void install_fatal_sighandlers(void)
{
	unsigned mask;

	/* We will restore tty pgrp on these signals */
	mask = 0
		+ (1 << SIGILL ) * HUSH_DEBUG
		+ (1 << SIGFPE ) * HUSH_DEBUG
		+ (1 << SIGBUS ) * HUSH_DEBUG
		+ (1 << SIGSEGV) * HUSH_DEBUG
		+ (1 << SIGTRAP) * HUSH_DEBUG
		+ (1 << SIGABRT)
	/* bash 3.2 seems to handle these just like 'fatal' ones */
		+ (1 << SIGPIPE)
		+ (1 << SIGALRM)
	/* if we are interactive, SIGHUP, SIGTERM and SIGINT are special sigs.
	 * if we aren't interactive... but in this case
	 * we never want to restore pgrp on exit, and this fn is not called
	 */
		/*+ (1 << SIGHUP )*/
		/*+ (1 << SIGTERM)*/
		/*+ (1 << SIGINT )*/
	;
	G_fatal_sig_mask = mask;

	install_sighandlers(mask);
}
#endif

static int set_mode(int state, char mode, const char *o_opt)
{
	int idx;
	switch (mode) {
	case 'n':
		G.o_opt[OPT_O_NOEXEC] = state;
		break;
	case 'x':
		IF_HUSH_MODE_X(G_x_mode = state;)
		break;
	case 'o':
		if (!o_opt) {
			/* "set -+o" without parameter.
			 * in bash, set -o produces this output:
			 *  pipefail        off
			 * and set +o:
			 *  set +o pipefail
			 * We always use the second form.
			 */
			const char *p = o_opt_strings;
			idx = 0;
			while (*p) {
				printf("set %co %s\n", (G.o_opt[idx] ? '-' : '+'), p);
				idx++;
				p += strlen(p) + 1;
			}
			break;
		}
		idx = index_in_strings(o_opt_strings, o_opt);
		if (idx >= 0) {
			G.o_opt[idx] = state;
			break;
		}
	default:
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int hush_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hush_main(int argc, char **argv)
{
	enum {
		OPT_login = (1 << 0),
	};
	unsigned flags;
	int opt;
	unsigned builtin_argc;
	char **e;
	struct variable *cur_var;
	struct variable *shell_ver;

	INIT_G();
	if (EXIT_SUCCESS != 0) /* if EXIT_SUCCESS == 0, it is already done */
		G.last_exitcode = EXIT_SUCCESS;

#if ENABLE_HUSH_FAST
	G.count_SIGCHLD++; /* ensure it is != G.handled_SIGCHLD */
#endif
#if !BB_MMU
	G.argv0_for_re_execing = argv[0];
#endif
	/* Deal with HUSH_VERSION */
	shell_ver = xzalloc(sizeof(*shell_ver));
	shell_ver->flg_export = 1;
	shell_ver->flg_read_only = 1;
	/* Code which handles ${var<op>...} needs writable values for all variables,
	 * therefore we xstrdup: */
	shell_ver->varstr = xstrdup(hush_version_str);
	/* Create shell local variables from the values
	 * currently living in the environment */
	debug_printf_env("unsetenv '%s'\n", "HUSH_VERSION");
	unsetenv("HUSH_VERSION"); /* in case it exists in initial env */
	G.top_var = shell_ver;
	cur_var = G.top_var;
	e = environ;
	if (e) while (*e) {
		char *value = strchr(*e, '=');
		if (value) { /* paranoia */
			cur_var->next = xzalloc(sizeof(*cur_var));
			cur_var = cur_var->next;
			cur_var->varstr = *e;
			cur_var->max_len = strlen(*e);
			cur_var->flg_export = 1;
		}
		e++;
	}
	/* (Re)insert HUSH_VERSION into env (AFTER we scanned the env!) */
	debug_printf_env("putenv '%s'\n", shell_ver->varstr);
	putenv(shell_ver->varstr);

	/* Export PWD */
	set_pwd_var(/*exp:*/ 1);

#if ENABLE_HUSH_BASH_COMPAT
	/* Set (but not export) HOSTNAME unless already set */
	if (!get_local_var_value("HOSTNAME")) {
		struct utsname uts;
		uname(&uts);
		set_local_var_from_halves("HOSTNAME", uts.nodename);
	}
	/* bash also exports SHLVL and _,
	 * and sets (but doesn't export) the following variables:
	 * BASH=/bin/bash
	 * BASH_VERSINFO=([0]="3" [1]="2" [2]="0" [3]="1" [4]="release" [5]="i386-pc-linux-gnu")
	 * BASH_VERSION='3.2.0(1)-release'
	 * HOSTTYPE=i386
	 * MACHTYPE=i386-pc-linux-gnu
	 * OSTYPE=linux-gnu
	 * PPID=<NNNNN> - we also do it elsewhere
	 * EUID=<NNNNN>
	 * UID=<NNNNN>
	 * GROUPS=()
	 * LINES=<NNN>
	 * COLUMNS=<NNN>
	 * BASH_ARGC=()
	 * BASH_ARGV=()
	 * BASH_LINENO=()
	 * BASH_SOURCE=()
	 * DIRSTACK=()
	 * PIPESTATUS=([0]="0")
	 * HISTFILE=/<xxx>/.bash_history
	 * HISTFILESIZE=500
	 * HISTSIZE=500
	 * MAILCHECK=60
	 * PATH=/usr/gnu/bin:/usr/local/bin:/bin:/usr/bin:.
	 * SHELL=/bin/bash
	 * SHELLOPTS=braceexpand:emacs:hashall:histexpand:history:interactive-comments:monitor
	 * TERM=dumb
	 * OPTERR=1
	 * OPTIND=1
	 * IFS=$' \t\n'
	 * PS1='\s-\v\$ '
	 * PS2='> '
	 * PS4='+ '
	 */
#endif

#if ENABLE_FEATURE_EDITING
	G.line_input_state = new_line_input_t(FOR_SHELL);
#endif

	/* Initialize some more globals to non-zero values */
	cmdedit_update_prompt();

	die_func = restore_ttypgrp_and__exit;

	/* Shell is non-interactive at first. We need to call
	 * install_special_sighandlers() if we are going to execute "sh <script>",
	 * "sh -c <cmds>" or login shell's /etc/profile and friends.
	 * If we later decide that we are interactive, we run install_special_sighandlers()
	 * in order to intercept (more) signals.
	 */

	/* Parse options */
	/* http://www.opengroup.org/onlinepubs/9699919799/utilities/sh.html */
	flags = (argv[0] && argv[0][0] == '-') ? OPT_login : 0;
	builtin_argc = 0;
	while (1) {
		opt = getopt(argc, argv, "+c:xinsl"
#if !BB_MMU
				"<:$:R:V:"
# if ENABLE_HUSH_FUNCTIONS
				"F:"
# endif
#endif
		);
		if (opt <= 0)
			break;
		switch (opt) {
		case 'c':
			/* Possibilities:
			 * sh ... -c 'script'
			 * sh ... -c 'script' ARG0 [ARG1...]
			 * On NOMMU, if builtin_argc != 0,
			 * sh ... -c 'builtin' BARGV... "" ARG0 [ARG1...]
			 * "" needs to be replaced with NULL
			 * and BARGV vector fed to builtin function.
			 * Note: the form without ARG0 never happens:
			 * sh ... -c 'builtin' BARGV... ""
			 */
			if (!G.root_pid) {
				G.root_pid = getpid();
				G.root_ppid = getppid();
			}
			G.global_argv = argv + optind;
			G.global_argc = argc - optind;
			if (builtin_argc) {
				/* -c 'builtin' [BARGV...] "" ARG0 [ARG1...] */
				const struct built_in_command *x;

				install_special_sighandlers();
				x = find_builtin(optarg);
				if (x) { /* paranoia */
					G.global_argc -= builtin_argc; /* skip [BARGV...] "" */
					G.global_argv += builtin_argc;
					G.global_argv[-1] = NULL; /* replace "" */
					fflush_all();
					G.last_exitcode = x->b_function(argv + optind - 1);
				}
				goto final_return;
			}
			if (!G.global_argv[0]) {
				/* -c 'script' (no params): prevent empty $0 */
				G.global_argv--; /* points to argv[i] of 'script' */
				G.global_argv[0] = argv[0];
				G.global_argc++;
			} /* else -c 'script' ARG0 [ARG1...]: $0 is ARG0 */
			install_special_sighandlers();
			parse_and_run_string(optarg);
			goto final_return;
		case 'i':
			/* Well, we cannot just declare interactiveness,
			 * we have to have some stuff (ctty, etc) */
			/* G_interactive_fd++; */
			break;
		case 's':
			/* "-s" means "read from stdin", but this is how we always
			 * operate, so simply do nothing here. */
			break;
		case 'l':
			flags |= OPT_login;
			break;
#if !BB_MMU
		case '<': /* "big heredoc" support */
			full_write1_str(optarg);
			_exit(0);
		case '$': {
			unsigned long long empty_trap_mask;

			G.root_pid = bb_strtou(optarg, &optarg, 16);
			optarg++;
			G.root_ppid = bb_strtou(optarg, &optarg, 16);
			optarg++;
			G.last_bg_pid = bb_strtou(optarg, &optarg, 16);
			optarg++;
			G.last_exitcode = bb_strtou(optarg, &optarg, 16);
			optarg++;
			builtin_argc = bb_strtou(optarg, &optarg, 16);
			optarg++;
			empty_trap_mask = bb_strtoull(optarg, &optarg, 16);
			if (empty_trap_mask != 0) {
				int sig;
				install_special_sighandlers();
				G.traps = xzalloc(sizeof(G.traps[0]) * NSIG);
				for (sig = 1; sig < NSIG; sig++) {
					if (empty_trap_mask & (1LL << sig)) {
						G.traps[sig] = xzalloc(1); /* == xstrdup(""); */
						install_sighandler(sig, SIG_IGN);
					}
				}
			}
# if ENABLE_HUSH_LOOPS
			optarg++;
			G.depth_of_loop = bb_strtou(optarg, &optarg, 16);
# endif
			break;
		}
		case 'R':
		case 'V':
			set_local_var(xstrdup(optarg), /*exp:*/ 0, /*lvl:*/ 0, /*ro:*/ opt == 'R');
			break;
# if ENABLE_HUSH_FUNCTIONS
		case 'F': {
			struct function *funcp = new_function(optarg);
			/* funcp->name is already set to optarg */
			/* funcp->body is set to NULL. It's a special case. */
			funcp->body_as_string = argv[optind];
			optind++;
			break;
		}
# endif
#endif
		case 'n':
		case 'x':
			if (set_mode(1, opt, NULL) == 0) /* no error */
				break;
		default:
#ifndef BB_VER
			fprintf(stderr, "Usage: sh [FILE]...\n"
					"   or: sh -c command [args]...\n\n");
			exit(EXIT_FAILURE);
#else
			bb_show_usage();
#endif
		}
	} /* option parsing loop */

	/* Skip options. Try "hush -l": $1 should not be "-l"! */
	G.global_argc = argc - (optind - 1);
	G.global_argv = argv + (optind - 1);
	G.global_argv[0] = argv[0];

	if (!G.root_pid) {
		G.root_pid = getpid();
		G.root_ppid = getppid();
	}

	/* If we are login shell... */
	if (flags & OPT_login) {
		FILE *input;
		debug_printf("sourcing /etc/profile\n");
		input = fopen_for_read("/etc/profile");
		if (input != NULL) {
			close_on_exec_on(fileno(input));
			install_special_sighandlers();
			parse_and_run_file(input);
			fclose(input);
		}
		/* bash: after sourcing /etc/profile,
		 * tries to source (in the given order):
		 * ~/.bash_profile, ~/.bash_login, ~/.profile,
		 * stopping on first found. --noprofile turns this off.
		 * bash also sources ~/.bash_logout on exit.
		 * If called as sh, skips .bash_XXX files.
		 */
	}

	if (G.global_argv[1]) {
		FILE *input;
		/*
		 * "bash <script>" (which is never interactive (unless -i?))
		 * sources $BASH_ENV here (without scanning $PATH).
		 * If called as sh, does the same but with $ENV.
		 */
		G.global_argc--;
		G.global_argv++;
		debug_printf("running script '%s'\n", G.global_argv[0]);
		input = xfopen_for_read(G.global_argv[0]);
		close_on_exec_on(fileno(input));
		install_special_sighandlers();
		parse_and_run_file(input);
#if ENABLE_FEATURE_CLEAN_UP
		fclose(input);
#endif
		goto final_return;
	}

	/* Up to here, shell was non-interactive. Now it may become one.
	 * NB: don't forget to (re)run install_special_sighandlers() as needed.
	 */

	/* A shell is interactive if the '-i' flag was given,
	 * or if all of the following conditions are met:
	 *    no -c command
	 *    no arguments remaining or the -s flag given
	 *    standard input is a terminal
	 *    standard output is a terminal
	 * Refer to Posix.2, the description of the 'sh' utility.
	 */
#if ENABLE_HUSH_JOB
	if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
		G_saved_tty_pgrp = tcgetpgrp(STDIN_FILENO);
		debug_printf("saved_tty_pgrp:%d\n", G_saved_tty_pgrp);
		if (G_saved_tty_pgrp < 0)
			G_saved_tty_pgrp = 0;

		/* try to dup stdin to high fd#, >= 255 */
		G_interactive_fd = fcntl(STDIN_FILENO, F_DUPFD, 255);
		if (G_interactive_fd < 0) {
			/* try to dup to any fd */
			G_interactive_fd = dup(STDIN_FILENO);
			if (G_interactive_fd < 0) {
				/* give up */
				G_interactive_fd = 0;
				G_saved_tty_pgrp = 0;
			}
		}
// TODO: track & disallow any attempts of user
// to (inadvertently) close/redirect G_interactive_fd
	}
	debug_printf("interactive_fd:%d\n", G_interactive_fd);
	if (G_interactive_fd) {
		close_on_exec_on(G_interactive_fd);

		if (G_saved_tty_pgrp) {
			/* If we were run as 'hush &', sleep until we are
			 * in the foreground (tty pgrp == our pgrp).
			 * If we get started under a job aware app (like bash),
			 * make sure we are now in charge so we don't fight over
			 * who gets the foreground */
			while (1) {
				pid_t shell_pgrp = getpgrp();
				G_saved_tty_pgrp = tcgetpgrp(G_interactive_fd);
				if (G_saved_tty_pgrp == shell_pgrp)
					break;
				/* send TTIN to ourself (should stop us) */
				kill(- shell_pgrp, SIGTTIN);
			}
		}

		/* Install more signal handlers */
		install_special_sighandlers();

		if (G_saved_tty_pgrp) {
			/* Set other signals to restore saved_tty_pgrp */
			install_fatal_sighandlers();
			/* Put ourselves in our own process group
			 * (bash, too, does this only if ctty is available) */
			bb_setpgrp(); /* is the same as setpgid(our_pid, our_pid); */
			/* Grab control of the terminal */
			tcsetpgrp(G_interactive_fd, getpid());
		}
		enable_restore_tty_pgrp_on_exit();

# if ENABLE_HUSH_SAVEHISTORY && MAX_HISTORY > 0
		{
			const char *hp = get_local_var_value("HISTFILE");
			if (!hp) {
				hp = get_local_var_value("HOME");
				if (hp)
					hp = concat_path_file(hp, ".hush_history");
			} else {
				hp = xstrdup(hp);
			}
			if (hp) {
				G.line_input_state->hist_file = hp;
				//set_local_var(xasprintf("HISTFILE=%s", ...));
			}
#  if ENABLE_FEATURE_SH_HISTFILESIZE
			hp = get_local_var_value("HISTFILESIZE");
			G.line_input_state->max_history = size_from_HISTFILESIZE(hp);
#  endif
		}
# endif
	} else {
		install_special_sighandlers();
	}
#elif ENABLE_HUSH_INTERACTIVE
	/* No job control compiled in, only prompt/line editing */
	if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
		G_interactive_fd = fcntl(STDIN_FILENO, F_DUPFD, 255);
		if (G_interactive_fd < 0) {
			/* try to dup to any fd */
			G_interactive_fd = dup(STDIN_FILENO);
			if (G_interactive_fd < 0)
				/* give up */
				G_interactive_fd = 0;
		}
	}
	if (G_interactive_fd) {
		close_on_exec_on(G_interactive_fd);
	}
	install_special_sighandlers();
#else
	/* We have interactiveness code disabled */
	install_special_sighandlers();
#endif
	/* bash:
	 * if interactive but not a login shell, sources ~/.bashrc
	 * (--norc turns this off, --rcfile <file> overrides)
	 */

	if (!ENABLE_FEATURE_SH_EXTRA_QUIET && G_interactive_fd) {
		/* note: ash and hush share this string */
		printf("\n\n%s %s\n"
			IF_HUSH_HELP("Enter 'help' for a list of built-in commands.\n")
			"\n",
			bb_banner,
			"hush - the humble shell"
		);
	}

	parse_and_run_file(stdin);

 final_return:
	hush_exit(G.last_exitcode);
}


#if ENABLE_MSH
int msh_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int msh_main(int argc, char **argv)
{
	//bb_error_msg("msh is deprecated, please use hush instead");
	return hush_main(argc, argv);
}
#endif


/*
 * Built-ins
 */
static int FAST_FUNC builtin_true(char **argv UNUSED_PARAM)
{
	return 0;
}

static int run_applet_main(char **argv, int (*applet_main_func)(int argc, char **argv))
{
	int argc = 0;
	while (*argv) {
		argc++;
		argv++;
	}
	return applet_main_func(argc, argv - argc);
}

static int FAST_FUNC builtin_test(char **argv)
{
	return run_applet_main(argv, test_main);
}

static int FAST_FUNC builtin_echo(char **argv)
{
	return run_applet_main(argv, echo_main);
}

#if ENABLE_PRINTF
static int FAST_FUNC builtin_printf(char **argv)
{
	return run_applet_main(argv, printf_main);
}
#endif

static char **skip_dash_dash(char **argv)
{
	argv++;
	if (argv[0] && argv[0][0] == '-' && argv[0][1] == '-' && argv[0][2] == '\0')
		argv++;
	return argv;
}

static int FAST_FUNC builtin_eval(char **argv)
{
	int rcode = EXIT_SUCCESS;

	argv = skip_dash_dash(argv);
	if (*argv) {
		char *str = expand_strvec_to_string(argv);
		/* bash:
		 * eval "echo Hi; done" ("done" is syntax error):
		 * "echo Hi" will not execute too.
		 */
		parse_and_run_string(str);
		free(str);
		rcode = G.last_exitcode;
	}
	return rcode;
}

static int FAST_FUNC builtin_cd(char **argv)
{
	const char *newdir;

	argv = skip_dash_dash(argv);
	newdir = argv[0];
	if (newdir == NULL) {
		/* bash does nothing (exitcode 0) if HOME is ""; if it's unset,
		 * bash says "bash: cd: HOME not set" and does nothing
		 * (exitcode 1)
		 */
		const char *home = get_local_var_value("HOME");
		newdir = home ? home : "/";
	}
	if (chdir(newdir)) {
		/* Mimic bash message exactly */
		bb_perror_msg("cd: %s", newdir);
		return EXIT_FAILURE;
	}
	/* Read current dir (get_cwd(1) is inside) and set PWD.
	 * Note: do not enforce exporting. If PWD was unset or unexported,
	 * set it again, but do not export. bash does the same.
	 */
	set_pwd_var(/*exp:*/ 0);
	return EXIT_SUCCESS;
}

static int FAST_FUNC builtin_exec(char **argv)
{
	argv = skip_dash_dash(argv);
	if (argv[0] == NULL)
		return EXIT_SUCCESS; /* bash does this */

	/* Careful: we can end up here after [v]fork. Do not restore
	 * tty pgrp then, only top-level shell process does that */
	if (G_saved_tty_pgrp && getpid() == G.root_pid)
		tcsetpgrp(G_interactive_fd, G_saved_tty_pgrp);

	/* TODO: if exec fails, bash does NOT exit! We do.
	 * We'll need to undo trap cleanup (it's inside execvp_or_die)
	 * and tcsetpgrp, and this is inherently racy.
	 */
	execvp_or_die(argv);
}

static int FAST_FUNC builtin_exit(char **argv)
{
	debug_printf_exec("%s()\n", __func__);

	/* interactive bash:
	 * # trap "echo EEE" EXIT
	 * # exit
	 * exit
	 * There are stopped jobs.
	 * (if there are _stopped_ jobs, running ones don't count)
	 * # exit
	 * exit
	 * EEE (then bash exits)
	 *
	 * TODO: we can use G.exiting = -1 as indicator "last cmd was exit"
	 */

	/* note: EXIT trap is run by hush_exit */
	argv = skip_dash_dash(argv);
	if (argv[0] == NULL)
		hush_exit(G.last_exitcode);
	/* mimic bash: exit 123abc == exit 255 + error msg */
	xfunc_error_retval = 255;
	/* bash: exit -2 == exit 254, no error msg */
	hush_exit(xatoi(argv[0]) & 0xff);
}

static void print_escaped(const char *s)
{
	if (*s == '\'')
		goto squote;
	do {
		const char *p = strchrnul(s, '\'');
		/* print 'xxxx', possibly just '' */
		printf("'%.*s'", (int)(p - s), s);
		if (*p == '\0')
			break;
		s = p;
 squote:
		/* s points to '; print "'''...'''" */
		putchar('"');
		do putchar('\''); while (*++s == '\'');
		putchar('"');
	} while (*s);
}

#if !ENABLE_HUSH_LOCAL
#define helper_export_local(argv, exp, lvl) \
	helper_export_local(argv, exp)
#endif
static void helper_export_local(char **argv, int exp, int lvl)
{
	do {
		char *name = *argv;
		char *name_end = strchrnul(name, '=');

		/* So far we do not check that name is valid (TODO?) */

		if (*name_end == '\0') {
			struct variable *var, **vpp;

			vpp = get_ptr_to_local_var(name, name_end - name);
			var = vpp ? *vpp : NULL;

			if (exp == -1) { /* unexporting? */
				/* export -n NAME (without =VALUE) */
				if (var) {
					var->flg_export = 0;
					debug_printf_env("%s: unsetenv '%s'\n", __func__, name);
					unsetenv(name);
				} /* else: export -n NOT_EXISTING_VAR: no-op */
				continue;
			}
			if (exp == 1) { /* exporting? */
				/* export NAME (without =VALUE) */
				if (var) {
					var->flg_export = 1;
					debug_printf_env("%s: putenv '%s'\n", __func__, var->varstr);
					putenv(var->varstr);
					continue;
				}
			}
			/* Exporting non-existing variable.
			 * bash does not put it in environment,
			 * but remembers that it is exported,
			 * and does put it in env when it is set later.
			 * We just set it to "" and export. */
			/* Or, it's "local NAME" (without =VALUE).
			 * bash sets the value to "". */
			name = xasprintf("%s=", name);
		} else {
			/* (Un)exporting/making local NAME=VALUE */
			name = xstrdup(name);
		}
		set_local_var(name, /*exp:*/ exp, /*lvl:*/ lvl, /*ro:*/ 0);
	} while (*++argv);
}

static int FAST_FUNC builtin_export(char **argv)
{
	unsigned opt_unexport;

#if ENABLE_HUSH_EXPORT_N
	/* "!": do not abort on errors */
	opt_unexport = getopt32(argv, "!n");
	if (opt_unexport == (uint32_t)-1)
		return EXIT_FAILURE;
	argv += optind;
#else
	opt_unexport = 0;
	argv++;
#endif

	if (argv[0] == NULL) {
		char **e = environ;
		if (e) {
			while (*e) {
#if 0
				puts(*e++);
#else
				/* ash emits: export VAR='VAL'
				 * bash: declare -x VAR="VAL"
				 * we follow ash example */
				const char *s = *e++;
				const char *p = strchr(s, '=');

				if (!p) /* wtf? take next variable */
					continue;
				/* export var= */
				printf("export %.*s", (int)(p - s) + 1, s);
				print_escaped(p + 1);
				putchar('\n');
#endif
			}
			/*fflush_all(); - done after each builtin anyway */
		}
		return EXIT_SUCCESS;
	}

	helper_export_local(argv, (opt_unexport ? -1 : 1), 0);

	return EXIT_SUCCESS;
}

#if ENABLE_HUSH_LOCAL
static int FAST_FUNC builtin_local(char **argv)
{
	if (G.func_nest_level == 0) {
		bb_error_msg("%s: not in a function", argv[0]);
		return EXIT_FAILURE; /* bash compat */
	}
	helper_export_local(argv, 0, G.func_nest_level);
	return EXIT_SUCCESS;
}
#endif

static int FAST_FUNC builtin_trap(char **argv)
{
	int sig;
	char *new_cmd;

	if (!G.traps)
		G.traps = xzalloc(sizeof(G.traps[0]) * NSIG);

	argv++;
	if (!*argv) {
		int i;
		/* No args: print all trapped */
		for (i = 0; i < NSIG; ++i) {
			if (G.traps[i]) {
				printf("trap -- ");
				print_escaped(G.traps[i]);
				/* note: bash adds "SIG", but only if invoked
				 * as "bash". If called as "sh", or if set -o posix,
				 * then it prints short signal names.
				 * We are printing short names: */
				printf(" %s\n", get_signame(i));
			}
		}
		/*fflush_all(); - done after each builtin anyway */
		return EXIT_SUCCESS;
	}

	new_cmd = NULL;
	/* If first arg is a number: reset all specified signals */
	sig = bb_strtou(*argv, NULL, 10);
	if (errno == 0) {
		int ret;
 process_sig_list:
		ret = EXIT_SUCCESS;
		while (*argv) {
			sighandler_t handler;

			sig = get_signum(*argv++);
			if (sig < 0 || sig >= NSIG) {
				ret = EXIT_FAILURE;
				/* Mimic bash message exactly */
				bb_perror_msg("trap: %s: invalid signal specification", argv[-1]);
				continue;
			}

			free(G.traps[sig]);
			G.traps[sig] = xstrdup(new_cmd);

			debug_printf("trap: setting SIG%s (%i) to '%s'\n",
				get_signame(sig), sig, G.traps[sig]);

			/* There is no signal for 0 (EXIT) */
			if (sig == 0)
				continue;

			if (new_cmd)
				handler = (new_cmd[0] ? record_pending_signo : SIG_IGN);
			else
				/* We are removing trap handler */
				handler = pick_sighandler(sig);
			install_sighandler(sig, handler);
		}
		return ret;
	}

	if (!argv[1]) { /* no second arg */
		bb_error_msg("trap: invalid arguments");
		return EXIT_FAILURE;
	}

	/* First arg is "-": reset all specified to default */
	/* First arg is "--": skip it, the rest is "handler SIGs..." */
	/* Everything else: set arg as signal handler
	 * (includes "" case, which ignores signal) */
	if (argv[0][0] == '-') {
		if (argv[0][1] == '\0') { /* "-" */
			/* new_cmd remains NULL: "reset these sigs" */
			goto reset_traps;
		}
		if (argv[0][1] == '-' && argv[0][2] == '\0') { /* "--" */
			argv++;
		}
		/* else: "-something", no special meaning */
	}
	new_cmd = *argv;
 reset_traps:
	argv++;
	goto process_sig_list;
}

/* http://www.opengroup.org/onlinepubs/9699919799/utilities/type.html */
static int FAST_FUNC builtin_type(char **argv)
{
	int ret = EXIT_SUCCESS;

	while (*++argv) {
		const char *type;
		char *path = NULL;

		if (0) {} /* make conditional compile easier below */
		/*else if (find_alias(*argv))
			type = "an alias";*/
#if ENABLE_HUSH_FUNCTIONS
		else if (find_function(*argv))
			type = "a function";
#endif
		else if (find_builtin(*argv))
			type = "a shell builtin";
		else if ((path = find_in_path(*argv)) != NULL)
			type = path;
		else {
			bb_error_msg("type: %s: not found", *argv);
			ret = EXIT_FAILURE;
			continue;
		}

		printf("%s is %s\n", *argv, type);
		free(path);
	}

	return ret;
}

#if ENABLE_HUSH_JOB
/* built-in 'fg' and 'bg' handler */
static int FAST_FUNC builtin_fg_bg(char **argv)
{
	int i, jobnum;
	struct pipe *pi;

	if (!G_interactive_fd)
		return EXIT_FAILURE;

	/* If they gave us no args, assume they want the last backgrounded task */
	if (!argv[1]) {
		for (pi = G.job_list; pi; pi = pi->next) {
			if (pi->jobid == G.last_jobid) {
				goto found;
			}
		}
		bb_error_msg("%s: no current job", argv[0]);
		return EXIT_FAILURE;
	}
	if (sscanf(argv[1], "%%%d", &jobnum) != 1) {
		bb_error_msg("%s: bad argument '%s'", argv[0], argv[1]);
		return EXIT_FAILURE;
	}
	for (pi = G.job_list; pi; pi = pi->next) {
		if (pi->jobid == jobnum) {
			goto found;
		}
	}
	bb_error_msg("%s: %d: no such job", argv[0], jobnum);
	return EXIT_FAILURE;
 found:
	/* TODO: bash prints a string representation
	 * of job being foregrounded (like "sleep 1 | cat") */
	if (argv[0][0] == 'f' && G_saved_tty_pgrp) {
		/* Put the job into the foreground.  */
		tcsetpgrp(G_interactive_fd, pi->pgrp);
	}

	/* Restart the processes in the job */
	debug_printf_jobs("reviving %d procs, pgrp %d\n", pi->num_cmds, pi->pgrp);
	for (i = 0; i < pi->num_cmds; i++) {
		debug_printf_jobs("reviving pid %d\n", pi->cmds[i].pid);
	}
	pi->stopped_cmds = 0;

	i = kill(- pi->pgrp, SIGCONT);
	if (i < 0) {
		if (errno == ESRCH) {
			delete_finished_bg_job(pi);
			return EXIT_SUCCESS;
		}
		bb_perror_msg("kill (SIGCONT)");
	}

	if (argv[0][0] == 'f') {
		remove_bg_job(pi);
		return checkjobs_and_fg_shell(pi);
	}
	return EXIT_SUCCESS;
}
#endif

#if ENABLE_HUSH_HELP
static int FAST_FUNC builtin_help(char **argv UNUSED_PARAM)
{
	const struct built_in_command *x;

	printf(
		"Built-in commands:\n"
		"------------------\n");
	for (x = bltins1; x != &bltins1[ARRAY_SIZE(bltins1)]; x++) {
		if (x->b_descr)
			printf("%-10s%s\n", x->b_cmd, x->b_descr);
	}
	bb_putchar('\n');
	return EXIT_SUCCESS;
}
#endif

#if MAX_HISTORY && ENABLE_FEATURE_EDITING
static int FAST_FUNC builtin_history(char **argv UNUSED_PARAM)
{
	show_history(G.line_input_state);
	return EXIT_SUCCESS;
}
#endif

#if ENABLE_HUSH_JOB
static int FAST_FUNC builtin_jobs(char **argv UNUSED_PARAM)
{
	struct pipe *job;
	const char *status_string;

	for (job = G.job_list; job; job = job->next) {
		if (job->alive_cmds == job->stopped_cmds)
			status_string = "Stopped";
		else
			status_string = "Running";

		printf(JOB_STATUS_FORMAT, job->jobid, status_string, job->cmdtext);
	}
	return EXIT_SUCCESS;
}
#endif

#if HUSH_DEBUG
static int FAST_FUNC builtin_memleak(char **argv UNUSED_PARAM)
{
	void *p;
	unsigned long l;

# ifdef M_TRIM_THRESHOLD
	/* Optional. Reduces probability of false positives */
	malloc_trim(0);
# endif
	/* Crude attempt to find where "free memory" starts,
	 * sans fragmentation. */
	p = malloc(240);
	l = (unsigned long)p;
	free(p);
	p = malloc(3400);
	if (l < (unsigned long)p) l = (unsigned long)p;
	free(p);

	if (!G.memleak_value)
		G.memleak_value = l;

	l -= G.memleak_value;
	if ((long)l < 0)
		l = 0;
	l /= 1024;
	if (l > 127)
		l = 127;

	/* Exitcode is "how many kilobytes we leaked since 1st call" */
	return l;
}
#endif

static int FAST_FUNC builtin_pwd(char **argv UNUSED_PARAM)
{
	puts(get_cwd(0));
	return EXIT_SUCCESS;
}

/* Interruptibility of read builtin in bash
 * (tested on bash-4.2.8 by sending signals (not by ^C)):
 *
 * Empty trap makes read ignore corresponding signal, for any signal.
 *
 * SIGINT:
 * - terminates non-interactive shell;
 * - interrupts read in interactive shell;
 * if it has non-empty trap:
 * - executes trap and returns to command prompt in interactive shell;
 * - executes trap and returns to read in non-interactive shell;
 * SIGTERM:
 * - is ignored (does not interrupt) read in interactive shell;
 * - terminates non-interactive shell;
 * if it has non-empty trap:
 * - executes trap and returns to read;
 * SIGHUP:
 * - terminates shell (regardless of interactivity);
 * if it has non-empty trap:
 * - executes trap and returns to read;
 */
static int FAST_FUNC builtin_read(char **argv)
{
	const char *r;
	char *opt_n = NULL;
	char *opt_p = NULL;
	char *opt_t = NULL;
	char *opt_u = NULL;
	const char *ifs;
	int read_flags;

	/* "!": do not abort on errors.
	 * Option string must start with "sr" to match BUILTIN_READ_xxx
	 */
	read_flags = getopt32(argv, "!srn:p:t:u:", &opt_n, &opt_p, &opt_t, &opt_u);
	if (read_flags == (uint32_t)-1)
		return EXIT_FAILURE;
	argv += optind;
	ifs = get_local_var_value("IFS"); /* can be NULL */

 again:
	r = shell_builtin_read(set_local_var_from_halves,
		argv,
		ifs,
		read_flags,
		opt_n,
		opt_p,
		opt_t,
		opt_u
	);

	if ((uintptr_t)r == 1 && errno == EINTR) {
		unsigned sig = check_and_run_traps();
		if (sig && sig != SIGINT)
			goto again;
	}

	if ((uintptr_t)r > 1) {
		bb_error_msg("%s", r);
		r = (char*)(uintptr_t)1;
	}

	return (uintptr_t)r;
}

/* http://www.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#set
 * built-in 'set' handler
 * SUSv3 says:
 * set [-abCefhmnuvx] [-o option] [argument...]
 * set [+abCefhmnuvx] [+o option] [argument...]
 * set -- [argument...]
 * set -o
 * set +o
 * Implementations shall support the options in both their hyphen and
 * plus-sign forms. These options can also be specified as options to sh.
 * Examples:
 * Write out all variables and their values: set
 * Set $1, $2, and $3 and set "$#" to 3: set c a b
 * Turn on the -x and -v options: set -xv
 * Unset all positional parameters: set --
 * Set $1 to the value of x, even if it begins with '-' or '+': set -- "$x"
 * Set the positional parameters to the expansion of x, even if x expands
 * with a leading '-' or '+': set -- $x
 *
 * So far, we only support "set -- [argument...]" and some of the short names.
 */
static int FAST_FUNC builtin_set(char **argv)
{
	int n;
	char **pp, **g_argv;
	char *arg = *++argv;

	if (arg == NULL) {
		struct variable *e;
		for (e = G.top_var; e; e = e->next)
			puts(e->varstr);
		return EXIT_SUCCESS;
	}

	do {
		if (strcmp(arg, "--") == 0) {
			++argv;
			goto set_argv;
		}
		if (arg[0] != '+' && arg[0] != '-')
			break;
		for (n = 1; arg[n]; ++n) {
			if (set_mode((arg[0] == '-'), arg[n], argv[1]))
				goto error;
			if (arg[n] == 'o' && argv[1])
				argv++;
		}
	} while ((arg = *++argv) != NULL);
	/* Now argv[0] is 1st argument */

	if (arg == NULL)
		return EXIT_SUCCESS;
 set_argv:

	/* NB: G.global_argv[0] ($0) is never freed/changed */
	g_argv = G.global_argv;
	if (G.global_args_malloced) {
		pp = g_argv;
		while (*++pp)
			free(*pp);
		g_argv[1] = NULL;
	} else {
		G.global_args_malloced = 1;
		pp = xzalloc(sizeof(pp[0]) * 2);
		pp[0] = g_argv[0]; /* retain $0 */
		g_argv = pp;
	}
	/* This realloc's G.global_argv */
	G.global_argv = pp = add_strings_to_strings(g_argv, argv, /*dup:*/ 1);

	n = 1;
	while (*++pp)
		n++;
	G.global_argc = n;

	return EXIT_SUCCESS;

	/* Nothing known, so abort */
 error:
	bb_error_msg("set: %s: invalid option", arg);
	return EXIT_FAILURE;
}

static int FAST_FUNC builtin_shift(char **argv)
{
	int n = 1;
	argv = skip_dash_dash(argv);
	if (argv[0]) {
		n = atoi(argv[0]);
	}
	if (n >= 0 && n < G.global_argc) {
		if (G.global_args_malloced) {
			int m = 1;
			while (m <= n)
				free(G.global_argv[m++]);
		}
		G.global_argc -= n;
		memmove(&G.global_argv[1], &G.global_argv[n+1],
				G.global_argc * sizeof(G.global_argv[0]));
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

static int FAST_FUNC builtin_source(char **argv)
{
	char *arg_path, *filename;
	FILE *input;
	save_arg_t sv;
#if ENABLE_HUSH_FUNCTIONS
	smallint sv_flg;
#endif

	argv = skip_dash_dash(argv);
	filename = argv[0];
	if (!filename) {
		/* bash says: "bash: .: filename argument required" */
		return 2; /* bash compat */
	}
	arg_path = NULL;
	if (!strchr(filename, '/')) {
		arg_path = find_in_path(filename);
		if (arg_path)
			filename = arg_path;
	}
	input = fopen_or_warn(filename, "r");
	free(arg_path);
	if (!input) {
		/* bb_perror_msg("%s", *argv); - done by fopen_or_warn */
		/* POSIX: non-interactive shell should abort here,
		 * not merely fail. So far no one complained :)
		 */
		return EXIT_FAILURE;
	}
	close_on_exec_on(fileno(input));

#if ENABLE_HUSH_FUNCTIONS
	sv_flg = G.flag_return_in_progress;
	/* "we are inside sourced file, ok to use return" */
	G.flag_return_in_progress = -1;
#endif
	if (argv[1])
		save_and_replace_G_args(&sv, argv);

	parse_and_run_file(input);
	fclose(input);

	if (argv[1])
		restore_G_args(&sv, argv);
#if ENABLE_HUSH_FUNCTIONS
	G.flag_return_in_progress = sv_flg;
#endif

	return G.last_exitcode;
}

static int FAST_FUNC builtin_umask(char **argv)
{
	int rc;
	mode_t mask;

	rc = 1;
	mask = umask(0);
	argv = skip_dash_dash(argv);
	if (argv[0]) {
		mode_t old_mask = mask;

		/* numeric umasks are taken as-is */
		/* symbolic umasks are inverted: "umask a=rx" calls umask(222) */
		if (!isdigit(argv[0][0]))
			mask ^= 0777;
		mask = bb_parse_mode(argv[0], mask);
		if (!isdigit(argv[0][0]))
			mask ^= 0777;
		if ((unsigned)mask > 0777) {
			mask = old_mask;
			/* bash messages:
			 * bash: umask: 'q': invalid symbolic mode operator
			 * bash: umask: 999: octal number out of range
			 */
			bb_error_msg("%s: invalid mode '%s'", "umask", argv[0]);
			rc = 0;
		}
	} else {
		/* Mimic bash */
		printf("%04o\n", (unsigned) mask);
		/* fall through and restore mask which we set to 0 */
	}
	umask(mask);

	return !rc; /* rc != 0 - success */
}

/* http://www.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#unset */
static int FAST_FUNC builtin_unset(char **argv)
{
	int ret;
	unsigned opts;

	/* "!": do not abort on errors */
	/* "+": stop at 1st non-option */
	opts = getopt32(argv, "!+vf");
	if (opts == (unsigned)-1)
		return EXIT_FAILURE;
	if (opts == 3) {
		bb_error_msg("unset: -v and -f are exclusive");
		return EXIT_FAILURE;
	}
	argv += optind;

	ret = EXIT_SUCCESS;
	while (*argv) {
		if (!(opts & 2)) { /* not -f */
			if (unset_local_var(*argv)) {
				/* unset <nonexistent_var> doesn't fail.
				 * Error is when one tries to unset RO var.
				 * Message was printed by unset_local_var. */
				ret = EXIT_FAILURE;
			}
		}
#if ENABLE_HUSH_FUNCTIONS
		else {
			unset_func(*argv);
		}
#endif
		argv++;
	}
	return ret;
}

/* http://www.opengroup.org/onlinepubs/9699919799/utilities/wait.html */
static int FAST_FUNC builtin_wait(char **argv)
{
	int ret = EXIT_SUCCESS;
	int status;

	argv = skip_dash_dash(argv);
	if (argv[0] == NULL) {
		/* Don't care about wait results */
		/* Note 1: must wait until there are no more children */
		/* Note 2: must be interruptible */
		/* Examples:
		 * $ sleep 3 & sleep 6 & wait
		 * [1] 30934 sleep 3
		 * [2] 30935 sleep 6
		 * [1] Done                   sleep 3
		 * [2] Done                   sleep 6
		 * $ sleep 3 & sleep 6 & wait
		 * [1] 30936 sleep 3
		 * [2] 30937 sleep 6
		 * [1] Done                   sleep 3
		 * ^C <-- after ~4 sec from keyboard
		 * $
		 */
		while (1) {
			int sig;
			sigset_t oldset, allsigs;

			/* waitpid is not interruptible by SA_RESTARTed
			 * signals which we use. Thus, this ugly dance:
			 */

			/* Make sure possible SIGCHLD is stored in kernel's
			 * pending signal mask before we call waitpid.
			 * Or else we may race with SIGCHLD, lose it,
			 * and get stuck in sigwaitinfo...
			 */
			sigfillset(&allsigs);
			sigprocmask(SIG_SETMASK, &allsigs, &oldset);

			if (!sigisemptyset(&G.pending_set)) {
				/* Crap! we raced with some signal! */
			//	sig = 0;
				goto restore;
			}

			checkjobs(NULL); /* waitpid(WNOHANG) inside */
			if (errno == ECHILD) {
				sigprocmask(SIG_SETMASK, &oldset, NULL);
				break;
			}

			/* Wait for SIGCHLD or any other signal */
			//sig = sigwaitinfo(&allsigs, NULL);
			/* It is vitally important for sigsuspend that SIGCHLD has non-DFL handler! */
			/* Note: sigsuspend invokes signal handler */
			sigsuspend(&oldset);
 restore:
			sigprocmask(SIG_SETMASK, &oldset, NULL);

			/* So, did we get a signal? */
			//if (sig > 0)
			//	raise(sig); /* run handler */
			sig = check_and_run_traps();
			if (sig /*&& sig != SIGCHLD - always true */) {
				/* see note 2 */
				ret = 128 + sig;
				break;
			}
			/* SIGCHLD, or no signal, or ignored one, such as SIGQUIT. Repeat */
		}
		return ret;
	}

	/* This is probably buggy wrt interruptible-ness */
	while (*argv) {
		pid_t pid = bb_strtou(*argv, NULL, 10);
		if (errno) {
			/* mimic bash message */
			bb_error_msg("wait: '%s': not a pid or valid job spec", *argv);
			return EXIT_FAILURE;
		}
		if (waitpid(pid, &status, 0) == pid) {
			ret = WEXITSTATUS(status);
			if (WIFSIGNALED(status))
				ret = 128 + WTERMSIG(status);
		} else {
			bb_perror_msg("wait %s", *argv);
			ret = 127;
		}
		argv++;
	}

	return ret;
}

#if ENABLE_HUSH_LOOPS || ENABLE_HUSH_FUNCTIONS
static unsigned parse_numeric_argv1(char **argv, unsigned def, unsigned def_min)
{
	if (argv[1]) {
		def = bb_strtou(argv[1], NULL, 10);
		if (errno || def < def_min || argv[2]) {
			bb_error_msg("%s: bad arguments", argv[0]);
			def = UINT_MAX;
		}
	}
	return def;
}
#endif

#if ENABLE_HUSH_LOOPS
static int FAST_FUNC builtin_break(char **argv)
{
	unsigned depth;
	if (G.depth_of_loop == 0) {
		bb_error_msg("%s: only meaningful in a loop", argv[0]);
		return EXIT_SUCCESS; /* bash compat */
	}
	G.flag_break_continue++; /* BC_BREAK = 1 */

	G.depth_break_continue = depth = parse_numeric_argv1(argv, 1, 1);
	if (depth == UINT_MAX)
		G.flag_break_continue = BC_BREAK;
	if (G.depth_of_loop < depth)
		G.depth_break_continue = G.depth_of_loop;

	return EXIT_SUCCESS;
}

static int FAST_FUNC builtin_continue(char **argv)
{
	G.flag_break_continue = 1; /* BC_CONTINUE = 2 = 1+1 */
	return builtin_break(argv);
}
#endif

#if ENABLE_HUSH_FUNCTIONS
static int FAST_FUNC builtin_return(char **argv)
{
	int rc;

	if (G.flag_return_in_progress != -1) {
		bb_error_msg("%s: not in a function or sourced script", argv[0]);
		return EXIT_FAILURE; /* bash compat */
	}

	G.flag_return_in_progress = 1;

	/* bash:
	 * out of range: wraps around at 256, does not error out
	 * non-numeric param:
	 * f() { false; return qwe; }; f; echo $?
	 * bash: return: qwe: numeric argument required  <== we do this
	 * 255  <== we also do this
	 */
	rc = parse_numeric_argv1(argv, G.last_exitcode, 0);
	return rc;
}
#endif

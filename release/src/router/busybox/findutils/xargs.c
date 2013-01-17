/* vi: set sw=4 ts=4: */
/*
 * Mini xargs implementation for busybox
 *
 * (C) 2002,2003 by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Special thanks
 * - Mark Whitley and Glenn McGrath for stimulus to rewrite :)
 * - Mike Rendell <michael@cs.mun.ca>
 * and David MacKenzie <djm@gnu.ai.mit.edu>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * xargs is described in the Single Unix Specification v3 at
 * http://www.opengroup.org/onlinepubs/007904975/utilities/xargs.html
 */

//config:config XARGS
//config:	bool "xargs"
//config:	default y
//config:	help
//config:	  xargs is used to execute a specified command for
//config:	  every item from standard input.
//config:
//config:config FEATURE_XARGS_SUPPORT_CONFIRMATION
//config:	bool "Enable -p: prompt and confirmation"
//config:	default y
//config:	depends on XARGS
//config:	help
//config:	  Support -p: prompt the user whether to run each command
//config:	  line and read a line from the terminal.
//config:
//config:config FEATURE_XARGS_SUPPORT_QUOTES
//config:	bool "Enable single and double quotes and backslash"
//config:	default y
//config:	depends on XARGS
//config:	help
//config:	  Support quoting in the input.
//config:
//config:config FEATURE_XARGS_SUPPORT_TERMOPT
//config:	bool "Enable -x: exit if -s or -n is exceeded"
//config:	default y
//config:	depends on XARGS
//config:	help
//config:	  Support -x: exit if the command size (see the -s or -n option)
//config:	  is exceeded.
//config:
//config:config FEATURE_XARGS_SUPPORT_ZERO_TERM
//config:	bool "Enable -0: NUL-terminated input"
//config:	default y
//config:	depends on XARGS
//config:	help
//config:	  Support -0: input items are terminated by a NUL character
//config:	  instead of whitespace, and the quotes and backslash
//config:	  are not special.

//applet:IF_XARGS(APPLET_NOEXEC(xargs, xargs, BB_DIR_USR_BIN, BB_SUID_DROP, xargs))

//kbuild:lib-$(CONFIG_XARGS) += xargs.o

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */


//#define dbg_msg(...) bb_error_msg(__VA_ARGS__)
#define dbg_msg(...) ((void)0)


#ifdef TEST
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_CONFIRMATION
#  define ENABLE_FEATURE_XARGS_SUPPORT_CONFIRMATION 1
# endif
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_QUOTES
#  define ENABLE_FEATURE_XARGS_SUPPORT_QUOTES 1
# endif
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_TERMOPT
#  define ENABLE_FEATURE_XARGS_SUPPORT_TERMOPT 1
# endif
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM
#  define ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM 1
# endif
#endif


struct globals {
	char **args;
	const char *eof_str;
	int idx;
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { \
	G.eof_str = NULL; /* need to clear by hand because we are NOEXEC applet */ \
} while (0)


/*
 * This function has special algorithm.
 * Don't use fork and include to main!
 */
static int xargs_exec(void)
{
	int status;

	status = spawn_and_wait(G.args);
	if (status < 0) {
		bb_simple_perror_msg(G.args[0]);
		return errno == ENOENT ? 127 : 126;
	}
	if (status == 255) {
		bb_error_msg("%s: exited with status 255; aborting", G.args[0]);
		return 124;
	}
	if (status >= 0x180) {
		bb_error_msg("%s: terminated by signal %d",
			G.args[0], status - 0x180);
		return 125;
	}
	if (status)
		return 123;
	return 0;
}

/* In POSIX/C locale isspace is only these chars: "\t\n\v\f\r" and space.
 * "\t\n\v\f\r" happen to have ASCII codes 9,10,11,12,13.
 */
#define ISSPACE(a) ({ unsigned char xargs__isspace = (a) - 9; xargs__isspace == (' ' - 9) || xargs__isspace <= (13 - 9); })

static void store_param(char *s)
{
	/* Grow by 256 elements at once */
	if (!(G.idx & 0xff)) { /* G.idx == N*256 */
		/* Enlarge, make G.args[(N+1)*256 - 1] last valid idx */
		G.args = xrealloc(G.args, sizeof(G.args[0]) * (G.idx + 0x100));
	}
	G.args[G.idx++] = s;
}

/* process[0]_stdin:
 * Read characters into buf[n_max_chars+1], and when parameter delimiter
 * is seen, store the address of a new parameter to args[].
 * If reading discovers that last chars do not form the complete
 * parameter, the pointer to the first such "tail character" is returned.
 * (buf has extra byte at the end to accomodate terminating NUL
 * of "tail characters" string).
 * Otherwise, the returned pointer points to NUL byte.
 * On entry, buf[] may contain some "seed chars" which are to become
 * the beginning of the first parameter.
 */

#if ENABLE_FEATURE_XARGS_SUPPORT_QUOTES
static char* FAST_FUNC process_stdin(int n_max_chars, int n_max_arg, char *buf)
{
#define NORM      0
#define QUOTE     1
#define BACKSLASH 2
#define SPACE     4
	char q = '\0';             /* quote char */
	char state = NORM;
	char *s = buf;             /* start of the word */
	char *p = s + strlen(buf); /* end of the word */

	buf += n_max_chars;        /* past buffer's end */

	/* "goto ret" is used instead of "break" to make control flow
	 * more obvious: */

	while (1) {
		int c = getchar();
		if (c == EOF) {
			if (p != s)
				goto close_word;
			goto ret;
		}
		if (state == BACKSLASH) {
			state = NORM;
			goto set;
		}
		if (state == QUOTE) {
			if (c != q)
				goto set;
			q = '\0';
			state = NORM;
		} else { /* if (state == NORM) */
			if (ISSPACE(c)) {
				if (p != s) {
 close_word:
					state = SPACE;
					c = '\0';
					goto set;
				}
			} else {
				if (c == '\\') {
					state = BACKSLASH;
				} else if (c == '\'' || c == '"') {
					q = c;
					state = QUOTE;
				} else {
 set:
					*p++ = c;
				}
			}
		}
		if (state == SPACE) {   /* word's delimiter or EOF detected */
			if (q) {
				bb_error_msg_and_die("unmatched %s quote",
					q == '\'' ? "single" : "double");
			}
			/* A full word is loaded */
			if (G.eof_str) {
				if (strcmp(s, G.eof_str) == 0) {
					while (getchar() != EOF)
						continue;
					p = s;
					goto ret;
				}
			}
			store_param(s);
			dbg_msg("args[]:'%s'", s);
			s = p;
			n_max_arg--;
			if (n_max_arg == 0) {
				goto ret;
			}
			state = NORM;
		}
		if (p == buf) {
			goto ret;
		}
	}
 ret:
	*p = '\0';
	/* store_param(NULL) - caller will do it */
	dbg_msg("return:'%s'", s);
	return s;
}
#else
/* The variant does not support single quotes, double quotes or backslash */
static char* FAST_FUNC process_stdin(int n_max_chars, int n_max_arg, char *buf)
{
	char *s = buf;             /* start of the word */
	char *p = s + strlen(buf); /* end of the word */

	buf += n_max_chars;        /* past buffer's end */

	while (1) {
		int c = getchar();
		if (c == EOF) {
			if (p == s)
				goto ret;
		}
		if (c == EOF || ISSPACE(c)) {
			if (p == s)
				continue;
			c = EOF;
		}
		*p++ = (c == EOF ? '\0' : c);
		if (c == EOF) { /* word's delimiter or EOF detected */
			/* A full word is loaded */
			if (G.eof_str) {
				if (strcmp(s, G.eof_str) == 0) {
					while (getchar() != EOF)
						continue;
					p = s;
					goto ret;
				}
			}
			store_param(s);
			dbg_msg("args[]:'%s'", s);
			s = p;
			n_max_arg--;
			if (n_max_arg == 0) {
				goto ret;
			}
		}
		if (p == buf) {
			goto ret;
		}
	}
 ret:
	*p = '\0';
	/* store_param(NULL) - caller will do it */
	dbg_msg("return:'%s'", s);
	return s;
}
#endif /* FEATURE_XARGS_SUPPORT_QUOTES */

#if ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM
static char* FAST_FUNC process0_stdin(int n_max_chars, int n_max_arg, char *buf)
{
	char *s = buf;             /* start of the word */
	char *p = s + strlen(buf); /* end of the word */

	buf += n_max_chars;        /* past buffer's end */

	while (1) {
		int c = getchar();
		if (c == EOF) {
			if (p == s)
				goto ret;
			c = '\0';
		}
		*p++ = c;
		if (c == '\0') {   /* word's delimiter or EOF detected */
			/* A full word is loaded */
			store_param(s);
			dbg_msg("args[]:'%s'", s);
			s = p;
			n_max_arg--;
			if (n_max_arg == 0) {
				goto ret;
			}
		}
		if (p == buf) {
			goto ret;
		}
	}
 ret:
	*p = '\0';
	/* store_param(NULL) - caller will do it */
	dbg_msg("return:'%s'", s);
	return s;
}
#endif /* FEATURE_XARGS_SUPPORT_ZERO_TERM */

#if ENABLE_FEATURE_XARGS_SUPPORT_CONFIRMATION
/* Prompt the user for a response, and
   if the user responds affirmatively, return true;
   otherwise, return false. Uses "/dev/tty", not stdin. */
static int xargs_ask_confirmation(void)
{
	FILE *tty_stream;
	int c, savec;

	tty_stream = xfopen_for_read(CURRENT_TTY);
	fputs(" ?...", stderr);
	fflush_all();
	c = savec = getc(tty_stream);
	while (c != EOF && c != '\n')
		c = getc(tty_stream);
	fclose(tty_stream);
	return (savec == 'y' || savec == 'Y');
}
#else
# define xargs_ask_confirmation() 1
#endif

//usage:#define xargs_trivial_usage
//usage:       "[OPTIONS] [PROG ARGS]"
//usage:#define xargs_full_usage "\n\n"
//usage:       "Run PROG on every item given by stdin\n"
//usage:	IF_FEATURE_XARGS_SUPPORT_CONFIRMATION(
//usage:     "\n	-p	Ask user whether to run each command"
//usage:	)
//usage:     "\n	-r	Don't run command if input is empty"
//usage:	IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(
//usage:     "\n	-0	Input is separated by NUL characters"
//usage:	)
//usage:     "\n	-t	Print the command on stderr before execution"
//usage:     "\n	-e[STR]	STR stops input processing"
//usage:     "\n	-n N	Pass no more than N args to PROG"
//usage:     "\n	-s N	Pass command line of no more than N bytes"
//usage:	IF_FEATURE_XARGS_SUPPORT_TERMOPT(
//usage:     "\n	-x	Exit if size is exceeded"
//usage:	)
//usage:#define xargs_example_usage
//usage:       "$ ls | xargs gzip\n"
//usage:       "$ find . -name '*.c' -print | xargs rm\n"

/* Correct regardless of combination of CONFIG_xxx */
enum {
	OPTBIT_VERBOSE = 0,
	OPTBIT_NO_EMPTY,
	OPTBIT_UPTO_NUMBER,
	OPTBIT_UPTO_SIZE,
	OPTBIT_EOF_STRING,
	OPTBIT_EOF_STRING1,
	IF_FEATURE_XARGS_SUPPORT_CONFIRMATION(OPTBIT_INTERACTIVE,)
	IF_FEATURE_XARGS_SUPPORT_TERMOPT(     OPTBIT_TERMINATE  ,)
	IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(   OPTBIT_ZEROTERM   ,)

	OPT_VERBOSE     = 1 << OPTBIT_VERBOSE    ,
	OPT_NO_EMPTY    = 1 << OPTBIT_NO_EMPTY   ,
	OPT_UPTO_NUMBER = 1 << OPTBIT_UPTO_NUMBER,
	OPT_UPTO_SIZE   = 1 << OPTBIT_UPTO_SIZE  ,
	OPT_EOF_STRING  = 1 << OPTBIT_EOF_STRING , /* GNU: -e[<param>] */
	OPT_EOF_STRING1 = 1 << OPTBIT_EOF_STRING1, /* SUS: -E<param> */
	OPT_INTERACTIVE = IF_FEATURE_XARGS_SUPPORT_CONFIRMATION((1 << OPTBIT_INTERACTIVE)) + 0,
	OPT_TERMINATE   = IF_FEATURE_XARGS_SUPPORT_TERMOPT(     (1 << OPTBIT_TERMINATE  )) + 0,
	OPT_ZEROTERM    = IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(   (1 << OPTBIT_ZEROTERM   )) + 0,
};
#define OPTION_STR "+trn:s:e::E:" \
	IF_FEATURE_XARGS_SUPPORT_CONFIRMATION("p") \
	IF_FEATURE_XARGS_SUPPORT_TERMOPT(     "x") \
	IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(   "0")

int xargs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int xargs_main(int argc, char **argv)
{
	int i;
	int child_error = 0;
	char *max_args;
	char *max_chars;
	char *buf;
	unsigned opt;
	int n_max_chars;
	int n_max_arg;
#if ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM
	char* FAST_FUNC (*read_args)(int, int, char*) = process_stdin;
#else
#define read_args process_stdin
#endif

	INIT_G();

#if ENABLE_DESKTOP && ENABLE_LONG_OPTS
	/* For example, Fedora's build system uses --no-run-if-empty */
	applet_long_options =
		"no-run-if-empty\0" No_argument "r"
		;
#endif
	opt = getopt32(argv, OPTION_STR, &max_args, &max_chars, &G.eof_str, &G.eof_str);

	/* -E ""? You may wonder why not just omit -E?
	 * This is used for portability:
	 * old xargs was using "_" as default for -E / -e */
	if ((opt & OPT_EOF_STRING1) && G.eof_str[0] == '\0')
		G.eof_str = NULL;

	if (opt & OPT_ZEROTERM)
		IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(read_args = process0_stdin);

	argv += optind;
	argc -= optind;
	if (!argv[0]) {
		/* default behavior is to echo all the filenames */
		*--argv = (char*)"echo";
		argc++;
	}

	/* -s NUM default. fileutils-4.4.2 uses 128k, but I heasitate
	 * to use such a big value - first need to change code to use
	 * growable buffer instead of fixed one.
	 */
	n_max_chars = 32 * 1024;
	/* Make smaller if system does not allow our default value.
	 * The Open Group Base Specifications Issue 6:
	 * "The xargs utility shall limit the command line length such that
	 * when the command line is invoked, the combined argument
	 * and environment lists (see the exec family of functions
	 * in the System Interfaces volume of IEEE Std 1003.1-2001)
	 * shall not exceed {ARG_MAX}-2048 bytes".
	 */
	{
		long arg_max = 0;
#if defined _SC_ARG_MAX
		arg_max = sysconf(_SC_ARG_MAX) - 2048;
#elif defined ARG_MAX
		arg_max = ARG_MAX - 2048;
#endif
		if (arg_max > 0 && n_max_chars > arg_max)
			n_max_chars = arg_max;
	}
	if (opt & OPT_UPTO_SIZE) {
		n_max_chars = xatou_range(max_chars, 1, INT_MAX);
	}
	/* Account for prepended fixed arguments */
	{
		size_t n_chars = 0;
		for (i = 0; argv[i]; i++) {
			n_chars += strlen(argv[i]) + 1;
		}
		n_max_chars -= n_chars;
	}
	/* Sanity check */
	if (n_max_chars <= 0) {
		bb_error_msg_and_die("can't fit single argument within argument list size limit");
	}

	buf = xzalloc(n_max_chars + 1);

	n_max_arg = n_max_chars;
	if (opt & OPT_UPTO_NUMBER) {
		n_max_arg = xatou_range(max_args, 1, INT_MAX);
		/* Not necessary, we use growable args[]: */
		/* if (n_max_arg > n_max_chars) n_max_arg = n_max_chars */
	}

	/* Allocate pointers for execvp */
	/* We can statically allocate (argc + n_max_arg + 1) elements
	 * and do not bother with resizing args[], but on 64-bit machines
	 * this results in args[] vector which is ~8 times bigger
	 * than n_max_chars! That is, with n_max_chars == 20k,
	 * args[] will take 160k (!), which will most likely be
	 * almost entirely unused.
	 */
	/* See store_param() for matching 256-step growth logic */
	G.args = xmalloc(sizeof(G.args[0]) * ((argc + 0xff) & ~0xff));

	/* Store the command to be executed, part 1 */
	for (i = 0; argv[i]; i++)
		G.args[i] = argv[i];

	while (1) {
		char *rem;

		G.idx = argc;
		rem = read_args(n_max_chars, n_max_arg, buf);
		store_param(NULL);

		if (!G.args[argc]) {
			if (*rem != '\0')
				bb_error_msg_and_die("argument line too long");
			if (opt & OPT_NO_EMPTY)
				break;
		}
		opt |= OPT_NO_EMPTY;

		if (opt & (OPT_INTERACTIVE | OPT_VERBOSE)) {
			const char *fmt = " %s" + 1;
			char **args = G.args;
			for (i = 0; args[i]; i++) {
				fprintf(stderr, fmt, args[i]);
				fmt = " %s";
			}
			if (!(opt & OPT_INTERACTIVE))
				bb_putchar_stderr('\n');
		}

		if (!(opt & OPT_INTERACTIVE) || xargs_ask_confirmation()) {
			child_error = xargs_exec();
		}

		if (child_error > 0 && child_error != 123) {
			break;
		}

		overlapping_strcpy(buf, rem);
	} /* while */

	if (ENABLE_FEATURE_CLEAN_UP) {
		free(G.args);
		free(buf);
	}

	return child_error;
}


#ifdef TEST

const char *applet_name = "debug stuff usage";

void bb_show_usage(void)
{
	fprintf(stderr, "Usage: %s [-p] [-r] [-t] -[x] [-n max_arg] [-s max_chars]\n",
		applet_name);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	return xargs_main(argc, argv);
}
#endif /* TEST */

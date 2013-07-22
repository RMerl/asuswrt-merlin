/*
 * pg  - A clone of the System V CRT paging utility.
 *
 *	Copyright (c) 2000-2001 Gunnar Ritter. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. [deleted]
 * 4. Neither the name of Gunnar Ritter nor the names of his contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GUNNAR RITTER AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL GUNNAR RITTER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Sccsid @(#)pg.c 1.44 (gritter) 2/8/02 - modified for util-linux */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#ifndef	TIOCGWINSZ
#include <sys/ioctl.h>
#endif
#include <sys/termios.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <libgen.h>

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include <term.h>

#include "nls.h"
#include "xalloc.h"
#include "widechar.h"
#include "writeall.h"

#define	READBUF		LINE_MAX	/* size of input buffer */
#define CMDBUF		255		/* size of command buffer */
#define	TABSIZE		8		/* spaces consumed by tab character */

/*
 * Avoid the message "`var' might be clobbered by `longjmp' or `vfork'"
 */
#define	CLOBBGRD(a)	(void)(&(a));

#define	cuc(c)		((c) & 0377)

enum { FORWARD = 1, BACKWARD = 2 };	/* search direction */
enum { TOP, MIDDLE, BOTTOM };		/* position of matching line */

/*
 * States for syntax-aware command line editor.
 */
enum {
	COUNT,
	SIGN,
	CMD_FIN,
	SEARCH,
	SEARCH_FIN,
	ADDON_FIN,
	STRING,
	INVALID
};

/*
 * Current command
 */
struct {
	char cmdline[CMDBUF];
	size_t cmdlen;
	int count;
	int key;
	char pattern[CMDBUF];
	char addon;
} cmd;

/*
 * Position of file arguments on argv[] to main()
 */
struct {
	int first;
	int current;
	int last;
} files;

void		(*oldint)(int);		/* old SIGINT handler */
void		(*oldquit)(int);	/* old SIGQUIT handler */
void		(*oldterm)(int);	/* old SIGTERM handler */
char		*tty;			/* result of ttyname(1) */
char		*progname;		/* program name */
unsigned	ontty;			/* whether running on tty device */
unsigned	exitstatus;		/* exit status */
int		pagelen = 23;		/* lines on a single screen page */
int		ttycols = 79;		/* screen columns (starting at 0) */
struct termios	otio;			/* old termios settings */
int		tinfostat = -1;		/* terminfo routines initialized */
int		searchdisplay = TOP;	/* matching line position */
regex_t		re;			/* regular expression to search for */
int		remembered;		/* have a remembered search string */
int		cflag;			/* clear screen before each page */
int		eflag;			/* suppress (EOF) */
int		fflag;			/* do not split lines */
int		nflag;			/* no newline for commands required */
int		rflag;			/* "restricted" pg */
int		sflag;			/* use standout mode */
char		*pstring = ":";		/* prompt string */
char		*searchfor;		/* search pattern from argv[] */
int		havepagelen;		/* page length is manually defined */
long		startline;		/* start line from argv[] */
int		nextfile = 1;		/* files to advance */
jmp_buf		jmpenv;			/* jump from signal handlers */
int		canjump;		/* jmpenv is valid */
wchar_t		wbuf[READBUF];		/* used in several widechar routines */

const char *copyright =
"@(#)pg 1.44 2/8/02. Copyright (c) 2000-2001 Gunnar Ritter. ";
const char *helpscreen = N_("All rights reserved.\n\
-------------------------------------------------------\n\
  h                       this screen\n\
  q or Q                  quit program\n\
  <newline>               next page\n\
  f                       skip a page forward\n\
  d or ^D                 next halfpage\n\
  l                       next line\n\
  $                       last page\n\
  /regex/                 search forward for regex\n\
  ?regex? or ^regex^      search backward for regex\n\
  . or ^L                 redraw screen\n\
  w or z                  set page size and go to next page\n\
  s filename              save current file to filename\n\
  !command                shell escape\n\
  p                       go to previous file\n\
  n                       go to next file\n\
\n\
Many commands accept preceding numbers, for example:\n\
+1<newline> (next page); -1<newline> (previous page); 1<newline> (first page).\n\
\n\
See pg(1) for more information.\n\
-------------------------------------------------------\n");

#ifndef HAVE_FSEEKO
  static int fseeko(FILE *f, off_t off, int whence) {
	return fseek(f, (long) off, whence);
  }
  static off_t ftello(FILE *f) {
	return (off_t) ftell(f);
  }
#endif

#ifdef USE_SIGSET	/* never defined */
/* sigset and sigrelse are obsolete - use when POSIX stuff is unavailable */
#define my_sigset	sigset
#define my_sigrelse	sigrelse
#else
static int my_sigrelse(int sig) {
	sigset_t sigs;

	if (sigemptyset(&sigs) || sigaddset(&sigs, sig))
		return -1;
	return sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}
typedef void (*my_sighandler_t)(int);
static my_sighandler_t my_sigset(int sig, my_sighandler_t disp) {
	struct sigaction act, oact;

	act.sa_handler = disp;
	if (sigemptyset(&act.sa_mask))
		return SIG_ERR;
	act.sa_flags = 0;
	if (sigaction(sig, &act, &oact))
		return SIG_ERR;
	if (my_sigrelse(sig))
		return SIG_ERR;
	return oact.sa_handler;
}
#endif

/*
 * Quit pg.
 */
static void
quit(int status)
{
	exit(status < 0100 ? status : 077);
}

/*
 * Usage message and similar routines.
 */
static void
usage(void)
{
	fprintf(stderr, _("%s: Usage: %s [-number] [-p string] [-cefnrs] "
			  "[+line] [+/pattern/] [files]\n"),
			progname, progname);
	quit(2);
}

static void
needarg(char *s)
{
	fprintf(stderr, _("%s: option requires an argument -- %s\n"),
		progname, s);
	usage();
}

static void
invopt(char *s)
{
	fprintf(stderr, _("%s: illegal option -- %s\n"), progname, s);
	usage();
}

#ifdef HAVE_WIDECHAR
/*
 * A mbstowcs()-alike function that transparently handles invalid sequences.
 */
static size_t
xmbstowcs(wchar_t *pwcs, const char *s, size_t nwcs)
{
	size_t n = nwcs;
	int c;

	ignore_result( mbtowc(pwcs, NULL, MB_CUR_MAX) );	/* reset shift state */
	while (*s && n) {
		if ((c = mbtowc(pwcs, s, MB_CUR_MAX)) < 0) {
			s++;
			*pwcs = L'?';
		} else
			s += c;
		pwcs++;
		n--;
	}
	if (n)
		*pwcs = L'\0';
	ignore_result( mbtowc(pwcs, NULL, MB_CUR_MAX) );
	return nwcs - n;
}
#endif

/*
 * Helper function for tputs().
 */
static int
outcap(int i)
{
	char c = i;
	return write_all(1, &c, 1) == 0 ? 1 : -1;
}

/*
 * Write messages to terminal.
 */
static void
mesg(char *message)
{
	if (ontty == 0)
		return;
	if (*message != '\n' && sflag)
		vidputs(A_STANDOUT, outcap);
	write_all(1, message, strlen(message));
	if (*message != '\n' && sflag)
		vidputs(A_NORMAL, outcap);
}

/*
 * Get the window size.
 */
static void
getwinsize(void)
{
	static int initialized, envlines, envcols, deflines, defcols;
#ifdef	TIOCGWINSZ
	struct winsize winsz;
	int badioctl;
#endif
	char *p;

	if (initialized == 0) {
		if ((p = getenv("LINES")) != NULL && *p != '\0')
			if ((envlines = atoi(p)) < 0)
				envlines = 0;
		if ((p = getenv("COLUMNS")) != NULL && *p != '\0')
			if ((envcols = atoi(p)) < 0)
				envcols = 0;
		/* terminfo values. */
		if (tinfostat != 1 || columns == 0)
			defcols = 24;
		else
			defcols = columns;
		if (tinfostat != 1 || lines == 0)
			deflines = 80;
		else
			deflines = lines;
		initialized = 1;
	}
#ifdef	TIOCGWINSZ
	badioctl = ioctl(1, TIOCGWINSZ, &winsz);
#endif
	if (envcols)
		ttycols = envcols - 1;
#ifdef	TIOCGWINSZ
	else if (!badioctl)
		ttycols = winsz.ws_col - 1;
#endif
	else
		ttycols = defcols - 1;
	if (havepagelen == 0) {
		if (envlines)
			pagelen = envlines - 1;
#ifdef	TIOCGWINSZ
		else if (!badioctl)
			pagelen = winsz.ws_row - 1;
#endif
		else
			pagelen = deflines - 1;
	}
}

/*
 * Message if skipping parts of files.
 */
static void
skip(int direction)
{
	if (direction > 0)
		mesg(_("...skipping forward\n"));
	else
		mesg(_("...skipping backward\n"));
}

/*
 * Signal handler while reading from input file.
 */
static void
sighandler(int signum)
{
	if (canjump && (signum == SIGINT || signum == SIGQUIT))
		longjmp(jmpenv, signum);
	tcsetattr(1, TCSADRAIN, &otio);
	quit(exitstatus);
}

/*
 * Check whether the requested file was specified on the command line.
 */
static int
checkf(void)
{
	if (files.current + nextfile >= files.last) {
		mesg(_("No next file"));
		return 1;
	}
	if (files.current + nextfile < files.first) {
		mesg(_("No previous file"));
		return 1;
	}
	return 0;
}

#ifdef HAVE_WIDECHAR
/*
 * Return the last character that will fit on the line at col columns
 * in case MB_CUR_MAX > 1.
 */
static char *
endline_for_mb(unsigned col, char *s)
{
	size_t pos = 0;
	wchar_t *p = wbuf;
	wchar_t *end;
	size_t wl;
	char *t = s;

	if ((wl = xmbstowcs(wbuf, t, sizeof wbuf - 1)) == (size_t)-1)
		return s + 1;
	wbuf[wl] = L'\0';
	while (*p != L'\0') {
		switch (*p) {
			/*
			 * Cursor left.
			 */
		case L'\b':
			if (pos > 0)
				pos--;
			break;
			/*
			 * No cursor movement.
			 */
		case L'\a':
			break;
			/*
			 * Special.
			 */
		case L'\r':
			pos = 0;
			break;
		case L'\n':
			end = p + 1;
			goto ended;
			/*
			 * Cursor right.
			 */
		case L'\t':
			pos += TABSIZE - (pos % TABSIZE);
			break;
		default:
			if (iswprint(*p))
				pos += wcwidth(*p);
			else
				pos += wcwidth(L'?');
		}
		if (pos > col) {
			if (*p == L'\t')
				p++;
			else if (pos > col + 1)
				/*
				 * wcwidth() found a character that
				 * has multiple columns. What happens
				 * now? Assume the terminal will print
				 * the entire character onto the next
				 * row.
				 */
				p--;
			if (*++p == L'\n')
				p++;
			end = p;
			goto ended;
		}
		p++;
	}
	end = p;
 ended:
	*end = L'\0';
	p = wbuf;
	if ((pos = wcstombs(NULL, p, READBUF)) == (size_t) -1)
		return s + 1;
	return s + pos;
}
#endif

/*
 * Return the last character that will fit on the line at col columns.
 */
static char *
endline(unsigned col, char *s)
{
	unsigned pos = 0;
	char *t = s;

#ifdef HAVE_WIDECHAR
	if (MB_CUR_MAX > 1)
		return endline_for_mb(col, s);
#endif

	while (*s != '\0') {
		switch (*s) {
			/*
			 * Cursor left.
			 */
		case '\b':
			if (pos > 0)
				pos--;
			break;
			/*
			 * No cursor movement.
			 */
		case '\a':
			break;
			/*
			 * Special.
			 */
		case '\r':
			pos = 0;
			break;
		case '\n':
			t = s + 1;
			goto cend;
			/*
			 * Cursor right.
			 */
		case '\t':
			pos += TABSIZE - (pos % TABSIZE);
			break;
		default:
			pos++;
		}
		if (pos > col) {
			if (*s == '\t')
				s++;
			if (*++s == '\n')
				s++;
			t = s;
			goto cend;
		}
		s++;
	}
	t = s;
 cend:
	return t;
}

/*
 * Clear the current line on the terminal's screen.
 */
static void
cline(void)
{
	char *buf = xmalloc(ttycols + 2);
	memset(buf, ' ', ttycols + 2);
	buf[0] = '\r';
	buf[ttycols + 1] = '\r';
	write_all(1, buf, ttycols + 2);
	free(buf);
}

/*
 * Evaluate a command character's semantics.
 */
static int
getstate(int c)
{
	switch (c) {
	case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9': case '0':
	case '\0':
		return COUNT;
	case '-': case '+':
		return SIGN;
	case 'l': case 'd': case '\004': case 'f': case 'z':
	case '.': case '\014': case '$': case 'n': case 'p':
	case 'w': case 'h': case 'q': case 'Q':
		return CMD_FIN;
	case '/': case '?': case '^':
		return SEARCH;
	case 's': case '!':
		return STRING;
	case 'm': case 'b': case 't':
		return ADDON_FIN;
	default:
#ifdef PG_BELL
		if (bell)
			tputs(bell, 1, outcap);
#endif  /*  PG_BELL  */
		return INVALID;
	}
}

/*
 * Get the count and ignore last character of string.
 */
static int
getcount(char *cmdstr)
{
	char *buf;
	char *p;
	int i;

	if (*cmdstr == '\0')
		return 1;
	buf = xmalloc(strlen(cmdstr) + 1);
	strcpy(buf, cmdstr);
	if (cmd.key != '\0') {
		if (cmd.key == '/' || cmd.key == '?' || cmd.key == '^') {
			if ((p = strchr(buf, cmd.key)) != NULL)
				*p = '\0';
		} else
			*(buf + strlen(buf) - 1) = '\0';
	}
	if (*buf == '\0')
		return 1;
	if (buf[0] == '-' && buf[1] == '\0') {
		i = -1;
	} else {
		if (*buf == '+')
			i = atoi(buf + 1);
		else
			i = atoi(buf);
	}
	free(buf);
	return i;
}

/*
 * Read what the user writes at the prompt. This is tricky because
 * we check for valid input.
 */
static void
prompt(long long pageno)
{
	struct termios tio;
	char key;
	int state = COUNT;
	int escape = 0;
	char b[LINE_MAX], *p;

	if (pageno != -1) {
		if ((p = strstr(pstring, "%d")) == NULL) {
			mesg(pstring);
		} else {
			strcpy(b, pstring);
			sprintf(b + (p - pstring), "%lld", pageno);
			strcat(b, p + 2);
			mesg(b);
		}
	}
	cmd.key = cmd.addon = cmd.cmdline[0] = '\0';
	cmd.cmdlen = 0;
	tcgetattr(1, &tio);
	tio.c_lflag &= ~(ICANON | ECHO);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	tcsetattr(1, TCSADRAIN, &tio);
	tcflush(1, TCIFLUSH);
	for (;;) {
		switch (read(1, &key, 1)) {
		case 0: quit(0);
			/*NOTREACHED*/
		case -1: quit(1);
		}
		if (key == tio.c_cc[VERASE]) {
			if (cmd.cmdlen) {
				write_all(1, "\b \b", 3);
				cmd.cmdline[--cmd.cmdlen] = '\0';
				switch (state) {
				case ADDON_FIN:
					state = SEARCH_FIN;
					cmd.addon = '\0';
					break;
				case CMD_FIN:
					cmd.key = '\0';
					state = COUNT;
					break;
				case SEARCH_FIN:
					state = SEARCH;
					/*FALLTHRU*/
				case SEARCH:
					if (cmd.cmdline[cmd.cmdlen - 1]
							== '\\') {
						escape = 1;
						while(cmd.cmdline[cmd.cmdlen
								- escape - 1]
							== '\\') escape++;
						escape %= 2;
					}
					else {
						escape = 0;
						if (strchr(cmd.cmdline, cmd.key)
							== NULL) {
							cmd.key = '\0';
							state = COUNT;
						}
					}
					break;
				}
			}
			if (cmd.cmdlen == 0) {
				state = COUNT;
				cmd.key = '\0';
			}
			continue;
		}
		if (key == tio.c_cc[VKILL]) {
			cline();
			cmd.cmdlen = 0;
			cmd.cmdline[0] = '\0';
			state = COUNT;
			cmd.key = '\0';
			continue;
		}
		if (key == '\n' || (nflag && state == COUNT && key == ' '))
			break;
		if (cmd.cmdlen >= CMDBUF - 1)
			continue;
		switch (state) {
		case STRING:
			break;
		case SEARCH:
			if (!escape) {
				if (key == cmd.key)
					state = SEARCH_FIN;
				if (key == '\\')
					escape = 1;
			} else
				escape = 0;
			break;
		case SEARCH_FIN:
			if (getstate(key) != ADDON_FIN)
				continue;
			state = ADDON_FIN;
			cmd.addon = key;
			switch (key) {
			case 't':
				searchdisplay = TOP;
				break;
			case 'm':
				searchdisplay = MIDDLE;
				break;
			case 'b':
				searchdisplay = BOTTOM;
				break;
			}
			break;
		case CMD_FIN:
		case ADDON_FIN:
			continue;
		default:
			state = getstate(key);
			switch (state) {
			case SIGN:
				if (cmd.cmdlen != 0) {
					state = INVALID;
					continue;
				}
				state = COUNT;
				/*FALLTHRU*/
			case COUNT:
				break;
			case ADDON_FIN:
			case INVALID:
				continue;
			default:
				cmd.key = key;
			}
		}
		write_all(1, &key, 1);
		cmd.cmdline[cmd.cmdlen++] = key;
		cmd.cmdline[cmd.cmdlen] = '\0';
		if (nflag && state == CMD_FIN)
			goto endprompt;
	}
endprompt:
	tcsetattr(1, TCSADRAIN, &otio);
	cline();
	cmd.count = getcount(cmd.cmdline);
}

#ifdef HAVE_WIDECHAR
/*
 * Remove backspace formatting, for searches
 * in case MB_CUR_MAX > 1.
 */
static char *
colb_for_mb(char *s)
{
	char *p = s;
	wchar_t *wp, *wq;
	size_t l = strlen(s), wl;
	unsigned i;

	if ((wl = xmbstowcs(wbuf, p, sizeof wbuf)) == (size_t)-1)
		return s;
	for (wp = wbuf, wq = wbuf, i = 0; *wp != L'\0' && i < wl;
	     wp++, wq++) {
		if (*wp == L'\b') {
			if (wq != wbuf)
				wq -= 2;
			else
				wq--;
		} else
			*wq = *wp;
	}
	*wq = L'\0';
	wp = wbuf;
	wcstombs(s, wp, l + 1);

	return s;
}
#endif

/*
 * Remove backspace formatting, for searches.
 */
static char *
colb(char *s)
{
	char *p = s, *q;

#ifdef HAVE_WIDECHAR
	if (MB_CUR_MAX > 1)
		return colb_for_mb(s);
#endif

	for (q = s; *p != '\0'; p++, q++) {
		if (*p == '\b') {
			if (q != s)
				q -= 2;
			else
				q--;
		} else
			*q = *p;
	}
	*q = '\0';

	return s;
}

#ifdef HAVE_WIDECHAR
/*
 * Convert nonprintable characters to spaces
 * in case MB_CUR_MAX > 1.
 */
static void
makeprint_for_mb(char *s, size_t l)
{
	char *t = s;
	wchar_t *wp = wbuf;
	size_t wl;

	if ((wl = xmbstowcs(wbuf, t, sizeof wbuf)) == (size_t)-1)
		return;
	while (wl--) {
		if (!iswprint(*wp) && *wp != L'\n' && *wp != L'\r'
		    && *wp != L'\b' && *wp != L'\t')
			*wp = L'?';
		wp++;
	}
	wp = wbuf;
	wcstombs(s, wp, l);
}
#endif

/*
 * Convert nonprintable characters to spaces.
 */
static void
makeprint(char *s, size_t l)
{
#ifdef HAVE_WIDECHAR
	if (MB_CUR_MAX > 1) {
		makeprint_for_mb(s, l);
		return;
	}
#endif

	while (l--) {
		if (!isprint(cuc(*s)) && *s != '\n' && *s != '\r'
		    && *s != '\b' && *s != '\t')
			*s = '?';
		s++;
	}
}

/*
 * Strip backslash characters from the given string.
 */
static void
striprs(char *s)
{
	char *p = s;

	do {
		if (*s == '\\') {
			s++;
		}
		*p++ = *s;
	} while (*s++ != '\0');
}

/*
 * Extract the search pattern off the command line.
 */
static char *
makepat(void)
{
	char *p;

	if (cmd.addon == '\0')
		p = cmd.cmdline + strlen(cmd.cmdline) - 1;
	else
		p = cmd.cmdline + strlen(cmd.cmdline) - 2;
	if (*p == cmd.key)
		*p = '\0';
	else
		*(p + 1) = '\0';
	if ((p = strchr(cmd.cmdline, cmd.key)) != NULL) {
		p++;
		striprs(p);
	}
	return p;
}

/*
 * Process errors that occurred in temporary file operations.
 */
static void
tmperr(FILE *f, char *ftype)
{
	if (ferror(f))
		fprintf(stderr, _("%s: Read error from %s file\n"),
			progname, ftype);
	else if (feof(f))
		/*
		 * Most likely '\0' in input.
		 */
		fprintf(stderr, _("%s: Unexpected EOF in %s file\n"),
			progname, ftype);
	else
		fprintf(stderr, _("%s: Unknown error in %s file\n"),
			progname, ftype);
	quit(++exitstatus);
}

/*
 * perror()-like, but showing the program's name.
 */
static void
pgerror(int eno, char *string)
{
	fprintf(stderr, "%s: %s: %s\n", progname, string, strerror(eno));
}

/*
 * Read the file and respond to user input.
 * Beware: long and ugly.
 */
static void
pgfile(FILE *f, char *name)
{
	off_t pos, oldpos, fpos;
	off_t line = 0, fline = 0, bline = 0, oldline = 0, eofline = 0;
	int dline = 0;
	/*
	 * These are the line counters:
	 * line		the line desired to display
	 * fline	the current line of the input file
	 * bline	the current line of the file buffer
	 * oldline	the line before a search was started
	 * eofline	the last line of the file if it is already reached
	 * dline	the line on the display
	 */
	int search = 0;
	unsigned searchcount = 0;
	/*
	 * Advance to EOF immediately.
	 */
	int seekeof = 0;
	/*
	 * EOF has been reached by `line'.
	 */
	int eof = 0;
	/*
	 * f and fbuf refer to the same file.
	 */
	int nobuf = 0;
	int sig;
	int rerror;
	size_t sz;
	char b[READBUF + 1];
	char *p;
	/*
	 * fbuf		an exact copy of the input file as it gets read
	 * find		index table for input, one entry per line
	 * save		for the s command, to save to a file
	 */
	FILE *fbuf, *find, *save;

	/* silence compiler - it may warn about longjmp() */
	CLOBBGRD(line);
	CLOBBGRD(fline);
	CLOBBGRD(bline);
	CLOBBGRD(oldline);
	CLOBBGRD(eofline);
	CLOBBGRD(dline);
	CLOBBGRD(ttycols);
	CLOBBGRD(search);
	CLOBBGRD(searchcount);
	CLOBBGRD(seekeof);
	CLOBBGRD(eof);
	CLOBBGRD(fpos);
	CLOBBGRD(nobuf);
	CLOBBGRD(fbuf);

	if (ontty == 0) {
		/*
		 * Just copy stdin to stdout.
		 */
		while ((sz = fread(b, sizeof *b, READBUF, f)) != 0)
			write_all(1, b, sz);
		if (ferror(f)) {
			pgerror(errno, name);
			exitstatus++;
		}
		return;
	}
	if ((fpos = fseeko(f, (off_t)0, SEEK_SET)) == -1)
		fbuf = tmpfile();
	else {
		fbuf = f;
		nobuf = 1;
	}
	find = tmpfile();
	if (fbuf == NULL || find == NULL) {
		fprintf(stderr, _("%s: Cannot create tempfile\n"), progname);
		quit(++exitstatus);
	}
	if (searchfor) {
		search = FORWARD;
		oldline = 0;
		searchcount = 1;
		rerror = regcomp(&re, searchfor, REG_NOSUB | REG_NEWLINE);
		if (rerror != 0) {
			mesg(_("RE error: "));
			regerror(rerror, &re, b, READBUF);
			mesg(b);
			goto newcmd;
		}
		remembered = 1;
	}

	for (line = startline; ; ) {
		/*
		 * Get a line from input file or buffer.
		 */
		if (line < bline) {
			fseeko(find, line * sizeof pos, SEEK_SET);
			if (fread(&pos, sizeof pos, 1, find) == 0)
				tmperr(find, "index");
			fseeko(find, (off_t)0, SEEK_END);
			fseeko(fbuf, pos, SEEK_SET);
			if (fgets(b, READBUF, fbuf) == NULL)
				tmperr(fbuf, "buffer");
		} else if (eofline == 0) {
			fseeko(find, (off_t)0, SEEK_END);
			do {
				if (!nobuf)
					fseeko(fbuf, (off_t)0, SEEK_END);
				pos = ftello(fbuf);
				if ((sig = setjmp(jmpenv)) != 0) {
					/*
					 * We got a signal.
					 */
					canjump = 0;
					my_sigrelse(sig);
					fseeko(fbuf, pos, SEEK_SET);
					*b = '\0';
					dline = pagelen;
					break;
				} else {
					if (nobuf)
						fseeko(f, fpos, SEEK_SET);
					canjump = 1;
					p = fgets(b, READBUF, f);
					if (nobuf)
						if ((fpos = ftello(f)) == -1)
							pgerror(errno, name);
					canjump = 0;
				}
				if (p == NULL || *b == '\0') {
					if (ferror(f))
						pgerror(errno, name);
					eofline = fline;
					eof = 1;
					break;
				} else {
					if (!nobuf)
						fputs(b, fbuf);
					fwrite_all(&pos, sizeof pos, 1, find);
					if (!fflag) {
						oldpos = pos;
						p = b;
						while (*(p = endline(ttycols,
									p))
								!= '\0') {
							pos = oldpos + (p - b);
							fwrite_all(&pos,
								sizeof pos,
								1, find);
							fline++;
							bline++;
						}
					}
					fline++;
				}
			} while (line > bline++);
		} else {
			/*
			 * eofline != 0
			 */
			eof = 1;
		}
		if (search == FORWARD && remembered == 1) {
			if (eof) {
				line = oldline;
				search = searchcount = 0;
				mesg(_("Pattern not found"));
				eof = 0;
				goto newcmd;
			}
			line++;
			colb(b);
			if (regexec(&re, b, 0, NULL, 0) == 0) {
				searchcount--;
			}
			if (searchcount == 0) {
				search = dline = 0;
				switch (searchdisplay) {
				case TOP:
					line -= 1;
					break;
				case MIDDLE:
					line -= pagelen / 2 + 1;
					break;
				case BOTTOM:
					line -= pagelen;
					break;
				}
				skip(1);
			}
			continue;
		} else if (eof)	{	/*
					 * We are not searching.
					 */
			line = bline;
		} else if (*b != '\0') {
			if (cflag && clear_screen) {
				switch (dline) {
				case 0:
					tputs(clear_screen, 1, outcap);
					dline = 0;
				}
			}
			line++;
			if (eofline && line == eofline)
				eof = 1;
			dline++;
			if ((sig = setjmp(jmpenv)) != 0) {
				/*
				 * We got a signal.
				 */
				canjump = 0;
				my_sigrelse(sig);
				dline = pagelen;
			} else {
				p = endline(ttycols, b);
				sz = p - b;
				makeprint(b, sz);
				canjump = 1;
				write_all(1, b, sz);
				canjump = 0;
			}
		}
		if (dline >= pagelen || eof) {
			/*
			 * Time for prompting!
			 */
			if (eof && seekeof) {
				eof = seekeof = 0;
				if (line >= pagelen)
					line -= pagelen;
				else
					line = 0;
				dline = -1;
				continue;
			}
newcmd:
			if (eof) {
				if (fline == 0 || eflag)
					break;
				mesg(_("(EOF)"));
			}
			prompt((line - 1) / pagelen + 1);
			switch (cmd.key) {
			case '/':
				/*
				 * Search forward.
				 */
				search = FORWARD;
				oldline = line;
				searchcount = cmd.count;
				p = makepat();
				if (p != NULL && *p) {
					if (remembered == 1)
						regfree(&re);
					rerror = regcomp(&re, p,
						REG_NOSUB | REG_NEWLINE);
					if (rerror != 0) {
						mesg(_("RE error: "));
						sz = regerror(rerror, &re,
								b, READBUF);
						mesg(b);
						goto newcmd;
					}
					remembered = 1;
				} else if (remembered == 0) {
					mesg(_("No remembered search string"));
					goto newcmd;
				}
				continue;
			case '?':
			case '^':
				/*
				 * Search backward.
				 */
				search = BACKWARD;
				oldline = line;
				searchcount = cmd.count;
				p = makepat();
				if (p != NULL && *p) {
					if (remembered == 1)
						regfree(&re);
					rerror = regcomp(&re, p,
						REG_NOSUB | REG_NEWLINE);
					if (rerror != 0) {
						mesg(_("RE error: "));
						regerror(rerror, &re,
								b, READBUF);
						mesg(b);
						goto newcmd;
					}
					remembered = 1;
				} else if (remembered == 0) {
					mesg(_("No remembered search string"));
					goto newcmd;
				}
				line -= pagelen;
				if (line <= 0)
					goto notfound_bw;
				while (line) {
					fseeko(find, --line * sizeof pos,
							SEEK_SET);
					if(fread(&pos, sizeof pos, 1,find)==0)
						tmperr(find, "index");
					fseeko(find, (off_t)0, SEEK_END);
					fseeko(fbuf, pos, SEEK_SET);
					if (fgets(b, READBUF, fbuf) == NULL)
						tmperr(fbuf, "buffer");
					colb(b);
					if (regexec(&re, b, 0, NULL, 0) == 0)
						searchcount--;
					if (searchcount == 0)
						goto found_bw;
				}
notfound_bw:
				line = oldline;
				search = searchcount = 0;
				mesg(_("Pattern not found"));
				goto newcmd;
found_bw:
				eof = search = dline = 0;
				skip(-1);
				switch (searchdisplay) {
				case TOP:
					/* line -= 1; */
					break;
				case MIDDLE:
					line -= pagelen / 2;
					break;
				case BOTTOM:
					if (line != 0)
						dline = -1;
					line -= pagelen;
					break;
				}
				if (line < 0)
					line = 0;
				continue;
			case 's':
				/*
				 * Save to file.
				 */
				p = cmd.cmdline;
				while (*++p == ' ');
				if (*p == '\0')
					goto newcmd;
				save = fopen(p, "wb");
				if (save == NULL) {
					cmd.count = errno;
					mesg(_("Cannot open "));
					mesg(p);
					mesg(": ");
					mesg(strerror(cmd.count));
					goto newcmd;
				}
				/*
				 * Advance to EOF.
				 */
				fseeko(find, (off_t)0, SEEK_END);
				for (;;) {
					if (!nobuf)
						fseeko(fbuf,(off_t)0,SEEK_END);
					pos = ftello(fbuf);
					if (fgets(b, READBUF, f) == NULL) {
						eofline = fline;
						break;
					}
					if (!nobuf)
						fputs(b, fbuf);
					fwrite_all(&pos, sizeof pos, 1, find);
					if (!fflag) {
						oldpos = pos;
						p = b;
						while (*(p = endline(ttycols,
									p))
								!= '\0') {
							pos = oldpos + (p - b);
							fwrite_all(&pos,
								sizeof pos,
								1, find);
							fline++;
							bline++;
						}
					}
					fline++;
					bline++;
				}
				fseeko(fbuf, (off_t)0, SEEK_SET);
				while ((sz = fread(b, sizeof *b, READBUF,
							fbuf)) != 0) {
					/*
					 * No error check for compat.
					 */
					fwrite_all(b, sizeof *b, sz, save);
				}
				fclose(save);
				fseeko(fbuf, (off_t)0, SEEK_END);
				mesg(_("saved"));
				goto newcmd;
			case 'l':
				/*
				 * Next line.
				 */
				if (*cmd.cmdline != 'l')
					eof = 0;
				if (cmd.count == 0)
					cmd.count = 1; /* compat */
				if (isdigit(cuc(*cmd.cmdline))) {
					line = cmd.count - 2;
					dline = 0;
				} else {
					if (cmd.count != 1) {
						line += cmd.count - 1
							- pagelen;
						dline = -1;
						skip(cmd.count);
					}
					/*
					 * Nothing to do if count==1.
					 */
				}
				break;
			case 'd':
				/*
				 * Half screen forward.
				 */
			case '\004':	/* ^D */
				if (*cmd.cmdline != cmd.key)
					eof = 0;
				if (cmd.count == 0)
					cmd.count = 1; /* compat */
				line += (cmd.count * pagelen / 2)
					- pagelen - 1;
				dline = -1;
				skip(cmd.count);
				break;
			case 'f':
				/*
				 * Skip forward.
				 */
				if (cmd.count <= 0)
					cmd.count = 1; /* compat */
				line += cmd.count * pagelen - 2;
				if (eof)
					line += 2;
				if (*cmd.cmdline != 'f')
					eof = 0;
				else if (eof)
					break;
				if (eofline && line >= eofline)
					line -= pagelen;
				dline = -1;
				skip(cmd.count);
				break;
			case '\0':
				/*
				 * Just a number, or '-', or <newline>.
				 */
				if (cmd.count == 0)
					cmd.count = 1; /* compat */
				if (isdigit(cuc(*cmd.cmdline)))
					line = (cmd.count - 1) * pagelen - 2;
				else
					line += (cmd.count - 1)
						* (pagelen - 1) - 2;
				if (*cmd.cmdline != '\0')
					eof = 0;
				if (cmd.count != 1) {
					skip(cmd.count);
					dline = -1;
				} else {
					dline = 1;
					line += 2;
				}
				break;
			case '$':
				/*
				 * Advance to EOF.
				 */
				if (!eof)
					skip(1);
				eof = 0;
				line = LONG_MAX;
				seekeof = 1;
				dline = -1;
				break;
			case '.':
			case '\014': /* ^L */
				/*
				 * Repaint screen.
				 */
				eof = 0;
				if (line >= pagelen)
					line -= pagelen;
				else
					line = 0;
				dline = 0;
				break;
			case '!':
				/*
				 * Shell escape.
				 */
				if (rflag) {
					mesg(progname);
					mesg(_(": !command not allowed in "
					       "rflag mode.\n"));
				} else {
					pid_t cpid;

					write_all(1, cmd.cmdline,
					      strlen(cmd.cmdline));
					write_all(1, "\n", 1);
					my_sigset(SIGINT, SIG_IGN);
					my_sigset(SIGQUIT, SIG_IGN);
					switch (cpid = fork()) {
					case 0:
						p = getenv("SHELL");
						if (p == NULL) p = "/bin/sh";
						if (!nobuf)
							fclose(fbuf);
						fclose(find);
						if (isatty(0) == 0) {
							close(0);
							open(tty, O_RDONLY);
						} else {
							fclose(f);
						}
						my_sigset(SIGINT, oldint);
						my_sigset(SIGQUIT, oldquit);
						my_sigset(SIGTERM, oldterm);
						execl(p, p, "-c",
							cmd.cmdline + 1, NULL);
						pgerror(errno, p);
						_exit(0177);
						/*NOTREACHED*/
					case -1:
						mesg(_("fork() failed, "
						       "try again later\n"));
						break;
					default:
						while (wait(NULL) != cpid);
					}
					my_sigset(SIGINT, sighandler);
					my_sigset(SIGQUIT, sighandler);
					mesg("!\n");
				}
				goto newcmd;
			case 'h':
			{
				/*
				 * Help!
				 */
				const char *help = _(helpscreen);
				write_all(1, copyright + 4, strlen(copyright + 4));
				write_all(1, help, strlen(help));
				goto newcmd;
			}
			case 'n':
				/*
				 * Next file.
				 */
				if (cmd.count == 0)
					cmd.count = 1;
				nextfile = cmd.count;
				if (checkf()) {
					nextfile = 1;
					goto newcmd;
				}
				eof = 1;
				break;
			case 'p':
				/*
				 * Previous file.
				 */
				if (cmd.count == 0)
					cmd.count = 1;
				nextfile = 0 - cmd.count;
				if (checkf()) {
					nextfile = 1;
					goto newcmd;
				}
				eof = 1;
				break;
			case 'q':
			case 'Q':
				/*
				 * Exit pg.
				 */
				quit(exitstatus);
				/*NOTREACHED*/
			case 'w':
			case 'z':
				/*
				 * Set window size.
				 */
				if (cmd.count < 0)
					cmd.count = 0;
				if (*cmd.cmdline != cmd.key)
					pagelen = ++cmd.count;
				dline = 1;
				break;
			}
			if (line <= 0) {
				line = 0;
				dline = 0;
			}
			if (cflag && dline == 1) {
				dline = 0;
				line--;
			}
		}
		if (eof)
			break;
	}
	fclose(find);
	if (!nobuf)
		fclose(fbuf);
}

int
main(int argc, char **argv)
{
	int arg, i;
	char *p;
	FILE *input;

	progname = basename(argv[0]);

	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (tcgetattr(1, &otio) == 0) {
		ontty = 1;
		oldint = my_sigset(SIGINT, sighandler);
		oldquit = my_sigset(SIGQUIT, sighandler);
		oldterm = my_sigset(SIGTERM, sighandler);
		setlocale(LC_CTYPE, "");
		setlocale(LC_COLLATE, "");
		tty = ttyname(1);
		setupterm(NULL, 1, &tinfostat);
		getwinsize();
		helpscreen = _(helpscreen);
	}
	for (arg = 1; argv[arg]; arg++) {
		if (*argv[arg] == '+')
			continue;
		if (*argv[arg] != '-' || argv[arg][1] == '\0')
			break;
		argc--;
		for (i = 1; argv[arg][i]; i++) {
			switch (argv[arg][i]) {
			case '-':
				if (i != 1 || argv[arg][i + 1])
					invopt(&argv[arg][i]);
				goto endargs;
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9': case '0':
				pagelen = atoi(argv[arg] + i);
				havepagelen = 1;
				goto nextarg;
			case 'c':
				cflag = 1;
				break;
			case 'e':
				eflag = 1;
				break;
			case 'f':
				fflag = 1;
				break;
			case 'n':
				nflag = 1;
				break;
			case 'p':
				if (argv[arg][i + 1]) {
					pstring = &argv[arg][i + 1];
				} else if (argv[++arg]) {
					--argc;
					pstring = argv[arg];
				} else
					needarg("-p");
				goto nextarg;
			case 'r':
				rflag = 1;
				break;
			case 's':
				sflag = 1;
				break;
			default:
				invopt(&argv[arg][i]);
			}
		}
nextarg:
		;
	}
endargs:
	for (arg = 1; argv[arg]; arg++) {
		if (*argv[arg] == '-') {
			if (argv[arg][1] == '-') {
				arg++;
				break;
			}
			if (argv[arg][1] == '\0')
				break;
			if (argv[arg][1] == 'p' && argv[arg][2] == '\0')
				arg++;
			continue;
		}
		if (*argv[arg] != '+')
			break;
		argc--;
		switch (*(argv[arg] + 1)) {
		case '\0':
			needarg("+");
			/*NOTREACHED*/
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': case '0':
			startline = atoi(argv[arg] + 1);
			break;
		case '/':
			searchfor = argv[arg] + 2;
			if (*searchfor == '\0')
				needarg("+/");
			p = searchfor + strlen(searchfor) - 1;
			if (*p == '/') *p = '\0';
			if (*searchfor == '\0')
				needarg("+/");
			break;
		default:
			invopt(argv[arg]);
		}
	}
	if (argc == 1) {
		pgfile(stdin, "stdin");
	} else {
		files.first = arg;
		files.last = arg + argc - 1;
		for ( ; argv[arg]; arg += nextfile) {
			nextfile = 1;
			files.current = arg;
			if (argc > 2) {
				static int firsttime;
				firsttime++;
				if (firsttime > 1) {
					mesg(_("(Next file: "));
					mesg(argv[arg]);
					mesg(")");
newfile:
					if (ontty) {
					prompt(-1);
					switch(cmd.key) {
					case 'n':
						/*
					 	* Next file.
					 	*/
						if (cmd.count == 0)
							cmd.count = 1;
						nextfile = cmd.count;
						if (checkf()) {
							nextfile = 1;
							mesg(":");
							goto newfile;
						}
						continue;
					case 'p':
						/*
					 	* Previous file.
					 	*/
						if (cmd.count == 0)
							cmd.count = 1;
						nextfile = 0 - cmd.count;
						if (checkf()) {
							nextfile = 1;
							mesg(":");
							goto newfile;
						}
						continue;
					case 'q':
					case 'Q':
						quit(exitstatus);
				}
				} else mesg("\n");
				}
			}
			if (strcmp(argv[arg], "-") == 0)
				input = stdin;
			else {
				input = fopen(argv[arg], "r");
				if (input == NULL) {
					pgerror(errno, argv[arg]);
					exitstatus++;
					continue;
				}
			}
			if (ontty == 0 && argc > 2) {
				/*
				 * Use the prefix as specified by SUSv2.
				 */
				write_all(1, "::::::::::::::\n", 15);
				write_all(1, argv[arg], strlen(argv[arg]));
				write_all(1, "\n::::::::::::::\n", 16);
			}
			pgfile(input, argv[arg]);
			if (input != stdin)
				fclose(input);
		}
	}
	quit(exitstatus);
	/*NOTREACHED*/
	return 0;
}

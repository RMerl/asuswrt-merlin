/*
 * Copyright (C) 1980 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
** more.c - General purpose tty output filter and file perusal program
**
**	by Eric Shienbrood, UC Berkeley
**
**	modified by Geoff Peck, UCB to add underlining, single spacing
**	modified by John Foderaro, UCB to add -c and MORE environment variable
**	modified by Erik Troan <ewt@redhat.com> to be more posix and so compile
**	  on linux/axp.
**	modified by Kars de Jong <jongk@cs.utwente.nl> to use terminfo instead
**	  of termcap.
	1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
	- added Native Language Support
	1999-03-19 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
	- more nls translatable strings
	1999-05-09 aeb - applied a RedHat patch (setjmp->sigsetjmp); without it
	a second ^Z would fail.
	1999-05-09 aeb - undone Kars' work, so that more works without
	libcurses (and hence can be in /bin with libcurses being in /usr/lib
	which may not be mounted). However, when termcap is not present curses
	can still be used.
	2010-10-21 Davidlohr Bueso <dave@gnu.org>
	- modified mem allocation handling for util-linux
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>		/* for alloca() */
#include <stdarg.h>		/* for va_start() etc */
#include <sys/param.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include "strutils.h"

#include "nls.h"
#include "xalloc.h"
#include "widechar.h"

#define _REGEX_RE_COMP
#include <regex.h>
#undef _REGEX_RE_COMP

#ifndef XTABS
#define XTABS TAB3
#endif /* XTABS */

#define VI		"vi"	/* found on the user's path */

#define Fopen(s,m)	(Currline = 0,file_pos=0,fopen(s,m))
#define Ftell(f)	file_pos
#define Fseek(f,off)	(file_pos=off,fseek(f,off,0))
#define Getc(f)		(++file_pos, getc(f))
#define Ungetc(c,f)	(--file_pos, ungetc(c,f))
#define putcerr(c)	fputc(c, stderr)
#define putserr(s)	fputs(s, stderr)
#define putsout(s)	fputs(s, stdout)

#define stty(fd,argp)  tcsetattr(fd,TCSANOW,argp)

/* some function declarations */
void initterm(void);
void kill_line(void);
void doclear(void);
void cleareol(void);
void clreos(void);
void home(void);
void error (char *mess);
void do_shell (char *filename);
int  colon (char *filename, int cmd, int nlines);
int  expand (char **outbuf, char *inbuf);
void argscan(char *s);
void rdline (register FILE *f);
void copy_file(register FILE *f);
void search(char buf[], FILE *file, register int n);
void skipf (register int nskip);
void skiplns(register int n, register FILE *f);
void screen (register FILE *f, register int num_lines);
int  command (char *filename, register FILE *f);
void erasep (register int col);
void show (register char ch);
void set_tty(void);
void reset_tty(void);
void ttyin (char buf[], register int nmax, char pchar);
int  number(char *cmd);
int  readch (void);
int  get_line(register FILE *f, int *length);
void prbuf (register char *s, register int n);
void execute (char *filename, char *cmd, ...);
FILE *checkf (char *, int *);
void prepare_line_buffer(void);

#define TBUFSIZ	1024
#define LINSIZ	256	/* minimal Line buffer size */
#define ctrl(letter)	(letter & 077)
#define RUBOUT	'\177'
#define ESC	'\033'
#define QUIT	'\034'
#define SCROLL_LEN	11
#define LINES_PER_PAGE	24
#define NUM_COLUMNS	80
#define TERMINAL_BUF	4096
#define INIT_BUF	80
#define SHELL_LINE	1000
#define COMMAND_BUF	200

struct termios	otty, savetty0;
long		file_pos, file_size;
int		fnum, no_intty, no_tty, slow_tty;
int		dum_opt, dlines;
void		onquit(int), onsusp(int), chgwinsz(int), end_it(int);
int		nscroll = SCROLL_LEN;	/* Number of lines scrolled by 'd' */
int		fold_opt = 1;		/* Fold long lines */
int		stop_opt = 1;		/* Stop after form feeds */
int		ssp_opt = 0;		/* Suppress white space */
int		ul_opt = 1;		/* Underline as best we can */
int		promptlen;
int		Currline;		/* Line we are currently at */
int		startup = 1;
int		firstf = 1;
int		notell = 1;
int		docrterase = 0;
int		docrtkill = 0;
int		bad_so;			/* True if overwriting does not turn
					   off standout */
int		inwait, Pause, errors;
int		within;			/* true if we are within a file,
					   false if we are between files */
int		hard, dumb, noscroll, hardtabs, clreol, eatnl;
int		catch_susp;		/* We should catch the SIGTSTP signal */
char		**fnames;		/* The list of file names */
int		nfiles;			/* Number of files left to process */
char		*shell;			/* The name of the shell to use */
int		shellp;			/* A previous shell command exists */
sigjmp_buf	restore;
char		*Line;			/* Line buffer */
size_t		LineLen;		/* size of Line buffer */
int		Lpp = LINES_PER_PAGE;	/* lines per page */
char		*Clear;			/* clear screen */
char		*eraseln;		/* erase line */
char		*Senter, *Sexit;	/* enter and exit standout mode */
char		*ULenter, *ULexit;	/* enter and exit underline mode */
char		*chUL;			/* underline character */
char		*chBS;			/* backspace character */
char		*Home;			/* go to home */
char		*cursorm;		/* cursor movement */
char		cursorhome[40];		/* contains cursor movement to home */
char		*EodClr;		/* clear rest of screen */
int		Mcol = NUM_COLUMNS;	/* number of columns */
int		Wrap = 1;		/* set if automargins */
int		soglitch;		/* terminal has standout mode glitch */
int		ulglitch;		/* terminal has underline mode glitch */
int		pstate = 0;		/* current UL state */
static int	magic(FILE *, char *);
struct {
    long chrctr, line;
} context, screen_start;
extern char	PC;		/* pad character */

#ifdef HAVE_NCURSES_H
# include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
# include <ncurses/ncurses.h>
#endif /* HAVE_NCURSES_H */

#if defined(HAVE_NCURSES_H) || defined(HAVE_NCURSES_NCURSES_H)
# include <term.h>			/* include after <curses.h> */

#define TERM_AUTO_RIGHT_MARGIN    "am"
#define TERM_CEOL                 "xhp"
#define TERM_CLEAR                "clear"
#define TERM_CLEAR_TO_LINE_END    "el"
#define TERM_CLEAR_TO_SCREEN_END  "ed"
#define TERM_COLS                 "cols"
#define TERM_CURSOR_ADDRESS       "cup"
#define TERM_EAT_NEW_LINE         "xenl"
#define TERM_ENTER_UNDERLINE      "smul"
#define TERM_EXIT_STANDARD_MODE   "rmso"
#define TERM_EXIT_UNDERLINE       "rmul"
#define TERM_HARD_COPY            "hc"
#define TERM_HOME                 "home"
#define TERM_LINE_DOWN            "cud1"
#define TERM_LINES                "lines"
#define TERM_OVER_STRIKE          "os"
#define TERM_PAD_CHAR             "pad"
#define TERM_STANDARD_MODE        "smso"
#define TERM_STD_MODE_GLITCH      "xmc"
#define TERM_UNDERLINE_CHAR       "uc"
#define TERM_UNDERLINE            "ul"

static void my_putstring(char *s) {
	tputs (s, fileno(stdout), putchar);		/* putp(s); */
}

static void my_setupterm(char *term, int fildes, int *errret) {
     setupterm(term, fildes, errret);
}

static int my_tgetnum(char *s) {
     return tigetnum(s);
}

static int my_tgetflag(char *s) {
     return tigetflag(s);
}

static char *my_tgetstr(char *s) {
     return tigetstr(s);
}

static char *my_tgoto(char *cap, int col, int row) {
     return tparm(cap, col, row);
}

#elif defined(HAVE_LIBTERMCAP)          /* !ncurses */

#include <termcap.h>

#define TERM_AUTO_RIGHT_MARGIN    "am"
#define TERM_CEOL                 "xs"
#define TERM_CLEAR                "cl"
#define TERM_CLEAR_TO_LINE_END    "ce"
#define TERM_CLEAR_TO_SCREEN_END  "cd"
#define TERM_COLS                 "co"
#define TERM_CURSOR_ADDRESS       "cm"
#define TERM_EAT_NEW_LINE         "xn"
#define TERM_ENTER_UNDERLINE      "us"
#define TERM_EXIT_STANDARD_MODE   "se"
#define TERM_EXIT_UNDERLINE       "ue"
#define TERM_HARD_COPY            "hc"
#define TERM_HOME                 "ho"
#define TERM_LINE_DOWN            "le"
#define TERM_LINES                "li"
#define TERM_OVER_STRIKE          "os"
#define TERM_PAD_CHAR             "pc"
#define TERM_STANDARD_MODE        "so"
#define TERM_STD_MODE_GLITCH      "sg"
#define TERM_UNDERLINE_CHAR       "uc"
#define TERM_UNDERLINE            "ul"

char termbuffer[TERMINAL_BUF];
char tcbuffer[TERMINAL_BUF];
char *strbuf = termbuffer;

static void my_putstring(char *s) {
     tputs (s, fileno(stdout), putchar);
}

static void my_setupterm(char *term, int fildes, int *errret) {
     *errret = tgetent(tcbuffer, term);
}

static int my_tgetnum(char *s) {
     return tgetnum(s);
}

static int my_tgetflag(char *s) {
     return tgetflag(s);
}

static char *my_tgetstr(char *s) {
     return tgetstr(s, &strbuf);
}

static char *my_tgoto(char *cap, int col, int row) {
     return tgoto(cap, col, row);
}

#endif /* HAVE_LIBTERMCAP */

static void __attribute__ ((__noreturn__)) usage(FILE *out)
{
     fprintf(out,
            _("Usage: %s [options] file...\n\n"),
	       program_invocation_short_name);
     fprintf(out,
	    _("Options:\n"
	      "  -d        display help instead of ring bell\n"
              "  -f        count logical, rather than screen lines\n"
              "  -l        suppress pause after form feed\n"
              "  -p        suppress scroll, clean screen and disblay text\n"
              "  -c        suppress scroll, display text and clean line ends\n"
              "  -u        suppress underlining\n"
              "  -s        squeeze multiple blank lines into one\n"
              "  -NUM      specify the number of lines per screenful\n"
              "  +NUM      display file beginning from line number NUM\n"
              "  +/STRING  display file beginning from search string match\n"
              "  -V        output version information and exit\n"));
       exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    FILE	*f;
    char	*s;
    char	*p;
    int		ch;
    int		left;
    int		prnames = 0;
    int		initopt = 0;
    int		srchopt = 0;
    int		clearit = 0;
    int		initline = 0;
    char	initbuf[INIT_BUF];

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    nfiles = argc;
    fnames = argv;
    setlocale(LC_ALL, "");
    initterm ();

    /* Auto set no scroll on when binary is called page */
    if (!(strcmp(program_invocation_short_name, "page")))
       noscroll++;

    prepare_line_buffer();

    nscroll = Lpp/2 - 1;
    if (nscroll <= 0)
	nscroll = 1;

    if ((s = getenv("MORE")) != NULL)
	    argscan(s);

    while (--nfiles > 0) {
	if ((ch = (*++fnames)[0]) == '-') {
	    argscan(*fnames+1);
	}
	else if (ch == '+') {
	    s = *fnames;
	    if (*++s == '/') {
		srchopt++;
		for (++s, p = initbuf; p < initbuf + (INIT_BUF - 1) && *s != '\0';)
		    *p++ = *s++;
		*p = '\0';
	    }
	    else {
		initopt++;
		for (initline = 0; *s != '\0'; s++)
		    if (isdigit (*s))
			initline = initline*10 + *s -'0';
		--initline;
	    }
	}
	else break;
    }
    /* allow clreol only if Home and eraseln and EodClr strings are
     *  defined, and in that case, make sure we are in noscroll mode
     */
    if (clreol) {
        if((Home == NULL) || (*Home == '\0') ||
	   (eraseln == NULL) || (*eraseln == '\0') ||
           (EodClr == NULL) || (*EodClr == '\0') )
	      clreol = 0;
	else noscroll = 1;
    }
    if (dlines == 0)
	    dlines = Lpp - 1;	/* was: Lpp - (noscroll ? 1 : 2) */
    left = dlines;
    if (nfiles > 1)
	prnames++;
    if (!no_intty && nfiles == 0)
	usage(stderr);
    else
	f = stdin;
    if (!no_tty) {
	signal(SIGQUIT, onquit);
	signal(SIGINT, end_it);
#ifdef SIGWINCH
	signal(SIGWINCH, chgwinsz);
#endif /* SIGWINCH */
	if (signal (SIGTSTP, SIG_IGN) == SIG_DFL) {
	    signal(SIGTSTP, onsusp);
	    catch_susp++;
	}
	stty (fileno(stderr), &otty);
    }
    if (no_intty) {
	if (no_tty)
	    copy_file (stdin);
	else {
	    if ((ch = Getc (f)) == '\f')
		doclear();
	    else {
		Ungetc (ch, f);
		if (noscroll && (ch != EOF)) {
		    if (clreol)
			home ();
		    else
			doclear ();
		}
	    }
	    if (srchopt)
	    {
		search (initbuf, stdin, 1);
		if (noscroll)
		    left--;
	    }
	    else if (initopt)
		skiplns (initline, stdin);
	    screen (stdin, left);
	}
	no_intty = 0;
	prnames++;
	firstf = 0;
    }

    while (fnum < nfiles) {
	if ((f = checkf (fnames[fnum], &clearit)) != NULL) {
	    context.line = context.chrctr = 0;
	    Currline = 0;
	    if (firstf) sigsetjmp (restore, 1);
	    if (firstf) {
		firstf = 0;
		if (srchopt) {
		    search (initbuf, f, 1);
		    if (noscroll)
			left--;
		}
		else if (initopt)
		    skiplns (initline, f);
	    }
	    else if (fnum < nfiles && !no_tty) {
		sigsetjmp (restore, 1);
		left = command (fnames[fnum], f);
	    }
	    if (left != 0) {
		if ((noscroll || clearit) && (file_size != LONG_MAX)) {
		    if (clreol)
			home ();
		    else
			doclear ();
		}
		if (prnames) {
		    if (bad_so)
			erasep (0);
		    if (clreol)
			cleareol ();
		    putsout("::::::::::::::");
		    if (promptlen > 14)
			erasep (14);
		    putchar('\n');
		    if(clreol) cleareol();
		    puts(fnames[fnum]);
		    if(clreol) cleareol();
		    puts("::::::::::::::");
		    if (left > Lpp - 4)
			left = Lpp - 4;
		}
		if (no_tty)
		    copy_file (f);
		else {
		    within++;
		    screen(f, left);
		    within = 0;
		}
	    }
	    sigsetjmp (restore, 1);
	    fflush(stdout);
	    fclose(f);
	    screen_start.line = screen_start.chrctr = 0L;
	    context.line = context.chrctr = 0L;
	}
	fnum++;
	firstf = 0;
    }
    reset_tty ();
    exit(EXIT_SUCCESS);
}

void argscan(char *s) {
	int seen_num = 0;

	while (*s != '\0') {
		switch (*s) {
		  case '0': case '1': case '2':
		  case '3': case '4': case '5':
		  case '6': case '7': case '8':
		  case '9':
			if (!seen_num) {
				dlines = 0;
				seen_num = 1;
			}
			dlines = dlines*10 + *s - '0';
			break;
		  case 'd':
			dum_opt = 1;
			break;
		  case 'l':
			stop_opt = 0;
			break;
		  case 'f':
			fold_opt = 0;
			break;
		  case 'p':
			noscroll++;
			break;
		  case 'c':
			clreol++;
			break;
		  case 's':
			ssp_opt = 1;
			break;
		  case 'u':
			ul_opt = 0;
			break;
		  case '-': case ' ': case '\t':
			break;
                  case 'V':
                        printf(_("more (%s)\n"), PACKAGE_STRING);
                        exit(EXIT_SUCCESS);
                        break;
		  default:
		        warnx(_("unknown option -%s"), s);
			usage(stderr);
			break;
		}
		s++;
	}
}


/*
** Check whether the file named by fs is an ASCII file which the user may
** access.  If it is, return the opened file. Otherwise return NULL.
*/

FILE *
checkf (fs, clearfirst)
	register char *fs;
	int *clearfirst;
{
	struct stat stbuf;
	register FILE *f;
	int c;

	if (stat (fs, &stbuf) == -1) {
		(void)fflush(stdout);
		if (clreol)
			cleareol ();
		perror(fs);
		return((FILE *)NULL);
	}
	if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
		printf(_("\n*** %s: directory ***\n\n"), fs);
		return((FILE *)NULL);
	}
	if ((f = Fopen(fs, "r")) == NULL) {
		(void)fflush(stdout);
		perror(fs);
		return((FILE *)NULL);
	}
	if (magic(f, fs))
		return((FILE *)NULL);
	fcntl(fileno(f), F_SETFD, FD_CLOEXEC );
	c = Getc(f);
	*clearfirst = (c == '\f');
	Ungetc (c, f);
	if ((file_size = stbuf.st_size) == 0)
		file_size = LONG_MAX;
	return(f);
}

/*
 * magic --
 *	check for file magic numbers.  This code would best be shared with
 *	the file(1) program or, perhaps, more should not try to be so smart.
 */
static int
magic(f, fs)
	FILE *f;
	char *fs;
{
	signed char twobytes[2];

	/* don't try to look ahead if the input is unseekable */
	if (fseek(f, 0L, SEEK_SET))
		return 0;

	if (fread(twobytes, 2, 1, f) == 1) {
		switch(twobytes[0] + (twobytes[1]<<8)) {
		case 0407:	/* a.out obj */
		case 0410:	/* a.out exec */
		case 0413:	/* a.out demand exec */
		case 0405:
		case 0411:
		case 0177545:
		case 0x457f:		/* simple ELF detection */
			printf(_("\n******** %s: Not a text file ********\n\n"), fs);
			(void)fclose(f);
			return 1;
		}
	}
	(void)fseek(f, 0L, SEEK_SET);		/* rewind() not necessary */
	return 0;
}

/*
** Print out the contents of the file f, one screenful at a time.
*/

#define STOP -10

void screen (register FILE *f, register int num_lines)
{
    register int c;
    register int nchars;
    int length;			/* length of current line */
    static int prev_len = 1;	/* length of previous line */

    for (;;) {
	while (num_lines > 0 && !Pause) {
	    if ((nchars = get_line (f, &length)) == EOF)
	    {
		if (clreol)
		    clreos();
		return;
	    }
	    if (ssp_opt && length == 0 && prev_len == 0)
		continue;
	    prev_len = length;
	    if (bad_so || ((Senter && *Senter == ' ') && (promptlen > 0)))
		erasep (0);
	    /* must clear before drawing line since tabs on some terminals
	     * do not erase what they tab over.
	     */
	    if (clreol)
		cleareol ();
	    prbuf (Line, length);
	    if (nchars < promptlen)
		erasep (nchars);	/* erasep () sets promptlen to 0 */
	    else promptlen = 0;
	    /* is this needed?
	     * if (clreol)
	     *	cleareol();	* must clear again in case we wrapped *
	     */
	    if (nchars < Mcol || !fold_opt)
		prbuf("\n", 1);	/* will turn off UL if necessary */
	    if (nchars == STOP)
		break;
	    num_lines--;
	}
	if (pstate) {
		my_putstring (ULexit);
		pstate = 0;
	}
	fflush(stdout);
	if ((c = Getc(f)) == EOF)
	{
	    if (clreol)
		clreos ();
	    return;
	}

	if (Pause && clreol)
	    clreos ();
	Ungetc (c, f);
	sigsetjmp (restore, 1);
	Pause = 0; startup = 0;
	if ((num_lines = command (NULL, f)) == 0)
	    return;
	if (hard && promptlen > 0)
		erasep (0);
	if (noscroll && num_lines >= dlines)
	{
	    if (clreol)
		home();
	    else
		doclear ();
	}
	screen_start.line = Currline;
	screen_start.chrctr = Ftell (f);
    }
}

/*
** Come here if a quit signal is received
*/

void onquit(int dummy __attribute__ ((__unused__)))
{
    signal(SIGQUIT, SIG_IGN);
    if (!inwait) {
	putchar ('\n');
	if (!startup) {
	    signal(SIGQUIT, onquit);
	    siglongjmp (restore, 1);
	}
	else
	    Pause++;
    }
    else if (!dum_opt && notell) {
	promptlen += fprintf(stderr, _("[Use q or Q to quit]"));
	notell = 0;
    }
    signal(SIGQUIT, onquit);
}

/*
** Come here if a signal for a window size change is received
*/

#ifdef SIGWINCH
void chgwinsz(int dummy __attribute__ ((__unused__)))
{
    struct winsize win;

    signal(SIGWINCH, SIG_IGN);
    if (ioctl(fileno(stdout), TIOCGWINSZ, &win) != -1) {
	if (win.ws_row != 0) {
	    Lpp = win.ws_row;
	    nscroll = Lpp/2 - 1;
	    if (nscroll <= 0)
		nscroll = 1;
	    dlines = Lpp - 1;	/* was: Lpp - (noscroll ? 1 : 2) */
	}
	if (win.ws_col != 0)
	    Mcol = win.ws_col;
    }
    (void) signal(SIGWINCH, chgwinsz);
}
#endif /* SIGWINCH */

/*
** Clean up terminal state and exit. Also come here if interrupt signal received
*/

void end_it (int dummy __attribute__ ((__unused__)))
{
    reset_tty ();
    if (clreol) {
	putchar ('\r');
	clreos ();
	fflush (stdout);
    }
    else if (!clreol && (promptlen > 0)) {
	kill_line ();
	fflush (stdout);
    }
    else
	putcerr('\n');
    _exit(EXIT_SUCCESS);
}

void copy_file(register FILE *f) {
    register int c;

    while ((c = getc(f)) != EOF)
	putchar(c);
}

#define ringbell()	putcerr('\007')

static void prompt (char *filename)
{
    if (clreol)
	cleareol ();
    else if (promptlen > 0)
	kill_line ();
    if (!hard) {
	promptlen = 0;
	if (Senter && Sexit) {
	    my_putstring (Senter);
	    promptlen += (2 * soglitch);
	}
	if (clreol)
	    cleareol ();
	promptlen += printf(_("--More--"));
	if (filename != NULL) {
	    promptlen += printf(_("(Next file: %s)"), filename);
	} else if (!no_intty) {
	    promptlen += printf("(%d%%)", (int) ((file_pos * 100) / file_size));
	}
	if (dum_opt) {
	    promptlen += printf(_("[Press space to continue, 'q' to quit.]"));
	}
	if (Senter && Sexit)
	    my_putstring (Sexit);
	if (clreol)
	    clreos ();
	fflush(stdout);
    }
    else
	ringbell();
    inwait++;
}

void prepare_line_buffer(void)
{
    char *nline;
    size_t nsz = Mcol * 4;

    if (LineLen >= nsz)
        return;

    if (nsz < LINSIZ)
        nsz = LINSIZ;

    nline = xrealloc(Line, nsz);
    Line = nline;
    LineLen = nsz;
}

/*
 * Get a logical line
 */

int get_line(register FILE *f, int *length)
{
    int	c;
    char *p;
    int	column;
    static int colflg;

#ifdef HAVE_WIDECHAR
    size_t i;
    wchar_t wc;
    int wc_width;
    mbstate_t state, state_bak;		/* Current status of the stream. */
    char mbc[MB_LEN_MAX];		/* Buffer for one multibyte char. */
    size_t mblength;			/* Byte length of multibyte char. */
    size_t mbc_pos = 0;			/* Postion of the MBC. */
    int use_mbc_buffer_flag = 0;	/* If 1, mbc has data. */
    int break_flag = 0;			/* If 1, exit while(). */
    long file_pos_bak = Ftell (f);

    memset (&state, '\0', sizeof (mbstate_t));
#endif /* HAVE_WIDECHAR */

    prepare_line_buffer();

    p = Line;
    column = 0;
    c = Getc (f);
    if (colflg && c == '\n') {
	Currline++;
	c = Getc (f);
    }
    while (p < &Line[LineLen - 1]) {
#ifdef HAVE_WIDECHAR
	if (fold_opt && use_mbc_buffer_flag && MB_CUR_MAX > 1) {
	    use_mbc_buffer_flag = 0;
	    state_bak = state;
	    mbc[mbc_pos++] = c;
process_mbc:
	    mblength = mbrtowc (&wc, mbc, mbc_pos, &state);

	    switch (mblength) {
	      case (size_t)-2:	  /* Incomplete multibyte character. */
		use_mbc_buffer_flag = 1;
		state = state_bak;
		break;

	      case (size_t)-1:	  /* Invalid as a multibyte character. */
		*p++ = mbc[0];
		state = state_bak;
		column++;
		file_pos_bak++;
		
		if (column >= Mcol) {
		    Fseek (f, file_pos_bak);
		} else {
		    memmove (mbc, mbc + 1, --mbc_pos);
		    if (mbc_pos > 0) {
			mbc[mbc_pos] = '\0';
			goto process_mbc;
		    }
		}
		break;

	      default:
		wc_width = wcwidth (wc);

		if (column + wc_width > Mcol) {
		    Fseek (f, file_pos_bak);
		    break_flag = 1;
		} else {
		    for (i = 0; i < mbc_pos; i++)
		      *p++ = mbc[i];
		    if (wc_width > 0)
		      column += wc_width;
		}
	    }

	    if (break_flag || column >= Mcol)
	      break;

	    c = Getc (f);
	    continue;
	}
#endif /* HAVE_WIDECHAR */
	if (c == EOF) {
	    if (p > Line) {
		*p = '\0';
		*length = p - Line;
		return (column);
	    }
	    *length = p - Line;
	    return (EOF);
	}
	if (c == '\n') {
	    Currline++;
	    break;
	}

	*p++ = c;
#if 0
	if (c == '\033') {      /* ESC */
		c = Getc(f);
		while (c > ' ' && c < '0' && p < &Line[LineLen - 1]) {
			*p++ = c;
			c = Getc(f);
		}
		if (c >= '0' && c < '\177' && p < &Line[LineLen - 1]) {
			*p++ = c;
			c = Getc(f);
			continue;
		}
	}
#endif /* 0 */
	if (c == '\t') {
	    if (!hardtabs || (column < promptlen && !hard)) {
		if (hardtabs && eraseln && !dumb) {
		    column = 1 + (column | 7);
		    my_putstring (eraseln);
		    promptlen = 0;
		}
		else {
		    for (--p; p < &Line[LineLen - 1];) {
			*p++ = ' ';
			if ((++column & 7) == 0)
			    break;
		    }
		    if (column >= promptlen) promptlen = 0;
		}
	    } else
		column = 1 + (column | 7);
	} else if (c == '\b' && column > 0) {
	    column--;
	} else if (c == '\r') {
	    int next = Getc(f);
	    if (next == '\n') {
		p--;
		Currline++;
		break;
	    }
	    Ungetc(next,f);
	    column = 0;
	} else if (c == '\f' && stop_opt) {
		p[-1] = '^';
		*p++ = 'L';
		column += 2;
		Pause++;
	} else if (c == EOF) {
	    *length = p - Line;
	    return (column);
	} else {
#ifdef HAVE_WIDECHAR
	    if (fold_opt && MB_CUR_MAX > 1) {
		memset (mbc, '\0', MB_LEN_MAX);
		mbc_pos = 0;
		mbc[mbc_pos++] = c;
		state_bak = state;

		mblength = mbrtowc (&wc, mbc, mbc_pos, &state);

		/* The value of mblength is always less than 2 here. */
		switch (mblength) {
		  case (size_t)-2:
		    p--;
		    file_pos_bak = Ftell (f) - 1;
		    state = state_bak;
		    use_mbc_buffer_flag = 1;
		    break;

		  case (size_t)-1:
		    state = state_bak;
		    column++;
		    break;

		  default:
		    wc_width = wcwidth (wc);
		    if (wc_width > 0)
		      column += wc_width;
		}
	    } else
#endif /* HAVE_WIDECHAR */
	      {
		if (isprint(c))
		   column++;
	      }
	}

	if (column >= Mcol && fold_opt)
		break;
	c = Getc (f);
    }
    if (column >= Mcol && Mcol > 0) {
	if (!Wrap) {
	    *p++ = '\n';
	}
    }
    colflg = column == Mcol && fold_opt;
    if (colflg && eatnl && Wrap) {
	*p++ = '\n'; /* simulate normal wrap */
    }
    *length = p - Line;
    *p = 0;
    return (column);
}

/*
** Erase the rest of the prompt, assuming we are starting at column col.
*/

void erasep (register int col)
{

    if (promptlen == 0)
	return;
    if (hard) {
	putchar ('\n');
    }
    else {
	if (col == 0)
	    putchar ('\r');
	if (!dumb && eraseln)
	    my_putstring (eraseln);
	else
	    for (col = promptlen - col; col > 0; col--)
		putchar (' ');
    }
    promptlen = 0;
}

/*
** Erase the current line entirely
*/

void kill_line()
{
    erasep(0);
    if (!eraseln || dumb)
	putchar('\r');
}

/*
 * force clear to end of line
 */
void cleareol()
{
    my_putstring(eraseln);
}

void clreos()
{
    my_putstring(EodClr);
}

/* Print a buffer of n characters */

void prbuf (register char *s, register int n)
{
    register char c;			/* next output character */
    register int state;			/* next output char's UL state */
#define wouldul(s,n)	((n) >= 2 && (((s)[0] == '_' && (s)[1] == '\b') || ((s)[1] == '\b' && (s)[2] == '_')))

    while (--n >= 0)
	if (!ul_opt)
	    putchar (*s++);
	else {
	    if (*s == ' ' && pstate == 0 && ulglitch && wouldul(s+1, n-1)) {
		s++;
		continue;
	    }
	    if ((state = wouldul(s, n)) != 0) {
		c = (*s == '_')? s[2] : *s ;
		n -= 2;
		s += 3;
	    } else
		c = *s++;
	    if (state != pstate) {
		if (c == ' ' && state == 0 && ulglitch && wouldul(s, n-1))
		    state = 1;
		else
		    my_putstring(state ? ULenter : ULexit);
	    }
	    if (c != ' ' || pstate == 0 || state != 0 || ulglitch == 0)
#ifdef HAVE_WIDECHAR
	    {
		wchar_t wc;
		size_t mblength;
		mbstate_t state;
		memset (&state, '\0', sizeof (mbstate_t));
		s--; n++;
		mblength = mbrtowc (&wc, s, n, &state);
		if (mblength == (size_t) -2 || mblength == (size_t) -1)
			mblength = 1;
		while (mblength--)
			putchar (*s++);
		n += mblength;
	    }
#else
	        putchar(c);
#endif /* HAVE_WIDECHAR */
	    if (state && *chUL) {
		putsout(chBS);
		my_putstring(chUL);
	    }
	    pstate = state;
	}
}

/*
**  Clear the screen
*/
void
doclear()
{
    if (Clear && !hard) {
	my_putstring(Clear);

	/* Put out carriage return so that system doesn't
	** get confused by escape sequences when expanding tabs
	*/
	putchar ('\r');
	promptlen = 0;
    }
}

/*
 * Go to home position
 */
void
home()
{
    my_putstring(Home);
}

static int lastcmd, lastarg, lastp;
static int lastcolon;
char shell_line[SHELL_LINE];

/*
** Read a command and do it. A command consists of an optional integer
** argument followed by the command character.  Return the number of lines
** to display in the next screenful.  If there is nothing more to display
** in the current file, zero is returned.
*/

int command (char *filename, register FILE *f)
{
    register int nlines;
    register int retval = 0;
    register int c;
    char colonch;
    int done;
    char comchar, cmdbuf[INIT_BUF];

#define ret(val) retval=val;done++;break

    done = 0;
    if (!errors)
	prompt (filename);
    else
	errors = 0;
    for (;;) {
	nlines = number (&comchar);
	lastp = colonch = 0;
	if (comchar == '.') {	/* Repeat last command */
		lastp++;
		comchar = lastcmd;
		nlines = lastarg;
		if (lastcmd == ':')
			colonch = lastcolon;
	}
	lastcmd = comchar;
	lastarg = nlines;
	if ((cc_t) comchar == otty.c_cc[VERASE]) {
	    kill_line ();
	    prompt (filename);
	    continue;
	}
	switch (comchar) {
	case ':':
	    retval = colon (filename, colonch, nlines);
	    if (retval >= 0)
		done++;
	    break;
	case 'b':
	case ctrl('B'):
	    {
		register int initline;

		if (no_intty) {
		    ringbell();
		    return (-1);
		}

		if (nlines == 0) nlines++;

		putchar ('\r');
		erasep (0);
		putchar('\n');
		if (clreol)
			cleareol ();
		if (nlines != 1)
			printf(_("...back %d pages"), nlines);
		else
			putsout(_("...back 1 page"));
		if (clreol)
			cleareol ();
		putchar('\n');

		initline = Currline - dlines * (nlines + 1);
		if (! noscroll)
		    --initline;
		if (initline < 0) initline = 0;
		Fseek(f, 0L);
		Currline = 0;	/* skiplns() will make Currline correct */
		skiplns(initline, f);
		if (! noscroll) {
		    ret(dlines + 1);
		}
		else {
		    ret(dlines);
		}
	    }
	case ' ':
	case 'z':
	    if (nlines == 0) nlines = dlines;
	    else if (comchar == 'z') dlines = nlines;
	    ret (nlines);
	case 'd':
	case ctrl('D'):
	    if (nlines != 0) nscroll = nlines;
	    ret (nscroll);
	case 'q':
	case 'Q':
	    end_it (0);
	case 's':
	case 'f':
	case ctrl('F'):
	    if (nlines == 0) nlines++;
	    if (comchar == 'f')
		nlines *= dlines;
	    putchar ('\r');
	    erasep (0);
	    putchar('\n');
	    if (clreol)
		cleareol ();
	    if (nlines == 1)
		    putsout(_("...skipping one line"));
	    else
		    printf(_("...skipping %d lines"), nlines);

	    if (clreol)
		cleareol ();
	    putchar('\n');

	    while (nlines > 0) {
		while ((c = Getc (f)) != '\n')
		    if (c == EOF) {
			retval = 0;
			done++;
			goto endsw;
		    }
		    Currline++;
		    nlines--;
	    }
	    ret (dlines);
	case '\n':
	    if (nlines != 0)
		dlines = nlines;
	    else
		nlines = 1;
	    ret (nlines);
	case '\f':
	    if (!no_intty) {
		doclear ();
		Fseek (f, screen_start.chrctr);
		Currline = screen_start.line;
		ret (dlines);
	    }
	    else {
		ringbell();
		break;
	    }
	case '\'':
	    if (!no_intty) {
		kill_line ();
		putsout(_("\n***Back***\n\n"));
		Fseek (f, context.chrctr);
		Currline = context.line;
		ret (dlines);
	    }
	    else {
		ringbell();
		break;
	    }
	case '=':
	    kill_line ();
	    promptlen = printf("%d", Currline);
	    fflush (stdout);
	    break;
	case 'n':
	    lastp++;
	case '/':
	    if (nlines == 0) nlines++;
	    kill_line ();
	    putchar('/');
	    promptlen = 1;
	    fflush (stdout);
	    if (lastp) {
		putcerr('\r');
		search (NULL, f, nlines);	/* Use previous r.e. */
	    }
	    else {
		ttyin (cmdbuf, sizeof(cmdbuf)-2, '/');
		putcerr('\r');
		search (cmdbuf, f, nlines);
	    }
	    ret (dlines-1);
	case '!':
	    do_shell (filename);
	    break;
	case '?':
	case 'h':
	    if (noscroll) doclear();
	    putsout(_("\n"
"Most commands optionally preceded by integer argument k.  "
"Defaults in brackets.\n"
"Star (*) indicates argument becomes new default.\n"));
	    puts("---------------------------------------"
		"----------------------------------------");
	    putsout(_(
"<space>                 Display next k lines of text [current screen size]\n"
"z                       Display next k lines of text [current screen size]*\n"
"<return>                Display next k lines of text [1]*\n"
"d or ctrl-D             Scroll k lines [current scroll size, initially 11]*\n"
"q or Q or <interrupt>   Exit from more\n"
"s                       Skip forward k lines of text [1]\n"
"f                       Skip forward k screenfuls of text [1]\n"
"b or ctrl-B             Skip backwards k screenfuls of text [1]\n"
"'                       Go to place where previous search started\n"
"=                       Display current line number\n"
"/<regular expression>   Search for kth occurrence of regular expression [1]\n"
"n                       Search for kth occurrence of last r.e [1]\n"
"!<cmd> or :!<cmd>       Execute <cmd> in a subshell\n"
"v                       Start up /usr/bin/vi at current line\n"
"ctrl-L                  Redraw screen\n"
":n                      Go to kth next file [1]\n"
":p                      Go to kth previous file [1]\n"
":f                      Display current file name and line number\n"
".                       Repeat previous command\n"));
	    puts("---------------------------------------"
		"----------------------------------------");
	    prompt(filename);
	    break;
	case 'v':	/* This case should go right before default */
	    if (!no_intty) {
		    /*
		     * Earlier: call vi +n file. This also works for emacs.
		     * POSIX: call vi -c n file (when editor is vi or ex).
		     */
		    char *editor, *p;
		    int n = (Currline - dlines <= 0 ? 1 :
			     Currline - (dlines + 1) / 2);
		    int split = 0;

		    editor = getenv("VISUAL");
		    if (editor == NULL || *editor == '\0')
			    editor = getenv("EDITOR");
		    if (editor == NULL || *editor == '\0')
			    editor = VI;

		    p = strrchr(editor, '/');
		    if (p)
			    p++;
		    else
			    p = editor;
		    if (!strcmp(p, "vi") || !strcmp(p, "ex")) {
			    sprintf(cmdbuf, "-c %d", n);
			    split = 1;
		    } else {
			    sprintf(cmdbuf, "+%d", n);
		    }

		    kill_line();
		    printf("%s %s %s", editor, cmdbuf, fnames[fnum]);
		    if (split) {
			    cmdbuf[2] = 0;
			    execute(filename, editor, editor, cmdbuf,
				    cmdbuf+3, fnames[fnum], (char *)0);
		    } else
			    execute(filename, editor, editor,
				    cmdbuf, fnames[fnum], (char *)0);
		    break;
	    }
	    /* fall through */
	default:
	    if (dum_opt) {
   		kill_line ();
		if (Senter && Sexit) {
		    my_putstring (Senter);
		    promptlen = printf(_("[Press 'h' for instructions.]"))
			    + 2 * soglitch;
		    my_putstring (Sexit);
		}
		else
		    promptlen = printf(_("[Press 'h' for instructions.]"));
		fflush (stdout);
	    }
	    else
		ringbell();
	    break;
	}
	if (done) break;
    }
    putchar ('\r');
endsw:
    inwait = 0;
    notell++;
    return (retval);
}

static char ch;

/*
 * Execute a colon-prefixed command.
 * Returns <0 if not a command that should cause
 * more of the file to be printed.
 */

int colon (char *filename, int cmd, int nlines) {
	if (cmd == 0)
		ch = readch ();
	else
		ch = cmd;
	lastcolon = ch;
	switch (ch) {
	case 'f':
		kill_line ();
		if (!no_intty)
			promptlen = printf(_("\"%s\" line %d"), fnames[fnum], Currline);
		else
			promptlen = printf(_("[Not a file] line %d"), Currline);
		fflush (stdout);
		return (-1);
	case 'n':
		if (nlines == 0) {
			if (fnum >= nfiles - 1)
				end_it (0);
			nlines++;
		}
		putchar ('\r');
		erasep (0);
		skipf (nlines);
		return (0);
	case 'p':
		if (no_intty) {
			ringbell();
			return (-1);
		}
		putchar ('\r');
		erasep (0);
		if (nlines == 0)
			nlines++;
		skipf (-nlines);
		return (0);
	case '!':
		do_shell (filename);
		return (-1);
	case 'q':
	case 'Q':
		end_it (0);
	default:
		ringbell();
		return (-1);
	}
}

/*
** Read a decimal number from the terminal. Set cmd to the non-digit which
** terminates the number.
*/

int number(char *cmd)
{
	register int i;

	i = 0; ch = otty.c_cc[VKILL];
	for (;;) {
		ch = readch ();
		if (isdigit(ch))
			i = i*10 + ch - '0';
		else if ((cc_t) ch == otty.c_cc[VKILL])
			i = 0;
		else {
			*cmd = ch;
			break;
		}
	}
	return (i);
}

void do_shell (char *filename)
{
	char cmdbuf[COMMAND_BUF];
	int rc;
	char *expanded;

	kill_line ();
	putchar('!');
	fflush (stdout);
	promptlen = 1;
	if (lastp)
		putsout(shell_line);
	else {
		ttyin (cmdbuf, sizeof(cmdbuf)-2, '!');
		expanded = 0;
		rc = expand (&expanded, cmdbuf);
		if (expanded) {
			if (strlen(expanded) < sizeof(shell_line))
				strcpy(shell_line, expanded);
			else
				rc = -1;
			free(expanded);
		}
		if (rc < 0) {
			putserr(_("  Overflow\n"));
			prompt (filename);
			return;
		} else if (rc > 0) {
			kill_line ();
			promptlen = printf("!%s", shell_line);
		}
	}
	fflush (stdout);
	putcerr('\n');
	promptlen = 0;
	shellp = 1;
	execute (filename, shell, shell, "-c", shell_line, 0);
}

/*
** Search for nth ocurrence of regular expression contained in buf in the file
*/

void search(char buf[], FILE *file, register int n)
{
    long startline = Ftell (file);
    register long line1 = startline;
    register long line2 = startline;
    register long line3 = startline;
    register int lncount;
    int saveln, rv;
    char *s;

    context.line = saveln = Currline;
    context.chrctr = startline;
    lncount = 0;
    if ((s = re_comp (buf)) != 0)
	error (s);
    while (!feof (file)) {
	line3 = line2;
	line2 = line1;
	line1 = Ftell (file);
	rdline (file);
	lncount++;
	if ((rv = re_exec (Line)) == 1) {
		if (--n == 0) {
		    if (lncount > 3 || (lncount > 1 && no_intty))
		    {
			putchar('\n');
			if (clreol)
			    cleareol ();
			putsout(_("...skipping\n"));
		    }
		    if (!no_intty) {
			Currline -= (lncount >= 3 ? 3 : lncount);
			Fseek (file, line3);
			if (noscroll) {
			    if (clreol) {
				home ();
				cleareol ();
			    }
			    else
				doclear ();
			}
		    }
		    else {
			kill_line ();
			if (noscroll) {
			    if (clreol) {
			        home ();
			        cleareol ();
			    }
			    else
				doclear ();
			}
			puts(Line);
		    }
		    break;
		}
	} else if (rv == -1)
	    error (_("Regular expression botch"));
    }
    if (feof (file)) {
	if (!no_intty) {
	    Currline = saveln;
	    Fseek (file, startline);
	}
	else {
	    putsout(_("\nPattern not found\n"));
	    end_it (0);
	}
	error (_("Pattern not found"));
    }
}

/*VARARGS2*/
void execute (char *filename, char *cmd, ...)
{
	int id;
	int n;
	va_list argp;
	char * arg;
 	char ** args;
	int argcount;

	fflush (stdout);
	reset_tty ();
	for (n = 10; (id = fork ()) < 0 && n > 0; n--)
	    sleep (5);
	if (id == 0) {
	    if (!isatty(0)) {
		close(0);
		open("/dev/tty", 0);
	    }

	    va_start(argp, cmd);
	    arg = va_arg(argp, char *);
	    argcount = 0;
	    while (arg) {
		argcount++;
	        arg = va_arg(argp, char *);
	    }
	    va_end(argp);

	    args = alloca(sizeof(char *) * (argcount + 1));
	    args[argcount] = NULL;
	    
	    va_start(argp, cmd);
	    arg = va_arg(argp, char *);
	    argcount = 0;
	    while (arg) {
		args[argcount] = arg;
		argcount++;
	        arg = va_arg(argp, char *);
	    }
	    va_end(argp);
	
	    execvp (cmd, args);
	    putserr(_("exec failed\n"));
	    exit (EXIT_FAILURE);
	}
	if (id > 0) {
	    signal (SIGINT, SIG_IGN);
	    signal (SIGQUIT, SIG_IGN);
	    if (catch_susp)
		signal(SIGTSTP, SIG_DFL);
	    while (wait(0) > 0);
	    signal (SIGINT, end_it);
	    signal (SIGQUIT, onquit);
	    if (catch_susp)
		signal(SIGTSTP, onsusp);
	} else
	    putserr(_("can't fork\n"));
	set_tty ();
	puts("------------------------");
	prompt (filename);
}
/*
** Skip n lines in the file f
*/

void skiplns (register int n, register FILE *f)
{
    register int c;

    while (n > 0) {
	while ((c = Getc (f)) != '\n')
	    if (c == EOF)
		return;
	    n--;
	    Currline++;
    }
}

/*
** Skip nskip files in the file list (from the command line). Nskip may be
** negative.
*/

void skipf (register int nskip)
{
    if (nskip == 0) return;
    if (nskip > 0) {
	if (fnum + nskip > nfiles - 1)
	    nskip = nfiles - fnum - 1;
    }
    else if (within)
	++fnum;
    fnum += nskip;
    if (fnum < 0)
	fnum = 0;
    puts(_("\n...Skipping "));
    if (clreol)
	cleareol ();
    if (nskip > 0)
	    putsout(_("...Skipping to file "));
    else
	    putsout(_("...Skipping back to file "));
    puts(fnames[fnum]);
    if (clreol)
	cleareol ();
    putchar('\n');
    --fnum;
}

/*----------------------------- Terminal I/O -------------------------------*/

void initterm()
{
    int		ret;
    char	*padstr;
    char	*term;
    struct winsize win;

#ifdef do_SIGTTOU
retry:
#endif /* do_SIGTTOU */
    no_tty = tcgetattr(fileno(stdout), &otty);
    if (!no_tty) {	
	docrterase = (otty.c_cc[VERASE] != 255);
	docrtkill =  (otty.c_cc[VKILL] != 255);
#ifdef do_SIGTTOU
	{
	    int tgrp;
	    /*
	     * Wait until we're in the foreground before we save the
	     * the terminal modes.
	     */
	    if ((tgrp = tcgetpgrp(fileno(stdout))) < 0) {
		perror("tcgetpgrp");
		exit(EXIT_FAILURE);
	    }
	    if (tgrp != getpgrp(0)) {
		kill(0, SIGTTOU);
		goto retry;
	    }
	}
#endif /* do_SIGTTOU */
	if ((term = getenv("TERM")) == 0) {
	    dumb++; ul_opt = 0;
	}
        my_setupterm(term, 1, &ret);
	if (ret <= 0) {
	    dumb++; ul_opt = 0;
	}
	else {
#ifdef TIOCGWINSZ
	    if (ioctl(fileno(stdout), TIOCGWINSZ, &win) < 0) {
#endif /* TIOCGWINSZ */
		Lpp = my_tgetnum(TERM_LINES);
		Mcol = my_tgetnum(TERM_COLS);
#ifdef TIOCGWINSZ
	    } else {
		if ((Lpp = win.ws_row) == 0)
		    Lpp = my_tgetnum(TERM_LINES);
		if ((Mcol = win.ws_col) == 0)
		    Mcol = my_tgetnum(TERM_COLS);
	    }
#endif /* TIOCGWINSZ */
	    if ((Lpp <= 0) || my_tgetflag(TERM_HARD_COPY)) {
		hard++;	/* Hard copy terminal */
		Lpp = LINES_PER_PAGE;
	    }

	    if (my_tgetflag(TERM_EAT_NEW_LINE))
		eatnl++; /* Eat newline at last column + 1; dec, concept */
	    if (Mcol <= 0)
		Mcol = NUM_COLUMNS;

	    Wrap = my_tgetflag(TERM_AUTO_RIGHT_MARGIN);
	    bad_so = my_tgetflag (TERM_CEOL);
	    eraseln = my_tgetstr(TERM_CLEAR_TO_LINE_END);
	    Clear = my_tgetstr(TERM_CLEAR);
	    Senter = my_tgetstr(TERM_STANDARD_MODE);
	    Sexit = my_tgetstr(TERM_EXIT_STANDARD_MODE);
	    if ((soglitch = my_tgetnum(TERM_STD_MODE_GLITCH)) < 0)
		soglitch = 0;

	    /*
	     *  Set up for underlining:  some terminals don't need it;
	     *  others have start/stop sequences, still others have an
	     *  underline char sequence which is assumed to move the
	     *  cursor forward one character.  If underline sequence
	     *  isn't available, settle for standout sequence.
	     */

	    if (my_tgetflag(TERM_UNDERLINE) || my_tgetflag(TERM_OVER_STRIKE))
		ul_opt = 0;
	    if ((chUL = my_tgetstr(TERM_UNDERLINE_CHAR)) == NULL )
		chUL = "";
	    if (((ULenter = my_tgetstr(TERM_ENTER_UNDERLINE)) == NULL ||
	         (ULexit = my_tgetstr(TERM_EXIT_UNDERLINE)) == NULL) && !*chUL) {
	        if ((ULenter = Senter) == NULL || (ULexit = Sexit) == NULL) {
			ULenter = "";
			ULexit = "";
		} else
			ulglitch = soglitch;
	    } else {
		ulglitch = 0;
	    }

	    if ((padstr = my_tgetstr(TERM_PAD_CHAR)) != NULL)
		PC = *padstr;
	    Home = my_tgetstr(TERM_HOME);
	    if (Home == 0 || *Home == '\0') {
		if ((cursorm = my_tgetstr(TERM_CURSOR_ADDRESS)) != NULL) {
		    const char *t = (const char *)my_tgoto(cursorm, 0, 0);
		    xstrncpy(cursorhome, t, sizeof(cursorhome));
		    Home = cursorhome;
		}
	    }
	    EodClr = my_tgetstr(TERM_CLEAR_TO_SCREEN_END);
	    if ((chBS = my_tgetstr(TERM_LINE_DOWN)) == NULL)
		chBS = "\b";

	}
	if ((shell = getenv("SHELL")) == NULL)
	    shell = "/bin/sh";
    }
    no_intty = tcgetattr(fileno(stdin), &otty);
    tcgetattr(fileno(stderr), &otty);
    savetty0 = otty;
    slow_tty = cfgetispeed(&otty) < B1200;
    hardtabs = (otty.c_oflag & TABDLY) != XTABS;
    if (!no_tty) {
	otty.c_lflag &= ~(ICANON|ECHO);
	otty.c_cc[VMIN] = 1;
	otty.c_cc[VTIME] = 0;
    }
}

int readch () {
	unsigned char c;

	errno = 0;
	if (read (fileno(stderr), &c, 1) <= 0) {
		if (errno != EINTR)
			end_it(0);
		else
			c = otty.c_cc[VKILL];
	}
	return (c);
}

static char *BS = "\b";
static char *BSB = "\b \b";
static char *CARAT = "^";
#define ERASEONECOLUMN \
    if (docrterase) \
	putserr(BSB); \
    else \
	putserr(BS);

void ttyin (char buf[], register int nmax, char pchar) {
    char *sp;
    int c;
    int slash = 0;
    int	maxlen;

    sp = buf;
    maxlen = 0;
    while (sp - buf < nmax) {
	if (promptlen > maxlen) maxlen = promptlen;
	c = readch ();
	if (c == '\\') {
	    slash++;
	}
	else if (((cc_t) c == otty.c_cc[VERASE]) && !slash) {
	    if (sp > buf) {
#ifdef HAVE_WIDECHAR
		if (MB_CUR_MAX > 1)
		  {
		    wchar_t wc;
		    size_t pos = 0, mblength;
		    mbstate_t state, state_bak;

		    memset (&state, '\0', sizeof (mbstate_t));

		    while (1) {
			 state_bak = state;
			 mblength = mbrtowc (&wc, buf + pos, sp - buf, &state);

			 state = (mblength == (size_t)-2
				 || mblength == (size_t)-1) ? state_bak : state;
			 mblength = (mblength == (size_t)-2
				     || mblength == (size_t)-1
				     || mblength == 0) ? 1 : mblength;

			 if (buf + pos + mblength >= sp)
			 break;

			 pos += mblength;
		    }

		    if (mblength == 1) {
		      ERASEONECOLUMN
		    }
		    else {
			int wc_width;
			wc_width = wcwidth (wc);
			wc_width = (wc_width < 1) ? 1 : wc_width;
			while (wc_width--) {
			    ERASEONECOLUMN
			}
		    }

		    while (mblength--) {
			--promptlen;
			--sp;
		    }
		  }
		else
#endif /* HAVE_WIDECHAR */
		  {
		    --promptlen;
		    ERASEONECOLUMN
		    --sp;
		  }

		if ((*sp < ' ' && *sp != '\n') || *sp == RUBOUT) {
		    --promptlen;
		    ERASEONECOLUMN
		}
		continue;
	    }
	    else {
		if (!eraseln) promptlen = maxlen;
		siglongjmp (restore, 1);
	    }
	}
	else if (((cc_t) c == otty.c_cc[VKILL]) && !slash) {
	    if (hard) {
		show (c);
		putchar ('\n');
		putchar (pchar);
	    }
	    else {
		putchar ('\r');
		putchar (pchar);
		if (eraseln)
		    erasep (1);
		else if (docrtkill)
		    while (promptlen-- > 1)
			putserr(BSB);
		promptlen = 1;
	    }
	    sp = buf;
	    fflush (stdout);
	    continue;
	}
	if (slash && ((cc_t) c == otty.c_cc[VKILL]
		   || (cc_t) c == otty.c_cc[VERASE])) {
	    ERASEONECOLUMN
	    --sp;
	}
	if (c != '\\')
	    slash = 0;
	*sp++ = c;
	if ((c < ' ' && c != '\n' && c != ESC) || c == RUBOUT) {
	    c += (c == RUBOUT) ? -0100 : 0100;
	    putserr(CARAT);
	    promptlen++;
	}
	if (c != '\n' && c != ESC) {
	    putcerr(c);
	    promptlen++;
	}
	else
	    break;
    }
    *--sp = '\0';
    if (!eraseln) promptlen = maxlen;
    if (sp - buf >= nmax - 1)
	error (_("Line too long"));
}

/* return: 0 - unchanged, 1 - changed, -1 - overflow (unchanged) */
int expand (char **outbuf, char *inbuf) {
    char *inpstr;
    char *outstr;
    char c;
    char *temp;
    int changed = 0;
    int tempsz, xtra, offset;

    xtra = strlen (fnames[fnum]) + strlen (shell_line) + 1;
    tempsz = 200 + xtra;
    temp = xmalloc(tempsz);
    inpstr = inbuf;
    outstr = temp;
    while ((c = *inpstr++) != '\0'){
	offset = outstr-temp;
	if (tempsz-offset-1 < xtra) {
		tempsz += 200 + xtra;
		temp = xrealloc(temp, tempsz);
		outstr = temp + offset;
	}
	switch (c) {
	case '%':
	    if (!no_intty) {
		strcpy (outstr, fnames[fnum]);
		outstr += strlen (fnames[fnum]);
		changed++;
	    } else
		*outstr++ = c;
	    break;
	case '!':
	    if (!shellp)
		error (_("No previous command to substitute for"));
	    strcpy (outstr, shell_line);
	    outstr += strlen (shell_line);
	    changed++;
	    break;
	case '\\':
	    if (*inpstr == '%' || *inpstr == '!') {
		*outstr++ = *inpstr++;
		break;
	    }
	default:
	    *outstr++ = c;
	}
    }
    *outstr++ = '\0';
    *outbuf = temp;
    return (changed);
}

void show (char c) {
    if ((c < ' ' && c != '\n' && c != ESC) || c == RUBOUT) {
	c += (c == RUBOUT) ? -0100 : 0100;
	putserr(CARAT);
	promptlen++;
    }
    putcerr(c);
    promptlen++;
}

void error (char *mess)
{
    if (clreol)
	cleareol ();
    else
	kill_line ();
    promptlen += strlen (mess);
    if (Senter && Sexit) {
	my_putstring (Senter);
	putsout(mess);
	my_putstring (Sexit);
    }
    else
	putsout(mess);
    fflush(stdout);
    errors++;
    siglongjmp (restore, 1);
}


void set_tty () {
	otty.c_lflag &= ~(ICANON|ECHO);
	otty.c_cc[VMIN] = 1;	/* read at least 1 char */
	otty.c_cc[VTIME] = 0;	/* no timeout */
	stty(fileno(stderr), &otty);
}

static int
ourputch(int c) {
	return putc(c, stdout);
}

void
reset_tty () {
    if (no_tty)
	return;
    if (pstate) {
	tputs(ULexit, fileno(stdout), ourputch);	/* putchar - if that isnt a macro */
	fflush(stdout);
	pstate = 0;
    }
    otty.c_lflag |= ICANON|ECHO;
    otty.c_cc[VMIN] = savetty0.c_cc[VMIN];
    otty.c_cc[VTIME] = savetty0.c_cc[VTIME];
    stty(fileno(stderr), &savetty0);
}

void rdline (register FILE *f)
{
    register int  c;
    register char *p;

    prepare_line_buffer();

    p = Line;
    while ((c = Getc (f)) != '\n' && c != EOF && (size_t) (p - Line) < LineLen - 1)
	*p++ = c;
    if (c == '\n')
	Currline++;
    *p = '\0';
}

/* Come here when we get a suspend signal from the terminal */

void onsusp (int dummy __attribute__ ((__unused__)))
{
    sigset_t signals, oldmask;

    /* ignore SIGTTOU so we don't get stopped if csh grabs the tty */
    signal(SIGTTOU, SIG_IGN);
    reset_tty ();
    fflush (stdout);
    signal(SIGTTOU, SIG_DFL);
    /* Send the TSTP signal to suspend our process group */
    signal(SIGTSTP, SIG_DFL);

    /* unblock SIGTSTP or we won't be able to suspend ourself */
    sigemptyset(&signals);
    sigaddset(&signals, SIGTSTP);
    sigprocmask(SIG_UNBLOCK, &signals, &oldmask);

    kill (0, SIGTSTP);
    /* Pause for station break */

    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    /* We're back */
    signal (SIGTSTP, onsusp);
    set_tty ();
    if (inwait)
	    siglongjmp (restore, 1);
}

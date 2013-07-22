/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * modified by Kars de Jong <jongk@cs.utwente.nl>
 *	to use terminfo instead of termcap.
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * 	added Native Language Support
 * 1999-09-19 Bruno Haible <haible@clisp.cons.org>
 * 	modified to work correctly in multi-byte locales
 */

#include <stdio.h>
#include <unistd.h>		/* for getopt(), isatty() */
#include <string.h>		/* for memset(), strcpy() */
#include <term.h>		/* for setupterm() */
#include <stdlib.h>		/* for getenv() */
#include <limits.h>		/* for INT_MAX */
#include <signal.h>		/* for signal() */
#include <errno.h>
#include <getopt.h>

#include "nls.h"
#include "xalloc.h"
#include "widechar.h"
#include "c.h"

#ifdef HAVE_WIDECHAR
/* Output an ASCII character as a wide character */
static int put1wc(int c)
{
	if (putwchar(c) == WEOF)
		return EOF;
	else
		return c;
}
#define putwp(s) tputs(s, STDOUT_FILENO, put1wc)
#else
#define putwp(s) putp(s)
#endif

static void usage(FILE *out);
static int handle_escape(FILE * f);
static void filter(FILE *f);
static void flushln(void);
static void overstrike(void);
static void iattr(void);
static void initbuf(void);
static void fwd(void);
static void reverse(void);
static void initinfo(void);
static void outc(wint_t c, int width);
static void setmode(int newmode);
static void setcol(int newcol);
static void needcol(int col);
static void sig_handler(int signo);
static void print_out(char *line);

#define	IESC	'\033'
#define	SO	'\016'
#define	SI	'\017'
#define	HFWD	'9'
#define	HREV	'8'
#define	FREV	'7'

#define	NORMAL	000
#define	ALTSET	001	/* Reverse */
#define	SUPERSC	002	/* Dim */
#define	SUBSC	004	/* Dim | Ul */
#define	UNDERL	010	/* Ul */
#define	BOLD	020	/* Bold */

int	must_use_uc, must_overstrike;
char	*CURS_UP,
	*CURS_RIGHT,
	*CURS_LEFT,
	*ENTER_STANDOUT,
	*EXIT_STANDOUT,
	*ENTER_UNDERLINE,
	*EXIT_UNDERLINE,
	*ENTER_DIM,
	*ENTER_BOLD,
	*ENTER_REVERSE,
	*UNDER_CHAR,
	*EXIT_ATTRIBUTES;

struct	CHAR	{
	char	c_mode;
	wchar_t	c_char;
	int	c_width;
};

struct	CHAR	*obuf;
int	obuflen;
int	col, maxcol;
int	mode;
int	halfpos;
int	upln;
int	iflag;

static void __attribute__((__noreturn__))
usage(FILE *out)
{
	fprintf(out, _(
		"\nUsage:\n"
		" %s [options] [file...]\n"), program_invocation_short_name);

	fprintf(out, _(
		"\nOptions:\n"
		" -t, --terminal TERMINAL    override the TERM environment variable\n"
		" -i, --indicated            underlining is indicated via a separate line\n"
		" -V, --version              output version information and exit\n"
		" -h, --help                 display this help and exit\n\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int c, ret, tflag = 0;
	char *termtype;
	FILE *f;

	static const struct option longopts[] = {
		{ "terminal",	required_argument,	0, 't' },
		{ "indicated",	no_argument,		0, 'i' },
		{ "version",	no_argument,		0, 'V' },
		{ "help",	no_argument,		0, 'h' },
		{ NULL, 0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	termtype = getenv("TERM");

	/*
	 * FIXME: why terminal type is lpr when command begins with c and has
	 * no terminal? If this behavior can be explained please insert
	 * refrence or remove the code. In case this truly is desired command
	 * behavior this should be mentioned in manual page.
	 */
	if (termtype == NULL || (argv[0][0] == 'c' && !isatty(STDOUT_FILENO)))
		termtype = "lpr";

	while ((c = getopt_long(argc, argv, "it:T:Vh", longopts, NULL)) != -1)
		switch (c) {

		case 't':
		case 'T':
			/* for nroff compatibility */
			termtype = optarg;
			tflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
						  PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}
	setupterm(termtype, STDOUT_FILENO, &ret);
	switch (ret) {

	case 1:
		break;

	default:
		warnx(_("trouble reading terminfo"));
		/* fall through to ... */

	case 0:
		if (tflag)
			warnx(_("terminal `%s' is not known, defaulting to `dumb'"),
				termtype);
		setupterm("dumb", STDOUT_FILENO, (int *)0);
		break;
	}
	initinfo();
	if ((tigetflag("os") && ENTER_BOLD==NULL ) ||
	    (tigetflag("ul") && ENTER_UNDERLINE==NULL && UNDER_CHAR==NULL))
		must_overstrike = 1;
	initbuf();
	if (optind == argc)
		filter(stdin);
	else
		for (; optind < argc; optind++) {
			f = fopen(argv[optind],"r");
			if (!f)
				err(EXIT_FAILURE, _("%s: open failed"),
				    argv[optind]);
			filter(f);
			fclose(f);
		}
	if (ferror(stdout) || fclose(stdout))
		return EXIT_FAILURE;

	free(obuf);
	return EXIT_SUCCESS;
}

static int handle_escape(FILE * f)
{
	wint_t c;

	switch (c = getwc(f)) {
	case HREV:
		if (halfpos == 0) {
			mode |= SUPERSC;
			halfpos--;
		} else if (halfpos > 0) {
			mode &= ~SUBSC;
			halfpos--;
		} else {
			halfpos = 0;
			reverse();
		}
		return 0;
	case HFWD:
		if (halfpos == 0) {
			mode |= SUBSC;
			halfpos++;
		} else if (halfpos < 0) {
			mode &= ~SUPERSC;
			halfpos++;
		} else {
			halfpos = 0;
			fwd();
		}
		return 0;
	case FREV:
		reverse();
		return 0;
	default:
		/* unknown escape */
		ungetwc(c, f);
		return 1;
	}
}

static void filter(FILE *f)
{
	wint_t c;
	int i, w;

	while ((c = getwc(f)) != WEOF)
	switch (c) {

	case '\b':
		setcol(col - 1);
		continue;

	case '\t':
		setcol((col+8) & ~07);
		continue;

	case '\r':
		setcol(0);
		continue;

	case SO:
		mode |= ALTSET;
		continue;

	case SI:
		mode &= ~ALTSET;
		continue;

	case IESC:
		if(handle_escape(f)) {
			c = getwc(f);
			errx(EXIT_FAILURE,
				_("unknown escape sequence in input: %o, %o"),
				IESC, c);
		}
		continue;

	case '_':
		if (obuf[col].c_char || obuf[col].c_width < 0) {
			while(col > 0 && obuf[col].c_width < 0)
				col--;
			w = obuf[col].c_width;
			for (i = 0; i < w; i++)
				obuf[col++].c_mode |= UNDERL | mode;
			setcol(col);
			continue;
		}
		obuf[col].c_char = '_';
		obuf[col].c_width = 1;
		/* fall through */
	case ' ':
		setcol(col + 1);
		continue;

	case '\n':
		flushln();
		continue;

	case '\f':
		flushln();
		putwchar('\f');
		continue;

	default:
		if (!iswprint(c))
			/* non printable */
			continue;
		w = wcwidth(c);
		needcol(col + w);
		if (obuf[col].c_char == '\0') {
			obuf[col].c_char = c;
			for (i = 0; i < w; i++)
				obuf[col+i].c_mode = mode;
			obuf[col].c_width = w;
			for (i = 1; i < w; i++)
				obuf[col+i].c_width = -1;
		} else if (obuf[col].c_char == '_') {
			obuf[col].c_char = c;
			for (i = 0; i < w; i++)
				obuf[col+i].c_mode |= UNDERL|mode;
			obuf[col].c_width = w;
			for (i = 1; i < w; i++)
				obuf[col+i].c_width = -1;
		} else if ((wint_t) obuf[col].c_char == c) {
			for (i = 0; i < w; i++)
				obuf[col+i].c_mode |= BOLD|mode;
		} else {
			w = obuf[col].c_width;
			for (i = 0; i < w; i++)
				obuf[col+i].c_mode = mode;
		}
		setcol(col + w);
		continue;
	}
	if (maxcol)
		flushln();
}

static void flushln(void)
{
	int lastmode;
	int i;
	int hadmodes = 0;

	lastmode = NORMAL;
	for (i = 0; i < maxcol; i++) {
		if (obuf[i].c_mode != lastmode) {
			hadmodes++;
			setmode(obuf[i].c_mode);
			lastmode = obuf[i].c_mode;
		}
		if (obuf[i].c_char == '\0') {
			if (upln) {
				print_out(CURS_RIGHT);
			} else
				outc(' ', 1);
		} else
			outc(obuf[i].c_char, obuf[i].c_width);
		if (obuf[i].c_width > 1)
			i += obuf[i].c_width - 1;
	}
	if (lastmode != NORMAL) {
		setmode(0);
	}
	if (must_overstrike && hadmodes)
		overstrike();
	putwchar('\n');
	if (iflag && hadmodes)
		iattr();
	fflush(stdout);
	if (upln)
		upln--;
	initbuf();
}

/*
 * For terminals that can overstrike, overstrike underlines and bolds.
 * We don't do anything with halfline ups and downs, or Greek.
 */
static void overstrike(void)
{
	register int i;
#ifdef __GNUC__
	register wchar_t *lbuf = __builtin_alloca((maxcol + 1) * sizeof(wchar_t));
#else
	wchar_t lbuf[BUFSIZ];
#endif
	register wchar_t *cp = lbuf;
	int hadbold=0;

	/* Set up overstrike buffer */
	for (i = 0; i < maxcol; i++)
		switch (obuf[i].c_mode) {
		case NORMAL:
		default:
			*cp++ = ' ';
			break;
		case UNDERL:
			*cp++ = '_';
			break;
		case BOLD:
			*cp++ = obuf[i].c_char;
			if (obuf[i].c_width > 1)
				i += obuf[i].c_width - 1;
			hadbold=1;
			break;
		}
	putwchar('\r');
	for (*cp = ' '; *cp == ' '; cp--)
		*cp = 0;
	for (cp = lbuf; *cp; cp++)
		putwchar(*cp);
	if (hadbold) {
		putwchar('\r');
		for (cp = lbuf; *cp; cp++)
			putwchar(*cp == '_' ? ' ' : *cp);
		putwchar('\r');
		for (cp = lbuf; *cp; cp++)
			putwchar(*cp == '_' ? ' ' : *cp);
	}
}

static void iattr(void)
{
	register int i;
#ifdef __GNUC__
	register char *lbuf = __builtin_alloca((maxcol+1)*sizeof(char));
#else
	char lbuf[BUFSIZ];
#endif
	register char *cp = lbuf;

	for (i = 0; i < maxcol; i++)
		switch (obuf[i].c_mode) {
		case NORMAL:	*cp++ = ' '; break;
		case ALTSET:	*cp++ = 'g'; break;
		case SUPERSC:	*cp++ = '^'; break;
		case SUBSC:	*cp++ = 'v'; break;
		case UNDERL:	*cp++ = '_'; break;
		case BOLD:	*cp++ = '!'; break;
		default:	*cp++ = 'X'; break;
		}
	for (*cp = ' '; *cp == ' '; cp--)
		*cp = 0;
	for (cp = lbuf; *cp; cp++)
		putwchar(*cp);
	putwchar('\n');
}

static void initbuf(void)
{
	if (obuf == NULL) {
		/* First time. */
		obuflen = BUFSIZ;
		obuf = xmalloc(sizeof(struct CHAR) * obuflen);
	}

	/* assumes NORMAL == 0 */
	memset(obuf, 0, sizeof(struct CHAR) * obuflen);
	setcol(0);
	maxcol = 0;
	mode &= ALTSET;
}

static void fwd(void)
{
	int oldcol, oldmax;

	oldcol = col;
	oldmax = maxcol;
	flushln();
	setcol(oldcol);
	maxcol = oldmax;
}

static void reverse(void)
{
	upln++;
	fwd();
	print_out(CURS_UP);
	print_out(CURS_UP);
	upln++;
}

static void initinfo(void)
{
	CURS_UP =		tigetstr("cuu1");
	CURS_RIGHT =		tigetstr("cuf1");
	CURS_LEFT =		tigetstr("cub1");
	if (CURS_LEFT == NULL)
		CURS_LEFT =	"\b";

	ENTER_STANDOUT =	tigetstr("smso");
	EXIT_STANDOUT =		tigetstr("rmso");
	ENTER_UNDERLINE =	tigetstr("smul");
	EXIT_UNDERLINE =	tigetstr("rmul");
	ENTER_DIM =		tigetstr("dim");
	ENTER_BOLD =		tigetstr("bold");
	ENTER_REVERSE =		tigetstr("rev");
	EXIT_ATTRIBUTES =	tigetstr("sgr0");

	if (!ENTER_BOLD && ENTER_REVERSE)
		ENTER_BOLD = ENTER_REVERSE;
	if (!ENTER_BOLD && ENTER_STANDOUT)
		ENTER_BOLD = ENTER_STANDOUT;
	if (!ENTER_UNDERLINE && ENTER_STANDOUT) {
		ENTER_UNDERLINE = ENTER_STANDOUT;
		EXIT_UNDERLINE = EXIT_STANDOUT;
	}
	if (!ENTER_DIM && ENTER_STANDOUT)
		ENTER_DIM = ENTER_STANDOUT;
	if (!ENTER_REVERSE && ENTER_STANDOUT)
		ENTER_REVERSE = ENTER_STANDOUT;
	if (!EXIT_ATTRIBUTES && EXIT_STANDOUT)
		EXIT_ATTRIBUTES = EXIT_STANDOUT;

	/*
	 * Note that we use REVERSE for the alternate character set,
	 * not the as/ae capabilities.  This is because we are modelling
	 * the model 37 teletype (since that's what nroff outputs) and
	 * the typical as/ae is more of a graphics set, not the greek
	 * letters the 37 has.
	 */

	UNDER_CHAR =		tigetstr("uc");
	must_use_uc = (UNDER_CHAR && !ENTER_UNDERLINE);
}

static int curmode = 0;

static void outc(wint_t c, int width) {
	int i;

	putwchar(c);
	if (must_use_uc && (curmode&UNDERL)) {
		for (i = 0; i < width; i++)
			print_out(CURS_LEFT);
		for (i = 0; i < width; i++)
			print_out(UNDER_CHAR);
	}
}

static void setmode(int newmode)
{
	if (!iflag) {
		if (curmode != NORMAL && newmode != NORMAL)
			setmode(NORMAL);
		switch (newmode) {
		case NORMAL:
			switch (curmode) {
			case NORMAL:
				break;
			case UNDERL:
				print_out(EXIT_UNDERLINE);
				break;
			default:
				/* This includes standout */
				print_out(EXIT_ATTRIBUTES);
				break;
			}
			break;
		case ALTSET:
			print_out(ENTER_REVERSE);
			break;
		case SUPERSC:
			/*
			 * This only works on a few terminals.
			 * It should be fixed.
			 */
			print_out(ENTER_UNDERLINE);
			print_out(ENTER_DIM);
			break;
		case SUBSC:
			print_out(ENTER_DIM);
			break;
		case UNDERL:
			print_out(ENTER_UNDERLINE);
			break;
		case BOLD:
			print_out(ENTER_BOLD);
			break;
		default:
			/*
			 * We should have some provision here for multiple modes
			 * on at once.  This will have to come later.
			 */
			print_out(ENTER_STANDOUT);
			break;
		}
	}
	curmode = newmode;
}

static void setcol(int newcol) {
	col = newcol;

	if (col < 0)
		col = 0;
	else if (col > maxcol)
		needcol(col);
}

static void needcol(int col) {
	maxcol = col;

	/* If col >= obuflen, expand obuf until obuflen > col. */
	while (col >= obuflen) {
		/* Paranoid check for obuflen == INT_MAX. */
		if (obuflen == INT_MAX)
			errx(EXIT_FAILURE, _("Input line too long."));

		/* Similar paranoia: double only up to INT_MAX. */
		if (obuflen < (INT_MAX / 2))
			obuflen *= 2;
		else
			obuflen = INT_MAX;

		/* Now we can try to expand obuf. */
		obuf = xrealloc(obuf, sizeof(struct CHAR) * obuflen);
	}
}

static void sig_handler(int signo __attribute__ ((__unused__)))
{
	_exit(EXIT_SUCCESS);
}

static void print_out(char *line)
{
	if (line == NULL)
		return;

	putwp(line);
}

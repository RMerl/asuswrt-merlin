/****************************************************************************
 * Copyright (c) 2014 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/*
 * Author: Thomas E. Dickey
 *
 * $Id: dots_curses.c,v 1.3 2014/08/09 22:28:42 tom Exp $
 *
 * A simple demo of the curses interface used for comparison with termcap.
 */
#include <test.priv.h>

#if !defined(__MINGW32__)
#include <sys/time.h>
#endif

#include <time.h>

#define valid(s) ((s != 0) && s != (char *)-1)

static bool interrupted = FALSE;
static long total_chars = 0;
static time_t started;

static void
cleanup(void)
{
    endwin();

    printf("\n\n%ld total chars, rate %.2f/sec\n",
	   total_chars,
	   ((double) (total_chars) / (double) (time((time_t *) 0) - started)));
}

static void
onsig(int n GCC_UNUSED)
{
    interrupted = TRUE;
}

static double
ranf(void)
{
    long r = (rand() & 077777);
    return ((double) r / 32768.);
}

static int
mypair(int fg, int bg)
{
    int pair = (fg * COLORS) + bg;
    return (pair >= COLOR_PAIRS) ? -1 : pair;
}

static void
set_colors(int fg, int bg)
{
    int pair = mypair(fg, bg);
    if (pair > 0) {
	attron(COLOR_PAIR(mypair(fg, bg)));
    }
}

int
main(int argc GCC_UNUSED,
     char *argv[]GCC_UNUSED)
{
    int x, y, z, p;
    int fg, bg;
    double r;
    double c;

    CATCHALL(onsig);

    srand((unsigned) time(0));

    initscr();
    if (has_colors()) {
	start_color();
	for (fg = 0; fg < COLORS; fg++) {
	    for (bg = 0; bg < COLORS; bg++) {
		int pair = mypair(fg, bg);
		if (pair > 0)
		    init_pair((short) pair, (short) fg, (short) bg);
	    }
	}
    }

    r = (double) (LINES - 4);
    c = (double) (COLS - 4);
    started = time((time_t *) 0);

    fg = COLOR_WHITE;
    bg = COLOR_BLACK;
    while (!interrupted) {
	x = (int) (c * ranf()) + 2;
	y = (int) (r * ranf()) + 2;
	p = (ranf() > 0.9) ? '*' : ' ';

	move(y, x);
	if (has_colors()) {
	    z = (int) (ranf() * COLORS);
	    if (ranf() > 0.01) {
		set_colors(fg = z, bg);
		attron(COLOR_PAIR(mypair(fg, bg)));
	    } else {
		set_colors(fg, bg = z);
		napms(1);
	    }
	} else {
	    if (ranf() <= 0.01) {
		if (ranf() > 0.6) {
		    attron(A_REVERSE);
		} else {
		    attroff(A_REVERSE);
		}
		napms(1);
	    }
	}
	addch((chtype) p);
	refresh();
	++total_chars;
    }
    cleanup();
    ExitProgram(EXIT_SUCCESS);
}

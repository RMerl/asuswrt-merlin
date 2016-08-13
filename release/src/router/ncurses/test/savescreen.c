/****************************************************************************
 * Copyright (c) 2007-2011,2015 Free Software Foundation, Inc.              *
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
 * $Id: savescreen.c,v 1.27 2015/03/28 23:21:28 tom Exp $
 *
 * Demonstrate save/restore functions from the curses library.
 * Thomas Dickey - 2007/7/14
 */

#include <test.priv.h>

#if HAVE_SCR_DUMP

#include <sys/types.h>
#include <sys/stat.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

static bool use_init = FALSE;
static bool keep_dumps = FALSE;

static int
fexists(const char *name)
{
    struct stat sb;
    return (stat(name, &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFREG);
}

static void
setup_next(void)
{
    curs_set(1);
    reset_shell_mode();
}

static void
cleanup(char *files[])
{
    int n;

    if (!keep_dumps) {
	for (n = 0; files[n] != 0; ++n) {
	    unlink(files[n]);
	}
    }
}

static int
load_screen(char *filename)
{
    int result;

    if (use_init) {
	if ((result = scr_init(filename)) != ERR)
	    result = scr_restore(filename);
    } else {
	result = scr_set(filename);
    }
    return result;
}

/*
 * scr_restore() or scr_set() operates on curscr.  If we read a character using
 * getch() that will refresh stdscr, wiping out the result.  To avoid that,
 * copy the data back from curscr to stdscr.
 */
static void
after_load(void)
{
    overwrite(curscr, stdscr);
    doupdate();
}

static void
show_what(int which, int last)
{
    int y, x, n;
    time_t now;
    char *mytime;

    getyx(stdscr, y, x);

    move(0, 0);
    printw("Saved %d of %d (? for help)", which, last + 1);

    now = time((time_t *) 0);
    mytime = ctime(&now);
    for (n = (int) strlen(mytime) - 1; n >= 0; --n) {
	if (isspace(UChar(mytime[n]))) {
	    mytime[n] = '\0';
	} else {
	    break;
	}
    }
    mvprintw(0, (COLS - n - 2), " %s", mytime);

    move(y, x);

    refresh();
}

static int
get_command(int which, int last)
{
    int ch;

    timeout(50);

    do {
	show_what(which, last);
	ch = getch();
    } while (ch == ERR);

    return ch;
}

static void
show_help(const char **help)
{
    WINDOW *mywin = newwin(LINES, COLS, 0, 0);
    int n;

    box(mywin, 0, 0);
    wmove(mywin, 1, 1);
    for (n = 0; help[n] != 0; ++n) {
	wmove(mywin, 1 + n, 2);
	wprintw(mywin, "%.*s", COLS - 4, help[n]);
    }
    wgetch(mywin);
    delwin(mywin);
    touchwin(stdscr);
    refresh();
}

static void
editor_help(void)
{
    static const char *msgs[] =
    {
	"You are now in the screen-editor, which allows you to make some",
	"lines on the screen, as well as save copies of the screen to a",
	"temporary file",
	"",
	"Keys:",
	"   q           quit",
	"   n           run the screen-loader to show the saved screens",
	"   <space>     dump a screen",
	"",
	"   a           toggle between '#' and graphic symbol for drawing",
	"   c           change color drawn by line to next in palette",
	"   h,j,k,l or arrows to move around the screen, drawing",
    };
    show_help(msgs);
}

static void
replay_help(void)
{
    static const char *msgs[] =
    {
	"You are now in the screen-loader, which allows you to view",
	"the dumped/restored screens.",
	"",
	"Keys:",
	"   q           quit",
	"   <space>     load the next screen",
	"   <backspace> load the previous screen",
    };
    show_help(msgs);
}

static void
usage(void)
{
    static const char *msg[] =
    {
	"Usage: savescreen [-r] files",
	"",
	"Options:",
	" -f file  fill/initialize screen using text from this file",
	" -i       use scr_init/scr_restore rather than scr_set",
	" -k       keep the restored dump-files rather than removing them",
	" -r       replay the screen-dump files"
    };
    unsigned n;
    for (n = 0; n < SIZEOF(msg); ++n) {
	fprintf(stderr, "%s\n", msg[n]);
    }
    ExitProgram(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    int ch;
    int which = 0;
    int last;
    bool replaying = FALSE;
    bool done = FALSE;
    char **files;
    char *fill_by = 0;
#if USE_WIDEC_SUPPORT
    cchar_t mycc;
    int myxx;
#endif

    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "f:ikr")) != -1) {
	switch (ch) {
	case 'f':
	    fill_by = optarg;
	    break;
	case 'i':
	    use_init = TRUE;
	    break;
	case 'k':
	    keep_dumps = TRUE;
	    break;
	case 'r':
	    replaying = TRUE;
	    break;
	default:
	    usage();
	    break;
	}
    }

    files = argv + optind;
    last = argc - optind - 1;

    if (replaying) {
	while (last >= 0 && !fexists(files[last]))
	    --last;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    if (has_colors()) {
	short pair;
	short color;

	start_color();
	/*
	 * Assume pairs is the square of colors, and assign pairs going down
	 * so that there is minimal conflict with the background color (which
	 * counts up).  The intent is just to show how color pair values are
	 * saved and restored.
	 */
	for (pair = 0; pair < COLOR_PAIRS; ++pair) {
	    color = (short) (pair % (COLORS - 1));
	    init_pair(pair, (short) (COLOR_WHITE - color), color);
	}
    }

    if (fill_by != 0) {
	FILE *fp = fopen(fill_by, "r");
	if (fp != 0) {
	    bool filled = FALSE;
	    move(1, 0);
	    while ((ch = fgetc(fp)) != EOF) {
		if (addch(UChar(ch)) == ERR) {
		    filled = TRUE;
		    break;
		}
	    }
	    fclose(fp);
	    if (!filled) {
		while (addch(' ') != ERR) {
		    ;
		}
	    }
	    move(0, 0);
	} else {
	    endwin();
	    fprintf(stderr, "Cannot open \"%s\"\n", fill_by);
	    ExitProgram(EXIT_FAILURE);
	}
    }

    if (replaying) {

	/*
	 * Use the last file as the initial/current screen.
	 */
	if (last < 0) {
	    endwin();
	    printf("No screen-dumps given\n");
	    ExitProgram(EXIT_FAILURE);
	}

	which = last;
	if (load_screen(files[which]) == ERR) {
	    endwin();
	    printf("Cannot load screen-dump %s\n", files[which]);
	    ExitProgram(EXIT_FAILURE);
	}
	after_load();

	while (!done && (ch = getch()) != ERR) {
	    switch (ch) {
	    case 'n':
		/*
		 * If we got a "next" here, skip to the final screen before
		 * moving to the next process.
		 */
		setup_next();
		which = last;
		done = TRUE;
		break;
	    case 'q':
		cleanup(files);
		done = TRUE;
		break;
	    case KEY_BACKSPACE:
	    case '\b':
		if (--which < 0)
		    which = last;
		break;
	    case ' ':
		if (++which > last)
		    which = 0;
		break;
	    case '?':
		replay_help();
		break;
	    default:
		beep();
		continue;
	    }

	    if (ch == 'q') {
		;
	    } else if (scr_restore(files[which]) == ERR) {
		endwin();
		printf("Cannot load screen-dump %s\n", files[which]);
		cleanup(files);
		ExitProgram(EXIT_FAILURE);
	    } else {
		wrefresh(curscr);
	    }
	}
	endwin();
    } else {
	int y = 0;
	int x = 0;
	int color = 0;
	int altchars = 0;

	while (!done) {
	    switch (get_command(which, last)) {
	    case 'n':
		setup_next();
		done = TRUE;
		break;
	    case 'q':
		cleanup(files);
		done = TRUE;
		break;
	    case ' ':
		if (files[which] != 0) {
		    show_what(which + 1, last);
		    if (scr_dump(files[which]) == ERR) {
			endwin();
			printf("Cannot write screen-dump %s\n", files[which]);
			cleanup(files);
			done = TRUE;
			break;
		    }
		    ++which;
		    if (has_colors()) {
			int cx, cy;
			short pair = (short) (which % COLOR_PAIRS);
			/*
			 * Change the background color, to make it more
			 * obvious.  But that changes the existing text-color. 
			 * Copy the old values from the currently displayed
			 * screen.
			 */
			bkgd((chtype) COLOR_PAIR(pair));
			for (cy = 1; cy < LINES; ++cy) {
			    for (cx = 0; cx < COLS; ++cx) {
				wmove(curscr, cy, cx);
				wmove(stdscr, cy, cx);
#if USE_WIDEC_SUPPORT
				if (win_wch(curscr, &mycc) != ERR) {
				    myxx = wcwidth(mycc.chars[0]);
				    if (myxx > 0) {
					wadd_wchnstr(stdscr, &mycc, 1);
					cx += (myxx - 1);
				    }
				}
#else
				waddch(stdscr, winch(curscr));
#endif
			    }
			}
		    }
		} else {
		    beep();
		}
		break;
	    case KEY_LEFT:
	    case 'h':
		if (--x < 0)
		    x = COLS - 1;
		break;
	    case KEY_DOWN:
	    case 'j':
		if (++y >= LINES)
		    y = 1;
		break;
	    case KEY_UP:
	    case 'k':
		if (--y < 1)
		    y = LINES - 1;
		break;
	    case KEY_RIGHT:
	    case 'l':
		if (++x >= COLS)
		    x = 0;
		break;
	    case 'a':
		altchars = !altchars;
		break;
	    case 'c':
		color = (color + 1) % COLORS;
		break;
	    case '?':
		editor_help();
		break;
	    default:
		beep();
		continue;
	    }
	    if (!done) {
		attr_t attr = (A_REVERSE | COLOR_PAIR(color * COLORS));
		chtype ch2 = (altchars ? ACS_DIAMOND : '#');
		move(y, x);
		addch(ch2 | attr);
		move(y, x);
	    }
	}
	endwin();
    }
    ExitProgram(EXIT_SUCCESS);
}

#else
int
main(int argc, char *argv[])
{
    printf("This program requires the screen-dump functions\n");
    ExitProgram(EXIT_FAILURE);
}
#endif

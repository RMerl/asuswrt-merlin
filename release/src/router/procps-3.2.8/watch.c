/* watch -- execute a program repeatedly, displaying output fullscreen
 *
 * Based on the original 1991 'watch' by Tony Rems <rembo@unisoft.com>
 * (with mods and corrections by Francois Pinard).
 *
 * Substantially reworked, new features (differences option, SIGWINCH
 * handling, unlimited command length, long line handling) added Apr 1999 by
 * Mike Coleman <mkc@acm.org>.
 *
 * Changes by Albert Cahalan, 2002-2003.
 */

#define VERSION "0.2.0"

#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <locale.h>
#include "proc/procps.h"

#ifdef FORCE_8BIT
#undef isprint
#define isprint(x) ( (x>=' '&&x<='~') || (x>=0xa0) )
#endif

static struct option longopts[] = {
	{"differences", optional_argument, 0, 'd'},
	{"help", no_argument, 0, 'h'},
	{"interval", required_argument, 0, 'n'},
	{"no-title", no_argument, 0, 't'},
	{"version", no_argument, 0, 'v'},
	{0, 0, 0, 0}
};

static char usage[] =
    "Usage: %s [-dhntv] [--differences[=cumulative]] [--help] [--interval=<n>] [--no-title] [--version] <command>\n";

static char *progname;

static int curses_started = 0;
static int height = 24, width = 80;
static int screen_size_changed = 0;
static int first_screen = 1;
static int show_title = 2;  // number of lines used, 2 or 0

#define min(x,y) ((x) > (y) ? (y) : (x))

static void do_usage(void) NORETURN;
static void do_usage(void)
{
	fprintf(stderr, usage, progname);
	exit(1);
}

static void do_exit(int status) NORETURN;
static void do_exit(int status)
{
	if (curses_started)
		endwin();
	exit(status);
}

/* signal handler */
static void die(int notused) NORETURN;
static void die(int notused)
{
	(void) notused;
	do_exit(0);
}

static void
winch_handler(int notused)
{
	(void) notused;
	screen_size_changed = 1;
}

static char env_col_buf[24];
static char env_row_buf[24];
static int incoming_cols;
static int incoming_rows;

static void
get_terminal_size(void)
{
	struct winsize w;
	if(!incoming_cols){  // have we checked COLUMNS?
		const char *s = getenv("COLUMNS");
		incoming_cols = -1;
		if(s && *s){
			long t;
			char *endptr;
			t = strtol(s, &endptr, 0);
			if(!*endptr && (t>0) && (t<(long)666)) incoming_cols = (int)t;
			width = incoming_cols;
			snprintf(env_col_buf, sizeof env_col_buf, "COLUMNS=%d", width);
			putenv(env_col_buf);
		}
	}
	if(!incoming_rows){  // have we checked LINES?
		const char *s = getenv("LINES");
		incoming_rows = -1;
		if(s && *s){
			long t;
			char *endptr;
			t = strtol(s, &endptr, 0);
			if(!*endptr && (t>0) && (t<(long)666)) incoming_rows = (int)t;
			height = incoming_rows;
			snprintf(env_row_buf, sizeof env_row_buf, "LINES=%d", height);
			putenv(env_row_buf);
		}
	}
	if (incoming_cols<0 || incoming_rows<0){
		if (ioctl(2, TIOCGWINSZ, &w) == 0) {
			if (incoming_rows<0 && w.ws_row > 0){
				height = w.ws_row;
				snprintf(env_row_buf, sizeof env_row_buf, "LINES=%d", height);
				putenv(env_row_buf);
			}
			if (incoming_cols<0 && w.ws_col > 0){
				width = w.ws_col;
				snprintf(env_col_buf, sizeof env_col_buf, "COLUMNS=%d", width);
				putenv(env_col_buf);
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int optc;
	int option_differences = 0,
	    option_differences_cumulative = 0,
	    option_help = 0, option_version = 0;
	double interval = 2;
	char *command;
	int command_length = 0;	/* not including final \0 */

	setlocale(LC_ALL, "");
	progname = argv[0];

	while ((optc = getopt_long(argc, argv, "+d::hn:vt", longopts, (int *) 0))
	       != EOF) {
		switch (optc) {
		case 'd':
			option_differences = 1;
			if (optarg)
				option_differences_cumulative = 1;
			break;
		case 'h':
			option_help = 1;
			break;
		case 't':
			show_title = 0;
			break;
		case 'n':
			{
				char *str;
				interval = strtod(optarg, &str);
				if (!*optarg || *str)
					do_usage();
				if(interval < 0.1)
					interval = 0.1;
				if(interval > ~0u/1000000)
					interval = ~0u/1000000;
			}
			break;
		case 'v':
			option_version = 1;
			break;
		default:
			do_usage();
			break;
		}
	}

	if (option_version) {
		fprintf(stderr, "%s\n", VERSION);
		if (!option_help)
			exit(0);
	}

	if (option_help) {
		fprintf(stderr, usage, progname);
		fputs("  -d, --differences[=cumulative]\thighlight changes between updates\n", stderr);
		fputs("\t\t(cumulative means highlighting is cumulative)\n", stderr);
		fputs("  -h, --help\t\t\t\tprint a summary of the options\n", stderr);
		fputs("  -n, --interval=<seconds>\t\tseconds to wait between updates\n", stderr);
		fputs("  -v, --version\t\t\t\tprint the version number\n", stderr);
		fputs("  -t, --no-title\t\t\tturns off showing the header\n", stderr);
		exit(0);
	}

	if (optind >= argc)
		do_usage();

	command = strdup(argv[optind++]);
	command_length = strlen(command);
	for (; optind < argc; optind++) {
		char *endp;
		int s = strlen(argv[optind]);
		command = realloc(command, command_length + s + 2);	/* space and \0 */
		endp = command + command_length;
		*endp = ' ';
		memcpy(endp + 1, argv[optind], s);
		command_length += 1 + s;	/* space then string length */
		command[command_length] = '\0';
	}

	get_terminal_size();

	/* Catch keyboard interrupts so we can put tty back in a sane state.  */
	signal(SIGINT, die);
	signal(SIGTERM, die);
	signal(SIGHUP, die);
	signal(SIGWINCH, winch_handler);

	/* Set up tty for curses use.  */
	curses_started = 1;
	initscr();
	nonl();
	noecho();
	cbreak();

	for (;;) {
		time_t t = time(NULL);
		char *ts = ctime(&t);
		int tsl = strlen(ts);
		char *header;
		FILE *p;
		int x, y;
		int oldeolseen = 1;

		if (screen_size_changed) {
			get_terminal_size();
			resizeterm(height, width);
			clear();
			/* redrawwin(stdscr); */
			screen_size_changed = 0;
			first_screen = 1;
		}

		if (show_title) {
			// left justify interval and command,
			// right justify time, clipping all to fit window width
			asprintf(&header, "Every %.1fs: %.*s",
				interval, min(width - 1, command_length), command);
			mvaddstr(0, 0, header);
			if (strlen(header) > (size_t) (width - tsl - 1))
				mvaddstr(0, width - tsl - 4, "...  ");
			mvaddstr(0, width - tsl + 1, ts);
			free(header);
		}

		if (!(p = popen(command, "r"))) {
			perror("popen");
			do_exit(2);
		}

		for (y = show_title; y < height; y++) {
			int eolseen = 0, tabpending = 0;
			for (x = 0; x < width; x++) {
				int c = ' ';
				int attr = 0;

				if (!eolseen) {
					/* if there is a tab pending, just spit spaces until the
					   next stop instead of reading characters */
					if (!tabpending)
						do
							c = getc(p);
						while (c != EOF && !isprint(c)
						       && c != '\n'
						       && c != '\t');
					if (c == '\n')
						if (!oldeolseen && x == 0) {
							x = -1;
							continue;
						} else
							eolseen = 1;
					else if (c == '\t')
						tabpending = 1;
					if (c == EOF || c == '\n' || c == '\t')
						c = ' ';
					if (tabpending && (((x + 1) % 8) == 0))
						tabpending = 0;
				}
				move(y, x);
				if (option_differences) {
					chtype oldch = inch();
					char oldc = oldch & A_CHARTEXT;
					attr = !first_screen
					    && ((char)c != oldc
						||
						(option_differences_cumulative
						 && (oldch & A_ATTRIBUTES)));
				}
				if (attr)
					standout();
				addch(c);
				if (attr)
					standend();
			}
			oldeolseen = eolseen;
		}

		pclose(p);

		first_screen = 0;
		refresh();
		usleep(interval * 1000000);
	}

	endwin();

	return 0;
}

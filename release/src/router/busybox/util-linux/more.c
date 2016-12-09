/* vi: set sw=4 ts=4: */
/*
 * Mini more implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Latest version blended together by Erik Andersen <andersen@codepoet.org>,
 * based on the original more implementation by Bruce, and code from the
 * Debian boot-floppies team.
 *
 * Termios corrects by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define more_trivial_usage
//usage:       "[FILE]..."
//usage:#define more_full_usage "\n\n"
//usage:       "View FILE (or stdin) one screenful at a time"
//usage:
//usage:#define more_example_usage
//usage:       "$ dmesg | more\n"

#include "libbb.h"
#include "common_bufsiz.h"

/* Support for FEATURE_USE_TERMIOS */

struct globals {
	int cin_fileno;
	struct termios initial_settings;
	struct termios new_settings;
} FIX_ALIASING;
#define G (*(struct globals*)bb_common_bufsiz1)
#define initial_settings (G.initial_settings)
#define new_settings     (G.new_settings    )
#define cin_fileno       (G.cin_fileno      )
#define INIT_G() do { setup_common_bufsiz(); } while (0)

#define setTermSettings(fd, argp) \
do { \
	if (ENABLE_FEATURE_USE_TERMIOS) \
		tcsetattr(fd, TCSANOW, argp); \
} while (0)
#define getTermSettings(fd, argp) tcgetattr(fd, argp)

static void gotsig(int sig UNUSED_PARAM)
{
	/* bb_putchar_stderr doesn't use stdio buffering,
	 * therefore it is safe in signal handler */
	bb_putchar_stderr('\n');
	setTermSettings(cin_fileno, &initial_settings);
	_exit(EXIT_FAILURE);
}

#define CONVERTED_TAB_SIZE 8

int more_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int more_main(int argc UNUSED_PARAM, char **argv)
{
	int c = c; /* for compiler */
	int lines;
	int input = 0;
	int spaces = 0;
	int please_display_more_prompt;
	struct stat st;
	FILE *file;
	FILE *cin;
	int len;
	unsigned terminal_width;
	unsigned terminal_height;

	INIT_G();

	argv++;
	/* Another popular pager, most, detects when stdout
	 * is not a tty and turns into cat. This makes sense. */
	if (!isatty(STDOUT_FILENO))
		return bb_cat(argv);
	cin = fopen_for_read(CURRENT_TTY);
	if (!cin)
		return bb_cat(argv);

	if (ENABLE_FEATURE_USE_TERMIOS) {
		cin_fileno = fileno(cin);
		getTermSettings(cin_fileno, &initial_settings);
		new_settings = initial_settings;
		new_settings.c_lflag &= ~(ICANON | ECHO);
		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;
		setTermSettings(cin_fileno, &new_settings);
		bb_signals(0
			+ (1 << SIGINT)
			+ (1 << SIGQUIT)
			+ (1 << SIGTERM)
			, gotsig);
	}

	do {
		file = stdin;
		if (*argv) {
			file = fopen_or_warn(*argv, "r");
			if (!file)
				continue;
		}
		st.st_size = 0;
		fstat(fileno(file), &st);

		please_display_more_prompt = 0;
		/* never returns w, h <= 1 */
		get_terminal_width_height(fileno(cin), &terminal_width, &terminal_height);
		terminal_height -= 1;

		len = 0;
		lines = 0;
		while (spaces || (c = getc(file)) != EOF) {
			int wrap;
			if (spaces)
				spaces--;
 loop_top:
			if (input != 'r' && please_display_more_prompt) {
				len = printf("--More-- ");
				if (st.st_size != 0) {
					uoff_t d = (uoff_t)st.st_size / 100;
					if (d == 0)
						d = 1;
					len += printf("(%u%% of %"OFF_FMT"u bytes)",
						(int) ((uoff_t)ftello(file) / d),
						st.st_size);
				}
				fflush_all();

				/*
				 * We've just displayed the "--More--" prompt, so now we need
				 * to get input from the user.
				 */
				for (;;) {
					input = getc(cin);
					input = tolower(input);
					if (!ENABLE_FEATURE_USE_TERMIOS)
						printf("\033[A"); /* cursor up */
					/* Erase the last message */
					printf("\r%*s\r", len, "");

					/* Due to various multibyte escape
					 * sequences, it's not ok to accept
					 * any input as a command to scroll
					 * the screen. We only allow known
					 * commands, else we show help msg. */
					if (input == ' ' || input == '\n' || input == 'q' || input == 'r')
						break;
					len = printf("(Enter:next line Space:next page Q:quit R:show the rest)");
				}
				len = 0;
				lines = 0;
				please_display_more_prompt = 0;

				if (input == 'q')
					goto end;

				/* The user may have resized the terminal.
				 * Re-read the dimensions. */
				if (ENABLE_FEATURE_USE_TERMIOS) {
					get_terminal_width_height(cin_fileno, &terminal_width, &terminal_height);
					terminal_height -= 1;
				}
			}

			/* Crudely convert tabs into spaces, which are
			 * a bajillion times easier to deal with. */
			if (c == '\t') {
				spaces = ((unsigned)~len) % CONVERTED_TAB_SIZE;
				c = ' ';
			}

			/*
			 * There are two input streams to worry about here:
			 *
			 * c    : the character we are reading from the file being "mored"
			 * input: a character received from the keyboard
			 *
			 * If we hit a newline in the _file_ stream, we want to test and
			 * see if any characters have been hit in the _input_ stream. This
			 * allows the user to quit while in the middle of a file.
			 */
			wrap = (++len > terminal_width);
			if (c == '\n' || wrap) {
				/* Then outputting this character
				 * will move us to a new line. */
				if (++lines >= terminal_height || input == '\n')
					please_display_more_prompt = 1;
				len = 0;
			}
			if (c != '\n' && wrap) {
				/* Then outputting this will also put a character on
				 * the beginning of that new line. Thus we first want to
				 * display the prompt (if any), so we skip the putchar()
				 * and go back to the top of the loop, without reading
				 * a new character. */
				goto loop_top;
			}
			/* My small mind cannot fathom backspaces and UTF-8 */
			putchar(c);
			die_if_ferror_stdout(); /* if tty was destroyed (closed xterm, etc) */
		}
		fclose(file);
		fflush_all();
	} while (*argv && *++argv);
 end:
	setTermSettings(cin_fileno, &initial_settings);
	return 0;
}

/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
    Software supplied and written by http://www.cleancode.org

    This file is part of eMail.

    eMail is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    eMail is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with eMail; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "email.h"
#include "utils.h"
#include "progress_bar.h"

#define SUBLEN 20
#define MIN_WIN_SIZE 50
#define MAX_WIN_SIZE 120
#define BAR_REFRESH  500

/**
 * Truncate the message supplied if 'message' is not NULL.  
 * If message is NULL, return the string "(No Subject)".  
 * If message is not null, truncate it to the right size and 
 * return it to the user via a static variable.
**/
static void
truncateSubject(const char *message, struct prbar *bar)
{
	size_t messlen;

	bar->subject = xmalloc(SUBLEN);
	memset(bar->subject, 0, SUBLEN);

	if (message == NULL) {
		memcpy(bar->subject, "(No Subject)", 12);
	} else {
		messlen = strlen(message);
		if ((messlen - SUBLEN) > 0) {   /* If message exceeds SUBLEN */
			memcpy(bar->subject, message, SUBLEN - 4);
			bar->subject[SUBLEN - 4] = '.';
			bar->subject[SUBLEN - 3] = '.';
			bar->subject[SUBLEN - 2] = '.';
		} else {
			memcpy(bar->subject, message, messlen);
		}
	}
}

/**
 * will initialize the prbar struct and return it to the caller
 * this function will set defaults and configure the necessary values
**/
struct prbar *
prbarInit(size_t bytes)
{
	struct winsize win_size;
	struct prbar *bar;

	bar = xmalloc(sizeof(struct prbar));

	/* Clear error status if any */
	bar->percent = 0;
	bar->curr_size = 0;

	bar->actual_file_size = bytes;
	bar->truncated_file_size = bytes;

	/* Set up truncated file size to the nearest KB or MB */
	bar->size_type = "Bytes";
	if (bar->truncated_file_size > 1024) {      /* Kilobyte */
		bar->truncated_file_size /= 1024;
		bar->size_type = "KB";
	}
	if (bar->truncated_file_size > 1024) {      /* Megabyte */
		bar->truncated_file_size /= 1024;
		bar->size_type = "MB";
	}

	/* If they don't have stdout, set error */
	if (!isatty(STDOUT_FILENO)) {
		free(bar);
		return NULL;
	}

	/* Set the size of the terminal */
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *) &win_size) < 0) {
		free(bar);
		return NULL;
	}

	/* If the bar is so small, no point in displaying it */
	if (win_size.ws_col < MIN_WIN_SIZE) {
		free(bar);
		return NULL;
	}

	if (win_size.ws_col > MAX_WIN_SIZE) {
		win_size.ws_col = MAX_WIN_SIZE;
	}

	/**
	* We know that our message has atleast 29 characters in it.  
	* We need to subtract the length of the subject and the lenght
	* of the size_ent as well so that we may fit the screen.
	**/
	truncateSubject(Mopts.subject, bar);
	bar->bar_size = ((win_size.ws_col - 29) - 
		(strlen(bar->subject) + strlen(bar->size_type)));

	bar->buf = malloc(bar->bar_size + 1);
	memset(bar->buf, ' ', bar->bar_size);
	bar->buf[bar->bar_size] = '\0';
	return bar;
}

/**
 * prints a progress bar on the screen determined by the 
 * file size and current size sent to the server. 
**/
void
prbarPrint(size_t bytes, struct prbar *bar)
{
	int curr_bar_size;

	assert(bar != NULL);

	/* Set our current display state */
	bar->curr_size += bytes;
	bar->percent = (bar->curr_size * 100) / bar->actual_file_size;

	/**
	* only update the progress bar when
	* the size of the file sent is greater
	* than BAR_REFRESH  or if we're at 100%
	*
	* RETHINK: Um, what the hell did I mean here??
	**/
	if ((bar->percent != 100) && 
		((bar->curr_size - bar->progress) <= BAR_REFRESH)) {
		return;
	}

	bar->progress = bar->curr_size;
	curr_bar_size = (bar->curr_size * bar->bar_size) / bar->actual_file_size;

	memset(bar->buf, '*', curr_bar_size);
	printf("\rSending  \"%s\"  |%s| %3d%% of %2d %s",
		bar->subject, bar->buf, bar->percent, 
		bar->truncated_file_size, bar->size_type);
	if (bar->percent == 100) {
		printf("\n");
	}
	fflush(stdout);
}

/**
 * allows a user to destroy the prbar struct properly
**/
void
prbarDestroy(struct prbar *bar)
{
	if (bar) {
		xfree(bar->buf);
		xfree(bar->subject);
		xfree(bar);
	}
}


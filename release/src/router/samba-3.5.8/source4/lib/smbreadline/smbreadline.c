/* 
   Unix SMB/CIFS implementation.
   Samba readline wrapper implementation
   Copyright (C) Simo Sorce 2001
   Copyright (C) Andrew Tridgell 2001
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "system/select.h"
#include "system/readline.h"
#include "lib/smbreadline/smbreadline.h"

/*******************************************************************
 Similar to sys_select() but catch EINTR and continue.
 This is what sys_select() used to do in Samba.
********************************************************************/

static int sys_select_intr(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *tval)
{
	int ret;
	fd_set *readfds2, readfds_buf, *writefds2, writefds_buf, *errorfds2, errorfds_buf;
	struct timeval tval2, *ptval;

	readfds2 = (readfds ? &readfds_buf : NULL);
	writefds2 = (writefds ? &writefds_buf : NULL);
	errorfds2 = (errorfds ? &errorfds_buf : NULL);
	ptval = (tval ? &tval2 : NULL);

	do {
		if (readfds)
			readfds_buf = *readfds;
		if (writefds)
			writefds_buf = *writefds;
		if (errorfds)
			errorfds_buf = *errorfds;
		if (tval)
			tval2 = *tval;

		/* We must use select and not sys_select here. If we use
		   sys_select we'd lose the fact a signal occurred when sys_select
		   read a byte from the pipe. Fix from Mark Weaver
		   <mark-clist@npsl.co.uk>
		*/

		ret = select(maxfd, readfds2, writefds2, errorfds2, ptval);
	} while (ret == -1 && errno == EINTR);

	if (readfds)
		*readfds = readfds_buf;
	if (writefds)
		*writefds = writefds_buf;
	if (errorfds)
		*errorfds = errorfds_buf;

	return ret;
}

/****************************************************************************
 Display the prompt and wait for input. Call callback() regularly
****************************************************************************/

static char *smb_readline_replacement(const char *prompt, void (*callback)(void), 
				      char **(completion_fn)(const char *text, int start, int end))
{
	fd_set fds;
	char *line;
	struct timeval timeout;
	int fd = STDIN_FILENO;
	char *ret;

	printf("%s", prompt);
	fflush(stdout);

	line = (char *)malloc(BUFSIZ);
	if (!line) {
		return NULL;
	}

	while (1) {
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		FD_ZERO(&fds);
		FD_SET(fd,&fds);

		if (sys_select_intr(fd+1,&fds,NULL,NULL,&timeout) == 1) {
			ret = x_fgets(line, BUFSIZ, x_stdin);
			return ret;
		}
		if (callback)
			callback();
	}
}

/****************************************************************************
 Display the prompt and wait for input. Call callback() regularly.
****************************************************************************/

char *smb_readline(const char *prompt, void (*callback)(void), 
		   char **(completion_fn)(const char *text, int start, int end))
{
#if HAVE_LIBREADLINE
	if (isatty(STDIN_FILENO)) {
		char *ret;

		/* Aargh!  Readline does bizzare things with the terminal width
		that mucks up expect(1).  Set CLI_NO_READLINE in the environment
		to force readline not to be used. */

		if (getenv("CLI_NO_READLINE"))
			return smb_readline_replacement(prompt, callback, completion_fn);

		if (completion_fn) {
			/* The callback prototype has changed slightly between
			different versions of Readline, so the same function
			works in all of them to date, but we get compiler
			warnings in some.  */
			rl_attempted_completion_function = RL_COMPLETION_CAST completion_fn;
		}

#if HAVE_DECL_RL_EVENT_HOOK
		if (callback)
			rl_event_hook = (Function *)callback;
#endif
		ret = readline(prompt);
		if (ret && *ret)
			add_history(ret);
		return ret;
	} else
#endif
	return smb_readline_replacement(prompt, callback, completion_fn);
}

/****************************************************************************
 * return line buffer text
 ****************************************************************************/
const char *smb_readline_get_line_buffer(void)
{
#if defined(HAVE_LIBREADLINE)
	return rl_line_buffer;
#else
	return NULL;
#endif
}

/****************************************************************************
 * set completion append character
 ***************************************************************************/
void smb_readline_ca_char(char c)
{
#if defined(HAVE_LIBREADLINE)
	rl_completion_append_character = c;
#endif
}




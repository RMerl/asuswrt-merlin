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
#include "../lib/util/select.h"
#include "system/filesys.h"
#include "system/select.h"
#include "system/readline.h"
#include "libcli/smbreadline/smbreadline.h"

#undef malloc

#ifdef HAVE_LIBREADLINE
#  ifdef HAVE_READLINE_READLINE_H
#    include <readline/readline.h>
#    ifdef HAVE_READLINE_HISTORY_H
#      include <readline/history.h>
#    endif
#  else
#    ifdef HAVE_READLINE_H
#      include <readline.h>
#      ifdef HAVE_HISTORY_H
#        include <history.h>
#      endif
#    else
#      undef HAVE_LIBREADLINE
#    endif
#  endif
#endif

static bool smb_rl_done;

#if HAVE_LIBREADLINE
/*
 * MacOS/X does not have rl_done in readline.h, but
 * readline.so has it
 */
extern int rl_done;
#endif

void smb_readline_done(void)
{
	smb_rl_done = true;
#if HAVE_LIBREADLINE
	rl_done = 1;
#endif
}

/****************************************************************************
 Display the prompt and wait for input. Call callback() regularly
****************************************************************************/

static char *smb_readline_replacement(const char *prompt, void (*callback)(void),
				char **(completion_fn)(const char *text, int start, int end))
{
	char *line = NULL;
	int fd = x_fileno(x_stdin);
	char *ret;

	/* Prompt might be NULL in non-interactive mode. */
	if (prompt) {
		x_fprintf(x_stdout, "%s", prompt);
		x_fflush(x_stdout);
	}

	line = (char *)malloc(BUFSIZ);
	if (!line) {
		return NULL;
	}

	while (!smb_rl_done) {
		struct pollfd pfd;

		ZERO_STRUCT(pfd);
		pfd.fd = fd;
		pfd.events = POLLIN|POLLHUP;

		if (sys_poll_intr(&pfd, 1, 5000) == 1) {
			ret = x_fgets(line, BUFSIZ, x_stdin);
			if (ret == 0) {
				SAFE_FREE(line);
			}
			return ret;
		}
		if (callback) {
			callback();
		}
	}
	SAFE_FREE(line);
	return NULL;
}

/****************************************************************************
 Display the prompt and wait for input. Call callback() regularly.
****************************************************************************/

char *smb_readline(const char *prompt, void (*callback)(void),
		   char **(completion_fn)(const char *text, int start, int end))
{
	char *ret;
	bool interactive;

	interactive = isatty(x_fileno(x_stdin)) || getenv("CLI_FORCE_INTERACTIVE");
	if (!interactive) {
		return smb_readline_replacement(NULL, callback, completion_fn);
	}

#if HAVE_LIBREADLINE

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

#else
	ret = smb_readline_replacement(prompt, callback, completion_fn);
#endif

	return ret;
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

/*
 * Dropbear SSH
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "buffer.h"
#include "dbutil.h"
#include "session.h"
#include "ssh.h"
#include "runopts.h"

#ifdef ENABLE_CLI_PASSWORD_AUTH

#ifdef ENABLE_CLI_ASKPASS_HELPER
/* Returns 1 if we want to use the askpass program, 0 otherwise */
static int want_askpass()
{
	char* askpass_prog = NULL;

	askpass_prog = getenv("SSH_ASKPASS");
	return askpass_prog && 
		((!isatty(STDIN_FILENO) && getenv("DISPLAY") )
		 	|| getenv("SSH_ASKPASS_ALWAYS"));
}

/* returns a statically allocated password from a helper app, or NULL
 * on failure */
static char *gui_getpass(const char *prompt) {

	pid_t pid;
	int p[2], maxlen, len, status;
	static char buf[DROPBEAR_MAX_CLI_PASS + 1];
	char* helper = NULL;

	TRACE(("enter gui_getpass"))

	helper = getenv("SSH_ASKPASS");
	if (!helper)
	{
		TRACE(("leave gui_getpass: no askpass program"))
		return NULL;
	}

	if (pipe(p) < 0) {
		TRACE(("error creating child pipe"))
		return NULL;
	}

	pid = fork();

	if (pid < 0) {
		TRACE(("fork error"))
		return NULL;
	}

	if (!pid) {
		/* child */
		close(p[0]);
		if (dup2(p[1], STDOUT_FILENO) < 0) {
			TRACE(("error redirecting stdout"))
			exit(1);
		}
		close(p[1]);
		execlp(helper, helper, prompt, (char *)0);
		TRACE(("execlp error"))
		exit(1);
	}

	close(p[1]);
	maxlen = sizeof(buf);
	while (maxlen > 0) {
		len = read(p[0], buf + sizeof(buf) - maxlen, maxlen);
		if (len > 0) {
			maxlen -= len;
		} else {
			if (errno != EINTR)
				break;
		}
	}

	close(p[0]);

	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return(NULL);

	len = sizeof(buf) - maxlen;
	buf[len] = '\0';
	if (len > 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	TRACE(("leave gui_getpass"))
	return(buf);
}
#endif /* ENABLE_CLI_ASKPASS_HELPER */

void cli_auth_password() {

	char* password = NULL;
	char prompt[80];

	TRACE(("enter cli_auth_password"))
	CHECKCLEARTOWRITE();

	snprintf(prompt, sizeof(prompt), "%s@%s's password: ", 
				cli_opts.username, cli_opts.remotehost);
#ifdef ENABLE_CLI_ASKPASS_HELPER
	if (want_askpass())
	{
		password = gui_getpass(prompt);
		if (!password) {
			dropbear_exit("No password");
		}
	} else
#endif
	{
		password = getpass_or_cancel(prompt);
	}

	buf_putbyte(ses.writepayload, SSH_MSG_USERAUTH_REQUEST);

	buf_putstring(ses.writepayload, cli_opts.username,
			strlen(cli_opts.username));

	buf_putstring(ses.writepayload, SSH_SERVICE_CONNECTION, 
			SSH_SERVICE_CONNECTION_LEN);

	buf_putstring(ses.writepayload, AUTH_METHOD_PASSWORD, 
			AUTH_METHOD_PASSWORD_LEN);

	buf_putbyte(ses.writepayload, 0); /* FALSE - so says the spec */

	buf_putstring(ses.writepayload, password, strlen(password));

	encrypt_packet();
	m_burn(password, strlen(password));

	TRACE(("leave cli_auth_password"))
}
#endif	/* ENABLE_CLI_PASSWORD_AUTH */

/*
 * Dropbear SSH
 * 
 * Copyright (c) 2005 Matt Johnston
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

#ifdef ENABLE_CLI_INTERACT_AUTH

static char* get_response(char* prompt)
{
	FILE* tty = NULL;
	char* response = NULL;
	/* not a password, but a reasonable limit */
	char buf[DROPBEAR_MAX_CLI_PASS];
	char* ret = NULL;

	fprintf(stderr, "%s", prompt);

	tty = fopen(_PATH_TTY, "r");
	if (tty) {
		ret = fgets(buf, sizeof(buf), tty);
		fclose(tty);
	} else {
		ret = fgets(buf, sizeof(buf), stdin);
	}

	if (ret == NULL) {
		response = m_strdup("");
	} else {
		unsigned int buflen = strlen(buf);
		/* fgets includes newlines */
		if (buflen > 0 && buf[buflen-1] == '\n')
			buf[buflen-1] = '\0';
		response = m_strdup(buf);
	}

	m_burn(buf, sizeof(buf));

	return response;
}

void recv_msg_userauth_info_request() {

	char *name = NULL;
	char *instruction = NULL;
	unsigned int num_prompts = 0;
	unsigned int i;

	char *prompt = NULL;
	unsigned int echo = 0;
	char *response = NULL;

	TRACE(("enter recv_msg_recv_userauth_info_request"))

	/* Let the user know what password/host they are authing for */
	if (!cli_ses.interact_request_received) {
		fprintf(stderr, "Login for %s@%s\n", cli_opts.username,
				cli_opts.remotehost);
	}
	cli_ses.interact_request_received = 1;

	name = buf_getstring(ses.payload, NULL);
	instruction = buf_getstring(ses.payload, NULL);

	/* language tag */
	buf_eatstring(ses.payload);

	num_prompts = buf_getint(ses.payload);
	
	if (num_prompts >= DROPBEAR_MAX_CLI_INTERACT_PROMPTS) {
		dropbear_exit("Too many prompts received for keyboard-interactive");
	}

	/* we'll build the response as we go */
	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_USERAUTH_INFO_RESPONSE);
	buf_putint(ses.writepayload, num_prompts);

	if (strlen(name) > 0) {
		cleantext(name);
		fprintf(stderr, "%s", name);
	}
	m_free(name);

	if (strlen(instruction) > 0) {
		cleantext(instruction);
		fprintf(stderr, "%s", instruction);
	}
	m_free(instruction);

	for (i = 0; i < num_prompts; i++) {
		unsigned int response_len = 0;
		prompt = buf_getstring(ses.payload, NULL);
		cleantext(prompt);

		echo = buf_getbool(ses.payload);

		if (!echo) {
			char* p = getpass_or_cancel(prompt);
			response = m_strdup(p);
			m_burn(p, strlen(p));
		} else {
			response = get_response(prompt);
		}

		response_len = strlen(response);
		buf_putstring(ses.writepayload, response, response_len);
		m_burn(response, response_len);
		m_free(prompt);
		m_free(response);
	}

	encrypt_packet();


	TRACE(("leave recv_msg_recv_userauth_info_request"))
}

void cli_auth_interactive() {

	TRACE(("enter cli_auth_interactive"))
	CHECKCLEARTOWRITE();

	buf_putbyte(ses.writepayload, SSH_MSG_USERAUTH_REQUEST);

	/* username */
	buf_putstring(ses.writepayload, cli_opts.username,
			strlen(cli_opts.username));

	/* service name */
	buf_putstring(ses.writepayload, SSH_SERVICE_CONNECTION,
			SSH_SERVICE_CONNECTION_LEN);

	/* method */
	buf_putstring(ses.writepayload, AUTH_METHOD_INTERACT,
			AUTH_METHOD_INTERACT_LEN);

	/* empty language tag */
	buf_putstring(ses.writepayload, "", 0);

	/* empty submethods */
	buf_putstring(ses.writepayload, "", 0);

	encrypt_packet();
	cli_ses.interact_request_received = 0;

	TRACE(("leave cli_auth_interactive"))

}
#endif	/* ENABLE_CLI_INTERACT_AUTH */

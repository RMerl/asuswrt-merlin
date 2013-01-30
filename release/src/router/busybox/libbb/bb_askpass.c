/* vi: set sw=4 ts=4: */
/*
 * Ask for a password
 * I use a static buffer in this function.  Plan accordingly.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* do nothing signal handler */
static void askpass_timeout(int UNUSED_PARAM ignore)
{
}

char* FAST_FUNC bb_ask_stdin(const char *prompt)
{
	return bb_ask(STDIN_FILENO, 0, prompt);
}
char* FAST_FUNC bb_ask(const int fd, int timeout, const char *prompt)
{
	/* Was static char[BIGNUM] */
	enum { sizeof_passwd = 128 };
	static char *passwd;

	char *ret;
	int i;
	struct sigaction sa, oldsa;
	struct termios tio, oldtio;

	tcgetattr(fd, &oldtio);
	tcflush(fd, TCIFLUSH);
	tio = oldtio;
#ifndef IUCLC
# define IUCLC 0
#endif
	tio.c_iflag &= ~(IUCLC|IXON|IXOFF|IXANY);
	tio.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|TOSTOP);
	tcsetattr(fd, TCSANOW, &tio);

	memset(&sa, 0, sizeof(sa));
	/* sa.sa_flags = 0; - no SA_RESTART! */
	/* SIGINT and SIGALRM will interrupt reads below */
	sa.sa_handler = askpass_timeout;
	sigaction(SIGINT, &sa, &oldsa);
	if (timeout) {
		sigaction_set(SIGALRM, &sa);
		alarm(timeout);
	}

	fputs(prompt, stdout);
	fflush_all();

	if (!passwd)
		passwd = xmalloc(sizeof_passwd);
	ret = passwd;
	i = 0;
	while (1) {
		int r = read(fd, &ret[i], 1);
		if (r < 0) {
			/* read is interrupted by timeout or ^C */
			ret = NULL;
			break;
		}
		if (r == 0 /* EOF */
		 || ret[i] == '\r' || ret[i] == '\n' /* EOL */
		 || ++i == sizeof_passwd-1 /* line limit */
		) {
			ret[i] = '\0';
			break;
		}
	}

	if (timeout) {
		alarm(0);
	}
	sigaction_set(SIGINT, &oldsa);
	tcsetattr(fd, TCSANOW, &oldtio);
	bb_putchar('\n');
	fflush_all();
	return ret;
}

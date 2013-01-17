/* vi: set sw=4 ts=4: */
/*
 * issue.c: issue printing code
 *
 * Copyright (C) 2003 Bastian Blank <waldi@tuxbox.org>
 *
 * Optimize and correcting OCRNL by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"
/* After libbb.h, since it needs sys/types.h on some systems */
#include <sys/utsname.h>

#define LOGIN " login: "

static const char fmtstr_d[] ALIGN1 = "%A, %d %B %Y";
static const char fmtstr_t[] ALIGN1 = "%H:%M:%S";

void FAST_FUNC print_login_issue(const char *issue_file, const char *tty)
{
	FILE *fp;
	int c;
	char buf[256+1];
	const char *outbuf;
	time_t t;
	struct utsname uts;

	time(&t);
	uname(&uts);

	puts("\r");  /* start a new line */

	fp = fopen_for_read(issue_file);
	if (!fp)
		return;
	while ((c = fgetc(fp)) != EOF) {
		outbuf = buf;
		buf[0] = c;
		buf[1] = '\0';
		if (c == '\n') {
			buf[1] = '\r';
			buf[2] = '\0';
		}
		if (c == '\\' || c == '%') {
			c = fgetc(fp);
			switch (c) {
			case 's':
				outbuf = uts.sysname;
				break;
			case 'n':
			case 'h':
				outbuf = uts.nodename;
				break;
			case 'r':
				outbuf = uts.release;
				break;
			case 'v':
				outbuf = uts.version;
				break;
			case 'm':
				outbuf = uts.machine;
				break;
/* The field domainname of struct utsname is Linux specific. */
#if defined(__linux__)
			case 'D':
			case 'o':
				outbuf = uts.domainname;
				break;
#endif
			case 'd':
				strftime(buf, sizeof(buf), fmtstr_d, localtime(&t));
				break;
			case 't':
				strftime(buf, sizeof(buf), fmtstr_t, localtime(&t));
				break;
			case 'l':
				outbuf = tty;
				break;
			default:
				buf[0] = c;
			}
		}
		fputs(outbuf, stdout);
	}
	fclose(fp);
	fflush_all();
}

void FAST_FUNC print_login_prompt(void)
{
	char *hostname = safe_gethostname();

	fputs(hostname, stdout);
	fputs(LOGIN, stdout);
	fflush_all();
	free(hostname);
}

/* Clear dangerous stuff, set PATH */
static const char forbid[] ALIGN1 =
	"ENV" "\0"
	"BASH_ENV" "\0"
	"HOME" "\0"
	"IFS" "\0"
	"SHELL" "\0"
	"LD_LIBRARY_PATH" "\0"
	"LD_PRELOAD" "\0"
	"LD_TRACE_LOADED_OBJECTS" "\0"
	"LD_BIND_NOW" "\0"
	"LD_AOUT_LIBRARY_PATH" "\0"
	"LD_AOUT_PRELOAD" "\0"
	"LD_NOWARN" "\0"
	"LD_KEEPDIR" "\0";

int FAST_FUNC sanitize_env_if_suid(void)
{
	const char *p;

	if (getuid() == geteuid())
		return 0;

	p = forbid;
	do {
		unsetenv(p);
		p += strlen(p) + 1;
	} while (*p);
	putenv((char*)bb_PATH_root_path);

	return 1; /* we indeed were run by different user! */
}

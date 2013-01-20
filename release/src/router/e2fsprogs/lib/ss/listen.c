/*
 * Listener loop for subsystem library libss.a.
 *
 *	$Header$
 *	$Locker$
 *
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose is hereby granted, provided that
 * the names of M.I.T. and the M.I.T. S.I.P.B. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  M.I.T. and the
 * M.I.T. S.I.P.B. make no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "ss_internal.h"
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/param.h>

typedef void sigret_t;

static ss_data *current_info;
static jmp_buf listen_jmpb;
static sigret_t (*sig_cont)(int);

static sigret_t print_prompt(int sig __SS_ATTR((unused)))
{
    if (current_info->redisplay)
	    (*current_info->redisplay)();
    else {
	    (void) fputs(current_info->prompt, stdout);
	    (void) fflush(stdout);
    }
}

static sigret_t listen_int_handler(int sig __SS_ATTR((unused)))
{
    putc('\n', stdout);
    signal(SIGINT, listen_int_handler);
    longjmp(listen_jmpb, 1);
}

int ss_listen (int sci_idx)
{
    char *cp;
    ss_data *info;
    sigret_t (*sig_int)(int), (*old_sig_cont)(int);
    char input[BUFSIZ];
    sigset_t omask, igmask;
    int code;
    jmp_buf old_jmpb;
    ss_data *old_info = current_info;
    char *line;

    current_info = info = ss_info(sci_idx);
    sig_cont = (sigret_t (*)(int)) 0;
    info->abort = 0;
    sigemptyset(&igmask);
    sigaddset(&igmask, SIGINT);
    sigprocmask(SIG_BLOCK, &igmask, &omask);
    memcpy(old_jmpb, listen_jmpb, sizeof(jmp_buf));
    sig_int = signal(SIGINT, listen_int_handler);
    setjmp(listen_jmpb);
    sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);

    while(!info->abort) {
	old_sig_cont = sig_cont;
	sig_cont = signal(SIGCONT, print_prompt);
	if (sig_cont == print_prompt)
	    sig_cont = old_sig_cont;
	if (info->readline) {
		line = (*info->readline)(current_info->prompt);
	} else {
		print_prompt(0);
		if (fgets(input, BUFSIZ, stdin) == input)
			line = input;
		else
			line = NULL;

		input[BUFSIZ-1] = 0;
	}
	if (line == NULL) {
		code = SS_ET_EOF;
		(void) signal(SIGCONT, sig_cont);
		goto egress;
	}

	cp = strchr(line, '\n');
	if (cp) {
	    *cp = '\0';
	    if (cp == line)
		continue;
	}
	(void) signal(SIGCONT, sig_cont);
	if (info->add_history)
		(*info->add_history)(line);

	code = ss_execute_line (sci_idx, line);
	if (code == SS_ET_COMMAND_NOT_FOUND) {
	    register char *c = line;
	    while (*c == ' ' || *c == '\t')
		c++;
	    cp = strchr (c, ' ');
	    if (cp)
		*cp = '\0';
	    cp = strchr (c, '\t');
	    if (cp)
		*cp = '\0';
	    ss_error (sci_idx, 0,
		    "Unknown request \"%s\".  Type \"?\" for a request list.",
		       c);
	}
	if (info->readline)
		free(line);
    }
    code = 0;
egress:
    (void) signal(SIGINT, sig_int);
    memcpy(listen_jmpb, old_jmpb, sizeof(jmp_buf));
    current_info = old_info;
    return code;
}

void ss_abort_subsystem(int sci_idx, int code)
{
    ss_info(sci_idx)->abort = 1;
    ss_info(sci_idx)->exit_status = code;

}

void ss_quit(int argc __SS_ATTR((unused)),
	     const char * const *argv __SS_ATTR((unused)),
	     int sci_idx, pointer infop __SS_ATTR((unused)))
{
    ss_abort_subsystem(sci_idx, 0);
}

#ifdef HAVE_DLOPEN
#define get_request(tbl,idx)    ((tbl) -> requests + (idx))

static char *cmd_generator(const char *text, int state)
{
	static int	len;
	static ss_request_table **rqtbl;
	static int	curr_rqt;
	static char const * const * name;
	ss_request_entry *request;
	char		*ret;

	if (state == 0) {
		len = strlen(text);
		rqtbl = current_info->rqt_tables;
		if (!rqtbl || !*rqtbl)
			return 0;
		curr_rqt = 0;
		name = 0;
	}

	while (1) {
		if (!name || !*name) {
			request = get_request(*rqtbl, curr_rqt++);
			name = request->command_names;
			if (!name) {
				rqtbl++;
				if (*rqtbl) {
					curr_rqt = 0;
					continue;
				} else
					break;
			}
		}
		if (strncmp(*name, text, len) == 0) {
			ret = malloc(strlen(*name)+1);
			if (ret)
				strcpy(ret, *name);
			name++;
			return ret;
		}
		name++;
	}

	return 0;
}

char **ss_rl_completion(const char *text, int start,
			int end __SS_ATTR((unused)))
{
	if ((start == 0) && current_info->rl_completion_matches)
		return (*current_info->rl_completion_matches)
			(text, cmd_generator);
	return 0;
}
#endif


/*
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
#include "config.h"
#include "ss_internal.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/wait.h>

typedef void sigret_t;

void ss_list_requests(int argc __SS_ATTR((unused)),
		      const char * const *argv __SS_ATTR((unused)),
		      int sci_idx, void *infop __SS_ATTR((unused)))
{
    ss_request_entry *entry;
    char const * const *name;
    int i, spacing;
    ss_request_table **table;

    FILE *output;
    int fd;
    sigset_t omask, igmask;
    sigret_t (*func)(int);
#ifndef NO_FORK
    int waitb;
#endif

    sigemptyset(&igmask);
    sigaddset(&igmask, SIGINT);
    sigprocmask(SIG_BLOCK, &igmask, &omask);
    func = signal(SIGINT, SIG_IGN);
    fd = ss_pager_create();
    if (fd < 0) {
        perror("ss_pager_create");
        (void) signal(SIGINT, func);
        return;
    }
    output = fdopen(fd, "w");
    sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);

    fprintf (output, "Available %s requests:\n\n",
	     ss_info (sci_idx) -> subsystem_name);

    for (table = ss_info(sci_idx)->rqt_tables; *table; table++) {
        entry = (*table)->requests;
        for (; entry->command_names; entry++) {
            spacing = -2;
            if (entry->flags & SS_OPT_DONT_LIST)
                continue;
            for (name = entry->command_names; *name; name++) {
                int len = strlen(*name);
                fputs(*name, output);
                spacing += len + 2;
                if (name[1]) {
                    fputs(", ", output);
                }
            }
            if (spacing > 23) {
                fputc('\n', output);
                spacing = 0;
            }
            for (i = 0; i < 25 - spacing; i++)
                fputc(' ', output);
            fputs(entry->info_string, output);
            fputc('\n', output);
        }
    }
    fclose(output);
#ifndef NO_FORK
    wait(&waitb);
#endif
    (void) signal(SIGINT, func);
}

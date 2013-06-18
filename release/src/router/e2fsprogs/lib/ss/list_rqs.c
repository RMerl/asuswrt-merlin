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

static char const twentyfive_spaces[26] =
    "                         ";
static char const NL[2] = "\n";

void ss_list_requests(int argc __SS_ATTR((unused)),
		      const char * const *argv __SS_ATTR((unused)),
		      int sci_idx, void *infop __SS_ATTR((unused)))
{
    ss_request_entry *entry;
    char const * const *name;
    int spacing;
    ss_request_table **table;

    char buffer[BUFSIZ];
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
    output = fdopen(fd, "w");
    sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);

    fprintf (output, "Available %s requests:\n\n",
	     ss_info (sci_idx) -> subsystem_name);

    for (table = ss_info(sci_idx)->rqt_tables; *table; table++) {
        entry = (*table)->requests;
        for (; entry->command_names; entry++) {
            spacing = -2;
            buffer[0] = '\0';
            if (entry->flags & SS_OPT_DONT_LIST)
                continue;
            for (name = entry->command_names; *name; name++) {
                int len = strlen(*name);
                strncat(buffer, *name, len);
                spacing += len + 2;
                if (name[1]) {
                    strcat(buffer, ", ");
                }
            }
            if (spacing > 23) {
                strcat(buffer, NL);
                fputs(buffer, output);
                spacing = 0;
                buffer[0] = '\0';
            }
            strncat(buffer, twentyfive_spaces, 25-spacing);
            strcat(buffer, entry->info_string);
            strcat(buffer, NL);
            fputs(buffer, output);
        }
    }
    fclose(output);
#ifndef NO_FORK
    wait(&waitb);
#endif
    (void) signal(SIGINT, func);
}

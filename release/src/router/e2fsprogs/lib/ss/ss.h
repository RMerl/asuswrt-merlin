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
 *
 * This quote is just too good to not pass on:
 *
 * 	"BTW, I would have rejected the name Story Server because its
 * 	initials are SS, the name of the secret police in Nazi
 * 	Germany, probably the most despised pair of letters in western
 * 	culture."  --- http://scriptingnewsarchive.userland.com/1999/12/13
 *
 * Let no one say political correctness isn't dead....
 */

#ifndef _ss_h
#define _ss_h __FILE__

#include <ss/ss_err.h>

#define __SS_CONST const
#define __SS_PROTO (int, const char * const *, int, void *)

#ifdef __GNUC__
#define __SS_ATTR(x) __attribute__(x)
#else
#define __SS_ATTR(x)
#endif


typedef __SS_CONST struct _ss_request_entry {
    __SS_CONST char * __SS_CONST *command_names; /* whatever */
    void (* __SS_CONST function) __SS_PROTO; /* foo */
    __SS_CONST char * __SS_CONST info_string;	/* NULL */
    int flags;			/* 0 */
} ss_request_entry;

typedef __SS_CONST struct _ss_request_table {
    int version;
    ss_request_entry *requests;
} ss_request_table;

#define SS_RQT_TBL_V2	2

typedef struct _ss_rp_options {	/* DEFAULT VALUES */
    int version;		/* SS_RP_V1 */
    void (*unknown) __SS_PROTO;	/* call for unknown command */
    int allow_suspend;
    int catch_int;
} ss_rp_options;

#define SS_RP_V1 1

#define SS_OPT_DONT_LIST	0x0001
#define SS_OPT_DONT_SUMMARIZE	0x0002

void ss_help __SS_PROTO;
#if 0
char *ss_current_request();	/* This is actually a macro */
#endif

char *ss_name(int sci_idx);
void ss_error (int, long, char const *, ...)
	__SS_ATTR((format(printf, 3, 4)));
void ss_perror (int, long, char const *);

int ss_create_invocation(const char *, const char *, void *,
			 ss_request_table *, int *);
void ss_delete_invocation(int);
int ss_listen(int);
int ss_execute_line(int, char *);
void ss_add_request_table(int, ss_request_table *, int, int *);
void ss_delete_request_table(int, ss_request_table *, int *);
void ss_abort_subsystem(int sci_idx, int code);
void ss_quit(int argc, const char * const *argv, int sci_idx, void *infop);
void ss_self_identify(int argc, const char * const *argv, int sci_idx, void *infop);
void ss_subsystem_name(int argc, const char * const *argv,
		       int sci_idx, void *infop);
void ss_subsystem_version(int argc, const char * const *argv,
			  int sci_idx, void *infop);
void ss_unimplemented(int argc, const char * const *argv,
		      int sci_idx, void *infop);
void ss_set_prompt(int sci_idx, char *new_prompt);
char *ss_get_prompt(int sci_idx);
void ss_get_readline(int sci_idx);

extern ss_request_table ss_std_requests;
#endif /* _ss_h */

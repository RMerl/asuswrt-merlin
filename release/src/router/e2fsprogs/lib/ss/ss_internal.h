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

#ifndef _ss_ss_internal_h
#define _ss_ss_internal_h __FILE__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PROTOTYPE(p) p
typedef void * pointer;

#include "ss.h"

typedef char BOOL;

typedef struct _ss_abbrev_entry {
    char *name;			/* abbrev name */
    char **abbrev;		/* new tokens to insert */
    unsigned int beginning_of_line : 1;
} ss_abbrev_entry;

typedef struct _ss_abbrev_list {
    int n_abbrevs;
    ss_abbrev_entry *first_abbrev;
} ss_abbrev_list;

typedef struct {
/*    char *path; */
    ss_abbrev_list abbrevs[127];
} ss_abbrev_info;

typedef struct _ss_data {	/* init values */
    /* this subsystem */
    const char *subsystem_name;
    const char *subsystem_version;
    /* current request info */
    int argc;
    char **argv;		/* arg list */
    char const *current_request; /* primary name */
    /* info directory for 'help' */
    char **info_dirs;
    /* to be extracted by subroutines */
    pointer info_ptr;		/* (void *) NULL */
    /* for ss_listen processing */
    char *prompt;
    ss_request_table **rqt_tables;
    ss_abbrev_info *abbrev_info;
    struct {
	unsigned int  escape_disabled : 1,
		      abbrevs_disabled : 1;
    } flags;
    /*
     * Dynamic usage of readline library if present
     */
    void *readline_handle;
    void (*readline_shutdown)(struct _ss_data *info);
    char *(*readline)(const char *);
    void (*add_history)(const char *);
    void (*redisplay)(void);
    char **(*rl_completion_matches)(const char *,
				    char *(*completer)(const char *, int));
    /* to get out */
    int abort;			/* exit subsystem */
    int exit_status;
} ss_data;

#define CURRENT_SS_VERSION 1

#define	ss_info(sci_idx)	(_ss_table[sci_idx])
#define	ss_current_request(sci_idx,code_ptr)	\
     (*code_ptr=0,ss_info(sci_idx)->current_request)
void ss_add_info_dir (int sci_idx, char *info_dir, int *code_ptr);
void ss_delete_info_dir (int sci_idx, char *info_dir, int *code_ptr);
int ss_execute_line(int sci_idx, char *line_ptr);
char **ss_parse(int sci_idx, char *line_ptr, int *argc_ptr);
ss_abbrev_info *ss_abbrev_initialize(char *, int *);
void ss_page_stdin(void);
void ss_list_requests(int, char const * const *, int, pointer);
int ss_execute_command(int sci_idx, char *argv[]);
int ss_pager_create(void);
char *ss_safe_getenv(const char *arg);
char **ss_rl_completion(const char *text, int start, int end);

extern ss_data **_ss_table;
extern char *ss_et_msgs[];
extern char *_ss_pager_name;

#ifdef USE_SIGPROCMASK
/* fake sigmask, sigblock, sigsetmask */
#include <signal.h>
#define sigmask(x) (1L<<(x)-1)
#define sigsetmask(x) sigprocmask(SIG_SETMASK,&x,NULL)
static int _fake_sigstore;
#define sigblock(x) (_fake_sigstore=x,sigprocmask(SIG_BLOCK,&_fake_sigstore,0))
#endif
#endif /* _ss_internal_h */

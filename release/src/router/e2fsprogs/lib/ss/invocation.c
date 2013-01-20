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

#ifdef HAS_STDLIB_H
#include <stdlib.h>
#endif
#include "ss_internal.h"
#define	size	sizeof(ss_data *)
#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

int ss_create_invocation(subsystem_name, version_string, info_ptr,
			 request_table_ptr, code_ptr)
	const char *subsystem_name, *version_string;
	void *info_ptr;
	ss_request_table *request_table_ptr;
	int *code_ptr;
{
	register int sci_idx;
	register ss_data *new_table;
	register ss_data **table;

	*code_ptr = 0;
	table = _ss_table;
	new_table = (ss_data *) malloc(sizeof(ss_data));

	if (table == (ss_data **) NULL) {
		table = (ss_data **) malloc(2 * size);
		table[0] = table[1] = (ss_data *)NULL;
	}
	initialize_ss_error_table ();

	for (sci_idx = 1; table[sci_idx] != (ss_data *)NULL; sci_idx++)
		;
	table = (ss_data **) realloc((char *)table,
				     ((unsigned)sci_idx+2)*size);
	table[sci_idx+1] = (ss_data *) NULL;
	table[sci_idx] = new_table;

	new_table->subsystem_name = subsystem_name;
	new_table->subsystem_version = version_string;
	new_table->argv = (char **)NULL;
	new_table->current_request = (char *)NULL;
	new_table->info_dirs = (char **)malloc(sizeof(char *));
	*new_table->info_dirs = (char *)NULL;
	new_table->info_ptr = info_ptr;
	new_table->prompt = malloc((unsigned)strlen(subsystem_name)+4);
	strcpy(new_table->prompt, subsystem_name);
	strcat(new_table->prompt, ":  ");
#ifdef silly
	new_table->abbrev_info = ss_abbrev_initialize("/etc/passwd", code_ptr);
#else
	new_table->abbrev_info = NULL;
#endif
	new_table->flags.escape_disabled = 0;
	new_table->flags.abbrevs_disabled = 0;
	new_table->rqt_tables =
		(ss_request_table **) calloc(2, sizeof(ss_request_table *));
	*(new_table->rqt_tables) = request_table_ptr;
	*(new_table->rqt_tables+1) = (ss_request_table *) NULL;

	new_table->readline_handle = 0;
	new_table->readline_shutdown = 0;
	new_table->readline = 0;
	new_table->add_history = 0;
	new_table->redisplay = 0;
	new_table->rl_completion_matches = 0;
	_ss_table = table;
#if defined(HAVE_DLOPEN) && defined(SHARED_ELF_LIB)
	ss_get_readline(sci_idx);
#endif
	return(sci_idx);
}

void
ss_delete_invocation(sci_idx)
	int sci_idx;
{
	register ss_data *t;
	int ignored_code;

	t = ss_info(sci_idx);
	free(t->prompt);
	free(t->rqt_tables);
	while(t->info_dirs[0] != (char *)NULL)
		ss_delete_info_dir(sci_idx, t->info_dirs[0], &ignored_code);
	free(t->info_dirs);
#if defined(HAVE_DLOPEN) && defined(SHARED_ELF_LIB)
	if (t->readline_shutdown)
		(*t->readline_shutdown)(t);
#endif
	free(t);
}

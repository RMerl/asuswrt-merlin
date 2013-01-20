/*
 * Copyright 2003 by MIT Student Information Processing Board
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

static void ss_release_readline(ss_data *info)
{
#ifdef HAVE_DLOPEN
	if (!info->readline_handle)
		return;

	info->readline = 0;
	info->add_history = 0;
	info->redisplay = 0;
	info->rl_completion_matches = 0;
	dlclose(info->readline_handle);
	info->readline_handle = 0;
#endif
}

/* Libraries we will try to use for readline/editline functionality */
#define DEFAULT_LIBPATH "libreadline.so.6:libreadline.so.5:libreadline.so.4:libreadline.so:libedit.so.2:libedit.so:libeditline.so.0:libeditline.so"

void ss_get_readline(int sci_idx)
{
#ifdef HAVE_DLOPEN
	void	*handle = NULL;
	ss_data *info = ss_info(sci_idx);
	const char **t, *libpath = 0;
	char	*tmp, *cp, *next;
	char **(**completion_func)(const char *, int, int);

	if (info->readline_handle)
		return;

	libpath = ss_safe_getenv("SS_READLINE_PATH");
	if (!libpath)
		libpath = DEFAULT_LIBPATH;
	if (*libpath == 0 || !strcmp(libpath, "none"))
		return;

	tmp = malloc(strlen(libpath)+1);
	if (!tmp)
		return;
	strcpy(tmp, libpath);
	for (cp = tmp; cp; cp = next) {
		next = strchr(cp, ':');
		if (next)
			*next++ = 0;
		if (*cp == 0)
			continue;
		if ((handle = dlopen(cp, RTLD_NOW))) {
			/* printf("Using %s for readline library\n", cp); */
			break;
		}
	}
	free(tmp);
	if (!handle)
		return;

	info->readline_handle = handle;
	info->readline = (char *(*)(const char *))
		dlsym(handle, "readline");
	info->add_history = (void (*)(const char *))
		dlsym(handle, "add_history");
	info->redisplay = (void (*)(void))
		dlsym(handle, "rl_forced_update_display");
	info->rl_completion_matches = (char **(*)(const char *,
				    char *(*)(const char *, int)))
		dlsym(handle, "rl_completion_matches");
	if ((t = dlsym(handle, "rl_readline_name")) != NULL)
		*t = info->subsystem_name;
	if ((completion_func =
	     dlsym(handle, "rl_attempted_completion_function")) != NULL)
		*completion_func = ss_rl_completion;
	info->readline_shutdown = ss_release_readline;
#endif
}



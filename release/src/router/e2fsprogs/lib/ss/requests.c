/*
 * Various minor routines...
 *
 * Copyright 1987, 1988, 1989 by MIT
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

#include <stdio.h>
#include "ss_internal.h"

#define	DECLARE(name) void name(int argc,const char * const *argv, \
				int sci_idx, void *infop)

/*
 * ss_self_identify -- assigned by default to the "." request
 */
void ss_self_identify(int argc __SS_ATTR((unused)),
		      const char * const *argv __SS_ATTR((unused)),
		      int sci_idx, void *infop __SS_ATTR((unused)))
{
     register ss_data *info = ss_info(sci_idx);
     printf("%s version %s\n", info->subsystem_name,
	    info->subsystem_version);
}

/*
 * ss_subsystem_name -- print name of subsystem
 */
void ss_subsystem_name(int argc __SS_ATTR((unused)),
		       const char * const *argv __SS_ATTR((unused)),
		       int sci_idx,
		       void *infop __SS_ATTR((unused)))
{
     printf("%s\n", ss_info(sci_idx)->subsystem_name);
}

/*
 * ss_subsystem_version -- print version of subsystem
 */
void ss_subsystem_version(int argc __SS_ATTR((unused)),
			  const char * const *argv __SS_ATTR((unused)),
			  int sci_idx,
			  void *infop __SS_ATTR((unused)))
{
     printf("%s\n", ss_info(sci_idx)->subsystem_version);
}

/*
 * ss_unimplemented -- routine not implemented (should be
 * set up as (dont_list,dont_summarize))
 */
void ss_unimplemented(int argc __SS_ATTR((unused)),
		      const char * const *argv __SS_ATTR((unused)),
		      int sci_idx, void *infop __SS_ATTR((unused)))
{
     ss_perror(sci_idx, SS_ET_UNIMPLEMENTED, "");
}

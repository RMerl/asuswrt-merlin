/*
 * profile.h
 *
 * Copyright (C) 2005 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * Copyright (C) 1985-2005 by the Massachusetts Institute of Technology.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original MIT software.
 * M.I.T. makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _PROFILE_H
#define _PROFILE_H

typedef struct _profile_t *profile_t;

typedef void (*profile_syntax_err_cb_t)(const char *file, long err,
					int line_num);

/*
 * Used by the profile iterator in prof_get.c
 */
#define PROFILE_ITER_LIST_SECTION	0x0001
#define PROFILE_ITER_SECTIONS_ONLY	0x0002
#define PROFILE_ITER_RELATIONS_ONLY	0x0004

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

long profile_init
	(const char * *files, profile_t *ret_profile);

void profile_release
	(profile_t profile);

long profile_set_default
	(profile_t profile, const char *def_string);

long profile_get_string
	(profile_t profile, const char *name, const char *subname,
			const char *subsubname, const char *def_val,
			char **ret_string);
long profile_get_integer
	(profile_t profile, const char *name, const char *subname,
			const char *subsubname, int def_val,
			int *ret_default);

long profile_get_uint
	(profile_t profile, const char *name, const char *subname,
		const char *subsubname, unsigned int def_val,
		unsigned int *ret_int);

long profile_get_double
	(profile_t profile, const char *name, const char *subname,
		const char *subsubname, double def_val,
		double *ret_float);

long profile_get_boolean
	(profile_t profile, const char *name, const char *subname,
			const char *subsubname, int def_val,
			int *ret_default);

long profile_iterator_create
	(profile_t profile, const char *const *names,
		   int flags, void **ret_iter);

void profile_iterator_free
	(void **iter_p);

long profile_iterator
	(void	**iter_p, char **ret_name, char **ret_value);

profile_syntax_err_cb_t profile_set_syntax_err_cb(profile_syntax_err_cb_t hook);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KRB5_H */

/*
 * mke2fs.h
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
 * 	2003, 2004, 2005 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/* mke2fs.c */
extern const char * program_name;
extern int	quiet;
extern int	verbose;
extern int	zero_hugefile;
extern char **fs_types;

extern char *get_string_from_profile(char **types, const char *opt,
				     const char *def_val);
extern int get_int_from_profile(char **types, const char *opt, int def_val);
extern int get_bool_from_profile(char **types, const char *opt, int def_val);
extern int int_log10(unsigned long long arg);

/* mk_hugefiles.c */
extern errcode_t mk_hugefiles(ext2_filsys fs, const char *device_name);




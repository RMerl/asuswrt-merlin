/*
 * util.h --- header file defining prototypes for helper functions
 * used by tune2fs and mke2fs
 *
 * Copyright 2000 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

extern int	 journal_size;
extern int	 journal_flags;
extern char	*journal_device;

#ifndef HAVE_STRCASECMP
extern int strcasecmp (char *s1, char *s2);
#endif
extern char *get_progname(char *argv_zero);
extern void proceed_question(void);
extern void check_plausibility(const char *device);
extern void parse_journal_opts(const char *opts);
extern void check_mount(const char *device, int force, const char *type);
extern unsigned int figure_journal_size(int size, ext2_filsys fs);
extern void print_check_message(int, unsigned int);
extern void dump_mmp_msg(struct mmp_struct *mmp, const char *msg);

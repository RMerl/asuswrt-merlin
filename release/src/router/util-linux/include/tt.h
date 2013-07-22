/*
 * Prints table or tree. See lib/table.c for more details and example.
 *
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef UTIL_LINUX_TT_H
#define UTIL_LINUX_TT_H

#include "list.h"

enum {
	/*
	 * Global flags
	 */
	TT_FL_RAW         = (1 << 1),
	TT_FL_ASCII       = (1 << 2),
	TT_FL_NOHEADINGS  = (1 << 3),
	TT_FL_EXPORT      = (1 << 4),

	/*
	 * Column flags
	 */
	TT_FL_TRUNC       = (1 << 5),
	TT_FL_TREE        = (1 << 6),
	TT_FL_RIGHT	  = (1 << 7),
	TT_FL_STRICTWIDTH = (1 << 8)
};

struct tt {
	size_t	ncols;		/* number of columns */
	size_t	termwidth;	/* terminal width */
	int	flags;
	int	first_run;

	struct list_head	tb_columns;
	struct list_head	tb_lines;

	const struct tt_symbols	*symbols;
};

struct tt_column {
	const char *name;	/* header */
	size_t	seqnum;

	size_t	width;		/* real column width */
	size_t	width_min;	/* minimal width (width of header) */
	double	width_hint;	/* hint (N < 1 is in percent of termwidth) */

	int	flags;

	struct list_head	cl_columns;
};

struct tt_line {
	struct tt	*table;
	char const	**data;
	void		*userdata;
	size_t		data_sz;		/* strlen of all data */

	struct list_head	ln_lines;	/* table lines */

	struct list_head	ln_branch;	/* begin of branch (head of ln_children) */
	struct list_head	ln_children;

	struct tt_line	*parent;
};

extern struct tt *tt_new_table(int flags);
extern void tt_free_table(struct tt *tb);
extern void tt_remove_lines(struct tt *tb);
extern int tt_print_table(struct tt *tb);

extern struct tt_column *tt_define_column(struct tt *tb, const char *name,
						double whint, int flags);

extern struct tt_column *tt_get_column(struct tt *tb, size_t colnum);

extern struct tt_line *tt_add_line(struct tt *tb, struct tt_line *parent);

extern int tt_line_set_data(struct tt_line *ln, int colnum, const char *data);
extern int tt_line_set_userdata(struct tt_line *ln, void *data);

extern int tt_parse_columns_list(const char *list, int ary[], size_t arrsz,
				int (name2id)(const char *, size_t));

#endif /* UTIL_LINUX_TT_H */

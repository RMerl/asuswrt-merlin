/*
 * TT - Table or Tree, features:
 * - column width could be defined as absolute or relative to the terminal width
 * - allows to truncate or wrap data in columns
 * - prints tree if parent->child relation is defined
 * - draws the tree by ASCII or UTF8 lines (depends on terminal setting)
 *
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "nls.h"
#include "widechar.h"
#include "c.h"
#include "tt.h"
#include "mbsalign.h"

struct tt_symbols {
	const char *branch;
	const char *vert;
	const char *right;
};

static const struct tt_symbols ascii_tt_symbols = {
	.branch = "|-",
	.vert	= "| ",
	.right	= "`-",
};

#ifdef HAVE_WIDECHAR
#define	mbs_width(_s)	mbstowcs(NULL, _s, 0)

#define UTF_V	"\342\224\202"	/* U+2502, Vertical line drawing char */
#define UTF_VR	"\342\224\234"	/* U+251C, Vertical and right */
#define UTF_H	"\342\224\200"	/* U+2500, Horizontal */
#define UTF_UR	"\342\224\224"	/* U+2514, Up and right */

static const struct tt_symbols utf8_tt_symbols = {
	.branch = UTF_VR UTF_H,
	.vert   = UTF_V " ",
	.right	= UTF_UR UTF_H,
};

#else /* !HAVE_WIDECHAR */
# define mbs_width(_s)       strlen(_s)
#endif /* !HAVE_WIDECHAR */

#define is_last_column(_tb, _cl) \
		list_last_entry(&(_cl)->cl_columns, &(_tb)->tb_columns)

/*
 * @flags: TT_FL_* flags (usually TT_FL_{ASCII,RAW})
 *
 * Returns: newly allocated table
 */
struct tt *tt_new_table(int flags)
{
	struct tt *tb;

	tb = calloc(1, sizeof(struct tt));
	if (!tb)
		return NULL;

	tb->flags = flags;
	INIT_LIST_HEAD(&tb->tb_lines);
	INIT_LIST_HEAD(&tb->tb_columns);

#if defined(HAVE_WIDECHAR)
	if (!(flags & TT_FL_ASCII) && !strcmp(nl_langinfo(CODESET), "UTF-8"))
		tb->symbols = &utf8_tt_symbols;
	else
#endif
		tb->symbols = &ascii_tt_symbols;

	tb->first_run = TRUE;
	return tb;
}

void tt_remove_lines(struct tt *tb)
{
	if (!tb)
		return;

	while (!list_empty(&tb->tb_lines)) {
		struct tt_line *ln = list_entry(tb->tb_lines.next,
						struct tt_line, ln_lines);
		list_del(&ln->ln_lines);
		free(ln->data);
		free(ln);
	}
}

void tt_free_table(struct tt *tb)
{
	if (!tb)
		return;

	tt_remove_lines(tb);

	while (!list_empty(&tb->tb_columns)) {
		struct tt_column *cl = list_entry(tb->tb_columns.next,
						struct tt_column, cl_columns);
		list_del(&cl->cl_columns);
		free(cl);
	}
	free(tb);
}


/*
 * @tb: table
 * @name: column header
 * @whint: column width hint (absolute width: N > 1; relative width: N < 1)
 * @flags: usually TT_FL_{TREE,TRUNCATE}
 *
 * The column width is possible to define by three ways:
 *
 *  @whint = 0..1    : relative width, percent of terminal width
 *
 *  @whint = 1..N    : absolute width, empty colum will be truncated to
 *                     the column header width
 *
 *  @whint = 1..N
 *  @flags = TT_FL_STRICTWIDTH
 *                   : absolute width, empty colum won't be truncated
 *
 * The column is necessary to address (for example for tt_line_set_data()) by
 * sequential number. The first defined column has the colnum = 0. For example:
 *
 *	tt_define_column(tab, "FOO", 0.5, 0);		// colnum = 0
 *	tt_define_column(tab, "BAR", 0.5, 0);		// colnum = 1
 *      .
 *      .
 *	tt_line_set_data(line, 0, "foo-data");		// FOO column
 *	tt_line_set_data(line, 1, "bar-data");		// BAR column
 *
 * Returns: newly allocated column definition
 */
struct tt_column *tt_define_column(struct tt *tb, const char *name,
					double whint, int flags)
{
	struct tt_column *cl;

	if (!tb)
		return NULL;
	cl = calloc(1, sizeof(*cl));
	if (!cl)
		return NULL;

	cl->name = name;
	cl->width_hint = whint;
	cl->flags = flags;
	cl->seqnum = tb->ncols++;

	if (flags & TT_FL_TREE)
		tb->flags |= TT_FL_TREE;

	INIT_LIST_HEAD(&cl->cl_columns);
	list_add_tail(&cl->cl_columns, &tb->tb_columns);
	return cl;
}

/*
 * @tb: table
 * @parent: parental line or NULL
 *
 * Returns: newly allocate line
 */
struct tt_line *tt_add_line(struct tt *tb, struct tt_line *parent)
{
	struct tt_line *ln = NULL;

	if (!tb || !tb->ncols)
		goto err;
	ln = calloc(1, sizeof(*ln));
	if (!ln)
		goto err;
	ln->data = calloc(tb->ncols, sizeof(char *));
	if (!ln->data)
		goto err;

	ln->table = tb;
	ln->parent = parent;
	INIT_LIST_HEAD(&ln->ln_lines);
	INIT_LIST_HEAD(&ln->ln_children);
	INIT_LIST_HEAD(&ln->ln_branch);

	list_add_tail(&ln->ln_lines, &tb->tb_lines);

	if (parent)
		list_add_tail(&ln->ln_children, &parent->ln_branch);
	return ln;
err:
	free(ln);
	return NULL;
}

/*
 * @tb: table
 * @colnum: number of column (0..N)
 *
 * Returns: pointer to column or NULL
 */
struct tt_column *tt_get_column(struct tt *tb, size_t colnum)
{
	struct list_head *p;

	list_for_each(p, &tb->tb_columns) {
		struct tt_column *cl =
				list_entry(p, struct tt_column, cl_columns);
		if (cl->seqnum == colnum)
			return cl;
	}
	return NULL;
}

/*
 * @ln: line
 * @colnum: number of column (0..N)
 * @data: printable data
 *
 * Stores data that will be printed to the table cell.
 */
int tt_line_set_data(struct tt_line *ln, int colnum, const char *data)
{
	struct tt_column *cl;

	if (!ln)
		return -1;
	cl = tt_get_column(ln->table, colnum);
	if (!cl)
		return -1;

	if (ln->data[cl->seqnum]) {
		size_t sz = strlen(ln->data[cl->seqnum]);;
		ln->data_sz = ln->data_sz > sz ? ln->data_sz - sz : 0;
	}

	ln->data[cl->seqnum] = data;
	if (data)
		ln->data_sz += strlen(data);
	return 0;
}

static int get_terminal_width(void)
{
#ifdef TIOCGSIZE
	struct ttysize	t_win;
#endif
#ifdef TIOCGWINSZ
	struct winsize	w_win;
#endif
        const char	*cp;

#ifdef TIOCGSIZE
	if (ioctl (0, TIOCGSIZE, &t_win) == 0)
		return t_win.ts_cols;
#endif
#ifdef TIOCGWINSZ
	if (ioctl (0, TIOCGWINSZ, &w_win) == 0)
		return w_win.ws_col;
#endif
        cp = getenv("COLUMNS");
	if (cp)
		return strtol(cp, NULL, 10);
	return 0;
}

int tt_line_set_userdata(struct tt_line *ln, void *data)
{
	if (!ln)
		return -1;
	ln->userdata = data;
	return 0;
}

static char *line_get_ascii_art(struct tt_line *ln, char *buf, size_t *bufsz)
{
	const char *art;
	size_t len;

	if (!ln->parent)
		return buf;

	buf = line_get_ascii_art(ln->parent, buf, bufsz);
	if (!buf)
		return NULL;

	if (list_last_entry(&ln->ln_children, &ln->parent->ln_branch))
		art = "  ";
	else
		art = ln->table->symbols->vert;

	len = strlen(art);
	if (*bufsz < len)
		return NULL;	/* no space, internal error */

	memcpy(buf, art, len);
	*bufsz -= len;
	return buf + len;
}

static char *line_get_data(struct tt_line *ln, struct tt_column *cl,
				char *buf, size_t bufsz)
{
	const char *data = ln->data[cl->seqnum];
	const struct tt_symbols *sym;
	char *p = buf;

	memset(buf, 0, bufsz);

	if (!data)
		return NULL;
	if (!(cl->flags & TT_FL_TREE)) {
		strncpy(buf, data, bufsz);
		buf[bufsz - 1] = '\0';
		return buf;
	}

	/*
	 * Tree stuff
	 */
	if (ln->parent) {
		p = line_get_ascii_art(ln->parent, buf, &bufsz);
		if (!p)
			return NULL;
	}

	sym = ln->table->symbols;

	if (!ln->parent)
		snprintf(p, bufsz, "%s", data);			/* root node */
	else if (list_last_entry(&ln->ln_children, &ln->parent->ln_branch))
		snprintf(p, bufsz, "%s%s", sym->right, data);	/* last chaild */
	else
		snprintf(p, bufsz, "%s%s", sym->branch, data);	/* any child */

	return buf;
}

/*
 * This function counts column width.
 *
 * For the TT_FL_NOEXTREMES columns is possible to call this function two
 * times.  The first pass counts width and average width. If the column
 * contains too large fields (width greater than 2 * average) then the column
 * is marked as "extreme". In the second pass all extreme fields are ignored
 * and column width is counted from non-extreme fields only.
 */
static void count_column_width(struct tt *tb, struct tt_column *cl,
				 char *buf, size_t bufsz)
{
	struct list_head *lp;
	int count = 0;
	size_t sum = 0;

	cl->width = 0;

	list_for_each(lp, &tb->tb_lines) {
		struct tt_line *ln = list_entry(lp, struct tt_line, ln_lines);
		char *data = line_get_data(ln, cl, buf, bufsz);
		size_t len = data ? mbs_width(data) : 0;

		if (len > cl->width_max)
			cl->width_max = len;

		if (cl->is_extreme && len > cl->width_avg * 2)
			continue;
		else if (cl->flags & TT_FL_NOEXTREMES) {
			sum += len;
			count++;
		}
		if (len > cl->width)
			cl->width = len;
	}

	if (count && cl->width_avg == 0) {
		cl->width_avg = sum / count;

		if (cl->width_max > cl->width_avg * 2)
			cl->is_extreme = 1;
	}

	/* check and set minimal column width */
	if (cl->name)
		cl->width_min = mbs_width(cl->name);

	/* enlarge to minimal width */
	if (cl->width < cl->width_min && !(cl->flags & TT_FL_STRICTWIDTH))
		cl->width = cl->width_min;

	/* use relative size for large columns */
	else if (cl->width_hint >= 1 && cl->width < (size_t) cl->width_hint &&
		 cl->width_min < (size_t) cl->width_hint)

		cl->width = (size_t) cl->width_hint;
}

/*
 * This is core of the tt_* voodo...
 */
static void recount_widths(struct tt *tb, char *buf, size_t bufsz)
{
	struct list_head *p;
	size_t width = 0;	/* output width */
	int trunc_only;
	int extremes = 0;

	/* set basic columns width
	 */
	list_for_each(p, &tb->tb_columns) {
		struct tt_column *cl =
				list_entry(p, struct tt_column, cl_columns);

		count_column_width(tb, cl, buf, bufsz);
		width += cl->width + (is_last_column(tb, cl) ? 0 : 1);
		extremes += cl->is_extreme;
	}

	/* reduce columns with extreme fields
	 */
	if (width > tb->termwidth && extremes) {
		list_for_each(p, &tb->tb_columns) {
			struct tt_column *cl = list_entry(p, struct tt_column, cl_columns);
			size_t org_width;

			if (!cl->is_extreme)
				continue;

			org_width = cl->width;
			count_column_width(tb, cl, buf, bufsz);

			if (org_width > cl->width)
				width -= org_width - cl->width;
			else
				extremes--;	/* hmm... nothing reduced */
		}
	}

	if (width < tb->termwidth) {
		/* cool, use the extra space for the extreme columns or/and last column
		 */
		if (extremes) {
			/* enlarge the first extreme column */
			list_for_each(p, &tb->tb_columns) {
				struct tt_column *cl =
					list_entry(p, struct tt_column, cl_columns);
				size_t add;

				if (!cl->is_extreme)
					continue;
				add = tb->termwidth - width;
				if (add && cl->width + add > cl->width_max)
					add = cl->width_max - cl->width;

				cl->width += add;
				width += add;

				if (width == tb->termwidth)
					break;
			}
		}
		if (width < tb->termwidth) {
			/* enalarge the last column */
			struct tt_column *cl = list_entry(
				tb->tb_columns.prev, struct tt_column, cl_columns);

			if (!(cl->flags & TT_FL_RIGHT) && tb->termwidth - width > 0) {
				cl->width += tb->termwidth - width;
				width = tb->termwidth;
			}
		}
	}

	/* bad, we have to reduce output width, this is done in two steps:
	 * 1/ reduce columns with a relative width and with truncate flag
	 * 2) reduce columns with a relative width without truncate flag
	 */
	trunc_only = 1;
	while (width > tb->termwidth) {
		size_t org = width;

		list_for_each(p, &tb->tb_columns) {
			struct tt_column *cl =
				list_entry(p, struct tt_column, cl_columns);

			if (width <= tb->termwidth)
				break;
			if (cl->width_hint > 1 && !(cl->flags & TT_FL_TRUNC))
				continue;	/* never truncate columns with absolute sizes */
			if (cl->flags & TT_FL_TREE)
				continue;	/* never truncate the tree */
			if (trunc_only && !(cl->flags & TT_FL_TRUNC))
				continue;
			if (cl->width == cl->width_min)
				continue;

			/* truncate column with relative sizes */
			if (cl->width_hint < 1 && cl->width > 0 && width > 0 &&
			    cl->width > cl->width_hint * tb->termwidth) {
				cl->width--;
				width--;
			}
			/* truncate column with absolute size */
			if (cl->width_hint > 1 && cl->width > 0 && width > 0 &&
			    !trunc_only) {
				cl->width--;
				width--;
			}

		}
		if (org == width) {
			if (trunc_only)
				trunc_only = 0;
			else
				break;
		}
	}

/*
	fprintf(stderr, "terminal: %d, output: %d\n", tb->termwidth, width);

	list_for_each(p, &tb->tb_columns) {
		struct tt_column *cl =
			list_entry(p, struct tt_column, cl_columns);

		fprintf(stderr, "width: %s=%zd [hint=%d, avg=%zd, max=%zd, extreme=%s]\n",
			cl->name, cl->width,
			cl->width_hint > 1 ? (int) cl->width_hint :
					     (int) (cl->width_hint * tb->termwidth),
			cl->width_avg,
			cl->width_max,
			cl->is_extreme ? "yes" : "not");
	}
*/
	return;
}

/* note that this function modifies @data
 */
static void print_data(struct tt *tb, struct tt_column *cl, char *data)
{
	size_t len, i, width;

	if (!data)
		data = "";

	/* raw mode */
	if (tb->flags & TT_FL_RAW) {
		fputs(data, stdout);
		if (!is_last_column(tb, cl))
			fputc(' ', stdout);
		return;
	}

	/* NAME=value mode */
	if (tb->flags & TT_FL_EXPORT) {
		fprintf(stdout, "%s=\"%s\"", cl->name, data);
		if (!is_last_column(tb, cl))
			fputc(' ', stdout);
		return;
	}

	/* note that 'len' and 'width' are number of cells, not bytes */
	len = mbs_width(data);

	if (!len || len == (size_t) -1) {
		len = 0;
		data = NULL;
	}
	width = cl->width;

	if (is_last_column(tb, cl) && len < width)
		width = len;

	/* truncate data */
	if (len > width && (cl->flags & TT_FL_TRUNC)) {
		if (data)
			len = mbs_truncate(data, &width);
		if (!data || len == (size_t) -1) {
			len = 0;
			data = NULL;
		}
	}
	if (data) {
		if (!(tb->flags & TT_FL_RAW) && (cl->flags & TT_FL_RIGHT)) {
			size_t xw = cl->width;
			fprintf(stdout, "%*s", (int) xw, data);
			if (len < xw)
				len = xw;
		}
		else
			fputs(data, stdout);
	}
	for (i = len; i < width; i++)
		fputc(' ', stdout);		/* padding */

	if (!is_last_column(tb, cl)) {
		if (len > width && !(cl->flags & TT_FL_TRUNC)) {
			fputc('\n', stdout);
			for (i = 0; i <= (size_t) cl->seqnum; i++) {
				struct tt_column *x = tt_get_column(tb, i);
				printf("%*s ", -((int)x->width), " ");
			}
		} else
			fputc(' ', stdout);	/* columns separator */
	}
}

static void print_line(struct tt_line *ln, char *buf, size_t bufsz)
{
	struct list_head *p;

	/* set width according to the size of data
	 */
	list_for_each(p, &ln->table->tb_columns) {
		struct tt_column *cl =
				list_entry(p, struct tt_column, cl_columns);

		print_data(ln->table, cl, line_get_data(ln, cl, buf, bufsz));
	}
	fputc('\n', stdout);
}

static void print_header(struct tt *tb, char *buf, size_t bufsz)
{
	struct list_head *p;

	if (!tb->first_run ||
	    (tb->flags & TT_FL_NOHEADINGS) ||
	    (tb->flags & TT_FL_EXPORT) ||
	    list_empty(&tb->tb_lines))
		return;

	/* set width according to the size of data
	 */
	list_for_each(p, &tb->tb_columns) {
		struct tt_column *cl =
				list_entry(p, struct tt_column, cl_columns);

		strncpy(buf, cl->name, bufsz);
		buf[bufsz - 1] = '\0';
		print_data(tb, cl, buf);
	}
	fputc('\n', stdout);
}

static void print_table(struct tt *tb, char *buf, size_t bufsz)
{
	struct list_head *p;

	print_header(tb, buf, bufsz);

	list_for_each(p, &tb->tb_lines) {
		struct tt_line *ln = list_entry(p, struct tt_line, ln_lines);

		print_line(ln, buf, bufsz);
	}
}

static void print_tree_line(struct tt_line *ln, char *buf, size_t bufsz)
{
	struct list_head *p;

	print_line(ln, buf, bufsz);

	if (list_empty(&ln->ln_branch))
		return;

	/* print all children */
	list_for_each(p, &ln->ln_branch) {
		struct tt_line *chld =
				list_entry(p, struct tt_line, ln_children);
		print_tree_line(chld, buf, bufsz);
	}
}

static void print_tree(struct tt *tb, char *buf, size_t bufsz)
{
	struct list_head *p;

	print_header(tb, buf, bufsz);

	list_for_each(p, &tb->tb_lines) {
		struct tt_line *ln = list_entry(p, struct tt_line, ln_lines);

		if (ln->parent)
			continue;

		print_tree_line(ln, buf, bufsz);
	}
}

/*
 * @tb: table
 *
 * Prints the table to stdout
 */
int tt_print_table(struct tt *tb)
{
	char *line;
	size_t line_sz;
	struct list_head *p;

	if (!tb)
		return -1;

	if (tb->first_run && !tb->termwidth) {
		tb->termwidth = get_terminal_width();
		if (tb->termwidth <= 0)
			tb->termwidth = 80;
	}

	line_sz = tb->termwidth;

	list_for_each(p, &tb->tb_lines) {
		struct tt_line *ln = list_entry(p, struct tt_line, ln_lines);
		if (ln->data_sz > line_sz)
			line_sz = ln->data_sz;
	}

	line_sz++;			/* make a space for \0 */
	line = malloc(line_sz);
	if (!line)
		return -1;

	if (tb->first_run &&
	    !((tb->flags & TT_FL_RAW) || (tb->flags & TT_FL_EXPORT)))
		recount_widths(tb, line, line_sz);

	if (tb->flags & TT_FL_TREE)
		print_tree(tb, line, line_sz);
	else
		print_table(tb, line, line_sz);

	free(line);

	tb->first_run = FALSE;
	return 0;
}

#ifdef TEST_PROGRAM
#include <errno.h>

enum { MYCOL_NAME, MYCOL_FOO, MYCOL_BAR, MYCOL_PATH };

int main(int argc, char *argv[])
{
	struct tt *tb;
	struct tt_line *ln, *pr, *root;
	int flags = 0, notree = 0, i;

	if (argc == 2 && !strcmp(argv[1], "--help")) {
		printf("%s [--ascii | --raw | --list]\n",
				program_invocation_short_name);
		return EXIT_SUCCESS;
	} else if (argc == 2 && !strcmp(argv[1], "--ascii")) {
		flags |= TT_FL_ASCII;
	} else if (argc == 2 && !strcmp(argv[1], "--raw")) {
		flags |= TT_FL_RAW;
		notree = 1;
	} else if (argc == 2 && !strcmp(argv[1], "--export")) {
		flags |= TT_FL_EXPORT;
		notree = 1;
	} else if (argc == 2 && !strcmp(argv[1], "--list"))
		notree = 1;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	tb = tt_new_table(flags);
	if (!tb)
		err(EXIT_FAILURE, "table initialization failed");

	tt_define_column(tb, "NAME", 0.3, notree ? 0 : TT_FL_TREE);
	tt_define_column(tb, "FOO", 0.3, TT_FL_TRUNC);
	tt_define_column(tb, "BAR", 0.3, 0);
	tt_define_column(tb, "PATH", 0.3, 0);

	for (i = 0; i < 2; i++) {
		root = ln = tt_add_line(tb, NULL);
		tt_line_set_data(ln, MYCOL_NAME, "AAA");
		tt_line_set_data(ln, MYCOL_FOO, "a-foo-foo");
		tt_line_set_data(ln, MYCOL_BAR, "barBar-A");
		tt_line_set_data(ln, MYCOL_PATH, "/mnt/AAA");

		pr = ln = tt_add_line(tb, ln);
		tt_line_set_data(ln, MYCOL_NAME, "AAA.A");
		tt_line_set_data(ln, MYCOL_FOO, "a.a-foo-foo");
		tt_line_set_data(ln, MYCOL_BAR, "barBar-A.A");
		tt_line_set_data(ln, MYCOL_PATH, "/mnt/AAA/A");

		ln = tt_add_line(tb, pr);
		tt_line_set_data(ln, MYCOL_NAME, "AAA.A.AAA");
		tt_line_set_data(ln, MYCOL_FOO, "a.a.a-foo-foo");
		tt_line_set_data(ln, MYCOL_BAR, "barBar-A.A.A");
		tt_line_set_data(ln, MYCOL_PATH, "/mnt/AAA/A/AAA");

		ln = tt_add_line(tb, root);
		tt_line_set_data(ln, MYCOL_NAME, "AAA.B");
		tt_line_set_data(ln, MYCOL_FOO, "a.b-foo-foo");
		tt_line_set_data(ln, MYCOL_BAR, "barBar-A.B");
		tt_line_set_data(ln, MYCOL_PATH, "/mnt/AAA/B");

		ln = tt_add_line(tb, pr);
		tt_line_set_data(ln, MYCOL_NAME, "AAA.A.BBB");
		tt_line_set_data(ln, MYCOL_FOO, "a.a.b-foo-foo");
		tt_line_set_data(ln, MYCOL_BAR, "barBar-A.A.BBB");
		tt_line_set_data(ln, MYCOL_PATH, "/mnt/AAA/A/BBB");

		ln = tt_add_line(tb, pr);
		tt_line_set_data(ln, MYCOL_NAME, "AAA.A.CCC");
		tt_line_set_data(ln, MYCOL_FOO, "a.a.c-foo-foo");
		tt_line_set_data(ln, MYCOL_BAR, "barBar-A.A.CCC");
		tt_line_set_data(ln, MYCOL_PATH, "/mnt/AAA/A/CCC");

		ln = tt_add_line(tb, root);
		tt_line_set_data(ln, MYCOL_NAME, "AAA.C");
		tt_line_set_data(ln, MYCOL_FOO, "a.c-foo-foo");
		tt_line_set_data(ln, MYCOL_BAR, "barBar-A.C");
		tt_line_set_data(ln, MYCOL_PATH, "/mnt/AAA/C");
	}

	tt_print_table(tb);
	tt_free_table(tb);

	return EXIT_SUCCESS;
}
#endif

/* 
 * slabtop.c - utility to display kernel slab information.
 *
 * Chris Rivera <cmrivera@ufl.edu>
 * Robert Love <rml@tech9.net>
 *
 * This program is licensed under the GNU Library General Public License, v2
 *
 * Copyright (C) 2003 Chris Rivera
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ncurses.h>
#include <termios.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "proc/slab.h"
#include "proc/version.h"

#define DEF_SORT_FUNC		sort_nr_objs
#define SLAB_STAT_ZERO		{ nr_objs: 0 }

static unsigned short cols, rows;
static struct termios saved_tty;
static long delay = 3;
static int (*sort_func)(const struct slab_info *, const struct slab_info *);

static struct slab_info *merge_objs(struct slab_info *a, struct slab_info *b)
{
	struct slab_info sorted_list;
	struct slab_info *curr = &sorted_list;

	while ((a != NULL) && (b != NULL)) {
		if (sort_func(a, b)) {
			curr->next = a;
			curr = a;
			a = a->next;
		} else {
			curr->next = b;
			curr = b;
			b = b->next;
		}
	}

	curr->next = (a == NULL) ? b : a;
	return sorted_list.next;
}

/* 
 * slabsort - merge sort the slab_info linked list based on sort_func
 */
static struct slab_info *slabsort(struct slab_info *list)
{
	struct slab_info *a, *b;

	if ((list == NULL) || (list->next == NULL))
		return list;

	a = list;
	b = list->next;

	while ((b != NULL) && (b->next != NULL)) {
		list = list->next;
		b = b->next->next;
	}
	
	b = list->next;
	list->next = NULL;

	return merge_objs(slabsort(a), slabsort(b));
}

/*
 * Sort Routines.  Each of these should be associated with a command-line
 * search option.  The functions should fit the prototype:
 *
 *	int sort_foo(const struct slab_info *a, const struct slab_info *b)
 *
 * They return one if the first parameter is larger than the second
 * Otherwise, they return zero.
 */

static int sort_name(const struct slab_info *a, const struct slab_info *b)
{
	return (strcmp(a->name, b->name) < 0) ? 1 : 0;
}

static int sort_nr_objs(const struct slab_info *a, const struct slab_info *b)
{
	return (a->nr_objs > b->nr_objs);
}

static int sort_nr_active_objs(const struct slab_info *a,
				const struct slab_info *b)
{
	return (a->nr_active_objs > b->nr_active_objs);
}

static int sort_obj_size(const struct slab_info *a, const struct slab_info *b)
{
	return (a->obj_size > b->obj_size);
}

static int sort_objs_per_slab(const struct slab_info *a,
				const struct slab_info *b)
{
	return (a->objs_per_slab > b->objs_per_slab);
}

static int sort_pages_per_slab(const struct slab_info *a,
		const struct slab_info *b)
{
	return (a->pages_per_slab > b->pages_per_slab);
}

static int sort_nr_slabs(const struct slab_info *a, const struct slab_info *b)
{
	return (a->nr_slabs > b->nr_slabs);
}

static int sort_nr_active_slabs(const struct slab_info *a,
			const struct slab_info *b)
{
	return (a->nr_active_slabs > b->nr_active_slabs);
}


static int sort_use(const struct slab_info *a, const struct slab_info *b)
{
	return (a->use > b->use);
}

static int sort_cache_size(const struct slab_info *a, const struct slab_info *b)
{
	return (a->cache_size > b->cache_size);
}

/*
 * term_size - set the globals 'cols' and 'rows' to the current terminal size
 */
static void term_size(int unused)
{
	struct winsize ws;
	(void) unused;

	if ((ioctl(1, TIOCGWINSZ, &ws) != -1) && ws.ws_row > 10) {
		cols = ws.ws_col;
		rows = ws.ws_row;
	} else {
		cols = 80;
		rows = 24;
	}
}

static void sigint_handler(int unused)
{
	(void) unused;

	delay = 0;
}

static void usage(const char *cmd)
{
	fprintf(stderr, "usage: %s [options]\n\n", cmd);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  --delay=n, -d n    "
		"delay n seconds between updates\n");
	fprintf(stderr, "  --once, -o         "
		"only display once, then exit\n");
	fprintf(stderr, "  --sort=S, -s S     "
		"specify sort criteria S (see below)\n");
	fprintf(stderr, "  --version, -V      "
		"display version information and exit\n");
	fprintf(stderr, "  --help             display this help and exit\n\n");
	fprintf(stderr, "The following are valid sort criteria:\n");
	fprintf(stderr, "  a: sort by number of active objects\n");
	fprintf(stderr, "  b: sort by objects per slab\n");
	fprintf(stderr, "  c: sort by cache size\n");
	fprintf(stderr, "  l: sort by number of slabs\n");
	fprintf(stderr, "  v: sort by number of active slabs\n");
	fprintf(stderr, "  n: sort by name\n");
	fprintf(stderr, "  o: sort by number of objects\n");
	fprintf(stderr, "  p: sort by pages per slab\n");
	fprintf(stderr, "  s: sort by object size\n");
	fprintf(stderr, "  u: sort by cache utilization\n");
}

/*
 * set_sort_func - return the slab_sort_func that matches the given key.
 * On unrecognizable key, DEF_SORT_FUNC is returned.
 */
static void * set_sort_func(char key)
{
	switch (key) {
	case 'n':
		return sort_name;
	case 'o':
		return sort_nr_objs;
	case 'a':
		return sort_nr_active_objs;
	case 's':
		return sort_obj_size;
	case 'b':
		return sort_objs_per_slab;
	case 'p':
		return sort_pages_per_slab;
	case 'l':
		return sort_nr_slabs;
	case 'v':
		return sort_nr_active_slabs;
	case 'c':
		return sort_cache_size;
	case 'u':
		return sort_use;
	default:
		return DEF_SORT_FUNC;
	}
}

static void parse_input(char c)
{
	c = toupper(c);
	switch(c) {
	case 'A':
		sort_func = sort_nr_active_objs;
		break;
	case 'B':
		sort_func = sort_objs_per_slab;
		break;
	case 'C':
		sort_func = sort_cache_size;
		break;
	case 'L':
		sort_func = sort_nr_slabs;
		break;
	case 'V':
		sort_func = sort_nr_active_slabs;
		break;
	case 'N':
		sort_func = sort_name;
		break;
	case 'O':
		sort_func = sort_nr_objs;
		break;
	case 'P':
		sort_func = sort_pages_per_slab;
		break;
	case 'S':
		sort_func = sort_obj_size;
		break;
	case 'U':
		sort_func = sort_use;
		break;
	case 'Q':
		delay = 0;
		break;
	}
}

int main(int argc, char *argv[])
{
	int o;
	unsigned short old_rows;
	struct slab_info *slab_list = NULL;

	struct option longopts[] = {
		{ "delay",	1, NULL, 'd' },
		{ "sort",	1, NULL, 's' },
		{ "once",	0, NULL, 'o' },
		{ "help",	0, NULL, 'h' },
		{ "version",	0, NULL, 'V' },
		{  NULL,	0, NULL, 0 }
	};

	sort_func = DEF_SORT_FUNC;

	while ((o = getopt_long(argc, argv, "d:s:ohV", longopts, NULL)) != -1) {
		int ret = 1;

		switch (o) {
		case 'd':
			errno = 0;
			delay = strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtoul");
				return 1;
			}
			if (delay < 0) {
				fprintf(stderr, "error: can't have a "\
					"negative delay\n");
				exit(1);
			}
			break;
		case 's':
			sort_func = set_sort_func(optarg[0]);
			break;
		case 'o':
			delay = 0;
			break;
		case 'V':
			display_version();
			return 0;
		case 'h':
			ret = 0;
		default:
			usage(argv[0]);
			return ret;
		}
	}

	if (tcgetattr(0, &saved_tty) == -1)
		perror("tcgetattr");

	initscr();
	term_size(0);
	old_rows = rows;
	resizeterm(rows, cols);
	signal(SIGWINCH, term_size);
	signal(SIGINT, sigint_handler);

	do {
		struct slab_info *curr;
		struct slab_stat stats = SLAB_STAT_ZERO;
		struct timeval tv;
		fd_set readfds;
		char c;
		int i;

		if (get_slabinfo(&slab_list, &stats))
			break;

		if (old_rows != rows) {
			resizeterm(rows, cols);
			old_rows = rows;
		}

		move(0,0);
		printw(	" Active / Total Objects (%% used)    : %d / %d (%.1f%%)\n"
			" Active / Total Slabs (%% used)      : %d / %d (%.1f%%)\n"
			" Active / Total Caches (%% used)     : %d / %d (%.1f%%)\n"
			" Active / Total Size (%% used)       : %.2fK / %.2fK (%.1f%%)\n"
			" Minimum / Average / Maximum Object : %.2fK / %.2fK / %.2fK\n\n",
			stats.nr_active_objs, stats.nr_objs, 100.0 * stats.nr_active_objs / stats.nr_objs,
			stats.nr_active_slabs, stats.nr_slabs, 100.0 * stats.nr_active_slabs / stats.nr_slabs,
			stats.nr_active_caches, stats.nr_caches, 100.0 * stats.nr_active_caches / stats.nr_caches,
			stats.active_size / 1024.0, stats.total_size / 1024.0, 100.0 * stats.active_size / stats.total_size,
			stats.min_obj_size / 1024.0, stats.avg_obj_size / 1024.0, stats.max_obj_size / 1024.0
		);

		slab_list = slabsort(slab_list);

		attron(A_REVERSE);
		printw(	"%6s %6s %4s %8s %6s %8s %10s %-23s\n",
			"OBJS", "ACTIVE", "USE", "OBJ SIZE", "SLABS",
			"OBJ/SLAB", "CACHE SIZE", "NAME");
		attroff(A_REVERSE);

		curr = slab_list;
		for (i = 0; i < rows - 8 && curr->next; i++) {
			printw("%6u %6u %3u%% %7.2fK %6u %8u %9uK %-23s\n",
				curr->nr_objs, curr->nr_active_objs, curr->use,
				curr->obj_size / 1024.0, curr->nr_slabs,
				curr->objs_per_slab, (unsigned)(curr->cache_size / 1024),
				curr->name);
			curr = curr->next;
		}

		refresh();
		put_slabinfo(slab_list);

		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		tv.tv_sec = delay;
		tv.tv_usec = 0;
		if (select(1, &readfds, NULL, NULL, &tv) > 0) {
			if (read(0, &c, 1) != 1)
				break;
			parse_input(c);
		}
	} while (delay);

	tcsetattr(0, TCSAFLUSH, &saved_tty);
	free_slabinfo(slab_list);
	endwin();
	return 0;
}

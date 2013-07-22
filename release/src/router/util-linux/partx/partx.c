/*
 * partx: tell the kernel about your disk's partitions
 * [This is not an fdisk - adding and removing partitions
 * is not a change of the disk, but just telling the kernel
 * about presence and numbering of on-disk partitions.]
 *
 * aeb, 2000-03-21 -- sah is 42 now
 *
 * Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
 *      Rewritten to use libblkid for util-linux
 *      based on ideas from Karel Zak <kzak@redhat.com>
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>

#include <blkid.h>

#include "c.h"
#include "pathnames.h"
#include "nls.h"
#include "tt.h"
#include "blkdev.h"
#include "strutils.h"
#include "xalloc.h"
#include "partx.h"
#include "sysfs.h"
#include "at.h"

/* this is the default upper limit, could be modified by --nr */
#define SLICES_MAX	256

/* all the columns (-o option) */
enum {
	COL_PARTNO,
	COL_START,
	COL_END,
	COL_SECTORS,
	COL_SIZE,
	COL_NAME,
	COL_UUID,
	COL_TYPE,
	COL_FLAGS,
	COL_SCHEME,
	__NCOLUMNS
};

enum {
	ACT_LIST = 1,
	ACT_SHOW,
	ACT_ADD,
	ACT_DELETE
};

enum {
	FL_BYTES = (1 << 1)
};

/* column names */
struct colinfo {
	const char	*name;	/* header */
	double		whint;	/* width hint (N < 1 is in percent of termwidth) */
	int		flags;	/* TT_FL_* */
	const char      *help;
};

/* columns descriptions */
struct colinfo infos[__NCOLUMNS] = {
	[COL_PARTNO]   = { "NR",    0.25, TT_FL_RIGHT, N_("partition number") },
	[COL_START]    = { "START",   0.30, TT_FL_RIGHT, N_("start of the partition in sectors") },
	[COL_END]      = { "END",     0.30, TT_FL_RIGHT, N_("end of the partition in sectors") },
	[COL_SECTORS]  = { "SECTORS", 0.30, TT_FL_RIGHT, N_("number of sectors") },
	[COL_SIZE]     = { "SIZE",    0.30, TT_FL_RIGHT, N_("human readable size") },
	[COL_NAME]     = { "NAME",    0.30, TT_FL_TRUNC, N_("partition name") },
	[COL_UUID]     = { "UUID",    36, 0, N_("partition UUID")},
	[COL_SCHEME]   = { "SCHEME",  0.1, TT_FL_TRUNC, N_("partition table type (dos, gpt, ...)")},
	[COL_FLAGS]    = { "FLAGS",   0.1, TT_FL_TRUNC, N_("partition flags")},
	[COL_TYPE]     = { "TYPE",    1, TT_FL_RIGHT, N_("partition type hex or uuid")},
};
/* array with IDs of enabled columns */
static int columns[__NCOLUMNS], ncolumns;

static int verbose;
static int partx_flags;


static inline int get_column_id(int num)
{
	assert(ARRAY_SIZE(columns) == __NCOLUMNS);
	assert(num < ncolumns);
	assert(columns[num] < __NCOLUMNS);
	return columns[num];
}

static inline struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

static int column_name_to_id(const char *name, size_t namesz)
{
	int i;

	assert(name);

	for (i = 0; i < __NCOLUMNS; i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

/*
 * Given a partition return the corresponding partition number.
 *
 * Note that this function tries to use sysfs, otherwise it assumes that the
 * last characters are always numeric (sda1, sdc20, etc).
 */
static int get_partno_from_device(char *partition, dev_t devno)
{
	int partno = 0;
	size_t sz;
	char *p, *end = NULL;

	assert(partition);

	if (devno) {
		struct sysfs_cxt cxt;

		sysfs_init(&cxt, devno, NULL);
		if (sysfs_read_int(&cxt, "partition", &partno) >= 0) {
			sysfs_deinit(&cxt);
			return partno;
		}
	}

	sz = strlen(partition);
	p = partition + sz - 1;

	if (!isdigit((unsigned int) *p))
		goto err;

	while (isdigit((unsigned int) *(p - 1))) p--;

	errno = 0;
	partno = strtol(p, &end, 10);
	if (errno || !end || *end || p == end)
		goto err;

	return partno;
err:
	errx(EXIT_FAILURE, _("%s: failed to get partition number"), partition);
}

static int get_max_partno(const char *disk, dev_t devno)
{
	char path[PATH_MAX], *parent, *dirname = NULL;
	struct stat st;
	DIR *dir;
	struct dirent *d;
	int partno = 0;

	if (!devno && !stat(disk, &st))
		devno = st.st_rdev;
	if (!devno)
		goto dflt;
	parent = strrchr(disk, '/');
	if (!parent)
		goto dflt;
	parent++;

	snprintf(path, sizeof(path), _PATH_SYS_DEVBLOCK "/%d:%d/",
			major(devno), minor(devno));

	dir = opendir(path);
	if (!dir)
		goto dflt;

	dirname = xstrdup(path);

	while ((d = readdir(dir))) {
		int fd;

		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;
#ifdef _DIRENT_HAVE_D_TYPE
		if (d->d_type != DT_DIR)
			continue;
#endif
		if (strncmp(parent, d->d_name, strlen(parent)))
			continue;
		snprintf(path, sizeof(path), "%s/partition", d->d_name);

		fd = open_at(dirfd(dir), dirname, path, O_RDONLY);
		if (fd) {
			int x = 0;
			FILE *f = fdopen(fd, "r");
			if (f) {
				if (fscanf(f, "%d", &x) == 1 && x > partno)
					partno = x;
				fclose(f);
			}
		}
	}

	free(dirname);
	closedir(dir);
	return partno;
dflt:
	return SLICES_MAX;
}

static void del_parts_warnx(const char *device, int first, int last)
{
	if (first == last)
		warnx(_("%s: error deleting partition %d"), device, first);
	else
		warnx(_("%s: error deleting partitions %d-%d"),
				device, first, last);
}

static int del_parts(int fd, const char *device, dev_t devno,
		     int lower, int upper)
{
	int rc = 0, i, errfirst = 0, errlast = 0;

	assert(fd >= 0);
	assert(device);

	if (!lower)
		lower = 1;
	if (!upper || lower < 0 || upper < 0) {
		int n = get_max_partno(device, devno);
		if (!upper)
			upper = n;
		else if (upper < 0)
			upper = n + upper + 1;
		if (lower < 0)
			lower = n + lower + 1;
	}
	if (lower > upper) {
		warnx(_("specified range <%d:%d> "
			"does not make sense"), lower, upper);
		return -1;
	}

	for (i = lower; i <= upper; i++) {
		if (partx_del_partition(fd, i) == 0) {
			if (verbose)
				printf(_("%s: partition #%d removed\n"), device, i);
			continue;
		}
		rc = -1;
		if (verbose)
			warn(_("%s: deleting partition #%d failed"), device, i);
		if (!errfirst)
			errlast = errfirst = i;
		else if (errlast + 1 == i)
			errlast++;
		else {
			del_parts_warnx(device, errfirst, errlast);
			errlast = errfirst = i;
		}
	}

	if (errfirst)
		del_parts_warnx(device, errfirst, errlast);
	return rc;
}

static void add_parts_warnx(const char *device, int first, int last)
{
	if (first == last)
		warnx(_("%s: error adding partition %d"), device, first);
	else
		warnx(_("%s: error adding partitions %d-%d"),
				device, first, last);
}

static int add_parts(int fd, const char *device,
			blkid_partlist ls, int lower, int upper)
{
	int i, nparts, rc = 0, errfirst = 0, errlast = 0;

	assert(fd >= 0);
	assert(device);
	assert(ls);

	nparts = blkid_partlist_numof_partitions(ls);

	for (i = 0; i < nparts; i++) {
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		int n = blkid_partition_get_partno(par);
		uintmax_t start, size;

		if (lower && n < lower)
			continue;
		if (upper && n > upper)
			continue;

		start = blkid_partition_get_start(par);
		size =  blkid_partition_get_size(par);

		if (blkid_partition_is_extended(par))
			/*
			 * Let's follow the Linux kernel and reduce
			 * DOS extended partition to 1 or 2 sectors.
			 */
			size = min(size, (uintmax_t) 2);

		if (partx_add_partition(fd, n, start, size) == 0) {
			if (verbose)
				printf(_("%s: partition #%d added\n"), device, n);
			continue;
		}
		rc = -1;
		if (verbose)
			warn(_("%s: adding partition #%d failed"), device, n);
		if (!errfirst)
			errlast = errfirst = n;
		else if (errlast + 1 == n)
			errlast++;
		else {
			add_parts_warnx(device, errfirst, errlast);
			errlast = errfirst = n;
		}
	}

	if (errfirst)
		add_parts_warnx(device, errfirst, errlast);
	return rc;
}

static int list_parts(blkid_partlist ls, int lower, int upper)
{
	int i, nparts;

	assert(ls);

	nparts = blkid_partlist_numof_partitions(ls);

	for (i = 0; i < nparts; i++) {
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		int n = blkid_partition_get_partno(par);
		uintmax_t start, size;

		if (lower && n < lower)
			continue;
		if (upper && n > upper)
			continue;

		start = blkid_partition_get_start(par);
		size =  blkid_partition_get_size(par);

		printf(_("#%2d: %9ju-%9ju (%9ju sectors, %6ju MB)\n"),
		       n, start, start + size -1,
		       size, (size << 9) / 1000000);
	}
	return 0;
}

static void add_tt_line(struct tt *tt, blkid_partition par)
{
	struct tt_line *line;
	int i;

	assert(tt);
	assert(par);

	line = tt_add_line(tt, NULL);
	if (!line) {
		warn(_("failed to add line to output"));
		return;
	}

	for (i = 0; i < ncolumns; i++) {
		char *str = NULL;
		int rc = 0;

		switch (get_column_id(i)) {
		case COL_PARTNO:
			rc = asprintf(&str, "%d",
					blkid_partition_get_partno(par));
			break;
		case COL_START:
			rc = asprintf(&str, "%ju",
					blkid_partition_get_start(par));
			break;
		case COL_END:
			rc = asprintf(&str, "%ju",
					blkid_partition_get_start(par) +
					blkid_partition_get_size(par) - 1);
			break;
		case COL_SECTORS:
			rc = asprintf(&str, "%ju",
					blkid_partition_get_size(par));
			break;
		case COL_SIZE:
			if (partx_flags & FL_BYTES)
				rc = asprintf(&str, "%ju", (uintmax_t)
					blkid_partition_get_size(par) << 9);
			else
				str = size_to_human_string(SIZE_SUFFIX_1LETTER,
					blkid_partition_get_size(par) << 9);
			break;
		case COL_NAME:
			str = (char *) blkid_partition_get_name(par);
			if (str)
				str = xstrdup(str);
			break;
		case COL_UUID:
			str = (char *) blkid_partition_get_uuid(par);
			if (str)
				str = xstrdup(str);
			break;
		case COL_TYPE:
			str = (char *) blkid_partition_get_type_string(par);
			if (str)
				str = xstrdup(str);
			else
				rc = asprintf(&str, "0x%x",
					blkid_partition_get_type(par));
			break;
		case COL_FLAGS:
			rc = asprintf(&str, "0x%llx", blkid_partition_get_flags(par));
			break;
		case COL_SCHEME:
		{
			blkid_parttable tab = blkid_partition_get_table(par);
			if (tab) {
				str = (char *) blkid_parttable_get_type(tab);
				if (str)
					str = xstrdup(str);
			}
			break;
		}
		default:
			break;
		}

		if (rc || str)
			tt_line_set_data(line, i, str);
	}
}

static int show_parts(blkid_partlist ls, int tt_flags, int lower, int upper)
{
	int i, rc = -1;
	struct tt *tt;
	int nparts;

	assert(ls);

	nparts = blkid_partlist_numof_partitions(ls);
	if (!nparts)
		return 0;

	tt = tt_new_table(tt_flags);
	if (!tt) {
		warn(_("failed to initialize output table"));
		return -1;
	}

	for (i = 0; i < ncolumns; i++) {
		struct colinfo *col = get_column_info(i);

		if (!tt_define_column(tt, col->name, col->whint, col->flags)) {
			warnx(_("failed to initialize output column"));
			goto done;
		}
	}

	for (i = 0; i < nparts; i++) {
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		int n = blkid_partition_get_partno(par);

		if (lower && n < lower)
			continue;
		if (upper && n > upper)
			continue;

		add_tt_line(tt, par);
	}

	rc = 0;
	tt_print_table(tt);
done:
	tt_free_table(tt);
	return rc;
}

static int parse_range(const char *str, int *lower, int *upper)
{
	char *end = NULL;

	if (!str)
		return 0;

	*upper = *lower = 0;
	errno = 0;

	if (*str == ':') {				/* <:N> */
		str++;
		*upper = strtol(str, &end, 10);
		if (errno || !end || *end || end == str)
			return -1;
	} else {
		*upper = *lower = strtol(str, &end, 10);
		if (errno || !end || end == str)
			return -1;

		if (*end == ':' && !*(end + 1))		/* <M:> */
			*upper = 0;
		else if (*end == '-' || *end == ':') {	/* <M:N> <M-N> */
			str = end + 1;
			end = NULL;
			errno = 0;
			*upper = strtol(str, &end, 10);

			if (errno || !end || *end || end == str)
				return -1;
		}
	}
	return 0;
}

static blkid_partlist get_partlist(blkid_probe pr,
			const char *device, char *type)
{
	blkid_partlist ls;
	blkid_parttable tab;

	assert(pr);
	assert(device);

	if (type) {
		char *name[] = { type, NULL };

		if (blkid_probe_filter_partitions_type(pr,
				BLKID_FLTR_ONLYIN, name)) {
			warnx(_("failed to initialize blkid "
				"filter for '%s'"), type);
			return NULL;
		}
	}

	ls = blkid_probe_get_partitions(pr);
	if (!ls) {
		warnx(_("%s: failed to read partition table"), device);
		return NULL;
	}

	tab = blkid_partlist_get_table(ls);
	if (verbose && tab)
		printf(_("%s: partition table type '%s' detected\n"),
		       device, blkid_parttable_get_type(tab));

	if (!blkid_partlist_numof_partitions(ls)) {
		warnx(_("%s: %s partition table does not contains "
			"usable partitions"), device,
			blkid_parttable_get_type(tab));
		return NULL;
	}
	return ls;
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	int i;

	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [-a|-d|-s] [--nr <n:m> | <partition>] <disk>\n"),
		program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -a, --add            add specified partitions or all of them\n"
		" -d, --delete         delete specified partitions or all of them\n"
		" -l, --list           list partitions (DEPRECATED)\n"
		" -s, --show           list partitions\n\n"

		" -b, --bytes          print SIZE in bytes rather than in human readable format\n"
		" -g, --noheadings     don't print headings for --show\n"
		" -P, --pairs          use key=\"value\" output format\n"
		" -r, --raw            use raw output format\n"
		" -t, --type <type>    specify the partition type (dos, bsd, solaris, etc.)\n"
		" -n, --nr <n:m>       specify the range of partitions (e.g. --nr 2:4)\n"
		" -o, --output <type>  define which output columns to use\n"
		" -h, --help           print this help\n"), out);

	fputs(_("\nAvailable columns (for --show, --raw or --pairs):\n"), out);

	for (i = 0; i < __NCOLUMNS; i++)
		fprintf(out, " %10s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, _("\nFor more information see partx(8).\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((__noreturn__))
errx_mutually_exclusive(const char *opts)
{
	errx(EXIT_FAILURE, _("the options %s are mutually exclusive"), opts);
}

int main(int argc, char **argv)
{
	int fd, c, what = 0, lower = 0, upper = 0, rc = 0;
	int tt_flags = 0;
	char *type = NULL;
	char *device = NULL; /* pointer to argv[], ie: /dev/sda1 */
	char *wholedisk = NULL; /* allocated, ie: /dev/sda */
	dev_t disk_devno = 0, part_devno = 0;

	static const struct option long_opts[] = {
		{ "bytes",	no_argument,       NULL, 'b' },
		{ "noheadings",	no_argument,       NULL, 'g' },
		{ "raw",	no_argument,       NULL, 'r' },
		{ "list",	no_argument,	   NULL, 'l' },
		{ "show",	no_argument,       NULL, 's' },
		{ "add",	no_argument,       NULL, 'a' },
		{ "delete",	no_argument,	   NULL, 'd' },
		{ "type",	required_argument, NULL, 't' },
		{ "nr",		required_argument, NULL, 'n' },
		{ "output",	required_argument, NULL, 'o' },
		{ "pairs",      no_argument,       NULL, 'P' },
		{ "help",	no_argument,       NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv,
				"abdglrsvn:t:o:Ph", long_opts, NULL)) != -1) {

		switch(c) {
		case 'a':
		case 'd':
		case 'l':
		case 'r':
		case 'P':
		case 's':
			if (what)
				 errx_mutually_exclusive("--{add,delete,show,list,raw,pairs}");
			break;
		}

		switch(c) {
		case 'a':
			what = ACT_ADD;
			break;
		case 'b':
			partx_flags |= FL_BYTES;
			break;
		case 'd':
			what = ACT_DELETE;
			break;
		case 'g':
			tt_flags |= TT_FL_NOHEADINGS;
			break;
		case 'l':
			what = ACT_LIST;
			break;
		case 'n':
			if (parse_range(optarg, &lower, &upper))
				errx(EXIT_FAILURE, _("failed to parse --nr <M-N> range"));
			break;
		case 'o':
			ncolumns = string_to_idarray(optarg,
						columns, ARRAY_SIZE(columns),
						column_name_to_id);
			if (ncolumns < 0)
				return EXIT_FAILURE;
			break;
		case 'P':
			tt_flags |= TT_FL_EXPORT;
			what = ACT_SHOW;
			break;
		case 'r':
			tt_flags |= TT_FL_RAW;
			what = ACT_SHOW;
			break;
		case 's':
			what = ACT_SHOW;
			break;
		case 't':
			type = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			usage(stdout);
		case '?':
		default:
			usage(stderr);
		}
	}

	/* -o <list> enables --show mode by default */
	if (ncolumns && !what)
		what = ACT_SHOW;

	/* backwardly compatible default */
	if (!what)
		what = ACT_LIST;

	/* --show default, could by modified by -o  */
	if (what == ACT_SHOW && !ncolumns) {
		columns[ncolumns++] = COL_PARTNO;
		columns[ncolumns++] = COL_START;
		columns[ncolumns++] = COL_END;
		columns[ncolumns++] = COL_SECTORS;
		columns[ncolumns++] = COL_SIZE;
		columns[ncolumns++] = COL_NAME;
		columns[ncolumns++] = COL_UUID;
	}

	/*
	 * Note that 'partx /dev/sda1' == 'partx /dev/sda1 /dev/sda'
	 * so assume that the device and/or disk are always the last
	 * arguments to be passed to partx.
	 */
	if (optind == argc - 2) {
		/* passed 2 arguments:
		 *   /dev/sda1 /dev/sda  : partition + whole-disk
		 *   -- /dev/sda1	 : partition that should be used as a whole-disk
		 */
		device = argv[optind];

		if (strcmp(device, "-") == 0) {
			device = NULL;
			wholedisk = xstrdup(argv[optind + 1]);
		} else {
			device = argv[optind];
			wholedisk = xstrdup(argv[optind + 1]);
		}
	} else if (optind == argc - 1) {
		/* passed only one arg (ie: /dev/sda3 or /dev/sda) */
		struct stat sb;

		device = argv[optind];

		if (stat(device, &sb))
			err(EXIT_FAILURE, _("%s: stat failed"), device);

		part_devno = sb.st_rdev;

		if (blkid_devno_to_wholedisk(part_devno,
					     NULL, 0, &disk_devno) == 0 &&
		    part_devno != disk_devno)
			wholedisk = blkid_devno_to_devname(disk_devno);

		if (!wholedisk) {
			wholedisk = xstrdup(device);
			disk_devno = part_devno;
			device = NULL;
			part_devno = 0;
		}
	} else
		usage(stderr);

	if (device && (upper || lower))
		errx(EXIT_FAILURE, _("--nr and <partition> are mutually exclusive"));

	assert(wholedisk);

	if (device) {
		/* use partno from given partition instead of --nr range, e.g:
		 *   partx -d /dev/sda3
		 * is the same like:
		 *   partx -d --nr 3 /dev/sda
		 */
		struct stat sb;

		if (!part_devno && !stat(device, &sb))
			part_devno = sb.st_rdev;

		lower = upper = get_partno_from_device(device, part_devno);
	}

	if (verbose)
		printf(_("partition: %s, disk: %s, lower: %d, upper: %d\n"),
		       device ? device : "none", wholedisk, lower, upper);

	if (what == ACT_ADD || what == ACT_DELETE) {
		struct stat x;

		if (stat(wholedisk, &x) || !S_ISBLK(x.st_mode))
			errx(EXIT_FAILURE, _("%s: not a block device"), wholedisk);
	}
	if ((fd = open(wholedisk, O_RDONLY)) == -1)
		err(EXIT_FAILURE, _("%s: open failed"), wholedisk);

	if (what == ACT_DELETE)
		rc = del_parts(fd, wholedisk, disk_devno, lower, upper);
	else {
		blkid_probe pr = blkid_new_probe();
		blkid_partlist ls = NULL;

		if (!pr || blkid_probe_set_device(pr, fd, 0, 0))
			warnx(_("%s: failed to initialize blkid prober"),
					wholedisk);
		else
			ls = get_partlist(pr, wholedisk, type);

		if (ls) {
			int n = blkid_partlist_numof_partitions(ls);

			if (lower < 0)
				lower = n + lower + 1;
			if (upper < 0)
				upper = n + upper + 1;
			if (lower > upper) {
				warnx(_("specified range <%d:%d> "
					"does not make sense"), lower, upper);
				rc = -1, what = 0;
			}

			switch (what) {
			case ACT_SHOW:
				rc = show_parts(ls, tt_flags, lower, upper);
				break;
			case ACT_LIST:
				rc = list_parts(ls, lower, upper);
				break;
			case ACT_ADD:
				rc = add_parts(fd, wholedisk, ls, lower, upper);
				break;
			}
		}
		blkid_free_probe(pr);
	}

	close(fd);
	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}


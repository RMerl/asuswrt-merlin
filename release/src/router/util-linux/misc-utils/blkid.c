/*
 * blkid.c - User command-line interface for libblkid
 *
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int getopt(int argc, char * const argv[], const char *optstring);
extern char *optarg;
extern int optind;
#endif

#define OUTPUT_VALUE_ONLY	(1 << 1)
#define OUTPUT_DEVICE_ONLY	(1 << 2)
#define OUTPUT_PRETTY_LIST	(1 << 3)
#define OUTPUT_UDEV_LIST	(1 << 4)
#define OUTPUT_EXPORT_LIST	(1 << 5)

#define LOWPROBE_TOPOLOGY	(1 << 1)
#define LOWPROBE_SUPERBLOCKS	(1 << 2)

#include <blkid.h>

#include "ismounted.h"
#include "strutils.h"

const char *progname = "blkid";

int raw_chars;

static void print_version(FILE *out)
{
	fprintf(out, "%s from %s (libblkid %s, %s)\n",
		progname, PACKAGE_STRING, LIBBLKID_VERSION, LIBBLKID_DATE);
}

static void usage(int error)
{
	FILE *out = error ? stderr : stdout;

	print_version(out);
	fprintf(out,
		"Usage:\n"
		" %1$s -L <label> | -U <uuid>\n\n"
		" %1$s [-c <file>] [-ghlLv] [-o <format>] [-s <tag>] \n"
		"       [-t <token>] [<dev> ...]\n\n"
		" %1$s -p [-s <tag>] [-O <offset>] [-S <size>] \n"
		"       [-o <format>] <dev> ...\n\n"
		" %1$s -i [-s <tag>] [-o <format>] <dev> ...\n\n"
		"Options:\n"
		" -c <file>   read from <file> instead of reading from the default\n"
		"               cache file (-c /dev/null means no cache)\n"
		" -d          don't encode non-printing characters\n"
		" -h          print this usage message and exit\n"
		" -g          garbage collect the blkid cache\n"
		" -o <format> output format; can be one of:\n"
		"               value, device, list, udev, export or full; (default: full)\n"
		" -k          list all known filesystems/RAIDs and exit\n"
		" -s <tag>    show specified tag(s) (default show all tags)\n"
		" -t <token>  find device with a specific token (NAME=value pair)\n"
		" -l          look up only first device with token specified by -t\n"
		" -L <label>  convert LABEL to device name\n"
		" -U <uuid>   convert UUID to device name\n"
		" -v          print version and exit\n"
		" <dev>       specify device(s) to probe (default: all devices)\n\n"
		"Low-level probing options:\n"
		" -p          low-level superblocks probing (bypass cache)\n"
		" -i          gather information about I/O limits\n"
		" -S <size>   overwrite device size\n"
		" -O <offset> probe at the given offset\n"
		" -u <list>   filter by \"usage\" (e.g. -u filesystem,raid)\n"
		" -n <list>   filter by filesystem type (e.g. -n vfat,ext3)\n"
		"\n", progname);

	exit(error);
}

/*
 * This function does "safe" printing.  It will convert non-printable
 * ASCII characters using '^' and M- notation.
 */
static void safe_print(const char *cp, int len)
{
	unsigned char	ch;

	if (len < 0)
		len = strlen(cp);

	while (len--) {
		ch = *cp++;
		if (!raw_chars) {
			if (ch >= 128) {
				fputs("M-", stdout);
				ch -= 128;
			}
			if ((ch < 32) || (ch == 0x7f)) {
				fputc('^', stdout);
				ch ^= 0x40; /* ^@, ^A, ^B; ^? for DEL */
			}
		}
		fputc(ch, stdout);
	}
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
		return (t_win.ts_cols);
#endif
#ifdef TIOCGWINSZ
	if (ioctl (0, TIOCGWINSZ, &w_win) == 0)
		return (w_win.ws_col);
#endif
        cp = getenv("COLUMNS");
	if (cp)
		return strtol(cp, NULL, 10);
	return 80;
}

static int pretty_print_word(const char *str, int max_len,
			     int left_len, int overflow_nl)
{
	int len = strlen(str) + left_len;
	int ret = 0;

	fputs(str, stdout);
	if (overflow_nl && len > max_len) {
		fputc('\n', stdout);
		len = 0;
	} else if (len > max_len)
		ret = len - max_len;
	do
		fputc(' ', stdout);
	while (len++ < max_len);
	return ret;
}

static void pretty_print_line(const char *device, const char *fs_type,
			      const char *label, const char *mtpt,
			      const char *uuid)
{
	static int device_len = 10, fs_type_len = 7;
	static int label_len = 8, mtpt_len = 14;
	static int term_width = -1;
	int len, w;

	if (term_width < 0)
		term_width = get_terminal_width();

	if (term_width > 80) {
		term_width -= 80;
		w = term_width / 10;
		if (w > 8)
			w = 8;
		term_width -= 2*w;
		label_len += w;
		fs_type_len += w;
		w = term_width/2;
		device_len += w;
		mtpt_len +=w;
	}

	len = pretty_print_word(device, device_len, 0, 1);
	len = pretty_print_word(fs_type, fs_type_len, len, 0);
	len = pretty_print_word(label, label_len, len, 0);
	pretty_print_word(mtpt, mtpt_len, len, 0);

	fputs(uuid, stdout);
	fputc('\n', stdout);
}

static void pretty_print_dev(blkid_dev dev)
{
	blkid_tag_iterate	iter;
	const char		*type, *value, *devname;
	const char		*uuid = "", *fs_type = "", *label = "";
	int			len, mount_flags;
	char			mtpt[80];
	int			retval;

	if (dev == NULL) {
		pretty_print_line("device", "fs_type", "label",
				  "mount point", "UUID");
		for (len=get_terminal_width()-1; len > 0; len--)
			fputc('-', stdout);
		fputc('\n', stdout);
		return;
	}

	devname = blkid_dev_devname(dev);
	if (access(devname, F_OK))
		return;

	/* Get the uuid, label, type */
	iter = blkid_tag_iterate_begin(dev);
	while (blkid_tag_next(iter, &type, &value) == 0) {
		if (!strcmp(type, "UUID"))
			uuid = value;
		if (!strcmp(type, "TYPE"))
			fs_type = value;
		if (!strcmp(type, "LABEL"))
			label = value;
	}
	blkid_tag_iterate_end(iter);

	/* Get the mount point */
	mtpt[0] = 0;
	retval = check_mount_point(devname, &mount_flags, mtpt, sizeof(mtpt));
	if (retval == 0) {
		if (mount_flags & MF_MOUNTED) {
			if (!mtpt[0])
				strcpy(mtpt, "(mounted, mtpt unknown)");
		} else if (mount_flags & MF_BUSY)
			strcpy(mtpt, "(in use)");
		else
			strcpy(mtpt, "(not mounted)");
	}

	pretty_print_line(devname, fs_type, label, mtpt, uuid);
}

static void print_udev_format(const char *name, const char *value)
{
	char enc[265], safe[256];
	size_t namelen = strlen(name);

	*safe = *enc = '\0';

	if (!strcmp(name, "TYPE") || !strcmp(name, "VERSION")) {
		blkid_encode_string(value, enc, sizeof(enc));
		printf("ID_FS_%s=%s\n", name, enc);

	} else if (!strcmp(name, "UUID") ||
		 !strcmp(name, "LABEL") ||
		 !strcmp(name, "UUID_SUB")) {

		blkid_safe_string(value, safe, sizeof(safe));
		printf("ID_FS_%s=%s\n", name, safe);

		blkid_encode_string(value, enc, sizeof(enc));
		printf("ID_FS_%s_ENC=%s\n", name, enc);

	} else if (!strcmp(name, "PTTYPE")) {
		printf("ID_PART_TABLE_TYPE=%s\n", value);

	} else if (!strcmp(name, "PART_ENTRY_NAME") ||
		  !strcmp(name, "PART_ENTRY_TYPE")) {

		blkid_encode_string(value, enc, sizeof(enc));
		printf("ID_%s=%s\n", name, enc);

	} else if (!strncmp(name, "PART_ENTRY_", 11))
		printf("ID_%s=%s\n", name, value);

	else if (namelen >= 15 && (
		   !strcmp(name + (namelen - 12), "_SECTOR_SIZE") ||
		   !strcmp(name + (namelen - 8), "_IO_SIZE") ||
		   !strcmp(name, "ALIGNMENT_OFFSET")))
			printf("ID_IOLIMIT_%s=%s\n", name, value);
	else
		printf("ID_FS_%s=%s\n", name, value);
}

static int has_item(char *ary[], const char *item)
{
	char **p;

	for (p = ary; *p != NULL; p++)
		if (!strcmp(item, *p))
			return 1;
	return 0;
}

static void print_value(int output, int num, const char *devname,
			const char *value, const char *name, size_t valsz)
{
	if (output & OUTPUT_VALUE_ONLY) {
		fputs(value, stdout);
		fputc('\n', stdout);

	} else if (output & OUTPUT_UDEV_LIST) {
		print_udev_format(name, value);

	} else if (output & OUTPUT_EXPORT_LIST) {
		fputs(name, stdout);
		fputs("=", stdout);
		safe_print(value, valsz);
		fputs("\n", stdout);

	} else {
		if (num == 1 && devname)
			printf("%s: ", devname);
		fputs(name, stdout);
		fputs("=\"", stdout);
		safe_print(value, valsz);
		fputs("\" ", stdout);
	}
}

static void print_tags(blkid_dev dev, char *show[], int output)
{
	blkid_tag_iterate	iter;
	const char		*type, *value, *devname;
	int			num = 1;
	static int		first = 1;

	if (!dev)
		return;

	if (output & OUTPUT_PRETTY_LIST) {
		pretty_print_dev(dev);
		return;
	}

	devname = blkid_dev_devname(dev);

	if (output & OUTPUT_DEVICE_ONLY) {
		printf("%s\n", devname);
		return;
	}

	iter = blkid_tag_iterate_begin(dev);
	while (blkid_tag_next(iter, &type, &value) == 0) {
		if (show[0] && !has_item(show, type))
			continue;

		if (num == 1 && !first &&
		    (output & (OUTPUT_UDEV_LIST | OUTPUT_EXPORT_LIST)))
			/* add extra line between output from more devices */
			fputc('\n', stdout);

		print_value(output, num++, devname, value, type, strlen(value));
	}
	blkid_tag_iterate_end(iter);

	if (num > 1) {
		if (!(output & (OUTPUT_VALUE_ONLY | OUTPUT_UDEV_LIST |
						OUTPUT_EXPORT_LIST)))
			printf("\n");
		first = 0;
	}
}


static int append_str(char **res, size_t *sz, const char *a, const char *b)
{
	char *str = *res;
	size_t asz = a ? strlen(a) : 0;
	size_t bsz = b ? strlen(b) : 0;
	size_t len = *sz + asz + bsz;

	if (!len)
		return -1;

	str = realloc(str, len + 1);
	if (!str)
		return -1;

	*res = str;
	str += *sz;

	if (a) {
		memcpy(str, a, asz);
		str += asz;
	}
	if (b) {
		memcpy(str, b, bsz);
		str += bsz;
	}
	*str = '\0';
	*sz = len;
	return 0;
}

/*
 * Compose and print ID_FS_AMBIVALENT for udev
 */
static int print_udev_ambivalent(blkid_probe pr)
{
	char *val = NULL;
	size_t valsz = 0;
	int count = 0, rc = -1;

	while (!blkid_do_probe(pr)) {
		const char *usage = NULL, *type = NULL, *version = NULL;
		char enc[256];

		blkid_probe_lookup_value(pr, "USAGE", &usage, NULL);
		blkid_probe_lookup_value(pr, "TYPE", &type, NULL);
		blkid_probe_lookup_value(pr, "VERSION", &version, NULL);

		if (!usage || !type)
			continue;

		blkid_encode_string(usage, enc, sizeof(enc));
		if (append_str(&val, &valsz, enc, ":"))
			goto done;

		blkid_encode_string(type, enc, sizeof(enc));
		if (append_str(&val, &valsz, enc, version ? ":" : " "))
			goto done;

		if (version) {
			blkid_encode_string(version, enc, sizeof(enc));
			if (append_str(&val, &valsz, enc, " "))
				goto done;
		}
		count++;
	}

	if (count > 1) {
		*(val + valsz - 1) = '\0';		/* rem tailing whitespace */
		printf("ID_FS_AMBIVALEN=%s\n", val);
		rc = 0;
	}
done:
	free(val);
	return rc;
}

static int lowprobe_superblocks(blkid_probe pr)
{
	struct stat st;
	int rc, fd = blkid_probe_get_fd(pr);

	if (fd < 0 || fstat(fd, &st))
		return -1;

	blkid_probe_enable_partitions(pr, 1);

	if (!S_ISCHR(st.st_mode) && blkid_probe_get_size(pr) <= 1024 * 1440 &&
	    blkid_probe_is_wholedisk(pr)) {
		/*
		 * check if the small disk is partitioned, if yes then
		 * don't probe for filesystems.
		 */
		blkid_probe_enable_superblocks(pr, 0);

		rc = blkid_do_fullprobe(pr);
		if (rc < 0)
			return rc;	/* -1 = error, 1 = nothing, 0 = succes */

		if (blkid_probe_lookup_value(pr, "PTTYPE", NULL, NULL) == 0)
			return 0;	/* partition table detected */
	}

	blkid_probe_set_partitions_flags(pr, BLKID_PARTS_ENTRY_DETAILS);
	blkid_probe_enable_superblocks(pr, 1);

	return blkid_do_safeprobe(pr);
}

static int lowprobe_topology(blkid_probe pr)
{
	/* enable topology probing only */
	blkid_probe_enable_topology(pr, 1);

	blkid_probe_enable_superblocks(pr, 0);
	blkid_probe_enable_partitions(pr, 0);

	return blkid_do_fullprobe(pr);
}

static int lowprobe_device(blkid_probe pr, const char *devname,
			int chain, char *show[], int output,
			blkid_loff_t offset, blkid_loff_t size)
{
	const char *data;
	const char *name;
	int nvals = 0, n, num = 1;
	size_t len;
	int fd;
	int rc = 0;
	static int first = 1;

	fd = open(devname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "error: %s: %m\n", devname);
		return 2;
	}
	if (blkid_probe_set_device(pr, fd, offset, size))
		goto done;

	if (chain & LOWPROBE_TOPOLOGY)
		rc = lowprobe_topology(pr);
	if (rc >= 0 && (chain & LOWPROBE_SUPERBLOCKS))
		rc = lowprobe_superblocks(pr);
	if (rc < 0)
		goto done;

	if (!rc)
		nvals = blkid_probe_numof_values(pr);

	if (nvals &&
	    !(chain & LOWPROBE_TOPOLOGY) &&
	    !(output & OUTPUT_UDEV_LIST) &&
	    !blkid_probe_has_value(pr, "TYPE") &&
	    !blkid_probe_has_value(pr, "PTTYPE"))
		/*
		 * Ignore probing result if there is not any filesystem or
		 * partition table on the device and udev output is not
		 * requested.
		 *
		 * The udev db stores information about partitions, so
		 * PART_ENTRY_* values are alway important.
		 */
		nvals = 0;

	if (nvals && !first && output & (OUTPUT_UDEV_LIST | OUTPUT_EXPORT_LIST))
		/* add extra line between output from devices */
		fputc('\n', stdout);

	if (nvals && (output & OUTPUT_DEVICE_ONLY)) {
		printf("%s\n", devname);
		goto done;
	}

	for (n = 0; n < nvals; n++) {
		if (blkid_probe_get_value(pr, n, &name, &data, &len))
			continue;
		if (show[0] && !has_item(show, name))
			continue;
		len = strnlen((char *) data, len);
		print_value(output, num++, devname, (char *) data, name, len);
	}

	if (first)
		first = 0;
	if (nvals >= 1 && !(output & (OUTPUT_VALUE_ONLY |
					OUTPUT_UDEV_LIST | OUTPUT_EXPORT_LIST)))
		printf("\n");
done:
	if (rc == -2) {
		if (output & OUTPUT_UDEV_LIST)
			print_udev_ambivalent(pr);
		else
			fprintf(stderr,
				"%s: ambivalent result (probably more "
				"filesystems on the device, use wipefs(8) "
				"to see more details)\n",
				devname);
	}
	close(fd);

	if (rc == -2)
		return 8;	/* ambivalent probing result */
	if (!nvals)
		return 2;	/* nothing detected */

	return 0;		/* sucess */
}

/* converts comma separated list to BLKID_USAGE_* mask */
static int list_to_usage(const char *list, int *flag)
{
	int mask = 0;
	const char *word = NULL, *p = list;

	if (p && strncmp(p, "no", 2) == 0) {
		*flag = BLKID_FLTR_NOTIN;
		p += 2;
	}
	if (!p || !*p)
		goto err;
	while(p) {
		word = p;
		p = strchr(p, ',');
		if (p)
			p++;
		if (!strncmp(word, "filesystem", 10))
			mask |= BLKID_USAGE_FILESYSTEM;
		else if (!strncmp(word, "raid", 4))
			mask |= BLKID_USAGE_RAID;
		else if (!strncmp(word, "crypto", 6))
			mask |= BLKID_USAGE_CRYPTO;
		else if (!strncmp(word, "other", 5))
			mask |= BLKID_USAGE_OTHER;
		else
			goto err;
	}
	return mask;
err:
	*flag = 0;
	fprintf(stderr, "unknown kerword in -u <list> argument: '%s'\n",
			word ? word : list);
	exit(4);
}

/* converts comma separated list to types[] */
static char **list_to_types(const char *list, int *flag)
{
	int i;
	const char *p = list;
	char **res = NULL;

	if (p && strncmp(p, "no", 2) == 0) {
		*flag = BLKID_FLTR_NOTIN;
		p += 2;
	}
	if (!p || !*p) {
		fprintf(stderr, "error: -u <list> argument is empty\n");
		goto err;
	}
	for (i = 1; p && (p = strchr(p, ',')); i++, p++);

	res = calloc(i + 1, sizeof(char *));
	if (!res)
		goto err_mem;
	p = *flag & BLKID_FLTR_NOTIN ? list + 2 : list;
	i = 0;

	while(p) {
		const char *word = p;
		p = strchr(p, ',');
		res[i] = p ? strndup(word, p - word) : strdup(word);
		if (!res[i++])
			goto err_mem;
		if (p)
			p++;
	}
	res[i] = NULL;
	return res;
err_mem:
	fprintf(stderr, "out of memory\n");
err:
	*flag = 0;
	free(res);
	exit(4);
}

static void free_types_list(char *list[])
{
	char **n;

	if (!list)
		return;
	for (n = list; *n; n++)
		free(*n);
	free(list);
}

int main(int argc, char **argv)
{
	blkid_cache cache = NULL;
	char **devices = NULL;
	char *show[128] = { NULL, };
	char *search_type = NULL, *search_value = NULL;
	char *read = NULL;
	int fltr_usage = 0;
	char **fltr_type = NULL;
	int fltr_flag = BLKID_FLTR_ONLYIN;
	unsigned int numdev = 0, numtag = 0;
	int version = 0;
	int err = 4;
	unsigned int i;
	int output_format = 0;
	int lookup = 0, gc = 0, lowprobe = 0, eval = 0;
	int c;
	uintmax_t offset = 0, size = 0;

	show[0] = NULL;

	while ((c = getopt (argc, argv, "c:df:ghilL:n:ko:O:ps:S:t:u:U:w:v")) != EOF)
		switch (c) {
		case 'c':
			if (optarg && !*optarg)
				read = NULL;
			else
				read = optarg;
			break;
		case 'd':
			raw_chars = 1;
			break;
		case 'L':
			eval++;
			search_value = strdup(optarg);
			search_type = strdup("LABEL");
			break;
		case 'n':
			if (fltr_usage) {
				fprintf(stderr, "error: -u and -n options are mutually exclusive\n");
				exit(4);
			}
			fltr_type = list_to_types(optarg, &fltr_flag);
			break;
		case 'u':
			if (fltr_type) {
				fprintf(stderr, "error: -u and -n options are mutually exclusive\n");
				exit(4);
			}
			fltr_usage = list_to_usage(optarg, &fltr_flag);
			break;
		case 'U':
			eval++;
			search_value = strdup(optarg);
			search_type = strdup("UUID");
			break;
		case 'i':
			lowprobe |= LOWPROBE_TOPOLOGY;
			break;
		case 'l':
			lookup++;
			break;
		case 'g':
			gc = 1;
			break;
		case 'k':
		{
			size_t idx = 0;
			const char *name = NULL;

			while (blkid_superblocks_get_name(idx++, &name, NULL) == 0)
				printf("%s\n", name);
			exit(0);
		}
		case 'o':
			if (!strcmp(optarg, "value"))
				output_format = OUTPUT_VALUE_ONLY;
			else if (!strcmp(optarg, "device"))
				output_format = OUTPUT_DEVICE_ONLY;
			else if (!strcmp(optarg, "list"))
				output_format = OUTPUT_PRETTY_LIST;
			else if (!strcmp(optarg, "udev"))
				output_format = OUTPUT_UDEV_LIST;
			else if (!strcmp(optarg, "export"))
				output_format = OUTPUT_EXPORT_LIST;
			else if (!strcmp(optarg, "full"))
				output_format = 0;
			else {
				fprintf(stderr, "Invalid output format %s. "
					"Choose from value,\n\t"
					"device, list, udev or full\n", optarg);
				exit(4);
			}
			break;
		case 'O':
			if (strtosize(optarg, &offset))
				fprintf(stderr,
					"Invalid offset '%s' specified\n",
					optarg);
			break;
		case 'p':
			lowprobe |= LOWPROBE_SUPERBLOCKS;
			break;
		case 's':
			if (numtag + 1 >= sizeof(show) / sizeof(*show)) {
				fprintf(stderr, "Too many tags specified\n");
				usage(err);
			}
			show[numtag++] = optarg;
			show[numtag] = NULL;
			break;
		case 'S':
			if (strtosize(optarg, &size))
				fprintf(stderr,
					"Invalid size '%s' specified\n",
					optarg);
			break;
		case 't':
			if (search_type) {
				fprintf(stderr, "Can only search for "
						"one NAME=value pair\n");
				usage(err);
			}
			if (blkid_parse_tag_string(optarg,
						   &search_type,
						   &search_value)) {
				fprintf(stderr, "-t needs NAME=value pair\n");
				usage(err);
			}
			break;
		case 'v':
			version = 1;
			break;
		case 'w':
			/* ignore - backward compatibility */
			break;
		case 'h':
			err = 0;
			/* fallthrough */
		default:
			usage(err);
		}


	/* The rest of the args are device names */
	if (optind < argc) {
		devices = calloc(argc - optind, sizeof(char *));
		if (!devices) {
			fprintf(stderr, "Failed to allocate device name array\n");
			goto exit;
		}

		while (optind < argc)
			devices[numdev++] = argv[optind++];
	}

	if (version) {
		print_version(stdout);
		goto exit;
	}

	/* convert LABEL/UUID lookup to evaluate request */
	if (lookup && output_format == OUTPUT_DEVICE_ONLY && search_type &&
	    (!strcmp(search_type, "LABEL") || !strcmp(search_type, "UUID"))) {
		eval++;
		lookup = 0;
	}

	if (!lowprobe && !eval && blkid_get_cache(&cache, read) < 0)
		goto exit;

	if (gc) {
		blkid_gc_cache(cache);
		err = 0;
		goto exit;
	}
	err = 2;

	if (eval == 0 && (output_format & OUTPUT_PRETTY_LIST)) {
		if (lowprobe) {
			fprintf(stderr, "The low-level probing mode does not "
					"support 'list' output format\n");
			exit(4);
		}
		pretty_print_dev(NULL);
	}

	if (lowprobe) {
		/*
		 * Low-level API
		 */
		blkid_probe pr;

		if (!numdev) {
			fprintf(stderr, "The low-level probing mode "
					"requires a device\n");
			exit(4);
		}

		/* automatically enable 'export' format for I/O Limits */
		if (!output_format  && (lowprobe & LOWPROBE_TOPOLOGY))
			output_format = OUTPUT_EXPORT_LIST;

		pr = blkid_new_probe();
		if (!pr)
			goto exit;

		if (lowprobe & LOWPROBE_SUPERBLOCKS) {
			blkid_probe_set_superblocks_flags(pr,
				BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID |
				BLKID_SUBLKS_TYPE | BLKID_SUBLKS_SECTYPE |
				BLKID_SUBLKS_USAGE | BLKID_SUBLKS_VERSION);

			if (fltr_usage && blkid_probe_filter_superblocks_usage(
						pr, fltr_flag, fltr_usage))
				goto exit;

			else if (fltr_type && blkid_probe_filter_superblocks_type(
						pr, fltr_flag, fltr_type))
				goto exit;
		}

		for (i = 0; i < numdev; i++)
			err = lowprobe_device(pr, devices[i], lowprobe, show,
					output_format,
					(blkid_loff_t) offset,
					(blkid_loff_t) size);
		blkid_free_probe(pr);
	} else if (eval) {
		/*
		 * Evaluate API
		 */
		char *res = blkid_evaluate_tag(search_type, search_value, NULL);
		if (res) {
			err = 0;
			printf("%s\n", res);
		}
	} else if (lookup) {
		/*
		 * Classic (cache based) API
		 */
		blkid_dev dev;

		if (!search_type) {
			fprintf(stderr, "The lookup option requires a "
				"search type specified using -t\n");
			exit(4);
		}
		/* Load any additional devices not in the cache */
		for (i = 0; i < numdev; i++)
			blkid_get_dev(cache, devices[i], BLKID_DEV_NORMAL);

		if ((dev = blkid_find_dev_with_tag(cache, search_type,
						   search_value))) {
			print_tags(dev, show, output_format);
			err = 0;
		}
	/* If we didn't specify a single device, show all available devices */
	} else if (!numdev) {
		blkid_dev_iterate	iter;
		blkid_dev		dev;

		blkid_probe_all(cache);

		iter = blkid_dev_iterate_begin(cache);
		blkid_dev_set_search(iter, search_type, search_value);
		while (blkid_dev_next(iter, &dev) == 0) {
			dev = blkid_verify(cache, dev);
			if (!dev)
				continue;
			print_tags(dev, show, output_format);
			err = 0;
		}
		blkid_dev_iterate_end(iter);
	/* Add all specified devices to cache (optionally display tags) */
	} else for (i = 0; i < numdev; i++) {
		blkid_dev dev = blkid_get_dev(cache, devices[i],
						  BLKID_DEV_NORMAL);

		if (dev) {
			if (search_type &&
			    !blkid_dev_has_tag(dev, search_type,
					       search_value))
				continue;
			print_tags(dev, show, output_format);
			err = 0;
		}
	}

exit:
	free(search_type);
	free(search_value);
	free_types_list(fltr_type);
	if (!lowprobe && !eval)
		blkid_put_cache(cache);
	free(devices);
	return err;
}

/* vi: set sw=4 ts=4: */
/*
 * public domain -- Dave 'Kill a Cop' Cinege <dcinege@psychosis.com>
 *
 * makedevs
 * Make ranges of device files quickly.
 * known bugs: can't deal with alpha ranges
 */

#include "libbb.h"

#if ENABLE_FEATURE_MAKEDEVS_LEAF
/*
makedevs NAME TYPE MAJOR MINOR FIRST LAST [s]
TYPEs:
b       Block device
c       Character device
f       FIFO

FIRST..LAST specify numbers appended to NAME.
If 's' is the last argument, the base device is created as well.
Examples:
        makedevs /dev/ttyS c 4 66 2 63   ->  ttyS2-ttyS63
        makedevs /dev/hda b 3 0 0 8 s    ->  hda,hda1-hda8
*/
int makedevs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int makedevs_main(int argc, char **argv)
{
	mode_t mode;
	char *basedev, *type, *nodname, *buf;
	int Smajor, Sminor, S, E;

	if (argc < 7 || argv[1][0] == '-')
		bb_show_usage();

	basedev = argv[1];
	buf = xasprintf("%s%u", argv[1], (unsigned)-1);
	type = argv[2];
	Smajor = xatoi_u(argv[3]);
	Sminor = xatoi_u(argv[4]);
	S = xatoi_u(argv[5]);
	E = xatoi_u(argv[6]);
	nodname = argv[7] ? basedev : buf;

	mode = 0660;
	switch (type[0]) {
	case 'c':
		mode |= S_IFCHR;
		break;
	case 'b':
		mode |= S_IFBLK;
		break;
	case 'f':
		mode |= S_IFIFO;
		break;
	default:
		bb_show_usage();
	}

	while (S <= E) {
		sprintf(buf, "%s%u", basedev, S);

		/* if mode != S_IFCHR and != S_IFBLK,
		 * third param in mknod() ignored */
		if (mknod(nodname, mode, makedev(Smajor, Sminor)))
			bb_perror_msg("can't create '%s'", nodname);

		/*if (nodname == basedev)*/ /* ex. /dev/hda - to /dev/hda1 ... */
			nodname = buf;
		S++;
		Sminor++;
	}

	return 0;
}

#elif ENABLE_FEATURE_MAKEDEVS_TABLE

/* Licensed under the GPL v2 or later, see the file LICENSE in this tarball. */

int makedevs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int makedevs_main(int argc UNUSED_PARAM, char **argv)
{
	parser_t *parser;
	char *line = (char *)"-";
	int ret = EXIT_SUCCESS;

	opt_complementary = "=1"; /* exactly one param */
	getopt32(argv, "d:", &line);
	argv += optind;

	xchdir(*argv); /* ensure root dir exists */

	umask(0);

	printf("rootdir=%s\ntable=", *argv);
	if (NOT_LONE_DASH(line)) {
		printf("'%s'\n", line);
	} else {
		puts("<stdin>");
	}

	parser = config_open(line);
	while (config_read(parser, &line, 1, 1, "# \t", PARSE_NORMAL)) {
		int linenum;
		char type;
		unsigned mode = 0755;
		unsigned major = 0;
		unsigned minor = 0;
		unsigned count = 0;
		unsigned increment = 0;
		unsigned start = 0;
		char name[41];
		char user[41];
		char group[41];
		char *full_name = name;
		uid_t uid;
		gid_t gid;

		linenum = parser->lineno;

		if ((2 > sscanf(line, "%40s %c %o %40s %40s %u %u %u %u %u",
					name, &type, &mode, user, group,
					&major,	&minor, &start, &increment, &count))
		 || ((unsigned)(major | minor | start | count | increment) > 255)
		) {
			bb_error_msg("invalid line %d: '%s'", linenum, line);
			ret = EXIT_FAILURE;
			continue;
		}

		gid = (*group) ? get_ug_id(group, xgroup2gid) : getgid();
		uid = (*user) ? get_ug_id(user, xuname2uid) : getuid();
		/* We are already in the right root dir,
		 * so make absolute paths relative */
		if ('/' == *full_name)
			full_name++;

		if (type == 'd') {
			bb_make_directory(full_name, mode | S_IFDIR, FILEUTILS_RECUR);
			if (chown(full_name, uid, gid) == -1) {
 chown_fail:
				bb_perror_msg("line %d: can't chown %s", linenum, full_name);
				ret = EXIT_FAILURE;
				continue;
			}
			if (chmod(full_name, mode) < 0) {
 chmod_fail:
				bb_perror_msg("line %d: can't chmod %s", linenum, full_name);
				ret = EXIT_FAILURE;
				continue;
			}
		} else if (type == 'f') {
			struct stat st;
			if ((stat(full_name, &st) < 0 || !S_ISREG(st.st_mode))) {
				bb_perror_msg("line %d: regular file '%s' does not exist", linenum, full_name);
				ret = EXIT_FAILURE;
				continue;
			}
			if (chown(full_name, uid, gid) < 0)
				goto chown_fail;
			if (chmod(full_name, mode) < 0)
				goto chmod_fail;
		} else {
			dev_t rdev;
			unsigned i;
			char *full_name_inc;

			if (type == 'p') {
				mode |= S_IFIFO;
			} else if (type == 'c') {
				mode |= S_IFCHR;
			} else if (type == 'b') {
				mode |= S_IFBLK;
			} else {
				bb_error_msg("line %d: unsupported file type %c", linenum, type);
				ret = EXIT_FAILURE;
				continue;
			}

			full_name_inc = xmalloc(strlen(full_name) + sizeof(int)*3 + 2);
			if (count)
				count--;
			for (i = start; i <= start + count; i++) {
				sprintf(full_name_inc, count ? "%s%u" : "%s", full_name, i);
				rdev = makedev(major, minor + (i - start) * increment);
				if (mknod(full_name_inc, mode, rdev) < 0) {
					bb_perror_msg("line %d: can't create node %s", linenum, full_name_inc);
					ret = EXIT_FAILURE;
				} else if (chown(full_name_inc, uid, gid) < 0) {
					bb_perror_msg("line %d: can't chown %s", linenum, full_name_inc);
					ret = EXIT_FAILURE;
				} else if (chmod(full_name_inc, mode) < 0) {
					bb_perror_msg("line %d: can't chmod %s", linenum, full_name_inc);
					ret = EXIT_FAILURE;
				}
			}
			free(full_name_inc);
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		config_close(parser);

	return ret;
}

#else
# error makedevs configuration error, either leaf or table must be selected
#endif

/* mini man implementation for busybox
 * Copyright (C) 2008 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

enum {
	OPT_a = 1, /* all */
	OPT_w = 2, /* print path */
};

/* This is what I see on my desktop system being executed:

(
echo ".ll 12.4i"
echo ".nr LL 12.4i"
echo ".pl 1100i"
gunzip -c '/usr/man/man1/bzip2.1.gz'
echo ".\\\""
echo ".pl \n(nlu+10"
) | gtbl | nroff -Tlatin1 -mandoc | less

*/

#if ENABLE_FEATURE_SEAMLESS_LZMA
#define Z_SUFFIX ".lzma"
#elif ENABLE_FEATURE_SEAMLESS_BZ2
#define Z_SUFFIX ".bz2"
#elif ENABLE_FEATURE_SEAMLESS_GZ
#define Z_SUFFIX ".gz"
#else
#define Z_SUFFIX ""
#endif

static int show_manpage(const char *pager, char *man_filename, int man, int level);

static int run_pipe(const char *pager, char *man_filename, int man, int level)
{
	char *cmd;

	/* Prevent man page link loops */
	if (level > 10)
		return 0;

	if (access(man_filename, R_OK) != 0)
		return 0;

	if (option_mask32 & OPT_w) {
		puts(man_filename);
		return 1;
	}

	if (man) { /* man page, not cat page */
		/* Is this a link to another manpage? */
		/* The link has the following on the first line: */
		/* ".so another_man_page" */

		struct stat sb;
		char *line;
		char *linkname, *p;

		/* On my system:
		 * man1/genhostid.1.gz: 203 bytes - smallest real manpage
		 * man2/path_resolution.2.gz: 114 bytes - largest link
		 */
		xstat(man_filename, &sb);
		if (sb.st_size > 300) /* err on the safe side */
			goto ordinary_manpage;

		line = xmalloc_open_zipped_read_close(man_filename, NULL);
		if (!line || strncmp(line, ".so ", 4) != 0) {
			free(line);
			goto ordinary_manpage;
		}
		/* Example: man2/path_resolution.2.gz contains
		 * ".so man7/path_resolution.7\n<junk>"
		 */
		*strchrnul(line, '\n') = '\0';
		linkname = skip_whitespace(&line[4]);

		/* If link has no slashes, we just replace man page name.
		 * If link has slashes (however many), we go back *once*.
		 * ".so zzz/ggg/page.3" does NOT go back two levels. */
		p = strrchr(man_filename, '/');
		if (!p)
			goto ordinary_manpage;
		*p = '\0';
		if (strchr(linkname, '/')) {
			p = strrchr(man_filename, '/');
			if (!p)
				goto ordinary_manpage;
			*p = '\0';
		}

		/* Links do not have .gz extensions, even if manpage
		 * is compressed */
		man_filename = xasprintf("%s/%s" Z_SUFFIX, man_filename, linkname);
		free(line);
		/* Note: we leak "new" man_filename string as well... */
		if (show_manpage(pager, man_filename, man, level + 1))
			return 1;
		/* else: show the link, it's better than nothing */
	}

 ordinary_manpage:
	close(STDIN_FILENO);
	open_zipped(man_filename); /* guaranteed to use fd 0 (STDIN_FILENO) */
	/* "2>&1" is added so that nroff errors are shown in pager too.
	 * Otherwise it may show just empty screen */
	cmd = xasprintf(
		man ? "gtbl | nroff -Tlatin1 -mandoc 2>&1 | %s"
		    : "%s",
		pager);
	system(cmd);
	free(cmd);
	return 1;
}

/* man_filename is of the form "/dir/dir/dir/name.s" Z_SUFFIX */
static int show_manpage(const char *pager, char *man_filename, int man, int level)
{
#if ENABLE_FEATURE_SEAMLESS_LZMA
	if (run_pipe(pager, man_filename, man, level))
		return 1;
#endif

#if ENABLE_FEATURE_SEAMLESS_BZ2
#if ENABLE_FEATURE_SEAMLESS_LZMA
	strcpy(strrchr(man_filename, '.') + 1, "bz2");
#endif
	if (run_pipe(pager, man_filename, man, level))
		return 1;
#endif

#if ENABLE_FEATURE_SEAMLESS_GZ
#if ENABLE_FEATURE_SEAMLESS_LZMA || ENABLE_FEATURE_SEAMLESS_BZ2
	strcpy(strrchr(man_filename, '.') + 1, "gz");
#endif
	if (run_pipe(pager, man_filename, man, level))
		return 1;
#endif

#if ENABLE_FEATURE_SEAMLESS_LZMA || ENABLE_FEATURE_SEAMLESS_BZ2 || ENABLE_FEATURE_SEAMLESS_GZ
	*strrchr(man_filename, '.') = '\0';
#endif
	if (run_pipe(pager, man_filename, man, level))
		return 1;

	return 0;
}

int man_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int man_main(int argc UNUSED_PARAM, char **argv)
{
	parser_t *parser;
	const char *pager;
	char **man_path_list;
	char *sec_list;
	char *cur_path, *cur_sect;
	int count_mp, cur_mp;
	int opt, not_found;
	char *token[2];

	opt_complementary = "-1"; /* at least one argument */
	opt = getopt32(argv, "+aw");
	argv += optind;

	sec_list = xstrdup("1:2:3:4:5:6:7:8:9");
	/* Last valid man_path_list[] is [0x10] */
	count_mp = 0;
	man_path_list = xzalloc(0x11 * sizeof(man_path_list[0]));
	man_path_list[0] = getenv("MANPATH");
	if (!man_path_list[0]) /* default, may be overridden by /etc/man.conf */
		man_path_list[0] = (char*)"/usr/man";
	else
		count_mp++;
	pager = getenv("MANPAGER");
	if (!pager) {
		pager = getenv("PAGER");
		if (!pager)
			pager = "more";
	}

	/* Parse man.conf[ig] */
	/* man version 1.6f uses man.config */
	parser = config_open2("/etc/man.config", fopen_for_read);
	if (!parser)
		parser = config_open2("/etc/man.conf", fopen_for_read);

	while (config_read(parser, token, 2, 0, "# \t", PARSE_NORMAL)) {
		if (!token[1])
			continue;
		if (strcmp("MANPATH", token[0]) == 0) {
			char *path = token[1];
			while (*path) {
				char *next_path;
				char **path_element;

				next_path = strchr(path, ':');
				if (next_path) {
					*next_path = '\0';
					if (next_path++ == path) /* "::"? */
						goto next;
				}
				/* Do we already have path? */
				path_element = man_path_list;
				while (*path_element) {
					if (strcmp(*path_element, path) == 0)
						goto skip;
					path_element++;
				}
				man_path_list = xrealloc_vector(man_path_list, 4, count_mp);
				man_path_list[count_mp] = xstrdup(path);
				count_mp++;
				/* man_path_list is NULL terminated */
				/*man_path_list[count_mp] = NULL; - xrealloc_vector did it */
 skip:
				if (!next_path)
					break;
 next:
				path = next_path;
			}
		}
		if (strcmp("MANSECT", token[0]) == 0) {
			free(sec_list);
			sec_list = xstrdup(token[1]);
		}
	}
	config_close(parser);

	not_found = 0;
	do { /* for each argv[] */
		int found = 0;
		cur_mp = 0;

		if (strchr(*argv, '/')) {
			found = show_manpage(pager, *argv, /*man:*/ 1, 0);
			goto check_found;
		}
		while ((cur_path = man_path_list[cur_mp++]) != NULL) {
			/* for each MANPATH */
			cur_sect = sec_list;
			do { /* for each section */
				char *next_sect = strchrnul(cur_sect, ':');
				int sect_len = next_sect - cur_sect;
				char *man_filename;
				int cat0man1 = 0;

				/* Search for cat, then man page */
				while (cat0man1 < 2) {
					int found_here;
					man_filename = xasprintf("%s/%s%.*s/%s.%.*s" Z_SUFFIX,
							cur_path,
							"cat\0man" + (cat0man1 * 4),
							sect_len, cur_sect,
							*argv,
							sect_len, cur_sect);
					found_here = show_manpage(pager, man_filename, cat0man1, 0);
					found |= found_here;
					cat0man1 += found_here + 1;
					free(man_filename);
				}

				if (found && !(opt & OPT_a))
					goto next_arg;
				cur_sect = next_sect;
				while (*cur_sect == ':')
					cur_sect++;
			} while (*cur_sect);
		}
 check_found:
		if (!found) {
			bb_error_msg("no manual entry for '%s'", *argv);
			not_found = 1;
		}
 next_arg:
		argv++;
	} while (*argv);

	return not_found;
}

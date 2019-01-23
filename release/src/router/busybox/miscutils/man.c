/* mini man implementation for busybox
 * Copyright (C) 2008 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//usage:#define man_trivial_usage
//usage:       "[-aw] [MANPAGE]..."
//usage:#define man_full_usage "\n\n"
//usage:       "Format and display manual page\n"
//usage:     "\n	-a	Display all pages"
//usage:     "\n	-w	Show page locations"

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
		if (!line || !is_prefixed_with(line, ".so ")) {
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
		man_filename = xasprintf("%s/%s", man_filename, linkname);
		free(line);
		/* Note: we leak "new" man_filename string as well... */
		if (show_manpage(pager, man_filename, man, level + 1))
			return 1;
		/* else: show the link, it's better than nothing */
	}

 ordinary_manpage:
	close(STDIN_FILENO);
	open_zipped(man_filename, /*fail_if_not_compressed:*/ 0); /* guaranteed to use fd 0 (STDIN_FILENO) */
	/* "2>&1" is added so that nroff errors are shown in pager too.
	 * Otherwise it may show just empty screen */
	cmd = xasprintf(
		/* replaced -Tlatin1 with -Tascii for non-UTF8 displays */
		man ? "gtbl | nroff -Tascii -mandoc 2>&1 | %s"
		    : "%s",
		pager);
	system(cmd);
	free(cmd);
	return 1;
}

/* man_filename is of the form "/dir/dir/dir/name.s" */
static int show_manpage(const char *pager, char *man_filename, int man, int level)
{
#if SEAMLESS_COMPRESSION
	/* We leak this allocation... */
	char *filename_with_zext = xasprintf("%s.lzma", man_filename);
	char *ext = strrchr(filename_with_zext, '.') + 1;
#endif

#if ENABLE_FEATURE_SEAMLESS_LZMA
	if (run_pipe(pager, filename_with_zext, man, level))
		return 1;
#endif
#if ENABLE_FEATURE_SEAMLESS_XZ
	strcpy(ext, "xz");
	if (run_pipe(pager, filename_with_zext, man, level))
		return 1;
#endif
#if ENABLE_FEATURE_SEAMLESS_BZ2
	strcpy(ext, "bz2");
	if (run_pipe(pager, filename_with_zext, man, level))
		return 1;
#endif
#if ENABLE_FEATURE_SEAMLESS_GZ
	strcpy(ext, "gz");
	if (run_pipe(pager, filename_with_zext, man, level))
		return 1;
#endif

	return run_pipe(pager, man_filename, man, level);
}

static char **add_MANPATH(char **man_path_list, int *count_mp, char *path)
{
	if (path) while (*path) {
		char *next_path;
		char **path_element;

		next_path = strchr(path, ':');
		if (next_path) {
			if (next_path == path) /* "::"? */
				goto next;
			*next_path = '\0';
		}
		/* Do we already have path? */
		path_element = man_path_list;
		if (path_element) while (*path_element) {
			if (strcmp(*path_element, path) == 0)
				goto skip;
			path_element++;
		}
		man_path_list = xrealloc_vector(man_path_list, 4, *count_mp);
		man_path_list[*count_mp] = xstrdup(path);
		(*count_mp)++;
		/* man_path_list is NULL terminated */
		/* man_path_list[*count_mp] = NULL; - xrealloc_vector did it */
 skip:
		if (!next_path)
			break;
		/* "path" may be a result of getenv(), be nice and don't mangle it */
		*next_path = ':';
 next:
		path = next_path + 1;
	}
	return man_path_list;
}

int man_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int man_main(int argc UNUSED_PARAM, char **argv)
{
	parser_t *parser;
	const char *pager = ENABLE_LESS ? "less" : "more";
	char *sec_list;
	char *cur_path, *cur_sect;
	char **man_path_list;
	int count_mp;
	int cur_mp;
	int opt, not_found;
	char *token[2];

	opt_complementary = "-1"; /* at least one argument */
	opt = getopt32(argv, "+aw");
	argv += optind;

	sec_list = xstrdup("0p:1:1p:2:3:3p:4:5:6:7:8:9");

	count_mp = 0;
	man_path_list = add_MANPATH(NULL, &count_mp,
			getenv("MANDATORY_MANPATH"+10) /* "MANPATH" */
	);
	if (!man_path_list) {
		/* default, may be overridden by /etc/man.conf */
		man_path_list = xzalloc(2 * sizeof(man_path_list[0]));
		man_path_list[0] = (char*)"/usr/man";
		/* count_mp stays 0.
		 * Thus, man.conf will overwrite man_path_list[0]
		 * if a path is defined there.
		 */
	}

	/* Parse man.conf[ig] or man_db.conf */
	/* man version 1.6f uses man.config */
	/* man-db implementation of man uses man_db.conf */
	parser = config_open2("/etc/man.config", fopen_for_read);
	if (!parser)
		parser = config_open2("/etc/man.conf", fopen_for_read);
	if (!parser)
		parser = config_open2("/etc/man_db.conf", fopen_for_read);

	while (config_read(parser, token, 2, 0, "# \t", PARSE_NORMAL)) {
		if (!token[1])
			continue;
		if (strcmp("DEFINE", token[0]) == 0) {
			if (is_prefixed_with("pager", token[1])) {
				pager = xstrdup(skip_whitespace(token[1]) + 5);
			}
		} else
		if (strcmp("MANDATORY_MANPATH"+10, token[0]) == 0 /* "MANPATH"? */
		 || strcmp("MANDATORY_MANPATH", token[0]) == 0
		) {
			man_path_list = add_MANPATH(man_path_list, &count_mp, token[1]);
		}
		if (strcmp("MANSECT", token[0]) == 0) {
			free(sec_list);
			sec_list = xstrdup(token[1]);
		}
	}
	config_close(parser);

	{
		/* environment overrides setting from man.config */
		char *env_pager = getenv("MANPAGER");
		if (!env_pager)
			env_pager = getenv("PAGER");
		if (env_pager)
			pager = env_pager;
	}

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
					man_filename = xasprintf("%s/%s%.*s/%s.%.*s",
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

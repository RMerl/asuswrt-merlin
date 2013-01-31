/* vi: set sw=4 ts=4: */
/*
 * Sysctl 1.01 - A utility to read and manipulate the sysctl parameters
 *
 * Copyright 1999 George Staikos
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Changelog:
 * v1.01   - added -p <preload> to preload values from a file
 * v1.01.1 - busybox applet aware by <solar@gentoo.org>
 */

//usage:#define sysctl_trivial_usage
//usage:       "[OPTIONS] [VALUE]..."
//usage:#define sysctl_full_usage "\n\n"
//usage:       "Configure kernel parameters at runtime\n"
//usage:     "\n	-n	Don't print key names"
//usage:     "\n	-e	Don't warn about unknown keys"
//usage:     "\n	-w	Change sysctl setting"
//usage:     "\n	-p FILE	Load sysctl settings from FILE (default /etc/sysctl.conf)"
//usage:     "\n	-a	Display all values"
//usage:     "\n	-A	Display all values in table form"
//usage:
//usage:#define sysctl_example_usage
//usage:       "sysctl [-n] [-e] variable...\n"
//usage:       "sysctl [-n] [-e] -w variable=value...\n"
//usage:       "sysctl [-n] [-e] -a\n"
//usage:       "sysctl [-n] [-e] -p file	(default /etc/sysctl.conf)\n"
//usage:       "sysctl [-n] [-e] -A\n"

#include "libbb.h"

enum {
	FLAG_SHOW_KEYS       = 1 << 0,
	FLAG_SHOW_KEY_ERRORS = 1 << 1,
	FLAG_TABLE_FORMAT    = 1 << 2, /* not implemented */
	FLAG_SHOW_ALL        = 1 << 3,
	FLAG_PRELOAD_FILE    = 1 << 4,
	FLAG_WRITE           = 1 << 5,
};
#define OPTION_STR "neAapw"

static void sysctl_dots_to_slashes(char *name)
{
	char *cptr, *last_good, *end;

	/* Convert minimum number of '.' to '/' so that
	 * we end up with existing file's name.
	 *
	 * Example from bug 3894:
	 * net.ipv4.conf.eth0.100.mc_forwarding ->
	 * net/ipv4/conf/eth0.100/mc_forwarding
	 * NB: net/ipv4/conf/eth0/mc_forwarding *also exists*,
	 * therefore we must start from the end, and if
	 * we replaced even one . -> /, start over again,
	 * but never replace dots before the position
	 * where last replacement occurred.
	 *
	 * Another bug we later had is that
	 * net.ipv4.conf.eth0.100
	 * (without .mc_forwarding) was mishandled.
	 *
	 * To set up testing: modprobe 8021q; vconfig add eth0 100
	 */
	end = name + strlen(name);
	last_good = name - 1;
	*end = '.'; /* trick the loop into trying full name too */

 again:
	cptr = end;
	while (cptr > last_good) {
		if (*cptr == '.') {
			*cptr = '\0';
			//bb_error_msg("trying:'%s'", name);
			if (access(name, F_OK) == 0) {
				*cptr = '/';
				//bb_error_msg("replaced:'%s'", name);
				last_good = cptr;
				goto again;
			}
			*cptr = '.';
		}
		cptr--;
	}
	*end = '\0';
}

static int sysctl_act_on_setting(char *setting)
{
	int fd, retval = EXIT_SUCCESS;
	char *cptr, *outname;
	char *value = value; /* for compiler */

	outname = xstrdup(setting);

	cptr = outname;
	while (*cptr) {
		if (*cptr == '/')
			*cptr = '.';
		cptr++;
	}

	if (option_mask32 & FLAG_WRITE) {
		cptr = strchr(setting, '=');
		if (cptr == NULL) {
			bb_error_msg("error: '%s' must be of the form name=value",
				outname);
			retval = EXIT_FAILURE;
			goto end;
		}
		value = cptr + 1;  /* point to the value in name=value */
		if (setting == cptr || !*value) {
			bb_error_msg("error: malformed setting '%s'", outname);
			retval = EXIT_FAILURE;
			goto end;
		}
		*cptr = '\0';
		outname[cptr - setting] = '\0';
		/* procps 3.2.7 actually uses these flags */
		fd = open(setting, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	} else {
		fd = open(setting, O_RDONLY);
	}

	if (fd < 0) {
		switch (errno) {
		case ENOENT:
			if (option_mask32 & FLAG_SHOW_KEY_ERRORS)
				bb_error_msg("error: '%s' is an unknown key", outname);
			break;
		default:
			bb_perror_msg("error %sing key '%s'",
					option_mask32 & FLAG_WRITE ?
						"sett" : "read",
					outname);
			break;
		}
		retval = EXIT_FAILURE;
		goto end;
	}

	if (option_mask32 & FLAG_WRITE) {
//TODO: procps 3.2.7 writes "value\n", note trailing "\n"
		xwrite_str(fd, value);
		close(fd);
		if (option_mask32 & FLAG_SHOW_KEYS)
			printf("%s = ", outname);
		puts(value);
	} else {
		char c;

		value = cptr = xmalloc_read(fd, NULL);
		close(fd);
		if (value == NULL) {
			bb_perror_msg("error reading key '%s'", outname);
			goto end;
		}

		/* dev.cdrom.info and sunrpc.transports, for example,
		 * are multi-line. Try "sysctl sunrpc.transports"
		 */
		while ((c = *cptr) != '\0') {
			if (option_mask32 & FLAG_SHOW_KEYS)
				printf("%s = ", outname);
			while (1) {
				fputc(c, stdout);
				cptr++;
				if (c == '\n')
					break;
				c = *cptr;
				if (c == '\0')
					break;
			}
		}
		free(value);
	}
 end:
	free(outname);
	return retval;
}

static int sysctl_act_recursive(const char *path)
{
	DIR *dirp;
	struct stat buf;
	struct dirent *entry;
	char *next;
	int retval = 0;

	stat(path, &buf);
	if (S_ISDIR(buf.st_mode) && !(option_mask32 & FLAG_WRITE)) {
		dirp = opendir(path);
		if (dirp == NULL)
			return -1;
		while ((entry = readdir(dirp)) != NULL) {
			next = concat_subpath_file(path, entry->d_name);
			if (next == NULL)
				continue; /* d_name is "." or ".." */
			/* if path was ".", drop "./" prefix: */
			retval |= sysctl_act_recursive((next[0] == '.' && next[1] == '/') ?
					    next + 2 : next);
			free(next);
		}
		closedir(dirp);
	} else {
		char *name = xstrdup(path);
		retval |= sysctl_act_on_setting(name);
		free(name);
	}

	return retval;
}

/* Set sysctl's from a conf file. Format example:
 * # Controls IP packet forwarding
 * net.ipv4.ip_forward = 0
 */
static int sysctl_handle_preload_file(const char *filename)
{
	char *token[2];
	parser_t *parser;

	parser = config_open(filename);
	/* Must do it _after_ config_open(): */
	xchdir("/proc/sys");
	/* xchroot("/proc/sys") - if you are paranoid */

//TODO: ';' is comment char too
//TODO: comment may be only at line start. "var=1 #abc" - "1 #abc" is the value
// (but _whitespace_ from ends should be trimmed first (and we do it right))
//TODO: "var==1" is mishandled (must use "=1" as a value, but uses "1")
// can it be fixed by removing PARSE_COLLAPSE bit?
	while (config_read(parser, token, 2, 2, "# \t=", PARSE_NORMAL)) {
		char *tp;
		sysctl_dots_to_slashes(token[0]);
		tp = xasprintf("%s=%s", token[0], token[1]);
		sysctl_act_recursive(tp);
		free(tp);
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		config_close(parser);
	return 0;
}

int sysctl_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sysctl_main(int argc UNUSED_PARAM, char **argv)
{
	int retval;
	int opt;

	opt = getopt32(argv, "+" OPTION_STR); /* '+' - stop on first non-option */
	argv += optind;
	opt ^= (FLAG_SHOW_KEYS | FLAG_SHOW_KEY_ERRORS);
	option_mask32 = opt;

	if (opt & FLAG_PRELOAD_FILE) {
		option_mask32 |= FLAG_WRITE;
		/* xchdir("/proc/sys") is inside */
		return sysctl_handle_preload_file(*argv ? *argv : "/etc/sysctl.conf");
	}
	xchdir("/proc/sys");
	/* xchroot("/proc/sys") - if you are paranoid */
	if (opt & (FLAG_TABLE_FORMAT | FLAG_SHOW_ALL)) {
		return sysctl_act_recursive(".");
	}

	retval = 0;
	while (*argv) {
		sysctl_dots_to_slashes(*argv);
		retval |= sysctl_act_recursive(*argv);
		argv++;
	}

	return retval;
}

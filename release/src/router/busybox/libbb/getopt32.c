/* vi: set sw=4 ts=4: */
/*
 * universal getopt32 implementation for busybox
 *
 * Copyright (C) 2003-2005  Vladimir Oleynik  <dzo@simtreas.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#if ENABLE_LONG_OPTS || ENABLE_FEATURE_GETOPT_LONG
# include <getopt.h>
#endif
#include "libbb.h"

/*      Documentation

uint32_t
getopt32(char **argv, const char *applet_opts, ...)

        The command line options must be declared in const char
        *applet_opts as a string of chars, for example:

        flags = getopt32(argv, "rnug");

        If one of the given options is found, a flag value is added to
        the return value (an unsigned long).

        The flag value is determined by the position of the char in
        applet_opts string.  For example, in the above case:

        flags = getopt32(argv, "rnug");

        "r" will add 1    (bit 0)
        "n" will add 2    (bit 1)
        "u" will add 4    (bit 2)
        "g" will add 8    (bit 3)

        and so on.  You can also look at the return value as a bit
        field and each option sets one bit.

        On exit, global variable optind is set so that if you
        will do argc -= optind; argv += optind; then
        argc will be equal to number of remaining non-option
        arguments, first one would be in argv[0], next in argv[1] and so on
        (options and their parameters will be moved into argv[]
        positions prior to argv[optind]).

 ":"    If one of the options requires an argument, then add a ":"
        after the char in applet_opts and provide a pointer to store
        the argument.  For example:

        char *pointer_to_arg_for_a;
        char *pointer_to_arg_for_b;
        char *pointer_to_arg_for_c;
        char *pointer_to_arg_for_d;

        flags = getopt32(argv, "a:b:c:d:",
                        &pointer_to_arg_for_a, &pointer_to_arg_for_b,
                        &pointer_to_arg_for_c, &pointer_to_arg_for_d);

        The type of the pointer (char* or llist_t*) may be controlled
        by the "::" special separator that is set in the external string
        opt_complementary (see below for more info).

 "::"   If option can have an *optional* argument, then add a "::"
        after its char in applet_opts and provide a pointer to store
        the argument.  Note that optional arguments _must_
        immediately follow the option: -oparam, not -o param.

 "+"    If the first character in the applet_opts string is a plus,
        then option processing will stop as soon as a non-option is
        encountered in the argv array.  Useful for applets like env
        which should not process arguments to subprograms:
        env -i ls -d /
        Here we want env to process just the '-i', not the '-d'.

 "!"    Report bad option, missing required options,
        inconsistent options with all-ones return value (instead of abort).

const char *applet_long_options

        This struct allows you to define long options:

        static const char applet_longopts[] ALIGN1 =
                //"name\0" has_arg val
                "verbose\0" No_argument "v"
                ;
        applet_long_options = applet_longopts;

        The last member of struct option (val) typically is set to
        matching short option from applet_opts. If there is no matching
        char in applet_opts, then:
        - return bit have next position after short options
        - if has_arg is not "No_argument", use ptr for arg also
        - opt_complementary affects it too

        Note: a good applet will make long options configurable via the
        config process and not a required feature.  The current standard
        is to name the config option CONFIG_FEATURE_<applet>_LONG_OPTIONS.

const char *opt_complementary

 ":"    The colon (":") is used to separate groups of two or more chars
        and/or groups of chars and special characters (stating some
        conditions to be checked).

 "abc"  If groups of two or more chars are specified, the first char
        is the main option and the other chars are secondary options.
        Their flags will be turned on if the main option is found even
        if they are not specifed on the command line.  For example:

        opt_complementary = "abc";
        flags = getopt32(argv, "abcd")

        If getopt() finds "-a" on the command line, then
        getopt32's return value will be as if "-a -b -c" were
        found.

 "ww"   Adjacent double options have a counter associated which indicates
        the number of occurrences of the option.
        For example the ps applet needs:
        if w is given once, GNU ps sets the width to 132,
        if w is given more than once, it is "unlimited"

        int w_counter = 0; // must be initialized!
        opt_complementary = "ww";
        getopt32(argv, "w", &w_counter);
        if (w_counter)
                width = (w_counter == 1) ? 132 : INT_MAX;
        else
                get_terminal_width(...&width...);

        w_counter is a pointer to an integer. It has to be passed to
        getopt32() after all other option argument sinks.

        For example: accept multiple -v to indicate the level of verbosity
        and for each -b optarg, add optarg to my_b. Finally, if b is given,
        turn off c and vice versa:

        llist_t *my_b = NULL;
        int verbose_level = 0;
        opt_complementary = "vv:b::b-c:c-b";
        f = getopt32(argv, "vb:c", &my_b, &verbose_level);
        if (f & 2)       // -c after -b unsets -b flag
                while (my_b) dosomething_with(llist_pop(&my_b));
        if (my_b)        // but llist is stored if -b is specified
                free_llist(my_b);
        if (verbose_level) printf("verbose level is %d\n", verbose_level);

Special characters:

 "-"    A group consisting of just a dash forces all arguments
        to be treated as options, even if they have no leading dashes.
        Next char in this case can't be a digit (0-9), use ':' or end of line.
        Example:

        opt_complementary = "-:w-x:x-w"; // "-w-x:x-w" would also work,
        getopt32(argv, "wx");            // but is less readable

        This makes it possible to use options without a dash (./program w x)
        as well as with a dash (./program -x).

        NB: getopt32() will leak a small amount of memory if you use
        this option! Do not use it if there is a possibility of recursive
        getopt32() calls.

 "--"   A double dash at the beginning of opt_complementary means the
        argv[1] string should always be treated as options, even if it isn't
        prefixed with a "-".  This is useful for special syntax in applets
        such as "ar" and "tar":
        tar xvf foo.tar

        NB: getopt32() will leak a small amount of memory if you use
        this option! Do not use it if there is a possibility of recursive
        getopt32() calls.

 "-N"   A dash as the first char in a opt_complementary group followed
        by a single digit (0-9) means that at least N non-option
        arguments must be present on the command line

 "=N"   An equal sign as the first char in a opt_complementary group followed
        by a single digit (0-9) means that exactly N non-option
        arguments must be present on the command line

 "?N"   A "?" as the first char in a opt_complementary group followed
        by a single digit (0-9) means that at most N arguments must be present
        on the command line.

 "V-"   An option with dash before colon or end-of-line results in
        bb_show_usage() being called if this option is encountered.
        This is typically used to implement "print verbose usage message
        and exit" option.

 "a-b"  A dash between two options causes the second of the two
        to be unset (and ignored) if it is given on the command line.

        [FIXME: what if they are the same? like "x-x"? Is it ever useful?]

        For example:
        The du applet has the options "-s" and "-d depth".  If
        getopt32 finds -s, then -d is unset or if it finds -d
        then -s is unset.  (Note:  busybox implements the GNU
        "--max-depth" option as "-d".)  To obtain this behavior, you
        set opt_complementary = "s-d:d-s".  Only one flag value is
        added to getopt32's return value depending on the
        position of the options on the command line.  If one of the
        two options requires an argument pointer (":" in applet_opts
        as in "d:") optarg is set accordingly.

        char *smax_print_depth;

        opt_complementary = "s-d:d-s:x-x";
        opt = getopt32(argv, "sd:x", &smax_print_depth);

        if (opt & 2)
                max_print_depth = atoi(smax_print_depth);
        if (opt & 4)
                printf("Detected odd -x usage\n");

 "a--b" A double dash between two options, or between an option and a group
        of options, means that they are mutually exclusive.  Unlike
        the "-" case above, an error will be forced if the options
        are used together.

        For example:
        The cut applet must have only one type of list specified, so
        -b, -c and -f are mutually exclusive and should raise an error
        if specified together.  In this case you must set
        opt_complementary = "b--cf:c--bf:f--bc".  If two of the
        mutually exclusive options are found, getopt32 will call
        bb_show_usage() and die.

 "x--x" Variation of the above, it means that -x option should occur
        at most once.

 "a+"   A plus after a char in opt_complementary means that the parameter
        for this option is a nonnegative integer. It will be processed
        with xatoi_positive() - allowed range is 0..INT_MAX.

        int param;  // "unsigned param;" will also work
        opt_complementary = "p+";
        getopt32(argv, "p:", &param);

 "a::"  A double colon after a char in opt_complementary means that the
        option can occur multiple times. Each occurrence will be saved as
        a llist_t element instead of char*.

        For example:
        The grep applet can have one or more "-e pattern" arguments.
        In this case you should use getopt32() as follows:

        llist_t *patterns = NULL;

        (this pointer must be initializated to NULL if the list is empty
        as required by llist_add_to_end(llist_t **old_head, char *new_item).)

        opt_complementary = "e::";

        getopt32(argv, "e:", &patterns);
        $ grep -e user -e root /etc/passwd
        root:x:0:0:root:/root:/bin/bash
        user:x:500:500::/home/user:/bin/bash

 "a?b"  A "?" between an option and a group of options means that
        at least one of them is required to occur if the first option
        occurs in preceding command line arguments.

        For example from "id" applet:

        // Don't allow -n -r -rn -ug -rug -nug -rnug
        opt_complementary = "r?ug:n?ug:u--g:g--u";
        flags = getopt32(argv, "rnug");

        This example allowed only:
        $ id; id -u; id -g; id -ru; id -nu; id -rg; id -ng; id -rnu; id -rng

 "X"    A opt_complementary group with just a single letter means
        that this option is required. If more than one such group exists,
        at least one option is required to occur (not all of them).
        For example from "start-stop-daemon" applet:

        // Don't allow -KS -SK, but -S or -K is required
        opt_complementary = "K:S:K--S:S--K";
        flags = getopt32(argv, "KS...);


        Don't forget to use ':'. For example, "?322-22-23X-x-a"
        is interpreted as "?3:22:-2:2-2:2-3Xa:2--x" -
        max 3 args; count uses of '-2'; min 2 args; if there is
        a '-2' option then unset '-3', '-X' and '-a'; if there is
        a '-2' and after it a '-x' then error out.
        But it's far too obfuscated. Use ':' to separate groups.
*/

/* Code here assumes that 'unsigned' is at least 32 bits wide */

const char *const bb_argv_dash[] = { "-", NULL };

const char *opt_complementary;

enum {
	PARAM_STRING,
	PARAM_LIST,
	PARAM_INT,
};

typedef struct {
	unsigned char opt_char;
	smallint param_type;
	unsigned switch_on;
	unsigned switch_off;
	unsigned incongruously;
	unsigned requires;
	void **optarg;  /* char**, llist_t** or int *. */
	int *counter;
} t_complementary;

/* You can set applet_long_options for parse called long options */
#if ENABLE_LONG_OPTS || ENABLE_FEATURE_GETOPT_LONG
static const struct option bb_null_long_options[1] = {
	{ 0, 0, 0, 0 }
};
const char *applet_long_options;
#endif

uint32_t option_mask32;

uint32_t FAST_FUNC
getopt32(char **argv, const char *applet_opts, ...)
{
	int argc;
	unsigned flags = 0;
	unsigned requires = 0;
	t_complementary complementary[33]; /* last stays zero-filled */
	char first_char;
	int c;
	const unsigned char *s;
	t_complementary *on_off;
	va_list p;
#if ENABLE_LONG_OPTS || ENABLE_FEATURE_GETOPT_LONG
	const struct option *l_o;
	struct option *long_options = (struct option *) &bb_null_long_options;
#endif
	unsigned trigger;
	char **pargv;
	int min_arg = 0;
	int max_arg = -1;

#define SHOW_USAGE_IF_ERROR     1
#define ALL_ARGV_IS_OPTS        2
#define FIRST_ARGV_IS_OPT       4

	int spec_flgs = 0;

	/* skip 0: some applets cheat: they do not actually HAVE argv[0] */
	argc = 1;
	while (argv[argc])
		argc++;

	va_start(p, applet_opts);

	c = 0;
	on_off = complementary;
	memset(on_off, 0, sizeof(complementary));

	/* skip bbox extension */
	first_char = applet_opts[0];
	if (first_char == '!')
		applet_opts++;

	/* skip GNU extension */
	s = (const unsigned char *)applet_opts;
	if (*s == '+' || *s == '-')
		s++;
	while (*s) {
		if (c >= 32)
			break;
		on_off->opt_char = *s;
		on_off->switch_on = (1 << c);
		if (*++s == ':') {
			on_off->optarg = va_arg(p, void **);
			while (*++s == ':')
				continue;
		}
		on_off++;
		c++;
	}

#if ENABLE_LONG_OPTS || ENABLE_FEATURE_GETOPT_LONG
	if (applet_long_options) {
		const char *optstr;
		unsigned i, count;

		count = 1;
		optstr = applet_long_options;
		while (optstr[0]) {
			optstr += strlen(optstr) + 3; /* skip NUL, has_arg, val */
			count++;
		}
		/* count == no. of longopts + 1 */
		long_options = alloca(count * sizeof(*long_options));
		memset(long_options, 0, count * sizeof(*long_options));
		i = 0;
		optstr = applet_long_options;
		while (--count) {
			long_options[i].name = optstr;
			optstr += strlen(optstr) + 1;
			long_options[i].has_arg = (unsigned char)(*optstr++);
			/* long_options[i].flag = NULL; */
			long_options[i].val = (unsigned char)(*optstr++);
			i++;
		}
		for (l_o = long_options; l_o->name; l_o++) {
			if (l_o->flag)
				continue;
			for (on_off = complementary; on_off->opt_char; on_off++)
				if (on_off->opt_char == l_o->val)
					goto next_long;
			if (c >= 32)
				break;
			on_off->opt_char = l_o->val;
			on_off->switch_on = (1 << c);
			if (l_o->has_arg != no_argument)
				on_off->optarg = va_arg(p, void **);
			c++;
 next_long: ;
		}
		/* Make it unnecessary to clear applet_long_options
		 * by hand after each call to getopt32
		 */
		applet_long_options = NULL;
	}
#endif /* ENABLE_LONG_OPTS || ENABLE_FEATURE_GETOPT_LONG */
	for (s = (const unsigned char *)opt_complementary; s && *s; s++) {
		t_complementary *pair;
		unsigned *pair_switch;

		if (*s == ':')
			continue;
		c = s[1];
		if (*s == '?') {
			if (c < '0' || c > '9') {
				spec_flgs |= SHOW_USAGE_IF_ERROR;
			} else {
				max_arg = c - '0';
				s++;
			}
			continue;
		}
		if (*s == '-') {
			if (c < '0' || c > '9') {
				if (c == '-') {
					spec_flgs |= FIRST_ARGV_IS_OPT;
					s++;
				} else
					spec_flgs |= ALL_ARGV_IS_OPTS;
			} else {
				min_arg = c - '0';
				s++;
			}
			continue;
		}
		if (*s == '=') {
			min_arg = max_arg = c - '0';
			s++;
			continue;
		}
		for (on_off = complementary; on_off->opt_char; on_off++)
			if (on_off->opt_char == *s)
				goto found_opt;
		/* Without this, diagnostic of such bugs is not easy */
		bb_error_msg_and_die("NO OPT %c!", *s);
 found_opt:
		if (c == ':' && s[2] == ':') {
			on_off->param_type = PARAM_LIST;
			continue;
		}
		if (c == '+' && (s[2] == ':' || s[2] == '\0')) {
			on_off->param_type = PARAM_INT;
			s++;
			continue;
		}
		if (c == ':' || c == '\0') {
			requires |= on_off->switch_on;
			continue;
		}
		if (c == '-' && (s[2] == ':' || s[2] == '\0')) {
			flags |= on_off->switch_on;
			on_off->incongruously |= on_off->switch_on;
			s++;
			continue;
		}
		if (c == *s) {
			on_off->counter = va_arg(p, int *);
			s++;
		}
		pair = on_off;
		pair_switch = &pair->switch_on;
		for (s++; *s && *s != ':'; s++) {
			if (*s == '?') {
				pair_switch = &pair->requires;
			} else if (*s == '-') {
				if (pair_switch == &pair->switch_off)
					pair_switch = &pair->incongruously;
				else
					pair_switch = &pair->switch_off;
			} else {
				for (on_off = complementary; on_off->opt_char; on_off++)
					if (on_off->opt_char == *s) {
						*pair_switch |= on_off->switch_on;
						break;
					}
			}
		}
		s--;
	}
	opt_complementary = NULL;
	va_end(p);

	if (spec_flgs & (FIRST_ARGV_IS_OPT | ALL_ARGV_IS_OPTS)) {
		pargv = argv + 1;
		while (*pargv) {
			if (pargv[0][0] != '-' && pargv[0][0] != '\0') {
				/* Can't use alloca: opts with params will
				 * return pointers to stack!
				 * NB: we leak these allocations... */
				char *pp = xmalloc(strlen(*pargv) + 2);
				*pp = '-';
				strcpy(pp + 1, *pargv);
				*pargv = pp;
			}
			if (!(spec_flgs & ALL_ARGV_IS_OPTS))
				break;
			pargv++;
		}
	}

	/* In case getopt32 was already called:
	 * reset the libc getopt() function, which keeps internal state.
	 * run_nofork_applet() does this, but we might end up here
	 * also via gunzip_main() -> gzip_main(). Play safe.
	 */
#ifdef __GLIBC__
	optind = 0;
#else /* BSD style */
	optind = 1;
	/* optreset = 1; */
#endif
	/* optarg = NULL; opterr = 0; optopt = 0; - do we need this?? */

	/* Note: just "getopt() <= 0" will not work well for
	 * "fake" short options, like this one:
	 * wget $'-\203' "Test: test" http://kernel.org/
	 * (supposed to act as --header, but doesn't) */
#if ENABLE_LONG_OPTS || ENABLE_FEATURE_GETOPT_LONG
	while ((c = getopt_long(argc, argv, applet_opts,
			long_options, NULL)) != -1) {
#else
	while ((c = getopt(argc, argv, applet_opts)) != -1) {
#endif
		/* getopt prints "option requires an argument -- X"
		 * and returns '?' if an option has no arg, but one is reqd */
		c &= 0xff; /* fight libc's sign extension */
		for (on_off = complementary; on_off->opt_char != c; on_off++) {
			/* c can be NUL if long opt has non-NULL ->flag,
			 * but we construct long opts so that flag
			 * is always NULL (see above) */
			if (on_off->opt_char == '\0' /* && c != '\0' */) {
				/* c is probably '?' - "bad option" */
				goto error;
			}
		}
		if (flags & on_off->incongruously)
			goto error;
		trigger = on_off->switch_on & on_off->switch_off;
		flags &= ~(on_off->switch_off ^ trigger);
		flags |= on_off->switch_on ^ trigger;
		flags ^= trigger;
		if (on_off->counter)
			(*(on_off->counter))++;
		if (optarg) {
			if (on_off->param_type == PARAM_LIST) {
				llist_add_to_end((llist_t **)(on_off->optarg), optarg);
			} else if (on_off->param_type == PARAM_INT) {
//TODO: xatoi_positive indirectly pulls in printf machinery
				*(unsigned*)(on_off->optarg) = xatoi_positive(optarg);
			} else if (on_off->optarg) {
				*(char **)(on_off->optarg) = optarg;
			}
		}
	}

	/* check depending requires for given options */
	for (on_off = complementary; on_off->opt_char; on_off++) {
		if (on_off->requires
		 && (flags & on_off->switch_on)
		 && (flags & on_off->requires) == 0
		) {
			goto error;
		}
	}
	if (requires && (flags & requires) == 0)
		goto error;
	argc -= optind;
	if (argc < min_arg || (max_arg >= 0 && argc > max_arg))
		goto error;

	option_mask32 = flags;
	return flags;

 error:
	if (first_char != '!')
		bb_show_usage();
	return (int32_t)-1;
}

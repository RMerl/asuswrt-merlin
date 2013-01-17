/* vi: set sw=4 ts=4: */
/*
 * simplified modprobe
 *
 * Copyright (c) 2008 Vladimir Dronnikov
 * Copyright (c) 2008 Bernhard Reutner-Fischer (initial depmod code)
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//applet:IF_MODPROBE_SMALL(APPLET(modprobe, BB_DIR_SBIN, BB_SUID_DROP))
//applet:IF_MODPROBE_SMALL(APPLET_ODDNAME(depmod, modprobe, BB_DIR_SBIN, BB_SUID_DROP, modprobe))
//applet:IF_MODPROBE_SMALL(APPLET_ODDNAME(insmod, modprobe, BB_DIR_SBIN, BB_SUID_DROP, modprobe))
//applet:IF_MODPROBE_SMALL(APPLET_ODDNAME(lsmod, modprobe, BB_DIR_SBIN, BB_SUID_DROP, modprobe))
//applet:IF_MODPROBE_SMALL(APPLET_ODDNAME(rmmod, modprobe, BB_DIR_SBIN, BB_SUID_DROP, modprobe))

#include "libbb.h"
/* After libbb.h, since it needs sys/types.h on some systems */
#include <sys/utsname.h> /* uname() */
#include <fnmatch.h>

extern int init_module(void *module, unsigned long len, const char *options);
extern int delete_module(const char *module, unsigned flags);
extern int query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret);


#if 1
# define dbg1_error_msg(...) ((void)0)
# define dbg2_error_msg(...) ((void)0)
#else
# define dbg1_error_msg(...) bb_error_msg(__VA_ARGS__)
# define dbg2_error_msg(...) bb_error_msg(__VA_ARGS__)
#endif

#define DEPFILE_BB CONFIG_DEFAULT_DEPMOD_FILE".bb"

enum {
	OPT_q = (1 << 0), /* be quiet */
	OPT_r = (1 << 1), /* module removal instead of loading */
};

typedef struct module_info {
	char *pathname;
	char *aliases;
	char *deps;
} module_info;

/*
 * GLOBALS
 */
struct globals {
	module_info *modinfo;
	char *module_load_options;
	smallint dep_bb_seen;
	smallint wrote_dep_bb_ok;
	unsigned module_count;
	int module_found_idx;
	unsigned stringbuf_idx;
	unsigned stringbuf_size;
	char *stringbuf; /* some modules have lots of stuff */
	/* for example, drivers/media/video/saa7134/saa7134.ko */
	/* therefore having a fixed biggish buffer is not wise */
};
#define G (*ptr_to_globals)
#define modinfo             (G.modinfo            )
#define dep_bb_seen         (G.dep_bb_seen        )
#define wrote_dep_bb_ok     (G.wrote_dep_bb_ok    )
#define module_count        (G.module_count       )
#define module_found_idx    (G.module_found_idx   )
#define module_load_options (G.module_load_options)
#define stringbuf_idx       (G.stringbuf_idx      )
#define stringbuf_size      (G.stringbuf_size     )
#define stringbuf           (G.stringbuf          )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

static void append(const char *s)
{
	unsigned len = strlen(s);
	if (stringbuf_idx + len + 15 > stringbuf_size) {
		stringbuf_size = stringbuf_idx + len + 127;
		dbg2_error_msg("grow stringbuf to %u", stringbuf_size);
		stringbuf = xrealloc(stringbuf, stringbuf_size);
	}
	memcpy(stringbuf + stringbuf_idx, s, len);
	stringbuf_idx += len;
}

static void appendc(char c)
{
	/* We appendc() only after append(), + 15 trick in append()
	 * makes it unnecessary to check for overflow here */
	stringbuf[stringbuf_idx++] = c;
}

static void bksp(void)
{
	if (stringbuf_idx)
		stringbuf_idx--;
}

static void reset_stringbuf(void)
{
	stringbuf_idx = 0;
}

static char* copy_stringbuf(void)
{
	char *copy = xzalloc(stringbuf_idx + 1); /* terminating NUL */
	return memcpy(copy, stringbuf, stringbuf_idx);
}

static char* find_keyword(char *ptr, size_t len, const char *word)
{
	int wlen;

	if (!ptr) /* happens if xmalloc_open_zipped_read_close cannot read it */
		return NULL;

	wlen = strlen(word);
	len -= wlen - 1;
	while ((ssize_t)len > 0) {
		char *old = ptr;
		/* search for the first char in word */
		ptr = memchr(ptr, *word, len);
		if (ptr == NULL) /* no occurance left, done */
			break;
		if (strncmp(ptr, word, wlen) == 0)
			return ptr + wlen; /* found, return ptr past it */
		++ptr;
		len -= (ptr - old);
	}
	return NULL;
}

static void replace(char *s, char what, char with)
{
	while (*s) {
		if (what == *s)
			*s = with;
		++s;
	}
}

/* Take "word word", return malloced "word",NUL,"word",NUL,NUL */
static char* str_2_list(const char *str)
{
	int len = strlen(str) + 1;
	char *dst = xmalloc(len + 1);

	dst[len] = '\0';
	memcpy(dst, str, len);
//TODO: protect against 2+ spaces: "word  word"
	replace(dst, ' ', '\0');
	return dst;
}

/* We use error numbers in a loose translation... */
static const char *moderror(int err)
{
	switch (err) {
	case ENOEXEC:
		return "invalid module format";
	case ENOENT:
		return "unknown symbol in module or invalid parameter";
	case ESRCH:
		return "module has wrong symbol version";
	case EINVAL: /* "invalid parameter" */
		return "unknown symbol in module or invalid parameter"
		+ sizeof("unknown symbol in module or");
	default:
		return strerror(err);
	}
}

static int load_module(const char *fname, const char *options)
{
#if 1
	int r;
	size_t len = MAXINT(ssize_t);
	char *module_image;
	dbg1_error_msg("load_module('%s','%s')", fname, options);

	module_image = xmalloc_open_zipped_read_close(fname, &len);
	r = (!module_image || init_module(module_image, len, options ? options : "") != 0);
	free(module_image);
	dbg1_error_msg("load_module:%d", r);
	return r; /* 0 = success */
#else
	/* For testing */
	dbg1_error_msg("load_module('%s','%s')", fname, options);
	return 1;
#endif
}

static void parse_module(module_info *info, const char *pathname)
{
	char *module_image;
	char *ptr;
	size_t len;
	size_t pos;
	dbg1_error_msg("parse_module('%s')", pathname);

	/* Read (possibly compressed) module */
	len = 64 * 1024 * 1024; /* 64 Mb at most */
	module_image = xmalloc_open_zipped_read_close(pathname, &len);
	/* module_image == NULL is ok here, find_keyword handles it */
//TODO: optimize redundant module body reads

	/* "alias1 symbol:sym1 alias2 symbol:sym2" */
	reset_stringbuf();
	pos = 0;
	while (1) {
		ptr = find_keyword(module_image + pos, len - pos, "alias=");
		if (!ptr) {
			ptr = find_keyword(module_image + pos, len - pos, "__ksymtab_");
			if (!ptr)
				break;
			/* DOCME: __ksymtab_gpl and __ksymtab_strings occur
			 * in many modules. What do they mean? */
			if (strcmp(ptr, "gpl") == 0 || strcmp(ptr, "strings") == 0)
				goto skip;
			dbg2_error_msg("alias:'symbol:%s'", ptr);
			append("symbol:");
		} else {
			dbg2_error_msg("alias:'%s'", ptr);
		}
		append(ptr);
		appendc(' ');
 skip:
		pos = (ptr - module_image);
	}
	bksp(); /* remove last ' ' */
	info->aliases = copy_stringbuf();
	replace(info->aliases, '-', '_');

	/* "dependency1 depandency2" */
	reset_stringbuf();
	ptr = find_keyword(module_image, len, "depends=");
	if (ptr && *ptr) {
		replace(ptr, ',', ' ');
		replace(ptr, '-', '_');
		dbg2_error_msg("dep:'%s'", ptr);
		append(ptr);
	}
	info->deps = copy_stringbuf();

	free(module_image);
}

static int pathname_matches_modname(const char *pathname, const char *modname)
{
	const char *fname = bb_get_last_path_component_nostrip(pathname);
	const char *suffix = strrstr(fname, ".ko");
//TODO: can do without malloc?
	char *name = xstrndup(fname, suffix - fname);
	int r;
	replace(name, '-', '_');
	r = (strcmp(name, modname) == 0);
	free(name);
	return r;
}

static FAST_FUNC int fileAction(const char *pathname,
		struct stat *sb UNUSED_PARAM,
		void *modname_to_match,
		int depth UNUSED_PARAM)
{
	int cur;
	const char *fname;

	pathname += 2; /* skip "./" */
	fname = bb_get_last_path_component_nostrip(pathname);
	if (!strrstr(fname, ".ko")) {
		dbg1_error_msg("'%s' is not a module", pathname);
		return TRUE; /* not a module, continue search */
	}

	cur = module_count++;
	modinfo = xrealloc_vector(modinfo, 12, cur);
	modinfo[cur].pathname = xstrdup(pathname);
	/*modinfo[cur].aliases = NULL; - xrealloc_vector did it */
	/*modinfo[cur+1].pathname = NULL;*/

	if (!pathname_matches_modname(fname, modname_to_match)) {
		dbg1_error_msg("'%s' module name doesn't match", pathname);
		return TRUE; /* module name doesn't match, continue search */
	}

	dbg1_error_msg("'%s' module name matches", pathname);
	module_found_idx = cur;
	parse_module(&modinfo[cur], pathname);

	if (!(option_mask32 & OPT_r)) {
		if (load_module(pathname, module_load_options) == 0) {
			/* Load was successful, there is nothing else to do.
			 * This can happen ONLY for "top-level" module load,
			 * not a dep, because deps dont do dirscan. */
			exit(EXIT_SUCCESS);
		}
	}

	return TRUE;
}

static int load_dep_bb(void)
{
	char *line;
	FILE *fp = fopen_for_read(DEPFILE_BB);

	if (!fp)
		return 0;

	dep_bb_seen = 1;
	dbg1_error_msg("loading "DEPFILE_BB);

	/* Why? There is a rare scenario: we did not find modprobe.dep.bb,
	 * we scanned the dir and found no module by name, then we search
	 * for alias (full scan), and we decided to generate modprobe.dep.bb.
	 * But we see modprobe.dep.bb.new! Other modprobe is at work!
	 * We wait and other modprobe renames it to modprobe.dep.bb.
	 * Now we can use it.
	 * But we already have modinfo[] filled, and "module_count = 0"
	 * makes us start anew. Yes, we leak modinfo[].xxx pointers -
	 * there is not much of data there anyway. */
	module_count = 0;
	memset(&modinfo[0], 0, sizeof(modinfo[0]));

	while ((line = xmalloc_fgetline(fp)) != NULL) {
		char* space;
		char* linebuf;
		int cur;

		if (!line[0]) {
			free(line);
			continue;
		}
		space = strchrnul(line, ' ');
		cur = module_count++;
		modinfo = xrealloc_vector(modinfo, 12, cur);
		/*modinfo[cur+1].pathname = NULL; - xrealloc_vector did it */
		modinfo[cur].pathname = line; /* we take ownership of malloced block here */
		if (*space)
			*space++ = '\0';
		modinfo[cur].aliases = space;
		linebuf = xmalloc_fgetline(fp);
		modinfo[cur].deps = linebuf ? linebuf : xzalloc(1);
		if (modinfo[cur].deps[0]) {
			/* deps are not "", so next line must be empty */
			line = xmalloc_fgetline(fp);
			/* Refuse to work with damaged config file */
			if (line && line[0])
				bb_error_msg_and_die("error in %s at '%s'", DEPFILE_BB, line);
			free(line);
		}
	}
	return 1;
}

static int start_dep_bb_writeout(void)
{
	int fd;

	/* depmod -n: write result to stdout */
	if (applet_name[0] == 'd' && (option_mask32 & 1))
		return STDOUT_FILENO;

	fd = open(DEPFILE_BB".new", O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0644);
	if (fd < 0) {
		if (errno == EEXIST) {
			int count = 5 * 20;
			dbg1_error_msg(DEPFILE_BB".new exists, waiting for "DEPFILE_BB);
			while (1) {
				usleep(1000*1000 / 20);
				if (load_dep_bb()) {
					dbg1_error_msg(DEPFILE_BB" appeared");
					return -2; /* magic number */
				}
				if (!--count)
					break;
			}
			bb_error_msg("deleting stale %s", DEPFILE_BB".new");
			fd = open_or_warn(DEPFILE_BB".new", O_WRONLY | O_CREAT | O_TRUNC);
		}
	}
	dbg1_error_msg("opened "DEPFILE_BB".new:%d", fd);
	return fd;
}

static void write_out_dep_bb(int fd)
{
	int i;
	FILE *fp;

	/* We want good error reporting. fdprintf is not good enough. */
	fp = xfdopen_for_write(fd);
	i = 0;
	while (modinfo[i].pathname) {
		fprintf(fp, "%s%s%s\n" "%s%s\n",
			modinfo[i].pathname, modinfo[i].aliases[0] ? " " : "", modinfo[i].aliases,
			modinfo[i].deps, modinfo[i].deps[0] ? "\n" : "");
		i++;
	}
	/* Badly formatted depfile is a no-no. Be paranoid. */
	errno = 0;
	if (ferror(fp) | fclose(fp)) /* | instead of || is intended */
		goto err;

	if (fd == STDOUT_FILENO) /* it was depmod -n */
		goto ok;

	if (rename(DEPFILE_BB".new", DEPFILE_BB) != 0) {
 err:
		bb_perror_msg("can't create '%s'", DEPFILE_BB);
		unlink(DEPFILE_BB".new");
	} else {
 ok:
		wrote_dep_bb_ok = 1;
		dbg1_error_msg("created "DEPFILE_BB);
	}
}

static module_info* find_alias(const char *alias)
{
	int i;
	int dep_bb_fd;
	module_info *result;
	dbg1_error_msg("find_alias('%s')", alias);

 try_again:
	/* First try to find by name (cheaper) */
	i = 0;
	while (modinfo[i].pathname) {
		if (pathname_matches_modname(modinfo[i].pathname, alias)) {
			dbg1_error_msg("found '%s' in module '%s'",
					alias, modinfo[i].pathname);
			if (!modinfo[i].aliases) {
				parse_module(&modinfo[i], modinfo[i].pathname);
			}
			return &modinfo[i];
		}
		i++;
	}

	/* Ok, we definitely have to scan module bodies. This is a good
	 * moment to generate modprobe.dep.bb, if it does not exist yet */
	dep_bb_fd = dep_bb_seen ? -1 : start_dep_bb_writeout();
	if (dep_bb_fd == -2) /* modprobe.dep.bb appeared? */
		goto try_again;

	/* Scan all module bodies, extract modinfo (it contains aliases) */
	i = 0;
	result = NULL;
	while (modinfo[i].pathname) {
		char *desc, *s;
		if (!modinfo[i].aliases) {
			parse_module(&modinfo[i], modinfo[i].pathname);
		}
		if (result) {
			i++;
			continue;
		}
		/* "alias1 symbol:sym1 alias2 symbol:sym2" */
		desc = str_2_list(modinfo[i].aliases);
		/* Does matching substring exist? */
		for (s = desc; *s; s += strlen(s) + 1) {
			/* Aliases in module bodies can be defined with
			 * shell patterns. Example:
			 * "pci:v000010DEd000000D9sv*sd*bc*sc*i*".
			 * Plain strcmp() won't catch that */
			if (fnmatch(s, alias, 0) == 0) {
				dbg1_error_msg("found alias '%s' in module '%s'",
						alias, modinfo[i].pathname);
				result = &modinfo[i];
				break;
			}
		}
		free(desc);
		if (result && dep_bb_fd < 0)
			return result;
		i++;
	}

	/* Create module.dep.bb if needed */
	if (dep_bb_fd >= 0) {
		write_out_dep_bb(dep_bb_fd);
	}

	dbg1_error_msg("find_alias '%s' returns %p", alias, result);
	return result;
}

#if ENABLE_FEATURE_MODPROBE_SMALL_CHECK_ALREADY_LOADED
// TODO: open only once, invent config_rewind()
static int already_loaded(const char *name)
{
	int ret = 0;
	char *s;
	parser_t *parser = config_open2("/proc/modules", xfopen_for_read);
	while (config_read(parser, &s, 1, 1, "# \t", PARSE_NORMAL & ~PARSE_GREEDY)) {
		if (strcmp(s, name) == 0) {
			ret = 1;
			break;
		}
	}
	config_close(parser);
	return ret;
}
#else
#define already_loaded(name) is_rmmod
#endif

/*
 * Given modules definition and module name (or alias, or symbol)
 * load/remove the module respecting dependencies.
 * NB: also called by depmod with bogus name "/",
 * just in order to force modprobe.dep.bb creation.
*/
#if !ENABLE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE
#define process_module(a,b) process_module(a)
#define cmdline_options ""
#endif
static void process_module(char *name, const char *cmdline_options)
{
	char *s, *deps, *options;
	module_info *info;
	int is_rmmod = (option_mask32 & OPT_r) != 0;
	dbg1_error_msg("process_module('%s','%s')", name, cmdline_options);

	replace(name, '-', '_');

	dbg1_error_msg("already_loaded:%d is_rmmod:%d", already_loaded(name), is_rmmod);
	if (already_loaded(name) != is_rmmod) {
		dbg1_error_msg("nothing to do for '%s'", name);
		return;
	}

	options = NULL;
	if (!is_rmmod) {
		char *opt_filename = xasprintf("/etc/modules/%s", name);
		options = xmalloc_open_read_close(opt_filename, NULL);
		if (options)
			replace(options, '\n', ' ');
#if ENABLE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE
		if (cmdline_options) {
			/* NB: cmdline_options always have one leading ' '
			 * (see main()), we remove it here */
			char *op = xasprintf(options ? "%s %s" : "%s %s" + 3,
						cmdline_options + 1, options);
			free(options);
			options = op;
		}
#endif
		free(opt_filename);
		module_load_options = options;
		dbg1_error_msg("process_module('%s'): options:'%s'", name, options);
	}

	if (!module_count) {
		/* Scan module directory. This is done only once.
		 * It will attempt module load, and will exit(EXIT_SUCCESS)
		 * on success. */
		module_found_idx = -1;
		recursive_action(".",
			ACTION_RECURSE, /* flags */
			fileAction, /* file action */
			NULL, /* dir action */
			name, /* user data */
			0); /* depth */
		dbg1_error_msg("dirscan complete");
		/* Module was not found, or load failed, or is_rmmod */
		if (module_found_idx >= 0) { /* module was found */
			info = &modinfo[module_found_idx];
		} else { /* search for alias, not a plain module name */
			info = find_alias(name);
		}
	} else {
		info = find_alias(name);
	}

// Problem here: there can be more than one module
// for the given alias. For example,
// "pci:v00008086d00007010sv00000000sd00000000bc01sc01i80" matches
// ata_piix because it has an alias "pci:v00008086d00007010sv*sd*bc*sc*i*"
// and ata_generic, it has an alias "alias=pci:v*d*sv*sd*bc01sc01i*"
// Standard modprobe would load them both.
// In this code, find_alias() returns only the first matching module.

	/* rmmod? unload it by name */
	if (is_rmmod) {
		if (delete_module(name, O_NONBLOCK | O_EXCL) != 0) {
			if (!(option_mask32 & OPT_q))
				bb_perror_msg("remove '%s'", name);
			goto ret;
		}
		/* N.B. we do not stop here -
		 * continue to unload modules on which the module depends:
		 * "-r --remove: option causes modprobe to remove a module.
		 * If the modules it depends on are also unused, modprobe
		 * will try to remove them, too." */
	}

	if (!info) {
		/* both dirscan and find_alias found nothing */
		if (!is_rmmod && applet_name[0] != 'd') /* it wasn't rmmod or depmod */
			bb_error_msg("module '%s' not found", name);
//TODO: _and_die()? or should we continue (un)loading modules listed on cmdline?
		goto ret;
	}

	/* Iterate thru dependencies, trying to (un)load them */
	deps = str_2_list(info->deps);
	for (s = deps; *s; s += strlen(s) + 1) {
		//if (strcmp(name, s) != 0) // N.B. do loops exist?
		dbg1_error_msg("recurse on dep '%s'", s);
		process_module(s, NULL);
		dbg1_error_msg("recurse on dep '%s' done", s);
	}
	free(deps);

	/* modprobe -> load it */
	if (!is_rmmod) {
		if (!options || strstr(options, "blacklist") == NULL) {
			errno = 0;
			if (load_module(info->pathname, options) != 0) {
				if (EEXIST != errno) {
					bb_error_msg("'%s': %s",
						info->pathname,
						moderror(errno));
				} else {
					dbg1_error_msg("'%s': %s",
						info->pathname,
						moderror(errno));
				}
			}
		} else {
			dbg1_error_msg("'%s': blacklisted", info->pathname);
		}
	}
 ret:
	free(options);
//TODO: return load attempt result from process_module.
//If dep didn't load ok, continuing makes little sense.
}
#undef cmdline_options


/* For reference, module-init-tools v3.4 options:

# insmod
Usage: insmod filename [args]

# rmmod --help
Usage: rmmod [-fhswvV] modulename ...
 -f (or --force) forces a module unload, and may crash your
    machine. This requires the Forced Module Removal option
    when the kernel was compiled.
 -h (or --help) prints this help text
 -s (or --syslog) says use syslog, not stderr
 -v (or --verbose) enables more messages
 -V (or --version) prints the version code
 -w (or --wait) begins module removal even if it is used
    and will stop new users from accessing the module (so it
    should eventually fall to zero).

# modprobe
Usage: modprobe [-v] [-V] [-C config-file] [-n] [-i] [-q] [-b]
    [-o <modname>] [ --dump-modversions ] <modname> [parameters...]
modprobe -r [-n] [-i] [-v] <modulename> ...
modprobe -l -t <dirname> [ -a <modulename> ...]

# depmod --help
depmod 3.4 -- part of module-init-tools
depmod -[aA] [-n -e -v -q -V -r -u]
      [-b basedirectory] [forced_version]
depmod [-n -e -v -q -r -u] [-F kernelsyms] module1.ko module2.ko ...
If no arguments (except options) are given, "depmod -a" is assumed.
depmod will output a dependency list suitable for the modprobe utility.
Options:
    -a, --all           Probe all modules
    -A, --quick         Only does the work if there's a new module
    -n, --show          Write the dependency file on stdout only
    -e, --errsyms       Report not supplied symbols
    -V, --version       Print the release version
    -v, --verbose       Enable verbose mode
    -h, --help          Print this usage message
The following options are useful for people managing distributions:
    -b basedirectory
    --basedir basedirectory
                        Use an image of a module tree
    -F kernelsyms
    --filesyms kernelsyms
                        Use the file instead of the current kernel symbols
*/

//usage:#if ENABLE_MODPROBE_SMALL

//// Note: currently, help system shows modprobe --help text for all aliased cmds
//// (see APPLET_ODDNAME macro definition).
//// All other help texts defined below are not used. FIXME?

//usage:#define depmod_trivial_usage NOUSAGE_STR
//usage:#define depmod_full_usage ""

//usage:#define lsmod_trivial_usage
//usage:       ""
//usage:#define lsmod_full_usage "\n\n"
//usage:       "List the currently loaded kernel modules"

//usage:#define insmod_trivial_usage
//usage:	IF_FEATURE_2_4_MODULES("[OPTIONS] MODULE ")
//usage:	IF_NOT_FEATURE_2_4_MODULES("FILE ")
//usage:	"[SYMBOL=VALUE]..."
//usage:#define insmod_full_usage "\n\n"
//usage:       "Load the specified kernel modules into the kernel"
//usage:	IF_FEATURE_2_4_MODULES( "\n"
//usage:     "\n	-f	Force module to load into the wrong kernel version"
//usage:     "\n	-k	Make module autoclean-able"
//usage:     "\n	-v	Verbose"
//usage:     "\n	-q	Quiet"
//usage:     "\n	-L	Lock: prevent simultaneous loads"
//usage:	IF_FEATURE_INSMOD_LOAD_MAP(
//usage:     "\n	-m	Output load map to stdout"
//usage:	)
//usage:     "\n	-x	Don't export externs"
//usage:	)

//usage:#define rmmod_trivial_usage
//usage:       "[-wfa] [MODULE]..."
//usage:#define rmmod_full_usage "\n\n"
//usage:       "Unload kernel modules\n"
//usage:     "\n	-w	Wait until the module is no longer used"
//usage:     "\n	-f	Force unload"
//usage:     "\n	-a	Remove all unused modules (recursively)"
//usage:
//usage:#define rmmod_example_usage
//usage:       "$ rmmod tulip\n"

//usage:#define modprobe_trivial_usage
//usage:	"[-qfwrsv] MODULE [symbol=value]..."
//usage:#define modprobe_full_usage "\n\n"
//usage:       "	-r	Remove MODULE (stacks) or do autoclean"
//usage:     "\n	-q	Quiet"
//usage:     "\n	-v	Verbose"
//usage:     "\n	-f	Force"
//usage:     "\n	-w	Wait for unload"
//usage:     "\n	-s	Report via syslog instead of stderr"

//usage:#endif

int modprobe_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int modprobe_main(int argc UNUSED_PARAM, char **argv)
{
	struct utsname uts;
	char applet0 = applet_name[0];
	IF_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE(char *options;)

	/* are we lsmod? -> just dump /proc/modules */
	if ('l' == applet0) {
		xprint_and_close_file(xfopen_for_read("/proc/modules"));
		return EXIT_SUCCESS;
	}

	INIT_G();

	/* Prevent ugly corner cases with no modules at all */
	modinfo = xzalloc(sizeof(modinfo[0]));

	if ('i' != applet0) { /* not insmod */
		/* Goto modules directory */
		xchdir(CONFIG_DEFAULT_MODULES_DIR);
	}
	uname(&uts); /* never fails */

	/* depmod? */
	if ('d' == applet0) {
		/* Supported:
		 * -n: print result to stdout
		 * -a: process all modules (default)
		 * optional VERSION parameter
		 * Ignored:
		 * -A: do work only if a module is newer than depfile
		 * -e: report any symbols which a module needs
		 *  which are not supplied by other modules or the kernel
		 * -F FILE: System.map (symbols for -e)
		 * -q, -r, -u: noop?
		 * Not supported:
		 * -b BASEDIR: (TODO!) modules are in
		 *  $BASEDIR/lib/modules/$VERSION
		 * -v: human readable deps to stdout
		 * -V: version (don't want to support it - people may depend
		 *  on it as an indicator of "standard" depmod)
		 * -h: help (well duh)
		 * module1.o module2.o parameters (just ignored for now)
		 */
		getopt32(argv, "na" "AeF:qru" /* "b:vV", NULL */, NULL);
		argv += optind;
		/* if (argv[0] && argv[1]) bb_show_usage(); */
		/* Goto $VERSION directory */
		xchdir(argv[0] ? argv[0] : uts.release);
		/* Force full module scan by asking to find a bogus module.
		 * This will generate modules.dep.bb as a side effect. */
		process_module((char*)"/", NULL);
		return !wrote_dep_bb_ok;
	}

	/* insmod, modprobe, rmmod require at least one argument */
	opt_complementary = "-1";
	/* only -q (quiet) and -r (rmmod),
	 * the rest are accepted and ignored (compat) */
	getopt32(argv, "qrfsvwb");
	argv += optind;

	/* are we rmmod? -> simulate modprobe -r */
	if ('r' == applet0) {
		option_mask32 |= OPT_r;
	}

	if ('i' != applet0) { /* not insmod */
		/* Goto $VERSION directory */
		xchdir(uts.release);
	}

#if ENABLE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE
	/* If not rmmod, parse possible module options given on command line.
	 * insmod/modprobe takes one module name, the rest are parameters. */
	options = NULL;
	if ('r' != applet0) {
		char **arg = argv;
		while (*++arg) {
			/* Enclose options in quotes */
			char *s = options;
			options = xasprintf("%s \"%s\"", s ? s : "", *arg);
			free(s);
			*arg = NULL;
		}
	}
#else
	if ('r' != applet0)
		argv[1] = NULL;
#endif

	if ('i' == applet0) { /* insmod */
		size_t len;
		void *map;

		len = MAXINT(ssize_t);
		map = xmalloc_open_zipped_read_close(*argv, &len);
		if (!map)
			bb_perror_msg_and_die("can't read '%s'", *argv);
		if (init_module(map, len,
			IF_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE(options ? options : "")
			IF_NOT_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE("")
			) != 0
		) {
			bb_error_msg_and_die("can't insert '%s': %s",
					*argv, moderror(errno));
		}
		return 0;
	}

	/* Try to load modprobe.dep.bb */
	load_dep_bb();

	/* Load/remove modules.
	 * Only rmmod loops here, modprobe has only argv[0] */
	do {
		process_module(*argv, options);
	} while (*++argv);

	if (ENABLE_FEATURE_CLEAN_UP) {
		IF_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE(free(options);)
	}
	return EXIT_SUCCESS;
}

/* vi: set sw=4 ts=4: */
/*
 * depmod - generate modules.dep
 * Copyright (c) 2008 Bernhard Reutner-Fischer
 * Copyrihgt (c) 2008 Timo Teras <timo.teras@iki.fi>
 * Copyright (c) 2008 Vladimir Dronnikov
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//applet:IF_DEPMOD(APPLET(depmod, BB_DIR_SBIN, BB_SUID_DROP))

#include "libbb.h"
#include "modutils.h"
#include <sys/utsname.h> /* uname() */

/*
 * Theory of operation:
 * - iterate over all modules and record their full path
 * - iterate over all modules looking for "depends=" entries
 *   for each depends, look through our list of full paths and emit if found
 */

typedef struct module_info {
	struct module_info *next;
	char *name, *modname;
	llist_t *dependencies;
	llist_t *aliases;
	llist_t *symbols;
	struct module_info *dnext, *dprev;
} module_info;

static int FAST_FUNC parse_module(const char *fname, struct stat *sb UNUSED_PARAM,
				  void *data, int depth UNUSED_PARAM)
{
	char modname[MODULE_NAME_LEN];
	module_info **first = (module_info **) data;
	char *image, *ptr;
	module_info *info;
	/* Arbitrary. Was sb->st_size, but that breaks .gz etc */
	size_t len = (64*1024*1024 - 4096);

	if (strrstr(fname, ".ko") == NULL)
		return TRUE;

	image = xmalloc_open_zipped_read_close(fname, &len);
	info = xzalloc(sizeof(*info));

	info->next = *first;
	*first = info;

	info->dnext = info->dprev = info;
	info->name = xstrdup(fname + 2); /* skip "./" */
	info->modname = xstrdup(filename2modname(fname, modname));
	for (ptr = image; ptr < image + len - 10; ptr++) {
		if (strncmp(ptr, "depends=", 8) == 0) {
			char *u;

			ptr += 8;
			for (u = ptr; *u; u++)
				if (*u == '-')
					*u = '_';
			ptr += string_to_llist(ptr, &info->dependencies, ",");
		} else if (ENABLE_FEATURE_MODUTILS_ALIAS
		 && strncmp(ptr, "alias=", 6) == 0
		) {
			llist_add_to(&info->aliases, xstrdup(ptr + 6));
			ptr += strlen(ptr);
		} else if (ENABLE_FEATURE_MODUTILS_SYMBOLS
		 && strncmp(ptr, "__ksymtab_", 10) == 0
		) {
			ptr += 10;
			if (strncmp(ptr, "gpl", 3) == 0
			 || strcmp(ptr, "strings") == 0
			) {
				continue;
			}
			llist_add_to(&info->symbols, xstrdup(ptr));
			ptr += strlen(ptr);
		}
	}
	free(image);

	return TRUE;
}

static module_info *find_module(module_info *modules, const char *modname)
{
	module_info *m;

	for (m = modules; m != NULL; m = m->next)
		if (strcmp(m->modname, modname) == 0)
			return m;
	return NULL;
}

static void order_dep_list(module_info *modules, module_info *start,
			   llist_t *add)
{
	module_info *m;
	llist_t *n;

	for (n = add; n != NULL; n = n->link) {
		m = find_module(modules, n->data);
		if (m == NULL)
			continue;

		/* unlink current entry */
		m->dnext->dprev = m->dprev;
		m->dprev->dnext = m->dnext;

		/* and add it to tail */
		m->dnext = start;
		m->dprev = start->dprev;
		start->dprev->dnext = m;
		start->dprev = m;

		/* recurse */
		order_dep_list(modules, start, m->dependencies);
	}
}

static void xfreopen_write(const char *file, FILE *f)
{
	if (freopen(file, "w", f) == NULL)
		bb_perror_msg_and_die("can't open '%s'", file);
}

//usage:#if !ENABLE_MODPROBE_SMALL
//usage:#define depmod_trivial_usage "[-n] [-b BASE] [VERSION] [MODFILES]..."
//usage:#define depmod_full_usage "\n\n"
//usage:       "Generate modules.dep, alias, and symbols files"
//usage:     "\n"
//usage:     "\n	-b BASE	Use BASE/lib/modules/VERSION"
//usage:     "\n	-n	Dry run: print files to stdout"
//usage:#endif

/* Upstream usage:
 * [-aAenv] [-C FILE or DIR] [-b BASE] [-F System.map] [VERSION] [MODFILES]...
 *	-a --all
 *		Probe all modules. Default if no MODFILES.
 *	-A --quick
 *		Check modules.dep's mtime against module files' mtimes.
 *	-b --basedir BASE
 *		Use $BASE/lib/modules/VERSION
 *	-C --config FILE or DIR
 *		Path to /etc/depmod.conf or /etc/depmod.d/
 *	-e --errsyms
 *		When combined with the -F option, this reports any symbols
 *		which are not supplied by other modules or kernel.
 *	-F --filesyms System.map
 *	-n --dry-run
 *		Print modules.dep etc to standard output
 *	-v --verbose
 *		Print to stdout all the symbols each module depends on
 *		and the module's file name which provides that symbol.
 *	-r	No-op
 *	-u	No-op
 *	-q	No-op
 *
 * So far we only support: [-n] [-b BASE] [VERSION] [MODFILES]...
 * Accepted but ignored:
 * -aAe
 * -F System.map
 * -C FILE/DIR
 *
 * Not accepted: -v
 */
enum {
	//OPT_a = (1 << 0), /* All modules, ignore mods in argv */
	//OPT_A = (1 << 1), /* Only emit .ko that are newer than modules.dep file */
	OPT_b = (1 << 2), /* base directory when modules are in staging area */
	//OPT_e = (1 << 3), /* with -F, print unresolved symbols */
	//OPT_F = (1 << 4), /* System.map that contains the symbols */
	OPT_n = (1 << 5), /* dry-run, print to stdout only */
	OPT_r = (1 << 6), /* Compat dummy. Linux Makefile uses it */
	OPT_u = (1 << 7), /* -u,--unresolved-error: ignored */
	OPT_q = (1 << 8), /* -q,--quiet: ignored */
	OPT_C = (1 << 9), /* -C,--config etc_modules_conf: ignored */
};

int depmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int depmod_main(int argc UNUSED_PARAM, char **argv)
{
	module_info *modules, *m, *dep;
	const char *moddir_base = "/";
	char *moddir, *version;
	struct utsname uts;
	int tmp;

	getopt32(argv, "aAb:eF:nruqC:", &moddir_base, NULL, NULL);
	argv += optind;

	/* goto modules location */
	xchdir(moddir_base);

	/* If a version is provided, then that kernel version's module directory
	 * is used, rather than the current kernel version (as returned by
	 * "uname -r").  */
	if (*argv && sscanf(*argv, "%u.%u.%u", &tmp, &tmp, &tmp) == 3) {
		version = *argv++;
	} else {
		uname(&uts);
		version = uts.release;
	}
	moddir = concat_path_file(&CONFIG_DEFAULT_MODULES_DIR[1], version);
	xchdir(moddir);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(moddir);

	/* Scan modules */
	modules = NULL;
	if (*argv) {
		do {
			parse_module(*argv, /*sb (unused):*/ NULL, &modules, 0);
		} while (*++argv);
	} else {
		recursive_action(".", ACTION_RECURSE,
				 parse_module, NULL, &modules, 0);
	}

	/* Generate dependency and alias files */
	if (!(option_mask32 & OPT_n))
		xfreopen_write(CONFIG_DEFAULT_DEPMOD_FILE, stdout);
	for (m = modules; m != NULL; m = m->next) {
		printf("%s:", m->name);

		order_dep_list(modules, m, m->dependencies);
		while (m->dnext != m) {
			dep = m->dnext;
			printf(" %s", dep->name);

			/* unlink current entry */
			dep->dnext->dprev = dep->dprev;
			dep->dprev->dnext = dep->dnext;
			dep->dnext = dep->dprev = dep;
		}
		bb_putchar('\n');
	}

#if ENABLE_FEATURE_MODUTILS_ALIAS
	if (!(option_mask32 & OPT_n))
		xfreopen_write("modules.alias", stdout);
	for (m = modules; m != NULL; m = m->next) {
		const char *fname = bb_basename(m->name);
		int fnlen = strchrnul(fname, '.') - fname;
		while (m->aliases) {
			/* Last word can well be m->modname instead,
			 * but depmod from module-init-tools 3.4
			 * uses module basename, i.e., no s/-/_/g.
			 * (pathname and .ko.* are still stripped)
			 * Mimicking that... */
			printf("alias %s %.*s\n",
				(char*)llist_pop(&m->aliases),
				fnlen, fname);
		}
	}
#endif
#if ENABLE_FEATURE_MODUTILS_SYMBOLS
	if (!(option_mask32 & OPT_n))
		xfreopen_write("modules.symbols", stdout);
	for (m = modules; m != NULL; m = m->next) {
		const char *fname = bb_basename(m->name);
		int fnlen = strchrnul(fname, '.') - fname;
		while (m->symbols) {
			printf("alias symbol:%s %.*s\n",
				(char*)llist_pop(&m->symbols),
				fnlen, fname);
		}
	}
#endif

	if (ENABLE_FEATURE_CLEAN_UP) {
		while (modules) {
			module_info *old = modules;
			modules = modules->next;
			free(old->name);
			free(old->modname);
			free(old);
		}
	}

	return EXIT_SUCCESS;
}

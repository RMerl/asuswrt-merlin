/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <locale.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/time.h>

#define LKC_DIRECT_LINK
#include "lkc.h"

static void conf(struct menu *menu);
static void check_conf(struct menu *menu);

enum input_mode {
	oldaskconfig,
	silentoldconfig,
	oldconfig,
	allnoconfig,
	allyesconfig,
	allmodconfig,
	alldefconfig,
	randconfig,
	defconfig,
	savedefconfig,
	listnewconfig,
	oldnoconfig,
} input_mode = oldaskconfig;

char *defconfig_file;

static int indent = 1;
static int valid_stdin = 1;
static int sync_kconfig;
static int conf_cnt;
static char line[128];
static struct menu *rootEntry;

static void print_help(struct menu *menu)
{
	struct gstr help = str_new();

	menu_get_ext_help(menu, &help);

	printf("\n%s\n", str_get(&help));
	str_free(&help);
}

static void strip(char *str)
{
	char *p = str;
	int l;

	while ((isspace(*p)))
		p++;
	l = strlen(p);
	if (p != str)
		memmove(str, p, l + 1);
	if (!l)
		return;
	p = str + l - 1;
	while ((isspace(*p)))
		*p-- = 0;
}

static void check_stdin(void)
{
	if (!valid_stdin) {
		printf(_("aborted!\n\n"));
		printf(_("Console input/output is redirected. "));
		printf(_("Run 'make oldconfig' to update configuration.\n\n"));
		exit(1);
	}
}

static int conf_askvalue(struct symbol *sym, const char *def)
{
	enum symbol_type type = sym_get_type(sym);

	if (!sym_has_value(sym))
		printf(_("(NEW) "));

	line[0] = '\n';
	line[1] = 0;

	if (!sym_is_changable(sym)) {
		printf("%s\n", def);
		line[0] = '\n';
		line[1] = 0;
		return 0;
	}

	switch (input_mode) {
	case oldconfig:
	case silentoldconfig:
		if (sym_has_value(sym)) {
			printf("%s\n", def);
			return 0;
		}
		check_stdin();
	case oldaskconfig:
		fflush(stdout);
		xfgets(line, 128, stdin);
		return 1;
	default:
		break;
	}

	switch (type) {
	case S_INT:
	case S_HEX:
	case S_STRING:
		printf("%s\n", def);
		return 1;
	default:
		;
	}
	printf("%s", line);
	return 1;
}

static int conf_string(struct menu *menu)
{
	struct symbol *sym = menu->sym;
	const char *def;

	while (1) {
		printf("%*s%s ", indent - 1, "", _(menu->prompt->text));
		printf("(%s) ", sym->name);
		def = sym_get_string_value(sym);
		if (sym_get_string_value(sym))
			printf("[%s] ", def);
		if (!conf_askvalue(sym, def))
			return 0;
		switch (line[0]) {
		case '\n':
			break;
		case '?':
			/* print help */
			if (line[1] == '\n') {
				print_help(menu);
				def = NULL;
				break;
			}
		default:
			line[strlen(line)-1] = 0;
			def = line;
		}
		if (def && sym_set_string_value(sym, def))
			return 0;
	}
}

static int conf_sym(struct menu *menu)
{
	struct symbol *sym = menu->sym;
	tristate oldval, newval;

	while (1) {
		printf("%*s%s ", indent - 1, "", _(menu->prompt->text));
		if (sym->name)
			printf("(%s) ", sym->name);
		putchar('[');
		oldval = sym_get_tristate_value(sym);
		switch (oldval) {
		case no:
			putchar('N');
			break;
		case mod:
			putchar('M');
			break;
		case yes:
			putchar('Y');
			break;
		}
		if (oldval != no && sym_tristate_within_range(sym, no))
			printf("/n");
		if (oldval != mod && sym_tristate_within_range(sym, mod))
			printf("/m");
		if (oldval != yes && sym_tristate_within_range(sym, yes))
			printf("/y");
		if (menu_has_help(menu))
			printf("/?");
		printf("] ");
		if (!conf_askvalue(sym, sym_get_string_value(sym)))
			return 0;
		strip(line);

		switch (line[0]) {
		case 'n':
		case 'N':
			newval = no;
			if (!line[1] || !strcmp(&line[1], "o"))
				break;
			continue;
		case 'm':
		case 'M':
			newval = mod;
			if (!line[1])
				break;
			continue;
		case 'y':
		case 'Y':
			newval = yes;
			if (!line[1] || !strcmp(&line[1], "es"))
				break;
			continue;
		case 0:
			newval = oldval;
			break;
		case '?':
			goto help;
		default:
			continue;
		}
		if (sym_set_tristate_value(sym, newval))
			return 0;
help:
		print_help(menu);
	}
}

static int conf_choice(struct menu *menu)
{
	struct symbol *sym, *def_sym;
	struct menu *child;
	bool is_new;

	sym = menu->sym;
	is_new = !sym_has_value(sym);
	if (sym_is_changable(sym)) {
		conf_sym(menu);
		sym_calc_value(sym);
		switch (sym_get_tristate_value(sym)) {
		case no:
			return 1;
		case mod:
			return 0;
		case yes:
			break;
		}
	} else {
		switch (sym_get_tristate_value(sym)) {
		case no:
			return 1;
		case mod:
			printf("%*s%s\n", indent - 1, "", _(menu_get_prompt(menu)));
			return 0;
		case yes:
			break;
		}
	}

	while (1) {
		int cnt, def;

		printf("%*s%s\n", indent - 1, "", _(menu_get_prompt(menu)));
		def_sym = sym_get_choice_value(sym);
		cnt = def = 0;
		line[0] = 0;
		for (child = menu->list; child; child = child->next) {
			if (!menu_is_visible(child))
				continue;
			if (!child->sym) {
				printf("%*c %s\n", indent, '*', _(menu_get_prompt(child)));
				continue;
			}
			cnt++;
			if (child->sym == def_sym) {
				def = cnt;
				printf("%*c", indent, '>');
			} else
				printf("%*c", indent, ' ');
			printf(" %d. %s", cnt, _(menu_get_prompt(child)));
			if (child->sym->name)
				printf(" (%s)", child->sym->name);
			if (!sym_has_value(child->sym))
				printf(_(" (NEW)"));
			printf("\n");
		}
		printf(_("%*schoice"), indent - 1, "");
		if (cnt == 1) {
			printf("[1]: 1\n");
			goto conf_childs;
		}
		printf("[1-%d", cnt);
		if (menu_has_help(menu))
			printf("?");
		printf("]: ");
		switch (input_mode) {
		case oldconfig:
		case silentoldconfig:
			if (!is_new) {
				cnt = def;
				printf("%d\n", cnt);
				break;
			}
			check_stdin();
		case oldaskconfig:
			fflush(stdout);
			xfgets(line, 128, stdin);
			strip(line);
			if (line[0] == '?') {
				print_help(menu);
				continue;
			}
			if (!line[0])
				cnt = def;
			else if (isdigit(line[0]))
				cnt = atoi(line);
			else
				continue;
			break;
		default:
			break;
		}

	conf_childs:
		for (child = menu->list; child; child = child->next) {
			if (!child->sym || !menu_is_visible(child))
				continue;
			if (!--cnt)
				break;
		}
		if (!child)
			continue;
		if (line[strlen(line) - 1] == '?') {
			print_help(child);
			continue;
		}
		sym_set_choice_value(sym, child->sym);
		for (child = child->list; child; child = child->next) {
			indent += 2;
			conf(child);
			indent -= 2;
		}
		return 1;
	}
}

static void conf(struct menu *menu)
{
	struct symbol *sym;
	struct property *prop;
	struct menu *child;

	if (!menu_is_visible(menu))
		return;

	sym = menu->sym;
	prop = menu->prompt;
	if (prop) {
		const char *prompt;

		switch (prop->type) {
		case P_MENU:
			if ((input_mode == silentoldconfig ||
			     input_mode == listnewconfig ||
			     input_mode == oldnoconfig) &&
			    rootEntry != menu) {
				check_conf(menu);
				return;
			}
		case P_COMMENT:
			prompt = menu_get_prompt(menu);
			if (prompt)
				printf("%*c\n%*c %s\n%*c\n",
					indent, '*',
					indent, '*', _(prompt),
					indent, '*');
		default:
			;
		}
	}

	if (!sym)
		goto conf_childs;

	if (sym_is_choice(sym)) {
		conf_choice(menu);
		if (sym->curr.tri != mod)
			return;
		goto conf_childs;
	}

	switch (sym->type) {
	case S_INT:
	case S_HEX:
	case S_STRING:
		conf_string(menu);
		break;
	default:
		conf_sym(menu);
		break;
	}

conf_childs:
	if (sym)
		indent += 2;
	for (child = menu->list; child; child = child->next)
		conf(child);
	if (sym)
		indent -= 2;
}

static void check_conf(struct menu *menu)
{
	struct symbol *sym;
	struct menu *child;

	if (!menu_is_visible(menu))
		return;

	sym = menu->sym;
	if (sym && !sym_has_value(sym)) {
		if (sym_is_changable(sym) ||
		    (sym_is_choice(sym) && sym_get_tristate_value(sym) == yes)) {
			if (input_mode == listnewconfig) {
				if (sym->name && !sym_is_choice_value(sym)) {
					printf("CONFIG_%s\n", sym->name);
				}
			} else if (input_mode != oldnoconfig) {
				if (!conf_cnt++)
					printf(_("*\n* Restart config...\n*\n"));
				rootEntry = menu_get_parent_menu(menu);
				conf(rootEntry);
			}
		}
	}

	for (child = menu->list; child; child = child->next)
		check_conf(child);
}

static struct option long_opts[] = {
	{"oldaskconfig",    no_argument,       NULL, oldaskconfig},
	{"oldconfig",       no_argument,       NULL, oldconfig},
	{"silentoldconfig", no_argument,       NULL, silentoldconfig},
	{"defconfig",       optional_argument, NULL, defconfig},
	{"savedefconfig",   required_argument, NULL, savedefconfig},
	{"allnoconfig",     no_argument,       NULL, allnoconfig},
	{"allyesconfig",    no_argument,       NULL, allyesconfig},
	{"allmodconfig",    no_argument,       NULL, allmodconfig},
	{"alldefconfig",    no_argument,       NULL, alldefconfig},
	{"randconfig",      no_argument,       NULL, randconfig},
	{"listnewconfig",   no_argument,       NULL, listnewconfig},
	{"oldnoconfig",     no_argument,       NULL, oldnoconfig},
	{NULL, 0, NULL, 0}
};

int main(int ac, char **av)
{
	int opt;
	const char *name;
	struct stat tmpstat;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((opt = getopt_long(ac, av, "", long_opts, NULL)) != -1) {
		input_mode = (enum input_mode)opt;
		switch (opt) {
		case silentoldconfig:
			sync_kconfig = 1;
			break;
		case defconfig:
		case savedefconfig:
			defconfig_file = optarg;
			break;
		case randconfig:
		{
			struct timeval now;
			unsigned int seed;

			/*
			 * Use microseconds derived seed,
			 * compensate for systems where it may be zero
			 */
			gettimeofday(&now, NULL);

			seed = (unsigned int)((now.tv_sec + 1) * (now.tv_usec + 1));
			srand(seed);
			break;
		}
		case '?':
			fprintf(stderr, _("See README for usage info\n"));
			exit(1);
			break;
		}
	}
	if (ac == optind) {
		printf(_("%s: Kconfig file missing\n"), av[0]);
		exit(1);
	}
	name = av[optind];
	conf_parse(name);
	//zconfdump(stdout);
	if (sync_kconfig) {
		name = conf_get_configname();
		if (stat(name, &tmpstat)) {
			fprintf(stderr, _("***\n"
				"*** You have not yet configured your kernel!\n"
				"*** (missing kernel config file \"%s\")\n"
				"***\n"
				"*** Please run some configurator (e.g. \"make oldconfig\" or\n"
				"*** \"make menuconfig\" or \"make xconfig\").\n"
				"***\n"), name);
			exit(1);
		}
	}

	switch (input_mode) {
	case defconfig:
		if (!defconfig_file)
			defconfig_file = conf_get_default_confname();
		if (conf_read(defconfig_file)) {
			printf(_("***\n"
				"*** Can't find default configuration \"%s\"!\n"
				"***\n"), defconfig_file);
			exit(1);
		}
		break;
	case savedefconfig:
		conf_read(NULL);
		break;
	case silentoldconfig:
	case oldaskconfig:
	case oldconfig:
	case listnewconfig:
	case oldnoconfig:
		conf_read(NULL);
		break;
	case allnoconfig:
	case allyesconfig:
	case allmodconfig:
	case alldefconfig:
	case randconfig:
		name = getenv("KCONFIG_ALLCONFIG");
		if (name && !stat(name, &tmpstat)) {
			conf_read_simple(name, S_DEF_USER);
			break;
		}
		switch (input_mode) {
		case allnoconfig:	name = "allno.config"; break;
		case allyesconfig:	name = "allyes.config"; break;
		case allmodconfig:	name = "allmod.config"; break;
		case alldefconfig:	name = "alldef.config"; break;
		case randconfig:	name = "allrandom.config"; break;
		default: break;
		}
		if (!stat(name, &tmpstat))
			conf_read_simple(name, S_DEF_USER);
		else if (!stat("all.config", &tmpstat))
			conf_read_simple("all.config", S_DEF_USER);
		break;
	default:
		break;
	}

	if (sync_kconfig) {
		if (conf_get_changed()) {
			name = getenv("KCONFIG_NOSILENTUPDATE");
			if (name && *name) {
				fprintf(stderr,
					_("\n*** Kernel configuration requires explicit update.\n\n"));
				return 1;
			}
		}
		valid_stdin = isatty(0) && isatty(1) && isatty(2);
	}

	switch (input_mode) {
	case allnoconfig:
		conf_set_all_new_symbols(def_no);
		break;
	case allyesconfig:
		conf_set_all_new_symbols(def_yes);
		break;
	case allmodconfig:
		conf_set_all_new_symbols(def_mod);
		break;
	case alldefconfig:
		conf_set_all_new_symbols(def_default);
		break;
	case randconfig:
		conf_set_all_new_symbols(def_random);
		break;
	case defconfig:
		conf_set_all_new_symbols(def_default);
		break;
	case savedefconfig:
		break;
	case oldaskconfig:
		rootEntry = &rootmenu;
		conf(&rootmenu);
		input_mode = silentoldconfig;
		/* fall through */
	case oldconfig:
	case listnewconfig:
	case oldnoconfig:
	case silentoldconfig:
		/* Update until a loop caused no more changes */
		do {
			conf_cnt = 0;
			check_conf(&rootmenu);
		} while (conf_cnt &&
			 (input_mode != listnewconfig &&
			  input_mode != oldnoconfig));
		break;
	}

	if (sync_kconfig) {
		/* silentoldconfig is used during the build so we shall update autoconf.
		 * All other commands are only used to generate a config.
		 */
		if (conf_get_changed() && conf_write(NULL)) {
			fprintf(stderr, _("\n*** Error during writing of the kernel configuration.\n\n"));
			exit(1);
		}
		if (conf_write_autoconf()) {
			fprintf(stderr, _("\n*** Error during update of the kernel configuration.\n\n"));
			return 1;
		}
	} else if (input_mode == savedefconfig) {
		if (conf_write_defconfig(defconfig_file)) {
			fprintf(stderr, _("n*** Error while saving defconfig to: %s\n\n"),
			        defconfig_file);
			return 1;
		}
	} else if (input_mode != listnewconfig) {
		if (conf_write(NULL)) {
			fprintf(stderr, _("\n*** Error during writing of the kernel configuration.\n\n"));
			exit(1);
		}
	}
	return 0;
}
/*
 * Helper function to facilitate fgets() by Jean Sacren.
 */
void xfgets(str, size, in)
	char *str;
	int size;
	FILE *in;
{
	if (fgets(str, size, in) == NULL)
		fprintf(stderr, "\nError in reading or end of file.\n");
}

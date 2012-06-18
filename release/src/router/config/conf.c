/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define LKC_DIRECT_LINK
#include "lkc.h"

static void conf(struct menu *menu);
static void check_conf(struct menu *menu);

enum {
	ask_all,
	ask_new,
	ask_silent,
	set_default,
	set_yes,
	set_mod,
	set_no,
	set_random
} input_mode = ask_all;

static int indent = 1;
static int valid_stdin = 1;
static int conf_cnt;
static char line[128];
static struct menu *rootEntry;

static char nohelp_text[] = "Sorry, no help available for this option yet.\n";


static void printo(const char *o)
{
	static int sep = 0;

	if (!sep) {
		putchar('(');
		sep = 1;
	} else if (o) {
		putchar(',');
		putchar(' ');
	}
	if (!o) {
		putchar(')');
		putchar(' ');
		sep = 0;
	} else
		printf("%s", o);
}

static void strip(char *str)
{
	char *p = str;
	int l;

	while ((isspace((int)*p)))
		p++;
	l = strlen(p);
	if (p != str)
		memmove(str, p, l + 1);
	if (!l)
		return;
	p = str + l - 1;
	while ((isspace((int)*p)))
		*p-- = 0;
}

static void conf_askvalue(struct symbol *sym, const char *def)
{
	enum symbol_type type = sym_get_type(sym);
	tristate val;

	if (!sym_has_value(sym))
		printf("(NEW) ");

	line[0] = '\n';
	line[1] = 0;

	switch (input_mode) {
	case ask_new:
	case ask_silent:
		if (sym_has_value(sym)) {
			printf("%s\n", def);
			return;
		}
		if (!valid_stdin && input_mode == ask_silent) {
			printf("aborted!\n\n");
			printf("Console input/output is redirected. ");
			printf("Run 'make oldconfig' to update configuration.\n\n");
			exit(1);
		}
	case ask_all:
		fflush(stdout);
		fgets(line, 128, stdin);
		return;
	case set_default:
		printf("%s\n", def);
		return;
	default:
		break;
	}

	switch (type) {
	case S_INT:
	case S_HEX:
	case S_STRING:
		printf("%s\n", def);
		return;
	default:
		;
	}
	switch (input_mode) {
	case set_yes:
		if (sym_tristate_within_range(sym, yes)) {
			line[0] = 'y';
			line[1] = '\n';
			line[2] = 0;
			break;
		}
	case set_mod:
		if (type == S_TRISTATE) {
			if (sym_tristate_within_range(sym, mod)) {
				line[0] = 'm';
				line[1] = '\n';
				line[2] = 0;
				break;
			}
		} else {
			if (sym_tristate_within_range(sym, yes)) {
				line[0] = 'y';
				line[1] = '\n';
				line[2] = 0;
				break;
			}
		}
	case set_no:
		if (sym_tristate_within_range(sym, no)) {
			line[0] = 'n';
			line[1] = '\n';
			line[2] = 0;
			break;
		}
	case set_random:
		do {
			val = (tristate)(random() % 3);
		} while (!sym_tristate_within_range(sym, val));
		switch (val) {
		case no: line[0] = 'n'; break;
		case mod: line[0] = 'm'; break;
		case yes: line[0] = 'y'; break;
		}
		line[1] = '\n';
		line[2] = 0;
		break;
	default:
		break;
	}
	printf("%s", line);
}

int conf_string(struct menu *menu)
{
	struct symbol *sym = menu->sym;
	const char *def, *help;

	while (1) {
		printf("%*s%s ", indent - 1, "", menu->prompt->text);
		printf("(%s) ", sym->name);
		def = sym_get_string_value(sym);
		if (sym_get_string_value(sym))
			printf("[%s] ", def);
		conf_askvalue(sym, def);
		switch (line[0]) {
		case '\n':
			break;
		case '?':
			/* print help */
			if (line[1] == 0) {
				help = nohelp_text;
				if (menu->sym->help)
					help = menu->sym->help;
				printf("\n%s\n", menu->sym->help);
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
	int type;
	tristate oldval, newval;
	const char *help;

	while (1) {
		printf("%*s%s ", indent - 1, "", menu->prompt->text);
		if (sym->name)
			printf("(%s) ", sym->name);
		type = sym_get_type(sym);
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
		if (sym->help)
			printf("/?");
		printf("] ");
		conf_askvalue(sym, sym_get_string_value(sym));
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
		help = nohelp_text;
		if (sym->help)
			help = sym->help;
		printf("\n%s\n", help);
	}
}

static int conf_choice(struct menu *menu)
{
	struct symbol *sym, *def_sym;
	struct menu *cmenu, *def_menu;
	const char *help;
	int type, len;
	bool is_new;

	sym = menu->sym;
	type = sym_get_type(sym);
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
		sym->def = sym->curr;
		if (S_TRI(sym->curr) == mod) {
			printf("%*s%s\n", indent - 1, "", menu_get_prompt(menu));
			return 0;
		}
	}

	while (1) {
		printf("%*s%s ", indent - 1, "", menu_get_prompt(menu));
		def_sym = sym_get_choice_value(sym);
		def_menu = NULL;
		for (cmenu = menu->list; cmenu; cmenu = cmenu->next) {
			if (!menu_is_visible(cmenu))
				continue;
			printo(menu_get_prompt(cmenu));
			if (cmenu->sym == def_sym)
				def_menu = cmenu;
		}
		printo(NULL);
		if (def_menu)
			printf("[%s] ", menu_get_prompt(def_menu));
		else {
			printf("\n");
			return 1;
		}
		switch (input_mode) {
		case ask_new:
		case ask_silent:
		case ask_all:
			conf_askvalue(sym, menu_get_prompt(def_menu));
			strip(line);
			break;
		default:
			line[0] = 0;
			printf("\n");
		}
		if (line[0] == '?' && !line[1]) {
			help = nohelp_text;
			if (menu->sym->help)
				help = menu->sym->help;
			printf("\n%s\n", help);
			continue;
		}
		if (line[0]) {
			len = strlen(line);
			line[len] = 0;

			def_menu = NULL;
			for (cmenu = menu->list; cmenu; cmenu = cmenu->next) {
				if (!cmenu->sym || !menu_is_visible(cmenu))
					continue;
				if (!strncasecmp(line, menu_get_prompt(cmenu), len)) {
					def_menu = cmenu;
					break;
				}
			}
		}
		if (def_menu) {
			sym_set_choice_value(sym, def_menu->sym);
			if (def_menu->list) {
				indent += 2;
				conf(def_menu->list);
				indent -= 2;
			}
			return 1;
		}
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
			if (input_mode == ask_silent && rootEntry != menu) {
				check_conf(menu);
				return;
			}
		case P_COMMENT:
			prompt = menu_get_prompt(menu);
			if (prompt)
				printf("%*c\n%*c %s\n%*c\n",
					indent, '*',
					indent, '*', prompt,
					indent, '*');
		default:
			;
		}
	}

	if (!sym)
		goto conf_childs;

	if (sym_is_choice(sym)) {
		conf_choice(menu);
		if (S_TRI(sym->curr) != mod)
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
	if (!sym)
		goto conf_childs;

	if (sym_is_choice(sym)) {
		if (!sym_has_value(sym)) {
			if (!conf_cnt++)
				printf("*\n* Restart config...\n*\n");
			rootEntry = menu_get_parent_menu(menu);
			conf(rootEntry);
		}
		if (sym_get_tristate_value(sym) != mod)
			return;
		goto conf_childs;
	}

	if (!sym_has_value(sym)) {
		if (!conf_cnt++)
			printf("*\n* Restart config...\n*\n");
		rootEntry = menu_get_parent_menu(menu);
		conf(rootEntry);
	}

conf_childs:
	for (child = menu->list; child; child = child->next)
		check_conf(child);
}

int main(int ac, char **av)
{
	const char *name;
	struct stat tmpstat;

	if (ac > 1 && av[1][0] == '-') {
		switch (av[1][1]) {
		case 'o':
			input_mode = ask_new;
			break;
		case 's':
			input_mode = ask_silent;
			valid_stdin = isatty(0) && isatty(1) && isatty(2);
			break;
		case 'd':
			input_mode = set_default;
			break;
		case 'n':
			input_mode = set_no;
			break;
		case 'm':
			input_mode = set_mod;
			break;
		case 'y':
			input_mode = set_yes;
			break;
		case 'r':
			input_mode = set_random;
			srandom(time(NULL));
			break;
		case 'h':
		case '?':
			printf("%s [-o|-s] config\n", av[0]);
			exit(0);
		}
		name = av[2];
	} else
		name = av[1];
	conf_parse(name);
	//zconfdump(stdout);
	switch (input_mode) {
	case set_default:
		name = conf_get_default_confname();
		if (conf_read(name)) {
			printf("***\n"
				"*** Can't find default configuration \"%s\"!\n"
				"***\n", name);
			exit(1);
		}
		break;
	case ask_silent:
		if (stat(".config", &tmpstat)) {
			printf("***\n"
				"*** You have not yet configured!\n"
				"***\n"
				"*** Please run some configurator (e.g. \"make oldconfig\"\n"
				"*** or \"make menuconfig\").\n"
				"***\n");
			exit(1);
		}
	case ask_all:
	case ask_new:
		conf_read(NULL);
		break;
	default:
		break;
	}

	if (input_mode != ask_silent) {
		rootEntry = &rootmenu;
		conf(&rootmenu);
		if (input_mode == ask_all) {
			input_mode = ask_silent;
			valid_stdin = 1;
		}
	}
	do {
		conf_cnt = 0;
		check_conf(&rootmenu);
	} while (conf_cnt);
	conf_write(NULL);
	return 0;
}

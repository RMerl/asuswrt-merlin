/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 *
 * Allow 'n' as a symbol value.
 * 2002-11-05 Petr Baudis <pasky@ucw.cz>
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LKC_DIRECT_LINK
#include "lkc.h"

const char conf_def_filename[] = ".config";

const char conf_defname[] = "extra/Configs/Config.$TARGET_ARCH.default";

const char *conf_confnames[] = {
	".config",
	conf_defname,
	NULL,
};

static char *conf_expand_value(const char *in)
{
	struct symbol *sym;
	const char *src;
	static char res_value[SYMBOL_MAXLENGTH];
	char *dst, name[SYMBOL_MAXLENGTH];

	res_value[0] = 0;
	dst = name;
	while ((src = strchr(in, '$'))) {
		strncat(res_value, in, src - in);
		src++;
		dst = name;
		while (isalnum((int)*src) || *src == '_')
			*dst++ = *src++;
		*dst = 0;
		sym = sym_lookup(name, 0);
		sym_calc_value(sym);
		strcat(res_value, sym_get_string_value(sym));
		in = src;
	}
	strcat(res_value, in);

	return res_value;
}

char *conf_get_default_confname(void)
{
	return conf_expand_value(conf_defname);
}

int conf_read(const char *name)
{
	FILE *in = NULL;
	char line[1024];
	char *p, *p2;
	int lineno = 0;
	struct symbol *sym;
	struct property *prop;
	struct expr *e;
	int i;

	if (name) {
		in = fopen(name, "r");
	} else {
		const char **names = conf_confnames;
		while ((name = *names++)) {
			name = conf_expand_value(name);
			in = fopen(name, "r");
			if (in) {
				printf("#\n"
				       "# using defaults found in %s\n"
				       "#\n", name);
				break;
			}
		}
	}

	if (!in)
		return 1;

	for_all_symbols(i, sym) {
		sym->flags |= SYMBOL_NEW | SYMBOL_CHANGED;
		sym->flags &= ~SYMBOL_VALID;
		switch (sym->type) {
		case S_INT:
		case S_HEX:
		case S_STRING:
			if (S_VAL(sym->def))
				free(S_VAL(sym->def));
		default:
			S_VAL(sym->def) = NULL;
			S_TRI(sym->def) = no;
			;
		}
	}

	while (fgets(line, sizeof(line), in)) {
		lineno++;
		switch (line[0]) {
		case '\n':
			break;
		case ' ':
			break;
		case '#':
			p = strchr(line, ' ');
			if (!p)
				continue;
			*p++ = 0;
			p = strchr(p, ' ');
			if (!p)
				continue;
			*p++ = 0;
			if (strncmp(p, "is not set", 10))
				continue;
			sym = sym_lookup(line+2, 0);
			switch (sym->type) {
			case S_BOOLEAN:
			case S_TRISTATE:
				sym->def = symbol_no.curr;
				sym->flags &= ~SYMBOL_NEW;
				break;
			default:
				;
			}
			break;
		case 'A' ... 'Z':
			p = strchr(line, '=');
			if (!p)
				continue;
			*p++ = 0;
			p2 = strchr(p, '\n');
			if (p2)
				*p2 = 0;
			sym = sym_find(line);
			if (!sym) {
				fprintf(stderr, "%s:%d: trying to assign nonexistent symbol %s\n", name, lineno, line);
				break;
			}
			switch (sym->type) {
  			case S_TRISTATE:
				if (p[0] == 'm') {
					S_TRI(sym->def) = mod;
					sym->flags &= ~SYMBOL_NEW;
					break;
				}
			case S_BOOLEAN:
				if (p[0] == 'y') {
					S_TRI(sym->def) = yes;
					sym->flags &= ~SYMBOL_NEW;
					break;
				}
				if (p[0] == 'n') {
					S_TRI(sym->def) = no;
					sym->flags &= ~SYMBOL_NEW;
					break;
				}
  				break;
			case S_STRING:
			case S_INT:
			case S_HEX:
				if (sym_string_valid(sym, p)) {
					S_VAL(sym->def) = strdup(p);
					sym->flags &= ~SYMBOL_NEW;
				} else
					fprintf(stderr, "%s:%d:symbol value '%s' invalid for %s\n", name, lineno, p, sym->name);
				break;
			default:
				;
			}
			if (sym_is_choice_value(sym)) {
				prop = sym_get_choice_prop(sym);
				switch (S_TRI(sym->def)) {
				case mod:
					if (S_TRI(prop->def->def) == yes)
						/* warn? */;
					break;
				case yes:
					if (S_TRI(prop->def->def) != no)
						/* warn? */;
					S_VAL(prop->def->def) = sym;
					break;
				case no:
					break;
				}
				S_TRI(prop->def->def) = S_TRI(sym->def);
			}
			break;
		default:
			continue;
		}
	}
	fclose(in);

	for_all_symbols(i, sym) {
		if (!sym_is_choice(sym))
			continue;
		prop = sym_get_choice_prop(sym);
		for (e = prop->dep; e; e = e->left.expr)
			sym->flags |= e->right.sym->flags & SYMBOL_NEW;
		sym->flags &= ~SYMBOL_NEW;
	}

	sym_change_count = 1;

	return 0;
}

int conf_write(const char *name)
{
	FILE *out, *out_h;
	struct symbol *sym;
	struct menu *menu;
	char oldname[128];
	int type, l;
	const char *str;

	out = fopen(".tmpconfig", "w");
	if (!out)
		return 1;
	out_h = fopen(".tmpconfig.h", "w");
	if (!out_h)
		return 1;
	fprintf(out, "#\n"
		     "# Automatically generated make config: don't edit\n"
		     "#\n");
	fprintf(out_h, "/*\n"
		       " * Automatically generated C config: don't edit\n"
		       " */\n"
       );

	if (!sym_change_count)
		sym_clear_all_valid();

	menu = rootmenu.list;
	while (menu) {
		sym = menu->sym;
		if (!sym) {
			if (!menu_is_visible(menu))
				goto next;
			str = menu_get_prompt(menu);
			fprintf(out, "\n"
				     "#\n"
				     "# %s\n"
				     "#\n", str);
			fprintf(out_h, "\n"
				       "/*\n"
				       " * %s\n"
				       " */\n", str);
		} else if (!(sym->flags & SYMBOL_CHOICE)) {
			sym_calc_value(sym);
			if (!(sym->flags & SYMBOL_WRITE))
				goto next;
			sym->flags &= ~SYMBOL_WRITE;
			type = sym->type;
			if (type == S_TRISTATE) {
				sym_calc_value(modules_sym);
				if (S_TRI(modules_sym->curr) == no)
					type = S_BOOLEAN;
			}
			switch (type) {
			case S_BOOLEAN:
			case S_TRISTATE:
				switch (sym_get_tristate_value(sym)) {
				case no:
					fprintf(out, "# %s is not set\n", sym->name);
					fprintf(out_h, "#undef %s\n", sym->name);
					break;
				case mod:
					fprintf(out, "%s=m\n", sym->name);
					fprintf(out_h, "#define %s__MODULE 1\n", sym->name);
					break;
				case yes:
					fprintf(out, "%s=y\n", sym->name);
					fprintf(out_h, "#define %s 1\n", sym->name);
					break;
				}
				break;
			case S_STRING:
				// fix me
				str = sym_get_string_value(sym);
				fprintf(out, "%s=", sym->name);
				fprintf(out_h, "#define %s \"", sym->name);
				do {
					l = strcspn(str, "\"\\");
					if (l) {
						fwrite(str, l, 1, out);
						fwrite(str, l, 1, out_h);
					}
					str += l;
					while (*str == '\\' || *str == '"') {
						fprintf(out, "\\%c", *str);
						fprintf(out_h, "\\%c", *str);
						str++;
					}
				} while (*str);
				fputs("\n", out);
				fputs("\"\n", out_h);
				break;
			case S_HEX:
				str = sym_get_string_value(sym);
				if (str[0] != '0' || (str[1] != 'x' && str[1] != 'X')) {
					fprintf(out, "%s=%s\n", sym->name, str);
					fprintf(out_h, "#define %s 0x%s\n", sym->name, str);
					break;
				}
			case S_INT:
				str = sym_get_string_value(sym);
				fprintf(out, "%s=%s\n", sym->name, str);
				fprintf(out_h, "#define %s %s\n", sym->name, str);
				break;
			}
		}

	next:
		if (menu->list) {
			menu = menu->list;
			continue;
		}
		if (menu->next)
			menu = menu->next;
		else while ((menu = menu->parent)) {
			if (menu->next) {
				menu = menu->next;
				break;
			}
		}
	}
	fclose(out);
	fclose(out_h);

	if (!name) {
		rename(".tmpconfig.h", "shared/rtconfig.h");
		name = conf_def_filename;
		file_write_dep(NULL);
	} else
		unlink(".tmpconfig.h");

	sprintf(oldname, "%s.old", name);
	rename(name, oldname);
	if (rename(".tmpconfig", name))
		return 1;

	sym_change_count = 0;

	return 0;
}

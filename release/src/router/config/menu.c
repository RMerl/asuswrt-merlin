/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <stdlib.h>
#include <string.h>

#define LKC_DIRECT_LINK
#include "lkc.h"

struct menu rootmenu;
struct menu *current_menu, *current_entry;
static struct menu **last_entry_ptr;

struct file *file_list;
struct file *current_file;

void menu_init(void)
{
	current_entry = current_menu = &rootmenu;
	last_entry_ptr = &rootmenu.list;
}

void menu_add_entry(struct symbol *sym)
{
	struct menu *menu;

	menu = malloc(sizeof(*menu));
	memset(menu, 0, sizeof(*menu));
	menu->sym = sym;
	menu->parent = current_menu;
	menu->file = current_file;
	menu->lineno = zconf_lineno();

	*last_entry_ptr = menu;
	last_entry_ptr = &menu->next;
	current_entry = menu;
}

void menu_end_entry(void)
{
}

void menu_add_menu(void)
{
	current_menu = current_entry;
	last_entry_ptr = &current_entry->list;
}

void menu_end_menu(void)
{
	last_entry_ptr = &current_menu->next;
	current_menu = current_menu->parent;
}

void menu_add_dep(struct expr *dep)
{
	current_entry->dep = expr_alloc_and(current_entry->dep, dep);
}

void menu_set_type(int type)
{
	struct symbol *sym = current_entry->sym;

	if (sym->type == type)
		return;
	if (sym->type == S_UNKNOWN) {
		sym->type = type;
		return;
	}
	fprintf(stderr, "%s:%d: type of '%s' redefined from '%s' to '%s'\n",
		current_entry->file->name, current_entry->lineno,
		sym->name ? sym->name : "<choice>", sym_type_name(sym->type), sym_type_name(type));
}

struct property *create_prop(enum prop_type type)
{
	struct property *prop;

	prop = malloc(sizeof(*prop));
	memset(prop, 0, sizeof(*prop));
	prop->type = type;
	prop->file = current_file;
	prop->lineno = zconf_lineno();

	return prop;
}

struct property *menu_add_prop(int token, char *prompt, struct symbol *def, struct expr *dep)
{
	struct property *prop = create_prop(token);
	struct property **propp;

	prop->sym = current_entry->sym;
	prop->menu = current_entry;
	prop->text = prompt;
	prop->def = def;
	E_EXPR(prop->visible) = dep;

	if (prompt)
		current_entry->prompt = prop;

	/* append property to the prop list of symbol */
	if (prop->sym) {
		for (propp = &prop->sym->prop; *propp; propp = &(*propp)->next)
			;
		*propp = prop;
	}

	return prop;
}

void menu_add_prompt(int token, char *prompt, struct expr *dep)
{
	current_entry->prompt = menu_add_prop(token, prompt, NULL, dep);
}

void menu_add_default(int token, struct symbol *def, struct expr *dep)
{
	current_entry->prompt = menu_add_prop(token, NULL, def, dep);
}

void menu_finalize(struct menu *parent)
{
	struct menu *menu, *last_menu;
	struct symbol *sym;
	struct property *prop;
	struct expr *parentdep, *basedep, *dep, *dep2;

	sym = parent->sym;
	if (parent->list) {
		if (sym && sym_is_choice(sym)) {
			/* find the first choice value and find out choice type */
			for (menu = parent->list; menu; menu = menu->next) {
				if (menu->sym) {
					current_entry = parent;
					menu_set_type(menu->sym->type);
					current_entry = menu;
					menu_set_type(sym->type);
					break;
				}
			}
			parentdep = expr_alloc_symbol(sym);
		} else if (parent->prompt)
			parentdep = E_EXPR(parent->prompt->visible);
		else
			parentdep = parent->dep;

		for (menu = parent->list; menu; menu = menu->next) {
			basedep = expr_transform(menu->dep);
			basedep = expr_alloc_and(expr_copy(parentdep), basedep);
			basedep = expr_eliminate_dups(basedep);
			menu->dep = basedep;
			if (menu->sym)
				prop = menu->sym->prop;
			else
				prop = menu->prompt;
			for (; prop; prop = prop->next) {
				if (prop->menu != menu)
					continue;
				dep = expr_transform(E_EXPR(prop->visible));
				dep = expr_alloc_and(expr_copy(basedep), dep);
				dep = expr_eliminate_dups(dep);
				if (menu->sym && menu->sym->type != S_TRISTATE)
					dep = expr_trans_bool(dep);
				E_EXPR(prop->visible) = dep;
			}
		}
		for (menu = parent->list; menu; menu = menu->next)
			menu_finalize(menu);
	} else if (sym && parent->prompt) {
		basedep = E_EXPR(parent->prompt->visible);
		basedep = expr_trans_compare(basedep, E_UNEQUAL, &symbol_no);
		basedep = expr_eliminate_dups(expr_transform(basedep));
		last_menu = NULL;
		for (menu = parent->next; menu; menu = menu->next) {
			dep = menu->prompt ? E_EXPR(menu->prompt->visible) : menu->dep;
			if (!expr_contains_symbol(dep, sym))
				break;
			if (expr_depends_symbol(dep, sym))
				goto next;
			dep = expr_trans_compare(dep, E_UNEQUAL, &symbol_no);
			dep = expr_eliminate_dups(expr_transform(dep));
			dep2 = expr_copy(basedep);
			expr_eliminate_eq(&dep, &dep2);
			expr_free(dep);
			if (!expr_is_yes(dep2)) {
				expr_free(dep2);
				break;
			}
			expr_free(dep2);
		next:
			menu_finalize(menu);
			menu->parent = parent;
			last_menu = menu;
		}
		if (last_menu) {
			parent->list = parent->next;
			parent->next = last_menu->next;
			last_menu->next = NULL;
		}
	}
	for (menu = parent->list; menu; menu = menu->next) {
		if (sym && sym_is_choice(sym) && menu->sym) {
			menu->sym->flags |= SYMBOL_CHOICEVAL;
			current_entry = menu;
			menu_set_type(sym->type);
			menu_add_prop(P_CHOICE, NULL, parent->sym, NULL);
			prop = sym_get_choice_prop(parent->sym);
			//dep = expr_alloc_one(E_CHOICE, dep);
			//dep->right.sym = menu->sym;
			prop->dep = expr_alloc_one(E_CHOICE, prop->dep);
			prop->dep->right.sym = menu->sym;
		}
		if (menu->list && (!menu->prompt || !menu->prompt->text)) {
			for (last_menu = menu->list; ; last_menu = last_menu->next) {
				last_menu->parent = parent;
				if (!last_menu->next)
					break;
			}
			last_menu->next = menu->next;
			menu->next = menu->list;
			menu->list = NULL;
		}
	}
}

bool menu_is_visible(struct menu *menu)
{
	tristate visible;

	if (!menu->prompt)
		return false;
	if (menu->sym) {
		sym_calc_value(menu->sym);
		visible = E_TRI(menu->prompt->visible);
	} else
		visible = E_CALC(menu->prompt->visible);
	return visible != no;
}

const char *menu_get_prompt(struct menu *menu)
{
	if (menu->prompt)
		return menu->prompt->text;
	else if (menu->sym)
		return menu->sym->name;
	return NULL;
}

struct menu *menu_get_root_menu(struct menu *menu)
{
	return &rootmenu;
}

struct menu *menu_get_parent_menu(struct menu *menu)
{
	enum prop_type type;

	while (menu != &rootmenu) {
		menu = menu->parent;
		type = menu->prompt ? menu->prompt->type : 0;
		if (type == P_MENU || type == P_ROOTMENU)
			break;
	}
	return menu;
}

struct file *file_lookup(const char *name)
{
	struct file *file;

	for (file = file_list; file; file = file->next) {
		if (!strcmp(name, file->name))
			return file;
	}

	file = malloc(sizeof(*file));
	memset(file, 0, sizeof(*file));
	file->name = strdup(name);
	file->next = file_list;
	file_list = file;
	return file;
}

int file_write_dep(const char *name)
{
	struct file *file;
	FILE *out;

	if (!name)
		name = ".config.cmd";
	out = fopen(".config.tmp", "w");
	if (!out)
		return 1;
	fprintf(out, "deps_config := \\\n");
	for (file = file_list; file; file = file->next) {
		if (file->next)
			fprintf(out, "\t%s \\\n", file->name);
		else
			fprintf(out, "\t%s\n", file->name);
	}
	fclose(out);
	rename(".config.tmp", name);
	return 0;
}


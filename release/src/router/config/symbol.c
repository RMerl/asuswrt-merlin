/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#define LKC_DIRECT_LINK
#include "lkc.h"

struct symbol symbol_yes = {
	name: "y",
	curr: { "y", yes },
	flags: SYMBOL_YES|SYMBOL_VALID,
}, symbol_mod = {
	name: "m",
	curr: { "m", mod },
	flags: SYMBOL_MOD|SYMBOL_VALID,
}, symbol_no = {
	name: "n",
	curr: { "n", no },
	flags: SYMBOL_NO|SYMBOL_VALID,
}, symbol_empty = {
	name: "",
	curr: { "", no },
	flags: SYMBOL_VALID,
};

int sym_change_count;
struct symbol *modules_sym;

void sym_add_default(struct symbol *sym, const char *def)
{
	struct property *prop = create_prop(P_DEFAULT);
	struct property **propp;

	prop->sym = sym;
	prop->def = sym_lookup(def, 1);

	/* append property to the prop list of symbol */
	if (prop->sym) {
		for (propp = &prop->sym->prop; *propp; propp = &(*propp)->next)
			;
		*propp = prop;
	}
}

void sym_init(void)
{
	struct symbol *sym;
	struct utsname uts;
	char *p;
	static bool inited = false;

	if (inited)
		return;
	inited = true;

	uname(&uts);


	sym = sym_lookup("VERSION", 0);
	sym->type = S_STRING;
	sym->flags |= SYMBOL_AUTO;
	p = getenv("VERSION");
	if (p)
		sym_add_default(sym, p);


	sym = sym_lookup("TARGET_ARCH", 0);
	sym->type = S_STRING;
	sym->flags |= SYMBOL_AUTO;
	p = getenv("TARGET_ARCH");
	if (p)
		sym_add_default(sym, p);
}

int sym_get_type(struct symbol *sym)
{
	int type = sym->type;
	if (type == S_TRISTATE) {
		if (sym_is_choice_value(sym) && sym->visible == yes)
			type = S_BOOLEAN;
		else {
			sym_calc_value(modules_sym);
			if (S_TRI(modules_sym->curr) == no)
				type = S_BOOLEAN;
		}
	}
	return type;
}

const char *sym_type_name(int type)
{
	switch (type) {
	case S_BOOLEAN:
		return "boolean";
	case S_TRISTATE:
		return "tristate";
	case S_INT:
		return "integer";
	case S_HEX:
		return "hex";
	case S_STRING:
		return "string";
	case S_UNKNOWN:
		return "unknown";
	}
	return "???";
}

struct property *sym_get_choice_prop(struct symbol *sym)
{
	struct property *prop;

	for_all_choices(sym, prop)
		return prop;
	return NULL;
}

struct property *sym_get_default_prop(struct symbol *sym)
{
	struct property *prop;
	tristate visible;

	for_all_defaults(sym, prop) {
		visible = E_CALC(prop->visible);
		if (visible != no)
			return prop;
	}
	return NULL;
}

void sym_calc_visibility(struct symbol *sym)
{
	struct property *prop;
	tristate visible, oldvisible;

	/* any prompt visible? */
	oldvisible = sym->visible;
	visible = no;
	for_all_prompts(sym, prop)
		visible = E_OR(visible, E_CALC(prop->visible));
	if (oldvisible != visible) {
		sym->visible = visible;
		sym->flags |= SYMBOL_CHANGED;
	}
}

void sym_calc_value(struct symbol *sym)
{
	struct symbol_value newval, oldval;
	struct property *prop, *def_prop;
	struct symbol *def_sym;
	struct expr *e;

	if (sym->flags & SYMBOL_VALID)
		return;

	oldval = sym->curr;

	switch (sym->type) {
	case S_INT:
	case S_HEX:
	case S_STRING:
		newval = symbol_empty.curr;
		break;
	case S_BOOLEAN:
	case S_TRISTATE:
		newval = symbol_no.curr;
		break;
	default:
		S_VAL(newval) = sym->name;
		S_TRI(newval) = no;
		if (sym->flags & SYMBOL_CONST) {
			goto out;
		}
		//newval = symbol_empty.curr;
		// generate warning somewhere here later
		//S_TRI(newval) = yes;
		goto out;
	}
	sym->flags |= SYMBOL_VALID;
	if (!sym_is_choice_value(sym))
		sym->flags &= ~SYMBOL_WRITE;

	sym_calc_visibility(sym);

	/* set default if recursively called */
	sym->curr = newval;

	if (sym->visible != no) {
		sym->flags |= SYMBOL_WRITE;
		if (!sym_has_value(sym)) {
			if (!sym_is_choice(sym)) {
				prop = sym_get_default_prop(sym);
				if (prop) {
					sym_calc_value(prop->def);
					newval = prop->def->curr;
				}
			}
		} else
			newval = sym->def;

		S_TRI(newval) = E_AND(S_TRI(newval), sym->visible);
		/* if the symbol is visible and not optionial,
		 * possibly ignore old user choice. */
		if (!sym_is_optional(sym) && S_TRI(newval) == no)
			S_TRI(newval) = sym->visible;
		if (sym_is_choice_value(sym) && sym->visible == yes) {
			prop = sym_get_choice_prop(sym);
			S_TRI(newval) = (S_VAL(prop->def->curr) == sym) ? yes : no;
		}
	} else {
		prop = sym_get_default_prop(sym);
		if (prop) {
			sym->flags |= SYMBOL_WRITE;
			sym_calc_value(prop->def);
			newval = prop->def->curr;
		}
	}

	switch (sym_get_type(sym)) {
	case S_TRISTATE:
		if (S_TRI(newval) != mod)
			break;
		sym_calc_value(modules_sym);
		if (S_TRI(modules_sym->curr) == no)
			S_TRI(newval) = yes;
		break;
	case S_BOOLEAN:
		if (S_TRI(newval) == mod)
			S_TRI(newval) = yes;
	}

out:
	sym->curr = newval;

	if (sym_is_choice(sym) && S_TRI(newval) == yes) {
		def_sym = S_VAL(sym->def);
		if (def_sym) {
			sym_calc_visibility(def_sym);
			if (def_sym->visible == no)
				def_sym = NULL;
		}
		if (!def_sym) {
			for_all_defaults(sym, def_prop) {
				if (E_CALC(def_prop->visible) == no)
					continue;
				sym_calc_visibility(def_prop->def);
				if (def_prop->def->visible != no) {
					def_sym = def_prop->def;
					break;
				}
			}
		}

		if (!def_sym) {
			prop = sym_get_choice_prop(sym);
			for (e = prop->dep; e; e = e->left.expr) {
				sym_calc_visibility(e->right.sym);
				if (e->right.sym->visible != no) {
					def_sym = e->right.sym;
					break;
				}
			}
		}

		S_VAL(newval) = def_sym;
	}

	if (memcmp(&oldval, &newval, sizeof(newval)))
		sym->flags |= SYMBOL_CHANGED;
	sym->curr = newval;

	if (sym_is_choice(sym)) {
		int flags = sym->flags & (SYMBOL_CHANGED | SYMBOL_WRITE);
		prop = sym_get_choice_prop(sym);
		for (e = prop->dep; e; e = e->left.expr)
			e->right.sym->flags |= flags;
	}
}

void sym_clear_all_valid(void)
{
	struct symbol *sym;
	int i;

	for_all_symbols(i, sym)
		sym->flags &= ~SYMBOL_VALID;
	sym_change_count++;
}

void sym_set_all_changed(void)
{
	struct symbol *sym;
	int i;

	for_all_symbols(i, sym)
		sym->flags |= SYMBOL_CHANGED;
}

bool sym_tristate_within_range(struct symbol *sym, tristate val)
{
	int type = sym_get_type(sym);

	if (sym->visible == no)
		return false;

	if (type != S_BOOLEAN && type != S_TRISTATE)
		return false;

	switch (val) {
	case no:
		if (sym_is_choice_value(sym) && sym->visible == yes)
			return false;
		return sym_is_optional(sym);
	case mod:
		if (sym_is_choice_value(sym) && sym->visible == yes)
			return false;
		return type == S_TRISTATE;
	case yes:
		return type == S_BOOLEAN || sym->visible == yes;
	}
	return false;
}

bool sym_set_tristate_value(struct symbol *sym, tristate val)
{
	tristate oldval = sym_get_tristate_value(sym);

	if (oldval != val && !sym_tristate_within_range(sym, val))
		return false;

	if (sym->flags & SYMBOL_NEW) {
		sym->flags &= ~SYMBOL_NEW;
		sym->flags |= SYMBOL_CHANGED;
	}
	if (sym_is_choice_value(sym) && val == yes) {
		struct property *prop = sym_get_choice_prop(sym);

		S_VAL(prop->def->def) = sym;
		prop->def->flags &= ~SYMBOL_NEW;
	}

	S_TRI(sym->def) = val;
	if (oldval != val) {
		sym_clear_all_valid();
		if (sym == modules_sym)
			sym_set_all_changed();
	}

	return true;
}

tristate sym_toggle_tristate_value(struct symbol *sym)
{
	tristate oldval, newval;

	oldval = newval = sym_get_tristate_value(sym);
	do {
		switch (newval) {
		case no:
			newval = mod;
			break;
		case mod:
			newval = yes;
			break;
		case yes:
			newval = no;
			break;
		}
		if (sym_set_tristate_value(sym, newval))
			break;
	} while (oldval != newval);
	return newval;
}

bool sym_string_valid(struct symbol *sym, const char *str)
{
	char ch;

	switch (sym->type) {
	case S_STRING:
		return true;
	case S_INT:
		ch = *str++;
		if (ch == '-')
			ch = *str++;
		if (!isdigit((int)ch))
			return false;
		if (ch == '0' && *str != 0)
			return false;
		while ((ch = *str++)) {
			if (!isdigit((int)ch))
				return false;
		}
		return true;
	case S_HEX:
		if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
			str += 2;
		ch = *str++;
		do {
			if (!isxdigit((int)ch))
				return false;
		} while ((ch = *str++));
		return true;
	case S_BOOLEAN:
	case S_TRISTATE:
		switch (str[0]) {
		case 'y':
		case 'Y':
			return sym_tristate_within_range(sym, yes);
		case 'm':
		case 'M':
			return sym_tristate_within_range(sym, mod);
		case 'n':
		case 'N':
			return sym_tristate_within_range(sym, no);
		}
		return false;
	default:
		return false;
	}
}

bool sym_set_string_value(struct symbol *sym, const char *newval)
{
	const char *oldval;
	char *val;
	int size;

	switch (sym->type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		switch (newval[0]) {
		case 'y':
		case 'Y':
			return sym_set_tristate_value(sym, yes);
		case 'm':
		case 'M':
			return sym_set_tristate_value(sym, mod);
		case 'n':
		case 'N':
			return sym_set_tristate_value(sym, no);
		}
		return false;
	default:
		;
	}

	if (!sym_string_valid(sym, newval))
		return false;

	if (sym->flags & SYMBOL_NEW) {
		sym->flags &= ~SYMBOL_NEW;
		sym->flags |= SYMBOL_CHANGED;
	}

	oldval = S_VAL(sym->def);
	size = strlen(newval) + 1;
	if (sym->type == S_HEX && (newval[0] != '0' || (newval[1] != 'x' && newval[1] != 'X'))) {
		size += 2;
		S_VAL(sym->def) = val = malloc(size);
		*val++ = '0';
		*val++ = 'x';
	} else if (!oldval || strcmp(oldval, newval))
		S_VAL(sym->def) = val = malloc(size);
	else
		return true;

	strcpy(val, newval);
	free((void *)oldval);
	sym_clear_all_valid();

	return true;
}

const char *sym_get_string_value(struct symbol *sym)
{
	tristate val;

	switch (sym->type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		val = sym_get_tristate_value(sym);
		switch (val) {
		case no:
			return "n";
		case mod:
			return "m";
		case yes:
			return "y";
		}
		break;
	default:
		;
	}
	return (const char *)S_VAL(sym->curr);
}

bool sym_is_changable(struct symbol *sym)
{
	if (sym->visible == no)
		return false;
	/* at least 'n' and 'y'/'m' is selectable */
	if (sym_is_optional(sym))
		return true;
	/* no 'n', so 'y' and 'm' must be selectable */
	if (sym_get_type(sym) == S_TRISTATE && sym->visible == yes)
		return true;
	return false;
}

struct symbol *sym_lookup(const char *name, int isconst)
{
	struct symbol *symbol;
	const char *ptr;
	char *new_name;
	int hash = 0;

	//printf("lookup: %s -> ", name);
	if (name) {
		if (name[0] && !name[1]) {
			switch (name[0]) {
			case 'y': return &symbol_yes;
			case 'm': return &symbol_mod;
			case 'n': return &symbol_no;
			}
		}
		for (ptr = name; *ptr; ptr++)
			hash += *ptr;
		hash &= 0xff;

		for (symbol = symbol_hash[hash]; symbol; symbol = symbol->next) {
			if (!strcmp(symbol->name, name)) {
				if ((isconst && symbol->flags & SYMBOL_CONST) ||
				    (!isconst && !(symbol->flags & SYMBOL_CONST))) {
					//printf("h:%p\n", symbol);
					return symbol;
				}
			}
		}
		new_name = strdup(name);
	} else {
		new_name = NULL;
		hash = 256;
	}

	symbol = malloc(sizeof(*symbol));
	memset(symbol, 0, sizeof(*symbol));
	symbol->name = new_name;
	symbol->type = S_UNKNOWN;
	symbol->flags = SYMBOL_NEW;
	if (isconst)
		symbol->flags |= SYMBOL_CONST;

	symbol->next = symbol_hash[hash];
	symbol_hash[hash] = symbol;

	//printf("n:%p\n", symbol);
	return symbol;
}

struct symbol *sym_find(const char *name)
{
	struct symbol *symbol = NULL;
	const char *ptr;
	int hash = 0;

	if (!name)
		return NULL;

	if (name[0] && !name[1]) {
		switch (name[0]) {
		case 'y': return &symbol_yes;
		case 'm': return &symbol_mod;
		case 'n': return &symbol_no;
		}
	}
	for (ptr = name; *ptr; ptr++)
		hash += *ptr;
	hash &= 0xff;

	for (symbol = symbol_hash[hash]; symbol; symbol = symbol->next) {
		if (!strcmp(symbol->name, name) &&
		    !(symbol->flags & SYMBOL_CONST))
				break;
	}

	return symbol;
}

const char *prop_get_type_name(enum prop_type type)
{
	switch (type) {
	case P_PROMPT:
		return "prompt";
	case P_COMMENT:
		return "comment";
	case P_MENU:
		return "menu";
	case P_ROOTMENU:
		return "rootmenu";
	case P_DEFAULT:
		return "default";
	case P_CHOICE:
		return "choice";
	default:
		return "unknown";
	}
}

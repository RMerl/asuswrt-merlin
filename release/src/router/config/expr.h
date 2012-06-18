/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#ifndef EXPR_H
#define EXPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

struct file {
	struct file *next;
	struct file *parent;
#ifdef CML1
	struct statement *stmt;
	struct statement *last_stmt;
#endif
	char *name;
	int lineno;
	int flags;
};

#define FILE_BUSY		0x0001
#define FILE_SCANNED		0x0002
#define FILE_PRINTED		0x0004

typedef enum tristate {
	no, mod, yes
} tristate;

enum expr_type {
	E_NONE, E_OR, E_AND, E_NOT, E_EQUAL, E_UNEQUAL, E_CHOICE, E_SYMBOL
};

union expr_data {
	struct expr *expr;
	struct symbol *sym;
};

struct expr {
#ifdef CML1
	int token;
#else
	enum expr_type type;
#endif
	union expr_data left, right;
};

#define E_TRI(ev)	((ev).tri)
#define E_EXPR(ev)	((ev).expr)
#define E_CALC(ev)	(E_TRI(ev) = expr_calc_value(E_EXPR(ev)))

#define E_OR(dep1, dep2)	(((dep1)>(dep2))?(dep1):(dep2))
#define E_AND(dep1, dep2)	(((dep1)<(dep2))?(dep1):(dep2))
#define E_NOT(dep)		(2-(dep))

struct expr_value {
	struct expr *expr;
	tristate tri;
};

#define S_VAL(sv)	((sv).value)
#define S_TRI(sv)	((sv).tri)
#define S_EQ(sv1, sv2)	(S_VAL(sv1) == S_VAL(sv2) || !strcmp(S_VAL(sv1), S_VAL(sv2)))

struct symbol_value {
	void *value;
	tristate tri;
};

enum symbol_type {
	S_UNKNOWN, S_BOOLEAN, S_TRISTATE, S_INT, S_HEX, S_STRING, S_OTHER
};

struct symbol {
	struct symbol *next;
	char *name;
	char *help;
#ifdef CML1
	int type;
#else
	enum symbol_type type;
#endif
	struct symbol_value curr, def;
	tristate visible;
	int flags;
	struct property *prop;
	struct expr *dep, *dep2;
	struct menu *menu;
};

#define for_all_symbols(i, sym) for (i = 0; i < 257; i++) for (sym = symbol_hash[i]; sym; sym = sym->next) if (sym->type != S_OTHER)

#ifdef CML1
#define SYMBOL_UNKNOWN		S_UNKNOWN
#define SYMBOL_BOOLEAN		S_BOOLEAN
#define SYMBOL_TRISTATE		S_TRISTATE
#define SYMBOL_INT		S_INT
#define SYMBOL_HEX		S_HEX
#define SYMBOL_STRING		S_STRING
#define SYMBOL_OTHER		S_OTHER
#endif

#define SYMBOL_YES		0x0001
#define SYMBOL_MOD		0x0002
#define SYMBOL_NO		0x0004
#define SYMBOL_CONST		0x0007
#define SYMBOL_CHECK		0x0008
#define SYMBOL_CHOICE		0x0010
#define SYMBOL_CHOICEVAL	0x0020
#define SYMBOL_PRINTED		0x0040
#define SYMBOL_VALID		0x0080
#define SYMBOL_OPTIONAL		0x0100
#define SYMBOL_WRITE		0x0200
#define SYMBOL_CHANGED		0x0400
#define SYMBOL_NEW		0x0800
#define SYMBOL_AUTO		0x1000

#define SYMBOL_MAXLENGTH	256
#define SYMBOL_HASHSIZE		257
#define SYMBOL_HASHMASK		0xff

enum prop_type {
	P_UNKNOWN, P_PROMPT, P_COMMENT, P_MENU, P_ROOTMENU, P_DEFAULT, P_CHOICE
};

struct property {
	struct property *next;
	struct symbol *sym;
#ifdef CML1
	int token;
#else
	enum prop_type type;
#endif
	const char *text;
	struct symbol *def;
	struct expr_value visible;
	struct expr *dep;
	struct expr *dep2;
	struct menu *menu;
	struct file *file;
	int lineno;
#ifdef CML1
	struct property *next_pos;
#endif
};

#define for_all_properties(sym, st, tok) \
	for (st = sym->prop; st; st = st->next) \
		if (st->type == (tok))
#define for_all_prompts(sym, st) for_all_properties(sym, st, P_PROMPT)
#define for_all_defaults(sym, st) for_all_properties(sym, st, P_DEFAULT)
#define for_all_choices(sym, st) for_all_properties(sym, st, P_CHOICE)

struct menu {
	struct menu *next;
	struct menu *parent;
	struct menu *list;
	struct symbol *sym;
	struct property *prompt;
	struct expr *dep;
	//char *help;
	struct file *file;
	int lineno;
	void *data;
};

#ifndef SWIG

extern struct file *file_list;
extern struct file *current_file;
struct file *lookup_file(const char *name);

extern struct symbol symbol_yes, symbol_no, symbol_mod;
extern struct symbol *modules_sym;
extern int cdebug;
extern int print_type;
struct expr *expr_alloc_symbol(struct symbol *sym);
#ifdef CML1
struct expr *expr_alloc_one(int token, struct expr *ce);
struct expr *expr_alloc_two(int token, struct expr *e1, struct expr *e2);
struct expr *expr_alloc_comp(int token, struct symbol *s1, struct symbol *s2);
#else
struct expr *expr_alloc_one(enum expr_type type, struct expr *ce);
struct expr *expr_alloc_two(enum expr_type type, struct expr *e1, struct expr *e2);
struct expr *expr_alloc_comp(enum expr_type type, struct symbol *s1, struct symbol *s2);
#endif
struct expr *expr_alloc_and(struct expr *e1, struct expr *e2);
struct expr *expr_copy(struct expr *org);
void expr_free(struct expr *e);
int expr_eq(struct expr *e1, struct expr *e2);
void expr_eliminate_eq(struct expr **ep1, struct expr **ep2);
tristate expr_calc_value(struct expr *e);
struct expr *expr_eliminate_yn(struct expr *e);
struct expr *expr_trans_bool(struct expr *e);
struct expr *expr_eliminate_dups(struct expr *e);
struct expr *expr_transform(struct expr *e);
int expr_contains_symbol(struct expr *dep, struct symbol *sym);
bool expr_depends_symbol(struct expr *dep, struct symbol *sym);
struct expr *expr_extract_eq_and(struct expr **ep1, struct expr **ep2);
struct expr *expr_extract_eq_or(struct expr **ep1, struct expr **ep2);
void expr_extract_eq(enum expr_type type, struct expr **ep, struct expr **ep1, struct expr **ep2);
struct expr *expr_trans_compare(struct expr *e, enum expr_type type, struct symbol *sym);

void expr_fprint(struct expr *e, FILE *out);
void print_expr(int mask, struct expr *e, int prevtoken);

#ifdef CML1
static inline int expr_is_yes(struct expr *e)
{
	return !e || (e->token == WORD && e->left.sym == &symbol_yes);
}

static inline int expr_is_no(struct expr *e)
{
	return e && (e->token == WORD && e->left.sym == &symbol_no);
}
#else
static inline int expr_is_yes(struct expr *e)
{
	return !e || (e->type == E_SYMBOL && e->left.sym == &symbol_yes);
}

static inline int expr_is_no(struct expr *e)
{
	return e && (e->type == E_SYMBOL && e->left.sym == &symbol_no);
}
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* EXPR_H */

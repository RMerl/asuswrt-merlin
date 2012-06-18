%{
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define printd(mask, fmt...) if (cdebug & (mask)) printf(fmt)

#define PRINTD		0x0001
#define DEBUG_PARSE	0x0002

int cdebug = PRINTD;

extern int zconflex(void);
static void zconfprint(const char *err, ...);
static void zconferror(const char *err);
static bool zconf_endtoken(int token, int starttoken, int endtoken);

struct symbol *symbol_hash[257];

#define YYERROR_VERBOSE
%}
%expect 36

%union
{
	int token;
	char *string;
	struct symbol *symbol;
	struct expr *expr;
	struct menu *menu;
}

%token T_MAINMENU
%token T_MENU
%token T_ENDMENU
%token T_SOURCE
%token T_CHOICE
%token T_ENDCHOICE
%token T_COMMENT
%token T_CONFIG
%token T_HELP
%token <string> T_HELPTEXT
%token T_IF
%token T_ENDIF
%token T_DEPENDS
%token T_REQUIRES
%token T_OPTIONAL
%token T_PROMPT
%token T_DEFAULT
%token T_TRISTATE
%token T_BOOLEAN
%token T_INT
%token T_HEX
%token <string> T_WORD
%token <string> T_STRING
%token T_UNEQUAL
%token T_EOF
%token T_EOL
%token T_CLOSE_PAREN
%token T_OPEN_PAREN
%token T_ON

%left T_OR
%left T_AND
%left T_EQUAL T_UNEQUAL
%nonassoc T_NOT

%type <string> prompt
%type <string> source
%type <symbol> symbol
%type <expr> expr
%type <expr> if_expr
%type <token> end

%{
#define LKC_DIRECT_LINK
#include "lkc.h"
%}
%%
input:	  /* empty */
	| input block
;

block:	  common_block
	| choice_stmt
	| menu_stmt
	| T_MAINMENU prompt nl_or_eof
	| T_ENDMENU		{ zconfprint("unexpected 'endmenu' statement"); }
	| T_ENDIF		{ zconfprint("unexpected 'endif' statement"); }
	| T_ENDCHOICE		{ zconfprint("unexpected 'endchoice' statement"); }
	| error nl_or_eof	{ zconfprint("syntax error"); yyerrok; }
;

common_block:
	  if_stmt
	| comment_stmt
	| config_stmt
	| source_stmt
	| nl_or_eof
;


/* config entry */

config_entry_start: T_CONFIG T_WORD
{
	struct symbol *sym = sym_lookup($2, 0);
	sym->flags |= SYMBOL_OPTIONAL;
	menu_add_entry(sym);
	printd(DEBUG_PARSE, "%s:%d:config %s\n", zconf_curname(), zconf_lineno(), $2);
};

config_stmt: config_entry_start T_EOL config_option_list
{
	menu_end_entry();
	printd(DEBUG_PARSE, "%s:%d:endconfig\n", zconf_curname(), zconf_lineno());
};

config_option_list:
	  /* empty */
	| config_option_list config_option T_EOL
	| config_option_list depends T_EOL
	| config_option_list help
	| config_option_list T_EOL
{ };

config_option: T_TRISTATE prompt_stmt_opt
{
	menu_set_type(S_TRISTATE);
	printd(DEBUG_PARSE, "%s:%d:tristate\n", zconf_curname(), zconf_lineno());
};

config_option: T_BOOLEAN prompt_stmt_opt
{
	menu_set_type(S_BOOLEAN);
	printd(DEBUG_PARSE, "%s:%d:boolean\n", zconf_curname(), zconf_lineno());
};

config_option: T_INT prompt_stmt_opt
{
	menu_set_type(S_INT);
	printd(DEBUG_PARSE, "%s:%d:int\n", zconf_curname(), zconf_lineno());
};

config_option: T_HEX prompt_stmt_opt
{
	menu_set_type(S_HEX);
	printd(DEBUG_PARSE, "%s:%d:hex\n", zconf_curname(), zconf_lineno());
};

config_option: T_STRING prompt_stmt_opt
{
	menu_set_type(S_STRING);
	printd(DEBUG_PARSE, "%s:%d:string\n", zconf_curname(), zconf_lineno());
};

config_option: T_PROMPT prompt if_expr
{
	menu_add_prop(P_PROMPT, $2, NULL, $3);
	printd(DEBUG_PARSE, "%s:%d:prompt\n", zconf_curname(), zconf_lineno());
};

config_option: T_DEFAULT symbol if_expr
{
	menu_add_prop(P_DEFAULT, NULL, $2, $3);
	printd(DEBUG_PARSE, "%s:%d:default\n", zconf_curname(), zconf_lineno());
};

/* choice entry */

choice: T_CHOICE
{
	struct symbol *sym = sym_lookup(NULL, 0);
	sym->flags |= SYMBOL_CHOICE;
	menu_add_entry(sym);
	menu_add_prop(P_CHOICE, NULL, NULL, NULL);
	printd(DEBUG_PARSE, "%s:%d:choice\n", zconf_curname(), zconf_lineno());
};

choice_entry: choice T_EOL choice_option_list
{
	menu_end_entry();
	menu_add_menu();
};

choice_end: end
{
	if (zconf_endtoken($1, T_CHOICE, T_ENDCHOICE)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endchoice\n", zconf_curname(), zconf_lineno());
	}
};

choice_stmt:
	  choice_entry choice_block choice_end T_EOL
	| choice_entry choice_block
{
	printf("%s:%d: missing 'endchoice' for this 'choice' statement\n", current_menu->file->name, current_menu->lineno);
	zconfnerrs++;
};

choice_option_list:
	  /* empty */
	| choice_option_list choice_option T_EOL
	| choice_option_list depends T_EOL
	| choice_option_list help
	| choice_option_list T_EOL
;

choice_option: T_PROMPT prompt if_expr
{
	menu_add_prop(P_PROMPT, $2, NULL, $3);
	printd(DEBUG_PARSE, "%s:%d:prompt\n", zconf_curname(), zconf_lineno());
};

choice_option: T_OPTIONAL
{
	current_entry->sym->flags |= SYMBOL_OPTIONAL;
	printd(DEBUG_PARSE, "%s:%d:optional\n", zconf_curname(), zconf_lineno());
};

choice_option: T_DEFAULT symbol
{
	menu_add_prop(P_DEFAULT, NULL, $2, NULL);
	//current_choice->prop->def = $2;
	printd(DEBUG_PARSE, "%s:%d:default\n", zconf_curname(), zconf_lineno());
};

choice_block:
	  /* empty */
	| choice_block common_block
;

/* if entry */

if: T_IF expr
{
	printd(DEBUG_PARSE, "%s:%d:if\n", zconf_curname(), zconf_lineno());
	menu_add_entry(NULL);
	//current_entry->prompt = menu_add_prop(T_IF, NULL, NULL, $2);
	menu_add_dep($2);
	menu_end_entry();
	menu_add_menu();
};

if_end: end
{
	if (zconf_endtoken($1, T_IF, T_ENDIF)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endif\n", zconf_curname(), zconf_lineno());
	}
};

if_stmt:
	  if T_EOL if_block if_end T_EOL
	| if T_EOL if_block
{
	printf("%s:%d: missing 'endif' for this 'if' statement\n", current_menu->file->name, current_menu->lineno);
	zconfnerrs++;
};

if_block:
	  /* empty */
	| if_block common_block
	| if_block menu_stmt
	| if_block choice_stmt
;

/* menu entry */

menu: T_MENU prompt
{
	menu_add_entry(NULL);
	menu_add_prop(P_MENU, $2, NULL, NULL);
	printd(DEBUG_PARSE, "%s:%d:menu\n", zconf_curname(), zconf_lineno());
};

menu_entry: menu T_EOL depends_list
{
	menu_end_entry();
	menu_add_menu();
};

menu_end: end
{
	if (zconf_endtoken($1, T_MENU, T_ENDMENU)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endmenu\n", zconf_curname(), zconf_lineno());
	}
};

menu_stmt:
	  menu_entry menu_block menu_end T_EOL
	| menu_entry menu_block
{
	printf("%s:%d: missing 'endmenu' for this 'menu' statement\n", current_menu->file->name, current_menu->lineno);
	zconfnerrs++;
};

menu_block:
	  /* empty */
	| menu_block common_block
	| menu_block menu_stmt
	| menu_block choice_stmt
	| menu_block error T_EOL		{ zconfprint("invalid menu option"); yyerrok; }
;

source: T_SOURCE prompt
{
	$$ = $2;
	printd(DEBUG_PARSE, "%s:%d:source %s\n", zconf_curname(), zconf_lineno(), $2);
};

source_stmt: source T_EOL
{
	zconf_nextfile($1);
};

/* comment entry */

comment: T_COMMENT prompt
{
	menu_add_entry(NULL);
	menu_add_prop(P_COMMENT, $2, NULL, NULL);
	printd(DEBUG_PARSE, "%s:%d:comment\n", zconf_curname(), zconf_lineno());
};

comment_stmt: comment T_EOL depends_list
{
	menu_end_entry();
};

/* help option */

help_start: T_HELP T_EOL
{
	printd(DEBUG_PARSE, "%s:%d:help\n", zconf_curname(), zconf_lineno());
	zconf_starthelp();
};

help: help_start T_HELPTEXT
{
	current_entry->sym->help = $2;
};

/* depends option */

depends_list:	  /* empty */
		| depends_list depends T_EOL
		| depends_list T_EOL
{ };

depends: T_DEPENDS T_ON expr
{
	menu_add_dep($3);
	printd(DEBUG_PARSE, "%s:%d:depends on\n", zconf_curname(), zconf_lineno());
}
	| T_DEPENDS expr
{
	menu_add_dep($2);
	printd(DEBUG_PARSE, "%s:%d:depends\n", zconf_curname(), zconf_lineno());
}
	| T_REQUIRES expr
{
	menu_add_dep($2);
	printd(DEBUG_PARSE, "%s:%d:requires\n", zconf_curname(), zconf_lineno());
};

/* prompt statement */

prompt_stmt_opt:
	  /* empty */
	| prompt
{
	menu_add_prop(P_PROMPT, $1, NULL, NULL);
}
	| prompt T_IF expr
{
	menu_add_prop(P_PROMPT, $1, NULL, $3);
};

prompt:	  T_WORD
	| T_STRING
;

end:	  T_ENDMENU		{ $$ = T_ENDMENU; }
	| T_ENDCHOICE		{ $$ = T_ENDCHOICE; }
	| T_ENDIF		{ $$ = T_ENDIF; }
;

nl_or_eof:
	T_EOL | T_EOF;

if_expr:  /* empty */			{ $$ = NULL; }
	| T_IF expr			{ $$ = $2; }
;

expr:	  symbol				{ $$ = expr_alloc_symbol($1); }
	| symbol T_EQUAL symbol			{ $$ = expr_alloc_comp(E_EQUAL, $1, $3); }
	| symbol T_UNEQUAL symbol		{ $$ = expr_alloc_comp(E_UNEQUAL, $1, $3); }
	| T_OPEN_PAREN expr T_CLOSE_PAREN	{ $$ = $2; }
	| T_NOT expr				{ $$ = expr_alloc_one(E_NOT, $2); }
	| expr T_OR expr			{ $$ = expr_alloc_two(E_OR, $1, $3); }
	| expr T_AND expr			{ $$ = expr_alloc_two(E_AND, $1, $3); }
;

symbol:	  T_WORD	{ $$ = sym_lookup($1, 0); free($1); }
	| T_STRING	{ $$ = sym_lookup($1, 1); free($1); }
;

%%

void conf_parse(const char *name)
{
	zconf_initscan(name);

	sym_init();
	menu_init();
	rootmenu.prompt = menu_add_prop(P_MENU, "Broadcom Linux Router Configuration", NULL, NULL);

	//zconfdebug = 1;
	zconfparse();
	if (zconfnerrs)
		exit(1);
	menu_finalize(&rootmenu);

	modules_sym = sym_lookup("MODULES", 0);

	sym_change_count = 1;
}

const char *zconf_tokenname(int token)
{
	switch (token) {
	case T_MENU:		return "menu";
	case T_ENDMENU:		return "endmenu";
	case T_CHOICE:		return "choice";
	case T_ENDCHOICE:	return "endchoice";
	case T_IF:		return "if";
	case T_ENDIF:		return "endif";
	}
	return "<token>";
} 

static bool zconf_endtoken(int token, int starttoken, int endtoken)
{
	if (token != endtoken) {
		zconfprint("unexpected '%s' within %s block", zconf_tokenname(token), zconf_tokenname(starttoken));
		zconfnerrs++;
		return false;
	}
	if (current_menu->file != current_file) {
		zconfprint("'%s' in different file than '%s'", zconf_tokenname(token), zconf_tokenname(starttoken));
		zconfprint("location of the '%s'", zconf_tokenname(starttoken));
		zconfnerrs++;
		return false;
	}
	return true;
}

static void zconfprint(const char *err, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%d: ", zconf_curname(), zconf_lineno());
	va_start(ap, err);
	vfprintf(stderr, err, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

static void zconferror(const char *err)
{
	fprintf(stderr, "%s:%d: %s\n", zconf_curname(), zconf_lineno(), err);
}

void print_quoted_string(FILE *out, const char *str)
{
	const char *p;
	int len;

	putc('"', out);
	while ((p = strchr(str, '"'))) {
		len = p - str;
		if (len)
			fprintf(out, "%.*s", len, str);
		fputs("\\\"", out);
		str = p + 1;
	}
	fputs(str, out);
	putc('"', out);
}

void print_symbol(FILE *out, struct menu *menu)
{
	struct symbol *sym = menu->sym;
	struct property *prop;

	//sym->flags |= SYMBOL_PRINTED;

	if (sym_is_choice(sym))
		fprintf(out, "choice\n");
	else
		fprintf(out, "config %s\n", sym->name);
	switch (sym->type) {
	case S_BOOLEAN:
		fputs("  boolean\n", out);
		break;
	case S_TRISTATE:
		fputs("  tristate\n", out);
		break;
	case S_STRING:
		fputs("  string\n", out);
		break;
	case S_INT:
		fputs("  integer\n", out);
		break;
	case S_HEX:
		fputs("  hex\n", out);
		break;
	default:
		fputs("  ???\n", out);
		break;
	}
	for (prop = sym->prop; prop; prop = prop->next) {
		if (prop->menu != menu)
			continue;
		switch (prop->type) {
		case P_PROMPT:
			fputs("  prompt ", out);
			print_quoted_string(out, prop->text);
			if (prop->def) {
				fputc(' ', out);
				if (prop->def->flags & SYMBOL_CONST)
					print_quoted_string(out, prop->def->name);
				else
					fputs(prop->def->name, out);
			}
			if (!expr_is_yes(E_EXPR(prop->visible))) {
				fputs(" if ", out);
				expr_fprint(E_EXPR(prop->visible), out);
			}
			fputc('\n', out);
			break;
		case P_DEFAULT:
			fputs( "  default ", out);
			print_quoted_string(out, prop->def->name);
			if (!expr_is_yes(E_EXPR(prop->visible))) {
				fputs(" if ", out);
				expr_fprint(E_EXPR(prop->visible), out);
			}
			fputc('\n', out);
			break;
		case P_CHOICE:
			fputs("  #choice value\n", out);
			break;
		default:
			fprintf(out, "  unknown prop %d!\n", prop->type);
			break;
		}
	}
	if (sym->help) {
		int len = strlen(sym->help);
		while (sym->help[--len] == '\n')
			sym->help[len] = 0;
		fprintf(out, "  help\n%s\n", sym->help);
	}
	fputc('\n', out);
}

void zconfdump(FILE *out)
{
	//struct file *file;
	struct property *prop;
	struct symbol *sym;
	struct menu *menu;

	menu = rootmenu.list;
	while (menu) {
		if ((sym = menu->sym))
			print_symbol(out, menu);
		else if ((prop = menu->prompt)) {
			switch (prop->type) {
			//case T_MAINMENU:
			//	fputs("\nmainmenu ", out);
			//	print_quoted_string(out, prop->text);
			//	fputs("\n", out);
			//	break;
			case P_COMMENT:
				fputs("\ncomment ", out);
				print_quoted_string(out, prop->text);
				fputs("\n", out);
				break;
			case P_MENU:
				fputs("\nmenu ", out);
				print_quoted_string(out, prop->text);
				fputs("\n", out);
				break;
			//case T_SOURCE:
			//	fputs("\nsource ", out);
			//	print_quoted_string(out, prop->text);
			//	fputs("\n", out);
			//	break;
			//case T_IF:
			//	fputs("\nif\n", out);
			default:
				;
			}
			if (!expr_is_yes(E_EXPR(prop->visible))) {
				fputs("  depends ", out);
				expr_fprint(E_EXPR(prop->visible), out);
				fputc('\n', out);
			}
			fputs("\n", out);
		}

		if (menu->list)
			menu = menu->list;
		else if (menu->next)
			menu = menu->next;
		else while ((menu = menu->parent)) {
			if (menu->prompt && menu->prompt->type == P_MENU)
				fputs("\nendmenu\n", out);
			if (menu->next) {
				menu = menu->next;
				break;
			}
		}
	}
}

#include "lex.zconf.c"
#include "confdata.c"
#include "expr.c"
#include "symbol.c"
#include "menu.c"

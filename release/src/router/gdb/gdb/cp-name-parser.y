/* YACC parser for C++ names, for GDB.

   Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

   Parts of the lexer are based on c-exp.y from GDB.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* Note that malloc's and realloc's in this file are transformed to
   xmalloc and xrealloc respectively by the same sed command in the
   makefile that remaps any other malloc/realloc inserted by the parser
   generator.  Doing this with #defines and trying to control the interaction
   with include files (<malloc.h> and <stdlib.h> for example) just became
   too messy, particularly when such includes can be inserted at random
   times by the parser generator.  */

%{

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "safe-ctype.h"
#include "libiberty.h"
#include "demangle.h"

/* Bison does not make it easy to create a parser without global
   state, unfortunately.  Here are all the global variables used
   in this parser.  */

/* LEXPTR is the current pointer into our lex buffer.  PREV_LEXPTR
   is the start of the last token lexed, only used for diagnostics.
   ERROR_LEXPTR is the first place an error occurred.  GLOBAL_ERRMSG
   is the first error message encountered.  */

static const char *lexptr, *prev_lexptr, *error_lexptr, *global_errmsg;

/* The components built by the parser are allocated ahead of time,
   and cached in this structure.  */

struct demangle_info {
  int used;
  struct demangle_component comps[1];
};

static struct demangle_info *demangle_info;
#define d_grab() (&demangle_info->comps[demangle_info->used++])

/* The parse tree created by the parser is stored here after a successful
   parse.  */

static struct demangle_component *global_result;

/* Prototypes for helper functions used when constructing the parse
   tree.  */

static struct demangle_component *d_qualify (struct demangle_component *, int,
					     int);

static struct demangle_component *d_int_type (int);

static struct demangle_component *d_unary (const char *,
					   struct demangle_component *);
static struct demangle_component *d_binary (const char *,
					    struct demangle_component *,
					    struct demangle_component *);

/* Flags passed to d_qualify.  */

#define QUAL_CONST 1
#define QUAL_RESTRICT 2
#define QUAL_VOLATILE 4

/* Flags passed to d_int_type.  */

#define INT_CHAR	(1 << 0)
#define INT_SHORT	(1 << 1)
#define INT_LONG	(1 << 2)
#define INT_LLONG	(1 << 3)

#define INT_SIGNED	(1 << 4)
#define INT_UNSIGNED	(1 << 5)

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth cpname_maxdepth
#define	yyparse	cpname_parse
#define	yylex	cpname_lex
#define	yyerror	cpname_error
#define	yylval	cpname_lval
#define	yychar	cpname_char
#define	yydebug	cpname_debug
#define	yypact	cpname_pact	
#define	yyr1	cpname_r1			
#define	yyr2	cpname_r2			
#define	yydef	cpname_def		
#define	yychk	cpname_chk		
#define	yypgo	cpname_pgo		
#define	yyact	cpname_act		
#define	yyexca	cpname_exca
#define yyerrflag cpname_errflag
#define yynerrs	cpname_nerrs
#define	yyps	cpname_ps
#define	yypv	cpname_pv
#define	yys	cpname_s
#define	yy_yys	cpname_yys
#define	yystate	cpname_state
#define	yytmp	cpname_tmp
#define	yyv	cpname_v
#define	yy_yyv	cpname_yyv
#define	yyval	cpname_val
#define	yylloc	cpname_lloc
#define yyreds	cpname_reds		/* With YYDEBUG defined */
#define yytoks	cpname_toks		/* With YYDEBUG defined */
#define yyname	cpname_name		/* With YYDEBUG defined */
#define yyrule	cpname_rule		/* With YYDEBUG defined */
#define yylhs	cpname_yylhs
#define yylen	cpname_yylen
#define yydefred cpname_yydefred
#define yydgoto	cpname_yydgoto
#define yysindex cpname_yysindex
#define yyrindex cpname_yyrindex
#define yygindex cpname_yygindex
#define yytable	 cpname_yytable
#define yycheck	 cpname_yycheck

int yyparse (void);
static int yylex (void);
static void yyerror (char *);

/* Enable yydebug for the stand-alone parser.  */
#ifdef TEST_CPNAMES
# define YYDEBUG	1
#endif

/* Helper functions.  These wrap the demangler tree interface, handle
   allocation from our global store, and return the allocated component.  */

static struct demangle_component *
fill_comp (enum demangle_component_type d_type, struct demangle_component *lhs,
	   struct demangle_component *rhs)
{
  struct demangle_component *ret = d_grab ();
  cplus_demangle_fill_component (ret, d_type, lhs, rhs);
  return ret;
}

static struct demangle_component *
make_empty (enum demangle_component_type d_type)
{
  struct demangle_component *ret = d_grab ();
  ret->type = d_type;
  return ret;
}

static struct demangle_component *
make_operator (const char *name, int args)
{
  struct demangle_component *ret = d_grab ();
  cplus_demangle_fill_operator (ret, name, args);
  return ret;
}

static struct demangle_component *
make_dtor (enum gnu_v3_dtor_kinds kind, struct demangle_component *name)
{
  struct demangle_component *ret = d_grab ();
  cplus_demangle_fill_dtor (ret, kind, name);
  return ret;
}

static struct demangle_component *
make_builtin_type (const char *name)
{
  struct demangle_component *ret = d_grab ();
  cplus_demangle_fill_builtin_type (ret, name);
  return ret;
}

static struct demangle_component *
make_name (const char *name, int len)
{
  struct demangle_component *ret = d_grab ();
  cplus_demangle_fill_name (ret, name, len);
  return ret;
}

#define d_left(dc) (dc)->u.s_binary.left
#define d_right(dc) (dc)->u.s_binary.right

%}

%union
  {
    struct demangle_component *comp;
    struct nested {
      struct demangle_component *comp;
      struct demangle_component **last;
    } nested;
    struct {
      struct demangle_component *comp, *last;
    } nested1;
    struct {
      struct demangle_component *comp, **last;
      struct nested fn;
      struct demangle_component *start;
      int fold_flag;
    } abstract;
    int lval;
    struct {
      int val;
      struct demangle_component *type;
    } typed_val_int;
    const char *opname;
  }

%type <comp> exp exp1 type start start_opt operator colon_name
%type <comp> unqualified_name colon_ext_name
%type <comp> template template_arg
%type <comp> builtin_type
%type <comp> typespec_2 array_indicator
%type <comp> colon_ext_only ext_only_name

%type <comp> demangler_special function conversion_op
%type <nested> conversion_op_name

%type <abstract> abstract_declarator direct_abstract_declarator
%type <abstract> abstract_declarator_fn
%type <nested> declarator direct_declarator function_arglist

%type <nested> declarator_1 direct_declarator_1

%type <nested> template_params function_args
%type <nested> ptr_operator

%type <nested1> nested_name

%type <lval> qualifier qualifiers qualifiers_opt

%type <lval> int_part int_seq

%token <comp> INT
%token <comp> FLOAT

%token <comp> NAME
%type <comp> name

%token STRUCT CLASS UNION ENUM SIZEOF UNSIGNED COLONCOLON
%token TEMPLATE
%token ERROR
%token NEW DELETE OPERATOR
%token STATIC_CAST REINTERPRET_CAST DYNAMIC_CAST

/* Special type cases, put in to allow the parser to distinguish different
   legal basetypes.  */
%token SIGNED_KEYWORD LONG SHORT INT_KEYWORD CONST_KEYWORD VOLATILE_KEYWORD DOUBLE_KEYWORD BOOL
%token ELLIPSIS RESTRICT VOID FLOAT_KEYWORD CHAR WCHAR_T

%token <opname> ASSIGN_MODIFY

/* C++ */
%token TRUEKEYWORD
%token FALSEKEYWORD

/* Non-C++ things we get from the demangler.  */
%token <lval> DEMANGLER_SPECIAL
%token CONSTRUCTION_VTABLE CONSTRUCTION_IN
%token <typed_val_int> GLOBAL

%{
enum {
  GLOBAL_CONSTRUCTORS = DEMANGLE_COMPONENT_LITERAL + 20,
  GLOBAL_DESTRUCTORS = DEMANGLE_COMPONENT_LITERAL + 21
};
%}

/* Precedence declarations.  */

/* Give NAME lower precedence than COLONCOLON, so that nested_name will
   associate greedily.  */
%nonassoc NAME

/* Give NEW and DELETE lower precedence than ']', because we can not
   have an array of type operator new.  This causes NEW '[' to be
   parsed as operator new[].  */
%nonassoc NEW DELETE

/* Give VOID higher precedence than NAME.  Then we can use %prec NAME
   to prefer (VOID) to (function_args).  */
%nonassoc VOID

/* Give VOID lower precedence than ')' for similar reasons.  */
%nonassoc ')'

%left ','
%right '=' ASSIGN_MODIFY
%right '?'
%left OROR
%left ANDAND
%left '|'
%left '^'
%left '&'
%left EQUAL NOTEQUAL
%left '<' '>' LEQ GEQ
%left LSH RSH
%left '@'
%left '+' '-'
%left '*' '/' '%'
%right UNARY INCREMENT DECREMENT

/* We don't need a precedence for '(' in this reduced grammar, and it
   can mask some unpleasant bugs, so disable it for now.  */

%right ARROW '.' '[' /* '(' */
%left COLONCOLON


%%

result		:	start
			{ global_result = $1; }
		;

start		:	type

		|	demangler_special

		|	function

		;

start_opt	:	/* */
			{ $$ = NULL; }
		|	COLONCOLON start
			{ $$ = $2; }
		;

function
		/* Function with a return type.  declarator_1 is used to prevent
		   ambiguity with the next rule.  */
		:	typespec_2 declarator_1
			{ $$ = $2.comp;
			  *$2.last = $1;
			}

		/* Function without a return type.  We need to use typespec_2
		   to prevent conflicts from qualifiers_opt - harmless.  The
		   start_opt is used to handle "function-local" variables and
		   types.  */
		|	typespec_2 function_arglist start_opt
			{ $$ = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, $1, $2.comp);
			  if ($3) $$ = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, $$, $3); }
		|	colon_ext_only function_arglist start_opt
			{ $$ = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, $1, $2.comp);
			  if ($3) $$ = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, $$, $3); }

		|	conversion_op_name start_opt
			{ $$ = $1.comp;
			  if ($2) $$ = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, $$, $2); }
		|	conversion_op_name abstract_declarator_fn
			{ if ($2.last)
			    {
			       /* First complete the abstract_declarator's type using
				  the typespec from the conversion_op_name.  */
			      *$2.last = *$1.last;
			      /* Then complete the conversion_op_name with the type.  */
			      *$1.last = $2.comp;
			    }
			  /* If we have an arglist, build a function type.  */
			  if ($2.fn.comp)
			    $$ = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, $1.comp, $2.fn.comp);
			  else
			    $$ = $1.comp;
			  if ($2.start) $$ = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, $$, $2.start);
			}
		;

demangler_special
		:	DEMANGLER_SPECIAL start
			{ $$ = make_empty ($1);
			  d_left ($$) = $2;
			  d_right ($$) = NULL; }
		|	CONSTRUCTION_VTABLE start CONSTRUCTION_IN start
			{ $$ = fill_comp (DEMANGLE_COMPONENT_CONSTRUCTION_VTABLE, $2, $4); }
		|	GLOBAL
			{ $$ = make_empty ($1.val);
			  d_left ($$) = $1.type;
			  d_right ($$) = NULL; }
		;

operator	:	OPERATOR NEW
			{ $$ = make_operator ("new", 1); }
		|	OPERATOR DELETE
			{ $$ = make_operator ("delete", 1); }
		|	OPERATOR NEW '[' ']'
			{ $$ = make_operator ("new[]", 1); }
		|	OPERATOR DELETE '[' ']'
			{ $$ = make_operator ("delete[]", 1); }
		|	OPERATOR '+'
			{ $$ = make_operator ("+", 2); }
		|	OPERATOR '-'
			{ $$ = make_operator ("-", 2); }
		|	OPERATOR '*'
			{ $$ = make_operator ("*", 2); }
		|	OPERATOR '/'
			{ $$ = make_operator ("/", 2); }
		|	OPERATOR '%'
			{ $$ = make_operator ("%", 2); }
		|	OPERATOR '^'
			{ $$ = make_operator ("^", 2); }
		|	OPERATOR '&'
			{ $$ = make_operator ("&", 2); }
		|	OPERATOR '|'
			{ $$ = make_operator ("|", 2); }
		|	OPERATOR '~'
			{ $$ = make_operator ("~", 1); }
		|	OPERATOR '!'
			{ $$ = make_operator ("!", 1); }
		|	OPERATOR '='
			{ $$ = make_operator ("=", 2); }
		|	OPERATOR '<'
			{ $$ = make_operator ("<", 2); }
		|	OPERATOR '>'
			{ $$ = make_operator (">", 2); }
		|	OPERATOR ASSIGN_MODIFY
			{ $$ = make_operator ($2, 2); }
		|	OPERATOR LSH
			{ $$ = make_operator ("<<", 2); }
		|	OPERATOR RSH
			{ $$ = make_operator (">>", 2); }
		|	OPERATOR EQUAL
			{ $$ = make_operator ("==", 2); }
		|	OPERATOR NOTEQUAL
			{ $$ = make_operator ("!=", 2); }
		|	OPERATOR LEQ
			{ $$ = make_operator ("<=", 2); }
		|	OPERATOR GEQ
			{ $$ = make_operator (">=", 2); }
		|	OPERATOR ANDAND
			{ $$ = make_operator ("&&", 2); }
		|	OPERATOR OROR
			{ $$ = make_operator ("||", 2); }
		|	OPERATOR INCREMENT
			{ $$ = make_operator ("++", 1); }
		|	OPERATOR DECREMENT
			{ $$ = make_operator ("--", 1); }
		|	OPERATOR ','
			{ $$ = make_operator (",", 2); }
		|	OPERATOR ARROW '*'
			{ $$ = make_operator ("->*", 2); }
		|	OPERATOR ARROW
			{ $$ = make_operator ("->", 2); }
		|	OPERATOR '(' ')'
			{ $$ = make_operator ("()", 0); }
		|	OPERATOR '[' ']'
			{ $$ = make_operator ("[]", 2); }
		;

		/* Conversion operators.  We don't try to handle some of
		   the wackier demangler output for function pointers,
		   since it's not clear that it's parseable.  */
conversion_op
		:	OPERATOR typespec_2
			{ $$ = fill_comp (DEMANGLE_COMPONENT_CAST, $2, NULL); }
		;

conversion_op_name
		:	nested_name conversion_op
			{ $$.comp = $1.comp;
			  d_right ($1.last) = $2;
			  $$.last = &d_left ($2);
			}
		|	conversion_op
			{ $$.comp = $1;
			  $$.last = &d_left ($1);
			}
		|	COLONCOLON nested_name conversion_op
			{ $$.comp = $2.comp;
			  d_right ($2.last) = $3;
			  $$.last = &d_left ($3);
			}
		|	COLONCOLON conversion_op
			{ $$.comp = $2;
			  $$.last = &d_left ($2);
			}
		;

/* DEMANGLE_COMPONENT_NAME */
/* This accepts certain invalid placements of '~'.  */
unqualified_name:	operator
		|	operator '<' template_params '>'
			{ $$ = fill_comp (DEMANGLE_COMPONENT_TEMPLATE, $1, $3.comp); }
		|	'~' NAME
			{ $$ = make_dtor (gnu_v3_complete_object_dtor, $2); }
		;

/* This rule is used in name and nested_name, and expanded inline there
   for efficiency.  */
/*
scope_id	:	NAME
		|	template
		;
*/

colon_name	:	name
		|	COLONCOLON name
			{ $$ = $2; }
		;

/* DEMANGLE_COMPONENT_QUAL_NAME */
/* DEMANGLE_COMPONENT_CTOR / DEMANGLE_COMPONENT_DTOR ? */
name		:	nested_name NAME %prec NAME
			{ $$ = $1.comp; d_right ($1.last) = $2; }
		|	NAME %prec NAME
		|	nested_name template %prec NAME
			{ $$ = $1.comp; d_right ($1.last) = $2; }
		|	template %prec NAME
		;

colon_ext_name	:	colon_name
		|	colon_ext_only
		;

colon_ext_only	:	ext_only_name
		|	COLONCOLON ext_only_name
			{ $$ = $2; }
		;

ext_only_name	:	nested_name unqualified_name
			{ $$ = $1.comp; d_right ($1.last) = $2; }
		|	unqualified_name
		;

nested_name	:	NAME COLONCOLON
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  d_left ($$.comp) = $1;
			  d_right ($$.comp) = NULL;
			  $$.last = $$.comp;
			}
		|	nested_name NAME COLONCOLON
			{ $$.comp = $1.comp;
			  d_right ($1.last) = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  $$.last = d_right ($1.last);
			  d_left ($$.last) = $2;
			  d_right ($$.last) = NULL;
			}
		|	template COLONCOLON
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  d_left ($$.comp) = $1;
			  d_right ($$.comp) = NULL;
			  $$.last = $$.comp;
			}
		|	nested_name template COLONCOLON
			{ $$.comp = $1.comp;
			  d_right ($1.last) = make_empty (DEMANGLE_COMPONENT_QUAL_NAME);
			  $$.last = d_right ($1.last);
			  d_left ($$.last) = $2;
			  d_right ($$.last) = NULL;
			}
		;

/* DEMANGLE_COMPONENT_TEMPLATE */
/* DEMANGLE_COMPONENT_TEMPLATE_ARGLIST */
template	:	NAME '<' template_params '>'
			{ $$ = fill_comp (DEMANGLE_COMPONENT_TEMPLATE, $1, $3.comp); }
		;

template_params	:	template_arg
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_TEMPLATE_ARGLIST, $1, NULL);
			$$.last = &d_right ($$.comp); }
		|	template_params ',' template_arg
			{ $$.comp = $1.comp;
			  *$1.last = fill_comp (DEMANGLE_COMPONENT_TEMPLATE_ARGLIST, $3, NULL);
			  $$.last = &d_right (*$1.last);
			}
		;

/* "type" is inlined into template_arg and function_args.  */

/* Also an integral constant-expression of integral type, and a
   pointer to member (?) */
template_arg	:	typespec_2
		|	typespec_2 abstract_declarator
			{ $$ = $2.comp;
			  *$2.last = $1;
			}
		|	'&' start
			{ $$ = fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator ("&", 1), $2); }
		|	'&' '(' start ')'
			{ $$ = fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator ("&", 1), $3); }
		|	exp
		;

function_args	:	typespec_2
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_ARGLIST, $1, NULL);
			  $$.last = &d_right ($$.comp);
			}
		|	typespec_2 abstract_declarator
			{ *$2.last = $1;
			  $$.comp = fill_comp (DEMANGLE_COMPONENT_ARGLIST, $2.comp, NULL);
			  $$.last = &d_right ($$.comp);
			}
		|	function_args ',' typespec_2
			{ *$1.last = fill_comp (DEMANGLE_COMPONENT_ARGLIST, $3, NULL);
			  $$.comp = $1.comp;
			  $$.last = &d_right (*$1.last);
			}
		|	function_args ',' typespec_2 abstract_declarator
			{ *$4.last = $3;
			  *$1.last = fill_comp (DEMANGLE_COMPONENT_ARGLIST, $4.comp, NULL);
			  $$.comp = $1.comp;
			  $$.last = &d_right (*$1.last);
			}
		|	function_args ',' ELLIPSIS
			{ *$1.last
			    = fill_comp (DEMANGLE_COMPONENT_ARGLIST,
					   make_builtin_type ("..."),
					   NULL);
			  $$.comp = $1.comp;
			  $$.last = &d_right (*$1.last);
			}
		;

function_arglist:	'(' function_args ')' qualifiers_opt %prec NAME
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_FUNCTION_TYPE, NULL, $2.comp);
			  $$.last = &d_left ($$.comp);
			  $$.comp = d_qualify ($$.comp, $4, 1); }
		|	'(' VOID ')' qualifiers_opt
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_FUNCTION_TYPE, NULL, NULL);
			  $$.last = &d_left ($$.comp);
			  $$.comp = d_qualify ($$.comp, $4, 1); }
		|	'(' ')' qualifiers_opt
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_FUNCTION_TYPE, NULL, NULL);
			  $$.last = &d_left ($$.comp);
			  $$.comp = d_qualify ($$.comp, $3, 1); }
		;

/* Should do something about DEMANGLE_COMPONENT_VENDOR_TYPE_QUAL */
qualifiers_opt	:	/* epsilon */
			{ $$ = 0; }
		|	qualifiers
		;

qualifier	:	RESTRICT
			{ $$ = QUAL_RESTRICT; }
		|	VOLATILE_KEYWORD
			{ $$ = QUAL_VOLATILE; }
		|	CONST_KEYWORD
			{ $$ = QUAL_CONST; }
		;

qualifiers	:	qualifier
		|	qualifier qualifiers
			{ $$ = $1 | $2; }
		;

/* This accepts all sorts of invalid constructions and produces
   invalid output for them - an error would be better.  */

int_part	:	INT_KEYWORD
			{ $$ = 0; }
		|	SIGNED_KEYWORD
			{ $$ = INT_SIGNED; }
		|	UNSIGNED
			{ $$ = INT_UNSIGNED; }
		|	CHAR
			{ $$ = INT_CHAR; }
		|	LONG
			{ $$ = INT_LONG; }
		|	SHORT
			{ $$ = INT_SHORT; }
		;

int_seq		:	int_part
		|	int_seq int_part
			{ $$ = $1 | $2; if ($1 & $2 & INT_LONG) $$ = $1 | INT_LLONG; }
		;

builtin_type	:	int_seq
			{ $$ = d_int_type ($1); }
		|	FLOAT_KEYWORD
			{ $$ = make_builtin_type ("float"); }
		|	DOUBLE_KEYWORD
			{ $$ = make_builtin_type ("double"); }
		|	LONG DOUBLE_KEYWORD
			{ $$ = make_builtin_type ("long double"); }
		|	BOOL
			{ $$ = make_builtin_type ("bool"); }
		|	WCHAR_T
			{ $$ = make_builtin_type ("wchar_t"); }
		|	VOID
			{ $$ = make_builtin_type ("void"); }
		;

ptr_operator	:	'*' qualifiers_opt
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_POINTER);
			  $$.comp->u.s_binary.left = $$.comp->u.s_binary.right = NULL;
			  $$.last = &d_left ($$.comp);
			  $$.comp = d_qualify ($$.comp, $2, 0); }
		/* g++ seems to allow qualifiers after the reference?  */
		|	'&'
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_REFERENCE);
			  $$.comp->u.s_binary.left = $$.comp->u.s_binary.right = NULL;
			  $$.last = &d_left ($$.comp); }
		|	nested_name '*' qualifiers_opt
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_PTRMEM_TYPE);
			  $$.comp->u.s_binary.left = $1.comp;
			  /* Convert the innermost DEMANGLE_COMPONENT_QUAL_NAME to a DEMANGLE_COMPONENT_NAME.  */
			  *$1.last = *d_left ($1.last);
			  $$.comp->u.s_binary.right = NULL;
			  $$.last = &d_right ($$.comp);
			  $$.comp = d_qualify ($$.comp, $3, 0); }
		|	COLONCOLON nested_name '*' qualifiers_opt
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_PTRMEM_TYPE);
			  $$.comp->u.s_binary.left = $2.comp;
			  /* Convert the innermost DEMANGLE_COMPONENT_QUAL_NAME to a DEMANGLE_COMPONENT_NAME.  */
			  *$2.last = *d_left ($2.last);
			  $$.comp->u.s_binary.right = NULL;
			  $$.last = &d_right ($$.comp);
			  $$.comp = d_qualify ($$.comp, $4, 0); }
		;

array_indicator	:	'[' ']'
			{ $$ = make_empty (DEMANGLE_COMPONENT_ARRAY_TYPE);
			  d_left ($$) = NULL;
			}
		|	'[' INT ']'
			{ $$ = make_empty (DEMANGLE_COMPONENT_ARRAY_TYPE);
			  d_left ($$) = $2;
			}
		;

/* Details of this approach inspired by the G++ < 3.4 parser.  */

/* This rule is only used in typespec_2, and expanded inline there for
   efficiency.  */
/*
typespec	:	builtin_type
		|	colon_name
		;
*/

typespec_2	:	builtin_type qualifiers
			{ $$ = d_qualify ($1, $2, 0); }
		|	builtin_type
		|	qualifiers builtin_type qualifiers
			{ $$ = d_qualify ($2, $1 | $3, 0); }
		|	qualifiers builtin_type
			{ $$ = d_qualify ($2, $1, 0); }

		|	name qualifiers
			{ $$ = d_qualify ($1, $2, 0); }
		|	name
		|	qualifiers name qualifiers
			{ $$ = d_qualify ($2, $1 | $3, 0); }
		|	qualifiers name
			{ $$ = d_qualify ($2, $1, 0); }

		|	COLONCOLON name qualifiers
			{ $$ = d_qualify ($2, $3, 0); }
		|	COLONCOLON name
			{ $$ = $2; }
		|	qualifiers COLONCOLON name qualifiers
			{ $$ = d_qualify ($3, $1 | $4, 0); }
		|	qualifiers COLONCOLON name
			{ $$ = d_qualify ($3, $1, 0); }
		;

abstract_declarator
		:	ptr_operator
			{ $$.comp = $1.comp; $$.last = $1.last;
			  $$.fn.comp = NULL; $$.fn.last = NULL; }
		|	ptr_operator abstract_declarator
			{ $$ = $2; $$.fn.comp = NULL; $$.fn.last = NULL;
			  if ($2.fn.comp) { $$.last = $2.fn.last; *$2.last = $2.fn.comp; }
			  *$$.last = $1.comp;
			  $$.last = $1.last; }
		|	direct_abstract_declarator
			{ $$.fn.comp = NULL; $$.fn.last = NULL;
			  if ($1.fn.comp) { $$.last = $1.fn.last; *$1.last = $1.fn.comp; }
			}
		;

direct_abstract_declarator
		:	'(' abstract_declarator ')'
			{ $$ = $2; $$.fn.comp = NULL; $$.fn.last = NULL; $$.fold_flag = 1;
			  if ($2.fn.comp) { $$.last = $2.fn.last; *$2.last = $2.fn.comp; }
			}
		|	direct_abstract_declarator function_arglist
			{ $$.fold_flag = 0;
			  if ($1.fn.comp) { $$.last = $1.fn.last; *$1.last = $1.fn.comp; }
			  if ($1.fold_flag)
			    {
			      *$$.last = $2.comp;
			      $$.last = $2.last;
			    }
			  else
			    $$.fn = $2;
			}
		|	direct_abstract_declarator array_indicator
			{ $$.fn.comp = NULL; $$.fn.last = NULL; $$.fold_flag = 0;
			  if ($1.fn.comp) { $$.last = $1.fn.last; *$1.last = $1.fn.comp; }
			  *$1.last = $2;
			  $$.last = &d_right ($2);
			}
		|	array_indicator
			{ $$.fn.comp = NULL; $$.fn.last = NULL; $$.fold_flag = 0;
			  $$.comp = $1;
			  $$.last = &d_right ($1);
			}
		/* G++ has the following except for () and (type).  Then
		   (type) is handled in regcast_or_absdcl and () is handled
		   in fcast_or_absdcl.

		   However, this is only useful for function types, and
		   generates reduce/reduce conflicts with direct_declarator.
		   We're interested in pointer-to-function types, and in
		   functions, but not in function types - so leave this
		   out.  */
		/* |	function_arglist */
		;

abstract_declarator_fn
		:	ptr_operator
			{ $$.comp = $1.comp; $$.last = $1.last;
			  $$.fn.comp = NULL; $$.fn.last = NULL; $$.start = NULL; }
		|	ptr_operator abstract_declarator_fn
			{ $$ = $2;
			  if ($2.last)
			    *$$.last = $1.comp;
			  else
			    $$.comp = $1.comp;
			  $$.last = $1.last;
			}
		|	direct_abstract_declarator
			{ $$.comp = $1.comp; $$.last = $1.last; $$.fn = $1.fn; $$.start = NULL; }
		|	direct_abstract_declarator function_arglist COLONCOLON start
			{ $$.start = $4;
			  if ($1.fn.comp) { $$.last = $1.fn.last; *$1.last = $1.fn.comp; }
			  if ($1.fold_flag)
			    {
			      *$$.last = $2.comp;
			      $$.last = $2.last;
			    }
			  else
			    $$.fn = $2;
			}
		|	function_arglist start_opt
			{ $$.fn = $1;
			  $$.start = $2;
			  $$.comp = NULL; $$.last = NULL;
			}
		;

type		:	typespec_2
		|	typespec_2 abstract_declarator
			{ $$ = $2.comp;
			  *$2.last = $1;
			}
		;

declarator	:	ptr_operator declarator
			{ $$.comp = $2.comp;
			  $$.last = $1.last;
			  *$2.last = $1.comp; }
		|	direct_declarator
		;

direct_declarator
		:	'(' declarator ')'
			{ $$ = $2; }
		|	direct_declarator function_arglist
			{ $$.comp = $1.comp;
			  *$1.last = $2.comp;
			  $$.last = $2.last;
			}
		|	direct_declarator array_indicator
			{ $$.comp = $1.comp;
			  *$1.last = $2;
			  $$.last = &d_right ($2);
			}
		|	colon_ext_name
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_TYPED_NAME);
			  d_left ($$.comp) = $1;
			  $$.last = &d_right ($$.comp);
			}
		;

/* These are similar to declarator and direct_declarator except that they
   do not permit ( colon_ext_name ), which is ambiguous with a function
   argument list.  They also don't permit a few other forms with redundant
   parentheses around the colon_ext_name; any colon_ext_name in parentheses
   must be followed by an argument list or an array indicator, or preceded
   by a pointer.  */
declarator_1	:	ptr_operator declarator_1
			{ $$.comp = $2.comp;
			  $$.last = $1.last;
			  *$2.last = $1.comp; }
		|	colon_ext_name
			{ $$.comp = make_empty (DEMANGLE_COMPONENT_TYPED_NAME);
			  d_left ($$.comp) = $1;
			  $$.last = &d_right ($$.comp);
			}
		|	direct_declarator_1

			/* Function local variable or type.  The typespec to
			   our left is the type of the containing function. 
			   This should be OK, because function local types
			   can not be templates, so the return types of their
			   members will not be mangled.  If they are hopefully
			   they'll end up to the right of the ::.  */
		|	colon_ext_name function_arglist COLONCOLON start
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, $1, $2.comp);
			  $$.last = $2.last;
			  $$.comp = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, $$.comp, $4);
			}
		|	direct_declarator_1 function_arglist COLONCOLON start
			{ $$.comp = $1.comp;
			  *$1.last = $2.comp;
			  $$.last = $2.last;
			  $$.comp = fill_comp (DEMANGLE_COMPONENT_LOCAL_NAME, $$.comp, $4);
			}
		;

direct_declarator_1
		:	'(' ptr_operator declarator ')'
			{ $$.comp = $3.comp;
			  $$.last = $2.last;
			  *$3.last = $2.comp; }
		|	direct_declarator_1 function_arglist
			{ $$.comp = $1.comp;
			  *$1.last = $2.comp;
			  $$.last = $2.last;
			}
		|	direct_declarator_1 array_indicator
			{ $$.comp = $1.comp;
			  *$1.last = $2;
			  $$.last = &d_right ($2);
			}
		|	colon_ext_name function_arglist
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, $1, $2.comp);
			  $$.last = $2.last;
			}
		|	colon_ext_name array_indicator
			{ $$.comp = fill_comp (DEMANGLE_COMPONENT_TYPED_NAME, $1, $2);
			  $$.last = &d_right ($2);
			}
		;

exp	:	'(' exp1 ')'
		{ $$ = $2; }
	;

/* Silly trick.  Only allow '>' when parenthesized, in order to
   handle conflict with templates.  */
exp1	:	exp
	;

exp1	:	exp '>' exp
		{ $$ = d_binary (">", $1, $3); }
	;

/* References.  Not allowed everywhere in template parameters, only
   at the top level, but treat them as expressions in case they are wrapped
   in parentheses.  */
exp1	:	'&' start
		{ $$ = fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator ("&", 1), $2); }
	;

/* Expressions, not including the comma operator.  */
exp	:	'-' exp    %prec UNARY
		{ $$ = d_unary ("-", $2); }
	;

exp	:	'!' exp    %prec UNARY
		{ $$ = d_unary ("!", $2); }
	;

exp	:	'~' exp    %prec UNARY
		{ $$ = d_unary ("~", $2); }
	;

/* Casts.  First your normal C-style cast.  If exp is a LITERAL, just change
   its type.  */

exp	:	'(' type ')' exp  %prec UNARY
		{ if ($4->type == DEMANGLE_COMPONENT_LITERAL
		      || $4->type == DEMANGLE_COMPONENT_LITERAL_NEG)
		    {
		      $$ = $4;
		      d_left ($4) = $2;
		    }
		  else
		    $$ = fill_comp (DEMANGLE_COMPONENT_UNARY,
				      fill_comp (DEMANGLE_COMPONENT_CAST, $2, NULL),
				      $4);
		}
	;

/* Mangling does not differentiate between these, so we don't need to
   either.  */
exp	:	STATIC_CAST '<' type '>' '(' exp1 ')' %prec UNARY
		{ $$ = fill_comp (DEMANGLE_COMPONENT_UNARY,
				    fill_comp (DEMANGLE_COMPONENT_CAST, $3, NULL),
				    $6);
		}
	;

exp	:	DYNAMIC_CAST '<' type '>' '(' exp1 ')' %prec UNARY
		{ $$ = fill_comp (DEMANGLE_COMPONENT_UNARY,
				    fill_comp (DEMANGLE_COMPONENT_CAST, $3, NULL),
				    $6);
		}
	;

exp	:	REINTERPRET_CAST '<' type '>' '(' exp1 ')' %prec UNARY
		{ $$ = fill_comp (DEMANGLE_COMPONENT_UNARY,
				    fill_comp (DEMANGLE_COMPONENT_CAST, $3, NULL),
				    $6);
		}
	;

/* Another form of C++-style cast.  "type ( exp1 )" is not allowed (it's too
   ambiguous), but "name ( exp1 )" is.  Because we don't need to support
   function types, we can handle this unambiguously (the use of typespec_2
   prevents a silly, harmless conflict with qualifiers_opt).  This does not
   appear in demangler output so it's not a great loss if we need to
   disable it.  */
exp	:	typespec_2 '(' exp1 ')' %prec UNARY
		{ $$ = fill_comp (DEMANGLE_COMPONENT_UNARY,
				    fill_comp (DEMANGLE_COMPONENT_CAST, $1, NULL),
				    $3);
		}
	;

/* TO INVESTIGATE: ._0 style anonymous names; anonymous namespaces */

/* Binary operators in order of decreasing precedence.  */

exp	:	exp '*' exp
		{ $$ = d_binary ("*", $1, $3); }
	;

exp	:	exp '/' exp
		{ $$ = d_binary ("/", $1, $3); }
	;

exp	:	exp '%' exp
		{ $$ = d_binary ("%", $1, $3); }
	;

exp	:	exp '+' exp
		{ $$ = d_binary ("+", $1, $3); }
	;

exp	:	exp '-' exp
		{ $$ = d_binary ("-", $1, $3); }
	;

exp	:	exp LSH exp
		{ $$ = d_binary ("<<", $1, $3); }
	;

exp	:	exp RSH exp
		{ $$ = d_binary (">>", $1, $3); }
	;

exp	:	exp EQUAL exp
		{ $$ = d_binary ("==", $1, $3); }
	;

exp	:	exp NOTEQUAL exp
		{ $$ = d_binary ("!=", $1, $3); }
	;

exp	:	exp LEQ exp
		{ $$ = d_binary ("<=", $1, $3); }
	;

exp	:	exp GEQ exp
		{ $$ = d_binary (">=", $1, $3); }
	;

exp	:	exp '<' exp
		{ $$ = d_binary ("<", $1, $3); }
	;

exp	:	exp '&' exp
		{ $$ = d_binary ("&", $1, $3); }
	;

exp	:	exp '^' exp
		{ $$ = d_binary ("^", $1, $3); }
	;

exp	:	exp '|' exp
		{ $$ = d_binary ("|", $1, $3); }
	;

exp	:	exp ANDAND exp
		{ $$ = d_binary ("&&", $1, $3); }
	;

exp	:	exp OROR exp
		{ $$ = d_binary ("||", $1, $3); }
	;

/* Not 100% sure these are necessary, but they're harmless.  */
exp	:	exp ARROW NAME
		{ $$ = d_binary ("->", $1, $3); }
	;

exp	:	exp '.' NAME
		{ $$ = d_binary (".", $1, $3); }
	;

exp	:	exp '?' exp ':' exp	%prec '?'
		{ $$ = fill_comp (DEMANGLE_COMPONENT_TRINARY, make_operator ("?", 3),
				    fill_comp (DEMANGLE_COMPONENT_TRINARY_ARG1, $1,
						 fill_comp (DEMANGLE_COMPONENT_TRINARY_ARG2, $3, $5)));
		}
	;
			  
exp	:	INT
	;

/* Not generally allowed.  */
exp	:	FLOAT
	;

exp	:	SIZEOF '(' type ')'	%prec UNARY
		{ $$ = d_unary ("sizeof", $3); }
	;

/* C++.  */
exp     :       TRUEKEYWORD    
		{ struct demangle_component *i;
		  i = make_name ("1", 1);
		  $$ = fill_comp (DEMANGLE_COMPONENT_LITERAL,
				    make_builtin_type ("bool"),
				    i);
		}
	;

exp     :       FALSEKEYWORD   
		{ struct demangle_component *i;
		  i = make_name ("0", 1);
		  $$ = fill_comp (DEMANGLE_COMPONENT_LITERAL,
				    make_builtin_type ("bool"),
				    i);
		}
	;

/* end of C++.  */

%%

/* Apply QUALIFIERS to LHS and return a qualified component.  IS_METHOD
   is set if LHS is a method, in which case the qualifiers are logically
   applied to "this".  We apply qualifiers in a consistent order; LHS
   may already be qualified; duplicate qualifiers are not created.  */

struct demangle_component *
d_qualify (struct demangle_component *lhs, int qualifiers, int is_method)
{
  struct demangle_component **inner_p;
  enum demangle_component_type type;

  /* For now the order is CONST (innermost), VOLATILE, RESTRICT.  */

#define HANDLE_QUAL(TYPE, MTYPE, QUAL)				\
  if ((qualifiers & QUAL) && (type != TYPE) && (type != MTYPE))	\
    {								\
      *inner_p = fill_comp (is_method ? MTYPE : TYPE,	\
			      *inner_p, NULL);			\
      inner_p = &d_left (*inner_p);				\
      type = (*inner_p)->type;					\
    }								\
  else if (type == TYPE || type == MTYPE)			\
    {								\
      inner_p = &d_left (*inner_p);				\
      type = (*inner_p)->type;					\
    }

  inner_p = &lhs;

  type = (*inner_p)->type;

  HANDLE_QUAL (DEMANGLE_COMPONENT_RESTRICT, DEMANGLE_COMPONENT_RESTRICT_THIS, QUAL_RESTRICT);
  HANDLE_QUAL (DEMANGLE_COMPONENT_VOLATILE, DEMANGLE_COMPONENT_VOLATILE_THIS, QUAL_VOLATILE);
  HANDLE_QUAL (DEMANGLE_COMPONENT_CONST, DEMANGLE_COMPONENT_CONST_THIS, QUAL_CONST);

  return lhs;
}

/* Return a builtin type corresponding to FLAGS.  */

static struct demangle_component *
d_int_type (int flags)
{
  const char *name;

  switch (flags)
    {
    case INT_SIGNED | INT_CHAR:
      name = "signed char";
      break;
    case INT_CHAR:
      name = "char";
      break;
    case INT_UNSIGNED | INT_CHAR:
      name = "unsigned char";
      break;
    case 0:
    case INT_SIGNED:
      name = "int";
      break;
    case INT_UNSIGNED:
      name = "unsigned int";
      break;
    case INT_LONG:
    case INT_SIGNED | INT_LONG:
      name = "long";
      break;
    case INT_UNSIGNED | INT_LONG:
      name = "unsigned long";
      break;
    case INT_SHORT:
    case INT_SIGNED | INT_SHORT:
      name = "short";
      break;
    case INT_UNSIGNED | INT_SHORT:
      name = "unsigned short";
      break;
    case INT_LLONG | INT_LONG:
    case INT_SIGNED | INT_LLONG | INT_LONG:
      name = "long long";
      break;
    case INT_UNSIGNED | INT_LLONG | INT_LONG:
      name = "unsigned long long";
      break;
    default:
      return NULL;
    }

  return make_builtin_type (name);
}

/* Wrapper to create a unary operation.  */

static struct demangle_component *
d_unary (const char *name, struct demangle_component *lhs)
{
  return fill_comp (DEMANGLE_COMPONENT_UNARY, make_operator (name, 1), lhs);
}

/* Wrapper to create a binary operation.  */

static struct demangle_component *
d_binary (const char *name, struct demangle_component *lhs, struct demangle_component *rhs)
{
  return fill_comp (DEMANGLE_COMPONENT_BINARY, make_operator (name, 2),
		      fill_comp (DEMANGLE_COMPONENT_BINARY_ARGS, lhs, rhs));
}

/* Find the end of a symbol name starting at LEXPTR.  */

static const char *
symbol_end (const char *lexptr)
{
  const char *p = lexptr;

  while (*p && (ISALNUM (*p) || *p == '_' || *p == '$' || *p == '.'))
    p++;

  return p;
}

/* Take care of parsing a number (anything that starts with a digit).
   The number starts at P and contains LEN characters.  Store the result in
   YYLVAL.  */

static int
parse_number (const char *p, int len, int parsed_float)
{
  int unsigned_p = 0;

  /* Number of "L" suffixes encountered.  */
  int long_p = 0;

  struct demangle_component *signed_type;
  struct demangle_component *unsigned_type;
  struct demangle_component *type, *name;
  enum demangle_component_type literal_type;

  if (p[0] == '-')
    {
      literal_type = DEMANGLE_COMPONENT_LITERAL_NEG;
      p++;
      len--;
    }
  else
    literal_type = DEMANGLE_COMPONENT_LITERAL;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      char c;

      /* The GDB lexer checks the result of scanf at this point.  Not doing
         this leaves our error checking slightly weaker but only for invalid
         data.  */

      /* See if it has `f' or `l' suffix (float or long double).  */

      c = TOLOWER (p[len - 1]);

      if (c == 'f')
      	{
      	  len--;
      	  type = make_builtin_type ("float");
      	}
      else if (c == 'l')
	{
	  len--;
	  type = make_builtin_type ("long double");
	}
      else if (ISDIGIT (c) || c == '.')
	type = make_builtin_type ("double");
      else
	return ERROR;

      name = make_name (p, len);
      yylval.comp = fill_comp (literal_type, type, name);

      return FLOAT;
    }

  /* This treats 0x1 and 1 as different literals.  We also do not
     automatically generate unsigned types.  */

  long_p = 0;
  unsigned_p = 0;
  while (len > 0)
    {
      if (p[len - 1] == 'l' || p[len - 1] == 'L')
	{
	  len--;
	  long_p++;
	  continue;
	}
      if (p[len - 1] == 'u' || p[len - 1] == 'U')
	{
	  len--;
	  unsigned_p++;
	  continue;
	}
      break;
    }

  if (long_p == 0)
    {
      unsigned_type = make_builtin_type ("unsigned int");
      signed_type = make_builtin_type ("int");
    }
  else if (long_p == 1)
    {
      unsigned_type = make_builtin_type ("unsigned long");
      signed_type = make_builtin_type ("long");
    }
  else
    {
      unsigned_type = make_builtin_type ("unsigned long long");
      signed_type = make_builtin_type ("long long");
    }

   if (unsigned_p)
     type = unsigned_type;
   else
     type = signed_type;

   name = make_name (p, len);
   yylval.comp = fill_comp (literal_type, type, name);

   return INT;
}

static char backslashable[] = "abefnrtv";
static char represented[] = "\a\b\e\f\n\r\t\v";

/* Translate the backslash the way we would in the host character set.  */
static int
c_parse_backslash (int host_char, int *target_char)
{
  const char *ix;
  ix = strchr (backslashable, host_char);
  if (! ix)
    return 0;
  else
    *target_char = represented[ix - backslashable];
  return 1;
}

/* Parse a C escape sequence.  STRING_PTR points to a variable
   containing a pointer to the string to parse.  That pointer
   should point to the character after the \.  That pointer
   is updated past the characters we use.  The value of the
   escape sequence is returned.

   A negative value means the sequence \ newline was seen,
   which is supposed to be equivalent to nothing at all.

   If \ is followed by a null character, we return a negative
   value and leave the string pointer pointing at the null character.

   If \ is followed by 000, we return 0 and leave the string pointer
   after the zeros.  A value of 0 does not mean end of string.  */

static int
parse_escape (const char **string_ptr)
{
  int target_char;
  int c = *(*string_ptr)++;
  if (c_parse_backslash (c, &target_char))
    return target_char;
  else
    switch (c)
      {
      case '\n':
	return -2;
      case 0:
	(*string_ptr)--;
	return 0;
      case '^':
	{
	  c = *(*string_ptr)++;

	  if (c == '?')
	    return 0177;
	  else if (c == '\\')
	    target_char = parse_escape (string_ptr);
	  else
	    target_char = c;

	  /* Now target_char is something like `c', and we want to find
	     its control-character equivalent.  */
	  target_char = target_char & 037;

	  return target_char;
	}

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
	{
	  int i = c - '0';
	  int count = 0;
	  while (++count < 3)
	    {
	      c = (**string_ptr);
	      if (c >= '0' && c <= '7')
		{
		  (*string_ptr)++;
		  i *= 8;
		  i += c - '0';
		}
	      else
		{
		  break;
		}
	    }
	  return i;
	}
      default:
	return c;
      }
}

#define HANDLE_SPECIAL(string, comp)				\
  if (strncmp (tokstart, string, sizeof (string) - 1) == 0)	\
    {								\
      lexptr = tokstart + sizeof (string) - 1;			\
      yylval.lval = comp;					\
      return DEMANGLER_SPECIAL;					\
    }

#define HANDLE_TOKEN2(string, token)			\
  if (lexptr[1] == string[1])				\
    {							\
      lexptr += 2;					\
      yylval.opname = string;				\
      return token;					\
    }      

#define HANDLE_TOKEN3(string, token)			\
  if (lexptr[1] == string[1] && lexptr[2] == string[2])	\
    {							\
      lexptr += 3;					\
      yylval.opname = string;				\
      return token;					\
    }      

/* Read one token, getting characters through LEXPTR.  */

static int
yylex (void)
{
  int c;
  int namelen;
  const char *tokstart, *tokptr;

 retry:
  prev_lexptr = lexptr;
  tokstart = lexptr;

  switch (c = *tokstart)
    {
    case 0:
      return 0;

    case ' ':
    case '\t':
    case '\n':
      lexptr++;
      goto retry;

    case '\'':
      /* We either have a character constant ('0' or '\177' for example)
	 or we have a quoted symbol reference ('foo(int,int)' in C++
	 for example). */
      lexptr++;
      c = *lexptr++;
      if (c == '\\')
	c = parse_escape (&lexptr);
      else if (c == '\'')
	{
	  yyerror ("empty character constant");
	  return ERROR;
	}

      c = *lexptr++;
      if (c != '\'')
	{
	  yyerror ("invalid character constant");
	  return ERROR;
	}

      /* FIXME: We should refer to a canonical form of the character,
	 presumably the same one that appears in manglings - the decimal
	 representation.  But if that isn't in our input then we have to
	 allocate memory for it somewhere.  */
      yylval.comp = fill_comp (DEMANGLE_COMPONENT_LITERAL,
				 make_builtin_type ("char"),
				 make_name (tokstart, lexptr - tokstart));

      return INT;

    case '(':
      if (strncmp (tokstart, "(anonymous namespace)", 21) == 0)
	{
	  lexptr += 21;
	  yylval.comp = make_name ("(anonymous namespace)",
				     sizeof "(anonymous namespace)" - 1);
	  return NAME;
	}
	/* FALL THROUGH */

    case ')':
    case ',':
      lexptr++;
      return c;

    case '.':
      if (lexptr[1] == '.' && lexptr[2] == '.')
	{
	  lexptr += 3;
	  return ELLIPSIS;
	}

      /* Might be a floating point number.  */
      if (lexptr[1] < '0' || lexptr[1] > '9')
	goto symbol;		/* Nope, must be a symbol. */

      goto try_number;

    case '-':
      HANDLE_TOKEN2 ("-=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("--", DECREMENT);
      HANDLE_TOKEN2 ("->", ARROW);

      /* For construction vtables.  This is kind of hokey.  */
      if (strncmp (tokstart, "-in-", 4) == 0)
	{
	  lexptr += 4;
	  return CONSTRUCTION_IN;
	}

      if (lexptr[1] < '0' || lexptr[1] > '9')
	{
	  lexptr++;
	  return '-';
	}
      /* FALL THRU into number case.  */

    try_number:
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      {
	/* It's a number.  */
	int got_dot = 0, got_e = 0, toktype;
	const char *p = tokstart;
	int hex = 0;

	if (c == '-')
	  p++;

	if (c == '0' && (p[1] == 'x' || p[1] == 'X'))
	  {
	    p += 2;
	    hex = 1;
	  }
	else if (c == '0' && (p[1]=='t' || p[1]=='T' || p[1]=='d' || p[1]=='D'))
	  {
	    p += 2;
	    hex = 0;
	  }

	for (;; ++p)
	  {
	    /* This test includes !hex because 'e' is a valid hex digit
	       and thus does not indicate a floating point number when
	       the radix is hex.  */
	    if (!hex && !got_e && (*p == 'e' || *p == 'E'))
	      got_dot = got_e = 1;
	    /* This test does not include !hex, because a '.' always indicates
	       a decimal floating point number regardless of the radix.

	       NOTE drow/2005-03-09: This comment is not accurate in C99;
	       however, it's not clear that all the floating point support
	       in this file is doing any good here.  */
	    else if (!got_dot && *p == '.')
	      got_dot = 1;
	    else if (got_e && (p[-1] == 'e' || p[-1] == 'E')
		     && (*p == '-' || *p == '+'))
	      /* This is the sign of the exponent, not the end of the
		 number.  */
	      continue;
	    /* We will take any letters or digits.  parse_number will
	       complain if past the radix, or if L or U are not final.  */
	    else if (! ISALNUM (*p))
	      break;
	  }
	toktype = parse_number (tokstart, p - tokstart, got_dot|got_e);
        if (toktype == ERROR)
	  {
	    char *err_copy = (char *) alloca (p - tokstart + 1);

	    memcpy (err_copy, tokstart, p - tokstart);
	    err_copy[p - tokstart] = 0;
	    yyerror ("invalid number");
	    return ERROR;
	  }
	lexptr = p;
	return toktype;
      }

    case '+':
      HANDLE_TOKEN2 ("+=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("++", INCREMENT);
      lexptr++;
      return c;
    case '*':
      HANDLE_TOKEN2 ("*=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '/':
      HANDLE_TOKEN2 ("/=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '%':
      HANDLE_TOKEN2 ("%=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '|':
      HANDLE_TOKEN2 ("|=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("||", OROR);
      lexptr++;
      return c;
    case '&':
      HANDLE_TOKEN2 ("&=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("&&", ANDAND);
      lexptr++;
      return c;
    case '^':
      HANDLE_TOKEN2 ("^=", ASSIGN_MODIFY);
      lexptr++;
      return c;
    case '!':
      HANDLE_TOKEN2 ("!=", NOTEQUAL);
      lexptr++;
      return c;
    case '<':
      HANDLE_TOKEN3 ("<<=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 ("<=", LEQ);
      HANDLE_TOKEN2 ("<<", LSH);
      lexptr++;
      return c;
    case '>':
      HANDLE_TOKEN3 (">>=", ASSIGN_MODIFY);
      HANDLE_TOKEN2 (">=", GEQ);
      HANDLE_TOKEN2 (">>", RSH);
      lexptr++;
      return c;
    case '=':
      HANDLE_TOKEN2 ("==", EQUAL);
      lexptr++;
      return c;
    case ':':
      HANDLE_TOKEN2 ("::", COLONCOLON);
      lexptr++;
      return c;

    case '[':
    case ']':
    case '?':
    case '@':
    case '~':
    case '{':
    case '}':
    symbol:
      lexptr++;
      return c;

    case '"':
      /* These can't occur in C++ names.  */
      yyerror ("unexpected string literal");
      return ERROR;
    }

  if (!(c == '_' || c == '$' || ISALPHA (c)))
    {
      /* We must have come across a bad character (e.g. ';').  */
      yyerror ("invalid character");
      return ERROR;
    }

  /* It's a name.  See how long it is.  */
  namelen = 0;
  do
    c = tokstart[++namelen];
  while (ISALNUM (c) || c == '_' || c == '$');

  lexptr += namelen;

  /* Catch specific keywords.  Notice that some of the keywords contain
     spaces, and are sorted by the length of the first word.  They must
     all include a trailing space in the string comparison.  */
  switch (namelen)
    {
    case 16:
      if (strncmp (tokstart, "reinterpret_cast", 16) == 0)
        return REINTERPRET_CAST;
      break;
    case 12:
      if (strncmp (tokstart, "construction vtable for ", 24) == 0)
	{
	  lexptr = tokstart + 24;
	  return CONSTRUCTION_VTABLE;
	}
      if (strncmp (tokstart, "dynamic_cast", 12) == 0)
        return DYNAMIC_CAST;
      break;
    case 11:
      if (strncmp (tokstart, "static_cast", 11) == 0)
        return STATIC_CAST;
      break;
    case 9:
      HANDLE_SPECIAL ("covariant return thunk to ", DEMANGLE_COMPONENT_COVARIANT_THUNK);
      HANDLE_SPECIAL ("reference temporary for ", DEMANGLE_COMPONENT_REFTEMP);
      break;
    case 8:
      HANDLE_SPECIAL ("typeinfo for ", DEMANGLE_COMPONENT_TYPEINFO);
      HANDLE_SPECIAL ("typeinfo fn for ", DEMANGLE_COMPONENT_TYPEINFO_FN);
      HANDLE_SPECIAL ("typeinfo name for ", DEMANGLE_COMPONENT_TYPEINFO_NAME);
      if (strncmp (tokstart, "operator", 8) == 0)
	return OPERATOR;
      if (strncmp (tokstart, "restrict", 8) == 0)
	return RESTRICT;
      if (strncmp (tokstart, "unsigned", 8) == 0)
	return UNSIGNED;
      if (strncmp (tokstart, "template", 8) == 0)
	return TEMPLATE;
      if (strncmp (tokstart, "volatile", 8) == 0)
	return VOLATILE_KEYWORD;
      break;
    case 7:
      HANDLE_SPECIAL ("virtual thunk to ", DEMANGLE_COMPONENT_VIRTUAL_THUNK);
      if (strncmp (tokstart, "wchar_t", 7) == 0)
	return WCHAR_T;
      break;
    case 6:
      if (strncmp (tokstart, "global constructors keyed to ", 29) == 0)
	{
	  const char *p;
	  lexptr = tokstart + 29;
	  yylval.typed_val_int.val = GLOBAL_CONSTRUCTORS;
	  /* Find the end of the symbol.  */
	  p = symbol_end (lexptr);
	  yylval.typed_val_int.type = make_name (lexptr, p - lexptr);
	  lexptr = p;
	  return GLOBAL;
	}
      if (strncmp (tokstart, "global destructors keyed to ", 28) == 0)
	{
	  const char *p;
	  lexptr = tokstart + 28;
	  yylval.typed_val_int.val = GLOBAL_DESTRUCTORS;
	  /* Find the end of the symbol.  */
	  p = symbol_end (lexptr);
	  yylval.typed_val_int.type = make_name (lexptr, p - lexptr);
	  lexptr = p;
	  return GLOBAL;
	}

      HANDLE_SPECIAL ("vtable for ", DEMANGLE_COMPONENT_VTABLE);
      if (strncmp (tokstart, "delete", 6) == 0)
	return DELETE;
      if (strncmp (tokstart, "struct", 6) == 0)
	return STRUCT;
      if (strncmp (tokstart, "signed", 6) == 0)
	return SIGNED_KEYWORD;
      if (strncmp (tokstart, "sizeof", 6) == 0)
	return SIZEOF;
      if (strncmp (tokstart, "double", 6) == 0)
	return DOUBLE_KEYWORD;
      break;
    case 5:
      HANDLE_SPECIAL ("guard variable for ", DEMANGLE_COMPONENT_GUARD);
      if (strncmp (tokstart, "false", 5) == 0)
	return FALSEKEYWORD;
      if (strncmp (tokstart, "class", 5) == 0)
	return CLASS;
      if (strncmp (tokstart, "union", 5) == 0)
	return UNION;
      if (strncmp (tokstart, "float", 5) == 0)
	return FLOAT_KEYWORD;
      if (strncmp (tokstart, "short", 5) == 0)
	return SHORT;
      if (strncmp (tokstart, "const", 5) == 0)
	return CONST_KEYWORD;
      break;
    case 4:
      if (strncmp (tokstart, "void", 4) == 0)
	return VOID;
      if (strncmp (tokstart, "bool", 4) == 0)
	return BOOL;
      if (strncmp (tokstart, "char", 4) == 0)
	return CHAR;
      if (strncmp (tokstart, "enum", 4) == 0)
	return ENUM;
      if (strncmp (tokstart, "long", 4) == 0)
	return LONG;
      if (strncmp (tokstart, "true", 4) == 0)
	return TRUEKEYWORD;
      break;
    case 3:
      HANDLE_SPECIAL ("VTT for ", DEMANGLE_COMPONENT_VTT);
      HANDLE_SPECIAL ("non-virtual thunk to ", DEMANGLE_COMPONENT_THUNK);
      if (strncmp (tokstart, "new", 3) == 0)
	return NEW;
      if (strncmp (tokstart, "int", 3) == 0)
	return INT_KEYWORD;
      break;
    default:
      break;
    }

  yylval.comp = make_name (tokstart, namelen);
  return NAME;
}

static void
yyerror (char *msg)
{
  if (global_errmsg)
    return;

  error_lexptr = prev_lexptr;
  global_errmsg = msg ? msg : "parse error";
}

/* Allocate all the components we'll need to build a tree.  We generally
   allocate too many components, but the extra memory usage doesn't hurt
   because the trees are temporary.  If we start keeping the trees for
   a longer lifetime we'll need to be cleverer.  */
static struct demangle_info *
allocate_info (int comps)
{
  struct demangle_info *ret;

  ret = malloc (sizeof (struct demangle_info)
		+ sizeof (struct demangle_component) * (comps - 1));
  ret->used = 0;
  return ret;
}

/* Convert RESULT to a string.  The return value is allocated
   using xmalloc.  ESTIMATED_LEN is used only as a guide to the
   length of the result.  This functions handles a few cases that
   cplus_demangle_print does not, specifically the global destructor
   and constructor labels.  */

char *
cp_comp_to_string (struct demangle_component *result, int estimated_len)
{
  char *str, *prefix = NULL, *buf;
  size_t err = 0;

  if (result->type == GLOBAL_DESTRUCTORS)
    {
      result = d_left (result);
      prefix = "global destructors keyed to ";
    }
  else if (result->type == GLOBAL_CONSTRUCTORS)
    {
      result = d_left (result);
      prefix = "global constructors keyed to ";
    }

  str = cplus_demangle_print (DMGL_PARAMS | DMGL_ANSI, result, estimated_len, &err);
  if (str == NULL)
    return NULL;

  if (prefix == NULL)
    return str;

  buf = malloc (strlen (str) + strlen (prefix) + 1);
  strcpy (buf, prefix);
  strcat (buf, str);
  free (str);
  return (buf);
}

/* Convert a demangled name to a demangle_component tree.  *MEMORY is set to the
   block of used memory that should be freed when finished with the
   tree.  On error, NULL is returned, and an error message will be
   set in *ERRMSG (which does not need to be freed).  */

struct demangle_component *
cp_demangled_name_to_comp (const char *demangled_name, void **memory,
			   const char **errmsg)
{
  static char errbuf[60];
  struct demangle_component *result;

  int len = strlen (demangled_name);

  len = len + len / 8;
  prev_lexptr = lexptr = demangled_name;
  error_lexptr = NULL;
  global_errmsg = NULL;

  demangle_info = allocate_info (len);

  if (yyparse ())
    {
      if (global_errmsg && errmsg)
	{
	  snprintf (errbuf, sizeof (errbuf) - 2, "%s, near `%s",
		    global_errmsg, error_lexptr);
	  strcat (errbuf, "'");
	  *errmsg = errbuf;
	}
      free (demangle_info);
      return NULL;
    }

  *memory = demangle_info;
  result = global_result;
  global_result = NULL;

  return result;
}

#ifdef TEST_CPNAMES

static void
cp_print (struct demangle_component *result)
{
  char *str;
  size_t err = 0;

  if (result->type == GLOBAL_DESTRUCTORS)
    {
      result = d_left (result);
      fputs ("global destructors keyed to ", stdout);
    }
  else if (result->type == GLOBAL_CONSTRUCTORS)
    {
      result = d_left (result);
      fputs ("global constructors keyed to ", stdout);
    }

  str = cplus_demangle_print (DMGL_PARAMS | DMGL_ANSI, result, 64, &err);
  if (str == NULL)
    return;

  fputs (str, stdout);

  free (str);
}

static char
trim_chars (char *lexptr, char **extra_chars)
{
  char *p = (char *) symbol_end (lexptr);
  char c = 0;

  if (*p)
    {
      c = *p;
      *p = 0;
      *extra_chars = p + 1;
    }

  return c;
}

int
main (int argc, char **argv)
{
  char *str2, *extra_chars, c;
  char buf[65536];
  int arg;
  const char *errmsg;
  void *memory;
  struct demangle_component *result;

  arg = 1;
  if (argv[arg] && strcmp (argv[arg], "--debug") == 0)
    {
      yydebug = 1;
      arg++;
    }

  if (argv[arg] == NULL)
    while (fgets (buf, 65536, stdin) != NULL)
      {
	int len;
	buf[strlen (buf) - 1] = 0;
	/* Use DMGL_VERBOSE to get expanded standard substitutions.  */
	c = trim_chars (buf, &extra_chars);
	str2 = cplus_demangle (buf, DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE);
	if (str2 == NULL)
	  {
	    /* printf ("Demangling error\n"); */
	    if (c)
	      printf ("%s%c%s\n", buf, c, extra_chars);
	    else
	      printf ("%s\n", buf);
	    continue;
	  }
	result = cp_demangled_name_to_comp (str2, &memory, &errmsg);
	if (result == NULL)
	  {
	    fputs (errmsg, stderr);
	    fputc ('\n', stderr);
	    continue;
	  }

	cp_print (result);
	free (memory);

	free (str2);
	if (c)
	  {
	    putchar (c);
	    fputs (extra_chars, stdout);
	  }
	putchar ('\n');
      }
  else
    {
      result = cp_demangled_name_to_comp (argv[arg], &memory, &errmsg);
      if (result == NULL)
	{
	  fputs (errmsg, stderr);
	  fputc ('\n', stderr);
	  return 0;
	}
      cp_print (result);
      putchar ('\n');
      free (memory);
    }
  return 0;
}

#endif

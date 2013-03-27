/* YACC parser for Ada expressions, for GDB.
   Copyright (C) 1986, 1989, 1990, 1991, 1993, 1994, 1997, 2000, 2003, 2004,
   2007 Free Software Foundation, Inc.

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

/* Parse an Ada expression from text in a string,
   and return the result as a  struct expression  pointer.
   That structure contains arithmetic operations in reverse polish,
   with constants represented by operations that are followed by special data.
   See expression.h for the details of the format.
   What is important here is that it can be built up sequentially
   during the process of parsing; the lower levels of the tree always
   come first in the result.

   malloc's and realloc's in this file are transformed to
   xmalloc and xrealloc respectively by the same sed command in the
   makefile that remaps any other malloc/realloc inserted by the parser
   generator.  Doing this with #defines and trying to control the interaction
   with include files (<malloc.h> and <stdlib.h> for example) just became
   too messy, particularly when such includes can be inserted at random
   times by the parser generator.  */

%{

#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "ada-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */
#include "frame.h"
#include "block.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  These are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list.  */

/* NOTE: This is clumsy, especially since BISON and FLEX provide --prefix
   options.  I presume we are maintaining it to accommodate systems
   without BISON?  (PNH) */

#define	yymaxdepth ada_maxdepth
#define	yyparse	_ada_parse	/* ada_parse calls this after  initialization */
#define	yylex	ada_lex
#define	yyerror	ada_error
#define	yylval	ada_lval
#define	yychar	ada_char
#define	yydebug	ada_debug
#define	yypact	ada_pact
#define	yyr1	ada_r1
#define	yyr2	ada_r2
#define	yydef	ada_def
#define	yychk	ada_chk
#define	yypgo	ada_pgo
#define	yyact	ada_act
#define	yyexca	ada_exca
#define yyerrflag ada_errflag
#define yynerrs	ada_nerrs
#define	yyps	ada_ps
#define	yypv	ada_pv
#define	yys	ada_s
#define	yy_yys	ada_yys
#define	yystate	ada_state
#define	yytmp	ada_tmp
#define	yyv	ada_v
#define	yy_yyv	ada_yyv
#define	yyval	ada_val
#define	yylloc	ada_lloc
#define yyreds	ada_reds		/* With YYDEBUG defined */
#define yytoks	ada_toks		/* With YYDEBUG defined */
#define yyname	ada_name		/* With YYDEBUG defined */
#define yyrule	ada_rule		/* With YYDEBUG defined */

#ifndef YYDEBUG
#define	YYDEBUG	1		/* Default to yydebug support */
#endif

#define YYFPRINTF parser_fprintf

struct name_info {
  struct symbol *sym;
  struct minimal_symbol *msym;
  struct block *block;
  struct stoken stoken;
};

static struct stoken empty_stoken = { "", 0 };

/* If expression is in the context of TYPE'(...), then TYPE, else
 * NULL.  */
static struct type *type_qualifier;

int yyparse (void);

static int yylex (void);

void yyerror (char *);

static struct stoken string_to_operator (struct stoken);

static void write_int (LONGEST, struct type *);

static void write_object_renaming (struct block *, struct symbol *, int);

static struct type* write_var_or_type (struct block *, struct stoken);

static void write_name_assoc (struct stoken);

static void write_exp_op_with_string (enum exp_opcode, struct stoken);

static struct block *block_lookup (struct block *, char *);

static LONGEST convert_char_literal (struct type *, LONGEST);

static void write_ambiguous_var (struct block *, char *, int);

static struct type *type_int (void);

static struct type *type_long (void);

static struct type *type_long_long (void);

static struct type *type_float (void);

static struct type *type_double (void);

static struct type *type_long_double (void);

static struct type *type_char (void);

static struct type *type_system_address (void);

%}

%union
  {
    LONGEST lval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val;
    struct {
      DOUBLEST dval;
      struct type *type;
    } typed_val_float;
    struct type *tval;
    struct stoken sval;
    struct block *bval;
    struct internalvar *ivar;
  }

%type <lval> positional_list component_groups component_associations
%type <lval> aggregate_component_list 
%type <tval> var_or_type

%token <typed_val> INT NULL_PTR CHARLIT
%token <typed_val_float> FLOAT
%token COLONCOLON
%token <sval> STRING NAME DOT_ID 
%type <bval> block
%type <lval> arglist tick_arglist

%type <tval> save_qualifier

%token DOT_ALL

/* Special type cases, put in to allow the parser to distinguish different
   legal basetypes.  */
%token <sval> SPECIAL_VARIABLE

%nonassoc ASSIGN
%left _AND_ OR XOR THEN ELSE
%left '=' NOTEQUAL '<' '>' LEQ GEQ IN DOTDOT
%left '@'
%left '+' '-' '&'
%left UNARY
%left '*' '/' MOD REM
%right STARSTAR ABS NOT

/* Artificial token to give NAME => ... and NAME | priority over reducing 
   NAME to <primary> and to give <primary>' priority over reducing <primary>
   to <simple_exp>. */
%nonassoc VAR

%nonassoc ARROW '|'

%right TICK_ACCESS TICK_ADDRESS TICK_FIRST TICK_LAST TICK_LENGTH
%right TICK_MAX TICK_MIN TICK_MODULUS
%right TICK_POS TICK_RANGE TICK_SIZE TICK_TAG TICK_VAL
 /* The following are right-associative only so that reductions at this
    precedence have lower precedence than '.' and '('.  The syntax still
    forces a.b.c, e.g., to be LEFT-associated.  */
%right '.' '(' '[' DOT_ID DOT_ALL

%token NEW OTHERS


%%

start   :	exp1
	;

/* Expressions, including the sequencing operator.  */
exp1	:	exp
	|	exp1 ';' exp
			{ write_exp_elt_opcode (BINOP_COMMA); }
	| 	primary ASSIGN exp   /* Extension for convenience */
			{ write_exp_elt_opcode (BINOP_ASSIGN); }
	;

/* Expressions, not including the sequencing operator.  */
primary :	primary DOT_ALL
			{ write_exp_elt_opcode (UNOP_IND); }
	;

primary :	primary DOT_ID
			{ write_exp_op_with_string (STRUCTOP_STRUCT, $2); }
	;

primary :	primary '(' arglist ')'
			{
			  write_exp_elt_opcode (OP_FUNCALL);
			  write_exp_elt_longcst ($3);
			  write_exp_elt_opcode (OP_FUNCALL);
		        }
	|	var_or_type '(' arglist ')'
			{
			  if ($1 != NULL)
			    {
			      if ($3 != 1)
				error (_("Invalid conversion"));
			      write_exp_elt_opcode (UNOP_CAST);
			      write_exp_elt_type ($1);
			      write_exp_elt_opcode (UNOP_CAST);
			    }
			  else
			    {
			      write_exp_elt_opcode (OP_FUNCALL);
			      write_exp_elt_longcst ($3);
			      write_exp_elt_opcode (OP_FUNCALL);
			    }
			}
	;

primary :	var_or_type '\'' save_qualifier { type_qualifier = $1; } 
		   '(' exp ')'
			{
			  if ($1 == NULL)
			    error (_("Type required for qualification"));
			  write_exp_elt_opcode (UNOP_QUAL);
			  write_exp_elt_type ($1);
			  write_exp_elt_opcode (UNOP_QUAL);
			  type_qualifier = $3;
			}
	;

save_qualifier : 	{ $$ = type_qualifier; }
	;

primary :
		primary '(' simple_exp DOTDOT simple_exp ')'
			{ write_exp_elt_opcode (TERNOP_SLICE); }
	|	var_or_type '(' simple_exp DOTDOT simple_exp ')'
			{ if ($1 == NULL) 
                            write_exp_elt_opcode (TERNOP_SLICE);
			  else
			    error (_("Cannot slice a type"));
			}
	;

primary :	'(' exp1 ')'	{ }
	;

/* The following rule causes a conflict with the type conversion
       var_or_type (exp)
   To get around it, we give '(' higher priority and add bridge rules for 
       var_or_type (exp, exp, ...)
       var_or_type (exp .. exp)
   We also have the action for  var_or_type(exp) generate a function call
   when the first symbol does not denote a type. */

primary :	var_or_type	%prec VAR
			{ if ($1 != NULL)
			    {
			      write_exp_elt_opcode (OP_TYPE);
			      write_exp_elt_type ($1);
			      write_exp_elt_opcode (OP_TYPE);
			    }
			}
	;

primary :	SPECIAL_VARIABLE /* Various GDB extensions */
			{ write_dollar_variable ($1); }
	;

primary :     	aggregate
        ;        

simple_exp : 	primary
	;

simple_exp :	'-' simple_exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_NEG); }
	;

simple_exp :	'+' simple_exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_PLUS); }
	;

simple_exp :	NOT simple_exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); }
	;

simple_exp :    ABS simple_exp	   %prec UNARY
			{ write_exp_elt_opcode (UNOP_ABS); }
	;

arglist	:		{ $$ = 0; }
	;

arglist	:	exp
			{ $$ = 1; }
	|	NAME ARROW exp
			{ $$ = 1; }
	|	arglist ',' exp
			{ $$ = $1 + 1; }
	|	arglist ',' NAME ARROW exp
			{ $$ = $1 + 1; }
	;

simple_exp :	'{' var_or_type '}' simple_exp  %prec '.'
		/* GDB extension */
			{ 
			  if ($2 == NULL)
			    error (_("Type required within braces in coercion"));
			  write_exp_elt_opcode (UNOP_MEMVAL);
			  write_exp_elt_type ($2);
			  write_exp_elt_opcode (UNOP_MEMVAL);
			}
	;

/* Binary operators in order of decreasing precedence.  */

simple_exp 	: 	simple_exp STARSTAR simple_exp
			{ write_exp_elt_opcode (BINOP_EXP); }
	;

simple_exp	:	simple_exp '*' simple_exp
			{ write_exp_elt_opcode (BINOP_MUL); }
	;

simple_exp	:	simple_exp '/' simple_exp
			{ write_exp_elt_opcode (BINOP_DIV); }
	;

simple_exp	:	simple_exp REM simple_exp /* May need to be fixed to give correct Ada REM */
			{ write_exp_elt_opcode (BINOP_REM); }
	;

simple_exp	:	simple_exp MOD simple_exp
			{ write_exp_elt_opcode (BINOP_MOD); }
	;

simple_exp	:	simple_exp '@' simple_exp	/* GDB extension */
			{ write_exp_elt_opcode (BINOP_REPEAT); }
	;

simple_exp	:	simple_exp '+' simple_exp
			{ write_exp_elt_opcode (BINOP_ADD); }
	;

simple_exp	:	simple_exp '&' simple_exp
			{ write_exp_elt_opcode (BINOP_CONCAT); }
	;

simple_exp	:	simple_exp '-' simple_exp
			{ write_exp_elt_opcode (BINOP_SUB); }
	;

relation :	simple_exp
	;

relation :	simple_exp '=' simple_exp
			{ write_exp_elt_opcode (BINOP_EQUAL); }
	;

relation :	simple_exp NOTEQUAL simple_exp
			{ write_exp_elt_opcode (BINOP_NOTEQUAL); }
	;

relation :	simple_exp LEQ simple_exp
			{ write_exp_elt_opcode (BINOP_LEQ); }
	;

relation :	simple_exp IN simple_exp DOTDOT simple_exp
			{ write_exp_elt_opcode (TERNOP_IN_RANGE); }
        |       simple_exp IN primary TICK_RANGE tick_arglist
			{ write_exp_elt_opcode (BINOP_IN_BOUNDS);
			  write_exp_elt_longcst ((LONGEST) $5);
			  write_exp_elt_opcode (BINOP_IN_BOUNDS);
			}
 	|	simple_exp IN var_or_type	%prec TICK_ACCESS
			{ 
			  if ($3 == NULL)
			    error (_("Right operand of 'in' must be type"));
			  write_exp_elt_opcode (UNOP_IN_RANGE);
		          write_exp_elt_type ($3);
		          write_exp_elt_opcode (UNOP_IN_RANGE);
			}
	|	simple_exp NOT IN simple_exp DOTDOT simple_exp
			{ write_exp_elt_opcode (TERNOP_IN_RANGE);
		          write_exp_elt_opcode (UNOP_LOGICAL_NOT);
			}
        |       simple_exp NOT IN primary TICK_RANGE tick_arglist
			{ write_exp_elt_opcode (BINOP_IN_BOUNDS);
			  write_exp_elt_longcst ((LONGEST) $6);
			  write_exp_elt_opcode (BINOP_IN_BOUNDS);
		          write_exp_elt_opcode (UNOP_LOGICAL_NOT);
			}
 	|	simple_exp NOT IN var_or_type	%prec TICK_ACCESS
			{ 
			  if ($4 == NULL)
			    error (_("Right operand of 'in' must be type"));
			  write_exp_elt_opcode (UNOP_IN_RANGE);
		          write_exp_elt_type ($4);
		          write_exp_elt_opcode (UNOP_IN_RANGE);
		          write_exp_elt_opcode (UNOP_LOGICAL_NOT);
			}
	;

relation :	simple_exp GEQ simple_exp
			{ write_exp_elt_opcode (BINOP_GEQ); }
	;

relation :	simple_exp '<' simple_exp
			{ write_exp_elt_opcode (BINOP_LESS); }
	;

relation :	simple_exp '>' simple_exp
			{ write_exp_elt_opcode (BINOP_GTR); }
	;

exp	:	relation
	|	and_exp
	|	and_then_exp
	|	or_exp
	|	or_else_exp
	|	xor_exp
	;

and_exp :
		relation _AND_ relation 
			{ write_exp_elt_opcode (BINOP_BITWISE_AND); }
	|	and_exp _AND_ relation
			{ write_exp_elt_opcode (BINOP_BITWISE_AND); }
	;

and_then_exp :
	       relation _AND_ THEN relation
			{ write_exp_elt_opcode (BINOP_LOGICAL_AND); }
	|	and_then_exp _AND_ THEN relation
			{ write_exp_elt_opcode (BINOP_LOGICAL_AND); }
        ;

or_exp :
		relation OR relation 
			{ write_exp_elt_opcode (BINOP_BITWISE_IOR); }
	|	or_exp OR relation
			{ write_exp_elt_opcode (BINOP_BITWISE_IOR); }
	;

or_else_exp :
	       relation OR ELSE relation
			{ write_exp_elt_opcode (BINOP_LOGICAL_OR); }
	|      or_else_exp OR ELSE relation
			{ write_exp_elt_opcode (BINOP_LOGICAL_OR); }
        ;

xor_exp :       relation XOR relation
			{ write_exp_elt_opcode (BINOP_BITWISE_XOR); }
	|	xor_exp XOR relation
			{ write_exp_elt_opcode (BINOP_BITWISE_XOR); }
        ;

/* Primaries can denote types (OP_TYPE).  In cases such as 
   primary TICK_ADDRESS, where a type would be invalid, it will be
   caught when evaluate_subexp in ada-lang.c tries to evaluate the
   primary, expecting a value.  Precedence rules resolve the ambiguity
   in NAME TICK_ACCESS in favor of shifting to form a var_or_type.  A
   construct such as aType'access'access will again cause an error when
   aType'access evaluates to a type that evaluate_subexp attempts to 
   evaluate. */
primary :	primary TICK_ACCESS
			{ write_exp_elt_opcode (UNOP_ADDR); }
	|	primary TICK_ADDRESS
			{ write_exp_elt_opcode (UNOP_ADDR);
			  write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type (type_system_address ());
			  write_exp_elt_opcode (UNOP_CAST);
			}
	|	primary TICK_FIRST tick_arglist
			{ write_int ($3, type_int ());
			  write_exp_elt_opcode (OP_ATR_FIRST); }
	|	primary TICK_LAST tick_arglist
			{ write_int ($3, type_int ());
			  write_exp_elt_opcode (OP_ATR_LAST); }
	| 	primary TICK_LENGTH tick_arglist
			{ write_int ($3, type_int ());
			  write_exp_elt_opcode (OP_ATR_LENGTH); }
        |       primary TICK_SIZE
			{ write_exp_elt_opcode (OP_ATR_SIZE); }
	|	primary TICK_TAG
			{ write_exp_elt_opcode (OP_ATR_TAG); }
        |       opt_type_prefix TICK_MIN '(' exp ',' exp ')'
			{ write_exp_elt_opcode (OP_ATR_MIN); }
        |       opt_type_prefix TICK_MAX '(' exp ',' exp ')'
			{ write_exp_elt_opcode (OP_ATR_MAX); }
	| 	opt_type_prefix TICK_POS '(' exp ')'
			{ write_exp_elt_opcode (OP_ATR_POS); }
	|	type_prefix TICK_VAL '(' exp ')'
			{ write_exp_elt_opcode (OP_ATR_VAL); }
	|	type_prefix TICK_MODULUS
			{ write_exp_elt_opcode (OP_ATR_MODULUS); }
	;

tick_arglist :			%prec '('
			{ $$ = 1; }
	| 	'(' INT ')'
			{ $$ = $2.val; }
	;

type_prefix :
                var_or_type
			{ 
			  if ($1 == NULL)
			    error (_("Prefix must be type"));
			  write_exp_elt_opcode (OP_TYPE);
			  write_exp_elt_type ($1);
			  write_exp_elt_opcode (OP_TYPE); }
	;

opt_type_prefix :
		type_prefix
	| 	/* EMPTY */
			{ write_exp_elt_opcode (OP_TYPE);
			  write_exp_elt_type (builtin_type_void);
			  write_exp_elt_opcode (OP_TYPE); }
	;


primary	:	INT
			{ write_int ((LONGEST) $1.val, $1.type); }
	;

primary	:	CHARLIT
                  { write_int (convert_char_literal (type_qualifier, $1.val),
			       (type_qualifier == NULL) 
			       ? $1.type : type_qualifier);
		  }
	;

primary	:	FLOAT
			{ write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type ($1.type);
			  write_exp_elt_dblcst ($1.dval);
			  write_exp_elt_opcode (OP_DOUBLE);
			}
	;

primary	:	NULL_PTR
			{ write_int (0, type_int ()); }
	;

primary	:	STRING
			{ 
			  write_exp_op_with_string (OP_STRING, $1);
			}
	;

primary	: 	NEW NAME
			{ error (_("NEW not implemented.")); }
	;

var_or_type:	NAME   	    %prec VAR
				{ $$ = write_var_or_type (NULL, $1); } 
	|	block NAME  %prec VAR
                                { $$ = write_var_or_type ($1, $2); }
	|       NAME TICK_ACCESS 
			{ 
			  $$ = write_var_or_type (NULL, $1);
			  if ($$ == NULL)
			    write_exp_elt_opcode (UNOP_ADDR);
			  else
			    $$ = lookup_pointer_type ($$);
			}
	|	block NAME TICK_ACCESS
			{ 
			  $$ = write_var_or_type ($1, $2);
			  if ($$ == NULL)
			    write_exp_elt_opcode (UNOP_ADDR);
			  else
			    $$ = lookup_pointer_type ($$);
			}
	;

/* GDB extension */
block   :       NAME COLONCOLON
			{ $$ = block_lookup (NULL, $1.ptr); }
	|	block NAME COLONCOLON
			{ $$ = block_lookup ($1, $2.ptr); }
	;

aggregate :
		'(' aggregate_component_list ')'  
			{
			  write_exp_elt_opcode (OP_AGGREGATE);
			  write_exp_elt_longcst ($2);
			  write_exp_elt_opcode (OP_AGGREGATE);
		        }
	;

aggregate_component_list :
		component_groups	 { $$ = $1; }
	|	positional_list exp
			{ write_exp_elt_opcode (OP_POSITIONAL);
			  write_exp_elt_longcst ($1);
			  write_exp_elt_opcode (OP_POSITIONAL);
			  $$ = $1 + 1;
			}
	|	positional_list component_groups
					 { $$ = $1 + $2; }
	;

positional_list :
		exp ','
			{ write_exp_elt_opcode (OP_POSITIONAL);
			  write_exp_elt_longcst (0);
			  write_exp_elt_opcode (OP_POSITIONAL);
			  $$ = 1;
			} 
	|	positional_list exp ','
			{ write_exp_elt_opcode (OP_POSITIONAL);
			  write_exp_elt_longcst ($1);
			  write_exp_elt_opcode (OP_POSITIONAL);
			  $$ = $1 + 1; 
			}
	;

component_groups:
		others			 { $$ = 1; }
	|	component_group		 { $$ = 1; }
	|	component_group ',' component_groups
					 { $$ = $3 + 1; }
	;

others 	:	OTHERS ARROW exp
			{ write_exp_elt_opcode (OP_OTHERS); }
	;

component_group :
		component_associations
			{
			  write_exp_elt_opcode (OP_CHOICES);
			  write_exp_elt_longcst ($1);
			  write_exp_elt_opcode (OP_CHOICES);
		        }
	;

/* We use this somewhat obscure definition in order to handle NAME => and
   NAME | differently from exp => and exp |.  ARROW and '|' have a precedence
   above that of the reduction of NAME to var_or_type.  By delaying 
   decisions until after the => or '|', we convert the ambiguity to a 
   resolved shift/reduce conflict. */
component_associations :
		NAME ARROW 
			{ write_name_assoc ($1); }
		    exp	{ $$ = 1; }
	|	simple_exp ARROW exp
			{ $$ = 1; }
	|	simple_exp DOTDOT simple_exp ARROW 
			{ write_exp_elt_opcode (OP_DISCRETE_RANGE);
			  write_exp_op_with_string (OP_NAME, empty_stoken);
			}
		    exp { $$ = 1; }
	|	NAME '|' 
		        { write_name_assoc ($1); }
		    component_associations  { $$ = $4 + 1; }
	|	simple_exp '|'  
	            component_associations  { $$ = $3 + 1; }
	|	simple_exp DOTDOT simple_exp '|'
			{ write_exp_elt_opcode (OP_DISCRETE_RANGE); }
		    component_associations  { $$ = $6 + 1; }
	;

/* Some extensions borrowed from C, for the benefit of those who find they
   can't get used to Ada notation in GDB.  */

primary	:	'*' primary		%prec '.'
			{ write_exp_elt_opcode (UNOP_IND); }
	|	'&' primary		%prec '.'
			{ write_exp_elt_opcode (UNOP_ADDR); }
	|	primary '[' exp ']'
			{ write_exp_elt_opcode (BINOP_SUBSCRIPT); }
	;

%%

/* yylex defined in ada-lex.c: Reads one token, getting characters */
/* through lexptr.  */

/* Remap normal flex interface names (yylex) as well as gratuitiously */
/* global symbol names, so we can have multiple flex-generated parsers */
/* in gdb.  */

/* (See note above on previous definitions for YACC.) */

#define yy_create_buffer ada_yy_create_buffer
#define yy_delete_buffer ada_yy_delete_buffer
#define yy_init_buffer ada_yy_init_buffer
#define yy_load_buffer_state ada_yy_load_buffer_state
#define yy_switch_to_buffer ada_yy_switch_to_buffer
#define yyrestart ada_yyrestart
#define yytext ada_yytext
#define yywrap ada_yywrap

static struct obstack temp_parse_space;

/* The following kludge was found necessary to prevent conflicts between */
/* defs.h and non-standard stdlib.h files.  */
#define qsort __qsort__dummy
#include "ada-lex.c"

int
ada_parse (void)
{
  lexer_init (yyin);		/* (Re-)initialize lexer.  */
  type_qualifier = NULL;
  obstack_free (&temp_parse_space, NULL);
  obstack_init (&temp_parse_space);

  return _ada_parse ();
}

void
yyerror (char *msg)
{
  error (_("Error in expression, near `%s'."), lexptr);
}

/* The operator name corresponding to operator symbol STRING (adds
   quotes and maps to lower-case).  Destroys the previous contents of
   the array pointed to by STRING.ptr.  Error if STRING does not match
   a valid Ada operator.  Assumes that STRING.ptr points to a
   null-terminated string and that, if STRING is a valid operator
   symbol, the array pointed to by STRING.ptr contains at least
   STRING.length+3 characters.  */

static struct stoken
string_to_operator (struct stoken string)
{
  int i;

  for (i = 0; ada_opname_table[i].encoded != NULL; i += 1)
    {
      if (string.length == strlen (ada_opname_table[i].decoded)-2
	  && strncasecmp (string.ptr, ada_opname_table[i].decoded+1,
			  string.length) == 0)
	{
	  strncpy (string.ptr, ada_opname_table[i].decoded,
		   string.length+2);
	  string.length += 2;
	  return string;
	}
    }
  error (_("Invalid operator symbol `%s'"), string.ptr);
}

/* Emit expression to access an instance of SYM, in block BLOCK (if
 * non-NULL), and with :: qualification ORIG_LEFT_CONTEXT.  */
static void
write_var_from_sym (struct block *orig_left_context,
		    struct block *block,
		    struct symbol *sym)
{
  if (orig_left_context == NULL && symbol_read_needs_frame (sym))
    {
      if (innermost_block == 0
	  || contained_in (block, innermost_block))
	innermost_block = block;
    }

  write_exp_elt_opcode (OP_VAR_VALUE);
  write_exp_elt_block (block);
  write_exp_elt_sym (sym);
  write_exp_elt_opcode (OP_VAR_VALUE);
}

/* Write integer constant ARG of type TYPE.  */

static void
write_int (LONGEST arg, struct type *type)
{
  write_exp_elt_opcode (OP_LONG);
  write_exp_elt_type (type);
  write_exp_elt_longcst (arg);
  write_exp_elt_opcode (OP_LONG);
}

/* Write an OPCODE, string, OPCODE sequence to the current expression.  */
static void
write_exp_op_with_string (enum exp_opcode opcode, struct stoken token)
{
  write_exp_elt_opcode (opcode);
  write_exp_string (token);
  write_exp_elt_opcode (opcode);
}
  
/* Emit expression corresponding to the renamed object designated by
 * the type RENAMING, which must be the referent of an object renaming
 * type, in the context of ORIG_LEFT_CONTEXT.  MAX_DEPTH is the maximum
 * number of cascaded renamings to allow.  */
static void
write_object_renaming (struct block *orig_left_context, 
		       struct symbol *renaming, int max_depth)
{
  const char *qualification = SYMBOL_LINKAGE_NAME (renaming);
  const char *simple_tail;
  const char *expr = TYPE_FIELD_NAME (SYMBOL_TYPE (renaming), 0);
  const char *suffix;
  char *name;
  struct symbol *sym;
  enum { SIMPLE_INDEX, LOWER_BOUND, UPPER_BOUND } slice_state;

  if (max_depth <= 0)
    error (_("Could not find renamed symbol"));

  /* if orig_left_context is null, then use the currently selected
     block; otherwise we might fail our symbol lookup below.  */
  if (orig_left_context == NULL)
    orig_left_context = get_selected_block (NULL);

  for (simple_tail = qualification + strlen (qualification);
       simple_tail != qualification; simple_tail -= 1)
    {
      if (*simple_tail == '.')
	{
	  simple_tail += 1;
	  break;
	}
      else if (strncmp (simple_tail, "__", 2) == 0)
	{
	  simple_tail += 2;
	  break;
	}
    }

  suffix = strstr (expr, "___XE");
  if (suffix == NULL)
    goto BadEncoding;

  name = (char *) obstack_alloc (&temp_parse_space, suffix - expr + 1);
  strncpy (name, expr, suffix-expr);
  name[suffix-expr] = '\000';
  sym = lookup_symbol (name, orig_left_context, VAR_DOMAIN, 0, NULL);
  if (sym == NULL)
    error (_("Could not find renamed variable: %s"), ada_decode (name));
  if (ada_is_object_renaming (sym))
    write_object_renaming (orig_left_context, sym, max_depth-1);
  else
    write_var_from_sym (orig_left_context, block_found, sym);

  suffix += 5;
  slice_state = SIMPLE_INDEX;
  while (*suffix == 'X')
    {
      suffix += 1;

      switch (*suffix) {
      case 'A':
        suffix += 1;
        write_exp_elt_opcode (UNOP_IND);
        break;
      case 'L':
	slice_state = LOWER_BOUND;
      case 'S':
	suffix += 1;
	if (isdigit (*suffix))
	  {
	    char *next;
	    long val = strtol (suffix, &next, 10);
	    if (next == suffix)
	      goto BadEncoding;
	    suffix = next;
	    write_exp_elt_opcode (OP_LONG);
	    write_exp_elt_type (type_int ());
	    write_exp_elt_longcst ((LONGEST) val);
	    write_exp_elt_opcode (OP_LONG);
	  }
	else
	  {
	    const char *end;
	    char *index_name;
	    int index_len;
	    struct symbol *index_sym;

	    end = strchr (suffix, 'X');
	    if (end == NULL)
	      end = suffix + strlen (suffix);

	    index_len = simple_tail - qualification + 2 + (suffix - end) + 1;
	    index_name
	      = (char *) obstack_alloc (&temp_parse_space, index_len);
	    memset (index_name, '\000', index_len);
	    strncpy (index_name, qualification, simple_tail - qualification);
	    index_name[simple_tail - qualification] = '\000';
	    strncat (index_name, suffix, suffix-end);
	    suffix = end;

	    index_sym =
	      lookup_symbol (index_name, NULL, VAR_DOMAIN, 0, NULL);
	    if (index_sym == NULL)
	      error (_("Could not find %s"), index_name);
	    write_var_from_sym (NULL, block_found, sym);
	  }
	if (slice_state == SIMPLE_INDEX)
	  {
	    write_exp_elt_opcode (OP_FUNCALL);
	    write_exp_elt_longcst ((LONGEST) 1);
	    write_exp_elt_opcode (OP_FUNCALL);
	  }
	else if (slice_state == LOWER_BOUND)
	  slice_state = UPPER_BOUND;
	else if (slice_state == UPPER_BOUND)
	  {
	    write_exp_elt_opcode (TERNOP_SLICE);
	    slice_state = SIMPLE_INDEX;
	  }
	break;

      case 'R':
	{
	  struct stoken field_name;
	  const char *end;
	  suffix += 1;

	  if (slice_state != SIMPLE_INDEX)
	    goto BadEncoding;
	  end = strchr (suffix, 'X');
	  if (end == NULL)
	    end = suffix + strlen (suffix);
	  field_name.length = end - suffix;
	  field_name.ptr = xmalloc (end - suffix + 1);
	  strncpy (field_name.ptr, suffix, end - suffix);
	  field_name.ptr[end - suffix] = '\000';
	  suffix = end;
	  write_exp_op_with_string (STRUCTOP_STRUCT, field_name);
	  break;
	}

      default:
	goto BadEncoding;
      }
    }
  if (slice_state == SIMPLE_INDEX)
    return;

 BadEncoding:
  error (_("Internal error in encoding of renaming declaration: %s"),
	 SYMBOL_LINKAGE_NAME (renaming));
}

static struct block*
block_lookup (struct block *context, char *raw_name)
{
  char *name;
  struct ada_symbol_info *syms;
  int nsyms;
  struct symtab *symtab;

  if (raw_name[0] == '\'')
    {
      raw_name += 1;
      name = raw_name;
    }
  else
    name = ada_encode (raw_name);

  nsyms = ada_lookup_symbol_list (name, context, VAR_DOMAIN, &syms);
  if (context == NULL &&
      (nsyms == 0 || SYMBOL_CLASS (syms[0].sym) != LOC_BLOCK))
    symtab = lookup_symtab (name);
  else
    symtab = NULL;

  if (symtab != NULL)
    return BLOCKVECTOR_BLOCK (BLOCKVECTOR (symtab), STATIC_BLOCK);
  else if (nsyms == 0 || SYMBOL_CLASS (syms[0].sym) != LOC_BLOCK)
    {
      if (context == NULL)
	error (_("No file or function \"%s\"."), raw_name);
      else
	error (_("No function \"%s\" in specified context."), raw_name);
    }
  else
    {
      if (nsyms > 1)
	warning (_("Function name \"%s\" ambiguous here"), raw_name);
      return SYMBOL_BLOCK_VALUE (syms[0].sym);
    }
}

static struct symbol*
select_possible_type_sym (struct ada_symbol_info *syms, int nsyms)
{
  int i;
  int preferred_index;
  struct type *preferred_type;
	  
  preferred_index = -1; preferred_type = NULL;
  for (i = 0; i < nsyms; i += 1)
    switch (SYMBOL_CLASS (syms[i].sym))
      {
      case LOC_TYPEDEF:
	if (ada_prefer_type (SYMBOL_TYPE (syms[i].sym), preferred_type))
	  {
	    preferred_index = i;
	    preferred_type = SYMBOL_TYPE (syms[i].sym);
	  }
	break;
      case LOC_REGISTER:
      case LOC_ARG:
      case LOC_REF_ARG:
      case LOC_REGPARM:
      case LOC_REGPARM_ADDR:
      case LOC_LOCAL:
      case LOC_LOCAL_ARG:
      case LOC_BASEREG:
      case LOC_BASEREG_ARG:
      case LOC_COMPUTED:
      case LOC_COMPUTED_ARG:
	return NULL;
      default:
	break;
      }
  if (preferred_type == NULL)
    return NULL;
  return syms[preferred_index].sym;
}

static struct type*
find_primitive_type (char *name)
{
  struct type *type;
  type = language_lookup_primitive_type_by_name (current_language,
						 current_gdbarch,
						 name);
  if (type == NULL && strcmp ("system__address", name) == 0)
    type = type_system_address ();

  if (type != NULL)
    {
      /* Check to see if we have a regular definition of this
	 type that just didn't happen to have been read yet.  */
      int ntypes;
      struct symbol *sym;
      char *expanded_name = 
	(char *) alloca (strlen (name) + sizeof ("standard__"));
      strcpy (expanded_name, "standard__");
      strcat (expanded_name, name);
      sym = ada_lookup_symbol (expanded_name, NULL, VAR_DOMAIN, NULL, NULL);
      if (sym != NULL && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
	type = SYMBOL_TYPE (sym);
    }

  return type;
}

static int
chop_selector (char *name, int end)
{
  int i;
  for (i = end - 1; i > 0; i -= 1)
    if (name[i] == '.' || (name[i] == '_' && name[i+1] == '_'))
      return i;
  return -1;
}

/* Given that SELS is a string of the form (<sep><identifier>)*, where
   <sep> is '__' or '.', write the indicated sequence of
   STRUCTOP_STRUCT expression operators. */
static void
write_selectors (char *sels)
{
  while (*sels != '\0')
    {
      struct stoken field_name;
      char *p;
      while (*sels == '_' || *sels == '.')
	sels += 1;
      p = sels;
      while (*sels != '\0' && *sels != '.' 
	     && (sels[0] != '_' || sels[1] != '_'))
	sels += 1;
      field_name.length = sels - p;
      field_name.ptr = p;
      write_exp_op_with_string (STRUCTOP_STRUCT, field_name);
    }
}

/* Write a variable access (OP_VAR_VALUE) to ambiguous encoded name
   NAME[0..LEN-1], in block context BLOCK, to be resolved later.  Writes
   a temporary symbol that is valid until the next call to ada_parse.
   */
static void
write_ambiguous_var (struct block *block, char *name, int len)
{
  struct symbol *sym =
    obstack_alloc (&temp_parse_space, sizeof (struct symbol));
  memset (sym, 0, sizeof (struct symbol));
  SYMBOL_DOMAIN (sym) = UNDEF_DOMAIN;
  SYMBOL_LINKAGE_NAME (sym) = obsavestring (name, len, &temp_parse_space);
  SYMBOL_LANGUAGE (sym) = language_ada;

  write_exp_elt_opcode (OP_VAR_VALUE);
  write_exp_elt_block (block);
  write_exp_elt_sym (sym);
  write_exp_elt_opcode (OP_VAR_VALUE);
}


/* Look up NAME0 (an unencoded identifier or dotted name) in BLOCK (or 
   expression_block_context if NULL).  If it denotes a type, return
   that type.  Otherwise, write expression code to evaluate it as an
   object and return NULL. In this second case, NAME0 will, in general,
   have the form <name>(.<selector_name>)*, where <name> is an object
   or renaming encoded in the debugging data.  Calls error if no
   prefix <name> matches a name in the debugging data (i.e., matches
   either a complete name or, as a wild-card match, the final 
   identifier).  */

static struct type*
write_var_or_type (struct block *block, struct stoken name0)
{
  int depth;
  char *encoded_name;
  int name_len;

  if (block == NULL)
    block = expression_context_block;

  encoded_name = ada_encode (name0.ptr);
  name_len = strlen (encoded_name);
  encoded_name = obsavestring (encoded_name, name_len, &temp_parse_space);
  for (depth = 0; depth < MAX_RENAMING_CHAIN_LENGTH; depth += 1)
    {
      int tail_index;
      
      tail_index = name_len;
      while (tail_index > 0)
	{
	  int nsyms;
	  struct ada_symbol_info *syms;
	  struct symbol *type_sym;
	  int terminator = encoded_name[tail_index];

	  encoded_name[tail_index] = '\0';
	  nsyms = ada_lookup_symbol_list (encoded_name, block,
					  VAR_DOMAIN, &syms);
	  encoded_name[tail_index] = terminator;

	  /* A single symbol may rename a package or object. */

	  if (nsyms == 1 && !ada_is_object_renaming (syms[0].sym))
	    {
	      struct symbol *renaming_sym =
		ada_find_renaming_symbol (SYMBOL_LINKAGE_NAME (syms[0].sym), 
					  syms[0].block);

	      if (renaming_sym != NULL)
		syms[0].sym = renaming_sym;
	    }

	  type_sym = select_possible_type_sym (syms, nsyms);
	  if (type_sym != NULL)
	    {
	      struct type *type = SYMBOL_TYPE (type_sym);

	      if (TYPE_CODE (type) == TYPE_CODE_VOID)
		error (_("`%s' matches only void type name(s)"), name0.ptr);
	      else if (ada_is_object_renaming (type_sym))
		{
		  write_object_renaming (block, type_sym, 
					 MAX_RENAMING_CHAIN_LENGTH);
		  write_selectors (encoded_name + tail_index);
		  return NULL;
		}
	      else if (ada_renaming_type (SYMBOL_TYPE (type_sym)) != NULL)
		{
		  int result;
		  char *renaming = ada_simple_renamed_entity (type_sym);
		  int renaming_len = strlen (renaming);

		  char *new_name
		    = obstack_alloc (&temp_parse_space,
				     renaming_len + name_len - tail_index 
				     + 1);
		  strcpy (new_name, renaming);
		  xfree (renaming);
		  strcpy (new_name + renaming_len, encoded_name + tail_index);
		  encoded_name = new_name;
		  name_len = renaming_len + name_len - tail_index;
		  goto TryAfterRenaming;
		}
	      else if (tail_index == name_len)
		return type;
	      else 
		error (_("Invalid attempt to select from type: \"%s\"."), name0.ptr);
	    }
	  else if (tail_index == name_len && nsyms == 0)
	    {
	      struct type *type = find_primitive_type (encoded_name);

	      if (type != NULL)
		return type;
	    }

	  if (nsyms == 1)
	    {
	      write_var_from_sym (block, syms[0].block, syms[0].sym);
	      write_selectors (encoded_name + tail_index);
	      return NULL;
	    }
	  else if (nsyms == 0) 
	    {
	      int i;
	      struct minimal_symbol *msym 
		= ada_lookup_simple_minsym (encoded_name);
	      if (msym != NULL)
		{
		  write_exp_msymbol (msym, lookup_function_type (type_int ()),
				     type_int ());
		  /* Maybe cause error here rather than later? FIXME? */
		  write_selectors (encoded_name + tail_index);
		  return NULL;
		}

	      if (tail_index == name_len
		  && strncmp (encoded_name, "standard__", 
			      sizeof ("standard__") - 1) == 0)
		error (_("No definition of \"%s\" found."), name0.ptr);

	      tail_index = chop_selector (encoded_name, tail_index);
	    } 
	  else
	    {
	      write_ambiguous_var (block, encoded_name, tail_index);
	      write_selectors (encoded_name + tail_index);
	      return NULL;
	    }
	}

      if (!have_full_symbols () && !have_partial_symbols () && block == NULL)
	error (_("No symbol table is loaded.  Use the \"file\" command."));
      if (block == expression_context_block)
	error (_("No definition of \"%s\" in current context."), name0.ptr);
      else
	error (_("No definition of \"%s\" in specified context."), name0.ptr);
      
    TryAfterRenaming: ;
    }

  error (_("Could not find renamed symbol \"%s\""), name0.ptr);

}

/* Write a left side of a component association (e.g., NAME in NAME =>
   exp).  If NAME has the form of a selected component, write it as an
   ordinary expression.  If it is a simple variable that unambiguously
   corresponds to exactly one symbol that does not denote a type or an
   object renaming, also write it normally as an OP_VAR_VALUE.
   Otherwise, write it as an OP_NAME.

   Unfortunately, we don't know at this point whether NAME is supposed
   to denote a record component name or the value of an array index.
   Therefore, it is not appropriate to disambiguate an ambiguous name
   as we normally would, nor to replace a renaming with its referent.
   As a result, in the (one hopes) rare case that one writes an
   aggregate such as (R => 42) where R renames an object or is an
   ambiguous name, one must write instead ((R) => 42). */
   
static void
write_name_assoc (struct stoken name)
{
  if (strchr (name.ptr, '.') == NULL)
    {
      struct ada_symbol_info *syms;
      int nsyms = ada_lookup_symbol_list (name.ptr, expression_context_block,
					  VAR_DOMAIN, &syms);
      if (nsyms != 1 || SYMBOL_CLASS (syms[0].sym) == LOC_TYPEDEF)
	write_exp_op_with_string (OP_NAME, name);
      else
	write_var_from_sym (NULL, syms[0].block, syms[0].sym);
    }
  else
    if (write_var_or_type (NULL, name) != NULL)
      error (_("Invalid use of type."));
}

/* Convert the character literal whose ASCII value would be VAL to the
   appropriate value of type TYPE, if there is a translation.
   Otherwise return VAL.  Hence, in an enumeration type ('A', 'B'),
   the literal 'A' (VAL == 65), returns 0.  */

static LONGEST
convert_char_literal (struct type *type, LONGEST val)
{
  char name[7];
  int f;

  if (type == NULL || TYPE_CODE (type) != TYPE_CODE_ENUM)
    return val;
  sprintf (name, "QU%02x", (int) val);
  for (f = 0; f < TYPE_NFIELDS (type); f += 1)
    {
      if (strcmp (name, TYPE_FIELD_NAME (type, f)) == 0)
	return TYPE_FIELD_BITPOS (type, f);
    }
  return val;
}

static struct type *
type_int (void)
{
  return builtin_type (current_gdbarch)->builtin_int;
}

static struct type *
type_long (void)
{
  return builtin_type (current_gdbarch)->builtin_long;
}

static struct type *
type_long_long (void)
{
  return builtin_type (current_gdbarch)->builtin_long_long;
}

static struct type *
type_float (void)
{
  return builtin_type (current_gdbarch)->builtin_float;
}

static struct type *
type_double (void)
{
  return builtin_type (current_gdbarch)->builtin_double;
}

static struct type *
type_long_double (void)
{
  return builtin_type (current_gdbarch)->builtin_long_double;
}

static struct type *
type_char (void)
{
  return language_string_char_type (current_language, current_gdbarch);
}

static struct type *
type_system_address (void)
{
  struct type *type 
    = language_lookup_primitive_type_by_name (current_language,
					      current_gdbarch, 
					      "system__address");
  return  type != NULL ? type : lookup_pointer_type (builtin_type_void);
}

void
_initialize_ada_exp (void)
{
  obstack_init (&temp_parse_space);
}

/* FIXME: hilfingr/2004-10-05: Hack to remove warning.  The function
   string_to_operator is supposed to be used for cases where one
   calls an operator function with prefix notation, as in 
   "+" (a, b), but at some point, this code seems to have gone
   missing. */

struct stoken (*dummy_string_to_ada_operator) (struct stoken) 
     = string_to_operator;

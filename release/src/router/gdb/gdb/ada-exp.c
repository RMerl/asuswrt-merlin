/* A Bison parser, made by GNU Bison 1.875c.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     INT = 258,
     NULL_PTR = 259,
     CHARLIT = 260,
     FLOAT = 261,
     COLONCOLON = 262,
     STRING = 263,
     NAME = 264,
     DOT_ID = 265,
     DOT_ALL = 266,
     SPECIAL_VARIABLE = 267,
     ASSIGN = 268,
     ELSE = 269,
     THEN = 270,
     XOR = 271,
     OR = 272,
     _AND_ = 273,
     DOTDOT = 274,
     IN = 275,
     GEQ = 276,
     LEQ = 277,
     NOTEQUAL = 278,
     UNARY = 279,
     REM = 280,
     MOD = 281,
     NOT = 282,
     ABS = 283,
     STARSTAR = 284,
     VAR = 285,
     ARROW = 286,
     TICK_LENGTH = 287,
     TICK_LAST = 288,
     TICK_FIRST = 289,
     TICK_ADDRESS = 290,
     TICK_ACCESS = 291,
     TICK_MODULUS = 292,
     TICK_MIN = 293,
     TICK_MAX = 294,
     TICK_VAL = 295,
     TICK_TAG = 296,
     TICK_SIZE = 297,
     TICK_RANGE = 298,
     TICK_POS = 299,
     NEW = 300,
     OTHERS = 301
   };
#endif
#define INT 258
#define NULL_PTR 259
#define CHARLIT 260
#define FLOAT 261
#define COLONCOLON 262
#define STRING 263
#define NAME 264
#define DOT_ID 265
#define DOT_ALL 266
#define SPECIAL_VARIABLE 267
#define ASSIGN 268
#define ELSE 269
#define THEN 270
#define XOR 271
#define OR 272
#define _AND_ 273
#define DOTDOT 274
#define IN 275
#define GEQ 276
#define LEQ 277
#define NOTEQUAL 278
#define UNARY 279
#define REM 280
#define MOD 281
#define NOT 282
#define ABS 283
#define STARSTAR 284
#define VAR 285
#define ARROW 286
#define TICK_LENGTH 287
#define TICK_LAST 288
#define TICK_FIRST 289
#define TICK_ADDRESS 290
#define TICK_ACCESS 291
#define TICK_MODULUS 292
#define TICK_MIN 293
#define TICK_MAX 294
#define TICK_VAL 295
#define TICK_TAG 296
#define TICK_SIZE 297
#define TICK_RANGE 298
#define TICK_POS 299
#define NEW 300
#define OTHERS 301




/* Copy the first part of user declarations.  */
#line 39 "ada-exp.y"


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



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 160 "ada-exp.y"
typedef union YYSTYPE {
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
  } YYSTYPE;
/* Line 191 of yacc.c.  */
#line 304 "ada-exp.c.tmp"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 316 "ada-exp.c.tmp"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

# ifndef YYFREE
#  define YYFREE free
# endif
# ifndef YYMALLOC
#  define YYMALLOC xmalloc
# endif

/* The parser invokes alloca or xmalloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   define YYSTACK_ALLOC alloca
#  endif
# else
#  if defined (alloca) || defined (_ALLOCA_H)
#   define YYSTACK_ALLOC alloca
#  else
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  55
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   705

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  67
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  31
/* YYNRULES -- Number of rules. */
#define YYNRULES  120
/* YYNRULES -- Number of states. */
#define YYNSTATES  231

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   301

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    30,    62,
      56,    61,    32,    28,    63,    29,    55,    33,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    60,
      20,    19,    21,     2,    27,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    57,     2,    66,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    64,    40,    65,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    22,    23,    24,    25,    26,    31,
      34,    35,    36,    37,    38,    39,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      58,    59
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short yyprhs[] =
{
       0,     0,     3,     5,     7,    11,    15,    18,    21,    26,
      31,    32,    40,    41,    48,    55,    59,    61,    63,    65,
      67,    70,    73,    76,    79,    80,    82,    86,    90,    96,
     101,   105,   109,   113,   117,   121,   125,   129,   133,   137,
     139,   143,   147,   151,   157,   163,   167,   174,   181,   186,
     190,   194,   198,   200,   202,   204,   206,   208,   210,   214,
     218,   223,   228,   232,   236,   241,   246,   250,   254,   257,
     260,   264,   268,   272,   275,   278,   286,   294,   300,   306,
     309,   310,   314,   316,   318,   319,   321,   323,   325,   327,
     329,   332,   334,   337,   340,   344,   347,   351,   355,   357,
     360,   363,   366,   370,   372,   374,   378,   382,   384,   385,
     390,   394,   395,   402,   403,   408,   412,   413,   420,   423,
     426
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      68,     0,    -1,    69,    -1,    76,    -1,    69,    60,    76,
      -1,    70,    13,    76,    -1,    70,    11,    -1,    70,    10,
      -1,    70,    56,    74,    61,    -1,    85,    56,    74,    61,
      -1,    -1,    85,    62,    72,    71,    56,    76,    61,    -1,
      -1,    70,    56,    73,    22,    73,    61,    -1,    85,    56,
      73,    22,    73,    61,    -1,    56,    69,    61,    -1,    85,
      -1,    12,    -1,    87,    -1,    70,    -1,    29,    73,    -1,
      28,    73,    -1,    36,    73,    -1,    37,    73,    -1,    -1,
      76,    -1,     9,    41,    76,    -1,    74,    63,    76,    -1,
      74,    63,     9,    41,    76,    -1,    64,    85,    65,    73,
      -1,    73,    38,    73,    -1,    73,    32,    73,    -1,    73,
      33,    73,    -1,    73,    34,    73,    -1,    73,    35,    73,
      -1,    73,    27,    73,    -1,    73,    28,    73,    -1,    73,
      30,    73,    -1,    73,    29,    73,    -1,    73,    -1,    73,
      19,    73,    -1,    73,    26,    73,    -1,    73,    25,    73,
      -1,    73,    23,    73,    22,    73,    -1,    73,    23,    70,
      53,    82,    -1,    73,    23,    85,    -1,    73,    36,    23,
      73,    22,    73,    -1,    73,    36,    23,    70,    53,    82,
      -1,    73,    36,    23,    85,    -1,    73,    24,    73,    -1,
      73,    20,    73,    -1,    73,    21,    73,    -1,    75,    -1,
      77,    -1,    78,    -1,    79,    -1,    80,    -1,    81,    -1,
      75,    18,    75,    -1,    77,    18,    75,    -1,    75,    18,
      15,    75,    -1,    78,    18,    15,    75,    -1,    75,    17,
      75,    -1,    79,    17,    75,    -1,    75,    17,    14,    75,
      -1,    80,    17,    14,    75,    -1,    75,    16,    75,    -1,
      81,    16,    75,    -1,    70,    46,    -1,    70,    45,    -1,
      70,    44,    82,    -1,    70,    43,    82,    -1,    70,    42,
      82,    -1,    70,    52,    -1,    70,    51,    -1,    84,    48,
      56,    76,    63,    76,    61,    -1,    84,    49,    56,    76,
      63,    76,    61,    -1,    84,    54,    56,    76,    61,    -1,
      83,    50,    56,    76,    61,    -1,    83,    47,    -1,    -1,
      56,     3,    61,    -1,    85,    -1,    83,    -1,    -1,     3,
      -1,     5,    -1,     6,    -1,     4,    -1,     8,    -1,    58,
       9,    -1,     9,    -1,    86,     9,    -1,     9,    46,    -1,
      86,     9,    46,    -1,     9,     7,    -1,    86,     9,     7,
      -1,    56,    88,    61,    -1,    90,    -1,    89,    76,    -1,
      89,    90,    -1,    76,    63,    -1,    89,    76,    63,    -1,
      91,    -1,    92,    -1,    92,    63,    90,    -1,    59,    41,
      76,    -1,    93,    -1,    -1,     9,    41,    94,    76,    -1,
      73,    41,    76,    -1,    -1,    73,    22,    73,    41,    95,
      76,    -1,    -1,     9,    40,    96,    93,    -1,    73,    40,
      93,    -1,    -1,    73,    22,    73,    40,    97,    93,    -1,
      32,    70,    -1,    30,    70,    -1,    70,    57,    76,    66,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   224,   224,   228,   229,   231,   236,   240,   244,   250,
     269,   269,   281,   285,   287,   295,   306,   316,   320,   323,
     326,   330,   334,   338,   342,   345,   347,   349,   351,   355,
     368,   372,   376,   380,   384,   388,   392,   396,   400,   404,
     407,   411,   415,   419,   421,   426,   434,   438,   444,   455,
     459,   463,   467,   468,   469,   470,   471,   472,   476,   478,
     483,   485,   490,   492,   497,   499,   503,   505,   517,   519,
     525,   528,   531,   534,   536,   538,   540,   542,   544,   546,
     550,   552,   557,   567,   569,   575,   579,   586,   594,   598,
     604,   608,   610,   612,   620,   631,   633,   638,   647,   648,
     654,   659,   665,   674,   675,   676,   680,   685,   700,   699,
     702,   705,   704,   710,   709,   712,   715,   714,   722,   724,
     726
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "INT", "NULL_PTR", "CHARLIT", "FLOAT",
  "COLONCOLON", "STRING", "NAME", "DOT_ID", "DOT_ALL", "SPECIAL_VARIABLE",
  "ASSIGN", "ELSE", "THEN", "XOR", "OR", "_AND_", "'='", "'<'", "'>'",
  "DOTDOT", "IN", "GEQ", "LEQ", "NOTEQUAL", "'@'", "'+'", "'-'", "'&'",
  "UNARY", "'*'", "'/'", "REM", "MOD", "NOT", "ABS", "STARSTAR", "VAR",
  "'|'", "ARROW", "TICK_LENGTH", "TICK_LAST", "TICK_FIRST", "TICK_ADDRESS",
  "TICK_ACCESS", "TICK_MODULUS", "TICK_MIN", "TICK_MAX", "TICK_VAL",
  "TICK_TAG", "TICK_SIZE", "TICK_RANGE", "TICK_POS", "'.'", "'('", "'['",
  "NEW", "OTHERS", "';'", "')'", "'''", "','", "'{'", "'}'", "']'",
  "$accept", "start", "exp1", "primary", "@1", "save_qualifier",
  "simple_exp", "arglist", "relation", "exp", "and_exp", "and_then_exp",
  "or_exp", "or_else_exp", "xor_exp", "tick_arglist", "type_prefix",
  "opt_type_prefix", "var_or_type", "block", "aggregate",
  "aggregate_component_list", "positional_list", "component_groups",
  "others", "component_group", "component_associations", "@2", "@3", "@4",
  "@5", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,    61,
      60,    62,   274,   275,   276,   277,   278,    64,    43,    45,
      38,   279,    42,    47,   280,   281,   282,   283,   284,   285,
     124,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,    46,    40,    91,   300,   301,
      59,    41,    39,    44,   123,   125,    93
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    67,    68,    69,    69,    69,    70,    70,    70,    70,
      71,    70,    72,    70,    70,    70,    70,    70,    70,    73,
      73,    73,    73,    73,    74,    74,    74,    74,    74,    73,
      73,    73,    73,    73,    73,    73,    73,    73,    73,    75,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      75,    75,    76,    76,    76,    76,    76,    76,    77,    77,
      78,    78,    79,    79,    80,    80,    81,    81,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      82,    82,    83,    84,    84,    70,    70,    70,    70,    70,
      70,    85,    85,    85,    85,    86,    86,    87,    88,    88,
      88,    89,    89,    90,    90,    90,    91,    92,    94,    93,
      93,    95,    93,    96,    93,    93,    97,    93,    70,    70,
      70
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     3,     3,     2,     2,     4,     4,
       0,     7,     0,     6,     6,     3,     1,     1,     1,     1,
       2,     2,     2,     2,     0,     1,     3,     3,     5,     4,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     1,
       3,     3,     3,     5,     5,     3,     6,     6,     4,     3,
       3,     3,     1,     1,     1,     1,     1,     1,     3,     3,
       4,     4,     3,     3,     4,     4,     3,     3,     2,     2,
       3,     3,     3,     2,     2,     7,     7,     5,     5,     2,
       0,     3,     1,     1,     0,     1,     1,     1,     1,     1,
       2,     1,     2,     2,     3,     2,     3,     3,     1,     2,
       2,     2,     3,     1,     1,     3,     3,     1,     0,     4,
       3,     0,     6,     0,     4,     3,     0,     6,     2,     2,
       4
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
      84,    85,    88,    86,    87,    89,    91,    17,    84,    84,
      84,    84,    84,    84,    84,     0,     0,     0,     2,    19,
      39,    52,     3,    53,    54,    55,    56,    57,    83,     0,
      16,     0,    18,    95,    93,    19,    21,    20,   119,   118,
      22,    23,    91,     0,     0,    39,     3,     0,    84,    98,
     103,   104,   107,    90,     0,     1,    84,     7,     6,    84,
      80,    80,    80,    69,    68,    74,    73,    84,    84,    84,
      84,    84,    84,    84,    84,    84,    84,    84,    84,    84,
      84,    84,    84,    84,     0,    84,    84,    84,    84,    84,
       0,    84,     0,    84,    79,     0,     0,     0,     0,    84,
      12,    92,   113,   108,    84,    15,    84,    84,    84,   101,
      97,    99,   100,    84,    84,     4,     5,     0,    72,    71,
      70,    91,    39,     0,    25,     0,    40,    50,    51,    19,
       0,    16,    49,    42,    41,    35,    36,    38,    37,    31,
      32,    33,    34,    84,    30,    66,    84,    62,    84,    58,
      59,    84,    63,    84,    67,    84,    84,    84,    84,    39,
       0,    10,    96,    94,    84,    84,   106,     0,     0,   115,
     110,   102,   105,    29,     0,    84,    84,     8,    84,   120,
      80,    84,    19,     0,    16,    64,    60,    61,    65,     0,
       0,     0,     0,    84,     9,     0,   114,   109,   116,   111,
      81,    26,     0,    91,    27,    44,    43,    80,    84,    78,
      84,    84,    77,     0,    84,    84,    84,    13,    84,    47,
      46,     0,     0,    14,     0,   117,   112,    28,    75,    76,
      11
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short yydefgoto[] =
{
      -1,    17,    18,    35,   195,   161,    20,   123,    21,   124,
      23,    24,    25,    26,    27,   118,    28,    29,    30,    31,
      32,    47,    48,    49,    50,    51,    52,   165,   216,   164,
     215
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -104
static const short yypact[] =
{
     396,  -104,  -104,  -104,  -104,  -104,    16,  -104,   396,   396,
     121,   121,   396,   396,   282,     2,     6,     9,   -28,   150,
     629,    89,  -104,    20,    24,    33,    41,    35,    43,    18,
     275,    55,  -104,  -104,  -104,   513,    -7,    -7,    -3,    -3,
      -7,    -7,     3,    28,   -14,   566,    25,    31,   282,  -104,
    -104,    37,  -104,  -104,    44,  -104,   396,  -104,  -104,   396,
      42,    42,    42,  -104,  -104,  -104,  -104,   272,   396,   396,
     396,   396,   396,   396,   396,   396,   396,   396,   396,   396,
     396,   396,   396,   396,    79,   396,   396,   339,   351,   396,
      50,   396,    99,   396,  -104,    58,    59,    60,    63,   272,
    -104,    17,  -104,  -104,   396,  -104,   396,   408,   396,  -104,
    -104,    57,  -104,   282,   396,  -104,  -104,   119,  -104,  -104,
    -104,    -1,   589,   -41,  -104,    65,   656,   656,   656,   476,
     215,   173,   656,   656,   656,   667,    -7,    -7,    -7,    85,
      85,    85,    85,   396,    85,  -104,   396,  -104,   396,  -104,
    -104,   396,  -104,   396,  -104,   396,   396,   396,   396,   609,
      34,  -104,  -104,  -104,   408,   396,  -104,   641,   466,  -104,
    -104,  -104,  -104,  -104,    71,   396,   396,  -104,   453,  -104,
      42,   396,   492,   441,   208,  -104,  -104,  -104,  -104,    73,
      76,    81,    84,   396,  -104,    93,  -104,  -104,  -104,  -104,
    -104,  -104,   108,    14,  -104,  -104,   656,    42,   396,  -104,
     396,   396,  -104,   544,   396,   408,   396,  -104,   396,  -104,
     656,    91,    98,  -104,   103,  -104,  -104,  -104,  -104,  -104,
    -104
};

/* YYPGOTO[NTERM-NUM].  */
static const short yypgoto[] =
{
    -104,  -104,   136,    19,  -104,  -104,     4,    67,   -52,     0,
    -104,  -104,  -104,  -104,  -104,   -59,  -104,  -104,   -15,  -104,
    -104,  -104,  -104,   -43,  -104,  -104,  -103,  -104,  -104,  -104,
    -104
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -83
static const short yytable[] =
{
      22,    54,   119,   120,   169,   112,    33,    57,    58,    55,
      33,    53,    36,    37,    46,     6,    40,    41,    45,    19,
     177,    33,   178,    33,   162,    80,    81,    82,    83,    38,
      39,    85,    56,    19,   145,   147,   149,   150,    89,   152,
     175,   154,    90,   102,   103,    34,    56,   105,   111,    34,
      91,    93,    45,    67,    68,   218,   115,   131,    92,   116,
      34,   196,    34,   163,   101,   151,    96,    97,   125,   104,
     172,   122,    98,   126,   127,   128,   130,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   109,   144,
      94,   129,   110,    95,   185,   194,   186,   178,   117,   187,
     113,   188,   143,   159,   166,    86,    87,    88,   170,   114,
     167,   168,   225,   153,   155,   156,   157,   168,   173,   158,
     171,   205,   174,    85,     1,     2,     3,     4,   184,     5,
       6,   179,   200,     7,   209,    76,    77,    78,    79,   210,
      80,    81,    82,    83,   211,   212,    85,   183,   219,   214,
      44,    10,   228,    11,     0,   189,   190,   191,   192,   229,
      57,    58,   182,    59,   230,   197,   160,     0,   168,   217,
       0,     0,     0,   -45,     0,   201,     0,    14,   204,    15,
     202,     0,     0,     0,     0,   206,     0,     0,     0,   -45,
     -45,   -45,    60,    61,    62,    63,    64,   213,     0,     0,
       0,    65,    66,     0,     0,     0,    67,    68,   -48,     0,
     221,   222,   220,     0,   224,     0,   226,     0,   227,   168,
     -82,   -82,   -82,   -82,   -48,   -48,   -48,   -82,     0,    99,
       0,     0,     0,   -45,   -45,   100,   -45,   181,     0,   -45,
       0,     0,    76,    77,    78,    79,     0,    80,    81,    82,
      83,     0,     0,    85,     0,   -82,   -82,   -82,   -82,     0,
       0,     0,   -82,     0,    99,     0,     0,     0,   -48,   -48,
     100,   -48,     0,     0,   -48,     1,     2,     3,     4,     0,
       5,   121,     0,     0,     7,     1,     2,     3,     4,     0,
       5,    42,     0,     0,     7,     0,     0,     0,     0,     0,
       8,     9,    10,     0,    11,     0,     0,     0,    12,    13,
       8,     9,    10,     0,    11,     0,     0,     0,    12,    13,
       0,     0,   -82,   -82,   -82,   -82,     0,     0,    14,   -82,
      15,    99,     0,   -24,     0,   -24,    16,   100,    14,     0,
      15,    43,     1,     2,     3,     4,    16,     5,     6,     0,
       0,     7,     0,   146,     1,     2,     3,     4,     0,     5,
       6,     0,     0,     7,     0,     0,   148,     8,     9,    10,
       0,    11,     0,     0,     0,    12,    13,     0,     0,     8,
       9,    10,     0,    11,     0,     0,     0,    12,    13,     0,
       0,     0,     0,     0,     0,    14,     0,    15,     0,     1,
       2,     3,     4,    16,     5,     6,     0,    14,     7,    15,
       0,     1,     2,     3,     4,    16,     5,    42,     0,     0,
       7,     0,     0,     0,     8,     9,    10,     0,    11,     0,
       0,     0,    12,    13,     0,     0,     8,     9,    10,     0,
      11,     0,     0,     0,    12,    13,     0,     0,     0,     0,
       0,     0,    14,     0,    15,     0,     1,     2,     3,     4,
      16,     5,   203,   208,    14,     7,    15,     0,    76,    77,
      78,    79,    16,    80,    81,    82,    83,     0,     0,    85,
       0,     8,     9,    10,     0,    11,    57,    58,   106,    12,
      13,     0,     0,    76,    77,    78,    79,     0,    80,    81,
      82,    83,    57,    58,    85,     0,   107,   108,     0,    14,
       0,    15,     0,     0,     0,     0,     0,    16,    60,    61,
      62,    63,    64,    57,    58,     0,     0,    65,    66,   180,
       0,     0,    67,    68,    60,    61,    62,    63,    64,     0,
       0,     0,     0,    65,    66,   207,     0,     0,    67,    68,
       0,     0,     0,     0,     0,    60,    61,    62,    63,    64,
       0,     0,     0,     0,    65,    66,     0,     0,     0,    67,
      68,    76,    77,    78,    79,     0,    80,    81,    82,    83,
       0,     0,    85,     0,     0,    69,    70,    71,   106,    72,
      73,    74,    75,    76,    77,    78,    79,     0,    80,    81,
      82,    83,    84,     0,    85,   223,   107,   108,    69,    70,
      71,   176,    72,    73,    74,    75,    76,    77,    78,    79,
       0,    80,    81,    82,    83,    84,     0,    85,    69,    70,
      71,   193,    72,    73,    74,    75,    76,    77,    78,    79,
       0,    80,    81,    82,    83,    84,     0,    85,    69,    70,
      71,     0,    72,    73,    74,    75,    76,    77,    78,    79,
       0,    80,    81,    82,    83,    84,     0,    85,    76,    77,
      78,    79,     0,    80,    81,    82,    83,     0,     0,    85,
       0,   198,   199,    76,    77,    78,    79,     0,    80,    81,
      82,    83,     0,     0,    85,    77,    78,    79,     0,    80,
      81,    82,    83,     0,     0,    85
};

static const short yycheck[] =
{
       0,    16,    61,    62,   107,    48,     7,    10,    11,     0,
       7,     9,     8,     9,    14,     9,    12,    13,    14,     0,
      61,     7,    63,     7,     7,    32,    33,    34,    35,    10,
      11,    38,    60,    14,    86,    87,    88,    89,    18,    91,
      41,    93,    18,    40,    41,    46,    60,    61,    48,    46,
      17,    16,    48,    56,    57,    41,    56,    72,    17,    59,
      46,   164,    46,    46,     9,    15,    48,    49,    68,    41,
     113,    67,    54,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    63,    85,
      47,    72,    61,    50,   146,    61,   148,    63,    56,   151,
      63,   153,    23,    99,   104,    16,    17,    18,   108,    65,
     106,   107,   215,    14,    56,    56,    56,   113,   114,    56,
      63,   180,     3,    38,     3,     4,     5,     6,   143,     8,
       9,    66,    61,    12,    61,    27,    28,    29,    30,    63,
      32,    33,    34,    35,    63,    61,    38,   143,   207,    56,
      14,    30,    61,    32,    -1,   155,   156,   157,   158,    61,
      10,    11,   143,    13,    61,   165,    99,    -1,   164,    61,
      -1,    -1,    -1,     0,    -1,   175,    -1,    56,   178,    58,
     176,    -1,    -1,    -1,    -1,   181,    -1,    -1,    -1,    16,
      17,    18,    42,    43,    44,    45,    46,   193,    -1,    -1,
      -1,    51,    52,    -1,    -1,    -1,    56,    57,     0,    -1,
     210,   211,   208,    -1,   214,    -1,   216,    -1,   218,   215,
      47,    48,    49,    50,    16,    17,    18,    54,    -1,    56,
      -1,    -1,    -1,    60,    61,    62,    63,    22,    -1,    66,
      -1,    -1,    27,    28,    29,    30,    -1,    32,    33,    34,
      35,    -1,    -1,    38,    -1,    47,    48,    49,    50,    -1,
      -1,    -1,    54,    -1,    56,    -1,    -1,    -1,    60,    61,
      62,    63,    -1,    -1,    66,     3,     4,     5,     6,    -1,
       8,     9,    -1,    -1,    12,     3,     4,     5,     6,    -1,
       8,     9,    -1,    -1,    12,    -1,    -1,    -1,    -1,    -1,
      28,    29,    30,    -1,    32,    -1,    -1,    -1,    36,    37,
      28,    29,    30,    -1,    32,    -1,    -1,    -1,    36,    37,
      -1,    -1,    47,    48,    49,    50,    -1,    -1,    56,    54,
      58,    56,    -1,    61,    -1,    63,    64,    62,    56,    -1,
      58,    59,     3,     4,     5,     6,    64,     8,     9,    -1,
      -1,    12,    -1,    14,     3,     4,     5,     6,    -1,     8,
       9,    -1,    -1,    12,    -1,    -1,    15,    28,    29,    30,
      -1,    32,    -1,    -1,    -1,    36,    37,    -1,    -1,    28,
      29,    30,    -1,    32,    -1,    -1,    -1,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    56,    -1,    58,    -1,     3,
       4,     5,     6,    64,     8,     9,    -1,    56,    12,    58,
      -1,     3,     4,     5,     6,    64,     8,     9,    -1,    -1,
      12,    -1,    -1,    -1,    28,    29,    30,    -1,    32,    -1,
      -1,    -1,    36,    37,    -1,    -1,    28,    29,    30,    -1,
      32,    -1,    -1,    -1,    36,    37,    -1,    -1,    -1,    -1,
      -1,    -1,    56,    -1,    58,    -1,     3,     4,     5,     6,
      64,     8,     9,    22,    56,    12,    58,    -1,    27,    28,
      29,    30,    64,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    28,    29,    30,    -1,    32,    10,    11,    22,    36,
      37,    -1,    -1,    27,    28,    29,    30,    -1,    32,    33,
      34,    35,    10,    11,    38,    -1,    40,    41,    -1,    56,
      -1,    58,    -1,    -1,    -1,    -1,    -1,    64,    42,    43,
      44,    45,    46,    10,    11,    -1,    -1,    51,    52,    53,
      -1,    -1,    56,    57,    42,    43,    44,    45,    46,    -1,
      -1,    -1,    -1,    51,    52,    53,    -1,    -1,    56,    57,
      -1,    -1,    -1,    -1,    -1,    42,    43,    44,    45,    46,
      -1,    -1,    -1,    -1,    51,    52,    -1,    -1,    -1,    56,
      57,    27,    28,    29,    30,    -1,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    -1,    32,    33,
      34,    35,    36,    -1,    38,    61,    40,    41,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      -1,    32,    33,    34,    35,    36,    -1,    38,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      -1,    32,    33,    34,    35,    36,    -1,    38,    19,    20,
      21,    -1,    23,    24,    25,    26,    27,    28,    29,    30,
      -1,    32,    33,    34,    35,    36,    -1,    38,    27,    28,
      29,    30,    -1,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    40,    41,    27,    28,    29,    30,    -1,    32,    33,
      34,    35,    -1,    -1,    38,    28,    29,    30,    -1,    32,
      33,    34,    35,    -1,    -1,    38
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     3,     4,     5,     6,     8,     9,    12,    28,    29,
      30,    32,    36,    37,    56,    58,    64,    68,    69,    70,
      73,    75,    76,    77,    78,    79,    80,    81,    83,    84,
      85,    86,    87,     7,    46,    70,    73,    73,    70,    70,
      73,    73,     9,    59,    69,    73,    76,    88,    89,    90,
      91,    92,    93,     9,    85,     0,    60,    10,    11,    13,
      42,    43,    44,    45,    46,    51,    52,    56,    57,    19,
      20,    21,    23,    24,    25,    26,    27,    28,    29,    30,
      32,    33,    34,    35,    36,    38,    16,    17,    18,    18,
      18,    17,    17,    16,    47,    50,    48,    49,    54,    56,
      62,     9,    40,    41,    41,    61,    22,    40,    41,    63,
      61,    76,    90,    63,    65,    76,    76,    56,    82,    82,
      82,     9,    73,    74,    76,    76,    73,    73,    73,    70,
      73,    85,    73,    73,    73,    73,    73,    73,    73,    73,
      73,    73,    73,    23,    73,    75,    14,    75,    15,    75,
      75,    15,    75,    14,    75,    56,    56,    56,    56,    73,
      74,    72,     7,    46,    96,    94,    76,    73,    73,    93,
      76,    63,    90,    73,     3,    41,    22,    61,    63,    66,
      53,    22,    70,    73,    85,    75,    75,    75,    75,    76,
      76,    76,    76,    22,    61,    71,    93,    76,    40,    41,
      61,    76,    73,     9,    76,    82,    73,    53,    22,    61,
      63,    63,    61,    73,    56,    97,    95,    61,    41,    82,
      73,    76,    76,    61,    76,    93,    76,    76,    61,    61,
      61
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)		\
   ((Current).first_line   = (Rhs)[1].first_line,	\
    (Current).first_column = (Rhs)[1].first_column,	\
    (Current).last_line    = (Rhs)[N].last_line,	\
    (Current).last_column  = (Rhs)[N].last_column)
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short *bottom, short *top)
#else
static void
yy_stack_print (bottom, top)
    short *bottom;
    short *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if defined (YYMAXDEPTH) && YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to xreallocate them elsewhere.  */

  /* The state stack.  */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to xreallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 4:
#line 230 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_COMMA); }
    break;

  case 5:
#line 232 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_ASSIGN); }
    break;

  case 6:
#line 237 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_IND); }
    break;

  case 7:
#line 241 "ada-exp.y"
    { write_exp_op_with_string (STRUCTOP_STRUCT, yyvsp[0].sval); }
    break;

  case 8:
#line 245 "ada-exp.y"
    {
			  write_exp_elt_opcode (OP_FUNCALL);
			  write_exp_elt_longcst (yyvsp[-1].lval);
			  write_exp_elt_opcode (OP_FUNCALL);
		        }
    break;

  case 9:
#line 251 "ada-exp.y"
    {
			  if (yyvsp[-3].tval != NULL)
			    {
			      if (yyvsp[-1].lval != 1)
				error (_("Invalid conversion"));
			      write_exp_elt_opcode (UNOP_CAST);
			      write_exp_elt_type (yyvsp[-3].tval);
			      write_exp_elt_opcode (UNOP_CAST);
			    }
			  else
			    {
			      write_exp_elt_opcode (OP_FUNCALL);
			      write_exp_elt_longcst (yyvsp[-1].lval);
			      write_exp_elt_opcode (OP_FUNCALL);
			    }
			}
    break;

  case 10:
#line 269 "ada-exp.y"
    { type_qualifier = yyvsp[-2].tval; }
    break;

  case 11:
#line 271 "ada-exp.y"
    {
			  if (yyvsp[-6].tval == NULL)
			    error (_("Type required for qualification"));
			  write_exp_elt_opcode (UNOP_QUAL);
			  write_exp_elt_type (yyvsp[-6].tval);
			  write_exp_elt_opcode (UNOP_QUAL);
			  type_qualifier = yyvsp[-4].tval;
			}
    break;

  case 12:
#line 281 "ada-exp.y"
    { yyval.tval = type_qualifier; }
    break;

  case 13:
#line 286 "ada-exp.y"
    { write_exp_elt_opcode (TERNOP_SLICE); }
    break;

  case 14:
#line 288 "ada-exp.y"
    { if (yyvsp[-5].tval == NULL) 
                            write_exp_elt_opcode (TERNOP_SLICE);
			  else
			    error (_("Cannot slice a type"));
			}
    break;

  case 15:
#line 295 "ada-exp.y"
    { }
    break;

  case 16:
#line 307 "ada-exp.y"
    { if (yyvsp[0].tval != NULL)
			    {
			      write_exp_elt_opcode (OP_TYPE);
			      write_exp_elt_type (yyvsp[0].tval);
			      write_exp_elt_opcode (OP_TYPE);
			    }
			}
    break;

  case 17:
#line 317 "ada-exp.y"
    { write_dollar_variable (yyvsp[0].sval); }
    break;

  case 20:
#line 327 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_NEG); }
    break;

  case 21:
#line 331 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_PLUS); }
    break;

  case 22:
#line 335 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_LOGICAL_NOT); }
    break;

  case 23:
#line 339 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_ABS); }
    break;

  case 24:
#line 342 "ada-exp.y"
    { yyval.lval = 0; }
    break;

  case 25:
#line 346 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 26:
#line 348 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 27:
#line 350 "ada-exp.y"
    { yyval.lval = yyvsp[-2].lval + 1; }
    break;

  case 28:
#line 352 "ada-exp.y"
    { yyval.lval = yyvsp[-4].lval + 1; }
    break;

  case 29:
#line 357 "ada-exp.y"
    { 
			  if (yyvsp[-2].tval == NULL)
			    error (_("Type required within braces in coercion"));
			  write_exp_elt_opcode (UNOP_MEMVAL);
			  write_exp_elt_type (yyvsp[-2].tval);
			  write_exp_elt_opcode (UNOP_MEMVAL);
			}
    break;

  case 30:
#line 369 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_EXP); }
    break;

  case 31:
#line 373 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_MUL); }
    break;

  case 32:
#line 377 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_DIV); }
    break;

  case 33:
#line 381 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_REM); }
    break;

  case 34:
#line 385 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_MOD); }
    break;

  case 35:
#line 389 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_REPEAT); }
    break;

  case 36:
#line 393 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_ADD); }
    break;

  case 37:
#line 397 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_CONCAT); }
    break;

  case 38:
#line 401 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_SUB); }
    break;

  case 40:
#line 408 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_EQUAL); }
    break;

  case 41:
#line 412 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_NOTEQUAL); }
    break;

  case 42:
#line 416 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_LEQ); }
    break;

  case 43:
#line 420 "ada-exp.y"
    { write_exp_elt_opcode (TERNOP_IN_RANGE); }
    break;

  case 44:
#line 422 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_IN_BOUNDS);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
			  write_exp_elt_opcode (BINOP_IN_BOUNDS);
			}
    break;

  case 45:
#line 427 "ada-exp.y"
    { 
			  if (yyvsp[0].tval == NULL)
			    error (_("Right operand of 'in' must be type"));
			  write_exp_elt_opcode (UNOP_IN_RANGE);
		          write_exp_elt_type (yyvsp[0].tval);
		          write_exp_elt_opcode (UNOP_IN_RANGE);
			}
    break;

  case 46:
#line 435 "ada-exp.y"
    { write_exp_elt_opcode (TERNOP_IN_RANGE);
		          write_exp_elt_opcode (UNOP_LOGICAL_NOT);
			}
    break;

  case 47:
#line 439 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_IN_BOUNDS);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
			  write_exp_elt_opcode (BINOP_IN_BOUNDS);
		          write_exp_elt_opcode (UNOP_LOGICAL_NOT);
			}
    break;

  case 48:
#line 445 "ada-exp.y"
    { 
			  if (yyvsp[0].tval == NULL)
			    error (_("Right operand of 'in' must be type"));
			  write_exp_elt_opcode (UNOP_IN_RANGE);
		          write_exp_elt_type (yyvsp[0].tval);
		          write_exp_elt_opcode (UNOP_IN_RANGE);
		          write_exp_elt_opcode (UNOP_LOGICAL_NOT);
			}
    break;

  case 49:
#line 456 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_GEQ); }
    break;

  case 50:
#line 460 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_LESS); }
    break;

  case 51:
#line 464 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_GTR); }
    break;

  case 58:
#line 477 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_AND); }
    break;

  case 59:
#line 479 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_AND); }
    break;

  case 60:
#line 484 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_LOGICAL_AND); }
    break;

  case 61:
#line 486 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_LOGICAL_AND); }
    break;

  case 62:
#line 491 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_IOR); }
    break;

  case 63:
#line 493 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_IOR); }
    break;

  case 64:
#line 498 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_LOGICAL_OR); }
    break;

  case 65:
#line 500 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_LOGICAL_OR); }
    break;

  case 66:
#line 504 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_XOR); }
    break;

  case 67:
#line 506 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_XOR); }
    break;

  case 68:
#line 518 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_ADDR); }
    break;

  case 69:
#line 520 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_ADDR);
			  write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type (type_system_address ());
			  write_exp_elt_opcode (UNOP_CAST);
			}
    break;

  case 70:
#line 526 "ada-exp.y"
    { write_int (yyvsp[0].lval, type_int ());
			  write_exp_elt_opcode (OP_ATR_FIRST); }
    break;

  case 71:
#line 529 "ada-exp.y"
    { write_int (yyvsp[0].lval, type_int ());
			  write_exp_elt_opcode (OP_ATR_LAST); }
    break;

  case 72:
#line 532 "ada-exp.y"
    { write_int (yyvsp[0].lval, type_int ());
			  write_exp_elt_opcode (OP_ATR_LENGTH); }
    break;

  case 73:
#line 535 "ada-exp.y"
    { write_exp_elt_opcode (OP_ATR_SIZE); }
    break;

  case 74:
#line 537 "ada-exp.y"
    { write_exp_elt_opcode (OP_ATR_TAG); }
    break;

  case 75:
#line 539 "ada-exp.y"
    { write_exp_elt_opcode (OP_ATR_MIN); }
    break;

  case 76:
#line 541 "ada-exp.y"
    { write_exp_elt_opcode (OP_ATR_MAX); }
    break;

  case 77:
#line 543 "ada-exp.y"
    { write_exp_elt_opcode (OP_ATR_POS); }
    break;

  case 78:
#line 545 "ada-exp.y"
    { write_exp_elt_opcode (OP_ATR_VAL); }
    break;

  case 79:
#line 547 "ada-exp.y"
    { write_exp_elt_opcode (OP_ATR_MODULUS); }
    break;

  case 80:
#line 551 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 81:
#line 553 "ada-exp.y"
    { yyval.lval = yyvsp[-1].typed_val.val; }
    break;

  case 82:
#line 558 "ada-exp.y"
    { 
			  if (yyvsp[0].tval == NULL)
			    error (_("Prefix must be type"));
			  write_exp_elt_opcode (OP_TYPE);
			  write_exp_elt_type (yyvsp[0].tval);
			  write_exp_elt_opcode (OP_TYPE); }
    break;

  case 84:
#line 569 "ada-exp.y"
    { write_exp_elt_opcode (OP_TYPE);
			  write_exp_elt_type (builtin_type_void);
			  write_exp_elt_opcode (OP_TYPE); }
    break;

  case 85:
#line 576 "ada-exp.y"
    { write_int ((LONGEST) yyvsp[0].typed_val.val, yyvsp[0].typed_val.type); }
    break;

  case 86:
#line 580 "ada-exp.y"
    { write_int (convert_char_literal (type_qualifier, yyvsp[0].typed_val.val),
			       (type_qualifier == NULL) 
			       ? yyvsp[0].typed_val.type : type_qualifier);
		  }
    break;

  case 87:
#line 587 "ada-exp.y"
    { write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type (yyvsp[0].typed_val_float.type);
			  write_exp_elt_dblcst (yyvsp[0].typed_val_float.dval);
			  write_exp_elt_opcode (OP_DOUBLE);
			}
    break;

  case 88:
#line 595 "ada-exp.y"
    { write_int (0, type_int ()); }
    break;

  case 89:
#line 599 "ada-exp.y"
    { 
			  write_exp_op_with_string (OP_STRING, yyvsp[0].sval);
			}
    break;

  case 90:
#line 605 "ada-exp.y"
    { error (_("NEW not implemented.")); }
    break;

  case 91:
#line 609 "ada-exp.y"
    { yyval.tval = write_var_or_type (NULL, yyvsp[0].sval); }
    break;

  case 92:
#line 611 "ada-exp.y"
    { yyval.tval = write_var_or_type (yyvsp[-1].bval, yyvsp[0].sval); }
    break;

  case 93:
#line 613 "ada-exp.y"
    { 
			  yyval.tval = write_var_or_type (NULL, yyvsp[-1].sval);
			  if (yyval.tval == NULL)
			    write_exp_elt_opcode (UNOP_ADDR);
			  else
			    yyval.tval = lookup_pointer_type (yyval.tval);
			}
    break;

  case 94:
#line 621 "ada-exp.y"
    { 
			  yyval.tval = write_var_or_type (yyvsp[-2].bval, yyvsp[-1].sval);
			  if (yyval.tval == NULL)
			    write_exp_elt_opcode (UNOP_ADDR);
			  else
			    yyval.tval = lookup_pointer_type (yyval.tval);
			}
    break;

  case 95:
#line 632 "ada-exp.y"
    { yyval.bval = block_lookup (NULL, yyvsp[-1].sval.ptr); }
    break;

  case 96:
#line 634 "ada-exp.y"
    { yyval.bval = block_lookup (yyvsp[-2].bval, yyvsp[-1].sval.ptr); }
    break;

  case 97:
#line 639 "ada-exp.y"
    {
			  write_exp_elt_opcode (OP_AGGREGATE);
			  write_exp_elt_longcst (yyvsp[-1].lval);
			  write_exp_elt_opcode (OP_AGGREGATE);
		        }
    break;

  case 98:
#line 647 "ada-exp.y"
    { yyval.lval = yyvsp[0].lval; }
    break;

  case 99:
#line 649 "ada-exp.y"
    { write_exp_elt_opcode (OP_POSITIONAL);
			  write_exp_elt_longcst (yyvsp[-1].lval);
			  write_exp_elt_opcode (OP_POSITIONAL);
			  yyval.lval = yyvsp[-1].lval + 1;
			}
    break;

  case 100:
#line 655 "ada-exp.y"
    { yyval.lval = yyvsp[-1].lval + yyvsp[0].lval; }
    break;

  case 101:
#line 660 "ada-exp.y"
    { write_exp_elt_opcode (OP_POSITIONAL);
			  write_exp_elt_longcst (0);
			  write_exp_elt_opcode (OP_POSITIONAL);
			  yyval.lval = 1;
			}
    break;

  case 102:
#line 666 "ada-exp.y"
    { write_exp_elt_opcode (OP_POSITIONAL);
			  write_exp_elt_longcst (yyvsp[-2].lval);
			  write_exp_elt_opcode (OP_POSITIONAL);
			  yyval.lval = yyvsp[-2].lval + 1; 
			}
    break;

  case 103:
#line 674 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 104:
#line 675 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 105:
#line 677 "ada-exp.y"
    { yyval.lval = yyvsp[0].lval + 1; }
    break;

  case 106:
#line 681 "ada-exp.y"
    { write_exp_elt_opcode (OP_OTHERS); }
    break;

  case 107:
#line 686 "ada-exp.y"
    {
			  write_exp_elt_opcode (OP_CHOICES);
			  write_exp_elt_longcst (yyvsp[0].lval);
			  write_exp_elt_opcode (OP_CHOICES);
		        }
    break;

  case 108:
#line 700 "ada-exp.y"
    { write_name_assoc (yyvsp[-1].sval); }
    break;

  case 109:
#line 701 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 110:
#line 703 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 111:
#line 705 "ada-exp.y"
    { write_exp_elt_opcode (OP_DISCRETE_RANGE);
			  write_exp_op_with_string (OP_NAME, empty_stoken);
			}
    break;

  case 112:
#line 708 "ada-exp.y"
    { yyval.lval = 1; }
    break;

  case 113:
#line 710 "ada-exp.y"
    { write_name_assoc (yyvsp[-1].sval); }
    break;

  case 114:
#line 711 "ada-exp.y"
    { yyval.lval = yyvsp[0].lval + 1; }
    break;

  case 115:
#line 713 "ada-exp.y"
    { yyval.lval = yyvsp[0].lval + 1; }
    break;

  case 116:
#line 715 "ada-exp.y"
    { write_exp_elt_opcode (OP_DISCRETE_RANGE); }
    break;

  case 117:
#line 716 "ada-exp.y"
    { yyval.lval = yyvsp[0].lval + 1; }
    break;

  case 118:
#line 723 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_IND); }
    break;

  case 119:
#line 725 "ada-exp.y"
    { write_exp_elt_opcode (UNOP_ADDR); }
    break;

  case 120:
#line 727 "ada-exp.y"
    { write_exp_elt_opcode (BINOP_SUBSCRIPT); }
    break;


    }

/* Line 1000 of yacc.c.  */
#line 2171 "ada-exp.c.tmp"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  const char* yyprefix;
	  char *yymsg;
	  int yyx;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 0;

	  yyprefix = ", expecting ";
	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		yysize += yystrlen (yyprefix) + yystrlen (yytname [yyx]);
		yycount += 1;
		if (yycount == 5)
		  {
		    yysize = 0;
		    break;
		  }
	      }
	  yysize += (sizeof ("syntax error, unexpected ")
		     + yystrlen (yytname[yytype]));
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yyprefix = ", expecting ";
		  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			yyp = yystpcpy (yyp, yyprefix);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yyprefix = " or ";
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* If at end of input, pop the error token,
	     then the rest of the stack, then return failure.  */
	  if (yychar == YYEOF)
	     for (;;)
	       {
		 YYPOPSTACK;
		 if (yyssp == yyss)
		   YYABORT;
		 YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
		 yydestruct (yystos[*yyssp], yyvsp);
	       }
        }
      else
	{
	  YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
	  yydestruct (yytoken, &yylval);
	  yychar = YYEMPTY;

	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

#ifdef __GNUC__
  /* Pacify GCC when the user code never invokes YYERROR and the label
     yyerrorlab therefore never appears in user code.  */
  if (0)
     goto yyerrorlab;
#endif

  yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 730 "ada-exp.y"


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



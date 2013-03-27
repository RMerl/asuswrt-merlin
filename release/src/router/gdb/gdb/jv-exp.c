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
     INTEGER_LITERAL = 258,
     FLOATING_POINT_LITERAL = 259,
     IDENTIFIER = 260,
     STRING_LITERAL = 261,
     BOOLEAN_LITERAL = 262,
     TYPENAME = 263,
     NAME_OR_INT = 264,
     ERROR = 265,
     LONG = 266,
     SHORT = 267,
     BYTE = 268,
     INT = 269,
     CHAR = 270,
     BOOLEAN = 271,
     DOUBLE = 272,
     FLOAT = 273,
     VARIABLE = 274,
     ASSIGN_MODIFY = 275,
     SUPER = 276,
     NEW = 277,
     OROR = 278,
     ANDAND = 279,
     NOTEQUAL = 280,
     EQUAL = 281,
     GEQ = 282,
     LEQ = 283,
     RSH = 284,
     LSH = 285,
     DECREMENT = 286,
     INCREMENT = 287
   };
#endif
#define INTEGER_LITERAL 258
#define FLOATING_POINT_LITERAL 259
#define IDENTIFIER 260
#define STRING_LITERAL 261
#define BOOLEAN_LITERAL 262
#define TYPENAME 263
#define NAME_OR_INT 264
#define ERROR 265
#define LONG 266
#define SHORT 267
#define BYTE 268
#define INT 269
#define CHAR 270
#define BOOLEAN 271
#define DOUBLE 272
#define FLOAT 273
#define VARIABLE 274
#define ASSIGN_MODIFY 275
#define SUPER 276
#define NEW 277
#define OROR 278
#define ANDAND 279
#define NOTEQUAL 280
#define EQUAL 281
#define GEQ 282
#define LEQ 283
#define RSH 284
#define LSH 285
#define DECREMENT 286
#define INCREMENT 287




/* Copy the first part of user declarations.  */
#line 39 "jv-exp.y"


#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "jv-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */
#include "block.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth java_maxdepth
#define	yyparse	java_parse
#define	yylex	java_lex
#define	yyerror	java_error
#define	yylval	java_lval
#define	yychar	java_char
#define	yydebug	java_debug
#define	yypact	java_pact	
#define	yyr1	java_r1			
#define	yyr2	java_r2			
#define	yydef	java_def		
#define	yychk	java_chk		
#define	yypgo	java_pgo		
#define	yyact	java_act		
#define	yyexca	java_exca
#define yyerrflag java_errflag
#define yynerrs	java_nerrs
#define	yyps	java_ps
#define	yypv	java_pv
#define	yys	java_s
#define	yy_yys	java_yys
#define	yystate	java_state
#define	yytmp	java_tmp
#define	yyv	java_v
#define	yy_yyv	java_yyv
#define	yyval	java_val
#define	yylloc	java_lloc
#define yyreds	java_reds		/* With YYDEBUG defined */
#define yytoks	java_toks		/* With YYDEBUG defined */
#define yyname	java_name		/* With YYDEBUG defined */
#define yyrule	java_rule		/* With YYDEBUG defined */
#define yylhs	java_yylhs
#define yylen	java_yylen
#define yydefred java_yydefred
#define yydgoto	java_yydgoto
#define yysindex java_yysindex
#define yyrindex java_yyrindex
#define yygindex java_yygindex
#define yytable	 java_yytable
#define yycheck	 java_yycheck

#ifndef YYDEBUG
#define	YYDEBUG 1		/* Default to yydebug support */
#endif

#define YYFPRINTF parser_fprintf

int yyparse (void);

static int yylex (void);

void yyerror (char *);

static struct type *java_type_from_name (struct stoken);
static void push_expression_name (struct stoken);
static void push_fieldnames (struct stoken);

static struct expression *copy_exp (struct expression *, int);
static void insert_exp (int, struct expression *);



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
#line 128 "jv-exp.y"
typedef union YYSTYPE {
    LONGEST lval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val_int;
    struct {
      DOUBLEST dval;
      struct type *type;
    } typed_val_float;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;
    int *ivec;
  } YYSTYPE;
/* Line 191 of yacc.c.  */
#line 245 "jv-exp.c.tmp"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */
#line 149 "jv-exp.y"

/* YYSTYPE gets defined by %union */
static int parse_number (char *, int, int, YYSTYPE *);


/* Line 214 of yacc.c.  */
#line 261 "jv-exp.c.tmp"

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
#define YYFINAL  98
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   373

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  56
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  58
/* YYNRULES -- Number of rules. */
#define YYNRULES  132
/* YYNRULES -- Number of states. */
#define YYNSTATES  209

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   287

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    54,     2,     2,     2,    43,    30,     2,
      48,    49,    41,    39,    23,    40,    46,    42,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    55,     2,
      33,    24,    34,    25,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    47,     2,    52,    29,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    50,    28,    51,    53,     2,     2,     2,
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
      15,    16,    17,    18,    19,    20,    21,    22,    26,    27,
      31,    32,    35,    36,    37,    38,    44,    45
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    15,    17,
      19,    21,    23,    25,    27,    29,    31,    33,    35,    37,
      39,    41,    43,    45,    47,    49,    51,    54,    57,    59,
      61,    63,    65,    67,    69,    73,    75,    79,    81,    83,
      85,    89,    91,    93,    95,    97,   101,   103,   105,   111,
     113,   117,   118,   120,   125,   130,   132,   135,   139,   142,
     146,   148,   149,   153,   157,   160,   161,   166,   173,   180,
     185,   190,   195,   197,   199,   201,   203,   205,   208,   211,
     213,   215,   218,   221,   224,   226,   229,   232,   234,   237,
     240,   242,   248,   253,   259,   261,   265,   269,   273,   275,
     279,   283,   285,   289,   293,   295,   299,   303,   307,   311,
     313,   317,   321,   323,   327,   329,   333,   335,   339,   341,
     345,   347,   351,   353,   359,   361,   363,   367,   371,   373,
     375,   377,   379
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      57,     0,    -1,    73,    -1,    58,    -1,    59,    -1,    62,
      -1,    68,    -1,     6,    -1,     3,    -1,     9,    -1,     4,
      -1,     7,    -1,    60,    -1,    63,    -1,    16,    -1,    64,
      -1,    65,    -1,    13,    -1,    12,    -1,    14,    -1,    11,
      -1,    15,    -1,    18,    -1,    17,    -1,    69,    -1,    66,
      -1,    62,    84,    -1,    69,    84,    -1,     5,    -1,    72,
      -1,    71,    -1,    72,    -1,     5,    -1,     9,    -1,    69,
      46,    71,    -1,   113,    -1,    73,    23,   113,    -1,    75,
      -1,    81,    -1,    61,    -1,    48,   113,    49,    -1,    78,
      -1,    86,    -1,    88,    -1,    90,    -1,    76,    79,    77,
      -1,    50,    -1,    51,    -1,    22,    67,    48,    80,    49,
      -1,   113,    -1,    79,    23,   113,    -1,    -1,    79,    -1,
      22,    62,    82,    85,    -1,    22,    66,    82,    85,    -1,
      83,    -1,    82,    83,    -1,    47,   113,    52,    -1,    47,
      52,    -1,    84,    47,    52,    -1,    84,    -1,    -1,    74,
      46,    71,    -1,    19,    46,    71,    -1,    69,    48,    -1,
      -1,    87,    89,    80,    49,    -1,    74,    46,    71,    48,
      80,    49,    -1,    21,    46,    71,    48,    80,    49,    -1,
      69,    47,   113,    52,    -1,    19,    47,   113,    52,    -1,
      75,    47,   113,    52,    -1,    74,    -1,    69,    -1,    19,
      -1,    92,    -1,    93,    -1,    91,    45,    -1,    91,    44,
      -1,    95,    -1,    96,    -1,    39,    94,    -1,    40,    94,
      -1,    41,    94,    -1,    97,    -1,    45,    94,    -1,    44,
      94,    -1,    91,    -1,    53,    94,    -1,    54,    94,    -1,
      98,    -1,    48,    62,    85,    49,    94,    -1,    48,   113,
      49,    97,    -1,    48,    69,    84,    49,    97,    -1,    94,
      -1,    99,    41,    94,    -1,    99,    42,    94,    -1,    99,
      43,    94,    -1,    99,    -1,   100,    39,    99,    -1,   100,
      40,    99,    -1,   100,    -1,   101,    38,   100,    -1,   101,
      37,   100,    -1,   101,    -1,   102,    33,   101,    -1,   102,
      34,   101,    -1,   102,    36,   101,    -1,   102,    35,   101,
      -1,   102,    -1,   103,    32,   102,    -1,   103,    31,   102,
      -1,   103,    -1,   104,    30,   103,    -1,   104,    -1,   105,
      29,   104,    -1,   105,    -1,   106,    28,   105,    -1,   106,
      -1,   107,    27,   106,    -1,   107,    -1,   108,    26,   107,
      -1,   108,    -1,   108,    25,   113,    55,   109,    -1,   109,
      -1,   111,    -1,   112,    24,   109,    -1,   112,    20,   109,
      -1,    70,    -1,    19,    -1,    86,    -1,    90,    -1,   110,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   205,   205,   206,   209,   218,   219,   223,   232,   237,
     245,   250,   255,   266,   267,   272,   273,   277,   279,   281,
     283,   285,   290,   292,   304,   309,   313,   315,   320,   321,
     325,   326,   330,   331,   335,   358,   359,   364,   365,   369,
     370,   371,   372,   373,   374,   375,   383,   388,   393,   399,
     401,   407,   408,   412,   415,   421,   422,   426,   430,   432,
     437,   439,   443,   445,   451,   457,   456,   462,   464,   469,
     486,   488,   493,   494,   496,   498,   499,   503,   508,   513,
     514,   515,   516,   518,   520,   524,   529,   534,   535,   537,
     539,   543,   547,   568,   576,   577,   579,   581,   586,   587,
     589,   594,   595,   597,   603,   604,   606,   608,   610,   616,
     617,   619,   624,   625,   630,   631,   635,   636,   641,   642,
     647,   648,   653,   654,   659,   660,   664,   666,   673,   675,
     677,   678,   683
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "INTEGER_LITERAL",
  "FLOATING_POINT_LITERAL", "IDENTIFIER", "STRING_LITERAL",
  "BOOLEAN_LITERAL", "TYPENAME", "NAME_OR_INT", "ERROR", "LONG", "SHORT",
  "BYTE", "INT", "CHAR", "BOOLEAN", "DOUBLE", "FLOAT", "VARIABLE",
  "ASSIGN_MODIFY", "SUPER", "NEW", "','", "'='", "'?'", "OROR", "ANDAND",
  "'|'", "'^'", "'&'", "NOTEQUAL", "EQUAL", "'<'", "'>'", "GEQ", "LEQ",
  "RSH", "LSH", "'+'", "'-'", "'*'", "'/'", "'%'", "DECREMENT",
  "INCREMENT", "'.'", "'['", "'('", "')'", "'{'", "'}'", "']'", "'~'",
  "'!'", "':'", "$accept", "start", "type_exp", "PrimitiveOrArrayType",
  "StringLiteral", "Literal", "PrimitiveType", "NumericType",
  "IntegralType", "FloatingPointType", "ClassOrInterfaceType", "ClassType",
  "ArrayType", "Name", "ForcedName", "SimpleName", "QualifiedName", "exp1",
  "Primary", "PrimaryNoNewArray", "lcurly", "rcurly",
  "ClassInstanceCreationExpression", "ArgumentList", "ArgumentList_opt",
  "ArrayCreationExpression", "DimExprs", "DimExpr", "Dims", "Dims_opt",
  "FieldAccess", "FuncStart", "MethodInvocation", "@1", "ArrayAccess",
  "PostfixExpression", "PostIncrementExpression",
  "PostDecrementExpression", "UnaryExpression", "PreIncrementExpression",
  "PreDecrementExpression", "UnaryExpressionNotPlusMinus",
  "CastExpression", "MultiplicativeExpression", "AdditiveExpression",
  "ShiftExpression", "RelationalExpression", "EqualityExpression",
  "AndExpression", "ExclusiveOrExpression", "InclusiveOrExpression",
  "ConditionalAndExpression", "ConditionalOrExpression",
  "ConditionalExpression", "AssignmentExpression", "Assignment",
  "LeftHandSide", "Expression", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,    44,    61,    63,   278,   279,   124,    94,
      38,   280,   281,    60,    62,   282,   283,   284,   285,    43,
      45,    42,    47,    37,   286,   287,    46,    91,    40,    41,
     123,   125,    93,   126,    33,    58
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    56,    57,    57,    58,    59,    59,    60,    61,    61,
      61,    61,    61,    62,    62,    63,    63,    64,    64,    64,
      64,    64,    65,    65,    66,    67,    68,    68,    69,    69,
      70,    70,    71,    71,    72,    73,    73,    74,    74,    75,
      75,    75,    75,    75,    75,    75,    76,    77,    78,    79,
      79,    80,    80,    81,    81,    82,    82,    83,    84,    84,
      85,    85,    86,    86,    87,    89,    88,    88,    88,    90,
      90,    90,    91,    91,    91,    91,    91,    92,    93,    94,
      94,    94,    94,    94,    94,    95,    96,    97,    97,    97,
      97,    98,    98,    98,    99,    99,    99,    99,   100,   100,
     100,   101,   101,   101,   102,   102,   102,   102,   102,   103,
     103,   103,   104,   104,   105,   105,   106,   106,   107,   107,
     108,   108,   109,   109,   110,   110,   111,   111,   112,   112,
     112,   112,   113
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     1,     1,
       1,     1,     1,     1,     3,     1,     3,     1,     1,     1,
       3,     1,     1,     1,     1,     3,     1,     1,     5,     1,
       3,     0,     1,     4,     4,     1,     2,     3,     2,     3,
       1,     0,     3,     3,     2,     0,     4,     6,     6,     4,
       4,     4,     1,     1,     1,     1,     1,     2,     2,     1,
       1,     2,     2,     2,     1,     2,     2,     1,     2,     2,
       1,     5,     4,     5,     1,     3,     3,     3,     1,     3,
       3,     1,     3,     3,     1,     3,     3,     3,     3,     1,
       3,     3,     1,     3,     1,     3,     1,     3,     1,     3,
       1,     3,     1,     5,     1,     1,     3,     3,     1,     1,
       1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     8,    10,    28,     7,    11,     9,    20,    18,    17,
      19,    21,    14,    23,    22,    74,     0,     0,     0,     0,
       0,     0,     0,     0,    46,     0,     0,     0,     3,     4,
      12,    39,     5,    13,    15,    16,     6,    73,   128,    30,
      29,     2,    72,    37,     0,    41,    38,    42,    65,    43,
      44,    87,    75,    76,    94,    79,    80,    84,    90,    98,
     101,   104,   109,   112,   114,   116,   118,   120,   122,   124,
     132,   125,     0,    35,     0,     0,     0,    28,     0,    25,
       0,    24,    29,     9,    74,    73,    42,    44,    81,    82,
      83,    86,    85,    61,    73,     0,    88,    89,     1,     0,
      26,     0,     0,    64,    27,     0,     0,     0,     0,    49,
      51,    78,    77,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    32,    33,    63,     0,     0,     0,
      61,    55,    61,    51,     0,    60,     0,     0,    40,    58,
       0,    34,     0,    36,    62,     0,     0,    47,    45,    52,
       0,    95,    96,    97,    99,   100,   103,   102,   105,   106,
     108,   107,   111,   110,   113,   115,   117,   119,     0,   121,
     127,   126,    70,    51,     0,     0,    56,    53,    54,     0,
       0,     0,    92,    59,    69,    51,    71,    50,    66,     0,
       0,    57,    48,    91,    93,     0,   123,    68,    67
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short yydefgoto[] =
{
      -1,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      79,    80,    36,    85,    38,    39,    82,    41,    42,    43,
      44,   158,    45,   159,   160,    46,   140,   141,   145,   146,
      86,    48,    49,   110,    87,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,   109
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -145
static const short yypact[] =
{
     215,  -145,  -145,    -5,  -145,  -145,     1,  -145,  -145,  -145,
    -145,  -145,  -145,  -145,  -145,    -7,   -19,   279,    50,    50,
      50,    50,    50,   215,  -145,    50,    50,    46,  -145,  -145,
    -145,  -145,    -9,  -145,  -145,  -145,  -145,    87,  -145,  -145,
      12,    44,     5,    16,   319,  -145,  -145,    28,  -145,  -145,
      38,    29,  -145,  -145,  -145,  -145,  -145,  -145,  -145,    99,
      53,    85,    52,    94,    66,    41,    71,    74,   122,  -145,
    -145,  -145,    40,  -145,    26,   319,    26,  -145,    59,    59,
      67,    82,  -145,  -145,   111,   107,  -145,  -145,  -145,  -145,
    -145,  -145,  -145,    -9,    87,    68,  -145,  -145,  -145,    79,
      91,    26,   267,  -145,    91,   319,    26,   319,   -18,  -145,
     319,  -145,  -145,    50,    50,    50,    50,    50,    50,    50,
      50,    50,    50,    50,    50,    50,    50,    50,    50,    50,
     319,    50,    50,    50,  -145,  -145,  -145,   112,   126,   319,
     128,  -145,   128,   319,   319,    91,   127,   -31,   193,  -145,
     125,  -145,   129,  -145,   131,   130,   319,  -145,  -145,   157,
     135,  -145,  -145,  -145,    99,    99,    53,    53,    85,    85,
      85,    85,    52,    52,    94,    66,    41,    71,   132,    74,
    -145,  -145,  -145,   319,   134,   267,  -145,  -145,  -145,   139,
      50,   193,  -145,  -145,  -145,   319,  -145,  -145,  -145,    50,
     141,  -145,  -145,  -145,  -145,   144,  -145,  -145,  -145
};

/* YYPGOTO[NTERM-NUM].  */
static const short yypgoto[] =
{
    -145,  -145,  -145,  -145,  -145,  -145,    -3,  -145,  -145,  -145,
    -145,  -145,  -145,    11,  -145,   -64,     0,  -145,  -145,  -145,
    -145,  -145,  -145,   150,  -134,  -145,   124,  -116,   -29,   -99,
       6,  -145,  -145,  -145,    22,  -145,  -145,  -145,    58,  -145,
    -145,  -144,  -145,    43,    49,    -2,    45,    78,    81,    83,
      77,    92,  -145,  -131,  -145,  -145,  -145,     7
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -132
static const short yytable[] =
{
      40,   180,   181,   100,   192,   156,    47,    73,   104,   189,
     136,    37,   138,  -129,    78,   -32,   150,  -129,   191,   -32,
      93,   -33,    50,    40,   186,   -33,   186,    76,    81,    47,
      95,   134,   -31,   157,    94,   135,   -31,   151,    99,    74,
      75,   187,   154,   188,    40,    50,    98,   204,  -130,   200,
      47,   106,  -130,     1,     2,    77,     4,     5,  -131,    83,
     132,   205,  -131,   107,   133,   147,    50,   105,   206,    84,
     127,    16,    17,   111,   112,    40,    88,    89,    90,    91,
      92,    47,   137,    96,    97,   120,   121,   122,   123,    18,
      19,    20,   116,   117,    21,    22,   126,    50,    23,   128,
      24,   129,    40,    25,    26,    40,   139,    40,    47,   152,
      40,    47,   153,    47,   155,   143,    47,   148,   168,   169,
     170,   171,   118,   119,    50,   124,   125,    50,   101,    50,
      40,   149,    50,   101,   102,   103,    47,   178,   150,    40,
     113,   114,   115,    40,    40,    47,   184,   130,   131,    47,
      47,   152,    50,   101,   144,   103,    40,    74,    75,   164,
     165,    50,    47,   197,   182,    50,    50,   166,   167,   172,
     173,   161,   162,   163,   183,   185,   190,   193,    50,   195,
     156,   194,   196,    40,   198,    40,   201,   199,   202,    47,
     207,    47,   184,   208,   108,    40,     1,     2,    77,     4,
       5,    47,    83,   142,   174,    50,   177,    50,   175,     0,
       0,   176,    84,     0,    16,    17,     0,    50,     1,     2,
       3,     4,     5,   179,     6,     0,     7,     8,     9,    10,
      11,    12,    13,    14,    15,     0,    16,    17,     0,     0,
       0,    23,     0,    24,     0,     0,    25,    26,   203,     0,
       0,     0,     0,     0,    18,    19,    20,     0,     0,    21,
      22,     0,     0,    23,     0,    24,     0,     0,    25,    26,
       1,     2,     3,     4,     5,     0,     6,     0,     0,     0,
       0,     0,     0,     0,    77,     0,    15,     0,    16,    17,
       7,     8,     9,    10,    11,    12,    13,    14,     0,     0,
       0,     0,     0,     0,     0,     0,    18,    19,    20,     0,
       0,    21,    22,     0,     0,    23,     0,    24,     0,   149,
      25,    26,     1,     2,     3,     4,     5,     0,     6,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    15,     0,
      16,    17,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    18,    19,
      20,     0,     0,    21,    22,     0,     0,    23,     0,    24,
       0,     0,    25,    26
};

static const short yycheck[] =
{
       0,   132,   133,    32,   148,    23,     0,     0,    37,   143,
      74,     0,    76,    20,    17,    20,    47,    24,    49,    24,
      23,    20,     0,    23,   140,    24,   142,    46,    17,    23,
      23,     5,    20,    51,    23,     9,    24,   101,    47,    46,
      47,   140,   106,   142,    44,    23,     0,   191,    20,   183,
      44,    46,    24,     3,     4,     5,     6,     7,    20,     9,
      20,   195,    24,    47,    24,    94,    44,    23,   199,    19,
      29,    21,    22,    44,    45,    75,    18,    19,    20,    21,
      22,    75,    75,    25,    26,    33,    34,    35,    36,    39,
      40,    41,    39,    40,    44,    45,    30,    75,    48,    28,
      50,    27,   102,    53,    54,   105,    47,   107,   102,   102,
     110,   105,   105,   107,   107,    48,   110,    49,   120,   121,
     122,   123,    37,    38,   102,    31,    32,   105,    46,   107,
     130,    52,   110,    46,    47,    48,   130,   130,    47,   139,
      41,    42,    43,   143,   144,   139,   139,    25,    26,   143,
     144,   144,   130,    46,    47,    48,   156,    46,    47,   116,
     117,   139,   156,   156,    52,   143,   144,   118,   119,   124,
     125,   113,   114,   115,    48,    47,    49,    52,   156,    48,
      23,    52,    52,   183,    49,   185,    52,    55,    49,   183,
      49,   185,   185,    49,    44,   195,     3,     4,     5,     6,
       7,   195,     9,    79,   126,   183,   129,   185,   127,    -1,
      -1,   128,    19,    -1,    21,    22,    -1,   195,     3,     4,
       5,     6,     7,   131,     9,    -1,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    -1,    21,    22,    -1,    -1,
      -1,    48,    -1,    50,    -1,    -1,    53,    54,   190,    -1,
      -1,    -1,    -1,    -1,    39,    40,    41,    -1,    -1,    44,
      45,    -1,    -1,    48,    -1,    50,    -1,    -1,    53,    54,
       3,     4,     5,     6,     7,    -1,     9,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     5,    -1,    19,    -1,    21,    22,
      11,    12,    13,    14,    15,    16,    17,    18,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    39,    40,    41,    -1,
      -1,    44,    45,    -1,    -1,    48,    -1,    50,    -1,    52,
      53,    54,     3,     4,     5,     6,     7,    -1,     9,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    -1,
      21,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,    40,
      41,    -1,    -1,    44,    45,    -1,    -1,    48,    -1,    50,
      -1,    -1,    53,    54
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     3,     4,     5,     6,     7,     9,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    21,    22,    39,    40,
      41,    44,    45,    48,    50,    53,    54,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    78,    81,    86,    87,    88,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,    46,    47,    46,     5,    62,    66,
      67,    69,    72,     9,    19,    69,    86,    90,    94,    94,
      94,    94,    94,    62,    69,   113,    94,    94,     0,    47,
      84,    46,    47,    48,    84,    23,    46,    47,    79,   113,
      89,    44,    45,    41,    42,    43,    39,    40,    37,    38,
      33,    34,    35,    36,    31,    32,    30,    29,    28,    27,
      25,    26,    20,    24,     5,     9,    71,   113,    71,    47,
      82,    83,    82,    48,    47,    84,    85,    84,    49,    52,
      47,    71,   113,   113,    71,   113,    23,    51,    77,    79,
      80,    94,    94,    94,    99,    99,   100,   100,   101,   101,
     101,   101,   102,   102,   103,   104,   105,   106,   113,   107,
     109,   109,    52,    48,   113,    47,    83,    85,    85,    80,
      49,    49,    97,    52,    52,    48,    52,   113,    49,    55,
      80,    52,    49,    94,    97,    80,   109,    49,    49
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
#line 210 "jv-exp.y"
    {
		  write_exp_elt_opcode(OP_TYPE);
		  write_exp_elt_type(yyvsp[0].tval);
		  write_exp_elt_opcode(OP_TYPE);
		}
    break;

  case 7:
#line 224 "jv-exp.y"
    {
		  write_exp_elt_opcode (OP_STRING);
		  write_exp_string (yyvsp[0].sval);
		  write_exp_elt_opcode (OP_STRING);
		}
    break;

  case 8:
#line 233 "jv-exp.y"
    { write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (yyvsp[0].typed_val_int.type);
		  write_exp_elt_longcst ((LONGEST)(yyvsp[0].typed_val_int.val));
		  write_exp_elt_opcode (OP_LONG); }
    break;

  case 9:
#line 238 "jv-exp.y"
    { YYSTYPE val;
		  parse_number (yyvsp[0].sval.ptr, yyvsp[0].sval.length, 0, &val);
		  write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (val.typed_val_int.type);
		  write_exp_elt_longcst ((LONGEST)val.typed_val_int.val);
		  write_exp_elt_opcode (OP_LONG);
		}
    break;

  case 10:
#line 246 "jv-exp.y"
    { write_exp_elt_opcode (OP_DOUBLE);
		  write_exp_elt_type (yyvsp[0].typed_val_float.type);
		  write_exp_elt_dblcst (yyvsp[0].typed_val_float.dval);
		  write_exp_elt_opcode (OP_DOUBLE); }
    break;

  case 11:
#line 251 "jv-exp.y"
    { write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (java_boolean_type);
		  write_exp_elt_longcst ((LONGEST)yyvsp[0].lval);
		  write_exp_elt_opcode (OP_LONG); }
    break;

  case 14:
#line 268 "jv-exp.y"
    { yyval.tval = java_boolean_type; }
    break;

  case 17:
#line 278 "jv-exp.y"
    { yyval.tval = java_byte_type; }
    break;

  case 18:
#line 280 "jv-exp.y"
    { yyval.tval = java_short_type; }
    break;

  case 19:
#line 282 "jv-exp.y"
    { yyval.tval = java_int_type; }
    break;

  case 20:
#line 284 "jv-exp.y"
    { yyval.tval = java_long_type; }
    break;

  case 21:
#line 286 "jv-exp.y"
    { yyval.tval = java_char_type; }
    break;

  case 22:
#line 291 "jv-exp.y"
    { yyval.tval = java_float_type; }
    break;

  case 23:
#line 293 "jv-exp.y"
    { yyval.tval = java_double_type; }
    break;

  case 24:
#line 305 "jv-exp.y"
    { yyval.tval = java_type_from_name (yyvsp[0].sval); }
    break;

  case 26:
#line 314 "jv-exp.y"
    { yyval.tval = java_array_type (yyvsp[-1].tval, yyvsp[0].lval); }
    break;

  case 27:
#line 316 "jv-exp.y"
    { yyval.tval = java_array_type (java_type_from_name (yyvsp[-1].sval), yyvsp[0].lval); }
    break;

  case 34:
#line 336 "jv-exp.y"
    { yyval.sval.length = yyvsp[-2].sval.length + yyvsp[0].sval.length + 1;
		  if (yyvsp[-2].sval.ptr + yyvsp[-2].sval.length + 1 == yyvsp[0].sval.ptr
		      && yyvsp[-2].sval.ptr[yyvsp[-2].sval.length] == '.')
		    yyval.sval.ptr = yyvsp[-2].sval.ptr;  /* Optimization. */
		  else
		    {
		      yyval.sval.ptr = (char *) xmalloc (yyval.sval.length + 1);
		      make_cleanup (free, yyval.sval.ptr);
		      sprintf (yyval.sval.ptr, "%.*s.%.*s",
			       yyvsp[-2].sval.length, yyvsp[-2].sval.ptr, yyvsp[0].sval.length, yyvsp[0].sval.ptr);
		} }
    break;

  case 36:
#line 360 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_COMMA); }
    break;

  case 45:
#line 376 "jv-exp.y"
    { write_exp_elt_opcode (OP_ARRAY);
		  write_exp_elt_longcst ((LONGEST) 0);
		  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
		  write_exp_elt_opcode (OP_ARRAY); }
    break;

  case 46:
#line 384 "jv-exp.y"
    { start_arglist (); }
    break;

  case 47:
#line 389 "jv-exp.y"
    { yyval.lval = end_arglist () - 1; }
    break;

  case 48:
#line 394 "jv-exp.y"
    { internal_error (__FILE__, __LINE__,
				  _("FIXME - ClassInstanceCreationExpression")); }
    break;

  case 49:
#line 400 "jv-exp.y"
    { arglist_len = 1; }
    break;

  case 50:
#line 402 "jv-exp.y"
    { arglist_len++; }
    break;

  case 51:
#line 407 "jv-exp.y"
    { arglist_len = 0; }
    break;

  case 53:
#line 413 "jv-exp.y"
    { internal_error (__FILE__, __LINE__,
				  _("FIXME - ArrayCreationExpression")); }
    break;

  case 54:
#line 416 "jv-exp.y"
    { internal_error (__FILE__, __LINE__,
				  _("FIXME - ArrayCreationExpression")); }
    break;

  case 58:
#line 431 "jv-exp.y"
    { yyval.lval = 1; }
    break;

  case 59:
#line 433 "jv-exp.y"
    { yyval.lval = yyvsp[-2].lval + 1; }
    break;

  case 61:
#line 439 "jv-exp.y"
    { yyval.lval = 0; }
    break;

  case 62:
#line 444 "jv-exp.y"
    { push_fieldnames (yyvsp[0].sval); }
    break;

  case 63:
#line 446 "jv-exp.y"
    { push_fieldnames (yyvsp[0].sval); }
    break;

  case 64:
#line 452 "jv-exp.y"
    { push_expression_name (yyvsp[-1].sval); }
    break;

  case 65:
#line 457 "jv-exp.y"
    { start_arglist(); }
    break;

  case 66:
#line 459 "jv-exp.y"
    { write_exp_elt_opcode (OP_FUNCALL);
		  write_exp_elt_longcst ((LONGEST) end_arglist ());
		  write_exp_elt_opcode (OP_FUNCALL); }
    break;

  case 67:
#line 463 "jv-exp.y"
    { error (_("Form of method invocation not implemented")); }
    break;

  case 68:
#line 465 "jv-exp.y"
    { error (_("Form of method invocation not implemented")); }
    break;

  case 69:
#line 470 "jv-exp.y"
    {
                  /* Emit code for the Name now, then exchange it in the
		     expout array with the Expression's code.  We could
		     introduce a OP_SWAP code or a reversed version of
		     BINOP_SUBSCRIPT, but that makes the rest of GDB pay
		     for our parsing kludges.  */
		  struct expression *name_expr;

		  push_expression_name (yyvsp[-3].sval);
		  name_expr = copy_exp (expout, expout_ptr);
		  expout_ptr -= name_expr->nelts;
		  insert_exp (expout_ptr-length_of_subexp (expout, expout_ptr),
			      name_expr);
		  free (name_expr);
		  write_exp_elt_opcode (BINOP_SUBSCRIPT);
		}
    break;

  case 70:
#line 487 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_SUBSCRIPT); }
    break;

  case 71:
#line 489 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_SUBSCRIPT); }
    break;

  case 73:
#line 495 "jv-exp.y"
    { push_expression_name (yyvsp[0].sval); }
    break;

  case 77:
#line 504 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_POSTINCREMENT); }
    break;

  case 78:
#line 509 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_POSTDECREMENT); }
    break;

  case 82:
#line 517 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_NEG); }
    break;

  case 83:
#line 519 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_IND); }
    break;

  case 85:
#line 525 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_PREINCREMENT); }
    break;

  case 86:
#line 530 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_PREDECREMENT); }
    break;

  case 88:
#line 536 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_COMPLEMENT); }
    break;

  case 89:
#line 538 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_LOGICAL_NOT); }
    break;

  case 91:
#line 544 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (java_array_type (yyvsp[-3].tval, yyvsp[-2].lval));
		  write_exp_elt_opcode (UNOP_CAST); }
    break;

  case 92:
#line 548 "jv-exp.y"
    {
		  int exp_size = expout_ptr;
		  int last_exp_size = length_of_subexp(expout, expout_ptr);
		  struct type *type;
		  int i;
		  int base = expout_ptr - last_exp_size - 3;
		  if (base < 0 || expout->elts[base+2].opcode != OP_TYPE)
		    error (_("Invalid cast expression"));
		  type = expout->elts[base+1].type;
		  /* Remove the 'Expression' and slide the
		     UnaryExpressionNotPlusMinus down to replace it. */
		  for (i = 0;  i < last_exp_size;  i++)
		    expout->elts[base + i] = expout->elts[base + i + 3];
		  expout_ptr -= 3;
		  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
		    type = lookup_pointer_type (type);
		  write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (type);
		  write_exp_elt_opcode (UNOP_CAST);
		}
    break;

  case 93:
#line 569 "jv-exp.y"
    { write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (java_array_type (java_type_from_name (yyvsp[-3].sval), yyvsp[-2].lval));
		  write_exp_elt_opcode (UNOP_CAST); }
    break;

  case 95:
#line 578 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_MUL); }
    break;

  case 96:
#line 580 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_DIV); }
    break;

  case 97:
#line 582 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_REM); }
    break;

  case 99:
#line 588 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_ADD); }
    break;

  case 100:
#line 590 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_SUB); }
    break;

  case 102:
#line 596 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_LSH); }
    break;

  case 103:
#line 598 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_RSH); }
    break;

  case 105:
#line 605 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_LESS); }
    break;

  case 106:
#line 607 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_GTR); }
    break;

  case 107:
#line 609 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_LEQ); }
    break;

  case 108:
#line 611 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_GEQ); }
    break;

  case 110:
#line 618 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_EQUAL); }
    break;

  case 111:
#line 620 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_NOTEQUAL); }
    break;

  case 113:
#line 626 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_AND); }
    break;

  case 115:
#line 632 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_XOR); }
    break;

  case 117:
#line 637 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_BITWISE_IOR); }
    break;

  case 119:
#line 643 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_LOGICAL_AND); }
    break;

  case 121:
#line 649 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_LOGICAL_OR); }
    break;

  case 123:
#line 655 "jv-exp.y"
    { write_exp_elt_opcode (TERNOP_COND); }
    break;

  case 126:
#line 665 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_ASSIGN); }
    break;

  case 127:
#line 667 "jv-exp.y"
    { write_exp_elt_opcode (BINOP_ASSIGN_MODIFY);
		  write_exp_elt_opcode (yyvsp[-1].opcode);
		  write_exp_elt_opcode (BINOP_ASSIGN_MODIFY); }
    break;

  case 128:
#line 674 "jv-exp.y"
    { push_expression_name (yyvsp[0].sval); }
    break;


    }

/* Line 1000 of yacc.c.  */
#line 1850 "jv-exp.c.tmp"

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


#line 686 "jv-exp.y"

/* Take care of parsing a number (anything that starts with a digit).
   Set yylval and return the token type; update lexptr.
   LEN is the number of characters in it.  */

/*** Needs some error checking for the float case ***/

static int
parse_number (p, len, parsed_float, putithere)
     char *p;
     int len;
     int parsed_float;
     YYSTYPE *putithere;
{
  ULONGEST n = 0;
  ULONGEST limit, limit_div_base;

  int c;
  int base = input_radix;

  struct type *type;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      char c;
      int num = 0;	/* number of tokens scanned by scanf */
      char saved_char = p[len];

      p[len] = 0;	/* null-terminate the token */
      num = sscanf (p, DOUBLEST_SCAN_FORMAT "%c",
		    &putithere->typed_val_float.dval, &c);
      p[len] = saved_char;	/* restore the input stream */
      if (num != 1) 		/* check scanf found ONLY a float ... */
	return ERROR;
      /* See if it has `f' or `d' suffix (float or double).  */

      c = tolower (p[len - 1]);

      if (c == 'f' || c == 'F')
	putithere->typed_val_float.type = builtin_type_float;
      else if (isdigit (c) || c == '.' || c == 'd' || c == 'D')
	putithere->typed_val_float.type = builtin_type_double;
      else
	return ERROR;

      return FLOATING_POINT_LITERAL;
    }

  /* Handle base-switching prefixes 0x, 0t, 0d, 0 */
  if (p[0] == '0')
    switch (p[1])
      {
      case 'x':
      case 'X':
	if (len >= 3)
	  {
	    p += 2;
	    base = 16;
	    len -= 2;
	  }
	break;

      case 't':
      case 'T':
      case 'd':
      case 'D':
	if (len >= 3)
	  {
	    p += 2;
	    base = 10;
	    len -= 2;
	  }
	break;

      default:
	base = 8;
	break;
      }

  c = p[len-1];
  /* A paranoid calculation of (1<<64)-1. */
  limit = (ULONGEST)0xffffffff;
  limit = ((limit << 16) << 16) | limit;
  if (c == 'l' || c == 'L')
    {
      type = java_long_type;
      len--;
    }
  else
    {
      type = java_int_type;
    }
  limit_div_base = limit / (ULONGEST) base;

  while (--len >= 0)
    {
      c = *p++;
      if (c >= '0' && c <= '9')
	c -= '0';
      else if (c >= 'A' && c <= 'Z')
	c -= 'A' - 10;
      else if (c >= 'a' && c <= 'z')
	c -= 'a' - 10;
      else
	return ERROR;	/* Char not a digit */
      if (c >= base)
	return ERROR;
      if (n > limit_div_base
	  || (n *= base) > limit - c)
	error (_("Numeric constant too large"));
      n += c;
	}

  /* If the type is bigger than a 32-bit signed integer can be, implicitly
     promote to long.  Java does not do this, so mark it as builtin_type_uint64
     rather than java_long_type.  0x80000000 will become -0x80000000 instead
     of 0x80000000L, because we don't know the sign at this point.
  */
  if (type == java_int_type && n > (ULONGEST)0x80000000)
    type = builtin_type_uint64;

  putithere->typed_val_int.val = n;
  putithere->typed_val_int.type = type;

  return INTEGER_LITERAL;
}

struct token
{
  char *operator;
  int token;
  enum exp_opcode opcode;
};

static const struct token tokentab3[] =
  {
    {">>=", ASSIGN_MODIFY, BINOP_RSH},
    {"<<=", ASSIGN_MODIFY, BINOP_LSH}
  };

static const struct token tokentab2[] =
  {
    {"+=", ASSIGN_MODIFY, BINOP_ADD},
    {"-=", ASSIGN_MODIFY, BINOP_SUB},
    {"*=", ASSIGN_MODIFY, BINOP_MUL},
    {"/=", ASSIGN_MODIFY, BINOP_DIV},
    {"%=", ASSIGN_MODIFY, BINOP_REM},
    {"|=", ASSIGN_MODIFY, BINOP_BITWISE_IOR},
    {"&=", ASSIGN_MODIFY, BINOP_BITWISE_AND},
    {"^=", ASSIGN_MODIFY, BINOP_BITWISE_XOR},
    {"++", INCREMENT, BINOP_END},
    {"--", DECREMENT, BINOP_END},
    {"&&", ANDAND, BINOP_END},
    {"||", OROR, BINOP_END},
    {"<<", LSH, BINOP_END},
    {">>", RSH, BINOP_END},
    {"==", EQUAL, BINOP_END},
    {"!=", NOTEQUAL, BINOP_END},
    {"<=", LEQ, BINOP_END},
    {">=", GEQ, BINOP_END}
  };

/* Read one token, getting characters through lexptr.  */

static int
yylex ()
{
  int c;
  int namelen;
  unsigned int i;
  char *tokstart;
  char *tokptr;
  int tempbufindex;
  static char *tempbuf;
  static int tempbufsize;
  
 retry:

  prev_lexptr = lexptr;

  tokstart = lexptr;
  /* See if it is a special token of length 3.  */
  for (i = 0; i < sizeof tokentab3 / sizeof tokentab3[0]; i++)
    if (strncmp (tokstart, tokentab3[i].operator, 3) == 0)
      {
	lexptr += 3;
	yylval.opcode = tokentab3[i].opcode;
	return tokentab3[i].token;
      }

  /* See if it is a special token of length 2.  */
  for (i = 0; i < sizeof tokentab2 / sizeof tokentab2[0]; i++)
    if (strncmp (tokstart, tokentab2[i].operator, 2) == 0)
      {
	lexptr += 2;
	yylval.opcode = tokentab2[i].opcode;
	return tokentab2[i].token;
      }

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
	error (_("Empty character constant"));

      yylval.typed_val_int.val = c;
      yylval.typed_val_int.type = java_char_type;

      c = *lexptr++;
      if (c != '\'')
	{
	  namelen = skip_quoted (tokstart) - tokstart;
	  if (namelen > 2)
	    {
	      lexptr = tokstart + namelen;
	      if (lexptr[-1] != '\'')
		error (_("Unmatched single quote"));
	      namelen -= 2;
	      tokstart++;
	      goto tryname;
	    }
	  error (_("Invalid character constant"));
	}
      return INTEGER_LITERAL;

    case '(':
      paren_depth++;
      lexptr++;
      return c;

    case ')':
      if (paren_depth == 0)
	return 0;
      paren_depth--;
      lexptr++;
      return c;

    case ',':
      if (comma_terminates && paren_depth == 0)
	return 0;
      lexptr++;
      return c;

    case '.':
      /* Might be a floating point number.  */
      if (lexptr[1] < '0' || lexptr[1] > '9')
	goto symbol;		/* Nope, must be a symbol. */
      /* FALL THRU into number case.  */

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
	char *p = tokstart;
	int hex = input_radix > 10;

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
	       a decimal floating point number regardless of the radix.  */
	    else if (!got_dot && *p == '.')
	      got_dot = 1;
	    else if (got_e && (p[-1] == 'e' || p[-1] == 'E')
		     && (*p == '-' || *p == '+'))
	      /* This is the sign of the exponent, not the end of the
		 number.  */
	      continue;
	    /* We will take any letters or digits.  parse_number will
	       complain if past the radix, or if L or U are not final.  */
	    else if ((*p < '0' || *p > '9')
		     && ((*p < 'a' || *p > 'z')
				  && (*p < 'A' || *p > 'Z')))
	      break;
	  }
	toktype = parse_number (tokstart, p - tokstart, got_dot|got_e, &yylval);
        if (toktype == ERROR)
	  {
	    char *err_copy = (char *) alloca (p - tokstart + 1);

	    memcpy (err_copy, tokstart, p - tokstart);
	    err_copy[p - tokstart] = 0;
	    error (_("Invalid number \"%s\""), err_copy);
	  }
	lexptr = p;
	return toktype;
      }

    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '|':
    case '&':
    case '^':
    case '~':
    case '!':
    case '<':
    case '>':
    case '[':
    case ']':
    case '?':
    case ':':
    case '=':
    case '{':
    case '}':
    symbol:
      lexptr++;
      return c;

    case '"':

      /* Build the gdb internal form of the input string in tempbuf,
	 translating any standard C escape forms seen.  Note that the
	 buffer is null byte terminated *only* for the convenience of
	 debugging gdb itself and printing the buffer contents when
	 the buffer contains no embedded nulls.  Gdb does not depend
	 upon the buffer being null byte terminated, it uses the length
	 string instead.  This allows gdb to handle C strings (as well
	 as strings in other languages) with embedded null bytes */

      tokptr = ++tokstart;
      tempbufindex = 0;

      do {
	/* Grow the static temp buffer if necessary, including allocating
	   the first one on demand. */
	if (tempbufindex + 1 >= tempbufsize)
	  {
	    tempbuf = (char *) xrealloc (tempbuf, tempbufsize += 64);
	  }
	switch (*tokptr)
	  {
	  case '\0':
	  case '"':
	    /* Do nothing, loop will terminate. */
	    break;
	  case '\\':
	    tokptr++;
	    c = parse_escape (&tokptr);
	    if (c == -1)
	      {
		continue;
	      }
	    tempbuf[tempbufindex++] = c;
	    break;
	  default:
	    tempbuf[tempbufindex++] = *tokptr++;
	    break;
	  }
      } while ((*tokptr != '"') && (*tokptr != '\0'));
      if (*tokptr++ != '"')
	{
	  error (_("Unterminated string in expression"));
	}
      tempbuf[tempbufindex] = '\0';	/* See note above */
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = tokptr;
      return (STRING_LITERAL);
    }

  if (!(c == '_' || c == '$'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error (_("Invalid character '%c' in expression"), c);

  /* It's a name.  See how long it is.  */
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_'
	|| c == '$'
	|| (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'z')
	|| (c >= 'A' && c <= 'Z')
	|| c == '<');
       )
    {
      if (c == '<')
	{
	  int i = namelen;
	  while (tokstart[++i] && tokstart[i] != '>');
	  if (tokstart[i] == '>')
	    namelen = i;
	}
       c = tokstart[++namelen];
     }

  /* The token "if" terminates the expression and is NOT 
     removed from the input stream.  */
  if (namelen == 2 && tokstart[0] == 'i' && tokstart[1] == 'f')
    {
      return 0;
    }

  lexptr += namelen;

  tryname:

  /* Catch specific keywords.  Should be done with a data structure.  */
  switch (namelen)
    {
    case 7:
      if (DEPRECATED_STREQN (tokstart, "boolean", 7))
	return BOOLEAN;
      break;
    case 6:
      if (DEPRECATED_STREQN (tokstart, "double", 6))      
	return DOUBLE;
      break;
    case 5:
      if (DEPRECATED_STREQN (tokstart, "short", 5))
	return SHORT;
      if (DEPRECATED_STREQN (tokstart, "false", 5))
	{
	  yylval.lval = 0;
	  return BOOLEAN_LITERAL;
	}
      if (DEPRECATED_STREQN (tokstart, "super", 5))
	return SUPER;
      if (DEPRECATED_STREQN (tokstart, "float", 5))
	return FLOAT;
      break;
    case 4:
      if (DEPRECATED_STREQN (tokstart, "long", 4))
	return LONG;
      if (DEPRECATED_STREQN (tokstart, "byte", 4))
	return BYTE;
      if (DEPRECATED_STREQN (tokstart, "char", 4))
	return CHAR;
      if (DEPRECATED_STREQN (tokstart, "true", 4))
	{
	  yylval.lval = 1;
	  return BOOLEAN_LITERAL;
	}
      break;
    case 3:
      if (strncmp (tokstart, "int", 3) == 0)
	return INT;
      if (strncmp (tokstart, "new", 3) == 0)
	return NEW;
      break;
    default:
      break;
    }

  yylval.sval.ptr = tokstart;
  yylval.sval.length = namelen;

  if (*tokstart == '$')
    {
      write_dollar_variable (yylval.sval);
      return VARIABLE;
    }

  /* Input names that aren't symbols but ARE valid hex numbers,
     when the input radix permits them, can be names or numbers
     depending on the parse.  Note we support radixes > 16 here.  */
  if (((tokstart[0] >= 'a' && tokstart[0] < 'a' + input_radix - 10) ||
       (tokstart[0] >= 'A' && tokstart[0] < 'A' + input_radix - 10)))
    {
      YYSTYPE newlval;	/* Its value is ignored.  */
      int hextype = parse_number (tokstart, namelen, 0, &newlval);
      if (hextype == INTEGER_LITERAL)
	return NAME_OR_INT;
    }
  return IDENTIFIER;
}

void
yyerror (msg)
     char *msg;
{
  if (prev_lexptr)
    lexptr = prev_lexptr;

  if (msg)
    error (_("%s: near `%s'"), msg, lexptr);
  else
    error (_("error in expression, near `%s'"), lexptr);
}

static struct type *
java_type_from_name (name)
     struct stoken name;
 
{
  char *tmp = copy_name (name);
  struct type *typ = java_lookup_class (tmp);
  if (typ == NULL || TYPE_CODE (typ) != TYPE_CODE_STRUCT)
    error (_("No class named `%s'"), tmp);
  return typ;
}

/* If NAME is a valid variable name in this scope, push it and return 1.
   Otherwise, return 0. */

static int
push_variable (struct stoken name)
{
  char *tmp = copy_name (name);
  int is_a_field_of_this = 0;
  struct symbol *sym;
  sym = lookup_symbol (tmp, expression_context_block, VAR_DOMAIN,
		       &is_a_field_of_this, (struct symtab **) NULL);
  if (sym && SYMBOL_CLASS (sym) != LOC_TYPEDEF)
    {
      if (symbol_read_needs_frame (sym))
	{
	  if (innermost_block == 0 ||
	      contained_in (block_found, innermost_block))
	    innermost_block = block_found;
	}

      write_exp_elt_opcode (OP_VAR_VALUE);
      /* We want to use the selected frame, not another more inner frame
	 which happens to be in the same block.  */
      write_exp_elt_block (NULL);
      write_exp_elt_sym (sym);
      write_exp_elt_opcode (OP_VAR_VALUE);
      return 1;
    }
  if (is_a_field_of_this)
    {
      /* it hangs off of `this'.  Must not inadvertently convert from a
	 method call to data ref.  */
      if (innermost_block == 0 || 
	  contained_in (block_found, innermost_block))
	innermost_block = block_found;
      write_exp_elt_opcode (OP_THIS);
      write_exp_elt_opcode (OP_THIS);
      write_exp_elt_opcode (STRUCTOP_PTR);
      write_exp_string (name);
      write_exp_elt_opcode (STRUCTOP_PTR);
      return 1;
    }
  return 0;
}

/* Assuming a reference expression has been pushed, emit the
   STRUCTOP_PTR ops to access the field named NAME.  If NAME is a
   qualified name (has '.'), generate a field access for each part. */

static void
push_fieldnames (name)
     struct stoken name;
{
  int i;
  struct stoken token;
  token.ptr = name.ptr;
  for (i = 0;  ;  i++)
    {
      if (i == name.length || name.ptr[i] == '.')
	{
	  /* token.ptr is start of current field name. */
	  token.length = &name.ptr[i] - token.ptr;
	  write_exp_elt_opcode (STRUCTOP_PTR);
	  write_exp_string (token);
	  write_exp_elt_opcode (STRUCTOP_PTR);
	  token.ptr += token.length + 1;
	}
      if (i >= name.length)
	break;
    }
}

/* Helper routine for push_expression_name.
   Handle a qualified name, where DOT_INDEX is the index of the first '.' */

static void
push_qualified_expression_name (struct stoken name, int dot_index)
{
  struct stoken token;
  char *tmp;
  struct type *typ;

  token.ptr = name.ptr;
  token.length = dot_index;

  if (push_variable (token))
    {
      token.ptr = name.ptr + dot_index + 1;
      token.length = name.length - dot_index - 1;
      push_fieldnames (token);
      return;
    }

  token.ptr = name.ptr;
  for (;;)
    {
      token.length = dot_index;
      tmp = copy_name (token);
      typ = java_lookup_class (tmp);
      if (typ != NULL)
	{
	  if (dot_index == name.length)
	    {
	      write_exp_elt_opcode(OP_TYPE);
	      write_exp_elt_type(typ);
	      write_exp_elt_opcode(OP_TYPE);
	      return;
	    }
	  dot_index++;  /* Skip '.' */
	  name.ptr += dot_index;
	  name.length -= dot_index;
	  dot_index = 0;
	  while (dot_index < name.length && name.ptr[dot_index] != '.') 
	    dot_index++;
	  token.ptr = name.ptr;
	  token.length = dot_index;
	  write_exp_elt_opcode (OP_SCOPE);
	  write_exp_elt_type (typ);
	  write_exp_string (token);
	  write_exp_elt_opcode (OP_SCOPE); 
	  if (dot_index < name.length)
	    {
	      dot_index++;
	      name.ptr += dot_index;
	      name.length -= dot_index;
	      push_fieldnames (name);
	    }
	  return;
	}
      else if (dot_index >= name.length)
	break;
      dot_index++;  /* Skip '.' */
      while (dot_index < name.length && name.ptr[dot_index] != '.')
	dot_index++;
    }
  error (_("unknown type `%.*s'"), name.length, name.ptr);
}

/* Handle Name in an expression (or LHS).
   Handle VAR, TYPE, TYPE.FIELD1....FIELDN and VAR.FIELD1....FIELDN. */

static void
push_expression_name (name)
     struct stoken name;
{
  char *tmp;
  struct type *typ;
  char *ptr;
  int i;

  for (i = 0;  i < name.length;  i++)
    {
      if (name.ptr[i] == '.')
	{
	  /* It's a Qualified Expression Name. */
	  push_qualified_expression_name (name, i);
	  return;
	}
    }

  /* It's a Simple Expression Name. */
  
  if (push_variable (name))
    return;
  tmp = copy_name (name);
  typ = java_lookup_class (tmp);
  if (typ != NULL)
    {
      write_exp_elt_opcode(OP_TYPE);
      write_exp_elt_type(typ);
      write_exp_elt_opcode(OP_TYPE);
    }
  else
    {
      struct minimal_symbol *msymbol;

      msymbol = lookup_minimal_symbol (tmp, NULL, NULL);
      if (msymbol != NULL)
	{
	  write_exp_msymbol (msymbol,
			     lookup_function_type (builtin_type_int),
			     builtin_type_int);
	}
      else if (!have_full_symbols () && !have_partial_symbols ())
	error (_("No symbol table is loaded.  Use the \"file\" command"));
      else
	error (_("No symbol \"%s\" in current context"), tmp);
    }

}


/* The following two routines, copy_exp and insert_exp, aren't specific to
   Java, so they could go in parse.c, but their only purpose is to support
   the parsing kludges we use in this file, so maybe it's best to isolate
   them here.  */

/* Copy the expression whose last element is at index ENDPOS - 1 in EXPR
   into a freshly xmalloc'ed struct expression.  Its language_defn is set
   to null.  */
static struct expression *
copy_exp (expr, endpos)
     struct expression *expr;
     int endpos;
{
  int len = length_of_subexp (expr, endpos);
  struct expression *new
    = (struct expression *) xmalloc (sizeof (*new) + EXP_ELEM_TO_BYTES (len));
  new->nelts = len;
  memcpy (new->elts, expr->elts + endpos - len, EXP_ELEM_TO_BYTES (len));
  new->language_defn = 0;

  return new;
}

/* Insert the expression NEW into the current expression (expout) at POS.  */
static void
insert_exp (pos, new)
     int pos;
     struct expression *new;
{
  int newlen = new->nelts;

  /* Grow expout if necessary.  In this function's only use at present,
     this should never be necessary.  */
  if (expout_ptr + newlen > expout_size)
    {
      expout_size = max (expout_size * 2, expout_ptr + newlen + 10);
      expout = (struct expression *)
	xrealloc ((char *) expout, (sizeof (struct expression)
				    + EXP_ELEM_TO_BYTES (expout_size)));
    }

  {
    int i;

    for (i = expout_ptr - 1; i >= pos; i--)
      expout->elts[i + newlen] = expout->elts[i];
  }
  
  memcpy (expout->elts + pos, new->elts, EXP_ELEM_TO_BYTES (newlen));
  expout_ptr += newlen;
}



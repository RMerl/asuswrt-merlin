/* A Bison parser, made from zconf.y, by GNU bison 1.75.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software Foundation, Inc.

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
#define YYBISON	1

/* Pure parsers.  */
#define YYPURE	0

/* Using locations.  */
#define YYLSP_NEEDED 0

/* If NAME_PREFIX is specified substitute the variables and functions
   names.  */
#define yyparse zconfparse
#define yylex   zconflex
#define yyerror zconferror
#define yylval  zconflval
#define yychar  zconfchar
#define yydebug zconfdebug
#define yynerrs zconfnerrs


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     T_MAINMENU = 258,
     T_MENU = 259,
     T_ENDMENU = 260,
     T_SOURCE = 261,
     T_CHOICE = 262,
     T_ENDCHOICE = 263,
     T_COMMENT = 264,
     T_CONFIG = 265,
     T_HELP = 266,
     T_HELPTEXT = 267,
     T_IF = 268,
     T_ENDIF = 269,
     T_DEPENDS = 270,
     T_REQUIRES = 271,
     T_OPTIONAL = 272,
     T_PROMPT = 273,
     T_DEFAULT = 274,
     T_TRISTATE = 275,
     T_BOOLEAN = 276,
     T_INT = 277,
     T_HEX = 278,
     T_WORD = 279,
     T_STRING = 280,
     T_UNEQUAL = 281,
     T_EOF = 282,
     T_EOL = 283,
     T_CLOSE_PAREN = 284,
     T_OPEN_PAREN = 285,
     T_ON = 286,
     T_OR = 287,
     T_AND = 288,
     T_EQUAL = 289,
     T_NOT = 290
   };
#endif
#define T_MAINMENU 258
#define T_MENU 259
#define T_ENDMENU 260
#define T_SOURCE 261
#define T_CHOICE 262
#define T_ENDCHOICE 263
#define T_COMMENT 264
#define T_CONFIG 265
#define T_HELP 266
#define T_HELPTEXT 267
#define T_IF 268
#define T_ENDIF 269
#define T_DEPENDS 270
#define T_REQUIRES 271
#define T_OPTIONAL 272
#define T_PROMPT 273
#define T_DEFAULT 274
#define T_TRISTATE 275
#define T_BOOLEAN 276
#define T_INT 277
#define T_HEX 278
#define T_WORD 279
#define T_STRING 280
#define T_UNEQUAL 281
#define T_EOF 282
#define T_EOL 283
#define T_CLOSE_PAREN 284
#define T_OPEN_PAREN 285
#define T_ON 286
#define T_OR 287
#define T_AND 288
#define T_EQUAL 289
#define T_NOT 290




/* Copy the first part of user declarations.  */
#line 1 "zconf.y"

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


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#ifndef YYSTYPE
#line 33 "zconf.y"
typedef union {
	int token;
	char *string;
	struct symbol *symbol;
	struct expr *expr;
	struct menu *menu;
} yystype;
/* Line 193 of /usr/share/bison/yacc.c.  */
#line 190 "zconf.tab.c"
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif

#ifndef YYLTYPE
typedef struct yyltype
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} yyltype;
# define YYLTYPE yyltype
# define YYLTYPE_IS_TRIVIAL 1
#endif

/* Copy the second part of user declarations.  */
#line 83 "zconf.y"

#define LKC_DIRECT_LINK
#include "lkc.h"


/* Line 213 of /usr/share/bison/yacc.c.  */
#line 215 "zconf.tab.c"

#if !defined(yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
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
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (!defined(yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];	\
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
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined(__STDC__) || defined(__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  2
#define YYLAST   151

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  36
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  39
/* YYNRULES -- Number of rules. */
#define YYNRULES  96
/* YYNRULES -- Number of states. */
#define YYNSTATES  145

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   290

#define YYTRANSLATE(X) \
  ((unsigned)(X) <= YYMAXUTOK ? yytranslate[X] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    13,    17,    19,
      21,    23,    26,    28,    30,    32,    34,    36,    39,    43,
      44,    48,    52,    55,    58,    61,    64,    67,    70,    73,
      77,    81,    83,    87,    89,    94,    97,    98,   102,   106,
     109,   112,   116,   118,   121,   122,   125,   128,   130,   136,
     140,   141,   144,   147,   150,   153,   157,   159,   164,   167,
     168,   171,   174,   177,   181,   184,   187,   190,   194,   197,
     200,   201,   205,   208,   212,   215,   218,   219,   221,   225,
     227,   229,   231,   233,   235,   237,   239,   240,   243,   245,
     249,   253,   257,   260,   264,   268,   270
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      37,     0,    -1,    -1,    37,    38,    -1,    39,    -1,    47,
      -1,    58,    -1,     3,    69,    71,    -1,     5,    -1,    14,
      -1,     8,    -1,     1,    71,    -1,    53,    -1,    63,    -1,
      41,    -1,    61,    -1,    71,    -1,    10,    24,    -1,    40,
      28,    42,    -1,    -1,    42,    43,    28,    -1,    42,    67,
      28,    -1,    42,    65,    -1,    42,    28,    -1,    20,    68,
      -1,    21,    68,    -1,    22,    68,    -1,    23,    68,    -1,
      25,    68,    -1,    18,    69,    72,    -1,    19,    74,    72,
      -1,     7,    -1,    44,    28,    48,    -1,    70,    -1,    45,
      50,    46,    28,    -1,    45,    50,    -1,    -1,    48,    49,
      28,    -1,    48,    67,    28,    -1,    48,    65,    -1,    48,
      28,    -1,    18,    69,    72,    -1,    17,    -1,    19,    74,
      -1,    -1,    50,    39,    -1,    13,    73,    -1,    70,    -1,
      51,    28,    54,    52,    28,    -1,    51,    28,    54,    -1,
      -1,    54,    39,    -1,    54,    58,    -1,    54,    47,    -1,
       4,    69,    -1,    55,    28,    66,    -1,    70,    -1,    56,
      59,    57,    28,    -1,    56,    59,    -1,    -1,    59,    39,
      -1,    59,    58,    -1,    59,    47,    -1,    59,     1,    28,
      -1,     6,    69,    -1,    60,    28,    -1,     9,    69,    -1,
      62,    28,    66,    -1,    11,    28,    -1,    64,    12,    -1,
      -1,    66,    67,    28,    -1,    66,    28,    -1,    15,    31,
      73,    -1,    15,    73,    -1,    16,    73,    -1,    -1,    69,
      -1,    69,    13,    73,    -1,    24,    -1,    25,    -1,     5,
      -1,     8,    -1,    14,    -1,    28,    -1,    27,    -1,    -1,
      13,    73,    -1,    74,    -1,    74,    34,    74,    -1,    74,
      26,    74,    -1,    30,    73,    29,    -1,    35,    73,    -1,
      73,    32,    73,    -1,    73,    33,    73,    -1,    24,    -1,
      25,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,    88,    88,    89,    92,    93,    94,    95,    96,    97,
      98,    99,   102,   104,   105,   106,   107,   113,   121,   127,
     129,   130,   131,   132,   135,   141,   147,   153,   159,   165,
     171,   179,   188,   194,   202,   204,   210,   212,   213,   214,
     215,   218,   224,   230,   237,   239,   244,   254,   262,   264,
     270,   272,   273,   274,   279,   286,   292,   300,   302,   308,
     310,   311,   312,   313,   316,   322,   329,   336,   343,   349,
     356,   357,   358,   361,   366,   371,   379,   381,   385,   390,
     391,   394,   395,   396,   399,   400,   402,   403,   406,   407,
     408,   409,   410,   411,   412,   415,   416
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_MAINMENU", "T_MENU", "T_ENDMENU", 
  "T_SOURCE", "T_CHOICE", "T_ENDCHOICE", "T_COMMENT", "T_CONFIG", 
  "T_HELP", "T_HELPTEXT", "T_IF", "T_ENDIF", "T_DEPENDS", "T_REQUIRES", 
  "T_OPTIONAL", "T_PROMPT", "T_DEFAULT", "T_TRISTATE", "T_BOOLEAN", 
  "T_INT", "T_HEX", "T_WORD", "T_STRING", "T_UNEQUAL", "T_EOF", "T_EOL", 
  "T_CLOSE_PAREN", "T_OPEN_PAREN", "T_ON", "T_OR", "T_AND", "T_EQUAL", 
  "T_NOT", "$accept", "input", "block", "common_block", 
  "config_entry_start", "config_stmt", "config_option_list", 
  "config_option", "choice", "choice_entry", "choice_end", "choice_stmt", 
  "choice_option_list", "choice_option", "choice_block", "if", "if_end", 
  "if_stmt", "if_block", "menu", "menu_entry", "menu_end", "menu_stmt", 
  "menu_block", "source", "source_stmt", "comment", "comment_stmt", 
  "help_start", "help", "depends_list", "depends", "prompt_stmt_opt", 
  "prompt", "end", "nl_or_eof", "if_expr", "expr", "symbol", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    36,    37,    37,    38,    38,    38,    38,    38,    38,
      38,    38,    39,    39,    39,    39,    39,    40,    41,    42,
      42,    42,    42,    42,    43,    43,    43,    43,    43,    43,
      43,    44,    45,    46,    47,    47,    48,    48,    48,    48,
      48,    49,    49,    49,    50,    50,    51,    52,    53,    53,
      54,    54,    54,    54,    55,    56,    57,    58,    58,    59,
      59,    59,    59,    59,    60,    61,    62,    63,    64,    65,
      66,    66,    66,    67,    67,    67,    68,    68,    68,    69,
      69,    70,    70,    70,    71,    71,    72,    72,    73,    73,
      73,    73,    73,    73,    73,    74,    74
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     3,     1,     1,
       1,     2,     1,     1,     1,     1,     1,     2,     3,     0,
       3,     3,     2,     2,     2,     2,     2,     2,     2,     3,
       3,     1,     3,     1,     4,     2,     0,     3,     3,     2,
       2,     3,     1,     2,     0,     2,     2,     1,     5,     3,
       0,     2,     2,     2,     2,     3,     1,     4,     2,     0,
       2,     2,     2,     3,     2,     2,     2,     3,     2,     2,
       0,     3,     2,     3,     2,     2,     0,     1,     3,     1,
       1,     1,     1,     1,     1,     1,     0,     2,     1,     3,
       3,     3,     2,     3,     3,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       2,     0,     1,     0,     0,     0,     8,     0,    31,    10,
       0,     0,     0,     9,    85,    84,     3,     4,     0,    14,
       0,    44,     5,     0,    12,     0,    59,     6,     0,    15,
       0,    13,    16,    11,    79,    80,     0,    54,    64,    66,
      17,    95,    96,     0,     0,    46,    88,    19,    36,    35,
      50,    70,     0,    65,    70,     7,     0,    92,     0,     0,
       0,     0,    18,    32,    81,    82,    83,    45,     0,    33,
      49,    55,     0,    60,    62,     0,    61,    56,    67,    91,
      93,    94,    90,    89,     0,     0,     0,     0,     0,    76,
      76,    76,    76,    76,    23,     0,     0,    22,     0,    42,
       0,     0,    40,     0,    39,     0,    34,    51,    53,     0,
      52,    47,    72,     0,    63,    57,    68,     0,    74,    75,
      86,    86,    24,    77,    25,    26,    27,    28,    20,    69,
      21,    86,    43,    37,    38,    48,    71,    73,     0,    29,
      30,     0,    41,    87,    78
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short yydefgoto[] =
{
      -1,     1,    16,    17,    18,    19,    62,    95,    20,    21,
      68,    22,    63,   103,    49,    23,   109,    24,    70,    25,
      26,    75,    27,    52,    28,    29,    30,    31,    96,    97,
      71,   113,   122,   123,    69,    32,   139,    45,    46
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -120
static const short yypact[] =
{
    -120,    17,  -120,    41,    48,    48,  -120,    48,  -120,  -120,
      48,   -11,    40,  -120,  -120,  -120,  -120,  -120,    13,  -120,
      23,  -120,  -120,    66,  -120,    72,  -120,  -120,    77,  -120,
      81,  -120,  -120,  -120,  -120,  -120,    41,  -120,  -120,  -120,
    -120,  -120,  -120,    40,    40,    57,    59,  -120,  -120,    98,
    -120,  -120,    49,  -120,  -120,  -120,     7,  -120,    40,    40,
      67,    67,    99,   117,  -120,  -120,  -120,  -120,    85,  -120,
      74,    18,    88,  -120,  -120,    95,  -120,  -120,    18,  -120,
      96,  -120,  -120,  -120,   102,    36,    40,    48,    67,    48,
      48,    48,    48,    48,  -120,   103,   129,  -120,   114,  -120,
      48,    67,  -120,   115,  -120,   116,  -120,  -120,  -120,   118,
    -120,  -120,  -120,   119,  -120,  -120,  -120,    40,    57,    57,
     135,   135,  -120,   136,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,   135,  -120,  -120,  -120,  -120,  -120,    57,    40,  -120,
    -120,    40,  -120,    57,    57
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
    -120,  -120,  -120,   -38,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,   -42,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,   -33,  -120,  -120,  -120,  -120,  -120,  -120,    87,
      97,    34,    47,    -1,   -23,     2,  -119,   -43,   -53
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, parse error.  */
#define YYTABLE_NINF -59
static const short yytable[] =
{
      56,    57,   140,    36,    37,    33,    38,    82,    83,    39,
      74,    67,   142,    40,    73,    80,    81,     2,     3,    76,
       4,     5,     6,     7,     8,     9,    10,    11,   108,    77,
      12,    13,   107,    85,    86,   121,    79,   110,    55,    58,
      59,    47,   118,   119,    14,    15,   112,   111,   132,   -58,
      72,    48,   -58,     5,    64,     7,     8,    65,    10,    11,
      41,    42,    12,    66,    41,    42,    43,   117,    14,    15,
      43,    44,    34,    35,   137,    44,    14,    15,     5,    64,
       7,     8,    65,    10,    11,    60,   120,    12,    66,    58,
      59,    41,    42,    61,    50,   143,    98,   105,   144,   131,
      51,    14,    15,    64,     7,    53,    65,    10,    11,    54,
      84,    12,    66,   106,    85,    86,   114,    87,    88,    89,
      90,    91,    92,   115,    93,    14,    15,    94,    84,    59,
     116,   128,    85,    86,    99,   100,   101,   124,   125,   126,
     127,   129,   130,   133,   134,   102,   135,   136,   138,   141,
     104,    78
};

static const unsigned char yycheck[] =
{
      43,    44,   121,     4,     5,     3,     7,    60,    61,    10,
      52,    49,   131,    24,    52,    58,    59,     0,     1,    52,
       3,     4,     5,     6,     7,     8,     9,    10,    70,    52,
      13,    14,    70,    15,    16,    88,    29,    70,    36,    32,
      33,    28,    85,    86,    27,    28,    28,    70,   101,     0,
       1,    28,     3,     4,     5,     6,     7,     8,     9,    10,
      24,    25,    13,    14,    24,    25,    30,    31,    27,    28,
      30,    35,    24,    25,   117,    35,    27,    28,     4,     5,
       6,     7,     8,     9,    10,    26,    87,    13,    14,    32,
      33,    24,    25,    34,    28,   138,    62,    63,   141,   100,
      28,    27,    28,     5,     6,    28,     8,     9,    10,    28,
      11,    13,    14,    28,    15,    16,    28,    18,    19,    20,
      21,    22,    23,    28,    25,    27,    28,    28,    11,    33,
      28,    28,    15,    16,    17,    18,    19,    90,    91,    92,
      93,    12,    28,    28,    28,    28,    28,    28,    13,    13,
      63,    54
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    37,     0,     1,     3,     4,     5,     6,     7,     8,
       9,    10,    13,    14,    27,    28,    38,    39,    40,    41,
      44,    45,    47,    51,    53,    55,    56,    58,    60,    61,
      62,    63,    71,    71,    24,    25,    69,    69,    69,    69,
      24,    24,    25,    30,    35,    73,    74,    28,    28,    50,
      28,    28,    59,    28,    28,    71,    73,    73,    32,    33,
      26,    34,    42,    48,     5,     8,    14,    39,    46,    70,
      54,    66,     1,    39,    47,    57,    58,    70,    66,    29,
      73,    73,    74,    74,    11,    15,    16,    18,    19,    20,
      21,    22,    23,    25,    28,    43,    64,    65,    67,    17,
      18,    19,    28,    49,    65,    67,    28,    39,    47,    52,
      58,    70,    28,    67,    28,    28,    28,    31,    73,    73,
      69,    74,    68,    69,    68,    68,    68,    68,    28,    12,
      28,    69,    74,    28,    28,    28,    28,    73,    13,    72,
      72,    13,    72,    73,    73
};

#if !defined(YYSIZE_T) && defined(__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if !defined(YYSIZE_T) && defined(size_t)
# define YYSIZE_T size_t
#endif
#if !defined(YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if !defined(YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrlab1

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
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)           \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#define YYLEX	yylex ()

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
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
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

#if YYMAXDEPTH == 0
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
/*-----------------------------.
| Print this symbol on YYOUT.  |
`-----------------------------*/

static void
#if defined(__STDC__) || defined(__cplusplus)
yysymprint (FILE* yyout, int yytype, YYSTYPE yyvalue)
#else
yysymprint (yyout, yytype, yyvalue)
    FILE* yyout;
    int yytype;
    YYSTYPE yyvalue;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvalue;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyout, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyout, yytoknum[yytype], yyvalue);
# endif
    }
  else
    YYFPRINTF (yyout, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyout, ")");
}
#endif /* YYDEBUG. */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
#if defined(__STDC__) || defined(__cplusplus)
yydestruct (int yytype, YYSTYPE yyvalue)
#else
yydestruct (yytype, yyvalue)
    int yytype;
    YYSTYPE yyvalue;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvalue;

  switch (yytype)
    {
      default:
        break;
    }
}



/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of parse errors so far.  */
int yynerrs;


int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

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

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
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
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
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

      if (yyssp >= yyss + yystacksize - 1)
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

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with.  */

  if (yychar <= 0)		/* This means end of input.  */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more.  */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

      /* We have to keep this `#if YYDEBUG', since we use variables
	 which are defined only if `YYDEBUG' is set.  */
      YYDPRINTF ((stderr, "Next token is "));
      YYDSYMPRINT ((stderr, yychar1, yylval));
      YYDPRINTF ((stderr, "\n"));
    }

  /* If the proper action on seeing token YYCHAR1 is to reduce or to
     detect an error, take that action.  */
  yyn += yychar1;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yychar1)
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
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

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



#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn - 1, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] >= 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif
  switch (yyn)
    {
        case 8:
#line 96 "zconf.y"
    { zconfprint("unexpected 'endmenu' statement"); }
    break;

  case 9:
#line 97 "zconf.y"
    { zconfprint("unexpected 'endif' statement"); }
    break;

  case 10:
#line 98 "zconf.y"
    { zconfprint("unexpected 'endchoice' statement"); }
    break;

  case 11:
#line 99 "zconf.y"
    { zconfprint("syntax error"); yyerrok; }
    break;

  case 17:
#line 114 "zconf.y"
    {
	struct symbol *sym = sym_lookup(yyvsp[0].string, 0);
	sym->flags |= SYMBOL_OPTIONAL;
	menu_add_entry(sym);
	printd(DEBUG_PARSE, "%s:%d:config %s\n", zconf_curname(), zconf_lineno(), yyvsp[0].string);
}
    break;

  case 18:
#line 122 "zconf.y"
    {
	menu_end_entry();
	printd(DEBUG_PARSE, "%s:%d:endconfig\n", zconf_curname(), zconf_lineno());
}
    break;

  case 23:
#line 133 "zconf.y"
    { }
    break;

  case 24:
#line 136 "zconf.y"
    {
	menu_set_type(S_TRISTATE);
	printd(DEBUG_PARSE, "%s:%d:tristate\n", zconf_curname(), zconf_lineno());
}
    break;

  case 25:
#line 142 "zconf.y"
    {
	menu_set_type(S_BOOLEAN);
	printd(DEBUG_PARSE, "%s:%d:boolean\n", zconf_curname(), zconf_lineno());
}
    break;

  case 26:
#line 148 "zconf.y"
    {
	menu_set_type(S_INT);
	printd(DEBUG_PARSE, "%s:%d:int\n", zconf_curname(), zconf_lineno());
}
    break;

  case 27:
#line 154 "zconf.y"
    {
	menu_set_type(S_HEX);
	printd(DEBUG_PARSE, "%s:%d:hex\n", zconf_curname(), zconf_lineno());
}
    break;

  case 28:
#line 160 "zconf.y"
    {
	menu_set_type(S_STRING);
	printd(DEBUG_PARSE, "%s:%d:string\n", zconf_curname(), zconf_lineno());
}
    break;

  case 29:
#line 166 "zconf.y"
    {
	menu_add_prop(P_PROMPT, yyvsp[-1].string, NULL, yyvsp[0].expr);
	printd(DEBUG_PARSE, "%s:%d:prompt\n", zconf_curname(), zconf_lineno());
}
    break;

  case 30:
#line 172 "zconf.y"
    {
	menu_add_prop(P_DEFAULT, NULL, yyvsp[-1].symbol, yyvsp[0].expr);
	printd(DEBUG_PARSE, "%s:%d:default\n", zconf_curname(), zconf_lineno());
}
    break;

  case 31:
#line 180 "zconf.y"
    {
	struct symbol *sym = sym_lookup(NULL, 0);
	sym->flags |= SYMBOL_CHOICE;
	menu_add_entry(sym);
	menu_add_prop(P_CHOICE, NULL, NULL, NULL);
	printd(DEBUG_PARSE, "%s:%d:choice\n", zconf_curname(), zconf_lineno());
}
    break;

  case 32:
#line 189 "zconf.y"
    {
	menu_end_entry();
	menu_add_menu();
}
    break;

  case 33:
#line 195 "zconf.y"
    {
	if (zconf_endtoken(yyvsp[0].token, T_CHOICE, T_ENDCHOICE)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endchoice\n", zconf_curname(), zconf_lineno());
	}
}
    break;

  case 35:
#line 205 "zconf.y"
    {
	printf("%s:%d: missing 'endchoice' for this 'choice' statement\n", current_menu->file->name, current_menu->lineno);
	zconfnerrs++;
}
    break;

  case 41:
#line 219 "zconf.y"
    {
	menu_add_prop(P_PROMPT, yyvsp[-1].string, NULL, yyvsp[0].expr);
	printd(DEBUG_PARSE, "%s:%d:prompt\n", zconf_curname(), zconf_lineno());
}
    break;

  case 42:
#line 225 "zconf.y"
    {
	current_entry->sym->flags |= SYMBOL_OPTIONAL;
	printd(DEBUG_PARSE, "%s:%d:optional\n", zconf_curname(), zconf_lineno());
}
    break;

  case 43:
#line 231 "zconf.y"
    {
	menu_add_prop(P_DEFAULT, NULL, yyvsp[0].symbol, NULL);
	//current_choice->prop->def = ;
	printd(DEBUG_PARSE, "%s:%d:default\n", zconf_curname(), zconf_lineno());
}
    break;

  case 46:
#line 245 "zconf.y"
    {
	printd(DEBUG_PARSE, "%s:%d:if\n", zconf_curname(), zconf_lineno());
	menu_add_entry(NULL);
	//current_entry->prompt = menu_add_prop(T_IF, NULL, NULL, );
	menu_add_dep(yyvsp[0].expr);
	menu_end_entry();
	menu_add_menu();
}
    break;

  case 47:
#line 255 "zconf.y"
    {
	if (zconf_endtoken(yyvsp[0].token, T_IF, T_ENDIF)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endif\n", zconf_curname(), zconf_lineno());
	}
}
    break;

  case 49:
#line 265 "zconf.y"
    {
	printf("%s:%d: missing 'endif' for this 'if' statement\n", current_menu->file->name, current_menu->lineno);
	zconfnerrs++;
}
    break;

  case 54:
#line 280 "zconf.y"
    {
	menu_add_entry(NULL);
	menu_add_prop(P_MENU, yyvsp[0].string, NULL, NULL);
	printd(DEBUG_PARSE, "%s:%d:menu\n", zconf_curname(), zconf_lineno());
}
    break;

  case 55:
#line 287 "zconf.y"
    {
	menu_end_entry();
	menu_add_menu();
}
    break;

  case 56:
#line 293 "zconf.y"
    {
	if (zconf_endtoken(yyvsp[0].token, T_MENU, T_ENDMENU)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endmenu\n", zconf_curname(), zconf_lineno());
	}
}
    break;

  case 58:
#line 303 "zconf.y"
    {
	printf("%s:%d: missing 'endmenu' for this 'menu' statement\n", current_menu->file->name, current_menu->lineno);
	zconfnerrs++;
}
    break;

  case 63:
#line 313 "zconf.y"
    { zconfprint("invalid menu option"); yyerrok; }
    break;

  case 64:
#line 317 "zconf.y"
    {
	yyval.string = yyvsp[0].string;
	printd(DEBUG_PARSE, "%s:%d:source %s\n", zconf_curname(), zconf_lineno(), yyvsp[0].string);
}
    break;

  case 65:
#line 323 "zconf.y"
    {
	zconf_nextfile(yyvsp[-1].string);
}
    break;

  case 66:
#line 330 "zconf.y"
    {
	menu_add_entry(NULL);
	menu_add_prop(P_COMMENT, yyvsp[0].string, NULL, NULL);
	printd(DEBUG_PARSE, "%s:%d:comment\n", zconf_curname(), zconf_lineno());
}
    break;

  case 67:
#line 337 "zconf.y"
    {
	menu_end_entry();
}
    break;

  case 68:
#line 344 "zconf.y"
    {
	printd(DEBUG_PARSE, "%s:%d:help\n", zconf_curname(), zconf_lineno());
	zconf_starthelp();
}
    break;

  case 69:
#line 350 "zconf.y"
    {
	current_entry->sym->help = yyvsp[0].string;
}
    break;

  case 72:
#line 359 "zconf.y"
    { }
    break;

  case 73:
#line 362 "zconf.y"
    {
	menu_add_dep(yyvsp[0].expr);
	printd(DEBUG_PARSE, "%s:%d:depends on\n", zconf_curname(), zconf_lineno());
}
    break;

  case 74:
#line 367 "zconf.y"
    {
	menu_add_dep(yyvsp[0].expr);
	printd(DEBUG_PARSE, "%s:%d:depends\n", zconf_curname(), zconf_lineno());
}
    break;

  case 75:
#line 372 "zconf.y"
    {
	menu_add_dep(yyvsp[0].expr);
	printd(DEBUG_PARSE, "%s:%d:requires\n", zconf_curname(), zconf_lineno());
}
    break;

  case 77:
#line 382 "zconf.y"
    {
	menu_add_prop(P_PROMPT, yyvsp[0].string, NULL, NULL);
}
    break;

  case 78:
#line 386 "zconf.y"
    {
	menu_add_prop(P_PROMPT, yyvsp[-2].string, NULL, yyvsp[0].expr);
}
    break;

  case 81:
#line 394 "zconf.y"
    { yyval.token = T_ENDMENU; }
    break;

  case 82:
#line 395 "zconf.y"
    { yyval.token = T_ENDCHOICE; }
    break;

  case 83:
#line 396 "zconf.y"
    { yyval.token = T_ENDIF; }
    break;

  case 86:
#line 402 "zconf.y"
    { yyval.expr = NULL; }
    break;

  case 87:
#line 403 "zconf.y"
    { yyval.expr = yyvsp[0].expr; }
    break;

  case 88:
#line 406 "zconf.y"
    { yyval.expr = expr_alloc_symbol(yyvsp[0].symbol); }
    break;

  case 89:
#line 407 "zconf.y"
    { yyval.expr = expr_alloc_comp(E_EQUAL, yyvsp[-2].symbol, yyvsp[0].symbol); }
    break;

  case 90:
#line 408 "zconf.y"
    { yyval.expr = expr_alloc_comp(E_UNEQUAL, yyvsp[-2].symbol, yyvsp[0].symbol); }
    break;

  case 91:
#line 409 "zconf.y"
    { yyval.expr = yyvsp[-1].expr; }
    break;

  case 92:
#line 410 "zconf.y"
    { yyval.expr = expr_alloc_one(E_NOT, yyvsp[0].expr); }
    break;

  case 93:
#line 411 "zconf.y"
    { yyval.expr = expr_alloc_two(E_OR, yyvsp[-2].expr, yyvsp[0].expr); }
    break;

  case 94:
#line 412 "zconf.y"
    { yyval.expr = expr_alloc_two(E_AND, yyvsp[-2].expr, yyvsp[0].expr); }
    break;

  case 95:
#line 415 "zconf.y"
    { yyval.symbol = sym_lookup(yyvsp[0].string, 0); free(yyvsp[0].string); }
    break;

  case 96:
#line 416 "zconf.y"
    { yyval.symbol = sym_lookup(yyvsp[0].string, 1); free(yyvsp[0].string); }
    break;


    }

/* Line 1016 of /usr/share/bison/yacc.c.  */
#line 1565 "zconf.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;


#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

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
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        {
	  /* Pop the error token.  */
          YYPOPSTACK;
	  /* Pop the rest of the stack.  */
	  while (yyssp > yyss)
	    {
	      YYDPRINTF ((stderr, "Error: popping "));
	      YYDSYMPRINT ((stderr,
			    yystos[*yyssp],
			    *yyvsp));
	      YYDPRINTF ((stderr, "\n"));
	      yydestruct (yystos[*yyssp], *yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yydestruct (yychar1, yylval);
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

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

      YYDPRINTF ((stderr, "Error: popping "));
      YYDSYMPRINT ((stderr,
		    yystos[*yyssp], *yyvsp));
      YYDPRINTF ((stderr, "\n"));

      yydestruct (yystos[yystate], *yyvsp);
      yyvsp--;
      yystate = *--yyssp;


#if YYDEBUG
      if (yydebug)
	{
	  short *yyssp1 = yyss - 1;
	  YYFPRINTF (stderr, "Error: state stack now");
	  while (yyssp1 != yyssp)
	    YYFPRINTF (stderr, " %d", *++yyssp1);
	  YYFPRINTF (stderr, "\n");
	}
#endif
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


#line 419 "zconf.y"


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


/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     tAGO = 258,
     tDST = 259,
     tDAY = 260,
     tDAY_UNIT = 261,
     tDAYZONE = 262,
     tHOUR_UNIT = 263,
     tLOCAL_ZONE = 264,
     tMERIDIAN = 265,
     tMINUTE_UNIT = 266,
     tMONTH = 267,
     tMONTH_UNIT = 268,
     tSEC_UNIT = 269,
     tYEAR_UNIT = 270,
     tZONE = 271,
     tSNUMBER = 272,
     tUNUMBER = 273
   };
#endif
/* Tokens.  */
#define tAGO 258
#define tDST 259
#define tDAY 260
#define tDAY_UNIT 261
#define tDAYZONE 262
#define tHOUR_UNIT 263
#define tLOCAL_ZONE 264
#define tMERIDIAN 265
#define tMINUTE_UNIT 266
#define tMONTH 267
#define tMONTH_UNIT 268
#define tSEC_UNIT 269
#define tYEAR_UNIT 270
#define tZONE 271
#define tSNUMBER 272
#define tUNUMBER 273




/* Copy the first part of user declarations.  */
#line 1 "getdate.y"

/* Parse a string into an internal time stamp.
   Copyright (C) 1999, 2000, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

/* Originally written by Steven M. Bellovin <smb@research.att.com> while
   at the University of North Carolina at Chapel Hill.  Later tweaked by
   a couple of people on Usenet.  Completely overhauled by Rich $alz
   <rsalz@bbn.com> and Jim Berets <jberets@bbn.com> in August, 1990.

   Modified by Paul Eggert <eggert@twinsun.com> in August 1999 to do
   the right thing about local DST.  Unlike previous versions, this
   version is reentrant.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
# ifdef HAVE_ALLOCA_H
#  include <alloca.h>
# endif
#endif

/* Since the code of getdate.y is not included in the Emacs executable
   itself, there is no need to #define static in this file.  Even if
   the code were included in the Emacs executable, it probably
   wouldn't do any harm to #undef it here; this will only cause
   problems if we try to write to a static variable, which I don't
   think this code needs to do.  */
#ifdef emacs
# undef static
#endif

#include <ctype.h>
#include <string.h>

#if HAVE_STDLIB_H
# include <stdlib.h> /* for `free'; used by Bison 1.27 */
#endif

#if STDC_HEADERS || (! defined isascii && ! HAVE_ISASCII)
# define IN_CTYPE_DOMAIN(c) 1
#else
# define IN_CTYPE_DOMAIN(c) isascii (c)
#endif

#define ISSPACE(c) (IN_CTYPE_DOMAIN (c) && isspace (c))
#define ISALPHA(c) (IN_CTYPE_DOMAIN (c) && isalpha (c))
#define ISLOWER(c) (IN_CTYPE_DOMAIN (c) && islower (c))
#define ISDIGIT_LOCALE(c) (IN_CTYPE_DOMAIN (c) && isdigit (c))

/* ISDIGIT differs from ISDIGIT_LOCALE, as follows:
   - Its arg may be any int or unsigned int; it need not be an unsigned char.
   - It's guaranteed to evaluate its argument exactly once.
   - It's typically faster.
   POSIX says that only '0' through '9' are digits.  Prefer ISDIGIT to
   ISDIGIT_LOCALE unless it's important to use the locale's definition
   of `digit' even when the host does not conform to POSIX.  */
#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
#endif

#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8) || __STRICT_ANSI__
# define __attribute__(x)
#endif

#ifndef ATTRIBUTE_UNUSED
# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif

#define EPOCH_YEAR 1970
#define TM_YEAR_BASE 1900

#define HOUR(x) ((x) * 60)

/* An integer value, and the number of digits in its textual
   representation.  */
typedef struct
{
  int value;
  int digits;
} textint;

/* An entry in the lexical lookup table.  */
typedef struct
{
  char const *name;
  int type;
  int value;
} table;

/* Meridian: am, pm, or 24-hour style.  */
enum { MERam, MERpm, MER24 };

/* Information passed to and from the parser.  */
typedef struct
{
  /* The input string remaining to be parsed. */
  const char *input;

  /* N, if this is the Nth Tuesday.  */
  int day_ordinal;

  /* Day of week; Sunday is 0.  */
  int day_number;

  /* tm_isdst flag for the local zone.  */
  int local_isdst;

  /* Time zone, in minutes east of UTC.  */
  int time_zone;

  /* Style used for time.  */
  int meridian;

  /* Gregorian year, month, day, hour, minutes, and seconds.  */
  textint year;
  int month;
  int day;
  int hour;
  int minutes;
  int seconds;

  /* Relative year, month, day, hour, minutes, and seconds.  */
  int rel_year;
  int rel_month;
  int rel_day;
  int rel_hour;
  int rel_minutes;
  int rel_seconds;

  /* Counts of nonterminals of various flavors parsed so far.  */
  int dates_seen;
  int days_seen;
  int local_zones_seen;
  int rels_seen;
  int times_seen;
  int zones_seen;

  /* Table of local time zone abbrevations, terminated by a null entry.  */
  table local_time_zone_table[3];
} parser_control;

#define PC (* (parser_control *) parm)
#define YYLEX_PARAM parm
#define YYPARSE_PARAM parm



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

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 169 "getdate.y"
{
  int intval;
  textint textintval;
}
/* Line 187 of yacc.c.  */
#line 298 "getdate.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */
#line 174 "getdate.y"


static int yyerror(const char *);
static int yylex(YYSTYPE *, parser_control *);



/* Line 216 of yacc.c.  */
#line 317 "getdate.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
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
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   52

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  22
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  12
/* YYNRULES -- Number of rules.  */
#define YYNRULES  54
/* YYNRULES -- Number of states.  */
#define YYNSTATES  64

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   273

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    20,     2,     2,    21,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    19,     2,
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
      15,    16,    17,    18
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    13,    15,    17,
      19,    21,    24,    29,    34,    41,    48,    50,    53,    55,
      57,    60,    62,    65,    68,    72,    78,    82,    86,    89,
      94,    97,   101,   104,   106,   109,   112,   114,   117,   120,
     122,   125,   128,   130,   133,   136,   138,   141,   144,   146,
     149,   152,   154,   156,   157
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      23,     0,    -1,    -1,    23,    24,    -1,    25,    -1,    26,
      -1,    27,    -1,    29,    -1,    28,    -1,    30,    -1,    32,
      -1,    18,    10,    -1,    18,    19,    18,    33,    -1,    18,
      19,    18,    17,    -1,    18,    19,    18,    19,    18,    33,
      -1,    18,    19,    18,    19,    18,    17,    -1,     9,    -1,
       9,     4,    -1,    16,    -1,     7,    -1,    16,     4,    -1,
       5,    -1,     5,    20,    -1,    18,     5,    -1,    18,    21,
      18,    -1,    18,    21,    18,    21,    18,    -1,    18,    17,
      17,    -1,    18,    12,    17,    -1,    12,    18,    -1,    12,
      18,    20,    18,    -1,    18,    12,    -1,    18,    12,    18,
      -1,    31,     3,    -1,    31,    -1,    18,    15,    -1,    17,
      15,    -1,    15,    -1,    18,    13,    -1,    17,    13,    -1,
      13,    -1,    18,     6,    -1,    17,     6,    -1,     6,    -1,
      18,     8,    -1,    17,     8,    -1,     8,    -1,    18,    11,
      -1,    17,    11,    -1,    11,    -1,    18,    14,    -1,    17,
      14,    -1,    14,    -1,    18,    -1,    -1,    10,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   192,   192,   194,   198,   200,   202,   204,   206,   208,
     210,   214,   221,   228,   236,   243,   255,   257,   262,   264,
     266,   271,   276,   281,   289,   294,   314,   321,   329,   334,
     340,   345,   354,   363,   367,   369,   371,   373,   375,   377,
     379,   381,   383,   385,   387,   389,   391,   393,   395,   397,
     399,   401,   406,   443,   444
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "tAGO", "tDST", "tDAY", "tDAY_UNIT",
  "tDAYZONE", "tHOUR_UNIT", "tLOCAL_ZONE", "tMERIDIAN", "tMINUTE_UNIT",
  "tMONTH", "tMONTH_UNIT", "tSEC_UNIT", "tYEAR_UNIT", "tZONE", "tSNUMBER",
  "tUNUMBER", "':'", "','", "'/'", "$accept", "spec", "item", "time",
  "local_zone", "zone", "day", "date", "rel", "relunit", "number",
  "o_merid", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,    58,
      44,    47
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    22,    23,    23,    24,    24,    24,    24,    24,    24,
      24,    25,    25,    25,    25,    25,    26,    26,    27,    27,
      27,    28,    28,    28,    29,    29,    29,    29,    29,    29,
      29,    29,    30,    30,    31,    31,    31,    31,    31,    31,
      31,    31,    31,    31,    31,    31,    31,    31,    31,    31,
      31,    31,    32,    33,    33
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     2,     4,     4,     6,     6,     1,     2,     1,     1,
       2,     1,     2,     2,     3,     5,     3,     3,     2,     4,
       2,     3,     2,     1,     2,     2,     1,     2,     2,     1,
       2,     2,     1,     2,     2,     1,     2,     2,     1,     2,
       2,     1,     1,     0,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     0,     1,    21,    42,    19,    45,    16,    48,     0,
      39,    51,    36,    18,     0,    52,     3,     4,     5,     6,
       8,     7,     9,    33,    10,    22,    17,    28,    20,    41,
      44,    47,    38,    50,    35,    23,    40,    43,    11,    46,
      30,    37,    49,    34,     0,     0,     0,    32,     0,    27,
      31,    26,    53,    24,    29,    54,    13,     0,    12,     0,
      53,    25,    15,    14
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    58
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -17
static const yytype_int8 yypact[] =
{
     -17,     0,   -17,     1,   -17,   -17,   -17,    19,   -17,   -14,
     -17,   -17,   -17,    32,    26,    14,   -17,   -17,   -17,   -17,
     -17,   -17,   -17,    27,   -17,   -17,   -17,    22,   -17,   -17,
     -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,
     -16,   -17,   -17,   -17,    29,    25,    30,   -17,    31,   -17,
     -17,   -17,    28,    23,   -17,   -17,   -17,    33,   -17,    34,
      -7,   -17,   -17,   -17
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,
     -17,   -10
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
       2,    49,    50,    55,    27,     3,     4,     5,     6,     7,
      62,     8,     9,    10,    11,    12,    13,    14,    15,    35,
      36,    25,    37,    26,    38,    39,    40,    41,    42,    43,
      47,    44,    29,    45,    30,    46,    28,    31,    55,    32,
      33,    34,    48,    52,    59,    56,    51,    57,    53,    54,
      63,    60,    61
};

static const yytype_uint8 yycheck[] =
{
       0,    17,    18,    10,    18,     5,     6,     7,     8,     9,
      17,    11,    12,    13,    14,    15,    16,    17,    18,     5,
       6,    20,     8,     4,    10,    11,    12,    13,    14,    15,
       3,    17,     6,    19,     8,    21,     4,    11,    10,    13,
      14,    15,    20,    18,    21,    17,    17,    19,    18,    18,
      60,    18,    18
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    23,     0,     5,     6,     7,     8,     9,    11,    12,
      13,    14,    15,    16,    17,    18,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    20,     4,    18,     4,     6,
       8,    11,    13,    14,    15,     5,     6,     8,    10,    11,
      12,    13,    14,    15,    17,    19,    21,     3,    20,    17,
      18,    17,    18,    18,    18,    10,    17,    19,    33,    21,
      18,    18,    17,    33
};

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
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval)
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
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
				       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
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
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */






/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  /* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;

  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

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
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
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

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
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
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
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

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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
#line 199 "getdate.y"
    { PC.times_seen++; ;}
    break;

  case 5:
#line 201 "getdate.y"
    { PC.local_zones_seen++; ;}
    break;

  case 6:
#line 203 "getdate.y"
    { PC.zones_seen++; ;}
    break;

  case 7:
#line 205 "getdate.y"
    { PC.dates_seen++; ;}
    break;

  case 8:
#line 207 "getdate.y"
    { PC.days_seen++; ;}
    break;

  case 9:
#line 209 "getdate.y"
    { PC.rels_seen++; ;}
    break;

  case 11:
#line 215 "getdate.y"
    {
	PC.hour = (yyvsp[(1) - (2)].textintval).value;
	PC.minutes = 0;
	PC.seconds = 0;
	PC.meridian = (yyvsp[(2) - (2)].intval);
      ;}
    break;

  case 12:
#line 222 "getdate.y"
    {
	PC.hour = (yyvsp[(1) - (4)].textintval).value;
	PC.minutes = (yyvsp[(3) - (4)].textintval).value;
	PC.seconds = 0;
	PC.meridian = (yyvsp[(4) - (4)].intval);
      ;}
    break;

  case 13:
#line 229 "getdate.y"
    {
	PC.hour = (yyvsp[(1) - (4)].textintval).value;
	PC.minutes = (yyvsp[(3) - (4)].textintval).value;
	PC.meridian = MER24;
	PC.zones_seen++;
	PC.time_zone = (yyvsp[(4) - (4)].textintval).value % 100 + ((yyvsp[(4) - (4)].textintval).value / 100) * 60;
      ;}
    break;

  case 14:
#line 237 "getdate.y"
    {
	PC.hour = (yyvsp[(1) - (6)].textintval).value;
	PC.minutes = (yyvsp[(3) - (6)].textintval).value;
	PC.seconds = (yyvsp[(5) - (6)].textintval).value;
	PC.meridian = (yyvsp[(6) - (6)].intval);
      ;}
    break;

  case 15:
#line 244 "getdate.y"
    {
	PC.hour = (yyvsp[(1) - (6)].textintval).value;
	PC.minutes = (yyvsp[(3) - (6)].textintval).value;
	PC.seconds = (yyvsp[(5) - (6)].textintval).value;
	PC.meridian = MER24;
	PC.zones_seen++;
	PC.time_zone = (yyvsp[(6) - (6)].textintval).value % 100 + ((yyvsp[(6) - (6)].textintval).value / 100) * 60;
      ;}
    break;

  case 16:
#line 256 "getdate.y"
    { PC.local_isdst = (yyvsp[(1) - (1)].intval); ;}
    break;

  case 17:
#line 258 "getdate.y"
    { PC.local_isdst = (yyvsp[(1) - (2)].intval) < 0 ? 1 : (yyvsp[(1) - (2)].intval) + 1; ;}
    break;

  case 18:
#line 263 "getdate.y"
    { PC.time_zone = (yyvsp[(1) - (1)].intval); ;}
    break;

  case 19:
#line 265 "getdate.y"
    { PC.time_zone = (yyvsp[(1) - (1)].intval) + 60; ;}
    break;

  case 20:
#line 267 "getdate.y"
    { PC.time_zone = (yyvsp[(1) - (2)].intval) + 60; ;}
    break;

  case 21:
#line 272 "getdate.y"
    {
	PC.day_ordinal = 1;
	PC.day_number = (yyvsp[(1) - (1)].intval);
      ;}
    break;

  case 22:
#line 277 "getdate.y"
    {
	PC.day_ordinal = 1;
	PC.day_number = (yyvsp[(1) - (2)].intval);
      ;}
    break;

  case 23:
#line 282 "getdate.y"
    {
	PC.day_ordinal = (yyvsp[(1) - (2)].textintval).value;
	PC.day_number = (yyvsp[(2) - (2)].intval);
      ;}
    break;

  case 24:
#line 290 "getdate.y"
    {
	PC.month = (yyvsp[(1) - (3)].textintval).value;
	PC.day = (yyvsp[(3) - (3)].textintval).value;
      ;}
    break;

  case 25:
#line 295 "getdate.y"
    {
	/* Interpret as YYYY/MM/DD if the first value has 4 or more digits,
	   otherwise as MM/DD/YY.
	   The goal in recognizing YYYY/MM/DD is solely to support legacy
	   machine-generated dates like those in an RCS log listing.  If
	   you want portability, use the ISO 8601 format.  */
	if (4 <= (yyvsp[(1) - (5)].textintval).digits)
	  {
	    PC.year = (yyvsp[(1) - (5)].textintval);
	    PC.month = (yyvsp[(3) - (5)].textintval).value;
	    PC.day = (yyvsp[(5) - (5)].textintval).value;
	  }
	else
	  {
	    PC.month = (yyvsp[(1) - (5)].textintval).value;
	    PC.day = (yyvsp[(3) - (5)].textintval).value;
	    PC.year = (yyvsp[(5) - (5)].textintval);
	  }
      ;}
    break;

  case 26:
#line 315 "getdate.y"
    {
	/* ISO 8601 format.  YYYY-MM-DD.  */
	PC.year = (yyvsp[(1) - (3)].textintval);
	PC.month = -(yyvsp[(2) - (3)].textintval).value;
	PC.day = -(yyvsp[(3) - (3)].textintval).value;
      ;}
    break;

  case 27:
#line 322 "getdate.y"
    {
	/* e.g. 17-JUN-1992.  */
	PC.day = (yyvsp[(1) - (3)].textintval).value;
	PC.month = (yyvsp[(2) - (3)].intval);
	PC.year.value = -(yyvsp[(3) - (3)].textintval).value;
	PC.year.digits = (yyvsp[(3) - (3)].textintval).digits;
      ;}
    break;

  case 28:
#line 330 "getdate.y"
    {
	PC.month = (yyvsp[(1) - (2)].intval);
	PC.day = (yyvsp[(2) - (2)].textintval).value;
      ;}
    break;

  case 29:
#line 335 "getdate.y"
    {
	PC.month = (yyvsp[(1) - (4)].intval);
	PC.day = (yyvsp[(2) - (4)].textintval).value;
	PC.year = (yyvsp[(4) - (4)].textintval);
      ;}
    break;

  case 30:
#line 341 "getdate.y"
    {
	PC.day = (yyvsp[(1) - (2)].textintval).value;
	PC.month = (yyvsp[(2) - (2)].intval);
      ;}
    break;

  case 31:
#line 346 "getdate.y"
    {
	PC.day = (yyvsp[(1) - (3)].textintval).value;
	PC.month = (yyvsp[(2) - (3)].intval);
	PC.year = (yyvsp[(3) - (3)].textintval);
      ;}
    break;

  case 32:
#line 355 "getdate.y"
    {
	PC.rel_seconds = -PC.rel_seconds;
	PC.rel_minutes = -PC.rel_minutes;
	PC.rel_hour = -PC.rel_hour;
	PC.rel_day = -PC.rel_day;
	PC.rel_month = -PC.rel_month;
	PC.rel_year = -PC.rel_year;
      ;}
    break;

  case 34:
#line 368 "getdate.y"
    { PC.rel_year += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 35:
#line 370 "getdate.y"
    { PC.rel_year += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 36:
#line 372 "getdate.y"
    { PC.rel_year += (yyvsp[(1) - (1)].intval); ;}
    break;

  case 37:
#line 374 "getdate.y"
    { PC.rel_month += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 38:
#line 376 "getdate.y"
    { PC.rel_month += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 39:
#line 378 "getdate.y"
    { PC.rel_month += (yyvsp[(1) - (1)].intval); ;}
    break;

  case 40:
#line 380 "getdate.y"
    { PC.rel_day += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 41:
#line 382 "getdate.y"
    { PC.rel_day += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 42:
#line 384 "getdate.y"
    { PC.rel_day += (yyvsp[(1) - (1)].intval); ;}
    break;

  case 43:
#line 386 "getdate.y"
    { PC.rel_hour += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 44:
#line 388 "getdate.y"
    { PC.rel_hour += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 45:
#line 390 "getdate.y"
    { PC.rel_hour += (yyvsp[(1) - (1)].intval); ;}
    break;

  case 46:
#line 392 "getdate.y"
    { PC.rel_minutes += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 47:
#line 394 "getdate.y"
    { PC.rel_minutes += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 48:
#line 396 "getdate.y"
    { PC.rel_minutes += (yyvsp[(1) - (1)].intval); ;}
    break;

  case 49:
#line 398 "getdate.y"
    { PC.rel_seconds += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 50:
#line 400 "getdate.y"
    { PC.rel_seconds += (yyvsp[(1) - (2)].textintval).value * (yyvsp[(2) - (2)].intval); ;}
    break;

  case 51:
#line 402 "getdate.y"
    { PC.rel_seconds += (yyvsp[(1) - (1)].intval); ;}
    break;

  case 52:
#line 407 "getdate.y"
    {
	if (PC.dates_seen
	    && ! PC.rels_seen && (PC.times_seen || 2 < (yyvsp[(1) - (1)].textintval).digits))
	  PC.year = (yyvsp[(1) - (1)].textintval);
	else
	  {
	    if (4 < (yyvsp[(1) - (1)].textintval).digits)
	      {
		PC.dates_seen++;
		PC.day = (yyvsp[(1) - (1)].textintval).value % 100;
		PC.month = ((yyvsp[(1) - (1)].textintval).value / 100) % 100;
		PC.year.value = (yyvsp[(1) - (1)].textintval).value / 10000;
		PC.year.digits = (yyvsp[(1) - (1)].textintval).digits - 4;
	      }
	    else
	      {
		PC.times_seen++;
		if ((yyvsp[(1) - (1)].textintval).digits <= 2)
		  {
		    PC.hour = (yyvsp[(1) - (1)].textintval).value;
		    PC.minutes = 0;
		  }
		else
		  {
		    PC.hour = (yyvsp[(1) - (1)].textintval).value / 100;
		    PC.minutes = (yyvsp[(1) - (1)].textintval).value % 100;
		  }
		PC.seconds = 0;
		PC.meridian = MER24;
	      }
	  }
      ;}
    break;

  case 53:
#line 443 "getdate.y"
    { (yyval.intval) = MER24; ;}
    break;

  case 54:
#line 445 "getdate.y"
    { (yyval.intval) = (yyvsp[(1) - (1)].intval); ;}
    break;


/* Line 1267 of yacc.c.  */
#line 1935 "getdate.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
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
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
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


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 448 "getdate.y"


/* Include this file down here because bison inserts code above which
   may define-away `const'.  We want the prototype for get_date to have
   the same signature as the function definition.  */
#include "modules/getdate.h"

#ifndef gmtime
struct tm *gmtime (const time_t *);
#endif
#ifndef localtime
struct tm *localtime (const time_t *);
#endif
#ifndef mktime
time_t mktime (struct tm *);
#endif

static table const meridian_table[] =
{
  { "AM",   tMERIDIAN, MERam },
  { "A.M.", tMERIDIAN, MERam },
  { "PM",   tMERIDIAN, MERpm },
  { "P.M.", tMERIDIAN, MERpm },
  { 0, 0, 0 }
};

static table const dst_table[] =
{
  { "DST", tDST, 0 }
};

static table const month_and_day_table[] =
{
  { "JANUARY",	tMONTH,	 1 },
  { "FEBRUARY",	tMONTH,	 2 },
  { "MARCH",	tMONTH,	 3 },
  { "APRIL",	tMONTH,	 4 },
  { "MAY",	tMONTH,	 5 },
  { "JUNE",	tMONTH,	 6 },
  { "JULY",	tMONTH,	 7 },
  { "AUGUST",	tMONTH,	 8 },
  { "SEPTEMBER",tMONTH,	 9 },
  { "SEPT",	tMONTH,	 9 },
  { "OCTOBER",	tMONTH,	10 },
  { "NOVEMBER",	tMONTH,	11 },
  { "DECEMBER",	tMONTH,	12 },
  { "SUNDAY",	tDAY,	 0 },
  { "MONDAY",	tDAY,	 1 },
  { "TUESDAY",	tDAY,	 2 },
  { "TUES",	tDAY,	 2 },
  { "WEDNESDAY",tDAY,	 3 },
  { "WEDNES",	tDAY,	 3 },
  { "THURSDAY",	tDAY,	 4 },
  { "THUR",	tDAY,	 4 },
  { "THURS",	tDAY,	 4 },
  { "FRIDAY",	tDAY,	 5 },
  { "SATURDAY",	tDAY,	 6 },
  { 0, 0, 0 }
};

static table const time_units_table[] =
{
  { "YEAR",	tYEAR_UNIT,	 1 },
  { "MONTH",	tMONTH_UNIT,	 1 },
  { "FORTNIGHT",tDAY_UNIT,	14 },
  { "WEEK",	tDAY_UNIT,	 7 },
  { "DAY",	tDAY_UNIT,	 1 },
  { "HOUR",	tHOUR_UNIT,	 1 },
  { "MINUTE",	tMINUTE_UNIT,	 1 },
  { "MIN",	tMINUTE_UNIT,	 1 },
  { "SECOND",	tSEC_UNIT,	 1 },
  { "SEC",	tSEC_UNIT,	 1 },
  { 0, 0, 0 }
};

/* Assorted relative-time words. */
static table const relative_time_table[] =
{
  { "TOMORROW",	tMINUTE_UNIT,	24 * 60 },
  { "YESTERDAY",tMINUTE_UNIT,	- (24 * 60) },
  { "TODAY",	tMINUTE_UNIT,	 0 },
  { "NOW",	tMINUTE_UNIT,	 0 },
  { "LAST",	tUNUMBER,	-1 },
  { "THIS",	tUNUMBER,	 0 },
  { "NEXT",	tUNUMBER,	 1 },
  { "FIRST",	tUNUMBER,	 1 },
/*{ "SECOND",	tUNUMBER,	 2 }, */
  { "THIRD",	tUNUMBER,	 3 },
  { "FOURTH",	tUNUMBER,	 4 },
  { "FIFTH",	tUNUMBER,	 5 },
  { "SIXTH",	tUNUMBER,	 6 },
  { "SEVENTH",	tUNUMBER,	 7 },
  { "EIGHTH",	tUNUMBER,	 8 },
  { "NINTH",	tUNUMBER,	 9 },
  { "TENTH",	tUNUMBER,	10 },
  { "ELEVENTH",	tUNUMBER,	11 },
  { "TWELFTH",	tUNUMBER,	12 },
  { "AGO",	tAGO,		 1 },
  { 0, 0, 0 }
};

/* The time zone table.  This table is necessarily incomplete, as time
   zone abbreviations are ambiguous; e.g. Australians interpret "EST"
   as Eastern time in Australia, not as US Eastern Standard Time.
   You cannot rely on getdate to handle arbitrary time zone
   abbreviations; use numeric abbreviations like `-0500' instead.  */
static table const time_zone_table[] =
{
  { "GMT",	tZONE,     HOUR ( 0) },	/* Greenwich Mean */
  { "UT",	tZONE,     HOUR ( 0) },	/* Universal (Coordinated) */
  { "UTC",	tZONE,     HOUR ( 0) },
  { "WET",	tZONE,     HOUR ( 0) },	/* Western European */
  { "WEST",	tDAYZONE,  HOUR ( 0) },	/* Western European Summer */
  { "BST",	tDAYZONE,  HOUR ( 0) },	/* British Summer */
  { "ART",	tZONE,	  -HOUR ( 3) },	/* Argentina */
  { "BRT",	tZONE,	  -HOUR ( 3) },	/* Brazil */
  { "BRST",	tDAYZONE, -HOUR ( 3) },	/* Brazil Summer */
  { "NST",	tZONE,	 -(HOUR ( 3) + 30) },	/* Newfoundland Standard */
  { "NDT",	tDAYZONE,-(HOUR ( 3) + 30) },	/* Newfoundland Daylight */
  { "AST",	tZONE,    -HOUR ( 4) },	/* Atlantic Standard */
  { "ADT",	tDAYZONE, -HOUR ( 4) },	/* Atlantic Daylight */
  { "CLT",	tZONE,    -HOUR ( 4) },	/* Chile */
  { "CLST",	tDAYZONE, -HOUR ( 4) },	/* Chile Summer */
  { "EST",	tZONE,    -HOUR ( 5) },	/* Eastern Standard */
  { "EDT",	tDAYZONE, -HOUR ( 5) },	/* Eastern Daylight */
  { "CST",	tZONE,    -HOUR ( 6) },	/* Central Standard */
  { "CDT",	tDAYZONE, -HOUR ( 6) },	/* Central Daylight */
  { "MST",	tZONE,    -HOUR ( 7) },	/* Mountain Standard */
  { "MDT",	tDAYZONE, -HOUR ( 7) },	/* Mountain Daylight */
  { "PST",	tZONE,    -HOUR ( 8) },	/* Pacific Standard */
  { "PDT",	tDAYZONE, -HOUR ( 8) },	/* Pacific Daylight */
  { "AKST",	tZONE,    -HOUR ( 9) },	/* Alaska Standard */
  { "AKDT",	tDAYZONE, -HOUR ( 9) },	/* Alaska Daylight */
  { "HST",	tZONE,    -HOUR (10) },	/* Hawaii Standard */
  { "HAST",	tZONE,	  -HOUR (10) },	/* Hawaii-Aleutian Standard */
  { "HADT",	tDAYZONE, -HOUR (10) },	/* Hawaii-Aleutian Daylight */
  { "SST",	tZONE,    -HOUR (12) },	/* Samoa Standard */
  { "WAT",	tZONE,     HOUR ( 1) },	/* West Africa */
  { "CET",	tZONE,     HOUR ( 1) },	/* Central European */
  { "CEST",	tDAYZONE,  HOUR ( 1) },	/* Central European Summer */
  { "MET",	tZONE,     HOUR ( 1) },	/* Middle European */
  { "MEZ",	tZONE,     HOUR ( 1) },	/* Middle European */
  { "MEST",	tDAYZONE,  HOUR ( 1) },	/* Middle European Summer */
  { "MESZ",	tDAYZONE,  HOUR ( 1) },	/* Middle European Summer */
  { "EET",	tZONE,     HOUR ( 2) },	/* Eastern European */
  { "EEST",	tDAYZONE,  HOUR ( 2) },	/* Eastern European Summer */
  { "CAT",	tZONE,	   HOUR ( 2) },	/* Central Africa */
  { "SAST",	tZONE,	   HOUR ( 2) },	/* South Africa Standard */
  { "EAT",	tZONE,	   HOUR ( 3) },	/* East Africa */
  { "MSK",	tZONE,	   HOUR ( 3) },	/* Moscow */
  { "MSD",	tDAYZONE,  HOUR ( 3) },	/* Moscow Daylight */
  { "IST",	tZONE,	  (HOUR ( 5) + 30) },	/* India Standard */
  { "SGT",	tZONE,     HOUR ( 8) },	/* Singapore */
  { "KST",	tZONE,     HOUR ( 9) },	/* Korea Standard */
  { "JST",	tZONE,     HOUR ( 9) },	/* Japan Standard */
  { "GST",	tZONE,     HOUR (10) },	/* Guam Standard */
  { "NZST",	tZONE,     HOUR (12) },	/* New Zealand Standard */
  { "NZDT",	tDAYZONE,  HOUR (12) },	/* New Zealand Daylight */
  { 0, 0, 0  }
};

/* Military time zone table. */
static table const military_table[] =
{
  { "A", tZONE,	-HOUR ( 1) },
  { "B", tZONE,	-HOUR ( 2) },
  { "C", tZONE,	-HOUR ( 3) },
  { "D", tZONE,	-HOUR ( 4) },
  { "E", tZONE,	-HOUR ( 5) },
  { "F", tZONE,	-HOUR ( 6) },
  { "G", tZONE,	-HOUR ( 7) },
  { "H", tZONE,	-HOUR ( 8) },
  { "I", tZONE,	-HOUR ( 9) },
  { "K", tZONE,	-HOUR (10) },
  { "L", tZONE,	-HOUR (11) },
  { "M", tZONE,	-HOUR (12) },
  { "N", tZONE,	 HOUR ( 1) },
  { "O", tZONE,	 HOUR ( 2) },
  { "P", tZONE,	 HOUR ( 3) },
  { "Q", tZONE,	 HOUR ( 4) },
  { "R", tZONE,	 HOUR ( 5) },
  { "S", tZONE,	 HOUR ( 6) },
  { "T", tZONE,	 HOUR ( 7) },
  { "U", tZONE,	 HOUR ( 8) },
  { "V", tZONE,	 HOUR ( 9) },
  { "W", tZONE,	 HOUR (10) },
  { "X", tZONE,	 HOUR (11) },
  { "Y", tZONE,	 HOUR (12) },
  { "Z", tZONE,	 HOUR ( 0) },
  { 0, 0, 0 }
};



static int
to_hour (int hours, int meridian)
{
  switch (meridian)
    {
    case MER24:
      return 0 <= hours && hours < 24 ? hours : -1;
    case MERam:
      return 0 < hours && hours < 12 ? hours : hours == 12 ? 0 : -1;
    case MERpm:
      return 0 < hours && hours < 12 ? hours + 12 : hours == 12 ? 12 : -1;
    default:
      abort ();
    }
  /* NOTREACHED */
    return 0;
}

static int
to_year (textint textyear)
{
  int year = textyear.value;

  if (year < 0)
    year = -year;

  /* XPG4 suggests that years 00-68 map to 2000-2068, and
     years 69-99 map to 1969-1999.  */
  if (textyear.digits == 2)
    year += year < 69 ? 2000 : 1900;

  return year;
}

static table const *
lookup_zone (parser_control const *pc, char const *name)
{
  table const *tp;

  /* Try local zone abbreviations first; they're more likely to be right.  */
  for (tp = pc->local_time_zone_table; tp->name; tp++)
    if (strcmp (name, tp->name) == 0)
      return tp;

  for (tp = time_zone_table; tp->name; tp++)
    if (strcmp (name, tp->name) == 0)
      return tp;

  return 0;
}

#if ! HAVE_TM_GMTOFF
/* Yield the difference between *A and *B,
   measured in seconds, ignoring leap seconds.
   The body of this function is taken directly from the GNU C Library;
   see src/strftime.c.  */
static int
tm_diff (struct tm const *a, struct tm const *b)
{
  /* Compute intervening leap days correctly even if year is negative.
     Take care to avoid int overflow in leap day calculations,
     but it's OK to assume that A and B are close to each other.  */
  int a4 = (a->tm_year >> 2) + (TM_YEAR_BASE >> 2) - ! (a->tm_year & 3);
  int b4 = (b->tm_year >> 2) + (TM_YEAR_BASE >> 2) - ! (b->tm_year & 3);
  int a100 = a4 / 25 - (a4 % 25 < 0);
  int b100 = b4 / 25 - (b4 % 25 < 0);
  int a400 = a100 >> 2;
  int b400 = b100 >> 2;
  int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
  int years = a->tm_year - b->tm_year;
  int days = (365 * years + intervening_leap_days
	      + (a->tm_yday - b->tm_yday));
  return (60 * (60 * (24 * days + (a->tm_hour - b->tm_hour))
		+ (a->tm_min - b->tm_min))
	  + (a->tm_sec - b->tm_sec));
}
#endif /* ! HAVE_TM_GMTOFF */

static table const *
lookup_word (parser_control const *pc, char *word)
{
  char *p;
  char *q;
  size_t wordlen;
  table const *tp;
  int i;
  int abbrev;

  /* Make it uppercase.  */
  for (p = word; *p; p++)
    if (ISLOWER ((unsigned char) *p))
      *p = toupper ((unsigned char) *p);

  for (tp = meridian_table; tp->name; tp++)
    if (strcmp (word, tp->name) == 0)
      return tp;

  /* See if we have an abbreviation for a month. */
  wordlen = strlen (word);
  abbrev = wordlen == 3 || (wordlen == 4 && word[3] == '.');

  for (tp = month_and_day_table; tp->name; tp++)
    if ((abbrev ? strncmp (word, tp->name, 3) : strcmp (word, tp->name)) == 0)
      return tp;

  if ((tp = lookup_zone (pc, word)))
    return tp;

  if (strcmp (word, dst_table[0].name) == 0)
    return dst_table;

  for (tp = time_units_table; tp->name; tp++)
    if (strcmp (word, tp->name) == 0)
      return tp;

  /* Strip off any plural and try the units table again. */
  if (word[wordlen - 1] == 'S')
    {
      word[wordlen - 1] = '\0';
      for (tp = time_units_table; tp->name; tp++)
	if (strcmp (word, tp->name) == 0)
	  return tp;
      word[wordlen - 1] = 'S';	/* For "this" in relative_time_table.  */
    }

  for (tp = relative_time_table; tp->name; tp++)
    if (strcmp (word, tp->name) == 0)
      return tp;

  /* Military time zones. */
  if (wordlen == 1)
    for (tp = military_table; tp->name; tp++)
      if (word[0] == tp->name[0])
	return tp;

  /* Drop out any periods and try the time zone table again. */
  for (i = 0, p = q = word; (*p = *q); q++)
    if (*q == '.')
      i = 1;
    else
      p++;
  if (i && (tp = lookup_zone (pc, word)))
    return tp;

  return 0;
}

static int
yylex (YYSTYPE *lvalp, parser_control *pc)
{
  unsigned char c;
  int count;

  for (;;)
    {
      while (c = *pc->input, ISSPACE (c))
	pc->input++;

      if (ISDIGIT (c) || c == '-' || c == '+')
	{
	  char const *p;
	  int sign;
	  int value;
	  if (c == '-' || c == '+')
	    {
	      sign = c == '-' ? -1 : 1;
	      c = *++pc->input;
	      if (! ISDIGIT (c))
		/* skip the '-' sign */
		continue;
	    }
	  else
	    sign = 0;
	  p = pc->input;
	  value = 0;
	  do
	    {
	      value = 10 * value + c - '0';
	      c = *++p;
	    }
	  while (ISDIGIT (c));
	  lvalp->textintval.value = sign < 0 ? -value : value;
	  lvalp->textintval.digits = p - pc->input;
	  pc->input = p;
	  return sign ? tSNUMBER : tUNUMBER;
	}

      if (ISALPHA (c))
	{
	  char buff[20];
	  char *p = buff;
	  table const *tp;

	  do
	    {
	      if (p < buff + sizeof buff - 1)
		*p++ = c;
	      c = *++pc->input;
	    }
	  while (ISALPHA (c) || c == '.');

	  *p = '\0';
	  tp = lookup_word (pc, buff);
	  if (! tp)
	    return '?';
	  lvalp->intval = tp->value;
	  return tp->type;
	}

      if (c != '(')
	return *pc->input++;
      count = 0;
      do
	{
	  c = *pc->input++;
	  if (c == '\0')
	    return c;
	  if (c == '(')
	    count++;
	  else if (c == ')')
	    count--;
	}
      while (count > 0);
    }
}

/* Do nothing if the parser reports an error.  */
static int
yyerror (const char *s ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Parse a date/time string P.  Return the corresponding time_t value,
   or (time_t) -1 if there is an error.  P can be an incomplete or
   relative time specification; if so, use *NOW as the basis for the
   returned time.  */
time_t
get_date (const char *p, const time_t *now)
{
  time_t Start = now ? *now : time (0);
  struct tm *tmp = localtime (&Start);
  struct tm tm;
  struct tm tm0;
  parser_control pc;

  if (! tmp)
    return -1;

  pc.input = p;
  pc.year.value = tmp->tm_year + TM_YEAR_BASE;
  pc.year.digits = 4;
  pc.month = tmp->tm_mon + 1;
  pc.day = tmp->tm_mday;
  pc.hour = tmp->tm_hour;
  pc.minutes = tmp->tm_min;
  pc.seconds = tmp->tm_sec;
  tm.tm_isdst = tmp->tm_isdst;

  pc.meridian = MER24;
  pc.rel_seconds = 0;
  pc.rel_minutes = 0;
  pc.rel_hour = 0;
  pc.rel_day = 0;
  pc.rel_month = 0;
  pc.rel_year = 0;
  pc.dates_seen = 0;
  pc.days_seen = 0;
  pc.rels_seen = 0;
  pc.times_seen = 0;
  pc.local_zones_seen = 0;
  pc.zones_seen = 0;

#if HAVE_STRUCT_TM_TM_ZONE
  pc.local_time_zone_table[0].name = tmp->tm_zone;
  pc.local_time_zone_table[0].type = tLOCAL_ZONE;
  pc.local_time_zone_table[0].value = tmp->tm_isdst;
  pc.local_time_zone_table[1].name = 0;

  /* Probe the names used in the next three calendar quarters, looking
     for a tm_isdst different from the one we already have.  */
  {
    int quarter;
    for (quarter = 1; quarter <= 3; quarter++)
      {
	time_t probe = Start + quarter * (90 * 24 * 60 * 60);
	struct tm *probe_tm = localtime (&probe);
	if (probe_tm && probe_tm->tm_zone
	    && probe_tm->tm_isdst != pc.local_time_zone_table[0].value)
	  {
	      {
		pc.local_time_zone_table[1].name = probe_tm->tm_zone;
		pc.local_time_zone_table[1].type = tLOCAL_ZONE;
		pc.local_time_zone_table[1].value = probe_tm->tm_isdst;
		pc.local_time_zone_table[2].name = 0;
	      }
	    break;
	  }
      }
  }
#else
#if HAVE_TZNAME
  {
# ifndef tzname
    extern char *tzname[];
# endif
    int i;
    for (i = 0; i < 2; i++)
      {
	pc.local_time_zone_table[i].name = tzname[i];
	pc.local_time_zone_table[i].type = tLOCAL_ZONE;
	pc.local_time_zone_table[i].value = i;
      }
    pc.local_time_zone_table[i].name = 0;
  }
#else
  pc.local_time_zone_table[0].name = 0;
#endif
#endif

  if (pc.local_time_zone_table[0].name && pc.local_time_zone_table[1].name
      && ! strcmp (pc.local_time_zone_table[0].name,
		   pc.local_time_zone_table[1].name))
    {
      /* This locale uses the same abbrevation for standard and
	 daylight times.  So if we see that abbreviation, we don't
	 know whether it's daylight time.  */
      pc.local_time_zone_table[0].value = -1;
      pc.local_time_zone_table[1].name = 0;
    }

  if (yyparse (&pc) != 0
      || 1 < pc.times_seen || 1 < pc.dates_seen || 1 < pc.days_seen
      || 1 < (pc.local_zones_seen + pc.zones_seen)
      || (pc.local_zones_seen && 1 < pc.local_isdst))
    return -1;

  tm.tm_year = to_year (pc.year) - TM_YEAR_BASE + pc.rel_year;
  tm.tm_mon = pc.month - 1 + pc.rel_month;
  tm.tm_mday = pc.day + pc.rel_day;
  if (pc.times_seen || (pc.rels_seen && ! pc.dates_seen && ! pc.days_seen))
    {
      tm.tm_hour = to_hour (pc.hour, pc.meridian);
      if (tm.tm_hour < 0)
	return -1;
      tm.tm_min = pc.minutes;
      tm.tm_sec = pc.seconds;
    }
  else
    {
      tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    }

  /* Let mktime deduce tm_isdst if we have an absolute time stamp,
     or if the relative time stamp mentions days, months, or years.  */
  if (pc.dates_seen | pc.days_seen | pc.times_seen | pc.rel_day
      | pc.rel_month | pc.rel_year)
    tm.tm_isdst = -1;

  /* But if the input explicitly specifies local time with or without
     DST, give mktime that information.  */
  if (pc.local_zones_seen)
    tm.tm_isdst = pc.local_isdst;

  tm0 = tm;

  Start = mktime (&tm);

  if (Start == (time_t) -1)
    {

      /* Guard against falsely reporting errors near the time_t boundaries
         when parsing times in other time zones.  For example, if the min
         time_t value is 1970-01-01 00:00:00 UTC and we are 8 hours ahead
         of UTC, then the min localtime value is 1970-01-01 08:00:00; if
         we apply mktime to 1970-01-01 00:00:00 we will get an error, so
         we apply mktime to 1970-01-02 08:00:00 instead and adjust the time
         zone by 24 hours to compensate.  This algorithm assumes that
         there is no DST transition within a day of the time_t boundaries.  */
      if (pc.zones_seen)
	{
	  tm = tm0;
	  if (tm.tm_year <= EPOCH_YEAR - TM_YEAR_BASE)
	    {
	      tm.tm_mday++;
	      pc.time_zone += 24 * 60;
	    }
	  else
	    {
	      tm.tm_mday--;
	      pc.time_zone -= 24 * 60;
	    }
	  Start = mktime (&tm);
	}

      if (Start == (time_t) -1)
	return Start;
    }

  if (pc.days_seen && ! pc.dates_seen)
    {
      tm.tm_mday += ((pc.day_number - tm.tm_wday + 7) % 7
		     + 7 * (pc.day_ordinal - (0 < pc.day_ordinal)));
      tm.tm_isdst = -1;
      Start = mktime (&tm);
      if (Start == (time_t) -1)
	return Start;
    }

  if (pc.zones_seen)
    {
      int delta = pc.time_zone * 60;
#ifdef HAVE_TM_GMTOFF
      delta -= tm.tm_gmtoff;
#else
      struct tm *gmt = gmtime (&Start);
      if (! gmt)
	return -1;
      delta -= tm_diff (&tm, gmt);
#endif
      if ((Start < Start - delta) != (delta < 0))
	return -1;	/* time_t overflow */
      Start -= delta;
    }

  /* Add relative hours, minutes, and seconds.  Ignore leap seconds;
     i.e. "+ 10 minutes" means 600 seconds, even if one of them is a
     leap second.  Typically this is not what the user wants, but it's
     too hard to do it the other way, because the time zone indicator
     must be applied before relative times, and if mktime is applied
     again the time zone will be lost.  */
  {
    time_t t0 = Start;
    long d1 = 60 * 60 * (long) pc.rel_hour;
    time_t t1 = t0 + d1;
    long d2 = 60 * (long) pc.rel_minutes;
    time_t t2 = t1 + d2;
    int d3 = pc.rel_seconds;
    time_t t3 = t2 + d3;
    if ((d1 / (60 * 60) ^ pc.rel_hour)
	| (d2 / 60 ^ pc.rel_minutes)
	| ((t0 + d1 < t0) ^ (d1 < 0))
	| ((t1 + d2 < t1) ^ (d2 < 0))
	| ((t2 + d3 < t2) ^ (d3 < 0)))
      return -1;
    Start = t3;
  }

  return Start;
}

#if TEST

#include <stdio.h>

int
main (int ac, char **av)
{
  char buff[BUFSIZ];
  time_t d;

  printf ("Enter date, or blank line to exit.\n\t> ");
  fflush (stdout);

  buff[BUFSIZ - 1] = 0;
  while (fgets (buff, BUFSIZ - 1, stdin) && buff[0])
    {
      d = get_date (buff, 0);
      if (d == (time_t) -1)
	printf ("Bad format - couldn't convert.\n");
      else
	printf ("%s", ctime (&d));
      printf ("\t> ");
      fflush (stdout);
    }
  return 0;
}
#endif /* defined TEST */


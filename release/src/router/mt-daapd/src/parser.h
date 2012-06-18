/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

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

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     AND = 258,
     OR = 259,
     ARTIST = 260,
     ALBUM = 261,
     GENRE = 262,
     PATH = 263,
     COMPOSER = 264,
     ORCHESTRA = 265,
     CONDUCTOR = 266,
     GROUPING = 267,
     TYPE = 268,
     COMMENT = 269,
     EQUALS = 270,
     LESS = 271,
     LESSEQUAL = 272,
     GREATER = 273,
     GREATEREQUAL = 274,
     IS = 275,
     INCLUDES = 276,
     NOT = 277,
     ID = 278,
     NUM = 279,
     DATE = 280,
     YEAR = 281,
     BPM = 282,
     BITRATE = 283,
     DATEADDED = 284,
     BEFORE = 285,
     AFTER = 286,
     AGO = 287,
     INTERVAL = 288
   };
#endif
/* Tokens.  */
#define AND 258
#define OR 259
#define ARTIST 260
#define ALBUM 261
#define GENRE 262
#define PATH 263
#define COMPOSER 264
#define ORCHESTRA 265
#define CONDUCTOR 266
#define GROUPING 267
#define TYPE 268
#define COMMENT 269
#define EQUALS 270
#define LESS 271
#define LESSEQUAL 272
#define GREATER 273
#define GREATEREQUAL 274
#define IS 275
#define INCLUDES 276
#define NOT 277
#define ID 278
#define NUM 279
#define DATE 280
#define YEAR 281
#define BPM 282
#define BITRATE 283
#define DATEADDED 284
#define BEFORE 285
#define AFTER 286
#define AGO 287
#define INTERVAL 288




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 51 "parser.y"
{
    unsigned int ival;
    char *cval;
    PL_NODE *plval;    
}
/* Line 1489 of yacc.c.  */
#line 121 "parser.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;


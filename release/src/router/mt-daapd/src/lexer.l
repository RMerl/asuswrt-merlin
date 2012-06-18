%{ 

/* $Id: lexer.l,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Simple playlist lexer
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "err.h"
#include "playlist.h"
#include "parser.h"

extern int yydebug;
time_t l_converttime(int day, int month, int year);
time_t l_convertyyyymmdd(char *date);

%}

%option yylineno
%option case-insensitive

qstring \"[^\"\n]*[\"\n]
yyyymmdd  [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]

%%

[\n\t ]+
\#.*\n

artist                   { yylval.ival=ARTIST; return(ARTIST); }
album                    { yylval.ival=ALBUM; return(ALBUM); }
genre                    { yylval.ival=GENRE; return(GENRE); }
path                     { yylval.ival=PATH; return(PATH); }
composer                 { yylval.ival=COMPOSER; return(COMPOSER); }
orchestra                { yylval.ival=ORCHESTRA; return(ORCHESTRA); }
conductor                { yylval.ival=CONDUCTOR; return(CONDUCTOR); }
grouping                 { yylval.ival=GROUPING; return(GROUPING); }
type                     { yylval.ival=TYPE; return(TYPE); }
comment                  { yylval.ival=COMMENT; return(COMMENT); }

year                     { yylval.ival=YEAR; return(YEAR); }
bpm                      { yylval.ival=BPM; return(BPM); }
bitrate                  { yylval.ival=BITRATE; return(BITRATE); }

date                     { yylval.ival=DATEADDED; return(DATEADDED); }

{yyyymmdd}               { yylval.ival=l_convertyyyymmdd(yytext); return(DATE); }
today                    { yylval.ival=time(NULL); return(DATE); }
yesterday                { yylval.ival=time(NULL) - 24*3600; return(DATE); }
last\ week               { yylval.ival=time(NULL) - 24*3600*7; return(DATE); }
last\ month              { yylval.ival=time(NULL) - 24*3600*30; return(DATE); }
last\ year               { yylval.ival=time(NULL) - 24*3600*365; return(DATE); }

days?                    { yylval.ival=24*3600; return(INTERVAL); }
weeks?                   { yylval.ival=24*3600*7; return(INTERVAL); }
months?                  { yylval.ival=24*3600*30; return(INTERVAL); }
years?                   { yylval.ival=24*3600*365; return(INTERVAL); }

ago                      { yylval.ival=AGO; return(AGO); }
before                   { yylval.ival=BEFORE; return(BEFORE); }
after                    { yylval.ival=AFTER; return(AFTER); }

is                       { yylval.ival=IS; return(IS); }
includes                 { yylval.ival=INCLUDES; return(INCLUDES); }
=                        { yylval.ival=EQUALS; return(EQUALS); }

or |
\|\|                     { yylval.ival=OR; return(OR); }

and |
&&                       { yylval.ival=AND; return(AND); }

not |
!                        { yylval.ival=1; return(NOT); }

\<=                      { yylval.ival=LESSEQUAL; return(LESSEQUAL); }
\<                       { yylval.ival=LESS; return(LESS); }
\>=                      { yylval.ival=GREATEREQUAL; return(GREATEREQUAL); }
\>                       { yylval.ival=GREATER; return(GREATER); }

{qstring}                { yylval.cval=strdup(yytext+1);
                           if(yylval.cval[strlen(yylval.cval)-1] == '"')
                               yylval.cval[strlen(yylval.cval)-1] = '\0';
                           return(ID); }

[0-9]+                   { yylval.ival=atoi(yytext); return(NUM); }

. { return yytext[0]; }

%%

time_t l_convertyyyymmdd(char *date) {
    char year[5];
    char month[3];
    char day[3];
    
    memset(year,0,sizeof(year));
    memset(month,0,sizeof(month));
    memset(day,0,sizeof(day));

    strncpy(year,date,4);
    strncpy(month,date+5,2);
    strncpy(day,date+8,2);

    DPRINTF(E_INF,L_PL,"Converting %d-%d-%d\n",atoi(year),atoi(month),atoi(day));
    return l_converttime(atoi(day), atoi(month), atoi(year));
}

time_t l_converttime(int day, int month, int year) {
    struct tm tm;

    memset((void*)&tm,0,sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month-1;
    tm.tm_mday = day;
 
    return mktime(&tm);
}

int yywrap(void) {
    return 1;
}

void yyerror(char *msg) {
    pl_error=1;
    DPRINTF(E_LOG,L_PL,"Playlist error, line %d: %s\n",yylineno, msg);
}



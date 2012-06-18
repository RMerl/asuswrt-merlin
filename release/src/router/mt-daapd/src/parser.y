%{


/* $Id: parser.y,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Simple playlist parser
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
#include "playlist.h"

#define YYERROR_VERBOSE 1

extern int yyerror(char *msg);

/* Forwards */

extern PL_NODE *pl_newcharpredicate(int tag, int op, char *value);
extern PL_NODE *pl_newintpredicate(int tag, int op, int value);
extern PL_NODE *pl_newdatepredicate(int tag, int op, int value);
extern PL_NODE *pl_newexpr(PL_NODE *arg1, int op, PL_NODE *arg2);
extern int pl_addplaylist(char *name, PL_NODE *root);

/* Globals */

int pl_number=2;

%}

%left OR AND

%union {
    unsigned int ival;
    char *cval;
    PL_NODE *plval;    
}

%token <ival> ARTIST 
%token <ival> ALBUM 
%token <ival> GENRE
%token <ival> PATH
%token <ival> COMPOSER
%token <ival> ORCHESTRA
%token <ival> CONDUCTOR
%token <ival> GROUPING
%token <ival> TYPE
%token <ival> COMMENT

%token <ival> EQUALS
%token <ival> LESS
%token <ival> LESSEQUAL
%token <ival> GREATER
%token <ival> GREATEREQUAL
%token <ival> IS 
%token <ival> INCLUDES

%token <ival> OR 
%token <ival> AND
%token <ival> NOT

%token <cval> ID
%token <ival> NUM
%token <ival> DATE

%token <ival> YEAR
%token <ival> BPM
%token <ival> BITRATE

%token <ival> DATEADDED
%token <ival> BEFORE
%token <ival> AFTER
%token <ival> AGO
%token <ival> INTERVAL

%type <plval> expression
%type <plval> predicate
%type <ival> strtag
%type <ival> inttag
%type <ival> datetag
%type <ival> dateval
%type <ival> interval
%type <ival> strbool
%type <ival> intbool
%type <ival> datebool
%type <ival> playlist

%%

playlistlist: playlist {}
| playlistlist playlist {}
;

playlist: ID '{' expression '}' { $$ = pl_addplaylist($1, $3); }
;

expression: expression AND expression { $$=pl_newexpr($1,$2,$3); }
| expression OR expression { $$=pl_newexpr($1,$2,$3); }
| '(' expression ')' { $$=$2; }
| predicate
;

predicate: strtag strbool ID { $$=pl_newcharpredicate($1, $2, $3); }
| inttag intbool NUM { $$=pl_newintpredicate($1, $2, $3); }
| datetag datebool dateval { $$=pl_newdatepredicate($1, $2, $3); }
;

datetag: DATEADDED { $$ = $1; }
;

inttag: YEAR
| BPM
| BITRATE
;

intbool: EQUALS { $$ = $1; }
| LESS { $$ = $1; }
| LESSEQUAL { $$ = $1; }
| GREATER { $$ = $1; }
| GREATEREQUAL { $$ = $1; }
| NOT intbool { $$ = $2 | 0x80000000; }
;

datebool: BEFORE { $$ = $1; }
| AFTER { $$ = $1; }
| NOT datebool { $$=$2 | 0x80000000; }
;

interval: INTERVAL { $$ = $1; }
| NUM INTERVAL { $$ = $1 * $2; }
;

dateval: DATE { $$ = $1; }
| interval BEFORE dateval { $$ = $3 - $1; }
| interval AFTER dateval { $$ = $3 + $1; }
| interval AGO { $$ = time(NULL) - $1; }
;

strtag: ARTIST
| ALBUM
| GENRE
| PATH
| COMPOSER
| ORCHESTRA
| CONDUCTOR
| GROUPING
| TYPE
| COMMENT
;

strbool: IS { $$=$1; }
| INCLUDES { $$=$1; }
| NOT strbool { $$=$2 | 0x80000000; }
;

%%
PL_NODE *pl_newintpredicate(int tag, int op, int value) {
    PL_NODE *pnew;

    pnew=(PL_NODE*)malloc(sizeof(PL_NODE));
    if(!pnew)
	return NULL;

    pnew->op=op;
    pnew->type=T_INT;
    pnew->arg1.ival=tag;
    pnew->arg2.ival=value;
    return pnew;
}

PL_NODE *pl_newdatepredicate(int tag, int op, int value) {
    PL_NODE *pnew;

    pnew=(PL_NODE*)malloc(sizeof(PL_NODE));
    if(!pnew)
	return NULL;

    pnew->op=op;
    pnew->type=T_DATE;
    pnew->arg1.ival=tag;
    pnew->arg2.ival=value;
    return pnew;
}

PL_NODE *pl_newcharpredicate(int tag, int op, char *value) {
    PL_NODE *pnew;

    pnew=(PL_NODE*)malloc(sizeof(PL_NODE));
    if(!pnew)
	return NULL;

    pnew->op=op;
    pnew->type=T_STR;
    pnew->arg1.ival=tag;
    pnew->arg2.cval=value;
    return pnew;
}

PL_NODE *pl_newexpr(PL_NODE *arg1, int op, PL_NODE *arg2) {
    PL_NODE *pnew;

    pnew=(PL_NODE*)malloc(sizeof(PL_NODE));
    if(!pnew)
	return NULL;

    pnew->op=op;
    pnew->arg1.plval=arg1;
    pnew->arg2.plval=arg2;
    return pnew;
}

int pl_addplaylist(char *name, PL_NODE *root) {
    SMART_PLAYLIST *pnew;

    pnew=(SMART_PLAYLIST *)malloc(sizeof(SMART_PLAYLIST));
    if(!pnew)
	return -1;

    pnew->next=pl_smart.next;
    pnew->name=name;
    pnew->root=root;
    pnew->id=pl_number++;
    pl_smart.next=pnew;

    return 0;
}

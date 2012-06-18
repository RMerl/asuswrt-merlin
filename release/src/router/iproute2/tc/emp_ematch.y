%{
 #include <stdio.h>
 #include <stdlib.h>
 #include <malloc.h>
 #include <string.h>
 #include "m_ematch.h"
%}

%locations
%token-table
%error-verbose
%name-prefix="ematch_"

%union {
	unsigned int i;
	struct bstr *b;
	struct ematch *e;
}

%{
 extern int ematch_lex(void);
 extern void yyerror(char *s);
 extern struct ematch *ematch_root;
 extern char *ematch_err;
%}

%token <i> ERROR
%token <b> ATTRIBUTE
%token <i> AND OR NOT
%type <i> invert relation
%type <e> match expr
%type <b> args
%right AND OR
%start input
%%
input:
	/* empty */
	| expr
		{ ematch_root = $1; }
	| expr error
		{
			ematch_root = $1;
			YYACCEPT;
		}
	;

expr:
	match
		{ $$ = $1; }
	| match relation expr
		{
			$1->relation = $2;
			$1->next = $3;
			$$ = $1;
		}
	;

match:
	invert ATTRIBUTE '(' args ')'
		{
			$2->next = $4;
			$$ = new_ematch($2, $1);
			if ($$ == NULL)
				YYABORT;
		}
	| invert '(' expr ')'
		{
			$$ = new_ematch(NULL, $1);
			if ($$ == NULL)
				YYABORT;
			$$->child = $3;
		}
	;

args:
	ATTRIBUTE
		{ $$ = $1; }
	| ATTRIBUTE args
		{ $1->next = $2; }
	;

relation:
	AND
		{ $$ = TCF_EM_REL_AND; }
	| OR
		{ $$ = TCF_EM_REL_OR; }
	;

invert:
	/* empty */
		{ $$ = 0; }
	| NOT
		{ $$ = 1; }
	;
%%

 void yyerror(char *s)
 {
	 ematch_err = strdup(s);
 }


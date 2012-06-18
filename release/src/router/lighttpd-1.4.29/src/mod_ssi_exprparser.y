%token_prefix TK_
%token_type {buffer *}
%extra_argument {ssi_ctx_t *ctx}
%name ssiexprparser

%include {
#include "mod_ssi_expr.h"
#include "buffer.h"

#include <assert.h>
#include <string.h>
}

%parse_failure {
  ctx->ok = 0;
}

%type expr { ssi_val_t * }
%type value { buffer * }
%type exprline { ssi_val_t * }
%type cond { int }
%token_destructor { buffer_free($$); }

%left AND.
%left OR.
%nonassoc EQ NE GT GE LT LE.
%right NOT.

input ::= exprline(B). {
  ctx->val.bo = ssi_val_tobool(B);
  ctx->val.type = SSI_TYPE_BOOL;

  ssi_val_free(B);
}

exprline(A) ::= expr(B) cond(C) expr(D). {
  int cmp;

  if (B->type == SSI_TYPE_STRING &&
      D->type == SSI_TYPE_STRING) {
       cmp = strcmp(B->str->ptr, D->str->ptr);
  } else {
    cmp = ssi_val_tobool(B) - ssi_val_tobool(D);
  }

  A = B;

  switch(C) {
  case SSI_COND_EQ: A->bo = (cmp == 0) ? 1 : 0; break;
  case SSI_COND_NE: A->bo = (cmp != 0) ? 1 : 0; break;
  case SSI_COND_GE: A->bo = (cmp >= 0) ? 1 : 0; break;
  case SSI_COND_GT: A->bo = (cmp > 0) ? 1 : 0; break;
  case SSI_COND_LE: A->bo = (cmp <= 0) ? 1 : 0; break;
  case SSI_COND_LT: A->bo = (cmp < 0) ? 1 : 0; break;
  }

  A->type = SSI_TYPE_BOOL;

  ssi_val_free(D);
}
exprline(A) ::= expr(B). {
  A = B;
}
expr(A) ::= expr(B) AND expr(C). {
  int e;

  e = ssi_val_tobool(B) && ssi_val_tobool(C);

  A = B;
  A->bo = e;
  A->type = SSI_TYPE_BOOL;
  ssi_val_free(C);
}

expr(A) ::= expr(B) OR expr(C). {
  int e;

  e = ssi_val_tobool(B) || ssi_val_tobool(C);

  A = B;
  A->bo = e;
  A->type = SSI_TYPE_BOOL;
  ssi_val_free(C);
}

expr(A) ::= NOT expr(B). {
  int e;

  e = !ssi_val_tobool(B);

  A = B;
  A->bo = e;
  A->type = SSI_TYPE_BOOL;
}
expr(A) ::= LPARAN exprline(B) RPARAN. {
  A = B;
}

expr(A) ::= value(B). {
  A = ssi_val_init();
  A->str = B;
  A->type = SSI_TYPE_STRING;
}

value(A) ::= VALUE(B). {
  A = B;
}

value(A) ::= value(B) VALUE(C). {
  A = B;
  buffer_append_string_buffer(A, C);
  buffer_free(C);
}

cond(A) ::= EQ. { A = SSI_COND_EQ; }
cond(A) ::= NE. { A = SSI_COND_NE; }
cond(A) ::= LE. { A = SSI_COND_LE; }
cond(A) ::= GE. { A = SSI_COND_GE; }
cond(A) ::= LT. { A = SSI_COND_LT; }
cond(A) ::= GT. { A = SSI_COND_GT; }

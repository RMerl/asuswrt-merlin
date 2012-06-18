%token_prefix TK_
%extra_argument {config_t *ctx}
%name configparser

%include {
#include "configfile.h"
#include "buffer.h"
#include "array.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void configparser_push(config_t *ctx, data_config *dc, int isnew) {
  if (isnew) {
    dc->context_ndx = ctx->all_configs->used;
    assert(dc->context_ndx > ctx->current->context_ndx);
    array_insert_unique(ctx->all_configs, (data_unset *)dc);
    dc->parent = ctx->current;
    array_insert_unique(dc->parent->childs, (data_unset *)dc);
  }
  if (ctx->configs_stack->used > 0 && ctx->current->context_ndx == 0) {
    fprintf(stderr, "Cannot use conditionals inside a global { ... } block\n");
    exit(-1);
  }
  array_insert_unique(ctx->configs_stack, (data_unset *)ctx->current);
  ctx->current = dc;
}

static data_config *configparser_pop(config_t *ctx) {
  data_config *old = ctx->current;
  ctx->current = (data_config *) array_pop(ctx->configs_stack);
  return old;
}

/* return a copied variable */
static data_unset *configparser_get_variable(config_t *ctx, const buffer *key) {
  data_unset *du;
  data_config *dc;

#if 0
  fprintf(stderr, "get var %s\n", key->ptr);
#endif
  for (dc = ctx->current; dc; dc = dc->parent) {
#if 0
    fprintf(stderr, "get var on block: %s\n", dc->key->ptr);
    array_print(dc->value, 0);
#endif
    if (NULL != (du = array_get_element(dc->value, key->ptr))) {
      return du->copy(du);
    }
  }
  return NULL;
}

/* op1 is to be eat/return by this function if success, op1->key is not cared
   op2 is left untouch, unreferenced
 */
data_unset *configparser_merge_data(data_unset *op1, const data_unset *op2) {
  /* type mismatch */
  if (op1->type != op2->type) {
    if (op1->type == TYPE_STRING && op2->type == TYPE_INTEGER) {
      data_string *ds = (data_string *)op1;
      buffer_append_long(ds->value, ((data_integer*)op2)->value);
      return op1;
    } else if (op1->type == TYPE_INTEGER && op2->type == TYPE_STRING) {
      data_string *ds = data_string_init();
      buffer_append_long(ds->value, ((data_integer*)op1)->value);
      buffer_append_string_buffer(ds->value, ((data_string*)op2)->value);
      op1->free(op1);
      return (data_unset *)ds;
    } else {
      fprintf(stderr, "data type mismatch, cannot merge\n");
      return NULL;
    }
  }

  switch (op1->type) {
    case TYPE_STRING:
      buffer_append_string_buffer(((data_string *)op1)->value, ((data_string *)op2)->value);
      break;
    case TYPE_INTEGER:
      ((data_integer *)op1)->value += ((data_integer *)op2)->value;
      break;
    case TYPE_ARRAY: {
      array *dst = ((data_array *)op1)->value;
      array *src = ((data_array *)op2)->value;
      data_unset *du;
      size_t i;

      for (i = 0; i < src->used; i ++) {
        du = (data_unset *)src->data[i];
        if (du) {
          array_insert_unique(dst, du->copy(du));
        }
      }
      break;
    default:
      assert(0);
      break;
    }
  }
  return op1;
}

}

%parse_failure {
  ctx->ok = 0;
}

input ::= metalines.
metalines ::= metalines metaline.
metalines ::= .
metaline ::= varline.
metaline ::= global.
metaline ::= condlines(A) EOL. { A = NULL; }
metaline ::= include.
metaline ::= include_shell.
metaline ::= EOL.

%type       value                  {data_unset *}
%type       expression             {data_unset *}
%type       aelement               {data_unset *}
%type       condline               {data_config *}
%type       condlines              {data_config *}
%type       global                 {data_config *}
%type       aelements              {array *}
%type       array                  {array *}
%type       key                    {buffer *}
%type       stringop               {buffer *}

%type       cond                   {config_cond_t }

%destructor value                  { $$->free($$); }
%destructor expression             { $$->free($$); }
%destructor aelement               { $$->free($$); }
%destructor aelements              { array_free($$); }
%destructor array                  { array_free($$); }
%destructor key                    { buffer_free($$); }
%destructor stringop               { buffer_free($$); }

%token_type                        {buffer *}
%token_destructor                  { buffer_free($$); }

varline ::= key(A) ASSIGN expression(B). {
  if (ctx->ok) {
    buffer_copy_string_buffer(B->key, A);
    if (strncmp(A->ptr, "env.", sizeof("env.") - 1) == 0) {
      fprintf(stderr, "Setting env variable is not supported in conditional %d %s: %s\n",
          ctx->current->context_ndx,
          ctx->current->key->ptr, A->ptr);
      ctx->ok = 0;
    } else if (NULL == array_get_element(ctx->current->value, B->key->ptr)) {
      array_insert_unique(ctx->current->value, B);
      B = NULL;
    } else {
      fprintf(stderr, "Duplicate config variable in conditional %d %s: %s\n",
              ctx->current->context_ndx,
              ctx->current->key->ptr, B->key->ptr);
      ctx->ok = 0;
      B->free(B);
      B = NULL;
    }
  }
  buffer_free(A);
  A = NULL;
}

varline ::= key(A) APPEND expression(B). {
  array *vars = ctx->current->value;
  data_unset *du;

  if (strncmp(A->ptr, "env.", sizeof("env.") - 1) == 0) {
    fprintf(stderr, "Appending env variable is not supported in conditional %d %s: %s\n",
        ctx->current->context_ndx,
        ctx->current->key->ptr, A->ptr);
    ctx->ok = 0;
  } else if (NULL != (du = array_get_element(vars, A->ptr))) {
    /* exists in current block */
    du = configparser_merge_data(du, B);
    if (NULL == du) {
      ctx->ok = 0;
    }
    else {
      buffer_copy_string_buffer(du->key, A);
      array_replace(vars, du);
    }
    B->free(B);
  } else if (NULL != (du = configparser_get_variable(ctx, A))) {
    du = configparser_merge_data(du, B);
    if (NULL == du) {
      ctx->ok = 0;
    }
    else {
      buffer_copy_string_buffer(du->key, A);
      array_insert_unique(ctx->current->value, du);
    }
    B->free(B);
  } else {
    buffer_copy_string_buffer(B->key, A);
    array_insert_unique(ctx->current->value, B);
  }
  buffer_free(A);
  A = NULL;
  B = NULL;
}

key(A) ::= LKEY(B). {
  if (strchr(B->ptr, '.') == NULL) {
    A = buffer_init_string("var.");
    buffer_append_string_buffer(A, B);
    buffer_free(B);
    B = NULL;
  } else {
    A = B;
    B = NULL;
  }
}

expression(A) ::= expression(B) PLUS value(C). {
  A = configparser_merge_data(B, C);
  if (NULL == A) {
    ctx->ok = 0;
  }
  B = NULL;
  C->free(C);
  C = NULL;
}

expression(A) ::= value(B). {
  A = B;
  B = NULL;
}

value(A) ::= key(B). {
  A = NULL;
  if (strncmp(B->ptr, "env.", sizeof("env.") - 1) == 0) {
    char *env;

    if (NULL != (env = getenv(B->ptr + 4))) {
      data_string *ds;
      ds = data_string_init();
      buffer_append_string(ds->value, env);
      A = (data_unset *)ds;
    }
    else {
      fprintf(stderr, "Undefined env variable: %s\n", B->ptr + 4);
      ctx->ok = 0;
    }
  } else if (NULL == (A = configparser_get_variable(ctx, B))) {
    fprintf(stderr, "Undefined config variable: %s\n", B->ptr);
    ctx->ok = 0;
  }
  if (!A) {
    /* make a dummy so it won't crash */
    A = (data_unset *)data_string_init();
  }
  buffer_free(B);
  B = NULL;
}

value(A) ::= STRING(B). {
  A = (data_unset *)data_string_init();
  buffer_copy_string_buffer(((data_string *)(A))->value, B);
  buffer_free(B);
  B = NULL;
}

value(A) ::= INTEGER(B). {
  A = (data_unset *)data_integer_init();
  ((data_integer *)(A))->value = strtol(B->ptr, NULL, 10);
  buffer_free(B);
  B = NULL;
}
value(A) ::= array(B). {
  A = (data_unset *)data_array_init();
  array_free(((data_array *)(A))->value);
  ((data_array *)(A))->value = B;
  B = NULL;
}
array(A) ::= LPARAN RPARAN. {
  A = array_init();
}
array(A) ::= LPARAN aelements(B) RPARAN. {
  A = B;
  B = NULL;
}

aelements(A) ::= aelements(C) COMMA aelement(B). {
  if (buffer_is_empty(B->key) ||
      NULL == array_get_element(C, B->key->ptr)) {
    array_insert_unique(C, B);
    B = NULL;
  } else {
    fprintf(stderr, "Duplicate array-key: %s\n",
            B->key->ptr);
    ctx->ok = 0;
    B->free(B);
    B = NULL;
  }

  A = C;
  C = NULL;
}

aelements(A) ::= aelements(C) COMMA. {
  A = C;
  C = NULL;
}

aelements(A) ::= aelement(B). {
  A = array_init();
  array_insert_unique(A, B);
  B = NULL;
}

aelement(A) ::= expression(B). {
  A = B;
  B = NULL;
}
aelement(A) ::= stringop(B) ARRAY_ASSIGN expression(C). {
  buffer_copy_string_buffer(C->key, B);
  buffer_free(B);
  B = NULL;

  A = C;
  C = NULL;
}

eols ::= EOL.
eols ::= .

globalstart ::= GLOBAL. {
  data_config *dc;
  dc = (data_config *)array_get_element(ctx->srv->config_context, "global");
  assert(dc);
  configparser_push(ctx, dc, 0);
}

global(A) ::= globalstart LCURLY metalines RCURLY. {
  data_config *cur;

  cur = ctx->current;
  configparser_pop(ctx);

  assert(cur && ctx->current);

  A = cur;
}

condlines(A) ::= condlines(B) eols ELSE condline(C). {
  if (B->context_ndx >= C->context_ndx) {
    fprintf(stderr, "unreachable else condition\n");
    ctx->ok = 0;
  }
  C->prev = B;
  B->next = C;
  A = C;
  B = NULL;
  C = NULL;
}

condlines(A) ::= condline(B). {
  A = B;
  B = NULL;
}

condline(A) ::= context LCURLY metalines RCURLY. {
  data_config *cur;

  cur = ctx->current;
  configparser_pop(ctx);

  assert(cur && ctx->current);

  A = cur;
}

context ::= DOLLAR SRVVARNAME(B) LBRACKET stringop(C) RBRACKET cond(E) expression(D). {
  data_config *dc;
  buffer *b, *rvalue, *op;

  if (ctx->ok && D->type != TYPE_STRING) {
    fprintf(stderr, "rvalue must be string");
    ctx->ok = 0;
  }

  switch(E) {
  case CONFIG_COND_NE:
    op = buffer_init_string("!=");
    break;
  case CONFIG_COND_EQ:
    op = buffer_init_string("==");
    break;
  case CONFIG_COND_NOMATCH:
    op = buffer_init_string("!~");
    break;
  case CONFIG_COND_MATCH:
    op = buffer_init_string("=~");
    break;
  default:
    assert(0);
    return;
  }

  b = buffer_init();
  buffer_copy_string_buffer(b, ctx->current->key);
  buffer_append_string(b, "/");
  buffer_append_string_buffer(b, B);
  buffer_append_string_buffer(b, C);
  buffer_append_string_buffer(b, op);
  rvalue = ((data_string*)D)->value;
  buffer_append_string_buffer(b, rvalue);

  if (NULL != (dc = (data_config *)array_get_element(ctx->all_configs, b->ptr))) {
    configparser_push(ctx, dc, 0);
  } else {
    struct {
      comp_key_t comp;
      char *comp_key;
      size_t len;
    } comps[] = {
      { COMP_SERVER_SOCKET,      CONST_STR_LEN("SERVER[\"socket\"]"   ) },
      { COMP_HTTP_URL,           CONST_STR_LEN("HTTP[\"url\"]"        ) },
      { COMP_HTTP_HOST,          CONST_STR_LEN("HTTP[\"host\"]"       ) },
      { COMP_HTTP_REFERER,       CONST_STR_LEN("HTTP[\"referer\"]"    ) },
      { COMP_HTTP_USER_AGENT,    CONST_STR_LEN("HTTP[\"useragent\"]"  ) },
      { COMP_HTTP_USER_AGENT,    CONST_STR_LEN("HTTP[\"user-agent\"]"  ) },
      { COMP_HTTP_LANGUAGE,      CONST_STR_LEN("HTTP[\"language\"]"   ) },
      { COMP_HTTP_COOKIE,        CONST_STR_LEN("HTTP[\"cookie\"]"     ) },
      { COMP_HTTP_REMOTE_IP,     CONST_STR_LEN("HTTP[\"remoteip\"]"   ) },
      { COMP_HTTP_REMOTE_IP,     CONST_STR_LEN("HTTP[\"remote-ip\"]"   ) },
      { COMP_HTTP_QUERY_STRING,  CONST_STR_LEN("HTTP[\"querystring\"]") },
      { COMP_HTTP_QUERY_STRING,  CONST_STR_LEN("HTTP[\"query-string\"]") },
      { COMP_HTTP_REQUEST_METHOD, CONST_STR_LEN("HTTP[\"request-method\"]") },
      { COMP_HTTP_SCHEME,        CONST_STR_LEN("HTTP[\"scheme\"]"     ) },
      { COMP_UNSET, NULL, 0 },
    };
    size_t i;

    dc = data_config_init();

    buffer_copy_string_buffer(dc->key, b);
    buffer_copy_string_buffer(dc->op, op);
    buffer_copy_string_buffer(dc->comp_key, B);
    buffer_append_string_len(dc->comp_key, CONST_STR_LEN("[\""));
    buffer_append_string_buffer(dc->comp_key, C);
    buffer_append_string_len(dc->comp_key, CONST_STR_LEN("\"]"));
    dc->cond = E;

    for (i = 0; comps[i].comp_key; i ++) {
      if (buffer_is_equal_string(
            dc->comp_key, comps[i].comp_key, comps[i].len)) {
        dc->comp = comps[i].comp;
        break;
      }
    }
    if (COMP_UNSET == dc->comp) {
      fprintf(stderr, "error comp_key %s", dc->comp_key->ptr);
      ctx->ok = 0;
    }

    switch(E) {
    case CONFIG_COND_NE:
    case CONFIG_COND_EQ:
      dc->string = buffer_init_buffer(rvalue);
      break;
    case CONFIG_COND_NOMATCH:
    case CONFIG_COND_MATCH: {
#ifdef HAVE_PCRE_H
      const char *errptr;
      int erroff, captures;

      if (NULL == (dc->regex =
          pcre_compile(rvalue->ptr, 0, &errptr, &erroff, NULL))) {
        dc->string = buffer_init_string(errptr);
        dc->cond = CONFIG_COND_UNSET;

        fprintf(stderr, "parsing regex failed: %s -> %s at offset %d\n",
            rvalue->ptr, errptr, erroff);

        ctx->ok = 0;
      } else if (NULL == (dc->regex_study =
          pcre_study(dc->regex, 0, &errptr)) &&
                 errptr != NULL) {
        fprintf(stderr, "studying regex failed: %s -> %s\n",
            rvalue->ptr, errptr);
        ctx->ok = 0;
      } else if (0 != (pcre_fullinfo(dc->regex, dc->regex_study, PCRE_INFO_CAPTURECOUNT, &captures))) {
        fprintf(stderr, "getting capture count for regex failed: %s\n",
            rvalue->ptr);
        ctx->ok = 0;
      } else if (captures > 9) {
        fprintf(stderr, "Too many captures in regex, use (?:...) instead of (...): %s\n",
            rvalue->ptr);
        ctx->ok = 0;
      } else {
        dc->string = buffer_init_buffer(rvalue);
      }
#else
      fprintf(stderr, "can't handle '$%s[%s] =~ ...' as you compiled without pcre support. \n"
		      "(perhaps just a missing pcre-devel package ?) \n",
                      B->ptr, C->ptr);
      ctx->ok = 0;
#endif
      break;
    }

    default:
      fprintf(stderr, "unknown condition for $%s[%s]\n",
                      B->ptr, C->ptr);
      ctx->ok = 0;
      break;
    }

    configparser_push(ctx, dc, 1);
  }

  buffer_free(b);
  buffer_free(op);
  buffer_free(B);
  B = NULL;
  buffer_free(C);
  C = NULL;
  D->free(D);
  D = NULL;
}
cond(A) ::= EQ. {
  A = CONFIG_COND_EQ;
}
cond(A) ::= MATCH. {
  A = CONFIG_COND_MATCH;
}
cond(A) ::= NE. {
  A = CONFIG_COND_NE;
}
cond(A) ::= NOMATCH. {
  A = CONFIG_COND_NOMATCH;
}

stringop(A) ::= expression(B). {
  A = NULL;
  if (ctx->ok) {
    if (B->type == TYPE_STRING) {
      A = buffer_init_buffer(((data_string*)B)->value);
    } else if (B->type == TYPE_INTEGER) {
      A = buffer_init();
      buffer_copy_long(A, ((data_integer *)B)->value);
    } else {
      fprintf(stderr, "operand must be string");
      ctx->ok = 0;
    }
  }
  B->free(B);
  B = NULL;
}

include ::= INCLUDE stringop(A). {
  if (ctx->ok) {
    if (0 != config_parse_file(ctx->srv, ctx, A->ptr)) {
      ctx->ok = 0;
    }
    buffer_free(A);
    A = NULL;
  }
}

include_shell ::= INCLUDE_SHELL stringop(A). {
  if (ctx->ok) {
    if (0 != config_parse_cmd(ctx->srv, ctx, A->ptr)) {
      ctx->ok = 0;
    }
    buffer_free(A);
    A = NULL;
  }
}

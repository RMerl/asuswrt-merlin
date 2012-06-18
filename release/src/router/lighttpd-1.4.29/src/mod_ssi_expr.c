#include "buffer.h"
#include "log.h"
#include "mod_ssi.h"
#include "mod_ssi_expr.h"
#include "mod_ssi_exprparser.h"

#include <ctype.h>
#include <string.h>

typedef struct {
	const char *input;
	size_t offset;
	size_t size;

	int line_pos;

	int in_key;
	int in_brace;
	int in_cond;
} ssi_tokenizer_t;

ssi_val_t *ssi_val_init() {
	ssi_val_t *s;

	s = calloc(1, sizeof(*s));

	return s;
}

void ssi_val_free(ssi_val_t *s) {
	if (s->str) buffer_free(s->str);

	free(s);
}

int ssi_val_tobool(ssi_val_t *B) {
	if (B->type == SSI_TYPE_STRING) {
		return B->str->used > 1 ? 1 : 0;
	} else {
		return B->bo;
	}
}

static int ssi_expr_tokenizer(server *srv, connection *con, plugin_data *p,
			      ssi_tokenizer_t *t, int *token_id, buffer *token) {
	int tid = 0;
	size_t i;

	UNUSED(con);

	for (tid = 0; tid == 0 && t->offset < t->size && t->input[t->offset] ; ) {
		char c = t->input[t->offset];
		data_string *ds;

		switch (c) {
		case '=':
			tid = TK_EQ;

			t->offset++;
			t->line_pos++;

			buffer_copy_string_len(token, CONST_STR_LEN("(=)"));

			break;
		case '>':
			if (t->input[t->offset + 1] == '=') {
				t->offset += 2;
				t->line_pos += 2;

				tid = TK_GE;

				buffer_copy_string_len(token, CONST_STR_LEN("(>=)"));
			} else {
				t->offset += 1;
				t->line_pos += 1;

				tid = TK_GT;

				buffer_copy_string_len(token, CONST_STR_LEN("(>)"));
			}

			break;
		case '<':
			if (t->input[t->offset + 1] == '=') {
				t->offset += 2;
				t->line_pos += 2;

				tid = TK_LE;

				buffer_copy_string_len(token, CONST_STR_LEN("(<=)"));
			} else {
				t->offset += 1;
				t->line_pos += 1;

				tid = TK_LT;

				buffer_copy_string_len(token, CONST_STR_LEN("(<)"));
			}

			break;

		case '!':
			if (t->input[t->offset + 1] == '=') {
				t->offset += 2;
				t->line_pos += 2;

				tid = TK_NE;

				buffer_copy_string_len(token, CONST_STR_LEN("(!=)"));
			} else {
				t->offset += 1;
				t->line_pos += 1;

				tid = TK_NOT;

				buffer_copy_string_len(token, CONST_STR_LEN("(!)"));
			}

			break;
		case '&':
			if (t->input[t->offset + 1] == '&') {
				t->offset += 2;
				t->line_pos += 2;

				tid = TK_AND;

				buffer_copy_string_len(token, CONST_STR_LEN("(&&)"));
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"pos:", t->line_pos,
						"missing second &");
				return -1;
			}

			break;
		case '|':
			if (t->input[t->offset + 1] == '|') {
				t->offset += 2;
				t->line_pos += 2;

				tid = TK_OR;

				buffer_copy_string_len(token, CONST_STR_LEN("(||)"));
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"pos:", t->line_pos,
						"missing second |");
				return -1;
			}

			break;
		case '\t':
		case ' ':
			t->offset++;
			t->line_pos++;
			break;

		case '\'':
			/* search for the terminating " */
			for (i = 1; t->input[t->offset + i] && t->input[t->offset + i] != '\'';  i++);

			if (t->input[t->offset + i]) {
				tid = TK_VALUE;

				buffer_copy_string_len(token, t->input + t->offset + 1, i-1);

				t->offset += i + 1;
				t->line_pos += i + 1;
			} else {
				/* ERROR */

				log_error_write(srv, __FILE__, __LINE__, "sds",
						"pos:", t->line_pos,
						"missing closing quote");

				return -1;
			}

			break;
		case '(':
			t->offset++;
			t->in_brace++;

			tid = TK_LPARAN;

			buffer_copy_string_len(token, CONST_STR_LEN("("));
			break;
		case ')':
			t->offset++;
			t->in_brace--;

			tid = TK_RPARAN;

			buffer_copy_string_len(token, CONST_STR_LEN(")"));
			break;
		case '$':
			if (t->input[t->offset + 1] == '{') {
				for (i = 2; t->input[t->offset + i] && t->input[t->offset + i] != '}';  i++);

				if (t->input[t->offset + i] != '}') {
					log_error_write(srv, __FILE__, __LINE__, "sds",
							"pos:", t->line_pos,
							"missing closing quote");

					return -1;
				}

				buffer_copy_string_len(token, t->input + t->offset + 2, i-3);
			} else {
				for (i = 1; isalpha(t->input[t->offset + i]) || t->input[t->offset + i] == '_';  i++);

				buffer_copy_string_len(token, t->input + t->offset + 1, i-1);
			}

			tid = TK_VALUE;

			if (NULL != (ds = (data_string *)array_get_element(p->ssi_cgi_env, token->ptr))) {
				buffer_copy_string_buffer(token, ds->value);
			} else if (NULL != (ds = (data_string *)array_get_element(p->ssi_vars, token->ptr))) {
				buffer_copy_string_buffer(token, ds->value);
			} else {
				buffer_copy_string_len(token, CONST_STR_LEN(""));
			}

			t->offset += i;
			t->line_pos += i;

			break;
		default:
			for (i = 0; isgraph(t->input[t->offset + i]);  i++) {
				char d = t->input[t->offset + i];
				switch(d) {
				case ' ':
				case '\t':
				case ')':
				case '(':
				case '\'':
				case '=':
				case '!':
				case '<':
				case '>':
				case '&':
				case '|':
					break;
				}
			}

			tid = TK_VALUE;

			buffer_copy_string_len(token, t->input + t->offset, i);

			t->offset += i;
			t->line_pos += i;

			break;
		}
	}

	if (tid) {
		*token_id = tid;

		return 1;
	} else if (t->offset < t->size) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"pos:", t->line_pos,
				"foobar");
	}
	return 0;
}

int ssi_eval_expr(server *srv, connection *con, plugin_data *p, const char *expr) {
	ssi_tokenizer_t t;
	void *pParser;
	int token_id;
	buffer *token;
	ssi_ctx_t context;
	int ret;

	t.input = expr;
	t.offset = 0;
	t.size = strlen(expr);
	t.line_pos = 1;

	t.in_key = 1;
	t.in_brace = 0;
	t.in_cond = 0;

	context.ok = 1;
	context.srv = srv;

	/* default context */

	pParser = ssiexprparserAlloc( malloc );
	token = buffer_init();
	while((1 == (ret = ssi_expr_tokenizer(srv, con, p, &t, &token_id, token))) && context.ok) {
		ssiexprparser(pParser, token_id, token, &context);

		token = buffer_init();
	}
	ssiexprparser(pParser, 0, token, &context);
	ssiexprparserFree(pParser, free );

	buffer_free(token);

	if (ret == -1) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"expr parser failed");
		return -1;
	}

	if (context.ok == 0) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"pos:", t.line_pos,
				"parser failed somehow near here");
		return -1;
	}
#if 0
	log_error_write(srv, __FILE__, __LINE__, "ssd",
			"expr: ",
			expr,
			context.val.bo);
#endif
	return context.val.bo;
}

#ifndef _MOD_SSI_EXPR_H_
#define _MOD_SSI_EXPR_H_

#include "buffer.h"

typedef struct {
	enum { SSI_TYPE_UNSET, SSI_TYPE_BOOL, SSI_TYPE_STRING } type;

	buffer *str;
	int     bo;
} ssi_val_t;

typedef struct {
	int     ok;

	ssi_val_t val;

	void   *srv;
} ssi_ctx_t;

typedef enum { SSI_COND_UNSET, SSI_COND_LE, SSI_COND_GE, SSI_COND_EQ, SSI_COND_NE, SSI_COND_LT, SSI_COND_GT } ssi_expr_cond;

void *ssiexprparserAlloc(void *(*mallocProc)(size_t));
void ssiexprparserFree(void *p, void (*freeProc)(void*));
void ssiexprparser(void *yyp, int yymajor, buffer *yyminor, ssi_ctx_t *ctx);

int ssi_val_tobool(ssi_val_t *B);
ssi_val_t *ssi_val_init();
void ssi_val_free(ssi_val_t *s);

#endif

/*
 * arithmetic code ripped out of ash shell for code sharing
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Original BSD copyright notice is retained at the end of this file.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 * rewrite arith.y to micro stack based cryptic algorithm by
 * Copyright (c) 2001 Aaron Lehmann <aaronl@vitelus.com>
 *
 * Modified by Paul Mundt <lethal@linux-sh.org> (c) 2004 to support
 * dynamic variables.
 *
 * Modified by Vladimir Oleynik <dzo@simtreas.ru> (c) 2001-2005 to be
 * used in busybox and size optimizations,
 * rewrote arith (see notes to this), added locale support,
 * rewrote dynamic variables.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
/* Copyright (c) 2001 Aaron Lehmann <aaronl@vitelus.com>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/* This is my infix parser/evaluator. It is optimized for size, intended
 * as a replacement for yacc-based parsers. However, it may well be faster
 * than a comparable parser written in yacc. The supported operators are
 * listed in #defines below. Parens, order of operations, and error handling
 * are supported. This code is thread safe. The exact expression format should
 * be that which POSIX specifies for shells. */

/* The code uses a simple two-stack algorithm. See
 * http://www.onthenet.com.au/~grahamis/int2008/week02/lect02.html
 * for a detailed explanation of the infix-to-postfix algorithm on which
 * this is based (this code differs in that it applies operators immediately
 * to the stack instead of adding them to a queue to end up with an
 * expression). */

/* To use the routine, call it with an expression string and error return
 * pointer */

/*
 * Aug 24, 2001              Manuel Novoa III
 *
 * Reduced the generated code size by about 30% (i386) and fixed several bugs.
 *
 * 1) In arith_apply():
 *    a) Cached values of *numptr and &(numptr[-1]).
 *    b) Removed redundant test for zero denominator.
 *
 * 2) In arith():
 *    a) Eliminated redundant code for processing operator tokens by moving
 *       to a table-based implementation.  Also folded handling of parens
 *       into the table.
 *    b) Combined all 3 loops which called arith_apply to reduce generated
 *       code size at the cost of speed.
 *
 * 3) The following expressions were treated as valid by the original code:
 *       1()  ,    0!  ,    1 ( *3 )   .
 *    These bugs have been fixed by internally enclosing the expression in
 *    parens and then checking that all binary ops and right parens are
 *    preceded by a valid expression (NUM_TOKEN).
 *
 * Note: It may be desirable to replace Aaron's test for whitespace with
 * ctype's isspace() if it is used by another busybox applet or if additional
 * whitespace chars should be considered.  Look below the "#include"s for a
 * precompiler test.
 */
/*
 * Aug 26, 2001              Manuel Novoa III
 *
 * Return 0 for null expressions.  Pointed out by Vladimir Oleynik.
 *
 * Merge in Aaron's comments previously posted to the busybox list,
 * modified slightly to take account of my changes to the code.
 *
 */
/*
 *  (C) 2003 Vladimir Oleynik <dzo@simtreas.ru>
 *
 * - allow access to variable,
 *   used recursive find value indirection (c=2*2; a="c"; $((a+=2)) produce 6)
 * - realize assign syntax (VAR=expr, +=, *= etc)
 * - realize exponentiation (** operator)
 * - realize comma separated - expr, expr
 * - realise ++expr --expr expr++ expr--
 * - realise expr ? expr : expr (but, second expr calculate always)
 * - allow hexadecimal and octal numbers
 * - was restored loses XOR operator
 * - remove one goto label, added three ;-)
 * - protect $((num num)) as true zero expr (Manuel`s error)
 * - always use special isspace(), see comment from bash ;-)
 */
#include "libbb.h"
#include "math.h"

#define a_e_h_t arith_eval_hooks_t
#define lookupvar (math_hooks->lookupvar)
#define setvar    (math_hooks->setvar   )
#define endofname (math_hooks->endofname)

#define arith_isspace(arithval) \
	(arithval == ' ' || arithval == '\n' || arithval == '\t')

typedef unsigned char operator;

/* An operator's token id is a bit of a bitfield. The lower 5 bits are the
 * precedence, and 3 high bits are an ID unique across operators of that
 * precedence. The ID portion is so that multiple operators can have the
 * same precedence, ensuring that the leftmost one is evaluated first.
 * Consider * and /. */

#define tok_decl(prec,id) (((id)<<5)|(prec))
#define PREC(op) ((op) & 0x1F)

#define TOK_LPAREN tok_decl(0,0)

#define TOK_COMMA tok_decl(1,0)

#define TOK_ASSIGN tok_decl(2,0)
#define TOK_AND_ASSIGN tok_decl(2,1)
#define TOK_OR_ASSIGN tok_decl(2,2)
#define TOK_XOR_ASSIGN tok_decl(2,3)
#define TOK_PLUS_ASSIGN tok_decl(2,4)
#define TOK_MINUS_ASSIGN tok_decl(2,5)
#define TOK_LSHIFT_ASSIGN tok_decl(2,6)
#define TOK_RSHIFT_ASSIGN tok_decl(2,7)

#define TOK_MUL_ASSIGN tok_decl(3,0)
#define TOK_DIV_ASSIGN tok_decl(3,1)
#define TOK_REM_ASSIGN tok_decl(3,2)

/* all assign is right associativity and precedence eq, but (7+3)<<5 > 256 */
#define convert_prec_is_assing(prec) do { if (prec == 3) prec = 2; } while (0)

/* conditional is right associativity too */
#define TOK_CONDITIONAL tok_decl(4,0)
#define TOK_CONDITIONAL_SEP tok_decl(4,1)

#define TOK_OR tok_decl(5,0)

#define TOK_AND tok_decl(6,0)

#define TOK_BOR tok_decl(7,0)

#define TOK_BXOR tok_decl(8,0)

#define TOK_BAND tok_decl(9,0)

#define TOK_EQ tok_decl(10,0)
#define TOK_NE tok_decl(10,1)

#define TOK_LT tok_decl(11,0)
#define TOK_GT tok_decl(11,1)
#define TOK_GE tok_decl(11,2)
#define TOK_LE tok_decl(11,3)

#define TOK_LSHIFT tok_decl(12,0)
#define TOK_RSHIFT tok_decl(12,1)

#define TOK_ADD tok_decl(13,0)
#define TOK_SUB tok_decl(13,1)

#define TOK_MUL tok_decl(14,0)
#define TOK_DIV tok_decl(14,1)
#define TOK_REM tok_decl(14,2)

/* exponent is right associativity */
#define TOK_EXPONENT tok_decl(15,1)

/* For now unary operators. */
#define UNARYPREC 16
#define TOK_BNOT tok_decl(UNARYPREC,0)
#define TOK_NOT tok_decl(UNARYPREC,1)

#define TOK_UMINUS tok_decl(UNARYPREC+1,0)
#define TOK_UPLUS tok_decl(UNARYPREC+1,1)

#define PREC_PRE (UNARYPREC+2)

#define TOK_PRE_INC tok_decl(PREC_PRE, 0)
#define TOK_PRE_DEC tok_decl(PREC_PRE, 1)

#define PREC_POST (UNARYPREC+3)

#define TOK_POST_INC tok_decl(PREC_POST, 0)
#define TOK_POST_DEC tok_decl(PREC_POST, 1)

#define SPEC_PREC (UNARYPREC+4)

#define TOK_NUM tok_decl(SPEC_PREC, 0)
#define TOK_RPAREN tok_decl(SPEC_PREC, 1)

#define NUMPTR (*numstackptr)

static int
tok_have_assign(operator op)
{
	operator prec = PREC(op);

	convert_prec_is_assing(prec);
	return (prec == PREC(TOK_ASSIGN) ||
			prec == PREC_PRE || prec == PREC_POST);
}

static int
is_right_associativity(operator prec)
{
	return (prec == PREC(TOK_ASSIGN) || prec == PREC(TOK_EXPONENT)
	        || prec == PREC(TOK_CONDITIONAL));
}

typedef struct {
	arith_t val;
	arith_t contidional_second_val;
	char contidional_second_val_initialized;
	char *var;      /* if NULL then is regular number,
			   else is variable name */
} v_n_t;

typedef struct chk_var_recursive_looped_t {
	const char *var;
	struct chk_var_recursive_looped_t *next;
} chk_var_recursive_looped_t;

static chk_var_recursive_looped_t *prev_chk_var_recursive;

static int
arith_lookup_val(v_n_t *t, a_e_h_t *math_hooks)
{
	if (t->var) {
		const char *p = lookupvar(t->var);

		if (p) {
			int errcode;

			/* recursive try as expression */
			chk_var_recursive_looped_t *cur;
			chk_var_recursive_looped_t cur_save;

			for (cur = prev_chk_var_recursive; cur; cur = cur->next) {
				if (strcmp(cur->var, t->var) == 0) {
					/* expression recursion loop detected */
					return -5;
				}
			}
			/* save current lookuped var name */
			cur = prev_chk_var_recursive;
			cur_save.var = t->var;
			cur_save.next = cur;
			prev_chk_var_recursive = &cur_save;

			t->val = arith (p, &errcode, math_hooks);
			/* restore previous ptr after recursiving */
			prev_chk_var_recursive = cur;
			return errcode;
		}
		/* allow undefined var as 0 */
		t->val = 0;
	}
	return 0;
}

/* "applying" a token means performing it on the top elements on the integer
 * stack. For a unary operator it will only change the top element, but a
 * binary operator will pop two arguments and push a result */
static NOINLINE int
arith_apply(operator op, v_n_t *numstack, v_n_t **numstackptr, a_e_h_t *math_hooks)
{
	v_n_t *numptr_m1;
	arith_t numptr_val, rez;
	int ret_arith_lookup_val;

	/* There is no operator that can work without arguments */
	if (NUMPTR == numstack) goto err;
	numptr_m1 = NUMPTR - 1;

	/* check operand is var with noninteger value */
	ret_arith_lookup_val = arith_lookup_val(numptr_m1, math_hooks);
	if (ret_arith_lookup_val)
		return ret_arith_lookup_val;

	rez = numptr_m1->val;
	if (op == TOK_UMINUS)
		rez *= -1;
	else if (op == TOK_NOT)
		rez = !rez;
	else if (op == TOK_BNOT)
		rez = ~rez;
	else if (op == TOK_POST_INC || op == TOK_PRE_INC)
		rez++;
	else if (op == TOK_POST_DEC || op == TOK_PRE_DEC)
		rez--;
	else if (op != TOK_UPLUS) {
		/* Binary operators */

		/* check and binary operators need two arguments */
		if (numptr_m1 == numstack) goto err;

		/* ... and they pop one */
		--NUMPTR;
		numptr_val = rez;
		if (op == TOK_CONDITIONAL) {
			if (!numptr_m1->contidional_second_val_initialized) {
				/* protect $((expr1 ? expr2)) without ": expr" */
				goto err;
			}
			rez = numptr_m1->contidional_second_val;
		} else if (numptr_m1->contidional_second_val_initialized) {
			/* protect $((expr1 : expr2)) without "expr ? " */
			goto err;
		}
		numptr_m1 = NUMPTR - 1;
		if (op != TOK_ASSIGN) {
			/* check operand is var with noninteger value for not '=' */
			ret_arith_lookup_val = arith_lookup_val(numptr_m1, math_hooks);
			if (ret_arith_lookup_val)
				return ret_arith_lookup_val;
		}
		if (op == TOK_CONDITIONAL) {
			numptr_m1->contidional_second_val = rez;
		}
		rez = numptr_m1->val;
		if (op == TOK_BOR || op == TOK_OR_ASSIGN)
			rez |= numptr_val;
		else if (op == TOK_OR)
			rez = numptr_val || rez;
		else if (op == TOK_BAND || op == TOK_AND_ASSIGN)
			rez &= numptr_val;
		else if (op == TOK_BXOR || op == TOK_XOR_ASSIGN)
			rez ^= numptr_val;
		else if (op == TOK_AND)
			rez = rez && numptr_val;
		else if (op == TOK_EQ)
			rez = (rez == numptr_val);
		else if (op == TOK_NE)
			rez = (rez != numptr_val);
		else if (op == TOK_GE)
			rez = (rez >= numptr_val);
		else if (op == TOK_RSHIFT || op == TOK_RSHIFT_ASSIGN)
			rez >>= numptr_val;
		else if (op == TOK_LSHIFT || op == TOK_LSHIFT_ASSIGN)
			rez <<= numptr_val;
		else if (op == TOK_GT)
			rez = (rez > numptr_val);
		else if (op == TOK_LT)
			rez = (rez < numptr_val);
		else if (op == TOK_LE)
			rez = (rez <= numptr_val);
		else if (op == TOK_MUL || op == TOK_MUL_ASSIGN)
			rez *= numptr_val;
		else if (op == TOK_ADD || op == TOK_PLUS_ASSIGN)
			rez += numptr_val;
		else if (op == TOK_SUB || op == TOK_MINUS_ASSIGN)
			rez -= numptr_val;
		else if (op == TOK_ASSIGN || op == TOK_COMMA)
			rez = numptr_val;
		else if (op == TOK_CONDITIONAL_SEP) {
			if (numptr_m1 == numstack) {
				/* protect $((expr : expr)) without "expr ? " */
				goto err;
			}
			numptr_m1->contidional_second_val_initialized = op;
			numptr_m1->contidional_second_val = numptr_val;
		} else if (op == TOK_CONDITIONAL) {
			rez = rez ?
				numptr_val : numptr_m1->contidional_second_val;
		} else if (op == TOK_EXPONENT) {
			if (numptr_val < 0)
				return -3;      /* exponent less than 0 */
			else {
				arith_t c = 1;

				if (numptr_val)
					while (numptr_val--)
						c *= rez;
				rez = c;
			}
		} else if (numptr_val==0)          /* zero divisor check */
			return -2;
		else if (op == TOK_DIV || op == TOK_DIV_ASSIGN)
			rez /= numptr_val;
		else if (op == TOK_REM || op == TOK_REM_ASSIGN)
			rez %= numptr_val;
	}
	if (tok_have_assign(op)) {
		char buf[sizeof(arith_t)*3 + 2];

		if (numptr_m1->var == NULL) {
			/* Hmm, 1=2 ? */
			goto err;
		}
		/* save to shell variable */
		sprintf(buf, arith_t_fmt, rez);
		setvar(numptr_m1->var, buf);
		/* after saving, make previous value for v++ or v-- */
		if (op == TOK_POST_INC)
			rez--;
		else if (op == TOK_POST_DEC)
			rez++;
	}
	numptr_m1->val = rez;
	/* protect geting var value, is number now */
	numptr_m1->var = NULL;
	return 0;
 err:
	return -1;
}

/* longest must be first */
static const char op_tokens[] ALIGN1 = {
	'<','<','=',0, TOK_LSHIFT_ASSIGN,
	'>','>','=',0, TOK_RSHIFT_ASSIGN,
	'<','<',    0, TOK_LSHIFT,
	'>','>',    0, TOK_RSHIFT,
	'|','|',    0, TOK_OR,
	'&','&',    0, TOK_AND,
	'!','=',    0, TOK_NE,
	'<','=',    0, TOK_LE,
	'>','=',    0, TOK_GE,
	'=','=',    0, TOK_EQ,
	'|','=',    0, TOK_OR_ASSIGN,
	'&','=',    0, TOK_AND_ASSIGN,
	'*','=',    0, TOK_MUL_ASSIGN,
	'/','=',    0, TOK_DIV_ASSIGN,
	'%','=',    0, TOK_REM_ASSIGN,
	'+','=',    0, TOK_PLUS_ASSIGN,
	'-','=',    0, TOK_MINUS_ASSIGN,
	'-','-',    0, TOK_POST_DEC,
	'^','=',    0, TOK_XOR_ASSIGN,
	'+','+',    0, TOK_POST_INC,
	'*','*',    0, TOK_EXPONENT,
	'!',        0, TOK_NOT,
	'<',        0, TOK_LT,
	'>',        0, TOK_GT,
	'=',        0, TOK_ASSIGN,
	'|',        0, TOK_BOR,
	'&',        0, TOK_BAND,
	'*',        0, TOK_MUL,
	'/',        0, TOK_DIV,
	'%',        0, TOK_REM,
	'+',        0, TOK_ADD,
	'-',        0, TOK_SUB,
	'^',        0, TOK_BXOR,
	/* uniq */
	'~',        0, TOK_BNOT,
	',',        0, TOK_COMMA,
	'?',        0, TOK_CONDITIONAL,
	':',        0, TOK_CONDITIONAL_SEP,
	')',        0, TOK_RPAREN,
	'(',        0, TOK_LPAREN,
	0
};
/* ptr to ")" */
#define endexpression (&op_tokens[sizeof(op_tokens)-7])

arith_t
arith(const char *expr, int *perrcode, a_e_h_t *math_hooks)
{
	char arithval; /* Current character under analysis */
	operator lasttok, op;
	operator prec;
	operator *stack, *stackptr;
	const char *p = endexpression;
	int errcode;
	v_n_t *numstack, *numstackptr;
	unsigned datasizes = strlen(expr) + 2;

	/* Stack of integers */
	/* The proof that there can be no more than strlen(startbuf)/2+1 integers
	 * in any given correct or incorrect expression is left as an exercise to
	 * the reader. */
	numstackptr = numstack = alloca((datasizes / 2) * sizeof(numstack[0]));
	/* Stack of operator tokens */
	stackptr = stack = alloca(datasizes * sizeof(stack[0]));

	*stackptr++ = lasttok = TOK_LPAREN;     /* start off with a left paren */
	*perrcode = errcode = 0;

	while (1) {
		arithval = *expr;
		if (arithval == 0) {
			if (p == endexpression) {
				/* Null expression. */
				return 0;
			}

			/* This is only reached after all tokens have been extracted from the
			 * input stream. If there are still tokens on the operator stack, they
			 * are to be applied in order. At the end, there should be a final
			 * result on the integer stack */

			if (expr != endexpression + 1) {
				/* If we haven't done so already, */
				/* append a closing right paren */
				expr = endexpression;
				/* and let the loop process it. */
				continue;
			}
			/* At this point, we're done with the expression. */
			if (numstackptr != numstack+1) {
				/* ... but if there isn't, it's bad */
 err:
				*perrcode = -1;
				return *perrcode;
			}
			if (numstack->var) {
				/* expression is $((var)) only, lookup now */
				errcode = arith_lookup_val(numstack, math_hooks);
			}
 ret:
			*perrcode = errcode;
			return numstack->val;
		}

		/* Continue processing the expression. */
		if (arith_isspace(arithval)) {
			/* Skip whitespace */
			goto prologue;
		}
		p = endofname(expr);
		if (p != expr) {
			size_t var_name_size = (p-expr) + 1;  /* trailing zero */

			numstackptr->var = alloca(var_name_size);
			safe_strncpy(numstackptr->var, expr, var_name_size);
			expr = p;
 num:
			numstackptr->contidional_second_val_initialized = 0;
			numstackptr++;
			lasttok = TOK_NUM;
			continue;
		}
		if (isdigit(arithval)) {
			numstackptr->var = NULL;
			errno = 0;
			/* call strtoul[l]: */
			numstackptr->val = strto_arith_t(expr, (char **) &expr, 0);
			if (errno)
				numstackptr->val = 0; /* bash compat */
			goto num;
		}
		for (p = op_tokens; ; p++) {
			const char *o;

			if (*p == 0) {
				/* strange operator not found */
				goto err;
			}
			for (o = expr; *p && *o == *p; p++)
				o++;
			if (!*p) {
				/* found */
				expr = o - 1;
				break;
			}
			/* skip tail uncompared token */
			while (*p)
				p++;
			/* skip zero delim */
			p++;
		}
		op = p[1];

		/* post grammar: a++ reduce to num */
		if (lasttok == TOK_POST_INC || lasttok == TOK_POST_DEC)
			lasttok = TOK_NUM;

		/* Plus and minus are binary (not unary) _only_ if the last
		 * token was a number, or a right paren (which pretends to be
		 * a number, since it evaluates to one). Think about it.
		 * It makes sense. */
		if (lasttok != TOK_NUM) {
			switch (op) {
			case TOK_ADD:
				op = TOK_UPLUS;
				break;
			case TOK_SUB:
				op = TOK_UMINUS;
				break;
			case TOK_POST_INC:
				op = TOK_PRE_INC;
				break;
			case TOK_POST_DEC:
				op = TOK_PRE_DEC;
				break;
			}
		}
		/* We don't want an unary operator to cause recursive descent on the
		 * stack, because there can be many in a row and it could cause an
		 * operator to be evaluated before its argument is pushed onto the
		 * integer stack. */
		/* But for binary operators, "apply" everything on the operator
		 * stack until we find an operator with a lesser priority than the
		 * one we have just extracted. */
		/* Left paren is given the lowest priority so it will never be
		 * "applied" in this way.
		 * if associativity is right and priority eq, applied also skip
		 */
		prec = PREC(op);
		if ((prec > 0 && prec < UNARYPREC) || prec == SPEC_PREC) {
			/* not left paren or unary */
			if (lasttok != TOK_NUM) {
				/* binary op must be preceded by a num */
				goto err;
			}
			while (stackptr != stack) {
				if (op == TOK_RPAREN) {
					/* The algorithm employed here is simple: while we don't
					 * hit an open paren nor the bottom of the stack, pop
					 * tokens and apply them */
					if (stackptr[-1] == TOK_LPAREN) {
						--stackptr;
						/* Any operator directly after a */
						lasttok = TOK_NUM;
						/* close paren should consider itself binary */
						goto prologue;
					}
				} else {
					operator prev_prec = PREC(stackptr[-1]);

					convert_prec_is_assing(prec);
					convert_prec_is_assing(prev_prec);
					if (prev_prec < prec)
						break;
					/* check right assoc */
					if (prev_prec == prec && is_right_associativity(prec))
						break;
				}
				errcode = arith_apply(*--stackptr, numstack, &numstackptr, math_hooks);
				if (errcode) goto ret;
			}
			if (op == TOK_RPAREN) {
				goto err;
			}
		}

		/* Push this operator to the stack and remember it. */
		*stackptr++ = lasttok = op;
 prologue:
		++expr;
	} /* while */
}

/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

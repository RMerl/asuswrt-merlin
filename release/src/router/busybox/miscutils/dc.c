/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <math.h>

/* Tiny RPN calculator, because "expr" didn't give me bitwise operations. */


struct globals {
	unsigned pointer;
	unsigned base;
	double stack[1];
} FIX_ALIASING;
enum { STACK_SIZE = (COMMON_BUFSIZE - offsetof(struct globals, stack)) / sizeof(double) };
#define G (*(struct globals*)&bb_common_bufsiz1)
#define pointer   (G.pointer   )
#define base      (G.base      )
#define stack     (G.stack     )
#define INIT_G() do { \
	base = 10; \
} while (0)


static void push(double a)
{
	if (pointer >= STACK_SIZE)
		bb_error_msg_and_die("stack overflow");
	stack[pointer++] = a;
}

static double pop(void)
{
	if (pointer == 0)
		bb_error_msg_and_die("stack underflow");
	return stack[--pointer];
}

static void add(void)
{
	push(pop() + pop());
}

static void sub(void)
{
	double subtrahend = pop();

	push(pop() - subtrahend);
}

static void mul(void)
{
	push(pop() * pop());
}

#if ENABLE_FEATURE_DC_LIBM
static void power(void)
{
	double topower = pop();

	push(pow(pop(), topower));
}
#endif

static void divide(void)
{
	double divisor = pop();

	push(pop() / divisor);
}

static void mod(void)
{
	unsigned d = pop();

	push((unsigned) pop() % d);
}

static void and(void)
{
	push((unsigned) pop() & (unsigned) pop());
}

static void or(void)
{
	push((unsigned) pop() | (unsigned) pop());
}

static void eor(void)
{
	push((unsigned) pop() ^ (unsigned) pop());
}

static void not(void)
{
	push(~(unsigned) pop());
}

static void set_output_base(void)
{
	static const char bases[] ALIGN1 = { 2, 8, 10, 16, 0 };
	unsigned b = (unsigned)pop();

	base = *strchrnul(bases, b);
	if (base == 0) {
		bb_error_msg("error, base %u is not supported", b);
		base = 10;
	}
}

static void print_base(double print)
{
	unsigned x, i;

	if (base == 10) {
		printf("%g\n", print);
		return;
	}

	x = (unsigned)print;
	switch (base) {
	case 16:
		printf("%x\n", x);
		break;
	case 8:
		printf("%o\n", x);
		break;
	default: /* base 2 */
		i = (unsigned)INT_MAX + 1;
		do {
			if (x & i) break;
			i >>= 1;
		} while (i > 1);
		do {
			bb_putchar('1' - !(x & i));
			i >>= 1;
		} while (i);
		bb_putchar('\n');
	}
}

static void print_stack_no_pop(void)
{
	unsigned i = pointer;
	while (i)
		print_base(stack[--i]);
}

static void print_no_pop(void)
{
	print_base(stack[pointer-1]);
}

struct op {
	const char name[4];
	void (*function) (void);
};

static const struct op operators[] = {
	{"+",   add},
	{"add", add},
	{"-",   sub},
	{"sub", sub},
	{"*",   mul},
	{"mul", mul},
	{"/",   divide},
	{"div", divide},
#if ENABLE_FEATURE_DC_LIBM
	{"**",  power},
	{"exp", power},
	{"pow", power},
#endif
	{"%",   mod},
	{"mod", mod},
	{"and", and},
	{"or",  or},
	{"not", not},
	{"eor", eor},
	{"xor", eor},
	{"p", print_no_pop},
	{"f", print_stack_no_pop},
	{"o", set_output_base},
	{ "", NULL }
};

static void stack_machine(const char *argument)
{
	char *endPointer;
	double d;
	const struct op *o = operators;

	d = strtod(argument, &endPointer);

	if (endPointer != argument && *endPointer == '\0') {
		push(d);
		return;
	}

	while (o->function) {
		if (strcmp(o->name, argument) == 0) {
			o->function();
			return;
		}
		o++;
	}
	bb_error_msg_and_die("syntax error at '%s'", argument);
}

/* return pointer to next token in buffer and set *buffer to one char
 * past the end of the above mentioned token
 */
static char *get_token(char **buffer)
{
	char *current = skip_whitespace(*buffer);
	if (*current != '\0') {
		*buffer = skip_non_whitespace(current);
		return current;
	}
	return NULL;
}

int dc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dc_main(int argc UNUSED_PARAM, char **argv)
{
	INIT_G();

	argv++;
	if (!argv[0]) {
		/* take stuff from stdin if no args are given */
		char *line;
		char *cursor;
		char *token;
		while ((line = xmalloc_fgetline(stdin)) != NULL) {
			cursor = line;
			while (1) {
				token = get_token(&cursor);
				if (!token)
					break;
				*cursor++ = '\0';
				stack_machine(token);
			}
			free(line);
		}
	} else {
		// why? it breaks "dc -2 2 * p"
		//if (argv[0][0] == '-')
		//	bb_show_usage();
		do {
			stack_machine(*argv);
		} while (*++argv);
	}
	return EXIT_SUCCESS;
}

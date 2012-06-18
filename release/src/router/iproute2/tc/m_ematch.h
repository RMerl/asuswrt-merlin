#ifndef __TC_EMATCH_H_
#define __TC_EMATCH_H_

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"

#define EMATCHKINDSIZ 16

struct bstr
{
	char	*data;
	unsigned int	len;
	int		quoted;
	struct bstr	*next;
};

static inline struct bstr * bstr_alloc(const char *text)
{
	struct bstr *b = calloc(1, sizeof(*b));

	if (b == NULL)
		return NULL;

	b->data = strdup(text);
	if (b->data == NULL) {
		free(b);
		return NULL;
	}

	b->len = strlen(text);

	return b;
}

static inline struct bstr * bstr_new(char *data, unsigned int len)
{
	struct bstr *b = calloc(1, sizeof(*b));

	if (b == NULL)
		return NULL;

	b->data = data;
	b->len = len;

	return b;
}

static inline int bstrcmp(struct bstr *b, const char *text)
{
	int len = strlen(text);
	int d = b->len - len;

	if (d == 0)
		return strncmp(b->data, text, len);

	return d;
}

static inline unsigned long bstrtoul(struct bstr *b)
{
	char *inv = NULL;
	unsigned long l;
	char buf[b->len+1];

	memcpy(buf, b->data, b->len);
	buf[b->len] = '\0';
	
	l = strtol(buf, &inv, 0);
	if (l == ULONG_MAX || inv == buf)
		return LONG_MAX;

	return l;
}

static inline void bstr_print(FILE *fd, struct bstr *b, int ascii)
{
	int i;
	char *s = b->data;

	if (ascii)
		for (i = 0; i < b->len; i++)
		    fprintf(fd, "%c", isprint(s[i]) ? s[i] : '.');
	else {
		for (i = 0; i < b->len; i++)
		    fprintf(fd, "%02x", s[i]);
		fprintf(fd, "\"");
		for (i = 0; i < b->len; i++)
		    fprintf(fd, "%c", isprint(s[i]) ? s[i] : '.');
		fprintf(fd, "\"");
	}
}

static inline struct bstr *bstr_next(struct bstr *b)
{
	return b->next;
}

struct ematch
{
	struct bstr	*args;
	int		index;
	int		inverted;
	int		relation;
	int		child_ref;
	struct ematch	*child;
	struct ematch	*next;
};

static inline struct ematch * new_ematch(struct bstr *args, int inverted)
{
	struct ematch *e = calloc(1, sizeof(*e));

	if (e == NULL)
		return NULL;

	e->args = args;
	e->inverted = inverted;

	return e;
}

static inline void print_ematch_tree(struct ematch *tree)
{
	struct ematch *t;

	for (t = tree; t; t = t->next) {
		if (t->inverted)
			printf("NOT ");

		if (t->child) {
			printf("(");
			print_ematch_tree(t->child);
			printf(")");
		} else {
			struct bstr *b;
			for (b = t->args; b; b = b->next)
				printf("%s%s", b->data, b->next ? " " : "");
		}

		if (t->relation == TCF_EM_REL_AND)
			printf(" AND ");
		else if (t->relation == TCF_EM_REL_OR)
			printf(" OR ");
	}
}

struct ematch_util
{
	char			kind[EMATCHKINDSIZ];
	int			kind_num;
	int	(*parse_eopt)(struct nlmsghdr *,struct tcf_ematch_hdr *,
			      struct bstr *);
	int	(*print_eopt)(FILE *, struct tcf_ematch_hdr *, void *, int);
	void	(*print_usage)(FILE *);
	struct ematch_util	*next;
};

static inline int parse_layer(struct bstr *b)
{
	if (*((char *) b->data) == 'l')
		return TCF_LAYER_LINK;
	else if (*((char *) b->data) == 'n')
		return TCF_LAYER_NETWORK;
	else if (*((char *) b->data) == 't')
		return TCF_LAYER_TRANSPORT;
	else
		return INT_MAX;
}

extern int em_parse_error(int err, struct bstr *args, struct bstr *carg,
		   struct ematch_util *, char *fmt, ...);
extern int print_ematch(FILE *, const struct rtattr *);
extern int parse_ematch(int *, char ***, int, struct nlmsghdr *);

#endif

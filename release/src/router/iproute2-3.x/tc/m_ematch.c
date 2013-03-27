/*
 * m_ematch.c		Extended Matches
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Thomas Graf <tgraf@suug.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <errno.h>

#include "utils.h"
#include "tc_util.h"
#include "m_ematch.h"

#define EMATCH_MAP "/etc/iproute2/ematch_map"

static struct ematch_util *ematch_list;

/* export to bison parser */
int ematch_argc;
char **ematch_argv;
char *ematch_err = NULL;
struct ematch *ematch_root;

static int begin_argc;
static char **begin_argv;

static inline void map_warning(int num, char *kind)
{
	fprintf(stderr,
	    "Error: Unable to find ematch \"%s\" in %s\n" \
	    "Please assign a unique ID to the ematch kind the suggested " \
	    "entry is:\n" \
	    "\t%d\t%s\n",
	    kind, EMATCH_MAP, num, kind);
}

static int lookup_map(__u16 num, char *dst, int len, const char *file)
{
	int err = -EINVAL;
	char buf[512];
	FILE *fd = fopen(file, "r");

	if (fd == NULL)
		return -errno;

	while (fgets(buf, sizeof(buf), fd)) {
		char namebuf[512], *p = buf;
		int id;

		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '#' || *p == '\n' || *p == 0)
			continue;

		if (sscanf(p, "%d %s", &id, namebuf) != 2) {
			fprintf(stderr, "ematch map %s corrupted at %s\n",
			    file, p);
			goto out;
		}

		if (id == num) {
			if (dst)
				strncpy(dst, namebuf, len - 1);
			err = 0;
			goto out;
		}
	}

	err = -ENOENT;
out:
	fclose(fd);
	return err;
}

static int lookup_map_id(char *kind, int *dst, const char *file)
{
	int err = -EINVAL;
	char buf[512];
	FILE *fd = fopen(file, "r");

	if (fd == NULL)
		return -errno;

	while (fgets(buf, sizeof(buf), fd)) {
		char namebuf[512], *p = buf;
		int id;

		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '#' || *p == '\n' || *p == 0)
			continue;

		if (sscanf(p, "%d %s", &id, namebuf) != 2) {
			fprintf(stderr, "ematch map %s corrupted at %s\n",
			    file, p);
			goto out;
		}

		if (!strcasecmp(namebuf, kind)) {
			if (dst)
				*dst = id;
			err = 0;
			goto out;
		}
	}

	err = -ENOENT;
	*dst = 0;
out:
	fclose(fd);
	return err;
}

static struct ematch_util *get_ematch_kind(char *kind)
{
	static void *body;
	void *dlh;
	char buf[256];
	struct ematch_util *e;

	for (e = ematch_list; e; e = e->next) {
		if (strcmp(e->kind, kind) == 0)
			return e;
	}

	snprintf(buf, sizeof(buf), "em_%s.so", kind);
	dlh = dlopen(buf, RTLD_LAZY);
	if (dlh == NULL) {
		dlh = body;
		if (dlh == NULL) {
			dlh = body = dlopen(NULL, RTLD_LAZY);
			if (dlh == NULL)
				return NULL;
		}
	}

	snprintf(buf, sizeof(buf), "%s_ematch_util", kind);
	e = dlsym(dlh, buf);
	if (e == NULL)
		return NULL;

	e->next = ematch_list;
	ematch_list = e;

	return e;
}

static struct ematch_util *get_ematch_kind_num(__u16 kind)
{
	char name[32];

	if (lookup_map(kind, name, sizeof(name), EMATCH_MAP) < 0)
		return NULL;

	return get_ematch_kind(name);
}

static int parse_tree(struct nlmsghdr *n, struct ematch *tree)
{
	int index = 1;
	struct ematch *t;

	for (t = tree; t; t = t->next) {
		struct rtattr *tail = NLMSG_TAIL(n);
		struct tcf_ematch_hdr hdr = {
			.flags = t->relation
		};

		if (t->inverted)
			hdr.flags |= TCF_EM_INVERT;

		addattr_l(n, MAX_MSG, index++, NULL, 0);

		if (t->child) {
			__u32 r = t->child_ref;
			addraw_l(n, MAX_MSG, &hdr, sizeof(hdr));
			addraw_l(n, MAX_MSG, &r, sizeof(r));
		} else {
			int num = 0, err;
			char buf[64];
			struct ematch_util *e;

			if (t->args == NULL)
				return -1;

			strncpy(buf, (char*) t->args->data, sizeof(buf)-1);
			e = get_ematch_kind(buf);
			if (e == NULL) {
				fprintf(stderr, "Unknown ematch \"%s\"\n",
				    buf);
				return -1;
			}

			err = lookup_map_id(buf, &num, EMATCH_MAP);
			if (err < 0) {
				if (err == -ENOENT)
					map_warning(e->kind_num, buf);
				return err;
			}

			hdr.kind = num;
			if (e->parse_eopt(n, &hdr, t->args->next) < 0)
				return -1;
		}

		tail->rta_len = (void*) NLMSG_TAIL(n) - (void*) tail;
	}

	return 0;
}

static int flatten_tree(struct ematch *head, struct ematch *tree)
{
	int i, count = 0;
	struct ematch *t;

	for (;;) {
		count++;

		if (tree->child) {
			for (t = head; t->next; t = t->next);
			t->next = tree->child;
			count += flatten_tree(head, tree->child);
		}

		if (tree->relation == 0)
			break;

		tree = tree->next;
	}

	for (i = 0, t = head; t; t = t->next, i++)
		t->index = i;

	for (t = head; t; t = t->next)
		if (t->child)
			t->child_ref = t->child->index;

	return count;
}

int em_parse_error(int err, struct bstr *args, struct bstr *carg,
		   struct ematch_util *e, char *fmt, ...)
{
	va_list a;

	va_start(a, fmt);
	vfprintf(stderr, fmt, a);
	va_end(a);

	if (ematch_err)
		fprintf(stderr, ": %s\n... ", ematch_err);
	else
		fprintf(stderr, "\n... ");

	while (ematch_argc < begin_argc) {
		if (ematch_argc == (begin_argc - 1))
			fprintf(stderr, ">>%s<< ", *begin_argv);
		else
			fprintf(stderr, "%s ", *begin_argv);
		begin_argv++;
		begin_argc--;
	}

	fprintf(stderr, "...\n");

	if (args) {
		fprintf(stderr, "... %s(", e->kind);
		while (args) {
			fprintf(stderr, "%s", args == carg ? ">>" : "");
			bstr_print(stderr, args, 1);
			fprintf(stderr, "%s%s", args == carg ? "<<" : "",
			    args->next ? " " : "");
			args = args->next;
		}
		fprintf(stderr, ")...\n");

	}

	if (e == NULL) {
		fprintf(stderr,
		    "Usage: EXPR\n" \
		    "where: EXPR  := TERM [ { and | or } EXPR ]\n" \
		    "       TERM  := [ not ] { MATCH | '(' EXPR ')' }\n" \
		    "       MATCH := module '(' ARGS ')'\n" \
		    "       ARGS := ARG1 ARG2 ...\n" \
		    "\n" \
		    "Example: a(x y) and not (b(x) or c(x y z))\n");
	} else
		e->print_usage(stderr);

	return -err;
}

static inline void free_ematch_err(void)
{
	if (ematch_err) {
		free(ematch_err);
		ematch_err = NULL;
	}
}

extern int ematch_parse(void);

int parse_ematch(int *argc_p, char ***argv_p, int tca_id, struct nlmsghdr *n)
{
	begin_argc = ematch_argc = *argc_p;
	begin_argv = ematch_argv = *argv_p;

	if (ematch_parse()) {
		int err = em_parse_error(EINVAL, NULL, NULL, NULL,
		    "Parse error");
		free_ematch_err();
		return err;
	}

	free_ematch_err();

	/* undo look ahead by parser */
	ematch_argc++;
	ematch_argv--;

	if (ematch_root) {
		struct rtattr *tail, *tail_list;

		struct tcf_ematch_tree_hdr hdr = {
			.nmatches = flatten_tree(ematch_root, ematch_root),
			.progid = TCF_EM_PROG_TC
		};

		tail = NLMSG_TAIL(n);
		addattr_l(n, MAX_MSG, tca_id, NULL, 0);
		addattr_l(n, MAX_MSG, TCA_EMATCH_TREE_HDR, &hdr, sizeof(hdr));

		tail_list = NLMSG_TAIL(n);
		addattr_l(n, MAX_MSG, TCA_EMATCH_TREE_LIST, NULL, 0);

		if (parse_tree(n, ematch_root) < 0)
			return -1;

		tail_list->rta_len = (void*) NLMSG_TAIL(n) - (void*) tail_list;
		tail->rta_len = (void*) NLMSG_TAIL(n) - (void*) tail;
	}

	*argc_p = ematch_argc;
	*argv_p = ematch_argv;

	return 0;
}

static int print_ematch_seq(FILE *fd, struct rtattr **tb, int start,
			    int prefix)
{
	int n, i = start;
	struct tcf_ematch_hdr *hdr;
	int dlen;
	void *data;

	for (;;) {
		if (tb[i] == NULL)
			return -1;

		dlen = RTA_PAYLOAD(tb[i]) - sizeof(*hdr);
		data = (void *) RTA_DATA(tb[i]) + sizeof(*hdr);

		if (dlen < 0)
			return -1;

		hdr = RTA_DATA(tb[i]);

		if (hdr->flags & TCF_EM_INVERT)
			fprintf(fd, "NOT ");

		if (hdr->kind == 0) {
			__u32 ref;

			if (dlen < sizeof(__u32))
				return -1;

			ref = *(__u32 *) data;
			fprintf(fd, "(\n");
			for (n = 0; n <= prefix; n++)
				fprintf(fd, "  ");
			if (print_ematch_seq(fd, tb, ref + 1, prefix + 1) < 0)
				return -1;
			for (n = 0; n < prefix; n++)
				fprintf(fd, "  ");
			fprintf(fd, ") ");

		} else {
			struct ematch_util *e;

			e = get_ematch_kind_num(hdr->kind);
			if (e == NULL)
				fprintf(fd, "[unknown ematch %d]\n",
				    hdr->kind);
			else {
				fprintf(fd, "%s(", e->kind);
				if (e->print_eopt(fd, hdr, data, dlen) < 0)
					return -1;
				fprintf(fd, ")\n");
			}
			if (hdr->flags & TCF_EM_REL_MASK)
				for (n = 0; n < prefix; n++)
					fprintf(fd, "  ");
		}

		switch (hdr->flags & TCF_EM_REL_MASK) {
			case TCF_EM_REL_AND:
				fprintf(fd, "AND ");
				break;

			case TCF_EM_REL_OR:
				fprintf(fd, "OR ");
				break;

			default:
				return 0;
		}

		i++;
	}

	return 0;
}

static int print_ematch_list(FILE *fd, struct tcf_ematch_tree_hdr *hdr,
			     struct rtattr *rta)
{
	int err = -1;
	struct rtattr **tb;

	tb = malloc((hdr->nmatches + 1) * sizeof(struct rtattr *));
	if (tb == NULL)
		return -1;

	if (hdr->nmatches > 0) {
		if (parse_rtattr_nested(tb, hdr->nmatches, rta) < 0)
			goto errout;

		fprintf(fd, "\n  ");
		if (print_ematch_seq(fd, tb, 1, 1) < 0)
			goto errout;
	}

	err = 0;
errout:
	free(tb);
	return err;
}

int print_ematch(FILE *fd, const struct rtattr *rta)
{
	struct rtattr *tb[TCA_EMATCH_TREE_MAX+1];
	struct tcf_ematch_tree_hdr *hdr;

	if (parse_rtattr_nested(tb, TCA_EMATCH_TREE_MAX, rta) < 0)
		return -1;

	if (tb[TCA_EMATCH_TREE_HDR] == NULL) {
		fprintf(stderr, "Missing ematch tree header\n");
		return -1;
	}

	if (tb[TCA_EMATCH_TREE_LIST] == NULL) {
		fprintf(stderr, "Missing ematch tree list\n");
		return -1;
	}

	if (RTA_PAYLOAD(tb[TCA_EMATCH_TREE_HDR]) < sizeof(*hdr)) {
		fprintf(stderr, "Ematch tree header size mismatch\n");
		return -1;
	}

	hdr = RTA_DATA(tb[TCA_EMATCH_TREE_HDR]);

	return print_ematch_list(fd, hdr, tb[TCA_EMATCH_TREE_LIST]);
}

struct bstr * bstr_alloc(const char *text)
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

unsigned long bstrtoul(const struct bstr *b)
{
	char *inv = NULL;
	unsigned long l;
	char buf[b->len+1];

	memcpy(buf, b->data, b->len);
	buf[b->len] = '\0';

	l = strtoul(buf, &inv, 0);
	if (l == ULONG_MAX || inv == buf)
		return ULONG_MAX;

	return l;
}

void bstr_print(FILE *fd, const struct bstr *b, int ascii)
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

void print_ematch_tree(const struct ematch *tree)
{
	const struct ematch *t;

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

/* Generate assembler source containing symbol information
 *
 * Copyright 2002       by Kai Germaschewski
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Usage: nm -n vmlinux | scripts/kallsyms [--all-symbols] > symbols.S
 *
 * ChangeLog:
 *
 * (25/Aug/2004) Paulo Marques <pmarques@grupopie.com>
 *      Changed the compression method from stem compression to "table lookup"
 *      compression
 *
 *      Table compression uses all the unused char codes on the symbols and
 *  maps these to the most used substrings (tokens). For instance, it might
 *  map char code 0xF7 to represent "write_" and then in every symbol where
 *  "write_" appears it can be replaced by 0xF7, saving 5 bytes.
 *      The used codes themselves are also placed in the table so that the
 *  decompresion can work without "special cases".
 *      Applied to kernel symbols, this usually produces a compression ratio
 *  of about 50%.
 *
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define KSYM_NAME_LEN		127


struct sym_entry {
	unsigned long long addr;
	unsigned int len;
	unsigned char *sym;
};


static struct sym_entry *table;
static unsigned int table_size, table_cnt;
static unsigned long long _text, _stext, _etext, _sinittext, _einittext, _sextratext, _eextratext;
static int all_symbols = 0;
static char symbol_prefix_char = '\0';

int token_profit[0x10000];

/* the table that holds the result of the compression */
unsigned char best_table[256][2];
unsigned char best_table_len[256];


static void usage(void)
{
	fprintf(stderr, "Usage: kallsyms [--all-symbols] [--symbol-prefix=<prefix char>] < in.map > out.S\n");
	exit(1);
}

/*
 * This ignores the intensely annoying "mapping symbols" found
 * in ARM ELF files: $a, $t and $d.
 */
static inline int is_arm_mapping_symbol(const char *str)
{
	return str[0] == '$' && strchr("atd", str[1])
	       && (str[2] == '\0' || str[2] == '.');
}

static int read_symbol(FILE *in, struct sym_entry *s)
{
	char str[500];
	char *sym, stype;
	int rc;

	rc = fscanf(in, "%llx %c %499s\n", &s->addr, &stype, str);
	if (rc != 3) {
		if (rc != EOF) {
			/* skip line */
			fgets(str, 500, in);
		}
		return -1;
	}

	sym = str;
	/* skip prefix char */
	if (symbol_prefix_char && str[0] == symbol_prefix_char)
		sym++;

	/* Ignore most absolute/undefined (?) symbols. */
	if (strcmp(sym, "_text") == 0)
		_text = s->addr;
	else if (strcmp(sym, "_stext") == 0)
		_stext = s->addr;
	else if (strcmp(sym, "_etext") == 0)
		_etext = s->addr;
	else if (strcmp(sym, "_sinittext") == 0)
		_sinittext = s->addr;
	else if (strcmp(sym, "_einittext") == 0)
		_einittext = s->addr;
	else if (strcmp(sym, "_sextratext") == 0)
		_sextratext = s->addr;
	else if (strcmp(sym, "_eextratext") == 0)
		_eextratext = s->addr;
	else if (toupper(stype) == 'A')
	{
		/* Keep these useful absolute symbols */
		if (strcmp(sym, "__kernel_syscall_via_break") &&
		    strcmp(sym, "__kernel_syscall_via_epc") &&
		    strcmp(sym, "__kernel_sigtramp") &&
		    strcmp(sym, "__gp"))
			return -1;

	}
	else if (toupper(stype) == 'U' ||
		 is_arm_mapping_symbol(sym))
		return -1;
	/* exclude also MIPS ELF local symbols ($L123 instead of .L123) */
	else if (str[0] == '$')
		return -1;

	/* include the type field in the symbol name, so that it gets
	 * compressed together */
	s->len = strlen(str) + 1;
	s->sym = malloc(s->len + 1);
	if (!s->sym) {
		fprintf(stderr, "kallsyms failure: "
			"unable to allocate required amount of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy((char *)s->sym + 1, str);
	s->sym[0] = stype;

	return 0;
}

static int symbol_valid(struct sym_entry *s)
{
	/* Symbols which vary between passes.  Passes 1 and 2 must have
	 * identical symbol lists.  The kallsyms_* symbols below are only added
	 * after pass 1, they would be included in pass 2 when --all-symbols is
	 * specified so exclude them to get a stable symbol list.
	 */
	static char *special_symbols[] = {
		"kallsyms_addresses",
		"kallsyms_num_syms",
		"kallsyms_names",
		"kallsyms_markers",
		"kallsyms_token_table",
		"kallsyms_token_index",

	/* Exclude linker generated symbols which vary between passes */
		"_SDA_BASE_",		/* ppc */
		"_SDA2_BASE_",		/* ppc */
		NULL };
	int i;
	int offset = 1;

	/* skip prefix char */
	if (symbol_prefix_char && *(s->sym + 1) == symbol_prefix_char)
		offset++;

	/* if --all-symbols is not specified, then symbols outside the text
	 * and inittext sections are discarded */
	if (!all_symbols) {
		if ((s->addr < _stext || s->addr > _etext)
		    && (s->addr < _sinittext || s->addr > _einittext)
		    && (s->addr < _sextratext || s->addr > _eextratext))
			return 0;
		/* Corner case.  Discard any symbols with the same value as
		 * _etext _einittext or _eextratext; they can move between pass
		 * 1 and 2 when the kallsyms data are added.  If these symbols
		 * move then they may get dropped in pass 2, which breaks the
		 * kallsyms rules.
		 */
		if ((s->addr == _etext && strcmp((char*)s->sym + offset, "_etext")) ||
		    (s->addr == _einittext && strcmp((char*)s->sym + offset, "_einittext")) ||
		    (s->addr == _eextratext && strcmp((char*)s->sym + offset, "_eextratext")))
			return 0;
	}

	/* Exclude symbols which vary between passes. */
	if (strstr((char *)s->sym + offset, "_compiled."))
		return 0;

	for (i = 0; special_symbols[i]; i++)
		if( strcmp((char *)s->sym + offset, special_symbols[i]) == 0 )
			return 0;

	return 1;
}

static void read_map(FILE *in)
{
	while (!feof(in)) {
		if (table_cnt >= table_size) {
			table_size += 10000;
			table = realloc(table, sizeof(*table) * table_size);
			if (!table) {
				fprintf(stderr, "out of memory\n");
				exit (1);
			}
		}
		if (read_symbol(in, &table[table_cnt]) == 0)
			table_cnt++;
	}
}

static void output_label(char *label)
{
	if (symbol_prefix_char)
		printf(".globl %c%s\n", symbol_prefix_char, label);
	else
		printf(".globl %s\n", label);
	printf("\tALGN\n");
	if (symbol_prefix_char)
		printf("%c%s:\n", symbol_prefix_char, label);
	else
		printf("%s:\n", label);
}

/* uncompress a compressed symbol. When this function is called, the best table
 * might still be compressed itself, so the function needs to be recursive */
static int expand_symbol(unsigned char *data, int len, char *result)
{
	int c, rlen, total=0;

	while (len) {
		c = *data;
		/* if the table holds a single char that is the same as the one
		 * we are looking for, then end the search */
		if (best_table[c][0]==c && best_table_len[c]==1) {
			*result++ = c;
			total++;
		} else {
			/* if not, recurse and expand */
			rlen = expand_symbol(best_table[c], best_table_len[c], result);
			total += rlen;
			result += rlen;
		}
		data++;
		len--;
	}
	*result=0;

	return total;
}

static void write_src(void)
{
	unsigned int i, k, off;
	unsigned int best_idx[256];
	unsigned int *markers;
	char buf[KSYM_NAME_LEN+1];

	printf("#include <asm/types.h>\n");
	printf("#if BITS_PER_LONG == 64\n");
	printf("#define PTR .quad\n");
	printf("#define ALGN .align 8\n");
	printf("#else\n");
	printf("#define PTR .long\n");
	printf("#define ALGN .align 4\n");
	printf("#endif\n");

	printf("\t.section .rodata, \"a\"\n");

	/* Provide proper symbols relocatability by their '_text'
	 * relativeness.  The symbol names cannot be used to construct
	 * normal symbol references as the list of symbols contains
	 * symbols that are declared static and are private to their
	 * .o files.  This prevents .tmp_kallsyms.o or any other
	 * object from referencing them.
	 */
	output_label("kallsyms_addresses");
	for (i = 0; i < table_cnt; i++) {
		if (toupper(table[i].sym[0]) != 'A') {
			if (_text <= table[i].addr)
				printf("\tPTR\t_text + %#llx\n",
					table[i].addr - _text);
			else
				printf("\tPTR\t_text - %#llx\n",
					_text - table[i].addr);
		} else {
			printf("\tPTR\t%#llx\n", table[i].addr);
		}
	}
	printf("\n");

	output_label("kallsyms_num_syms");
	printf("\tPTR\t%d\n", table_cnt);
	printf("\n");

	/* table of offset markers, that give the offset in the compressed stream
	 * every 256 symbols */
	markers = malloc(sizeof(unsigned int) * ((table_cnt + 255) / 256));
	if (!markers) {
		fprintf(stderr, "kallsyms failure: "
			"unable to allocate required memory\n");
		exit(EXIT_FAILURE);
	}

	output_label("kallsyms_names");
	off = 0;
	for (i = 0; i < table_cnt; i++) {
		if ((i & 0xFF) == 0)
			markers[i >> 8] = off;

		printf("\t.byte 0x%02x", table[i].len);
		for (k = 0; k < table[i].len; k++)
			printf(", 0x%02x", table[i].sym[k]);
		printf("\n");

		off += table[i].len + 1;
	}
	printf("\n");

	output_label("kallsyms_markers");
	for (i = 0; i < ((table_cnt + 255) >> 8); i++)
		printf("\tPTR\t%d\n", markers[i]);
	printf("\n");

	free(markers);

	output_label("kallsyms_token_table");
	off = 0;
	for (i = 0; i < 256; i++) {
		best_idx[i] = off;
		expand_symbol(best_table[i], best_table_len[i], buf);
		printf("\t.asciz\t\"%s\"\n", buf);
		off += strlen(buf) + 1;
	}
	printf("\n");

	output_label("kallsyms_token_index");
	for (i = 0; i < 256; i++)
		printf("\t.short\t%d\n", best_idx[i]);
	printf("\n");
}


/* table lookup compression functions */

/* count all the possible tokens in a symbol */
static void learn_symbol(unsigned char *symbol, int len)
{
	int i;

	for (i = 0; i < len - 1; i++)
		token_profit[ symbol[i] + (symbol[i + 1] << 8) ]++;
}

/* decrease the count for all the possible tokens in a symbol */
static void forget_symbol(unsigned char *symbol, int len)
{
	int i;

	for (i = 0; i < len - 1; i++)
		token_profit[ symbol[i] + (symbol[i + 1] << 8) ]--;
}

/* remove all the invalid symbols from the table and do the initial token count */
static void build_initial_tok_table(void)
{
	unsigned int i, pos;

	pos = 0;
	for (i = 0; i < table_cnt; i++) {
		if ( symbol_valid(&table[i]) ) {
			if (pos != i)
				table[pos] = table[i];
			learn_symbol(table[pos].sym, table[pos].len);
			pos++;
		}
	}
	table_cnt = pos;
}

/* replace a given token in all the valid symbols. Use the sampled symbols
 * to update the counts */
static void compress_symbols(unsigned char *str, int idx)
{
	unsigned int i, len, size;
	unsigned char *p1, *p2;

	for (i = 0; i < table_cnt; i++) {

		len = table[i].len;
		p1 = table[i].sym;

		/* find the token on the symbol */
		p2 = memmem(p1, len, str, 2);
		if (!p2) continue;

		/* decrease the counts for this symbol's tokens */
		forget_symbol(table[i].sym, len);

		size = len;

		do {
			*p2 = idx;
			p2++;
			size -= (p2 - p1);
			memmove(p2, p2 + 1, size);
			p1 = p2;
			len--;

			if (size < 2) break;

			/* find the token on the symbol */
			p2 = memmem(p1, size, str, 2);

		} while (p2);

		table[i].len = len;

		/* increase the counts for this symbol's new tokens */
		learn_symbol(table[i].sym, len);
	}
}

/* search the token with the maximum profit */
static int find_best_token(void)
{
	int i, best, bestprofit;

	bestprofit=-10000;
	best = 0;

	for (i = 0; i < 0x10000; i++) {
		if (token_profit[i] > bestprofit) {
			best = i;
			bestprofit = token_profit[i];
		}
	}
	return best;
}

/* this is the core of the algorithm: calculate the "best" table */
static void optimize_result(void)
{
	int i, best;

	/* using the '\0' symbol last allows compress_symbols to use standard
	 * fast string functions */
	for (i = 255; i >= 0; i--) {

		/* if this table slot is empty (it is not used by an actual
		 * original char code */
		if (!best_table_len[i]) {

			/* find the token with the breates profit value */
			best = find_best_token();

			/* place it in the "best" table */
			best_table_len[i] = 2;
			best_table[i][0] = best & 0xFF;
			best_table[i][1] = (best >> 8) & 0xFF;

			/* replace this token in all the valid symbols */
			compress_symbols(best_table[i], i);
		}
	}
}

/* start by placing the symbols that are actually used on the table */
static void insert_real_symbols_in_table(void)
{
	unsigned int i, j, c;

	memset(best_table, 0, sizeof(best_table));
	memset(best_table_len, 0, sizeof(best_table_len));

	for (i = 0; i < table_cnt; i++) {
		for (j = 0; j < table[i].len; j++) {
			c = table[i].sym[j];
			best_table[c][0]=c;
			best_table_len[c]=1;
		}
	}
}

static void optimize_token_table(void)
{
	build_initial_tok_table();

	insert_real_symbols_in_table();

	/* When valid symbol is not registered, exit to error */
	if (!table_cnt) {
		fprintf(stderr, "No valid symbol.\n");
		exit(1);
	}

	optimize_result();
}


int main(int argc, char **argv)
{
	if (argc >= 2) {
		int i;
		for (i = 1; i < argc; i++) {
			if(strcmp(argv[i], "--all-symbols") == 0)
				all_symbols = 1;
			else if (strncmp(argv[i], "--symbol-prefix=", 16) == 0) {
				char *p = &argv[i][16];
				/* skip quote */
				if ((*p == '"' && *(p+2) == '"') || (*p == '\'' && *(p+2) == '\''))
					p++;
				symbol_prefix_char = *p;
			} else
				usage();
		}
	} else if (argc != 1)
		usage();

	read_map(stdin);
	optimize_token_table();
	write_src();

	return 0;
}

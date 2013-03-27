%{
 #include "emp_ematch.yacc.h"
 #include "m_ematch.h"

 extern int ematch_argc;
 extern char **ematch_argv;

 #define yylval ematch_lval

 #define NEXT_EM_ARG() do { ematch_argc--; ematch_argv++; } while(0);

 #define YY_INPUT(buf, result, max_size)				\
 {									\
 next:									\
 	if (ematch_argc <= 0)						\
		result = YY_NULL;					\
	else if (**ematch_argv == '\0') {				\
		NEXT_EM_ARG();						\
		goto next;						\
	} else {							\
		if (max_size <= strlen(*ematch_argv) + 1) {		\
			fprintf(stderr, "match argument too long.\n");	\
			result = YY_NULL;				\
		} else {						\
			strcpy(buf, *ematch_argv);			\
			result = strlen(*ematch_argv) + 1;		\
			buf[result-1] = ' ';				\
			buf[result] = '\0';				\
			NEXT_EM_ARG();					\
		}							\
	}								\
 }

 static void __attribute__ ((unused)) yyunput (int c,char *buf_ptr  );
 static void __attribute__ ((unused)) yy_push_state (int  new_state );
 static void __attribute__ ((unused)) yy_pop_state  (void);
 static int  __attribute__ ((unused)) yy_top_state (void );

 static char *strbuf;
 static unsigned int strbuf_size;
 static unsigned int strbuf_index;

 static void strbuf_enlarge(void)
 {
 	strbuf_size += 512;
 	strbuf = realloc(strbuf, strbuf_size);
 }

 static void strbuf_append_char(char c)
 {
 	while (strbuf_index >= strbuf_size)
 		strbuf_enlarge();
 	strbuf[strbuf_index++] = c;
 }

 static void strbuf_append_charp(char *s)
 {
 	while (strbuf_index >= strbuf_size)
 		strbuf_enlarge();
 	memcpy(strbuf + strbuf_index, s, strlen(s));
 	strbuf_index += strlen(s);
 }

%}

%x lexstr

%option 8bit stack warn noyywrap prefix="ematch_"
%%
[ \t\r\n]+

\"					{
						if (strbuf == NULL) {
							strbuf_size = 512;
							strbuf = calloc(1, strbuf_size);
							if (strbuf == NULL)
								return ERROR;
						}
						strbuf_index = 0;

						BEGIN(lexstr);
					}

<lexstr>\"					{
						BEGIN(INITIAL);
						yylval.b = bstr_new(strbuf, strbuf_index);
						yylval.b->quoted = 1;
						return ATTRIBUTE;
					}

<lexstr>\\[0-7]{1,3}			{ /* octal escape sequence */
						int res;

						sscanf(yytext + 1, "%o", &res);
						if (res > 0xFF) {
							fprintf(stderr, "error: octal escape sequence" \
							" out of range\n");
							return ERROR;
						}
						strbuf_append_char((unsigned char) res);
					}

<lexstr>\\[0-9]+				{ /* catch wrong octal escape seq. */
						fprintf(stderr, "error: invalid octale escape sequence\n");
						return ERROR;
					}

<lexstr>\\x[0-9a-fA-F]{1,2}		{
						int res;

						sscanf(yytext + 2, "%x", &res);

						if (res > 0xFF) {
							fprintf(stderr, "error: hexadecimal escape " \
							"sequence out of range\n");
							return ERROR;
						}
						strbuf_append_char((unsigned char) res);
					}

<lexstr>\\n				strbuf_append_char('\n');
<lexstr>\\r				strbuf_append_char('\r');
<lexstr>\\t				strbuf_append_char('\t');
<lexstr>\\v				strbuf_append_char('\v');
<lexstr>\\b				strbuf_append_char('\b');
<lexstr>\\f				strbuf_append_char('\f');
<lexstr>\\a				strbuf_append_char('\a');

<lexstr>\\(.|\n)			strbuf_append_char(yytext[1]);
<lexstr>[^\\\n\"]+			strbuf_append_charp(yytext);

[aA][nN][dD]				return AND;
[oO][rR]				return OR;
[nN][oO][tT]				return NOT;
"("					|
")"					{
						return yylval.i = *yytext;
					}
[^ \t\r\n()]+				{
						yylval.b = bstr_alloc(yytext);
						if (yylval.b == NULL)
							return ERROR;
						return ATTRIBUTE;
					}
%%

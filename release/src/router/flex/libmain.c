/* libmain - flex run-time support library "main" function */

/* $Header: /ramdisk/repositories/20_cvs_clean_up/2011-02-11_sj/src/router/flex/libmain.c,v 1.1.1.1 2001-04-08 23:53:37 mhuang Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}

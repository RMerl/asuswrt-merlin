/* Reads in up to MAX bytes and runs regcomp against them TIMES times, using
the regular expression given on the command line.

Uses the Henry Spencer V8 regular expressions which the kernel version of 
l7-filter uses.

See ../LICENCE for copyright
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include "regexp/regexp.c"

#define MAX 1500
#define TIMES 100000
#define MAX_PATTERN_LEN 8192

static int hex2dec(char c)
{
        switch (c)
        {
                case '0' ... '9':
                        return c - '0';
                case 'a' ... 'f':
                        return c - 'a' + 10;
                case 'A' ... 'F':
                        return c - 'A' + 10;
                default:
			fprintf(stderr, "hex2dec: bad value!\n");
                        exit(1);
        }
}

/* takes a string with \xHH escapes and returns one with the characters
they stand for */
static char * pre_process(char * s)
{
        char * result = malloc(strlen(s) + 1);
        int sindex = 0, rindex = 0;
        while( sindex < strlen(s) )  
        {
            if( sindex + 3 < strlen(s) &&
                s[sindex] == '\\' && s[sindex+1] == 'x' &&
                isxdigit(s[sindex + 2]) && isxdigit(s[sindex + 3]) )
                {
                        /* carefully remember to call tolower here... */
                        result[rindex] = tolower( hex2dec(s[sindex + 2])*16 +
                                                  hex2dec(s[sindex + 3] ) );
                        sindex += 3; /* 4 total */
                }
                else 
                        result[rindex] = tolower(s[sindex]);

                sindex++;  
                rindex++;
        }
        result[rindex] = '\0';

        return result;
}


void doit(regexp * pattern, char ** argv, int verbose)
{
	char input[MAX];
	int c;

	for(c = 0; c < MAX; c++){
		char temp = 0;
		while(temp == 0){
			if(EOF == scanf("%c", &temp))
				goto out;
			input[c] = temp;
		}
	}
	out:

	input[c-1] = '\0';

	for(c = 0; c < MAX; c++) input[c] = tolower(input[c]);

	for(c = 1; c < TIMES; c++){
		int result = regexec(pattern, input);
		if(c == 1)
			if(result)
				printf("match\t");
			else
				printf("no_match\t");

		if(TIMES/20 > 0 && c%(TIMES/20) == 0){ fprintf(stderr, "."); }
	}
	if(verbose)
		puts("");
	else
		printf(" ");
}

// Syntax: test_speed regex [verbose]
int main(int argc, char ** argv)
{
	regexp * pattern = (regexp *)malloc(sizeof(struct regexp));
	char * s = argv[1];
	int patternlen, i, verbose = 0;

	if(argc < 2){
		fprintf(stderr, "need an arg\n");
		return 1;
	}
	if(argc > 2)
		verbose = 1;

	patternlen = strlen(s);
	if(patternlen > MAX_PATTERN_LEN){
		fprintf(stderr, "Pattern too long! Max is %d\n", MAX_PATTERN_LEN);
		return 1;
	}

	s = pre_process(s); /* do \xHH escapes */

	pattern = regcomp(s, &patternlen);

	if(!pattern){
		fprintf(stderr, "error compiling regexp\n");
		exit(1);
	}

	if(verbose)
		printf("running regexec \"%.16s...\" %d times\n", argv[1], TIMES);

	doit(pattern, argv, verbose);

	return 0;
}

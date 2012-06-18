/* Reads in up to MAX bytes and runs regcomp against them TIMES times, 
using the regular expression given on the command line.

Uses the standard GNU regular expression library which the userspace 
version of l7-filter uses.

See ../LICENCE for copyright
*/

using namespace std;

#include <ctype.h>
#include <stdio.h>
#include <regex.h>
#include <getopt.h>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <stdlib.h>
#include "l7-parse-patterns.h"

#define MAX 512

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
void pre_process(string & s)
{
        char * result = (char *)malloc(s.size() + 1);
        unsigned int sindex = 0, rindex = 0;

        while( sindex < s.size() )  
        {
            if( sindex + 3 < s.size() &&
                s[sindex] == '\\' && s[sindex+1] == 'x' &&
                isxdigit(s[sindex + 2]) && isxdigit(s[sindex + 3]) )
            {
                result[rindex] = hex2dec(s[sindex+2])*16 + hex2dec(s[sindex+3]);
                sindex += 3; /* 4 total */
            }
            else
                    result[rindex] = s[sindex];

            sindex++;  
            rindex++;
        }
        result[rindex] = '\0';

	s = result;
}


void doit(regex_t * pattern, int eflags, int verbose, int nexec)
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

	for(c = 0; c < nexec; c++){
		int result = regexec(pattern, input, 0, 0, eflags);
		if(c == 0){
			if(result == 0) printf("match\t");
			else 	        printf("no_match\t");
		}
		if(nexec/20 > 0 && c%(nexec/20) == 0) fprintf(stderr, ".");
	}

	if(verbose) puts("");
	else        printf(" ");
}

void handle_cmdline(string & filename, int * nexecs, int * verbose, 
	int argc, char ** argv)
{
	const char * opts = "f:vh?n:";
	*verbose = 0;

	int done = 0, gotfilename = 0;
	while(!done)
	{
		char c;
		switch(c = getopt(argc, argv, opts))
		{
			case -1:
			done = 1;
			break;

			case 'f':
			filename = optarg;
			gotfilename = 1;
			break;

			case 'v':
			(*verbose)++;
			break;

			case 'n':
			*nexecs = atoi(optarg);
			if(*nexecs < 1){
				cerr << "You're silly! Make n > 0, please.\n";
				exit(1);
			}
			break;

			case 'h':
			case '?':
			default:
			printf("Usage: test_speed-userspace -f proto.pat [-v] [-v] [-n reps]\n");
			exit(0);
			break;
		}
	}

	if(!gotfilename)
	{
		cerr << "Please specify a file.\n";
		cerr << "Try test_speed-userspace -h\n";
		exit(1);
	}
}

// Syntax: test_speed -f patternfile
int main(int argc, char ** argv)
{
	regex_t patterncomp;
	int verbose = 0, cflags, eflags, nexecs = 100000;
	string filename, patternstring;

	handle_cmdline(filename, &nexecs, &verbose, argc, argv);

	if(!parse_pattern_file(cflags, eflags, patternstring, filename))
	{
		cerr << "Failed to get pattern from file\n";
		exit(1);
	}

	if(verbose >= 2) 
		cout << "Pattern before pre_process: " << patternstring << endl;

	pre_process(patternstring); /* do \xHH escapes */

	if(verbose >= 2)
		cout << "Pattern after pre_process:  " << patternstring << endl;

	if(regcomp(&patterncomp, patternstring.c_str(), cflags)){
		fprintf(stderr, "error compiling regexp\n");
		exit(1);
	}

	if(verbose)
		printf("running regexec %d times\n", nexecs);

	doit(&patterncomp, eflags, verbose, nexecs);

	return 0;
}

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: xlate.c
 * PURPOSE: translate the FC, FS, SX, XS fields
 *
 *  xlate ":fc#0000374:fs#0000003:xc#0:xs#0040040:"
 *   - makes best guesses about the various fields
 **************************************************************************/

#include <stdio.h>
#include <string.h>

static char _id[] = "xlate.c,v 3.1 1996/12/28 21:40:53 papowell Exp";

struct bits{
char *name;
int bitfields;
char *comment;
char *try;
int mask;
};

/* f flags - used with the TIOCGET and the struct sgttyb.sg_flags field */
struct bits tiocget[] = {
{ "TANDEM",00000001, "send stopc on out q full", "tandem" },
{ "CBREAK",00000002, "half-cooked mode", "cbreak" },
{ "LCASE",00000004, "simulate lower case", "lcase" },
{ "ECHO",00000010, "echo input", "echo" },
{ "CRMOD",00000020, "map \\r to \\r\\n on output", "crmod" },
{ "RAW",00000040, "no i/o processing", "raw" },
{ "ODDP",00000100, "get/send odd parity", "oddp" },
{ "EVENP",00000200, "get/send even parity", "evenp" },
{ "ANYP",00000300, "get any parity/send none", "parity? anyp? pass8? (caution)" },
{ "NL0",0000000, "new line delay", "nl??",00001400 },
{ "NL1",00000400, "new line delay tty 37", "nl??",00001400  },
{ "NL2",00001000, "new line delay vt05", "nl??",00001400  },
{ "NL3",00001400, "new line delay", "nl??",00001400  },
{ "TAB0",00000000, "tab expansion delay", "tab??",00006000 },
{ "TAB1",00002000, "tab expansion delay tty 37", "tab??",00006000  },
{ "TAB2",00004000, "tab expansion delay", "tab??",00006000  },
{ "XTABS",00006000, "expand tabs on output", "tabs" },
{ "CR0",00000000, "cr??", "",00030000 },
{ "CR1",00010000, "tn 300", "cr??",00030000 },
{ "CR2",00020000, "tty 37", "cr??",00030000 },
{ "CR3",00030000, "concept 100", "cr??",00030000},
{ "FF1",00040000, "form feed delay tty 37", "ff??" },
{ "BS1",0010000, "backspace timing", "bs??" },
{ 0 } };

/* x flags - used with the TIOCLGET and the struct sgttyb.sg_flags field */
struct bits tiolget[] = {
{ "CRTBS",00000001, "do backspacing for crt", "crterase" },
{ "PRTERA",00000002, "\\ ... / erase", "prterase" },
{ "CRTERA",00000004, "\"\\b\" to wipe out char", "crterase" },
{ "TILDE",00000010, "hazeltine tilde kludge", "don't even think about this" },
{ "MDMBUF",00000020, "start/stop output on carrier intr", "crtscts" },
{ "LITOUT",00000040, "literal output", "litout" },
{ "TOSTOP",00000100, "SIGSTOP on background output", "tostop" },
{ "FLUSHO",00000200, "flush output to terminal", "noflsh?? (caution)" },
{ "NOHANG",00000400, "no SIGHUP on carrier drop", "nohand" },
{ "CRTKIL",00002000, "kill line with \"\\b\"", "crtkill" },
{ "PASS8",00004000, "pass 8 bits", "pass8" },
{ "CTLECH",00010000, "echo control chars as ^X", "echok" },
{ "PENDIN",00020000, "tp->t_rawq needs reread", "don't even think about this" },
{ "DECCTQ",00040000, "only ^Q starts after ^S", "decctlq? -ixany? (caution)" },
{ "NOFLSH",00100000, "no output flush on signal", "noflsh" },
{ 0 } };

char *msg[] = {
 "xlate optionstrings",
 "  Example",
 "  xlate \":fc#0000374:fs#0000003:xc#0:xs#0040040:\"",
    0
};

usage()
{
    char **m;
    for( m = msg; *m; ++m ){
        fprintf( stderr, "%s\n", *m );
    }
    exit( 1 );
}


main( argc, argv )
    int argc;
    char *argv[];
{
    char *s, *end;
    char *value;
    int c, v, set;
    struct bits *table;

    if( argc != 2 ) usage();
    for( s = argv[1]; s && *s; s = end ){
        end = strchr( s, ':' );
        if( end ){
            *end++ = 0;
        }
        while( (c = *s) && isspace( c ) ) ++s;
        if( c == 0 ) continue;

        /* now translate option */
        value = strchr( s, '#' );
        if( value == 0 ) usage();
        *value++ = 0;
        v = strtol( value, (char **)0, 0 );
        printf( "%s = %o\n", s, v);
        switch( s[0] ){
            case 'f': table = tiocget; break;
            case 'x': table = tiolget; break;
            default: usage();
        }
        switch( s[1] ){
            case 's': set = 1; break;
            case 'c': set = 0; break;
            default: usage();
        }
        /* now we scan down the values */
        for(; table->name; ++table ){
            if( (table->bitfields & v)
                && ( ((table->bitfields & v) ^ table->bitfields)
            & (table->mask?table->mask:~0) ) == 0 ){
                printf( "  %s %s (%s) try '%s%s'\n",
                    set?"set":"clear",
                    table->name, table->comment,
                    set? "":"-", table->try );
            }
        }
    }
}

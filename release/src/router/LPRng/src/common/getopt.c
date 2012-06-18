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
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: getopt.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


#include "lp.h"
/**** ENDINCLUDE ****/


# if 0
   --------- now in lp.h ---------
 int Optind;                 /* next argv to process */
 int Opterr = 1;                 /* Zero disables errors msgs */
 char *Optarg;               /* Pointer to option argument */
 char *Name;					/* Name of program */
#endif
 static char *next_opt;			    /* pointer to next option char */
 static char **Argv_p;
 static int Argc_p;

int Getopt (int argc, char *argv[], char *optstring)
{
	int  option;               /* current option found */
	char *match;                /* matched option in optstring */

	if( argv == 0 ){
		/* reset parsing */
		next_opt = 0;
		Optind = 0;
		return(0);
	}

	if (Optind == 0 ) {
		char *basename;
		/*
		 * set up the Name variable for error messages
		 * setproctitle will change this, so
		 * make a copy.
		 */
		if( Name == 0 ){
			if( argv[0] ){
				if( (basename = safestrrchr( argv[0], '/' )) ){
					++basename;
				} else {
					basename = argv[0];
				}
				Name = basename;
			} else {
				Name = "???";
			}
		}
		Argv_p = argv;
		Argc_p = argc;
		Optind = 1;
	}

	while( next_opt == 0 || *next_opt == '\0' ){
		/* No more arguments left in current or initial string */
		if (Optind >= argc){
		    return (EOF);
		}
		next_opt = argv[Optind++];
	}

	/* check for start of option string AND no initial '-'  */
	if( (next_opt == argv[Optind-1]) ){
		if( next_opt[0] != '-' ){
			--Optind;
			return( EOF );
		} else {
			++next_opt;
			if( next_opt[0] == 0 ){
				return( EOF );
			}
		}
	}
	option = *next_opt++;
	/*
	 * Case of '--',  Force end of options
	 */
	if (option == '-') {
		if( *next_opt ){
			if( Opterr ){
				(void) FPRINTF (STDERR, "--X option form illegal\n" );
				return('?');
			}
		}
		return ( EOF );
	}
	/*
	 * See if option is in optstring
	 */
	if ((match = (char *) safestrchr (optstring, option)) == 0 ){
		if( Opterr ){
		    (void) FPRINTF (STDERR, "%s: Illegal option '%c'\n", Name, option);
		}
		return( '?' );
	}
	/*
	 * Argument?
	 */
	if (match[1] == ':') {
		/*
		 * Set Optarg to proper value
		 */
		Optarg = 0;
		if (*next_opt != '\0') {
		    Optarg = next_opt;
		} else if (Optind < argc) {
		    Optarg = argv[Optind++];
		    if (Optarg != 0 && *Optarg == '-') {
				Optarg = 0;
			}
		}
		if( Optarg == 0 && Opterr ) {
			(void) FPRINTF (STDERR,
				"%s: missing argument for '%c'\n", Name, option);
			option = '?';
		}
		next_opt = 0;
	} else if (match[1] == '?') {
		/*
		 * Set Optarg to proper value
		 */
		if (*next_opt != '\0') {
		    Optarg = next_opt;
		} else {
		    Optarg = 0;
		}
		next_opt = 0;
	}
	return (option);
}

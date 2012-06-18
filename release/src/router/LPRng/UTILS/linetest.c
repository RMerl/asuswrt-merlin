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
 * MODULE: linetest.c
 * PURPOSE: print a rolling banner pattern
 **************************************************************************/

/*
 * linetest: print the rolling banner pattern
 * linetest [width]
 * Sat Jun 18 09:47:06 CDT 1988 Patrick Powell
 * linetest.c,v
 * Revision 3.1  1996/12/28 21:40:52  papowell
 * Update
 *
 * Revision 3.1  1996/12/28 21:32:47  papowell
 * Update
 *
 * Revision 1.1  1996/12/28 21:07:15  papowell
 * Update
 *
 * Revision 4.1  1996/11/05  06:38:20  papowell
 * Update
 *
 * Revision 3.0  1996/05/19  04:06:49  papowell
 * Update
 *
 * Revision 3.0  1996/05/19  04:06:49  papowell
 * Update
 *
 * Revision 2.7  1996/05/13  03:26:13  papowell
 * Update
 *
 * Revision 2.7  1996/05/13  03:26:13  papowell
 * Update
 *
 * Revision 2.6  1996/03/03  22:31:37  papowell
 * Update
 *
 * Revision 2.6  1996/03/03  22:31:37  papowell
 * Update
 *
 * Revision 2.5  1995/12/03  02:52:29  papowell
 * Update
 *
 * Revision 2.5  1995/12/03  02:52:29  papowell
 * Update
 *
 * Revision 2.4  1995/11/22  14:58:52  papowell
 * Update
 *
 * Revision 2.4  1995/11/22  14:58:52  papowell
 * Update
 *
 * Revision 2.3  1995/11/08  14:28:25  papowell
 * Update
 *
 * Revision 2.3  1995/11/08  14:28:25  papowell
 * Update
 *
 * Revision 2.2  1995/11/06  01:38:35  papowell
 * Update
 *
 * Revision 2.2  1995/11/06  01:38:35  papowell
 * Update
 *
 * Revision 2.1  1995/11/05  01:27:39  papowell
 * Update
 *
 * Revision 2.1  1995/11/05  01:27:39  papowell
 * Update
 *
 * Revision 2.0  1995/11/04  06:29:04  papowell
 * *** empty log message ***
 *
 * Revision 2.0  1995/11/04  06:29:04  papowell
 * *** empty log message ***
 *
 * Revision 2.0  1995/11/04  06:28:06  papowell
 * *** empty log message ***
 *
 * 
 */
#include <stdio.h>
#include <ctype.h>
static char id_str1[] =
	"linetest.c,v 3.1 1996/12/28 21:40:52 papowell Exp PLP Copyright 1988 Patrick Powell";
char *Name;

main(argc, argv)
	int argc;
	char **argv;
{
	int width = 132;
	int i,j;

	Name = argv[0];
	if( argc > 2 || argc < 1 ){
		usage();
	}
	if( argc == 2 ){
		i = atoi( argv[1] );
		if( i != 0 ){
			width = i;
		} else {
			usage();
		}
	}
	
	i = ' ';
	while(1){
		for( j = 0; j < width; ++j ){
			if( !isprint(i) || isspace(i) ){
				i = ' '+1;
			}
			putchar( i );
			i = i + 1;
		}
		putchar( '\n' );
	}
}

usage()
{
	fprintf( stderr, "%s [width]\n", Name );
	exit( 0 );
}

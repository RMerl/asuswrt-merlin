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
"$Id: lpbanner.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";

#include "lp.h"

/***************************************************************************
 *  Filter template and frontend.
 *
 *	A filter is invoked with the following parameters,
 *  which can be in any order, and perhaps some missing.
 *
 *  filtername arguments \   <- from PRINTCAP entry
 *      -PPrinter -wwidth -llength -xwidth -ylength [-c] \
 *      -Kcontrolfilename -Lbnrname \
 *      [-iindent] \
 *		[-Zoptions] [-Cclass] [-Jjob] [-Raccntname] -nlogin -hHost  \
 *      -Fformat -Tdebug [affile]
 * 
 *  1. Parameters can be in different order than the above.
 *  2. Optional parameters can be missing
 *  3. Values specified for the width, length, etc., are from PRINTCAP
 *     or from the overridding user specified options.
 *
 *  This program provides a common front end for most of the necessary
 *  grunt work.  This falls into the following classes:
 * 1. Parameter extraction.
 * 2. Suspension when used as the "of" filter.
 * 3. Termination and accounting
 *  The front end will extract parameters,  then call the filter()
 *  routine,  which is responsible for carrying out the required filter
 *  actions. filter() is invoked with the printer device on fd 1,
 *	and error log on fd 2.  The npages variable is used to record the
 *  number of pages that were used.
 *  The "halt string", which is a sequence of characters that
 *  should cause the filter to suspend itself, is passed to filter.
 *  When these characters are detected,  the "suspend_ofilter()" routine should be
 *  called.
 *
 *  On successful termination,  the accounting file will be updated.
 *
 *  The filter() routine should return 0 (success), 1 (retry) or 2 (abort).
 *
 * Parameter Extraction
 *	The main() routine will extract parameters
 *  whose values are placed in the appropriate variables.  This is done
 *  by using the ParmTable[], which has entries for each valid letter
 *  parmeter, such as the letter flag, the type of variable,
 *  and the address of the variable.
 *  The following variables are provided as a default set.
 *      -PPrinter -wwidth -llength -xwidth -ylength [-c] [-iindent] \
 *		[-Zoptions] [-Cclass] [-Jjob] [-Raccntname] -nlogin -hHost  \
 *      -Fformat [affile]
 * VARIABLE  FLAG          TYPE    PURPOSE / PRINTCAP ENTRTY
 * name     name of filter char*    argv[0], program identification
 * width    -wwidth	       int      PW, width in chars
 * length   -llength	   int      PL, length in lines
 * xwidth   -xwidth        int      PX, width in pixels
 * xlength  -xlength       int      PY, length in pixels
 * literal  -c	           int      if set, ignore control chars
 * controlfile -kcontrolfile char*  control file name
 * bnrname  -Lbnrname      char*    banner name
 * indent   -iindent       int      indent amount (depends on device)
 * zopts    -Zoptions      char*    extra options for printer
 * comment  -Scomment      char*    printer name in comment field
 * class    -Cclass        char*    classname
 * job      -Jjob          char*    jobname
 * accntname -Raccntname   char*    account for billing purposes
 * login    -nlogin        char*    login name
 * host     -hhost         char*    host name
 * format   -Fformat       char*    format
 * special   -snumber      int      Special Variable for passing flags
 * accntfile file          char*    AF, accounting file
 *
 * npages    - number of pages for accounting
 * debug     - sets debug level
 * verbose   - echo to a log file
 *
 *	The functions fatal(), logerr(), and logerr_die() can be used to report
 *	status. The variable errorcode can be set by the user before calling
 *	these functions, and will be the exit value of the program. Its default
 *	value will be 2 (abort status).
 *	fatal() reports a fatal message, and terminates.
 *	logerr() reports a message, appends information indicated by errno
 *	(see perror(2) for details), and then returns.
 *	logerr_die() will call logerr(), and then will exit with errorcode
 *	status.
 *	Both fatal() and logerr_die() call the cleanup() function before exit.
 *
 * DEBUGGING:  a simple minded debugging version can be enabled by
 * compiling with the -DDEBUG option.
 */


int errorcode;
char *name;		/* name of filter */
int debug, verbose, width = 80, length = 66, xwidth, ylength, literal, indent;
char *zopts, *class, *job, *login, *accntname, *host;
char *printer, *accntfile, *format;
char *controlfile;
char *bnrname, *comment;
int npages;	/* number of pages */
int special;
char *queuename, *errorfile;

#define GLYPHSIZE 15
struct glyph{
	int ch, x, y;	/* baseline location relative to x and y position */
	char bits[GLYPHSIZE];
};

struct font{
	int height;	/* height from top to bottom */
	int width;	/* width in pixels */
	int above;	/* max height above baseline */
	struct glyph *glyph;	/* glyphs */
};

void banner( void );
void cleanup( void );
void getargs( int argc, char *argv[], char *envp[] );

int main( int argc, char *argv[], char *envp[] )
{
	/* set umask to safe level */
	umask( 0077 );
	getargs( argc, argv, envp );
	/*
	 * Turn off SIGPIPE
	 */
	(void)signal( SIGPIPE, SIG_IGN );
	(void)signal( SIGCHLD, SIG_DFL );
	banner();
	return(0);
}


void getargs( int argc, char *argv[], char *envp[] )
{
	int i, c;		/* argument index */
	char *arg, *optargv;	/* argument */

	if( (name = argv[0]) == 0 ) name = "FILTER";
	for( i = 1; i < argc && (arg = argv[i])[0] == '-'; ++i ){
		if( (c = arg[1]) == 0 ){
			FPRINTF( STDERR, "missing option flag");
			i = argc;
			break;
		}
		if( c == 'c' ){
			literal = 1;
			continue;
		}
		optargv = &arg[2];
		if( arg[2] == 0 ){
			optargv = argv[i++];
			if( optargv == 0 ){
				FPRINTF( STDERR, "missing option '%c' value", c );
				i = argc;
				break;
			}
		}
		switch(c){
			case 'C': class = optargv; break; 
			case 'T': debug = atoi( optargv ); break;
			case 'F': format = optargv; break; 
			case 'J': job = optargv; break; 
			case 'K': controlfile = optargv; break; 
			case 'L': bnrname = optargv; break; 
			case 'P': printer = optargv; break; 
			case 'Q': queuename = optargv; break; 
			case 'R': accntname = optargv; break; 
			case 'S': comment = optargv; break; 
			case 'Z': zopts = optargv; break; 
			case 'h': host = optargv; break; 
			case 'i': indent = atoi( optargv ); break; 
			case 'l': length = atoi( optargv ); break; 
			case 'n': login = optargv; break; 
			case 's': special = atoi( optargv ); break; 
			case 'v': verbose = atoi( optargv ); break; 
			case 'w': width = atoi( optargv ); break; 
			case 'x': xwidth = atoi( optargv ); break; 
			case 'y': ylength = atoi( optargv ); break;
			case 'E': errorfile = optargv; break;
			default: break;
		}
	}
	if( i < argc ){
		accntfile = argv[i];
	}
	if( errorfile ){
		int fd;
		fd = open( errorfile, O_APPEND | O_WRONLY, 0600 );
		if( fd < 0 ){
			FPRINTF( STDERR, "cannot open error log file '%s'", errorfile );
		} else {
			FPRINTF( STDERR, "using error log file '%s'", errorfile );
			if( fd != 2 ){
				dup2(fd, 2 );
				close(fd);
			}
		}
	}
	if( verbose || debug ){
		FPRINTF(STDERR, "%s command: verbose %d, debug %d ", name, verbose, debug );
		for( i = 0; i < argc; ++i ){
			FPRINTF(STDERR, "%s ", argv[i] );
		}
		FPRINTF( STDERR, "\n" );
	}
	if( debug ){
		FPRINTF(STDERR, "FILTER decoded options: " );
		FPRINTF(STDERR,"login '%s'\n", login? login : "null" );
		FPRINTF(STDERR,"host '%s'\n", host? host : "null" );
		FPRINTF(STDERR,"class '%s'\n", class? class : "null" );
		FPRINTF(STDERR,"format '%s'\n", format? format : "null" );
		FPRINTF(STDERR,"job '%s'\n", job? job : "null" );
		FPRINTF(STDERR,"printer '%s'\n", printer? printer : "null" );
		FPRINTF(STDERR,"queuename '%s'\n", queuename? queuename : "null" );
		FPRINTF(STDERR,"accntname '%s'\n", accntname? accntname : "null" );
		FPRINTF(STDERR,"zopts '%s'\n", zopts? zopts : "null" );
		FPRINTF(STDERR,"literal, %d\n", literal);
		FPRINTF(STDERR,"indent, %d\n", indent);
		FPRINTF(STDERR,"length, %d\n", length);
		FPRINTF(STDERR,"width, %d\n", width);
		FPRINTF(STDERR,"xwidth, %d\n", xwidth);
		FPRINTF(STDERR,"ylength, %d\n", ylength);
		FPRINTF(STDERR,"accntfile '%s'\n", accntfile? accntfile : "null" );
		FPRINTF(STDERR,"errorfile '%s'\n", errorfile? errorfile : "null" );
		FPRINTF(STDERR, "FILTER environment: " );
		for( i = 0; (arg = envp[i]); ++i ){
			FPRINTF(STDERR,"%s\n", arg );
		}
		FPRINTF(STDERR, "RUID: %d, EUID: %d\n", (int)getuid(), (int)geteuid() );
	}
}
/***************************************************************************
Commentary
Patrick Powell Wed Jun  7 19:42:01 PDT 1995

The font information is provided as entries in a data structure.

The struct font{} entry specifies the character heights and widths,
as well as the number of lines needed to display the characters.
The struct glyph{} array is the set of glyphs for each character.

    
	{ 
	X__11___,
	X__11___,
	X__11___,
	X__11___,
	X__11___,
	X_______,
	X_______,
	X__11___,
	cX_11___},			/ * ! * /

     ^ lower left corner, i.e. - on baseline - x = 0, y = 8

	{
	X_______,
	X_______,
	X_______,
	X_111_1_,
	X1___11_,
	X1____1_,
	X1____1_,
	X1___11_,
	cX111_1_,
	X_____1_,
	X1____1_,
	X_1111__},			/ * g * /

     ^ lower left corner, i.e. - on baseline - x = 0, y = 8

 ***************************************************************************/

#define X_______ 0
#define X______1 01
#define X_____1_ 02
#define X____1__ 04
#define X____11_ 06
#define X___1___ 010
#define X___1__1 011
#define X___1_1_ 012
#define X___11__ 014
#define X__1____ 020
#define X__1__1_ 022
#define X__1_1__ 024
#define X__11___ 030
#define X__111__ 034
#define X__111_1 035
#define X__1111_ 036
#define X__11111 037
#define X_1_____ 040
#define X_1____1 041
#define X_1___1_ 042
#define X_1__1__ 044
#define X_1_1___ 050
#define X_1_1__1 051
#define X_1_1_1_ 052
#define X_11____ 060
#define X_11_11_ 066
#define X_111___ 070
#define X_111__1 071
#define X_111_1_ 072
#define X_1111__ 074
#define X_1111_1 075
#define X_11111_ 076
#define X_111111 077
#define X1______ 0100
#define X1_____1 0101
#define X1____1_ 0102
#define X1____11 0103
#define X1___1__ 0104
#define X1___1_1 0105
#define X1___11_ 0106
#define X1__1___ 0110
#define X1__1__1 0111
#define X1__11_1 0115
#define X1__1111 0117
#define X1_1____ 0120
#define X1_1___1 0121
#define X1_1_1_1 0125
#define X1_1_11_ 0126
#define X1_111__ 0134
#define X1_1111_ 0136
#define X11____1 0141
#define X11___1_ 0142
#define X11___11 0143
#define X11_1___ 0150
#define X11_1__1 0151
#define X111_11_ 0166
#define X1111___ 0170
#define X11111__ 0174
#define X111111_ 0176
#define X1111111 0177

struct glyph g9x8[] = {
	{ ' ', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______}},			/* */

	{ '!', 0, 8, {
	X__11___,
	X__11___,
	X__11___,
	X__11___,
	X__11___,
	X_______,
	X_______,
	X__11___,
	X__11___}},			/* ! */

	{ '"', 0, 8, {
	X_1__1__,
	X_1__1__,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______}},			/* " */

	{ '#', 0, 8, {
	X_______,
	X__1_1__,
	X__1_1__,
	X1111111,
	X__1_1__,
	X1111111,
	X__1_1__,
	X__1_1__,
	X_______}},			/* # */

	{ '$', 0, 8, {
	X___1___,
	X_11111_,
	X1__1__1,
	X1__1___,
	X_11111_,
	X___1__1,
	X1__1__1,
	X_11111_,
	X___1___}},			/* $ */

	{ '%', 0, 8, {
	X_1_____,
	X1_1___1,
	X_1___1_,
	X____1__,
	X___1___,
	X__1____,
	X_1___1_,
	X1___1_1,
	X_____1_}},			/* % */

	{ '&', 0, 8, {
	X_11____,
	X1__1___,
	X1___1__,
	X_1_1___,
	X__1____,
	X_1_1__1,
	X1___11_,
	X1___11_,
	X_111__1}},			/* & */

	{ '\'', 0, 8, {
	X___11__,
	X___11__,
	X___1___,
	X__1____,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______}},			/* ' */

	{ '(', 0, 8, {
	X____1__,
	X___1___,
	X__1____,
	X__1____,
	X__1____,
	X__1____,
	X__1____,
	X___1___,
	X____1__}},			/* ( */

	{ ')', 0, 8, {
	X__1____,
	X___1___,
	X____1__,
	X____1__,
	X____1__,
	X____1__,
	X____1__,
	X___1___,
	X__1____}},			/* ) */

	{ '*', 0, 8, {
	X_______,
	X___1___,
	X1__1__1,
	X_1_1_1_,
	X__111__,
	X_1_1_1_,
	X1__1__1,
	X___1___,
	X_______}},			/* * */

	{ '+', 0, 8, {
	X_______,
	X___1___,
	X___1___,
	X___1___,
	X1111111,
	X___1___,
	X___1___,
	X___1___,
	X_______}},			/* + */

	{ ',', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X__11___,
	X__11___,
	X__1____,
	X_1_____,
	X_______}},			/* , */

	{ '-', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_______,
	X1111111,
	X_______,
	X_______,
	X_______,
	X_______}},			/* - */

	{ '.', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X__11___,
	X__11___}},			/* . */

	{ '/', 0, 8, {
	X_______,
	X______1,
	X_____1_,
	X____1__,
	X___1___,
	X__1____,
	X_1_____,
	X1______,
	X_______}},			/* / */

	{ '0', 0, 8, {
	X_11111_,
	X1_____1,
	X1____11,
	X1___1_1,
	X1__1__1,
	X1_1___1,
	X11____1,
	X1_____1,
	X_11111_}},			/* 0 */

	{ '1', 0, 8, {
	X___1___,
	X__11___,
	X_1_1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X_11111_}},			/* 1 */

	{ '2', 0, 8, {
	X_11111_,
	X1_____1,
	X______1,
	X_____1_,
	X__111__,
	X_1_____,
	X1______,
	X1______,
	X1111111}},			/* 2 */

	{ '3', 0, 8, {
	X_11111_,
	X1_____1,
	X______1,
	X______1,
	X__1111_,
	X______1,
	X______1,
	X1_____1,
	X_11111_}},			/* 3 */

	{ '4', 0, 8, {
	X_____1_,
	X____11_,
	X___1_1_,
	X__1__1_,
	X_1___1_,
	X1____1_,
	X1111111,
	X_____1_,
	X_____1_}},			/* 4 */

	{ '5', 0, 8, {
	X1111111,
	X1______,
	X1______,
	X11111__,
	X_____1_,
	X______1,
	X______1,
	X1____1_,
	X_1111__}},			/* 5 */

	{ '6', 0, 8, {
	X__1111_,
	X_1_____,
	X1______,
	X1______,
	X1_1111_,
	X11____1,
	X1_____1,
	X1_____1,
	X_11111_}},			/* 6 */

	{ '7', 0, 8, {
	X1111111,
	X1_____1,
	X_____1_,
	X____1__,
	X___1___,
	X__1____,
	X__1____,
	X__1____,
	X__1____}},			/* 7 */

	{ '8', 0, 8, {
	X_11111_,
	X1_____1,
	X1_____1,
	X1_____1,
	X_11111_,
	X1_____1,
	X1_____1,
	X1_____1,
	X_11111_}},			/* 8 */

	{ '9', 0, 8, {
	X_11111_,
	X1_____1,
	X1_____1,
	X1_____1,
	X_111111,
	X______1,
	X______1,
	X1_____1,
	X_1111__}},			/* 9 */

	{ ':', 0, 8, {
	X_______,
	X_______,
	X_______,
	X__11___,
	X__11___,
	X_______,
	X_______,
	X__11___,
	X__11___}},			/* : */


	{ ';', 0, 8, {
	X_______,
	X_______,
	X_______,
	X__11___,
	X__11___,
	X_______,
	X_______,
	X__11___,
	X__11___,
	X__1____,
	X_1_____}},			/* ; */

	{ '<', 0, 8, {
	X____1__,
	X___1___,
	X__1____,
	X_1_____,
	X1______,
	X_1_____,
	X__1____,
	X___1___,
	X____1__}},			/* < */

	{ '=', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1111111,
	X_______,
	X1111111,
	X_______,
	X_______,
	X_______}},			/* = */

	{ '>', 0, 8, {
	X__1____,
	X___1___,
	X____1__,
	X_____1_,
	X______1,
	X_____1_,
	X____1__,
	X___1___,
	X__1____}},			/* > */

	{ '?', 0, 8, {
	X__1111_,
	X_1____1,
	X_1____1,
	X______1,
	X____11_,
	X___1___,
	X___1___,
	X_______,
	X___1___}},			/* ? */

	{ '@', 0, 8, {
	X__1111_,
	X_1____1,
	X1__11_1,
	X1_1_1_1,
	X1_1_1_1,
	X1_1111_,
	X1______,
	X_1____1,
	X__1111_}},			/* @ */

	{ 'A', 0, 8, {
	X__111__,
	X_1___1_,
	X1_____1,
	X1_____1,
	X1111111,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1}},			/* A */

	{ 'B', 0, 8, {
	X111111_,
	X_1____1,
	X_1____1,
	X_1____1,
	X_11111_,
	X_1____1,
	X_1____1,
	X_1____1,
	X111111_}},			/* B */

	{ 'C', 0, 8, {
	X__1111_,
	X_1____1,
	X1______,
	X1______,
	X1______,
	X1______,
	X1______,
	X_1____1,
	X__1111_}},			/* C */

	{ 'D', 0, 8, {
	X11111__,
	X_1___1_,
	X_1____1,
	X_1____1,
	X_1____1,
	X_1____1,
	X_1____1,
	X_1___1_,
	X11111__}},			/* D */

	{ 'E', 0, 8, {
	X1111111,
	X1______,
	X1______,
	X1______,
	X111111_,
	X1______,
	X1______,
	X1______,
	X1111111}},			/* E */

	{ 'F', 0, 8, {
	X1111111,
	X1______,
	X1______,
	X1______,
	X111111_,
	X1______,
	X1______,
	X1______,
	X1______}},			/* F */

	{ 'G', 0, 8, {
	X__1111_,
	X_1____1,
	X1______,
	X1______,
	X1______,
	X1__1111,
	X1_____1,
	X_1____1,
	X__1111_}},			/* G */

	{ 'H', 0, 8, {
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1111111,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1}},			/* H */

	{ 'I', 0, 8, {
	X_11111_,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X_11111_}},			/* I */

	{ 'J', 0, 8, {
	X__11111,
	X____1__,
	X____1__,
	X____1__,
	X____1__,
	X____1__,
	X____1__,
	X1___1__,
	X_111___}},			/* J */

	{ 'K', 0, 8, {
	X1_____1,
	X1____1_,
	X1___1__,
	X1__1___,
	X1_1____,
	X11_1___,
	X1___1__,
	X1____1_,
	X1_____1}},			/* K */

	{ 'L', 0, 8, {
	X1______,
	X1______,
	X1______,
	X1______,
	X1______,
	X1______,
	X1______,
	X1______,
	X1111111}},			/* L */

	{ 'M', 0, 8, {
	X1_____1,
	X11___11,
	X1_1_1_1,
	X1__1__1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1}},			/* M */

	{ 'N', 0, 8, {
	X1_____1,
	X11____1,
	X1_1___1,
	X1__1__1,
	X1___1_1,
	X1____11,
	X1_____1,
	X1_____1,
	X1_____1}},			/* N */

	{ 'O', 0, 8, {
	X__111__,
	X_1___1_,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X_1___1_,
	X__111__}},			/* O */

	{ 'P', 0, 8, {
	X111111_,
	X1_____1,
	X1_____1,
	X1_____1,
	X111111_,
	X1______,
	X1______,
	X1______,
	X1______}},			/* P */

	{ 'Q', 0, 8, {
	X__111__,
	X_1___1_,
	X1_____1,
	X1_____1,
	X1_____1,
	X1__1__1,
	X1___1_1,
	X_1___1_,
	X__111_1}},			/* Q */

	{ 'R', 0, 8, {
	X111111_,
	X1_____1,
	X1_____1,
	X1_____1,
	X111111_,
	X1__1___,
	X1___1__,
	X1____1_,
	X1_____1}},			/* R */

	{ 'S', 0, 8, {
	X_11111_,
	X1_____1,
	X1______,
	X1______,
	X_11111_,
	X______1,
	X______1,
	X1_____1,
	X_11111_}},			/* S */

	{ 'T', 0, 8, {
	X1111111,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___}},			/* T */

	{ 'U', 0, 8, {
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X_11111_}},			/* U */

	{ 'V', 0, 8, {
	X1_____1,
	X1_____1,
	X1_____1,
	X_1___1_,
	X_1___1_,
	X__1_1__,
	X__1_1__,
	X___1___,
	X___1___}},			/* V */

	{ 'W', 0, 8, {
	X1_____1,
	X1_____1,
	X1_____1,
	X1_____1,
	X1__1__1,
	X1__1__1,
	X1_1_1_1,
	X11___11,
	X1_____1}},			/* W */

	{ 'X', 0, 8, {
	X1_____1,
	X1_____1,
	X_1___1_,
	X__1_1__,
	X___1___,
	X__1_1__,
	X_1___1_,
	X1_____1,
	X1_____1}},			/* X */

	{ 'Y', 0, 8, {
	X1_____1,
	X1_____1,
	X_1___1_,
	X__1_1__,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___}},			/* Y */

	{ 'Z', 0, 8, {
	X1111111,
	X______1,
	X_____1_,
	X____1__,
	X___1___,
	X__1____,
	X_1_____,
	X1______,
	X1111111}},			/* Z */

	{ '[', 0, 8, {
	X_1111__,
	X_1_____,
	X_1_____,
	X_1_____,
	X_1_____,
	X_1_____,
	X_1_____,
	X_1_____,
	X_1111__}},			/* [ */

	{ '\\', 0, 8, {
	X_______,
	X1______,
	X_1_____,
	X__1____,
	X___1___,
	X____1__,
	X_____1_,
	X______1,
	X_______}},			/* \ */

	{ ']', 0, 8, {
	X__1111_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_____1_,
	X__1111_}},			/* ] */

	{ '^', 0, 8, {
	X___1___,
	X__1_1__,
	X_1___1_,
	X1_____1,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______}},			/* ^ */

	{ '_', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X1111111,
	X_______}},			/* _ */

	{ '`', 0, 8, {
	X__11___,
	X__11___,
	X___1___,
	X____1__,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______}},			/* ` */

	{ 'a', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_1111__,
	X_____1_,
	X_11111_,
	X1_____1,
	X1____11,
	X_1111_1}},			/* a */

	{ 'b', 0, 8, {
	X1______,
	X1______,
	X1______,
	X1_111__,
	X11___1_,
	X1_____1,
	X1_____1,
	X11___1_,
	X1_111__}},			/* b */

	{ 'c', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_1111__,
	X1____1_,
	X1______,
	X1______,
	X1____1_,
	X_1111__}},			/* c */

	{ 'd', 0, 8, {
	X_____1_,
	X_____1_,
	X_____1_,
	X_111_1_,
	X1___11_,
	X1____1_,
	X1____1_,
	X1___11_,
	X_111_1_}},			/* d */

	{ 'e', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_1111__,
	X1____1_,
	X111111_,
	X1______,
	X1____1_,
	X_1111__}},			/* e */

	{ 'f', 0, 8, {
	X___11__,
	X__1__1_,
	X__1____,
	X__1____,
	X11111__,
	X__1____,
	X__1____,
	X__1____,
	X__1____}},			/* f */

	{ 'g', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_111_1_,
	X1___11_,
	X1____1_,
	X1____1_,
	X1___11_,
	X_111_1_,
	X_____1_,
	X1____1_,
	X_1111__}},			/* g */

	{ 'h', 0, 8, {
	X1______,
	X1______,
	X1______,
	X1_111__,
	X11___1_,
	X1____1_,
	X1____1_,
	X1____1_,
	X1____1_}},			/* h */

	{ 'i', 0, 8, {
	X_______,
	X___1___,
	X_______,
	X__11___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X__111__}},			/* i */

	{ 'j', 0, 8, {
	X_______,
	X_______,
	X_______,
	X____11_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_____1_,
	X_1___1_,
	X__111__}},			/* j */

	{ 'k', 0, 8, {
	X1______,
	X1______,
	X1______,
	X1___1__,
	X1__1___,
	X1_1____,
	X11_1___,
	X1___1__,
	X1____1_}},			/* k */

	{ 'l', 0, 8, {
	X__11___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X__111__}},			/* l */

	{ 'm', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1_1_11_,
	X11_1__1,
	X1__1__1,
	X1__1__1,
	X1__1__1,
	X1__1__1}},			/* m */

	{ 'n', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1_111__,
	X11___1_,
	X1____1_,
	X1____1_,
	X1____1_,
	X1____1_}},			/* n */

	{ 'o', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_1111__,
	X1____1_,
	X1____1_,
	X1____1_,
	X1____1_,
	X_1111__}},			/* o */


	{ 'p', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1_111__,
	X11___1_,
	X1____1_,
	X1____1_,
	X11___1_,
	X1_111__,
	X1______,
	X1______,
	X1______}},			/* p */

	{ 'q', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_111_1_,
	X1___11_,
	X1____1_,
	X1____1_,
	X1___11_,
	X_111_1_,
	X_____1_,
	X_____1_,
	X_____1_}},			/* q */

	{ 'r', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1_111__,
	X11___1_,
	X1______,
	X1______,
	X1______,
	X1______}},			/* r */

	{ 's', 0, 8, {
	X_______,
	X_______,
	X_______,
	X_1111__,
	X1____1_,
	X_11____,
	X___11__,
	X1____1_,
	X_1111__}},			/* s */

	{ 't', 0, 8, {
	X_______,
	X__1____,
	X__1____,
	X11111__,
	X__1____,
	X__1____,
	X__1____,
	X__1__1_,
	X___11__}},			/* t */

	{ 'u', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1____1_,
	X1____1_,
	X1____1_,
	X1____1_,
	X1___11_,
	X_111_1_}},			/* u */

	{ 'v', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1_____1,
	X1_____1,
	X1_____1,
	X_1___1_,
	X__1_1__,
	X___1___}},			/* v */

	{ 'w', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1_____1,
	X1__1__1,
	X1__1__1,
	X1__1__1,
	X1__1__1,
	X_11_11_}},			/* w */

	{ 'x', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1____1_,
	X_1__1__,
	X__11___,
	X__11___,
	X_1__1__,
	X1____1_}},			/* x */

	{ 'y', 0, 8, {
	X_______,
	X_______,
	X_______,
	X1____1_,
	X1____1_,
	X1____1_,
	X1____1_,
	X1___11_,
	X_111_1_,
	X_____1_,
	X1____1_,
	X_1111__}},			/* y */

	{ 'z', 0, 8, {
	X_______,
	X_______,
	X_______,
	X111111_,
	X____1__,
	X___1___,
	X__1____,
	X_1_____,
	X111111_}},			/* z */

	{ '}', 0, 8, {
	X___11__,
	X__1____,
	X__1____,
	X__1____,
	X_1_____,
	X__1____,
	X__1____,
	X__1____,
	X___11__}},			/* } */

	{ '|', 0, 8, {
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___,
	X___1___}},			/* | */

	{ '}', 0, 8, {
	X__11___,
	X____1__,
	X____1__,
	X____1__,
	X_____1_,
	X____1__,
	X____1__,
	X____1__,
	X__11___}},			/* } */

	{ '~', 0, 8, {
	X_11____,
	X1__1__1,
	X____11_,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______,
	X_______}},			/* ~ */

	{ 'X', 0, 8, {
	X_1__1__,
	X1__1__1,
	X__1__1_,
	X_1__1__,
	X1__1__1,
	X__1__1_,
	X_1__1__,
	X1__1__1,
	X__1__1_}}			/* rub-out */
};

/*
  9 by 8 font:
  12 rows high, 8 cols wide, 9 lines above baseline
 */
struct font Font9x8 = {
	12, 8, 9, g9x8 
};

void Out_line( void );
void breakline( int c );
void bigprint( struct font *font, char *line );
void do_char( struct font *font, struct glyph *glyph,
	char *str, int line, int wid );
/*
 * Print a banner
 * 
 * banner(): print the banner
 * 1. Allocate a buffer for output string
 * 2. Calculate the various proportions
 *     - topblast 
 *     - topbreak 
 *     - big letters  
 *     - info -          6 lines
 *     - <filler>
 *     - bottombreak
 *     - bottomblast
 */

char bline[1024];
int bigjobnumber, biglogname, bigfromhost, bigjobname;
int top_break,	/* break lines at top of page */
	top_sep,	/* separator from info at top of page */
	bottom_sep,	/* separator from info at bottom of page */
	bottom_break;	/* break lines at bottom of page */
int breaksize = 3;	/* numbers of rows in break */

/*
 * userinfo: just p rintf the information
 */
 static void userinfo( void )
{
	(void) SNPRINTF( bline, sizeof(bline)) "User:  %s@%s (%s)", login, host, bnrname);
	Out_line();
	(void) SNPRINTF( bline, sizeof(bline)) "Date:  %s", Time_str(0,0));
	Out_line();
	(void) SNPRINTF( bline, sizeof(bline)) "Job:   %s", job );
	Out_line();
	(void) SNPRINTF( bline, sizeof(bline)) "Class: %s", class );
	Out_line();
}

/*
 * seebig: calcuate if the big letters can be seen
 * i.e.- printed on the page
 */

void seebig( int *len, int bigletter_height, int *big )
{
	*big = 0;
	if( *len > bigletter_height ){
		*big = bigletter_height+1;
		*len -= *big;
	}
}

/*
 * banner: does all the actual work
 */

char *isnull( char *s )
{
	if( s == 0 ) s = "";
	return( s );
}

void banner(void)
{
	int len;					/* length of page */
	int i;                      /* ACME integers, INC */
	char jobnumber[1024];

	jobnumber[0] = 0;
#if 0
	/* read from the STDIN */
	(void)fgets( jobnumber, sizeof(jobnumber), stdin );
	if(debug)FPRINTF(STDOUT, "BANNER CMD '%s'\n", jobnumber );
#endif
	bigjobnumber = biglogname = bigfromhost = bigjobname = 0;

	/* now calculate the numbers of lines available */
	if(debug)FPRINTF(STDERR, "BANNER: length %d\n", length );
	len = length;
	len -= 4;	/* user information */
	/* now we add a top break and bottom break */
	if( len > 2*breaksize ){
		top_break = breaksize;
		bottom_break = breaksize;
	}  else {
		top_break = 1;
		bottom_break = 1;
	}
	len -= (top_break + bottom_break);

	if( bnrname == 0 ){
		bnrname = login;
	}

	/* see if we can do big letters */
	jobnumber[0] = 0;
	if( controlfile ){
		strncpy( jobnumber, controlfile+3, 3 );
		jobnumber[3] = 0;
	}
	if(jobnumber && *jobnumber ) seebig( &len, Font9x8.height, &bigjobnumber );
	if(bnrname && *bnrname) seebig( &len, Font9x8.height, &biglogname );
	if(host && *host ) seebig( &len, Font9x8.height, &bigfromhost );
	if(job && *job) seebig( &len, Font9x8.height, &bigjobname );

	/* now we see how much space we have left */
	while( length > 0 && len < 0 ){
		len += length;
	}

	/*
	 * we add padding
	 * Note that we can optionally produce a banner page
     * exactly PL-1 lines long
	 * This allows a form feed to be added onto the end.
	 */
	if( len > 0 ){
		/* adjust the total page length */
		/* len = len -1; */
		/* check to see if we make breaks a little larger */
		if( len > 16 ){
			top_break += 3;
			bottom_break += 3;
			len -= 6;
		}
		top_sep = len/2;
		bottom_sep = len - top_sep;
	}
	if(debug)FPRINTF(STDERR, "BANNER: length %d, top_break %d, top_sep %d\n",
		length, top_break, top_sep  );
	if(debug)FPRINTF(STDERR, "BANNER: bigjobnumber %d, jobnumber '%s'\n", bigjobnumber,
		isnull(jobnumber) );
	if(debug)FPRINTF(STDERR, "BANNER: biglogname %d, bnrname '%s'\n", biglogname,
		isnull(bnrname) );
	if(debug)FPRINTF(STDERR, "BANNER: bigfromhost %d, host '%s'\n", bigfromhost,
		isnull(host) );
	if(debug)FPRINTF(STDERR, "BANNER: bigjobname %d, jobname '%s'\n", bigjobname,
		isnull(job) );
	if(debug)FPRINTF(STDERR, "BANNER: userinfo %d\n", 4 );
	if(debug)FPRINTF(STDERR, "BANNER: bottom_sep %d, bottom_break %d\n",
		bottom_sep, bottom_break  );

	for( i = 0; i < top_break; ++i ){
		breakline( '*');
	}
	for( i = 0; i < top_sep; ++i ){
		breakline( 0 );
	}

	/*
	 * print the Name, Host and Jobname in BIG letters
	 * allow some of them to be dropped if there isn't enough
	 * room.
	 */

	if( bigjobnumber ) bigprint( &Font9x8, jobnumber );
	if( biglogname ) bigprint( &Font9x8, bnrname );
	if( bigfromhost ) bigprint( &Font9x8, host);
	if( bigjobname ) bigprint( &Font9x8, job );
	userinfo();

	for( i = 0; i < bottom_sep; ++i ){
		breakline( 0 );
	}
	for( i = 0; i < bottom_break; ++i ){
		breakline( '*');
	}
}

void breakline( int c )
{
	int i;

	if( c ){
		for( i = 0; i < width; i++) bline[i] = c;
		bline[i] = '\0';
	} else {
		bline[0] = '\0';
	}
	Out_line();
}

/***************************************************************************
 * bigprint( struct font *font, char * line)
 * print the line in big characters
 * for i = topline to bottomline do
 *  for each character in the line do
 *    get the scan line representation for the character
 *    foreach bit in the string do
 *       if the bit is set, print X, else print ' ';
 *        endfor
 *  endfor
 * endfor
 *
 ***************************************************************************/

void bigprint( struct font *font, char *line )
{
	int i, j, k, len;                   /* ACME Integers, Inc. */

	bline[width] = 0;
	len = strlen(line);
	if(debug)FPRINTF(STDERR,"bigprint: '%s'\n", line );
	for( i = 0; i < font->height; ++i ){
		for( j = 0; j < width; ++j ){
			bline[j] = ' ';
		}
		for( j = 0, k = 0; j < width && k < len; j += font->width, ++k ){
			do_char( font, &font->glyph[line[k]-32], &bline[j], i, width - j );
		}
		Out_line();
	}
	bline[0] = 0;
	Out_line();
}

/***************************************************************************
 * write the buffer out to the file descriptor.
 * don't do if fail is invalid.
 ***************************************************************************/

void Out_line( void )
{
	int i, l;
	char *str;
	bline[sizeof(bline)-1] = 0;
	if( width < (int)sizeof(bline) ) bline[width] = 0;
	for( str = bline, i = strlen(str);
		i > 0 && (l = write( 1, str, i)) > 0;
		i -= l, str += l );
	for( str = "\n", i = strlen(str);
		i > 0 && (l = write( 1, str, i)) > 0;
		i -= l, str += l );
}

/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * Thu Aug 4 12:34:17 BST 1994 -> Aug  4 12:34:17
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Time_str(int shortform, time_t tm)
{
    time_t tvec;
    static char s[99];
	char *t;

	if( tm ){
		tvec = tm;
	} else {
		(void) time (&tvec);
	}
    (void)strcpy( s, ctime(&tvec) );
	t = s;
	s[29] = 0;
	if( shortform > 0 ){
		t = &s[11];
		s[19] = 0;
	} else if( shortform == 0 ){
		t = &s[4];
		s[19] = 0;
	}
	return(t);
}

void do_char( struct font *font, struct glyph *glyph,
	char *str, int line, int wid )
{
	int chars, i, j, k;
	char *s;

	/* if(debug)FPRINTF(STDERR,"do_char: '%c', wid %d\n", glyph->ch, wid ); */
	chars = (font->width+7)/8;	/* calculate the row */
	s = &glyph->bits[line*chars];	/* get start of row */
	for( k = 0, i = 0; k < wid && i < chars; ++i ){	/* for each byte in row */
		for( j = 7; k < wid && j >= 0; ++k, --j ){	/* from most to least sig bit */
			if( *s & (1<<j) ){		/* get bit value */
				str[k] = 'X';
			}
		}
		++s;
	}
}

int Write_fd_str( int fd, const char *buf )
{
	int n;
	n = strlen(buf);
	return write(fd,buf,n);
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
 void safefprintf (int fd, char *format,...)
#else
 void safefprintf (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	int fd;
    char *format;
#endif
	char buf[1024];
    VA_LOCAL_DECL

    VA_START (format);
    VA_SHIFT (fd, int);
    VA_SHIFT (format, char *);

	buf[0] = 0;
	(void) VSNPRINTF (buf, sizeof(buf)) format, ap);
	Write_fd_str(fd,buf);
}


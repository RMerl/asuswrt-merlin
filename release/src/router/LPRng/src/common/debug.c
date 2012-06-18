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
"$Id: debug.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


/*************************************************************
 * void Get_debug_parm(int argc, char *argv[], struct keywords *list)
 * Scan the command line for -D debugparms
 *  debugparms has the format value,key=value,key,key@...
 *  1. if the value is an integer,  then we treat it as a value for DEBUG
 *  2. if a key is present,  then we scan the list and find the
 *     match for the key.  We then convert according to the type of
 *     option expected.
 *************************************************************/

#include "lp.h"
#include "errorcodes.h"
#include "getopt.h"
#include "child.h"
/**** ENDINCLUDE ****/

 struct keywords debug_vars[]		/* debugging variables */
 = {
#if !defined(NODEBUG)
    { "network",0,FLAG_K,(void *)&DbgFlag,DNW1, DNWMASK,0},
    { "network+1",0,FLAG_K,(void *)&DbgFlag,DNW1, DNWMASK,0},
    { "network+2",0,FLAG_K,(void *)&DbgFlag,DNW2|DNW1, DNWMASK,0},
    { "network+3",0,FLAG_K,(void *)&DbgFlag,DNW3|DNW2|DNW1, DNWMASK,0},
    { "network+4",0,FLAG_K,(void *)&DbgFlag,DNW4|DNW3|DNW2|DNW1, DNWMASK,0},
    { "database",0,FLAG_K,(void *)&DbgFlag,DDB1, DDBMASK,0},
    { "database+1",0,FLAG_K,(void *)&DbgFlag,DDB1, DDBMASK,0},
    { "database+2",0,FLAG_K,(void *)&DbgFlag,DDB2|DDB1, DDBMASK,0},
    { "database+3",0,FLAG_K,(void *)&DbgFlag,DDB3|DDB2|DDB1, DDBMASK,0},
    { "database+4",0,FLAG_K,(void *)&DbgFlag,DDB4|DDB3|DDB2|DDB1, DDBMASK,0},
    { "database+4",0,FLAG_K,(void *)&DbgFlag,DDB4, DDBMASK,0},
    { "lpr",0,FLAG_K,(void *)&DbgFlag,DRECV1, DRECVMASK,0},
    { "lpr+1",0,FLAG_K,(void *)&DbgFlag,DRECV1, DRECVMASK,0},
    { "lpr+2",0,FLAG_K,(void *)&DbgFlag,DRECV2|DRECV1, DRECVMASK,0},
    { "lpr+3",0,FLAG_K,(void *)&DbgFlag,DRECV3|DRECV2|DRECV1, DRECVMASK,0},
    { "lpr+4",0,FLAG_K,(void *)&DbgFlag,DRECV4|DRECV3|DRECV2|DRECV1, DRECVMASK,0},
    { "lpc",0,FLAG_K,(void *)&DbgFlag,DCTRL1, DCTRLMASK,0},
    { "lpc+1",0,FLAG_K,(void *)&DbgFlag,DCTRL1, DCTRLMASK,0},
    { "lpc+2",0,FLAG_K,(void *)&DbgFlag,DCTRL2|DCTRL1, DCTRLMASK,0},
    { "lpc+3",0,FLAG_K,(void *)&DbgFlag,DCTRL3|DCTRL2|DCTRL1, DCTRLMASK,0},
    { "lpc+4",0,FLAG_K,(void *)&DbgFlag,DCTRL4|DCTRL3|DCTRL2|DCTRL1, DCTRLMASK,0},
    { "lprm",0,FLAG_K,(void *)&DbgFlag,DLPRM1, DLPRMMASK,0},
    { "lprm+1",0,FLAG_K,(void *)&DbgFlag,DLPRM1, DLPRMMASK,0},
    { "lprm+2",0,FLAG_K,(void *)&DbgFlag,DLPRM2|DLPRM1, DLPRMMASK,0},
    { "lprm+3",0,FLAG_K,(void *)&DbgFlag,DLPRM3|DLPRM2|DLPRM1, DLPRMMASK,0},
    { "lprm+4",0,FLAG_K,(void *)&DbgFlag,DLPRM4|DLPRM3|DLPRM2|DLPRM1, DLPRMMASK,0},
    { "lpq",0,FLAG_K,(void *)&DbgFlag,DLPQ1, DLPQMASK,0},
    { "lpq+1",0,FLAG_K,(void *)&DbgFlag,DLPQ1, DLPQMASK,0},
    { "lpq+2",0,FLAG_K,(void *)&DbgFlag,DLPQ2|DLPQ1, DLPQMASK,0},
    { "lpq+3",0,FLAG_K,(void *)&DbgFlag,DLPQ3|DLPQ2|DLPQ1, DLPQMASK,0},
    { "lpq+4",0,FLAG_K,(void *)&DbgFlag,DLPQ4|DLPQ3|DLPQ2|DLPQ1, DLPQMASK,0},
    { "log",0,FLAG_K,(void *)&DbgFlag,DLOG1, DLOGMASK,0},
    { "log+1",0,FLAG_K,(void *)&DbgFlag,DLOG1, DLOGMASK,0},
    { "log+2",0,FLAG_K,(void *)&DbgFlag,DLOG2|DLOG1, DLOGMASK,0},
    { "log+3",0,FLAG_K,(void *)&DbgFlag,DLOG3|DLOG2|DLOG1, DLOGMASK,0},
    { "log+4",0,FLAG_K,(void *)&DbgFlag,DLOG4|DLOG3|DLOG2|DLOG1, DLOGMASK,0},
    { "test",0,INTEGER_K,(void *)&DbgTest,0,0,0},
#endif
    { 0,0,0,0,0,0,0 }
};

/*

 Parse_debug (char *dbgstr, struct keywords *list, int interactive );
 Input string:  value,key=value,flag,flag@,...

 1. crack the input line at the ','
 2. crack each option at = 
 3. search for key words
 4. assign value to variable

*/

void Parse_debug (char *dbgstr, int interactive )
#ifdef ORIGINAL_DEBUG//JY@1020
{
#else
{}
#endif
#ifdef ORIGINAL_DEBUG//JY@1020
#if !defined(NODEBUG)
	char *key, *convert, *end;
	int i, n, found, count;
	struct keywords *list = debug_vars;
	struct line_list l;

	Init_line_list(&l);
	Split(&l,dbgstr,File_sep,0,0,0,0,0,0);

	for( count = 0; count < l.count; ++count ){
		found = 0;
		end = key = l.list[count];
		n = strtol(key,&end,0);
		if( *end == 0 ){
			Debug = n;
			if( n == 0 )DbgFlag = 0;
			found = 1;
		} else {
			if( (end = safestrchr(key,'=')) ){
				*end++ = 0;
				n = strtol(end,0,0);
			}
			/* search the keyword list */
			for (i = 0;
				(convert = list[i].keyword) && safestrcasecmp( convert, key );
				++i );
			if( convert != 0 ){
				switch( list[i].type ){
				case INTEGER_K:
					*(int *)list[i].variable = n;
					found = 1;
					break;
				case FLAG_K:
					*(int *)list[i].variable |= list[i].maxval;
					/*
						DEBUG1("Parse_debug: key '%s', val 0x%x, DbgFlag 0x%x",
						key, list[i].maxval, DbgFlag );
					 */
					found = 1;
					break;
				default:
					break;
				}
			}
		}
		if(!found && interactive ){
			int lastflag = 0;
			int nooutput = 0;
		    FPRINTF (STDERR,
	"debug flags: [ num | flag=num | flag=str | flag | flag@ | flag+N ]*\n");
		    FPRINTF (STDERR, "  flags recognized:");
		    for (i = 0; list[i].keyword; i++) {
				if( safestrchr( list[i].keyword, '+' ) ) continue;
				if( nooutput == 0 ){
					if( i ){
						FPRINTF( STDERR, ", " );
						if( !(i % 4) ) FPRINTF( STDERR, "\n   " );
					} else {
						FPRINTF( STDERR, " " );
					}
				} else {
					nooutput = 0;
				}
				switch( list[i].type ){
				case INTEGER_K:
					FPRINTF (STDERR, "%s=num", list[i].keyword);
					break;
				case STRING_K:
					FPRINTF (STDERR, "%s=str", list[i].keyword);
					break;
				case FLAG_K:
					if( list[i].maxval == 0 || lastflag != list[i].flag ){
						FPRINTF (STDERR, "%s[+N,@]", list[i].keyword );
						lastflag = list[i].maxval;
					} else {
						nooutput = 1;
					}
					break;
				default:
					break;
				}
			}
		    FPRINTF (STDERR, "\n");
			Errorcode = JABORT;
			if( interactive > 0 ) cleanup(0);
		}
	}
	Free_line_list(&l);
#endif
	/* LOGDEBUG("Parse_debug: Debug %d, DbgFlag 0x%x", Debug, DbgFlag ); */
}
#endif
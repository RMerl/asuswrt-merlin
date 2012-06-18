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
"$Id: user_objs.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";

#include "lp.h"
#include "getqueue.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 *
 * Patrick Powell Mon May 29 11:49:21 PDT 2000
 * This is a sample of what you need to do to implement
 * a chooser function at the routine level.  You may need to do this
 * if your overhead for checking, etc., is too high for a filter.
 * 
 * extern int CHOOSER_ROUTINE( struct line_list *servers,
 * 	struct line_list *available, int &use_subserver );
 *  
 * servers is a (dynamically) allocated array of pointers to line_list data
 *         structures that contains the information about the subserver queue.
 * 
 *         The first entry in the queue is the information about load
 *         balance queue itself, the remaining ones are the information about
 *         the sub server queues.  You can access this by using:
 * 
 *         struct line_list *sp;
 * 		for( i = 1; i < servers->count; ++i ){
 * 			sp = (void *)servers->list[i];
 * 			pr = Find_str_value(sp,PRINTER,Value_sep);
 * 			if( (s = Find_str_value(sp,SERVER,Value_sep)) ){
 * 				DEBUG1("subserver '%s' active server '%s'", pr,s );
 * 			}
 * 		}
 * 
 * available: is list of lines of the form:
 *        queue1=0x1
 *        queue2=0x2
 *  
 *        These are the queue names that are available with the
 *    indexes into the servers.list for their information.  You can
 *    use this as follows:
 * 
 *         for( i = 0; i < available->count; ++i ){
 * 			s = available.list[i];
 * 			if( (t = safestrpbrk(s,Value_sep)) ){
 * 				*t++ = 0;
 * 				n = strtol(t,0,0);
 * 				sp = (void *)servers->list[n];
 * 				/ * now you have the information for the server * /
 * 			}
 * 		}
 * 
 * use_subserver:  return value for queue (subserver) to use in the
 *    servers list.  If you return or set it to -1, skip this job.
 * 
 * RETURN VALUE:
 * 
 *    if 0, then process job.  (note: return value of 0 and use_subserver[0] == -1
 * 		will skip the job.
 *    Nonzero - set job status to the value and update the job.  Useful to
 *       set JHOLD, etc., if you need to do something like this.
 * 
 ***************************************************************************/

int test_chooser( struct line_list *servers,
	struct line_list *available, int *use_subserver )
{
	struct line_list *sp;
	char *s, *t, *pr;
	int i, n = -1;

	DEBUG1("test_chooser:  servers %d, available %d",
		servers->count, available->count );
	for( i = 0; i < available->count; ++i ){
 		s = available->list[i];
		DEBUG1("test_chooser: avail[%d]='%s'", i, s );
		if( (t = safestrpbrk(s,Value_sep)) ){
			n = strtol(t+1,0,0);
			DEBUG1("test_chooser: '%s' index '%d'", s, n );
			sp = (void *)servers->list[n];
			pr = Find_str_value(sp,PRINTER,Value_sep);
			DEBUG1("test_chooser: available '%s'", pr );
			if( i == 0 ){
				*use_subserver = n;
			}
		}
	}
	return( 0 );
}


/*
 * Make_sort_key
 *   Make a sort key from the image information
 *  See the comments in LPRng/src/common/getqueue for details on
 *  the various functions and what they do.
 *
 *  This routine returns a 'key;value' string (dynmically allocated)
 *  that is used to sort the jobs in a spool directory.
 *  The format of the key returned string is:
 *   xxx|xxx|xxx|xxx;cfname
 *  Note 1: xxx cannot have ';' in them.
 *  Note 2: cfname must be at the end of the string.
 *  
 */

char *test_sort_key( struct job *job )
{
	char *cmpstr = 0;
	/* first key is DONE_TIME - done jobs come last */
	cmpstr = intval(DONE_TIME,&job->info,cmpstr);
	/* next key is REMOVE_TIME - removed jobs come before last */
	cmpstr = intval(REMOVE_TIME,&job->info,cmpstr);
	/* next key is ERROR - error jobs jobs come before removed */
	cmpstr = strzval(ERROR,&job->info,cmpstr);
	/* next key is HOLD - before the error jobs  */
	cmpstr = intval(HOLD_CLASS,&job->info,cmpstr);
	cmpstr = intval(HOLD_TIME,&job->info,cmpstr);
	/* next key is MOVE - before the held jobs  */
	cmpstr = strnzval(MOVE,&job->info,cmpstr);
	/* now by priority */
	if( Ignore_requested_user_priority_DYN == 0 ){
		cmpstr = strval(PRIORITY,&job->info,cmpstr,Reverse_priority_order_DYN);
	}
	/* now we do TOPQ */
	cmpstr = revintval(PRIORITY_TIME,&job->info,cmpstr);
	/* now we do FirstIn, FirstOut */
	cmpstr = intval(JOB_TIME,&job->info,cmpstr);
	cmpstr = intval(JOB_TIME_USEC,&job->info,cmpstr);
	/* now we do by job number if two at same time (very unlikely) */
	cmpstr = intval(NUMBER,&job->info,cmpstr);

	DEBUG4("test_sort_key: cmpstr '%s'", cmpstr );

	return(cmpstr);
}


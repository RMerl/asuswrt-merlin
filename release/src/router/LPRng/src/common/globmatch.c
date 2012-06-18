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
"$Id: globmatch.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";

#include "lp.h"

/**** ENDINCLUDE ****/

int glob_pattern( char *pattern, const char *str )
{
	int result = 1;
	int len, c, invert;
	char *glob;

	/* DEBUG4("glob_pattern: pattern '%s' to '%s'", pattern, str ); */
	if( (glob = safestrpbrk( pattern, "*?[" )) ){
		/* check the characters up to the glob length for a match */
		len = glob - pattern;
		c = *glob;
	 	/* DEBUG4("glob_pattern: meta '%c', len %d", c, len ); */
		if( (len == 0 || !safestrncasecmp( pattern, str, len )) ){
			/* matched: pattern xxxx*  to xxxx
			 * now we have to do the remainder. We simply check for rest
			 */
			pattern += len+1;
			str += len;
			/* DEBUG4("glob_pattern: rest of pattern '%s', str '%s'", pattern, str ); */
			/* check for trailing glob */
			if( c == '?' ){
				if( *str ){
					result = glob_pattern( pattern, str+1 ); 
				} else {
					result = 1;
				}
			} else if( c == '[' ){
				/* now we check for the characters in the pattern
				 * this can have the format N or N-M
				 */
				glob = safestrchr( pattern, ']' );
				if( glob == 0 ){
					return( 1 );
				}
				len = glob - pattern;
				invert = c = 0;
				/* DEBUG4("globmatch: now case '%s', len %d", pattern, len ); */
				if( len > 0 && (invert = (*pattern == '^')) ){
					--len; ++pattern;
				}
				while( result && len > 0 ){
					/* DEBUG4("globmatch: c '0x%x', pat '%c', str '%c'",
						c, *pattern, *str ); */
					if( c && *pattern == '-' && len > 1 ){
						/* we have preceeding, get end */
						pattern++; --len;
						while( result && c <= *pattern ){
					/* DEBUG4("globmatch: range c '%c', pat '%c', str '%c'",
								c, *pattern, *str ); */
							result = (*str != c++);
						}
						pattern++; --len;
						c = 0;
					} else {
						c = *pattern++; --len;
						result = (*str != c);
					}
				}
				if( invert ) result = !result;
				/* DEBUG4("globmatch: after pattern result %d", result); */
				if( result == 0 ){
					pattern += len+1;
					++str;
					/* at this point pattern is past */
					result = glob_pattern( pattern, str );
				}
			} else if( pattern[0] ){
				while( *str && (result = glob_pattern( pattern, str )) ){
					++str;
				}
			} else {
				result = 0;
			}
		}
	} else {
		result = safestrcasecmp( pattern, str );
	}
	return( result );
}

int Globmatch( char *pattern, const char *str )
{
	int result;

	/* try simple test first: string compare */
	/* DEBUG4("Globmatch: pattern '%s' to '%s'", pattern, str ); */
	if( pattern == 0 ) pattern = "";
	if( str == 0 ) str = "";
	result = glob_pattern( pattern, str );
	DEBUG4("Globmatch: '%s' to '%s' result %d", pattern, str, result );
	return( result );
}

int Globmatch_list( struct line_list *l, char *str )
{
	int result = 1, i;
	for( i = 0; result && i < l->count; ++i ){
		result = Globmatch(l->list[i],str);
	}
	return( result );
}

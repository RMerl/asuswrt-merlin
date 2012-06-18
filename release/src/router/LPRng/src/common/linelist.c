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
"$Id: linelist.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";

#include "lp.h"
#include "errorcodes.h"
#include "globmatch.h"
#include "gethostinfo.h"
#include "child.h"
#include "fileopen.h"
#include "getqueue.h"
#include "getprinter.h"
#include "lpd_logger.h"
#include "lpd_dispatch.h"
#include "lpd_jobs.h"
#include "linelist.h"

/**** ENDINCLUDE ****/

/* lowercase and uppercase (destructive) a string */
void lowercase( char *s )
{
	int c;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( isupper(c) ) *s = tolower(c);
		}
	}
}
void uppercase( char *s )
{
	int c;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( islower(c) ) *s = toupper(c);
		}
	}
}

/*
 * Trunc str - remove trailing white space (destructive)
 */

char *trunc_str( char *s)
{
	char *t;
	if(s && *s){
		for( t=s+safestrlen(s); t > s && isspace(cval(t-1)); --t );
		*t = 0;
	}
	return( s );
}

int Lastchar( char *s )
{
	int c = 0;
	if( s && *s ){
		s += safestrlen(s)-1;
		c = cval(s);
	}
	return(c);
}

/*
 * Memory Allocation Routines
 * - same as malloc, realloc, but with error messages
 */
#if defined(DMALLOC)
#undef malloc
#define malloc(size) \
  _malloc_leap(file, line, size)
#undef calloc
#define calloc(count, size) \
  _calloc_leap(file, line, count, size)
#undef realloc
#define realloc(ptr, size) \
  _realloc_leap(file, line, ptr, size)
#endif
void *malloc_or_die( size_t size, const char *file, int line )
{
    void *p;
    p = malloc(size);
    if( p == 0 ){
        LOGERR_DIE(LOG_INFO) "malloc of %d failed, file '%s', line %d",
			size, file, line );
    }
	DEBUG6("malloc_or_die: size %d, addr 0x%lx, file '%s', line %d",
		size,  Cast_ptr_to_long(p), file, line );
    return( p );
}

void *realloc_or_die( void *p, size_t size, const char *file, int line )
{
	void *orig = p;
	if( p == 0 ){
		p = malloc(size);
	} else {
		p = realloc(p, size);
	}
    if( p == 0 ){
        LOGERR(LOG_INFO) "realloc of 0x%lx, new size %d failed, file '%s', line %d",
			Cast_ptr_to_long(orig), size, file, line );
		abort();
    }
	DEBUG6("realloc_or_die: size %d, orig 0x%lx, addr 0x%lx, file '%s', line %d",
		size, Cast_ptr_to_long(orig), Cast_ptr_to_long(p), file, line );
    return( p );
}

/*
 * duplicate a string safely, generate an error message
 */

char *safestrdup (const char *p, const char *file, int line)
{
    char *new = 0;

	if( p == 0) p = "";
	new = malloc_or_die( safestrlen (p) + 1, file, line );
	strcpy( new, p );
	return( new );
}

/*
 * char *safestrdup2( char *s1, char *s2, char *file, int line )
 *  duplicate two concatenated strings
 *  returns: malloced string area
 */

char *safestrdup2( const char *s1, const char *s2, const char *file, int line )
{
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	return( s );
}

/*
 * char *safeextend2( char *s1, char *s2, char *file, int line )
 *  extends a malloc'd string
 *  returns: malloced string area
 */

char *safeextend2( char *s1, const char *s2, const char *file, int line )
{
	char *s;
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0);
	s = realloc_or_die( s1, n, file, line );
	if( s1 == 0 ) *s = 0;
	if( s2 ) strcat(s,s2);
	return(s);
}

/*
 * char *safestrdup3( char *s1, char *s2, char *s3, char *file, int line )
 *  duplicate three concatenated strings
 *  returns: malloced string area
 */

char *safestrdup3( const char *s1, const char *s2, const char *s3,
	const char *file, int line )
{
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0) + (s3?safestrlen(s3):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	return( s );
}


/*
 * char *safeextend3( char *s1, char *s2, char *s3 char *file, int line )
 *  extends a malloc'd string
 *  returns: malloced string area
 */

char *safeextend3( char *s1, const char *s2, const char *s3,
	const char *file, int line )
{
	char *s;
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0) + (s3?safestrlen(s3):0);
	s = realloc_or_die( s1, n, file, line );
	if( s1 == 0 ) *s = 0;
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	return(s);
}



/*
 * char *safeextend4( char *s1, char *s2, char *s3, char *s4,
 *	char *file, int line )
 *  extends a malloc'd string
 *  returns: malloced string area
 */

char *safeextend4( char *s1, const char *s2, const char *s3, const char *s4,
	const char *file, int line )
{
	char *s;
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0)
		+ (s3?safestrlen(s3):0) + (s4?safestrlen(s4):0);
	s = realloc_or_die( s1, n, file, line );
	if( s1 == 0 ) *s = 0;
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	if( s4 ) strcat(s,s4);
	return(s);
}

/*
 * char *safestrdup4
 *  duplicate four concatenated strings
 *  returns: malloced string area
 */

char *safestrdup4( const char *s1, const char *s2,
	const char *s3, const char *s4,
	const char *file, int line )
{
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0)
		+ (s3?safestrlen(s3):0) + (s4?safestrlen(s4):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	if( s4 ) strcat(s,s4);
	return( s );
}



/*
 * char *safeextend5( char *s1, char *s2, char *s3, char *s4, char *s5
 *	char *file, int line )
 *  extends a malloc'd string
 *  returns: malloced string area
 */

char *safeextend5( char *s1, const char *s2, const char *s3, const char *s4, const char *s5,
	const char *file, int line )
{
	char *s;
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0)
		+ (s3?safestrlen(s3):0) + (s4?safestrlen(s4):0) + (s5?safestrlen(s5):0);
	s = realloc_or_die( s1, n, file, line );
	if( s1 == 0 ) *s = 0;
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	if( s4 ) strcat(s,s4);
	if( s5 ) strcat(s,s5);
	return(s);
}


/*
 * char *safestrdup5
 *  duplicate five concatenated strings
 *  returns: malloced string area
 */

char *safestrdup5( const char *s1, const char *s2,
	const char *s3, const char *s4, const char *s5,
	const char *file, int line )
{
	int n = 1 + (s1?safestrlen(s1):0) + (s2?safestrlen(s2):0)
		+ (s3?safestrlen(s3):0) + (s4?safestrlen(s4):0) + (s5?safestrlen(s5):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	if( s4 ) strcat(s,s4);
	if( s5 ) strcat(s,s5);
	return( s );
}

/*
  Line Splitting and List Management
 
  Model:  we have a list of malloced and duplicated lines
          we never remove the lines unless we free them.
          we never put them in unless we malloc them
 */

/*
 * void Init_line_list( struct line_list *l )
 *  - inititialize a list by zeroing it
 */

void Init_line_list( struct line_list *l )
{
	memset(l, 0, sizeof(l[0]));
}

/*
 * void Free_line_list( struct line_list *l )
 *  - clear a list by freeing the allocated array
 */

void Free_line_list( struct line_list *l )
{
	int i;
	if( l == 0 ) return;
	if( l->list ){
		for( i = 0; i < l->count; ++i ){
			if( l->list[i] ) free( l->list[i]); l->list[i] = 0;
		}
		free(l->list);
	}
	memset(l,0,sizeof(l[0]));
}

void Free_listof_line_list( struct line_list *l )
{
	int i;
	struct line_list *lp;
	if( l == 0 ) return;
	for( i = 0; i < l->count; ++i ){
		lp = (void *)l->list[i];
		Free_line_list(lp);
		memset( lp, 0, sizeof(lp[0]) );
	}
	Free_line_list(l);
}

/*
 * void Check_max( struct line_list *l, int incr )
 *
 */

void Check_max( struct line_list *l, int incr )
{
	if( l->count+incr >= l->max ){
		l->max += 100+incr;
		if( !(l->list = realloc_or_die( l->list, l->max*sizeof(char *),
			__FILE__,__LINE__)) ){
			Errorcode = JFAIL;
			LOGERR(LOG_INFO) "Check_max: realloc %d failed",
				l->max*sizeof(char*) );
		}
	}
}


/*
 *char *Add_line_list( struct line_list *l, char *str,
 *  char *sep, int sort, int uniq )
 *  - add a copy of str to the line list
 *  sep      - key separator, used for sorting
 *  sort = 1 - sort the values
 *  uniq = 1 - only one value
 *  returns:  added string
 */

char *Add_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq )
{
	char *s = 0;
	int c = 0, cmp, mid;
	if(DEBUGL5){
		char b[48];
		int n;
		SNPRINTF( b,sizeof(b)-8)"%s",str );
		if( (n = safestrlen(b)) > (int)sizeof(b)-10 ) strcpy( b+n,"..." );
		LOGDEBUG("Add_line_list: '%s', sep '%s', sort %d, uniq %d",
			b, sep, sort, uniq );
	}

	Check_max(l, 2);
	str = safestrdup( str,__FILE__,__LINE__);
	if( sort == 0 ){
		l->list[l->count++] = str;
	} else {
		s = 0;
		if( sep && (s = safestrpbrk( str, sep )) ){ c = *s; *s = 0; }
		/* find everything <= the mid point */
		/* cmp = key <> list[mid] */
		cmp = Find_last_key( l, str, sep, &mid );
		if( s ) *s = c;
		/* str < list[mid+1] */
		if( cmp == 0 && uniq ){
			/* we replace */
			free( l->list[mid] );		
			l->list[mid] = str;
		} else if( cmp >= 0 ){
			/* we need to insert after mid */
			++l->count;
			memmove( l->list+mid+2, l->list+mid+1,
				sizeof( char * ) * (l->count - mid - 1));
			l->list[mid+1] = str;
		} else if( cmp < 0 ) {
			/* we need to insert before mid */
			++l->count;
			memmove( l->list+mid+1, l->list+mid,
				sizeof( char * ) * (l->count - mid));
			l->list[mid] = str;
		}
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL5)Dump_line_list("Add_line_list: result", l);
#endif
	return( str );
}

/*
 *void Add_casekey_line_list( struct line_list *l, char *str,
 *  char *sep, int sort, int uniq )
 *  - add a copy of str to the line list, using case sensitive keys
 *  sep      - key separator, used for sorting
 *  sort = 1 - sort the values
 *  uniq = 1 - only one value
 */

void Add_casekey_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq )
{
	char *s = 0;
	int c = 0, cmp, mid;
	if(DEBUGL5){
		char b[40];
		int n;
		SNPRINTF( b,sizeof(b)-8)"%s",str );
		if( (n = safestrlen(b)) > (int)sizeof(b)-10 ) strcpy( b+n,"..." );
		LOGDEBUG("Add_casekey_line_list: '%s', sep '%s', sort %d, uniq %d",
			b, sep, sort, uniq );
	}

	Check_max(l, 2);
	str = safestrdup( str,__FILE__,__LINE__);
	if( sort == 0 ){
		l->list[l->count++] = str;
	} else {
		s = 0;
		if( sep && (s = safestrpbrk( str, sep )) ){ c = *s; *s = 0; }
		/* find everything <= the mid point */
		/* cmp = key <> list[mid] */
		cmp = Find_last_casekey( l, str, sep, &mid );
		if( s ) *s = c;
		/* str < list[mid+1] */
		if( cmp == 0 && uniq ){
			/* we replace */
			free( l->list[mid] );		
			l->list[mid] = str;
		} else if( cmp >= 0 ){
			/* we need to insert after mid */
			++l->count;
			memmove( l->list+mid+2, l->list+mid+1,
				sizeof( char * ) * (l->count - mid - 1));
			l->list[mid+1] = str;
		} else if( cmp < 0 ) {
			/* we need to insert before mid */
			++l->count;
			memmove( l->list+mid+1, l->list+mid,
				sizeof( char * ) * (l->count - mid));
			l->list[mid] = str;
		}
	}
	/* if(DEBUGL4)Dump_line_list("Add_casekey_line_list: result", l); */
}

/*
 * Prefix_line_list( struct line_list *l, char *str )
 *  put the str at the front of the line list
 */
void Prefix_line_list( struct line_list *l, char *str )
{
	Check_max(l, 2);
	str = safestrdup( str,__FILE__,__LINE__);

	memmove( &l->list[1], &l->list[0], l->count * sizeof(l->list[0]) );
	l->list[0] = str;
	++l->count;
}

void Merge_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq )
{
	int i;
	for( i = 0; i < src->count; ++i ){
		Add_line_list( dest, src->list[i], sep, sort, uniq );
	}
}

void Merge_listof_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq )
{
	struct line_list *sp, *dp;
	int i;
	for( i = 0; i < src->count; ++i ){
		if( (sp = (void *)src->list[i]) ){
			Check_max( dest, 1 );
			dp = malloc_or_die(sizeof(dp[0]),__FILE__,__LINE__);
			memset(dp,0,sizeof(dp[0]));
			Merge_line_list( dp, sp, sep, sort, uniq);
			dest->list[dest->count++] = (void *)dp;
		}
	}
}


void Move_line_list( struct line_list *dest, struct line_list *src )
{
	int i;
	
	Free_line_list(dest);
	Check_max(dest,src->count);
	for( i = 0; i < src->count; ++i ){
		dest->list[i] = src->list[i];
		src->list[i] = 0;
	}
	src->count = 0;
	dest->count = i;
}

/*
 * Split( struct line_list *l, char *str, int sort, char *keysep,
 *		int uniq, int trim, int nocomments, char *escape )
 *  Split the str up into strings, as delimted by sep.
 *   put duplicates of the original into the line_list l.
 *  If sort != 0, then sort them using keysep to separate sort key from value
 *  if uniq != then replace rather than add entries
 *  if trim != 0 then remove leading and trailing whitespace and
 *    if trim is a printable character any characters at start == trim
 *  if nocomments != 0, then remove comments as well
 *  if escape != 0, then allow the characters in the string to be escaped
 *     i.e. - escape = ":" then \: would be replace by :
 *
 */
void Split( struct line_list *l, char *str, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments, char *escape )
{
	char *end = 0, *t, *buffer = 0;
	int len, blen = 0;
	if(DEBUGL5){
		char b[40];
		int n;
		SNPRINTF( b,sizeof(b)-8)"%s",str );
		if( (n = safestrlen(b)) > (int)sizeof(b)-10 ) strcpy( b+n,"..." );
		LOGDEBUG("Split: str 0x%lx '%s', sep '%s', escape '%s', sort %d, keysep '%s', uniq %d, trim %d",
			Cast_ptr_to_long(str), b, sep, escape, sort, keysep, uniq, trim );
	}
	for( ; str && *str; str = end ){
		end = 0;
		t = str;
		if( !ISNULL(sep) ) while( (t = safestrpbrk( t, sep )) ){
			if( escape && t != str && cval(t-1) == '\\'
				&& strchr( escape, cval(t) ) ){
				++t;
				DEBUG5("Split: escape '%s'", t );
				continue;
			}
			end = t+1;
			break;
		}
		if( !end ){
			t = str + safestrlen(str);
		}
		DEBUG5("Split: str 0x%lx, ('%c%c...') end 0x%lx, t 0x%lx",
			Cast_ptr_to_long(str), str[0], str[1],
			Cast_ptr_to_long(end), Cast_ptr_to_long(t));
		if( trim ){
			while( isspace(cval(str)) ) ++str;
			/* you can also remove leading characters */
			if( cval(str) == trim && isprint(trim) ) ++str;
			for( ; t > str && isspace(cval(t-1)); --t );
		}
		len = t - str;
		DEBUG5("Split: after trim len %d, str 0x%lx, end 0x%lx, t 0x%lx",
			len, Cast_ptr_to_long(str),
			Cast_ptr_to_long(end), Cast_ptr_to_long(t));
		if( len <= 0 || (nocomments && *str == '#') ) continue;
		if( blen <= len ){
			blen = 2*len;
			buffer = realloc_or_die(buffer,blen+1,__FILE__,__LINE__);
		}
		memmove(buffer,str,len);
		buffer[len] = 0;
		Add_line_list( l, buffer, keysep, sort, uniq );
	}
	if( buffer ) free(buffer);
}

char *Join_line_list( struct line_list *l, char *sep )
{
	char *s, *t, *str = 0;
	int len = 0, i, n = 0;

	if( sep ) n = safestrlen(sep);
	for( i = 0; i < l->count; ++i ){
		s = l->list[i];
		if( s && *s ) len += safestrlen(s) + n;
	}
	if( len ){
		str = malloc_or_die(len+1,__FILE__,__LINE__);
		t = str;
		for( i = 0; i < l->count; ++i ){
			s = l->list[i];
			if( s && *s ){
				strcpy( t, s );
				t += safestrlen(t);
				if( n ){
					strcpy(t,sep);
					t += n;
				}
			}
		}
		*t = 0;
	}
	return( str );
}

char *Join_line_list_with_sep( struct line_list *l, char *sep )
{
	char *s = Join_line_list( l, sep );
	int len = 0;

	if( sep ) len = safestrlen(sep);
	if( s ){
		*(s+safestrlen(s)-len) = 0;;
	}
	return( s );
}

/*
 * join the line list with a separator, putting quotes around
 *  the entries starting at position 1.
 */
char *Join_line_list_with_quotes( struct line_list *l, char *sep )
{
	char *s, *t, *str = 0;
	int len = 0, i, n = 0;

	if( sep ) n = safestrlen(sep);
	for( i = 0; i < l->count; ++i ){
		s = l->list[i];
		if( s && *s ) len += safestrlen(s) + n + 2;
	}
	if( len ){
		str = malloc_or_die(len+1,__FILE__,__LINE__);
		t = str;
		for( i = 0; i < l->count; ++i ){
			s = l->list[i];
			if( s && *s ){
				if( i ) *t++ = '\'';
				strcpy( t, s );
				t += safestrlen(t);
				if( i ) *t++ = '\'';
				if( n ){
					strcpy(t,sep);
					t += n;
				}
			}
		}
		*t = 0;
	}
	return( str );
}

#ifdef ORIGINAL_DEBUG//JY@1020
void Dump_line_list( const char *title, struct line_list *l )
{
	int i;
	LOGDEBUG("Dump_line_list: %s - 0x%lx, count %d, max %d, list 0x%lx",
		title, Cast_ptr_to_long(l), l?l->count:0, l?l->max:0, l?Cast_ptr_to_long(l->list):(long)0 );
	if(l)for( i = 0; i < l->count; ++i ){
		LOGDEBUG( "  [%2d] 0x%lx ='%s'", i, Cast_ptr_to_long(l->list[i]), l->list[i] );
	}
}
#endif

#ifdef ORIGINAL_DEBUG//JY@1020
void Dump_line_list_sub( const char *title, struct line_list *l )
{
	int i;
	LOGDEBUG(" %s - 0x%lx, count %d, max %d, list 0x%lx",
		title, Cast_ptr_to_long(l), l?l->count:0, l?l->max:0, l?Cast_ptr_to_long(l->list):(long)0 );
	if(l)for( i = 0; i < l->count; ++i ){
		LOGDEBUG( "  [%2d] 0x%lx ='%s'", i, Cast_ptr_to_long(l->list[i]), l->list[i] );
	}
}
#endif

/*
 * Find_str_in_flat
 *   find the string value starting with key and ending with sep
 */
char *Find_str_in_flat( char *str, const char *key, const char *sep )
{
	char *s, *end;
	int n, c = 0;

	if( str == 0 || key == 0 || sep == 0 ) return( 0 );
	n = safestrlen(key);
	for( s = str; (s = strstr(s,key)); ){
		s += n;
		if( *s == '=' ){
			++s;
			if( (end = safestrpbrk( s, sep )) ) { c = *end; *end = c; }
			s = safestrdup(s,__FILE__,__LINE__);
			if( end ) *end = c;
			return( s );
		}
	}
	return( 0 );
}

/*
 * int Find_first_key( struct line_list *l, char *key, char *sep, int *mid )
 * int Find_last_key( struct line_list *l, char *key, char *sep, int *mid )
 *  Search the list for the last corresponding key value
 *  The list has lines of the form:
 *    key [separator] value
 *  returns:
 *    *at = index of last tested value
 *    return value: 0 if found;
 *                  <0 if list[*at] < key
 *                  >0 if list[*at] > key
 */

int Find_last_key( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_last_key: count %d, key '%s'", l->count, key );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcasecmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid+1 < l->count ){
			s = l->list[mid+1];
			DEBUG5("Find_last_key: existing entry, mid %d, '%s'",
				mid, l->list[mid] );
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcasecmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			++mid;
		}
		DEBUG5("Find_last_key: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_last_key: key '%s', cmp %d, mid %d", key, cmp, mid );
	return( cmp );
}


/*
 * int Find_first_casekey( struct line_list *l, char *key, char *sep, int *mid )
 * int Find_last_casekey( struct line_list *l, char *key, char *sep, int *mid )
 *  Search the list for the last corresponding key value using case sensitive keys
 *  The list has lines of the form:
 *    key [separator] value
 *  returns:
 *    *at = index of last tested value
 *    return value: 0 if found;
 *                  <0 if list[*at] < key
 *                  >0 if list[*at] > key
 */

int Find_last_casekey( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_last_casekey: count %d, key '%s'", l->count, key );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid+1 < l->count ){
			s = l->list[mid+1];
			DEBUG5("Find_last_key: existing entry, mid %d, '%s'",
				mid, l->list[mid] );
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			++mid;
		}
		DEBUG5("Find_last_casekey: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_last_casekey: key '%s', cmp %d, mid %d", key, cmp, mid );
	return( cmp );
}

int Find_first_key( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_first_key: count %d, key '%s', sep '%s'",
		l->count, key, sep );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcasecmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid > 0 ){
			s = l->list[mid-1];
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcasecmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			--mid;
		}
		DEBUG5("Find_first_key: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_first_key: cmp %d, mid %d, key '%s', count %d",
		cmp, mid, key, l->count );
	return( cmp );
}

int Find_first_casekey( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_first_casekey: count %d, key '%s', sep '%s'",
		l->count, key, sep );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid > 0 ){
			s = l->list[mid-1];
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			--mid;
		}
		DEBUG5("Find_first_casekey: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_first_casekey: cmp %d, mid %d, key '%s', count %d",
		cmp, mid, key, l->count );
	return( cmp );
}

/*
 * char *Find_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    "1"
 *   key@   "0"
 *   key#v  v
 *   key=v  v
 *   key v  v
 *  If key does not exist, we return "0"
 */

const char *Find_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = "0";
	int mid, cmp = -1;

	DEBUG5("Find_value: key '%s', sep '%s'", key, sep );
	if( l ) cmp = Find_first_key( l, key, sep, &mid );
	DEBUG5("Find_value: key '%s', cmp %d, mid %d", key, cmp, mid );
	if( cmp==0 ){
		if( sep ){
			s = Fix_val( safestrpbrk(l->list[mid], sep ) );
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_value: key '%s', value '%s'", key, s );
	return(s);
}

/*
 * char *Find_first_letter( struct line_list *l, char letter, int *mid )
 *   return the first entry starting with the letter
 */

char *Find_first_letter( struct line_list *l, const char letter, int *mid )
{
	char *s = 0;
	int i;
	if(l)for( i = 0; i < l->count; ++i ){
		if( (s = l->list[i])[0] == letter ){
			if( mid ) *mid = i;
			DEBUG4( "Find_first_letter: letter '%c', at [%d]=value '%s'", letter, i, s );
			return(s+1);
		}
	}
	return(0);
}

/*
 * char *Find_exists_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    "1"
 *   key@   "0"
 *   key#v  v
 *   key=v  v
 *   If value exists we return 0 (null)
 */

const char *Find_exists_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = 0;
	int mid, cmp = -1;

	if( l ) cmp = Find_first_key( l, key, sep, &mid );
	if( cmp==0 ){
		if( sep ){
			s = Fix_val( safestrpbrk(l->list[mid], sep ) );
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_exists_value: key '%s', cmp %d, value '%s'", key, cmp, s );
	return(s);
}


/*
 * char *Find_str_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    0
 *   key@   0
 *   key#v  0
 *   key=v  v
 */

char *Find_str_value( struct line_list *l, const char *key, const char *sep )
{
	char *s = 0;
	int mid, cmp = -1;

	if( l ) cmp = Find_first_key( l, key, sep, &mid );
	if( cmp==0 ){
		/*
		 *  value: NULL, "", "@", "=xx", "#xx".
		 *  returns: "0", "1","0",  "xx",  "xx"
		 */
		if( sep ){
			s = safestrpbrk(l->list[mid], sep );
			if( s && *s == '=' ){
				++s;
			} else {
				s = 0;
			}
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_str_value: key '%s', value '%s'", key, s );
	return(s);
}
 

/*
 * char *Find_casekey_str_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value using case sensitive keys
 *          value
 *   key    0
 *   key@   0
 *   key#v  0
 *   key=v  v
 */

char *Find_casekey_str_value( struct line_list *l, const char *key, const char *sep )
{
	char *s = 0;
	int mid, cmp = -1;

	if( l ) cmp = Find_first_casekey( l, key, sep, &mid );
	if( cmp==0 ){
		/*
		 *  value: NULL, "", "@", "=xx", "#xx".
		 *  returns: "0", "1","0",  "xx",  "xx"
		 */
		if( sep ){
			s = safestrpbrk(l->list[mid], sep );
			if( s && *s == '=' ){
				++s;
			} else {
				s = 0;
			}
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_casekey_str_value: key '%s', value '%s'", key, s );
	return(s);
}
 
 
/*
 * Set_str_value( struct line_list *l, char *key, char *value )
 *   set a string value in an ordered, sorted list
 */
void Set_str_value( struct line_list *l, const char *key, const char *value )
{
	char *s = 0;
	int mid;
	if( key == 0 ) return;
	if(DEBUGL6){
		char buffer[16];
		SNPRINTF(buffer,sizeof(buffer)-5)"%s",value);
		buffer[12] = 0;
		if( value && safestrlen(value) > 12 ) strcat(buffer,"...");
		LOGDEBUG("Set_str_value: '%s'= 0x%lx '%s'", key,
			Cast_ptr_to_long(value), buffer);
	}
	if( value && *value ){
		s = safestrdup3(key,"=",value,__FILE__,__LINE__);
		Add_line_list(l,s,Value_sep,1,1);
		if(s) free(s); s = 0;
	} else if( !Find_first_key(l, key, Value_sep, &mid ) ){
		Remove_line_list(l,mid);
	}
}
 
/*
 * Set_expanded_str_value( struct line_list *l, char *key, char *value )
 *   set a string value in an ordered, sorted list
 */
void Set_expanded_str_value( struct line_list *l, const char *key, const char *orig )
{
	char *s = 0;
	char *value = 0;
	int mid;
	if( key == 0 ) return;
	value = Fix_str( (char *)orig );
	if(DEBUGL6){
		char buffer[16];
		SNPRINTF(buffer,sizeof(buffer)-5)"%s",value);
		buffer[12] = 0;
		if( value && safestrlen(value) > 12 ) strcat(buffer,"...");
		LOGDEBUG("Set_str_value: '%s'= 0x%lx '%s'", key,
			Cast_ptr_to_long(value), buffer);
	}
	if( value && *value ){
		s = safestrdup3(key,"=",value,__FILE__,__LINE__);
		Add_line_list(l,s,Value_sep,1,1);
		if(s) free(s); s = 0;
	} else if( !Find_first_key(l, key, Value_sep, &mid ) ){
		Remove_line_list(l,mid);
	}
	if( value ) free(value); value = 0;
}
 
/*
 * Set_casekey_str_value( struct line_list *l, char *key, char *value )
 *   set an string value in an ordered, sorted list, with case sensitive keys
 */
void Set_casekey_str_value( struct line_list *l, const char *key, const char *value )
{
	char *s = 0;
	int mid;
	if( key == 0 ) return;
	if(DEBUGL6){
		char buffer[16];
		SNPRINTF(buffer,sizeof(buffer)-5)"%s",value);
		buffer[12] = 0;
		if( value && safestrlen(value) > 12 ) strcat(buffer,"...");
		LOGDEBUG("Set_str_value: '%s'= 0x%lx '%s'", key,
			Cast_ptr_to_long(value), buffer);
	}
	if( value && *value ){
		s = safestrdup3(key,"=",value,__FILE__,__LINE__);
		Add_casekey_line_list(l,s,Value_sep,1,1);
		if(s) free(s); s = 0;
	} else if( !Find_first_casekey(l, key, Value_sep, &mid ) ){
		Remove_line_list(l,mid);
	}
}

 
/*
 * Set_flag_value( struct line_list *l, char *key, int value )
 *   set a flag value in an ordered, sorted list
 */
void Set_flag_value( struct line_list *l, const char *key, long value )
{
	char buffer[SMALLBUFFER];
	if( key == 0 ) return;
	SNPRINTF(buffer,sizeof(buffer))"%s=0x%lx",key,value);
	Add_line_list(l,buffer,Value_sep,1,1);
}

 
/*
 * Set_nz_flag_value( struct line_list *l, char *key, int value )
 *   set a nonzero flag value in an ordered, sorted list
 */
void Set_nz_flag_value( struct line_list *l, const char *key, long value )
{
	if( !Find_flag_value( l, key, Value_sep ) ){
		Set_flag_value( l, key, value );
	}
}

 
/*
 * Set_double_value( struct line_list *l, char *key, int value )
 *   set a double value in an ordered, sorted list
 */
void Set_double_value( struct line_list *l, const char *key, double value )
{
	char buffer[SMALLBUFFER];
	if( key == 0 ) return;
	SNPRINTF(buffer,sizeof(buffer))"%s=%0.0f",key,value);
	Add_line_list(l,buffer,Value_sep,1,1);
}

 
/*
 * Set_decimal_value( struct line_list *l, char *key, int value )
 *   set a decimal value in an ordered, sorted list
 */
void Set_decimal_value( struct line_list *l, const char *key, long value )
{
	char buffer[SMALLBUFFER];
	if( key == 0 ) return;
	SNPRINTF(buffer,sizeof(buffer))"%s=%ld",key,value);
	Add_line_list(l,buffer,Value_sep,1,1);
}
/*
 * Remove_line_list( struct line_list *l, int mid ) 
 *   Remove the indicated entry and move the other
 *   entries up.
 */
void Remove_line_list( struct line_list *l, int mid )
{
	char *s;
	if( mid >= 0 && mid < l->count ){
		if( (s = l->list[mid]) ){
			free(s);
			l->list[mid] = 0;
		}
		memmove(&l->list[mid],&l->list[mid+1],(l->count-mid-1)*sizeof(char *));
		--l->count;
	}
}


/*
 * Remove_duplicates_line_list( struct line_list *l )
 *   Remove duplicate entries in the list
 */
void Remove_duplicates_line_list( struct line_list *l )
{
	char *s, *t;
	int i, j;
	for( i = 0; i < l->count; ){
		if( (s = l->list[i]) ){
			for( j = i+1; j < l->count; ){
				if( !(t = l->list[j]) || !safestrcmp(s,t) ){
					Remove_line_list( l, j );
				} else {
					++j;
				}
			}
			++i;
		} else {
			Remove_line_list( l, i );
		}
	}
}


/*
 * char *Find_flag_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    1
 *   key@   0
 *   key#v  v  if v is integer, 0 otherwise
 *   key=v  v  if v is integer, 0 otherwise
 */

int Find_flag_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s;
	char *e;
	int n = 0;

	if( l && (s = Find_value( l, key, sep )) ){
		e = 0;
		n = strtol(s,&e,0);
		if( !e || *e ) n = 0;
	}
	DEBUG4( "Find_flag_value: key '%s', value '%d'", key, n );
	return(n);
}
 

/*
 * char *Find_decimal_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    1
 *   key@   0
 *   key#v  v  if v is decimal, 0 otherwise
 *   key=v  v  if v is decimal, 0 otherwise
 */

int Find_decimal_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = 0;
	char *e;
	int n = 0;

	if( l && (s = Find_value( l, key, sep )) ){
		e = 0;
		n = strtol(s,&e,10);
		if( !e || *e ){
			e = 0;
			n = strtol(s,&e,0);
			if( !e || *e ) n = 0;
		}
	}
	DEBUG4( "Find_decimal_value: key '%s', value '%d'", key, n );
	return(n);
}
 

/*
 * double Find_double_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    1
 *   key@   0
 *   key#v  v  if v is decimal, 0 otherwise
 *   key=v  v  if v is decimal, 0 otherwise
 */

double Find_double_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = 0;
	char *e;
	double n = 0;

	if( l && (s = Find_value( l, key, sep )) ){
		e = 0;
		n = strtod(s,&e);
	}
	DEBUG4( "Find_double_value: key '%s', value '%0.0f'", key, n );
	return(n);
}
 
/*
 * char *Fix_val( char *s )
 *  passed: NULL, "", "@", "=xx", "#xx".
 *  returns: "0", "1","0",  "xx",  "xx"
 */


const char *Fix_val( const char *s )
{
	int c = 0;
	if( s ){
		c = cval(s);
		++s;
		while( isspace(cval(s)) ) ++s;
	}
	if( c == 0 ){
		s = "1";
	} else if( c == '@' ){
		s = "0";
	}
	return( s );
}

/*
 * Find_tags( struct line_list dest,
 *  struct line_list *list, char *tag )
 * 
 * Scan the list (ordered, of course) for the
 * set of entries starting with 'tag' and extract them
 * to list
 */

void Find_tags( struct line_list *dest, struct line_list *l, char *key )
{
	int cmp=-1, cmpl = 0, bot, top, mid, len;
	char *s;

	if( key == 0 || *key == 0 ) return;
	mid = bot = 0; top = l->count-1;
	len = safestrlen(key);
	DEBUG5("Find_tags: count %d, key '%s'", l->count, key );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		cmp = safestrncasecmp(key,s,len);
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid > 0 ){
			DEBUG5("Find_tags: existing entry, mid %d, '%s'", mid, l->list[mid] );
			s = l->list[mid-1];
			cmpl = safestrncasecmp(s,key,len);
			if( cmpl ) break;
			--mid;
		}
		DEBUG5("Find_tags: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( cmp == 0 ){
		s = l->list[mid];
		do{
			DEBUG5("Find_tags: adding '%s'", s+len );
			Add_line_list(dest,s+len,Value_sep,1,1);
			++mid;
		} while( mid < l->count
			&& (s = l->list[mid])
			&& !(cmp = safestrncasecmp(key,s,len)));
	}
}

/*
 * Find_defaulttags( struct line_list dest,
 *  struct keywords *var_list, char *tag )
 * 
 * Scan the variable list for default values
 * starting with 'tag' and extract them
 * to list
 */

void Find_default_tags( struct line_list *dest,
	struct keywords *var_list, char *tag )
{
	int len = safestrlen(tag);
	char *key, *value;

	if( var_list ) while( var_list->keyword ){
		if( !strncmp((key = var_list->keyword), tag, len)
			&& (value = var_list->default_value) ){
			if( *value == '=' ) ++value;
			DEBUG5("Find_default_tags: adding '%s'='%s'", key, value);
			Set_str_value(dest, key+len, value );
		}
		++var_list;
	}
}



/*
 * Read_file_list( struct line_list *model, char *str
 *	char *sep, int sort, char *keysep, int uniq, int trim, int marker,
 *  int doinclude, int nocomment, int depth, int maxdepth )
 *  read the model information from these files
 *  if marker != then add a NULL line after each file
 */

void Read_file_list( int required, struct line_list *model, char *str,
	const char *linesep, int sort, const char *keysep, int uniq, int trim,
	int marker, int doinclude, int nocomment, int depth, int maxdepth )
{
	struct line_list l;
	int i, start, end, c=0, n, found;
	char *s, *t;
	struct stat statb;

	Init_line_list(&l);
	DEBUG3("Read_file_list: '%s', doinclude %d, depth %d, maxdepth %d",
		str, doinclude, depth, maxdepth );
	if( depth > maxdepth ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_ERR)
			"Read_file_list: recursion depth %d exceeds maxdepth %d for file '%s'",
			depth, maxdepth, str );
	}
	Split( &l, str, File_sep, 0, 0, 0, 1, 0 ,0);
	start = model->count;
	for( i = 0; i < l.count; ++i ){
		if( stat( l.list[i], &statb ) == -1 ){
			if( required || depth ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_ERR)
					"Read_file_list: cannot stat required or included file '%s'",
					l.list[i] );
			}
			continue;
		}
		Read_file_and_split( model, l.list[i], linesep, sort, keysep,
			uniq, trim, nocomment );
		if( doinclude ){
			/* scan through the list, looking for include lines */
			for( end = model->count; start < end; ){
				t = 0; 
				s = model->list[start];
				found = 0;
				t = 0;
				if( s && (t = safestrpbrk( s, Whitespace )) ){
					c = *t; *t = 0;
					found = !safestrcasecmp( s, "include" );
					*t = c;
				}
				if( found ){
					DEBUG4("Read_file_list: include '%s'", t+1 );
					Read_file_list( 1, model, t+1, linesep, sort, keysep, uniq, trim,
						marker, doinclude, nocomment, depth+1, maxdepth );
					/* at this point the include lines are at
					 *  end to model->count-1
					 * we need to move the lines from start to end-1
					 * to model->count, and then move end to start
					 */
					n = end - start;
					Check_max( model, n );
					/* copy to end */
#ifdef ORIGINAL_DEBUG//JY@1020
					if(DEBUGL5)Dump_line_list("Read_file_list: include before",
						model );
#endif
					memmove( &model->list[model->count], 
						&model->list[start], n*sizeof(char *) );
					memmove( &model->list[start], 
						&model->list[end],(model->count-start)*sizeof(char *));
#ifdef ORIGINAL_DEBUG//JY@1020
					if(DEBUGL4)Dump_line_list("Read_file_list: include after",
						model );
#endif
					end = model->count;
					start = end - n;
					DEBUG4("Read_file_list: start now '%s'",model->list[start]);
					/* we get rid of include line */
					s = model->list[start];
					free(s);
					model->list[start] = 0;
					memmove( &model->list[start], &model->list[start+1],
						n*sizeof(char *) );
					--model->count;
					end = model->count;
				} else {
					++start;
				}
			}
		}
		if( marker ){
			/* put null at end of list */
			Check_max( model, 1 );
			model->list[model->count++] = 0;
		}
	}
	Free_line_list(&l);
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL5)Dump_line_list("Read_file_list: result", model);
#endif
}

void Read_fd_and_split( struct line_list *list, int fd,
	const char *linesep, int sort, const char *keysep, int uniq,
	int trim, int nocomment )
{
	int size = 0, count, len;
	char *sv;
	char buffer[LARGEBUFFER];

	sv = 0;
	while( (count = read(fd, buffer, sizeof(buffer)-1)) > 0 ){
		buffer[count] = 0;
		len = size+count+1;
		if( (sv = realloc_or_die( sv, len,__FILE__,__LINE__)) == 0 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO) "Read_fd_and_split: realloc %d failed", len );
		}
		memmove( sv+size, buffer, count );
		size += count;
		sv[size] = 0;
	}
	close( fd );
	DEBUG4("Read_fd_and_split: size %d", size );
	Split( list, sv, linesep, sort, keysep, uniq, trim, nocomment ,0);
	if( sv ) free( sv );
}

void Read_file_and_split( struct line_list *list, char *file,
	const char *linesep, int sort, const char *keysep, int uniq,
	int trim, int nocomment )
{
	int fd;
	struct stat statb;

	DEBUG3("Read_file_and_split: '%s', trim %d, nocomment %d",
		file, trim, nocomment );
	if( (fd = Checkread( file, &statb )) < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		LOGERR_DIE(LOG_INFO)
		"Read_file_and_split: cannot open '%s' - '%s'",
			file, Errormsg(errno) );
#endif
	}
	Read_fd_and_split( list, fd, linesep, sort, keysep, uniq,
		trim, nocomment );
}

#ifdef REMOVE
/*
 * Printcap information
 */


/*
 * Build_pc_names( struct line_list *names, struct line_list *order, char *s )
 *  names = list of aliases and names
 *  order = order that names were found
 *
 *   get the primary name
 *   if it is not in the names lists, add to order list
 *   put the names and aliases in the names list
 */
int  Build_pc_names( struct line_list *names, struct line_list *order,
	char *str, struct host_information *hostname  )
{
	char *s, *t;
	int c = 0, i, ok = 0, len, start_oh, end_oh;
	struct line_list l, opts, oh;

	Init_line_list(&l);
	Init_line_list(&opts);
	Init_line_list(&oh);

	DEBUG4("Build_pc_names: '%s'", str);
	if( (s = safestrpbrk(str, ":")) ){
		c = *s; *s = 0;
		Split(&opts,s+1,":",1,Value_sep,0,1,0,":");
#if 0
		for( i = 0; i < opts.count; ++i ){
			t = opts.list[i];  
			while( t && (t = strstr(t,"\\:")) ){
				memmove(t, t+1, safestrlen(t+1)+1 );
				++t;
			}
		}
#endif
	}
	Split(&l,str,"|",0,0,0,1,0,0);
	if( s ) *s = c;
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Build_pc_names- names", &l);
	if(DEBUGL4)Dump_line_list("Build_pc_names- options", &opts);
#endif
	if( l.count == 0 ){
		if(Warnings){
			WARNMSG(
			"no name for printcap entry '%s'", str );
		} else {
			LOGMSG(LOG_INFO)
			"no name for printcap entry '%s'", str );
		}
	} else {
		ok = 1;
		if( Find_flag_value( &opts,SERVER,Value_sep) && !Is_server ){
			DEBUG4("Build_pc_names: not server" );
			ok = 0;
		} else if( Find_flag_value( &opts,CLIENT,Value_sep) && Is_server ){
			DEBUG4("Build_pc_names: not client" );
			ok = 0;
		} else if( !Find_first_key(&opts,"oh",Value_sep,&start_oh)
			&& !Find_last_key(&opts,"oh",Value_sep,&end_oh) ){
			ok = 0;
			DEBUG4("Build_pc_names: start_oh %d, end_oh %d",
				start_oh, end_oh );
			for( i = start_oh; !ok && i <= end_oh; ++i ){
				DEBUG4("Build_pc_names: [%d] '%s'", i, opts.list[i] );
				if( (t = safestrchr( opts.list[i], '=' )) ){
					Split(&oh,t+1,File_sep,0,0,0,1,0,0);
					ok = !Match_ipaddr_value(&oh, hostname);
					DEBUG4("Build_pc_names: check host '%s', ok %d",
						t+1, ok );
					Free_line_list(&oh);
				}
			}
		}
		if( ok && ((s = safestrpbrk( l.list[0], Value_sep))
			|| (s = safestrpbrk( l.list[0], "@")) ) ){
			ok = 0;
			if(Warnings){
				WARNMSG(
				"bad printcap name '%s', has '%c' character",
				l.list[0], *s );
			} else {
				LOGMSG(LOG_INFO)
				"bad printcap name '%s', has '%c' character",
				l.list[0], *s );
			}
		} else if( ok ){
#ifdef ORIGINAL_DEBUG//JY@1020
			if(DEBUGL4)Dump_line_list("Build_pc_names: adding ", &l);
			if(DEBUGL4)Dump_line_list("Build_pc_names- before names", names );
			if(DEBUGL4)Dump_line_list("Build_pc_names- before order", order );
#endif
			if( !Find_exists_value( names, l.list[0], Value_sep ) ){
				Add_line_list(order,l.list[0],0,0,0);
			}
			for( i = 0; i < l.count; ++i ){
				if( safestrpbrk( l.list[i], Value_sep ) ){
					continue;
				}
				Set_str_value(names,l.list[i],l.list[0]);
			}
			len = safestrlen(str);
			s = str;
			DEBUG4("Build_pc_names: before '%s'", str );
			*s = 0;
			for( i = 0; i < l.count; ++i ){
				if( *str ) *s++ = '|';
				strcpy(s,l.list[i]);
				s += safestrlen(s);
			}
			for( i = 0; i < opts.count; ++i ){
				*s++ = ':';
				strcpy(s,opts.list[i]);
				s += safestrlen(s);
			}
			if( safestrlen(str) > len ){
				Errorcode = JABORT;
				FATAL(LOG_ERR) "Build_pc_names: LINE GREW! fatal error");
			}
			DEBUG4("Build_pc_names: after '%s'", str );
		}
	}
	
	Free_line_list(&l);
	Free_line_list(&opts);
	DEBUG4("Build_pc_names: returning ok '%d'", ok );
	return ok;
}

/*
 * Build_printcap_info
 *  OUTPUT
 *  names = list of names in the form
 *           alias=primary
 *  order = list of names in order
 *  list  = list of all of the printcap entries
 *  INPUT
 *  input = orginal list information in split line format
 *
 *  run through the raw information, extrating primary name and aliases
 *  create entries in the names and order lists
 */
void Build_printcap_info( 
	struct line_list *names, struct line_list *order,
	struct line_list *list, struct line_list *raw,
	struct host_information *hostname )
{
	int i, c;
	char *t, *keyid = 0;
	int appendline = 0;

	DEBUG1("Build_printcap_info: list->count %d, raw->count %d",
		list->count, raw->count );
	for( i = 0; i < raw->count; ++i ){
		t = raw->list[i];
		DEBUG4("Build_printcap_info: doing '%s'", t );
		if( t ) while( isspace( cval(t) ) ) ++t;
		/* ignore blank lines and comments */
		if( t == 0 || (c = *t) == 0 || c == '#') continue;
		/* append lines starting with :, | */
		if( keyid
			&& (safestrchr(Printcap_sep,c) || appendline) ){
			DEBUG4("Build_printcap_info: old keyid '%s', adding '%s'",
				keyid, t );
			keyid = safeextend3(keyid, " ", t,__FILE__,__LINE__ );
			if( (appendline = (Lastchar(keyid) == '\\')) ){
				keyid[safestrlen(keyid)-1] = 0;
			}
		} else {
			DEBUG4("Build_printcap_info: old keyid '%s', new '%s'",
				keyid, t );
			if( keyid ){
				if( Build_pc_names( names, order, keyid, hostname ) ){
					Add_line_list( list, keyid, Printcap_sep, 1, 0 );
				}
				free(keyid); keyid = 0;
			}
			keyid = safestrdup(t,__FILE__,__LINE__);
			if( (appendline = (Lastchar(keyid) == '\\')) ){
				keyid[safestrlen(keyid)-1] = 0;
			}
		}
	}
	if( keyid ){
		if( Build_pc_names( names, order, keyid, hostname ) ){
			Add_line_list( list, keyid, Printcap_sep, 1, 0 );
		}
		free(keyid); keyid = 0;
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4) Dump_line_list( "Build_printcap_info- end", list );
#endif
	return;
}

/*
 * char *Select_pc_info(
 *   - returns the main name of the print queue
 * struct line_list *aliases  = list of names and aliases
 * struct line_list *info     = printcap infor
 * struct line_list *names    = entry names in the input list
 *                              alias=mainname
 * struct line_list *input    = printcap entries, starting with mainname
 *
 *  Select the printcap information and put it in the info list.
 *  Return the main name;
 */

char *Select_pc_info( const char *id,
	struct line_list *info,
	struct line_list *aliases,
	struct line_list *names,
	struct line_list *order,
	struct line_list *input,
	int depth, int wildcard )
{
	int start, end, i, c;
	char *s, *t, *found = 0, *allglob = 0;
	struct line_list l;

	Init_line_list(&l);
	DEBUG1("Select_pc_info: looking for '%s', depth %d", id, depth );
	if( depth > 5 ){
		Errorcode = JABORT;
		FATAL(LOG_ERR)"Select_pc_info: printcap tc recursion depth %d", depth );
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Select_pc_info- names", names );
	if(DEBUGL4)Dump_line_list("Select_pc_info- order", order );
	if(DEBUGL4)Dump_line_list("Select_pc_info- input", input );
#endif
	start = 0; end = 0;
	found = Find_str_value( names, id, Value_sep );
	if( !found && PC_filters_line_list.count ){
		Filterprintcap( &l, &PC_filters_line_list, id);
		Build_printcap_info( names, order, input, &l, &Host_IP );
		Free_line_list( &l );
#ifdef ORIGINAL_DEBUG//JY@1020
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter aliases", aliases );
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter info", info );
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter names", names );
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter input", input );
#endif
		found = Find_str_value( names, id, Value_sep );
	}
	/* do partial glob match  */
	c = 0;
	for( i = 0; !found && i < names->count; ++i ){
		s = names->list[i];
		if( (t = safestrpbrk(s, Value_sep)) ){
			c = *t; *t = 0;
			DEBUG1("Select_pc_info: wildcard trying '%s'", s );
			if( !safestrcmp(s, id ) ){
				found = t+1;
			}
			*t = c;
		}
	}
	if( !found && wildcard ){
		c = 0;
		for( i = 0; !found && i < names->count; ++i ){
			s = names->list[i];
			if( (t = safestrpbrk(s, Value_sep)) ){
				c = *t; *t = 0;
				DEBUG1("Select_pc_info: wildcard trying '%s'", s );
				if( !strcmp(s,"*") ){ 
					if( ISNULL(allglob) ){
						allglob = t+1;
					}
				} else if( !Globmatch( s, id ) ){
					found = t+1;
				}
				*t = c;
			}
		}
	}
	if( !found ){
		found = allglob;
	}
	if( found ){
		Find_pc_info( found, info, aliases, names, order, input, depth, 0 );
	}
	DEBUG1("Select_pc_info: returning '%s'", found );
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Select_pc_info- returning aliases", aliases );
	if(DEBUGL4)Dump_line_list("Select_pc_info- returning info", info );
#endif
	return( found );
}

void Find_pc_info( char *name,
	struct line_list *info,
	struct line_list *aliases,
	struct line_list *names,
	struct line_list *order,
	struct line_list *input,
	int depth, int wildcard )
{
	int start, end, i, j, c, start_tc, end_tc;
	char *s, *t, *u;
	struct line_list l, pc, tc;

	Init_line_list(&l); Init_line_list(&pc); Init_line_list(&tc);

	DEBUG1("Find_pc_info: found name '%s'", name );
	if( Find_first_key(input,name,Printcap_sep,&start)
		|| Find_last_key(input,name,Printcap_sep,&end) ){
		Errorcode = JABORT;
		FATAL(LOG_ERR)
			"Find_pc_info: name '%s' in names and not in input list",
			name );
	}
	DEBUG4("Find_pc_info: name '%s', start %d, end %d",
		name, start, end );
	for(; start <= end; ++start ){
		u = input->list[start];
		DEBUG4("Find_pc_info: line [%d]='%s'", start, u );
		if( u && *u ){
			Add_line_list( &pc, u, 0, 0, 0 );
		}
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Find_pc_info- entry lines", &l );
#endif
	for( start = 0; start < pc.count; ++ start ){
		u = pc.list[start];
		c = 0;
		if( (t = safestrpbrk(u,":")) ){
			Split(&l, t+1, ":", 1, Value_sep, 0, 1, 0,":");
		}
		if( aliases ){
			if( t ){
				c = *t; *t = 0;
				Split(aliases, u, Printcap_sep, 0, 0, 0, 0, 0,0);
				Remove_duplicates_line_list(aliases);
				*t = c;
			} else {
				Split(aliases, u, Printcap_sep, 0, 0, 0, 0, 0,0);
				Remove_duplicates_line_list(aliases);
			}
		}
		/* get the tc entries */
#ifdef ORIGINAL_DEBUG//JY@1020
		if(DEBUGL4)Dump_line_list("Find_pc_info- pc entry", &l );
#endif
		if( !Find_first_key(&l,"tc",Value_sep,&start_tc)
			&& !Find_last_key(&l,"tc",Value_sep,&end_tc) ){
			for( ; start_tc <= end_tc; ++start_tc ){
				if( (s = l.list[start_tc]) ){
					lowercase(s);
					DEBUG4("Find_pc_info: tc '%s'", s );
					if( (t = safestrchr( s, '=' )) ){
						Split(&tc,t+1,File_sep,0,0,0,1,0,0);
					}
					free( l.list[start_tc] );
					l.list[start_tc] = 0;
				}
			}
		}
#ifdef ORIGINAL_DEBUG//JY@1020
		if(DEBUGL4)Dump_line_list("Find_pc_info- tc", &tc );
#endif
		for( j = 0; j < tc.count; ++j ){
			s = tc.list[j];
			DEBUG4("Find_pc_info: tc entry '%s'", s );
			if( !Select_pc_info( s, info, 0, names, order, input, depth+1, wildcard ) ){
				FATAL(LOG_ERR)
				"Find_pc_info: tc entry '%s' not found", s);
			}
		}
		Free_line_list(&tc);
#ifdef ORIGINAL_DEBUG//JY@1020
		if(DEBUGL4)Dump_line_list("Find_pc_info - adding", &l );
#endif
		for( i = 0; i < l.count; ++i ){
			if( (t = l.list[i]) ){
				Add_line_list( info, t, Value_sep, 1, 1 );
			}
		}
		Free_line_list(&l);
	}
	Free_line_list(&pc);
}
#endif

/*
 * variable lists and initialization
 */
/***************************************************************************
 * Clear_var_list( struct pc_var_list *vars );
 *   Set the printcap variable value to 0 or null;
 ***************************************************************************/

void Clear_var_list( struct keywords *v, int setv )
{
	char *s;
	void *p;
	struct keywords *vars;
    for( vars = v; vars->keyword; ++vars ){
		if( !(p = vars->variable) ) continue;
        switch( vars->type ){
            case STRING_K:
				s = ((char **)p)[0];
				if(s)free(s);
				((char **)p)[0] = 0;
				break;
            case INTEGER_K:
            case FLAG_K: ((int *)p)[0] = 0; break;
            default: break;
        }
		if( setv && vars->default_value ){
			Config_value_conversion( vars, vars->default_value );
		}
    }
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL5)Dump_parms("Clear_var_list: after",v );
#endif
}

/***************************************************************************
 * Set_var_list( struct keywords *vars, struct line_list *values );
 *  for each name in  keywords
 *    find entry in values
 ***************************************************************************/

void Set_var_list( struct keywords *keys, struct line_list *values )
{
	struct keywords *vars;
	const char *s;

	for( vars = keys; vars->keyword; ++vars ){
		if( (s = Find_exists_value( values, vars->keyword, Value_sep )) ){
			Config_value_conversion( vars, s );
		}
	}
}


/***************************************************************************
 * int Check_str_keyword( char *name, int *value )
 * - check a string for a simple keyword name
 ***************************************************************************/

#define FIXV(S,V) { S, N_(S), INTEGER_K, (void *)0, V, 0,0 }
 static struct keywords simple_words[] = {
 FIXV( "all", 1 ), FIXV( "yes", 1 ), FIXV( "allow", 1 ), FIXV( "true", 1 ),
 FIXV( "no", 0 ), FIXV( "deny", 0 ), FIXV( "false", 0 ),
 FIXV( "none", 0 ),
{0,0,0,0,0,0,0}
 };

int Check_str_keyword( const char *name, int *value )
{
	struct keywords *keys;
	for( keys = simple_words; keys->keyword; ++keys ){
		if( !safestrcasecmp( name, keys->keyword ) ){
			*value = keys->maxval;
			return( 1 );
		}
	}
	return( 0 );
}

/***************************************************************************
 * void Config_value_conversion( struct keyword *key, char *value )
 *  set the value of the variable as required
 ***************************************************************************/
#if defined(JYWENG20031104Config_value_conversion)
void Config_value_conversion( struct keywords *key, const char *s )
{
	int i = 0, c = 0, value = 0;
	char *end;		/* end of conversion */
	void *p;

	DEBUG5("Config_value_conversion: '%s'='%s'", key->keyword, s );
	if( !(p = key->variable) ) return;
	while(s && isspace(cval(s)) ) ++s;
	/*
	 * we have null str "", "@", "#val", or "=val"
	 * FLAG              1   0     val!=0     val!=0
     * INT               1   0     val        val
	 */
	switch( key->type ){
	case FLAG_K:
	case INTEGER_K:
		DEBUG5("Config_value_conversion: int '%s'", s );
		i = 1;
		if( s && (c=cval(s)) ){
			if( c == '@' ){
				i = 0;
			} else {
				/* get rid of leading junk */
				while( safestrchr(Value_sep,c) ){
					++s;
					c = cval(s);
				}
				if( Check_str_keyword( s, &value ) ){
					i = value;
				} else {
					end = 0;
					i = strtol( s, &end, 0 );
					if( end == 0 ){
						i = 1;
					}
				}
			}
		}
		((int *)p)[0] = i;
		DEBUG5("Config_value_conversion: setting '%d'", i );
		break;
	case STRING_K:
		end = ((char **)p)[0];
		DEBUG5("Config_value_conversion:  current value '%s'", end );
		if( end ) free( end );
		((char **)p)[0] = 0;
		while(s && (c=cval(s)) && safestrchr(Value_sep,c) ) ++s;
		end = 0;
		if( s && *s ){
			end = safestrdup(s,__FILE__,__LINE__);
			trunc_str(end);
		}
		((char **)p)[0] = end;
		DEBUG5("Config_value_conversion: setting '%s'", end );
		break;
	default:
		break;
	}
}
#endif

 static struct keywords Keyletter[] = {
	{ "P", 0, STRING_K, &Printer_DYN, 0,0,0 },
	{ "Q", 0, STRING_K, &Queue_name_DYN, 0,0,0 },
	{ "h", 0, STRING_K, &ShortHost_FQDN, 0,0,0 },
	{ "H", 0, STRING_K, &FQDNHost_FQDN, 0,0,0 },
	{ "a", 0, STRING_K, &Architecture_DYN, 0,0,0 },
	{ "R", 0, STRING_K, &RemotePrinter_DYN, 0,0,0 },
	{ "M", 0, STRING_K, &RemoteHost_DYN, 0,0,0 },
	{ "D", 0, STRING_K, &Current_date_DYN, 0,0,0 },
	{ 0,0,0,0,0,0,0 }
};

void Expand_percent( char **var )
{
	struct keywords *key;
	char *str, *s, *t, *u, **v = var;
	int c, len;

	if( v == 0 || (str = *v) == 0 || !safestrpbrk(str,"%") ){
		return;
	}
	DEBUG4("Expand_percent: starting '%s'", str );
	if( Current_date_DYN == 0 ){
		Set_DYN(&Current_date_DYN, Time_str(0,0) );
		if( (s = safestrrchr(Current_date_DYN,'-')) ){
			*s = 0;
		}
	}
	s = str;
	while( (s = safestrpbrk(s,"%")) ){
		t = 0;
		if( (c = cval(s+1)) && isalpha( c ) ){
			for( key = Keyletter; t == 0 && key->keyword; ++key ){
				if( (c == key->keyword[0]) ){
					t = *(char **)key->variable;
					break;
				}
			}
		}
		if( t ){
			*s = 0;
			s += 2;
			len = safestrlen(str) + safestrlen(t);
			u = str;
			str = safestrdup3(str,t,s,__FILE__,__LINE__);
			if(u) free(u); u = 0;
			s = str+len;
		} else {
			++s;
		}
	}
	*v = str;
	DEBUG4("Expand_percent: ending '%s'", str );
}

/***************************************************************************
 * Expand_vars:
 *  expand the values of a selected list of strings
 *  These should be from _DYN
 ***************************************************************************/
void Expand_vars( void )
{
	void *p;
	struct keywords *var;

#ifdef REMOVE
	/* check to see if you need to expand */
	for( var = Pc_var_list; var->keyword; ++var ){
		if( var->type == STRING_K && (p = var->variable) ){
			Expand_percent(p);
		}
	}
#endif
}


#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * Expand_hash_values:
 *  expand the values of a hash
 ***************************************************************************/
void Expand_hash_values( struct line_list *hash )
{
	char *u, *s;
	int i;

	/* check to see if you need to expand */
	for( i = 0; i < hash->count; ++i ){
		s = hash->list[i];
		if( safestrchr( s, '%' ) ){
			u = safestrdup(s,__FILE__,__LINE__);
			Expand_percent( &u );
			if( s ) free(s); s = 0;
			hash->list[i] = u;
		}
	}
}
#endif

/*
 * Set a _DYN variable
 */

char *Set_DYN( char **v, const char *s )
{
	char *t = *v;
	*v = 0;
	if( s && *s ) *v = safestrdup(s,__FILE__,__LINE__);
	if( t ) free(t);
	return( *v );
}

/*
 * Clear the total configuration information
 *  - we simply remove all dynmically allocated values
 */
void Clear_config( void )
{
	struct line_list **l;

#ifdef REMOVE
	DEBUGF(DDB1)("Clear_config: freeing everything");
	Remove_tempfiles();
	Clear_tempfile_list();
    Clear_var_list( Pc_var_list, 1 );
    Clear_var_list( DYN_var_list, 1 );
	for( l = Allocs; *l; ++l ) Free_line_list(*l);
#endif
}

char *Find_default_var_value( void *v )
{
#ifdef REMOVE
	struct keywords *k;
	char *s;
	for( k = Pc_var_list; (s = k->keyword); ++k ){
		if( k->type == STRING_K && k->variable == v ){
			s = k->default_value;
			if( s && cval(s) == '=' ) ++s;
			DEBUG1("Find_default_var_value: found 0x%lx key '%s' '%s'",
				(long)v, k->keyword, s );
			return( s );
		}
	}
#endif
	return(0);
}

/***************************************************************************
 * void Get_config( char *names )
 *  gets the configuration information from a list of files
 ***************************************************************************/

void Get_config( int required, char *path )
{
#ifdef REMOVE
	DEBUG1("Get_config: required '%d', '%s'", required, path );
	/* void Read_file_list( int required, struct line_list *model, char *str,
	 *  const char *linesep, int sort, const char *keysep, int uniq, int trim,
	 *  int marker, int doinclude, int nocomment, int depth, int maxdepth )
	 */
	Read_file_list( /*required*/required,
		/*model*/ &Config_line_list,/*str*/ path,
		/*linesep*/Line_ends, /*sort*/1, /*keysep*/Value_sep,/*uniq*/1,
		/*trim*/':',/*marker*/0,/*doinclude*/1,/*nocomment*/1,
		/*depth*/0,/*maxdepth*/4 ); 

	Set_var_list( Pc_var_list, &Config_line_list);
	Get_local_host();
	Expand_vars();
#endif
}

/***************************************************************************
 * void Reset_config( char *names )
 *  Resets the configuration and printcap information
 ***************************************************************************/

void Reset_config( void )
{
#ifdef REMOVE
	DEBUG1("Reset_config: starting");
	Clear_var_list( Pc_var_list, 1 );
	Free_line_list( &PC_entry_line_list );
	Free_line_list( &PC_alias_line_list );
	Set_var_list( Pc_var_list, &Config_line_list);
	Expand_vars();
#endif
}

void close_on_exec( int fd )
{
    for( ;fd <= Max_fd+10; fd++ ){
        (void) close( fd);
    }
}

void Setup_env_for_process( struct line_list *env, struct job *job )
{
	struct line_list env_names;
	struct passwd *pw;
	char *s, *t, *u, *name;
	int i;

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Setup_env_for_process: check point 1\n");
fclose(aaaaaa);
#endif
	Init_line_list(&env_names);
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Setup_env_for_process: check point 1.1\n");
fclose(aaaaaa);
#endif

#if 1
{
char a1[8], a2[8], a3[8], a4[8], a5[16];
int b1=0, b2=0;
strcpy(a1, "root");
strcpy(a2, "x");
strcpy(a3, "root");
strcpy(a4, "/root");
strcpy(a5, "/bin/bash");
pw->pw_name=a1;
pw->pw_passwd=a2;
pw->pw_gecos=a3;
pw->pw_dir=a4;
pw->pw_shell=a5;
pw->pw_uid=b1;
pw->pw_gid=b2;
}	
#else
	if( (pw = getpwuid( getuid())) == 0 ){
		LOGERR_DIE(LOG_INFO) "setup_envp: getpwuid(%d) failed", getuid());
	}
#endif

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Setup_env_for_process: check point 2\n");
fclose(aaaaaa);
#endif
	Set_str_value(env,"PRINTER",Printer_DYN);
	Set_str_value(env,"USER",pw->pw_name);
	Set_str_value(env,"LOGNAME",pw->pw_name);
	Set_str_value(env,"HOME",pw->pw_dir);
	Set_str_value(env,"LOGDIR",pw->pw_dir);
	Set_str_value(env,"PATH",Filter_path_DYN);
	Set_str_value(env,"LD_LIBRARY_PATH",Filter_ld_path_DYN);
	Set_str_value(env,"SHELL",Shell_DYN);
	Set_str_value(env,"IFS"," \t");

	s = getenv( "TZ" );  Set_str_value(env,"TZ",s);
	Set_str_value(env,"SPOOL_DIR", Spool_dir_DYN );
	if( PC_entry_line_list.count ){
		t = Join_line_list_with_sep(&PC_alias_line_list,"|");
		s = Join_line_list_with_sep(&PC_entry_line_list,"\n :");
		u = safestrdup4(t,(s?"\n :":0),s,"\n",__FILE__,__LINE__);
		Expand_percent( &u );
		Set_str_value(env, "PRINTCAP_ENTRY",u);
		if(s) free(s); s = 0;
		if(t) free(t); t = 0;
		if(u) free(u); u = 0;
	}
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Setup_env_for_process: check point 3\n");
fclose(aaaaaa);
#endif
	if( job ){
		if( !(s = Find_str_value(&job->info,CF_OUT_IMAGE,Value_sep)) ){
			s = Find_str_value(&job->info,OPENNAME,Value_sep);
			if( !s ) s = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
			s = Get_file_image( s, 0 );
			Set_str_value(&job->info, CF_OUT_IMAGE, s );
			if( s ) free(s); s = 0;
			s = Find_str_value(&job->info,CF_OUT_IMAGE,Value_sep);
		}
		Set_str_value(env, "CONTROL", s );
	}

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Setup_env_for_process: check point 4\n");
fclose(aaaaaa);
#endif
	if( Pass_env_DYN ){
		Free_line_list(&env_names);
		Split(&env_names,Pass_env_DYN,File_sep,1,Value_sep,1,1,0,0);
		for( i = 0; i < env_names.count; ++i ){
			name = env_names.list[i];
			if( (s = getenv( name )) ){
				Set_str_value( env, name, s);
			}
		}
	}
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Setup_env_for_process: check point 5\n");
fclose(aaaaaa);
#endif
	Free_line_list(&env_names);
	Check_max(env,1);
	env->list[env->count] = 0;
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL1)Dump_line_list("Setup_env_for_process", env );
#endif
}

/***************************************************************************
 * void Getprintcap_pathlist( char *path )
 * Read printcap information from a (semi)colon or comma separated set of files
 *   or filter specifications
 *   1. break path up into set of path names
 *   2. read the printcap information into memory
 *   3. parse the printcap informormation
 ***************************************************************************/

void Getprintcap_pathlist( int required,
	struct line_list *raw, struct line_list *filters,
	char *path )
{
	struct line_list l;
	int i, c;

	Init_line_list(&l);
	DEBUG2("Getprintcap_pathlist: processing '%s'", path );
	Split(&l,path,Strict_file_sep,0,0,0,1,0,0);
	for( i = 0; i < l.count; ++i ){
		path = l.list[i];
		c = path[0];
		switch( c ){
		case '|':
			DEBUG2("Getprintcap_pathlist: filter '%s'", path );
			if( filters ) Add_line_list( filters, path, 0, 0, 0 );
			break;
		case '/':
			DEBUG2("Getprintcap_pathlist: file '%s'", path );
			/*
			void Read_file_list( int required, struct line_list *model, char *str,
				const char *linesep, int sort, const char *keysep, int uniq, int trim,
				int marker, int doinclude, int nocomment, int depth, int maxdepth )
			*/
			Read_file_list(/*required*/required,/*model*/raw,/*str*/path,
				/*linesep*/Line_ends,/*sort*/0,/*keysep*/0,/*uniq*/0,/*trim*/1,
				/*marker*/0,/*doinclude*/1,/*nocomment*/1,/*depth*/0,/*maxdepth*/4);
			break;
		default:
			FATAL(LOG_ERR)
				"Getprintcap_pathlist: entry not filter or absolute pathname '%s'",
				path );
		}
	}
	Free_line_list(&l);

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4){
		Dump_line_list( "Getprintcap_pathlist - filters", filters  );
		Dump_line_list( "Getprintcap_pathlist - info", raw  );
	}
#endif
}

/***************************************************************************
 * int Filterprintcap( struct line_list *raw, *filters, char *str )
 *  - for each entry in the filters list do the following:
 *    - make the filter, sending it the 'name' for access
 *    - read from the filter until EOF, adding it to the raw list
 *    - kill off the filter process
 ***************************************************************************/

#if defined(JYWENG20031104Filterprintcap)
void Filterprintcap( struct line_list *raw, struct line_list *filters,
	const char *str )
{
	int count, n, intempfd, outtempfd;
	char *filter;

	if( filters->count > 0 ){
		intempfd = Make_temp_fd( 0 );
		outtempfd = Make_temp_fd( 0 );
		if( Write_fd_str( intempfd, str) < 0
			|| Write_fd_str( intempfd,"\n") < 0 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) "Filterprintcap: Write_fd_str failed");
		}
		for( count = 0; count < filters->count; ++count ){
			filter = filters->list[count];
			DEBUG2("Filterprintcap: filter '%s'", filter );
			if( lseek(intempfd,0,SEEK_SET) == -1 ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_ERR) "Filterprintcap: lseek intempfd failed");
			}
			n = Filter_file(intempfd, outtempfd, "PC_FILTER",
				filter, Filter_options_DYN, 0,
				0, 0 );
			if( n ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_ERR) "Filterprintcap: filter '%s' failed", filter);
			}
		}
		if( lseek(outtempfd,0,SEEK_SET) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) "Filterprintcap: lseek outtempfd failed");
		}
		Read_fd_and_split( raw,outtempfd,Line_ends,0,0,0,1,1);
		/* do not worry if these fail */
		close( intempfd); intempfd = -1;
		close( outtempfd); outtempfd = -1;
	}
}
#endif

#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * int In_group( char* *group, char *user );
 *  returns 1 on failure, 0 on success
 *  scan group for user name
 * Note: we first check for the group.  If there is none, we check for
 *  wildcard (*) in group name, and then scan only if we need to
 ***************************************************************************/

int In_group( char *group, char *user )
{
	struct group *grent;
	struct passwd *pwent;
	char **members;
	int result = 1;

	DEBUGF(DDB3)("In_group: checking '%s' for membership in group '%s'", user, group);
	if( group == 0 || user == 0 ){
		return( result );
	}
	/* first try getgrnam, see if it is a group */
	pwent = getpwnam(user);
	if( (grent = getgrnam( group )) ){
		DEBUGF(DDB3)("In_group: group id: %d\n", grent->gr_gid);
		if( pwent && ((int)pwent->pw_gid == (int)grent->gr_gid) ){
			DEBUGF(DDB3)("In_group: user default group id: %d\n", pwent->pw_gid);
			result = 0;
		} else for( members = grent->gr_mem; result && *members; ++members ){
			DEBUGF(DDB3)("In_group: member '%s'", *members);
			result = (safestrcmp( user, *members ) != 0);
		}
	}
	if( result && safestrchr( group, '*') ){
		/* wildcard in group name, scan through all groups */
		setgrent();
		while( result && (grent = getgrent()) ){
			DEBUGF(DDB3)("In_group: group name '%s'", grent->gr_name);
			/* now do match against group */
			if( Globmatch( group, grent->gr_name ) == 0 ){
				if( pwent && ((int)pwent->pw_gid == (int)grent->gr_gid) ){
					DEBUGF(DDB3)("In_group: user default group id: %d\n",
					pwent->pw_gid);
					result = 0;
				} else {
					DEBUGF(DDB3)("In_group: found '%s'", grent->gr_name);
					for( members = grent->gr_mem; result && *members; ++members ){
						DEBUGF(DDB3)("In_group: member '%s'", *members);
						result = (safestrcmp( user, *members ) != 0);
					}
				}
			}
		}
		endgrent();
	}
	if( result && group[0] == '@' ) {	/* look up user in netgroup */
#ifdef HAVE_INNETGR
		if( !innetgr( group+1, NULL, user, NULL ) ) {
			DEBUGF(DDB3)( "In_group: user %s NOT in netgroup %s", user, group+1 );
		} else {
			DEBUGF(DDB3)( "In_group: user %s in netgroup %s", user, group+1 );
			result = 0;
		}
#else
		DEBUGF(DDB3)( "In_group: no innetgr() call, netgroups not permitted" );
#endif
	}
	DEBUGF(DDB3)("In_group: result: %d", result );
	return( result );
}

int Check_for_rg_group( char *user )
{
	int i, match = 0;
	struct line_list l;
	char *s;

	Init_line_list(&l);

	s = RestrictToGroupMembers_DYN;
	DEBUG3("Check_for_rg_group: name '%s', restricted_group '%s'",
		user, s );
	if( s ){
		match = 1;
		Split(&l,s,List_sep,0,0,0,0,0,0);
		for( i = 0; match && i < l.count; ++i ){
			s = l.list[i];
			match = In_group( s, user );
		}
	}
	Free_line_list(&l);
	DEBUG3("Check_for_rg_group: result: %d", match );
	return( match );
}
#endif


/***************************************************************************
 * Make_temp_fd( char *name, int namelen )
 * 1. we can call this repeatedly,  and it will make
 *    different temporary files.
 * 2. we NEVER modify the temporary file name - up to the first '.'
 *    is the base - we keep adding suffixes as needed.
 * 3. Remove_files uses the tempfile information to find and delete temp
 *    files so be careful.
 ***************************************************************************/


char *Init_tempfile( void )
{
	char *dir = 0, *s;
	struct stat statb;

	if( Is_server ){
		if( dir == 0 )  dir = Spool_dir_DYN;
		if( dir == 0 )  dir = Server_tmp_dir_DYN;
	} else {
		dir = getenv( "LPR_TMP" );
		if( dir == 0 ) dir = Default_tmp_dir_DYN;
	}
	/* remove trailing / */
	if( (s = safestrrchr(dir,'/')) && s[1] == 0 ) *s = 0;
	if( dir == 0 || stat( dir, &statb ) != 0
		|| !S_ISDIR(statb.st_mode) ){
		FATAL(LOG_ERR) "Init_tempfile: bad tempdir '%s'", dir );
	}
	DEBUG3("Init_tempfile: temp file '%s'", dir );
	return( dir );
}

int Make_temp_fd_in_dir( char **temppath, char *dir )
{
	int tempfd;
	struct stat statb;
	char pathname[MAXPATHLEN];

	SNPRINTF(pathname,sizeof(pathname))"%s/temp%02dXXXXXX",dir,Tempfiles.count );
	tempfd = mkstemp( pathname );
	if( tempfd == -1 ){
		Errorcode = JFAIL;
		FATAL(LOG_INFO)"Make_temp_fd_in_dir: cannot create tempfile '%s'", pathname );
	}
	Add_line_list(&Tempfiles,pathname,0,0,0);
	if( temppath ){
		*temppath = Tempfiles.list[Tempfiles.count-1];
	}
	if( fchmod(tempfd,(Is_server?Spool_file_perms_DYN:0) | 0600 ) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Make_temp_fd_in_dir: chmod '%s' to 0%o failed ",
			pathname, Spool_file_perms_DYN );
	}
	if( stat(pathname,&statb) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Make_temp_fd_in_dir: stat '%s' failed ", pathname );
	}
	DEBUG1("Make_temp_fd_in_dir: fd %d, name '%s'", tempfd, pathname );
	return( tempfd );
}

int Make_temp_fd( char **temppath )
{
	return( Make_temp_fd_in_dir( temppath, Init_tempfile()) );
}

/***************************************************************************
 * Clear_tempfile_list()
 *  - clear the list of tempfiles created for this job
 *  - this is done by a child process
 ***************************************************************************/
void Clear_tempfile_list(void)
{
	Free_line_list(&Tempfiles);
}

/***************************************************************************
 * Unlink_tempfiles()
 *  - remove the tempfiles created for this job
 ***************************************************************************/

void Unlink_tempfiles(void)
{
	int i;
	for( i = 0; i < Tempfiles.count; ++i ){
		DEBUG4("Unlink_tempfiles: unlinking '%s'", Tempfiles.list[i] );
		unlink(Tempfiles.list[i]);
	}
	Free_line_list(&Tempfiles);
}


/***************************************************************************
 * Remove_tempfiles()
 *  - remove the tempfiles created for this job
 ***************************************************************************/

void Remove_tempfiles(void)
{
	Unlink_tempfiles();
}

/***************************************************************************
 * Split_cmd_line
 *   if we have xx "yy zz" we split this as
 *  xx
 *  yy zz
 ***************************************************************************/

void Split_cmd_line( struct line_list *l, char *line )
{
	char *s = line, *t;
	int c;

	DEBUG1("Split_cmd_line: line '%s'", line );
	while( s && cval(s) ){
		while( strchr(Whitespace,cval(s)) ) ++s;
		/* ok, we have skipped the whitespace */
		if( (c = cval(s)) ){
			if( c == '"' || c == '\'' ){
				/* we now have hit a quoted string */
				++s;
				t = strchr(s,c);
			} else if( !(t = strpbrk(s, Whitespace)) ){
				t = s+safestrlen(s);
			}
			if( t ){
				c = cval(t);
				*t = 0;
				Add_line_list(l, s, 0, 0, 0);
				*t = c;
				if( c ) ++t;
			}
			s = t;
		}
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL1){ Dump_line_list("Split_cmd_line", l ); }
#endif
}

/***************************************************************************
 * Make_passthrough
 *   
 * int Make_passthrough   - returns PID of process
 *  char *line            - command line
 *  char *flags,          - additional flags
 *  struct line_list *passfd, - file descriptors
 *  struct job *job       - job with for option expansion
 *  struct line_list *env_init  - environment
 ***************************************************************************/

int Make_passthrough( char *line, char *flags, struct line_list *passfd,
	struct job *job, struct line_list *env_init )
{
	int c, i, pid = -1, noopts, root, newfd, fd;
	struct line_list cmd;
	struct line_list env;
	char error[SMALLBUFFER];
	char *s;

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 1\n");
fclose(aaaaaa);
#endif

	DEBUG1("Make_passthrough: cmd '%s', flags '%s'", line, flags );
	if( job ){
		s = Find_str_value( &job->info,QUEUENAME, Value_sep );
		if( !ISNULL(s) ){
			Set_DYN(&Queue_name_DYN,s );
		}
	}
	Init_line_list(&env);
	if( env_init ){
		Merge_line_list(&env,env_init,Value_sep,1,1);
	}
	Init_line_list(&cmd);

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 2\n");
fclose(aaaaaa);
#endif
	while( isspace(cval(line)) ) ++line;
	if( cval(line) == '|' ) ++line;
	noopts = root = 0;
	while( cval(line) ){
		while( isspace(cval(line)) ) ++line;
		if( !safestrncmp(line,"$-", 2)
			||	!safestrncmp(line,"-$", 2) ){
			noopts = 1;
			line += 2;
		} else if( !safestrncasecmp(line,"root", 4) ){
			/* only set to root if it is the LPD server */
			root = Is_server;
			line += 4;
		} else {
			break;
		}
	}

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 3\n");
fclose(aaaaaa);
#endif
	c = cval(line);
#if defined(JYWENG20031104Fix_dollars)
	if( strpbrk(line, "<>|;") || c == '(' ){
		Add_line_list( &cmd, Shell_DYN, 0, 0, 0 );
		Add_line_list( &cmd, "-c", 0, 0, 0 );
		Add_line_list( &cmd, line, 0, 0, 0 );
		if( c != '(' ){
			s = cmd.list[cmd.count-1];
			s = safestrdup3("( ",s," )",__FILE__,__LINE__);
			if( cmd.list[cmd.count-1] ) free( cmd.list[cmd.count-1] );
			cmd.list[cmd.count-1] = s;
		}
		Fix_dollars(&cmd, job, 1, flags);
	} else {
		Split_cmd_line(&cmd, line);
		if( !noopts ){
			Split(&cmd, flags, Whitespace, 0,0, 0, 0, 0,0);
		}
		Fix_dollars(&cmd, job, 0, flags);
	}
#endif

	Check_max(&cmd,1);
	cmd.list[cmd.count] = 0;

	Setup_env_for_process(&env, job);

	if(DEBUGL1){
#ifdef ORIGINAL_DEBUG//JY@1020
		Dump_line_list("Make_passthrough - cmd",&cmd );
		LOGDEBUG("Make_passthrough: fd count %d, root %d", passfd->count, root );
#endif
		for( i = 0 ; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			LOGDEBUG("  [%d]=%d",i,fd);
		}
#ifdef ORIGINAL_DEBUG//JY@1020
		Dump_line_list("Make_passthrough - env",&env );
#endif
	}
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 4.5\n");
fclose(aaaaaa);
#endif

	c = cmd.list[0][0];
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5\n");
fclose(aaaaaa);
#endif
	if( c != '/' ){
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.1\n");
fclose(aaaaaa);
#endif
		FATAL(LOG_ERR)"Make_passthrough: bad filter - not absolute path name'%s'",
			cmd.list[0] );
	}
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.2\n");
fclose(aaaaaa);
#endif
	if( (pid = dofork(0)) == -1 ){
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.3\n");
fclose(aaaaaa);
#endif
		LOGERR_DIE(LOG_ERR)"Make_passthrough: fork failed");
	} else if( pid == 0 ){
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.4\n");
fclose(aaaaaa);
#endif
		for( i = 0; i < passfd->count; ++i ){
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.5\n");
fclose(aaaaaa);
#endif
			fd = Cast_ptr_to_int(passfd->list[i]);
			if( fd < i  ){
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.6\n");
fclose(aaaaaa);
#endif
				/* we have fd 3 -> 4, but 3 gets wiped out */
				do{
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.7\n");
fclose(aaaaaa);
#endif
					newfd = dup(fd);
					Max_open(newfd);
					if( newfd < 0 ){
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.8\n");
fclose(aaaaaa);
#endif
						Errorcode = JABORT;
						LOGERR_DIE(LOG_INFO)"Make_passthrough: dup failed");
					}
					DEBUG4("Make_passthrough: fd [%d] = %d, dup2 -> %d",
						i, fd, newfd );
					passfd->list[i] = Cast_int_to_voidstar(newfd);
				} while( newfd < i );
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 5.9\n");
fclose(aaaaaa);
#endif
			}
		}
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 6\n");
fclose(aaaaaa);
#endif
		if(DEBUGL4){
			LOGDEBUG("Make_passthrough: after fixing fd, count %d", passfd->count );
			for( i = 0 ; i < passfd->count; ++i ){
				fd = Cast_ptr_to_int(passfd->list[i]);
				LOGDEBUG("  [%d]=%d",i,fd);
			}
		}
		/* set up full perms for filter */ 
		if( Is_server ){
			if( root ){
				if( UID_root ) To_euid_root();
			} else {
				Full_daemon_perms();
			}
		} else {
			Full_user_perms();
		}
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Make_passthrough: check point 7\n");
fclose(aaaaaa);
#endif
		for( i = 0; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			if( dup2(fd,i) == -1 ){
				SNPRINTF(error,sizeof(error))
					"Make_passthrough: pid %d, dup2(%d,%d) failed", getpid(), fd, i );
				Write_fd_str(2,error);
				exit(JFAIL);
			}
		}
		close_on_exec(passfd->count);
		execve(cmd.list[0],cmd.list,env.list);
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF(error,sizeof(error))
			"Make_passthrough: pid %d, execve '%s' failed - '%s'\n", getpid(),
			cmd.list[0], Errormsg(errno) );
#endif
		Write_fd_str(2,error);
		exit(JABORT);
	}
	passfd->count = 0;
	Free_line_list(passfd);
	Free_line_list(&env);
	Free_line_list(&cmd);
	return( pid );
}

/*
 * Filter_file:  we filter a file through this program
 *  input_fd = input file descriptor.  if -1, then we make it /dev/null
 *  tempfile = name of tempfile for output
 *  error_header = header used for error messages from filter
 *  pgm      = program
 *  filter_options = options for filter
 *  job      = job we are doing the work for
 *  env      = environment options we want to pass
 * RETURN:
 *   exit status of the filter,  adjusted to be in the JXXX status
 *   if it exits with error status, we get JSIGNAL
 */

int Filter_file( int input_fd, int output_fd, char *error_header,
	char *pgm, char * filter_options, struct job *job,
	struct line_list *env, int verbose )
{
	int innull_fd, outnull_fd, pid, len, n;
	char *s;
	int of_error[2];
    plp_status_t status;
	struct line_list files;
	char buffer[SMALLBUFFER];

	Init_line_list( &files );

	of_error[0] = of_error[1] = -1;

	innull_fd = input_fd;
	if( innull_fd < 0 && (innull_fd = open("/dev/null", O_RDWR )) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Filter_file: open /dev/null failed");
	}
	Max_open(innull_fd);

	outnull_fd = output_fd;
	if( outnull_fd < 0 && (outnull_fd = open("/dev/null", O_RDWR )) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Filter_file: open /dev/null failed");
	}
	Max_open(outnull_fd);

	if( pipe( of_error ) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Filter_file: pipe() failed");
	}
	Max_open(of_error[0]); Max_open(of_error[1]);
	DEBUG3("Filter_file: fd of_error[%d,%d]", of_error[0], of_error[1] );

	Check_max(&files, 10 );
	files.list[files.count++] = Cast_int_to_voidstar(innull_fd);	/* stdin */
	files.list[files.count++] = Cast_int_to_voidstar(outnull_fd);	/* stdout */
	files.list[files.count++] = Cast_int_to_voidstar(of_error[1]);	/* stderr */
	if( (pid = Make_passthrough( pgm, filter_options, &files, job, env )) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Filter_file: could not create process '%s'", pgm);
	}
	files.count = 0;
	Free_line_list(&files);

	if( input_fd < 0 ) close(innull_fd); innull_fd = -1;
	if( output_fd < 0 ) close(outnull_fd); outnull_fd = -1;
	if( (close(of_error[1]) == -1 ) ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Filter_file: X8 close(%d) failed",
			of_error[1]);
	}
	of_error[1] = -1;
	buffer[0] = 0;
	len = 0;
	while( len < (int)sizeof(buffer)-1
		&& (n = read(of_error[0],buffer+len,sizeof(buffer)-len-1)) >0 ){
		buffer[n+len] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
#ifdef ORIGINAL_DEBUG//JY@1020
			SETSTATUS(job)"%s: %s", error_header, buffer );
#endif
			memmove(buffer,s,safestrlen(s)+1);
		}
		len = safestrlen(buffer);
	}
	if( buffer[0] ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SETSTATUS(job)"%s: %s", error_header, buffer );
#endif
	}
	if( (close(of_error[0]) == -1 ) ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Filter_file: X8 close(%d) failed",
			of_error[0]);
	}
	of_error[0] = -1;
	while( (n = plp_waitpid(pid,&status,0)) != pid ){
		int err = errno;
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUG1("Filter_file: waitpid(%d) returned %d, err '%s'",
			pid, n, Errormsg(err) );
#endif
		if( err == EINTR ) continue; 
		Errorcode = JABORT;
		LOGERR_DIE(LOG_ERR) "Filter_file: waitpid(%d) failed", pid);
	} 
	DEBUG1("Filter_file: pid %d, exit status '%s'", pid, Decode_status(&status) );
	n = 0;
	if( WIFSIGNALED(status) ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Filter_file: pgm '%s' died with signal %d, '%s'",
			pgm, n, Sigstr(n));
	}
	n = WEXITSTATUS(status);
	if( n > 0 && n < 32 ) n+=(JFAIL-1);
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUG1("Filter_file: final status '%s'", Server_status(n) );
	if( verbose ){
		SETSTATUS(job)"Filter_file: pgm '%s' exited with status '%s'", pgm, Server_status(n));
	}
#endif
	return( n );
}

#define UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define LOWER "abcdefghijklmnopqrstuvwxyz"
#define DIGIT "01234567890"
#define SAFE "-_."
#define LESS_SAFE SAFE "@/:()=,+-%"

char *Is_clean_name( char *s )
{
	int c;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( !(isalnum(c) || safestrchr( SAFE, c )) ) return( s );
		}
	}
	return( 0 );
}

void Clean_name( char *s )
{
	int c;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( !(isalnum(c) || safestrchr( SAFE, c )) ) *s = '_';
		}
	}
}

/*
 * Find a possible bad character in a line
 */

int Is_meta( int c )
{
	return( !( isspace(c) || isalnum( c )
		|| (Safe_chars_DYN && safestrchr(Safe_chars_DYN,c))
		|| safestrchr( LESS_SAFE, c ) ) );
}

char *Find_meta( char *s )
{
	int c = 0;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( Is_meta( c ) ) return( s );
		}
		s = 0;
	}
	return( s );
}

void Clean_meta( char *t )
{
	char *s = t;
	if( t ){
		while( (s = safestrchr(s,'\\')) ) *s = '/';
		s = t;
		for( s = t; (s = Find_meta( s )); ++s ){
			*s = '_';
		}
	}
}

#ifdef ORIGINAL_DEBUG//JY@1020
/**********************************************************************
 * Dump_parms( char *title, struct keywords *k )
 * - dump the list of keywords and variable values given by the
 *   entries in the array.
 **********************************************************************/

void Dump_parms( char *title, struct keywords *k )
{
	char *s;
	void *p;
	int v;

	if( title ) LOGDEBUG( "*** Current Values '%s' ***", title );
	for( ; k &&  k->keyword; ++k ){
		if( !(p = k->variable) ) continue;
		switch(k->type){
		case FLAG_K:
			v =	*(int *)(p);
			LOGDEBUG( "  %s FLAG %d", k->keyword, v);
			break;
		case INTEGER_K:
			v =	*(int *)(p);
			LOGDEBUG( "  %s# %d (0x%x, 0%o)", k->keyword,v,v,v);
			break;
		case STRING_K:
			s = *(char **)(p);
			if( s ){
				LOGDEBUG( "  %s= '%s'", k->keyword, s );
			} else {
				LOGDEBUG( "  %s= <NULL>", k->keyword );
			}
			break;
		default:
			LOGDEBUG( "  %s: UNKNOWN TYPE", k->keyword );
		}
	}
	if( title ) LOGDEBUG( "*** <END> ***");
}
#endif

#ifdef ORIGINAL_DEBUG//JY@1020
/**********************************************************************
 * Dump_parms( char *title, struct keywords *k )
 * - dump the list of keywords and variable values given by the
 *   entries in the array.
 **********************************************************************/

void Dump_default_parms( int fd, char *title, struct keywords *k )
{
	char *def, *key;
	char buffer[2*SMALLBUFFER];
	int n;

	if( title ){
		SNPRINTF(buffer,sizeof(buffer))"%s\n", title );
		Write_fd_str(fd, buffer);
	}
	for( ; k &&  k->keyword; ++k ){
		n = 0;
		key = k->keyword;
		def = k->default_value;
		switch(k->type){
		case FLAG_K:
			if( def ){
				if( cval(def) == '=' ) ++def;
				n = strtol(def,0,0);
			}
			SNPRINTF(buffer,sizeof(buffer))" :%s%s\n", key, n?"":"@");
			break;
		case INTEGER_K:
			if( def ){
				if( cval(def) == '=' ) ++def;
				n = strtol(def,0,0);
			}
			SNPRINTF(buffer,sizeof(buffer))" :%s=%d\n", key, n);
			break;
		case STRING_K:
			if( def ){
				if( cval(def) == '=' ) ++def;
			} else {
				def = "";
			}
			SNPRINTF(buffer,sizeof(buffer))" :%s=%s\n", key, def);
			break;
		default:
			SNPRINTF(buffer,sizeof(buffer))"# %s UNKNOWN\n", key);
		}
		Write_fd_str(fd, buffer);
	}
	Write_fd_str(fd, "\n");
}
#endif

/***************************************************************************
 *char *Fix_Z_opts( struct job *job )
 *
 * fix the -Z option value
 *  Remove_Z_DYN - remove these from the Z string
 *  Prefix_Z_DYN - put these at the start
 *  Append_Z_DYN - put these at the end
 *  Prefix_option_to_option - prefix options to start of option
 *     OS Z -> O and S to Z
 *     Z  S -> Z to S
 ***************************************************************************/

void Remove_sequential_separators( char *start )
{
	char *end;
	if( start == 0 || *start == 0 ) return;
	while( strchr( File_sep, *start) ){
		memmove(start,start+1,safestrlen(start+1)+1);
	}
	for( end = start + safestrlen(start)-1;
		*start && (end = strpbrk( end, File_sep )); ){
		*end-- = 0;
	}
	for( ; *start && (end = strpbrk(start+1,File_sep)); start = end ){
		if( start+1 == end ){
			memmove(start,start+1,safestrlen(start+1)+1);
			end = start;
		}
	}
}

#if defined(JYWENG20031104Fix_dollars)
void Fix_Z_opts( struct job *job )
{
	char *str, *s, *pattern, *start, *end;
	char buffer[16];
	struct line_list l;
	int i, c, n;

	Init_line_list(&l);
	str = Find_str_value( &job->info,"Z", Value_sep );
	DEBUG4("Fix_Z_opts: initially '%s', remove '%s', append '%s', prefix '%s'",
		str, Remove_Z_DYN, Append_Z_DYN, Prefix_Z_DYN );
	DEBUG4("Fix_Z_opts: prefix_options '%s'", Prefix_option_to_option_DYN );
	if( Prefix_option_to_option_DYN ){
		s = Prefix_option_to_option_DYN;
		while( s && *s ){
			if( !isalpha(cval(s)) ){
				memmove(s,s+1,safestrlen(s+1)+1);
			} else {
				++s;
			}
		}
		s = Prefix_option_to_option_DYN;
		/* now we have the fixed value */
		DEBUG4("Fix_Z_opts: prefix_options fixed '%s'", s);
		n = safestrlen(s);
		if( n < 2 ){
			FATAL(LOG_ERR) "Fix_Z_opts: not enough letters '%s'", s );
		}
		/* find the starting values */
		str = 0;
		buffer[1] = 0;
		for( i = 0; i < n-1; ++i ){
			buffer[0] = s[i];
			if( (start = Find_str_value(&job->info,buffer,Value_sep)) ){
				str= safeextend2(str,start, __FILE__,__LINE__);
				Set_str_value(&job->info,buffer,0);
			}
		}
		/* do we need to prefix it? */
		if( str ){
			buffer[0] = s[i];
			start = Find_str_value(&job->info,buffer,Value_sep);
				/* put at start */
			start= safestrdup3(str,(start?",":""),start,
				__FILE__,__LINE__);
			Set_str_value(&job->info, buffer, start );
			if( start ) free(start); start = 0;
		}
		if( str ) free(str); str = 0;
	}
	str = Find_str_value( &job->info,"Z", Value_sep );
	DEBUG4("Fix_Z_opts: after Prefix_option_to_option '%s'", str );
	if( Remove_Z_DYN && str ){
		/* remove the various options - split on commas */
		Split(&l, Remove_Z_DYN, ",", 0, 0, 0, 0, 0,0);
		for( i = 0; i < l.count; ++i ){
			pattern = l.list[i];
			DEBUG4("Fix_Z_opts: REMOVE pattern '%s'", pattern );
			for( start = str; start && *start; start = end ){
				c = 0;
				if( !(end = strpbrk(start,",")) ){
					end = start+safestrlen(start);
				}
				c = *end;
				*end = 0;
				/* now we have the option */
				DEBUG4("Fix_Z_opts: str '%s'", start );
				if( !Globmatch( pattern, start) ){
					/* move the values up in the string, end -> ',' */
					if( c ){
						memmove( start,end+1, safestrlen(end+1)+1);
					} else {
						*start = 0;
					}
					end = start;
				} else {
					*end = c;
					if( c ) ++end;
				}
			}
		}
		Free_line_list(&l);
	}
	DEBUG4("Fix_Z_opts: after remove '%s'", str );
	if( Append_Z_DYN && *Append_Z_DYN ){
		s = safestrdup3(str,",",Append_Z_DYN,__FILE__,__LINE__);
		Set_str_value(&job->info,"Z",s);
		str = Find_str_value(&job->info,"Z",Value_sep);
		if(s) free(s); s = 0;
	}
	DEBUG4("Fix_Z_opts: after append '%s'", str );
	if( Prefix_Z_DYN && *Prefix_Z_DYN ){
		s = safestrdup3(Prefix_Z_DYN,",",str,__FILE__,__LINE__);
		Set_str_value(&job->info,"Z",s);
		str = Find_str_value(&job->info,"Z",Value_sep);
		if(s) free(s); s = 0;
	}
	DEBUG4("Fix_Z_opts: after Prefix_Z '%s'", str );
	for( s = safestrchr(str,','); s; s = strchr(s,',') ){
		if( cval(s+1) == ',' ){
			memmove(s,s+1,safestrlen(s+1)+1);
		} else {
			++s;
		}
	}
	if( str ){
		if( cval(str) == ',' ){
			memmove(str,str+1,safestrlen(str+1)+1);
		}
		if( (n = safestrlen(str)) && cval(str+n-1) == ',' ) str[n-1] = 0;
	}
	DEBUG4("Fix_Z_opts: final Z '%s'", str );
	Free_line_list(&l);
}
#endif

#if defined(JYWENG20031104Fix_dollars)
/***************************************************************************
 * void Fix_dollars( struct line_list *l, struct job *job,
 *   int nosplit, char *flags )
 * Note: see the code for the keys!
 * replace
 *  \x with x except for \r,\n,\t, -> space
 *  \nnn with nnn
 *  $*   with flag string, and then evaluate options
 *  $X   with -X<value>
 *  $0X  with -X <value>
 *  $-X  with  <value>
 *  $0-X with  <value> (same as $-X)
 *  ${s}   with value of control file parameter s (must be upper case)
 *  ${ss}  with value of printcap option ss
 *  $'{ss} with quoted value of printcap option ss
 *
 *  nosplit - do not split the option value over two entries
 *  flags -   flags to use for $*
 ***************************************************************************/

void Fix_dollars( struct line_list *l, struct job *job, int nosplit, char *flags )
{
	int i, j, count, space, notag, kind, n, c, position, quote;
	const char *str;
	char *strv, *s, *t, *rest;
	char buffer[SMALLBUFFER], tag[32];

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Fix_dollars- before", l );
#endif
	for( count = 0; count < l->count; ++count ){
		position = 0;
		for( strv = l->list[count]; (s = safestrpbrk(strv+position,"$\\")); ){
			DEBUG4("Fix_dollars: expanding [%d]='%s'", count, strv );
			position = s - strv;
			c = cval(s);
			*s++ = 0;
			if( c == '\\' ){
				c = *s++;
				/* check for end of string */
				if( c == 0 ) break;
				if( c == 'r' || c == 'n' || c == 't' ){
					c = ' ';
				} else if( isdigit( c ) ){
					tag[0] = c;
					if( (tag[1] = *s) ) ++s;
					if( (tag[2] = *s) ) ++s;
					tag[3] = 0;
					c = strtol( tag, 0, 8 );
				}
				if( !isprint(c) || isspace(c) ) c = ' ';
				strv[position] = c;
				++position;
				memmove(strv+position,s,safestrlen(s)+1);
				continue;
			}
			/* now we handle the $ */
			str = 0;
			rest = 0;
			n = space = notag = quote = 0;
			kind = STRING_K;
			while( (c = cval(s)) && safestrchr( " 0-'", c) ){
				switch( c ){
				case '0': case ' ': space = 1; break;
				case '-':           notag = 1; break;
				case '\'':          quote = 1; break;
				default: break;
				}
				++s;
			}
			rest = s+1;
			if( c == '*' ){
				if( flags && *flags ){
					rest = safestrdup(rest,__FILE__,__LINE__);
					position = safestrlen(strv);
					l->list[count] = strv
						 = safeextend3(strv,flags,rest,__FILE__,__LINE__);
					if( rest ) free(rest); rest = 0;
				}
				continue;
			} else if( c == '{' ){
				++s;
				if( !(rest = safestrchr(rest,'}')) ){
					break;
				}
				*rest++ = 0;
				if( !cval(s+1) && isupper(cval(s)) ){
					str = job?Find_str_value( &job->info,s,Value_sep):0;
				} else {
					str = Find_value( &PC_entry_line_list, s, Value_sep );
				}
				notag = 1;
				space = 0;
			} else {
				quote = 0;
				switch( c ){
				case 'a': 
					str = Accounting_file_DYN;
					if( str && cval(str) == '|' ) str = 0;
					break;
				case 'b': str = job?Find_str_value(&job->info,SIZE,Value_sep):0; break;
				case 'c':
					notag = 1; space=0;
					t = job?Find_str_value(&job->info,FORMAT,Value_sep):0;
					if( t && *t == 'l'){
						str="-c";
					}
					break;
				case 'd': str = Spool_dir_DYN; break;
				case 'e':
					str = job?Find_str_value(&job->info,
						DF_NAME,Value_sep):0;
					break;
				case 'f':
					str = job?Find_str_value(&job->info,"N",Value_sep):0;
					break;
				case 'h':
					str = job?Find_str_value(&job->info,FROMHOST,Value_sep):0;
					break;
				case 'i':
					str = job?Find_str_value(&job->info,"I",Value_sep):0;
					break;
				case 'j':
					str = job?Find_str_value(&job->info,NUMBER,Value_sep):0;
					break;
				case 'k':
					str = job?Find_str_value(&job->info,TRANSFERNAME,Value_sep):0;
					break;
				case 'l':
					kind = INTEGER_K; n = Page_length_DYN; break;
				case 'n':
					str = job?Find_str_value(&job->info,LOGNAME,Value_sep):0;
					break;
				case 'p': str = RemotePrinter_DYN; break;
				case 'r': str = RemoteHost_DYN; break;
				case 's': str = Status_file_DYN; break;
				case 't':
					str = Time_str( 0, time( (void *)0 ) ); break;
				case 'w': kind = INTEGER_K; n = Page_width_DYN; break;
				case 'x': kind = INTEGER_K; n = Page_x_DYN; break;
				case 'y': kind = INTEGER_K; n = Page_y_DYN; break;
				case 'F':
					str = job?Find_str_value(&job->info,FORMAT,Value_sep):0;
					break;
				case 'P': str = Printer_DYN; break;
				case 'S': str = Comment_tag_DYN; break;
				/* case '_': str = esc_Auth_client_id_DYN; break; */
				default:
					if( isupper(c) ){
						buffer[1] = 0; buffer[0] = c;
						str = job?Find_str_value( &job->info,buffer,Value_sep):0;
					}
					break;
				}
			}
			buffer[0] = 0;
			tag[0] = 0;
			switch( kind ){
			case INTEGER_K:
				SNPRINTF(buffer,sizeof(buffer))"%d", n );
				str = buffer;
				break;
			}
			DEBUG4(
				"Fix_dollars: strv '%s', found '%s', rest '%s', notag %d, space %d",
				strv, str, rest, notag, space );
			tag[0] = 0;
			if( str && !cval(str) ) str = 0;
			if( quote && !str ) str = "";
			if( str ){
				rest = safestrdup(rest,__FILE__,__LINE__);
				if( notag ){
					space = 0;
				} else {
					i = 0;
					if( (quote || nosplit) && !space ) tag[i++] = '\'';
					tag[i++] = '-'; tag[i++] = c; tag[i++] = 0;
					l->list[count] = strv = safeextend2( strv, tag, __FILE__,__LINE__ );
					if( !(quote || nosplit) ) tag[0] = 0;
					tag[1] = 0;
				}
				if( space ){
					DEBUG4("Fix_dollars: space [%d]='%s'", count, l->list[count] );
					if( quote || nosplit ){
						position = safestrlen(strv) + safestrlen(str) + 2;
						l->list[count] =
							strv = safeextend5( strv," '",str,"'",rest,__FILE__,__LINE__);
					} else {
						Check_max(l,2);
						for( i = l->count; i >= count; --i ){
							l->list[i+1] = l->list[i];
						}
						++l->count;
						++count;
						l->list[count] = strv = safestrdup2(str,rest,__FILE__,__LINE__);
						position = safestrlen(str);
					}
				} else {
					position = safestrlen(strv) + safestrlen(str)+safestrlen(tag);
					l->list[count] = strv
						 = safeextend4(strv,str,tag,rest,__FILE__,__LINE__);
				}
				if( rest ) free(rest); rest = 0;
			} else {
				memmove(strv+position,rest,safestrlen(rest)+1);
			}
			DEBUG4("Fix_dollars: [%d]='%s'", count, strv );
		}
	}
	for( i = j = 0; i < l->count; ++i ){
		if( (s = l->list[i]) && *s == 0 ){
			free(s); s = 0;
		}
		l->list[j] = s;
		if( s ) ++j;
	}
	l->count = j;
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Fix_dollars- after", l );
#endif
}
#endif

/*
 * char *Make_pathname( char *dir, char *file )
 *  - makes a full pathname from the dir and file part
 */

char *Make_pathname( const char *dir,  const char *file )
{
	char *s, *path;
	if( file == 0 ){
		path = 0;
	} else if( file[0] == '/' ){
		path = safestrdup(file,__FILE__,__LINE__);
	} else if( dir ){
		path = safestrdup3(dir,"/",file,__FILE__,__LINE__);
	} else {
		path = safestrdup2("./",file,__FILE__,__LINE__);
	}
	if( (s = path) ) while((s = strstr(s,"//"))) memmove(s,s+1,safestrlen(s)+1 );
	return(path);
}

/***************************************************************************
 * Get_keywords and keyval
 * - decode the control word and return a key
 ***************************************************************************/

int Get_keyval( char *s, struct keywords *controlwords )
{
	int i;
	char *t;
	for( i = 0; controlwords[i].keyword; ++i ){
		if(
			safestrcasecmp( s, controlwords[i].keyword ) == 0
			|| ( (t = controlwords[i].translation) && safestrcasecmp( s, _(t) ) == 0)
			){
			return( controlwords[i].type );
		}
	}
	return( 0 );
}

char *Get_keystr( int c, struct keywords *controlwords )
{
	int i;
	for( i = 0; controlwords[i].keyword; ++i ){
		if( controlwords[i].type == c ){
			return( controlwords[i].keyword );
		}
	}
	return( 0 );
}

char *Escape( char *str, int level )
{
	char *s = 0;
	int i, c, j, k, incr = 3*level;
	int len = 0;

	if( str == 0 || *str == 0 ) return(0);
	if( level <= 0 ) level = 1;

	len = safestrlen(str);
	for( j = 0; (c = cval(str+j)); ++j ){
		if( c != ' ' && !isalnum( c ) ){
			len += incr;
		}
	}
	DEBUG5("Escape: level %d, allocated length %d, length %d, for '%s'",
		level, len, safestrlen(str), str );
	s = malloc_or_die(len+1,__FILE__,__LINE__);
	i = 0;
	for( i = j = 0; (c = cval(str+j)); ++j ){
		if( c == ' ' ){
			s[i++] = '?';
		} else if( !isalnum( c ) ){
			SNPRINTF(s+i,4)"%%%02x",c);
			/* we encode the % as %25 and move the other stuff over */
			for( k = 1; k < level; ++k ){
				/* we move the stuff after the % two positions */
				/* s+i is the %, s+i+1 is the first digit */
				memmove(s+i+3, s+i+1, safestrlen(s+i+1)+1);
				memmove(s+i+1, "25", 2 );
			}
			i += safestrlen(s+i);
		} else {
			s[i++] = c;
		}
	}
	s[i] = 0;
	DEBUG5("Escape: final length %d '%s'", i,  s );
	return(s);
}

/*
 * we replace a colon by \072 in a dynmaically allocated string
 */

void Escape_colons( struct line_list *list )
{
	int linenumber, len, c;
	char *str, *s, *t, *newstr;
	
	for( linenumber = 0; list && linenumber < list->count; ++linenumber ){
		str = list->list[linenumber];

		if( str == 0 || strchr(str,':') == 0 ) continue;

		len = safestrlen(str);
		for( s = str; (s = strchr(s,':')); ++s ){
			len += 4;
		}
		DEBUG4("Escape_colons: new length %d for '%s'",
			len, str );
		newstr = t = malloc_or_die(len+1,__FILE__,__LINE__);
		for( s = str; (c = cval(s)); ++s ){
			if( c != ':' ){
				*t++ = c;
			} else {
				strcpy(t,"\\072");
				t += 4;
			}
		}
		*t = 0;
		free(str);
		list->list[linenumber] = newstr;
		DEBUG4("Escape_colons: '%s'", newstr );
	}
}

void Unescape( char *str )
{
	int i, c;
	char *s = str;
	char buffer[4];
	if( str == 0 ) return;
	for( i = 0; (c = cval(str)); ++str ){
		if( c == '?' ){
			c = ' ';
		} else if( c == '%'
			&& (buffer[0] = cval(str+1))
			&& (buffer[1] = cval(str+2))
			){
			buffer[2] = 0;
			c = strtol(buffer,0,16);
			str += 2;
		}
		s[i++] = c;
	}
	s[i] = 0;
	DEBUG5("Unescape '%s'", s );
}

char *Find_str_in_str( char *str, const char *key, const char *sep )
{
	char *s = 0, *end;
	int len = safestrlen(key), c;

	if(str) for( s = str; (s = strstr(s,key)); ++s ){
		c = cval(s+len);
		if( !(safestrchr(Value_sep, c) || safestrchr(sep, c)) ) continue;
		if( s > str && !safestrchr(sep,cval(s-1)) ) continue;
		s += len;
		if( (end = safestrpbrk(s,sep)) ){ c = *end; *end = 0; }
		/* skip over Value_sep character
		 * x@;  -> x@\000  - get null str
		 * x;   -> x\000  - get null str
		 * x=v;  -> x=v  - get v
		 */
		if( *s ) ++s;
		if( *s ){
			s = safestrdup(s,__FILE__,__LINE__);
		} else {
			s = 0;
		}
		if( end ) *end = c;
		break;
	}
	return(s);
}

/*
 * int Find_key_in_list( struct line_list *l, char *key, char *sep, int *mid )
 *  Search and unsorted list for a key value, starting at *mid.
 *
 *  The list has lines of the form:
 *    key [separator] value
 *  returns:
 *    *at = index of last tested value
 *    return value: 0 if found;
 *                  <0 if list[*at] < key
 *                  >0 if list[*at] > key
 */

int Find_key_in_list( struct line_list *l, const char *key, const char *sep, int *m )
{
	int mid = 0, cmp = -1, c = 0;
	char *s, *t;
	if( m ) mid = *m;
	DEBUG5("Find_key_in_list: start %d, count %d, key '%s'", mid, l->count, key );
	while( mid < l->count ){
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcasecmp(key,s);
		if( t ) *t = c;
		DEBUG5("Find_key_in_list: cmp %d, mid %d", cmp, mid);
		if( cmp == 0 ){
			if( m ) *m = mid;
			break;
		}
		++mid;
	}
	DEBUG5("Find_key_in_list: key '%s', cmp %d, mid %d", key, cmp, mid );
	return( cmp );
}

/***************************************************************************
 * int Fix_str( char * str )
 * - make a copy of the original string
 * - substitute all the escape characters
 * \f, \n, \r, \t, and \nnn
 ***************************************************************************/

char *Fix_str( char *str )
{
	char *s, *end, *dupstr, buffer[4];
	int c, len;
	DEBUG3("Fix_str: '%s'", str );
	if( str == 0 ) return(str);
	dupstr = s = safestrdup(str,__FILE__,__LINE__);
	DEBUG3("Fix_str: dup '%s', 0x%lx", dupstr, Cast_ptr_to_long(dupstr) );
	for( ; (s = safestrchr(s,'\\')); ){
		end = s+1;
		c = cval(end);
		/* check for \nnn */
		if( isdigit( c ) ){
			for( len = 0; len < 3; ++len ){
				if( !isdigit(cval(end)) ){
					break;
				}
				buffer[len] = *end++;
			}
			c = strtol(buffer,0,8);
		} else {
			switch( c ){
				case 'f': c = '\f'; break;
				case 'r': c = '\r'; break;
				case 'n': c = '\n'; break;
				case 't': c = '\t'; break;
			}
			++end;
		}
		s[0] = c;
		if( c == 0 ) break;
		memcpy(s+1,end,safestrlen(end)+1);
		++s;
	}
	if( *dupstr == 0 ){ free(dupstr); dupstr = 0; }
	DEBUG3( "Fix_str: final str '%s' -> '%s'", str, dupstr );
	return( dupstr );
}

#if defined(JYWENG20031104Shutdown_or_close)
/***************************************************************************
 * int Shutdown_or_close( int fd )
 * - if the file descriptor is a socket, then do a shutdown (write), return fd;
 *   or if the 
 * - otherwise close it and return -1;
 ***************************************************************************/

int Shutdown_or_close( int fd )
{
	struct stat statb;

	if( fd < 0 || fstat( fd, &statb ) == -1 ){
		fd = -1;
	} else if( Backwards_compatible_DYN || !Half_close_DYN
		|| !(S_ISSOCK(statb.st_mode)) || shutdown( fd, 1 ) == -1 ){
		close(fd);
		fd = -1;
	}
	return( fd );
}
#endif

#ifdef REMOVE
/*
 *  Support for non-copy on write fork as for NT
 *   1. Preparation for the fork is done by calling 'Setup_lpd_call'
 *      This establishes a default setup for the new process by setting
 *      up a list of parameters and file descriptors to be passed.
 *   2. The user then adds fork/process specific options
 *   3. The fork is done by calling Make_lpd_call which actually
 *      does the fork() operation.  If the lpd_path option is set,
 *      then a -X command line flag is added and an execv() of the program
 *      is done.
 *   4.A - fork()
 *        Make_lpd_call (child) will call Do_work(),  which dispatches
 *         a call to the required function.
 *   4.B - execv()
 *        The execv'd process checks the command line parameters for -X
 *         flag and when it finds it calls Do_work() with the same parameters
 *         as would be done for the fork() version.
 */

void Setup_lpd_call( struct line_list *passfd, struct line_list *args )
{
	Free_line_list( args );
	Check_max(passfd, 10 );
	passfd->count = 0;
	passfd->list[passfd->count++] = Cast_int_to_voidstar(0);
	passfd->list[passfd->count++] = Cast_int_to_voidstar(1);
	passfd->list[passfd->count++] = Cast_int_to_voidstar(2);
	if( Mail_fd > 0 ){
		Set_decimal_value(args,MAIL_FD,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Mail_fd);
	}
	if( Status_fd > 0 ){
		Set_decimal_value(args,STATUS_FD,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Status_fd);
	}
	if( Logger_fd > 0 ){
		Set_decimal_value(args,LOGGER,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Logger_fd);
	}
	if( Lpd_request > 0 ){
		Set_decimal_value(args,LPD_REQUEST,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Lpd_request);
	}
	Set_flag_value(args,DEBUG,Debug);
	Set_flag_value(args,DEBUGFV,DbgFlag);
#ifdef DMALLOC
	{
		extern int _dmalloc_outfile_fd;
		if( _dmalloc_outfile_fd > 0 ){
			Set_decimal_value(args,DMALLOC_OUTFILE,passfd->count);
			passfd->list[passfd->count++] = Cast_int_to_voidstar(_dmalloc_outfile_fd);
		}
	}
#endif
}

/*
 * Make_lpd_call - does the actual forking operation
 *  - sets up file descriptor for child, can close_on_exec()
 *  - does fork() or execve() as appropriate
 *
 *  returns: pid of child or -1 if fork failed.
 */

int Make_lpd_call( char *name, struct line_list *passfd, struct line_list *args )
{
	int pid, fd, i, n, newfd;
	struct line_list env;


	Init_line_list(&env);
	pid = dofork(1);
	if( pid ){
		return(pid);
	}
	Name = "LPD_CALL";
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL2){
		LOGDEBUG("Make_lpd_call: lpd path '%s'", Lpd_path_DYN );
		LOGDEBUG("Make_lpd_call: passfd count %d", passfd->count );
		for( i = 0; i < passfd->count; ++i ){
			LOGDEBUG(" [%d] %d", i, Cast_ptr_to_int(passfd->list[i]));
		}
		Dump_line_list("Make_lpd_call - args", args );
	}
#endif
	for( i = 0; i < passfd->count; ++i ){
		fd = Cast_ptr_to_int(passfd->list[i]);
		if( fd < i  ){
			/* we have fd 3 -> 4, but 3 gets wiped out */
			do{
				newfd = dup(fd);
				Max_open(newfd);
				if( newfd < 0 ){
					Errorcode = JABORT;
					LOGERR_DIE(LOG_INFO)"Make_lpd_call: dup failed");
				}
				DEBUG4("Make_lpd_call: fd [%d] = %d, dup2 -> %d",
					i, fd, newfd );
				passfd->list[i] = Cast_int_to_voidstar(newfd);
			} while( newfd < i );
		}
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL2){
		LOGDEBUG("Make_lpd_call: after fixing fd count %d", passfd->count);
		for( i = 0 ; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			LOGDEBUG("  [%d]=%d",i,fd);
		}
	}
#endif
	for( i = 0; i < passfd->count; ++i ){
		fd = Cast_ptr_to_int(passfd->list[i]);
		DEBUG2("Make_lpd_call: fd %d -> %d",fd, i );
		if( dup2( fd, i ) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO)"Make_lpd_call: dup2(%d,%d) failed",
				fd, i );
		}
	}
	/* close other ones to simulate close_on_exec() */
	n = Max_fd+10;
	for( i = passfd->count ; i < n; ++i ){
		close(i);
	}
	passfd->count = 0;
	Free_line_list( passfd );
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "linelist:Make_lpd_call: b4 Do_work\n");
fclose(aaaaaa);
#endif
	Do_work( name, args );
	return(0);
}

void Do_work( char *name, struct line_list *args )
{
	void  (*proc)() = 0;
	Logger_fd = Find_flag_value(args, LOGGER,Value_sep);
	Status_fd = Find_flag_value(args, STATUS_FD,Value_sep);
	Mail_fd = Find_flag_value(args, MAIL_FD,Value_sep);
	Lpd_request = Find_flag_value(args, LPD_REQUEST,Value_sep);
	/* undo the non-blocking IO */
	if( Lpd_request > 0 ) Set_block_io( Lpd_request );
	Debug= Find_flag_value( args, DEBUG, Value_sep);
	DbgFlag= Find_flag_value( args, DEBUGFV, Value_sep);
#ifdef DMALLOC
	{
		extern int _dmalloc_outfile_fd;
		_dmalloc_outfile_fd = Find_flag_value(args, DMALLOC_OUTFILE,Value_sep);
	}
#endif
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "linelist: Do_work: starting...name=%s\n", name);
fclose(aaaaaa);
#endif
#ifdef ORIGINAL_DEBUG//JY@1020
	if( !safestrcasecmp(name,"logger") ) proc = Logger;
	else if( !safestrcasecmp(name,"all") ) proc = Service_all;
#else
	if( !safestrcasecmp(name,"all") ) proc = Service_all;
#endif
	else if( !safestrcasecmp(name,"server") ) proc = Service_connection;
	else if( !safestrcasecmp(name,"queue") ) proc = Service_queue;
	else if( !safestrcasecmp(name,"printer") ) proc = Service_worker;
	DEBUG3("Do_work: '%s', proc 0x%lx ", name, Cast_ptr_to_long(proc) );
	(proc)(args);
	cleanup(0);
}

/*
 * Start_worker - general purpose dispatch function
 *   - adds an input FD
 */

int Start_worker( char *name, struct line_list *parms, int fd )
{
	struct line_list passfd, args;
	int pid;

	Init_line_list(&passfd);
	Init_line_list(&args);
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL1){
		DEBUG1("Start_worker: fd %d", fd );
		Dump_line_list("Start_worker - parms", parms );
	}
#endif
	Setup_lpd_call( &passfd, &args );
	Merge_line_list( &args, parms, Value_sep,1,1);
	Free_line_list( parms );
	if( fd ){
		Check_max(&passfd,2);
		Set_decimal_value(&args,INPUT,passfd.count);
		passfd.list[passfd.count++] = Cast_int_to_voidstar(fd);
	}

	pid = Make_lpd_call( name, &passfd, &args );
	Free_line_list( &args );
	passfd.count = 0;
	Free_line_list( &passfd );
	DEBUG1("Start_worker: pid %d", pid );
	return(pid);
}
#endif

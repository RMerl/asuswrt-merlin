/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

extern int DEBUGLEVEL;

static char *last_ptr=NULL;

void set_first_token(char *ptr)
{
	last_ptr = ptr;
}

/****************************************************************************
  Get the next token from a string, return False if none found
  handles double-quotes. 
Based on a routine by GJC@VILLAGE.COM. 
Extensively modified by Andrew.Tridgell@anu.edu.au
****************************************************************************/
BOOL next_token(char **ptr,char *buff,char *sep, size_t bufsize)
{
  char *s;
  BOOL quoted;
  size_t len=1;

  if (!ptr) ptr = &last_ptr;
  if (!ptr) return(False);

  s = *ptr;

  /* default to simple separators */
  if (!sep) sep = " \t\n\r";

  /* find the first non sep char */
  while(*s && strchr(sep,*s)) s++;

  /* nothing left? */
  if (! *s) return(False);

  /* copy over the token */
  for (quoted = False; len < bufsize && *s && (quoted || !strchr(sep,*s)); s++)
    {
	    if (*s == '\"') {
		    quoted = !quoted;
	    } else {
		    len++;
		    *buff++ = *s;
	    }
    }

  *ptr = (*s) ? s+1 : s;  
  *buff = 0;
  last_ptr = *ptr;

  return(True);
}

/****************************************************************************
Convert list of tokens to array; dependent on above routine.
Uses last_ptr from above - bit of a hack.
****************************************************************************/
char **toktocliplist(int *ctok, char *sep)
{
  char *s=last_ptr;
  int ictok=0;
  char **ret, **iret;

  if (!sep) sep = " \t\n\r";

  while(*s && strchr(sep,*s)) s++;

  /* nothing left? */
  if (!*s) return(NULL);

  do {
    ictok++;
    while(*s && (!strchr(sep,*s))) s++;
    while(*s && strchr(sep,*s)) *s++=0;
  } while(*s);

  *ctok=ictok;
  s=last_ptr;

  if (!(ret=iret=malloc(ictok*sizeof(char *)))) return NULL;
  
  while(ictok--) {    
    *iret++=s;
    while(*s++);
    while(!*s) s++;
  }

  return ret;
}


/*******************************************************************
  case insensitive string compararison
********************************************************************/
int StrCaseCmp(const char *s, const char *t)
{
  /* compare until we run out of string, either t or s, or find a difference */
  /* We *must* use toupper rather than tolower here due to the
     asynchronous upper to lower mapping.
   */
#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA.
   */

  if(lp_client_code_page() == KANJI_CODEPAGE)
  {
    /* Win95 treats full width ascii characters as case sensitive. */
    int diff;
    for (;;)
    {
      if (!*s || !*t)
	    return toupper (*s) - toupper (*t);
      else if (is_sj_alph (*s) && is_sj_alph (*t))
      {
        diff = sj_toupper2 (*(s+1)) - sj_toupper2 (*(t+1));
        if (diff)
          return diff;
        s += 2;
        t += 2;
      }
      else if (is_shift_jis (*s) && is_shift_jis (*t))
      {
        diff = ((int) (unsigned char) *s) - ((int) (unsigned char) *t);
        if (diff)
          return diff;
        diff = ((int) (unsigned char) *(s+1)) - ((int) (unsigned char) *(t+1));
        if (diff)
          return diff;
        s += 2;
        t += 2;
      }
      else if (is_shift_jis (*s))
        return 1;
      else if (is_shift_jis (*t))
        return -1;
      else 
      {
        diff = toupper (*s) - toupper (*t);
        if (diff)
          return diff;
        s++;
        t++;
      }
    }
  }
  else
#endif /* KANJI_WIN95_COMPATIBILITY */
  {
    while (*s && *t && toupper(*s) == toupper(*t))
    {
      s++;
      t++;
    }

    return(toupper(*s) - toupper(*t));
  }
}

/*******************************************************************
  case insensitive string compararison, length limited
********************************************************************/
int StrnCaseCmp(const char *s, const char *t, size_t n)
{
  /* compare until we run out of string, either t or s, or chars */
  /* We *must* use toupper rather than tolower here due to the
     asynchronous upper to lower mapping.
   */
#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA. 
   */

  if(lp_client_code_page() == KANJI_CODEPAGE)
  {
    /* Win95 treats full width ascii characters as case sensitive. */
    int diff;
    for (;n > 0;)
    {
      if (!*s || !*t)
        return toupper (*s) - toupper (*t);
      else if (is_sj_alph (*s) && is_sj_alph (*t))
      {
        diff = sj_toupper2 (*(s+1)) - sj_toupper2 (*(t+1));
        if (diff)
          return diff;
        s += 2;
        t += 2;
        n -= 2;
      }
      else if (is_shift_jis (*s) && is_shift_jis (*t))
      {
        diff = ((int) (unsigned char) *s) - ((int) (unsigned char) *t);
        if (diff)
          return diff;
        diff = ((int) (unsigned char) *(s+1)) - ((int) (unsigned char) *(t+1));
        if (diff)
          return diff;
        s += 2;
        t += 2;
        n -= 2;
      }
      else if (is_shift_jis (*s))
        return 1;
      else if (is_shift_jis (*t))
        return -1;
      else 
      {
        diff = toupper (*s) - toupper (*t);
        if (diff)
          return diff;
        s++;
        t++;
        n--;
      }
    }
    return 0;
  }
  else
#endif /* KANJI_WIN95_COMPATIBILITY */
  {
    while (n && *s && *t && toupper(*s) == toupper(*t))
    {
      s++;
      t++;
      n--;
    }

    /* not run out of chars - strings are different lengths */
    if (n) 
      return(toupper(*s) - toupper(*t));

    /* identical up to where we run out of chars, 
       and strings are same length */
    return(0);
  }
}

/*******************************************************************
  compare 2 strings 
********************************************************************/
BOOL strequal(const char *s1, const char *s2)
{
  if (s1 == s2) return(True);
  if (!s1 || !s2) return(False);
  
  return(StrCaseCmp(s1,s2)==0);
}

/*******************************************************************
  compare 2 strings up to and including the nth char.
  ******************************************************************/
BOOL strnequal(const char *s1,const char *s2,size_t n)
{
  if (s1 == s2) return(True);
  if (!s1 || !s2 || !n) return(False);
  
  return(StrnCaseCmp(s1,s2,n)==0);
}

/*******************************************************************
  compare 2 strings (case sensitive)
********************************************************************/
BOOL strcsequal(const char *s1,const char *s2)
{
  if (s1 == s2) return(True);
  if (!s1 || !s2) return(False);
  
  return(strcmp(s1,s2)==0);
}


/*******************************************************************
  convert a string to lower case
********************************************************************/
void strlower(char *s)
{
  while (*s)
  {
#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA. 
   */

    if(lp_client_code_page() == KANJI_CODEPAGE)
    {
      /* Win95 treats full width ascii characters as case sensitive. */
      if (is_shift_jis (*s))
      {
        if (is_sj_upper (s[0], s[1]))
          s[1] = sj_tolower2 (s[1]);
        s += 2;
      }
      else if (is_kana (*s))
      {
        s++;
      }
      else
      {
        if (isupper(*s))
          *s = tolower(*s);
        s++;
      }
    }
    else
#endif /* KANJI_WIN95_COMPATIBILITY */
    {
      size_t skip = get_character_len( *s );
      if( skip != 0 )
        s += skip;
      else
      {
        if (isupper(*s))
          *s = tolower(*s);
        s++;
      }
    }
  }
}

/*******************************************************************
  convert a string to upper case
********************************************************************/
void strupper(char *s)
{
  while (*s)
  {
#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA. 
   */

    if(lp_client_code_page() == KANJI_CODEPAGE)
    {
      /* Win95 treats full width ascii characters as case sensitive. */
      if (is_shift_jis (*s))
      {
        if (is_sj_lower (s[0], s[1]))
          s[1] = sj_toupper2 (s[1]);
        s += 2;
      }
      else if (is_kana (*s))
      {
        s++;
      }
      else
      {
        if (islower(*s))
          *s = toupper(*s);
        s++;
      }
    }
    else
#endif /* KANJI_WIN95_COMPATIBILITY */
    {
      size_t skip = get_character_len( *s );
      if( skip != 0 )
        s += skip;
      else
      {
        if (islower(*s))
          *s = toupper(*s);
        s++;
      }
    }
  }
}

/*******************************************************************
  convert a string to "normal" form
********************************************************************/
void strnorm(char *s)
{
  extern int case_default;
  if (case_default == CASE_UPPER)
    strupper(s);
  else
    strlower(s);
}

/*******************************************************************
check if a string is in "normal" case
********************************************************************/
BOOL strisnormal(char *s)
{
  extern int case_default;
  if (case_default == CASE_UPPER)
    return(!strhaslower(s));

  return(!strhasupper(s));
}


/****************************************************************************
  string replace
****************************************************************************/
void string_replace(char *s,char oldc,char newc)
{
  size_t skip;

  /*
   * sbcs optimization.
   */
  if(!global_is_multibyte_codepage) {
    while (*s) {
      if (oldc == *s)
        *s = newc;
      s++;
    }
  } else {
    while (*s)
    {
      skip = get_character_len( *s );
      if( skip != 0 )
        s += skip;
      else
      {
        if (oldc == *s)
          *s = newc;
        s++;
      }
    }
  }
}


/*******************************************************************
skip past some strings in a buffer
********************************************************************/
char *skip_string(char *buf,size_t n)
{
  while (n--)
    buf += strlen(buf) + 1;
  return(buf);
}

/*******************************************************************
 Count the number of characters in a string. Normally this will
 be the same as the number of bytes in a string for single byte strings,
 but will be different for multibyte.
 16.oct.98, jdblair@cobaltnet.com.
********************************************************************/

size_t str_charnum(const char *s)
{
  size_t len = 0;
  
  /*
   * sbcs optimization.
   */
  if(!global_is_multibyte_codepage) {
    return strlen(s);
  } else {
    while (*s != '\0') {
      int skip = get_character_len(*s);
      s += (skip ? skip : 1);
      len++;
    }
  }
  return len;
}

/*******************************************************************
trim the specified elements off the front and back of a string
********************************************************************/

BOOL trim_string(char *s,const char *front,const char *back)
{
  BOOL ret = False;
  size_t front_len = (front && *front) ? strlen(front) : 0;
  size_t back_len = (back && *back) ? strlen(back) : 0;
  size_t s_len;

  while (front_len && strncmp(s, front, front_len) == 0)
  {
    char *p = s;
    ret = True;
    while (1)
    {
      if (!(*p = p[front_len]))
        break;
      p++;
    }
  }

  /*
   * We split out the multibyte code page
   * case here for speed purposes. Under a
   * multibyte code page we need to walk the
   * string forwards only and multiple times.
   * Thanks to John Blair for finding this
   * one. JRA.
   */

  if(back_len)
  {
    if(!global_is_multibyte_codepage)
    {
      s_len = strlen(s);
      while ((s_len >= back_len) && 
             (strncmp(s + s_len - back_len, back, back_len)==0))  
      {
        ret = True;
        s[s_len - back_len] = '\0';
        s_len = strlen(s);
      }
    }
    else
    {

      /*
       * Multibyte code page case.
       * Keep going through the string, trying
       * to match the 'back' string with the end
       * of the string. If we get a match, truncate
       * 'back' off the end of the string and
       * go through the string again from the
       * start. Keep doing this until we have
       * gone through the string with no match
       * at the string end.
       */

      size_t mb_back_len = str_charnum(back);
      size_t mb_s_len = str_charnum(s);

      while(mb_s_len >= mb_back_len)
      {
        size_t charcount = 0;
        char *mbp = s;

        /*
         * sbcs optimization.
         */
        if(!global_is_multibyte_codepage) {
          while(charcount < (mb_s_len - mb_back_len)) {
            mbp += 1;
            charcount++;
          }
        } else {
          while(charcount < (mb_s_len - mb_back_len)) {
            size_t skip = skip_multibyte_char(*mbp);
            mbp += (skip ? skip : 1);
            charcount++;
          }
        }

        /*
         * mbp now points at mb_back_len multibyte
         * characters from the end of s.
         */

        if(strcmp(mbp, back) == 0)
        {
          ret = True;
          *mbp = '\0';
          mb_s_len = str_charnum(s);
          mbp = s;
        }
        else
          break;
      } /* end while mb_s_len... */
    } /* end else .. */
  } /* end if back_len .. */

  return(ret);
}


/****************************************************************************
does a string have any uppercase chars in it?
****************************************************************************/
BOOL strhasupper(const char *s)
{
  while (*s) 
  {
#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA. 
   */

    if(lp_client_code_page() == KANJI_CODEPAGE)
    {
      /* Win95 treats full width ascii characters as case sensitive. */
      if (is_shift_jis (*s))
        s += 2;
      else if (is_kana (*s))
        s++;
      else
      {
        if (isupper(*s))
          return(True);
        s++;
      }
    }
    else
#endif /* KANJI_WIN95_COMPATIBILITY */
    {
      size_t skip = get_character_len( *s );
      if( skip != 0 )
        s += skip;
      else {
        if (isupper(*s))
          return(True);
        s++;
      }
    }
  }
  return(False);
}

/****************************************************************************
does a string have any lowercase chars in it?
****************************************************************************/
BOOL strhaslower(const char *s)
{
  while (*s) 
  {
#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA. 
   */

    if(lp_client_code_page() == KANJI_CODEPAGE)
    {
      /* Win95 treats full width ascii characters as case sensitive. */
      if (is_shift_jis (*s))
      {
        if (is_sj_upper (s[0], s[1]))
          return(True);
        if (is_sj_lower (s[0], s[1]))
          return (True);
        s += 2;
      }
      else if (is_kana (*s))
      {
        s++;
      }
      else
      {
        if (islower(*s))
          return(True);
        s++;
      }
    }
    else
#endif /* KANJI_WIN95_COMPATIBILITY */
    {
      size_t skip = get_character_len( *s );
      if( skip != 0 )
        s += skip;
      else {
        if (islower(*s))
          return(True);
        s++;
      }
    }
  }
  return(False);
}

/****************************************************************************
find the number of chars in a string
****************************************************************************/
size_t count_chars(const char *s,char c)
{
  size_t count=0;

#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA. 
   */

  if(lp_client_code_page() == KANJI_CODEPAGE)
  {
    /* Win95 treats full width ascii characters as case sensitive. */
    while (*s) 
    {
      if (is_shift_jis (*s))
        s += 2;
      else 
      {
        if (*s == c)
          count++;
        s++;
      }
    }
  }
  else
#endif /* KANJI_WIN95_COMPATIBILITY */
  {
    while (*s) 
    {
      size_t skip = get_character_len( *s );
      if( skip != 0 )
        s += skip;
      else {
        if (*s == c)
          count++;
        s++;
      }
    }
  }
  return(count);
}

/*******************************************************************
Return True if a string consists only of one particular character.
********************************************************************/

BOOL str_is_all(const char *s,char c)
{
  if(s == NULL)
    return False;
  if(!*s)
    return False;

#if !defined(KANJI_WIN95_COMPATIBILITY)
  /*
   * For completeness we should put in equivalent code for code pages
   * 949 (Korean hangul) and 950 (Big5 Traditional Chinese) here - but
   * doubt anyone wants Samba to behave differently from Win95 and WinNT
   * here. They both treat full width ascii characters as case senstive
   * filenames (ie. they don't do the work we do here).
   * JRA.
   */

  if(lp_client_code_page() == KANJI_CODEPAGE)
  {
    /* Win95 treats full width ascii characters as case sensitive. */
    while (*s)
    {
      if (is_shift_jis (*s))
        s += 2;
      else
      {
        if (*s != c)
          return False;
        s++;
      }
    }
  }
  else
#endif /* KANJI_WIN95_COMPATIBILITY */
  {
    while (*s)
    {
      size_t skip = get_character_len( *s );
      if( skip != 0 )
        s += skip;
      else {
        if (*s != c)
          return False;
        s++;
      }
    }
  }
  return True;
}

/*******************************************************************
safe string copy into a known length string. maxlength does not
include the terminating zero.
********************************************************************/

char *safe_strcpy(char *dest,const char *src, size_t maxlength)
{
    size_t len;

    if (!dest) {
        DEBUG(0,("ERROR: NULL dest in safe_strcpy\n"));
        return NULL;
    }

    if (!src) {
        *dest = 0;
        return dest;
    }  

    len = strlen(src);

    if (len > maxlength) {
	    DEBUG(0,("ERROR: string overflow by %d in safe_strcpy [%.50s]\n",
		     (int)(len-maxlength), src));
	    len = maxlength;
    }
      
    memcpy(dest, src, len);
    dest[len] = 0;
    return dest;
}  

/*******************************************************************
safe string cat into a string. maxlength does not
include the terminating zero.
********************************************************************/

char *safe_strcat(char *dest, const char *src, size_t maxlength)
{
    size_t src_len, dest_len;

    if (!dest) {
        DEBUG(0,("ERROR: NULL dest in safe_strcat\n"));
        return NULL;
    }

    if (!src) {
        return dest;
    }  

    src_len = strlen(src);
    dest_len = strlen(dest);

    if (src_len + dest_len > maxlength) {
	    DEBUG(0,("ERROR: string overflow by %d in safe_strcat [%.50s]\n",
		     (int)(src_len + dest_len - maxlength), src));
	    src_len = maxlength - dest_len;
    }
      
    memcpy(&dest[dest_len], src, src_len);
    dest[dest_len + src_len] = 0;
    return dest;
}

/*******************************************************************
 Paranoid strcpy into a buffer of given length (includes terminating
 zero. Strips out all but 'a-Z0-9' and the character in other_safe_chars
 and replaces with '_'. Deliberately does *NOT* check for multibyte
 characters. Don't change it !
********************************************************************/

char *alpha_strcpy(char *dest, const char *src, const char *other_safe_chars, size_t maxlength)
{
	size_t len, i;

	if (!dest) {
		DEBUG(0,("ERROR: NULL dest in alpha_strcpy\n"));
		return NULL;
	}

	if (!src) {
		*dest = 0;
		return dest;
	}  

	len = strlen(src);
	if (len >= maxlength)
		len = maxlength - 1;

	if (!other_safe_chars)
		other_safe_chars = "";

	for(i = 0; i < len; i++) {
		int val = (src[i] & 0xff);
		if(isupper(val) || islower(val) || isdigit(val) || strchr(other_safe_chars, val))
			dest[i] = src[i];
		else
			dest[i] = '_';
	}

	dest[i] = '\0';

	return dest;
}

/****************************************************************************
 Like strncpy but always null terminates. Make sure there is room!
 The variable n should always be one less than the available size.
****************************************************************************/

char *StrnCpy(char *dest,const char *src,size_t n)
{
  char *d = dest;
  if (!dest) return(NULL);
  if (!src) {
    *dest = 0;
    return(dest);
  }
  while (n-- && (*d++ = *src++)) ;
  *d = 0;
  return(dest);
}

/****************************************************************************
like strncpy but copies up to the character marker.  always null terminates.
returns a pointer to the character marker in the source string (src).
****************************************************************************/
char *strncpyn(char *dest, const char *src,size_t n, char c)
{
	char *p;
	size_t str_len;

	p = strchr(src, c);
	if (p == NULL)
	{
		DEBUG(5, ("strncpyn: separator character (%c) not found\n", c));
		return NULL;
	}

	str_len = PTR_DIFF(p, src);
	strncpy(dest, src, MIN(n, str_len));
	dest[str_len] = '\0';

	return p;
}


/*************************************************************
 Routine to get hex characters and turn them into a 16 byte array.
 the array can be variable length, and any non-hex-numeric
 characters are skipped.  "0xnn" or "0Xnn" is specially catered
 for.

 valid examples: "0A5D15"; "0x15, 0x49, 0xa2"; "59\ta9\te3\n"

**************************************************************/
size_t strhex_to_str(char *p, size_t len, const char *strhex)
{
	size_t i;
	size_t num_chars = 0;
	unsigned char   lonybble, hinybble;
	char           *hexchars = "0123456789ABCDEF";
	char           *p1 = NULL, *p2 = NULL;

	for (i = 0; i < len && strhex[i] != 0; i++)
	{
		if (strnequal(hexchars, "0x", 2))
		{
			i++; /* skip two chars */
			continue;
		}

		if (!(p1 = strchr(hexchars, toupper(strhex[i]))))
		{
			break;
		}

		i++; /* next hex digit */

		if (!(p2 = strchr(hexchars, toupper(strhex[i]))))
		{
			break;
		}

		/* get the two nybbles */
		hinybble = PTR_DIFF(p1, hexchars);
		lonybble = PTR_DIFF(p2, hexchars);

		p[num_chars] = (hinybble << 4) | lonybble;
		num_chars++;

		p1 = NULL;
		p2 = NULL;
	}
	return num_chars;
}

/****************************************************************************
check if a string is part of a list
****************************************************************************/
BOOL in_list(char *s,char *list,BOOL casesensitive)
{
  pstring tok;
  char *p=list;

  if (!list) return(False);

  while (next_token(&p,tok,LIST_SEP,sizeof(tok))) {
    if (casesensitive) {
      if (strcmp(tok,s) == 0)
        return(True);
    } else {
      if (StrCaseCmp(tok,s) == 0)
        return(True);
    }
  }
  return(False);
}

/* this is used to prevent lots of mallocs of size 1 */
static char *null_string = NULL;

/****************************************************************************
set a string value, allocing the space for the string
****************************************************************************/
static BOOL string_init(char **dest,const char *src)
{
  size_t l;
  if (!src)     
    src = "";

  l = strlen(src);

  if (l == 0)
    {
      if (!null_string) {
        if((null_string = (char *)malloc(1)) == NULL) {
          DEBUG(0,("string_init: malloc fail for null_string.\n"));
          return False;
        }
        *null_string = 0;
      }
      *dest = null_string;
    }
  else
    {
      (*dest) = (char *)malloc(l+1);
      if ((*dest) == NULL) {
	      DEBUG(0,("Out of memory in string_init\n"));
	      return False;
      }

      pstrcpy(*dest,src);
    }
  return(True);
}

/****************************************************************************
free a string value
****************************************************************************/
void string_free(char **s)
{
  if (!s || !(*s)) return;
  if (*s == null_string)
    *s = NULL;
  if (*s) free(*s);
  *s = NULL;
}

/****************************************************************************
set a string value, allocing the space for the string, and deallocating any 
existing space
****************************************************************************/
BOOL string_set(char **dest,const char *src)
{
  string_free(dest);

  return(string_init(dest,src));
}


/****************************************************************************
substitute a string for a pattern in another string. Make sure there is 
enough room!

This routine looks for pattern in s and replaces it with 
insert. It may do multiple replacements.

any of " ; ' $ or ` in the insert string are replaced with _
if len==0 then no length check is performed

Return True if a change was made, False otherwise.
****************************************************************************/
BOOL string_sub(char *s,const char *pattern,const char *insert, size_t len)
{
	BOOL changed = False;
	char *p;
	ssize_t ls,lp,li, i;

	if (!insert || !pattern || !s) return False;

	ls = (ssize_t)strlen(s);
	lp = (ssize_t)strlen(pattern);
	li = (ssize_t)strlen(insert);

	if (!*pattern) return False;
	
	while (lp <= ls && (p = strstr(s,pattern))) {
		changed = True;
		if (len && (ls + (li-lp) >= len)) {
			DEBUG(0,("ERROR: string overflow by %d in string_sub(%.50s, %d)\n", 
				 (int)(ls + (li-lp) - len),
				 pattern, (int)len));
			break;
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		for (i=0;i<li;i++) {
			switch (insert[i]) {
			case '`':
			case '"':
			case '\'':
			case ';':
			case '$':
			case '%':
			case '\r':
			case '\n':
				p[i] = '_';
				break;
			default:
				p[i] = insert[i];
			}
		}
		s = p + li;
		ls += (li-lp);
	}

	return changed;
}

BOOL fstring_sub(char *s,const char *pattern,const char *insert)
{
	return string_sub(s, pattern, insert, sizeof(fstring));
}

BOOL pstring_sub(char *s,const char *pattern,const char *insert)
{
	return string_sub(s, pattern, insert, sizeof(pstring));
}

/****************************************************************************
similar to string_sub() but allows for any character to be substituted. 
Use with caution!
if len==0 then no length check is performed
****************************************************************************/
BOOL all_string_sub(char *s,const char *pattern,const char *insert, size_t len)
{
	BOOL changed = False;
	char *p;
	ssize_t ls,lp,li;

	if (!insert || !pattern || !s) return False;

	ls = (ssize_t)strlen(s);
	lp = (ssize_t)strlen(pattern);
	li = (ssize_t)strlen(insert);

	if (!*pattern) return False;
	
	while (lp <= ls && (p = strstr(s,pattern))) {
		changed = True;
		if (len && (ls + (li-lp) >= len)) {
			DEBUG(0,("ERROR: string overflow by %d in all_string_sub(%.50s, %d)\n", 
				 (int)(ls + (li-lp) - len),
				 pattern, (int)len));
			break;
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		memcpy(p, insert, li);
		s = p + li;
		ls += (li-lp);
	}

	return changed;
}

/****************************************************************************
 splits out the front and back at a separator.
****************************************************************************/
void split_at_last_component(char *path, char *front, char sep, char *back)
{
	char *p = strrchr(path, sep);

	if (p != NULL)
	{
		*p = 0;
	}
	if (front != NULL)
	{
		pstrcpy(front, path);
	}
	if (p != NULL)
	{
		if (back != NULL)
		{
			pstrcpy(back, p+1);
		}
		*p = '\\';
	}
	else
	{
		if (back != NULL)
		{
			back[0] = 0;
		}
	}
}


/****************************************************************************
write an octal as a string
****************************************************************************/
char *octal_string(int i)
{
	static char ret[64];
	if (i == -1) {
		return "-1";
	}
	slprintf(ret, sizeof(ret), "0%o", i);
	return ret;
}


/****************************************************************************
truncate a string at a specified length
****************************************************************************/
char *string_truncate(char *s, int length)
{
	if (s && strlen(s) > length) {
		s[length] = 0;
	}
	return s;
}

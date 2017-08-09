/* 
   Unix SMB/CIFS implementation.
   filename matching routine
   Copyright (C) Andrew Tridgell 1992-2004

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  
*/

/*
   This module was originally based on fnmatch.c copyright by the Free
   Software Foundation. It bears little (if any) resemblence to that
   code now
*/  

/**
 * @file
 * @brief MS-style Filename matching
 */

#include "includes.h"

static int null_match(const char *p)
{
	for (;*p;p++) {
		if (*p != '*' &&
		    *p != '<' &&
		    *p != '"' &&
		    *p != '>') return -1;
	}
	return 0;
}

/*
  the max_n structure is purely for efficiency, it doesn't contribute
  to the matching algorithm except by ensuring that the algorithm does
  not grow exponentially
*/
struct max_n {
	const char *predot;
	const char *postdot;
};


/*
  p and n are the pattern and string being matched. The max_n array is
  an optimisation only. The ldot pointer is NULL if the string does
  not contain a '.', otherwise it points at the last dot in 'n'.
*/
static int ms_fnmatch_core(const char *p, const char *n, 
			   struct max_n *max_n, const char *ldot)
{
	codepoint_t c, c2;
	int i;
	size_t size, size_n;

	while ((c = next_codepoint(p, &size))) {
		p += size;

		switch (c) {
		case '*':
			/* a '*' matches zero or more characters of any type */
			if (max_n->predot && max_n->predot <= n) {
				return null_match(p);
			}
			for (i=0; n[i]; i += size_n) {
				next_codepoint(n+i, &size_n);
				if (ms_fnmatch_core(p, n+i, max_n+1, ldot) == 0) {
					return 0;
				}
			}
			if (!max_n->predot || max_n->predot > n) max_n->predot = n;
			return null_match(p);

		case '<':
			/* a '<' matches zero or more characters of
			   any type, but stops matching at the last
			   '.' in the string. */
			if (max_n->predot && max_n->predot <= n) {
				return null_match(p);
			}
			if (max_n->postdot && max_n->postdot <= n && n <= ldot) {
				return -1;
			}
			for (i=0; n[i]; i += size_n) {
				next_codepoint(n+i, &size_n);
				if (ms_fnmatch_core(p, n+i, max_n+1, ldot) == 0) return 0;
				if (n+i == ldot) {
					if (ms_fnmatch_core(p, n+i+size_n, max_n+1, ldot) == 0) return 0;
					if (!max_n->postdot || max_n->postdot > n) max_n->postdot = n;
					return -1;
				}
			}
			if (!max_n->predot || max_n->predot > n) max_n->predot = n;
			return null_match(p);

		case '?':
			/* a '?' matches any single character */
			if (! *n) {
				return -1;
			}
			next_codepoint(n, &size_n);
			n += size_n;
			break;

		case '>':
			/* a '?' matches any single character, but
			   treats '.' specially */
			if (n[0] == '.') {
				if (! n[1] && null_match(p) == 0) {
					return 0;
				}
				break;
			}
			if (! *n) return null_match(p);
			next_codepoint(n, &size_n);
			n += size_n;
			break;

		case '"':
			/* a bit like a soft '.' */
			if (*n == 0 && null_match(p) == 0) {
				return 0;
			}
			if (*n != '.') return -1;
			next_codepoint(n, &size_n);
			n += size_n;
			break;

		default:
			c2 = next_codepoint(n, &size_n);
			if (c != c2 && codepoint_cmpi(c, c2) != 0) {
				return -1;
			}
			n += size_n;
			break;
		}
	}
	
	if (! *n) {
		return 0;
	}
	
	return -1;
}

int ms_fnmatch(const char *pattern, const char *string, enum protocol_types protocol)
{
	int ret, count, i;
	struct max_n *max_n = NULL;

	if (strcmp(string, "..") == 0) {
		string = ".";
	}

	if (strpbrk(pattern, "<>*?\"") == NULL) {
		/* this is not just an optimisation - it is essential
		   for LANMAN1 correctness */
		return strcasecmp_m(pattern, string);
	}

	if (protocol <= PROTOCOL_LANMAN2) {
		char *p = talloc_strdup(NULL, pattern);
		if (p == NULL) {
			return -1;
		}
		/*
		  for older negotiated protocols it is possible to
		  translate the pattern to produce a "new style"
		  pattern that exactly matches w2k behaviour
		*/
		for (i=0;p[i];i++) {
			if (p[i] == '?') {
				p[i] = '>';
			} else if (p[i] == '.' && 
				   (p[i+1] == '?' || 
				    p[i+1] == '*' ||
				    p[i+1] == 0)) {
				p[i] = '"';
			} else if (p[i] == '*' && 
				   p[i+1] == '.') {
				p[i] = '<';
			}
		}
		ret = ms_fnmatch(p, string, PROTOCOL_NT1);
		talloc_free(p);
		return ret;
	}

	for (count=i=0;pattern[i];i++) {
		if (pattern[i] == '*' || pattern[i] == '<') count++;
	}

	max_n = talloc_zero_array(NULL, struct max_n, count);
	if (max_n == NULL) {
		return -1;
	}

	ret = ms_fnmatch_core(pattern, string, max_n, strrchr(string, '.'));

	talloc_free(max_n);

	return ret;
}


/** a generic fnmatch function - uses for non-CIFS pattern matching */
int gen_fnmatch(const char *pattern, const char *string)
{
	return ms_fnmatch(pattern, string, PROTOCOL_NT1);
}

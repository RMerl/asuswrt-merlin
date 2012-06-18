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


#include "includes.h"

static int null_match(const smb_ucs2_t *p)
{
	for (;*p;p++) {
		if (*p != UCS2_CHAR('*') &&
		    *p != UCS2_CHAR('<') &&
		    *p != UCS2_CHAR('"') &&
		    *p != UCS2_CHAR('>')) return -1;
	}
	return 0;
}

/*
  the max_n structure is purely for efficiency, it doesn't contribute
  to the matching algorithm except by ensuring that the algorithm does
  not grow exponentially
*/
struct max_n {
	const smb_ucs2_t *predot;
	const smb_ucs2_t *postdot;
};


/*
  p and n are the pattern and string being matched. The max_n array is
  an optimisation only. The ldot pointer is NULL if the string does
  not contain a '.', otherwise it points at the last dot in 'n'.
*/
static int ms_fnmatch_core(const smb_ucs2_t *p, const smb_ucs2_t *n,
			   struct max_n *max_n, const smb_ucs2_t *ldot,
			   bool is_case_sensitive)
{
	smb_ucs2_t c;
	int i;

	while ((c = *p++)) {
		switch (c) {
			/* a '*' matches zero or more characters of any type */
		case UCS2_CHAR('*'):
			if (max_n->predot && max_n->predot <= n) {
				return null_match(p);
			}
			for (i=0; n[i]; i++) {
				if (ms_fnmatch_core(p, n+i, max_n+1, ldot, is_case_sensitive) == 0) {
					return 0;
				}
			}
			if (!max_n->predot || max_n->predot > n) max_n->predot = n;
			return null_match(p);

			/* a '<' matches zero or more characters of
			   any type, but stops matching at the last
			   '.' in the string. */
		case UCS2_CHAR('<'):
			if (max_n->predot && max_n->predot <= n) {
				return null_match(p);
			}
			if (max_n->postdot && max_n->postdot <= n && n <= ldot) {
				return -1;
			}
			for (i=0; n[i]; i++) {
				if (ms_fnmatch_core(p, n+i, max_n+1, ldot, is_case_sensitive) == 0) return 0;
				if (n+i == ldot) {
					if (ms_fnmatch_core(p, n+i+1, max_n+1, ldot, is_case_sensitive) == 0) return 0;
					if (!max_n->postdot || max_n->postdot > n) max_n->postdot = n;
					return -1;
				}
			}
			if (!max_n->predot || max_n->predot > n) max_n->predot = n;
			return null_match(p);

			/* a '?' matches any single character */
		case UCS2_CHAR('?'):
			if (! *n) {
				return -1;
			}
			n++;
			break;

			/* a '?' matches any single character */
		case UCS2_CHAR('>'):
			if (n[0] == UCS2_CHAR('.')) {
				if (! n[1] && null_match(p) == 0) {
					return 0;
				}
				break;
			}
			if (! *n) return null_match(p);
			n++;
			break;

		case UCS2_CHAR('"'):
			if (*n == 0 && null_match(p) == 0) {
				return 0;
			}
			if (*n != UCS2_CHAR('.')) return -1;
			n++;
			break;

		default:
			if (c != *n) {
				if (is_case_sensitive) {
					return -1;
				}
				if (toupper_w(c) != toupper_w(*n)) {
					return -1;
				}
			}
			n++;
			break;
		}
	}

	if (! *n) {
		return 0;
	}

	return -1;
}

int ms_fnmatch(const char *pattern, const char *string, bool translate_pattern,
	       bool is_case_sensitive)
{
	smb_ucs2_t *p = NULL;
	smb_ucs2_t *s = NULL;
	int ret, count, i;
	struct max_n *max_n = NULL;
	struct max_n *max_n_free = NULL;
	struct max_n one_max_n;
	size_t converted_size;

	if (ISDOTDOT(string)) {
		string = ".";
	}

	if (strpbrk(pattern, "<>*?\"") == NULL) {
		/* this is not just an optmisation - it is essential
		   for LANMAN1 correctness */
		if (is_case_sensitive) {
			return strcmp(pattern, string);
		} else {
			return StrCaseCmp(pattern, string);
		}
	}

	if (!push_ucs2_talloc(talloc_tos(), &p, pattern, &converted_size)) {
		return -1;
	}

	if (!push_ucs2_talloc(talloc_tos(), &s, string, &converted_size)) {
		TALLOC_FREE(p);
		return -1;
	}

	if (translate_pattern) {
		/*
		  for older negotiated protocols it is possible to
		  translate the pattern to produce a "new style"
		  pattern that exactly matches w2k behaviour
		*/
		for (i=0;p[i];i++) {
			if (p[i] == UCS2_CHAR('?')) {
				p[i] = UCS2_CHAR('>');
			} else if (p[i] == UCS2_CHAR('.') &&
				   (p[i+1] == UCS2_CHAR('?') ||
				    p[i+1] == UCS2_CHAR('*') ||
				    p[i+1] == 0)) {
				p[i] = UCS2_CHAR('"');
			} else if (p[i] == UCS2_CHAR('*') && p[i+1] == UCS2_CHAR('.')) {
				p[i] = UCS2_CHAR('<');
			}
		}
	}

	for (count=i=0;p[i];i++) {
		if (p[i] == UCS2_CHAR('*') || p[i] == UCS2_CHAR('<')) count++;
	}

	if (count != 0) {
		if (count == 1) {
			/*
			 * We're doing this a LOT, so save the effort to allocate
			 */
			ZERO_STRUCT(one_max_n);
			max_n = &one_max_n;
		}
		else {
			max_n = SMB_CALLOC_ARRAY(struct max_n, count);
			if (!max_n) {
				TALLOC_FREE(p);
				TALLOC_FREE(s);
				return -1;
			}
			max_n_free = max_n;
		}
	}

	ret = ms_fnmatch_core(p, s, max_n, strrchr_w(s, UCS2_CHAR('.')), is_case_sensitive);

	SAFE_FREE(max_n_free);
	TALLOC_FREE(p);
	TALLOC_FREE(s);
	return ret;
}


/* a generic fnmatch function - uses for non-CIFS pattern matching */
int gen_fnmatch(const char *pattern, const char *string)
{
	return ms_fnmatch(pattern, string, PROTOCOL_NT1, False);
}

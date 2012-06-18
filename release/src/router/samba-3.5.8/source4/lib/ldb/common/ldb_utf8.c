/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: ldb
 *
 *  Component: ldb utf8 handling
 *
 *  Description: case folding and case comparison for UTF8 strings
 *
 *  Author: Andrew Tridgell
 */

#include "ldb_private.h"
#include "system/locale.h"


/*
  this allow the user to pass in a caseless comparison
  function to handle utf8 caseless comparisons
 */
void ldb_set_utf8_fns(struct ldb_context *ldb,
		      void *context,
		      char *(*casefold)(void *, void *, const char *, size_t))
{
	if (context)
		ldb->utf8_fns.context = context;
	if (casefold)
		ldb->utf8_fns.casefold = casefold;
}

/*
  a simple case folding function
  NOTE: does not handle UTF8
*/
char *ldb_casefold_default(void *context, void *mem_ctx, const char *s, size_t n)
{
	int i;
	char *ret = talloc_strndup(mem_ctx, s, n);
	if (!s) {
		errno = ENOMEM;
		return NULL;
	}
	for (i=0;ret[i];i++) {
		ret[i] = toupper((unsigned char)ret[i]);
	}
	return ret;
}

void ldb_set_utf8_default(struct ldb_context *ldb)
{
	ldb_set_utf8_fns(ldb, NULL, ldb_casefold_default);
}

char *ldb_casefold(struct ldb_context *ldb, void *mem_ctx, const char *s, size_t n)
{
	return ldb->utf8_fns.casefold(ldb->utf8_fns.context, mem_ctx, s, n);
}

/*
  check the attribute name is valid according to rfc2251
  returns 1 if the name is ok
 */

int ldb_valid_attr_name(const char *s)
{
	int i;

	if (!s || !s[0])
		return 0;

	/* handle special ldb_tdb wildcard */
	if (strcmp(s, "*") == 0) return 1;

	for (i = 0; s[i]; i++) {
		if (! isascii(s[i])) {
			return 0;
		}
		if (i == 0) { /* first char must be an alpha (or our special '@' identifier) */
			if (! (isalpha(s[i]) || (s[i] == '@'))) {
				return 0;
			}
		} else {
			if (! (isalnum(s[i]) || (s[i] == '-'))) {
				return 0;
			}
		}
	}
	return 1;
}

char *ldb_attr_casefold(void *mem_ctx, const char *s)
{
	int i;
	char *ret = talloc_strdup(mem_ctx, s);
	if (!ret) {
		errno = ENOMEM;
		return NULL;
	}
	for (i = 0; ret[i]; i++) {
		ret[i] = toupper((unsigned char)ret[i]);
	}
	return ret;
}

/*
  we accept either 'dn' or 'distinguishedName' for a distinguishedName
*/
int ldb_attr_dn(const char *attr)
{
	if (ldb_attr_cmp(attr, "dn") == 0 ||
	    ldb_attr_cmp(attr, "distinguishedName") == 0) {
		return 0;
	}
	return -1;
}

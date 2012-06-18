/* 
 *  Unix SMB/Netbios implementation.
 *  SEC_ACL handling routines
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Copyright (C) Jeremy R. Allison            1995-2003.
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                  1997-1998.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/secace.h"

#define  SEC_ACL_HEADER_SIZE (2 * sizeof(uint16_t) + sizeof(uint32_t))

/*******************************************************************
 Create a SEC_ACL structure.  
********************************************************************/

struct security_acl *make_sec_acl(TALLOC_CTX *ctx, 
								  enum security_acl_revision revision,
								  int num_aces, struct security_ace *ace_list)
{
	struct security_acl *dst;
	int i;

	if((dst = talloc_zero(ctx, struct security_acl)) == NULL)
		return NULL;

	dst->revision = revision;
	dst->num_aces = num_aces;
	dst->size = SEC_ACL_HEADER_SIZE;

	/* Now we need to return a non-NULL address for the ace list even
	   if the number of aces required is zero.  This is because there
	   is a distinct difference between a NULL ace and an ace with zero
	   entries in it.  This is achieved by checking that num_aces is a
	   positive number. */

	if ((num_aces) && 
            ((dst->aces = talloc_array(dst, struct security_ace, num_aces))
             == NULL)) {
		return NULL;
	}
        
	for (i = 0; i < num_aces; i++) {
		dst->aces[i] = ace_list[i]; /* Structure copy. */
		dst->size += ace_list[i].size;
	}

	return dst;
}

/*******************************************************************
 Duplicate a SEC_ACL structure.  
********************************************************************/

struct security_acl *dup_sec_acl(TALLOC_CTX *ctx, struct security_acl *src)
{
	if(src == NULL)
		return NULL;

	return make_sec_acl(ctx, src->revision, src->num_aces, src->aces);
}

/*******************************************************************
 Compares two SEC_ACL structures
********************************************************************/

bool sec_acl_equal(struct security_acl *s1, struct security_acl *s2)
{
	unsigned int i, j;

	/* Trivial cases */

	if (!s1 && !s2) return true;
	if (!s1 || !s2) return false;

	/* Check top level stuff */

	if (s1->revision != s2->revision) {
		DEBUG(10, ("sec_acl_equal(): revision differs (%d != %d)\n",
			   s1->revision, s2->revision));
		return false;
	}

	if (s1->num_aces != s2->num_aces) {
		DEBUG(10, ("sec_acl_equal(): num_aces differs (%d != %d)\n",
			   s1->revision, s2->revision));
		return false;
	}

	/* The ACEs could be in any order so check each ACE in s1 against 
	   each ACE in s2. */

	for (i = 0; i < s1->num_aces; i++) {
		bool found = false;

		for (j = 0; j < s2->num_aces; j++) {
			if (sec_ace_equal(&s1->aces[i], &s2->aces[j])) {
				found = true;
				break;
			}
		}

		if (!found) return false;
	}

	return true;
}

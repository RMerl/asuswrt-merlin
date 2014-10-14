/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"

/*
 * Source file used to associate/disassociate behaviors with virtualized
 * objects.  See xfs_behavior.h for more information about behaviors, etc.
 *
 * The implementation is split between functions in this file and macros
 * in xfs_behavior.h.
 */

/*
 * Insert a new behavior descriptor into a behavior chain.
 *
 * The behavior chain is ordered based on the 'position' number which
 * lives in the first field of the ops vector (higher numbers first).
 *
 * Attempts to insert duplicate ops result in an EINVAL return code.
 * Otherwise, return 0 to indicate success.
 */
int
bhv_insert(bhv_head_t *bhp, bhv_desc_t *bdp)
{
	bhv_desc_t	*curdesc, *prev;
	int		position;

	/*
	 * Validate the position value of the new behavior.
	 */
	position = BHV_POSITION(bdp);
	ASSERT(position >= BHV_POSITION_BASE && position <= BHV_POSITION_TOP);

	/*
	 * Find location to insert behavior.  Check for duplicates.
	 */
	prev = NULL;
	for (curdesc = bhp->bh_first;
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		/* Check for duplication. */
		if (curdesc->bd_ops == bdp->bd_ops) {
			ASSERT(0);
			return EINVAL;
		}

		/* Find correct position */
		if (position >= BHV_POSITION(curdesc)) {
			ASSERT(position != BHV_POSITION(curdesc));
			break;		/* found it */
		}

		prev = curdesc;
	}

	if (prev == NULL) {
		/* insert at front of chain */
		bdp->bd_next = bhp->bh_first;
		bhp->bh_first = bdp;
	} else {
		/* insert after prev */
		bdp->bd_next = prev->bd_next;
		prev->bd_next = bdp;
	}

	return 0;
}

/*
 * Remove a behavior descriptor from a position in a behavior chain;
 * the position is guaranteed not to be the first position.
 * Should only be called by the bhv_remove() macro.
 */
void
bhv_remove_not_first(bhv_head_t *bhp, bhv_desc_t *bdp)
{
	bhv_desc_t	*curdesc, *prev;

	ASSERT(bhp->bh_first != NULL);
	ASSERT(bhp->bh_first->bd_next != NULL);

	prev = bhp->bh_first;
	for (curdesc = bhp->bh_first->bd_next;
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc == bdp)
			break;		/* found it */
		prev = curdesc;
	}

	ASSERT(curdesc == bdp);
	prev->bd_next = bdp->bd_next;	/* remove from after prev */
}

/*
 * Looks for the first behavior within a specified range of positions.
 * Return the associated behavior descriptor.  Or NULL, if none found.
 */
bhv_desc_t *
bhv_lookup_range(bhv_head_t *bhp, int low, int high)
{
	bhv_desc_t	*curdesc;

	for (curdesc = bhp->bh_first;
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		int	position = BHV_POSITION(curdesc);

		if (position <= high) {
			if (position >= low)
				return curdesc;
			return NULL;
		}
	}

	return NULL;
}

/*
 * Return the base behavior in the chain, or NULL if the chain
 * is empty.
 *
 * The caller has not read locked the behavior chain, so acquire the
 * lock before traversing the chain.
 */
bhv_desc_t *
bhv_base(bhv_head_t *bhp)
{
	bhv_desc_t	*curdesc;

	for (curdesc = bhp->bh_first;
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc->bd_next == NULL) {
			return curdesc;
		}
	}

	return NULL;
}

void
bhv_head_init(
	bhv_head_t *bhp,
	char *name)
{
	bhp->bh_first = NULL;
}

void
bhv_insert_initial(
	bhv_head_t *bhp,
	bhv_desc_t *bdp)
{
	ASSERT(bhp->bh_first == NULL);
	(bhp)->bh_first = bdp;
}

void
bhv_head_destroy(
	bhv_head_t *bhp)
{
	ASSERT(bhp->bh_first == NULL);
}

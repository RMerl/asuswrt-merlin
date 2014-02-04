/*
 * This file contains the common code routines to access/update the
 * IGMP Snooping database.
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igsc_sdb.c 360693 2012-10-04 02:29:26Z $
 */
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/bcmip.h>
#include <osl.h>
#include <clist.h>
#if defined(linux)
#include <osl_linux.h>
#elif defined(__ECOS)
#include <osl_ecos.h>
#else /* defined(osl_xx) */
#error "Unsupported osl"
#endif /* defined(osl_xx) */
#include "emfc_export.h"
#include "igs_cfg.h"
#include "igsc_export.h"
#include "igsc.h"
#include "igsc_sdb.h"

/*
 * Description: This function initializes the IGSDB. IGSDB group
 *              entries are organized as a hash table with chaining
 *              for collision resolution. Each IGSDB group entry
 *              points to the chain of IGSDB host entries that are
 *              members of the group.
 *
 * Input:       igsc_info - IGSL Common code global data handle
 */
void
igsc_sdb_init(igsc_info_t *igsc_info)
{
	int32 i;

	for (i = 0; i < IGSDB_HASHT_SIZE; i++)
	{
		clist_init_head(&igsc_info->mgrp_sdb[i]);
	}

	return;
}

/*
 * Description: This function deletes the host entry of IGSDB. It also
 *              deletes the corresponding interfaces and group entries
 *              if this is the last member of the group.
 *
 * Input:       igsc_info - IGSL Common code global data handle
 *              mh        - Pointer to Multicast host entry of IGSDB
 */
static void
igsc_sdb_mh_delete(igsc_info_t *igsc_info, igsc_mh_t *mh)
{
	/* Delete the interface entry if no stream is going on it */
	if (--mh->mh_mi->mi_ref == 0)
	{
		IGS_DEBUG("Deleting interface entry %p\n", mh->mh_mi);

		/* Delete the MFDB entry if no more members of the group
		 * are present on the interface.
		 */
		if (emfc_mfdb_membership_del(igsc_info->emf_handle,
		                             mh->mh_mgrp->mgrp_ip,
		                             mh->mh_mi->mi_ifp) != SUCCESS)
		{
			IGS_ERROR("Membership entry %x %p delete failed\n",
			          mh->mh_mgrp->mgrp_ip, mh->mh_mi->mi_ifp);
			return;
		}

		clist_delete(&mh->mh_mi->mi_list);
		MFREE(igsc_info->osh, mh->mh_mi, sizeof(igsc_mi_t));
	}

	/* Delete the host entry */
	clist_delete(&mh->mh_list);
	IGSC_STATS_DECR(igsc_info, igmp_mcast_members);

	/* If the member being deleted is last node in the host list, 
	 * delete the group entry also.
	 */
	if (clist_empty(&mh->mh_mgrp->mh_head))
	{
		IGS_IGSDB("Deleting group entry of %d.%d.%d.%d too\n",
		          (mh->mh_mgrp->mgrp_ip >> 24),
		          ((mh->mh_mgrp->mgrp_ip >> 16) & 0xff),
		          ((mh->mh_mgrp->mgrp_ip >> 8) & 0xff),
		          (mh->mh_mgrp->mgrp_ip & 0xff));

		clist_delete(&mh->mh_mgrp->mgrp_hlist);
		MFREE(igsc_info->osh, mh->mh_mgrp, sizeof(igsc_mgrp_t));
		IGSC_STATS_DECR(igsc_info, igmp_mcast_groups);
	}

	MFREE(igsc_info->osh, mh, sizeof(igsc_mh_t));

	return;
}

/*
 * Description: This function is called when no reports are received
 *              from any of the participating group members with in
 *              multicast group membership interval time. It deletes
 *              all the group member information from IGSDB so that
 *              the multicast stream forwarding is stopped.
 *
 * Input:       mh - Pointer to multicast host entry that timed out.
 */
static void
igsc_sdb_timer(igsc_mh_t *mh)
{
	igsc_info_t *igsc_info;

	igsc_info = mh->mh_mgrp->igsc_info;

	IGSC_STATS_INCR(igsc_info, igmp_mem_timeouts);

	IGS_IGSDB("Multicast host entry %d.%d.%d.%d timed out\n",
	          (mh->mh_ip >> 24), ((mh->mh_ip >> 16) & 0xff),
	          ((mh->mh_ip >> 8) & 0xff), (mh->mh_ip & 0xff));

	/* Timer expiry indicates that no report is seen from the
	 * member. First check the interface entry before deleting
	 * the host entry.
	 */
	igsc_sdb_mh_delete(igsc_info, mh);

	return;
}

/*
 * Description: This function does the IGSDB lookup to locate a multicast
 *              group entry.
 *
 * Input:       mgrp_ip - Multicast group address of the entry.
 *
 * Return:      Returns NULL is no group entry is found. Otherwise
 *              returns pointer to the IGSDB group entry.
 */
static igsc_mgrp_t *
igsc_sdb_group_find(igsc_info_t *igsc_info, uint32 mgrp_ip)
{
	uint32 hash;
	igsc_mgrp_t *mgrp;
	clist_head_t *ptr;

	ASSERT(IP_ISMULTI(mgrp_ip));

	hash = IGSDB_MGRP_HASH(mgrp_ip);

	for (ptr = igsc_info->mgrp_sdb[hash].next;
	     ptr != &igsc_info->mgrp_sdb[hash]; ptr = ptr->next)
	{
		mgrp = clist_entry(ptr, igsc_mgrp_t, mgrp_hlist);

		/* Compare group address */
		if (mgrp_ip == mgrp->mgrp_ip)
		{
			IGS_IGSDB("Multicast Group entry %d.%d.%d.%d found\n",
			          (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
			          ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff));

			return (mgrp);
		}
	}

	return (NULL);
}

/*
 * Description: This fnction does the MFDB lookup to locate host entry
 *              of the specified group.
 *
 * Input:       igsc_info - IGMP Snooper instance handle.
 *              mgrp      - Pointer to multicast group entry of IGSDB
 *              mh_ip     - Member IP Address to find
 *              ifp       - Pointer to the member interface.
 *
 * Return:      Returns pointer to host entry or NULL.
 */
static igsc_mh_t *
igsc_sdb_mh_entry_find(igsc_info_t *igsc_info, igsc_mgrp_t *mgrp,
                       uint32 mh_ip, void *ifp)
{
	igsc_mh_t *mh;
	clist_head_t *ptr;

	for (ptr = mgrp->mh_head.next;
	     ptr != &mgrp->mh_head; ptr = ptr->next)
	{
		mh = clist_entry(ptr, igsc_mh_t, mh_list);
		if ((mh_ip == mh->mh_ip) && (ifp == mh->mh_mi->mi_ifp))
		{
			return (mh);
		}
	}

	return (NULL);
}

static igsc_mi_t *
igsc_sdb_mi_entry_find(igsc_info_t *igsc_info, igsc_mgrp_t *mgrp, void *ifp)
{
	igsc_mi_t *mi;
	clist_head_t *ptr;

	for (ptr = mgrp->mi_head.next;
	     ptr != &mgrp->mi_head; ptr = ptr->next)
	{
		mi = clist_entry(ptr, igsc_mi_t, mi_list);
		if (ifp == mi->mi_ifp)
		{
			return (mi);
		}
	}

	return (NULL);
}

/*
 * Description: This function does the IGSDB lookup to locate a host
 *              entry of the specified multicast group.
 *
 * Input:       igsc_info - IGMP Snooper instance handle.
 *              mgrp_ip   - Multicast group IP address of the entry.
 *              mh_ip     - Multicast host address.
 *              ifp       - Pointer to the interface on which the member
 *                          is present.
 *
 * Return:      Returns NULL is no host entry is found. Otherwise
 *              returns pointer to the IGSDB host entry.
 */
static igsc_mh_t *
igsc_sdb_member_find(igsc_info_t *igsc_info, uint32 mgrp_ip,
                     uint32 mh_ip, void *ifp)
{
	uint32 hash;
	igsc_mgrp_t *mgrp;
	igsc_mh_t *mh;

	ASSERT(IP_ISMULTI(mgrp_ip));

	hash = IGSDB_MGRP_HASH(mgrp_ip);

	/* Find group entry */
	mgrp = igsc_sdb_group_find(igsc_info, mgrp_ip);

	if (mgrp != NULL)
	{
		/* Find host entry */
		mh = igsc_sdb_mh_entry_find(igsc_info, mgrp, mh_ip, ifp);

		if (mh != NULL)
		{
			IGS_IGSDB("Host entry of %d.%d.%d.%d:%d.%d.%d.%d:%p found\n",
			          (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
			          ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff),
			          (mh_ip >> 24), ((mh_ip >> 16) & 0xff),
			          ((mh_ip >> 8) & 0xff), (mh_ip & 0xff), (uint32)ifp);
			return (mh);
		}
	}

	IGS_IGSDB("Host entry %x %x %p not found\n", mgrp_ip, mh_ip, ifp);

	return (NULL);
}
#ifdef MEDIAROOM_IGMP_WAR
/* MSFT MediaRoom clients are aware of the entire group. If one client sends out a membership
 * report and the other clients see it, the other clients will not send out separate membership
 * reports. This causes us to age out perfectly good streaming clients and causes packet loss.
 * WAR this by updating the timer for entire group
 */
static void
update_all_timers_in_group(igsc_info_t *igsc_info, igsc_mgrp_t *mgrp,
                       uint32 mh_ip, void *ifp)
{
	igsc_mh_t *mh;
	clist_head_t *ptr;

	for (ptr = mgrp->mh_head.next;
	     ptr != &mgrp->mh_head; ptr = ptr->next)
	{
		mh = clist_entry(ptr, igsc_mh_t, mh_list);
		IGS_IGSDB("Refreshing timer for group %x and host %x\n",
			mh->mh_mgrp->mgrp_ip, mh->mh_ip);
		osl_timer_update(mh->mgrp_timer, igsc_info->grp_mem_intv, FALSE);
	}

	return;
}
#endif /* MEDIAROOM_IGMP_WAR */

/*
 * Description: This function is called by IGMP Snooper when it wants
 *              to add IGSDB entry or refresh the entry. This function
 *              is also called by the management application to add a
 *              static IGSDB entry.
 *
 *              If the IGSDB entry is not present, it allocates group
 *              entry, host entry, interface entry and links them
 *              together. Othewise it just updates the timer for the
 *              matched host entry.
 *
 * Input:       mgrp_ip - Multicast group IP address of the entry.
 *              mh_ip   - Multicast host address. When adding static
 *                        entry this parameter is zero.
 *              ifp     - Pointer to the interface on which the member
 *                        is present.
 *
 * Return:      SUCCESS or FAILURE
 */
int32
igsc_sdb_member_add(igsc_info_t *igsc_info, void *ifp, uint32 mgrp_ip,
                    uint32 mh_ip)
{
	int32 hash, ret;
	igsc_mgrp_t *mgrp;
	igsc_mh_t *mh;
	igsc_mi_t *mi;

	ASSERT(IP_ISMULTI(mgrp_ip));
	ASSERT(!IP_ISMULTI(mh_ip));

	OSL_LOCK(igsc_info->sdb_lock);

	/* If the group entry doesn't exist, add a new entry and update
	 * the member/host information.
	 */
	mgrp = igsc_sdb_group_find(igsc_info, mgrp_ip);

	if (mgrp == NULL)
	{
		/* Allocate and initialize multicast group entry */
		mgrp = MALLOC(igsc_info->osh, sizeof(igsc_mgrp_t));
		if (mgrp == NULL)
		{
			IGS_ERROR("Failed to alloc size %d for IGSDB group entry\n",
			          sizeof(igsc_mgrp_t));
			goto sdb_add_exit0;
		}

		mgrp->mgrp_ip = mgrp_ip;
		clist_init_head(&mgrp->mh_head);
		clist_init_head(&mgrp->mi_head);
		mgrp->igsc_info = igsc_info;

		IGS_IGSDB("Adding group entry %d.%d.%d.%d\n",
		          (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
		          ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff));

		/* Add the group entry to hash table */
		hash = IGSDB_MGRP_HASH(mgrp_ip);
		clist_add_head(&igsc_info->mgrp_sdb[hash], &mgrp->mgrp_hlist);

		IGSC_STATS_INCR(igsc_info, igmp_mcast_groups);
	}
	else
	{
		IGS_IGSDB("Refreshing FDB entry for group %d.%d.%d.%d:%d.%d.%d.%d\n",
		          (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
		          ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff),
		          (mh_ip >> 24), ((mh_ip >> 16) & 0xff),
		          ((mh_ip >> 8) & 0xff), (mh_ip & 0xff));

		/* Avoid adding duplicate entries */
		mh = igsc_sdb_mh_entry_find(igsc_info, mgrp, mh_ip, ifp);
		if (mh != NULL)
		{
			OSL_UNLOCK(igsc_info->sdb_lock);
#ifdef MEDIAROOM_IGMP_WAR
			/* Refresh the group interval timer for all group members */
			update_all_timers_in_group(igsc_info, mgrp, mh_ip, ifp);
#else
			/* Refresh the group interval timer for this group */
			osl_timer_update(mh->mgrp_timer, igsc_info->grp_mem_intv, FALSE);
#endif
			return (SUCCESS);
		}
	}

	/* Allocate and initialize multicast host entry */
	mh = MALLOC(igsc_info->osh, sizeof(igsc_mh_t));
	if (mh == NULL)
	{
		IGS_ERROR("Failed to allocated memory size %d for IGSDB host entry\n",
		          sizeof(igsc_mh_t));
		goto sdb_add_exit1;
	}

	/* Initialize the host entry */
	mh->mh_ip = mh_ip;
	mh->mh_mgrp = mgrp;

	IGS_IGSDB("Adding host entry %d.%d.%d.%d on interface %p\n",
	          (mh_ip >> 24), ((mh_ip >> 16) & 0xff),
	          ((mh_ip >> 8) & 0xff), (mh_ip & 0xff), ifp);

	IGSC_STATS_INCR(igsc_info, igmp_mcast_members);

	/* Set the group interval timer */
	mh->mgrp_timer = osl_timer_init("IGMPV2_GRP_MEM_INTV",
	                                (void (*)(void *))igsc_sdb_timer,
	                                (void *)mh);
	if (mh->mgrp_timer == NULL)
	{
		IGS_ERROR("Failed to allocate memory size %d for IGSDB timer\n",
		          sizeof(osl_timer_t));
		goto sdb_add_exit2;
	}

	osl_timer_add(mh->mgrp_timer, igsc_info->grp_mem_intv, FALSE);

	/* Add the host entry to the group */
	clist_add_head(&mgrp->mh_head, &mh->mh_list);

	/* Avoid adding duplicate interface list entries */
	mi = igsc_sdb_mi_entry_find(igsc_info, mgrp, ifp);
	if (mi != NULL)
	{
		/* Link the interface list entry */
		mh->mh_mi = mi;

		/* Increment ref count indicating a new reference from
		 * host entry.
		 */
		ASSERT(mi->mi_ref > 0);
		mi->mi_ref++;

		OSL_UNLOCK(igsc_info->sdb_lock);
		return (SUCCESS);
	}

	ret = emfc_mfdb_membership_add(igsc_info->emf_handle,
	                               mgrp->mgrp_ip, ifp);
	if (ret != SUCCESS)
	{
		IGS_ERROR("Failed to add MFDB entry for %x %p\n",
		          mgrp_ip, ifp);
		goto sdb_add_exit3;
	}

	/* Allocate and initialize multicast interface entry */
	mi = MALLOC(igsc_info->osh, sizeof(igsc_mi_t));
	if (mi == NULL)
	{
		IGS_ERROR("Failed to allocated memory size %d for interface entry\n",
		          sizeof(igsc_mi_t));
		goto sdb_add_exit4;
	}

	/* Initialize the multicast interface list entry */
	mi->mi_ifp = ifp;
	mi->mi_ref = 1;

	IGS_IGSDB("Adding interface entry for interface %p\n", mi->mi_ifp);

	/* Add the multicast interface entry */
	clist_add_head(&mgrp->mi_head, &mi->mi_list);

	/* Link the interface list entry to host entry */
	mh->mh_mi = mi;

	OSL_UNLOCK(igsc_info->sdb_lock);

	return (SUCCESS);

sdb_add_exit4:
	emfc_mfdb_membership_del(igsc_info->emf_handle, mgrp->mgrp_ip, ifp);
sdb_add_exit3:
	osl_timer_del(mh->mgrp_timer);
	clist_delete(&mh->mh_list);
sdb_add_exit2:
	MFREE(igsc_info->osh, mh, sizeof(igsc_mh_t));
sdb_add_exit1:
	if (clist_empty(&mgrp->mh_head))
	{
		clist_delete(&mgrp->mgrp_hlist);
		MFREE(igsc_info->osh, mgrp, sizeof(igsc_mgrp_t));
	}
sdb_add_exit0:
	OSL_UNLOCK(igsc_info->sdb_lock);
	return (FAILURE);
}

/*
 * Description: This function is called by the IGMP snooper layer
 *              to delete the IGSDB host entry. It deletes the group
 *              entry also if the host entry is last in the group.
 *
 * Input:       Same as above function.
 *
 * Return:      SUCCESS or FAILURE
 */
int32
igsc_sdb_member_del(igsc_info_t *igsc_info, void *ifp, uint32 mgrp_ip,
                    uint32 mh_ip)
{
	igsc_mh_t *mh;

	OSL_LOCK(igsc_info->sdb_lock);

	mh = igsc_sdb_member_find(igsc_info, mgrp_ip, mh_ip, ifp);

	if (mh != NULL)
	{
		/* Delete the timer */
		osl_timer_del(mh->mgrp_timer);

		IGS_IGSDB("Deleting host entry %d.%d.%d.%d\n",
		          (mh_ip >> 24), ((mh_ip >> 16) & 0xff),
		          ((mh_ip >> 8) & 0xff), (mh_ip & 0xff));

		igsc_sdb_mh_delete(igsc_info, mh);

		OSL_UNLOCK(igsc_info->sdb_lock);

		return (SUCCESS);
	}

	OSL_UNLOCK(igsc_info->sdb_lock);

	return (FAILURE);
}

/*
 * Description: This function clears the group interval timers and
 *              deletes the group, host and interface entries of the
 *              IGSDB.
 */
void
igsc_sdb_clear(igsc_info_t *igsc_info)
{
	uint32 i;
	igsc_mgrp_t *mgrp;
	igsc_mh_t *mh;
	clist_head_t *ptr1, *ptr2, *tmp1, *tmp2;

	OSL_LOCK(igsc_info->sdb_lock);

	/* Delete all the group entries */
	for (i = 0; i < IGSDB_HASHT_SIZE; i++)
	{
		for (ptr1 = igsc_info->mgrp_sdb[i].next;
		     ptr1 != &igsc_info->mgrp_sdb[i]; ptr1 = tmp1)
		{
			mgrp = clist_entry(ptr1, igsc_mgrp_t, mgrp_hlist);

			/* Delete all host entries */
			for (ptr2 = mgrp->mh_head.next; ptr2 != &mgrp->mh_head; ptr2 = tmp2)
			{
				mh = clist_entry(ptr2, igsc_mh_t, mh_list);

				/* Delete the interface entry if no stream is going on it */
				if (--mh->mh_mi->mi_ref == 0)
				{
					IGS_IGSDB("Deleting interface entry %p\n", mh->mh_mi);

					/* Delete the MFDB entry if no more members of the group
					 * are present on the interface.
					 */
					emfc_mfdb_membership_del(igsc_info->emf_handle,
					                         mh->mh_mgrp->mgrp_ip,
					                         mh->mh_mi->mi_ifp);
					clist_delete(&mh->mh_mi->mi_list);
					MFREE(igsc_info->osh, mh->mh_mi, sizeof(igsc_mi_t));
				}

				osl_timer_del(mh->mgrp_timer);
				tmp2 = ptr2->next;
				clist_delete(ptr2);
				MFREE(igsc_info->osh, mh, sizeof(igsc_mh_t));
			}

			tmp1 = ptr1->next;
			clist_delete(ptr1);
			MFREE(igsc_info->osh, mgrp, sizeof(igsc_mgrp_t));
		}
	}

	OSL_UNLOCK(igsc_info->sdb_lock);

	return;
}

/*
 * IGSDB Listing Function
 */
int32
igsc_sdb_list(igsc_info_t *igsc_info, igs_cfg_sdb_list_t *list, uint32 size)
{
	clist_head_t *ptr1, *ptr2;
	igsc_mh_t *mh;
	igsc_mgrp_t *mgrp;
	int32 i, index = 0;

	if (igsc_info == NULL)
	{
		IGS_ERROR("Invalid IGSC handle passed\n");
		return (FAILURE);
	}

	if (list == NULL)
	{
		IGS_ERROR("Invalid buffer input\n");
		return (FAILURE);
	}

	for (i = 0; i < IGSDB_HASHT_SIZE; i++)
	{
		for (ptr1 = igsc_info->mgrp_sdb[i].next;
		     ptr1 != &igsc_info->mgrp_sdb[i];
		     ptr1 = ptr1->next)
		{
			mgrp = clist_entry(ptr1, igsc_mgrp_t, mgrp_hlist);
			for (ptr2 = mgrp->mh_head.next;
			     ptr2 != &mgrp->mh_head; ptr2 = ptr2->next)
			{
				mh = clist_entry(ptr2, igsc_mh_t, mh_list);

				list->sdb_entry[index].mgrp_ip = mgrp->mgrp_ip;
				list->sdb_entry[index].mh_ip = mh->mh_ip;
				strncpy(list->sdb_entry[index].if_name,
				        DEV_IFNAME(mh->mh_mi->mi_ifp), 16);
				index++;
			}
		}
	}

	list->num_entries = index;

	return (SUCCESS);
}

/*
 * Description: This function is called to delete the IGSDB
 *              an interface
 *
 * Input:       igsc_info - igsc info pointer
 *		ifp       - interface pointer
 *
 * Return:      SUCCESS or FAILURE
 */
int32
igsc_sdb_interface_del(igsc_info_t *igsc_info, void *ifp)
{
	uint32 i;
	igsc_mgrp_t *mgrp;
	igsc_mh_t *mh;
	clist_head_t *ptr1, *ptr2, *tmp1, *tmp2;

	if (igsc_info == NULL)
	{
		IGS_ERROR("Invalid IGSC handle passed\n");
		return (FAILURE);
	}

	if (ifp == NULL)
	{
		IGS_ERROR("Invalid interface input\n");
		return (FAILURE);
	}


	OSL_LOCK(igsc_info->sdb_lock);

	/* Delete all the group entries */
	for (i = 0; i < IGSDB_HASHT_SIZE; i++)
	{
		for (ptr1 = igsc_info->mgrp_sdb[i].next;
		     ptr1 != &igsc_info->mgrp_sdb[i]; ptr1 = tmp1)
		{
			mgrp = clist_entry(ptr1, igsc_mgrp_t, mgrp_hlist);

			/* Delete the host and interface entries */
			for (ptr2 = mgrp->mh_head.next; ptr2 != &mgrp->mh_head; ptr2 = tmp2)
			{
				mh = clist_entry(ptr2, igsc_mh_t, mh_list);

				if (mh->mh_mi->mi_ifp != ifp) {
					tmp2  = ptr2->next;
					continue;
				}

				/* Delete the interface entry if no stream is going on it */
				if (--mh->mh_mi->mi_ref == 0)
				{
					IGS_IGSDB("Deleting interface entry %p\n", mh->mh_mi);

					/* Delete the MFDB entry if no more members of the group
					 * are present on the interface.
					 */
					emfc_mfdb_membership_del(igsc_info->emf_handle,
					                         mh->mh_mgrp->mgrp_ip,
					                         mh->mh_mi->mi_ifp);
					clist_delete(&mh->mh_mi->mi_list);
					MFREE(igsc_info->osh, mh->mh_mi, sizeof(igsc_mi_t));
				}

				osl_timer_del(mh->mgrp_timer);
				tmp2 = ptr2->next;
				clist_delete(ptr2);
				MFREE(igsc_info->osh, mh, sizeof(igsc_mh_t));
			}

			/* If the member being deleted is last node in the host list,
			 * delete the group entry also.
			 */
			tmp1 = ptr1->next;
			if (clist_empty(&mgrp->mh_head))
			{
				IGS_IGSDB("Deleting group entry of %d.%d.%d.%d too\n",
					(mgrp->mgrp_ip >> 24),
					((mgrp->mgrp_ip >> 16) & 0xff),
					((mgrp->mgrp_ip >> 8) & 0xff),
					(mgrp->mgrp_ip & 0xff));

				clist_delete(ptr1);
				MFREE(igsc_info->osh, mgrp, sizeof(igsc_mgrp_t));
			}
		}
	}

	OSL_UNLOCK(igsc_info->sdb_lock);

	return (SUCCESS);
}

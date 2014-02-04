/*
 * Efficient Multicast Forwarding Layer: This module does the efficient
 * layer 2 forwarding of multicast streams, i.e., forward the streams
 * only on to the ports that have corresponding group members there by
 * reducing the bandwidth utilization and latency. It uses the information
 * updated by IGMP Snooping layer to do the optimal forwarding. This file
 * contains the common code routines of EMFL.
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emfc.c 390728 2013-03-13 11:20:19Z $
 */
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/bcmip.h>
#include <osl.h>
#if defined(linux)
#include <osl_linux.h>
#elif defined(__ECOS)
#include <osl_ecos.h>
#else /* defined(osl_xx) */
#error "Unsupported osl"
#endif /* defined(osl_xx) */
#include <bcmutils.h>
#include "clist.h"
#include "emf_cfg.h"
#include "emfc_export.h"
#include "emfc.h"
#include "emf_export.h"

static CLIST_DECL_INIT(emfc_list_head);
static osl_lock_t emfc_list_lock;

static emfc_info_t *
emfc_instance_find(char *inst_id)
{
	emfc_info_t *emfc;
	clist_head_t *ptr;

	if (inst_id == NULL)
	{
		EMF_ERROR("Invalid instance id string\n");
		return (NULL);
	}

	OSL_LOCK(emfc_list_lock);

	for (ptr = emfc_list_head.next; ptr != &emfc_list_head;
	     ptr = ptr->next)
	{
		emfc = clist_entry(ptr, emfc_info_t, emfc_list);

		if (strcmp(inst_id, emfc->inst_id) == 0)
		{
			OSL_UNLOCK(emfc_list_lock);
			return (emfc);
		}
	}

	OSL_UNLOCK(emfc_list_lock);

	return (NULL);
}

/*
 * Description: This function is called by the IGMP snooper layer to
 *              register snooper instance with EMFL.
 *
 * Input:       inst_id - Instance identier used to associate EMF
 *                        and IGMP snooper instances.
 *              emfc    - EMFL Common code global instance handle
 *              snooper - Contains snooper specific parameters and
 *                        event callback functions. These functions
 *                        are called by EMFL on events like IGMP
 *                        packet receive, EMF enable and disable.
 *                        The snooper parameter needs to global or
 *                        persistent.
 *
 * Return:      SUCCESS or FAILURE
 */
int32
emfc_igmp_snooper_register(int8 *inst_id, emfc_info_t **emfc, emfc_snooper_t *snooper)
{
	/* Invalid input */
	if (snooper == NULL)
	{
		EMF_ERROR("Snooper parameter should be non NULL\n");
		return (FAILURE);
	}

	if (emfc == NULL)
	{
		EMF_ERROR("EMF handle should be non NULL\n");
		return (FAILURE);
	}

	/* Validate the instance id */
	if ((*emfc = emfc_instance_find(inst_id)) == NULL)
	{
		EMF_ERROR("EMF Instance doesn't exist\n");
		return (FAILURE);
	}

	if (snooper->input_fn == NULL)
	{
		EMF_ERROR("Snooper input function should be non NULL\n");
		return (FAILURE);
	}

	(*emfc)->snooper = snooper;

	return (SUCCESS);
}

/*
 * Description: This function is called by the IGMP snooper layer
 *              to unregister snooper instance.
 *
 * Input:       handle  - Handle returned during registration.
 *              snooper - Contains snooper specific parameters and
 */
void
emfc_igmp_snooper_unregister(emfc_info_t *emfc)
{
	if (emfc == NULL)
	{
		EMF_ERROR("Unregistering using invalid handle\n");
		return;
	}

	emfc->snooper = NULL;

	return;
}

/*
 * Description: This function handles the unregistered frame forwarding.
 *              IGMP frames are forwarded on router ports. Data frames
 *              are flooded on to user configured UFFP.
 *
 * Input:       emfc    - EMFC Global instance data
 *              sdu     - Pointer to packet buffer.
 *              ifp     - Interface on which the packet arrived.
 *              dest_ip - Multicast destination IP address
 *              rt_port - TRUE when the packet is received from IP
 *                        Stack otherwise FALSE.
 *
 * Return:      EMF_TAKEN - EMF has taken the ownership of the packet.
 *              EMF_NOP   - No processing needed by EMF, just return
 *                          the packet back.
 *              EMF_DROP  - Drop and free the packet.
 */
static uint32
emfc_unreg_frame_handle(emfc_info_t *emfc, void *sdu, void *ifp, uint8 proto,
                        uint32 dest_ip, bool rt_port)
{
	emfc_iflist_t *ptr;
	void *sdu_clone;
	uint32 mcast_flooded = 0;

	/* Forward the frame on to router port */
	if (!rt_port)
	{
		EMF_DEBUG("Sending frame on to router port\n");

		if ((sdu_clone = PKTDUP(emfc->osh, sdu)) == NULL)
		{
			EMFC_PROT_STATS_INCR(emfc, proto, igmp_frames_dropped,
			                     mcast_data_dropped);
			return (EMF_DROP);
		}

		emfc->wrapper.sendup_fn(emfc->emfi, sdu_clone);

		EMFC_PROT_STATS_INCR(emfc, proto, igmp_frames_sentup,
		                     mcast_data_sentup);
	}

	OSL_LOCK(emfc->iflist_lock);

	/* Flood the frame on to user specified ports */
	for (ptr = emfc->iflist_head; ptr != NULL; ptr = ptr->next)
	{
		/* Dont forward the frame on to the port on which it
		 * was received.
		 */
		if (ifp == ptr->ifp)
			continue;

		if (proto == IP_PROT_IGMP)
		{
			/* Dont forward IGMP frame if the port is not router port */
			if (ptr->rtport_ref == 0)
				continue;
		}
		else
		{
			/* Dont forward data frame if the port is neither router
			 * port nor uffp
			 */
			if ((ptr->rtport_ref + ptr->uffp_ref) == 0)
				continue;
		}

		if ((sdu_clone = PKTDUP(emfc->osh, sdu)) == NULL)
		{
			EMFC_PROT_STATS_INCR(emfc, proto, igmp_frames_dropped,
			                     mcast_data_dropped);
			OSL_UNLOCK(emfc->iflist_lock);
			return (EMF_DROP);
		}

		if (emfc->wrapper.forward_fn(emfc->emfi, sdu_clone, dest_ip,
		                ptr->ifp, rt_port) != SUCCESS)
		{
			EMF_INFO("Unable to flood the unreg frame on to %s\n",
			         DEV_IFNAME(ptr->ifp));
			EMFC_PROT_STATS_INCR(emfc, proto, igmp_frames_dropped,
			                     mcast_data_dropped);
		}

		mcast_flooded++;
	}

	OSL_UNLOCK(emfc->iflist_lock);

	if (mcast_flooded > 0)
	{
		EMFC_PROT_STATS_INCR(emfc, proto, igmp_frames_fwd,
		                     mcast_data_flooded);
	}
	else
	{
		if (rt_port)
		{
			EMFC_PROT_STATS_INCR(emfc, proto, igmp_frames_dropped,
			                     mcast_data_dropped);
		}
	}

	PKTFREE(emfc->osh, sdu, FALSE);

	return (EMF_TAKEN);
}

/*
 * Description: This function is called from the registered OS hook
 *              points once for every frame. This function does the
 *              MFDB lookup, packet cloning and frame forwarding.
 *
 * Input:       emfc    - EMFC Global instance data
 *              sdu     - Pointer to packet buffer.
 *              ifp     - Interface on which the packet arrived.
 *              iph     - Pointer to start of IP header.
 *              rt_port - TRUE when the packet is received from IP
 *                        Stack otherwise FALSE.
 *
 * Return:      EMF_NOP   - No processing needed by EMF, just return
 *                          the packet back.
 *              EMF_TAKEN - EMF has taken the ownership of the packet.
 *              EMF_DROP  - Drop and free the packet.
 */
uint32
emfc_input(emfc_info_t *emfc, void *sdu, void *ifp, uint8 *iph, bool rt_port)
{
	uint32 dest_ip;

	EMF_DEBUG("Received frame with dest ip %d.%d.%d.%d\n",
	          iph[IPV4_DEST_IP_OFFSET], iph[IPV4_DEST_IP_OFFSET + 1],
	          iph[IPV4_DEST_IP_OFFSET + 2], iph[IPV4_DEST_IP_OFFSET + 3]);

	dest_ip = ntoh32(*((uint32 *)(iph + IPV4_DEST_IP_OFFSET)));

	/* No processing is required if the received frame is unicast or
	 * broadcast, when EMF is disabled. Send the frame back to bridge.
	 */
	if ((!IP_ISMULTI(dest_ip)) || (!emfc->emf_enable))
	{
		EMF_DEBUG("Unicast frame recevied/EMF disabled\n");
		return (EMF_NOP);
	}

	/* Non-IPv4 multicast packets are not handled */
	if (IP_VER(iph) != IP_VER_4)
	{
		EMF_INFO("Non-IPv4 multicast packets will be flooded\n");
		return (EMF_NOP);
	}

	/* Check the protocol type of multicast frame */
	if ((IPV4_PROT(iph) == IP_PROT_IGMP) && (emfc->snooper != NULL))
	{
		int32 action;

		EMF_DEBUG("Received IGMP frame type %d\n", *(iph + IPV4_HLEN(iph)));

		EMFC_STATS_INCR(emfc, igmp_frames);

		/* IGMP packet received from LAN or IP Stack. Call the IGMP
		 * Snooping function. Based on the IGMP packet type it may
		 * add/delete MFDB entry.  Also the function return value
		 * tells whether to drop, forward, or flood the frame.
		 */
		ASSERT(emfc->snooper->input_fn);
		action = emfc->snooper->input_fn(emfc->snooper, ifp, iph,
		                                 iph + IPV4_HLEN(iph), rt_port);

		switch (action)
		{
			case EMF_DROP:
				EMF_INFO("Dropping the IGMP frame\n");
				return (EMF_DROP);

			case EMF_SENDUP:
				emfc->wrapper.sendup_fn(emfc->emfi, sdu);
				EMFC_STATS_INCR(emfc, igmp_frames_sentup);
				return (EMF_TAKEN);

			case EMF_FORWARD:
				return (emfc_unreg_frame_handle(emfc, sdu, ifp,
				                                IPV4_PROT(iph),
				                                dest_ip, rt_port));

			case EMF_FLOOD:
				EMF_DEBUG("Returning the IGMP frame to bridge\n");
				EMFC_STATS_INCR(emfc, igmp_frames_fwd);
				break;

			default:
				EMF_ERROR("Unknown return value from IGMP Snooper\n");
				EMFC_STATS_INCR(emfc, igmp_frames_fwd);
				break;
		}

		return (EMF_NOP);
	}
	else
	{
		clist_head_t *ptr;
		emfc_mgrp_t *mgrp;
		emfc_mi_t *mi;
		void *sdu_clone;

		EMF_DEBUG("Received frame with proto %d\n", IPV4_PROT(iph));

		EMFC_STATS_INCR(emfc, mcast_data_frames);

		/* Packets with destination IP address in the range 224.0.0.x
		 * must be forwarded on all ports.
		 */
		if (MCAST_ADDR_LINKLOCAL(dest_ip))
		{
			EMF_DEBUG("Flooding the frames with link-local address\n");
			return (EMF_NOP);
		}

		OSL_LOCK(emfc->fdb_lock);

		/* Do the MFDB lookup to determine the destination port(s)
		 * to forward the frame.
		 */
		mgrp = emfc_mfdb_group_find(emfc, dest_ip);

		/* If no matching MFDB entry is found send the frame back to 
		 * bridge module so that it floods on to all the ports.
		 */
		if (mgrp == NULL)
		{
			OSL_UNLOCK(emfc->fdb_lock);
			/* UPnP specific  protocol traffic such as SSDP must be forwarded on to all
			 * the ports.
			 */
			if (MCAST_ADDR_UPNP_SSDP(dest_ip))
			{
				EMF_DEBUG("Flooding the frames with ssdp address\n");
				return (EMF_NOP);
			}

			EMF_DEBUG("MFDB Group entry not found\n");
			return (emfc_unreg_frame_handle(emfc, sdu, ifp,
			                                IPV4_PROT(iph),
			                                dest_ip, rt_port));
		}

#ifdef PLC
		/* Clone the packet and send it to bridge layer if the packet is
		 * from PLC network (wmf0) and the group address is found in br0.
		 * This is to support simultaneous video playing on wired and
		 * wireless clients when the packet is from PLC network.
		 */
		if (strcmp(emfc->inst_id, "wmf0") == 0)
		{
			void *sdu_clone1;
			emfc_info_t *emfc1;

			if ((sdu_clone1 = PKTDUP(emfc->osh, sdu)) == NULL)
			{
				OSL_UNLOCK(emfc->fdb_lock);
				EMFC_STATS_INCR(emfc, mcast_data_dropped);
				return (EMF_DROP);
			}

			emfc1 = emfc_instance_find("br0");

			if (emfc1 && emfc_mfdb_group_find(emfc1, dest_ip))
			{
				emfc->wrapper.sendup_fn(emfc->emfi, sdu_clone1);

				EMFC_PROT_STATS_INCR(emfc, IPV4_PROT(iph), igmp_frames_sentup,
					mcast_data_sentup);
			}
			else
				PKTFREE(emfc->osh, sdu_clone1, FALSE);
		}
#endif /* PLC */

		ASSERT(!clist_empty(&mgrp->mi_head));

		/* If the data frame is received from one of the bridge
		 * ports then a copy has to be sent up to the router port.
		 * Send up data frames only when allowed to do so. For
		 * performance reasons (to avoid pkt copy in wmf) default
		 * is to not send up.
		 */
		if (!rt_port && emfc->mc_data_ind)
		{
			EMF_DEBUG("Sending to router port\n");

			if ((sdu_clone = PKTDUP(emfc->osh, sdu)) == NULL)
			{
				OSL_UNLOCK(emfc->fdb_lock);
				EMFC_STATS_INCR(emfc, mcast_data_dropped);
				return (EMF_DROP);
			}

			emfc->wrapper.sendup_fn(emfc->emfi, sdu_clone);
			EMFC_STATS_INCR(emfc, mcast_data_sentup);
		}

		/* Data frame received from LAN or IP Stack (WAN). Clone 
		 * the buffer and send on all but the last interface.
		 */
		for (ptr = mgrp->mi_head.next;
		     ptr != mgrp->mi_head.prev; ptr = ptr->next)
		{
			mi = clist_entry(ptr, emfc_mi_t, mi_list);

			/* Dont forward the frame on to the port on which it
			 * was received.
			 */
			if (ifp == mi->mi_mhif->mhif_ifp)
				continue;

			EMF_DEBUG("Cloning the buffer for forwarding\n");

			if ((sdu_clone = PKTDUP(emfc->osh, sdu)) == NULL)
			{
				OSL_UNLOCK(emfc->fdb_lock);
				EMFC_STATS_INCR(emfc, mcast_data_dropped);
				return (EMF_DROP);
			}

			emfc->wrapper.forward_fn(emfc->emfi, sdu_clone,
			            dest_ip, mi->mi_mhif->mhif_ifp, rt_port) ?
			            EMFC_STATS_INCR(emfc, mcast_data_dropped) :
			            mi->mi_mhif->mhif_data_fwd++,
			            mi->mi_data_fwd++;
		}

		/* Send the last frame without cloning */
		mi = clist_entry(ptr, emfc_mi_t, mi_list);

		/* Dont forward the frame on to the port on which it was received */
	        if (ifp != mi->mi_mhif->mhif_ifp)
		{
			EMF_DEBUG("Sending the original packet buffer\n");

			emfc->wrapper.forward_fn(emfc->emfi, sdu, dest_ip,
			            mi->mi_mhif->mhif_ifp, rt_port) ?
			            EMFC_STATS_INCR(emfc, mcast_data_dropped) :
			            mi->mi_mhif->mhif_data_fwd++,
			            mi->mi_data_fwd++;
		}
		else
		{
			EMF_DEBUG("Freeing the original packet buffer\n");
			PKTFREE(emfc->osh, sdu, FALSE);
		}

		EMFC_STATS_INCR(emfc, mcast_data_fwd);

		OSL_UNLOCK(emfc->fdb_lock);

		return (EMF_TAKEN);
	}
}

/*
 * Description: This function initializes the MFDB. MFDB group
 *              entries are organized as a hash table with chaining
 *              for collision resolution. Each MFDB group entry
 *              points to the chain of MFDB interface entries that are
 *              members of the group.
 *
 * Input:       emfc - EMFL Common code global data handle
 */
static void
emfc_mfdb_init(emfc_info_t *emfc)
{
	int32 i;

	/* Initialize the multicast forwarding database */
	for (i = 0; i < MFDB_HASHT_SIZE; i++)
	{
		clist_init_head(&emfc->mgrp_fdb[i]);
	}

	/* Initialize the multicast interface list. This list contains
	 * all the interfaces that have multicast group members. Entries in
	 * this list are never expired/deleted. Each entry maintains stats
	 * specific to the interface.
	 */
	emfc->mhif_head = NULL;

	return;
}

/*
 * Description: This function does the MFDB lookup to locate interface
 *              entry of the specified group.
 *
 * Input:       emfc - EMFL Common code global data handle
 *              mgrp - Pointer to multicast group entry of MFDB
 *              ifp  - Interface pointer to locate.
 *
 * Return:      Returns pointer to the MFDB interface entry, NULL
 *              otherwise.
 */
static emfc_mi_t *
emfc_mfdb_mi_entry_find(emfc_info_t *emfc, emfc_mgrp_t *mgrp, void *ifp)
{
	emfc_mi_t *mi;
	clist_head_t *ptr;

	ASSERT(mgrp);

	for (ptr = mgrp->mi_head.next;
	     ptr != &mgrp->mi_head; ptr = ptr->next)
	{
		mi = clist_entry(ptr, emfc_mi_t, mi_list);
		if (ifp == mi->mi_mhif->mhif_ifp)
		{
			return (mi);
		}
	}

	return (NULL);
}

/*
 * Description: This function does the MFDB lookup to locate a multicast
 *              group entry.
 *
 * Input:       emfc    - EMFL Common code global data handle
 *              mgrp_ip - Multicast group address of the entry.
 *
 * Return:      Returns NULL is no group entry is found. Otherwise
 *              returns pointer to the MFDB group entry.
 */
static emfc_mgrp_t *
emfc_mfdb_group_find(emfc_info_t *emfc, uint32 mgrp_ip)
{
	uint32 hash;
	emfc_mgrp_t *mgrp;
	clist_head_t *ptr;

	ASSERT(IP_ISMULTI(mgrp_ip));

	/* Do the cache lookup first. Since the multicast video traffic
	 * is bursty in nature there is a good chance that the cache
	 * hit ratio will be good. If during the testing we find that
	 * the hit ratio is not as good then this single entry cache
	 * mechanism will be removed.
	 */
	if (mgrp_ip == emfc->mgrp_cache_ip)
	{
		EMFC_STATS_INCR(emfc, mfdb_cache_hits);
		return (emfc->mgrp_cache);
	}

	EMFC_STATS_INCR(emfc, mfdb_cache_misses);

	hash = MFDB_MGRP_HASH(mgrp_ip);

	for (ptr = emfc->mgrp_fdb[hash].next;
	     ptr != &emfc->mgrp_fdb[hash];
	     ptr = ptr->next)
	{
		mgrp = clist_entry(ptr, emfc_mgrp_t, mgrp_hlist);

		/* Compare group address */
		if (mgrp_ip == mgrp->mgrp_ip)
		{
			EMF_MFDB("Multicast Group entry %d.%d.%d.%d found\n",
			         (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
			         ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff));
			emfc->mgrp_cache = mgrp;
			emfc->mgrp_cache_ip = mgrp_ip;
			return (mgrp);
		}
	}

	return (NULL);
}

/*
 * Add the entry if not present otherwise return the pointer to
 * the entry.
 */
emfc_mhif_t *
emfc_mfdb_mhif_add(emfc_info_t *emfc, void *ifp)
{
	emfc_mhif_t *ptr, *mhif;

	for (ptr = emfc->mhif_head; ptr != NULL; ptr = ptr->next)
	{
		if (ptr->mhif_ifp == ifp)
		{
			return (ptr);
		}
	}

	/* Add new entry */
	mhif = MALLOC(emfc->osh, sizeof(emfc_mhif_t));
	if (mhif == NULL)
	{
		EMF_ERROR("Failed to alloc mem size %d for mhif entry\n",
		          sizeof(emfc_mhif_t));
		return (NULL);
	}

	mhif->mhif_ifp = ifp;
	mhif->mhif_data_fwd = 0;
	mhif->next = emfc->mhif_head;
	emfc->mhif_head = mhif;

	return (mhif);
}

/*
 * Description: This function does the MFDB lookup to locate a interface
 *              entry of the specified multicast group.
 *
 * Input:       emfc    - EMFL Common code global data handle
 *              mgrp_ip - Multicast group IP address of the entry.
 *              ifp     - Pointer to the interface on which the member
 *                        is present.
 *
 * Return:      Returns NULL is no interface entry is found. Otherwise
 *              returns pointer to the MFDB interface entry.
 */
emfc_mi_t *
emfc_mfdb_membership_find(emfc_info_t *emfc, uint32 mgrp_ip, void *ifp)
{
	emfc_mi_t *mi;
	emfc_mgrp_t *mgrp;

	ASSERT(IP_ISMULTI(mgrp_ip));

	OSL_LOCK(emfc->fdb_lock);

	/* Find group entry */
	mgrp = emfc_mfdb_group_find(emfc, mgrp_ip);

	if (mgrp != NULL)
	{
		/* Find interface entry */
		mi = emfc_mfdb_mi_entry_find(emfc, mgrp, ifp);
		if (mi != NULL)
		{
			EMF_MFDB("Interface entry %d.%d.%d.%d:%p found\n",
			         (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
			         ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff), ifp);
			return (mi);
		}
	}

	OSL_UNLOCK(emfc->fdb_lock);

	EMF_MFDB("Interface entry %x %p not found\n", mgrp_ip, ifp);

	return (NULL);
}

/*
 * Description: This function is called by IGMP Snooper when it wants
 *              to add MFDB entry or refresh the entry. This function
 *              is also called by the management application to add a
 *              static MFDB entry.
 *
 *              If the MFDB entry is not present, it allocates group
 *              entry, interface entry and links them together.
 *
 * Input:       Same as above function.
 *
 * Return:      SUCCESS or FAILURE
 */
int32
emfc_mfdb_membership_add(emfc_info_t *emfc, uint32 mgrp_ip, void *ifp)
{
	uint32 hash;
	emfc_mgrp_t *mgrp;
	emfc_mi_t *mi;

	ASSERT(IP_ISMULTI(mgrp_ip));

	OSL_LOCK(emfc->fdb_lock);

	/* If the group entry doesn't exist, add a new entry and update
	 * the member/interface information.
	 */
	mgrp = emfc_mfdb_group_find(emfc, mgrp_ip);

	if (mgrp == NULL)
	{
		/* Allocate and initialize multicast group entry */
		mgrp = MALLOC(emfc->osh, sizeof(emfc_mgrp_t));
		if (mgrp == NULL)
		{
			EMF_ERROR("Failed to alloc mem size %d for group entry\n",
			          sizeof(emfc_mgrp_t));
			OSL_UNLOCK(emfc->fdb_lock);
			return (FAILURE);
		}

		mgrp->mgrp_ip = mgrp_ip;
		clist_init_head(&mgrp->mi_head);

		EMF_MFDB("Adding group entry %d.%d.%d.%d\n",
		         (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
		         ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff));

		/* Add the group entry to hash table */
		hash = MFDB_MGRP_HASH(mgrp_ip);
		clist_add_head(&emfc->mgrp_fdb[hash], &mgrp->mgrp_hlist);
	}
	else
	{
		mi = emfc_mfdb_mi_entry_find(emfc, mgrp, ifp);

		/* Update the ref count */
		if (mi != NULL)
		{
			mi->mi_ref++;
			OSL_UNLOCK(emfc->fdb_lock);
			return (SUCCESS);
		}
	}

	EMF_MFDB("Adding interface entry for interface %p\n", ifp);

	/* Allocate and initialize multicast interface entry */
	mi = MALLOC(emfc->osh, sizeof(emfc_mi_t));
	if (mi == NULL)
	{
		EMF_ERROR("Failed to allocated memory %d for interface entry\n",
		          sizeof(emfc_mi_t));
		if (clist_empty(&mgrp->mi_head))
		{
			clist_delete(&mgrp->mgrp_hlist);
			emfc->mgrp_cache_ip = ((emfc->mgrp_cache == mgrp) ?
			                       0 : emfc->mgrp_cache_ip);
			MFREE(emfc->osh, mgrp, sizeof(emfc_mgrp_t));
		}
		OSL_UNLOCK(emfc->fdb_lock);
		return (FAILURE);
	}

	/* Initialize the multicast interface list entry */
	mi->mi_ref = 1;
	mi->mi_mhif = emfc_mfdb_mhif_add(emfc, ifp);
	mi->mi_data_fwd = 0;

	/* Add the multicast interface entry */
	clist_add_head(&mgrp->mi_head, &mi->mi_list);

	OSL_UNLOCK(emfc->fdb_lock);

	return (SUCCESS);
}

/*
 * Description: This function is called by the IGMP snooper layer
 *              to delete the MFDB entry. It deletes the group
 *              entry also if the interface entry is last in the
 *              group.
 *
 * Input:       Same as above function.
 *
 * Return:      SUCCESS or FAILURE
 */
int32
emfc_mfdb_membership_del(emfc_info_t *emfc, uint32 mgrp_ip, void *ifp)
{
	emfc_mi_t *mi;
	emfc_mgrp_t *mgrp;

	OSL_LOCK(emfc->fdb_lock);

	/* Find group entry */
	mgrp = emfc_mfdb_group_find(emfc, mgrp_ip);

	if (mgrp == NULL)
	{
		OSL_UNLOCK(emfc->fdb_lock);
		return (FAILURE);
	}

	/* Find interface entry */
	mi = emfc_mfdb_mi_entry_find(emfc, mgrp, ifp);

	if (mi == NULL)
	{
		OSL_UNLOCK(emfc->fdb_lock);
		return (FAILURE);
	}

	EMF_MFDB("Deleting MFDB interface entry for interface %p\n", ifp);

	/* Delete the interface entry when ref count reaches zero */
	mi->mi_ref--;
	if (mi->mi_ref != 0)
	{
		OSL_UNLOCK(emfc->fdb_lock);
		return (SUCCESS);
	}
	else
	{
		EMF_MFDB("Deleting interface entry %p\n", mi->mi_mhif->mhif_ifp);
		clist_delete(&mi->mi_list);
	}

	/* If the member being deleted is last node in the interface list, 
	 * delete the group entry also.
	 */
	if (clist_empty(&mgrp->mi_head))
	{
		EMF_MFDB("Deleting group entry of %d.%d.%d.%d too\n",
		         (mgrp_ip >> 24), ((mgrp_ip >> 16) & 0xff),
		         ((mgrp_ip >> 8) & 0xff), (mgrp_ip & 0xff));

		clist_delete(&mgrp->mgrp_hlist);
		emfc->mgrp_cache_ip = ((emfc->mgrp_cache == mgrp) ?
		                       0 : emfc->mgrp_cache_ip);
		MFREE(emfc->osh, mgrp, sizeof(emfc_mgrp_t));
	}

	MFREE(emfc->osh, mi, sizeof(emfc_mi_t));

	OSL_UNLOCK(emfc->fdb_lock);

	return (SUCCESS);
}

/*
 * Description: This function clears the group interval timers and
 *              deletes the group and interface entries of the MFDB.
 *
 * Input:       emfc     - EMFL Common code global data handle
 */
void
emfc_mfdb_clear(emfc_info_t *emfc)
{
	uint32 i;
	emfc_mgrp_t *mgrp;
	emfc_mi_t *mi;
	clist_head_t *ptr1, *ptr2, *tmp1, *tmp2;

	OSL_LOCK(emfc->fdb_lock);

	/* Delete all the group entries */
	for (i = 0; i < MFDB_HASHT_SIZE; i++)
	{
		for (ptr1 = emfc->mgrp_fdb[i].next;
		     ptr1 != &emfc->mgrp_fdb[i]; ptr1 = tmp1)
		{
			mgrp = clist_entry(ptr1, emfc_mgrp_t, mgrp_hlist);

			/* Delete all interface entries */
			for (ptr2 = mgrp->mi_head.next;
			     ptr2 != &mgrp->mi_head; ptr2 = tmp2)
			{
				mi = clist_entry(ptr2, emfc_mi_t, mi_list);

				tmp2 = ptr2->next;
				EMF_MFDB("Deleting interface entry %p\n", mi);
				clist_delete(ptr2);
				MFREE(emfc->osh, mi, sizeof(emfc_mi_t));
			}

			tmp1 = ptr1->next;

			/* Delete the group entry when there are no more interface
			 * entries for this group.
			 */
			clist_delete(ptr1);
			MFREE(emfc->osh, mgrp, sizeof(emfc_mgrp_t));
		}
	}

	emfc->mgrp_cache_ip = 0;

	OSL_UNLOCK(emfc->fdb_lock);

	return;
}

/*
 * EMFC Interface List cleanup
 */
static void
emfc_iflist_clear(emfc_info_t *emfc)
{
	emfc_iflist_t *ptr, *temp;

	OSL_LOCK(emfc->iflist_lock);

	ptr = emfc->iflist_head;
	while (ptr != NULL)
	{
		temp = ptr->next;
		MFREE(emfc->osh, ptr, sizeof(emfc_iflist_t));
		ptr = temp;
	}

	emfc->iflist_head = NULL;

	OSL_UNLOCK(emfc->iflist_lock);

	return;
}

/*
 * Description:  This function is called to enable/disable the efficient
 *               multicast forwarding feature. When the config operation
 *               cannot be completed respective status is returned.
 *
 * Input:        emfc - EMFL Common code global data handle
 *
 * Input/Output: cfg  - Pointer to configuration request data. It contains
 *                      the command id, operation type, corresponding
 *                      arguments and output status.
 */
static void
emfc_cfg_emf_enable(emfc_info_t *emfc, emf_cfg_request_t *cfg)
{
	bool emf_enable;

	EMF_DEBUG("Operation type: %d\n", cfg->oper_type);

	switch (cfg->oper_type)
	{
		case EMFCFG_OPER_TYPE_GET:
			*(bool *)cfg->arg = emfc->emf_enable;
			cfg->size = sizeof(bool);
			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_OPER_TYPE_SET:
			/* Enable or disable EMF */
			emf_enable = (*(bool *)cfg->arg ? TRUE : FALSE);
			if (emfc->emf_enable == emf_enable)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size = sprintf(cfg->arg,
				                    "Duplicate configuration request\n");
				break;
			}

			emfc->emf_enable = emf_enable;

			if (emfc->emf_enable)
			{
				/* Register the hooks to start receiving multicast frames */
				if (emfc->wrapper.hooks_register_fn(emfc->emfi) != SUCCESS)
				{
					cfg->status = EMFCFG_STATUS_FAILURE;
					cfg->size = sprintf(cfg->arg,
					                    "Duplicate hooks registration\n");
					break;
				}

				/* Call the registered EMF enable entry point function */
				if (emfc->snooper && emfc->snooper->emf_enable_fn != NULL)
				{
					EMF_DEBUG("Calling the EMF enable function\n");
					emfc->snooper->emf_enable_fn(emfc->snooper);
				}
			}
			else
			{
				/* Call the registered EMF disable entry point function */
				if (emfc->snooper && emfc->snooper->emf_disable_fn != NULL)
				{
					EMF_DEBUG("Calling the EMF disable function\n");
					emfc->snooper->emf_disable_fn(emfc->snooper);
				}

				/* Unregister the packet hooks first */
				emfc->wrapper.hooks_unregister_fn(emfc->emfi);

				/* Cleanup the MFDB entries */
				emfc_mfdb_clear(emfc);

				/* Cleanup the UFFP entries */
				emfc_iflist_clear(emfc);
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		default:
			cfg->status = EMFCFG_STATUS_OPER_UNKNOWN;
			cfg->size = sprintf(cfg->arg, "Unknown operation\n");
			break;
	}

	return;
}

/*
 * Description:  This function is called to enable/disable the sending
 *               multicast packets to host.
 *
 * Input:        emfc - EMFL Common code global data handle
 *
 * Input/Output: cfg  - Pointer to configuration request data. It contains
 *                      the command id, operation type, corresponding
 *                      arguments and output status.
 */
static void
emfc_cfg_mc_data_ind(emfc_info_t *emfc, emf_cfg_request_t *cfg)
{
	EMF_DEBUG("Operation type: %d\n", cfg->oper_type);

	switch (cfg->oper_type)
	{
		case EMFCFG_OPER_TYPE_GET:
			*(bool *)cfg->arg = emfc->mc_data_ind;
			cfg->size = sizeof(bool);
			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_OPER_TYPE_SET:
			emfc->mc_data_ind = (*((bool *)cfg->arg) ? TRUE : FALSE);
			cfg->size = sizeof(bool);
			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		default:
			cfg->status = EMFCFG_STATUS_OPER_UNKNOWN;
			cfg->size = sprintf(cfg->arg, "Unknown operation\n");
			break;
	}
}

/*
 * EMFL Packet Counters/Statistics
 */
int32
emfc_stats_get(emfc_info_t *emfc, emf_stats_t *emfs, uint32 size)
{
	if (emfc == NULL)
	{
		EMF_ERROR("Invalid EMFC handle passed\n");
		return (FAILURE);
	}

	if (emfs == NULL)
	{
		EMF_ERROR("Invalid buffer input\n");
		return (FAILURE);
	}

	if (size < sizeof(emf_stats_t))
	{
		EMF_ERROR("Insufficient buffer size %d\n", size);
		return (FAILURE);
	}

	*emfs = emfc->stats;

	return (SUCCESS);
}

/*
 * MFDB Listing Function
 */
int32
emfc_mfdb_list(emfc_info_t *emfc, emf_cfg_mfdb_list_t *list, uint32 size)
{
	clist_head_t *ptr1, *ptr2;
	emfc_mi_t *mi;
	emfc_mgrp_t *mgrp;
	int32 i, index = 0;

	if (emfc == NULL)
	{
		EMF_ERROR("Invalid EMFC handle passed\n");
		return (FAILURE);
	}

	if (list == NULL)
	{
		EMF_ERROR("Invalid buffer input\n");
		return (FAILURE);
	}

	for (i = 0; i < MFDB_HASHT_SIZE; i++)
	{
		for (ptr1 = emfc->mgrp_fdb[i].next;
		     ptr1 != &emfc->mgrp_fdb[i]; ptr1 = ptr1->next)
		{
			mgrp = clist_entry(ptr1, emfc_mgrp_t, mgrp_hlist);
			for (ptr2 = mgrp->mi_head.next;
			     ptr2 != &mgrp->mi_head; ptr2 = ptr2->next)
			{
				mi = clist_entry(ptr2, emfc_mi_t, mi_list);

				list->mfdb_entry[index].mgrp_ip = mgrp->mgrp_ip;
				strncpy(list->mfdb_entry[index].if_name,
				        DEV_IFNAME(mi->mi_mhif->mhif_ifp), 16);
				list->mfdb_entry[index].pkts_fwd = mi->mi_data_fwd;
				list->mfdb_entry[index].if_ptr = (void *)mi->mi_mhif->mhif_ifp;
				index++;
			}
		}
	}

	/* Update the total number of entries */
	list->num_entries = index;

	return (SUCCESS);
}

/*
 * EMFC Interface List find
 * This function must be called with (emfc->iflist_lock) taken
 */
static emfc_iflist_t *
emfc_iflist_find(emfc_info_t *emfc, void *ifp, emfc_iflist_t **prev)
{
	emfc_iflist_t *ptr;

	*prev = NULL;
	for (ptr = emfc->iflist_head; ptr != NULL;
	     *prev = ptr, ptr = ptr->next)
	{
		if (ptr->ifp == ifp)
		{
			return (ptr);
		}
	}

	return (NULL);
}

/*
 * EMFC Interface List add entry
 */
int32
emfc_iflist_add(emfc_info_t *emfc, void *ifp)
{
	emfc_iflist_t *iflist, *prev;

	if (ifp == NULL)
	{
		EMF_ERROR("Invalid interface identifier\n");
		return (FAILURE);
	}

	OSL_LOCK(emfc->iflist_lock);
	iflist = emfc_iflist_find(emfc, ifp, &prev);
	OSL_UNLOCK(emfc->iflist_lock);

	if (iflist != NULL)
	{
		EMF_DEBUG("Adding duplicate interface entry\n");
		return (FAILURE);
	}

	/* Allocate and initialize UFFP entry */
	iflist = MALLOC(emfc->osh, sizeof(emfc_iflist_t));
	if (iflist == NULL)
	{
		EMF_ERROR("Failed to alloc mem size %d for interface entry\n",
		          sizeof(emfc_iflist_t));
		return (FAILURE);
	}

	iflist->ifp = ifp;
	iflist->uffp_ref = 0;
	iflist->rtport_ref = 0;

	/* Add the UFFP entry to the list */
	OSL_LOCK(emfc->iflist_lock);
	iflist->next = emfc->iflist_head;
	emfc->iflist_head = iflist;
	OSL_UNLOCK(emfc->iflist_lock);

	return (SUCCESS);
}

/*
 * EMFC Interface List delete entry
 */
int32
emfc_iflist_del(emfc_info_t *emfc, void *ifp)
{
	emfc_iflist_t *ptr, *prev;

	if (ifp == NULL)
	{
		EMF_ERROR("Invalid interface identifier\n");
		return (FAILURE);
	}

	OSL_LOCK(emfc->iflist_lock);
	ptr = emfc_iflist_find(emfc, ifp, &prev);
	if (ptr == NULL)
	{
		OSL_UNLOCK(emfc->iflist_lock);
		EMF_ERROR("UFFP entry not found\n");
		return (FAILURE);
	}

	/* Delete the UFFP entry from the list */
	if (prev != NULL)
		prev->next = ptr->next;
	else
		emfc->iflist_head = ptr->next;

	OSL_UNLOCK(emfc->iflist_lock);

	MFREE(emfc->osh, ptr, sizeof(emfc_iflist_t));

	return (SUCCESS);
}

/*
 * UFFP add entry
 */
static int32
emfc_uffp_add(emfc_info_t *emfc, void *ifp)
{
	emfc_iflist_t *iflist, *prev;

	/* Add the interface entry if not present already */
	emfc_iflist_add(emfc, ifp);

	OSL_LOCK(emfc->iflist_lock);
	if ((iflist = emfc_iflist_find(emfc, ifp, &prev)) != NULL)
		iflist->uffp_ref++;
	OSL_UNLOCK(emfc->iflist_lock);

	return (SUCCESS);
}

/*
 * UFFP delete entry
 */
static int32
emfc_uffp_del(emfc_info_t *emfc, void *ifp)
{
	emfc_iflist_t *iflist, *prev;

	OSL_LOCK(emfc->iflist_lock);
	iflist = emfc_iflist_find(emfc, ifp, &prev);
	if (iflist == NULL)
	{
		OSL_UNLOCK(emfc->iflist_lock);
		return (FAILURE);
	}

	/* Delete the interface entry when flags is zero */
	iflist->uffp_ref--;
	if ((iflist->uffp_ref == 0) && (iflist->rtport_ref == 0))
		emfc_iflist_del(emfc, ifp );
	OSL_UNLOCK(emfc->iflist_lock);

	return (SUCCESS);
}

/*
 * UFFP Interface Listing
 */
static int32
emfc_uffp_list(emfc_info_t *emfc, emf_cfg_uffp_list_t *list, uint32 size)
{
	int32 index = 0, bytes = 0;
	emfc_iflist_t *ptr;

	for (ptr = emfc->iflist_head; ptr != NULL; ptr = ptr->next)
	{
		if (ptr->uffp_ref == 0)
			continue;

		bytes += sizeof(emf_cfg_uffp_t);
		if (bytes > size)
			return (FAILURE);

		strncpy(list->uffp_entry[index].if_name, DEV_IFNAME(ptr->ifp), 16);
		list->uffp_entry[index].if_name[15] = 0;
		index++;
	}

	/* Update the total number of entries */
	list->num_entries = index;

	return (SUCCESS);
}

/*
 * Description: This function is called by the IGMP Snooper to add a Router
 *              Port. Router Port is the interface on which the IGMP Snooper
 *              determines that a multicast router is present. We set a bit
 *              in the flag field of the interface list entry to mark it as
 *              router port.
 *
 * Input:       emfc - EMFC Global Instance handle
 *              ifp  - Interface pointer
 *
 * Return:      SUCCESS/FAILURE
 */
int32
emfc_rtport_add(emfc_info_t *emfc, void *ifp)
{
	emfc_iflist_t *iflist, *prev;

	/* Add interface list entry */
	emfc_iflist_add(emfc, ifp);

	OSL_LOCK(emfc->iflist_lock);
	if ((iflist = emfc_iflist_find(emfc, ifp, &prev)) != NULL)
		iflist->rtport_ref++;
	OSL_UNLOCK(emfc->iflist_lock);

	EMF_INFO("RTPORT %s added refcount %d\n", DEV_IFNAME(ifp), iflist->rtport_ref);

	return (SUCCESS);
}

/*
 * Description: This function is called by the IGMP Snooper to delete a
 *              Router Port. We clear the corresponding bit in the flags
 *              field to mark the port as non-router port.
 *
 * Input:       emfc - EMFC Global Instance handle
 *              ifp  - Interface pointer
 *
 * Return:      SUCCESS/FAILURE
 */
int32
emfc_rtport_del(emfc_info_t *emfc, void *ifp)
{
	emfc_iflist_t *iflist, *prev;

	OSL_LOCK(emfc->iflist_lock);
	if ((iflist = emfc_iflist_find(emfc, ifp, &prev)) == NULL)
	{
		OSL_UNLOCK(emfc->iflist_lock);
		EMF_ERROR("Invalid interface specified to rtport delete\n");
		return (FAILURE);
	}

	/* Delete the interface entry when flags is zero */
	iflist->rtport_ref--;
	if ((iflist->rtport_ref == 0) && (iflist->uffp_ref == 0))
		emfc_iflist_del(emfc, ifp);
	OSL_UNLOCK(emfc->iflist_lock);

	return (SUCCESS);
}

/*
 * RTPORT listing function
 */
static int32
emfc_rtport_list(emfc_info_t *emfc, emf_cfg_rtport_list_t *list, uint32 size)
{
	int32 index = 0, bytes = 0;
	emfc_iflist_t *ptr;

	for (ptr = emfc->iflist_head; ptr != NULL; ptr = ptr->next)
	{
		if (ptr->rtport_ref == 0)
			continue;

		bytes += sizeof(emf_cfg_rtport_t);
		if (bytes > size)
			return (FAILURE);

		strncpy(list->rtport_entry[index].if_name, DEV_IFNAME(ptr->ifp), 16);
		list->rtport_entry[index].if_name[15] = 0;
		index++;
	}

	/* Update the total number of entries */
	list->num_entries = index;

	return (SUCCESS);
}

/*
 * Description: This function is called from the OS Specific layer when the
 *              user issues a configuration command.
 *
 * Input/Output: Same as emfc_cfg_emf_enable.
 */
void
emfc_cfg_request_process(emfc_info_t *emfc, emf_cfg_request_t *cfg)
{
	emf_cfg_mfdb_t *mfdb;
	emf_cfg_uffp_t *uffp;
	emf_cfg_rtport_t *rtport;

	EMF_DEBUG("Command identifier: %d\n", cfg->command_id);

	switch (cfg->command_id)
	{
		case EMFCFG_CMD_EMF_ENABLE:
			emfc_cfg_emf_enable(emfc, cfg);
			break;

		case EMFCFG_CMD_MFDB_ADD:
			mfdb = (emf_cfg_mfdb_t *)cfg->arg;
			cfg->size = 0;

			/* Add MFDB entry for this group and interface */
			if (emfc_mfdb_membership_add(emfc, mfdb->mgrp_ip,
			                             mfdb->if_ptr) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size += sprintf(cfg->arg, "Unable to add entry\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;

			EMF_MFDB("MFDB entry %x %p added by user\n",
			         mfdb->mgrp_ip, mfdb->if_ptr);
			break;

		case EMFCFG_CMD_MFDB_DEL:
			mfdb = (emf_cfg_mfdb_t *)cfg->arg;
			cfg->size = 0;

			/* Delete MFDB entry */
			if (emfc_mfdb_membership_del(emfc, mfdb->mgrp_ip,
			                             mfdb->if_ptr) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size += sprintf(cfg->arg, "MFDB entry not found\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;

			EMF_MFDB("MFDB entry %x %p deleted by user\n",
			         mfdb->mgrp_ip, mfdb->if_ptr);
			break;

		case EMFCFG_CMD_MFDB_LIST:
			if (emfc_mfdb_list(emfc, (emf_cfg_mfdb_list_t *)cfg->arg,
			                   cfg->size) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size += sprintf(cfg->arg, "MFDB list get failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_MFDB_CLEAR:
			emfc_mfdb_clear(emfc);
			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_RTPORT_ADD:
			rtport = (emf_cfg_rtport_t *)cfg->arg;
			cfg->size = 0;

			if (emfc_rtport_add(emfc, rtport->if_ptr) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size += sprintf(cfg->arg,
				                     "Unknown interface, rtport add failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_RTPORT_DEL:
			rtport = (emf_cfg_rtport_t *)cfg->arg;
			cfg->size = 0;

			if (emfc_rtport_del(emfc, rtport->if_ptr) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size += sprintf(cfg->arg,
				                     "Unknown interface, rtport del failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_RTPORT_LIST:
			if (emfc_rtport_list(emfc, (emf_cfg_rtport_list_t *)cfg->arg,
			                     cfg->size) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size = sprintf(cfg->arg, "rtport list get failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_UFFP_ADD:
			uffp = (emf_cfg_uffp_t *)cfg->arg;
			cfg->size = 0;

			if (emfc_uffp_add(emfc, uffp->if_ptr) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size += sprintf(cfg->arg,
				                     "Unknown interface, UFFP add failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_UFFP_DEL:
			uffp = (emf_cfg_uffp_t *)cfg->arg;
			cfg->size = 0;

			if (emfc_uffp_del(emfc, uffp->if_ptr) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size += sprintf(cfg->arg,
				                     "Unknown interface, UFFP del failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_UFFP_LIST:
			if (emfc_uffp_list(emfc, (emf_cfg_uffp_list_t *)cfg->arg,
			                   cfg->size) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size = sprintf(cfg->arg, "UFFP list get failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_EMF_STATS:
			if (emfc_stats_get(emfc, (emf_stats_t *)cfg->arg,
			                   cfg->size) != SUCCESS)
			{
				cfg->status = EMFCFG_STATUS_FAILURE;
				cfg->size = sprintf(cfg->arg, "EMF stats get failed\n");
				break;
			}

			cfg->status = EMFCFG_STATUS_SUCCESS;
			break;

		case EMFCFG_CMD_MC_DATA_IND:
			emfc_cfg_mc_data_ind(emfc, cfg);
			break;

		default:
			EMF_DEBUG("Unknown command %d\n", cfg->command_id);
			cfg->status = EMFCFG_STATUS_CMD_UNKNOWN;
			cfg->size = sprintf(cfg->arg, "Unknown command\n");
			break;
	}

	return;
}

/*
 * Description: This function is called from the OS specific module init
 *              routine to create and initialize EMFC instance. This function
 *              primarily initializes the EMFL global data and MFDB.
 *
 * Input:       inst_id - Instance identier used to associate EMF
 *                        and IGMP snooper instances.
 *              emfi    - EMFL OS Specific global data handle
 *              osh     - OS abstraction layer handle
 *              wrapper - EMFC wrapper info
 *
 * Return:      emfc    - EMFL Common code global data handle
 */
emfc_info_t *
emfc_init(int8 *inst_id, void *emfi, osl_t *osh, emfc_wrapper_t *wrapper)
{
	emfc_info_t *emfc;

	EMF_DEBUG("Initializing EMFL\n");

	/* Check for the wrapper parameter */
	if (wrapper == NULL)
	{
		EMF_ERROR("emfc_init: wrapper parameter NULL\n");
		return (NULL);
	}

	/* Allocate memory */
	emfc = MALLOC(osh, sizeof(emfc_info_t));
	if (emfc == NULL)
	{
		EMF_ERROR("Failed to allocated memory size %d for MFL\n",
		          sizeof(emfc_info_t));
		return (NULL);
	}

	EMF_DEBUG("Allocated memory for EMFC info\n");

	/* Initialize the EMF global data */
	bzero(emfc, sizeof(emfc_info_t));
	emfc->osh = osh;
	emfc->emfi = emfi;
	emfc->mgrp_cache_ip = 0;
	emfc->mgrp_cache = NULL;
	emfc->iflist_head = NULL;

	/* Set EMF status as disabled */
	emfc->emf_enable = FALSE;

	/* Initialize Multicast FDB */
	emfc_mfdb_init(emfc);

	/* Create lock for MFDB access */
	emfc->fdb_lock = OSL_LOCK_CREATE("FDB Lock");
	if (emfc->fdb_lock == NULL)
	{
		MFREE(emfc->osh, emfc, sizeof(emfc_info_t));
		return (NULL);
	}

	/* Create lock for router port list access */
	emfc->iflist_lock = OSL_LOCK_CREATE("Router Port List Lock");
	if (emfc->iflist_lock == NULL)
	{
		OSL_LOCK_DESTROY(emfc->fdb_lock);
		MFREE(emfc->osh, emfc, sizeof(emfc_info_t));
		return (NULL);
	}

	/* Save the instance id */
	strncpy(emfc->inst_id, inst_id, IFNAMSIZ);
	emfc->inst_id[IFNAMSIZ - 1] = 0;

	/* Fill up the wrapper specific functions */
	emfc->wrapper.forward_fn = wrapper->forward_fn;
	emfc->wrapper.sendup_fn = wrapper->sendup_fn;
	emfc->wrapper.hooks_register_fn = wrapper->hooks_register_fn;
	emfc->wrapper.hooks_unregister_fn = wrapper->hooks_unregister_fn;

	/* Add to the EMFC instance list */
	OSL_LOCK(emfc_list_lock);
	clist_add_head(&emfc_list_head, &emfc->emfc_list);
	OSL_UNLOCK(emfc_list_lock);

	EMF_DEBUG("Initialized MFDB\n");

	return (emfc);
}

/*
 * Description: This function is called from OS specific module cleanup
 *              routine. This routine primarily clears the MFDB entries
 *              and frees the global instance data.
 *
 * Input:       emfc - EMFL global instance handle
 */
void
emfc_exit(emfc_info_t *emfc)
{
	emfc_mhif_t *ptr, *temp;

	/* Unregister the packet hooks if not already */
	emfc->wrapper.hooks_unregister_fn(emfc->emfi);

	/* Cleanup MFDB entries */
	emfc_mfdb_clear(emfc);

	/* Cleanup the interface list entries */
	emfc_iflist_clear(emfc);
	OSL_LOCK_DESTROY(emfc->iflist_lock);

	OSL_LOCK(emfc->fdb_lock);

	/* Delete interface list */
	ptr = emfc->mhif_head;
	while (ptr != NULL)
	{
		temp = ptr->next;
		MFREE(emfc->osh, ptr, sizeof(emfc_mhif_t));
		ptr = temp;
	}

	OSL_UNLOCK(emfc->fdb_lock);

	OSL_LOCK_DESTROY(emfc->fdb_lock);

	/* Delete the EMFC instance */
	OSL_LOCK(emfc_list_lock);
	clist_delete(&emfc->emfc_list);
	OSL_UNLOCK(emfc_list_lock);

	MFREE(emfc->osh, emfc, sizeof(emfc_info_t));

	EMF_DEBUG("Cleaned up EMFL, exiting common code\n");

	return;
}

/*
 * Description: This function is called from OS specific module init
 *              routine. This allocates global resources required by the
 *              common code.
 */
int32
emfc_module_init(void)
{
	/* Create lock for EMFC instance list access */
	emfc_list_lock = OSL_LOCK_CREATE("EMFC List Lock");

	if (emfc_list_lock == NULL)
	{
		EMF_ERROR("EMFC List lock create failed\n");
		return (FAILURE);
	}

	return (SUCCESS);
}

/*
 * Description: This function is called from OS specific module cleanup
 *              routine. This frees all the global resources.
 */
void
emfc_module_exit(void)
{
	OSL_LOCK_DESTROY(emfc_list_lock);
	return;
}

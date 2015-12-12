/*
 * Private header file for event-component data structure and API
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

/*
 *
 *  Here is a picture of the data structure that's maintained.
 *  The list is circular for efficiency: FIFO insert is constant time.
 *  It's traversed FIFO to signal interest.
 *  Any client may ask to be removed.
 *
 *
 *  --------------------------------------------+
 *  |                                           |
 *  |  +-------+      +-------+      +-------+  |
 *  +->| x c n |----->| x c n |----->| x c n |---
 *     +-------+      +-------+      +-------+
 *                                   ^
 *                                   |
 *                                +------+
 *                       listp--->| tail |
 *                                +------+
 *
 *
 *        x is client passthru data for notifications
 *        c is the client callback
 *        n is a "next" pointer of the list
 *        tail is the server's pointer to the list.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

#ifndef BCM_NOTIF_PRIV_H
#define BCM_NOTIF_PRIV_H 1

#include <osl.h>
#include <bcm_notif_pub.h>
#include <bcm_mpool_pub.h>


/* Global notifier module state */
struct bcm_notif_module {

	/* Operating system handle. */
	osl_t		*osh;

	/* Memory pool manager handle. */
	bcm_mpm_mgr_h	mpm;

	/* Memory pool for server notifier objects. */
	bcm_mp_pool_h	server_mem_pool;

	/* Memory pool for client request objects. */
	bcm_mp_pool_h	client_mem_pool;
};


/*
 * Private helper type for nodes in the linked list of client registrations
 */
struct bcm_notif_client_request {
	bcm_notif_client_data		passthru;
	bcm_notif_client_callback	callback;
	struct bcm_notif_client_request	*next;
};

/*
 * This type defines the overall list. The list is kept circular. This way
 * We can insert to tail easily, and invoke client callbacks in exactly the same
 * order in which they express interest.
 */
struct bcm_notif_list_struct {
	/* Circular list of client requests. */
	struct bcm_notif_client_request	*tail;

	/* Pointer to global notif state. */
	bcm_notif_module_t		*notif_module;

	/* Used to ensure that nested events do not occur, and that list operations
	 * are not performed within client callbacks.
	 */
	bool				allow_list_operations;
};

#endif /* BCM_NOTIF_PRIV_H */

/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#ifndef OSL_H
#define OSL_H

#include "util.h"

/* Operating-System Layer.
 * Hides details of underlying OS details beneath this consistent API.
 */

struct osl_st
{
    char *	responder_if;	/* interface we're handling */
    char *      wireless_if;    /* name of wireless interface, may be different, as in WRT (eth1 vs br0) */
    int		sock;		/* fd for topology frames, or -1 if not open */
};

/* Private state needed to handle the OS. */
typedef struct osl_st osl_t;

#include "tlv.h"

/* Do any start-of-day initialistation, and return private state. */
extern osl_t *osl_init(void);

/* Do anything required to become a long-running background process. */
extern void osl_become_daemon(osl_t *osl);

/* Open "interface", and add packetio_recv_handler(state) as the IO
 * event handler for its packets (or die on failure).  If possible,
 * the OSL should try to set the OS to filter packets so just frames
 * with ethertype == topology protocol are received, but if not the
 * packetio_recv_handler will filter appropriately, so providing more
 * frames than necessary is safe. */
extern void osl_interface_open(osl_t *osl, char *interface, void *state);

/* pidfile maintenance: this is not locking (there's plenty of scope
 * for races here!) but it should stop most accidental runs of two
 * lltdd instances on the same interface.  We open the pidfile for
 * read: if it exists and the named pid is running we abort ourselves.
 * Otherwise we reopen the pidfile for write and log our pid to it. */
extern void osl_write_pidfile(osl_t *osl);

/* Permanently drop elevated privilleges, eg by using POSIX.1e
 * capabilities, or by setuid(nobody) */
extern void osl_drop_privs(osl_t *osl);

/* Turn promiscuous mode on or off. */
extern void osl_set_promisc(osl_t *osl, bool_t promisc);

/* Return the Ethernet address for the interface, or die. */
extern void osl_get_hwaddr(osl_t *osl, /*OUT*/ etheraddr_t *addr);


/* Usual read/write functions; here if they need wrapping */

/* Read from "fd" argument passed into the IO event handler. */
extern ssize_t osl_read(int fd, void *buf, size_t count);

/* Write to opened interface. */
extern ssize_t osl_write(osl_t *osl, const void *buf, size_t count);

#endif /* OSL_H */

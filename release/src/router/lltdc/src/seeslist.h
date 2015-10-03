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

#ifndef SEESLIST_H
#define SEESLIST_H

#include "protocol.h"

typedef struct topo_seeslist_st topo_seeslist_t;

extern topo_seeslist_t *seeslist_new(int count);

extern bool_t seeslist_enqueue(bool_t isARP, etheraddr_t *realsrc);

extern bool_t seeslist_dequeue(/*OUT*/ topo_recvee_desc_t *dest);

extern void seeslist_clear(void);

extern bool_t seeslist_is_empty(void);

extern void seeslist_free(void);

#endif /* SEESLIST_H */

/*
 * Copyright (c) 2005-2009 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/* messages define for BFA_AEN_CAT_ITNIM Module */
#ifndef	__bfa_aen_itnim_h__
#define	__bfa_aen_itnim_h__

#include  <cs/bfa_log.h>
#include  <defs/bfa_defs_aen.h>

#define BFA_AEN_ITNIM_ONLINE \
	BFA_LOG_CREATE_ID(BFA_AEN_CAT_ITNIM, BFA_ITNIM_AEN_ONLINE)
#define BFA_AEN_ITNIM_OFFLINE \
	BFA_LOG_CREATE_ID(BFA_AEN_CAT_ITNIM, BFA_ITNIM_AEN_OFFLINE)
#define BFA_AEN_ITNIM_DISCONNECT \
	BFA_LOG_CREATE_ID(BFA_AEN_CAT_ITNIM, BFA_ITNIM_AEN_DISCONNECT)

#endif

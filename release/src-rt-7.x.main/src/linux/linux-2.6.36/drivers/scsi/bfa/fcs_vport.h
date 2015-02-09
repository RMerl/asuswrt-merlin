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

#ifndef __FCS_VPORT_H__
#define __FCS_VPORT_H__

#include <fcs/bfa_fcs_lport.h>
#include <fcs/bfa_fcs_vport.h>
#include <defs/bfa_defs_pci.h>

void bfa_fcs_vport_cleanup(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_online(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_offline(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_delete_comp(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_fcs_delete(struct bfa_fcs_vport_s *vport);

#endif /* __FCS_VPORT_H__ */

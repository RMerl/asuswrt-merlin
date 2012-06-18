/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef SG_CMDS_H
#define SG_CMDS_H

/********************************************************************
 * This header did contain wrapper declarations for many SCSI commands
 * up until sg3_utils version 1.22 . In that version, the command
 * wrappers were broken into two groups, the 'basic' ones found in the
 * "sg_cmds_basic.h" header and the 'extra' ones found in the
 * "sg_cmds_extra.h" header. This header now simply includes those two
 * headers.
 * The wrappers function definitions are found in the "sg_cmds_basic.c"
 * and "sg_cmds_extra.c" files.
 ********************************************************************/

#include "sg_cmds_basic.h"
#include "sg_cmds_extra.h"

#endif

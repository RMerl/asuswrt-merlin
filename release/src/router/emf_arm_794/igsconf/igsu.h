/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igsu.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _IGSU_H_
#define _IGSU_H_

#define	IGS_ARGC_ADD_BRIDGE             3
#define	IGS_ARGC_DEL_BRIDGE             3
#define	IGS_ARGC_LIST_BRIDGE            3
#define	IGS_ARGC_LIST_IGSDB             3
#define	IGS_ARGC_LIST_RTPORT            3
#define	IGS_ARGC_CLEAR_IGSDB            3
#define	IGS_ARGC_SHOW_STATS             3

#define IGS_USAGE \
"Usage: igs  add   bridge  <bridge>\n"\
"            del   bridge  <bridge>\n"\
"            list  sdb     <bridge>\n"\
"            list  rtport  <bridge>\n"\
"            show  stats   <bridge>\n"

typedef struct igs_cmd_arg
{
	char *cmd_oper_str;             /* Operation type string */
	char *cmd_id_str;               /* Command id string */
	int  (*input)(char *[]);        /* Command process function */
	int  arg_count;                 /* Arguments count */
} igs_cmd_arg_t;


#endif /* _IGSU_H_ */

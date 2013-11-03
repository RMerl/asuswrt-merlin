/*
 *
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 */

#ifndef AFPD_SWITCH_H
#define AFPD_SWITCH_H 1

extern AFPCmd	*afp_switch;
extern AFPCmd	postauth_switch[];

/* switch.c */
#define UAM_AFPSERVER_PREAUTH  (0)
#define UAM_AFPSERVER_POSTAUTH (1 << 0)

extern int uam_afpserver_action (const int /*id*/, const int /*switch*/, 
				     AFPCmd new_table, AFPCmd *old);


#endif

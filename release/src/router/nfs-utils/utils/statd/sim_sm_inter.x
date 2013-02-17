/*
 * Copyright (C) 1995, 1997-1999 Jeffrey A. Uphoff
 * Modified by Olaf Kirch, 1996.
 * Modified by H.J. Lu, 1998.
 *
 * NSM for Linux.
 */

#ifdef RPC_CLNT
%#include <string.h>
#endif

program SIM_SM_PROG { 
	version SIM_SM_VERS  {
		void			 SIM_SM_MON(struct status) = 1;
	} = 1;
} = 200048;

const	SM_MAXSTRLEN = 1024;
const	SM_PRIV_SIZE = 16;

/* 
 * structure of the status message sent back by the status monitor
 * when monitor site status changes
 */
%#ifndef SM_INTER_X
struct status {
	string mon_name<SM_MAXSTRLEN>;
	int state;
	opaque priv[SM_PRIV_SIZE]; /* stored private information */
};
%#endif /* SM_INTER_X */

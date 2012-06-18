/* RPC extension for IP netfilter matching, Version 2.2
 * (C) 2000 by Marcelo Barbosa Lima <marcelo.lima@dcc.unicamp.br>
 *	- original rpc tracking module
 *	- "recent" connection handling for kernel 2.3+ netfilter
 *
 * (C) 2001 by Rusty Russell <rusty@rustcorp.com.au>
 *	- upgraded conntrack modules to oldnat api - kernel 2.4.0+
 *
 * (C) 2002 by Ian (Larry) Latter <Ian.Latter@mq.edu.au>
 *	- upgraded conntrack modules to newnat api - kernel 2.4.20+
 *	- extended matching to support filtering on procedures
 *
 * ipt_rpc.h.c,v 2.2 2003/01/12 18:30:00
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 **
 */

#ifndef _IPT_RPC_H
#define _IPT_RPC_H

struct ipt_rpc_data;

struct ipt_rpc_info {
	int inverse;
	int strict;
	const char c_procs[1408];
	int i_procs;
	struct ipt_rpc_data *data;
};

#endif /* _IPT_RPC_H */

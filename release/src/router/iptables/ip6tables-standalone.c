/*
 * Author: Paul.Russell@rustcorp.com.au and mneuling@radlogic.com.au
 *
 * (C) 2000-2002 by the netfilter coreteam <coreteam@netfilter.org>:
 * 		    Paul 'Rusty' Russell <rusty@rustcorp.com.au>
 * 		    Marc Boucher <marc+nf@mbsi.ca>
 * 		    James Morris <jmorris@intercode.com.au>
 * 		    Harald Welte <laforge@gnumonks.org>
 * 		    Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * Based on the ipchains code by Paul Russell and Michael Neuling
 *
 *	iptables -- IP firewall administration for kernels with
 *	firewall table (aimed for the 2.3 kernels)
 *
 *	See the accompanying manual page iptables(8) for information
 *	about proper usage of this program.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ip6tables.h>

#ifdef IPTABLES_MULTI
int
ip6tables_main(int argc, char *argv[])
#else
int
main(int argc, char *argv[])
#endif
{
	int ret;
	char *table = "filter";
	ip6tc_handle_t handle = NULL;

	program_name = "ip6tables";
	program_version = IPTABLES_VERSION;

	lib_dir = getenv("IP6TABLES_LIB_DIR");
	if (!lib_dir)
		lib_dir = IP6T_LIB_DIR;

#ifdef NO_SHARED_LIBS
	init_extensions();
#endif

	ret = do_command6(argc, argv, &table, &handle);
	if (ret)
		ret = ip6tc_commit(&handle);

	if (!ret)
		fprintf(stderr, "ip6tables: %s\n",
			ip6tc_strerror(errno));

	exit(!ret);
}

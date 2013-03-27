/* 
   Test program for the neon resolver interface
   Copyright (C) 2002-2003, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "config.h"

#include <stdio.h>

#include "ne_socket.h"

int main(int argc, char **argv)
{
    ne_sock_addr *addr;
    char buf[256];
    int ret = 0;

    if (argc < 2) {
	printf("Usage: %s hostname\n", argv[0]);
	return 1;
    }
    
    if (ne_sock_init()) {
	printf("%s: Failed to initialize socket library.\n", argv[0]);
        return 1;
    }

    addr = ne_addr_resolve(argv[1], 0);
    if (ne_addr_result(addr)) {
	printf("Could not resolve `%s': %s\n", argv[1],
	       ne_addr_error(addr, buf, sizeof buf));
	ret = 2;
    } else {
	const ne_inet_addr *ia;
	printf("Resolved `%s' OK:", argv[1]);
	for (ia = ne_addr_first(addr); ia; ia = ne_addr_next(addr)) {
	    printf(" <%s>", ne_iaddr_print(ia, buf, sizeof buf));
	}
	putchar('\n');
    }
    ne_addr_destroy(addr);

    return ret;
}

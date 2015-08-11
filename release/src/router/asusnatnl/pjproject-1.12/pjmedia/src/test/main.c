/* $Id: main.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "test.h"

 
/* Any tests that want to build a linked executable for RTEMS must include
   this header to get a default config for the network stack. */
#if defined(PJ_RTEMS) 
#   include <bsp.h>
#   include <rtems.h>
#   include <rtems/rtems_bsdnet.h>
#   include "../../../pjlib/include/rtems-network-config.h"
#endif


int main(int argc, char *argv[])
{
    int rc;
    char s[10];
    rc = test_main();

    if (argc == 2 && argv[1][0]=='-' && argv[1][1]=='i') {
	puts("\nPress <ENTER> to quit");
	if (fgets(s, sizeof(s), stdin) == NULL)
	    return rc;
    }

    return rc;
}



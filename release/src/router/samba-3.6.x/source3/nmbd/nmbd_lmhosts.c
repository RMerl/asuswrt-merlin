/*
   Unix SMB/CIFS implementation.
   NBT netbios routines and daemon - version 2
   Copyright (C) Jeremy Allison 1994-1998

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Revision History:

   Handle lmhosts file reading.

*/

#include "includes.h"
#include "../libcli/nbt/libnbt.h"
#include "nmbd/nmbd.h"

/****************************************************************************
Load a lmhosts file.
****************************************************************************/

void load_lmhosts_file(const char *fname)
{
	char *name = NULL;
	int name_type;
	struct sockaddr_storage ss;
	TALLOC_CTX *ctx = talloc_init("load_lmhosts_file");
	XFILE *fp = startlmhosts( fname );

	if (!fp) {
		DEBUG(2,("load_lmhosts_file: Can't open lmhosts file %s. Error was %s\n",
			fname, strerror(errno)));
		TALLOC_FREE(ctx);
		return;
	}

	while (getlmhostsent(ctx, fp, &name, &name_type, &ss) ) {
		struct in_addr ipaddr;
		struct subnet_record *subrec = NULL;
		enum name_source source = LMHOSTS_NAME;

		if (ss.ss_family != AF_INET) {
			TALLOC_FREE(name);
			continue;
		}

		ipaddr = ((struct sockaddr_in *)&ss)->sin_addr;

		/* We find a relevent subnet to put this entry on, then add it. */
		/* Go through all the broadcast subnets and see if the mask matches. */
		for (subrec = FIRST_SUBNET; subrec ; subrec = NEXT_SUBNET_EXCLUDING_UNICAST(subrec)) {
			if(same_net_v4(ipaddr, subrec->bcast_ip, subrec->mask_ip))
				break;
		}

		/* If none match add the name to the remote_broadcast_subnet. */
		if(subrec == NULL)
			subrec = remote_broadcast_subnet;

		if(name_type == -1) {
			/* Add the (0) and (0x20) names directly into the namelist for this subnet. */
			(void)add_name_to_subnet(subrec,name,0x00,(uint16)NB_ACTIVE,PERMANENT_TTL,source,1,&ipaddr);
			(void)add_name_to_subnet(subrec,name,0x20,(uint16)NB_ACTIVE,PERMANENT_TTL,source,1,&ipaddr);
		} else {
			/* Add the given name type to the subnet namelist. */
			(void)add_name_to_subnet(subrec,name,name_type,(uint16)NB_ACTIVE,PERMANENT_TTL,source,1,&ipaddr);
		}
	}

	TALLOC_FREE(ctx);
	endlmhosts(fp);
}

/****************************************************************************
  Find a name read from the lmhosts file. We secretly check the names on
  the remote_broadcast_subnet as if the name was added to a regular broadcast
  subnet it will be found by normal name query processing.
****************************************************************************/

bool find_name_in_lmhosts(struct nmb_name *nmbname, struct name_record **namerecp)
{
	struct name_record *namerec;

	*namerecp = NULL;

	if((namerec = find_name_on_subnet(remote_broadcast_subnet, nmbname, FIND_ANY_NAME))==NULL)
		return False;

	if(!NAME_IS_ACTIVE(namerec) || (namerec->data.source != LMHOSTS_NAME))
		return False;

	*namerecp = namerec;
	return True;
}

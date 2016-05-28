/* 
   Unix SMB/CIFS implementation.

   idmap PASSDB backend

   Copyright (C) Simo Sorce 2006
   
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

#include "includes.h"
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/*****************************
 Initialise idmap database. 
*****************************/

static NTSTATUS idmap_nss_int_init(struct idmap_domain *dom)
{	
	dom->initialized = True;
	return NT_STATUS_OK;
}

/**********************************
 lookup a set of unix ids. 
**********************************/

static NTSTATUS idmap_nss_unixids_to_sids(struct idmap_domain *dom, struct id_map **ids)
{
	TALLOC_CTX *ctx;
	int i;

	if (! dom->initialized) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	ctx = talloc_new(dom);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; ids[i]; i++) {
		struct passwd *pw;
		struct group *gr;
		const char *name;
		enum lsa_SidType type;
		BOOL ret;
		
		switch (ids[i]->xid.type) {
		case ID_TYPE_UID:
			pw = getpwuid((uid_t)ids[i]->xid.id);

			if (!pw) {
				ids[i]->status = ID_UNMAPPED;
				continue;
			}
			name = pw->pw_name;
			break;
		case ID_TYPE_GID:
			gr = getgrgid((gid_t)ids[i]->xid.id);

			if (!gr) {
				ids[i]->status = ID_UNMAPPED;
				continue;
			}
			name = gr->gr_name;
			break;
		default: /* ?? */
			ids[i]->status = ID_UNKNOWN;
			continue;
		}

		/* by default calls to winbindd are disabled
		   the following call will not recurse so this is safe */
		winbind_on();
		/* Lookup name from PDC using lsa_lookup_names() */
		ret = winbind_lookup_name(dom->name, name, ids[i]->sid, &type);
		winbind_off();

		if (!ret) {
			/* TODO: how do we know if the name is really not mapped,
			 * or something just failed ? */
			ids[i]->status = ID_UNMAPPED;
			continue;
		}

		switch (type) {
		case SID_NAME_USER:
			if (ids[i]->xid.type == ID_TYPE_UID) {
				ids[i]->status = ID_MAPPED;
			}
			break;

		case SID_NAME_DOM_GRP:
		case SID_NAME_ALIAS:
		case SID_NAME_WKN_GRP:
			if (ids[i]->xid.type == ID_TYPE_GID) {
				ids[i]->status = ID_MAPPED;
			}
			break;

		default:
			ids[i]->status = ID_UNKNOWN;
			break;
		}
	}


	talloc_free(ctx);
	return NT_STATUS_OK;
}

/**********************************
 lookup a set of sids. 
**********************************/

static NTSTATUS idmap_nss_sids_to_unixids(struct idmap_domain *dom, struct id_map **ids)
{
	TALLOC_CTX *ctx;
	int i;

	if (! dom->initialized) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	ctx = talloc_new(dom);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; ids[i]; i++) {
		struct passwd *pw;
		struct group *gr;
		enum lsa_SidType type;
		const char *dom_name = NULL;
		const char *name = NULL;
		BOOL ret;

		/* by default calls to winbindd are disabled
		   the following call will not recurse so this is safe */
		winbind_on();
		ret = winbind_lookup_sid(ctx, ids[i]->sid, &dom_name, &name, &type);
		winbind_off();

		if (!ret) {
			/* TODO: how do we know if the name is really not mapped,
			 * or something just failed ? */
			ids[i]->status = ID_UNMAPPED;
			continue;
		}

		switch (type) {
		case SID_NAME_USER:

			/* this will find also all lower case name and use username level */
			
			pw = Get_Pwnam(name);
			if (pw) {
				ids[i]->xid.id = pw->pw_uid;
				ids[i]->xid.type = ID_TYPE_UID;
				ids[i]->status = ID_MAPPED;
			}
			break;

		case SID_NAME_DOM_GRP:
		case SID_NAME_ALIAS:
		case SID_NAME_WKN_GRP:

			gr = getgrnam(name);
			if (gr) {
				ids[i]->xid.id = gr->gr_gid;
				ids[i]->xid.type = ID_TYPE_GID;
				ids[i]->status = ID_MAPPED;
			}
			break;

		default:
			ids[i]->status = ID_UNKNOWN;
			break;
		}
	}

	talloc_free(ctx);
	return NT_STATUS_OK;
}

/**********************************
 Close the idmap tdb instance
**********************************/

static NTSTATUS idmap_nss_close(struct idmap_domain *dom)
{
	return NT_STATUS_OK;
}

static struct idmap_methods nss_methods = {

	.init = idmap_nss_int_init,
	.unixids_to_sids = idmap_nss_unixids_to_sids,
	.sids_to_unixids = idmap_nss_sids_to_unixids,
	.close_fn = idmap_nss_close
};

NTSTATUS idmap_nss_init(void)
{
	return smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION, "nss", &nss_methods);
}

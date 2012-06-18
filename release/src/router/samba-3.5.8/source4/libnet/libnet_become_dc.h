/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006

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
*/

#ifndef _LIBNET_BECOME_DC_H
#define _LIBNET_BECOME_DC_H

#include "librpc/gen_ndr/drsuapi.h"

struct libnet_BecomeDC_Domain {
	/* input */
	const char *dns_name;
	const char *netbios_name;
	const struct dom_sid *sid;

	/* constructed */
	struct GUID guid;
	const char *dn_str;
	uint32_t behavior_version;
	uint32_t w2k3_update_revision;
};

struct libnet_BecomeDC_Forest {
	/* constructed */
	const char *dns_name;
	const char *root_dn_str;
	const char *config_dn_str;
	uint32_t crossref_behavior_version;
	const char *schema_dn_str;
	uint32_t schema_object_version;
};

struct libnet_BecomeDC_SourceDSA {
	/* input */
	const char *address;

	/* constructed */
	const char *dns_name;
	const char *netbios_name;
	const char *site_name;
	const char *server_dn_str;
	const char *ntds_dn_str;
};

struct libnet_BecomeDC_CheckOptions {
	const struct libnet_BecomeDC_Domain *domain;
	const struct libnet_BecomeDC_Forest *forest;
	const struct libnet_BecomeDC_SourceDSA *source_dsa;
};

struct libnet_BecomeDC_DestDSA {
	/* input */
	const char *netbios_name;

	/* constructed */
	const char *dns_name;
	const char *site_name;
	struct GUID site_guid;
	const char *computer_dn_str;
	const char *server_dn_str;
	const char *ntds_dn_str;
	struct GUID ntds_guid;
	struct GUID invocation_id;
	uint32_t user_account_control;
};

struct libnet_BecomeDC_PrepareDB {
	const struct libnet_BecomeDC_Domain *domain;
	const struct libnet_BecomeDC_Forest *forest;
	const struct libnet_BecomeDC_SourceDSA *source_dsa;
	const struct libnet_BecomeDC_DestDSA *dest_dsa;
};

struct libnet_BecomeDC_StoreChunk;

struct libnet_BecomeDC_Partition {
	struct drsuapi_DsReplicaObjectIdentifier nc;
	struct GUID destination_dsa_guid;
	struct GUID source_dsa_guid;
	struct GUID source_dsa_invocation_id;
	struct drsuapi_DsReplicaHighWaterMark highwatermark;
	bool more_data;
	uint32_t replica_flags;

	NTSTATUS (*store_chunk)(void *private_data,
				const struct libnet_BecomeDC_StoreChunk *info);
};

struct libnet_BecomeDC_StoreChunk {
	const struct libnet_BecomeDC_Domain *domain;
	const struct libnet_BecomeDC_Forest *forest;
	const struct libnet_BecomeDC_SourceDSA *source_dsa;
	const struct libnet_BecomeDC_DestDSA *dest_dsa;
	const struct libnet_BecomeDC_Partition *partition;
	uint32_t ctr_level;
	const struct drsuapi_DsGetNCChangesCtr1 *ctr1;
	const struct drsuapi_DsGetNCChangesCtr6 *ctr6;
	const DATA_BLOB *gensec_skey;
};

struct libnet_BecomeDC_Callbacks {
	void *private_data;
	NTSTATUS (*check_options)(void *private_data,
				  const struct libnet_BecomeDC_CheckOptions *info);
	NTSTATUS (*prepare_db)(void *private_data,
			       const struct libnet_BecomeDC_PrepareDB *info);
	NTSTATUS (*schema_chunk)(void *private_data,
				 const struct libnet_BecomeDC_StoreChunk *info);
	NTSTATUS (*config_chunk)(void *private_data,
				 const struct libnet_BecomeDC_StoreChunk *info);
	NTSTATUS (*domain_chunk)(void *private_data,
				 const struct libnet_BecomeDC_StoreChunk *info);
};

struct libnet_BecomeDC {
	struct {
		const char *domain_dns_name;
		const char *domain_netbios_name;
		const struct dom_sid *domain_sid;
		const char *source_dsa_address;
		const char *dest_dsa_netbios_name;

		struct libnet_BecomeDC_Callbacks callbacks;
	} in;

	struct {
		const char *error_string;
	} out;
};

#endif /* _LIBNET_BECOME_DC_H */

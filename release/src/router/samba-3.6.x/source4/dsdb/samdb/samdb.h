/* 
   Unix SMB/CIFS implementation.

   interface functions for the sam database

   Copyright (C) Andrew Tridgell 2004
   
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

#ifndef __SAMDB_H__
#define __SAMDB_H__

struct auth_session_info;
struct dsdb_control_current_partition;
struct dsdb_extended_replicated_object;
struct dsdb_extended_replicated_objects;
struct loadparm_context;
struct tevent_context;

#include "librpc/gen_ndr/security.h"
#include <ldb.h>
#include "lib/ldb-samba/ldif_handlers.h"
#include "librpc/gen_ndr/samr.h"
#include "librpc/gen_ndr/drsuapi.h"
#include "librpc/gen_ndr/drsblobs.h"
#include "dsdb/schema/schema.h"
#include "dsdb/samdb/samdb_proto.h"
#include "dsdb/common/dsdb_dn.h"
#include "dsdb/common/proto.h"
#include "../libds/common/flags.h"

#define DSDB_CONTROL_CURRENT_PARTITION_OID "1.3.6.1.4.1.7165.4.3.2"
struct dsdb_control_current_partition {
	/* 
	 * this is the version of the dsdb_control_current_partition
	 * version 0: initial implementation
	 * version 1: got rid of backend and module fields
	 */
#define DSDB_CONTROL_CURRENT_PARTITION_VERSION 1
	uint32_t version;
	struct ldb_dn *dn;
};

#define DSDB_CONTROL_REPLICATED_UPDATE_OID "1.3.6.1.4.1.7165.4.3.3"
/* DSDB_CONTROL_REPLICATED_UPDATE_OID has NULL data */

#define DSDB_CONTROL_DN_STORAGE_FORMAT_OID "1.3.6.1.4.1.7165.4.3.4"
/* DSDB_CONTROL_DN_STORAGE_FORMAT_OID has NULL data and behaves very
 * much like LDB_CONTROL_EXTENDED_DN_OID when the DB stores an
 * extended DN, and otherwise returns normal DNs */

#define DSDB_CONTROL_PASSWORD_CHANGE_STATUS_OID "1.3.6.1.4.1.7165.4.3.8"

struct dsdb_control_password_change_status {
	struct {
		uint32_t pwdProperties;
		uint32_t pwdHistoryLength;
		int64_t maxPwdAge;
		int64_t minPwdAge;
		uint32_t minPwdLength;
		bool store_cleartext;
		const char *netbios_domain;
		const char *dns_domain;
		const char *realm;
	} domain_data;
	enum samPwdChangeReason reject_reason;
};

#define DSDB_CONTROL_PASSWORD_HASH_VALUES_OID "1.3.6.1.4.1.7165.4.3.9"

#define DSDB_CONTROL_PASSWORD_CHANGE_OID "1.3.6.1.4.1.7165.4.3.10"

struct dsdb_control_password_change {
	const struct samr_Password *old_nt_pwd_hash;
	const struct samr_Password *old_lm_pwd_hash;
};

/**
   DSDB_CONTROL_APPLY_LINKS is internal to Samba4 - a token passed between repl_meta_data and linked_attributes modules
*/
#define DSDB_CONTROL_APPLY_LINKS "1.3.6.1.4.1.7165.4.3.11"

/*
 * this should only be used for importing users from Samba3
 */
#define DSDB_CONTROL_BYPASS_PASSWORD_HASH_OID "1.3.6.1.4.1.7165.4.3.12"

/**
  OID used to allow the replacement of replPropertyMetaData.
  It is used when the current replmetadata needs to be edited.
*/
#define DSDB_CONTROL_CHANGEREPLMETADATA_OID "1.3.6.1.4.1.7165.4.3.14"

#define DSDB_EXTENDED_REPLICATED_OBJECTS_OID "1.3.6.1.4.1.7165.4.4.1"
struct dsdb_extended_replicated_object {
	struct ldb_message *msg;
	struct ldb_val guid_value;
	const char *when_changed;
	struct replPropertyMetaDataBlob *meta_data;
};

struct dsdb_extended_replicated_objects {
	/* 
	 * this is the version of the dsdb_extended_replicated_objects
	 * version 0: initial implementation
	 */
#define DSDB_EXTENDED_REPLICATED_OBJECTS_VERSION 1
	uint32_t version;

	struct ldb_dn *partition_dn;

	const struct repsFromTo1 *source_dsa;
	const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector;

	uint32_t num_objects;
	struct dsdb_extended_replicated_object *objects;

	uint32_t linked_attributes_count;
	const struct drsuapi_DsReplicaLinkedAttribute *linked_attributes;
};

struct dsdb_naming_fsmo {
	bool we_are_master;
	struct ldb_dn *master_dn;
};

struct dsdb_pdc_fsmo {
	bool we_are_master;
	struct ldb_dn *master_dn;
};

#define DSDB_EXTENDED_CREATE_PARTITION_OID "1.3.6.1.4.1.7165.4.4.4"
struct dsdb_create_partition_exop {
	struct ldb_dn *new_dn;
};

/*
 * the schema_dn is passed as struct ldb_dn in
 * req->op.extended.data
 */
#define DSDB_EXTENDED_SCHEMA_UPDATE_NOW_OID "1.3.6.1.4.1.7165.4.4.2"

#define DSDB_OPENLDAP_DEREFERENCE_CONTROL "1.3.6.1.4.1.4203.666.5.16"

struct dsdb_openldap_dereference {
	const char *source_attribute;
	const char **dereference_attribute;
};

struct dsdb_openldap_dereference_control {
	struct dsdb_openldap_dereference **dereference;
};

struct dsdb_openldap_dereference_result {
	const char *source_attribute;
	const char *dereferenced_dn;
	int num_attributes;
	struct ldb_message_element *attributes;
};

struct dsdb_openldap_dereference_result_control {
	struct dsdb_openldap_dereference_result **attributes;
};

#define DSDB_PARTITION_DN "@PARTITION"
#define DSDB_PARTITION_ATTR "partition"

#define DSDB_EXTENDED_DN_STORE_FORMAT_OPAQUE_NAME "dsdb_extended_dn_store_format"
struct dsdb_extended_dn_store_format {
	bool store_extended_dn_in_ldb;
};

#define DSDB_OPAQUE_PARTITION_MODULE_MSG_OPAQUE_NAME "DSDB_OPAQUE_PARTITION_MODULE_MSG"

/* this takes a struct dsdb_fsmo_extended_op */
#define DSDB_EXTENDED_ALLOCATE_RID_POOL "1.3.6.1.4.1.7165.4.4.5"

struct dsdb_fsmo_extended_op {
	uint64_t fsmo_info;
	struct GUID destination_dsa_guid;
};

#endif /* __SAMDB_H__ */

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

#include "includes.h"
#include "libnet/libnet.h"
#include "libcli/composite/composite.h"
#include "libcli/cldap/cldap.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "ldb_wrap.h"
#include "dsdb/samdb/samdb.h"
#include "../libds/common/flags.h"
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"
#include "lib/tsocket/tsocket.h"

/*****************************************************************************
 * Windows 2003 (w2k3) does the following steps when changing the server role
 * from domain member to domain controller
 *
 * We mostly do the same.
 *****************************************************************************/

/*
 * lookup DC:
 * - using nbt name<1C> request and a samlogon mailslot request
 * or
 * - using a DNS SRV _ldap._tcp.dc._msdcs. request and a CLDAP netlogon request
 *
 * see: becomeDC_recv_cldap() and becomeDC_send_cldap()
 */

/*
 * Open 1st LDAP connection to the DC using admin credentials
 *
 * see: becomeDC_connect_ldap1() and becomeDC_ldap_connect()
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_rootdse()
 *
 * Request:
 *	basedn:	""
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	*
 * Result:
 *      ""
 *		currentTime:		20061202155100.0Z
 *		subschemaSubentry:	CN=Aggregate,CN=Schema,CN=Configuration,<domain_partition>
 *		dsServiceName:		CN=<netbios_name>,CN=Servers,CN=<site_name>,CN=Sites,CN=Configuration,<domain_partition>
 *		namingContexts:		<domain_partition>
 *					CN=Configuration,<domain_partition>
 *					CN=Schema,CN=Configuration,<domain_partition>
 *		defaultNamingContext:	<domain_partition>
 *		schemaNamingContext:	CN=Schema,CN=Configuration,<domain_partition>
 *		configurationNamingContext:CN=Configuration,<domain_partition>
 *		rootDomainNamingContext:<domain_partition>
 *		supportedControl:	...
 *		supportedLDAPVersion:	3
 *					2
 *		supportedLDAPPolicies:	...
 *		highestCommitedUSN:	...
 *		supportedSASLMechanisms:GSSAPI
 *					GSS-SPNEGO
 *					EXTERNAL
 *					DIGEST-MD5
 *		dnsHostName:		<dns_host_name>
 *		ldapServiceName:	<domain_dns_name>:<netbios_name>$@<REALM>
 *		serverName:		CN=Servers,CN=<site_name>,CN=Sites,CN=Configuration,<domain_partition>
 *		supportedCapabilities:	...
 *		isSyncronized:		TRUE
 *		isGlobalCatalogReady:	TRUE
 *		domainFunctionality:	0
 *		forestFunctionality:	0
 *		domainControllerFunctionality: 2
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_crossref_behavior_version()
 *
 * Request:
 *	basedn:	CN=Configuration,<domain_partition>
 *	scope:	one
 *	filter:	(cn=Partitions)
 *	attrs:	msDS-Behavior-Version
 * Result:
 *      CN=Partitions,CN=Configuration,<domain_partition>
 *		msDS-Behavior-Version:	0
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * NOTE: this seems to be a bug! as the messageID of the LDAP message is corrupted!
 *
 * not implemented here
 * 
 * Request:
 *	basedn:	CN=Schema,CN=Configuration,<domain_partition>
 *	scope:	one
 *	filter:	(cn=Partitions)
 *	attrs:	msDS-Behavior-Version
 * Result:
 *	<none>
 *
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_domain_behavior_version()
 * 
 * Request:
 *	basedn:	<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	msDS-Behavior-Version
 * Result:
 *	<domain_partition>
 *		msDS-Behavior-Version:	0
 */

/*
 * LDAP search 1st LDAP connection:
 * 
 * see: becomeDC_ldap1_schema_object_version()
 *
 * Request:
 *	basedn:	CN=Schema,CN=Configuration,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	objectVersion
 * Result:
 *	CN=Schema,CN=Configuration,<domain_partition>
 *		objectVersion:	30
 */

/*
 * LDAP search 1st LDAP connection:
 * 
 * not implemented, because the information is already there
 *
 * Request:
 *	basedn:	""
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	defaultNamingContext
 *		dnsHostName
 * Result:
 *	""
 *		defaultNamingContext:	<domain_partition>
 *		dnsHostName:		<dns_host_name>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_infrastructure_fsmo()
 * 
 * Request:
 *	basedn:	<WKGUID=2fbac1870ade11d297c400c04fd8d5cd,domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	1.1
 * Result:
 *	CN=Infrastructure,<domain_partition>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_w2k3_update_revision()
 *
 * Request:
 *	basedn:	CN=Windows2003Update,CN=DomainUpdates,CN=System,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	revision
 * Result:
 *      CN=Windows2003Update,CN=DomainUpdates,CN=System,<domain_partition>
 *		revision:	8
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_infrastructure_fsmo()
 *
 * Request:
 *	basedn:	CN=Infrastructure,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	fSMORoleOwner
 * Result:
 *      CN=Infrastructure,<domain_partition>
 *		fSMORoleOwner:	CN=NTDS Settings,<infrastructure_fsmo_server_object>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_infrastructure_fsmo()
 *
 * Request:
 *	basedn:	<infrastructure_fsmo_server_object>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	dnsHostName
 * Result:
 *      <infrastructure_fsmo_server_object>
 *		dnsHostName:	<dns_host_name>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_infrastructure_fsmo()
 *
 * Request:
 *	basedn:	CN=NTDS Settings,<infrastructure_fsmo_server_object>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	objectGUID
 * Result:
 *      CN=NTDS Settings,<infrastructure_fsmo_server_object>
 *		objectGUID:	<object_guid>
 */

/*
 * LDAP search 1st LDAP connection:
 * 
 * see: becomeDC_ldap1_rid_manager_fsmo()
 *
 * Request:
 *	basedn:	<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	rIDManagerReference
 * Result:
 *	<domain_partition>
 *		rIDManagerReference:	CN=RID Manager$,CN=System,<domain_partition>
 */

/*
 * LDAP search 1st LDAP connection:
 * 
 * see: becomeDC_ldap1_rid_manager_fsmo()
 *
 * Request:
 *	basedn:	CN=RID Manager$,CN=System,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	fSMORoleOwner
 * Result:
 *      CN=Infrastructure,<domain_partition>
 *		fSMORoleOwner:	CN=NTDS Settings,<rid_manager_fsmo_server_object>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_rid_manager_fsmo()
 *
 * Request:
 *	basedn:	<rid_manager_fsmo_server_object>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	dnsHostName
 * Result:
 *      <rid_manager_fsmo_server_object>
 *		dnsHostName:	<dns_host_name>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_rid_manager_fsmo()
 *
 * Request:
 *	basedn:	CN=NTDS Settings,<rid_manager_fsmo_server_object>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	msDs-ReplicationEpoch
 * Result:
 *      CN=NTDS Settings,<rid_manager_fsmo_server_object>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_site_object()
 *
 * Request:
 *	basedn:	CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:
 * Result:
 *      CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>
 *		objectClass:	top
 *				site
 *		cn:		<new_dc_site_name>
 *		distinguishedName:CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>
 *		instanceType:	4
 *		whenCreated:	...
 *		whenChanged:	...
 *		uSNCreated:	...
 *		uSNChanged:	...
 *		showInAdvancedViewOnly:	TRUE
 *		name:		<new_dc_site_name>
 *		objectGUID:	<object_guid>
 *		systemFlags:	1107296256 <0x42000000>
 *		objectCategory:	CN=Site,CN=Schema,CN=Configuration,<domain_partition>
 */

/***************************************************************
 * Add this stage we call the check_options() callback function
 * of the caller, to see if he wants us to continue
 *
 * see: becomeDC_check_options()
 ***************************************************************/

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_computer_object()
 *
 * Request:
 *	basedn:	<domain_partition>
 *	scope:	sub
 *	filter:	(&(|(objectClass=user)(objectClass=computer))(sAMAccountName=<new_dc_account_name>))
 *	attrs:	distinguishedName
 *		userAccountControl
 * Result:
 *      CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *		distinguishedName:	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *		userAccoountControl:	4096 <0x1000>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_server_object_1()
 *
 * Request:
 *	basedn:	CN=<new_dc_netbios_name>,CN=Servers,CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:
 * Result:
 *      <noSuchObject>
 *	<matchedDN:CN=Servers,CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: becomeDC_ldap1_server_object_2()
 * 
 * Request:
 *	basedn:	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	serverReferenceBL
 *	typesOnly: TRUE!!!
 * Result:
 *      CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 */

/*
 * LDAP add 1st LDAP connection:
 * 
 * see: becomeDC_ldap1_server_object_add()
 *
 * Request:
 *	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *	objectClass:	server
 *	systemFlags:	50000000 <0x2FAF080>
 *	serverReference:CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 * Result:
 *      <success>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * not implemented, maybe we can add that later
 *
 * Request:
 *	basedn:	CN=NTDS Settings,CN=<new_dc_netbios_name>,CN=Servers,CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:
 * Result:
 *      <noSuchObject>
 *	<matchedDN:CN=<new_dc_netbios_name>,CN=Servers,CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * not implemented because it gives no new information
 * 
 * Request:
 *	basedn:	CN=Partitions,CN=Configuration,<domain_partition>
 *	scope:	sub
 *	filter:	(nCName=<domain_partition>)
 *	attrs:	nCName
 *		dnsRoot
 *	controls: LDAP_SERVER_EXTENDED_DN_OID:critical=false
 * Result:
 *      <GUID=<hex_guid>>;CN=<domain_netbios_name>,CN=Partitions,<domain_partition>>
 *		nCName:		<GUID=<hex_guid>>;<SID=<hex_sid>>;<domain_partition>>
 *		dnsRoot:	<domain_dns_name>
 */

/*
 * LDAP modify 1st LDAP connection:
 *
 * see: becomeDC_ldap1_server_object_modify()
 * 
 * Request (add):
 *	CN=<new_dc_netbios_name>,CN=Servers,CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>>
 *	serverReference:CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 * Result:
 *	<attributeOrValueExist>
 */

/*
 * LDAP modify 1st LDAP connection:
 *
 * see: becomeDC_ldap1_server_object_modify()
 *
 * Request (replace):
 *	CN=<new_dc_netbios_name>,CN=Servers,CN=<new_dc_site_name>,CN=Sites,CN=Configuration,<domain_partition>>
 *	serverReference:CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 * Result:
 *	<success>
 */

/*
 * Open 1st DRSUAPI connection to the DC using admin credentials
 * DsBind with DRSUAPI_DS_BIND_GUID_W2K3 ("6afab99c-6e26-464a-975f-f58f105218bc")
 * (w2k3 does 2 DsBind() calls here..., where is first is unused and contains garbage at the end)
 *
 * see: becomeDC_drsuapi_connect_send(), becomeDC_drsuapi1_connect_recv(),
 *      becomeDC_drsuapi_bind_send(), becomeDC_drsuapi_bind_recv() and becomeDC_drsuapi1_bind_recv()
 */

/*
 * DsAddEntry to create the CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name, ...
 * on the 1st DRSUAPI connection
 *
 * see: becomeDC_drsuapi1_add_entry_send() and becomeDC_drsuapi1_add_entry_recv()
 */

/***************************************************************
 * Add this stage we call the prepare_db() callback function
 * of the caller, to see if he wants us to continue
 *
 * see: becomeDC_prepare_db()
 ***************************************************************/

/*
 * Open 2nd and 3rd DRSUAPI connection to the DC using admin credentials
 * - a DsBind with DRSUAPI_DS_BIND_GUID_W2K3 ("6afab99c-6e26-464a-975f-f58f105218bc")
 *   on the 2nd connection
 *
 * see: becomeDC_drsuapi_connect_send(), becomeDC_drsuapi2_connect_recv(),
 *      becomeDC_drsuapi_bind_send(), becomeDC_drsuapi_bind_recv(), becomeDC_drsuapi2_bind_recv()
 *	and becomeDC_drsuapi3_connect_recv()
 */

/*
 * replicate CN=Schema,CN=Configuration,...
 * on the 3rd DRSUAPI connection and the bind_handle from the 2nd connection
 *
 * see: becomeDC_drsuapi_pull_partition_send(), becomeDC_drsuapi_pull_partition_recv(),
 *	becomeDC_drsuapi3_pull_schema_send() and becomeDC_drsuapi3_pull_schema_recv()
 *
 ***************************************************************
 * Add this stage we call the schema_chunk() callback function
 * for each replication message
 ***************************************************************/

/*
 * replicate CN=Configuration,...
 * on the 3rd DRSUAPI connection and the bind_handle from the 2nd connection
 *
 * see: becomeDC_drsuapi_pull_partition_send(), becomeDC_drsuapi_pull_partition_recv(),
 *	becomeDC_drsuapi3_pull_config_send() and becomeDC_drsuapi3_pull_config_recv()
 *
 ***************************************************************
 * Add this stage we call the config_chunk() callback function
 * for each replication message
 ***************************************************************/

/*
 * LDAP unbind on the 1st LDAP connection
 *
 * not implemented, because it's not needed...
 */

/*
 * Open 2nd LDAP connection to the DC using admin credentials
 *
 * see: becomeDC_connect_ldap2() and becomeDC_ldap_connect()
 */

/*
 * LDAP search 2nd LDAP connection:
 * 
 * not implemented because it gives no new information
 * same as becomeDC_ldap1_computer_object()
 *
 * Request:
 *	basedn:	<domain_partition>
 *	scope:	sub
 *	filter:	(&(|(objectClass=user)(objectClass=computer))(sAMAccountName=<new_dc_account_name>))
 *	attrs:	distinguishedName
 *		userAccountControl
 * Result:
 *      CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *		distinguishedName:	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *		userAccoountControl:	4096 <0x00001000>
 */

/*
 * LDAP search 2nd LDAP connection:
 * 
 * not implemented because it gives no new information
 * same as becomeDC_ldap1_computer_object()
 *
 * Request:
 *	basedn:	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	userAccountControl
 * Result:
 *      CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *		userAccoountControl:	4096 <0x00001000>
 */

/*
 * LDAP modify 2nd LDAP connection:
 *
 * see: becomeDC_ldap2_modify_computer()
 *
 * Request (replace):
 *	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *	userAccoountControl:	532480 <0x82000>
 * Result:
 *	<success>
 */

/*
 * LDAP search 2nd LDAP connection:
 *
 * see: becomeDC_ldap2_move_computer()
 * 
 * Request:
 *	basedn:	<WKGUID=2fbac1870ade11d297c400c04fd8d5cd,<domain_partition>>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	1.1
 * Result:
 *	CN=Domain Controllers,<domain_partition>
 */

/*
 * LDAP search 2nd LDAP connection:
 *
 * not implemented because it gives no new information
 * 
 * Request:
 *	basedn:	CN=Domain Controllers,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	distinguishedName
 * Result:
 *	CN=Domain Controller,<domain_partition>
 *		distinguishedName:	CN=Domain Controllers,<domain_partition>
 */

/*
 * LDAP modifyRDN 2nd LDAP connection:
 *
 * see: becomeDC_ldap2_move_computer()
 * 
 * Request:
 *      entry:		CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *	newrdn:		CN=<new_dc_netbios_name>
 *	deleteoldrdn:	TRUE
 *	newparent:	CN=Domain Controllers,<domain_partition>
 * Result:
 *	<success>
 */

/*
 * LDAP unbind on the 2nd LDAP connection
 *
 * not implemented, because it's not needed...
 */

/*
 * replicate Domain Partition
 * on the 3rd DRSUAPI connection and the bind_handle from the 2nd connection
 *
 * see: becomeDC_drsuapi_pull_partition_send(), becomeDC_drsuapi_pull_partition_recv(),
 *	becomeDC_drsuapi3_pull_domain_send() and becomeDC_drsuapi3_pull_domain_recv()
 *
 ***************************************************************
 * Add this stage we call the domain_chunk() callback function
 * for each replication message
 ***************************************************************/

/* call DsReplicaUpdateRefs() for all partitions like this:
 *     req1: struct drsuapi_DsReplicaUpdateRefsRequest1
 *
 *                 naming_context: struct drsuapi_DsReplicaObjectIdentifier
 *                     __ndr_size               : 0x000000ae (174)
 *                     __ndr_size_sid           : 0x00000000 (0)
 *                     guid                     : 00000000-0000-0000-0000-000000000000
 *                     sid                      : S-0-0
 *                     dn                       : 'CN=Schema,CN=Configuration,DC=w2k3,DC=vmnet1,DC=vm,DC=base'
 *
 *                 dest_dsa_dns_name        : '4a0df188-a0b8-47ea-bbe5-e614723f16dd._msdcs.w2k3.vmnet1.vm.base'
 *           dest_dsa_guid            : 4a0df188-a0b8-47ea-bbe5-e614723f16dd
 *           options                  : 0x0000001c (28)
 *                 0: DRSUAPI_DS_REPLICA_UPDATE_ASYNCHRONOUS_OPERATION
 *                 0: DRSUAPI_DS_REPLICA_UPDATE_WRITEABLE
 *                 1: DRSUAPI_DS_REPLICA_UPDATE_ADD_REFERENCE
 *                 1: DRSUAPI_DS_REPLICA_UPDATE_DELETE_REFERENCE
 *                 1: DRSUAPI_DS_REPLICA_UPDATE_0x00000010
 *
 * 4a0df188-a0b8-47ea-bbe5-e614723f16dd is the objectGUID the DsAddEntry() returned for the
 * CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name, ...
 * on the 2nd!!! DRSUAPI connection
 *
 * see:	becomeDC_drsuapi_update_refs_send(), becomeDC_drsuapi2_update_refs_schema_recv(),
 *	becomeDC_drsuapi2_update_refs_config_recv() and becomeDC_drsuapi2_update_refs_domain_recv()
 */

/*
 * Windows does opens the 4th and 5th DRSUAPI connection...
 * and does a DsBind() with the objectGUID from DsAddEntry() as bind_guid
 * on the 4th connection
 *
 * and then 2 full replications of the domain partition on the 5th connection
 * with the bind_handle from the 4th connection
 *
 * not implemented because it gives no new information
 */

struct libnet_BecomeDC_state {
	struct composite_context *creq;

	struct libnet_context *libnet;

	struct dom_sid zero_sid;

	struct {
		struct cldap_socket *sock;
		struct cldap_netlogon io;
		struct NETLOGON_SAM_LOGON_RESPONSE_EX netlogon;
	} cldap;

	struct becomeDC_ldap {
		struct ldb_context *ldb;
		const struct ldb_message *rootdse;
	} ldap1, ldap2;

	struct becomeDC_drsuapi {
		struct libnet_BecomeDC_state *s;
		struct dcerpc_binding *binding;
		struct dcerpc_pipe *pipe;
		struct dcerpc_binding_handle *drsuapi_handle;
		DATA_BLOB gensec_skey;
		struct drsuapi_DsBind bind_r;
		struct GUID bind_guid;
		struct drsuapi_DsBindInfoCtr bind_info_ctr;
		struct drsuapi_DsBindInfo28 local_info28;
		struct drsuapi_DsBindInfo28 remote_info28;
		struct policy_handle bind_handle;
	} drsuapi1, drsuapi2, drsuapi3;

	void *ndr_struct_ptr;

	struct libnet_BecomeDC_Domain domain;
	struct libnet_BecomeDC_Forest forest;
	struct libnet_BecomeDC_SourceDSA source_dsa;
	struct libnet_BecomeDC_DestDSA dest_dsa;

	struct libnet_BecomeDC_Partition schema_part, config_part, domain_part;

	struct becomeDC_fsmo {
		const char *dns_name;
		const char *server_dn_str;
		const char *ntds_dn_str;
		struct GUID ntds_guid;
	} infrastructure_fsmo;

	struct becomeDC_fsmo rid_manager_fsmo;

	struct libnet_BecomeDC_CheckOptions _co;
	struct libnet_BecomeDC_PrepareDB _pp;
	struct libnet_BecomeDC_StoreChunk _sc;
	struct libnet_BecomeDC_Callbacks callbacks;

	bool rodc_join;
};

static int32_t get_dc_function_level(struct loadparm_context *lp_ctx)
{
	/* per default we are (Windows) 2008 R2 compatible */
	return lpcfg_parm_int(lp_ctx, NULL, "ads", "dc function level",
			   DS_DOMAIN_FUNCTION_2008_R2);
}

static void becomeDC_recv_cldap(struct tevent_req *req);

static void becomeDC_send_cldap(struct libnet_BecomeDC_state *s)
{
	struct composite_context *c = s->creq;
	struct tevent_req *req;
	struct tsocket_address *dest_address;
	int ret;

	s->cldap.io.in.dest_address	= NULL;
	s->cldap.io.in.dest_port	= 0;
	s->cldap.io.in.realm		= s->domain.dns_name;
	s->cldap.io.in.host		= s->dest_dsa.netbios_name;
	s->cldap.io.in.user		= NULL;
	s->cldap.io.in.domain_guid	= NULL;
	s->cldap.io.in.domain_sid	= NULL;
	s->cldap.io.in.acct_control	= -1;
	s->cldap.io.in.version		= NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX;
	s->cldap.io.in.map_response	= true;

	ret = tsocket_address_inet_from_strings(s, "ip",
						s->source_dsa.address,
						lpcfg_cldap_port(s->libnet->lp_ctx),
						&dest_address);
	if (ret != 0) {
		c->status = map_nt_error_from_unix(errno);
		if (!composite_is_ok(c)) return;
	}

	c->status = cldap_socket_init(s, s->libnet->event_ctx,
				      NULL, dest_address, &s->cldap.sock);
	if (!composite_is_ok(c)) return;

	req = cldap_netlogon_send(s, s->cldap.sock, &s->cldap.io);
	if (composite_nomem(req, c)) return;
	tevent_req_set_callback(req, becomeDC_recv_cldap, s);
}

static void becomeDC_connect_ldap1(struct libnet_BecomeDC_state *s);

static void becomeDC_recv_cldap(struct tevent_req *req)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(req,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;

	c->status = cldap_netlogon_recv(req, s, &s->cldap.io);
	talloc_free(req);
	if (!composite_is_ok(c)) {
		DEBUG(0,("Failed to send, receive or parse CLDAP reply from server %s for our host %s: %s\n", 
			 s->cldap.io.in.dest_address, 
			 s->cldap.io.in.host, 
			 nt_errstr(c->status)));
		return;
	}
	s->cldap.netlogon = s->cldap.io.out.netlogon.data.nt5_ex;

	s->domain.dns_name		= s->cldap.netlogon.dns_domain;
	s->domain.netbios_name		= s->cldap.netlogon.domain_name;
	s->domain.guid			= s->cldap.netlogon.domain_uuid;

	s->forest.dns_name		= s->cldap.netlogon.forest;

	s->source_dsa.dns_name		= s->cldap.netlogon.pdc_dns_name;
	s->source_dsa.netbios_name	= s->cldap.netlogon.pdc_name;
	s->source_dsa.site_name		= s->cldap.netlogon.server_site;

	s->dest_dsa.site_name		= s->cldap.netlogon.client_site;

	DEBUG(0,("CLDAP response: forest=%s dns=%s netbios=%s server_site=%s  client_site=%s\n",
		 s->forest.dns_name, s->domain.dns_name, s->domain.netbios_name,
		 s->source_dsa.site_name, s->dest_dsa.site_name));
	if (!s->dest_dsa.site_name || strcmp(s->dest_dsa.site_name, "") == 0) {
		DEBUG(0,("Got empty client site - using server site name %s\n",
			 s->source_dsa.site_name));
		s->dest_dsa.site_name = s->source_dsa.site_name;
	}

	becomeDC_connect_ldap1(s);
}

static NTSTATUS becomeDC_ldap_connect(struct libnet_BecomeDC_state *s, 
				      struct becomeDC_ldap *ldap)
{
	char *url;

	url = talloc_asprintf(s, "ldap://%s/", s->source_dsa.dns_name);
	NT_STATUS_HAVE_NO_MEMORY(url);

	ldap->ldb = ldb_wrap_connect(s, s->libnet->event_ctx, s->libnet->lp_ctx, url,
				     NULL,
				     s->libnet->cred,
				     0);
	talloc_free(url);
	if (ldap->ldb == NULL) {
		return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_rootdse(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"*",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap1.ldb, NULL);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE, attrs,
			 "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->ldap1.rootdse = r->msgs[0];

	s->domain.dn_str	= ldb_msg_find_attr_as_string(s->ldap1.rootdse, "defaultNamingContext", NULL);
	if (!s->domain.dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;

	s->forest.root_dn_str	= ldb_msg_find_attr_as_string(s->ldap1.rootdse, "rootDomainNamingContext", NULL);
	if (!s->forest.root_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	s->forest.config_dn_str	= ldb_msg_find_attr_as_string(s->ldap1.rootdse, "configurationNamingContext", NULL);
	if (!s->forest.config_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	s->forest.schema_dn_str	= ldb_msg_find_attr_as_string(s->ldap1.rootdse, "schemaNamingContext", NULL);
	if (!s->forest.schema_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;

	s->source_dsa.server_dn_str	= ldb_msg_find_attr_as_string(s->ldap1.rootdse, "serverName", NULL);
	if (!s->source_dsa.server_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	s->source_dsa.ntds_dn_str	= ldb_msg_find_attr_as_string(s->ldap1.rootdse, "dsServiceName", NULL);
	if (!s->source_dsa.ntds_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;

	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_crossref_behavior_version(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"msDs-Behavior-Version",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap1.ldb, s->forest.config_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_ONELEVEL, attrs,
			 "(cn=Partitions)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->forest.crossref_behavior_version = ldb_msg_find_attr_as_uint(r->msgs[0], "msDs-Behavior-Version", 0);
	if (s->forest.crossref_behavior_version >
			get_dc_function_level(s->libnet->lp_ctx)) {
		talloc_free(r);
		DEBUG(0,("The servers function level %u is above 'ads:dc function level' of %u\n", 
			 s->forest.crossref_behavior_version, 
			 get_dc_function_level(s->libnet->lp_ctx)));
		return NT_STATUS_NOT_SUPPORTED;
	}

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_domain_behavior_version(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"msDs-Behavior-Version",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap1.ldb, s->domain.dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE, attrs,
			 "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->domain.behavior_version = ldb_msg_find_attr_as_uint(r->msgs[0], "msDs-Behavior-Version", 0);
	if (s->domain.behavior_version >
			get_dc_function_level(s->libnet->lp_ctx)) {
		talloc_free(r);
		DEBUG(0,("The servers function level %u is above 'ads:dc function level' of %u\n", 
			 s->forest.crossref_behavior_version, 
			 get_dc_function_level(s->libnet->lp_ctx)));
		return NT_STATUS_NOT_SUPPORTED;
	}

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_schema_object_version(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"objectVersion",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap1.ldb, s->forest.schema_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE, attrs,
			 "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->forest.schema_object_version = ldb_msg_find_attr_as_uint(r->msgs[0], "objectVersion", 0);

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_w2k3_update_revision(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"revision",
		NULL
	};

	basedn = ldb_dn_new_fmt(s, s->ldap1.ldb, "CN=Windows2003Update,CN=DomainUpdates,CN=System,%s",
				s->domain.dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE, attrs,
			 "(objectClass=*)");
	talloc_free(basedn);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		/* w2k doesn't have this object */
		s->domain.w2k3_update_revision = 0;
		return NT_STATUS_OK;
	} else if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->domain.w2k3_update_revision = ldb_msg_find_attr_as_uint(r->msgs[0], "revision", 0);

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_infrastructure_fsmo(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	struct ldb_dn *ntds_dn;
	struct ldb_dn *server_dn;
	static const char *dns_attrs[] = {
		"dnsHostName",
		NULL
	};
	static const char *guid_attrs[] = {
		"objectGUID",
		NULL
	};

	ret = dsdb_wellknown_dn(s->ldap1.ldb, s,
				ldb_get_default_basedn(s->ldap1.ldb),
				DS_GUID_INFRASTRUCTURE_CONTAINER,
				&basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	}

	ret = samdb_reference_dn(s->ldap1.ldb, s, basedn, "fSMORoleOwner", &ntds_dn);
	if (ret != LDB_SUCCESS) {
		talloc_free(basedn);
		return NT_STATUS_LDAP(ret);
	}

	s->infrastructure_fsmo.ntds_dn_str = ldb_dn_get_linearized(ntds_dn);
	NT_STATUS_HAVE_NO_MEMORY(s->infrastructure_fsmo.ntds_dn_str);

	server_dn = ldb_dn_get_parent(s, ntds_dn);
	NT_STATUS_HAVE_NO_MEMORY(server_dn);

	s->infrastructure_fsmo.server_dn_str = ldb_dn_alloc_linearized(s, server_dn);
	NT_STATUS_HAVE_NO_MEMORY(s->infrastructure_fsmo.server_dn_str);

	ret = ldb_search(s->ldap1.ldb, s, &r, server_dn, LDB_SCOPE_BASE,
			 dns_attrs, "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->infrastructure_fsmo.dns_name	= ldb_msg_find_attr_as_string(r->msgs[0], "dnsHostName", NULL);
	if (!s->infrastructure_fsmo.dns_name) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->infrastructure_fsmo.dns_name);

	talloc_free(r);

	ret = ldb_search(s->ldap1.ldb, s, &r, ntds_dn, LDB_SCOPE_BASE,
			 guid_attrs, "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->infrastructure_fsmo.ntds_guid = samdb_result_guid(r->msgs[0], "objectGUID");

	talloc_free(r);

	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_rid_manager_fsmo(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	const char *reference_dn_str;
	struct ldb_dn *ntds_dn;
	struct ldb_dn *server_dn;
	static const char *rid_attrs[] = {
		"rIDManagerReference",
		NULL
	};
	static const char *fsmo_attrs[] = {
		"fSMORoleOwner",
		NULL
	};
	static const char *dns_attrs[] = {
		"dnsHostName",
		NULL
	};
	static const char *guid_attrs[] = {
		"objectGUID",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap1.ldb, s->domain.dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE,
			 rid_attrs, "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	reference_dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "rIDManagerReference", NULL);
	if (!reference_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;

	basedn = ldb_dn_new(s, s->ldap1.ldb, reference_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	talloc_free(r);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE,
			 fsmo_attrs, "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->rid_manager_fsmo.ntds_dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "fSMORoleOwner", NULL);
	if (!s->rid_manager_fsmo.ntds_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->rid_manager_fsmo.ntds_dn_str);

	talloc_free(r);

	ntds_dn = ldb_dn_new(s, s->ldap1.ldb, s->rid_manager_fsmo.ntds_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(ntds_dn);

	server_dn = ldb_dn_get_parent(s, ntds_dn);
	NT_STATUS_HAVE_NO_MEMORY(server_dn);

	s->rid_manager_fsmo.server_dn_str = ldb_dn_alloc_linearized(s, server_dn);
	NT_STATUS_HAVE_NO_MEMORY(s->rid_manager_fsmo.server_dn_str);

	ret = ldb_search(s->ldap1.ldb, s, &r, server_dn, LDB_SCOPE_BASE,
			 dns_attrs, "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->rid_manager_fsmo.dns_name	= ldb_msg_find_attr_as_string(r->msgs[0], "dnsHostName", NULL);
	if (!s->rid_manager_fsmo.dns_name) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->rid_manager_fsmo.dns_name);

	talloc_free(r);

	ret = ldb_search(s->ldap1.ldb, s, &r, ntds_dn, LDB_SCOPE_BASE,
			 guid_attrs, "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->rid_manager_fsmo.ntds_guid = samdb_result_guid(r->msgs[0], "objectGUID");

	talloc_free(r);

	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_site_object(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;

	basedn = ldb_dn_new_fmt(s, s->ldap1.ldb, "CN=%s,CN=Sites,%s",
				s->dest_dsa.site_name,
				s->forest.config_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE,
			 NULL, "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->dest_dsa.site_guid = samdb_result_guid(r->msgs[0], "objectGUID");

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_check_options(struct libnet_BecomeDC_state *s)
{
	if (!s->callbacks.check_options) return NT_STATUS_OK;

	s->_co.domain		= &s->domain;
	s->_co.forest		= &s->forest;
	s->_co.source_dsa	= &s->source_dsa;

	return s->callbacks.check_options(s->callbacks.private_data, &s->_co);
}

static NTSTATUS becomeDC_ldap1_computer_object(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"distinguishedName",
		"userAccountControl",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap1.ldb, s->domain.dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_SUBTREE, attrs,
			 "(&(|(objectClass=user)(objectClass=computer))(sAMAccountName=%s$))",
			 s->dest_dsa.netbios_name);
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->dest_dsa.computer_dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "distinguishedName", NULL);
	if (!s->dest_dsa.computer_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->dest_dsa.computer_dn_str);

	s->dest_dsa.user_account_control = ldb_msg_find_attr_as_uint(r->msgs[0], "userAccountControl", 0);

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_server_object_1(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	const char *server_reference_dn_str;
	struct ldb_dn *server_reference_dn;
	struct ldb_dn *computer_dn;

	basedn = ldb_dn_new_fmt(s, s->ldap1.ldb, "CN=%s,CN=Servers,CN=%s,CN=Sites,%s",
				s->dest_dsa.netbios_name,
				s->dest_dsa.site_name,
				s->forest.config_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE,
			 NULL, "(objectClass=*)");
	talloc_free(basedn);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		/* if the object doesn't exist, we'll create it later */
		return NT_STATUS_OK;
	} else if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	server_reference_dn_str = ldb_msg_find_attr_as_string(r->msgs[0], "serverReference", NULL);
	if (server_reference_dn_str) {
		server_reference_dn	= ldb_dn_new(r, s->ldap1.ldb, server_reference_dn_str);
		NT_STATUS_HAVE_NO_MEMORY(server_reference_dn);

		computer_dn		= ldb_dn_new(r, s->ldap1.ldb, s->dest_dsa.computer_dn_str);
		NT_STATUS_HAVE_NO_MEMORY(computer_dn);

		/*
		 * if the server object belongs to another DC in another domain
		 * in the forest, we should not touch this object!
		 */
		if (ldb_dn_compare(computer_dn, server_reference_dn) != 0) {
			talloc_free(r);
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	}

	/* if the server object is already for the dest_dsa, then we don't need to create it */
	s->dest_dsa.server_dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "distinguishedName", NULL);
	if (!s->dest_dsa.server_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->dest_dsa.server_dn_str);

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_server_object_2(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	const char *server_reference_bl_dn_str;
	static const char *attrs[] = {
		"serverReferenceBL",
		NULL
	};

	/* if the server_dn_str has a valid value, we skip this lookup */
	if (s->dest_dsa.server_dn_str) return NT_STATUS_OK;

	basedn = ldb_dn_new(s, s->ldap1.ldb, s->dest_dsa.computer_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap1.ldb, s, &r, basedn, LDB_SCOPE_BASE,
			 attrs, "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	server_reference_bl_dn_str = ldb_msg_find_attr_as_string(r->msgs[0], "serverReferenceBL", NULL);
	if (!server_reference_bl_dn_str) {
		/* if no back link is present, we're done for this function */
		talloc_free(r);
		return NT_STATUS_OK;
	}

	/* if the server object is already for the dest_dsa, then we don't need to create it */
	s->dest_dsa.server_dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "serverReferenceBL", NULL);
	if (s->dest_dsa.server_dn_str) {
		/* if a back link is present, we know that the server object is present */
		talloc_steal(s, s->dest_dsa.server_dn_str);
	}

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_server_object_add(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_message *msg;
	char *server_dn_str;

	/* if the server_dn_str has a valid value, we skip this lookup */
	if (s->dest_dsa.server_dn_str) return NT_STATUS_OK;

	msg = ldb_msg_new(s);
	NT_STATUS_HAVE_NO_MEMORY(msg);

	msg->dn = ldb_dn_new_fmt(msg, s->ldap1.ldb, "CN=%s,CN=Servers,CN=%s,CN=Sites,%s",
				 s->dest_dsa.netbios_name,
				 s->dest_dsa.site_name,
				 s->forest.config_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(msg->dn);

	ret = ldb_msg_add_string(msg, "objectClass", "server");
	if (ret != 0) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	ret = ldb_msg_add_string(msg, "systemFlags", "50000000");
	if (ret != 0) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	ret = ldb_msg_add_string(msg, "serverReference", s->dest_dsa.computer_dn_str);
	if (ret != 0) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	server_dn_str = ldb_dn_alloc_linearized(s, msg->dn);
	NT_STATUS_HAVE_NO_MEMORY(server_dn_str);

	ret = ldb_add(s->ldap1.ldb, msg);
	talloc_free(msg);
	if (ret != LDB_SUCCESS) {
		talloc_free(server_dn_str);
		return NT_STATUS_LDAP(ret);
	}

	s->dest_dsa.server_dn_str = server_dn_str;

	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap1_server_object_modify(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_message *msg;
	unsigned int i;

	/* make a 'modify' msg, and only for serverReference */
	msg = ldb_msg_new(s);
	NT_STATUS_HAVE_NO_MEMORY(msg);
	msg->dn = ldb_dn_new(msg, s->ldap1.ldb, s->dest_dsa.server_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(msg->dn);

	ret = ldb_msg_add_string(msg, "serverReference", s->dest_dsa.computer_dn_str);
	if (ret != 0) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	/* mark all the message elements (should be just one)
	   as LDB_FLAG_MOD_ADD */
	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_ADD;
	}

	ret = ldb_modify(s->ldap1.ldb, msg);
	if (ret == LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_OK;
	} else if (ret == LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS) {
		/* retry with LDB_FLAG_MOD_REPLACE */
	} else {
		talloc_free(msg);
		return NT_STATUS_LDAP(ret);
	}

	/* mark all the message elements (should be just one)
	   as LDB_FLAG_MOD_REPLACE */
	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	ret = ldb_modify(s->ldap1.ldb, msg);
	talloc_free(msg);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	}

	return NT_STATUS_OK;
}

static void becomeDC_drsuapi_connect_send(struct libnet_BecomeDC_state *s,
					  struct becomeDC_drsuapi *drsuapi,
					  void (*recv_fn)(struct composite_context *req));
static void becomeDC_drsuapi1_connect_recv(struct composite_context *req);
static void becomeDC_connect_ldap2(struct libnet_BecomeDC_state *s);

static void becomeDC_connect_ldap1(struct libnet_BecomeDC_state *s)
{
	struct composite_context *c = s->creq;

	c->status = becomeDC_ldap_connect(s, &s->ldap1);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_rootdse(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_crossref_behavior_version(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_domain_behavior_version(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_schema_object_version(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_w2k3_update_revision(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_infrastructure_fsmo(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_rid_manager_fsmo(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_site_object(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_check_options(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_computer_object(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_server_object_1(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_server_object_2(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_server_object_add(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap1_server_object_modify(s);
	if (!composite_is_ok(c)) return;

	becomeDC_drsuapi_connect_send(s, &s->drsuapi1, becomeDC_drsuapi1_connect_recv);
}

static void becomeDC_drsuapi_connect_send(struct libnet_BecomeDC_state *s,
					  struct becomeDC_drsuapi *drsuapi,
					  void (*recv_fn)(struct composite_context *req))
{
	struct composite_context *c = s->creq;
	struct composite_context *creq;
	char *binding_str;

	drsuapi->s = s;

	if (!drsuapi->binding) {
		const char *krb5_str = "";
		const char *print_str = "";
		/*
		 * Note: Replication only works with Windows 2000 when 'krb5' is
		 *       passed as auth_type here. If NTLMSSP is used, Windows
		 *       2000 returns garbage in the DsGetNCChanges() response
		 *       if encrypted password attributes would be in the
		 *       response. That means the replication of the schema and
		 *       configuration partition works fine, but it fails for
		 *       the domain partition.
		 */
		if (lpcfg_parm_bool(s->libnet->lp_ctx, NULL, "become_dc",
				 "force krb5", true))
		{
			krb5_str = "krb5,";
		}
		if (lpcfg_parm_bool(s->libnet->lp_ctx, NULL, "become_dc",
				 "print", false))
		{
			print_str = "print,";
		}
		binding_str = talloc_asprintf(s, "ncacn_ip_tcp:%s[%s%sseal]",
					      s->source_dsa.dns_name,
					      krb5_str, print_str);
		if (composite_nomem(binding_str, c)) return;
		c->status = dcerpc_parse_binding(s, binding_str, &drsuapi->binding);
		talloc_free(binding_str);
		if (!composite_is_ok(c)) return;
	}

	creq = dcerpc_pipe_connect_b_send(s, drsuapi->binding, &ndr_table_drsuapi,
					  s->libnet->cred, s->libnet->event_ctx,
					  s->libnet->lp_ctx);
	composite_continue(c, creq, recv_fn, s);
}

static void becomeDC_drsuapi_bind_send(struct libnet_BecomeDC_state *s,
				       struct becomeDC_drsuapi *drsuapi,
				       void (*recv_fn)(struct tevent_req *subreq));
static void becomeDC_drsuapi1_bind_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi1_connect_recv(struct composite_context *req)
{
	struct libnet_BecomeDC_state *s = talloc_get_type(req->async.private_data,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;

	c->status = dcerpc_pipe_connect_b_recv(req, s, &s->drsuapi1.pipe);
	if (!composite_is_ok(c)) return;

	s->drsuapi1.drsuapi_handle = s->drsuapi1.pipe->binding_handle;

	c->status = gensec_session_key(s->drsuapi1.pipe->conn->security_state.generic_state,
				       &s->drsuapi1.gensec_skey);
	if (!composite_is_ok(c)) return;

	becomeDC_drsuapi_bind_send(s, &s->drsuapi1, becomeDC_drsuapi1_bind_recv);
}

static void becomeDC_drsuapi_bind_send(struct libnet_BecomeDC_state *s,
				       struct becomeDC_drsuapi *drsuapi,
				       void (*recv_fn)(struct tevent_req *subreq))
{
	struct composite_context *c = s->creq;
	struct drsuapi_DsBindInfo28 *bind_info28;
	struct tevent_req *subreq;

	GUID_from_string(DRSUAPI_DS_BIND_GUID_W2K3, &drsuapi->bind_guid);

	bind_info28				= &drsuapi->local_info28;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_BASE;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ASYNC_REPLICATION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_REMOVEAPI;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_MOVEREQ_V2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHG_COMPRESS;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V1;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_RESTORE_USN_OPTIMIZATION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_KCC_EXECUTE;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRY_V2;
	if (s->domain.behavior_version >= DS_DOMAIN_FUNCTION_2003) {
		/* TODO: find out how this is really triggered! */
		bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_LINKED_VALUE_REPLICATION;
	}
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_INSTANCE_TYPE_NOT_REQ_ON_MOD;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_CRYPTO_BIND;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_REPL_INFO;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V01;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_TRANSITIVE_MEMBERSHIP;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADD_SID_HISTORY;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V5;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_MEMBERSHIPS2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V6;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_NONDOMAIN_NCS;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V5;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V6;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRYREPLY_V3;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V7;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_VERIFY_OBJECT;
#if 0 /* we don't support XPRESS compression yet */
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_XPRESS_COMPRESS;
#endif
	bind_info28->site_guid			= s->dest_dsa.site_guid;
	bind_info28->pid			= 0;
	bind_info28->repl_epoch			= 0;

	drsuapi->bind_info_ctr.length		= 28;
	drsuapi->bind_info_ctr.info.info28	= *bind_info28;

	drsuapi->bind_r.in.bind_guid = &drsuapi->bind_guid;
	drsuapi->bind_r.in.bind_info = &drsuapi->bind_info_ctr;
	drsuapi->bind_r.out.bind_handle = &drsuapi->bind_handle;

	subreq = dcerpc_drsuapi_DsBind_r_send(s, c->event_ctx,
					      drsuapi->drsuapi_handle,
					      &drsuapi->bind_r);
	if (composite_nomem(subreq, c)) return;
	tevent_req_set_callback(subreq, recv_fn, s);
}

static WERROR becomeDC_drsuapi_bind_recv(struct libnet_BecomeDC_state *s,
					 struct becomeDC_drsuapi *drsuapi)
{
	if (!W_ERROR_IS_OK(drsuapi->bind_r.out.result)) {
		return drsuapi->bind_r.out.result;
	}

	ZERO_STRUCT(drsuapi->remote_info28);
	if (drsuapi->bind_r.out.bind_info) {
		switch (drsuapi->bind_r.out.bind_info->length) {
		case 24: {
			struct drsuapi_DsBindInfo24 *info24;
			info24 = &drsuapi->bind_r.out.bind_info->info.info24;
			drsuapi->remote_info28.supported_extensions	= info24->supported_extensions;
			drsuapi->remote_info28.site_guid		= info24->site_guid;
			drsuapi->remote_info28.pid			= info24->pid;
			drsuapi->remote_info28.repl_epoch		= 0;
			break;
		}
		case 48: {
			struct drsuapi_DsBindInfo48 *info48;
			info48 = &drsuapi->bind_r.out.bind_info->info.info48;
			drsuapi->remote_info28.supported_extensions	= info48->supported_extensions;
			drsuapi->remote_info28.site_guid		= info48->site_guid;
			drsuapi->remote_info28.pid			= info48->pid;
			drsuapi->remote_info28.repl_epoch		= info48->repl_epoch;
			break;
		}
		case 28:
			drsuapi->remote_info28 = drsuapi->bind_r.out.bind_info->info.info28;
			break;
		}
	}

	return WERR_OK;
}

static void becomeDC_drsuapi1_add_entry_send(struct libnet_BecomeDC_state *s);

static void becomeDC_drsuapi1_bind_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	WERROR status;

	c->status = dcerpc_drsuapi_DsBind_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	status = becomeDC_drsuapi_bind_recv(s, &s->drsuapi1);
	if (!W_ERROR_IS_OK(status)) {
		composite_error(c, werror_to_ntstatus(status));
		return;
	}

	becomeDC_drsuapi1_add_entry_send(s);
}

static void becomeDC_drsuapi1_add_entry_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi1_add_entry_send(struct libnet_BecomeDC_state *s)
{
	struct composite_context *c = s->creq;
	struct drsuapi_DsAddEntry *r;
	struct drsuapi_DsReplicaObjectIdentifier *identifier;
	uint32_t num_attrs, i = 0;
	struct drsuapi_DsReplicaAttribute *attrs;
	enum ndr_err_code ndr_err;
	bool w2k3;
	struct tevent_req *subreq;

	/* choose a random invocationId */
	s->dest_dsa.invocation_id = GUID_random();

	/*
	 * if the schema version indicates w2k3, then also send some w2k3
	 * specific attributes.
	 */
	if (s->forest.schema_object_version >= 30) {
		w2k3 = true;
	} else {
		w2k3 = false;
	}

	r = talloc_zero(s, struct drsuapi_DsAddEntry);
	if (composite_nomem(r, c)) return;

	/* setup identifier */
	identifier		= talloc(r, struct drsuapi_DsReplicaObjectIdentifier);
	if (composite_nomem(identifier, c)) return;
	identifier->guid	= GUID_zero();
	identifier->sid		= s->zero_sid;
	identifier->dn		= talloc_asprintf(identifier, "CN=NTDS Settings,%s",
						  s->dest_dsa.server_dn_str);
	if (composite_nomem(identifier->dn, c)) return;

	/* allocate attribute array */
	num_attrs	= 12;
	attrs		= talloc_array(r, struct drsuapi_DsReplicaAttribute, num_attrs);
	if (composite_nomem(attrs, c)) return;

	/* ntSecurityDescriptor */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		struct security_descriptor *v;
		struct dom_sid *domain_admins_sid;
		const char *domain_admins_sid_str;

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		domain_admins_sid = dom_sid_add_rid(vs, s->domain.sid, DOMAIN_RID_ADMINS);
		if (composite_nomem(domain_admins_sid, c)) return;

		domain_admins_sid_str = dom_sid_string(domain_admins_sid, domain_admins_sid);
		if (composite_nomem(domain_admins_sid_str, c)) return;

		v = security_descriptor_dacl_create(vd,
					       0,
					       /* owner: domain admins */
					       domain_admins_sid_str,
					       /* owner group: domain admins */
					       domain_admins_sid_str,
					       /* authenticated users */
					       SID_NT_AUTHENTICATED_USERS,
					       SEC_ACE_TYPE_ACCESS_ALLOWED,
					       SEC_STD_READ_CONTROL |
					       SEC_ADS_LIST |
					       SEC_ADS_READ_PROP |
					       SEC_ADS_LIST_OBJECT,
					       0,
					       /* domain admins */
					       domain_admins_sid_str,
					       SEC_ACE_TYPE_ACCESS_ALLOWED,
					       SEC_STD_REQUIRED |
					       SEC_ADS_CREATE_CHILD |
					       SEC_ADS_LIST |
					       SEC_ADS_SELF_WRITE |
					       SEC_ADS_READ_PROP |
					       SEC_ADS_WRITE_PROP |
					       SEC_ADS_DELETE_TREE |
					       SEC_ADS_LIST_OBJECT |
					       SEC_ADS_CONTROL_ACCESS,
					       0,
					       /* system */
					       SID_NT_SYSTEM,
					       SEC_ACE_TYPE_ACCESS_ALLOWED,
					       SEC_STD_REQUIRED |
					       SEC_ADS_CREATE_CHILD |
					       SEC_ADS_DELETE_CHILD |
					       SEC_ADS_LIST |
					       SEC_ADS_SELF_WRITE |
					       SEC_ADS_READ_PROP |
					       SEC_ADS_WRITE_PROP |
					       SEC_ADS_DELETE_TREE |
					       SEC_ADS_LIST_OBJECT |
					       SEC_ADS_CONTROL_ACCESS,
					       0,
					       /* end */
					       NULL);
		if (composite_nomem(v, c)) return;

		ndr_err = ndr_push_struct_blob(&vd[0], vd, v,
	    	(ndr_push_flags_fn_t)ndr_push_security_descriptor);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_ntSecurityDescriptor;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* objectClass: nTDSDSA */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		vd[0] = data_blob_talloc(vd, NULL, 4);
		if (composite_nomem(vd[0].data, c)) return;

		/* value for nTDSDSA */
		SIVAL(vd[0].data, 0, 0x0017002F);

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_objectClass;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* objectCategory: CN=NTDS-DSA,CN=Schema,... or CN=NTDS-DSA-RO,CN=Schema,... */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		struct drsuapi_DsReplicaObjectIdentifier3 v[1];

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		v[0].guid		= GUID_zero();
		v[0].sid		= s->zero_sid;

		if (s->rodc_join) {
		    v[0].dn		= talloc_asprintf(vd, "CN=NTDS-DSA-RO,%s",
							  s->forest.schema_dn_str);
		} else {
		    v[0].dn		= talloc_asprintf(vd, "CN=NTDS-DSA,%s",
							  s->forest.schema_dn_str);
		}
		if (composite_nomem(v[0].dn, c)) return;

		ndr_err = ndr_push_struct_blob(&vd[0], vd, &v[0], 
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_objectCategory;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* invocationId: random guid */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		const struct GUID *v;

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		v = &s->dest_dsa.invocation_id;

		c->status = GUID_to_ndr_blob(v, vd, &vd[0]);
		if (!composite_is_ok(c)) return;

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_invocationId;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* hasMasterNCs: ... */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		struct drsuapi_DsReplicaObjectIdentifier3 v[3];

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 3);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 3);
		if (composite_nomem(vd, c)) return;

		v[0].guid		= GUID_zero();
		v[0].sid		= s->zero_sid;
		v[0].dn			= s->forest.config_dn_str;

		v[1].guid		= GUID_zero();
		v[1].sid		= s->zero_sid;
		v[1].dn			= s->domain.dn_str;

		v[2].guid		= GUID_zero();
		v[2].sid		= s->zero_sid;
		v[2].dn			= s->forest.schema_dn_str;

		ndr_err = ndr_push_struct_blob(&vd[0], vd, &v[0],
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		ndr_err = ndr_push_struct_blob(&vd[1], vd, &v[1],
	    	(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		ndr_err = ndr_push_struct_blob(&vd[2], vd, &v[2],
		   (ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		vs[0].blob		= &vd[0];
		vs[1].blob		= &vd[1];
		vs[2].blob		= &vd[2];

		attrs[i].attid			= DRSUAPI_ATTID_hasMasterNCs;
		attrs[i].value_ctr.num_values	= 3;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* msDS-hasMasterNCs: ... */
	if (w2k3) {
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		struct drsuapi_DsReplicaObjectIdentifier3 v[3];

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 3);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 3);
		if (composite_nomem(vd, c)) return;

		v[0].guid		= GUID_zero();
		v[0].sid		= s->zero_sid;
		v[0].dn			= s->forest.config_dn_str;

		v[1].guid		= GUID_zero();
		v[1].sid		= s->zero_sid;
		v[1].dn			= s->domain.dn_str;

		v[2].guid		= GUID_zero();
		v[2].sid		= s->zero_sid;
		v[2].dn			= s->forest.schema_dn_str;

		ndr_err = ndr_push_struct_blob(&vd[0], vd, &v[0],
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		ndr_err = ndr_push_struct_blob(&vd[1], vd, &v[1],
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		ndr_err = ndr_push_struct_blob(&vd[2], vd, &v[2],
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		vs[0].blob		= &vd[0];
		vs[1].blob		= &vd[1];
		vs[2].blob		= &vd[2];

		attrs[i].attid			= DRSUAPI_ATTID_msDS_hasMasterNCs;
		attrs[i].value_ctr.num_values	= 3;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* dMDLocation: CN=Schema,... */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		struct drsuapi_DsReplicaObjectIdentifier3 v[1];

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		v[0].guid		= GUID_zero();
		v[0].sid		= s->zero_sid;
		v[0].dn			= s->forest.schema_dn_str;

		ndr_err = ndr_push_struct_blob(&vd[0], vd, &v[0],
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_dMDLocation;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* msDS-HasDomainNCs: <domain_partition> */
	if (w2k3) {
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		struct drsuapi_DsReplicaObjectIdentifier3 v[1];

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		v[0].guid		= GUID_zero();
		v[0].sid		= s->zero_sid;
		v[0].dn			= s->domain.dn_str;

		ndr_err = ndr_push_struct_blob(&vd[0], vd, &v[0],
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_msDS_HasDomainNCs;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* msDS-Behavior-Version */
	if (w2k3) {
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		vd[0] = data_blob_talloc(vd, NULL, 4);
		if (composite_nomem(vd[0].data, c)) return;

		SIVAL(vd[0].data, 0, get_dc_function_level(s->libnet->lp_ctx));

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_msDS_Behavior_Version;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* systemFlags */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		vd[0] = data_blob_talloc(vd, NULL, 4);
		if (composite_nomem(vd[0].data, c)) return;

		if (s->rodc_join) {
		    SIVAL(vd[0].data, 0, SYSTEM_FLAG_CONFIG_ALLOW_RENAME);
		} else {
		    SIVAL(vd[0].data, 0, SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE);
		}

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_systemFlags;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* serverReference: ... */
	{
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;
		struct drsuapi_DsReplicaObjectIdentifier3 v[1];

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		v[0].guid		= GUID_zero();
		v[0].sid		= s->zero_sid;
		v[0].dn			= s->dest_dsa.computer_dn_str;

		ndr_err = ndr_push_struct_blob(&vd[0], vd, &v[0],
			(ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			c->status = ndr_map_error2ntstatus(ndr_err);
			if (!composite_is_ok(c)) return;
		}

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_serverReference;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* options:... */
	if (s->rodc_join) {
		struct drsuapi_DsAttributeValue *vs;
		DATA_BLOB *vd;

		vs = talloc_array(attrs, struct drsuapi_DsAttributeValue, 1);
		if (composite_nomem(vs, c)) return;

		vd = talloc_array(vs, DATA_BLOB, 1);
		if (composite_nomem(vd, c)) return;

		vd[0] = data_blob_talloc(vd, NULL, 4);
		if (composite_nomem(vd[0].data, c)) return;

		SIVAL(vd[0].data, 0, DS_NTDSDSA_OPT_DISABLE_OUTBOUND_REPL);

		vs[0].blob		= &vd[0];

		attrs[i].attid			= DRSUAPI_ATTID_options;
		attrs[i].value_ctr.num_values	= 1;
		attrs[i].value_ctr.values	= vs;

		i++;
	}

	/* truncate the attribute list to the attribute count we have filled in */
	num_attrs = i;

	/* setup request structure */
	r->in.bind_handle						= &s->drsuapi1.bind_handle;
	r->in.level							= 2;
	r->in.req							= talloc(s, union drsuapi_DsAddEntryRequest);
	r->in.req->req2.first_object.next_object			= NULL;
	r->in.req->req2.first_object.object.identifier			= identifier;
	r->in.req->req2.first_object.object.flags			= 0x00000000;
	r->in.req->req2.first_object.object.attribute_ctr.num_attributes= num_attrs;
	r->in.req->req2.first_object.object.attribute_ctr.attributes	= attrs;

	r->out.level_out	= talloc(s, uint32_t);
	r->out.ctr		= talloc(s, union drsuapi_DsAddEntryCtr);

	s->ndr_struct_ptr = r;
	subreq = dcerpc_drsuapi_DsAddEntry_r_send(s, c->event_ctx,
						  s->drsuapi1.drsuapi_handle, r);
	if (composite_nomem(subreq, c)) return;
	tevent_req_set_callback(subreq, becomeDC_drsuapi1_add_entry_recv, s);
}

static void becomeDC_drsuapi2_connect_recv(struct composite_context *req);
static NTSTATUS becomeDC_prepare_db(struct libnet_BecomeDC_state *s);

static void becomeDC_drsuapi1_add_entry_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsAddEntry *r = talloc_get_type_abort(s->ndr_struct_ptr,
				       struct drsuapi_DsAddEntry);
	char *binding_str;

	s->ndr_struct_ptr = NULL;

	c->status = dcerpc_drsuapi_DsAddEntry_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(r->out.result)) {
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	if (*r->out.level_out == 3) {
		WERROR status;
		union drsuapi_DsAddEntry_ErrData *err_data = r->out.ctr->ctr3.err_data;

		/* check for errors */
		status = err_data ? err_data->v1.status : WERR_OK;
		if (!W_ERROR_IS_OK(status)) {
			struct drsuapi_DsAddEntryErrorInfo_Attr_V1 *attr_err;
			struct drsuapi_DsAddEntry_AttrErrListItem_V1 *attr_err_li;
			struct drsuapi_DsAddEntryErrorInfo_Name_V1 *name_err;
			struct drsuapi_DsAddEntryErrorInfo_Referr_V1 *ref_err;
			struct drsuapi_DsAddEntry_RefErrListItem_V1 *ref_li;

			if (r->out.ctr->ctr3.err_ver != 1) {
				composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
				return;
			}

			DEBUG(0,("DsAddEntry (R3) of '%s' failed: "
				 "Errors: dir_err = %d, status = %s;\n",
				 r->in.req->req3.first_object.object.identifier->dn,
				 err_data->v1.dir_err,
				 win_errstr(err_data->v1.status)));

			if (!err_data->v1.info) {
				DEBUG(0, ("DsAddEntry (R3): no error info returned!\n"));
				composite_error(c, werror_to_ntstatus(status));
				return;
			}

			/* dump more detailed error */
			switch (err_data->v1.dir_err) {
			case DRSUAPI_DIRERR_ATTRIBUTE:
				/* Dump attribute errors */
				attr_err = &err_data->v1.info->attr_err;
				DEBUGADD(0,(" Attribute Error: object = %s, count = %d;\n",
					    attr_err->id->dn,
					    attr_err->count));
				attr_err_li = &attr_err->first;
				for (; attr_err_li; attr_err_li = attr_err_li->next) {
					struct drsuapi_DsAddEntry_AttrErr_V1 *err = &attr_err_li->err_data;
					DEBUGADD(0,(" Error: err = %s, problem = 0x%08X, attid = 0x%08X;\n",
						    win_errstr(err->extended_err),
						    err->problem,
						    err->attid));
					/* TODO: should we print attribute value here? */
				}
				break;
			case DRSUAPI_DIRERR_NAME:
				/* Dump Name resolution error */
				name_err = &err_data->v1.info->name_err;
				DEBUGADD(0,(" Name Error: err = %s, problem = 0x%08X, id_matched = %s;\n",
					    win_errstr(name_err->extended_err),
					    name_err->problem,
					    name_err->id_matched->dn));
				break;
			case DRSUAPI_DIRERR_REFERRAL:
				/* Dump Referral errors */
				ref_err = &err_data->v1.info->referral_err;
				DEBUGADD(0,(" Referral Error: extended_err = %s\n",
					    win_errstr(ref_err->extended_err)));
				ref_li = &ref_err->refer;
				for (; ref_li; ref_li = ref_li->next) {
					struct drsuapi_DsaAddressListItem_V1 *addr;
					DEBUGADD(0,(" Referral: id_target = %s, ref_type = 0x%04X,",
						    ref_li->id_target->dn,
						    ref_li->ref_type));
					if (ref_li->is_choice_set) {
						DEBUGADD(0,(" choice = 0x%02X, ",
							    ref_li->choice));
					}
					DEBUGADD(0,(" add_list ("));
					for (addr = ref_li->addr_list; addr; addr = addr->next) {
						DEBUGADD(0,("%s", addr->address->string));
						if (addr->next) {
							DEBUGADD(0,(", "));
						}
					}
					DEBUGADD(0,(");\n"));
				}
				break;
			case DRSUAPI_DIRERR_SECURITY:
				/* Dump Security error. */
				DEBUGADD(0,(" Security Error: extended_err = %s, problem = 0x%08X\n",
					    win_errstr(err_data->v1.info->security_err.extended_err),
					    err_data->v1.info->security_err.problem));
				break;
			case DRSUAPI_DIRERR_SERVICE:
				/* Dump Service error. */
				DEBUGADD(0,(" Service Error: extended_err = %s, problem = 0x%08X\n",
					    win_errstr(err_data->v1.info->service_err.extended_err),
					    err_data->v1.info->service_err.problem));
				break;
			case DRSUAPI_DIRERR_UPDATE:
				/* Dump Update error. */
				DEBUGADD(0,(" Update Error: extended_err = %s, problem = 0x%08X\n",
					    win_errstr(err_data->v1.info->update_err.extended_err),
					    err_data->v1.info->update_err.problem));
				break;
			case DRSUAPI_DIRERR_SYSTEM:
				/* System error. */
				DEBUGADD(0,(" System Error: extended_err = %s, problem = 0x%08X\n",
					    win_errstr(err_data->v1.info->system_err.extended_err),
					    err_data->v1.info->system_err.problem));
				break;
			case DRSUAPI_DIRERR_OK: /* mute compiler warnings */
			default:
				DEBUGADD(0,(" Unknown DIRERR error class returned!\n"));
				break;
			}

			composite_error(c, werror_to_ntstatus(status));
			return;
		}

		if (1 != r->out.ctr->ctr3.count) {
			DEBUG(0,("DsAddEntry - Ctr3: something very wrong had happened - "
				 "method succeeded but objects returned are %d (expected 1).\n",
				 r->out.ctr->ctr3.count));
			composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		}

		s->dest_dsa.ntds_guid	= r->out.ctr->ctr3.objects[0].guid;

	} else if (*r->out.level_out == 2) {
		if (DRSUAPI_DIRERR_OK != r->out.ctr->ctr2.dir_err) {
			DEBUG(0,("DsAddEntry failed with: dir_err = %d, extended_err = %s\n",
				 r->out.ctr->ctr2.dir_err,
				 win_errstr(r->out.ctr->ctr2.extended_err)));
			composite_error(c, werror_to_ntstatus(r->out.ctr->ctr2.extended_err));
			return;
		}

		if (1 != r->out.ctr->ctr2.count) {
			DEBUG(0,("DsAddEntry: something very wrong had happened - "
				 "method succeeded but objects returned are %d (expected 1). "
				 "Errors: dir_err = %d, extended_err = %s\n",
				 r->out.ctr->ctr2.count,
				 r->out.ctr->ctr2.dir_err,
				 win_errstr(r->out.ctr->ctr2.extended_err)));
			composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		}

		s->dest_dsa.ntds_guid	= r->out.ctr->ctr2.objects[0].guid;
	} else {
		composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	talloc_free(r);

	s->dest_dsa.ntds_dn_str = talloc_asprintf(s, "CN=NTDS Settings,%s",
						  s->dest_dsa.server_dn_str);
	if (composite_nomem(s->dest_dsa.ntds_dn_str, c)) return;

	c->status = becomeDC_prepare_db(s);
	if (!composite_is_ok(c)) return;

	/* this avoids the epmapper lookup on the 2nd connection */
	binding_str = dcerpc_binding_string(s, s->drsuapi1.binding);
	if (composite_nomem(binding_str, c)) return;

	c->status = dcerpc_parse_binding(s, binding_str, &s->drsuapi2.binding);
	talloc_free(binding_str);
	if (!composite_is_ok(c)) return;

	/* w2k3 uses the same assoc_group_id as on the first connection, so we do */
	s->drsuapi2.binding->assoc_group_id	= s->drsuapi1.pipe->assoc_group_id;

	becomeDC_drsuapi_connect_send(s, &s->drsuapi2, becomeDC_drsuapi2_connect_recv);
}

static NTSTATUS becomeDC_prepare_db(struct libnet_BecomeDC_state *s)
{
	if (!s->callbacks.prepare_db) return NT_STATUS_OK;

	s->_pp.domain		= &s->domain;
	s->_pp.forest		= &s->forest;
	s->_pp.source_dsa	= &s->source_dsa;
	s->_pp.dest_dsa		= &s->dest_dsa;

	return s->callbacks.prepare_db(s->callbacks.private_data, &s->_pp);
}

static void becomeDC_drsuapi2_bind_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi2_connect_recv(struct composite_context *req)
{
	struct libnet_BecomeDC_state *s = talloc_get_type(req->async.private_data,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;

	c->status = dcerpc_pipe_connect_b_recv(req, s, &s->drsuapi2.pipe);
	if (!composite_is_ok(c)) return;

	s->drsuapi2.drsuapi_handle = s->drsuapi2.pipe->binding_handle;

	c->status = gensec_session_key(s->drsuapi2.pipe->conn->security_state.generic_state,
				       &s->drsuapi2.gensec_skey);
	if (!composite_is_ok(c)) return;

	becomeDC_drsuapi_bind_send(s, &s->drsuapi2, becomeDC_drsuapi2_bind_recv);
}

static void becomeDC_drsuapi3_connect_recv(struct composite_context *req);

static void becomeDC_drsuapi2_bind_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	char *binding_str;
	WERROR status;

	c->status = dcerpc_drsuapi_DsBind_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	status = becomeDC_drsuapi_bind_recv(s, &s->drsuapi2);
	if (!W_ERROR_IS_OK(status)) {
		composite_error(c, werror_to_ntstatus(status));
		return;
	}

	/* this avoids the epmapper lookup on the 3rd connection */
	binding_str = dcerpc_binding_string(s, s->drsuapi1.binding);
	if (composite_nomem(binding_str, c)) return;

	c->status = dcerpc_parse_binding(s, binding_str, &s->drsuapi3.binding);
	talloc_free(binding_str);
	if (!composite_is_ok(c)) return;

	/* w2k3 uses the same assoc_group_id as on the first connection, so we do */
	s->drsuapi3.binding->assoc_group_id	= s->drsuapi1.pipe->assoc_group_id;
	/* w2k3 uses the concurrent multiplex feature on the 3rd connection, so we do */
	s->drsuapi3.binding->flags		|= DCERPC_CONCURRENT_MULTIPLEX;

	becomeDC_drsuapi_connect_send(s, &s->drsuapi3, becomeDC_drsuapi3_connect_recv);
}

static void becomeDC_drsuapi3_pull_schema_send(struct libnet_BecomeDC_state *s);

static void becomeDC_drsuapi3_connect_recv(struct composite_context *req)
{
	struct libnet_BecomeDC_state *s = talloc_get_type(req->async.private_data,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;

	c->status = dcerpc_pipe_connect_b_recv(req, s, &s->drsuapi3.pipe);
	if (!composite_is_ok(c)) return;

	s->drsuapi3.drsuapi_handle = s->drsuapi3.pipe->binding_handle;

	c->status = gensec_session_key(s->drsuapi3.pipe->conn->security_state.generic_state,
				       &s->drsuapi3.gensec_skey);
	if (!composite_is_ok(c)) return;

	becomeDC_drsuapi3_pull_schema_send(s);
}

static void becomeDC_drsuapi_pull_partition_send(struct libnet_BecomeDC_state *s,
						 struct becomeDC_drsuapi *drsuapi_h,
						 struct becomeDC_drsuapi *drsuapi_p,
						 struct libnet_BecomeDC_Partition *partition,
						 void (*recv_fn)(struct tevent_req *subreq))
{
	struct composite_context *c = s->creq;
	struct drsuapi_DsGetNCChanges *r;
	struct tevent_req *subreq;

	r = talloc(s, struct drsuapi_DsGetNCChanges);
	if (composite_nomem(r, c)) return;

	r->out.level_out = talloc(r, uint32_t);
	if (composite_nomem(r->out.level_out, c)) return;
	r->in.req = talloc(r, union drsuapi_DsGetNCChangesRequest);
	if (composite_nomem(r->in.req, c)) return;
	r->out.ctr = talloc(r, union drsuapi_DsGetNCChangesCtr);
	if (composite_nomem(r->out.ctr, c)) return;

	r->in.bind_handle	= &drsuapi_h->bind_handle;
	if (drsuapi_h->remote_info28.supported_extensions & DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8) {
		r->in.level				= 8;
		r->in.req->req8.destination_dsa_guid	= partition->destination_dsa_guid;
		r->in.req->req8.source_dsa_invocation_id= partition->source_dsa_invocation_id;
		r->in.req->req8.naming_context		= &partition->nc;
		r->in.req->req8.highwatermark		= partition->highwatermark;
		r->in.req->req8.uptodateness_vector	= NULL;
		r->in.req->req8.replica_flags		= partition->replica_flags;
		r->in.req->req8.max_object_count	= 133;
		r->in.req->req8.max_ndr_size		= 1336811;
		r->in.req->req8.extended_op		= DRSUAPI_EXOP_NONE;
		r->in.req->req8.fsmo_info		= 0;
		r->in.req->req8.partial_attribute_set	= NULL;
		r->in.req->req8.partial_attribute_set_ex= NULL;
		r->in.req->req8.mapping_ctr.num_mappings= 0;
		r->in.req->req8.mapping_ctr.mappings	= NULL;
	} else {
		r->in.level				= 5;
		r->in.req->req5.destination_dsa_guid	= partition->destination_dsa_guid;
		r->in.req->req5.source_dsa_invocation_id= partition->source_dsa_invocation_id;
		r->in.req->req5.naming_context		= &partition->nc;
		r->in.req->req5.highwatermark		= partition->highwatermark;
		r->in.req->req5.uptodateness_vector	= NULL;
		r->in.req->req5.replica_flags		= partition->replica_flags;
		r->in.req->req5.max_object_count	= 133;
		r->in.req->req5.max_ndr_size		= 1336770;
		r->in.req->req5.extended_op		= DRSUAPI_EXOP_NONE;
		r->in.req->req5.fsmo_info		= 0;
	}

	/* 
	 * we should try to use the drsuapi_p->pipe here, as w2k3 does
	 * but it seems that some extra flags in the DCERPC Bind call
	 * are needed for it. Or the same KRB5 TGS is needed on both
	 * connections.
	 */
	s->ndr_struct_ptr = r;
	subreq = dcerpc_drsuapi_DsGetNCChanges_r_send(s, c->event_ctx,
						      drsuapi_p->drsuapi_handle,
						      r);
	if (composite_nomem(subreq, c)) return;
	tevent_req_set_callback(subreq, recv_fn, s);
}

static WERROR becomeDC_drsuapi_pull_partition_recv(struct libnet_BecomeDC_state *s,
						   struct becomeDC_drsuapi *drsuapi_h,
						   struct becomeDC_drsuapi *drsuapi_p,
						   struct libnet_BecomeDC_Partition *partition,
						   struct drsuapi_DsGetNCChanges *r)
{
	uint32_t ctr_level = 0;
	struct drsuapi_DsGetNCChangesCtr1 *ctr1 = NULL;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6 = NULL;
	struct GUID *source_dsa_guid = NULL;
	struct GUID *source_dsa_invocation_id = NULL;
	struct drsuapi_DsReplicaHighWaterMark *new_highwatermark = NULL;
	bool more_data = false;
	NTSTATUS nt_status;

	if (!W_ERROR_IS_OK(r->out.result)) {
		return r->out.result;
	}

	if (*r->out.level_out == 1) {
		ctr_level = 1;
		ctr1 = &r->out.ctr->ctr1;
	} else if (*r->out.level_out == 2 &&
		   r->out.ctr->ctr2.mszip1.ts) {
		ctr_level = 1;
		ctr1 = &r->out.ctr->ctr2.mszip1.ts->ctr1;
	} else if (*r->out.level_out == 6) {
		ctr_level = 6;
		ctr6 = &r->out.ctr->ctr6;
	} else if (*r->out.level_out == 7 &&
		   r->out.ctr->ctr7.level == 6 &&
		   r->out.ctr->ctr7.type == DRSUAPI_COMPRESSION_TYPE_MSZIP &&
		   r->out.ctr->ctr7.ctr.mszip6.ts) {
		ctr_level = 6;
		ctr6 = &r->out.ctr->ctr7.ctr.mszip6.ts->ctr6;
	} else if (*r->out.level_out == 7 &&
		   r->out.ctr->ctr7.level == 6 &&
		   r->out.ctr->ctr7.type == DRSUAPI_COMPRESSION_TYPE_XPRESS &&
		   r->out.ctr->ctr7.ctr.xpress6.ts) {
		ctr_level = 6;
		ctr6 = &r->out.ctr->ctr7.ctr.xpress6.ts->ctr6;
	} else {
		return WERR_BAD_NET_RESP;
	}

	if (!ctr1 && ! ctr6) {
		return WERR_BAD_NET_RESP;
	}

	if (ctr_level == 6) {
		if (!W_ERROR_IS_OK(ctr6->drs_error)) {
			return ctr6->drs_error;
		}
	}

	switch (ctr_level) {
	case 1:
		source_dsa_guid			= &ctr1->source_dsa_guid;
		source_dsa_invocation_id	= &ctr1->source_dsa_invocation_id;
		new_highwatermark		= &ctr1->new_highwatermark;
		more_data			= ctr1->more_data;
		break;
	case 6:
		source_dsa_guid			= &ctr6->source_dsa_guid;
		source_dsa_invocation_id	= &ctr6->source_dsa_invocation_id;
		new_highwatermark		= &ctr6->new_highwatermark;
		more_data			= ctr6->more_data;
		break;
	}

	partition->highwatermark		= *new_highwatermark;
	partition->source_dsa_guid		= *source_dsa_guid;
	partition->source_dsa_invocation_id	= *source_dsa_invocation_id;
	partition->more_data			= more_data;

	if (!partition->store_chunk) return WERR_OK;

	s->_sc.domain		= &s->domain;
	s->_sc.forest		= &s->forest;
	s->_sc.source_dsa	= &s->source_dsa;
	s->_sc.dest_dsa		= &s->dest_dsa;
	s->_sc.partition	= partition;
	s->_sc.ctr_level	= ctr_level;
	s->_sc.ctr1		= ctr1;
	s->_sc.ctr6		= ctr6;
	/* 
	 * we need to use the drsuapi_p->gensec_skey here,
	 * when we use drsuapi_p->pipe in the for this request
	 */
	s->_sc.gensec_skey	= &drsuapi_p->gensec_skey;

	nt_status = partition->store_chunk(s->callbacks.private_data, &s->_sc);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return ntstatus_to_werror(nt_status);
	}

	return WERR_OK;
}

static void becomeDC_drsuapi3_pull_schema_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi3_pull_schema_send(struct libnet_BecomeDC_state *s)
{
	s->schema_part.nc.guid	= GUID_zero();
	s->schema_part.nc.sid	= s->zero_sid;
	s->schema_part.nc.dn	= s->forest.schema_dn_str;

	s->schema_part.destination_dsa_guid	= s->drsuapi2.bind_guid;

	s->schema_part.replica_flags	= DRSUAPI_DRS_WRIT_REP
					| DRSUAPI_DRS_INIT_SYNC
					| DRSUAPI_DRS_PER_SYNC
					| DRSUAPI_DRS_FULL_SYNC_IN_PROGRESS
					| DRSUAPI_DRS_NEVER_SYNCED
					| DRSUAPI_DRS_USE_COMPRESSION;
	if (s->rodc_join) {
	    s->schema_part.replica_flags &= ~DRSUAPI_DRS_WRIT_REP;
	}

	s->schema_part.store_chunk	= s->callbacks.schema_chunk;

	becomeDC_drsuapi_pull_partition_send(s, &s->drsuapi2, &s->drsuapi3, &s->schema_part,
					     becomeDC_drsuapi3_pull_schema_recv);
}

static void becomeDC_drsuapi3_pull_config_send(struct libnet_BecomeDC_state *s);

static void becomeDC_drsuapi3_pull_schema_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsGetNCChanges *r = talloc_get_type_abort(s->ndr_struct_ptr,
					   struct drsuapi_DsGetNCChanges);
	WERROR status;

	s->ndr_struct_ptr = NULL;

	c->status = dcerpc_drsuapi_DsGetNCChanges_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	status = becomeDC_drsuapi_pull_partition_recv(s, &s->drsuapi2, &s->drsuapi3, &s->schema_part, r);
	if (!W_ERROR_IS_OK(status)) {
		composite_error(c, werror_to_ntstatus(status));
		return;
	}

	talloc_free(r);

	if (s->schema_part.more_data) {
		becomeDC_drsuapi_pull_partition_send(s, &s->drsuapi2, &s->drsuapi3, &s->schema_part,
						     becomeDC_drsuapi3_pull_schema_recv);
		return;
	}

	becomeDC_drsuapi3_pull_config_send(s);
}

static void becomeDC_drsuapi3_pull_config_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi3_pull_config_send(struct libnet_BecomeDC_state *s)
{
	s->config_part.nc.guid	= GUID_zero();
	s->config_part.nc.sid	= s->zero_sid;
	s->config_part.nc.dn	= s->forest.config_dn_str;

	s->config_part.destination_dsa_guid	= s->drsuapi2.bind_guid;

	s->config_part.replica_flags	= DRSUAPI_DRS_WRIT_REP
					| DRSUAPI_DRS_INIT_SYNC
					| DRSUAPI_DRS_PER_SYNC
					| DRSUAPI_DRS_FULL_SYNC_IN_PROGRESS
					| DRSUAPI_DRS_NEVER_SYNCED
					| DRSUAPI_DRS_USE_COMPRESSION;
	if (s->rodc_join) {
	    s->schema_part.replica_flags &= ~DRSUAPI_DRS_WRIT_REP;
	}

	s->config_part.store_chunk	= s->callbacks.config_chunk;

	becomeDC_drsuapi_pull_partition_send(s, &s->drsuapi2, &s->drsuapi3, &s->config_part,
					     becomeDC_drsuapi3_pull_config_recv);
}

static void becomeDC_drsuapi3_pull_config_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsGetNCChanges *r = talloc_get_type_abort(s->ndr_struct_ptr,
					   struct drsuapi_DsGetNCChanges);
	WERROR status;

	s->ndr_struct_ptr = NULL;

	c->status = dcerpc_drsuapi_DsGetNCChanges_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	status = becomeDC_drsuapi_pull_partition_recv(s, &s->drsuapi2, &s->drsuapi3, &s->config_part, r);
	if (!W_ERROR_IS_OK(status)) {
		composite_error(c, werror_to_ntstatus(status));
		return;
	}

	talloc_free(r);

	if (s->config_part.more_data) {
		becomeDC_drsuapi_pull_partition_send(s, &s->drsuapi2, &s->drsuapi3, &s->config_part,
						     becomeDC_drsuapi3_pull_config_recv);
		return;
	}

	becomeDC_connect_ldap2(s);
}

static void becomeDC_drsuapi3_pull_domain_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi3_pull_domain_send(struct libnet_BecomeDC_state *s)
{
	s->domain_part.nc.guid	= GUID_zero();
	s->domain_part.nc.sid	= s->zero_sid;
	s->domain_part.nc.dn	= s->domain.dn_str;

	s->domain_part.destination_dsa_guid	= s->drsuapi2.bind_guid;

	s->domain_part.replica_flags	= DRSUAPI_DRS_WRIT_REP
					| DRSUAPI_DRS_INIT_SYNC
					| DRSUAPI_DRS_PER_SYNC
					| DRSUAPI_DRS_FULL_SYNC_IN_PROGRESS
					| DRSUAPI_DRS_NEVER_SYNCED
					| DRSUAPI_DRS_USE_COMPRESSION;
	if (s->rodc_join) {
	    s->schema_part.replica_flags &= ~DRSUAPI_DRS_WRIT_REP;
	}

	s->domain_part.store_chunk	= s->callbacks.domain_chunk;

	becomeDC_drsuapi_pull_partition_send(s, &s->drsuapi2, &s->drsuapi3, &s->domain_part,
					     becomeDC_drsuapi3_pull_domain_recv);
}

static void becomeDC_drsuapi_update_refs_send(struct libnet_BecomeDC_state *s,
					      struct becomeDC_drsuapi *drsuapi,
					      struct libnet_BecomeDC_Partition *partition,
					      void (*recv_fn)(struct tevent_req *subreq));
static void becomeDC_drsuapi2_update_refs_schema_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi3_pull_domain_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsGetNCChanges *r = talloc_get_type_abort(s->ndr_struct_ptr,
					   struct drsuapi_DsGetNCChanges);
	WERROR status;

	s->ndr_struct_ptr = NULL;

	c->status = dcerpc_drsuapi_DsGetNCChanges_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	status = becomeDC_drsuapi_pull_partition_recv(s, &s->drsuapi2, &s->drsuapi3, &s->domain_part, r);
	if (!W_ERROR_IS_OK(status)) {
		composite_error(c, werror_to_ntstatus(status));
		return;
	}

	talloc_free(r);

	if (s->domain_part.more_data) {
		becomeDC_drsuapi_pull_partition_send(s, &s->drsuapi2, &s->drsuapi3, &s->domain_part,
						     becomeDC_drsuapi3_pull_domain_recv);
		return;
	}

	becomeDC_drsuapi_update_refs_send(s, &s->drsuapi2, &s->schema_part,
					  becomeDC_drsuapi2_update_refs_schema_recv);
}

static void becomeDC_drsuapi_update_refs_send(struct libnet_BecomeDC_state *s,
					      struct becomeDC_drsuapi *drsuapi,
					      struct libnet_BecomeDC_Partition *partition,
					      void (*recv_fn)(struct tevent_req *subreq))
{
	struct composite_context *c = s->creq;
	struct drsuapi_DsReplicaUpdateRefs *r;
	const char *ntds_guid_str;
	const char *ntds_dns_name;
	struct tevent_req *subreq;

	r = talloc(s, struct drsuapi_DsReplicaUpdateRefs);
	if (composite_nomem(r, c)) return;

	ntds_guid_str = GUID_string(r, &s->dest_dsa.ntds_guid);
	if (composite_nomem(ntds_guid_str, c)) return;

	ntds_dns_name = talloc_asprintf(r, "%s._msdcs.%s",
					ntds_guid_str,
					s->domain.dns_name);
	if (composite_nomem(ntds_dns_name, c)) return;

	r->in.bind_handle		= &drsuapi->bind_handle;
	r->in.level			= 1;
	r->in.req.req1.naming_context	= &partition->nc;
	r->in.req.req1.dest_dsa_dns_name= ntds_dns_name;
	r->in.req.req1.dest_dsa_guid	= s->dest_dsa.ntds_guid;
	r->in.req.req1.options		= DRSUAPI_DRS_ADD_REF | DRSUAPI_DRS_DEL_REF;

	/* I think this is how we mark ourselves as a RODC */
	if (!lpcfg_parm_bool(s->libnet->lp_ctx, NULL, "repl", "RODC", false)) {
		r->in.req.req1.options |= DRSUAPI_DRS_WRIT_REP;
	}

	s->ndr_struct_ptr = r;
	subreq = dcerpc_drsuapi_DsReplicaUpdateRefs_r_send(s, c->event_ctx,
							   drsuapi->drsuapi_handle,
							   r);
	if (composite_nomem(subreq, c)) return;
	tevent_req_set_callback(subreq, recv_fn, s);
}

static void becomeDC_drsuapi2_update_refs_config_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi2_update_refs_schema_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsReplicaUpdateRefs *r = talloc_get_type_abort(s->ndr_struct_ptr,
					   struct drsuapi_DsReplicaUpdateRefs);

	s->ndr_struct_ptr = NULL;

	c->status = dcerpc_drsuapi_DsReplicaUpdateRefs_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(r->out.result)) {
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	talloc_free(r);

	becomeDC_drsuapi_update_refs_send(s, &s->drsuapi2, &s->config_part,
					  becomeDC_drsuapi2_update_refs_config_recv);
}

static void becomeDC_drsuapi2_update_refs_domain_recv(struct tevent_req *subreq);

static void becomeDC_drsuapi2_update_refs_config_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsReplicaUpdateRefs *r = talloc_get_type(s->ndr_struct_ptr,
					   struct drsuapi_DsReplicaUpdateRefs);

	s->ndr_struct_ptr = NULL;

	c->status = dcerpc_drsuapi_DsReplicaUpdateRefs_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(r->out.result)) {
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	talloc_free(r);

	becomeDC_drsuapi_update_refs_send(s, &s->drsuapi2, &s->domain_part,
					  becomeDC_drsuapi2_update_refs_domain_recv);
}

static void becomeDC_drsuapi2_update_refs_domain_recv(struct tevent_req *subreq)
{
	struct libnet_BecomeDC_state *s = tevent_req_callback_data(subreq,
					  struct libnet_BecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsReplicaUpdateRefs *r = talloc_get_type(s->ndr_struct_ptr,
					   struct drsuapi_DsReplicaUpdateRefs);

	s->ndr_struct_ptr = NULL;

	c->status = dcerpc_drsuapi_DsReplicaUpdateRefs_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(r->out.result)) {
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	talloc_free(r);

	/* TODO: use DDNS updates and register dns names */
	composite_done(c);
}

static NTSTATUS becomeDC_ldap2_modify_computer(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_message *msg;
	unsigned int i;
	uint32_t user_account_control = UF_SERVER_TRUST_ACCOUNT |
					UF_TRUSTED_FOR_DELEGATION;

	/* as the value is already as we want it to be, we're done */
	if (s->dest_dsa.user_account_control == user_account_control) {
		return NT_STATUS_OK;
	}

	/* make a 'modify' msg, and only for serverReference */
	msg = ldb_msg_new(s);
	NT_STATUS_HAVE_NO_MEMORY(msg);
	msg->dn = ldb_dn_new(msg, s->ldap2.ldb, s->dest_dsa.computer_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(msg->dn);

	ret = samdb_msg_add_uint(s->ldap2.ldb, msg, msg, "userAccountControl",
				 user_account_control);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	/* mark all the message elements (should be just one)
	   as LDB_FLAG_MOD_REPLACE */
	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	ret = ldb_modify(s->ldap2.ldb, msg);
	talloc_free(msg);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	}

	s->dest_dsa.user_account_control = user_account_control;

	return NT_STATUS_OK;
}

static NTSTATUS becomeDC_ldap2_move_computer(struct libnet_BecomeDC_state *s)
{
	int ret;
	struct ldb_dn *old_dn;
	struct ldb_dn *new_dn;

	ret = dsdb_wellknown_dn(s->ldap2.ldb, s,
				ldb_get_default_basedn(s->ldap2.ldb),
				DS_GUID_DOMAIN_CONTROLLERS_CONTAINER,
				&new_dn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	}

	if (!ldb_dn_add_child_fmt(new_dn, "CN=%s", s->dest_dsa.netbios_name)) {
		talloc_free(new_dn);
		return NT_STATUS_NO_MEMORY;
	}

	old_dn = ldb_dn_new(new_dn, s->ldap2.ldb, s->dest_dsa.computer_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(old_dn);

	if (ldb_dn_compare(old_dn, new_dn) == 0) {
		/* we don't need to rename if the old and new dn match */
		talloc_free(new_dn);
		return NT_STATUS_OK;
	}

	ret = ldb_rename(s->ldap2.ldb, old_dn, new_dn);
	if (ret != LDB_SUCCESS) {
		talloc_free(new_dn);
		return NT_STATUS_LDAP(ret);
	}

	s->dest_dsa.computer_dn_str = ldb_dn_alloc_linearized(s, new_dn);
	NT_STATUS_HAVE_NO_MEMORY(s->dest_dsa.computer_dn_str);

	talloc_free(new_dn);

	return NT_STATUS_OK;
}

static void becomeDC_connect_ldap2(struct libnet_BecomeDC_state *s)
{
	struct composite_context *c = s->creq;

	c->status = becomeDC_ldap_connect(s, &s->ldap2);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap2_modify_computer(s);
	if (!composite_is_ok(c)) return;

	c->status = becomeDC_ldap2_move_computer(s);
	if (!composite_is_ok(c)) return;

	becomeDC_drsuapi3_pull_domain_send(s);
}

struct composite_context *libnet_BecomeDC_send(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_BecomeDC *r)
{
	struct composite_context *c;
	struct libnet_BecomeDC_state *s;
	char *tmp_name;

	c = composite_create(mem_ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct libnet_BecomeDC_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;
	s->creq		= c;
	s->libnet	= ctx;

	/* Domain input */
	s->domain.dns_name	= talloc_strdup(s, r->in.domain_dns_name);
	if (composite_nomem(s->domain.dns_name, c)) return c;
	s->domain.netbios_name	= talloc_strdup(s, r->in.domain_netbios_name);
	if (composite_nomem(s->domain.netbios_name, c)) return c;
	s->domain.sid		= dom_sid_dup(s, r->in.domain_sid);
	if (composite_nomem(s->domain.sid, c)) return c;

	/* Source DSA input */
	s->source_dsa.address	= talloc_strdup(s, r->in.source_dsa_address);
	if (composite_nomem(s->source_dsa.address, c)) return c;

	/* Destination DSA input */
	s->dest_dsa.netbios_name= talloc_strdup(s, r->in.dest_dsa_netbios_name);
	if (composite_nomem(s->dest_dsa.netbios_name, c)) return c;

	/* Destination DSA dns_name construction */
	tmp_name	= strlower_talloc(s, s->dest_dsa.netbios_name);
	if (composite_nomem(tmp_name, c)) return c;
	tmp_name	= talloc_asprintf_append_buffer(tmp_name, ".%s",s->domain.dns_name);
	if (composite_nomem(tmp_name, c)) return c;
	s->dest_dsa.dns_name	= tmp_name;

	/* Callback function pointers */
	s->callbacks = r->in.callbacks;

        /* RODC join*/
        s->rodc_join = r->in.rodc_join;

	becomeDC_send_cldap(s);
	return c;
}

NTSTATUS libnet_BecomeDC_recv(struct composite_context *c, TALLOC_CTX *mem_ctx, struct libnet_BecomeDC *r)
{
	NTSTATUS status;

	status = composite_wait(c);

	ZERO_STRUCT(r->out);

	talloc_free(c);
	return status;
}

NTSTATUS libnet_BecomeDC(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_BecomeDC *r)
{
	NTSTATUS status;
	struct composite_context *c;
	c = libnet_BecomeDC_send(ctx, mem_ctx, r);
	status = libnet_BecomeDC_recv(c, mem_ctx, r);
	return status;
}

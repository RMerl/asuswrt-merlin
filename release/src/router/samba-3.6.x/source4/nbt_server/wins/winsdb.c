/* 
   Unix SMB/CIFS implementation.

   WINS database routines

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Stefan Metzmacher	2005
      
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
#include "nbt_server/nbt_server.h"
#include "nbt_server/wins/winsdb.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "librpc/gen_ndr/ndr_nbt.h"
#include "system/time.h"
#include "ldb_wrap.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"

uint64_t winsdb_get_maxVersion(struct winsdb_handle *h)
{
	int ret;
	struct ldb_context *ldb = h->ldb;
	struct ldb_dn *dn;
	struct ldb_result *res = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	uint64_t maxVersion = 0;

	dn = ldb_dn_new(tmp_ctx, ldb, "CN=VERSION");
	if (!dn) goto failed;

	/* find the record in the WINS database */
	ret = ldb_search(ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) goto failed;
	if (res->count > 1) goto failed;

	if (res->count == 1) {
		maxVersion = ldb_msg_find_attr_as_uint64(res->msgs[0], "maxVersion", 0);
	}

failed:
	talloc_free(tmp_ctx);
	return maxVersion;
}

/*
 if newVersion == 0 return the old maxVersion + 1 and save it
 if newVersion > 0 return MAX(oldMaxVersion, newMaxVersion) and save it
*/
uint64_t winsdb_set_maxVersion(struct winsdb_handle *h, uint64_t newMaxVersion)
{
	int trans;
	int ret;
	struct ldb_dn *dn;
	struct ldb_result *res = NULL;
	struct ldb_message *msg = NULL;
	struct ldb_context *wins_db = h->ldb;
	TALLOC_CTX *tmp_ctx = talloc_new(wins_db);
	uint64_t oldMaxVersion = 0;

	trans = ldb_transaction_start(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	dn = ldb_dn_new(tmp_ctx, wins_db, "CN=VERSION");
	if (!dn) goto failed;

	/* find the record in the WINS database */
	ret = ldb_search(wins_db, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) goto failed;
	if (res->count > 1) goto failed;

	if (res->count == 1) {
		oldMaxVersion = ldb_msg_find_attr_as_uint64(res->msgs[0], "maxVersion", 0);
	}

	if (newMaxVersion == 0) {
		newMaxVersion = oldMaxVersion + 1;
	} else {
		newMaxVersion = MAX(oldMaxVersion, newMaxVersion);
	}

	msg = ldb_msg_new(tmp_ctx);
	if (!msg) goto failed;
	msg->dn = dn;


	ret = ldb_msg_add_empty(msg, "objectClass", LDB_FLAG_MOD_REPLACE, NULL);
	if (ret != LDB_SUCCESS) goto failed;
	ret = ldb_msg_add_string(msg, "objectClass", "winsMaxVersion");
	if (ret != LDB_SUCCESS) goto failed;
	ret = ldb_msg_add_empty(msg, "maxVersion", LDB_FLAG_MOD_REPLACE, NULL);
	if (ret != LDB_SUCCESS) goto failed;
	ret = ldb_msg_add_fmt(msg, "maxVersion", "%llu", (long long)newMaxVersion);
	if (ret != LDB_SUCCESS) goto failed;

	ret = ldb_modify(wins_db, msg);
	if (ret != LDB_SUCCESS) ret = ldb_add(wins_db, msg);
	if (ret != LDB_SUCCESS) goto failed;

	trans = ldb_transaction_commit(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	talloc_free(tmp_ctx);
	return newMaxVersion;

failed:
	if (trans == LDB_SUCCESS) ldb_transaction_cancel(wins_db);
	talloc_free(tmp_ctx);
	return 0;
}

uint64_t winsdb_get_seqnumber(struct winsdb_handle *h)
{
	int ret;
	struct ldb_context *ldb = h->ldb;
	struct ldb_dn *dn;
	struct ldb_result *res = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	uint64_t seqnumber = 0;

	dn = ldb_dn_new(tmp_ctx, ldb, "@BASEINFO");
	if (!dn) goto failed;

	/* find the record in the WINS database */
	ret = ldb_search(ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) goto failed;
	if (res->count > 1) goto failed;

	if (res->count == 1) {
		seqnumber = ldb_msg_find_attr_as_uint64(res->msgs[0], "sequenceNumber", 0);
	}

failed:
	talloc_free(tmp_ctx);
	return seqnumber;
}

/*
  return a DN for a nbt_name
*/
static struct ldb_dn *winsdb_dn(TALLOC_CTX *mem_ctx, struct ldb_context *ldb,
				const struct nbt_name *name)
{
	struct ldb_dn *dn;

	dn = ldb_dn_new_fmt(mem_ctx, ldb, "type=0x%02X", name->type);
	if (ldb_dn_is_valid(dn) && name->name && *name->name) {
		ldb_dn_add_child_fmt(dn, "name=%s", name->name);
	}
	if (ldb_dn_is_valid(dn) && name->scope && *name->scope) {
		ldb_dn_add_child_fmt(dn, "scope=%s", name->scope);
	}
	return dn;
}

static NTSTATUS winsdb_nbt_name(TALLOC_CTX *mem_ctx, struct ldb_dn *dn, struct nbt_name **_name)
{
	NTSTATUS status;
	struct nbt_name *name;
	unsigned int comp_num;
	uint32_t cur = 0;

	name = talloc(mem_ctx, struct nbt_name);
	if (!name) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	comp_num = ldb_dn_get_comp_num(dn);

	if (comp_num > 3) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}

	if (comp_num > cur && strcasecmp("scope", ldb_dn_get_component_name(dn, cur)) == 0) {
		name->scope	= (const char *)talloc_strdup(name, (char *)ldb_dn_get_component_val(dn, cur)->data);
		cur++;
	} else {
		name->scope	= NULL;
	}

	if (comp_num > cur && strcasecmp("name", ldb_dn_get_component_name(dn, cur)) == 0) {
		name->name	= (const char *)talloc_strdup(name, (char *)ldb_dn_get_component_val(dn, cur)->data);
		cur++;
	} else {
		name->name	= talloc_strdup(name, "");
		if (!name->name) {
			status = NT_STATUS_NO_MEMORY;
			goto failed;
		}
	}

	if (comp_num > cur && strcasecmp("type", ldb_dn_get_component_name(dn, cur)) == 0) {
		name->type	= strtoul((char *)ldb_dn_get_component_val(dn, cur)->data, NULL, 0);
		cur++;
	} else {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}

	*_name = name;
	return NT_STATUS_OK;
failed:
	talloc_free(name);
	return status;
}

/*
 decode the winsdb_addr("address") attribute:
 "172.31.1.1" or 
 "172.31.1.1;winsOwner:172.31.9.202;expireTime:20050923032330.0Z;"
 are valid records
*/
static NTSTATUS winsdb_addr_decode(struct winsdb_handle *h, struct winsdb_record *rec, struct ldb_val *val,
				   TALLOC_CTX *mem_ctx, struct winsdb_addr **_addr)
{
	NTSTATUS status;
	struct winsdb_addr *addr;
	const char *address;
	const char *wins_owner;
	const char *expire_time;
	char *p;

	addr = talloc(mem_ctx, struct winsdb_addr);
	if (!addr) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	address = (char *)val->data;

	p = strchr(address, ';');
	if (!p) {
		/* support old entries, with only the address */
		addr->address		= (const char *)talloc_steal(addr, val->data);
		addr->wins_owner	= talloc_reference(addr, rec->wins_owner);
		if (!addr->wins_owner) {
			status = NT_STATUS_NO_MEMORY;
			goto failed;
		}
		addr->expire_time	= rec->expire_time;
		*_addr = addr;
		return NT_STATUS_OK;
	}

	*p = '\0'; p++;
	addr->address = talloc_strdup(addr, address);
	if (!addr->address) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	if (strncmp("winsOwner:", p, 10) != 0) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}
	wins_owner = p + 10;
	p = strchr(wins_owner, ';');
	if (!p) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}

	*p = '\0';p++;
	if (strcmp(wins_owner, "0.0.0.0") == 0) {
		wins_owner = h->local_owner;
	}
	addr->wins_owner = talloc_strdup(addr, wins_owner);
	if (!addr->wins_owner) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	if (strncmp("expireTime:", p, 11) != 0) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}

	expire_time = p + 11;
	p = strchr(expire_time, ';');
	if (!p) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}

	*p = '\0';p++;
	addr->expire_time = ldb_string_to_time(expire_time);

	*_addr = addr;
	return NT_STATUS_OK;
failed:
	talloc_free(addr);
	return status;
}

/*
 encode the winsdb_addr("address") attribute like this:
 non-static record:
 "172.31.1.1;winsOwner:172.31.9.202;expireTime:20050923032330.0Z;"
 static record:
 "172.31.1.1"
*/
static int ldb_msg_add_winsdb_addr(struct ldb_message *msg, struct winsdb_record *rec,
				   const char *attr_name, struct winsdb_addr *addr)
{
	const char *str;

	if (rec->is_static) {
		str = talloc_strdup(msg, addr->address);
		if (!str) return LDB_ERR_OPERATIONS_ERROR;
	} else {
		char *expire_time;
		expire_time = ldb_timestring(msg, addr->expire_time);
		if (!expire_time) return LDB_ERR_OPERATIONS_ERROR;
		str = talloc_asprintf(msg, "%s;winsOwner:%s;expireTime:%s;",
				      addr->address, addr->wins_owner,
				      expire_time);
		talloc_free(expire_time);
		if (!str) return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_msg_add_string(msg, attr_name, str);
}

struct winsdb_addr **winsdb_addr_list_make(TALLOC_CTX *mem_ctx)
{
	struct winsdb_addr **addresses;

	addresses = talloc_array(mem_ctx, struct winsdb_addr *, 1);
	if (!addresses) return NULL;

	addresses[0] = NULL;

	return addresses;
}

static int winsdb_addr_sort_list (struct winsdb_addr **p1, struct winsdb_addr **p2, void *opaque)
{
	struct winsdb_addr *a1 = talloc_get_type(*p1, struct winsdb_addr);
	struct winsdb_addr *a2 = talloc_get_type(*p2, struct winsdb_addr);
	struct winsdb_handle *h= talloc_get_type(opaque, struct winsdb_handle);
	bool a1_owned = false;
	bool a2_owned = false;

	/*
	 * first the owned addresses with the newest to the oldest address
	 * then the replica addresses with the newest to the oldest address
	 */
	if (a2->expire_time != a1->expire_time) {
		return a2->expire_time - a1->expire_time;
	}

	if (strcmp(a2->wins_owner, h->local_owner) == 0) {
		a2_owned = true;
	}

	if (strcmp(a1->wins_owner, h->local_owner) == 0) {
		a1_owned = true;
	}

	return a2_owned - a1_owned;
}

struct winsdb_addr **winsdb_addr_list_add(struct winsdb_handle *h, const struct winsdb_record *rec,
					  struct winsdb_addr **addresses, const char *address,
					  const char *wins_owner, time_t expire_time,
					  bool is_name_registration)
{
	struct winsdb_addr *old_addr = NULL;
	size_t len = 0;
	size_t i;
	bool found_old_replica = false;

	/*
	 * count the addresses and maybe
	 * find an old entry for the new address
	 */
	for (i=0; addresses[i]; i++) {
		if (old_addr) continue;
		if (strcmp(addresses[i]->address, address) == 0) {
			old_addr = addresses[i];
		}
	}
	len = i;

	/*
	 * the address is already there
	 * and we can replace it
	 */
	if (old_addr) {
		goto remove_old_addr;
	}

	/*
	 * if we don't have 25 addresses already,
	 * we can just add the new address
	 */
	if (len < 25) {
		goto add_new_addr;
	}

	/*
	 * if we haven't found the address,
	 * and we have already have 25 addresses
	 * if so then we need to do the following:
	 * - if it isn't a name registration, then just ignore the new address
	 * - if it is a name registration, then first search for 
	 *   the oldest replica and if there's no replica address
	 *   search the oldest owned address
	 */
	if (!is_name_registration) {
		return addresses;
	}

	/*
	 * find the oldest replica address, if there's no replica
	 * record at all, find the oldest owned address
	 */
	for (i=0; addresses[i]; i++) {
		bool cur_is_replica = false;
		/* find out if the current address is a replica */
		if (strcmp(addresses[i]->wins_owner, h->local_owner) != 0) {
			cur_is_replica = true;
		}

		/*
		 * if we already found a replica address and the current address
		 * is not a replica, then skip it
		 */
		if (found_old_replica && !cur_is_replica) continue;

		/*
		 * if we found the first replica address, reset the address
		 * that would be replaced
		 */
		if (!found_old_replica && cur_is_replica) {
			found_old_replica = true;
			old_addr = addresses[i];
			continue;
		}

		/*
		 * if the first address isn't a replica, just start with 
		 * the first one
		 */
		if (!old_addr) {
			old_addr = addresses[i];
			continue;
		}

		/*
		 * see if we find an older address
		 */
		if (addresses[i]->expire_time < old_addr->expire_time) {
			old_addr = addresses[i];
			continue;
		}
	}

remove_old_addr:
	winsdb_addr_list_remove(addresses, old_addr->address);
	len --;

add_new_addr:
	addresses = talloc_realloc(addresses, addresses, struct winsdb_addr *, len + 2);
	if (!addresses) return NULL;

	addresses[len] = talloc(addresses, struct winsdb_addr);
	if (!addresses[len]) {
		talloc_free(addresses);
		return NULL;
	}

	addresses[len]->address = talloc_strdup(addresses[len], address);
	if (!addresses[len]->address) {
		talloc_free(addresses);
		return NULL;
	}

	addresses[len]->wins_owner = talloc_strdup(addresses[len], wins_owner);
	if (!addresses[len]->wins_owner) {
		talloc_free(addresses);
		return NULL;
	}

	addresses[len]->expire_time = expire_time;

	addresses[len+1] = NULL;

	LDB_TYPESAFE_QSORT(addresses, len+1, h, winsdb_addr_sort_list);

	return addresses;
}

void winsdb_addr_list_remove(struct winsdb_addr **addresses, const char *address)
{
	size_t i;

	for (i=0; addresses[i]; i++) {
		if (strcmp(addresses[i]->address, address) == 0) {
			break;
		}
	}

	for (; addresses[i]; i++) {
		addresses[i] = addresses[i+1];
	}

	return;
}

struct winsdb_addr *winsdb_addr_list_check(struct winsdb_addr **addresses, const char *address)
{
	size_t i;

	for (i=0; addresses[i]; i++) {
		if (strcmp(addresses[i]->address, address) == 0) {
			return addresses[i];
		}
	}

	return NULL;
}

size_t winsdb_addr_list_length(struct winsdb_addr **addresses)
{
	size_t i;
	for (i=0; addresses[i]; i++);
	return i;
}

const char **winsdb_addr_string_list(TALLOC_CTX *mem_ctx, struct winsdb_addr **addresses)
{
	size_t len = winsdb_addr_list_length(addresses);
	const char **str_list=NULL;
	size_t i;

	for (i=0; i < len; i++) {
		str_list = str_list_add(str_list, addresses[i]->address);
		if (!str_list[i]) {
			return NULL;
		}
	}
	talloc_steal(mem_ctx, str_list);
	return str_list;
}

/*
  load a WINS entry from the database
*/
NTSTATUS winsdb_lookup(struct winsdb_handle *h, 
		       const struct nbt_name *name,
		       TALLOC_CTX *mem_ctx,
		       struct winsdb_record **_rec)
{
	NTSTATUS status;
	struct ldb_result *res = NULL;
	int ret;
	struct winsdb_record *rec;
	struct ldb_context *wins_db = h->ldb;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	time_t now = time(NULL);

	/* find the record in the WINS database */
	ret = ldb_search(wins_db, tmp_ctx, &res,
			 winsdb_dn(tmp_ctx, wins_db, name),
			 LDB_SCOPE_BASE, NULL, NULL);

	if (ret != LDB_SUCCESS || res->count > 1) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	} else if (res->count== 0) {
		status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		goto failed;
	}

	status = winsdb_record(h, res->msgs[0], tmp_ctx, now, &rec);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	talloc_steal(mem_ctx, rec);
	talloc_free(tmp_ctx);
	*_rec = rec;
	return NT_STATUS_OK;

failed:
	talloc_free(tmp_ctx);
	return status;
}

NTSTATUS winsdb_record(struct winsdb_handle *h, struct ldb_message *msg, TALLOC_CTX *mem_ctx, time_t now, struct winsdb_record **_rec)
{
	NTSTATUS status;
	struct winsdb_record *rec;
	struct ldb_message_element *el;
	struct nbt_name *name;
	uint32_t i, j, num_values;

	rec = talloc(mem_ctx, struct winsdb_record);
	if (rec == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	status = winsdb_nbt_name(rec, msg->dn, &name);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	if (strlen(name->name) > 15) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}
	if (name->scope && strlen(name->scope) > 238) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}

	/* parse it into a more convenient winsdb_record structure */
	rec->name		= name;
	rec->type		= ldb_msg_find_attr_as_int(msg, "recordType", WREPL_TYPE_UNIQUE);
	rec->state		= ldb_msg_find_attr_as_int(msg, "recordState", WREPL_STATE_RELEASED);
	rec->node		= ldb_msg_find_attr_as_int(msg, "nodeType", WREPL_NODE_B);
	rec->is_static		= ldb_msg_find_attr_as_int(msg, "isStatic", 0);
	rec->expire_time	= ldb_string_to_time(ldb_msg_find_attr_as_string(msg, "expireTime", NULL));
	rec->version		= ldb_msg_find_attr_as_uint64(msg, "versionID", 0);
	rec->wins_owner		= ldb_msg_find_attr_as_string(msg, "winsOwner", NULL);
	rec->registered_by	= ldb_msg_find_attr_as_string(msg, "registeredBy", NULL);
	talloc_steal(rec, rec->wins_owner);
	talloc_steal(rec, rec->registered_by);

	if (!rec->wins_owner || strcmp(rec->wins_owner, "0.0.0.0") == 0) {
		rec->wins_owner = h->local_owner;
	}

	el = ldb_msg_find_element(msg, "address");
	if (el) {
		num_values = el->num_values;
	} else {
		num_values = 0;
	}

	if (rec->type == WREPL_TYPE_UNIQUE || rec->type == WREPL_TYPE_GROUP) {
		if (num_values != 1) {
			status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			goto failed;
		}
	}
	if (rec->state == WREPL_STATE_ACTIVE) {
		if (num_values < 1) {
			status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			goto failed;
		}
	}
	if (num_values > 25) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto failed;
	}

	rec->addresses     = talloc_array(rec, struct winsdb_addr *, num_values+1);
	if (rec->addresses == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	for (i=0,j=0;i<num_values;i++) {
		bool we_are_owner = false;

		status = winsdb_addr_decode(h, rec, &el->values[i], rec->addresses, &rec->addresses[j]);
		if (!NT_STATUS_IS_OK(status)) goto failed;

		if (strcmp(rec->addresses[j]->wins_owner, h->local_owner) == 0) {
			we_are_owner = true;
		}

		/*
		 * the record isn't static and is active
		 * then don't add the address if it's expired,
		 * but only if we're the owner of the address
		 *
		 * This is important for SGROUP records,
		 * because each server thinks he's the owner of the
		 * record and the record isn't replicated on a
		 * name_refresh. So addresses owned by another owner
		 * could expire, but we still need to return them
		 * (as windows does).
		 */
		if (!rec->is_static &&
		    rec->addresses[j]->expire_time <= now &&
		    rec->state == WREPL_STATE_ACTIVE &&
		    we_are_owner) {
			DEBUG(5,("WINS: expiring name addr %s of %s (expired at %s)\n", 
				 rec->addresses[j]->address, nbt_name_string(rec->addresses[j], rec->name),
				 timestring(rec->addresses[j], rec->addresses[j]->expire_time)));
			talloc_free(rec->addresses[j]);
			rec->addresses[j] = NULL;
			continue;
		}
		j++;
	}
	rec->addresses[j] = NULL;
	num_values = j;

	if (rec->is_static && rec->state == WREPL_STATE_ACTIVE) {
		rec->expire_time = get_time_t_max();
		for (i=0;rec->addresses[i];i++) {
			rec->addresses[i]->expire_time = rec->expire_time;
		}
	}

	if (rec->state == WREPL_STATE_ACTIVE) {
		if (num_values < 1) {
			DEBUG(5,("WINS: expiring name %s (because it has no active addresses)\n", 
				 nbt_name_string(mem_ctx, rec->name)));
			rec->state = WREPL_STATE_RELEASED;
		}
	}

	*_rec = rec;
	return NT_STATUS_OK;
failed:
	if (NT_STATUS_EQUAL(NT_STATUS_INTERNAL_DB_CORRUPTION, status)) {
		DEBUG(1,("winsdb_record: corrupted record: %s\n", ldb_dn_get_linearized(msg->dn)));
	}
	talloc_free(rec);
	return status;
}

/*
  form a ldb_message from a winsdb_record
*/
static struct ldb_message *winsdb_message(struct ldb_context *ldb,
					  struct winsdb_record *rec,
					  TALLOC_CTX *mem_ctx)
{
	int i, ret;
	size_t addr_count;
	const char *expire_time;
	struct ldb_message *msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) goto failed;

	/* make sure we don't put in corrupted records */
	addr_count = winsdb_addr_list_length(rec->addresses);
	if (rec->state == WREPL_STATE_ACTIVE && addr_count == 0) {
		rec->state = WREPL_STATE_RELEASED;
	}
	if (rec->type == WREPL_TYPE_UNIQUE && addr_count > 1) {
		rec->type = WREPL_TYPE_MHOMED;
	}

	expire_time = ldb_timestring(msg, rec->expire_time);
	if (!expire_time) {
		goto failed;
	}

	msg->dn = winsdb_dn(msg, ldb, rec->name);
	if (msg->dn == NULL) goto failed;
	ret = ldb_msg_add_fmt(msg, "type", "0x%02X", rec->name->type);
	if (rec->name->name && *rec->name->name) {
		ret |= ldb_msg_add_string(msg, "name", rec->name->name);
	}
	if (rec->name->scope && *rec->name->scope) {
		ret |= ldb_msg_add_string(msg, "scope", rec->name->scope);
	}
	ret |= ldb_msg_add_fmt(msg, "objectClass", "winsRecord");
	ret |= ldb_msg_add_fmt(msg, "recordType", "%u", rec->type);
	ret |= ldb_msg_add_fmt(msg, "recordState", "%u", rec->state);
	ret |= ldb_msg_add_fmt(msg, "nodeType", "%u", rec->node);
	ret |= ldb_msg_add_fmt(msg, "isStatic", "%u", rec->is_static);
	ret |= ldb_msg_add_empty(msg, "expireTime", 0, NULL);
	if (!(rec->is_static && rec->state == WREPL_STATE_ACTIVE)) {
		ret |= ldb_msg_add_string(msg, "expireTime", expire_time);
	}
	ret |= ldb_msg_add_fmt(msg, "versionID", "%llu", (long long)rec->version);
	ret |= ldb_msg_add_string(msg, "winsOwner", rec->wins_owner);
	ret |= ldb_msg_add_empty(msg, "address", 0, NULL);
	for (i=0;rec->addresses[i];i++) {
		ret |= ldb_msg_add_winsdb_addr(msg, rec, "address", rec->addresses[i]);
	}
	if (rec->registered_by) {
		ret |= ldb_msg_add_empty(msg, "registeredBy", 0, NULL);
		ret |= ldb_msg_add_string(msg, "registeredBy", rec->registered_by);
	}
	if (ret != LDB_SUCCESS) goto failed;
	return msg;

failed:
	talloc_free(msg);
	return NULL;
}

/*
  save a WINS record into the database
*/
uint8_t winsdb_add(struct winsdb_handle *h, struct winsdb_record *rec, uint32_t flags)
{
	struct ldb_message *msg;
	struct ldb_context *wins_db = h->ldb;
	TALLOC_CTX *tmp_ctx = talloc_new(wins_db);
	int trans = -1;
	int ret;

	trans = ldb_transaction_start(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	if (flags & WINSDB_FLAG_ALLOC_VERSION) {
		/* passing '0' means auto-allocate a new one */
		rec->version = winsdb_set_maxVersion(h, 0);
		if (rec->version == 0) goto failed;
	}
	if (flags & WINSDB_FLAG_TAKE_OWNERSHIP) {
		rec->wins_owner = h->local_owner;
	}

	msg = winsdb_message(wins_db, rec, tmp_ctx);
	if (msg == NULL) goto failed;
	ret = ldb_add(wins_db, msg);
	if (ret != LDB_SUCCESS) goto failed;

	trans = ldb_transaction_commit(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	wins_hook(h, rec, WINS_HOOK_ADD, h->hook_script);

	talloc_free(tmp_ctx);
	return NBT_RCODE_OK;

failed:
	if (trans == LDB_SUCCESS) ldb_transaction_cancel(wins_db);
	talloc_free(tmp_ctx);
	return NBT_RCODE_SVR;
}


/*
  modify a WINS record in the database
*/
uint8_t winsdb_modify(struct winsdb_handle *h, struct winsdb_record *rec, uint32_t flags)
{
	struct ldb_message *msg;
	struct ldb_context *wins_db = h->ldb;
	TALLOC_CTX *tmp_ctx = talloc_new(wins_db);
	int trans;
	int ret;
	unsigned int i;

	trans = ldb_transaction_start(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	if (flags & WINSDB_FLAG_ALLOC_VERSION) {
		/* passing '0' means auto-allocate a new one */
		rec->version = winsdb_set_maxVersion(h, 0);
		if (rec->version == 0) goto failed;
	}
	if (flags & WINSDB_FLAG_TAKE_OWNERSHIP) {
		rec->wins_owner = h->local_owner;
	}

	msg = winsdb_message(wins_db, rec, tmp_ctx);
	if (msg == NULL) goto failed;

	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	ret = ldb_modify(wins_db, msg);
	if (ret != LDB_SUCCESS) goto failed;

	trans = ldb_transaction_commit(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	wins_hook(h, rec, WINS_HOOK_MODIFY, h->hook_script);

	talloc_free(tmp_ctx);
	return NBT_RCODE_OK;

failed:
	if (trans == LDB_SUCCESS) ldb_transaction_cancel(wins_db);
	talloc_free(tmp_ctx);
	return NBT_RCODE_SVR;
}


/*
  delete a WINS record from the database
*/
uint8_t winsdb_delete(struct winsdb_handle *h, struct winsdb_record *rec)
{
	struct ldb_context *wins_db = h->ldb;
	TALLOC_CTX *tmp_ctx = talloc_new(wins_db);
	struct ldb_dn *dn;
	int trans;
	int ret;

	trans = ldb_transaction_start(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	dn = winsdb_dn(tmp_ctx, wins_db, rec->name);
	if (dn == NULL) goto failed;

	ret = ldb_delete(wins_db, dn);
	if (ret != LDB_SUCCESS) goto failed;

	trans = ldb_transaction_commit(wins_db);
	if (trans != LDB_SUCCESS) goto failed;

	wins_hook(h, rec, WINS_HOOK_DELETE, h->hook_script);

	talloc_free(tmp_ctx);
	return NBT_RCODE_OK;

failed:
	if (trans == LDB_SUCCESS) ldb_transaction_cancel(wins_db);
	talloc_free(tmp_ctx);
	return NBT_RCODE_SVR;
}

static bool winsdb_check_or_add_module_list(struct tevent_context *ev_ctx, 
					    struct loadparm_context *lp_ctx, struct winsdb_handle *h)
{
	int trans;
	int ret;
	struct ldb_dn *dn;
	struct ldb_result *res = NULL;
	struct ldb_message *msg = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(h);
	unsigned int flags = 0;

	trans = ldb_transaction_start(h->ldb);
	if (trans != LDB_SUCCESS) goto failed;

	/* check if we have a special @MODULES record already */
	dn = ldb_dn_new(tmp_ctx, h->ldb, "@MODULES");
	if (!dn) goto failed;

	/* find the record in the WINS database */
	ret = ldb_search(h->ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) goto failed;

	if (res->count > 0) goto skip;

	/* if there's no record, add one */
	msg = ldb_msg_new(tmp_ctx);
	if (!msg) goto failed;
	msg->dn = dn;

	ret = ldb_msg_add_string(msg, "@LIST", "wins_ldb");
	if (ret != LDB_SUCCESS) goto failed;

	ret = ldb_add(h->ldb, msg);
	if (ret != LDB_SUCCESS) goto failed;

	trans = ldb_transaction_commit(h->ldb);
	if (trans != LDB_SUCCESS) goto failed;

	/* close and reopen the database, with the modules */
	trans = LDB_ERR_OTHER;
	talloc_free(h->ldb);
	h->ldb = NULL;

	if (lpcfg_parm_bool(lp_ctx, NULL,"winsdb", "nosync", false)) {
		flags |= LDB_FLG_NOSYNC;
	}

	h->ldb = ldb_wrap_connect(h, ev_ctx, lp_ctx, lock_path(h, lp_ctx, lpcfg_wins_url(lp_ctx)),
				  NULL, NULL, flags);
	if (!h->ldb) goto failed;

	talloc_free(tmp_ctx);
	return true;

skip:
	if (trans == LDB_SUCCESS) ldb_transaction_cancel(h->ldb);
	talloc_free(tmp_ctx);
	return true;

failed:
	if (trans == LDB_SUCCESS) ldb_transaction_cancel(h->ldb);
	talloc_free(tmp_ctx);
	return false;
}

struct winsdb_handle *winsdb_connect(TALLOC_CTX *mem_ctx, 
				     struct tevent_context *ev_ctx,
				     struct loadparm_context *lp_ctx,
				     const char *owner,
				     enum winsdb_handle_caller caller)
{
	struct winsdb_handle *h = NULL;
	unsigned int flags = 0;
	bool ret;
	int ldb_err;

	h = talloc_zero(mem_ctx, struct winsdb_handle);
	if (!h) return NULL;

	if (lpcfg_parm_bool(lp_ctx, NULL,"winsdb", "nosync", false)) {
		flags |= LDB_FLG_NOSYNC;
	}

	h->ldb = ldb_wrap_connect(h, ev_ctx, lp_ctx, lock_path(h, lp_ctx, lpcfg_wins_url(lp_ctx)),
				  NULL, NULL, flags);
	if (!h->ldb) goto failed;

	h->caller = caller;
	h->hook_script = lpcfg_wins_hook(lp_ctx);

	h->local_owner = talloc_strdup(h, owner);
	if (!h->local_owner) goto failed;

	/* make sure the module list is available and used */
	ret = winsdb_check_or_add_module_list(ev_ctx, lp_ctx, h);
	if (!ret) goto failed;

	ldb_err = ldb_set_opaque(h->ldb, "winsdb_handle", h);
	if (ldb_err != LDB_SUCCESS) goto failed;

	return h;
failed:
	talloc_free(h);
	return NULL;
}


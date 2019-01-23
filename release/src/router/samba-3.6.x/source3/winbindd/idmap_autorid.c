/*
 *  idmap_autorid: static map between Active Directory/NT RIDs
 *  and RFC 2307 accounts
 *
 *  based on the idmap_rid module, but this module defines the ranges
 *  for the domains by automatically allocating a range for each domain
 *
 *  Copyright (C) Christian Ambach, 2010-2011
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "includes.h"
#include "system/filesys.h"
#include "winbindd.h"
#include "dbwrap.h"
#include "idmap.h"
#include "../libcli/security/dom_sid.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

#define HWM "NEXT RANGE"
#define ALLOC_HWM_UID "NEXT ALLOC UID"
#define ALLOC_HWM_GID "NEXT ALLOC GID"
#define ALLOC_RANGE "ALLOC"
#define CONFIGKEY "CONFIG"

struct autorid_global_config {
	uint32_t minvalue;
	uint32_t rangesize;
	uint32_t maxranges;
};

struct autorid_domain_config {
	fstring sid;
	uint32_t domainnum;
	struct autorid_global_config *globalcfg;
};

/* handle to the tdb storing domain <-> range assignments */
static struct db_context *autorid_db;

static NTSTATUS idmap_autorid_get_domainrange(struct db_context *db,
					      void *private_data)
{
	NTSTATUS ret;
	uint32_t domainnum, hwm;
	char *numstr;
	struct autorid_domain_config *cfg;

	cfg = (struct autorid_domain_config *)private_data;

	if (!dbwrap_fetch_uint32(db, cfg->sid, &domainnum)) {
		DEBUG(10, ("Acquiring new range for domain %s\n", cfg->sid));

		/* fetch the current HWM */
		if (!dbwrap_fetch_uint32(db, HWM, &hwm)) {
			DEBUG(1, ("Fatal error while fetching current "
				  "HWM value!\n"));
			ret = NT_STATUS_INTERNAL_ERROR;
			goto error;
		}

		/* do we have a range left? */
		if (hwm >= cfg->globalcfg->maxranges) {
			DEBUG(1, ("No more domain ranges available!\n"));
			ret = NT_STATUS_NO_MEMORY;
			goto error;
		}

		/* increase the HWM */
		ret = dbwrap_change_uint32_atomic(db, HWM, &domainnum, 1);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1, ("Fatal error while fetching a new "
				  "domain range value!\n"));
			goto error;
		}

		/* store away the new mapping in both directions */
		ret = dbwrap_trans_store_uint32(db, cfg->sid, domainnum);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1, ("Fatal error while storing new "
				  "domain->range assignment!\n"));
			goto error;
		}

		numstr = talloc_asprintf(db, "%u", domainnum);
		if (!numstr) {
			ret = NT_STATUS_NO_MEMORY;
			goto error;
		}

		ret = dbwrap_trans_store_bystring(db, numstr,
				string_term_tdb_data(cfg->sid), TDB_INSERT);

		talloc_free(numstr);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1, ("Fatal error while storing "
				  "new domain->range assignment!\n"));
			goto error;
		}
		DEBUG(5, ("Acquired new range #%d for domain %s\n",
			  domainnum, cfg->sid));
	}

	DEBUG(10, ("Using range #%d for domain %s\n", domainnum, cfg->sid));
	cfg->domainnum = domainnum;

	return NT_STATUS_OK;

      error:
	return ret;

}

static NTSTATUS idmap_autorid_id_to_sid(struct autorid_global_config *cfg,
					struct id_map *map)
{
	uint32_t range;
	TDB_DATA data;
	char *keystr;
	struct dom_sid sid;

	/* can this be one of our ids? */
	if (map->xid.id < cfg->minvalue) {
		DEBUG(10, ("id %d is lower than minimum value, "
			   "ignoring mapping request\n", map->xid.id));
		map->status = ID_UNKNOWN;
		return NT_STATUS_OK;
	}

	if (map->xid.id > (cfg->minvalue + cfg->rangesize * cfg->maxranges)) {
		DEBUG(10, ("id %d is outside of maximum id value, "
			   "ignoring mapping request\n", map->xid.id));
		map->status = ID_UNKNOWN;
		return NT_STATUS_OK;
	}

	/* determine the range of this uid */
	range = ((map->xid.id - cfg->minvalue) / cfg->rangesize);

	keystr = talloc_asprintf(talloc_tos(), "%u", range);
	if (!keystr) {
		return NT_STATUS_NO_MEMORY;
	}

	data = dbwrap_fetch_bystring(autorid_db, talloc_tos(), keystr);
	TALLOC_FREE(keystr);

	if (!data.dptr) {
		DEBUG(4, ("id %d belongs to range %d which does not have "
			  "domain mapping, ignoring mapping request\n",
			  map->xid.id, range));
		TALLOC_FREE(data.dptr);
		map->status = ID_UNKNOWN;
		return NT_STATUS_OK;
	}

	if (strncmp((const char *)data.dptr,
		    ALLOC_RANGE,
		    strlen(ALLOC_RANGE)) == 0) {
		/* this is from the alloc range, there is no mapping back */
		DEBUG(5, ("id %d belongs to alloc range, cannot map back\n",
			  map->xid.id));
		TALLOC_FREE(data.dptr);
		map->status = ID_UNKNOWN;
		return NT_STATUS_OK;
	}

	string_to_sid(&sid, (const char *)data.dptr);
	TALLOC_FREE(data.dptr);

	sid_compose(map->sid, &sid,
		    (map->xid.id - cfg->minvalue -
		     range * cfg->rangesize));

	/* We **really** should have some way of validating
	   the SID exists and is the correct type here.  But
	   that is a deficiency in the idmap_rid design. */

	map->status = ID_MAPPED;
	return NT_STATUS_OK;
}

/**********************************
 Single sid to id lookup function.
**********************************/

static NTSTATUS idmap_autorid_sid_to_id(struct autorid_global_config *global,
					struct autorid_domain_config *domain,
					struct id_map *map)
{
	uint32_t rid;

	sid_peek_rid(map->sid, &rid);

	/* if the rid is higher than the size of the range, we cannot map it */
	if (rid >= global->rangesize) {
		map->status = ID_UNKNOWN;
		DEBUG(2, ("RID %d is larger then size of range (%d), "
			  "user cannot be mapped\n", rid, global->rangesize));
		return NT_STATUS_UNSUCCESSFUL;
	}
	map->xid.id = global->minvalue +
	    (global->rangesize * domain->domainnum)+rid;

	/* We **really** should have some way of validating
	   the SID exists and is the correct type here.  But
	   that is a deficiency in the idmap_rid design. */

	map->status = ID_MAPPED;

	return NT_STATUS_OK;
}

/**********************************
 lookup a set of unix ids.
**********************************/

static NTSTATUS idmap_autorid_unixids_to_sids(struct idmap_domain *dom,
					      struct id_map **ids)
{
	struct autorid_global_config *globalcfg;
	NTSTATUS ret;
	int i;

	/* initialize the status to avoid surprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	globalcfg = talloc_get_type(dom->private_data,
				    struct autorid_global_config);

	for (i = 0; ids[i]; i++) {

		ret = idmap_autorid_id_to_sid(globalcfg, ids[i]);

		if ((!NT_STATUS_IS_OK(ret)) &&
		    (!NT_STATUS_EQUAL(ret, NT_STATUS_NONE_MAPPED))) {
			/* some fatal error occurred, log it */
			DEBUG(3, ("Unexpected error resolving an ID "
				  " (%d)\n", ids[i]->xid.id));
			goto failure;
		}
	}
	return NT_STATUS_OK;

      failure:
	return ret;
}

/**********************************
 lookup a set of sids.
**********************************/

static NTSTATUS idmap_autorid_sids_to_unixids(struct idmap_domain *dom,
					      struct id_map **ids)
{
	struct autorid_global_config *global;
	NTSTATUS ret;
	int i;

	/* initialize the status to avoid surprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	global = talloc_get_type(dom->private_data,
				 struct autorid_global_config);

	for (i = 0; ids[i]; i++) {
		struct winbindd_tdc_domain *domain;
		struct autorid_domain_config domaincfg;
		uint32_t rid;
		struct dom_sid domainsid;

		ZERO_STRUCT(domaincfg);

		sid_copy(&domainsid, ids[i]->sid);
		if (!sid_split_rid(&domainsid, &rid)) {
			DEBUG(4, ("Could not determine domain SID from %s, "
				  "ignoring mapping request\n",
				  sid_string_dbg(ids[i]->sid)));
			continue;
		}

		/*
		 * Check if the domain is around
		 */
		domain = wcache_tdc_fetch_domainbysid(talloc_tos(),
						      &domainsid);
		if (domain == NULL) {
			DEBUG(10, ("Ignoring unknown domain sid %s\n",
				   sid_string_dbg(&domainsid)));
			continue;
		}
		TALLOC_FREE(domain);

		domaincfg.globalcfg = global;
		sid_to_fstring(domaincfg.sid, &domainsid);

		ret = dbwrap_trans_do(autorid_db,
				      idmap_autorid_get_domainrange,
				      &domaincfg);

		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(3, ("Could not determine range for domain, "
				  "check previous messages for reason\n"));
			goto failure;
		}

		ret = idmap_autorid_sid_to_id(global, &domaincfg, ids[i]);

		if ((!NT_STATUS_IS_OK(ret)) &&
		    (!NT_STATUS_EQUAL(ret, NT_STATUS_NONE_MAPPED))) {
			/* some fatal error occurred, log it */
			DEBUG(3, ("Unexpected error resolving a SID (%s)\n",
				  sid_string_dbg(ids[i]->sid)));
			goto failure;
		}
	}
	return NT_STATUS_OK;

      failure:
	return ret;

}

/* initialize the given HWM to 0 if it does not exist yet */
static NTSTATUS idmap_autorid_init_hwm(const char *hwm) {

	NTSTATUS status;
	int32_t hwmval;

	hwmval = dbwrap_fetch_int32(autorid_db, hwm);
	if ((hwmval < 0))  {
		status = dbwrap_trans_store_int32(autorid_db, hwm, 0);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,
			      ("Unable to initialise HWM (%s) in autorid "
			       "database: %s\n", hwm, nt_errstr(status)));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}
	}

	return NT_STATUS_OK;
}

/*
 * open and initialize the database which stores the ranges for the domains
 */
static NTSTATUS idmap_autorid_db_init(void)
{
	NTSTATUS status;

	if (autorid_db) {
		/* its already open */
		return NT_STATUS_OK;
	}

	/* Open idmap repository */
	autorid_db = db_open(NULL, state_path("autorid.tdb"), 0,
			     TDB_DEFAULT, O_RDWR | O_CREAT, 0644);

	if (!autorid_db) {
		DEBUG(0, ("Unable to open idmap_autorid database '%s'\n",
			  state_path("autorid.tdb")));
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Initialize high water mark for the currently used range to 0 */

	status = idmap_autorid_init_hwm(HWM);
	NT_STATUS_NOT_OK_RETURN(status);

	status = idmap_autorid_init_hwm(ALLOC_HWM_UID);
	NT_STATUS_NOT_OK_RETURN(status);

	status = idmap_autorid_init_hwm(ALLOC_HWM_GID);

	return status;
}

static struct autorid_global_config *idmap_autorid_loadconfig(TALLOC_CTX * ctx)
{

	TDB_DATA data;
	struct autorid_global_config *cfg;
	unsigned long minvalue, rangesize, maxranges;

	data = dbwrap_fetch_bystring(autorid_db, ctx, CONFIGKEY);

	if (!data.dptr) {
		DEBUG(10, ("No saved config found\n"));
		return NULL;
	}

	cfg = TALLOC_ZERO_P(ctx, struct autorid_global_config);
	if (!cfg) {
		return NULL;
	}

	if (sscanf((char *)data.dptr,
		   "minvalue:%lu rangesize:%lu maxranges:%lu",
		   &minvalue, &rangesize, &maxranges) != 3) {
		DEBUG(1,
		      ("Found invalid configuration data"
		       "creating new config\n"));
		return NULL;
	}

	cfg->minvalue = minvalue;
	cfg->rangesize = rangesize;
	cfg->maxranges = maxranges;

	DEBUG(10, ("Loaded previously stored configuration "
		   "minvalue:%d rangesize:%d\n",
		   cfg->minvalue, cfg->rangesize));

	return cfg;

}

static NTSTATUS idmap_autorid_saveconfig(struct autorid_global_config *cfg)
{

	NTSTATUS status;
	TDB_DATA data;
	char *cfgstr;

	cfgstr =
	    talloc_asprintf(talloc_tos(),
			    "minvalue:%u rangesize:%u maxranges:%u",
			    cfg->minvalue, cfg->rangesize, cfg->maxranges);

	if (!cfgstr) {
		return NT_STATUS_NO_MEMORY;
	}

	data = string_tdb_data(cfgstr);

	status = dbwrap_trans_store_bystring(autorid_db, CONFIGKEY,
					     data, TDB_REPLACE);

	talloc_free(cfgstr);

	return status;
}

static NTSTATUS idmap_autorid_initialize(struct idmap_domain *dom)
{
	struct autorid_global_config *config;
	struct autorid_global_config *storedconfig = NULL;
	NTSTATUS status;
	uint32_t hwm;

	if (!strequal(dom->name, "*")) {
		DEBUG(0, ("idmap_autorid_initialize: Error: autorid configured "
			  "for domain '%s'. But autorid can only be used for "
			  "the default idmap configuration.\n", dom->name));
		return NT_STATUS_INVALID_PARAMETER;
	}

	config = TALLOC_ZERO_P(dom, struct autorid_global_config);
	if (!config) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	status = idmap_autorid_db_init();
	if (!NT_STATUS_IS_OK(status)) {
		goto error;
	}

	config->minvalue = dom->low_id;
	config->rangesize = lp_parm_int(-1, "idmap config *", "rangesize", 100000);

	if (config->rangesize < 2000) {
		DEBUG(1, ("autorid rangesize must be at least 2000\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto error;
	}

	config->maxranges = (dom->high_id - dom->low_id + 1) /
	    config->rangesize;

	if (config->maxranges == 0) {
		DEBUG(1, ("allowed uid range is smaller then rangesize, "
			  "increase uid range or decrease rangesize\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto error;
	}

	/* check if the high-low limit is a multiple of the rangesize */
	if ((dom->high_id - dom->low_id + 1) % config->rangesize != 0) {
		DEBUG(5, ("High uid-low uid difference of %d "
			  "is not a multiple of the rangesize %d, "
			  "limiting ranges to lower boundary number of %d\n",
			  (dom->high_id - dom->low_id + 1), config->rangesize,
			  config->maxranges));
	}

	DEBUG(10, ("Current configuration in config is "
		   "minvalue:%d rangesize:%d maxranges:%d\n",
		   config->minvalue, config->rangesize, config->maxranges));

	/* read previously stored config and current HWM */
	storedconfig = idmap_autorid_loadconfig(talloc_tos());

	if (!dbwrap_fetch_uint32(autorid_db, HWM, &hwm)) {
		DEBUG(1, ("Fatal error while fetching current "
			  "HWM value!\n"));
		status = NT_STATUS_INTERNAL_ERROR;
		goto error;
	}

	/* did the minimum value or rangesize change? */
	if (storedconfig &&
	    ((storedconfig->minvalue != config->minvalue) ||
	     (storedconfig->rangesize != config->rangesize))) {
		DEBUG(1, ("New configuration values for rangesize or "
			  "minimum uid value conflict with previously "
			  "used values! Aborting initialization\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto error;
	}

	/*
	 * has the highest uid value been reduced to setting that is not
	 * sufficient any more for already existing ranges?
	 */
	if (hwm > config->maxranges) {
		DEBUG(1, ("New upper uid limit is too low to cover "
			  "existing mappings! Aborting initialization\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto error;
	}

	status = idmap_autorid_saveconfig(config);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to store configuration data!\n"));
		goto error;
	}

	DEBUG(5, ("%d domain ranges with a size of %d are available\n",
		  config->maxranges, config->rangesize));

	dom->private_data = config;

	goto done;

error:
	talloc_free(config);

done:
	talloc_free(storedconfig);

	return status;
}

static NTSTATUS idmap_autorid_allocate_id(struct idmap_domain *dom,
					  struct unixid *xid) {

	NTSTATUS ret;
	struct autorid_global_config *globalcfg;
	struct autorid_domain_config domaincfg;
	uint32_t hwm;
	const char *hwmkey;

	if (!strequal(dom->name, "*")) {
		DEBUG(3, ("idmap_autorid_allocate_id: "
			  "Refusing creation of mapping for domain'%s'. "
			  "Currently only supported for the default "
			  "domain \"*\".\n",
			   dom->name));
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	if ((xid->type != ID_TYPE_UID) && (xid->type != ID_TYPE_GID)) {
		return NT_STATUS_INVALID_PARAMETER;
	}


	globalcfg = talloc_get_type(dom->private_data,
				    struct autorid_global_config);

	/* fetch the range for the allocation pool */

	ZERO_STRUCT(domaincfg);

	domaincfg.globalcfg = globalcfg;
	fstrcpy(domaincfg.sid, ALLOC_RANGE);

	ret = dbwrap_trans_do(autorid_db,
			      idmap_autorid_get_domainrange,
			      &domaincfg);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(3, ("Could not determine range for allocation pool, "
			  "check previous messages for reason\n"));
		return ret;
	}

	/* fetch the current HWM */
	hwmkey = (xid->type==ID_TYPE_UID)?ALLOC_HWM_UID:ALLOC_HWM_GID;

	if (!dbwrap_fetch_uint32(autorid_db, hwmkey, &hwm)) {
		DEBUG(1, ("Failed to fetch current allocation HWM value: %s\n",
			  nt_errstr(ret)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (hwm >= globalcfg->rangesize) {
		DEBUG(1, ("allocation range is depleted!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ret = dbwrap_change_uint32_atomic(autorid_db, hwmkey, &(xid->id), 1);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(1, ("Fatal error while allocating new ID!\n"));
		return ret;
	}

	xid->id = globalcfg->minvalue +
		  globalcfg->rangesize * domaincfg.domainnum +
		  xid->id;

	DEBUG(10, ("Returned new %s %d from allocation range\n",
		   (xid->type==ID_TYPE_UID)?"uid":"gid", xid->id));

	return ret;
}

/*
  Close the idmap tdb instance
*/
static struct idmap_methods autorid_methods = {
	.init = idmap_autorid_initialize,
	.unixids_to_sids = idmap_autorid_unixids_to_sids,
	.sids_to_unixids = idmap_autorid_sids_to_unixids,
	.allocate_id	 = idmap_autorid_allocate_id
};

NTSTATUS idmap_autorid_init(void)
{
	return smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION,
				  "autorid", &autorid_methods);
}

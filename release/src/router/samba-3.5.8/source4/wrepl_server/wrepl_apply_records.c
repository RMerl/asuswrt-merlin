/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
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
#include "smbd/service_task.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "librpc/gen_ndr/ndr_winsrepl.h"
#include "wrepl_server/wrepl_server.h"
#include "nbt_server/wins/winsdb.h"
#include "libcli/wrepl/winsrepl.h"
#include "system/time.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "param/param.h"

enum _R_ACTION {
	R_INVALID,
	R_DO_REPLACE,
	R_NOT_REPLACE,
	R_DO_PROPAGATE,
	R_DO_CHALLENGE,
	R_DO_RELEASE_DEMAND,
	R_DO_SGROUP_MERGE
};

static const char *_R_ACTION_enum_string(enum _R_ACTION action)
{
	switch (action) {
	case R_INVALID:			return "INVALID";
	case R_DO_REPLACE:		return "REPLACE";
	case R_NOT_REPLACE:		return "NOT_REPLACE";
	case R_DO_PROPAGATE:		return "PROPAGATE";
	case R_DO_CHALLENGE:		return "CHALLEGNE";
	case R_DO_RELEASE_DEMAND:	return "RELEASE_DEMAND";
	case R_DO_SGROUP_MERGE:		return "SGROUP_MERGE";
	}

	return "enum _R_ACTION unknown";
}

#define R_IS_ACTIVE(r) ((r)->state == WREPL_STATE_ACTIVE)
#if 0 /* unused */
#define R_IS_RELEASED(r) ((r)->state == WREPL_STATE_RELEASED)
#endif
#define R_IS_TOMBSTONE(r) ((r)->state == WREPL_STATE_TOMBSTONE)

#define R_IS_UNIQUE(r) ((r)->type == WREPL_TYPE_UNIQUE)
#define R_IS_GROUP(r) ((r)->type == WREPL_TYPE_GROUP)
#define R_IS_SGROUP(r) ((r)->type == WREPL_TYPE_SGROUP)
#if 0 /* unused */
#define R_IS_MHOMED(r) ((r)->type == WREPL_TYPE_MHOMED)
#endif

/* blindly overwrite records from the same owner in all cases */
static enum _R_ACTION replace_same_owner(struct winsdb_record *r1, struct wrepl_name *r2)
{
	/* REPLACE */
	return R_DO_REPLACE;
}

static bool r_1_is_subset_of_2_address_list(struct winsdb_record *r1, struct wrepl_name *r2, bool check_owners)
{
	uint32_t i,j;
	size_t len = winsdb_addr_list_length(r1->addresses);

	for (i=0; i < len; i++) {
		bool found = false;
		for (j=0; j < r2->num_addresses; j++) {
			if (strcmp(r1->addresses[i]->address, r2->addresses[j].address) != 0) {
				continue;
			}

			if (check_owners && strcmp(r1->addresses[i]->wins_owner, r2->addresses[j].owner) != 0) {
				return false;
			}
			found = true;
			break;
		}
		if (!found) return false;
	}

	return true;
}

static bool r_1_is_superset_of_2_address_list(struct winsdb_record *r1, struct wrepl_name *r2, bool check_owners)
{
	uint32_t i,j;
	size_t len = winsdb_addr_list_length(r1->addresses);

	for (i=0; i < r2->num_addresses; i++) {
		bool found = false;
		for (j=0; j < len; j++) {
			if (strcmp(r2->addresses[i].address, r1->addresses[j]->address) != 0) {
				continue;
			}

			if (check_owners && strcmp(r2->addresses[i].owner, r1->addresses[j]->wins_owner) != 0) {
				return false;
			}
			found = true;
			break;
		}
		if (!found) return false;
	}

	return true;
}

static bool r_1_is_same_as_2_address_list(struct winsdb_record *r1, struct wrepl_name *r2, bool check_owners)
{
	size_t len = winsdb_addr_list_length(r1->addresses);

	if (len != r2->num_addresses) {
		return false;
	}

	return r_1_is_superset_of_2_address_list(r1, r2, check_owners);
}

static bool r_contains_addrs_from_owner(struct winsdb_record *r1, const char *owner)
{
	uint32_t i;
	size_t len = winsdb_addr_list_length(r1->addresses);

	for (i=0; i < len; i++) {
		if (strcmp(r1->addresses[i]->wins_owner, owner) == 0) {
			return true;
		}
	}

	return false;
}

/*
UNIQUE,ACTIVE vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
UNIQUE,ACTIVE vs. UNIQUE,TOMBSTONE with different ip(s) => NOT REPLACE
UNIQUE,RELEASED vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
UNIQUE,RELEASED vs. UNIQUE,TOMBSTONE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. UNIQUE,TOMBSTONE with different ip(s) => REPLACE
UNIQUE,ACTIVE vs. GROUP,ACTIVE with different ip(s) => REPLACE
UNIQUE,ACTIVE vs. GROUP,TOMBSTONE with same ip(s) => NOT REPLACE
UNIQUE,RELEASED vs. GROUP,ACTIVE with different ip(s) => REPLACE
UNIQUE,RELEASED vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. GROUP,ACTIVE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
UNIQUE,ACTIVE vs. SGROUP,ACTIVE with same ip(s) => NOT REPLACE
UNIQUE,ACTIVE vs. SGROUP,TOMBSTONE with same ip(s) => NOT REPLACE
UNIQUE,RELEASED vs. SGROUP,ACTIVE with different ip(s) => REPLACE
UNIQUE,RELEASED vs. SGROUP,TOMBSTONE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. SGROUP,ACTIVE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. SGROUP,TOMBSTONE with different ip(s) => REPLACE
UNIQUE,ACTIVE vs. MHOMED,ACTIVE with different ip(s) => REPLACE
UNIQUE,ACTIVE vs. MHOMED,TOMBSTONE with same ip(s) => NOT REPLACE
UNIQUE,RELEASED vs. MHOMED,ACTIVE with different ip(s) => REPLACE
UNIQUE,RELEASED vs. MHOMED,TOMBSTONE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. MHOMED,ACTIVE with different ip(s) => REPLACE
UNIQUE,TOMBSTONE vs. MHOMED,TOMBSTONE with different ip(s) => REPLACE
*/
static enum _R_ACTION replace_unique_replica_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (!R_IS_ACTIVE(r1)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (!R_IS_SGROUP(r2) && R_IS_ACTIVE(r2)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	/* NOT REPLACE */
	return R_NOT_REPLACE;
}

/*
GROUP,ACTIVE vs. UNIQUE,ACTIVE with same ip(s) => NOT REPLACE
GROUP,ACTIVE vs. UNIQUE,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,RELEASED vs. UNIQUE,ACTIVE with same ip(s) => NOT REPLACE
GROUP,RELEASED vs. UNIQUE,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,TOMBSTONE vs. UNIQUE,ACTIVE with same ip(s) => NOT REPLACE
GROUP,TOMBSTONE vs. UNIQUE,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,ACTIVE vs. GROUP,ACTIVE with same ip(s) => NOT REPLACE
GROUP,ACTIVE vs. GROUP,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,RELEASED vs. GROUP,ACTIVE with different ip(s) => REPLACE
GROUP,RELEASED vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
GROUP,TOMBSTONE vs. GROUP,ACTIVE with different ip(s) => REPLACE
GROUP,TOMBSTONE vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
GROUP,ACTIVE vs. SGROUP,ACTIVE with same ip(s) => NOT REPLACE
GROUP,ACTIVE vs. SGROUP,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,RELEASED vs. SGROUP,ACTIVE with different ip(s) => REPLACE
GROUP,RELEASED vs. SGROUP,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,TOMBSTONE vs. SGROUP,ACTIVE with different ip(s) => REPLACE
GROUP,TOMBSTONE vs. SGROUP,TOMBSTONE with different ip(s) => REPLACE
GROUP,ACTIVE vs. MHOMED,ACTIVE with same ip(s) => NOT REPLACE
GROUP,ACTIVE vs. MHOMED,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,RELEASED vs. MHOMED,ACTIVE with same ip(s) => NOT REPLACE
GROUP,RELEASED vs. MHOMED,TOMBSTONE with same ip(s) => NOT REPLACE
GROUP,TOMBSTONE vs. MHOMED,ACTIVE with different ip(s) => REPLACE
GROUP,TOMBSTONE vs. MHOMED,TOMBSTONE with different ip(s) => REPLACE
*/
static enum _R_ACTION replace_group_replica_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (!R_IS_ACTIVE(r1) && R_IS_GROUP(r2)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (R_IS_TOMBSTONE(r1) && !R_IS_UNIQUE(r2)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	/* NOT REPLACE */
	return R_NOT_REPLACE;
}

/*
SGROUP,ACTIVE vs. UNIQUE,ACTIVE with same ip(s) => NOT REPLACE
SGROUP,ACTIVE vs. UNIQUE,TOMBSTONE with same ip(s) => NOT REPLACE
SGROUP,RELEASED vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
SGROUP,RELEASED vs. UNIQUE,TOMBSTONE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. UNIQUE,TOMBSTONE with different ip(s) => REPLACE
SGROUP,ACTIVE vs. GROUP,ACTIVE with same ip(s) => NOT REPLACE
SGROUP,ACTIVE vs. GROUP,TOMBSTONE with same ip(s) => NOT REPLACE
SGROUP,RELEASED vs. GROUP,ACTIVE with different ip(s) => REPLACE
SGROUP,RELEASED vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. GROUP,ACTIVE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
SGROUP,RELEASED vs. SGROUP,ACTIVE with different ip(s) => REPLACE
SGROUP,RELEASED vs. SGROUP,TOMBSTONE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. SGROUP,ACTIVE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. SGROUP,TOMBSTONE with different ip(s) => REPLACE
SGROUP,ACTIVE vs. MHOMED,ACTIVE with same ip(s) => NOT REPLACE
SGROUP,ACTIVE vs. MHOMED,TOMBSTONE with same ip(s) => NOT REPLACE
SGROUP,RELEASED vs. MHOMED,ACTIVE with different ip(s) => REPLACE
SGROUP,RELEASED vs. MHOMED,TOMBSTONE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. MHOMED,ACTIVE with different ip(s) => REPLACE
SGROUP,TOMBSTONE vs. MHOMED,TOMBSTONE with different ip(s) => REPLACE

SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:A_3_4 => NOT REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:NULL => NOT REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4_X_3_4 vs. B:A_3_4 => NOT REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4 vs. B:A_3_4 => REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:A_3_4_OWNER_B => REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4_OWNER_B vs. B:A_3_4 => REPLACE

SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:B_3_4 => C:A_3_4_B_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4_X_3_4 vs. B:A_3_4 => B:A_3_4_X_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:X_3_4 vs. B:A_3_4 => C:A_3_4_X_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4_X_3_4 vs. B:A_3_4_OWNER_B => B:A_3_4_OWNER_B_X_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4_X_3_4 vs. B:B_3_4_X_1_2 => C:B_3_4_X_1_2_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4_X_3_4 vs. B:NULL => B:X_3_4 => SGROUP_MERGE

 
this is a bit strange, incoming tombstone replicas always replace old replicas:

SGROUP,ACTIVE vs. SGROUP,TOMBSTONE A:B_3_4_X_3_4 vs. B:NULL => B:NULL => REPLACE
SGROUP,ACTIVE vs. SGROUP,TOMBSTONE A:B_3_4_X_3_4 vs. B:A_3_4 => B:A_3_4 => REPLACE
SGROUP,ACTIVE vs. SGROUP,TOMBSTONE A:B_3_4_X_3_4 vs. B:B_3_4 => B:B_3_4 => REPLACE
SGROUP,ACTIVE vs. SGROUP,TOMBSTONE A:B_3_4_X_3_4 vs. B:B_3_4_X_3_4 => B:B_3_4_X_3_4 => REPLACE
*/
static enum _R_ACTION replace_sgroup_replica_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (!R_IS_ACTIVE(r1)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (!R_IS_SGROUP(r2)) {
		/* NOT REPLACE */
		return R_NOT_REPLACE;
	}

	/*
	 * this is strange, but correct
	 * the incoming tombstone replace the current active
	 * record
	 */
	if (!R_IS_ACTIVE(r2)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (r2->num_addresses == 0) {
		if (r_contains_addrs_from_owner(r1, r2->owner)) {
			/* not handled here: MERGE */
			return R_DO_SGROUP_MERGE;
		}

		/* NOT REPLACE */
		return R_NOT_REPLACE;
	}

	if (r_1_is_superset_of_2_address_list(r1, r2, true)) {
		/* NOT REPLACE */
		return R_NOT_REPLACE;
	}

	if (r_1_is_same_as_2_address_list(r1, r2, false)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	/* not handled here: MERGE */
	return R_DO_SGROUP_MERGE;
}

/*
MHOMED,ACTIVE vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
MHOMED,ACTIVE vs. UNIQUE,TOMBSTONE with same ip(s) => NOT REPLACE
MHOMED,RELEASED vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
MHOMED,RELEASED vs. UNIQUE,TOMBSTONE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. UNIQUE,ACTIVE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. UNIQUE,TOMBSTONE with different ip(s) => REPLACE
MHOMED,ACTIVE vs. GROUP,ACTIVE with different ip(s) => REPLACE
MHOMED,ACTIVE vs. GROUP,TOMBSTONE with same ip(s) => NOT REPLACE
MHOMED,RELEASED vs. GROUP,ACTIVE with different ip(s) => REPLACE
MHOMED,RELEASED vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. GROUP,ACTIVE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. GROUP,TOMBSTONE with different ip(s) => REPLACE
MHOMED,ACTIVE vs. SGROUP,ACTIVE with same ip(s) => NOT REPLACE
MHOMED,ACTIVE vs. SGROUP,TOMBSTONE with same ip(s) => NOT REPLACE
MHOMED,RELEASED vs. SGROUP,ACTIVE with different ip(s) => REPLACE
MHOMED,RELEASED vs. SGROUP,TOMBSTONE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. SGROUP,ACTIVE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. SGROUP,TOMBSTONE with different ip(s) => REPLACE
MHOMED,ACTIVE vs. MHOMED,ACTIVE with different ip(s) => REPLACE
MHOMED,ACTIVE vs. MHOMED,TOMBSTONE with same ip(s) => NOT REPLACE
MHOMED,RELEASED vs. MHOMED,ACTIVE with different ip(s) => REPLACE
MHOMED,RELEASED vs. MHOMED,TOMBSTONE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. MHOMED,ACTIVE with different ip(s) => REPLACE
MHOMED,TOMBSTONE vs. MHOMED,TOMBSTONE with different ip(s) => REPLACE
*/
static enum _R_ACTION replace_mhomed_replica_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (!R_IS_ACTIVE(r1)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (!R_IS_SGROUP(r2) && R_IS_ACTIVE(r2)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	/* NOT REPLACE */
	return R_NOT_REPLACE;
}

/*
active:
_UA_UA_SI_U<00> => REPLACE
_UA_UA_DI_P<00> => NOT REPLACE
_UA_UA_DI_O<00> => NOT REPLACE
_UA_UA_DI_N<00> => REPLACE
_UA_UT_SI_U<00> => NOT REPLACE
_UA_UT_DI_U<00> => NOT REPLACE
_UA_GA_SI_R<00> => REPLACE
_UA_GA_DI_R<00> => REPLACE
_UA_GT_SI_U<00> => NOT REPLACE
_UA_GT_DI_U<00> => NOT REPLACE
_UA_SA_SI_R<00> => REPLACE
_UA_SA_DI_R<00> => REPLACE
_UA_ST_SI_U<00> => NOT REPLACE
_UA_ST_DI_U<00> => NOT REPLACE
_UA_MA_SI_U<00> => REPLACE
_UA_MA_SP_U<00> => REPLACE
_UA_MA_DI_P<00> => NOT REPLACE
_UA_MA_DI_O<00> => NOT REPLACE
_UA_MA_DI_N<00> => REPLACE
_UA_MT_SI_U<00> => NOT REPLACE
_UA_MT_DI_U<00> => NOT REPLACE
Test Replica vs. owned active: some more UNIQUE,MHOMED combinations
_UA_UA_DI_A<00> => MHOMED_MERGE
_UA_MA_DI_A<00> => MHOMED_MERGE

released:
_UR_UA_SI<00> => REPLACE
_UR_UA_DI<00> => REPLACE
_UR_UT_SI<00> => REPLACE
_UR_UT_DI<00> => REPLACE
_UR_GA_SI<00> => REPLACE
_UR_GA_DI<00> => REPLACE
_UR_GT_SI<00> => REPLACE
_UR_GT_DI<00> => REPLACE
_UR_SA_SI<00> => REPLACE
_UR_SA_DI<00> => REPLACE
_UR_ST_SI<00> => REPLACE
_UR_ST_DI<00> => REPLACE
_UR_MA_SI<00> => REPLACE
_UR_MA_DI<00> => REPLACE
_UR_MT_SI<00> => REPLACE
_UR_MT_DI<00> => REPLACE
*/
static enum _R_ACTION replace_unique_owned_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (!R_IS_ACTIVE(r1)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (!R_IS_ACTIVE(r2)) {
		/* NOT REPLACE, and PROPAGATE */
		return R_DO_PROPAGATE;
	}

	if (R_IS_GROUP(r2) || R_IS_SGROUP(r2)) {
		/* REPLACE and send a release demand to the old name owner */
		return R_DO_RELEASE_DEMAND;
	}

	/* 
	 * here we only have unique,active,owned vs.
	 * is unique,active,replica or mhomed,active,replica
	 */

	if (r_1_is_subset_of_2_address_list(r1, r2, false)) {
		/* 
		 * if r1 has a subset(or same) of the addresses of r2
		 * <=>
		 * if r2 has a superset(or same) of the addresses of r1
		 *
		 * then replace the record
		 */
		return R_DO_REPLACE;
	}

	/*
	 * in any other case, we need to do
	 * a name request to the old name holder
	 * to see if it's still there...
	 */
	return R_DO_CHALLENGE;
}

/*
active:
_GA_UA_SI_U<00> => NOT REPLACE
_GA_UA_DI_U<00> => NOT REPLACE
_GA_UT_SI_U<00> => NOT REPLACE
_GA_UT_DI_U<00> => NOT REPLACE
_GA_GA_SI_U<00> => REPLACE
_GA_GA_DI_U<00> => REPLACE
_GA_GT_SI_U<00> => NOT REPLACE
_GA_GT_DI_U<00> => NOT REPLACE
_GA_SA_SI_U<00> => NOT REPLACE
_GA_SA_DI_U<00> => NOT REPLACE
_GA_ST_SI_U<00> => NOT REPLACE
_GA_ST_DI_U<00> => NOT REPLACE
_GA_MA_SI_U<00> => NOT REPLACE
_GA_MA_DI_U<00> => NOT REPLACE
_GA_MT_SI_U<00> => NOT REPLACE
_GA_MT_DI_U<00> => NOT REPLACE

released:
_GR_UA_SI<00> => NOT REPLACE
_GR_UA_DI<00> => NOT REPLACE
_GR_UT_SI<00> => NOT REPLACE
_GR_UT_DI<00> => NOT REPLACE
_GR_GA_SI<00> => REPLACE
_GR_GA_DI<00> => REPLACE
_GR_GT_SI<00> => REPLACE
_GR_GT_DI<00> => REPLACE
_GR_SA_SI<00> => NOT REPLACE
_GR_SA_DI<00> => NOT REPLACE
_GR_ST_SI<00> => NOT REPLACE
_GR_ST_DI<00> => NOT REPLACE
_GR_MA_SI<00> => NOT REPLACE
_GR_MA_DI<00> => NOT REPLACE
_GR_MT_SI<00> => NOT REPLACE
_GR_MT_DI<00> => NOT REPLACE
*/
static enum _R_ACTION replace_group_owned_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (R_IS_GROUP(r1) && R_IS_GROUP(r2)) {
		if (!R_IS_ACTIVE(r1) || R_IS_ACTIVE(r2)) {
			/* REPLACE */
			return R_DO_REPLACE;
		}
	}

	/* NOT REPLACE, but PROPAGATE */
	return R_DO_PROPAGATE;
}

/*
active (not sgroup vs. sgroup yet!):
_SA_UA_SI_U<1c> => NOT REPLACE
_SA_UA_DI_U<1c> => NOT REPLACE
_SA_UT_SI_U<1c> => NOT REPLACE
_SA_UT_DI_U<1c> => NOT REPLACE
_SA_GA_SI_U<1c> => NOT REPLACE
_SA_GA_DI_U<1c> => NOT REPLACE
_SA_GT_SI_U<1c> => NOT REPLACE
_SA_GT_DI_U<1c> => NOT REPLACE
_SA_MA_SI_U<1c> => NOT REPLACE
_SA_MA_DI_U<1c> => NOT REPLACE
_SA_MT_SI_U<1c> => NOT REPLACE
_SA_MT_DI_U<1c> => NOT REPLACE

Test Replica vs. owned active: SGROUP vs. SGROUP tests
_SA_SA_DI_U<1c> => SGROUP_MERGE
_SA_SA_SI_U<1c> => SGROUP_MERGE
_SA_SA_SP_U<1c> => SGROUP_MERGE
_SA_SA_SB_U<1c> => SGROUP_MERGE
_SA_ST_DI_U<1c> => NOT REPLACE
_SA_ST_SI_U<1c> => NOT REPLACE
_SA_ST_SP_U<1c> => NOT REPLACE
_SA_ST_SB_U<1c> => NOT REPLACE

SGROUP,ACTIVE vs. SGROUP,* is not handled here!

released:
_SR_UA_SI<1c> => REPLACE
_SR_UA_DI<1c> => REPLACE
_SR_UT_SI<1c> => REPLACE
_SR_UT_DI<1c> => REPLACE
_SR_GA_SI<1c> => REPLACE
_SR_GA_DI<1c> => REPLACE
_SR_GT_SI<1c> => REPLACE
_SR_GT_DI<1c> => REPLACE
_SR_SA_SI<1c> => REPLACE
_SR_SA_DI<1c> => REPLACE
_SR_ST_SI<1c> => REPLACE
_SR_ST_DI<1c> => REPLACE
_SR_MA_SI<1c> => REPLACE
_SR_MA_DI<1c> => REPLACE
_SR_MT_SI<1c> => REPLACE
_SR_MT_DI<1c> => REPLACE
*/
static enum _R_ACTION replace_sgroup_owned_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (!R_IS_ACTIVE(r1)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (!R_IS_SGROUP(r2) || !R_IS_ACTIVE(r2)) {
		/* NOT REPLACE, but PROPAGATE */
		return R_DO_PROPAGATE;
	}

	if (r_1_is_same_as_2_address_list(r1, r2, true)) {
		/*
		 * as we're the old owner and the addresses and their
		 * owners are identical
		 */
		return R_NOT_REPLACE;
	}

	/* not handled here: MERGE */
	return R_DO_SGROUP_MERGE;
}

/*
active:
_MA_UA_SI_U<00> => REPLACE
_MA_UA_DI_P<00> => NOT REPLACE
_MA_UA_DI_O<00> => NOT REPLACE
_MA_UA_DI_N<00> => REPLACE
_MA_UT_SI_U<00> => NOT REPLACE
_MA_UT_DI_U<00> => NOT REPLACE
_MA_GA_SI_R<00> => REPLACE
_MA_GA_DI_R<00> => REPLACE
_MA_GT_SI_U<00> => NOT REPLACE
_MA_GT_DI_U<00> => NOT REPLACE
_MA_SA_SI_R<00> => REPLACE
_MA_SA_DI_R<00> => REPLACE
_MA_ST_SI_U<00> => NOT REPLACE
_MA_ST_DI_U<00> => NOT REPLACE
_MA_MA_SI_U<00> => REPLACE
_MA_MA_SP_U<00> => REPLACE
_MA_MA_DI_P<00> => NOT REPLACE
_MA_MA_DI_O<00> => NOT REPLACE
_MA_MA_DI_N<00> => REPLACE
_MA_MT_SI_U<00> => NOT REPLACE
_MA_MT_DI_U<00> => NOT REPLACE
Test Replica vs. owned active: some more MHOMED combinations
_MA_MA_SP_U<00> => REPLACE
_MA_MA_SM_U<00> => REPLACE
_MA_MA_SB_P<00> => MHOMED_MERGE
_MA_MA_SB_A<00> => MHOMED_MERGE
_MA_MA_SB_PRA<00> => NOT REPLACE
_MA_MA_SB_O<00> => NOT REPLACE
_MA_MA_SB_N<00> => REPLACE
Test Replica vs. owned active: some more UNIQUE,MHOMED combinations
_MA_UA_SB_P<00> => MHOMED_MERGE

released:
_MR_UA_SI<00> => REPLACE
_MR_UA_DI<00> => REPLACE
_MR_UT_SI<00> => REPLACE
_MR_UT_DI<00> => REPLACE
_MR_GA_SI<00> => REPLACE
_MR_GA_DI<00> => REPLACE
_MR_GT_SI<00> => REPLACE
_MR_GT_DI<00> => REPLACE
_MR_SA_SI<00> => REPLACE
_MR_SA_DI<00> => REPLACE
_MR_ST_SI<00> => REPLACE
_MR_ST_DI<00> => REPLACE
_MR_MA_SI<00> => REPLACE
_MR_MA_DI<00> => REPLACE
_MR_MT_SI<00> => REPLACE
_MR_MT_DI<00> => REPLACE
*/
static enum _R_ACTION replace_mhomed_owned_vs_X_replica(struct winsdb_record *r1, struct wrepl_name *r2)
{
	if (!R_IS_ACTIVE(r1)) {
		/* REPLACE */
		return R_DO_REPLACE;
	}

	if (!R_IS_ACTIVE(r2)) {
		/* NOT REPLACE, but PROPAGATE */
		return R_DO_PROPAGATE;
	}

	if (R_IS_GROUP(r2) || R_IS_SGROUP(r2)) {
		/* REPLACE and send a release demand to the old name owner */
		return R_DO_RELEASE_DEMAND;
	}

	/* 
	 * here we only have mhomed,active,owned vs.
	 * is unique,active,replica or mhomed,active,replica
	 */

	if (r_1_is_subset_of_2_address_list(r1, r2, false)) {
		/* 
		 * if r1 has a subset(or same) of the addresses of r2
		 * <=>
		 * if r2 has a superset(or same) of the addresses of r1
		 *
		 * then replace the record
		 */
		return R_DO_REPLACE;
	}

	/*
	 * in any other case, we need to do
	 * a name request to the old name holder
	 * to see if it's still there...
	 */
	return R_DO_CHALLENGE;
}

static NTSTATUS r_do_add(struct wreplsrv_partner *partner,
			 TALLOC_CTX *mem_ctx,
			 struct wrepl_wins_owner *owner,
			 struct wrepl_name *replica)
{
	struct winsdb_record *rec;
	uint32_t i;
	uint8_t ret;

	rec = talloc(mem_ctx, struct winsdb_record);
	NT_STATUS_HAVE_NO_MEMORY(rec);

	rec->name	= &replica->name;
	rec->type	= replica->type;
	rec->state	= replica->state;
	rec->node	= replica->node;
	rec->is_static	= replica->is_static;
	rec->expire_time= time(NULL) + partner->service->config.verify_interval;
	rec->version	= replica->version_id;
	rec->wins_owner	= replica->owner;
	rec->addresses	= winsdb_addr_list_make(rec);
	NT_STATUS_HAVE_NO_MEMORY(rec->addresses);
	rec->registered_by = NULL;

	for (i=0; i < replica->num_addresses; i++) {
		/* TODO: find out if rec->expire_time is correct here */
		rec->addresses = winsdb_addr_list_add(partner->service->wins_db,
						      rec, rec->addresses,
						      replica->addresses[i].address,
						      replica->addresses[i].owner,
						      rec->expire_time,
						      false);
		NT_STATUS_HAVE_NO_MEMORY(rec->addresses);
	}

	ret = winsdb_add(partner->service->wins_db, rec, 0);
	if (ret != NBT_RCODE_OK) {
		DEBUG(0,("Failed to add record %s: %u\n",
			nbt_name_string(mem_ctx, &replica->name), ret));
		return NT_STATUS_FOOBAR;
	}

	DEBUG(4,("added record %s\n",
		nbt_name_string(mem_ctx, &replica->name)));

	return NT_STATUS_OK;
}

static NTSTATUS r_do_replace(struct wreplsrv_partner *partner,
			     TALLOC_CTX *mem_ctx,
			     struct winsdb_record *rec,
			     struct wrepl_wins_owner *owner,
			     struct wrepl_name *replica)
{
	uint32_t i;
	uint8_t ret;

	rec->name	= &replica->name;
	rec->type	= replica->type;
	rec->state	= replica->state;
	rec->node	= replica->node;
	rec->is_static	= replica->is_static;
	rec->expire_time= time(NULL) + partner->service->config.verify_interval;
	rec->version	= replica->version_id;
	rec->wins_owner	= replica->owner;
	rec->addresses	= winsdb_addr_list_make(rec);
	NT_STATUS_HAVE_NO_MEMORY(rec->addresses);
	rec->registered_by = NULL;

	for (i=0; i < replica->num_addresses; i++) {
		/* TODO: find out if rec->expire_time is correct here */
		rec->addresses = winsdb_addr_list_add(partner->service->wins_db,
						      rec, rec->addresses,
						      replica->addresses[i].address,
						      replica->addresses[i].owner,
						      rec->expire_time,
						      false);
		NT_STATUS_HAVE_NO_MEMORY(rec->addresses);
	}

	ret = winsdb_modify(partner->service->wins_db, rec, 0);
	if (ret != NBT_RCODE_OK) {
		DEBUG(0,("Failed to replace record %s: %u\n",
			nbt_name_string(mem_ctx, &replica->name), ret));
		return NT_STATUS_FOOBAR;
	}

	DEBUG(4,("replaced record %s\n",
		nbt_name_string(mem_ctx, &replica->name)));

	return NT_STATUS_OK;
}

static NTSTATUS r_not_replace(struct wreplsrv_partner *partner,
			      TALLOC_CTX *mem_ctx,
			      struct winsdb_record *rec,
			      struct wrepl_wins_owner *owner,
			      struct wrepl_name *replica)
{
	DEBUG(4,("not replace record %s\n",
		 nbt_name_string(mem_ctx, &replica->name)));
	return NT_STATUS_OK;
}

static NTSTATUS r_do_propagate(struct wreplsrv_partner *partner,
			       TALLOC_CTX *mem_ctx,
			       struct winsdb_record *rec,
			       struct wrepl_wins_owner *owner,
			       struct wrepl_name *replica)
{
	uint8_t ret;
	uint32_t modify_flags;

	/*
	 * allocate a new version id for the record to that it'll be replicated
	 */
	modify_flags	= WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;

	ret = winsdb_modify(partner->service->wins_db, rec, modify_flags);
	if (ret != NBT_RCODE_OK) {
		DEBUG(0,("Failed to replace record %s: %u\n",
			nbt_name_string(mem_ctx, &replica->name), ret));
		return NT_STATUS_FOOBAR;
	}

	DEBUG(4,("propagated record %s\n",
		 nbt_name_string(mem_ctx, &replica->name)));

	return NT_STATUS_OK;
}

/* 
Test Replica vs. owned active: some more MHOMED combinations
_MA_MA_SP_U<00>: C:MHOMED vs. B:ALL => B:ALL => REPLACE
_MA_MA_SM_U<00>: C:MHOMED vs. B:MHOMED => B:MHOMED => REPLACE
_MA_MA_SB_P<00>: C:MHOMED vs. B:BEST (C:MHOMED) => B:MHOMED => MHOMED_MERGE
_MA_MA_SB_A<00>: C:MHOMED vs. B:BEST (C:ALL) => B:MHOMED => MHOMED_MERGE
_MA_MA_SB_PRA<00>: C:MHOMED vs. B:BEST (C:BEST) => C:MHOMED => NOT REPLACE
_MA_MA_SB_O<00>: C:MHOMED vs. B:BEST (B:B_3_4) =>C:MHOMED => NOT REPLACE
_MA_MA_SB_N<00>: C:MHOMED vs. B:BEST (NEGATIVE) => B:BEST => REPLACE
Test Replica vs. owned active: some more UNIQUE,MHOMED combinations
_MA_UA_SB_P<00>: C:MHOMED vs. B:UNIQUE,BEST (C:MHOMED) => B:MHOMED => MHOMED_MERGE
_UA_UA_DI_PRA<00>: C:BEST vs. B:BEST2 (C:BEST2,LR:BEST2) => C:BEST => NOT REPLACE
_UA_UA_DI_A<00>: C:BEST vs. B:BEST2 (C:ALL) => B:MHOMED => MHOMED_MERGE
_UA_MA_DI_A<00>: C:BEST vs. B:BEST2 (C:ALL) => B:MHOMED => MHOMED_MERGE
*/
static NTSTATUS r_do_mhomed_merge(struct wreplsrv_partner *partner,
				  TALLOC_CTX *mem_ctx,
				  struct winsdb_record *rec,
				  struct wrepl_wins_owner *owner,
				  struct wrepl_name *replica)
{
	struct winsdb_record *merge;
	uint32_t i,j;
	uint8_t ret;
	size_t len;

	merge = talloc(mem_ctx, struct winsdb_record);
	NT_STATUS_HAVE_NO_MEMORY(merge);

	merge->name		= &replica->name;
	merge->type		= WREPL_TYPE_MHOMED;
	merge->state		= replica->state;
	merge->node		= replica->node;
	merge->is_static	= replica->is_static;
	merge->expire_time	= time(NULL) + partner->service->config.verify_interval;
	merge->version		= replica->version_id;
	merge->wins_owner	= replica->owner;
	merge->addresses	= winsdb_addr_list_make(merge);
	NT_STATUS_HAVE_NO_MEMORY(merge->addresses);
	merge->registered_by = NULL;

	for (i=0; i < replica->num_addresses; i++) {
		merge->addresses = winsdb_addr_list_add(partner->service->wins_db,
							merge, merge->addresses,
							replica->addresses[i].address,
							replica->addresses[i].owner,
							merge->expire_time,
							false);
		NT_STATUS_HAVE_NO_MEMORY(merge->addresses);
	}

	len = winsdb_addr_list_length(rec->addresses);

	for (i=0; i < len; i++) {
		bool found = false;
		for (j=0; j < replica->num_addresses; j++) {
			if (strcmp(replica->addresses[j].address, rec->addresses[i]->address) == 0) {
				found = true;
				break;
			}
		}
		if (found) continue;

		merge->addresses = winsdb_addr_list_add(partner->service->wins_db,
							merge, merge->addresses,
							rec->addresses[i]->address,
							rec->addresses[i]->wins_owner,
							rec->addresses[i]->expire_time,
							false);
		NT_STATUS_HAVE_NO_MEMORY(merge->addresses);
	}

	ret = winsdb_modify(partner->service->wins_db, merge, 0);
	if (ret != NBT_RCODE_OK) {
		DEBUG(0,("Failed to modify mhomed merge record %s: %u\n",
			nbt_name_string(mem_ctx, &replica->name), ret));
		return NT_STATUS_FOOBAR;
	}

	DEBUG(4,("mhomed merge record %s\n",
		nbt_name_string(mem_ctx, &replica->name)));

	return NT_STATUS_OK;
}

struct r_do_challenge_state {
	struct messaging_context *msg_ctx;
	struct wreplsrv_partner *partner;
	struct winsdb_record *rec;
	struct wrepl_wins_owner owner;
	struct wrepl_name replica;
	struct nbtd_proxy_wins_challenge r;
};

static void r_do_late_release_demand_handler(struct irpc_request *ireq)
{
	NTSTATUS status;
	struct r_do_challenge_state *state = talloc_get_type(ireq->async.private_data,
							     struct r_do_challenge_state);

	status = irpc_call_recv(ireq);
	/* don't care about the result */
	talloc_free(state);
}

static NTSTATUS r_do_late_release_demand(struct r_do_challenge_state *state)
{
	struct irpc_request *ireq;
	struct server_id *nbt_servers;
	struct nbtd_proxy_wins_release_demand r;
	uint32_t i;

	DEBUG(4,("late release demand record %s\n",
		 nbt_name_string(state, &state->replica.name)));

	nbt_servers = irpc_servers_byname(state->msg_ctx, state, "nbt_server");
	if ((nbt_servers == NULL) || (nbt_servers[0].id == 0)) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	r.in.name	= state->replica.name;
	r.in.num_addrs	= state->r.out.num_addrs;
	r.in.addrs	= talloc_array(state, struct nbtd_proxy_wins_addr, r.in.num_addrs);
	NT_STATUS_HAVE_NO_MEMORY(r.in.addrs);
	/* TODO: fix pidl to handle inline ipv4address arrays */
	for (i=0; i < r.in.num_addrs; i++) {
		r.in.addrs[i].addr = state->r.out.addrs[i].addr;
	}

	ireq = IRPC_CALL_SEND(state->msg_ctx, nbt_servers[0],
			      irpc, NBTD_PROXY_WINS_RELEASE_DEMAND,
			      &r, state);
	NT_STATUS_HAVE_NO_MEMORY(ireq);

	ireq->async.fn		= r_do_late_release_demand_handler;
	ireq->async.private_data= state;

	return NT_STATUS_OK;
}

/* 
Test Replica vs. owned active: some more MHOMED combinations
_MA_MA_SP_U<00>: C:MHOMED vs. B:ALL => B:ALL => REPLACE
_MA_MA_SM_U<00>: C:MHOMED vs. B:MHOMED => B:MHOMED => REPLACE
_MA_MA_SB_P<00>: C:MHOMED vs. B:BEST (C:MHOMED) => B:MHOMED => MHOMED_MERGE
_MA_MA_SB_A<00>: C:MHOMED vs. B:BEST (C:ALL) => B:MHOMED => MHOMED_MERGE
_MA_MA_SB_PRA<00>: C:MHOMED vs. B:BEST (C:BEST) => C:MHOMED => NOT REPLACE
_MA_MA_SB_O<00>: C:MHOMED vs. B:BEST (B:B_3_4) =>C:MHOMED => NOT REPLACE
_MA_MA_SB_N<00>: C:MHOMED vs. B:BEST (NEGATIVE) => B:BEST => REPLACE
Test Replica vs. owned active: some more UNIQUE,MHOMED combinations
_MA_UA_SB_P<00>: C:MHOMED vs. B:UNIQUE,BEST (C:MHOMED) => B:MHOMED => MHOMED_MERGE
_UA_UA_DI_PRA<00>: C:BEST vs. B:BEST2 (C:BEST2,LR:BEST2) => C:BEST => NOT REPLACE
_UA_UA_DI_A<00>: C:BEST vs. B:BEST2 (C:ALL) => B:MHOMED => MHOMED_MERGE
_UA_MA_DI_A<00>: C:BEST vs. B:BEST2 (C:ALL) => B:MHOMED => MHOMED_MERGE
*/
static void r_do_challenge_handler(struct irpc_request *ireq)
{
	NTSTATUS status;
	struct r_do_challenge_state *state = talloc_get_type(ireq->async.private_data,
							     struct r_do_challenge_state);
	bool old_is_subset = false;
	bool new_is_subset = false;
	bool found = false;
	uint32_t i,j;
	uint32_t num_rec_addrs;

	status = irpc_call_recv(ireq);

	DEBUG(4,("r_do_challenge_handler: %s: %s\n", 
		 nbt_name_string(state, &state->replica.name), nt_errstr(status)));

	if (NT_STATUS_EQUAL(NT_STATUS_IO_TIMEOUT, status) ||
	    NT_STATUS_EQUAL(NT_STATUS_OBJECT_NAME_NOT_FOUND, status)) {
		r_do_replace(state->partner, state, state->rec, &state->owner, &state->replica);
		talloc_free(state);
		return;
	}

	for (i=0; i < state->replica.num_addresses; i++) {
		found = false;
		new_is_subset = true;
		for (j=0; j < state->r.out.num_addrs; j++) {
			if (strcmp(state->replica.addresses[i].address, state->r.out.addrs[j].addr) == 0) {
				found = true;
				break;
			}
		}
		if (found) continue;

		new_is_subset = false;
		break;
	}

	if (!new_is_subset) {
		r_not_replace(state->partner, state, state->rec, &state->owner, &state->replica);
		talloc_free(state);
		return;
	}

	num_rec_addrs = winsdb_addr_list_length(state->rec->addresses);
	for (i=0; i < num_rec_addrs; i++) {
		found = false;
		old_is_subset = true;
		for (j=0; j < state->r.out.num_addrs; j++) {
			if (strcmp(state->rec->addresses[i]->address, state->r.out.addrs[j].addr) == 0) {
				found = true;
				break;
			}
		}
		if (found) continue;

		old_is_subset = false;
		break;
	}

	if (!old_is_subset) {
		status = r_do_late_release_demand(state);
		/* 
		 * only free state on error, because we pass it down,
		 * and r_do_late_release_demand() will free it
		 */
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(state);
		}
		return;
	}

	r_do_mhomed_merge(state->partner, state, state->rec, &state->owner, &state->replica);
	talloc_free(state);
}

static NTSTATUS r_do_challenge(struct wreplsrv_partner *partner,
			       TALLOC_CTX *mem_ctx,
			       struct winsdb_record *rec,
			       struct wrepl_wins_owner *owner,
			       struct wrepl_name *replica)
{
	struct irpc_request *ireq;
	struct r_do_challenge_state *state;
	struct server_id *nbt_servers;
	const char **addrs;
	uint32_t i;

	DEBUG(4,("challenge record %s\n",
		 nbt_name_string(mem_ctx, &replica->name)));

	state = talloc_zero(mem_ctx, struct r_do_challenge_state);
	NT_STATUS_HAVE_NO_MEMORY(state);
	state->msg_ctx	= partner->service->task->msg_ctx;
	state->partner	= partner;
	state->rec	= talloc_steal(state, rec);
	state->owner	= *owner;
	state->replica	= *replica;
	/* some stuff to have valid memory pointers in the async complete function */
	state->replica.name = *state->rec->name;
	talloc_steal(state, replica->owner);
	talloc_steal(state, replica->addresses);

	nbt_servers = irpc_servers_byname(state->msg_ctx, state, "nbt_server");
	if ((nbt_servers == NULL) || (nbt_servers[0].id == 0)) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	state->r.in.name	= *rec->name;
	state->r.in.num_addrs	= winsdb_addr_list_length(rec->addresses);
	state->r.in.addrs	= talloc_array(state, struct nbtd_proxy_wins_addr, state->r.in.num_addrs);
	NT_STATUS_HAVE_NO_MEMORY(state->r.in.addrs);
	/* TODO: fix pidl to handle inline ipv4address arrays */
	addrs			= winsdb_addr_string_list(state->r.in.addrs, rec->addresses);
	NT_STATUS_HAVE_NO_MEMORY(addrs);
	for (i=0; i < state->r.in.num_addrs; i++) {
		state->r.in.addrs[i].addr = addrs[i];
	}

	ireq = IRPC_CALL_SEND(state->msg_ctx, nbt_servers[0],
			      irpc, NBTD_PROXY_WINS_CHALLENGE,
			      &state->r, state);
	NT_STATUS_HAVE_NO_MEMORY(ireq);

	ireq->async.fn		= r_do_challenge_handler;
	ireq->async.private_data= state;

	talloc_steal(partner, state);
	return NT_STATUS_OK;
}

struct r_do_release_demand_state {
	struct messaging_context *msg_ctx;
	struct nbtd_proxy_wins_release_demand r;
};

static void r_do_release_demand_handler(struct irpc_request *ireq)
{
	NTSTATUS status;
	struct r_do_release_demand_state *state = talloc_get_type(ireq->async.private_data,
						  struct r_do_release_demand_state);

	status = irpc_call_recv(ireq);
	/* don't care about the result */
	talloc_free(state);
}

static NTSTATUS r_do_release_demand(struct wreplsrv_partner *partner,
				    TALLOC_CTX *mem_ctx,
				    struct winsdb_record *rec,
				    struct wrepl_wins_owner *owner,
				    struct wrepl_name *replica)
{
	NTSTATUS status;
	struct irpc_request *ireq;
	struct server_id *nbt_servers;
	const char **addrs;
	struct winsdb_addr **addresses;
	struct r_do_release_demand_state *state;
	uint32_t i;

	/*
	 * we need to get a reference to the old addresses,
	 * as we need to send a release demand to them after replacing the record
	 * and r_do_replace() will modify rec->addresses
	 */
	addresses = rec->addresses;

	status = r_do_replace(partner, mem_ctx, rec, owner, replica);
	NT_STATUS_NOT_OK_RETURN(status);

	DEBUG(4,("release demand record %s\n",
		 nbt_name_string(mem_ctx, &replica->name)));

	state = talloc_zero(mem_ctx, struct r_do_release_demand_state);
	NT_STATUS_HAVE_NO_MEMORY(state);
	state->msg_ctx	= partner->service->task->msg_ctx;

	nbt_servers = irpc_servers_byname(state->msg_ctx, state, "nbt_server");
	if ((nbt_servers == NULL) || (nbt_servers[0].id == 0)) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	state->r.in.name	= *rec->name;
	state->r.in.num_addrs	= winsdb_addr_list_length(addresses);
	state->r.in.addrs	= talloc_array(state, struct nbtd_proxy_wins_addr,
					       state->r.in.num_addrs);
	NT_STATUS_HAVE_NO_MEMORY(state->r.in.addrs);
	/* TODO: fix pidl to handle inline ipv4address arrays */
	addrs			= winsdb_addr_string_list(state->r.in.addrs, addresses);
	NT_STATUS_HAVE_NO_MEMORY(addrs);
	for (i=0; i < state->r.in.num_addrs; i++) {
		state->r.in.addrs[i].addr = addrs[i];
	}

	ireq = IRPC_CALL_SEND(state->msg_ctx, nbt_servers[0],
			      irpc, NBTD_PROXY_WINS_RELEASE_DEMAND,
			      &state->r, state);
	NT_STATUS_HAVE_NO_MEMORY(ireq);

	ireq->async.fn		= r_do_release_demand_handler;
	ireq->async.private_data= state;

	talloc_steal(partner, state);
	return NT_STATUS_OK;
}

/*
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:A_3_4 => NOT REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:NULL => NOT REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4_X_3_4 vs. B:A_3_4 => NOT REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4 vs. B:A_3_4 => REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:A_3_4_OWNER_B => REPLACE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4_OWNER_B vs. B:A_3_4 => REPLACE

SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4 vs. B:B_3_4 => C:A_3_4_B_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4_X_3_4 vs. B:A_3_4 => B:A_3_4_X_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:X_3_4 vs. B:A_3_4 => C:A_3_4_X_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:A_3_4_X_3_4 vs. B:A_3_4_OWNER_B => B:A_3_4_OWNER_B_X_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4_X_3_4 vs. B:B_3_4_X_1_2 => C:B_3_4_X_1_2_3_4 => SGROUP_MERGE
SGROUP,ACTIVE vs. SGROUP,ACTIVE A:B_3_4_X_3_4 vs. B:NULL => B:X_3_4 => SGROUP_MERGE

Test Replica vs. owned active: SGROUP vs. SGROUP tests
_SA_SA_DI_U<1c> => SGROUP_MERGE
_SA_SA_SI_U<1c> => SGROUP_MERGE
_SA_SA_SP_U<1c> => SGROUP_MERGE
_SA_SA_SB_U<1c> => SGROUP_MERGE
*/
static NTSTATUS r_do_sgroup_merge(struct wreplsrv_partner *partner,
				  TALLOC_CTX *mem_ctx,
				  struct winsdb_record *rec,
				  struct wrepl_wins_owner *owner,
				  struct wrepl_name *replica)
{
	struct winsdb_record *merge;
	uint32_t modify_flags = 0;
	uint32_t i,j;
	uint8_t ret;
	size_t len;
	bool changed_old_addrs = false;
	bool skip_replica_owned_by_us = false;
	bool become_owner = true;
	bool propagate = lp_parm_bool(partner->service->task->lp_ctx, NULL, "wreplsrv", "propagate name releases", false);
	const char *local_owner = partner->service->wins_db->local_owner;

	merge = talloc(mem_ctx, struct winsdb_record);
	NT_STATUS_HAVE_NO_MEMORY(merge);

	merge->name		= &replica->name;
	merge->type		= replica->type;
	merge->state		= replica->state;
	merge->node		= replica->node;
	merge->is_static	= replica->is_static;
	merge->expire_time	= time(NULL) + partner->service->config.verify_interval;
	merge->version		= replica->version_id;
	merge->wins_owner	= replica->owner;
	merge->addresses	= winsdb_addr_list_make(merge);
	NT_STATUS_HAVE_NO_MEMORY(merge->addresses);
	merge->registered_by = NULL;

	len = winsdb_addr_list_length(rec->addresses);

	for (i=0; i < len; i++) {
		bool found = false;

		for (j=0; j < replica->num_addresses; j++) {
			if (strcmp(rec->addresses[i]->address, replica->addresses[j].address) != 0) {
				continue;
			}

			found = true;

			if (strcmp(rec->addresses[i]->wins_owner, replica->addresses[j].owner) != 0) {
				changed_old_addrs = true;
				break;
			}
			break;
		}

		/* 
		 * if the address isn't in the replica and is owned by replicas owner,
		 * it won't be added to the merged record
		 */
		if (!found && strcmp(rec->addresses[i]->wins_owner, owner->address) == 0) {
			changed_old_addrs = true;
			continue;
		}

		/*
		 * add the address to the merge result, with the old owner and expire_time,
		 * the owner and expire_time will be overwritten later if the address is
		 * in the replica too
		 */
		merge->addresses = winsdb_addr_list_add(partner->service->wins_db,
							merge, merge->addresses,
							rec->addresses[i]->address,
							rec->addresses[i]->wins_owner,
							rec->addresses[i]->expire_time,
							false);
		NT_STATUS_HAVE_NO_MEMORY(merge->addresses);
	}

	for (i=0; i < replica->num_addresses; i++) {
		if (propagate &&
		    strcmp(replica->addresses[i].owner, local_owner) == 0) {
			const struct winsdb_addr *a;

			/*
			 * NOTE: this is different to the windows behavior
			 *       and off by default, but it better propagated
			 *       name releases
			 */
			a = winsdb_addr_list_check(merge->addresses,
						   replica->addresses[i].address);
			if (!a) {
				/* don't add addresses owned by us */
				skip_replica_owned_by_us = true;
			}
			continue;
		}
		merge->addresses = winsdb_addr_list_add(partner->service->wins_db,
							merge, merge->addresses,
							replica->addresses[i].address,
							replica->addresses[i].owner,
							merge->expire_time,
							false);
		NT_STATUS_HAVE_NO_MEMORY(merge->addresses);
	}

	/* we the old addresses change changed we don't become the owner */
	if (changed_old_addrs) {
		become_owner = false;
	}

	/*
	 * when we notice another server believes an address
	 * is owned by us and that's not the case
	 * we propagate the result
	 */
	if (skip_replica_owned_by_us) {
		become_owner = true;
	}

	/* if we're the owner of the old record, we'll be the owner of the new one too */
	if (strcmp(rec->wins_owner, local_owner)==0) {
		become_owner = true;
	}

	/*
	 * if the result has no addresses we take the ownership
	 */
	len = winsdb_addr_list_length(merge->addresses);
	if (len == 0) {
		become_owner = true;
	}

	/* 
	 * if addresses of the old record will be changed the replica owner
	 * will be owner of the merge result, otherwise we take the ownership
	 */
	if (become_owner) {
		time_t lh = 0;

		modify_flags = WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;

		/*
		 * if we're the owner, the expire time becomes the highest
		 * expire time of owned addresses
		 */
		len = winsdb_addr_list_length(merge->addresses);

		for (i=0; i < len; i++) {
			if (strcmp(merge->addresses[i]->wins_owner, local_owner)==0) {
				lh = MAX(lh, merge->addresses[i]->expire_time);
			}
		}

		if (lh != 0) {
			merge->expire_time = lh;
		}
	}

	ret = winsdb_modify(partner->service->wins_db, merge, modify_flags);
	if (ret != NBT_RCODE_OK) {
		DEBUG(0,("Failed to modify sgroup merge record %s: %u\n",
			nbt_name_string(mem_ctx, &replica->name), ret));
		return NT_STATUS_FOOBAR;
	}

	DEBUG(4,("sgroup merge record %s\n",
		nbt_name_string(mem_ctx, &replica->name)));

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_apply_one_record(struct wreplsrv_partner *partner,
					  TALLOC_CTX *mem_ctx,
					  struct wrepl_wins_owner *owner,
					  struct wrepl_name *replica)
{
	NTSTATUS status;
	struct winsdb_record *rec = NULL;
	enum _R_ACTION action = R_INVALID;
	bool same_owner = false;
	bool replica_vs_replica = false;
	bool local_vs_replica = false;

	status = winsdb_lookup(partner->service->wins_db,
			       &replica->name, mem_ctx, &rec);
	if (NT_STATUS_EQUAL(NT_STATUS_OBJECT_NAME_NOT_FOUND, status)) {
		return r_do_add(partner, mem_ctx, owner, replica);
	}
	NT_STATUS_NOT_OK_RETURN(status);

	if (strcmp(rec->wins_owner, partner->service->wins_db->local_owner)==0) {
		local_vs_replica = true;
	} else if (strcmp(rec->wins_owner, owner->address)==0) {
		same_owner = true;
	} else {
		replica_vs_replica = true;
	}

	if (rec->is_static && !same_owner) {
		action = R_NOT_REPLACE;

		/*
		 * if we own the local record, then propagate it back to
		 * the other wins servers.
		 * to prevent ping-pong with other servers, we don't do this
		 * if the replica is static too.
		 *
		 * It seems that w2k3 doesn't do this, but I thing that's a bug
		 * and doing propagation helps to have consistent data on all servers
		 */
		if (local_vs_replica && !replica->is_static) {
			action = R_DO_PROPAGATE;
		}
	} else if (replica->is_static && !rec->is_static && !same_owner) {
		action = R_DO_REPLACE;
	} else if (same_owner) {
		action = replace_same_owner(rec, replica);
	} else if (replica_vs_replica) {
		switch (rec->type) {
		case WREPL_TYPE_UNIQUE:
			action = replace_unique_replica_vs_X_replica(rec, replica);
			break;
		case WREPL_TYPE_GROUP:
			action = replace_group_replica_vs_X_replica(rec, replica);
			break;
		case WREPL_TYPE_SGROUP:
			action = replace_sgroup_replica_vs_X_replica(rec, replica);
			break;
		case WREPL_TYPE_MHOMED:
			action = replace_mhomed_replica_vs_X_replica(rec, replica);
			break;
		}
	} else if (local_vs_replica) {
		switch (rec->type) {
		case WREPL_TYPE_UNIQUE:
			action = replace_unique_owned_vs_X_replica(rec, replica);
			break;
		case WREPL_TYPE_GROUP:
			action = replace_group_owned_vs_X_replica(rec, replica);
			break;
		case WREPL_TYPE_SGROUP:
			action = replace_sgroup_owned_vs_X_replica(rec, replica);
			break;
		case WREPL_TYPE_MHOMED:
			action = replace_mhomed_owned_vs_X_replica(rec, replica);
			break;
		}
	}

	DEBUG(4,("apply record %s: %s\n",
		 nbt_name_string(mem_ctx, &replica->name), _R_ACTION_enum_string(action)));

	switch (action) {
	case R_INVALID: break;
	case R_DO_REPLACE:
		return r_do_replace(partner, mem_ctx, rec, owner, replica);
	case R_NOT_REPLACE:
		return r_not_replace(partner, mem_ctx, rec, owner, replica);
	case R_DO_PROPAGATE:
		return r_do_propagate(partner, mem_ctx, rec, owner, replica);
	case R_DO_CHALLENGE:
		return r_do_challenge(partner, mem_ctx, rec, owner, replica);
	case R_DO_RELEASE_DEMAND:
		return r_do_release_demand(partner, mem_ctx, rec, owner, replica);
	case R_DO_SGROUP_MERGE:
		return r_do_sgroup_merge(partner, mem_ctx, rec, owner, replica);
	}

	return NT_STATUS_INTERNAL_ERROR;
}

NTSTATUS wreplsrv_apply_records(struct wreplsrv_partner *partner,
				struct wrepl_wins_owner *owner,
				uint32_t num_names, struct wrepl_name *names)
{
	NTSTATUS status;
	uint32_t i;

	DEBUG(4,("apply records count[%u]:owner[%s]:min[%llu]:max[%llu]:partner[%s]\n",
		num_names, owner->address,
		(long long)owner->min_version, 
		(long long)owner->max_version,
		partner->address));

	for (i=0; i < num_names; i++) {
		TALLOC_CTX *tmp_mem = talloc_new(partner);
		NT_STATUS_HAVE_NO_MEMORY(tmp_mem);

		status = wreplsrv_apply_one_record(partner, tmp_mem,
						   owner, &names[i]);
		talloc_free(tmp_mem);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	status = wreplsrv_add_table(partner->service,
				    partner->service,
				    &partner->service->table,
				    owner->address,
				    owner->max_version);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

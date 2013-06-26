/*
 * idmap_centeris: Support for Local IDs and Centeris Cell Structure
 *
 * Copyright (C) Gerald (Jerry) Carter 2006-2008
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _IDMAP_ADEX_H
#define _IDMAP_ADEX_H

#include "winbindd/winbindd.h"

#define ADEX_CELL_RDN             "$LikewiseIdentityCell"

#define ADEX_OC_USER              "centerisLikewiseUser"
#define ADEX_OC_GROUP             "centerisLikewiseGroup"

#define AD_USER                 "User"
#define AD_GROUP                "Group"

#define ADEX_OC_POSIX_USER        "posixAccount"
#define ADEX_OC_POSIX_GROUP       "posixGroup"

#define ADEX_ATTR_UIDNUM          "uidNumber"
#define ADEX_ATTR_GIDNUM          "gidNUmber"
#define ADEX_ATTR_HOMEDIR         "unixHomeDirectory"
#define ADEX_ATTR_USERPW          "unixUserPassword"
#define ADEX_ATTR_GROUPALIAS      "groupAlias"	/* Not part of RFC2307 */
#define ADEX_ATTR_SHELL           "loginShell"
#define ADEX_ATTR_GECOS           "gecos"
#define ADEX_ATTR_UID             "uid"
#define ADEX_ATTR_DISPLAYNAME     "displayName"

#define MIN_ID_VALUE            100

#define BAIL_ON_NTSTATUS_ERROR(x)	   \
	do {				   \
		if (!NT_STATUS_IS_OK(x)) { \
			DEBUG(10,("Failed! (%s)\n", nt_errstr(x)));	\
			goto done;	   \
		}			   \
	}				   \
	while (0);			   \

#define WARN_ON_NTSTATUS_ERROR(x)	   \
	do {				   \
		if (!NT_STATUS_IS_OK(x)) { \
			DEBUG(10,("Failure ignored! (%s)\n", nt_errstr(x)));	\
		}			   \
	}				   \
	while (0);			   \

#define BAIL_ON_ADS_ERROR(x)		   \
	do {				   \
		if (!ADS_ERR_OK(x)) {	   \
			goto done;	   \
		}			   \
	}				   \
	while (0);

#define BAIL_ON_PTR_ERROR(p, x)				\
	do {						\
		if ((p) == NULL ) {			\
			DEBUG(10,("NULL pointer!\n"));	\
			x = NT_STATUS_NO_MEMORY;	\
			goto done;			\
		}					\
	} while (0);

#define PRINT_NTSTATUS_ERROR(x, hdr, level)				\
	do {								\
		if (!NT_STATUS_IS_OK(x)) {				\
			DEBUG(level,("LWI ("hdr"): %s\n", nt_errstr(x))); \
		}							\
	} while(0);
/*
 * Cell Provider API
 */

struct cell_provider_api {
	NTSTATUS(*get_sid_from_id) (struct dom_sid * sid,
				    uint32_t id, enum id_type type);
	NTSTATUS(*get_id_from_sid) (uint32_t * id,
				    enum id_type * type, const struct dom_sid * sid);
	NTSTATUS(*get_nss_info) (const struct dom_sid * sid,
				 TALLOC_CTX * ctx,
				 const char **homedir,
				 const char **shell,
				 const char **gecos, gid_t * p_gid);
	NTSTATUS(*map_to_alias) (TALLOC_CTX * mem_ctx,
				 const char *domain,
				 const char *name, char **alias);
	NTSTATUS(*map_from_alias) (TALLOC_CTX * mem_ctx,
				   const char *domain,
				   const char *alias, char **name);
};

/* registered providers */

extern struct cell_provider_api ccp_unified;
extern struct cell_provider_api ccp_local;

#define LWCELL_FLAG_USE_RFC2307_ATTRS     0x00000001
#define LWCELL_FLAG_SEARCH_FOREST         0x00000002
#define LWCELL_FLAG_GC_CELL               0x00000004
#define LWCELL_FLAG_LOCAL_MODE            0x00000008

struct likewise_cell {
	struct likewise_cell *prev, *next;
	ADS_STRUCT *conn;
	struct likewise_cell *gc_search_cell;
	struct dom_sid domain_sid;
	char *dns_domain;
	char *forest_name;
	char *dn;
	struct GUID *links;        /* only held by owning cell */
	size_t num_links;
	uint32_t flags;
	struct cell_provider_api *provider;
};

/* Search flags used for Global Catalog API */

#define ADEX_GC_SEARCH_CHECK_UNIQUE        0x00000001

struct gc_info {
	struct gc_info *prev, *next;
	char *forest_name;
	char *search_base;
	struct likewise_cell *forest_cell;
};

/* Available functions outside of idmap_lwidentity.c */

/* cell_util.c */

char *find_attr_string(char **list, size_t num_lines, const char *substr);
bool is_object_class(char **list, size_t num_lines, const char *substr);
int min_id_value(void);
char *cell_dn_to_dns(const char *dn);
NTSTATUS get_sid_type(ADS_STRUCT *ads,
		      LDAPMessage *msg,
		      enum lsa_SidType *type);

NTSTATUS cell_locate_membership(ADS_STRUCT * ads);
NTSTATUS cell_lookup_settings(struct likewise_cell * cell);

/* likewise_cell.c */

struct likewise_cell *cell_new(void);
struct likewise_cell *cell_list_head(void);

bool cell_list_add(struct likewise_cell *cell);
bool cell_list_remove(struct likewise_cell * cell);

void cell_list_destroy(void);
void cell_destroy(struct likewise_cell *c);
void cell_set_dns_domain(struct likewise_cell *c,
			   const char *dns_domain);
void cell_set_connection(struct likewise_cell *c,
			   ADS_STRUCT *ads);
void cell_set_dn(struct likewise_cell *c,
		   const char *dn);
void cell_set_domain_sid(struct likewise_cell *c,
			   struct dom_sid *sid);
void cell_set_flags(struct likewise_cell *c, uint32_t flags);
void cell_clear_flags(struct likewise_cell *c, uint32_t flags);

const char* cell_search_base(struct likewise_cell *c);
const char *cell_dns_domain(struct likewise_cell *c);
ADS_STRUCT *cell_connection(struct likewise_cell *c);
bool cell_search_forest(struct likewise_cell *c);
ADS_STATUS cell_do_search(struct likewise_cell *c,
			  const char *search_base,
			  int scope,
			  const char *expr,
			  const char **attrs,
			  LDAPMessage ** msg);
uint32_t cell_flags(struct likewise_cell *c);

NTSTATUS cell_connect_dn(struct likewise_cell **c,
			 const char *dn);
NTSTATUS cell_connect(struct likewise_cell *c);


/* gc_util.c */

NTSTATUS gc_init_list(void);

NTSTATUS gc_find_forest_root(struct gc_info *gc,
			     const char *domain);

struct gc_info *gc_search_start(void);

NTSTATUS gc_search_forest(struct gc_info *gc,
			  LDAPMessage **msg,
			  const char *filter);

NTSTATUS gc_search_all_forests(const char *filter,
			       ADS_STRUCT ***ads_list,
			       LDAPMessage ***msg_list,
			       int *num_resp, uint32_t flags);

NTSTATUS gc_search_all_forests_unique(const char *filter,
				      ADS_STRUCT **ads,
				      LDAPMessage **msg);

NTSTATUS gc_name_to_sid(const char *domain,
			const char *name,
			struct dom_sid *sid,
			enum lsa_SidType *sid_type);

NTSTATUS gc_sid_to_name(const struct dom_sid *sid,
			char **name,
			enum lsa_SidType *sid_type);

NTSTATUS add_ads_result_to_array(ADS_STRUCT *ads,
				 LDAPMessage *msg,
				 ADS_STRUCT ***ads_list,
				 LDAPMessage ***msg_list,
				 int *size);

void free_result_array(ADS_STRUCT **ads_list,
		       LDAPMessage **msg_list,
		       int num_resp);

NTSTATUS check_result_unique(ADS_STRUCT *ads,
			     LDAPMessage *msg);


/* domain_util.c */

NTSTATUS domain_init_list(void);

NTSTATUS dc_search_domains(struct likewise_cell **cell,
			   LDAPMessage **msg,
			   const char *dn,
			   const struct dom_sid *user_sid);


#endif	/* _IDMAP_ADEX_H */

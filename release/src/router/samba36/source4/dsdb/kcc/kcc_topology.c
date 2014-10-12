/*
   Unix SMB/CIFS implementation.

   KCC service

   Copyright (C) Crístian Deives 2010

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
#include "dsdb/samdb/samdb.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "dsdb/kcc/kcc_service.h"

#define FLAG_CR_NTDS_NC 0x00000001
#define FLAG_CR_NTDS_DOMAIN 0x00000002

#define NTDSCONN_OPT_IS_GENERATED 0x00000001
#define NTDSCONN_OPT_TWOWAY_SYNC 0x00000002
#define NTDSCONN_OPT_OVERRIDE_NOTIFY_DEFAULT 0x00000004
#define NTDSCONN_OPT_USE_NOTIFY 0x00000008
#define NTDSCONN_OPT_DISABLE_INTERSITE_COMPRESSION 0x00000010
#define NTDSCONN_OPT_USER_OWNED_SCHEDULE 0x00000020
#define NTDSCONN_OPT_RODC_TOPOLOGY 0x00000040

#define NTDSDSA_OPT_IS_GC 0x00000001

#define NTDSSETTINGS_OPT_IS_TOPL_DETECT_STALE_DISABLED 0x00000008
#define NTDSSETTINGS_OPT_IS_RAND_BH_SELECTION_DISABLED 0x00000100
#define NTDSSETTINGS_OPT_W2K3_BRIDGES_REQUIRED 0x00001000

#define NTDSSITELINK_OPT_USE_NOTIFY 0x00000001
#define NTDSSITELINK_OPT_TWOWAY_SYNC 0x00000002
#define NTDSSITELINK_OPT_DISABLE_COMPRESSION 0x00000004

#define NTDSTRANSPORT_OPT_BRIDGES_REQUIRED 0x00000002

#define DS_BEHAVIOR_WIN2008 3

/** replication parameters of a graph edge */
struct kcctpl_repl_info {
	uint32_t cost;
	uint32_t interval;
	uint32_t options;
	uint8_t schedule[84];
};

/** color of a vertex */
enum kcctpl_color { RED, BLACK, WHITE };

/** a GUID array list */
struct GUID_list {
	struct GUID *data;
	uint32_t count;
};

/** a vertex in the site graph */
struct kcctpl_vertex {
	struct GUID id;
	struct GUID_list edge_ids;
	enum kcctpl_color color;
	struct GUID_list accept_red_red;
	struct GUID_list accept_black;
	struct kcctpl_repl_info repl_info;
	uint32_t dist_to_red;

	/* Dijkstra data */
	struct GUID root_id;
	bool demoted;

	/* Kruskal data */
	struct GUID component_id;
	uint32_t component_index;
};

/** fully connected subgraph of vertices */
struct kcctpl_multi_edge {
	struct GUID id;
	struct GUID_list vertex_ids;
	struct GUID type;
	struct kcctpl_repl_info repl_info;
	bool directed;
};

/** set of transitively connected kcc_multi_edge's. all edges within the set
 * have the same type. */
struct kcctpl_multi_edge_set {
	struct GUID id;
	struct GUID_list edge_ids;
};

/** a vertices array list */
struct kcctpl_vertex_list {
	struct kcctpl_vertex *data;
	uint32_t count;
};

/** an edges array list */
struct kcctpl_multi_edge_list {
	struct kcctpl_multi_edge *data;
	uint32_t count;
};

/** an edge sets array list */
struct kcctpl_multi_edge_set_list {
	struct kcctpl_multi_edge_set *data;
	uint32_t count;
};

/** a site graph */
struct kcctpl_graph {
	struct kcctpl_vertex_list vertices;
	struct kcctpl_multi_edge_list edges;
	struct kcctpl_multi_edge_set_list edge_sets;
};

/** path found in the graph between two non-white vertices */
struct kcctpl_internal_edge {
	struct GUID v1id;
	struct GUID v2id;
	bool red_red;
	struct kcctpl_repl_info repl_info;
	struct GUID type;
};

/** an internal edges array list */
struct kcctpl_internal_edge_list {
	struct kcctpl_internal_edge *data;
	uint32_t count;
};

/** an LDB messages array list */
struct message_list {
	struct ldb_message *data;
	uint32_t count;
};

/**
 * sort internal edges based on:
 * - descending red_red,
 * - ascending repl_info.cost,
 * - descending available time in repl_info.schedule,
 * - ascending v1id,
 * - ascending v2id,
 * - ascending type.
 *
 * this function is used in 'kcctpl_kruskal'.
 */
static int kcctpl_sort_internal_edges(const void *internal_edge1,
				      const void *internal_edge2)
{
	const struct kcctpl_internal_edge *ie1, *ie2;
	int cmp_red_red;

	ie1 = (const struct kcctpl_internal_edge *) internal_edge1;
	ie2 = (const struct kcctpl_internal_edge *) internal_edge2;

	cmp_red_red = ie2->red_red - ie1->red_red;
	if (cmp_red_red == 0) {
		int cmp_cost = ie1->repl_info.cost - ie2->repl_info.cost;

		if (cmp_cost == 0) {
			uint32_t available1, available2, i;
			int cmp_schedule;

			available1 = available2 = 0;
			for (i = 0; i < 84; i++) {
				if (ie1->repl_info.schedule[i] == 0) {
					available1++;
				}

				if (ie2->repl_info.schedule[i] == 0) {
					available2++;
				}
			}
			cmp_schedule = available2 - available1;

			if (cmp_schedule == 0) {
				int cmp_v1id = GUID_compare(&ie1->v1id,
							    &ie2->v1id);

				if (cmp_v1id == 0) {
					int cmp_v2id = GUID_compare(&ie1->v2id,
								    &ie2->v2id);

					if (cmp_v2id == 0) {
						return GUID_compare(&ie1->type,
								    &ie2->type);
					} else {
						return cmp_v2id;
					}
				} else {
					return cmp_v1id;
				}
			} else {
				return cmp_schedule;
			}
		} else {
			return cmp_cost;
		}
	} else {
		return cmp_red_red;
	}
}

/**
 * sort vertices based on the following criteria:
 * - ascending color (RED < BLACK),
 * - ascending repl_info.cost,
 * - ascending id.
 *
 * this function is used in 'kcctpl_process_edge'.
 */
static int kcctpl_sort_vertices(const void *vertex1, const void *vertex2)
{
	const struct kcctpl_vertex *v1, *v2;
	int cmp_color;

	v1 = (const struct kcctpl_vertex *) vertex1;
	v2 = (const struct kcctpl_vertex *) vertex2;

	cmp_color = v1->color - v2->color;
	if (cmp_color == 0) {
		int cmp_cost = v1->repl_info.cost - v2->repl_info.cost;
		if (cmp_cost == 0) {
			return GUID_compare(&v1->id, &v2->id);
		} else {
			return cmp_cost;
		}
	} else {
		return cmp_color;
	}
}

/**
 * sort bridgehead elements (nTDSDSA) based on the following criteria:
 * - GC servers precede non-GC servers
 * - ascending objectGUID
 *
 * this function is used in 'kcctpl_get_all_bridgehead_dcs'.
 */
static int kcctpl_sort_bridgeheads(const void *bridgehead1,
				   const void *bridgehead2)
{
	const struct ldb_message *bh1, *bh2;
	uint32_t bh1_opts, bh2_opts;
	int cmp_gc;

	bh1 = (const struct ldb_message *) bridgehead1;
	bh2 = (const struct ldb_message *) bridgehead2;

	bh1_opts = ldb_msg_find_attr_as_uint(bh1, "options", 0);
	bh2_opts = ldb_msg_find_attr_as_uint(bh2, "options", 0);

	cmp_gc = (bh1_opts & NTDSDSA_OPT_IS_GC) -
		 (bh2_opts & NTDSDSA_OPT_IS_GC);

	if (cmp_gc == 0) {
		struct GUID bh1_id, bh2_id;

		bh1_id = samdb_result_guid(bh1, "objectGUID");
		bh2_id = samdb_result_guid(bh2, "objectGUID");

		return GUID_compare(&bh1_id, &bh2_id);
	} else {
		return cmp_gc;
	}
}

/**
 * sort bridgehead elements (nTDSDSA) in a random order.
 *
 * this function is used in 'kcctpl_get_all_bridgehead_dcs'.
 */
static void kcctpl_shuffle_bridgeheads(struct message_list bridgeheads)
{
	uint32_t i;

	srandom(time(NULL));

	for (i = bridgeheads.count; i > 1; i--) {
		uint32_t r;
		struct ldb_message tmp;

		r = random() % i;

		tmp = bridgeheads.data[i - 1];
		bridgeheads.data[i - 1] = bridgeheads.data[r];
		bridgeheads.data[r] = tmp;
	}
}

/**
 * find a graph vertex based on its GUID.
 */
static struct kcctpl_vertex *kcctpl_find_vertex_by_guid(struct kcctpl_graph *graph,
							struct GUID guid)
{
	uint32_t i;

	for (i = 0; i < graph->vertices.count; i++) {
		if (GUID_equal(&graph->vertices.data[i].id, &guid)) {
			return &graph->vertices.data[i];
		}
	}

	return NULL;
}

/**
 * find a graph edge based on its GUID.
 */
static struct kcctpl_multi_edge *kcctpl_find_edge_by_guid(struct kcctpl_graph *graph,
							  struct GUID guid)
{
	uint32_t i;

	for (i = 0; i < graph->edges.count; i++) {
		if (GUID_equal(&graph->edges.data[i].id, &guid)) {
			return &graph->edges.data[i];
		}
	}

	return NULL;
}

/**
 * find a graph edge that contains a vertex with the specified GUID. the first
 * occurrence will be returned.
 */
static struct kcctpl_multi_edge *kcctpl_find_edge_by_vertex_guid(struct kcctpl_graph *graph,
								 struct GUID guid)
{
	uint32_t i;

	for (i = 0; i < graph->edges.count; i++) {
		struct kcctpl_multi_edge *edge;
		uint32_t j;

		edge = &graph->edges.data[i];

		for (j = 0; j < edge->vertex_ids.count; j++) {
			struct GUID vertex_guid = edge->vertex_ids.data[j];

			struct GUID *p = &guid;

			if (GUID_equal(&vertex_guid, p)) {
				return edge;
			}
		}
	}

	return NULL;
}

/**
 * search for an occurrence of a GUID inside a list of GUIDs.
 */
static bool kcctpl_guid_list_contains(struct GUID_list list, struct GUID guid)
{
	uint32_t i;

	for (i = 0; i < list.count; i++) {
		if (GUID_equal(&list.data[i], &guid)) {
			return true;
		}
	}

	return false;
}

/**
 * search for an occurrence of an ldb_message inside a list of ldb_messages,
 * based on the ldb_message's DN.
 */
static bool kcctpl_message_list_contains_dn(struct message_list list,
					    struct ldb_dn *dn)
{
	uint32_t i;

	for (i = 0; i < list.count; i++) {
		struct ldb_message *message = &list.data[i];

		if (ldb_dn_compare(message->dn, dn) == 0) {
			return true;
		}
	}

	return false;
}

/**
 * get the Transports DN
 * (CN=Inter-Site Transports,CN=Sites,CN=Configuration,DC=<domain>).
 */
static struct ldb_dn *kcctpl_transports_dn(struct ldb_context *ldb,
					   TALLOC_CTX *mem_ctx)
{
	struct ldb_dn *sites_dn;
	bool ok;

	sites_dn = samdb_sites_dn(ldb, mem_ctx);
	if (!sites_dn) {
		return NULL;
	}

	ok = ldb_dn_add_child_fmt(sites_dn, "CN=Inter-Site Transports");
	if (!ok) {
		talloc_free(sites_dn);
		return NULL;
	}

	return sites_dn;
}
/**
 * get the domain local site object.
 */
static struct ldb_message *kcctpl_local_site(struct ldb_context *ldb,
					     TALLOC_CTX *mem_ctx)
{
	int ret;
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *sites_dn;
	struct ldb_result *res;
	const char * const attrs[] = { "objectGUID", "options", NULL };

	tmp_ctx = talloc_new(ldb);

	sites_dn = samdb_sites_dn(ldb, tmp_ctx);
	if (!sites_dn) {
		talloc_free(tmp_ctx);
		return NULL;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, sites_dn, LDB_SCOPE_SUBTREE, attrs,
			 "objectClass=site");

	if (ret != LDB_SUCCESS || res->count == 0) {
		talloc_free(tmp_ctx);
		return NULL;
	}

	talloc_steal(mem_ctx, res);
	talloc_free(tmp_ctx);
	return res->msgs[0];
}

/*
 * compare two internal edges for equality. every field of the structure will be
 * compared.
 */
static bool kcctpl_internal_edge_equal(struct kcctpl_internal_edge *edge1,
				       struct kcctpl_internal_edge *edge2)
{
	if (!edge1 || !edge2) {
		return false;
	}

	if (!GUID_equal(&edge1->v1id, &edge2->v1id)) {
		return false;
	}

	if (!GUID_equal(&edge1->v2id, &edge2->v2id)) {
		return false;
	}

	if (edge1->red_red != edge2->red_red) {
		return false;
	}

	if (edge1->repl_info.cost != edge2->repl_info.cost ||
	    edge1->repl_info.interval != edge2->repl_info.interval ||
	    edge1->repl_info.options != edge2->repl_info.options ||
	    memcmp(&edge1->repl_info.schedule,
		   &edge2->repl_info.schedule, 84) != 0) {
		return false;
	}

	if (!GUID_equal(&edge1->type, &edge2->type)) {
		return false;
	}

	return true;
}

/**
 * create a kcctpl_graph instance.
 */
static NTSTATUS kcctpl_create_graph(TALLOC_CTX *mem_ctx,
				    struct GUID_list guids,
				    struct kcctpl_graph **_graph)
{
	struct kcctpl_graph *graph;
	uint32_t i;

	graph = talloc_zero(mem_ctx, struct kcctpl_graph);
	NT_STATUS_HAVE_NO_MEMORY(graph);

	graph->vertices.count = guids.count;
	graph->vertices.data = talloc_zero_array(graph, struct kcctpl_vertex,
						 guids.count);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(graph->vertices.data, graph);

	TYPESAFE_QSORT(guids.data, guids.count, GUID_compare);

	for (i = 0; i < guids.count; i++) {
		graph->vertices.data[i].id = guids.data[i];
	}

	*_graph = graph;
	return NT_STATUS_OK;
}

/**
 * create a kcctpl_multi_edge instance.
 */
static NTSTATUS kcctpl_create_edge(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
				   struct GUID type,
				   struct ldb_message *site_link,
				   struct kcctpl_multi_edge **_edge)
{
	struct kcctpl_multi_edge *edge;
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *sites_dn;
	struct ldb_result *res;
	const char * const attrs[] = { "siteList", NULL };
	int ret;
	struct ldb_message_element *el;
	unsigned int i;
	struct ldb_val val;

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	edge = talloc_zero(tmp_ctx, struct kcctpl_multi_edge);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(edge, tmp_ctx);

	edge->id = samdb_result_guid(site_link, "objectGUID");

	sites_dn = samdb_sites_dn(ldb, tmp_ctx);
	if (!sites_dn) {
		DEBUG(1, (__location__ ": failed to find our own Sites DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, sites_dn, LDB_SCOPE_SUBTREE, attrs,
			 "objectGUID=%s", GUID_string(tmp_ctx, &edge->id));
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find siteLink object %s: "
			  "%s\n", GUID_string(tmp_ctx, &edge->id),
			  ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (res->count == 0) {
		DEBUG(1, (__location__ ": failed to find siteLink object %s\n",
			  GUID_string(tmp_ctx, &edge->id)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	el = ldb_msg_find_element(res->msgs[0], "siteList");
	if (!el) {
		DEBUG(1, (__location__ ": failed to find siteList attribute of "
			  "object %s\n",
			  ldb_dn_get_linearized(res->msgs[0]->dn)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	edge->vertex_ids.data = talloc_array(edge, struct GUID, el->num_values);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(edge->vertex_ids.data, tmp_ctx);
	edge->vertex_ids.count = el->num_values;

	for (i = 0; i < el->num_values; i++) {
		struct ldb_dn *dn;
		struct GUID guid;

		val = el->values[i];
		dn = ldb_dn_from_ldb_val(tmp_ctx, ldb, &val);
		if (!dn) {
			DEBUG(1, (__location__ ": failed to read a DN from "
				  "siteList attribute of %s\n",
				  ldb_dn_get_linearized(res->msgs[0]->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		ret = dsdb_find_guid_by_dn(ldb, dn, &guid);
		if (ret != LDB_SUCCESS) {
			DEBUG(1, (__location__ ": failed to find objectGUID "
				  "for object %s: %s\n",
				  ldb_dn_get_linearized(dn),
				  ldb_strerror(ret)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		edge->vertex_ids.data[i] = guid;
	}

	edge->repl_info.cost = ldb_msg_find_attr_as_int64(site_link, "cost", 0);
	edge->repl_info.options = ldb_msg_find_attr_as_uint(site_link, "options", 0);
	edge->repl_info.interval = ldb_msg_find_attr_as_int64(site_link,
						      "replInterval", 0);
	/* TODO: edge->repl_info.schedule = site_link!schedule */
	edge->type = type;
	edge->directed = false;

	*_edge = talloc_steal(mem_ctx, edge);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * create a kcctpl_multi_edge_set instance containing edges for all siteLink
 * objects.
 */
static NTSTATUS kcctpl_create_auto_edge_set(struct kcctpl_graph *graph,
					    struct GUID type,
					    struct ldb_result *res_site_link,
					    struct kcctpl_multi_edge_set **_set)
{
	struct kcctpl_multi_edge_set *set;
	TALLOC_CTX *tmp_ctx;
	uint32_t i;

	tmp_ctx = talloc_new(graph);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	set = talloc_zero(tmp_ctx, struct kcctpl_multi_edge_set);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(set, tmp_ctx);

	for (i = 0; i < res_site_link->count; i++) {
		struct GUID site_link_guid;
		struct kcctpl_multi_edge *edge;

		site_link_guid = samdb_result_guid(res_site_link->msgs[i],
						   "objectGUID");
		edge = kcctpl_find_edge_by_guid(graph, site_link_guid);
		if (!edge) {
			DEBUG(1, (__location__ ": failed to find a graph edge "
				  "with ID=%s\n",
				  GUID_string(tmp_ctx, &site_link_guid)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (GUID_equal(&edge->type, &type)) {
			struct GUID *new_data;

			new_data = talloc_realloc(set, set->edge_ids.data,
						  struct GUID,
						  set->edge_ids.count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
			new_data[set->edge_ids.count] = site_link_guid;
			set->edge_ids.data = new_data;
			set->edge_ids.count++;
		}
	}

	*_set = talloc_steal(graph, set);
	return NT_STATUS_OK;
}

/**
 * create a kcctpl_multi_edge_set instance.
 */
static NTSTATUS kcctpl_create_edge_set(struct ldb_context *ldb,
				       struct kcctpl_graph *graph,
				       struct GUID type,
				       struct ldb_message *bridge,
				       struct kcctpl_multi_edge_set **_set)
{
	struct kcctpl_multi_edge_set *set;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message_element *el;
	unsigned int i;

	tmp_ctx = talloc_new(ldb);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	set = talloc_zero(tmp_ctx, struct kcctpl_multi_edge_set);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(set, tmp_ctx);

	set->id = samdb_result_guid(bridge, "objectGUID");

	el = ldb_msg_find_element(bridge, "siteLinkList");
	if (!el) {
		DEBUG(1, (__location__ ": failed to find siteLinkList "
			  "attribute of object %s\n",
			  ldb_dn_get_linearized(bridge->dn)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	for (i = 0; i < el->num_values; i++) {
		struct ldb_val val;
		struct ldb_dn *dn;
		struct GUID site_link_guid;
		int ret;
		struct kcctpl_multi_edge *edge;

		val = el->values[i];
		dn = ldb_dn_from_ldb_val(tmp_ctx, ldb, &val);
		if (!dn) {
			DEBUG(1, (__location__ ": failed to read a DN from "
				  "siteList attribute of %s\n",
				  ldb_dn_get_linearized(bridge->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret = dsdb_find_guid_by_dn(ldb, dn, &site_link_guid);
		if (ret != LDB_SUCCESS) {
			DEBUG(1, (__location__ ": failed to find objectGUID "
				  "for object %s: %s\n",
				  ldb_dn_get_linearized(dn),
				  ldb_strerror(ret)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		edge = kcctpl_find_edge_by_guid(graph, site_link_guid);
		if (!edge) {
			DEBUG(1, (__location__ ": failed to find a graph edge "
				  "with ID=%s\n",
				  GUID_string(tmp_ctx, &site_link_guid)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (GUID_equal(&edge->type, &type)) {
			struct GUID *new_data;

			new_data = talloc_realloc(set, set->edge_ids.data,
						  struct GUID,
						  set->edge_ids.count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
			new_data[set->edge_ids.count] = site_link_guid;
			set->edge_ids.data = new_data;
			set->edge_ids.count++;
		}
	}

	*_set = talloc_steal(graph, set);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * set up a kcctpl_graph, populated with a kcctpl_vertex for each site object, a
 * kcctpl_multi_edge for each siteLink object, and a kcctpl_multi_edge_set for
 * each siteLinkBridge object (or implied siteLinkBridge).
 */
static NTSTATUS kcctpl_setup_graph(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
				   struct kcctpl_graph **_graph)
{
	struct kcctpl_graph *graph;
	struct ldb_dn *sites_dn, *transports_dn;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;
	const char * const transport_attrs[] = { "objectGUID", NULL };
	const char * const site_attrs[] = { "objectGUID", "options", NULL };
	const char * const attrs[] = { "objectGUID", "cost", "options",
				       "replInterval", "schedule", NULL };
	const char * const site_link_bridge_attrs[] = { "objectGUID",
							"siteLinkList",
							NULL };
	int ret;
	struct GUID_list vertex_ids;
	unsigned int i;
	NTSTATUS status;
	struct ldb_message *site;
	uint32_t site_opts;

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	sites_dn = samdb_sites_dn(ldb, tmp_ctx);
	if (!sites_dn) {
		DEBUG(1, (__location__ ": failed to find our own Sites DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, sites_dn, LDB_SCOPE_SUBTREE,
			 site_attrs, "objectClass=site");
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find site objects under "
			  "%s: %s\n", ldb_dn_get_linearized(sites_dn),
			  ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ZERO_STRUCT(vertex_ids);
	for (i = 0; i < res->count; i++) {
		struct GUID guid, *new_data;

		guid = samdb_result_guid(res->msgs[i], "objectGUID");

		new_data = talloc_realloc(tmp_ctx, vertex_ids.data, struct GUID,
					  vertex_ids.count + 1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
		new_data[vertex_ids.count] = guid;
		vertex_ids.data = new_data;
		vertex_ids.count++;
	}

	status = kcctpl_create_graph(tmp_ctx, vertex_ids, &graph);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to create graph: %s\n",
			  nt_errstr(status)));

		talloc_free(tmp_ctx);
		return status;
	}

	site = kcctpl_local_site(ldb, tmp_ctx);
	if (!site) {
		DEBUG(1, (__location__ ": failed to find our own local DC's "
			  "site\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	site_opts = ldb_msg_find_attr_as_uint(site, "options", 0);

	transports_dn = kcctpl_transports_dn(ldb, tmp_ctx);
	if (!transports_dn) {
		DEBUG(1, (__location__ ": failed to find our own Inter-Site "
			  "Transports DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, transports_dn, LDB_SCOPE_ONELEVEL,
			transport_attrs, "objectClass=interSiteTransport");
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find interSiteTransport "
			  "objects under %s: %s\n",
			  ldb_dn_get_linearized(transports_dn),
			  ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	for (i = 0; i < res->count; i++) {
		struct ldb_message *transport;
		struct ldb_result *res_site_link;
		struct GUID transport_guid;
		unsigned int j;
		uint32_t transport_opts;

		transport = res->msgs[i];

		ret = ldb_search(ldb, tmp_ctx, &res_site_link, transport->dn,
				 LDB_SCOPE_SUBTREE, attrs,
				 "objectClass=siteLink");
		if (ret != LDB_SUCCESS) {
			DEBUG(1, (__location__ ": failed to find siteLink "
				  "objects under %s: %s\n",
				  ldb_dn_get_linearized(transport->dn),
				  ldb_strerror(ret)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		transport_guid = samdb_result_guid(transport, "objectGUID");
		for (j = 0; j < res_site_link->count; j++) {
			struct kcctpl_multi_edge *edge, *new_data;

			status = kcctpl_create_edge(ldb, graph, transport_guid,
						    res_site_link->msgs[j],
						    &edge);
			if (NT_STATUS_IS_ERR(status)) {
				DEBUG(1, (__location__ ": failed to create "
					  "edge: %s\n", nt_errstr(status)));
				talloc_free(tmp_ctx);
				return status;
			}

			new_data = talloc_realloc(graph, graph->edges.data,
						  struct kcctpl_multi_edge,
						  graph->edges.count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
			new_data[graph->edges.count] = *edge;
			graph->edges.data = new_data;
			graph->edges.count++;
		}

		transport_opts = ldb_msg_find_attr_as_uint(transport, "options", 0);
		if (!(transport_opts & NTDSTRANSPORT_OPT_BRIDGES_REQUIRED) &&
		    !(site_opts & NTDSSETTINGS_OPT_W2K3_BRIDGES_REQUIRED)) {
			struct kcctpl_multi_edge_set *edge_set, *new_data;

			status = kcctpl_create_auto_edge_set(graph,
							     transport_guid,
							     res_site_link,
							     &edge_set);
			if (NT_STATUS_IS_ERR(status)) {
				DEBUG(1, (__location__ ": failed to create "
					  "edge set: %s\n", nt_errstr(status)));
				talloc_free(tmp_ctx);
				return status;
			}

			new_data = talloc_realloc(graph, graph->edge_sets.data,
						  struct kcctpl_multi_edge_set,
						  graph->edge_sets.count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
			new_data[graph->edge_sets.count] = *edge_set;
			graph->edge_sets.data = new_data;
			graph->edge_sets.count++;
		} else {
			ret = ldb_search(ldb, tmp_ctx, &res_site_link,
					 transport->dn, LDB_SCOPE_SUBTREE,
					 site_link_bridge_attrs,
					 "objectClass=siteLinkBridge");
			if (ret != LDB_SUCCESS) {
				DEBUG(1, (__location__ ": failed to find "
					  "siteLinkBridge objects under %s: "
					  "%s\n",
					  ldb_dn_get_linearized(transport->dn),
					  ldb_strerror(ret)));

				talloc_free(tmp_ctx);
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}

			for (j = 0; j < res_site_link->count; j++) {
				struct ldb_message *bridge;
				struct kcctpl_multi_edge_set *edge_set,
							     *new_data;

				bridge = res_site_link->msgs[j];
				status = kcctpl_create_edge_set(ldb, graph,
								transport_guid,
								bridge,
								&edge_set);
				if (NT_STATUS_IS_ERR(status)) {
					DEBUG(1, (__location__ ": failed to "
						  "create edge set: %s\n",
						  nt_errstr(status)));

					talloc_free(tmp_ctx);
					return status;
				}

				new_data = talloc_realloc(graph,
							  graph->edge_sets.data,
							  struct kcctpl_multi_edge_set,
							  graph->edge_sets.count + 1);
				NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data,
								  tmp_ctx);
				new_data[graph->edge_sets.count] = *edge_set;
				graph->edge_sets.data = new_data;
				graph->edge_sets.count++;
			}
		}
	}

	*_graph = talloc_steal(mem_ctx, graph);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * determine whether a given DC is known to be in a failed state.
 */
static NTSTATUS kcctpl_bridgehead_dc_failed(struct ldb_context *ldb,
					    struct GUID guid,
					    bool detect_failed_dcs,
					    bool *_failed)
{
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *settings_dn;
	struct ldb_result *res;
	const char * const attrs[] = { "options", NULL };
	int ret;
	struct ldb_message *settings;
	uint32_t settings_opts;
	bool failed;

	tmp_ctx = talloc_new(ldb);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	settings_dn = samdb_ntds_settings_dn(ldb);
	if (!settings_dn) {
		DEBUG(1, (__location__ ": failed to find our own NTDS Settings "
			  "DN\n"));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, settings_dn, LDB_SCOPE_BASE, attrs,
			"objectClass=nTDSSiteSettings");
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find site settings object "
			  "%s: %s\n", ldb_dn_get_linearized(settings_dn),
			  ldb_strerror(ret)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (res->count == 0) {
		DEBUG(1, ("failed to find site settings object %s\n",
			  ldb_dn_get_linearized(settings_dn)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	settings = res->msgs[0];

	settings_opts = ldb_msg_find_attr_as_uint(settings, "options", 0);
	if (settings_opts & NTDSSETTINGS_OPT_IS_TOPL_DETECT_STALE_DISABLED) {
		failed = false;
	} else if (true) { /* TODO: how to get kCCFailedLinks and
			      kCCFailedConnections? */
		failed = true;
	} else {
		failed = detect_failed_dcs;
	}

	*_failed = failed;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * get all bridgehead DCs satisfying the given criteria.
 */
static NTSTATUS kcctpl_get_all_bridgehead_dcs(struct kccsrv_service *service,
					      TALLOC_CTX *mem_ctx,
					      struct GUID site_guid,
					      struct ldb_message *cross_ref,
					      struct ldb_message *transport,
					      bool partial_replica_okay,
					      bool detect_failed_dcs,
					      struct message_list *_bridgeheads)
{
	struct message_list bridgeheads, all_dcs_in_site;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;
	struct ldb_dn *sites_dn, *schemas_dn;
	const char * const attrs[] = { "options", NULL };
	int ret;
	struct ldb_message *site, *schema;
	const char * const dc_attrs[] = { "objectGUID", "options", NULL };
	struct ldb_message_element *el;
	unsigned int i;
	const char *transport_name, *transport_address_attr;
	uint32_t site_opts;

	ZERO_STRUCT(bridgeheads);

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	sites_dn = samdb_sites_dn(service->samdb, tmp_ctx);
	if (!sites_dn) {
		DEBUG(1, (__location__ ": failed to find our own Sites DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_search(service->samdb, tmp_ctx, &res, sites_dn, LDB_SCOPE_ONELEVEL,
			 attrs, "(&(objectClass=site)(objectGUID=%s))",
			 GUID_string(tmp_ctx, &site_guid));
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find site object %s: %s\n",
			  GUID_string(tmp_ctx, &site_guid),
			  ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (res->count == 0) {
		DEBUG(1, (__location__ ": failed to find site object %s\n",
			  GUID_string(tmp_ctx, &site_guid)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	site = res->msgs[0];

	schemas_dn = ldb_get_schema_basedn(service->samdb);
	if (!schemas_dn) {
		DEBUG(1, (__location__ ": failed to find our own Schemas DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_search(service->samdb, tmp_ctx, &res, schemas_dn, LDB_SCOPE_SUBTREE,
			 NULL,
			"(&(lDAPDisplayName=nTDSDSA)(objectClass=classSchema))");
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find classSchema object :"
			  "%s\n", ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (res->count == 0) {
		DEBUG(1, (__location__ ": failed to find classSchema "
			  "object\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	schema = res->msgs[0];

	ZERO_STRUCT(all_dcs_in_site);

	ret = ldb_search(service->samdb, tmp_ctx, &res, site->dn, LDB_SCOPE_SUBTREE,
			dc_attrs, "objectCategory=%s",
			ldb_dn_get_linearized(schema->dn));
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find DCs objects :%s\n",
			  ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	el = ldb_msg_find_element(transport, "bridgeheadServerListBL");

	transport_name = ldb_msg_find_attr_as_string(transport, "name", NULL);
	if (!transport_name) {
		DEBUG(1, (__location__ ": failed to find name attribute of "
			  "object %s\n", ldb_dn_get_linearized(transport->dn)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	transport_address_attr = ldb_msg_find_attr_as_string(transport,
						     "transportAddressAttribute",
						     NULL);
	if (!transport_address_attr) {
		DEBUG(1, (__location__ ": failed to find "
			  "transportAddressAttribute attribute of object %s\n",
			  ldb_dn_get_linearized(transport->dn)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	site_opts = ldb_msg_find_attr_as_uint(site, "options", 0);

	for (i = 0; i < res->count; i++) {
		struct ldb_message *dc, *new_data;
		struct ldb_dn *parent_dn;
		uint64_t behavior_version;
		const char *dc_transport_address;
		struct ldb_result *parent_res;
		const char *parent_attrs[] = { transport_address_attr, NULL };
		NTSTATUS status;
		struct GUID dc_guid;
		bool failed;

		dc = res->msgs[i];

		parent_dn = ldb_dn_get_parent(tmp_ctx, dc->dn);
		if (!parent_dn) {
			DEBUG(1, (__location__ ": failed to get parent DN of "
				  "%s\n", ldb_dn_get_linearized(dc->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (el && (el->num_values >= 1)) {
			bool contains;
			unsigned int j;

			contains = false;

			for (j = 0; j < el->num_values; j++) {
				struct ldb_val val;
				struct ldb_dn *dn;

				val = el->values[j];

				dn = ldb_dn_from_ldb_val(tmp_ctx, service->samdb, &val);
				if (!dn) {
					DEBUG(1, (__location__ ": failed to read a DN "
						  "from bridgeheadServerListBL "
						  "attribute of %s\n",
						  ldb_dn_get_linearized(transport->dn)));

					talloc_free(tmp_ctx);
					return NT_STATUS_INTERNAL_DB_CORRUPTION;
				}

				if (ldb_dn_compare(dn, parent_dn) == 0) {
					contains = true;
					break;
				}
			}

			if (!contains) {
				continue;
			}
		}

		/* TODO: if dc is in the same site as the local DC */
		if (true) {
			/* TODO: if a replica of cr!nCName is not in the set of
			 * NC replicas that "should be present" on 'dc' */
			/* TODO: a partial replica of the NC "should be
			   present" */
			if (true || (true && !partial_replica_okay)) {
				continue;
			}
		} else {
			/* TODO: if an NC replica of cr!nCName is not in the set
			 * of NC replicas that "are present" on 'dc' */
			/* TODO: a partial replica of the NC "is present" */
			if (true || (true && !partial_replica_okay)) {
				continue;
			}
		}

		behavior_version = ldb_msg_find_attr_as_int64(dc,
						      "msDS-Behavior-Version", 0);
		/* TODO: cr!nCName corresponds to default NC */
		if (service->am_rodc && true && behavior_version < DS_BEHAVIOR_WIN2008) {
			continue;
		}

		ret = ldb_search(service->samdb, tmp_ctx, &parent_res, parent_dn,
				LDB_SCOPE_BASE, parent_attrs , NULL);

		dc_transport_address = ldb_msg_find_attr_as_string(parent_res->msgs[0],
							   transport_address_attr,
							   NULL);

		if (strncmp(transport_name, "IP", 2) != 0 &&
		    dc_transport_address == NULL) {
			continue;
		}

		dc_guid = samdb_result_guid(dc, "objectGUID");

		status = kcctpl_bridgehead_dc_failed(service->samdb, dc_guid,
						     detect_failed_dcs,
						     &failed);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to check if "
				  "bridgehead DC has failed: %s\n",
				  nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}

		if (failed) {
			continue;
		}

		new_data = talloc_realloc(tmp_ctx, bridgeheads.data,
					  struct ldb_message,
					  bridgeheads.count + 1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
		new_data[bridgeheads.count + 1] = *dc;
		bridgeheads.data = new_data;
		bridgeheads.count++;
	}

	if (site_opts & NTDSSETTINGS_OPT_IS_RAND_BH_SELECTION_DISABLED) {
		qsort(bridgeheads.data, bridgeheads.count,
		      sizeof(struct ldb_message), kcctpl_sort_bridgeheads);
	} else {
		kcctpl_shuffle_bridgeheads(bridgeheads);
	}

	talloc_steal(mem_ctx, bridgeheads.data);
	*_bridgeheads = bridgeheads;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * get a bridgehead DC.
 */
static NTSTATUS kcctpl_get_bridgehead_dc(struct kccsrv_service *service,
					 TALLOC_CTX *mem_ctx,
					 struct GUID site_guid,
					 struct ldb_message *cross_ref,
					 struct ldb_message *transport,
					 bool partial_replica_okay,
					 bool detect_failed_dcs,
					 struct ldb_message **_dsa)
{
	struct message_list dsa_list;
	NTSTATUS status;

	status = kcctpl_get_all_bridgehead_dcs(service, mem_ctx,
					       site_guid, cross_ref, transport,
					       partial_replica_okay,
					       detect_failed_dcs, &dsa_list);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to get all bridgehead DCs: "
			  "%s\n", nt_errstr(status)));
		return status;
	}

	*_dsa = (dsa_list.count == 0) ? NULL : &dsa_list.data[0];

	return NT_STATUS_OK;
}

/*
 * color each vertex to indicate which kinds of NC replicas it contains.
 */
static NTSTATUS kcctpl_color_vertices(struct kccsrv_service *service,
				      struct kcctpl_graph *graph,
				      struct ldb_message *cross_ref,
				      bool detect_failed_dcs,
				      bool *_found_failed_dcs)
{
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *sites_dn;
	bool found_failed_dcs, partial_replica_okay;
	uint32_t i;
	struct ldb_message *site;
	struct ldb_result *res;
	int ret, cr_flags;
	struct GUID site_guid;
	struct kcctpl_vertex *site_vertex;

	found_failed_dcs = false;

	tmp_ctx = talloc_new(service);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	sites_dn = samdb_sites_dn(service->samdb, tmp_ctx);
	if (!sites_dn) {
		DEBUG(1, (__location__ ": failed to find our own Sites DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	for (i = 0; i < graph->vertices.count; i++) {
		struct kcctpl_vertex *vertex;
		struct ldb_dn *nc_name;
		/* TODO: set 'attrs' with its corresponding values */
		const char * const attrs[] = { NULL };

		vertex = &graph->vertices.data[i];

		ret = ldb_search(service->samdb, tmp_ctx, &res, sites_dn,
				 LDB_SCOPE_SUBTREE, attrs, "objectGUID=%s",
				 GUID_string(tmp_ctx, &vertex->id));
		if (ret != LDB_SUCCESS) {
			DEBUG(1, (__location__ ": failed to find site object "
				  "%s: %s\n", GUID_string(tmp_ctx, &vertex->id),
				  ldb_strerror(ret)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		if (res->count == 0) {
			DEBUG(1, (__location__ ": failed to find site object "
				  "%s\n", GUID_string(tmp_ctx, &vertex->id)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		site = res->msgs[0];

		nc_name = samdb_result_dn(service->samdb, tmp_ctx, cross_ref,
					  "nCName", NULL);
		if (!nc_name) {
			DEBUG(1, (__location__ ": failed to find nCName "
				  "attribute of object %s\n",
				  ldb_dn_get_linearized(cross_ref->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (true) { /* TODO: site contains 1+ DCs with full replicas of
			       'nc_name' */
			vertex->color = RED;
		} else if (true) { /* TODO: site contains 1+ partial replicas of
				      'nc_name' */
			vertex->color = BLACK;
		} else {
			vertex->color = WHITE;
		}
	}

	site = kcctpl_local_site(service->samdb, tmp_ctx);
	if (!site) {
		DEBUG(1, (__location__ ": failed to find our own local DC's "
			  "site\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	site_guid = samdb_result_guid(site, "objectGUID");

	site_vertex = kcctpl_find_vertex_by_guid(graph, site_guid);
	if (!site_vertex) {
		DEBUG(1, (__location__ ": failed to find a vertex edge with "
			  "GUID=%s\n", GUID_string(tmp_ctx, &site_guid)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	partial_replica_okay = (site_vertex->color == BLACK);

	cr_flags = ldb_msg_find_attr_as_int64(cross_ref, "systemFlags", 0);

	for (i = 0; i < graph->vertices.count; i++) {
		struct kcctpl_vertex *vertex;
		struct ldb_dn *transports_dn;
		const char * const attrs[] = { "objectGUID", "name",
					       "transportAddressAttribute",
					       NULL };
		unsigned int j;

		vertex = &graph->vertices.data[i];

		transports_dn = kcctpl_transports_dn(service->samdb, tmp_ctx);
		if (!transports_dn) {
			DEBUG(1, (__location__ ": failed to find our own "
				  "Inter-Site Transports DN\n"));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret = ldb_search(service->samdb, tmp_ctx, &res, transports_dn,
				 LDB_SCOPE_ONELEVEL, attrs,
				 "objectClass=interSiteTransport");
		if (ret != LDB_SUCCESS) {
			DEBUG(1, (__location__ ": failed to find "
				  "interSiteTransport objects under %s: %s\n",
				  ldb_dn_get_linearized(transports_dn),
				  ldb_strerror(ret)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		for (j = 0; j < res->count; j++) {
			struct ldb_message *transport, *bridgehead;
			const char *transport_name;
			struct GUID transport_guid, *new_data;
			NTSTATUS status;

			transport = res->msgs[j];

			transport_name = ldb_msg_find_attr_as_string(transport,
							     "name", NULL);
			if (!transport_name) {
				DEBUG(1, (__location__ ": failed to find name "
					  "attribute of object %s\n",
					  ldb_dn_get_linearized(transport->dn)));

				talloc_free(tmp_ctx);
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}

			transport_guid = samdb_result_guid(transport,
							   "objectGUID");

			if (site_vertex->color == RED &&
			    strncmp(transport_name, "IP", 2) != 0 &&
			    (cr_flags & FLAG_CR_NTDS_DOMAIN)) {
				continue;
			}

			if (!kcctpl_find_edge_by_vertex_guid(graph,
							     vertex->id)) {
				continue;
			}

			status = kcctpl_get_bridgehead_dc(service, tmp_ctx,
							  site_vertex->id,
							  cross_ref, transport,
							  partial_replica_okay,
							  detect_failed_dcs,
							  &bridgehead);
			if (NT_STATUS_IS_ERR(status)) {
				DEBUG(1, (__location__ ": failed to get a "
					  "bridgehead DC: %s\n",
					  nt_errstr(status)));

				talloc_free(tmp_ctx);
				return status;
			}
			if (!bridgehead) {
				found_failed_dcs = true;
				continue;
			}

			new_data = talloc_realloc(vertex,
						  vertex->accept_red_red.data,
						  struct GUID,
						  vertex->accept_red_red.count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
			new_data[vertex->accept_red_red.count + 1] = transport_guid;
			vertex->accept_red_red.data = new_data;
			vertex->accept_red_red.count++;

			new_data = talloc_realloc(vertex,
						  vertex->accept_black.data,
						  struct GUID,
						  vertex->accept_black.count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
			new_data[vertex->accept_black.count + 1] = transport_guid;
			vertex->accept_black.data = new_data;
			vertex->accept_black.count++;
		}
	}

	*_found_failed_dcs = found_failed_dcs;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * setup the fields of the vertices that are relevant to Phase I (Dijkstra's
 * Algorithm). for each vertex, set up its cost, root vertex and component. this
 * defines the shortest-path forest structures.
 */
static void kcctpl_setup_vertices(struct kcctpl_graph *graph)
{
	uint32_t i;

	for (i = 0; i < graph->vertices.count; i++) {
		struct kcctpl_vertex *vertex = &graph->vertices.data[i];

		if (vertex->color == WHITE) {
			vertex->repl_info.cost = UINT32_MAX;
			vertex->root_id = vertex->component_id = GUID_zero();
		} else {
			vertex->repl_info.cost = 0;
			vertex->root_id = vertex->component_id = vertex->id;
		}

		vertex->repl_info.interval = 0;
		vertex->repl_info.options = 0xFFFFFFFF;
		ZERO_STRUCT(vertex->repl_info.schedule);
		vertex->demoted = false;
	}
}

/**
 * demote one vertex if necessary.
 */
static void kcctpl_check_demote_one_vertex(struct kcctpl_vertex *vertex,
					   struct GUID type)
{
	if (vertex->color == WHITE) {
		return;
	}

	if (!kcctpl_guid_list_contains(vertex->accept_black, type) &&
	    !kcctpl_guid_list_contains(vertex->accept_red_red, type)) {
		vertex->repl_info.cost = UINT32_MAX;
		vertex->root_id = GUID_zero();
		vertex->demoted = true;
	}
}

/**
 * clear the demoted state of a vertex.
 */
static void kcctpl_undemote_one_vertex(struct kcctpl_vertex *vertex)
{
	if (vertex->color == WHITE) {
		return;
	}

	vertex->repl_info.cost = 0;
	vertex->root_id = vertex->id;
	vertex->demoted = false;
}

/**
 * returns the id of the component containing 'vertex' by traversing the up-tree
 * implied by the component pointers.
 */
static struct GUID kcctpl_get_component_id(struct kcctpl_graph *graph,
					   struct kcctpl_vertex *vertex)
{
	struct kcctpl_vertex *u;
	struct GUID root;

	u = vertex;
	while (!GUID_equal(&u->component_id, &u->id)) {
		u = kcctpl_find_vertex_by_guid(graph, u->component_id);
	}

	root = u->id;

	u = vertex;
	while (!GUID_equal(&u->component_id, &u->id)) {
		struct kcctpl_vertex *w;

		w = kcctpl_find_vertex_by_guid(graph, u->component_id);
		u->component_id = root;
		u = w;
	}

	return root;
}

/**
 * copy all spanning tree edges from 'output_edges' that contain the vertex for
 * DCs in the local DC's site.
 */
static NTSTATUS kcctpl_copy_output_edges(struct kccsrv_service *service,
					 TALLOC_CTX *mem_ctx,
					 struct kcctpl_graph *graph,
					 struct kcctpl_multi_edge_list output_edges,
					 struct kcctpl_multi_edge_list *_copy)
{
	struct kcctpl_multi_edge_list copy;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message *site;
	struct GUID site_guid;
	uint32_t i;

	ZERO_STRUCT(copy);

	tmp_ctx = talloc_new(service);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	site = kcctpl_local_site(service->samdb, tmp_ctx);
	if (!site) {
		DEBUG(1, (__location__ ": failed to find our own local DC's "
			  "site\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	site_guid = samdb_result_guid(site, "objectGUID");

	for (i = 0; i < output_edges.count; i++) {
		struct kcctpl_multi_edge *edge;
		struct kcctpl_vertex *vertex1, *vertex2;

		edge = &output_edges.data[i];

		vertex1 = kcctpl_find_vertex_by_guid(graph,
						     edge->vertex_ids.data[0]);
		if (!vertex1) {
			DEBUG(1, (__location__ ": failed to find vertex %s\n",
				  GUID_string(tmp_ctx,
					      &edge->vertex_ids.data[0])));
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		vertex2 = kcctpl_find_vertex_by_guid(graph,
						     edge->vertex_ids.data[1]);
		if (!vertex2) {
			DEBUG(1, (__location__ ": failed to find vertex %s\n",
				  GUID_string(tmp_ctx,
					      &edge->vertex_ids.data[1])));
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (GUID_equal(&vertex1->id, &site_guid) ||
		    GUID_equal(&vertex2->id, &site_guid)) {
			struct kcctpl_multi_edge *new_data;

			if ((vertex1->color == BLACK ||
			     vertex2->color == BLACK) &&
			    vertex1->dist_to_red != UINT32_MAX) {

				edge->directed = true;

				if (vertex2->dist_to_red <
				    vertex1->dist_to_red) {
					struct GUID tmp;

					tmp = edge->vertex_ids.data[0];
					edge->vertex_ids.data[0] = edge->vertex_ids.data[1];
					edge->vertex_ids.data[1] = tmp;
				}
			}

			new_data = talloc_realloc(tmp_ctx, copy.data,
						  struct kcctpl_multi_edge,
						  copy.count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
			new_data[copy.count + 1] = *edge;
			copy.data = new_data;
			copy.count++;
		}
	}

	talloc_steal(mem_ctx, copy.data);
	talloc_free(tmp_ctx);
	*_copy = copy;
	return NT_STATUS_OK;
}

/**
 * build the initial sequence for use with Dijkstra's algorithm. it will contain
 * the red and black vertices as root vertices, unless these vertices accept no
 * edges of the current 'type', or unless black vertices are not being
 * including.
 */
static NTSTATUS kcctpl_setup_dijkstra(TALLOC_CTX *mem_ctx,
				      struct kcctpl_graph *graph,
				      struct GUID type, bool include_black,
				      struct kcctpl_vertex_list *_vertices)
{
	struct kcctpl_vertex_list vertices;
	uint32_t i;

	kcctpl_setup_vertices(graph);

	ZERO_STRUCT(vertices);

	for (i = 0; i < graph->vertices.count; i++) {
		struct kcctpl_vertex *vertex = &graph->vertices.data[i];

		if (vertex->color == WHITE) {
			continue;
		}

		if ((vertex->color == BLACK && !include_black) ||
		    !kcctpl_guid_list_contains(vertex->accept_black, type) ||
		    !kcctpl_guid_list_contains(vertex->accept_red_red, type)) {
			vertex->repl_info.cost = UINT32_MAX;
			vertex->root_id = GUID_zero();
			vertex->demoted = true;
		} else {
			struct kcctpl_vertex *new_data;

			new_data = talloc_realloc(mem_ctx, vertices.data,
						  struct kcctpl_vertex,
						  vertices.count + 1);
			NT_STATUS_HAVE_NO_MEMORY(new_data);
			new_data[vertices.count] = *vertex;
			vertices.data = new_data;
			vertices.count++;
		}
	}

	*_vertices = vertices;
	return NT_STATUS_OK;
}

/**
 * merge schedules, replication intervals, options and costs.
 */
static bool kcctpl_combine_repl_info(struct kcctpl_graph *graph,
				     struct kcctpl_repl_info *ria,
				     struct kcctpl_repl_info *rib,
				     struct kcctpl_repl_info *ric)
{
	uint8_t schedule[84];
	bool is_available;
	uint32_t i;
	int32_t ric_cost;

	is_available = false;
	for (i = 0; i < 84; i++) {
		schedule[i] = ria->schedule[i] & rib->schedule[i];

		if (schedule[i] == 1) {
			is_available = true;
		}
	}
	if (!is_available) {
		return false;
	}

	ric_cost = ria->cost + rib->cost;
	ric->cost = (ric_cost < 0) ? UINT32_MAX : ric_cost;

	ric->interval = MAX(ria->interval, rib->interval);
	ric->options = ria->options & rib->options;
	memcpy(&ric->schedule, &schedule, 84);

	return true;
}

/**
 * helper function for Dijkstra's algorithm. a new path has been found from a
 * root vertex to vertex 'vertex2'. this path is ('vertex1->root, ..., vertex1,
 * vertex2'). 'edge' is the edge connecting 'vertex1' and 'vertex2'. if this new
 * path is better (in this case cheaper, or has a longer schedule), update
 * 'vertex2' to use the new path.
 */
static NTSTATUS kcctpl_try_new_path(TALLOC_CTX *mem_ctx,
				    struct kcctpl_graph *graph,
				    struct kcctpl_vertex_list vertices,
				    struct kcctpl_vertex *vertex1,
				    struct kcctpl_multi_edge *edge,
				    struct kcctpl_vertex *vertex2)
{
	struct kcctpl_repl_info new_repl_info;
	bool intersect;
	uint32_t i, new_duration, old_duration;

	ZERO_STRUCT(new_repl_info);

	intersect = kcctpl_combine_repl_info(graph, &vertex1->repl_info,
					     &edge->repl_info, &new_repl_info);

	if (new_repl_info.cost > vertex2->repl_info.cost) {
		return NT_STATUS_OK;
	}

	if (new_repl_info.cost < vertex2->repl_info.cost && !intersect) {
		return NT_STATUS_OK;
	}

	new_duration = old_duration = 0;
	for (i = 0; i < 84; i++) {
		if (new_repl_info.schedule[i] == 1) {
			new_duration++;
		}

		if (vertex2->repl_info.schedule[i] == 1) {
			old_duration++;
		}
	}

	if (new_repl_info.cost < vertex2->repl_info.cost ||
	    new_duration > old_duration) {
		struct kcctpl_vertex *new_data;

		vertex2->root_id = vertex1->root_id;
		vertex2->component_id = vertex1->component_id;
		vertex2->repl_info = new_repl_info;

		new_data = talloc_realloc(mem_ctx, vertices.data,
					  struct kcctpl_vertex,
					  vertices.count + 1);
		NT_STATUS_HAVE_NO_MEMORY(new_data);
		new_data[vertices.count + 1] = *vertex2;
		vertices.data = new_data;
		vertices.count++;
	}

	return NT_STATUS_OK;
}

/**
 * run Dijkstra's algorithm with the red (and possibly black) vertices as the
 * root vertices, and build up a shortest-path forest.
 */
static NTSTATUS kcctpl_dijkstra(struct kcctpl_graph *graph, struct GUID type,
				bool include_black)
{
	TALLOC_CTX *tmp_ctx;
	struct kcctpl_vertex_list vertices;
	NTSTATUS status;

	tmp_ctx = talloc_new(graph);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	status = kcctpl_setup_dijkstra(tmp_ctx, graph, type, include_black,
				       &vertices);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to build the initial sequence "
			  "for Dijkstra's algorithm: %s\n", nt_errstr(status)));

		talloc_free(tmp_ctx);
		return status;
	}

	while (vertices.count > 0) {
		uint32_t minimum_cost, minimum_index, i;
		struct kcctpl_vertex *minimum_vertex, *new_data;

		minimum_cost = UINT32_MAX;
		minimum_index = -1;
		minimum_vertex = NULL;
		for (i = 0; i < vertices.count; i++) {
			struct kcctpl_vertex *vertex = &vertices.data[i];

			if (vertex->repl_info.cost < minimum_cost) {
				minimum_cost = vertex->repl_info.cost;
				minimum_vertex = vertex;
				minimum_index = i;
			} else if (vertex->repl_info.cost == minimum_cost &&
				   GUID_compare(&vertex->id,
					        &minimum_vertex->id) < 0) {
				minimum_vertex = vertex;
				minimum_index = i;
			}
		}

		if (minimum_index < vertices.count - 1) {
			memcpy(&vertices.data[minimum_index + 1],
			       &vertices.data[minimum_index],
			       vertices.count - minimum_index - 1);
		}
		new_data = talloc_realloc(tmp_ctx, vertices.data,
					  struct kcctpl_vertex,
					  vertices.count - 1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
		talloc_free(vertices.data);
		vertices.data = new_data;
		vertices.count--;

		for (i = 0; i < graph->edges.count; i++) {
			struct kcctpl_multi_edge *edge = &graph->edges.data[i];

			if (kcctpl_guid_list_contains(minimum_vertex->edge_ids,
						      edge->id)) {
				uint32_t j;

				for (j = 0; j < edge->vertex_ids.count; j++) {
					struct GUID vertex_id;
					struct kcctpl_vertex *vertex;

					vertex_id = edge->vertex_ids.data[j];
					vertex = kcctpl_find_vertex_by_guid(graph,
									    vertex_id);
					if (!vertex) {
						DEBUG(1, (__location__
							  ": failed to find "
							  "vertex %s\n",
							  GUID_string(tmp_ctx,
								      &vertex_id)));

						talloc_free(tmp_ctx);
						return NT_STATUS_INTERNAL_DB_CORRUPTION;
					}

					kcctpl_try_new_path(tmp_ctx, graph,
							    vertices,
							    minimum_vertex,
							    edge, vertex);
				}
			}
		}
	}

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * add an edge to the list of edges that will be processed with Kruskal's. the
 * endpoints are in fact the root of the vertices to pass in, so the endpoints
 * are always colored vertices.
 */
static NTSTATUS kcctpl_add_int_edge(TALLOC_CTX *mem_ctx,
				    struct kcctpl_graph *graph,
				    struct kcctpl_internal_edge_list internal_edges,
				    struct kcctpl_multi_edge *edge,
				    struct kcctpl_vertex *vertex1,
				    struct kcctpl_vertex *vertex2)
{
	struct kcctpl_vertex *root1, *root2;
	bool red_red, found;
	struct kcctpl_repl_info repl_info1, repl_info2;
	struct kcctpl_internal_edge new_internal_edge, *new_data;
	uint32_t i;

	root1 = kcctpl_find_vertex_by_guid(graph, vertex1->root_id);
	if (!root1) {
		TALLOC_CTX *tmp_ctx = talloc_new(graph);
		NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

		DEBUG(1, (__location__ ": failed to find vertex %s\n",
			  GUID_string(tmp_ctx, &vertex1->root_id)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	root2 = kcctpl_find_vertex_by_guid(graph, vertex2->root_id);
	if (!root2) {
		TALLOC_CTX *tmp_ctx = talloc_new(graph);
		NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

		DEBUG(1, (__location__ ": failed to find vertex %s\n",
			  GUID_string(tmp_ctx, &vertex2->root_id)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	red_red = (root1->color == RED && root2->color == RED);

	if (red_red) {
		if (!kcctpl_guid_list_contains(root1->accept_red_red,
					       edge->type) ||
		    !kcctpl_guid_list_contains(root2->accept_red_red,
					       edge->type)) {
			return NT_STATUS_OK;
		}
	} else if (!kcctpl_guid_list_contains(root1->accept_black,
					      edge->type) ||
		   !kcctpl_guid_list_contains(root2->accept_black,
					      edge->type)) {
		return NT_STATUS_OK;
	}

	if (!kcctpl_combine_repl_info(graph, &vertex1->repl_info,
				      &vertex2->repl_info, &repl_info1) ||
	    !kcctpl_combine_repl_info(graph, &repl_info1, &edge->repl_info,
				      &repl_info2)) {
		return NT_STATUS_OK;
	}

	new_internal_edge.v1id = root1->id;
	new_internal_edge.v2id = root2->id;
	new_internal_edge.red_red = red_red;
	new_internal_edge.repl_info = repl_info2;
	new_internal_edge.type = edge->type;

	if (GUID_compare(&new_internal_edge.v1id,
			 &new_internal_edge.v2id) > 0) {
		struct GUID tmp_guid = new_internal_edge.v1id;

		new_internal_edge.v1id = new_internal_edge.v2id;
		new_internal_edge.v2id = tmp_guid;
	}

	found = false;
	for (i = 0; i < internal_edges.count; i++) {
		struct kcctpl_internal_edge *ie = &internal_edges.data[i];

		if (kcctpl_internal_edge_equal(ie, &new_internal_edge)) {
			found = true;
		}
	}
	if (found) {
		return NT_STATUS_OK;
	}

	new_data = talloc_realloc(mem_ctx, internal_edges.data,
				  struct kcctpl_internal_edge,
				  internal_edges.count + 1);
	NT_STATUS_HAVE_NO_MEMORY(new_data);
	new_data[internal_edges.count + 1] = new_internal_edge;
	internal_edges.data = new_data;
	internal_edges.count++;

	return NT_STATUS_OK;
}

/**
 * after running Dijkstra's algorithm, this function examines a multi-edge and
 * adds internal edges between every tree connected by this edge.
 */
static NTSTATUS kcctpl_process_edge(TALLOC_CTX *mem_ctx,
				    struct kcctpl_graph *graph,
				    struct kcctpl_multi_edge *edge,
				    struct kcctpl_internal_edge_list internal_edges)
{
	TALLOC_CTX *tmp_ctx;
	struct kcctpl_vertex_list vertices;
	uint32_t i;
	struct kcctpl_vertex *best_vertex;

	ZERO_STRUCT(vertices);

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	for (i = 0; i < edge->vertex_ids.count; i++) {
		struct GUID id;
		struct kcctpl_vertex *vertex, *new_data;

		id = edge->vertex_ids.data[i];

		vertex = kcctpl_find_vertex_by_guid(graph, id);
		if (!vertex) {
			DEBUG(1, (__location__ ": failed to find vertex %s\n",
				  GUID_string(tmp_ctx, &id)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		new_data = talloc_realloc(tmp_ctx, vertices.data,
					  struct kcctpl_vertex,
					  vertices.count + 1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
		new_data[vertices.count] = *vertex;
		vertices.data = new_data;
		vertices.count++;
	}

	qsort(vertices.data, vertices.count, sizeof(struct kcctpl_vertex),
	      kcctpl_sort_vertices);

	best_vertex = &vertices.data[0];

	for (i = 0; i < edge->vertex_ids.count; i++) {
		struct GUID id, empty_id = GUID_zero();
		struct kcctpl_vertex *vertex = &graph->vertices.data[i];

		id = edge->vertex_ids.data[i];

		vertex = kcctpl_find_vertex_by_guid(graph, id);
		if (!vertex) {
			DEBUG(1, (__location__ ": failed to find vertex %s\n",
				  GUID_string(tmp_ctx, &id)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (!GUID_equal(&vertex->component_id, &empty_id) &&
		    !GUID_equal(&vertex->root_id, &empty_id)) {
			continue;
		}

		if (!GUID_equal(&best_vertex->component_id,
				&empty_id) &&
		    !GUID_equal(&best_vertex->root_id, &empty_id) &&
		    !GUID_equal(&vertex->component_id, &empty_id) &&
		    !GUID_equal(&vertex->root_id, &empty_id) &&
		    !GUID_equal(&best_vertex->component_id,
				&vertex->component_id)) {
			NTSTATUS status;

			status = kcctpl_add_int_edge(mem_ctx, graph,
						     internal_edges,
						     edge, best_vertex,
						     vertex);
			if (NT_STATUS_IS_ERR(status)) {
				DEBUG(1, (__location__ ": failed to add an "
					  "internal edge for %s: %s\n",
					  GUID_string(tmp_ctx, &vertex->id),
					  nt_errstr(status)));
				talloc_free(tmp_ctx);
				return status;
			}
		}
	}

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * after running Dijkstra's algorithm to determine the shortest-path forest,
 * examine all edges in this edge set. find all inter-tree edges, from which to
 * build the list of 'internal edges', which will later be passed on to
 * Kruskal's algorithm.
 */
static NTSTATUS kcctpl_process_edge_set(TALLOC_CTX *mem_ctx,
					struct kcctpl_graph *graph,
					struct kcctpl_multi_edge_set *set,
					struct kcctpl_internal_edge_list internal_edges)
{
	uint32_t i;

	if (!set) {
		for (i = 0; i < graph->edges.count; i++) {
			struct kcctpl_multi_edge *edge;
			uint32_t j;
			NTSTATUS status;

			edge = &graph->edges.data[i];

			for (j = 0; j < edge->vertex_ids.count; j++) {
				struct GUID id;
				struct kcctpl_vertex *vertex;

				id = edge->vertex_ids.data[j];

				vertex = kcctpl_find_vertex_by_guid(graph, id);
				if (!vertex) {
					TALLOC_CTX *tmp_ctx;

					tmp_ctx = talloc_new(mem_ctx);
					NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

					DEBUG(1, (__location__ ": failed to "
						  "find vertex %s\n",
						  GUID_string(tmp_ctx, &id)));

					talloc_free(tmp_ctx);
					return NT_STATUS_INTERNAL_DB_CORRUPTION;
				}

				kcctpl_check_demote_one_vertex(vertex,
							       edge->type);
			}

			status = kcctpl_process_edge(mem_ctx, graph, edge,
						     internal_edges);
			if (NT_STATUS_IS_ERR(status)) {
				TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
				NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

				DEBUG(1, (__location__ ": failed to process "
					  "edge %s: %s\n",
					  GUID_string(tmp_ctx, &edge->id),
					  nt_errstr(status)));

				talloc_free(tmp_ctx);
				return status;
			}

			for (j = 0; j < edge->vertex_ids.count; j++) {
				struct GUID id;
				struct kcctpl_vertex *vertex;

				id = edge->vertex_ids.data[j];

				vertex = kcctpl_find_vertex_by_guid(graph, id);
				if (!vertex) {
					TALLOC_CTX *tmp_ctx;

					tmp_ctx = talloc_new(mem_ctx);
					NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

					DEBUG(1, (__location__ ": failed to "
						  "find vertex %s\n",
						  GUID_string(tmp_ctx, &id)));

					talloc_free(tmp_ctx);
					return NT_STATUS_INTERNAL_DB_CORRUPTION;
				}

				kcctpl_undemote_one_vertex(vertex);
			}
		}
	} else {
		for (i = 0; i < graph->edges.count; i++) {
			struct kcctpl_multi_edge *edge = &graph->edges.data[i];

			if (kcctpl_guid_list_contains(set->edge_ids,
						      edge->id)) {
				NTSTATUS status;

				status = kcctpl_process_edge(mem_ctx, graph,
							     edge,
							     internal_edges);
				if (NT_STATUS_IS_ERR(status)) {
					TALLOC_CTX *tmp_ctx;

					tmp_ctx = talloc_new(mem_ctx);
					NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

					DEBUG(1, (__location__ ": failed to "
						  "process edge %s: %s\n",
						  GUID_string(tmp_ctx,
							      &edge->id),
						  nt_errstr(status)));

					talloc_free(tmp_ctx);
					return status;
				}
			}
		}
	}

	return NT_STATUS_OK;
}

/**
 * a new edge, 'internal_edge', has been found for the spanning tree edge. add
 * this edge to the list of output edges.
 */
static NTSTATUS kcctpl_add_out_edge(TALLOC_CTX *mem_ctx,
				    struct kcctpl_graph *graph,
				    struct kcctpl_multi_edge_list output_edges,
				    struct kcctpl_internal_edge *internal_edge)
{
	struct kcctpl_vertex *vertex1, *vertex2;
	TALLOC_CTX *tmp_ctx;
	struct kcctpl_multi_edge *new_edge, *new_data;
	struct GUID *new_data_id;

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	vertex1 = kcctpl_find_vertex_by_guid(graph, internal_edge->v1id);
	if (!vertex1) {
		DEBUG(1, (__location__ ": failed to find vertex %s\n",
			  GUID_string(tmp_ctx, &internal_edge->v1id)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	vertex2 = kcctpl_find_vertex_by_guid(graph, internal_edge->v2id);
	if (!vertex2) {
		DEBUG(1, (__location__ ": failed to find vertex %s\n",
			  GUID_string(tmp_ctx, &internal_edge->v2id)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	new_edge = talloc(tmp_ctx, struct kcctpl_multi_edge);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_edge, tmp_ctx);

	new_edge->id = GUID_random(); /* TODO: what should be new_edge->GUID? */
	new_edge->directed = false;

	new_edge->vertex_ids.data = talloc_array(new_edge, struct GUID, 2);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_edge->vertex_ids.data, tmp_ctx);

	new_edge->vertex_ids.data[0] = vertex1->id;
	new_edge->vertex_ids.data[1] = vertex2->id;
	new_edge->vertex_ids.count = 2;

	new_edge->type = internal_edge->type;
	new_edge->repl_info = internal_edge->repl_info;

	new_data = talloc_realloc(tmp_ctx, output_edges.data,
				  struct kcctpl_multi_edge,
				  output_edges.count + 1);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
	new_data[output_edges.count + 1] = *new_edge;
	output_edges.data = new_data;
	output_edges.count++;

	new_data_id = talloc_realloc(vertex1, vertex1->edge_ids.data,
				     struct GUID, vertex1->edge_ids.count);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data_id, tmp_ctx);
	new_data_id[vertex1->edge_ids.count] = new_edge->id;
	talloc_free(vertex1->edge_ids.data);
	vertex1->edge_ids.data = new_data_id;
	vertex1->edge_ids.count++;

	new_data_id = talloc_realloc(vertex2, vertex2->edge_ids.data,
				     struct GUID, vertex2->edge_ids.count);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data_id, tmp_ctx);
	new_data_id[vertex2->edge_ids.count] = new_edge->id;
	talloc_free(vertex2->edge_ids.data);
	vertex2->edge_ids.data = new_data_id;
	vertex2->edge_ids.count++;

	talloc_steal(graph, new_edge);
	talloc_steal(mem_ctx, output_edges.data);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * run Kruskal's minimum-cost spanning tree algorithm on the internal edges
 * (that represent shortest paths in the original graph between colored
 * vertices).
 */
static NTSTATUS kcctpl_kruskal(TALLOC_CTX *mem_ctx, struct kcctpl_graph *graph,
			       struct kcctpl_internal_edge_list internal_edges,
			       struct kcctpl_multi_edge_list *_output_edges)
{
	uint32_t i, num_expected_tree_edges, cst_edges;
	struct kcctpl_multi_edge_list output_edges;

	num_expected_tree_edges = 0;
	for (i = 0; i < graph->vertices.count; i++) {
		struct kcctpl_vertex *vertex = &graph->vertices.data[i];

		talloc_free(vertex->edge_ids.data);
		ZERO_STRUCT(vertex->edge_ids);

		if (vertex->color == RED || vertex->color == WHITE) {
			num_expected_tree_edges++;
		}
	}

	qsort(internal_edges.data, internal_edges.count,
	      sizeof(struct kcctpl_internal_edge), kcctpl_sort_internal_edges);

	cst_edges = 0;

	ZERO_STRUCT(output_edges);

	while (internal_edges.count > 0 &&
	       cst_edges < num_expected_tree_edges) {
		struct kcctpl_internal_edge *edge, *new_data;
		struct kcctpl_vertex *vertex1, *vertex2;
		struct GUID comp1, comp2;

		edge = &internal_edges.data[0];

		vertex1 = kcctpl_find_vertex_by_guid(graph, edge->v1id);
		if (!vertex1) {
			TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
			NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

			DEBUG(1, (__location__ ": failed to find vertex %s\n",
				  GUID_string(tmp_ctx, &edge->v1id)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		vertex2 = kcctpl_find_vertex_by_guid(graph, edge->v2id);
		if (!vertex2) {
			TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
			NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

			DEBUG(1, (__location__ ": failed to find vertex %s\n",
				  GUID_string(tmp_ctx, &edge->v2id)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		comp1 = kcctpl_get_component_id(graph, vertex1);
		comp2 = kcctpl_get_component_id(graph, vertex2);

		if (!GUID_equal(&comp1, &comp2)) {
			NTSTATUS status;
			struct kcctpl_vertex *vertex;

			cst_edges++;

			status = kcctpl_add_out_edge(mem_ctx, graph,
						     output_edges, edge);
			if (NT_STATUS_IS_ERR(status)) {
				TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
				NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

				DEBUG(1, (__location__ ": failed to add an "
					  "output edge between %s and %s: %s\n",
					  GUID_string(tmp_ctx, &edge->v1id),
					  GUID_string(tmp_ctx, &edge->v2id),
					  nt_errstr(status)));

				talloc_free(tmp_ctx);
				return status;
			}

			vertex = kcctpl_find_vertex_by_guid(graph, comp1);
			if (!vertex) {
				TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
				NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

				DEBUG(1, (__location__ ": failed to find "
					  "vertex %s\n", GUID_string(tmp_ctx,
								     &comp1)));

				talloc_free(tmp_ctx);
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}
			vertex->component_id = comp2;
		}

		internal_edges.data = internal_edges.data + 1;
		new_data = talloc_realloc(mem_ctx, internal_edges.data,
					  struct kcctpl_internal_edge,
					  internal_edges.count - 1);
		NT_STATUS_HAVE_NO_MEMORY(new_data);
		talloc_free(internal_edges.data);
		internal_edges.data = new_data;
		internal_edges.count--;
	}

	*_output_edges = output_edges;
	return NT_STATUS_OK;
}

/**
 * count the number of components. a component is considered to be a bunch of
 * colored vertices that are connected by the spanning tree. vertices whose
 * component ID is the same as their vertex ID are the root of the connected
 * component.
 */
static uint32_t kcctpl_count_components(struct kcctpl_graph *graph)
{
	uint32_t num_components = 0, i;

	for (i = 0; i < graph->vertices.count; i++) {
		struct kcctpl_vertex *vertex;
		struct GUID component_id;

		vertex = &graph->vertices.data[i];

		if (vertex->color == WHITE) {
			continue;
		}

		component_id = kcctpl_get_component_id(graph, vertex);
		if (GUID_equal(&component_id, &vertex->id)) {
			vertex->component_index = num_components;
			num_components++;
		}
	}

	return num_components;
}

/**
 * calculate the spanning tree and return the edges that include the vertex for
 * the local site.
 */
static NTSTATUS kcctpl_get_spanning_tree_edges(struct kccsrv_service *service,
					       TALLOC_CTX *mem_ctx,
					       struct kcctpl_graph *graph,
					       uint32_t *_component_count,
					       struct kcctpl_multi_edge_list *_st_edge_list)
{
	TALLOC_CTX *tmp_ctx;
	struct kcctpl_internal_edge_list internal_edges;
	uint32_t i, component_count;
	NTSTATUS status;
	struct kcctpl_multi_edge_list output_edges, st_edge_list;

	ZERO_STRUCT(internal_edges);

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	for (i = 0; i < graph->edge_sets.count; i++) {
		struct kcctpl_multi_edge_set *set;
		struct GUID edge_type;
		uint32_t j;

		set = &graph->edge_sets.data[i];

		edge_type = GUID_zero();

		for (j = 0; j < graph->vertices.count; j++) {
			struct kcctpl_vertex *vertex = &graph->vertices.data[j];

			talloc_free(vertex->edge_ids.data);
			ZERO_STRUCT(vertex->edge_ids.data);
		}

		for (j = 0; j < set->edge_ids.count; j++) {
			struct GUID edge_id;
			struct kcctpl_multi_edge *edge;
			uint32_t k;

			edge_id = set->edge_ids.data[j];
			edge = kcctpl_find_edge_by_guid(graph, edge_id);
			if (!edge) {
				DEBUG(1, (__location__ ": failed to find a "
					  "graph edge with ID=%s\n",
					  GUID_string(tmp_ctx, &edge_id)));

				talloc_free(tmp_ctx);
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}

			edge_type = edge->type;

			for (k = 0; k < edge->vertex_ids.count; k++) {
				struct GUID vertex_id, *new_data;
				struct kcctpl_vertex *vertex;

				vertex_id = edge->vertex_ids.data[k];
				vertex = kcctpl_find_vertex_by_guid(graph,
								    vertex_id);
				if (!vertex) {
					DEBUG(1, (__location__ ": failed to "
						  "find a graph vertex with "
						  "ID=%s\n",
						  GUID_string(tmp_ctx,
							      &edge_id)));

					talloc_free(tmp_ctx);
					return NT_STATUS_INTERNAL_DB_CORRUPTION;
				}

				new_data = talloc_realloc(tmp_ctx,
							  vertex->edge_ids.data,
							  struct GUID,
							  vertex->edge_ids.count + 1);
				NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data,
								  tmp_ctx);
				new_data[vertex->edge_ids.count] = edge->id;
				vertex->edge_ids.data = new_data;
				vertex->edge_ids.count++;
			}
		}

		status = kcctpl_dijkstra(graph, edge_type, false);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to run Dijkstra's "
				  "algorithm: %s\n", nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}

		status = kcctpl_process_edge_set(tmp_ctx, graph, set,
						 internal_edges);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to process edge set "
				  "%s: %s\n", GUID_string(tmp_ctx, &set->id),
				  nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}

		status = kcctpl_dijkstra(graph, edge_type, true);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to run Dijkstra's "
				  "algorithm: %s\n", nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}

		status = kcctpl_process_edge_set(tmp_ctx, graph, set,
						 internal_edges);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to process edge set "
				  "%s: %s\n", GUID_string(tmp_ctx, &set->id),
				  nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}
	}

	kcctpl_setup_vertices(graph);

	status = kcctpl_process_edge_set(tmp_ctx, graph, NULL, internal_edges);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to process empty edge set: "
			  "%s\n", nt_errstr(status)));

		talloc_free(tmp_ctx);
		return status;
	}

	status = kcctpl_kruskal(tmp_ctx, graph, internal_edges, &output_edges);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to run Kruskal's algorithm: "
			  "%s\n", nt_errstr(status)));

		talloc_free(tmp_ctx);
		return status;
	}

	for (i = 0; i < graph->vertices.count; i++) {
		struct kcctpl_vertex *vertex = &graph->vertices.data[i];

		if (vertex->color == RED) {
			vertex->dist_to_red = 0;
		} else if (true) { /* TODO: if there exists a path from 'vertex'
				    to a RED vertex */
			vertex->dist_to_red = -1; /* TODO: the length of the
						     shortest such path */
		} else {
			vertex->dist_to_red = UINT32_MAX;
		}
	}

	component_count = kcctpl_count_components(graph);

	status = kcctpl_copy_output_edges(service, tmp_ctx, graph, output_edges,
					  &st_edge_list);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to copy edge list: %s\n",
			  nt_errstr(status)));

		talloc_free(tmp_ctx);
		return status;
	}

	*_component_count = component_count;
	talloc_steal(mem_ctx, st_edge_list.data);
	*_st_edge_list = st_edge_list;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * creat an nTDSConnection object with the given parameters if one does not
 * already exist.
 */
static NTSTATUS kcctpl_create_connection(struct kccsrv_service *service,
					 TALLOC_CTX *mem_ctx,
					 struct ldb_message *cross_ref,
					 struct ldb_message *r_bridgehead,
					 struct ldb_message *transport,
					 struct ldb_message *l_bridgehead,
					 struct kcctpl_repl_info repl_info,
					 uint8_t schedule[84],
					 bool detect_failed_dcs,
					 bool partial_replica_okay,
					 struct GUID_list *_keep_connections)
{
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *r_site_dn, *l_site_dn, *servers_dn;
	bool ok;
	struct GUID r_site_guid, l_site_guid;
	int ret;
	struct message_list r_bridgeheads_all, l_bridgeheads_all,
			    r_bridgeheads_available, l_bridgeheads_available;
	NTSTATUS status;
	struct ldb_result *res;
	const char * const attrs[] = { "objectGUID", "parent", "fromServer",
				       "transportType", "schedule", "options",
				       "enabledConnection", NULL };
	unsigned int i, valid_connections;
	struct GUID_list keep_connections;

	tmp_ctx = talloc_new(service);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	r_site_dn = ldb_dn_copy(tmp_ctx, r_bridgehead->dn);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(r_site_dn, tmp_ctx);

	ok = ldb_dn_remove_child_components(r_site_dn, 3);
	if (!ok) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	ret = dsdb_find_guid_by_dn(service->samdb, r_site_dn, &r_site_guid);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find objectGUID for object "
			  "%s: %s\n", ldb_dn_get_linearized(r_site_dn),
			  ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	l_site_dn = ldb_dn_copy(tmp_ctx, l_bridgehead->dn);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(l_site_dn, tmp_ctx);

	ok = ldb_dn_remove_child_components(l_site_dn, 3);
	if (!ok) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	ret = dsdb_find_guid_by_dn(service->samdb, l_site_dn, &l_site_guid);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find objectGUID for object "
			  "%s: %s\n", ldb_dn_get_linearized(l_site_dn),
			  ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = kcctpl_get_all_bridgehead_dcs(service, tmp_ctx,
					       r_site_guid, cross_ref,
					       transport, partial_replica_okay,
					       false, &r_bridgeheads_all);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to get all bridgehead DCs: "
			  "%s\n", nt_errstr(status)));
		return status;
	}

	status = kcctpl_get_all_bridgehead_dcs(service, tmp_ctx,
					       r_site_guid, cross_ref,
					       transport, partial_replica_okay,
					       detect_failed_dcs,
					       &r_bridgeheads_available);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to get all bridgehead DCs: "
			  "%s\n", nt_errstr(status)));
		return status;
	}

	status = kcctpl_get_all_bridgehead_dcs(service, tmp_ctx,
					       l_site_guid, cross_ref,
					       transport, partial_replica_okay,
					       false, &l_bridgeheads_all);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to get all bridgehead DCs: "
			  "%s\n", nt_errstr(status)));
		return status;
	}

	status = kcctpl_get_all_bridgehead_dcs(service, tmp_ctx,
					       l_site_guid, cross_ref,
					       transport, partial_replica_okay,
					       detect_failed_dcs,
					       &l_bridgeheads_available);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to get all bridgehead DCs: "
			  "%s\n", nt_errstr(status)));
		return status;
	}

	servers_dn = samdb_sites_dn(service->samdb, tmp_ctx);
	if (!servers_dn) {
		DEBUG(1, (__location__ ": failed to find our own Sites DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	ok = ldb_dn_add_child_fmt(servers_dn, "CN=Servers");
	if (!ok) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_search(service->samdb, tmp_ctx, &res, servers_dn, LDB_SCOPE_SUBTREE,
			 attrs, "objectClass=nTDSConnection");
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find nTDSConnection "
			  "objects: %s\n", ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	for (i = 0; i < res->count; i++) {
		struct ldb_message *connection;
		struct ldb_dn *parent_dn, *from_server;

		connection = res->msgs[i];

		parent_dn = ldb_dn_get_parent(tmp_ctx, connection->dn);
		if (!parent_dn) {
			DEBUG(1, (__location__ ": failed to get parent DN of "
				  "%s\n",
				  ldb_dn_get_linearized(connection->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		from_server = samdb_result_dn(service->samdb, tmp_ctx, connection,
					      "fromServer", NULL);
		if (!from_server) {
			DEBUG(1, (__location__ ": failed to find fromServer "
				  "attribute of object %s\n",
				  ldb_dn_get_linearized(connection->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (kcctpl_message_list_contains_dn(l_bridgeheads_all,
						    parent_dn) &&
		    kcctpl_message_list_contains_dn(r_bridgeheads_all,
						    from_server)) {
			uint32_t conn_opts;
			/* TODO: initialize conn_schedule from connection */
			uint8_t conn_schedule[84];
			struct ldb_dn *conn_transport_type;

			conn_opts = ldb_msg_find_attr_as_uint(connection,
							      "options", 0);

			conn_transport_type = samdb_result_dn(service->samdb, tmp_ctx,
							      connection,
							      "transportType",
							      NULL);
			if (!conn_transport_type) {
				DEBUG(1, (__location__ ": failed to find "
					  "transportType attribute of object "
					  "%s\n",
					  ldb_dn_get_linearized(connection->dn)));

				talloc_free(tmp_ctx);
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}

			if ((conn_opts & NTDSCONN_OPT_IS_GENERATED) &&
			    !(conn_opts & NTDSCONN_OPT_RODC_TOPOLOGY) &&
			    ldb_dn_compare(conn_transport_type,
					   transport->dn) == 0) {
				if (!(conn_opts & NTDSCONN_OPT_USER_OWNED_SCHEDULE) &&
				    memcmp(&conn_schedule, &schedule, 84) != 0) {
					/* TODO: perform an originating update
					   to set conn!schedule to schedule */
				}

				if ((conn_opts & NTDSCONN_OPT_OVERRIDE_NOTIFY_DEFAULT) &&
				    (conn_opts & NTDSCONN_OPT_USE_NOTIFY)) {
					if (!(repl_info.options & NTDSSITELINK_OPT_USE_NOTIFY)) {
						/* TODO: perform an originating
						   update to clear bits
						   NTDSCONN_OPT_OVERRIDE_NOTIFY_DEFAULT
						   and NTDSCONN_OPT_USE_NOTIFY
						   in conn!options */
					}
				} else if (repl_info.options & NTDSSITELINK_OPT_USE_NOTIFY) {
					/* TODO: perform an originating update
					   to set bits
					   NTDSCONN_OPT_OVERRIDE_NOTIFY_DEFAULT
					   and NTDSCONN_OPT_USE_NOTIFY in
					   conn!options */
				}

				if (conn_opts & NTDSCONN_OPT_TWOWAY_SYNC) {
				    if (!(repl_info.options & NTDSSITELINK_OPT_TWOWAY_SYNC)) {
					    /* TODO: perform an originating
					       update to clear bit
					       NTDSCONN_OPT_TWOWAY_SYNC in
					       conn!options. */
				    }
				} else if (repl_info.options & NTDSSITELINK_OPT_TWOWAY_SYNC) {
					/* TODO: perform an originating update
					   to set bit NTDSCONN_OPT_TWOWAY_SYNC
					   in conn!options. */
				}

				if (conn_opts & NTDSCONN_OPT_DISABLE_INTERSITE_COMPRESSION) {
					if (!(repl_info.options & NTDSSITELINK_OPT_DISABLE_COMPRESSION)) {
						/* TODO: perform an originating
						   update to clear bit
						   NTDSCONN_OPT_DISABLE_INTERSITE_COMPRESSION
						   in conn!options. */
					}
				} else if (repl_info.options & NTDSSITELINK_OPT_DISABLE_COMPRESSION) {
					/* TODO: perform an originating update
					   to set bit
					   NTDSCONN_OPT_DISABLE_INTERSITE_COMPRESSION
					   in conn!options. */
				}
			}
		}
	}

	ZERO_STRUCT(keep_connections);

	valid_connections = 0;
	for (i = 0; i < res->count; i++) {
		struct ldb_message *connection;
		struct ldb_dn *parent_dn, *from_server;

		connection = res->msgs[i];

		parent_dn = ldb_dn_get_parent(tmp_ctx, connection->dn);
		if (!parent_dn) {
			DEBUG(1, (__location__ ": failed to get parent DN of "
				  "%s\n",
				  ldb_dn_get_linearized(connection->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		from_server = samdb_result_dn(service->samdb, tmp_ctx, connection,
					      "fromServer", NULL);
		if (!from_server) {
			DEBUG(1, (__location__ ": failed to find fromServer "
				  "attribute of object %s\n",
				  ldb_dn_get_linearized(connection->dn)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (kcctpl_message_list_contains_dn(l_bridgeheads_all,
						    parent_dn) &&
		    kcctpl_message_list_contains_dn(r_bridgeheads_all,
						    from_server)) {
			uint32_t conn_opts;
			struct ldb_dn *conn_transport_type;

			conn_opts = ldb_msg_find_attr_as_uint(connection,
							      "options", 0);

			conn_transport_type = samdb_result_dn(service->samdb, tmp_ctx,
							      connection,
							      "transportType",
							      NULL);
			if (!conn_transport_type) {
				DEBUG(1, (__location__ ": failed to find "
					  "transportType attribute of object "
					  "%s\n",
					  ldb_dn_get_linearized(connection->dn)));

				talloc_free(tmp_ctx);
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}

			if ((!(conn_opts & NTDSCONN_OPT_IS_GENERATED) ||
			     ldb_dn_compare(conn_transport_type,
					    transport->dn) == 0) &&
			    !(conn_opts & NTDSCONN_OPT_RODC_TOPOLOGY)) {
				struct GUID r_guid, l_guid, conn_guid;
				bool failed_state_r, failed_state_l;

				ret = dsdb_find_guid_by_dn(service->samdb, from_server,
							   &r_guid);
				if (ret != LDB_SUCCESS) {
					DEBUG(1, (__location__ ": failed to "
						  "find GUID for object %s\n",
						  ldb_dn_get_linearized(from_server)));

					talloc_free(tmp_ctx);
					return NT_STATUS_INTERNAL_DB_CORRUPTION;
				}

				ret = dsdb_find_guid_by_dn(service->samdb, parent_dn,
							   &l_guid);
				if (ret != LDB_SUCCESS) {
					DEBUG(1, (__location__ ": failed to "
						  "find GUID for object %s\n",
						  ldb_dn_get_linearized(parent_dn)));

					talloc_free(tmp_ctx);
					return NT_STATUS_INTERNAL_DB_CORRUPTION;
				}

				status = kcctpl_bridgehead_dc_failed(service->samdb,
								     r_guid,
								     detect_failed_dcs,
								     &failed_state_r);
				if (NT_STATUS_IS_ERR(status)) {
					DEBUG(1, (__location__ ": failed to "
						  "check if bridgehead DC has "
						  "failed: %s\n",
						  nt_errstr(status)));

					talloc_free(tmp_ctx);
					return status;
				}

				status = kcctpl_bridgehead_dc_failed(service->samdb,
								     l_guid,
								     detect_failed_dcs,
								     &failed_state_l);
				if (NT_STATUS_IS_ERR(status)) {
					DEBUG(1, (__location__ ": failed to "
						  "check if bridgehead DC has "
						  "failed: %s\n",
						  nt_errstr(status)));

					talloc_free(tmp_ctx);
					return status;
				}

				if (!failed_state_r && !failed_state_l) {
					valid_connections++;
				}

				conn_guid = samdb_result_guid(connection,
							      "objectGUID");

				if (!kcctpl_guid_list_contains(keep_connections,
							       conn_guid)) {
					struct GUID *new_data;

					new_data = talloc_realloc(tmp_ctx,
								 keep_connections.data,
								 struct GUID,
								 keep_connections.count + 1);
					NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data,
									  tmp_ctx);
					new_data[keep_connections.count] = conn_guid;
					keep_connections.data = new_data;
					keep_connections.count++;
				}
			}
		}
	}

	if (valid_connections == 0) {
		uint64_t opts = NTDSCONN_OPT_IS_GENERATED;
		struct GUID new_guid, *new_data;

		if (repl_info.options & NTDSSITELINK_OPT_USE_NOTIFY) {
			opts |= NTDSCONN_OPT_OVERRIDE_NOTIFY_DEFAULT;
			opts |= NTDSCONN_OPT_USE_NOTIFY;
		}

		if (repl_info.options & NTDSSITELINK_OPT_TWOWAY_SYNC) {
			opts |= NTDSCONN_OPT_TWOWAY_SYNC;
		}

		if (repl_info.options & NTDSSITELINK_OPT_DISABLE_COMPRESSION) {
			opts |= NTDSCONN_OPT_DISABLE_INTERSITE_COMPRESSION;
		}

		/* perform an originating update to create a new nTDSConnection
		 * object cn that is:
		 *
		 * - child of l_bridgehead
		 * - cn!enabledConnection = true
		 * - cn!options = opts
		 * - cn!transportType = t
		 * - cn!fromServer = r_bridgehead
		 * - cn!schedule = schedule
		 */

		/* TODO: what should be the new connection's GUID? */
		new_guid = GUID_random();

		new_data = talloc_realloc(tmp_ctx, keep_connections.data,
					  struct GUID,
					  keep_connections.count + 1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(new_data, tmp_ctx);
		new_data[keep_connections.count] = new_guid;
		keep_connections.data = new_data;
		keep_connections.count++;
	}

	talloc_steal(mem_ctx, keep_connections.data);
	*_keep_connections = keep_connections;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * construct an NC replica graph for the NC identified by the given 'cross_ref',
 * then create any additional nTDSConnection objects required.
 */
static NTSTATUS kcctpl_create_connections(struct kccsrv_service *service,
					  TALLOC_CTX *mem_ctx,
					  struct kcctpl_graph *graph,
					  struct ldb_message *cross_ref,
					  bool detect_failed_dcs,
					  struct GUID_list keep_connections,
					  bool *_found_failed_dcs,
					  bool *_connected)
{
	bool connected, found_failed_dcs, partial_replica_okay;
	NTSTATUS status;
	struct ldb_message *site;
	TALLOC_CTX *tmp_ctx;
	struct GUID site_guid;
	struct kcctpl_vertex *site_vertex;
	uint32_t component_count, i;
	struct kcctpl_multi_edge_list st_edge_list;
	struct ldb_dn *transports_dn;
	const char * const attrs[] = { "bridgeheadServerListBL", "name",
				       "transportAddressAttribute", NULL };
	int ret;

	connected = true;

	status = kcctpl_color_vertices(service, graph, cross_ref, detect_failed_dcs,
				       &found_failed_dcs);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed to color vertices: %s\n",
			  nt_errstr(status)));

		return status;
	}

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	site = kcctpl_local_site(service->samdb, tmp_ctx);
	if (!site) {
		DEBUG(1, (__location__ ": failed to find our own local DC's "
			  "site\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	site_guid = samdb_result_guid(site, "objectGUID");

	site_vertex = kcctpl_find_vertex_by_guid(graph, site_guid);
	if (!site_vertex) {
		DEBUG(1, (__location__ ": failed to find vertex %s\n",
			  GUID_string(tmp_ctx, &site_guid)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (site_vertex->color == WHITE) {
		*_found_failed_dcs = found_failed_dcs;
		*_connected = true;
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	status = kcctpl_get_spanning_tree_edges(service, tmp_ctx, graph,
						&component_count,
						&st_edge_list);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, (__location__ ": failed get spanning tree edges: %s\n",
			  nt_errstr(status)));

		talloc_free(tmp_ctx);
		return status;
	}

	if (component_count > 1) {
		connected = false;
	}

	partial_replica_okay = (site_vertex->color == BLACK);

	transports_dn = kcctpl_transports_dn(service->samdb, tmp_ctx);
	if (!transports_dn) {
		DEBUG(1, (__location__ ": failed to find our own Inter-Site "
			  "Transports DN\n"));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	for (i = 0; i < st_edge_list.count; i++) {
		struct kcctpl_multi_edge *edge;
		struct GUID other_site_id;
		struct kcctpl_vertex *other_site_vertex;
		struct ldb_result *res;
		struct ldb_message *transport, *r_bridgehead, *l_bridgehead;
		uint8_t schedule[84];
		uint32_t first_available, j, interval;

		edge = &st_edge_list.data[i];

		if (edge->directed && !GUID_equal(&edge->vertex_ids.data[1],
						  &site_vertex->id)) {
			continue;
		}

		if (GUID_equal(&edge->vertex_ids.data[0], &site_vertex->id)) {
			other_site_id = edge->vertex_ids.data[1];
		} else {
			other_site_id = edge->vertex_ids.data[0];
		}

		other_site_vertex = kcctpl_find_vertex_by_guid(graph,
							       other_site_id);
		if (!other_site_vertex) {
			DEBUG(1, (__location__ ": failed to find vertex %s\n",
				  GUID_string(tmp_ctx, &other_site_id)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret = ldb_search(service->samdb, tmp_ctx, &res, transports_dn,
				 LDB_SCOPE_ONELEVEL, attrs,
				 "(&(objectClass=interSiteTransport)"
				 "(objectGUID=%s))", GUID_string(tmp_ctx,
								 &edge->type));
		if (ret != LDB_SUCCESS) {
			DEBUG(1, (__location__ ": failed to find "
				  "interSiteTransport object %s: %s\n",
				  GUID_string(tmp_ctx, &edge->type),
				  ldb_strerror(ret)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		if (res->count == 0) {
			DEBUG(1, (__location__ ": failed to find "
				  "interSiteTransport object %s\n",
				  GUID_string(tmp_ctx, &edge->type)));

			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		transport = res->msgs[0];

		status = kcctpl_get_bridgehead_dc(service, tmp_ctx,
						  other_site_vertex->id,
						  cross_ref, transport,
						  partial_replica_okay,
						  detect_failed_dcs,
						  &r_bridgehead);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to get a bridgehead "
				  "DC: %s\n", nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}

		if (service->am_rodc) {
			/* TODO: l_bridgehad = nTDSDSA of local DC */
		} else {
			status = kcctpl_get_bridgehead_dc(service, tmp_ctx,
							  site_vertex->id,
							  cross_ref, transport,
							  partial_replica_okay,
							  detect_failed_dcs,
							  &l_bridgehead);
			if (NT_STATUS_IS_ERR(status)) {
				DEBUG(1, (__location__ ": failed to get a "
					  "bridgehead DC: %s\n",
					  nt_errstr(status)));

				talloc_free(tmp_ctx);
				return status;
			}
		}

		ZERO_ARRAY(schedule);
		first_available = 84;
		interval = edge->repl_info.interval / 15;
		for (j = 0; j < 84; j++) {
			if (edge->repl_info.schedule[j] == 1) {
				first_available = j;
				break;
			}
		}
		for (j = first_available; j < 84; j += interval) {
			schedule[j] = 1;
		}

		status = kcctpl_create_connection(service, mem_ctx, cross_ref,
						  r_bridgehead, transport,
						  l_bridgehead, edge->repl_info,
						  schedule, detect_failed_dcs,
						  partial_replica_okay,
						  &keep_connections);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to create a "
				  "connection: %s\n", nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}
	}

	*_found_failed_dcs = found_failed_dcs;
	*_connected = connected;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * computes an NC replica graph for each NC replica that "should be present" on
 * the local DC or "is present" on any DC in the same site as the local DC. for
 * each edge directed to an NC replica on such a DC from an NC replica on a DC
 * in another site, the KCC creates and nTDSConnection object to imply that edge
 * if one does not already exist.
 */
static NTSTATUS kcctpl_create_intersite_connections(struct kccsrv_service *service,
						    TALLOC_CTX *mem_ctx,
						    struct GUID_list *_keep_connections,
						    bool *_all_connected)
{
	struct GUID_list keep_connections;
	bool all_connected;
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *partitions_dn;
	struct ldb_result *res;
	const char * const attrs[] = { "enabled", "systemFlags", "nCName",
					NULL };
	int ret;
	unsigned int i;

	all_connected = true;

	ZERO_STRUCT(keep_connections);

	tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	partitions_dn = samdb_partitions_dn(service->samdb, tmp_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(partitions_dn, tmp_ctx);

	ret = ldb_search(service->samdb, tmp_ctx, &res, partitions_dn, LDB_SCOPE_ONELEVEL,
			 attrs, "objectClass=crossRef");
	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": failed to find crossRef objects: "
			  "%s\n", ldb_strerror(ret)));

		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	for (i = 0; i < res->count; i++) {
		struct ldb_message *cross_ref;
		unsigned int cr_enabled;
		int64_t cr_flags;
		struct kcctpl_graph *graph;
		bool found_failed_dc, connected;
		NTSTATUS status;

		cross_ref = res->msgs[i];
		cr_enabled = ldb_msg_find_attr_as_uint(cross_ref, "enabled", -1);
		cr_flags = ldb_msg_find_attr_as_int64(cross_ref, "systemFlags", 0);
		if ((cr_enabled == 0) || !(cr_flags & FLAG_CR_NTDS_NC)) {
			continue;
		}

		status = kcctpl_setup_graph(service->samdb, tmp_ctx, &graph);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to create a graph: "
				  "%s\n", nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}

		status = kcctpl_create_connections(service, mem_ctx, graph,
						   cross_ref, true,
						   keep_connections,
						   &found_failed_dc,
						   &connected);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(1, (__location__ ": failed to create "
				  "connections: %s\n", nt_errstr(status)));

			talloc_free(tmp_ctx);
			return status;
		}

		if (!connected) {
			all_connected = false;

			if (found_failed_dc) {
				status = kcctpl_create_connections(service, mem_ctx,
								   graph,
								   cross_ref,
								   true,
								   keep_connections,
								   &found_failed_dc,
								   &connected);
				if (NT_STATUS_IS_ERR(status)) {
					DEBUG(1, (__location__ ": failed to "
						  "create connections: %s\n",
						  nt_errstr(status)));

					talloc_free(tmp_ctx);
					return status;
				}
			}
		}
	}

	*_keep_connections = keep_connections;
	*_all_connected = all_connected;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

NTSTATUS kcctpl_test(struct kccsrv_service *service)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(service);
	struct GUID_list keep;
	bool all_connected;

	DEBUG(5, ("Testing kcctpl_create_intersite_connections\n"));
	status = kcctpl_create_intersite_connections(service, tmp_ctx, &keep,
						     &all_connected);
	DEBUG(4, ("%s\n", nt_errstr(status)));
	if (NT_STATUS_IS_OK(status)) {
		uint32_t i;

		DEBUG(4, ("all_connected=%d, %d GUIDs returned\n",
			  all_connected, keep.count));

		for (i = 0; i < keep.count; i++) {
			DEBUG(4, ("GUID %d: %s\n", i + 1,
				  GUID_string(tmp_ctx, &keep.data[i])));
		}
	}

	talloc_free(tmp_ctx);
	return status;
}

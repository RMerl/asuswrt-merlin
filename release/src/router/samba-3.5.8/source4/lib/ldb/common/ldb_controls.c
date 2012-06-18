/* 
   ldb database library

   Copyright (C) Simo Sorce  2005

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: ldb_controls.c
 *
 *  Component: ldb controls utility functions
 *
 *  Description: helper functions for control modules
 *
 *  Author: Simo Sorce
 */

#include "ldb_private.h"

/* check if a control with the specified "oid" exist and return it */
/* returns NULL if not found */
struct ldb_control *ldb_request_get_control(struct ldb_request *req, const char *oid)
{
	int i;

	if (req->controls != NULL) {
		for (i = 0; req->controls[i]; i++) {
			if (strcmp(oid, req->controls[i]->oid) == 0) {
				break;
			}
		}

		return req->controls[i];
	}

	return NULL;
}

/* check if a control with the specified "oid" exist and return it */
/* returns NULL if not found */
struct ldb_control *ldb_reply_get_control(struct ldb_reply *rep, const char *oid)
{
	int i;

	if (rep->controls != NULL) {
		for (i = 0; rep->controls[i]; i++) {
			if (strcmp(oid, rep->controls[i]->oid) == 0) {
				break;
			}
		}

		return rep->controls[i];
	}

	return NULL;
}

/* saves the current controls list into the "saver" and replace the one in req with a new one excluding
the "exclude" control */
/* returns 0 on error */
int save_controls(struct ldb_control *exclude, struct ldb_request *req, struct ldb_control ***saver)
{
	struct ldb_control **lcs;
	int i, j;

	*saver = req->controls;
	for (i = 0; req->controls[i]; i++);
	if (i == 1) {
		req->controls = NULL;
		return 1;
	}

	lcs = talloc_array(req, struct ldb_control *, i);
	if (!lcs) {
		return 0;
	}

	for (i = 0, j = 0; (*saver)[i]; i++) {
		if (exclude == (*saver)[i]) continue;
		lcs[j] = (*saver)[i];
		j++;
	}
	lcs[j] = NULL;

	req->controls = lcs;
	return 1;
}

/* check if there's any control marked as critical in the list */
/* return True if any, False if none */
int check_critical_controls(struct ldb_control **controls)
{
	int i;

	if (controls == NULL) {
		return 0;
	}

	for (i = 0; controls[i]; i++) {
		if (controls[i]->critical) {
			return 1;
		}
	}

	return 0;
}

int ldb_request_add_control(struct ldb_request *req, const char *oid, bool critical, void *data)
{
	unsigned n;
	struct ldb_control **ctrls;
	struct ldb_control *ctrl;

	for (n=0; req->controls && req->controls[n];) { 
		/* having two controls of the same OID makes no sense */
		if (strcmp(oid, req->controls[n]->oid) == 0) {
			return LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
		}
		n++; 
	}

	ctrls = talloc_realloc(req, req->controls,
			       struct ldb_control *,
			       n + 2);
	if (!ctrls) return LDB_ERR_OPERATIONS_ERROR;
	req->controls = ctrls;
	ctrls[n] = NULL;
	ctrls[n+1] = NULL;

	ctrl = talloc(ctrls, struct ldb_control);
	if (!ctrl) return LDB_ERR_OPERATIONS_ERROR;

	ctrl->oid	= talloc_strdup(ctrl, oid);
	if (!ctrl->oid) return LDB_ERR_OPERATIONS_ERROR;
	ctrl->critical	= critical;
	ctrl->data	= data;

	ctrls[n] = ctrl;
	return LDB_SUCCESS;
}

/* Parse controls from the format used on the command line and in ejs */

struct ldb_control **ldb_parse_control_strings(struct ldb_context *ldb, void *mem_ctx, const char **control_strings)
{
	int i;
	struct ldb_control **ctrl;

	char *error_string = NULL;

	if (control_strings == NULL || control_strings[0] == NULL)
		return NULL;

	for (i = 0; control_strings[i]; i++);

	ctrl = talloc_array(mem_ctx, struct ldb_control *, i + 1);

	for (i = 0; control_strings[i]; i++) {
		if (strncmp(control_strings[i], "vlv:", 4) == 0) {
			struct ldb_vlv_req_control *control;
			const char *p;
			char attr[1024];
			char ctxid[1024];
			int crit, bc, ac, os, cc, ret;

			attr[0] = '\0';
			ctxid[0] = '\0';
			p = &(control_strings[i][4]);
			ret = sscanf(p, "%d:%d:%d:%d:%d:%1023[^$]", &crit, &bc, &ac, &os, &cc, ctxid);
			if (ret < 5) {
				ret = sscanf(p, "%d:%d:%d:%1023[^:]:%1023[^$]", &crit, &bc, &ac, attr, ctxid);
			}
			       
			if ((ret < 4) || (crit < 0) || (crit > 1)) {
				error_string = talloc_asprintf(mem_ctx, "invalid server_sort control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b):bc(n):ac(n):<os(n):cc(n)|attr(s)>[:ctxid(o)]\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean, n = number, s = string, o = b64 binary blob");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}
			if (!(ctrl[i] = talloc(ctrl, struct ldb_control))) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_VLV_REQ_OID;
			ctrl[i]->critical = crit;
			if (!(control = talloc(ctrl[i],
					       struct ldb_vlv_req_control))) {
				ldb_oom(ldb);
				return NULL;
			}
			control->beforeCount = bc;
			control->afterCount = ac;
			if (attr[0]) {
				control->type = 1;
				control->match.gtOrEq.value = talloc_strdup(control, attr);
				control->match.gtOrEq.value_len = strlen(attr);
			} else {
				control->type = 0;
				control->match.byOffset.offset = os;
				control->match.byOffset.contentCount = cc;
			}
			if (ctxid[0]) {
				control->ctxid_len = ldb_base64_decode(ctxid);
				control->contextId = (char *)talloc_memdup(control, ctxid, control->ctxid_len);
			} else {
				control->ctxid_len = 0;
				control->contextId = NULL;
			}
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "dirsync:", 8) == 0) {
			struct ldb_dirsync_control *control;
			const char *p;
			char cookie[1024];
			int crit, flags, max_attrs, ret;
		       
			cookie[0] = '\0';
			p = &(control_strings[i][8]);
			ret = sscanf(p, "%d:%d:%d:%1023[^$]", &crit, &flags, &max_attrs, cookie);

			if ((ret < 3) || (crit < 0) || (crit > 1) || (flags < 0) || (max_attrs < 0)) {
				error_string = talloc_asprintf(mem_ctx, "invalid dirsync control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b):flags(n):max_attrs(n)[:cookie(o)]\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean, n = number, o = b64 binary blob");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			/* w2k3 seems to ignore the parameter,
			 * but w2k sends a wrong cookie when this value is to small
			 * this would cause looping forever, while getting
			 * the same data and same cookie forever
			 */
			if (max_attrs == 0) max_attrs = 0x0FFFFFFF;

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_DIRSYNC_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_dirsync_control);
			control->flags = flags;
			control->max_attributes = max_attrs;
			if (*cookie) {
				control->cookie_len = ldb_base64_decode(cookie);
				control->cookie = (char *)talloc_memdup(control, cookie, control->cookie_len);
			} else {
				control->cookie = NULL;
				control->cookie_len = 0;
			}
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "asq:", 4) == 0) {
			struct ldb_asq_control *control;
			const char *p;
			char attr[256];
			int crit, ret;

			attr[0] = '\0';
			p = &(control_strings[i][4]);
			ret = sscanf(p, "%d:%255[^$]", &crit, attr);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (attr[0] == '\0')) {
				error_string = talloc_asprintf(mem_ctx, "invalid asq control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b):attr(s)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean, s = string");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_ASQ_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_asq_control);
			control->request = 1;
			control->source_attribute = talloc_strdup(control, attr);
			control->src_attr_len = strlen(attr);
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "extended_dn:", 12) == 0) {
			struct ldb_extended_dn_control *control;
			const char *p;
			int crit, type, ret;

			p = &(control_strings[i][12]);
			ret = sscanf(p, "%d:%d", &crit, &type);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (type < 0) || (type > 1)) {
				ret = sscanf(p, "%d", &crit);
				if ((ret != 1) || (crit < 0) || (crit > 1)) {
					error_string = talloc_asprintf(mem_ctx, "invalid extended_dn control syntax\n");
					error_string = talloc_asprintf_append(error_string, " syntax: crit(b)[:type(i)]\n");
					error_string = talloc_asprintf_append(error_string, "   note: b = boolean\n");
					error_string = talloc_asprintf_append(error_string, "         i = integer\n");
					error_string = talloc_asprintf_append(error_string, "   valid values are: 0 - hexadecimal representation\n");
					error_string = talloc_asprintf_append(error_string, "                     1 - normal string representation");
					ldb_set_errstring(ldb, error_string);
					talloc_free(error_string);
					return NULL;
				}
				control = NULL;
			} else {
				control = talloc(ctrl, struct ldb_extended_dn_control);
				control->type = type;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_EXTENDED_DN_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = talloc_steal(ctrl[i], control);

			continue;
		}

		if (strncmp(control_strings[i], "sd_flags:", 9) == 0) {
			struct ldb_sd_flags_control *control;
			const char *p;
			int crit, ret;
			unsigned secinfo_flags;

			p = &(control_strings[i][9]);
			ret = sscanf(p, "%d:%u", &crit, &secinfo_flags);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (secinfo_flags < 0) || (secinfo_flags > 0xF)) {
				error_string = talloc_asprintf(mem_ctx, "invalid sd_flags control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b):secinfo_flags(n)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean, n = number");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_SD_FLAGS_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_sd_flags_control);
			control->secinfo_flags = secinfo_flags;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "search_options:", 15) == 0) {
			struct ldb_search_options_control *control;
			const char *p;
			int crit, ret;
			unsigned search_options;

			p = &(control_strings[i][15]);
			ret = sscanf(p, "%d:%u", &crit, &search_options);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (search_options < 0) || (search_options > 0xF)) {
				error_string = talloc_asprintf(mem_ctx, "invalid search_options control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b):search_options(n)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean, n = number");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_SEARCH_OPTIONS_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_search_options_control);
			control->search_options = search_options;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "domain_scope:", 13) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][13]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				error_string = talloc_asprintf(mem_ctx, "invalid domain_scope control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_DOMAIN_SCOPE_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "paged_results:", 14) == 0) {
			struct ldb_paged_control *control;
			const char *p;
			int crit, size, ret;
		       
			p = &(control_strings[i][14]);
			ret = sscanf(p, "%d:%d", &crit, &size);

			if ((ret != 2) || (crit < 0) || (crit > 1) || (size < 0)) {
				error_string = talloc_asprintf(mem_ctx, "invalid paged_results control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b):size(n)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean, n = number");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_PAGED_RESULTS_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_paged_control);
			control->size = size;
			control->cookie = NULL;
			control->cookie_len = 0;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "server_sort:", 12) == 0) {
			struct ldb_server_sort_control **control;
			const char *p;
			char attr[256];
			char rule[128];
			int crit, rev, ret;

			attr[0] = '\0';
			rule[0] = '\0';
			p = &(control_strings[i][12]);
			ret = sscanf(p, "%d:%d:%255[^:]:%127[^:]", &crit, &rev, attr, rule);
			if ((ret < 3) || (crit < 0) || (crit > 1) || (rev < 0 ) || (rev > 1) ||attr[0] == '\0') {
				error_string = talloc_asprintf(mem_ctx, "invalid server_sort control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b):rev(b):attr(s)[:rule(s)]\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean, s = string");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}
			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_SERVER_SORT_OID;
			ctrl[i]->critical = crit;
			control = talloc_array(ctrl[i], struct ldb_server_sort_control *, 2);
			control[0] = talloc(control, struct ldb_server_sort_control);
			control[0]->attributeName = talloc_strdup(control, attr);
			if (rule[0])
				control[0]->orderingRule = talloc_strdup(control, rule);
			else
				control[0]->orderingRule = NULL;
			control[0]->reverse = rev;
			control[1] = NULL;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "notification:", 13) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][13]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				error_string = talloc_asprintf(mem_ctx, "invalid notification control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_NOTIFICATION_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "show_deleted:", 13) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][13]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				error_string = talloc_asprintf(mem_ctx, "invalid show_deleted control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_SHOW_DELETED_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "show_deactivated_link:", 22) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][22]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				error_string = talloc_asprintf(mem_ctx, "invalid show_deactivated_link control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_SHOW_DEACTIVATED_LINK_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "show_recycled:", 14) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][14]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				error_string = talloc_asprintf(mem_ctx, "invalid show_recycled control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_SHOW_RECYCLED_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "permissive_modify:", 18) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][18]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				error_string = talloc_asprintf(mem_ctx, "invalid permissive_modify control syntax\n");
				error_string = talloc_asprintf_append(error_string, " syntax: crit(b)\n");
				error_string = talloc_asprintf_append(error_string, "   note: b = boolean");
				ldb_set_errstring(ldb, error_string);
				talloc_free(error_string);
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			if (!ctrl[i]) {
				ldb_oom(ldb);
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_PERMISSIVE_MODIFY_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		/* no controls matched, throw an error */
		ldb_asprintf_errstring(ldb, "Invalid control name: '%s'", control_strings[i]);
		return NULL;
	}

	ctrl[i] = NULL;

	return ctrl;
}



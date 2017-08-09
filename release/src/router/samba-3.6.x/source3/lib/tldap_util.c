/*
   Unix SMB/CIFS implementation.
   Infrastructure for async ldap client requests
   Copyright (C) Volker Lendecke 2009

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
#include "tldap.h"
#include "tldap_util.h"
#include "../libcli/security/security.h"
#include "../lib/util/asn1.h"
#include "../librpc/ndr/libndr.h"

bool tldap_entry_values(struct tldap_message *msg, const char *attribute,
			DATA_BLOB **values, int *num_values)
{
	struct tldap_attribute *attributes;
	int i, num_attributes;

	if (!tldap_entry_attributes(msg, &attributes, &num_attributes)) {
		return false;
	}

	for (i=0; i<num_attributes; i++) {
		if (strequal(attribute, attributes[i].name)) {
			break;
		}
	}
	if (i == num_attributes) {
		return false;
	}
	*num_values = attributes[i].num_values;
	*values = attributes[i].values;
	return true;
}

bool tldap_get_single_valueblob(struct tldap_message *msg,
				const char *attribute, DATA_BLOB *blob)
{
	int num_values;
	DATA_BLOB *values;

	if (attribute == NULL) {
		return NULL;
	}
	if (!tldap_entry_values(msg, attribute, &values, &num_values)) {
		return NULL;
	}
	if (num_values != 1) {
		return NULL;
	}
	*blob = values[0];
	return true;
}

char *tldap_talloc_single_attribute(struct tldap_message *msg,
				    const char *attribute,
				    TALLOC_CTX *mem_ctx)
{
	DATA_BLOB val;
	char *result;
	size_t len;

	if (!tldap_get_single_valueblob(msg, attribute, &val)) {
		return false;
	}
	if (!convert_string_talloc(mem_ctx, CH_UTF8, CH_UNIX,
				   val.data, val.length,
				   &result, &len, false)) {
		return NULL;
	}
	return result;
}

bool tldap_pull_binsid(struct tldap_message *msg, const char *attribute,
		       struct dom_sid *sid)
{
	DATA_BLOB val;

	if (!tldap_get_single_valueblob(msg, attribute, &val)) {
		return false;
	}
	return sid_parse((char *)val.data, val.length, sid);
}

bool tldap_pull_guid(struct tldap_message *msg, const char *attribute,
		     struct GUID *guid)
{
	DATA_BLOB val;

	if (!tldap_get_single_valueblob(msg, attribute, &val)) {
		return false;
	}
	return NT_STATUS_IS_OK(GUID_from_data_blob(&val, guid));
}

static bool tldap_add_blob_vals(TALLOC_CTX *mem_ctx, struct tldap_mod *mod,
				DATA_BLOB *newvals, int num_newvals)
{
	int num_values = talloc_array_length(mod->values);
	int i;
	DATA_BLOB *tmp;

	tmp = talloc_realloc(mem_ctx, mod->values, DATA_BLOB,
			     num_values + num_newvals);
	if (tmp == NULL) {
		return false;
	}
	mod->values = tmp;

	for (i=0; i<num_newvals; i++) {
		mod->values[i+num_values].data = (uint8_t *)talloc_memdup(
			mod->values, newvals[i].data, newvals[i].length);
		if (mod->values[i+num_values].data == NULL) {
			return false;
		}
		mod->values[i+num_values].length = newvals[i].length;
	}
	mod->num_values = num_values + num_newvals;
	return true;
}

bool tldap_add_mod_blobs(TALLOC_CTX *mem_ctx,
			 struct tldap_mod **pmods, int *pnum_mods,
			 int mod_op, const char *attrib,
			 DATA_BLOB *newvals, int num_newvals)
{
	struct tldap_mod new_mod;
	struct tldap_mod *mods = *pmods;
	struct tldap_mod *mod = NULL;
	int i, num_mods;

	if (mods == NULL) {
		mods = talloc_array(mem_ctx, struct tldap_mod, 0);
	}
	if (mods == NULL) {
		return false;
	}

	num_mods = *pnum_mods;

	for (i=0; i<num_mods; i++) {
		if ((mods[i].mod_op == mod_op)
		    && strequal(mods[i].attribute, attrib)) {
			mod = &mods[i];
			break;
		}
	}

	if (mod == NULL) {
		new_mod.mod_op = mod_op;
		new_mod.attribute = talloc_strdup(mods, attrib);
		if (new_mod.attribute == NULL) {
			return false;
		}
		new_mod.num_values = 0;
		new_mod.values = NULL;
		mod = &new_mod;
	}

	if ((num_newvals != 0)
	    && !tldap_add_blob_vals(mods, mod, newvals, num_newvals)) {
		return false;
	}

	if ((i == num_mods) && (talloc_array_length(mods) < num_mods + 1)) {
		mods = talloc_realloc(talloc_tos(), mods, struct tldap_mod,
				      num_mods+1);
		if (mods == NULL) {
			return false;
		}
		mods[num_mods] = *mod;
	}

	*pmods = mods;
	*pnum_mods += 1;
	return true;
}

bool tldap_add_mod_str(TALLOC_CTX *mem_ctx,
		       struct tldap_mod **pmods, int *pnum_mods,
		       int mod_op, const char *attrib, const char *str)
{
	DATA_BLOB utf8;
	bool ret;

	if (!convert_string_talloc(talloc_tos(), CH_UNIX, CH_UTF8, str,
				   strlen(str), &utf8.data, &utf8.length,
				   false)) {
		return false;
	}

	ret = tldap_add_mod_blobs(mem_ctx, pmods, pnum_mods, mod_op, attrib,
				  &utf8, 1);
	TALLOC_FREE(utf8.data);
	return ret;
}

static bool tldap_make_mod_blob_int(struct tldap_message *existing,
				    TALLOC_CTX *mem_ctx,
				    struct tldap_mod **pmods, int *pnum_mods,
				    const char *attrib, DATA_BLOB newval,
				    int (*comparison)(const DATA_BLOB *d1,
						      const DATA_BLOB *d2))
{
	int num_values = 0;
	DATA_BLOB *values = NULL;
	DATA_BLOB oldval = data_blob_null;

	if ((existing != NULL)
	    && tldap_entry_values(existing, attrib, &values, &num_values)) {

		if (num_values > 1) {
			/* can't change multivalue attributes atm */
			return false;
		}
		if (num_values == 1) {
			oldval = values[0];
		}
	}

	if ((oldval.data != NULL) && (newval.data != NULL)
	    && (comparison(&oldval, &newval) == 0)) {
		/* Believe it or not, but LDAP will deny a delete and
		   an add at the same time if the values are the
		   same... */
		DEBUG(10,("tldap_make_mod_blob_int: attribute |%s| not "
			  "changed.\n", attrib));
		return true;
	}

	if (oldval.data != NULL) {
		/* By deleting exactly the value we found in the entry this
		 * should be race-free in the sense that the LDAP-Server will
		 * deny the complete operation if somebody changed the
		 * attribute behind our back. */
		/* This will also allow modifying single valued attributes in
		 * Novell NDS. In NDS you have to first remove attribute and
		 * then you could add new value */

		DEBUG(10, ("tldap_make_mod_blob_int: deleting attribute |%s|\n",
			   attrib));
		if (!tldap_add_mod_blobs(mem_ctx, pmods, pnum_mods,
					 TLDAP_MOD_DELETE,
					 attrib, &oldval, 1)) {
			return false;
		}
	}

	/* Regardless of the real operation (add or modify)
	   we add the new value here. We rely on deleting
	   the old value, should it exist. */

	if (newval.data != NULL) {
		DEBUG(10, ("tldap_make_mod_blob_int: adding attribute |%s| value len "
			   "%d\n", attrib, (int)newval.length));
	        if (!tldap_add_mod_blobs(mem_ctx, pmods, pnum_mods,
					 TLDAP_MOD_ADD,
					 attrib, &newval, 1)) {
			return false;
		}
	}
	return true;
}

bool tldap_make_mod_blob(struct tldap_message *existing, TALLOC_CTX *mem_ctx,
			 struct tldap_mod **pmods, int *pnum_mods,
			 const char *attrib, DATA_BLOB newval)
{
	return tldap_make_mod_blob_int(existing, mem_ctx, pmods, pnum_mods,
				       attrib, newval, data_blob_cmp);
}

static int compare_utf8_blobs(const DATA_BLOB *d1, const DATA_BLOB *d2)
{
	char *s1, *s2;
	size_t s1len, s2len;
	int ret;

	if (!convert_string_talloc(talloc_tos(), CH_UTF8, CH_UNIX, d1->data,
				   d1->length, &s1, &s1len, false)) {
		/* can't do much here */
		return 0;
	}
	if (!convert_string_talloc(talloc_tos(), CH_UTF8, CH_UNIX, d2->data,
				   d2->length, &s2, &s2len, false)) {
		/* can't do much here */
		TALLOC_FREE(s1);
		return 0;
	}
	ret = StrCaseCmp(s1, s2);
	TALLOC_FREE(s2);
	TALLOC_FREE(s1);
	return ret;
}

bool tldap_make_mod_fmt(struct tldap_message *existing, TALLOC_CTX *mem_ctx,
			struct tldap_mod **pmods, int *pnum_mods,
			const char *attrib, const char *fmt, ...)
{
	va_list ap;
	char *newval;
	bool ret;
	DATA_BLOB blob = data_blob_null;

	va_start(ap, fmt);
	newval = talloc_vasprintf(talloc_tos(), fmt, ap);
	va_end(ap);

	if (newval == NULL) {
		return false;
	}

	blob.length = strlen(newval);
	if (blob.length != 0) {
		blob.data = CONST_DISCARD(uint8_t *, newval);
	}
	ret = tldap_make_mod_blob_int(existing, mem_ctx, pmods, pnum_mods,
				      attrib, blob, compare_utf8_blobs);
	TALLOC_FREE(newval);
	return ret;
}

const char *tldap_errstr(TALLOC_CTX *mem_ctx, struct tldap_context *ld, int rc)
{
	const char *ld_error = NULL;
	char *res;

	if (ld != NULL) {
		ld_error = tldap_msg_diagnosticmessage(tldap_ctx_lastmsg(ld));
	}
	res = talloc_asprintf(mem_ctx, "LDAP error %d (%s), %s", rc,
			      tldap_err2string(rc),
			      ld_error ? ld_error : "unknown");
	return res;
}

int tldap_search_va(struct tldap_context *ld, const char *base, int scope,
		    const char *attrs[], int num_attrs, int attrsonly,
		    TALLOC_CTX *mem_ctx, struct tldap_message ***res,
		    const char *fmt, va_list ap)
{
	char *filter;
	int ret;

	filter = talloc_vasprintf(talloc_tos(), fmt, ap);
	if (filter == NULL) {
		return TLDAP_NO_MEMORY;
	}

	ret = tldap_search(ld, base, scope, filter,
			   attrs, num_attrs, attrsonly,
			   NULL /*sctrls*/, 0, NULL /*cctrls*/, 0,
			   0 /*timelimit*/, 0 /*sizelimit*/, 0 /*deref*/,
			   mem_ctx, res, NULL);
	TALLOC_FREE(filter);
	return ret;
}

int tldap_search_fmt(struct tldap_context *ld, const char *base, int scope,
		     const char *attrs[], int num_attrs, int attrsonly,
		     TALLOC_CTX *mem_ctx, struct tldap_message ***res,
		     const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = tldap_search_va(ld, base, scope, attrs, num_attrs, attrsonly,
			      mem_ctx, res, fmt, ap);
	va_end(ap);
	return ret;
}

bool tldap_pull_uint64(struct tldap_message *msg, const char *attr,
		       uint64_t *presult)
{
	char *str;
	uint64_t result;

	str = tldap_talloc_single_attribute(msg, attr, talloc_tos());
	if (str == NULL) {
		DEBUG(10, ("Could not find attribute %s\n", attr));
		return false;
	}
	result = strtoull(str, NULL, 10);
	TALLOC_FREE(str);
	*presult = result;
	return true;
}

bool tldap_pull_uint32(struct tldap_message *msg, const char *attr,
		       uint32_t *presult)
{
	uint64_t result;

	if (!tldap_pull_uint64(msg, attr, &result)) {
		return false;
	}
	*presult = (uint32_t)result;
	return true;
}

struct tldap_fetch_rootdse_state {
	struct tldap_context *ld;
	struct tldap_message *rootdse;
};

static void tldap_fetch_rootdse_done(struct tevent_req *subreq);

struct tevent_req *tldap_fetch_rootdse_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct tldap_context *ld)
{
	struct tevent_req *req, *subreq;
	struct tldap_fetch_rootdse_state *state;
	static const char *attrs[2] = { "*", "+" };

	req = tevent_req_create(mem_ctx, &state,
				struct tldap_fetch_rootdse_state);
	if (req == NULL) {
		return NULL;
	}
	state->ld = ld;
	state->rootdse = NULL;

	subreq = tldap_search_send(
		mem_ctx, ev, ld, "", TLDAP_SCOPE_BASE, "(objectclass=*)",
		attrs, ARRAY_SIZE(attrs), 0, NULL, 0, NULL, 0, 0, 0, 0);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_fetch_rootdse_done, req);
	return req;
}

static void tldap_fetch_rootdse_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct tldap_fetch_rootdse_state *state = tevent_req_data(
		req, struct tldap_fetch_rootdse_state);
	struct tldap_message *msg;
	int rc;

	rc = tldap_search_recv(subreq, state, &msg);
	if (rc != TLDAP_SUCCESS) {
		TALLOC_FREE(subreq);
		tevent_req_error(req, rc);
		return;
	}

	switch (tldap_msg_type(msg)) {
	case TLDAP_RES_SEARCH_ENTRY:
		if (state->rootdse != NULL) {
			goto protocol_error;
		}
		state->rootdse = msg;
		break;
	case TLDAP_RES_SEARCH_RESULT:
		TALLOC_FREE(subreq);
		if (state->rootdse == NULL) {
			goto protocol_error;
		}
		tevent_req_done(req);
		break;
	default:
		goto protocol_error;
	}
	return;

protocol_error:
	tevent_req_error(req, TLDAP_PROTOCOL_ERROR);
	return;
}

int tldap_fetch_rootdse_recv(struct tevent_req *req)
{
	struct tldap_fetch_rootdse_state *state = tevent_req_data(
		req, struct tldap_fetch_rootdse_state);
	int err;
	char *dn;

	if (tevent_req_is_ldap_error(req, &err)) {
		return err;
	}
	/* Trigger parsing the dn, just to make sure it's ok */
	if (!tldap_entry_dn(state->rootdse, &dn)) {
		return TLDAP_DECODING_ERROR;
	}
	if (!tldap_context_setattr(state->ld, "tldap:rootdse",
				   &state->rootdse)) {
		return TLDAP_NO_MEMORY;
	}
	return 0;
}

int tldap_fetch_rootdse(struct tldap_context *ld)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	int result;

	ev = event_context_init(frame);
	if (ev == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	req = tldap_fetch_rootdse_send(frame, ev, ld);
	if (req == NULL) {
		result = TLDAP_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		result = TLDAP_OPERATIONS_ERROR;
		goto fail;
	}

	result = tldap_fetch_rootdse_recv(req);
 fail:
	TALLOC_FREE(frame);
	return result;
}

struct tldap_message *tldap_rootdse(struct tldap_context *ld)
{
	return talloc_get_type(tldap_context_getattr(ld, "tldap:rootdse"),
			       struct tldap_message);
}

bool tldap_entry_has_attrvalue(struct tldap_message *msg,
			       const char *attribute,
			       const DATA_BLOB blob)
{
	int i, num_values;
	DATA_BLOB *values;

	if (!tldap_entry_values(msg, attribute, &values, &num_values)) {
		return false;
	}
	for (i=0; i<num_values; i++) {
		if (data_blob_cmp(&values[i], &blob) == 0) {
			return true;
		}
	}
	return false;
}

bool tldap_supports_control(struct tldap_context *ld, const char *oid)
{
	struct tldap_message *rootdse = tldap_rootdse(ld);

	if (rootdse == NULL) {
		return false;
	}
	return tldap_entry_has_attrvalue(rootdse, "supportedControl",
					 data_blob_const(oid, strlen(oid)));
}

struct tldap_control *tldap_add_control(TALLOC_CTX *mem_ctx,
					struct tldap_control *ctrls,
					int num_ctrls,
					struct tldap_control *ctrl)
{
	struct tldap_control *result;

	result = talloc_array(mem_ctx, struct tldap_control, num_ctrls+1);
	if (result == NULL) {
		return NULL;
	}
	memcpy(result, ctrls, sizeof(struct tldap_control) * num_ctrls);
	result[num_ctrls] = *ctrl;
	return result;
}

/*
 * Find a control returned by the server
 */
struct tldap_control *tldap_msg_findcontrol(struct tldap_message *msg,
					    const char *oid)
{
	struct tldap_control *controls;
	int i, num_controls;

	tldap_msg_sctrls(msg, &num_controls, &controls);

	for (i=0; i<num_controls; i++) {
		if (strcmp(controls[i].oid, oid) == 0) {
			return &controls[i];
		}
	}
	return NULL;
}

struct tldap_search_paged_state {
	struct tevent_context *ev;
	struct tldap_context *ld;
	const char *base;
	const char *filter;
	int scope;
	const char **attrs;
	int num_attrs;
	int attrsonly;
	struct tldap_control *sctrls;
	int num_sctrls;
	struct tldap_control *cctrls;
	int num_cctrls;
	int timelimit;
	int sizelimit;
	int deref;

	int page_size;
	struct asn1_data *asn1;
	DATA_BLOB cookie;
	struct tldap_message *result;
};

static struct tevent_req *tldap_ship_paged_search(
	TALLOC_CTX *mem_ctx,
	struct tldap_search_paged_state *state)
{
	struct tldap_control *pgctrl;
	struct asn1_data *asn1;

	asn1 = asn1_init(state);
	if (asn1 == NULL) {
		return NULL;
	}
	asn1_push_tag(asn1, ASN1_SEQUENCE(0));
	asn1_write_Integer(asn1, state->page_size);
	asn1_write_OctetString(asn1, state->cookie.data, state->cookie.length);
	asn1_pop_tag(asn1);
	if (asn1->has_error) {
		TALLOC_FREE(asn1);
		return NULL;
	}
	state->asn1 = asn1;

	pgctrl = &state->sctrls[state->num_sctrls-1];
	pgctrl->oid = TLDAP_CONTROL_PAGEDRESULTS;
	pgctrl->critical = true;
	if (!asn1_blob(state->asn1, &pgctrl->value)) {
		TALLOC_FREE(asn1);
		return NULL;
	}
	return tldap_search_send(mem_ctx, state->ev, state->ld, state->base,
				 state->scope, state->filter, state->attrs,
				 state->num_attrs, state->attrsonly,
				 state->sctrls, state->num_sctrls,
				 state->cctrls, state->num_cctrls,
				 state->timelimit, state->sizelimit,
				 state->deref);
}

static void tldap_search_paged_done(struct tevent_req *subreq);

struct tevent_req *tldap_search_paged_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct tldap_context *ld,
					   const char *base, int scope,
					   const char *filter,
					   const char **attrs,
					   int num_attrs,
					   int attrsonly,
					   struct tldap_control *sctrls,
					   int num_sctrls,
					   struct tldap_control *cctrls,
					   int num_cctrls,
					   int timelimit,
					   int sizelimit,
					   int deref,
					   int page_size)
{
	struct tevent_req *req, *subreq;
	struct tldap_search_paged_state *state;
	struct tldap_control empty_control;

	req = tevent_req_create(mem_ctx, &state,
				struct tldap_search_paged_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->ld = ld;
	state->base = base;
	state->filter = filter;
	state->scope = scope;
	state->attrs = attrs;
	state->num_attrs = num_attrs;
	state->attrsonly = attrsonly;
	state->cctrls = cctrls;
	state->num_cctrls = num_cctrls;
	state->timelimit = timelimit;
	state->sizelimit = sizelimit;
	state->deref = deref;

	state->page_size = page_size;
	state->asn1 = NULL;
	state->cookie = data_blob_null;

	ZERO_STRUCT(empty_control);

	state->sctrls = tldap_add_control(state, sctrls, num_sctrls,
					  &empty_control);
	if (tevent_req_nomem(state->sctrls, req)) {
		return tevent_req_post(req, ev);
	}
	state->num_sctrls = num_sctrls+1;

	subreq = tldap_ship_paged_search(state, state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tldap_search_paged_done, req);

	return req;
}

static void tldap_search_paged_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct tldap_search_paged_state *state = tevent_req_data(
		req, struct tldap_search_paged_state);
	struct asn1_data *asn1;
	struct tldap_control *pgctrl;
	int rc, size;

	rc = tldap_search_recv(subreq, state, &state->result);
	if (rc != TLDAP_SUCCESS) {
		TALLOC_FREE(subreq);
		tevent_req_error(req, rc);
		return;
	}

	TALLOC_FREE(state->asn1);

	switch (tldap_msg_type(state->result)) {
	case TLDAP_RES_SEARCH_ENTRY:
	case TLDAP_RES_SEARCH_REFERENCE:
		tevent_req_notify_callback(req);
		return;
	case TLDAP_RES_SEARCH_RESULT:
		break;
	default:
		TALLOC_FREE(subreq);
		tevent_req_error(req, TLDAP_PROTOCOL_ERROR);
		return;
	}

	TALLOC_FREE(subreq);

	/* We've finished one paged search, fire the next */

	pgctrl = tldap_msg_findcontrol(state->result,
				       TLDAP_CONTROL_PAGEDRESULTS);
	if (pgctrl == NULL) {
		/* RFC2696 requires the server to return the control */
		tevent_req_error(req, TLDAP_PROTOCOL_ERROR);
		return;
	}

	TALLOC_FREE(state->cookie.data);

	asn1 = asn1_init(talloc_tos());
	if (asn1 == NULL) {
		tevent_req_error(req, TLDAP_NO_MEMORY);
		return;
	}

	asn1_load_nocopy(asn1, pgctrl->value.data, pgctrl->value.length);
	asn1_start_tag(asn1, ASN1_SEQUENCE(0));
	asn1_read_Integer(asn1, &size);
	asn1_read_OctetString(asn1, state, &state->cookie);
	asn1_end_tag(asn1);
	if (asn1->has_error) {
		tevent_req_error(req, TLDAP_DECODING_ERROR);
		return;
	}
	TALLOC_FREE(asn1);

	if (state->cookie.length == 0) {
		/* We're done, no cookie anymore */
		tevent_req_done(req);
		return;
	}

	TALLOC_FREE(state->result);

	subreq = tldap_ship_paged_search(state, state);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tldap_search_paged_done, req);
}

int tldap_search_paged_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			    struct tldap_message **pmsg)
{
	struct tldap_search_paged_state *state = tevent_req_data(
		req, struct tldap_search_paged_state);
	int err;

	if (!tevent_req_is_in_progress(req)
	    && tevent_req_is_ldap_error(req, &err)) {
		return err;
	}
	if (tevent_req_is_in_progress(req)) {
		switch (tldap_msg_type(state->result)) {
		case TLDAP_RES_SEARCH_ENTRY:
		case TLDAP_RES_SEARCH_REFERENCE:
			break;
		default:
			return TLDAP_PROTOCOL_ERROR;
		}
	}
	*pmsg = talloc_move(mem_ctx, &state->result);
	return TLDAP_SUCCESS;
}

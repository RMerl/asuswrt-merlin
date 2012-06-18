/*
  Linux DNS client library implementation
  Copyright (C) 2006 Gerald Carter <jerry@samba.org>

     ** NOTE! The following LGPL license applies to the libaddns
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "dns.h"
#include "assert.h"

struct dns_buffer *dns_create_buffer(TALLOC_CTX *mem_ctx)
{
	struct dns_buffer *result;

	if (!(result = talloc(mem_ctx, struct dns_buffer))) {
		return NULL;
	}

	result->offset = 0;
	result->error = ERROR_DNS_SUCCESS;
	
	/*
	 * Small inital size to excercise the realloc code
	 */
	result->size = 2;

	if (!(result->data = TALLOC_ARRAY(result, uint8, result->size))) {
		TALLOC_FREE(result);
		return NULL;
	}

	return result;
}

void dns_marshall_buffer(struct dns_buffer *buf, const uint8 *data,
			 size_t len)
{
	if (!ERR_DNS_IS_OK(buf->error)) return;

	if (buf->offset + len < buf->offset) {
		/*
		 * Wraparound!
		 */
		buf->error = ERROR_DNS_INVALID_PARAMETER;
		return;
	}

	if ((buf->offset + len) > 0xffff) {
		/*
		 * Only 64k possible
		 */
		buf->error = ERROR_DNS_INVALID_PARAMETER;
		return;
	}
		
	if (buf->offset + len > buf->size) {
		size_t new_size = buf->offset + len;
		uint8 *new_data;

		/*
		 * Don't do too many reallocs, round up to some multiple
		 */

		new_size += (64 - (new_size % 64));

		if (!(new_data = TALLOC_REALLOC_ARRAY(buf, buf->data, uint8,
						      new_size))) {
			buf->error = ERROR_DNS_NO_MEMORY;
			return;
		}

		buf->size = new_size;
		buf->data = new_data;
	}

	memcpy(buf->data + buf->offset, data, len);
	buf->offset += len;
	return;
}

void dns_marshall_uint16(struct dns_buffer *buf, uint16 val)
{
	uint16 n_val = htons(val);
	dns_marshall_buffer(buf, (uint8 *)&n_val, sizeof(n_val));
}

void dns_marshall_uint32(struct dns_buffer *buf, uint32 val)
{
	uint32 n_val = htonl(val);
	dns_marshall_buffer(buf, (uint8 *)&n_val, sizeof(n_val));
}

void dns_unmarshall_buffer(struct dns_buffer *buf, uint8 *data,
			   size_t len)
{
	if (!(ERR_DNS_IS_OK(buf->error))) return;

	if ((len > buf->size) || (buf->offset + len > buf->size)) {
		buf->error = ERROR_DNS_INVALID_MESSAGE;
		return;
	}

	memcpy((void *)data, (const void *)(buf->data + buf->offset), len);
	buf->offset += len;

	return;
}

void dns_unmarshall_uint16(struct dns_buffer *buf, uint16 *val)
{
	uint16 n_val;

	dns_unmarshall_buffer(buf, (uint8 *)&n_val, sizeof(n_val));
	if (!(ERR_DNS_IS_OK(buf->error))) return;

	*val = ntohs(n_val);
}

void dns_unmarshall_uint32(struct dns_buffer *buf, uint32 *val)
{
	uint32 n_val;

	dns_unmarshall_buffer(buf, (uint8 *)&n_val, sizeof(n_val));
	if (!(ERR_DNS_IS_OK(buf->error))) return;

	*val = ntohl(n_val);
}

void dns_marshall_domain_name(struct dns_buffer *buf,
			      const struct dns_domain_name *name)
{
	struct dns_domain_label *label;
	char end_char = '\0';

	/*
	 * TODO: Implement DNS compression
	 */

	for (label = name->pLabelList; label != NULL; label = label->next) {
		uint8 len = label->len;

		dns_marshall_buffer(buf, (uint8 *)&len, sizeof(len));
		if (!ERR_DNS_IS_OK(buf->error)) return;

		dns_marshall_buffer(buf, (uint8 *)label->label, len);
		if (!ERR_DNS_IS_OK(buf->error)) return;
	}

	dns_marshall_buffer(buf, (uint8 *)&end_char, 1);
}

static void dns_unmarshall_label(TALLOC_CTX *mem_ctx,
				 int level,
				 struct dns_buffer *buf,
				 struct dns_domain_label **plabel)
{
	struct dns_domain_label *label;
	uint8 len;

	if (!ERR_DNS_IS_OK(buf->error)) return;

	if (level > 128) {
		/*
		 * Protect against recursion
		 */
		buf->error = ERROR_DNS_INVALID_MESSAGE;
		return;
	}

	dns_unmarshall_buffer(buf, &len, sizeof(len));
	if (!ERR_DNS_IS_OK(buf->error)) return;

	if (len == 0) {
		*plabel = NULL;
		return;
	}

	if ((len & 0xc0) == 0xc0) {
		/*
		 * We've got a compressed name. Build up a new "fake" buffer
		 * and using the calculated offset.
		 */
		struct dns_buffer new_buf;
		uint8 low;

		dns_unmarshall_buffer(buf, &low, sizeof(low));
		if (!ERR_DNS_IS_OK(buf->error)) return;

		new_buf = *buf;
		new_buf.offset = len & 0x3f;
		new_buf.offset <<= 8;
		new_buf.offset |= low;

		dns_unmarshall_label(mem_ctx, level+1, &new_buf, plabel);
		buf->error = new_buf.error;
		return;
	}

	if ((len & 0xc0) != 0) {
		buf->error = ERROR_DNS_INVALID_NAME;
		return;
	}

	if (!(label = talloc(mem_ctx, struct dns_domain_label))) {
		buf->error = ERROR_DNS_NO_MEMORY;
		return;
	}

	label->len = len;

	if (!(label->label = TALLOC_ARRAY(label, char, len+1))) {
		buf->error = ERROR_DNS_NO_MEMORY;
		goto error;
	}

	dns_unmarshall_buffer(buf, (uint8 *)label->label, len);
	if (!ERR_DNS_IS_OK(buf->error)) goto error;

	dns_unmarshall_label(label, level+1, buf, &label->next);
	if (!ERR_DNS_IS_OK(buf->error)) goto error;

	*plabel = label;
	return;

 error:
	TALLOC_FREE(label);
	return;
}

void dns_unmarshall_domain_name(TALLOC_CTX *mem_ctx,
				struct dns_buffer *buf,
				struct dns_domain_name **pname)
{
	struct dns_domain_name *name;

	if (!ERR_DNS_IS_OK(buf->error)) return;

	if (!(name = talloc(mem_ctx, struct dns_domain_name))) {
		buf->error = ERROR_DNS_NO_MEMORY;
		return;
	}

	dns_unmarshall_label(name, 0, buf, &name->pLabelList);

	if (!ERR_DNS_IS_OK(buf->error)) {
		return;
	}

	*pname = name;
	return;
}

static void dns_marshall_question(struct dns_buffer *buf,
				  const struct dns_question *q)
{
	dns_marshall_domain_name(buf, q->name);
	dns_marshall_uint16(buf, q->q_type);
	dns_marshall_uint16(buf, q->q_class);
}

static void dns_unmarshall_question(TALLOC_CTX *mem_ctx,
				    struct dns_buffer *buf,
				    struct dns_question **pq)
{
	struct dns_question *q;

	if (!(ERR_DNS_IS_OK(buf->error))) return;

	if (!(q = talloc(mem_ctx, struct dns_question))) {
		buf->error = ERROR_DNS_NO_MEMORY;
		return;
	}

	dns_unmarshall_domain_name(q, buf, &q->name);
	dns_unmarshall_uint16(buf, &q->q_type);
	dns_unmarshall_uint16(buf, &q->q_class);

	if (!(ERR_DNS_IS_OK(buf->error))) return;

	*pq = q;
}

static void dns_marshall_rr(struct dns_buffer *buf,
			    const struct dns_rrec *r)
{
	dns_marshall_domain_name(buf, r->name);
	dns_marshall_uint16(buf, r->type);
	dns_marshall_uint16(buf, r->r_class);
	dns_marshall_uint32(buf, r->ttl);
	dns_marshall_uint16(buf, r->data_length);
	dns_marshall_buffer(buf, r->data, r->data_length);
}

static void dns_unmarshall_rr(TALLOC_CTX *mem_ctx,
			      struct dns_buffer *buf,
			      struct dns_rrec **pr)
{
	struct dns_rrec *r;

	if (!(ERR_DNS_IS_OK(buf->error))) return;

	if (!(r = talloc(mem_ctx, struct dns_rrec))) {
		buf->error = ERROR_DNS_NO_MEMORY;
		return;
	}

	dns_unmarshall_domain_name(r, buf, &r->name);
	dns_unmarshall_uint16(buf, &r->type);
	dns_unmarshall_uint16(buf, &r->r_class);
	dns_unmarshall_uint32(buf, &r->ttl);
	dns_unmarshall_uint16(buf, &r->data_length);
	r->data = NULL;

	if (!(ERR_DNS_IS_OK(buf->error))) return;

	if (r->data_length != 0) {
		if (!(r->data = TALLOC_ARRAY(r, uint8, r->data_length))) {
			buf->error = ERROR_DNS_NO_MEMORY;
			return;
		}
		dns_unmarshall_buffer(buf, r->data, r->data_length);
	}

	if (!(ERR_DNS_IS_OK(buf->error))) return;

	*pr = r;
}

DNS_ERROR dns_marshall_request(TALLOC_CTX *mem_ctx,
			       const struct dns_request *req,
			       struct dns_buffer **pbuf)
{
	struct dns_buffer *buf;
	uint16 i;

	if (!(buf = dns_create_buffer(mem_ctx))) {
		return ERROR_DNS_NO_MEMORY;
	}

	dns_marshall_uint16(buf, req->id);
	dns_marshall_uint16(buf, req->flags);
	dns_marshall_uint16(buf, req->num_questions);
	dns_marshall_uint16(buf, req->num_answers);
	dns_marshall_uint16(buf, req->num_auths);
	dns_marshall_uint16(buf, req->num_additionals);

	for (i=0; i<req->num_questions; i++) {
		dns_marshall_question(buf, req->questions[i]);
	}
	for (i=0; i<req->num_answers; i++) {
		dns_marshall_rr(buf, req->answers[i]);
	}
	for (i=0; i<req->num_auths; i++) {
		dns_marshall_rr(buf, req->auths[i]);
	}
	for (i=0; i<req->num_additionals; i++) {
		dns_marshall_rr(buf, req->additionals[i]);
	}

	if (!ERR_DNS_IS_OK(buf->error)) {
		DNS_ERROR err = buf->error;
		TALLOC_FREE(buf);
		return err;
	}

	*pbuf = buf;
	return ERROR_DNS_SUCCESS;
}

DNS_ERROR dns_unmarshall_request(TALLOC_CTX *mem_ctx,
				 struct dns_buffer *buf,
				 struct dns_request **preq)
{
	struct dns_request *req;
	uint16 i;
	DNS_ERROR err;

	if (!(req = TALLOC_ZERO_P(mem_ctx, struct dns_request))) {
		return ERROR_DNS_NO_MEMORY;
	}

	dns_unmarshall_uint16(buf, &req->id);
	dns_unmarshall_uint16(buf, &req->flags);
	dns_unmarshall_uint16(buf, &req->num_questions);
	dns_unmarshall_uint16(buf, &req->num_answers);
	dns_unmarshall_uint16(buf, &req->num_auths);
	dns_unmarshall_uint16(buf, &req->num_additionals);

	if (!ERR_DNS_IS_OK(buf->error)) goto error;

	err = ERROR_DNS_NO_MEMORY;

	if ((req->num_questions != 0) &&
	    !(req->questions = TALLOC_ARRAY(req, struct dns_question *,
					    req->num_questions))) {
		goto error;
	}
	if ((req->num_answers != 0) &&
	    !(req->answers = TALLOC_ARRAY(req, struct dns_rrec *,
					  req->num_answers))) {
		goto error;
	}
	if ((req->num_auths != 0) &&
	    !(req->auths = TALLOC_ARRAY(req, struct dns_rrec *,
					req->num_auths))) {
		goto error;
	}
	if ((req->num_additionals != 0) &&
	    !(req->additionals = TALLOC_ARRAY(req, struct dns_rrec *,
					      req->num_additionals))) {
		goto error;
	}

	for (i=0; i<req->num_questions; i++) {
		dns_unmarshall_question(req->questions, buf,
					&req->questions[i]);
	}
	for (i=0; i<req->num_answers; i++) {
		dns_unmarshall_rr(req->answers, buf,
				  &req->answers[i]);
	}
	for (i=0; i<req->num_auths; i++) {
		dns_unmarshall_rr(req->auths, buf,
				  &req->auths[i]);
	}
	for (i=0; i<req->num_additionals; i++) {
		dns_unmarshall_rr(req->additionals, buf,
				  &req->additionals[i]);
	}

	if (!ERR_DNS_IS_OK(buf->error)) {
		err = buf->error;
		goto error;
	}

	*preq = req;
	return ERROR_DNS_SUCCESS;

 error:
	err = buf->error;
	TALLOC_FREE(req);
	return err;
}

struct dns_request *dns_update2request(struct dns_update_request *update)
{
	struct dns_request *req;

	/*
	 * This is a non-specified construct that happens to work on Linux/gcc
	 * and I would expect it to work everywhere else. dns_request and
	 * dns_update_request are essentially the same structures with
	 * different names, so any difference would mean that the compiler
	 * applied two different variations of padding given the same types in
	 * the structures.
	 */

	req = (struct dns_request *)(void *)update;

	/*
	 * The assert statement here looks like we could do the equivalent
	 * assignments to get portable, but it would mean that we have to
	 * allocate the dns_question record for the dns_zone records. We
	 * assume that if this assert works then the same holds true for
	 * dns_zone<>dns_question as well.
	 */

#ifdef DEVELOPER
	assert((req->id == update->id) && (req->flags == update->flags) &&
	       (req->num_questions == update->num_zones) &&
	       (req->num_answers == update->num_preqs) &&
	       (req->num_auths == update->num_updates) &&
	       (req->num_additionals == update->num_additionals) &&
	       (req->questions ==
		(struct dns_question **)(void *)update->zones) &&
	       (req->answers == update->preqs) &&
	       (req->auths == update->updates) &&
	       (req->additionals == update->additionals));
#endif

	return req;
}

struct dns_update_request *dns_request2update(struct dns_request *request)
{
	/*
	 * For portability concerns see dns_update2request;
	 */
	return (struct dns_update_request *)(void *)request;
}

DNS_ERROR dns_marshall_update_request(TALLOC_CTX *mem_ctx,
				      struct dns_update_request *update,
				      struct dns_buffer **pbuf)
{
	return dns_marshall_request(mem_ctx, dns_update2request(update), pbuf);
}

DNS_ERROR dns_unmarshall_update_request(TALLOC_CTX *mem_ctx,
					struct dns_buffer *buf,
					struct dns_update_request **pupreq)
{
	/*
	 * See comments above about portability. If the above works, this will
	 * as well.
	 */

	return dns_unmarshall_request(mem_ctx, buf,
				      (struct dns_request **)(void *)pupreq);
}

uint16 dns_response_code(uint16 flags)
{
	return flags & 0xF;
}

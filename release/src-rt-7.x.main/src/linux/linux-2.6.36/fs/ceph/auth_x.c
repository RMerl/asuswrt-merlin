
#include "ceph_debug.h"

#include <linux/err.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>

#include "auth_x.h"
#include "auth_x_protocol.h"
#include "crypto.h"
#include "auth.h"
#include "decode.h"

#define TEMP_TICKET_BUF_LEN	256

static void ceph_x_validate_tickets(struct ceph_auth_client *ac, int *pneed);

static int ceph_x_is_authenticated(struct ceph_auth_client *ac)
{
	struct ceph_x_info *xi = ac->private;
	int need;

	ceph_x_validate_tickets(ac, &need);
	dout("ceph_x_is_authenticated want=%d need=%d have=%d\n",
	     ac->want_keys, need, xi->have_keys);
	return (ac->want_keys & xi->have_keys) == ac->want_keys;
}

static int ceph_x_should_authenticate(struct ceph_auth_client *ac)
{
	struct ceph_x_info *xi = ac->private;
	int need;

	ceph_x_validate_tickets(ac, &need);
	dout("ceph_x_should_authenticate want=%d need=%d have=%d\n",
	     ac->want_keys, need, xi->have_keys);
	return need != 0;
}

static int ceph_x_encrypt_buflen(int ilen)
{
	return sizeof(struct ceph_x_encrypt_header) + ilen + 16 +
		sizeof(u32);
}

static int ceph_x_encrypt(struct ceph_crypto_key *secret,
			  void *ibuf, int ilen, void *obuf, size_t olen)
{
	struct ceph_x_encrypt_header head = {
		.struct_v = 1,
		.magic = cpu_to_le64(CEPHX_ENC_MAGIC)
	};
	size_t len = olen - sizeof(u32);
	int ret;

	ret = ceph_encrypt2(secret, obuf + sizeof(u32), &len,
			    &head, sizeof(head), ibuf, ilen);
	if (ret)
		return ret;
	ceph_encode_32(&obuf, len);
	return len + sizeof(u32);
}

static int ceph_x_decrypt(struct ceph_crypto_key *secret,
			  void **p, void *end, void *obuf, size_t olen)
{
	struct ceph_x_encrypt_header head;
	size_t head_len = sizeof(head);
	int len, ret;

	len = ceph_decode_32(p);
	if (*p + len > end)
		return -EINVAL;

	dout("ceph_x_decrypt len %d\n", len);
	ret = ceph_decrypt2(secret, &head, &head_len, obuf, &olen,
			    *p, len);
	if (ret)
		return ret;
	if (head.struct_v != 1 || le64_to_cpu(head.magic) != CEPHX_ENC_MAGIC)
		return -EPERM;
	*p += len;
	return olen;
}

/*
 * get existing (or insert new) ticket handler
 */
static struct ceph_x_ticket_handler *
get_ticket_handler(struct ceph_auth_client *ac, int service)
{
	struct ceph_x_ticket_handler *th;
	struct ceph_x_info *xi = ac->private;
	struct rb_node *parent = NULL, **p = &xi->ticket_handlers.rb_node;

	while (*p) {
		parent = *p;
		th = rb_entry(parent, struct ceph_x_ticket_handler, node);
		if (service < th->service)
			p = &(*p)->rb_left;
		else if (service > th->service)
			p = &(*p)->rb_right;
		else
			return th;
	}

	/* add it */
	th = kzalloc(sizeof(*th), GFP_NOFS);
	if (!th)
		return ERR_PTR(-ENOMEM);
	th->service = service;
	rb_link_node(&th->node, parent, p);
	rb_insert_color(&th->node, &xi->ticket_handlers);
	return th;
}

static void remove_ticket_handler(struct ceph_auth_client *ac,
				  struct ceph_x_ticket_handler *th)
{
	struct ceph_x_info *xi = ac->private;

	dout("remove_ticket_handler %p %d\n", th, th->service);
	rb_erase(&th->node, &xi->ticket_handlers);
	ceph_crypto_key_destroy(&th->session_key);
	if (th->ticket_blob)
		ceph_buffer_put(th->ticket_blob);
	kfree(th);
}

static int ceph_x_proc_ticket_reply(struct ceph_auth_client *ac,
				    struct ceph_crypto_key *secret,
				    void *buf, void *end)
{
	struct ceph_x_info *xi = ac->private;
	int num;
	void *p = buf;
	int ret;
	char *dbuf;
	char *ticket_buf;
	u8 reply_struct_v;

	dbuf = kmalloc(TEMP_TICKET_BUF_LEN, GFP_NOFS);
	if (!dbuf)
		return -ENOMEM;

	ret = -ENOMEM;
	ticket_buf = kmalloc(TEMP_TICKET_BUF_LEN, GFP_NOFS);
	if (!ticket_buf)
		goto out_dbuf;

	ceph_decode_need(&p, end, 1 + sizeof(u32), bad);
	reply_struct_v = ceph_decode_8(&p);
	if (reply_struct_v != 1)
		goto bad;
	num = ceph_decode_32(&p);
	dout("%d tickets\n", num);
	while (num--) {
		int type;
		u8 tkt_struct_v, blob_struct_v;
		struct ceph_x_ticket_handler *th;
		void *dp, *dend;
		int dlen;
		char is_enc;
		struct timespec validity;
		struct ceph_crypto_key old_key;
		void *tp, *tpend;
		struct ceph_timespec new_validity;
		struct ceph_crypto_key new_session_key;
		struct ceph_buffer *new_ticket_blob;
		unsigned long new_expires, new_renew_after;
		u64 new_secret_id;

		ceph_decode_need(&p, end, sizeof(u32) + 1, bad);

		type = ceph_decode_32(&p);
		dout(" ticket type %d %s\n", type, ceph_entity_type_name(type));

		tkt_struct_v = ceph_decode_8(&p);
		if (tkt_struct_v != 1)
			goto bad;

		th = get_ticket_handler(ac, type);
		if (IS_ERR(th)) {
			ret = PTR_ERR(th);
			goto out;
		}

		/* blob for me */
		dlen = ceph_x_decrypt(secret, &p, end, dbuf,
				      TEMP_TICKET_BUF_LEN);
		if (dlen <= 0) {
			ret = dlen;
			goto out;
		}
		dout(" decrypted %d bytes\n", dlen);
		dend = dbuf + dlen;
		dp = dbuf;

		tkt_struct_v = ceph_decode_8(&dp);
		if (tkt_struct_v != 1)
			goto bad;

		memcpy(&old_key, &th->session_key, sizeof(old_key));
		ret = ceph_crypto_key_decode(&new_session_key, &dp, dend);
		if (ret)
			goto out;

		ceph_decode_copy(&dp, &new_validity, sizeof(new_validity));
		ceph_decode_timespec(&validity, &new_validity);
		new_expires = get_seconds() + validity.tv_sec;
		new_renew_after = new_expires - (validity.tv_sec / 4);
		dout(" expires=%lu renew_after=%lu\n", new_expires,
		     new_renew_after);

		/* ticket blob for service */
		ceph_decode_8_safe(&p, end, is_enc, bad);
		tp = ticket_buf;
		if (is_enc) {
			/* encrypted */
			dout(" encrypted ticket\n");
			dlen = ceph_x_decrypt(&old_key, &p, end, ticket_buf,
					      TEMP_TICKET_BUF_LEN);
			if (dlen < 0) {
				ret = dlen;
				goto out;
			}
			dlen = ceph_decode_32(&tp);
		} else {
			/* unencrypted */
			ceph_decode_32_safe(&p, end, dlen, bad);
			ceph_decode_need(&p, end, dlen, bad);
			ceph_decode_copy(&p, ticket_buf, dlen);
		}
		tpend = tp + dlen;
		dout(" ticket blob is %d bytes\n", dlen);
		ceph_decode_need(&tp, tpend, 1 + sizeof(u64), bad);
		blob_struct_v = ceph_decode_8(&tp);
		new_secret_id = ceph_decode_64(&tp);
		ret = ceph_decode_buffer(&new_ticket_blob, &tp, tpend);
		if (ret)
			goto out;

		/* all is well, update our ticket */
		ceph_crypto_key_destroy(&th->session_key);
		if (th->ticket_blob)
			ceph_buffer_put(th->ticket_blob);
		th->session_key = new_session_key;
		th->ticket_blob = new_ticket_blob;
		th->validity = new_validity;
		th->secret_id = new_secret_id;
		th->expires = new_expires;
		th->renew_after = new_renew_after;
		dout(" got ticket service %d (%s) secret_id %lld len %d\n",
		     type, ceph_entity_type_name(type), th->secret_id,
		     (int)th->ticket_blob->vec.iov_len);
		xi->have_keys |= th->service;
	}

	ret = 0;
out:
	kfree(ticket_buf);
out_dbuf:
	kfree(dbuf);
	return ret;

bad:
	ret = -EINVAL;
	goto out;
}

static int ceph_x_build_authorizer(struct ceph_auth_client *ac,
				   struct ceph_x_ticket_handler *th,
				   struct ceph_x_authorizer *au)
{
	int maxlen;
	struct ceph_x_authorize_a *msg_a;
	struct ceph_x_authorize_b msg_b;
	void *p, *end;
	int ret;
	int ticket_blob_len =
		(th->ticket_blob ? th->ticket_blob->vec.iov_len : 0);

	dout("build_authorizer for %s %p\n",
	     ceph_entity_type_name(th->service), au);

	maxlen = sizeof(*msg_a) + sizeof(msg_b) +
		ceph_x_encrypt_buflen(ticket_blob_len);
	dout("  need len %d\n", maxlen);
	if (au->buf && au->buf->alloc_len < maxlen) {
		ceph_buffer_put(au->buf);
		au->buf = NULL;
	}
	if (!au->buf) {
		au->buf = ceph_buffer_new(maxlen, GFP_NOFS);
		if (!au->buf)
			return -ENOMEM;
	}
	au->service = th->service;

	msg_a = au->buf->vec.iov_base;
	msg_a->struct_v = 1;
	msg_a->global_id = cpu_to_le64(ac->global_id);
	msg_a->service_id = cpu_to_le32(th->service);
	msg_a->ticket_blob.struct_v = 1;
	msg_a->ticket_blob.secret_id = cpu_to_le64(th->secret_id);
	msg_a->ticket_blob.blob_len = cpu_to_le32(ticket_blob_len);
	if (ticket_blob_len) {
		memcpy(msg_a->ticket_blob.blob, th->ticket_blob->vec.iov_base,
		       th->ticket_blob->vec.iov_len);
	}
	dout(" th %p secret_id %lld %lld\n", th, th->secret_id,
	     le64_to_cpu(msg_a->ticket_blob.secret_id));

	p = msg_a + 1;
	p += ticket_blob_len;
	end = au->buf->vec.iov_base + au->buf->vec.iov_len;

	get_random_bytes(&au->nonce, sizeof(au->nonce));
	msg_b.struct_v = 1;
	msg_b.nonce = cpu_to_le64(au->nonce);
	ret = ceph_x_encrypt(&th->session_key, &msg_b, sizeof(msg_b),
			     p, end - p);
	if (ret < 0)
		goto out_buf;
	p += ret;
	au->buf->vec.iov_len = p - au->buf->vec.iov_base;
	dout(" built authorizer nonce %llx len %d\n", au->nonce,
	     (int)au->buf->vec.iov_len);
	BUG_ON(au->buf->vec.iov_len > maxlen);
	return 0;

out_buf:
	ceph_buffer_put(au->buf);
	au->buf = NULL;
	return ret;
}

static int ceph_x_encode_ticket(struct ceph_x_ticket_handler *th,
				void **p, void *end)
{
	ceph_decode_need(p, end, 1 + sizeof(u64), bad);
	ceph_encode_8(p, 1);
	ceph_encode_64(p, th->secret_id);
	if (th->ticket_blob) {
		const char *buf = th->ticket_blob->vec.iov_base;
		u32 len = th->ticket_blob->vec.iov_len;

		ceph_encode_32_safe(p, end, len, bad);
		ceph_encode_copy_safe(p, end, buf, len, bad);
	} else {
		ceph_encode_32_safe(p, end, 0, bad);
	}

	return 0;
bad:
	return -ERANGE;
}

static void ceph_x_validate_tickets(struct ceph_auth_client *ac, int *pneed)
{
	int want = ac->want_keys;
	struct ceph_x_info *xi = ac->private;
	int service;

	*pneed = ac->want_keys & ~(xi->have_keys);

	for (service = 1; service <= want; service <<= 1) {
		struct ceph_x_ticket_handler *th;

		if (!(ac->want_keys & service))
			continue;

		if (*pneed & service)
			continue;

		th = get_ticket_handler(ac, service);

		if (IS_ERR(th)) {
			*pneed |= service;
			continue;
		}

		if (get_seconds() >= th->renew_after)
			*pneed |= service;
		if (get_seconds() >= th->expires)
			xi->have_keys &= ~service;
	}
}


static int ceph_x_build_request(struct ceph_auth_client *ac,
				void *buf, void *end)
{
	struct ceph_x_info *xi = ac->private;
	int need;
	struct ceph_x_request_header *head = buf;
	int ret;
	struct ceph_x_ticket_handler *th =
		get_ticket_handler(ac, CEPH_ENTITY_TYPE_AUTH);

	if (IS_ERR(th))
		return PTR_ERR(th);

	ceph_x_validate_tickets(ac, &need);

	dout("build_request want %x have %x need %x\n",
	     ac->want_keys, xi->have_keys, need);

	if (need & CEPH_ENTITY_TYPE_AUTH) {
		struct ceph_x_authenticate *auth = (void *)(head + 1);
		void *p = auth + 1;
		struct ceph_x_challenge_blob tmp;
		char tmp_enc[40];
		u64 *u;

		if (p > end)
			return -ERANGE;

		dout(" get_auth_session_key\n");
		head->op = cpu_to_le16(CEPHX_GET_AUTH_SESSION_KEY);

		/* encrypt and hash */
		get_random_bytes(&auth->client_challenge, sizeof(u64));
		tmp.client_challenge = auth->client_challenge;
		tmp.server_challenge = cpu_to_le64(xi->server_challenge);
		ret = ceph_x_encrypt(&xi->secret, &tmp, sizeof(tmp),
				     tmp_enc, sizeof(tmp_enc));
		if (ret < 0)
			return ret;

		auth->struct_v = 1;
		auth->key = 0;
		for (u = (u64 *)tmp_enc; u + 1 <= (u64 *)(tmp_enc + ret); u++)
			auth->key ^= *(__le64 *)u;
		dout(" server_challenge %llx client_challenge %llx key %llx\n",
		     xi->server_challenge, le64_to_cpu(auth->client_challenge),
		     le64_to_cpu(auth->key));

		/* now encode the old ticket if exists */
		ret = ceph_x_encode_ticket(th, &p, end);
		if (ret < 0)
			return ret;

		return p - buf;
	}

	if (need) {
		void *p = head + 1;
		struct ceph_x_service_ticket_request *req;

		if (p > end)
			return -ERANGE;
		head->op = cpu_to_le16(CEPHX_GET_PRINCIPAL_SESSION_KEY);

		ret = ceph_x_build_authorizer(ac, th, &xi->auth_authorizer);
		if (ret)
			return ret;
		ceph_encode_copy(&p, xi->auth_authorizer.buf->vec.iov_base,
				 xi->auth_authorizer.buf->vec.iov_len);

		req = p;
		req->keys = cpu_to_le32(need);
		p += sizeof(*req);
		return p - buf;
	}

	return 0;
}

static int ceph_x_handle_reply(struct ceph_auth_client *ac, int result,
			       void *buf, void *end)
{
	struct ceph_x_info *xi = ac->private;
	struct ceph_x_reply_header *head = buf;
	struct ceph_x_ticket_handler *th;
	int len = end - buf;
	int op;
	int ret;

	if (result)
		return result;

	if (xi->starting) {
		/* it's a hello */
		struct ceph_x_server_challenge *sc = buf;

		if (len != sizeof(*sc))
			return -EINVAL;
		xi->server_challenge = le64_to_cpu(sc->server_challenge);
		dout("handle_reply got server challenge %llx\n",
		     xi->server_challenge);
		xi->starting = false;
		xi->have_keys &= ~CEPH_ENTITY_TYPE_AUTH;
		return -EAGAIN;
	}

	op = le16_to_cpu(head->op);
	result = le32_to_cpu(head->result);
	dout("handle_reply op %d result %d\n", op, result);
	switch (op) {
	case CEPHX_GET_AUTH_SESSION_KEY:
		/* verify auth key */
		ret = ceph_x_proc_ticket_reply(ac, &xi->secret,
					       buf + sizeof(*head), end);
		break;

	case CEPHX_GET_PRINCIPAL_SESSION_KEY:
		th = get_ticket_handler(ac, CEPH_ENTITY_TYPE_AUTH);
		if (IS_ERR(th))
			return PTR_ERR(th);
		ret = ceph_x_proc_ticket_reply(ac, &th->session_key,
					       buf + sizeof(*head), end);
		break;

	default:
		return -EINVAL;
	}
	if (ret)
		return ret;
	if (ac->want_keys == xi->have_keys)
		return 0;
	return -EAGAIN;
}

static int ceph_x_create_authorizer(
	struct ceph_auth_client *ac, int peer_type,
	struct ceph_authorizer **a,
	void **buf, size_t *len,
	void **reply_buf, size_t *reply_len)
{
	struct ceph_x_authorizer *au;
	struct ceph_x_ticket_handler *th;
	int ret;

	th = get_ticket_handler(ac, peer_type);
	if (IS_ERR(th))
		return PTR_ERR(th);

	au = kzalloc(sizeof(*au), GFP_NOFS);
	if (!au)
		return -ENOMEM;

	ret = ceph_x_build_authorizer(ac, th, au);
	if (ret) {
		kfree(au);
		return ret;
	}

	*a = (struct ceph_authorizer *)au;
	*buf = au->buf->vec.iov_base;
	*len = au->buf->vec.iov_len;
	*reply_buf = au->reply_buf;
	*reply_len = sizeof(au->reply_buf);
	return 0;
}

static int ceph_x_verify_authorizer_reply(struct ceph_auth_client *ac,
					  struct ceph_authorizer *a, size_t len)
{
	struct ceph_x_authorizer *au = (void *)a;
	struct ceph_x_ticket_handler *th;
	int ret = 0;
	struct ceph_x_authorize_reply reply;
	void *p = au->reply_buf;
	void *end = p + sizeof(au->reply_buf);

	th = get_ticket_handler(ac, au->service);
	if (IS_ERR(th))
		return PTR_ERR(th);
	ret = ceph_x_decrypt(&th->session_key, &p, end, &reply, sizeof(reply));
	if (ret < 0)
		return ret;
	if (ret != sizeof(reply))
		return -EPERM;

	if (au->nonce + 1 != le64_to_cpu(reply.nonce_plus_one))
		ret = -EPERM;
	else
		ret = 0;
	dout("verify_authorizer_reply nonce %llx got %llx ret %d\n",
	     au->nonce, le64_to_cpu(reply.nonce_plus_one), ret);
	return ret;
}

static void ceph_x_destroy_authorizer(struct ceph_auth_client *ac,
				      struct ceph_authorizer *a)
{
	struct ceph_x_authorizer *au = (void *)a;

	ceph_buffer_put(au->buf);
	kfree(au);
}


static void ceph_x_reset(struct ceph_auth_client *ac)
{
	struct ceph_x_info *xi = ac->private;

	dout("reset\n");
	xi->starting = true;
	xi->server_challenge = 0;
}

static void ceph_x_destroy(struct ceph_auth_client *ac)
{
	struct ceph_x_info *xi = ac->private;
	struct rb_node *p;

	dout("ceph_x_destroy %p\n", ac);
	ceph_crypto_key_destroy(&xi->secret);

	while ((p = rb_first(&xi->ticket_handlers)) != NULL) {
		struct ceph_x_ticket_handler *th =
			rb_entry(p, struct ceph_x_ticket_handler, node);
		remove_ticket_handler(ac, th);
	}

	if (xi->auth_authorizer.buf)
		ceph_buffer_put(xi->auth_authorizer.buf);

	kfree(ac->private);
	ac->private = NULL;
}

static void ceph_x_invalidate_authorizer(struct ceph_auth_client *ac,
				   int peer_type)
{
	struct ceph_x_ticket_handler *th;

	th = get_ticket_handler(ac, peer_type);
	if (!IS_ERR(th))
		remove_ticket_handler(ac, th);
}


static const struct ceph_auth_client_ops ceph_x_ops = {
	.name = "x",
	.is_authenticated = ceph_x_is_authenticated,
	.should_authenticate = ceph_x_should_authenticate,
	.build_request = ceph_x_build_request,
	.handle_reply = ceph_x_handle_reply,
	.create_authorizer = ceph_x_create_authorizer,
	.verify_authorizer_reply = ceph_x_verify_authorizer_reply,
	.destroy_authorizer = ceph_x_destroy_authorizer,
	.invalidate_authorizer = ceph_x_invalidate_authorizer,
	.reset =  ceph_x_reset,
	.destroy = ceph_x_destroy,
};


int ceph_x_init(struct ceph_auth_client *ac)
{
	struct ceph_x_info *xi;
	int ret;

	dout("ceph_x_init %p\n", ac);
	ret = -ENOMEM;
	xi = kzalloc(sizeof(*xi), GFP_NOFS);
	if (!xi)
		goto out;

	ret = -EINVAL;
	if (!ac->secret) {
		pr_err("no secret set (for auth_x protocol)\n");
		goto out_nomem;
	}

	ret = ceph_crypto_key_unarmor(&xi->secret, ac->secret);
	if (ret)
		goto out_nomem;

	xi->starting = true;
	xi->ticket_handlers = RB_ROOT;

	ac->protocol = CEPH_AUTH_CEPHX;
	ac->private = xi;
	ac->ops = &ceph_x_ops;
	return 0;

out_nomem:
	kfree(xi);
out:
	return ret;
}

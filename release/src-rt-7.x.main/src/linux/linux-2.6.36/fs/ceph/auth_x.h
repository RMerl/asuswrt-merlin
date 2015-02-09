#ifndef _FS_CEPH_AUTH_X_H
#define _FS_CEPH_AUTH_X_H

#include <linux/rbtree.h>

#include "crypto.h"
#include "auth.h"
#include "auth_x_protocol.h"

/*
 * Handle ticket for a single service.
 */
struct ceph_x_ticket_handler {
	struct rb_node node;
	unsigned service;

	struct ceph_crypto_key session_key;
	struct ceph_timespec validity;

	u64 secret_id;
	struct ceph_buffer *ticket_blob;

	unsigned long renew_after, expires;
};


struct ceph_x_authorizer {
	struct ceph_buffer *buf;
	unsigned service;
	u64 nonce;
	char reply_buf[128];  /* big enough for encrypted blob */
};

struct ceph_x_info {
	struct ceph_crypto_key secret;

	bool starting;
	u64 server_challenge;

	unsigned have_keys;
	struct rb_root ticket_handlers;

	struct ceph_x_authorizer auth_authorizer;
};

extern int ceph_x_init(struct ceph_auth_client *ac);

#endif

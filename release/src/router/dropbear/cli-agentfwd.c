/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2005 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"

#ifdef ENABLE_CLI_AGENTFWD

#include "agentfwd.h"
#include "session.h"
#include "ssh.h"
#include "dbutil.h"
#include "chansession.h"
#include "channel.h"
#include "packet.h"
#include "buffer.h"
#include "random.h"
#include "listener.h"
#include "runopts.h"
#include "atomicio.h"
#include "signkey.h"
#include "auth.h"

/* The protocol implemented to talk to OpenSSH's SSH2 agent is documented in
   PROTOCOL.agent in recent OpenSSH source distributions (5.1p1 has it). */

static int new_agent_chan(struct Channel * channel);

const struct ChanType cli_chan_agent = {
	0, /* sepfds */
	"auth-agent@openssh.com",
	new_agent_chan,
	NULL,
	NULL,
	NULL
};

static int connect_agent() {

	int fd = -1;
	char* agent_sock = NULL;

	agent_sock = getenv("SSH_AUTH_SOCK");
	if (agent_sock == NULL)
		return -1;

	fd = connect_unix(agent_sock);

	if (fd < 0) {
		dropbear_log(LOG_INFO, "Failed to connect to agent");
	}

	return fd;
}

// handle a request for a connection to the locally running ssh-agent
// or forward.
static int new_agent_chan(struct Channel * channel) {

	int fd = -1;

	if (!cli_opts.agent_fwd)
		return SSH_OPEN_ADMINISTRATIVELY_PROHIBITED;

	fd = connect_agent();
	if (fd < 0) {
		return SSH_OPEN_CONNECT_FAILED;
	}

	setnonblocking(fd);

	ses.maxfd = MAX(ses.maxfd, fd);

	channel->readfd = fd;
	channel->writefd = fd;

	// success
	return 0;
}

/* Sends a request to the agent, returning a newly allocated buffer
 * with the response */
/* This function will block waiting for a response - it will
 * only be used by client authentication (not for forwarded requests)
 * won't cause problems for interactivity. */
/* Packet format (from draft-ylonen)
   4 bytes     Length, msb first.  Does not include length itself.
   1 byte      Packet type.  The value 255 is reserved for future extensions.
   data        Any data, depending on packet type.  Encoding as in the ssh packet
               protocol.
*/
static buffer * agent_request(unsigned char type, buffer *data) {

	buffer * payload = NULL;
	buffer * inbuf = NULL;
	size_t readlen = 0;
	ssize_t ret;
	const int fd = cli_opts.agent_fd;
	unsigned int data_len = 0;
	if (data)
	{
		data_len = data->len;
	}

	payload = buf_new(4 + 1 + data_len);

	buf_putint(payload, 1 + data_len);
	buf_putbyte(payload, type);
	if (data) {
		buf_putbytes(payload, data->data, data->len);
	}
	buf_setpos(payload, 0);

	ret = atomicio(write, fd, buf_getptr(payload, payload->len), payload->len);
	if ((size_t)ret != payload->len) {
		TRACE(("write failed fd %d for agent_request, %s", fd, strerror(errno)))
		goto out;
	}

	buf_free(payload);
	payload = NULL;
	TRACE(("Wrote out bytes for agent_request"))
	/* Now we read the response */
	inbuf = buf_new(4);
	ret = atomicio(read, fd, buf_getwriteptr(inbuf, 4), 4);
	if (ret != 4) {
		TRACE(("read of length failed for agent_request"))
		goto out;
	}
	buf_setpos(inbuf, 0);
	buf_setlen(inbuf, ret);

	readlen = buf_getint(inbuf);
	if (readlen > MAX_AGENT_REPLY) {
		TRACE(("agent reply is too big"));
		goto out;
	}
	
	TRACE(("agent_request readlen is %d", readlen))

	buf_resize(inbuf, readlen);
	buf_setpos(inbuf, 0);
	ret = atomicio(read, fd, buf_getwriteptr(inbuf, readlen), readlen);
	if ((size_t)ret != readlen) {
		TRACE(("read of data failed for agent_request"))
		goto out;
	}
	buf_incrwritepos(inbuf, readlen);
	buf_setpos(inbuf, 0);
	TRACE(("agent_request success, length %d", readlen))

out:
	if (payload)
		buf_free(payload);

	return inbuf;
}

static void agent_get_key_list(m_list * ret_list)
{
	buffer * inbuf = NULL;
	unsigned int num = 0;
	unsigned char packet_type;
	unsigned int i;
	int ret;

	inbuf = agent_request(SSH2_AGENTC_REQUEST_IDENTITIES, NULL);
	if (!inbuf) {
		TRACE(("agent_request failed returning identities"))
		goto out;
	}

	/* The reply has a format of:
		byte			SSH2_AGENT_IDENTITIES_ANSWER
		uint32			num_keys
  	   Followed by zero or more consecutive keys, encoded as:
       	 string			key_blob
    	 string			key_comment
	 */
	packet_type = buf_getbyte(inbuf);
	if (packet_type != SSH2_AGENT_IDENTITIES_ANSWER) {
		goto out;
	}

	num = buf_getint(inbuf);
	for (i = 0; i < num; i++) {
		sign_key * pubkey = NULL;
		int key_type = DROPBEAR_SIGNKEY_ANY;
		buffer * key_buf;

		/* each public key is encoded as a string */
		key_buf = buf_getstringbuf(inbuf);
		pubkey = new_sign_key();
		ret = buf_get_pub_key(key_buf, pubkey, &key_type);
		buf_free(key_buf);
		if (ret != DROPBEAR_SUCCESS) {
			/* This is slack, properly would cleanup vars etc */
			dropbear_exit("Bad pubkey received from agent");
		}
		pubkey->type = key_type;
		pubkey->source = SIGNKEY_SOURCE_AGENT;

		list_append(ret_list, pubkey);

		/* We'll ignore the comment for now. might want it later.*/
		buf_eatstring(inbuf);
	}

out:
	if (inbuf) {
		buf_free(inbuf);
		inbuf = NULL;
	}
}

void cli_setup_agent(struct Channel *channel) {
	if (!getenv("SSH_AUTH_SOCK")) {
		return;
	}
	
	cli_start_send_channel_request(channel, "auth-agent-req@openssh.com");
	/* Don't want replies */
	buf_putbyte(ses.writepayload, 0);
	encrypt_packet();
}

/* Returned keys are prepended to ret_list, which will
   be updated. */
void cli_load_agent_keys(m_list *ret_list) {
	/* agent_fd will be closed after successful auth */
	cli_opts.agent_fd = connect_agent();
	if (cli_opts.agent_fd < 0) {
		return;
	}

	agent_get_key_list(ret_list);
}

void agent_buf_sign(buffer *sigblob, sign_key *key, 
		const unsigned char *data, unsigned int len) {
	buffer *request_data = NULL;
	buffer *response = NULL;
	unsigned int siglen;
	int packet_type;
	
	/* Request format
	byte			SSH2_AGENTC_SIGN_REQUEST
	string			key_blob
	string			data
	uint32			flags
	*/
	request_data = buf_new(MAX_PUBKEY_SIZE + len + 12);
	buf_put_pub_key(request_data, key, key->type);
	
	buf_putstring(request_data, data, len);
	buf_putint(request_data, 0);
	
	response = agent_request(SSH2_AGENTC_SIGN_REQUEST, request_data);
	
	if (!response) {
		goto fail;
	}
	
	packet_type = buf_getbyte(response);
	if (packet_type != SSH2_AGENT_SIGN_RESPONSE) {
		goto fail;
	}
	
	/* Response format
	byte			SSH2_AGENT_SIGN_RESPONSE
	string			signature_blob
	*/
	siglen = buf_getint(response);
	buf_putbytes(sigblob, buf_getptr(response, siglen), siglen);
	goto cleanup;
	
fail:
	/* XXX don't fail badly here. instead propagate a failure code back up to
	   the cli auth pubkey code, and just remove this key from the list of 
	   ones to try. */
	dropbear_exit("Agent failed signing key");

cleanup:
	if (request_data) {
		buf_free(request_data);
	}
	if (response) {
		buf_free(response);
	}
}

#endif

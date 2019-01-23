/*
 * ministun.c
 * Part of the ministun package
 *
 * STUN support code borrowed from Asterisk -- An open source telephony toolkit.
 * Copyright (C) 1999 - 2006, Digium, Inc.
 * Mark Spencer <markster@digium.com>
 * Standalone remake (c) 2009 Vladislav Grishenko <themiron@mail.ru>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 * 
 * This code provides some support for doing STUN transactions.
 * Eventually it should be moved elsewhere as other protocols
 * than RTP can benefit from it - e.g. SIP.
 * STUN is described in RFC3489 and it is based on the exchange
 * of UDP packets between a client and one or more servers to
 * determine the externally visible address (and port) of the client
 * once it has gone through the NAT boxes that connect it to the
 * outside.
 * The simplest request packet is just the header defined in
 * struct stun_header, and from the response we may just look at
 * one attribute, STUN_MAPPED_ADDRESS, that we find in the response.
 * By doing more transactions with different server addresses we
 * may determine more about the behaviour of the NAT boxes, of
 * course - the details are in the RFC.
 *
 * All STUN packets start with a simple header made of a type,
 * length (excluding the header) and a 16-byte random transaction id.
 * Following the header we may have zero or more attributes, each
 * structured as a type, length and a value (whose format depends
 * on the type, but often contains addresses).
 * Of course all fields are in network format.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "ministun.h"

#undef STUN_BINDREQ_PROCESS

struct stun_strings {
	const int value;
	const char *name;
};

static char *stun_local = "0.0.0.0";
static char *stun_server = STUN_SERVER;
static char *stun_iface = NULL;
static int stun_port = STUN_PORT;
static int stun_rto = STUN_RTO;
static int stun_mrc = STUN_MRC;
static int stun_debug = 0;

static inline int stun_msg2class(int msg)
{
	return ((msg & 0x0010) >> 4) | ((msg & 0x0100) >> 7);
}

static inline int stun_msg2method(int msg)
{
	return (msg & 0x000f) | ((msg & 0x00e0) >> 1) | ((msg & 0x3e00) >> 2);
}

static inline int stun_msg2type(int class, int method)
{
	return ((class & 1) << 4) | ((class & 2) << 7) |
		(method & 0x000f) | ((method & 0x0070) << 1) | ((method & 0x0f800) << 2);
}

/* helper function to print message names */
static const char *stun_msg2str(int msg)
{
	static const struct stun_strings classes[] = {
		{ STUN_REQUEST, "Request" },
		{ STUN_INDICATION, "Indication" },
		{ STUN_RESPONSE, "Response" },
		{ STUN_ERROR_RESPONSE, "Error Response" },
		{ 0, NULL }
	};
	static const struct stun_strings methods[] = {
		{ STUN_BINDING, "Binding" },
		{ 0, NULL }
	};
	static char result[32];
	const char *class = NULL, *method = NULL;
	int i, value;

	value = stun_msg2class(msg);
	for (i = 0; classes[i].name; i++) {
		class = classes[i].name;
		if (classes[i].value == value)
			break;
	}
	value = stun_msg2method(msg);
	for (i = 0; methods[i].name; i++) {
		method = methods[i].name;
		if (methods[i].value == value)
			break;
	}
	snprintf(result, sizeof(result), "%s %s",
		method ? : "Unknown Method",
		class ? : "Unknown Class Message");
	return result;
}

/* helper function to print attribute names */
static const char *stun_attr2str(int msg)
{
	static const struct stun_strings attrs[] = {
		{ STUN_MAPPED_ADDRESS, "Mapped Address" },
		{ STUN_RESPONSE_ADDRESS, "Response Address" },
		{ STUN_CHANGE_ADDRESS, "Change Address" },
		{ STUN_SOURCE_ADDRESS, "Source Address" },
		{ STUN_CHANGED_ADDRESS, "Changed Address" },
		{ STUN_USERNAME, "Username" },
		{ STUN_PASSWORD, "Password" },
		{ STUN_MESSAGE_INTEGRITY, "Message Integrity" },
		{ STUN_ERROR_CODE, "Error Code" },
		{ STUN_UNKNOWN_ATTRIBUTES, "Unknown Attributes" },
		{ STUN_REFLECTED_FROM, "Reflected From" },
		{ STUN_REALM, "Realm" },
		{ STUN_NONCE, "Nonce" },
		{ STUN_XOR_MAPPED_ADDRESS, "XOR Mapped Address" },
		{ STUN_MS_VERSION, "MS Version" },
		{ STUN_MS_XOR_MAPPED_ADDRESS, "MS XOR Mapped Address" },
		{ STUN_SOFTWARE, "Software" },
		{ STUN_ALTERNATE_SERVER, "Alternate Server" },
		{ STUN_FINGERPRINT, "Fingerprint" },
		{ 0, NULL }
	};
	int i;

	for (i = 0; attrs[i].name; i++) {
		if (attrs[i].value == msg)
			return attrs[i].name;
	}
	return "Unknown Attribute";
}

/* here we store credentials extracted from a message */
struct stun_state {
#ifdef STUN_BINDREQ_PROCESS
	const char *username;
	const char *password;
#endif
	unsigned short attr;
};

static int stun_process_attr(struct stun_state *state, struct stun_attr *attr)
{
	if (stun_debug)
		fprintf(stderr, "Found STUN Attribute %s (%04x), length %d\n",
			    stun_attr2str(ntohs(attr->attr)), ntohs(attr->attr), ntohs(attr->len));
	switch (ntohs(attr->attr)) {
#ifdef STUN_BINDREQ_PROCESS
	case STUN_USERNAME:
		state->username = (const char *) (attr->value);
		break;
	case STUN_PASSWORD:
		state->password = (const char *) (attr->value);
		break;
#endif
	case STUN_MAPPED_ADDRESS:
	case STUN_XOR_MAPPED_ADDRESS:
	case STUN_MS_XOR_MAPPED_ADDRESS:
		break;
	default:
		if (stun_debug)
			fprintf(stderr, "Ignoring STUN Attribute %s (%04x), length %d\n", 
				    stun_attr2str(ntohs(attr->attr)), ntohs(attr->attr), ntohs(attr->len));
	}
	return 0;
}

/* append a string to an STUN message */
static void append_attr_string(struct stun_attr **attr, int attrval, const char *s, int *len, int *left)
{
	int str_length = strlen(s);
	int attr_length = str_length + ((~(str_length - 1)) & 0x3);
	int size = sizeof(**attr) + attr_length;
	if (*left > size) {
		(*attr)->attr = htons(attrval);
		(*attr)->len = htons(attr_length);
		memcpy((*attr)->value, s, str_length);
		memset((*attr)->value + str_length, 0, attr_length - str_length);
		(*attr) = (struct stun_attr *)((*attr)->value + attr_length);
		*len += size;
		*left -= size;
	}
}

#ifdef STUN_BINDREQ_PROCESS
/* append an address to an STUN message */
static void append_attr_address(struct stun_attr **attr, int attrval, struct sockaddr_in *sock_in, int *len, int *left, unsigned int magic)
{
	int size = sizeof(**attr) + 8;
	struct stun_addr *addr;
	if (*left > size) {
		(*attr)->attr = htons(attrval);
		(*attr)->len = htons(8);
		addr = (struct stun_addr *)((*attr)->value);
		addr->unused = 0;
		addr->family = 0x01;
		addr->port = sock_in->sin_port ^ htons(ntohl(magic) >> 16);
		addr->addr = sock_in->sin_addr.s_addr ^ magic;
		(*attr) = (struct stun_attr *)((*attr)->value + 8);
		*len += size;
		*left -= size;
	}
}
#endif

/* wrapper to send an STUN message */
static int stun_send(int s, struct sockaddr_in *dst, struct stun_header *resp)
{
	return sendto(s, resp, ntohs(resp->msglen) + sizeof(*resp), 0,
		      (struct sockaddr *)dst, sizeof(*dst));
}

/* helper function to generate a random request id */
static void stun_req_id(struct stun_header *req)
{
	int x;
	srandom(time(0));
	req->magic = htonl(STUN_MAGIC_COOKIE);
	for (x = 0; x < 3; x++)
		req->id.id[x] = random();
}

/* callback type to be invoked on stun responses. */
typedef int (stun_cb_f)(struct stun_state *st, struct stun_attr *attr, void *arg, unsigned int magic);

/* handle an incoming STUN message.
 *
 * Do some basic sanity checks on packet size and content,
 * try to extract a bit of information, and possibly reply.
 * At the moment this only processes BIND requests, and returns
 * the externally visible address of the request.
 * If a callback is specified, invoke it with the attribute.
 */
static int stun_handle_packet(int s, struct sockaddr_in *src,
	unsigned char *data, size_t len, stun_cb_f *stun_cb, void *arg)
{
	struct stun_header *hdr = (struct stun_header *)data;
	struct stun_attr *attr;
	struct stun_state st;
	int ret = STUN_IGNORE;
	int x;

	/* On entry, 'len' is the length of the udp payload. After the
	 * initial checks it becomes the size of unprocessed options,
	 * while 'data' is advanced accordingly.
	 */
	if (len < sizeof(struct stun_header)) {
		fprintf(stderr, "Runt STUN packet (only %d, wanting at least %d)\n", (int) len, (int) sizeof(struct stun_header));
		return -1;
	}
	len -= sizeof(struct stun_header);
	data += sizeof(struct stun_header);
	x = ntohs(hdr->msglen);	/* len as advertised in the message */
	if (stun_debug)
		fprintf(stderr, "STUN Packet, msg %s (%04x), length: %d\n", stun_msg2str(ntohs(hdr->msgtype)), ntohs(hdr->msgtype), x);
	if (x > len) {
		fprintf(stderr, "Scrambled STUN packet length (got %d, expecting %d)\n", x, (int)len);
	} else
		len = x;
	bzero(&st, sizeof(st));
	while (len) {
		if (len < sizeof(struct stun_attr)) {
			fprintf(stderr, "Runt Attribute (got %d, expecting %d)\n", (int)len, (int) sizeof(struct stun_attr));
			break;
		}
		attr = (struct stun_attr *)data;
		/* compute total attribute length */
		x = ntohs(attr->len) + sizeof(struct stun_attr);
		if (x > len) {
			fprintf(stderr, "Inconsistent Attribute (length %d exceeds remaining msg len %d)\n", x, (int)len);
			break;
		}
		if (stun_cb)
			stun_cb(&st, attr, arg, hdr->magic);
		if (stun_process_attr(&st, attr)) {
			fprintf(stderr, "Failed to handle attribute %s (%04x)\n", stun_attr2str(ntohs(attr->attr)), ntohs(attr->attr));
			break;
		}
		/* Clear attribute id: in case previous entry was a string,
		 * this will act as the terminator for the string.
		 */
		attr->attr = 0;
		data += x;
		len -= x;
	}
	/* Null terminate any string.
	 * XXX NOTE, we write past the size of the buffer passed by the
	 * caller, so this is potentially dangerous. The only thing that
	 * saves us is that usually we read the incoming message in a
	 * much larger buffer
	 */
	*data = '\0';

	/* Now prepare to generate a reply, which at the moment is done
	 * only for properly formed (len == 0) STUN_BINDREQ messages.
	 */

#ifdef STUN_BINDREQ_PROCESS
	if (len == 0) {
		unsigned char respdata[1024];
		struct stun_header *resp = (struct stun_header *)respdata;
		int resplen = 0;	// len excluding header
		int respleft = sizeof(respdata) - sizeof(struct stun_header);
		int class, method;

		resp->magic = hdr->magic;
		resp->id = hdr->id;
		resp->msgtype = 0;
		resp->msglen = 0;
		attr = (struct stun_attr *)resp->ies;
		class = stun_msg2class(ntohs(hdr->msgtype));
		method = stun_msg2method(ntohs(hdr->msgtype));
		if (class == STUN_REQUEST && method == STUN_BINDING) {
			if (stun_debug)
				fprintf(stderr, "STUN %s, username: %s\n",
					stun_msg2str(ntohs(hdr->msgtype)), st.username ? : "<none>");
			if (st.username)
				append_attr_string(&attr, STUN_USERNAME, st.username, &resplen, &respleft);
			if (resp->magic == htonl(STUN_MAGIC_COOKIE))
				append_attr_address(&attr, STUN_XOR_MAPPED_ADDRESS, src, &resplen, &respleft, hdr->magic);
			append_attr_address(&attr, STUN_MAPPED_ADDRESS, src, &resplen, &respleft, 0);
			append_attr_string(&attr, STUN_SOFTWARE, PACKAGE " v" VERSION, &reqlen, &reqleft);
			resp->msglen = htons(resplen);
			resp->msgtype = htons(stun_msg2type(STUN_RESPONSE, STUN_BINDING));
			stun_send(s, src, resp);
			ret = STUN_ACCEPT;
		} else if (class != STUN_RESPONSE || method != STUN_BINDING) {
			if (stun_debug)
				fprintf(stderr, "Dunno what to do with STUN message %04x (%s)\n",
					ntohs(hdr->msgtype), stun_msg2str(ntohs(hdr->msgtype)));
		}
	}
#endif
	return ret;
}

/* Extract the STUN_MAPPED_ADDRESS from the stun response.
 * This is used as a callback for stun_handle_response
 * when called from stun_request.
 */
static int stun_get_mapped(struct stun_state *st, struct stun_attr *attr, void *arg, unsigned int magic)
{
	struct stun_addr *addr = (struct stun_addr *)(attr + 1);
	struct sockaddr_in *sa = (struct sockaddr_in *)arg;
	unsigned short type = ntohs(attr->attr);

	switch (type) {
	case STUN_MAPPED_ADDRESS:
		if (st->attr == STUN_XOR_MAPPED_ADDRESS ||
		    st->attr == STUN_MS_XOR_MAPPED_ADDRESS)
			return 1;
		magic = 0;
		break;
	case STUN_MS_XOR_MAPPED_ADDRESS:
		if (st->attr == STUN_XOR_MAPPED_ADDRESS)
			return 1;
		break;
	case STUN_XOR_MAPPED_ADDRESS:
		break;
	default:
		return 1;
	}
	if (ntohs(attr->len) < 8 && addr->family != 1)
		return 1;

	st->attr = type;
	sa->sin_port = addr->port ^ htons(ntohl(magic) >> 16);
	sa->sin_addr.s_addr = addr->addr ^ magic;
	return 0;
}

/* Generic STUN request
 * Send a generic stun request to the server specified,
 * possibly waiting for a reply and filling the 'reply' field with
 * the externally visible address. Note that in this case the request
 * will be blocking.
 * (Note, the interface may change slightly in the future).
 *
 * \param s the socket used to send the request
 * \param dst the address of the STUN server
 * \param username if non null, add the username in the request
 * \param answer if non null, the function waits for a response and
 *    puts here the externally visible address.
 * \return 0 on success, other values on error.
 */
int stun_request(int s, struct sockaddr_in *dst,
	const char *username, struct sockaddr_in *answer)
{
	struct stun_header *req;
	unsigned char reqdata[1024];
	int reqlen, reqleft;
	struct stun_attr *attr;
	int res = 0;
	int retry, timeout = stun_rto;

	req = (struct stun_header *)reqdata;
	stun_req_id(req);
	reqlen = 0;
	reqleft = sizeof(reqdata) - sizeof(struct stun_header);
	req->msgtype = 0;
	req->msglen = 0;
	attr = (struct stun_attr *)req->ies;
#ifdef STUN_BINDREQ_PROCESS
	if (username)
		append_attr_string(&attr, STUN_USERNAME, username, &reqlen, &reqleft);
#endif
	append_attr_string(&attr, STUN_SOFTWARE, PACKAGE " v" VERSION, &reqlen, &reqleft);
	req->msglen = htons(reqlen);
	req->msgtype = htons(stun_msg2type(STUN_REQUEST, STUN_BINDING));
	for (retry = 0; retry < stun_mrc; retry++) {
		/* send request, possibly wait for reply */
		unsigned char reply_buf[1024];
		fd_set rfds;
		struct timeval to = { timeout / 1000, (timeout % 1000) * 1000 };
		struct sockaddr_in src;
		socklen_t srclen;

		do {
			res = stun_send(s, dst, req);
		} while (res < 0 && errno == EINTR);
		if (res < 0) {
			fprintf(stderr, "Request send #%d failed error %d, retry\n",
				retry, res);
			continue;
		}
		if (answer == NULL)
			break;
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		do {
			res = select(s + 1, &rfds, NULL, NULL, &to);
		} while (res < 0 && errno == EINTR);
		if (res == 0) {	/* timeout */
			fprintf(stderr, "Response read #%d timeout, retry\n", retry);
			timeout *= 2;
			continue;
		} else if (res < 0) { /* error */
			fprintf(stderr, "Response read #%d failed error %d, retry\n",
				retry, res);
			continue;
		}
		bzero(&src, sizeof(src));
		srclen = sizeof(src);
		/* XXX pass -1 in the size, because stun_handle_packet might
		 * write past the end of the buffer.
		 */
		do  {
			res = recvfrom(s, reply_buf, sizeof(reply_buf) - 1,
				0, (struct sockaddr *)&src, &srclen);
		} while (res < 0 && errno == EINTR);
		if (res <= 0) {
			fprintf(stderr, "Response read #%d failed error %d, retry\n",
				retry, res);
			continue;
		}
		bzero(answer, sizeof(struct sockaddr_in));
		stun_handle_packet(s, &src, reply_buf, res, stun_get_mapped, answer);
		return 0;
	}
	return -1;
}

static void usage(char *name)
{
	fprintf(stderr, "Minimalistic STUN client v%s\n", VERSION);
	fprintf(stderr, "Usage: %s [-t timeout ms] [-c count] [-l local ip] [-d] [stun_server[:port]]\n", PACKAGE);
}

int main(int argc, char *argv[])
{
	int sock, opt, res;
	struct sockaddr_in server,client,mapped;
	struct hostent *hostinfo;
	char *value;

	while ((opt = getopt(argc, argv, "t:c:l:dhi:")) != -1) {
		switch (opt) {
		case 'i':
			stun_iface = optarg;
			break;
		case 't':
			stun_rto = atoi(optarg);
			break;
		case 'c':
			stun_mrc = atoi(optarg);
			break;
		case 'l':
			stun_local = optarg;
			break;
		case 'd':
			stun_debug++;
			break;
		case 'h':
		default:
			usage(argv[0]);
			return -1;
		}
	}

	if (optind < argc) {
		value = argv[optind];
		stun_server = strsep(&value, ":");
		if (value && *value)
			stun_port = atoi(value);
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		fprintf(stderr, "Error creating socket\n");
		return -1;
	}

	bzero(&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = 0;
	if (inet_aton(stun_local, &client.sin_addr) == 0 ||
	    bind(sock, (struct sockaddr*) &client, sizeof(client)) < 0) {
		fprintf(stderr, "Error bind to socket\n");
		close(sock);
		return -1;
	}

	if (stun_iface) {
		int r;

		r = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, stun_iface, IFNAMSIZ);
		if (r == -1) {
			fprintf(stderr, "Bind socket to device [%s] failed! errno %d (%s)\n",
				stun_iface, errno, strerror(errno));
		}
	}

	hostinfo = gethostbyname(stun_server);
	if (!hostinfo) {
		fprintf(stderr, "Error resolving host %s\n", stun_server);
		close(sock);
		return -1;
	}
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr = *(struct in_addr*) hostinfo->h_addr;
	server.sin_port = htons(stun_port);

	res = stun_request(sock, &server, NULL, &mapped);
	close(sock);

	if (res == 0 && mapped.sin_addr.s_addr == INADDR_ANY)
		res = -1;
	if (res == 0)
		printf("%s\n", inet_ntoa(mapped.sin_addr));

	return res;
}


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"
#include "radius.h"
#include "md5.h"
#include "rtdot1x.h"

struct radius_msg *Radius_msg_new(u8 code, u8 identifier)
{
	struct radius_msg *msg;

	msg = (struct radius_msg *) malloc(sizeof(*msg));
	if (msg == NULL)
		return NULL;

	if (Radius_msg_initialize(msg, RADIUS_DEFAULT_MSG_SIZE))
	{
		free(msg);
		return NULL;
	}

	Radius_msg_set_hdr(msg, code, identifier);

	return msg;
}

int Radius_msg_initialize(struct radius_msg *msg, size_t init_len)
{
	if (msg == NULL || init_len < sizeof(struct radius_hdr))
		return -1;

	memset(msg, 0, sizeof(*msg));
	msg->buf = (unsigned char *) malloc(init_len);
	if (msg->buf == NULL)
		return -1;
	memset(msg->buf, 0, init_len);

	msg->buf_size = init_len;
	msg->hdr = (struct radius_hdr *) msg->buf;
	msg->buf_used = sizeof(*msg->hdr);

	msg->attrs = (struct radius_attr_hdr **)
		malloc(RADIUS_DEFAULT_ATTR_COUNT * sizeof(*msg->attrs));
	if (msg->attrs == NULL)
	{
		free(msg->buf);
		msg->buf = NULL;
		msg->hdr = NULL;
		return -1;
	}

	msg->attr_size = RADIUS_DEFAULT_ATTR_COUNT;
	msg->attr_used = 0;

	return 0;
}

void Radius_msg_set_hdr(struct radius_msg *msg, u8 code, u8 identifier)
{
	msg->hdr->code = code;
	msg->hdr->identifier = identifier;
}

void Radius_msg_free(struct radius_msg *msg)
{
	if (msg->buf != NULL)
	{
		free(msg->buf);
		msg->buf = NULL;
		msg->hdr = NULL;
	}
	msg->buf_size = msg->buf_used = 0;

	if (msg->attrs != NULL)
	{
		free(msg->attrs);
		msg->attrs = NULL;
	}
	msg->attr_size = msg->attr_used = 0;
}


struct radius_attr_type {
	u8 type;
	char *name;
	enum { RADIUS_ATTR_UNDIST, RADIUS_ATTR_TEXT, RADIUS_ATTR_IP,
	       RADIUS_ATTR_HEXDUMP, RADIUS_ATTR_INT32 } data_type;
};

void Radius_msg_finish(struct radius_msg *msg, u8 *secret, size_t secret_len)
{
	if (secret)
	{
		u8 auth[MD5_MAC_LEN];
		struct radius_attr_hdr *attr;

		memset(auth, 0, MD5_MAC_LEN);
		attr = Radius_msg_add_attr(msg, RADIUS_ATTR_MESSAGE_AUTHENTICATOR, auth, MD5_MAC_LEN);
		msg->hdr->length = htons(msg->buf_used);
		hmac_md5(secret, secret_len, msg->buf, msg->buf_used, (u8 *) (attr + 1));
	}
	else
		msg->hdr->length = htons(msg->buf_used);

	if (msg->buf_used > 0xffff)
	{
		DBGPRINT(RT_DEBUG_ERROR,"WARNING: too long RADIUS messages (%d)\n", msg->buf_used);
	}
}

static int Radius_msg_add_attr_to_array(struct radius_msg *msg, struct radius_attr_hdr *attr)
{
	if (msg->attr_used >= msg->attr_size)
	{
		struct radius_attr_hdr **nattrs;
		int nlen = msg->attr_size * 2;

		nattrs = (struct radius_attr_hdr **) realloc(msg->attrs, nlen * sizeof(*msg->attrs));

		if (nattrs == NULL)
			return -1;

		msg->attrs = nattrs;
		msg->attr_size = nlen;
	}

	msg->attrs[msg->attr_used++] = attr;

	return 0;
}

struct radius_attr_hdr *Radius_msg_add_attr(struct radius_msg *msg, u8 type, u8 *data, size_t data_len)
{
	size_t buf_needed;
	struct radius_attr_hdr *attr;

	if (data_len > RADIUS_MAX_ATTR_LEN)
	{
		DBGPRINT(RT_DEBUG_ERROR,"radius_msg_add_attr: too long attribute (%d bytes)\n", data_len);
		return NULL;
	}

	buf_needed = msg->buf_used + sizeof(*attr) + data_len;

	if (msg->buf_size < buf_needed)
	{
		/* allocate more space for message buffer */
		unsigned char *nbuf;
		int nlen = msg->buf_size;
		int diff, i;

		while (nlen < buf_needed)
			nlen *= 2;
		nbuf = (unsigned char *) realloc(msg->buf, nlen);
		if (nbuf == NULL)
			return NULL;
		diff = nbuf - msg->buf;
		msg->buf = nbuf;
		msg->hdr = (struct radius_hdr *) msg->buf;
		
		/* adjust attr pointers to match with the new buffer */
		for (i = 0; i < msg->attr_used; i++)
			msg->attrs[i] = (struct radius_attr_hdr *) (((u8 *) msg->attrs[i]) + diff);
		memset(msg->buf + msg->buf_size, 0, nlen - msg->buf_size);
		msg->buf_size = nlen;
	}

	attr = (struct radius_attr_hdr *) (msg->buf + msg->buf_used);
	attr->type = type;
	attr->length = sizeof(*attr) + data_len;
	if (data_len > 0)
		memcpy(attr + 1, data, data_len);

	msg->buf_used += sizeof(*attr) + data_len;

	if (Radius_msg_add_attr_to_array(msg, attr))
		return NULL;

	return attr;
}

struct radius_msg *Radius_msg_parse(const u8 *data, size_t len)
{
	struct radius_msg *msg;
	struct radius_hdr *hdr;
	struct radius_attr_hdr *attr;
	size_t msg_len;
	unsigned char *pos, *end;

	if (data == NULL || len < sizeof(*hdr))
		return NULL;

	hdr = (struct radius_hdr *) data;

	msg_len = ntohs(hdr->length);
	if (msg_len < sizeof(*hdr) || msg_len > len)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Invalid RADIUS message length\n");
		return NULL;
	}

	if (msg_len < len)
	{
		DBGPRINT(RT_DEBUG_INFO,"Ignored %d extra bytes after RADIUS message\n", len - msg_len);
	}

	msg = (struct radius_msg *) malloc(sizeof(*msg));
	if (msg == NULL)
		return NULL;

	if (Radius_msg_initialize(msg, msg_len))
	{
		free(msg);
		return NULL;
	}

	memcpy(msg->buf, data, msg_len);
	msg->buf_size = msg->buf_used = msg_len;

	/* parse attributes */
	pos = (unsigned char *) (msg->hdr + 1);
	end = msg->buf + msg->buf_used;
	while (pos < end)
	{
		if (end - pos < sizeof(*attr))
			goto fail;

		attr = (struct radius_attr_hdr *) pos;

		if (pos + attr->length > end || attr->length < sizeof(*attr))
			goto fail;

		/* TODO: check that attr->length is suitable for attr->type */

		if (Radius_msg_add_attr_to_array(msg, attr))
			goto fail;

		pos += attr->length;
	}

	return msg;

 fail:
	Radius_msg_free(msg);
	free(msg);
	return NULL;
}

int Radius_msg_add_eap(struct radius_msg *msg, u8 *data, size_t data_len)
{
	u8 *pos = data;
	size_t left = data_len;

	while (left > 0)
	{
		int len;
		if (left > RADIUS_MAX_ATTR_LEN)
			len = RADIUS_MAX_ATTR_LEN;
		else
			len = left;

		if (!Radius_msg_add_attr(msg, RADIUS_ATTR_EAP_MESSAGE, pos, len))
			return 0;

		pos += len;
		left -= len;
	}

	return 1;
}

u8 *Radius_msg_get_eap(struct radius_msg *msg, size_t *eap_len)
{
	u8 *eap, *pos;
	size_t len;
	int i;

	if (msg == NULL)
		return NULL;

	len = 0;
	for (i = 0; i < msg->attr_used; i++)
	{
		if (msg->attrs[i]->type == RADIUS_ATTR_EAP_MESSAGE)
			len += msg->attrs[i]->length - sizeof(struct radius_attr_hdr);
	}

	if (len == 0)
		return NULL;

	eap = (u8 *) malloc(len);
	if (eap == NULL)
		return NULL;

	pos = eap;
	for (i = 0; i < msg->attr_used; i++)
	{
		if (msg->attrs[i]->type == RADIUS_ATTR_EAP_MESSAGE)
		{
			struct radius_attr_hdr *attr = msg->attrs[i];
			int flen = attr->length - sizeof(*attr);
			memcpy(pos, attr + 1, flen);
			pos += flen;
		}
	}

	if (eap_len)
		*eap_len = len;

	return eap;
}

int Radius_msg_verify(struct radius_msg *msg, u8 *secret, size_t secret_len, struct radius_msg *sent_msg)
{
	u8 auth[MD5_MAC_LEN], orig[MD5_MAC_LEN];
	u8 orig_authenticator[16];
	struct radius_attr_hdr *attr = NULL;
	int i;
	MD5_CTX context;
	u8 hash[MD5_MAC_LEN];

	if (sent_msg == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR,"No matching Access-Request message found\n");
		return 1;
	}

	for (i = 0; i < msg->attr_used; i++)
	{
		if (msg->attrs[i]->type == RADIUS_ATTR_MESSAGE_AUTHENTICATOR)
		{
			if (attr != 0)
			{
				DBGPRINT(RT_DEBUG_ERROR,"Multiple Message-Authenticator attributes in RADIUS message\n");
				return 1;
			}
			attr = msg->attrs[i];
		}
	}

	if (attr == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR,"No Message-Authenticator attribute found\n");
		return 1;
	}

	memcpy(orig, attr + 1, MD5_MAC_LEN);
	memset(attr + 1, 0, MD5_MAC_LEN);
	memcpy(orig_authenticator, msg->hdr->authenticator, sizeof(orig_authenticator));
	memcpy(msg->hdr->authenticator, sent_msg->hdr->authenticator, sizeof(msg->hdr->authenticator));
	hmac_md5(secret, secret_len, msg->buf, msg->buf_used, auth);
	memcpy(attr + 1, orig, MD5_MAC_LEN);
	memcpy(msg->hdr->authenticator, orig_authenticator, sizeof(orig_authenticator));

	if (memcmp(orig, auth, MD5_MAC_LEN) != 0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Invalid Message-Authenticator!\n");
		return 1;
	}

	/* ResponseAuth = MD5(Code+ID+Length+RequestAuth+Attributes+Secret) */
	MD5Init(&context);
	MD5Update(&context, (u8 *) msg->hdr, 1 + 1 + 2);
	MD5Update(&context, sent_msg->hdr->authenticator, MD5_MAC_LEN);
	MD5Update(&context, (u8 *) (msg->hdr + 1), msg->buf_used - sizeof(*msg->hdr));
	MD5Update(&context, secret, secret_len);
	MD5Final(hash, &context);
	if (memcmp(hash, msg->hdr->authenticator, MD5_MAC_LEN) != 0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Response Authenticator invalid!\n");
		return 1;
	}
	return 0;

}

int Radius_msg_verify_acct(struct radius_msg *msg, u8 *secret,
			   size_t secret_len, struct radius_msg *sent_msg)
{
	MD5_CTX context;
	u8 hash[MD5_MAC_LEN];

	MD5Init(&context);
	MD5Update(&context, msg->buf, 4);
	MD5Update(&context, sent_msg->hdr->authenticator, MD5_MAC_LEN);
	if (msg->buf_used > sizeof(struct radius_hdr))
		MD5Update(&context, msg->buf + sizeof(struct radius_hdr), msg->buf_used - sizeof(struct radius_hdr));
	MD5Update(&context, secret, secret_len);
	MD5Final(hash, &context);
	if (memcmp(hash, msg->hdr->authenticator, MD5_MAC_LEN) != 0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Response Authenticator invalid!\n");
		return 1;
	}

	return 0;
}

int Radius_msg_copy_attr(struct radius_msg *dst, struct radius_msg *src, u8 type)
{
	struct radius_attr_hdr *attr = NULL;
	int i;

	for (i = 0; i < src->attr_used; i++)
	{
		if (src->attrs[i]->type == type)
		{
			attr = src->attrs[i];
			break;
		}
	}

	if (attr == NULL)
		return 0;

	if (!Radius_msg_add_attr(dst, type, (u8 *) (attr + 1), attr->length - sizeof(*attr)))
		return -1;

	return 1;
}

/* Create Request Authenticator. The value should be unique over the lifetime
 * of the shared secret between authenticator and authentication server.
 * Use one-way MD5 hash calculated from current timestamp and some data given
 * by the caller. */
void Radius_msg_make_authenticator(struct radius_msg *msg, u8 *data, size_t len)
{
	struct timeval tv;
	MD5_CTX context;
	long int l;

	gettimeofday(&tv, NULL);
	l = random();
	MD5Init(&context);
	MD5Update(&context, (u8 *) &tv, sizeof(tv));
	MD5Update(&context, data, len);
	MD5Update(&context, (u8 *) &l, sizeof(l));
	MD5Final(msg->hdr->authenticator, &context);
}

/* Get Microsoft Vendor-specific RADIUS Attribute from a parsed RADIUS message.
 * Returns the Attribute payload and sets alen to indicate the length of the
 * payload if a vendor attribute with ms_type is found, otherwise returns NULL.
 * The returned payload is allocated with malloc() and caller must free it.
 */
static u8 *Radius_msg_get_ms_attr(struct radius_msg *msg, u8 ms_type, size_t *alen)
{
	u8 *data, *pos;
	int i;
	size_t len;

	if (msg == NULL)
		return NULL;

	for (i = 0; i < msg->attr_used; i++)
	{
		struct radius_attr_hdr *attr = msg->attrs[i];
		int left;
		u32 vendor_id;
		struct radius_attr_vendor_microsoft *ms;

		if (attr->type != RADIUS_ATTR_VENDOR_SPECIFIC)
			continue;

		left = attr->length - sizeof(*attr);
		if (left < 4)
			continue;

		pos = (u8 *) (attr + 1);

		memcpy(&vendor_id, pos, 4);
		pos += 4;
		left -= 4;

		if (ntohl(vendor_id) != RADIUS_VENDOR_ID_MICROSOFT)
			continue;

		while (left >= sizeof(*ms))
		{
			ms = (struct radius_attr_vendor_microsoft *) pos;
			if (ms->vendor_length > left)
			{
				left = 0;
				continue;
			}
			if (ms->vendor_type != ms_type)
			{
				pos += ms->vendor_length;
				left -= ms->vendor_length;
				continue;
			}

			len = ms->vendor_length - sizeof(*ms);
			data = (u8 *) malloc(len);
			if (data == NULL)
				return NULL;
			memcpy(data, pos + sizeof(*ms), len);
			if (alen)
				*alen = len;
			return data;
		}
	}

	return NULL;
}

static u8 * decrypt_ms_key(u8 *key, size_t len, struct radius_msg *sent_msg,
			   u8 *secret, size_t secret_len, size_t *reslen)
{
	u8 *pos, *plain, *ppos, *res;
	size_t left, plen;
	u8 hash[MD5_MAC_LEN];
	MD5_CTX context;
	int i, first = 1;

	/* key: 16-bit salt followed by encrypted key info */

	if (len < 2 + 16)
		return NULL;

	pos = key + 2;
	left = len - 2;
	if (left % 16)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Invalid ms key len\n");
		return NULL;
	}

	plen = left;
	ppos = plain = malloc(plen);
	if (plain == NULL)
		return NULL;

	while (left > 0)
	{
		/* b(1) = MD5(Secret + Request-Authenticator + Salt)
		 * b(i) = MD5(Secret + c(i - 1)) for i > 1 */

		MD5Init(&context);
		MD5Update(&context, secret, secret_len);
		if (first)
		{
			MD5Update(&context, sent_msg->hdr->authenticator,
				  MD5_MAC_LEN);
			MD5Update(&context, key, 2); /* Salt */
			first = 0;
		} else
			MD5Update(&context, pos - MD5_MAC_LEN, MD5_MAC_LEN);
		MD5Final(hash, &context);

		for (i = 0; i < MD5_MAC_LEN; i++)
			*ppos++ = *pos++ ^ hash[i];
		left -= MD5_MAC_LEN;
	}

	if (plain[0] > plen - 1)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Failed to decrypt MPPE key\n");
		free(plain);
		return NULL;
	}

	res = malloc(plain[0]);
	if (res == NULL)
	{
		free(plain);
		return NULL;
	}
	memcpy(res, plain + 1, plain[0]);
	if (reslen)
		*reslen = plain[0];
	free(plain);
	return res;
}

struct radius_ms_mppe_keys *
Radius_msg_get_ms_keys(struct radius_msg *msg, struct radius_msg *sent_msg,
		       u8 *secret, size_t secret_len)
{
	u8 *key;
	size_t keylen;
	struct radius_ms_mppe_keys *keys;

	if (msg == NULL || sent_msg == NULL)
		return NULL;

	keys = (struct radius_ms_mppe_keys *) malloc(sizeof(*keys));
	if (keys == NULL)
		return NULL;

	memset(keys, 0, sizeof(*keys));

	key = Radius_msg_get_ms_attr(msg, RADIUS_VENDOR_ATTR_MS_MPPE_SEND_KEY, &keylen);
	if (key)
	{
		keys->send = decrypt_ms_key(key, keylen, sent_msg, secret, secret_len, &keys->send_len);
		free(key);
	}
    DBGPRINT(RT_DEBUG_INFO," secret_len = %d, secret= %s\n",secret_len,secret);
	key = Radius_msg_get_ms_attr(msg, RADIUS_VENDOR_ATTR_MS_MPPE_RECV_KEY, &keylen);
	if (key)
	{
		keys->recv = decrypt_ms_key(key, keylen, sent_msg, secret, secret_len, &keys->recv_len);
		free(key);
	}

	return keys;
}

/* Add User-Password attribute to a RADIUS message and encrypt it as specified
 * in RFC 2865, Chap. 5.2 */
struct radius_attr_hdr *
Radius_msg_add_attr_user_password(struct radius_msg *msg,
				  u8 *data, size_t data_len, u8 *secret, size_t secret_len)
{
	u8 buf[128];
	int padlen, i, pos;
	MD5_CTX context;
	size_t buf_len;
	u8 hash[16];

	if (data_len > 128)
		return NULL;

	memcpy(buf, data, data_len);
	buf_len = data_len;

	padlen = data_len % 16;
	if (padlen)
	{
		padlen = 16 - padlen;
		memset(buf + data_len, 0, padlen);
		buf_len += padlen;
	}

	MD5Init(&context);
	MD5Update(&context, secret, secret_len);
	MD5Update(&context, msg->hdr->authenticator, 16);
	MD5Final(hash, &context);

	for (i = 0; i < 16; i++)
		buf[i] ^= hash[i];
	pos = 16;

	while (pos < buf_len)
	{
		MD5Init(&context);
		MD5Update(&context, secret, secret_len);
		MD5Update(&context, &buf[pos - 16], 16);
		MD5Final(hash, &context);

		for (i = 0; i < 16; i++)
			buf[pos + i] ^= hash[i];

		pos += 16;
	}

	return Radius_msg_add_attr(msg, RADIUS_ATTR_USER_PASSWORD, buf, buf_len);
}

int Radius_msg_get_attr(struct radius_msg *msg, u8 type, u8 *buf, size_t len)
{
	int i;
	struct radius_attr_hdr *attr = NULL;
	size_t dlen;

	for (i = 0; i < msg->attr_used; i++)
	{
		if (msg->attrs[i]->type == type)
		{
			attr = msg->attrs[i];
			break;
		}
	}

	if (!attr)
		return -1;

	dlen = attr->length - sizeof(*attr);
	if (buf)
		memcpy(buf, (attr + 1), dlen > len ? len : dlen);
	return dlen;
}


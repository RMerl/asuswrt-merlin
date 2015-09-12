/*

	web (experimental)
	HTTP client request match
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/sock.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_web.h>

MODULE_AUTHOR("Jonathan Zarate");
MODULE_DESCRIPTION("HTTP client request match (experimental)");
MODULE_LICENSE("GPL");

#define LOG(...)	do { } while (0);

static int find(const char *data, const char *tail, const char *text)
{
	int n, o;
	int dlen;
	const char *p, *e;

	while ((data < tail) && (*data == ' ')) ++data;
	while ((tail > data) && (*(tail - 1) == ' ')) --tail;

	dlen = tail - data;

	// 012345
	// text
	// ^text
	// text$
	// ^text$
	// 012345

	while (*text) {
		n = o = strlen(text);
		if (*text == '^') {
			--n;
			if (*(text + n) == '$') {
				// exact
				--n;
				if ((dlen == n) && (memcmp(data, text + 1, n) == 0)) {
					LOG(KERN_INFO "matched %s\n", text);
					return 1;
				}
			}
			else {
				// begins with
				if ((dlen >= n) && (memcmp(data, text + 1, n) == 0)) {
					LOG(KERN_INFO "matched %s\n", text);
					return 1;
				}
			}
		}
		else if (*(text + n - 1) == '$') {
			// ends with
			--n;
			if (memcmp(tail - n, text, n) == 0) {
				LOG(KERN_INFO "matched %s\n", text);
				return 1;
			}
		}
		else {
			// contains
			p = data;
			e = tail - n;
			while (p <= e) {
				if (memcmp(p, text, n) == 0) {
					LOG(KERN_INFO "matched %s\n", text);
					return 1;
				}
				++p;
			}
		}

		text += o + 1;
	}
	return 0;
}

static inline const char *findend(const char *data, const char *tail, int min)
{
	int n = tail - data;
	if (n >= min) {
		while (data < tail) {
			if (*data == '\r') return data;
			++data;
		}
	}
	return NULL;
}


static bool
match(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct ipt_web_info *info = par->matchinfo;
	const int offset = par->fragoff;
	const struct iphdr *iph = ip_hdr(skb);
	const struct tcphdr *tcph = (void *)iph + iph->ihl * 4;
	const char *data;
	const char *tail;
	const char *p, *q;
	int doff, dlen;
	__u32 sig;

	if (offset != 0) return info->invert;

	doff = (tcph->doff * 4);
 	data = (void *)tcph + doff;
	dlen = ntohs(ip_hdr(skb)->tot_len);

	// POST / HTTP/1.0$$$$
	// GET / HTTP/1.0$$$$
	// 1234567890123456789
	if (dlen < 18) return info->invert;
	
	// "GET " or "POST"
	sig = *(__u32 *)data;
	if ((sig != __constant_htonl(0x47455420)) && (sig != __constant_htonl(0x504f5354))) {
		return info->invert;
	}
		
	tail = data + dlen;
	if (dlen > 1024) {
		dlen = 1024;
		tail = data + 1024;
	}

	// POST / HTTP/1.0$$$$
	// GET / HTTP/1.0$$$$	-- minimum
	// 0123456789012345678
	//      9876543210
	if (((p = findend(data + 14, tail, 18)) == NULL) || (memcmp(p - 9, " HTTP/", 6) != 0))
		return info->invert;

	switch (info->mode) {
	case IPT_WEB_HTTP:
		return !info->invert;
	case IPT_WEB_HORE:
		// entire request line, else host line
		if (find(data + 4, p - 9, info->text)) return !info->invert;
		break;
	case IPT_WEB_PATH:
		// left side of '?' or entire line
		q = data += 4;
		p -= 9;
		while ((q < p) && (*q != '?')) ++q;
		return find(data, q, info->text) ^ info->invert;
	case IPT_WEB_QUERY:
		// right side of '?' or none
		q = data + 4;
		p -= 9;
		while ((q < p) && (*q != '?')) ++q;
		if (q >= p) return info->invert;
		return find(q + 1, p, info->text) ^ info->invert;
	case IPT_WEB_RURI:
		// entire request line
		return find(data + 4, p - 9, info->text) ^ info->invert;
	default:
		// shutup compiler
		break;
	}

	// else, IPT_WEB_HOST

	while (1) {
		data = p + 2;				// skip previous \r\n
		p = findend(data, tail, 8);	// p = current line's \r
		if (p == NULL) return 0;

		if (memcmp(data, "Host: ", 6) == 0)
			return find(data + 6, p, info->text) ^ info->invert;
	}

	return !info->invert;
}

static int
checkentry(const struct xt_mtchk_param *par)
{
	return 0;
}

static struct xt_match web_match = {
	.name		= "web",
	.family		= AF_INET,
	.match		= &match,
	.matchsize	= sizeof(struct ipt_web_info),
	.checkentry	= &checkentry,
	.destroy	= NULL,
	.me		= THIS_MODULE
};

static int __init init(void)
{
	return xt_register_match(&web_match);
}

static void __exit fini(void)
{
	xt_unregister_match(&web_match);
}

module_init(init);
module_exit(fini);

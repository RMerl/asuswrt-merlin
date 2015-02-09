/*
 * Copyright (C)2002 USAGI/WIDE Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors
 *
 *	Mitsuru KANDA @USAGI       : IPv6 Support
 * 	Kazunori MIYAZAWA @USAGI   :
 * 	Kunihiro Ishiguro <kunihiro@ipinfusion.com>
 *
 * 	This file is derived from net/ipv4/esp.c
 */

#include <crypto/aead.h>
#include <crypto/authenc.h>
#include <linux/err.h>
#include <linux/module.h>
#include <net/ip.h>
#include <net/xfrm.h>
#include <net/esp.h>
#include <linux/scatterlist.h>
#include <linux/kernel.h>
#include <linux/pfkeyv2.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <net/icmp.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <linux/icmpv6.h>

struct esp_skb_cb {
	struct xfrm_skb_cb xfrm;
	void *tmp;
};

#define ESP_SKB_CB(__skb) ((struct esp_skb_cb *)&((__skb)->cb[0]))

/*
 * Allocate an AEAD request structure with extra space for SG and IV.
 *
 * For alignment considerations the IV is placed at the front, followed
 * by the request and finally the SG list.
 *
 * TODO: Use spare space in skb for this where possible.
 */
static void *esp_alloc_tmp(struct crypto_aead *aead, int nfrags)
{
	unsigned int len;

	len = crypto_aead_ivsize(aead);
	if (len) {
		len += crypto_aead_alignmask(aead) &
		       ~(crypto_tfm_ctx_alignment() - 1);
		len = ALIGN(len, crypto_tfm_ctx_alignment());
	}

	len += sizeof(struct aead_givcrypt_request) + crypto_aead_reqsize(aead);
	len = ALIGN(len, __alignof__(struct scatterlist));

	len += sizeof(struct scatterlist) * nfrags;

	return kmalloc(len, GFP_ATOMIC);
}

static inline u8 *esp_tmp_iv(struct crypto_aead *aead, void *tmp)
{
	return crypto_aead_ivsize(aead) ?
	       PTR_ALIGN((u8 *)tmp, crypto_aead_alignmask(aead) + 1) : tmp;
}

static inline struct aead_givcrypt_request *esp_tmp_givreq(
	struct crypto_aead *aead, u8 *iv)
{
	struct aead_givcrypt_request *req;

	req = (void *)PTR_ALIGN(iv + crypto_aead_ivsize(aead),
				crypto_tfm_ctx_alignment());
	aead_givcrypt_set_tfm(req, aead);
	return req;
}

static inline struct aead_request *esp_tmp_req(struct crypto_aead *aead, u8 *iv)
{
	struct aead_request *req;

	req = (void *)PTR_ALIGN(iv + crypto_aead_ivsize(aead),
				crypto_tfm_ctx_alignment());
	aead_request_set_tfm(req, aead);
	return req;
}

static inline struct scatterlist *esp_req_sg(struct crypto_aead *aead,
					     struct aead_request *req)
{
	return (void *)ALIGN((unsigned long)(req + 1) +
			     crypto_aead_reqsize(aead),
			     __alignof__(struct scatterlist));
}

static inline struct scatterlist *esp_givreq_sg(
	struct crypto_aead *aead, struct aead_givcrypt_request *req)
{
	return (void *)ALIGN((unsigned long)(req + 1) +
			     crypto_aead_reqsize(aead),
			     __alignof__(struct scatterlist));
}

static void esp_output_done(struct crypto_async_request *base, int err)
{
	struct sk_buff *skb = base->data;

	kfree(ESP_SKB_CB(skb)->tmp);
	xfrm_output_resume(skb, err);
}

static int esp6_output(struct xfrm_state *x, struct sk_buff *skb)
{
	int err;
	struct ip_esp_hdr *esph;
	struct crypto_aead *aead;
	struct aead_givcrypt_request *req;
	struct scatterlist *sg;
	struct scatterlist *asg;
	struct sk_buff *trailer;
	void *tmp;
	int blksize;
	int clen;
	int alen;
	int nfrags;
	u8 *iv;
	u8 *tail;
	struct esp_data *esp = x->data;

	/* skb is pure payload to encrypt */
	err = -ENOMEM;

	/* Round to block size */
	clen = skb->len;

	aead = esp->aead;
	alen = crypto_aead_authsize(aead);

	blksize = ALIGN(crypto_aead_blocksize(aead), 4);
	clen = ALIGN(clen + 2, blksize);
	if (esp->padlen)
		clen = ALIGN(clen, esp->padlen);

	if ((err = skb_cow_data(skb, clen - skb->len + alen, &trailer)) < 0)
		goto error;
	nfrags = err;

	tmp = esp_alloc_tmp(aead, nfrags + 1);
	if (!tmp)
		goto error;

	iv = esp_tmp_iv(aead, tmp);
	req = esp_tmp_givreq(aead, iv);
	asg = esp_givreq_sg(aead, req);
	sg = asg + 1;

	/* Fill padding... */
	tail = skb_tail_pointer(trailer);
	do {
		int i;
		for (i=0; i<clen-skb->len - 2; i++)
			tail[i] = i + 1;
	} while (0);
	tail[clen-skb->len - 2] = (clen - skb->len) - 2;
	tail[clen - skb->len - 1] = *skb_mac_header(skb);
	pskb_put(skb, trailer, clen - skb->len + alen);

	skb_push(skb, -skb_network_offset(skb));
	esph = ip_esp_hdr(skb);
	*skb_mac_header(skb) = IPPROTO_ESP;

	esph->spi = x->id.spi;
	esph->seq_no = htonl(XFRM_SKB_CB(skb)->seq.output);

	sg_init_table(sg, nfrags);
	skb_to_sgvec(skb, sg,
		     esph->enc_data + crypto_aead_ivsize(aead) - skb->data,
		     clen + alen);
	sg_init_one(asg, esph, sizeof(*esph));

	aead_givcrypt_set_callback(req, 0, esp_output_done, skb);
	aead_givcrypt_set_crypt(req, sg, sg, clen, iv);
	aead_givcrypt_set_assoc(req, asg, sizeof(*esph));
	aead_givcrypt_set_giv(req, esph->enc_data,
			      XFRM_SKB_CB(skb)->seq.output);

	ESP_SKB_CB(skb)->tmp = tmp;
	err = crypto_aead_givencrypt(req);
	if (err == -EINPROGRESS)
		goto error;

	if (err == -EBUSY)
		err = NET_XMIT_DROP;

	kfree(tmp);

error:
	return err;
}

static int esp_input_done2(struct sk_buff *skb, int err)
{
	struct xfrm_state *x = xfrm_input_state(skb);
	struct esp_data *esp = x->data;
	struct crypto_aead *aead = esp->aead;
	int alen = crypto_aead_authsize(aead);
	int hlen = sizeof(struct ip_esp_hdr) + crypto_aead_ivsize(aead);
	int elen = skb->len - hlen;
	int hdr_len = skb_network_header_len(skb);
	int padlen;
	u8 nexthdr[2];

	kfree(ESP_SKB_CB(skb)->tmp);

	if (unlikely(err))
		goto out;

	if (skb_copy_bits(skb, skb->len - alen - 2, nexthdr, 2))
		BUG();

	err = -EINVAL;
	padlen = nexthdr[0];
	if (padlen + 2 + alen >= elen) {
		LIMIT_NETDEBUG(KERN_WARNING "ipsec esp packet is garbage "
			       "padlen=%d, elen=%d\n", padlen + 2, elen - alen);
		goto out;
	}

	/* ... check padding bits here. Silly. :-) */

	pskb_trim(skb, skb->len - alen - padlen - 2);
	__skb_pull(skb, hlen);
	skb_set_transport_header(skb, -hdr_len);

	err = nexthdr[1];

	/* RFC4303: Drop dummy packets without any error */
	if (err == IPPROTO_NONE)
		err = -EINVAL;

out:
	return err;
}

static void esp_input_done(struct crypto_async_request *base, int err)
{
	struct sk_buff *skb = base->data;

	xfrm_input_resume(skb, esp_input_done2(skb, err));
}

static int esp6_input(struct xfrm_state *x, struct sk_buff *skb)
{
	struct ip_esp_hdr *esph;
	struct esp_data *esp = x->data;
	struct crypto_aead *aead = esp->aead;
	struct aead_request *req;
	struct sk_buff *trailer;
	int elen = skb->len - sizeof(*esph) - crypto_aead_ivsize(aead);
	int nfrags;
	int ret = 0;
	void *tmp;
	u8 *iv;
	struct scatterlist *sg;
	struct scatterlist *asg;

	if (!pskb_may_pull(skb, sizeof(*esph) + crypto_aead_ivsize(aead))) {
		ret = -EINVAL;
		goto out;
	}

	if (elen <= 0) {
		ret = -EINVAL;
		goto out;
	}

	if ((nfrags = skb_cow_data(skb, 0, &trailer)) < 0) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENOMEM;
	tmp = esp_alloc_tmp(aead, nfrags + 1);
	if (!tmp)
		goto out;

	ESP_SKB_CB(skb)->tmp = tmp;
	iv = esp_tmp_iv(aead, tmp);
	req = esp_tmp_req(aead, iv);
	asg = esp_req_sg(aead, req);
	sg = asg + 1;

	skb->ip_summed = CHECKSUM_NONE;

	esph = (struct ip_esp_hdr *)skb->data;

	/* Get ivec. This can be wrong, check against another impls. */
	iv = esph->enc_data;

	sg_init_table(sg, nfrags);
	skb_to_sgvec(skb, sg, sizeof(*esph) + crypto_aead_ivsize(aead), elen);
	sg_init_one(asg, esph, sizeof(*esph));

	aead_request_set_callback(req, 0, esp_input_done, skb);
	aead_request_set_crypt(req, sg, sg, elen, iv);
	aead_request_set_assoc(req, asg, sizeof(*esph));

	ret = crypto_aead_decrypt(req);
	if (ret == -EINPROGRESS)
		goto out;

	ret = esp_input_done2(skb, ret);

out:
	return ret;
}

static u32 esp6_get_mtu(struct xfrm_state *x, int mtu)
{
	struct esp_data *esp = x->data;
	u32 blksize = ALIGN(crypto_aead_blocksize(esp->aead), 4);
	u32 align = max_t(u32, blksize, esp->padlen);
	u32 rem;

	mtu -= x->props.header_len + crypto_aead_authsize(esp->aead);
	rem = mtu & (align - 1);
	mtu &= ~(align - 1);

	if (x->props.mode != XFRM_MODE_TUNNEL) {
		u32 padsize = ((blksize - 1) & 7) + 1;
		mtu -= blksize - padsize;
		mtu += min_t(u32, blksize - padsize, rem);
	}

	return mtu - 2;
}

static void esp6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
		     u8 type, u8 code, int offset, __be32 info)
{
	struct net *net = dev_net(skb->dev);
	struct ipv6hdr *iph = (struct ipv6hdr*)skb->data;
	struct ip_esp_hdr *esph = (struct ip_esp_hdr *)(skb->data + offset);
	struct xfrm_state *x;

	if (type != ICMPV6_DEST_UNREACH &&
	    type != ICMPV6_PKT_TOOBIG)
		return;

	x = xfrm_state_lookup(net, skb->mark, (xfrm_address_t *)&iph->daddr, esph->spi, IPPROTO_ESP, AF_INET6);
	if (!x)
		return;
	printk(KERN_DEBUG "pmtu discovery on SA ESP/%08x/%pI6\n",
			ntohl(esph->spi), &iph->daddr);
	xfrm_state_put(x);
}

static void esp6_destroy(struct xfrm_state *x)
{
	struct esp_data *esp = x->data;

	if (!esp)
		return;

	crypto_free_aead(esp->aead);
	kfree(esp);
}

static int esp_init_aead(struct xfrm_state *x)
{
	struct esp_data *esp = x->data;
	struct crypto_aead *aead;
	int err;

	aead = crypto_alloc_aead(x->aead->alg_name, 0, 0);
	err = PTR_ERR(aead);
	if (IS_ERR(aead))
		goto error;

	esp->aead = aead;

	err = crypto_aead_setkey(aead, x->aead->alg_key,
				 (x->aead->alg_key_len + 7) / 8);
	if (err)
		goto error;

	err = crypto_aead_setauthsize(aead, x->aead->alg_icv_len / 8);
	if (err)
		goto error;

error:
	return err;
}

static int esp_init_authenc(struct xfrm_state *x)
{
	struct esp_data *esp = x->data;
	struct crypto_aead *aead;
	struct crypto_authenc_key_param *param;
	struct rtattr *rta;
	char *key;
	char *p;
	char authenc_name[CRYPTO_MAX_ALG_NAME];
	unsigned int keylen;
	int err;

	err = -EINVAL;
	if (x->ealg == NULL)
		goto error;

	err = -ENAMETOOLONG;
	if (snprintf(authenc_name, CRYPTO_MAX_ALG_NAME, "authenc(%s,%s)",
		     x->aalg ? x->aalg->alg_name : "digest_null",
		     x->ealg->alg_name) >= CRYPTO_MAX_ALG_NAME)
		goto error;

	aead = crypto_alloc_aead(authenc_name, 0, 0);
	err = PTR_ERR(aead);
	if (IS_ERR(aead))
		goto error;

	esp->aead = aead;

	keylen = (x->aalg ? (x->aalg->alg_key_len + 7) / 8 : 0) +
		 (x->ealg->alg_key_len + 7) / 8 + RTA_SPACE(sizeof(*param));
	err = -ENOMEM;
	key = kmalloc(keylen, GFP_KERNEL);
	if (!key)
		goto error;

	p = key;
	rta = (void *)p;
	rta->rta_type = CRYPTO_AUTHENC_KEYA_PARAM;
	rta->rta_len = RTA_LENGTH(sizeof(*param));
	param = RTA_DATA(rta);
	p += RTA_SPACE(sizeof(*param));

	if (x->aalg) {
		struct xfrm_algo_desc *aalg_desc;

		memcpy(p, x->aalg->alg_key, (x->aalg->alg_key_len + 7) / 8);
		p += (x->aalg->alg_key_len + 7) / 8;

		aalg_desc = xfrm_aalg_get_byname(x->aalg->alg_name, 0);
		BUG_ON(!aalg_desc);

		err = -EINVAL;
		if (aalg_desc->uinfo.auth.icv_fullbits/8 !=
		    crypto_aead_authsize(aead)) {
			NETDEBUG(KERN_INFO "ESP: %s digestsize %u != %hu\n",
				 x->aalg->alg_name,
				 crypto_aead_authsize(aead),
				 aalg_desc->uinfo.auth.icv_fullbits/8);
			goto free_key;
		}

		err = crypto_aead_setauthsize(
			aead, x->aalg->alg_trunc_len / 8);
		if (err)
			goto free_key;
	}

	param->enckeylen = cpu_to_be32((x->ealg->alg_key_len + 7) / 8);
	memcpy(p, x->ealg->alg_key, (x->ealg->alg_key_len + 7) / 8);

	err = crypto_aead_setkey(aead, key, keylen);

free_key:
	kfree(key);

error:
	return err;
}

static int esp6_init_state(struct xfrm_state *x)
{
	struct esp_data *esp;
	struct crypto_aead *aead;
	u32 align;
	int err;

	if (x->encap)
		return -EINVAL;

	esp = kzalloc(sizeof(*esp), GFP_KERNEL);
	if (esp == NULL)
		return -ENOMEM;

	x->data = esp;

	if (x->aead)
		err = esp_init_aead(x);
	else
		err = esp_init_authenc(x);

	if (err)
		goto error;

	aead = esp->aead;

	esp->padlen = 0;

	x->props.header_len = sizeof(struct ip_esp_hdr) +
			      crypto_aead_ivsize(aead);
	switch (x->props.mode) {
	case XFRM_MODE_BEET:
		if (x->sel.family != AF_INET6)
			x->props.header_len += IPV4_BEET_PHMAXLEN +
				               (sizeof(struct ipv6hdr) - sizeof(struct iphdr));
		break;
	case XFRM_MODE_TRANSPORT:
		break;
	case XFRM_MODE_TUNNEL:
		x->props.header_len += sizeof(struct ipv6hdr);
		break;
	default:
		goto error;
	}

	align = ALIGN(crypto_aead_blocksize(aead), 4);
	if (esp->padlen)
		align = max_t(u32, align, esp->padlen);
	x->props.trailer_len = align + 1 + crypto_aead_authsize(esp->aead);

error:
	return err;
}

static const struct xfrm_type esp6_type =
{
	.description	= "ESP6",
	.owner	     	= THIS_MODULE,
	.proto	     	= IPPROTO_ESP,
	.flags		= XFRM_TYPE_REPLAY_PROT,
	.init_state	= esp6_init_state,
	.destructor	= esp6_destroy,
	.get_mtu	= esp6_get_mtu,
	.input		= esp6_input,
	.output		= esp6_output,
	.hdr_offset	= xfrm6_find_1stfragopt,
};

static const struct inet6_protocol esp6_protocol = {
	.handler 	=	xfrm6_rcv,
	.err_handler	=	esp6_err,
	.flags		=	INET6_PROTO_NOPOLICY,
};

static int __init esp6_init(void)
{
	if (xfrm_register_type(&esp6_type, AF_INET6) < 0) {
		printk(KERN_INFO "ipv6 esp init: can't add xfrm type\n");
		return -EAGAIN;
	}
	if (inet6_add_protocol(&esp6_protocol, IPPROTO_ESP) < 0) {
		printk(KERN_INFO "ipv6 esp init: can't add protocol\n");
		xfrm_unregister_type(&esp6_type, AF_INET6);
		return -EAGAIN;
	}

	return 0;
}

static void __exit esp6_fini(void)
{
	if (inet6_del_protocol(&esp6_protocol, IPPROTO_ESP) < 0)
		printk(KERN_INFO "ipv6 esp close: can't remove protocol\n");
	if (xfrm_unregister_type(&esp6_type, AF_INET6) < 0)
		printk(KERN_INFO "ipv6 esp close: can't remove xfrm type\n");
}

module_init(esp6_init);
module_exit(esp6_fini);

MODULE_LICENSE("GPL");
MODULE_ALIAS_XFRM_TYPE(AF_INET6, XFRM_PROTO_ESP);

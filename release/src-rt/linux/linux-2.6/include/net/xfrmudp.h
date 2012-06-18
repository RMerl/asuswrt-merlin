/*
 * pointer to function for type that xfrm4_input wants, to permit
 * decoupling of XFRM from udp.c
 */
#define HAVE_XFRM4_UDP_REGISTER

typedef int (*xfrm4_rcv_encap_t)(struct sk_buff *skb, __u16 encap_type);
extern int udp4_register_esp_rcvencap(xfrm4_rcv_encap_t func
				      , xfrm4_rcv_encap_t *oldfunc);
extern int udp4_unregister_esp_rcvencap(xfrm4_rcv_encap_t func);

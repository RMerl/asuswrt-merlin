/* This example is placed in the public domain. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <libmnl/libmnl.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>

static int parse_ip_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, CTA_IP_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case CTA_IP_V4_SRC:
	case CTA_IP_V4_DST:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static void print_ip(const struct nlattr *nest)
{
	struct nlattr *tb[CTA_IP_MAX+1] = {};

	mnl_attr_parse_nested(nest, parse_ip_cb, tb);
	if (tb[CTA_IP_V4_SRC]) {
		struct in_addr *in = mnl_attr_get_payload(tb[CTA_IP_V4_SRC]);
		printf("src=%s ", inet_ntoa(*in));
	}
	if (tb[CTA_IP_V4_DST]) {
		struct in_addr *in = mnl_attr_get_payload(tb[CTA_IP_V4_DST]);
		printf("dst=%s ", inet_ntoa(*in));
	}
}

static int parse_proto_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, CTA_PROTO_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case CTA_PROTO_NUM:
	case CTA_PROTO_ICMP_TYPE:
	case CTA_PROTO_ICMP_CODE:
		if (mnl_attr_validate(attr, MNL_TYPE_U8) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case CTA_PROTO_SRC_PORT:
	case CTA_PROTO_DST_PORT:
	case CTA_PROTO_ICMP_ID:
		if (mnl_attr_validate(attr, MNL_TYPE_U16) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static void print_proto(const struct nlattr *nest)
{
	struct nlattr *tb[CTA_PROTO_MAX+1] = {};

	mnl_attr_parse_nested(nest, parse_proto_cb, tb);
	if (tb[CTA_PROTO_NUM]) {
		printf("proto=%u ", mnl_attr_get_u8(tb[CTA_PROTO_NUM]));
	}
	if (tb[CTA_PROTO_SRC_PORT]) {
		printf("sport=%u ",
			ntohs(mnl_attr_get_u16(tb[CTA_PROTO_SRC_PORT])));
	}
	if (tb[CTA_PROTO_DST_PORT]) {
		printf("dport=%u ",
			ntohs(mnl_attr_get_u16(tb[CTA_PROTO_DST_PORT])));
	}
	if (tb[CTA_PROTO_ICMP_ID]) {
		printf("id=%u ",
			ntohs(mnl_attr_get_u16(tb[CTA_PROTO_ICMP_ID])));
	}
	if (tb[CTA_PROTO_ICMP_TYPE]) {
		printf("type=%u ", mnl_attr_get_u8(tb[CTA_PROTO_ICMP_TYPE]));
	}
	if (tb[CTA_PROTO_ICMP_CODE]) {
		printf("code=%u ", mnl_attr_get_u8(tb[CTA_PROTO_ICMP_CODE]));
	}
}

static int parse_tuple_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, CTA_TUPLE_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case CTA_TUPLE_IP:
		if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case CTA_TUPLE_PROTO:
		if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static void print_tuple(const struct nlattr *nest)
{
	struct nlattr *tb[CTA_TUPLE_MAX+1] = {};

	mnl_attr_parse_nested(nest, parse_tuple_cb, tb);
	if (tb[CTA_TUPLE_IP]) {
		print_ip(tb[CTA_TUPLE_IP]);
	}
	if (tb[CTA_TUPLE_PROTO]) {
		print_proto(tb[CTA_TUPLE_PROTO]);
	}
}

static int data_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, CTA_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case CTA_TUPLE_ORIG:
		if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case CTA_TIMEOUT:
	case CTA_MARK:
	case CTA_SECMARK:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static int data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[CTA_MAX+1] = {};
	struct nfgenmsg *nfg = mnl_nlmsg_get_payload(nlh);

	switch(nlh->nlmsg_type & 0xFF) {
	case IPCTNL_MSG_CT_NEW:
		if (nlh->nlmsg_flags & (NLM_F_CREATE|NLM_F_EXCL))
			printf("%9s ", "[NEW] ");
		else
			printf("%9s ", "[UPDATE] ");
		break;
	case IPCTNL_MSG_CT_DELETE:
		printf("%9s ", "[DESTROY] ");
		break;
	}

	mnl_attr_parse(nlh, sizeof(*nfg), data_attr_cb, tb);
	if (tb[CTA_TUPLE_ORIG]) {
		print_tuple(tb[CTA_TUPLE_ORIG]);
	}
	if (tb[CTA_MARK]) {
		printf("mark=%u ", ntohl(mnl_attr_get_u32(tb[CTA_MARK])));
	}
	if (tb[CTA_SECMARK]) {
		printf("secmark=%u ", ntohl(mnl_attr_get_u32(tb[CTA_SECMARK])));
	}
	printf("\n");
	return MNL_CB_OK;
}

int main(void)
{
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	int ret;

	nl = mnl_socket_open(NETLINK_NETFILTER);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, NF_NETLINK_CONNTRACK_NEW |
				NF_NETLINK_CONNTRACK_UPDATE |
				NF_NETLINK_CONNTRACK_DESTROY,
				MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}

	while (1) {
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
		if (ret == -1) {
			perror("mnl_socket_recvfrom");
			exit(EXIT_FAILURE);
		}

		ret = mnl_cb_run(buf, ret, 0, 0, data_cb, NULL);
		if (ret == -1) {
			perror("mnl_cb_run");
			exit(EXIT_FAILURE);
		}
	}

	mnl_socket_close(nl);

	return 0;
}

/* This example is placed in the public domain. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include <libmnl/libmnl.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>

#ifndef aligned_be64
#define aligned_be64 u_int64_t __attribute__((aligned(8)))
#endif

#include <linux/netfilter/nfnetlink_log.h>

static int parse_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, NFULA_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case NFULA_MARK:
	case NFULA_IFINDEX_INDEV:
	case NFULA_IFINDEX_OUTDEV:
	case NFULA_IFINDEX_PHYSINDEV:
	case NFULA_IFINDEX_PHYSOUTDEV:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case NFULA_TIMESTAMP:
		if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC,
		    sizeof(struct nfulnl_msg_packet_timestamp)) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case NFULA_HWADDR:
		if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC,
		    sizeof(struct nfulnl_msg_packet_hw)) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case NFULA_PREFIX:
		if (mnl_attr_validate(attr, MNL_TYPE_NUL_STRING) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case NFULA_PAYLOAD:
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static int log_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[NFULA_MAX+1] = {};
	struct nfulnl_msg_packet_hdr *ph = NULL;
	const char *prefix = NULL;
	uint32_t mark = 0;

	mnl_attr_parse(nlh, sizeof(struct nfgenmsg), parse_attr_cb, tb);
	if (tb[NFULA_PACKET_HDR])
		ph = mnl_attr_get_payload(tb[NFULA_PACKET_HDR]);
	if (tb[NFULA_PREFIX])
		prefix = mnl_attr_get_str(tb[NFULA_PREFIX]);
	if (tb[NFULA_MARK])
		mark = ntohl(mnl_attr_get_u32(tb[NFULA_MARK]));

	printf("log received (prefix=\"%s\" hw=0x%04x hook=%u mark=%u)\n",
		prefix ? prefix : "", ntohs(ph->hw_protocol), ph->hook,
		mark);

	return MNL_CB_OK;
}

static struct nlmsghdr *
nflog_build_cfg_pf_request(char *buf, uint8_t command)
{
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= (NFNL_SUBSYS_ULOG << 8) | NFULNL_MSG_CONFIG;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	struct nfgenmsg *nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_INET;
	nfg->version = NFNETLINK_V0;

	struct nfulnl_msg_config_cmd cmd = {
		.command = command,
	};
	mnl_attr_put(nlh, NFULA_CFG_CMD, sizeof(cmd), &cmd);

	return nlh;
}

static struct nlmsghdr *
nflog_build_cfg_request(char *buf, uint8_t command, int qnum)
{
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= (NFNL_SUBSYS_ULOG << 8) | NFULNL_MSG_CONFIG;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	struct nfgenmsg *nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_INET;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(qnum);

	struct nfulnl_msg_config_cmd cmd = {
		.command = command,
	};
	mnl_attr_put(nlh, NFULA_CFG_CMD, sizeof(cmd), &cmd);

	return nlh;
}

static struct nlmsghdr *
nflog_build_cfg_params(char *buf, uint8_t mode, int range, int qnum)
{
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= (NFNL_SUBSYS_ULOG << 8) | NFULNL_MSG_CONFIG;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	struct nfgenmsg *nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_UNSPEC;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(qnum);

	struct nfulnl_msg_config_mode params = {
		.copy_range = htonl(range),
		.copy_mode = mode,
	};
	mnl_attr_put(nlh, NFULA_CFG_MODE, sizeof(params), &params);

	return nlh;
}

int main(int argc, char *argv[])
{
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	int ret;
	unsigned int portid, qnum;

	if (argc != 2) {
		printf("Usage: %s [queue_num]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	qnum = atoi(argv[1]);

	nl = mnl_socket_open(NETLINK_NETFILTER);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	portid = mnl_socket_get_portid(nl);

	nlh = nflog_build_cfg_pf_request(buf, NFULNL_CFG_CMD_PF_UNBIND);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	nlh = nflog_build_cfg_pf_request(buf, NFULNL_CFG_CMD_PF_BIND);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	nlh = nflog_build_cfg_request(buf, NFULNL_CFG_CMD_BIND, qnum);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	nlh = nflog_build_cfg_params(buf, NFULNL_COPY_PACKET, 0xFFFF, qnum);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		exit(EXIT_FAILURE);
	}
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, 0, portid, log_cb, NULL);
		if (ret < 0){
			perror("mnl_cb_run");
			exit(EXIT_FAILURE);
		}

		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
		if (ret == -1) {
			perror("mnl_socket_recvfrom");
			exit(EXIT_FAILURE);
		}
	}

	mnl_socket_close(nl);

	return 0;
}

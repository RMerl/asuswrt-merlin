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

#include <linux/netfilter/nfnetlink_queue.h>

static int parse_attr_cb(const struct nlattr *attr, void *data)
{
        const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, NFQA_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case NFQA_MARK:
	case NFQA_IFINDEX_INDEV:
	case NFQA_IFINDEX_OUTDEV:
	case NFQA_IFINDEX_PHYSINDEV:
	case NFQA_IFINDEX_PHYSOUTDEV:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case NFQA_TIMESTAMP:
		if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC,
		    sizeof(struct nfqnl_msg_packet_timestamp)) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case NFQA_HWADDR:
		if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC,
		    sizeof(struct nfqnl_msg_packet_hw)) < 0) {
			perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case NFQA_PAYLOAD:
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

static int queue_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[NFQA_MAX+1] = {};
	struct nfqnl_msg_packet_hdr *ph = NULL;
	uint32_t id = 0;

	mnl_attr_parse(nlh, sizeof(struct nfgenmsg), parse_attr_cb, tb);
	if (tb[NFQA_PACKET_HDR]) {
		ph = mnl_attr_get_payload(tb[NFQA_PACKET_HDR]);
		id = ntohl(ph->packet_id);
	}
	printf("packet received (id=%u hw=0x%04x hook=%u)\n",
		id, ntohs(ph->hw_protocol), ph->hook);

	return MNL_CB_OK + id;
}

static struct nlmsghdr *
nfq_build_cfg_pf_request(char *buf, uint8_t command)
{
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= (NFNL_SUBSYS_QUEUE << 8) | NFQNL_MSG_CONFIG;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	struct nfgenmsg *nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_UNSPEC;
	nfg->version = NFNETLINK_V0;

	struct nfqnl_msg_config_cmd cmd = {
		.command = command,
		.pf = htons(AF_INET),
	};
	mnl_attr_put(nlh, NFQA_CFG_CMD, sizeof(cmd), &cmd);

	return nlh;
}

static struct nlmsghdr *
nfq_build_cfg_request(char *buf, uint8_t command, int queue_num)
{
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= (NFNL_SUBSYS_QUEUE << 8) | NFQNL_MSG_CONFIG;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	struct nfgenmsg *nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_UNSPEC;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(queue_num);

	struct nfqnl_msg_config_cmd cmd = {
		.command = command,
		.pf = htons(AF_INET),
	};
	mnl_attr_put(nlh, NFQA_CFG_CMD, sizeof(cmd), &cmd);

	return nlh;
}

static struct nlmsghdr *
nfq_build_cfg_params(char *buf, uint8_t mode, int range, int queue_num)
{
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= (NFNL_SUBSYS_QUEUE << 8) | NFQNL_MSG_CONFIG;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	struct nfgenmsg *nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_UNSPEC;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(queue_num);

	struct nfqnl_msg_config_params params = {
		.copy_range = htonl(range),
		.copy_mode = mode,
	};
	mnl_attr_put(nlh, NFQA_CFG_PARAMS, sizeof(params), &params);

	return nlh;
}

static struct nlmsghdr *
nfq_build_verdict(char *buf, int id, int queue_num, int verd)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = (NFNL_SUBSYS_QUEUE << 8) | NFQNL_MSG_VERDICT;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_UNSPEC;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(queue_num);

	struct nfqnl_msg_verdict_hdr vh = {
		.verdict = htonl(verd),
		.id = htonl(id),
	};
	mnl_attr_put(nlh, NFQA_VERDICT_HDR, sizeof(vh), &vh);

	return nlh;
}

int main(int argc, char *argv[])
{
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	int ret;
	unsigned int portid, queue_num;

	if (argc != 2) {
		printf("Usage: %s [queue_num]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	queue_num = atoi(argv[1]);

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

	nlh = nfq_build_cfg_pf_request(buf, NFQNL_CFG_CMD_PF_UNBIND);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	nlh = nfq_build_cfg_pf_request(buf, NFQNL_CFG_CMD_PF_BIND);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	nlh = nfq_build_cfg_request(buf, NFQNL_CFG_CMD_BIND, queue_num);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	nlh = nfq_build_cfg_params(buf, NFQNL_COPY_PACKET, 0xFFFF, queue_num);

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
		uint32_t id;

		ret = mnl_cb_run(buf, ret, 0, portid, queue_cb, NULL);
		if (ret < 0){
			perror("mnl_cb_run");
			exit(EXIT_FAILURE);
		}

		id = ret - MNL_CB_OK;
		nlh = nfq_build_verdict(buf, id, queue_num, NF_ACCEPT);
		if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
			perror("mnl_socket_send");
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

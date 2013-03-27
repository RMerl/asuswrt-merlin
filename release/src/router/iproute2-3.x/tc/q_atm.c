/*
 * q_atm.c		ATM.
 *
 * Hacked 1998-2000 by Werner Almesberger, EPFL ICA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <atm.h>
#include <linux/atmdev.h>
#include <linux/atmarp.h>

#include "utils.h"
#include "tc_util.h"


#define MAX_HDR_LEN 64


static int atm_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	if (argc) {
		fprintf(stderr,"Usage: atm\n");
		return -1;
	}
	return 0;
}


static void explain(void)
{
	fprintf(stderr, "Usage: ... atm ( pvc ADDR | svc ADDR [ sap SAP ] ) "
	    "[ qos QOS ] [ sndbuf BYTES ]\n");
	fprintf(stderr, "  [ hdr HEX... ] [ excess ( CLASSID | clp ) ] "
	  "[ clip ]\n");
}


static int atm_parse_class_opt(struct qdisc_util *qu, int argc, char **argv,
   struct nlmsghdr *n)
{
	struct sockaddr_atmsvc addr;
	struct atm_qos qos;
	struct atm_sap sap;
	unsigned char hdr[MAX_HDR_LEN];
	__u32 excess = 0;
	struct rtattr *tail;
	int sndbuf = 0;
	int hdr_len = -1;
	int set_clip = 0;
	int s;

	memset(&addr,0,sizeof(addr));
	(void) text2qos("aal5,ubr:sdu=9180,rx:none",&qos,0);
	(void) text2sap("blli:l2=iso8802",&sap,0);
	while (argc > 0) {
		if (!strcmp(*argv,"pvc")) {
			NEXT_ARG();
			if (text2atm(*argv,(struct sockaddr *) &addr,
			    sizeof(addr),T2A_PVC | T2A_NAME) < 0) {
				explain();
				return -1;
			}
		}
		else if (!strcmp(*argv,"svc")) {
			NEXT_ARG();
			if (text2atm(*argv,(struct sockaddr *) &addr,
			    sizeof(addr),T2A_SVC | T2A_NAME) < 0) {
				explain();
				return -1;
			}
		}
		else if (!strcmp(*argv,"qos")) {
			NEXT_ARG();
			if (text2qos(*argv,&qos,0) < 0) {
				explain();
				return -1;
			}
		}
		else if (!strcmp(*argv,"sndbuf")) {
			char *end;

			NEXT_ARG();
			sndbuf = strtol(*argv,&end,0);
			if (*end) {
				explain();
				return -1;
			}
		}
		else if (!strcmp(*argv,"sap")) {
			NEXT_ARG();
			if (addr.sas_family != AF_ATMSVC ||
			    text2sap(*argv,&sap,T2A_NAME) < 0) {
				explain();
				return -1;
			}
		}
		else if (!strcmp(*argv,"hdr")) {
			unsigned char *ptr;
			char *walk;

			NEXT_ARG();
			ptr = hdr;
			for (walk = *argv; *walk; walk++) {
				int tmp;

				if (ptr == hdr+MAX_HDR_LEN) {
					fprintf(stderr,"header is too long\n");
					return -1;
				}
				if (*walk == '.') continue;
				if (!isxdigit(walk[0]) || !walk[1] ||
				    !isxdigit(walk[1])) {
					explain();
					return -1;
				}
				sscanf(walk,"%2x",&tmp);
				*ptr++ = tmp;
				walk++;
			}
			hdr_len = ptr-hdr;
		}
		else if (!strcmp(*argv,"excess")) {
			NEXT_ARG();
			if (!strcmp(*argv,"clp")) excess = 0;
			else if (get_tc_classid(&excess,*argv)) {
					explain();
					return -1;
				}
		}
		else if (!strcmp(*argv,"clip")) {
			set_clip = 1;
		}
		else {
			explain();
			return 1;
		}
		argc--;
		argv++;
	}
	s = socket(addr.sas_family,SOCK_DGRAM,0);
	if (s < 0) {
		perror("socket");
		return -1;
	}
	if (setsockopt(s,SOL_ATM,SO_ATMQOS,&qos,sizeof(qos)) < 0) {
		perror("SO_ATMQOS");
		return -1;
	}
	if (sndbuf)
	    if (setsockopt(s,SOL_SOCKET,SO_SNDBUF,&sndbuf,sizeof(sndbuf)) < 0) {
		perror("SO_SNDBUF");
	    return -1;
	}
	if (addr.sas_family == AF_ATMSVC && setsockopt(s,SOL_ATM,SO_ATMSAP,
	    &sap,sizeof(sap)) < 0) {
		perror("SO_ATMSAP");
		return -1;
	}
	if (connect(s,(struct sockaddr *) &addr,addr.sas_family == AF_ATMPVC ?
	    sizeof(struct sockaddr_atmpvc) : sizeof(addr)) < 0) {
		perror("connect");
		return -1;
	}
	if (set_clip)
		if (ioctl(s,ATMARP_MKIP,0) < 0) {
			perror("ioctl ATMARP_MKIP");
			return -1;
		}
	tail = NLMSG_TAIL(n);
	addattr_l(n,1024,TCA_OPTIONS,NULL,0);
	addattr_l(n,1024,TCA_ATM_FD,&s,sizeof(s));
	if (excess) addattr_l(n,1024,TCA_ATM_EXCESS,&excess,sizeof(excess));
	if (hdr_len != -1) addattr_l(n,1024,TCA_ATM_HDR,hdr,hdr_len);
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}



static int atm_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_ATM_MAX+1];
	char buffer[MAX_ATM_ADDR_LEN+1];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_ATM_MAX, opt);
	if (tb[TCA_ATM_ADDR]) {
		if (RTA_PAYLOAD(tb[TCA_ATM_ADDR]) <
		    sizeof(struct sockaddr_atmpvc))
			fprintf(stderr,"ATM: address too short\n");
		else {
			if (atm2text(buffer,MAX_ATM_ADDR_LEN,
			    RTA_DATA(tb[TCA_ATM_ADDR]),A2T_PRETTY | A2T_NAME) <
			    0) fprintf(stderr,"atm2text error\n");
			fprintf(f,"pvc %s ",buffer);
		}
	}
	if (tb[TCA_ATM_HDR]) {
		int i;

		fprintf(f,"hdr");
		for (i = 0; i < RTA_PAYLOAD(tb[TCA_ATM_HDR]); i++)
			fprintf(f,"%c%02x",i ? '.' : ' ',
			    ((unsigned char *) RTA_DATA(tb[TCA_ATM_HDR]))[i]);
		if (!i) fprintf(f," .");
		fprintf(f," ");
	}
	if (tb[TCA_ATM_EXCESS]) {
		__u32 excess;

		if (RTA_PAYLOAD(tb[TCA_ATM_EXCESS]) < sizeof(excess))
			fprintf(stderr,"ATM: excess class ID too short\n");
		else {
			excess = *(__u32 *) RTA_DATA(tb[TCA_ATM_EXCESS]);
			if (!excess) fprintf(f,"excess clp ");
			else {
				char buf[64];

				print_tc_classid(buf,sizeof(buf),excess);
				fprintf(f,"excess %s ",buf);
			}
		}
	}
	if (tb[TCA_ATM_STATE]) {
		static const char *map[] = { ATM_VS2TXT_MAP };
		int state;

		if (RTA_PAYLOAD(tb[TCA_ATM_STATE]) < sizeof(state))
			fprintf(stderr,"ATM: state field too short\n");
		else {
			state = *(int *) RTA_DATA(tb[TCA_ATM_STATE]);
			fprintf(f,"%s ",map[state]);
		}
	}
	return 0;
}


struct qdisc_util atm_qdisc_util = {
	.id 		= "atm",
	.parse_qopt	= atm_parse_opt,
	.print_qopt	= atm_print_opt,
	.parse_copt	= atm_parse_class_opt,
	.print_copt	= atm_print_opt,
};

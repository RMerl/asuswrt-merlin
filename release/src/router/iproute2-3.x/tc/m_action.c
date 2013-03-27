/*
 * m_action.c		Action Management
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:  J Hadi Salim (hadi@cyberus.ca)
 *
 * TODO:
 * - parse to be passed a filedescriptor for logging purposes
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dlfcn.h>

#include "utils.h"
#include "tc_common.h"
#include "tc_util.h"

static struct action_util * action_list;
#ifdef CONFIG_GACT
int gact_ld = 0 ; //fuckin backward compatibility
#endif
int batch_c = 0;
int tab_flush = 0;

void act_usage(void)
{
	/*XXX: In the near future add a action->print_help to improve
	 * usability
	 * This would mean new tc will not be backward compatible
	 * with any action .so from the old days. But if someone really
	 * does that, they would know how to fix this ..
	 *
	*/
	fprintf (stderr, "usage: tc actions <ACTSPECOP>*\n");
	fprintf(stderr,
		"Where: \tACTSPECOP := ACR | GD | FL\n"
			"\tACR := add | change | replace <ACTSPEC>* \n"
			"\tGD := get | delete | <ACTISPEC>*\n"
			"\tFL := ls | list | flush | <ACTNAMESPEC>\n"
			"\tACTNAMESPEC :=  action <ACTNAME>\n"
			"\tACTISPEC := <ACTNAMESPEC> <INDEXSPEC>\n"
			"\tACTSPEC := action <ACTDETAIL> [INDEXSPEC]\n"
			"\tINDEXSPEC := index <32 bit indexvalue>\n"
			"\tACTDETAIL := <ACTNAME> <ACTPARAMS>\n"
			"\t\tExample ACTNAME is gact, mirred etc\n"
			"\t\tEach action has its own parameters (ACTPARAMS)\n"
			"\n");

	exit(-1);
}

static int print_noaopt(struct action_util *au, FILE *f, struct rtattr *opt)
{
	if (opt && RTA_PAYLOAD(opt))
		fprintf(f, "[Unknown action, optlen=%u] ",
			(unsigned) RTA_PAYLOAD(opt));
	return 0;
}

static int parse_noaopt(struct action_util *au, int *argc_p, char ***argv_p, int code, struct nlmsghdr *n)
{
	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc) {
		fprintf(stderr, "Unknown action \"%s\", hence option \"%s\" is unparsable\n", au->id, *argv);
	} else {
		fprintf(stderr, "Unknown action \"%s\"\n", au->id);
	}
	return -1;
}

struct action_util *get_action_kind(char *str)
{
	static void *aBODY;
	void *dlh;
	char buf[256];
	struct action_util *a;
#ifdef CONFIG_GACT
	int looked4gact = 0;
restart_s:
#endif
	for (a = action_list; a; a = a->next) {
		if (strcmp(a->id, str) == 0)
			return a;
	}

	snprintf(buf, sizeof(buf), "%s/m_%s.so", get_tc_lib(), str);
	dlh = dlopen(buf, RTLD_LAZY | RTLD_GLOBAL);
	if (dlh == NULL) {
		dlh = aBODY;
		if (dlh == NULL) {
			dlh = aBODY = dlopen(NULL, RTLD_LAZY);
			if (dlh == NULL)
				goto noexist;
		}
	}

	snprintf(buf, sizeof(buf), "%s_action_util", str);
	a = dlsym(dlh, buf);
	if (a == NULL)
		goto noexist;

reg:
	a->next = action_list;
	action_list = a;
	return a;

noexist:
#ifdef CONFIG_GACT
	if (!looked4gact) {
		looked4gact = 1;
		strcpy(str,"gact");
		goto restart_s;
	}
#endif
	a = malloc(sizeof(*a));
	if (a) {
		memset(a, 0, sizeof(*a));
		strncpy(a->id, "noact", 15);
		a->parse_aopt = parse_noaopt;
		a->print_aopt = print_noaopt;
		goto reg;
	}
	return a;
}

int
new_cmd(char **argv)
{
	if ((matches(*argv, "change") == 0) ||
		(matches(*argv, "replace") == 0)||
		(matches(*argv, "delete") == 0)||
		(matches(*argv, "add") == 0))
			return 1;

	return 0;

}

int
parse_action(int *argc_p, char ***argv_p, int tca_id, struct nlmsghdr *n)
{
	int argc = *argc_p;
	char **argv = *argv_p;
	struct rtattr *tail, *tail2;
	char k[16];
	int ok = 0;
	int eap = 0; /* expect action parameters */

	int ret = 0;
	int prio = 0;

	if (argc <= 0)
		return -1;

	tail = tail2 = NLMSG_TAIL(n);

	addattr_l(n, MAX_MSG, tca_id, NULL, 0);

	while (argc > 0) {

		memset(k, 0, sizeof (k));

		if (strcmp(*argv, "action") == 0 ) {
			argc--;
			argv++;
			eap = 1;
#ifdef CONFIG_GACT
			if (!gact_ld) {
				get_action_kind("gact");
			}
#endif
			continue;
		} else if (strcmp(*argv, "help") == 0) {
			return -1;
		} else if (new_cmd(argv)) {
			goto done0;
		} else {
			struct action_util *a = NULL;
			strncpy(k, *argv, sizeof (k) - 1);
			eap = 0;
			if (argc > 0 ) {
				a = get_action_kind(k);
			} else {
done0:
				if (ok)
					break;
				else
					goto done;
			}

			if (NULL == a) {
				goto bad_val;
			}

			tail = NLMSG_TAIL(n);
			addattr_l(n, MAX_MSG, ++prio, NULL, 0);
			addattr_l(n, MAX_MSG, TCA_ACT_KIND, k, strlen(k) + 1);

			ret = a->parse_aopt(a,&argc, &argv, TCA_ACT_OPTIONS, n);

			if (ret < 0) {
				fprintf(stderr,"bad action parsing\n");
				goto bad_val;
			}
			tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
			ok++;
		}

	}

	if (eap > 0) {
		fprintf(stderr,"bad action empty %d\n",eap);
		goto bad_val;
	}

	tail2->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail2;

done:
	*argc_p = argc;
	*argv_p = argv;
	return 0;
bad_val:
	/* no need to undo things, returning from here should
	 * cause enough pain */
	fprintf(stderr, "parse_action: bad value (%d:%s)!\n",argc,*argv);
	return -1;
}

int
tc_print_one_action(FILE * f, struct rtattr *arg)
{

	struct rtattr *tb[TCA_ACT_MAX + 1];
	int err = 0;
	struct action_util *a = NULL;

	if (arg == NULL)
		return -1;

	parse_rtattr_nested(tb, TCA_ACT_MAX, arg);
	if (tb[TCA_ACT_KIND] == NULL) {
		fprintf(stderr, "NULL Action!\n");
		return -1;
	}


	a = get_action_kind(RTA_DATA(tb[TCA_ACT_KIND]));
	if (NULL == a)
		return err;

	if (tab_flush) {
		fprintf(f," %s \n", a->id);
		tab_flush = 0;
		return 0;
	}

	err = a->print_aopt(a,f,tb[TCA_ACT_OPTIONS]);


	if (0 > err)
		return err;

	if (show_stats && tb[TCA_ACT_STATS]) {
		fprintf(f, "\tAction statistics:\n");
		print_tcstats2_attr(f, tb[TCA_ACT_STATS], "\t", NULL);
		fprintf(f, "\n");
	}

	return 0;
}

int
tc_print_action(FILE * f, const struct rtattr *arg)
{

	int i;
	struct rtattr *tb[TCA_ACT_MAX_PRIO + 1];

	if (arg == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_ACT_MAX_PRIO, arg);

	if (tab_flush && NULL != tb[0]  && NULL == tb[1]) {
		int ret = tc_print_one_action(f, tb[0]);
		return ret;
	}

	for (i = 0; i < TCA_ACT_MAX_PRIO; i++) {
		if (tb[i]) {
			fprintf(f, "\n\taction order %d: ", i + batch_c);
			if (0 > tc_print_one_action(f, tb[i])) {
				fprintf(f, "Error printing action\n");
			}
		}

	}

	batch_c+=TCA_ACT_MAX_PRIO ;
	return 0;
}

int print_action(const struct sockaddr_nl *who,
			   struct nlmsghdr *n,
			   void *arg)
{
	FILE *fp = (FILE*)arg;
	struct tcamsg *t = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[TCAA_MAX+1];

	len -= NLMSG_LENGTH(sizeof(*t));

	if (len < 0) {
		fprintf(stderr, "Wrong len %d\n", len);
		return -1;
	}

	parse_rtattr(tb, TCAA_MAX, TA_RTA(t), len);

	if (NULL == tb[TCA_ACT_TAB]) {
		if (n->nlmsg_type != RTM_GETACTION)
			fprintf(stderr, "print_action: NULL kind\n");
		return -1;
	}

	if (n->nlmsg_type == RTM_DELACTION) {
		if (n->nlmsg_flags & NLM_F_ROOT) {
			fprintf(fp, "Flushed table ");
			tab_flush = 1;
		} else {
			fprintf(fp, "deleted action ");
		}
	}

	if (n->nlmsg_type == RTM_NEWACTION)
		fprintf(fp, "Added action ");
	tc_print_action(fp, tb[TCA_ACT_TAB]);

	return 0;
}

int tc_action_gd(int cmd, unsigned flags, int *argc_p, char ***argv_p)
{
	char k[16];
	struct action_util *a = NULL;
	int argc = *argc_p;
	char **argv = *argv_p;
	int prio = 0;
	int ret = 0;
	__u32 i;
	struct sockaddr_nl nladdr;
	struct rtattr *tail;
	struct rtattr *tail2;
	struct nlmsghdr *ans = NULL;

	struct {
		struct nlmsghdr         n;
		struct tcamsg           t;
		char                    buf[MAX_MSG];
	} req;

	req.t.tca_family = AF_UNSPEC;

	memset(&req, 0, sizeof(req));

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcamsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	argc -=1;
	argv +=1;


	tail = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, MAX_MSG, TCA_ACT_TAB, NULL, 0);

	while (argc > 0) {
		if (strcmp(*argv, "action") == 0 ) {
			argc--;
			argv++;
			continue;
		} else if (strcmp(*argv, "help") == 0) {
			return -1;
		}

		strncpy(k, *argv, sizeof (k) - 1);
		a = get_action_kind(k);
		if (NULL == a) {
			fprintf(stderr, "Error: non existent action: %s\n",k);
			ret = -1;
			goto bad_val;
		}
		if (strcmp(a->id, k) != 0) {
			fprintf(stderr, "Error: non existent action: %s\n",k);
			ret = -1;
			goto bad_val;
		}

		argc -=1;
		argv +=1;
		if (argc <= 0) {
			fprintf(stderr, "Error: no index specified action: %s\n",k);
			ret = -1;
			goto bad_val;
		}

		if (matches(*argv, "index") == 0) {
			NEXT_ARG();
			if (get_u32(&i, *argv, 10)) {
				fprintf(stderr, "Illegal \"index\"\n");
				ret = -1;
				goto bad_val;
			}
			argc -=1;
			argv +=1;
		} else {
			fprintf(stderr, "Error: no index specified action: %s\n",k);
			ret = -1;
			goto bad_val;
		}

		tail2 = NLMSG_TAIL(&req.n);
		addattr_l(&req.n, MAX_MSG, ++prio, NULL, 0);
		addattr_l(&req.n, MAX_MSG, TCA_ACT_KIND, k, strlen(k) + 1);
		addattr32(&req.n, MAX_MSG, TCA_ACT_INDEX, i);
		tail2->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail2;

	}

	tail->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail;

	req.n.nlmsg_seq = rth.dump = ++rth.seq;
	if (cmd == RTM_GETACTION)
		ans = &req.n;

	if (rtnl_talk(&rth, &req.n, 0, 0, ans, NULL, NULL) < 0) {
		fprintf(stderr, "We have an error talking to the kernel\n");
		return 1;
	}

	if (ans && print_action(NULL, &req.n, (void*)stdout) < 0) {
		fprintf(stderr, "Dump terminated\n");
		return 1;
	}

	*argc_p = argc;
	*argv_p = argv;
bad_val:
	return ret;
}

int tc_action_modify(int cmd, unsigned flags, int *argc_p, char ***argv_p)
{
	int argc = *argc_p;
	char **argv = *argv_p;
	int ret = 0;

	struct rtattr *tail;
	struct {
		struct nlmsghdr         n;
		struct tcamsg           t;
		char                    buf[MAX_MSG];
	} req;

	req.t.tca_family = AF_UNSPEC;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcamsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	tail = NLMSG_TAIL(&req.n);
	argc -=1;
	argv +=1;
	if (parse_action(&argc, &argv, TCA_ACT_TAB, &req.n)) {
		fprintf(stderr, "Illegal \"action\"\n");
		return -1;
	}
	tail->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail;

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "We have an error talking to the kernel\n");
		ret = -1;
	}

	*argc_p = argc;
	*argv_p = argv;

	return ret;
}

int tc_act_list_or_flush(int argc, char **argv, int event)
{
	int ret = 0, prio = 0, msg_size = 0;
	char k[16];
	struct rtattr *tail,*tail2;
	struct action_util *a = NULL;
	struct {
		struct nlmsghdr         n;
		struct tcamsg           t;
		char                    buf[MAX_MSG];
	} req;

	req.t.tca_family = AF_UNSPEC;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcamsg));

	tail = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, MAX_MSG, TCA_ACT_TAB, NULL, 0);
	tail2 = NLMSG_TAIL(&req.n);

	strncpy(k, *argv, sizeof (k) - 1);
#ifdef CONFIG_GACT
	if (!gact_ld) {
		get_action_kind("gact");
	}
#endif
	a = get_action_kind(k);
	if (NULL == a) {
		fprintf(stderr,"bad action %s\n",k);
		goto bad_val;
	}
	if (strcmp(a->id, k) != 0) {
		fprintf(stderr,"bad action %s\n",k);
		goto bad_val;
	}
	strncpy(k, *argv, sizeof (k) - 1);

	addattr_l(&req.n, MAX_MSG, ++prio, NULL, 0);
	addattr_l(&req.n, MAX_MSG, TCA_ACT_KIND, k, strlen(k) + 1);
	tail2->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail2;
	tail->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail;

	msg_size = NLMSG_ALIGN(req.n.nlmsg_len) - NLMSG_ALIGN(sizeof(struct nlmsghdr));

	if (event == RTM_GETACTION) {
		if (rtnl_dump_request(&rth, event, (void *)&req.t, msg_size) < 0) {
			perror("Cannot send dump request");
			return 1;
		}
		ret = rtnl_dump_filter(&rth, print_action, stdout, NULL, NULL);
	}

	if (event == RTM_DELACTION) {
		req.n.nlmsg_len = NLMSG_ALIGN(req.n.nlmsg_len);
		req.n.nlmsg_type = RTM_DELACTION;
		req.n.nlmsg_flags |= NLM_F_ROOT;
		req.n.nlmsg_flags |= NLM_F_REQUEST;
		if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
			fprintf(stderr, "We have an error flushing\n");
			return 1;
		}

	}

bad_val:

	return ret;
}

int do_action(int argc, char **argv)
{

	int ret = 0;

	while (argc > 0) {

		if (matches(*argv, "add") == 0) {
			ret =  tc_action_modify(RTM_NEWACTION, NLM_F_EXCL|NLM_F_CREATE, &argc, &argv);
		} else if (matches(*argv, "change") == 0 ||
			  matches(*argv, "replace") == 0) {
			ret = tc_action_modify(RTM_NEWACTION, NLM_F_CREATE|NLM_F_REPLACE, &argc, &argv);
		} else if (matches(*argv, "delete") == 0) {
			argc -=1;
			argv +=1;
			ret = tc_action_gd(RTM_DELACTION, 0,  &argc, &argv);
		} else if (matches(*argv, "get") == 0) {
			argc -=1;
			argv +=1;
			ret = tc_action_gd(RTM_GETACTION, 0,  &argc, &argv);
		} else if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
						|| matches(*argv, "lst") == 0) {
			if (argc <= 2) {
				act_usage();
				return -1;
			}
			return tc_act_list_or_flush(argc-2, argv+2, RTM_GETACTION);
		} else if (matches(*argv, "flush") == 0) {
			if (argc <= 2) {
				act_usage();
				return -1;
			}
			return tc_act_list_or_flush(argc-2, argv+2, RTM_DELACTION);
		} else if (matches(*argv, "help") == 0) {
			act_usage();
			return -1;
		} else {

			ret = -1;
		}

		if (ret < 0) {
			fprintf(stderr, "Command \"%s\" is unknown, try \"tc actions help\".\n", *argv);
			return -1;
		}
	}

	return 0;
}


/*
 * m_ipt.c	iptables based targets
 * 		utilities mostly ripped from iptables <duh, its the linux way>
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:  J Hadi Salim (hadi@cyberus.ca)
 */

#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <iptables.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include "utils.h"
#include "tc_util.h"
#include <linux/tc_act/tc_ipt.h>
#include <stdio.h>
#include <dlfcn.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static const char *pname = "tc-ipt";
static const char *tname = "mangle";
static const char *pversion = "0.1";

static const char *ipthooks[] = {
	"NF_IP_PRE_ROUTING",
	"NF_IP_LOCAL_IN",
	"NF_IP_FORWARD",
	"NF_IP_LOCAL_OUT",
	"NF_IP_POST_ROUTING",
};

static struct option original_opts[] = {
	{"jump", 1, 0, 'j'},
	{0, 0, 0, 0}
};

static struct iptables_target *t_list = NULL;
static struct option *opts = original_opts;
static unsigned int global_option_offset = 0;
#define OPTION_OFFSET 256

char *lib_dir;

void
register_target(struct iptables_target *me)
{
/*      fprintf(stderr, "\nDummy register_target %s \n", me->name);
*/
	me->next = t_list;
	t_list = me;

}

void
xtables_register_target(struct iptables_target *me)
{
	me->next = t_list;
	t_list = me;
}

void
exit_tryhelp(int status)
{
	fprintf(stderr, "Try `%s -h' or '%s --help' for more information.\n",
		pname, pname);
	exit(status);
}

void
exit_error(enum exittype status, char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stderr, "%s v%s: ", pname, pversion);
	vfprintf(stderr, msg, args);
	va_end(args);
	fprintf(stderr, "\n");
	if (status == PARAMETER_PROBLEM)
		exit_tryhelp(status);
	if (status == VERSION_PROBLEM)
		fprintf(stderr,
			"Perhaps iptables or your kernel needs to be upgraded.\n");
	exit(status);
}

/* stolen from iptables 1.2.11
They should really have them as a library so i can link to them
Email them next time i remember
*/

char *
addr_to_dotted(const struct in_addr *addrp)
{
	static char buf[20];
	const unsigned char *bytep;

	bytep = (const unsigned char *) &(addrp->s_addr);
	sprintf(buf, "%d.%d.%d.%d", bytep[0], bytep[1], bytep[2], bytep[3]);
	return buf;
}

int string_to_number_ll(const char *s, unsigned long long min,
			unsigned long long max,
		 unsigned long long *ret)
{
	unsigned long long number;
	char *end;

	/* Handle hex, octal, etc. */
	errno = 0;
	number = strtoull(s, &end, 0);
	if (*end == '\0' && end != s) {
		/* we parsed a number, let's see if we want this */
		if (errno != ERANGE && min <= number && (!max || number <= max)) {
			*ret = number;
			return 0;
		}
	}
	return -1;
}

int string_to_number_l(const char *s, unsigned long min, unsigned long max,
		       unsigned long *ret)
{
	int result;
	unsigned long long number;

	result = string_to_number_ll(s, min, max, &number);
	*ret = (unsigned long)number;

	return result;
}

int string_to_number(const char *s, unsigned int min, unsigned int max,
		unsigned int *ret)
{
	int result;
	unsigned long number;

	result = string_to_number_l(s, min, max, &number);
	*ret = (unsigned int)number;

	return result;
}

static void free_opts(struct option *local_opts)
{
	if (local_opts != original_opts) {
		free(local_opts);
		opts = original_opts;
		global_option_offset = 0;
	}
}

static struct option *
merge_options(struct option *oldopts, const struct option *newopts,
	      unsigned int *option_offset)
{
	struct option *merge;
	unsigned int num_old, num_new, i;

	for (num_old = 0; oldopts[num_old].name; num_old++) ;
	for (num_new = 0; newopts[num_new].name; num_new++) ;

	*option_offset = global_option_offset + OPTION_OFFSET;

	merge = malloc(sizeof (struct option) * (num_new + num_old + 1));
	memcpy(merge, oldopts, num_old * sizeof (struct option));
	for (i = 0; i < num_new; i++) {
		merge[num_old + i] = newopts[i];
		merge[num_old + i].val += *option_offset;
	}
	memset(merge + num_old + num_new, 0, sizeof (struct option));

	return merge;
}

static void *
fw_calloc(size_t count, size_t size)
{
	void *p;

	if ((p = (void *) calloc(count, size)) == NULL) {
		perror("iptables: calloc failed");
		exit(1);
	}
	return p;
}

static struct iptables_target *
find_t(char *name)
{
	struct iptables_target *m;
	for (m = t_list; m; m = m->next) {
		if (strcmp(m->name, name) == 0)
			return m;
	}

	return NULL;
}

static struct iptables_target *
get_target_name(const char *name)
{
	void *handle;
	char *error;
	char *new_name, *lname;
	struct iptables_target *m;
	char path[strlen(lib_dir) + sizeof ("/libipt_.so") + strlen(name)];

#ifdef NO_SHARED_LIBS
	return NULL;
#endif

	new_name = malloc(strlen(name) + 1);
	lname = malloc(strlen(name) + 1);
	if (new_name)
		memset(new_name, '\0', strlen(name) + 1);
	else
		exit_error(PARAMETER_PROBLEM, "get_target_name");

	if (lname)
		memset(lname, '\0', strlen(name) + 1);
	else
		exit_error(PARAMETER_PROBLEM, "get_target_name");

	strcpy(new_name, name);
	strcpy(lname, name);

	if (isupper(lname[0])) {
		int i;
		for (i = 0; i < strlen(name); i++) {
			lname[i] = tolower(lname[i]);
		}
	}

	if (islower(new_name[0])) {
		int i;
		for (i = 0; i < strlen(new_name); i++) {
			new_name[i] = toupper(new_name[i]);
		}
	}

	/* try libxt_xx first */
	sprintf(path, "%s/libxt_%s.so", lib_dir, new_name);
	handle = dlopen(path, RTLD_LAZY);
	if (!handle) {
		/* try libipt_xx next */
		sprintf(path, "%s/libipt_%s.so", lib_dir, new_name);
		handle = dlopen(path, RTLD_LAZY);

		if (!handle) {
			sprintf(path, "%s/libxt_%s.so", lib_dir , lname);
			handle = dlopen(path, RTLD_LAZY);
		}

		if (!handle) {
			sprintf(path, "%s/libipt_%s.so", lib_dir , lname);
			handle = dlopen(path, RTLD_LAZY);
		}
		/* ok, lets give up .. */
		if (!handle) {
			fputs(dlerror(), stderr);
			printf("\n");
			free(new_name);
			free(lname);
			return NULL;
		}
	}

	m = dlsym(handle, new_name);
	if ((error = dlerror()) != NULL) {
		m = (struct iptables_target *) dlsym(handle, lname);
		if ((error = dlerror()) != NULL) {
			m = find_t(new_name);
			if (NULL == m) {
				m = find_t(lname);
				if (NULL == m) {
					fputs(error, stderr);
					fprintf(stderr, "\n");
					dlclose(handle);
					free(new_name);
					free(lname);
					return NULL;
				}
			}
		}
	}

	free(new_name);
	free(lname);
	return m;
}


struct in_addr *dotted_to_addr(const char *dotted)
{
	static struct in_addr addr;
	unsigned char *addrp;
	char *p, *q;
	unsigned int onebyte;
	int i;
	char buf[20];

	/* copy dotted string, because we need to modify it */
	strncpy(buf, dotted, sizeof (buf) - 1);
	addrp = (unsigned char *) &(addr.s_addr);

	p = buf;
	for (i = 0; i < 3; i++) {
		if ((q = strchr(p, '.')) == NULL)
			return (struct in_addr *) NULL;

		*q = '\0';
		if (string_to_number(p, 0, 255, &onebyte) == -1)
			return (struct in_addr *) NULL;

		addrp[i] = (unsigned char) onebyte;
		p = q + 1;
	}

	/* we've checked 3 bytes, now we check the last one */
	if (string_to_number(p, 0, 255, &onebyte) == -1)
		return (struct in_addr *) NULL;

	addrp[3] = (unsigned char) onebyte;

	return &addr;
}

static void set_revision(char *name, u_int8_t revision)
{
	/* Old kernel sources don't have ".revision" field,
	*  but we stole a byte from name. */
	name[IPT_FUNCTION_MAXNAMELEN - 2] = '\0';
	name[IPT_FUNCTION_MAXNAMELEN - 1] = revision;
}

/*
 * we may need to check for version mismatch
*/
int
build_st(struct iptables_target *target, struct ipt_entry_target *t)
{
	unsigned int nfcache = 0;

	if (target) {
		size_t size;

		size =
		    IPT_ALIGN(sizeof (struct ipt_entry_target)) + target->size;

		if (NULL == t) {
			target->t = fw_calloc(1, size);
			target->t->u.target_size = size;

			if (target->init != NULL)
				target->init(target->t, &nfcache);
			set_revision(target->t->u.user.name, target->revision);
		} else {
			target->t = t;
		}
		strcpy(target->t->u.user.name, target->name);
		return 0;
	}

	return -1;
}

static int parse_ipt(struct action_util *a,int *argc_p,
		     char ***argv_p, int tca_id, struct nlmsghdr *n)
{
	struct iptables_target *m = NULL;
	struct ipt_entry fw;
	struct rtattr *tail;
	int c;
	int rargc = *argc_p;
	char **argv = *argv_p;
	int argc = 0, iargc = 0;
	char k[16];
	int res = -1;
	int size = 0;
	int iok = 0, ok = 0;
	__u32 hook = 0, index = 0;
	res = 0;

	lib_dir = getenv("IPTABLES_LIB_DIR");
	if (!lib_dir)
		lib_dir = IPT_LIB_DIR;

	{
		int i;
		for (i = 0; i < rargc; i++) {
			if (NULL == argv[i] || 0 == strcmp(argv[i], "action")) {
				break;
			}
		}
		iargc = argc = i;
	}

	if (argc <= 2) {
		fprintf(stderr,"bad arguements to ipt %d vs %d \n", argc, rargc);
		return -1;
	}

	while (1) {
		c = getopt_long(argc, argv, "j:", opts, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'j':
			m = get_target_name(optarg);
			if (NULL != m) {

				if (0 > build_st(m, NULL)) {
					printf(" %s error \n", m->name);
					return -1;
				}
				opts =
				    merge_options(opts, m->extra_opts,
						  &m->option_offset);
			} else {
				fprintf(stderr," failed to find target %s\n\n", optarg);
				return -1;
			}
			ok++;
			break;

		default:
			memset(&fw, 0, sizeof (fw));
			if (m) {
				m->parse(c - m->option_offset, argv, 0,
					 &m->tflags, NULL, &m->t);
			} else {
				fprintf(stderr," failed to find target %s\n\n", optarg);
				return -1;

			}
			ok++;
			break;

		}
	}

	if (iargc > optind) {
		if (matches(argv[optind], "index") == 0) {
			if (get_u32(&index, argv[optind + 1], 10)) {
				fprintf(stderr, "Illegal \"index\"\n");
				free_opts(opts);
				return -1;
			}
			iok++;

			optind += 2;
		}
	}

	if (!ok && !iok) {
		fprintf(stderr," ipt Parser BAD!! (%s)\n", *argv);
		return -1;
	}

	/* check that we passed the correct parameters to the target */
	if (m)
		m->final_check(m->tflags);

	{
		struct tcmsg *t = NLMSG_DATA(n);
		if (t->tcm_parent != TC_H_ROOT
		    && t->tcm_parent == TC_H_MAJ(TC_H_INGRESS)) {
			hook = NF_IP_PRE_ROUTING;
		} else {
			hook = NF_IP_POST_ROUTING;
		}
	}

	tail = NLMSG_TAIL(n);
	addattr_l(n, MAX_MSG, tca_id, NULL, 0);
	fprintf(stdout, "tablename: %s hook: %s\n ", tname, ipthooks[hook]);
	fprintf(stdout, "\ttarget: ");

	if (m)
		m->print(NULL, m->t, 0);
	fprintf(stdout, " index %d\n", index);

	if (strlen(tname) > 16) {
		size = 16;
		k[15] = 0;
	} else {
		size = 1 + strlen(tname);
	}
	strncpy(k, tname, size);

	addattr_l(n, MAX_MSG, TCA_IPT_TABLE, k, size);
	addattr_l(n, MAX_MSG, TCA_IPT_HOOK, &hook, 4);
	addattr_l(n, MAX_MSG, TCA_IPT_INDEX, &index, 4);
	if (m)
		addattr_l(n, MAX_MSG, TCA_IPT_TARG, m->t, m->t->u.target_size);
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	argc -= optind;
	argv += optind;
	*argc_p = rargc - iargc;
	*argv_p = argv;

	optind = 0;
	free_opts(opts);
	/* Clear flags if target will be used again */
        m->tflags=0;
        m->used=0;
	/* Free allocated memory */
        if (m->t)
            free(m->t);


	return 0;

}

static int
print_ipt(struct action_util *au,FILE * f, struct rtattr *arg)
{
	struct rtattr *tb[TCA_IPT_MAX + 1];
	struct ipt_entry_target *t = NULL;

	if (arg == NULL)
		return -1;

	lib_dir = getenv("IPTABLES_LIB_DIR");
	if (!lib_dir)
		lib_dir = IPT_LIB_DIR;

	parse_rtattr_nested(tb, TCA_IPT_MAX, arg);

	if (tb[TCA_IPT_TABLE] == NULL) {
		fprintf(f, "[NULL ipt table name ] assuming mangle ");
	} else {
		fprintf(f, "tablename: %s ",
			(char *) RTA_DATA(tb[TCA_IPT_TABLE]));
	}

	if (tb[TCA_IPT_HOOK] == NULL) {
		fprintf(f, "[NULL ipt hook name ]\n ");
		return -1;
	} else {
		__u32 hook;
		hook = *(__u32 *) RTA_DATA(tb[TCA_IPT_HOOK]);
		fprintf(f, " hook: %s \n", ipthooks[hook]);
	}

	if (tb[TCA_IPT_TARG] == NULL) {
		fprintf(f, "\t[NULL ipt target parameters ] \n");
		return -1;
	} else {
		struct iptables_target *m = NULL;
		t = RTA_DATA(tb[TCA_IPT_TARG]);
		m = get_target_name(t->u.user.name);
		if (NULL != m) {
			if (0 > build_st(m, t)) {
				fprintf(stderr, " %s error \n", m->name);
				return -1;
			}

			opts =
			    merge_options(opts, m->extra_opts,
					  &m->option_offset);
		} else {
			fprintf(stderr, " failed to find target %s\n\n",
				t->u.user.name);
			return -1;
		}
		fprintf(f, "\ttarget ");
		m->print(NULL, m->t, 0);
		if (tb[TCA_IPT_INDEX] == NULL) {
			fprintf(f, " [NULL ipt target index ]\n");
		} else {
			__u32 index;
			index = *(__u32 *) RTA_DATA(tb[TCA_IPT_INDEX]);
			fprintf(f, " \n\tindex %d", index);
		}

		if (tb[TCA_IPT_CNT]) {
			struct tc_cnt *c  = RTA_DATA(tb[TCA_IPT_CNT]);;
			fprintf(f, " ref %d bind %d", c->refcnt, c->bindcnt);
		}
		if (show_stats) {
			if (tb[TCA_IPT_TM]) {
				struct tcf_t *tm = RTA_DATA(tb[TCA_IPT_TM]);
				print_tm(f,tm);
			}
		}
		fprintf(f, " \n");

	}
	free_opts(opts);

	return 0;
}

struct action_util ipt_action_util = {
        .id = "ipt",
        .parse_aopt = parse_ipt,
        .print_aopt = print_ipt,
};


/* Code to save the ip6tables state, in human readable-form. */
/* Author:  Andras Kis-Szabo <kisza@sch.bme.hu>
 * Original code: iptables-save
 * Authors: Paul 'Rusty' Russel <rusty@linuxcare.com.au> and
 * 	    Harald Welte <laforge@gnumonks.org>
 * This code is distributed under the terms of GNU GPL v2
 */
#include <getopt.h>
#include <sys/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "libiptc/libip6tc.h"
#include "ip6tables.h"

static int binary = 0, counters = 0;

static struct option options[] = {
	{ "binary", 0, 0, 'b' },
	{ "counters", 0, 0, 'c' },
	{ "dump", 0, 0, 'd' },
	{ "table", 1, 0, 't' },
	{ 0 }
};


/* This assumes that mask is contiguous, and byte-bounded. */
static void
print_iface(char letter, const char *iface, const unsigned char *mask,
	    int invert)
{
	unsigned int i;

	if (mask[0] == 0)
		return;

	printf("-%c %s", letter, invert ? "! " : "");

	for (i = 0; i < IFNAMSIZ; i++) {
		if (mask[i] != 0) {
			if (iface[i] != '\0')
				printf("%c", iface[i]);
		} else {
			/* we can access iface[i-1] here, because 
			 * a few lines above we make sure that mask[0] != 0 */
			if (iface[i-1] != '\0')
				printf("+");
			break;
		}
	}

	printf(" ");
}

/* These are hardcoded backups in ip6tables.c, so they are safe */
struct pprot {
	char *name;
	u_int8_t num;
};

static const struct pprot chain_protos[] = {
	{ "tcp", IPPROTO_TCP },
	{ "udp", IPPROTO_UDP },
	{ "icmpv6", IPPROTO_ICMPV6 },
	{ "esp", IPPROTO_ESP },
	{ "ah", IPPROTO_AH },
};

/* The ip6tables looks up the /etc/protocols. */
static void print_proto(u_int16_t proto, int invert)
{
	if (proto) {
		unsigned int i;
		const char *invertstr = invert ? "! " : "";

                struct protoent *pent = getprotobynumber(proto);
                if (pent) {
			printf("-p %s%s ",
			       invertstr, pent->p_name);
	                return;
		}

		for (i = 0; i < sizeof(chain_protos)/sizeof(struct pprot); i++)
			if (chain_protos[i].num == proto) {
				printf("-p %s%s ",
				       invertstr, chain_protos[i].name);
				return;
			}

		printf("-p %s%u ", invertstr, proto);
	}
}

static int print_match(const struct ip6t_entry_match *e,
			const struct ip6t_ip6 *ip)
{
	struct ip6tables_match *match
		= find_match(e->u.user.name, TRY_LOAD, NULL);

	if (match) {
		printf("-m %s ", e->u.user.name);

		/* some matches don't provide a save function */
		if (match->save)
			match->save(ip, e);
	} else {
		if (e->u.match_size) {
			fprintf(stderr,
				"Can't find library for match `%s'\n",
				e->u.user.name);
			exit(1);
		}
	}
	return 0;
}

/* print a given ip including mask if neccessary */
static void print_ip(char *prefix, const struct in6_addr *ip, const struct in6_addr *mask, int invert)
{
	char buf[51];
	int l = ipv6_prefix_length(mask);

	if (l == 0 && !invert)
		return;

	printf("%s %s%s",
		prefix,
		invert ? "! " : "",
		inet_ntop(AF_INET6, ip, buf, sizeof buf));

	if (l == -1)
		printf("/%s ", inet_ntop(AF_INET6, mask, buf, sizeof buf));
	else
		printf("/%d ", l);
}

/* We want this to be readable, so only print out neccessary fields.
 * Because that's the kind of world I want to live in.  */
static void print_rule(const struct ip6t_entry *e, 
		ip6tc_handle_t *h, const char *chain, int counters)
{
	struct ip6t_entry_target *t;
	const char *target_name;

	/* print counters */
	if (counters)
		printf("[%llu:%llu] ", (unsigned long long)e->counters.pcnt, (unsigned long long)e->counters.bcnt);

	/* print chain name */
	printf("-A %s ", chain);

	/* Print IP part. */
	print_ip("-s", &(e->ipv6.src), &(e->ipv6.smsk),
			e->ipv6.invflags & IP6T_INV_SRCIP);	

	print_ip("-d", &(e->ipv6.dst), &(e->ipv6.dmsk),
			e->ipv6.invflags & IP6T_INV_DSTIP);

	print_iface('i', e->ipv6.iniface, e->ipv6.iniface_mask,
		    e->ipv6.invflags & IP6T_INV_VIA_IN);

	print_iface('o', e->ipv6.outiface, e->ipv6.outiface_mask,
		    e->ipv6.invflags & IP6T_INV_VIA_OUT);

	print_proto(e->ipv6.proto, e->ipv6.invflags & IP6T_INV_PROTO);

#if 0
	/* not definied in ipv6
	 * FIXME: linux/netfilter_ipv6/ip6_tables: IP6T_INV_FRAG why definied? */
	if (e->ipv6.flags & IPT_F_FRAG)
		printf("%s-f ",
		       e->ipv6.invflags & IP6T_INV_FRAG ? "! " : "");
#endif

	if (e->ipv6.flags & IP6T_F_TOS)
		printf("%s-? %d ",
		       e->ipv6.invflags & IP6T_INV_TOS ? "! " : "", 
		       e->ipv6.tos);

	/* Print matchinfo part */
	if (e->target_offset) {
		IP6T_MATCH_ITERATE(e, print_match, &e->ipv6);
	}

	/* Print target name */	
	target_name = ip6tc_get_target(e, h);
	if (target_name && (*target_name != '\0'))
		printf("-j %s ", target_name);

	/* Print targinfo part */
	t = ip6t_get_target((struct ip6t_entry *)e);
	if (t->u.user.name[0]) {
		struct ip6tables_target *target
			= find_target(t->u.user.name, TRY_LOAD);

		if (!target) {
			fprintf(stderr, "Can't find library for target `%s'\n",
				t->u.user.name);
			exit(1);
		}

		if (target->save)
			target->save(&e->ipv6, t);
		else {
			/* If the target size is greater than ip6t_entry_target
			 * there is something to be saved, we just don't know
			 * how to print it */
			if (t->u.target_size != 
			    sizeof(struct ip6t_entry_target)) {
				fprintf(stderr, "Target `%s' is missing "
						"save function\n",
					t->u.user.name);
				exit(1);
			}
		}
	}
	printf("\n");
}

/* Debugging prototype. */
static int for_each_table(int (*func)(const char *tablename))
{
        int ret = 1;
	FILE *procfile = NULL;
	char tablename[IP6T_TABLE_MAXNAMELEN+1];

	procfile = fopen("/proc/net/ip6_tables_names", "r");
	if (!procfile)
		return 0;

	while (fgets(tablename, sizeof(tablename), procfile)) {
		if (tablename[strlen(tablename) - 1] != '\n')
			exit_error(OTHER_PROBLEM, 
				   "Badly formed tablename `%s'\n",
				   tablename);
		tablename[strlen(tablename) - 1] = '\0';
		ret &= func(tablename);
	}

	return ret;
}
	

static int do_output(const char *tablename)
{
	ip6tc_handle_t h;
	const char *chain = NULL;

	if (!tablename)
		return for_each_table(&do_output);

	h = ip6tc_init(tablename);
	if (!h)
 		exit_error(OTHER_PROBLEM, "Can't initialize: %s\n",
			   ip6tc_strerror(errno));

	if (!binary) {
		time_t now = time(NULL);

		printf("# Generated by ip6tables-save v%s on %s",
		       IPTABLES_VERSION, ctime(&now));
		printf("*%s\n", tablename);

		/* Dump out chain names first, 
		 * thereby preventing dependency conflicts */
		for (chain = ip6tc_first_chain(&h);
		     chain;
		     chain = ip6tc_next_chain(&h)) {

			printf(":%s ", chain);
			if (ip6tc_builtin(chain, h)) {
				struct ip6t_counters count;
				printf("%s ",
				       ip6tc_get_policy(chain, &count, &h));
				printf("[%llu:%llu]\n", (unsigned long long)count.pcnt, (unsigned long long)count.bcnt);
			} else {
				printf("- [0:0]\n");
			}
		}


		for (chain = ip6tc_first_chain(&h);
		     chain;
		     chain = ip6tc_next_chain(&h)) {
			const struct ip6t_entry *e;

			/* Dump out rules */
			e = ip6tc_first_rule(chain, &h);
			while(e) {
				print_rule(e, &h, chain, counters);
				e = ip6tc_next_rule(e, &h);
			}
		}

		now = time(NULL);
		printf("COMMIT\n");
		printf("# Completed on %s", ctime(&now));
	} else {
		/* Binary, huh?  OK. */
		exit_error(OTHER_PROBLEM, "Binary NYI\n");
	}

	ip6tc_free(&h);

	return 1;
}

/* Format:
 * :Chain name POLICY packets bytes
 * rule
 */
#ifdef IPTABLES_MULTI
int ip6tables_save_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	const char *tablename = NULL;
	int c;

	program_name = "ip6tables-save";
	program_version = IPTABLES_VERSION;

	lib_dir = getenv("IP6TABLES_LIB_DIR");
	if (!lib_dir)
		lib_dir = IP6T_LIB_DIR;

#ifdef NO_SHARED_LIBS
	init_extensions();
#endif

	while ((c = getopt_long(argc, argv, "bcdt:", options, NULL)) != -1) {
		switch (c) {
		case 'b':
			binary = 1;
			break;

		case 'c':
			counters = 1;
			break;

		case 't':
			/* Select specific table. */
			tablename = optarg;
			break;
		case 'd':
			do_output(tablename);
			exit(0);
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Unknown arguments found on commandline\n");
		exit(1);
	}

	return !do_output(tablename);
}

/* Code to take an iptables-style command line and do it. */

/*
 * Author: Paul.Russell@rustcorp.com.au and mneuling@radlogic.com.au
 *
 * (C) 2000-2002 by the netfilter coreteam <coreteam@netfilter.org>:
 * 		    Paul 'Rusty' Russell <rusty@rustcorp.com.au>
 * 		    Marc Boucher <marc+nf@mbsi.ca>
 * 		    James Morris <jmorris@intercode.com.au>
 * 		    Harald Welte <laforge@gnumonks.org>
 * 		    Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <getopt.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <iptables.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/utsname.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef PROC_SYS_MODPROBE
#define PROC_SYS_MODPROBE "/proc/sys/kernel/modprobe"
#endif

#define FMT_NUMERIC	0x0001
#define FMT_NOCOUNTS	0x0002
#define FMT_KILOMEGAGIGA 0x0004
#define FMT_OPTIONS	0x0008
#define FMT_NOTABLE	0x0010
#define FMT_NOTARGET	0x0020
#define FMT_VIA		0x0040
#define FMT_NONEWLINE	0x0080
#define FMT_LINENUMBERS 0x0100

#define FMT_PRINT_RULE (FMT_NOCOUNTS | FMT_OPTIONS | FMT_VIA \
			| FMT_NUMERIC | FMT_NOTABLE)
#define FMT(tab,notab) ((format) & FMT_NOTABLE ? (notab) : (tab))


#define CMD_NONE		0x0000U
#define CMD_INSERT		0x0001U
#define CMD_DELETE		0x0002U
#define CMD_DELETE_NUM		0x0004U
#define CMD_REPLACE		0x0008U
#define CMD_APPEND		0x0010U
#define CMD_LIST		0x0020U
#define CMD_FLUSH		0x0040U
#define CMD_ZERO		0x0080U
#define CMD_NEW_CHAIN		0x0100U
#define CMD_DELETE_CHAIN	0x0200U
#define CMD_SET_POLICY		0x0400U
#define CMD_RENAME_CHAIN	0x0800U
#define NUMBER_OF_CMD	13
static const char cmdflags[] = { 'I', 'D', 'D', 'R', 'A', 'L', 'F', 'Z',
				 'N', 'X', 'P', 'E' };

#define OPTION_OFFSET 256

#define OPT_NONE	0x00000U
#define OPT_NUMERIC	0x00001U
#define OPT_SOURCE	0x00002U
#define OPT_DESTINATION	0x00004U
#define OPT_PROTOCOL	0x00008U
#define OPT_JUMP	0x00010U
#define OPT_VERBOSE	0x00020U
#define OPT_EXPANDED	0x00040U
#define OPT_VIANAMEIN	0x00080U
#define OPT_VIANAMEOUT	0x00100U
#define OPT_FRAGMENT    0x00200U
#define OPT_LINENUMBERS 0x00400U
#define OPT_COUNTERS	0x00800U
#define NUMBER_OF_OPT	12
static const char optflags[NUMBER_OF_OPT]
= { 'n', 's', 'd', 'p', 'j', 'v', 'x', 'i', 'o', 'f', '0', 'c'};

static struct option original_opts[] = {
	{ "append", 1, 0, 'A' },
	{ "delete", 1, 0,  'D' },
	{ "insert", 1, 0,  'I' },
	{ "replace", 1, 0,  'R' },
	{ "list", 2, 0,  'L' },
	{ "flush", 2, 0,  'F' },
	{ "zero", 2, 0,  'Z' },
	{ "new-chain", 1, 0,  'N' },
	{ "delete-chain", 2, 0,  'X' },
	{ "rename-chain", 1, 0,  'E' },
	{ "policy", 1, 0,  'P' },
	{ "source", 1, 0, 's' },
	{ "destination", 1, 0,  'd' },
	{ "src", 1, 0,  's' }, /* synonym */
	{ "dst", 1, 0,  'd' }, /* synonym */
	{ "protocol", 1, 0,  'p' },
	{ "in-interface", 1, 0, 'i' },
	{ "jump", 1, 0, 'j' },
	{ "table", 1, 0, 't' },
	{ "match", 1, 0, 'm' },
	{ "numeric", 0, 0, 'n' },
	{ "out-interface", 1, 0, 'o' },
	{ "verbose", 0, 0, 'v' },
	{ "exact", 0, 0, 'x' },
	{ "fragments", 0, 0, 'f' },
	{ "version", 0, 0, 'V' },
	{ "help", 2, 0, 'h' },
	{ "line-numbers", 0, 0, '0' },
	{ "modprobe", 1, 0, 'M' },
	{ "set-counters", 1, 0, 'c' },
	{ "goto", 1, 0, 'g' },
	{ 0 }
};

/* we need this for iptables-restore.  iptables-restore.c sets line to the
 * current line of the input file, in order  to give a more precise error
 * message.  iptables itself doesn't need this, so it is initialized to the
 * magic number of -1 */
int line = -1;

static struct option *opts = original_opts;
static unsigned int global_option_offset = 0;

/* Table of legal combinations of commands and options.  If any of the
 * given commands make an option legal, that option is legal (applies to
 * CMD_LIST and CMD_ZERO only).
 * Key:
 *  +  compulsory
 *  x  illegal
 *     optional
 */

static char commands_v_options[NUMBER_OF_CMD][NUMBER_OF_OPT] =
/* Well, it's better than "Re: Linux vs FreeBSD" */
{
	/*     -n  -s  -d  -p  -j  -v  -x  -i  -o  -f  --line -c */
/*INSERT*/    {'x',' ',' ',' ',' ',' ','x',' ',' ',' ','x',' '},
/*DELETE*/    {'x',' ',' ',' ',' ',' ','x',' ',' ',' ','x','x'},
/*DELETE_NUM*/{'x','x','x','x','x',' ','x','x','x','x','x','x'},
/*REPLACE*/   {'x',' ',' ',' ',' ',' ','x',' ',' ',' ','x',' '},
/*APPEND*/    {'x',' ',' ',' ',' ',' ','x',' ',' ',' ','x',' '},
/*LIST*/      {' ','x','x','x','x',' ',' ','x','x','x',' ','x'},
/*FLUSH*/     {'x','x','x','x','x',' ','x','x','x','x','x','x'},
/*ZERO*/      {'x','x','x','x','x',' ','x','x','x','x','x','x'},
/*NEW_CHAIN*/ {'x','x','x','x','x',' ','x','x','x','x','x','x'},
/*DEL_CHAIN*/ {'x','x','x','x','x',' ','x','x','x','x','x','x'},
/*SET_POLICY*/{'x','x','x','x','x',' ','x','x','x','x','x','x'},
/*RENAME*/    {'x','x','x','x','x',' ','x','x','x','x','x','x'}
};

static int inverse_for_options[NUMBER_OF_OPT] =
{
/* -n */ 0,
/* -s */ IPT_INV_SRCIP,
/* -d */ IPT_INV_DSTIP,
/* -p */ IPT_INV_PROTO,
/* -j */ 0,
/* -v */ 0,
/* -x */ 0,
/* -i */ IPT_INV_VIA_IN,
/* -o */ IPT_INV_VIA_OUT,
/* -f */ IPT_INV_FRAG,
/*--line*/ 0,
/* -c */ 0,
};

const char *program_version;
const char *program_name;
char *lib_dir;

int kernel_version;

/* the path to command to load kernel module */
const char *modprobe = NULL;

/* Keeping track of external matches and targets: linked lists.  */
struct iptables_match *iptables_matches = NULL;
struct iptables_target *iptables_targets = NULL;

/* Extra debugging from libiptc */
extern void dump_entries(const iptc_handle_t handle);

/* A few hardcoded protocols for 'all' and in case the user has no
   /etc/protocols */
struct pprot {
	char *name;
	u_int8_t num;
};

/* Primitive headers... */
/* defined in netinet/in.h */
#if 0
#ifndef IPPROTO_ESP
#define IPPROTO_ESP 50
#endif
#ifndef IPPROTO_AH
#define IPPROTO_AH 51
#endif
#endif

static const struct pprot chain_protos[] = {
	{ "tcp", IPPROTO_TCP },
	{ "udp", IPPROTO_UDP },
	{ "udplite", IPPROTO_UDPLITE },
	{ "icmp", IPPROTO_ICMP },
	{ "esp", IPPROTO_ESP },
	{ "ah", IPPROTO_AH },
	{ "sctp", IPPROTO_SCTP },
	{ "all", 0 },
};

static char *
proto_to_name(u_int8_t proto, int nolookup)
{
	unsigned int i;

	if (proto && !nolookup) {
		struct protoent *pent = getprotobynumber(proto);
		if (pent)
			return pent->p_name;
	}

	for (i = 0; i < sizeof(chain_protos)/sizeof(struct pprot); i++)
		if (chain_protos[i].num == proto)
			return chain_protos[i].name;

	return NULL;
}

int
service_to_port(const char *name, const char *proto)
{
	struct servent *service;

	if ((service = getservbyname(name, proto)) != NULL)
		return ntohs((unsigned short) service->s_port);

	return -1;
}

u_int16_t
parse_port(const char *port, const char *proto)
{
	unsigned int portnum;

	if ((string_to_number(port, 0, 65535, &portnum)) != -1 ||
	    (portnum = service_to_port(port, proto)) != -1)
		return (u_int16_t)portnum;

	exit_error(PARAMETER_PROBLEM,
		   "invalid port/service `%s' specified", port);
}

enum {
	IPT_DOTTED_ADDR = 0,
	IPT_DOTTED_MASK
};

static struct in_addr *
__dotted_to_addr(const char *dotted, int type)
{
	static struct in_addr addr;
	unsigned char *addrp;
	char *p, *q;
	unsigned int onebyte;
	int i;
	char buf[20];

	/* copy dotted string, because we need to modify it */
	strncpy(buf, dotted, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	addrp = (unsigned char *) &(addr.s_addr);

	p = buf;
	for (i = 0; i < 3; i++) {
		if ((q = strchr(p, '.')) == NULL) {
			if (type == IPT_DOTTED_ADDR) {
				/* autocomplete, this is a network address */
				if (string_to_number(p, 0, 255, &onebyte) == -1)
					return (struct in_addr *) NULL;

				addrp[i] = (unsigned char) onebyte;
				while (i < 3)
					addrp[++i] = 0;

				return &addr;
			} else
				return (struct in_addr *) NULL;
		}

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

struct in_addr *
dotted_to_addr(const char *dotted)
{
	return __dotted_to_addr(dotted, IPT_DOTTED_ADDR);
}

struct in_addr *
dotted_to_mask(const char *dotted)
{
	return __dotted_to_addr(dotted, IPT_DOTTED_MASK);
}

static struct in_addr *
network_to_addr(const char *name)
{
	struct netent *net;
	static struct in_addr addr;

	if ((net = getnetbyname(name)) != NULL) {
		if (net->n_addrtype != AF_INET)
			return (struct in_addr *) NULL;
		addr.s_addr = htonl((unsigned long) net->n_net);
		return &addr;
	}

	return (struct in_addr *) NULL;
}

static void
inaddrcpy(struct in_addr *dst, struct in_addr *src)
{
	/* memcpy(dst, src, sizeof(struct in_addr)); */
	dst->s_addr = src->s_addr;
}

static void free_opts(int reset_offset)
{
	if (opts != original_opts) {
		free(opts);
		opts = original_opts;
		if (reset_offset)
			global_option_offset = 0;
	}
}

void
exit_error(enum exittype status, char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stderr, "%s v%s: ", program_name, program_version);
	vfprintf(stderr, msg, args);
	va_end(args);
	fprintf(stderr, "\n");
	if (status == PARAMETER_PROBLEM)
		exit_tryhelp(status);
	if (status == VERSION_PROBLEM)
		fprintf(stderr,
			"Perhaps iptables or your kernel needs to be upgraded.\n");
	/* On error paths, make sure that we don't leak memory */
	free_opts(1);
	exit(status);
}

void
exit_tryhelp(int status)
{
	if (line != -1)
		fprintf(stderr, "Error occurred at line: %d\n", line);
	fprintf(stderr, "Try `%s -h' or '%s --help' for more information.\n",
			program_name, program_name );
	free_opts(1);
	exit(status);
}

void
exit_printhelp(struct iptables_rule_match *matches)
{
	struct iptables_rule_match *matchp = NULL;
	struct iptables_target *t = NULL;

	printf("%s v%s\n\n"
"Usage: %s -[AD] chain rule-specification [options]\n"
"       %s -[RI] chain rulenum rule-specification [options]\n"
"       %s -D chain rulenum [options]\n"
"       %s -[LFZ] [chain] [options]\n"
"       %s -[NX] chain\n"
"       %s -E old-chain-name new-chain-name\n"
"       %s -P chain target [options]\n"
"       %s -h (print this help information)\n\n",
	       program_name, program_version, program_name, program_name,
	       program_name, program_name, program_name, program_name,
	       program_name, program_name);

	printf(
"Commands:\n"
"Either long or short options are allowed.\n"
"  --append  -A chain		Append to chain\n"
"  --delete  -D chain		Delete matching rule from chain\n"
"  --delete  -D chain rulenum\n"
"				Delete rule rulenum (1 = first) from chain\n"
"  --insert  -I chain [rulenum]\n"
"				Insert in chain as rulenum (default 1=first)\n"
"  --replace -R chain rulenum\n"
"				Replace rule rulenum (1 = first) in chain\n"
"  --list    -L [chain]		List the rules in a chain or all chains\n"
"  --flush   -F [chain]		Delete all rules in  chain or all chains\n"
"  --zero    -Z [chain]		Zero counters in chain or all chains\n"
"  --new     -N chain		Create a new user-defined chain\n"
"  --delete-chain\n"
"            -X [chain]		Delete a user-defined chain\n"
"  --policy  -P chain target\n"
"				Change policy on chain to target\n"
"  --rename-chain\n"
"            -E old-chain new-chain\n"
"				Change chain name, (moving any references)\n"

"Options:\n"
"  --proto	-p [!] proto	protocol: by number or name, eg. `tcp'\n"
"  --source	-s [!] address[/mask]\n"
"				source specification\n"
"  --destination -d [!] address[/mask]\n"
"				destination specification\n"
"  --in-interface -i [!] input name[+]\n"
"				network interface name ([+] for wildcard)\n"
"  --jump	-j target\n"
"				target for rule (may load target extension)\n"
#ifdef IPT_F_GOTO
"  --goto      -g chain\n"
"                              jump to chain with no return\n"
#endif
"  --match	-m match\n"
"				extended match (may load extension)\n"
"  --numeric	-n		numeric output of addresses and ports\n"
"  --out-interface -o [!] output name[+]\n"
"				network interface name ([+] for wildcard)\n"
"  --table	-t table	table to manipulate (default: `filter')\n"
"  --verbose	-v		verbose mode\n"
"  --line-numbers		print line numbers when listing\n"
"  --exact	-x		expand numbers (display exact values)\n"
"[!] --fragment	-f		match second or further fragments only\n"
"  --modprobe=<command>		try to insert modules using this command\n"
"  --set-counters PKTS BYTES	set the counter during insert/append\n"
"[!] --version	-V		print package version.\n");

	/* Print out any special helps. A user might like to be able
	   to add a --help to the commandline, and see expected
	   results. So we call help for all specified matches & targets */
	for (t = iptables_targets; t ;t = t->next) {
		if (t->used) {
			printf("\n");
			t->help();
		}
	}
	for (matchp = matches; matchp; matchp = matchp->next) {
		printf("\n");
		matchp->match->help();
	}
	exit(0);
}

static void
generic_opt_check(int command, int options)
{
	int i, j, legal = 0;

	/* Check that commands are valid with options.  Complicated by the
	 * fact that if an option is legal with *any* command given, it is
	 * legal overall (ie. -z and -l).
	 */
	for (i = 0; i < NUMBER_OF_OPT; i++) {
		legal = 0; /* -1 => illegal, 1 => legal, 0 => undecided. */

		for (j = 0; j < NUMBER_OF_CMD; j++) {
			if (!(command & (1<<j)))
				continue;

			if (!(options & (1<<i))) {
				if (commands_v_options[j][i] == '+')
					exit_error(PARAMETER_PROBLEM,
						   "You need to supply the `-%c' "
						   "option for this command\n",
						   optflags[i]);
			} else {
				if (commands_v_options[j][i] != 'x')
					legal = 1;
				else if (legal == 0)
					legal = -1;
			}
		}
		if (legal == -1)
			exit_error(PARAMETER_PROBLEM,
				   "Illegal option `-%c' with this command\n",
				   optflags[i]);
	}
}

static char
opt2char(int option)
{
	const char *ptr;
	for (ptr = optflags; option > 1; option >>= 1, ptr++);

	return *ptr;
}

static char
cmd2char(int option)
{
	const char *ptr;
	for (ptr = cmdflags; option > 1; option >>= 1, ptr++);

	return *ptr;
}

static void
add_command(unsigned int *cmd, const int newcmd, const int othercmds, 
	    int invert)
{
	if (invert)
		exit_error(PARAMETER_PROBLEM, "unexpected ! flag");
	if (*cmd & (~othercmds))
		exit_error(PARAMETER_PROBLEM, "Can't use -%c with -%c\n",
			   cmd2char(newcmd), cmd2char(*cmd & (~othercmds)));
	*cmd |= newcmd;
}

int
check_inverse(const char option[], int *invert, int *optind, int argc)
{
	if (option && strcmp(option, "!") == 0) {
		if (*invert)
			exit_error(PARAMETER_PROBLEM,
				   "Multiple `!' flags not allowed");
		*invert = TRUE;
		if (optind) {
			*optind = *optind+1;
			if (argc && *optind > argc)
				exit_error(PARAMETER_PROBLEM,
					   "no argument following `!'");
		}

		return TRUE;
	}
	return FALSE;
}

static void *
fw_calloc(size_t count, size_t size)
{
	void *p;

	if ((p = calloc(count, size)) == NULL) {
		perror("iptables: calloc failed");
		exit(1);
	}
	return p;
}

static void *
fw_malloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL) {
		perror("iptables: malloc failed");
		exit(1);
	}
	return p;
}

static struct in_addr *
host_to_addr(const char *name, unsigned int *naddr)
{
	struct hostent *host;
	struct in_addr *addr;
	unsigned int i;

	*naddr = 0;
	if ((host = gethostbyname(name)) != NULL) {
		if (host->h_addrtype != AF_INET ||
		    host->h_length != sizeof(struct in_addr))
			return (struct in_addr *) NULL;

		while (host->h_addr_list[*naddr] != (char *) NULL)
			(*naddr)++;
		addr = fw_calloc(*naddr, sizeof(struct in_addr) * *naddr);
		for (i = 0; i < *naddr; i++)
			inaddrcpy(&(addr[i]),
				  (struct in_addr *) host->h_addr_list[i]);
		return addr;
	}

	return (struct in_addr *) NULL;
}

static char *
addr_to_host(const struct in_addr *addr)
{
	struct hostent *host;

	if ((host = gethostbyaddr((char *) addr,
				  sizeof(struct in_addr), AF_INET)) != NULL)
		return (char *) host->h_name;

	return (char *) NULL;
}

/*
 *	All functions starting with "parse" should succeed, otherwise
 *	the program fails.
 *	Most routines return pointers to static data that may change
 *	between calls to the same or other routines with a few exceptions:
 *	"host_to_addr", "parse_hostnetwork", and "parse_hostnetworkmask"
 *	return global static data.
*/

static struct in_addr *
parse_hostnetwork(const char *name, unsigned int *naddrs)
{
	struct in_addr *addrp, *addrptmp;

	if ((addrptmp = dotted_to_addr(name)) != NULL ||
	    (addrptmp = network_to_addr(name)) != NULL) {
		addrp = fw_malloc(sizeof(struct in_addr));
		inaddrcpy(addrp, addrptmp);
		*naddrs = 1;
		return addrp;
	}
	if ((addrp = host_to_addr(name, naddrs)) != NULL)
		return addrp;

	exit_error(PARAMETER_PROBLEM, "host/network `%s' not found", name);
}

static struct in_addr *
parse_mask(char *mask)
{
	static struct in_addr maskaddr;
	struct in_addr *addrp;
	unsigned int bits;

	if (mask == NULL) {
		/* no mask at all defaults to 32 bits */
		maskaddr.s_addr = 0xFFFFFFFF;
		return &maskaddr;
	}
	if ((addrp = dotted_to_mask(mask)) != NULL)
		/* dotted_to_addr already returns a network byte order addr */
		return addrp;
	if (string_to_number(mask, 0, 32, &bits) == -1)
		exit_error(PARAMETER_PROBLEM,
			   "invalid mask `%s' specified", mask);
	if (bits != 0) {
		maskaddr.s_addr = htonl(0xFFFFFFFF << (32 - bits));
		return &maskaddr;
	}

	maskaddr.s_addr = 0L;
	return &maskaddr;
}

void
parse_hostnetworkmask(const char *name, struct in_addr **addrpp,
		      struct in_addr *maskp, unsigned int *naddrs)
{
	struct in_addr *addrp;
	char buf[256];
	char *p;
	int i, j, k, n;

	strncpy(buf, name, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	if ((p = strrchr(buf, '/')) != NULL) {
		*p = '\0';
		addrp = parse_mask(p + 1);
	} else
		addrp = parse_mask(NULL);
	inaddrcpy(maskp, addrp);

	/* if a null mask is given, the name is ignored, like in "any/0" */
	if (maskp->s_addr == 0L)
		strcpy(buf, "0.0.0.0");

	addrp = *addrpp = parse_hostnetwork(buf, naddrs);
	n = *naddrs;
	for (i = 0, j = 0; i < n; i++) {
		addrp[j++].s_addr &= maskp->s_addr;
		for (k = 0; k < j - 1; k++) {
			if (addrp[k].s_addr == addrp[j - 1].s_addr) {
				(*naddrs)--;
				j--;
				break;
			}
		}
	}
}

struct iptables_match *
find_match(const char *name, enum ipt_tryload tryload, struct iptables_rule_match **matches)
{
	struct iptables_match *ptr;

	for (ptr = iptables_matches; ptr; ptr = ptr->next) {
		if (strcmp(name, ptr->name) == 0) {
			struct iptables_match *clone;
			
			/* First match of this type: */
			if (ptr->m == NULL)
				break;

			/* Second and subsequent clones */
			clone = fw_malloc(sizeof(struct iptables_match));
			memcpy(clone, ptr, sizeof(struct iptables_match));
			clone->mflags = 0;
			/* This is a clone: */
			clone->next = clone;

			ptr = clone;
			break;
		}
	}		

#ifndef NO_SHARED_LIBS
	if (!ptr && tryload != DONT_LOAD && tryload != DURING_LOAD) {
		char path[strlen(lib_dir) + sizeof("/libipt_.so")
			 + strlen(name)];
#if 1	// for easier testing	zzz
		char s[256];
		sprintf(s, "%s/libipt_%s.so", "/etc/iptext/", name);
		if (dlopen(s, RTLD_NOW)) goto OPENED;
#endif
		sprintf(path, "%s/libipt_%s.so", lib_dir, name);
		if (dlopen(path, RTLD_NOW)) {
OPENED:
			/* Found library.  If it didn't register itself,
			   maybe they specified target as match. */
			ptr = find_match(name, DONT_LOAD, NULL);

			if (!ptr)
				exit_error(PARAMETER_PROBLEM,
					   "Couldn't load match `%s'\n",
					   name);
		} else if (tryload == LOAD_MUST_SUCCEED)
			exit_error(PARAMETER_PROBLEM,
				   "Couldn't load match `%s':%s\n",
				   name, dlerror());
	}
#else
	if (ptr && !ptr->loaded) {
		if (tryload != DONT_LOAD)
			ptr->loaded = 1;
		else
			ptr = NULL;
	}
	if(!ptr && (tryload == LOAD_MUST_SUCCEED)) {
		exit_error(PARAMETER_PROBLEM,
			   "Couldn't find match `%s'\n", name);
	}
#endif

	if (ptr && matches) {
		struct iptables_rule_match **i;
		struct iptables_rule_match *newentry;

		newentry = fw_malloc(sizeof(struct iptables_rule_match));

		for (i = matches; *i; i = &(*i)->next) {
			if (strcmp(name, (*i)->match->name) == 0)
				(*i)->completed = 1;
		}
		newentry->match = ptr;
		newentry->completed = 0;
		newentry->next = NULL;
		*i = newentry;
	}

	return ptr;
}

/* Christophe Burki wants `-p 6' to imply `-m tcp'.  */
static struct iptables_match *
find_proto(const char *pname, enum ipt_tryload tryload, int nolookup, struct iptables_rule_match **matches)
{
	unsigned int proto;

	if (string_to_number(pname, 0, 255, &proto) != -1) {
		char *protoname = proto_to_name(proto, nolookup);

		if (protoname)
			return find_match(protoname, tryload, matches);
	} else
		return find_match(pname, tryload, matches);

	return NULL;
}

u_int16_t
parse_protocol(const char *s)
{
	unsigned int proto;

	if (string_to_number(s, 0, 255, &proto) == -1) {
		struct protoent *pent;

		/* first deal with the special case of 'all' to prevent
		 * people from being able to redefine 'all' in nsswitch
		 * and/or provoke expensive [not working] ldap/nis/... 
		 * lookups */
		if (!strcmp(s, "all"))
			return 0;

		if ((pent = getprotobyname(s)))
			proto = pent->p_proto;
		else {
			unsigned int i;
			for (i = 0;
			     i < sizeof(chain_protos)/sizeof(struct pprot);
			     i++) {
				if (strcmp(s, chain_protos[i].name) == 0) {
					proto = chain_protos[i].num;
					break;
				}
			}
			if (i == sizeof(chain_protos)/sizeof(struct pprot))
				exit_error(PARAMETER_PROBLEM,
					   "unknown protocol `%s' specified",
					   s);
		}
	}

	return (u_int16_t)proto;
}

void parse_interface(const char *arg, char *vianame, unsigned char *mask)
{
	int vialen = strlen(arg);
	unsigned int i;

	memset(mask, 0, IFNAMSIZ);
	memset(vianame, 0, IFNAMSIZ);

	if (vialen + 1 > IFNAMSIZ)
		exit_error(PARAMETER_PROBLEM,
			   "interface name `%s' must be shorter than IFNAMSIZ"
			   " (%i)", arg, IFNAMSIZ-1);

	strcpy(vianame, arg);
	if ((vialen == 0) || (vialen == 1 && vianame[0] == '+'))
		memset(mask, 0, IFNAMSIZ);
	else if (vianame[vialen - 1] == '+') {
		memset(mask, 0xFF, vialen - 1);
		memset(mask + vialen - 1, 0, IFNAMSIZ - vialen + 1);
		/* Don't remove `+' here! -HW */
	} else {
		/* Include nul-terminator in match */
		memset(mask, 0xFF, vialen + 1);
		memset(mask + vialen + 1, 0, IFNAMSIZ - vialen - 1);
		for (i = 0; vianame[i]; i++) {
			if (vianame[i] == ':' ||
			    vianame[i] == '!' ||
			    vianame[i] == '*') {
				printf("Warning: weird character in interface"
				       " `%s' (No aliases, :, ! or *).\n",
				       vianame);
				break;
			}
		}
	}
}

/* Can't be zero. */
static int
parse_rulenumber(const char *rule)
{
	unsigned int rulenum;

	if (string_to_number(rule, 1, INT_MAX, &rulenum) == -1)
		exit_error(PARAMETER_PROBLEM,
			   "Invalid rule number `%s'", rule);

	return rulenum;
}

static const char *
parse_target(const char *targetname)
{
	const char *ptr;

	if (strlen(targetname) < 1)
		exit_error(PARAMETER_PROBLEM,
			   "Invalid target name (too short)");

	if (strlen(targetname)+1 > sizeof(ipt_chainlabel))
		exit_error(PARAMETER_PROBLEM,
			   "Invalid target name `%s' (%u chars max)",
			   targetname, (unsigned int)sizeof(ipt_chainlabel)-1);

	for (ptr = targetname; *ptr; ptr++)
		if (isspace(*ptr))
			exit_error(PARAMETER_PROBLEM,
				   "Invalid target name `%s'", targetname);
	return targetname;
}

static char *
addr_to_network(const struct in_addr *addr)
{
	struct netent *net;

	if ((net = getnetbyaddr((long) ntohl(addr->s_addr), AF_INET)) != NULL)
		return (char *) net->n_name;

	return (char *) NULL;
}

char *
addr_to_dotted(const struct in_addr *addrp)
{
	static char buf[20];
	const unsigned char *bytep;

	bytep = (const unsigned char *) &(addrp->s_addr);
	sprintf(buf, "%d.%d.%d.%d", bytep[0], bytep[1], bytep[2], bytep[3]);
	return buf;
}

char *
addr_to_anyname(const struct in_addr *addr)
{
	char *name;

	if ((name = addr_to_host(addr)) != NULL ||
	    (name = addr_to_network(addr)) != NULL)
		return name;

	return addr_to_dotted(addr);
}

char *
mask_to_dotted(const struct in_addr *mask)
{
	int i;
	static char buf[20];
	u_int32_t maskaddr, bits;

	maskaddr = ntohl(mask->s_addr);

	if (maskaddr == 0xFFFFFFFFL)
		/* we don't want to see "/32" */
		return "";

	i = 32;
	bits = 0xFFFFFFFEL;
	while (--i >= 0 && maskaddr != bits)
		bits <<= 1;
	if (i >= 0)
		sprintf(buf, "/%d", i);
	else
		/* mask was not a decent combination of 1's and 0's */
		sprintf(buf, "/%s", addr_to_dotted(mask));

	return buf;
}

int
string_to_number_ll(const char *s, unsigned long long min, unsigned long long max,
		 unsigned long long *ret)
{
	unsigned long long number;
	char *end;

	/* Handle hex, octal, etc. */
	errno = 0;
	number = strtoull(s, &end, 0);
	if ((*end == '\0' || *end == '/') && end != s) {
		/* we parsed a number, let's see if we want this */
		if (errno != ERANGE && min <= number && (!max || number <= max)) {
			*ret = number;
			return 0;
		}
	}
	return -1;
}

int
string_to_number_l(const char *s, unsigned long min, unsigned long max,
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

static void
set_option(unsigned int *options, unsigned int option, u_int8_t *invflg,
	   int invert)
{
	if (*options & option)
		exit_error(PARAMETER_PROBLEM, "multiple -%c flags not allowed",
			   opt2char(option));
	*options |= option;

	if (invert) {
		unsigned int i;
		for (i = 0; 1 << i != option; i++);

		if (!inverse_for_options[i])
			exit_error(PARAMETER_PROBLEM,
				   "cannot have ! before -%c",
				   opt2char(option));
		*invflg |= inverse_for_options[i];
	}
}

struct iptables_target *
find_target(const char *name, enum ipt_tryload tryload)
{
	struct iptables_target *ptr;

	/* Standard target? */
	if (strcmp(name, "") == 0
	    || strcmp(name, IPTC_LABEL_ACCEPT) == 0
	    || strcmp(name, IPTC_LABEL_DROP) == 0
	    || strcmp(name, IPTC_LABEL_QUEUE) == 0
	    || strcmp(name, IPTC_LABEL_RETURN) == 0)
		name = "standard";

	for (ptr = iptables_targets; ptr; ptr = ptr->next) {
		if (strcmp(name, ptr->name) == 0)
			break;
	}

#ifndef NO_SHARED_LIBS
	if (!ptr && tryload != DONT_LOAD && tryload != DURING_LOAD) {
		char path[strlen(lib_dir) + sizeof("/libipt_.so")
			 + strlen(name)];
#if 1	// for easier testing	zzz
		char s[256];
		sprintf(s, "%s/libipt_%s.so", "/etc/iptext/", name);
		if (dlopen(s, RTLD_NOW)) goto OPENED;
#endif
		sprintf(path, "%s/libipt_%s.so", lib_dir, name);
		if (dlopen(path, RTLD_NOW)) {
OPENED:
			/* Found library.  If it didn't register itself,
			   maybe they specified match as a target. */
			ptr = find_target(name, DONT_LOAD);
			if (!ptr)
				exit_error(PARAMETER_PROBLEM,
					   "Couldn't load target `%s'\n",
					   name);
		} else if (tryload == LOAD_MUST_SUCCEED)
			exit_error(PARAMETER_PROBLEM,
				   "Couldn't load target `%s':%s\n",
				   name, dlerror());
	}
#else
	if (ptr && !ptr->loaded) {
		if (tryload != DONT_LOAD)
			ptr->loaded = 1;
		else
			ptr = NULL;
	}
	if(!ptr && (tryload == LOAD_MUST_SUCCEED)) {
		exit_error(PARAMETER_PROBLEM,
			   "Couldn't find target `%s'\n", name);
	}
#endif

	if (ptr)
		ptr->used = 1;

	return ptr;
}

static struct option *
merge_options(struct option *oldopts, const struct option *newopts,
	      unsigned int *option_offset)
{
	unsigned int num_old, num_new, i;
	struct option *merge;

	for (num_old = 0; oldopts[num_old].name; num_old++);
	for (num_new = 0; newopts[num_new].name; num_new++);

	global_option_offset += OPTION_OFFSET;
	*option_offset = global_option_offset;

	merge = malloc(sizeof(struct option) * (num_new + num_old + 1));
	memcpy(merge, oldopts, num_old * sizeof(struct option));
	free_opts(0); /* Release previous options merged if any */
	for (i = 0; i < num_new; i++) {
		merge[num_old + i] = newopts[i];
		merge[num_old + i].val += *option_offset;
	}
	memset(merge + num_old + num_new, 0, sizeof(struct option));

	return merge;
}

static int compatible_revision(const char *name, u_int8_t revision, int opt)
{
	struct ipt_get_revision rev;
	socklen_t s = sizeof(rev);
	int max_rev, sockfd;

	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (sockfd < 0) {
		fprintf(stderr, "Could not open socket to kernel: %s\n",
			strerror(errno));
		exit(1);
	}

	load_iptables_ko(modprobe, 1);

	strcpy(rev.name, name);
	rev.revision = revision;

	max_rev = getsockopt(sockfd, IPPROTO_IP, opt, &rev, &s);
	if (max_rev < 0) {
		/* Definitely don't support this? */
		if (errno == ENOENT || errno == EPROTONOSUPPORT) {
			close(sockfd);
			return 0;
		} else if (errno == ENOPROTOOPT) {
			close(sockfd);
			/* Assume only revision 0 support (old kernel) */
			return (revision == 0);
		} else {
			fprintf(stderr, "getsockopt failed strangely: %s\n",
				strerror(errno));
			exit(1);
		}
	}
	close(sockfd);
	return 1;
}

static int compatible_match_revision(const char *name, u_int8_t revision)
{
	return compatible_revision(name, revision, IPT_SO_GET_REVISION_MATCH);
}

static int compatible_target_revision(const char *name, u_int8_t revision)
{
	return compatible_revision(name, revision, IPT_SO_GET_REVISION_TARGET);
}

void
register_match(struct iptables_match *me)
{
	struct iptables_match **i, *old;

	if (strcmp(me->version, program_version) != 0) {
		fprintf(stderr, "%s: match `%s' v%s (I'm v%s).\n",
			program_name, me->name, me->version, program_version);
		exit(1);
	}

	/* Revision field stole a char from name. */
	if (strlen(me->name) >= IPT_FUNCTION_MAXNAMELEN-1) {
		fprintf(stderr, "%s: target `%s' has invalid name\n",
			program_name, me->name);
		exit(1);
	}

	old = find_match(me->name, DURING_LOAD, NULL);
	if (old) {
		if (old->revision == me->revision) {
			fprintf(stderr,
				"%s: match `%s' already registered.\n",
				program_name, me->name);
			exit(1);
		}

		/* Now we have two (or more) options, check compatibility. */
		if (compatible_match_revision(old->name, old->revision)
		    && old->revision > me->revision)
			return;

		/* Replace if compatible. */
		if (!compatible_match_revision(me->name, me->revision))
			return;

		/* Delete old one. */
		for (i = &iptables_matches; *i!=old; i = &(*i)->next);
		*i = old->next;
	}

	if (me->size != IPT_ALIGN(me->size)) {
		fprintf(stderr, "%s: match `%s' has invalid size %u.\n",
			program_name, me->name, (unsigned int)me->size);
		exit(1);
	}

	/* Append to list. */
	for (i = &iptables_matches; *i; i = &(*i)->next);
	me->next = NULL;
	*i = me;

	me->m = NULL;
	me->mflags = 0;
}

void
register_target(struct iptables_target *me)
{
	struct iptables_target *old;

	if (strcmp(me->version, program_version) != 0) {
		fprintf(stderr, "%s: target `%s' v%s (I'm v%s).\n",
			program_name, me->name, me->version, program_version);
		exit(1);
	}

	/* Revision field stole a char from name. */
	if (strlen(me->name) >= IPT_FUNCTION_MAXNAMELEN-1) {
		fprintf(stderr, "%s: target `%s' has invalid name\n",
			program_name, me->name);
		exit(1);
	}

	old = find_target(me->name, DURING_LOAD);
	if (old) {
		struct iptables_target **i;

		if (old->revision == me->revision) {
			fprintf(stderr,
				"%s: target `%s' already registered.\n",
				program_name, me->name);
			exit(1);
		}

		/* Now we have two (or more) options, check compatibility. */
		if (compatible_target_revision(old->name, old->revision)
		    && old->revision > me->revision)
			return;

		/* Replace if compatible. */
		if (!compatible_target_revision(me->name, me->revision))
			return;

		/* Delete old one. */
		for (i = &iptables_targets; *i!=old; i = &(*i)->next);
		*i = old->next;
	}

	if (me->size != IPT_ALIGN(me->size)) {
		fprintf(stderr, "%s: target `%s' has invalid size %u.\n",
			program_name, me->name, (unsigned int)me->size);
		exit(1);
	}

	/* Prepend to list. */
	me->next = iptables_targets;
	iptables_targets = me;
	me->t = NULL;
	me->tflags = 0;
}

static void
print_num(u_int64_t number, unsigned int format)
{
	if (format & FMT_KILOMEGAGIGA) {
		if (number > 99999) {
			number = (number + 500) / 1000;
			if (number > 9999) {
				number = (number + 500) / 1000;
				if (number > 9999) {
					number = (number + 500) / 1000;
					if (number > 9999) {
						number = (number + 500) / 1000;
						printf(FMT("%4lluT ","%lluT "), (unsigned long long)number);
					}
					else printf(FMT("%4lluG ","%lluG "), (unsigned long long)number);
				}
				else printf(FMT("%4lluM ","%lluM "), (unsigned long long)number);
			} else
				printf(FMT("%4lluK ","%lluK "), (unsigned long long)number);
		} else
			printf(FMT("%5llu ","%llu "), (unsigned long long)number);
	} else
		printf(FMT("%8llu ","%llu "), (unsigned long long)number);
}


static void
print_header(unsigned int format, const char *chain, iptc_handle_t *handle)
{
	struct ipt_counters counters;
	const char *pol = iptc_get_policy(chain, &counters, handle);
	printf("Chain %s", chain);
	if (pol) {
		printf(" (policy %s", pol);
		if (!(format & FMT_NOCOUNTS)) {
			fputc(' ', stdout);
			print_num(counters.pcnt, (format|FMT_NOTABLE));
			fputs("packets, ", stdout);
			print_num(counters.bcnt, (format|FMT_NOTABLE));
			fputs("bytes", stdout);
		}
		printf(")\n");
	} else {
		unsigned int refs;
		if (!iptc_get_references(&refs, chain, handle))
			printf(" (ERROR obtaining refs)\n");
		else
			printf(" (%u references)\n", refs);
	}

	if (format & FMT_LINENUMBERS)
		printf(FMT("%-4s ", "%s "), "num");
	if (!(format & FMT_NOCOUNTS)) {
		if (format & FMT_KILOMEGAGIGA) {
			printf(FMT("%5s ","%s "), "pkts");
			printf(FMT("%5s ","%s "), "bytes");
		} else {
			printf(FMT("%8s ","%s "), "pkts");
			printf(FMT("%10s ","%s "), "bytes");
		}
	}
	if (!(format & FMT_NOTARGET))
		printf(FMT("%-9s ","%s "), "target");
	fputs(" prot ", stdout);
	if (format & FMT_OPTIONS)
		fputs("opt", stdout);
	if (format & FMT_VIA) {
		printf(FMT(" %-6s ","%s "), "in");
		printf(FMT("%-6s ","%s "), "out");
	}
	printf(FMT(" %-19s ","%s "), "source");
	printf(FMT(" %-19s "," %s "), "destination");
	printf("\n");
}


static int
print_match(const struct ipt_entry_match *m,
	    const struct ipt_ip *ip,
	    int numeric)
{
	struct iptables_match *match = find_match(m->u.user.name, TRY_LOAD, NULL);

	if (match) {
		if (match->print)
			match->print(ip, m, numeric);
		else
			printf("%s ", match->name);
	} else {
		if (m->u.user.name[0])
			printf("UNKNOWN match `%s' ", m->u.user.name);
	}
	/* Don't stop iterating. */
	return 0;
}

/* e is called `fw' here for hysterical raisins */
static void
print_firewall(const struct ipt_entry *fw,
	       const char *targname,
	       unsigned int num,
	       unsigned int format,
	       const iptc_handle_t handle)
{
	struct iptables_target *target = NULL;
	const struct ipt_entry_target *t;
	u_int8_t flags;
	char buf[BUFSIZ];

	if (!iptc_is_chain(targname, handle))
		target = find_target(targname, TRY_LOAD);
	else
		target = find_target(IPT_STANDARD_TARGET, LOAD_MUST_SUCCEED);

	t = ipt_get_target((struct ipt_entry *)fw);
	flags = fw->ip.flags;

	if (format & FMT_LINENUMBERS)
		printf(FMT("%-4u ", "%u "), num+1);

	if (!(format & FMT_NOCOUNTS)) {
		print_num(fw->counters.pcnt, format);
		print_num(fw->counters.bcnt, format);
	}

	if (!(format & FMT_NOTARGET))
		printf(FMT("%-9s ", "%s "), targname);

	fputc(fw->ip.invflags & IPT_INV_PROTO ? '!' : ' ', stdout);
	{
		char *pname = proto_to_name(fw->ip.proto, format&FMT_NUMERIC);
		if (pname)
			printf(FMT("%-5s", "%s "), pname);
		else
			printf(FMT("%-5hu", "%hu "), fw->ip.proto);
	}

	if (format & FMT_OPTIONS) {
		if (format & FMT_NOTABLE)
			fputs("opt ", stdout);
		fputc(fw->ip.invflags & IPT_INV_FRAG ? '!' : '-', stdout);
		fputc(flags & IPT_F_FRAG ? 'f' : '-', stdout);
		fputc(' ', stdout);
	}

	if (format & FMT_VIA) {
		char iface[IFNAMSIZ+2];

		if (fw->ip.invflags & IPT_INV_VIA_IN) {
			iface[0] = '!';
			iface[1] = '\0';
		}
		else iface[0] = '\0';

		if (fw->ip.iniface[0] != '\0') {
			strcat(iface, fw->ip.iniface);
		}
		else if (format & FMT_NUMERIC) strcat(iface, "*");
		else strcat(iface, "any");
		printf(FMT(" %-6s ","in %s "), iface);

		if (fw->ip.invflags & IPT_INV_VIA_OUT) {
			iface[0] = '!';
			iface[1] = '\0';
		}
		else iface[0] = '\0';

		if (fw->ip.outiface[0] != '\0') {
			strcat(iface, fw->ip.outiface);
		}
		else if (format & FMT_NUMERIC) strcat(iface, "*");
		else strcat(iface, "any");
		printf(FMT("%-6s ","out %s "), iface);
	}

	fputc(fw->ip.invflags & IPT_INV_SRCIP ? '!' : ' ', stdout);
	if (fw->ip.smsk.s_addr == 0L && !(format & FMT_NUMERIC))
		printf(FMT("%-19s ","%s "), "anywhere");
	else {
		if (format & FMT_NUMERIC)
			sprintf(buf, "%s", addr_to_dotted(&(fw->ip.src)));
		else
			sprintf(buf, "%s", addr_to_anyname(&(fw->ip.src)));
		strcat(buf, mask_to_dotted(&(fw->ip.smsk)));
		printf(FMT("%-19s ","%s "), buf);
	}

	fputc(fw->ip.invflags & IPT_INV_DSTIP ? '!' : ' ', stdout);
	if (fw->ip.dmsk.s_addr == 0L && !(format & FMT_NUMERIC))
		printf(FMT("%-19s ","-> %s"), "anywhere");
	else {
		if (format & FMT_NUMERIC)
			sprintf(buf, "%s", addr_to_dotted(&(fw->ip.dst)));
		else
			sprintf(buf, "%s", addr_to_anyname(&(fw->ip.dst)));
		strcat(buf, mask_to_dotted(&(fw->ip.dmsk)));
		printf(FMT("%-19s ","-> %s"), buf);
	}

	if (format & FMT_NOTABLE)
		fputs("  ", stdout);

#ifdef IPT_F_GOTO
	if(fw->ip.flags & IPT_F_GOTO)
		printf("[goto] ");
#endif

	IPT_MATCH_ITERATE(fw, print_match, &fw->ip, format & FMT_NUMERIC);

	if (target) {
		if (target->print)
			/* Print the target information. */
			target->print(&fw->ip, t, format & FMT_NUMERIC);
	} else if (t->u.target_size != sizeof(*t))
		printf("[%u bytes of unknown target data] ",
		       (unsigned int)(t->u.target_size - sizeof(*t)));

	if (!(format & FMT_NONEWLINE))
		fputc('\n', stdout);
}

static void
print_firewall_line(const struct ipt_entry *fw,
		    const iptc_handle_t h)
{
	struct ipt_entry_target *t;

	t = ipt_get_target((struct ipt_entry *)fw);
	print_firewall(fw, t->u.user.name, 0, FMT_PRINT_RULE, h);
}

static int
append_entry(const ipt_chainlabel chain,
	     struct ipt_entry *fw,
	     unsigned int nsaddrs,
	     const struct in_addr saddrs[],
	     unsigned int ndaddrs,
	     const struct in_addr daddrs[],
	     int verbose,
	     iptc_handle_t *handle)
{
	unsigned int i, j;
	int ret = 1;

	for (i = 0; i < nsaddrs; i++) {
		fw->ip.src.s_addr = saddrs[i].s_addr;
		for (j = 0; j < ndaddrs; j++) {
			fw->ip.dst.s_addr = daddrs[j].s_addr;
			if (verbose)
				print_firewall_line(fw, *handle);
			ret &= iptc_append_entry(chain, fw, handle);
		}
	}

	return ret;
}

static int
replace_entry(const ipt_chainlabel chain,
	      struct ipt_entry *fw,
	      unsigned int rulenum,
	      const struct in_addr *saddr,
	      const struct in_addr *daddr,
	      int verbose,
	      iptc_handle_t *handle)
{
	fw->ip.src.s_addr = saddr->s_addr;
	fw->ip.dst.s_addr = daddr->s_addr;

	if (verbose)
		print_firewall_line(fw, *handle);
	return iptc_replace_entry(chain, fw, rulenum, handle);
}

static int
insert_entry(const ipt_chainlabel chain,
	     struct ipt_entry *fw,
	     unsigned int rulenum,
	     unsigned int nsaddrs,
	     const struct in_addr saddrs[],
	     unsigned int ndaddrs,
	     const struct in_addr daddrs[],
	     int verbose,
	     iptc_handle_t *handle)
{
	unsigned int i, j;
	int ret = 1;

	for (i = 0; i < nsaddrs; i++) {
		fw->ip.src.s_addr = saddrs[i].s_addr;
		for (j = 0; j < ndaddrs; j++) {
			fw->ip.dst.s_addr = daddrs[j].s_addr;
			if (verbose)
				print_firewall_line(fw, *handle);
			ret &= iptc_insert_entry(chain, fw, rulenum, handle);
		}
	}

	return ret;
}

static unsigned char *
make_delete_mask(struct ipt_entry *fw, struct iptables_rule_match *matches)
{
	/* Establish mask for comparison */
	unsigned int size;
	struct iptables_rule_match *matchp;
	unsigned char *mask, *mptr;

	size = sizeof(struct ipt_entry);
	for (matchp = matches; matchp; matchp = matchp->next)
		size += IPT_ALIGN(sizeof(struct ipt_entry_match)) + matchp->match->size;

	mask = fw_calloc(1, size
			 + IPT_ALIGN(sizeof(struct ipt_entry_target))
			 + iptables_targets->size);

	memset(mask, 0xFF, sizeof(struct ipt_entry));
	mptr = mask + sizeof(struct ipt_entry);

	for (matchp = matches; matchp; matchp = matchp->next) {
		memset(mptr, 0xFF,
		       IPT_ALIGN(sizeof(struct ipt_entry_match))
		       + matchp->match->userspacesize);
		mptr += IPT_ALIGN(sizeof(struct ipt_entry_match)) + matchp->match->size;
	}

	memset(mptr, 0xFF,
	       IPT_ALIGN(sizeof(struct ipt_entry_target))
	       + iptables_targets->userspacesize);

	return mask;
}

static int
delete_entry(const ipt_chainlabel chain,
	     struct ipt_entry *fw,
	     unsigned int nsaddrs,
	     const struct in_addr saddrs[],
	     unsigned int ndaddrs,
	     const struct in_addr daddrs[],
	     int verbose,
	     iptc_handle_t *handle,
	     struct iptables_rule_match *matches)
{
	unsigned int i, j;
	int ret = 1;
	unsigned char *mask;

	mask = make_delete_mask(fw, matches);
	for (i = 0; i < nsaddrs; i++) {
		fw->ip.src.s_addr = saddrs[i].s_addr;
		for (j = 0; j < ndaddrs; j++) {
			fw->ip.dst.s_addr = daddrs[j].s_addr;
			if (verbose)
				print_firewall_line(fw, *handle);
			ret &= iptc_delete_entry(chain, fw, mask, handle);
		}
	}
	free(mask);

	return ret;
}

int
for_each_chain(int (*fn)(const ipt_chainlabel, int, iptc_handle_t *),
	       int verbose, int builtinstoo, iptc_handle_t *handle)
{
        int ret = 1;
	const char *chain;
	char *chains;
	unsigned int i, chaincount = 0;

	chain = iptc_first_chain(handle);
	while (chain) {
		chaincount++;
		chain = iptc_next_chain(handle);
        }

	chains = fw_malloc(sizeof(ipt_chainlabel) * chaincount);
	i = 0;
	chain = iptc_first_chain(handle);
	while (chain) {
		strcpy(chains + i*sizeof(ipt_chainlabel), chain);
		i++;
		chain = iptc_next_chain(handle);
        }

	for (i = 0; i < chaincount; i++) {
		if (!builtinstoo
		    && iptc_builtin(chains + i*sizeof(ipt_chainlabel),
				    *handle) == 1)
			continue;
	        ret &= fn(chains + i*sizeof(ipt_chainlabel), verbose, handle);
	}

	free(chains);
        return ret;
}

int
flush_entries(const ipt_chainlabel chain, int verbose,
	      iptc_handle_t *handle)
{
	if (!chain)
		return for_each_chain(flush_entries, verbose, 1, handle);

	if (verbose)
		fprintf(stdout, "Flushing chain `%s'\n", chain);
	return iptc_flush_entries(chain, handle);
}

static int
zero_entries(const ipt_chainlabel chain, int verbose,
	     iptc_handle_t *handle)
{
	if (!chain)
		return for_each_chain(zero_entries, verbose, 1, handle);

	if (verbose)
		fprintf(stdout, "Zeroing chain `%s'\n", chain);
	return iptc_zero_entries(chain, handle);
}

int
delete_chain(const ipt_chainlabel chain, int verbose,
	     iptc_handle_t *handle)
{
	if (!chain)
		return for_each_chain(delete_chain, verbose, 0, handle);

	if (verbose)
	        fprintf(stdout, "Deleting chain `%s'\n", chain);
	return iptc_delete_chain(chain, handle);
}

static int
list_entries(const ipt_chainlabel chain, int verbose, int numeric,
	     int expanded, int linenumbers, iptc_handle_t *handle)
{
	int found = 0;
	unsigned int format;
	const char *this;

	format = FMT_OPTIONS;
	if (!verbose)
		format |= FMT_NOCOUNTS;
	else
		format |= FMT_VIA;

	if (numeric)
		format |= FMT_NUMERIC;

	if (!expanded)
		format |= FMT_KILOMEGAGIGA;

	if (linenumbers)
		format |= FMT_LINENUMBERS;

	for (this = iptc_first_chain(handle);
	     this;
	     this = iptc_next_chain(handle)) {
		const struct ipt_entry *i;
		unsigned int num;

		if (chain && strcmp(chain, this) != 0)
			continue;

		if (found) printf("\n");

		print_header(format, this, handle);
		i = iptc_first_rule(this, handle);

		num = 0;
		while (i) {
			print_firewall(i,
				       iptc_get_target(i, handle),
				       num++,
				       format,
				       *handle);
			i = iptc_next_rule(i, handle);
		}
		found = 1;
	}

	errno = ENOENT;
	return found;
}

static char *get_modprobe(void)
{
	return strdup("/sbin/modprobe");
#if 0
	int procfile;
	char *ret;

#define PROCFILE_BUFSIZ	1024
	procfile = open(PROC_SYS_MODPROBE, O_RDONLY);
	if (procfile < 0)
		return NULL;

	ret = (char *) malloc(PROCFILE_BUFSIZ);
	if (ret) {
		memset(ret, 0, PROCFILE_BUFSIZ);
		switch (read(procfile, ret, PROCFILE_BUFSIZ)) {
		case -1: goto fail;
		case PROCFILE_BUFSIZ: goto fail; /* Partial read.  Wierd */
		}
		if (ret[strlen(ret)-1]=='\n') 
			ret[strlen(ret)-1]=0;
		close(procfile);
		return ret;
	}
 fail:
	free(ret);
	close(procfile);
	return NULL;
#endif
}

int iptables_insmod(const char *modname, const char *modprobe, int quiet)
{
	char *buf = NULL;
	char *argv[4];
	int status;

	/* If they don't explicitly set it, read out of kernel */
	if (!modprobe) {
		buf = get_modprobe();
		if (!buf)
			return -1;
		modprobe = buf;
	}

	switch (fork()) {
	case 0:
		argv[0] = (char *)modprobe;
		argv[1] = (char *)modname;
		if (quiet) {
			argv[2] = "-q";
			argv[3] = NULL;
		} else {
			argv[2] = NULL;
			argv[3] = NULL;
		}
		execv(argv[0], argv);

		/* not usually reached */
		exit(1);
	case -1:
		return -1;

	default: /* parent */
		wait(&status);
	}

	free(buf);
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return 0;
	return -1;
}

int load_iptables_ko(const char *modprobe, int quiet)
{
	// not needed, modprobe will be just noisy -- zzz
	return 0;
/*
	static int loaded = 0;
	static int ret = -1;

	if (!loaded) {
		ret = iptables_insmod("ip_tables", modprobe, quiet);
		loaded = (ret == 0);
	}

	return ret;
*/
}

static struct ipt_entry *
generate_entry(const struct ipt_entry *fw,
	       struct iptables_rule_match *matches,
	       struct ipt_entry_target *target)
{
	unsigned int size;
	struct iptables_rule_match *matchp;
	struct ipt_entry *e;

	size = sizeof(struct ipt_entry);
	for (matchp = matches; matchp; matchp = matchp->next)
		size += matchp->match->m->u.match_size;

	e = fw_malloc(size + target->u.target_size);
	*e = *fw;
	e->target_offset = size;
	e->next_offset = size + target->u.target_size;

	size = 0;
	for (matchp = matches; matchp; matchp = matchp->next) {
		memcpy(e->elems + size, matchp->match->m, matchp->match->m->u.match_size);
		size += matchp->match->m->u.match_size;
	}
	memcpy(e->elems + size, target, target->u.target_size);

	return e;
}

void clear_rule_matches(struct iptables_rule_match **matches)
{
	struct iptables_rule_match *matchp, *tmp;

	for (matchp = *matches; matchp;) {
		tmp = matchp->next;
		if (matchp->match->m) {
			free(matchp->match->m);
			matchp->match->m = NULL;
		}
		if (matchp->match == matchp->match->next) {
			free(matchp->match);
			matchp->match = NULL;
		}
		free(matchp);
		matchp = tmp;
	}

	*matches = NULL;
}

static void set_revision(char *name, u_int8_t revision)
{
	/* Old kernel sources don't have ".revision" field,
	   but we stole a byte from name. */
	name[IPT_FUNCTION_MAXNAMELEN - 2] = '\0';
	name[IPT_FUNCTION_MAXNAMELEN - 1] = revision;
}

void
get_kernel_version(void) {
	static struct utsname uts;
	int x = 0, y = 0, z = 0;

	if (uname(&uts) == -1) {
		fprintf(stderr, "Unable to retrieve kernel version.\n");
		free_opts(1);
		exit(1); 
	}

	sscanf(uts.release, "%d.%d.%d", &x, &y, &z);
	kernel_version = LINUX_VERSION(x, y, z);
}

int do_command(int argc, char *argv[], char **table, iptc_handle_t *handle)
{
	struct ipt_entry fw, *e = NULL;
	int invert = 0;
	unsigned int nsaddrs = 0, ndaddrs = 0;
	struct in_addr *saddrs = NULL, *daddrs = NULL;

	int c, verbose = 0;
	const char *chain = NULL;
	const char *shostnetworkmask = NULL, *dhostnetworkmask = NULL;
	const char *policy = NULL, *newname = NULL;
	unsigned int rulenum = 0, options = 0, command = 0;
	const char *pcnt = NULL, *bcnt = NULL;
	int ret = 1;
	struct iptables_match *m;
	struct iptables_rule_match *matches = NULL;
	struct iptables_rule_match *matchp;
	struct iptables_target *target = NULL;
	struct iptables_target *t;
	const char *jumpto = "";
	char *protocol = NULL;
	int proto_used = 0;

	memset(&fw, 0, sizeof(fw));

	/* re-set optind to 0 in case do_command gets called
	 * a second time */
	optind = 0;

	/* clear mflags in case do_command gets called a second time
	 * (we clear the global list of all matches for security)*/
	for (m = iptables_matches; m; m = m->next)
		m->mflags = 0;

	for (t = iptables_targets; t; t = t->next) {
		t->tflags = 0;
		t->used = 0;
	}

	/* Suppress error messages: we may add new options if we
           demand-load a protocol. */
	opterr = 0;

	while ((c = getopt_long(argc, argv,
	   "-A:D:R:I:L::M:F::Z::N:X::E:P:Vh::o:p:s:d:j:i:fbvnt:m:xc:g:",
					   opts, NULL)) != -1) {
		switch (c) {
			/*
			 * Command selection
			 */
		case 'A':
			add_command(&command, CMD_APPEND, CMD_NONE,
				    invert);
			chain = optarg;
			break;

		case 'D':
			add_command(&command, CMD_DELETE, CMD_NONE,
				    invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!') {
				rulenum = parse_rulenumber(argv[optind++]);
				command = CMD_DELETE_NUM;
			}
			break;

		case 'R':
			add_command(&command, CMD_REPLACE, CMD_NONE,
				    invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				rulenum = parse_rulenumber(argv[optind++]);
			else
				exit_error(PARAMETER_PROBLEM,
					   "-%c requires a rule number",
					   cmd2char(CMD_REPLACE));
			break;

		case 'I':
			add_command(&command, CMD_INSERT, CMD_NONE,
				    invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				rulenum = parse_rulenumber(argv[optind++]);
			else rulenum = 1;
			break;

		case 'L':
			add_command(&command, CMD_LIST, CMD_ZERO,
				    invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				chain = argv[optind++];
			break;

		case 'F':
			add_command(&command, CMD_FLUSH, CMD_NONE,
				    invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				chain = argv[optind++];
			break;

		case 'Z':
			add_command(&command, CMD_ZERO, CMD_LIST,
				    invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				&& argv[optind][0] != '!')
				chain = argv[optind++];
			break;

		case 'N':
			if (optarg && (*optarg == '-' || *optarg == '!'))
				exit_error(PARAMETER_PROBLEM,
					   "chain name not allowed to start "
					   "with `%c'\n", *optarg);
			if (find_target(optarg, TRY_LOAD))
				exit_error(PARAMETER_PROBLEM,
					   "chain name may not clash "
					   "with target name\n");
			add_command(&command, CMD_NEW_CHAIN, CMD_NONE,
				    invert);
			chain = optarg;
			break;

		case 'X':
			add_command(&command, CMD_DELETE_CHAIN, CMD_NONE,
				    invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				chain = argv[optind++];
			break;

		case 'E':
			add_command(&command, CMD_RENAME_CHAIN, CMD_NONE,
				    invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				newname = argv[optind++];
			else
				exit_error(PARAMETER_PROBLEM,
				           "-%c requires old-chain-name and "
					   "new-chain-name",
					    cmd2char(CMD_RENAME_CHAIN));
			break;

		case 'P':
			add_command(&command, CMD_SET_POLICY, CMD_NONE,
				    invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				policy = argv[optind++];
			else
				exit_error(PARAMETER_PROBLEM,
					   "-%c requires a chain and a policy",
					   cmd2char(CMD_SET_POLICY));
			break;

		case 'h':
			if (!optarg)
				optarg = argv[optind];

			/* iptables -p icmp -h */
			if (!matches && protocol)
				find_match(protocol, TRY_LOAD, &matches);

			exit_printhelp(matches);

			/*
			 * Option selection
			 */
		case 'p':
			check_inverse(optarg, &invert, &optind, argc);
			set_option(&options, OPT_PROTOCOL, &fw.ip.invflags,
				   invert);

			/* Canonicalize into lower case */
			for (protocol = argv[optind-1]; *protocol; protocol++)
				*protocol = tolower(*protocol);

			protocol = argv[optind-1];
			fw.ip.proto = parse_protocol(protocol);

			if (fw.ip.proto == 0
			    && (fw.ip.invflags & IPT_INV_PROTO))
				exit_error(PARAMETER_PROBLEM,
					   "rule would never match protocol");
			break;

		case 's':
			check_inverse(optarg, &invert, &optind, argc);
			set_option(&options, OPT_SOURCE, &fw.ip.invflags,
				   invert);
			shostnetworkmask = argv[optind-1];
			break;

		case 'd':
			check_inverse(optarg, &invert, &optind, argc);
			set_option(&options, OPT_DESTINATION, &fw.ip.invflags,
				   invert);
			dhostnetworkmask = argv[optind-1];
			break;

#ifdef IPT_F_GOTO
		case 'g':
			set_option(&options, OPT_JUMP, &fw.ip.invflags,
				   invert);
			fw.ip.flags |= IPT_F_GOTO;
			jumpto = parse_target(optarg);
			break;
#endif

		case 'j':
			set_option(&options, OPT_JUMP, &fw.ip.invflags,
				   invert);
			jumpto = parse_target(optarg);
			/* TRY_LOAD (may be chain name) */
			target = find_target(jumpto, TRY_LOAD);

			if (target) {
				size_t size;

				size = IPT_ALIGN(sizeof(struct ipt_entry_target))
					+ target->size;

				target->t = fw_calloc(1, size);
				target->t->u.target_size = size;
				strcpy(target->t->u.user.name, jumpto);
				set_revision(target->t->u.user.name,
					     target->revision);
				if (target->init != NULL)
					target->init(target->t, &fw.nfcache);
				opts = merge_options(opts, target->extra_opts, &target->option_offset);
			}
			break;


		case 'i':
			check_inverse(optarg, &invert, &optind, argc);
			set_option(&options, OPT_VIANAMEIN, &fw.ip.invflags,
				   invert);
			parse_interface(argv[optind-1],
					fw.ip.iniface,
					fw.ip.iniface_mask);
			break;

		case 'o':
			check_inverse(optarg, &invert, &optind, argc);
			set_option(&options, OPT_VIANAMEOUT, &fw.ip.invflags,
				   invert);
			parse_interface(argv[optind-1],
					fw.ip.outiface,
					fw.ip.outiface_mask);
			break;

		case 'f':
			set_option(&options, OPT_FRAGMENT, &fw.ip.invflags,
				   invert);
			fw.ip.flags |= IPT_F_FRAG;
			break;

		case 'v':
			if (!verbose)
				set_option(&options, OPT_VERBOSE,
					   &fw.ip.invflags, invert);
			verbose++;
			break;

		case 'm': {
			size_t size;

			if (invert)
				exit_error(PARAMETER_PROBLEM,
					   "unexpected ! flag before --match");

			m = find_match(optarg, LOAD_MUST_SUCCEED, &matches);
			size = IPT_ALIGN(sizeof(struct ipt_entry_match))
					 + m->size;
			m->m = fw_calloc(1, size);
			m->m->u.match_size = size;
			strcpy(m->m->u.user.name, m->name);
			set_revision(m->m->u.user.name, m->revision);
			if (m->init != NULL)
				m->init(m->m, &fw.nfcache);
			if (m != m->next)
				/* Merge options for non-cloned matches */
				opts = merge_options(opts, m->extra_opts, &m->option_offset);
		}
		break;

		case 'n':
			set_option(&options, OPT_NUMERIC, &fw.ip.invflags,
				   invert);
			break;

		case 't':
			if (invert)
				exit_error(PARAMETER_PROBLEM,
					   "unexpected ! flag before --table");
			*table = argv[optind-1];
			break;

		case 'x':
			set_option(&options, OPT_EXPANDED, &fw.ip.invflags,
				   invert);
			break;

		case 'V':
			if (invert)
				printf("Not %s ;-)\n", program_version);
			else
				printf("%s v%s\n",
				       program_name, program_version);
			exit(0);

		case '0':
			set_option(&options, OPT_LINENUMBERS, &fw.ip.invflags,
				   invert);
			break;

		case 'M':
			modprobe = optarg;
			break;

		case 'c':

			set_option(&options, OPT_COUNTERS, &fw.ip.invflags,
				   invert);
			pcnt = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				bcnt = argv[optind++];
			else
				exit_error(PARAMETER_PROBLEM,
					"-%c requires packet and byte counter",
					opt2char(OPT_COUNTERS));

			if (sscanf(pcnt, "%llu", (unsigned long long *)&fw.counters.pcnt) != 1)
				exit_error(PARAMETER_PROBLEM,
					"-%c packet counter not numeric",
					opt2char(OPT_COUNTERS));

			if (sscanf(bcnt, "%llu", (unsigned long long *)&fw.counters.bcnt) != 1)
				exit_error(PARAMETER_PROBLEM,
					"-%c byte counter not numeric",
					opt2char(OPT_COUNTERS));
			
			break;


		case 1: /* non option */
			if (optarg[0] == '!' && optarg[1] == '\0') {
				if (invert)
					exit_error(PARAMETER_PROBLEM,
						   "multiple consecutive ! not"
						   " allowed");
				invert = TRUE;
				optarg[0] = '\0';
				continue;
			}
			printf("Bad argument `%s'\n", optarg);
			exit_tryhelp(2);

		default:
			if (!target
			    || !(target->parse(c - target->option_offset,
					       argv, invert,
					       &target->tflags,
					       &fw, &target->t))) {
				for (matchp = matches; matchp; matchp = matchp->next) {
					if (matchp->completed) 
						continue;
					if (matchp->match->parse(c - matchp->match->option_offset,
						     argv, invert,
						     &matchp->match->mflags,
						     &fw,
						     &fw.nfcache,
						     &matchp->match->m))
						break;
				}
				m = matchp ? matchp->match : NULL;

				/* If you listen carefully, you can
				   actually hear this code suck. */

				/* some explanations (after four different bugs
				 * in 3 different releases): If we encounter a
				 * parameter, that has not been parsed yet,
				 * it's not an option of an explicitly loaded
				 * match or a target.  However, we support
				 * implicit loading of the protocol match
				 * extension.  '-p tcp' means 'l4 proto 6' and
				 * at the same time 'load tcp protocol match on
				 * demand if we specify --dport'.
				 *
				 * To make this work, we need to make sure:
				 * - the parameter has not been parsed by
				 *   a match (m above)
				 * - a protocol has been specified
				 * - the protocol extension has not been
				 *   loaded yet, or is loaded and unused
				 *   [think of iptables-restore!]
				 * - the protocol extension can be successively
				 *   loaded
				 */
				if (m == NULL
				    && protocol
				    && (!find_proto(protocol, DONT_LOAD,
						   options&OPT_NUMERIC, NULL) 
					|| (find_proto(protocol, DONT_LOAD,
							options&OPT_NUMERIC, NULL)
					    && (proto_used == 0))
				       )
				    && (m = find_proto(protocol, TRY_LOAD,
						       options&OPT_NUMERIC, &matches))) {
					/* Try loading protocol */
					size_t size;
					
					proto_used = 1;

					size = IPT_ALIGN(sizeof(struct ipt_entry_match))
							 + m->size;

					m->m = fw_calloc(1, size);
					m->m->u.match_size = size;
					strcpy(m->m->u.user.name, m->name);
					set_revision(m->m->u.user.name,
						     m->revision);
					if (m->init != NULL)
						m->init(m->m, &fw.nfcache);

					opts = merge_options(opts,
					    m->extra_opts, &m->option_offset);

					optind--;
					continue;
				}
				if (!m)
					exit_error(PARAMETER_PROBLEM,
						   "Unknown arg `%s'",
						   argv[optind-1]);
			}
		}
		invert = FALSE;
	}

	for (matchp = matches; matchp; matchp = matchp->next)
		matchp->match->final_check(matchp->match->mflags);

	if (target)
		target->final_check(target->tflags);

	/* Fix me: must put inverse options checking here --MN */

	if (optind < argc)
		exit_error(PARAMETER_PROBLEM,
			   "unknown arguments found on commandline");
	if (!command)
		exit_error(PARAMETER_PROBLEM, "no command specified");
	if (invert)
		exit_error(PARAMETER_PROBLEM,
			   "nothing appropriate following !");

	if (command & (CMD_REPLACE | CMD_INSERT | CMD_DELETE | CMD_APPEND)) {
		if (!(options & OPT_DESTINATION))
			dhostnetworkmask = "0.0.0.0/0";
		if (!(options & OPT_SOURCE))
			shostnetworkmask = "0.0.0.0/0";
	}

	if (shostnetworkmask)
		parse_hostnetworkmask(shostnetworkmask, &saddrs,
				      &(fw.ip.smsk), &nsaddrs);

	if (dhostnetworkmask)
		parse_hostnetworkmask(dhostnetworkmask, &daddrs,
				      &(fw.ip.dmsk), &ndaddrs);

	if ((nsaddrs > 1 || ndaddrs > 1) &&
	    (fw.ip.invflags & (IPT_INV_SRCIP | IPT_INV_DSTIP)))
		exit_error(PARAMETER_PROBLEM, "! not allowed with multiple"
			   " source or destination IP addresses");

	if (command == CMD_REPLACE && (nsaddrs != 1 || ndaddrs != 1))
		exit_error(PARAMETER_PROBLEM, "Replacement rule does not "
			   "specify a unique address");

	generic_opt_check(command, options);

	if (chain && strlen(chain) > IPT_FUNCTION_MAXNAMELEN)
		exit_error(PARAMETER_PROBLEM,
			   "chain name `%s' too long (must be under %i chars)",
			   chain, IPT_FUNCTION_MAXNAMELEN);

	/* only allocate handle if we weren't called with a handle */
	if (!*handle)
		*handle = iptc_init(*table);

	/* try to insmod the module if iptc_init failed */
	if (!*handle && load_iptables_ko(modprobe, 0) != -1)
		*handle = iptc_init(*table);

	if (!*handle)
		exit_error(VERSION_PROBLEM,
			   "can't initialize iptables table `%s': %s",
			   *table, iptc_strerror(errno));

	if (command == CMD_APPEND
	    || command == CMD_DELETE
	    || command == CMD_INSERT
	    || command == CMD_REPLACE) {
		if (strcmp(chain, "PREROUTING") == 0
		    || strcmp(chain, "INPUT") == 0) {
			/* -o not valid with incoming packets. */
			if (options & OPT_VIANAMEOUT)
				exit_error(PARAMETER_PROBLEM,
					   "Can't use -%c with %s\n",
					   opt2char(OPT_VIANAMEOUT),
					   chain);
		}

		if (strcmp(chain, "POSTROUTING") == 0
		    || strcmp(chain, "OUTPUT") == 0) {
			/* -i not valid with outgoing packets */
			if (options & OPT_VIANAMEIN)
				exit_error(PARAMETER_PROBLEM,
					   "Can't use -%c with %s\n",
					   opt2char(OPT_VIANAMEIN),
					   chain);
		}

		if (target && iptc_is_chain(jumpto, *handle)) {
			printf("Warning: using chain %s, not extension\n",
			       jumpto);

			if (target->t)
				free(target->t);

			target = NULL;
		}

		/* If they didn't specify a target, or it's a chain
		   name, use standard. */
		if (!target
		    && (strlen(jumpto) == 0
			|| iptc_is_chain(jumpto, *handle))) {
			size_t size;

			target = find_target(IPT_STANDARD_TARGET,
					     LOAD_MUST_SUCCEED);

			size = sizeof(struct ipt_entry_target)
				+ target->size;
			target->t = fw_calloc(1, size);
			target->t->u.target_size = size;
			strcpy(target->t->u.user.name, jumpto);
			if (!iptc_is_chain(jumpto, *handle))
				set_revision(target->t->u.user.name,
					     target->revision);
			if (target->init != NULL)
				target->init(target->t, &fw.nfcache);
		}

		if (!target) {
			/* it is no chain, and we can't load a plugin.
			 * We cannot know if the plugin is corrupt, non
			 * existant OR if the user just misspelled a
			 * chain. */
#ifdef IPT_F_GOTO
			if (fw.ip.flags & IPT_F_GOTO)
				exit_error(PARAMETER_PROBLEM,
					   "goto '%s' is not a chain\n", jumpto);
#endif
			find_target(jumpto, LOAD_MUST_SUCCEED);
		} else {
			e = generate_entry(&fw, matches, target->t);
			free(target->t);
		}
	}

	switch (command) {
	case CMD_APPEND:
		ret = append_entry(chain, e,
				   nsaddrs, saddrs, ndaddrs, daddrs,
				   options&OPT_VERBOSE,
				   handle);
		break;
	case CMD_DELETE:
		ret = delete_entry(chain, e,
				   nsaddrs, saddrs, ndaddrs, daddrs,
				   options&OPT_VERBOSE,
				   handle, matches);
		break;
	case CMD_DELETE_NUM:
		ret = iptc_delete_num_entry(chain, rulenum - 1, handle);
		break;
	case CMD_REPLACE:
		ret = replace_entry(chain, e, rulenum - 1,
				    saddrs, daddrs, options&OPT_VERBOSE,
				    handle);
		break;
	case CMD_INSERT:
		ret = insert_entry(chain, e, rulenum - 1,
				   nsaddrs, saddrs, ndaddrs, daddrs,
				   options&OPT_VERBOSE,
				   handle);
		break;
	case CMD_LIST:
		ret = list_entries(chain,
				   options&OPT_VERBOSE,
				   options&OPT_NUMERIC,
				   options&OPT_EXPANDED,
				   options&OPT_LINENUMBERS,
				   handle);
		break;
	case CMD_FLUSH:
		ret = flush_entries(chain, options&OPT_VERBOSE, handle);
		break;
	case CMD_ZERO:
		ret = zero_entries(chain, options&OPT_VERBOSE, handle);
		break;
	case CMD_LIST|CMD_ZERO:
		ret = list_entries(chain,
				   options&OPT_VERBOSE,
				   options&OPT_NUMERIC,
				   options&OPT_EXPANDED,
				   options&OPT_LINENUMBERS,
				   handle);
		if (ret)
			ret = zero_entries(chain,
					   options&OPT_VERBOSE, handle);
		break;
	case CMD_NEW_CHAIN:
		ret = iptc_create_chain(chain, handle);
		break;
	case CMD_DELETE_CHAIN:
		ret = delete_chain(chain, options&OPT_VERBOSE, handle);
		break;
	case CMD_RENAME_CHAIN:
		ret = iptc_rename_chain(chain, newname,	handle);
		break;
	case CMD_SET_POLICY:
		ret = iptc_set_policy(chain, policy, NULL, handle);
		break;
	default:
		/* We should never reach this... */
		exit_tryhelp(2);
	}

	if (verbose > 1)
		dump_entries(*handle);

	clear_rule_matches(&matches);

	if (e != NULL) {
		free(e);
		e = NULL;
	}

	free(saddrs);
	free(daddrs);
	free_opts(1);

	return ret;
}

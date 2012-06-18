/* Shared library add-on to iptables to add realm matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#if defined(__GLIBC__) && __GLIBC__ == 2
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif
#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_realm.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"realm v%s options:\n"
" --realm [!] value[/mask]\n"
"				Match realm\n"
"\n", IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "realm", 1, 0, '1' },
	{0}
};

struct realmname { 
	int	id;
	char*	name;
	int	len;
	struct realmname* next;
};

/* array of realms from /etc/iproute2/rt_realms */
static struct realmname *realms = NULL;
/* 1 if loading failed */
static int rdberr = 0;


void load_realms()
{
	const char* rfnm = "/etc/iproute2/rt_realms";
	char buf[512];
	FILE *fil;
	char *cur, *nxt;
	int id;
	struct realmname *oldnm = NULL, *newnm = NULL;

	fil = fopen(rfnm, "r");
	if (!fil) {
		rdberr = 1;
		return;
	}

	while (fgets(buf, sizeof(buf), fil)) {
		cur = buf;
		while ((*cur == ' ') || (*cur == '\t'))
			cur++;
		if ((*cur == '#') || (*cur == '\n') || (*cur == 0))
			continue;

		/* iproute2 allows hex and dec format */
		errno = 0;
		id = strtoul(cur, &nxt, strncmp(cur, "0x", 2) ? 10 : 16);
		if ((nxt == cur) || errno)
			continue;

		/* same boundaries as in iproute2 */
		if (id < 0 || id > 255)
			continue;
		cur = nxt;

		if (!isspace(*cur))
			continue;
		while ((*cur == ' ') || (*cur == '\t'))
			cur++;
		if ((*cur == '#') || (*cur == '\n') || (*cur == 0))
			continue;
		nxt = cur;
		while ((*nxt != 0) && !isspace(*nxt))
			nxt++;
		if (nxt == cur)
			continue;

		/* found valid data */
		newnm = (struct realmname*)malloc(sizeof(struct realmname));
		if (newnm == NULL) {
			perror("libipt_realm: malloc failed");
			exit(1);
		}
		newnm->id = id;
		newnm->len = nxt - cur;
		newnm->name = (char*)malloc(newnm->len + 1);
		if (newnm->name == NULL) {
			perror("libipt_realm: malloc failed");
			exit(1);
		}
		strncpy(newnm->name, cur, newnm->len);
		newnm->name[newnm->len] = 0;
		newnm->next = NULL;

		if (oldnm)
			oldnm->next = newnm;
		else
			realms = newnm;
		oldnm = newnm;
	}

	fclose(fil);
}

/* get realm id for name, -1 if error/not found */
int realm_name2id(const char* name)
{
	struct realmname* cur;

	if ((realms == NULL) && (rdberr == 0))
		load_realms();
	cur = realms;
	if (cur == NULL)
		return -1;
	while (cur) {
		if (!strncmp(name, cur->name, cur->len + 1))
			return cur->id;
		cur = cur->next;
	}
	return -1;
}

/* get realm name for id, NULL if error/not found */
const char* realm_id2name(int id)
{
	struct realmname* cur;

	if ((realms == NULL) && (rdberr == 0))
		load_realms();
	cur = realms;
	if (cur == NULL)
		return NULL;
	while (cur) {
		if (id == cur->id)
			return cur->name;
		cur = cur->next;
	}
	return NULL;
}


/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_realm_info *realminfo = (struct ipt_realm_info *)(*match)->data;
	int id;

	switch (c) {
		char *end;
	case '1':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		end = optarg = argv[optind-1];
		realminfo->id = strtoul(optarg, &end, 0);
		if (end != optarg && (*end == '/' || *end == '\0')) {
			if (*end == '/')
				realminfo->mask = strtoul(end+1, &end, 0);
			else
				realminfo->mask = 0xffffffff;
			if (*end != '\0' || end == optarg)
				exit_error(PARAMETER_PROBLEM,
					   "Bad realm value `%s'", optarg);
		} else {
			id = realm_name2id(optarg);
			if (id == -1)
				exit_error(PARAMETER_PROBLEM,
					   "Realm `%s' not found", optarg);
			realminfo->id = (u_int32_t)id;
			realminfo->mask = 0xffffffff;
		}
		if (invert)
			realminfo->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}
	return 1;
}

static void
print_realm(unsigned long id, unsigned long mask, int numeric)
{
	const char* name = NULL;

	if (mask != 0xffffffff)
		printf("0x%lx/0x%lx ", id, mask);
	else {
		if (numeric == 0)
			name = realm_id2name(id);
		if (name)
			printf("%s ", name);
		else
			printf("0x%lx ", id);
	}
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_realm_info *ri = (struct ipt_realm_info *) match->data;

	if (ri->invert)
		printf("! ");

	printf("realm ");
	print_realm(ri->id, ri->mask, numeric);
}


/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_realm_info *ri = (struct ipt_realm_info *) match->data;

	if (ri->invert)
		printf("! ");

	printf("--realm ");
	print_realm(ri->id, ri->mask, 0);
}

/* Final check; must have specified --mark. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "realm match: You must specify `--realm'");
}

static struct iptables_match realm = { NULL,
	.name		= "realm",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_realm_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_realm_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&realm);
}



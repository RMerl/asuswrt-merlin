/* Shared library add-on to iptables to add OWNER matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>

#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6t_owner.h>

/* Function which prints out usage message. */
static void
help(void)
{
#ifdef IP6T_OWNER_COMM
	printf(
"OWNER match v%s options:\n"
"[!] --uid-owner userid     Match local uid\n"
"[!] --gid-owner groupid    Match local gid\n"
"[!] --pid-owner processid  Match local pid\n"
"[!] --sid-owner sessionid  Match local sid\n"
"[!] --cmd-owner name       Match local command name\n"
"\n",
IPTABLES_VERSION);
#else
	printf(
"OWNER match v%s options:\n"
"[!] --uid-owner userid     Match local uid\n"
"[!] --gid-owner groupid    Match local gid\n"
"[!] --pid-owner processid  Match local pid\n"
"[!] --sid-owner sessionid  Match local sid\n"
"\n",
IPTABLES_VERSION);
#endif /* IP6T_OWNER_COMM */
}

static struct option opts[] = {
	{ "uid-owner", 1, 0, '1' },
	{ "gid-owner", 1, 0, '2' },
	{ "pid-owner", 1, 0, '3' },
	{ "sid-owner", 1, 0, '4' },
#ifdef IP6T_OWNER_COMM
	{ "cmd-owner", 1, 0, '5' },
#endif
	{0}
};

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_owner_info *ownerinfo = (struct ip6t_owner_info *)(*match)->data;

	switch (c) {
		char *end;
		struct passwd *pwd;
		struct group *grp;
	case '1':
		check_inverse(optarg, &invert, &optind, 0);

		if ((pwd = getpwnam(optarg)))
			ownerinfo->uid = pwd->pw_uid;
		else {
			ownerinfo->uid = strtoul(optarg, &end, 0);
			if (*end != '\0' || end == optarg)
				exit_error(PARAMETER_PROBLEM, "Bad OWNER UID value `%s'", optarg);
		}
		if (invert)
			ownerinfo->invert |= IP6T_OWNER_UID;
		ownerinfo->match |= IP6T_OWNER_UID;
		*flags = 1;
		break;

	case '2':
		check_inverse(optarg, &invert, &optind, 0);
		if ((grp = getgrnam(optarg)))
			ownerinfo->gid = grp->gr_gid;
		else {
			ownerinfo->gid = strtoul(optarg, &end, 0);
			if (*end != '\0' || end == optarg)
				exit_error(PARAMETER_PROBLEM, "Bad OWNER GID value `%s'", optarg);
		}
		if (invert)
			ownerinfo->invert |= IP6T_OWNER_GID;
		ownerinfo->match |= IP6T_OWNER_GID;
		*flags = 1;
		break;

	case '3':
		check_inverse(optarg, &invert, &optind, 0);
		ownerinfo->pid = strtoul(optarg, &end, 0);
		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad OWNER PID value `%s'", optarg);
		if (invert)
			ownerinfo->invert |= IP6T_OWNER_PID;
		ownerinfo->match |= IP6T_OWNER_PID;
		*flags = 1;
		break;

	case '4':
		check_inverse(optarg, &invert, &optind, 0);
		ownerinfo->sid = strtoul(optarg, &end, 0);
		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad OWNER SID value `%s'", optarg);
		if (invert)
			ownerinfo->invert |= IP6T_OWNER_SID;
		ownerinfo->match |= IP6T_OWNER_SID;
		*flags = 1;
		break;

#ifdef IP6T_OWNER_COMM
	case '5':
		check_inverse(optarg, &invert, &optind, 0);
		if(strlen(optarg) > sizeof(ownerinfo->comm))
			exit_error(PARAMETER_PROBLEM, "OWNER CMD `%s' too long, max %d characters", optarg, sizeof(ownerinfo->comm));
		
		strncpy(ownerinfo->comm, optarg, sizeof(ownerinfo->comm));
		ownerinfo->comm[sizeof(ownerinfo->comm)-1] = '\0';

		if (invert)
			ownerinfo->invert |= IP6T_OWNER_COMM;
		ownerinfo->match |= IP6T_OWNER_COMM;
		*flags = 1;
		break;
#endif
		
	default:
		return 0;
	}
	return 1;
}

static void
print_item(struct ip6t_owner_info *info, u_int8_t flag, int numeric, char *label)
{
	if(info->match & flag) {

		if (info->invert & flag)
			printf("! ");

		printf(label);

		switch(info->match & flag) {
		case IP6T_OWNER_UID:
			if(!numeric) {
				struct passwd *pwd = getpwuid(info->uid);

				if(pwd && pwd->pw_name) {
					printf("%s ", pwd->pw_name);
					break;
				}
				/* FALLTHROUGH */
			}
			printf("%u ", info->uid);
			break;
		case IP6T_OWNER_GID:
			if(!numeric) {
				struct group *grp = getgrgid(info->gid);

				if(grp && grp->gr_name) {
					printf("%s ", grp->gr_name);
					break;
				}
				/* FALLTHROUGH */
			}
			printf("%u ", info->gid);
			break;
		case IP6T_OWNER_PID:
			printf("%u ", info->pid);
			break;
		case IP6T_OWNER_SID:
			printf("%u ", info->sid);
			break;
#ifdef IP6T_OWNER_COMM
		case IP6T_OWNER_COMM:
			printf("%.*s ", (int)sizeof(info->comm), info->comm);
			break;
#endif
		default:
			break;
		}
	}
}

/* Final check; must have specified --own. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "OWNER match: You must specify one or more options");
}

/* Prints out the matchinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match,
      int numeric)
{
	struct ip6t_owner_info *info = (struct ip6t_owner_info *)match->data;

	print_item(info, IP6T_OWNER_UID, numeric, "OWNER UID match ");
	print_item(info, IP6T_OWNER_GID, numeric, "OWNER GID match ");
	print_item(info, IP6T_OWNER_PID, numeric, "OWNER PID match ");
	print_item(info, IP6T_OWNER_SID, numeric, "OWNER SID match ");
#ifdef IP6T_OWNER_COMM
	print_item(info, IP6T_OWNER_COMM, numeric, "OWNER CMD match ");
#endif
}

/* Saves the union ip6t_matchinfo in parsable form to stdout. */
static void
save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	struct ip6t_owner_info *info = (struct ip6t_owner_info *)match->data;

	print_item(info, IP6T_OWNER_UID, 0, "--uid-owner ");
	print_item(info, IP6T_OWNER_GID, 0, "--gid-owner ");
	print_item(info, IP6T_OWNER_PID, 0, "--pid-owner ");
	print_item(info, IP6T_OWNER_SID, 0, "--sid-owner ");
#ifdef IP6T_OWNER_COMM
	print_item(info, IP6T_OWNER_COMM, 0, "--cmd-owner ");
#endif
}

static struct ip6tables_match owner = {
	.name 		= "owner",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_owner_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_owner_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts,
};

void _init(void)
{
	register_match6(&owner);
}

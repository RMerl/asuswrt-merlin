/* Shared library add-on to iptables for SCTP matching
 *
 * (C) 2003 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_ecn.c borrowed heavily from libipt_dscp.c
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <ctype.h>

#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#include <linux/netfilter/xt_sctp.h>

/* Some ZS!#@:$%*#$! has replaced the ELEMCOUNT macro in ipt_sctp.h with
 * ARRAY_SIZE without noticing that this file is used from userserspace,
 * and userspace doesn't have ARRAY_SIZE */

#ifndef ELEMCOUNT
#define ELEMCOUNT ARRAY_SIZE
#endif

#if 0
#define DEBUGP(format, first...) printf(format, ##first)
#define static
#else
#define DEBUGP(format, fist...) 
#endif

static void
print_chunk(u_int32_t chunknum, int numeric);

/* Initialize the match. */
static void
init(struct ip6t_entry_match *m, 
     unsigned int *nfcache)
{
	int i;
	struct xt_sctp_info *einfo = (struct xt_sctp_info *)m->data;

	memset(einfo, 0, sizeof(struct xt_sctp_info));

	for (i = 0; i < XT_NUM_SCTP_FLAGS; i++) {
		einfo->flag_info[i].chunktype = -1;
	}
}

static void help(void)
{
	printf(
"SCTP match v%s options\n"
" --source-port [!] port[:port]                          match source port(s)\n"
" --sport ...\n"
" --destination-port [!] port[:port]                     match destination port(s)\n"
" --dport ...\n" 
" --chunk-types [!] (all|any|none) (chunktype[:flags])+	match if all, any or none of\n"
"						        chunktypes are present\n"
"chunktypes - DATA INIT INIT_ACK SACK HEARTBEAT HEARTBEAT_ACK ABORT SHUTDOWN SHUTDOWN_ACK ERROR COOKIE_ECHO COOKIE_ACK ECN_ECNE ECN_CWR SHUTDOWN_COMPLETE ASCONF ASCONF_ACK ALL NONE\n",
	IPTABLES_VERSION);
}

static struct option opts[] = {
	{ .name = "source-port", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "sport", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "destination-port", .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "dport", .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "chunk-types", .has_arg = 1, .flag = 0, .val = '3' },
	{ .name = 0 }
};

static void
parse_sctp_ports(const char *portstring, 
		 u_int16_t *ports)
{
	char *buffer;
	char *cp;

	buffer = strdup(portstring);
	DEBUGP("%s\n", portstring);
	if ((cp = strchr(buffer, ':')) == NULL) {
		ports[0] = ports[1] = parse_port(buffer, "sctp");
	}
	else {
		*cp = '\0';
		cp++;

		ports[0] = buffer[0] ? parse_port(buffer, "sctp") : 0;
		ports[1] = cp[0] ? parse_port(cp, "sctp") : 0xFFFF;

		if (ports[0] > ports[1])
			exit_error(PARAMETER_PROBLEM,
				   "invalid portrange (min > max)");
	}
	free(buffer);
}

struct sctp_chunk_names {
	const char *name;
	unsigned int chunk_type;
	const char *valid_flags;
};

/*'ALL' and 'NONE' will be treated specially. */
static struct sctp_chunk_names sctp_chunk_names[]
= { { .name = "DATA", 		.chunk_type = 0,   .valid_flags = "-----UBE"},
    { .name = "INIT", 		.chunk_type = 1,   .valid_flags = "--------"},
    { .name = "INIT_ACK", 	.chunk_type = 2,   .valid_flags = "--------"},
    { .name = "SACK",		.chunk_type = 3,   .valid_flags = "--------"},
    { .name = "HEARTBEAT",	.chunk_type = 4,   .valid_flags = "--------"},
    { .name = "HEARTBEAT_ACK",	.chunk_type = 5,   .valid_flags = "--------"},
    { .name = "ABORT",		.chunk_type = 6,   .valid_flags = "-------T"},
    { .name = "SHUTDOWN",	.chunk_type = 7,   .valid_flags = "--------"},
    { .name = "SHUTDOWN_ACK",	.chunk_type = 8,   .valid_flags = "--------"},
    { .name = "ERROR",		.chunk_type = 9,   .valid_flags = "--------"},
    { .name = "COOKIE_ECHO",	.chunk_type = 10,  .valid_flags = "--------"},
    { .name = "COOKIE_ACK",	.chunk_type = 11,  .valid_flags = "--------"},
    { .name = "ECN_ECNE",	.chunk_type = 12,  .valid_flags = "--------"},
    { .name = "ECN_CWR",	.chunk_type = 13,  .valid_flags = "--------"},
    { .name = "SHUTDOWN_COMPLETE", .chunk_type = 14,  .valid_flags = "-------T"},
    { .name = "ASCONF",		.chunk_type = 31,  .valid_flags = "--------"},
    { .name = "ASCONF_ACK",	.chunk_type = 30,  .valid_flags = "--------"},
};

static void
save_chunk_flag_info(struct xt_sctp_flag_info *flag_info,
		     int *flag_count,
		     int chunktype, 
		     int bit, 
		     int set)
{
	int i;

	for (i = 0; i < *flag_count; i++) {
		if (flag_info[i].chunktype == chunktype) {
			DEBUGP("Previous match found\n");
			flag_info[i].chunktype = chunktype;
			flag_info[i].flag_mask |= (1 << bit);
			if (set) {
				flag_info[i].flag |= (1 << bit);
			}

			return;
		}
	}
	
	if (*flag_count == XT_NUM_SCTP_FLAGS) {
		exit_error (PARAMETER_PROBLEM,
			"Number of chunk types with flags exceeds currently allowed limit."
			"Increasing this limit involves changing XT_NUM_SCTP_FLAGS and"
			"recompiling both the kernel space and user space modules\n");
	}

	flag_info[*flag_count].chunktype = chunktype;
	flag_info[*flag_count].flag_mask |= (1 << bit);
	if (set) {
		flag_info[*flag_count].flag |= (1 << bit);
	}
	(*flag_count)++;
}

static void
parse_sctp_chunk(struct xt_sctp_info *einfo, 
		 const char *chunks)
{
	char *ptr;
	char *buffer;
	unsigned int i, j;
	int found = 0;
	char *chunk_flags;

	buffer = strdup(chunks);
	DEBUGP("Buffer: %s\n", buffer);

	SCTP_CHUNKMAP_RESET(einfo->chunkmap);

	if (!strcasecmp(buffer, "ALL")) {
		SCTP_CHUNKMAP_SET_ALL(einfo->chunkmap);
		goto out;
	}
	
	if (!strcasecmp(buffer, "NONE")) {
		SCTP_CHUNKMAP_RESET(einfo->chunkmap);
		goto out;
	}

	for (ptr = strtok(buffer, ","); ptr; ptr = strtok(NULL, ",")) {
		found = 0;
		DEBUGP("Next Chunk type %s\n", ptr);
		
		if ((chunk_flags = strchr(ptr, ':')) != NULL) {
			*chunk_flags++ = 0;
		}
		
		for (i = 0; i < ELEMCOUNT(sctp_chunk_names); i++) {
			if (strcasecmp(sctp_chunk_names[i].name, ptr) == 0) {
				DEBUGP("Chunk num %d\n", sctp_chunk_names[i].chunk_type);
				SCTP_CHUNKMAP_SET(einfo->chunkmap, 
					sctp_chunk_names[i].chunk_type);
				found = 1;
				break;
			}
		}
		if (!found)
			exit_error(PARAMETER_PROBLEM,
				   "Unknown sctp chunk `%s'", ptr);

		if (chunk_flags) {
			DEBUGP("Chunk flags %s\n", chunk_flags);
			for (j = 0; j < strlen(chunk_flags); j++) {
				char *p;
				int bit;

				if ((p = strchr(sctp_chunk_names[i].valid_flags, 
						toupper(chunk_flags[j]))) != NULL) {
					bit = p - sctp_chunk_names[i].valid_flags;
					bit = 7 - bit;

					save_chunk_flag_info(einfo->flag_info, 
						&(einfo->flag_count), i, bit, 
						isupper(chunk_flags[j]));
				} else {
					exit_error(PARAMETER_PROBLEM, 
						"Invalid flags for chunk type %d\n", i);
				}
			}
		}
	}
out:
	free(buffer);
}

static void
parse_sctp_chunks(struct xt_sctp_info *einfo,
		  const char *match_type,
		  const char *chunks)
{
	DEBUGP("Match type: %s Chunks: %s\n", match_type, chunks);
	if (!strcasecmp(match_type, "ANY")) {
		einfo->chunk_match_type = SCTP_CHUNK_MATCH_ANY;
	} else 	if (!strcasecmp(match_type, "ALL")) {
		einfo->chunk_match_type = SCTP_CHUNK_MATCH_ALL;
	} else 	if (!strcasecmp(match_type, "ONLY")) {
		einfo->chunk_match_type = SCTP_CHUNK_MATCH_ONLY;
	} else {
		exit_error (PARAMETER_PROBLEM, 
			"Match type has to be one of \"ALL\", \"ANY\" or \"ONLY\"");
	}

	SCTP_CHUNKMAP_RESET(einfo->chunkmap);
	parse_sctp_chunk(einfo, chunks);
}

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct xt_sctp_info *einfo
		= (struct xt_sctp_info *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & XT_SCTP_SRC_PORTS)
			exit_error(PARAMETER_PROBLEM,
			           "Only one `--source-port' allowed");
		einfo->flags |= XT_SCTP_SRC_PORTS;
		check_inverse(optarg, &invert, &optind, 0);
		parse_sctp_ports(argv[optind-1], einfo->spts);
		if (invert)
			einfo->invflags |= XT_SCTP_SRC_PORTS;
		*flags |= XT_SCTP_SRC_PORTS;
		break;

	case '2':
		if (*flags & XT_SCTP_DEST_PORTS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--destination-port' allowed");
		einfo->flags |= XT_SCTP_DEST_PORTS;
		check_inverse(optarg, &invert, &optind, 0);
		parse_sctp_ports(argv[optind-1], einfo->dpts);
		if (invert)
			einfo->invflags |= XT_SCTP_DEST_PORTS;
		*flags |= XT_SCTP_DEST_PORTS;
		break;

	case '3':
		if (*flags & XT_SCTP_CHUNK_TYPES)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--chunk-types' allowed");
		check_inverse(optarg, &invert, &optind, 0);

		if (!argv[optind] 
		    || argv[optind][0] == '-' || argv[optind][0] == '!')
			exit_error(PARAMETER_PROBLEM,
				   "--chunk-types requires two args");

		einfo->flags |= XT_SCTP_CHUNK_TYPES;
		parse_sctp_chunks(einfo, argv[optind-1], argv[optind]);
		if (invert)
			einfo->invflags |= XT_SCTP_CHUNK_TYPES;
		optind++;
		*flags |= XT_SCTP_CHUNK_TYPES;
		break;

	default:
		return 0;
	}
	return 1;
}

static void
final_check(unsigned int flags)
{
}

static char *
port_to_service(int port)
{
	struct servent *service;

	if ((service = getservbyport(htons(port), "sctp")))
		return service->s_name;

	return NULL;
}

static void
print_port(u_int16_t port, int numeric)
{
	char *service;

	if (numeric || (service = port_to_service(port)) == NULL)
		printf("%u", port);
	else
		printf("%s", service);
}

static void
print_ports(const char *name, u_int16_t min, u_int16_t max,
	    int invert, int numeric)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFF || invert) {
		printf("%s", name);
		if (min == max) {
			printf(":%s", inv);
			print_port(min, numeric);
		} else {
			printf("s:%s", inv);
			print_port(min, numeric);
			printf(":");
			print_port(max, numeric);
		}
		printf(" ");
	}
}

static void
print_chunk_flags(u_int32_t chunknum, u_int8_t chunk_flags, u_int8_t chunk_flags_mask)
{
	int i;

	DEBUGP("type: %d\tflags: %x\tflag mask: %x\n", chunknum, chunk_flags, 
			chunk_flags_mask);

	if (chunk_flags_mask) {
		printf(":");
	}

	for (i = 7; i >= 0; i--) {
		if (chunk_flags_mask & (1 << i)) {
			if (chunk_flags & (1 << i)) {
				printf("%c", sctp_chunk_names[chunknum].valid_flags[7-i]);
			} else {
				printf("%c", tolower(sctp_chunk_names[chunknum].valid_flags[7-i]));
			}
		}
	}
}

static void
print_chunk(u_int32_t chunknum, int numeric)
{
	if (numeric) {
		printf("0x%04X", chunknum);
	}
	else {
		int i;

		for (i = 0; i < ELEMCOUNT(sctp_chunk_names); i++) {
			if (sctp_chunk_names[i].chunk_type == chunknum)
				printf("%s", sctp_chunk_names[chunknum].name);
		}
	}
}

static void
print_chunks(u_int32_t chunk_match_type, 
	     const u_int32_t *chunkmap, 
	     const struct xt_sctp_flag_info *flag_info,
	     int flag_count,
	     int numeric)
{
	int i, j;
	int flag;

	switch (chunk_match_type) {
		case SCTP_CHUNK_MATCH_ANY:	printf("any "); break;
		case SCTP_CHUNK_MATCH_ALL:	printf("all "); break;
		case SCTP_CHUNK_MATCH_ONLY:	printf("only "); break;
		default:	printf("Never reach herer\n"); break;
	}

	if (SCTP_CHUNKMAP_IS_CLEAR(chunkmap)) {
		printf("NONE ");
		goto out;
	}
	
	if (SCTP_CHUNKMAP_IS_ALL_SET(chunkmap)) {
		printf("ALL ");
		goto out;
	}
	
	flag = 0;
	for (i = 0; i < 256; i++) {
		if (SCTP_CHUNKMAP_IS_SET(chunkmap, i)) {
			if (flag)
				printf(",");
			flag = 1;
			print_chunk(i, numeric);
			for (j = 0; j < flag_count; j++) {
				if (flag_info[j].chunktype == i) {
					print_chunk_flags(i, flag_info[j].flag,
						flag_info[j].flag_mask);
				}
			}
		}
	}

	if (flag)
		printf(" ");
out:
	return;
}

/* Prints out the matchinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match,
      int numeric)
{
	const struct xt_sctp_info *einfo =
		(const struct xt_sctp_info *)match->data;

	printf("sctp ");

	if (einfo->flags & XT_SCTP_SRC_PORTS) {
		print_ports("spt", einfo->spts[0], einfo->spts[1],
			einfo->invflags & XT_SCTP_SRC_PORTS,
			numeric);
	}

	if (einfo->flags & XT_SCTP_DEST_PORTS) {
		print_ports("dpt", einfo->dpts[0], einfo->dpts[1],
			einfo->invflags & XT_SCTP_DEST_PORTS,
			numeric);
	}

	if (einfo->flags & XT_SCTP_CHUNK_TYPES) {
		/* FIXME: print_chunks() is used in save() where the printing of '!'
		s taken care of, so we need to do that here as well */
		if (einfo->invflags & XT_SCTP_CHUNK_TYPES) {
			printf("! ");
		}
		print_chunks(einfo->chunk_match_type, einfo->chunkmap,
			einfo->flag_info, einfo->flag_count, numeric);
	}
}

/* Saves the union xt_matchinfo in parsable form to stdout. */
static void
save(const struct ip6t_ip6 *ip, 
     const struct ip6t_entry_match *match)
{
	const struct xt_sctp_info *einfo =
		(const struct xt_sctp_info *)match->data;

	if (einfo->flags & XT_SCTP_SRC_PORTS) {
		if (einfo->invflags & XT_SCTP_SRC_PORTS)
			printf("! ");
		if (einfo->spts[0] != einfo->spts[1])
			printf("--sport %u:%u ", 
			       einfo->spts[0], einfo->spts[1]);
		else
			printf("--sport %u ", einfo->spts[0]);
	}

	if (einfo->flags & XT_SCTP_DEST_PORTS) {
		if (einfo->invflags & XT_SCTP_DEST_PORTS)
			printf("! ");
		if (einfo->dpts[0] != einfo->dpts[1])
			printf("--dport %u:%u ",
			       einfo->dpts[0], einfo->dpts[1]);
		else
			printf("--dport %u ", einfo->dpts[0]);
	}

	if (einfo->flags & XT_SCTP_CHUNK_TYPES) {
		if (einfo->invflags & XT_SCTP_CHUNK_TYPES)
			printf("! ");
		printf("--chunk-types ");

		print_chunks(einfo->chunk_match_type, einfo->chunkmap, 
			einfo->flag_info, einfo->flag_count, 0);
	}
}

static
struct ip6tables_match sctp
= { .name          = "sctp",
    .version       = IPTABLES_VERSION,
    .size          = IP6T_ALIGN(sizeof(struct xt_sctp_info)),
    .userspacesize = IP6T_ALIGN(sizeof(struct xt_sctp_info)),
    .help          = &help,
    .init          = &init,
    .parse         = &parse,
    .final_check   = &final_check,
    .print         = &print,
    .save          = &save,
    .extra_opts    = opts
};

void _init(void)
{
	register_match6(&sctp);
}


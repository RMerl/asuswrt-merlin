/* vi: set sw=4 ts=4: */
/*
 * DHCP server config and lease file manipulation
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include <netinet/ether.h>

#include "common.h"
#include "dhcpd.h"

/* on these functions, make sure your datatype matches */
static int FAST_FUNC read_str(const char *line, void *arg)
{
	char **dest = arg;

	free(*dest);
	*dest = xstrdup(line);
	return 1;
}

static int FAST_FUNC read_u32(const char *line, void *arg)
{
	*(uint32_t*)arg = bb_strtou32(line, NULL, 10);
	return errno == 0;
}

static int FAST_FUNC read_staticlease(const char *const_line, void *arg)
{
	char *line;
	char *mac_string;
	char *ip_string;
	struct ether_addr mac_bytes; /* it's "struct { uint8_t mac[6]; }" */
	uint32_t nip;

	/* Read mac */
	line = (char *) const_line;
	mac_string = strtok_r(line, " \t", &line);
	if (!mac_string || !ether_aton_r(mac_string, &mac_bytes))
		return 0;

	/* Read ip */
	ip_string = strtok_r(NULL, " \t", &line);
	if (!ip_string || !udhcp_str2nip(ip_string, &nip))
		return 0;

	add_static_lease(arg, (uint8_t*) &mac_bytes, nip);

	log_static_leases(arg);

	return 1;
}


struct config_keyword {
	const char *keyword;
	int (*handler)(const char *line, void *var) FAST_FUNC;
	unsigned ofs;
	const char *def;
};

#define OFS(field) offsetof(struct server_config_t, field)

static const struct config_keyword keywords[] = {
	/* keyword        handler           variable address               default */
	{"start"        , udhcp_str2nip   , OFS(start_ip     ), "192.168.0.20"},
	{"end"          , udhcp_str2nip   , OFS(end_ip       ), "192.168.0.254"},
	{"interface"    , read_str        , OFS(interface    ), "eth0"},
	/* Avoid "max_leases value not sane" warning by setting default
	 * to default_end_ip - default_start_ip + 1: */
	{"max_leases"   , read_u32        , OFS(max_leases   ), "235"},
	{"auto_time"    , read_u32        , OFS(auto_time    ), "7200"},
	{"decline_time" , read_u32        , OFS(decline_time ), "3600"},
	{"conflict_time", read_u32        , OFS(conflict_time), "3600"},
	{"offer_time"   , read_u32        , OFS(offer_time   ), "60"},
	{"min_lease"    , read_u32        , OFS(min_lease_sec), "60"},
	{"lease_file"   , read_str        , OFS(lease_file   ), LEASES_FILE},
	{"pidfile"      , read_str        , OFS(pidfile      ), "/var/run/udhcpd.pid"},
	{"siaddr"       , udhcp_str2nip   , OFS(siaddr_nip   ), "0.0.0.0"},
	/* keywords with no defaults must be last! */
	{"option"       , udhcp_str2optset, OFS(options      ), ""},
	{"opt"          , udhcp_str2optset, OFS(options      ), ""},
	{"notify_file"  , read_str        , OFS(notify_file  ), NULL},
	{"sname"        , read_str        , OFS(sname        ), NULL},
	{"boot_file"    , read_str        , OFS(boot_file    ), NULL},
	{"static_lease" , read_staticlease, OFS(static_leases), ""},
};
enum { KWS_WITH_DEFAULTS = ARRAY_SIZE(keywords) - 6 };

void FAST_FUNC read_config(const char *file)
{
	parser_t *parser;
	const struct config_keyword *k;
	unsigned i;
	char *token[2];

	for (i = 0; i < KWS_WITH_DEFAULTS; i++)
		keywords[i].handler(keywords[i].def, (char*)&server_config + keywords[i].ofs);

	parser = config_open(file);
	while (config_read(parser, token, 2, 2, "# \t", PARSE_NORMAL)) {
		for (k = keywords, i = 0; i < ARRAY_SIZE(keywords); k++, i++) {
			if (strcasecmp(token[0], k->keyword) == 0) {
				if (!k->handler(token[1], (char*)&server_config + k->ofs)) {
					bb_error_msg("can't parse line %u in %s",
							parser->lineno, file);
					/* reset back to the default value */
					k->handler(k->def, (char*)&server_config + k->ofs);
				}
				break;
			}
		}
	}
	config_close(parser);

	server_config.start_ip = ntohl(server_config.start_ip);
	server_config.end_ip = ntohl(server_config.end_ip);
}

void FAST_FUNC write_leases(void)
{
	int fd;
	unsigned i;
	leasetime_t curr;
	int64_t written_at;

	fd = open_or_warn(server_config.lease_file, O_WRONLY|O_CREAT|O_TRUNC);
	if (fd < 0)
		return;

	curr = written_at = time(NULL);

	written_at = SWAP_BE64(written_at);
	full_write(fd, &written_at, sizeof(written_at));

	for (i = 0; i < server_config.max_leases; i++) {
		leasetime_t tmp_time;

		if (g_leases[i].lease_nip == 0)
			continue;

		/* Screw with the time in the struct, for easier writing */
		tmp_time = g_leases[i].expires;

		g_leases[i].expires -= curr;
		if ((signed_leasetime_t) g_leases[i].expires < 0)
			g_leases[i].expires = 0;
		g_leases[i].expires = htonl(g_leases[i].expires);

		/* No error check. If the file gets truncated,
		 * we lose some leases on restart. Oh well. */
		full_write(fd, &g_leases[i], sizeof(g_leases[i]));

		/* Then restore it when done */
		g_leases[i].expires = tmp_time;
	}
	close(fd);

	if (server_config.notify_file) {
		char *argv[3];
		argv[0] = server_config.notify_file;
		argv[1] = server_config.lease_file;
		argv[2] = NULL;
		spawn_and_wait(argv);
	}
}

void FAST_FUNC read_leases(const char *file)
{
	struct dyn_lease lease;
	int64_t written_at, time_passed;
	int fd;
#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
	unsigned i = 0;
#endif

	fd = open_or_warn(file, O_RDONLY);
	if (fd < 0)
		return;

	if (full_read(fd, &written_at, sizeof(written_at)) != sizeof(written_at))
		goto ret;
	written_at = SWAP_BE64(written_at);

	time_passed = time(NULL) - written_at;
	/* Strange written_at, or lease file from old version of udhcpd
	 * which had no "written_at" field? */
	if ((uint64_t)time_passed > 12 * 60 * 60)
		goto ret;

	while (full_read(fd, &lease, sizeof(lease)) == sizeof(lease)) {
		uint32_t y = ntohl(lease.lease_nip);
		if (y >= server_config.start_ip && y <= server_config.end_ip) {
			signed_leasetime_t expires = ntohl(lease.expires) - (signed_leasetime_t)time_passed;
			uint32_t static_nip;

			if (expires <= 0)
				/* We keep expired leases: add_lease() will add
				 * a lease with 0 seconds remaining.
				 * Fewer IP address changes this way for mass reboot scenario.
				 */
				expires = 0;

			/* Check if there is a different static lease for this IP or MAC */
			static_nip = get_static_nip_by_mac(server_config.static_leases, lease.lease_mac);
			if (static_nip) {
				/* NB: we do not add lease even if static_nip == lease.lease_nip.
				 */
				continue;
			}
			if (is_nip_reserved(server_config.static_leases, lease.lease_nip))
				continue;

			/* NB: add_lease takes "relative time", IOW,
			 * lease duration, not lease deadline. */
			if (add_lease(lease.lease_mac, lease.lease_nip,
					expires,
					lease.hostname, sizeof(lease.hostname)
				) == 0
			) {
				bb_error_msg("too many leases while loading %s", file);
				break;
			}
#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
			i++;
#endif
		}
	}
	log1("read %d leases", i);
 ret:
	close(fd);
}

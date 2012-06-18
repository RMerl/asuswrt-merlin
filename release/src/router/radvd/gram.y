/*
 *
 *   Authors:
 *    Pedro Roque		<roque@di.fc.ul.pt>
 *    Lars Fenneberg		<lf@elemental.net>
 *
 *   This software is Copyright 1996-2000 by the above mentioned author(s),
 *   All Rights Reserved.
 *
 *   The license which is distributed with this software in the file COPYRIGHT
 *   applies to this software. If your distribution is missing this file, you
 *   may request it from <pekkas@netcore.fi>.
 *
 */
%{
#include "config.h"
#include "includes.h"
#include "radvd.h"
#include "defaults.h"

extern struct Interface *IfaceList;
struct Interface *iface = NULL;
struct AdvPrefix *prefix = NULL;
struct AdvRoute *route = NULL;
struct AdvRDNSS *rdnss = NULL;
struct AdvDNSSL *dnssl = NULL;

extern char *conf_file;
extern int num_lines;
extern char *yytext;

static void cleanup(void);
static void yyerror(char *msg);
static int countbits(int b);
static int count_mask(struct sockaddr_in6 *m);
static struct in6_addr get_prefix6(struct in6_addr const *addr, struct in6_addr const *mask);

#if 0 /* no longer necessary? */
#ifndef HAVE_IN6_ADDR_S6_ADDR
# ifdef __FreeBSD__
#  define s6_addr32 __u6_addr.__u6_addr32
#  define s6_addr16 __u6_addr.__u6_addr16
# endif
#endif
#endif

#define ABORT	do { cleanup(); YYABORT; } while (0);
#define ADD_TO_LL(type, list, value) \
	do { \
		if (iface->list == NULL) \
			iface->list = value; \
		else { \
			type *current = iface->list; \
			while (current->next != NULL) \
				current = current->next; \
			current->next = value; \
		} \
	} while (0)


%}

%token		T_INTERFACE
%token		T_PREFIX
%token		T_ROUTE
%token		T_RDNSS
%token		T_DNSSL
%token		T_CLIENTS

%token	<str>	STRING
%token	<num>	NUMBER
%token	<snum>	SIGNEDNUMBER
%token	<dec>	DECIMAL
%token	<num>	SWITCH
%token	<addr>	IPV6ADDR
%token 		INFINITY

%token		T_IgnoreIfMissing
%token		T_AdvSendAdvert
%token		T_MaxRtrAdvInterval
%token		T_MinRtrAdvInterval
%token		T_MinDelayBetweenRAs
%token		T_AdvManagedFlag
%token		T_AdvOtherConfigFlag
%token		T_AdvLinkMTU
%token		T_AdvReachableTime
%token		T_AdvRetransTimer
%token		T_AdvCurHopLimit
%token		T_AdvDefaultLifetime
%token		T_AdvDefaultPreference
%token		T_AdvSourceLLAddress

%token		T_AdvOnLink
%token		T_AdvAutonomous
%token		T_AdvValidLifetime
%token		T_AdvPreferredLifetime
%token		T_DeprecatePrefix
%token		T_DecrementLifetimes

%token		T_AdvRouterAddr
%token		T_AdvHomeAgentFlag
%token		T_AdvIntervalOpt
%token		T_AdvHomeAgentInfo

%token		T_Base6Interface
%token		T_Base6to4Interface
%token		T_UnicastOnly

%token		T_HomeAgentPreference
%token		T_HomeAgentLifetime

%token		T_AdvRoutePreference
%token		T_AdvRouteLifetime
%token		T_RemoveRoute

%token		T_AdvRDNSSPreference
%token		T_AdvRDNSSOpenFlag
%token		T_AdvRDNSSLifetime
%token		T_FlushRDNSS

%token		T_AdvDNSSLLifetime
%token		T_FlushDNSSL

%token		T_AdvMobRtrSupportFlag

%token		T_BAD_TOKEN

%type	<str>	name
%type	<pinfo> prefixdef
%type	<ainfo> clientslist v6addrlist
%type	<rinfo>	routedef
%type	<rdnssinfo> rdnssdef
%type	<dnsslinfo> dnssldef
%type   <num>	number_or_infinity

%union {
	unsigned int		num;
	int			snum;
	double			dec;
	struct in6_addr		*addr;
	char			*str;
	struct AdvPrefix	*pinfo;
	struct AdvRoute		*rinfo;
	struct AdvRDNSS		*rdnssinfo;
	struct AdvDNSSL		*dnsslinfo;
	struct Clients		*ainfo;
};

%%

grammar		: grammar ifacedef
		| ifacedef
		;

ifacedef	: ifacehead '{' ifaceparams  '}' ';'
		{
			struct Interface *iface2;

			iface2 = IfaceList;
			while (iface2)
			{
				if (!strcmp(iface2->Name, iface->Name))
				{
					flog(LOG_ERR, "duplicate interface "
						"definition for %s", iface->Name);
					ABORT;
				}
				iface2 = iface2->next;
			}

			if (check_device(iface) < 0) {
				if (iface->IgnoreIfMissing) {
					dlog(LOG_DEBUG, 4, "interface %s did not exist, ignoring the interface", iface->Name);
				}
				else {
					flog(LOG_ERR, "interface %s does not exist", iface->Name);
					ABORT;
				}
			}
			if (setup_deviceinfo(iface) < 0)
				if (!iface->IgnoreIfMissing)
				ABORT;
			if (check_iface(iface) < 0)
				if (!iface->IgnoreIfMissing)
				ABORT;
			if (setup_linklocal_addr(iface) < 0)
				if (!iface->IgnoreIfMissing)
				ABORT;
			if (setup_allrouters_membership(iface) < 0)
				if (!iface->IgnoreIfMissing)
				ABORT;

			dlog(LOG_DEBUG, 4, "interface definition for %s is ok", iface->Name);

			iface->next = IfaceList;
			IfaceList = iface;

			iface = NULL;
		};

ifacehead	: T_INTERFACE name
		{
			iface = malloc(sizeof(struct Interface));

			if (iface == NULL) {
				flog(LOG_CRIT, "malloc failed: %s", strerror(errno));
				ABORT;
			}

			iface_init_defaults(iface);
			strncpy(iface->Name, $2, IFNAMSIZ-1);
			iface->Name[IFNAMSIZ-1] = '\0';
		}
		;

name		: STRING
		{
			/* check vality */
			$$ = $1;
		}
		;

ifaceparams :
		/* empty */
		| ifaceparam ifaceparams
		;

ifaceparam 	: ifaceval
		| prefixdef 	{ ADD_TO_LL(struct AdvPrefix, AdvPrefixList, $1); }
		| clientslist 	{ ADD_TO_LL(struct Clients, ClientList, $1); }
		| routedef 	{ ADD_TO_LL(struct AdvRoute, AdvRouteList, $1); }
		| rdnssdef 	{ ADD_TO_LL(struct AdvRDNSS, AdvRDNSSList, $1); }
		| dnssldef 	{ ADD_TO_LL(struct AdvDNSSL, AdvDNSSLList, $1); }
		;

ifaceval	: T_MinRtrAdvInterval NUMBER ';'
		{
			iface->MinRtrAdvInterval = $2;
		}
		| T_MaxRtrAdvInterval NUMBER ';'
		{
			iface->MaxRtrAdvInterval = $2;
		}
		| T_MinDelayBetweenRAs NUMBER ';'
		{
			iface->MinDelayBetweenRAs = $2;
		}
		| T_MinRtrAdvInterval DECIMAL ';'
		{
			iface->MinRtrAdvInterval = $2;
		}
		| T_MaxRtrAdvInterval DECIMAL ';'
		{
			iface->MaxRtrAdvInterval = $2;
		}
		| T_MinDelayBetweenRAs DECIMAL ';'
		{
			iface->MinDelayBetweenRAs = $2;
		}
		| T_IgnoreIfMissing SWITCH ';'
		{
			iface->IgnoreIfMissing = $2;
		}
		| T_AdvSendAdvert SWITCH ';'
		{
			iface->AdvSendAdvert = $2;
		}
		| T_AdvManagedFlag SWITCH ';'
		{
			iface->AdvManagedFlag = $2;
		}
		| T_AdvOtherConfigFlag SWITCH ';'
		{
			iface->AdvOtherConfigFlag = $2;
		}
		| T_AdvLinkMTU NUMBER ';'
		{
			iface->AdvLinkMTU = $2;
		}
		| T_AdvReachableTime NUMBER ';'
		{
			iface->AdvReachableTime = $2;
		}
		| T_AdvRetransTimer NUMBER ';'
		{
			iface->AdvRetransTimer = $2;
		}
		| T_AdvDefaultLifetime NUMBER ';'
		{
			iface->AdvDefaultLifetime = $2;
		}
		| T_AdvDefaultPreference SIGNEDNUMBER ';'
		{
			iface->AdvDefaultPreference = $2;
		}
		| T_AdvCurHopLimit NUMBER ';'
		{
			iface->AdvCurHopLimit = $2;
		}
		| T_AdvSourceLLAddress SWITCH ';'
		{
			iface->AdvSourceLLAddress = $2;
		}
		| T_AdvIntervalOpt SWITCH ';'
		{
			iface->AdvIntervalOpt = $2;
		}
		| T_AdvHomeAgentInfo SWITCH ';'
		{
			iface->AdvHomeAgentInfo = $2;
		}
		| T_AdvHomeAgentFlag SWITCH ';'
		{
			iface->AdvHomeAgentFlag = $2;
		}
		| T_HomeAgentPreference NUMBER ';'
		{
			iface->HomeAgentPreference = $2;
		}
		| T_HomeAgentLifetime NUMBER ';'
		{
			iface->HomeAgentLifetime = $2;
		}
		| T_UnicastOnly SWITCH ';'
		{
			iface->UnicastOnly = $2;
		}
		| T_AdvMobRtrSupportFlag SWITCH ';'
		{
			iface->AdvMobRtrSupportFlag = $2;
		}
		;

clientslist	: T_CLIENTS '{' v6addrlist '}' ';'
		{
			$$ = $3;
		}
		;

v6addrlist	: IPV6ADDR ';'
		{
			struct Clients *new = calloc(1, sizeof(struct Clients));
			if (new == NULL) {
				flog(LOG_CRIT, "calloc failed: %s", strerror(errno));
				ABORT;
			}

			memcpy(&(new->Address), $1, sizeof(struct in6_addr));
			$$ = new;
		}
		| v6addrlist IPV6ADDR ';'
		{
			struct Clients *new = calloc(1, sizeof(struct Clients));
			if (new == NULL) {
				flog(LOG_CRIT, "calloc failed: %s", strerror(errno));
				ABORT;
			}

			memcpy(&(new->Address), $2, sizeof(struct in6_addr));
			new->next = $1;
			$$ = new;
		}
		;


prefixdef	: prefixhead optional_prefixplist ';'
		{
			if (prefix) {
				unsigned int dst;

				if (prefix->AdvPreferredLifetime > prefix->AdvValidLifetime)
				{
					flog(LOG_ERR, "AdvValidLifeTime must be "
						"greater than AdvPreferredLifetime in %s, line %d",
						conf_file, num_lines);
					ABORT;
				}

				if ( prefix->if6[0] && prefix->if6to4[0]) {
					flog(LOG_ERR, "Base6Interface and Base6to4Interface are mutually exclusive at this time.");
					ABORT;
				}

				if ( prefix->if6to4[0] )
				{
					if (get_v4addr(prefix->if6to4, &dst) < 0)
					{
						flog(LOG_ERR, "interface %s has no IPv4 addresses, disabling 6to4 prefix", prefix->if6to4 );
						prefix->enabled = 0;
					}
					else
					{
						*((uint16_t *)(prefix->Prefix.s6_addr)) = htons(0x2002);
						memcpy( prefix->Prefix.s6_addr + 2, &dst, sizeof( dst ) );
					}
				}

				if ( prefix->if6[0] )
				{
#ifndef HAVE_IFADDRS_H
					flog(LOG_ERR, "Base6Interface not supported in %s, line %d", conf_file, num_lines);
					ABORT;
#else
					struct ifaddrs *ifap = 0, *ifa = 0;
					struct AdvPrefix *next = prefix->next;

					if (prefix->PrefixLen != 64) {
						flog(LOG_ERR, "Only /64 is allowed with Base6Interface.  %s:%d", conf_file, num_lines);
						ABORT;
					}

					if (getifaddrs(&ifap) != 0)
						flog(LOG_ERR, "getifaddrs failed: %s", strerror(errno));

					for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
						struct sockaddr_in6 *s6 = 0;
						struct sockaddr_in6 *mask = (struct sockaddr_in6 *)ifa->ifa_netmask;
						struct in6_addr base6prefix;
						char buf[INET6_ADDRSTRLEN];
						int i;

						if (strncmp(ifa->ifa_name, prefix->if6, IFNAMSIZ))
							continue;

						if (ifa->ifa_addr->sa_family != AF_INET6)
							continue;

						s6 = (struct sockaddr_in6 *)(ifa->ifa_addr);

						if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr))
							continue;

						base6prefix = get_prefix6(&s6->sin6_addr, &mask->sin6_addr);
						for (i = 0; i < 8; ++i) {
							prefix->Prefix.s6_addr[i] &= ~mask->sin6_addr.s6_addr[i];
							prefix->Prefix.s6_addr[i] |= base6prefix.s6_addr[i];
						}
						memset(&prefix->Prefix.s6_addr[8], 0, 8);
						prefix->AdvRouterAddr = 1;
						prefix->AutoSelected = 1;
						prefix->next = next;

						if (inet_ntop(ifa->ifa_addr->sa_family, (void *)&(prefix->Prefix), buf, sizeof(buf)) == NULL)
							flog(LOG_ERR, "%s: inet_ntop failed in %s, line %d!", ifa->ifa_name, conf_file, num_lines);
						else
							dlog(LOG_DEBUG, 3, "auto-selected prefix %s/%d on interface %s from interface %s",
								buf, prefix->PrefixLen, iface->Name, ifa->ifa_name);

						/* Taking only one prefix from the Base6Interface.  Taking more than one would require allocating new
						   prefixes and building a list.  I'm not sure how to do that from here. So for now, break. */
						break;
					}

					if (ifap)
						freeifaddrs(ifap);
#endif /* ifndef HAVE_IFADDRS_H */
				}
			}
			$$ = prefix;
			prefix = NULL;
		}
		;

prefixhead	: T_PREFIX IPV6ADDR '/' NUMBER
		{
			struct in6_addr zeroaddr;
			memset(&zeroaddr, 0, sizeof(zeroaddr));

			if (!memcmp($2, &zeroaddr, sizeof(struct in6_addr))) {
#ifndef HAVE_IFADDRS_H
				flog(LOG_ERR, "invalid all-zeros prefix in %s, line %d", conf_file, num_lines);
				ABORT;
#else
				struct ifaddrs *ifap = 0, *ifa = 0;
				struct AdvPrefix *next = iface->AdvPrefixList;

				while (next) {
					if (next->AutoSelected) {
						flog(LOG_ERR, "auto selecting prefixes works only once per interface.  See %s, line %d", conf_file, num_lines);
						ABORT;
					}
					next = next->next;
				}
				next = 0;

				dlog(LOG_DEBUG, 5, "all-zeros prefix in %s, line %d, parsing..", conf_file, num_lines);

				if (getifaddrs(&ifap) != 0)
					flog(LOG_ERR, "getifaddrs failed: %s", strerror(errno));

				for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
					struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
					struct sockaddr_in6 *mask = (struct sockaddr_in6 *)ifa->ifa_netmask;
					char buf[INET6_ADDRSTRLEN];

					if (strncmp(ifa->ifa_name, iface->Name, IFNAMSIZ))
						continue;

					if (ifa->ifa_addr->sa_family != AF_INET6)
						continue;

					s6 = (struct sockaddr_in6 *)(ifa->ifa_addr);

					if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr))
						continue;

					prefix = malloc(sizeof(struct AdvPrefix));

					if (prefix == NULL) {
						flog(LOG_CRIT, "malloc failed: %s", strerror(errno));
						ABORT;
					}

					prefix_init_defaults(prefix);
					prefix->Prefix = get_prefix6(&s6->sin6_addr, &mask->sin6_addr);
					prefix->AdvRouterAddr = 1;
					prefix->AutoSelected = 1;
					prefix->next = next;
					next = prefix;

					if (prefix->PrefixLen == 0)
						prefix->PrefixLen = count_mask(mask);

					if (inet_ntop(ifa->ifa_addr->sa_family, (void *)&(prefix->Prefix), buf, sizeof(buf)) == NULL)
						flog(LOG_ERR, "%s: inet_ntop failed in %s, line %d!", ifa->ifa_name, conf_file, num_lines);
					else
						dlog(LOG_DEBUG, 3, "auto-selected prefix %s/%d on interface %s", buf, prefix->PrefixLen, ifa->ifa_name);
				}

				if (!prefix) {
					flog(LOG_WARNING, "no auto-selected prefix on interface %s, disabling advertisements",  iface->Name);
				}

				if (ifap)
					freeifaddrs(ifap);
#endif /* ifndef HAVE_IFADDRS_H */
			}
			else {
				prefix = malloc(sizeof(struct AdvPrefix));

				if (prefix == NULL) {
					flog(LOG_CRIT, "malloc failed: %s", strerror(errno));
					ABORT;
				}

				prefix_init_defaults(prefix);

				if ($4 > MAX_PrefixLen)
				{
					flog(LOG_ERR, "invalid prefix length in %s, line %d", conf_file, num_lines);
					ABORT;
				}

				prefix->PrefixLen = $4;

				memcpy(&prefix->Prefix, $2, sizeof(struct in6_addr));
			}
		}
		;

optional_prefixplist: /* empty */
		| '{' /* somewhat empty */ '}'
		| '{' prefixplist '}'
		;

prefixplist	: prefixplist prefixparms
		| prefixparms
		;

prefixparms	: T_AdvOnLink SWITCH ';'
		{
			if (prefix) {
				if (prefix->AutoSelected) {
					struct AdvPrefix *p = prefix;
					do {
						p->AdvOnLinkFlag = $2;
						p = p->next;
					} while (p && p->AutoSelected);
				}
				else
					prefix->AdvOnLinkFlag = $2;
			}
		}
		| T_AdvAutonomous SWITCH ';'
		{
			if (prefix) {
				if (prefix->AutoSelected) {
					struct AdvPrefix *p = prefix;
					do {
						p->AdvAutonomousFlag = $2;
						p = p->next;
					} while (p && p->AutoSelected);
				}
				else
					prefix->AdvAutonomousFlag = $2;
			}
		}
		| T_AdvRouterAddr SWITCH ';'
		{
			if (prefix) {
				if (prefix->AutoSelected && $2 == 0)
					flog(LOG_WARNING, "prefix automatically selected, AdvRouterAddr always enabled, ignoring config line %d", num_lines);
				else
					prefix->AdvRouterAddr = $2;
			}
		}
		| T_AdvValidLifetime number_or_infinity ';'
		{
			if (prefix) {
				if (prefix->AutoSelected) {
					struct AdvPrefix *p = prefix;
					do {
						p->AdvValidLifetime = $2;
						p->curr_validlft = $2;
						p = p->next;
					} while (p && p->AutoSelected);
				}
				else
					prefix->AdvValidLifetime = $2;
					prefix->curr_validlft = $2;
			}
		}
		| T_AdvPreferredLifetime number_or_infinity ';'
		{
			if (prefix) {
				if (prefix->AutoSelected) {
					struct AdvPrefix *p = prefix;
					do {
						p->AdvPreferredLifetime = $2;
						p->curr_preferredlft = $2;
						p = p->next;
					} while (p && p->AutoSelected);
				}
				else
					prefix->AdvPreferredLifetime = $2;
					prefix->curr_preferredlft = $2;
			}
		}
		| T_DeprecatePrefix SWITCH ';'
		{
			prefix->DeprecatePrefixFlag = $2;
		}
		| T_DecrementLifetimes SWITCH ';'
		{
			prefix->DecrementLifetimesFlag = $2;
		}
		| T_Base6Interface name ';'
		{
			if (prefix) {
				if (prefix->AutoSelected) {
					flog(LOG_ERR, "automatically selecting the prefix and Base6to4Interface are mutually exclusive");
					ABORT;
				} /* fallthrough */
				dlog(LOG_DEBUG, 4, "using prefixes on interface %s for prefixes on interface %s", $2, iface->Name);
				strncpy(prefix->if6, $2, IFNAMSIZ-1);
				prefix->if6[IFNAMSIZ-1] = '\0';
			}
		}

		| T_Base6to4Interface name ';'
		{
			if (prefix) {
				if (prefix->AutoSelected) {
					flog(LOG_ERR, "automatically selecting the prefix and Base6to4Interface are mutually exclusive");
					ABORT;
				} /* fallthrough */
				dlog(LOG_DEBUG, 4, "using interface %s for 6to4 prefixes on interface %s", $2, iface->Name);
				strncpy(prefix->if6to4, $2, IFNAMSIZ-1);
				prefix->if6to4[IFNAMSIZ-1] = '\0';
			}
		}
		;

routedef	: routehead '{' optional_routeplist '}' ';'
		{
			$$ = route;
			route = NULL;
		}
		;


routehead	: T_ROUTE IPV6ADDR '/' NUMBER
		{
			route = malloc(sizeof(struct AdvRoute));

			if (route == NULL) {
				flog(LOG_CRIT, "malloc failed: %s", strerror(errno));
				ABORT;
			}

			route_init_defaults(route, iface);

			if ($4 > MAX_PrefixLen)
			{
				flog(LOG_ERR, "invalid route prefix length in %s, line %d", conf_file, num_lines);
				ABORT;
			}

			route->PrefixLen = $4;

			memcpy(&route->Prefix, $2, sizeof(struct in6_addr));
		}
		;


optional_routeplist: /* empty */
		| routeplist
		;

routeplist	: routeplist routeparms
		| routeparms
		;


routeparms	: T_AdvRoutePreference SIGNEDNUMBER ';'
		{
			route->AdvRoutePreference = $2;
		}
		| T_AdvRouteLifetime number_or_infinity ';'
		{
			route->AdvRouteLifetime = $2;
		}
		| T_RemoveRoute SWITCH ';'
		{
			route->RemoveRouteFlag = $2;
		}
		;

rdnssdef	: rdnsshead '{' optional_rdnssplist '}' ';'
		{
			$$ = rdnss;
			rdnss = NULL;
		}
		;

rdnssaddrs	: rdnssaddrs rdnssaddr
		| rdnssaddr
		;

rdnssaddr	: IPV6ADDR
		{
			if (!rdnss) {
				/* first IP found */
				rdnss = malloc(sizeof(struct AdvRDNSS));

				if (rdnss == NULL) {
					flog(LOG_CRIT, "malloc failed: %s", strerror(errno));
					ABORT;
				}

				rdnss_init_defaults(rdnss, iface);
			}

			switch (rdnss->AdvRDNSSNumber) {
				case 0:
					memcpy(&rdnss->AdvRDNSSAddr1, $1, sizeof(struct in6_addr));
					rdnss->AdvRDNSSNumber++;
					break;
				case 1:
					memcpy(&rdnss->AdvRDNSSAddr2, $1, sizeof(struct in6_addr));
					rdnss->AdvRDNSSNumber++;
					break;
				case 2:
					memcpy(&rdnss->AdvRDNSSAddr3, $1, sizeof(struct in6_addr));
					rdnss->AdvRDNSSNumber++;
					break;
				default:
					flog(LOG_CRIT, "Too many addresses in RDNSS section");
					ABORT;
			}

		}
		;

rdnsshead	: T_RDNSS rdnssaddrs
		{
			if (!rdnss) {
				flog(LOG_CRIT, "No address specified in RDNSS section");
				ABORT;
			}
		}
		;

optional_rdnssplist: /* empty */
		| rdnssplist
		;

rdnssplist	: rdnssplist rdnssparms
		| rdnssparms
		;


rdnssparms	: T_AdvRDNSSPreference NUMBER ';'
		{
			flog(LOG_WARNING, "Ignoring deprecated RDNSS preference.");
		}
		| T_AdvRDNSSOpenFlag SWITCH ';'
		{
			flog(LOG_WARNING, "Ignoring deprecated RDNSS open flag.");
		}
		| T_AdvRDNSSLifetime number_or_infinity ';'
		{
			if ($2 < iface->MaxRtrAdvInterval && $2 != 0) {
				flog(LOG_ERR, "AdvRDNSSLifetime must be at least MaxRtrAdvInterval");
				ABORT;
			}
			if ($2 > 2*(iface->MaxRtrAdvInterval))
				flog(LOG_WARNING, "Warning: AdvRDNSSLifetime <= 2*MaxRtrAdvInterval would allow stale DNS servers to be deleted faster");

			rdnss->AdvRDNSSLifetime = $2;
		}
		| T_FlushRDNSS SWITCH ';'
		{
			rdnss->FlushRDNSSFlag = $2;
		}
		;

dnssldef	: dnsslhead '{' optional_dnsslplist '}' ';'
		{
			$$ = dnssl;
			dnssl = NULL;
		}
		;

dnsslsuffixes	: dnsslsuffixes dnsslsuffix
		| dnsslsuffix
		;

dnsslsuffix	: STRING
		{
			char *ch;
			for (ch = $1;*ch != '\0';ch++) {
				if (*ch >= 'A' && *ch <= 'Z')
					continue;
				if (*ch >= 'a' && *ch <= 'z')
					continue;
				if (*ch >= '0' && *ch <= '9')
					continue;
				if (*ch == '-' || *ch == '.')
					continue;

				flog(LOG_CRIT, "Invalid domain suffix specified");
				ABORT;
			}

			if (!dnssl) {
				/* first domain found */
				dnssl = malloc(sizeof(struct AdvDNSSL));

				if (dnssl == NULL) {
					flog(LOG_CRIT, "malloc failed: %s", strerror(errno));
					ABORT;
				}

				dnssl_init_defaults(dnssl, iface);
			}

			dnssl->AdvDNSSLNumber++;
			dnssl->AdvDNSSLSuffixes =
				realloc(dnssl->AdvDNSSLSuffixes,
					dnssl->AdvDNSSLNumber * sizeof(char*));
			if (dnssl->AdvDNSSLSuffixes == NULL) {
				flog(LOG_CRIT, "realloc failed: %s", strerror(errno));
				ABORT;
			}

			dnssl->AdvDNSSLSuffixes[dnssl->AdvDNSSLNumber - 1] = strdup($1);
		}
		;

dnsslhead	: T_DNSSL dnsslsuffixes
		{
			if (!dnssl) {
				flog(LOG_CRIT, "No domain specified in DNSSL section");
				ABORT;
			}
		}
		;

optional_dnsslplist: /* empty */
		| dnsslplist
		;

dnsslplist	: dnsslplist dnsslparms
		| dnsslparms
		;


dnsslparms	: T_AdvDNSSLLifetime number_or_infinity ';'
		{
			if ($2 < iface->MaxRtrAdvInterval && $2 != 0) {
				flog(LOG_ERR, "AdvDNSSLLifetime must be at least MaxRtrAdvInterval");
				ABORT;
			}
			if ($2 > 2*(iface->MaxRtrAdvInterval))
				flog(LOG_WARNING, "Warning: AdvDNSSLLifetime <= 2*MaxRtrAdvInterval would allow stale DNS suffixes to be deleted faster");

			dnssl->AdvDNSSLLifetime = $2;
		}
		| T_FlushDNSSL SWITCH ';'
		{
			dnssl->FlushDNSSLFlag = $2;
		}
		;

number_or_infinity	: NUMBER
			{
				$$ = $1;
			}
			| INFINITY
			{
				$$ = (uint32_t)~0;
			}
			;

%%

static
int countbits(int b)
{
	int count;

	for (count = 0; b != 0; count++) {
		b &= b - 1; // this clears the LSB-most set bit
	}

	return (count);
}

static
int count_mask(struct sockaddr_in6 *m)
{
	struct in6_addr *in6 = &m->sin6_addr;
	int i;
	int count = 0;

	for (i = 0; i < 16; ++i) {
		count += countbits(in6->s6_addr[i]);
	}
	return count;
}

static
struct in6_addr get_prefix6(struct in6_addr const *addr, struct in6_addr const *mask)
{
	struct in6_addr prefix = *addr;
	int i = 0;

	for (; i < 16; ++i) {
		prefix.s6_addr[i] &= mask->s6_addr[i];
	}

	return prefix;
}

static
void cleanup(void)
{
	if (iface)
		free(iface);

	if (prefix)
		free(prefix);

	if (route)
		free(route);

	if (rdnss)
		free(rdnss);

	if (dnssl) {
		int i;
		for (i = 0;i < dnssl->AdvDNSSLNumber;i++)
			free(dnssl->AdvDNSSLSuffixes[i]);
		free(dnssl->AdvDNSSLSuffixes);
		free(dnssl);
	}
}

static void
yyerror(char *msg)
{
	cleanup();
	flog(LOG_ERR, "%s in %s, line %d: %s", msg, conf_file, num_lines, yytext);
}

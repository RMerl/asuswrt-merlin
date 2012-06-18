/*	$KAME: cfparse.y,v 1.36 2005/05/03 06:46:00 jinmei Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
%{
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdlib.h>
#include <string.h>

#include "dhcp6.h"
#include "config.h"
#include "common.h"

extern int lineno;
extern int cfdebug;

extern void yywarn __P((char *, ...))
	__attribute__((__format__(__printf__, 1, 2)));
extern void yyerror __P((char *, ...))
	__attribute__((__format__(__printf__, 1, 2)));

#define MAKE_NAMELIST(l, n, p) do { \
	(l) = (struct cf_namelist *)malloc(sizeof(*(l))); \
	if ((l) == NULL) { \
		yywarn("can't allocate memory"); \
		if (p) cleanup_cflist(p); \
		return (-1); \
	} \
	memset((l), 0, sizeof(*(l))); \
	l->line = lineno; \
	l->name = (n); \
	l->params = (p); \
	} while (0)

#define MAKE_CFLIST(l, t, pp, pl) do { \
	(l) = (struct cf_list *)malloc(sizeof(*(l))); \
	if ((l) == NULL) { \
		yywarn("can't allocate memory"); \
		if (pp) free(pp); \
		if (pl) cleanup_cflist(pl); \
		return (-1); \
	} \
	memset((l), 0, sizeof(*(l))); \
	l->line = lineno; \
	l->type = (t); \
	l->ptr = (pp); \
	l->list = (pl); \
	l->tail = (l); \
	} while (0)

static struct cf_namelist *iflist_head, *hostlist_head, *iapdlist_head;
static struct cf_namelist *addrpoollist_head;
static struct cf_namelist *authinfolist_head, *keylist_head;
static struct cf_namelist *ianalist_head;
struct cf_list *cf_dns_list, *cf_dns_name_list, *cf_ntp_list;
struct cf_list *cf_sip_list, *cf_sip_name_list;
struct cf_list *cf_nis_list, *cf_nis_name_list;
struct cf_list *cf_nisp_list, *cf_nisp_name_list;
struct cf_list *cf_bcmcs_list, *cf_bcmcs_name_list;
long long cf_refreshtime = -1;

extern int yylex __P((void));
extern int cfswitch_buffer __P((char *));
static int add_namelist __P((struct cf_namelist *, struct cf_namelist **));
static void cleanup __P((void));
static void cleanup_namelist __P((struct cf_namelist *));
static void cleanup_cflist __P((struct cf_list *));
%}

%token INTERFACE IFNAME
%token PREFIX_INTERFACE SLA_ID SLA_LEN DUID_ID
%token ID_ASSOC IA_PD IAID IA_NA
%token ADDRESS
%token REQUEST SEND ALLOW PREFERENCE
%token HOST HOSTNAME DUID
%token OPTION RAPID_COMMIT DNS_SERVERS DNS_NAME NTP_SERVERS REFRESHTIME
%token SIP_SERVERS SIP_NAME
%token NIS_SERVERS NIS_NAME
%token NISP_SERVERS NISP_NAME
%token BCMCS_SERVERS BCMCS_NAME
%token INFO_ONLY
%token SCRIPT DELAYEDKEY
%token AUTHENTICATION PROTOCOL ALGORITHM DELAYED RECONFIG HMACMD5 MONOCOUNTER
%token AUTHNAME RDM KEY
%token KEYINFO REALM KEYID SECRET KEYNAME EXPIRE
%token ADDRPOOL POOLNAME RANGE TO ADDRESS_POOL
%token INCLUDE

%token NUMBER SLASH EOS BCL ECL STRING QSTRING PREFIX INFINITY
%token COMMA

%union {
	long long num;
	char* str;
	struct cf_list *list;
	struct dhcp6_prefix *prefix;
	struct dhcp6_range *range;
	struct dhcp6_poolspec *pool;
}

%type <str> IFNAME HOSTNAME AUTHNAME KEYNAME DUID_ID STRING QSTRING IAID
%type <str> POOLNAME
%type <num> NUMBER duration authproto authalg authrdm
%type <list> declaration declarations dhcpoption ifparam ifparams
%type <list> address_list address_list_ent dhcpoption_list
%type <list> iapdconf_list iapdconf prefix_interface
%type <list> ianaconf_list ianaconf
%type <list> authparam_list authparam
%type <list> keyparam_list keyparam
%type <prefix> addressparam prefixparam
%type <range> rangeparam
%type <pool> poolparam

%%
statements:
		/* empty */
	|	statements statement
	;

statement:
		interface_statement
	|	host_statement
	|	option_statement
	|	ia_statement
	|	authentication_statement
	|	key_statement
	|	addrpool_statement
	|	include_statement
	;

interface_statement:
	INTERFACE IFNAME BCL declarations ECL EOS 
	{
		struct cf_namelist *ifl;

		MAKE_NAMELIST(ifl, $2, $4);

		if (add_namelist(ifl, &iflist_head))
			return (-1);
	}
	;

host_statement:
	HOST HOSTNAME BCL declarations ECL EOS
	{
		struct cf_namelist *host;

		MAKE_NAMELIST(host, $2, $4);

		if (add_namelist(host, &hostlist_head))
			return (-1);
	}
	;

option_statement:
		OPTION DNS_SERVERS address_list EOS
		{
			if (cf_dns_list == NULL)
				cf_dns_list = $3;
			else {
				cf_dns_list->tail->next = $3;
				cf_dns_list->tail = $3->tail;
			}
		}
	|	OPTION DNS_NAME QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, CFLISTENT_GENERIC, $3, NULL);

			if (cf_dns_name_list == NULL) {
				cf_dns_name_list = l;
				cf_dns_name_list->tail = l;
				cf_dns_name_list->next = NULL;
			} else {
				cf_dns_name_list->tail->next = l;
				cf_dns_name_list->tail = l->tail;
			}
		}
	|	OPTION NTP_SERVERS address_list EOS
		{
			if (cf_ntp_list == NULL)
				cf_ntp_list = $3;
			else {
				cf_ntp_list->tail->next = $3;
				cf_ntp_list->tail = $3->tail;
			}
		}
	|	OPTION SIP_SERVERS address_list EOS
		{
			if (cf_sip_list == NULL)
				cf_sip_list = $3;
			else {
				cf_sip_list->tail->next = $3;
				cf_sip_list->tail = $3->tail;
			}
		}
	|	OPTION SIP_NAME QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, CFLISTENT_GENERIC, $3, NULL);

			if (cf_sip_name_list == NULL) {
				cf_sip_name_list = l;
				cf_sip_name_list->tail = l;
				cf_sip_name_list->next = NULL;
			} else {
				cf_sip_name_list->tail->next = l;
				cf_sip_name_list->tail = l->tail;
			}
		}
	|	OPTION NIS_SERVERS address_list EOS
		{
			if (cf_nis_list == NULL)
				cf_nis_list = $3;
			else {
				cf_nis_list->tail->next = $3;
				cf_nis_list->tail = $3->tail;
			}
		}
	|	OPTION NIS_NAME QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, CFLISTENT_GENERIC, $3, NULL);

			if (cf_nis_name_list == NULL) {
				cf_nis_name_list = l;
				cf_nis_name_list->tail = l;
				cf_nis_name_list->next = NULL;
			} else {
				cf_nis_name_list->tail->next = l;
				cf_nis_name_list->tail = l->tail;
			}
		}
	|	OPTION NISP_SERVERS address_list EOS
		{
			if (cf_nisp_list == NULL)
				cf_nisp_list = $3;
			else {
				cf_nisp_list->tail->next = $3;
				cf_nisp_list->tail = $3->tail;
			}
		}
	|	OPTION NISP_NAME QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, CFLISTENT_GENERIC, $3, NULL);

			if (cf_nisp_name_list == NULL) {
				cf_nisp_name_list = l;
				cf_nisp_name_list->tail = l;
				cf_nisp_name_list->next = NULL;
			} else {
				cf_nisp_name_list->tail->next = l;
				cf_nisp_name_list->tail = l->tail;
			}
		}
	|	OPTION BCMCS_SERVERS address_list EOS
		{
			if (cf_bcmcs_list == NULL)
				cf_bcmcs_list = $3;
			else {
				cf_bcmcs_list->tail->next = $3;
				cf_bcmcs_list->tail = $3->tail;
			}
		}
	|	OPTION BCMCS_NAME QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, CFLISTENT_GENERIC, $3, NULL);

			if (cf_bcmcs_name_list == NULL) {
				cf_bcmcs_name_list = l;
				cf_bcmcs_name_list->tail = l;
				cf_bcmcs_name_list->next = NULL;
			} else {
				cf_bcmcs_name_list->tail->next = l;
				cf_bcmcs_name_list->tail = l->tail;
			}
		}
	|	OPTION REFRESHTIME NUMBER EOS
		{
			if (cf_refreshtime == -1) {
				cf_refreshtime = $3;
				if (cf_refreshtime < -1 ||
				    cf_refreshtime > 0xffffffff) {
					/*
					 * refresh time should not be negative
					 * according to the lex definition,
					 * but check it for safety.
					 */
					yyerror("refresh time is out of range");
				}
				if (cf_refreshtime < DHCP6_IRT_MINIMUM) {
					/*
					 * the value MUST NOT be smaller than
					 * IRT_MINIMUM.
					 */
					yyerror("refresh time is too small "
					    "(must not be smaller than %d)",
					    DHCP6_IRT_MINIMUM);
				}
			} else {
				yywarn("multiple refresh times (ignored)");
			}
		}
	;

ia_statement:
		ID_ASSOC IA_PD IAID BCL iapdconf_list ECL EOS
		{
			struct cf_namelist *iapd;

			MAKE_NAMELIST(iapd, $3, $5);

			if (add_namelist(iapd, &iapdlist_head))
				return (-1);
		}
	|	ID_ASSOC IA_PD BCL iapdconf_list ECL EOS
		{
			struct cf_namelist *iapd;
			char *zero;

			if ((zero = strdup("0")) == NULL) {
				yywarn("can't allocate memory");
				return (-1);
			}
			MAKE_NAMELIST(iapd, zero, $4);

			if (add_namelist(iapd, &iapdlist_head))
				return (-1);
		}
	|	ID_ASSOC IA_NA IAID BCL ianaconf_list ECL EOS
		{
			struct cf_namelist *iana;

			MAKE_NAMELIST(iana, $3, $5);

			if (add_namelist(iana, &ianalist_head))
				return (-1);
		}
	|	ID_ASSOC IA_NA BCL ianaconf_list ECL EOS
		{
			struct cf_namelist *iana;
			char *zero;

			if ((zero = strdup("0")) == NULL) {
				yywarn("can't allocate memory");
				return (-1);
			}
			MAKE_NAMELIST(iana, zero, $4);

			if (add_namelist(iana, &ianalist_head))
				return (-1);
		}
	;

authentication_statement:
	AUTHENTICATION AUTHNAME BCL authparam_list ECL EOS
	{
		struct cf_namelist *authinfo;

		MAKE_NAMELIST(authinfo, $2, $4);

		if (add_namelist(authinfo, &authinfolist_head))
			return (-1);
	}
	;

key_statement:
	KEYINFO KEYNAME BCL keyparam_list ECL EOS
	{
		struct cf_namelist *key;

		MAKE_NAMELIST(key, $2, $4);

		if (add_namelist(key, &keylist_head))
			return (-1);
	}
	;

include_statement:
	INCLUDE QSTRING EOS
	{
		if (cfswitch_buffer($2)) {
			free($2);
			return (-1);
		}
		free($2);
	}
	;

addrpool_statement:
	ADDRPOOL POOLNAME BCL declarations ECL EOS
	{
		struct cf_namelist *pool;

		MAKE_NAMELIST(pool, $2, $4);

		if (add_namelist(pool, &addrpoollist_head))
			return (-1);
	}
	;

address_list:
		{ $$ = NULL; }
	|	address_list address_list_ent
		{
			struct cf_list *head;

			if ((head = $1) == NULL) {
				$2->next = NULL;
				$2->tail = $2;
				head = $2;
			} else {
				head->tail->next = $2;
				head->tail = $2->tail;
			}

			$$ = head;
		}
	;

address_list_ent:
	STRING
	{
		struct cf_list *l;
		struct in6_addr a0, *a;

		if (inet_pton(AF_INET6, $1, &a0) != 1) {
			yywarn("invalid IPv6 address: %s", $1);
			free($1);
			return (-1);
		}
		if ((a = malloc(sizeof(*a))) == NULL) {
			yywarn("can't allocate memory");
			return (-1);
		}
		*a = a0;

		MAKE_CFLIST(l, CFLISTENT_GENERIC, a, NULL);

		$$ = l;
	}
	;

declarations:
		{ $$ = NULL; }
	|	declarations declaration
		{
			struct cf_list *head;

			if ((head = $1) == NULL) {
				$2->next = NULL;
				$2->tail = $2;
				head = $2;
			} else {
				head->tail->next = $2;
				head->tail = $2->tail;
			}

			$$ = head;
		}
	;
	
declaration:
		SEND dhcpoption_list EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_SEND, NULL, $2);

			$$ = l;
		}
	|	REQUEST dhcpoption_list EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_REQUEST, NULL, $2);

			$$ = l;
		}
	|	INFO_ONLY EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_INFO_ONLY, NULL, NULL);
			/* no value */
			$$ = l;
		}
	|	ALLOW dhcpoption EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_ALLOW, NULL, $2);

			$$ = l;
		}
	|	DUID DUID_ID EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_DUID, $2, NULL);

			$$ = l;
		}
	|	ADDRESS addressparam EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_ADDRESS, $2,NULL);

			$$ = l;
		}
	|	PREFIX prefixparam EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_PREFIX, $2, NULL);

			$$ = l;
		}
	|	PREFERENCE NUMBER EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_PREFERENCE, NULL, NULL);
			l->num = $2;

			$$ = l;
		}
	|	SCRIPT QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_SCRIPT, $2, NULL);

			$$ = l;
		}
	|	DELAYEDKEY STRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_DELAYEDKEY, $2, NULL);

			$$ = l;
		}
	|	RANGE rangeparam EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_RANGE, $2, NULL);

			$$ = l;
		}
	|	ADDRESS_POOL poolparam EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DECL_ADDRESSPOOL, $2, NULL);

			$$ = l;
		}
	;

dhcpoption_list:
		dhcpoption
		{
			$$ = $1;
		}
	|	dhcpoption COMMA dhcpoption_list
		{
			$1->next = $3;
			$1->tail = $3->tail;

			$$ = $1;
		}
	;

dhcpoption:
		RAPID_COMMIT
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_RAPID_COMMIT, NULL, NULL);
			/* no value */
			$$ = l;
		}
	|	AUTHENTICATION AUTHNAME
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_AUTHINFO, NULL, NULL);
			l->ptr = $2;
			$$ = l;
		}
	|	IA_PD NUMBER
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_IA_PD, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	|       IA_NA NUMBER
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_IA_NA, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	|	SIP_SERVERS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_SIP, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	SIP_NAME
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_SIPNAME, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	DNS_SERVERS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_DNS, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	DNS_NAME
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_DNSNAME, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	NTP_SERVERS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_NTP, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	REFRESHTIME
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_REFRESHTIME, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	NIS_SERVERS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_NIS, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	NIS_NAME
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_NISNAME, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	NISP_SERVERS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_NISP, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	NISP_NAME
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_NISPNAME, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	BCMCS_SERVERS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_BCMCS, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	|	BCMCS_NAME
		{
			struct cf_list *l;

			MAKE_CFLIST(l, DHCPOPT_BCMCSNAME, NULL, NULL);
			/* currently no value */
			$$ = l;
		}
	;

rangeparam:
		STRING TO STRING
		{
			struct dhcp6_range range0, *range;		

			memset(&range0, 0, sizeof(range0));
			if (inet_pton(AF_INET6, $1, &range0.min) != 1) {
				yywarn("invalid IPv6 address: %s", $1);
				free($1);
				free($3);
				return (-1);
			}
			if (inet_pton(AF_INET6, $3, &range0.max) != 1) {
				yywarn("invalid IPv6 address: %s", $3);
				free($1);
				free($3);
				return (-1);
			}
			free($1);
			free($3);

			if ((range = malloc(sizeof(*range))) == NULL) {
				yywarn("can't allocate memory");
				return (-1);
			}
			*range = range0;

			$$ = range;
		}
	;

addressparam:
		STRING duration
		{
			struct dhcp6_prefix pconf0, *pconf;		

			memset(&pconf0, 0, sizeof(pconf0));
			if (inet_pton(AF_INET6, $1, &pconf0.addr) != 1) {
				yywarn("invalid IPv6 address: %s", $1);
				free($1);
				return (-1);
			}
			free($1);
			/* validate other parameters later */
			pconf0.plen = 128; /* XXX this field is ignored */
			if ($2 < 0)
				pconf0.pltime = DHCP6_DURATION_INFINITE;
			else
				pconf0.pltime = (u_int32_t)$2;
			pconf0.vltime = pconf0.pltime;

			if ((pconf = malloc(sizeof(*pconf))) == NULL) {
				yywarn("can't allocate memory");
				return (-1);
			}
			*pconf = pconf0;

			$$ = pconf;
		}
	|	STRING duration duration
		{
			struct dhcp6_prefix pconf0, *pconf;		

			memset(&pconf0, 0, sizeof(pconf0));
			if (inet_pton(AF_INET6, $1, &pconf0.addr) != 1) {
				yywarn("invalid IPv6 address: %s", $1);
				free($1);
				return (-1);
			}
			free($1);
			/* validate other parameters later */
			pconf0.plen = 128; /* XXX */
			if ($2 < 0)
				pconf0.pltime = DHCP6_DURATION_INFINITE;
			else
				pconf0.pltime = (u_int32_t)$2;
			if ($3 < 0)
				pconf0.vltime = DHCP6_DURATION_INFINITE;
			else
				pconf0.vltime = (u_int32_t)$3;

			if ((pconf = malloc(sizeof(*pconf))) == NULL) {
				yywarn("can't allocate memory");
				return (-1);
			}
			*pconf = pconf0;

			$$ = pconf;
		}
	;

prefixparam:
		STRING SLASH NUMBER duration
		{
			struct dhcp6_prefix pconf0, *pconf;		

			memset(&pconf0, 0, sizeof(pconf0));
			if (inet_pton(AF_INET6, $1, &pconf0.addr) != 1) {
				yywarn("invalid IPv6 address: %s", $1);
				free($1);
				return (-1);
			}
			free($1);
			/* validate other parameters later */
			pconf0.plen = $3;
			if ($4 < 0)
				pconf0.pltime = DHCP6_DURATION_INFINITE;
			else
				pconf0.pltime = (u_int32_t)$4;
			pconf0.vltime = pconf0.pltime;

			if ((pconf = malloc(sizeof(*pconf))) == NULL) {
				yywarn("can't allocate memory");
				return (-1);
			}
			*pconf = pconf0;

			$$ = pconf;
		}
	|	STRING SLASH NUMBER duration duration
		{
			struct dhcp6_prefix pconf0, *pconf;		

			memset(&pconf0, 0, sizeof(pconf0));
			if (inet_pton(AF_INET6, $1, &pconf0.addr) != 1) {
				yywarn("invalid IPv6 address: %s", $1);
				free($1);
				return (-1);
			}
			free($1);
			/* validate other parameters later */
			pconf0.plen = $3;
			if ($4 < 0)
				pconf0.pltime = DHCP6_DURATION_INFINITE;
			else
				pconf0.pltime = (u_int32_t)$4;
			if ($5 < 0)
				pconf0.vltime = DHCP6_DURATION_INFINITE;
			else
				pconf0.vltime = (u_int32_t)$5;

			if ((pconf = malloc(sizeof(*pconf))) == NULL) {
				yywarn("can't allocate memory");
				return (-1);
			}
			*pconf = pconf0;

			$$ = pconf;
		}
	;

poolparam:
		STRING duration
		{
			struct dhcp6_poolspec* pool;		

			if ((pool = malloc(sizeof(*pool))) == NULL) {
				yywarn("can't allocate memory");
				free($1);
				return (-1);
			}
			if ((pool->name = strdup($1)) == NULL) {
				yywarn("can't allocate memory");
				free($1);
				return (-1);
			}
			free($1);

			/* validate other parameters later */
			if ($2 < 0)
				pool->pltime = DHCP6_DURATION_INFINITE;
			else
				pool->pltime = (u_int32_t)$2;
			pool->vltime = pool->pltime;

			$$ = pool;
		}
	|	STRING duration duration
		{
			struct dhcp6_poolspec* pool;		

			if ((pool = malloc(sizeof(*pool))) == NULL) {
				yywarn("can't allocate memory");
				free($1);
				return (-1);
			}
			if ((pool->name = strdup($1)) == NULL) {
				yywarn("can't allocate memory");
				free($1);
				return (-1);
			}
			free($1);

			/* validate other parameters later */
			if ($2 < 0)
				pool->pltime = DHCP6_DURATION_INFINITE;
			else
				pool->pltime = (u_int32_t)$2;
			if ($3 < 0)
				pool->vltime = DHCP6_DURATION_INFINITE;
			else
				pool->vltime = (u_int32_t)$3;

			$$ = pool;
		}
	;

duration:
		INFINITY
		{
			$$ = -1;
		}
	|	NUMBER
		{
			$$ = $1;
		}
	;

iapdconf_list:
		{ $$ = NULL; }
	|	iapdconf_list iapdconf
		{
			struct cf_list *head;

			if ((head = $1) == NULL) {
				$2->next = NULL;
				$2->tail = $2;
				head = $2;
			} else {
				head->tail->next = $2;
				head->tail = $2->tail;
			}

			$$ = head;
		}
	;

iapdconf:
		prefix_interface { $$ = $1; }
	|	PREFIX prefixparam EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, IACONF_PREFIX, $2, NULL);

			$$ = l;
		}
	;

prefix_interface:
	PREFIX_INTERFACE IFNAME BCL ifparams ECL EOS
	{
		struct cf_list *ifl;

		MAKE_CFLIST(ifl, IACONF_PIF, $2, $4);
		$$ = ifl;
	}
	;

ifparams:
		{ $$ = NULL; }
	|	ifparams ifparam
		{
			struct cf_list *head;

			if ((head = $1) == NULL) {
				$2->next = NULL;
				$2->tail = $2;
				head = $2;
			} else {
				head->tail->next = $2;
				head->tail = $2->tail;
			}

			$$ = head;
		}
	;

ifparam:
		SLA_ID NUMBER EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, IFPARAM_SLA_ID, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	|	SLA_LEN NUMBER EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, IFPARAM_SLA_LEN, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	;

ianaconf_list:
		{ $$ = NULL; }
	|	ianaconf_list ianaconf
		{
			struct cf_list *head;

			if ((head = $1) == NULL) {
				$2->next = NULL;
				$2->tail = $2;
				head = $2;
			} else {
				head->tail->next = $2;
				head->tail = $2->tail;
			}

			$$ = head;
		}
	;

ianaconf:
	 	ADDRESS addressparam EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, IACONF_ADDR, $2, NULL);

			$$ = l;
		}
	;

authparam_list:
		{ $$ = NULL; }
	|	authparam_list authparam
		{
			struct cf_list *head;

			if ((head = $1) == NULL) {
				$2->next = NULL;
				$2->tail = $2;
				head = $2;
			} else {
				head->tail->next = $2;
				head->tail = $2->tail;
			}

			$$ = head;
		}
	;

authparam:
		PROTOCOL authproto EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, AUTHPARAM_PROTO, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	|	ALGORITHM authalg EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, AUTHPARAM_ALG, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	|	RDM authrdm EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, AUTHPARAM_RDM, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	|	KEY STRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, AUTHPARAM_KEY, NULL, NULL);
			l->ptr = $2;
			$$ = l;
		}
	;

authproto:
		DELAYED { $$ = DHCP6_AUTHPROTO_DELAYED; }
	|	RECONFIG { $$ = DHCP6_AUTHPROTO_RECONFIG; }
	;

authalg:
	HMACMD5 { $$ = DHCP6_AUTHALG_HMACMD5; }
	;

authrdm:
	MONOCOUNTER { $$ = DHCP6_AUTHRDM_MONOCOUNTER; }
	;

keyparam_list:
		{ $$ = NULL; }
	|	keyparam_list keyparam
		{
			struct cf_list *head;

			if ((head = $1) == NULL) {
				$2->next = NULL;
				$2->tail = $2;
				head = $2;
			} else {
				head->tail->next = $2;
				head->tail = $2->tail;
			}

			$$ = head;
		}
	;

keyparam:
		REALM QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, KEYPARAM_REALM, NULL, NULL);
			l->ptr = $2;
			$$ = l;
		}
	|	KEYID NUMBER EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, KEYPARAM_KEYID, NULL, NULL);
			l->num = $2;
			$$ = l;
		}
	|	SECRET QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, KEYPARAM_SECRET, NULL, NULL);
			l->ptr = $2;
			$$ = l;
		}
	|	EXPIRE QSTRING EOS
		{
			struct cf_list *l;

			MAKE_CFLIST(l, KEYPARAM_EXPIRE, NULL, NULL);
			l->ptr = $2;
			$$ = l;
		}
	;

%%
/* supplement routines for configuration */
static int
add_namelist(new, headp)
	struct cf_namelist *new, **headp;
{
	struct cf_namelist *n;
	
	/* check for duplicated configuration */
	for (n = *headp; n; n = n->next) {
		if (strcmp(n->name, new->name) == 0) {
			yywarn("duplicated name: %s (ignored)",
			       new->name);
			cleanup_namelist(new);
			return (0);
		}
	}

	new->next = *headp;
	*headp = new;

	return (0);
}

/* free temporary resources */
static void
cleanup()
{
	cleanup_namelist(iflist_head);
	iflist_head = NULL;
	cleanup_namelist(hostlist_head);
	hostlist_head = NULL;
	cleanup_namelist(iapdlist_head);
	iapdlist_head = NULL;
	cleanup_namelist(ianalist_head);
	ianalist_head = NULL;
	cleanup_namelist(authinfolist_head);
	authinfolist_head = NULL;
	cleanup_namelist(keylist_head);
	keylist_head = NULL;
	cleanup_namelist(addrpoollist_head);
	addrpoollist_head = NULL;

	cleanup_cflist(cf_sip_list);
	cf_sip_list = NULL;
	cleanup_cflist(cf_sip_name_list);
	cf_sip_name_list = NULL;
	cleanup_cflist(cf_dns_list);
	cf_dns_list = NULL;
	cleanup_cflist(cf_dns_name_list);
	cf_dns_name_list = NULL;
	cleanup_cflist(cf_ntp_list);
	cf_ntp_list = NULL;
	cleanup_cflist(cf_nis_list);
	cf_nis_list = NULL;
	cleanup_cflist(cf_nis_name_list);
	cf_nis_name_list = NULL;
	cleanup_cflist(cf_nisp_list);
	cf_nisp_list = NULL;
	cleanup_cflist(cf_nisp_name_list);
	cf_nisp_name_list = NULL;
	cleanup_cflist(cf_bcmcs_list);
	cf_bcmcs_list = NULL;
	cleanup_cflist(cf_bcmcs_name_list);
	cf_bcmcs_name_list = NULL;
}

static void
cleanup_namelist(head)
	struct cf_namelist *head;
{
	struct cf_namelist *ifp, *ifp_next;

	for (ifp = head; ifp; ifp = ifp_next) {
		ifp_next = ifp->next;
		cleanup_cflist(ifp->params);
		free(ifp->name);
		free(ifp);
	}
}

static void
cleanup_cflist(p)
	struct cf_list *p;
{
	struct cf_list *n;

	if (p == NULL)
		return;

	n = p->next;
	if (p->type == DECL_ADDRESSPOOL) {
		free(((struct dhcp6_poolspec *)p->ptr)->name);
	}
	if (p->ptr)
		free(p->ptr);
	if (p->list)
		cleanup_cflist(p->list);
	free(p);

	cleanup_cflist(n);
}

#define config_fail() \
	do { cleanup(); configure_cleanup(); return (-1); } while(0)

int
cf_post_config()
{
	if (configure_keys(keylist_head))
		config_fail();

	if (configure_authinfo(authinfolist_head))
		config_fail();

	if (configure_ia(iapdlist_head, IATYPE_PD))
		config_fail();

	if (configure_ia(ianalist_head, IATYPE_NA))
		config_fail();

	if (configure_pool(addrpoollist_head))
		config_fail();

	if (configure_interface(iflist_head))
		config_fail();

	if (configure_host(hostlist_head))
		config_fail();

	if (configure_global_option())
		config_fail();

	configure_commit();
	cleanup();
	return (0);
}
#undef config_fail

void
cf_init()
{
	iflist_head = NULL;
}

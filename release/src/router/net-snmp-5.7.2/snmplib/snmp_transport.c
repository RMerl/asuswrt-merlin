#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmp_transport.h>

#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <ctype.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/output_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/default_store.h>

#include <net-snmp/library/snmpUDPDomain.h>
#ifdef NETSNMP_TRANSPORT_TLSBASE_DOMAIN
#include <net-snmp/library/snmpTLSBaseDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_TLSTCP_DOMAIN
#include <net-snmp/library/snmpTLSTCPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_STD_DOMAIN
#include <net-snmp/library/snmpSTDDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_TCP_DOMAIN
#include <net-snmp/library/snmpTCPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_DTLSUDP_DOMAIN
#include <net-snmp/library/snmpDTLSUDPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_SSH_DOMAIN
#include <net-snmp/library/snmpSSHDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_ALIAS_DOMAIN
#include <net-snmp/library/snmpAliasDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_IPX_DOMAIN
#include <net-snmp/library/snmpIPXDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
#include <net-snmp/library/snmpUnixDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_AAL5PVC_DOMAIN
#include <net-snmp/library/snmpAAL5PVCDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN
#include <net-snmp/library/snmpUDPIPv6Domain.h>
#endif
#ifdef NETSNMP_TRANSPORT_TCPIPV6_DOMAIN
#include <net-snmp/library/snmpTCPIPv6Domain.h>
#endif
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_service.h>
#include <net-snmp/library/read_config.h>

netsnmp_feature_child_of(transport_all, libnetsnmp)

netsnmp_feature_child_of(tdomain_support, transport_all)
netsnmp_feature_child_of(tdomain_transport_oid, transport_all)
netsnmp_feature_child_of(sockaddr_size, transport_all)

/*
 * Our list of supported transport domains.  
 */

static netsnmp_tdomain *domain_list = NULL;



/*
 * The standard SNMP domains.  
 */

oid             netsnmpUDPDomain[] = { 1, 3, 6, 1, 6, 1, 1 };
size_t          netsnmpUDPDomain_len = OID_LENGTH(netsnmpUDPDomain);
oid             netsnmpCLNSDomain[] = { 1, 3, 6, 1, 6, 1, 2 };
size_t          netsnmpCLNSDomain_len = OID_LENGTH(netsnmpCLNSDomain);
oid             netsnmpCONSDomain[] = { 1, 3, 6, 1, 6, 1, 3 };
size_t          netsnmpCONSDomain_len = OID_LENGTH(netsnmpCONSDomain);
oid             netsnmpDDPDomain[] = { 1, 3, 6, 1, 6, 1, 4 };
size_t          netsnmpDDPDomain_len = OID_LENGTH(netsnmpDDPDomain);
oid             netsnmpIPXDomain[] = { 1, 3, 6, 1, 6, 1, 5 };
size_t          netsnmpIPXDomain_len = OID_LENGTH(netsnmpIPXDomain);



static void     netsnmp_tdomain_dump(void);



void
init_snmp_transport(void)
{
    netsnmp_ds_register_config(ASN_BOOLEAN,
                               "snmp", "dontLoadHostConfig",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DONT_LOAD_HOST_FILES);
}

/*
 * Make a deep copy of an netsnmp_transport.  
 */
netsnmp_transport *
netsnmp_transport_copy(netsnmp_transport *t)
{
    netsnmp_transport *n = NULL;

    if (t == NULL) {
        return NULL;
    }

    n = SNMP_MALLOC_TYPEDEF(netsnmp_transport);
    if (n == NULL) {
        return NULL;
    }

    if (t->domain != NULL) {
        n->domain = t->domain;
        n->domain_length = t->domain_length;
    } else {
        n->domain = NULL;
        n->domain_length = 0;
    }

    if (t->local != NULL) {
        n->local = (u_char *) malloc(t->local_length);
        if (n->local == NULL) {
            netsnmp_transport_free(n);
            return NULL;
        }
        n->local_length = t->local_length;
        memcpy(n->local, t->local, t->local_length);
    } else {
        n->local = NULL;
        n->local_length = 0;
    }

    if (t->remote != NULL) {
        n->remote = (u_char *) malloc(t->remote_length);
        if (n->remote == NULL) {
            netsnmp_transport_free(n);
            return NULL;
        }
        n->remote_length = t->remote_length;
        memcpy(n->remote, t->remote, t->remote_length);
    } else {
        n->remote = NULL;
        n->remote_length = 0;
    }

    if (t->data != NULL && t->data_length > 0) {
        n->data = malloc(t->data_length);
        if (n->data == NULL) {
            netsnmp_transport_free(n);
            return NULL;
        }
        n->data_length = t->data_length;
        memcpy(n->data, t->data, t->data_length);
    } else {
        n->data = NULL;
        n->data_length = 0;
    }

    n->msgMaxSize = t->msgMaxSize;
    n->f_accept = t->f_accept;
    n->f_recv = t->f_recv;
    n->f_send = t->f_send;
    n->f_close = t->f_close;
    n->f_copy = t->f_copy;
    n->f_config = t->f_config;
    n->f_fmtaddr = t->f_fmtaddr;
    n->sock = t->sock;
    n->flags = t->flags;
    n->base_transport = netsnmp_transport_copy(t->base_transport);

    /* give the transport a chance to do "special things" */
    if (t->f_copy)
        t->f_copy(t, n);
                
    return n;
}



void
netsnmp_transport_free(netsnmp_transport *t)
{
    if (NULL == t)
        return;

    SNMP_FREE(t->local);
    SNMP_FREE(t->remote);
    SNMP_FREE(t->data);
    netsnmp_transport_free(t->base_transport);

    SNMP_FREE(t);
}

/*
 * netsnmp_transport_peer_string
 *
 * returns string representation of peer address.
 *
 * caller is responsible for freeing the allocated string.
 */
char *
netsnmp_transport_peer_string(netsnmp_transport *t, void *data, int len)
{
    char           *str;

    if (NULL == t)
        return NULL;

    if (t->f_fmtaddr != NULL)
        str = t->f_fmtaddr(t, data, len);
    else
        str = strdup("<UNKNOWN>");

    return str;
}

#ifndef NETSNMP_FEATURE_REMOVE_SOCKADDR_SIZE
int
netsnmp_sockaddr_size(struct sockaddr *sa)
{
    if (NULL == sa)
        return 0;

    switch (sa->sa_family) {
        case AF_INET:
            return sizeof(struct sockaddr_in);
        break;
#ifdef NETSNMP_ENABLE_IPV6
        case AF_INET6:
            return sizeof(struct sockaddr_in6);
            break;
#endif
    }

    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_SOCKADDR_SIZE */
    
int
netsnmp_transport_send(netsnmp_transport *t, void *packet, int length,
                       void **opaque, int *olength)
{
    int dumpPacket, debugLength;

    if ((NULL == t) || (NULL == t->f_send)) {
        DEBUGMSGTL(("transport:pkt:send", "NULL transport or send function\n"));
        return SNMPERR_GENERR;
    }

    dumpPacket = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                        NETSNMP_DS_LIB_DUMP_PACKET);
    debugLength = (SNMPERR_SUCCESS ==
                   debug_is_token_registered("transport:send"));

    if (dumpPacket | debugLength) {
        char *str = netsnmp_transport_peer_string(t,
                                                  opaque ? *opaque : NULL,
                                                  olength ? *olength : 0);
        if (debugLength)
            DEBUGMSGT_NC(("transport:send","%lu bytes to %s\n",
                          (unsigned long)length, str));
        if (dumpPacket)
            snmp_log(LOG_DEBUG, "\nSending %lu bytes to %s\n", 
                     (unsigned long)length, str);
        SNMP_FREE(str);
    }
    if (dumpPacket)
        xdump(packet, length, "");

    return t->f_send(t, packet, length, opaque, olength);
}

int
netsnmp_transport_recv(netsnmp_transport *t, void *packet, int length,
                       void **opaque, int *olength)
{
    int debugLength;

    if ((NULL == t) || (NULL == t->f_recv)) {
        DEBUGMSGTL(("transport:recv", "NULL transport or recv function\n"));
        return SNMPERR_GENERR;
    }

    length = t->f_recv(t, packet, length, opaque, olength);

    if (length <=0)
        return length; /* don't log timeouts/socket closed */

    debugLength = (SNMPERR_SUCCESS ==
                   debug_is_token_registered("transport:recv"));

    if (debugLength) {
        char *str = netsnmp_transport_peer_string(t,
                                                  opaque ? *opaque : NULL,
                                                  olength ? *olength : 0);
        if (debugLength)
            DEBUGMSGT_NC(("transport:recv","%d bytes from %s\n",
                          length, str));
        SNMP_FREE(str);
    }

    return length;
}



#ifndef NETSNMP_FEATURE_REMOVE_TDOMAIN_SUPPORT
int
netsnmp_tdomain_support(const oid * in_oid,
                        size_t in_len,
                        const oid ** out_oid, size_t * out_len)
{
    netsnmp_tdomain *d = NULL;

    for (d = domain_list; d != NULL; d = d->next) {
        if (netsnmp_oid_equals(in_oid, in_len, d->name, d->name_length) == 0) {
            if (out_oid != NULL && out_len != NULL) {
                *out_oid = d->name;
                *out_len = d->name_length;
            }
            return 1;
        }
    }
    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_TDOMAIN_SUPPORT */


void
netsnmp_tdomain_init(void)
{
    DEBUGMSGTL(("tdomain", "netsnmp_tdomain_init() called\n"));

/* include the configure generated list of constructor calls */
#include "transports/snmp_transport_inits.h"

    netsnmp_tdomain_dump();
}

void
netsnmp_clear_tdomain_list(void)
{
    netsnmp_tdomain *list = domain_list, *next = NULL;
    DEBUGMSGTL(("tdomain", "clear_tdomain_list() called\n"));

    while (list != NULL) {
	next = list->next;
	SNMP_FREE(list->prefix);
        /* attention!! list itself is not in the heap, so we must not free it! */
	list = next;
    }
    domain_list = NULL;
}


static void
netsnmp_tdomain_dump(void)
{
    netsnmp_tdomain *d;
    int i = 0;

    DEBUGMSGTL(("tdomain", "domain_list -> "));
    for (d = domain_list; d != NULL; d = d->next) {
        DEBUGMSG(("tdomain", "{ "));
        DEBUGMSGOID(("tdomain", d->name, d->name_length));
        DEBUGMSG(("tdomain", ", \""));
        for (i = 0; d->prefix[i] != NULL; i++) {
            DEBUGMSG(("tdomain", "%s%s", d->prefix[i],
		      (d->prefix[i + 1]) ? "/" : ""));
        }
        DEBUGMSG(("tdomain", "\" } -> "));
    }
    DEBUGMSG(("tdomain", "[NIL]\n"));
}



int
netsnmp_tdomain_register(netsnmp_tdomain *n)
{
    netsnmp_tdomain **prevNext = &domain_list, *d;

    if (n != NULL) {
        for (d = domain_list; d != NULL; d = d->next) {
            if (netsnmp_oid_equals(n->name, n->name_length,
                                d->name, d->name_length) == 0) {
                /*
                 * Already registered.  
                 */
                return 0;
            }
            prevNext = &(d->next);
        }
        n->next = NULL;
        *prevNext = n;
        return 1;
    } else {
        return 0;
    }
}



netsnmp_feature_child_of(tdomain_unregister, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_TDOMAIN_UNREGISTER
int
netsnmp_tdomain_unregister(netsnmp_tdomain *n)
{
    netsnmp_tdomain **prevNext = &domain_list, *d;

    if (n != NULL) {
        for (d = domain_list; d != NULL; d = d->next) {
            if (netsnmp_oid_equals(n->name, n->name_length,
                                d->name, d->name_length) == 0) {
                *prevNext = n->next;
		SNMP_FREE(n->prefix);
                return 1;
            }
            prevNext = &(d->next);
        }
        return 0;
    } else {
        return 0;
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_TDOMAIN_UNREGISTER */


static netsnmp_tdomain *
find_tdomain(const char* spec)
{
    netsnmp_tdomain *d;
    for (d = domain_list; d != NULL; d = d->next) {
        int i;
        for (i = 0; d->prefix[i] != NULL; i++)
            if (strcasecmp(d->prefix[i], spec) == 0) {
                DEBUGMSGTL(("tdomain",
                            "Found domain \"%s\" from specifier \"%s\"\n",
                            d->prefix[0], spec));
                return d;
            }
    }
    DEBUGMSGTL(("tdomain", "Found no domain from specifier \"%s\"\n", spec));
    return NULL;
}

static int
netsnmp_is_fqdn(const char *thename)
{
    if (!thename)
        return 0;
    while(*thename) {
        if (*thename != '.' && !isupper((unsigned char)*thename) &&
            !islower((unsigned char)*thename) &&
            !isdigit((unsigned char)*thename) && *thename != '-') {
            return 0;
        }
        thename++;
    }
    return 1;
}

/*
 * Locate the appropriate transport domain and call the create function for
 * it.
 */
netsnmp_transport *
netsnmp_tdomain_transport_full(const char *application,
                               const char *str, int local,
                               const char *default_domain,
                               const char *default_target)
{
    netsnmp_tdomain    *match = NULL;
    const char         *addr = NULL;
    const char * const *spec = NULL;
    int                 any_found = 0;
    char buf[SNMP_MAXPATH];

    DEBUGMSGTL(("tdomain",
                "tdomain_transport_full(\"%s\", \"%s\", %d, \"%s\", \"%s\")\n",
                application, str ? str : "[NIL]", local,
                default_domain ? default_domain : "[NIL]",
                default_target ? default_target : "[NIL]"));

    /* see if we can load a host-name specific set of conf files */
    if (!netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                NETSNMP_DS_LIB_DONT_LOAD_HOST_FILES) &&
        netsnmp_is_fqdn(str)) {
        static int have_added_handler = 0;
        char *newhost;
        struct config_line *config_handlers;
        struct config_files file_names;
        char *prev_hostname;

        /* register a "transport" specifier */
        if (!have_added_handler) {
            have_added_handler = 1;
            netsnmp_ds_register_config(ASN_OCTET_STR,
                                       "snmp", "transport",
                                       NETSNMP_DS_LIBRARY_ID,
                                       NETSNMP_DS_LIB_HOSTNAME);
        }

        /* we save on specific setting that we don't allow to change
           from one transport creation to the next; ie, we don't want
           the "transport" specifier to be a default.  It should be a
           single invocation use only */
        prev_hostname = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                              NETSNMP_DS_LIB_HOSTNAME);
        if (prev_hostname)
            prev_hostname = strdup(prev_hostname);

        /* read in the hosts/STRING.conf files */
        config_handlers = read_config_get_handlers("snmp");
        snprintf(buf, sizeof(buf)-1, "hosts/%s", str);
        file_names.fileHeader = buf;
        file_names.start = config_handlers;
        file_names.next = NULL;
        DEBUGMSGTL(("tdomain", "checking for host specific config %s\n",
                    buf));
        read_config_files_of_type(EITHER_CONFIG, &file_names);

        if (NULL !=
            (newhost = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                             NETSNMP_DS_LIB_HOSTNAME))) {
            strlcpy(buf, newhost, sizeof(buf));
            str = buf;
        }

        netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                              NETSNMP_DS_LIB_HOSTNAME,
                              prev_hostname);
        SNMP_FREE(prev_hostname);
    }

    /* First try - assume that there is a domain in str (domain:target) */

    if (str != NULL) {
        const char *cp;
        if ((cp = strchr(str, ':')) != NULL) {
            char* mystring = (char*)malloc(cp + 1 - str);
            memcpy(mystring, str, cp - str);
            mystring[cp - str] = '\0';
            addr = cp + 1;

            match = find_tdomain(mystring);
            free(mystring);
        }
    }

    /*
     * Second try, if there is no domain in str (target), then try the
     * default domain
     */

    if (match == NULL) {
        addr = str;
        if (addr && *addr == '/') {
            DEBUGMSGTL(("tdomain",
                        "Address starts with '/', so assume \"unix\" "
                        "domain\n"));
            match = find_tdomain("unix");
        } else if (default_domain) {
            DEBUGMSGTL(("tdomain",
                        "Use user specified default domain \"%s\"\n",
                        default_domain));
            match = find_tdomain(default_domain);
        } else {
            spec = netsnmp_lookup_default_domains(application);
            if (spec == NULL) {
                DEBUGMSGTL(("tdomain",
                            "No default domain found, assume \"udp\"\n"));
                match = find_tdomain("udp");
            } else {
                const char * const * r = spec;
                DEBUGMSGTL(("tdomain",
                            "Use application default domains"));
                while(*r) {
                    DEBUGMSG(("tdomain", " \"%s\"", *r));
                    ++r;
                }
                DEBUGMSG(("tdomain", "\n"));
            }
        }
    }

    for(;;) {
        if (match) {
            netsnmp_transport *t = NULL;
            const char* addr2;

            any_found = 1;
            /*
             * Ok, we know what domain to try, lets see what default data
             * should be used with it
             */
            if (default_target != NULL)
                addr2 = default_target;
            else
                addr2 = netsnmp_lookup_default_target(application,
                                                      match->prefix[0]);
            DEBUGMSGTL(("tdomain",
                        "trying domain \"%s\" address \"%s\" "
                        "default address \"%s\"\n",
                        match->prefix[0], addr ? addr : "[NIL]",
                        addr2 ? addr2 : "[NIL]"));
            if (match->f_create_from_tstring) {
                NETSNMP_LOGONCE((LOG_WARNING,
                                 "transport domain %s uses deprecated f_create_from_tstring\n",
                                 match->prefix[0]));
                t = match->f_create_from_tstring(addr, local);
            }
            else
                t = match->f_create_from_tstring_new(addr, local, addr2);
            if (t) {
                return t;
            }
        }
        addr = str;
        if (spec && *spec)
            match = find_tdomain(*spec++);
        else
            break;
    }
    if (!any_found)
        snmp_log(LOG_ERR, "No support for any checked transport domain\n");
    return NULL;
}


netsnmp_transport *
netsnmp_tdomain_transport(const char *str, int local,
			  const char *default_domain)
{
    return netsnmp_tdomain_transport_full("snmp", str, local, default_domain,
					  NULL);
}


#ifndef NETSNMP_FEATURE_REMOVE_TDOMAIN_TRANSPORT_OID
netsnmp_transport *
netsnmp_tdomain_transport_oid(const oid * dom,
                              size_t dom_len,
                              const u_char * o, size_t o_len, int local)
{
    netsnmp_tdomain *d;
    int             i;

    DEBUGMSGTL(("tdomain", "domain \""));
    DEBUGMSGOID(("tdomain", dom, dom_len));
    DEBUGMSG(("tdomain", "\"\n"));

    for (d = domain_list; d != NULL; d = d->next) {
        for (i = 0; d->prefix[i] != NULL; i++) {
            if (netsnmp_oid_equals(dom, dom_len, d->name, d->name_length) ==
                0) {
                return d->f_create_from_ostring(o, o_len, local);
            }
        }
    }

    snmp_log(LOG_ERR, "No support for requested transport domain\n");
    return NULL;
}
#endif /* NETSNMP_FEATURE_REMOVE_TDOMAIN_TRANSPORT_OID */

netsnmp_transport*
netsnmp_transport_open(const char* application, const char* str, int local)
{
    return netsnmp_tdomain_transport_full(application, str, local, NULL, NULL);
}

netsnmp_transport*
netsnmp_transport_open_server(const char* application, const char* str)
{
    return netsnmp_tdomain_transport_full(application, str, 1, NULL, NULL);
}

netsnmp_transport*
netsnmp_transport_open_client(const char* application, const char* str)
{
    return netsnmp_tdomain_transport_full(application, str, 0, NULL, NULL);
}

/** adds a transport to a linked list of transports.
    Returns 1 on failure, 0 on success */
int
netsnmp_transport_add_to_list(netsnmp_transport_list **transport_list,
                              netsnmp_transport *transport)
{
    netsnmp_transport_list *newptr =
        SNMP_MALLOC_TYPEDEF(netsnmp_transport_list);

    if (!newptr)
        return 1;

    newptr->next = *transport_list;
    newptr->transport = transport;

    *transport_list = newptr;

    return 0;
}


/**  removes a transport from a linked list of transports.
     Returns 1 on failure, 0 on success */
int
netsnmp_transport_remove_from_list(netsnmp_transport_list **transport_list,
                                   netsnmp_transport *transport)
{
    netsnmp_transport_list *ptr = *transport_list, *lastptr = NULL;

    while (ptr && ptr->transport != transport) {
        lastptr = ptr;
        ptr = ptr->next;
    }

    if (!ptr)
        return 1;

    if (lastptr)
        lastptr->next = ptr->next;
    else
        *transport_list = ptr->next;

    SNMP_FREE(ptr);

    return 0;
}

int
netsnmp_transport_config_compare(netsnmp_transport_config *left,
                                 netsnmp_transport_config *right) {
    return strcmp(left->key, right->key);
}

netsnmp_transport_config *
netsnmp_transport_create_config(char *key, char *value) {
    netsnmp_transport_config *entry =
        SNMP_MALLOC_TYPEDEF(netsnmp_transport_config);
    entry->key = strdup(key);
    entry->value = strdup(value);
    return entry;
}

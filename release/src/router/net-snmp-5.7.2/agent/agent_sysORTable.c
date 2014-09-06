#include <net-snmp/net-snmp-config.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stddef.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_callbacks.h>
#include <net-snmp/agent/agent_sysORTable.h>
#include <net-snmp/agent/sysORTable.h>

typedef struct data_node_s {
    struct sysORTable data;
    struct data_node_s* next;
    struct data_node_s* prev;
}* data_node;

static data_node table = NULL;

static void
erase(data_node entry)
{
    entry->data.OR_uptime = netsnmp_get_agent_uptime();
    DEBUGMSGTL(("agent/sysORTable", "UNREG_SYSOR %p\n", &entry->data));
    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION, SNMPD_CALLBACK_UNREG_SYSOR,
                        &entry->data);
    free(entry->data.OR_oid);
    free(entry->data.OR_descr);
    if (entry->next == entry)
        table = NULL;
    else {
        entry->next->prev = entry->prev;
        entry->prev->next = entry->next;
        if (entry == table)
            table = entry->next;
    }
    free(entry);
}

void
netsnmp_sysORTable_foreach(void (*f)(const struct sysORTable*, void*), void* c)
{
    DEBUGMSGTL(("agent/sysORTable", "foreach(%p, %p)\n", f, c));
    if(table) {
        data_node run = table;
        do {
            data_node tmp = run;
            run = run->next;
            f(&tmp->data, c);
        } while(table && run != table);
    }
}

int
register_sysORTable_sess(oid * oidin,
                         size_t oidlen,
                         const char *descr, netsnmp_session * ss)
{
    data_node entry;

    DEBUGMSGTL(("agent/sysORTable", "registering: "));
    DEBUGMSGOID(("agent/sysORTable", oidin, oidlen));
    DEBUGMSG(("agent/sysORTable", ", session %p\n", ss));

    entry = (data_node)calloc(1, sizeof(struct data_node_s));
    if (entry == NULL) {
        DEBUGMSGTL(("agent/sysORTable", "Failed to allocate new entry\n"));
        return SYS_ORTABLE_REGISTRATION_FAILED;
    }

    entry->data.OR_descr = strdup(descr);
    if (entry->data.OR_descr == NULL) {
        DEBUGMSGTL(("agent/sysORTable", "Failed to allocate new sysORDescr\n"));
        free(entry);
        return SYS_ORTABLE_REGISTRATION_FAILED;
    }

    entry->data.OR_oid = (oid *) malloc(sizeof(oid) * oidlen);
    if (entry->data.OR_oid == NULL) {
        DEBUGMSGTL(("agent/sysORTable", "Failed to allocate new sysORID\n"));
        free(entry->data.OR_descr);
        free(entry);
        return SYS_ORTABLE_REGISTRATION_FAILED;
    }

    memcpy(entry->data.OR_oid, oidin, sizeof(oid) * oidlen);
    entry->data.OR_oidlen = oidlen;
    entry->data.OR_sess = ss;

    if(table) {
        entry->next = table;
        entry->prev = table->prev;
        table->prev->next = entry;
        table->prev = entry;
    } else
        table = entry->next = entry->prev = entry;

    entry->data.OR_uptime = netsnmp_get_agent_uptime();

    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                        SNMPD_CALLBACK_REG_SYSOR, &entry->data);

    return SYS_ORTABLE_REGISTERED_OK;
}

int
register_sysORTable(oid * oidin, size_t oidlen, const char *descr)
{
    return register_sysORTable_sess(oidin, oidlen, descr, NULL);
}

int
unregister_sysORTable_sess(oid * oidin,
                           size_t oidlen, netsnmp_session * ss)
{
    int any_unregistered = 0;

    DEBUGMSGTL(("agent/sysORTable", "sysORTable unregistering: "));
    DEBUGMSGOID(("agent/sysORTable", oidin, oidlen));
    DEBUGMSG(("agent/sysORTable", ", session %p\n", ss));

    if(table) {
        data_node run = table;
        do {
            data_node tmp = run;
            run = run->next;
            if (tmp->data.OR_sess == ss &&
                snmp_oid_compare(oidin, oidlen,
                                 tmp->data.OR_oid, tmp->data.OR_oidlen) == 0) {
                erase(tmp);
                any_unregistered = 1;
            }
        } while(table && run != table);
    }

    if (any_unregistered) {
        DEBUGMSGTL(("agent/sysORTable", "unregistering successfull\n"));
        return SYS_ORTABLE_UNREGISTERED_OK;
    } else {
        DEBUGMSGTL(("agent/sysORTable", "unregistering failed\n"));
        return SYS_ORTABLE_NO_SUCH_REGISTRATION;
    }
}


int
unregister_sysORTable(oid * oidin, size_t oidlen)
{
    return unregister_sysORTable_sess(oidin, oidlen, NULL);
}


void
unregister_sysORTable_by_session(netsnmp_session * ss)
{
    DEBUGMSGTL(("agent/sysORTable",
                "sysORTable unregistering session %p\n", ss));

   if(table) {
        data_node run = table;
        do {
            data_node tmp = run;
            run = run->next;
            if (((ss->flags & SNMP_FLAGS_SUBSESSION) &&
                 tmp->data.OR_sess == ss) ||
                (!(ss->flags & SNMP_FLAGS_SUBSESSION) && tmp->data.OR_sess &&
                 tmp->data.OR_sess->subsession == ss))
                erase(tmp);
        } while(table && run != table);
    }

    DEBUGMSGTL(("agent/sysORTable",
                "sysORTable unregistering session %p done\n", ss));
}

static int
register_sysOR_callback(int majorID, int minorID, void *serverarg,
                        void *clientarg)
{
    struct sysORTable *parms = (struct sysORTable *) serverarg;

    return register_sysORTable_sess(parms->OR_oid, parms->OR_oidlen,
                                    parms->OR_descr, parms->OR_sess);
}

static int
unregister_sysOR_by_session_callback(int majorID, int minorID,
                                     void *serverarg, void *clientarg)
{
    netsnmp_session *session = (netsnmp_session *) serverarg;

    unregister_sysORTable_by_session(session);

    return 0;
}

static int
unregister_sysOR_callback(int majorID, int minorID, void *serverarg,
                          void *clientarg)
{
    struct sysORTable *parms = (struct sysORTable *) serverarg;

    return unregister_sysORTable_sess(parms->OR_oid,
                                      parms->OR_oidlen,
                                      parms->OR_sess);
}

void
init_agent_sysORTable(void)
{
    DEBUGMSGTL(("agent/sysORTable", "init_agent_sysORTable\n"));

    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_REQ_REG_SYSOR,
                           register_sysOR_callback, NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_REQ_UNREG_SYSOR,
                           unregister_sysOR_callback, NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_REQ_UNREG_SYSOR_SESS,
                           unregister_sysOR_by_session_callback, NULL);
}

void
shutdown_agent_sysORTable(void)
{
    DEBUGMSGTL(("agent/sysORTable", "shutdown_sysORTable\n"));
    while(table)
        erase(table);
}

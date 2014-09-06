#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "sctpAssocTable.h"
#include "sctpAssocLocalAddrTable.h"
#include "sctpAssocRemAddrTable.h"
#include "sctpTables_common.h"

#include "mibgroup/util_funcs/get_pid_from_inode.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/*
 * Linux provides sctp statistics in /proc/net/sctp/assoc and 
 * /proc/net/sctp/remaddr. The 'assoc' file covers sctpAssocTable and 
 * sctpAssocLocalAddrTable, the later one contains sctpAssocRemAddrTable.
 * 
 * Linux does *not* provide *StartTime timestamps. This implementation tries
 * to guess these timestamps.  
 */


#define PROC_PREFIX          "/proc"
#define ASSOC_FILE           PROC_PREFIX "/net/sctp/assocs"
#define REMADDR_FILE         PROC_PREFIX "/net/sctp/remaddr"

/*
 * Convert string with ipv4 or ipv6 address to provided buffer.
 */
static int
convert_address(char *token, char *addr_buffer, u_long * addr_type,
                u_long * addr_len)
{
    int             family;
    int             ret;

    if (strchr(token, ':') != NULL) {
        family = AF_INET6;
        *addr_type = INETADDRESSTYPE_IPV6;
        *addr_len = 16;
    } else {
        family = AF_INET;
        *addr_type = INETADDRESSTYPE_IPV4;
        *addr_len = 4;
    }
    ret = inet_pton(family, token, addr_buffer);

    if (ret <= 0)
        return SNMP_ERR_GENERR;
    return SNMP_ERR_NOERROR;
}


/*
 * Parse local address part from assoc file. It assumes that strtok will return
 * these addresses. The addresses are separated by space and the list ends
 * with "<->". 
 */
static int
parse_assoc_local_addresses(sctpAssocTable_entry * entry,
                            sctpTables_containers * containers)
{
    char           *token;
    int             ret;
    /*
     * parse all local addresses 
     */

    while ((token = strtok(NULL, " "))) {
        sctpAssocLocalAddrTable_entry *localAddr;
        char           *ip = token;

        if (token[0] == '<')
            break;              /* local addresses finished */

        if (token[0] == '*')
            ip = token + 1;

        localAddr = sctpAssocLocalAddrTable_entry_create();
        if (localAddr == NULL)
            return SNMP_ERR_GENERR;

        localAddr->sctpAssocId = entry->sctpAssocId;
        ret =
            convert_address(ip, localAddr->sctpAssocLocalAddr,
                            &localAddr->sctpAssocLocalAddrType,
                            &localAddr->sctpAssocLocalAddr_len);
        if (ret != SNMP_ERR_NOERROR) {
            sctpAssocLocalAddrTable_entry_free(localAddr);
            return SNMP_ERR_GENERR;
        }

        ret =
            sctpAssocLocalAddrTable_add_or_update(containers->
                                                  sctpAssocLocalAddrTable,
                                                  localAddr);
        if (ret != SNMP_ERR_NOERROR)
            return SNMP_ERR_GENERR;
    }
    return SNMP_ERR_NOERROR;
}

/*
 * Parse primary remote address part from assoc file. It assumes that strtok will return
 * all remote addresses. The addresses are separated by space and the list ends with \t .
 */
static int
parse_assoc_remote_addresses(sctpAssocTable_entry * entry)
{
    char           *token;
    int             ret = SNMP_ERR_GENERR;

    while ((token = strtok(NULL, " ")) && (token[0] != '\t')) {
        if (token[0] == '*') {
            /*
             * that's the primary address 
             */
            ret =
                convert_address(token + 1, entry->sctpAssocRemPrimAddr,
                                &entry->sctpAssocRemPrimAddrType,
                                &entry->sctpAssocRemPrimAddr_len);
        }
    }
    return ret;
}

static int
parse_assoc_line(char *line, sctpTables_containers * containers)
{
    unsigned long long inode;
    char           *token;
    int             ret;
    sctpAssocTable_entry *entry;

    entry = sctpAssocTable_entry_create();
    if (entry == NULL)
        return SNMP_ERR_GENERR;

    token = strtok(line, " ");  /* ASSOC, ignore */
    token = strtok(NULL, " ");  /* SOCK, ignore */
    token = strtok(NULL, " ");  /* STY, ignore */
    token = strtok(NULL, " ");  /* SST, ignore */
    token = strtok(NULL, " ");  /* ST */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocState = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* HBKT */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocHeartBeatInterval = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* ASSOC-ID, store */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocId = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* TX_QUEUE, ignore */
    token = strtok(NULL, " ");  /* RX_QUEUE, ignore */
    token = strtok(NULL, " ");  /* UID, ignore */

    token = strtok(NULL, " ");  /* INODE */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    inode = strtoull(token, NULL, 10);
    entry->sctpAssocPrimProcess = netsnmp_get_pid_from_inode(inode);

    token = strtok(NULL, " ");  /* LPORT */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocLocalPort = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* RPORT */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocRemPort = strtol(token, NULL, 10);

    ret = parse_assoc_local_addresses(entry, containers);
    if (ret != SNMP_ERR_NOERROR)
        goto error;

    ret = parse_assoc_remote_addresses(entry);
    if (ret != SNMP_ERR_NOERROR)
        goto error;

    token = strtok(NULL, " ");  /* HBINT */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocHeartBeatInterval = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* INS */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocInStreams = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* OUTS */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocOutStreams = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* MAXRT */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocMaxRetr = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* T1X */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocT1expireds = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* T2X */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocT2expireds = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* RXTC */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocRtxChunks = strtol(token, NULL, 10);

    entry->sctpAssocRemHostName[0] = 0;
    entry->sctpAssocRemHostName_len = 0;
    entry->sctpAssocDiscontinuityTime = 0;

    ret = sctpAssocTable_add_or_update(containers->sctpAssocTable, entry);
    if (ret != SNMP_ERR_NOERROR) {
        DEBUGMSGTL(("sctp:tables:load:assoc",
                    "error adding/updating the entry in container\n"));
        return ret;
    }

    return SNMP_ERR_NOERROR;

  error:
    if (entry != NULL)
        sctpAssocTable_entry_free(entry);
    return ret;
}


/*
 * Load assocTable and localAddrTable from /proc/net/sctp/assoc. Mark all added
 * or updated entries as valid (so the missing, i.e. invalid, can be deleted).
 */
static int
load_assoc(sctpTables_containers * containers)
{
    FILE           *f;
    char            line[1024];
    int             ret = SNMP_ERR_NOERROR;

    DEBUGMSGTL(("sctp:tables:load:assoc", "arch load(linux)\n"));

    f = fopen(ASSOC_FILE, "r");
    if (f == NULL) {
        DEBUGMSGTL(("sctp:tables:load:assoc",
                    "arch load failed: can't open" ASSOC_FILE "\n"));
        return SNMP_ERR_GENERR;
    }

    netsnmp_get_pid_from_inode_init();

    /*
     * ignore the header. 
     */
    fgets(line, sizeof(line), f);

    while (fgets(line, sizeof(line), f) != NULL) {
        DEBUGMSGTL(("sctp:tables:load:assoc", "processing line: %s\n",
                    line));

        ret = parse_assoc_line(line, containers);
        if (ret != SNMP_ERR_NOERROR) {
            DEBUGMSGTL(("sctp:tables:load:assoc",
                        "error parsing the line\n"));
        }
    }
    fclose(f);

    return SNMP_ERR_NOERROR;
}


static int
parse_remaddr_line(char *line, sctpTables_containers * containers)
{
    char           *token;
    int             ret;
    sctpAssocRemAddrTable_entry *entry;

    entry = sctpAssocRemAddrTable_entry_create();
    if (entry == NULL)
        return SNMP_ERR_GENERR;

    token = strtok(line, " ");  /* rem. address */
    ret =
        convert_address(token, entry->sctpAssocRemAddr,
                        &entry->sctpAssocRemAddrType,
                        &entry->sctpAssocRemAddr_len);
    if (ret < 0) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }

    token = strtok(NULL, " ");  /* assoc id */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocId = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* hb act */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    if (token[0] == '1')
        entry->sctpAssocRemAddrHBActive = TRUTHVALUE_TRUE;
    else
        entry->sctpAssocRemAddrHBActive = TRUTHVALUE_FALSE;

    token = strtok(NULL, " ");  /* rto */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocRemAddrRTO = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* max path rtx */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocRemAddrMaxPathRtx = strtol(token, NULL, 10);

    token = strtok(NULL, " ");  /* rem addr rtx */
    if (token == NULL) {
        ret = SNMP_ERR_GENERR;
        goto error;
    }
    entry->sctpAssocRemAddrRtx = strtol(token, NULL, 10);

    entry->sctpAssocRemAddrStartTime = 0;
    entry->sctpAssocRemAddrActive = TRUTHVALUE_TRUE;

    ret =
        sctpAssocRemAddrTable_add_or_update(containers->
                                            sctpAssocRemAddrTable, entry);
    if (ret != SNMP_ERR_NOERROR) {
        DEBUGMSGTL(("sctp:load:remaddr",
                    "error adding/updating the entry in container\n"));
        return ret;
    }

    return SNMP_ERR_NOERROR;

  error:
    if (entry != NULL)
        sctpAssocRemAddrTable_entry_free(entry);
    return ret;
}

/*
 * Load sctpAssocRemAddrTable from /proc/net/sctp/remaddr. Mark all added
 * or updated entries as valid (so the missing, i.e. invalid, can be deleted).
 */
static int
load_remaddr(sctpTables_containers * containers)
{
    FILE           *f;
    char            line[1024];
    int             ret = SNMP_ERR_NOERROR;

    DEBUGMSGTL(("sctp:load:remaddr", "arch load(linux)\n"));

    f = fopen(REMADDR_FILE, "r");
    if (f == NULL) {
        DEBUGMSGTL(("sctp:load:remaddr",
                    "arch load failed: can't open" REMADDR_FILE "\n"));
        return SNMP_ERR_GENERR;
    }

    /*
     * ignore the header. 
     */
    fgets(line, sizeof(line), f);

    while (fgets(line, sizeof(line), f) != NULL) {
        DEBUGMSGTL(("sctp:load:remaddr", "processing line: %s\n", line));

        ret = parse_remaddr_line(line, containers);
        if (ret != SNMP_ERR_NOERROR) {
            DEBUGMSGTL(("sctp:load:remaddr", "error parsing the line\n"));
        }
    }
    fclose(f);

    return SNMP_ERR_NOERROR;
}


int
sctpTables_arch_load(sctpTables_containers * containers, u_long * flags)
{
    int             ret = SNMP_ERR_NOERROR;

    *flags |= SCTP_TABLES_LOAD_FLAG_DELETE_INVALID;
    *flags |= SCTP_TABLES_LOAD_FLAG_AUTO_LOOKUP;

    ret = load_assoc(containers);
    if (ret != SNMP_ERR_NOERROR)
        return ret;

    ret = load_remaddr(containers);
    if (ret != SNMP_ERR_NOERROR)
        return ret;

    return ret;
}

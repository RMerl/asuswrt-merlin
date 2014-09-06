#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "sctpAssocTable.h"
#include "sctpAssocLocalAddrTable.h"
#include "sctpAssocRemAddrTable.h"
#include "sctpTables_common.h"

#include <util_funcs.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/socket.h>
#include <inet/mib2.h>

#include "kernel_sunos5.h"

static int
parse_assoc_local_address(sctpTables_containers * containers,
			    mib2_sctpConnLocalEntry_t *lce)
{
    int ret;
    sctpAssocLocalAddrTable_entry *entry;
    entry = sctpAssocLocalAddrTable_entry_create();
    if (entry == NULL)
	return SNMP_ERR_GENERR;

    entry->sctpAssocId = 0;
    entry->sctpAssocLocalAddrType = lce->sctpAssocLocalAddrType;
    if (lce->sctpAssocLocalAddrType == INETADDRESSTYPE_IPV4) {
	entry->sctpAssocLocalAddr_len = 4;
	memcpy(entry->sctpAssocLocalAddr,
		   lce->sctpAssocLocalAddr.s6_addr+12,
		   entry->sctpAssocLocalAddr_len);
    } else if (lce->sctpAssocLocalAddrType == INETADDRESSTYPE_IPV6) {
	entry->sctpAssocLocalAddr_len = 16;
	memcpy(entry->sctpAssocLocalAddr,
		   &lce->sctpAssocLocalAddr,
		   entry->sctpAssocLocalAddr_len);
    }
    entry->sctpAssocLocalAddrStartTime = 0;
    ret = sctpAssocLocalAddrTable_add_or_update(containers->sctpAssocLocalAddrTable,
					      entry);
    return ret;
}

static int
parse_assoc(sctpTables_containers * containers, mib2_sctpConnEntry_t *sce)
{
    int             ret;
    sctpAssocTable_entry *entry;

    entry = sctpAssocTable_entry_create();
    if (entry == NULL)
	return SNMP_ERR_GENERR;

    switch (sce->sctpAssocState) {
    case MIB2_SCTP_closed:
	entry->sctpAssocState = SCTPASSOCSTATE_CLOSED;
	break;
    case MIB2_SCTP_cookieWait:
	entry->sctpAssocState = SCTPASSOCSTATE_COOKIEWAIT;
	break;
    case MIB2_SCTP_cookieEchoed:
	entry->sctpAssocState = SCTPASSOCSTATE_COOKIEECHOED;
	break;
    case MIB2_SCTP_established:
	entry->sctpAssocState = SCTPASSOCSTATE_ESTABLISHED;
	break;
    case MIB2_SCTP_shutdownPending:
	entry->sctpAssocState = SCTPASSOCSTATE_SHUTDOWNPENDING;
	break;
    case MIB2_SCTP_shutdownSent:
	entry->sctpAssocState = SCTPASSOCSTATE_SHUTDOWNSENT;
	break;
    case MIB2_SCTP_shutdownReceived:
	entry->sctpAssocState = SCTPASSOCSTATE_SHUTDOWNRECEIVED;
	break;
    case MIB2_SCTP_shutdownAckSent:
	entry->sctpAssocState = SCTPASSOCSTATE_SHUTDOWNACKSENT;
	break;
    }
    entry->sctpAssocHeartBeatInterval = sce->sctpAssocHeartBeatInterval;
    entry->sctpAssocId = sce->sctpAssocId;
    entry->sctpAssocPrimProcess = sce->sctpAssocPrimProcess;
    entry->sctpAssocLocalPort = sce->sctpAssocLocalPort;
    entry->sctpAssocRemPrimAddrType = sce->sctpAssocRemPrimAddrType;
    if (entry->sctpAssocRemPrimAddrType == INETADDRESSTYPE_IPV4) {
	entry->sctpAssocRemPrimAddr_len = 4;
	memcpy(entry->sctpAssocRemPrimAddr, sce->sctpAssocRemPrimAddr.s6_addr+12,
	    entry->sctpAssocRemPrimAddr_len);
    }
    else {
	entry->sctpAssocRemPrimAddr_len = 16;
	memcpy(entry->sctpAssocRemPrimAddr, sce->sctpAssocRemPrimAddr.s6_addr,
	    entry->sctpAssocRemPrimAddr_len);
    }
    entry->sctpAssocRemPort = sce->sctpAssocRemPort;
    entry->sctpAssocInStreams = sce->sctpAssocInStreams;
    entry->sctpAssocOutStreams = sce->sctpAssocOutStreams;
    entry->sctpAssocMaxRetr = sce->sctpAssocMaxRetr;
    entry->sctpAssocT1expireds = sce->sctpAssocT1expired;
    entry->sctpAssocRtxChunks = sce->sctpAssocRtxChunks;
    entry->sctpAssocT2expireds = sce->sctpAssocT2expired; 
    entry->sctpAssocRemHostName[0] = 0;
    entry->sctpAssocRemHostName_len = 0;
    entry->sctpAssocDiscontinuityTime = 0;
    entry->sctpAssocStartTime = sce->sctpAssocStartTime;	

    ret = sctpAssocTable_add_or_update(containers->sctpAssocTable, entry);
    return ret;
}


static int
parse_assoc_remote_address(sctpTables_containers * containers,
		    mib2_sctpConnRemoteEntry_t *rce)
{
    int             ret;
    sctpAssocRemAddrTable_entry *entry;

    entry = sctpAssocRemAddrTable_entry_create();
    if (entry == NULL)
	return SNMP_ERR_GENERR;

    entry->sctpAssocId = rce->sctpAssocId;
    entry->sctpAssocRemAddrActive = rce->sctpAssocRemAddrActive;
    entry->sctpAssocRemAddrHBActive = rce->sctpAssocRemAddrHBActive;
    entry->sctpAssocRemAddrRTO = rce->sctpAssocRemAddrRTO;
    entry->sctpAssocRemAddrMaxPathRtx = rce->sctpAssocRemAddrMaxPathRtx;
    entry->sctpAssocRemAddrRtx = rce->sctpAssocRemAddrRtx;
    entry->sctpAssocRemAddrStartTime = 0;

    entry->sctpAssocRemAddrType = rce->sctpAssocRemAddrType;
    if (rce->sctpAssocRemAddrType == INETADDRESSTYPE_IPV4) {
	entry->sctpAssocRemAddr_len = 4;
	memcpy(entry->sctpAssocRemAddr,
		   rce->sctpAssocRemAddr.s6_addr+12,
		   entry->sctpAssocRemAddr_len);
    } else if (rce->sctpAssocRemAddrType == INETADDRESSTYPE_IPV6) {
	entry->sctpAssocRemAddr_len = 16;
	memcpy(entry->sctpAssocRemAddr,
		   &rce->sctpAssocRemAddr,
		   entry->sctpAssocRemAddr_len);
    }
    ret =
	sctpAssocRemAddrTable_add_or_update(containers->sctpAssocRemAddrTable,
					    entry);
    return ret;
}


int
sctpTables_arch_load(sctpTables_containers * containers, u_long * flags)
{
    int                ret = SNMP_ERR_NOERROR;
    mib2_sctpConnEntry_t sce;
    mib2_sctpConnLocalEntry_t lce;
    mib2_sctpConnRemoteEntry_t rce;
    req_e              req = GET_FIRST;

    while (getMibstat(MIB_SCTP_CONN, &sce, sizeof(sce), req, &Get_everything, 0) == 0) {
	req = GET_NEXT;
	parse_assoc(containers, &sce);
    }
    req = GET_FIRST;
    while (getMibstat(MIB_SCTP_CONN_LOCAL, &lce, sizeof(lce), req, &Get_everything, 0) == 0) {
	req = GET_NEXT;
	parse_assoc_local_address(containers, &lce);
    }
    req = GET_FIRST;
    while (getMibstat(MIB_SCTP_CONN_REMOTE, &rce, sizeof(rce), req, &Get_everything, 0) == 0) {
	req = GET_NEXT;
	parse_assoc_remote_address(containers, &rce);
    }
    return ret;
}

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
#include <sys/sysctl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <netinet/sctp_constants.h>


static int
parse_assoc_local_addresses(sctpTables_containers * containers,
							struct xsctp_laddr *xladdr)
{
  int ret;
  sctpAssocLocalAddrTable_entry *entry;
  entry = sctpAssocLocalAddrTable_entry_create();
  if (entry == NULL)
	return SNMP_ERR_GENERR;

  entry->sctpAssocId = 0;
  if (xladdr->address.sa.sa_family == AF_INET) {
	entry->sctpAssocLocalAddrType = INETADDRESSTYPE_IPV4;
	entry->sctpAssocLocalAddr_len = 4;
	memcpy(entry->sctpAssocLocalAddr,
		   &xladdr->address.sin.sin_addr,
		   entry->sctpAssocLocalAddr_len);
  } else if (xladdr->address.sa.sa_family == AF_INET6) {
	entry->sctpAssocLocalAddrType = INETADDRESSTYPE_IPV6;
	entry->sctpAssocLocalAddr_len = 16;
	memcpy(entry->sctpAssocLocalAddr,
		   &xladdr->address.sin6.sin6_addr,
		   entry->sctpAssocLocalAddr_len);
  }
  entry->sctpAssocLocalAddrStartTime = xladdr->start_time.tv_sec;
  ret = sctpAssocLocalAddrTable_add_or_update(containers->sctpAssocLocalAddrTable,
											  entry);
  if (ret != SNMP_ERR_NOERROR)
	return SNMP_ERR_GENERR;

  return SNMP_ERR_NOERROR;
}

static int
parse_assoc_xstcb(sctpTables_containers * containers, struct xsctp_tcb *xstcb)
{
  int             ret;
  sctpAssocTable_entry *entry;

  entry = sctpAssocTable_entry_create();
  if (entry == NULL)
	return SNMP_ERR_GENERR;

  switch (xstcb->state) {
  case SCTP_STATE_INUSE:
	entry->sctpAssocState = SCTPASSOCSTATE_DELETETCB;
	break;
  case SCTP_STATE_COOKIE_WAIT:
	entry->sctpAssocState = SCTPASSOCSTATE_COOKIEWAIT;
	break;
  case SCTP_STATE_COOKIE_ECHOED:
	entry->sctpAssocState = SCTPASSOCSTATE_COOKIEECHOED;
	break;
  case SCTP_STATE_OPEN:
	entry->sctpAssocState = SCTPASSOCSTATE_ESTABLISHED;
	break;
  case SCTP_STATE_SHUTDOWN_SENT:
	entry->sctpAssocState = SCTPASSOCSTATE_SHUTDOWNSENT;
	break;
  case SCTP_STATE_SHUTDOWN_RECEIVED:
	entry->sctpAssocState = SCTPASSOCSTATE_SHUTDOWNRECEIVED;
	break;
  case SCTP_STATE_SHUTDOWN_ACK_SENT:
	entry->sctpAssocState = SCTPASSOCSTATE_SHUTDOWNACKSENT;
	break;
  default:
  case SCTP_STATE_EMPTY:
	entry->sctpAssocState = SCTPASSOCSTATE_CLOSED;
	break;
  };
  entry->sctpAssocHeartBeatInterval = xstcb->heartbeat_interval;
  entry->sctpAssocId = 0;
  entry->sctpAssocPrimProcess = xstcb->primary_process;
  entry->sctpAssocLocalPort = xstcb->local_port;
  entry->sctpAssocRemPort = xstcb->remote_port;
  entry->sctpAssocHeartBeatInterval = xstcb->heartbeat_interval;
  entry->sctpAssocInStreams = xstcb->in_streams;
  entry->sctpAssocOutStreams = xstcb->out_streams;
  entry->sctpAssocMaxRetr = xstcb->max_nr_retrans;
  entry->sctpAssocT1expireds = xstcb->T1_expireries;
  entry->sctpAssocRtxChunks = xstcb->retransmitted_tsns;
  entry->sctpAssocT2expireds = xstcb->T2_expireries; 
  entry->sctpAssocRemHostName[0] = 0;
  entry->sctpAssocRemHostName_len = 0;
  entry->sctpAssocDiscontinuityTime = xstcb->discontinuity_time.tv_sec;
  entry->sctpAssocStartTime = xstcb->start_time.tv_sec;	

  ret = sctpAssocTable_add_or_update(containers->sctpAssocTable, entry);
  if (ret != SNMP_ERR_NOERROR) {
	return ret;
  }

  return SNMP_ERR_NOERROR;
}


static int
parse_remaddr_xraddr(sctpTables_containers * containers,
					 struct xsctp_raddr *xraddr)
{
  int             ret;
  sctpAssocRemAddrTable_entry *entry;

  entry = sctpAssocRemAddrTable_entry_create();
  if (entry == NULL)
	return SNMP_ERR_GENERR;

	
  entry->sctpAssocId = 0;

  if(xraddr->active) 
    entry->sctpAssocRemAddrActive = TRUTHVALUE_TRUE;
  else
	entry->sctpAssocRemAddrActive = TRUTHVALUE_FALSE;
  
  if (xraddr->heartbeat_enabled) 
	entry->sctpAssocRemAddrHBActive = TRUTHVALUE_TRUE;
  else
	entry->sctpAssocRemAddrHBActive = TRUTHVALUE_FALSE;

  entry->sctpAssocRemAddrRTO = xraddr->rto;
  entry->sctpAssocRemAddrMaxPathRtx = xraddr->max_path_rtx;
  entry->sctpAssocRemAddrRtx = xraddr->rtx;
  entry->sctpAssocRemAddrStartTime = xraddr->start_time.tv_sec;

  if (xraddr->address.sa.sa_family == AF_INET) {
	entry->sctpAssocRemAddrType = INETADDRESSTYPE_IPV4;
	entry->sctpAssocRemAddr_len = 4;
	memcpy(entry->sctpAssocRemAddr,
		   &xraddr->address.sin.sin_addr,
		   entry->sctpAssocRemAddr_len);
  } else if (xraddr->address.sa.sa_family == AF_INET6) {
	entry->sctpAssocRemAddrType = INETADDRESSTYPE_IPV6;
	entry->sctpAssocRemAddr_len = 16;
	memcpy(entry->sctpAssocRemAddr,
		   &xraddr->address.sin6.sin6_addr,
		   entry->sctpAssocRemAddr_len);
  }
  ret =
	sctpAssocRemAddrTable_add_or_update(containers->
										sctpAssocRemAddrTable, entry);
  if (ret != SNMP_ERR_NOERROR) {
	return ret;
  }
  return SNMP_ERR_NOERROR;
}


int
sctpTables_arch_load(sctpTables_containers * containers, u_long * flags)
{
  int             ret = SNMP_ERR_NOERROR;
  size_t len;
  caddr_t buf;
  unsigned int offset;
  struct xsctp_inpcb *xinp;
  struct xsctp_tcb *xstcb;
  struct xsctp_laddr *xladdr;
  struct xsctp_raddr *xraddr;


  *flags |= SCTP_TABLES_LOAD_FLAG_DELETE_INVALID;
  *flags |= SCTP_TABLES_LOAD_FLAG_AUTO_LOOKUP;
  len = 0;
  if (sysctlbyname("net.inet.sctp.assoclist", 0, &len, 0, 0) < 0) {
	printf("Error %d (%s) could not get the assoclist\n", errno, strerror(errno));
	return(-1);
  }
  if ((buf = (caddr_t)malloc(len)) == 0) {
	printf("malloc %lu bytes failed.\n", (long unsigned)len);
	return(-1);
  }
  if (sysctlbyname("net.inet.sctp.assoclist", buf, &len, 0, 0) < 0) {
	printf("Error %d (%s) could not get the assoclist\n", errno, strerror(errno));
	free(buf);
	return(-1);
  }
  offset = 0;
  xinp = (struct xsctp_inpcb *)(buf + offset);
  while (xinp->last == 0) {
	/* for each INP */
	offset += sizeof(struct xsctp_inpcb);
	/* Local addresses */
	xladdr = (struct xsctp_laddr *)(buf + offset);
	while (xladdr->last == 0) {
	  offset += sizeof(struct xsctp_laddr);
	  xladdr = (struct xsctp_laddr *)(buf + offset);
	}
	offset += sizeof(struct xsctp_laddr);
	/* Associations */
	xstcb = (struct xsctp_tcb *)(buf + offset);
	while (xstcb->last == 0) {
	  xstcb = (struct xsctp_tcb *)(buf + offset);
	  offset += sizeof(struct xsctp_tcb);
	  parse_assoc_xstcb(containers, xstcb);
	  /* Local addresses */
	  xladdr = (struct xsctp_laddr *)(buf + offset);
	  while (xladdr->last == 0) {
		parse_assoc_local_addresses(containers, xladdr);
		offset += sizeof(struct xsctp_laddr);
		xladdr = (struct xsctp_laddr *)(buf + offset);
	  }
	  offset += sizeof(struct xsctp_laddr);

	  /* Remote addresses */
	  xraddr = (struct xsctp_raddr *)(buf + offset);
	  while (xraddr->last == 0) {
		parse_remaddr_xraddr(containers, xraddr);
		offset += sizeof(struct xsctp_raddr);
		xraddr = (struct xsctp_raddr *)(buf + offset);
	  }
	  offset += sizeof(struct xsctp_raddr);
	  xstcb = (struct xsctp_tcb *)(buf + offset);
	}
	offset += sizeof(struct xsctp_tcb);
	xinp = (struct xsctp_inpcb *)(buf + offset);
  }
  free((void *)buf);
  return ret;
}

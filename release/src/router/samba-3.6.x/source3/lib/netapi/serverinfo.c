/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Server Support
 *  Copyright (C) Guenther Deschner 2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

#include "librpc/gen_ndr/libnetapi.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "lib/netapi/libnetapi.h"
#include "../librpc/gen_ndr/ndr_srvsvc_c.h"
#include "lib/smbconf/smbconf.h"
#include "lib/smbconf/smbconf_reg.h"

/****************************************************************
****************************************************************/

static WERROR NetServerGetInfo_l_101(struct libnetapi_ctx *ctx,
				     uint8_t **buffer)
{
	struct SERVER_INFO_101 i;

	i.sv101_platform_id	= PLATFORM_ID_NT;
	i.sv101_name		= global_myname();
	i.sv101_version_major	= lp_major_announce_version();
	i.sv101_version_minor	= lp_minor_announce_version();
	i.sv101_type		= lp_default_server_announce();
	i.sv101_comment		= lp_serverstring();

	*buffer = (uint8_t *)talloc_memdup(ctx, &i, sizeof(i));
	if (!*buffer) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR NetServerGetInfo_l_1005(struct libnetapi_ctx *ctx,
				      uint8_t **buffer)
{
	struct SERVER_INFO_1005 info1005;

	info1005.sv1005_comment = lp_serverstring();
	*buffer = (uint8_t *)talloc_memdup(ctx, &info1005, sizeof(info1005));
	if (!*buffer) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR NetServerGetInfo_l(struct libnetapi_ctx *ctx,
			  struct NetServerGetInfo *r)
{
	switch (r->in.level) {
		case 101:
			return NetServerGetInfo_l_101(ctx, r->out.buffer);
		case 1005:
			return NetServerGetInfo_l_1005(ctx, r->out.buffer);
		default:
			break;
	}

	return WERR_UNKNOWN_LEVEL;
}

/****************************************************************
****************************************************************/

static NTSTATUS map_server_info_to_SERVER_INFO_buffer(TALLOC_CTX *mem_ctx,
						      uint32_t level,
						      union srvsvc_NetSrvInfo *i,
						      uint8_t **buffer)
{
	struct SERVER_INFO_100 i100;
	struct SERVER_INFO_101 i101;
	struct SERVER_INFO_102 i102;
	struct SERVER_INFO_402 i402;
	struct SERVER_INFO_403 i403;
	struct SERVER_INFO_502 i502;
	struct SERVER_INFO_503 i503;
	struct SERVER_INFO_599 i599;
	struct SERVER_INFO_1005 i1005;
#if 0
	struct SERVER_INFO_1010 i1010;
	struct SERVER_INFO_1016 i1016;
	struct SERVER_INFO_1017 i1017;
	struct SERVER_INFO_1018 i1018;
	struct SERVER_INFO_1107 i1107;
	struct SERVER_INFO_1501 i1501;
	struct SERVER_INFO_1502 i1502;
	struct SERVER_INFO_1503 i1503;
	struct SERVER_INFO_1506 i1506;
	struct SERVER_INFO_1509 i1509;
	struct SERVER_INFO_1510 i1510;
	struct SERVER_INFO_1511 i1511;
	struct SERVER_INFO_1512 i1512;
	struct SERVER_INFO_1513 i1513;
	struct SERVER_INFO_1514 i1514;
	struct SERVER_INFO_1515 i1515;
	struct SERVER_INFO_1516 i1516;
	struct SERVER_INFO_1518 i1518;
	struct SERVER_INFO_1520 i1520;
	struct SERVER_INFO_1521 i1521;
	struct SERVER_INFO_1522 i1522;
	struct SERVER_INFO_1523 i1523;
	struct SERVER_INFO_1524 i1524;
	struct SERVER_INFO_1525 i1525;
	struct SERVER_INFO_1528 i1528;
	struct SERVER_INFO_1529 i1529;
	struct SERVER_INFO_1530 i1530;
	struct SERVER_INFO_1533 i1533;
	struct SERVER_INFO_1534 i1534;
	struct SERVER_INFO_1535 i1535;
	struct SERVER_INFO_1536 i1536;
	struct SERVER_INFO_1537 i1537;
	struct SERVER_INFO_1538 i1538;
	struct SERVER_INFO_1539 i1539;
	struct SERVER_INFO_1540 i1540;
	struct SERVER_INFO_1541 i1541;
	struct SERVER_INFO_1542 i1542;
	struct SERVER_INFO_1543 i1543;
	struct SERVER_INFO_1544 i1544;
	struct SERVER_INFO_1545 i1545;
	struct SERVER_INFO_1546 i1546;
	struct SERVER_INFO_1547 i1547;
	struct SERVER_INFO_1548 i1548;
	struct SERVER_INFO_1549 i1549;
	struct SERVER_INFO_1550 i1550;
	struct SERVER_INFO_1552 i1552;
	struct SERVER_INFO_1553 i1553;
	struct SERVER_INFO_1554 i1554;
	struct SERVER_INFO_1555 i1555;
	struct SERVER_INFO_1556 i1556;
	struct SERVER_INFO_1557 i1557;
	struct SERVER_INFO_1560 i1560;
	struct SERVER_INFO_1561 i1561;
	struct SERVER_INFO_1562 i1562;
	struct SERVER_INFO_1563 i1563;
	struct SERVER_INFO_1564 i1564;
	struct SERVER_INFO_1565 i1565;
	struct SERVER_INFO_1566 i1566;
	struct SERVER_INFO_1567 i1567;
	struct SERVER_INFO_1568 i1568;
	struct SERVER_INFO_1569 i1569;
	struct SERVER_INFO_1570 i1570;
	struct SERVER_INFO_1571 i1571;
	struct SERVER_INFO_1572 i1572;
	struct SERVER_INFO_1573 i1573;
	struct SERVER_INFO_1574 i1574;
	struct SERVER_INFO_1575 i1575;
	struct SERVER_INFO_1576 i1576;
	struct SERVER_INFO_1577 i1577;
	struct SERVER_INFO_1578 i1578;
	struct SERVER_INFO_1579 i1579;
	struct SERVER_INFO_1580 i1580;
	struct SERVER_INFO_1581 i1581;
	struct SERVER_INFO_1582 i1582;
	struct SERVER_INFO_1583 i1583;
	struct SERVER_INFO_1584 i1584;
	struct SERVER_INFO_1585 i1585;
	struct SERVER_INFO_1586 i1586;
	struct SERVER_INFO_1587 i1587;
	struct SERVER_INFO_1588 i1588;
	struct SERVER_INFO_1590 i1590;
	struct SERVER_INFO_1591 i1591;
	struct SERVER_INFO_1592 i1592;
	struct SERVER_INFO_1593 i1593;
	struct SERVER_INFO_1594 i1594;
	struct SERVER_INFO_1595 i1595;
	struct SERVER_INFO_1596 i1596;
	struct SERVER_INFO_1597 i1597;
	struct SERVER_INFO_1598 i1598;
	struct SERVER_INFO_1599 i1599;
	struct SERVER_INFO_1600 i1600;
	struct SERVER_INFO_1601 i1601;
	struct SERVER_INFO_1602 i1602;
#endif
	uint32_t num_info = 0;

	switch (level) {
		case 100:
			i100.sv100_platform_id		= i->info100->platform_id;
			i100.sv100_name			= talloc_strdup(mem_ctx, i->info100->server_name);

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_100, i100,
				     (struct SERVER_INFO_100 **)buffer,
				     &num_info);
			break;

		case 101:
			i101.sv101_platform_id		= i->info101->platform_id;
			i101.sv101_name			= talloc_strdup(mem_ctx, i->info101->server_name);
			i101.sv101_version_major	= i->info101->version_major;
			i101.sv101_version_minor	= i->info101->version_minor;
			i101.sv101_type			= i->info101->server_type;
			i101.sv101_comment		= talloc_strdup(mem_ctx, i->info101->comment);

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_101, i101,
				     (struct SERVER_INFO_101 **)buffer,
				     &num_info);
			break;

		case 102:
			i102.sv102_platform_id		= i->info102->platform_id;
			i102.sv102_name			= talloc_strdup(mem_ctx, i->info102->server_name);
			i102.sv102_version_major	= i->info102->version_major;
			i102.sv102_version_minor	= i->info102->version_minor;
			i102.sv102_type			= i->info102->server_type;
			i102.sv102_comment		= talloc_strdup(mem_ctx, i->info102->comment);
			i102.sv102_users		= i->info102->users;
			i102.sv102_disc			= i->info102->disc;
			i102.sv102_hidden		= i->info102->hidden;
			i102.sv102_announce		= i->info102->announce;
			i102.sv102_anndelta		= i->info102->anndelta;
			i102.sv102_licenses		= i->info102->licenses;
			i102.sv102_userpath		= talloc_strdup(mem_ctx, i->info102->userpath);

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_102, i102,
				     (struct SERVER_INFO_102 **)buffer,
				     &num_info);
			break;

		case 402:

			i402.sv402_ulist_mtime		= i->info402->ulist_mtime;
			i402.sv402_glist_mtime		= i->info402->glist_mtime;
			i402.sv402_alist_mtime		= i->info402->alist_mtime;
			i402.sv402_alerts		= talloc_strdup(mem_ctx, i->info402->alerts);
			i402.sv402_security		= i->info402->security;
			i402.sv402_numadmin		= i->info402->numadmin;
			i402.sv402_lanmask		= i->info402->lanmask;
			i402.sv402_guestacct		= talloc_strdup(mem_ctx, i->info402->guestaccount);
			i402.sv402_chdevs		= i->info402->chdevs;
			i402.sv402_chdevq		= i->info402->chdevqs;
			i402.sv402_chdevjobs		= i->info402->chdevjobs;
			i402.sv402_connections		= i->info402->connections;
			i402.sv402_shares		= i->info402->shares;
			i402.sv402_openfiles		= i->info402->openfiles;
			i402.sv402_sessopens		= i->info402->sessopen;
			i402.sv402_sessvcs		= i->info402->sesssvc;
			i402.sv402_sessreqs		= i->info402->sessreqs;
			i402.sv402_opensearch		= i->info402->opensearch;
			i402.sv402_activelocks		= i->info402->activelocks;
			i402.sv402_numreqbuf		= i->info402->numreqbufs;
			i402.sv402_sizreqbuf		= i->info402->sizereqbufs;
			i402.sv402_numbigbuf		= i->info402->numbigbufs;
			i402.sv402_numfiletasks		= i->info402->numfiletasks;
			i402.sv402_alertsched		= i->info402->alertsched;
			i402.sv402_erroralert		= i->info402->erroralert;
			i402.sv402_logonalert		= i->info402->logonalert;
			i402.sv402_accessalert		= i->info402->accessalert;
			i402.sv402_diskalert		= i->info402->diskalert;
			i402.sv402_netioalert		= i->info402->netioalert;
			i402.sv402_maxauditsz		= i->info402->maxaudits;
			i402.sv402_srvheuristics	= i->info402->srvheuristics;

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_402, i402,
				     (struct SERVER_INFO_402 **)buffer,
				     &num_info);
			break;

		case 403:

			i403.sv403_ulist_mtime		= i->info403->ulist_mtime;
			i403.sv403_glist_mtime		= i->info403->glist_mtime;
			i403.sv403_alist_mtime		= i->info403->alist_mtime;
			i403.sv403_alerts		= talloc_strdup(mem_ctx, i->info403->alerts);
			i403.sv403_security		= i->info403->security;
			i403.sv403_numadmin		= i->info403->numadmin;
			i403.sv403_lanmask		= i->info403->lanmask;
			i403.sv403_guestacct		= talloc_strdup(mem_ctx, i->info403->guestaccount);
			i403.sv403_chdevs		= i->info403->chdevs;
			i403.sv403_chdevq		= i->info403->chdevqs;
			i403.sv403_chdevjobs		= i->info403->chdevjobs;
			i403.sv403_connections		= i->info403->connections;
			i403.sv403_shares		= i->info403->shares;
			i403.sv403_openfiles		= i->info403->openfiles;
			i403.sv403_sessopens		= i->info403->sessopen;
			i403.sv403_sessvcs		= i->info403->sesssvc;
			i403.sv403_sessreqs		= i->info403->sessreqs;
			i403.sv403_opensearch		= i->info403->opensearch;
			i403.sv403_activelocks		= i->info403->activelocks;
			i403.sv403_numreqbuf		= i->info403->numreqbufs;
			i403.sv403_sizreqbuf		= i->info403->sizereqbufs;
			i403.sv403_numbigbuf		= i->info403->numbigbufs;
			i403.sv403_numfiletasks		= i->info403->numfiletasks;
			i403.sv403_alertsched		= i->info403->alertsched;
			i403.sv403_erroralert		= i->info403->erroralert;
			i403.sv403_logonalert		= i->info403->logonalert;
			i403.sv403_accessalert		= i->info403->accessalert;
			i403.sv403_diskalert		= i->info403->diskalert;
			i403.sv403_netioalert		= i->info403->netioalert;
			i403.sv403_maxauditsz		= i->info403->maxaudits;
			i403.sv403_srvheuristics	= i->info403->srvheuristics;
			i403.sv403_auditedevents	= i->info403->auditedevents;
			i403.sv403_autoprofile		= i->info403->auditprofile;
			i403.sv403_autopath		= talloc_strdup(mem_ctx, i->info403->autopath);

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_403, i403,
				     (struct SERVER_INFO_403 **)buffer,
				     &num_info);
			break;

		case 502:
			i502.sv502_sessopens		= i->info502->sessopen;
			i502.sv502_sessvcs		= i->info502->sesssvc;
			i502.sv502_opensearch		= i->info502->opensearch;
			i502.sv502_sizreqbuf		= i->info502->sizereqbufs;
			i502.sv502_initworkitems	= i->info502->initworkitems;
			i502.sv502_maxworkitems		= i->info502->maxworkitems;
			i502.sv502_rawworkitems		= i->info502->rawworkitems;
			i502.sv502_irpstacksize		= i->info502->irpstacksize;
			i502.sv502_maxrawbuflen		= i->info502->maxrawbuflen;
			i502.sv502_sessusers		= i->info502->sessusers;
			i502.sv502_sessconns		= i->info502->sessconns;
			i502.sv502_maxpagedmemoryusage	= i->info502->maxpagedmemoryusage;
			i502.sv502_maxnonpagedmemoryusage = i->info502->maxnonpagedmemoryusage;
			i502.sv502_enablesoftcompat	= i->info502->enablesoftcompat;
			i502.sv502_enableforcedlogoff	= i->info502->enableforcedlogoff;
			i502.sv502_timesource		= i->info502->timesource;
			i502.sv502_acceptdownlevelapis	= i->info502->acceptdownlevelapis;
			i502.sv502_lmannounce		= i->info502->lmannounce;

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_502, i502,
				     (struct SERVER_INFO_502 **)buffer,
				     &num_info);
			break;

		case 503:
			i503.sv503_sessopens		= i->info503->sessopen;
			i503.sv503_sessvcs		= i->info503->sesssvc;
			i503.sv503_opensearch		= i->info503->opensearch;
			i503.sv503_sizreqbuf		= i->info503->sizereqbufs;
			i503.sv503_initworkitems	= i->info503->initworkitems;
			i503.sv503_maxworkitems		= i->info503->maxworkitems;
			i503.sv503_rawworkitems		= i->info503->rawworkitems;
			i503.sv503_irpstacksize		= i->info503->irpstacksize;
			i503.sv503_maxrawbuflen		= i->info503->maxrawbuflen;
			i503.sv503_sessusers		= i->info503->sessusers;
			i503.sv503_sessconns		= i->info503->sessconns;
			i503.sv503_maxpagedmemoryusage	= i->info503->maxpagedmemoryusage;
			i503.sv503_maxnonpagedmemoryusage = i->info503->maxnonpagedmemoryusage;
			i503.sv503_enablesoftcompat	= i->info503->enablesoftcompat;
			i503.sv503_enableforcedlogoff	= i->info503->enableforcedlogoff;
			i503.sv503_timesource		= i->info503->timesource;
			i503.sv503_acceptdownlevelapis	= i->info503->acceptdownlevelapis;
			i503.sv503_lmannounce		= i->info503->lmannounce;
			i503.sv503_domain		= talloc_strdup(mem_ctx, i->info503->domain);
			i503.sv503_maxcopyreadlen	= i->info503->maxcopyreadlen;
			i503.sv503_maxcopywritelen	= i->info503->maxcopywritelen;
			i503.sv503_minkeepsearch	= i->info503->minkeepsearch;
			i503.sv503_maxkeepsearch	= i->info503->maxkeepsearch;
			i503.sv503_minkeepcomplsearch	= i->info503->minkeepcomplsearch;
			i503.sv503_maxkeepcomplsearch	= i->info503->maxkeepcomplsearch;
			i503.sv503_threadcountadd	= i->info503->threadcountadd;
			i503.sv503_numblockthreads	= i->info503->numlockthreads;
			i503.sv503_scavtimeout		= i->info503->scavtimeout;
			i503.sv503_minrcvqueue		= i->info503->minrcvqueue;
			i503.sv503_minfreeworkitems	= i->info503->minfreeworkitems;
			i503.sv503_xactmemsize		= i->info503->xactmemsize;
			i503.sv503_threadpriority	= i->info503->threadpriority;
			i503.sv503_maxmpxct		= i->info503->maxmpxct;
			i503.sv503_oplockbreakwait	= i->info503->oplockbreakwait;
			i503.sv503_oplockbreakresponsewait = i->info503->oplockbreakresponsewait;
			i503.sv503_enableoplocks	= i->info503->enableoplocks;
			i503.sv503_enableoplockforceclose = i->info503->enableoplockforceclose;
			i503.sv503_enablefcbopens	= i->info503->enablefcbopens;
			i503.sv503_enableraw		= i->info503->enableraw;
			i503.sv503_enablesharednetdrives = i->info503->enablesharednetdrives;
			i503.sv503_minfreeconnections	= i->info503->minfreeconnections;
			i503.sv503_maxfreeconnections	= i->info503->maxfreeconnections;

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_503, i503,
				     (struct SERVER_INFO_503 **)buffer,
				     &num_info);
			break;

		case 599:
			i599.sv599_sessopens		= i->info599->sessopen;
			i599.sv599_sessvcs		= i->info599->sesssvc;
			i599.sv599_opensearch		= i->info599->opensearch;
			i599.sv599_sizreqbuf		= i->info599->sizereqbufs;
			i599.sv599_initworkitems	= i->info599->initworkitems;
			i599.sv599_maxworkitems		= i->info599->maxworkitems;
			i599.sv599_rawworkitems		= i->info599->rawworkitems;
			i599.sv599_irpstacksize		= i->info599->irpstacksize;
			i599.sv599_maxrawbuflen		= i->info599->maxrawbuflen;
			i599.sv599_sessusers		= i->info599->sessusers;
			i599.sv599_sessconns		= i->info599->sessconns;
			i599.sv599_maxpagedmemoryusage	= i->info599->maxpagedmemoryusage;
			i599.sv599_maxnonpagedmemoryusage = i->info599->maxnonpagedmemoryusage;
			i599.sv599_enablesoftcompat	= i->info599->enablesoftcompat;
			i599.sv599_enableforcedlogoff	= i->info599->enableforcedlogoff;
			i599.sv599_timesource		= i->info599->timesource;
			i599.sv599_acceptdownlevelapis	= i->info599->acceptdownlevelapis;
			i599.sv599_lmannounce		= i->info599->lmannounce;
			i599.sv599_domain		= talloc_strdup(mem_ctx, i->info599->domain);
			i599.sv599_maxcopyreadlen	= i->info599->maxcopyreadlen;
			i599.sv599_maxcopywritelen	= i->info599->maxcopywritelen;
			i599.sv599_minkeepsearch	= i->info599->minkeepsearch;
			i599.sv599_maxkeepsearch	= 0; /* ?? */
			i599.sv599_minkeepcomplsearch	= i->info599->minkeepcomplsearch;
			i599.sv599_maxkeepcomplsearch	= i->info599->maxkeepcomplsearch;
			i599.sv599_threadcountadd	= i->info599->threadcountadd;
			i599.sv599_numblockthreads	= i->info599->numlockthreads; /* typo ? */
			i599.sv599_scavtimeout		= i->info599->scavtimeout;
			i599.sv599_minrcvqueue		= i->info599->minrcvqueue;
			i599.sv599_minfreeworkitems	= i->info599->minfreeworkitems;
			i599.sv599_xactmemsize		= i->info599->xactmemsize;
			i599.sv599_threadpriority	= i->info599->threadpriority;
			i599.sv599_maxmpxct		= i->info599->maxmpxct;
			i599.sv599_oplockbreakwait	= i->info599->oplockbreakwait;
			i599.sv599_oplockbreakresponsewait = i->info599->oplockbreakresponsewait;
			i599.sv599_enableoplocks	= i->info599->enableoplocks;
			i599.sv599_enableoplockforceclose = i->info599->enableoplockforceclose;
			i599.sv599_enablefcbopens	= i->info599->enablefcbopens;
			i599.sv599_enableraw		= i->info599->enableraw;
			i599.sv599_enablesharednetdrives = i->info599->enablesharednetdrives;
			i599.sv599_minfreeconnections	= i->info599->minfreeconnections;
			i599.sv599_maxfreeconnections	= i->info599->maxfreeconnections;
			i599.sv599_initsesstable	= i->info599->initsesstable;
			i599.sv599_initconntable	= i->info599->initconntable;
			i599.sv599_initfiletable	= i->info599->initfiletable;
			i599.sv599_initsearchtable	= i->info599->initsearchtable;
			i599.sv599_alertschedule	= i->info599->alertsched;
			i599.sv599_errorthreshold	= i->info599->errortreshold;
			i599.sv599_networkerrorthreshold = i->info599->networkerrortreshold;
			i599.sv599_diskspacethreshold	= i->info599->diskspacetreshold;
			i599.sv599_reserved		= i->info599->reserved;
			i599.sv599_maxlinkdelay		= i->info599->maxlinkdelay;
			i599.sv599_minlinkthroughput	= i->info599->minlinkthroughput;
			i599.sv599_linkinfovalidtime	= i->info599->linkinfovalidtime;
			i599.sv599_scavqosinfoupdatetime = i->info599->scavqosinfoupdatetime;
			i599.sv599_maxworkitemidletime	= i->info599->maxworkitemidletime;

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_599, i599,
				     (struct SERVER_INFO_599 **)buffer,
				     &num_info);
			break;

		case 1005:
			i1005.sv1005_comment		= talloc_strdup(mem_ctx, i->info1005->comment);

			ADD_TO_ARRAY(mem_ctx, struct SERVER_INFO_1005, i1005,
				     (struct SERVER_INFO_1005 **)buffer,
				     &num_info);
			break;
		default:
			return NT_STATUS_NOT_SUPPORTED;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

WERROR NetServerGetInfo_r(struct libnetapi_ctx *ctx,
			  struct NetServerGetInfo *r)
{
	NTSTATUS status;
	WERROR werr;
	union srvsvc_NetSrvInfo info;
	struct dcerpc_binding_handle *b;

	if (!r->out.buffer) {
		return WERR_INVALID_PARAM;
	}

	switch (r->in.level) {
		case 100:
		case 101:
		case 102:
		case 402:
		case 502:
		case 503:
		case 1005:
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_srvsvc_NetSrvGetInfo(b, talloc_tos(),
					     r->in.server_name,
					     r->in.level,
					     &info,
					     &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = map_server_info_to_SERVER_INFO_buffer(ctx, r->in.level, &info,
						       r->out.buffer);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

static WERROR NetServerSetInfo_l_1005(struct libnetapi_ctx *ctx,
				      struct NetServerSetInfo *r)
{
	WERROR werr = WERR_OK;
	sbcErr err;
	struct smbconf_ctx *conf_ctx;
	struct srvsvc_NetSrvInfo1005 *info1005;

	if (!r->in.buffer) {
		*r->out.parm_error = 1005; /* sure here ? */
		return WERR_INVALID_PARAM;
	}

	info1005 = (struct srvsvc_NetSrvInfo1005 *)r->in.buffer;

	if (!info1005->comment) {
		*r->out.parm_error = 1005;
		return WERR_INVALID_PARAM;
	}

#ifdef REGISTRY_BACKEND
	if (!lp_config_backend_is_registry())
#endif
	{
		libnetapi_set_error_string(ctx,
			"Configuration manipulation requested but not "
			"supported by backend");
		return WERR_NOT_SUPPORTED;
	}

	err = smbconf_init_reg(ctx, &conf_ctx, NULL);
	if (!SBC_ERROR_IS_OK(err)) {
		libnetapi_set_error_string(ctx,
			"Could not initialize backend: %s",
			sbcErrorString(err));
		werr = WERR_NO_SUCH_SERVICE;
		goto done;
	}

	err = smbconf_set_global_parameter(conf_ctx, "server string",
					    info1005->comment);
	if (!SBC_ERROR_IS_OK(err)) {
		libnetapi_set_error_string(ctx,
			"Could not set global parameter: %s",
			sbcErrorString(err));
		werr = WERR_NO_SUCH_SERVICE;
		goto done;
	}

 done:
	smbconf_shutdown(conf_ctx);
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetServerSetInfo_l(struct libnetapi_ctx *ctx,
			  struct NetServerSetInfo *r)
{
	switch (r->in.level) {
		case 1005:
			return NetServerSetInfo_l_1005(ctx, r);
		default:
			break;
	}

	return WERR_UNKNOWN_LEVEL;
}

/****************************************************************
****************************************************************/

WERROR NetServerSetInfo_r(struct libnetapi_ctx *ctx,
			  struct NetServerSetInfo *r)
{
	NTSTATUS status;
	WERROR werr;
	union srvsvc_NetSrvInfo info;
	struct dcerpc_binding_handle *b;

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	switch (r->in.level) {
		case 1005:
			info.info1005 = (struct srvsvc_NetSrvInfo1005 *)r->in.buffer;
			break;
		default:
			werr = WERR_NOT_SUPPORTED;
			goto done;
	}

	status = dcerpc_srvsvc_NetSrvSetInfo(b, talloc_tos(),
					     r->in.server_name,
					     r->in.level,
					     &info,
					     r->out.parm_error,
					     &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetRemoteTOD_r(struct libnetapi_ctx *ctx,
		      struct NetRemoteTOD *r)
{
	NTSTATUS status;
	WERROR werr;
	struct srvsvc_NetRemoteTODInfo *info = NULL;
	struct dcerpc_binding_handle *b;

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_srvsvc.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_srvsvc_NetRemoteTOD(b, talloc_tos(),
					    r->in.server_name,
					    &info,
					    &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	*r->out.buffer = (uint8_t *)talloc_memdup(ctx, info,
			  sizeof(struct srvsvc_NetRemoteTODInfo));
	W_ERROR_HAVE_NO_MEMORY(*r->out.buffer);

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetRemoteTOD_l(struct libnetapi_ctx *ctx,
		      struct NetRemoteTOD *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetRemoteTOD);
}


/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Support
 *  Copyright (C) Guenther Deschner 2007-2009
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

#ifndef __LIB_NETAPI_H__
#define __LIB_NETAPI_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/****************************************************************
 NET_API_STATUS
****************************************************************/
typedef enum {
	NET_API_STATUS_SUCCESS = 0
} NET_API_STATUS;

#define ERROR_MORE_DATA	( 234L )

#define ENCRYPTED_PWLEN	( 16 )

/****************************************************************
****************************************************************/

#ifndef _HEADER_misc

struct GUID {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq[2];
	uint8_t node[6];
};

#endif /* _HEADER_misc */

#ifndef _HEADER_libnetapi

#ifndef MAXSUBAUTHS
#define MAXSUBAUTHS 15 /* max sub authorities in a SID */
#endif

struct domsid {
	uint8_t   sid_rev_num;
	uint8_t   num_auths;
	uint8_t   id_auth[6];
	uint32_t  sub_auths[MAXSUBAUTHS];
};

struct DOMAIN_CONTROLLER_INFO {
	const char * domain_controller_name;
	const char * domain_controller_address;
	uint32_t domain_controller_address_type;
	struct GUID domain_guid;
	const char * domain_name;
	const char * dns_forest_name;
	uint32_t flags;
	const char * dc_site_name;
	const char * client_site_name;
};

/* bitmap NetJoinFlags */
#define NETSETUP_JOIN_DOMAIN ( 0x00000001 )
#define NETSETUP_ACCT_CREATE ( 0x00000002 )
#define NETSETUP_ACCT_DELETE ( 0x00000004 )
#define NETSETUP_WIN9X_UPGRADE ( 0x00000010 )
#define NETSETUP_DOMAIN_JOIN_IF_JOINED ( 0x00000020 )
#define NETSETUP_JOIN_UNSECURE ( 0x00000040 )
#define NETSETUP_MACHINE_PWD_PASSED ( 0x00000080 )
#define NETSETUP_DEFER_SPN_SET ( 0x00000100 )
#define NETSETUP_JOIN_DC_ACCOUNT ( 0x00000200 )
#define NETSETUP_JOIN_WITH_NEW_NAME ( 0x00000400 )
#define NETSETUP_INSTALL_INVOCATION ( 0x00040000 )
#define NETSETUP_IGNORE_UNSUPPORTED_FLAGS ( 0x10000000 )

#define FILTER_TEMP_DUPLICATE_ACCOUNT	( 0x0001 )
#define FILTER_NORMAL_ACCOUNT	( 0x0002 )
#define FILTER_INTERDOMAIN_TRUST_ACCOUNT	( 0x0008 )
#define FILTER_WORKSTATION_TRUST_ACCOUNT	( 0x0010 )
#define FILTER_SERVER_TRUST_ACCOUNT	( 0x0020 )

#define TIMEQ_FOREVER  ( (uint32_t)-1L )

enum NETSETUP_JOIN_STATUS {
	NetSetupUnknownStatus=0,
	NetSetupUnjoined=1,
	NetSetupWorkgroupName=2,
	NetSetupDomainName=3
};

struct SERVER_INFO_100 {
	uint32_t sv100_platform_id;
	const char * sv100_name;
};

struct SERVER_INFO_101 {
	uint32_t sv101_platform_id;
	const char * sv101_name;
	uint32_t sv101_version_major;
	uint32_t sv101_version_minor;
	uint32_t sv101_type;
	const char * sv101_comment;
};

struct SERVER_INFO_102 {
	uint32_t sv102_platform_id;
	const char * sv102_name;
	uint32_t sv102_version_major;
	uint32_t sv102_version_minor;
	uint32_t sv102_type;
	const char * sv102_comment;
	uint32_t sv102_users;
	uint32_t sv102_disc;
	uint8_t sv102_hidden;
	uint32_t sv102_announce;
	uint32_t sv102_anndelta;
	uint32_t sv102_licenses;
	const char * sv102_userpath;
};

struct SERVER_INFO_402 {
	uint32_t sv402_ulist_mtime;
	uint32_t sv402_glist_mtime;
	uint32_t sv402_alist_mtime;
	const char * sv402_alerts;
	uint32_t sv402_security;
	uint32_t sv402_numadmin;
	uint32_t sv402_lanmask;
	const char * sv402_guestacct;
	uint32_t sv402_chdevs;
	uint32_t sv402_chdevq;
	uint32_t sv402_chdevjobs;
	uint32_t sv402_connections;
	uint32_t sv402_shares;
	uint32_t sv402_openfiles;
	uint32_t sv402_sessopens;
	uint32_t sv402_sessvcs;
	uint32_t sv402_sessreqs;
	uint32_t sv402_opensearch;
	uint32_t sv402_activelocks;
	uint32_t sv402_numreqbuf;
	uint32_t sv402_sizreqbuf;
	uint32_t sv402_numbigbuf;
	uint32_t sv402_numfiletasks;
	uint32_t sv402_alertsched;
	uint32_t sv402_erroralert;
	uint32_t sv402_logonalert;
	uint32_t sv402_accessalert;
	uint32_t sv402_diskalert;
	uint32_t sv402_netioalert;
	uint32_t sv402_maxauditsz;
	const char * sv402_srvheuristics;
};

struct SERVER_INFO_403 {
	uint32_t sv403_ulist_mtime;
	uint32_t sv403_glist_mtime;
	uint32_t sv403_alist_mtime;
	const char * sv403_alerts;
	uint32_t sv403_security;
	uint32_t sv403_numadmin;
	uint32_t sv403_lanmask;
	const char * sv403_guestacct;
	uint32_t sv403_chdevs;
	uint32_t sv403_chdevq;
	uint32_t sv403_chdevjobs;
	uint32_t sv403_connections;
	uint32_t sv403_shares;
	uint32_t sv403_openfiles;
	uint32_t sv403_sessopens;
	uint32_t sv403_sessvcs;
	uint32_t sv403_sessreqs;
	uint32_t sv403_opensearch;
	uint32_t sv403_activelocks;
	uint32_t sv403_numreqbuf;
	uint32_t sv403_sizreqbuf;
	uint32_t sv403_numbigbuf;
	uint32_t sv403_numfiletasks;
	uint32_t sv403_alertsched;
	uint32_t sv403_erroralert;
	uint32_t sv403_logonalert;
	uint32_t sv403_accessalert;
	uint32_t sv403_diskalert;
	uint32_t sv403_netioalert;
	uint32_t sv403_maxauditsz;
	const char * sv403_srvheuristics;
	uint32_t sv403_auditedevents;
	uint32_t sv403_autoprofile;
	const char * sv403_autopath;
};

struct SERVER_INFO_502 {
	uint32_t sv502_sessopens;
	uint32_t sv502_sessvcs;
	uint32_t sv502_opensearch;
	uint32_t sv502_sizreqbuf;
	uint32_t sv502_initworkitems;
	uint32_t sv502_maxworkitems;
	uint32_t sv502_rawworkitems;
	uint32_t sv502_irpstacksize;
	uint32_t sv502_maxrawbuflen;
	uint32_t sv502_sessusers;
	uint32_t sv502_sessconns;
	uint32_t sv502_maxpagedmemoryusage;
	uint32_t sv502_maxnonpagedmemoryusage;
	uint8_t sv502_enablesoftcompat;
	uint8_t sv502_enableforcedlogoff;
	uint8_t sv502_timesource;
	uint8_t sv502_acceptdownlevelapis;
	uint8_t sv502_lmannounce;
};

struct SERVER_INFO_503 {
	uint32_t sv503_sessopens;
	uint32_t sv503_sessvcs;
	uint32_t sv503_opensearch;
	uint32_t sv503_sizreqbuf;
	uint32_t sv503_initworkitems;
	uint32_t sv503_maxworkitems;
	uint32_t sv503_rawworkitems;
	uint32_t sv503_irpstacksize;
	uint32_t sv503_maxrawbuflen;
	uint32_t sv503_sessusers;
	uint32_t sv503_sessconns;
	uint32_t sv503_maxpagedmemoryusage;
	uint32_t sv503_maxnonpagedmemoryusage;
	uint8_t sv503_enablesoftcompat;
	uint8_t sv503_enableforcedlogoff;
	uint8_t sv503_timesource;
	uint8_t sv503_acceptdownlevelapis;
	uint8_t sv503_lmannounce;
	const char * sv503_domain;
	uint32_t sv503_maxcopyreadlen;
	uint32_t sv503_maxcopywritelen;
	uint32_t sv503_minkeepsearch;
	uint32_t sv503_maxkeepsearch;
	uint32_t sv503_minkeepcomplsearch;
	uint32_t sv503_maxkeepcomplsearch;
	uint32_t sv503_threadcountadd;
	uint32_t sv503_numblockthreads;
	uint32_t sv503_scavtimeout;
	uint32_t sv503_minrcvqueue;
	uint32_t sv503_minfreeworkitems;
	uint32_t sv503_xactmemsize;
	uint32_t sv503_threadpriority;
	uint32_t sv503_maxmpxct;
	uint32_t sv503_oplockbreakwait;
	uint32_t sv503_oplockbreakresponsewait;
	uint8_t sv503_enableoplocks;
	uint8_t sv503_enableoplockforceclose;
	uint8_t sv503_enablefcbopens;
	uint8_t sv503_enableraw;
	uint8_t sv503_enablesharednetdrives;
	uint32_t sv503_minfreeconnections;
	uint32_t sv503_maxfreeconnections;
};

struct SERVER_INFO_599 {
	uint32_t sv599_sessopens;
	uint32_t sv599_sessvcs;
	uint32_t sv599_opensearch;
	uint32_t sv599_sizreqbuf;
	uint32_t sv599_initworkitems;
	uint32_t sv599_maxworkitems;
	uint32_t sv599_rawworkitems;
	uint32_t sv599_irpstacksize;
	uint32_t sv599_maxrawbuflen;
	uint32_t sv599_sessusers;
	uint32_t sv599_sessconns;
	uint32_t sv599_maxpagedmemoryusage;
	uint32_t sv599_maxnonpagedmemoryusage;
	uint8_t sv599_enablesoftcompat;
	uint8_t sv599_enableforcedlogoff;
	uint8_t sv599_timesource;
	uint8_t sv599_acceptdownlevelapis;
	uint8_t sv599_lmannounce;
	const char * sv599_domain;
	uint32_t sv599_maxcopyreadlen;
	uint32_t sv599_maxcopywritelen;
	uint32_t sv599_minkeepsearch;
	uint32_t sv599_maxkeepsearch;
	uint32_t sv599_minkeepcomplsearch;
	uint32_t sv599_maxkeepcomplsearch;
	uint32_t sv599_threadcountadd;
	uint32_t sv599_numblockthreads;
	uint32_t sv599_scavtimeout;
	uint32_t sv599_minrcvqueue;
	uint32_t sv599_minfreeworkitems;
	uint32_t sv599_xactmemsize;
	uint32_t sv599_threadpriority;
	uint32_t sv599_maxmpxct;
	uint32_t sv599_oplockbreakwait;
	uint32_t sv599_oplockbreakresponsewait;
	uint8_t sv599_enableoplocks;
	uint8_t sv599_enableoplockforceclose;
	uint8_t sv599_enablefcbopens;
	uint8_t sv599_enableraw;
	uint8_t sv599_enablesharednetdrives;
	uint32_t sv599_minfreeconnections;
	uint32_t sv599_maxfreeconnections;
	uint32_t sv599_initsesstable;
	uint32_t sv599_initconntable;
	uint32_t sv599_initfiletable;
	uint32_t sv599_initsearchtable;
	uint32_t sv599_alertschedule;
	uint32_t sv599_errorthreshold;
	uint32_t sv599_networkerrorthreshold;
	uint32_t sv599_diskspacethreshold;
	uint32_t sv599_reserved;
	uint32_t sv599_maxlinkdelay;
	uint32_t sv599_minlinkthroughput;
	uint32_t sv599_linkinfovalidtime;
	uint32_t sv599_scavqosinfoupdatetime;
	uint32_t sv599_maxworkitemidletime;
};

struct SERVER_INFO_598 {
	uint32_t sv598_maxrawworkitems;
	uint32_t sv598_maxthreadsperqueue;
	uint32_t sv598_producttype;
	uint32_t sv598_serversize;
	uint32_t sv598_connectionlessautodisc;
	uint32_t sv598_sharingviolationretries;
	uint32_t sv598_sharingviolationdelay;
	uint32_t sv598_maxglobalopensearch;
	uint32_t sv598_removeduplicatesearches;
	uint32_t sv598_lockviolationoffset;
	uint32_t sv598_lockviolationdelay;
	uint32_t sv598_mdlreadswitchover;
	uint32_t sv598_cachedopenlimit;
	uint32_t sv598_otherqueueaffinity;
	uint8_t sv598_restrictnullsessaccess;
	uint8_t sv598_enablewfw311directipx;
	uint32_t sv598_queuesamplesecs;
	uint32_t sv598_balancecount;
	uint32_t sv598_preferredaffinity;
	uint32_t sv598_maxfreerfcbs;
	uint32_t sv598_maxfreemfcbs;
	uint32_t sv598_maxfreelfcbs;
	uint32_t sv598_maxfreepagedpoolchunks;
	uint32_t sv598_minpagedpoolchunksize;
	uint32_t sv598_maxpagedpoolchunksize;
	uint8_t sv598_sendsfrompreferredprocessor;
	uint32_t sv598_cacheddirectorylimit;
	uint32_t sv598_maxcopylength;
	uint8_t sv598_enablecompression;
	uint8_t sv598_autosharewks;
	uint8_t sv598_autoshareserver;
	uint8_t sv598_enablesecuritysignature;
	uint8_t sv598_requiresecuritysignature;
	uint32_t sv598_minclientbuffersize;
	struct GUID sv598_serverguid;
	uint32_t sv598_ConnectionNoSessionsTimeout;
	uint32_t sv598_IdleThreadTimeOut;
	uint8_t sv598_enableW9xsecuritysignature;
	uint8_t sv598_enforcekerberosreauthentication;
	uint8_t sv598_disabledos;
	uint32_t sv598_lowdiskspaceminimum;
	uint8_t sv598_disablestrictnamechecking;
};

struct SERVER_INFO_1005 {
	const char * sv1005_comment;
};

struct SERVER_INFO_1107 {
	uint32_t sv1107_users;
};

struct SERVER_INFO_1010 {
	int32_t sv1010_disc;
};

struct SERVER_INFO_1016 {
	uint8_t sv1016_hidden;
};

struct SERVER_INFO_1017 {
	uint32_t sv1017_announce;
};

struct SERVER_INFO_1018 {
	uint32_t sv1018_anndelta;
};

struct SERVER_INFO_1501 {
	uint32_t sv1501_sessopens;
};

struct SERVER_INFO_1502 {
	uint32_t sv1502_sessvcs;
};

struct SERVER_INFO_1503 {
	uint32_t sv1503_opensearch;
};

struct SERVER_INFO_1506 {
	uint32_t sv1506_maxworkitems;
};

struct SERVER_INFO_1509 {
	uint32_t sv1509_maxrawbuflen;
};

struct SERVER_INFO_1510 {
	uint32_t sv1510_sessusers;
};

struct SERVER_INFO_1511 {
	uint32_t sv1511_sessconns;
};

struct SERVER_INFO_1512 {
	uint32_t sv1512_maxnonpagedmemoryusage;
};

struct SERVER_INFO_1513 {
	uint32_t sv1513_maxpagedmemoryusage;
};

struct SERVER_INFO_1514 {
	uint8_t sv1514_enablesoftcompat;
};

struct SERVER_INFO_1515 {
	uint8_t sv1515_enableforcedlogoff;
};

struct SERVER_INFO_1516 {
	uint8_t sv1516_timesource;
};

struct SERVER_INFO_1518 {
	uint8_t sv1518_lmannounce;
};

struct SERVER_INFO_1520 {
	uint32_t sv1520_maxcopyreadlen;
};

struct SERVER_INFO_1521 {
	uint32_t sv1521_maxcopywritelen;
};

struct SERVER_INFO_1522 {
	uint32_t sv1522_minkeepsearch;
};

struct SERVER_INFO_1523 {
	uint32_t sv1523_maxkeepsearch;
};

struct SERVER_INFO_1524 {
	uint32_t sv1524_minkeepcomplsearch;
};

struct SERVER_INFO_1525 {
	uint32_t sv1525_maxkeepcomplsearch;
};

struct SERVER_INFO_1528 {
	uint32_t sv1528_scavtimeout;
};

struct SERVER_INFO_1529 {
	uint32_t sv1529_minrcvqueue;
};

struct SERVER_INFO_1530 {
	uint32_t sv1530_minfreeworkitems;
};

struct SERVER_INFO_1533 {
	uint32_t sv1533_maxmpxct;
};

struct SERVER_INFO_1534 {
	uint32_t sv1534_oplockbreakwait;
};

struct SERVER_INFO_1535 {
	uint32_t sv1535_oplockbreakresponsewait;
};

struct SERVER_INFO_1536 {
	uint8_t sv1536_enableoplocks;
};

struct SERVER_INFO_1537 {
	uint8_t sv1537_enableoplockforceclose;
};

struct SERVER_INFO_1538 {
	uint8_t sv1538_enablefcbopens;
};

struct SERVER_INFO_1539 {
	uint8_t sv1539_enableraw;
};

struct SERVER_INFO_1540 {
	uint8_t sv1540_enablesharednetdrives;
};

struct SERVER_INFO_1541 {
	uint8_t sv1541_minfreeconnections;
};

struct SERVER_INFO_1542 {
	uint8_t sv1542_maxfreeconnections;
};

struct SERVER_INFO_1543 {
	uint32_t sv1543_initsesstable;
};

struct SERVER_INFO_1544 {
	uint32_t sv1544_initconntable;
};

struct SERVER_INFO_1545 {
	uint32_t sv1545_initfiletable;
};

struct SERVER_INFO_1546 {
	uint32_t sv1546_initsearchtable;
};

struct SERVER_INFO_1547 {
	uint32_t sv1547_alertschedule;
};

struct SERVER_INFO_1548 {
	uint32_t sv1548_errorthreshold;
};

struct SERVER_INFO_1549 {
	uint32_t sv1549_networkerrorthreshold;
};

struct SERVER_INFO_1550 {
	uint32_t sv1550_diskspacethreshold;
};

struct SERVER_INFO_1552 {
	uint32_t sv1552_maxlinkdelay;
};

struct SERVER_INFO_1553 {
	uint32_t sv1553_minlinkthroughput;
};

struct SERVER_INFO_1554 {
	uint32_t sv1554_linkinfovalidtime;
};

struct SERVER_INFO_1555 {
	uint32_t sv1555_scavqosinfoupdatetime;
};

struct SERVER_INFO_1556 {
	uint32_t sv1556_maxworkitemidletime;
};

struct SERVER_INFO_1557 {
	uint32_t sv1557_maxrawworkitems;
};

struct SERVER_INFO_1560 {
	uint32_t sv1560_producttype;
};

struct SERVER_INFO_1561 {
	uint32_t sv1561_serversize;
};

struct SERVER_INFO_1562 {
	uint32_t sv1562_connectionlessautodisc;
};

struct SERVER_INFO_1563 {
	uint32_t sv1563_sharingviolationretries;
};

struct SERVER_INFO_1564 {
	uint32_t sv1564_sharingviolationdelay;
};

struct SERVER_INFO_1565 {
	uint32_t sv1565_maxglobalopensearch;
};

struct SERVER_INFO_1566 {
	uint8_t sv1566_removeduplicatesearches;
};

struct SERVER_INFO_1567 {
	uint32_t sv1567_lockviolationretries;
};

struct SERVER_INFO_1568 {
	uint32_t sv1568_lockviolationoffset;
};

struct SERVER_INFO_1569 {
	uint32_t sv1569_lockviolationdelay;
};

struct SERVER_INFO_1570 {
	uint32_t sv1570_mdlreadswitchover;
};

struct SERVER_INFO_1571 {
	uint32_t sv1571_cachedopenlimit;
};

struct SERVER_INFO_1572 {
	uint32_t sv1572_criticalthreads;
};

struct SERVER_INFO_1573 {
	uint32_t sv1573_restrictnullsessaccess;
};

struct SERVER_INFO_1574 {
	uint32_t sv1574_enablewfw311directipx;
};

struct SERVER_INFO_1575 {
	uint32_t sv1575_otherqueueaffinity;
};

struct SERVER_INFO_1576 {
	uint32_t sv1576_queuesamplesecs;
};

struct SERVER_INFO_1577 {
	uint32_t sv1577_balancecount;
};

struct SERVER_INFO_1578 {
	uint32_t sv1578_preferredaffinity;
};

struct SERVER_INFO_1579 {
	uint32_t sv1579_maxfreerfcbs;
};

struct SERVER_INFO_1580 {
	uint32_t sv1580_maxfreemfcbs;
};

struct SERVER_INFO_1581 {
	uint32_t sv1581_maxfreemlcbs;
};

struct SERVER_INFO_1582 {
	uint32_t sv1582_maxfreepagedpoolchunks;
};

struct SERVER_INFO_1583 {
	uint32_t sv1583_minpagedpoolchunksize;
};

struct SERVER_INFO_1584 {
	uint32_t sv1584_maxpagedpoolchunksize;
};

struct SERVER_INFO_1585 {
	uint8_t sv1585_sendsfrompreferredprocessor;
};

struct SERVER_INFO_1586 {
	uint32_t sv1586_maxthreadsperqueue;
};

struct SERVER_INFO_1587 {
	uint32_t sv1587_cacheddirectorylimit;
};

struct SERVER_INFO_1588 {
	uint32_t sv1588_maxcopylength;
};

struct SERVER_INFO_1590 {
	uint32_t sv1590_enablecompression;
};

struct SERVER_INFO_1591 {
	uint32_t sv1591_autosharewks;
};

struct SERVER_INFO_1592 {
	uint32_t sv1592_autosharewks;
};

struct SERVER_INFO_1593 {
	uint32_t sv1593_enablesecuritysignature;
};

struct SERVER_INFO_1594 {
	uint32_t sv1594_requiresecuritysignature;
};

struct SERVER_INFO_1595 {
	uint32_t sv1595_minclientbuffersize;
};

struct SERVER_INFO_1596 {
	uint32_t sv1596_ConnectionNoSessionsTimeout;
};

struct SERVER_INFO_1597 {
	uint32_t sv1597_IdleThreadTimeOut;
};

struct SERVER_INFO_1598 {
	uint32_t sv1598_enableW9xsecuritysignature;
};

struct SERVER_INFO_1599 {
	uint8_t sv1598_enforcekerberosreauthentication;
};

struct SERVER_INFO_1600 {
	uint8_t sv1598_disabledos;
};

struct SERVER_INFO_1601 {
	uint32_t sv1598_lowdiskspaceminimum;
};

struct SERVER_INFO_1602 {
	uint8_t sv_1598_disablestrictnamechecking;
};

struct USER_INFO_0 {
	const char * usri0_name;
};

#define USER_PRIV_GUEST	( 0 )
#define USER_PRIV_USER	( 1 )
#define USER_PRIV_ADMIN	( 2 )

struct USER_INFO_1 {
	const char * usri1_name;
	const char * usri1_password;
	uint32_t usri1_password_age;
	uint32_t usri1_priv;
	const char * usri1_home_dir;
	const char * usri1_comment;
	uint32_t usri1_flags;
	const char * usri1_script_path;
};

#define AF_OP_PRINT	( 0x1 )
#define AF_OP_COMM	( 0x2 )
#define AF_OP_SERVER	( 0x4 )
#define AF_OP_ACCOUNTS	( 0x8 )

struct USER_INFO_2 {
	const char * usri2_name;
	const char * usri2_password;
	uint32_t usri2_password_age;
	uint32_t usri2_priv;
	const char * usri2_home_dir;
	const char * usri2_comment;
	uint32_t usri2_flags;
	const char * usri2_script_path;
	uint32_t usri2_auth_flags;
	const char * usri2_full_name;
	const char * usri2_usr_comment;
	const char * usri2_parms;
	const char * usri2_workstations;
	uint32_t usri2_last_logon;
	uint32_t usri2_last_logoff;
	uint32_t usri2_acct_expires;
	uint32_t usri2_max_storage;
	uint32_t usri2_units_per_week;
	uint8_t *usri2_logon_hours;/* [unique] */
	uint32_t usri2_bad_pw_count;
	uint32_t usri2_num_logons;
	const char * usri2_logon_server;
	uint32_t usri2_country_code;
	uint32_t usri2_code_page;
};

struct USER_INFO_3 {
	const char * usri3_name;
	uint32_t usri3_password_age;
	uint32_t usri3_priv;
	const char * usri3_home_dir;
	const char * usri3_comment;
	uint32_t usri3_flags;
	const char * usri3_script_path;
	uint32_t usri3_auth_flags;
	const char * usri3_full_name;
	const char * usri3_usr_comment;
	const char * usri3_parms;
	const char * usri3_workstations;
	uint32_t usri3_last_logon;
	uint32_t usri3_last_logoff;
	uint32_t usri3_acct_expires;
	uint32_t usri3_max_storage;
	uint32_t usri3_units_per_week;
	uint8_t *usri3_logon_hours;/* [unique] */
	uint32_t usri3_bad_pw_count;
	uint32_t usri3_num_logons;
	const char * usri3_logon_server;
	uint32_t usri3_country_code;
	uint32_t usri3_code_page;
	uint32_t usri3_user_id;
	uint32_t usri3_primary_group_id;
	const char * usri3_profile;
	const char * usri3_home_dir_drive;
	uint32_t usri3_password_expired;
};

struct USER_INFO_4 {
	const char * usri4_name;
	const char * usri4_password;
	uint32_t usri4_password_age;
	uint32_t usri4_priv;
	const char * usri4_home_dir;
	const char * usri4_comment;
	uint32_t usri4_flags;
	const char * usri4_script_path;
	uint32_t usri4_auth_flags;
	const char * usri4_full_name;
	const char * usri4_usr_comment;
	const char * usri4_parms;
	const char * usri4_workstations;
	uint32_t usri4_last_logon;
	uint32_t usri4_last_logoff;
	uint32_t usri4_acct_expires;
	uint32_t usri4_max_storage;
	uint32_t usri4_units_per_week;
	uint8_t *usri4_logon_hours;/* [unique] */
	uint32_t usri4_bad_pw_count;
	uint32_t usri4_num_logons;
	const char * usri4_logon_server;
	uint32_t usri4_country_code;
	uint32_t usri4_code_page;
	struct domsid *usri4_user_sid;/* [unique] */
	uint32_t usri4_primary_group_id;
	const char * usri4_profile;
	const char * usri4_home_dir_drive;
	uint32_t usri4_password_expired;
};

struct USER_INFO_10 {
	const char * usri10_name;
	const char * usri10_comment;
	const char * usri10_usr_comment;
	const char * usri10_full_name;
};

struct USER_INFO_11 {
	const char * usri11_name;
	const char * usri11_comment;
	const char * usri11_usr_comment;
	const char * usri11_full_name;
	uint32_t usri11_priv;
	uint32_t usri11_auth_flags;
	uint32_t usri11_password_age;
	const char * usri11_home_dir;
	const char * usri11_parms;
	uint32_t usri11_last_logon;
	uint32_t usri11_last_logoff;
	uint32_t usri11_bad_pw_count;
	uint32_t usri11_num_logons;
	const char * usri11_logon_server;
	uint32_t usri11_country_code;
	const char * usri11_workstations;
	uint32_t usri11_max_storage;
	uint32_t usri11_units_per_week;
	uint8_t *usri11_logon_hours;/* [unique] */
	uint32_t usri11_code_page;
};

struct USER_INFO_20 {
	const char * usri20_name;
	const char * usri20_full_name;
	const char * usri20_comment;
	uint32_t usri20_flags;
	uint32_t usri20_user_id;
};

struct USER_INFO_21 {
	uint8_t *usri21_password;
};

struct USER_INFO_22 {
	const char * usri22_name;
	uint8_t *usri22_password;
	uint32_t usri22_password_age;
	uint32_t usri22_priv;
	const char * usri22_home_dir;
	const char * usri22_comment;
	uint32_t usri22_flags;
	uint32_t usri22_script_path;
	uint32_t usri22_auth_flags;
	const char * usri22_full_name;
	const char * usri22_usr_comment;
	const char * usri22_parms;
	const char * usri22_workstations;
	uint32_t usri22_last_logon;
	uint32_t usri22_last_logoff;
	uint32_t usri22_acct_expires;
	uint32_t usri22_max_storage;
	uint32_t usri22_units_per_week;
	uint8_t *usri22_logon_hours;/* [unique] */
	uint32_t usri22_bad_pw_count;
	uint32_t usri22_num_logons;
	const char * usri22_logon_server;
	uint32_t usri22_country_code;
	uint32_t usri22_code_page;
};

struct USER_INFO_23 {
	const char * usri23_name;
	const char * usri23_full_name;
	const char * usri23_comment;
	uint32_t usri23_flags;
	struct domsid *usri23_user_sid;/* [unique] */
};

struct USER_INFO_1003 {
	const char * usri1003_password;
};

struct USER_INFO_1005 {
	uint32_t usri1005_priv;
};

struct USER_INFO_1006 {
	const char * usri1006_home_dir;
};

struct USER_INFO_1007 {
	const char * usri1007_comment;
};

struct USER_INFO_1008 {
	uint32_t usri1008_flags;
};

struct USER_INFO_1009 {
	const char * usri1009_script_path;
};

struct USER_INFO_1010 {
	uint32_t usri1010_auth_flags;
};

struct USER_INFO_1011 {
	const char * usri1011_full_name;
};

struct USER_INFO_1012 {
	const char * usri1012_usr_comment;
};

struct USER_INFO_1013 {
	const char * usri1013_parms;
};

struct USER_INFO_1014 {
	const char * usri1014_workstations;
};

struct USER_INFO_1017 {
	uint32_t usri1017_acct_expires;
};

struct USER_INFO_1018 {
	uint32_t usri1018_max_storage;
};

struct USER_INFO_1020 {
	uint32_t usri1020_units_per_week;
	uint8_t *usri1020_logon_hours;/* [unique] */
};

struct USER_INFO_1023 {
	const char * usri1023_logon_server;
};

struct USER_INFO_1024 {
	uint32_t usri1024_country_code;
};

struct USER_INFO_1025 {
	uint32_t usri1025_code_page;
};

struct USER_INFO_1051 {
	uint32_t usri1051_primary_group_id;
};

struct USER_INFO_1052 {
	const char * usri1052_profile;
};

struct USER_INFO_1053 {
	const char * usri1053_home_dir_drive;
};

struct USER_MODALS_INFO_0 {
	uint32_t usrmod0_min_passwd_len;
	uint32_t usrmod0_max_passwd_age;
	uint32_t usrmod0_min_passwd_age;
	uint32_t usrmod0_force_logoff;
	uint32_t usrmod0_password_hist_len;
};

struct USER_MODALS_INFO_1 {
	uint32_t usrmod1_role;
	const char * usrmod1_primary;
};

struct USER_MODALS_INFO_2 {
	const char * usrmod2_domain_name;
	struct domsid *usrmod2_domain_id;/* [unique] */
};

struct USER_MODALS_INFO_3 {
	uint32_t usrmod3_lockout_duration;
	uint32_t usrmod3_lockout_observation_window;
	uint32_t usrmod3_lockout_threshold;
};

struct USER_MODALS_INFO_1001 {
	uint32_t usrmod1001_min_passwd_len;
};

struct USER_MODALS_INFO_1002 {
	uint32_t usrmod1002_max_passwd_age;
};

struct USER_MODALS_INFO_1003 {
	uint32_t usrmod1003_min_passwd_age;
};

struct USER_MODALS_INFO_1004 {
	uint32_t usrmod1004_force_logoff;
};

struct USER_MODALS_INFO_1005 {
	uint32_t usrmod1005_password_hist_len;
};

struct USER_MODALS_INFO_1006 {
	uint32_t usrmod1006_role;
};

struct USER_MODALS_INFO_1007 {
	const char * usrmod1007_primary;
};

struct NET_DISPLAY_USER {
	const char * usri1_name;
	const char * usri1_comment;
	uint32_t usri1_flags;
	const char * usri1_full_name;
	uint32_t usri1_user_id;
	uint32_t usri1_next_index;
};

struct NET_DISPLAY_MACHINE {
	const char * usri2_name;
	const char * usri2_comment;
	uint32_t usri2_flags;
	uint32_t usri2_user_id;
	uint32_t usri2_next_index;
};

struct NET_DISPLAY_GROUP {
	const char * grpi3_name;
	const char * grpi3_comment;
	uint32_t grpi3_group_id;
	uint32_t grpi3_attributes;
	uint32_t grpi3_next_index;
};

struct GROUP_INFO_0 {
	const char * grpi0_name;
};

struct GROUP_INFO_1 {
	const char * grpi1_name;
	const char * grpi1_comment;
};

struct GROUP_INFO_2 {
	const char * grpi2_name;
	const char * grpi2_comment;
	uint32_t grpi2_group_id;
	uint32_t grpi2_attributes;
};

struct GROUP_INFO_3 {
	const char * grpi3_name;
	const char * grpi3_comment;
	struct domsid * grpi3_group_sid;
	uint32_t grpi3_attributes;
};

struct GROUP_INFO_1002 {
	const char * grpi1002_comment;
};

struct GROUP_INFO_1005 {
	uint32_t grpi1005_attributes;
};

struct GROUP_USERS_INFO_0 {
	const char * grui0_name;
};

struct GROUP_USERS_INFO_1 {
	const char * grui1_name;
	uint32_t grui1_attributes;
};

struct LOCALGROUP_INFO_0 {
	const char * lgrpi0_name;
};

struct LOCALGROUP_INFO_1 {
	const char * lgrpi1_name;
	const char * lgrpi1_comment;
};

struct LOCALGROUP_INFO_1002 {
	const char * lgrpi1002_comment;
};

enum SID_NAME_USE {
	SidTypeUser=1,
	SidTypeGroup=2,
	SidTypeDomain=3,
	SidTypeAlias=4,
	SidTypeWellKnownGroup=5,
	SidTypeDeletedAccount=6,
	SidTypeInvalid=7,
	SidTypeUnknown=8,
	SidTypeComputer=9,
	SidTypeLabel=10
};

struct LOCALGROUP_MEMBERS_INFO_0 {
	struct domsid *lgrmi0_sid;/* [unique] */
};

struct LOCALGROUP_MEMBERS_INFO_1 {
	struct domsid *lgrmi1_sid;/* [unique] */
	enum SID_NAME_USE lgrmi1_sidusage;
	const char * lgrmi1_name;
};

struct LOCALGROUP_MEMBERS_INFO_2 {
	struct domsid *lgrmi2_sid;/* [unique] */
	enum SID_NAME_USE lgrmi2_sidusage;
	const char * lgrmi2_domainandname;
};

struct LOCALGROUP_MEMBERS_INFO_3 {
	const char * lgrmi3_domainandname;
};

struct LOCALGROUP_USERS_INFO_0 {
	const char * lgrui0_name;
};

struct TIME_OF_DAY_INFO {
	uint32_t tod_elapsedt;
	uint32_t tod_msecs;
	uint32_t tod_hours;
	uint32_t tod_mins;
	uint32_t tod_secs;
	uint32_t tod_hunds;
	int32_t tod_timezone;
	uint32_t tod_tinterval;
	uint32_t tod_day;
	uint32_t tod_month;
	uint32_t tod_year;
	uint32_t tod_weekday;
};

struct SHARE_INFO_0 {
	const char * shi0_netname;
};

struct SHARE_INFO_1 {
	const char * shi1_netname;
	uint32_t shi1_type;
	const char * shi1_remark;
};

struct SHARE_INFO_2 {
	const char * shi2_netname;
	uint32_t shi2_type;
	const char * shi2_remark;
	uint32_t shi2_permissions;
	uint32_t shi2_max_uses;
	uint32_t shi2_current_uses;
	const char * shi2_path;
	const char * shi2_passwd;
};

struct SHARE_INFO_501 {
	const char * shi501_netname;
	uint32_t shi501_type;
	const char * shi501_remark;
	uint32_t shi501_flags;
};

struct SHARE_INFO_1004 {
	const char * shi1004_remark;
};

struct SHARE_INFO_1005 {
	uint32_t shi1005_flags;
};

struct SHARE_INFO_1006 {
	uint32_t shi1006_max_uses;
};

struct FILE_INFO_2 {
	uint32_t fi2_id;
};

struct FILE_INFO_3 {
	uint32_t fi3_id;
	uint32_t fi3_permissions;
	uint32_t fi3_num_locks;
	const char * fi3_pathname;
	const char * fi3_username;
};

struct NETLOGON_INFO_1 {
	uint32_t netlog1_flags;
	NET_API_STATUS netlog1_pdc_connection_status;
};

struct NETLOGON_INFO_2 {
	uint32_t netlog2_flags;
	NET_API_STATUS netlog2_pdc_connection_status;
	const char * netlog2_trusted_dc_name;
	NET_API_STATUS netlog2_tc_connection_status;
};

struct NETLOGON_INFO_3 {
	uint32_t netlog1_flags;
	uint32_t netlog3_logon_attempts;
	uint32_t netlog3_reserved1;
	uint32_t netlog3_reserved2;
	uint32_t netlog3_reserved3;
	uint32_t netlog3_reserved4;
	uint32_t netlog3_reserved5;
};

struct NETLOGON_INFO_4 {
	const char * netlog4_trusted_dc_name;
	const char * netlog4_trusted_domain_name;
};

#endif /* _HEADER_libnetapi */

#ifndef _HEADER_netlogon

#define NETLOGON_CONTROL_QUERY ( 0x00000001 )
#define NETLOGON_CONTROL_REPLICATE ( 0x00000002 )
#define NETLOGON_CONTROL_SYNCHRONIZE ( 0x00000003 )
#define NETLOGON_CONTROL_PDC_REPLICATE ( 0x00000004 )
#define NETLOGON_CONTROL_REDISCOVER ( 0x00000005 )
#define NETLOGON_CONTROL_TC_QUERY ( 0x00000006 )
#define NETLOGON_CONTROL_TRANSPORT_NOTIFY ( 0x00000007 )
#define NETLOGON_CONTROL_FIND_USER ( 0x00000008 )
#define NETLOGON_CONTROL_CHANGE_PASSWORD ( 0x00000009 )
#define NETLOGON_CONTROL_TC_VERIFY ( 0x0000000A )
#define NETLOGON_CONTROL_FORCE_DNS_REG ( 0x0000000B )
#define NETLOGON_CONTROL_QUERY_DNS_REG ( 0x0000000C )
#define NETLOGON_CONTROL_BACKUP_CHANGE_LOG ( 0x0000FFFC )
#define NETLOGON_CONTROL_TRUNCATE_LOG ( 0x0000FFFD )
#define NETLOGON_CONTROL_SET_DBFLAG ( 0x0000FFFE )
#define NETLOGON_CONTROL_BREAKPOINT ( 0x0000FFFF )

#endif /* _HEADER_netlogon */

/****************************************************************
****************************************************************/

struct libnetapi_ctx {
	char *debuglevel;
	char *error_string;
	char *username;
	char *workgroup;
	char *password;
	char *krb5_cc_env;
	int use_kerberos;
	int use_ccache;
	int disable_policy_handle_cache;

	void *private_data;
};

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_init(struct libnetapi_ctx **ctx);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_free(struct libnetapi_ctx *ctx);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_getctx(struct libnetapi_ctx **ctx);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_debuglevel(struct libnetapi_ctx *ctx,
					const char *debuglevel);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_username(struct libnetapi_ctx *ctx,
				      const char *username);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_password(struct libnetapi_ctx *ctx,
				      const char *password);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_workgroup(struct libnetapi_ctx *ctx,
				       const char *workgroup);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_use_kerberos(struct libnetapi_ctx *ctx);

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_use_ccache(struct libnetapi_ctx *ctx);

/****************************************************************
****************************************************************/

const char *libnetapi_errstr(NET_API_STATUS status);

/****************************************************************
****************************************************************/

const char *libnetapi_get_error_string(struct libnetapi_ctx *ctx,
				       NET_API_STATUS status);

/****************************************************************
 NetApiBufferAllocate
****************************************************************/

NET_API_STATUS NetApiBufferAllocate(uint32_t byte_count,
				    void **buffer);

/****************************************************************
 NetApiBufferFree
****************************************************************/

NET_API_STATUS NetApiBufferFree(void *buffer);

/************************************************************//**
 *
 * ConvertSidToStringSid
 *
 * @brief Convert a domain sid into a string
 *
 * @param[in] sid A pointer to a sid structure
 * @param[in,out] sid_string A pointer that holds a pointer to a sid string. Caller
 * needs to free with free(3)
 * @return bool
 ***************************************************************/

int  ConvertSidToStringSid(const struct domsid *sid,
			   char **sid_string);

/************************************************************//**
 *
 * ConvertStringSidToSid
 *
 * @brief Convert a string into a domain sid
 *
 * @param[in] sid_string A pointer to a sid string.
 * @param[in,out] sid A pointer that holds a pointer to a sid structure.
 * Caller needs to free with free(3)
 * @return bool
 ***************************************************************/

int ConvertStringSidToSid(const char *sid_string,
			  struct domsid **sid);

/************************************************************//**
 *
 * NetJoinDomain
 *
 * @brief Join a computer to a domain or workgroup
 *
 * @param[in] server The server name to connect to
 * @param[in] domain The domain or workgroup to join
 * @param[in] account_ou The organizational Unit to create the computer account
 * in (AD only)
 * @param[in] account The domain account used for joining a domain
 * @param[in] password The domain account's password used for joining a domain
 * @param[in] join_flags Bitmask field to define specific join features
 * @return NET_API_STATUS
 *
 * example netdomjoin/netdomjoin.c
 ***************************************************************/

NET_API_STATUS NetJoinDomain(const char * server /* [in] */,
			     const char * domain /* [in] [ref] */,
			     const char * account_ou /* [in] */,
			     const char * account /* [in] */,
			     const char * password /* [in] */,
			     uint32_t join_flags /* [in] */);

/************************************************************//**
 *
 * NetUnjoinDomain
 *
 * @brief Unjoin a computer from a domain or workgroup
 *
 * @param[in] server_name The server name to connect to
 * @param[in] account The domain account used for unjoining a domain
 * @param[in] password The domain account's password used for unjoining a domain
 * @param[in] unjoin_flags Bitmask field to define specific unjoin features
 * @return NET_API_STATUS
 *
 ***************************************************************/

NET_API_STATUS NetUnjoinDomain(const char * server_name /* [in] */,
			       const char * account /* [in] */,
			       const char * password /* [in] */,
			       uint32_t unjoin_flags /* [in] */);

/************************************************************//**
 *
 * NetGetJoinInformation
 *
 * @brief Unjoin a computer from a domain or workgroup
 *
 * @param[in] server_name The server name to connect to
 * @param[out] name_buffer Returns the name of the workgroup or domain
 * @param[out] name_type  Returns the type of that name
 * @return NET_API_STATUS
 *
 * example netdomjoin-gui/netdomjoin-gui.c
 *
 ***************************************************************/

NET_API_STATUS NetGetJoinInformation(const char * server_name /* [in] */,
				     const char * *name_buffer /* [out] [ref] */,
				     uint16_t *name_type /* [out] [ref] */);

/************************************************************//**
 *
 * NetGetJoinableOUs
 *
 * @brief Query for the list of joinable organizational Units that can be used
 * for joining AD
 *
 * @param[in] server_name The server name to connect to
 * @param[in] domain The AD domain to query
 * @param[in] account The domain account used for the query
 * @param[in] password The domain account's password used for the query
 * @param[out] ou_count The number of ous returned
 * @param[out] ous Returned string array containing the ous
 * @return NET_API_STATUS
 *
 * example netdomjoin-gui/netdomjoin-gui.c
 *
 ***************************************************************/

NET_API_STATUS NetGetJoinableOUs(const char * server_name /* [in] */,
				 const char * domain /* [in] [ref] */,
				 const char * account /* [in] */,
				 const char * password /* [in] */,
				 uint32_t *ou_count /* [out] [ref] */,
				 const char * **ous /* [out] [ref] */);

/************************************************************//**
 *
 * NetRenameMachineInDomain
 *
 * @brief Rename a machine in a domain
 *
 * @param[in] server_name The server name to connect to
 * @param[in] new_machine_name The new machine name
 * @param[in] account The domain account used for the query
 * @param[in] password The domain account's password used for the query
 * @param[in] rename_options Options used for the rename operation
 * @return NET_API_STATUS
 *
 * example join/rename_machine.c
 *
 ***************************************************************/

NET_API_STATUS NetRenameMachineInDomain(const char * server_name /* [in] */,
					const char * new_machine_name /* [in] */,
					const char * account /* [in] */,
					const char * password /* [in] */,
					uint32_t rename_options /* [in] */);

/************************************************************//**
 *
 * NetServerGetInfo
 *
 * @brief Get Information on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level to define which information is requested
 * @param[out] buffer The returned buffer carrying the SERVER_INFO structure
 * @return NET_API_STATUS
 *
 ***************************************************************/

NET_API_STATUS NetServerGetInfo(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */);

/************************************************************//**
 *
 * NetServerSetInfo
 *
 * @brief Get Information on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level to define which information is set
 * @param[in] buffer The buffer carrying the SERVER_INFO structure
 * @param[out] parm_error On failure returns the invalid SERVER_INFO member
 * @return NET_API_STATUS
 *
 ***************************************************************/

NET_API_STATUS NetServerSetInfo(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_error /* [out] [ref] */);

/************************************************************//**
 *
 * NetGetDCName
 *
 * @brief Query for the PDC for a given domain
 *
 * @param[in] server_name The server name to connect to
 * @param[in] domain_name The name of the domain to lookup
 * @param[out] buffer The name of the domain to lookup
 * @return NET_API_STATUS
 *
 * example getdc/getdc.c
 ***************************************************************/

NET_API_STATUS NetGetDCName(const char * server_name /* [in] */,
			    const char * domain_name /* [in] */,
			    uint8_t **buffer /* [out] [ref] */);

/************************************************************//**
 *
 * NetGetAnyDCName
 *
 * @brief Query for any DC for a given domain
 *
 * @param[in] server_name The server name to connect to
 * @param[in] domain_name The name of the domain to lookup
 * @param[out] buffer The name of the domain to lookup
 * @return NET_API_STATUS
 *
 * example getdc/getdc.c
 ***************************************************************/

NET_API_STATUS NetGetAnyDCName(const char * server_name /* [in] */,
			       const char * domain_name /* [in] */,
			       uint8_t **buffer /* [out] [ref] */);


/************************************************************//**
 *
 * DsGetDcName
 *
 * @brief Lookup a DC for a given domain and return information structure
 *
 * @param[in] server_name The server name to connect to
 * @param[in] domain_name The name of the domain to lookup (cannot be NULL)
 * @param[in] domain_guid The GUID of the domain to lookup (optional)
 * @param[in] site_name The name of the site the DC should reside in
 * @param[in] flags A bitmask to request specific features supported by the DC
 * @param[out] dc_info Pointer to a DOMAIN_CONTROLLER_INFO structure
 * @return NET_API_STATUS
 *
 * example dsgetdc/dsgetdc.c
 ***************************************************************/

NET_API_STATUS DsGetDcName(const char * server_name /* [in] [unique] */,
			   const char * domain_name /* [in] [ref] */,
			   struct GUID *domain_guid /* [in] [unique] */,
			   const char * site_name /* [in] [unique] */,
			   uint32_t flags /* [in] */,
			   struct DOMAIN_CONTROLLER_INFO **dc_info /* [out] [ref] */);

/************************************************************//**
 *
 * NetUserAdd
 *
 * @brief Create a user on a given server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level of the USER_INFO structure passed in (Currently
 * only level 1 is supported)
 * @param[in] buffer The buffer carrying the USER_INFO structure
 * @param[out] parm_error In case of error returns the failing member of the
 * structure
 * @return NET_API_STATUS
 *
 * example user/user_add.c
 ***************************************************************/

NET_API_STATUS NetUserAdd(const char * server_name /* [in] */,
			  uint32_t level /* [in] */,
			  uint8_t *buffer /* [in] [ref] */,
			  uint32_t *parm_error /* [out] [ref] */);

/************************************************************//**
 *
 * NetUserDel
 *
 * @brief Delete a user on a given server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] user_name The user account to delete
 * @return NET_API_STATUS
 *
 * example user/user_del.c
 ***************************************************************/

NET_API_STATUS NetUserDel(const char * server_name /* [in] */,
			  const char * user_name /* [in] */);

/************************************************************//**
 *
 * NetUserEnum
 *
 * @brief Enumerate accounts on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The enumeration level used for the query (Currently only
 * level 0 is supported)
 * @param[in] filter The account flags filter used for the query
 * @param[out] buffer The returned enumeration buffer
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of returned entries
 * @param[out] total_entries The number of total entries
 * @param[in,out] resume_handle A handle passed in and returned for resuming
 * operations
 * @return NET_API_STATUS
 *
 * example user/user_enum.c
 ***************************************************************/

NET_API_STATUS NetUserEnum(const char * server_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint32_t filter /* [in] */,
			   uint8_t **buffer /* [out] [ref] */,
			   uint32_t prefmaxlen /* [in] */,
			   uint32_t *entries_read /* [out] [ref] */,
			   uint32_t *total_entries /* [out] [ref] */,
			   uint32_t *resume_handle /* [in,out] [ref] */);

/************************************************************//**
 *
 * NetUserChangePassword
 *
 * @brief Change the password for a user on a given server or in a given domain
 *
 * @param[in] domain_name The server or domain name to connect to
 * @param[in] user_name The user account to change the password for
 * @param[in] old_password The user account's old password
 * @param[in] new_password The user account's new password
 * @return NET_API_STATUS
 *
 * example user/user_chgpwd.c
 ***************************************************************/

NET_API_STATUS NetUserChangePassword(const char * domain_name /* [in] */,
				     const char * user_name /* [in] */,
				     const char * old_password /* [in] */,
				     const char * new_password /* [in] */);

/************************************************************//**
 *
 * NetUserGetInfo
 *
 * @brief Get User Information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] user_name The name of the user that is going to be queried
 * @param[in] level The level defining the requested USER_INFO_X structure
 * @param[out] buffer The buffer containing a USER_INFO_X structure
 * @return NET_API_STATUS
 *
 * example user/user_getinfo.c
 ***************************************************************/

NET_API_STATUS NetUserGetInfo(const char * server_name /* [in] */,
			      const char * user_name /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t **buffer /* [out] [ref] */);

/************************************************************//**
 *
 * NetUserSetInfo
 *
 * @brief Set User Information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] user_name The name of the user that is going to be modified
 * @param[in] level The level defining the requested USER_INFO_X structure
 * @param[in] buffer The buffer containing a USER_INFO_X structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example user/user_setinfo.c
 ***************************************************************/

NET_API_STATUS NetUserSetInfo(const char * server_name /* [in] */,
			      const char * user_name /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t *buffer /* [in] [ref] */,
			      uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetUserModalsGet
 *
 * @brief Get SAM domain and password information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level defining which USER_MODALS_INFO_X buffer to query
 * @param[out] buffer The returned USER_MODALS_INFO_X buffer
 * @return NET_API_STATUS
 *
 * example user/user_modalsget.c
 ***************************************************************/

NET_API_STATUS NetUserModalsGet(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */);

/************************************************************//**
 *
 * NetUserModalsSet
 *
 * @brief Set SAM domain and password information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level defining which USER_MODALS_INFO_X buffer to query
 * @param[out] buffer The buffer conntaing a USER_MODALS_INFO_X structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example user/user_modalsset.c
 ***************************************************************/

NET_API_STATUS NetUserModalsSet(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetUserGetGroups
 *
 * @brief Enumerate grouplist of a user on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] user_name The user name to query
 * @param[in] level The enumeration level used for the query (Currently only
 * level 0 is supported)
 * @param[out] buffer The returned enumeration buffer
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of returned entries
 * @param[out] total_entries The number of total entries
 * @return NET_API_STATUS
 *
 * example user/user_getgroups.c
 ***************************************************************/

NET_API_STATUS NetUserGetGroups(const char * server_name /* [in] */,
				const char * user_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */,
				uint32_t prefmaxlen /* [in] */,
				uint32_t *entries_read /* [out] [ref] */,
				uint32_t *total_entries /* [out] [ref] */);

/************************************************************//**
 *
 * NetUserSetGroups
 *
 * @brief Set grouplist of a user on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] user_name The user name to query
 * @param[in] level The level defining the GROUP_USERS_INFO_X structures in the buffer
 * @param[in] buffer The buffer containing GROUP_USERS_INFO_X structures
 * @param[in] num_entries The number of X structures in the buffer
 * @return NET_API_STATUS
 *
 * example user/user_setgroups.c
 ***************************************************************/

NET_API_STATUS NetUserSetGroups(const char * server_name /* [in] */,
				const char * user_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t num_entries /* [in] */);

/************************************************************//**
 *
 * NetUserGetLocalGroups
 *
 * @brief Enumerate local grouplist of a user on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] user_name The user name to query
 * @param[in] level The enumeration level used for the query
 * @param[in] flags The flags used for the query
 * @param[out] buffer The returned enumeration buffer
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of returned entries
 * @param[out] total_entries The number of total entries
 * @return NET_API_STATUS
 *
 * example user/user_getlocalgroups.c
 ***************************************************************/

NET_API_STATUS NetUserGetLocalGroups(const char * server_name /* [in] */,
				     const char * user_name /* [in] */,
				     uint32_t level /* [in] */,
				     uint32_t flags /* [in] */,
				     uint8_t **buffer /* [out] [ref] */,
				     uint32_t prefmaxlen /* [in] */,
				     uint32_t *entries_read /* [out] [ref] */,
				     uint32_t *total_entries /* [out] [ref] */);

/************************************************************//**
 *
 * NetQueryDisplayInformation
 *
 * @brief Enumerate accounts on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The enumeration level used for the query
 * @param[in] idx The index to start the the display enumeration at
 * @param[in] entries_requested The number of entries requested
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of returned entries
 * @param[out] buffer The returned display information buffer
 * @return NET_API_STATUS
 *
 * example user/user_dispinfo.c
 ***************************************************************/

NET_API_STATUS NetQueryDisplayInformation(const char * server_name /* [in] [unique] */,
					  uint32_t level /* [in] */,
					  uint32_t idx /* [in] */,
					  uint32_t entries_requested /* [in] */,
					  uint32_t prefmaxlen /* [in] */,
					  uint32_t *entries_read /* [out] [ref] */,
					  void **buffer /* [out] [noprint,ref] */);

/************************************************************//**
 *
 * NetGroupAdd
 *
 * @brief Create Domain Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level used for the new group creation
 * @param[in] buf The buffer containing the group structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example group/group_add.c
 ***************************************************************/

NET_API_STATUS NetGroupAdd(const char * server_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t *buf /* [in] [ref] */,
			   uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetGroupDel
 *
 * @brief Delete Domain Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be deleted
 * @return NET_API_STATUS
 *
 * example group/group_del.c
 ***************************************************************/

NET_API_STATUS NetGroupDel(const char * server_name /* [in] */,
			   const char * group_name /* [in] */);

/************************************************************//**
 *
 * NetGroupEnum
 *
 * @brief Enumerate groups on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The enumeration level used for the query (Currently only
 * level 0 is supported)
 * @param[out] buffer The returned enumeration buffer
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of returned entries
 * @param[out] total_entries The number of total entries
 * @param[in,out] resume_handle A handle passed in and returned for resuming
 * operations
 * @return NET_API_STATUS
 *
 * example group/group_enum.c
 ***************************************************************/

NET_API_STATUS NetGroupEnum(const char * server_name /* [in] */,
			    uint32_t level /* [in] */,
			    uint8_t **buffer /* [out] [ref] */,
			    uint32_t prefmaxlen /* [in] */,
			    uint32_t *entries_read /* [out] [ref] */,
			    uint32_t *total_entries /* [out] [ref] */,
			    uint32_t *resume_handle /* [in,out] [ref] */);

/************************************************************//**
 *
 * NetGroupSetInfo
 *
 * @brief Set Domain Group Information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be modified
 * @param[in] level The level defining the structure type in buf
 * @param[in] buf The buffer containing a GROUP_INFO_X structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example group/group_setinfo.c
 ***************************************************************/

NET_API_STATUS NetGroupSetInfo(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t *buf /* [in] [ref] */,
			       uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetGroupGetInfo
 *
 * @brief Get Domain Group Information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be queried
 * @param[in] level The level defining the requested GROUP_INFO_X structure
 * @param[out] buf The buffer containing a GROUP_INFO_X structure
 * @return NET_API_STATUS
 *
 * example group/group_getinfo.c
 ***************************************************************/

NET_API_STATUS NetGroupGetInfo(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t **buf /* [out] [ref] */);

/************************************************************//**
 *
 * NetGroupAddUser
 *
 * @brief Add existing User to existing Domain Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be modified
 * @param[in] user_name The name of the user that is going to be added to the
 * group
 * @return NET_API_STATUS
 *
 * example group/group_adduser.c
 ***************************************************************/

NET_API_STATUS NetGroupAddUser(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       const char * user_name /* [in] */);

/************************************************************//**
 *
 * NetGroupDelUser
 *
 * @brief Remove User from Domain Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be modified
 * @param[in] user_name The name of the user that is going to be removed from
 * the group
 * @return NET_API_STATUS
 *
 * example group/group_deluser.c
 ***************************************************************/

NET_API_STATUS NetGroupDelUser(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       const char * user_name /* [in] */);

/************************************************************//**
 *
 * NetGroupGetUsers
 *
 * @brief Get Users for a group on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The group name to enumerate users for
 * @param[in] level The enumeration level used for the query
 * @param[out] buffer The returned enumeration buffer
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of returned entries
 * @param[out] total_entries The number of total entries
 * @param[in,out] resume_handle A handle passed in and returned for resuming
 * operations
 * @return NET_API_STATUS
 *
 * example group/group_getusers.c
 ***************************************************************/

NET_API_STATUS NetGroupGetUsers(const char * server_name /* [in] */,
				const char * group_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */,
				uint32_t prefmaxlen /* [in] */,
				uint32_t *entries_read /* [out] [ref] */,
				uint32_t *total_entries /* [out] [ref] */,
				uint32_t *resume_handle /* [in,out] [ref] */);

/************************************************************//**
 *
 * NetGroupSetUsers
 *
 * @brief Set Users for a group on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The group name to enumerate users for
 * @param[in] level The enumeration level used for the query
 * @param[in] buffer The buffer containing a X structure
 * @param[in] num_entries The number of X entries in the buffer
 * @return NET_API_STATUS
 *
 * example group/group_setusers.c
 ***************************************************************/

NET_API_STATUS NetGroupSetUsers(const char * server_name /* [in] */,
				const char * group_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t num_entries /* [in] */);

/************************************************************//**
 *
 * NetLocalGroupAdd
 *
 * @brief Create Local Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level used for the new group creation
 * @param[in] buf The buffer containing the group structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_add.c
 ***************************************************************/

NET_API_STATUS NetLocalGroupAdd(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buf /* [in] [ref] */,
				uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetLocalGroupDel
 *
 * @brief Delete Local Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be deleted
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_del.c
 ***************************************************************/


NET_API_STATUS NetLocalGroupDel(const char * server_name /* [in] */,
				const char * group_name /* [in] */);

/************************************************************//**
 *
 * NetLocalGroupGetInfo
 *
 * @brief Get Local Group Information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be queried
 * @param[in] level The level defining the requested LOCALGROUP_INFO_X structure
 * @param[out] buf The buffer containing a LOCALGROUP_INFO_X structure
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_getinfo.c
 ***************************************************************/

NET_API_STATUS NetLocalGroupGetInfo(const char * server_name /* [in] */,
				    const char * group_name /* [in] */,
				    uint32_t level /* [in] */,
				    uint8_t **buf /* [out] [ref] */);

/************************************************************//**
 *
 * NetLocalGroupSetInfo
 *
 * @brief Set Local Group Information
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to be modified
 * @param[in] level The level defining the requested LOCALGROUP_INFO_X structure
 * @param[in] buf The buffer containing a LOCALGROUP_INFO_X structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_setinfo.c
 ***************************************************************/


NET_API_STATUS NetLocalGroupSetInfo(const char * server_name /* [in] */,
				    const char * group_name /* [in] */,
				    uint32_t level /* [in] */,
				    uint8_t *buf /* [in] [ref] */,
				    uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetLocalGroupEnum
 *
 * @brief Enumerate local groups on a server
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The enumeration level used for the query (Currently only
 * level 0 is supported)
 * @param[out] buffer The returned enumeration buffer
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of returned entries
 * @param[out] total_entries The number of total entries
 * @param[in,out] resume_handle A handle passed in and returned for resuming
 * operations
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_enum.c
 ***************************************************************/

NET_API_STATUS NetLocalGroupEnum(const char * server_name /* [in] */,
				 uint32_t level /* [in] */,
				 uint8_t **buffer /* [out] [ref] */,
				 uint32_t prefmaxlen /* [in] */,
				 uint32_t *entries_read /* [out] [ref] */,
				 uint32_t *total_entries /* [out] [ref] */,
				 uint32_t *resume_handle /* [in,out] [ref] */);

/************************************************************//**
 *
 * NetLocalGroupAddMembers
 *
 * @brief Add Members to a Local Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to modified
 * @param[in] level The level defining the LOCALGROUP_MEMBERS_INFO_X structure
 * @param[in] buffer The buffer containing a LOCALGROUP_MEMBERS_INFO_X structure
 * @param[in] total_entries The number of LOCALGROUP_MEMBERS_INFO_X entries in
 * the buffer
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_addmembers.c
 ***************************************************************/

NET_API_STATUS NetLocalGroupAddMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */);

/************************************************************//**
 *
 * NetLocalGroupDelMembers
 *
 * @brief Delete Members from a Local Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to modified
 * @param[in] level The level defining the LOCALGROUP_MEMBERS_INFO_X structure
 * @param[in] buffer The buffer containing a LOCALGROUP_MEMBERS_INFO_X structure
 * @param[in] total_entries The number of LOCALGROUP_MEMBERS_INFO_X entries in
 * the buffer
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_delmembers.c
 ***************************************************************/

NET_API_STATUS NetLocalGroupDelMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */);

/************************************************************//**
 *
 * NetLocalGroupGetMembers
 *
 * @brief Enumerate Members in a local group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] local_group_name The localgroup that is going to be queried
 * @param[in] level The level defining the LOCALGROUP_MEMBERS_INFO_X structure
 * @param[out] buffer The buffer containing a LOCALGROUP_MEMBERS_INFO_X
 * structure
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of LOCALGROUP_MEMBERS_INFO_X entries in the buffer
 * @param[out] total_entries The total number of LOCALGROUP_MEMBERS_INFO_X entries for that group
 * @param[in,out] resume_handle A handle passed in and returned for resuming
 * operations
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_getmembers.c
 ***************************************************************/

NET_API_STATUS NetLocalGroupGetMembers(const char * server_name /* [in] */,
				       const char * local_group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t **buffer /* [out] [ref] */,
				       uint32_t prefmaxlen /* [in] */,
				       uint32_t *entries_read /* [out] [ref] */,
				       uint32_t *total_entries /* [out] [ref] */,
				       uint32_t *resume_handle /* [in,out] [ref] */);

/************************************************************//**
 *
 * NetLocalGroupSetMembers
 *
 * @brief Set Members in a Local Group
 *
 * @param[in] server_name The server name to connect to
 * @param[in] group_name The name of the group that is going to modified
 * @param[in] level The level defining the LOCALGROUP_MEMBERS_INFO_X structure
 * @param[in] buffer The buffer containing a LOCALGROUP_MEMBERS_INFO_X structure
 * @param[in] total_entries The number of LOCALGROUP_MEMBERS_INFO_X entries in
 * the buffer
 * @return NET_API_STATUS
 *
 * example localgroup/localgroup_setmembers.c
 ***************************************************************/

NET_API_STATUS NetLocalGroupSetMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */);

/************************************************************//**
 *
 * NetRemoteTOD
 *
 * @brief Query remote Time of Day
 *
 * @param[in] server_name The server name to connect to
 * @param[out] buf The buffer containing a TIME_OF_DAY_INFO structure
 * @return NET_API_STATUS
 *
 * example server/remote_tod.c
 ***************************************************************/

NET_API_STATUS NetRemoteTOD(const char * server_name /* [in] */,
			    uint8_t **buf /* [out] [ref] */);

/************************************************************//**
 *
 * NetShareAdd
 *
 * @brief Add Share
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level defining the requested SHARE_INFO_X structure
 * @param[in] buffer The buffer containing a SHARE_INFO_X structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example share/share_add.c
 ***************************************************************/

NET_API_STATUS NetShareAdd(const char * server_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t *buffer /* [in] [ref] */,
			   uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetShareDel
 *
 * @brief Delete Share
 *
 * @param[in] server_name The server name to connect to
 * @param[in] net_name The name of the share to delete
 * @param[in] reserved
 * @return NET_API_STATUS
 *
 * example share/share_del.c
 ***************************************************************/

NET_API_STATUS NetShareDel(const char * server_name /* [in] */,
			   const char * net_name /* [in] */,
			   uint32_t reserved /* [in] */);

/************************************************************//**
 *
 * NetShareEnum
 *
 * @brief Enumerate Shares
 *
 * @param[in] server_name The server name to connect to
 * @param[in] level The level defining the SHARE_INFO_X structure
 * @param[out] buffer The buffer containing a SHARE_INFO_X structure
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of SHARE_INFO_X entries in the buffer
 * @param[out] total_entries The total number of SHARE_INFO_X entries
 * @param[in,out] resume_handle A handle passed in and returned for resuming
 * operations
 * @return NET_API_STATUS
 *
 * example share/share_enum.c
 ***************************************************************/

NET_API_STATUS NetShareEnum(const char * server_name /* [in] */,
			    uint32_t level /* [in] */,
			    uint8_t **buffer /* [out] [ref] */,
			    uint32_t prefmaxlen /* [in] */,
			    uint32_t *entries_read /* [out] [ref] */,
			    uint32_t *total_entries /* [out] [ref] */,
			    uint32_t *resume_handle /* [in,out] [ref] */);

/************************************************************//**
 *
 * NetShareGetInfo
 *
 * @brief Get Share Info
 *
 * @param[in] server_name The server name to connect to
 * @param[in] net_name The name of the share to query
 * @param[in] level The level defining the SHARE_INFO_X structure
 * @param[out] buffer The buffer containing a SHARE_INFO_X structure
 * @return NET_API_STATUS
 *
 * example share/share_getinfo.c
 ***************************************************************/

NET_API_STATUS NetShareGetInfo(const char * server_name /* [in] */,
			       const char * net_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t **buffer /* [out] [ref] */);

/************************************************************//**
 *
 * NetShareSetInfo
 *
 * @brief Set Share Info
 *
 * @param[in] server_name The server name to connect to
 * @param[in] net_name The name of the share to query
 * @param[in] level The level defining the SHARE_INFO_X structure
 * @param[in] buffer The buffer containing a SHARE_INFO_X structure
 * @param[out] parm_err The returned parameter error number if any
 * @return NET_API_STATUS
 *
 * example share/share_setinfo.c
 ***************************************************************/

NET_API_STATUS NetShareSetInfo(const char * server_name /* [in] */,
			       const char * net_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t *buffer /* [in] [ref] */,
			       uint32_t *parm_err /* [out] [ref] */);

/************************************************************//**
 *
 * NetFileClose
 *
 * @brief Close a file
 *
 * @param[in] server_name The server name to connect to
 * @param[in] fileid The fileid of the file that is going to be closed
 * @return NET_API_STATUS
 *
 * example file/file_close.c
 ***************************************************************/

NET_API_STATUS NetFileClose(const char * server_name /* [in] */,
			    uint32_t fileid /* [in] */);

/************************************************************//**
 *
 * NetFileGetInfo
 *
 * @brief Close a file
 *
 * @param[in] server_name The server name to connect to
 * @param[in] fileid The fileid of the file that is going to be closed
 * @param[in] level The level of the FILE_INFO_X buffer
 * @param[out] buffer The buffer containing a FILE_INFO_X structure
 * @return NET_API_STATUS
 *
 * example file/file_getinfo.c
 ***************************************************************/

NET_API_STATUS NetFileGetInfo(const char * server_name /* [in] */,
			      uint32_t fileid /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t **buffer /* [out] [ref] */);

/************************************************************//**
 *
 * NetFileEnum
 *
 * @brief Enumerate Files
 *
 * @param[in] server_name The server name to connect to
 * @param[in] base_path The
 * @param[in] user_name The
 * @param[in] level The level defining the FILE_INFO_X structure
 * @param[out] buffer The buffer containing a FILE_INFO_X structure
 * @param[in] prefmaxlen The requested maximal buffer size
 * @param[out] entries_read The number of FILE_INFO_X entries in the buffer
 * @param[out] total_entries The total number of FILE_INFO_X entries
 * @param[in,out] resume_handle A handle passed in and returned for resuming
 * operations
 * @return NET_API_STATUS
 *
 * example file/file_enum.c
 ***************************************************************/

NET_API_STATUS NetFileEnum(const char * server_name /* [in] */,
			   const char * base_path /* [in] */,
			   const char * user_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t **buffer /* [out] [ref] */,
			   uint32_t prefmaxlen /* [in] */,
			   uint32_t *entries_read /* [out] [ref] */,
			   uint32_t *total_entries /* [out] [ref] */,
			   uint32_t *resume_handle /* [in,out] [ref] */);

/************************************************************//**
 *
 * NetShutdownInit
 *
 * @brief Start a machine shutdown
 *
 * @param[in] server_name The server name to connect to
 * @param[in] message The message that is displayed before the shutdown
 * @param[in] timeout The amount of seconds to wait until shutting down
 * @param[in] force_apps Whether to close all applications before the shutdown
 * @param[in] do_reboot Whether to reboot after the shutdown
 * @return NET_API_STATUS
 *
 * example shutdown/shutdown_init.c
 ***************************************************************/

NET_API_STATUS NetShutdownInit(const char * server_name /* [in] */,
			       const char * message /* [in] */,
			       uint32_t timeout /* [in] */,
			       uint8_t force_apps /* [in] */,
			       uint8_t do_reboot /* [in] */);

/************************************************************//**
 *
 * NetShutdownAbort
 *
 * @brief Abort an initiated machine shutdown
 *
 * @param[in] server_name The server name to connect to
 * @return NET_API_STATUS
 *
 * example shutdown/shutdown_abort.c
 ***************************************************************/

NET_API_STATUS NetShutdownAbort(const char * server_name /* [in] */);

/************************************************************//**
 *
 * I_NetLogonControl
 *
 * @brief Control various aspects of the NETLOGON service
 *
 * @param[in] server_name The server name to connect to
 * @param[in] function_code The function code to call on the server
 * @param[in] query_level The level of the NETLOGON_INFO structure returned
 * @param[out] buffer The returned buffer containing the NETLOGON_INFO structure
 * @return NET_API_STATUS
 *
 * example netlogon/netlogon_control.c
 ***************************************************************/

NET_API_STATUS I_NetLogonControl(const char * server_name /* [in] */,
				 uint32_t function_code /* [in] */,
				 uint32_t query_level /* [in] */,
				 uint8_t **buffer /* [out] [ref] */);

/************************************************************//**
 *
 * I_NetLogonControl2
 *
 * @brief Control various aspects of the NETLOGON service
 *
 * @param[in] server_name The server name to connect to
 * @param[in] function_code The function code to call on the server
 * @param[in] query_level The level of the NETLOGON_INFO structure returned
 * @param[in] data The buffer containing information related to the function code
 * @param[out] buffer The returned buffer containing the NETLOGON_INFO structure
 * @return NET_API_STATUS
 *
 * example netlogon/netlogon_control2.c
 ***************************************************************/

NET_API_STATUS I_NetLogonControl2(const char * server_name /* [in] */,
				  uint32_t function_code /* [in] */,
				  uint32_t query_level /* [in] */,
				  uint8_t *data /* [in] [ref] */,
				  uint8_t **buffer /* [out] [ref] */);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LIB_NETAPI_H__ */

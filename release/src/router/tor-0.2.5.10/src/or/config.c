/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file config.c
 * \brief Code to parse and interpret configuration files.
 **/

#define CONFIG_PRIVATE
#include "or.h"
#include "addressmap.h"
#include "channel.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuitmux.h"
#include "circuitmux_ewma.h"
#include "config.h"
#include "connection.h"
#include "connection_edge.h"
#include "connection_or.h"
#include "control.h"
#include "confparse.h"
#include "cpuworker.h"
#include "dirserv.h"
#include "dirvote.h"
#include "dns.h"
#include "entrynodes.h"
#include "geoip.h"
#include "hibernate.h"
#include "main.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "policies.h"
#include "relay.h"
#include "rendclient.h"
#include "rendservice.h"
#include "rephist.h"
#include "router.h"
#include "sandbox.h"
#include "util.h"
#include "routerlist.h"
#include "routerset.h"
#include "statefile.h"
#include "transports.h"
#include "ext_orport.h"
#include "torgzip.h"
#ifdef _WIN32
#include <shlobj.h>
#endif

#include "procmon.h"

/* From main.c */
extern int quiet_level;

/** A list of abbreviations and aliases to map command-line options, obsolete
 * option names, or alternative option names, to their current values. */
static config_abbrev_t option_abbrevs_[] = {
  PLURAL(AuthDirBadDirCC),
  PLURAL(AuthDirBadExitCC),
  PLURAL(AuthDirInvalidCC),
  PLURAL(AuthDirRejectCC),
  PLURAL(ExitNode),
  PLURAL(EntryNode),
  PLURAL(ExcludeNode),
  PLURAL(FirewallPort),
  PLURAL(LongLivedPort),
  PLURAL(HiddenServiceNode),
  PLURAL(HiddenServiceExcludeNode),
  PLURAL(NumCPU),
  PLURAL(RendNode),
  PLURAL(RendExcludeNode),
  PLURAL(StrictEntryNode),
  PLURAL(StrictExitNode),
  PLURAL(StrictNode),
  { "l", "Log", 1, 0},
  { "AllowUnverifiedNodes", "AllowInvalidNodes", 0, 0},
  { "AutomapHostSuffixes", "AutomapHostsSuffixes", 0, 0},
  { "AutomapHostOnResolve", "AutomapHostsOnResolve", 0, 0},
  { "BandwidthRateBytes", "BandwidthRate", 0, 0},
  { "BandwidthBurstBytes", "BandwidthBurst", 0, 0},
  { "DirFetchPostPeriod", "StatusFetchPeriod", 0, 0},
  { "DirServer", "DirAuthority", 0, 0}, /* XXXX024 later, make this warn? */
  { "MaxConn", "ConnLimit", 0, 1},
  { "MaxMemInCellQueues", "MaxMemInQueues", 0, 0},
  { "ORBindAddress", "ORListenAddress", 0, 0},
  { "DirBindAddress", "DirListenAddress", 0, 0},
  { "SocksBindAddress", "SocksListenAddress", 0, 0},
  { "UseHelperNodes", "UseEntryGuards", 0, 0},
  { "NumHelperNodes", "NumEntryGuards", 0, 0},
  { "UseEntryNodes", "UseEntryGuards", 0, 0},
  { "NumEntryNodes", "NumEntryGuards", 0, 0},
  { "ResolvConf", "ServerDNSResolvConfFile", 0, 1},
  { "SearchDomains", "ServerDNSSearchDomains", 0, 1},
  { "ServerDNSAllowBrokenResolvConf", "ServerDNSAllowBrokenConfig", 0, 0},
  { "PreferTunnelledDirConns", "PreferTunneledDirConns", 0, 0},
  { "BridgeAuthoritativeDirectory", "BridgeAuthoritativeDir", 0, 0},
  { "HashedControlPassword", "__HashedControlSessionPassword", 1, 0},
  { "StrictEntryNodes", "StrictNodes", 0, 1},
  { "StrictExitNodes", "StrictNodes", 0, 1},
  { "VirtualAddrNetwork", "VirtualAddrNetworkIPv4", 0, 0},
  { "_UseFilteringSSLBufferevents", "UseFilteringSSLBufferevents", 0, 1},
  { NULL, NULL, 0, 0},
};

/** An entry for config_vars: "The option <b>name</b> has type
 * CONFIG_TYPE_<b>conftype</b>, and corresponds to
 * or_options_t.<b>member</b>"
 */
#define VAR(name,conftype,member,initvalue)                             \
  { name, CONFIG_TYPE_ ## conftype, STRUCT_OFFSET(or_options_t, member), \
      initvalue }
/** As VAR, but the option name and member name are the same. */
#define V(member,conftype,initvalue)                                    \
  VAR(#member, conftype, member, initvalue)
/** An entry for config_vars: "The option <b>name</b> is obsolete." */
#define OBSOLETE(name) { name, CONFIG_TYPE_OBSOLETE, 0, NULL }

#define VPORT(member,conftype,initvalue)                                    \
  VAR(#member, conftype, member ## _lines, initvalue)

/** Array of configuration options.  Until we disallow nonstandard
 * abbreviations, order is significant, since the first matching option will
 * be chosen first.
 */
static config_var_t option_vars_[] = {
  OBSOLETE("AccountingMaxKB"),
  V(AccountingMax,               MEMUNIT,  "0 bytes"),
  V(AccountingStart,             STRING,   NULL),
  V(Address,                     STRING,   NULL),
  V(AllowDotExit,                BOOL,     "0"),
  V(AllowInvalidNodes,           CSV,      "middle,rendezvous"),
  V(AllowNonRFC953Hostnames,     BOOL,     "0"),
  V(AllowSingleHopCircuits,      BOOL,     "0"),
  V(AllowSingleHopExits,         BOOL,     "0"),
  V(AlternateBridgeAuthority,    LINELIST, NULL),
  V(AlternateDirAuthority,       LINELIST, NULL),
  OBSOLETE("AlternateHSAuthority"),
  V(AssumeReachable,             BOOL,     "0"),
  V(AuthDirBadDir,               LINELIST, NULL),
  V(AuthDirBadDirCCs,            CSV,      ""),
  V(AuthDirBadExit,              LINELIST, NULL),
  V(AuthDirBadExitCCs,           CSV,      ""),
  V(AuthDirInvalid,              LINELIST, NULL),
  V(AuthDirInvalidCCs,           CSV,      ""),
  V(AuthDirFastGuarantee,        MEMUNIT,  "100 KB"),
  V(AuthDirGuardBWGuarantee,     MEMUNIT,  "2 MB"),
  V(AuthDirReject,               LINELIST, NULL),
  V(AuthDirRejectCCs,            CSV,      ""),
  V(AuthDirRejectUnlisted,       BOOL,     "0"),
  V(AuthDirListBadDirs,          BOOL,     "0"),
  V(AuthDirListBadExits,         BOOL,     "0"),
  V(AuthDirMaxServersPerAddr,    UINT,     "2"),
  V(AuthDirMaxServersPerAuthAddr,UINT,     "5"),
  V(AuthDirHasIPv6Connectivity,  BOOL,     "0"),
  VAR("AuthoritativeDirectory",  BOOL, AuthoritativeDir,    "0"),
  V(AutomapHostsOnResolve,       BOOL,     "0"),
  V(AutomapHostsSuffixes,        CSV,      ".onion,.exit"),
  V(AvoidDiskWrites,             BOOL,     "0"),
  V(BandwidthBurst,              MEMUNIT,  "1 GB"),
  V(BandwidthRate,               MEMUNIT,  "1 GB"),
  V(BridgeAuthoritativeDir,      BOOL,     "0"),
  VAR("Bridge",                  LINELIST, Bridges,    NULL),
  V(BridgePassword,              STRING,   NULL),
  V(BridgeRecordUsageByCountry,  BOOL,     "1"),
  V(BridgeRelay,                 BOOL,     "0"),
  V(CellStatistics,              BOOL,     "0"),
  V(LearnCircuitBuildTimeout,    BOOL,     "1"),
  V(CircuitBuildTimeout,         INTERVAL, "0"),
  V(CircuitIdleTimeout,          INTERVAL, "1 hour"),
  V(CircuitStreamTimeout,        INTERVAL, "0"),
  V(CircuitPriorityHalflife,     DOUBLE,  "-100.0"), /*negative:'Use default'*/
  V(ClientDNSRejectInternalAddresses, BOOL,"1"),
  V(ClientOnly,                  BOOL,     "0"),
  V(ClientPreferIPv6ORPort,      BOOL,     "0"),
  V(ClientRejectInternalAddresses, BOOL,   "1"),
  V(ClientTransportPlugin,       LINELIST, NULL),
  V(ClientUseIPv6,               BOOL,     "0"),
  V(ConsensusParams,             STRING,   NULL),
  V(ConnLimit,                   UINT,     "1000"),
  V(ConnDirectionStatistics,     BOOL,     "0"),
  V(ConstrainedSockets,          BOOL,     "0"),
  V(ConstrainedSockSize,         MEMUNIT,  "8192"),
  V(ContactInfo,                 STRING,   NULL),
  V(ControlListenAddress,        LINELIST, NULL),
  VPORT(ControlPort,                 LINELIST, NULL),
  V(ControlPortFileGroupReadable,BOOL,     "0"),
  V(ControlPortWriteToFile,      FILENAME, NULL),
  V(ControlSocket,               LINELIST, NULL),
  V(ControlSocketsGroupWritable, BOOL,     "0"),
  V(CookieAuthentication,        BOOL,     "0"),
  V(CookieAuthFileGroupReadable, BOOL,     "0"),
  V(CookieAuthFile,              STRING,   NULL),
  V(CountPrivateBandwidth,       BOOL,     "0"),
  V(DataDirectory,               FILENAME, NULL),
  OBSOLETE("DebugLogFile"),
  V(DisableNetwork,              BOOL,     "0"),
  V(DirAllowPrivateAddresses,    BOOL,     "0"),
  V(TestingAuthDirTimeToLearnReachability, INTERVAL, "30 minutes"),
  V(DirListenAddress,            LINELIST, NULL),
  OBSOLETE("DirFetchPeriod"),
  V(DirPolicy,                   LINELIST, NULL),
  VPORT(DirPort,                     LINELIST, NULL),
  V(DirPortFrontPage,            FILENAME, NULL),
  OBSOLETE("DirPostPeriod"),
  OBSOLETE("DirRecordUsageByCountry"),
  OBSOLETE("DirRecordUsageGranularity"),
  OBSOLETE("DirRecordUsageRetainIPs"),
  OBSOLETE("DirRecordUsageSaveInterval"),
  V(DirReqStatistics,            BOOL,     "1"),
  VAR("DirAuthority",            LINELIST, DirAuthorities, NULL),
  V(DirAuthorityFallbackRate,    DOUBLE,   "1.0"),
  V(DisableAllSwap,              BOOL,     "0"),
  V(DisableDebuggerAttachment,   BOOL,     "1"),
  V(DisableIOCP,                 BOOL,     "1"),
  OBSOLETE("DisableV2DirectoryInfo_"),
  V(DynamicDHGroups,             BOOL,     "0"),
  VPORT(DNSPort,                     LINELIST, NULL),
  V(DNSListenAddress,            LINELIST, NULL),
  V(DownloadExtraInfo,           BOOL,     "0"),
  V(TestingEnableConnBwEvent,    BOOL,     "0"),
  V(TestingEnableCellStatsEvent, BOOL,     "0"),
  V(TestingEnableTbEmptyEvent,   BOOL,     "0"),
  V(EnforceDistinctSubnets,      BOOL,     "1"),
  V(EntryNodes,                  ROUTERSET,   NULL),
  V(EntryStatistics,             BOOL,     "0"),
  V(TestingEstimatedDescriptorPropagationTime, INTERVAL, "10 minutes"),
  V(ExcludeNodes,                ROUTERSET, NULL),
  V(ExcludeExitNodes,            ROUTERSET, NULL),
  V(ExcludeSingleHopRelays,      BOOL,     "1"),
  V(ExitNodes,                   ROUTERSET, NULL),
  V(ExitPolicy,                  LINELIST, NULL),
  V(ExitPolicyRejectPrivate,     BOOL,     "1"),
  V(ExitPortStatistics,          BOOL,     "0"),
  V(ExtendAllowPrivateAddresses, BOOL,     "0"),
  VPORT(ExtORPort,               LINELIST, NULL),
  V(ExtORPortCookieAuthFile,     STRING,   NULL),
  V(ExtORPortCookieAuthFileGroupReadable, BOOL, "0"),
  V(ExtraInfoStatistics,         BOOL,     "1"),
  V(FallbackDir,                 LINELIST, NULL),

  OBSOLETE("FallbackNetworkstatusFile"),
  V(FascistFirewall,             BOOL,     "0"),
  V(FirewallPorts,               CSV,      ""),
  V(FastFirstHopPK,              AUTOBOOL, "auto"),
  V(FetchDirInfoEarly,           BOOL,     "0"),
  V(FetchDirInfoExtraEarly,      BOOL,     "0"),
  V(FetchServerDescriptors,      BOOL,     "1"),
  V(FetchHidServDescriptors,     BOOL,     "1"),
  V(FetchUselessDescriptors,     BOOL,     "0"),
  OBSOLETE("FetchV2Networkstatus"),
  V(GeoIPExcludeUnknown,         AUTOBOOL, "auto"),
#ifdef _WIN32
  V(GeoIPFile,                   FILENAME, "<default>"),
  V(GeoIPv6File,                 FILENAME, "<default>"),
#else
  V(GeoIPFile,                   FILENAME,
    SHARE_DATADIR PATH_SEPARATOR "tor" PATH_SEPARATOR "geoip"),
  V(GeoIPv6File,                 FILENAME,
    SHARE_DATADIR PATH_SEPARATOR "tor" PATH_SEPARATOR "geoip6"),
#endif
  OBSOLETE("GiveGuardFlagTo_CVE_2011_2768_VulnerableRelays"),
  OBSOLETE("Group"),
  V(GuardLifetime,               INTERVAL, "0 minutes"),
  V(HardwareAccel,               BOOL,     "0"),
  V(HeartbeatPeriod,             INTERVAL, "6 hours"),
  V(AccelName,                   STRING,   NULL),
  V(AccelDir,                    FILENAME, NULL),
  V(HashedControlPassword,       LINELIST, NULL),
  V(HidServDirectoryV2,          BOOL,     "1"),
  VAR("HiddenServiceDir",    LINELIST_S, RendConfigLines,    NULL),
  OBSOLETE("HiddenServiceExcludeNodes"),
  OBSOLETE("HiddenServiceNodes"),
  VAR("HiddenServiceOptions",LINELIST_V, RendConfigLines,    NULL),
  VAR("HiddenServicePort",   LINELIST_S, RendConfigLines,    NULL),
  VAR("HiddenServiceVersion",LINELIST_S, RendConfigLines,    NULL),
  VAR("HiddenServiceAuthorizeClient",LINELIST_S,RendConfigLines, NULL),
  V(HidServAuth,                 LINELIST, NULL),
  OBSOLETE("HSAuthoritativeDir"),
  OBSOLETE("HSAuthorityRecordStats"),
  V(CloseHSClientCircuitsImmediatelyOnTimeout, BOOL, "0"),
  V(CloseHSServiceRendCircuitsImmediatelyOnTimeout, BOOL, "0"),
  V(HTTPProxy,                   STRING,   NULL),
  V(HTTPProxyAuthenticator,      STRING,   NULL),
  V(HTTPSProxy,                  STRING,   NULL),
  V(HTTPSProxyAuthenticator,     STRING,   NULL),
  V(IPv6Exit,                    BOOL,     "0"),
  VAR("ServerTransportPlugin",   LINELIST, ServerTransportPlugin,  NULL),
  V(ServerTransportListenAddr,   LINELIST, NULL),
  V(ServerTransportOptions,      LINELIST, NULL),
  V(Socks4Proxy,                 STRING,   NULL),
  V(Socks5Proxy,                 STRING,   NULL),
  V(Socks5ProxyUsername,         STRING,   NULL),
  V(Socks5ProxyPassword,         STRING,   NULL),
  OBSOLETE("IgnoreVersion"),
  V(KeepalivePeriod,             INTERVAL, "5 minutes"),
  VAR("Log",                     LINELIST, Logs,             NULL),
  V(LogMessageDomains,           BOOL,     "0"),
  OBSOLETE("LinkPadding"),
  OBSOLETE("LogLevel"),
  OBSOLETE("LogFile"),
  V(LogTimeGranularity,          MSEC_INTERVAL, "1 second"),
  V(LongLivedPorts,              CSV,
        "21,22,706,1863,5050,5190,5222,5223,6523,6667,6697,8300"),
  VAR("MapAddress",              LINELIST, AddressMap,           NULL),
  V(MaxAdvertisedBandwidth,      MEMUNIT,  "1 GB"),
  V(MaxCircuitDirtiness,         INTERVAL, "10 minutes"),
  V(MaxClientCircuitsPending,    UINT,     "32"),
  VAR("MaxMemInQueues",          MEMUNIT,   MaxMemInQueues_raw, "0"),
  OBSOLETE("MaxOnionsPending"),
  V(MaxOnionQueueDelay,          MSEC_INTERVAL, "1750 msec"),
  V(MinMeasuredBWsForAuthToIgnoreAdvertised, INT, "500"),
  OBSOLETE("MonthlyAccountingStart"),
  V(MyFamily,                    STRING,   NULL),
  V(NewCircuitPeriod,            INTERVAL, "30 seconds"),
  VAR("NamingAuthoritativeDirectory",BOOL, NamingAuthoritativeDir, "0"),
  V(NATDListenAddress,           LINELIST, NULL),
  VPORT(NATDPort,                    LINELIST, NULL),
  V(Nickname,                    STRING,   NULL),
  V(PredictedPortsRelevanceTime,  INTERVAL, "1 hour"),
  V(WarnUnsafeSocks,              BOOL,     "1"),
  OBSOLETE("NoPublish"),
  VAR("NodeFamily",              LINELIST, NodeFamilies,         NULL),
  V(NumCPUs,                     UINT,     "0"),
  V(NumDirectoryGuards,          UINT,     "0"),
  V(NumEntryGuards,              UINT,     "0"),
  V(ORListenAddress,             LINELIST, NULL),
  VPORT(ORPort,                      LINELIST, NULL),
  V(OutboundBindAddress,         LINELIST,   NULL),

  OBSOLETE("PathBiasDisableRate"),
  V(PathBiasCircThreshold,       INT,      "-1"),
  V(PathBiasNoticeRate,          DOUBLE,   "-1"),
  V(PathBiasWarnRate,            DOUBLE,   "-1"),
  V(PathBiasExtremeRate,         DOUBLE,   "-1"),
  V(PathBiasScaleThreshold,      INT,      "-1"),
  OBSOLETE("PathBiasScaleFactor"),
  OBSOLETE("PathBiasMultFactor"),
  V(PathBiasDropGuards,          AUTOBOOL, "0"),
  OBSOLETE("PathBiasUseCloseCounts"),

  V(PathBiasUseThreshold,       INT,      "-1"),
  V(PathBiasNoticeUseRate,          DOUBLE,   "-1"),
  V(PathBiasExtremeUseRate,         DOUBLE,   "-1"),
  V(PathBiasScaleUseThreshold,      INT,      "-1"),

  V(PathsNeededToBuildCircuits,  DOUBLE,   "-1"),
  OBSOLETE("PathlenCoinWeight"),
  V(PerConnBWBurst,              MEMUNIT,  "0"),
  V(PerConnBWRate,               MEMUNIT,  "0"),
  V(PidFile,                     STRING,   NULL),
  V(TestingTorNetwork,           BOOL,     "0"),
  V(TestingMinExitFlagThreshold, MEMUNIT,  "0"),
  V(TestingMinFastFlagThreshold, MEMUNIT,  "0"),
  V(OptimisticData,              AUTOBOOL, "auto"),
  V(PortForwarding,              BOOL,     "0"),
  V(PortForwardingHelper,        FILENAME, "tor-fw-helper"),
  OBSOLETE("PreferTunneledDirConns"),
  V(ProtocolWarnings,            BOOL,     "0"),
  V(PublishServerDescriptor,     CSV,      "1"),
  V(PublishHidServDescriptors,   BOOL,     "1"),
  V(ReachableAddresses,          LINELIST, NULL),
  V(ReachableDirAddresses,       LINELIST, NULL),
  V(ReachableORAddresses,        LINELIST, NULL),
  V(RecommendedVersions,         LINELIST, NULL),
  V(RecommendedClientVersions,   LINELIST, NULL),
  V(RecommendedServerVersions,   LINELIST, NULL),
  OBSOLETE("RedirectExit"),
  V(RefuseUnknownExits,          AUTOBOOL, "auto"),
  V(RejectPlaintextPorts,        CSV,      ""),
  V(RelayBandwidthBurst,         MEMUNIT,  "0"),
  V(RelayBandwidthRate,          MEMUNIT,  "0"),
  OBSOLETE("RendExcludeNodes"),
  OBSOLETE("RendNodes"),
  V(RendPostPeriod,              INTERVAL, "1 hour"),
  V(RephistTrackTime,            INTERVAL, "24 hours"),
  OBSOLETE("RouterFile"),
  V(RunAsDaemon,                 BOOL,     "0"),
//  V(RunTesting,                  BOOL,     "0"),
  OBSOLETE("RunTesting"), // currently unused
  V(Sandbox,                     BOOL,     "0"),
  V(SafeLogging,                 STRING,   "1"),
  V(SafeSocks,                   BOOL,     "0"),
  V(ServerDNSAllowBrokenConfig,  BOOL,     "1"),
  V(ServerDNSAllowNonRFC953Hostnames, BOOL,"0"),
  V(ServerDNSDetectHijacking,    BOOL,     "1"),
  V(ServerDNSRandomizeCase,      BOOL,     "1"),
  V(ServerDNSResolvConfFile,     STRING,   NULL),
  V(ServerDNSSearchDomains,      BOOL,     "0"),
  V(ServerDNSTestAddresses,      CSV,
      "www.google.com,www.mit.edu,www.yahoo.com,www.slashdot.org"),
  V(ShutdownWaitLength,          INTERVAL, "30 seconds"),
  V(SocksListenAddress,          LINELIST, NULL),
  V(SocksPolicy,                 LINELIST, NULL),
  VPORT(SocksPort,                   LINELIST, NULL),
  V(SocksTimeout,                INTERVAL, "2 minutes"),
  V(SSLKeyLifetime,              INTERVAL, "0"),
  OBSOLETE("StatusFetchPeriod"),
  V(StrictNodes,                 BOOL,     "0"),
  V(Support022HiddenServices,    AUTOBOOL, "auto"),
  OBSOLETE("SysLog"),
  V(TestSocks,                   BOOL,     "0"),
  OBSOLETE("TestVia"),
  V(TokenBucketRefillInterval,   MSEC_INTERVAL, "100 msec"),
  V(Tor2webMode,                 BOOL,     "0"),
  V(TLSECGroup,                  STRING,   NULL),
  V(TrackHostExits,              CSV,      NULL),
  V(TrackHostExitsExpire,        INTERVAL, "30 minutes"),
  OBSOLETE("TrafficShaping"),
  V(TransListenAddress,          LINELIST, NULL),
  VPORT(TransPort,                   LINELIST, NULL),
  V(TransProxyType,              STRING,   "default"),
  OBSOLETE("TunnelDirConns"),
  V(UpdateBridgesFromAuthority,  BOOL,     "0"),
  V(UseBridges,                  BOOL,     "0"),
  V(UseEntryGuards,              BOOL,     "1"),
  V(UseEntryGuardsAsDirGuards,   BOOL,     "1"),
  V(UseMicrodescriptors,         AUTOBOOL, "auto"),
  V(UseNTorHandshake,            AUTOBOOL, "1"),
  V(User,                        STRING,   NULL),
  V(UserspaceIOCPBuffers,        BOOL,     "0"),
  OBSOLETE("V1AuthoritativeDirectory"),
  OBSOLETE("V2AuthoritativeDirectory"),
  VAR("V3AuthoritativeDirectory",BOOL, V3AuthoritativeDir,   "0"),
  V(TestingV3AuthInitialVotingInterval, INTERVAL, "30 minutes"),
  V(TestingV3AuthInitialVoteDelay, INTERVAL, "5 minutes"),
  V(TestingV3AuthInitialDistDelay, INTERVAL, "5 minutes"),
  V(TestingV3AuthVotingStartOffset, INTERVAL, "0"),
  V(V3AuthVotingInterval,        INTERVAL, "1 hour"),
  V(V3AuthVoteDelay,             INTERVAL, "5 minutes"),
  V(V3AuthDistDelay,             INTERVAL, "5 minutes"),
  V(V3AuthNIntervalsValid,       UINT,     "3"),
  V(V3AuthUseLegacyKey,          BOOL,     "0"),
  V(V3BandwidthsFile,            FILENAME, NULL),
  VAR("VersioningAuthoritativeDirectory",BOOL,VersioningAuthoritativeDir, "0"),
  V(VirtualAddrNetworkIPv4,      STRING,   "127.192.0.0/10"),
  V(VirtualAddrNetworkIPv6,      STRING,   "[FE80::]/10"),
  V(WarnPlaintextPorts,          CSV,      "23,109,110,143"),
  V(UseFilteringSSLBufferevents, BOOL,    "0"),
  VAR("__ReloadTorrcOnSIGHUP",   BOOL,  ReloadTorrcOnSIGHUP,      "1"),
  VAR("__AllDirActionsPrivate",  BOOL,  AllDirActionsPrivate,     "0"),
  VAR("__DisablePredictedCircuits",BOOL,DisablePredictedCircuits, "0"),
  VAR("__LeaveStreamsUnattached",BOOL,  LeaveStreamsUnattached,   "0"),
  VAR("__HashedControlSessionPassword", LINELIST, HashedControlSessionPassword,
      NULL),
  VAR("__OwningControllerProcess",STRING,OwningControllerProcess, NULL),
  V(MinUptimeHidServDirectoryV2, INTERVAL, "25 hours"),
  V(VoteOnHidServDirectoriesV2,  BOOL,     "1"),
  V(TestingServerDownloadSchedule, CSV_INTERVAL, "0, 0, 0, 60, 60, 120, "
                                 "300, 900, 2147483647"),
  V(TestingClientDownloadSchedule, CSV_INTERVAL, "0, 0, 60, 300, 600, "
                                 "2147483647"),
  V(TestingServerConsensusDownloadSchedule, CSV_INTERVAL, "0, 0, 60, "
                                 "300, 600, 1800, 1800, 1800, 1800, "
                                 "1800, 3600, 7200"),
  V(TestingClientConsensusDownloadSchedule, CSV_INTERVAL, "0, 0, 60, "
                                 "300, 600, 1800, 3600, 3600, 3600, "
                                 "10800, 21600, 43200"),
  V(TestingBridgeDownloadSchedule, CSV_INTERVAL, "3600, 900, 900, 3600"),
  V(TestingClientMaxIntervalWithoutRequest, INTERVAL, "10 minutes"),
  V(TestingDirConnectionMaxStall, INTERVAL, "5 minutes"),
  V(TestingConsensusMaxDownloadTries, UINT, "8"),
  V(TestingDescriptorMaxDownloadTries, UINT, "8"),
  V(TestingMicrodescMaxDownloadTries, UINT, "8"),
  V(TestingCertMaxDownloadTries, UINT, "8"),
  V(TestingDirAuthVoteGuard, ROUTERSET, NULL),
  VAR("___UsingTestNetworkDefaults", BOOL, UsingTestNetworkDefaults_, "0"),

  { NULL, CONFIG_TYPE_OBSOLETE, 0, NULL }
};

/** Override default values with these if the user sets the TestingTorNetwork
 * option. */
static const config_var_t testing_tor_network_defaults[] = {
  V(ServerDNSAllowBrokenConfig,  BOOL,     "1"),
  V(DirAllowPrivateAddresses,    BOOL,     "1"),
  V(EnforceDistinctSubnets,      BOOL,     "0"),
  V(AssumeReachable,             BOOL,     "1"),
  V(AuthDirMaxServersPerAddr,    UINT,     "0"),
  V(AuthDirMaxServersPerAuthAddr,UINT,     "0"),
  V(ClientDNSRejectInternalAddresses, BOOL,"0"),
  V(ClientRejectInternalAddresses, BOOL,   "0"),
  V(CountPrivateBandwidth,       BOOL,     "1"),
  V(ExitPolicyRejectPrivate,     BOOL,     "0"),
  V(ExtendAllowPrivateAddresses, BOOL,     "1"),
  V(V3AuthVotingInterval,        INTERVAL, "5 minutes"),
  V(V3AuthVoteDelay,             INTERVAL, "20 seconds"),
  V(V3AuthDistDelay,             INTERVAL, "20 seconds"),
  V(TestingV3AuthInitialVotingInterval, INTERVAL, "5 minutes"),
  V(TestingV3AuthInitialVoteDelay, INTERVAL, "20 seconds"),
  V(TestingV3AuthInitialDistDelay, INTERVAL, "20 seconds"),
  V(TestingV3AuthVotingStartOffset, INTERVAL, "0"),
  V(TestingAuthDirTimeToLearnReachability, INTERVAL, "0 minutes"),
  V(TestingEstimatedDescriptorPropagationTime, INTERVAL, "0 minutes"),
  V(MinUptimeHidServDirectoryV2, INTERVAL, "0 minutes"),
  V(TestingServerDownloadSchedule, CSV_INTERVAL, "0, 0, 0, 5, 10, 15, "
                                 "20, 30, 60"),
  V(TestingClientDownloadSchedule, CSV_INTERVAL, "0, 0, 5, 10, 15, 20, "
                                 "30, 60"),
  V(TestingServerConsensusDownloadSchedule, CSV_INTERVAL, "0, 0, 5, 10, "
                                 "15, 20, 30, 60"),
  V(TestingClientConsensusDownloadSchedule, CSV_INTERVAL, "0, 0, 5, 10, "
                                 "15, 20, 30, 60"),
  V(TestingBridgeDownloadSchedule, CSV_INTERVAL, "60, 30, 30, 60"),
  V(TestingClientMaxIntervalWithoutRequest, INTERVAL, "5 seconds"),
  V(TestingDirConnectionMaxStall, INTERVAL, "30 seconds"),
  V(TestingConsensusMaxDownloadTries, UINT, "80"),
  V(TestingDescriptorMaxDownloadTries, UINT, "80"),
  V(TestingMicrodescMaxDownloadTries, UINT, "80"),
  V(TestingCertMaxDownloadTries, UINT, "80"),
  V(TestingEnableConnBwEvent,    BOOL,     "1"),
  V(TestingEnableCellStatsEvent, BOOL,     "1"),
  V(TestingEnableTbEmptyEvent,   BOOL,     "1"),
  VAR("___UsingTestNetworkDefaults", BOOL, UsingTestNetworkDefaults_, "1"),

  { NULL, CONFIG_TYPE_OBSOLETE, 0, NULL }
};

#undef VAR
#undef V
#undef OBSOLETE

#ifdef _WIN32
static char *get_windows_conf_root(void);
#endif
static int options_act_reversible(const or_options_t *old_options, char **msg);
static int options_act(const or_options_t *old_options);
static int options_transition_allowed(const or_options_t *old,
                                      const or_options_t *new,
                                      char **msg);
static int options_transition_affects_workers(
      const or_options_t *old_options, const or_options_t *new_options);
static int options_transition_affects_descriptor(
      const or_options_t *old_options, const or_options_t *new_options);
static int check_nickname_list(char **lst, const char *name, char **msg);

static int parse_client_transport_line(const or_options_t *options,
                                       const char *line, int validate_only);

static int parse_server_transport_line(const or_options_t *options,
                                       const char *line, int validate_only);
static char *get_bindaddr_from_transport_listen_line(const char *line,
                                                     const char *transport);
static int parse_dir_authority_line(const char *line,
                                 dirinfo_type_t required_type,
                                 int validate_only);
static int parse_dir_fallback_line(const char *line,
                                   int validate_only);
static void port_cfg_free(port_cfg_t *port);
static int parse_ports(or_options_t *options, int validate_only,
                              char **msg_out, int *n_ports_out);
static int check_server_ports(const smartlist_t *ports,
                              const or_options_t *options);

static int validate_data_directory(or_options_t *options);
static int write_configuration_file(const char *fname,
                                    const or_options_t *options);
static int options_init_logs(or_options_t *options, int validate_only);

static void init_libevent(const or_options_t *options);
static int opt_streq(const char *s1, const char *s2);
static int parse_outbound_addresses(or_options_t *options, int validate_only,
                                    char **msg);
static void config_maybe_load_geoip_files_(const or_options_t *options,
                                           const or_options_t *old_options);
static int options_validate_cb(void *old_options, void *options,
                               void *default_options,
                               int from_setconf, char **msg);
static uint64_t compute_real_max_mem_in_queues(const uint64_t val,
                                               int log_guess);

/** Magic value for or_options_t. */
#define OR_OPTIONS_MAGIC 9090909

/** Configuration format for or_options_t. */
STATIC config_format_t options_format = {
  sizeof(or_options_t),
  OR_OPTIONS_MAGIC,
  STRUCT_OFFSET(or_options_t, magic_),
  option_abbrevs_,
  option_vars_,
  options_validate_cb,
  NULL
};

/*
 * Functions to read and write the global options pointer.
 */

/** Command-line and config-file options. */
static or_options_t *global_options = NULL;
/** The fallback options_t object; this is where we look for options not
 * in torrc before we fall back to Tor's defaults. */
static or_options_t *global_default_options = NULL;
/** Name of most recently read torrc file. */
static char *torrc_fname = NULL;
/** Name of the most recently read torrc-defaults file.*/
static char *torrc_defaults_fname;
/** Configuration options set by command line. */
static config_line_t *global_cmdline_options = NULL;
/** Non-configuration options set by the command line */
static config_line_t *global_cmdline_only_options = NULL;
/** Boolean: Have we parsed the command line? */
static int have_parsed_cmdline = 0;
/** Contents of most recently read DirPortFrontPage file. */
static char *global_dirfrontpagecontents = NULL;
/** List of port_cfg_t for all configured ports. */
static smartlist_t *configured_ports = NULL;

/** Return the contents of our frontpage string, or NULL if not configured. */
const char *
get_dirportfrontpage(void)
{
  return global_dirfrontpagecontents;
}

/** Return the currently configured options. */
or_options_t *
get_options_mutable(void)
{
  tor_assert(global_options);
  return global_options;
}

/** Returns the currently configured options */
MOCK_IMPL(const or_options_t *,
get_options,(void))
{
  return get_options_mutable();
}

/** Change the current global options to contain <b>new_val</b> instead of
 * their current value; take action based on the new value; free the old value
 * as necessary.  Returns 0 on success, -1 on failure.
 */
int
set_options(or_options_t *new_val, char **msg)
{
  int i;
  smartlist_t *elements;
  config_line_t *line;
  or_options_t *old_options = global_options;
  global_options = new_val;
  /* Note that we pass the *old* options below, for comparison. It
   * pulls the new options directly out of global_options. */
  if (options_act_reversible(old_options, msg)<0) {
    tor_assert(*msg);
    global_options = old_options;
    return -1;
  }
  if (options_act(old_options) < 0) { /* acting on the options failed. die. */
    log_err(LD_BUG,
            "Acting on config options left us in a broken state. Dying.");
    exit(1);
  }
  /* Issues a CONF_CHANGED event to notify controller of the change. If Tor is
   * just starting up then the old_options will be undefined. */
  if (old_options && old_options != global_options) {
    elements = smartlist_new();
    for (i=0; options_format.vars[i].name; ++i) {
      const config_var_t *var = &options_format.vars[i];
      const char *var_name = var->name;
      if (var->type == CONFIG_TYPE_LINELIST_S ||
          var->type == CONFIG_TYPE_OBSOLETE) {
        continue;
      }
      if (!config_is_same(&options_format, new_val, old_options, var_name)) {
        line = config_get_assigned_option(&options_format, new_val,
                                          var_name, 1);

        if (line) {
          config_line_t *next;
          for (; line; line = next) {
            next = line->next;
            smartlist_add(elements, line->key);
            smartlist_add(elements, line->value);
            tor_free(line);
          }
        } else {
          smartlist_add(elements, tor_strdup(options_format.vars[i].name));
          smartlist_add(elements, NULL);
        }
      }
    }
    control_event_conf_changed(elements);
    SMARTLIST_FOREACH(elements, char *, cp, tor_free(cp));
    smartlist_free(elements);
  }

  if (old_options != global_options)
    config_free(&options_format, old_options);

  return 0;
}

extern const char tor_git_revision[]; /* from tor_main.c */

/** The version of this Tor process, as parsed. */
static char *the_tor_version = NULL;
/** A shorter version of this Tor process's version, for export in our router
 *  descriptor.  (Does not include the git version, if any.) */
static char *the_short_tor_version = NULL;

/** Return the current Tor version. */
const char *
get_version(void)
{
  if (the_tor_version == NULL) {
    if (strlen(tor_git_revision)) {
      tor_asprintf(&the_tor_version, "%s (git-%s)", get_short_version(),
                   tor_git_revision);
    } else {
      the_tor_version = tor_strdup(get_short_version());
    }
  }
  return the_tor_version;
}

/** Return the current Tor version, without any git tag. */
const char *
get_short_version(void)
{

  if (the_short_tor_version == NULL) {
#ifdef TOR_BUILD_TAG
    tor_asprintf(&the_short_tor_version, "%s (%s)", VERSION, TOR_BUILD_TAG);
#else
    the_short_tor_version = tor_strdup(VERSION);
#endif
  }
  return the_short_tor_version;
}

/** Release additional memory allocated in options
 */
STATIC void
or_options_free(or_options_t *options)
{
  if (!options)
    return;

  routerset_free(options->ExcludeExitNodesUnion_);
  if (options->NodeFamilySets) {
    SMARTLIST_FOREACH(options->NodeFamilySets, routerset_t *,
                      rs, routerset_free(rs));
    smartlist_free(options->NodeFamilySets);
  }
  tor_free(options->BridgePassword_AuthDigest_);
  tor_free(options->command_arg);
  config_free(&options_format, options);
}

/** Release all memory and resources held by global configuration structures.
 */
void
config_free_all(void)
{
  or_options_free(global_options);
  global_options = NULL;
  or_options_free(global_default_options);
  global_default_options = NULL;

  config_free_lines(global_cmdline_options);
  global_cmdline_options = NULL;

  config_free_lines(global_cmdline_only_options);
  global_cmdline_only_options = NULL;

  if (configured_ports) {
    SMARTLIST_FOREACH(configured_ports,
                      port_cfg_t *, p, port_cfg_free(p));
    smartlist_free(configured_ports);
    configured_ports = NULL;
  }

  tor_free(torrc_fname);
  tor_free(torrc_defaults_fname);
  tor_free(the_tor_version);
  tor_free(global_dirfrontpagecontents);

  tor_free(the_short_tor_version);
  tor_free(the_tor_version);
}

/** Make <b>address</b> -- a piece of information related to our operation as
 * a client -- safe to log according to the settings in options->SafeLogging,
 * and return it.
 *
 * (We return "[scrubbed]" if SafeLogging is "1", and address otherwise.)
 */
const char *
safe_str_client(const char *address)
{
  tor_assert(address);
  if (get_options()->SafeLogging_ == SAFELOG_SCRUB_ALL)
    return "[scrubbed]";
  else
    return address;
}

/** Make <b>address</b> -- a piece of information of unspecified sensitivity
 * -- safe to log according to the settings in options->SafeLogging, and
 * return it.
 *
 * (We return "[scrubbed]" if SafeLogging is anything besides "0", and address
 * otherwise.)
 */
const char *
safe_str(const char *address)
{
  tor_assert(address);
  if (get_options()->SafeLogging_ != SAFELOG_SCRUB_NONE)
    return "[scrubbed]";
  else
    return address;
}

/** Equivalent to escaped(safe_str_client(address)).  See reentrancy note on
 * escaped(): don't use this outside the main thread, or twice in the same
 * log statement. */
const char *
escaped_safe_str_client(const char *address)
{
  if (get_options()->SafeLogging_ == SAFELOG_SCRUB_ALL)
    return "[scrubbed]";
  else
    return escaped(address);
}

/** Equivalent to escaped(safe_str(address)).  See reentrancy note on
 * escaped(): don't use this outside the main thread, or twice in the same
 * log statement. */
const char *
escaped_safe_str(const char *address)
{
  if (get_options()->SafeLogging_ != SAFELOG_SCRUB_NONE)
    return "[scrubbed]";
  else
    return escaped(address);
}

/** Add the default directory authorities directly into the trusted dir list,
 * but only add them insofar as they share bits with <b>type</b>. */
static void
add_default_trusted_dir_authorities(dirinfo_type_t type)
{
  int i;
  const char *authorities[] = {
    "moria1 orport=9101 "
      "v3ident=D586D18309DED4CD6D57C18FDB97EFA96D330566 "
      "128.31.0.39:9131 9695 DFC3 5FFE B861 329B 9F1A B04C 4639 7020 CE31",
    "tor26 orport=443 v3ident=14C131DFC5C6F93646BE72FA1401C02A8DF2E8B4 "
      "86.59.21.38:80 847B 1F85 0344 D787 6491 A548 92F9 0493 4E4E B85D",
    "dizum orport=443 v3ident=E8A9C45EDE6D711294FADF8E7951F4DE6CA56B58 "
      "194.109.206.212:80 7EA6 EAD6 FD83 083C 538F 4403 8BBF A077 587D D755",
    "Tonga orport=443 bridge 82.94.251.203:80 "
      "4A0C CD2D DC79 9508 3D73 F5D6 6710 0C8A 5831 F16D",
    "turtles orport=9090 "
      "v3ident=27B6B5996C426270A5C95488AA5BCEB6BCC86956 "
      "76.73.17.194:9030 F397 038A DC51 3361 35E7 B80B D99C A384 4360 292B",
    "gabelmoo orport=443 "
      "v3ident=ED03BB616EB2F60BEC80151114BB25CEF515B226 "
      "131.188.40.189:80 F204 4413 DAC2 E02E 3D6B CF47 35A1 9BCA 1DE9 7281",
    "dannenberg orport=443 "
      "v3ident=585769C78764D58426B8B52B6651A5A71137189A "
      "193.23.244.244:80 7BE6 83E6 5D48 1413 21C5 ED92 F075 C553 64AC 7123",
    "urras orport=80 v3ident=80550987E1D626E3EBA5E5E75A458DE0626D088C "
      "208.83.223.34:443 0AD3 FA88 4D18 F89E EA2D 89C0 1937 9E0E 7FD9 4417",
    "maatuska orport=80 "
      "v3ident=49015F787433103580E3B66A1707A00E60F2D15B "
      "171.25.193.9:443 BD6A 8292 55CB 08E6 6FBE 7D37 4836 3586 E46B 3810",
    "Faravahar orport=443 "
      "v3ident=EFCBE720AB3A82B99F9E953CD5BF50F7EEFC7B97 "
      "154.35.32.5:80 CF6D 0AAF B385 BE71 B8E1 11FC 5CFF 4B47 9237 33BC",
    NULL
  };
  for (i=0; authorities[i]; i++) {
    if (parse_dir_authority_line(authorities[i], type, 0)<0) {
      log_err(LD_BUG, "Couldn't parse internal DirAuthority line %s",
              authorities[i]);
    }
  }
}

/** Add the default fallback directory servers into the fallback directory
 * server list. */
static void
add_default_fallback_dir_servers(void)
{
  int i;
  const char *fallback[] = {
    NULL
  };
  for (i=0; fallback[i]; i++) {
    if (parse_dir_fallback_line(fallback[i], 0)<0) {
      log_err(LD_BUG, "Couldn't parse internal FallbackDir line %s",
              fallback[i]);
    }
  }
}

/** Look at all the config options for using alternate directory
 * authorities, and make sure none of them are broken. Also, warn the
 * user if we changed any dangerous ones.
 */
static int
validate_dir_servers(or_options_t *options, or_options_t *old_options)
{
  config_line_t *cl;

  if (options->DirAuthorities &&
      (options->AlternateDirAuthority || options->AlternateBridgeAuthority)) {
    log_warn(LD_CONFIG,
             "You cannot set both DirAuthority and Alternate*Authority.");
    return -1;
  }

  /* do we want to complain to the user about being partitionable? */
  if ((options->DirAuthorities &&
       (!old_options ||
        !config_lines_eq(options->DirAuthorities,
                         old_options->DirAuthorities))) ||
      (options->AlternateDirAuthority &&
       (!old_options ||
        !config_lines_eq(options->AlternateDirAuthority,
                         old_options->AlternateDirAuthority)))) {
    log_warn(LD_CONFIG,
             "You have used DirAuthority or AlternateDirAuthority to "
             "specify alternate directory authorities in "
             "your configuration. This is potentially dangerous: it can "
             "make you look different from all other Tor users, and hurt "
             "your anonymity. Even if you've specified the same "
             "authorities as Tor uses by default, the defaults could "
             "change in the future. Be sure you know what you're doing.");
  }

  /* Now go through the four ways you can configure an alternate
   * set of directory authorities, and make sure none are broken. */
  for (cl = options->DirAuthorities; cl; cl = cl->next)
    if (parse_dir_authority_line(cl->value, NO_DIRINFO, 1)<0)
      return -1;
  for (cl = options->AlternateBridgeAuthority; cl; cl = cl->next)
    if (parse_dir_authority_line(cl->value, NO_DIRINFO, 1)<0)
      return -1;
  for (cl = options->AlternateDirAuthority; cl; cl = cl->next)
    if (parse_dir_authority_line(cl->value, NO_DIRINFO, 1)<0)
      return -1;
  for (cl = options->FallbackDir; cl; cl = cl->next)
    if (parse_dir_fallback_line(cl->value, 1)<0)
      return -1;
  return 0;
}

/** Look at all the config options and assign new dir authorities
 * as appropriate.
 */
static int
consider_adding_dir_servers(const or_options_t *options,
                            const or_options_t *old_options)
{
  config_line_t *cl;
  int need_to_update =
    !smartlist_len(router_get_trusted_dir_servers()) ||
    !smartlist_len(router_get_fallback_dir_servers()) || !old_options ||
    !config_lines_eq(options->DirAuthorities, old_options->DirAuthorities) ||
    !config_lines_eq(options->FallbackDir, old_options->FallbackDir) ||
    !config_lines_eq(options->AlternateBridgeAuthority,
                     old_options->AlternateBridgeAuthority) ||
    !config_lines_eq(options->AlternateDirAuthority,
                     old_options->AlternateDirAuthority);

  if (!need_to_update)
    return 0; /* all done */

  /* Start from a clean slate. */
  clear_dir_servers();

  if (!options->DirAuthorities) {
    /* then we may want some of the defaults */
    dirinfo_type_t type = NO_DIRINFO;
    if (!options->AlternateBridgeAuthority)
      type |= BRIDGE_DIRINFO;
    if (!options->AlternateDirAuthority)
      type |= V3_DIRINFO | EXTRAINFO_DIRINFO | MICRODESC_DIRINFO;
    add_default_trusted_dir_authorities(type);
  }
  if (!options->FallbackDir)
    add_default_fallback_dir_servers();

  for (cl = options->DirAuthorities; cl; cl = cl->next)
    if (parse_dir_authority_line(cl->value, NO_DIRINFO, 0)<0)
      return -1;
  for (cl = options->AlternateBridgeAuthority; cl; cl = cl->next)
    if (parse_dir_authority_line(cl->value, NO_DIRINFO, 0)<0)
      return -1;
  for (cl = options->AlternateDirAuthority; cl; cl = cl->next)
    if (parse_dir_authority_line(cl->value, NO_DIRINFO, 0)<0)
      return -1;
  for (cl = options->FallbackDir; cl; cl = cl->next)
    if (parse_dir_fallback_line(cl->value, 0)<0)
      return -1;
  return 0;
}

/** Fetch the active option list, and take actions based on it. All of the
 * things we do should survive being done repeatedly.  If present,
 * <b>old_options</b> contains the previous value of the options.
 *
 * Return 0 if all goes well, return -1 if things went badly.
 */
static int
options_act_reversible(const or_options_t *old_options, char **msg)
{
  smartlist_t *new_listeners = smartlist_new();
  smartlist_t *replaced_listeners = smartlist_new();
  static int libevent_initialized = 0;
  or_options_t *options = get_options_mutable();
  int running_tor = options->command == CMD_RUN_TOR;
  int set_conn_limit = 0;
  int r = -1;
  int logs_marked = 0;
  int old_min_log_level = get_min_log_level();

  /* Daemonize _first_, since we only want to open most of this stuff in
   * the subprocess.  Libevent bases can't be reliably inherited across
   * processes. */
  if (running_tor && options->RunAsDaemon) {
    /* No need to roll back, since you can't change the value. */
    start_daemon();
  }

#ifndef HAVE_SYS_UN_H
  if (options->ControlSocket || options->ControlSocketsGroupWritable) {
    *msg = tor_strdup("Unix domain sockets (ControlSocket) not supported "
                      "on this OS/with this build.");
    goto rollback;
  }
#else
  if (options->ControlSocketsGroupWritable && !options->ControlSocket) {
    *msg = tor_strdup("Setting ControlSocketGroupWritable without setting"
                      "a ControlSocket makes no sense.");
    goto rollback;
  }
#endif

  if (running_tor) {
    int n_ports=0;
    /* We need to set the connection limit before we can open the listeners. */
    if (! sandbox_is_active()) {
      if (set_max_file_descriptors((unsigned)options->ConnLimit,
                                   &options->ConnLimit_) < 0) {
        *msg = tor_strdup("Problem with ConnLimit value. "
                          "See logs for details.");
        goto rollback;
      }
      set_conn_limit = 1;
    } else {
      tor_assert(old_options);
      options->ConnLimit_ = old_options->ConnLimit_;
    }

    /* Set up libevent.  (We need to do this before we can register the
     * listeners as listeners.) */
    if (running_tor && !libevent_initialized) {
      init_libevent(options);
      libevent_initialized = 1;
    }

    /* Adjust the port configuration so we can launch listeners. */
    if (parse_ports(options, 0, msg, &n_ports)) {
      if (!*msg)
        *msg = tor_strdup("Unexpected problem parsing port config");
      goto rollback;
    }

    /* Set the hibernation state appropriately.*/
    consider_hibernation(time(NULL));

    /* Launch the listeners.  (We do this before we setuid, so we can bind to
     * ports under 1024.)  We don't want to rebind if we're hibernating. If
     * networking is disabled, this will close all but the control listeners,
     * but disable those. */
    if (!we_are_hibernating()) {
      if (retry_all_listeners(replaced_listeners, new_listeners,
                              options->DisableNetwork) < 0) {
        *msg = tor_strdup("Failed to bind one of the listener ports.");
        goto rollback;
      }
    }
    if (options->DisableNetwork) {
      /* Aggressively close non-controller stuff, NOW */
      log_notice(LD_NET, "DisableNetwork is set. Tor will not make or accept "
                 "non-control network connections. Shutting down all existing "
                 "connections.");
      connection_mark_all_noncontrol_connections();
    }
  }

#if defined(HAVE_NET_IF_H) && defined(HAVE_NET_PFVAR_H)
  /* Open /dev/pf before dropping privileges. */
  if (options->TransPort_set &&
      options->TransProxyType_parsed == TPT_DEFAULT) {
    if (get_pf_socket() < 0) {
      *msg = tor_strdup("Unable to open /dev/pf for transparent proxy.");
      goto rollback;
    }
  }
#endif

  /* Attempt to lock all current and future memory with mlockall() only once */
  if (options->DisableAllSwap) {
    if (tor_mlockall() == -1) {
      *msg = tor_strdup("DisableAllSwap failure. Do you have proper "
                        "permissions?");
      goto done;
    }
  }

  /* Setuid/setgid as appropriate */
  if (options->User) {
    if (switch_id(options->User) != 0) {
      /* No need to roll back, since you can't change the value. */
      *msg = tor_strdup("Problem with User value. See logs for details.");
      goto done;
    }
  }

  /* Ensure data directory is private; create if possible. */
  if (check_private_dir(options->DataDirectory,
                        running_tor ? CPD_CREATE : CPD_CHECK,
                        options->User)<0) {
    tor_asprintf(msg,
              "Couldn't access/create private data directory \"%s\"",
              options->DataDirectory);
    goto done;
    /* No need to roll back, since you can't change the value. */
  }

  /* Bail out at this point if we're not going to be a client or server:
   * we don't run Tor itself. */
  if (!running_tor)
    goto commit;

  mark_logs_temp(); /* Close current logs once new logs are open. */
  logs_marked = 1;
  if (options_init_logs(options, 0)<0) { /* Configure the tor_log(s) */
    *msg = tor_strdup("Failed to init Log options. See logs for details.");
    goto rollback;
  }

 commit:
  r = 0;
  if (logs_marked) {
    log_severity_list_t *severity =
      tor_malloc_zero(sizeof(log_severity_list_t));
    close_temp_logs();
    add_callback_log(severity, control_event_logmsg);
    control_adjust_event_log_severity();
    tor_free(severity);
    tor_log_update_sigsafe_err_fds();
  }

  {
    const char *badness = NULL;
    int bad_safelog = 0, bad_severity = 0, new_badness = 0;
    if (options->SafeLogging_ != SAFELOG_SCRUB_ALL) {
      bad_safelog = 1;
      if (!old_options || old_options->SafeLogging_ != options->SafeLogging_)
        new_badness = 1;
    }
    if (get_min_log_level() >= LOG_INFO) {
      bad_severity = 1;
      if (get_min_log_level() != old_min_log_level)
        new_badness = 1;
    }
    if (bad_safelog && bad_severity)
      badness = "you disabled SafeLogging, and "
        "you're logging more than \"notice\"";
    else if (bad_safelog)
      badness = "you disabled SafeLogging";
    else
      badness = "you're logging more than \"notice\"";
    if (new_badness)
      log_warn(LD_GENERAL, "Your log may contain sensitive information - %s. "
               "Don't log unless it serves an important reason. "
               "Overwrite the log afterwards.", badness);
  }

  SMARTLIST_FOREACH(replaced_listeners, connection_t *, conn,
  {
    int marked = conn->marked_for_close;
    log_notice(LD_NET, "Closing old %s on %s:%d",
               conn_type_to_string(conn->type), conn->address, conn->port);
    connection_close_immediate(conn);
    if (!marked) {
      connection_mark_for_close(conn);
    }
  });
  goto done;

 rollback:
  r = -1;
  tor_assert(*msg);

  if (logs_marked) {
    rollback_log_changes();
    control_adjust_event_log_severity();
  }

  if (set_conn_limit && old_options)
    set_max_file_descriptors((unsigned)old_options->ConnLimit,
                             &options->ConnLimit_);

  SMARTLIST_FOREACH(new_listeners, connection_t *, conn,
  {
    log_notice(LD_NET, "Closing partially-constructed %s on %s:%d",
               conn_type_to_string(conn->type), conn->address, conn->port);
    connection_close_immediate(conn);
    connection_mark_for_close(conn);
  });

 done:
  smartlist_free(new_listeners);
  smartlist_free(replaced_listeners);
  return r;
}

/** If we need to have a GEOIP ip-to-country map to run with our configured
 * options, return 1 and set *<b>reason_out</b> to a description of why. */
int
options_need_geoip_info(const or_options_t *options, const char **reason_out)
{
  int bridge_usage =
    options->BridgeRelay && options->BridgeRecordUsageByCountry;
  int routerset_usage =
    routerset_needs_geoip(options->EntryNodes) ||
    routerset_needs_geoip(options->ExitNodes) ||
    routerset_needs_geoip(options->ExcludeExitNodes) ||
    routerset_needs_geoip(options->ExcludeNodes);

  if (routerset_usage && reason_out) {
    *reason_out = "We've been configured to use (or avoid) nodes in certain "
      "countries, and we need GEOIP information to figure out which ones they "
      "are.";
  } else if (bridge_usage && reason_out) {
    *reason_out = "We've been configured to see which countries can access "
      "us as a bridge, and we need GEOIP information to tell which countries "
      "clients are in.";
  }
  return bridge_usage || routerset_usage;
}

/** Return the bandwidthrate that we are going to report to the authorities
 * based on the config options. */
uint32_t
get_effective_bwrate(const or_options_t *options)
{
  uint64_t bw = options->BandwidthRate;
  if (bw > options->MaxAdvertisedBandwidth)
    bw = options->MaxAdvertisedBandwidth;
  if (options->RelayBandwidthRate > 0 && bw > options->RelayBandwidthRate)
    bw = options->RelayBandwidthRate;
  /* ensure_bandwidth_cap() makes sure that this cast can't overflow. */
  return (uint32_t)bw;
}

/** Return the bandwidthburst that we are going to report to the authorities
 * based on the config options. */
uint32_t
get_effective_bwburst(const or_options_t *options)
{
  uint64_t bw = options->BandwidthBurst;
  if (options->RelayBandwidthBurst > 0 && bw > options->RelayBandwidthBurst)
    bw = options->RelayBandwidthBurst;
  /* ensure_bandwidth_cap() makes sure that this cast can't overflow. */
  return (uint32_t)bw;
}

/** Return True if any changes from <b>old_options</b> to
 * <b>new_options</b> needs us to refresh our TLS context. */
static int
options_transition_requires_fresh_tls_context(const or_options_t *old_options,
                                              const or_options_t *new_options)
{
  tor_assert(new_options);

  if (!old_options)
    return 0;

  if ((old_options->DynamicDHGroups != new_options->DynamicDHGroups)) {
    return 1;
  }

  if (!opt_streq(old_options->TLSECGroup, new_options->TLSECGroup))
    return 1;

  return 0;
}

/** Fetch the active option list, and take actions based on it. All of the
 * things we do should survive being done repeatedly.  If present,
 * <b>old_options</b> contains the previous value of the options.
 *
 * Return 0 if all goes well, return -1 if it's time to die.
 *
 * Note: We haven't moved all the "act on new configuration" logic
 * here yet.  Some is still in do_hup() and other places.
 */
static int
options_act(const or_options_t *old_options)
{
  config_line_t *cl;
  or_options_t *options = get_options_mutable();
  int running_tor = options->command == CMD_RUN_TOR;
  char *msg=NULL;
  const int transition_affects_workers =
    old_options && options_transition_affects_workers(old_options, options);
  int old_ewma_enabled;

  /* disable ptrace and later, other basic debugging techniques */
  {
    /* Remember if we already disabled debugger attachment */
    static int disabled_debugger_attach = 0;
    /* Remember if we already warned about being configured not to disable
     * debugger attachment */
    static int warned_debugger_attach = 0;
    /* Don't disable debugger attachment when we're running the unit tests. */
    if (options->DisableDebuggerAttachment && !disabled_debugger_attach &&
        running_tor) {
      int ok = tor_disable_debugger_attach();
      if (warned_debugger_attach && ok == 1) {
        log_notice(LD_CONFIG, "Disabled attaching debuggers for unprivileged "
                   "users.");
      }
      disabled_debugger_attach = (ok == 1);
    } else if (!options->DisableDebuggerAttachment &&
               !warned_debugger_attach) {
      log_notice(LD_CONFIG, "Not disabling debugger attaching for "
                 "unprivileged users.");
      warned_debugger_attach = 1;
    }
  }

  /* Write control ports to disk as appropriate */
  control_ports_write_to_file();

  if (running_tor && !have_lockfile()) {
    if (try_locking(options, 1) < 0)
      return -1;
  }

  if (consider_adding_dir_servers(options, old_options) < 0)
    return -1;

#ifdef NON_ANONYMOUS_MODE_ENABLED
  log_warn(LD_GENERAL, "This copy of Tor was compiled to run in a "
      "non-anonymous mode. It will provide NO ANONYMITY.");
#endif

#ifdef ENABLE_TOR2WEB_MODE
  if (!options->Tor2webMode) {
    log_err(LD_CONFIG, "This copy of Tor was compiled to run in "
            "'tor2web mode'. It can only be run with the Tor2webMode torrc "
            "option enabled.");
    return -1;
  }
#else
  if (options->Tor2webMode) {
    log_err(LD_CONFIG, "This copy of Tor was not compiled to run in "
            "'tor2web mode'. It cannot be run with the Tor2webMode torrc "
            "option enabled. To enable Tor2webMode recompile with the "
            "--enable-tor2webmode option.");
    return -1;
  }
#endif

  /* If we are a bridge with a pluggable transport proxy but no
     Extended ORPort, inform the user that she is missing out. */
  if (server_mode(options) && options->ServerTransportPlugin &&
      !options->ExtORPort_lines) {
    log_notice(LD_CONFIG, "We use pluggable transports but the Extended "
               "ORPort is disabled. Tor and your pluggable transports proxy "
               "communicate with each other via the Extended ORPort so it "
               "is suggested you enable it: it will also allow your Bridge "
               "to collect statistics about its clients that use pluggable "
               "transports. Please enable it using the ExtORPort torrc option "
               "(e.g. set 'ExtORPort auto').");
  }

  if (options->Bridges) {
    mark_bridge_list();
    for (cl = options->Bridges; cl; cl = cl->next) {
      bridge_line_t *bridge_line = parse_bridge_line(cl->value);
      if (!bridge_line) {
        log_warn(LD_BUG,
                 "Previously validated Bridge line could not be added!");
        return -1;
      }
      bridge_add_from_config(bridge_line);
    }
    sweep_bridge_list();
  }

  if (running_tor && rend_config_services(options, 0)<0) {
    log_warn(LD_BUG,
       "Previously validated hidden services line could not be added!");
    return -1;
  }

  if (running_tor && rend_parse_service_authorization(options, 0) < 0) {
    log_warn(LD_BUG, "Previously validated client authorization for "
                     "hidden services could not be added!");
    return -1;
  }

  /* Load state */
  if (! or_state_loaded() && running_tor) {
    if (or_state_load())
      return -1;
    rep_hist_load_mtbf_data(time(NULL));
  }

  mark_transport_list();
  pt_prepare_proxy_list_for_config_read();
  if (options->ClientTransportPlugin) {
    for (cl = options->ClientTransportPlugin; cl; cl = cl->next) {
      if (parse_client_transport_line(options, cl->value, 0)<0) {
        log_warn(LD_BUG,
                 "Previously validated ClientTransportPlugin line "
                 "could not be added!");
        return -1;
      }
    }
  }

  if (options->ServerTransportPlugin && server_mode(options)) {
    for (cl = options->ServerTransportPlugin; cl; cl = cl->next) {
      if (parse_server_transport_line(options, cl->value, 0)<0) {
        log_warn(LD_BUG,
                 "Previously validated ServerTransportPlugin line "
                 "could not be added!");
        return -1;
      }
    }
  }
  sweep_transport_list();
  sweep_proxy_list();

  /* Start the PT proxy configuration. By doing this configuration
     here, we also figure out which proxies need to be restarted and
     which not. */
  if (pt_proxies_configuration_pending() && !net_is_disabled())
    pt_configure_remaining_proxies();

  /* Bail out at this point if we're not going to be a client or server:
   * we want to not fork, and to log stuff to stderr. */
  if (!running_tor)
    return 0;

  /* Finish backgrounding the process */
  if (options->RunAsDaemon) {
    /* We may be calling this for the n'th time (on SIGHUP), but it's safe. */
    finish_daemon(options->DataDirectory);
  }

  /* If needed, generate a new TLS DH prime according to the current torrc. */
  if (server_mode(options) && options->DynamicDHGroups) {
    char *keydir = get_datadir_fname("keys");
    if (check_private_dir(keydir, CPD_CREATE, options->User)) {
      tor_free(keydir);
      return -1;
    }
    tor_free(keydir);

    if (!old_options || !old_options->DynamicDHGroups) {
      char *fname = get_datadir_fname2("keys", "dynamic_dh_params");
      crypto_set_tls_dh_prime(fname);
      tor_free(fname);
    }
  } else { /* clients don't need a dynamic DH prime. */
    crypto_set_tls_dh_prime(NULL);
  }

  /* We want to reinit keys as needed before we do much of anything else:
     keys are important, and other things can depend on them. */
  if (transition_affects_workers ||
      (options->V3AuthoritativeDir && (!old_options ||
                                       !old_options->V3AuthoritativeDir))) {
    if (init_keys() < 0) {
      log_warn(LD_BUG,"Error initializing keys; exiting");
      return -1;
    }
  } else if (old_options &&
             options_transition_requires_fresh_tls_context(old_options,
                                                           options)) {
    if (router_initialize_tls_context() < 0) {
      log_warn(LD_BUG,"Error initializing TLS context.");
      return -1;
    }
  }

  /* Write our PID to the PID file. If we do not have write permissions we
   * will log a warning */
  if (options->PidFile && !sandbox_is_active()) {
    write_pidfile(options->PidFile);
  }

  /* Register addressmap directives */
  config_register_addressmaps(options);
  parse_virtual_addr_network(options->VirtualAddrNetworkIPv4, AF_INET,0,NULL);
  parse_virtual_addr_network(options->VirtualAddrNetworkIPv6, AF_INET6,0,NULL);

  /* Update address policies. */
  if (policies_parse_from_options(options) < 0) {
    /* This should be impossible, but let's be sure. */
    log_warn(LD_BUG,"Error parsing already-validated policy options.");
    return -1;
  }

  if (init_control_cookie_authentication(options->CookieAuthentication) < 0) {
    log_warn(LD_CONFIG,"Error creating control cookie authentication file.");
    return -1;
  }

  /* If we have an ExtORPort, initialize its auth cookie. */
  if (init_ext_or_cookie_authentication(!!options->ExtORPort_lines) < 0) {
    log_warn(LD_CONFIG,"Error creating Extended ORPort cookie file.");
    return -1;
  }

  monitor_owning_controller_process(options->OwningControllerProcess);

  /* reload keys as needed for rendezvous services. */
  if (rend_service_load_all_keys()<0) {
    log_warn(LD_GENERAL,"Error loading rendezvous service keys");
    return -1;
  }

  /* Set up accounting */
  if (accounting_parse_options(options, 0)<0) {
    log_warn(LD_CONFIG,"Error in accounting options");
    return -1;
  }
  if (accounting_is_enabled(options))
    configure_accounting(time(NULL));

#ifdef USE_BUFFEREVENTS
  /* If we're using the bufferevents implementation and our rate limits
   * changed, we need to tell the rate-limiting system about it. */
  if (!old_options ||
      old_options->BandwidthRate != options->BandwidthRate ||
      old_options->BandwidthBurst != options->BandwidthBurst ||
      old_options->RelayBandwidthRate != options->RelayBandwidthRate ||
      old_options->RelayBandwidthBurst != options->RelayBandwidthBurst)
    connection_bucket_init();
#endif

  old_ewma_enabled = cell_ewma_enabled();
  /* Change the cell EWMA settings */
  cell_ewma_set_scale_factor(options, networkstatus_get_latest_consensus());
  /* If we just enabled ewma, set the cmux policy on all active channels */
  if (cell_ewma_enabled() && !old_ewma_enabled) {
    channel_set_cmux_policy_everywhere(&ewma_policy);
  } else if (!cell_ewma_enabled() && old_ewma_enabled) {
    /* Turn it off everywhere */
    channel_set_cmux_policy_everywhere(NULL);
  }

  /* Update the BridgePassword's hashed version as needed.  We store this as a
   * digest so that we can do side-channel-proof comparisons on it.
   */
  if (options->BridgePassword) {
    char *http_authenticator;
    http_authenticator = alloc_http_authenticator(options->BridgePassword);
    if (!http_authenticator) {
      log_warn(LD_BUG, "Unable to allocate HTTP authenticator. Not setting "
               "BridgePassword.");
      return -1;
    }
    options->BridgePassword_AuthDigest_ = tor_malloc(DIGEST256_LEN);
    crypto_digest256(options->BridgePassword_AuthDigest_,
                     http_authenticator, strlen(http_authenticator),
                     DIGEST_SHA256);
    tor_free(http_authenticator);
  }

  if (parse_outbound_addresses(options, 0, &msg) < 0) {
    log_warn(LD_BUG, "Failed parsing oubound bind addresses: %s", msg);
    tor_free(msg);
    return -1;
  }

  /* Check for transitions that need action. */
  if (old_options) {
    int revise_trackexithosts = 0;
    int revise_automap_entries = 0;
    if ((options->UseEntryGuards && !old_options->UseEntryGuards) ||
        options->UseBridges != old_options->UseBridges ||
        (options->UseBridges &&
         !config_lines_eq(options->Bridges, old_options->Bridges)) ||
        !routerset_equal(old_options->ExcludeNodes,options->ExcludeNodes) ||
        !routerset_equal(old_options->ExcludeExitNodes,
                         options->ExcludeExitNodes) ||
        !routerset_equal(old_options->EntryNodes, options->EntryNodes) ||
        !routerset_equal(old_options->ExitNodes, options->ExitNodes) ||
        options->StrictNodes != old_options->StrictNodes) {
      log_info(LD_CIRC,
               "Changed to using entry guards or bridges, or changed "
               "preferred or excluded node lists. "
               "Abandoning previous circuits.");
      circuit_mark_all_unused_circs();
      circuit_mark_all_dirty_circs_as_unusable();
      revise_trackexithosts = 1;
    }

    if (!smartlist_strings_eq(old_options->TrackHostExits,
                              options->TrackHostExits))
      revise_trackexithosts = 1;

    if (revise_trackexithosts)
      addressmap_clear_excluded_trackexithosts(options);

    if (!options->AutomapHostsOnResolve) {
      if (old_options->AutomapHostsOnResolve)
        revise_automap_entries = 1;
    } else {
      if (!smartlist_strings_eq(old_options->AutomapHostsSuffixes,
                                options->AutomapHostsSuffixes))
        revise_automap_entries = 1;
      else if (!opt_streq(old_options->VirtualAddrNetworkIPv4,
                          options->VirtualAddrNetworkIPv4) ||
               !opt_streq(old_options->VirtualAddrNetworkIPv6,
                          options->VirtualAddrNetworkIPv6))
        revise_automap_entries = 1;
    }

    if (revise_automap_entries)
      addressmap_clear_invalid_automaps(options);

/* How long should we delay counting bridge stats after becoming a bridge?
 * We use this so we don't count people who used our bridge thinking it is
 * a relay. If you change this, don't forget to change the log message
 * below. It's 4 hours (the time it takes to stop being used by clients)
 * plus some extra time for clock skew. */
#define RELAY_BRIDGE_STATS_DELAY (6 * 60 * 60)

    if (! bool_eq(options->BridgeRelay, old_options->BridgeRelay)) {
      int was_relay = 0;
      if (options->BridgeRelay) {
        time_t int_start = time(NULL);
        if (config_lines_eq(old_options->ORPort_lines,options->ORPort_lines)) {
          int_start += RELAY_BRIDGE_STATS_DELAY;
          was_relay = 1;
        }
        geoip_bridge_stats_init(int_start);
        log_info(LD_CONFIG, "We are acting as a bridge now.  Starting new "
                 "GeoIP stats interval%s.", was_relay ? " in 6 "
                 "hours from now" : "");
      } else {
        geoip_bridge_stats_term();
        log_info(LD_GENERAL, "We are no longer acting as a bridge.  "
                 "Forgetting GeoIP stats.");
      }
    }

    if (transition_affects_workers) {
      log_info(LD_GENERAL,
               "Worker-related options changed. Rotating workers.");

      if (server_mode(options) && !server_mode(old_options)) {
        ip_address_changed(0);
        if (can_complete_circuit || !any_predicted_circuits(time(NULL)))
          inform_testing_reachability();
      }
      cpuworkers_rotate();
      if (dns_reset())
        return -1;
    } else {
      if (dns_reset())
        return -1;
    }

    if (options->PerConnBWRate != old_options->PerConnBWRate ||
        options->PerConnBWBurst != old_options->PerConnBWBurst)
      connection_or_update_token_buckets(get_connection_array(), options);
  }

  config_maybe_load_geoip_files_(options, old_options);

  if (geoip_is_loaded(AF_INET) && options->GeoIPExcludeUnknown) {
    /* ExcludeUnknown is true or "auto" */
    const int is_auto = options->GeoIPExcludeUnknown == -1;
    int changed;

    changed  = routerset_add_unknown_ccs(&options->ExcludeNodes, is_auto);
    changed += routerset_add_unknown_ccs(&options->ExcludeExitNodes, is_auto);

    if (changed)
      routerset_add_unknown_ccs(&options->ExcludeExitNodesUnion_, is_auto);
  }

  if (options->CellStatistics || options->DirReqStatistics ||
      options->EntryStatistics || options->ExitPortStatistics ||
      options->ConnDirectionStatistics ||
      options->BridgeAuthoritativeDir) {
    time_t now = time(NULL);
    int print_notice = 0;

    /* Only collect directory-request statistics on relays and bridges. */
    if (!server_mode(options)) {
      options->DirReqStatistics = 0;
    }

    /* Only collect other relay-only statistics on relays. */
    if (!public_server_mode(options)) {
      options->CellStatistics = 0;
      options->EntryStatistics = 0;
      options->ExitPortStatistics = 0;
    }

    if ((!old_options || !old_options->CellStatistics) &&
        options->CellStatistics) {
      rep_hist_buffer_stats_init(now);
      print_notice = 1;
    }
    if ((!old_options || !old_options->DirReqStatistics) &&
        options->DirReqStatistics) {
      if (geoip_is_loaded(AF_INET)) {
        geoip_dirreq_stats_init(now);
        print_notice = 1;
      } else {
        options->DirReqStatistics = 0;
        /* Don't warn Tor clients, they don't use statistics */
        if (options->ORPort_set)
          log_notice(LD_CONFIG, "Configured to measure directory request "
                                "statistics, but no GeoIP database found. "
                                "Please specify a GeoIP database using the "
                                "GeoIPFile option.");
      }
    }
    if ((!old_options || !old_options->EntryStatistics) &&
        options->EntryStatistics && !should_record_bridge_info(options)) {
      if (geoip_is_loaded(AF_INET) || geoip_is_loaded(AF_INET6)) {
        geoip_entry_stats_init(now);
        print_notice = 1;
      } else {
        options->EntryStatistics = 0;
        log_notice(LD_CONFIG, "Configured to measure entry node "
                              "statistics, but no GeoIP database found. "
                              "Please specify a GeoIP database using the "
                              "GeoIPFile option.");
      }
    }
    if ((!old_options || !old_options->ExitPortStatistics) &&
        options->ExitPortStatistics) {
      rep_hist_exit_stats_init(now);
      print_notice = 1;
    }
    if ((!old_options || !old_options->ConnDirectionStatistics) &&
        options->ConnDirectionStatistics) {
      rep_hist_conn_stats_init(now);
    }
    if ((!old_options || !old_options->BridgeAuthoritativeDir) &&
        options->BridgeAuthoritativeDir) {
      rep_hist_desc_stats_init(now);
      print_notice = 1;
    }
    if (print_notice)
      log_notice(LD_CONFIG, "Configured to measure statistics. Look for "
                 "the *-stats files that will first be written to the "
                 "data directory in 24 hours from now.");
  }

  if (old_options && old_options->CellStatistics &&
      !options->CellStatistics)
    rep_hist_buffer_stats_term();
  if (old_options && old_options->DirReqStatistics &&
      !options->DirReqStatistics)
    geoip_dirreq_stats_term();
  if (old_options && old_options->EntryStatistics &&
      !options->EntryStatistics)
    geoip_entry_stats_term();
  if (old_options && old_options->ExitPortStatistics &&
      !options->ExitPortStatistics)
    rep_hist_exit_stats_term();
  if (old_options && old_options->ConnDirectionStatistics &&
      !options->ConnDirectionStatistics)
    rep_hist_conn_stats_term();
  if (old_options && old_options->BridgeAuthoritativeDir &&
      !options->BridgeAuthoritativeDir)
    rep_hist_desc_stats_term();

  /* Check if we need to parse and add the EntryNodes config option. */
  if (options->EntryNodes &&
      (!old_options ||
       !routerset_equal(old_options->EntryNodes,options->EntryNodes) ||
       !routerset_equal(old_options->ExcludeNodes,options->ExcludeNodes)))
    entry_nodes_should_be_added();

  /* Since our options changed, we might need to regenerate and upload our
   * server descriptor.
   */
  if (!old_options ||
      options_transition_affects_descriptor(old_options, options))
    mark_my_descriptor_dirty("config change");

  /* We may need to reschedule some directory stuff if our status changed. */
  if (old_options) {
    if (authdir_mode_v3(options) && !authdir_mode_v3(old_options))
      dirvote_recalculate_timing(options, time(NULL));
    if (!bool_eq(directory_fetches_dir_info_early(options),
                 directory_fetches_dir_info_early(old_options)) ||
        !bool_eq(directory_fetches_dir_info_later(options),
                 directory_fetches_dir_info_later(old_options))) {
      /* Make sure update_router_have_min_dir_info gets called. */
      router_dir_info_changed();
      /* We might need to download a new consensus status later or sooner than
       * we had expected. */
      update_consensus_networkstatus_fetch_time(time(NULL));
    }
  }

  /* Load the webpage we're going to serve every time someone asks for '/' on
     our DirPort. */
  tor_free(global_dirfrontpagecontents);
  if (options->DirPortFrontPage) {
    global_dirfrontpagecontents =
      read_file_to_str(options->DirPortFrontPage, 0, NULL);
    if (!global_dirfrontpagecontents) {
      log_warn(LD_CONFIG,
               "DirPortFrontPage file '%s' not found. Continuing anyway.",
               options->DirPortFrontPage);
    }
  }

  return 0;
}

static const struct {
  const char *name;
  int takes_argument;
} CMDLINE_ONLY_OPTIONS[] = {
  { "-f",                     1 },
  { "--allow-missing-torrc",  0 },
  { "--defaults-torrc",       1 },
  { "--hash-password",        1 },
  { "--dump-config",          1 },
  { "--list-fingerprint",     0 },
  { "--verify-config",        0 },
  { "--ignore-missing-torrc", 0 },
  { "--quiet",                0 },
  { "--hush",                 0 },
  { "--version",              0 },
  { "--library-versions",     0 },
  { "-h",                     0 },
  { "--help",                 0 },
  { "--list-torrc-options",   0 },
  { "--digests",              0 },
  { "--nt-service",           0 },
  { "-nt-service",            0 },
  { NULL, 0 },
};

/** Helper: Read a list of configuration options from the command line.  If
 * successful, or if ignore_errors is set, put them in *<b>result</b>, put the
 * commandline-only options in *<b>cmdline_result</b>, and return 0;
 * otherwise, return -1 and leave *<b>result</b> and <b>cmdline_result</b>
 * alone. */
int
config_parse_commandline(int argc, char **argv, int ignore_errors,
                         config_line_t **result,
                         config_line_t **cmdline_result)
{
  config_line_t *param = NULL;

  config_line_t *front = NULL;
  config_line_t **new = &front;

  config_line_t *front_cmdline = NULL;
  config_line_t **new_cmdline = &front_cmdline;

  char *s, *arg;
  int i = 1;

  while (i < argc) {
    unsigned command = CONFIG_LINE_NORMAL;
    int want_arg = 1;
    int is_cmdline = 0;
    int j;

    for (j = 0; CMDLINE_ONLY_OPTIONS[j].name != NULL; ++j) {
      if (!strcmp(argv[i], CMDLINE_ONLY_OPTIONS[j].name)) {
        is_cmdline = 1;
        want_arg = CMDLINE_ONLY_OPTIONS[j].takes_argument;
        break;
      }
    }

    s = argv[i];

    /* Each keyword may be prefixed with one or two dashes. */
    if (*s == '-')
      s++;
    if (*s == '-')
      s++;
    /* Figure out the command, if any. */
    if (*s == '+') {
      s++;
      command = CONFIG_LINE_APPEND;
    } else if (*s == '/') {
      s++;
      command = CONFIG_LINE_CLEAR;
      /* A 'clear' command has no argument. */
      want_arg = 0;
    }

    if (want_arg && i == argc-1) {
      if (ignore_errors) {
        arg = strdup("");
      } else {
        log_warn(LD_CONFIG,"Command-line option '%s' with no value. Failing.",
            argv[i]);
        config_free_lines(front);
        config_free_lines(front_cmdline);
        return -1;
      }
    } else {
      arg = want_arg ? tor_strdup(argv[i+1]) : strdup("");
    }

    param = tor_malloc_zero(sizeof(config_line_t));
    param->key = is_cmdline ? tor_strdup(argv[i]) :
                   tor_strdup(config_expand_abbrev(&options_format, s, 1, 1));
    param->value = arg;
    param->command = command;
    param->next = NULL;
    log_debug(LD_CONFIG, "command line: parsed keyword '%s', value '%s'",
        param->key, param->value);

    if (is_cmdline) {
      *new_cmdline = param;
      new_cmdline = &((*new_cmdline)->next);
    } else {
      *new = param;
      new = &((*new)->next);
    }

    i += want_arg ? 2 : 1;
  }
  *cmdline_result = front_cmdline;
  *result = front;
  return 0;
}

/** Return true iff key is a valid configuration option. */
int
option_is_recognized(const char *key)
{
  const config_var_t *var = config_find_option(&options_format, key);
  return (var != NULL);
}

/** Return the canonical name of a configuration option, or NULL
 * if no such option exists. */
const char *
option_get_canonical_name(const char *key)
{
  const config_var_t *var = config_find_option(&options_format, key);
  return var ? var->name : NULL;
}

/** Return a canonical list of the options assigned for key.
 */
config_line_t *
option_get_assignment(const or_options_t *options, const char *key)
{
  return config_get_assigned_option(&options_format, options, key, 1);
}

/** Try assigning <b>list</b> to the global options. You do this by duping
 * options, assigning list to the new one, then validating it. If it's
 * ok, then throw out the old one and stick with the new one. Else,
 * revert to old and return failure.  Return SETOPT_OK on success, or
 * a setopt_err_t on failure.
 *
 * If not success, point *<b>msg</b> to a newly allocated string describing
 * what went wrong.
 */
setopt_err_t
options_trial_assign(config_line_t *list, int use_defaults,
                     int clear_first, char **msg)
{
  int r;
  or_options_t *trial_options = config_dup(&options_format, get_options());

  if ((r=config_assign(&options_format, trial_options,
                       list, use_defaults, clear_first, msg)) < 0) {
    config_free(&options_format, trial_options);
    return r;
  }

  if (options_validate(get_options_mutable(), trial_options,
                       global_default_options, 1, msg) < 0) {
    config_free(&options_format, trial_options);
    return SETOPT_ERR_PARSE; /*XXX make this a separate return value. */
  }

  if (options_transition_allowed(get_options(), trial_options, msg) < 0) {
    config_free(&options_format, trial_options);
    return SETOPT_ERR_TRANSITION;
  }

  if (set_options(trial_options, msg)<0) {
    config_free(&options_format, trial_options);
    return SETOPT_ERR_SETTING;
  }

  /* we liked it. put it in place. */
  return SETOPT_OK;
}

/** Print a usage message for tor. */
static void
print_usage(void)
{
  printf(
"Copyright (c) 2001-2004, Roger Dingledine\n"
"Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson\n"
"Copyright (c) 2007-2013, The Tor Project, Inc.\n\n"
"tor -f <torrc> [args]\n"
"See man page for options, or https://www.torproject.org/ for "
"documentation.\n");
}

/** Print all non-obsolete torrc options. */
static void
list_torrc_options(void)
{
  int i;
  smartlist_t *lines = smartlist_new();
  for (i = 0; option_vars_[i].name; ++i) {
    const config_var_t *var = &option_vars_[i];
    if (var->type == CONFIG_TYPE_OBSOLETE ||
        var->type == CONFIG_TYPE_LINELIST_V)
      continue;
    printf("%s\n", var->name);
  }
  smartlist_free(lines);
}

/** Last value actually set by resolve_my_address. */
static uint32_t last_resolved_addr = 0;

/** Accessor for last_resolved_addr from outside this file. */
uint32_t
get_last_resolved_addr(void)
{
  return last_resolved_addr;
}

/**
 * Use <b>options-\>Address</b> to guess our public IP address.
 *
 * Return 0 if all is well, or -1 if we can't find a suitable
 * public IP address.
 *
 * If we are returning 0:
 *   - Put our public IP address (in host order) into *<b>addr_out</b>.
 *   - If <b>method_out</b> is non-NULL, set *<b>method_out</b> to a static
 *     string describing how we arrived at our answer.
 *   - If <b>hostname_out</b> is non-NULL, and we resolved a hostname to
 *     get our address, set *<b>hostname_out</b> to a newly allocated string
 *     holding that hostname. (If we didn't get our address by resolving a
 *     hostname, set *<b>hostname_out</b> to NULL.)
 *
 * XXXX ipv6
 */
int
resolve_my_address(int warn_severity, const or_options_t *options,
                   uint32_t *addr_out,
                   const char **method_out, char **hostname_out)
{
  struct in_addr in;
  uint32_t addr; /* host order */
  char hostname[256];
  const char *method_used;
  const char *hostname_used;
  int explicit_ip=1;
  int explicit_hostname=1;
  int from_interface=0;
  char *addr_string = NULL;
  const char *address = options->Address;
  int notice_severity = warn_severity <= LOG_NOTICE ?
                          LOG_NOTICE : warn_severity;

  tor_addr_t myaddr;
  tor_assert(addr_out);

  /*
   * Step one: Fill in 'hostname' to be our best guess.
   */

  if (address && *address) {
    strlcpy(hostname, address, sizeof(hostname));
  } else { /* then we need to guess our address */
    explicit_ip = 0; /* it's implicit */
    explicit_hostname = 0; /* it's implicit */

    if (gethostname(hostname, sizeof(hostname)) < 0) {
      log_fn(warn_severity, LD_NET,"Error obtaining local hostname");
      return -1;
    }
    log_debug(LD_CONFIG, "Guessed local host name as '%s'", hostname);
  }

  /*
   * Step two: Now that we know 'hostname', parse it or resolve it. If
   * it doesn't parse or resolve, look at the interface address. Set 'addr'
   * to be our (host-order) 32-bit answer.
   */

  if (tor_inet_aton(hostname, &in) == 0) {
    /* then we have to resolve it */
    explicit_ip = 0;
    if (tor_lookup_hostname(hostname, &addr)) { /* failed to resolve */
      uint32_t interface_ip; /* host order */

      if (explicit_hostname) {
        log_fn(warn_severity, LD_CONFIG,
               "Could not resolve local Address '%s'. Failing.", hostname);
        return -1;
      }
      log_fn(notice_severity, LD_CONFIG,
             "Could not resolve guessed local hostname '%s'. "
             "Trying something else.", hostname);
      if (get_interface_address(warn_severity, &interface_ip)) {
        log_fn(warn_severity, LD_CONFIG,
               "Could not get local interface IP address. Failing.");
        return -1;
      }
      from_interface = 1;
      addr = interface_ip;
      log_fn(notice_severity, LD_CONFIG, "Learned IP address '%s' for "
             "local interface. Using that.", fmt_addr32(addr));
      strlcpy(hostname, "<guessed from interfaces>", sizeof(hostname));
    } else { /* resolved hostname into addr */
      tor_addr_from_ipv4h(&myaddr, addr);

      if (!explicit_hostname &&
          tor_addr_is_internal(&myaddr, 0)) {
        tor_addr_t interface_ip;

        log_fn(notice_severity, LD_CONFIG, "Guessed local hostname '%s' "
               "resolves to a private IP address (%s). Trying something "
               "else.", hostname, fmt_addr32(addr));

        if (get_interface_address6(warn_severity, AF_INET, &interface_ip)<0) {
          log_fn(warn_severity, LD_CONFIG,
                 "Could not get local interface IP address. Too bad.");
        } else if (tor_addr_is_internal(&interface_ip, 0)) {
          log_fn(notice_severity, LD_CONFIG,
                 "Interface IP address '%s' is a private address too. "
                 "Ignoring.", fmt_addr(&interface_ip));
        } else {
          from_interface = 1;
          addr = tor_addr_to_ipv4h(&interface_ip);
          log_fn(notice_severity, LD_CONFIG,
                 "Learned IP address '%s' for local interface."
                 " Using that.", fmt_addr32(addr));
          strlcpy(hostname, "<guessed from interfaces>", sizeof(hostname));
        }
      }
    }
  } else {
    addr = ntohl(in.s_addr); /* set addr so that addr_string is not
                              * illformed */
  }

  /*
   * Step three: Check whether 'addr' is an internal IP address, and error
   * out if it is and we don't want that.
   */

  tor_addr_from_ipv4h(&myaddr,addr);

  addr_string = tor_dup_ip(addr);
  if (tor_addr_is_internal(&myaddr, 0)) {
    /* make sure we're ok with publishing an internal IP */
    if (!options->DirAuthorities && !options->AlternateDirAuthority) {
      /* if they are using the default authorities, disallow internal IPs
       * always. */
      log_fn(warn_severity, LD_CONFIG,
             "Address '%s' resolves to private IP address '%s'. "
             "Tor servers that use the default DirAuthorities must have "
             "public IP addresses.", hostname, addr_string);
      tor_free(addr_string);
      return -1;
    }
    if (!explicit_ip) {
      /* even if they've set their own authorities, require an explicit IP if
       * they're using an internal address. */
      log_fn(warn_severity, LD_CONFIG, "Address '%s' resolves to private "
             "IP address '%s'. Please set the Address config option to be "
             "the IP address you want to use.", hostname, addr_string);
      tor_free(addr_string);
      return -1;
    }
  }

  /*
   * Step four: We have a winner! 'addr' is our answer for sure, and
   * 'addr_string' is its string form. Fill out the various fields to
   * say how we decided it.
   */

  log_debug(LD_CONFIG, "Resolved Address to '%s'.", addr_string);

  if (explicit_ip) {
    method_used = "CONFIGURED";
    hostname_used = NULL;
  } else if (explicit_hostname) {
    method_used = "RESOLVED";
    hostname_used = hostname;
  } else if (from_interface) {
    method_used = "INTERFACE";
    hostname_used = NULL;
  } else {
    method_used = "GETHOSTNAME";
    hostname_used = hostname;
  }

  *addr_out = addr;
  if (method_out)
    *method_out = method_used;
  if (hostname_out)
    *hostname_out = hostname_used ? tor_strdup(hostname_used) : NULL;

  /*
   * Step five: Check if the answer has changed since last time (or if
   * there was no last time), and if so call various functions to keep
   * us up-to-date.
   */

  if (last_resolved_addr && last_resolved_addr != *addr_out) {
    /* Leave this as a notice, regardless of the requested severity,
     * at least until dynamic IP address support becomes bulletproof. */
    log_notice(LD_NET,
               "Your IP address seems to have changed to %s "
               "(METHOD=%s%s%s). Updating.",
               addr_string, method_used,
               hostname_used ? " HOSTNAME=" : "",
               hostname_used ? hostname_used : "");
    ip_address_changed(0);
  }

  if (last_resolved_addr != *addr_out) {
    control_event_server_status(LOG_NOTICE,
                                "EXTERNAL_ADDRESS ADDRESS=%s METHOD=%s%s%s",
                                addr_string, method_used,
                                hostname_used ? " HOSTNAME=" : "",
                                hostname_used ? hostname_used : "");
  }
  last_resolved_addr = *addr_out;

  /*
   * And finally, clean up and return success.
   */

  tor_free(addr_string);
  return 0;
}

/** Return true iff <b>addr</b> is judged to be on the same network as us, or
 * on a private network.
 */
int
is_local_addr(const tor_addr_t *addr)
{
  if (tor_addr_is_internal(addr, 0))
    return 1;
  /* Check whether ip is on the same /24 as we are. */
  if (get_options()->EnforceDistinctSubnets == 0)
    return 0;
  if (tor_addr_family(addr) == AF_INET) {
    /*XXXX023 IP6 what corresponds to an /24? */
    uint32_t ip = tor_addr_to_ipv4h(addr);

    /* It's possible that this next check will hit before the first time
     * resolve_my_address actually succeeds.  (For clients, it is likely that
     * resolve_my_address will never be called at all).  In those cases,
     * last_resolved_addr will be 0, and so checking to see whether ip is on
     * the same /24 as last_resolved_addr will be the same as checking whether
     * it was on net 0, which is already done by tor_addr_is_internal.
     */
    if ((last_resolved_addr & (uint32_t)0xffffff00ul)
        == (ip & (uint32_t)0xffffff00ul))
      return 1;
  }
  return 0;
}

/** Return a new empty or_options_t.  Used for testing. */
or_options_t *
options_new(void)
{
  return config_new(&options_format);
}

/** Set <b>options</b> to hold reasonable defaults for most options.
 * Each option defaults to zero. */
void
options_init(or_options_t *options)
{
  config_init(&options_format, options);
}

/** Return a string containing a possible configuration file that would give
 * the configuration in <b>options</b>.  If <b>minimal</b> is true, do not
 * include options that are the same as Tor's defaults.
 */
char *
options_dump(const or_options_t *options, int how_to_dump)
{
  const or_options_t *use_defaults;
  int minimal;
  switch (how_to_dump) {
    case OPTIONS_DUMP_MINIMAL:
      use_defaults = global_default_options;
      minimal = 1;
      break;
    case OPTIONS_DUMP_DEFAULTS:
      use_defaults = NULL;
      minimal = 1;
      break;
    case OPTIONS_DUMP_ALL:
      use_defaults = NULL;
      minimal = 0;
      break;
    default:
      log_warn(LD_BUG, "Bogus value for how_to_dump==%d", how_to_dump);
      return NULL;
  }

  return config_dump(&options_format, use_defaults, options, minimal, 0);
}

/** Return 0 if every element of sl is a string holding a decimal
 * representation of a port number, or if sl is NULL.
 * Otherwise set *msg and return -1. */
static int
validate_ports_csv(smartlist_t *sl, const char *name, char **msg)
{
  int i;
  tor_assert(name);

  if (!sl)
    return 0;

  SMARTLIST_FOREACH(sl, const char *, cp,
  {
    i = atoi(cp);
    if (i < 1 || i > 65535) {
      tor_asprintf(msg, "Port '%s' out of range in %s", cp, name);
      return -1;
    }
  });
  return 0;
}

/** If <b>value</b> exceeds ROUTER_MAX_DECLARED_BANDWIDTH, write
 * a complaint into *<b>msg</b> using string <b>desc</b>, and return -1.
 * Else return 0.
 */
static int
ensure_bandwidth_cap(uint64_t *value, const char *desc, char **msg)
{
  if (*value > ROUTER_MAX_DECLARED_BANDWIDTH) {
    /* This handles an understandable special case where somebody says "2gb"
     * whereas our actual maximum is 2gb-1 (INT_MAX) */
    --*value;
  }
  if (*value > ROUTER_MAX_DECLARED_BANDWIDTH) {
    tor_asprintf(msg, "%s ("U64_FORMAT") must be at most %d",
                 desc, U64_PRINTF_ARG(*value),
                 ROUTER_MAX_DECLARED_BANDWIDTH);
    return -1;
  }
  return 0;
}

/** Parse an authority type from <b>options</b>-\>PublishServerDescriptor
 * and write it to <b>options</b>-\>PublishServerDescriptor_. Treat "1"
 * as "v3" unless BridgeRelay is 1, in which case treat it as "bridge".
 * Treat "0" as "".
 * Return 0 on success or -1 if not a recognized authority type (in which
 * case the value of PublishServerDescriptor_ is undefined). */
static int
compute_publishserverdescriptor(or_options_t *options)
{
  smartlist_t *list = options->PublishServerDescriptor;
  dirinfo_type_t *auth = &options->PublishServerDescriptor_;
  *auth = NO_DIRINFO;
  if (!list) /* empty list, answer is none */
    return 0;
  SMARTLIST_FOREACH_BEGIN(list, const char *, string) {
    if (!strcasecmp(string, "v1"))
      log_warn(LD_CONFIG, "PublishServerDescriptor v1 has no effect, because "
                          "there are no v1 directory authorities anymore.");
    else if (!strcmp(string, "1"))
      if (options->BridgeRelay)
        *auth |= BRIDGE_DIRINFO;
      else
        *auth |= V3_DIRINFO;
    else if (!strcasecmp(string, "v2"))
      log_warn(LD_CONFIG, "PublishServerDescriptor v2 has no effect, because "
                          "there are no v2 directory authorities anymore.");
    else if (!strcasecmp(string, "v3"))
      *auth |= V3_DIRINFO;
    else if (!strcasecmp(string, "bridge"))
      *auth |= BRIDGE_DIRINFO;
    else if (!strcasecmp(string, "hidserv"))
      log_warn(LD_CONFIG,
               "PublishServerDescriptor hidserv is invalid. See "
               "PublishHidServDescriptors.");
    else if (!strcasecmp(string, "") || !strcmp(string, "0"))
      /* no authority */;
    else
      return -1;
  } SMARTLIST_FOREACH_END(string);
  return 0;
}

/** Lowest allowable value for RendPostPeriod; if this is too low, hidden
 * services can overload the directory system. */
#define MIN_REND_POST_PERIOD (10*60)

/** Higest allowable value for PredictedPortsRelevanceTime; if this is
 * too high, our selection of exits will decrease for an extended
 * period of time to an uncomfortable level .*/
#define MAX_PREDICTED_CIRCS_RELEVANCE (60*60)

/** Highest allowable value for RendPostPeriod. */
#define MAX_DIR_PERIOD (MIN_ONION_KEY_LIFETIME/2)

/** Lowest allowable value for MaxCircuitDirtiness; if this is too low, Tor
 * will generate too many circuits and potentially overload the network. */
#define MIN_MAX_CIRCUIT_DIRTINESS 10

/** Highest allowable value for MaxCircuitDirtiness: prevents time_t
 * overflows. */
#define MAX_MAX_CIRCUIT_DIRTINESS (30*24*60*60)

/** Lowest allowable value for CircuitStreamTimeout; if this is too low, Tor
 * will generate too many circuits and potentially overload the network. */
#define MIN_CIRCUIT_STREAM_TIMEOUT 10

/** Lowest allowable value for HeartbeatPeriod; if this is too low, we might
 * expose more information than we're comfortable with. */
#define MIN_HEARTBEAT_PERIOD (30*60)

/** Lowest recommended value for CircuitBuildTimeout; if it is set too low
 * and LearnCircuitBuildTimeout is off, the failure rate for circuit
 * construction may be very high.  In that case, if it is set below this
 * threshold emit a warning.
 * */
#define RECOMMENDED_MIN_CIRCUIT_BUILD_TIMEOUT (10)

static int
options_validate_cb(void *old_options, void *options, void *default_options,
                    int from_setconf, char **msg)
{
  return options_validate(old_options, options, default_options,
                          from_setconf, msg);
}

/** Return 0 if every setting in <b>options</b> is reasonable, is a
 * permissible transition from <b>old_options</b>, and none of the
 * testing-only settings differ from <b>default_options</b> unless in
 * testing mode.  Else return -1.  Should have no side effects, except for
 * normalizing the contents of <b>options</b>.
 *
 * On error, tor_strdup an error explanation into *<b>msg</b>.
 *
 * XXX
 * If <b>from_setconf</b>, we were called by the controller, and our
 * Log line should stay empty. If it's 0, then give us a default log
 * if there are no logs defined.
 */
STATIC int
options_validate(or_options_t *old_options, or_options_t *options,
                 or_options_t *default_options, int from_setconf, char **msg)
{
  int i;
  config_line_t *cl;
  const char *uname = get_uname();
  int n_ports=0;
#define REJECT(arg) \
  STMT_BEGIN *msg = tor_strdup(arg); return -1; STMT_END
#define COMPLAIN(arg) STMT_BEGIN log_warn(LD_CONFIG, arg); STMT_END

  tor_assert(msg);
  *msg = NULL;

  if (server_mode(options) &&
      (!strcmpstart(uname, "Windows 95") ||
       !strcmpstart(uname, "Windows 98") ||
       !strcmpstart(uname, "Windows Me"))) {
    log_warn(LD_CONFIG, "Tor is running as a server, but you are "
        "running %s; this probably won't work. See "
        "https://www.torproject.org/docs/faq.html#BestOSForRelay "
        "for details.", uname);
  }

  if (parse_ports(options, 1, msg, &n_ports) < 0)
    return -1;

  if (parse_outbound_addresses(options, 1, msg) < 0)
    return -1;

  if (validate_data_directory(options)<0)
    REJECT("Invalid DataDirectory");

  if (options->Nickname == NULL) {
    if (server_mode(options)) {
        options->Nickname = tor_strdup(UNNAMED_ROUTER_NICKNAME);
    }
  } else {
    if (!is_legal_nickname(options->Nickname)) {
      tor_asprintf(msg,
          "Nickname '%s' is wrong length or contains illegal characters.",
          options->Nickname);
      return -1;
    }
  }

  if (server_mode(options) && !options->ContactInfo)
    log_notice(LD_CONFIG, "Your ContactInfo config option is not set. "
        "Please consider setting it, so we can contact you if your server is "
        "misconfigured or something else goes wrong.");

  /* Special case on first boot if no Log options are given. */
  if (!options->Logs && !options->RunAsDaemon && !from_setconf) {
    if (quiet_level == 0)
        config_line_append(&options->Logs, "Log", "notice stdout");
    else if (quiet_level == 1)
        config_line_append(&options->Logs, "Log", "warn stdout");
  }

  if (options_init_logs(options, 1)<0) /* Validate the tor_log(s) */
    REJECT("Failed to validate Log options. See logs for details.");

  if (authdir_mode(options)) {
    /* confirm that our address isn't broken, so we can complain now */
    uint32_t tmp;
    if (resolve_my_address(LOG_WARN, options, &tmp, NULL, NULL) < 0)
      REJECT("Failed to resolve/guess local address. See logs for details.");
  }

#ifndef _WIN32
  if (options->RunAsDaemon && torrc_fname && path_is_relative(torrc_fname))
    REJECT("Can't use a relative path to torrc when RunAsDaemon is set.");
#endif

  if (server_mode(options) && options->RendConfigLines)
    log_warn(LD_CONFIG,
        "Tor is currently configured as a relay and a hidden service. "
        "That's not very secure: you should probably run your hidden service "
        "in a separate Tor process, at least -- see "
        "https://trac.torproject.org/8742");

  /* XXXX require that the only port not be DirPort? */
  /* XXXX require that at least one port be listened-upon. */
  if (n_ports == 0 && !options->RendConfigLines)
    log_warn(LD_CONFIG,
        "SocksPort, TransPort, NATDPort, DNSPort, and ORPort are all "
        "undefined, and there aren't any hidden services configured.  "
        "Tor will still run, but probably won't do anything.");

  options->TransProxyType_parsed = TPT_DEFAULT;
#ifdef USE_TRANSPARENT
  if (options->TransProxyType) {
    if (!strcasecmp(options->TransProxyType, "default")) {
      options->TransProxyType_parsed = TPT_DEFAULT;
    } else if (!strcasecmp(options->TransProxyType, "pf-divert")) {
#ifndef __OpenBSD__
      REJECT("pf-divert is a OpenBSD-specific feature.");
#else
      options->TransProxyType_parsed = TPT_PF_DIVERT;
#endif
    } else if (!strcasecmp(options->TransProxyType, "tproxy")) {
#ifndef __linux__
      REJECT("TPROXY is a Linux-specific feature.");
#else
      options->TransProxyType_parsed = TPT_TPROXY;
#endif
    } else if (!strcasecmp(options->TransProxyType, "ipfw")) {
#ifndef __FreeBSD__
      REJECT("ipfw is a FreeBSD-specific feature.");
#else
      options->TransProxyType_parsed = TPT_IPFW;
#endif
    } else {
      REJECT("Unrecognized value for TransProxyType");
    }

    if (strcasecmp(options->TransProxyType, "default") &&
        !options->TransPort_set) {
      REJECT("Cannot use TransProxyType without any valid TransPort or "
             "TransListenAddress.");
    }
  }
#else
  if (options->TransPort_set)
    REJECT("TransPort and TransListenAddress are disabled "
           "in this build.");
#endif

  if (options->TokenBucketRefillInterval <= 0
      || options->TokenBucketRefillInterval > 1000) {
    REJECT("TokenBucketRefillInterval must be between 1 and 1000 inclusive.");
  }

  if (options->ExcludeExitNodes || options->ExcludeNodes) {
    options->ExcludeExitNodesUnion_ = routerset_new();
    routerset_union(options->ExcludeExitNodesUnion_,options->ExcludeExitNodes);
    routerset_union(options->ExcludeExitNodesUnion_,options->ExcludeNodes);
  }

  if (options->NodeFamilies) {
    options->NodeFamilySets = smartlist_new();
    for (cl = options->NodeFamilies; cl; cl = cl->next) {
      routerset_t *rs = routerset_new();
      if (routerset_parse(rs, cl->value, cl->key) == 0) {
        smartlist_add(options->NodeFamilySets, rs);
      } else {
        routerset_free(rs);
      }
    }
  }

  if (options->TLSECGroup && (strcasecmp(options->TLSECGroup, "P256") &&
                              strcasecmp(options->TLSECGroup, "P224"))) {
    COMPLAIN("Unrecognized TLSECGroup: Falling back to the default.");
    tor_free(options->TLSECGroup);
  }

  if (options->ExcludeNodes && options->StrictNodes) {
    COMPLAIN("You have asked to exclude certain relays from all positions "
             "in your circuits. Expect hidden services and other Tor "
             "features to be broken in unpredictable ways.");
  }

  if (options->AuthoritativeDir) {
    if (!options->ContactInfo && !options->TestingTorNetwork)
      REJECT("Authoritative directory servers must set ContactInfo");
    if (!options->RecommendedClientVersions)
      options->RecommendedClientVersions =
        config_lines_dup(options->RecommendedVersions);
    if (!options->RecommendedServerVersions)
      options->RecommendedServerVersions =
        config_lines_dup(options->RecommendedVersions);
    if (options->VersioningAuthoritativeDir &&
        (!options->RecommendedClientVersions ||
         !options->RecommendedServerVersions))
      REJECT("Versioning authoritative dir servers must set "
             "Recommended*Versions.");
    if (options->UseEntryGuards) {
      log_info(LD_CONFIG, "Authoritative directory servers can't set "
               "UseEntryGuards. Disabling.");
      options->UseEntryGuards = 0;
    }
    if (!options->DownloadExtraInfo && authdir_mode_any_main(options)) {
      log_info(LD_CONFIG, "Authoritative directories always try to download "
               "extra-info documents. Setting DownloadExtraInfo.");
      options->DownloadExtraInfo = 1;
    }
    if (!(options->BridgeAuthoritativeDir ||
          options->V3AuthoritativeDir))
      REJECT("AuthoritativeDir is set, but none of "
             "(Bridge/V3)AuthoritativeDir is set.");
    /* If we have a v3bandwidthsfile and it's broken, complain on startup */
    if (options->V3BandwidthsFile && !old_options) {
      dirserv_read_measured_bandwidths(options->V3BandwidthsFile, NULL);
    }
  }

  if (options->AuthoritativeDir && !options->DirPort_set)
    REJECT("Running as authoritative directory, but no DirPort set.");

  if (options->AuthoritativeDir && !options->ORPort_set)
    REJECT("Running as authoritative directory, but no ORPort set.");

  if (options->AuthoritativeDir && options->ClientOnly)
    REJECT("Running as authoritative directory, but ClientOnly also set.");

  if (options->FetchDirInfoExtraEarly && !options->FetchDirInfoEarly)
    REJECT("FetchDirInfoExtraEarly requires that you also set "
           "FetchDirInfoEarly");

  if (options->ConnLimit <= 0) {
    tor_asprintf(msg,
        "ConnLimit must be greater than 0, but was set to %d",
        options->ConnLimit);
    return -1;
  }

  if (options->PathsNeededToBuildCircuits >= 0.0) {
    if (options->PathsNeededToBuildCircuits < 0.25) {
      log_warn(LD_CONFIG, "PathsNeededToBuildCircuits is too low. Increasing "
               "to 0.25");
      options->PathsNeededToBuildCircuits = 0.25;
    } else if (options->PathsNeededToBuildCircuits > 0.95) {
      log_warn(LD_CONFIG, "PathsNeededToBuildCircuits is too high. Decreasing "
               "to 0.95");
      options->PathsNeededToBuildCircuits = 0.95;
    }
  }

  if (options->MaxClientCircuitsPending <= 0 ||
      options->MaxClientCircuitsPending > MAX_MAX_CLIENT_CIRCUITS_PENDING) {
    tor_asprintf(msg,
                 "MaxClientCircuitsPending must be between 1 and %d, but "
                 "was set to %d", MAX_MAX_CLIENT_CIRCUITS_PENDING,
                 options->MaxClientCircuitsPending);
    return -1;
  }

  if (validate_ports_csv(options->FirewallPorts, "FirewallPorts", msg) < 0)
    return -1;

  if (validate_ports_csv(options->LongLivedPorts, "LongLivedPorts", msg) < 0)
    return -1;

  if (validate_ports_csv(options->RejectPlaintextPorts,
                         "RejectPlaintextPorts", msg) < 0)
    return -1;

  if (validate_ports_csv(options->WarnPlaintextPorts,
                         "WarnPlaintextPorts", msg) < 0)
    return -1;

  if (options->FascistFirewall && !options->ReachableAddresses) {
    if (options->FirewallPorts && smartlist_len(options->FirewallPorts)) {
      /* We already have firewall ports set, so migrate them to
       * ReachableAddresses, which will set ReachableORAddresses and
       * ReachableDirAddresses if they aren't set explicitly. */
      smartlist_t *instead = smartlist_new();
      config_line_t *new_line = tor_malloc_zero(sizeof(config_line_t));
      new_line->key = tor_strdup("ReachableAddresses");
      /* If we're configured with the old format, we need to prepend some
       * open ports. */
      SMARTLIST_FOREACH(options->FirewallPorts, const char *, portno,
      {
        int p = atoi(portno);
        if (p<0) continue;
        smartlist_add_asprintf(instead, "*:%d", p);
      });
      new_line->value = smartlist_join_strings(instead,",",0,NULL);
      /* These have been deprecated since 0.1.1.5-alpha-cvs */
      log_notice(LD_CONFIG,
          "Converting FascistFirewall and FirewallPorts "
          "config options to new format: \"ReachableAddresses %s\"",
          new_line->value);
      options->ReachableAddresses = new_line;
      SMARTLIST_FOREACH(instead, char *, cp, tor_free(cp));
      smartlist_free(instead);
    } else {
      /* We do not have FirewallPorts set, so add 80 to
       * ReachableDirAddresses, and 443 to ReachableORAddresses. */
      if (!options->ReachableDirAddresses) {
        config_line_t *new_line = tor_malloc_zero(sizeof(config_line_t));
        new_line->key = tor_strdup("ReachableDirAddresses");
        new_line->value = tor_strdup("*:80");
        options->ReachableDirAddresses = new_line;
        log_notice(LD_CONFIG, "Converting FascistFirewall config option "
            "to new format: \"ReachableDirAddresses *:80\"");
      }
      if (!options->ReachableORAddresses) {
        config_line_t *new_line = tor_malloc_zero(sizeof(config_line_t));
        new_line->key = tor_strdup("ReachableORAddresses");
        new_line->value = tor_strdup("*:443");
        options->ReachableORAddresses = new_line;
        log_notice(LD_CONFIG, "Converting FascistFirewall config option "
            "to new format: \"ReachableORAddresses *:443\"");
      }
    }
  }

  for (i=0; i<3; i++) {
    config_line_t **linep =
      (i==0) ? &options->ReachableAddresses :
        (i==1) ? &options->ReachableORAddresses :
                 &options->ReachableDirAddresses;
    if (!*linep)
      continue;
    /* We need to end with a reject *:*, not an implicit accept *:* */
    for (;;) {
      if (!strcmp((*linep)->value, "reject *:*")) /* already there */
        break;
      linep = &((*linep)->next);
      if (!*linep) {
        *linep = tor_malloc_zero(sizeof(config_line_t));
        (*linep)->key = tor_strdup(
          (i==0) ?  "ReachableAddresses" :
            (i==1) ? "ReachableORAddresses" :
                     "ReachableDirAddresses");
        (*linep)->value = tor_strdup("reject *:*");
        break;
      }
    }
  }

  if ((options->ReachableAddresses ||
       options->ReachableORAddresses ||
       options->ReachableDirAddresses) &&
      server_mode(options))
    REJECT("Servers must be able to freely connect to the rest "
           "of the Internet, so they must not set Reachable*Addresses "
           "or FascistFirewall.");

  if (options->UseBridges &&
      server_mode(options))
    REJECT("Servers must be able to freely connect to the rest "
           "of the Internet, so they must not set UseBridges.");

  /* If both of these are set, we'll end up with funny behavior where we
   * demand enough entrynodes be up and running else we won't build
   * circuits, yet we never actually use them. */
  if (options->UseBridges && options->EntryNodes)
    REJECT("You cannot set both UseBridges and EntryNodes.");

  if (options->EntryNodes && !options->UseEntryGuards) {
    REJECT("If EntryNodes is set, UseEntryGuards must be enabled.");
  }

  options->MaxMemInQueues =
    compute_real_max_mem_in_queues(options->MaxMemInQueues_raw,
                                   server_mode(options));

  options->AllowInvalid_ = 0;

  if (options->AllowInvalidNodes) {
    SMARTLIST_FOREACH_BEGIN(options->AllowInvalidNodes, const char *, cp) {
        if (!strcasecmp(cp, "entry"))
          options->AllowInvalid_ |= ALLOW_INVALID_ENTRY;
        else if (!strcasecmp(cp, "exit"))
          options->AllowInvalid_ |= ALLOW_INVALID_EXIT;
        else if (!strcasecmp(cp, "middle"))
          options->AllowInvalid_ |= ALLOW_INVALID_MIDDLE;
        else if (!strcasecmp(cp, "introduction"))
          options->AllowInvalid_ |= ALLOW_INVALID_INTRODUCTION;
        else if (!strcasecmp(cp, "rendezvous"))
          options->AllowInvalid_ |= ALLOW_INVALID_RENDEZVOUS;
        else {
          tor_asprintf(msg,
              "Unrecognized value '%s' in AllowInvalidNodes", cp);
          return -1;
        }
    } SMARTLIST_FOREACH_END(cp);
  }

  if (!options->SafeLogging ||
      !strcasecmp(options->SafeLogging, "0")) {
    options->SafeLogging_ = SAFELOG_SCRUB_NONE;
  } else if (!strcasecmp(options->SafeLogging, "relay")) {
    options->SafeLogging_ = SAFELOG_SCRUB_RELAY;
  } else if (!strcasecmp(options->SafeLogging, "1")) {
    options->SafeLogging_ = SAFELOG_SCRUB_ALL;
  } else {
    tor_asprintf(msg,
                     "Unrecognized value '%s' in SafeLogging",
                     escaped(options->SafeLogging));
    return -1;
  }

  if (compute_publishserverdescriptor(options) < 0) {
    tor_asprintf(msg, "Unrecognized value in PublishServerDescriptor");
    return -1;
  }

  if ((options->BridgeRelay
        || options->PublishServerDescriptor_ & BRIDGE_DIRINFO)
      && (options->PublishServerDescriptor_ & V3_DIRINFO)) {
    REJECT("Bridges are not supposed to publish router descriptors to the "
           "directory authorities. Please correct your "
           "PublishServerDescriptor line.");
  }

  if (options->BridgeRelay && options->DirPort_set) {
    log_warn(LD_CONFIG, "Can't set a DirPort on a bridge relay; disabling "
             "DirPort");
    config_free_lines(options->DirPort_lines);
    options->DirPort_lines = NULL;
    options->DirPort_set = 0;
  }

  if (options->MinUptimeHidServDirectoryV2 < 0) {
    log_warn(LD_CONFIG, "MinUptimeHidServDirectoryV2 option must be at "
                        "least 0 seconds. Changing to 0.");
    options->MinUptimeHidServDirectoryV2 = 0;
  }

  if (options->RendPostPeriod < MIN_REND_POST_PERIOD) {
    log_warn(LD_CONFIG, "RendPostPeriod option is too short; "
             "raising to %d seconds.", MIN_REND_POST_PERIOD);
    options->RendPostPeriod = MIN_REND_POST_PERIOD;
  }

  if (options->RendPostPeriod > MAX_DIR_PERIOD) {
    log_warn(LD_CONFIG, "RendPostPeriod is too large; clipping to %ds.",
             MAX_DIR_PERIOD);
    options->RendPostPeriod = MAX_DIR_PERIOD;
  }

  if (options->PredictedPortsRelevanceTime >
      MAX_PREDICTED_CIRCS_RELEVANCE) {
    log_warn(LD_CONFIG, "PredictedPortsRelevanceTime is too large; "
             "clipping to %ds.", MAX_PREDICTED_CIRCS_RELEVANCE);
    options->PredictedPortsRelevanceTime = MAX_PREDICTED_CIRCS_RELEVANCE;
  }

  if (options->Tor2webMode && options->LearnCircuitBuildTimeout) {
    /* LearnCircuitBuildTimeout and Tor2webMode are incompatible in
     * two ways:
     *
     * - LearnCircuitBuildTimeout results in a low CBT, which
     *   Tor2webMode's use of one-hop rendezvous circuits lowers
     *   much further, producing *far* too many timeouts.
     *
     * - The adaptive CBT code does not update its timeout estimate
     *   using build times for single-hop circuits.
     *
     * If we fix both of these issues someday, we should test
     * Tor2webMode with LearnCircuitBuildTimeout on again. */
    log_notice(LD_CONFIG,"Tor2webMode is enabled; turning "
               "LearnCircuitBuildTimeout off.");
    options->LearnCircuitBuildTimeout = 0;
  }

  if (options->Tor2webMode && options->UseEntryGuards) {
    /* tor2web mode clients do not (and should not) use entry guards
     * in any meaningful way.  Further, tor2web mode causes the hidden
     * service client code to do things which break the path bias
     * detector, and it's far easier to turn off entry guards (and
     * thus the path bias detector with it) than to figure out how to
     * make a piece of code which cannot possibly help tor2web mode
     * users compatible with tor2web mode.
     */
    log_notice(LD_CONFIG,
               "Tor2WebMode is enabled; disabling UseEntryGuards.");
    options->UseEntryGuards = 0;
  }

  if (!(options->UseEntryGuards) &&
      (options->RendConfigLines != NULL)) {
    log_warn(LD_CONFIG,
             "UseEntryGuards is disabled, but you have configured one or more "
             "hidden services on this Tor instance.  Your hidden services "
             "will be very easy to locate using a well-known attack -- see "
             "http://freehaven.net/anonbib/#hs-attack06 for details.");
  }

  if (!options->LearnCircuitBuildTimeout && options->CircuitBuildTimeout &&
      options->CircuitBuildTimeout < RECOMMENDED_MIN_CIRCUIT_BUILD_TIMEOUT) {
    log_warn(LD_CONFIG,
        "CircuitBuildTimeout is shorter (%d seconds) than the recommended "
        "minimum (%d seconds), and LearnCircuitBuildTimeout is disabled.  "
        "If tor isn't working, raise this value or enable "
        "LearnCircuitBuildTimeout.",
        options->CircuitBuildTimeout,
        RECOMMENDED_MIN_CIRCUIT_BUILD_TIMEOUT );
  } else if (!options->LearnCircuitBuildTimeout &&
             !options->CircuitBuildTimeout) {
    log_notice(LD_CONFIG, "You disabled LearnCircuitBuildTimeout, but didn't "
               "a CircuitBuildTimeout. I'll pick a plausible default.");
  }

  if (options->PathBiasNoticeRate > 1.0) {
    tor_asprintf(msg,
              "PathBiasNoticeRate is too high. "
              "It must be between 0 and 1.0");
    return -1;
  }
  if (options->PathBiasWarnRate > 1.0) {
    tor_asprintf(msg,
              "PathBiasWarnRate is too high. "
              "It must be between 0 and 1.0");
    return -1;
  }
  if (options->PathBiasExtremeRate > 1.0) {
    tor_asprintf(msg,
              "PathBiasExtremeRate is too high. "
              "It must be between 0 and 1.0");
    return -1;
  }
  if (options->PathBiasNoticeUseRate > 1.0) {
    tor_asprintf(msg,
              "PathBiasNoticeUseRate is too high. "
              "It must be between 0 and 1.0");
    return -1;
  }
  if (options->PathBiasExtremeUseRate > 1.0) {
    tor_asprintf(msg,
              "PathBiasExtremeUseRate is too high. "
              "It must be between 0 and 1.0");
    return -1;
  }

  if (options->MaxCircuitDirtiness < MIN_MAX_CIRCUIT_DIRTINESS) {
    log_warn(LD_CONFIG, "MaxCircuitDirtiness option is too short; "
             "raising to %d seconds.", MIN_MAX_CIRCUIT_DIRTINESS);
    options->MaxCircuitDirtiness = MIN_MAX_CIRCUIT_DIRTINESS;
  }

  if (options->MaxCircuitDirtiness > MAX_MAX_CIRCUIT_DIRTINESS) {
    log_warn(LD_CONFIG, "MaxCircuitDirtiness option is too high; "
             "setting to %d days.", MAX_MAX_CIRCUIT_DIRTINESS/86400);
    options->MaxCircuitDirtiness = MAX_MAX_CIRCUIT_DIRTINESS;
  }

  if (options->CircuitStreamTimeout &&
      options->CircuitStreamTimeout < MIN_CIRCUIT_STREAM_TIMEOUT) {
    log_warn(LD_CONFIG, "CircuitStreamTimeout option is too short; "
             "raising to %d seconds.", MIN_CIRCUIT_STREAM_TIMEOUT);
    options->CircuitStreamTimeout = MIN_CIRCUIT_STREAM_TIMEOUT;
  }

  if (options->HeartbeatPeriod &&
      options->HeartbeatPeriod < MIN_HEARTBEAT_PERIOD) {
    log_warn(LD_CONFIG, "HeartbeatPeriod option is too short; "
             "raising to %d seconds.", MIN_HEARTBEAT_PERIOD);
    options->HeartbeatPeriod = MIN_HEARTBEAT_PERIOD;
  }

  if (options->KeepalivePeriod < 1)
    REJECT("KeepalivePeriod option must be positive.");

  if (options->PortForwarding && options->Sandbox) {
    REJECT("PortForwarding is not compatible with Sandbox; at most one can "
           "be set");
  }

  if (ensure_bandwidth_cap(&options->BandwidthRate,
                           "BandwidthRate", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->BandwidthBurst,
                           "BandwidthBurst", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->MaxAdvertisedBandwidth,
                           "MaxAdvertisedBandwidth", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->RelayBandwidthRate,
                           "RelayBandwidthRate", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->RelayBandwidthBurst,
                           "RelayBandwidthBurst", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->PerConnBWRate,
                           "PerConnBWRate", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->PerConnBWBurst,
                           "PerConnBWBurst", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->AuthDirFastGuarantee,
                           "AuthDirFastGuarantee", msg) < 0)
    return -1;
  if (ensure_bandwidth_cap(&options->AuthDirGuardBWGuarantee,
                           "AuthDirGuardBWGuarantee", msg) < 0)
    return -1;

  if (options->RelayBandwidthRate && !options->RelayBandwidthBurst)
    options->RelayBandwidthBurst = options->RelayBandwidthRate;
  if (options->RelayBandwidthBurst && !options->RelayBandwidthRate)
    options->RelayBandwidthRate = options->RelayBandwidthBurst;

  if (server_mode(options)) {
    if (options->BandwidthRate < ROUTER_REQUIRED_MIN_BANDWIDTH) {
      tor_asprintf(msg,
                       "BandwidthRate is set to %d bytes/second. "
                       "For servers, it must be at least %d.",
                       (int)options->BandwidthRate,
                       ROUTER_REQUIRED_MIN_BANDWIDTH);
      return -1;
    } else if (options->MaxAdvertisedBandwidth <
               ROUTER_REQUIRED_MIN_BANDWIDTH/2) {
      tor_asprintf(msg,
                       "MaxAdvertisedBandwidth is set to %d bytes/second. "
                       "For servers, it must be at least %d.",
                       (int)options->MaxAdvertisedBandwidth,
                       ROUTER_REQUIRED_MIN_BANDWIDTH/2);
      return -1;
    }
    if (options->RelayBandwidthRate &&
      options->RelayBandwidthRate < ROUTER_REQUIRED_MIN_BANDWIDTH) {
      tor_asprintf(msg,
                       "RelayBandwidthRate is set to %d bytes/second. "
                       "For servers, it must be at least %d.",
                       (int)options->RelayBandwidthRate,
                       ROUTER_REQUIRED_MIN_BANDWIDTH);
      return -1;
    }
  }

  if (options->RelayBandwidthRate > options->RelayBandwidthBurst)
    REJECT("RelayBandwidthBurst must be at least equal "
           "to RelayBandwidthRate.");

  if (options->BandwidthRate > options->BandwidthBurst)
    REJECT("BandwidthBurst must be at least equal to BandwidthRate.");

  /* if they set relaybandwidth* really high but left bandwidth*
   * at the default, raise the defaults. */
  if (options->RelayBandwidthRate > options->BandwidthRate)
    options->BandwidthRate = options->RelayBandwidthRate;
  if (options->RelayBandwidthBurst > options->BandwidthBurst)
    options->BandwidthBurst = options->RelayBandwidthBurst;

  if (accounting_parse_options(options, 1)<0)
    REJECT("Failed to parse accounting options. See logs for details.");

  if (options->AccountingMax) {
    if (options->RendConfigLines && server_mode(options)) {
      log_warn(LD_CONFIG, "Using accounting with a hidden service and an "
               "ORPort is risky: your hidden service(s) and your public "
               "address will all turn off at the same time, which may alert "
               "observers that they are being run by the same party.");
    } else if (config_count_key(options->RendConfigLines,
                                "HiddenServiceDir") > 1) {
      log_warn(LD_CONFIG, "Using accounting with multiple hidden services is "
               "risky: they will all turn off at the same time, which may "
               "alert observers that they are being run by the same party.");
    }
  }

  if (options->HTTPProxy) { /* parse it now */
    if (tor_addr_port_lookup(options->HTTPProxy,
                        &options->HTTPProxyAddr, &options->HTTPProxyPort) < 0)
      REJECT("HTTPProxy failed to parse or resolve. Please fix.");
    if (options->HTTPProxyPort == 0) { /* give it a default */
      options->HTTPProxyPort = 80;
    }
  }

  if (options->HTTPProxyAuthenticator) {
    if (strlen(options->HTTPProxyAuthenticator) >= 512)
      REJECT("HTTPProxyAuthenticator is too long (>= 512 chars).");
  }

  if (options->HTTPSProxy) { /* parse it now */
    if (tor_addr_port_lookup(options->HTTPSProxy,
                        &options->HTTPSProxyAddr, &options->HTTPSProxyPort) <0)
      REJECT("HTTPSProxy failed to parse or resolve. Please fix.");
    if (options->HTTPSProxyPort == 0) { /* give it a default */
      options->HTTPSProxyPort = 443;
    }
  }

  if (options->HTTPSProxyAuthenticator) {
    if (strlen(options->HTTPSProxyAuthenticator) >= 512)
      REJECT("HTTPSProxyAuthenticator is too long (>= 512 chars).");
  }

  if (options->Socks4Proxy) { /* parse it now */
    if (tor_addr_port_lookup(options->Socks4Proxy,
                        &options->Socks4ProxyAddr,
                        &options->Socks4ProxyPort) <0)
      REJECT("Socks4Proxy failed to parse or resolve. Please fix.");
    if (options->Socks4ProxyPort == 0) { /* give it a default */
      options->Socks4ProxyPort = 1080;
    }
  }

  if (options->Socks5Proxy) { /* parse it now */
    if (tor_addr_port_lookup(options->Socks5Proxy,
                            &options->Socks5ProxyAddr,
                            &options->Socks5ProxyPort) <0)
      REJECT("Socks5Proxy failed to parse or resolve. Please fix.");
    if (options->Socks5ProxyPort == 0) { /* give it a default */
      options->Socks5ProxyPort = 1080;
    }
  }

  /* Check if more than one proxy type has been enabled. */
  if (!!options->Socks4Proxy + !!options->Socks5Proxy +
      !!options->HTTPSProxy + !!options->ClientTransportPlugin > 1)
    REJECT("You have configured more than one proxy type. "
           "(Socks4Proxy|Socks5Proxy|HTTPSProxy|ClientTransportPlugin)");

  /* Check if the proxies will give surprising behavior. */
  if (options->HTTPProxy && !(options->Socks4Proxy ||
                              options->Socks5Proxy ||
                              options->HTTPSProxy)) {
    log_warn(LD_CONFIG, "HTTPProxy configured, but no SOCKS proxy or "
             "HTTPS proxy configured. Watch out: this configuration will "
             "proxy unencrypted directory connections only.");
  }

  if (options->Socks5ProxyUsername) {
    size_t len;

    len = strlen(options->Socks5ProxyUsername);
    if (len < 1 || len > MAX_SOCKS5_AUTH_FIELD_SIZE)
      REJECT("Socks5ProxyUsername must be between 1 and 255 characters.");

    if (!options->Socks5ProxyPassword)
      REJECT("Socks5ProxyPassword must be included with Socks5ProxyUsername.");

    len = strlen(options->Socks5ProxyPassword);
    if (len < 1 || len > MAX_SOCKS5_AUTH_FIELD_SIZE)
      REJECT("Socks5ProxyPassword must be between 1 and 255 characters.");
  } else if (options->Socks5ProxyPassword)
    REJECT("Socks5ProxyPassword must be included with Socks5ProxyUsername.");

  if (options->HashedControlPassword) {
    smartlist_t *sl = decode_hashed_passwords(options->HashedControlPassword);
    if (!sl) {
      REJECT("Bad HashedControlPassword: wrong length or bad encoding");
    } else {
      SMARTLIST_FOREACH(sl, char*, cp, tor_free(cp));
      smartlist_free(sl);
    }
  }

  if (options->HashedControlSessionPassword) {
    smartlist_t *sl = decode_hashed_passwords(
                                  options->HashedControlSessionPassword);
    if (!sl) {
      REJECT("Bad HashedControlSessionPassword: wrong length or bad encoding");
    } else {
      SMARTLIST_FOREACH(sl, char*, cp, tor_free(cp));
      smartlist_free(sl);
    }
  }

  if (options->OwningControllerProcess) {
    const char *validate_pspec_msg = NULL;
    if (tor_validate_process_specifier(options->OwningControllerProcess,
                                       &validate_pspec_msg)) {
      tor_asprintf(msg, "Bad OwningControllerProcess: %s",
                   validate_pspec_msg);
      return -1;
    }
  }

  if (options->ControlPort_set && !options->HashedControlPassword &&
      !options->HashedControlSessionPassword &&
      !options->CookieAuthentication) {
    log_warn(LD_CONFIG, "ControlPort is open, but no authentication method "
             "has been configured.  This means that any program on your "
             "computer can reconfigure your Tor.  That's bad!  You should "
             "upgrade your Tor controller as soon as possible.");
  }

  if (options->CookieAuthFileGroupReadable && !options->CookieAuthFile) {
    log_warn(LD_CONFIG, "CookieAuthFileGroupReadable is set, but will have "
             "no effect: you must specify an explicit CookieAuthFile to "
             "have it group-readable.");
  }

  if (options->MyFamily && options->BridgeRelay) {
    log_warn(LD_CONFIG, "Listing a family for a bridge relay is not "
             "supported: it can reveal bridge fingerprints to censors. "
             "You should also make sure you aren't listing this bridge's "
             "fingerprint in any other MyFamily.");
  }
  if (check_nickname_list(&options->MyFamily, "MyFamily", msg))
    return -1;
  for (cl = options->NodeFamilies; cl; cl = cl->next) {
    routerset_t *rs = routerset_new();
    if (routerset_parse(rs, cl->value, cl->key)) {
      routerset_free(rs);
      return -1;
    }
    routerset_free(rs);
  }

  if (validate_addr_policies(options, msg) < 0)
    return -1;

  if (validate_dir_servers(options, old_options) < 0)
    REJECT("Directory authority/fallback line did not parse. See logs "
           "for details.");

  if (options->UseBridges && !options->Bridges)
    REJECT("If you set UseBridges, you must specify at least one bridge.");

  for (cl = options->Bridges; cl; cl = cl->next) {
      bridge_line_t *bridge_line = parse_bridge_line(cl->value);
      if (!bridge_line)
        REJECT("Bridge line did not parse. See logs for details.");
      bridge_line_free(bridge_line);
  }

  for (cl = options->ClientTransportPlugin; cl; cl = cl->next) {
    if (parse_client_transport_line(options, cl->value, 1)<0)
      REJECT("Invalid client transport line. See logs for details.");
  }

  for (cl = options->ServerTransportPlugin; cl; cl = cl->next) {
    if (parse_server_transport_line(options, cl->value, 1)<0)
      REJECT("Invalid server transport line. See logs for details.");
  }

  if (options->ServerTransportPlugin && !server_mode(options)) {
    log_notice(LD_GENERAL, "Tor is not configured as a relay but you specified"
               " a ServerTransportPlugin line (%s). The ServerTransportPlugin "
               "line will be ignored.",
               escaped(options->ServerTransportPlugin->value));
  }

  for (cl = options->ServerTransportListenAddr; cl; cl = cl->next) {
    /** If get_bindaddr_from_transport_listen_line() fails with
        'transport' being NULL, it means that something went wrong
        while parsing the ServerTransportListenAddr line. */
    char *bindaddr = get_bindaddr_from_transport_listen_line(cl->value, NULL);
    if (!bindaddr)
      REJECT("ServerTransportListenAddr did not parse. See logs for details.");
    tor_free(bindaddr);
  }

  if (options->ServerTransportListenAddr && !options->ServerTransportPlugin) {
    log_notice(LD_GENERAL, "You need at least a single managed-proxy to "
               "specify a transport listen address. The "
               "ServerTransportListenAddr line will be ignored.");
  }

  for (cl = options->ServerTransportOptions; cl; cl = cl->next) {
    /** If get_options_from_transport_options_line() fails with
        'transport' being NULL, it means that something went wrong
        while parsing the ServerTransportOptions line. */
    smartlist_t *options_sl =
      get_options_from_transport_options_line(cl->value, NULL);
    if (!options_sl)
      REJECT("ServerTransportOptions did not parse. See logs for details.");

    SMARTLIST_FOREACH(options_sl, char *, cp, tor_free(cp));
    smartlist_free(options_sl);
  }

  if (options->ConstrainedSockets) {
    /* If the user wants to constrain socket buffer use, make sure the desired
     * limit is between MIN|MAX_TCPSOCK_BUFFER in k increments. */
    if (options->ConstrainedSockSize < MIN_CONSTRAINED_TCP_BUFFER ||
        options->ConstrainedSockSize > MAX_CONSTRAINED_TCP_BUFFER ||
        options->ConstrainedSockSize % 1024) {
      tor_asprintf(msg,
          "ConstrainedSockSize is invalid.  Must be a value between %d and %d "
          "in 1024 byte increments.",
          MIN_CONSTRAINED_TCP_BUFFER, MAX_CONSTRAINED_TCP_BUFFER);
      return -1;
    }
    if (options->DirPort_set) {
      /* Providing cached directory entries while system TCP buffers are scarce
       * will exacerbate the socket errors.  Suggest that this be disabled. */
      COMPLAIN("You have requested constrained socket buffers while also "
               "serving directory entries via DirPort.  It is strongly "
               "suggested that you disable serving directory requests when "
               "system TCP buffer resources are scarce.");
    }
  }

  if (options->V3AuthVoteDelay + options->V3AuthDistDelay >=
      options->V3AuthVotingInterval/2) {
    REJECT("V3AuthVoteDelay plus V3AuthDistDelay must be less than half "
           "V3AuthVotingInterval");
  }
  if (options->V3AuthVoteDelay < MIN_VOTE_SECONDS)
    REJECT("V3AuthVoteDelay is way too low.");
  if (options->V3AuthDistDelay < MIN_DIST_SECONDS)
    REJECT("V3AuthDistDelay is way too low.");

  if (options->V3AuthNIntervalsValid < 2)
    REJECT("V3AuthNIntervalsValid must be at least 2.");

  if (options->V3AuthVotingInterval < MIN_VOTE_INTERVAL) {
    REJECT("V3AuthVotingInterval is insanely low.");
  } else if (options->V3AuthVotingInterval > 24*60*60) {
    REJECT("V3AuthVotingInterval is insanely high.");
  } else if (((24*60*60) % options->V3AuthVotingInterval) != 0) {
    COMPLAIN("V3AuthVotingInterval does not divide evenly into 24 hours.");
  }

  if (rend_config_services(options, 1) < 0)
    REJECT("Failed to configure rendezvous options. See logs for details.");

  /* Parse client-side authorization for hidden services. */
  if (rend_parse_service_authorization(options, 1) < 0)
    REJECT("Failed to configure client authorization for hidden services. "
           "See logs for details.");

  if (parse_virtual_addr_network(options->VirtualAddrNetworkIPv4,
                                 AF_INET, 1, msg)<0)
    return -1;
  if (parse_virtual_addr_network(options->VirtualAddrNetworkIPv6,
                                 AF_INET6, 1, msg)<0)
    return -1;

  if (options->AutomapHostsSuffixes) {
    SMARTLIST_FOREACH(options->AutomapHostsSuffixes, char *, suf,
    {
      size_t len = strlen(suf);
      if (len && suf[len-1] == '.')
        suf[len-1] = '\0';
    });
  }

  if (options->TestingTorNetwork &&
      !(options->DirAuthorities ||
        (options->AlternateDirAuthority &&
         options->AlternateBridgeAuthority))) {
    REJECT("TestingTorNetwork may only be configured in combination with "
           "a non-default set of DirAuthority or both of "
           "AlternateDirAuthority and AlternateBridgeAuthority configured.");
  }

  if (options->AllowSingleHopExits && !options->DirAuthorities) {
    COMPLAIN("You have set AllowSingleHopExits; now your relay will allow "
             "others to make one-hop exits. However, since by default most "
             "clients avoid relays that set this option, most clients will "
             "ignore you.");
  }

#define CHECK_DEFAULT(arg)                                              \
  STMT_BEGIN                                                            \
    if (!options->TestingTorNetwork &&                                  \
        !options->UsingTestNetworkDefaults_ &&                          \
        !config_is_same(&options_format,options,                        \
                        default_options,#arg)) {                        \
      REJECT(#arg " may only be changed in testing Tor "                \
             "networks!");                                              \
    } STMT_END
  CHECK_DEFAULT(TestingV3AuthInitialVotingInterval);
  CHECK_DEFAULT(TestingV3AuthInitialVoteDelay);
  CHECK_DEFAULT(TestingV3AuthInitialDistDelay);
  CHECK_DEFAULT(TestingV3AuthVotingStartOffset);
  CHECK_DEFAULT(TestingAuthDirTimeToLearnReachability);
  CHECK_DEFAULT(TestingEstimatedDescriptorPropagationTime);
  CHECK_DEFAULT(TestingServerDownloadSchedule);
  CHECK_DEFAULT(TestingClientDownloadSchedule);
  CHECK_DEFAULT(TestingServerConsensusDownloadSchedule);
  CHECK_DEFAULT(TestingClientConsensusDownloadSchedule);
  CHECK_DEFAULT(TestingBridgeDownloadSchedule);
  CHECK_DEFAULT(TestingClientMaxIntervalWithoutRequest);
  CHECK_DEFAULT(TestingDirConnectionMaxStall);
  CHECK_DEFAULT(TestingConsensusMaxDownloadTries);
  CHECK_DEFAULT(TestingDescriptorMaxDownloadTries);
  CHECK_DEFAULT(TestingMicrodescMaxDownloadTries);
  CHECK_DEFAULT(TestingCertMaxDownloadTries);
#undef CHECK_DEFAULT

  if (options->TestingV3AuthInitialVotingInterval < MIN_VOTE_INTERVAL) {
    REJECT("TestingV3AuthInitialVotingInterval is insanely low.");
  } else if (((30*60) % options->TestingV3AuthInitialVotingInterval) != 0) {
    REJECT("TestingV3AuthInitialVotingInterval does not divide evenly into "
           "30 minutes.");
  }

  if (options->TestingV3AuthInitialVoteDelay < MIN_VOTE_SECONDS) {
    REJECT("TestingV3AuthInitialVoteDelay is way too low.");
  }

  if (options->TestingV3AuthInitialDistDelay < MIN_DIST_SECONDS) {
    REJECT("TestingV3AuthInitialDistDelay is way too low.");
  }

  if (options->TestingV3AuthInitialVoteDelay +
      options->TestingV3AuthInitialDistDelay >=
      options->TestingV3AuthInitialVotingInterval/2) {
    REJECT("TestingV3AuthInitialVoteDelay plus TestingV3AuthInitialDistDelay "
           "must be less than half TestingV3AuthInitialVotingInterval");
  }

  if (options->TestingV3AuthVotingStartOffset >
      MIN(options->TestingV3AuthInitialVotingInterval,
          options->V3AuthVotingInterval)) {
    REJECT("TestingV3AuthVotingStartOffset is higher than the voting "
           "interval.");
  }

  if (options->TestingAuthDirTimeToLearnReachability < 0) {
    REJECT("TestingAuthDirTimeToLearnReachability must be non-negative.");
  } else if (options->TestingAuthDirTimeToLearnReachability > 2*60*60) {
    COMPLAIN("TestingAuthDirTimeToLearnReachability is insanely high.");
  }

  if (options->TestingEstimatedDescriptorPropagationTime < 0) {
    REJECT("TestingEstimatedDescriptorPropagationTime must be non-negative.");
  } else if (options->TestingEstimatedDescriptorPropagationTime > 60*60) {
    COMPLAIN("TestingEstimatedDescriptorPropagationTime is insanely high.");
  }

  if (options->TestingClientMaxIntervalWithoutRequest < 1) {
    REJECT("TestingClientMaxIntervalWithoutRequest is way too low.");
  } else if (options->TestingClientMaxIntervalWithoutRequest > 3600) {
    COMPLAIN("TestingClientMaxIntervalWithoutRequest is insanely high.");
  }

  if (options->TestingDirConnectionMaxStall < 5) {
    REJECT("TestingDirConnectionMaxStall is way too low.");
  } else if (options->TestingDirConnectionMaxStall > 3600) {
    COMPLAIN("TestingDirConnectionMaxStall is insanely high.");
  }

  if (options->TestingConsensusMaxDownloadTries < 2) {
    REJECT("TestingConsensusMaxDownloadTries must be greater than 1.");
  } else if (options->TestingConsensusMaxDownloadTries > 800) {
    COMPLAIN("TestingConsensusMaxDownloadTries is insanely high.");
  }

  if (options->TestingDescriptorMaxDownloadTries < 2) {
    REJECT("TestingDescriptorMaxDownloadTries must be greater than 1.");
  } else if (options->TestingDescriptorMaxDownloadTries > 800) {
    COMPLAIN("TestingDescriptorMaxDownloadTries is insanely high.");
  }

  if (options->TestingMicrodescMaxDownloadTries < 2) {
    REJECT("TestingMicrodescMaxDownloadTries must be greater than 1.");
  } else if (options->TestingMicrodescMaxDownloadTries > 800) {
    COMPLAIN("TestingMicrodescMaxDownloadTries is insanely high.");
  }

  if (options->TestingCertMaxDownloadTries < 2) {
    REJECT("TestingCertMaxDownloadTries must be greater than 1.");
  } else if (options->TestingCertMaxDownloadTries > 800) {
    COMPLAIN("TestingCertMaxDownloadTries is insanely high.");
  }

  if (options->TestingEnableConnBwEvent &&
      !options->TestingTorNetwork && !options->UsingTestNetworkDefaults_) {
    REJECT("TestingEnableConnBwEvent may only be changed in testing "
           "Tor networks!");
  }

  if (options->TestingEnableCellStatsEvent &&
      !options->TestingTorNetwork && !options->UsingTestNetworkDefaults_) {
    REJECT("TestingEnableCellStatsEvent may only be changed in testing "
           "Tor networks!");
  }

  if (options->TestingEnableTbEmptyEvent &&
      !options->TestingTorNetwork && !options->UsingTestNetworkDefaults_) {
    REJECT("TestingEnableTbEmptyEvent may only be changed in testing "
           "Tor networks!");
  }

  if (options->TestingTorNetwork) {
    log_warn(LD_CONFIG, "TestingTorNetwork is set. This will make your node "
                        "almost unusable in the public Tor network, and is "
                        "therefore only advised if you are building a "
                        "testing Tor network!");
  }

  if (options->AccelName && !options->HardwareAccel)
    options->HardwareAccel = 1;
  if (options->AccelDir && !options->AccelName)
    REJECT("Can't use hardware crypto accelerator dir without engine name.");

  if (options->PublishServerDescriptor)
    SMARTLIST_FOREACH(options->PublishServerDescriptor, const char *, pubdes, {
      if (!strcmp(pubdes, "1") || !strcmp(pubdes, "0"))
        if (smartlist_len(options->PublishServerDescriptor) > 1) {
          COMPLAIN("You have passed a list of multiple arguments to the "
                   "PublishServerDescriptor option that includes 0 or 1. "
                   "0 or 1 should only be used as the sole argument. "
                   "This configuration will be rejected in a future release.");
          break;
        }
    });

  if (options->BridgeRelay == 1 && ! options->ORPort_set)
      REJECT("BridgeRelay is 1, ORPort is not set. This is an invalid "
             "combination.");

  return 0;
#undef REJECT
#undef COMPLAIN
}

/* Given the value that the user has set for MaxMemInQueues, compute the
 * actual maximum value.  We clip this value if it's too low, and autodetect
 * it if it's set to 0. */
static uint64_t
compute_real_max_mem_in_queues(const uint64_t val, int log_guess)
{
  uint64_t result;

  if (val == 0) {
#define ONE_GIGABYTE (U64_LITERAL(1) << 30)
#define ONE_MEGABYTE (U64_LITERAL(1) << 20)
#if SIZEOF_VOID_P >= 8
#define MAX_DEFAULT_MAXMEM (8*ONE_GIGABYTE)
#else
#define MAX_DEFAULT_MAXMEM (2*ONE_GIGABYTE)
#endif
    /* The user didn't pick a memory limit.  Choose a very large one
     * that is still smaller than the system memory */
    static int notice_sent = 0;
    size_t ram = 0;
    if (get_total_system_memory(&ram) < 0) {
      /* We couldn't determine our total system memory!  */
#if SIZEOF_VOID_P >= 8
      /* 64-bit system.  Let's hope for 8 GB. */
      result = 8 * ONE_GIGABYTE;
#else
      /* (presumably) 32-bit system. Let's hope for 1 GB. */
      result = ONE_GIGABYTE;
#endif
    } else {
      /* We detected it, so let's pick 3/4 of the total RAM as our limit. */
      const uint64_t avail = (ram / 4) * 3;

      /* Make sure it's in range from 0.25 GB to 8 GB. */
      if (avail > MAX_DEFAULT_MAXMEM) {
        /* If you want to use more than this much RAM, you need to configure
           it yourself */
        result = MAX_DEFAULT_MAXMEM;
      } else if (avail < ONE_GIGABYTE / 4) {
        result = ONE_GIGABYTE / 4;
      } else {
        result = avail;
      }
    }
    if (log_guess && ! notice_sent) {
      log_notice(LD_CONFIG, "%sMaxMemInQueues is set to "U64_FORMAT" MB. "
                 "You can override this by setting MaxMemInQueues by hand.",
                 ram ? "Based on detected system memory, " : "",
                 U64_PRINTF_ARG(result / ONE_MEGABYTE));
      notice_sent = 1;
    }
    return result;
  } else if (val < ONE_GIGABYTE / 4) {
    log_warn(LD_CONFIG, "MaxMemInQueues must be at least 256 MB for now. "
             "Ideally, have it as large as you can afford.");
    return ONE_GIGABYTE / 4;
  } else {
    /* The value was fine all along */
    return val;
  }
}

/** Helper: return true iff s1 and s2 are both NULL, or both non-NULL
 * equal strings. */
static int
opt_streq(const char *s1, const char *s2)
{
  return 0 == strcmp_opt(s1, s2);
}

/** Check if any of the previous options have changed but aren't allowed to. */
static int
options_transition_allowed(const or_options_t *old,
                           const or_options_t *new_val,
                           char **msg)
{
  if (!old)
    return 0;

  if (!opt_streq(old->PidFile, new_val->PidFile)) {
    *msg = tor_strdup("PidFile is not allowed to change.");
    return -1;
  }

  if (old->RunAsDaemon != new_val->RunAsDaemon) {
    *msg = tor_strdup("While Tor is running, changing RunAsDaemon "
                      "is not allowed.");
    return -1;
  }

  if (old->Sandbox != new_val->Sandbox) {
    *msg = tor_strdup("While Tor is running, changing Sandbox "
                      "is not allowed.");
    return -1;
  }

  if (strcmp(old->DataDirectory,new_val->DataDirectory)!=0) {
    tor_asprintf(msg,
               "While Tor is running, changing DataDirectory "
               "(\"%s\"->\"%s\") is not allowed.",
               old->DataDirectory, new_val->DataDirectory);
    return -1;
  }

  if (!opt_streq(old->User, new_val->User)) {
    *msg = tor_strdup("While Tor is running, changing User is not allowed.");
    return -1;
  }

  if ((old->HardwareAccel != new_val->HardwareAccel)
      || !opt_streq(old->AccelName, new_val->AccelName)
      || !opt_streq(old->AccelDir, new_val->AccelDir)) {
    *msg = tor_strdup("While Tor is running, changing OpenSSL hardware "
                      "acceleration engine is not allowed.");
    return -1;
  }

  if (old->TestingTorNetwork != new_val->TestingTorNetwork) {
    *msg = tor_strdup("While Tor is running, changing TestingTorNetwork "
                      "is not allowed.");
    return -1;
  }

  if (old->DisableAllSwap != new_val->DisableAllSwap) {
    *msg = tor_strdup("While Tor is running, changing DisableAllSwap "
                      "is not allowed.");
    return -1;
  }

  if (old->TokenBucketRefillInterval != new_val->TokenBucketRefillInterval) {
    *msg = tor_strdup("While Tor is running, changing TokenBucketRefill"
                      "Interval is not allowed");
    return -1;
  }

  if (old->DisableIOCP != new_val->DisableIOCP) {
    *msg = tor_strdup("While Tor is running, changing DisableIOCP "
                      "is not allowed.");
    return -1;
  }

  if (old->DisableDebuggerAttachment &&
      !new_val->DisableDebuggerAttachment) {
    *msg = tor_strdup("While Tor is running, disabling "
                      "DisableDebuggerAttachment is not allowed.");
    return -1;
  }

  if (sandbox_is_active()) {
#define SB_NOCHANGE_STR(opt)                                            \
    do {                                                                \
      if (! opt_streq(old->opt, new_val->opt)) {                        \
        *msg = tor_strdup("Can't change " #opt " while Sandbox is active"); \
        return -1;                                                      \
      }                                                                 \
    } while (0)

    SB_NOCHANGE_STR(PidFile);
    SB_NOCHANGE_STR(ServerDNSResolvConfFile);
    SB_NOCHANGE_STR(DirPortFrontPage);
    SB_NOCHANGE_STR(CookieAuthFile);
    SB_NOCHANGE_STR(ExtORPortCookieAuthFile);

#undef SB_NOCHANGE_STR

    if (! config_lines_eq(old->Logs, new_val->Logs)) {
      *msg = tor_strdup("Can't change Logs while Sandbox is active");
      return -1;
    }
    if (old->ConnLimit != new_val->ConnLimit) {
      *msg = tor_strdup("Can't change ConnLimit while Sandbox is active");
      return -1;
    }
    if (server_mode(old) != server_mode(new_val)) {
      *msg = tor_strdup("Can't start/stop being a server while "
                        "Sandbox is active");
      return -1;
    }
  }

  return 0;
}

/** Return 1 if any change from <b>old_options</b> to <b>new_options</b>
 * will require us to rotate the CPU and DNS workers; else return 0. */
static int
options_transition_affects_workers(const or_options_t *old_options,
                                   const or_options_t *new_options)
{
  if (!opt_streq(old_options->DataDirectory, new_options->DataDirectory) ||
      old_options->NumCPUs != new_options->NumCPUs ||
      !config_lines_eq(old_options->ORPort_lines, new_options->ORPort_lines) ||
      old_options->ServerDNSSearchDomains !=
                                       new_options->ServerDNSSearchDomains ||
      old_options->SafeLogging_ != new_options->SafeLogging_ ||
      old_options->ClientOnly != new_options->ClientOnly ||
      public_server_mode(old_options) != public_server_mode(new_options) ||
      !config_lines_eq(old_options->Logs, new_options->Logs) ||
      old_options->LogMessageDomains != new_options->LogMessageDomains)
    return 1;

  /* Check whether log options match. */

  /* Nothing that changed matters. */
  return 0;
}

/** Return 1 if any change from <b>old_options</b> to <b>new_options</b>
 * will require us to generate a new descriptor; else return 0. */
static int
options_transition_affects_descriptor(const or_options_t *old_options,
                                      const or_options_t *new_options)
{
  /* XXX We can be smarter here. If your DirPort isn't being
   * published and you just turned it off, no need to republish. Etc. */
  if (!opt_streq(old_options->DataDirectory, new_options->DataDirectory) ||
      !opt_streq(old_options->Nickname,new_options->Nickname) ||
      !opt_streq(old_options->Address,new_options->Address) ||
      !config_lines_eq(old_options->ExitPolicy,new_options->ExitPolicy) ||
      old_options->ExitPolicyRejectPrivate !=
        new_options->ExitPolicyRejectPrivate ||
      old_options->IPv6Exit != new_options->IPv6Exit ||
      !config_lines_eq(old_options->ORPort_lines,
                       new_options->ORPort_lines) ||
      !config_lines_eq(old_options->DirPort_lines,
                       new_options->DirPort_lines) ||
      old_options->ClientOnly != new_options->ClientOnly ||
      old_options->DisableNetwork != new_options->DisableNetwork ||
      old_options->PublishServerDescriptor_ !=
        new_options->PublishServerDescriptor_ ||
      get_effective_bwrate(old_options) != get_effective_bwrate(new_options) ||
      get_effective_bwburst(old_options) !=
        get_effective_bwburst(new_options) ||
      !opt_streq(old_options->ContactInfo, new_options->ContactInfo) ||
      !opt_streq(old_options->MyFamily, new_options->MyFamily) ||
      !opt_streq(old_options->AccountingStart, new_options->AccountingStart) ||
      old_options->AccountingMax != new_options->AccountingMax ||
      public_server_mode(old_options) != public_server_mode(new_options))
    return 1;

  return 0;
}

#ifdef _WIN32
/** Return the directory on windows where we expect to find our application
 * data. */
static char *
get_windows_conf_root(void)
{
  static int is_set = 0;
  static char path[MAX_PATH*2+1];
  TCHAR tpath[MAX_PATH] = {0};

  LPITEMIDLIST idl;
  IMalloc *m;
  HRESULT result;

  if (is_set)
    return path;

  /* Find X:\documents and settings\username\application data\ .
   * We would use SHGetSpecialFolder path, but that wasn't added until IE4.
   */
#ifdef ENABLE_LOCAL_APPDATA
#define APPDATA_PATH CSIDL_LOCAL_APPDATA
#else
#define APPDATA_PATH CSIDL_APPDATA
#endif
  if (!SUCCEEDED(SHGetSpecialFolderLocation(NULL, APPDATA_PATH, &idl))) {
    getcwd(path,MAX_PATH);
    is_set = 1;
    log_warn(LD_CONFIG,
             "I couldn't find your application data folder: are you "
             "running an ancient version of Windows 95? Defaulting to \"%s\"",
             path);
    return path;
  }
  /* Convert the path from an "ID List" (whatever that is!) to a path. */
  result = SHGetPathFromIDList(idl, tpath);
#ifdef UNICODE
  wcstombs(path,tpath,sizeof(path));
  path[sizeof(path)-1] = '\0';
#else
  strlcpy(path,tpath,sizeof(path));
#endif

  /* Now we need to free the memory that the path-idl was stored in.  In
   * typical Windows fashion, we can't just call 'free()' on it. */
  SHGetMalloc(&m);
  if (m) {
    m->lpVtbl->Free(m, idl);
    m->lpVtbl->Release(m);
  }
  if (!SUCCEEDED(result)) {
    return NULL;
  }
  strlcat(path,"\\tor",MAX_PATH);
  is_set = 1;
  return path;
}
#endif

/** Return the default location for our torrc file (if <b>defaults_file</b> is
 * false), or for the torrc-defaults file (if <b>defaults_file</b> is true). */
static const char *
get_default_conf_file(int defaults_file)
{
#ifdef _WIN32
  if (defaults_file) {
    static char defaults_path[MAX_PATH+1];
    tor_snprintf(defaults_path, MAX_PATH, "%s\\torrc-defaults",
                 get_windows_conf_root());
    return defaults_path;
  } else {
    static char path[MAX_PATH+1];
    tor_snprintf(path, MAX_PATH, "%s\\torrc",
                 get_windows_conf_root());
    return path;
  }
#else
  return defaults_file ? CONFDIR "/torrc-defaults" : CONFDIR "/torrc";
#endif
}

/** Verify whether lst is a string containing valid-looking comma-separated
 * nicknames, or NULL. Will normalise <b>lst</b> to prefix '$' to any nickname
 * or fingerprint that needs it. Return 0 on success.
 * Warn and return -1 on failure.
 */
static int
check_nickname_list(char **lst, const char *name, char **msg)
{
  int r = 0;
  smartlist_t *sl;
  int changes = 0;

  if (!*lst)
    return 0;
  sl = smartlist_new();

  smartlist_split_string(sl, *lst, ",",
    SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK|SPLIT_STRIP_SPACE, 0);

  SMARTLIST_FOREACH_BEGIN(sl, char *, s)
    {
      if (!is_legal_nickname_or_hexdigest(s)) {
        // check if first char is dollar
        if (s[0] != '$') {
          // Try again but with a dollar symbol prepended
          char *prepended;
          tor_asprintf(&prepended, "$%s", s);

          if (is_legal_nickname_or_hexdigest(prepended)) {
            // The nickname is valid when it's prepended, swap the current
            // version with a prepended one
            tor_free(s);
            SMARTLIST_REPLACE_CURRENT(sl, s, prepended);
            changes = 1;
            continue;
          }

          // Still not valid, free and fallback to error message
          tor_free(prepended);
        }

        tor_asprintf(msg, "Invalid nickname '%s' in %s line", s, name);
        r = -1;
        break;
      }
    }
  SMARTLIST_FOREACH_END(s);

  // Replace the caller's nickname list with a fixed one
  if (changes && r == 0) {
    char *newNicknames = smartlist_join_strings(sl, ", ", 0, NULL);
    tor_free(*lst);
    *lst = newNicknames;
  }

  SMARTLIST_FOREACH(sl, char *, s, tor_free(s));
  smartlist_free(sl);

  return r;
}

/** Learn config file name from command line arguments, or use the default.
 *
 * If <b>defaults_file</b> is true, we're looking for torrc-defaults;
 * otherwise, we're looking for the regular torrc_file.
 *
 * Set *<b>using_default_fname</b> to true if we're using the default
 * configuration file name; or false if we've set it from the command line.
 *
 * Set *<b>ignore_missing_torrc</b> to true if we should ignore the resulting
 * filename if it doesn't exist.
 */
static char *
find_torrc_filename(config_line_t *cmd_arg,
                    int defaults_file,
                    int *using_default_fname, int *ignore_missing_torrc)
{
  char *fname=NULL;
  config_line_t *p_index;
  const char *fname_opt = defaults_file ? "--defaults-torrc" : "-f";
  const char *ignore_opt = defaults_file ? NULL : "--ignore-missing-torrc";

  if (defaults_file)
    *ignore_missing_torrc = 1;

  for (p_index = cmd_arg; p_index; p_index = p_index->next) {
    if (!strcmp(p_index->key, fname_opt)) {
      if (fname) {
        log_warn(LD_CONFIG, "Duplicate %s options on command line.",
            fname_opt);
        tor_free(fname);
      }
      fname = expand_filename(p_index->value);

      {
        char *absfname;
        absfname = make_path_absolute(fname);
        tor_free(fname);
        fname = absfname;
      }

      *using_default_fname = 0;
    } else if (ignore_opt && !strcmp(p_index->key,ignore_opt)) {
      *ignore_missing_torrc = 1;
    }
  }

  if (*using_default_fname) {
    /* didn't find one, try CONFDIR */
    const char *dflt = get_default_conf_file(defaults_file);
    if (dflt && file_status(dflt) == FN_FILE) {
      fname = tor_strdup(dflt);
    } else {
#ifndef _WIN32
      char *fn = NULL;
      if (!defaults_file)
        fn = expand_filename("~/.torrc");
      if (fn && file_status(fn) == FN_FILE) {
        fname = fn;
      } else {
        tor_free(fn);
        fname = tor_strdup(dflt);
      }
#else
      fname = tor_strdup(dflt);
#endif
    }
  }
  return fname;
}

/** Load a configuration file from disk, setting torrc_fname or
 * torrc_defaults_fname if successful.
 *
 * If <b>defaults_file</b> is true, load torrc-defaults; otherwise load torrc.
 *
 * Return the contents of the file on success, and NULL on failure.
 */
static char *
load_torrc_from_disk(config_line_t *cmd_arg, int defaults_file)
{
  char *fname=NULL;
  char *cf = NULL;
  int using_default_torrc = 1;
  int ignore_missing_torrc = 0;
  char **fname_var = defaults_file ? &torrc_defaults_fname : &torrc_fname;

  fname = find_torrc_filename(cmd_arg, defaults_file,
                              &using_default_torrc, &ignore_missing_torrc);
  tor_assert(fname);
  log_debug(LD_CONFIG, "Opening config file \"%s\"", fname);

  tor_free(*fname_var);
  *fname_var = fname;

  /* Open config file */
  if (file_status(fname) != FN_FILE ||
      !(cf = read_file_to_str(fname,0,NULL))) {
    if (using_default_torrc == 1 || ignore_missing_torrc) {
      if (!defaults_file)
        log_notice(LD_CONFIG, "Configuration file \"%s\" not present, "
            "using reasonable defaults.", fname);
      tor_free(fname); /* sets fname to NULL */
      *fname_var = NULL;
      cf = tor_strdup("");
    } else {
      log_warn(LD_CONFIG,
          "Unable to open configuration file \"%s\".", fname);
      goto err;
    }
  } else {
    log_notice(LD_CONFIG, "Read configuration file \"%s\".", fname);
  }

  return cf;
 err:
  tor_free(fname);
  *fname_var = NULL;
  return NULL;
}

/** Read a configuration file into <b>options</b>, finding the configuration
 * file location based on the command line.  After loading the file
 * call options_init_from_string() to load the config.
 * Return 0 if success, -1 if failure. */
int
options_init_from_torrc(int argc, char **argv)
{
  char *cf=NULL, *cf_defaults=NULL;
  int command;
  int retval = -1;
  char *command_arg = NULL;
  char *errmsg=NULL;
  config_line_t *p_index = NULL;
  config_line_t *cmdline_only_options = NULL;

  /* Go through command-line variables */
  if (! have_parsed_cmdline) {
    /* Or we could redo the list every time we pass this place.
     * It does not really matter */
    if (config_parse_commandline(argc, argv, 0, &global_cmdline_options,
                                 &global_cmdline_only_options) < 0) {
      goto err;
    }
    have_parsed_cmdline = 1;
  }
  cmdline_only_options = global_cmdline_only_options;

  if (config_line_find(cmdline_only_options, "-h") ||
      config_line_find(cmdline_only_options, "--help")) {
    print_usage();
    exit(0);
  }
  if (config_line_find(cmdline_only_options, "--list-torrc-options")) {
    /* For documenting validating whether we've documented everything. */
    list_torrc_options();
    exit(0);
  }

  if (config_line_find(cmdline_only_options, "--version")) {
    printf("Tor version %s.\n",get_version());
    exit(0);
  }

  if (config_line_find(cmdline_only_options, "--digests")) {
    printf("Tor version %s.\n",get_version());
    printf("%s", libor_get_digests());
    printf("%s", tor_get_digests());
    exit(0);
  }

  if (config_line_find(cmdline_only_options, "--library-versions")) {
    printf("Tor version %s. \n", get_version());
    printf("Library versions\tCompiled\t\tRuntime\n");
    printf("Libevent\t\t%-15s\t\t%s\n",
                      tor_libevent_get_header_version_str(),
                      tor_libevent_get_version_str());
    printf("OpenSSL \t\t%-15s\t\t%s\n",
                      crypto_openssl_get_header_version_str(),
                      crypto_openssl_get_version_str());
    printf("Zlib    \t\t%-15s\t\t%s\n",
                      tor_zlib_get_header_version_str(),
                      tor_zlib_get_version_str());
    //TODO: Hex versions?
    exit(0);
  }

  command = CMD_RUN_TOR;
  for (p_index = cmdline_only_options; p_index; p_index = p_index->next) {
    if (!strcmp(p_index->key,"--list-fingerprint")) {
      command = CMD_LIST_FINGERPRINT;
    } else if (!strcmp(p_index->key, "--hash-password")) {
      command = CMD_HASH_PASSWORD;
      command_arg = p_index->value;
    } else if (!strcmp(p_index->key, "--dump-config")) {
      command = CMD_DUMP_CONFIG;
      command_arg = p_index->value;
    } else if (!strcmp(p_index->key, "--verify-config")) {
      command = CMD_VERIFY_CONFIG;
    }
  }

  if (command == CMD_HASH_PASSWORD) {
    cf_defaults = tor_strdup("");
    cf = tor_strdup("");
  } else {
    cf_defaults = load_torrc_from_disk(cmdline_only_options, 1);
    cf = load_torrc_from_disk(cmdline_only_options, 0);
    if (!cf) {
      if (config_line_find(cmdline_only_options, "--allow-missing-torrc")) {
        cf = tor_strdup("");
      } else {
        goto err;
      }
    }
  }

  retval = options_init_from_string(cf_defaults, cf, command, command_arg,
                                    &errmsg);

 err:

  tor_free(cf);
  tor_free(cf_defaults);
  if (errmsg) {
    log_warn(LD_CONFIG,"%s", errmsg);
    tor_free(errmsg);
  }
  return retval < 0 ? -1 : 0;
}

/** Load the options from the configuration in <b>cf</b>, validate
 * them for consistency and take actions based on them.
 *
 * Return 0 if success, negative on error:
 *  * -1 for general errors.
 *  * -2 for failure to parse/validate,
 *  * -3 for transition not allowed
 *  * -4 for error while setting the new options
 */
setopt_err_t
options_init_from_string(const char *cf_defaults, const char *cf,
                         int command, const char *command_arg,
                         char **msg)
{
  or_options_t *oldoptions, *newoptions, *newdefaultoptions=NULL;
  config_line_t *cl;
  int retval, i;
  setopt_err_t err = SETOPT_ERR_MISC;
  tor_assert(msg);

  oldoptions = global_options; /* get_options unfortunately asserts if
                                  this is the first time we run*/

  newoptions = tor_malloc_zero(sizeof(or_options_t));
  newoptions->magic_ = OR_OPTIONS_MAGIC;
  options_init(newoptions);
  newoptions->command = command;
  newoptions->command_arg = command_arg ? tor_strdup(command_arg) : NULL;

  for (i = 0; i < 2; ++i) {
    const char *body = i==0 ? cf_defaults : cf;
    if (!body)
      continue;
    /* get config lines, assign them */
    retval = config_get_lines(body, &cl, 1);
    if (retval < 0) {
      err = SETOPT_ERR_PARSE;
      goto err;
    }
    retval = config_assign(&options_format, newoptions, cl, 0, 0, msg);
    config_free_lines(cl);
    if (retval < 0) {
      err = SETOPT_ERR_PARSE;
      goto err;
    }
    if (i==0)
      newdefaultoptions = config_dup(&options_format, newoptions);
  }

  if (newdefaultoptions == NULL) {
    newdefaultoptions = config_dup(&options_format, global_default_options);
  }

  /* Go through command-line variables too */
  retval = config_assign(&options_format, newoptions,
                         global_cmdline_options, 0, 0, msg);
  if (retval < 0) {
    err = SETOPT_ERR_PARSE;
    goto err;
  }

  /* If this is a testing network configuration, change defaults
   * for a list of dependent config options, re-initialize newoptions
   * with the new defaults, and assign all options to it second time. */
  if (newoptions->TestingTorNetwork) {
    /* XXXX this is a bit of a kludge.  perhaps there's a better way to do
     * this?  We could, for example, make the parsing algorithm do two passes
     * over the configuration.  If it finds any "suite" options like
     * TestingTorNetwork, it could change the defaults before its second pass.
     * Not urgent so long as this seems to work, but at any sign of trouble,
     * let's clean it up.  -NM */

    /* Change defaults. */
    int i;
    for (i = 0; testing_tor_network_defaults[i].name; ++i) {
      const config_var_t *new_var = &testing_tor_network_defaults[i];
      config_var_t *old_var =
          config_find_option_mutable(&options_format, new_var->name);
      tor_assert(new_var);
      tor_assert(old_var);
      old_var->initvalue = new_var->initvalue;
    }

    /* Clear newoptions and re-initialize them with new defaults. */
    config_free(&options_format, newoptions);
    config_free(&options_format, newdefaultoptions);
    newdefaultoptions = NULL;
    newoptions = tor_malloc_zero(sizeof(or_options_t));
    newoptions->magic_ = OR_OPTIONS_MAGIC;
    options_init(newoptions);
    newoptions->command = command;
    newoptions->command_arg = command_arg ? tor_strdup(command_arg) : NULL;

    /* Assign all options a second time. */
    for (i = 0; i < 2; ++i) {
      const char *body = i==0 ? cf_defaults : cf;
      if (!body)
        continue;
      /* get config lines, assign them */
      retval = config_get_lines(body, &cl, 1);
      if (retval < 0) {
        err = SETOPT_ERR_PARSE;
        goto err;
      }
      retval = config_assign(&options_format, newoptions, cl, 0, 0, msg);
      config_free_lines(cl);
      if (retval < 0) {
        err = SETOPT_ERR_PARSE;
        goto err;
      }
      if (i==0)
        newdefaultoptions = config_dup(&options_format, newoptions);
    }
    /* Assign command-line variables a second time too */
    retval = config_assign(&options_format, newoptions,
                           global_cmdline_options, 0, 0, msg);
    if (retval < 0) {
      err = SETOPT_ERR_PARSE;
      goto err;
    }
  }

  /* Validate newoptions */
  if (options_validate(oldoptions, newoptions, newdefaultoptions,
                       0, msg) < 0) {
    err = SETOPT_ERR_PARSE; /*XXX make this a separate return value.*/
    goto err;
  }

  if (options_transition_allowed(oldoptions, newoptions, msg) < 0) {
    err = SETOPT_ERR_TRANSITION;
    goto err;
  }

  if (set_options(newoptions, msg)) {
    err = SETOPT_ERR_SETTING;
    goto err; /* frees and replaces old options */
  }
  config_free(&options_format, global_default_options);
  global_default_options = newdefaultoptions;

  return SETOPT_OK;

 err:
  config_free(&options_format, newoptions);
  config_free(&options_format, newdefaultoptions);
  if (*msg) {
    char *old_msg = *msg;
    tor_asprintf(msg, "Failed to parse/validate config: %s", old_msg);
    tor_free(old_msg);
  }
  return err;
}

/** Return the location for our configuration file.
 */
const char *
get_torrc_fname(int defaults_fname)
{
  const char *fname = defaults_fname ? torrc_defaults_fname : torrc_fname;

  if (fname)
    return fname;
  else
    return get_default_conf_file(defaults_fname);
}

/** Adjust the address map based on the MapAddress elements in the
 * configuration <b>options</b>
 */
void
config_register_addressmaps(const or_options_t *options)
{
  smartlist_t *elts;
  config_line_t *opt;
  const char *from, *to, *msg;

  addressmap_clear_configured();
  elts = smartlist_new();
  for (opt = options->AddressMap; opt; opt = opt->next) {
    smartlist_split_string(elts, opt->value, NULL,
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 2);
    if (smartlist_len(elts) < 2) {
      log_warn(LD_CONFIG,"MapAddress '%s' has too few arguments. Ignoring.",
               opt->value);
      goto cleanup;
    }

    from = smartlist_get(elts,0);
    to = smartlist_get(elts,1);

    if (to[0] == '.' || from[0] == '.') {
      log_warn(LD_CONFIG,"MapAddress '%s' is ambiguous - address starts with a"
              "'.'. Ignoring.",opt->value);
      goto cleanup;
    }

    if (addressmap_register_auto(from, to, 0, ADDRMAPSRC_TORRC, &msg) < 0) {
      log_warn(LD_CONFIG,"MapAddress '%s' failed: %s. Ignoring.", opt->value,
               msg);
      goto cleanup;
    }

    if (smartlist_len(elts) > 2)
      log_warn(LD_CONFIG,"Ignoring extra arguments to MapAddress.");

  cleanup:
    SMARTLIST_FOREACH(elts, char*, cp, tor_free(cp));
    smartlist_clear(elts);
  }
  smartlist_free(elts);
}

/** As addressmap_register(), but detect the wildcarded status of "from" and
 * "to", and do not steal a reference to <b>to</b>. */
/* XXXX024 move to connection_edge.c */
int
addressmap_register_auto(const char *from, const char *to,
                         time_t expires,
                         addressmap_entry_source_t addrmap_source,
                         const char **msg)
{
  int from_wildcard = 0, to_wildcard = 0;

  *msg = "whoops, forgot the error message";
  if (1) {
    if (!strcmp(to, "*") || !strcmp(from, "*")) {
      *msg = "can't remap from or to *";
      return -1;
    }
    /* Detect asterisks in expressions of type: '*.example.com' */
    if (!strncmp(from,"*.",2)) {
      from += 2;
      from_wildcard = 1;
    }
    if (!strncmp(to,"*.",2)) {
      to += 2;
      to_wildcard = 1;
    }

    if (to_wildcard && !from_wildcard) {
      *msg =  "can only use wildcard (i.e. '*.') if 'from' address "
        "uses wildcard also";
      return -1;
    }

    if (address_is_invalid_destination(to, 1)) {
      *msg = "destination is invalid";
      return -1;
    }

    addressmap_register(from, tor_strdup(to), expires, addrmap_source,
                        from_wildcard, to_wildcard);
  }
  return 0;
}

/**
 * Initialize the logs based on the configuration file.
 */
static int
options_init_logs(or_options_t *options, int validate_only)
{
  config_line_t *opt;
  int ok;
  smartlist_t *elts;
  int daemon =
#ifdef _WIN32
               0;
#else
               options->RunAsDaemon;
#endif

  if (options->LogTimeGranularity <= 0) {
    log_warn(LD_CONFIG, "Log time granularity '%d' has to be positive.",
             options->LogTimeGranularity);
    return -1;
  } else if (1000 % options->LogTimeGranularity != 0 &&
             options->LogTimeGranularity % 1000 != 0) {
    int granularity = options->LogTimeGranularity;
    if (granularity < 40) {
      do granularity++;
      while (1000 % granularity != 0);
    } else if (granularity < 1000) {
      granularity = 1000 / granularity;
      while (1000 % granularity != 0)
        granularity--;
      granularity = 1000 / granularity;
    } else {
      granularity = 1000 * ((granularity / 1000) + 1);
    }
    log_warn(LD_CONFIG, "Log time granularity '%d' has to be either a "
                        "divisor or a multiple of 1 second. Changing to "
                        "'%d'.",
             options->LogTimeGranularity, granularity);
    if (!validate_only)
      set_log_time_granularity(granularity);
  } else {
    if (!validate_only)
      set_log_time_granularity(options->LogTimeGranularity);
  }

  ok = 1;
  elts = smartlist_new();

  for (opt = options->Logs; opt; opt = opt->next) {
    log_severity_list_t *severity;
    const char *cfg = opt->value;
    severity = tor_malloc_zero(sizeof(log_severity_list_t));
    if (parse_log_severity_config(&cfg, severity) < 0) {
      log_warn(LD_CONFIG, "Couldn't parse log levels in Log option 'Log %s'",
               opt->value);
      ok = 0; goto cleanup;
    }

    smartlist_split_string(elts, cfg, NULL,
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 2);

    if (smartlist_len(elts) == 0)
      smartlist_add(elts, tor_strdup("stdout"));

    if (smartlist_len(elts) == 1 &&
        (!strcasecmp(smartlist_get(elts,0), "stdout") ||
         !strcasecmp(smartlist_get(elts,0), "stderr"))) {
      int err = smartlist_len(elts) &&
        !strcasecmp(smartlist_get(elts,0), "stderr");
      if (!validate_only) {
        if (daemon) {
          log_warn(LD_CONFIG,
                   "Can't log to %s with RunAsDaemon set; skipping stdout",
                   err?"stderr":"stdout");
        } else {
          add_stream_log(severity, err?"<stderr>":"<stdout>",
                         fileno(err?stderr:stdout));
        }
      }
      goto cleanup;
    }
    if (smartlist_len(elts) == 1 &&
        !strcasecmp(smartlist_get(elts,0), "syslog")) {
#ifdef HAVE_SYSLOG_H
      if (!validate_only) {
        add_syslog_log(severity);
      }
#else
      log_warn(LD_CONFIG, "Syslog is not supported on this system. Sorry.");
#endif
      goto cleanup;
    }

    if (smartlist_len(elts) == 2 &&
        !strcasecmp(smartlist_get(elts,0), "file")) {
      if (!validate_only) {
        char *fname = expand_filename(smartlist_get(elts, 1));
        if (add_file_log(severity, fname) < 0) {
          log_warn(LD_CONFIG, "Couldn't open file for 'Log %s': %s",
                   opt->value, strerror(errno));
          ok = 0;
        }
        tor_free(fname);
      }
      goto cleanup;
    }

    log_warn(LD_CONFIG, "Bad syntax on file Log option 'Log %s'",
             opt->value);
    ok = 0; goto cleanup;

  cleanup:
    SMARTLIST_FOREACH(elts, char*, cp, tor_free(cp));
    smartlist_clear(elts);
    tor_free(severity);
  }
  smartlist_free(elts);

  if (ok && !validate_only)
    logs_set_domain_logging(options->LogMessageDomains);

  return ok?0:-1;
}

/** Given a smartlist of SOCKS arguments to be passed to a transport
 *  proxy in <b>args</b>, validate them and return -1 if they are
 *  corrupted. Return 0 if they seem OK. */
static int
validate_transport_socks_arguments(const smartlist_t *args)
{
  char *socks_string = NULL;
  size_t socks_string_len;

  tor_assert(args);
  tor_assert(smartlist_len(args) > 0);

  SMARTLIST_FOREACH_BEGIN(args, const char *, s) {
    if (!string_is_key_value(LOG_WARN, s)) { /* items should be k=v items */
      log_warn(LD_CONFIG, "'%s' is not a k=v item.", s);
      return -1;
    }
  } SMARTLIST_FOREACH_END(s);

  socks_string = pt_stringify_socks_args(args);
  if (!socks_string)
    return -1;

  socks_string_len = strlen(socks_string);
  tor_free(socks_string);

  if (socks_string_len > MAX_SOCKS5_AUTH_SIZE_TOTAL) {
    log_warn(LD_CONFIG, "SOCKS arguments can't be more than %u bytes (%lu).",
             MAX_SOCKS5_AUTH_SIZE_TOTAL,
             (unsigned long) socks_string_len);
    return -1;
  }

  return 0;
}

/** Deallocate a bridge_line_t structure. */
/* private */ void
bridge_line_free(bridge_line_t *bridge_line)
{
  if (!bridge_line)
    return;

  if (bridge_line->socks_args) {
    SMARTLIST_FOREACH(bridge_line->socks_args, char*, s, tor_free(s));
    smartlist_free(bridge_line->socks_args);
  }
  tor_free(bridge_line->transport_name);
  tor_free(bridge_line);
}

/** Read the contents of a Bridge line from <b>line</b>. Return 0
 * if the line is well-formed, and -1 if it isn't. If
 * <b>validate_only</b> is 0, and the line is well-formed, then add
 * the bridge described in the line to our internal bridge list.
 *
 * Bridge line format:
 * Bridge [transport] IP:PORT [id-fingerprint] [k=v] [k=v] ...
 */
/* private */ bridge_line_t *
parse_bridge_line(const char *line)
{
  smartlist_t *items = NULL;
  char *addrport=NULL, *fingerprint=NULL;
  char *field=NULL;
  bridge_line_t *bridge_line = tor_malloc_zero(sizeof(bridge_line_t));

  items = smartlist_new();
  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);
  if (smartlist_len(items) < 1) {
    log_warn(LD_CONFIG, "Too few arguments to Bridge line.");
    goto err;
  }

  /* first field is either a transport name or addrport */
  field = smartlist_get(items, 0);
  smartlist_del_keeporder(items, 0);

  if (string_is_C_identifier(field)) {
    /* It's a transport name. */
    bridge_line->transport_name = field;
    if (smartlist_len(items) < 1) {
      log_warn(LD_CONFIG, "Too few items to Bridge line.");
      goto err;
    }
    addrport = smartlist_get(items, 0); /* Next field is addrport then. */
    smartlist_del_keeporder(items, 0);
  } else {
    addrport = field;
  }

  if (tor_addr_port_parse(LOG_INFO, addrport,
                          &bridge_line->addr, &bridge_line->port, 443)<0) {
    log_warn(LD_CONFIG, "Error parsing Bridge address '%s'", addrport);
    goto err;
  }

  /* If transports are enabled, next field could be a fingerprint or a
     socks argument. If transports are disabled, next field must be
     a fingerprint. */
  if (smartlist_len(items)) {
    if (bridge_line->transport_name) { /* transports enabled: */
      field = smartlist_get(items, 0);
      smartlist_del_keeporder(items, 0);

      /* If it's a key=value pair, then it's a SOCKS argument for the
         transport proxy... */
      if (string_is_key_value(LOG_DEBUG, field)) {
        bridge_line->socks_args = smartlist_new();
        smartlist_add(bridge_line->socks_args, field);
      } else { /* ...otherwise, it's the bridge fingerprint. */
        fingerprint = field;
      }

    } else { /* transports disabled: */
      fingerprint = smartlist_join_strings(items, "", 0, NULL);
    }
  }

  /* Handle fingerprint, if it was provided. */
  if (fingerprint) {
    if (strlen(fingerprint) != HEX_DIGEST_LEN) {
      log_warn(LD_CONFIG, "Key digest for Bridge is wrong length.");
      goto err;
    }
    if (base16_decode(bridge_line->digest, DIGEST_LEN,
                      fingerprint, HEX_DIGEST_LEN)<0) {
      log_warn(LD_CONFIG, "Unable to decode Bridge key digest.");
      goto err;
    }
  }

  /* If we are using transports, any remaining items in the smartlist
     should be k=v values. */
  if (bridge_line->transport_name && smartlist_len(items)) {
    if (!bridge_line->socks_args)
      bridge_line->socks_args = smartlist_new();

    /* append remaining items of 'items' to 'socks_args' */
    smartlist_add_all(bridge_line->socks_args, items);
    smartlist_clear(items);

    tor_assert(smartlist_len(bridge_line->socks_args) > 0);
  }

  if (bridge_line->socks_args) {
    if (validate_transport_socks_arguments(bridge_line->socks_args) < 0)
      goto err;
  }

  goto done;

 err:
  bridge_line_free(bridge_line);
  bridge_line = NULL;

 done:
  SMARTLIST_FOREACH(items, char*, s, tor_free(s));
  smartlist_free(items);
  tor_free(addrport);
  tor_free(fingerprint);

  return bridge_line;
}

/** Read the contents of a ClientTransportPlugin line from
 * <b>line</b>. Return 0 if the line is well-formed, and -1 if it
 * isn't.
 *
 * If <b>validate_only</b> is 0, the line is well-formed, and the
 * transport is needed by some bridge:
 * - If it's an external proxy line, add the transport described in the line to
 * our internal transport list.
 * - If it's a managed proxy line, launch the managed proxy. */
static int
parse_client_transport_line(const or_options_t *options,
                            const char *line, int validate_only)
{
  smartlist_t *items = NULL;
  int r;
  char *field2=NULL;

  const char *transports=NULL;
  smartlist_t *transport_list=NULL;
  char *addrport=NULL;
  tor_addr_t addr;
  uint16_t port = 0;
  int socks_ver=PROXY_NONE;

  /* managed proxy options */
  int is_managed=0;
  char **proxy_argv=NULL;
  char **tmp=NULL;
  int proxy_argc, i;
  int is_useless_proxy=1;

  int line_length;

  items = smartlist_new();
  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  line_length =  smartlist_len(items);
  if (line_length < 3) {
    log_warn(LD_CONFIG, "Too few arguments on ClientTransportPlugin line.");
    goto err;
  }

  /* Get the first line element, split it to commas into
     transport_list (in case it's multiple transports) and validate
     the transport names. */
  transports = smartlist_get(items, 0);
  transport_list = smartlist_new();
  smartlist_split_string(transport_list, transports, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(transport_list, const char *, transport_name) {
    /* validate transport names */
    if (!string_is_C_identifier(transport_name)) {
      log_warn(LD_CONFIG, "Transport name is not a C identifier (%s).",
               transport_name);
      goto err;
    }

    /* see if we actually need the transports provided by this proxy */
    if (!validate_only && transport_is_needed(transport_name))
      is_useless_proxy = 0;
  } SMARTLIST_FOREACH_END(transport_name);

  /* field2 is either a SOCKS version or "exec" */
  field2 = smartlist_get(items, 1);

  if (!strcmp(field2,"socks4")) {
    socks_ver = PROXY_SOCKS4;
  } else if (!strcmp(field2,"socks5")) {
    socks_ver = PROXY_SOCKS5;
  } else if (!strcmp(field2,"exec")) {
    is_managed=1;
  } else {
    log_warn(LD_CONFIG, "Strange ClientTransportPlugin field '%s'.",
             field2);
    goto err;
  }

  if (is_managed && options->Sandbox) {
    log_warn(LD_CONFIG, "Managed proxies are not compatible with Sandbox mode."
             "(ClientTransportPlugin line was %s)", escaped(line));
    goto err;
  }

  if (is_managed) { /* managed */
    if (!validate_only && is_useless_proxy) {
      log_info(LD_GENERAL, "Pluggable transport proxy (%s) does not provide "
               "any needed transports and will not be launched.", line);
    }

    /* If we are not just validating, use the rest of the line as the
       argv of the proxy to be launched. Also, make sure that we are
       only launching proxies that contribute useful transports.  */
    if (!validate_only && !is_useless_proxy) {
      proxy_argc = line_length-2;
      tor_assert(proxy_argc > 0);
      proxy_argv = tor_malloc_zero(sizeof(char*)*(proxy_argc+1));
      tmp = proxy_argv;
      for (i=0;i<proxy_argc;i++) { /* store arguments */
        *tmp++ = smartlist_get(items, 2);
        smartlist_del_keeporder(items, 2);
      }
      *tmp = NULL; /*terminated with NULL, just like execve() likes it*/

      /* kickstart the thing */
      pt_kickstart_client_proxy(transport_list, proxy_argv);
    }
  } else { /* external */
    if (smartlist_len(transport_list) != 1) {
      log_warn(LD_CONFIG, "You can't have an external proxy with "
               "more than one transports.");
      goto err;
    }

    addrport = smartlist_get(items, 2);

    if (tor_addr_port_lookup(addrport, &addr, &port)<0) {
      log_warn(LD_CONFIG, "Error parsing transport "
               "address '%s'", addrport);
      goto err;
    }
    if (!port) {
      log_warn(LD_CONFIG,
               "Transport address '%s' has no port.", addrport);
      goto err;
    }

    if (!validate_only) {
      transport_add_from_config(&addr, port, smartlist_get(transport_list, 0),
                                socks_ver);

      log_info(LD_DIR, "Transport '%s' found at %s",
               transports, fmt_addrport(&addr, port));
    }
  }

  r = 0;
  goto done;

 err:
  r = -1;

 done:
  SMARTLIST_FOREACH(items, char*, s, tor_free(s));
  smartlist_free(items);
  if (transport_list) {
    SMARTLIST_FOREACH(transport_list, char*, s, tor_free(s));
    smartlist_free(transport_list);
  }

  return r;
}

/** Given a ServerTransportListenAddr <b>line</b>, return its
 *  <address:port> string. Return NULL if the line was not
 *  well-formed.
 *
 *  If <b>transport</b> is set, return NULL if the line is not
 *  referring to <b>transport</b>.
 *
 *  The returned string is allocated on the heap and it's the
 *  responsibility of the caller to free it. */
static char *
get_bindaddr_from_transport_listen_line(const char *line,const char *transport)
{
  smartlist_t *items = NULL;
  const char *parsed_transport = NULL;
  char *addrport = NULL;
  tor_addr_t addr;
  uint16_t port = 0;

  items = smartlist_new();
  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  if (smartlist_len(items) < 2) {
    log_warn(LD_CONFIG,"Too few arguments on ServerTransportListenAddr line.");
    goto err;
  }

  parsed_transport = smartlist_get(items, 0);
  addrport = tor_strdup(smartlist_get(items, 1));

  /* If 'transport' is given, check if it matches the one on the line */
  if (transport && strcmp(transport, parsed_transport))
    goto err;

  /* Validate addrport */
  if (tor_addr_port_parse(LOG_WARN, addrport, &addr, &port, -1)<0) {
    log_warn(LD_CONFIG, "Error parsing ServerTransportListenAddr "
             "address '%s'", addrport);
    goto err;
  }

  goto done;

 err:
  tor_free(addrport);
  addrport = NULL;

 done:
  SMARTLIST_FOREACH(items, char*, s, tor_free(s));
  smartlist_free(items);

  return addrport;
}

/** Given a ServerTransportOptions <b>line</b>, return a smartlist
 *  with the options. Return NULL if the line was not well-formed.
 *
 *  If <b>transport</b> is set, return NULL if the line is not
 *  referring to <b>transport</b>.
 *
 *  The returned smartlist and its strings are allocated on the heap
 *  and it's the responsibility of the caller to free it. */
smartlist_t *
get_options_from_transport_options_line(const char *line,const char *transport)
{
  smartlist_t *items = smartlist_new();
  smartlist_t *options = smartlist_new();
  const char *parsed_transport = NULL;

  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  if (smartlist_len(items) < 2) {
    log_warn(LD_CONFIG,"Too few arguments on ServerTransportOptions line.");
    goto err;
  }

  parsed_transport = smartlist_get(items, 0);
  /* If 'transport' is given, check if it matches the one on the line */
  if (transport && strcmp(transport, parsed_transport))
    goto err;

  SMARTLIST_FOREACH_BEGIN(items, const char *, option) {
    if (option_sl_idx == 0) /* skip the transport field (first field)*/
      continue;

    /* validate that it's a k=v value */
    if (!string_is_key_value(LOG_WARN, option)) {
      log_warn(LD_CONFIG, "%s is not a k=v value.", escaped(option));
      goto err;
    }

    /* add it to the options smartlist */
    smartlist_add(options, tor_strdup(option));
    log_debug(LD_CONFIG, "Added %s to the list of options", escaped(option));
  } SMARTLIST_FOREACH_END(option);

  goto done;

 err:
  SMARTLIST_FOREACH(options, char*, s, tor_free(s));
  smartlist_free(options);
  options = NULL;

 done:
  SMARTLIST_FOREACH(items, char*, s, tor_free(s));
  smartlist_free(items);

  return options;
}

/** Given the name of a pluggable transport in <b>transport</b>, check
 *  the configuration file to see if the user has explicitly asked for
 *  it to listen on a specific port. Return a <address:port> string if
 *  so, otherwise NULL. */
char *
get_transport_bindaddr_from_config(const char *transport)
{
  config_line_t *cl;
  const or_options_t *options = get_options();

  for (cl = options->ServerTransportListenAddr; cl; cl = cl->next) {
    char *bindaddr =
      get_bindaddr_from_transport_listen_line(cl->value, transport);
    if (bindaddr)
      return bindaddr;
  }

  return NULL;
}

/** Given the name of a pluggable transport in <b>transport</b>, check
 *  the configuration file to see if the user has asked us to pass any
 *  parameters to the pluggable transport. Return a smartlist
 *  containing the parameters, otherwise NULL. */
smartlist_t *
get_options_for_server_transport(const char *transport)
{
  config_line_t *cl;
  const or_options_t *options = get_options();

  for (cl = options->ServerTransportOptions; cl; cl = cl->next) {
    smartlist_t *options_sl =
      get_options_from_transport_options_line(cl->value, transport);
    if (options_sl)
      return options_sl;
  }

  return NULL;
}

/** Read the contents of a ServerTransportPlugin line from
 * <b>line</b>. Return 0 if the line is well-formed, and -1 if it
 * isn't.
 * If <b>validate_only</b> is 0, the line is well-formed, and it's a
 * managed proxy line, launch the managed proxy. */
static int
parse_server_transport_line(const or_options_t *options,
                            const char *line, int validate_only)
{
  smartlist_t *items = NULL;
  int r;
  const char *transports=NULL;
  smartlist_t *transport_list=NULL;
  char *type=NULL;
  char *addrport=NULL;
  tor_addr_t addr;
  uint16_t port = 0;

  /* managed proxy options */
  int is_managed=0;
  char **proxy_argv=NULL;
  char **tmp=NULL;
  int proxy_argc,i;

  int line_length;

  items = smartlist_new();
  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  line_length =  smartlist_len(items);
  if (line_length < 3) {
    log_warn(LD_CONFIG, "Too few arguments on ServerTransportPlugin line.");
    goto err;
  }

  /* Get the first line element, split it to commas into
     transport_list (in case it's multiple transports) and validate
     the transport names. */
  transports = smartlist_get(items, 0);
  transport_list = smartlist_new();
  smartlist_split_string(transport_list, transports, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(transport_list, const char *, transport_name) {
    if (!string_is_C_identifier(transport_name)) {
      log_warn(LD_CONFIG, "Transport name is not a C identifier (%s).",
               transport_name);
      goto err;
    }
  } SMARTLIST_FOREACH_END(transport_name);

  type = smartlist_get(items, 1);

  if (!strcmp(type, "exec")) {
    is_managed=1;
  } else if (!strcmp(type, "proxy")) {
    is_managed=0;
  } else {
    log_warn(LD_CONFIG, "Strange ServerTransportPlugin type '%s'", type);
    goto err;
  }

  if (is_managed && options->Sandbox) {
    log_warn(LD_CONFIG, "Managed proxies are not compatible with Sandbox mode."
             "(ServerTransportPlugin line was %s)", escaped(line));
    goto err;
  }

  if (is_managed) { /* managed */
    if (!validate_only) {
      proxy_argc = line_length-2;
      tor_assert(proxy_argc > 0);
      proxy_argv = tor_malloc_zero(sizeof(char*)*(proxy_argc+1));
      tmp = proxy_argv;

      for (i=0;i<proxy_argc;i++) { /* store arguments */
        *tmp++ = smartlist_get(items, 2);
        smartlist_del_keeporder(items, 2);
      }
      *tmp = NULL; /*terminated with NULL, just like execve() likes it*/

      /* kickstart the thing */
      pt_kickstart_server_proxy(transport_list, proxy_argv);
    }
  } else { /* external */
    if (smartlist_len(transport_list) != 1) {
      log_warn(LD_CONFIG, "You can't have an external proxy with "
               "more than one transports.");
      goto err;
    }

    addrport = smartlist_get(items, 2);

    if (tor_addr_port_lookup(addrport, &addr, &port)<0) {
      log_warn(LD_CONFIG, "Error parsing transport "
               "address '%s'", addrport);
      goto err;
    }
    if (!port) {
      log_warn(LD_CONFIG,
               "Transport address '%s' has no port.", addrport);
      goto err;
    }

    if (!validate_only) {
      log_info(LD_DIR, "Server transport '%s' at %s.",
               transports, fmt_addrport(&addr, port));
    }
  }

  r = 0;
  goto done;

 err:
  r = -1;

 done:
  SMARTLIST_FOREACH(items, char*, s, tor_free(s));
  smartlist_free(items);
  if (transport_list) {
    SMARTLIST_FOREACH(transport_list, char*, s, tor_free(s));
    smartlist_free(transport_list);
  }

  return r;
}

/** Read the contents of a DirAuthority line from <b>line</b>. If
 * <b>validate_only</b> is 0, and the line is well-formed, and it
 * shares any bits with <b>required_type</b> or <b>required_type</b>
 * is 0, then add the dirserver described in the line (minus whatever
 * bits it's missing) as a valid authority. Return 0 on success,
 * or -1 if the line isn't well-formed or if we can't add it. */
static int
parse_dir_authority_line(const char *line, dirinfo_type_t required_type,
                         int validate_only)
{
  smartlist_t *items = NULL;
  int r;
  char *addrport=NULL, *address=NULL, *nickname=NULL, *fingerprint=NULL;
  uint16_t dir_port = 0, or_port = 0;
  char digest[DIGEST_LEN];
  char v3_digest[DIGEST_LEN];
  dirinfo_type_t type = 0;
  double weight = 1.0;

  items = smartlist_new();
  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);
  if (smartlist_len(items) < 1) {
    log_warn(LD_CONFIG, "No arguments on DirAuthority line.");
    goto err;
  }

  if (is_legal_nickname(smartlist_get(items, 0))) {
    nickname = smartlist_get(items, 0);
    smartlist_del_keeporder(items, 0);
  }

  while (smartlist_len(items)) {
    char *flag = smartlist_get(items, 0);
    if (TOR_ISDIGIT(flag[0]))
      break;
    if (!strcasecmp(flag, "hs") ||
               !strcasecmp(flag, "no-hs")) {
      log_warn(LD_CONFIG, "The DirAuthority options 'hs' and 'no-hs' are "
               "obsolete; you don't need them any more.");
    } else if (!strcasecmp(flag, "bridge")) {
      type |= BRIDGE_DIRINFO;
    } else if (!strcasecmp(flag, "no-v2")) {
      /* obsolete, but may still be contained in DirAuthority lines generated
         by various tools */;
    } else if (!strcasecmpstart(flag, "orport=")) {
      int ok;
      char *portstring = flag + strlen("orport=");
      or_port = (uint16_t) tor_parse_long(portstring, 10, 1, 65535, &ok, NULL);
      if (!ok)
        log_warn(LD_CONFIG, "Invalid orport '%s' on DirAuthority line.",
                 portstring);
    } else if (!strcmpstart(flag, "weight=")) {
      int ok;
      const char *wstring = flag + strlen("weight=");
      weight = tor_parse_double(wstring, 0, UINT64_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_CONFIG, "Invalid weight '%s' on DirAuthority line.",flag);
        weight=1.0;
      }
    } else if (!strcasecmpstart(flag, "v3ident=")) {
      char *idstr = flag + strlen("v3ident=");
      if (strlen(idstr) != HEX_DIGEST_LEN ||
          base16_decode(v3_digest, DIGEST_LEN, idstr, HEX_DIGEST_LEN)<0) {
        log_warn(LD_CONFIG, "Bad v3 identity digest '%s' on DirAuthority line",
                 flag);
      } else {
        type |= V3_DIRINFO|EXTRAINFO_DIRINFO|MICRODESC_DIRINFO;
      }
    } else {
      log_warn(LD_CONFIG, "Unrecognized flag '%s' on DirAuthority line",
               flag);
    }
    tor_free(flag);
    smartlist_del_keeporder(items, 0);
  }

  if (smartlist_len(items) < 2) {
    log_warn(LD_CONFIG, "Too few arguments to DirAuthority line.");
    goto err;
  }
  addrport = smartlist_get(items, 0);
  smartlist_del_keeporder(items, 0);
  if (addr_port_lookup(LOG_WARN, addrport, &address, NULL, &dir_port)<0) {
    log_warn(LD_CONFIG, "Error parsing DirAuthority address '%s'", addrport);
    goto err;
  }
  if (!dir_port) {
    log_warn(LD_CONFIG, "Missing port in DirAuthority address '%s'",addrport);
    goto err;
  }

  fingerprint = smartlist_join_strings(items, "", 0, NULL);
  if (strlen(fingerprint) != HEX_DIGEST_LEN) {
    log_warn(LD_CONFIG, "Key digest '%s' for DirAuthority is wrong length %d.",
             fingerprint, (int)strlen(fingerprint));
    goto err;
  }
  if (!strcmp(fingerprint, "E623F7625FBE0C87820F11EC5F6D5377ED816294")) {
    /* a known bad fingerprint. refuse to use it. We can remove this
     * clause once Tor 0.1.2.17 is obsolete. */
    log_warn(LD_CONFIG, "Dangerous dirserver line. To correct, erase your "
             "torrc file (%s), or reinstall Tor and use the default torrc.",
             get_torrc_fname(0));
    goto err;
  }
  if (base16_decode(digest, DIGEST_LEN, fingerprint, HEX_DIGEST_LEN)<0) {
    log_warn(LD_CONFIG, "Unable to decode DirAuthority key digest.");
    goto err;
  }

  if (!validate_only && (!required_type || required_type & type)) {
    dir_server_t *ds;
    if (required_type)
      type &= required_type; /* pare down what we think of them as an
                              * authority for. */
    log_debug(LD_DIR, "Trusted %d dirserver at %s:%d (%s)", (int)type,
              address, (int)dir_port, (char*)smartlist_get(items,0));
    if (!(ds = trusted_dir_server_new(nickname, address, dir_port, or_port,
                                      digest, v3_digest, type, weight)))
      goto err;
    dir_server_add(ds);
  }

  r = 0;
  goto done;

  err:
  r = -1;

  done:
  SMARTLIST_FOREACH(items, char*, s, tor_free(s));
  smartlist_free(items);
  tor_free(addrport);
  tor_free(address);
  tor_free(nickname);
  tor_free(fingerprint);
  return r;
}

/** Read the contents of a FallbackDir line from <b>line</b>. If
 * <b>validate_only</b> is 0, and the line is well-formed, then add the
 * dirserver described in the line as a fallback directory. Return 0 on
 * success, or -1 if the line isn't well-formed or if we can't add it. */
static int
parse_dir_fallback_line(const char *line,
                        int validate_only)
{
  int r = -1;
  smartlist_t *items = smartlist_new(), *positional = smartlist_new();
  int orport = -1;
  uint16_t dirport;
  tor_addr_t addr;
  int ok;
  char id[DIGEST_LEN];
  char *address=NULL;
  double weight=1.0;

  memset(id, 0, sizeof(id));
  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);
  SMARTLIST_FOREACH_BEGIN(items, const char *, cp) {
    const char *eq = strchr(cp, '=');
    ok = 1;
    if (! eq) {
      smartlist_add(positional, (char*)cp);
      continue;
    }
    if (!strcmpstart(cp, "orport=")) {
      orport = (int)tor_parse_long(cp+strlen("orport="), 10,
                                   1, 65535, &ok, NULL);
    } else if (!strcmpstart(cp, "id=")) {
      ok = !base16_decode(id, DIGEST_LEN,
                          cp+strlen("id="), strlen(cp)-strlen("id="));
    } else if (!strcmpstart(cp, "weight=")) {
      int ok;
      const char *wstring = cp + strlen("weight=");
      weight = tor_parse_double(wstring, 0, UINT64_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_CONFIG, "Invalid weight '%s' on FallbackDir line.", cp);
        weight=1.0;
      }
    }

    if (!ok) {
      log_warn(LD_CONFIG, "Bad FallbackDir option %s", escaped(cp));
      goto end;
    }
  } SMARTLIST_FOREACH_END(cp);

  if (smartlist_len(positional) != 1) {
    log_warn(LD_CONFIG, "Couldn't parse FallbackDir line %s", escaped(line));
    goto end;
  }

  if (tor_digest_is_zero(id)) {
    log_warn(LD_CONFIG, "Missing identity on FallbackDir line");
    goto end;
  }

  if (orport <= 0) {
    log_warn(LD_CONFIG, "Missing orport on FallbackDir line");
    goto end;
  }

  if (tor_addr_port_split(LOG_INFO, smartlist_get(positional, 0),
                          &address, &dirport) < 0 ||
      tor_addr_parse(&addr, address)<0) {
    log_warn(LD_CONFIG, "Couldn't parse address:port %s on FallbackDir line",
             (const char*)smartlist_get(positional, 0));
    goto end;
  }

  if (!validate_only) {
    dir_server_t *ds;
    ds = fallback_dir_server_new(&addr, dirport, orport, id, weight);
    if (!ds) {
      log_warn(LD_CONFIG, "Couldn't create FallbackDir %s", escaped(line));
      goto end;
    }
    dir_server_add(ds);
  }

  r = 0;

 end:
  SMARTLIST_FOREACH(items, char *, cp, tor_free(cp));
  smartlist_free(items);
  smartlist_free(positional);
  tor_free(address);
  return r;
}

/** Allocate and return a new port_cfg_t with reasonable defaults. */
static port_cfg_t *
port_cfg_new(void)
{
  port_cfg_t *cfg = tor_malloc_zero(sizeof(port_cfg_t));
  cfg->ipv4_traffic = 1;
  cfg->cache_ipv4_answers = 1;
  cfg->prefer_ipv6_virtaddr = 1;
  return cfg;
}

/** Free all storage held in <b>port</b> */
static void
port_cfg_free(port_cfg_t *port)
{
  tor_free(port);
}

/** Warn for every port in <b>ports</b> of type <b>listener_type</b> that is
 * on a publicly routable address. */
static void
warn_nonlocal_client_ports(const smartlist_t *ports, const char *portname,
                           int listener_type)
{
  SMARTLIST_FOREACH_BEGIN(ports, const port_cfg_t *, port) {
    if (port->type != listener_type)
      continue;
    if (port->is_unix_addr) {
      /* Unix sockets aren't accessible over a network. */
    } else if (!tor_addr_is_internal(&port->addr, 1)) {
      log_warn(LD_CONFIG, "You specified a public address '%s' for %sPort. "
               "Other people on the Internet might find your computer and "
               "use it as an open proxy. Please don't allow this unless you "
               "have a good reason.",
               fmt_addrport(&port->addr, port->port), portname);
    } else if (!tor_addr_is_loopback(&port->addr)) {
      log_notice(LD_CONFIG, "You configured a non-loopback address '%s' "
                 "for %sPort. This allows everybody on your local network to "
                 "use your machine as a proxy. Make sure this is what you "
                 "wanted.",
                 fmt_addrport(&port->addr, port->port), portname);
    }
  } SMARTLIST_FOREACH_END(port);
}

/** Warn for every Extended ORPort port in <b>ports</b> that is on a
 *  publicly routable address. */
static void
warn_nonlocal_ext_orports(const smartlist_t *ports, const char *portname)
{
  SMARTLIST_FOREACH_BEGIN(ports, const port_cfg_t *, port) {
    if (port->type != CONN_TYPE_EXT_OR_LISTENER)
      continue;
    if (port->is_unix_addr)
      continue;
    /* XXX maybe warn even if address is RFC1918? */
    if (!tor_addr_is_internal(&port->addr, 1)) {
      log_warn(LD_CONFIG, "You specified a public address '%s' for %sPort. "
               "This is not advised; this address is supposed to only be "
               "exposed on localhost so that your pluggable transport "
               "proxies can connect to it.",
               fmt_addrport(&port->addr, port->port), portname);
    }
  } SMARTLIST_FOREACH_END(port);
}

/** Given a list of port_cfg_t in <b>ports</b>, warn any controller port there
 * is listening on any non-loopback address.  If <b>forbid</b> is true,
 * then emit a stronger warning and remove the port from the list.
 */
static void
warn_nonlocal_controller_ports(smartlist_t *ports, unsigned forbid)
{
  int warned = 0;
  SMARTLIST_FOREACH_BEGIN(ports, port_cfg_t *, port) {
    if (port->type != CONN_TYPE_CONTROL_LISTENER)
      continue;
    if (port->is_unix_addr)
      continue;
    if (!tor_addr_is_loopback(&port->addr)) {
      if (forbid) {
        if (!warned)
          log_warn(LD_CONFIG,
                 "You have a ControlPort set to accept "
                 "unauthenticated connections from a non-local address.  "
                 "This means that programs not running on your computer "
                 "can reconfigure your Tor, without even having to guess a "
                 "password.  That's so bad that I'm closing your ControlPort "
                 "for you.  If you need to control your Tor remotely, try "
                 "enabling authentication and using a tool like stunnel or "
                 "ssh to encrypt remote access.");
        warned = 1;
        port_cfg_free(port);
        SMARTLIST_DEL_CURRENT(ports, port);
      } else {
        log_warn(LD_CONFIG, "You have a ControlPort set to accept "
                 "connections from a non-local address.  This means that "
                 "programs not running on your computer can reconfigure your "
                 "Tor.  That's pretty bad, since the controller "
                 "protocol isn't encrypted!  Maybe you should just listen on "
                 "127.0.0.1 and use a tool like stunnel or ssh to encrypt "
                 "remote connections to your control port.");
        return; /* No point in checking the rest */
      }
    }
  } SMARTLIST_FOREACH_END(port);
}

#define CL_PORT_NO_OPTIONS (1u<<0)
#define CL_PORT_WARN_NONLOCAL (1u<<1)
#define CL_PORT_ALLOW_EXTRA_LISTENADDR (1u<<2)
#define CL_PORT_SERVER_OPTIONS (1u<<3)
#define CL_PORT_FORBID_NONLOCAL (1u<<4)
#define CL_PORT_TAKES_HOSTNAMES (1u<<5)

/**
 * Parse port configuration for a single port type.
 *
 * Read entries of the "FooPort" type from the list <b>ports</b>, and
 * entries of the "FooListenAddress" type from the list
 * <b>listenaddrs</b>.  Two syntaxes are supported: a legacy syntax
 * where FooPort is at most a single entry containing a port number and
 * where FooListenAddress has any number of address:port combinations;
 * and a new syntax where there are no FooListenAddress entries and
 * where FooPort can have any number of entries of the format
 * "[Address:][Port] IsolationOptions".
 *
 * In log messages, describe the port type as <b>portname</b>.
 *
 * If no address is specified, default to <b>defaultaddr</b>.  If no
 * FooPort is given, default to defaultport (if 0, there is no default).
 *
 * If CL_PORT_NO_OPTIONS is set in <b>flags</b>, do not allow stream
 * isolation options in the FooPort entries.
 *
 * If CL_PORT_WARN_NONLOCAL is set in <b>flags</b>, warn if any of the
 * ports are not on a local address.  If CL_PORT_FORBID_NONLOCAL is set,
 * this is a contrl port with no password set: don't even allow it.
 *
 * Unless CL_PORT_ALLOW_EXTRA_LISTENADDR is set in <b>flags</b>, warn
 * if FooListenAddress is set but FooPort is 0.
 *
 * If CL_PORT_SERVER_OPTIONS is set in <b>flags</b>, do not allow stream
 * isolation options in the FooPort entries; instead allow the
 * server-port option set.
 *
 * If CL_PORT_TAKES_HOSTNAMES is set in <b>flags</b>, allow the options
 * {No,}IPv{4,6}Traffic.
 *
 * On success, if <b>out</b> is given, add a new port_cfg_t entry to
 * <b>out</b> for every port that the client should listen on.  Return 0
 * on success, -1 on failure.
 */
static int
parse_port_config(smartlist_t *out,
                  const config_line_t *ports,
                  const config_line_t *listenaddrs,
                  const char *portname,
                  int listener_type,
                  const char *defaultaddr,
                  int defaultport,
                  unsigned flags)
{
  smartlist_t *elts;
  int retval = -1;
  const unsigned is_control = (listener_type == CONN_TYPE_CONTROL_LISTENER);
  const unsigned is_ext_orport = (listener_type == CONN_TYPE_EXT_OR_LISTENER);
  const unsigned allow_no_options = flags & CL_PORT_NO_OPTIONS;
  const unsigned use_server_options = flags & CL_PORT_SERVER_OPTIONS;
  const unsigned warn_nonlocal = flags & CL_PORT_WARN_NONLOCAL;
  const unsigned forbid_nonlocal = flags & CL_PORT_FORBID_NONLOCAL;
  const unsigned allow_spurious_listenaddr =
    flags & CL_PORT_ALLOW_EXTRA_LISTENADDR;
  const unsigned takes_hostnames = flags & CL_PORT_TAKES_HOSTNAMES;
  int got_zero_port=0, got_nonzero_port=0;

  /* FooListenAddress is deprecated; let's make it work like it used to work,
   * though. */
  if (listenaddrs) {
    int mainport = defaultport;

    if (ports && ports->next) {
      log_warn(LD_CONFIG, "%sListenAddress can't be used when there are "
               "multiple %sPort lines", portname, portname);
      return -1;
    } else if (ports) {
      if (!strcmp(ports->value, "auto")) {
        mainport = CFG_AUTO_PORT;
      } else {
        int ok;
        mainport = (int)tor_parse_long(ports->value, 10, 0, 65535, &ok, NULL);
        if (!ok) {
          log_warn(LD_CONFIG, "%sListenAddress can only be used with a single "
                   "%sPort with value \"auto\" or 1-65535 and no options set.",
                   portname, portname);
          return -1;
        }
      }
    }

    if (mainport == 0) {
      if (allow_spurious_listenaddr)
        return 1; /*DOCDOC*/
      log_warn(LD_CONFIG, "%sPort must be defined if %sListenAddress is used",
               portname, portname);
      return -1;
    }

    if (use_server_options && out) {
      /* Add a no_listen port. */
      port_cfg_t *cfg = port_cfg_new();
      cfg->type = listener_type;
      cfg->port = mainport;
      tor_addr_make_unspec(&cfg->addr); /* Server ports default to 0.0.0.0 */
      cfg->no_listen = 1;
      cfg->bind_ipv4_only = 1;
      cfg->ipv4_traffic = 1;
      cfg->prefer_ipv6_virtaddr = 1;
      smartlist_add(out, cfg);
    }

    for (; listenaddrs; listenaddrs = listenaddrs->next) {
      tor_addr_t addr;
      uint16_t port = 0;
      if (tor_addr_port_lookup(listenaddrs->value, &addr, &port) < 0) {
        log_warn(LD_CONFIG, "Unable to parse %sListenAddress '%s'",
                 portname, listenaddrs->value);
        return -1;
      }
      if (out) {
        port_cfg_t *cfg = port_cfg_new();
        cfg->type = listener_type;
        cfg->port = port ? port : mainport;
        tor_addr_copy(&cfg->addr, &addr);
        cfg->session_group = SESSION_GROUP_UNSET;
        cfg->isolation_flags = ISO_DEFAULT;
        cfg->no_advertise = 1;
        smartlist_add(out, cfg);
      }
    }

    if (warn_nonlocal && out) {
      if (is_control)
        warn_nonlocal_controller_ports(out, forbid_nonlocal);
      else if (is_ext_orport)
        warn_nonlocal_ext_orports(out, portname);
      else
        warn_nonlocal_client_ports(out, portname, listener_type);
    }
    return 0;
  } /* end if (listenaddrs) */

  /* No ListenAddress lines. If there's no FooPort, then maybe make a default
   * one. */
  if (! ports) {
    if (defaultport && out) {
       port_cfg_t *cfg = port_cfg_new();
       cfg->type = listener_type;
       cfg->port = defaultport;
       tor_addr_parse(&cfg->addr, defaultaddr);
       cfg->session_group = SESSION_GROUP_UNSET;
       cfg->isolation_flags = ISO_DEFAULT;
       smartlist_add(out, cfg);
    }
    return 0;
  }

  /* At last we can actually parse the FooPort lines.  The syntax is:
   * [Addr:](Port|auto) [Options].*/
  elts = smartlist_new();

  for (; ports; ports = ports->next) {
    tor_addr_t addr;
    int port;
    int sessiongroup = SESSION_GROUP_UNSET;
    unsigned isolation = ISO_DEFAULT;
    int prefer_no_auth = 0;

    char *addrport;
    uint16_t ptmp=0;
    int ok;
    int no_listen = 0, no_advertise = 0, all_addrs = 0,
      bind_ipv4_only = 0, bind_ipv6_only = 0,
      ipv4_traffic = 1, ipv6_traffic = 0, prefer_ipv6 = 0,
      cache_ipv4 = 1, use_cached_ipv4 = 0,
      cache_ipv6 = 0, use_cached_ipv6 = 0,
      prefer_ipv6_automap = 1;

    smartlist_split_string(elts, ports->value, NULL,
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
    if (smartlist_len(elts) == 0) {
      log_warn(LD_CONFIG, "Invalid %sPort line with no value", portname);
      goto err;
    }

    if (allow_no_options && smartlist_len(elts) > 1) {
      log_warn(LD_CONFIG, "Too many options on %sPort line", portname);
      goto err;
    }

    /* Now parse the addr/port value */
    addrport = smartlist_get(elts, 0);
    if (!strcmp(addrport, "auto")) {
      port = CFG_AUTO_PORT;
      tor_addr_parse(&addr, defaultaddr);
    } else if (!strcasecmpend(addrport, ":auto")) {
      char *addrtmp = tor_strndup(addrport, strlen(addrport)-5);
      port = CFG_AUTO_PORT;
      if (tor_addr_port_lookup(addrtmp, &addr, &ptmp)<0 || ptmp) {
        log_warn(LD_CONFIG, "Invalid address '%s' for %sPort",
                 escaped(addrport), portname);
        tor_free(addrtmp);
        goto err;
      }
    } else {
      /* Try parsing integer port before address, because, who knows?
         "9050" might be a valid address. */
      port = (int) tor_parse_long(addrport, 10, 0, 65535, &ok, NULL);
      if (ok) {
        tor_addr_parse(&addr, defaultaddr);
      } else if (tor_addr_port_lookup(addrport, &addr, &ptmp) == 0) {
        if (ptmp == 0) {
          log_warn(LD_CONFIG, "%sPort line has address but no port", portname);
          goto err;
        }
        port = ptmp;
      } else {
        log_warn(LD_CONFIG, "Couldn't parse address '%s' for %sPort",
                 escaped(addrport), portname);
        goto err;
      }
    }

    /* Now parse the rest of the options, if any. */
    if (use_server_options) {
      /* This is a server port; parse advertising options */
      SMARTLIST_FOREACH_BEGIN(elts, char *, elt) {
        if (elt_sl_idx == 0)
          continue; /* Skip addr:port */

        if (!strcasecmp(elt, "NoAdvertise")) {
          no_advertise = 1;
        } else if (!strcasecmp(elt, "NoListen")) {
          no_listen = 1;
#if 0
        /* not implemented yet. */
        } else if (!strcasecmp(elt, "AllAddrs")) {

          all_addrs = 1;
#endif
        } else if (!strcasecmp(elt, "IPv4Only")) {
          bind_ipv4_only = 1;
        } else if (!strcasecmp(elt, "IPv6Only")) {
          bind_ipv6_only = 1;
        } else {
          log_warn(LD_CONFIG, "Unrecognized %sPort option '%s'",
                   portname, escaped(elt));
        }
      } SMARTLIST_FOREACH_END(elt);

      if (no_advertise && no_listen) {
        log_warn(LD_CONFIG, "Tried to set both NoListen and NoAdvertise "
                 "on %sPort line '%s'",
                 portname, escaped(ports->value));
        goto err;
      }
      if (bind_ipv4_only && bind_ipv6_only) {
        log_warn(LD_CONFIG, "Tried to set both IPv4Only and IPv6Only "
                 "on %sPort line '%s'",
                 portname, escaped(ports->value));
        goto err;
      }
      if (bind_ipv4_only && tor_addr_family(&addr) == AF_INET6) {
        log_warn(LD_CONFIG, "Could not interpret %sPort address as IPv6",
                 portname);
        goto err;
      }
      if (bind_ipv6_only && tor_addr_family(&addr) == AF_INET) {
        log_warn(LD_CONFIG, "Could not interpret %sPort address as IPv4",
                 portname);
        goto err;
      }
    } else {
      /* This is a client port; parse isolation options */
      SMARTLIST_FOREACH_BEGIN(elts, char *, elt) {
        int no = 0, isoflag = 0;
        const char *elt_orig = elt;
        if (elt_sl_idx == 0)
          continue; /* Skip addr:port */
        if (!strcasecmpstart(elt, "SessionGroup=")) {
          int group = (int)tor_parse_long(elt+strlen("SessionGroup="),
                                          10, 0, INT_MAX, &ok, NULL);
          if (!ok) {
            log_warn(LD_CONFIG, "Invalid %sPort option '%s'",
                     portname, escaped(elt));
            goto err;
          }
          if (sessiongroup >= 0) {
            log_warn(LD_CONFIG, "Multiple SessionGroup options on %sPort",
                     portname);
            goto err;
          }
          sessiongroup = group;
          continue;
        }

        if (!strcasecmpstart(elt, "No")) {
          no = 1;
          elt += 2;
        }

        if (takes_hostnames) {
          if (!strcasecmp(elt, "IPv4Traffic")) {
            ipv4_traffic = ! no;
            continue;
          } else if (!strcasecmp(elt, "IPv6Traffic")) {
            ipv6_traffic = ! no;
            continue;
          } else if (!strcasecmp(elt, "PreferIPv6")) {
            prefer_ipv6 = ! no;
            continue;
          }
        }
        if (!strcasecmp(elt, "CacheIPv4DNS")) {
          cache_ipv4 = ! no;
          continue;
        } else if (!strcasecmp(elt, "CacheIPv6DNS")) {
          cache_ipv6 = ! no;
          continue;
        } else if (!strcasecmp(elt, "CacheDNS")) {
          cache_ipv4 = cache_ipv6 = ! no;
          continue;
        } else if (!strcasecmp(elt, "UseIPv4Cache")) {
          use_cached_ipv4 = ! no;
          continue;
        } else if (!strcasecmp(elt, "UseIPv6Cache")) {
          use_cached_ipv6 = ! no;
          continue;
        } else if (!strcasecmp(elt, "UseDNSCache")) {
          use_cached_ipv4 = use_cached_ipv6 = ! no;
          continue;
        } else if (!strcasecmp(elt, "PreferIPv6Automap")) {
          prefer_ipv6_automap = ! no;
          continue;
        } else if (!strcasecmp(elt, "PreferSOCKSNoAuth")) {
          prefer_no_auth = ! no;
          continue;
        }

        if (!strcasecmpend(elt, "s"))
          elt[strlen(elt)-1] = '\0'; /* kill plurals. */

        if (!strcasecmp(elt, "IsolateDestPort")) {
          isoflag = ISO_DESTPORT;
        } else if (!strcasecmp(elt, "IsolateDestAddr")) {
          isoflag = ISO_DESTADDR;
        } else if (!strcasecmp(elt, "IsolateSOCKSAuth")) {
          isoflag = ISO_SOCKSAUTH;
        } else if (!strcasecmp(elt, "IsolateClientProtocol")) {
          isoflag = ISO_CLIENTPROTO;
        } else if (!strcasecmp(elt, "IsolateClientAddr")) {
          isoflag = ISO_CLIENTADDR;
        } else {
          log_warn(LD_CONFIG, "Unrecognized %sPort option '%s'",
                   portname, escaped(elt_orig));
        }

        if (no) {
          isolation &= ~isoflag;
        } else {
          isolation |= isoflag;
        }
      } SMARTLIST_FOREACH_END(elt);
    }

    if (port)
      got_nonzero_port = 1;
    else
      got_zero_port = 1;

    if (ipv4_traffic == 0 && ipv6_traffic == 0) {
      log_warn(LD_CONFIG, "You have a %sPort entry with both IPv4 and "
               "IPv6 disabled; that won't work.", portname);
      goto err;
    }

    if (out && port) {
      port_cfg_t *cfg = port_cfg_new();
      tor_addr_copy(&cfg->addr, &addr);
      cfg->port = port;
      cfg->type = listener_type;
      cfg->isolation_flags = isolation;
      cfg->session_group = sessiongroup;
      cfg->no_advertise = no_advertise;
      cfg->no_listen = no_listen;
      cfg->all_addrs = all_addrs;
      cfg->bind_ipv4_only = bind_ipv4_only;
      cfg->bind_ipv6_only = bind_ipv6_only;
      cfg->ipv4_traffic = ipv4_traffic;
      cfg->ipv6_traffic = ipv6_traffic;
      cfg->prefer_ipv6 = prefer_ipv6;
      cfg->cache_ipv4_answers = cache_ipv4;
      cfg->cache_ipv6_answers = cache_ipv6;
      cfg->use_cached_ipv4_answers = use_cached_ipv4;
      cfg->use_cached_ipv6_answers = use_cached_ipv6;
      cfg->prefer_ipv6_virtaddr = prefer_ipv6_automap;
      cfg->socks_prefer_no_auth = prefer_no_auth;
      if (! (isolation & ISO_SOCKSAUTH))
        cfg->socks_prefer_no_auth = 1;

      smartlist_add(out, cfg);
    }
    SMARTLIST_FOREACH(elts, char *, cp, tor_free(cp));
    smartlist_clear(elts);
  }

  if (warn_nonlocal && out) {
    if (is_control)
      warn_nonlocal_controller_ports(out, forbid_nonlocal);
    else if (is_ext_orport)
      warn_nonlocal_ext_orports(out, portname);
    else
      warn_nonlocal_client_ports(out, portname, listener_type);
  }

  if (got_zero_port && got_nonzero_port) {
    log_warn(LD_CONFIG, "You specified a nonzero %sPort along with '%sPort 0' "
             "in the same configuration. Did you mean to disable %sPort or "
             "not?", portname, portname, portname);
    goto err;
  }

  retval = 0;
 err:
  SMARTLIST_FOREACH(elts, char *, cp, tor_free(cp));
  smartlist_free(elts);
  return retval;
}

/** Parse a list of config_line_t for an AF_UNIX unix socket listener option
 * from <b>cfg</b> and add them to <b>out</b>.  No fancy options are
 * supported: the line contains nothing but the path to the AF_UNIX socket. */
static int
parse_unix_socket_config(smartlist_t *out, const config_line_t *cfg,
                         int listener_type)
{

  if (!out)
    return 0;

  for ( ; cfg; cfg = cfg->next) {
    size_t len = strlen(cfg->value);
    port_cfg_t *port = tor_malloc_zero(sizeof(port_cfg_t) + len + 1);
    port->is_unix_addr = 1;
    memcpy(port->unix_addr, cfg->value, len+1);
    port->type = listener_type;
    smartlist_add(out, port);
  }

  return 0;
}

/** Return the number of ports which are actually going to listen with type
 * <b>listenertype</b>.  Do not count no_listen ports.  Do not count unix
 * sockets. */
static int
count_real_listeners(const smartlist_t *ports, int listenertype)
{
  int n = 0;
  SMARTLIST_FOREACH_BEGIN(ports, port_cfg_t *, port) {
    if (port->no_listen || port->is_unix_addr)
      continue;
    if (port->type != listenertype)
      continue;
    ++n;
  } SMARTLIST_FOREACH_END(port);
  return n;
}

/** Parse all client port types (Socks, DNS, Trans, NATD) from
 * <b>options</b>. On success, set *<b>n_ports_out</b> to the number
 * of ports that are listed, update the *Port_set values in
 * <b>options</b>, and return 0.  On failure, set *<b>msg</b> to a
 * description of the problem and return -1.
 *
 * If <b>validate_only</b> is false, set configured_client_ports to the
 * new list of ports parsed from <b>options</b>.
 **/
static int
parse_ports(or_options_t *options, int validate_only,
            char **msg, int *n_ports_out)
{
  smartlist_t *ports;
  int retval = -1;

  ports = smartlist_new();

  *n_ports_out = 0;

  if (parse_port_config(ports,
             options->SocksPort_lines, options->SocksListenAddress,
             "Socks", CONN_TYPE_AP_LISTENER,
             "127.0.0.1", 9050,
             CL_PORT_WARN_NONLOCAL|CL_PORT_ALLOW_EXTRA_LISTENADDR|
             CL_PORT_TAKES_HOSTNAMES) < 0) {
    *msg = tor_strdup("Invalid SocksPort/SocksListenAddress configuration");
    goto err;
  }
  if (parse_port_config(ports,
                        options->DNSPort_lines, options->DNSListenAddress,
                        "DNS", CONN_TYPE_AP_DNS_LISTENER,
                        "127.0.0.1", 0,
                        CL_PORT_WARN_NONLOCAL|CL_PORT_TAKES_HOSTNAMES) < 0) {
    *msg = tor_strdup("Invalid DNSPort/DNSListenAddress configuration");
    goto err;
  }
  if (parse_port_config(ports,
                        options->TransPort_lines, options->TransListenAddress,
                        "Trans", CONN_TYPE_AP_TRANS_LISTENER,
                        "127.0.0.1", 0,
                        CL_PORT_WARN_NONLOCAL) < 0) {
    *msg = tor_strdup("Invalid TransPort/TransListenAddress configuration");
    goto err;
  }
  if (parse_port_config(ports,
                        options->NATDPort_lines, options->NATDListenAddress,
                        "NATD", CONN_TYPE_AP_NATD_LISTENER,
                        "127.0.0.1", 0,
                        CL_PORT_WARN_NONLOCAL) < 0) {
    *msg = tor_strdup("Invalid NatdPort/NatdListenAddress configuration");
    goto err;
  }
  {
    unsigned control_port_flags = CL_PORT_NO_OPTIONS | CL_PORT_WARN_NONLOCAL;
    const int any_passwords = (options->HashedControlPassword ||
                               options->HashedControlSessionPassword ||
                               options->CookieAuthentication);
    if (! any_passwords)
      control_port_flags |= CL_PORT_FORBID_NONLOCAL;

    if (parse_port_config(ports,
                          options->ControlPort_lines,
                          options->ControlListenAddress,
                          "Control", CONN_TYPE_CONTROL_LISTENER,
                          "127.0.0.1", 0,
                          control_port_flags) < 0) {
      *msg = tor_strdup("Invalid ControlPort/ControlListenAddress "
                        "configuration");
      goto err;
    }
    if (parse_unix_socket_config(ports,
                                 options->ControlSocket,
                                 CONN_TYPE_CONTROL_LISTENER) < 0) {
      *msg = tor_strdup("Invalid ControlSocket configuration");
      goto err;
    }
  }
  if (! options->ClientOnly) {
    if (parse_port_config(ports,
                          options->ORPort_lines, options->ORListenAddress,
                          "OR", CONN_TYPE_OR_LISTENER,
                          "0.0.0.0", 0,
                          CL_PORT_SERVER_OPTIONS) < 0) {
      *msg = tor_strdup("Invalid ORPort/ORListenAddress configuration");
      goto err;
    }
    if (parse_port_config(ports,
                          options->ExtORPort_lines, NULL,
                          "ExtOR", CONN_TYPE_EXT_OR_LISTENER,
                          "127.0.0.1", 0,
                          CL_PORT_SERVER_OPTIONS|CL_PORT_WARN_NONLOCAL) < 0) {
      *msg = tor_strdup("Invalid ExtORPort configuration");
      goto err;
    }
    if (parse_port_config(ports,
                          options->DirPort_lines, options->DirListenAddress,
                          "Dir", CONN_TYPE_DIR_LISTENER,
                          "0.0.0.0", 0,
                          CL_PORT_SERVER_OPTIONS) < 0) {
      *msg = tor_strdup("Invalid DirPort/DirListenAddress configuration");
      goto err;
    }
  }

  if (check_server_ports(ports, options) < 0) {
    *msg = tor_strdup("Misconfigured server ports");
    goto err;
  }

  *n_ports_out = smartlist_len(ports);

  retval = 0;

  /* Update the *Port_set options.  The !! here is to force a boolean out of
     an integer. */
  options->ORPort_set =
    !! count_real_listeners(ports, CONN_TYPE_OR_LISTENER);
  options->SocksPort_set =
    !! count_real_listeners(ports, CONN_TYPE_AP_LISTENER);
  options->TransPort_set =
    !! count_real_listeners(ports, CONN_TYPE_AP_TRANS_LISTENER);
  options->NATDPort_set =
    !! count_real_listeners(ports, CONN_TYPE_AP_NATD_LISTENER);
  options->ControlPort_set =
    !! count_real_listeners(ports, CONN_TYPE_CONTROL_LISTENER);
  options->DirPort_set =
    !! count_real_listeners(ports, CONN_TYPE_DIR_LISTENER);
  options->DNSPort_set =
    !! count_real_listeners(ports, CONN_TYPE_AP_DNS_LISTENER);
  options->ExtORPort_set =
    !! count_real_listeners(ports, CONN_TYPE_EXT_OR_LISTENER);

  if (!validate_only) {
    if (configured_ports) {
      SMARTLIST_FOREACH(configured_ports,
                        port_cfg_t *, p, port_cfg_free(p));
      smartlist_free(configured_ports);
    }
    configured_ports = ports;
    ports = NULL; /* prevent free below. */
  }

 err:
  if (ports) {
    SMARTLIST_FOREACH(ports, port_cfg_t *, p, port_cfg_free(p));
    smartlist_free(ports);
  }
  return retval;
}

/** Given a list of <b>port_cfg_t</b> in <b>ports</b>, check them for internal
 * consistency and warn as appropriate. */
static int
check_server_ports(const smartlist_t *ports,
                   const or_options_t *options)
{
  int n_orport_advertised = 0;
  int n_orport_advertised_ipv4 = 0;
  int n_orport_listeners = 0;
  int n_dirport_advertised = 0;
  int n_dirport_listeners = 0;
  int n_low_port = 0;
  int r = 0;

  SMARTLIST_FOREACH_BEGIN(ports, const port_cfg_t *, port) {
    if (port->type == CONN_TYPE_DIR_LISTENER) {
      if (! port->no_advertise)
        ++n_dirport_advertised;
      if (! port->no_listen)
        ++n_dirport_listeners;
    } else if (port->type == CONN_TYPE_OR_LISTENER) {
      if (! port->no_advertise) {
        ++n_orport_advertised;
        if (tor_addr_family(&port->addr) == AF_INET ||
            (tor_addr_family(&port->addr) == AF_UNSPEC &&
                !port->bind_ipv6_only))
          ++n_orport_advertised_ipv4;
      }
      if (! port->no_listen)
        ++n_orport_listeners;
    } else {
      continue;
    }
#ifndef _WIN32
    if (!port->no_listen && port->port < 1024)
      ++n_low_port;
#endif
  } SMARTLIST_FOREACH_END(port);

  if (n_orport_advertised && !n_orport_listeners) {
    log_warn(LD_CONFIG, "We are advertising an ORPort, but not actually "
             "listening on one.");
    r = -1;
  }
  if (n_orport_listeners && !n_orport_advertised) {
    log_warn(LD_CONFIG, "We are listening on an ORPort, but not advertising "
             "any ORPorts. This will keep us from building a %s "
             "descriptor, and make us impossible to use.",
             options->BridgeRelay ? "bridge" : "router");
    r = -1;
  }
  if (n_dirport_advertised && !n_dirport_listeners) {
    log_warn(LD_CONFIG, "We are advertising a DirPort, but not actually "
             "listening on one.");
    r = -1;
  }
  if (n_dirport_advertised > 1) {
    log_warn(LD_CONFIG, "Can't advertise more than one DirPort.");
    r = -1;
  }
  if (n_orport_advertised && !n_orport_advertised_ipv4 &&
      !options->BridgeRelay) {
    log_warn(LD_CONFIG, "Configured non-bridge only to listen on an IPv6 "
             "address.");
    r = -1;
  }

  if (n_low_port && options->AccountingMax) {
    log_warn(LD_CONFIG,
          "You have set AccountingMax to use hibernation. You have also "
          "chosen a low DirPort or OrPort. This combination can make Tor stop "
          "working when it tries to re-attach the port after a period of "
          "hibernation. Please choose a different port or turn off "
          "hibernation unless you know this combination will work on your "
          "platform.");
  }

  return r;
}

/** Return a list of port_cfg_t for client ports parsed from the
 * options. */
const smartlist_t *
get_configured_ports(void)
{
  if (!configured_ports)
    configured_ports = smartlist_new();
  return configured_ports;
}

/** Return an address:port string representation of the address
 *  where the first <b>listener_type</b> listener waits for
 *  connections. Return NULL if we couldn't find a listener. The
 *  string is allocated on the heap and it's the responsibility of the
 *  caller to free it after use.
 *
 *  This function is meant to be used by the pluggable transport proxy
 *  spawning code, please make sure that it fits your purposes before
 *  using it. */
char *
get_first_listener_addrport_string(int listener_type)
{
  static const char *ipv4_localhost = "127.0.0.1";
  static const char *ipv6_localhost = "[::1]";
  const char *address;
  uint16_t port;
  char *string = NULL;

  if (!configured_ports)
    return NULL;

  SMARTLIST_FOREACH_BEGIN(configured_ports, const port_cfg_t *, cfg) {
    if (cfg->no_listen)
      continue;

    if (cfg->type == listener_type &&
        tor_addr_family(&cfg->addr) != AF_UNSPEC) {

      /* We found the first listener of the type we are interested in! */

      /* If a listener is listening on INADDR_ANY, assume that it's
         also listening on 127.0.0.1, and point the transport proxy
         there: */
      if (tor_addr_is_null(&cfg->addr))
        address = tor_addr_is_v4(&cfg->addr) ? ipv4_localhost : ipv6_localhost;
      else
        address = fmt_and_decorate_addr(&cfg->addr);

      /* If a listener is configured with port 'auto', we are forced
         to iterate all listener connections and find out in which
         port it ended up listening: */
      if (cfg->port == CFG_AUTO_PORT) {
        port = router_get_active_listener_port_by_type_af(listener_type,
                                                  tor_addr_family(&cfg->addr));
        if (!port)
          return NULL;
      } else {
        port = cfg->port;
      }

      tor_asprintf(&string, "%s:%u", address, port);

      return string;
    }

  } SMARTLIST_FOREACH_END(cfg);

  return NULL;
}

/** Return the first advertised port of type <b>listener_type</b> in
    <b>address_family</b>.  */
int
get_first_advertised_port_by_type_af(int listener_type, int address_family)
{
  if (!configured_ports)
    return 0;
  SMARTLIST_FOREACH_BEGIN(configured_ports, const port_cfg_t *, cfg) {
    if (cfg->type == listener_type &&
        !cfg->no_advertise &&
        (tor_addr_family(&cfg->addr) == address_family ||
         tor_addr_family(&cfg->addr) == AF_UNSPEC)) {
      if (tor_addr_family(&cfg->addr) != AF_UNSPEC ||
          (address_family == AF_INET && !cfg->bind_ipv6_only) ||
          (address_family == AF_INET6 && !cfg->bind_ipv4_only)) {
        return cfg->port;
      }
    }
  } SMARTLIST_FOREACH_END(cfg);
  return 0;
}

/** Adjust the value of options->DataDirectory, or fill it in if it's
 * absent. Return 0 on success, -1 on failure. */
static int
normalize_data_directory(or_options_t *options)
{
#ifdef _WIN32
  char *p;
  if (options->DataDirectory)
    return 0; /* all set */
  p = tor_malloc(MAX_PATH);
  strlcpy(p,get_windows_conf_root(),MAX_PATH);
  options->DataDirectory = p;
  return 0;
#else
  const char *d = options->DataDirectory;
  if (!d)
    d = "~/.tor";

 if (strncmp(d,"~/",2) == 0) {
   char *fn = expand_filename(d);
   if (!fn) {
     log_warn(LD_CONFIG,"Failed to expand filename \"%s\".", d);
     return -1;
   }
   if (!options->DataDirectory && !strcmp(fn,"/.tor")) {
     /* If our homedir is /, we probably don't want to use it. */
     /* Default to LOCALSTATEDIR/tor which is probably closer to what we
      * want. */
     log_warn(LD_CONFIG,
              "Default DataDirectory is \"~/.tor\".  This expands to "
              "\"%s\", which is probably not what you want.  Using "
              "\"%s"PATH_SEPARATOR"tor\" instead", fn, LOCALSTATEDIR);
     tor_free(fn);
     fn = tor_strdup(LOCALSTATEDIR PATH_SEPARATOR "tor");
   }
   tor_free(options->DataDirectory);
   options->DataDirectory = fn;
 }
 return 0;
#endif
}

/** Check and normalize the value of options->DataDirectory; return 0 if it
 * is sane, -1 otherwise. */
static int
validate_data_directory(or_options_t *options)
{
  if (normalize_data_directory(options) < 0)
    return -1;
  tor_assert(options->DataDirectory);
  if (strlen(options->DataDirectory) > (512-128)) {
    log_warn(LD_CONFIG, "DataDirectory is too long.");
    return -1;
  }
  return 0;
}

/** This string must remain the same forevermore. It is how we
 * recognize that the torrc file doesn't need to be backed up. */
#define GENERATED_FILE_PREFIX "# This file was generated by Tor; " \
  "if you edit it, comments will not be preserved"
/** This string can change; it tries to give the reader an idea
 * that editing this file by hand is not a good plan. */
#define GENERATED_FILE_COMMENT "# The old torrc file was renamed " \
  "to torrc.orig.1 or similar, and Tor will ignore it"

/** Save a configuration file for the configuration in <b>options</b>
 * into the file <b>fname</b>.  If the file already exists, and
 * doesn't begin with GENERATED_FILE_PREFIX, rename it.  Otherwise
 * replace it.  Return 0 on success, -1 on failure. */
static int
write_configuration_file(const char *fname, const or_options_t *options)
{
  char *old_val=NULL, *new_val=NULL, *new_conf=NULL;
  int rename_old = 0, r;

  tor_assert(fname);

  switch (file_status(fname)) {
    case FN_FILE:
      old_val = read_file_to_str(fname, 0, NULL);
      if (!old_val || strcmpstart(old_val, GENERATED_FILE_PREFIX)) {
        rename_old = 1;
      }
      tor_free(old_val);
      break;
    case FN_NOENT:
      break;
    case FN_ERROR:
    case FN_DIR:
    default:
      log_warn(LD_CONFIG,
               "Config file \"%s\" is not a file? Failing.", fname);
      return -1;
  }

  if (!(new_conf = options_dump(options, OPTIONS_DUMP_MINIMAL))) {
    log_warn(LD_BUG, "Couldn't get configuration string");
    goto err;
  }

  tor_asprintf(&new_val, "%s\n%s\n\n%s",
               GENERATED_FILE_PREFIX, GENERATED_FILE_COMMENT, new_conf);

  if (rename_old) {
    int i = 1;
    char *fn_tmp = NULL;
    while (1) {
      tor_asprintf(&fn_tmp, "%s.orig.%d", fname, i);
      if (file_status(fn_tmp) == FN_NOENT)
        break;
      tor_free(fn_tmp);
      ++i;
    }
    log_notice(LD_CONFIG, "Renaming old configuration file to \"%s\"", fn_tmp);
    if (tor_rename(fname, fn_tmp) < 0) {//XXXX sandbox doesn't allow
      log_warn(LD_FS,
               "Couldn't rename configuration file \"%s\" to \"%s\": %s",
               fname, fn_tmp, strerror(errno));
      tor_free(fn_tmp);
      goto err;
    }
    tor_free(fn_tmp);
  }

  if (write_str_to_file(fname, new_val, 0) < 0)
    goto err;

  r = 0;
  goto done;
 err:
  r = -1;
 done:
  tor_free(new_val);
  tor_free(new_conf);
  return r;
}

/**
 * Save the current configuration file value to disk.  Return 0 on
 * success, -1 on failure.
 **/
int
options_save_current(void)
{
  /* This fails if we can't write to our configuration file.
   *
   * If we try falling back to datadirectory or something, we have a better
   * chance of saving the configuration, but a better chance of doing
   * something the user never expected. */
  return write_configuration_file(get_torrc_fname(0), get_options());
}

/** Return the number of cpus configured in <b>options</b>.  If we are
 * told to auto-detect the number of cpus, return the auto-detected number. */
int
get_num_cpus(const or_options_t *options)
{
  if (options->NumCPUs == 0) {
    int n = compute_num_cpus();
    return (n >= 1) ? n : 1;
  } else {
    return options->NumCPUs;
  }
}

/**
 * Initialize the libevent library.
 */
static void
init_libevent(const or_options_t *options)
{
  const char *badness=NULL;
  tor_libevent_cfg cfg;

  tor_assert(options);

  configure_libevent_logging();
  /* If the kernel complains that some method (say, epoll) doesn't
   * exist, we don't care about it, since libevent will cope.
   */
  suppress_libevent_log_msg("Function not implemented");

  tor_check_libevent_header_compatibility();

  memset(&cfg, 0, sizeof(cfg));
  cfg.disable_iocp = options->DisableIOCP;
  cfg.num_cpus = get_num_cpus(options);
  cfg.msec_per_tick = options->TokenBucketRefillInterval;

  tor_libevent_initialize(&cfg);

  suppress_libevent_log_msg(NULL);

  tor_check_libevent_version(tor_libevent_get_method(),
                             server_mode(get_options()),
                             &badness);
  if (badness) {
    const char *v = tor_libevent_get_version_str();
    const char *m = tor_libevent_get_method();
    control_event_general_status(LOG_WARN,
        "BAD_LIBEVENT VERSION=%s METHOD=%s BADNESS=%s RECOVERED=NO",
                                 v, m, badness);
  }
}

/** Return a newly allocated string holding a filename relative to the data
 * directory.  If <b>sub1</b> is present, it is the first path component after
 * the data directory.  If <b>sub2</b> is also present, it is the second path
 * component after the data directory.  If <b>suffix</b> is present, it
 * is appended to the filename.
 *
 * Examples:
 *    get_datadir_fname2_suffix("a", NULL, NULL) -> $DATADIR/a
 *    get_datadir_fname2_suffix("a", NULL, ".tmp") -> $DATADIR/a.tmp
 *    get_datadir_fname2_suffix("a", "b", ".tmp") -> $DATADIR/a/b/.tmp
 *    get_datadir_fname2_suffix("a", "b", NULL) -> $DATADIR/a/b
 *
 * Note: Consider using the get_datadir_fname* macros in or.h.
 */
char *
options_get_datadir_fname2_suffix(const or_options_t *options,
                                  const char *sub1, const char *sub2,
                                  const char *suffix)
{
  char *fname = NULL;
  size_t len;
  tor_assert(options);
  tor_assert(options->DataDirectory);
  tor_assert(sub1 || !sub2); /* If sub2 is present, sub1 must be present. */
  len = strlen(options->DataDirectory);
  if (sub1) {
    len += strlen(sub1)+1;
    if (sub2)
      len += strlen(sub2)+1;
  }
  if (suffix)
    len += strlen(suffix);
  len++;
  fname = tor_malloc(len);
  if (sub1) {
    if (sub2) {
      tor_snprintf(fname, len, "%s"PATH_SEPARATOR"%s"PATH_SEPARATOR"%s",
                   options->DataDirectory, sub1, sub2);
    } else {
      tor_snprintf(fname, len, "%s"PATH_SEPARATOR"%s",
                   options->DataDirectory, sub1);
    }
  } else {
    strlcpy(fname, options->DataDirectory, len);
  }
  if (suffix)
    strlcat(fname, suffix, len);
  return fname;
}

/** Check wether the data directory has a private subdirectory
 * <b>subdir</b>. If not, try to create it. Return 0 on success,
 * -1 otherwise. */
int
check_or_create_data_subdir(const char *subdir)
{
  char *statsdir = get_datadir_fname(subdir);
  int return_val = 0;

  if (check_private_dir(statsdir, CPD_CREATE, get_options()->User) < 0) {
    log_warn(LD_HIST, "Unable to create %s/ directory!", subdir);
    return_val = -1;
  }
  tor_free(statsdir);
  return return_val;
}

/** Create a file named <b>fname</b> with contents <b>str</b> in the
 * subdirectory <b>subdir</b> of the data directory. <b>descr</b>
 * should be a short description of the file's content and will be
 * used for the warning message, if it's present and the write process
 * fails. Return 0 on success, -1 otherwise.*/
int
write_to_data_subdir(const char* subdir, const char* fname,
                     const char* str, const char* descr)
{
  char *filename = get_datadir_fname2(subdir, fname);
  int return_val = 0;

  if (write_str_to_file(filename, str, 0) < 0) {
    log_warn(LD_HIST, "Unable to write %s to disk!", descr ? descr : fname);
    return_val = -1;
  }
  tor_free(filename);
  return return_val;
}

/** Given a file name check to see whether the file exists but has not been
 * modified for a very long time.  If so, remove it. */
void
remove_file_if_very_old(const char *fname, time_t now)
{
#define VERY_OLD_FILE_AGE (28*24*60*60)
  struct stat st;

  log_debug(LD_FS, "stat()ing %s", fname);
  if (stat(sandbox_intern_string(fname), &st)==0 &&
      st.st_mtime < now-VERY_OLD_FILE_AGE) {
    char buf[ISO_TIME_LEN+1];
    format_local_iso_time(buf, st.st_mtime);
    log_notice(LD_GENERAL, "Obsolete file %s hasn't been modified since %s. "
               "Removing it.", fname, buf);
    if (unlink(fname) != 0) {
      log_warn(LD_FS, "Failed to unlink %s: %s",
               fname, strerror(errno));
    }
  }
}

/** Return a smartlist of ports that must be forwarded by
 *  tor-fw-helper. The smartlist contains the ports in a string format
 *  that is understandable by tor-fw-helper. */
smartlist_t *
get_list_of_ports_to_forward(void)
{
  smartlist_t *ports_to_forward = smartlist_new();
  int port = 0;

  /** XXX TODO tor-fw-helper does not support forwarding ports to
      other hosts than the local one. If the user is binding to a
      different IP address, tor-fw-helper won't work.  */
  port = router_get_advertised_or_port(get_options());  /* Get ORPort */
  if (port)
    smartlist_add_asprintf(ports_to_forward, "%d:%d", port, port);

  port = router_get_advertised_dir_port(get_options(), 0); /* Get DirPort */
  if (port)
    smartlist_add_asprintf(ports_to_forward, "%d:%d", port, port);

  /* Get ports of transport proxies */
  {
    smartlist_t *transport_ports = get_transport_proxy_ports();
    if (transport_ports) {
      smartlist_add_all(ports_to_forward, transport_ports);
      smartlist_free(transport_ports);
    }
  }

  if (!smartlist_len(ports_to_forward)) {
    smartlist_free(ports_to_forward);
    ports_to_forward = NULL;
  }

  return ports_to_forward;
}

/** Helper to implement GETINFO functions about configuration variables (not
 * their values).  Given a "config/names" question, set *<b>answer</b> to a
 * new string describing the supported configuration variables and their
 * types. */
int
getinfo_helper_config(control_connection_t *conn,
                      const char *question, char **answer,
                      const char **errmsg)
{
  (void) conn;
  (void) errmsg;
  if (!strcmp(question, "config/names")) {
    smartlist_t *sl = smartlist_new();
    int i;
    for (i = 0; option_vars_[i].name; ++i) {
      const config_var_t *var = &option_vars_[i];
      const char *type;
      /* don't tell controller about triple-underscore options */
      if (!strncmp(option_vars_[i].name, "___", 3))
        continue;
      switch (var->type) {
        case CONFIG_TYPE_STRING: type = "String"; break;
        case CONFIG_TYPE_FILENAME: type = "Filename"; break;
        case CONFIG_TYPE_UINT: type = "Integer"; break;
        case CONFIG_TYPE_INT: type = "SignedInteger"; break;
        case CONFIG_TYPE_PORT: type = "Port"; break;
        case CONFIG_TYPE_INTERVAL: type = "TimeInterval"; break;
        case CONFIG_TYPE_MSEC_INTERVAL: type = "TimeMsecInterval"; break;
        case CONFIG_TYPE_MEMUNIT: type = "DataSize"; break;
        case CONFIG_TYPE_DOUBLE: type = "Float"; break;
        case CONFIG_TYPE_BOOL: type = "Boolean"; break;
        case CONFIG_TYPE_AUTOBOOL: type = "Boolean+Auto"; break;
        case CONFIG_TYPE_ISOTIME: type = "Time"; break;
        case CONFIG_TYPE_ROUTERSET: type = "RouterList"; break;
        case CONFIG_TYPE_CSV: type = "CommaList"; break;
        case CONFIG_TYPE_CSV_INTERVAL: type = "TimeIntervalCommaList"; break;
        case CONFIG_TYPE_LINELIST: type = "LineList"; break;
        case CONFIG_TYPE_LINELIST_S: type = "Dependant"; break;
        case CONFIG_TYPE_LINELIST_V: type = "Virtual"; break;
        default:
        case CONFIG_TYPE_OBSOLETE:
          type = NULL; break;
      }
      if (!type)
        continue;
      smartlist_add_asprintf(sl, "%s %s\n",var->name,type);
    }
    *answer = smartlist_join_strings(sl, "", 0, NULL);
    SMARTLIST_FOREACH(sl, char *, c, tor_free(c));
    smartlist_free(sl);
  } else if (!strcmp(question, "config/defaults")) {
    smartlist_t *sl = smartlist_new();
    int i;
    for (i = 0; option_vars_[i].name; ++i) {
      const config_var_t *var = &option_vars_[i];
      if (var->initvalue != NULL) {
          char *val = esc_for_log(var->initvalue);
          smartlist_add_asprintf(sl, "%s %s\n",var->name,val);
          tor_free(val);
      }
    }
    *answer = smartlist_join_strings(sl, "", 0, NULL);
    SMARTLIST_FOREACH(sl, char *, c, tor_free(c));
    smartlist_free(sl);
  }
  return 0;
}

/** Parse outbound bind address option lines. If <b>validate_only</b>
 * is not 0 update OutboundBindAddressIPv4_ and
 * OutboundBindAddressIPv6_ in <b>options</b>. On failure, set
 * <b>msg</b> (if provided) to a newly allocated string containing a
 * description of the problem and return -1. */
static int
parse_outbound_addresses(or_options_t *options, int validate_only, char **msg)
{
  const config_line_t *lines = options->OutboundBindAddress;
  int found_v4 = 0, found_v6 = 0;

  if (!validate_only) {
    memset(&options->OutboundBindAddressIPv4_, 0,
           sizeof(options->OutboundBindAddressIPv4_));
    memset(&options->OutboundBindAddressIPv6_, 0,
           sizeof(options->OutboundBindAddressIPv6_));
  }
  while (lines) {
    tor_addr_t addr, *dst_addr = NULL;
    int af = tor_addr_parse(&addr, lines->value);
    switch (af) {
    case AF_INET:
      if (found_v4) {
        if (msg)
          tor_asprintf(msg, "Multiple IPv4 outbound bind addresses "
                       "configured: %s", lines->value);
        return -1;
      }
      found_v4 = 1;
      dst_addr = &options->OutboundBindAddressIPv4_;
      break;
    case AF_INET6:
      if (found_v6) {
        if (msg)
          tor_asprintf(msg, "Multiple IPv6 outbound bind addresses "
                       "configured: %s", lines->value);
        return -1;
      }
      found_v6 = 1;
      dst_addr = &options->OutboundBindAddressIPv6_;
      break;
    default:
      if (msg)
        tor_asprintf(msg, "Outbound bind address '%s' didn't parse.",
                     lines->value);
      return -1;
    }
    if (!validate_only)
      tor_addr_copy(dst_addr, &addr);
    lines = lines->next;
  }
  return 0;
}

/** Load one of the geoip files, <a>family</a> determining which
 * one. <a>default_fname</a> is used if on Windows and
 * <a>fname</a> equals "<default>". */
static void
config_load_geoip_file_(sa_family_t family,
                        const char *fname,
                        const char *default_fname)
{
#ifdef _WIN32
  char *free_fname = NULL; /* Used to hold any temporary-allocated value */
  /* XXXX Don't use this "<default>" junk; make our filename options
   * understand prefixes somehow. -NM */
  if (!strcmp(fname, "<default>")) {
    const char *conf_root = get_windows_conf_root();
    tor_asprintf(&free_fname, "%s\\%s", conf_root, default_fname);
    fname = free_fname;
  }
  geoip_load_file(family, fname);
  tor_free(free_fname);
#else
  (void)default_fname;
  geoip_load_file(family, fname);
#endif
}

/** Load geoip files for IPv4 and IPv6 if <a>options</a> and
 * <a>old_options</a> indicate we should. */
static void
config_maybe_load_geoip_files_(const or_options_t *options,
                               const or_options_t *old_options)
{
  /* XXXX024 Reload GeoIPFile on SIGHUP. -NM */

  if (options->GeoIPFile &&
      ((!old_options || !opt_streq(old_options->GeoIPFile,
                                   options->GeoIPFile))
       || !geoip_is_loaded(AF_INET)))
    config_load_geoip_file_(AF_INET, options->GeoIPFile, "geoip");
  if (options->GeoIPv6File &&
      ((!old_options || !opt_streq(old_options->GeoIPv6File,
                                   options->GeoIPv6File))
       || !geoip_is_loaded(AF_INET6)))
    config_load_geoip_file_(AF_INET6, options->GeoIPv6File, "geoip6");
}

/** Initialize cookie authentication (used so far by the ControlPort
 *  and Extended ORPort).
 *
 *  Allocate memory and create a cookie (of length <b>cookie_len</b>)
 *  in <b>cookie_out</b>.
 *  Then write it down to <b>fname</b> and prepend it with <b>header</b>.
 *
 *  If <b>group_readable</b> is set, set <b>fname</b> to be readable
 *  by the default GID.
 *
 *  If the whole procedure was successful, set
 *  <b>cookie_is_set_out</b> to True. */
int
init_cookie_authentication(const char *fname, const char *header,
                           int cookie_len, int group_readable,
                           uint8_t **cookie_out, int *cookie_is_set_out)
{
  char cookie_file_str_len = strlen(header) + cookie_len;
  char *cookie_file_str = tor_malloc(cookie_file_str_len);
  int retval = -1;

  /* We don't want to generate a new cookie every time we call
   * options_act(). One should be enough. */
  if (*cookie_is_set_out) {
    retval = 0; /* we are all set */
    goto done;
  }

  /* If we've already set the cookie, free it before re-setting
     it. This can happen if we previously generated a cookie, but
     couldn't write it to a disk. */
  if (*cookie_out)
    tor_free(*cookie_out);

  /* Generate the cookie */
  *cookie_out = tor_malloc(cookie_len);
  if (crypto_rand((char *)*cookie_out, cookie_len) < 0)
    goto done;

  /* Create the string that should be written on the file. */
  memcpy(cookie_file_str, header, strlen(header));
  memcpy(cookie_file_str+strlen(header), *cookie_out, cookie_len);
  if (write_bytes_to_file(fname, cookie_file_str, cookie_file_str_len, 1)) {
    log_warn(LD_FS,"Error writing auth cookie to %s.", escaped(fname));
    goto done;
  }

#ifndef _WIN32
  if (group_readable) {
    if (chmod(fname, 0640)) {
      log_warn(LD_FS,"Unable to make %s group-readable.", escaped(fname));
    }
  }
#else
  (void) group_readable;
#endif

  /* Success! */
  log_info(LD_GENERAL, "Generated auth cookie file in '%s'.", escaped(fname));
  *cookie_is_set_out = 1;
  retval = 0;

 done:
  memwipe(cookie_file_str, 0, cookie_file_str_len);
  tor_free(cookie_file_str);
  return retval;
}


/*
 *  snmp_ospf.h
 *
 */
#ifndef _MIBGROUP_SNMP_OSPF_H
#define _MIBGROUP_SNMP_OSPF_H

config_require(smux/smux)

     extern FindVarMethod var_ospf;
     extern void     init_snmp_ospf(void);


#define ospfRouterId		0
#define ospfAdminStat		1
#define ospfVersionNumber		2
#define ospfAreaBdrRtrStatus		3
#define ospfASBdrRtrStatus		4
#define ospfExternLsaCount		5
#define ospfExternLsaCksumSum		6
#define ospfTOSSupport		7
#define ospfOriginateNewLsas		8
#define ospfRxNewLsas		9
#define ospfExtLsdbLimit		10
#define ospfMulticastExtensions		11
#define ospfAreaId		12
#define ospfAuthType		13
#define ospfImportAsExtern		14
#define ospfSpfRuns		15
#define ospfAreaBdrRtrCount		16
#define ospfAsBdrRtrCount		17
#define ospfAreaLsaCount		18
#define ospfAreaLsaCksumSum		19
#define ospfAreaSummary		20
#define ospfAreaStatus		21
#define ospfStubAreaId		22
#define ospfStubTOS		23
#define ospfStubMetric		24
#define ospfStubStatus		25
#define ospfStubMetricType		26
#define ospfLsdbAreaId		27
#define ospfLsdbType		28
#define ospfLsdbLsid		29
#define ospfLsdbRouterId		30
#define ospfLsdbSequence		31
#define ospfLsdbAge		32
#define ospfLsdbChecksum		33
#define ospfLsdbAdvertisement		34
#define ospfAreaRangeAreaId		35
#define ospfAreaRangeNet		36
#define ospfAreaRangeMask		37
#define ospfAreaRangeStatus		38
#define ospfAreaRangeEffect		39
#define ospfHostIpAddress		40
#define ospfHostTOS		41
#define ospfHostMetric		42
#define ospfHostStatus		43
#define ospfHostAreaID		44
#define ospfIfIpAddress		45
#define ospfAddressLessIf		46
#define ospfIfAreaId		47
#define ospfIfType		48
#define ospfIfAdminStat		49
#define ospfIfRtrPriority		50
#define ospfIfTransitDelay		51
#define ospfIfRetransInterval		52
#define ospfIfHelloInterval		53
#define ospfIfRtrDeadInterval		54
#define ospfIfPollInterval		55
#define ospfIfState		56
#define ospfIfDesignatedRouter		57
#define ospfIfBackupDesignatedRouter		58
#define ospfIfEvents		59
#define ospfIfAuthKey		60
#define ospfIfStatus		61
#define ospfIfMulticastForwarding		62
#define ospfIfMetricIpAddress		63
#define ospfIfMetricAddressLessIf		64
#define ospfIfMetricTOS		65
#define ospfIfMetricValue		66
#define ospfIfMetricStatus		67
#define ospfVirtIfAreaId		68
#define ospfVirtIfNeighbor		69
#define ospfVirtIfTransitDelay		70
#define ospfVirtIfRetransInterval		71
#define ospfVirtIfHelloInterval		72
#define ospfVirtIfRtrDeadInterval		73
#define ospfVirtIfState		74
#define ospfVirtIfEvents		75
#define ospfVirtIfAuthKey		76
#define ospfVirtIfStatus		77
#define ospfNbrIpAddr		78
#define ospfNbrAddressLessIndex		79
#define ospfNbrRtrId		80
#define ospfNbrOptions		81
#define ospfNbrPriority		82
#define ospfNbrState		83
#define ospfNbrEvents		84
#define ospfNbrLsRetransQLen		85
#define ospfNbmaNbrStatus		86
#define ospfNbmaNbrPermanence		87
#define ospfVirtNbrArea		88
#define ospfVirtNbrRtrId		89
#define ospfVirtNbrIpAddr		90
#define ospfVirtNbrOptions		91
#define ospfVirtNbrState		92
#define ospfVirtNbrEvents		93
#define ospfVirtNbrLsRetransQLen		94
#define ospfExtLsdbType		95
#define ospfExtLsdbLsid		96
#define ospfExtLsdbRouterId		97
#define ospfExtLsdbSequence		98
#define ospfExtLsdbAge		99
#define ospfExtLsdbChecksum		100
#define ospfExtLsdbAdvertisement		101
#define ospfAreaAggregateAreaID		102
#define ospfAreaAggregateLsdbType		103
#define ospfAreaAggregateNet		104
#define ospfAreaAggregateMask		105
#define ospfAreaAggregateStatus		106
#define ospfAreaAggregateEffect		107

#endif                          /* _MIBGROUP_SNMP_OSPF_H */

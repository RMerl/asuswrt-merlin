#!/bin/sh 
#
#     Test for RFC-1213MIB variable
#
#

HEADER RFC-1213 MIB SNMPv3 access

iclist="ic1 "	        # list invocable components

ic1="tp1 tp2 tp3 tp4 tp5 tp6 tp7 tp8 tp9 tp10 tp11 tp12 tp13 tp14 tp15 tp16 tp17 tp18 tp19 tp20 tp21 tp22 tp23 tp24 tp25 tp26 tp27 tp28 tp29 tp30 tp31 tp32 tp33 tp34 tp35 tp36 tp37 tp38 tp39 tp40 tp41 tp42 tp43 tp44 tp45 tp46 tp47 tp48 tp49 tp50 tp51 tp52 tp53 tp54 tp55 tp56 tp57 tp58 tp59 tp60 tp61 tp62 tp63 tp64 tp65 tp66 tp67 tp68 tp69 tp70 tp71 tp72 tp73 tp74 tp75 tp76 tp77 tp78 tp79 tp80 tp81 tp82 tp83 tp84 tp85 tp86 tp87 tp88 tp89 tp90 tp91 tp92 tp93 tp94 tp95 tp96 tp97 tp98 tp99 tp100 tp101 tp102 tp103 tp104 tp105 tp106 tp107 tp108" 

configv3

tp1()
{
	get_snmpv3_variable 3 system.sysDescr
}
tp2()
{
	get_snmpv3_variable 3 system.sysObjectID
}
tp3()
{
	get_snmpv3_variable 3 system.sysUpTime
}
tp4()
{
	get_snmpv3_variable 3 system.sysContact
}
tp5()
{
	get_snmpv3_variable 3 system.sysName
}
tp6()
{
	get_snmpv3_variable 3 system.sysLocation
}
tp7()
{
	get_snmpv3_variable 3 interfaces.ifNumber
}
tp8()
{
	get_snmpv3_table 3 interfaces.ifTable
}
tp9()
{
	get_snmpv3_table 3 at.atTable
}
tp10()
{
	get_snmpv3_variable 3 ip.ipInReceives
}
tp11()
{
	get_snmpv3_variable 3 ip.ipForwarding
}
tp12()
{
	get_snmpv3_variable 3 ip.ipDefaultTTL
}
tp13()
{
	get_snmpv3_variable 3 ip.ipInReceives
}
tp14()
{
	get_snmpv3_variable 3 ip.ipInHdrErrors
}
tp15()
{
	get_snmpv3_variable 3 ip.ipInAddrErrors
}
tp16()
{
	get_snmpv3_variable 3 ip.ipForwDatagrams
}
tp17()
{
	get_snmpv3_variable 3 ip.ipInUnknownProtos
}
tp18()
{
	get_snmpv3_variable 3 ip.ipInDiscards
}
tp19()
{
	get_snmpv3_variable 3 ip.ipInDelivers
}
tp20()
{
	get_snmpv3_variable 3 ip.ipOutRequests
}
tp21()
{
	get_snmpv3_variable 3 ip.ipOutDiscards
}
tp22()
{
	get_snmpv3_variable 3 ip.ipOutNoRoutes
}
tp23()
{
	get_snmpv3_variable 3 ip.ipReasmTimeout
}
tp24()
{
	get_snmpv3_variable 3 ip.ipReasmReqds
}
tp25()
{
	get_snmpv3_variable 3 ip.ipReasmOKs
}
tp26()
{
	get_snmpv3_variable 3 ip.ipReasmFails
}
tp27()
{
	get_snmpv3_variable 3 ip.ipFragOKs
}
tp28()
{
	get_snmpv3_variable 3 ip.ipFragFails
}
tp29()
{
	get_snmpv3_variable 3 ip.ipFragCreates
}
tp30()
{
	get_snmpv3_variable 3 ip.ipRoutingDiscards
}
tp31()
{
	get_snmpv3_table 3 ip.ipAddrTable
}
tp32()
{
	get_snmpv3_table 3 ip.ipRouteTable
}
tp33()
{
	get_snmpv3_table 3 ip.ipNetToMediaTable
}
tp34()
{
	get_snmpv3_variable 3 icmp.icmpInMsgs
}
tp35()
{
	get_snmpv3_variable 3 icmp.icmpInErrors
}
tp36()
{
	get_snmpv3_variable 3 icmp.icmpInDestUnreachs
}
tp37()
{
	get_snmpv3_variable 3 icmp.icmpInTimeExcds
}
tp38()
{
	get_snmpv3_variable 3 icmp.icmpInParmProbs
}
tp39()
{
	get_snmpv3_variable 3 icmp.icmpInSrcQuenchs
}
tp40()
{
	get_snmpv3_variable 3 icmp.icmpInRedirects
}
tp41()
{
	get_snmpv3_variable 3 icmp.icmpInEchos
}
tp42()
{
	get_snmpv3_variable 3 icmp.icmpInEchoReps
}
tp43()
{
	get_snmpv3_variable 3 icmp.icmpInTimestamps
}
tp44()
{
	get_snmpv3_variable 3 icmp.icmpInTimestampReps
}
tp45()
{
	get_snmpv3_variable 3 icmp.icmpInAddrMasks
}
tp46()
{
	get_snmpv3_variable 3 icmp.icmpInAddrMaskReps
}
tp47()
{
	get_snmpv3_variable 3 icmp.icmpOutMsgs
}
tp48()
{
	get_snmpv3_variable 3 icmp.icmpOutErrors
}
tp49()
{
	get_snmpv3_variable 3 icmp.icmpOutDestUnreachs
}
tp50()
{
	get_snmpv3_variable 3 icmp.icmpOutTimeExcds
}
tp51()
{
	get_snmpv3_variable 3 icmp.icmpOutParmProbs
}
tp52()
{
	get_snmpv3_variable 3 icmp.icmpOutSrcQuenchs
}
tp53()
{
	get_snmpv3_variable 3 icmp.icmpOutRedirects
}
tp54()
{
	get_snmpv3_variable 3 icmp.icmpOutEchos
}
tp55()
{
	get_snmpv3_variable 3 icmp.icmpOutEchoReps
}
tp56()
{
	get_snmpv3_variable 3 icmp.icmpOutTimestamps
}
tp57()
{
	get_snmpv3_variable 3 icmp.icmpOutTimestampReps
}
tp58()
{
	get_snmpv3_variable 3 icmp.icmpOutAddrMasks
}
tp59()
{
	get_snmpv3_variable 3 icmp.icmpOutAddrMaskReps
}
tp60()
{
	get_snmpv3_variable 3 tcp.tcpActiveOpens
}
tp61()
{
	get_snmpv3_variable 3 tcp.tcpRtoAlgorithm
}
tp62()
{
	get_snmpv3_variable 3 tcp.tcpRtoMin
}
tp63()
{
	get_snmpv3_variable 3 tcp.tcpRtoMax
}
tp64()
{
	get_snmpv3_variable 3 tcp.tcpMaxConn
}
tp65()
{
	get_snmpv3_variable 3 tcp.tcpActiveOpens
}
tp66()
{
	get_snmpv3_variable 3 tcp.tcpPassiveOpens
}
tp67()
{
	get_snmpv3_variable 3 tcp.tcpAttemptFails
}
tp68()
{
	get_snmpv3_variable 3 tcp.tcpEstabResets
}
tp69()
{
	get_snmpv3_variable 3 tcp.tcpCurrEstab
}
tp70()
{
	get_snmpv3_variable 3 tcp.tcpInSegs
}
tp71()
{
	get_snmpv3_variable 3 tcp.tcpOutSegs
}
tp72()
{
	get_snmpv3_variable 3 tcp.tcpRetransSegs
}
tp73()
{
	get_snmpv3_variable 3 tcp.tcpInErrs
}
tp74()
{
	get_snmpv3_variable 3 tcp.tcpOutRsts
}
tp75()
{
	get_snmpv3_table 3 tcp.tcpConnTable
}
tp76()
{
	get_snmpv3_variable 3 udp.udpInDatagrams
}
tp77()
{
	get_snmpv3_variable 3 udp.udpNoPorts
}
tp78()
{
	get_snmpv3_variable 3 udp.udpInErrors
}
tp79()
{
	get_snmpv3_variable 3 udp.udpOutDatagrams
}
tp80()
{
	get_snmpv3_table 3 udp.udpTable
}
tp81()
{
	get_snmpv3_variable 3 snmp.snmpInPkts
}
tp82()
{
	get_snmpv3_variable 3 snmp.snmpOutPkts
}
tp83()
{
	get_snmpv3_variable 3 snmp.snmpInBadVersions
}
tp84()
{
	get_snmpv3_variable 3 snmp.snmpInBadCommunityNames
}
tp85()
{
	get_snmpv3_variable 3 snmp.snmpInBadCommunityUses
}
tp86()
{
	get_snmpv3_variable 3 snmp.snmpInASNParseErrs
}
tp87()
{
	get_snmpv3_variable 3 snmp.snmpInTooBigs
}
tp88()
{
	get_snmpv3_variable 3 snmp.snmpInNoSuchNames
}
tp89()
{
	get_snmpv3_variable 3 snmp.snmpInBadValues
}
tp90()
{
	get_snmpv3_variable 3 snmp.snmpInReadOnlys
}
tp91()
{
	get_snmpv3_variable 3 snmp.snmpInGenErrs
}
tp92()
{
	get_snmpv3_variable 3 snmp.snmpInTotalReqVars
}
tp93()
{
	get_snmpv3_variable 3 snmp.snmpInTotalSetVars
}
tp94()
{
	get_snmpv3_variable 3 snmp.snmpInGetRequests
}
tp95()
{
	get_snmpv3_variable 3 snmp.snmpInGetNexts
}
tp96()
{
	get_snmpv3_variable 3 snmp.snmpInSetRequests
}
tp97()
{
	get_snmpv3_variable 3 snmp.snmpInGetResponses
}
tp98()
{
	get_snmpv3_variable 3 snmp.snmpInTraps
}
tp99()
{
	get_snmpv3_variable 3 snmp.snmpOutTooBigs
}
tp100()
{
	get_snmpv3_variable 3 snmp.snmpOutNoSuchNames
}
tp101()
{
	get_snmpv3_variable 3 snmp.snmpOutBadValues
}
tp102()
{
	get_snmpv3_variable 3 snmp.snmpOutGenErrs
}
tp103()
{
	get_snmpv3_variable 3 snmp.snmpOutGetRequests
}
tp104()
{
	get_snmpv3_variable 3 snmp.snmpOutGetNexts
}
tp105()
{
	get_snmpv3_variable 3 snmp.snmpOutSetRequests
}
tp106()
{
	get_snmpv3_variable 3 snmp.snmpOutGetResponses
}
tp107()
{
	get_snmpv3_variable 3 snmp.snmpOutTraps
}
tp108()
{
	get_snmpv3_variable 3 snmp.snmpEnableAuthenTraps
}



#!/bin/sh 
#
#     Test for RFC-1213MIB variable
#
#

HEADER RFC-1213 MIB SNMPv2c access

iclist="ic1 "	        # list invocable components

ic1="tp1 tp2 tp3 tp4 tp5 tp6 tp7 tp8 tp9 tp10 tp11 tp12 tp13 tp14 tp15 tp16 tp17 tp18 tp19 tp20 tp21 tp22 tp23 tp24 tp25 tp26 tp27 tp28 tp29 tp30 tp31 tp32 tp33 tp34 tp35 tp36 tp37 tp38 tp39 tp40 tp41 tp42 tp43 tp44 tp45 tp46 tp47 tp48 tp49 tp50 tp51 tp52 tp53 tp54 tp55 tp56 tp57 tp58 tp59 tp60 tp61 tp62 tp63 tp64 tp65 tp66 tp67 tp68 tp69 tp70 tp71 tp72 tp73 tp74 tp75 tp76 tp77 tp78 tp79 tp80 tp81 tp82 tp83 tp84 tp85 tp86 tp87 tp88 tp89 tp90 tp91 tp92 tp93 tp94 tp95 tp96 tp97 tp98 tp99 tp100 tp101 tp102 tp103 tp104 tp105 tp106 tp107 tp108" 

config

tp1()
{
	get_snmp_variable 2c system.sysDescr
}
tp2()
{
	get_snmp_variable 2c system.sysObjectID
}
tp3()
{
	get_snmp_variable 2c system.sysUpTime
}
tp4()
{
	get_snmp_variable 2c system.sysContact
}
tp5()
{
	get_snmp_variable 2c system.sysName
}
tp6()
{
	get_snmp_variable 2c system.sysLocation
}
tp7()
{
	get_snmp_variable 2c interfaces.ifNumber
}
tp8()
{
	get_snmp_table 2c interfaces.ifTable
}
tp9()
{
	get_snmp_table 2c at.atTable
}
tp10()
{
	get_snmp_variable 2c ip.ipInReceives
}
tp11()
{
	get_snmp_variable 2c ip.ipForwarding
}
tp12()
{
	get_snmp_variable 2c ip.ipDefaultTTL
}
tp13()
{
	get_snmp_variable 2c ip.ipInReceives
}
tp14()
{
	get_snmp_variable 2c ip.ipInHdrErrors
}
tp15()
{
	get_snmp_variable 2c ip.ipInAddrErrors
}
tp16()
{
	get_snmp_variable 2c ip.ipForwDatagrams
}
tp17()
{
	get_snmp_variable 2c ip.ipInUnknownProtos
}
tp18()
{
	get_snmp_variable 2c ip.ipInDiscards
}
tp19()
{
	get_snmp_variable 2c ip.ipInDelivers
}
tp20()
{
	get_snmp_variable 2c ip.ipOutRequests
}
tp21()
{
	get_snmp_variable 2c ip.ipOutDiscards
}
tp22()
{
	get_snmp_variable 2c ip.ipOutNoRoutes
}
tp23()
{
	get_snmp_variable 2c ip.ipReasmTimeout
}
tp24()
{
	get_snmp_variable 2c ip.ipReasmReqds
}
tp25()
{
	get_snmp_variable 2c ip.ipReasmOKs
}
tp26()
{
	get_snmp_variable 2c ip.ipReasmFails
}
tp27()
{
	get_snmp_variable 2c ip.ipFragOKs
}
tp28()
{
	get_snmp_variable 2c ip.ipFragFails
}
tp29()
{
	get_snmp_variable 2c ip.ipFragCreates
}
tp30()
{
	get_snmp_variable 2c ip.ipRoutingDiscards
}
tp31()
{
	get_snmp_table 2c ip.ipAddrTable
}
tp32()
{
	get_snmp_table 2c ip.ipRouteTable
}
tp33()
{
	get_snmp_table 2c ip.ipNetToMediaTable
}
tp34()
{
	get_snmp_variable 2c icmp.icmpInMsgs
}
tp35()
{
	get_snmp_variable 2c icmp.icmpInErrors
}
tp36()
{
	get_snmp_variable 2c icmp.icmpInDestUnreachs
}
tp37()
{
	get_snmp_variable 2c icmp.icmpInTimeExcds
}
tp38()
{
	get_snmp_variable 2c icmp.icmpInParmProbs
}
tp39()
{
	get_snmp_variable 2c icmp.icmpInSrcQuenchs
}
tp40()
{
	get_snmp_variable 2c icmp.icmpInRedirects
}
tp41()
{
	get_snmp_variable 2c icmp.icmpInEchos
}
tp42()
{
	get_snmp_variable 2c icmp.icmpInEchoReps
}
tp43()
{
	get_snmp_variable 2c icmp.icmpInTimestamps
}
tp44()
{
	get_snmp_variable 2c icmp.icmpInTimestampReps
}
tp45()
{
	get_snmp_variable 2c icmp.icmpInAddrMasks
}
tp46()
{
	get_snmp_variable 2c icmp.icmpInAddrMaskReps
}
tp47()
{
	get_snmp_variable 2c icmp.icmpOutMsgs
}
tp48()
{
	get_snmp_variable 2c icmp.icmpOutErrors
}
tp49()
{
	get_snmp_variable 2c icmp.icmpOutDestUnreachs
}
tp50()
{
	get_snmp_variable 2c icmp.icmpOutTimeExcds
}
tp51()
{
	get_snmp_variable 2c icmp.icmpOutParmProbs
}
tp52()
{
	get_snmp_variable 2c icmp.icmpOutSrcQuenchs
}
tp53()
{
	get_snmp_variable 2c icmp.icmpOutRedirects
}
tp54()
{
	get_snmp_variable 2c icmp.icmpOutEchos
}
tp55()
{
	get_snmp_variable 2c icmp.icmpOutEchoReps
}
tp56()
{
	get_snmp_variable 2c icmp.icmpOutTimestamps
}
tp57()
{
	get_snmp_variable 2c icmp.icmpOutTimestampReps
}
tp58()
{
	get_snmp_variable 2c icmp.icmpOutAddrMasks
}
tp59()
{
	get_snmp_variable 2c icmp.icmpOutAddrMaskReps
}
tp60()
{
	get_snmp_variable 2c tcp.tcpActiveOpens
}
tp61()
{
	get_snmp_variable 2c tcp.tcpRtoAlgorithm
}
tp62()
{
	get_snmp_variable 2c tcp.tcpRtoMin
}
tp63()
{
	get_snmp_variable 2c tcp.tcpRtoMax
}
tp64()
{
	get_snmp_variable 2c tcp.tcpMaxConn
}
tp65()
{
	get_snmp_variable 2c tcp.tcpActiveOpens
}
tp66()
{
	get_snmp_variable 2c tcp.tcpPassiveOpens
}
tp67()
{
	get_snmp_variable 2c tcp.tcpAttemptFails
}
tp68()
{
	get_snmp_variable 2c tcp.tcpEstabResets
}
tp69()
{
	get_snmp_variable 2c tcp.tcpCurrEstab
}
tp70()
{
	get_snmp_variable 2c tcp.tcpInSegs
}
tp71()
{
	get_snmp_variable 2c tcp.tcpOutSegs
}
tp72()
{
	get_snmp_variable 2c tcp.tcpRetransSegs
}
tp73()
{
	get_snmp_variable 2c tcp.tcpInErrs
}
tp74()
{
	get_snmp_variable 2c tcp.tcpOutRsts
}
tp75()
{
	get_snmp_table 2c tcp.tcpConnTable
}
tp76()
{
	get_snmp_variable 2c udp.udpInDatagrams
}
tp77()
{
	get_snmp_variable 2c udp.udpNoPorts
}
tp78()
{
	get_snmp_variable 2c udp.udpInErrors
}
tp79()
{
	get_snmp_variable 2c udp.udpOutDatagrams
}
tp80()
{
	get_snmp_table 2c udp.udpTable
}
tp81()
{
	get_snmp_variable 2c snmp.snmpInPkts
}
tp82()
{
	get_snmp_variable 2c snmp.snmpOutPkts
}
tp83()
{
	get_snmp_variable 2c snmp.snmpInBadVersions
}
tp84()
{
	get_snmp_variable 2c snmp.snmpInBadCommunityNames
}
tp85()
{
	get_snmp_variable 2c snmp.snmpInBadCommunityUses
}
tp86()
{
	get_snmp_variable 2c snmp.snmpInASNParseErrs
}
tp87()
{
	get_snmp_variable 2c snmp.snmpInTooBigs
}
tp88()
{
	get_snmp_variable 2c snmp.snmpInNoSuchNames
}
tp89()
{
	get_snmp_variable 2c snmp.snmpInBadValues
}
tp90()
{
	get_snmp_variable 2c snmp.snmpInReadOnlys
}
tp91()
{
	get_snmp_variable 2c snmp.snmpInGenErrs
}
tp92()
{
	get_snmp_variable 2c snmp.snmpInTotalReqVars
}
tp93()
{
	get_snmp_variable 2c snmp.snmpInTotalSetVars
}
tp94()
{
	get_snmp_variable 2c snmp.snmpInGetRequests
}
tp95()
{
	get_snmp_variable 2c snmp.snmpInGetNexts
}
tp96()
{
	get_snmp_variable 2c snmp.snmpInSetRequests
}
tp97()
{
	get_snmp_variable 2c snmp.snmpInGetResponses
}
tp98()
{
	get_snmp_variable 2c snmp.snmpInTraps
}
tp99()
{
	get_snmp_variable 2c snmp.snmpOutTooBigs
}
tp100()
{
	get_snmp_variable 2c snmp.snmpOutNoSuchNames
}
tp101()
{
	get_snmp_variable 2c snmp.snmpOutBadValues
}
tp102()
{
	get_snmp_variable 2c snmp.snmpOutGenErrs
}
tp103()
{
	get_snmp_variable 2c snmp.snmpOutGetRequests
}
tp104()
{
	get_snmp_variable 2c snmp.snmpOutGetNexts
}
tp105()
{
	get_snmp_variable 2c snmp.snmpOutSetRequests
}
tp106()
{
	get_snmp_variable 2c snmp.snmpOutGetResponses
}
tp107()
{
	get_snmp_variable 2c snmp.snmpOutTraps
}
tp108()
{
	get_snmp_variable 2c snmp.snmpEnableAuthenTraps
}


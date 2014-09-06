/*
*Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
*
*All right reserved
*
*File Name:traceroute-mib.h
*File Description:Add DISMAN-TRACEROUTE-MIB.
*
*Current Version:1.0
*Author:ChenJing
*Date:2004.8.20
*/

/*
 * wrapper for the disman traceroute mib code files 
 */
config_require(disman/traceroute/traceRouteCtlTable)
config_require(disman/traceroute/traceRouteResultsTable)
config_require(disman/traceroute/traceRouteProbeHistoryTable)
config_require(disman/traceroute/traceRouteHopsTable)
config_add_mib(DISMAN-TRACEROUTE-MIB)

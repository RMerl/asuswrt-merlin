/*
*Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
*
*All right reserved
*
*File Name:ping-mib.h
*File Description:Add DISMAN-PING-MIB.
*
*Current Version:1.0
*Author:ChenJing
*Date:2004.8.20
*/

/*
 * wrapper for the disman ping mib code files 
 */
config_require(disman/ping/pingCtlTable)
config_require(disman/ping/pingResultsTable)
config_require(disman/ping/pingProbeHistoryTable)
config_add_mib(DISMAN-PING-MIB)

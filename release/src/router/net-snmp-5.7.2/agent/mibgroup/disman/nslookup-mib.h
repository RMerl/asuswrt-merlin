/*
*Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
*
*All right reserved
*
*File Name:nslookup-mib.h
*File Description:Add DISMAN-NSLOOKUP-MIB.
*
*Current Version:1.0
*Author:ChenJing
*Date:2004.8.20
*/

/*
 * wrapper for the disman name lookup mib code files 
 */
config_require(disman/nslookup/lookupCtlTable)
config_require(disman/nslookup/lookupResultsTable)
config_add_mib(DISMAN-NSLOOKUP-MIB)


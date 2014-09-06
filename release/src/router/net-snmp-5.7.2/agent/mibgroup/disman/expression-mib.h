/*
*Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
*
*All right reserved
*
*File Name:expression-mib.h
*File Description: add DISMAN-EXPRESSION-MIB.
*
*Current Version:1.0
*Author:JianShun Tong
*Date:2004.8.20
*/

/*
 * wrapper for the disman expression mib code files 
 */
config_require(disman/expression/expExpressionTable)
config_require(disman/expression/expErrorTable)
config_require(disman/expression/expObjectTable)
config_require(disman/expression/expValueTable)
config_add_mib(DISMAN-EXPRESSION-MIB)

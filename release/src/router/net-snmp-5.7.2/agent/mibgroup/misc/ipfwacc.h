/*
 *  MIB group interface - ipfwacc.h
 *  IP accounting through firewall rules
 */
#ifndef _MIBGROUP_IPFWACC_H
#define _MIBGROUP_IPFWACC_H

/*
 * we use header_simple_table from the util_funcs module
 */

config_require(util_funcs/header_simple_table)

    /*
     * add the mib we implement to the list of default mibs to load 
     */
config_add_mib(UCD-IPFWACC-MIB)

    /*
     * Magic number definitions: 
     */
#define	IPFWACCINDEX		1
#define	IPFWACCSRCADDR		2
#define	IPFWACCSRCNM		3
#define	IPFWACCDSTADDR		4
#define	IPFWACCDSTNM		5
#define	IPFWACCVIANAME		6
#define	IPFWACCVIAADDR		7
#define	IPFWACCPROTO		8
#define	IPFWACCBIDIR		9
#define	IPFWACCDIR		10
#define	IPFWACCBYTES		11
#define	IPFWACCPACKETS		12
#define	IPFWACCNSRCPRTS		13
#define	IPFWACCNDSTPRTS		14
#define	IPFWACCSRCISRNG		15
#define	IPFWACCDSTISRNG		16
#define	IPFWACCPORT1		17
#define	IPFWACCPORT2		18
#define	IPFWACCPORT3		19
#define	IPFWACCPORT4		20
#define	IPFWACCPORT5		21
#define	IPFWACCPORT6		22
#define	IPFWACCPORT7		23
#define	IPFWACCPORT8		24
#define	IPFWACCPORT9		25
#define	IPFWACCPORT10		26
    /*
     * function definitions 
     */
     extern void     init_ipfwacc(void);
     extern FindVarMethod var_ipfwacc;

#endif                          /* _MIBGROUP_IPFWACC_H */

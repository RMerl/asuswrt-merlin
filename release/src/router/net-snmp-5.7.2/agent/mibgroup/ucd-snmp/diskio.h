#ifndef _MIBGROUP_DISKIO_H
#define _MIBGROUP_DISKIO_H

config_require(util_funcs/header_simple_table)
config_add_mib(UCD-DISKIO-MIB)

    /*
     * Define all our functions using prototyping for ANSI compilers 
     */
    /*
     * These functions are then defined in the example.c file 
     */
     void            init_diskio(void);
     FindVarMethod   var_diskio;


/*
 * Magic number definitions.  These numbers are the last oid index
 * numbers to the table that you are going to define.  For example,
 * lets say (since we are) creating a mib table at the location
 * .1.3.6.1.4.1.2021.254.  The following magic numbers would be the
 * next numbers on that oid for the var_example function to use, ie:
 * .1.3.6.1.4.1.2021.254.1 (and .2 and .3 ...) 
 */

#define	DISKIO_INDEX		1
#define DISKIO_DEVICE		2
#define DISKIO_NREAD		3
#define DISKIO_NWRITTEN		4
#define DISKIO_READS		5
#define DISKIO_WRITES		6
#define DISKIO_LA1		9
#define DISKIO_LA5              10
#define DISKIO_LA15             11
#define DISKIO_NREADX           12
#define DISKIO_NWRITTENX        13

#endif                          /* _MIBGROUP_DISKIO_H */
